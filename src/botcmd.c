/*
 * botcmd.c -- handles:
 *   commands that comes across the botnet
 *   userfile transfer and update commands from sharebots
 *
 */

#include "common.h"
#include "botcmd.h"
#include "main.h"
#include "src/mod/share.mod/share.h"
#include "src/mod/update.mod/update.h"
#include "net.h"
#include "tclhash.h"
#include "misc.h"
#include "dcc.h"
#include "userrec.h"
#include "cfg.h"
#include "dccutil.h"
#include "cmds.h"
#include "chanprog.h"
#include "botmsg.h"
#include "botnet.h"
#include "tandem.h"
#include "users.h"
#include "chan.h"
#include "core_binds.h"

int remote_boots = 2;

static char TBUF[1024] = "";		/* Static buffer for goofy bot stuff */

static char base64to[256] =
{
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0, 0,
  0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
  15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 62, 0, 63, 0, 0, 0, 26, 27, 28,
  29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
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

/* Used for 1.0 compatibility: if a join message arrives with no sock#,
 * i'll just grab the next "fakesock" # (incrementing to assure uniqueness)
 */
static int fakesock = 2300;

static void fake_alert(int idx, char *item, char *extra)
{
  static unsigned long lastfake;	/* The last time fake_alert was used */

  if (now - lastfake > 10) {	
    /* Don't fake_alert more than once every 10secs */
    dprintf(idx, STR("ct %s NOTICE: %s (%s != %s).\n"), conf.bot->nick, NET_FAKEREJECT, item, extra);
    putlog(LOG_BOTS, "*", "%s %s (%s != %s).", dcc[idx].nick, NET_FAKEREJECT, item, extra);
    lastfake = now;
  }
}

/* chan <from> <chan> <text>
 */
static void bot_chan2(int idx, char *msg)
{
  char *from = NULL, *p = NULL;
  int i, chan;

  if (bot_flags(dcc[idx].user) & BOT_ISOLATE)
    return;
  from = newsplit(&msg);
  p = newsplit(&msg);
  chan = base64_to_int(p);
  /* Strip annoying control chars */
  for (p = from; *p;) {
    if ((*p < 32) || (*p == 127))
/* FIXME: overlap      strcpy(p, p + 1); */
      sprintf(p, "%s", p + 1);
    else
      p++;
  }
  p = strchr(from, '@');
  if (p) {
    sprintf(TBUF, "<%s> %s", from, msg);
    *p = 0;
    if (!partyidle(p + 1, from)) {
      *p = '@';
      fake_alert(idx, "user", from);
      return;
    }
    *p = '@';
    p++;
  } else {
    sprintf(TBUF, "*** (%s) %s", from, msg);
    p = from;
  }
  i = nextbot(p);
  if (i != idx) {
    fake_alert(idx, "direction", p);
  } else {
    chanout_but(-1, chan, "%s\n", TBUF);
    /* Send to new version bots */
    if (i >= 0)
      botnet_send_chan(idx, from, NULL, chan, msg);
    if (strchr(from, '@') != NULL)
      check_bind_chat(from, chan, msg);
    else
      check_bind_bcst(from, chan, msg);
  }
}

#ifdef S_DCCPASS
void bot_cmdpass(int idx, char *par)
{
  char *p = NULL;

  p = strchr(par, ' ');
  if (p) {
    *p++ = 0;
    botnet_send_cmdpass(idx, par, p);
    p--;
    *p = ' ';
  } else {
    botnet_send_cmdpass(idx, par, "");
  }
  set_cmd_pass(par, 0);
}
#endif /* S_DCCPASS */

void bot_config(int idx, char *par)
{
  got_config_share(idx, par, 0);
}

void bot_configbroad(int idx, char *par)
{
  got_config_share(idx, par, 1);
}

void bot_remotecmd(int idx, char *par) {
  char *tbot = NULL, *fbot = NULL, *fhnd = NULL, *fidx = NULL;
 
  if (par[0])
    tbot = newsplit(&par);
  if (par[0])
    fbot = newsplit(&par);
  if (par[0])
    fhnd = newsplit(&par);
  if (par[0])
    fidx = newsplit(&par);

  if (!strcmp(tbot, conf.bot->nick)) {
    gotremotecmd(tbot, fbot, fhnd, fidx, par);
  } else if (!strcmp(tbot, "*")) {
    botnet_send_cmd_broad(idx, fbot, fhnd, atoi(fidx), par);
    gotremotecmd(tbot, fbot, fhnd, fidx, par);
  } else {
    if (nextbot(tbot) != idx)
      botnet_send_cmd(fbot, tbot, fhnd, atoi(fidx), par);
  }
}

void bot_remotereply(int idx, char *par) {
  char *tbot = NULL, *fbot = NULL, *fhnd = NULL, *fidx = NULL;

  if (par[0])
    tbot = newsplit(&par);
  if (par[0])
    fbot = newsplit(&par);
  if (par[0])
    fhnd = newsplit(&par);
  if (par[0])
    fidx = newsplit(&par);

  if (!strcmp(tbot, conf.bot->nick)) {
    gotremotereply(fbot, fhnd, fidx, par);
  } else {
    if (nextbot(tbot)!= idx)
      botnet_send_cmdreply(fbot, tbot, fhnd, fidx, par);
  }
}

/* chat <from> <notice>  -- only from bots
 */
static void bot_chat(int idx, char *par)
{
  char *from = NULL;
  int i;

  if (bot_flags(dcc[idx].user) & BOT_ISOLATE)
    return;
  from = newsplit(&par);
  if (strchr(from, '@') != NULL) {
    fake_alert(idx, "bot", from);
    return;
  }
  /* Make sure the bot is valid */
  i = nextbot(from);
  if (i != idx) {
    fake_alert(idx, "direction", from);
    return;
  }
  chatout("*** (%s) %s\n", from, par);
  botnet_send_chat(idx, from, par);
}

/* actchan <from> <chan> <text>
 */
static void bot_actchan(int idx, char *par)
{
  char *from = NULL, *p = NULL;
  int i, chan;

  if (bot_flags(dcc[idx].user) & BOT_ISOLATE)
    return;
  from = newsplit(&par);
  p = strchr(from, '@');
  if (p == NULL) {
    /* How can a bot do an action? */
    fake_alert(idx, "user@bot", from);
    return;
  }
  *p = 0;
  if (!partyidle(p + 1, from)) {
    *p = '@';
    fake_alert(idx, "user", from);
    return;
  }
  *p = '@';
  p++;
  i = nextbot(p);
  if (i != idx) {
    fake_alert(idx, "direction", p);
    return;
  }
  p = newsplit(&par);
  chan = base64_to_int(p);
  for (p = from; *p;) {
    if ((*p < 32) || (*p == 127))
      sprintf(p, "%s", p + 1);
/* FIXME: overlap      strcpy(p, p + 1); */
    else
      p++;
  }
  chanout_but(-1, chan, "* %s %s\n", from, par);
  botnet_send_act(idx, from, NULL, chan, par);
  check_bind_act(from, chan, par);
}

/* priv <from> <to> <message>
 */
static void bot_priv(int idx, char *par)
{
  char *from = NULL, *p = NULL, *to = TBUF, *tobot = NULL;
  int i;

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
    fake_alert(idx, "direction", p);
    return;
  }
  if (!to[0])
    return;			/* Silently ignore notes to '@bot' this
				 * is legacy code */
  if (!egg_strcasecmp(tobot, conf.bot->nick)) {		/* For me! */
    if (p == from)
      add_note(to, from, par, -2, 0);
    else {
      i = add_note(to, from, par, -1, 0);
      if (from[0] != '@')
	switch (i) {
	case NOTE_ERROR:
	  botnet_send_priv(idx, conf.bot->nick, from, NULL, "%s %s.", BOT_NOSUCHUSER, to);
	  break;
	case NOTE_STORED:
	  botnet_send_priv(idx, conf.bot->nick, from, NULL, "%s", BOT_NOTESTORED2);
	  break;
	case NOTE_FULL:
	  botnet_send_priv(idx, conf.bot->nick, from, NULL, "%s", BOT_NOTEBOXFULL);
	  break;
	case NOTE_AWAY:
	  botnet_send_priv(idx, conf.bot->nick, from, NULL, "%s %s", to, BOT_NOTEISAWAY);
	  break;
	case NOTE_FWD:
	  botnet_send_priv(idx, conf.bot->nick, from, NULL, "%s %s", "Not online; note forwarded to:", to);
	  break;
	case NOTE_TCL:
	  break;		/* Do nothing */
	case NOTE_OK:
	  botnet_send_priv(idx, conf.bot->nick, from, NULL, "%s %s.", BOT_NOTESENTTO, to);
	  break;
	}
    }
  } else {			/* Pass it on */
    i = nextbot(tobot);
    if (i >= 0)
      botnet_send_priv(i, from, to, tobot, "%s", par);
  }
}

static void bot_bye(int idx, char *par)
{
  char s[1024] = "";
  int users, bots;

  bots = bots_in_subtree(findbot(dcc[idx].nick));
  users = users_in_subtree(findbot(dcc[idx].nick));
  simple_sprintf(s, "%s %s. %s (lost %d bot%s and %d user%s)",
		 BOT_DISCONNECTED, dcc[idx].nick, par[0] ?
		 par : "No reason", bots, (bots != 1) ?
		 "s" : "", users, (users != 1) ? "s" : "");
  putlog(LOG_BOTS, "*", "%s", s);
  chatout("*** %s\n", s);
  botnet_send_unlinked(idx, dcc[idx].nick, s);
  dprintf(idx, "*bye\n");
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void remote_tell_who(int idx, char *nick, int chan)
{
  int i = 10, k, l, ok = 0;
  char s[1024] = "", *realnick = NULL;
  struct chanset_t *c = NULL;

  realnick = strchr(nick, ':');
  if (realnick)
    realnick++;
  else
    realnick = nick;
  putlog(LOG_BOTS, "*", "#%s# who", realnick);
  strcpy(s, "Channels: ");
  for (c = chanset; c; c = c->next)
    if (!channel_secret(c) && shouldjoin(c)) {
      l = strlen(c->dname);
      if (i + l < 1021) {
	if (i > 10) {
          sprintf(s, "%s, %s", s, c->dname);
	} else {
          strcpy(s, c->dname);
	  i += (l + 2);
        }
      }
    }
  if (i > 10) {
    botnet_send_priv(idx, conf.bot->nick, nick, NULL, "%s  (%s)", s, ver);
  } else {
    botnet_send_priv(idx, conf.bot->nick, nick, NULL, "%s  (%s)", BOT_NOCHANNELS, ver);
  }
  if (admin[0])
    botnet_send_priv(idx, conf.bot->nick, nick, NULL, "Admin: %s", admin);
  if (chan == 0) {
    botnet_send_priv(idx, conf.bot->nick, nick, NULL,
		     "%s  (* = %s, + = %s, @ = %s)",
		     BOT_PARTYMEMBS, MISC_OWNER, MISC_MASTER, MISC_OP);
  } else {
      botnet_send_priv(idx, conf.bot->nick, nick, NULL,
		       "%s %s%d:  (* = %s, + = %s, @ = %s)\n",
		       BOT_PEOPLEONCHAN,
		       (chan < GLOBAL_CHANS) ? "" : "*",
		       chan % GLOBAL_CHANS,
		       MISC_OWNER, MISC_MASTER, MISC_OP);
  }
  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type->flags & DCT_REMOTEWHO)
      if (dcc[i].u.chat->channel == chan) {
	k = sprintf(s, "  %c%-15s %s", (geticon(i) == '-' ? ' ' : geticon(i)),
		    dcc[i].nick, dcc[i].host);
	if (now - dcc[i].timeval > 300) {
	  unsigned long days, hrs, mins;

	  days = (now - dcc[i].timeval) / 86400;
	  hrs = ((now - dcc[i].timeval) - (days * 86400)) / 3600;
	  mins = ((now - dcc[i].timeval) - (hrs * 3600)) / 60;
	  if (days > 0)
	    sprintf(s + k, " (%s %lud%luh)", MISC_IDLE, days, hrs);
	  else if (hrs > 0)
	    sprintf(s + k, " (%s %luh%lum)", MISC_IDLE, hrs, mins);
	  else
	    sprintf(s + k, " (%s %lum)", MISC_IDLE, mins);
	}
	botnet_send_priv(idx, conf.bot->nick, nick, NULL, "%s", s);
	if (dcc[i].u.chat->away != NULL)
	  botnet_send_priv(idx, conf.bot->nick, nick, NULL, "      %s: %s", MISC_AWAY, dcc[i].u.chat->away);
      }
  }
  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type == &DCC_BOT) {
      if (!ok) {
	ok = 1;
	botnet_send_priv(idx, conf.bot->nick, nick, NULL, "%s:", BOT_BOTSCONNECTED);
      }
      sprintf(s, "  %s%c%-15s %s",
	      dcc[i].status & STAT_CALLED ? "<-" : "->",
	      dcc[i].status & STAT_SHARE ? '+' : ' ',
	      dcc[i].nick, dcc[i].u.bot->version);
      botnet_send_priv(idx, conf.bot->nick, nick, NULL, "%s", s);
    }
  }
  ok = 0;
  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type->flags & DCT_REMOTEWHO) {
      if (dcc[i].u.chat->channel != chan) {
	if (!ok) {
	  ok = 1;
	  botnet_send_priv(idx, conf.bot->nick, nick, NULL, "%s:", BOT_OTHERPEOPLE);
	}
	l = sprintf(s, "  %c%-15s %s", (geticon(i) == '-' ? ' ' : geticon(i)), dcc[i].nick, dcc[i].host);
	if (now - dcc[i].timeval > 300) {
	  k = (now - dcc[i].timeval) / 60;
	  if (k < 60)
	    sprintf(s + l, " (%s %dm)", MISC_IDLE, k);
	  else
	    sprintf(s + l, " (%s %dh%dm)", MISC_IDLE, k / 60, k % 60);
	}
	botnet_send_priv(idx, conf.bot->nick, nick, NULL, "%s", s);
	if (dcc[i].u.chat->away != NULL)
	  botnet_send_priv(idx, conf.bot->nick, nick, NULL, "      %s: %s", MISC_AWAY, dcc[i].u.chat->away);
      }
    }
  }
}

static void bot_sysname(int idx, char *par)
{
  dcc[idx].u.bot->sysname[0] = 0;
  strcpy(dcc[idx].u.bot->sysname, par);
}


/* who <from@bot> <tobot> <chan#>
 */
static void bot_who(int idx, char *par)
{
  char *from = NULL, *to = NULL, *p = NULL;
  int i, chan;

  from = newsplit(&par);
  p = strchr(from, '@');
  if (!p) {
    sprintf(TBUF, "%s@%s", from, dcc[idx].nick);
    from = TBUF;
  }
  to = newsplit(&par);
  if (!egg_strcasecmp(to, conf.bot->nick))
    to[0] = 0;			/* (for me) */
  chan = base64_to_int(par);
  if (to[0]) {			/* Pass it on */
    i = nextbot(to);
    if (i >= 0)
      botnet_send_who(i, from, to, chan);
  } else {
    remote_tell_who(idx, from, chan);
  }
}

static void bot_endlink(int idx, char *par)
{
  dcc[idx].status &= ~STAT_LINKING;
}

/* info? <from@bot>   -> send priv
 */
static void bot_infoq(int idx, char *par)
{
#ifdef LEAF
  char s[200] = "";
  struct chanset_t *chan = NULL;
#endif /* LEAF */
  char s2[32] = "", *realnick = NULL;
  time_t now2;
  int hr, min;

  /* Strip the idx from user@bot */
  realnick = strchr(par, ':');
  if (realnick)
    realnick++;
  else
    realnick = par;
  putlog(LOG_BOTS, "@", "#%s# botinfo", realnick);

  now2 = now - online_since;
  s2[0] = 0;
  if (now2 > 86400) {
    int days = now2 / 86400;

    /* Days */
    sprintf(s2, "%d day", days);
    if (days >= 2)
      strcat(s2, "s");
    strcat(s2, ", ");
    now2 -= days * 86400;
  }
  hr = (time_t) ((int) now2 / 3600);
  now2 -= (hr * 3600);
  min = (time_t) ((int) now2 / 60);
  sprintf(&s2[strlen(s2)], "%02d:%02d", (int) hr, (int) min);
#ifdef LEAF
  s[0] = 0;
  for (chan = chanset; chan; chan = chan->next) {
    if (!channel_secret(chan)) {
      if ((strlen(s) + strlen(chan->dname) + strlen(network) + strlen(conf.bot->nick) + strlen(ver) + 1) >= 200) {
        strcat(s,"++  ");
        break; /* Yegads..! */
      }
      strcat(s, chan->dname);
      strcat(s, ", ");
    }
  }
  if (s[0]) {
    s[strlen(s) - 2] = 0;
    botnet_send_priv(idx, conf.bot->nick, par, NULL, "%s <%s> (%s) [UP %s]", ver, network, s, s2);
  } else
    botnet_send_priv(idx, conf.bot->nick, par, NULL, "%s <%s> (%s) [UP %s]", ver, network, BOT_NOCHANNELS, s2);
#else /* !HUB */
  botnet_send_priv(idx, conf.bot->nick, par, NULL, "%s <NO_IRC> [UP %s]", ver, s2);
#endif /* LEAF */
  botnet_send_infoq(idx, par);
}

static void bot_ping(int idx, char *par)
{
  botnet_send_pong(idx);
}

static void bot_pong(int idx, char *par)
{
  dcc[idx].status &= ~STAT_PINGED;
  if (dcc[idx].pingtime > (now - 120))
    dcc[idx].pingtime -= now;
  else
    dcc[idx].pingtime = 120;
}

/* link <from@bot> <who> <to-whom>
 */
static void bot_link(int idx, char *par)
{
  char *from = NULL, *bot = NULL, *rfrom = NULL;
  int i;

  from = newsplit(&par);
  bot = newsplit(&par);

  if (!egg_strcasecmp(bot, conf.bot->nick)) {
    if ((rfrom = strchr(from, ':')))
      rfrom++;
    else
      rfrom = from;
    putlog(LOG_CMDS, "*", "#%s# link %s", rfrom, par);
    if (botlink(from, -1, par))
      botnet_send_priv(idx, conf.bot->nick, from, NULL, "%s %s ...", BOT_LINKATTEMPT, par);
    else
      botnet_send_priv(idx, conf.bot->nick, from, NULL, "%s.", BOT_CANTLINKTHERE);
  } else {
    i = nextbot(bot);
    if (i >= 0)
      botnet_send_link(i, from, bot, par);
  }
}

/* unlink <from@bot> <linking-bot> <undesired-bot> <reason>
 */
static void bot_unlink(int idx, char *par)
{
  char *from = NULL, *bot = NULL, *rfrom = NULL, *p = NULL, *undes = NULL;
  int i;

  from = newsplit(&par);
  bot = newsplit(&par);
  undes = newsplit(&par);
  if (!egg_strcasecmp(bot, conf.bot->nick)) {
    if ((rfrom = strchr(from, ':')))
      rfrom++;
    else
      rfrom = from;
    putlog(LOG_CMDS, "*", "#%s# unlink %s (%s)", rfrom, undes, par[0] ? par : "No reason");
    i = botunlink(-3, undes, par[0] ? par : NULL);
    if (i == 1) {
      p = strchr(from, '@');
      if (p) {
	/* idx will change after unlink -- get new idx
	 *
	 * TODO: This has changed with the new lostdcc() behaviour. Check
	 *       if we can optimise the situation.
	 */
	i = nextbot(p + 1);
	if (i >= 0)
	  botnet_send_priv(i, conf.bot->nick, from, NULL, "Unlinked from %s.", undes);
      }
    } else if (i == 0) {
      botnet_send_unlinked(-1, undes, "");
      p = strchr(from, '@');
      if (p) {
	/* Ditto above, about idx */
	i = nextbot(p + 1);
	if (i >= 0)
	  botnet_send_priv(i, conf.bot->nick, from, NULL, "%s %s.", BOT_CANTUNLINK, undes);
      }
    } else {
      p = strchr(from, '@');
      if (p) {
	i = nextbot(p + 1);
	if (i >= 0)
	  botnet_send_priv(i, conf.bot->nick, from, NULL, "Can't remotely unlink sharebots.");
      }
    }
  } else {
    i = nextbot(bot);
    if (i >= 0)
      botnet_send_unlink(i, from, bot, undes, par);
  }
}

/* Bot next share?
 */
static void bot_update(int idx, char *par)
{
  char *bot = NULL, x;
  int vnum;

  bot = newsplit(&par);
  x = par[0];
  if (x)
    par++;
  vnum = base64_to_int(par);
  if (in_chain(bot))
    updatebot(idx, bot, x, vnum);
}

/* Newbot next share?
 */
static void bot_nlinked(int idx, char *par)
{
  char *newbot = NULL, *next = NULL, *p = NULL, s[1024] = "", x = 0;
  struct userrec *u = NULL;
  int bogus = 0, i;

  newbot = newsplit(&par);
  next = newsplit(&par);
  s[0] = 0;
  if (!next[0]) {
    putlog(LOG_BOTS, "*", "Invalid eggnet protocol from %s (zapfing)", dcc[idx].nick);
    simple_sprintf(s, "%s %s (%s)", MISC_DISCONNECTED, dcc[idx].nick, MISC_INVALIDBOT);
    dprintf(idx, "error invalid eggnet protocol for 'nlinked'\n");
  } else if ((in_chain(newbot)) || (!egg_strcasecmp(newbot, conf.bot->nick))) {
    /* Loop! */
    putlog(LOG_BOTS, "*", "%s %s (mutual: %s)", BOT_LOOPDETECT, dcc[idx].nick, newbot);
    simple_sprintf(s, "%s %s: disconnecting %s", MISC_LOOP, newbot, dcc[idx].nick);
    dprintf(idx, "error Loop (%s)\n", newbot);
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
    putlog(LOG_BOTS, "*", "%s %s!  (%s -> %s)", BOT_BOGUSLINK, dcc[idx].nick, next, newbot);
    simple_sprintf(s, "%s: %s %s", BOT_BOGUSLINK, dcc[idx].nick, MISC_DISCONNECTED);
    dprintf(idx, "error %s (%s -> %s)\n", BOT_BOGUSLINK, next, newbot);
  }
  if (s[0]) {
    chatout("*** %s\n", s);
    botnet_send_unlinked(idx, dcc[idx].nick, s);
    dprintf(idx, "bye %s\n", BOT_ILLEGALLINK);
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
#ifdef HUB
    chatout("*** (%s) %s %s.\n", next, NET_LINKEDTO, newbot);
#else
    chatout("*** %s linked to botnet.\n", newbot);
#endif
    x = '-';
  }
  addbot(newbot, dcc[idx].nick, next, x, i);
  check_bind_link(newbot, next);
  u = get_user_by_handle(userlist, newbot);
  if (bot_flags(u) & BOT_REJECT) {
    botnet_send_reject(idx, conf.bot->nick, NULL, newbot, NULL, NULL);
    putlog(LOG_BOTS, "*", "%s %s %s %s", BOT_REJECTING, newbot, MISC_FROM, dcc[idx].nick);
  }
}

static void bot_unlinked(int idx, char *par)
{
  int i;
  char *bot = NULL;

  bot = newsplit(&par);
  i = nextbot(bot);
  if ((i >= 0) && (i != idx))	/* Bot is NOT downstream along idx, so
				 * BOGUS! */
    fake_alert(idx, "direction", bot);
  else if (i >= 0) {		/* Valid bot downstream of idx */
    if (par[0])
/* #ifdef HUB */
      chatout("*** (%s) %s\n", lastbot(bot), par);
/*
#else
      chatout("*** %s unlinked from botnet.\n", par);
#endif 
*/
    botnet_send_unlinked(idx, bot, par);
    unvia(idx, findbot(bot));
    rembot(bot);
  }
  /* Otherwise it's not even a valid bot, so just ignore! */
}

/* trace <from@bot> <dest> <chain:chain..>
 */
static void bot_trace(int idx, char *par)
{
  char *from = NULL, *dest = NULL;
  int i;

  from = newsplit(&par);
  dest = newsplit(&par);
  simple_sprintf(TBUF, "%s:%s", par, conf.bot->nick);
  botnet_send_traced(idx, from, TBUF);
  if (egg_strcasecmp(dest, conf.bot->nick) && ((i = nextbot(dest)) > 0))
    botnet_send_trace(i, from, dest, par);
}

/* traced <to@bot> <chain:chain..>
 */
static void bot_traced(int idx, char *par)
{
  char *to = NULL, *p = NULL;
  int i, sock;

  to = newsplit(&par);
  p = strchr(to, '@');
  if (p == NULL)
    p = to;
  else {
    *p = 0;
    p++;
  }
  if (!egg_strcasecmp(p, conf.bot->nick)) {
    time_t t = 0;
    char *p = par, *ss = TBUF;

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
      if ((dcc[i].type->flags & DCT_CHAT) &&
	  (!egg_strcasecmp(dcc[i].nick, to)) &&
	  ((sock == (-1)) || (sock == dcc[i].sock))) {
	if (t) {
          int j=0;
          {
            register char *c=p;
            for (; *c != '\0'; c++) if (*c == ':') j++;
          }
         dprintf(i, "%s -> %s (%lu secs, %d hop%s)\n", BOT_TRACERESULT, p,
            now - t, j, (j != 1) ? "s" : "");
	} else
	  dprintf(i, "%s -> %s\n", BOT_TRACERESULT, p);
      }
  } else {
    i = nextbot(p);
    if (p != to)
      *--p = '@';
    if (i >= 0)
      botnet_send_traced(i, to, par);
  }
}

static void bot_buildts(int idx, char *par)
{
  if (par && par[0])
    dcc[idx].u.bot->bts = atoi(par);
}

static void bot_timesync(int idx, char *par)
{
  putlog(LOG_DEBUG, "@", "Got timesync from %s: %s (%li - %li)", dcc[idx].nick, par, atol(par), now);
  timesync = atol(par) - now;

#ifdef HUB
  send_timesync(-1);
#endif /* HUB */
}

/* reject <from> <bot>
 */
static void bot_reject(int idx, char *par)
{
  char *from = NULL, *who = NULL, *destbot = NULL, *frombot = NULL;
  struct userrec *u = NULL;
  int i;

  if (bot_flags(dcc[idx].user) & BOT_ISOLATE)
    return;
  from = newsplit(&par);
  frombot = strchr(from, '@');
  if (frombot)
    frombot++;
  else
    frombot = from;
  i = nextbot(frombot);
  if (i != idx) {
    fake_alert(idx, "direction", frombot);
    return;
  }
  who = newsplit(&par);
  if (!(destbot = strchr(who, '@'))) {
    /* Rejecting a bot */
    i = nextbot(who);
    if (i < 0) {
      botnet_send_priv(idx, conf.bot->nick, from, NULL, "%s %s (%s)",
		       BOT_CANTUNLINK, who, BOT_DOESNTEXIST);
    } else if (!egg_strcasecmp(dcc[i].nick, who)) {
      char s[1024] = "";

      /* I'm the connection to the rejected bot */
      putlog(LOG_BOTS, "*", "%s %s %s", from, MISC_REJECTED, dcc[i].nick);
      dprintf(i, "bye %s\n", par[0] ? par : MISC_REJECTED);
      simple_sprintf(s, "%s %s (%s: %s)",
		     MISC_DISCONNECTED, dcc[i].nick, from,
		     par[0] ? par : MISC_REJECTED);
      chatout("*** %s\n", s);
      botnet_send_unlinked(i, dcc[i].nick, s);
      killsock(dcc[i].sock);
      lostdcc(i);
    } else {
      if (i >= 0)
	botnet_send_reject(i, from, NULL, who, NULL, par);
    }
  } else {			/* Rejecting user */
    *destbot++ = 0;
    if (!egg_strcasecmp(destbot, conf.bot->nick)) {
      /* Kick someone here! */
      int ok = 0;

      if (remote_boots == 1) {
	frombot = strchr(from, '@');
	if (frombot == NULL)
	  frombot = from;
	else
	  frombot++;
	u = get_user_by_handle(userlist, frombot);
	if (!(bot_flags(u) & BOT_SHARE)) {
	  add_note(from, conf.bot->nick, "No non sharebot boots.", -1, 0);
	  ok = 1;
	}
      } else if (remote_boots == 0) {
	botnet_send_priv(idx, conf.bot->nick, from, NULL, "%s",
			 BOT_NOREMOTEBOOT);
	ok = 1;
      }
      for (i = 0; (i < dcc_total) && (!ok); i++)
	if ((!egg_strcasecmp(who, dcc[i].nick)) &&
	    (dcc[i].type->flags & DCT_CHAT)) {
	  u = get_user_by_handle(userlist, dcc[i].nick);
	  if (u && 
              ((u->flags & USER_OWNER) && !(dcc[idx].user->flags & USER_ADMIN))) {
	    add_note(from, conf.bot->nick, BOT_NOOWNERBOOT, -1, 0);
	    return;
	  }
	  do_boot(i, from, par);
	  ok = 1;
	  putlog(LOG_CMDS, "*", "#%s# boot %s (%s)", from, who, 
		 par[0] ? par : "No reason");
	}
    } else {
      i = nextbot(destbot);
      *--destbot = '@';
      if (i >= 0)
	botnet_send_reject(i, from, NULL, who, NULL, par);
    }
  }
}

static void bot_thisbot(int idx, char *par)
{
  if (egg_strcasecmp(par, dcc[idx].nick)) {
    char s[1024] = "";

    putlog(LOG_BOTS, "*", NET_WRONGBOT, dcc[idx].nick, par);
    dprintf(idx, "bye %s\n", MISC_IMPOSTER);
    simple_sprintf(s, "%s %s (%s)", MISC_DISCONNECTED, dcc[idx].nick,
		   MISC_IMPOSTER);
    chatout("*** %s\n", s);
    botnet_send_unlinked(idx, dcc[idx].nick, s);
    unvia(idx, findbot(dcc[idx].nick));
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }
  if (bot_flags(dcc[idx].user) & BOT_LEAF)
    dcc[idx].status |= STAT_LEAF;
  /* Set capitalization the way they want it */
  noshare = 1;
  change_handle(dcc[idx].user, par);
  noshare = 0;
  strcpy(dcc[idx].nick, par);
}

static void bot_hublog(char *botnick, char *code, char *msg)
{
#ifdef HUB
  char *par = NULL, *parp;
  par = parp = strdup(msg);
  if (egg_isdigit(par[0])) {
    int type = atoi(newsplit(&par));

    putlog(type, "@", "(%s) %s", botnick, par);
  } else {
    putlog(LOG_ERRORS, "*", "Malformed HL line from %s: %s %s", botnick, code, par);
  }
  free(parp);
#endif /* HUB */
}

static void bot_handshake(int idx, char *par)
{
  struct userrec *u = get_user_by_handle(userlist, dcc[idx].nick);

  /* We *don't* want botnet passwords migrating */
  noshare = 1;
  set_user(&USERENTRY_PASS, u, par);
  noshare = 0;
}

/* Used to send a direct msg from Tcl on one bot to Tcl on another
 * zapf <frombot> <tobot> <code [param]>
 */
static void bot_zapf(int idx, char *par)
{
  char *from = NULL, *to = NULL;
  int i;

  from = newsplit(&par);
  to = newsplit(&par);
  i = nextbot(from);
  if (i != idx) {
    fake_alert(idx, "direction", from);
    return;
  }
  if (!egg_strcasecmp(to, conf.bot->nick)) {
    /* For me! */
    char *opcode;

    opcode = newsplit(&par);
    check_bind_bot(from, opcode, par);
    return;
  } else {
    i = nextbot(to);
    if (i >= 0)
      botnet_send_zapf(i, from, to, par);
  }
}

/* Used to send a global msg from Tcl on one bot to every other bot
 * zapf-broad <frombot> <code [param]>
 */
static void bot_zapfbroad(int idx, char *par)
{
  char *from = NULL, *opcode = NULL;
  int i;

  from = newsplit(&par);
  opcode = newsplit(&par);

  i = nextbot(from);
  if (i != idx) {
    fake_alert(idx, "direction", from);
    return;
  }
  check_bind_bot(from, opcode, par);
  botnet_send_zapf_broad(idx, from, opcode, par);
}

static void bot_error(int idx, char *par)
{
  putlog(LOG_MISC | LOG_BOTS, "*", "%s: %s", dcc[idx].nick, par);
}

/* nc <bot> <sock> <newnick>
 */
static void bot_nickchange(int idx, char *par)
{
  char *bot = NULL, *ssock = NULL, *newnick = NULL;
  int sock, i;

  if (bot_flags(dcc[idx].user) & BOT_ISOLATE)
    return;
  bot = newsplit(&par);
  i = nextbot(bot);
  if (i != idx) {
    fake_alert(idx, "direction", bot);
    return;
  }
  ssock = newsplit(&par);
  sock = base64_to_int(ssock);
  newnick = newsplit(&par);
  i = partynick(bot, sock, newnick);
  if (i < 0) {
    fake_alert(idx, "sock#", ssock);
    return;
  }
  chanout_but(-1, party[i].chan, "*** (%s) Nick change: %s -> %s\n",
	      bot, newnick, party[i].nick);
  botnet_send_nkch_part(idx, i, newnick);
}

/* join <bot> <nick> <chan> <flag><sock> <from>
 */
static void bot_join(int idx, char *par)
{
  char *bot = NULL, *nick = NULL, *x = NULL, *y = NULL;
  struct userrec *u = NULL;
  int i, sock, chan, i2, linking = 0;

  if (bot_flags(dcc[idx].user) & BOT_ISOLATE)
    return;
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
    return;			/* Woops! pre 1.2.1's send .chat off'ers
				 * too!! */
  if (!y[0]) {
    y[0] = '-';
    sock = 0;
  } else {
    sock = base64_to_int(y + 1);
  }
  /* 1.1 bots always send a sock#, even on a channel change
   * so if sock# is 0, this is from an old bot and we must tread softly
   * grab old sock# if there is one, otherwise make up one.
   */
  if (sock == 0)
    sock = partysock(bot, nick);
  if (sock == 0)
    sock = fakesock++;
  i = nextbot(bot);
  if (i != idx) {		/* Ok, garbage from a 1.0g (who uses that
				 * now?) OR raistlin being evil :) */
    fake_alert(idx, "direction", bot);
    return;
  }
  u = get_user_by_handle(userlist, nick);
  if (u) {
    sprintf(TBUF, "@%s", bot);
    touch_laston(u, TBUF, now);
  }
  i = addparty(bot, nick, chan, y[0], sock, par, &i2);
  botnet_send_join_party(idx, linking, i2, i);
  if (i != chan) {
    if (i >= 0) {
      chanout_but(-1, i, "*** (%s) %s %s %s.\n", bot, nick, NET_LEFTTHE, i ? "channel" : "party line");
      check_bind_chpt(bot, nick, sock, i);
    }
    if (!linking)
    chanout_but(-1, chan, "*** (%s) %s %s %s.\n", bot, nick, NET_JOINEDTHE, chan ? "channel" : "party line");
    check_bind_chjn(bot, nick, chan, y[0], sock, par);
  }
}

/* part <bot> <nick> <sock> [etc..]
 */
static void bot_part(int idx, char *par)
{
  char *bot = NULL, *nick = NULL, *etc = NULL;
  struct userrec *u = NULL;
  int sock, partyidx;
  int silent = 0;

  if (bot_flags(dcc[idx].user) & BOT_ISOLATE)
    return;
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
    sprintf(TBUF, "@%s", bot);
    touch_laston(u, TBUF, now);
  }
  if ((partyidx = getparty(bot, sock)) != -1) {
    if (party[partyidx].chan >= 0)
      check_bind_chpt(bot, nick, sock, party[partyidx].chan);
    if (!silent) {
      register int chan = party[partyidx].chan;

      if (par[0])
	chanout_but(-1, chan, "*** (%s) %s %s %s (%s).\n", bot, nick,
		    NET_LEFTTHE,
		    chan ? "channel" : "party line", par);
      else
	chanout_but(-1, chan, "*** (%s) %s %s %s.\n", bot, nick,
		    NET_LEFTTHE,
		    chan ? "channel" : "party line");
    }
    botnet_send_part_party(idx, partyidx, par, silent);
    remparty(bot, sock);
  }
}

/* away <bot> <sock> <message>
 * null message = unaway
 */
static void bot_away(int idx, char *par)
{
  char *bot = NULL, *etc = NULL;
  int sock, partyidx, linking = 0;

  if (bot_flags(dcc[idx].user) & BOT_ISOLATE)
    return;
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
  check_bind_away(bot, idx, par);
  if (par[0]) {
    partystat(bot, sock, PLSTAT_AWAY, 0);
    partyaway(bot, sock, par);
  } else {
    partystat(bot, sock, 0, PLSTAT_AWAY);
  }
  partyidx = getparty(bot, sock);
  if (!linking) {
    if (par[0])
      chanout_but(-1, party[partyidx].chan,
		  "*** (%s) %s %s: %s.\n", bot,
		  party[partyidx].nick, NET_AWAY, par);
    else
      chanout_but(-1, party[partyidx].chan,
		  "*** (%s) %s %s.\n", bot,
		  party[partyidx].nick, NET_UNAWAY);
  }
  botnet_send_away(idx, bot, sock, par, linking);
}

/* (a courtesy info to help during connect bursts)
 * idle <bot> <sock> <#secs> [away msg]
 */
static void bot_idle(int idx, char *par)
{
  char *bot = NULL, *work = NULL;
  int sock, idle;

  if (bot_flags(dcc[idx].user) & BOT_ISOLATE)
    return;
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
  sharein(idx, par);
}

void bot_shareupdate(int idx, char *par)
{
  updatein(idx, par);
}

/* v <frombot> <tobot> <idx:nick>
 */
static void bot_versions(int sock, char *par)
{
  char *frombot = newsplit(&par), *tobot = NULL, *from = NULL;

  if (nextbot(frombot) != sock)
    fake_alert(sock, "versions-direction", frombot);
  else if (egg_strcasecmp(tobot = newsplit(&par), conf.bot->nick)) {
    if ((sock = nextbot(tobot)) >= 0)
      dprintf(sock, "v %s %s %s\n", frombot, tobot, par);
  } else {
    from = newsplit(&par);
    botnet_send_priv(sock, conf.bot->nick, from, frombot, "Modules loaded:\n");
/* wtf?
    for (me = module_list; me; me = me->next)
      botnet_send_priv(sock, conf.bot->nick, from, frombot, "  Module: %s\n", me->name);
*/
    botnet_send_priv(sock, conf.bot->nick, from, frombot, "End of module list.\n");
  }
}

/* BOT COMMANDS
 *
 * function call should be:
 * int bot_whatever(idx,"parameters");
 *
 * SORT these, dcc_bot uses a shortcut which requires them sorted
 *
 * yup, those are tokens there to allow a more efficient botnet as
 * time goes on (death to slowly upgrading llama's)
 */
botcmd_t C_bot[] =
{
  {"a",			(Function) bot_actchan},
  {"aw",		(Function) bot_away},
  {"away",		(Function) bot_away},
  {"bts",		(Function) bot_buildts},
  {"bye",		(Function) bot_bye},
  {"c",			(Function) bot_chan2},
  {"cg",                (Function) bot_config},
  {"cgb",		(Function) bot_configbroad},
#ifdef S_DCCPASS
  {"cp", 		(Function) bot_cmdpass},
#endif
  {"ct",		(Function) bot_chat},
  {"e",			(Function) bot_error},
  {"el",		(Function) bot_endlink},
  {"hs",		(Function) bot_handshake},
  {"i",			(Function) bot_idle},
  {"i?",		(Function) bot_infoq},
  {"j",			(Function) bot_join},
  {"l",			(Function) bot_link},
  {"n",			(Function) bot_nlinked},
  {"nc",		(Function) bot_nickchange},
  {"p",			(Function) bot_priv},
  {"pi",		(Function) bot_ping},
  {"po",		(Function) bot_pong},
  {"pt",		(Function) bot_part},
  {"r",			(Function) bot_reject},
  {"rc", 		(Function) bot_remotecmd},
  {"rr", 		(Function) bot_remotereply},
  {"s",			(Function) bot_share},
  {"sb",		(Function) bot_shareupdate},
  {"t",			(Function) bot_trace},
  {"tb",		(Function) bot_thisbot},
  {"td",		(Function) bot_traced},
  {"ts", 		(Function) bot_timesync},
  {"u",			(Function) bot_update},
  {"ul",		(Function) bot_unlink},
  {"un",		(Function) bot_unlinked},
  {"v",			(Function) bot_versions},
  {"vs",		(Function) bot_sysname},
  {"w",			(Function) bot_who},
  {"z",			(Function) bot_zapf},
  {"zb",		(Function) bot_zapfbroad},
  {NULL,		NULL}
};

void send_remote_simul(int idx, char *bot, char *cmd, char *par)
{
  char msg[SGRAB - 110] = "";

  egg_snprintf(msg, sizeof msg, "r-s %d %s %d %s %lu %s %s", idx, dcc[idx].nick, dcc[idx].u.chat->con_flags, 
               dcc[idx].u.chat->con_chan, dcc[idx].status, cmd, par);
  putbot(bot, msg);
}

/* idx nick conmask cmd par */
static void bot_rsim(char *botnick, char *code, char *msg)
{
  int ridx = -1, idx = -1, i = 0, rconmask;
  unsigned long status = 0;
  char *nick = NULL, *cmd = NULL, *rconchan = NULL, buf[UHOSTMAX] = "", *par = NULL, *parp = NULL;

  par = parp = strdup(msg);
  ridx = atoi(newsplit(&par));
  nick = newsplit(&par);
  rconmask = atoi(newsplit(&par));
  rconchan = newsplit(&par);
  if (egg_isdigit(par[0]))
    status = (unsigned long) atoi(newsplit(&par));
  cmd = newsplit(&par);
  if (ridx < 0 || !nick || !cmd) {
    free(parp);
    return;
  }

  for (i = 0; i < dcc_total; i++) {
   if (dcc[i].simul == ridx) {
     putlog(LOG_DEBUG, "*", "Simul found old idx for %s: %d (ridx: %d)", nick, i, ridx);
     dcc[i].simultime = now;
     idx = i;
     break;
   }
  }

  if (idx < 0) {
    idx = new_dcc(&DCC_CHAT, sizeof(struct chat_info));
    putlog(LOG_DEBUG, "*", "Making new idx for %s@%s: %d ridx: %d", nick, botnick, idx, ridx);
    dcc[idx].sock = -1;
    dcc[idx].timeval = now;
    dcc[idx].simultime = now;
    dcc[idx].simul = ridx;
    dcc[idx].status = status;
    strcpy(dcc[idx].simulbot, botnick);
    dcc[idx].u.chat->con_flags = rconmask;
    strcpy(dcc[idx].u.chat->con_chan, rconchan);
    dcc[idx].u.chat->strip_flags = STRIP_ALL;
    strcpy(dcc[idx].nick, nick);
    egg_snprintf(buf, sizeof buf, "%s@%s", nick, botnick);
    strcpy(dcc[idx].host, buf);
    dcc[idx].addr = 0L;
    dcc[idx].user = get_user_by_handle(userlist, nick);
  }
  rmspace(par);
  check_bind_dcc(cmd, idx, par);
  free(parp);
}

void bounce_simul(int idx, char *buf)
{
  char rmsg[SGRAB - 110] = "";

  if (!buf || !buf[0] || !dcc[idx].simulbot || !dcc[idx].simulbot[0] || idx < 0)
    return;

  egg_snprintf(rmsg, sizeof rmsg, "r-sr %d %s", dcc[idx].simul, buf);          /* remote-simul[r]eturn idx buf */
  putbot(dcc[idx].simulbot, rmsg);
}

#ifdef HUB
static void bot_rsimr(char *botnick, char *code, char *msg)
{
  if (msg[0]) {
    char *par = strdup(msg), *parp = par, *prefix = NULL;
    int idx = atoi(newsplit(&par));
    size_t size = strlen(botnick) + 4;

    prefix = calloc(1, size);
    egg_snprintf(prefix, size, "[%s] ", botnick);
    dumplots(idx, prefix, par);
    free(prefix);
    free(parp);
  }
}
#endif /* HUB */

static cmd_t my_bot[] = 
{
  {"hl",	"",	(Function) bot_hublog,  NULL},
#ifdef HUB	/* This will only allow hubs to read the return text */
  {"r-sr",	"",	(Function) bot_rsimr,	NULL},
#endif /* HUB */
  {"r-s",	"",	(Function) bot_rsim,	NULL},
  {NULL, 	NULL, 	NULL, 			NULL}
};

void init_botcmd()
{
  add_builtins("bot", my_bot);
}
