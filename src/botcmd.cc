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
 * botcmd.c -- handles:
 *   commands that comes across the botnet
 *   userfile transfer and update commands from sharebots
 *
 */


#include "common.h"
#include "botcmd.h"
#include "main.h"
#include "base64.h"
#include "src/mod/share.mod/share.h"
#include "src/mod/update.mod/update.h"
#include "net.h"
#include "binds.h"
#include "misc.h"
#include "dcc.h"
#include "userrec.h"
#include "dccutil.h"
#include "cmds.h"
#include "chanprog.h"
#include "botmsg.h"
#include "botnet.h"
#include "tandem.h"
#include "users.h"
#include "chan.h"
#include "core_binds.h"
#include "egg_timer.h"
#include "shell.h"

static char TBUF[1024] = "";		/* Static buffer for goofy bot stuff */

static void fake_alert(int idx, char *item, char *extra, char *what)
{
  static time_t lastfake;	/* The last time fake_alert was used */

  if ((now - lastfake) > 10) {	
    /* Don't fake_alert more than once every 10secs */
    dprintf(idx, "ct %s NOTICE: %s (%s != %s). (%s)\n", conf.bot->nick, "Fake message rejected", item, extra, what);
    putlog(LOG_BOTS, "*", "%s %s (%s != %s). (%s)", dcc[idx].nick, "Fake message rejected", item, extra, what);
    lastfake = now;
  }
}

/* chan <from> <chan> <text>
 */
static void bot_chan2(int idx, char *msg)
{
  char *from = NULL, *p = NULL;
  int i, chan;

  from = newsplit(&msg);
  p = newsplit(&msg);
  chan = base64_to_int(p);
  /* Strip annoying control chars */
  for (p = from; *p;) {
    if ((*p < 32) || (*p == 127))
      memmove(p, p + 1, strlen(p));
    else
      p++;
  }
  p = strchr(from, '@');
  if (p) {
    if (!conf.bot->hub) { /* Need to strip out the hub nick */
      char *q = NULL, newfrom[HANDLEN + 9 + 1]; /* HANDLEN@[botnet] */
   
      strlcpy(newfrom, from, sizeof(newfrom));
      q = strchr(newfrom, '@');
      *q = 0;
      simple_snprintf(TBUF, sizeof(TBUF), "<%s@[botnet]> %s", newfrom, msg);
    } else
      simple_snprintf(TBUF, sizeof(TBUF), "<%s> %s", from, msg);
    *p = 0;
    if (!partyidle(p + 1, from)) {
      *p = '@';
      fake_alert(idx, "user", from, "chan2_i");
      return;
    }
    *p = '@';
    p++;
  } else {
    simple_snprintf(TBUF, sizeof(TBUF), "*** (%s) %s", from, msg);
    p = from;
  }
  i = nextbot(p);
  if (i != idx) {
    fake_alert(idx, "direction", p, "chan2_ii");
  } else {
    chanout_but(-1, chan, "%s\n", TBUF);
    /* Send to new version bots */
    if (i >= 0)
      botnet_send_chan(idx, from, NULL, chan, msg);
  }
}

void bot_cmdpass(int idx, char *par)
{
  char *p = strchr(par, ' ');

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

void bot_set(int idx, char *par)
{
  var_userfile_share_line(par, idx, 0);
}

void bot_setbroad(int idx, char *par)
{
  var_userfile_share_line(par, idx, 1);
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

  /* Make sure the bot is valid */
  int i = nextbot(fbot);
  if (i != idx) {
    fake_alert(idx, "direction", fbot, "rc");
    return;
  }

  if (!strcasecmp(tbot, conf.bot->nick)) {
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

  /* Make sure the bot is valid */
  int i = nextbot(fbot);
  if (i != idx) {
    fake_alert(idx, "direction", fbot, "rr");
    return;
  }

  if (!strcasecmp(tbot, conf.bot->nick)) {
    gotremotereply(fbot, fhnd, fidx, par);
  } else {
    if (nextbot(tbot)!= idx)
      botnet_send_cmdreply(fbot, tbot, fhnd, fidx, par);
  }
}

/* chat <from> <notice>  -- only from bots
 */
/* this is only sent to hub bots */
static void bot_chat(int idx, char *par)
{
  char *from = NULL;
  int i;

  from = newsplit(&par);
  if (strchr(from, '@') != NULL) {
    fake_alert(idx, "bot", from, "chat_i");
    return;
  }
  /* Make sure the bot is valid */
  i = nextbot(from);
  if (i != idx) {
    fake_alert(idx, "direction", from, "chat_ii");
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

  from = newsplit(&par);
  p = strchr(from, '@');
  if (p == NULL) {
    /* How can a bot do an action? */
    fake_alert(idx, "user@bot", from, "actchan_i");
    return;
  }
  *p = 0;
  if (!partyidle(p + 1, from)) {
    *p = '@';
    fake_alert(idx, "user", from, "actchan_ii");
    return;
  }
  *p = '@';
  p++;
  i = nextbot(p);
  if (i != idx) {
    fake_alert(idx, "direction", p, "actchan_iii");
    return;
  }
  p = newsplit(&par);
  chan = base64_to_int(p);
  for (p = from; *p;) {
    if ((*p < 32) || (*p == 127))
      memmove(p, p + 1, strlen(p));
    else
      p++;
  }
  if (!conf.bot->hub) { /* Need to strip out the hub nick */
    char *q = NULL, newfrom[HANDLEN + 9 + 1]; /* HANDLEN@[botnet] */
 
    strlcpy(newfrom, from, sizeof(newfrom));
    q = strchr(newfrom, '@');
    *q = 0;
    chanout_but(-1, chan, "* %s@[botnet] %s\n", newfrom, par);
  } else
    chanout_but(-1, chan, "* %s %s\n", from, par);
  botnet_send_act(idx, from, NULL, chan, par);
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
    fake_alert(idx, "direction", p, "priv_i");
    return;
  }
  if (!to[0])
    return;			/* Silently ignore notes to '@bot' this
				 * is legacy code */
  if (!strcasecmp(tobot, conf.bot->nick)) {		/* For me! */
    if (p == from)
      add_note(to, from, par, -2, 0);
    else {
      i = add_note(to, from, par, -1, 0);
      if (from[0] != '@')
	switch (i) {
	case NOTE_ERROR:
	  botnet_send_priv(idx, conf.bot->nick, from, NULL, "%s %s.", "No such user", to);
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
  simple_snprintf(s, sizeof(s), "%s %s. %s (lost %d bot%s and %d user%s)",
		 "Disconnected from:",
                 (conf.bot->hub || (conf.bot->localhub && bot_aggressive_to(dcc[idx].user))) ? dcc[idx].nick : "botnet",
                  par[0] ? par : "No reason", bots, (bots != 1) ?
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
  strlcpy(s, "Channels: ", sizeof(s));
  for (c = chanset; c; c = c->next)
    if (!channel_secret(c) && shouldjoin(c)) {
      l = strlen(c->dname);
      if (i + l < 1021) {
	if (i > 10) {
          strlcat(s, ", ", sizeof(s));
          strlcat(s, c->dname, sizeof(s));
	} else {
          strlcpy(s, c->dname, sizeof(s));
	  i += (l + 2);
        }
      }
    }
  if (i > 10) {
    botnet_send_priv(idx, conf.bot->nick, nick, NULL, "%s  (%s)", s, ver);
  } else {
    botnet_send_priv(idx, conf.bot->nick, nick, NULL, "%s  (%s)", "no channels", ver);
  }
  if (admin[0])
    botnet_send_priv(idx, conf.bot->nick, nick, NULL, "Admin: %s", admin);
  if (chan == 0) {
    botnet_send_priv(idx, conf.bot->nick, nick, NULL,
		     "Party line members:  (^ = admin, * = owner, + = master, @ = op)");
  } else {
      botnet_send_priv(idx, conf.bot->nick, nick, NULL,
		       "People on channel %s%d:  (^ = admin, * = owner, + = master, @ = op)\n",
		       (chan < GLOBAL_CHANS) ? "" : "*",
		       chan % GLOBAL_CHANS);
  }
  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type && dcc[i].type->flags & DCT_REMOTEWHO) {
      if (dcc[i].u.chat->channel == chan) {
	k = simple_snprintf(s, sizeof(s), "  %c%-15s %s", (geticon(i) == '-' ? ' ' : geticon(i)),
		    dcc[i].nick, dcc[i].host);
	if (now - dcc[i].timeval > 300) {
	  unsigned long mydays, hrs, mins;

	  mydays = (now - dcc[i].timeval) / 86400;
	  hrs = ((now - dcc[i].timeval) - (mydays * 86400)) / 3600;
	  mins = ((now - dcc[i].timeval) - (hrs * 3600)) / 60;
	  if (mydays > 0)
	    simple_snprintf(s + k, sizeof(s) - k, " (idle %lud%luh)", mydays, hrs);
	  else if (hrs > 0)
	    simple_snprintf(s + k, sizeof(s) - k, " (idle %luh%lum)", hrs, mins);
	  else
	    simple_snprintf(s + k, sizeof(s) - k, " (idle %lum)", mins);
	}
	botnet_send_priv(idx, conf.bot->nick, nick, NULL, "%s", s);
	if (dcc[i].u.chat->away != NULL)
	  botnet_send_priv(idx, conf.bot->nick, nick, NULL, "      AWAY: %s", dcc[i].u.chat->away);
      }
    }
  }
  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type && dcc[i].type == &DCC_BOT) {
      if (!ok) {
	ok = 1;
	botnet_send_priv(idx, conf.bot->nick, nick, NULL, "%s:", "Bots connected");
      }
      simple_snprintf(s, sizeof(s), "  %s%c%-15s %s",
	      dcc[i].status & STAT_CALLED ? "<-" : "->",
	      dcc[i].status & STAT_SHARE ? '+' : ' ',
	      dcc[i].nick, dcc[i].u.bot->version);
      botnet_send_priv(idx, conf.bot->nick, nick, NULL, "%s", s);
    }
  }
  ok = 0;
  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type && dcc[i].type->flags & DCT_REMOTEWHO) {
      if (dcc[i].u.chat->channel != chan) {
	if (!ok) {
	  ok = 1;
	  botnet_send_priv(idx, conf.bot->nick, nick, NULL, "%s:", "Other people on the bot");
	}
	l = simple_snprintf(s, sizeof(s), "  %c%-15s %s", (geticon(i) == '-' ? ' ' : geticon(i)), dcc[i].nick, dcc[i].host);
	if (now - dcc[i].timeval > 300) {
	  k = (now - dcc[i].timeval) / 60;
	  if (k < 60)
	    simple_snprintf(s + l, sizeof(s) - l, " (idle %dm)", k);
	  else
	    simple_snprintf(s + l, sizeof(s) - l, " (idle %dh%dm)", k / 60, k % 60);
	}
	botnet_send_priv(idx, conf.bot->nick, nick, NULL, "%s", s);
	if (dcc[i].u.chat->away != NULL)
	  botnet_send_priv(idx, conf.bot->nick, nick, NULL, "      AWAY: %s", dcc[i].u.chat->away);
      }
    }
  }
}

static void bot_shellinfo(int idx, char *par)
{
  char *username = NULL, *sysname = NULL, *nodename = NULL, *arch = NULL, *botversion = NULL;
  
  username = newsplit(&par);
  sysname = newsplit(&par);
  nodename = newsplit(&par);
  arch = newsplit(&par);
  botversion = newsplit(&par);

  set_user(&USERENTRY_USERNAME, dcc[idx].user, username);
  set_user(&USERENTRY_OS, dcc[idx].user, sysname);
  dcc[idx].u.bot->sysname[0] = 0;
  struct bot_info dummy;
  strlcpy(dcc[idx].u.bot->sysname, sysname, sizeof(dummy.sysname)); 
  set_user(&USERENTRY_NODENAME, dcc[idx].user, nodename);
  set_user(&USERENTRY_ARCH, dcc[idx].user, arch);
  set_user(&USERENTRY_OSVER, dcc[idx].user, botversion);
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
    simple_snprintf(TBUF, sizeof(TBUF), "%s@%s", from, dcc[idx].nick);
    from = TBUF;
  }
  to = newsplit(&par);
  if (!strcasecmp(to, conf.bot->nick))
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

  if (!strcasecmp(bot, conf.bot->nick)) {
    if ((rfrom = strchr(from, ':')))
      rfrom++;
    else
      rfrom = from;
    putlog(LOG_CMDS, "*", "#%s# link %s", rfrom, par);
    if (botlink(from, -1, par))
      botnet_send_priv(idx, conf.bot->nick, from, NULL, "%s %s ...", "Attempting to link", par);
    else
      botnet_send_priv(idx, conf.bot->nick, from, NULL, "%s.", "Can't link there");
  } else {
    i = nextbot(bot);
    if (i >= 0)
      botnet_send_link(i, from, bot, par);
  }
}

/*
	bot_log
	 - forwards to all hubs linked directly if a hub (excluding the one who sent it)
	 - leaf bots display and do not pass along.
*/
static void bot_log(int idx, char *par)
{
  char *from = newsplit(&par);
  int i = nextbot(from);

  if (i != idx) {
    fake_alert(idx, "direction", from, "log");
    return;
  }

  if (egg_isdigit(par[0])) {
    int type = atoi(newsplit(&par));

    if (conf.bot->hub || conf.bot->localhub)
      botnet_send_log(idx, from, type, par);

    if (conf.bot->hub)
      putlog(type, "@", "(%s) %s", from, par);

  } else {
    putlog(LOG_ERRORS, "*", "Malformed HL line from %s: %s", from, par);
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
  if (!strcasecmp(bot, conf.bot->nick)) {
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
	  botnet_send_priv(i, conf.bot->nick, from, NULL, "%s %s.", "Can't unlink", undes);
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
  char *bot = NULL, x, *vversion = NULL, *vcommit = NULL;
  int vlocalhub = 0, fflags = -1;
  time_t vbuildts = 0L;

  bot = newsplit(&par);
  x = par[0];
  if (x)
    par++;

  newsplit(&par);		/* vnum */

  if (par[0])
    vlocalhub = atoi(newsplit(&par));
  if (par[0])
    vbuildts = atol(newsplit(&par));
  if (par[0])
    vcommit = newsplit(&par);
  if (par[0])
    vversion = newsplit(&par);
  if (par[0])
    fflags = atoi(newsplit(&par));

  if (in_chain(bot))
    updatebot(idx, bot, x, vlocalhub, vbuildts, vcommit, vversion, fflags);
}

/* Newbot next share?
 */
static void bot_nlinked(int idx, char *par)
{
  char *newbot = NULL, *next = NULL, *p = NULL, s[1024] = "", x = 0, *vversion = NULL, *vcommit = NULL;
  int i, vlocalhub = 0, fflags = -1;
  time_t vbuildts = 0L;
  bool bogus = 0;

  newbot = newsplit(&par);
  next = newsplit(&par);
  s[0] = 0;
  if (!next[0]) {
    putlog(LOG_BOTS, "*", "Invalid eggnet protocol from %s (zapfing)", dcc[idx].nick);
    simple_snprintf(s, sizeof(s), "Disconnected %s (invalid bot)", dcc[idx].nick);
    dprintf(idx, "error invalid eggnet protocol for 'nlinked'\n");
  } else if ((in_chain(newbot)) || (!strcasecmp(newbot, conf.bot->nick))) {
    /* Loop! */
    putlog(LOG_BOTS, "*", "Loop detected %s (mutual: %s)", dcc[idx].nick, newbot);
    simple_snprintf(s, sizeof(s), "Detected loop: two bots exist named %s: disconnecting %s", newbot, dcc[idx].nick);
    dprintf(idx, "error Loop (%s)\n", newbot);
  }
  if (!s[0]) {
    for (p = newbot; *p; p++)
      if ((*p < 32) || (*p == 127))
	bogus = 1;
    i = nextbot(next);
    if (i != idx)
      bogus = 1;

    if (bogus) {
      putlog(LOG_BOTS, "*", "Bogus link notice from %s!  (%s -> %s)", dcc[idx].nick, next, newbot);
      simple_snprintf(s, sizeof(s), "Bogus link notice from: %s Disconnected", dcc[idx].nick);
      dprintf(idx, "error Bogus link notice from (%s -> %s)\n", next, newbot);
    }
  }

  if (s[0]) {
    chatout("*** %s\n", s);
    botnet_send_unlinked(idx, dcc[idx].nick, s);
    dprintf(idx, "bye %s\n", s);
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }
  x = par[0];
  if (x)
    par++;
  else
    x = '-';

  if (par[0])
    newsplit(&par);		/* vnum */
  if (par[0])
    vlocalhub = atoi(newsplit(&par));
  if (par[0])
    vbuildts = atol(newsplit(&par));
  if (par[0])
    vcommit = newsplit(&par);
  if (par[0])
    vversion = newsplit(&par);
  if (par[0])
    fflags = atoi(newsplit(&par));
  botnet_send_nlinked(idx, newbot, next, x, vlocalhub, vbuildts, vcommit, vversion, fflags);
  
  if (x == '!') {
    if (conf.bot->hub)
      chatout("*** (%s) %s %s.\n", next, "Linked to", newbot);
    else
      chatout("*** %s linked to botnet.\n", newbot);
    x = '-';
  }
  addbot(newbot, dcc[idx].nick, next, x, vlocalhub, vbuildts, vcommit, vversion ? vversion : (char *) "", fflags);
}

static void bot_unlinked(int idx, char *par)
{
  int i;
  char *bot = NULL;

  bot = newsplit(&par);
  i = nextbot(bot);
  if ((i >= 0) && (i != idx))	/* Bot is NOT downstream along idx, so
				 * BOGUS! */
    fake_alert(idx, "direction", bot, "unlinked");
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
  simple_snprintf(TBUF, sizeof(TBUF), "%s:%s", par, conf.bot->nick);
  botnet_send_traced(idx, from, TBUF);
  if (strcasecmp(dest, conf.bot->nick) && ((i = nextbot(dest)) > 0))
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
  if (!strcasecmp(p, conf.bot->nick)) {
    time_t t = 0;
    char *p2 = par, *ss = TBUF;

    splitc(ss, to, ':');
    if (ss[0])
      sock = atoi(ss);
    else
      sock = (-1);
    if (par[0] == ':') {
      t = atoi(par + 1);
      p2 = strchr(par + 1, ':');
      if (p2)
	p2++;
      else
	p2 = par + 1;
    }
    for (i = 0; i < dcc_total; i++) {
      if (dcc[i].type && (dcc[i].type->flags & DCT_CHAT) && (!strcasecmp(dcc[i].nick, to)) &&
          ((sock == (-1)) || (sock == dcc[i].sock))) {
	if (t) {
          int j=0;
          char *c = p2;
          for (; *c != '\0'; c++) if (*c == ':') j++;

          time_t tm;
          egg_timeval_t tv;

          timer_get_now(&tv);
          tm = ((tv.sec % 10000) * 100 + (tv.usec * 100) / (1000000)) - t;

          dprintf(i, "%s -> %s (%d.%d secs, %d hop%s)\n", "Trace result", p2,
            (int)(tm / 100), (int)(tm % 100), j, (j != 1) ? "s" : "");
	} else
	  dprintf(i, "%s -> %s\n", "Trace result", p);
      }
    }
  } else {
    i = nextbot(p);
    if (p != to)
      *--p = '@';
    if (i >= 0)
      botnet_send_traced(i, to, par);
  }
}

static void bot_timesync(int idx, char *par)
{
//  putlog(LOG_DEBUG, "@", "Got timesync from %s: %s (%li - %li)", dcc[idx].nick, par, atol(par), now);
  timesync = atol(par) - now;

  if (conf.bot->hub || conf.bot->localhub)
    send_timesync(-1);
}

/* reject <from> <bot>
 */
static void bot_reject(int idx, char *par)
{
  char *from = NULL, *who = NULL, *destbot = NULL, *frombot = NULL;
  struct userrec *u = NULL;
  int i;

  from = newsplit(&par);
  frombot = strchr(from, '@');
  if (frombot)
    frombot++;
  else
    frombot = from;
  i = nextbot(frombot);
  if (i != idx) {
    fake_alert(idx, "direction", frombot, "reject");
    return;
  }
  who = newsplit(&par);
  if (!(destbot = strchr(who, '@'))) {
    /* Rejecting a bot */
    i = nextbot(who);
    if (i < 0) {
      botnet_send_priv(idx, conf.bot->nick, from, NULL, "Can't unlink %s (doesn't exist)", who);
    } else if (!strcasecmp(dcc[i].nick, who)) {
      char s[1024];

      /* I'm the connection to the rejected bot */
      putlog(LOG_BOTS, "*", "%s rejected %s", from, dcc[i].nick);
      dprintf(i, "bye %s\n", par[0] ? par : "rejected");
      simple_sprintf(s, "Disconnected %s (%s: %s)", dcc[i].nick, from, par[0] ? par : "rejected");
      chatout("*** %s\n", s);
      botnet_send_unlinked(i, dcc[i].nick, s);
      killsock(dcc[i].sock);
      lostdcc(i);
    } else {
      if (i >= 0)
        botnet_send_reject(i, from, NULL, who, NULL, par);
    }
  } else {                      /* Rejecting user */
    *destbot++ = 0;
    if (!strcasecmp(destbot, conf.bot->nick)) {
      /* Kick someone here! */

      for (i = 0; i < dcc_total; i++) {
        if (dcc[i].type && dcc[i].simul == -1 && (dcc[i].type->flags & DCT_CHAT) && !strcasecmp(who, dcc[i].nick)) {
          u = get_user_by_handle(userlist, from);
          if (u) {
            if (!whois_access(u, dcc[idx].user)) {
              add_note(from, conf.bot->nick, "Sorry, you cannot boot them.", -1, 0);
              return;
            }
            do_boot(i, from, par);
            putlog(LOG_CMDS, "*", "#%s# boot %s (%s)", from, who, par[0] ? par : "No reason");
          }
        }
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
  if (strcasecmp(par, dcc[idx].nick)) {
    char s[1024] = "";

    putlog(LOG_BOTS, "*", "Wrong bot--wanted %s, got %s", dcc[idx].nick, par);
    dprintf(idx, "bye imposter\n");
    simple_snprintf(s, sizeof(s), "Disconnected %s (imposter)", dcc[idx].nick);
    chatout("*** %s\n", s);
    botnet_send_unlinked(idx, dcc[idx].nick, s);
    unvia(idx, findbot(dcc[idx].nick));
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }

  /* Set capitalization the way they want it */
  noshare = 1;
  change_handle(dcc[idx].user, par);
  noshare = 0;
  strlcpy(dcc[idx].nick, par, sizeof(dcc[idx].nick));
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
    fake_alert(idx, "direction", from, "zapf");
    return;
  }
  if (!strcasecmp(to, conf.bot->nick)) {
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
    fake_alert(idx, "direction", from, "zapfb");
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

  bot = newsplit(&par);
  i = nextbot(bot);
  if (i != idx) {
    fake_alert(idx, "direction", bot, "nickchange_i");
    return;
  }
  ssock = newsplit(&par);
  sock = base64_to_int(ssock);
  newnick = newsplit(&par);
  i = partynick(bot, sock, newnick);
  if (i < 0) {
    fake_alert(idx, "sock#", ssock, "nickchange_ii");
    return;
  }
  chanout_but(-1, party[i].chan, "*** (%s) Nick change: %s -> %s\n",
	      conf.bot->hub ? bot : "[botnet]", newnick, party[i].nick);
  botnet_send_nkch_part(idx, i, newnick);
}

/* join <bot> <nick> <chan> <flag><sock> <from>
 */
static void bot_join(int idx, char *par)
{
  char *bot = NULL, *nick = NULL, *x = NULL, *y = NULL;
  struct userrec *u = NULL;
  int i, sock, chan, i2, linking = 0;

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
  i = nextbot(bot);
  if (i != idx) {		/* Ok, garbage from a 1.0g (who uses that
				 * now?) OR raistlin being evil :) */
    fake_alert(idx, "direction", bot, "join");
    return;
  }
  u = get_user_by_handle(userlist, nick);
  if (u) {
    simple_snprintf(TBUF, sizeof(TBUF), "@%s", bot);
    touch_laston(u, TBUF, now);
  }
  i = addparty(bot, nick, chan, y[0], sock, par, &i2);
  botnet_send_join_party(idx, linking, i2);
  if (i != chan) {
    if (i >= 0) {
      chanout_but(-1, i, "*** (%s) %s %s %s.\n", conf.bot->hub ? bot : "[botnet]", nick, "has left the", i ? "channel" : "party line");
    }
    if (!linking)
    chanout_but(-1, chan, "*** (%s) %s %s %s.\n", conf.bot->hub ? bot : "[botnet]", nick, "has joined the", chan ? "channel" : "party line");
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

  bot = newsplit(&par);
  if (bot[0] == '!') {
    silent = 1;
    bot++;
  }
  nick = newsplit(&par);
  etc = newsplit(&par);
  sock = base64_to_int(etc);

  u = get_user_by_handle(userlist, nick);
  if (u) {
    simple_snprintf(TBUF, sizeof(TBUF), "@%s", bot);
    touch_laston(u, TBUF, now);
  }
  if ((partyidx = getparty(bot, sock)) != -1) {
    if (!silent) {
      int chan = party[partyidx].chan;

      if (par[0])
	chanout_but(-1, chan, "*** (%s) %s %s %s (%s).\n", conf.bot->hub ? bot : "[botnet]", nick,
		    "has left the",
		    chan ? "channel" : "party line", par);
      else
	chanout_but(-1, chan, "*** (%s) %s %s %s.\n", conf.bot->hub ? bot : "[botnet]", nick,
		    "has left the",
		    chan ? "channel" : "party line");
    }
    botnet_send_part_party(idx, partyidx, par, silent);
    remparty(bot, sock);
  }

  /* check if we have a remote idx for them */
  int i = 0;

  for (i = 0; i < dcc_total; i++) {
    /* This will potentially close all simul-idxs with matching nick, even though they may be connected multiple times
       but, it won't matter as a new will just be made as needed. */
    if (dcc[i].type && dcc[i].simul >= 0 && !strcasecmp(dcc[i].nick, nick)) {
        dcc[i].simul = -1;
        lostdcc(i);
    }
  }

}

/* away <bot> <sock> <message>
 * null message = unaway
 */
static void bot_away(int idx, char *par)
{
  char *bot = NULL, *etc = NULL;
  int sock, partyidx, linking = 0;

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
		  "*** (%s) %s %s: %s.\n", conf.bot->hub ? bot : "[botnet]",
		  party[partyidx].nick, "is now away", par);
    else
      chanout_but(-1, party[partyidx].chan,
		  "*** (%s) %s %s.\n", conf.bot->hub ? bot : "[botnet]",
		  party[partyidx].nick, "is no longer away");
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

  bot = newsplit(&par);
  work = newsplit(&par);
  sock = base64_to_int(work);
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

static void bot_shareupdate(int idx, char *par)
{
  updatein(idx, par);
}

/* v <frombot> <tobot> <idx:nick>
 */
static void bot_versions(int sock, char *par)
{
  char *frombot = newsplit(&par), *tobot = NULL, *from = NULL;

  if (nextbot(frombot) != sock)
    fake_alert(sock, "direction", frombot, "versions");
  else if (strcasecmp(tobot = newsplit(&par), conf.bot->nick)) {
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
  {"a",			bot_actchan, 0},
  {"aw",		bot_away, 0},
  {"bye",		bot_bye, 0},
  {"c",			bot_chan2, 0},
  {"cp", 		bot_cmdpass, 0},
  {"ct",		bot_chat, 0},
  {"e",			bot_error, 0},
  {"el",		bot_endlink, 0},
  {"i",			bot_idle, 0},
  {"j",			bot_join, 0},
  {"l",			bot_link, 0},
  {"lo",                bot_log, 0},
  {"n",			bot_nlinked, 0},
  {"nc",		bot_nickchange, 0},
  {"p",			bot_priv, 0},
  {"pi",		bot_ping, 0},
  {"po",		bot_pong, 0},
  {"pt",		bot_part, 0},
  {"r",			bot_reject, 0},
  {"rc", 		bot_remotecmd, 0},
  {"rr", 		bot_remotereply, 0},
  {"s",			bot_share, 0},
  {"sb",		bot_shareupdate, 0},
  {"si",		bot_shellinfo, 0},
  {"t",			bot_trace, 0},
  {"tb",		bot_thisbot, 0},
  {"td",		bot_traced, 0},
  {"ts", 		bot_timesync, 0},
  {"u",			bot_update, 0},
  {"ul",		bot_unlink, 0},
  {"un",		bot_unlinked, 0},
  {"v",			bot_versions, 0},
  {"va",                bot_set, 0},
  {"vab",		bot_setbroad, 0},
  {"w",			bot_who, 0},
  {"z",			bot_zapf, 0},
  {"zb",		bot_zapfbroad, 0},
  {NULL,		NULL, 0}
};

static int comp_botcmd_t(const void *m1, const void *m2) {
  const botcmd_t *mi1 = (const botcmd_t *) m1;
  const botcmd_t *mi2 = (const botcmd_t *) m2;
  return strcasecmp(mi1->name, mi2->name);
}

const botcmd_t *search_botcmd_t(const botcmd_t *table, const char* keyString, size_t elements) {
  botcmd_t key;
  key.name = keyString;
  return (const botcmd_t*) bsearch(&key, table, elements, sizeof(botcmd_t), comp_botcmd_t);
}

void parse_botcmd(int idx, const char* code, const char* msg) {
  const botcmd_t *cmd = search_botcmd_t((const botcmd_t*)&C_bot, code, lengthof(C_bot) - 1);

  if (cmd) {
    /* Found a match */
    if (have_cmd(NULL, cmd->type))
      (cmd->func) (idx, (char*)msg);
  }
}

void send_remote_simul(int idx, char *bot, char *cmd, char *par)
{
  char msg[SGRAB - 110] = "";

  simple_snprintf(msg, sizeof msg, "r-s %d %s %d %s %lu %s %s", idx, dcc[idx].nick, dcc[idx].u.chat->con_flags, 
               dcc[idx].u.chat->con_chan, dcc[idx].status, cmd, par);
  putbot(bot, msg);
}

/* idx nick conmask cmd par */
static void bot_rsim(char *botnick, char *code, char *msg)
{
  int ridx = -1, idx = -1, i = 0, rconmask;
  unsigned long status = 0;
  char *nick = NULL, *cmd = NULL, *rconchan = NULL, buf[UHOSTMAX] = "", *par = NULL, *parp = NULL;
  struct userrec *u = get_user_by_handle(userlist, botnick);

  if (bot_hublevel(u) == 999) {
    putlog(LOG_WARN, "*", "BOTCMD received from a leaf. HIJACK.");
    return;
  }

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
   /* See if we can find a simul-idx for the same ridx/nick */
   if (dcc[i].type && dcc[i].simul == ridx && !strcasecmp(dcc[i].nick, nick)) {
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
    strlcpy(dcc[idx].simulbot, botnick, sizeof(dcc[idx].simulbot));
    dcc[idx].u.chat->con_flags = rconmask;
    struct chat_info dummy;
    strlcpy(dcc[idx].u.chat->con_chan, rconchan, sizeof(dummy.con_chan));
    dcc[idx].u.chat->strip_flags = STRIP_ALL;
    strlcpy(dcc[idx].nick, nick, sizeof(dcc[idx].nick));
    simple_snprintf(buf, sizeof buf, "%s@%s", nick, botnick);
    strlcpy(dcc[idx].host, buf, sizeof(dcc[idx].host));
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

  if (!buf || !buf[0] || !dcc[idx].simulbot[0] || idx < 0)
    return;

  /* Truncate out the newline that was put in from the dprintf() */
  char *p = strchr(buf, '\n');
  if (p)
    *p = 0;

  simple_snprintf(rmsg, sizeof rmsg, "r-sr %d %s %s", dcc[idx].simul, dcc[idx].user->handle, buf);          /* remote-simul[r]eturn idx buf */
  putbot(dcc[idx].simulbot, rmsg);
}

static void bot_rsimr(char *botnick, char *code, char *msg)
{
  if (msg[0]) {
    char * par = strdup(msg), *parp = par;
    int idx = atoi(newsplit(&par, ' ', 0));
    char *nick = newsplit(&par, ' ', 0);

    if (dcc[idx].type && (dcc[idx].type == &DCC_CHAT) && dcc[idx].user && !strcmp(dcc[idx].user->handle, nick)) {
      dprintf(idx, "[%s] %s\n", botnick, par);
    }
    free(parp);
  }
}

static void bot_rd(char* botnick, char* code, char* msg)
{
  size_t len = atoi(newsplit(&msg));
  if (msg[0]) {
    int idx = atoi(newsplit(&msg));
    if (msg[0])
      dprintf_real(idx, msg, len, len);
  }
}

static void bot_suicide(char* botnick, char* code, char* msg)
{
  conf_bot *bot = NULL;
  bool valid_source = 0;

  for (bot = conf.bots; bot && bot->nick; bot = bot->next) {
    if (!strcmp(botnick, bot->nick)) {
      valid_source = 1;
      break;
    }
  }

  if (!valid_source) {
    putlog(LOG_WARN, "*", STR("AN INVALID BOT (%s) JUST SENT ME A SUICIDE REQUEST!"), botnick);
    return;
  }

  suicide(msg);
}

static cmd_t my_bot[] = 
{
  {"rd",	"",	(Function) bot_rd,	NULL, 0},
  {"r-sr",	"",	(Function) bot_rsimr,	NULL, HUB},
  {"r-s",	"",	(Function) bot_rsim,	NULL, 0},
  {"suicide",	"",	(Function) bot_suicide,	NULL, 0},
  {NULL, 	NULL, 	NULL, 			NULL, 0}
};

void init_botcmd()
{
  add_builtins("bot", my_bot);
}
/* vim: set sts=2 sw=2 ts=8 et: */
