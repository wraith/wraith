
/* 
 * cmds.c -- handles:
 *   commands from a user via dcc
 *   (split in 2, this portion contains no-irc commands)
 * 
 * dprintf'ized, 3nov1995
 * 
 * $Id: cmds.c,v 1.34 2000/01/17 16:14:45 per Exp $
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
#include "hook.h"
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef HAVE_UNAME
#include <sys/utsname.h>
#endif

char * cuserid (char * str);

extern struct chanset_t *chanset;
extern struct logcategory *logcat;
extern struct dcc_t *dcc;
extern struct userrec *userlist;
extern struct help_list *help;
extern struct user_entry_type USERENTRY_LOG;
extern struct cfg_entry CFG_MOTD;
extern struct cfg_entry ** cfg;
extern int cfg_count;
extern char * binname;

#ifdef G_USETCL
extern tcl_timer_t *timer,
 *utimer;
#endif
extern int dcc_total,
  remote_boots,
  backgrd,
  make_userfile,
  do_restart;
extern int conmask,
  must_be_owner;
#ifdef LEAF
extern struct server_list * serverlist;
extern int curserv, newserverport, cycle_time, default_port;
extern time_t server_online;
extern char botname[], newserver[], newserverpass[];
extern char lock_file[];
void nuke_server(char *reason);
extern char cursrvname[];
#endif

#ifdef G_USETCL
extern Tcl_Interp *interp;
#endif
extern char botnetnick[],
  origbotname[],
  ver[];
extern char network[],
  owner[],
  spaces[],
  localkey[],
  netpass[];
extern time_t now,
  online_since;

void tell_who(struct userrec *u, int idx, int chan)
{
  int i,
    k,
    ok = 0,
    atr = u ? u->flags : 0,
    len;
  char s[1024];			/* temp fix - 1.4 has a better one */

  if (chan == 0)
    dprintf(idx, STR("Party line members:  (* = owner, + = master, @ = op)\n"));
  else {
    dprintf(idx, STR("People on channel %s%d:  (* = owner, + = master, @ = op)\n"), (chan < 100000) ? "" : "*", chan % 100000);
  }
  for (i = 0; i < dcc_total; i++)
    if (dcc[i].type == &DCC_CHAT)
      if (dcc[i].u.chat->channel == chan) {
	spaces[len = HANDLEN - strlen(dcc[i].nick)] = 0;
	if (atr & USER_OWNER) {
	  sprintf(s, STR("  [%.2lu]  %c%s%s %s"), dcc[i].sock, (geticon(i) == '-' ? ' ' : geticon(i)), dcc[i].nick, spaces, dcc[i].host);
	} else {
	  sprintf(s, STR("  %c%s%s %s"), (geticon(i) == '-' ? ' ' : geticon(i)), dcc[i].nick, spaces, dcc[i].host);
	}
	spaces[len] = ' ';
	if (now - dcc[i].timeval > 300) {
	  unsigned long days,
	    hrs,
	    mins;

	  days = (now - dcc[i].timeval) / 86400;
	  hrs = ((now - dcc[i].timeval) - (days * 86400)) / 3600;
	  mins = ((now - dcc[i].timeval) - (hrs * 3600)) / 60;
	  if (days > 0)
	    sprintf(&s[strlen(s)], STR(" (idle %lud%luh)"), days, hrs);
	  else if (hrs > 0)
	    sprintf(&s[strlen(s)], STR(" (idle %luh%lum)"), hrs, mins);
	  else
	    sprintf(&s[strlen(s)], STR(" (idle %lum)"), mins);
	}
	dprintf(idx, STR("%s\n"), s);
	if (dcc[i].u.chat->away != NULL)
	  dprintf(idx, STR("      AWAY: %s\n"), dcc[i].u.chat->away);
      }
  for (i = 0; i < dcc_total; i++)
    if (dcc[i].type == &DCC_BOT) {
      if (!ok) {
	ok = 1;
	dprintf(idx, STR("Bots connected:\n"));
      }
      strcpy(s, ctime(&dcc[i].timeval));
      strcpy(s, &s[1]);
      s[9] = 0;
      strcpy(s, &s[7]);
      s[2] = ' ';
      strcpy(&s[7], &s[10]);
      s[12] = 0;
      spaces[len = HANDLEN - strlen(dcc[i].nick)] = 0;
      if (atr & USER_OWNER) {
	dprintf(idx, STR("  [%.2lu]  %s%c%s%s (%s) %s\n"),
		dcc[i].sock, dcc[i].status & STAT_CALLED ? "<-" : "->", dcc[i].status & STAT_SHARE ? '+' : ' ', dcc[i].nick, spaces, s, dcc[i].u.bot->version);
      } else {
	dprintf(idx, STR("  %s%c%s%s (%s) %s\n"), dcc[i].status & STAT_CALLED ? "<-" : "->", dcc[i].status & STAT_SHARE ? '+' : ' ', dcc[i].nick, spaces, s, dcc[i].u.bot->version);
      }
      spaces[len] = ' ';
    }
  ok = 0;
  for (i = 0; i < dcc_total; i++) {
    if ((dcc[i].type == &DCC_CHAT) && (dcc[i].u.chat->channel != chan)) {
      if (!ok) {
	ok = 1;
	dprintf(idx, STR("Other people on the bot:\n"));
      }
      spaces[len = HANDLEN - strlen(dcc[i].nick)] = 0;
      if (atr & USER_OWNER) {
	sprintf(s, STR("  [%.2lu]  %c%s%s "), dcc[i].sock, (geticon(i) == '-' ? ' ' : geticon(i)), dcc[i].nick, spaces);
      } else {
	sprintf(s, STR("  %c%s%s "), (geticon(i) == '-' ? ' ' : geticon(i)), dcc[i].nick, spaces);
      }
      spaces[len] = ' ';
      if (atr & USER_MASTER) {
	if (dcc[i].u.chat->channel < 0)
	  strcat(s, STR("(-OFF-) "));
	else if (dcc[i].u.chat->channel == 0)
	  strcat(s, STR("(party) "));
	else
	  sprintf(&s[strlen(s)], STR("(%5d) "), dcc[i].u.chat->channel);
      }
      strcat(s, dcc[i].host);
      if (now - dcc[i].timeval > 300) {
	k = (now - dcc[i].timeval) / 60;
	if (k < 60)
	  sprintf(&s[strlen(s)], STR(" (idle %dm)"), k);
	else
	  sprintf(&s[strlen(s)], STR(" (idle %dh%dm)"), k / 60, k % 60);
      }
      dprintf(idx, STR("%s\n"), s);
      if (dcc[i].u.chat->away != NULL)
	dprintf(idx, STR("      AWAY: %s\n"), dcc[i].u.chat->away);
    }
    if ((atr & USER_MASTER) && (dcc[i].type->flags & DCT_SHOWWHO) && (dcc[i].type != &DCC_CHAT)) {
      if (!ok) {
	ok = 1;
	dprintf(idx, STR("Other people on the bot:\n"));
      }
      spaces[len = HANDLEN - strlen(dcc[i].nick)] = 0;
      if (atr & USER_OWNER) {
	sprintf(s, STR("  [%.2lu]  %c%s%s (files) %s"), dcc[i].sock, dcc[i].status & STAT_CHAT ? '+' : ' ', dcc[i].nick, spaces, dcc[i].host);
      } else {
	sprintf(s, STR("  %c%s%s (files) %s"), dcc[i].status & STAT_CHAT ? '+' : ' ', dcc[i].nick, spaces, dcc[i].host);
      }
      spaces[len] = ' ';
      dprintf(idx, STR("%s\n"), s);
    }
  }
}

void cmd_botinfo(struct userrec *u, int idx, char *par)
{
  char s[512],
    s2[32];
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
  log(LCAT_COMMAND, STR("#%s# botinfo"), dcc[idx].nick);
  simple_sprintf(s, STR("%d:%s@%s"), dcc[idx].sock, dcc[idx].nick, botnetnick);
  botnet_send_infoq(-1, s);
  s[0] = 0;
#ifdef LEAF
  while (chan != NULL) {
    if ((strlen(s) + strlen(chan->name) + strlen(network)
	 + strlen(botnetnick) + strlen(ver) + 1) >= 490) {
      strcat(s, STR("++  "));
      break;			/* yeesh! */
    }
    strcat(s, chan->name);
    strcat(s, ", ");
    chan = chan->next;
  }

  if (s[0]) {
    s[strlen(s) - 2] = 0;
    dprintf(idx, STR("*** [%s] %s <%s> (%s) [UP %s]\n"), botnetnick, ver, network, s, s2);
  } else
    dprintf(idx, STR("*** [%s] %s <%s> (no channels) [UP %s]\n"), botnetnick, ver, network, s2);
#else
  dprintf(idx, STR("*** [%s] %s <NO_IRC> [UP %s]\n"), botnetnick, ver, s2);
#endif
}

void cmd_whom(struct userrec *u, int idx, char *par)
{
  log(LCAT_COMMAND, STR("#%s# whom %s"), dcc[idx].nick, par);
  if (par[0] == '*') {
    answer_local_whom(idx, -1);
    return;
  } else if (dcc[idx].u.chat->channel < 0) {
    dprintf(idx, STR("You have chat turned off.\n"));
    return;
  }
  if (!par[0]) {
    answer_local_whom(idx, dcc[idx].u.chat->channel);
  } else {
    int chan = -1;

    if ((par[0] < '0') || (par[0] > '9')) {
      dprintf(idx, STR("No such channel.\n"));
      return;
    } else
      chan = atoi(par);
    if ((chan < 0) || (chan > 99999)) {
      dprintf(idx, STR("Channel # out of range: must be 0-99999\n"));
      return;
    }
    answer_local_whom(idx, chan);
  }
}

void cmd_me(struct userrec *u, int idx, char *par)
{
  int i;

  if (dcc[idx].u.chat->channel < 0) {
    dprintf(idx, STR("You have chat turned off.\n"));
    return;
  }
  if (!par[0]) {
    dprintf(idx, STR("Usage: me <action>\n"));
    return;
  }
  if (dcc[idx].u.chat->away != NULL)
    not_away(idx);
  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type->flags & DCT_CHAT) && (dcc[i].u.chat->channel == dcc[idx].u.chat->channel) && ((i != idx) || (dcc[i].status & STAT_ECHO)))
      dprintf(i, STR("* %s %s\n"), dcc[idx].nick, par);
  botnet_send_act(idx, botnetnick, dcc[idx].nick, dcc[idx].u.chat->channel, par);
  check_tcl_act(dcc[idx].nick, dcc[idx].u.chat->channel, par);
}

void cmd_motd(struct userrec *u, int idx, char *par)
{
  log(LCAT_COMMAND, STR("#%s# motd %s"), dcc[idx].nick, par);
  if (par[0] && (u->flags & USER_MASTER)) {
    char *s;

    s = nmalloc(strlen(par) + 20);
    sprintf(s, STR("(%s) %s"), dcc[idx].nick, par);
    set_cfg_str(NULL, "motd", s);
    nfree(s);
    dprintf(idx, STR("Motd set\n"));
  } else {
    show_motd(idx);
  }
}

void cmd_away(struct userrec *u, int idx, char *par)
{
  if (strlen(par) > 60)
    par[60] = 0;
  set_away(idx, par);
}

void cmd_back(struct userrec *u, int idx, char *par)
{
  not_away(idx);
}

void cmd_newpass(struct userrec *u, int idx, char *par)
{
  char *new;

  log(LCAT_COMMAND, STR("#%s# newpass ..."), dcc[idx].nick);
  if (!par[0]) {
    dprintf(idx, STR("Usage: newpass <newpassword>\n"));
    return;
  }
  new = newsplit(&par);
  if (strlen(new) > 16)
    new[16] = 0;
  if (strlen(new) < 6) {
    dprintf(idx, STR("Please use at least 6 characters.\n"));
    return;
  }
  set_user(&USERENTRY_PASS, u, new);
  dprintf(idx, STR("Changed password to '%s'\n"), new);
}

void cmd_bots(struct userrec *u, int idx, char *par)
{
  log(LCAT_COMMAND, STR("#%s# bots"), dcc[idx].nick);
  tell_bots(idx);
}

void cmd_downbots(struct userrec *u, int idx, char *par)
{
  struct userrec *u2;
  int cnt = 0;
  char work[128] = "";

  log(LCAT_COMMAND, STR("#%s# downbots"), dcc[idx].nick);
  for (u2 = userlist; u2; u2 = u2->next) {
    if (u2->flags & USER_BOT) {
      if (strcasecmp(u2->handle, botnetnick)) {
	if (nextbot(u2->handle) == -1) {
	  strcat(work, u2->handle);
	  cnt++;
	  if (cnt == 10) {
	    dprintf(idx, STR("Down bots: %s\n"), work);
	    work[0] = 0;
	    cnt = 0;
	  } else
	    strcat(work, " ");
	}
      }
    }
  }
  if (work[0])
    dprintf(idx, STR("Down bots: %s\n"), work);
}

void cmd_bottree(struct userrec *u, int idx, char *par)
{
  log(LCAT_COMMAND, STR("#%s# bottree"), dcc[idx].nick);
  tell_bottree(idx, 0);
}

void cmd_who(struct userrec *u, int idx, char *par)
{
  int i;

  log(LCAT_COMMAND, STR("#%s# who %s"), dcc[idx].nick, par);

  if (par[0]) {
    if (dcc[idx].u.chat->channel < 0) {
      dprintf(idx, STR("You have chat turned off.\n"));
      return;
    }
    if (!strcasecmp(par, botnetnick))
      tell_who(u, idx, dcc[idx].u.chat->channel);
    else {
      i = nextbot(par);
      if (i < 0) {
	dprintf(idx, STR("That bot isn't connected.\n"));
      } else if (dcc[idx].u.chat->channel > 99999)
	dprintf(idx, STR("You are on a local channel\n"));
      else {
	char s[40];

	simple_sprintf(s, STR("%d:%s@%s"), dcc[idx].sock, dcc[idx].nick, botnetnick);
	botnet_send_who(i, s, par, dcc[idx].u.chat->channel);
      }
    }
  } else {
    if (dcc[idx].u.chat->channel < 0)
      tell_who(u, idx, 0);
    else
      tell_who(u, idx, dcc[idx].u.chat->channel);
  }
}

void cmd_whois(struct userrec *u, int idx, char *par)
{
  log(LCAT_COMMAND, STR("#%s# whois %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: whois <handle>\n"));
    return;
  }
  tell_user_ident(idx, par, u ? (u->flags & USER_MASTER) : 0);
}

void cmd_match(struct userrec *u, int idx, char *par)
{
  int start = 1,
    limit = 20;
  char *s,
   *s1,
   *chname;

  log(LCAT_COMMAND, STR("#%s# match %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: match <nick/host> [[skip] count]\n"));
    return;
  }
  s = newsplit(&par);
  if (strchr(CHANMETA, par[0]) != NULL)
    chname = newsplit(&par);
  else
    chname = "";
  if (atoi(par) > 0) {
    s1 = newsplit(&par);
    if (atoi(par) > 0) {
      start = atoi(s1);
      limit = atoi(par);
    } else
      limit = atoi(s1);
  }
  tell_users_match(idx, s, start, limit, u ? (u->flags & USER_MASTER) : 0, chname);
}

void cmd_uptime(struct userrec *u, int idx, char *par)
{
  log(LCAT_COMMAND, STR("#%s# uptime"), dcc[idx].nick);
  tell_verbose_uptime(idx);
}

void cmd_userlist(struct userrec *u, int idx, char *par)
{
  int cnt=0;
  log(LCAT_COMMAND, STR("#%s# userlist"), dcc[idx].nick);

  for (u=userlist;u;u=u->next) {
    if (!(u->flags & USER_BOT) && (u->flags & USER_OWNER)) {
      if (cnt)
        dprintf(idx, ", ");
      else
	dprintf(idx, STR("Owners  : "));
      dprintf(idx, u->handle);
      cnt++;
      if (cnt==15) {
	dprintf(idx, "\n");
	cnt=0;
      }
    }
  }
  if (cnt)
    dprintf(idx, "\n");
  cnt=0;

  for (u=userlist;u;u=u->next) {
    if (!(u->flags & (USER_BOT | USER_OWNER)) && (u->flags & USER_MASTER)) {
      if (cnt)
        dprintf(idx, ", ");
      else
	dprintf(idx, STR("Masters : "));
      dprintf(idx, u->handle);
      cnt++;
      if (cnt==15) {
	dprintf(idx, "\n");
	cnt=0;
      }
    }
  }
  if (cnt)
    dprintf(idx, "\n");
  cnt=0;

  for (u=userlist;u;u=u->next) {
    if (!(u->flags & (USER_BOT | USER_MASTER)) && (u->flags & USER_OP)) {
      if (cnt)
        dprintf(idx, ", ");
      else
	dprintf(idx, STR("Ops     : "));
      dprintf(idx, u->handle);
      cnt++;
      if (cnt==15) {
	dprintf(idx, "\n");
	cnt=0;
      }
    }
  }
  if (cnt)
    dprintf(idx, "\n");
  cnt=0;

  for (u=userlist;u;u=u->next) {
    if (!(u->flags & (USER_BOT | USER_OP))) {
      if (cnt)
        dprintf(idx, ", ");
      else
	dprintf(idx, STR("Users   : "));
      dprintf(idx, u->handle);
      cnt++;
      if (cnt==15) {
	dprintf(idx, "\n");
	cnt=0;
      }
    }
  }
  if (cnt)
    dprintf(idx, "\n");
  cnt=0;
}

void cmd_channels(struct userrec *u, int idx, char *par) {
  struct chanset_t * chan;
  log(LCAT_COMMAND, STR("#%s# channels"), dcc[idx].nick);
  dprintf(idx, STR("You have access to these channels:\n"));
  for (chan=chanset;chan;chan=chan->next) {
    struct flag_record fr = { FR_CHAN | FR_GLOBAL, 0, 0, 0, 0 };
    get_user_flagrec(u, &fr, chan->name);
    if (glob_owner(fr) || ((glob_op(fr) || chan_op(fr)) && !(chan_deop(fr) || glob_deop(fr)))) {
      if (channel_inactive(chan))
	dprintf(idx, STR("  %s (inactive)\n"), chan->name);
      else
	dprintf(idx, STR("  %s\n"), chan->name);
    }
  }
}

void cmd_status(struct userrec *u, int idx, char *par)
{
  int atr = u ? u->flags : 0;

  log(LCAT_COMMAND, STR("#%s# status %s"), dcc[idx].nick, par);

  if (!strcasecmp(par, STR("all"))) {
    if (!(atr & USER_MASTER)) {
      dprintf(idx, STR("You do not have Bot Master privileges.\n"));
      return;
    }
    tell_verbose_status(idx);
    tell_mem_status_dcc(idx);
    dprintf(idx, "\n");
    tell_settings(idx);
  } else {
    tell_verbose_status(idx);
    tell_mem_status_dcc(idx);
  }
}

void cmd_dccstat(struct userrec *u, int idx, char *par)
{
  log(LCAT_COMMAND, STR("#%s# dccstat"), dcc[idx].nick);
  tell_dcc(idx);
}

void cmd_boot(struct userrec *u, int idx, char *par)
{
  int i,
    ok = 0;
  char *who;
  struct userrec *u2;

  log(LCAT_COMMAND, STR("#%s# boot %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: boot nick[@bot]\n"));
    return;
  }
  who = newsplit(&par);
  if (strchr(who, '@') != NULL) {
    char whonick[512];

    splitc(whonick, who, '@');
    whonick[20] = 0;
    if (!strcasecmp(who, botnetnick)) {
      cmd_boot(u, idx, whonick);
      return;
    }
    if (remote_boots > 0) {
      i = nextbot(who);
      if (i < 0) {
	dprintf(idx, STR("No such bot connected.\n"));
	return;
      }
      botnet_send_reject(i, dcc[idx].nick, botnetnick, whonick, who, par[0] ? par : dcc[idx].nick);
      log(LCAT_BOT, STR("#%s# boot %s@%s (%s)"), dcc[idx].nick, whonick, who, par[0] ? par : dcc[idx].nick);
    } else
      dprintf(idx, STR("Remote boots are disabled here.\n"));
    return;
  }
  for (i = 0; i < dcc_total; i++)
    if (!strcasecmp(dcc[i].nick, who) && !ok && (dcc[i].type->flags & DCT_CANBOOT)) {
      u2 = get_user_by_handle(userlist, dcc[i].nick);
      if (u2 && (u2->flags & USER_OWNER) && strcasecmp(dcc[idx].nick, who)) {
	dprintf(idx, STR("Can't boot the bot owner.\n"));
	return;
      }
      if (u2 && (u2->flags & USER_MASTER) && !(u && (u->flags & USER_MASTER))) {
	dprintf(idx, STR("Can't boot a bot master.\n"));
	return;
      }
      dprintf(idx, STR("Booted %s from the bot.\n"), dcc[i].nick);
      do_boot(i, dcc[idx].nick, par);
      ok = 1;
    }
  if (!ok)
    dprintf(idx, STR("Who?  No such person on the party line.\n"));
}

#ifdef HUB
void cmd_config(struct userrec *u, int idx, char *par)
{
  /*
     .config
     Usage + available entry list
     .config name
     Show current value + description
     .config name value
     Set
   */
  char *name;
  struct cfg_entry * cfgent = NULL;
  int cnt, i;

  log(LCAT_COMMAND, STR("#%s# config %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: .config [name [value]]\n"));
    dprintf(idx, STR("Defined config entry names:\n"));
    cnt = 0;
    for (i=0;i<cfg_count;i++) {
      if ((cfg[i]->flags & CFGF_GLOBAL) && (cfg[i]->describe)) {
	if (!cnt)
	  dprintf(idx, "  ");
	dprintf(idx, STR("%s "), cfg[i]->name);
	cnt++;
	if (cnt==10) {
	  dprintf(idx, "\n");
	  cnt=0;
	}
      }
    }
    if (cnt)
      dprintf(idx, "\n");
    return;
  }
  name = newsplit(&par);
  for (i=0;!cfgent && (i<cfg_count);i++)
    if (!strcmp(cfg[i]->name, name))
      cfgent=cfg[i];
  if (!cfgent || !cfgent->describe) {
    dprintf(idx, STR("No such config entry\n"));
    return;
  }
  if (!par[0]) {
    cfgent->describe(cfgent, idx);
    if (!cfgent->gdata)
      dprintf(idx, STR("No current value\n"));
    else {
      dprintf(idx, STR("Currently: %s\n"), cfgent->gdata);
    }
    return;
  }
  if (strlen(par)>=256) {
    dprintf(idx, STR("Value can't be longer than 255 chars"));
    return;
  }
  set_cfg_str(NULL, cfgent->name, par);
  if (!cfgent->gdata)
    dprintf(idx, STR("Now: (not set)\n"));
  else {
    dprintf(idx, STR("Now: %s\n"), cfgent->gdata);
  }
#ifdef HUB
  write_userfile(idx);
#endif

}

void cmd_botconfig(struct userrec *u, int idx, char *par)
{
  struct userrec *u2;
  char *p;
  struct xtra_key *k;
  struct cfg_entry * cfgent;
  int i, cnt;

  /* botconfig bot [name [value]]  */
  log(LCAT_COMMAND, STR("#%s# botconfig %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: .botconfig bot [name [value|-]]\n"));
    cnt=0;
    for (i=0;i<cfg_count;i++) {
      if (cfg[i]->flags & CFGF_LOCAL) {
	dprintf(idx, STR("%s "), cfg[i]->name);
	cnt++;
	if (cnt==10) {
	  dprintf(idx, "\n");
	  cnt=0;
	}
      }
    }
    if (cnt>0)
      dprintf(idx, "\n");
    return;
  }
  p = newsplit(&par);
  u2 = get_user_by_handle(userlist, p);
  if (!u2) {
    dprintf(idx, STR("No such user.\n"));
    return;
  }
  if (!(u2->flags & USER_BOT)) {
    dprintf(idx, STR("%s isn't a bot.\n"), p);
    return;
  }
  if (!par[0]) {
    for (i=0;i<cfg_count;i++) {
      if ((cfg[i]->flags & CFGF_LOCAL) && (cfg[i]->describe)) {
	k=get_user(&USERENTRY_CONFIG, u2);
	while (k && strcmp(k->key, cfg[i]->name))
	  k=k->next;
	if (k)
	  dprintf(idx, STR("  %s: %s\n"), k->key, k->data);
	else
	  dprintf(idx, STR("  %s: (not set)\n"), cfg[i]->name);
      }
    }
    return;
  }
  p=newsplit(&par);
  cfgent=NULL;
  for (i=0;!cfgent && (i<cfg_count);i++)
    if (!strcmp(cfg[i]->name, p) && (cfg[i]->flags & CFGF_LOCAL) && (cfg[i]->describe))
      cfgent=cfg[i];
  if (!cfgent) {
    dprintf(idx, STR("No such configuration value\n"));
    return;
  }
  if (par[0]) {
    set_cfg_str(u2->handle, cfgent->name, (strcmp(par, "-")) ? par : NULL);
    dprintf(idx, STR("Now: "));
#ifdef HUB
    write_userfile(idx);
#endif
  } else {
    if (cfgent->describe)
      cfgent->describe(cfgent, idx);
  }
  k=get_user(&USERENTRY_CONFIG, u2);
  while (k && strcmp(k->key, cfgent->name))
    k=k->next;
  if (k)
    dprintf(idx, STR("  %s: %s\n"), k->key, k->data);
  else
    dprintf(idx, STR("  %s: (not set)\n"), cfgent->name);
}

#ifdef G_DCCPASS
void cmd_cmdpass(struct userrec *u, int idx, char *par)
{
  struct tcl_bind_mask *hm;
  char *cmd,
   *pass;
  int i;

  /* cmdpass [command [newpass]] */
  log(LCAT_COMMAND, STR("#%s# cmdpass ..."), dcc[idx].nick);
  if (!isowner(u->handle)) {
    log(LCAT_WARNING, STR("%s attempted to set a command password - not perm owner"), dcc[idx].nick);
    dprintf(idx, STR("Perm owners only.\n"));
    return;
  }
  cmd = newsplit(&par);
  pass = newsplit(&par);
  if (!cmd[0] || par[0]) {
    dprintf(idx, STR("Usage: .cmdpass command [password]\n"));
    dprintf(idx, STR("  if no password is specified, the commands password is reset\n"));
    return;
  }
  for (i = 0; cmd[i]; i++)
    cmd[i] = tolower(cmd[i]);
  for (hm = H_dcc->first; hm; hm = hm->next)
    if (!strcasecmp2(cmd, hm->mask))
      break;
  if (!hm) {
    dprintf(idx, STR("No such DCC command\n"));
    return;
  }
  if (pass[0]) {
    char epass[36],
      tmp[256];

    encrypt_pass(pass, epass);
    sprintf(tmp, STR("%s %s"), cmd, epass);
    set_cmd_pass(tmp, 1);
    dprintf(idx, STR("Set command password for %s\n"), cmd);
  } else {
    set_cmd_pass(cmd, 1);
    dprintf(idx, STR("Removed command password for %s\n"), cmd);
  }
#ifdef HUB
    write_userfile(idx);
#endif

}
#endif
#endif

#ifdef HUB
void cmd_lagged(struct userrec *u, int idx, char *par)
{
  /* Lists botnet lag to *directly connected* bots */
  int i;

  log(LCAT_COMMAND, STR("#%s# lagged %s"), u->handle, par);
  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type == &DCC_BOT) {
      dprintf(idx, STR("%9s - %i seconds\n"), dcc[i].nick, (dcc[i].pingtime > 120) ? (now - dcc[i].pingtime) : dcc[i].pingtime);
    }
  }
}

void cmd_logconfig(struct userrec *u, int idx, char *par) {
  char * name, *tochan, *tofile, *bcast, *flags;
  struct logcategory * lc;
  char tmp[1024];
  log(LCAT_COMMAND, STR("#%s# logconfig %s"), dcc[idx].nick, par);
  if (!isowner(dcc[idx].nick)) {
    dprintf(idx, STR("Perm owners only\n"));
    return;
  }
  /*
    .logconfig category tochannel tofile broadcast flags
   */
  name=newsplit(&par);
  tochan=newsplit(&par);
  tofile=newsplit(&par);
  bcast=newsplit(&par);
  flags=newsplit(&par);
  if (!name[0]) {
    dprintf(idx, STR("Usage: logconfig category [tochannel tofile broadcast flag]\n"));
    return;
  }

  lc=findlogcategory(name);
  if (!lc) {
    dprintf(idx, STR("Invalid log category\n"));
    return;
  }
  if (!tochan[0]) {
    int i=lc->flags;
    char fl;
    fl='a';
    while (i>1) {
      fl++;
      i = i >> 1;
    }
    dprintf(idx, STR("%s settings: %s, %s, %s, +%c\n"), lc->name, 
	    lc->logtochan ? STR("channel") : STR("not channel"),
	    lc->logtofile ? STR("file") : STR("not file"),
	    lc->broadcast ? STR("broadcast") : STR("not broadcast"),
	    fl);
    return;
  }

  if (!flags[0] || par[0]) {
    dprintf(idx, STR("Usage: logconfig category [tochannel tofile broadcast flag]\n"));
    return;
  }
  if (strcmp(tochan, "1") && strcmp(tochan, "0")) {
    dprintf(idx, STR("tochan must be 1 or 0\n"));
    return;
  }
  if (strcmp(tofile, "1") && strcmp(tofile, "0")) {
    dprintf(idx, STR("tofile must be 1 or 0\n"));
    return;
  }
  if (strcmp(bcast, "1") && strcmp(bcast, "0")) {
    dprintf(idx, STR("bcast must be 1 or 0\n"));
    return;
  }
  if (strlen(flags)!=1) {
    dprintf(idx, STR("flag must be ONE valid user flag\n"));
    return;
  }
  if ( (flags[0]<'a') || (flags[0]>'z') ) {
    dprintf(idx, STR("Invalid flag\n"));
    return;
  }
  lc->logtochan=atoi(tochan);
  lc->logtofile=atoi(tofile);
  lc->broadcast=atoi(bcast);
  lc->flags= (1 << (flags[0] - 'a'));
  sprintf(tmp, STR("%s %i %i %i %i"), lc->name, lc->logtochan, lc->logtofile, lc->broadcast, lc->flags); 
  botnet_send_logsettings_broad(-1, tmp);
  dprintf(idx, STR("Log settings changed\n"));
}
#endif

void cmd_log(struct userrec *u, int idx, char *par)
{
  char work[512],
    *clist,
    *cat;
  int i;
  struct logcategory *lc;

  log(LCAT_COMMAND, STR("#%s# log %s"), u->handle, par);
  if (!par[0]) {
    dprintf(idx, STR("Your log settings are:\n"));
    work[0] = 0;
    i = 0;
    for (lc = logcat; lc; lc = lc->next) {
      if (user_has_cat(u, lc))
	strcat(work, "+");
      else
	strcat(work, "-");
      strcat(work, lc->name);
      strcat(work, " ");
      i++;
      if (i == 7) {
	dprintf(idx, STR("  %s\n"), work);
	work[0] = 0;
	i = 0;
      }
    }
    if (work[0])
      dprintf(idx, STR("  %s\n"), work);
    return;
  }
  cat = newsplit(&par);
  clist = get_user(&USERENTRY_LOG, u);
  if (clist) {
    strncpy0(work, clist, sizeof(work));
  }
  else
    work[0] = 0;
  while ((cat) && (cat[0])) {
    char act,
     *p;
    int hascat;

    act = cat[0];
    cat++;
    lc = findlogcategory(cat);
    if (lc) {
      p = strstr(work, cat);
      if (p)
	p += strlen(cat);
      if ((p) && ((p[0] == 0) || (p[0] == ' ')))
	hascat = 1;
      else
	hascat = 0;
      switch (act) {
      case '+':
	if (!hascat) {
	  strncat(work, " ", sizeof(work));
	  strncat(work, lc->name, sizeof(work));
	  dprintf(idx, STR("Added category %s\n"), cat);
	}
	break;
      case '-':
	if (hascat) {
	  if (!p[0])
	    *(p - strlen(cat)) = 0;
	  else
	    memcpy((p - strlen(cat)), (p + 1), strlen(p + 1) + 1);
	  dprintf(idx, STR("Deleted category %s\n"), cat);
	}
	break;
      case '?':
	dprintf(idx, STR("%s description: %s\n"), lc->name, lc->desc);
	break;
      default:
	dprintf(idx, STR("Use +category|-category|?category\n"));
      }
    } else {
      dprintf(idx, STR("No such log category: %s\n"), cat);
    }
    cat = newsplit(&par);
  }
  set_user(&USERENTRY_LOG, u, work);
}

#ifdef HUB
void cmd_newleaf(struct userrec *u, int idx, char *par)
{
  char *handle,
   *host;
  struct userrec *u1;
  struct bot_addr *bi;

  log(LCAT_COMMAND, STR("#%s# newleaf %s"), dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, STR("Usage: newleaf <handle> [host] [anotherhost]\n"));
    dprintf(idx, STR("       Leafs can't link unless you specify a *!ident@ip hostmask\n"));
  } else {
    handle = newsplit(&par);
    if (strlen(handle) > HANDLEN)
      handle[HANDLEN] = 0;
    if (get_user_by_handle(userlist, handle))
      dprintf(idx, STR("Already got a %s user/bot\n"), handle);
    else if (strchr(BADHANDCHARS, handle[0]) != NULL)
      dprintf(idx, STR("You can't start a botnick with '%c'.\n"), handle[0]);
    else {
      userlist = adduser(userlist, handle, STR("none"), "-", USER_BOT | USER_FRIEND | USER_OP);
      u1 = get_user_by_handle(userlist, handle);
      bi = user_malloc(sizeof(struct bot_addr));

      bi->uplink = user_malloc(strlen(botnetnick) + 1);
      strcpy(bi->uplink, botnetnick);
      bi->address = user_malloc(1);
      bi->address[0] = 0;
      bi->telnet_port = 3333;
      bi->relay_port = 3333;
      bi->hublevel = 0;
      set_user(&USERENTRY_BOTADDR, u1, bi);
      host = newsplit(&par);
      while ((host) && (host[0])) {
	addhost_by_handle(handle, host);
	host = newsplit(&par);
      }
      set_user(&USERENTRY_PASS, u1, netpass);
      dprintf(idx, STR("Added new leaf: %s\n"), handle);
    }
  }
}
#endif

/*
void cmd_pls_bot(struct userrec *u, int idx, char *par)
{
  char *handle, *addr, *p, *q, *host;
  struct userrec *u1;
  struct bot_addr *bi;
  log(LCAT_COMMAND, STR("#%s# +bot %s"), dcc[idx].nick, par);

  if (!par[0])
    dprintf(idx, STR("Usage: +bot <handle> <address[:telnet-port[/relay-port]]> [host]\n"));
  else {
    handle = newsplit(&par);
    addr = newsplit(&par);
    if (strlen(handle) > HANDLEN)
      handle[HANDLEN] = 0;
    if (get_user_by_handle(userlist, handle))
      dprintf(idx, STR("Someone already exists by that name.\n"));
    else if (strchr(BADHANDCHARS, handle[0]) != NULL)
      dprintf(idx, STR("You can't start a botnick with '%c'.\n"), handle[0]);
    else {
      char tmp[30];
      if (strlen(addr) > 60)
	addr[60] = 0;
      userlist = adduser(userlist, handle, STR("none"), "-", USER_BOT);
      u1 = get_user_by_handle(userlist, handle);
      bi = user_malloc(sizeof(struct bot_addr));
      bi->uplink=user_malloc(1);
      bi->uplink[0]=0;
      q = strchr(addr, ':');
      if (!q) {
	bi->address = user_malloc(strlen(addr) + 1);
	strcpy(bi->address, addr);
	bi->telnet_port = 3333;
	bi->relay_port = 3333;
      } else {
	bi->address = user_malloc(q - addr + 1);
	strncpy0(bi->address, addr, q - addr);
	p = q + 1;
	bi->telnet_port = atoi(p);
	q = strchr(p, '/');
	if (!q) {
	  bi->relay_port = bi->telnet_port;
	} else {
	  bi->relay_port = atoi(q + 1);
	}
      }
      set_user(&USERENTRY_BOTADDR, u1, bi);
      sprintf(tmp, STR("%lu %s"), time(NULL), u->handle);
      set_user(&USERENTRY_ADDED, u1, tmp);
      dprintf(idx, STR("Added bot '%s' with address '%s' and no password.\n"),
	      handle, addr);
      host = newsplit(&par);
      if (host[0]) {
	addhost_by_handle(handle, host);
      } else if (!add_bot_hostmask(idx, handle))
	dprintf(idx, STR("You'll want to add a hostmask if this bot will ever %s"),
		STR("be on any channels that I'm on.\n"));
    }
  }
}
*/

void cmd_chnick(struct userrec *u, int idx, char *par)
{
  char hand[HANDLEN + 1],
    newhand[HANDLEN + 1];
  int i,
    atr = u ? u->flags : 0,
    atr2;
  struct userrec *u2;

  log(LCAT_COMMAND, STR("#%s# chnick %s"), dcc[idx].nick, par);

  strncpy0(hand, newsplit(&par), sizeof(hand));
  strncpy0(newhand, newsplit(&par), sizeof(newhand));

  if (!hand[0] || !newhand[0]) {
    dprintf(idx, STR("Usage: chnick <oldnick> <newnick>\n"));
    return;
  }
  for (i = 0; i < strlen(newhand); i++)
    if ((newhand[i] <= 32) || (newhand[i] >= 127) || (newhand[i] == '@'))
      newhand[i] = '?';
  if (strcasecmp(hand, newhand))
    dprintf(idx, STR("chnick can only change capitalization of nicks.\n"));
  else {
    u2 = get_user_by_handle(userlist, hand);
    atr2 = u2 ? u2->flags : 0;
    if (!(atr & USER_OWNER))
      dprintf(idx, STR("You can't change shared bot's nick.\n"));
    else if ((atr2 & USER_OWNER) && !(atr & USER_OWNER) && strcasecmp(dcc[idx].nick, hand))
      dprintf(idx, STR("Can't change the bot owner's handle.\n"));
    else if (isowner(hand) && strcasecmp(dcc[idx].nick, hand))
      dprintf(idx, STR("Can't change the permanent bot owner's handle.\n"));
    else if (!strcasecmp(newhand, botnetnick) && (!(atr2 & USER_BOT) || nextbot(hand) != -1))
      dprintf(idx, STR("Hey! That's MY name!\n"));
    else if (change_handle(u2, newhand)) {
      dprintf(idx, STR("Changed.\n"));
    } else
      dprintf(idx, STR("Failed.\n"));
  }
}

void cmd_chpass(struct userrec *u, int idx, char *par)
{
  char *handle,
   *new;
  int atr = u ? u->flags : 0,
    l;

  if (!par[0]) {
    log(LCAT_COMMAND, STR("#%s# chpass "), dcc[idx].nick);
    dprintf(idx, STR("Usage: chpass <handle> [password]\n"));
  } else {
    handle = newsplit(&par);
    log(LCAT_COMMAND, STR("#%s# chpass %s ..."), dcc[idx].nick, handle);
    u = get_user_by_handle(userlist, handle);
    if (!u)
      dprintf(idx, STR("No such user.\n"));
    else if (!(atr & USER_OWNER))
      dprintf(idx, STR("You can't change shared bot's password.\n"));
    else if ((u->flags & USER_OWNER) && !(atr & USER_OWNER) && strcasecmp(handle, dcc[idx].nick))
      dprintf(idx, STR("Can't change the bot owner's password.\n"));
    else if (isowner(handle) && strcasecmp(dcc[idx].nick, handle))
      dprintf(idx, STR("Can't change the permanent bot owner's password.\n"));
    else if (!par[0]) {
      set_user(&USERENTRY_PASS, u, NULL);
      dprintf(idx, STR("Removed password.\n"));
    } else {
      l = strlen(new = newsplit(&par));
      if (l > 16)
	new[16] = 0;
      if (l < 6)
	dprintf(idx, STR("Please use at least 6 characters.\n"));
      else {
	set_user(&USERENTRY_PASS, u, new);
	dprintf(idx, STR("Changed password.\n"));
      }
    }
  }
}

#ifdef HUB
void cmd_hublevel(struct userrec *u, int idx, char *par)
{
  char *handle,
   *level;
  struct bot_addr *bi,
   *obi;
  struct userrec *u1;

  log(LCAT_COMMAND, STR("#%s# hublevel %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: hublevel <botname> <level>\n"));
    return;
  }
  handle = newsplit(&par);
  level = newsplit(&par);
  u1 = get_user_by_handle(userlist, handle);
  if (!u1 || !(u1->flags & USER_BOT)) {
    dprintf(idx, STR("Useful only for bots.\n"));
    return;
  }
  dprintf(idx, STR("Changed bot's hublevel.\n"));
  obi = get_user(&USERENTRY_BOTADDR, u1);
  bi = user_malloc(sizeof(struct bot_addr));

  bi->uplink = user_malloc(strlen(obi->uplink) + 1);
  strcpy(bi->uplink, obi->uplink);
  bi->address = user_malloc(strlen(obi->address) + 1);
  strcpy(bi->address, obi->address);
  bi->telnet_port = obi->telnet_port;
  bi->relay_port = obi->relay_port;
  bi->hublevel = atoi(level);
  set_user(&USERENTRY_BOTADDR, u1, bi);
  write_userfile(idx);
}

#endif

#ifdef HUB
void cmd_uplink(struct userrec *u, int idx, char *par)
{
  char *handle,
   *uplink;
  struct bot_addr *bi,
   *obi;
  struct userrec *u1;

  log(LCAT_COMMAND, STR("#%s# uplink %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: uplink <botname> [<uplink>]\n"));
    return;
  }
  handle = newsplit(&par);
  uplink = newsplit(&par);
  if (!uplink)
    uplink = "";
  u1 = get_user_by_handle(userlist, handle);
  if (!u1 || !(u1->flags & USER_BOT)) {
    dprintf(idx, STR("Useful only for bots.\n"));
    return;
  }
  dprintf(idx, STR("Changed bot's uplink.\n"));
  obi = get_user(&USERENTRY_BOTADDR, u1);
  bi = user_malloc(sizeof(struct bot_addr));

  bi->uplink = user_malloc(strlen(uplink) + 1);
  strcpy(bi->uplink, uplink);
  bi->address = user_malloc(strlen(obi->address) + 1);
  strcpy(bi->address, obi->address);
  bi->telnet_port = obi->telnet_port;
  bi->relay_port = obi->relay_port;
  bi->hublevel = obi->hublevel;
  set_user(&USERENTRY_BOTADDR, u1, bi);
}

#endif

#ifdef HUB
void cmd_chaddr(struct userrec *u, int idx, char *par)
{
  char *handle,
   *addr,
   *p,
   *q;
  struct bot_addr *bi,
   *obi;
  struct userrec *u1;

  log(LCAT_COMMAND, STR("#%s# chaddr %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: chaddr <botname> <address:botport#/userport#>\n"));
    return;
  }
  handle = newsplit(&par);
  addr = newsplit(&par);
  if (strlen(addr) > UHOSTMAX)
    addr[UHOSTMAX] = 0;
  u1 = get_user_by_handle(userlist, handle);
  if (!u1 || !(u1->flags & USER_BOT)) {
    dprintf(idx, STR("Useful only for tandem bots.\n"));
    return;
  }
  if (!u || !u->flags & USER_OWNER) {
    dprintf(idx, STR("You can't change shared bot's address.\n"));
    return;
  }
  dprintf(idx, STR("Changed bot's address.\n"));
  obi = get_user(&USERENTRY_BOTADDR, u1);
  bi = user_malloc(sizeof(struct bot_addr));

  bi->uplink = user_malloc(strlen(obi->uplink) + 1);
  strcpy(bi->uplink, obi->uplink);
  bi->hublevel = obi->hublevel;
  q = strchr(addr, ':');
  if (!q) {
    bi->address = user_malloc(strlen(addr) + 1);
    strcpy(bi->address, addr);
    bi->telnet_port = 3333;
    bi->relay_port = 3333;
  } else {
    bi->address = user_malloc(q - addr + 1);
    strncpy0(bi->address, addr, q - addr + 1);
    p = q + 1;
    bi->telnet_port = atoi(p);
    q = strchr(p, '/');
    if (!q) {
      bi->relay_port = bi->telnet_port;
    } else {
      bi->relay_port = atoi(q + 1);
    }
  }
  set_user(&USERENTRY_BOTADDR, u1, bi);
}
#endif

void cmd_comment(struct userrec *u, int idx, char *par)
{
  char *handle;
  struct userrec *u1;

  log(LCAT_COMMAND, STR("#%s# comment %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: comment <handle> <newcomment>\n"));
    return;
  }
  handle = newsplit(&par);
  u1 = get_user_by_handle(userlist, handle);
  if (!u1) {
    dprintf(idx, STR("No such user!\n"));
    return;
  }
  if ((u1->flags & USER_OWNER) && !(u && (u->flags & USER_OWNER)) && strcasecmp(handle, dcc[idx].nick)) {
    dprintf(idx, STR("Can't change comment on the bot owner.\n"));
    return;
  }
  if (!strcasecmp(par, STR("none"))) {
    dprintf(idx, STR("Okay, comment blanked.\n"));
    set_user(&USERENTRY_COMMENT, u1, NULL);
    return;
  }
  dprintf(idx, STR("Changed comment.\n"));
  set_user(&USERENTRY_COMMENT, u1, par);
}

#ifdef LEAF
void got_rsn(char *botnick, char *code, char *par);
void got_rn(char *botnick, char *code, char *par);
#endif

void cmd_resetnicks(struct userrec *u, int idx, char *par)
{
  log(LCAT_COMMAND, STR("#%s# resetnicks"), dcc[idx].nick);
  botnet_send_zapf_broad(-1, botnetnick, NULL, STR("rsn"));
  dprintf(idx, STR("Sent resetnick request to all bots\n"));
#ifdef LEAF
  got_rsn(0,0,0);
#endif
}

void cmd_randnicks(struct userrec *u, int idx, char *par)
{
  log(LCAT_COMMAND, STR("#%s# randnicks"), dcc[idx].nick);
  botnet_send_zapf_broad(-1, botnetnick, NULL, STR("rn"));
  dprintf(idx, STR("Sent randnick request to all bots\n"));
#ifdef LEAF
  got_rn(0,0,0);
#endif
}

void cmd_restart(struct userrec *u, int idx, char *par)
{
  log(LCAT_COMMAND, STR("#%s# restart"), dcc[idx].nick);
  if (!backgrd) {
    dprintf(idx, STR("You can not .restart a bot when running -n (due to tcl)\n"));
    return;
  }
  dprintf(idx, STR("Restarting.\n"));
  if (make_userfile) {
    make_userfile = 0;
  }
#ifdef HUB
  write_userfile(-1);
#endif
  log(LCAT_INFO, STR("Restarting ..."));
#ifdef G_USETCL
  wipe_timers(interp, &utimer);
  wipe_timers(interp, &timer);
#endif
  do_restart = idx;
}

void cmd_rehash(struct userrec *u, int idx, char *par)
{
  log(LCAT_COMMAND, STR("#%s# rehash"), dcc[idx].nick);
  dprintf(idx, STR("Rehashing.\n"));
  if (make_userfile) {
    make_userfile = 0;
  }
#ifdef HUB
  write_userfile(-1);
#endif
  log(LCAT_INFO, STR("Rehashing ..."));
  do_restart = -2;
}

/* this get replaced in server.so with a version that handles the server */
void cmd_die(struct userrec *u, int idx, char *par)
{
  char s[1024];

  log(LCAT_COMMAND, STR("#%s# die %s"), dcc[idx].nick, par);
  if (par[0]) {
    simple_sprintf(s, STR("BOT SHUTDOWN (%s: %s)"), dcc[idx].nick, par);
  } else {
    simple_sprintf(s, STR("BOT SHUTDOWN (authorized by %s)"), dcc[idx].nick);
  }
  log(LCAT_BOT, STR("%s\n"), s);
  botnet_send_chat(-1, botnetnick, s);
  botnet_send_bye();
#ifdef HUB
  write_userfile(-1);
#endif
  simple_sprintf(s, STR("DIE BY %s!%s (%s)"), dcc[idx].nick, dcc[idx].host, par[0] ? par : STR("request"));
  fatal(s, 0);
}

void cmd_debug(struct userrec *u, int idx, char *par)
{
  log(LCAT_COMMAND, STR("#%s# debug"), dcc[idx].nick);
  debug_mem_to_dcc(idx);
}

void cmd_unlink(struct userrec *u, int idx, char *par)
{
  int i;
  char *bot;

  log(LCAT_COMMAND, STR("#%s# unlink %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: unlink <bot> [reason]\n"));
    return;
  }
  bot = newsplit(&par);
  i = nextbot(bot);
  if (i < 0) {
    botunlink(idx, bot, par);
    return;
  }
  /* if we're directly connected to that bot, just do it 
   * (is nike gunna sue?) */
  if (!strcasecmp(dcc[i].nick, bot))
    botunlink(idx, bot, par);
  else {
    char x[40];

    simple_sprintf(x, STR("%d:%s@%s"), dcc[idx].sock, dcc[idx].nick, botnetnick);
    botnet_send_unlink(i, x, lastbot(bot), bot, par);
  }
}

void cmd_update(struct userrec *u, int idx, char *par)
{
#ifdef HUB
  log(LCAT_COMMAND, STR("#%s# update %s"), dcc[idx].nick, par);
  dprintf(idx, STR("Hubs can't be autoupdated.\n"));
#else
  char * path=NULL, *newbin;
  struct stat sb;
  int i;
  path=newsplit(&par);
  par=path;
  if (!par[0]) {
    dprintf(idx, STR("Usage: update <newbinname>\n"));
    return;
  }
  path = nmalloc(strlen(binname) + strlen(par));
  strcpy(path, binname);
  newbin=strrchr(path, '/');
  if (!newbin) {
    nfree(path);
    dprintf(idx, STR("Don't know current binary name\n"));
    return;
  }
  newbin++;
  if (strchr(par, '/')) {
    *newbin=0;
    dprintf(idx, STR("New binary must be in %s and name must be specified without path information\n"), path);
    nfree(path);
    return;
  }
  strcpy(newbin, par);
  if (!strcmp(path, binname)) {
    nfree(path);
    dprintf(idx, STR("Can't update with the current binary\n"));
    return;
  }
  if (stat(path, &sb)) {
    dprintf(idx, STR("%s can't be accessed\n"), path);
    nfree(path);
    return;
  }
  if (chmod(path, S_IRUSR | S_IWUSR | S_IXUSR)) {
    dprintf(idx, STR("Can't set mode 0600 on %s\n"), path);
    nfree(path);
    return;
  }
  if (unlink(binname)) {
    dprintf(idx, STR("Can't remove old binary\n"));
    nfree(path);
    return;
  }
  if (rename(path, binname)) {
    dprintf(idx, STR("Can't rename %s to %s - Manual update required\n"), path, binname);
    nfree(path);
    return;
  }
  unlink(lock_file);
  dprintf(idx, STR("Restarting new binary\n"));
  system(binname);
  for (i=0;i<dcc_total;i++) {
    if (dcc[i].type) {
      killsock(dcc[i].sock);
      lostdcc(i);
    }
  }
  exit(0);
#endif
}

void cmd_relay(struct userrec *u, int idx, char *par)
{
  log(LCAT_COMMAND, STR("#%s# relay %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: relay <bot>\n"));
    return;
  }
  tandem_relay(idx, par, 0);
}

#ifdef HUB
void cmd_save(struct userrec *u, int idx, char *par)
{
  log(LCAT_COMMAND, STR("#%s# save"), dcc[idx].nick);
  dprintf(idx, STR("Saving user file...\n"));
  write_userfile(-1);
}

void cmd_backup(struct userrec *u, int idx, char *par)
{
  log(LCAT_COMMAND, STR("#%s# backup"), dcc[idx].nick);
  dprintf(idx, STR("Backing up the user file...\n"));
  backup_userfile();
}
#endif

void cmd_trace(struct userrec *u, int idx, char *par)
{
  int i;
  char x[NOTENAMELEN + 11],
    y[11];

  log(LCAT_COMMAND, STR("#%s# trace %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: trace <botname>\n"));
    return;
  }
  if (!strcasecmp(par, botnetnick)) {
    dprintf(idx, STR("That's me!  Hiya! :)\n"));
    return;
  }
  i = nextbot(par);
  if (i < 0) {
    dprintf(idx, STR("Unreachable bot.\n"));
    return;
  }
  simple_sprintf(x, STR("%d:%s@%s"), dcc[idx].sock, dcc[idx].nick, botnetnick);
  simple_sprintf(y, STR(":%d"), now);
  botnet_send_trace(i, x, par, y);
}

void cmd_binds(struct userrec *u, int idx, char *par)
{
  log(LCAT_COMMAND, STR("#%s# binds %s"), dcc[idx].nick, par);
  tell_binds(idx, par);
}

void cmd_banner(struct userrec *u, int idx, char *par)
{
  char s[1024];
  int i;

  if (!par[0]) {
    dprintf(idx, STR("Usage: banner <message>\n"));
    return;
  }
  simple_sprintf(s, STR("\007\007### Botwide:[%s] %s\n"), dcc[idx].nick, par);
  for (i = 0; i < dcc_total; i++)
    if (dcc[i].type->flags & DCT_MASTER)
      dprintf(i, "%s", s);
}

/* after messing with someone's user flags, make sure the dcc-chat flags
 * are set correctly */
int check_dcc_attrs(struct userrec *u, int oatr)
{
  int i,
    stat;
  char *p,
   *q,
    s[121];

  /* if it matches someone in the owner list, make sure he/she has +n */
  if (!u)
    return 0;
  /* make sure default owners are +n */
  if (owner[0]) {
    q = owner;
    p = strchr(q, ',');
    while (p) {
      strncpy0(s, q, p - q);
      rmspace(s);
      if (!strcasecmp(u->handle, s))
	u->flags = sanity_check(u->flags | USER_OWNER);
      q = p + 1;
      p = strchr(q, ',');
    }
    strcpy(s, q);
    rmspace(s);
    if (!strcasecmp(u->handle, s))
      u->flags = sanity_check(u->flags | USER_OWNER);
  }
  for (i = 0; i < dcc_total; i++) {
    if ((dcc[i].type->flags & DCT_MASTER)
	&& !strcasecmp(u->handle, dcc[i].nick)) {
      stat = dcc[i].status;
      if ((dcc[i].type == &DCC_CHAT) && ((u->flags & (USER_OP | USER_MASTER | USER_OWNER))
					 != (oatr & (USER_OP | USER_MASTER | USER_OWNER)))) {
	botnet_send_join_idx(i, -1);
      }
      if ((oatr & USER_MASTER) && !(u->flags & USER_MASTER)) {
	struct flag_record fr = { FR_CHAN | FR_ANYWH, 0, 0, 0, 0 };

	get_user_flagrec(u, &fr, NULL);
	dprintf(i, STR("*** POOF! ***\n"));
	dprintf(i, STR("You are no longer a master on this bot.\n"));
      }
      if (!(oatr & USER_MASTER) && (u->flags & USER_MASTER)) {
	dprintf(i, STR("*** POOF! ***\n"));
	dprintf(i, STR("You are now a master on this bot.\n"));
      }
      if (!(oatr & USER_OWNER) && (u->flags & USER_OWNER)) {
	dprintf(i, STR("@@@ POOF! @@@\n"));
	dprintf(i, STR("You are now an OWNER of this bot.\n"));
      }
      if ((oatr & USER_OWNER) && !(u->flags & USER_OWNER)) {
	dprintf(i, STR("@@@ POOF! @@@\n"));
	dprintf(i, STR("You are no longer an owner of this bot.\n"));
      }
      if ((stat & STAT_PARTY) && (u->flags & USER_OP))
	stat &= ~STAT_PARTY;
      if (!(stat & STAT_PARTY) && !(u->flags & USER_OP) && !(u->flags & USER_MASTER))
	stat |= STAT_PARTY;
      if ((stat & STAT_CHAT) && !(u->flags & USER_PARTY) && !(u->flags & USER_MASTER))
	stat &= ~STAT_CHAT;
      dcc[i].status = stat;
      /* check if they no longer have access to wherever they are */

      if (!(u->flags & (USER_PARTY | USER_MASTER | USER_OWNER))) {
	/* no +p = off partyline/chat */
	dprintf(i, STR("-+- POOF! -+-\n"));
	dprintf(i, STR("You no longer have party line access.\n\n"));
	do_boot(i, botnetnick, STR("No partyline access.\n\n"));
      }
#ifdef G_SILENT
      else if ((u->flags & USER_SILENT) && (dcc[i].u.chat->channel != -1)) {
	dprintf(i, STR("-+- POOF! -+-\n"));
	dprintf(i, STR("You no longer have party line chat access.\n"));
	dprintf(i, STR("Leaving chat mode...\n"));
	check_tcl_chpt(botnetnick, dcc[i].nick, dcc[i].sock, dcc[i].u.chat->channel);
	chanout_but(-1, dcc[i].u.chat->channel, STR("*** %s left the party line - no chat access.\n"), dcc[i].nick);
	Context;
	if (dcc[i].u.chat->channel < 100000)
	  botnet_send_part_idx(i, "");
	dcc[i].u.chat->channel = (-1);
      }
#endif
    }
    if ((dcc[i].type == &DCC_BOT) && !strcasecmp(u->handle, dcc[i].nick)) {
      if ((dcc[i].status & STAT_LEAF) && !(bot_hublevel(u) == 999))
	dcc[i].status &= ~(STAT_LEAF | STAT_WARNED);
      if (!(dcc[i].status & STAT_LEAF) && (bot_hublevel(u) == 999))
	dcc[i].status |= STAT_LEAF;
    }
  }
  return u->flags;
}

int check_dcc_chanattrs(struct userrec *u, char *chname, int chflags, int ochatr)
{
  int i,
    found = 0;
  struct chanset_t *chan;

  if (!u)
    return 0;
  chan = chanset;
  for (i = 0; i < dcc_total; i++) {
    if ((dcc[i].type->flags & DCT_MASTER) && !strcasecmp(u->handle, dcc[i].nick)) {
      if ((dcc[i].type == &DCC_CHAT) && ((chflags & (USER_OP | USER_MASTER | USER_OWNER))
					 != (ochatr & (USER_OP | USER_MASTER | USER_OWNER))))
	botnet_send_join_idx(i, -1);
      if ((ochatr & USER_MASTER) && !(chflags & USER_MASTER)) {
	dprintf(i, STR("*** POOF! ***\n"));
	dprintf(i, STR("You are no longer a master on %s.\n"), chname);
      }
      if (!(ochatr & USER_MASTER) && (chflags & USER_MASTER)) {
	dprintf(i, STR("*** POOF! ***\n"));
	dprintf(i, STR("You are now a master on %s.\n"), chname);
      }
      if (!(ochatr & USER_OWNER) && (chflags & USER_OWNER)) {
	dprintf(i, STR("@@@ POOF! @@@\n"));
	dprintf(i, STR("You are now an OWNER of %s.\n"), chname);
      }
      if ((ochatr & USER_OWNER) && !(chflags & USER_OWNER)) {
	dprintf(i, STR("@@@ POOF! @@@\n"));
	dprintf(i, STR("You are no longer an owner of %s.\n"), chname);
      }
      if (((ochatr & (USER_OP | USER_MASTER | USER_OWNER)) &&
	   (!(chflags & (USER_OP | USER_MASTER | USER_OWNER)))) || ((chflags & (USER_OP | USER_MASTER | USER_OWNER)) && (!(ochatr & (USER_OP | USER_MASTER | USER_OWNER))))) {
	struct flag_record fr = { FR_CHAN, 0, 0, 0, 0 };

	while (chan && !found) {
	  get_user_flagrec(u, &fr, chan->name);
	  if (fr.chan & (USER_OP | USER_MASTER | USER_OWNER))
	    found = 1;
	  else
	    chan = chan->next;
	}
      }
    }
  }
  return chflags;
}

void cmd_chattr(struct userrec *u, int idx, char *par)
{
  char *hand,
   *arg = NULL,
   *tmpchg = NULL,
   *chg = NULL,
    work[1024];
  struct chanset_t *chan = NULL;
  struct userrec *u2;
  struct flag_record pls = { 0, 0, 0, 0, 0 }, mns = {
  0, 0, 0, 0, 0}, user = {
  0, 0, 0, 0, 0};
  int fl = -1,
    of = 0,
    ocf = 0;

  log(LCAT_COMMAND, STR("#%s# chattr %s"), dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, STR("Usage: chattr <handle> [changes] [channel]\n"));
    return;
  }
  hand = newsplit(&par);
  u2 = get_user_by_handle(userlist, hand);
  if (!u2) {
    dprintf(idx, STR("No such user!\n"));
    return;
  }

  /* Parse args */
  if (par[0]) {
    arg = newsplit(&par);
    if (par[0]) {
      /* .chattr <handle> <changes> <channel> */
      chg = arg;
      arg = newsplit(&par);
      chan = findchan(arg);
    } else {
      chan = findchan(arg);
      /* ugly hack for modeless channels -- rtc */
      if (!(arg[0] == '+' && chan) && !(arg[0] != '+' && strchr(CHANMETA, arg[0]))) {
	/* .chattr <handle> <changes> */
	chg = arg;
	chan = NULL;		/* uh, !strchr (CHANMETA, channel[0]) && channel found?? */
	arg = NULL;
      }
      /* .chattr <handle> <channel>: nothing to do... */
    }
  }
  /* arg:  pointer to channel name, NULL if none specified
   * chan: pointer to channel structure, NULL if none found or none specified
   * chg:  pointer to changes, NULL if none specified
   */
  Assert(!(!arg && chan));
  if (arg && !chan) {
    dprintf(idx, STR("No channel record for %s.\n"), arg);
    return;
  }
  if (chg) {
    if (!arg && strpbrk(chg, "&|")) {
      /* .chattr <handle> *[&|]*: use console channel if found... */
      if (arg && !chan) {
	dprintf(idx, STR("No channel name specified\n"));
	return;
      }
    } else if (arg && !strpbrk(chg, "&|")) {
      Context;
      tmpchg = nmalloc(strlen(chg) + 2);
      strcpy(tmpchg, "|");
      strcat(tmpchg, chg);
      chg = tmpchg;
    }
  }
  par = arg;
  user.match = FR_GLOBAL;
  if (chan)
    user.match |= FR_CHAN;
  get_user_flagrec(u, &user, chan ? chan->name : 0);
  if (!chan && !glob_master(user)) {
    dprintf(idx, STR("You do not have Bot Master privileges.\n"));
    if (tmpchg)
      nfree(tmpchg);
    return;
  }
  if (chan && !glob_master(user) && !chan_master(user)) {
    dprintf(idx, STR("You do not have channel master privileges for channel %s\n"), par);
    if (tmpchg)
      nfree(tmpchg);
    return;
  }
  user.match &= fl;
  if (chg) {
    pls.match = user.match;
    break_down_flags(chg, &pls, &mns);
    /* no-one can change these flags on-the-fly */
    pls.global &=~(USER_BOT);
    mns.global &=~(USER_BOT);

    if (!isowner(u->handle)) {
      if (pls.global &USER_HUB)
	log(LCAT_WARNING, STR("%s attempted to give %s hub connect access"), dcc[idx].nick, u2->handle);
      if (mns.global &USER_HUB)
	log(LCAT_WARNING, STR("%s attempted to take away hub connect access from %s"), dcc[idx].nick, u2->handle);
      if (pls.global &USER_SU)
	log(LCAT_WARNING, STR("%s attempted to give %s su access"), dcc[idx].nick, u2->handle);
      if (mns.global &USER_SU)
	log(LCAT_WARNING, STR("%s attempted to take away su access from %s"), dcc[idx].nick, u2->handle);
      pls.global &=~(USER_HUB | USER_SU | USER_OWNER);
      mns.global &=~(USER_HUB | USER_SU | USER_OWNER);
    }
    if (!glob_owner(user)) {
      pls.global &=~(USER_OWNER | USER_MASTER | USER_UNSHARED);
      mns.global &=~(USER_OWNER | USER_MASTER | USER_UNSHARED);

      if (chan) {
	pls.chan &= ~USER_OWNER;
	mns.chan &= ~USER_OWNER;
      }
      if (!glob_master(user)) {
	pls.global &=USER_PARTY;
	mns.global &=USER_PARTY;

	if (!glob_master(user)) {
	  pls.global = 0;
	  mns.global = 0;
	}
      }
    }
    if (chan && !chan_owner(user) && !glob_owner(user)) {
      pls.chan &= ~USER_MASTER;
      mns.chan &= ~USER_MASTER;
      if (!chan_master(user) && !glob_master(user)) {
	pls.chan = 0;
	mns.chan = 0;
      }
    }
#ifdef LEAF
    pls.global &=~(USER_OWNER | USER_MASTER | USER_SU | USER_HUB);
    mns.global &=~(USER_OWNER | USER_MASTER | USER_SU | USER_HUB);
#endif
    get_user_flagrec(u2, &user, par);
    if (user.match & FR_GLOBAL) {
      of = user.global;
      user.global = sanity_check((user.global |pls.global) &~mns.global);

      user.udef_global = (user.udef_global | pls.udef_global)
	& ~mns.udef_global;
    }
    if (chan) {
      ocf = user.chan;
      user.chan = chan_sanity_check((user.chan | pls.chan) & ~mns.chan, user.global);

      user.udef_chan = (user.udef_chan | pls.udef_chan) & ~mns.udef_chan;
    }
    set_user_flagrec(u2, &user, par);
  }
  /* get current flags and display them */
  if (user.match & FR_GLOBAL) {
    user.match = FR_GLOBAL;
    if (chg)
      check_dcc_attrs(u2, of);
    get_user_flagrec(u2, &user, NULL);
    build_flags(work, &user, NULL);
    if (work[0] != '-')
      dprintf(idx, STR("Global flags for %s are now +%s\n"), hand, work);
    else
      dprintf(idx, STR("No global flags for %s.\n"), hand);
  }
  if (chan) {
    user.match = FR_CHAN;
    get_user_flagrec(u2, &user, par);

    if (chg)
      check_dcc_chanattrs(u2, chan->name, user.chan, ocf);
    build_flags(work, &user, NULL);
    if (work[0] != '-')
      dprintf(idx, STR("Channel flags for %s on %s are now +%s\n"), hand, chan->name, work);
    else
      dprintf(idx, STR("No flags for %s on %s.\n"), hand, chan->name);
#ifdef LEAF
    recheck_channel(chan, 0);
#endif
#ifdef HUB
    write_userfile(idx);
#endif
  }
  if (tmpchg)
    nfree(tmpchg);
}

/* !!
void cmd_wget(struct userrec *u, int idx, char *par) {
  if (strncmp(par, STR("-nolog "), 7)) {
    log(LCAT_COMMAND, STR("#%s# wget %s"), dcc[idx].nick, par);
  } else {
    newsplit(&par);
  }
  if (!par[0]) {
    dprintf(idx, STR("Active wget connections:\n"));
    for (i=0;i<dcc_total;i++) {
      if (dcc[i].type = &DCC_WGET) {
	
      }
    }
    return;
  }
  p=strstr(par, "//");
  if (!p) {
    dprintf(idx, STR("Invalid URL\n"));
    return;
  }
  *p=0;
  p+=2;
  if (strncmp(par, STR("http:"))) {
    dprintf(idx, STR("Only http URLs are supported\n"));
    return;
  }
  
}
*/

void cmd_chat(struct userrec *u, int idx, char *par)
{
  char *arg;
  int newchan,
    oldchan;

  log(LCAT_COMMAND, STR("#%s# chat %s"), dcc[idx].nick, par);
#ifdef G_SILENT
  if (u->flags & USER_SILENT) {
    dprintf(idx, STR("You don't have partyline access\n"));
    return;
  }
#endif
  arg = newsplit(&par);
  if (!strcasecmp(arg, STR("off"))) {
    /* turn chat off */
    if (dcc[idx].u.chat->channel < 0) {
      dprintf(idx, STR("You weren't in chat anyway!\n"));
      return;
    } else {
      dprintf(idx, STR("Leaving chat mode...\n"));
      check_tcl_chpt(botnetnick, dcc[idx].nick, dcc[idx].sock, dcc[idx].u.chat->channel);
      chanout_but(-1, dcc[idx].u.chat->channel, STR("*** %s left the party line.\n"), dcc[idx].nick);
      Context;
      if (dcc[idx].u.chat->channel < 100000)
	botnet_send_part_idx(idx, "");
    }
    dcc[idx].u.chat->channel = (-1);
  } else {
    if (arg[0] == '*') {
      if (((arg[1] < '0') || (arg[1] > '9'))) {
	if (arg[1] == 0)
	  newchan = 0;
	else {
	  newchan = -1;
	}
	if (newchan < 0) {
	  dprintf(idx, STR("No channel by that name.\n"));
	  return;
	}
      } else
	newchan = 100000 + atoi(arg + 1);
      if (newchan < 100000 || newchan > 199999) {
	dprintf(idx, STR("Channel # out of range: local channels must be *0-*99999\n"));
	return;
      }
    } else {
      if (((arg[0] < '0') || (arg[0] > '9')) && (arg[0])) {
	if (!strcasecmp(arg, "on"))
	  newchan = 0;
	else {
	  newchan = -1;
	}
	if (newchan < 0) {
	  dprintf(idx, STR("No channel by that name.\n"));
	  return;
	}
      } else
	newchan = atoi(arg);
      if ((newchan < 0) || (newchan > 99999)) {
	dprintf(idx, STR("Channel # out of range: must be between 0 and 99999.\n"));
	return;
      }
    }
    /* if coming back from being off the party line, make sure they're
     * not away */
    if ((dcc[idx].u.chat->channel < 0) && (dcc[idx].u.chat->away != NULL))
      not_away(idx);
    if (dcc[idx].u.chat->channel == newchan) {
      if (newchan == 0) {
	dprintf(idx, STR("You're already on the party line!\n"));
	return;
      } else {
	dprintf(idx, STR("You're already on channel %s%d!\n"), (newchan < 100000) ? "" : "*", newchan % 100000);
	return;
      }
    } else {
      oldchan = dcc[idx].u.chat->channel;
      if (oldchan >= 0)
	check_tcl_chpt(botnetnick, dcc[idx].nick, dcc[idx].sock, oldchan);
      if (oldchan == 0) {
	chanout_but(-1, 0, STR("*** %s left the party line.\n"), dcc[idx].nick);
	Context;
      } else if (oldchan > 0) {
	chanout_but(-1, oldchan, STR("*** %s left the channel.\n"), dcc[idx].nick);
	Context;
      }
      dcc[idx].u.chat->channel = newchan;
      if (newchan == 0) {
	dprintf(idx, STR("Entering the party line...\n"));
	chanout_but(-1, 0, STR("*** %s joined the party line.\n"), dcc[idx].nick);
	Context;
      } else {
	dprintf(idx, STR("Joining channel '%s'...\n"), arg);
	chanout_but(-1, newchan, STR("*** %s joined the channel.\n"), dcc[idx].nick);
	Context;
      }
      check_tcl_chjn(botnetnick, dcc[idx].nick, newchan, geticon(idx), dcc[idx].sock, dcc[idx].host);
      if (newchan < 100000)
	botnet_send_join_idx(idx, oldchan);
      else if (oldchan < 100000)
	botnet_send_part_idx(idx, "");
    }
  }
  /* new style autosave here too -- rtc, 09/28/1999 */
}

int exec_str(struct userrec *u, int idx, char *cmd) {
  char *out, *err, *p, *np;
  if (shell_exec(cmd, NULL, &out, &err)) {
    if (out) {
      dprintf(idx, STR("Result:\n"));
      p=out;
      while (p && p[0]) {
	np=strchr(p, '\n');
	if (np) 
	  *np++=0;
	dprintf(idx, STR("%s\n"), p);
	p=np;
      }
      dprintf(idx, "\n");
      nfree(out);
    }
    if (err) {
      dprintf(idx, STR("Errors:\n"));
      p=err;
      while (p && p[0]) {
	np=strchr(p, '\n');
	if (np) 
	  *np++=0;
	dprintf(idx, STR("%s\n"), p);
	p=np;
      }
      dprintf(idx, "\n");
      nfree(err);
    }
    return 1;
  }
  return 0;
}

#ifdef G_EXEC
void cmd_exec(struct userrec *u, int idx, char *par) {
  log(LCAT_COMMAND, STR("#%s# exec %s"), dcc[idx].nick, par);
  if (!isowner(u->handle)) {
    log(LCAT_WARNING, STR("%s attempted .exec %s"), dcc[idx].nick, par);
    dprintf(idx, STR("exec is only available to permanent owners\n"));
    return;
  }
  if (exec_str(u, idx, par)) 
    dprintf(idx, STR("Exec completed\n"));
  else
    dprintf(idx, STR("Exec failed\n"));
}
#endif

void cmd_w(struct userrec *u, int idx, char *par) {
  log(LCAT_COMMAND, STR("#%s# w"), dcc[idx].nick);
  if (!exec_str(u, idx, "w")) 
    dprintf(idx, STR("Exec failed\n"));
}

void cmd_ps(struct userrec *u, int idx, char *par) {
  char * buf;
  log(LCAT_COMMAND, STR("#%s# ps %s"), dcc[idx].nick, par);
  if (strchr(par, '|') || strchr(par, '<') || strchr(par, ';') || strchr(par, '>')) {
    log(LCAT_WARNING, STR("%s attempted .ps with pipe/semicolon in parameters: %s"), dcc[idx].nick, par);
    dprintf(idx, "No.");
    return;
  }
  buf=nmalloc(strlen(par)+4);
  sprintf(buf, STR("ps %s"), par);
  if (!exec_str(u, idx, buf)) 
    dprintf(idx, STR("Exec failed\n"));
  nfree(buf);
}


void cmd_last(struct userrec *u, int idx, char *par) {
  char user[20], buf[30];
  log(LCAT_COMMAND, STR("#%s# last %s"), dcc[idx].nick, par);
  if (strchr(par, '|') || strchr(par, '<') || strchr(par, ';') || strchr(par, '>')) {
    log(LCAT_WARNING, STR("%s attempted .last with pipe/semicolon in parameters: %s"), dcc[idx].nick, par);
    dprintf(idx, "No.");
    return;
  }
  if (par[0]) {
    strncpy0(user, par, sizeof(user));
  } else {
    strncpy0(user, cuserid(NULL), sizeof(user));
  }
  if (!user[0]) {
    dprintf(idx, STR("Can't determine user id for process\n"));
    return;
  }
  sprintf(buf, STR("last %s"), user);
  if (!exec_str(u, idx, buf))
    dprintf(idx, STR("Failed to execute /bin/sh last\n"));
}

int crontab_exists() {
  char buf[2048], *out=NULL;
  sprintf(buf, STR("crontab -l | grep \"%s\" | grep -v \"^#\""), binname);
  if (shell_exec(buf, NULL, &out, NULL)) {

    if (out && strstr(out, binname)) {
      nfree(out);
      return 1;
    } else {
      if (out)
	nfree(out);
      return 0;
    }
  } else
    return (-1);
}

void crontab_show(struct userrec * u, int idx) {
  dprintf(idx, STR("Showing current crontab:\n"));
  if (!exec_str(u, idx, STR("crontab -l | grep -v \"^#\"")))
    dprintf(idx, STR("Exec failed"));
}

void crontab_del() {
  char * tmpfile, *p, buf[2048];
  tmpfile=nmalloc(strlen(binname)+100);
  strcpy(tmpfile, binname);
  if (!(p=strrchr(tmpfile, '/'))) 
    return;
  p++;
  strcpy(p, STR(".ctb"));
  sprintf(buf, STR("crontab -l | grep -v \"%s\" | grep -v \"^#\" | grep -v \"^\\$\" > %s"), binname, tmpfile);
  if (shell_exec(buf, NULL, NULL, NULL)) {
    sprintf(buf, STR("crontab %s"), tmpfile);
    shell_exec(buf, NULL, NULL, NULL);
  }
  unlink(tmpfile);
}

void crontab_create(int interval) {
  char * tmpfile, *p, buf[2048];
  tmpfile=nmalloc(strlen(binname)+100);
  strcpy(tmpfile, binname);
  if (!(p=strrchr(tmpfile, '/'))) 
    return;
  p++;
  strcpy(p, STR(".ctb"));
  sprintf(buf, STR("crontab -l | grep -v \"%s\" | grep -v \"^#\" | grep -v \"^\\$\"> %s"), binname, tmpfile);
  if (shell_exec(buf, NULL, NULL, NULL)) {
    FILE * f=fopen(tmpfile, "a");
    if (f) {
      buf[0]=0;
      if (interval==1) 
	strcpy(buf, "*");
      else {
	int i=1;
	int si=random() % interval;
	while (i<60) {
	  if (buf[0])
	    sprintf(buf + strlen(buf), ",%i", (i+si) % 60);
	  else
	    sprintf(buf, "%i", (i+si) % 60);
	  i+=interval;
	}
      }
      sprintf(buf + strlen(buf), STR(" * * * * %s > /dev/null 2>&1"), binname);
      fseek(f, 0, SEEK_END);
      fprintf(f, STR("\n%s\n"), buf);
      fclose(f);
      sprintf(buf, STR("crontab %s"), tmpfile);
      shell_exec(buf, NULL, NULL, NULL);
    }
  }
  unlink(tmpfile);
  

}

void cmd_crontab(struct userrec *u, int idx, char *par) {
  char * code;
  int i;
  log(LCAT_COMMAND, STR("#%s# crontab %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: crontab status|delete|show|new [interval]\n"));
    return;
  }
  code=newsplit(&par);
  if (!strcmp(code, STR("status"))) {
    i=crontab_exists();
    if (!i) 
      dprintf(idx, STR("No crontab\n"));
    else if (i==1) 
      dprintf(idx, STR("Crontabbed\n"));
    else
      dprintf(idx, STR("Error checking crontab status\n"));
  } else if (!strcmp(code, STR("show"))) {
    crontab_show(u, idx);
  } else if (!strcmp(code, STR("delete"))) {
    crontab_del();
    i=crontab_exists();
    if (!i) 
      dprintf(idx, STR("No crontab\n"));
    else if (i==1) 
      dprintf(idx, STR("Crontabbed\n"));
    else
      dprintf(idx, STR("Error checking crontab status\n"));
  } else if (!strcmp(code, STR("new"))) {
    i=atoi(par);
    if ((i<=0) || (i>60))
      i=10;
    crontab_create(i);
    i=crontab_exists();
    if (!i) 
      dprintf(idx, STR("No crontab\n"));
    else if (i==1) 
      dprintf(idx, STR("Crontabbed\n"));
    else
      dprintf(idx, STR("Error checking crontab status\n"));
  } else {
    dprintf(idx, STR("Usage: crontab status|delete|show|new [interval]\n"));
  }
}

void cmd_echo(struct userrec *u, int idx, char *par)
{

  if (!par[0]) {
    dprintf(idx, STR("Echo is currently %s.\n"), dcc[idx].status & STAT_ECHO ? "on" : STR("off"));
    return;
  }
  if (!strcasecmp(par, "on")) {
    dprintf(idx, STR("Echo turned on.\n"));
    dcc[idx].status |= STAT_ECHO;
  } else if (!strcasecmp(par, STR("off"))) {
    dprintf(idx, STR("Echo turned off.\n"));
    dcc[idx].status &= ~STAT_ECHO;
  } else {
    dprintf(idx, STR("Usage: echo <on/off>\n"));
    return;
  }
}

int stripmodes(char *s)
{
  int res = 0;

  for (; *s; s++)
    switch (tolower(*s)) {
    case 'b':
      res |= STRIP_BOLD;
      break;
    case 'c':
      res |= STRIP_COLOR;
      break;
    case 'r':
      res |= STRIP_REV;
      break;
    case 'u':
      res |= STRIP_UNDER;
      break;
    case 'a':
      res |= STRIP_ANSI;
      break;
    case 'g':
      res |= STRIP_BELLS;
      break;
    case '*':
      res |= STRIP_ALL;
      break;
    }
  return res;
}

char *stripmasktype(int x)
{
  static char s[20];
  char *p = s;

  if (x & STRIP_BOLD)
    *p++ = 'b';
  if (x & STRIP_COLOR)
    *p++ = 'c';
  if (x & STRIP_REV)
    *p++ = 'r';
  if (x & STRIP_UNDER)
    *p++ = 'u';
  if (x & STRIP_ANSI)
    *p++ = 'a';
  if (x & STRIP_BELLS)
    *p++ = 'g';
  if (p == s)
    *p++ = '-';
  *p = 0;
  return s;
}

char *stripmaskname(int x)
{
  static char s[161];
  int i = 0;

  s[i] = 0;
  if (x & STRIP_BOLD)
    i += my_strcpy(s + i, STR("bold, "));
  if (x & STRIP_COLOR)
    i += my_strcpy(s + i, STR("color, "));
  if (x & STRIP_REV)
    i += my_strcpy(s + i, STR("reverse, "));
  if (x & STRIP_UNDER)
    i += my_strcpy(s + i, STR("underline, "));
  if (x & STRIP_ANSI)
    i += my_strcpy(s + i, STR("ansi, "));
  if (x & STRIP_BELLS)
    i += my_strcpy(s + i, STR("bells, "));
  if (!i)
    strcpy(s, STR("none"));
  else
    s[i - 2] = 0;
  return s;
}

void cmd_strip(struct userrec *u, int idx, char *par)
{
  char *nick,
   *changes,
   *c,
    s[2];
  int dest = 0,
    i,
    pls,
    md,
    ok = 0;

  log(LCAT_COMMAND, STR("#%s# strip %s"), dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, STR("Your current strip settings are: %s (%s)\n"), stripmasktype(dcc[idx].u.chat->strip_flags), stripmaskname(dcc[idx].u.chat->strip_flags));
    return;
  }
  nick = newsplit(&par);
  if ((nick[0] != '+') && (nick[0] != '-') && u && (u->flags & USER_MASTER)) {
    for (i = 0; i < dcc_total; i++)
      if (!strcasecmp(nick, dcc[i].nick) && (dcc[i].type == &DCC_CHAT) && !ok) {
	ok = 1;
	dest = i;
      }
    if (!ok) {
      dprintf(idx, STR("No such user on the party line!\n"));
      return;
    }
    changes = par;
  } else {
    changes = nick;
    nick = "";
    dest = idx;
  }
  c = changes;
  if ((c[0] != '+') && (c[0] != '-'))
    dcc[dest].u.chat->strip_flags = 0;
  s[1] = 0;
  for (pls = 1; *c; c++) {
    switch (*c) {
    case '+':
      pls = 1;
      break;
    case '-':
      pls = 0;
      break;
    default:
      s[0] = *c;
      md = stripmodes(s);
      if (pls == 1)
	dcc[dest].u.chat->strip_flags |= md;
      else
	dcc[dest].u.chat->strip_flags &= ~md;
    }
  }
  if (dest == idx) {
    dprintf(idx, STR("Your strip settings are: %s (%s)\n"), stripmasktype(dcc[idx].u.chat->strip_flags), stripmaskname(dcc[idx].u.chat->strip_flags));
  } else {
    dprintf(idx, STR("Strip setting for %s: %s (%s)\n"), dcc[dest].nick, stripmasktype(dcc[dest].u.chat->strip_flags), stripmaskname(dcc[dest].u.chat->strip_flags));
    dprintf(dest, STR("%s set your strip settings to: %s (%s)\n"), dcc[idx].nick, stripmasktype(dcc[dest].u.chat->strip_flags), stripmaskname(dcc[dest].u.chat->strip_flags));
  }
  /* new style autosave here too -- rtc, 09/28/1999 */
}

void cmd_su(struct userrec *u, int idx, char *par)
{
  int atr = u ? u->flags : 0;
  struct flag_record fr = { FR_ANYWH | FR_CHAN | FR_GLOBAL, 0, 0, 0, 0 };

  Context;
  log(LCAT_COMMAND, STR("#%s# su %s"), dcc[idx].nick, par);
  u = get_user_by_handle(userlist, par);

  if (!par[0])
    dprintf(idx, STR("Usage: su <user>\n"));
  else if (!u)
    dprintf(idx, STR("No such user.\n"));
  else if (u->flags & USER_BOT)
    dprintf(idx, STR("Can't su to a bot... then again, why would you wanna?\n"));
  else if (dcc[idx].u.chat->su_nick)
    dprintf(idx, STR("You cannot currently double .su, try .su'ing directly\n"));
  else {
    get_user_flagrec(u, &fr, NULL);
    if (!glob_party(fr))
      dprintf(idx, STR("No party line access permitted for %s.\n"), par);
    else {
      correct_handle(par);
      if (!(atr & USER_OWNER) || ((atr & USER_OWNER) && !(isowner(dcc[idx].nick))) || ((u->flags & USER_OWNER) && (isowner(par)))) {
	/* This check is only important for non-owners */
	if (u_pass_match(u, "-")) {
	  dprintf(idx, STR("No password set for user. You may not .su to them\n"));
	  return;
	}
	if (dcc[idx].u.chat->channel < 100000)
	  botnet_send_part_idx(idx, "");
	chanout_but(-1, dcc[idx].u.chat->channel, STR("*** %s left the party line.\n"), dcc[idx].nick);
	Context;
	/* store the old nick in the away section, for weenies who can't get
	 * their password right ;) */
	if (dcc[idx].u.chat->away != NULL)
	  nfree(dcc[idx].u.chat->away);
	dcc[idx].u.chat->away = get_data_ptr(strlen(dcc[idx].nick) + 1);
	strcpy(dcc[idx].u.chat->away, dcc[idx].nick);
	dcc[idx].u.chat->su_nick = get_data_ptr(strlen(dcc[idx].nick) + 1);
	strcpy(dcc[idx].u.chat->su_nick, dcc[idx].nick);
	dcc[idx].user = u;
	strcpy(dcc[idx].nick, par);
	dprintf(idx, STR("Enter password for %s%s\n"), par, (dcc[idx].status & STAT_TELNET) ? "\377\373\001" : "");
	dcc[idx].type = &DCC_CHAT_PASS;
      } else if (atr & USER_OWNER) {
	if (dcc[idx].u.chat->channel < 100000)
	  botnet_send_part_idx(idx, "");
	chanout_but(-1, dcc[idx].u.chat->channel, STR("*** %s left the party line.\n"), dcc[idx].nick);
	dprintf(idx, STR("Setting your username to %s.\n"), par);
	dcc[idx].u.chat->su_nick = get_data_ptr(strlen(dcc[idx].nick) + 1);
	strcpy(dcc[idx].u.chat->su_nick, dcc[idx].nick);
	dcc[idx].user = u;
	strcpy(dcc[idx].nick, par);
	dcc_chatter(idx);
      }
    }
  }
}

void cmd_fixcodes(struct userrec *u, int idx, char *par)
{
  if (dcc[idx].status & STAT_ECHO) {
    dcc[idx].status |= STAT_TELNET;
    dcc[idx].status &= ~STAT_ECHO;
    dprintf(idx, STR("Turned on telnet codes\n"));
    log(LCAT_COMMAND, STR("#%s# fixcodes (telnet on)"), dcc[idx].nick);
    return;
  }
  if (dcc[idx].status & STAT_TELNET) {
    dcc[idx].status |= STAT_ECHO;
    dcc[idx].status &= ~STAT_TELNET;
    dprintf(idx, STR("Turned off telnet codes\n"));
    log(LCAT_COMMAND, STR("#%s# fixcodes (telnet off)"), dcc[idx].nick);
    return;
  }
}

void cmd_page(struct userrec *u, int idx, char *par)
{
  int a;

  log(LCAT_COMMAND, STR("#%s# page %s"), dcc[idx].nick, par);

  if (!par[0]) {
    if (dcc[idx].status & STAT_PAGE) {
      dprintf(idx, STR("Currently paging outputs to %d lines.\n"), dcc[idx].u.chat->max_line);
    } else
      dprintf(idx, STR("You dont have paging on.\n"));
    return;
  }
  a = atoi(par);
  if (!strcasecmp(par, STR("off")) || (a == 0 && par[0] == 0)) {
    dcc[idx].status &= ~STAT_PAGE;
    dcc[idx].u.chat->max_line = 0x7ffffff;	/* flush_lines needs this */
    while (dcc[idx].u.chat->buffer)
      flush_lines(idx, dcc[idx].u.chat);
    dprintf(idx, STR("Paging turned off.\n"));
  } else if (a > 0) {
    dprintf(idx, STR("Paging turned on, stopping every %d lines.\n"), a);
    dcc[idx].status |= STAT_PAGE;
    dcc[idx].u.chat->max_line = a;
    dcc[idx].u.chat->line_count = 0;
    dcc[idx].u.chat->current_lines = 0;
    return;
  } else {
    dprintf(idx, STR("Usage: page <off or #>\n"));
    return;
  }
  /* new style autosave here too -- rtc, 09/28/1999 */
}

#ifdef G_USETCL

/* evaluate a Tcl command, send output to a dcc user */
void cmd_tcl(struct userrec *u, int idx, char *msg)
{
  int code;

  if (!(isowner(dcc[idx].nick)) && (must_be_owner)) {
    dprintf(idx, STR("What?"));
    return;
  }
  debug1(STR("tcl: evaluate (.tcl): %s"), msg);
  code = Tcl_GlobalEval(interp, msg);
  if (code == TCL_OK)
    dumplots(idx, STR("Tcl: "), interp->result);
  else
    dumplots(idx, STR("TCL error: "), interp->result);
}

/* perform a 'set' command */
void cmd_set(struct userrec *u, int idx, char *msg)
{
  int code;
  char s[512];

  log(LCAT_COMMAND, STR("#%s# set %s"), dcc[idx].nick, msg);

  if (!(isowner(dcc[idx].nick)) && (must_be_owner)) {
    dprintf(idx, STR("What?"));
    return;
  }
  strcpy(s, STR("set "));
  if (!msg[0]) {
    strcpy(s, STR("info globals"));
    Tcl_Eval(interp, s);
    dumplots(idx, STR("global vars: "), interp->result);
    return;
  }
  strcpy(s + 4, msg);
  code = Tcl_Eval(interp, s);
  if (code == TCL_OK) {
    if (!strchr(msg, ' '))
      dumplots(idx, STR("currently: "), interp->result);
    else
      dprintf(idx, STR("Ok, set.\n"));
  } else
    dprintf(idx, STR("Error: %s\n"), interp->result);
}
#endif

void cmd_pls_ignore(struct userrec *u, int idx, char *par)
{
  char *who;
  char s[UHOSTLEN];

  log(LCAT_COMMAND, STR("#%s# +ignore %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: +ignore <hostmask> [comment]\n"));
    return;
  }
  who = newsplit(&par);
  remove_gunk(who);
  if (!par[0])
    par = STR("requested");
  else if (strlen(par) > 65)
    par[65] = 0;
  if (strlen(who) > UHOSTMAX - 4)
    who[UHOSTMAX - 4] = 0;
  /* fix missing ! or @ BEFORE continuing - sounds familiar */
  if (!strchr(who, '!')) {
    if (!strchr(who, '@'))
      simple_sprintf(s, STR("%s!*@*"), who);
    else
      simple_sprintf(s, STR("*!%s"), who);
  } else if (!strchr(who, '@'))
    simple_sprintf(s, STR("%s@*"), who);
  else
    strcpy(s, who);
  if (match_ignore(s)) {
    dprintf(idx, STR("That already matches an existing ignore.\n"));
    return;
  }
  dprintf(idx, STR("Now ignoring: %s (%s)\n"), s, par);
  addignore(s, dcc[idx].nick, par, 0L);
}

void cmd_mns_ignore(struct userrec *u, int idx, char *par)
{
  char buf[UHOSTLEN];

  log(LCAT_COMMAND, STR("#%s# -ignore %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: -ignore <hostmask | ignore #>\n"));
    return;
  }
  strncpy0(buf, par, UHOSTMAX);
  if (delignore(buf)) {
    dprintf(idx, STR("No longer ignoring: %s\n"), buf);
  } else
    dprintf(idx, STR("Can't find that ignore.\n"));
}

void cmd_ignores(struct userrec *u, int idx, char *par)
{
  log(LCAT_COMMAND, STR("#%s# ignores %s"), dcc[idx].nick, par);
  tell_ignores(idx, par);
}

void cmd_pls_user(struct userrec *u, int idx, char *par)
{
  char *handle,
   *host;

  log(LCAT_COMMAND, STR("#%s# +user %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: +user <handle> <hostmask>\n"));
    return;
  }
  handle = newsplit(&par);
  host = newsplit(&par);
  if (strlen(handle) > HANDLEN)
    handle[HANDLEN] = 0;	/* max len = XX */
  if (get_user_by_handle(userlist, handle))
    dprintf(idx, STR("Someone already exists by that name.\n"));
  else if (strchr(BADNICKCHARS, handle[0]) != NULL)
    dprintf(idx, STR("You can't start a nick with '%c'.\n"), handle[0]);
  else if (!strcasecmp(handle, botnetnick))
    dprintf(idx, STR("Hey! That's MY name!\n"));
  else {
    struct userrec *u2;
    char tmp[50];
    int i;
    userlist = adduser(userlist, handle, host, "-", USER_FRIEND | USER_PARTY);
    u2 = get_user_by_handle(userlist, handle);
    sprintf(tmp, STR("%lu %s"), time(NULL), u->handle);
    set_user(&USERENTRY_ADDED, u2, tmp);
    dprintf(idx, STR("Added %s (%s) as +fp.\n"), handle, host);
    while (par[0]) {
      host=newsplit(&par);
      set_user(&USERENTRY_HOSTS, u2, host);
      dprintf(idx, STR("Added host %s to %s.\n"), host, handle);
    }
    for (i=0;i<8;i++)
      tmp[i] = (random() % 2) * 32 + random() % 26 + 65;
    tmp[8]=0;
    set_user(&USERENTRY_PASS, u2, tmp);
    dprintf(idx, STR("%s's password set to \002%s\002.\n"), handle, tmp);
#ifdef HUB
    write_userfile(idx);
#endif
  }
}

void cmd_mns_user(struct userrec *u, int idx, char *par)
{
  char *handle;
  struct userrec *u2;

  log(LCAT_COMMAND, STR("#%s# -user %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: -user <nick>\n"));
    return;
  }
  handle = newsplit(&par);
  u2 = get_user_by_handle(userlist, handle);
  if (!u2 || !u) {
    dprintf(idx, STR("No such user!\n"));
    return;
  }
  if (!strcmp(u2->handle, u->handle)) {
    dprintf(idx, STR("You can't -user yourself\n"));
    return;
  }
  if (isowner(u2->handle)) {
    dprintf(idx, STR("Can't remove the permanent bot owner!\n"));
    return;
  }
  if ((u2->flags & USER_OWNER) && !(u->flags & USER_OWNER)) {
    dprintf(idx, STR("Can't remove the bot owner!\n"));
    return;
  }
  if ((u2->flags & USER_BOT) && !(u->flags & USER_OWNER)) {
    dprintf(idx, STR("You can't remove shared bots.\n"));
    return;
  }
  if (deluser(handle)) {
    dprintf(idx, STR("Deleted %s.\n"), handle);
#ifdef HUB
    write_userfile(idx);
#endif
  } else
    dprintf(idx, STR("Failed.\n"));
}

void cmd_pls_host(struct userrec *u, int idx, char *par)
{
  char *handle,
   *host;
  struct userrec *u2;
  struct list_type *q;
  struct flag_record fr = { FR_CHAN | FR_ANYWH, 0, 0, 0, 0 };

  Context;
  log(LCAT_COMMAND, STR("#%s# +host %s"), dcc[idx].nick, par);
  handle = newsplit(&par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: +host <handle> <newhostmask>\n"));
    return;
  }
  host = newsplit(&par);
  u2 = get_user_by_handle(userlist, handle);
  if (!u2) {
    dprintf(idx, STR("No such user.\n"));
    return;
  }
  for (q = get_user(&USERENTRY_HOSTS, u); q; q = q->next)
    if (!strcasecmp(q->extra, host)) {
      dprintf(idx, STR("That hostmask is already there.\n"));
      return;
    }
  get_user_flagrec(u, &fr, NULL);
  if (!(u->flags & USER_OWNER) && (u2->flags & USER_BOT)) {
    dprintf(idx, STR("You can't add hostmasks to share-bots.\n"));
    return;
  }
  if ((u2->flags & USER_OWNER) && !(u->flags & USER_OWNER) && strcasecmp(handle, dcc[idx].nick)) {
    dprintf(idx, STR("Can't add hostmasks to the bot owner.\n"));
    return;
  }
  addhost_by_handle(handle, host);
  dprintf(idx, STR("Added '%s' to %s\n"), host, handle);
}

void cmd_mns_host(struct userrec *u, int idx, char *par)
{
  char *handle,
   *host;
  struct userrec *u2;
  struct flag_record fr = { FR_CHAN | FR_ANYWH, 0, 0, 0, 0 };

  log(LCAT_COMMAND, STR("#%s# -host %s"), dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, STR("Usage: -host [handle] <hostmask>\n"));
    return;
  }
  handle = newsplit(&par);
  if (par[0]) {
    host = newsplit(&par);
    u2 = get_user_by_handle(userlist, handle);
  } else {
    host = handle;
    handle = dcc[idx].nick;
    u2 = u;
  }
  if (!u2 || !u) {
    dprintf(idx, STR("No such user.\n"));
    return;
  }
  if (strcasecmp(handle, dcc[idx].nick)) {
    get_user_flagrec(u, &fr, NULL);
    if (!(u2->flags & USER_BOT) && !(u->flags & USER_MASTER) && !chan_master(fr)) {
      dprintf(idx, STR("You can't remove hostmasks from non-bots.\n"));
      return;
    } else if ((u2->flags & USER_BOT) && !(u->flags & USER_OWNER)) {
      dprintf(idx, STR("You can't remove hostmask from a shared bot.\n"));
      return;
    } else if ((u2->flags & USER_OWNER) && !(u->flags & USER_OWNER) && (u2 != u)) {
      dprintf(idx, STR("Can't remove hostmasks from the bot owner.\n"));
      return;
    } else if (!(u->flags & USER_MASTER) && !chan_master(fr)) {
      dprintf(idx, STR("Permission denied.\n"));
      return;
    }
  }
  if (delhost_by_handle(handle, host)) {
    dprintf(idx, STR("Removed '%s' from %s\n"), host, handle);
  } else
    dprintf(idx, STR("Failed.\n"));
}

/* netserver */
void cmd_netserver(struct userrec * u, int idx, char * par) {
  log(LCAT_COMMAND, STR("#%s# netserver"), dcc[idx].nick);
  botnet_send_cmd_broad(-1, botnetnick, u->handle, idx, STR("cursrv"));
}

void cmd_botserver(struct userrec * u, int idx, char * par) {
  log(LCAT_COMMAND, STR("#%s# botserver %s"), dcc[idx].nick, par);
  if (!par || !par[0]) {
    dprintf(idx, STR("Usage: botserver <botname>\n"));
    return;
  }
  if (nextbot(par)<0) {
    dprintf(idx, STR("%s isn't a linked bot\n"), par);
  }
  botnet_send_cmd(botnetnick, par, u->handle, idx, STR("cursrv"));
}


void rcmd_cursrv(char * fbot, char * fhand, char * fidx) {
#ifdef LEAF
  char tmp[2048];
  struct server_list * x = serverlist;
  int i=curserv;
  while ((i>0) && (x)) {
    x=x->next;
    i--;
  }
  if (server_online && x)
    sprintf(tmp, STR("Currently: %s:%i"), x->name, x->port);
  else if (server_online)
    sprintf(tmp, STR("Currently: %s"), cursrvname);
  else
    sprintf(tmp, STR("Currently: none"));
  botnet_send_cmdreply(botnetnick, fbot, fhand, fidx, tmp);
#endif
}


/* netversion */
void cmd_netversion(struct userrec * u, int idx, char * par) {
  log(LCAT_COMMAND, STR("#%s# netversion"), dcc[idx].nick);
  botnet_send_cmd_broad(-1, botnetnick, u->handle, idx, STR("ver"));
}

void cmd_botversion(struct userrec * u, int idx, char * par) {
  log(LCAT_COMMAND, STR("#%s# botversion %s"), dcc[idx].nick, par);
  if (!par || !par[0]) {
    dprintf(idx, STR("Usage: botversion <botname>\n"));
    return;
  }
  if (nextbot(par)<0) {
    dprintf(idx, STR("%s isn't a linked bot\n"), par);
  }
  botnet_send_cmd(botnetnick, par, u->handle, idx, STR("ver"));
}

void rcmd_ver(char * fbot, char * fhand, char * fidx) {
  char tmp[2048];
#ifdef HAVE_UNAME
  struct utsname un;
#endif
  sprintf(tmp, STR("%s "), ver);
#ifdef HAVE_UNAME
  if (uname(&un) < 0) {
#endif
    strcat(tmp, STR("(unknown OS)"));
#ifdef HAVE_UNAME
  } else {
    sprintf(tmp + strlen(tmp), STR("%s %s (%s)"), un.sysname, un.release, un.machine);
  }
#endif
  botnet_send_cmdreply(botnetnick, fbot, fhand, fidx, tmp);
}


/* netnick, botnick */
void cmd_netnick(struct userrec * u, int idx, char * par) {
  log(LCAT_COMMAND, STR("#%s# netnick"), dcc[idx].nick);
  botnet_send_cmd_broad(-1, botnetnick, u->handle, idx, STR("curnick"));  
}

void cmd_botnick(struct userrec * u, int idx, char * par) {
  log(LCAT_COMMAND, STR("#%s# botnick %s"), dcc[idx].nick, par);
  if (!par || !par[0]) {
    dprintf(idx, STR("Usage: botnick <botname>\n"));
    return;
  }
  if (nextbot(par)<0) {
    dprintf(idx, STR("%s isn't a linked bot\n"), par);
  }
  botnet_send_cmd(botnetnick, par, u->handle, idx, STR("curnick"));
}

void rcmd_curnick(char * fbot, char * fhand, char * fidx) {
#ifdef LEAF
  char tmp[1024];
  if (server_online) 
    sprintf(tmp, STR("Currently: %s"), botname);
  else
    sprintf(tmp, STR("Currently: %s (not online)"), botname);
  botnet_send_cmdreply(botnetnick, fbot, fhand, fidx, tmp);  
#endif
}


/* netmsg, botmsg */
void cmd_botmsg(struct userrec * u, int idx, char * par) {
  char * tnick, * tbot;
  char tmp[1024];
  log(LCAT_COMMAND, STR("#%s# botmsg %s"), dcc[idx].nick, par);
  tbot=newsplit(&par);
  tnick=newsplit(&par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: botmsg <botname> <nick|#channel> <message>\n"));
    return;
  }
  if (nextbot(tbot)<0) {
    dprintf(idx, STR("No such bot linked\n"));
    return;
  }
  sprintf(tmp, STR("msg %s %s"), tnick, par);
  botnet_send_cmd(botnetnick, tbot, u->handle, idx, tmp);
}

void cmd_netmsg(struct userrec * u, int idx, char * par) {
  char * tnick;
  char tmp[1024];
  log(LCAT_COMMAND, STR("#%s# netmsg %s"), dcc[idx].nick, par);
  tnick=newsplit(&par);
  if (!par[0]) {
    dprintf(idx, "Usage: netmsg <nick|#channel> <message>\n");
    return;
  }
  sprintf(tmp, STR("msg %s %s"), tnick, par);
  botnet_send_cmd_broad(-1, botnetnick, u->handle, idx, tmp);
}

void rcmd_msg(char * tobot, char * frombot, char * fromhand, char * fromidx, char * par) { 
#ifdef LEAF
  char buf[1024], *nick;
  nick=newsplit(&par);
  dprintf(DP_SERVER, STR("PRIVMSG %s :%s\n"), nick, par);
  if (!strcmp(tobot, botnetnick)) {
    sprintf(buf, STR("Sent message to %s"), nick);
    botnet_send_cmdreply(botnetnick, frombot, fromhand, fromidx, buf);
  }
#endif
}


/* netlag */
void cmd_netlag(struct userrec * u, int idx, char * par) {
  struct timeval tv;
  time_t tm;
  char tmp[64];
  log(LCAT_COMMAND, STR("#%s# netlag"), dcc[idx].nick);
  gettimeofday(&tv, NULL);
  tm = (tv.tv_sec % 10000) * 100 + (tv.tv_usec * 100) / (1000000);
  sprintf(tmp, STR("ping %lu"), tm);
  dprintf(idx, STR("Sent ping to all linked bots\n"));
  botnet_send_cmd_broad(-1, botnetnick, u->handle, idx, tmp);
}

void rcmd_ping(char * frombot, char *fromhand, char * fromidx, char * par) {
  char tmp[64];
  sprintf(tmp, STR("pong %s"), par);
  botnet_send_cmd(botnetnick, frombot, fromhand, atoi(fromidx), tmp);
}

void rcmd_pong(char *frombot, char *fromhand, char *fromidx, char *par) {
  int i=atoi(fromidx);
  if ((i>=0) && (i<dcc_total) && (dcc[i].type==&DCC_CHAT) && (!strcmp(dcc[i].nick, fromhand))) {
    struct timeval tv;
    time_t tm;
    gettimeofday(&tv, NULL);
    tm = ((tv.tv_sec % 10000) * 100 + (tv.tv_usec * 100) / (1000000)) - atoi(par);
    dprintf(i, STR("Pong from %s: %i.%i seconds\n"), frombot, (tm / 100), (tm % 100));
  }
}


/* exec commands */
void cmd_botw(struct userrec * u, int idx, char * par) {
  char * tbot, tmp[128];
  log(LCAT_COMMAND, STR("#%s# botw %s"), dcc[idx].nick, par);
  tbot=newsplit(&par);
  if (!tbot[0]) {
    dprintf(idx, STR("Usage: botw <botname>\n"));
    return;
  }
  if (nextbot(tbot)<0) {
    dprintf(idx, STR("No such linked bot\n"));
    return;
  }
  strcpy(tmp, STR("exec w"));
  botnet_send_cmd(botnetnick, tbot, dcc[idx].nick, idx, tmp);
}

void cmd_netw(struct userrec * u, int idx, char * par) {
  char tmp[128];
  log(LCAT_COMMAND, STR("#%s# netw"), dcc[idx].nick);
  strcpy(tmp, STR("exec w"));
  botnet_send_cmd_broad(-1, botnetnick, dcc[idx].nick, idx, tmp);
}

void cmd_botps(struct userrec * u, int idx, char * par) {
  char * tbot, buf[1024];
  log(LCAT_COMMAND, STR("#%s# botps %s"), dcc[idx].nick, par);
  tbot=newsplit(&par);
  if (!tbot[0]) {
    dprintf(idx, STR("Usage: botps <botname> [ps-parameters]\n"));
    return;
  }
  if (strchr(par, '|') || strchr(par, '<') || strchr(par, ';') || strchr(par, '>')) {
    log(LCAT_WARNING, STR("%s attempted .botps with pipe/semicolon in parameters: %s"), dcc[idx].nick, par);
    dprintf(idx, STR("No."));
    return;
  }
  if (nextbot(tbot)<0) {
    dprintf(idx, STR("No such linked bot\n"));
    return;
  }
  sprintf(buf, STR("exec ps %s"), par);
  botnet_send_cmd(botnetnick, tbot, dcc[idx].nick, idx, buf);
}

void cmd_netps(struct userrec * u, int idx, char * par) {
  char buf[1024];
  log(LCAT_COMMAND, STR("#%s# netps %s"), dcc[idx].nick, par);
  if (strchr(par, '|') || strchr(par, '<') || strchr(par, ';') || strchr(par, '>')) {
    log(LCAT_WARNING, STR("%s attempted .netps with pipe/semicolon in parameters: %s"), dcc[idx].nick, par);
    dprintf(idx, STR("No."));
    return;
  }
  sprintf(buf, STR("exec ps %s"), par);
  botnet_send_cmd_broad(-1, botnetnick, dcc[idx].nick, idx, buf);
}


void cmd_botlast(struct userrec * u, int idx, char * par) {
  char * tbot, buf[1024];
  log(LCAT_COMMAND, STR("#%s# botlast %s"), dcc[idx].nick, par);
  tbot=newsplit(&par);
  if (!tbot[0]) {
    dprintf(idx, STR("Usage: botlast <botname> [userid]\n"));
    return;
  }
  if (strchr(par, '|') || strchr(par, '<') || strchr(par, ';') || strchr(par, '>')) {
    log(LCAT_WARNING, STR("%s attempted .botlast with pipe/semicolon in parameters: %s"), dcc[idx].nick, par);
    dprintf(idx, STR("No."));
    return;
  }
  if (nextbot(tbot)<0) {
    dprintf(idx, STR("No such linked bot\n"));
    return;
  }
  sprintf(buf, STR("exec last %s"), par);
  botnet_send_cmd(botnetnick, tbot, dcc[idx].nick, idx, buf);
}

void cmd_netlast(struct userrec * u, int idx, char * par) {
  char buf[1024];
  log(LCAT_COMMAND, STR("#%s# netlast %s"), dcc[idx].nick, par);
  if (strchr(par, '|') || strchr(par, '<') || strchr(par, ';') || strchr(par, '>')) {
    log(LCAT_WARNING, STR("%s attempted .netlast with pipe/semicolon in parameters: %s"), dcc[idx].nick, par);
    dprintf(idx, STR("No."));
    return;
  }
  sprintf(buf, STR("exec last %s"), par);
  botnet_send_cmd_broad(-1, botnetnick, dcc[idx].nick, idx, buf);
}

void cmd_botcrontab(struct userrec * u, int idx, char * par) {
  char * tbot, buf[1024], *cmd;
  log(LCAT_COMMAND, STR("#%s# botcrontab %s"), dcc[idx].nick, par);
  tbot=newsplit(&par);
  cmd=newsplit(&par);
  if (!tbot[0] || (strcmp(cmd, STR("status")) && strcmp(cmd, STR("show")) && strcmp(cmd, STR("delete")) && strcmp(cmd, STR("new")))) {
    dprintf(idx, STR("Usage: botcrontab <botname> status|delete|show|new [interval]\n"));
    return;
  }
  if (nextbot(tbot)<0) {
    dprintf(idx, STR("No such linked bot\n"));
    return;
  }
  sprintf(buf, STR("exec crontab %s %s"), cmd, par);
  botnet_send_cmd(botnetnick, tbot, dcc[idx].nick, idx, buf);
}

void cmd_netcrontab(struct userrec * u, int idx, char * par) {
  char buf[1024], *cmd;
  log(LCAT_COMMAND, STR("#%s# netcrontab %s"), dcc[idx].nick, par);
  cmd=newsplit(&par);
  if ((strcmp(cmd, STR("status")) && strcmp(cmd, STR("show")) && strcmp(cmd, STR("delete")) && strcmp(cmd, STR("new")))) {
    dprintf(idx, STR("Usage: netcrontab status|delete|show|new [interval]\n"));
    return;
  }
  sprintf(buf, STR("exec crontab %s %s"), cmd, par);
  botnet_send_cmd_broad(-1, botnetnick, dcc[idx].nick, idx, buf);
}

void rcmd_exec(char * frombot, char * fromhand, char * fromidx, char * par) {
  char * cmd, scmd[512], *out, *err;
  cmd=newsplit(&par);
  scmd[0]=0;
  if (!strcmp(cmd, "w")) {
    strcpy(scmd, "w");
  } else if (!strcmp(cmd, STR("last"))) {
    char user[20];
    if (par[0]) {
      strncpy0(user, par, sizeof(user));
    } else {
      strncpy0(user, cuserid(NULL), sizeof(user));
    }
    if (!user[0]) {
      botnet_send_cmdreply(botnetnick, frombot, fromhand, fromidx, STR("Can't determine user id for process"));
      return;
    }
    sprintf(scmd, STR("last %s"), user);
  } else if (!strcmp(cmd, STR("ps"))) {
    sprintf(scmd, STR("ps %s"), par);
  } else if (!strcmp(cmd, STR("crontab"))) {
    char * code=newsplit(&par);
    scmd[0]=0;
    if (!strcmp(code, STR("show"))) {
      strcpy(scmd, STR("crontab -l | grep -v \"^#\""));
    } else if (!strcmp(code, STR("delete"))) {
      crontab_del();
    } else if (!strcmp(code, STR("new"))) {
      int i=atoi(par);
      if ((i<=0) || (i>60))
	i=10;
      crontab_create(i);
    }
    if (!scmd[0]) {
      char s[200];
      int i;
      i=crontab_exists();
      if (!i) 
	sprintf(s, STR("No crontab"));
      else if (i==1) 
	sprintf(s, STR("Crontabbed"));
      else
	sprintf(s, STR("Error checking crontab status"));
      botnet_send_cmdreply(botnetnick, frombot, fromhand, fromidx, s);      
    }
  }
  if (!scmd[0]) 
    return;
  if (shell_exec(scmd, NULL, &out, &err)) {
    if (out) {
      char * p, * np;
      botnet_send_cmdreply(botnetnick, frombot, fromhand, fromidx, STR("Result:"));
      p=out;
      while (p && p[0]) {
	np=strchr(p, '\n');
	if (np) 
	  *np++=0;
	botnet_send_cmdreply(botnetnick, frombot, fromhand, fromidx, p);
	p=np;
      }
      nfree(out);
    }
    if (err) {
      char * p, * np;
      botnet_send_cmdreply(botnetnick, frombot, fromhand, fromidx, STR("Errors:"));
      p=err;
      while (p && p[0]) {
	np=strchr(p, '\n');
	if (np) 
	  *np++=0;
	botnet_send_cmdreply(botnetnick, frombot, fromhand, fromidx, p);
	p=np;
      }
      nfree(err);
    }
  } else {
    botnet_send_cmdreply(botnetnick, frombot, fromhand, fromidx, STR("exec failed"));
  }

}

void cmd_botjump(struct userrec * u, int idx, char * par) {
  char *tbot, buf[1024];
  log(LCAT_COMMAND, STR("#%s# botjump %s"), dcc[idx].nick, par);
  tbot=newsplit(&par);
  if (!tbot[0]) {
    dprintf(idx, STR("Usage: botjump <botname> [server [port [pass]]]\n"));
    return;
  }
  if (nextbot(tbot)<0) {
    dprintf(idx, STR("No such linked bot\n"));
    return;
  }
  sprintf(buf, STR("jump %s"), par);
  botnet_send_cmd(botnetnick, tbot, dcc[idx].nick, idx, buf);
}

void rcmd_jump(char * frombot, char * fromhand, char * fromidx, char * par) {
#ifdef LEAF
  char * other;
  int port;
  if (par[0]) {
    other = newsplit(&par);
    port = atoi(newsplit(&par));
    if (!port)
      port = default_port;
    strncpy0(newserver, other, 120);
    newserverport = port;
    strncpy0(newserverpass, par, 120);
  }
  botnet_send_cmdreply(botnetnick, frombot, fromhand, fromidx, STR("Jumping..."));
  cycle_time = 0;
  nuke_server(STR("jumping..."));
#endif
}

/* "Remotable" commands */
void gotremotecmd (char * forbot, char * frombot, char * fromhand, char * fromidx, char * cmd) {
  char * par = cmd;
  cmd=newsplit(&par);
  if (!strcmp(cmd, STR("exec"))) {
    rcmd_exec(frombot, fromhand, fromidx, par);
  } else if (!strcmp(cmd, STR("curnick"))) {
    rcmd_curnick(frombot, fromhand, fromidx);
  } else if (!strcmp(cmd, STR("cursrv"))) {
    rcmd_cursrv(frombot, fromhand, fromidx);
  } else if (!strcmp(cmd, STR("jump"))) {
    rcmd_jump(frombot, fromhand, fromidx, par);
  } else if (!strcmp(cmd, STR("msg"))) {
    rcmd_msg(forbot, frombot, fromhand, fromidx, par);
  } else if (!strcmp(cmd, STR("ver"))) {
    rcmd_ver(frombot, fromhand, fromidx);
  } else if (!strcmp(cmd, STR("ping"))) {
    rcmd_ping(frombot, fromhand, fromidx, par);
  } else if (!strcmp(cmd, STR("pong"))) {
    rcmd_pong(frombot, fromhand, fromidx, par);
  } else {
    botnet_send_cmdreply(botnetnick, frombot, fromhand, fromidx, STR("Unrecognized remote command"));
  }
}

void gotremotereply (char * frombot, char * tohand, char * toidx, char * ln) {
  int idx=atoi(toidx);
  if ((idx>=0) && (idx<dcc_total) && (dcc[idx].type == &DCC_CHAT) && (!strcmp(dcc[idx].nick, tohand))) {
    dprintf(idx, STR("(%s) %s\n"), frombot, ln);
  }
}


/* DCC CHAT COMMANDS */

/* function call should be:
 * int cmd_whatever(idx,"parameters");
 * as with msg commands, function is responsible for any logging
 */
cmd_t C_dcc[] = {
  {"+host", "m", (Function) cmd_pls_host, NULL},
  {"+ignore", "p", (Function) cmd_pls_ignore, NULL},
  {"+user", "m", (Function) cmd_pls_user, NULL},
  {"-bot", "n", (Function) cmd_mns_user, NULL},
  {"-host", "", (Function) cmd_mns_host, NULL},
  {"-ignore", "m", (Function) cmd_mns_ignore, NULL},
  {"-user", "m", (Function) cmd_mns_user, NULL},
  {"away", "", (Function) cmd_away, NULL},
  {"back", "", (Function) cmd_back, NULL},
  {"banner", "m", (Function) cmd_banner, NULL},
  {"binds", "m", (Function) cmd_binds, NULL},
  {"boot", "m", (Function) cmd_boot, NULL},
  {"botinfo", "m", (Function) cmd_botinfo, NULL},
  {"bots", "", (Function) cmd_bots, NULL},
  {"bottree", "m", (Function) cmd_bottree, NULL},
  {"channels", "", (Function) cmd_channels, NULL},
  {"chat", "", (Function) cmd_chat, NULL},
  {"chattr", "m", (Function) cmd_chattr, NULL},
#ifdef HUB
  {"chnick", "n", (Function) cmd_chnick, NULL},
#endif
  {"comment", "m", (Function) cmd_comment, NULL},
  {"crontab", "n", (Function) cmd_crontab, NULL},
  {"dccstat", "m", (Function) cmd_dccstat, NULL},
  {"debug", "m", (Function) cmd_debug, NULL},
  {"die", "n", (Function) cmd_die, NULL},
  {"downbots", "m", (Function) cmd_downbots, NULL},
  {"echo", "", (Function) cmd_echo, NULL},
#ifdef G_EXEC
  {"exec", "n", (Function) cmd_exec, NULL},
#endif
  {"fixcodes", "", (Function) cmd_fixcodes, NULL},
  {"ignores", "m", (Function) cmd_ignores, NULL},
  {"last", "n", (Function) cmd_last, NULL},
  {"log", "p", (Function) cmd_log, NULL},
  {"match", "mo|o", (Function) cmd_match, NULL},
  {"me", "", (Function) cmd_me, NULL},
  {"motd", "", (Function) cmd_motd, NULL},
  {"newpass", "", (Function) cmd_newpass, NULL},
  {"page", "", (Function) cmd_page, NULL},
  {"ps", "n", (Function) cmd_ps, NULL},
  {"quit", "", (Function) 0, NULL},
  {"rehash", "m", (Function) cmd_rehash, NULL},
  {"relay", "j", (Function) cmd_relay, NULL},
  {"restart", "m", (Function) cmd_restart, NULL},
#ifdef G_USETCL
  {"set", "n", (Function) cmd_set, NULL},
#endif
  {"status", "p", (Function) cmd_status, NULL},
  {"strip", "", (Function) cmd_strip, NULL},
  {"su", "i", (Function) cmd_su, NULL},
#ifdef G_USETCL
  {"tcl", "n", (Function) cmd_tcl, NULL},
#endif
  {"trace", "", (Function) cmd_trace, NULL},
  {"unlink", "m", (Function) cmd_unlink, NULL},
  {"update", "n", (Function) cmd_update, NULL},
  {"uptime", "m", (Function) cmd_uptime, NULL},
  {"userlist", "m", (Function) cmd_userlist, NULL},
  {"w", "n", (Function) cmd_w, NULL},
  {"who", "", (Function) cmd_who, NULL},
  {"whois", "p", (Function) cmd_whois, NULL},
  {"whom", "", (Function) cmd_whom, NULL},


#ifdef HUB
  {"backup", "m", (Function) cmd_backup, NULL},
  {"botconfig", "n", (Function) cmd_botconfig, NULL},
  {"chpass", "m", (Function) cmd_chpass, NULL},
  {"chaddr", "m", (Function) cmd_chaddr, NULL},
#ifdef G_DCCPASS
  {"cmdpass", "n", (Function) cmd_cmdpass, NULL},
#endif
  {"config", "n", (Function) cmd_config, NULL},
  {"hublevel", "m", (Function) cmd_hublevel, NULL},
  {"lagged", "m", (Function) cmd_lagged, NULL},
  {"logconfig", "n", (Function) cmd_logconfig, NULL},
  {"newleaf", "m", (Function) cmd_newleaf, NULL},
  {"randnicks", "m", (Function) cmd_randnicks, NULL},
  {"resetnicks", "m", (Function) cmd_resetnicks, NULL},
  {"save", "m", (Function) cmd_save, NULL},
  {"uplink", "m", (Function) cmd_uplink, NULL},
#endif

  {"botjump", "n", (Function) cmd_botjump, NULL},
  {"botmsg", "o", (Function) cmd_botmsg, NULL},
  {"netmsg", "n", (Function) cmd_netmsg, NULL},
  {"botnick", "m", (Function) cmd_botnick, NULL},
  {"netnick", "m", (Function) cmd_netnick, NULL},
  {"botw", "n", (Function) cmd_botw, NULL},
  {"netw", "n", (Function) cmd_netw, NULL}, 
  {"botps", "n", (Function) cmd_botps, NULL},
  {"netps", "n", (Function) cmd_netps, NULL}, 
  {"botcrontab", "n", (Function) cmd_botcrontab, NULL},
  {"netcrontab", "n", (Function) cmd_netcrontab, NULL},
  {"botlast", "n", (Function) cmd_botlast, NULL},
  {"netlast", "n", (Function) cmd_netlast, NULL},
  {"netlag", "m", (Function) cmd_netlag, NULL},
  {"botserver", "m", (Function) cmd_botserver, NULL},
  {"netserver", "m", (Function) cmd_netserver, NULL},
  {"botversion", "o", (Function) cmd_botversion, NULL},
  {"netversion", "o", (Function) cmd_netversion, NULL},
  {0, 0, 0, 0}
};
























