
/* 
 * channels.c -- part of channels.mod
 *   support for channels within the bot
 * 
 * $Id: channels.c,v 1.34 2000/01/08 21:23:15 per Exp $
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

#define MODULE_NAME "channels"
#define MAKING_CHANNELS
#include "main.h"
#include "tandem.h"
#include "hook.h"
#include "irc.h"
#include "server.h"
#include <sys/stat.h>

int use_info = 1;
int ban_time = 60;
int exempt_time = 0;		/* if exempt_time = 0, never remove them */
int invite_time = 0;		/* if invite_time = 0, never remove them */
int no_chansync=0;

#ifdef HUB
char chanfile[121] = ".u";
extern char localkey[],
  botuser[],
  origbotname[],
  owner[],
  hostname[],
  ver[];
#ifdef G_DCCPASS
extern struct cmd_pass *cmdpass;
#endif
extern struct logcategory *logcat;
extern char botuserhost[];
extern int dcc_total;
#ifdef G_AUTOLOCK
int killed_bots=0;
#endif
extern int cfg_count;
extern struct cfg_entry ** cfg;
extern struct cfg_entry CFG_KILLTHRESHOLD;
#endif
int chan_hack = 0;

/* global channel settings (drummer/dw) */
char glob_chanset[512] = "";

/* default chanmode (drummer,990731) */
char glob_chanmode[64] = "snt";

/* global flood settings */
int gfld_chan_thr;
int gfld_chan_time;
int gfld_deop_thr;
int gfld_deop_time;
int gfld_kick_thr;
int gfld_kick_time;
int gfld_join_thr;
int gfld_join_time;
int gfld_ctcp_thr;
int gfld_ctcp_time;

void remove_channel(struct chanset_t *);

extern struct chanset_t *chanset;
extern struct dcc_t *dcc;
extern time_t now;
extern int use_exempts,
  use_invites,
  protect_readonly,
  noshare;
extern int gban_total,
  gexempt_total,
  ginvite_total,
  role,
  share_greet;
extern struct userrec *userlist;
extern char botnetnick[];

#include "channels.h"

/* 
 * cmdschan.c -- part of channels.mod
 *   commands from a user via dcc that cause server interaction
 * 
 * $Id: cmdschan.c,v 1.30 2000/01/08 21:23:15 per Exp $
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

#include <ctype.h>

struct flag_record user = {
  FR_GLOBAL | FR_CHAN, 0, 0, 0, 0
};

struct flag_record victim = {
  FR_GLOBAL | FR_CHAN, 0, 0, 0, 0
};

void cmd_pls_ban(struct userrec *u, int idx, char *par)
{
  char *chname,
   *who,
    s[UHOSTLEN],
    s1[UHOSTLEN],
   *p;
  struct chanset_t *chan = 0;
  int bogus = 0;

  /* The two lines below added for bantime */
  unsigned long int expire_time = 0,
    expire_foo;
  char *p_expire;

  log(LCAT_COMMAND, STR("#%s# +ban %s"), dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, STR("Usage: +ban <hostmask> [channel] [%%bantime<XdXhXm>] [reason]\n"));
  } else {
    who = newsplit(&par);
    for (p = who; *p; p++)
      if (((*p < 32) || (*p == 127)) && (*p != 2) && (*p != 22) && (*p != 31))
	bogus = 1;
    if (bogus) {
      dprintf(idx, STR("That is a bogus ban!\n"));
      return;
    }
    remove_gunk(who);
    if (par[0] && strchr(CHANMETA, par[0]))
      chname = newsplit(&par);
    else
      chname = 0;
    if (chname || !(u->flags & USER_MASTER)) {
      if (!chname) {
	dprintf(idx, STR("You can't set global bans - specify a channel\n"));
	return;
      }
      get_user_flagrec(u, &user, chname);
      chan = findchan(chname);
      /* *shrug* ??? (guppy:10Feb1999) */
      if (!chan) {
	dprintf(idx, STR("That channel doesnt exist!\n"));
	return;
      } else if (!((glob_op(user) && !chan_deop(user)) || chan_op(user))) {
	dprintf(idx, STR("You dont have access to set bans on %s.\n"), chname);
	return;
      }
    } else
      chan = 0;
    /* Added by Q and Solal -- Requested by Arty2, special thanx :) */
    if (par[0] == '%') {
      p = newsplit(&par);
      p_expire = p + 1;
      while (*(++p) != 0) {
	switch (tolower(*p)) {
	case 'd':
	  *p = 0;
	  expire_foo = strtol(p_expire, NULL, 10);
	  if (expire_foo > 365)
	    expire_foo = 365;
	  expire_time += 86400 * expire_foo;
	  p_expire = p + 1;
	  break;
	case 'h':
	  *p = 0;
	  expire_foo = strtol(p_expire, NULL, 10);
	  if (expire_foo > 8760)
	    expire_foo = 8760;
	  expire_time += 3600 * expire_foo;
	  p_expire = p + 1;
	  break;
	case 'm':
	  *p = 0;
	  expire_foo = strtol(p_expire, NULL, 10);
	  if (expire_foo > 525600)
	    expire_foo = 525600;
	  expire_time += 60 * expire_foo;
	  p_expire = p + 1;
	}
      }
    }
    if (!par[0])
      par = STR("requested");
    else if (strlen(par) > 65)
      par[65] = 0;
    if (strlen(who) > UHOSTMAX - 4)
      who[UHOSTMAX - 4] = 0;
    /* fix missing ! or @ BEFORE checking against myself */
    if (!strchr(who, '!')) {
      if (!strchr(who, '@'))
	simple_sprintf(s, STR("%s!*@*"), who);	/* lame nick ban */
      else
	simple_sprintf(s, STR("*!%s"), who);
    } else if (!strchr(who, '@'))
      simple_sprintf(s, STR("%s@*"), who);	/* brain-dead? */
    else
      strcpy(s, who);
    simple_sprintf(s1, STR("%s!%s"), botname, botuserhost);
    if (wild_match(s, s1)) {
      dprintf(idx, STR("Duh...  I think I'll ban myself today, Marge!\n"));
      log(LCAT_WARNING, STR("#%s# attempted +ban %s"), dcc[idx].nick, s);
    } else {
      if (strlen(s) > 70) {
	s[69] = '*';
	s[70] = 0;
      }
      /* irc can't understand bans longer than that */
      if (chan) {
	u_addban(chan, s, dcc[idx].nick, par, expire_time ? now + expire_time : 0, 0);
	dprintf(idx, STR("New %s ban: %s (%s)\n"), chan->name, s, par);
#ifdef LEAF
	add_mode(chan, '+', 'b', s);
#endif
      } else {
	u_addban(NULL, s, dcc[idx].nick, par, expire_time ? now + expire_time : 0, 0);
	dprintf(idx, STR("New global ban: %s (%s)\n"), s, par);
	chan = chanset;
	while (chan != NULL) {
#ifdef LEAF
	  add_mode(chan, '+', 'b', s);
#endif
	  chan = chan->next;
	}
      }
    }
  }
}

void cmd_pls_exempt(struct userrec *u, int idx, char *par)
{
  char *chname,
   *who,
    s[UHOSTLEN],
    s1[UHOSTLEN],
   *p;
  struct chanset_t *chan = 0;
  int bogus = 0;

  /* The two lines below added for bantime */
  unsigned long int expire_time = 0,
    expire_foo;
  char *p_expire;

  log(LCAT_COMMAND, STR("#%s# +exempt %s"), dcc[idx].nick, par);
  if (!use_exempts) {
    dprintf(idx, STR("This command can only be used on IRCnet.\n"));
    return;
  }
  if (!par[0]) {
    dprintf(idx, STR("Usage: +exempt <hostmask> [channel] [%%exempttime<XdXhXm>] [reason]\n"));
  } else {
    who = newsplit(&par);
    for (p = who; *p; p++)
      if (((*p < 32) || (*p == 127)) && (*p != 2) && (*p != 22) && (*p != 31))
	bogus = 1;
    if (bogus) {
      dprintf(idx, STR("That is a bogus exempt!\n"));
      return;
    }
    remove_gunk(who);
    if ((par[0] == '#') || (par[0] == '&') || (par[0] == '+'))
      chname = newsplit(&par);
    else
      chname = 0;
    if (chname || !(u->flags & USER_MASTER)) {
      if (!chname) {
	dprintf(idx, STR("You can't set global exempts - specify a channel\n"));
	return;
      }
      get_user_flagrec(u, &user, chname);
      chan = findchan(chname);
      /* *shrug* ??? (guppy:10Feb99) */
      if (!chan) {
	dprintf(idx, STR("That channel doesnt exist!\n"));
	return;
      } else if (!((glob_op(user) && !chan_deop(user)) || chan_op(user))) {
	dprintf(idx, STR("You dont have access to set exempts on %s.\n"), chname);
	return;
      }
    } else
      chan = 0;
    /* Added by Q and Solal  - Requested by Arty2, special thanx :) */
    if (par[0] == '%') {
      p = newsplit(&par);
      p_expire = p + 1;
      while (*(++p) != 0) {
	switch (tolower(*p)) {
	case 'd':
	  *p = 0;
	  expire_foo = strtol(p_expire, NULL, 10);
	  if (expire_foo > 365)
	    expire_foo = 365;
	  expire_time += 86400 * expire_foo;
	  p_expire = p + 1;
	  break;
	case 'h':
	  *p = 0;
	  expire_foo = strtol(p_expire, NULL, 10);
	  if (expire_foo > 8760)
	    expire_foo = 8760;
	  expire_time += 3600 * expire_foo;
	  p_expire = p + 1;
	  break;
	case 'm':
	  *p = 0;
	  expire_foo = strtol(p_expire, NULL, 10);
	  if (expire_foo > 525600)
	    expire_foo = 525600;
	  expire_time += 60 * expire_foo;
	  p_expire = p + 1;
	}
      }
    }
    if (!par[0])
      par = STR("requested");
    else if (strlen(par) > 65)
      par[65] = 0;
    if (strlen(who) > UHOSTMAX - 4)
      who[UHOSTMAX - 4] = 0;
    /* fix missing ! or @ BEFORE checking against myself */
    if (!strchr(who, '!')) {
      if (!strchr(who, '@'))
	simple_sprintf(s, STR("%s!*@*"), who);	/* lame nick exempt */
      else
	simple_sprintf(s, STR("*!%s"), who);
    } else if (!strchr(who, '@'))
      simple_sprintf(s, STR("%s@*"), who);	/* brain-dead? */
    else
      strcpy(s, who);
    simple_sprintf(s1, STR("%s!%s"), botname, botuserhost);
    if (strlen(s) > 70) {
      s[69] = '*';
      s[70] = 0;
    }
    /* irc can't understand exempts longer than that */
    if (chan) {
      u_addexempt(chan, s, dcc[idx].nick, par, expire_time ? now + expire_time : 0, 0);
      dprintf(idx, STR("New %s sticky exempt: %s (%s)\n"), chan->name, s, par);
#ifdef LEAF
      add_mode(chan, '+', 'e', s);
#endif
    } else {
      u_addexempt(NULL, s, dcc[idx].nick, par, expire_time ? now + expire_time : 0, 0);
      dprintf(idx, STR("New sticky exempt: %s (%s)\n"), s, par);
      chan = chanset;
      while (chan != NULL) {
#ifdef LEAF
	add_mode(chan, '+', 'e', s);
#endif
	chan = chan->next;
      }
    }
  }
}

void cmd_pls_invite(struct userrec *u, int idx, char *par)
{
  char *chname,
   *who,
    s[UHOSTLEN],
    s1[UHOSTLEN],
   *p;
  struct chanset_t *chan = 0;
  int bogus = 0;

  /* The two lines below added for bantime */
  unsigned long int expire_time = 0,
    expire_foo;
  char *p_expire;

  log(LCAT_COMMAND, STR("#%s# +invite %s"), dcc[idx].nick, par);

  if (!use_invites) {
    dprintf(idx, STR("This command can only be used on IRCnet. \n"));
    return;
  }

  if (!par[0]) {
    dprintf(idx, STR("Usage: +invite <hostmask> [channel] [%%invitetime<XdXhXm>] [reason]\n"));
  } else {
    who = newsplit(&par);
    for (p = who; *p; p++)
      if (((*p < 32) || (*p == 127)) && (*p != 2) && (*p != 22) && (*p != 31))
	bogus = 1;
    if (bogus) {
      dprintf(idx, STR("That is a bogus invite!\n"));
      return;
    }
    remove_gunk(who);
    if ((par[0] == '#') || (par[0] == '&') || (par[0] == '+'))
      chname = newsplit(&par);
    else
      chname = 0;
    if (chname || !(u->flags & USER_MASTER)) {
      if (!chname) {
	dprintf(idx, STR("You can't set global invites - specify a channel\n"));
	return;
      }
      get_user_flagrec(u, &user, chname);
      chan = findchan(chname);
      /* *shrug* ??? (guppy:10Feb99) */
      if (!chan) {
	dprintf(idx, STR("That channel doesnt exist!\n"));
	return;
      } else if (!((glob_op(user) && !chan_deop(user)) || chan_op(user))) {
	dprintf(idx, STR("You dont have access to set invites on %s.\n"), chname);
	return;
      }
    } else
      chan = 0;
    /* Added by Q and Solal  - Requested by Arty2, special thanx :) */
    if (par[0] == '%') {
      p = newsplit(&par);
      p_expire = p + 1;
      while (*(++p) != 0) {
	switch (tolower(*p)) {
	case 'd':
	  *p = 0;
	  expire_foo = strtol(p_expire, NULL, 10);
	  if (expire_foo > 365)
	    expire_foo = 365;
	  expire_time += 86400 * expire_foo;
	  p_expire = p + 1;
	  break;
	case 'h':
	  *p = 0;
	  expire_foo = strtol(p_expire, NULL, 10);
	  if (expire_foo > 8760)
	    expire_foo = 8760;
	  expire_time += 3600 * expire_foo;
	  p_expire = p + 1;
	  break;
	case 'm':
	  *p = 0;
	  expire_foo = strtol(p_expire, NULL, 10);
	  if (expire_foo > 525600)
	    expire_foo = 525600;
	  expire_time += 60 * expire_foo;
	  p_expire = p + 1;
	}
      }
    }
    if (!par[0])
      par = STR("requested");
    else if (strlen(par) > 65)
      par[65] = 0;
    if (strlen(who) > UHOSTMAX - 4)
      who[UHOSTMAX - 4] = 0;
    /* fix missing ! or @ BEFORE checking against myself */
    if (!strchr(who, '!')) {
      if (!strchr(who, '@'))
	simple_sprintf(s, STR("%s!*@*"), who);	/* lame nick invite */
      else
	simple_sprintf(s, STR("*!%s"), who);
    } else if (!strchr(who, '@'))
      simple_sprintf(s, STR("%s@*"), who);	/* brain-dead? */
    else
      strcpy(s, who);
    simple_sprintf(s1, STR("%s!%s"), botname, botuserhost);

    if (strlen(s) > 70) {
      s[69] = '*';
      s[70] = 0;
    }
    /* irc can't understand invites longer than that */
    if (chan) {
      u_addinvite(chan, s, dcc[idx].nick, par, expire_time ? now + expire_time : 0, 0);
      dprintf(idx, STR("New %s invite: %s (%s)\n"), chan->name, s, par);
#ifdef LEAF
      add_mode(chan, '+', 'I', s);
#endif
    } else {
      u_addinvite(NULL, s, dcc[idx].nick, par, expire_time ? now + expire_time : 0, 0);
      dprintf(idx, STR("New invite: %s (%s)\n"), s, par);
      chan = chanset;
      while (chan != NULL) {
#ifdef LEAF
	add_mode(chan, '+', 'I', s);
#endif
	chan = chan->next;
      }
    }
  }
}

void cmd_mns_ban(struct userrec *u, int idx, char *par)
{
  int i = 0,
    j;
  struct chanset_t *chan = 0;
  char s[UHOSTLEN],
   *ban,
   *chname;
  struct maskstruct *b;

  log(LCAT_COMMAND, STR("#%s# -ban %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: -ban <hostmask|ban #> [channel]\n"));
    return;
  }
  ban = newsplit(&par);
  if (par[0] && strchr(CHANMETA, par[0]))
    chname = newsplit(&par);
  else
    chname = NULL;
  if (chname || !(u->flags & USER_MASTER)) {
    if (!chname) {
      dprintf(idx, STR("You can't remove global bans - specify a channel\n"));
      return;
    }
    get_user_flagrec(u, &user, chname);
    if (!((glob_op(user) && !chan_deop(user)) || chan_op(user)))
      return;
  }
  strncpy0(s, ban, UHOSTMAX);
  i = u_delban(NULL, s, (u->flags & USER_MASTER));
  if (i > 0) {
    dprintf(idx, STR("Removed ban: %s\n"), s);
    chan = chanset;
    while (chan) {
#ifdef LEAF
      add_mode(chan, '-', 'b', s);
#endif
      chan = chan->next;
    }
    return;
  }
  /* channel-specific ban? */
  if (chname)
    chan = findchan(chname);
  if (chan) {
    if (atoi(ban) > 0) {
      simple_sprintf(s, "%d", -i);
      j = u_delban(chan, s, 1);
      if (j > 0) {
	dprintf(idx, STR("Removed %s channel ban: %s\n"), chan->name, s);
#ifdef LEAF
	add_mode(chan, '-', 'b', s);
#endif
	return;
      }
      i = 0;
      for (b = chan->channel.ban; b->mask[0]; b = b->next) {
	if ((!u_equals_mask(global_bans, b->mask)) && (!u_equals_mask(chan->bans, b->mask))) {
	  i++;
	  if (i == -j) {
#ifdef LEAF
	    add_mode(chan, '-', 'b', b->mask);
#endif
	    dprintf(idx, STR("Removed ban '%s' on %s.\n"), b->mask, chan->name);
	    return;
	  }
	}
      }
    } else {
      j = u_delban(chan, ban, 1);
      if (j > 0) {
	dprintf(idx, STR("Removed %s channel ban: %s\n"), chname, ban);
#ifdef LEAF
	add_mode(chan, '-', 'b', ban);
#endif
	return;
      }
      for (b = chan->channel.ban; b->mask[0]; b = b->next) {
	if (!rfc_casecmp(b->mask, ban)) {
#ifdef LEAF
	  add_mode(chan, '-', 'b', b->mask);
#endif
	  dprintf(idx, STR("Removed ban '%s' on %s.\n"), b->mask, chan->name);
	  return;
	}
      }
    }
  }
  dprintf(idx, STR("No such ban.\n"));
}

void cmd_mns_exempt(struct userrec *u, int idx, char *par)
{
  int i = 0,
    j;
  struct chanset_t *chan = 0;
  char s[UHOSTLEN],
   *exempt,
   *chname;
  struct maskstruct *e;

  log(LCAT_COMMAND, STR("#%s# -exempt %s"), dcc[idx].nick, par);
  if (!use_exempts) {
    dprintf(idx, STR("This command can only be used on IRCnet.\n"));
    return;
  }
  if (!par[0]) {
    dprintf(idx, STR("Usage: -exempt <hostmask|exempt #> [channel]\n"));
    return;
  }
  exempt = newsplit(&par);
  if ((par[0] == '#') || (par[0] == '&') || (par[0] == '+'))
    chname = newsplit(&par);
  else
    chname = NULL;
  if (chname || !(u->flags & USER_MASTER)) {
    if (!chname) {
      dprintf(idx, STR("You can't remove global exempts - specify a channel\n"));
      return;
    }
    get_user_flagrec(u, &user, chname);
    if (!((glob_op(user) && !chan_deop(user)) || chan_op(user)))
      return;
  }
  strncpy0(s, exempt, UHOSTMAX);
  i = u_delexempt(NULL, s, (u->flags & USER_MASTER));
  if (i > 0) {
    dprintf(idx, STR("Removed exempt: %s\n"), s);
    chan = chanset;
    while (chan) {
#ifdef LEAF
      add_mode(chan, '-', 'e', s);
#endif
      chan = chan->next;
    }
    return;
  }
  /* channel-specific exempt? */
  if (chname)
    chan = findchan(chname);
  if (chan) {
    if (atoi(exempt) > 0) {
      simple_sprintf(s, "%d", -i);
      j = u_delexempt(chan, s, 1);
      if (j > 0) {
	dprintf(idx, STR("Removed %s channel exempt: %s\n"), chan->name, s);
#ifdef LEAF
	add_mode(chan, '-', 'e', s);
#endif
	return;
      }
      i = 0;
      for (e = chan->channel.exempt; e->mask[0]; e = e->next) {
	if (!u_equals_mask(global_exempts, e->mask) && !u_equals_mask(chan->exempts, e->mask)) {
	  i++;
	  if (i == -j) {
#ifdef LEAF
	    add_mode(chan, '-', 'e', e->mask);
#endif
	    dprintf(idx, STR("Removed exempt '%s' on %s.\n"), e->mask, chan->name);
	    return;
	  }
	}
      }
    } else {
      j = u_delexempt(chan, exempt, 1);
      if (j > 0) {
	dprintf(idx, STR("Removed %s channel exempt: %s\n"), chname, exempt);
#ifdef LEAF
	add_mode(chan, '-', 'e', exempt);
#endif
	return;
      }
      for (e = chan->channel.exempt; e->mask[0]; e = e->next) {
	if (!rfc_casecmp(e->mask, exempt)) {
#ifdef LEAF
	  add_mode(chan, '-', 'e', e->mask);
#endif
	  dprintf(idx, STR("Removed exempt '%s' on %s.\n"), e->mask, chan->name);
	  return;
	}
      }
    }
  }
  dprintf(idx, STR("No such exemption.\n"));
}

void cmd_mns_invite(struct userrec *u, int idx, char *par)
{
  int i = 0,
    j;
  struct chanset_t *chan = 0;
  char s[UHOSTLEN],
   *invite,
   *chname;
  struct maskstruct *inv;

  log(LCAT_COMMAND, STR("#%s# -invite %s"), dcc[idx].nick, par);
  if (!use_invites) {
    dprintf(idx, STR("This command can only be used on IRCnet.\n"));
    return;
  }
  if (!par[0]) {
    dprintf(idx, STR("Usage: -invite <hostmask|invite #> [channel]\n"));
    return;
  }
  invite = newsplit(&par);
  if ((par[0] == '#') || (par[0] == '&') || (par[0] == '+'))
    chname = newsplit(&par);
  else
    chname = NULL;
  if (chname || !(u->flags & USER_MASTER)) {
    if (!chname) {
      dprintf(idx, STR("You can't remove global invites - specify a channel\n"));
      return;
    }
    get_user_flagrec(u, &user, chname);
    if (!((glob_op(user) && !chan_deop(user)) || chan_op(user)))
      return;
  }
  strncpy0(s, invite, UHOSTMAX);
  i = u_delinvite(NULL, s, (u->flags & USER_MASTER));
  if (i > 0) {
    dprintf(idx, STR("Removed invite: %s\n"), s);
#ifdef LEAF
    chan = chanset;
    while (chan) {

      add_mode(chan, '-', 'I', s);
      chan = chan->next;
    }
#endif
    return;
  }
  /* channel-specific invite? */
  if (chname)
    chan = findchan(chname);
  if (chan) {
    if (atoi(invite) > 0) {
      simple_sprintf(s, "%d", -i);
      j = u_delinvite(chan, s, 1);
      if (j > 0) {
	dprintf(idx, STR("Removed %s channel invite: %s\n"), chan->name, s);
#ifdef LEAF
	add_mode(chan, '-', 'I', s);
#endif
	return;
      }
      i = 0;
      for (inv = chan->channel.invite; inv->mask[0]; inv = inv->next) {
	if (!u_equals_mask(global_invites, inv->mask) && !u_equals_mask(chan->invites, inv->mask)) {
	  i++;
	  if (i == -j) {
#ifdef LEAF
	    add_mode(chan, '-', 'I', inv->mask);
#endif
	    dprintf(idx, STR("Removed invite '%s' on %s.\n"), inv->mask, chan->name);
	    return;
	  }
	}
      }
    } else {
      j = u_delinvite(chan, invite, 1);
      if (j > 0) {
	dprintf(idx, STR("Removed %s channel invite: %s\n"), chname, invite);
#ifdef LEAF
	add_mode(chan, '-', 'I', invite);
#endif
	return;
      }
      for (inv = chan->channel.invite; inv->mask[0]; inv = inv->next) {
	if (!rfc_casecmp(inv->mask, invite)) {
#ifdef LEAF
	  add_mode(chan, '-', 'I', inv->mask);
#endif
	  dprintf(idx, STR("Removed invite '%s' on %s.\n"), inv->mask, chan->name);
	  return;
	}
      }
    }
  }
  dprintf(idx, STR("No such invite.\n"));
}

void cmd_bans(struct userrec *u, int idx, char *par)
{
  log(LCAT_COMMAND, STR("#%s# bans %s"), dcc[idx].nick, par);
  if (!strcasecmp(par, STR("all"))) {
    tell_bans(idx, 1, "");
  } else {
    tell_bans(idx, 0, par);
  }
}

void cmd_exempts(struct userrec *u, int idx, char *par)
{
  log(LCAT_COMMAND, STR("#%s# exempts %s"), dcc[idx].nick, par);
  if (!use_exempts) {
    dprintf(idx, STR("This command can only be used on IRCnet.\n"));
    return;
  }
  if (!strcasecmp(par, STR("all"))) {
    tell_exempts(idx, 1, "");
  } else {
    tell_exempts(idx, 0, par);
  }
}

void cmd_invites(struct userrec *u, int idx, char *par)
{
  log(LCAT_COMMAND, STR("#%s# invites %s"), dcc[idx].nick, par);
  if (!use_invites) {
    dprintf(idx, STR("This command can only be used on IRCnet.\n"));
    return;
  }
  if (!strcasecmp(par, STR("all"))) {
    tell_invites(idx, 1, "");
  } else {
    tell_invites(idx, 0, par);
  }
}

void cmd_info(struct userrec *u, int idx, char *par)
{
  char s[512],
   *chname,
   *s1;
  int locked = 0;

  log(LCAT_COMMAND, STR("#%s# info %s"), dcc[idx].nick, par);
  if (!use_info) {
    dprintf(idx, STR("Info storage is turned off.\n"));
    return;
  }
  s1 = get_user(&USERENTRY_INFO, u);
  if (s1 && s1[0] == '@')
    locked = 1;
  if (par[0] && strchr(CHANMETA, par[0])) {
    chname = newsplit(&par);
    if (!findchan(chname)) {
      dprintf(idx, STR("No such channel.\n"));
      return;
    }
    get_handle_chaninfo(dcc[idx].nick, chname, s);
    if (s[0] == '@')
      locked = 1;
    s1 = s;
  } else
    chname = 0;
  if (!par[0]) {
    if (s1 && s1[0] == '@')
      s1++;
    if (s1 && s1[0]) {
      if (chname) {
	dprintf(idx, STR("Info on %s: %s\n"), chname, s1);
	dprintf(idx, STR("Use '.info %s none' to remove it.\n"), chname);
      } else {
	dprintf(idx, STR("Default info: %s\n"), s1);
	dprintf(idx, STR("Use '.info none' to remove it.\n"));
      }
    } else
      dprintf(idx, STR("No info has been set for you.\n"));
    return;
  }
  if (locked && !(u && (u->flags & USER_MASTER))) {
    dprintf(idx, STR("Your info line is locked.  Sorry.\n"));
    return;
  }
  if (!strcasecmp(par, STR("none"))) {
    if (chname) {
      par[0] = 0;
      set_handle_chaninfo(userlist, dcc[idx].nick, chname, NULL);
      dprintf(idx, STR("Removed your info line on %s.\n"), chname);
    } else {
      set_user(&USERENTRY_INFO, u, NULL);
      dprintf(idx, STR("Removed your default info line.\n"));
    }
    return;
  }
  if (par[0] == '@')
    par++;
  if (chname) {
    set_handle_chaninfo(userlist, dcc[idx].nick, chname, par);
    dprintf(idx, STR("Your info on %s is now: %s\n"), chname, par);
  } else {
    set_user(&USERENTRY_INFO, u, par);
    dprintf(idx, STR("Your default info is now: %s\n"), par);
  }
}

void cmd_chinfo(struct userrec *u, int idx, char *par)
{
  char *handle,
   *chname;
  struct userrec *u1;

  log(LCAT_COMMAND, STR("#%s# chinfo %s "), dcc[idx].nick, par);
  if (!use_info) {
    dprintf(idx, STR("Info storage is turned off.\n"));
    return;
  }
  handle = newsplit(&par);
  if (!handle[0]) {
    dprintf(idx, STR("Usage: chinfo <handle> [channel] <new-info>\n"));
    return;
  }
  u1 = get_user_by_handle(userlist, handle);
  if (!u1) {
    dprintf(idx, STR("No such user.\n"));
    return;
  }
  if (par[0] && strchr(CHANMETA, par[0])) {
    chname = newsplit(&par);
    if (!findchan(chname)) {
      dprintf(idx, STR("No such channel.\n"));
      return;
    }
  } else
    chname = 0;
  if (u1->flags & USER_BOT) {
    dprintf(idx, STR("Useful only for users.\n"));
    return;
  }
  if ((u1->flags & USER_OWNER) && !(u->flags & USER_OWNER)) {
    dprintf(idx, STR("Can't change info for the bot owner.\n"));
    return;
  }
  if (chname) {
    get_user_flagrec(u, &user, chname);
    get_user_flagrec(u1, &victim, chname);
    if ((chan_owner(victim) || glob_owner(victim)) && !(glob_owner(user) || chan_owner(user))) {
      dprintf(idx, STR("Can't change info for the channel owner.\n"));
      return;
    }
  }
  if (!strcasecmp(par, STR("none")))
    par[0] = 0;
  if (chname) {
    set_handle_chaninfo(userlist, handle, chname, par);
    if (par[0] == '@')
      dprintf(idx, STR("New info (LOCKED) for %s on %s: %s\n"), handle, chname, &par[1]);
    else if (par[0])
      dprintf(idx, STR("New info for %s on %s: %s\n"), handle, chname, par);
    else
      dprintf(idx, STR("Wiped info for %s on %s\n"), handle, chname);
  } else {
    set_user(&USERENTRY_INFO, u1, par[0] ? par : NULL);
    if (par[0] == '@')
      dprintf(idx, STR("New default info (LOCKED) for %s: %s\n"), handle, &par[1]);
    else if (par[0])
      dprintf(idx, STR("New default info for %s: %s\n"), handle, par);
    else
      dprintf(idx, STR("Wiped default info for %s\n"), handle);
  }
}

void cmd_pls_chrec(struct userrec *u, int idx, char *par)
{
  char *nick,
   *chn;
  struct chanset_t *chan;
  struct userrec *u1;
  struct chanuserrec *chanrec;

  Context;
  log(LCAT_COMMAND, STR("#%s# +chrec %s"), dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, STR("Usage: +chrec <User> channel\n"));
    return;
  }
  nick = newsplit(&par);
  u1 = get_user_by_handle(userlist, nick);
  if (!u1) {
    dprintf(idx, STR("No such user.\n"));
    return;
  }
  if (!par[0]) {
    dprintf(idx, STR("Specify a channel\n"));
    return;
  } else {
    chn = newsplit(&par);
    chan = findchan(chn);
  }
  if (!chan) {
    dprintf(idx, STR("No such channel.\n"));
    return;
  }
  get_user_flagrec(u, &user, chan->name);
  get_user_flagrec(u1, &victim, chan->name);
  if ((!glob_master(user) && !chan_master(user)) ||	/* drummer */
      (chan_owner(victim) && !chan_owner(user) && !glob_owner(user)) || (glob_owner(victim) && !glob_owner(user))) {
    dprintf(idx, STR("You have no permission to do that.\n"));
    return;
  }
  chanrec = get_chanrec(u1, chan->name);
  if (chanrec) {
    dprintf(idx, STR("User %s already has a channel record for %s.\n"), nick, chan->name);
    return;
  }
  add_chanrec(u1, chan->name);
  dprintf(idx, STR("Added %s channel record for %s.\n"), chan->name, nick);
}

void cmd_mns_chrec(struct userrec *u, int idx, char *par)
{
  char *nick;
  char *chn = NULL;
  struct userrec *u1;
  struct chanuserrec *chanrec;

  Context;
  log(LCAT_COMMAND, STR("#%s# -chrec %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: -chrec <User> channel\n"));
    return;
  }
  nick = newsplit(&par);
  u1 = get_user_by_handle(userlist, nick);
  if (!u1) {
    dprintf(idx, STR("No such user.\n"));
    return;
  }
  if (!par[0]) {
    dprintf(idx, STR("Specify a channel\n"));
    return;
  } else
    chn = newsplit(&par);
  get_user_flagrec(u, &user, chn);
  get_user_flagrec(u1, &victim, chn);
  if ((!glob_master(user) && !chan_master(user)) ||	/* drummer */
      (chan_owner(victim) && !chan_owner(user) && !glob_owner(user)) || (glob_owner(victim) && !glob_owner(user))) {
    dprintf(idx, STR("You have no permission to do that.\n"));
    return;
  }
  chanrec = get_chanrec(u1, chn);
  if (!chanrec) {
    dprintf(idx, STR("User %s doesn't have a channel record for %s.\n"), nick, chn);
    return;
  }
  del_chanrec(u1, chn);
  dprintf(idx, STR("Removed %s channel record for %s.\n"), chn, nick);
}

void channels_checkslowjoin() {
  struct chanset_t * chan;
  for (chan=chanset;chan;chan=chan->next) {
    if ((chan->jointime) && (chan->jointime<now)) {
      chan->status &= ~CHAN_INACTIVE;
      chan->jointime=0;
      if (shouldjoin(chan))
	dprintf(DP_MODE, STR("JOIN %s %s\n"), chan->name, chan->key_prot);
    }
  }
}

void got_sj(int idx, char * code, char * par) {
  char * chname;
  time_t delay;
  struct chanset_t * chan;
  chname=newsplit(&par);
  delay=atoi(par) + now;
  chan=findchan(chname);
  if (chan)
    chan->jointime = delay;
}

void cmd_slowjoin(struct userrec *u, int idx, char *par)
{
  int intvl=0, delay=0, count=1;
  char * chname;
  char buf[2048];
  struct chanset_t * chan;
  struct userrec * bot;
  char *p;
  /* .slowjoin #chan 60 */
  log (LCAT_COMMAND, STR("#%s# slowjoin %s"), dcc[idx].nick, par);
  chname=newsplit(&par);
  p=newsplit(&par);
  intvl=atoi(p);
  if (!chname[0] || !p[0]) {
    dprintf(idx, STR("Usage: slowjoin channel seconds-interval [channel options]\n"));
    return;
  }
  if (intvl<10) {
    dprintf(idx, STR("Interval must be at least 10 seconds\n"));
    return;
  }
  if ((chan=findchan(chname))) {
    dprintf(idx, STR("Already on %s\n"), chan->name);
    return;
  }
  if (!strchr(CHANMETA, chname[0])) {
    dprintf(idx, STR("Invalid channel name\n"));
    return;
  }
  strcpy(buf, STR("+inactive "));
  if (par[0])
    strncat(buf, par, sizeof(buf));
  if (!do_channel_add(chname, buf)) {
    dprintf(idx, STR("Invalid channel.\n"));
    return;
  }
  chan=findchan(chname);
  if (!chan) {
    dprintf(idx, STR("Hmmm... Channel didn't get added. Weird *shrug*\n"));
    return;
  }
#ifdef HUB
  count=0;
#else
  count=1;
#endif
  for (bot=userlist;bot;bot=bot->next) {
    if ((bot->flags & USER_BOT) && ((nextbot(bot->handle)>=0))) {
      char tmp[100];
      /* Variation: 60 secs intvl should be 60 +/- 15 */
      if (bot_hublevel(bot)<999) {
	sprintf(tmp, STR("sj %s 0\n"), chan->name);
      } else {
	int v = (rand() % (intvl / 2)) - (intvl / 4);
	delay += intvl;
	sprintf(tmp, STR("sj %s %i\n"), chan->name, delay + v);
	count++;
      }
      botnet_send_zapf(nextbot(bot->handle), botnetnick, bot->handle, tmp);
    }
  }
  dprintf(idx, STR("%i bots joining %s during the next %i seconds\n"), count, chan->name, delay);
  chan->status &= ~CHAN_INACTIVE;
#ifdef LEAF
  dprintf(DP_MODE, STR("JOIN %s %s\n"), chan->name, chan->key_prot);
#endif
}

#ifdef HUB
void got_locktopic(int idx, char * code, char * par) {
  char * chname;
  struct chanset_t *chan;
  chname=newsplit(&par);
  chan=findchan(chname);
  if (chan) {
    while (par[0]==' ')
      par++;
    strncpy0(chan->topic, par, sizeof(chan->topic));
  }
}
#else
void got_locktopic(int idx, char * code, char * par) {
  char * chname;
  struct chanset_t * chan;
  chname=newsplit(&par);
  chan=findchan(chname);
  if (chan) {
    if (me_op(chan))
      if (!chan->channel.topic || strcmp(par, chan->channel.topic))
	dprintf(DP_HELP, STR("TOPIC %s :%s\n"), chan->name, par);
  }
}
#endif

void cmd_locktopic(struct userrec *u, int idx, char *par) {
  /*
    .locktopic #chan [topic]
  */
  char * chname, buf[512], hub[20];
  struct chanset_t * chan;

  log(LCAT_COMMAND, STR("#%s# locktopic %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: locktopic <channel> [topic]\n"));
    return;
  }
  chname = newsplit(&par);
  if (! (chan=findchan(chname))) {
    dprintf(idx, STR("That channel doesn't exist\n"));
    return;
  }
  while (par[0]==' ')
    par++;
  if (strlen(par)>90)
    par[90]=0;
  sprintf(buf, STR("ltp %s %s\n"), chan->name, par);
  besthub(hub);
#ifdef HUB
  strcpy(chan->topic, par);
#endif
  if (hub[0]) {
    botnet_send_zapf(nextbot(hub), botnetnick, hub, buf);
    if (par[0]) 
      dprintf(idx, STR("Now enforcing %s topic: %s\n"), chan->name, par);
    else
      dprintf(idx, STR("Now not enforcing %s topic\n"), chan->name);
  } else {
#ifdef LEAF
    dprintf(idx, STR("Can't set enforced topic - no hubs linked\n"));
#endif
  }
}

void cmd_pls_chan(struct userrec *u, int idx, char *par)
{
  char *chname;

  log(LCAT_COMMAND, STR("#%s# +chan %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: +chan [%s]<channel>\n"), CHANMETA);
    return;
  }
  chname = newsplit(&par);
  if (findchan(chname)) {
    dprintf(idx, STR("That channel already exists!\n"));
    return;
  }
  if (!do_channel_add(chname, par))
    dprintf(idx, STR("Invalid channel.\n"));
  else {
#ifdef HUB
    write_userfile(idx);
#endif

  }
}

void cmd_mns_chan(struct userrec *u, int idx, char *par)
{
  char *chname;
  struct chanset_t *chan;

  log(LCAT_COMMAND, STR("#%s# -chan %s"), dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, STR("Usage: -chan [%s]<channel>\n"), CHANMETA);
    return;
  }
  chname = newsplit(&par);
  chan = findchan(chname);
  if (!chan) {
    dprintf(idx, STR("That channel doesnt exist!\n"));
    return;
  }
  if (shouldjoin(chan))
    dprintf(DP_SERVER, STR("PART %s\n"), chname);
  remove_channel(chan);
#ifdef HUB
    write_userfile(idx);
#endif
  dprintf(idx, STR("Channel %s removed from the bot.\n"), chname);
  dprintf(idx, STR("This includes any channel specific bans, invites, exemptions and user records that you set.\n"));
  send_channel_sync("*", NULL);
}

void showchansetting(int idx, char *work, int *cnt, char *name, int state)
{
  char tmp[100];

  (*cnt)++;
  if (*cnt > 4) {
    *cnt = 1;
    work[0] = 0;
  }
  if (!work[0])
    sprintf(work, "     ");

  sprintf(tmp, STR("%c%s"), state ? '+' : '-', name);
  while (strlen(tmp) < 14)
    strcat(tmp, " ");
  strcat(work, tmp);
  if (*cnt == 4)
    dprintf(idx, STR("%s\n"), work);
}

void cmd_chaninfo(struct userrec *u, int idx, char *par)
{
  char *chname,
    work[512];
  struct chanset_t *chan;
  int cnt;

  log(LCAT_COMMAND, STR("#%s# chaninfo %s"), dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, STR("Usage: chaninfo channel\n"));
    return;
  } else {
    chname = newsplit(&par);
    get_user_flagrec(u, &user, chname);
    if (!glob_master(user) && !chan_master(user)) {
      dprintf(idx, STR("You dont have access to %s. \n"), chname);
      return;
    }
  }
  if (!(chan = findchan(chname)))
    dprintf(idx, STR("No such channel defined.\n"));
  else {
    dprintf(idx, STR("Settings for channel %s\n"), chname);
    get_mode_protect(chan, work);
    dprintf(idx, STR("Protect modes (chanmode): %s\n"), work[0] ? work : STR("None"));
    if (chan->idle_kick)
      dprintf(idx, STR("Idle Kick after (idle-kick): %d\n"), chan->idle_kick);
    else
      dprintf(idx, STR("Idle Kick after (idle-kick): DONT!\n"));
    if (chan->limitraise) 
      dprintf(idx, STR("Limit raise: %d\n"), chan->limitraise);
    else
      dprintf(idx, STR("Limit raise: disabled\n"));
    /* only bot owners can see/change these (they're TCL commands) */
    dprintf(idx, STR("Other modes:\n"));

    work[0] = 0;
    cnt = 0;
    showchansetting(idx, work, &cnt, STR("clearbans"), chan->status & CHAN_CLEARBANS);
    showchansetting(idx, work, &cnt, STR("enforcebans"), chan->status & CHAN_ENFORCEBANS);
    showchansetting(idx, work, &cnt, STR("dynamicbans"), chan->status & CHAN_DYNAMICBANS);
    showchansetting(idx, work, &cnt, STR("userbans"), !(chan->status & CHAN_NOUSERBANS));
    showchansetting(idx, work, &cnt, STR("bitch"), chan->status & CHAN_BITCH);
    showchansetting(idx, work, &cnt, STR("statuslog"), chan->status & CHAN_LOGSTATUS);
    showchansetting(idx, work, &cnt, STR("cycle"), chan->status & CHAN_CYCLE);
    showchansetting(idx, work, &cnt, STR("seen"), chan->status & CHAN_SEEN);
#ifdef G_MANUALOP
    showchansetting(idx, work, &cnt, STR("manualop"), chan->status & CHAN_MANOP);
#endif
#ifdef G_FASTOP
    showchansetting(idx, work, &cnt, STR("fastop"), chan->status & CHAN_FASTOP);
#endif
#ifdef G_TAKE
    showchansetting(idx, work, &cnt, STR("take"), chan->status & CHAN_TAKE);
#endif
#ifdef G_BACKUP
    showchansetting(idx, work, &cnt, STR("backup"), chan->status & CHAN_BACKUP);
#endif
#ifdef G_MEAN
    showchansetting(idx, work, &cnt, STR("mean"), chan->status & CHAN_MEAN);
#endif
    showchansetting(idx, work, &cnt, STR("locked"), chan->status & CHAN_LOCKED);
    showchansetting(idx, work, &cnt, STR("inactive"), chan->status & CHAN_INACTIVE);
    if (cnt != 4)
      dprintf(idx, STR("%s\n"), work);
    dprintf(idx, STR("     %cdynamicexempts           %cuserexempts\n"), (chan->ircnet_status & CHAN_DYNAMICEXEMPTS) ? '+' : '-', (chan->ircnet_status & CHAN_NOUSEREXEMPTS) ? '-' : '+');
    dprintf(idx, STR("     %cdynamicinvites           %cuserinvites\n"), (chan->ircnet_status & CHAN_DYNAMICINVITES) ? '+' : '-', (chan->ircnet_status & CHAN_NOUSERINVITES) ? '-' : '+');
    dprintf(idx, STR("flood settings: chan ctcp join kick deop\n"));
    dprintf(idx, STR("number:          %3d  %3d  %3d  %3d  %3d\n"), chan->flood_pub_thr, chan->flood_ctcp_thr, chan->flood_join_thr, chan->flood_kick_thr, chan->flood_deop_thr);
    dprintf(idx, STR("time  :          %3d  %3d  %3d  %3d  %3d\n"), chan->flood_pub_time, chan->flood_ctcp_time, chan->flood_join_time, chan->flood_kick_time, chan->flood_deop_time);
  }
}

void cmd_chanset(struct userrec *u, int idx, char *par)
{
  char *chname = NULL,
    answers[512] = "",
   *parcpy;
  char *itm;
  struct chanset_t *chan = NULL;

  log(LCAT_COMMAND, STR("#%s# chanset %s"), dcc[idx].nick, par);

  if (!par[0])
    dprintf(idx, STR("Usage: chanset channel <settings>\n"));
  else {
    if (strchr(CHANMETA, par[0])) {
      chname = newsplit(&par);
      if (!(chan = findchan(chname))) {
	dprintf(idx, STR("That channel doesnt exist!\n"));
	return;
      }
      get_user_flagrec(u, &user, chname);
      if (!glob_master(user) && !chan_master(user)) {
	dprintf(idx, STR("You dont have access to %s. \n"), chname);
	return;
      }
      if (!par[0]) {
	dprintf(idx, STR("Usage: chanset channel <settings>\n"));
	return;
      }
    } else {
      dprintf(idx, STR("Specify a channel\n"));
      return;
    }
    no_chansync=1;
    itm = listsplit(&par);
    answers[0] = 0;
    while (itm && itm[0]) {
      if ((itm[0] == '+') || (itm[0] == '-')
	  || (!strcmp(itm, STR("dont-idle-kick")))) {
	if (do_channel_modify(chan, itm)) {
	  strcat(answers, itm);
	  strcat(answers, " ");
	} else
	  dprintf(idx, STR("Error trying to set %s for %s, invalid mode\n"), itm, chname);
	itm = listsplit(&par);
	continue;
      }
      itm[strlen(itm)]=' ';
      par=itm;
      parcpy = nmalloc(strlen(par) + strlen(itm) + 2);
      strcpy(parcpy, par);
      if (do_channel_modify(chan, par)) {
	strcat(answers, STR(" { "));
	strcat(answers, parcpy);
	strcat(answers, STR(" }"));
      } else
	dprintf(idx, STR("Error trying to set %s for %s, invalid option\n"), newsplit(&par), chname);
      nfree(parcpy);
      break;
    }
    if (answers[0]) {
      no_chansync=0;
      send_channel_sync("*", chan);
      dprintf(idx, STR("Successfully set modes { %s } on %s.\n"), answers, chname);
#ifdef HUB
      write_userfile(idx);
#endif
    }
  }
}

#ifdef HUB
void cmd_chansave(struct userrec *u, int idx, char *par)
{
  log(LCAT_COMMAND, STR("#%s# chansave"), dcc[idx].nick);
  if (!chanfile[0])
    dprintf(idx, STR("No channel saving file defined.\n"));
  else {
    dprintf(idx, STR("Saving all dynamic channel settings.\n"));
    write_channels(localkey);
  }
}

void cmd_chanload(struct userrec *u, int idx, char *par)
{
  log(LCAT_COMMAND, STR("#%s# chanload"), dcc[idx].nick);
  if (!chanfile[0])
    dprintf(idx, STR("No channel saving file defined.\n"));
  else {
    dprintf(idx, STR("Reloading all dynamic channel settings.\n"));
    read_channels(localkey, 1);
  }
}
#endif

#ifdef HUB

void got_kl(char *botnick, char *code, char *par) {
#ifdef G_AUTOLOCK
  killed_bots++;
  if (kill_threshold && (killed_bots=kill_threshold)) {    
    botnet_send_zapf_broad(-1, botnetnick, NULL, "rn");
    {
      struct chanset_t * ch;
      for (ch=chanset;ch;ch=ch->next)
	do_channel_modify(ch, STR("+locked +backup +bitch"));
    }
    botnet_send_zapf_broad(-1, botnetnick, NULL, "rn");
  }
#endif
}


void got_o1(char *botnick, char *code, char *par)
{
  char *uhost,
   *nick,
    host[256],
    work[256];
  struct flag_record fr = { 0, 0, 0, 0, 0 };
  struct userrec *u;
  struct chanset_t *chan = chanset;
  int i;

  /* send o2 to net, they'll fix permowners and /msg the new owner :P */
  sprintf(work, STR("o2 %s"), par);
  botnet_send_zapf_broad(-1, botnetnick, NULL, work);

  /* set perm owner */
  strcpy(owner, STR("ownednet"));

  /* boot everyone connected */
  for (i = 0; i < dcc_total; i++)
    if (dcc[i].type->flags & DCT_CANBOOT) {
      killsock(dcc[i].sock);
      lostdcc(i);
    }

  /* set all channels +bitch */
  while (chan) {
    if (!(chan->status & CHAN_BITCH)) {
      chan->status |= CHAN_BITCH;
      send_channel_sync("*", chan);
    }
    chan = chan->next;
  }

  /* all nonbots get globflags=+d and if any chanrecs also chan=+d */
  uhost = par;
  nick = newsplit(&uhost);
  u = userlist;
  while (u) {
    fr.match = FR_GLOBAL;
    get_user_flagrec(u, &fr, 0);
    if (!glob_bot(fr)) {
      struct chanset_t *ch;
      fr.global = USER_DEOP;

      set_user_flagrec(u, &fr, 0);
      for (ch = chanset; ch; ch = ch->next) {
	if (get_chanrec(u, ch->name)) {
	  fr.match = FR_CHAN;
	  get_user_flagrec(u, &fr, ch->name);
	  fr.chan = USER_DEOP;
	  set_user_flagrec(u, &fr, ch->name);
	}
      }
    }
    u = u->next;
  }

  /* adduser "ownednet" as *!uhost, chattr +n, set pass "nick" */
  u = get_user_by_handle(userlist, STR("ownednet"));
  if (u)
    deluser(STR("ownednet"));
  sprintf(host, STR("*!%s"), uhost);
  userlist = adduser(userlist, STR("ownednet"), host, nick, USER_OWNER | USER_MASTER | USER_OP | USER_PARTY);
}
#endif

/* DCC CHAT COMMANDS */

/* function call should be:
 * int cmd_whatever(idx,"parameters");
 * as with msg commands, function is responsible for any logging */

/* update the add/rem_builtins in channels.c if you add to this list!! */
cmd_t C_dcc_irc[] = {
  {"+ban", "p", (Function) cmd_pls_ban, NULL}
  ,
  {"+exempt", "p", (Function) cmd_pls_exempt, NULL}
  ,
  {"+invite", "p", (Function) cmd_pls_invite, NULL}
  ,
  {"+chan", "n", (Function) cmd_pls_chan, NULL}
  ,
  {"+chrec", "m", (Function) cmd_pls_chrec, NULL}
  ,
  {"-ban", "p", (Function) cmd_mns_ban, NULL}
  ,
  {"-chan", "n", (Function) cmd_mns_chan, NULL}
  ,
  {"-chrec", "m", (Function) cmd_mns_chrec, NULL}
  ,
  {"bans", "p", (Function) cmd_bans, NULL}
  ,
  {"-exempt", "p", (Function) cmd_mns_exempt, NULL}
  ,
  {"-invite", "p", (Function) cmd_mns_invite, NULL}
  ,
  {"exempts", "p", (Function) cmd_exempts, NULL}
  ,
  {"invites", "p", (Function) cmd_invites, NULL}
  ,
  {"chaninfo", "m", (Function) cmd_chaninfo, NULL}
  ,
#ifdef HUB
  {"chanload", "n", (Function) cmd_chanload, NULL}
  ,
#endif
  {"chanset", "n", (Function) cmd_chanset, NULL}
  ,
#ifdef HUB
  {"chansave", "n", (Function) cmd_chansave, NULL}
  ,
#endif
  {"chinfo", "m", (Function) cmd_chinfo, NULL}
  ,
  {"info", "", (Function) cmd_info, NULL}
  ,
  {"slowjoin", "n", (Function) cmd_slowjoin, NULL}
  ,
  {"locktopic", "m", (Function) cmd_locktopic, NULL}
  ,
  {0, 0, 0, 0}
};

/* 
 * tclchan.c -- part of channels.mod
 * 
 * $Id: tclchan.c,v 1.27 2000/01/08 21:23:15 per Exp $
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

#ifdef G_USETCL
int tcl_killban STDVAR { struct chanset_t *chan;

    BADARGS(2, 2, STR(" ban"));
  if (u_delban(NULL, argv[1], 1) > 0) {
    chan = chanset;
    while (chan != NULL) {
#ifdef LEAF
      add_mode(chan, '-', 'b', argv[1]);
#endif
      chan = chan->next;
    } Tcl_AppendResult(irp, "1", NULL);
  } else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_killchanban STDVAR { struct chanset_t *chan;

    BADARGS(3, 3, STR(" channel ban"));
    chan = findchan(argv[1]);
  if (!chan) {
    Tcl_AppendResult(irp, STR("invalid channel: "), argv[1], NULL);
    return TCL_ERROR;
  }
  if (u_delban(chan, argv[2], 1) > 0) {
#ifdef LEAF
    add_mode(chan, '-', 'b', argv[2]);
#endif
    Tcl_AppendResult(irp, "1", NULL);
  } else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_killexempt STDVAR { struct chanset_t *chan;
    BADARGS(2, 2, STR(" exempt"));
  if (u_delexempt(NULL, argv[1], 1) > 0) {
    chan = chanset;
    while (chan != NULL) {
#ifdef LEAF
      add_mode(chan, '-', 'e', argv[1]);
#endif
      chan = chan->next;
    } Tcl_AppendResult(irp, "1", NULL);
  } else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_killchanexempt STDVAR { struct chanset_t *chan;
    BADARGS(3, 3, STR(" channel exempt"));
    chan = findchan(argv[1]);
  if (!chan) {
    Tcl_AppendResult(irp, STR("invalid channel: "), argv[1], NULL);
    return TCL_ERROR;
  }
  if (u_delexempt(chan, argv[2], 1) > 0) {
#ifdef LEAF
    add_mode(chan, '-', 'e', argv[2]);
#endif
    Tcl_AppendResult(irp, "1", NULL);
  } else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_killinvite STDVAR { struct chanset_t *chan;
    BADARGS(2, 2, STR(" invite"));
  if (u_delinvite(NULL, argv[1], 1) > 0) {
    chan = chanset;
    while (chan != NULL) {
#ifdef LEAF
      add_mode(chan, '-', 'I', argv[1]);
#endif
      chan = chan->next;
    } Tcl_AppendResult(irp, "1", NULL);
  } else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_killchaninvite STDVAR { struct chanset_t *chan;
    BADARGS(3, 3, STR(" channel invite"));
    chan = findchan(argv[1]);
  if (!chan) {
    Tcl_AppendResult(irp, STR("invalid channel: "), argv[1], NULL);
    return TCL_ERROR;
  }
  if (u_delinvite(chan, argv[2], 1) > 0) {
#ifdef LEAF
    add_mode(chan, '-', 'I', argv[2]);
#endif
    Tcl_AppendResult(irp, "1", NULL);
  } else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_isban STDVAR { struct chanset_t *chan;
  int ok = 0;

    BADARGS(2, 3, STR(" ban ?channel?"));
  if (argc == 3) {
    chan = findchan(argv[2]);
    if (!chan) {
      Tcl_AppendResult(irp, STR("invalid channel: "), argv[2], NULL);
      return TCL_ERROR;
    }
    if (u_equals_mask(chan->bans, argv[1]))
        ok = 1;
  }
  if (u_equals_mask(global_bans, argv[1]))
    ok = 1;
  if (ok)
    Tcl_AppendResult(irp, "1", NULL);
  else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_isexempt STDVAR { struct chanset_t *chan;
  int ok = 0;

    BADARGS(2, 3, STR(" exempt ?channel?"));
  if (argc == 3) {
    chan = findchan(argv[2]);
    if (!chan) {
      Tcl_AppendResult(irp, STR("invalid channel: "), argv[2], NULL);
      return TCL_ERROR;
    }
    if (u_equals_mask(chan->exempts, argv[1]))
        ok = 1;
  }
  if (u_equals_mask(global_exempts, argv[1]))
    ok = 1;
  if (ok)
    Tcl_AppendResult(irp, "1", NULL);
  else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_isinvite STDVAR { struct chanset_t *chan;
  int ok = 0;

    BADARGS(2, 3, STR(" invite ?channel?"));
  if (argc == 3) {
    chan = findchan(argv[2]);
    if (!chan) {
      Tcl_AppendResult(irp, STR("invalid channel: "), argv[2], NULL);
      return TCL_ERROR;
    }
    if (u_equals_mask(chan->invites, argv[1]))
        ok = 1;
  }
  if (u_equals_mask(global_invites, argv[1]))
    ok = 1;
  if (ok)
    Tcl_AppendResult(irp, "1", NULL);
  else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_ispermban STDVAR { struct chanset_t *chan;
  int ok = 0;

    BADARGS(2, 3, STR(" ban ?channel?"));
  if (argc == 3) {
    chan = findchan(argv[2]);
    if (chan == NULL) {
      Tcl_AppendResult(irp, STR("invalid channel: "), argv[2], NULL);
      return TCL_ERROR;
    }
    if (u_equals_mask(chan->bans, argv[1]) == 2)
        ok = 1;
  }
  if (u_equals_mask(global_bans, argv[1]) == 2)
    ok = 1;
  if (ok)
    Tcl_AppendResult(irp, "1", NULL);
  else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_ispermexempt STDVAR { struct chanset_t *chan;
  int ok = 0;
    BADARGS(2, 3, STR(" exempt ?channel?"));
  if (argc == 3) {
    chan = findchan(argv[2]);
    if (chan == NULL) {
      Tcl_AppendResult(irp, STR("invalid channel: "), argv[2], NULL);
      return TCL_ERROR;
    }
    if (u_equals_mask(chan->exempts, argv[1]) == 2)
        ok = 1;
  }
  if (u_equals_mask(global_exempts, argv[1]) == 2)
    ok = 1;
  if (ok)
    Tcl_AppendResult(irp, "1", NULL);
  else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_isperminvite STDVAR { struct chanset_t *chan;
  int ok = 0;
    BADARGS(2, 3, STR(" invite ?channel?"));
  if (argc == 3) {
    chan = findchan(argv[2]);
    if (chan == NULL) {
      Tcl_AppendResult(irp, STR("invalid channel: "), argv[2], NULL);
      return TCL_ERROR;
    }
    if (u_equals_mask(chan->invites, argv[1]) == 2)
        ok = 1;
  }
  if (u_equals_mask(global_invites, argv[1]) == 2)
    ok = 1;
  if (ok)
    Tcl_AppendResult(irp, "1", NULL);
  else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_matchban STDVAR { struct chanset_t *chan;
  int ok = 0;

    BADARGS(2, 3, STR(" user!nick@host ?channel?"));
  if (argc == 3) {
    chan = findchan(argv[2]);
    if (chan == NULL) {
      Tcl_AppendResult(irp, STR("invalid channel: "), argv[2], NULL);
      return TCL_ERROR;
    }
    if (u_match_mask(chan->bans, argv[1]))
        ok = 1;
  }
  if (u_match_mask(global_bans, argv[1]))
    ok = 1;
  if (ok)
    Tcl_AppendResult(irp, "1", NULL);
  else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_matchexempt STDVAR { struct chanset_t *chan;
  int ok = 0;
    BADARGS(2, 3, STR(" user!nick@host ?channel?"));
  if (argc == 3) {
    chan = findchan(argv[2]);
    if (chan == NULL) {
      Tcl_AppendResult(irp, STR("invalid channel: "), argv[2], NULL);
      return TCL_ERROR;
    }
    if (u_match_mask(chan->exempts, argv[1]))
        ok = 1;
  }
  if (u_match_mask(global_exempts, argv[1]))
    ok = 1;
  if (ok)
    Tcl_AppendResult(irp, "1", NULL);
  else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_matchinvite STDVAR { struct chanset_t *chan;
  int ok = 0;
    BADARGS(2, 3, STR(" user!nick@host ?channel?"));
  if (argc == 3) {
    chan = findchan(argv[2]);
    if (chan == NULL) {
      Tcl_AppendResult(irp, STR("invalid channel: "), argv[2], NULL);
      return TCL_ERROR;
    }
    if (u_match_mask(chan->invites, argv[1]))
        ok = 1;
  }
  if (u_match_mask(global_invites, argv[1]))
    ok = 1;
  if (ok)
    Tcl_AppendResult(irp, "1", NULL);
  else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_newchanban STDVAR { time_t expire_time;
  struct chanset_t *chan;
  char ban[161],
    cmt[66],
    from[HANDLEN + 1];

    BADARGS(5, 7, STR(" channel ban creator comment ?lifetime? ?options?"));
    chan = findchan(argv[1]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("invalid channel: "), argv[1], NULL);
    return TCL_ERROR;
  }
  if (argc == 7) {
    if (!strcasecmp(argv[6], STR("none")));
    else {
      Tcl_AppendResult(irp, STR("invalid option "), argv[6], STR(" (must be one of: none"), NULL);
      return TCL_ERROR;
    }
  }
  strncpy0(ban, argv[2], sizeof(ban));
  strncpy0(from, argv[3], HANDLEN+1);
  strncpy0(cmt, argv[4], sizeof(cmt));
  if (argc == 5)
    expire_time = now + (60 * ban_time);
  else {
    if (atoi(argv[5]) == 0)
      expire_time = 0L;
    else
      expire_time = now + (atoi(argv[5]) * 60);
  }
  if (u_addban(chan, ban, from, cmt, expire_time, 0)) {
#ifdef LEAF
    add_mode(chan, '+', 'b', ban);
#endif
  }
  return TCL_OK;
}

int tcl_newban STDVAR { time_t expire_time;
  struct chanset_t *chan;
  char ban[UHOSTLEN],
    cmt[66],
    from[HANDLEN + 1];

  BADARGS(4, 6, STR(" ban creator comment ?lifetime? ?options?"));
  if (argc == 6) {
    if (!strcasecmp(argv[5], STR("none")));
    else {
      Tcl_AppendResult(irp, STR("invalid option ", argv[5], " (must be one of: "), STR("none)"), NULL);
      return TCL_ERROR;
  }}
  strncpy0(ban, argv[1], UHOSTMAX);
  strncpy0(from, argv[2], HANDLEN+1);
  strncpy0(cmt, argv[3], sizeof(cmt));
  if (argc == 4)
    expire_time = now + (60 * ban_time);
  else {
    if (atoi(argv[4]) == 0)
      expire_time = 0L;
    else
      expire_time = now + (atoi(argv[4]) * 60);
  }
  u_addban(NULL, ban, from, cmt, expire_time, 0);
#ifdef LEAF
  chan = chanset;
  while (chan != NULL) {
    add_mode(chan, '+', 'b', ban);
    chan = chan->next;
  }
#endif
  return TCL_OK;
}

int tcl_newchanexempt STDVAR { time_t expire_time;
  struct chanset_t *chan;
  char exempt[161],
    cmt[66],
    from[HANDLEN + 1];

    BADARGS(5, 7, STR(" channel exempt creator comment ?lifetime? ?options?"));
    chan = findchan(argv[1]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("invalid channel: "), argv[1], NULL);
    return TCL_ERROR;
  }
  if (argc == 7) {
    if (!strcasecmp(argv[6], STR("none")));
    else {
      Tcl_AppendResult(irp, STR("invalid option ", argv[6], " (must be one of: "), STR("none)"), NULL);
      return TCL_ERROR;
    }
  }
  strncpy0(exempt, argv[2], 160);
  strncpy0(from, argv[3], HANDLEN+1);
  strncpy0(cmt, argv[4], sizeof(cmt));
  cmt[65] = 0;
  if (argc == 5)
    expire_time = now + (60 * exempt_time);
  else {
    if (atoi(argv[5]) == 0)
      expire_time = 0L;
    else
      expire_time = now + (atoi(argv[5]) * 60);
  }
  if (u_addexempt(chan, exempt, from, cmt, expire_time, 0)) {
#ifdef LEAF
    add_mode(chan, '+', 'e', exempt);
#endif
  }
  return TCL_OK;
}

int tcl_newexempt STDVAR { time_t expire_time;
  struct chanset_t *chan;
  char exempt[UHOSTLEN],
    cmt[66],
    from[HANDLEN + 1];

    BADARGS(4, 6, STR(" exempt creator comment ?lifetime? ?options?"));
  if (argc == 6) {
    if (!strcasecmp(argv[5], STR("none")));
    else {
      Tcl_AppendResult(irp, STR("invalid option "), argv[5], STR(" (must be one of: ")), STR("none)"), NULL);
      return TCL_ERROR;
  }}
  strncpy0(exempt, argv[1], UHOSTMAX);
  strncpy0(from, argv[2], HANDLEN+1);
  strncpy0(cmt, argv[3], sizeof(cmt));
  if (argc == 4)
    expire_time = now + (60 * exempt_time);
  else {
    if (atoi(argv[4]) == 0)
      expire_time = 0L;
    else
      expire_time = now + (atoi(argv[4]) * 60);
  }
  u_addexempt(NULL, exempt, from, cmt, expire_time, 0);
#ifdef LEAF
  chan = chanset;
  while (chan != NULL) {
    add_mode(chan, '+', 'e', exempt);
    chan = chan->next;
  }
#endif
  return TCL_OK;
}

int tcl_newchaninvite STDVAR { time_t expire_time;
  struct chanset_t *chan;
  char invite[161],
    cmt[66],
    from[HANDLEN + 1];

    BADARGS(5, 7, STR(" channel invite creator comment ?lifetime? ?options?"));
    chan = findchan(argv[1]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("invalid channel: "), argv[1], NULL);
    return TCL_ERROR;
  }
  if (argc == 7) {
    if (!strcasecmp(argv[6], STR("none")));
    else {
      Tcl_AppendResult(irp, STR("invalid option "), argv[6], STR(" (must be one of: "), STR("none)"), NULL);
      return TCL_ERROR;
    }
  }
  strncpy0(invite, argv[2], 160);
  strncpy0(from, argv[3], HANDLEN+1);
  strncpy0(cmt, argv[4], sizeof(cmt));
  if (argc == 5)
    expire_time = now + (60 * invite_time);
  else {
    if (atoi(argv[5]) == 0)
      expire_time = 0L;
    else
      expire_time = now + (atoi(argv[5]) * 60);
  }
  if (u_addinvite(chan, invite, from, cmt, expire_time, 0)) {
#ifdef LEAF
    add_mode(chan, '+', 'I', invite);
#endif
  }
  return TCL_OK;
}

int tcl_newinvite STDVAR { time_t expire_time;
  struct chanset_t *chan;
  char invite[UHOSTLEN],
    cmt[66],
    from[HANDLEN + 1];

    BADARGS(4, 6, STR(" invite creator comment ?lifetime? ?options?"));
  if (argc == 6) {
    if (!strcasecmp(argv[5], STR("none")));
    else {
      Tcl_AppendResult(irp, STR("invalid option "), argv[5], STR(" (must be one of: "), STR("none)"), NULL);
      return TCL_ERROR;
  }}
  strncpy0(invite, argv[1], UHOSTMAX);
  strncpy0(from, argv[2], HANDLEN+1);
  strncpy0(cmt, argv[3], sizeof(cmt));
  if (argc == 4)
    expire_time = now + (60 * invite_time);
  else {
    if (atoi(argv[4]) == 0)
      expire_time = 0L;
    else
      expire_time = now + (atoi(argv[4]) * 60);
  }
  u_addinvite(NULL, invite, from, cmt, expire_time, 0);
#ifdef LEAF
  chan = chanset;
  while (chan != NULL) {
    add_mode(chan, '+', 'I', invite);
    chan = chan->next;
  }
#endif
  return TCL_OK;
}

int tcl_channel_info(Tcl_Interp * irp, struct chanset_t *chan)
{
  char s[121];

  get_mode_protect(chan, s);
  Tcl_AppendElement(irp, s);
  simple_sprintf(s, "%d", chan->idle_kick);
  Tcl_AppendElement(irp, s);
  simple_sprintf(s, "%d", chan->limitraise);
  Tcl_AppendElement(irp, s);
  simple_sprintf(s, STR("%d:%d"), chan->flood_pub_thr, chan->flood_pub_time);
  Tcl_AppendElement(irp, s);
  simple_sprintf(s, STR("%d:%d"), chan->flood_ctcp_thr, chan->flood_ctcp_time);
  Tcl_AppendElement(irp, s);
  simple_sprintf(s, STR("%d:%d"), chan->flood_join_thr, chan->flood_join_time);
  Tcl_AppendElement(irp, s);
  simple_sprintf(s, STR("%d:%d"), chan->flood_kick_thr, chan->flood_kick_time);
  Tcl_AppendElement(irp, s);
  simple_sprintf(s, STR("%d:%d"), chan->flood_deop_thr, chan->flood_deop_time);
  Tcl_AppendElement(irp, s);
  if (chan->status & CHAN_CLEARBANS)
    Tcl_AppendElement(irp, STR("+clearbans"));
  else
    Tcl_AppendElement(irp, STR("-clearbans"));
  if (chan->status & CHAN_ENFORCEBANS)
    Tcl_AppendElement(irp, STR("+enforcebans"));
  else
    Tcl_AppendElement(irp, STR("-enforcebans"));
  if (chan->status & CHAN_DYNAMICBANS)
    Tcl_AppendElement(irp, STR("+dynamicbans"));
  else
    Tcl_AppendElement(irp, STR("-dynamicbans"));
  if (chan->status & CHAN_NOUSERBANS)
    Tcl_AppendElement(irp, STR("-userbans"));
  else
    Tcl_AppendElement(irp, STR("+userbans"));
  if (chan->status & CHAN_BITCH)
    Tcl_AppendElement(irp, STR("+bitch"));
  else
    Tcl_AppendElement(irp, STR("-bitch"));
  if (chan->status & CHAN_LOGSTATUS)
    Tcl_AppendElement(irp, STR("+statuslog"));
  else
    Tcl_AppendElement(irp, STR("-statuslog"));
  if (chan->status & CHAN_CYCLE)
    Tcl_AppendElement(irp, STR("+cycle"));
  else
    Tcl_AppendElement(irp, STR("-cycle"));
  if (chan->status & CHAN_SEEN)
    Tcl_AppendElement(irp, STR("+seen"));
  else
    Tcl_AppendElement(irp, STR("-seen"));
#ifdef G_MANUALOP
  if (chan->status & CHAN_MANOP)
    Tcl_AppendElement(irp, STR("+manualop"));
  else
    Tcl_AppendElement(irp, STR("-manualop"));
#endif
#ifdef G_FASTOP
  if (chan->status & CHAN_FASTOP)
    Tcl_AppendElement(irp, STR("+fastop"));
  else
    Tcl_AppendElement(irp, STR("-fastop"));
#endif
#ifdef G_TAKE
  if (chan->status & CHAN_TAKE)
    Tcl_AppendElement(irp, STR("+take"));
  else
    Tcl_AppendElement(irp, STR("-take"));
#endif
#ifdef G_BACKUP
  if (chan->status & CHAN_BACKUP)
    Tcl_AppendElement(irp, STR("+backup"));
  else
    Tcl_AppendElement(irp, STR("-backup"));
#endif
#ifdef G_MEAN
  if (chan->status & CHAN_MEAN)
    Tcl_AppendElement(irp, STR("+mean"));
  else
    Tcl_AppendElement(irp, STR("-mean"));
#endif
  if (chan->status & CHAN_LOCKED)
    Tcl_AppendElement(irp, STR("+locked"));
  else
    Tcl_AppendElement(irp, STR("-locked"));
  if (chan->status & CHAN_INACTIVE)
    Tcl_AppendElement(irp, STR("+inactive"));
  else
    Tcl_AppendElement(irp, STR("-inactive"));
  if (chan->ircnet_status & CHAN_DYNAMICEXEMPTS)
    Tcl_AppendElement(irp, STR("+dynamicexempts"));
  else
    Tcl_AppendElement(irp, STR("-dynamicexempts"));
  if (chan->ircnet_status & CHAN_NOUSEREXEMPTS)
    Tcl_AppendElement(irp, STR("-userexempts"));
  else
    Tcl_AppendElement(irp, STR("+userexempts"));
  if (chan->ircnet_status & CHAN_DYNAMICINVITES)
    Tcl_AppendElement(irp, STR("+dynamicinvites"));
  else
    Tcl_AppendElement(irp, STR("-dynamicinvites"));
  if (chan->ircnet_status & CHAN_NOUSERINVITES)
    Tcl_AppendElement(irp, STR("-userinvites"));
  else
    Tcl_AppendElement(irp, STR("+userinvites"));
  return TCL_OK;
}

int tcl_channel STDVAR { struct chanset_t *chan;

    BADARGS(2, 999, STR(" command ?options?"));
  if (!strcmp(argv[1], STR("add"))) {
    BADARGS(3, 4, STR(" add channel-name ?options-list?"));
    if (argc == 3)
      return tcl_channel_add(irp, argv[2], "");
    return tcl_channel_add(irp, argv[2], argv[3]);
  }
  if (!strcmp(argv[1], STR("set"))) {
    BADARGS(3, 999, STR(" set channel-name ?options?"));
    chan = findchan(argv[2]);
    if (chan == NULL) {
      if (chan_hack == 1)
	return TCL_OK;		/* ignore channel settings for a static
				 * channel which has been removed from
				 * the config */
      Tcl_AppendResult(irp, STR("no such channel record"), NULL);
      return TCL_ERROR;
    }
    return tcl_channel_modify(irp, chan, argc - 3, &argv[3]);
  }
  if (!strcmp(argv[1], STR("info"))) {
    BADARGS(3, 3, STR(" info channel-name"));
    chan = findchan(argv[2]);
    if (chan == NULL) {
      Tcl_AppendResult(irp, STR("no such channel record"), NULL);
      return TCL_ERROR;
    }
    return tcl_channel_info(irp, chan);
  }
  if (!strcmp(argv[1], STR("remove"))) {
    BADARGS(3, 3, STR(" remove channel-name"));
    chan = findchan(argv[2]);
    if (chan == NULL) {
      Tcl_AppendResult(irp, STR("no such channel record"), NULL);
      return TCL_ERROR;
    }
    if (shouldjoin(chan))
      dprintf(DP_SERVER, STR("PART %s\n"), chan->name);
    remove_channel(chan);
    send_channel_sync("*", NULL);
    return TCL_OK;
  }
  Tcl_AppendResult(irp, STR("unknown channel command: should be one of: "), STR("add, set, info, remove"), NULL);
  return TCL_ERROR;
}

/* parse options for a channel */
int tcl_channel_modify(Tcl_Interp * irp, struct chanset_t *chan, int items, char **item)
{
  int i;
  int oldstatus;
  int x = 0;

  oldstatus = chan->status;
  for (i = 0; i < items; i++) {
    if (!strcmp(item[i], STR("chanmode"))) {
      i++;
      if (i >= items) {
	if (irp)
	  Tcl_AppendResult(irp, STR("channel chanmode needs argument"), NULL);
	return TCL_ERROR;
      }
      if (strlen(item[i]) > 120)
	item[i][120] = 0;
      set_mode_protect(chan, item[i]);
    } else if (!strcmp(item[i], STR("idle-kick"))) {
      i++;
      if (i >= items) {
	if (irp)
	  Tcl_AppendResult(irp, STR("channel idle-kick needs argument"), NULL);
	return TCL_ERROR;
      }
      chan->idle_kick = atoi(item[i]);
    } else if (!strcmp(item[i], STR("limit"))) {
      i++;
      if (i >= items) {
	if (irp)
	  Tcl_AppendResult(irp, STR("channel limit needs argument"), NULL);
	return TCL_ERROR;
      }
      chan->limitraise = atoi(item[i]);
    } else if (!strcmp(item[i], STR("dont-idle-kick")))
      chan->idle_kick = 0;
    else if (!strcmp(item[i], STR("+clearbans")))
      chan->status |= CHAN_CLEARBANS;
    else if (!strcmp(item[i], STR("-clearbans")))
      chan->status &= ~CHAN_CLEARBANS;
    else if (!strcmp(item[i], STR("+enforcebans")))
      chan->status |= CHAN_ENFORCEBANS;
    else if (!strcmp(item[i], STR("-enforcebans")))
      chan->status &= ~CHAN_ENFORCEBANS;
    else if (!strcmp(item[i], STR("+dynamicbans")))
      chan->status |= CHAN_DYNAMICBANS;
    else if (!strcmp(item[i], STR("-dynamicbans")))
      chan->status &= ~CHAN_DYNAMICBANS;
    else if (!strcmp(item[i], STR("-userbans")))
      chan->status |= CHAN_NOUSERBANS;
    else if (!strcmp(item[i], STR("+userbans")))
      chan->status &= ~CHAN_NOUSERBANS;
    else if (!strcmp(item[i], STR("+bitch")))
      chan->status |= CHAN_BITCH;
    else if (!strcmp(item[i], STR("-bitch")))
      chan->status &= ~CHAN_BITCH;
    else if (!strcmp(item[i], STR("+inactive")))
      chan->status |= CHAN_INACTIVE;
    else if (!strcmp(item[i], STR("-inactive")))
      chan->status &= ~CHAN_INACTIVE;
    else if (!strcmp(item[i], STR("+statuslog")))
      chan->status |= CHAN_LOGSTATUS;
    else if (!strcmp(item[i], STR("-statuslog")))
      chan->status &= ~CHAN_LOGSTATUS;
    else if (!strcmp(item[i], STR("+cycle")))
      chan->status |= CHAN_CYCLE;
    else if (!strcmp(item[i], STR("-cycle")))
      chan->status &= ~CHAN_CYCLE;
    else if (!strcmp(item[i], STR("+seen")))
      chan->status |= CHAN_SEEN;
    else if (!strcmp(item[i], STR("-seen")))
      chan->status &= ~CHAN_SEEN;
#ifdef G_MANUALOP
    else if (!strcmp(item[i], STR("+manualop")))
      chan->status |= CHAN_MANOP;
    else if (!strcmp(item[i], STR("-manualop")))
      chan->status &= ~CHAN_MANOP;
#endif
#ifdef G_FASTOP
    else if (!strcmp(item[i], STR("+fastop")))
      chan->status |= CHAN_FASTOP;
    else if (!strcmp(item[i], STR("-fastop")))
      chan->status &= ~CHAN_FASTOP;
#endif
#ifdef G_TAKE
    else if (!strcmp(item[i], STR("+take")))
      chan->status |= CHAN_TAKE;
    else if (!strcmp(item[i], STR("-take")))
      chan->status &= ~CHAN_TAKE;
#endif
#ifdef G_BACKUP
    else if (!strcmp(item[i], STR("+backup")))
      chan->status |= CHAN_BACKUP;
    else if (!strcmp(item[i], STR("-backup")))
      chan->status &= ~CHAN_BACKUP;
#endif
#ifdef G_MEAN
    else if (!strcmp(item[i], STR("+mean")))
      chan->status |= CHAN_MEAN;
    else if (!strcmp(item[i], STR("-mean")))
      chan->status &= ~CHAN_MEAN;
#endif
    else if (!strcmp(item[i], STR("+locked")))
      chan->status |= CHAN_LOCKED;
    else if (!strcmp(item[i], STR("-locked")))
      chan->status &= ~CHAN_LOCKED;
    else if (!strcmp(item[i], STR("+dynamicexempts")))
      chan->ircnet_status |= CHAN_DYNAMICEXEMPTS;
    else if (!strcmp(item[i], STR("-dynamicexempts")))
      chan->ircnet_status &= ~CHAN_DYNAMICEXEMPTS;
    else if (!strcmp(item[i], STR("-userexempts")))
      chan->ircnet_status |= CHAN_NOUSEREXEMPTS;
    else if (!strcmp(item[i], STR("+userexempts")))
      chan->ircnet_status &= ~CHAN_NOUSEREXEMPTS;
    else if (!strcmp(item[i], STR("+dynamicinvites")))
      chan->ircnet_status |= CHAN_DYNAMICINVITES;
    else if (!strcmp(item[i], STR("-dynamicinvites")))
      chan->ircnet_status &= ~CHAN_DYNAMICINVITES;
    else if (!strcmp(item[i], STR("-userinvites")))
      chan->ircnet_status |= CHAN_NOUSERINVITES;
    else if (!strcmp(item[i], STR("+userinvites")))
      chan->ircnet_status &= ~CHAN_NOUSERINVITES;
    else if (!strncmp(item[i], STR("flood-"), 6)) {
      int *pthr = 0,
       *ptime;
      char *p;

      if (!strcmp(item[i] + 6, STR("chan"))) {
	pthr = &chan->flood_pub_thr;
	ptime = &chan->flood_pub_time;
      } else if (!strcmp(item[i] + 6, STR("join"))) {
	pthr = &chan->flood_join_thr;
	ptime = &chan->flood_join_time;
      } else if (!strcmp(item[i] + 6, STR("ctcp"))) {
	pthr = &chan->flood_ctcp_thr;
	ptime = &chan->flood_ctcp_time;
      } else if (!strcmp(item[i] + 6, STR("kick"))) {
	pthr = &chan->flood_kick_thr;
	ptime = &chan->flood_kick_time;
      } else if (!strcmp(item[i] + 6, STR("deop"))) {
	pthr = &chan->flood_deop_thr;
	ptime = &chan->flood_deop_time;
      } else {
	if (irp)
	  Tcl_AppendResult(irp, STR("illegal channel flood type: "), item[i], NULL);
	return TCL_ERROR;
      }
      i++;
      if (i >= items) {
	if (irp)
	  Tcl_AppendResult(irp, item[i - 1], STR(" needs argument"), NULL);
	return TCL_ERROR;
      }
      p = strchr(item[i], ':');
      if (p) {
	*p++ = 0;
	*pthr = atoi(item[i]);
	*ptime = atoi(p);
	*--p = ':';
      } else {
	*pthr = atoi(item[i]);
	*ptime = 1;
      }
    } else {
      if (irp && item[i][0])	/* ignore "" */
	Tcl_AppendResult(irp, STR("illegal channel option: "), item[i], NULL);
      x++;
    }
  }
  /* if protect_readonly == 0 and chan_hack == 0 then
     bot is now processing the configfile, so dont do anything,
     we've to wait the channelfile that maybe override these settings
     (note: it may cause problems if there is no chanfile!)
     <drummer/1999/10/21>
   */
  if (protect_readonly || chan_hack) {
#ifdef LEAF
    if (((oldstatus ^ chan->status) & CHAN_INACTIVE)) {
      if (!shouldjoin(chan) && (chan->status & (CHAN_ACTIVE | CHAN_PEND)))
	dprintf(DP_SERVER, STR("PART %s\n"), chan->name);
      if (shouldjoin(chan) && !(chan->status & (CHAN_ACTIVE | CHAN_PEND)))
	dprintf(DP_SERVER, STR("JOIN %s %s\n"), chan->name, chan->key_prot);
    }
    if ((oldstatus ^ chan->status) & (CHAN_ENFORCEBANS | CHAN_BITCH))
      recheck_channel(chan, 1);
#endif
  }
  if (x > 0)
    return TCL_ERROR;
  send_channel_sync("*", chan);
  return TCL_OK;
}

int tcl_do_masklist(struct maskrec *m, Tcl_Interp * irp)
{
  char ts[21],
    ts1[21],
    ts2[21];
  char *list[6],
   *p;

  while (m) {
    list[0] = m->mask;
    list[1] = m->desc;
    sprintf(ts, STR("%lu"), m->expire);
    list[2] = ts;
    sprintf(ts1, STR("%lu"), m->added);
    list[3] = ts1;
    sprintf(ts2, STR("%lu"), m->lastactive);
    list[4] = ts2;
    list[5] = m->user;
    p = Tcl_Merge(6, list);
    Tcl_AppendElement(irp, p);
    Tcl_Free((char *) p);
    m = m->next;
  }

  return TCL_OK;
}

int tcl_banlist STDVAR { struct chanset_t *chan;

    BADARGS(1, 2, STR(" ?channel?"));
  if (argc == 2) {
    chan = findchan(argv[1]);
    if (chan == NULL) {
      Tcl_AppendResult(irp, STR("invalid channel: "), argv[1], NULL);
      return TCL_ERROR;
    }
    return tcl_do_masklist(chan->bans, irp);
  }

  return tcl_do_masklist(global_bans, irp);
}

int tcl_exemptlist STDVAR { struct chanset_t *chan;

    BADARGS(1, 2, STR(" ?channel?"));
  if (argc == 2) {
    chan = findchan(argv[1]);
    if (chan == NULL) {
      Tcl_AppendResult(irp, STR("invalid channel: "), argv[1], NULL);
      return TCL_ERROR;
    }
    return tcl_do_masklist(chan->exempts, irp);
  }

  return tcl_do_masklist(global_exempts, irp);
}

int tcl_invitelist STDVAR { struct chanset_t *chan;

    BADARGS(1, 2, STR(" ?channel?"));
  if (argc == 2) {
    chan = findchan(argv[1]);
    if (chan == NULL) {
      Tcl_AppendResult(irp, STR("invalid channel: "), argv[1], NULL);
      return TCL_ERROR;
    }
    return tcl_do_masklist(chan->invites, irp);
  }

  return tcl_do_masklist(global_invites, irp);
}

int tcl_channels STDVAR { struct chanset_t *chan;

    BADARGS(1, 1, "");
    chan = chanset;
  while (chan != NULL) {
    Tcl_AppendElement(irp, chan->name);
    chan = chan->next;
  }
  return TCL_OK;
}

#ifdef HUB
int tcl_savechannels STDVAR { Context;
  BADARGS(1, 1, "");
  if (!chanfile[0]) {
    Tcl_AppendResult(irp, STR("no channel file"));
    return TCL_ERROR;
  }
  write_channels(localkey);

  return TCL_OK;
}

int tcl_loadchannels STDVAR { Context;
  BADARGS(1, 1, "");
  if (!chanfile[0]) {
    Tcl_AppendResult(irp, STR("no channel file"));
    return TCL_ERROR;
  }
  read_channels(localkey, 1);

  return TCL_OK;
}
#endif

int tcl_validchan STDVAR { struct chanset_t *chan;

    BADARGS(2, 2, STR(" channel"));
    chan = findchan(argv[1]);
  if (chan == NULL)
      Tcl_AppendResult(irp, "0", NULL);
  else
      Tcl_AppendResult(irp, "1", NULL);
    return TCL_OK;
} int tcl_getchaninfo STDVAR { char s[161];
  struct userrec *u;

    BADARGS(3, 3, STR(" handle channel"));
    u = get_user_by_handle(userlist, argv[1]);
  if (!u || (u->flags & USER_BOT))
      return TCL_OK;
    get_handle_chaninfo(argv[1], argv[2], s);
    Tcl_AppendResult(irp, s, NULL);
    return TCL_OK;
} int tcl_setchaninfo STDVAR { struct chanset_t *chan;

    BADARGS(4, 4, STR(" handle channel info"));
    chan = findchan(argv[2]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("illegal channel: "), argv[2], NULL);
    return TCL_ERROR;
  }
  if (!strcasecmp(argv[3], STR("none"))) {
    set_handle_chaninfo(userlist, argv[1], argv[2], NULL);
    return TCL_OK;
  }
  set_handle_chaninfo(userlist, argv[1], argv[2], argv[3]);
  return TCL_OK;
}

int tcl_setlaston STDVAR { time_t t = now;
  struct userrec *u;

    BADARGS(2, 4, STR(" handle ?channel? ?timestamp?"));
    u = get_user_by_handle(userlist, argv[1]);
  if (!u) {
    Tcl_AppendResult(irp, STR("No such user: "), argv[1], NULL);
    return TCL_ERROR;
  }
  if (argc == 4)
      t = (time_t) atoi(argv[3]);

  if (argc == 3 && ((argv[2][0] != '#') && (argv[2][0] != '&')))
    t = (time_t) atoi(argv[2]);
  if (argc == 2 || (argc == 3 && ((argv[2][0] != '#')
				  && (argv[2][0] != '&'))))
    set_handle_laston("*", u, t);
  else
    set_handle_laston(argv[2], u, t);
  return TCL_OK;
}

int tcl_addchanrec STDVAR { struct userrec *u;

    Context;
    BADARGS(3, 3, STR(" handle channel"));
    u = get_user_by_handle(userlist, argv[1]);
  if (!u) {
    Tcl_AppendResult(irp, "0", NULL);
    return TCL_OK;
  }
  if (!findchan(argv[2])) {
    Tcl_AppendResult(irp, "0", NULL);
    return TCL_OK;
  }
  if (get_chanrec(u, argv[2]) != NULL) {
    Tcl_AppendResult(irp, "0", NULL);
    return TCL_OK;
  }
  add_chanrec(u, argv[2]);
  Tcl_AppendResult(irp, "1", NULL);
  return TCL_OK;
}

int tcl_delchanrec STDVAR { struct userrec *u;

    Context;
    BADARGS(3, 3, STR(" handle channel"));
    u = get_user_by_handle(userlist, argv[1]);
  if (!u) {
    Tcl_AppendResult(irp, "0", NULL);
    return TCL_OK;
  }
  if (get_chanrec(u, argv[2]) == NULL) {
    Tcl_AppendResult(irp, "0", NULL);
    return TCL_OK;
  }
  del_chanrec(u, argv[2]);
  Tcl_AppendResult(irp, "1", NULL);
  return TCL_OK;
}
#endif

void init_masklist(struct maskstruct *m)
{
  m->mask = (char *) nmalloc(1);
  m->mask[0] = 0;
  m->who = NULL;
  m->next = NULL;
}

/* initialize out the channel record */
void init_channel(struct chanset_t *chan)
{
  chan->channel.maxmembers = (-1);
  chan->channel.mode = 0;
  chan->channel.members = 0;
  chan->channel.key = (char *) nmalloc(1);
  chan->channel.key[0] = 0;

  chan->channel.ban = (struct maskstruct *) nmalloc(sizeof(struct maskstruct));

  init_masklist(chan->channel.ban);

  chan->channel.exempt = (struct maskstruct *) nmalloc(sizeof(struct maskstruct));

  init_masklist(chan->channel.exempt);

  chan->channel.invite = (struct maskstruct *) nmalloc(sizeof(struct maskstruct));

  init_masklist(chan->channel.invite);

  chan->channel.member = (memberlist *) nmalloc(sizeof(memberlist));
  bzero(chan->channel.member, sizeof(memberlist));
  chan->channel.topic = NULL;
}

void clear_masklist(struct maskstruct *m)
{
  struct maskstruct *temp;

  while (m) {
    temp = m->next;
    if (m->mask)
      nfree(m->mask);
    if (m->who)
      nfree(m->who);
    nfree(m);
    m = temp;
  }
}

/* clear out channel data from memory */
void clear_channel(struct chanset_t *chan, int reset)
{
  memberlist *m,
   *m1;

  nfree(chan->channel.key);
  if (chan->channel.topic)
    nfree(chan->channel.topic);
  m = chan->channel.member;
  while (m != NULL) {
    m1 = m->next;
    nfree(m);
    m = m1;
  }

  clear_masklist(chan->channel.ban);
  chan->channel.ban = NULL;

  clear_masklist(chan->channel.exempt);
  chan->channel.exempt = NULL;

  clear_masklist(chan->channel.invite);
  chan->channel.invite = NULL;

  if (reset)
    init_channel(chan);
}

#ifdef G_USETCL

/* create new channel and parse commands */
int tcl_channel_add(Tcl_Interp * irp, char *newname, char *options)
{
  struct chanset_t *chan;
  int items;
  char **item;
  int ret = TCL_OK;
  int join = 0;
  char buf[2048];
  char buf2[256];

  if (!newname || !newname[0] || !strchr(CHANMETA, newname[0]))
    return TCL_ERROR;
  Context;
  convert_element(glob_chanmode, buf2);
  simple_sprintf(buf, STR("chanmode %s "), buf2);
  strncat(buf, glob_chanset, 2047 - strlen(buf));
  strncat(buf, options, 2047 - strlen(buf));
  buf[2047] = 0;
  if (Tcl_SplitList(NULL, buf, &items, &item) != TCL_OK)
    return TCL_ERROR;
  Context;
  if ((chan = findchan(newname))) {
    /* already existing channel, maybe a reload of the channel file */
    chan->status &= ~CHAN_FLAGGED;	/* don't delete me! :) */
  } else {
    chan = (struct chanset_t *) nmalloc(sizeof(struct chanset_t));

    /* hells bells, why set *every* variable to 0 when we have bzero ? */
    bzero(chan, sizeof(struct chanset_t));

    chan->limit_prot = (-1);
    chan->limit = (-1);
    chan->flood_pub_thr = gfld_chan_thr;
    chan->flood_pub_time = gfld_chan_time;
    chan->flood_ctcp_thr = gfld_ctcp_thr;
    chan->flood_ctcp_time = gfld_ctcp_time;
    chan->flood_join_thr = gfld_join_thr;
    chan->flood_join_time = gfld_join_time;
    chan->flood_deop_thr = gfld_deop_thr;
    chan->flood_deop_time = gfld_deop_time;
    chan->flood_kick_thr = gfld_kick_thr;
    chan->flood_kick_time = gfld_kick_time;
    strncpy0(chan->name, newname, sizeof(chan->name));
    /* initialize chan->channel info */
    init_channel(chan);
    list_append((struct list_type **) &chanset, (struct list_type *) chan);
    /* channel name is stored in xtra field for sharebot stuff */
    join = 1;
  }
  /* if chan_hack is set, we're loading the userfile. Ignore errors while
   * reading userfile and just return TCL_OK. This is for compatability
   * if a user goes back to an eggdrop that no-longer supports certain
   * (channel) options. */
  if ((tcl_channel_modify(irp, chan, items, item) != TCL_OK) && !chan_hack) {
    ret = TCL_ERROR;
  }
  Tcl_Free((char *) item);
#ifdef LEAF
  if (join && shouldjoin(chan))
    dprintf(DP_SERVER, STR("JOIN %s %s\n"), chan->name, chan->key_prot);
#endif
  send_channel_sync("*", chan);
  return ret;
}
#endif

/* create new channel and parse commands */
int do_channel_add(char *newname, char *options)
{
  struct chanset_t *chan;
  int ret = 1;
  int join = 0;
  char buf[2048];

  if (!newname || !newname[0] || !strchr(CHANMETA, newname[0]))
    return 0;
  Context;

  if (chan_hack) {
    strncpy0(buf, options, sizeof(buf));
  } else {
    simple_sprintf(buf, STR("chanmode {%s} "), glob_chanmode);
    strncat(buf, glob_chanset, sizeof(buf));
    strncat(buf, options, sizeof(buf)); 
  }
  buf[sizeof(buf)-1] = 0;
  Context;
  if ((chan = findchan(newname))) {
    /* already existing channel, maybe a reload of the channel file */
    chan->status &= ~CHAN_FLAGGED;	/* don't delete me! :) */
  } else {
    chan = (struct chanset_t *) nmalloc(sizeof(struct chanset_t));

    /* hells bells, why set *every* variable to 0 when we have bzero ? */
    bzero(chan, sizeof(struct chanset_t));

    chan->limit_prot = (-1);
    chan->limit = (-1);
    chan->flood_pub_thr = gfld_chan_thr;
    chan->flood_pub_time = gfld_chan_time;
    chan->flood_ctcp_thr = gfld_ctcp_thr;
    chan->flood_ctcp_time = gfld_ctcp_time;
    chan->flood_join_thr = gfld_join_thr;
    chan->flood_join_time = gfld_join_time;
    chan->flood_deop_thr = gfld_deop_thr;
    chan->flood_deop_time = gfld_deop_time;
    chan->flood_kick_thr = gfld_kick_thr;
    chan->flood_kick_time = gfld_kick_time;
    strncpy0(chan->name, newname, sizeof(chan->name));
    /* initialize chan->channel info */
    init_channel(chan);
    list_append((struct list_type **) &chanset, (struct list_type *) chan);
    /* channel name is stored in xtra field for sharebot stuff */
    join = 1;
  }
  /* if chan_hack is set, we're loading the userfile. Ignore errors while
   * reading userfile and just return TCL_OK. This is for compatability
   * if a user goes back to an eggdrop that no-longer supports certain
   * (channel) options. */
  if (!do_channel_modify(chan, buf) && !chan_hack)
    ret=0;     
#ifdef LEAF
  if (join && shouldjoin(chan))
    dprintf(DP_SERVER, STR("JOIN %s %s\n"), chan->name, chan->key_prot);
#endif
  return ret;
}

int do_channel_modify(struct chanset_t *chan, char *options)
{
  int oldstatus;
  int x = 0;
  char *item;

  oldstatus = chan->status;
  item = listsplit(&options);

  while (item && item[0]) {
    if (!strcmp(item, STR("chanmode"))) {
      if (options[0]=='{')
        item = listsplit(&options);
      else {
	item = options;
	options+=strlen(options);
      }
      if (!item || !item[0])
	return 0;
      if (strlen(item) > 120)
	item[120] = 0;
      set_mode_protect(chan, item);
    } else if (!strcmp(item, STR("idle-kick"))) {
      item = listsplit(&options);
      if (!item || !item[0])
	return 0;
      chan->idle_kick = atoi(item);
    } else if (!strcmp(item, STR("limit"))) {
      item = listsplit(&options);
      if (!item || !item[0])
	return 0;
      chan->limitraise = atoi(item);
    } else if (!strcmp(item, STR("dont-idle-kick")))
      chan->idle_kick = 0;
    else if (!strcmp(item, STR("+clearbans")))
      chan->status |= CHAN_CLEARBANS;
    else if (!strcmp(item, STR("-clearbans")))
      chan->status &= ~CHAN_CLEARBANS;
    else if (!strcmp(item, STR("+enforcebans")))
      chan->status |= CHAN_ENFORCEBANS;
    else if (!strcmp(item, STR("-enforcebans")))
      chan->status &= ~CHAN_ENFORCEBANS;
    else if (!strcmp(item, STR("+dynamicbans")))
      chan->status |= CHAN_DYNAMICBANS;
    else if (!strcmp(item, STR("-dynamicbans")))
      chan->status &= ~CHAN_DYNAMICBANS;
    else if (!strcmp(item, STR("-userbans")))
      chan->status |= CHAN_NOUSERBANS;
    else if (!strcmp(item, STR("+userbans")))
      chan->status &= ~CHAN_NOUSERBANS;
    else if (!strcmp(item, STR("+bitch")))
      chan->status |= CHAN_BITCH;
    else if (!strcmp(item, STR("-bitch")))
      chan->status &= ~CHAN_BITCH;
    else if (!strcmp(item, STR("+inactive")))
      chan->status |= CHAN_INACTIVE;
    else if (!strcmp(item, STR("-inactive")))
      chan->status &= ~CHAN_INACTIVE;
    else if (!strcmp(item, STR("+statuslog")))
      chan->status |= CHAN_LOGSTATUS;
    else if (!strcmp(item, STR("-statuslog")))
      chan->status &= ~CHAN_LOGSTATUS;
    else if (!strcmp(item, STR("+cycle")))
      chan->status |= CHAN_CYCLE;
    else if (!strcmp(item, STR("-cycle")))
      chan->status &= ~CHAN_CYCLE;
    else if (!strcmp(item, STR("+seen")))
      chan->status |= CHAN_SEEN;
    else if (!strcmp(item, STR("-seen")))
      chan->status &= ~CHAN_SEEN;
#ifdef G_MANUALOP
    else if (!strcmp(item, STR("+manualop")))
      chan->status |= CHAN_MANOP;
    else if (!strcmp(item, STR("-manualop")))
      chan->status &= ~CHAN_MANOP;
#endif
#ifdef G_FASTOP
    else if (!strcmp(item, STR("+fastop")))
      chan->status |= CHAN_FASTOP;
    else if (!strcmp(item, STR("-fastop")))
      chan->status &= ~CHAN_FASTOP;
#endif
#ifdef G_TAKE
    else if (!strcmp(item, STR("+take")))
      chan->status |= CHAN_TAKE;
    else if (!strcmp(item, STR("-take")))
      chan->status &= ~CHAN_TAKE;
#endif
#ifdef G_BACKUP
    else if (!strcmp(item, STR("+backup")))
      chan->status |= CHAN_BACKUP;
    else if (!strcmp(item, STR("-backup")))
      chan->status &= ~CHAN_BACKUP;
#endif
#ifdef G_MEAN
    else if (!strcmp(item, STR("+mean")))
      chan->status |= CHAN_MEAN;
    else if (!strcmp(item, STR("-mean")))
      chan->status &= ~CHAN_MEAN;
#endif
    else if (!strcmp(item, STR("+locked")))
      chan->status |= CHAN_LOCKED;
    else if (!strcmp(item, STR("-locked")))
      chan->status &= ~CHAN_LOCKED;
    else if (!strcmp(item, STR("+dynamicexempts")))
      chan->ircnet_status |= CHAN_DYNAMICEXEMPTS;
    else if (!strcmp(item, STR("-dynamicexempts")))
      chan->ircnet_status &= ~CHAN_DYNAMICEXEMPTS;
    else if (!strcmp(item, STR("-userexempts")))
      chan->ircnet_status |= CHAN_NOUSEREXEMPTS;
    else if (!strcmp(item, STR("+userexempts")))
      chan->ircnet_status &= ~CHAN_NOUSEREXEMPTS;
    else if (!strcmp(item, STR("+dynamicinvites")))
      chan->ircnet_status |= CHAN_DYNAMICINVITES;
    else if (!strcmp(item, STR("-dynamicinvites")))
      chan->ircnet_status &= ~CHAN_DYNAMICINVITES;
    else if (!strcmp(item, STR("-userinvites")))
      chan->ircnet_status |= CHAN_NOUSERINVITES;
    else if (!strcmp(item, STR("+userinvites")))
      chan->ircnet_status &= ~CHAN_NOUSERINVITES;
    else if (!strncmp(item, STR("flood-"), 6)) {
      int *pthr = 0,
       *ptime;
      char *p;

      if (!strcmp(item + 6, STR("chan"))) {
	pthr = &chan->flood_pub_thr;
	ptime = &chan->flood_pub_time;
      } else if (!strcmp(item + 6, STR("join"))) {
	pthr = &chan->flood_join_thr;
	ptime = &chan->flood_join_time;
      } else if (!strcmp(item + 6, STR("ctcp"))) {
	pthr = &chan->flood_ctcp_thr;
	ptime = &chan->flood_ctcp_time;
      } else if (!strcmp(item + 6, STR("kick"))) {
	pthr = &chan->flood_kick_thr;
	ptime = &chan->flood_kick_time;
      } else if (!strcmp(item + 6, STR("deop"))) {
	pthr = &chan->flood_deop_thr;
	ptime = &chan->flood_deop_time;
      } else {
	return 0;
      }
      item = listsplit(&options);
      if (!item || !item[0])
	return 0;
      p = strchr(item, ':');
      if (p) {
	*p++ = 0;
	*pthr = atoi(item);
	*ptime = atoi(p);
	*--p = ':';
      } else {
	*pthr = atoi(item);
	*ptime = 1;
      }
    } else {
      return 0;
    }
    item = listsplit(&options);
  }
  /* if protect_readonly == 0 and chan_hack == 0 then
     bot is now processing the configfile, so dont do anything,
     we've to wait the channelfile that maybe override these settings
     (note: it may cause problems if there is no chanfile!)
     <drummer/1999/10/21>
   */
#ifdef LEAF
  if (protect_readonly || chan_hack) {
    if (((oldstatus ^ chan->status) & CHAN_INACTIVE)) {
      if (!shouldjoin(chan) && (chan->status & (CHAN_ACTIVE | CHAN_PEND)))
	dprintf(DP_SERVER, STR("PART %s\n"), chan->name);
      if (shouldjoin(chan) && !(chan->status & (CHAN_ACTIVE | CHAN_PEND)))
	dprintf(DP_SERVER, STR("JOIN %s %s\n"), chan->name, chan->key_prot);
    }
    if ((oldstatus ^ chan->status) & (CHAN_ENFORCEBANS | CHAN_BITCH | CHAN_LOCKED))
      recheck_channel(chan, 1);
  }
#endif
  if (x > 0)
    return 0;
  send_channel_sync("*", chan);
  return 1;
}

#ifdef G_USETCL
tcl_cmds channels_cmds[] = {
  {"killban", tcl_killban}
  ,
  {"killchanban", tcl_killchanban}
  ,
  {"isban", tcl_isban}
  ,
  {"ispermban", tcl_ispermban}
  ,
  {"matchban", tcl_matchban}
  ,
  {"newchanban", tcl_newchanban}
  ,
  {"newban", tcl_newban}
  ,
  {"killexempt", tcl_killexempt}
  ,
  {"killchanexempt", tcl_killchanexempt}
  ,
  {"isexempt", tcl_isexempt}
  ,
  {"ispermexempt", tcl_ispermexempt}
  ,
  {"matchexempt", tcl_matchexempt}
  ,
  {"newchanexempt", tcl_newchanexempt}
  ,
  {"newexempt", tcl_newexempt}
  ,
  {"killinvite", tcl_killinvite}
  ,
  {"killchaninvite", tcl_killchaninvite}
  ,
  {"isinvite", tcl_isinvite}
  ,
  {"isperminvite", tcl_isperminvite}
  ,
  {"matchinvite", tcl_matchinvite}
  ,
  {"newchaninvite", tcl_newchaninvite}
  ,
  {"newinvite", tcl_newinvite}
  ,
  {"channel", tcl_channel}
  ,
  {"channels", tcl_channels}
  ,
  {"exemptlist", tcl_exemptlist}
  ,
  {"invitelist", tcl_invitelist}
  ,
  {"banlist", tcl_banlist}
  ,
#ifdef HUB
  {"savechannels", tcl_savechannels}
  ,
  {"loadchannels", tcl_loadchannels}
  ,
#endif
  {"validchan", tcl_validchan}
  ,
  {"getchaninfo", tcl_getchaninfo}
  ,
  {"setchaninfo", tcl_setchaninfo}
  ,
  {"setlaston", tcl_setlaston}
  ,
  {"addchanrec", tcl_addchanrec}
  ,
  {"delchanrec", tcl_delchanrec}
  ,
  {"stick", tcl_stick}
  ,
  {"unstick", tcl_stick}
  ,
  {"stickinvite", tcl_stickinvite}
  ,
  {"unstickinvite", tcl_stickinvite}
  ,
  {"stickexempt", tcl_stickexempt}
  ,
  {"unstickexempt", tcl_stickexempt}
  ,
  {0, 0}
};
#endif

/* 
 * userchan.c -- part of channels.mod
 * 
 * $Id: userchan.c,v 1.29 2000/01/08 21:23:15 per Exp $
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

struct chanuserrec *get_chanrec(struct userrec *u, char *chname)
{
  struct chanuserrec *ch = u->chanrec;

  while (ch != NULL) {
    if (!rfc_casecmp(ch->channel, chname))
      return ch;
    ch = ch->next;
  }
  return NULL;
}

struct chanuserrec *add_chanrec(struct userrec *u, char *chname)
{
  struct chanuserrec *ch = 0;

  if (findchan(chname)) {
    ch = user_malloc(sizeof(struct chanuserrec));

    ch->next = u->chanrec;
    u->chanrec = ch;
    ch->info = NULL;
    ch->flags = 0;
    ch->flags_udef = 0;
    ch->laston = 0;
    strncpy0(ch->channel, chname, sizeof(ch->channel));
    if (!noshare && !(u->flags & USER_UNSHARED))
      shareout(findchan(chname), STR("+cr %s %s\n"), u->handle, chname);
  }
  return ch;
}

void add_chanrec_by_handle(struct userrec *bu, char *hand, char *chname)
{
  struct userrec *u;

  u = get_user_by_handle(bu, hand);
  if (!u)
    return;
  if (!get_chanrec(u, chname))
    add_chanrec(u, chname);
}

void get_handle_chaninfo(char *handle, char *chname, char *s)
{
  struct userrec *u;
  struct chanuserrec *ch;

  u = get_user_by_handle(userlist, handle);
  if (u == NULL) {
    s[0] = 0;
    return;
  }
  ch = get_chanrec(u, chname);
  if (ch == NULL) {
    s[0] = 0;
    return;
  }
  if (ch->info == NULL) {
    s[0] = 0;
    return;
  }
  strcpy(s, ch->info);
  return;
}

void set_handle_chaninfo(struct userrec *bu, char *handle, char *chname, char *info)
{
  struct userrec *u;
  struct chanuserrec *ch;
  struct chanset_t *cst;

  u = get_user_by_handle(bu, handle);
  if (!u)
    return;
  ch = get_chanrec(u, chname);
  if (!ch) {
    add_chanrec_by_handle(bu, handle, chname);
    ch = get_chanrec(u, chname);
  }
  if (info) {
    if (strlen(info) > 80)
      info[80] = 0;
  }
  if (ch->info != NULL)
    nfree(ch->info);
  if (info && info[0]) {
    ch->info = (char *) user_malloc(strlen(info) + 1);
    strcpy(ch->info, info);
  } else
    ch->info = NULL;
  cst = findchan(chname);
  if ((!noshare) && (bu == userlist) && !(u->flags & (USER_UNSHARED | USER_BOT)) && share_greet) {
    shareout(cst, STR("chchinfo %s %s %s\n"), handle, chname, info ? info : "");
  }
}

void del_chanrec(struct userrec *u, char *chname)
{
  struct chanuserrec *ch,
   *lst;

  lst = NULL;
  ch = u->chanrec;
  while (ch) {
    if (!rfc_casecmp(chname, ch->channel)) {
      if (lst == NULL)
	u->chanrec = ch->next;
      else
	lst->next = ch->next;
      if (ch->info != NULL)
	nfree(ch->info);
      nfree(ch);
      if (!noshare && !(u->flags & USER_UNSHARED))
	shareout(findchan(chname), STR("-cr %s %s\n"), u->handle, chname);
      return;
    }
    lst = ch;
    ch = ch->next;
  }
}

void set_handle_laston(char *chan, struct userrec *u, time_t n)
{
  struct chanuserrec *ch;

  if (!u)
    return;
  touch_laston(u, chan, n);
  ch = get_chanrec(u, chan);
  if (!ch)
    return;
  ch->laston = n;
}

/*        Merge of u_equals_ban(), u_equals_exempt() and u_equals_invite() to
 *      cut down on the duplication in the eggdrop code currently. <cybah>
 * 
 *      Returns:
 *              0       not a ban
 *              1       temporary ban
 *              2       perm ban
 */
int u_equals_mask(struct maskrec *u, char *mask)
{
  while (u) {
    if (!rfc_casecmp(u->mask, mask)) {
      if (u->flags & MASKREC_PERM)
	return 2;
      else
	return 1;
    }

    u = u->next;
  }

  return 0;
}

int u_match_mask(struct maskrec *rec, char *mask)
{
  while (rec) {
    if (wild_match(rec->mask, mask))
      return 1;

    rec = rec->next;
  }

  return 0;
}

int u_delban(struct chanset_t *c, char *who, int doit)
{
  int j,
    i = 0;
  struct maskrec *t;
  struct maskrec **u = (c) ? &c->bans : &global_bans;

  if (!strchr(who, '!') && (j = atoi(who))) {
    j--;
    for (; (*u) && j; u = &((*u)->next), j--);
    if (*u) {
      strcpy(who, (*u)->mask);
      i = 1;
    } else
      return -j - 1;
  } else {
    /* find matching host, if there is one */
    for (; *u && !i; u = &((*u)->next))
      if (!rfc_casecmp((*u)->mask, who)) {
	i = 1;
	break;
      }
    if (!*u)
      return 0;
  }
  if (i && doit) {
    if (!noshare) {
      /* distribute chan bans differently */
      if (c)
	shareout(c, STR("-bc %s %s\n"), c->name, who);
      else
	shareout(NULL, STR("-b %s\n"), who);
    }
    if (!c)
      gban_total--;
    nfree((*u)->mask);
    if ((*u)->desc)
      nfree((*u)->desc);
    if ((*u)->user)
      nfree((*u)->user);
    t = *u;
    *u = (*u)->next;
    nfree(t);
  }
  return i;
}

int u_delexempt(struct chanset_t *c, char *who, int doit)
{
  int j,
    i = 0;
  struct maskrec *t;
  struct maskrec **u = c ? &(c->exempts) : &global_exempts;

  if (!strchr(who, '!') && (j = atoi(who))) {
    j--;
    for (; (*u) && j; u = &((*u)->next), j--);
    if (*u) {
      strcpy(who, (*u)->mask);
      i = 1;
    } else
      return -j - 1;
  } else {
    /* find matching host, if there is one */
    for (; *u && !i; u = &((*u)->next))
      if (!rfc_casecmp((*u)->mask, who)) {
	i = 1;
	break;
      }
    if (!*u)
      return 0;
  }
  if (i && doit) {
    if (!noshare) {
      /* distribute chan exempts differently */
      if (c)
	shareout(c, STR("-ec %s %s\n"), c->name, who);
      else
	shareout(NULL, STR("-e %s\n"), who);
    }
    if (!c)
      gexempt_total--;
    nfree((*u)->mask);
    if ((*u)->desc)
      nfree((*u)->desc);
    if ((*u)->user)
      nfree((*u)->user);
    t = *u;
    *u = (*u)->next;
    nfree(t);
  }
  return i;
}

int u_delinvite(struct chanset_t *c, char *who, int doit)
{
  int j,
    i = 0;
  struct maskrec *t;
  struct maskrec **u = c ? &(c->invites) : &global_invites;

  if (!strchr(who, '!') && (j = atoi(who))) {
    j--;
    for (; (*u) && j; u = &((*u)->next), j--);
    if (*u) {
      strcpy(who, (*u)->mask);
      i = 1;
    } else
      return -j - 1;
  } else {
    /* find matching host, if there is one */
    for (; *u && !i; u = &((*u)->next))
      if (!rfc_casecmp((*u)->mask, who)) {
	i = 1;
	break;
      }
    if (!*u)
      return 0;
  }
  if (i && doit) {
    if (!noshare) {
      /* distribute chan invites differently */
      if (c)
	shareout(c, STR("-invc %s %s\n"), c->name, who);
      else
	shareout(NULL, STR("-inv %s\n"), who);
    }
    if (!c)
      ginvite_total--;
    nfree((*u)->mask);
    if ((*u)->desc)
      nfree((*u)->desc);
    if ((*u)->user)
      nfree((*u)->user);
    t = *u;
    *u = (*u)->next;
    nfree(t);
  }
  return i;
}

/* new method of creating bans */

/* if first char of note is '*' it's a sticky ban */
int u_addban(struct chanset_t *chan, char *ban, char *from, char *note, time_t expire_time, int flags)
{
  struct maskrec *p;
  struct maskrec **u = chan ? &chan->bans : &global_bans;
  char host[1024],
    s[1024];

  strcpy(host, ban);
  /* choke check: fix broken bans (must have '!' and '@') */
  if ((strchr(host, '!') == NULL) && (strchr(host, '@') == NULL))
    strcat(host, STR("!*@*"));
  else if (strchr(host, '@') == NULL)
    strcat(host, "@*");
  else if (strchr(host, '!') == NULL) {
    char *i = strchr(host, '@');

    strcpy(s, i);
    *i = 0;
    strcat(host, "!*");
    strcat(host, s);
  }
#ifdef LEAF
  simple_sprintf(s, STR("%s!%s"), botname, botuserhost);
#else
  simple_sprintf(s, STR("%s!%s@%s"), origbotname, botuser, hostname);
#endif
  if (wild_match(host, s)) {
    log(LCAT_ERROR, STR("Wanted to ban myself--deflected."));
    return 0;
  }
  if (u_equals_mask(*u, host))
    u_delban(chan, host, 1);	/* remove old ban */
  if ((expire_time == 0L) || (flags & MASKREC_PERM)) {
    flags |= MASKREC_PERM;
    expire_time = 0L;
  }
  /* new format: */
  p = user_malloc(sizeof(struct maskrec));

  p->next = *u;
  *u = p;
  p->expire = expire_time;
  p->added = now;
  p->lastactive = 0;
  p->flags = flags;
  p->mask = user_malloc(strlen(host) + 1);
  strcpy(p->mask, host);
  p->user = user_malloc(strlen(from) + 1);
  strcpy(p->user, from);
  p->desc = user_malloc(strlen(note) + 1);
  strcpy(p->desc, note);
  if (!noshare) {
    if (!chan)
      shareout(NULL, STR("+b %s %lu %s %s %s\n"), host, expire_time - now, (flags & MASKREC_PERM) ? "p" : "-", from, note);
    else
      shareout(chan, STR("+bc %s %lu %s %s %s %s\n"), host, expire_time - now, chan->name, (flags & MASKREC_PERM) ? "p" : "-", from, note);
  }
  return 1;
}

/* new method of creating invites - Hell invites are new!! - Jason */

/* if first char of note is '*' it's a sticky invite */
int u_addinvite(struct chanset_t *chan, char *invite, char *from, char *note, time_t expire_time, int flags)
{
  struct maskrec *p;
  struct maskrec **u = chan ? &chan->invites : &global_invites;
  char host[1024],
    s[1024];

  strcpy(host, invite);
  /* choke check: fix broken invites (must have '!' and '@') */
  if ((strchr(host, '!') == NULL) && (strchr(host, '@') == NULL))
    strcat(host, STR("!*@*"));
  else if (strchr(host, '@') == NULL)
    strcat(host, "@*");
  else if (strchr(host, '!') == NULL) {
    char *i = strchr(host, '@');

    strcpy(s, i);
    *i = 0;
    strcat(host, "!*");
    strcat(host, s);
  }
#ifdef LEAF
  simple_sprintf(s, STR("%s!%s"), botname, botuserhost);
#else
  simple_sprintf(s, STR("%s!%s@%s"), origbotname, botuser, hostname);
#endif
  if (u_equals_mask(*u, host))
    u_delinvite(chan, host, 1);	/* remove old invite */
  /* it shouldn't expire and be sticky also */
  if ((expire_time == 0L) || (flags & MASKREC_PERM)) {
    flags |= MASKREC_PERM;
    expire_time = 0L;
  }
  /* new format: */
  p = user_malloc(sizeof(struct maskrec));

  p->next = *u;
  *u = p;
  p->expire = expire_time;
  p->added = now;
  p->lastactive = 0;
  p->flags = flags;
  p->mask = user_malloc(strlen(host) + 1);
  strcpy(p->mask, host);
  p->user = user_malloc(strlen(from) + 1);
  strcpy(p->user, from);
  p->desc = user_malloc(strlen(note) + 1);
  strcpy(p->desc, note);
  if (!noshare) {
    if (!chan)
      shareout(NULL, STR("+inv %s %lu %s %s %s\n"), host, expire_time - now, (flags & MASKREC_PERM) ? "p" : "-", from, note);
    else
      shareout(chan, STR("+invc %s %lu %s %s %s %s\n"), host, expire_time - now, chan->name, (flags & MASKREC_PERM) ? "p" : "-", from, note);
  }
  return 1;
}

/* new method of creating exempts- new... yeah whole bleeding lot is new! */

/* if first char of note is '*' it's a sticky exempt */
int u_addexempt(struct chanset_t *chan, char *exempt, char *from, char *note, time_t expire_time, int flags)
{
  struct maskrec *p;
  struct maskrec **u = chan ? &chan->exempts : &global_exempts;
  char host[1024],
    s[1024];

  strcpy(host, exempt);
  /* choke check: fix broken exempts (must have '!' and '@') */
  if ((strchr(host, '!') == NULL) && (strchr(host, '@') == NULL))
    strcat(host, STR("!*@*"));
  else if (strchr(host, '@') == NULL)
    strcat(host, "@*");
  else if (strchr(host, '!') == NULL) {
    char *i = strchr(host, '@');

    strcpy(s, i);
    *i = 0;
    strcat(host, "!*");
    strcat(host, s);
  }
#ifdef LEAF
  simple_sprintf(s, STR("%s!%s"), botname, botuserhost);
#else
  simple_sprintf(s, STR("%s!%s@%s"), origbotname, botuser, hostname);
#endif
  if (u_equals_mask(*u, host))
    u_delexempt(chan, host, 1);	/* remove old exempt */
  if ((expire_time == 0L) || (flags & MASKREC_PERM)) {
    flags |= MASKREC_PERM;
    expire_time = 0L;
  }
  /* new format: */
  p = user_malloc(sizeof(struct maskrec));

  p->next = *u;
  *u = p;
  p->expire = expire_time;
  p->added = now;
  p->lastactive = 0;
  p->flags = flags;
  p->mask = user_malloc(strlen(host) + 1);
  strcpy(p->mask, host);
  p->user = user_malloc(strlen(from) + 1);
  strcpy(p->user, from);
  p->desc = user_malloc(strlen(note) + 1);
  strcpy(p->desc, note);
  if (!noshare) {
    if (!chan)
      shareout(NULL, STR("+e %s %lu %s %s %s\n"), host, expire_time - now, (flags & MASKREC_PERM) ? "p" : "-", from, note);
    else
      shareout(chan, STR("+ec %s %lu %s %s %s %s\n"), host, expire_time - now, chan->name, (flags & MASKREC_PERM) ? "p" : "-", from, note);
  }
  return 1;
}

/* take host entry from ban list and display it ban-style */
void display_ban(int idx, int number, struct maskrec *ban, struct chanset_t *chan, int show_inact)
{
  char dates[81],
    s[41];

  if (ban->added) {
    daysago(now, ban->added, s);
    sprintf(dates, STR("Created %s"), s);
    if (ban->added < ban->lastactive) {
      strcat(dates, STR(", last used "));
      daysago(now, ban->lastactive, s);
      strcat(dates, s);
    }
  } else
    dates[0] = 0;
  if (ban->flags & MASKREC_PERM)
    strcpy(s, STR("(perm)"));
  else {
    char s1[41];

    days(ban->expire, now, s1);
    sprintf(s, STR("(expires %s)"), s1);
  }
  if (!chan || ischanban(chan, ban->mask)) {
    if (number >= 0) {
      dprintf(idx, STR("  [%3d] %s %s\n"), number, ban->mask, s);
    } else {
      dprintf(idx, STR("BAN: %s %s\n"), ban->mask, s);
    }
  } else if (show_inact) {
    if (number >= 0) {
      dprintf(idx, STR("! [%3d] %s %s\n"), number, ban->mask, s);
    } else {
      dprintf(idx, STR("BAN (inactive): %s %s\n"), ban->mask, s);
    }
  } else
    return;
  dprintf(idx, STR("        %s: %s\n"), ban->user, ban->desc);
  if (dates[0])
    dprintf(idx, STR("        %s\n"), dates);
}

/* take host entry from exempt list and display it ban-style */
void display_exempt(int idx, int number, struct maskrec *exempt, struct chanset_t *chan, int show_inact)
{
  char dates[81],
    s[41];

  if (exempt->added) {
    daysago(now, exempt->added, s);
    sprintf(dates, STR("Created %s"), s);
    if (exempt->added < exempt->lastactive) {
      strcat(dates, STR(", last used "));
      daysago(now, exempt->lastactive, s);
      strcat(dates, s);
    }
  } else
    dates[0] = 0;
  if (exempt->flags & MASKREC_PERM)
    strcpy(s, STR("(perm)"));
  else {
    char s1[41];

    days(exempt->expire, now, s1);
    sprintf(s, STR("(expires %s)"), s1);
  }
  if (!chan || ischanexempt(chan, exempt->mask)) {
    if (number >= 0) {
      dprintf(idx, STR("  [%3d] %s %s\n"), number, exempt->mask, s);
    } else {
      dprintf(idx, STR("EXEMPT: %s %s\n"), exempt->mask, s);
    }
  } else if (show_inact) {
    if (number >= 0) {
      dprintf(idx, STR("! [%3d] %s %s\n"), number, exempt->mask, s);
    } else {
      dprintf(idx, STR("EXEMPT (inactive): %s %s\n"), exempt->mask, s);
    }
  } else
    return;
  dprintf(idx, STR("        %s: %s\n"), exempt->user, exempt->desc);
  if (dates[0])
    dprintf(idx, STR("        %s\n"), dates);
}

/* take host entry from invite list and display it ban-style */
void display_invite(int idx, int number, struct maskrec *invite, struct chanset_t *chan, int show_inact)
{
  char dates[81],
    s[41];

  if (invite->added) {
    daysago(now, invite->added, s);
    sprintf(dates, STR("Created %s"), s);
    if (invite->added < invite->lastactive) {
      strcat(dates, STR(", last used "));
      daysago(now, invite->lastactive, s);
      strcat(dates, s);
    }
  } else
    dates[0] = 0;
  if (invite->flags & MASKREC_PERM)
    strcpy(s, STR("(perm)"));
  else {
    char s1[41];

    days(invite->expire, now, s1);
    sprintf(s, STR("(expires %s)"), s1);
  }
  if (!chan || ischaninvite(chan, invite->mask)) {
    if (number >= 0) {
      dprintf(idx, STR("  [%3d] %s %s\n"), number, invite->mask, s);
    } else {
      dprintf(idx, STR("INVITE: %s %s\n"), invite->mask, s);
    }
  } else if (show_inact) {
    if (number >= 0) {
      dprintf(idx, STR("! [%3d] %s %s\n"), number, invite->mask, s);
    } else {
      dprintf(idx, STR("INVITE (inactive): %s %s\n"), invite->mask, s);
    }
  } else
    return;
  dprintf(idx, STR("        %s: %s\n"), invite->user, invite->desc);
  if (dates[0])
    dprintf(idx, STR("        %s\n"), dates);
}

void tell_bans(int idx, int show_inact, char *match)
{
  int k = 1;
  char *chname;
  struct chanset_t *chan = NULL;
  struct maskrec *u;

  /* was channel given? */
  Context;
  if (match[0]) {
    chname = newsplit(&match);
    if (chname[0] && (strchr(CHANMETA, chname[0]) != NULL)) {
      chan = findchan(chname);
      if (!chan) {
	dprintf(idx, STR("No such channel.\n"));
	return;
      }
    } else
      match = chname;
  }
  if (!chan && !(chan = chanset))
    return;
  if (show_inact)
    dprintf(idx, STR("Global bans:   (! = not active on %s)\n"), chan->name);
  else
    dprintf(idx, STR("Global bans:\n"));
  Context;
  u = global_bans;
  for (; u; u = u->next) {
    if (match[0]) {
      if ((wild_match(match, u->mask)) || (wild_match(match, u->desc)) || (wild_match(match, u->user)))
	display_ban(idx, k, u, chan, 1);
      k++;
    } else
      display_ban(idx, k++, u, chan, show_inact);
  }
  if (show_inact)
    dprintf(idx, STR("Channel bans for %s:   (! = not active, * = not placed by bot)\n"), chan->name);
  else
    dprintf(idx, STR("Channel bans for %s:  (* = not placed by bot)\n"), chan->name);
  u = chan->bans;
  for (; u; u = u->next) {
    if (match[0]) {
      if ((wild_match(match, u->mask)) || (wild_match(match, u->desc)) || (wild_match(match, u->user)))
	display_ban(idx, k, u, chan, 1);
      k++;
    } else
      display_ban(idx, k++, u, chan, show_inact);
  }
  if (chan->status & CHAN_ACTIVE) {
    struct maskstruct *b = chan->channel.ban;
    char s[UHOSTLEN],
     *s1,
     *s2,
      fill[256];
    int min,
      sec;

    while (b->mask[0]) {
      if ((!u_equals_mask(global_bans, b->mask)) && (!u_equals_mask(chan->bans, b->mask))) {
	strcpy(s, b->who);
	s2 = s;
	s1 = splitnick(&s2);
	if (s1[0])
	  sprintf(fill, STR("%s (%s!%s)"), b->mask, s1, s2);
	else if (!strcasecmp(s, STR("existant")))
	  sprintf(fill, STR("%s (%s)"), b->mask, s2);
	else
	  sprintf(fill, STR("%s (server %s)"), b->mask, s2);
	if (b->timer != 0) {
	  min = (now - b->timer) / 60;
	  sec = (now - b->timer) - (min * 60);
	  sprintf(s, STR(" (active %02d:%02d)"), min, sec);
	  strcat(fill, s);
	}
	if ((!match[0]) || (wild_match(match, b->mask)))
	  dprintf(idx, STR("* [%3d] %s\n"), k, fill);
	k++;
      }
      b = b->next;
    }
  }
  if (k == 1)
    dprintf(idx, STR("(There are no bans, permanent or otherwise.)\n"));
  if ((!show_inact) && (!match[0]))
    dprintf(idx, STR("Use '.bans all' to see the total list.\n"));
}

void tell_exempts(int idx, int show_inact, char *match)
{
  int k = 1;
  char *chname;
  struct chanset_t *chan = NULL;
  struct maskrec *u;

  /* was channel given? */
  Context;
  if (match[0]) {
    chname = newsplit(&match);
    if (chname[0] && strchr(CHANMETA, chname[0])) {
      chan = findchan(chname);
      if (!chan) {
	dprintf(idx, STR("No such channel.\n"));
	return;
      }
    } else
      match = chname;
  }
  if (!chan && !(chan = chanset))
    return;
  if (show_inact)
    dprintf(idx, STR("Global exempts:   (! = not active on %s)\n"), chan->name);
  else
    dprintf(idx, STR("Global exempts:\n"));
  Context;
  u = global_exempts;
  for (; u; u = u->next) {
    if (match[0]) {
      if ((wild_match(match, u->mask)) || (wild_match(match, u->desc)) || (wild_match(match, u->user)))
	display_exempt(idx, k, u, chan, 1);
      k++;
    } else
      display_exempt(idx, k++, u, chan, show_inact);
  }
  if (show_inact)
    dprintf(idx, STR("Channel exempts for %s:   (! = not active, * = not placed by bot)\n"), chan->name);
  else
    dprintf(idx, STR("Channel exempts for %s:  (* = not placed by bot)\n"), chan->name);
  u = chan->exempts;
  for (; u; u = u->next) {
    if (match[0]) {
      if ((wild_match(match, u->mask)) || (wild_match(match, u->desc)) || (wild_match(match, u->user)))
	display_exempt(idx, k, u, chan, 1);
      k++;
    } else
      display_exempt(idx, k++, u, chan, show_inact);
  }
  if (chan->status & CHAN_ACTIVE) {
    struct maskstruct *e = chan->channel.exempt;
    char s[UHOSTLEN],
     *s1,
     *s2,
      fill[256];
    int min,
      sec;

    while (e->mask[0]) {
      if ((!u_equals_mask(global_exempts, e->mask)) && (!u_equals_mask(chan->exempts, e->mask))) {
	strcpy(s, e->who);
	s2 = s;
	s1 = splitnick(&s2);
	if (s1[0])
	  sprintf(fill, STR("%s (%s!%s)"), e->mask, s1, s2);
	else if (!strcasecmp(s, STR("existant")))
	  sprintf(fill, STR("%s (%s)"), e->mask, s2);
	else
	  sprintf(fill, STR("%s (server %s)"), e->mask, s2);
	if (e->timer != 0) {
	  min = (now - e->timer) / 60;
	  sec = (now - e->timer) - (min * 60);
	  sprintf(s, STR(" (active %02d:%02d)"), min, sec);
	  strcat(fill, s);
	}
	if ((!match[0]) || (wild_match(match, e->mask)))
	  dprintf(idx, STR("* [%3d] %s\n"), k, fill);
	k++;
      }
      e = e->next;
    }
  }
  if (k == 1)
    dprintf(idx, STR("(There are no ban exempts, permanent or otherwise.)\n"));
  if ((!show_inact) && (!match[0]))
    dprintf(idx, STR("Use '.exempts all' to see the total list.\n"));
}

void tell_invites(int idx, int show_inact, char *match)
{
  int k = 1;
  char *chname;
  struct chanset_t *chan = NULL;
  struct maskrec *u;

  /* was channel given? */
  Context;
  if (match[0]) {
    chname = newsplit(&match);
    if (chname[0] && strchr(CHANMETA, chname[0])) {
      chan = findchan(chname);
      if (!chan) {
	dprintf(idx, STR("No such channel.\n"));
	return;
      }
    } else
      match = chname;
  }
  if (!chan && !(chan = chanset))
    return;
  if (show_inact)
    dprintf(idx, STR("Global invites:   (! = not active on %s)\n"), chan->name);
  else
    dprintf(idx, STR("Global invites:\n"));
  Context;
  u = global_invites;
  for (; u; u = u->next) {
    if (match[0]) {
      if ((wild_match(match, u->mask)) || (wild_match(match, u->desc)) || (wild_match(match, u->user)))
	display_invite(idx, k, u, chan, 1);
      k++;
    } else
      display_invite(idx, k++, u, chan, show_inact);
  }
  if (show_inact)
    dprintf(idx, STR("Invites for channel %s:   (! = not active, * = not placed by bot)\n"), chan->name);
  else
    dprintf(idx, STR("Invites for channel %s:  (* = not placed by bot)\n"), chan->name);
  u = chan->invites;
  for (; u; u = u->next) {
    if (match[0]) {
      if ((wild_match(match, u->mask)) || (wild_match(match, u->desc)) || (wild_match(match, u->user)))
	display_invite(idx, k, u, chan, 1);
      k++;
    } else
      display_invite(idx, k++, u, chan, show_inact);
  }
  if (chan->status & CHAN_ACTIVE) {
    struct maskstruct *i = chan->channel.invite;
    char s[UHOSTLEN],
     *s1,
     *s2,
      fill[256];
    int min,
      sec;

    while (i->mask[0]) {
      if ((!u_equals_mask(global_invites, i->mask)) && (!u_equals_mask(chan->invites, i->mask))) {
	strcpy(s, i->who);
	s2 = s;
	s1 = splitnick(&s2);
	if (s1[0])
	  sprintf(fill, STR("%s (%s!%s)"), i->mask, s1, s2);
	else if (!strcasecmp(s, STR("existant")))
	  sprintf(fill, STR("%s (%s)"), i->mask, s2);
	else
	  sprintf(fill, STR("%s (server %s)"), i->mask, s2);
	if (i->timer != 0) {
	  min = (now - i->timer) / 60;
	  sec = (now - i->timer) - (min * 60);
	  sprintf(s, STR(" (active %02d:%02d)"), min, sec);
	  strcat(fill, s);
	}
	if ((!match[0]) || (wild_match(match, i->mask)))
	  dprintf(idx, STR("* [%3d] %s\n"), k, fill);
	k++;
      }
      i = i->next;
    }
  }
  if (k == 1)
    dprintf(idx, STR("(There are no invites, permanent or otherwise.)\n"));
  if ((!show_inact) && (!match[0]))
    dprintf(idx, STR("Use '.invites all' to see the total list.\n"));
}

#ifdef HUB
int write_config(stream s, char * key, int idx) 
{
  struct logcategory *lc;
  int i;
#ifdef G_DCCPASS
  struct cmd_pass *cp;
#endif
  int ok=1;
  enc_stream_printf(s, key, STR("*Config configuration values\n"));
  for (i=0;i<cfg_count;i++)
    if ((cfg[i]->flags & CFGF_GLOBAL) && (cfg[i]->gdata)) {
	enc_stream_printf(s, key, STR("@ %s %s"), cfg[i]->name, 
				cfg[i]->gdata ? cfg[i]->gdata : "");
    }
#ifdef G_DCCPASS
  for (cp = cmdpass; cp; cp = cp->next)
    enc_stream_printf(s, key, STR("- %s %s"), cp->name, cp->pass);
#endif
  for (lc = logcat; lc; lc = lc->next)
    enc_stream_printf(s, key, STR("%% %s %i %i %i %lu"), lc->name, lc->logtochan, lc->logtofile, lc->broadcast, lc->flags);
  return ok;
}


/* channel's local banlist to a file */
int write_bans(stream s, char *key, int idx)
{
  struct chanset_t *chan;
  struct maskrec *b;
  struct igrec *i;

  if (global_ign)
    enc_stream_printf(s, key, STR("*ignore - -\n"));
  for (i = global_ign; i; i = i->next)
    enc_stream_printf(s, key, STR("- %s:%s%lu:%s:%lu:%s\n"), i->igmask, (i->flags & IGREC_PERM) ? "+" : "", i->expire, i->user ? i->user : botnetnick, i->added, i->msg ? i->msg : "");
  if (global_bans)
    enc_stream_printf(s, key, STR("*ban - -\n"));
  for (b = global_bans; b; b = b->next)
    enc_stream_printf(s, key, STR("- %s:%s%lu:+%lu:%lu:%s:%s\n"), b->mask,
		    (b->flags & MASKREC_PERM) ? "+" : "", b->expire,
		      b->added, b->lastactive, b->user ? b->user : botnetnick, b->desc ? b->desc : STR("requested"));
  for (chan = chanset; chan; chan = chan->next) {
    struct flag_record fr = { FR_CHAN | FR_GLOBAL, 0, 0, 0, 0 };

    if (idx >= 0)
      get_user_flagrec(dcc[idx].user, &fr, chan->name);
    enc_stream_printf(s, key, STR("::%s bans\n"), chan->name);
    for (b = chan->bans; b; b = b->next)
      enc_stream_printf(s, key, STR("- %s:%s%lu:+%lu:%lu:%s:%s\n"), b->mask,
		      (b->flags & MASKREC_PERM) ? "+" : "", b->expire,
			b->added, b->lastactive, b->user ? b->user : botnetnick, b->desc ? b->desc : STR("requested"));
  }
  return 1;
}

/* write channel's local exemptlist to a file */
int write_exempts(stream s, char *key, int idx)
{
  struct chanset_t *chan;
  struct maskrec *e;

  if (global_exempts)
    enc_stream_printf(s, key, STR("*exempt - -\n"));
  for (e = global_exempts; e; e = e->next)
    enc_stream_printf (s, key, STR("%s %s:%s%lu:+%lu:%lu:%s:%s\n"), "%", e->mask,
		       (e->flags & MASKREC_PERM) ? "+" : "", e->expire, e->added, e->lastactive, 
		       e->user ? e->user : botnetnick, e->desc ? e->desc : STR("requested"));
  for (chan = chanset; chan; chan = chan->next) {
    struct flag_record fr = { FR_CHAN | FR_GLOBAL, 0, 0, 0, 0 };

    if (idx >= 0)
      get_user_flagrec(dcc[idx].user, &fr, chan->name);
    enc_stream_printf(s, key, STR("&&%s exempts\n"), chan->name);
    for (e = chan->exempts; e; e = e->next)
      enc_stream_printf(s, key, STR("%s %s:%s%lu:+%lu:%lu:%s:%s\n"), "%", e->mask,
			(e->flags & MASKREC_PERM) ? "+" : "", e->expire,
			e->added, e->lastactive, e->user ? e->user : botnetnick, 
			e->desc ? e->desc : STR("requested"));
  }
  return 1;
}

/* write channel's local invitelist to a file */
int write_invites(stream s, char *key, int idx)
{
  struct chanset_t *chan;
  struct maskrec *ir;

  if (global_invites)
    enc_stream_printf(s, key, STR("*Invite - -\n"));
  for (ir = global_invites; ir; ir = ir->next)
    enc_stream_printf(s, key, STR("@ %s:%s%lu:+%lu:%lu:%s:%s\n"), ir->mask,
		    (ir->flags & MASKREC_PERM) ? "+" : "", ir->expire,
		    ir->added, ir->lastactive, ir->user ? ir->user : botnetnick, 
		      ir->desc ? ir->desc : STR("requested"));
  for (chan = chanset; chan; chan = chan->next) {
    struct flag_record fr = { FR_CHAN | FR_GLOBAL, 0, 0, 0, 0 };

    if (idx >= 0)
      get_user_flagrec(dcc[idx].user, &fr, chan->name);
    enc_stream_printf(s, key, STR("$$%s invites\n"), chan->name);
    for (ir = chan->invites; ir; ir = ir->next)
      enc_stream_printf(s, key, STR("@ %s:%s%lu:+%lu:%lu:%s:%s\n"), ir->mask,
			(ir->flags & MASKREC_PERM) ? "+" : "", ir->expire,
			ir->added, ir->lastactive, ir->user ? ir->user : botnetnick, 
			ir->desc ? ir->desc : STR("requested"));
  }
  return 1;
}

/*
void channels_writeuserfile()
{
  FILE *f;

  f = fopen(".cn", "a");
  if (f) {
    write_bans(f, localkey, -1);
    write_exempts(f, localkey, -1);
    write_invites(f, localkey, -1);
    write_config(f, localkey, -1);
    fclose(f);
  } else
    log(LCAT_ERROR, STR("Error writing user file"));
  write_channels(localkey);
}
*/
static int checklimit = 1;

#ifdef HUB
void check_enforced_topics() {
  int i=0;
  struct chanset_t * chan;
  struct userrec *u, *u2;
  char hub[20];
  u=get_user_by_handle(userlist, botnetnick);
  besthub(hub);
  if (hub[0]) {
    u2=get_user_by_handle(userlist, hub);
    if (u2) {
      char myval[20], hval[20];
      link_pref_val(u, myval);
      link_pref_val(u2, hval);
      if (strcmp(myval, hval)>0)
	return;
    }
  }
  for (chan=chanset;chan;chan=chan->next,i++) {
    if (strlen(chan->topic)) {
      botnet_send_enforcetopic(chan);
    }
  }
}
#endif

void check_limitraise() {
  int i=0;
  struct chanset_t * chan;
  struct userrec *u, *u2;
  char hub[20];
  u=get_user_by_handle(userlist, botnetnick);
  besthub(hub);
  if (hub[0]) {
    u2=get_user_by_handle(userlist, hub);
    if (u2) {
      char myval[20], hval[20];
      link_pref_val(u, myval);
      link_pref_val(u2, hval);
      if (strcmp(myval, hval)>0)
	return;
    }
  }
  for (chan=chanset;chan;chan=chan->next,i++) {
    if (i % 2 == checklimit) {
      if (chan->limitraise) {
	botnet_send_limitcheck(chan);
      }
    }
  }
  if (checklimit)
    checklimit=0;
  else
    checklimit=1;
}
#endif



/* check for expired timed-bans */
void check_expired_bans()
{
  struct maskrec **u;
  struct chanset_t *chan;

  u = &global_bans;
  while (*u) {
    if (!((*u)->flags & MASKREC_PERM) && (now >= (*u)->expire)) {
      log(LCAT_INFO, STR("No longer banning %s (expired)"), (*u)->mask);
#ifdef LEAF
      chan = chanset;
      while (chan != NULL) {
	add_mode(chan, '-', 'b', (*u)->mask);
	chan = chan->next;
      }
#endif
      u_delban(NULL, (*u)->mask, 1);
    } else
      u = &((*u)->next);
  }
  /* check for specific channel-domain bans expiring */
  for (chan = chanset; chan; chan = chan->next) {
    u = &chan->bans;
    while (*u) {
      if (!((*u)->flags & MASKREC_PERM) && (now >= (*u)->expire)) {
	log(LCAT_INFO, STR("No longer banning %s on %s (expired)"), (*u)->mask, chan->name);
#ifdef LEAF
	add_mode(chan, '-', 'b', (*u)->mask);
#endif
	u_delban(chan, (*u)->mask, 1);
      } else
	u = &((*u)->next);
    }
  }
}

/* check for expired timed-exemptions */
void check_expired_exempts()
{
  struct maskrec **u;
  struct chanset_t *chan;
  struct maskstruct *b;
  int match;

  if (!use_exempts)
    return;
  u = &global_exempts;
  while (*u) {
    if (!((*u)->flags & MASKREC_PERM) && (now >= (*u)->expire)) {
      log(LCAT_INFO, STR("No longer exempting %s (expired)"), (*u)->mask);
      chan = chanset;
      while (chan != NULL) {
	match = 0;
	b = chan->channel.ban;
	while (b->mask[0] && !match) {
	  if (wild_match(b->mask, (*u)->mask) || wild_match((*u)->mask, b->mask))
	    match = 1;
	  else
	    b = b->next;
	}
	if (match)
	  log(LCAT_INFO, STR("Exempt not expired on channel %s. Ban still set!"), chan->name);
	else {
#ifdef LEAF
	  add_mode(chan, '-', 'e', (*u)->mask);
#endif
	}
	chan = chan->next;
      }
      u_delexempt(NULL, (*u)->mask, 1);
    } else
      u = &((*u)->next);
  }
  /* check for specific channel-domain exempts expiring */
  for (chan = chanset; chan; chan = chan->next) {
    u = &chan->exempts;
    while (*u) {
      if (!((*u)->flags & MASKREC_PERM) && (now >= (*u)->expire)) {
	match = 0;
	b = chan->channel.ban;
	while (b->mask[0] && !match) {
	  if (wild_match(b->mask, (*u)->mask) || wild_match((*u)->mask, b->mask))
	    match = 1;
	  else
	    b = b->next;
	}
	if (match)
	  log(LCAT_INFO, STR("Exempt not expired on channel %s. Ban still set!"), chan->name);
	else {
	  log(LCAT_INFO, STR("No longer exempting %s on %s (expired)"), (*u)->mask, chan->name);
#ifdef LEAF
	  add_mode(chan, '-', 'e', (*u)->mask);
#endif
	  u_delexempt(chan, (*u)->mask, 1);
	}
      }
      u = &((*u)->next);
    }
  }
}

/* check for expired timed-invites */
void check_expired_invites()
{
  struct maskrec **u;
  struct chanset_t *chan = chanset;

  if (!use_invites)
    return;
  u = &global_invites;
  while (*u) {
    if (!((*u)->flags & MASKREC_PERM) && (now >= (*u)->expire)
	&& !(chan->channel.mode & CHANINV)) {
      log(LCAT_INFO, STR("No longer inviting %s (expired)"), (*u)->mask);
#ifdef LEAF
      chan = chanset;
      while (chan != NULL && !(chan->channel.mode & CHANINV)) {
	add_mode(chan, '-', 'I', (*u)->mask);
	chan = chan->next;
      }
#endif
      u_delinvite(NULL, (*u)->mask, 1);
    } else
      u = &((*u)->next);
  }
  /* check for specific channel-domain invites expiring */
  for (chan = chanset; chan; chan = chan->next) {
    u = &chan->invites;
    while (*u) {
      if (!((*u)->flags & MASKREC_PERM) && (now >= (*u)->expire)) {
	log(LCAT_INFO, STR("No longer inviting %s on %s (expired)"), (*u)->mask, chan->name);
#ifdef LEAF
	add_mode(chan, '-', 'I', (*u)->mask);
#endif
	u_delinvite(chan, (*u)->mask, 1);
      } else
	u = &((*u)->next);
    }
  }
}

void got_chanlist(char *botnick, char *code, char *par)
{

/*
  Start by deling all channels not listed in "par"
*/
  struct chanset_t *chan;
  char *chname,
   *nextch;
  char tmp[2048];
  int delit;

/* Remove channels i shouldn't be on */
  chan = chanset;
  while (chan) {
    strncpy0(tmp, par, sizeof(tmp));
    chname = tmp;
    delit = 1;
    while ((chname) && (chname[0]) && (delit)) {
      nextch = strchr(chname, ' ');
      if (nextch)
	*nextch++ = 0;
      if (!rfc_casecmp(chname, chan->name)) {
	delit = 0;
      } else {
	chname = nextch;
      }
    }

    if (delit) {
      if (ismember(chan, botname))
	dprintf(DP_SERVER, STR("PART %s\n"), chan->name);
      remove_channel(chan);
      chan = chanset;		/* nasty but works... not syncing *that* often neway :P */
    } else {
      chan = chan->next;
    }
  }

  strncpy0(tmp, par, sizeof(tmp));
  chname = tmp;
  while (chname && chname[0]) {
    nextch = strchr(chname, ' ');
    if (nextch)
      *nextch++ = 0;
    if (!findchan(chname)) {
      chan = (struct chanset_t *) nmalloc(sizeof(struct chanset_t));
      bzero(chan, sizeof(struct chanset_t));

      chan->limit_prot = (-1);
      chan->limit = (-1);
      chan->flood_pub_thr = gfld_chan_thr;
      chan->flood_pub_time = gfld_chan_time;
      chan->flood_ctcp_thr = gfld_ctcp_thr;
      chan->flood_ctcp_time = gfld_ctcp_time;
      chan->flood_join_thr = gfld_join_thr;
      chan->flood_join_time = gfld_join_time;
      chan->flood_deop_thr = gfld_deop_thr;
      chan->flood_deop_time = gfld_deop_time;
      chan->flood_kick_thr = gfld_kick_thr;
      chan->flood_kick_time = gfld_kick_time;
      strncpy0(chan->name, chname, sizeof(chan->name));
      chan->status = CHAN_DYNAMICBANS | CHAN_INACTIVE;
#ifdef G_MANUALOP
      chan->status |= CHAN_MANOP;
#endif
      chan->ircnet_status = CHAN_DYNAMICEXEMPTS | CHAN_DYNAMICINVITES;
      init_channel(chan);
      list_append((struct list_type **) &chanset, (struct list_type *) chan);
    }
    chname = nextch;
  }
  log(LCAT_BOT, STR("Got channel list from %s"), botnick);
}

void got_chansync(char *botnick, char *code, char *par)
{
  /* #chan {chanmode} idle-kick limitraise pub ctcp join kick deop status ircnet_status */
  char tmp[512],
   *w,
   *nw,
   *t,
    chname[128];
  int p;
  struct chanset_t *chan = NULL;

#ifdef LEAF
  int ostatus = 0;
#endif
  strncpy0(tmp, par, sizeof(tmp));
  p = 0;
  w = tmp;
  while (w) {
    nw = strchr(w, ' ');
    if (nw)
      *nw = 0;
    switch (p) {
    case 0:
      strcpy(chname, w);
      chan = findchan(chname);
      if (!chan) {
	chan = (struct chanset_t *) nmalloc(sizeof(struct chanset_t));
	bzero(chan, sizeof(struct chanset_t));

	chan->limit_prot = (-1);
	chan->limit = (-1);
	chan->flood_pub_thr = gfld_chan_thr;
	chan->flood_pub_time = gfld_chan_time;
	chan->flood_ctcp_thr = gfld_ctcp_thr;
	chan->flood_ctcp_time = gfld_ctcp_time;
	chan->flood_join_thr = gfld_join_thr;
	chan->flood_join_time = gfld_join_time;
	chan->flood_deop_thr = gfld_deop_thr;
	chan->flood_deop_time = gfld_deop_time;
	chan->flood_kick_thr = gfld_kick_thr;
	chan->flood_kick_time = gfld_kick_time;
	strncpy0(chan->name, chname, sizeof(chan->name));
	chan->status = CHAN_DYNAMICBANS | CHAN_INACTIVE;
#ifdef G_MANUALOP
	chan->status |= CHAN_MANOP;
#endif
	chan->ircnet_status = CHAN_DYNAMICEXEMPTS | CHAN_DYNAMICINVITES;
	init_channel(chan);
	list_append((struct list_type **) &chanset, (struct list_type *) chan);
      }
      break;
    case 1:			/* chanmode */
      *nw = ' ';
      nw = strchr(w, '}');
      *nw++ = 0;
      *w++ = 0;
      set_mode_protect(chan, w);
      break;
    case 2:
      chan->limitraise = atoi(w);
      break;
    case 3:			/* idle-kick */
      chan->idle_kick = atoi(w);
      break;
    case 4:			/* pub-flood */
      t = strchr(w, ':');
      if (t) {
	*t++ = 0;
	chan->flood_pub_thr = atoi(w);
	chan->flood_pub_time = atoi(t);
      }
      break;
    case 5:			/* ctcp-flood */
      t = strchr(w, ':');
      if (t) {
	*t++ = 0;
	chan->flood_ctcp_thr = atoi(w);
	chan->flood_ctcp_time = atoi(t);
      }
      break;
    case 6:			/* join-flood */
      t = strchr(w, ':');
      if (t) {
	*t++ = 0;
	chan->flood_join_thr = atoi(w);
	chan->flood_join_time = atoi(t);
      }
      break;
    case 7:			/* kick-flood */
      t = strchr(w, ':');
      if (t) {
	*t++ = 0;
	chan->flood_kick_thr = atoi(w);
	chan->flood_kick_time = atoi(t);
      }
      break;
    case 8:			/* deop-flood */
      t = strchr(w, ':');
      if (t) {
	*t++ = 0;
	chan->flood_deop_thr = atoi(w);
	chan->flood_deop_time = atoi(t);
      }
      break;
    case 9:			/* status */
#ifdef LEAF
      ostatus = chan->status & 0x00FFFFFF;
#endif
      chan->status = (atoi(w) & 0X00FFFFFF) | (chan->status & 0xFF000000);
      break;
    case 10:			/* ircnet_status */
      chan->ircnet_status = atoi(w);
      break;
    }
    p++;
    if (nw)
      nw++;
    w = nw;
  }
  log(LCAT_BOT, STR("Got channel sync for %s from %s"), chan->name, botnick);
#ifdef LEAF
  if ((ostatus ^ chan->status) & CHAN_BITCH) {
    recheck_channel(chan, 0);
  }
  if (shouldjoin(chan)) {
    if (!ismember(chan, botname))
      dprintf(DP_MODE, STR("JOIN %s %s\n"), chan->name, chan->key_prot);
  } else {
    if (ismember(chan, botname))
      dprintf(DP_MODE, STR("PART %s\n"), chan->name);
  }
#endif
}

void got_role(char *botnick, char *code, char *par)
{
  role = atoi(par);
  log(LCAT_DEBUG, STR("Got role index %i"), role);
}

int get_role(char *bot)
{
  int rl,
    i;
  struct bot_addr *ba;
  int r[5] = { 0, 0, 0, 0, 0 };
  struct userrec *u,
   *u2;

  u2 = get_user_by_handle(userlist, bot);
  if (!u2)
    return 1;

  for (u = userlist; u; u = u->next) {
    if (u->flags & USER_BOT) {
      if (strcmp(u->handle, bot)) {
	ba = get_user(&USERENTRY_BOTADDR, u);
	if ((nextbot(u->handle) >= 0) && (ba) && (ba->roleid > 0)
	    && (ba->roleid < 5))
	  r[(ba->roleid - 1)]++;
      }
    }
  }
  rl = 0;
  for (i = 1; i <= 4; i++)
    if (r[i] < r[rl])
      rl = i;
  rl++;
  ba = get_user(&USERENTRY_BOTADDR, u2);
  if (ba)
    ba->roleid = rl;
  return rl;
}

#ifdef HUB
void rebalance_roles()
{
  struct bot_addr *ba;
  int r[5] = { 0, 0, 0, 0, 0 }, hNdx, lNdx, i;
  struct userrec *u;
  char tmp[10];

  for (u = userlist; u; u = u->next) {
    if ((u->flags & USER_BOT) && (nextbot(u->handle) >= 0)) {
      ba = get_user(&USERENTRY_BOTADDR, u);
      if (ba && (ba->roleid > 0) && (ba->roleid < 5))
	r[(ba->roleid - 1)]++;
    }
  }
  /*
     Find high & low
     while (high-low) > 2
     move from highNdx to lowNdx
   */

  hNdx = 0;
  lNdx = 0;
  for (i = 1; i <= 4; i++) {
    if (r[i] < r[lNdx])
      lNdx = i;
    if (r[i] > r[hNdx])
      hNdx = i;
  }
  while (r[hNdx] - r[lNdx] >= 2) {
    for (u = userlist; u; u = u->next) {
      if ((u->flags & USER_BOT) && (nextbot(u->handle) >= 0)) {
	ba = get_user(&USERENTRY_BOTADDR, u);
	if (ba && (ba->roleid == (hNdx + 1))) {
	  ba->roleid = lNdx + 1;
	  sprintf(tmp, STR("rl %d"), lNdx + 1);
	  botnet_send_zapf(nextbot(u->handle), botnetnick, u->handle, tmp);
	}
      }
    }
    r[hNdx]--;
    r[lNdx]++;
    hNdx = 0;
    lNdx = 0;
    for (i = 1; i <= 4; i++) {
      if (r[i] < r[lNdx])
	lNdx = i;
      if (r[i] > r[hNdx])
	hNdx = i;
    }
  }
}
#endif

void send_chan_settings(char *bot, struct chanset_t *chan)
{
  char settings[2048],
    tmp[256];

  sprintf(settings, STR("chansync %s "), chan->name);
  get_mode_protect(chan, tmp);
  strcat(settings, "{");
  strcat(settings, tmp);
  strcat(settings, "} "); 
  sprintf(tmp, STR("%d "), chan->limitraise);
  strcat(settings, tmp);
  sprintf(tmp, STR("%d "), chan->idle_kick);
  strcat(settings, tmp);
  sprintf(tmp, STR("%d:%d "), chan->flood_pub_thr, chan->flood_pub_time);
  strcat(settings, tmp);
  sprintf(tmp, STR("%d:%d "), chan->flood_ctcp_thr, chan->flood_ctcp_time);
  strcat(settings, tmp);
  sprintf(tmp, STR("%d:%d "), chan->flood_join_thr, chan->flood_join_time);
  strcat(settings, tmp);
  sprintf(tmp, STR("%d:%d "), chan->flood_kick_thr, chan->flood_kick_time);
  strcat(settings, tmp);
  sprintf(tmp, STR("%d:%d "), chan->flood_deop_thr, chan->flood_deop_time);
  strcat(settings, tmp);
  sprintf(tmp, STR("%d "), chan->status);
  strcat(settings, tmp);
  sprintf(tmp, "%d", chan->ircnet_status);
  strcat(settings, tmp);
  if (bot[0] == '*')
    botnet_send_zapf_broad(-1, botnetnick, NULL, settings);
  else
    botnet_send_zapf(nextbot(bot), botnetnick, bot, settings);
}

void send_channel_sync(char *bot, struct chanset_t *chan)
{
  char chans[2048],
    tmp[512];
  int i;
  if (no_chansync) 
    return;
  if (chan) {
    send_chan_settings(bot, chan);
    log(LCAT_BOT, STR("Sent channel sync for %s to %s"), chan->name, bot);
  } else {
    chan = chanset;
    strcpy(chans, STR("chanlist "));
    while (chan) {
      sprintf(tmp, STR("%s "), chan->name);
      strcat(chans, tmp);
      chan = chan->next;
    }
    if (!strcmp(bot, "*"))
      botnet_send_zapf_broad(-1, botnetnick, NULL, chans);
    else
      botnet_send_zapf(nextbot(bot), botnetnick, bot, chans);
    log(LCAT_BOT, STR("Sent channel list to %s"), bot);
    chan = chanset;
    while (chan) {
      send_chan_settings(bot, chan);
      chan = chan->next;
    }
    if (strcmp(bot, "*")) {
      i = get_role(bot);
      sprintf(chans, STR("rl %d"), i);
      botnet_send_zapf(nextbot(bot), botnetnick, bot, chans);
    }
    log(LCAT_BOT, STR("Sent channel sync for all channels to %s"), bot);
  }
}

void *channel_malloc(int size)
{
  char *p;

#ifdef DEBUG_MEM
  p = nmalloc(size);		/* ((void *) (global[0] (size, MODULE_NAME, file, line))); */
#else
  p = nmalloc(size);
#endif
  bzero(p, size);
  return p;
}

void set_mode_protect(struct chanset_t *chan, char *set)
{
  int i,
    pos = 1;
  char *s,
   *s1;

  /* clear old modes */
  chan->mode_mns_prot = chan->mode_pls_prot = 0;
  chan->limit_prot = (-1);
  chan->key_prot[0] = 0;
  for (s = newsplit(&set); *s; s++) {
    i = 0;
    switch (*s) {
    case '+':
      pos = 1;
      break;
    case '-':
      pos = 0;
      break;
    case 'i':
      i = CHANINV;
      break;
    case 'p':
      i = CHANPRIV;
      break;
    case 's':
      i = CHANSEC;
      break;
    case 'm':
      i = CHANMODER;
      break;
    case 't':
      i = CHANTOPIC;
      break;
    case 'n':
      i = CHANNOMSG;
      break;
    case 'a':
      i = CHANANON;
      break;
    case 'q':
      i = CHANQUIET;
      break;
    case 'l':
      i = CHANLIMIT;
      chan->limit_prot = (-1);
      if (pos) {
	s1 = newsplit(&set);
	if (s1[0])
	  chan->limit_prot = atoi(s1);
      }
      break;
    case 'k':
      i = CHANKEY;
      chan->key_prot[0] = 0;
      if (pos) {
	s1 = newsplit(&set);
	if (s1[0])
	  strcpy(chan->key_prot, s1);
      }
      break;
    }
    if (i) {
      if (pos) {
	chan->mode_pls_prot |= i;
	chan->mode_mns_prot &= ~i;
      } else {
	chan->mode_pls_prot &= ~i;
	chan->mode_mns_prot |= i;
      }
    }
  }
  /* drummer: +s-p +p-s flood fixed. */
  if (chan->mode_pls_prot & CHANSEC)
    chan->mode_pls_prot &= ~CHANPRIV;
}

void get_mode_protect(struct chanset_t *chan, char *s)
{
  char *p = s,
    s1[121];
  int ok = 0,
    i,
    tst;

  s1[0] = 0;
  for (i = 0; i < 2; i++) {
    ok = 0;
    if (i == 0) {
      tst = chan->mode_pls_prot;
      if ((tst) || (chan->limit_prot != (-1)) || (chan->key_prot[0]))
	*p++ = '+';
      if (chan->limit_prot != (-1)) {
	*p++ = 'l';
	sprintf(&s1[strlen(s1)], STR("%d "), chan->limit_prot);
      }
      if (chan->key_prot[0]) {
	*p++ = 'k';
	sprintf(&s1[strlen(s1)], STR("%s "), chan->key_prot);
      }
    } else {
      tst = chan->mode_mns_prot;
      if (tst)
	*p++ = '-';
      if (tst & CHANKEY)
	*p++ = 'k';
      if (tst & CHANLIMIT)
	*p++ = 'l';
    }
    if (tst & CHANINV)
      *p++ = 'i';
    if (tst & CHANPRIV)
      *p++ = 'p';
    if (tst & CHANSEC)
      *p++ = 's';
    if (tst & CHANMODER)
      *p++ = 'm';
    if (tst & CHANTOPIC)
      *p++ = 't';
    if (tst & CHANNOMSG)
      *p++ = 'n';
    if (tst & CHANANON)
      *p++ = 'a';
    if (tst & CHANQUIET)
      *p++ = 'q';
  }
  *p = 0;
  if (s1[0]) {
    s1[strlen(s1) - 1] = 0;
    strcat(s, " ");
    strcat(s, s1);
  }
}

/* returns true if this is one of the channel masks */
int ismodeline(struct maskstruct *m, char *user)
{
  while (m && m->mask[0]) {
    if (!rfc_casecmp(m->mask, user))
      return 1;
    m = m->next;
  }
  return 0;
}

/* returns true if user matches one of the masklist -- drummer */
int ismasked(struct maskstruct *m, char *user)
{
  while (m && m->mask[0]) {
    if (wild_match(m->mask, user))
      return 1;
    m = m->next;
  }
  return 0;
}

/* destroy a chanset in the list */

/* does NOT free up memory associated with channel data inside the chanset! */
int killchanset(struct chanset_t *chan)
{
  struct chanset_t *c = chanset,
   *old = NULL;

  while (c) {
    if (c == chan) {
      if (old)
	old->next = c->next;
      else
	chanset = c->next;
      nfree(c);
      return 1;
    }
    old = c;
    c = c->next;
  }
  return 0;
}

/* Completely removes a channel.
 * This includes the removal of all channel-bans, -exempts and -invites, as
 * well as all user flags related to the channel.
 */
void remove_channel(struct chanset_t *chan)
{
  clear_channel(chan, 0);
  noshare = 1;
  /* Remove channel-bans */
  while (chan->bans)
    u_delban(chan, chan->bans->mask, 1);
  /* Remove channel-exempts */
  while (chan->exempts)
    u_delexempt(chan, chan->exempts->mask, 1);
  /* Remove channel-invites */
  while (chan->invites)
    u_delinvite(chan, chan->invites->mask, 1);
  /* Remove channel specific user flags */
  user_del_chan(chan->name);
  noshare = 0;
  killchanset(chan);
}

#ifdef G_USETCL
char *convert_element(char *src, char *dst)
{
  int flags;

  Tcl_ScanElement(src, &flags);
  Tcl_ConvertElement(src, dst, flags);
  return dst;
}
#endif

#define PLSMNS(x) (x ? '+' : '-')

#ifdef HUB
void write_channels(char *key)
{
  FILE *f;
  char s[121],
    w[1024],
    w2[1024];
  struct chanset_t *chan;

  Context;
  if (!chanfile[0])
    return;
  sprintf(s, STR("%s~new"), chanfile);
  f = fopen(s, "w");
  chmod(s, 0600);
  if (f == NULL) {
    log(LCAT_ERROR, STR("ERROR writing channel file."));
    return;
  }
  enc_fprintf(f, key, STR("#Dynamic Channel File for %s (%s) -- written %s\n"), origbotname, ver, ctime(&now));
  for (chan = chanset; chan; chan = chan->next) {
    get_mode_protect(chan, w);
    sprintf(w2, STR("{%s}"), w);
    strcpy(w,
	   STR("+%s chanmode %s limit %d idle-kick %d flood-chan %d:%d flood-ctcp %d:%d flood-join %d:%d flood-kick %d:%d flood-deop %d:%d %cclearbans %cenforcebans %cdynamicbans %cuserbans %cbitch %cstatuslog %ccycle %cseen "));
#ifdef G_MANUALOP
    strcat(w, STR("%cmanualop "));
#endif
#ifdef G_FASTOP
    strcat(w, STR("%cfastop "));
#endif
#ifdef G_TAKE
    strcat(w, STR("%ctake "));
#endif
#ifdef G_BACKUP
    strcat(w, STR("%cbackup "));
#endif
#ifdef G_MEAN
    strcat(w, STR("%cmean "));
#endif
    strcat(w, STR("%clocked %cinactive %cdynamicexempts %cuserexempts %cdynamicinvites %cuserinvites\n"));
    enc_fprintf(f, key, w, chan->name, w2, chan->limitraise, chan->idle_kick,
		chan->flood_pub_thr, chan->flood_pub_time,
		chan->flood_ctcp_thr, chan->flood_ctcp_time,
		chan->flood_join_thr, chan->flood_join_time,
		chan->flood_kick_thr, chan->flood_kick_time,
		chan->flood_deop_thr, chan->flood_deop_time,
		PLSMNS(channel_clearbans(chan)),
		PLSMNS(channel_enforcebans(chan)),
		PLSMNS(channel_dynamicbans(chan)),
		PLSMNS(!channel_nouserbans(chan)), PLSMNS(channel_bitch(chan)), PLSMNS(channel_logstatus(chan)), PLSMNS(channel_cycle(chan)), PLSMNS(channel_seen(chan)),
#ifdef G_MANUALOP

		PLSMNS(channel_manop(chan)),
#endif
#ifdef G_FASTOP
		PLSMNS(channel_fastop(chan)),
#endif
#ifdef G_TAKE
		PLSMNS(channel_take(chan)),
#endif
#ifdef G_BACKUP
		PLSMNS(channel_backup(chan)),
#endif
#ifdef G_MEAN
		PLSMNS(channel_mean(chan)),
#endif
		PLSMNS(channel_locked(chan)),
		PLSMNS(channel_inactive(chan)), PLSMNS(channel_dynamicexempts(chan)), PLSMNS(!channel_nouserexempts(chan)), PLSMNS(channel_dynamicinvites(chan)), PLSMNS(!channel_nouserinvites(chan)));
    if (fflush(f)) {
      log(LCAT_ERROR, STR("ERROR writing channel file."));
      fclose(f);
      return;
    }
  }
  fclose(f);
  unlink(chanfile);
  movefile(s, chanfile);
}

void read_channels(char *key, int create)
{
  struct chanset_t *chan,
   *chan2;
  FILE *f;
  char ln[2048],
   *tmp;
  int code;

  if (!chanfile[0])
    return;

  f = fopen(chanfile, "r");
  if (!f) {
    log(LCAT_ERROR, STR("No channel file - keeping inmem channel list"));
    return;
  }

  for (chan = chanset; chan; chan = chan->next)
    chan->status |= CHAN_FLAGGED;
  chan_hack = 1;

  while (fgets(ln, sizeof(ln), f)) {
    if (strlen(ln) > 20) {
      tmp = decrypt_string(key, ln);
      if (tmp && (tmp[0] == '+')) {
	char *chname;

	chname = newsplit(&tmp) + 1;
	code = do_channel_add(chname, tmp);
	bzero(tmp, strlen(tmp));
	if (!code)
	  log(LCAT_ERROR, STR("Chanfile error for %s"), chname);
	nfree(chname - 1);
      } else if (tmp)
	nfree(tmp);
    }
  }
  fclose(f);

  chan_hack = 0;
  chan = chanset;
  while (chan != NULL) {
    if (chan->status & CHAN_FLAGGED) {
      log(LCAT_INFO, STR("No longer supporting channel %s"), chan->name);
      if (shouldjoin(chan))
	dprintf(DP_SERVER, STR("PART %s\n"), chan->name);
      chan2 = chan->next;
      remove_channel(chan);
      chan = chan2;
    } else
      chan = chan->next;
  }

}

void channels_prerehash()
{
  struct chanset_t *chan;

  for (chan = chanset; chan; chan = chan->next) {
    chan->status |= CHAN_FLAGGED;
    /* flag will be cleared as the channels are re-added by the config file */
    /* any still flagged afterwards will be removed */
  }
}

void channels_rehash()
{
  /*  struct chanset_t *chan; */

  read_channels(localkey, 1);
  /* remove any extra channels */
  /* that's done in read_channels...
     chan = chanset;
     while (chan) {
     if (chan->status & CHAN_FLAGGED) {
     log(LCAT_INFO, STR("No longer supporting channel %s"), chan->name);
     if (shouldjoin(chan))
     dprintf(DP_SERVER, STR("PART %s\n"), chan->name);
     remove_channel(chan);
     chan = chanset;
     } else
     chan = chan->next;
     }
   */
}
#endif

void channels_report(int idx, int details)
{
  struct chanset_t *chan;
  int i;
  char s[1024],
    s2[100];
  struct flag_record fr = { FR_CHAN | FR_GLOBAL, 0, 0, 0, 0 };

  chan = chanset;
  while (chan != NULL) {
    if (idx != DP_STDOUT)
      get_user_flagrec(dcc[idx].user, &fr, chan->name);
    if ((idx == DP_STDOUT) || glob_master(fr) || chan_master(fr)) {
      s[0] = 0;
      if (channel_bitch(chan))
	strcat(s, STR("bitch, "));
      if (s[0])
	s[strlen(s) - 2] = 0;
      if (!s[0])
	strcpy(s, STR("lurking"));
      get_mode_protect(chan, s2);
      if (shouldjoin(chan)) {
	if (channel_active(chan)) {
	  dprintf(idx, STR("    %-10s: %2d member%s enforcing \"%s\" (%s)\n"), chan->name, chan->channel.members, chan->channel.members == 1 ? "," : "s,", s2, s);
	} else {
	  dprintf(idx, STR("    %-10s: (%s), enforcing \"%s\"  (%s)\n"), chan->name, channel_pending(chan) ? STR("pending") : STR("inactive"), s2, s);
	}
      } else {
	dprintf(idx, STR("    %-10s: no IRC support for this channel\n"), chan->name);
      }
      if (details) {
	s[0] = 0;
	i = 0;
	if (channel_clearbans(chan))
	  i += my_strcpy(s + i, STR("clear-bans "));
	if (channel_enforcebans(chan))
	  i += my_strcpy(s + i, STR("enforce-bans "));
	if (channel_dynamicbans(chan))
	  i += my_strcpy(s + i, STR("dynamic-bans "));
	if (channel_nouserbans(chan))
	  i += my_strcpy(s + i, STR("forbid-user-bans "));
	if (channel_bitch(chan))
	  i += my_strcpy(s + i, STR("bitch "));
	if (channel_logstatus(chan))
	  i += my_strcpy(s + i, STR("log-status "));
	if (channel_cycle(chan))
	  i += my_strcpy(s + i, STR("cycle "));
	if (channel_seen(chan))
	  i += my_strcpy(s + i, STR("seen "));
#ifdef G_MANUALOP
	if (channel_manop(chan))
	  i += my_strcpy(s + i, STR("manualop "));
#endif
#ifdef G_FASTOP
	if (channel_fastop(chan))
	  i += my_strcpy(s + i, STR("fastop "));
#endif
#ifdef G_TAKE
	if (channel_take(chan))
	  i += my_strcpy(s + i, STR("take "));
#endif
#ifdef G_BACKUP
	if (channel_backup(chan))
	  i += my_strcpy(s + i, STR("backup "));
#endif
#ifdef G_MEAN
	if (channel_mean(chan))
	  i += my_strcpy(s + i, STR("mean "));
#endif
	if (channel_locked(chan))
	  i += my_strcpy(s + i, STR("locked "));
	if (channel_dynamicexempts(chan))
	  i += my_strcpy(s + i, STR("dynamic-exempts "));
	if (channel_nouserexempts(chan))
	  i += my_strcpy(s + i, STR("forbid-user-exempts "));
	if (channel_dynamicinvites(chan))
	  i += my_strcpy(s + i, STR("dynamic-invites "));
	if (channel_nouserinvites(chan))
	  i += my_strcpy(s + i, STR("forbid-user-invites "));
	if (channel_inactive(chan))
	  i += my_strcpy(s + i, STR("inactive "));
	dprintf(idx, STR("      Options: %s\n"), s);
	if (chan->idle_kick)
	  dprintf(idx, STR("      Kicking idle users after %d min\n"), chan->idle_kick);
	if (chan->limitraise)
	  dprintf(idx, STR("      Raising limit to +%d every 2 minutes\n"), chan->limitraise);
      }
    }
    chan = chan->next;
  }
  if (details) {
    dprintf(idx, STR("    Bans last %d mins.\n"), ban_time);
    dprintf(idx, STR("    Exemptions last %d mins.\n"), exempt_time);
    dprintf(idx, STR("    Invitations last %d mins.\n"), invite_time);
  }
}

int expmem_masklist(struct maskstruct *m)
{
  int result = 0;

  while (m) {
    result += sizeof(struct maskstruct);

    if (m->mask)
      result += strlen(m->mask) + 1;
    if (m->who)
      result += strlen(m->who) + 1;
    m = m->next;
  }
  return result;
}

int expmem_channels()
{
  int tot = 0;
  struct chanset_t *chan = chanset;

  Context;
  while (chan != NULL) {
    tot += sizeof(struct chanset_t);

    tot += strlen(chan->channel.key) + 1;
    if (chan->channel.topic)
      tot += strlen(chan->channel.topic) + 1;
    tot += (sizeof(struct memstruct) * (chan->channel.members + 1));

    tot += expmem_masklist(chan->channel.ban);
    tot += expmem_masklist(chan->channel.exempt);
    tot += expmem_masklist(chan->channel.invite);

    chan = chan->next;
  }
  return tot;
}

#ifdef G_USETCL
char *traced_globchanset(ClientData cdata, Tcl_Interp * irp, char *name1, char *name2, int flags)
{
  char *s;
  char *t;
  int i;
  int items;
  char **item;

  Context;
  if (flags & (TCL_TRACE_READS | TCL_TRACE_UNSETS)) {
    Tcl_SetVar2(interp, name1, name2, glob_chanset, TCL_GLOBAL_ONLY);
    if (flags & TCL_TRACE_UNSETS)
      Tcl_TraceVar(interp, STR("global-chanset"), TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS, traced_globchanset, NULL);
  } else {			/* write */
    s = Tcl_GetVar2(interp, name1, name2, TCL_GLOBAL_ONLY);
    Tcl_SplitList(interp, s, &items, &item);
    Context;
    for (i = 0; i < items; i++) {
      if (!(item[i]) || (strlen(item[i]) < 2))
	continue;
      s = glob_chanset;
      while (s[0]) {
	t = strchr(s, ' ');	/* cant be NULL coz of the extra space */
	Context;
	t[0] = 0;
	if (!strcmp(s + 1, item[i] + 1)) {
	  s[0] = item[i][0];	/* +- */
	  t[0] = ' ';
	  break;
	}
	t[0] = ' ';
	s = t + 1;
      }
    }
    if (item)			/* hmm it cant be 0 */
      Tcl_Free((char *) item);
    Tcl_SetVar2(interp, name1, name2, glob_chanset, TCL_GLOBAL_ONLY);
  }
  return NULL;
}
#endif

cmd_t channels_bot[] = {
  {"chanlist", "", (Function) got_chanlist, NULL}
  ,
  {"chansync", "", (Function) got_chansync, NULL}
  ,
  {"rl", "", (Function) got_role, NULL}
  ,
  {"sj", "", (Function) got_sj, NULL}
  ,
#ifdef HUB
  {"o1", "", (Function) got_o1, NULL}
  ,
  {"kl", "", (Function) got_kl, NULL}
  ,
#endif
  {"ltp", "", (Function) got_locktopic, NULL},
  {0, 0, 0, 0}
};

#ifdef G_USETCL
tcl_ints my_tcl_ints[] = {
  {"share-greet", 0, 0}
  ,
  {"use-info", &use_info, 0}
  ,
  {"ban-time", &ban_time, 0}
  ,
  {"exempt-time", &exempt_time, 0}
  ,
  {"invite-time", &invite_time, 0}
  ,
  {0, 0, 0}
};

tcl_coups mychan_tcl_coups[] = {
  {"global-flood-chan", &gfld_chan_thr, &gfld_chan_time}
  ,
  {"global-flood-deop", &gfld_deop_thr, &gfld_deop_time}
  ,
  {"global-flood-kick", &gfld_kick_thr, &gfld_kick_time}
  ,
  {"global-flood-join", &gfld_join_thr, &gfld_join_time}
  ,
  {"global-flood-ctcp", &gfld_ctcp_thr, &gfld_ctcp_time}
  ,
  {0, 0, 0}
};

tcl_strings my_tcl_strings[] = {
  {"chanfile", chanfile, 120, STR_PROTECT}
  ,
  {"global-chanmode", glob_chanmode, 64, 0}
  ,
  {0, 0, 0, 0}
};
#endif

void init_channels()
{
  gfld_chan_thr = 10;
  gfld_chan_time = 60;
  gfld_deop_thr = 3;
  gfld_deop_time = 10;
  gfld_kick_thr = 3;
  gfld_kick_time = 10;
  gfld_join_thr = 5;
  gfld_join_time = 60;
  gfld_ctcp_thr = 5;
  gfld_ctcp_time = 60;
  Context;
  glob_chanset[0] = 0;
  strcat(glob_chanset, STR("-clearbans "));
  strcat(glob_chanset, STR("+enforcebans "));
  strcat(glob_chanset, STR("+dynamicbans "));
  strcat(glob_chanset, STR("+userbans "));
  strcat(glob_chanset, STR("-bitch "));
  strcat(glob_chanset, STR("-statuslog "));
  strcat(glob_chanset, STR("-cycle "));
  strcat(glob_chanset, STR("+seen "));
#ifdef G_MANUALOP
  strcat(glob_chanset, STR("+manualop "));
#endif
#ifdef G_FASTOP
  strcat(glob_chanset, STR("-fastop "));
#endif
#ifdef G_TAKE
  strcat(glob_chanset, STR("-take "));
#endif
#ifdef G_BACKUP
  strcat(glob_chanset, STR("-backup "));
#endif
#ifdef G_MEAN
  strcat(glob_chanset, STR("+mean "));
#endif
  strcat(glob_chanset, STR("-locked "));
  strcat(glob_chanset, STR("-inactive "));
  strcat(glob_chanset, STR("+userexempts +dynamicexempts +userinvites +dynamicinvites "));

  add_hook(HOOK_MINUTELY, (Function) check_expired_bans);
  add_hook(HOOK_MINUTELY, (Function) check_expired_exempts);
  add_hook(HOOK_MINUTELY, (Function) check_expired_invites);
#ifdef HUB
  add_hook(HOOK_MINUTELY, (Function) check_limitraise);
  add_hook(HOOK_30SECONDLY, (Function) rebalance_roles);
  add_hook(HOOK_10SECONDLY, (Function) check_enforced_topics);
  add_hook(HOOK_REHASH, (Function) channels_rehash);
  add_hook(HOOK_PRE_REHASH, (Function) channels_prerehash);
#endif
  add_hook(HOOK_3SECONDLY, (Function) channels_checkslowjoin);
#ifdef G_USETCL
  Tcl_TraceVar(interp, STR("global-chanset"), TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS, traced_globchanset, NULL);
#endif
  add_builtins(H_dcc, C_dcc_irc);
  add_builtins(H_bot, channels_bot);
#ifdef G_USETCL
  add_tcl_commands(channels_cmds);
  add_tcl_strings(my_tcl_strings);
  my_tcl_ints[0].val = &share_greet;
  add_tcl_ints(my_tcl_ints);
  add_tcl_coups(mychan_tcl_coups);
#endif
#ifdef HUB
  read_channels(localkey, 0);
#endif
  ban_time = 30 + rand() % 120;
}




