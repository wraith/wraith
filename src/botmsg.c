
/* 
 * botmsg.c -- handles:
 *   formatting of messages to be sent on the botnet
 *   sending differnet messages to different versioned bots
 * 
 * by Darrin Smith (beldin@light.iinet.net.au)
 * 
 * $Id: botmsg.c,v 1.13 2000/01/08 21:23:13 per Exp $
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
#include "tandem.h"

extern struct dcc_t *dcc;
extern struct logcategory *logcat;
extern int dcc_total,
  tands;
extern char botnetnick[];
extern party_t *party;
extern time_t now;

#ifdef G_USETCL
extern Tcl_Interp *interp;
#endif
extern struct userrec *userlist;

char OBUF[1024];

/* thank you ircu :) */
char tobase64array[64] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '[', ']'
};

char *int_to_base64(unsigned int val)
{
  static char buf_base64[12];
  int i = 11;

  buf_base64[11] = 0;
  if (!val) {
    buf_base64[10] = 'A';
    return buf_base64 + 10;
  }
  while (val) {
    i--;
    buf_base64[i] = tobase64array[val & 0x3f];
    val = val >> 6;
  }
  return buf_base64 + i;
}

char *int_to_base10(int val)
{
  static char buf_base10[17];
  int p = 0;
  int i = 16;

  buf_base10[16] = 0;
  if (!val) {
    buf_base10[15] = '0';
    return buf_base10 + 15;
  }
  if (val < 0) {
    p = 1;
    val *= -1;
  }
  while (val) {
    i--;
    buf_base10[i] = '0' + (val % 10);
    val /= 10;
  }
  if (p) {
    i--;
    buf_base10[i] = '-';
  }
  return buf_base10 + i;
}

char *unsigned_int_to_base10(unsigned int val)
{
  static char buf_base10[16];
  int i = 15;

  buf_base10[15] = 0;
  if (!val) {
    buf_base10[14] = '0';
    return buf_base10 + 14;
  }
  while (val) {
    i--;
    buf_base10[i] = '0' + (val % 10);
    val /= 10;
  }
  return buf_base10 + i;
}

int simple_sprintf EGG_VARARGS_DEF(char *, arg1)
{
  char *buf,
   *format,
   *s;
  int c = 0,
    i;

  va_list va;
  buf = EGG_VARARGS_START(char *, arg1, va);
  format = va_arg(va, char *);

  while (*format && (c < 1023)) {
    if (*format == '%') {
      format++;
      switch (*format) {
      case 's':
	s = va_arg(va, char *);

	break;
      case 'd':
      case 'i':
	i = va_arg(va, int);

	s = int_to_base10(i);
	break;
      case 'D':
	i = va_arg(va, int);

	s = int_to_base64((unsigned int) i);
	break;
      case 'u':
	i = va_arg(va, unsigned int);

	s = unsigned_int_to_base10(i);
	break;
      case '%':
	buf[c++] = *format++;
	continue;
      case 'c':
	buf[c++] = (char) va_arg(va, int);

	format++;
	continue;
      default:
	continue;
      }
      if (s)
	while (*s && (c < 1023))
	  buf[c++] = *s++;
      format++;
    } else
      buf[c++] = *format++;
  }
  va_end(va);
  buf[c] = 0;
  return c;
}

/* ditto for tandem bots */
void send_tand_but(int x, char *buf, int len)
{
  int i,
    iso = 0;

  if (len < 0) {
    len = -len;
    iso = 1;
  }
  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type == &DCC_BOT) && (i != x) && (b_numver(i) >= NEAT_BOTNET))
      tputs(dcc[i].sock, buf, len);
}

#ifdef G_DCCPASS
void botnet_send_cmdpass(int idx, char *cmd, char *pass)
{
  char *buf;

  if (tands > 0) {
    buf = nmalloc(strlen(cmd) + strlen(pass) + 10);
    sprintf(buf, STR("cp %s %s\n"), cmd, pass);
    send_tand_but(idx, buf, strlen(buf));
    nfree(buf);
  }
}
#endif

#ifdef HUB
void botnet_send_enforcetopic(struct chanset_t * chan) {
  struct userrec * u;
  int cnt=0;
  for (u=userlist;u;u=u->next) {
    if ((u->flags & USER_BOT) && (nextbot(u->handle)>=0) 
#ifdef G_BACKUP
	&& ( !(u->flags & USER_BACKUPBOT) || channel_backup(chan))
#endif
	&& (bot_hublevel(u)==999)) {
      cnt++;
    }
  }
  if (cnt) {
    cnt = rand() % cnt + 1;
    for (u=userlist;u;u=u->next) {
      if ((u->flags & USER_BOT) && (nextbot(u->handle)>=0) 
#ifdef G_BACKUP
	  && ( !(u->flags & USER_BACKUPBOT) || channel_backup(chan))
#endif
	  && (bot_hublevel(u)==999)) {
	cnt--;
	if (!cnt) {
	  cnt=nextbot(u->handle);
	  if (cnt>=0) {
	    char buf[512];
	    sprintf(buf, STR("ltp %s %s\n"), chan->name, chan->topic);
	    botnet_send_zapf(cnt, botnetnick, u->handle, buf);
	  }
	  return;
	}
      }
    }
  }

}


void botnet_send_limitcheck(struct chanset_t * chan) {
  struct userrec * u;
  int cnt=0;
  for (u=userlist;u;u=u->next) {
    if ((u->flags & USER_BOT) && (nextbot(u->handle)>=0) && (u->flags & USER_DOLIMIT) 
#ifdef G_BACKUP
	&& ( !(u->flags & USER_BACKUPBOT) || channel_backup(chan))
#endif
	&& (bot_hublevel(u)==999)) {
      cnt++;
    }
  }
  if (cnt) {
    cnt = rand() % cnt + 1;
    for (u=userlist;u;u=u->next) {
      if ((u->flags & USER_BOT) && (nextbot(u->handle)>=0) && (u->flags & USER_DOLIMIT)
#ifdef G_BACKUP
	  && ( !(u->flags & USER_BACKUPBOT) || channel_backup(chan))
#endif
	  && (bot_hublevel(u)==999)) {
	cnt--;
	if (!cnt) {
	  cnt=nextbot(u->handle);
	  if (cnt>=0) {
	    int l;
	    l=simple_sprintf(OBUF, STR("lc %s %s\n"), u->handle, chan->name);
	    tputs(dcc[cnt].sock, OBUF, l);
	  }
	  return;
	}
      }
    }
  }
}
#endif

void botnet_send_log_broad(int idx, char * from, struct logcategory * lc, char *msg)
{
  char *buf;

  if (tands > 0) {
    buf = nmalloc(strlen(lc->name) + strlen(msg) + strlen(from) + 10);
    sprintf(buf, STR("lg %s * %s %s\n"), from, lc->name, msg);
    send_tand_but(idx, buf, strlen(buf));
    nfree(buf);
  }
}

void botnet_send_log(char * bot, struct logcategory * lc, char *msg)
{
  char *buf;
  int i;
  if ((i=nextbot(bot))>=0) {
    buf = nmalloc(strlen(lc->name) + strlen(msg) + strlen(bot) + strlen(botnetnick) + 10);
    sprintf(buf, STR("lgc %s %s %s %s\n"), botnetnick, bot, lc->name, msg);
    tputs(dcc[i].sock, buf, strlen(buf));
    nfree(buf);
  }
}


int botnet_send_cmd(char * fbot, char * bot, char * from, int fromidx, char * cmd) {
  int i = nextbot(bot);
  if (i>=0) {
    char buf[2048];
    sprintf(buf, STR("rc %s %s %s %i %s\n"), bot, fbot, from, fromidx, cmd);
    tputs(dcc[i].sock, buf, strlen(buf));
    return 1;
  } else if (!strcmp(bot, botnetnick)) {
    char tmp[24];
    sprintf(tmp, "%i", fromidx);
    gotremotecmd(botnetnick, botnetnick, from, tmp, cmd);
  }
  return 0;
}

void botnet_send_cmd_broad(int idx, char * fbot, char * from, int fromidx, char * cmd) {
  if (tands > 0) {
    char buf[2048];
    sprintf(buf, STR("rc * %s %s %i %s\n"), fbot, from, fromidx, cmd);
    send_tand_but(idx, buf, strlen(buf));
  }
  if (idx<0) {
    char tmp[24];
    sprintf(tmp, "%i", fromidx);
    gotremotecmd("*", botnetnick, from, tmp, cmd);
  }
}

void botnet_send_cmdreply(char * fbot, char * bot, char * to, char * toidx, char * ln) {
  int i = nextbot(bot);
  if (i>=0) {
    char buf[2048];
    sprintf(buf, STR("rr %s %s %s %s %s\n"), bot, fbot, to, toidx, ln);
    tputs(dcc[i].sock, buf, strlen(buf));
  } else if (!strcmp(bot, botnetnick)) {
    gotremotereply(botnetnick, to, toidx, ln);
  }
}



void botnet_send_bye()
{
  if (tands > 0) {
    send_tand_but(-1, STR("bye\n"), 4);
  }
}

void botnet_send_chan(int idx, char *botnick, char *user, int chan, char *data)
{
  int i;

  if ((tands > 0) && (chan < GLOBAL_CHANS)) {
    if (user) {
      i = simple_sprintf(OBUF, STR("c %s@%s %D %s\n"), user, botnick, chan, data);
    } else {
      i = simple_sprintf(OBUF, STR("c %s %D %s\n"), botnick, chan, data);
    }
    send_tand_but(idx, OBUF, -i);
  }
}

void botnet_send_act(int idx, char *botnick, char *user, int chan, char *data)
{
  int i;

  if ((tands > 0) && (chan < GLOBAL_CHANS)) {
    if (user) {
      i = simple_sprintf(OBUF, STR("a %s@%s %D %s\n"), user, botnick, chan, data);
    } else {
      i = simple_sprintf(OBUF, STR("a %s %D %s\n"), botnick, chan, data);
    }
    send_tand_but(idx, OBUF, -i);
  }
}

void botnet_send_chat(int idx, char *botnick, char *data)
{
  int i;

  if (tands > 0) {
    i = simple_sprintf(OBUF, STR("ct %s %s\n"), botnick, data);
    send_tand_but(idx, OBUF, -i);
  }
}

void botnet_send_ping(int idx)
{
  tputs(dcc[idx].sock, STR("pi\n"), 3);
  dcc[idx].pingtime = now;
}

void botnet_send_pong(int idx)
{
  tputs(dcc[idx].sock, STR("po\n"), 3);
}

void botnet_send_priv EGG_VARARGS_DEF(int, arg1)
{
  int idx,
    l;
  char *from,
   *to,
   *tobot,
   *format;
  char tbuf[1024];

  va_list va;
  idx = EGG_VARARGS_START(int, arg1, va);
  from = va_arg(va, char *);
  to = va_arg(va, char *);
  tobot = va_arg(va, char *);
  format = va_arg(va, char *);

#ifdef HAVE_VSNPRINTF
  if (vsnprintf(tbuf, 450, format, va) < 0)
    tbuf[450] = 0;
#else
  vsprintf(tbuf, format, va);
#endif
  va_end(va);
  if (tobot) {
    l = simple_sprintf(OBUF, STR("p %s %s@%s %s\n"), from, to, tobot, tbuf);
  } else {
    l = simple_sprintf(OBUF, STR("p %s %s %s\n"), from, to, tbuf);
  }
  tputs(dcc[idx].sock, OBUF, l);
}

void botnet_send_who(int idx, char *from, char *to, int chan)
{
  int l;

  l = simple_sprintf(OBUF, STR("w %s %s %D\n"), from, to, chan);
  tputs(dcc[idx].sock, OBUF, l);
}

void botnet_send_infoq(int idx, char *par)
{
  int i = simple_sprintf(OBUF, STR("i? %s\n"), par);

  send_tand_but(idx, OBUF, i);
}

void botnet_send_unlink(int idx, char *who, char *via, char *bot, char *reason)
{
  int l;

  l = simple_sprintf(OBUF, STR("ul %s %s %s %s\n"), who, via, bot, reason);
  tputs(dcc[idx].sock, OBUF, l);
}

void botnet_send_link(int idx, char *who, char *via, char *bot)
{
  int l;

  l = simple_sprintf(OBUF, STR("l %s %s %s\n"), who, via, bot);
  tputs(dcc[idx].sock, OBUF, l);
}

void botnet_send_unlinked(int idx, char *bot, char *args)
{
  int l;

  Context;

  if (tands > 0) {
    l = simple_sprintf(OBUF, STR("un %s %s\n"), bot, args ? args : "");
    send_tand_but(idx, OBUF, l);
  }
}

void botnet_send_nlinked(int idx, char *bot, char *next, char flag, int vernum)
{
  int l;

  if (tands > 0) {
    l = simple_sprintf(OBUF, STR("n %s %s %c%D\n"), bot, next, flag, vernum);
    send_tand_but(idx, OBUF, l);
  }
}

void botnet_send_traced(int idx, char *bot, char *buf)
{
  int l;

  l = simple_sprintf(OBUF, STR("td %s %s\n"), bot, buf);
  tputs(dcc[idx].sock, OBUF, l);
}

void botnet_send_trace(int idx, char *to, char *from, char *buf)
{
  int l;

  l = simple_sprintf(OBUF, STR("t %s %s %s:%s\n"), to, from, buf, botnetnick);
  tputs(dcc[idx].sock, OBUF, l);
}

void botnet_send_update(int idx, tand_t * ptr)
{
  int l;

  if (tands > 0) {
    l = simple_sprintf(OBUF, STR("u %s %c%D\n"), ptr->bot, ptr->share, ptr->ver);
    send_tand_but(idx, OBUF, l);
  }
}

void botnet_send_reject(int idx, char *fromp, char *frombot, char *top, char *tobot, char *reason)
{
  int l;
  char to[NOTENAMELEN + 1],
    from[NOTENAMELEN + 1];

  if (tobot) {
    simple_sprintf(to, STR("%s@%s"), top, tobot);
    top = to;
  }
  if (frombot) {
    simple_sprintf(from, STR("%s@%s"), fromp, frombot);
    fromp = from;
  }
  if (!reason)
    reason = "";
  l = simple_sprintf(OBUF, STR("r %s %s %s\n"), fromp, top, reason);
  tputs(dcc[idx].sock, OBUF, l);
}

void botnet_send_zapf(int idx, char *a, char *b, char *c)
{
  int l;

  l = simple_sprintf(OBUF, STR("z %s %s %s\n"), a, b, c);
  tputs(dcc[idx].sock, OBUF, l);
}

void botnet_send_cfg(int idx, struct cfg_entry * entry) {
  int l;

  l = simple_sprintf(OBUF, STR("cg %s %s\n"), entry->name, entry->gdata ? entry->gdata : "");
  tputs(dcc[idx].sock, OBUF, l);
}

void botnet_send_cfg_broad(int idx, struct cfg_entry * entry) {
  int l;
  if (tands > 0) {
      l = simple_sprintf(OBUF, STR("cg %s %s\n"), entry->name, entry->gdata ? entry->gdata : "");
    send_tand_but(idx, OBUF, l);
  }
}

void botnet_send_logsettings(int idx)
{
  struct logcategory *lc;
  char tmp[512];

  for (lc = logcat; lc; lc = lc->next) {
    sprintf(tmp, STR("lst %s %i %i %i %i\n"), lc->name, lc->logtochan, lc->logtofile, lc->broadcast, lc->flags);
    tputs(dcc[idx].sock, tmp, strlen(tmp));
  }
}

void botnet_send_logsettings_broad(int idx, char *a)
{
  int l;

  if (tands > 0) {
    l = simple_sprintf(OBUF, STR("lst %s\n"), a);
    send_tand_but(idx, OBUF, l);
  }
}

void botnet_send_zapf_broad(int idx, char *a, char *b, char *c)
{
  int l;

  if (tands > 0) {
    l = simple_sprintf(OBUF, STR("zb %s %s%s%s\n"), a, b ? b : "", b ? " " : "", c);
    send_tand_but(idx, OBUF, l);
  }
}

void botnet_send_filereject(int idx, char *path, char *from, char *reason)
{
  int l;

  l = simple_sprintf(OBUF, STR("f! %s %s %s\n"), path, from, reason);
  tputs(dcc[idx].sock, OBUF, l);
}

void botnet_send_idle(int idx, char *bot, int sock, int idle, char *away)
{
  int l;

  Context;
  if (tands > 0) {
    l = simple_sprintf(OBUF, STR("i %s %D %D %s\n"), bot, sock, idle, away ? away : "");
    send_tand_but(idx, OBUF, -l);
  }
}

void botnet_send_away(int idx, char *bot, int sock, char *msg, int linking)
{
  int l;

  if (tands > 0) {
    l = simple_sprintf(OBUF, STR("aw %s%s %D %s\n"), ((idx >= 0) && linking) ? "!" : "", bot, sock, msg ? msg : "");
    send_tand_but(idx, OBUF, -l);
  }
}

void botnet_send_join_idx(int useridx, int oldchan)
{
  int l;

  if (tands > 0) {
    l = simple_sprintf(OBUF, STR("j %s %s %D %c%D %s\n"), botnetnick, dcc[useridx].nick, dcc[useridx].u.chat->channel, geticon(useridx), dcc[useridx].sock, dcc[useridx].host);
    send_tand_but(-1, OBUF, -l);
  }
}

void botnet_send_join_party(int idx, int linking, int useridx, int oldchan)
{
  int l;

  Context;
  if (tands > 0) {
    l =
      simple_sprintf(OBUF, STR("j %s%s %s %D %c%D %s\n"),
		     linking ? "!" : "", party[useridx].bot, party[useridx].nick, party[useridx].chan, party[useridx].flag, party[useridx].sock, safe_str(party[useridx].from));
    send_tand_but(idx, OBUF, -l);
  }
}

void botnet_send_part_idx(int useridx, char *reason)
{
  int l = simple_sprintf(OBUF, STR("pt %s %s %D %s\n"), botnetnick,
			 dcc[useridx].nick, dcc[useridx].sock,
			 reason ? reason : "");

  if (tands > 0) {
    send_tand_but(-1, OBUF, -l);
  }
}

void botnet_send_part_party(int idx, int partyidx, char *reason, int silent)
{
  int l;

  if (tands > 0) {
    l = simple_sprintf(OBUF, STR("pt %s%s %s %D %s\n"), silent ? "!" : "", party[partyidx].bot, party[partyidx].nick, party[partyidx].sock, reason ? reason : "");
    send_tand_but(idx, OBUF, -l);
  }
}

void botnet_send_nkch(int useridx, char *oldnick)
{
  int l;

  if (tands > 0) {
    l = simple_sprintf(OBUF, STR("nc %s %D %s\n"), botnetnick, dcc[useridx].sock, dcc[useridx].nick);
    send_tand_but(-1, OBUF, -l);
  }
}

void botnet_send_nkch_part(int butidx, int useridx, char *oldnick)
{
  int l;

  if (tands > 0) {
    l = simple_sprintf(OBUF, STR("nc %s %D %s\n"), party[useridx].bot, party[useridx].sock, party[useridx].nick);
    send_tand_but(butidx, OBUF, -l);
  }
}

/* this part of add_note is more relevant to the botnet than
 * to the notes file */
int add_note(char *to, char *from, char *msg, int idx, int echo)
{
  int status,
    i,
    iaway,
    sock;
  char *p,
    botf[81],
    ss[81],
    ssf[81];
  struct userrec *u;

  if (strlen(msg) > 450)
    msg[450] = 0;		/* notes have a limit */
  /* note length + PRIVMSG header + nickname + date  must be <512  */
  p = strchr(to, '@');
  if (p != NULL) {		/* cross-bot note */
    char x[21];

    *p = 0;
    strncpy0(x, to, 20);
    *p = '@';
    p++;
    if (!strcasecmp(p, botnetnick))	/* to me?? */
      return add_note(x, from, msg, idx, echo);	/* start over, dimwit. */
    if (strcasecmp(from, botnetnick)) {
      if (strlen(from) > 40)
	from[40] = 0;
      if (strchr(from, '@')) {
	strcpy(botf, from);
      } else
	sprintf(botf, STR("%s@%s"), from, botnetnick);
    } else
      strcpy(botf, botnetnick);
    i = nextbot(p);
    if (i < 0) {
      if (idx >= 0)
	dprintf(idx, STR("That bot isn't there.\n"));
      return NOTE_ERROR;
    }
    if ((idx >= 0) && (echo))
      dprintf(idx, STR("-> %s@%s: %s\n"), x, p, msg);
    if (idx >= 0) {
      sprintf(ssf, STR("%lu:%s"), dcc[idx].sock, botf);
      botnet_send_priv(i, ssf, x, p, "%s", msg);
    } else
      botnet_send_priv(i, botf, x, p, "%s", msg);
    return NOTE_OK;		/* forwarded to the right bot */
  }
  /* might be form STR("sock:nick") */
  splitc(ssf, from, ':');
  rmspace(ssf);
  splitc(ss, to, ':');
  rmspace(ss);
  if (!ss[0])
    sock = (-1);
  else
    sock = atoi(ss);
  /* don't process if there's a note binding for it */
  if (!(u = get_user_by_handle(userlist, to))) {
    if (idx >= 0)
      dprintf(idx, STR("I don't know anyone by that name.\n"));
    return NOTE_ERROR;
  }
  if (is_bot(u)) {
    if (idx >= 0)
      dprintf(idx, STR("That's a bot. You can't leave notes for a bot.\n"));
    return NOTE_ERROR;
  }
  if (match_noterej(u, from)) {
    if (idx >= 0)
      dprintf(idx, STR("%s rejected your note\n"), u->handle);
    return NOTE_REJECT;
  }
  status = NOTE_STORED;
  iaway = 0;
  /* online right now? */
  for (i = 0; i < dcc_total; i++) {
    if ((dcc[i].type->flags & DCT_GETNOTES) && ((sock == (-1)) || (sock == dcc[i].sock)) && (!strcasecmp(dcc[i].nick, to))) {
      int aok = 1;

      if (dcc[i].type == &DCC_CHAT)
	if ((dcc[i].u.chat->away != NULL) && (idx != (-2))) {
	  /* only check away if it's not from a bot */
	  aok = 0;
	  if (idx >= 0)
	    dprintf(idx, STR("%s is away: %s\n"), dcc[i].nick, dcc[i].u.chat->away);
	  if (!iaway)
	    iaway = i;
	  status = NOTE_AWAY;
	}
      if (aok) {
	char *p,
	 *fr = from;
	int l = 0;
	char work[1024];

	while ((*msg == '<') || (*msg == '>')) {
	  p = newsplit(&msg);
	  if (*p == '<')
	    l += simple_sprintf(work + l, STR("via %s, "), p + 1);
	  else if (*from == '@')
	    fr = p + 1;
	}
	if ((idx == (-2)) || (!strcasecmp(from, botnetnick)))
	  dprintf(i, STR("*** [%s] %s%s\n"), fr, l ? work : "", msg);
	else
	  dprintf(i, STR("%cNote [%s]: %s%s\n"), 7, fr, l ? work : "", msg);
	if ((idx >= 0) && (echo))
	  dprintf(idx, STR("-> %s: %s\n"), to, msg);
	return NOTE_OK;
      }
    }
  }
  if (idx == (-2))
    return NOTE_OK;		/* error msg from a tandembot: don't store */

/* call store note here */
#ifdef G_USETCL
  Tcl_SetVar(interp, STR("_from"), from, 0);
  Tcl_SetVar(interp, STR("_to"), to, 0);
  Tcl_SetVar(interp, STR("_data"), msg, 0);
  simple_sprintf(ss, "%d", dcc[idx].sock);
  Tcl_SetVar(interp, STR("_idx"), ss, 0);
  if (Tcl_VarEval(interp, STR("storenote"), STR(" $_from $_to $_data $_idx"), NULL) == TCL_OK) {
    if (interp->result && interp->result[0]) {
      /* strncpy0(to, interp->result, NOTENAMELEN);
   to[NOTENAMELEN] = 0; *//* notebug fixed ;) -- drummer 29May1999 */
      status = NOTE_FWD;
    }
    if (status == NOTE_AWAY) {
      /* user is away in all sessions -- just notify the user that a
       * message arrived and was stored. (only oldest session is notified.) */
      dprintf(iaway, STR("*** Note arrived for you.\n"));
    }
    return status;
  }
#endif
  return NOTE_ERROR;
}
