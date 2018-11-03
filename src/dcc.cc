/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2014 Bryan Drewery
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

/*
 * dcc.c -- handles:
 *   activity on a dcc socket
 *   disconnect on a dcc socket
 *   ...and that's it!  (but it's a LOT)
 *
 */


#include "common.h"
#include "dcc.h"
#include "settings.h"
#include "enclink.h"
#include "binds.h"
#include "adns.h"
#include "main.h"
#include "cmds.h"
#include "color.h"
#include "net.h"
#include "response.h"
#include "misc.h"
#include "users.h"
#include "userrec.h"
#include "userent.h"
#include "match.h"
#include "auth.h"
#include "dccutil.h"
#include "crypt.h"
#include "chanprog.h"
#include "botmsg.h"
#include "botcmd.h"
#include "botnet.h"
#include "socket.h"
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include "tandem.h"
#include "core_binds.h"
#include "src/mod/console.mod/console.h"
#include <bdlib/src/String.h>
#include <bdlib/src/Array.h>


struct cmd_pass *cmdpass = NULL;
struct dcc_t *dcc = NULL;       /* DCC list                                */
time_t timesync = 0;
int dcc_total = 0;              /* size of dcc table                             */
int dccn = 0;			/* actual number of dcc entries */
int uplink_idx = -1;

static interval_t password_timeout = 40;       /* Time to wait for a password from a user */
static interval_t auth_timeout = 80;
static interval_t bot_timeout = 15;    /* Bot timeout value                       */
static interval_t identtimeout = 5;   /* Timeout value for ident lookups         */
static interval_t dupwait_timeout = 5; /* Timeout for rejecting duplicate entries */

bool protect_telnet = 0;  /* Even bother with ident lookups :)       */
static int flood_telnet_thr = 10;       /* Number of telnet connections to be
                                         * considered a flood                      */
static interval_t flood_telnet_time = 5;       /* In how many seconds?                    */

static void dcc_telnet_got_ident(int, char *);
static void dcc_telnet_pass(int, int);

static void
strip_telnet(int sock, char *buf, int *len)
{
  unsigned char *p = (unsigned char *) buf, *o = (unsigned char *) buf;
  int mark;

  while (*p != 0) {
    while ((*p != TLN_IAC) && (*p != 0)) {
      if (*p == 0xA0) {
        *o++ = 32;
        p++;
      } else
        *o++ = *p++;
    }

    if (*p == TLN_IAC) {
      p++;
      mark = 2;
      if (!*p)
        mark = 1;               /* bogus */
      if ((*p >= TLN_WILL) && (*p <= TLN_DONT)) {
        mark = 3;
        if (!*(p + 1))
          mark = 2;             /* bogus */
      }
      if (*p == TLN_WILL) {
        /* WILL X -> response: DONT X */
        /* except WILL ECHO which we just smile and ignore */
        if (*(p + 1) != TLN_ECHO) {
          if (write(sock, TLN_IAC_C TLN_DONT_C, 2) == -1) {
            ;
          }
          if (write(sock, p + 1, 1) == -1) {
            ;
          }
        }
      }
      if (*p == TLN_DO) {
        /* DO X -> response: WONT X */
        /* except DO ECHO which we just smile and ignore */
        if (*(p + 1) != TLN_ECHO) {
          if (write(sock, TLN_IAC_C TLN_WONT_C, 2) == -1) {
            ;
          }
          if (write(sock, p + 1, 1) == -1) {
            ;
          }
        }
      }
      if (*p == TLN_AYT) {
        /* "are you there?" */
        /* response is: "hell yes!" */
        if (write(sock, "\r\nHell, yes!\r\n", 14) == -1) {
          ;
        }
      }
      /* Anything else can probably be ignored */
      p += mark - 1;
      *len = *len - mark;
    }
  }
  *o = *p;
}

void
send_sysinfo()
{
  char *username = NULL, *sysname = NULL, *nodename = NULL, *arch = NULL, *osver = NULL;
  struct utsname un;
  bool gotun = 0;

  if (uname(&un) < 0)
    gotun = 0;
  else 
    gotun = 1;

  username = (char *) get_user(&USERENTRY_USERNAME, conf.bot->u);
  sysname = (char *) get_user(&USERENTRY_OS, conf.bot->u);
  nodename = (char *) get_user(&USERENTRY_NODENAME, conf.bot->u);
  arch = (char *) get_user(&USERENTRY_ARCH, conf.bot->u);
  osver = (char *) get_user(&USERENTRY_OSVER, conf.bot->u);

  const char *usysname = NULL, *uusername = NULL, *unodename = NULL, *uarch = NULL, *uosver = NULL;

  usysname = gotun ? un.sysname : "*";
  uusername = conf.username ? conf.username : "*";
  unodename = gotun ? un.nodename : "*";
  uarch = gotun ? un.machine : "*";
  uosver = gotun ? un.release : "*";

  if (((sysname && strcasecmp(sysname, usysname)) ||
       (username && strcasecmp(username, uusername)) ||
       (nodename && strcasecmp(nodename, unodename)) ||
       (arch && strcasecmp(arch, uarch)) ||
       (osver && strcasecmp(osver, uosver))
      ) ||
      ((!sysname && usysname) || 
       (!username && uusername) || 
       (!nodename && unodename) || 
       (!arch && uarch) ||
       (!osver && uosver)
      )
      ) {
      char buf[201] = "";
      size_t len = 0;

      len = simple_snprintf(buf, sizeof(buf), "si %s %s %s %s %s\n",
            conf.username ? conf.username : "*", gotun ? un.sysname : "*", gotun ? un.nodename : "*",
            gotun ? un.machine : "*", gotun ? un.release : "*");

      send_uplink(buf, len);
  }
}

void
send_timesync(int idx)
{
  /* Send timesync to idx, or all lower bots if idx<0 */
  if (idx >= 0)
    dprintf(idx, "ts %li\n", (long)(timesync + now));
  else {

    for (int i = 0; i < dcc_total; i++) {
      if (dcc[i].type && (dcc[i].type == &DCC_BOT) && (bot_aggressive_to(dcc[i].user))) {
        dprintf(i, "ts %li\n", (long)(timesync + now));
      }
    }
  }
}

static void
greet_new_bot(int idx)
{
  dcc[idx].timeval = now;
  dcc[idx].u.bot->version[0] = 0;
  dcc[idx].u.bot->sysname[0] = 0;
  dcc[idx].u.bot->numver = 0;
  // Reject -o bots, and if we're a localhub who hasnt linked to hub yet, dont allow links in
  if ((conf.bot->hub || conf.bot->localhub) && dcc[idx].user && (!(dcc[idx].user->flags & USER_OP) || (conf.bot->localhub && (dcc[idx].status & STAT_UNIXDOMAIN) && !have_linked_to_hub))) {
    if (!(dcc[idx].user->flags & USER_OP)) {
      putlog(LOG_BOTS, "*", "Rejecting link from %s (Bot is not +o)", dcc[idx].nick);
      dprintf(idx, "error You are being rejected.\n");
    } else if (conf.bot->localhub && !have_linked_to_hub) {
      putlog(LOG_BOTS, "*", "Delaying link from %s", dcc[idx].nick);
      dprintf(idx, "error Delaying your link until I get userfile.\n");
    }
    dprintf(idx, "bye\n");
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }

  if (bot_hublevel(dcc[idx].user) == 999)
    dcc[idx].status |= STAT_LEAF;
  dcc[idx].status |= STAT_LINKING;

  dprintf(idx, "v 1001500 9 Wraith %s %d %d %li %s %s\n", egg_version,
      conf.bot->u->fflags, conf.bot->localhub, (long)buildts, commit,
      egg_version);

  for (int i = 0; i < dcc_total; i++) {
    if (dcc[i].type && dcc[i].type == &DCC_FORK_BOT) {
      killsock(dcc[i].sock);
      lostdcc(i);
    }
  }
}

static void
bot_version(int idx, char *par)
{
  char *work;

  dcc[idx].timeval = now;
  if (in_chain(dcc[idx].nick)) {
    dprintf(idx, "error Sorry, already connected.\n");
    dprintf(idx, "bye\n");
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }

  if ((par[0] >= '0') && (par[0] <= '9')) {
    work = newsplit(&par);
    dcc[idx].u.bot->numver = atoi(work);
    /* old numver crap */
  } else
    dcc[idx].u.bot->numver = 0;

  dprintf(idx, "tb %s\n", conf.bot->nick);

  newsplit(&par); //Ignore handlen

#ifdef no
  size_t l = atol(newsplit(&par));

  if (l != HANDLEN) {
    putlog(LOG_BOTS, "*", "Non-matching handle lengths with %s, they use %zu characters.", dcc[idx].nick, l);
    dprintf(idx, "error Non-matching handle length: mine %d, yours %zu\n", HANDLEN, l);
    dprintf(idx, "bye %s\n", "bad handlen");
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }
#endif

  char x[1024] = "", *vversion = NULL, *vcommit = NULL;
  int vlocalhub = -1;
  time_t vbuildts = 0;
  int fflags = -1;

  strlcpy(dcc[idx].u.bot->version, par, 120);
  newsplit(&par);               /* 'ver' */
  newsplit(&par);               /* handlen */
  /* fflags / (backward compat: network) */
  if (par[0]) {
    work = newsplit(&par);
    /* Must support older bots which sent '<->' here for network. */
    if (strcmp(work, "<->")) {
      fflags = atoi(work);
    } else {
      /* Older bot doesn't have feature flags. */
      fflags = 0;
    }
  }
  if (par[0])
    vlocalhub = atoi(newsplit(&par));
  if (par[0])
    vbuildts = atol(newsplit(&par));
  if (par[0])
    vcommit = newsplit(&par);
  if (par[0])
    vversion = newsplit(&par);

  if (vlocalhub == -1 || vbuildts == 0 || vcommit == NULL || vversion == NULL) {
    putlog(LOG_BOTS, "*", "Invalid link from %s [likely a hack].\n", dcc[idx].nick);
    dprintf(idx, "error badbot\n");
    dprintf(idx, "bye badbot\n");
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }

  if (conf.bot->hub || (conf.bot->localhub && (dcc[idx].status & STAT_UNIXDOMAIN))) {
    putlog(LOG_BOTS, "*", "Linked to %s.\n", dcc[idx].nick);
    chatout("*** Linked to %s.\n", dcc[idx].nick);
  } else {
    putlog(LOG_BOTS, "*", "Linked to botnet.");
    chatout("*** Linked to botnet.\n");
  }

  if (conf.bot->hub || conf.bot->localhub) {
    if (bot_hublevel(dcc[idx].user) < 999) {
      if (!bot_aggressive_to(dcc[idx].user)) {   //not aggressive, so they are technically my uplink.
        uplink_idx = idx;
        // This is now done in share_endstartup
        //have_linked_to_hub = 1;
      }
      dcc[idx].hub = 1;
    }

    botnet_send_nlinked(idx, dcc[idx].nick, conf.bot->nick, '!', vlocalhub, vbuildts, vcommit, vversion, fflags);
  } else {
        // This is now done in share_endstartup
        //have_linked_to_hub = 1;
    uplink_idx = idx;
    dcc[idx].hub = 1;
  }

  dump_links(idx);

  touch_laston(dcc[idx].user, "linked", now);
  dcc[idx].type = &DCC_BOT;
  addbot(dcc[idx].nick, dcc[idx].nick, conf.bot->nick, '-', vlocalhub, vbuildts, vcommit, vversion, fflags);
  simple_snprintf(x, sizeof x, "v 1001500");
  bot_share(idx, x);
  dprintf(idx, "el\n");
}

void
failed_link(int idx)
{
  char nick[NICKLEN] = "", s1[NICKLEN + 17 + 1] = "";

  if (dcc[idx].u.bot->linker[0]) {
    simple_snprintf(s1, sizeof s1, "Couldn't link to %s.", dcc[idx].nick);
    strlcpy(nick, dcc[idx].u.bot->linker, sizeof(s1));
    add_note(nick, conf.bot->nick, s1, -2, 0);
  }
  if (dcc[idx].u.bot->numver >= (-1))
    putlog(LOG_BOTS, "*", "Failed link to %s.", dcc[idx].nick);
  if (dcc[idx].sock != -1) {
    killsock(dcc[idx].sock);
    dcc[idx].sock = -1;
  }
  strlcpy(nick, dcc[idx].nick, sizeof(nick));
  lostdcc(idx);
  if (conf.bot->hub || conf.bot->localhub)
    strlcpy(autolink_failed, nick, HANDLEN + 1);
}

static void
cont_link(int idx, char *buf, int ii)
{
  /* If we're already connected somewhere, unlink and idle a sec */
  for (int i = 0; i < dcc_total; i++) {
    if (unlikely(dcc[i].type && (dcc[i].type == &DCC_BOT) && (!bot_aggressive_to(dcc[i].user)))) {
      putlog(LOG_BOTS, "*", "Unlinking %s - restructure", dcc[i].nick);
      botnet_send_unlinked(i, dcc[i].nick, "Restructure");
      killsock(dcc[i].sock);
      lostdcc(i);
      usleep(1000 * 500);
      break;
    }
  }

  dcc[idx].type = &DCC_BOT_NEW;
  dcc[idx].u.bot->numver = 0;

  if (ii == 3)
    dprintf(idx, STR("-%s\n"), conf.bot->nick);
    /* wait for "neg?" now */

  /* now we wait to negotiate an encryption */
  return;
}

static void
dcc_bot_new(int idx, char *buf, int x)
{
/*  struct userrec *u = get_user_by_handle(userlist, dcc[idx].nick); */
  char *code = NULL;

  strip_telnet(dcc[idx].sock, buf, &x);
  code = newsplit(&buf);
  if (!strcasecmp(code, "goodbye!")) {
    greet_new_bot(idx);
  } else if (!strcasecmp(code, "v")) {
    bot_version(idx, buf);
  } else if (!strcasecmp(code, STR("neg!"))) {	/* something to parse in enclink.c */
    link_parse(idx, buf);
  } else if (!strcasecmp(code, STR("neg?"))) {	/* we're connecting to THEM */
    link_challenge_to(idx, buf);
  } else if (!strcasecmp(code, "error")) {
    putlog(LOG_MISC, "*", "ERROR linking %s: %s", dcc[idx].nick, buf);
    killsock(dcc[idx].sock);
    lostdcc(idx);
  } else if (strcmp(code, "")) {
    /* Invalid password/digest on leaf */
    putlog(LOG_WARN, "*", STR("%s failed encrypted link handshake"), dcc[idx].nick);
    killsock(dcc[idx].sock);
    lostdcc(idx);
  }
  /* Ignore otherwise */
}

static void
eof_dcc_bot_new(int idx)
{
  putlog(LOG_BOTS, "*", "Lost Bot: %s", dcc[idx].nick);
  if (dcc[idx].hub)
    putlog(LOG_BOTS, "*", "See log on %s for disconnect reason.\n", dcc[idx].nick);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
timeout_dcc_bot_new(int idx)
{
  if (conf.bot->hub)
    putlog(LOG_BOTS, "*", "Timeout: bot link to %s at %s:%d", dcc[idx].nick, dcc[idx].host, dcc[idx].port);
  else
    putlog(LOG_BOTS, "*", "Timeout: bot link to %s", dcc[idx].nick);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
display_dcc_bot_new(int idx, char *buf, size_t bufsiz)
{
  simple_snprintf(buf, bufsiz, "bot*  waited %ds", (int) (now - dcc[idx].timeval));
}

static void
free_dcc_bot_(int n, void *x)
{
  if (dcc[n].type == &DCC_BOT) {
    unvia(n, findbot(dcc[n].nick));
    /* Stop sharing with this bot in case rembot->laston is shared out. */
    dcc[n].status &= ~STAT_SHARE;
    rembot(dcc[n].nick);
  }
  free(x);
}

struct dcc_table DCC_BOT_NEW = {
  "BOT_NEW",
  0,
  eof_dcc_bot_new,
  dcc_bot_new,
  &bot_timeout,
  timeout_dcc_bot_new,
  display_dcc_bot_new,
  free_dcc_bot_,
  NULL,
  NULL
};

static void
dcc_bot(int idx, char *code, int i)
{
  char *msg = NULL;

  strip_telnet(dcc[idx].sock, code, &i);
  if (debug_output) {
/*    if (code[0] != 'z' && code[1] != 'b' && code[2] != ' ') { */
    if (code[0] == 's')
      putlog(LOG_BOTSHARE, "@", "{%s} %s", dcc[idx].nick, code);
    else
      putlog(LOG_BOTNET, "@", "<-[%s] %s", dcc[idx].nick, code);
/*     } */
  }
  msg = strchr(code, ' ');
  if (msg) {
    *msg = 0;
    msg++;
  } else
    msg = "";

  parse_botcmd(idx, code, msg);
}

static void
eof_dcc_bot(int idx)
{
  char x[1024] = "";
  int bots, users;

  bots = bots_in_subtree(findbot(dcc[idx].nick));
  users = users_in_subtree(findbot(dcc[idx].nick));
  simple_snprintf(x, sizeof x,
               "Lost bot: %s (lost %d bot%s and %d user%s)",
               dcc[idx].nick, bots, (bots != 1) ? "s" : "", users, (users != 1) ? "s" : "");
  putlog(LOG_BOTS, "*", "%s.", x);
  chatout("*** %s\n", x);

  botnet_send_unlinked(idx, dcc[idx].nick, x);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
display_dcc_bot(int idx, char *buf, size_t bufsiz)
{
  size_t i = simple_snprintf(buf, bufsiz, "bot   flags: ");

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
  buf[i++] = b_status(idx) & STAT_OFFEREDU ? 'B' : 'b';
  buf[i++] = b_status(idx) & STAT_SENDINGU ? 'D' : 'd';
  buf[i++] = b_status(idx) & STAT_GETTINGU ? 'E' : 'e';
  buf[i++] = b_status(idx) & STAT_UNIXDOMAIN ? 'Z' : 'z';
#ifdef USE_IPV6
  if (sockprotocol(dcc[idx].sock) == AF_INET6 && dcc[idx].host6[0])
    buf[i++] = '6';
#endif /* USE_IPV6 */
  buf[i++] = 0;
}

static void
display_dcc_fork_bot(int idx, char *buf, size_t bufsiz)
{
  simple_snprintf(buf, bufsiz, "conn  bot");
}

struct dcc_table DCC_BOT = {
  "BOT",
  DCT_BOT,
  eof_dcc_bot,
  dcc_bot,
  NULL,
  NULL,
  display_dcc_bot,
  free_dcc_bot_,
  NULL,
  NULL
};

struct dcc_table DCC_FORK_BOT = {
  "FORK_BOT",
  0,
  failed_link,
  cont_link,
  &connect_timeout,
  failed_link,
  display_dcc_fork_bot,
  free_dcc_bot_,
  NULL,
  NULL
};

static void
dcc_identd(int idx, char *buf, int atr)
{
  char outbuf[1024] = "";

  size_t len = simple_snprintf(outbuf, sizeof outbuf, "%s : USERID : UNIX : %s\n", buf, botuser);
  tputs(dcc[idx].sock, outbuf, len);

  /* just close it, functions neededing it will open it. */
  identd_close();
}

static void
eof_dcc_identd(int idx)
{
  /* dont bother logging it, who gives a fuck */
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
display_dcc_identd(int idx, char *buf, size_t bufsiz)
{
  simple_snprintf(buf, bufsiz, "idtd  %d%s", dcc[idx].port, (dcc[idx].status & LSTN_PUBLIC) ? " pub" : "");
}

struct dcc_table DCC_IDENTD = {
  "IDENTD",
  DCT_LISTEN,
  eof_dcc_identd,
  dcc_identd,
  NULL,
  NULL,
  display_dcc_identd,
  NULL,
  NULL,
  NULL
};

static void
dcc_identd_connect(int idx, char *buf, int atr)
{
  in_addr_t ip;
  in_port_t port;
  int j, sock;
  char s[UHOSTLEN + 1] = "";

  if (dcc_total + 1 > max_dcc) {
    j = answer(dcc[idx].sock, s, &ip, &port, 0);
    if (j != -1)
      killsock(j);
    return;
  }
  sock = answer(dcc[idx].sock, s, &ip, &port, 0);

  while ((sock == -1) && (errno == EAGAIN))
    sock = answer(sock, s, &ip, &port, 0);

  if (sock < 0) {
    putlog(LOG_MISC, "*", "Failed TELNET incoming (%s)", strerror(errno));
    return;
  }
  /* changeover_dcc(idx, &DCC_IDENTD, 0); */

  j = new_dcc(&DCC_IDENTD, 0);

  dcc[j].sock = sock;
  dcc[j].port = port;
  dcc[j].addr = dcc[idx].addr;
  strlcpy(dcc[j].host, dcc[idx].host, sizeof(dcc[j].host));
  strlcpy(dcc[j].nick, "*", sizeof(dcc[j].nick));
  /* dcc[j].uint.ident_sock = dcc[idx].sock; */
  dcc[j].timeval = now;
}

struct dcc_table DCC_IDENTD_CONNECT = {
  "IDENTD",
  DCT_LISTEN,
  eof_dcc_identd,
  dcc_identd_connect,
  NULL,
  NULL,
  display_dcc_identd,
  NULL,
  NULL,
  NULL
};

char s1_13[3] = "",s1_15[3] = "",s1_3[3] = "";

static void
dcc_chat_secpass(int idx, char *buf, int atr)
{
  int badauth = 0;

  if (!atr)
    return;

  strip_telnet(dcc[idx].sock, buf, &atr);
  atr = dcc[idx].user ? dcc[idx].user->flags : 0;

  if (dccauth) {
    char check[MD5_HASH_LENGTH + 7] = "";

    simple_snprintf(check, sizeof check, STR("+Auth %s"), dcc[idx].hash);
    badauth = strcmp(check, buf);
    /* +secpass */
  }

  /* Correct pass or secpass! */
  if (!dcc[idx].wrong_pass && (!dccauth || (dccauth && !badauth))) {
    putlog(LOG_MISC, "*", "Logged in: %s (%s/%d)", dcc[idx].nick, dcc[idx].host, dcc[idx].port);
    if (dcc[idx].u.chat->away) {
      free(dcc[idx].u.chat->away);
      dcc[idx].u.chat->away = NULL;
    }
    dcc[idx].type = &DCC_CHAT;
    dcc[idx].status &= ~STAT_CHAT;
    dcc[idx].u.chat->channel = -2;
    /* Turn echo back on for telnet sessions (send IAC WON'T ECHO). */
    if (dcc[idx].status & STAT_TELNET)
      dprintf(idx, TLN_IAC_C TLN_WONT_C TLN_ECHO_C "\n");
    stats_add(dcc[idx].user, 1, 0);
    bool writeuserfile = 0;
    if (!get_user(&USERENTRY_SECPASS, dcc[idx].user)) {	/* this should check how many logins instead */
      char pass[MAXPASSLEN + 1] = "";

      writeuserfile = 1;

      dprintf(idx, "********************************************************************\n \n \n");
      dprintf(idx, "%sWARNING: YOU DO NOT HAVE A SECPASS SET, NOW SETTING A RANDOM ONE....%s\n", FLASH(-1), FLASH_END(-1));
      make_rand_str(pass, MAXPASSLEN);
      set_user(&USERENTRY_SECPASS, dcc[idx].user, pass);
      dprintf(idx, "Your secpass is now: %s%s%s\n", pass, BOLD(-1), BOLD_END(-1));
      dprintf(idx, "Make sure you do not lose this, as it may be needed in the future.\n \n");
      dprintf(idx, "********************************************************************\n");
    }
    if (!get_user(&USERENTRY_CONSOLE, dcc[idx].user)) { /* No console, force them to have at least +mcobseuw */
      struct flag_record fr = {FR_GLOBAL, 0, 0, 0 };

      get_user_flagrec(dcc[idx].user, &fr, NULL);

      strlcpy(dcc[idx].u.chat->con_chan, "*", 2);
      dcc[idx].u.chat->con_flags = LOG_MSGS|LOG_SERV;
      if (glob_owner(fr))
        dcc[idx].u.chat->con_flags |= LOG_ERRORS|LOG_WARN;
      if (glob_master(fr))
        dcc[idx].u.chat->con_flags |= LOG_BOTS|LOG_CMDS|LOG_MISC|LOG_WALL;

      /* Avoid rewriting it later */
      writeuserfile = 0;
      console_dostore(idx);
    }
    if (writeuserfile && conf.bot->hub)
      write_userfile(idx);
    dcc_chatter(idx);
  } else if ((dccauth && badauth) || dcc[idx].wrong_pass) { 		/* bad auth */
    dprintf(idx, "%s\n", response(RES_BADUSERPASS));
    putlog(LOG_MISC, "*", "Bad Auth: [%s]%s/%d", dcc[idx].nick, dcc[idx].host, dcc[idx].port);
    if (dcc[idx].u.chat->away) {        /* su from a dumb user */
      /* Turn echo back on for telnet sessions (send IAC WON'T ECHO). */
      if (dcc[idx].status & STAT_TELNET)
        dprintf(idx, TLN_IAC_C TLN_WONT_C TLN_ECHO_C "\n");
      dcc[idx].user = get_user_by_handle(userlist, dcc[idx].u.chat->away);
      strlcpy(dcc[idx].nick, dcc[idx].u.chat->away, sizeof(dcc[idx].nick));
      free(dcc[idx].u.chat->away);
      free(dcc[idx].u.chat->su_nick);
      dcc[idx].u.chat->away = NULL;
      dcc[idx].u.chat->su_nick = NULL;
      dcc[idx].type = &DCC_CHAT;

      dcc[idx].u.chat->channel = dcc[idx].u.chat->su_channel;
      if (dcc[idx].u.chat->channel < GLOBAL_CHANS)
        botnet_send_join_idx(idx);
      chanout_but(-1, dcc[idx].u.chat->channel, "*** %s has joined the party line.\n", dcc[idx].nick);
    } else {
      killsock(dcc[idx].sock);
      lostdcc(idx);
    }
  }
}

static void
eof_dcc_general(int idx)
{
  putlog(LOG_MISC, "*", "Lost dcc connection to %s (%s/%d)", dcc[idx].nick, dcc[idx].host, dcc[idx].port);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
tout_dcc_chat_secpass(int idx)
{
  putlog(LOG_MISC, "*", "Auth timeout on dcc chat: [%s]%s", dcc[idx].nick, dcc[idx].host);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
display_dcc_chat_secpass(int idx, char *buf, size_t bufsiz)
{
  simple_snprintf(buf, bufsiz, "secpass  waited %ds", (int) (now - dcc[idx].timeval));
}

static void
tout_dcc_chat_pass(int idx)
{
  putlog(LOG_MISC, "*", "Password timeout on dcc chat: [%s]%s", dcc[idx].nick, dcc[idx].host);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
display_dcc_chat_pass(int idx, char *buf, size_t bufsiz)
{
  simple_snprintf(buf, bufsiz, "pass  waited %ds", (int) (now - dcc[idx].timeval));
}

static void
kill_dcc_general(int idx, void *x)
{
  struct chat_info *p = (struct chat_info *) x;

  if (p) {
    if (p->buffer) {
      struct msgq *r = NULL, *q = NULL;

      for (r = dcc[idx].u.chat->buffer; r; r = q) {
        q = r->next;
        free(r->msg);
        free(r);
      }
    }
    if (p->away) {
      free(p->away);
    }
    free(p);
  }
}

/* Remove the color control codes that mIRC,pIRCh etc use to make
 * their client seem so fecking cool! (Sorry, Khaled, you are a nice
 * guy, but when you added this feature you forced people to either
 * use your *SHAREWARE* client or face screenfulls of crap!)
 */
static void
strip_mirc_codes(int flags, char *text)
{
  char *dd = text;

  while (*text) {
    switch (*text) {
      case 2:                  /* Bold text */
        if (flags & STRIP_BOLD) {
          text++;
          continue;
        }
        break;
      case 3:                  /* mIRC colors? */
        if (flags & STRIP_COLOR) {
          if (egg_isdigit(text[1])) {   /* Is the first char a number? */
            text += 2;          /* Skip over the ^C and the first digit */
            if (egg_isdigit(*text))
              text++;           /* Is this a double digit number? */
            if (*text == ',') { /* Do we have a background color next? */
              if (egg_isdigit(text[1]))
                text += 2;      /* Skip over the first background digit */
              if (egg_isdigit(*text))
                text++;         /* Is it a double digit? */
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
      case 0x16:               /* Reverse video */
        if (flags & STRIP_REV) {
          text++;
          continue;
        }
        break;
      case 0x1f:               /* Underlined text */
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
            while ((*text == ';') || egg_isdigit(*text))
              text++;
            if (*text)
              text++;           /* also kill the following char */
          }
          continue;
        }
        break;
    }
    *dd++ = *text++;            /* Move on to the next char */
  }
  *dd = 0;
}

static void
append_line(int idx, char *line)
{
  struct msgq *p = NULL, *q = NULL;
  struct chat_info *c = dcc[idx].u.chat;

  if (c->current_lines > 1000) {
    /* They're probably trying to fill up the bot nuke the sods :) */
    for (p = c->buffer; p; p = q) {
      q = p->next;
      free(p->msg);
      free(p);
    }
    c->buffer = NULL;
    c->current_lines = 0;
    dcc[idx].status &= ~STAT_PAGE;
    do_boot(idx, conf.bot->nick, "too many pages - senq full");
    return;
  }

  size_t l = strlen(line);

  if ((c->line_count < c->max_line) && (c->buffer == NULL)) {
    ++c->line_count;
    tputs(dcc[idx].sock, line, l);
  } else {
    ++c->current_lines;

    p = (struct msgq *) calloc(1, sizeof(struct msgq));

    p->len = l;
    p->msg = (char *) calloc(1, l + 1);
    p->next = NULL;
    strlcpy(p->msg, line, l + 1);

    if (c->buffer == NULL)
      c->buffer = p;
    else {
      for (q = c->buffer; q->next; q = q->next) ;
      q->next = p;
    }
  }
}

static void
out_dcc_general(int idx, char *buf, void *x)
{
  char *y = buf;

  if (dcc[idx].type == &DCC_CHAT) {
    struct chat_info *p = (struct chat_info *) x;

    strip_mirc_codes(p->strip_flags, buf);
  }
  if (dcc[idx].status & STAT_TELNET)
    y = add_cr(buf);
  if (!dcc[idx].bot && dcc[idx].status & STAT_PAGE)
    append_line(idx, y);
  else
    tputs(dcc[idx].sock, y, strlen(y));
}

static struct dcc_table DCC_CHAT_SECPASS = {
  "CHAT_SECPASS",
  0,
  eof_dcc_general,
  dcc_chat_secpass,
  &auth_timeout,
  tout_dcc_chat_secpass,
  display_dcc_chat_secpass,
  kill_dcc_general,
  out_dcc_general,
  NULL
};

//su drops us here
static void
dcc_chat_pass(int idx, char *buf, int atr)
{
  if (!atr)
    return;

  char *pass = NULL;

  strip_telnet(dcc[idx].sock, buf, &atr);
  atr = dcc[idx].user ? dcc[idx].user->flags : 0;

  pass = newsplit(&buf);

  if (dcc[idx].encrypt == 1) {
    if (!strcasecmp(pass, STR("neg!"))) {		/* we're the hub */
      link_parse(idx, buf);
    } else if (!strcasecmp(pass, STR("neg."))) {		/* we're done, link up! */
      int snum = findanysnum(dcc[idx].sock);

      if (socklist[snum].enclink == -1) {
	putlog(LOG_WARN, "*", STR("%s attempted to negotiate an encryption out of order."), dcc[idx].nick);
	killsock(dcc[idx].sock);
	lostdcc(idx);
	return;
      }

      dcc[idx].encrypt = 2;
      if (dcc[idx].bot) {
        dcc[idx].type = &DCC_BOT_NEW;
        dcc[idx].u.bot = (struct bot_info *) calloc(1, sizeof(struct bot_info));
        if (dcc[idx].status & STAT_UNIXDOMAIN)
          dcc[idx].status = STAT_UNIXDOMAIN|STAT_CALLED;
        else
          dcc[idx].status = STAT_CALLED;
        dprintf(idx, "goodbye!\n");
        greet_new_bot(idx);
        if (conf.bot->hub || conf.bot->localhub)
          send_timesync(idx);
      } else {
        // User encrypted over relay
        /* Turn off remote telnet echo (send IAC WILL ECHO). */
        dprintf(idx, "\n%s" TLN_IAC_C TLN_WILL_C TLN_ECHO_C "\n", "Enter your password");
      }
    } else if (!strcasecmp(pass, STR("neg"))) {
      //link_challenge_from()
      int snum = findanysnum(dcc[idx].sock);

      if (snum >= 0) {
        char *hash = newsplit(&buf);

        int hash_n = strcmp(dcc[idx].shahash, hash);
        OPENSSL_cleanse(dcc[idx].shahash, sizeof(dcc[idx].shahash));
        OPENSSL_cleanse(hash, strlen(hash));
        if (hash_n) {
          putlog(LOG_WARN, "*", STR("%s attempted to negotiate an encryption with an invalid hash."), dcc[idx].nick);
          killsock(dcc[idx].sock);
          lostdcc(idx);
          return;
        }
        int type = atoi(newsplit(&buf)), i = -1;

        /* verify we have that type and then initiate it */
        if ((i = link_find_by_type(type)) == -1) {
          putlog(LOG_WARN, "*", STR("%s attempted to link with an invalid encryption. (%d)"), dcc[idx].nick, type);
          if (type == 0 && !link_cleartext) {
            putlog(LOG_WARN, "*", "This is likely due to %s needing to be upgraded. Enable 'link_cleartext' to allow linking.", dcc[idx].nick);
            putlog(LOG_WARN, "*", "Be sure to disable 'link_cleartext' after all bots are upgraded.");
          }
          killsock(dcc[idx].sock);
          lostdcc(idx);
          return;
        }

        sdprintf(STR("Using '%s' (%d/%d) for link with %s"), enclink[i].name, enclink[i].type, i, dcc[idx].nick);

        if (buf[0]) {
          const char *expected_nick = newsplit(&buf);

          if (strcasecmp(expected_nick, conf.bot->nick)) {
            putlog(LOG_WARN, "*", STR("%s failed encrypted link handshake (was expecting '%s' instead of me)"), dcc[idx].nick, expected_nick);
            killsock(dcc[idx].sock);
            lostdcc(idx);
            return;
          }
        }

        socklist[snum].enclink = i;

        link_link(idx, -1, i, FROM);
      }
    } else {
      /* Invalid password/digest on hub */
      putlog(LOG_WARN, "*", STR("%s failed encrypted link handshake."), dcc[idx].nick);
      killsock(dcc[idx].sock);
      lostdcc(idx);
    }

    return;
  }
  /* else !bot */
  int passok = u_pass_match(dcc[idx].user, pass);
  bool do_obscure = (!passok && auth_obscure) ? 1 : 0;

  if (passok || do_obscure) {
    if (do_obscure)
      dcc[idx].wrong_pass = 1;

    if (dccauth || do_obscure) { 
      char randstr[51] = "";

      make_rand_str(randstr, 50);
      makehash(dcc[idx].user, randstr, dcc[idx].hash, MD5_HASH_LENGTH + 1);

      dcc[idx].type = &DCC_CHAT_SECPASS;
      dcc[idx].timeval = now;
      dprintf(-dcc[idx].sock, STR("-Auth %s %s\n"), randstr, conf.bot->nick);
    } else {
      dcc_chat_secpass(idx, pass, atr);
    }
  } else {
    dprintf(idx, "%s\n", response(RES_BADUSERPASS));
    putlog(LOG_MISC, "*", "Bad Password: [%s]%s/%d", dcc[idx].nick, dcc[idx].host, dcc[idx].port);
    if (dcc[idx].u.chat->away) {        /* su from a dumb user */
      /* Turn echo back on for telnet sessions (send IAC WON'T ECHO). */
      if (dcc[idx].status & STAT_TELNET)
        dprintf(idx, TLN_IAC_C TLN_WONT_C TLN_ECHO_C "\n");
      dcc[idx].user = get_user_by_handle(userlist, dcc[idx].u.chat->away);
      strlcpy(dcc[idx].nick, dcc[idx].u.chat->away, sizeof(dcc[idx].nick));
      free(dcc[idx].u.chat->away);
      free(dcc[idx].u.chat->su_nick);
      dcc[idx].u.chat->away = NULL;
      dcc[idx].u.chat->su_nick = NULL;
      dcc[idx].type = &DCC_CHAT;

      dcc[idx].u.chat->channel = dcc[idx].u.chat->su_channel;
      if (dcc[idx].u.chat->channel < GLOBAL_CHANS)
        botnet_send_join_idx(idx);
      chanout_but(-1, dcc[idx].u.chat->channel, "*** %s has joined the party line.\n", dcc[idx].nick);
    } else {
      killsock(dcc[idx].sock);
      lostdcc(idx);
    }
  }
}

struct dcc_table DCC_CHAT_PASS = {
  "CHAT_PASS",
  0,
  eof_dcc_general,
  dcc_chat_pass,
  &password_timeout,
  tout_dcc_chat_pass,
  display_dcc_chat_pass,
  kill_dcc_general,
  out_dcc_general,
  NULL
};


/* Make sure ansi code is just for color-changing
 */
static int
__attribute__((pure))
check_ansi(const char *v)
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

int ansi_len(const char *s)
{
  const char *c = s;
  int count = 0;

  while (*c) {
    if (*c == 27) {
      c++;
      count++;

      if (*c == '[') {
        c++;
        count++;
        while ((*c != 'm')) {
          c++;
          count++;
        }
        c++;
        count++;
      }
    } else
      c++;
  }

  return count;
}

static void
eof_dcc_chat(int idx)
{
  putlog(LOG_MISC, "*", "Lost dcc connection to %s (%s/%d)", dcc[idx].nick, dcc[idx].host, dcc[idx].port);
  if (dcc[idx].u.chat->channel >= 0) {
    chanout_but(idx, dcc[idx].u.chat->channel, "*** %s lost dcc link.\n", dcc[idx].nick);
    if (dcc[idx].u.chat->channel < GLOBAL_CHANS)
      botnet_send_part_idx(idx, "lost dcc link");
  }
  check_bind_chof(dcc[idx].nick, idx);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
dcc_chat(int idx, char *buf, int len)
{
  int i = 0;

  strip_telnet(dcc[idx].sock, buf, &len);
  if (buf[0] && (buf[0] != settings.dcc_prefix[0]) && !(dcc[idx].user && (dcc[idx].user->flags & USER_NOFLOOD)) &&
      detect_dcc_flood(&dcc[idx].timeval, dcc[idx].u.chat, idx))
    return;

  dcc[idx].timeval = now;
  if (buf[0]) {
    int nathan = 0, doron = 0, fixed = 0;
    char *v = buf, *d = buf;

    /* Check for beeps and cancel annoying ones */
    while (*v)
      switch (*v) {
        case 1:			/* CTCP ?! */
          v++;
          break;
        case 7:                /* Beep - no more than 3 */
          nathan++;
          if (nathan > 3)
            v++;
          else
            *d++ = *v++;
          break;
        case 8:                /* Backspace - for lame telnet's :) */
          if (d > buf) {
            d--;
          }
          v++;
          break;
        case 27:               /* ESC - ansi code? */
          doron = check_ansi(v);
          /* If it's valid, append a return-to-normal code at the end */
          if (!doron) {
            *d++ = *v++;
            fixed = 1;
          } else
            v += doron;
          break;
        case '\r':             /* Weird pseudo-linefeed */
          v++;
          break;
        default:
          *d++ = *v++;
      }
    if (fixed)
      strcpy(d, "\033[0m");
    else
      *d = 0;

    if (unlikely(u_pass_match(dcc[idx].user, buf))) {     /* user said their password :) */
      dprintf(idx, "Sure you want that going to the partyline? ;) (msg to partyline halted.)\n");
    } else if (unlikely(!strncmp(buf, STR("+Auth "), 6))) {    /* ignore extra +Auth lines */
    } else if ((!strncmp(buf, settings.dcc_prefix, strlen(settings.dcc_prefix))) || (dcc[idx].u.chat->channel < 0)) {
      if (!strncmp(buf, settings.dcc_prefix, strlen(settings.dcc_prefix)) && (dcc[idx].u.chat->channel >= 0))        /* strip '.' out */
        buf++;
      v = newsplit(&buf);
      rmspace(buf);
      check_bind_dcc(v, idx, buf);
    } else if (unlikely(buf[0] == ',')) {
      int me = 0;

      if ((buf[1] == 'm') && (buf[2] == 'e') && buf[3] == ' ')
        me = 1;
      for (i = 0; i < dcc_total; i++) {
       if (dcc[i].type) {
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
              dprintf(i, "-> %s%s\n", dcc[idx].nick, buf + 3);
            else
              dprintf(i, "-%s-> %s\n", dcc[idx].nick, buf + 1);
          }
        }
       }
      }
    } else if (unlikely(buf[0] == '\'')) {
      int me = 0;

      if ((buf[1] == 'm') && (buf[2] == 'e') && ((buf[3] == ' ') || (buf[3] == '\'') || (buf[3] == ',')))
        me = 1;
      for (i = 0; i < dcc_total; i++) {
        if (dcc[i].type && dcc[i].type->flags & DCT_CHAT) {
          if (me)
            dprintf(i, "=> %s%s\n", dcc[idx].nick, buf + 3);
          else
            dprintf(i, "=%s=> %s\n", dcc[idx].nick, buf + 1);
        }
      }
    } else { /* partyline chat */
      if (dcc[idx].u.chat->away != NULL)
        not_away(idx);
      /* Check for CTCP (/me) */
      if (!strncmp(buf, "CTCP_MESSAGE ", 13))		/* irssi */
        buf += 13;
      if (!strncmp(buf, "ACTION ", 7)) {
        buf += 7;
        check_bind_dcc("me", idx, buf);
      } else {		/* regular text */
        if (dcc[idx].status & STAT_ECHO)
          chanout_but(-1, dcc[idx].u.chat->channel, "<%s> %s\n", dcc[idx].nick, buf);
        else
          chanout_but(idx, dcc[idx].u.chat->channel, "<%s> %s\n", dcc[idx].nick, buf);
        botnet_send_chan(-1, conf.bot->nick, dcc[idx].nick, dcc[idx].u.chat->channel, buf);
      }
    }
  }
  if (dcc[idx].type == &DCC_CHAT)       /* Could have change to files */
    if (dcc[idx].status & STAT_PAGE)
      flush_lines(idx, dcc[idx].u.chat);
}

static void
display_dcc_chat(int idx, char *buf, size_t bufsiz)
{
  size_t i = simple_snprintf(buf, bufsiz, "chat  flags: ");
  int colori = 0;

  buf[i++] = dcc[idx].status & STAT_CHAT ? 'C' : 'c';
  buf[i++] = dcc[idx].status & STAT_PARTY ? 'P' : 'p';
  buf[i++] = dcc[idx].status & STAT_TELNET ? 'T' : 't';
  buf[i++] = dcc[idx].status & STAT_ECHO ? 'E' : 'e';
  buf[i++] = dcc[idx].status & STAT_PAGE ? 'P' : 'p';
  if ((colori = coloridx(idx)))
    buf[i++] = colori == 1 ? 'A' : 'M';
#ifdef USE_IPV6
  if (sockprotocol(dcc[idx].sock) == AF_INET6 && dcc[idx].host6[0])
    buf[i++] = '6';
#endif /* USE_IPV6 */
  simple_snprintf(buf + i, bufsiz - i, "/%d", dcc[idx].u.chat->channel);
}

struct dcc_table DCC_CHAT = {
  "CHAT",
  DCT_CHAT | DCT_MASTER | DCT_SHOWWHO | DCT_VALIDIDX | DCT_SIMUL | DCT_CANBOOT | DCT_REMOTEWHO,
  eof_dcc_chat,
  dcc_chat,
  NULL,
  NULL,
  display_dcc_chat,
  kill_dcc_general,
  out_dcc_general,
  NULL
};

static int lasttelnets;
static char lasttelnethost[81];
static time_t lasttelnettime;

/* A modified detect_flood for incoming telnet flood protection.
 */
static bool
detect_telnet_flood(char *floodhost)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0 };

  get_user_flagrec(get_user_by_host(floodhost), &fr, NULL);
  if (!flood_telnet_thr || glob_noflood(fr))
    return 0;                   /* No flood protection */
  if (strcasecmp(lasttelnethost, floodhost)) {      /* New... */
    strlcpy(lasttelnethost, floodhost, sizeof(lasttelnethost));
    lasttelnettime = now;
    lasttelnets = 0;
    return 0;
  }
  if (lasttelnettime < now - flood_telnet_time) {
    /* Flood timer expired, reset it */
    lasttelnettime = now;
    lasttelnets = 0;
    return 0;
  }
  lasttelnets++;
  if (lasttelnets >= flood_telnet_thr) {        /* FLOOD! */
    /* Reset counters */
    lasttelnets = 0;
    lasttelnettime = 0;
    lasttelnethost[0] = 0;
    putlog(LOG_MISC, "*", "Telnet connection flood from %s!  Placing on ignore!", floodhost);
    addignore(floodhost, conf.bot->nick, "Telnet connection flood", now + (60 * ignore_time));
    return 1;
  }
  return 0;
}

static void dcc_telnet_dns_callback(int, void *, const char *, bd::Array<bd::String>);
static void dcc_telnet_dns_forward_callback(int, void *, const char *, bd::Array<bd::String>);

static void
dcc_telnet(int idx, char *buf, int ii)
{
  in_addr_t ip;
  in_port_t port;
  char s[UHOSTLEN + 1] = "";
  int i;
  char x[1024] = "";

  if (unlikely(dcc_total + 1 > max_dcc)) {
    int j;

    j = answer(dcc[idx].sock, s, &ip, &port, 0);
    if (j != -1) {
      dprintf(-j, "Sorry, too many connections already.\r\n");
      killsock(j);
    }
    return;
  }

  int sock = answer(dcc[idx].sock, s, &ip, &port, 0);

  while ((sock == -1) && (errno == EAGAIN))
    sock = answer(dcc[idx].sock, s, &ip, &port, 0);
  if (unlikely(sock < 0)) {
    putlog(LOG_MISC, "*", "Failed TELNET incoming (%s)", strerror(errno));
//    killsock(dcc[idx].sock);
    return;
  }
  /* Buffer data received on this socket.  */
  sockoptions(sock, EGG_OPTION_SET, SOCK_BUFFER);

  int af_type = sockprotocol(sock);

  if (af_type == AF_UNIX) {

    //i = new_dcc(&DCC_IDENT, 0);
    i = new_dcc(&DCC_TELNET_ID, 0);

    simple_snprintf(x, sizeof(x), "UNKNOWN@localhost");
    dcc[i].sock = sock;
    dcc[i].uint.ident_sock = dcc[idx].sock;
    dcc[i].port = 0;
    dcc[i].timeval = now;
    strlcpy(dcc[i].nick, "*", sizeof(dcc[i].nick));
    putlog(LOG_BOTS, "*", "Connection over local socket: %s", s);
    dcc_telnet_got_ident(i, x);
    return;
  }

  if (port < 1024 || port > 65534) {
    putlog(LOG_BOTS, "*", "Refused %s/%d (bad src port)", s, port);
    killsock(sock);
    return;
  }

  putlog(LOG_DEBUG, "*", "Telnet connection: %s/%d", s, port);

  // Are they ignored by IP?
  simple_snprintf(x, sizeof(x), "-telnet!telnet@%s", iptostr(htonl(ip)));

  if (unlikely(match_ignore(x) || detect_telnet_flood(x))) {
    putlog(LOG_DEBUG, "*", "Ignored telnet connection from: %s", x);
    killsock(sock);
    return;
  }
  
  /* If a matching bot ip is found, it might still be on the ignore list as a host,
   * so we'll just reverse the ip anyway and check the ignores before
   * proceeding with user matching 
   */

  i = new_dcc(&DCC_DNSWAIT, sizeof(struct dns_info));

  dcc[i].addr = ip;
  dcc[i].sock = sock;
  dcc[i].user = get_user_by_host(x);		/* check for matching -telnet!telnet@ip */
  strlcpy(dcc[i].host, s, sizeof(dcc[i].host));
#ifdef USE_IPV6
  if (af_type == AF_INET6)
    strlcpy(dcc[i].host6, s, sizeof(dcc[i].host6));
#endif /* USE_IPV6 */
  dcc[i].port = port;
  dcc[i].timeval = now;
  strlcpy(dcc[i].nick, "*", sizeof(dcc[i].nick));

  dcc[i].u.dns->ibuf = idx;

  int dns_id = egg_dns_reverse(s, 20, dcc_telnet_dns_callback, (void *) (long) i);
  if (dns_id >= 0)
    dcc[i].dns_id = dns_id;
}

static void dcc_telnet_dns_callback(int id, void *client_data, const char *ip, bd::Array<bd::String> hosts)
{
  // 64bit hacks
  long data = (long) client_data;
  int i = (int) data;

  Context;

  if (!valid_dns_id(i, id))
    return;

  int idx = dcc[i].u.dns->ibuf;

  if (!valid_idx(idx)) {
    putlog(LOG_BOTS, "*", "Lost listening socket while resolving %s", dcc[i].host);
    killsock(dcc[i].sock);
    lostdcc(i);
    return;
  }

  //Reset timer
  dcc[i].timeval = now;

  //Clear the ip (still saved in dcc[i].addr)
  dcc[i].host[0] = 0;
  if (hosts.size()) {
    strlcpy(dcc[i].host, bd::String(hosts[0]).c_str(), sizeof(dcc[i].host));

    //Check forward; only check the protocol of which this connection is.
    //If they connected on V4, lookup A, if V6, lookup AAAA
    int dns_type = DNS_LOOKUP_A;
    if (is_dotted_ip(iptostr(htonl(dcc[i].addr))) == AF_INET6)//Is this even valid?
      dns_type = DNS_LOOKUP_AAAA;
    int dns_id = egg_dns_lookup(bd::String(hosts[0]).c_str(), 20, dcc_telnet_dns_forward_callback, (void *) (long) i, dns_type);
    if (dns_id >= 0)
      dcc[i].dns_id = dns_id;
  } else {
    bd::Array<bd::String> empty;
    dcc_telnet_dns_forward_callback(id, client_data, ip, empty);
  }
}

static void dcc_telnet_dns_forward_callback(int id, void *client_data, const char *host, bd::Array<bd::String> ips) {
  // 64bit hacks
  long data = (long) client_data;
  int i = (int) data;

  Context;

  if (!valid_dns_id(i, id))
    return;

  int j = -1, sock, idx = -1;
  char s2[UHOSTLEN + 20] = "";

  if (valid_idx(i))
    idx = dcc[i].u.dns->ibuf;

  if (!valid_idx(idx)) {
    putlog(LOG_BOTS, "*", "Lost listening socket while resolving %s", iptostr(htonl(dcc[i].addr)));
    killsock(dcc[i].sock);
    lostdcc(i);
    return;
  }

  bool forward_matched = false;
  bd::String look_for_ip(iptostr(htonl(dcc[i].addr)));
  // Look for any match
  for (size_t n = 0; n < ips.size(); ++n) {
    if (ips[n] == look_for_ip) {
      forward_matched = true;
      break;
    }
  }

  // If the forward did not match, replace the saved host with the ip
  if (!forward_matched)
    strlcpy(dcc[i].host, iptostr(htonl(dcc[i].addr)), sizeof(dcc[i].host));

  if (forward_matched) {
    // Are they ignored by host?
    simple_snprintf(s2, sizeof(s2), "-telnet!telnet@%s", dcc[i].host);

    if (match_ignore(s2)) {
      putlog(LOG_DEBUG, "*", "Ignored telnet connection from: %s", s2);
      killsock(dcc[i].sock);
      lostdcc(i);
      return;
    }
  }

  if (dcc[idx].host[0] == '@') {
    /* Restrict by hostname */
    if (!wild_match(dcc[idx].host + 1, dcc[i].host)) {
      putlog(LOG_BOTS, "*", "Refused %s (bad hostname)", dcc[i].host);
      killsock(dcc[i].sock);
      lostdcc(i);
      return;
    }
  }

  changeover_dcc(i, &DCC_IDENTWAIT, 0);
  dcc[i].timeval = now;
  dcc[i].uint.ident_sock = dcc[idx].sock;

  if (!dcc[i].user)
    dcc[i].user = get_user_by_host(s2);		/* check for matching -telnet!telnet@host */
  
  if (forward_matched)
    putlog(LOG_MISC, "*", "Telnet connection: %s[%s]/%d", dcc[i].host, iptostr(htonl(dcc[i].addr)), dcc[i].port);
  else
    putlog(LOG_MISC, "*", "Telnet connection: %s/%d", iptostr(htonl(dcc[i].addr)), dcc[i].port);

  sock = open_telnet((char *) iptostr(htonl(dcc[i].addr)), 113, 0);

  char s[UHOSTLEN] = "";

  if (sock < 0) {
    if (sock == -2)
      strlcpy(s, "DNS lookup failed for ident", sizeof(s));
    else
      strlcpy(s, strerror(errno), sizeof(s));
  } else {
    j = new_dcc(&DCC_IDENT, 0);
    if (j < 0) {
      killsock(sock);
      strlcpy(s, "No Free DCC's", sizeof(s));
    }
  }
  if (s[0]) {
    putlog(LOG_MISC, "*", "Ident failed for %s: %s", dcc[i].host, s);
    simple_snprintf(s, sizeof(s), "telnet@%s", dcc[i].host);
    dcc_telnet_got_ident(i, s);
    return;
  }
  dcc[j].sock = sock;
  dcc[j].port = 113;
  dcc[j].addr = dcc[i].addr;
  strlcpy(dcc[j].host, dcc[i].host, sizeof(dcc[j].host));
  strlcpy(dcc[j].nick, "*", sizeof(dcc[j].nick));
  dcc[j].uint.ident_sock = dcc[i].sock;
  dcc[j].user = dcc[i].user;
  dcc[j].timeval = now;
  dprintf(j, "%d, %d\n", dcc[i].port, dcc[idx].port);
}

static void
eof_dcc_telnet(int idx)
{
  putlog(LOG_MISC, "*", "(!) Listening port %d abruptly died.", dcc[idx].port);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
display_telnet(int idx, char *buf, size_t bufsiz)
{
  simple_snprintf(buf, bufsiz, "lstn  %d%s", dcc[idx].port, (dcc[idx].status & LSTN_PUBLIC) ? " pub" : "");
}

struct dcc_table DCC_TELNET = {
  "TELNET",
  DCT_LISTEN,
  eof_dcc_telnet,
  dcc_telnet,
  NULL,
  NULL,
  display_telnet,
  NULL,
  NULL,
  NULL
};

static void
eof_dcc_dupwait(int idx)
{
  putlog(LOG_BOTS, "*", "Lost telnet connection from %s while checking for duplicate", dcc[idx].host);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
dcc_dupwait(int idx, char *buf, int i)
{
  /* We just ignore any data at this point. */
  return;
}

/* We now check again. If the bot is still marked as duplicate, there is no
 * botnet lag we could push it on, so we just drop the connection.
 */
static void
timeout_dupwait(int idx)
{
  /* Still duplicate? */
  if (in_chain(dcc[idx].nick)) {
    char x[UHOSTLEN] = "";

    simple_snprintf(x, sizeof x, "%s!%s", dcc[idx].nick, dcc[idx].host);
    putlog(LOG_BOTS, "*", "Refused telnet connection from %s (duplicate)", x);
    killsock(dcc[idx].sock);
    lostdcc(idx);
  } else {
    /* Ha! Now it's gone and we can grant this bot access. */
    dcc_telnet_pass(idx, dcc[idx].u.dupwait->atr);
  }
}

static void
display_dupwait(int idx, char *buf, size_t bufsiz)
{
  simple_snprintf(buf, bufsiz, "wait  duplicate?");
}

static void
kill_dupwait(int idx, void *x)
{
  struct dupwait_info *p = (struct dupwait_info *) x;

  if (p)
    free(p);
}

struct dcc_table DCC_DUPWAIT = {
  "DUPWAIT",
  DCT_VALIDIDX,
  eof_dcc_dupwait,
  dcc_dupwait,
  &dupwait_timeout,
  timeout_dupwait,
  display_dupwait,
  kill_dupwait,
  NULL,
  NULL
};

/* This function is called if a bot gets removed from the list. It checks
 * wether we have a pending duplicate connection for that bot and continues
 * with the login in that case.
 */
void
dupwait_notify(const char *who)
{
  for (int idx = 0; idx < dcc_total; idx++)
    if (dcc[idx].type && (dcc[idx].type == &DCC_DUPWAIT) && !strcasecmp(dcc[idx].nick, who)) {
      dcc_telnet_pass(idx, dcc[idx].u.dupwait->atr);
      break;
    }
}

static void
dcc_telnet_id(int idx, char *buf, int atr)
{
  char *nick = buf;

  strip_telnet(dcc[idx].sock, nick, &atr);

  if (nick[0] == '-') {
    nick++;
    dcc[idx].bot = 1;
    dcc[idx].encrypt = 1;
  } else if (nick[0] == '+') {
    nick++;
    dcc[idx].encrypt = 1;
  }

  nick[HANDLEN] = 0; // Trim to handle length before looking up user.

  dcc[idx].user = get_user_by_handle(userlist, nick);

  bool ok = 0;

  if (dcc[idx].user) {
    if (dcc[idx].bot != dcc[idx].user->bot) {
      putlog(LOG_WARN, "*", "Refused %s (fake bot login for '%s')", dcc[idx].host, nick);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
    }

    struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0 };

    get_user_flagrec(dcc[idx].user, &fr, NULL);

    ok = 1;
    if (conf.bot->hub && !glob_huba(fr))
      ok = 0;
    
    if (!conf.bot->hub) {
      /* if I am a chanhub and they dont have +c then drop */
      if (ischanhub() && !glob_chuba(fr))
        ok = 0;
      if (!ischanhub())
        ok = 0;
    }
    if (!ok && glob_bot(fr))
      ok = 1;
  }

  if (!ok) {
    if (dcc[idx].user)
      putlog(LOG_BOTS, "*", "%s: %s!%s", ischanhub() ? "Refused DCC chat (no access)" : "Refused DCC chat (I'm not a chathub (+c))", nick, dcc[idx].host);
    else if (dcc[idx].bot)
      putlog(LOG_BOTS, "*", "Refused %s (invalid bot handle: %s) (Add with '%snewleaf %s -telnet!%s')", dcc[idx].host, nick, settings.dcc_prefix, nick, dcc[idx].host);
    else
      putlog(LOG_BOTS, "*", "Refused %s (invalid handle: %s)", dcc[idx].host, nick);
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }
  correct_handle(nick);
  strlcpy(dcc[idx].nick, nick, sizeof(dcc[idx].nick));
  if (!strcmp(dcc[idx].host, "UNKNOWN@localhost"))
    simple_snprintf(dcc[idx].host, sizeof(dcc[idx].host), "%s@localhost", nick);
  if (dcc[idx].user->bot) {
    if (!strcasecmp(conf.bot->nick, dcc[idx].nick)) {
      putlog(LOG_BOTS, "*", "Refused telnet connection from %s (tried using my botnetnick)", dcc[idx].host);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
    } else if (in_chain(dcc[idx].nick)) {

      dcc[idx].type = &DCC_DUPWAIT;
      dcc[idx].u.dupwait = (struct dupwait_info *) calloc(1, sizeof(struct dupwait_info));
      dcc[idx].u.dupwait->atr = atr;
      return;
    }
//    dcc[idx].u.enc = (struct enc_link_dcc *) calloc(1, sizeof(struct enc_link_dcc));
//    dcc[idx].u.enc->method_number = 0;
//    link_get_method(idx);
  } else {
  }

  if (dcc[idx].bot && !(dcc[idx].status & STAT_UNIXDOMAIN)) {
    char shost[UHOSTLEN + 20] = "", sip[UHOSTLEN + 20] = "", user[30] = "";
    simple_snprintf(shost, sizeof(shost), "-telnet!%s", dcc[idx].host);
    char *p = strchr(dcc[idx].host, '@');
    strlcpy(user, dcc[idx].host, p - dcc[idx].host + 1);
    simple_snprintf(sip, sizeof(sip), "-telnet!%s@%s", user, iptostr(htonl(dcc[idx].addr)));
    struct userrec *u = get_user_by_handle(userlist, nick);

    ok = 1;

    // Require that the linking bot has a matching host

    if (u) {
      /* Check for -telnet!ident@ip or -telnet!ident@host */
      if (!user_has_matching_host(nick, u, sip) && !user_has_matching_host(nick, u, shost)) {
	ok = 0;
      }
    } else {
      ok = 0;
    }

    if (!ok) {
      putlog(LOG_BOTS, "*", "Denied link to '%s': Host not recognized: %s", nick, dcc[idx].host);
      putlog(LOG_BOTS, "*", "If this host/bot is trusted: %s+host %s %s", settings.dcc_prefix, nick, shost);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
    }
  }

  dcc_telnet_pass(idx, atr);
}


static void
dcc_telnet_pass(int idx, int atr)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0 };

  get_user_flagrec(dcc[idx].user, &fr, NULL);

  /* No password set? */
  if (!dcc[idx].user->bot && (u_pass_match(dcc[idx].user, "-"))) {
    dprintf(idx, "Can't telnet until you have a password set.\r\n");
    putlog(LOG_MISC, "*", "Refused [%s]%s (no password)", dcc[idx].nick, dcc[idx].host);
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }

  if (dcc[idx].type == &DCC_DUPWAIT) {
    free(dcc[idx].u.dupwait);
    dcc[idx].u.other = NULL;
  }

  dcc[idx].type = &DCC_CHAT_PASS;
  dcc[idx].timeval = now;

  if ((conf.bot->hub && !glob_huba(fr)) || (!conf.bot->hub && ischanhub() && !glob_chuba(fr)))
    dcc[idx].status |= STAT_PARTY;

  if (!dcc[idx].bot) {
    //bots dont need this
    dcc[idx].u.chat = (struct chat_info *) calloc(1, sizeof(struct chat_info));
    struct chat_info dummy;
    strlcpy(dcc[idx].u.chat->con_chan, chanset ? chanset->dname : "*", sizeof(dummy.con_chan));
  }

  if (conf.bot->hub || (conf.bot->localhub && (dcc[idx].status & STAT_UNIXDOMAIN))) {
    if (dcc[idx].encrypt) {
      /* negotiate a new linking scheme */
      int i = 0;
      char buf[1024] = "", rand[51] = "";
  
      make_rand_str(rand, 50);

      link_hash(idx, rand);

      
      for (i = 0; enclink[i].name; i++) {
        if (enclink[i].type == LINK_CLEARTEXT && !link_cleartext) continue;
        simple_snprintf(&buf[strlen(buf)], sizeof(buf) - strlen(buf), "%d ", enclink[i].type);
      }
      dprintf(-dcc[idx].sock, "neg? %s %s\n", rand, buf);
    } else {
      /* Turn off remote telnet echo (send IAC WILL ECHO). */
      dprintf(idx, "\n%s" TLN_IAC_C TLN_WILL_C TLN_ECHO_C "\n", "Enter your password");
    }
  } else
    dprintf(idx, "%s\n" TLN_IAC_C TLN_WILL_C TLN_ECHO_C, response(RES_PASSWORD));
}

static void
eof_dcc_telnet_id(int idx)
{
  if (dcc[idx].port)
    putlog(LOG_MISC, "*", "Lost telnet connection to %s/%d", dcc[idx].host, dcc[idx].port);
  else
    putlog(LOG_MISC, "*", "Lost local connection for: %s", dcc[idx].host);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
timeout_dcc_telnet_id(int idx)
{
  putlog(LOG_MISC, "*", "Ident timeout on telnet: %s", dcc[idx].host);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
display_dcc_telnet_id(int idx, char *buf, size_t bufsiz)
{
  simple_snprintf(buf, bufsiz, "t-in  waited %ds", (int) (now - dcc[idx].timeval));
}

struct dcc_table DCC_TELNET_ID = {
  "TELNET_ID",
  0,
  eof_dcc_telnet_id,
  dcc_telnet_id,
  &password_timeout,
  timeout_dcc_telnet_id,
  display_dcc_telnet_id,
  NULL,
  out_dcc_general,
  NULL
};

static void
dcc_socket(int idx, char *buf, int len)
{
}

static void
eof_dcc_socket(int idx)
{
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
display_dcc_socket(int idx, char *buf, size_t bufsiz)
{
  strlcpy(buf, "sock  (stranded)", bufsiz);
}

struct dcc_table DCC_SOCKET = {
  "SOCKET",
  DCT_VALIDIDX,
  eof_dcc_socket,
  dcc_socket,
  NULL,
  NULL,
  display_dcc_socket,
  NULL,
  NULL,
  NULL
};

static void
__attribute__((const))
dcc_identwait(int idx, char *buf, int len)
{
  /* Ignore anything now */
}

void
eof_dcc_identwait(int idx)
{
  putlog(LOG_MISC, "*", "Lost connection while identing [%s/%d]", dcc[idx].host, dcc[idx].port);
  for (int i = 0; i < dcc_total; i++)
    if (dcc[i].type && (dcc[i].type == &DCC_IDENT) && (dcc[i].uint.ident_sock == dcc[idx].sock)) {
      killsock(dcc[i].sock);    /* Cleanup ident socket */
      dcc[i].u.other = 0;
      lostdcc(i);
      break;
    }
  killsock(dcc[idx].sock);      /* Cleanup waiting socket */
  dcc[idx].u.other = 0;
  lostdcc(idx);
}

static void
display_dcc_identwait(int idx, char *buf, size_t bufsiz)
{
  simple_snprintf(buf, bufsiz, "idtw  waited %ds", (int) (now - dcc[idx].timeval));
}

struct dcc_table DCC_IDENTWAIT = {
  "IDENTWAIT",
  0,
  eof_dcc_identwait,
  dcc_identwait,
  NULL,
  NULL,
  display_dcc_identwait,
  NULL,
  NULL,
  NULL
};

void
dcc_ident(int idx, char *buf, int len)
{
  char ident_response[512] = "", uid[512] = "", buf1[UHOSTLEN] = "";

  sscanf(buf, "%*[^:]:%[^:]:%*[^:]:%[^\n]\n", ident_response, uid);
  rmspace(ident_response);
  if (ident_response[0] != 'U') {
    dcc[idx].timeval = now;
    return;
  }
  rmspace(uid);
  uid[20] = 0;                  /* 20 character ident max */
  for (int i = 0; i < dcc_total; i++)
    if (dcc[i].type && (dcc[i].type == &DCC_IDENTWAIT) && (dcc[i].sock == dcc[idx].uint.ident_sock)) {
      simple_snprintf(buf1, sizeof(buf1), "%s@%s", uid, dcc[idx].host);
      dcc_telnet_got_ident(i, buf1);
    }
  dcc[idx].u.other = 0;
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

void
eof_dcc_ident(int idx)
{
  char buf[UHOSTLEN] = "";

  for (int i = 0; i < dcc_total; i++)
    if (dcc[i].type && (dcc[i].type == &DCC_IDENTWAIT) && (dcc[i].sock == dcc[idx].uint.ident_sock)) {
      putlog(LOG_MISC, "*", "Timeout/EOF ident connection");
      simple_snprintf(buf, sizeof(buf), "telnet@%s", dcc[idx].host);
      dcc_telnet_got_ident(i, buf);
    }
  killsock(dcc[idx].sock);
  dcc[idx].u.other = 0;
  lostdcc(idx);
}

static void
display_dcc_ident(int idx, char *buf, size_t bufsiz)
{
  simple_snprintf(buf, bufsiz, "idnt  (sock %d)", dcc[idx].uint.ident_sock);
}

struct dcc_table DCC_IDENT = {
  "IDENT",
  0,
  eof_dcc_ident,
  dcc_ident,
  &identtimeout,
  eof_dcc_ident,
  display_dcc_ident,
  NULL,
  NULL,
  NULL
};

static void
dcc_telnet_got_ident(int i, char *host)
{
  int idx;

  for (idx = 0; idx < dcc_total; idx++)
    if (dcc[i].type && (dcc[idx].type == &DCC_TELNET) && (dcc[idx].sock == dcc[i].uint.ident_sock))
      break;

  dcc[i].u.other = 0;

  if (idx == dcc_total || !dcc[idx].type) {
    putlog(LOG_MISC, "*", "Lost ident wait telnet socket!!");
    killsock(dcc[i].sock);
    lostdcc(i);
    return;
  }

  strlcpy(dcc[i].host, host, sizeof(dcc[i].host));

  bool unix_domain = 0;
  if (!strcmp(host, "UNKNOWN@localhost"))
    unix_domain = 1;


  if (!unix_domain) {
    char shost[UHOSTLEN + 20] = "", sip[UHOSTLEN + 20] = "";
    char *p = strchr(host, '@');
    *p = 0;

    simple_snprintf(shost, sizeof(shost), "-telnet!%s", dcc[i].host);
    simple_snprintf(sip, sizeof(sip), "-telnet!%s@%s", host, iptostr(htonl(dcc[i].addr)));

    if (match_ignore(shost) || match_ignore(sip)) {
      putlog(LOG_DEBUG, "*", "Ignored telnet connection from: %s[%s]",dcc[i].host, iptostr(htonl(dcc[i].addr)));
      killsock(dcc[i].sock);
      lostdcc(i);
      return;
    }

    if (protect_telnet) {
      struct userrec *u = NULL;
      bool ok = 1;

      u = dcc[i].user;
      if (!u)
        u = get_user_by_host(sip);			/* Check for -telnet!ident@ip */
      if (!u)
        u = get_user_by_host(shost);		/* Check for -telnet!ident@host */
      if (!u)
        ok = 0;

      if (ok && u && conf.bot->hub && !(u->flags & USER_HUBA))
        ok = 0;
      /* if I am a chanhub and they dont have +c then drop */
      if (ok && (!conf.bot->hub && ischanhub() && u && !(u->flags & USER_CHUBA)))
        ok = 0;
  /*    else if (!(u->flags & USER_PARTY))
      ok = 0; */
      if (!ok && u && u->bot)
        ok = 1;
      if (!ok && (dcc[idx].status & LSTN_PUBLIC))
        ok = 1;
      if (!ok) {
        putlog(LOG_MISC, "*", "Denied telnet: %s, No Access", dcc[i].host);
        killsock(dcc[i].sock);
        lostdcc(i);
        return;
      }
    }
  }

  /* Do not buffer data anymore. All received and stored data is passed
   * over to the dcc functions from now on.  */
  sockoptions(dcc[i].sock, EGG_OPTION_UNSET, SOCK_BUFFER);

  dcc[i].type = &DCC_TELNET_ID;

  /* Copy acceptable-nick/host mask */
  dcc[i].status = (STAT_TELNET | STAT_ECHO | STAT_COLOR | STAT_BANNER | STAT_CHANNELS | STAT_BOTS | STAT_WHOM);

  if (unix_domain)
    dcc[i].status |= STAT_UNIXDOMAIN;

  /* Copy acceptable-nick/host mask */
  strlcpy(dcc[i].nick, dcc[idx].host, sizeof(dcc[i].nick));
  dcc[i].timeval = now;

  dcc[i].u.other = NULL;
  /* This is so we dont tell someone doing a portscan anything
   * about ourselves. <cybah>
   */
  if (conf.bot->hub || (conf.bot->localhub && unix_domain))
    dprintf(i, " \n");			/* represents hub that support new linking scheme */
  else
    dprintf(i, "%s\n", response(RES_USERNAME));
}
/* vim: set sts=2 sw=2 ts=8 et: */
