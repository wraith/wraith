
/* 
 * dcc.c -- handles:
 *   activity on a dcc socket
 *   disconnect on a dcc socket
 *   ...and that's it!  (but it's a LOT)
 * 
 * dprintf'ized, 27oct1995
 * 
 * $Id: dcc.c,v 1.24 2000/01/08 21:23:13 per Exp $
 */

/* 
 * Copyright (C) 1997  Robey Pointer
 * Copyright (C) 1999, 2000  Eggheads
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "main.h"
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "hook.h"
#include "tandem.h"

extern struct userrec *userlist;
extern struct chanset_t *chanset;

#ifdef G_USETCL
extern Tcl_Interp *interp;
#endif
extern time_t now;
extern int egg_numver,
  connect_timeout,
  conmask,
  backgrd,
  max_dcc;
extern int make_userfile,
  default_flags,
  debug_output,
  ignore_time;
extern char botnetnick[],
  ver[],
  origbotname[],
  netpass[];
extern sock_list *socklist;
extern int MAXSOCKS;
struct dcc_t *dcc = 0;		/* dcc list */
int timesync = 0;
#ifdef G_WGET
int wget_timeout = 20;
#endif
int dcc_total = 0;		/* total dcc's */
char tempdir[121] = "";		/* temporary directory (default: current dir) */
char network[41] = "unknown-net";	/* name of the IRC network you're on */
int password_timeout = 30;	/* time to wait for a password from a user */
int bot_timeout = 60;		/* bot timeout value */
int identtimeout = 10;		/* timeout value for ident lookups */
int protect_telnet = 1;		/* even bother with ident lookups :) */
int flood_telnet_thr = 7;	/* number of telnet connections to be considered a flood */
int flood_telnet_time = 60;	/* in how many seconds? */
extern int min_dcc_port,
  max_dcc_port;			/* valid portrange for telnets */
extern int par_telnet_flood;	/* trigger telnet flood for +f ppl? */

#ifdef G_WGET
struct wget_info {
  int idx;
  char nick[HANDLEN+1];
  char request[512];
  char local[256];
  FILE * lfile;
  int status;
  int expected;
  int received;
};
#endif

void strip_telnet(int sock, char *buf, int *len)
{
  unsigned char *p = (unsigned char *) buf,
   *o = (unsigned char *) buf;
  int mark;

  while (*p != 0) {
    while ((*p != 255) && (*p != 0)) {
      if (*p==0xA0) {
	*o++=32;
	p++;
      } else
	*o++=*p++;
    }
    if (*p == 255) {
      p++;
      mark = 2;
      if (!*p)
	mark = 1;		/* bogus */
      if ((*p >= 251) && (*p <= 254)) {
	mark = 3;
	if (!*(p + 1))
	  mark = 2;		/* bogus */
      }
      if (*p == 251) {
	/* WILL X -> response: DONT X */
	/* except WILL ECHO which we just smile and ignore */
	if (!(*(p + 1) == 1)) {
	  write(sock, "\377\376", 2);
	  write(sock, p + 1, 1);
	}
      }
      if (*p == 253) {
	/* DO X -> response: WONT X */
	/* except DO ECHO which we just smile and ignore */
	if (!(*(p + 1) == 1)) {
	  write(sock, "\377\374", 2);
	  write(sock, p + 1, 1);
	}
      }
      if (*p == 246) {
	/* "are you there?" */
	/* response is: "hell yes!" */
	write(sock, STR("\r\nHell, yes!\r\n"), 14);
      }
      /* anything else can probably be ignored */
      p += mark - 1;
      *len = *len - mark;
    }
  }
  *o = *p;
}

void send_timesync(idx)
{
  /* Send timesync to idx, or all lower bots if idx<0 */
  if (idx >= 0)
    dprintf(idx, STR("ts %li\n"), (timesync + now));
  else {
#ifdef HUB
    char s[30];
    int i;

    sprintf(s, STR("ts %li\n"), (timesync + now));
    for (i = 0; i < dcc_total; i++) {
      if ((dcc[i].type == &DCC_BOT) && (bot_aggressive_to(dcc[i].user))) {
	dprintf(i, s);
      }
    }
#else
    log(LCAT_ERROR, STR("I'm a leaf - where should i send timesync?"));
#endif
  }
}

void greet_new_bot(int idx)
{
  int i;

  dcc[idx].timeval = now;
  dcc[idx].u.bot->version[0] = 0;
  dcc[idx].u.bot->numver = 0;

#ifdef HUB
  if (dcc[idx].user && (!(dcc[idx].user->flags & USER_OP))) {
    log(LCAT_BOT, STR("Rejecting link from %s"), dcc[idx].nick);
    dprintf(idx, STR("error You are being rejected.\n"));
    dprintf(idx, STR("bye\n"));
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }
#endif

  if (bot_hublevel(dcc[idx].user) == 999)
    dcc[idx].status |= STAT_LEAF;
  dcc[idx].status |= STAT_LINKING;
  dprintf(idx, STR("v %d %d %s <%s>\n"), egg_numver, HANDLEN, ver, network);
  for (i = 0; i < dcc_total; i++)
    if (dcc[i].type == &DCC_FORK_BOT) {
      killsock(dcc[i].sock);
      lostdcc(i);
    }
}

void bot_version(int idx, char *par)
{
  char x[1024];
  int l;

  Context;
  dcc[idx].timeval = now;
  if ((par[0] >= '0') && (par[0] <= '9')) {
    char *work;

    work = newsplit(&par);
    dcc[idx].u.bot->numver = atoi(work);
  } else
    dcc[idx].u.bot->numver = 0;

  dprintf(idx, STR("tb %s\n"), botnetnick);
  l = atoi(newsplit(&par));
  if (l != HANDLEN) {
    dprintf(idx, STR("error Non-matching handle length: mine %d, yours %d\n"), HANDLEN, l);
    dprintf(idx, STR("bye\n"));
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }
  strncpy0(dcc[idx].u.bot->version, par, 120);
  log(LCAT_BOT, STR("Linked to %s"), dcc[idx].nick);
  botnet_send_nlinked(idx, dcc[idx].nick, botnetnick, '!', dcc[idx].u.bot->numver);
  dump_links(idx);
  dcc[idx].type = &DCC_BOT;
  addbot(dcc[idx].nick, dcc[idx].nick, botnetnick, '-', dcc[idx].u.bot->numver);
  check_tcl_link(dcc[idx].nick, botnetnick);
  simple_sprintf(x, STR("v %d"), dcc[idx].u.bot->numver);
  bot_share(idx, x);
  dprintf(idx, STR("el\n"));
}

void failed_link(int idx)
{
  char s[81],
    s1[512];

  if (dcc[idx].u.bot->linker[0]) {
    sprintf(s, STR("Couldn't link to %s."), dcc[idx].nick);
    strcpy(s1, dcc[idx].u.bot->linker);
    add_note(s1, botnetnick, s, -2, 0);
  }
  if (dcc[idx].u.bot->numver >= (-1))
    log(LCAT_BOT, STR("Failed link to %s."), dcc[idx].nick);
  killsock(dcc[idx].sock);
  strcpy(s, dcc[idx].nick);
  lostdcc(idx);
  autolink_cycle(s);		/* check for more auto-connections */
}

void cont_link(int idx, char *buf, int ii)
{
  /* Now set the initial link key (incoming only, we're not sending more until we get an OK)... */
  struct sockaddr_in sa;
  char tmp[256];
  MD5_CTX ctx;
  int i,
    snum = -1;

  for (i = 0; i < MAXSOCKS; i++) {
    if ((socklist[i].sock == dcc[idx].sock)
	&& !(socklist[i].flags & SOCK_UNUSED)) {
      snum = i;
    }
  }
  if (snum >= 0) {
    int i;

    /* If we're already connected somewhere, unlink and idle a sec */
    for (i = 0; i < dcc_total; i++) {
      if ((dcc[i].type == &DCC_BOT) && (!bot_aggressive_to(dcc[i].user))) {
	log(LCAT_BOT, STR("Unlinking %s - restructure"), dcc[i].nick);
	botnet_send_unlinked(i, dcc[i].nick, STR("Restructure"));
	killsock(dcc[i].sock);
	lostdcc(i);
	usleep(1000 * 500);
	break;
      }
    }
    dcc[idx].type = &DCC_BOT_NEW;
    dcc[idx].u.bot->numver = 0;
    dprintf(idx, STR("%s\n"), botnetnick);
    i = sizeof(sa);
    /* myip myport hubnick mynick */
    getsockname(socklist[snum].sock, (struct sockaddr *) &sa, &i);
    sprintf(tmp, STR("%8x@%4x@%s@%s"), getmyip(), sa.sin_port, dcc[idx].nick, botnetnick);
    MD5Init(&ctx);
    MD5Update(&ctx, tmp, strlen(tmp));
    MD5Final(socklist[snum].ikey, &ctx);
    socklist[snum].encstatus = 1;
  } else {
    lostdcc(idx);
    killsock(dcc[idx].sock);
  }
  return;
}

/*    This function generates a digest by combining 'challenge' with
 *  'password' and then sends it to the other bot. <Cybah>
 */
void dcc_bot_new(int idx, char *buf, int x)
{
  /*   struct userrec *u = get_user_by_handle(userlist, dcc[idx].nick); */
  char *code;
  int i;

  strip_telnet(dcc[idx].sock, buf, &x);
  code = newsplit(&buf);

  if (!strcasecmp(code, STR("*hello!"))) {
    greet_new_bot(idx);
  } else if (!strcasecmp(code, "v")) {
    bot_version(idx, buf);
  } else if (!strcasecmp(code, STR("elink"))) {
    int snum = -1;

    /* Set the socket key and we're linked */
    for (i = 0; i < MAXSOCKS; i++) {
      if (!(socklist[i].flags & SOCK_UNUSED)
	  && (socklist[i].sock == dcc[idx].sock)) {
	snum = i;
	break;
      }
    }
    if (snum >= 0) {
      char *tmp,
       *p;

      p = newsplit(&buf);
      tmp = decrypt_string(netpass, p);
      strncpy0(socklist[snum].okey, tmp, 17);
      strcpy(socklist[snum].ikey, socklist[snum].okey);
      nfree(tmp);
      socklist[snum].iseed = atoi(buf);
      socklist[snum].oseed = atoi(buf);
      dprintf(idx, STR("elinkdone\n"));
      log(LCAT_BOT, STR("Handshake with %s succeeded, we're linked."), dcc[idx].nick);
    }
  } else if (!strcasecmp(code, STR("error"))) {
    log(LCAT_ERROR, STR("ERROR linking %s: %s"), dcc[idx].nick, buf);
    killsock(dcc[idx].sock);    
    lostdcc(idx);
  }
  /* ignore otherwise */
}

void eof_dcc_bot_new(int idx)
{
  Context;
  log(LCAT_BOT, STR("Lost bot: %s"), dcc[idx].nick, dcc[idx].port);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

void timeout_dcc_bot_new(int idx)
{
  log(LCAT_BOT, STR("Timeout: bot link to %s at %s:%d"), dcc[idx].nick, dcc[idx].host, dcc[idx].port);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

void display_dcc_bot_new(int idx, char *buf)
{
  sprintf(buf, STR("bot*  waited %lus"), now - dcc[idx].timeval);
}

int expmem_dcc_bot_(void *x)
{
  return sizeof(struct bot_info);
}

void free_dcc_bot_(int n, void *x)
{
  if (dcc[n].type == &DCC_BOT) {
    unvia(n, findbot(dcc[n].nick));
    rembot(dcc[n].nick);
  }
  nfree(x);
}

struct dcc_table DCC_BOT_NEW = {
  "BOT_NEW",
  0,
  eof_dcc_bot_new,
  dcc_bot_new,
  &bot_timeout,
  timeout_dcc_bot_new,
  display_dcc_bot_new,
  expmem_dcc_bot_,
  free_dcc_bot_,
  0
};

/* hash function for tandem bot commands */
extern botcmd_t C_bot[];

void dcc_bot(int idx, char *code, int i)
{
  char *msg;
  int f;

  Context;
  strip_telnet(dcc[idx].sock, code, &i);
  if (debug_output) {
    if (code[0] == 's')
      log(LCAT_BOT, STR("{%s} %s"), dcc[idx].nick, code + 2);
    else
      log(LCAT_BOT, STR("[%s] %s"), dcc[idx].nick, code);
  }
  msg = strchr(code, ' ');
  if (msg) {
    *msg = 0;
    msg++;
  } else
    msg = "";
  f = 0;
  i = 0;
  while ((C_bot[i].name != NULL) && (!f)) {
    int y = strcasecmp(code, C_bot[i].name);

    if (y == 0) {
      /* found a match */
      (C_bot[i].func) (idx, msg);
      f = 1;
    } else if (y < 0)
      return;
    i++;
  }
}

void eof_dcc_bot(int idx)
{
  char x[1024];

  simple_sprintf(x, STR("Lost bot: %s"), dcc[idx].nick);
  log(LCAT_BOT, STR("%s."), x);
  botnet_send_unlinked(idx, dcc[idx].nick, x);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

void display_dcc_bot(int idx, char *buf)
{
  int i = simple_sprintf(buf, STR("bot   flags: "));

  buf[i++] = b_status(idx) & STAT_PINGED ? 'P' : 'p';
  buf[i++] = b_status(idx) & STAT_SHARE ? 'U' : 'u';
  buf[i++] = b_status(idx) & STAT_CALLED ? 'C' : 'c';
  buf[i++] = b_status(idx) & STAT_OFFERED ? 'O' : 'o';
  buf[i++] = b_status(idx) & STAT_SENDING ? 'S' : 's';
  buf[i++] = b_status(idx) & STAT_GETTING ? 'G' : 'g';
  buf[i++] = b_status(idx) & STAT_WARNED ? 'W' : 'w';
  buf[i++] = b_status(idx) & STAT_LEAF ? 'L' : 'l';
  buf[i++] = b_status(idx) & STAT_LINKING ? 'I' : 'i';
  buf[i++] = b_status(idx) & STAT_AGGRESSIVE ? 'a' : 'A';
  buf[i++] = 0;
}

void display_dcc_fork_bot(int idx, char *buf)
{
  sprintf(buf, STR("conn  bot"));
}

struct dcc_table DCC_BOT = {
  "BOT",
  DCT_BOT,
  eof_dcc_bot,
  dcc_bot,
  0,
  0,
  display_dcc_bot,
  expmem_dcc_bot_,
  free_dcc_bot_,
  0
};

struct dcc_table DCC_FORK_BOT = {
  "FORK_BOT",
  0,
  failed_link,
  cont_link,
  &connect_timeout,
  failed_link,
  display_dcc_fork_bot,
  expmem_dcc_bot_,
  free_dcc_bot_,
  0
};

void dcc_chat_pass(int idx, char *buf, int atr)
{
  if (!atr)
    return;
  strip_telnet(dcc[idx].sock, buf, &atr);
  atr = dcc[idx].user ? dcc[idx].user->flags : 0;

  /* Check for MD5 digest from remote _bot_. <cybah> */
  if (atr & USER_BOT) {
    if (!strcasecmp(buf, STR("elinkdone"))) {
      nfree(dcc[idx].u.chat);
      dcc[idx].type = &DCC_BOT_NEW;
      dcc[idx].u.bot = get_data_ptr(sizeof(struct bot_info));

      dcc[idx].status = STAT_CALLED;
      dprintf(idx, STR("*hello!\n"));
      greet_new_bot(idx);
      send_timesync(idx);
    } else {
      log(LCAT_WARNING, STR("%s failed encrypted link handshake"), dcc[idx].nick);
      log(LCAT_DEBUG, STR("Expected elinkdone, got %s"), buf);
      killsock(dcc[idx].sock);
      lostdcc(idx);
    }
    return;
  }

  if (u_pass_match(dcc[idx].user, buf)) {
    /* log entry for successful login -slennox 3/28/1999 */
    log(LCAT_CONN, STR("Logged in: %s (%s/%d)"), dcc[idx].nick, dcc[idx].host, dcc[idx].port);
    if (dcc[idx].u.chat->away) {
      nfree(dcc[idx].u.chat->away);
      dcc[idx].u.chat->away = NULL;
    }
    dcc[idx].type = &DCC_CHAT;
    dcc[idx].status &= ~STAT_CHAT;
    dcc[idx].u.chat->channel = -2;
    if (dcc[idx].status & STAT_TELNET)
      dprintf(idx, "\377\374\001\n");	/* turn echo back on */
    stats_add(dcc[idx].user, 1, 0);
    dcc_chatter(idx);
  } else {
    dprintf(idx, STR("Go away.\n"));
    log(LCAT_CONN, STR("Bad password: [%s]%s/%d"), dcc[idx].nick, dcc[idx].host, dcc[idx].port);
    log(LCAT_WARNING, STR("Bad password: [%s]%s/%d"), dcc[idx].nick, dcc[idx].host, dcc[idx].port);
    if (dcc[idx].u.chat->away) {	/* su from a dumb user */
      if (dcc[idx].status & STAT_TELNET)
	dprintf(idx, "\377\374\001\n");	/* turn echo back on */
      dcc[idx].user = get_user_by_handle(userlist, dcc[idx].u.chat->away);
      strcpy(dcc[idx].nick, dcc[idx].u.chat->away);
      nfree(dcc[idx].u.chat->away);
      nfree(dcc[idx].u.chat->su_nick);
      dcc[idx].u.chat->away = NULL;
      dcc[idx].u.chat->su_nick = NULL;
      dcc[idx].type = &DCC_CHAT;
      if (dcc[idx].u.chat->channel < 100000)
	botnet_send_join_idx(idx, -1);
      chanout_but(-1, dcc[idx].u.chat->channel, STR("*** %s has joined the party line.\n"), dcc[idx].nick);
    } else {
      killsock(dcc[idx].sock);
      lostdcc(idx);
    }
  }
}

void eof_dcc_general(int idx)
{
  log(LCAT_CONN, STR("Lost dcc connection to %s (%s/%d)"), dcc[idx].nick, dcc[idx].host, dcc[idx].port);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

void tout_dcc_chat_pass(int idx)
{
  dprintf(idx, STR("Timeout.\n"));
  log(LCAT_CONN, STR("Password timeout on dcc chat: [%s]%s"), dcc[idx].nick, dcc[idx].host);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

void display_dcc_chat_pass(int idx, char *buf)
{
  sprintf(buf, STR("pass  waited %lus"), now - dcc[idx].timeval);
}

int expmem_dcc_general(void *x)
{
  register struct chat_info *p = (struct chat_info *) x;
  int tot = sizeof(struct chat_info);

  if (p->away)
    tot += strlen(p->away) + 1;
  if (p->buffer) {
    struct msgq *q = p->buffer;

    while (q) {
      tot += sizeof(struct list_type);

      tot += q->len + 1;
      q = q->next;
    }
  }
  if (p->su_nick)
    tot += strlen(p->su_nick) + 1;
  return tot;
}

void kill_dcc_general(int idx, void *x)
{
  register struct chat_info *p = (struct chat_info *) x;

  if (p) {
    if (p->buffer) {
      struct msgq *r = dcc[idx].u.chat->buffer,
       *q;

      while (r) {
	q = r->next;
	nfree(r->msg);
	nfree(r);
	r = q;
      }
    }
    if (p->away) {
      nfree(p->away);
    }
    nfree(p);
  }
}

/* Remove the color control codes that mIRC,pIRCh etc use to make
 * their client seem so fecking cool! (Sorry, Khaled, you are a nice
 * guy, but when you added this feature you forced people to either
 * use your *SHAREWARE* client or face screenfulls of crap!) */
void strip_mirc_codes(int flags, char *text)
{
  char *dd = text;

  while (*text) {
    switch (*text) {
    case 2:			/* Bold text */
      if (flags & STRIP_BOLD) {
	text++;
	continue;
      }
      break;
    case 3:			/* mIRC colors? */
      if (flags & STRIP_COLOR) {
	if (isdigit(text[1])) {	/* Is the first char a number? */
	  text += 2;		/* Skip over the ^C and the first digit */
	  if (isdigit(*text))
	    text++;		/* Is this a double digit number? */
	  if (*text == ',') {	/* Do we have a background color next? */
	    if (isdigit(text[1]))
	      text += 2;	/* Skip over the first background digit */
	    if (isdigit(*text))
	      text++;		/* Is it a double digit? */
	  }
	} else
	  text++;
	continue;
      }
      break;
    case 7:
      if (flags & STRIP_BELLS) {
	text++;
	continue;
      }
      break;
    case 0x16:			/* Reverse video */
      if (flags & STRIP_REV) {
	text++;
	continue;
      }
      break;
    case 0x1f:			/* Underlined text */
      if (flags & STRIP_UNDER) {
	text++;
	continue;
      }
      break;
    case 033:
      if (flags & STRIP_ANSI) {
	text++;
	if (*text == '[') {
	  text++;
	  while ((*text == ';') || isdigit(*text))
	    text++;
	  if (*text)
	    text++;		/* also kill the following char */
	}
	continue;
      }
      break;
    }
    *dd++ = *text++;		/* Move on to the next char */
  }
  *dd = 0;
}

void append_line(int idx, char *line)
{
  int l = strlen(line);
  struct msgq *p,
   *q;
  struct chat_info *c = (dcc[idx].type == &DCC_CHAT) ? dcc[idx].u.chat : dcc[idx].u.file->chat;

  if (c->current_lines > 1000) {
    p = c->buffer;
    /* they're probably trying to fill up the bot nuke the sods :) */
    while (p) {			/* flush their queue */
      q = p->next;
      nfree(p->msg);
      nfree(p);
      p = q;
    }
    c->buffer = 0;
    dcc[idx].status &= ~STAT_PAGE;
    do_boot(idx, botnetnick, STR("too many pages - senq full"));
    return;
  }
  if ((c->line_count < c->max_line) && (c->buffer == NULL)) {
    c->line_count++;
    tputs(dcc[idx].sock, line, l);
  } else {
    c->current_lines++;
    if (c->buffer == NULL)
      q = NULL;
    else {
      q = c->buffer;
      while (q->next != NULL)
	q = q->next;
    }
    p = get_data_ptr(sizeof(struct msgq));

    p->len = l;
    p->msg = get_data_ptr(l + 1);
    p->next = NULL;
    strcpy(p->msg, line);
    if (q == NULL)
      c->buffer = p;
    else
      q->next = p;
  }
}

void out_dcc_general(int idx, char *buf, void *x)
{
  register struct chat_info *p = (struct chat_info *) x;
  char *y = buf;

  strip_mirc_codes(p->strip_flags, buf);
  if (dcc[idx].status & STAT_TELNET)
    y = add_cr(buf);
  if (dcc[idx].status & STAT_PAGE)
    append_line(idx, y);
  else
    tputs(dcc[idx].sock, y, strlen(y));
}

struct dcc_table DCC_CHAT_PASS = {
  "CHAT_PASS",
  0,
  eof_dcc_general,
  dcc_chat_pass,
  &password_timeout,
  tout_dcc_chat_pass,
  display_dcc_chat_pass,
  expmem_dcc_general,
  kill_dcc_general,
  out_dcc_general
};

/* make sure ansi code is just for color-changing */
int check_ansi(char *v)
{
  int count = 2;

  if (*v++ != '\033')
    return 1;
  if (*v++ != '[')
    return 1;
  while (*v) {
    if (*v == 'm')
      return 0;
    if ((*v != ';') && ((*v < '0') || (*v > '9')))
      return count;
    v++;
    count++;
  }
  return count;
}

void eof_dcc_chat(int idx)
{
  Context;
  log(LCAT_CONN, STR("Lost dcc connection to %s (%s/%d)"), dcc[idx].nick, dcc[idx].host, dcc[idx].port);
  Context;
  if (dcc[idx].u.chat->channel >= 0) {
    chanout_but(idx, dcc[idx].u.chat->channel, STR("*** %s lost dcc link.\n"), dcc[idx].nick);
    Context;
    if (dcc[idx].u.chat->channel < 100000)
      botnet_send_part_idx(idx, STR("lost dcc link"));
    check_tcl_chpt(botnetnick, dcc[idx].nick, dcc[idx].sock, dcc[idx].u.chat->channel);
  }
  Context;
  check_tcl_chof(dcc[idx].nick, dcc[idx].sock);
  Context;
  killsock(dcc[idx].sock);
  Context;
  lostdcc(idx);
  Context;
}

void dcc_chat(int idx, char *buf, int i)
{
  int nathan = 0,
    doron = 0,
    fixed = 0;
  char *v,
   *d;

  Context;
  strip_telnet(dcc[idx].sock, buf, &i);
  if (buf[0] && (buf[0] != '.') && detect_dcc_flood(&dcc[idx].timeval, dcc[idx].u.chat, idx))
    return;
  dcc[idx].timeval = now;
  if (buf[0])
    strcpy(buf, check_tcl_filt(idx, buf));
  if (buf[0]) {
    /* check for beeps and cancel annoying ones */
    v = buf;
    d = buf;
    while (*v)
      switch (*v) {
      case 7:			/* beep - no more than 3 */
	nathan++;
	if (nathan > 3)
	  v++;
	else
	  *d++ = *v++;
	break;
      case 8:			/* backspace - for lame telnet's :) */
	if (d > buf) {
	  d--;
	}
	v++;
	break;
      case 27:			/* ESC - ansi code? */
	doron = check_ansi(v);
	/* if it's valid, append a return-to-normal code at the end */
	if (!doron) {
	  *d++ = *v++;
	  fixed = 1;
	} else
	  v += doron;
	break;
      case '\r':		/* weird pseudo-linefeed */
	v++;
	break;
      default:
	*d++ = *v++;
      }
    if (fixed)
      strcpy(d, STR("\033[0m"));
    else
      *d = 0;
    if (buf[0]) {		/* nothing to say - maybe paging... */
      if ((buf[0] == '.') || (dcc[idx].u.chat->channel < 0)) {
	if (buf[0] == '.')
	  buf++;
	v = newsplit(&buf);
	rmspace(buf);
	if (check_tcl_dcc(v, idx, buf)) {
	  if (dcc[idx].u.chat->channel >= 0)
	    check_tcl_chpt(botnetnick, dcc[idx].nick, dcc[idx].sock, dcc[idx].u.chat->channel);
	  check_tcl_chof(dcc[idx].nick, dcc[idx].sock);
	  dprintf(idx, STR("*** Ja mata!\n"));
	  flush_lines(idx, dcc[idx].u.chat);
	  log(LCAT_CONN, STR("DCC connection closed (%s!%s)"), dcc[idx].nick, dcc[idx].host);
	  if (dcc[idx].u.chat->channel >= 0) {
	    chanout_but(-1, dcc[idx].u.chat->channel, STR("*** %s left the party line%s%s\n"), dcc[idx].nick, buf[0] ? ": " : ".", buf);
	    if (dcc[idx].u.chat->channel < 100000)
	      botnet_send_part_idx(idx, buf);
	  }
	  if (dcc[idx].u.chat->su_nick) {
	    dcc[idx].user = get_user_by_handle(userlist, dcc[idx].u.chat->su_nick);
	    strcpy(dcc[idx].nick, dcc[idx].u.chat->su_nick);
	    dcc[idx].type = &DCC_CHAT;
	    nfree(dcc[idx].u.chat->su_nick);
	    dcc[idx].u.chat->su_nick = NULL;
	    dcc_chatter(idx);
	    if (dcc[idx].u.chat->channel < 100000 && dcc[idx].u.chat->channel >= 0)
	      botnet_send_join_idx(idx, -1);
	    return;
	  } else if ((dcc[idx].sock != STDOUT) || backgrd) {
	    killsock(dcc[idx].sock);
	    lostdcc(idx);
	    return;
	  } else {
	    dprintf(DP_STDOUT, STR("\n### SIMULATION RESET\n\n"));
	    dcc_chatter(idx);
	    return;
	  }
	}
      } else if (buf[0] == ',') {
	int me = 0;

	if ((buf[1] == 'm') && (buf[2] == 'e') && buf[3] == ' ')
	  me = 1;
	for (i = 0; i < dcc_total; i++) {
	  int ok = 0;

	  if (dcc[i].type->flags & DCT_MASTER) {
	    if ((dcc[i].type != &DCC_CHAT) || (dcc[i].u.chat->channel >= 0))
	      if ((i != idx) || (dcc[idx].status & STAT_ECHO))
		ok = 1;
	  }
	  if (ok) {
	    struct userrec *u = get_user_by_handle(userlist, dcc[i].nick);

	    if (u && (u->flags & USER_MASTER)) {
	      if (me)
		dprintf(i, STR("-> %s%s\n"), dcc[idx].nick, buf + 3);
	      else
		dprintf(i, STR("-%s-> %s\n"), dcc[idx].nick, buf + 1);
	    }
	  }
	}
      } else if (buf[0] == '\'') {
	int me = 0;

	if ((buf[1] == 'm') && (buf[2] == 'e') && ((buf[3] == ' ') || (buf[3] == '\'') || (buf[3] == ',')))
	  me = 1;
	for (i = 0; i < dcc_total; i++) {
	  if (dcc[i].type->flags & DCT_CHAT) {
	    if (me)
	      dprintf(i, STR("=> %s%s\n"), dcc[idx].nick, buf + 3);
	    else
	      dprintf(i, STR("=%s=> %s\n"), dcc[idx].nick, buf + 1);
	  }
	}
      } else {
	if (dcc[idx].u.chat->away != NULL)
	  not_away(idx);
	if (dcc[idx].status & STAT_ECHO)
	  chanout_but(-1, dcc[idx].u.chat->channel, STR("<%s> %s\n"), dcc[idx].nick, buf);
	else
	  chanout_but(idx, dcc[idx].u.chat->channel, STR("<%s> %s\n"), dcc[idx].nick, buf);
	botnet_send_chan(-1, botnetnick, dcc[idx].nick, dcc[idx].u.chat->channel, buf);
	check_tcl_chat(dcc[idx].nick, dcc[idx].u.chat->channel, buf);
      }
    }
  }
  if (dcc[idx].type == &DCC_CHAT)	/* could have change to files */
    if (dcc[idx].status & STAT_PAGE)
      flush_lines(idx, dcc[idx].u.chat);
}

void display_dcc_chat(int idx, char *buf)
{
  int i = simple_sprintf(buf, STR("chat  flags: "));

  buf[i++] = dcc[idx].status & STAT_CHAT ? 'C' : 'c';
  buf[i++] = dcc[idx].status & STAT_PARTY ? 'P' : 'p';
  buf[i++] = dcc[idx].status & STAT_TELNET ? 'T' : 't';
  buf[i++] = dcc[idx].status & STAT_ECHO ? 'E' : 'e';
  buf[i++] = dcc[idx].status & STAT_PAGE ? 'P' : 'p';
  simple_sprintf(buf + i, STR("/%d"), dcc[idx].u.chat->channel);
}

struct dcc_table DCC_CHAT = {
  "CHAT",
  DCT_CHAT | DCT_MASTER | DCT_SHOWWHO | DCT_VALIDIDX | DCT_SIMUL | DCT_CANBOOT | DCT_REMOTEWHO,
  eof_dcc_chat,
  dcc_chat,
  0,
  0,
  display_dcc_chat,
  expmem_dcc_general,
  kill_dcc_general,
  out_dcc_general
};

int lasttelnets;
char lasttelnethost[81];
time_t lasttelnettime;

/* a modified detect_flood for incoming telnet flood protection */
int detect_telnet_flood(char *floodhost)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0 };

  Context;
  get_user_flagrec(get_user_by_host(floodhost), &fr, NULL);
  Context;
  if (flood_telnet_thr == 0 || (glob_friend(fr) && !par_telnet_flood))
    return 0;			/* no flood protection */
  if (strcasecmp(lasttelnethost, floodhost) != 0) {	/* new */
    strcpy(lasttelnethost, floodhost);
    lasttelnettime = now;
    lasttelnets = 0;
    return 0;
  }
  if (lasttelnettime < now - flood_telnet_time) {
    /* flood timer expired, reset it */
    lasttelnettime = now;
    lasttelnets = 0;
    return 0;
  }
  lasttelnets++;
  if (lasttelnets >= flood_telnet_thr) {	/* FLOOD */
    /* reset counters */
    lasttelnets = 0;
    lasttelnettime = 0;
    lasttelnethost[0] = 0;
    log(LCAT_WARNING, STR("Telnet connection flood from %s! Placing on ignore!"), floodhost);
    addignore(floodhost, origbotname, STR("Telnet connection flood"), now + (60 * ignore_time));
    return 1;
  }
  return 0;
}

void dcc_telnet_got_ident(int, char *);

void dcc_telnet(int idx, char *buf, int i)
{
  unsigned long ip;
  unsigned short port;
  int j = 0,
    sock;
  char s[UHOSTLEN],
    s2[UHOSTLEN + 20];

  Context;
  if (dcc_total + 1 > max_dcc) {
    j = answer(dcc[idx].sock, s, &ip, &port, 0);
    if (j != -1) {
      dprintf(-j, STR("Sorry, too many connections already.\r\n"));
      killsock(j);
    }
    return;
  }
  Context;
  sock = answer(dcc[idx].sock, s, &ip, &port, 0);
  while ((sock == -1) && (errno == EAGAIN))
    sock = answer(sock, s, &ip, &port, 0);
  if (sock < 0) {
    neterror(s);
    log(LCAT_CONN, STR("Failed TELNET incoming (%s)"), s);
    return;
  }
  /* <bindle> [09:37] Telnet connection: 168.246.255.191/0
   * <bindle> [09:37] Lost connection while identing [168.246.255.191/0]
   */
  Context;
  /* use dcc-portrange x:x on incoming telnets too, dw */
  if ((port < min_dcc_port) || (port > max_dcc_port)) {
    log(LCAT_CONN, STR("Refused %s/%d (bad src port)"), s, port);
    log(LCAT_WARNING, STR("Refused %s/%d (bad src port)"), s, port);
    killsock(sock);
    return;
  }
  Context;
  /* deny ips that ends with 0 or 255, dw */
  if ((ip & 0xff) == 0 || (ip & 0xff) == 0xff) {
    log(LCAT_CONN, STR("Refused %s/%d (invalid ip)"), s, port);
    log(LCAT_WARNING, STR("Refused %s/%d (invalid ip)"), s, port);
    killsock(sock);
    return;
  }
  if (dcc[idx].host[0] == '@') {
    /* restrict by hostname */
    if (!wild_match(dcc[idx].host + 1, s)) {
      log(LCAT_CONN, STR("Refused %s (bad hostname)"), s);
      log(LCAT_WARNING, STR("Refused %s (bad hostname)"), s);
      killsock(sock);
      return;
    }
  }
  Context;
  sprintf(s2, STR("telnet!telnet@%s"), s);
  if (match_ignore(s2) || detect_telnet_flood(s2)) {
    killsock(sock);
    return;
  }
  Context;
  i = new_dcc(&DCC_IDENTWAIT, 0);
  dcc[i].sock = sock;
  dcc[i].addr = ip;
  dcc[i].port = port;
  dcc[i].timeval = now;
  dcc[i].u.ident_sock = dcc[idx].sock;
  strncpy0(dcc[i].host, s, UHOSTMAX);
  strcpy(dcc[i].nick, "*");
  sock = open_telnet(s, 113);
  log(LCAT_CONN, STR("Telnet connection: %s/%d"), s, port);
  s[0] = 0;
  Context;
  if (sock < 0) {
    if (sock == -2)
      strcpy(s, STR("DNS lookup failed for ident"));
    else
      neterror(s);
  } else {
    j = new_dcc(&DCC_IDENT, 0);
    if (j < 0) {
      killsock(sock);
      strcpy(s, STR("No Free DCC's"));
    }
  }
  Context;
  if (s[0]) {
    log(LCAT_CONN, STR("Ident failed for %s: %s"), dcc[i].host, s);
    sprintf(s, STR("telnet@%s"), dcc[i].host);
    dcc_telnet_got_ident(i, s);
    return;
  }
  Context;
  dcc[j].sock = sock;
  dcc[j].port = 113;
  dcc[j].addr = ip;
  strcpy(dcc[j].host, dcc[i].host);
  strcpy(dcc[j].nick, "*");
  dcc[j].u.ident_sock = dcc[i].sock;
  dcc[j].timeval = now;
  dprintf(j, STR("%d, %d\n"), dcc[i].port, dcc[idx].port);
}

void eof_dcc_telnet(int idx)
{
  log(LCAT_ERROR, STR("(!) Listening port %d abruptly died."), dcc[idx].port);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

void display_telnet(int idx, char *buf)
{
  sprintf(buf, STR("lstn  %d"), dcc[idx].port);
}

struct dcc_table DCC_TELNET = {
  "TELNET",
  DCT_LISTEN,
  eof_dcc_telnet,
  dcc_telnet,
  0,
  0,
  display_telnet,
  0,
  0,
  0
};

void dcc_telnet_id(int idx, char *buf, int atr)
{
  int ok = 0,
    i;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0 };

  Context;
  strip_telnet(dcc[idx].sock, buf, &atr);
  buf[HANDLEN] = 0;
  /* toss out bad nicknames */
  if ((dcc[idx].nick[0] != '@') && (!wild_match(dcc[idx].nick, buf))) {
    dprintf(idx, STR("Sorry, that nickname format is invalid.\r\n"));
    log(LCAT_CONN, STR("Refused %s (bad nick)"), dcc[idx].host);
    log(LCAT_WARNING, STR("Refused %s (bad nick)"), dcc[idx].host);
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }
  dcc[idx].user = get_user_by_handle(userlist, buf);
  get_user_flagrec(dcc[idx].user, &fr, NULL);
  /* make sure users-only/bots-only connects are honored */
  if ((dcc[idx].status & STAT_BOTONLY) && !glob_bot(fr)) {
    dprintf(idx, STR("This telnet port is for bots only.\r\n"));
    log(LCAT_CONN, STR("Refused %s (non-bot)"), dcc[idx].host);
    log(LCAT_WARNING, STR("Refused %s (non-bot)"), dcc[idx].host);
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }
  if ((dcc[idx].status & STAT_USRONLY) && glob_bot(fr)) {
    dprintf(idx, STR("error Only users may connect at this port.\n"));
    log(LCAT_CONN, STR("Refused %s (non-user)"), dcc[idx].host);
    log(LCAT_WARNING, STR("Refused %s (non-user)"), dcc[idx].host);
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }
  dcc[idx].status &= ~(STAT_BOTONLY | STAT_USRONLY);
  if (glob_party(fr) || glob_bot(fr))
    ok = 1;
  if (!ok) {
    dprintf(idx, STR("You don't have access.\r\n"));
    log(LCAT_CONN, STR("Refused %s (invalid handle: %s)"), dcc[idx].host, buf);
    log(LCAT_WARNING, STR("Refused %s (invalid handle: %s)"), dcc[idx].host, buf);
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }
  if (glob_bot(fr)) {
    if (!strcasecmp(botnetnick, buf)) {
      dprintf(idx, STR("error You cannot link using my botnetnick.\n"));
      log(LCAT_CONN, STR("Refused telnet connection from %s (tried using my botnetnick)"), dcc[idx].host);
      log(LCAT_WARNING, STR("Refused telnet connection from %s (tried using my botnetnick)"), dcc[idx].host);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
    } else if (in_chain(buf)) {
      dprintf(idx, STR("error Already connected.\n"));
      log(LCAT_CONN, STR("Refused telnet connection from %s (duplicate)"), dcc[idx].host);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
    }
  }
  /* no password set? */
  if (!glob_bot(fr) && (u_pass_match(dcc[idx].user, "-"))) {
    dprintf(idx, STR("Can't telnet until you have a password set.\r\n"));
    log(LCAT_CONN, STR("Refused [%s]%s (no password)"), buf, dcc[idx].host);
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }
  ok = 0;
  dcc[idx].type = &DCC_CHAT_PASS;
  dcc[idx].timeval = now;
  if (glob_master(fr))
    ok = 1;
  else if (glob_party(fr)) {
    ok = 1;
    dcc[idx].status |= STAT_PARTY;
  }
  if (glob_bot(fr))
    ok = 1;
  if (!ok) {
    struct chat_info *ci;

    ci = dcc[idx].u.chat;
    dcc[idx].u.file = get_data_ptr(sizeof(struct file_info));

    dcc[idx].u.file->chat = ci;
  }
  correct_handle(buf);
  strcpy(dcc[idx].nick, buf);

  if (glob_bot(fr)) {
    int snum = -1;

    for (i = 0; i < MAXSOCKS; i++) {
      if (!(socklist[i].flags & SOCK_UNUSED)
	  && (socklist[i].sock == dcc[idx].sock)) {
	snum = i;
	break;
      }
    }
    if (snum >= 0) {
      char initkey[17],
       *tmp2;
      char tmp[256];
      MD5_CTX ctx;

      sprintf(tmp, STR("%8x@%4x@%s@%s"), htonl(dcc[idx].addr), htons(dcc[idx].port), botnetnick, dcc[idx].nick);
      MD5Init(&ctx);
      MD5Update(&ctx, tmp, strlen(tmp));
      MD5Final(socklist[snum].okey, &ctx);
      *(dword *) & initkey[0] = rand();
      *(dword *) & initkey[4] = rand();
      *(dword *) & initkey[8] = rand();
      *(dword *) & initkey[12] = rand();
      for (i = 0; i <= 15; i++) {
	if (!socklist[snum].okey[i])
	  socklist[snum].okey[i] = 1;
	if (!initkey[i])
	  initkey[i] = 1;
      }
      socklist[snum].okey[16] = 0;
      socklist[snum].oseed = rand();
      socklist[snum].iseed = socklist[snum].oseed;
      initkey[16] = 0;
      tmp2 = encrypt_string(netpass, initkey);
      log(LCAT_BOT, STR("Sending encrypted link handshake to %s..."), dcc[idx].nick);
      socklist[snum].encstatus = 1;
      dprintf(idx, STR("elink %s %lu\n"), tmp2, socklist[snum].oseed);
      strcpy(socklist[snum].okey, initkey);
      strcpy(socklist[snum].ikey, initkey);
      nfree(tmp2);
    } else {
      log(LCAT_ERROR, STR("Couldn't find socket for %s connection?? Shouldn't happen :/"), dcc[idx].nick);
      killsock(dcc[idx].sock);
      lostdcc(idx);
    }
  } else {
    /*    *note* The MD5 digest used above to prevent cleartext passwords
     *  being sent across the net will _only_ work when we have the cleartext
     *  password. User passwords are encrypted (with blowfish usually) so the
     *  same thing cant be done. Botnet passwords are always stored in
     *  cleartext, or at least something that can be reversed. <Cybah>
     */
    dprintf(idx, STR("\n%s\377\373\001\n"), STR("Enter your password."));
    /* turn off remote telnet echo: IAC WILL ECHO */
  }
}

void eof_dcc_telnet_id(int idx)
{
  log(LCAT_CONN, STR("Lost connection while identing [%s/%d]"), dcc[idx].host, dcc[idx].port);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

void timeout_dcc_telnet_id(int idx)
{
  dprintf(idx, STR("Timeout.\n"));
  log(LCAT_CONN, STR("Timeout: bot link to %s at %s:%d"), dcc[idx].nick, dcc[idx].host);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

void display_dcc_telnet_id(int idx, char *buf)
{
  sprintf(buf, STR("t-in  waited %lus"), now - dcc[idx].timeval);
}

struct dcc_table DCC_TELNET_ID = {
  "TELNET_ID",
  0,
  eof_dcc_telnet_id,
  dcc_telnet_id,
  &password_timeout,
  timeout_dcc_telnet_id,
  display_dcc_telnet_id,
  expmem_dcc_general,
  kill_dcc_general,
  out_dcc_general
};

#ifdef G_USETCL
int call_tcl_func(char *name, int idx, char *args)
{
  char s[11];

  sprintf(s, "%d", idx);
  Tcl_SetVar(interp, "_n", s, 0);
  Tcl_SetVar(interp, "_a", args, 0);
  if (Tcl_VarEval(interp, name, STR(" $_n $_a"), NULL) == TCL_ERROR) {
    log(LCAT_ERROR, STR("Tcl error [%s]: %s"), name, interp->result);
    return -1;
  }
  return (atoi(interp->result));
}
#endif

#ifdef G_USETCL
void dcc_script(int idx, char *buf, int len)
{
  void *old = NULL;
  long oldsock = dcc[idx].sock;

  strip_telnet(dcc[idx].sock, buf, &len);
  if (!len)
    return;
  dcc[idx].timeval = now;
  if (call_tcl_func(dcc[idx].u.script->command, dcc[idx].sock, buf)) {
    Context;
    if ((dcc[idx].sock != oldsock) || (idx > max_dcc))
      return;			/* drummer: this happen after killdcc */
    old = dcc[idx].u.script->u.other;
    dcc[idx].type = dcc[idx].u.script->type;
    nfree(dcc[idx].u.script);
    dcc[idx].u.other = old;
    if (dcc[idx].type == &DCC_SOCKET) {
      /* kill the whole thing off */
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
    }
    if (dcc[idx].type == &DCC_CHAT) {
      if (dcc[idx].u.chat->channel >= 0) {
	chanout_but(-1, dcc[idx].u.chat->channel, STR("*** %s has joined the party line.\n"), dcc[idx].nick);
	Context;
	if (dcc[idx].u.chat->channel < 10000)
	  botnet_send_join_idx(idx, -1);
	check_tcl_chjn(botnetnick, dcc[idx].nick, dcc[idx].u.chat->channel, geticon(idx), dcc[idx].sock, dcc[idx].host);
      }
      check_tcl_chon(dcc[idx].nick, dcc[idx].sock);
    }
  }
}

void eof_dcc_script(int idx)
{
  void *old;
  int oldflags;

  Context;
  /* This will stop a killdcc from working, incase the script tries
   * to kill it's controlling socket while handling an EOF <cybah> */
  oldflags = dcc[idx].type->flags;
  dcc[idx].type->flags &= ~(DCT_VALIDIDX);
  /* tell the script they're gone: */
  call_tcl_func(dcc[idx].u.script->command, dcc[idx].sock, "");
  /* restore the flags */
  dcc[idx].type->flags = oldflags;
  Context;
  old = dcc[idx].u.script->u.other;
  dcc[idx].type = dcc[idx].u.script->type;
  nfree(dcc[idx].u.script);
  dcc[idx].u.other = old;
  /* then let it fall thru to the real one */
  if (dcc[idx].type && dcc[idx].type->eof)
    dcc[idx].type->eof(idx);
  else {
    log(LCAT_CONN, STR("*** ATTENTION: DEAD SOCKET (%d) OF TYPE %s UNTRAPPED"), dcc[idx].sock, dcc[idx].type->name);
    killsock(dcc[idx].sock);
    lostdcc(idx);
  }
}

void display_dcc_script(int idx, char *buf)
{
  sprintf(buf, STR("scri  %s"), dcc[idx].u.script->command);
}

int expmem_dcc_script(void *x)
{
  register struct script_info *p = (struct script_info *) x;
  int tot = sizeof(struct script_info);

  if (p->type && p->u.other)
    tot += p->type->expmem(p->u.other);
  return tot;
}

void kill_dcc_script(int idx, void *x)
{
  register struct script_info *p = (struct script_info *) x;

  if (p->type && p->u.other)
    p->type->kill(idx, p->u.other);
  nfree(p);
}

void out_dcc_script(int idx, char *buf, void *x)
{
  register struct script_info *p = (struct script_info *) x;

  if (p && p->type && p->u.other)
    p->type->output(idx, buf, p->u.other);
  else
    tputs(dcc[idx].sock, buf, strlen(buf));
}

struct dcc_table DCC_SCRIPT = {
  "SCRIPT",
  DCT_VALIDIDX,
  eof_dcc_script,
  dcc_script,
  0,
  0,
  display_dcc_script,
  expmem_dcc_script,
  kill_dcc_script,
  out_dcc_script
};

#endif

#ifdef G_WGET
int start_wget(int idx, char * url, char * local) {
  struct wget_info * w;
  char *p, *p2;
  char *wgetuser = NULL, *wgetpass = NULL, *wgetsite = NULL, *wgetpath = NULL, *wgetfile = NULL;
  if (!url) 
    return 0;
  p=strstr(url, "://");
  if (!p) {
    dprintf(idx, STR("Invalid url\n"));
    return 0;
  }
  *p=0;
  if (strcmp(url, STR("http"))) {
    dprintf(idx, STR("Only http:// URLs are supported\n"));
    return 0;
  }
  p += 3;
  url=p;
  p=strchr(url, '@');
  if (p) {
    *p++ = 0;
    p2 = strchr(url, ':');
    if (!p2) {
      dprintf(idx, STR("Invalid URL - user specified but no password\n"));
      return 0;
    }
    *p2++=0;
    wgetuser=url;
    wgetpass=p2;
    url=p;
  }
  p=strchr(url, '/');
  if (p) {
    *p++=0;
    p2=strrchr(p, '/');
    if (p2) {
      *p2++=0;
      wgetfile = p2;
      wgetpath = p;
    } else {
      wgetfile = p;
    }
    wgetsite = url;
  } else
    wgetsite = url;

  i = new_dcc(&DCC_WGET, sizeof(struct wget_info));
  w = (struct wget_info *) dcc[i].u.other;
  w->idx=idx;
  if (idx>=0) {
    strncpy0(w->nick, dcc[idx].nick, HANDLEN+1);
  }
  if (wgetpass) {
    

  }

}
/*
struct wget_info {
  int idx;
  char nick[HANDLEN+1];
  char request[512];
  char local[256];
  FILE * lfile;
  int status;
  int expected;
  int received;
};
*/
void dcc_wget(int idx, char *buf, int len)
{
  void *old = NULL;
  long oldsock = dcc[idx].sock;

  strip_telnet(dcc[idx].sock, buf, &len);
  if (!len)
    return;
  dcc[idx].timeval = now;
  if (call_tcl_func(dcc[idx].u.script->command, dcc[idx].sock, buf)) {
    Context;
    if ((dcc[idx].sock != oldsock) || (idx > max_dcc))
      return;			/* drummer: this happen after killdcc */
    old = dcc[idx].u.script->u.other;
    dcc[idx].type = dcc[idx].u.script->type;
    nfree(dcc[idx].u.script);
    dcc[idx].u.other = old;
    if (dcc[idx].type == &DCC_SOCKET) {
      /* kill the whole thing off */
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
    }
    if (dcc[idx].type == &DCC_CHAT) {
      if (dcc[idx].u.chat->channel >= 0) {
	chanout_but(-1, dcc[idx].u.chat->channel, STR("*** %s has joined the party line.\n"), dcc[idx].nick);
	Context;
	if (dcc[idx].u.chat->channel < 10000)
	  botnet_send_join_idx(idx, -1);
	check_tcl_chjn(botnetnick, dcc[idx].nick, dcc[idx].u.chat->channel, geticon(idx), dcc[idx].sock, dcc[idx].host);
      }
      check_tcl_chon(dcc[idx].nick, dcc[idx].sock);
    }
  }
}

void eof_dcc_wget(int idx)
{
  void *old;
  int oldflags;

  Context;
  /* This will stop a killdcc from working, incase the script tries
   * to kill it's controlling socket while handling an EOF <cybah> */
  oldflags = dcc[idx].type->flags;
  dcc[idx].type->flags &= ~(DCT_VALIDIDX);
  /* tell the script they're gone: */
  call_tcl_func(dcc[idx].u.script->command, dcc[idx].sock, "");
  /* restore the flags */
  dcc[idx].type->flags = oldflags;
  Context;
  old = dcc[idx].u.script->u.other;
  dcc[idx].type = dcc[idx].u.script->type;
  nfree(dcc[idx].u.script);
  dcc[idx].u.other = old;
  /* then let it fall thru to the real one */
  if (dcc[idx].type && dcc[idx].type->eof)
    dcc[idx].type->eof(idx);
  else {
    log(LCAT_CONN, STR("*** ATTENTION: DEAD SOCKET (%d) OF TYPE %s UNTRAPPED"), dcc[idx].sock, dcc[idx].type->name);
    killsock(dcc[idx].sock);
    lostdcc(idx);
  }
}

void display_dcc_wget(int idx, char *buf)
{
  sprintf(buf, STR("scri  %s"), dcc[idx].u.script->command);
}

int expmem_dcc_wget(void *x)
{
  register struct script_info *p = (struct script_info *) x;
  int tot = sizeof(struct script_info);

  if (p->type && p->u.other)
    tot += p->type->expmem(p->u.other);
  return tot;
}

void kill_dcc_wget(int idx, void *x)
{
  register struct script_info *p = (struct script_info *) x;

  if (p->type && p->u.other)
    p->type->kill(idx, p->u.other);
  nfree(p);
}

void out_dcc_wget(int idx, char *buf, void *x)
{
  register struct script_info *p = (struct script_info *) x;

  if (p && p->type && p->u.other)
    p->type->output(idx, buf, p->u.other);
  else
    tputs(dcc[idx].sock, buf, strlen(buf));
}

struct dcc_table DCC_WGET = {
  "WGET",
  0,
  eof_dcc_wget,
  dcc_wget,
  &wget_timeout,
  timout_dcc_wget,
  display_dcc_wget,
  expmem_dcc_wget,
  kill_dcc_wget,
  out_dcc_wget
};
#endif

void dcc_socket(int idx, char *buf, int len)
{
}

void eof_dcc_socket(int idx)
{
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

void display_dcc_socket(int idx, char *buf)
{
  strcpy(buf, STR("sock  (stranded)"));
}

struct dcc_table DCC_SOCKET = {
  "SOCKET",
  DCT_VALIDIDX,
  eof_dcc_socket,
  dcc_socket,
  0,
  0,
  display_dcc_socket,
  0,
  0,
  0
};

void display_dcc_lost(int idx, char *buf)
{
  strcpy(buf, STR("lost"));
}

struct dcc_table DCC_LOST = {
  "LOST",
  0,
  0,
  dcc_socket,
  0,
  0,
  display_dcc_lost,
  0,
  0,
  0
};

void dcc_identwait(int idx, char *buf, int len)
{
  /* ignore anything now */
  Context;
}

void eof_dcc_identwait(int idx)
{
  int i;

  log(LCAT_CONN, STR("Lost connection while identing [%s/%d]"), dcc[idx].host, dcc[idx].port);
  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type == &DCC_IDENT) && (dcc[i].u.ident_sock == dcc[idx].sock)) {
      killsock(dcc[i].sock);	/* cleanup ident socket */
      dcc[i].u.other = 0;
      lostdcc(i);
      break;
    }
  killsock(dcc[idx].sock);	/* cleanup waiting socket */
  dcc[idx].u.other = 0;
  lostdcc(idx);
}

void display_dcc_identwait(int idx, char *buf)
{
  sprintf(buf, STR("idtw  waited %lus"), now - dcc[idx].timeval);
}

struct dcc_table DCC_IDENTWAIT = {
  "IDENTWAIT",
  0,
  eof_dcc_identwait,
  dcc_identwait,
  0,
  0,
  display_dcc_identwait,
  0,
  0,
  0
};

void dcc_ident(int idx, char *buf, int len)
{
  char response[512],
    uid[512],
    buf1[UHOSTLEN];
  int i;

  Context;
  sscanf(buf, STR("%*[^:]:%[^:]:%*[^:]:%[^\n]\n"), response, uid);
  rmspace(response);
  if (response[0] != 'U') {
    dcc[idx].timeval = now;
    return;
  }
  rmspace(uid);
  uid[20] = 0;			/* 20 character ident max */
  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type == &DCC_IDENTWAIT) && (dcc[i].sock == dcc[idx].u.ident_sock)) {
      simple_sprintf(buf1, STR("%s@%s"), uid, dcc[idx].host);
      dcc_telnet_got_ident(i, buf1);
    }
  dcc[idx].u.other = 0;
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

void eof_dcc_ident(int idx)
{
  char buf[UHOSTLEN];
  int i;

  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type == &DCC_IDENTWAIT) && (dcc[i].sock == dcc[idx].u.ident_sock)) {
      log(LCAT_CONN, STR("Timeout/EOF ident connection"));
      simple_sprintf(buf, STR("telnet@%s"), dcc[idx].host);
      dcc_telnet_got_ident(i, buf);
    }
  killsock(dcc[idx].sock);
  dcc[idx].u.other = 0;
  lostdcc(idx);
}

void display_dcc_ident(int idx, char *buf)
{
  sprintf(buf, STR("idnt  (sock %d)"), dcc[idx].u.ident_sock);
}

struct dcc_table DCC_IDENT = {
  "IDENT",
  0,
  eof_dcc_ident,
  dcc_ident,
  &identtimeout,
  eof_dcc_ident,
  display_dcc_ident,
  0,
  0,
  0
};

void dcc_telnet_got_ident(int i, char *host)
{
  int idx;
  char x[1024];

  for (idx = 0; idx < dcc_total; idx++)
    if ((dcc[idx].type == &DCC_TELNET) && (dcc[idx].sock == dcc[i].u.ident_sock))
      break;
  dcc[i].u.other = 0;
  if (dcc_total == idx) {
    log(LCAT_CONN, STR("Lost ident wait telnet socket!!"));
    killsock(dcc[i].sock);
    lostdcc(i);
    return;
  }
  strncpy0(dcc[i].host, host, UHOSTMAX);
  simple_sprintf(x, STR("telnet!%s"), dcc[i].host);
  if (protect_telnet && !make_userfile) {
    struct userrec *u;
    int ok = 1;

    Context;
    u = get_user_by_host(x);
    /* not a user or +p & require p OR +o */
    if (!u)
      ok = 0;
#ifdef HUB
    else if (!(u->flags & USER_HUB))
      ok = 0;
#endif
    else if (!(u->flags & USER_PARTY))
      ok = 0;
    if (!ok && u && (u->flags & USER_BOT))
      ok = 1;
    if (!ok) {
      log(LCAT_WARNING, STR("Denied telnet: %s"), dcc[i].host);
      killsock(dcc[i].sock);
      lostdcc(i);
      return;
    }
  }
  Context;
  if (match_ignore(x)) {
    killsock(dcc[i].sock);
    lostdcc(i);
    return;
  }
  /* script? */
#ifdef G_USETCL
  if (!strcmp(dcc[idx].nick, STR("(script)"))) {
    dcc[i].type = &DCC_SOCKET;
    dcc[i].u.other = NULL;
    strcpy(dcc[i].nick, "*");
    check_tcl_listen(dcc[idx].host, dcc[i].sock);
    return;
  }
#endif
  dcc[i].type = &DCC_TELNET_ID;
  dcc[i].u.chat = get_data_ptr(sizeof(struct chat_info));
  bzero(dcc[i].u.chat, sizeof(struct chat_info));

  /* copy acceptable-nick/host mask */
  dcc[i].status = STAT_TELNET | STAT_ECHO;
  if (!strcmp(dcc[idx].nick, STR("(bots)")))
    dcc[i].status |= STAT_BOTONLY;
  if (!strcmp(dcc[idx].nick, STR("(users)")))
    dcc[i].status |= STAT_USRONLY;
  /* copy acceptable-nick/host mask */
  strncpy0(dcc[i].nick, dcc[idx].host, HANDLEN+1);
  dcc[i].timeval = now;

  /*!!ENCDCC Send challenge here */
  dprintf(i, STR("No Access.\n"));
}


