
/* 
 * botcmd.c -- handles:
 *   commands that comes across the botnet
 *   userfile transfer and update commands from sharebots
 * 
 * dprintf'ized, 10nov1995
 * 
 * $Id: botcmd.c,v 1.16 2000/01/22 23:37:02 per Exp $
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
#include "users.h"
#include "chan.h"

extern char botnetnick[],
  ver[],
  network[];
extern int dcc_total,
  remote_boots,
  timesync,
  noshare,
  got_first_userfile;
extern struct dcc_t *dcc;
extern struct chanset_t *chanset;
extern struct userrec *userlist;
extern char OBUF[];

#ifdef G_USETCL
extern Tcl_Interp *interp;
#endif
extern time_t now,
  online_since;
extern party_t *party;

char TBUF[1024];		/* static buffer for goofy bot stuff */

char base64to[256] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0, 0,
  0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
  15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 62, 0, 63, 0, 0, 0, 26, 27, 28,
  29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
  48,

  49, 50, 51, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

int base64_to_int(char *buf)
{
  int i = 0;

  while (*buf) {
    i = i << 6;
    i += base64to[(int) *buf];
    buf++;
  }
  return i;
}

/* used for 1.0 compatibility: if a join message arrives with no sock#, */

/* i'll just grab the next fakesock # (incrementing to assure uniqueness) */
int fakesock = 2300;

void fake_alert(int idx, char *item, char *extra)
{
  dprintf(idx, STR("ct %s NOTICE: Fake message rejected (%s != %s).\n"), botnetnick, item, extra);
  log(LCAT_WARNING, STR("%s Fake message rejected (%s != %s)."), dcc[idx].nick, item, extra);
}

/* chan <from> <chan> <text> */
void bot_chan2(int idx, char *msg)
{
  char *from,
   *p;
  int i,
    chan;

  Context;
  from = newsplit(&msg);
  p = newsplit(&msg);
  chan = base64_to_int(p);
  /* strip annoying control chars */
  for (p = from; *p;) {
    if ((*p < 32) || (*p == 127))
      strcpy(p, p + 1);
    else
      p++;
  }
  p = strchr(from, '@');
  if (p) {
    sprintf(TBUF, STR("<%s> %s"), from, msg);
    *p = 0;
    if (!partyidle(p + 1, from)) {
      *p = '@';
      fake_alert(idx, STR("user"), from);
      return;
    }
    *p = '@';
    p++;
  } else {
    sprintf(TBUF, STR("*** (%s) %s"), from, msg);
    p = from;
  }
  i = nextbot(p);
  if (i != idx) {
    fake_alert(idx, STR("direction"), p);
  } else {
    chanout_but(-1, chan, STR("%s\n"), TBUF);
    /* send to new version bots */
    if (i >= 0)
      botnet_send_chan(idx, from, NULL, chan, msg);
    if (strchr(from, '@') != NULL)
      check_tcl_chat(from, chan, msg);
    else
      check_tcl_bcst(from, chan, msg);
  }
}

#ifdef G_DCCPASS
void bot_cmdpass(int idx, char *par)
{
  char * p;
  p=strchr(par, ' ');
  if (p) {
    *p++=0;
    botnet_send_cmdpass(idx, par, p);
    p--;
    *p=' ';
  } else {
    botnet_send_cmdpass(idx, par, "");
  }
  set_cmd_pass(par, 0);
}
#endif

void bot_config(int idx, char *par)
{
  got_config_share(idx, par);
}

void bot_logsettings(int idx, char *par)
{
  
  botnet_send_logsettings_broad(idx, par);
  set_log_info(par);
}

void bot_remotecmd(int idx, char *par) {
  char *tbot, *fbot, *fhnd, *fidx;
  tbot=newsplit(&par);
  fbot=newsplit(&par);
  fhnd=newsplit(&par);
  fidx=newsplit(&par);
  if (!strcmp(tbot, botnetnick)) {
    gotremotecmd(tbot, fbot, fhnd, fidx, par);
  } else if (!strcmp(tbot, "*")) {
    botnet_send_cmd_broad(idx, fbot, fhnd, atoi(fidx), par);
    gotremotecmd(tbot, fbot, fhnd, fidx, par);
  } else {
    if (nextbot(tbot)!=idx) 
      botnet_send_cmd(fbot, tbot, fhnd, atoi(fidx), par);
  }
}

void bot_remotereply(int idx, char *par) {
  char *tbot, *fbot, *fhnd, *fidx;
  tbot=newsplit(&par);
  fbot=newsplit(&par);
  fhnd=newsplit(&par);
  fidx=newsplit(&par);
  if (!strcmp(tbot, botnetnick)) {
    gotremotereply(fbot, fhnd, fidx, par);
  } else {
    if (nextbot(tbot)!=idx) 
      botnet_send_cmdreply(fbot, tbot, fhnd, fidx, par);
  }
}


/* chat <from> <notice>  -- only from bots */
void bot_chat(int idx, char *par)
{
  char *from;
  int i;

  Context;
  from = newsplit(&par);
  if (strchr(from, '@') != NULL) {
    fake_alert(idx, STR("bot"), from);
    return;
  }
  /* make sure the bot is valid */
  i = nextbot(from);
  if (i != idx) {
    fake_alert(idx, STR("direction"), from);
    return;
  }
  botnet_send_chat(idx, from, par);
}

/* actchan <from> <chan> <text> */
void bot_actchan(int idx, char *par)
{
  char *from,
   *p;
  int i,
    chan;

  Context;
  from = newsplit(&par);
  p = strchr(from, '@');
  if (p == NULL) {
    /* how can a bot do an action? */
    fake_alert(idx, STR("user@bot"), from);
    return;
  }
  *p = 0;
  if (!partyidle(p + 1, from)) {
    *p = '@';
    fake_alert(idx, STR("user"), from);
  }
  *p = '@';
  p++;
  i = nextbot(p);
  if (i != idx) {
    fake_alert(idx, STR("direction"), p);
    return;
  }
  p = newsplit(&par);
  chan = base64_to_int(p);
  for (p = from; *p;) {
    if ((*p < 32) || (*p == 127))
      strcpy(p, p + 1);
    else
      p++;
  }
  chanout_but(-1, chan, STR("* %s %s\n"), from, par);
  botnet_send_act(idx, from, NULL, chan, par);
  check_tcl_act(from, chan, par);
}

/* priv <from> <to> <message> */
void bot_priv(int idx, char *par)
{
  char *from,
   *p,
   *to = TBUF,
   *tobot;
  int i;

  Context;
  from = newsplit(&par);
  tobot = newsplit(&par);
  splitc(to, tobot, '@');
  p = strchr(from, '@');
  if (p != NULL)
    p++;
  else
    p = from;
  i = nextbot(p);
  if (i != idx) {
    fake_alert(idx, STR("direction"), p);
    return;
  }
  if (!to[0])
    return;			/* silently ignore notes to '@bot' this
				 * is legacy code */
  if (!strcasecmp(tobot, botnetnick)) {	/* for me! */
    if (p != from)
      botnet_send_priv(idx, botnetnick, from, NULL, STR("No notes."));
  } else {			/* pass it on */
    i = nextbot(tobot);
    if (i >= 0)
      botnet_send_priv(i, from, to, tobot, "%s", par);
  }
}

void bot_bye(int idx, char *par)
{
  char s[1024];

  Context;
  simple_sprintf(s, STR("Disconnected from: %s. %s"), dcc[idx].nick, par);
  log(LCAT_BOT, "%s", s);
  botnet_send_unlinked(idx, dcc[idx].nick, s);
  dprintf(idx, STR("*bye\n"));
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

void bot_log(int idx, char *par)
{
  char *from, *to;
  from=newsplit(&par);
  to=newsplit(&par);
  if (!strcmp(to, "*")) {
    char *buf;
    buf = nmalloc(strlen(from) + strlen(par) + 10);
    sprintf(buf, STR("lg %s * %s\n"), from, par);
    send_tand_but(idx, buf, strlen(buf));
    nfree(buf);
  }
  gotbotlog(idx, from, to, par, 0);
}

void bot_logc(int idx, char *par)
{
  char *from, *to;
  from=newsplit(&par);
  to=newsplit(&par);
  if (!strcmp(to, botnetnick)) {
    gotbotlog(idx, from, to, par, 1);
  } else {
    int i=nextbot(to);
    char * buf = NULL;
    if ((i>=0) && (i!=idx)) {
      buf=nmalloc(strlen(to) + strlen(from) + strlen(par) + 10);
      sprintf(buf, STR("lgc %s %s %s\n"), from, to, par);
      tputs(dcc[i].sock, buf, strlen(buf));
      nfree(buf);
    }
  }
}


void remote_tell_who(int idx, char *nick, int chan)
{
  int i = 10,
    k,
    l,
    ok = 0;
  char s[1024];
  struct chanset_t *c;

  Context;
  strcpy(s, STR("Channels: "));
  c = chanset;
  while (c != NULL) {
    l = strlen(c->name);
    if (i + l < 1021) {
      if (i > 10) {
	sprintf(s, STR("%s, %s"), s, c->name);
      } else {
	strcpy(s, c->name);
	i += (l + 2);
      }
    }
    c = c->next;
  }
  if (i > 10) {
    botnet_send_priv(idx, botnetnick, nick, NULL, STR("%s  (%s)"), s, ver);
  } else
    botnet_send_priv(idx, botnetnick, nick, NULL, STR("no channels  (%s)"), ver);
  if (chan == 0)
    botnet_send_priv(idx, botnetnick, nick, NULL, STR("Party line members:  (* = owner, + = master, @ = op)"));
  else {
    botnet_send_priv(idx, botnetnick, nick, NULL, STR("People on channel %s%d:  (* = owner, + = master, @ = op)\n"), (chan < GLOBAL_CHANS) ? "" : "*", chan % GLOBAL_CHANS);

  }
  for (i = 0; i < dcc_total; i++)
    if (dcc[i].type->flags & DCT_REMOTEWHO)
      if (dcc[i].u.chat->channel == chan) {
	k = sprintf(s, STR("  %c%-15s %s"), (geticon(i) == '-' ? ' ' : geticon(i)), dcc[i].nick, dcc[i].host);
	if (now - dcc[i].timeval > 300) {
	  unsigned long days,
	    hrs,
	    mins;

	  days = (now - dcc[i].timeval) / 86400;
	  hrs = ((now - dcc[i].timeval) - (days * 86400)) / 3600;
	  mins = ((now - dcc[i].timeval) - (hrs * 3600)) / 60;
	  if (days > 0)
	    sprintf(s + k, STR(" (idle %lud%luh)"), days, hrs);
	  else if (hrs > 0)
	    sprintf(s + k, STR(" (idle %luh%lum)"), hrs, mins);
	  else
	    sprintf(s + k, STR(" (idle %lum)"), mins);
	}
	botnet_send_priv(idx, botnetnick, nick, NULL, "%s", s);
	if (dcc[i].u.chat->away != NULL)
	  botnet_send_priv(idx, botnetnick, nick, NULL, STR("      AWAY: %s"), dcc[i].u.chat->away);
      }
  for (i = 0; i < dcc_total; i++)
    if (dcc[i].type == &DCC_BOT) {
      if (!ok) {
	ok = 1;
	botnet_send_priv(idx, botnetnick, nick, NULL, STR("Bots connected:"));
      }
      sprintf(s, STR("  %s%c%-15s %s"), dcc[i].status & STAT_CALLED ? "<-" : "->", dcc[i].status & STAT_SHARE ? '+' : ' ', dcc[i].nick, dcc[i].u.bot->version);
      botnet_send_priv(idx, botnetnick, nick, NULL, "%s", s);
    }
  ok = 0;
  for (i = 0; i < dcc_total; i++)
    if (dcc[i].type->flags & DCT_REMOTEWHO)
      if (dcc[i].u.chat->channel != chan) {
	if (!ok) {
	  ok = 1;
	  botnet_send_priv(idx, botnetnick, nick, NULL, STR("Other people on the bot:"));
	}
	l = sprintf(s, STR("  %c%-15s %s"), (geticon(i) == '-' ? ' ' : geticon(i)), dcc[i].nick, dcc[i].host);
	if (now - dcc[i].timeval > 300) {
	  k = (now - dcc[i].timeval) / 60;
	  if (k < 60)
	    sprintf(s + l, STR(" (idle %dm)"), k);
	  else
	    sprintf(s + l, STR(" (idle %dh%dm)"), k / 60, k % 60);
	}
	botnet_send_priv(idx, botnetnick, nick, NULL, "%s", s);
	if (dcc[i].u.chat->away != NULL)
	  botnet_send_priv(idx, botnetnick, nick, NULL, STR("      AWAY: %s"), dcc[i].u.chat->away);
      }
}

/* who <from@bot> <tobot> <chan#> */
void bot_who(int idx, char *par)
{
  char *from,
   *to,
   *p;
  int i,
    chan;

  Context;
  from = newsplit(&par);
  p = strchr(from, '@');
  if (!p) {
    sprintf(TBUF, STR("%s@%s"), from, dcc[idx].nick);
    from = TBUF;
  }
  to = newsplit(&par);
  if (!strcasecmp(to, botnetnick))
    to[0] = 0;			/* (for me) */
  chan = base64_to_int(par);
  if (to[0]) {			/* pass it on */
    i = nextbot(to);
    if (i >= 0)
      botnet_send_who(i, from, to, chan);
  } else {
    remote_tell_who(idx, from, chan);
  }
}

void bot_endlink(int idx, char *par)
{
  dcc[idx].status &= ~STAT_LINKING;
}

/* info? <from@bot>   -> send priv */
void bot_infoq(int idx, char *par)
{
#ifdef LEAF
  char s[200];
#endif
  char s2[32];
  struct chanset_t *chan;
  time_t now2;
  int hr,
    min;

  Context;
  chan = chanset;
  now2 = now - online_since;
  s2[0] = 0;
  if (now2 > 86400) {
    int days = now2 / 86400;

    /* days */
    sprintf(s2, STR("%d day"), days);
    if (days >= 2)
      strcat(s2, "s");
    strcat(s2, ", ");
    now2 -= days * 86400;
  }
  hr = (time_t) ((int) now2 / 3600);
  now2 -= (hr * 3600);
  min = (time_t) ((int) now2 / 60);
  sprintf(&s2[strlen(s2)], STR("%02d:%02d"), (int) hr, (int) min);
#ifdef LEAF
  s[0] = 0;
  while (chan != NULL) {
    if ((strlen(s) + strlen(chan->name) + strlen(network)
	 + strlen(botnetnick) + strlen(ver) + 1) >= 200) {
      strcat(s, STR("++  "));
      break;			/* yegads..! */
    }
    strcat(s, chan->name);
    strcat(s, ", ");
    chan = chan->next;
  }
  if (s[0]) {
    s[strlen(s) - 2] = 0;
    botnet_send_priv(idx, botnetnick, par, NULL, STR("%s <%s> (%s) [UP %s]"), ver, network, s, s2);
  } else
    botnet_send_priv(idx, botnetnick, par, NULL, STR("%s <%s> (no channels) [UP %s]"), ver, network, s2);
#else
  botnet_send_priv(idx, botnetnick, par, NULL, STR("%s <NO_IRC> [UP %s]"), ver, s2);
#endif
  botnet_send_infoq(idx, par);
}

void bot_ping(int idx, char *par)
{
  Context;
  botnet_send_pong(idx);
}

void bot_pong(int idx, char *par)
{
  Context;
  dcc[idx].status &= ~STAT_PINGED;
  if (dcc[idx].pingtime > (now - 120))
    dcc[idx].pingtime -= now;
  else
    dcc[idx].pingtime = 120;
}

void bot_limitcheck(int idx, char *par)
{
#ifdef LEAF
  struct chanset_t * chan;
  newsplit(&par);
  chan=findchan(par);
  if (chan) {
    raise_limit(chan);
  }
#else
  char * bot;
  int i, l;
  bot=newsplit(&par);
  i=nextbot(bot);
  if (i>=0) {
    l=simple_sprintf(OBUF, STR("lc %s %s\n"), bot, par);
    tputs(dcc[i].sock, OBUF, l);
  }
#endif
}

/* link <from@bot> <who> <to-whom> */
void bot_link(int idx, char *par)
{
  char *from,
   *bot,
   *rfrom;
  int i;

  Context;
  from = newsplit(&par);
  bot = newsplit(&par);

  if (!strcasecmp(bot, botnetnick)) {
    if ((rfrom = strchr(from, ':')))
      rfrom++;
    else
      rfrom = from;
    log(LCAT_COMMAND, STR("#%s# link %s"), rfrom, par);
    if (botlink(from, -1, par))
      botnet_send_priv(idx, botnetnick, from, NULL, STR("Attempting to link %s ..."), par);
    else
      botnet_send_priv(idx, botnetnick, from, NULL, STR("Can't link there."));
  } else {
    i = nextbot(bot);
    if (i >= 0)
      botnet_send_link(i, from, bot, par);
  }
}

/* unlink <from@bot> <linking-bot> <undesired-bot> <reason> */
void bot_unlink(int idx, char *par)
{
  char *from,
   *bot,
   *rfrom,
   *p,
   *undes;
  int i;

  Context;
  from = newsplit(&par);
  bot = newsplit(&par);
  undes = newsplit(&par);
  if (!strcasecmp(bot, botnetnick)) {
    if ((rfrom = strchr(from, ':')))
      rfrom++;
    else
      rfrom = from;
    log(LCAT_COMMAND, STR("#%s# unlink %s (%s)"), rfrom, undes, par[0] ? par : STR("No reason"));
    i = botunlink(-3, undes, par[0] ? par : NULL);
    if (i == 1) {
      p = strchr(from, '@');
      if (p) {
	/* idx will change after unlink -- get new idx */
	i = nextbot(p + 1);
	if (i >= 0)
	  botnet_send_priv(i, botnetnick, from, NULL, STR("Unlinked from %s."), undes);
      }
    } else if (i == 0) {
      botnet_send_unlinked(-1, undes, "");
      p = strchr(from, '@');
      if (p) {
	/* ditto above, about idx */
	i = nextbot(p + 1);
	if (i >= 0)
	  botnet_send_priv(i, botnetnick, from, NULL, STR("Can't unlink %s."), undes);
      }
    } else {
      p = strchr(from, '@');
      if (p) {
	i = nextbot(p + 1);
	if (i >= 0)
	  botnet_send_priv(i, botnetnick, from, NULL, STR("Can't remotely unlink sharebots."));
      }
    }
  } else {
    i = nextbot(bot);
    if (i >= 0)
      botnet_send_unlink(i, from, bot, undes, par);
  }
}

/* bot next share? */
void bot_update(int idx, char *par)
{
  char *bot,
    x;
  int vnum;

  Context;
  bot = newsplit(&par);
  x = par[0];
  if (x)
    par++;
  vnum = base64_to_int(par);
  if (in_chain(bot))
    updatebot(idx, bot, x, vnum);
}

/* newbot next share? */
void bot_nlinked(int idx, char *par)
{
  char *newbot,
   *next,
   *p,
    s[1024],
    x;
  int bogus = 0,
    i;

#ifdef HUB
  struct userrec *u;
#endif
  Context;
  newbot = newsplit(&par);
  next = newsplit(&par);
  s[0] = 0;
  if (!next[0]) {
    log(LCAT_BOT, STR("Invalid eggnet protocol from %s (zapfing)"), dcc[idx].nick);
    log(LCAT_ERROR, STR("Invalid eggnet protocol from %s (zapfing)"), dcc[idx].nick);

    simple_sprintf(s, STR("Disconnected %s (invalid bot)"), dcc[idx].nick);
    dprintf(idx, STR("error invalid eggnet protocol for 'nlinked'\n"));
  } else if ((in_chain(newbot)) || (!strcasecmp(newbot, botnetnick))) {
    /* loop! */
    log(LCAT_BOT, STR("Loop detected %s (mutual: %s)"), dcc[idx].nick, newbot);
    log(LCAT_WARNING, STR("Loop detected %s (mutual: %s)"), dcc[idx].nick, newbot);
    simple_sprintf(s, STR("Loop (%s): Disconnected %s"), newbot, dcc[idx].nick);
    dprintf(idx, STR("error Loop (%s)\n"), newbot);
  }
  if (!s[0]) {
    for (p = newbot; *p; p++)
      if ((*p < 32) || (*p == 127) || ((p - newbot) >= HANDLEN))
	bogus = 1;
    i = nextbot(next);
    if (i != idx)
      bogus = 1;
  }
  if (bogus) {
    log(LCAT_BOT, STR("Bogus link notice from %s!  (%s -> %s)"), dcc[idx].nick, next, newbot);
    log(LCAT_WARNING, STR("Bogus link notice from %s!  (%s -> %s)"), dcc[idx].nick, next, newbot);
    simple_sprintf(s, STR("Bogus link notice from: %s Disconnected"), dcc[idx].nick);
    dprintf(idx, STR("error Bogus link notice from (%s -> %s)\n"), next, newbot);
  }
  /* beautiful huh? :) */
  if (bot_hublevel(dcc[idx].user) == 999) {
    log(LCAT_BOT, STR("Disconnected leaf %s  (Linked to %s)"), dcc[idx].nick, newbot);
    simple_sprintf(s, STR("Illegal link by leaf %s (to %s): Disconnected"), dcc[idx].nick, newbot);
    dprintf(idx, STR("error You are supposed to be a leaf\n"));
  }
  if (s[0]) {
    log(LCAT_BOT, STR("*** %s\n"), s);
    botnet_send_unlinked(idx, dcc[idx].nick, s);
    dprintf(idx, STR("bye\n"));
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }
  x = par[0];
  if (x)
    par++;
  else
    x = '-';
  i = base64_to_int(par);
  botnet_send_nlinked(idx, newbot, next, x, i);
  if (x == '!') {
    x = '-';
  }
  addbot(newbot, dcc[idx].nick, next, x, i);
  check_tcl_link(newbot, next);
#ifdef HUB
  u = get_user_by_handle(userlist, newbot);
  if (got_first_userfile && !(u && (u->flags & USER_OP))) {
    botnet_send_reject(idx, botnetnick, NULL, newbot, NULL, NULL);
    log(LCAT_BOT, STR("Rejecting %s from %s"), newbot, dcc[idx].nick);
    log(LCAT_WARNING, STR("Rejecting %s from %s"), newbot, dcc[idx].nick);
  }
#endif
}

void bot_unlinked(int idx, char *par)
{
  int i;
  char *bot;

  Context;
  bot = newsplit(&par);
  i = nextbot(bot);
  if ((i >= 0) && (i != idx))	/* bot is NOT downstream along idx, so
				   * BOGUS! */
    fake_alert(idx, STR("direction"), bot);
  else if (i >= 0) {		/* valid bot downstream of idx */
    botnet_send_unlinked(idx, bot, par);
    unvia(idx, findbot(bot));
    rembot(bot);
  }
  /* otherwise it's not even a valid bot, so just ignore! */
}

void bot_trace(int idx, char *par)
{
  char *from,
   *dest;
  int i;

  /* trace <from@bot> <dest> <chain:chain..> */
  Context;
  from = newsplit(&par);
  dest = newsplit(&par);
  simple_sprintf(TBUF, STR("%s:%s"), par, botnetnick);
  botnet_send_traced(idx, from, TBUF);
  if (strcasecmp(dest, botnetnick) && ((i = nextbot(dest)) > 0))
    botnet_send_trace(i, from, dest, par);
}

void bot_traced(int idx, char *par)
{
  char *to,
   *p;
  int i,
    sock;

  /* traced <to@bot> <chain:chain..> */
  Context;
  to = newsplit(&par);
  p = strchr(to, '@');
  if (p == NULL)
    p = to;
  else {
    *p = 0;
    p++;
  }
  if (!strcasecmp(p, botnetnick)) {
    time_t t = 0;
    char *p = par,
     *ss = TBUF;

    splitc(ss, to, ':');
    if (ss[0])
      sock = atoi(ss);
    else
      sock = (-1);
    if (par[0] == ':') {
      t = atoi(par + 1);
      p = strchr(par + 1, ':');
      if (p)
	p++;
      else
	p = par + 1;
    }
    for (i = 0; i < dcc_total; i++)
      if ((dcc[i].type->flags & DCT_CHAT) && (!strcasecmp(dcc[i].nick, to)) && ((sock == (-1)) || (sock == dcc[i].sock))) {
	if (t) {
	  dprintf(i, STR("Trace result -> %s (%lu secs)\n"), p, now - t);
	} else
	  dprintf(i, STR("Trace result -> %s\n"), p);
      }
  } else {
    i = nextbot(p);
    if (p != to)
      *--p = '@';
    if (i >= 0)
      botnet_send_traced(i, to, par);
  }
}

void bot_timesync(int idx, char *par)
{
  log(LCAT_BOT, STR("Got timesync from %s: %s\n"), dcc[idx].nick, par);
  timesync = atoi(par) - now;
#ifdef HUB
  send_timesync(-1);
#endif
}

/* reject <from> <bot> */
void bot_reject(int idx, char *par)
{
  char *from,
   *who,
   *destbot,
   *frombot;
  struct userrec *u;
  int i;

  Context;
  from = newsplit(&par);
  frombot = strchr(from, '@');
  if (frombot)
    frombot++;
  else
    frombot = from;
  i = nextbot(frombot);
  if (i != idx) {
    fake_alert(idx, STR("direction"), frombot);
    return;
  }
  who = newsplit(&par);
  if (!(destbot = strchr(who, '@'))) {
    /* rejecting a bot */
    i = nextbot(who);
    if (i < 0) {
      botnet_send_priv(idx, botnetnick, from, NULL, STR("Can't unlink %s (doesnt exist)"), who);
    } else if (!strcasecmp(dcc[i].nick, who)) {
      char s[1024];

      /* i'm the connection to the rejected bot */
      log(LCAT_BOT, STR("%s rejected %s"), from, dcc[i].nick);
      dprintf(i, STR("bye\n"));
      simple_sprintf(s, STR("Disconnected %s (%s: %s)"), dcc[i].nick, from, par[0] ? par : STR("Rejected"));

      log(LCAT_BOT, STR("*** %s\n"), s);
      botnet_send_unlinked(i, dcc[i].nick, s);
      killsock(dcc[i].sock);
      lostdcc(i);
    } else {
      if (i >= 0)
	botnet_send_reject(i, from, NULL, who, NULL, par);
    }
  } else {			/* rejecting user */
    *destbot++ = 0;
    if (!strcasecmp(destbot, botnetnick)) {
      /* kick someone here! */
      int ok = 0;

      if (remote_boots == 1) {
	frombot = strchr(from, '@');
	if (frombot == NULL)
	  frombot = from;
	else
	  frombot++;
	u = get_user_by_handle(userlist, frombot);
      } else if (remote_boots == 0) {
	botnet_send_priv(idx, botnetnick, from, NULL, STR("Remote boots are not allowed"));
	ok = 1;
      }
      for (i = 0; (i < dcc_total) && (!ok); i++)
	if ((!strcasecmp(who, dcc[i].nick)) && (dcc[i].type->flags & DCT_CHAT)) {
	  u = get_user_by_handle(userlist, dcc[i].nick);
	  if (u && (u->flags & USER_OWNER)) {
	    return;
	  }
	  do_boot(i, from, par);
	  ok = 1;
	  log(LCAT_COMMAND, STR("#%s# boot %s (%s)"), from, dcc[i].nick, par);
	}
    } else {
      i = nextbot(destbot);
      *--destbot = '@';
      if (i >= 0)
	botnet_send_reject(i, from, NULL, who, NULL, par);
    }
  }
}

void bot_thisbot(int idx, char *par)
{
  Context;
  if (strcasecmp(par, dcc[idx].nick) != 0) {
    char s[1024];

    log(LCAT_BOT, STR("Wrong bot--wanted %s"), dcc[idx].nick, par);
    log(LCAT_WARNING, STR("Wrong bot--wanted %s"), dcc[idx].nick, par);
    dprintf(idx, STR("bye\n"));
    simple_sprintf(s, STR("Disconnect %s (imposter)"), dcc[idx].nick);
    botnet_send_unlinked(idx, dcc[idx].nick, s);
    unvia(idx, findbot(dcc[idx].nick));
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }
  if (bot_hublevel(dcc[idx].user) == 999)
    dcc[idx].status |= STAT_LEAF;
  /* set capitalization the way they want it */
  noshare = 1;
  change_handle(dcc[idx].user, par);
  noshare = 0;
  strcpy(dcc[idx].nick, par);
}

void bot_handshake(int idx, char *par)
{
  struct userrec *u = get_user_by_handle(userlist, dcc[idx].nick);

  /* only set a new password if no old one exists */
  Context;
  /* if (u_pass_match(u, "-")) { */
  noshare = 1;			/* we *don't* want botnet passwords
				 * migrating */
  set_user(&USERENTRY_PASS, u, par);
  noshare = 0;
  /* } */
}

/* used to send a direct msg from Tcl on one bot to Tcl on another 
 * zapf <frombot> <tobot> <code [param]>   */
void bot_zapf(int idx, char *par)
{
  char *from,
   *to;
  int i;

  Context;
  from = newsplit(&par);
  to = newsplit(&par);
  i = nextbot(from);
  if (i != idx) {
    fake_alert(idx, STR("direction"), from);
    return;
  }
  if (!strcasecmp(to, botnetnick)) {
    /* for me! */
    char *opcode;

    opcode = newsplit(&par);
    check_tcl_bot(from, opcode, par);
    return;
  }
  i = nextbot(to);
  if (i >= 0)
    botnet_send_zapf(i, from, to, par);
}

/* used to send a global msg from Tcl on one bot to every other bot 
 * zapf-broad <frombot> <code [param]> */
void bot_zapfbroad(int idx, char *par)
{
  char *from,
   *opcode;
  int i;

  Context;
  from = newsplit(&par);
  opcode = newsplit(&par);

  i = nextbot(from);
  if (i != idx) {
    fake_alert(idx, STR("direction"), from);
    return;
  }
  check_tcl_bot(from, opcode, par);
  botnet_send_zapf_broad(idx, from, opcode, par);
}

/* these are still here, so that they will pass the relevant 
 * requests through even if no filesys is loaded */

/* filereject <bot:filepath> <sock:nick@bot> <reason...> */
void bot_filereject(int idx, char *par)
{
  char *path,
   *to,
   *tobot,
   *p;
  int i;

  Context;
  path = newsplit(&par);
  to = newsplit(&par);
  if ((tobot = strchr(to, '@')))
    tobot++;
  else
    tobot = to;			/* bot wants a file?! :) */
  if (strcasecmp(tobot, botnetnick)) {	/* for me! */
    p = strchr(to, ':');
    if (p != NULL) {
      *p = 0;
      for (i = 0; i < dcc_total; i++) {
	if (dcc[i].sock == atoi(to))
	  dprintf(i, STR("FILE TRANSFER REJECTED (%s): %s\n"), path, par);
      }
      *p = ':';
    }
    /* no ':'? malformed */
    log(LCAT_BOT, STR("%s rejected: %s"), path, par);
  } else {			/* pass it on */
    i = nextbot(tobot);
    if (i >= 0)
      botnet_send_filereject(i, path, to, par);
  }
}

void bot_error(int idx, char *par)
{
  Context;
  log(LCAT_BOT, STR("%s: %s"), dcc[idx].nick, par);
  log(LCAT_ERROR, STR("%s: %s"), dcc[idx].nick, par);
}

/* nc <bot> <sock> <newnick> */
void bot_nickchange(int idx, char *par)
{
  char *bot,
   *ssock,
   *newnick;
  int sock,
    i;

  Context;
  bot = newsplit(&par);
  i = nextbot(bot);
  if (i != idx) {
    fake_alert(idx, STR("direction"), bot);
    return;
  }
  ssock = newsplit(&par);
  sock = base64_to_int(ssock);
  newnick = newsplit(&par);
  i = partynick(bot, sock, newnick);
  if (i < 0) {
    fake_alert(idx, STR("sock#"), ssock);
    return;
  }
  chanout_but(-1, party[i].chan, STR("*** (%s) Nick change: %s -> %s\n"), bot, newnick, party[i].nick);
  botnet_send_nkch_part(idx, i, newnick);
}

/* join <bot> <nick> <chan> <flag><sock> <from> */
void bot_join(int idx, char *par)
{
  char *bot,
   *nick,
   *x,
   *y;
  struct userrec *u;
  int i,
    sock,
    chan,
    i2,
    linking = 0;

  Context;
  bot = newsplit(&par);
  if (bot[0] == '!') {
    linking = 1;
    bot++;
  }
  if (b_status(idx) & STAT_LINKING) {
    linking = 1;
  }
  nick = newsplit(&par);
  x = newsplit(&par);
  chan = base64_to_int(x);
  y = newsplit(&par);
  if ((chan < 0) || !y[0])
    return;			/* woops! pre 1.2.1's send .chat off'ers
				 * too!! */
  if (!y[0]) {
    y[0] = '-';
    sock = 0;
  } else {
    sock = base64_to_int(y + 1);
  }
  /* 1.1 bots always send a sock#, even on a channel change
   * so if sock# is 0, this is from an old bot and we must tread softly
   * grab old sock# if there is one, otherwise make up one */
  if (sock == 0)
    sock = partysock(bot, nick);
  if (sock == 0)
    sock = fakesock++;
  i = nextbot(bot);
  if (i != idx) {		/* ok, garbage from a 1.0g (who uses that
				 * now?) OR raistlin being evil :) */
    fake_alert(idx, STR("direction"), bot);
    return;
  }
  u = get_user_by_handle(userlist, nick);
  if (u) {
    sprintf(TBUF, STR("@%s"), bot);
    touch_laston(u, TBUF, now);
  }
  i = addparty(bot, nick, chan, y[0], sock, par, &i2);
  Context;
  botnet_send_join_party(idx, linking, i2, i);
  Context;
  if (i != chan) {
    if (i >= 0) {
      if (b_numver(idx) >= NEAT_BOTNET)
	chanout_but(-1, i, STR("*** (%s) %s has left the %s.\n"), bot, nick, i ? STR("channel") : STR("party line"));
      check_tcl_chpt(bot, nick, sock, i);
    }
    if ((b_numver(idx) >= NEAT_BOTNET) && !linking)
      chanout_but(-1, chan, STR("*** (%s) %s joined the %s.\n"), bot, nick, chan ? STR("channel") : STR("party line"));
    check_tcl_chjn(bot, nick, chan, y[0], sock, par);
  }
  Context;
}

/* part <bot> <nick> <sock> [etc..] */
void bot_part(int idx, char *par)
{
  char *bot,
   *nick,
   *etc;
  struct userrec *u;
  int sock,
    partyidx;
  int silent = 0;

  Context;
  bot = newsplit(&par);
  if (bot[0] == '!') {
    silent = 1;
    bot++;
  }
  nick = newsplit(&par);
  etc = newsplit(&par);
  sock = base64_to_int(etc);
  if (sock == 0)
    sock = partysock(bot, nick);
  u = get_user_by_handle(userlist, nick);
  if (u) {
    sprintf(TBUF, STR("@%s"), bot);
    touch_laston(u, TBUF, now);
  }
  if ((partyidx = getparty(bot, sock)) != -1) {
    if (party[partyidx].chan >= 0)
      check_tcl_chpt(bot, nick, sock, party[partyidx].chan);
    if ((b_numver(idx) >= NEAT_BOTNET) && !silent) {
      register int chan = party[partyidx].chan;

      if (par[0])
	chanout_but(-1, chan, STR("*** (%s) %s left the %s (%s).\n"), bot, nick, chan ? STR("channel") : STR("party line"), par);
      else
	chanout_but(-1, chan, STR("*** (%s) %s left the %s.\n"), bot, nick, chan ? STR("channel") : STR("party line"));
    }
    botnet_send_part_party(idx, partyidx, par, silent);
    remparty(bot, sock);
  }
  Context;
}

/* away <bot> <sock> <message> */

/* null message = unaway */
void bot_away(int idx, char *par)
{
  char *bot,
   *etc;
  int sock,
    partyidx,
    linking = 0;

  Context;
  bot = newsplit(&par);
  if (bot[0] == '!') {
    linking = 1;
    bot++;
  }
  if (b_status(idx) & STAT_LINKING) {
    linking = 1;
  }
  etc = newsplit(&par);
  sock = base64_to_int(etc);
  if (sock == 0)
    sock = partysock(bot, etc);
  check_tcl_away(bot, sock, par);
  if (par[0]) {
    partystat(bot, sock, PLSTAT_AWAY, 0);
    partyaway(bot, sock, par);
  } else {
    partystat(bot, sock, 0, PLSTAT_AWAY);
  }
  partyidx = getparty(bot, sock);
  if ((b_numver(idx) >= NEAT_BOTNET) && !linking) {
    if (par[0])
      chanout_but(-1, party[partyidx].chan, STR("*** (%s) %s is now away: %s.\n"), bot, party[partyidx].nick, par);
    else
      chanout_but(-1, party[partyidx].chan, STR("*** (%s) %s is no longer away.\n"), bot, party[partyidx].nick);
  }
  botnet_send_away(idx, bot, sock, par, linking);
}

/* (a courtesy info to help during connect bursts) */

/* idle <bot> <sock> <#secs> [away msg] */
void bot_idle(int idx, char *par)
{
  char *bot,
   *work;
  int sock,
    idle;

  Context;
  bot = newsplit(&par);
  work = newsplit(&par);
  sock = base64_to_int(work);
  if (sock == 0)
    sock = partysock(bot, work);
  work = newsplit(&par);
  idle = base64_to_int(work);
  partysetidle(bot, sock, idle);
  if (par[0]) {
    partystat(bot, sock, PLSTAT_AWAY, 0);
    partyaway(bot, sock, par);
  }
  botnet_send_idle(idx, bot, sock, idle, par);
}

void bot_share(int idx, char *par)
{
  Context;
  sharein(idx, par);
}

/* v <frombot> <tobot> <idx:nick> */
void bot_versions(int sock, char *par)
{
  char *frombot = newsplit(&par),
   *tobot,
   *from;

  if (nextbot(frombot) != sock)
    fake_alert(sock, STR("versions-direction"), frombot);
  else if (strcasecmp(tobot = newsplit(&par), botnetnick)) {
    if ((sock = nextbot(tobot)) >= 0)
      dprintf(sock, STR("v %s %s %s\n"), frombot, tobot, par);
  } else {
    from = newsplit(&par);
  }
}

/* BOT COMMANDS */

/* function call should be:
 * int bot_whatever(idx, parameters);
 * 
 * SORT these, dcc_bot uses a shortcut which requires them sorted
 * 
 * yup, those are tokens there to allow a more efficient botnet as
 * time goes on (death to slowly upgrading llama's)
 */
botcmd_t C_bot[] = {
  {"a", (Function) bot_actchan}
  ,
  {"aw", (Function) bot_away}
  ,
  {"bye", (Function) bot_bye}
  ,
  {"c", (Function) bot_chan2}
  ,
  {"cg", (Function) bot_config}
  ,
#ifdef G_DCCPASS
  {"cp", (Function) bot_cmdpass}
  ,
#endif
  {"ct", (Function) bot_chat}
  ,
  {"e", (Function) bot_error}
  ,
  {"el", (Function) bot_endlink}
  ,
  {"f!", (Function) bot_filereject}
  ,
  {"h", (Function) bot_handshake}
  ,
  {"i", (Function) bot_idle}
  ,
  {"i?", (Function) bot_infoq}
  ,
  {"j", (Function) bot_join}
  ,
  {"l", (Function) bot_link}
  ,
  {"lc", (Function) bot_limitcheck}
  ,
  {"lg", (Function) bot_log}
  ,
  {"lgc", (Function) bot_logc}
  ,
  {"lst", (Function) bot_logsettings}
  ,
  {"n", (Function) bot_nlinked}
  ,
  {"nc", (Function) bot_nickchange}
  ,
  {"p", (Function) bot_priv}
  ,
  {"pi", (Function) bot_ping}
  ,
  {"po", (Function) bot_pong}
  ,
  {"pt", (Function) bot_part}
  ,
  {"r", (Function) bot_reject}
  ,
  {"rc", (Function) bot_remotecmd}
  ,
  {"rr", (Function) bot_remotereply}
  ,
  {"s", (Function) bot_share}
  ,
  {"t", (Function) bot_trace}
  ,
  {"tb", (Function) bot_thisbot}
  ,
  {"td", (Function) bot_traced}
  ,
  {"ts", (Function) bot_timesync}
  ,
  {"u", (Function) bot_update}
  ,
  {"ul", (Function) bot_unlink}
  ,
  {"un", (Function) bot_unlinked}
  ,
  {"v", (Function) bot_versions}
  ,
  {"w", (Function) bot_who}
  ,
  {"z", (Function) bot_zapf}
  ,
  {"zb", (Function) bot_zapfbroad}
  ,
  {0, 0}
};
