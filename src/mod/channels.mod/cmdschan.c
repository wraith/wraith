/*
 * cmdschan.c -- part of channels.mod
 *   commands from a user via dcc that cause server interaction
 *
 */

#include <ctype.h>

static struct flag_record user	 = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0};
static struct flag_record victim = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0};


static void cmd_pls_ban(struct userrec *u, int idx, char *par)
{
  char *chname, *who, s[UHOSTLEN], s1[UHOSTLEN], *p, *p_expire;
  unsigned long int expire_time = 0, expire_foo;
  int sticky = 0;
  struct chanset_t *chan = NULL;
  module_entry *me;

  if (!par[0]) {
    dprintf(idx, "Usage: +ban <hostmask> [channel] [%%<XdXhXm>] [reason]\n");
  } else {
    who = newsplit(&par);
    if (par[0] && strchr(CHANMETA, par[0]))
      chname = newsplit(&par);
    else
      chname = 0;
    if (chname || !(u->flags & USER_MASTER)) {
      if (!chname)
	chname = dcc[idx].u.chat->con_chan;
      get_user_flagrec(u, &user, chname);
      chan = findchan_by_dname(chname);
      /* *shrug* ??? (guppy:10Feb1999) */
      if (!chan) {
        dprintf(idx, "That channel doesn't exist!\n");
        return;
      } else if (private(user, chan, PRIV_OP)) {
        dprintf(idx, "No such channel.\n");
        return;
      } else if (!((glob_op(user) && !chan_deop(user)) || chan_op(user))) {
        dprintf(idx, "You don't have access to set bans on %s.\n", chname);
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
      par = "requested";
    else if (strlen(par) > MASKREASON_MAX)
      par[MASKREASON_MAX] = 0;
    if (strlen(who) > UHOSTMAX - 4)
      who[UHOSTMAX - 4] = 0;
    /* Fix missing ! or @ BEFORE checking against myself */
    if (!strchr(who, '!')) {
      if (!strchr(who, '@'))
	egg_snprintf(s, sizeof s, "%s!*@*", who);	/* Lame nick ban */
      else
	egg_snprintf(s, sizeof s, "*!%s", who);
    } else if (!strchr(who, '@'))
      egg_snprintf(s, sizeof s, "%s@*", who);	/* brain-dead? */
    else
      strncpyz(s, who, sizeof s);
    if ((me = module_find("server", 0, 0)) && me->funcs)
      egg_snprintf(s1, sizeof s1, "%s!%s", me->funcs[SERVER_BOTNAME],
	            me->funcs[SERVER_BOTUSERHOST]);
    else
      egg_snprintf(s1, sizeof s1, "%s!%s@%s", origbotname, botuser, hostname);
    if (wild_match(s, s1)) {
      dprintf(idx, "I'm not going to ban myself.\n");
      putlog(LOG_CMDS, "*", "#%s# attempted +ban %s", dcc[idx].nick, s);
    } else {
      /* IRC can't understand bans longer than 70 characters */
      if (strlen(s) > 70) {
	s[69] = '*';
	s[70] = 0;
      }
      if (chan) {
	u_addban(chan, s, dcc[idx].nick, par,
		 expire_time ? now + expire_time : 0, 0);
	if (par[0] == '*') {
	  sticky = 1;
	  par++;
	  putlog(LOG_CMDS, "*", "#%s# (%s) +ban %s %s (%s) (sticky)",
		 dcc[idx].nick, dcc[idx].u.chat->con_chan, s, chan->dname, par);
	  dprintf(idx, "New %s sticky ban: %s (%s)\n", chan->dname, s, par);
	} else {
	  putlog(LOG_CMDS, "*", "#%s# (%s) +ban %s %s (%s)", dcc[idx].nick,
		 dcc[idx].u.chat->con_chan, s, chan->dname, par);
	  dprintf(idx, "New %s ban: %s (%s)\n", chan->dname, s, par);
	}
	/* Avoid unnesessary modes if you got +dynamicbans, and there is
	 * no reason to set mode if irc.mod aint loaded. (dw 001120)
	 */
	if ((me = module_find("irc", 0, 0)))
	  (me->funcs[IRC_CHECK_THIS_BAN])(chan, s, sticky);
      } else {
	u_addban(NULL, s, dcc[idx].nick, par,
		 expire_time ? now + expire_time : 0, 0);
	if (par[0] == '*') {
	  sticky = 1;
	  par++;
	  putlog(LOG_CMDS, "*", "#%s# (GLOBAL) +ban %s (%s) (sticky)",
		 dcc[idx].nick, s, par);
	  dprintf(idx, "New sticky ban: %s (%s)\n", s, par);
	} else {
	  putlog(LOG_CMDS, "*", "#%s# (GLOBAL) +ban %s (%s)", dcc[idx].nick,
		 s, par);
	  dprintf(idx, "New ban: %s (%s)\n", s, par);
	}
	if ((me = module_find("irc", 0, 0)))
	  for (chan = chanset; chan != NULL; chan = chan->next)
	    (me->funcs[IRC_CHECK_THIS_BAN])(chan, s, sticky);
      }
    }
  }
}

static void cmd_pls_exempt(struct userrec *u, int idx, char *par)
{
  char *chname, *who, s[UHOSTLEN], s1[UHOSTLEN], *p, *p_expire;
  unsigned long int expire_time = 0, expire_foo;
  struct chanset_t *chan = NULL;
  module_entry *me;

  if (!use_exempts) {
    dprintf(idx, "This command can only be used with use-exempts enabled.\n");
    return;
  }
  if (!par[0]) {
    dprintf(idx, "Usage: +exempt <hostmask> [channel] [%%<XdXhXm>] [reason]\n");
  } else {
    who = newsplit(&par);
    if (par[0] && strchr(CHANMETA, par[0]))
      chname = newsplit(&par);
    else
      chname = 0;
    if (chname || !(u->flags & USER_MASTER)) {
      if (!chname)
	chname = dcc[idx].u.chat->con_chan;
      get_user_flagrec(u,&user,chname);
      chan = findchan_by_dname(chname);
      /* *shrug* ??? (guppy:10Feb99) */
      if (!chan) {
        dprintf(idx, "That channel doesn't exist!\n");
	return;
      } else if (private(user, chan, PRIV_OP)) {
        dprintf(idx, "No such channel.\n");
        return;
      } else if (!((glob_op(user) && !chan_deop(user)) || chan_op(user))) {
        dprintf(idx, "You don't have access to set exempts on %s.\n", chname);
        return;
      }
    } else
      chan = 0;
    /* Added by Q and Solal  - Requested by Arty2, special thanx :) */
    if (par[0] == '%') {
      p = newsplit (&par);
      p_expire = p + 1;
      while (*(++p) != 0) {
	switch (tolower(*p)) {
	case 'd':
	  *p = 0;
	  expire_foo = strtol (p_expire, NULL, 10);
	  if (expire_foo > 365)
	    expire_foo = 365;
	  expire_time += 86400 * expire_foo;
	  p_expire = p + 1;
	  break;
	case 'h':
	  *p = 0;
	  expire_foo = strtol (p_expire, NULL, 10);
	  if (expire_foo > 8760)
	    expire_foo = 8760;
	  expire_time += 3600 * expire_foo;
	  p_expire = p + 1;
	  break;
	case 'm':
	  *p = 0;
	  expire_foo = strtol (p_expire, NULL, 10);
	  if (expire_foo > 525600)
	    expire_foo = 525600;
	  expire_time += 60 * expire_foo;
	  p_expire = p + 1;
	}
      }
    }
    if (!par[0])
      par = "requested";
    else if (strlen(par) > MASKREASON_MAX)
      par[MASKREASON_MAX] = 0;
    if (strlen(who) > UHOSTMAX - 4)
      who[UHOSTMAX - 4] = 0;
    /* Fix missing ! or @ BEFORE checking against myself */
    if (!strchr(who, '!')) {
      if (!strchr(who, '@'))
	egg_snprintf(s, sizeof s, "%s!*@*", who);	/* Lame nick exempt */
      else
	egg_snprintf(s, sizeof s, "*!%s", who);
    } else if (!strchr(who, '@'))
      egg_snprintf(s, sizeof s, "%s@*", who);		/* brain-dead? */
    else
      strncpyz(s, who, sizeof s);
    if ((me = module_find("server",0,0)) && me->funcs)
      egg_snprintf(s1, sizeof s1, "%s!%s", me->funcs[SERVER_BOTNAME],
		     me->funcs[SERVER_BOTUSERHOST]);
    else
      egg_snprintf(s1, sizeof s1, "%s!%s@%s", origbotname, botuser, hostname);

    /* IRC can't understand exempts longer than 70 characters */
    if (strlen(s) > 70) {
      s[69] = '*';
      s[70] = 0;
    }
    if (chan) {
      u_addexempt(chan, s, dcc[idx].nick, par,
		  expire_time ? now + expire_time : 0, 0);
      if (par[0] == '*') {
	par++;
	putlog(LOG_CMDS, "*", "#%s# (%s) +exempt %s %s (%s) (sticky)",
	       dcc[idx].nick, dcc[idx].u.chat->con_chan, s, chan->dname, par);
	dprintf(idx, "New %s sticky exempt: %s (%s)\n", chan->dname, s, par);
      } else {
	putlog(LOG_CMDS, "*", "#%s# (%s) +exempt %s %s (%s)", dcc[idx].nick,
	       dcc[idx].u.chat->con_chan, s, chan->dname, par);
	dprintf(idx, "New %s exempt: %s (%s)\n", chan->dname, s, par);
      }
      add_mode(chan, '+', 'e', s);
    } else {
      u_addexempt(NULL, s, dcc[idx].nick, par,
		  expire_time ? now + expire_time : 0, 0);
      if (par[0] == '*') {
	par++;
	putlog(LOG_CMDS, "*", "#%s# (GLOBAL) +exempt %s (%s) (sticky)",
	       dcc[idx].nick, s, par);
	dprintf(idx, "New sticky exempt: %s (%s)\n", s, par);
      } else {
	putlog(LOG_CMDS, "*", "#%s# (GLOBAL) +exempt %s (%s)", dcc[idx].nick,
	       s, par);
	dprintf(idx, "New exempt: %s (%s)\n", s, par);
      }
      for (chan = chanset; chan != NULL; chan = chan->next)
	add_mode(chan, '+', 'e', s);
    }
  }
}

static void cmd_pls_invite(struct userrec *u, int idx, char *par)
{
  char *chname, *who, s[UHOSTLEN], s1[UHOSTLEN], *p, *p_expire;
  unsigned long int expire_time = 0, expire_foo;
  struct chanset_t *chan = NULL;
  module_entry *me;

  if (!use_invites) {
    dprintf(idx, "This command can only be used with use-invites enabled.\n");
    return;
  }

  if (!par[0]) {
    dprintf(idx, "Usage: +invite <hostmask> [channel] [%%<XdXhXm>] [reason]\n");
  } else {
    who = newsplit(&par);
    if (par[0] && strchr(CHANMETA, par[0]))
      chname = newsplit(&par);
    else
      chname = 0;
    if (chname || !(u->flags & USER_MASTER)) {
      if (!chname)
	chname = dcc[idx].u.chat->con_chan;
      get_user_flagrec(u,&user,chname);
      chan = findchan_by_dname(chname);
      /* *shrug* ??? (guppy:10Feb99) */
      if (!chan) {
	dprintf(idx, "That channel doesn't exist!\n");
	return;
      } else if (private(user, chan, PRIV_OP)) {
        dprintf(idx, "No such channel.\n");
        return;
      } else if (!((glob_op(user) && !chan_deop(user)) || chan_op(user))) {
        dprintf(idx, "You don't have access to set invites on %s.\n", chname);
        return;
      }
    } else
      chan = 0;
    /* Added by Q and Solal  - Requested by Arty2, special thanx :) */
    if (par[0] == '%') {
      p = newsplit (&par);
      p_expire = p + 1;
      while (*(++p) != 0) {
	switch (tolower(*p)) {
	case 'd':
	  *p = 0;
	  expire_foo = strtol (p_expire, NULL, 10);
	  if (expire_foo > 365)
	    expire_foo = 365;
	  expire_time += 86400 * expire_foo;
	  p_expire = p + 1;
	  break;
	case 'h':
	  *p = 0;
	  expire_foo = strtol (p_expire, NULL, 10);
	  if (expire_foo > 8760)
	    expire_foo = 8760;
	  expire_time += 3600 * expire_foo;
	  p_expire = p + 1;
	  break;
	case 'm':
	  *p = 0;
	  expire_foo = strtol (p_expire, NULL, 10);
	  if (expire_foo > 525600)
	    expire_foo = 525600;
	  expire_time += 60 * expire_foo;
	  p_expire = p + 1;
	}
      }
    }
    if (!par[0])
      par = "requested";
    else if (strlen(par) > MASKREASON_MAX)
      par[MASKREASON_MAX] = 0;
    if (strlen(who) > UHOSTMAX - 4)
      who[UHOSTMAX - 4] = 0;
    /* Fix missing ! or @ BEFORE checking against myself */
    if (!strchr(who, '!')) {
      if (!strchr(who, '@'))
	egg_snprintf(s, sizeof s, "%s!*@*", who);	/* Lame nick invite */
      else
	egg_snprintf(s, sizeof s, "*!%s", who);
    } else if (!strchr(who, '@'))
      egg_snprintf(s, sizeof s, "%s@*", who);		/* brain-dead? */
    else
      strncpyz(s, who, sizeof s);
    if ((me = module_find("server",0,0)) && me->funcs)
      egg_snprintf(s1, sizeof s1, "%s!%s", me->funcs[SERVER_BOTNAME],
		     me->funcs[SERVER_BOTUSERHOST]);
    else
      egg_snprintf(s1, sizeof s1, "%s!%s@%s", origbotname, botuser, hostname);

    /* IRC can't understand invites longer than 70 characters */
    if (strlen(s) > 70) {
      s[69] = '*';
      s[70] = 0;
    }
    if (chan) {
      u_addinvite(chan, s, dcc[idx].nick, par,
		  expire_time ? now + expire_time : 0, 0);
      if (par[0] == '*') {
	par++;
	putlog(LOG_CMDS, "*", "#%s# (%s) +invite %s %s (%s) (sticky)",
	       dcc[idx].nick, dcc[idx].u.chat->con_chan, s, chan->dname, par);
	dprintf(idx, "New %s sticky invite: %s (%s)\n", chan->dname, s, par);
      } else {
	putlog(LOG_CMDS, "*", "#%s# (%s) +invite %s %s (%s)", dcc[idx].nick,
	       dcc[idx].u.chat->con_chan, s, chan->dname, par);
	dprintf(idx, "New %s invite: %s (%s)\n", chan->dname, s, par);
      }
      add_mode(chan, '+', 'I', s);
    } else {
      u_addinvite(NULL, s, dcc[idx].nick, par,
		  expire_time ? now + expire_time : 0, 0);
      if (par[0] == '*') {
	par++;
	putlog(LOG_CMDS, "*", "#%s# (GLOBAL) +invite %s (%s) (sticky)",
	       dcc[idx].nick, s, par);
	dprintf(idx, "New sticky invite: %s (%s)\n", s, par);
      } else {
	putlog(LOG_CMDS, "*", "#%s# (GLOBAL) +invite %s (%s)", dcc[idx].nick,
	       s, par);
	dprintf(idx, "New invite: %s (%s)\n", s, par);
      }
      for (chan = chanset; chan != NULL; chan = chan->next)
	add_mode(chan, '+', 'I', s);
    }
  }
}

static void cmd_mns_ban(struct userrec *u, int idx, char *par)
{
  int console = 0, i = 0, j;
  struct chanset_t *chan = NULL;
  char s[UHOSTLEN], *ban, *chname, *mask;
  masklist *b;

  if (!par[0]) {
    dprintf(idx, "Usage: -ban <hostmask|ban #> [channel]\n");
    return;
  }
  ban = newsplit(&par);
  if (par[0] && strchr(CHANMETA, par[0]))
    chname = newsplit(&par);
  else {
    chname = dcc[idx].u.chat->con_chan;
    console = 1;
  }
  if (chname || !(u->flags & USER_MASTER)) {
    if (!chname)
      chname = dcc[idx].u.chat->con_chan;
    get_user_flagrec(u, &user, chname);

    if (private(user, findchan_by_dname(chname), PRIV_OP)) {
      dprintf(idx, "No such channel.\n");
      return;
    }

    if ((!chan_op(user) && (!glob_op(user) || chan_deop(user)))) {
      dprintf(idx, "You don't have access to remove bans on %s.\n", chname);
      return;
    }
  }
  strncpyz(s, ban, sizeof s);
  if (console) {
    i = u_delban(NULL, s, (u->flags & USER_MASTER));
    if (i > 0) {
      if (lastdeletedmask)
        mask = lastdeletedmask;
      else
        mask = s;
      putlog(LOG_CMDS, "*", "#%s# -ban %s", dcc[idx].nick, mask);
      dprintf(idx, "%s: %s\n", IRC_REMOVEDBAN, mask);
      for (chan = chanset; chan != NULL; chan = chan->next)
        add_mode(chan, '-', 'b', mask);
      return;
    }
  }
  /* Channel-specific ban? */
  if (chname)
    chan = findchan_by_dname(chname);
  if (!chan) {
    dprintf(idx, "Invalid channel.\n");
    return;
  }
  if (str_isdigit(ban)) {
    i = atoi(ban);
    /* substract the numer of global bans to get the number of the channel ban */
    egg_snprintf(s, sizeof s, "%d", i);
    j = u_delban(0, s, 0);
    if (j < 0) {
      egg_snprintf(s, sizeof s, "%d", -j);
      j = u_delban(chan, s, 1);
      if (j > 0) {
        if (lastdeletedmask)
          mask = lastdeletedmask;
        else
          mask = s;
        putlog(LOG_CMDS, "*", "#%s# (%s) -ban %s", dcc[idx].nick, chan->dname,
               mask);
        dprintf(idx, "Removed %s channel ban: %s\n", chan->dname, mask);
        add_mode(chan, '-', 'b', mask);
        return;
      }
    }
    i = 0;
    for (b = chan->channel.ban; b && b->mask && b->mask[0]; b = b->next) {
      if ((!u_equals_mask(global_bans, b->mask)) &&
          (!u_equals_mask(chan->bans, b->mask))) {
        i++;
        if (i == -j) {
          add_mode(chan, '-', 'b', b->mask);
          dprintf(idx, "%s '%s' on %s.\n", IRC_REMOVEDBAN,
                  b->mask, chan->dname);
          putlog(LOG_CMDS, "*", "#%s# (%s) -ban %s [on channel]",
                 dcc[idx].nick, dcc[idx].u.chat->con_chan, ban);
          return;
        }
      }
    }
  } else {
    j = u_delban(chan, ban, 1);
    if (j > 0) {
      putlog(LOG_CMDS, "*", "#%s# (%s) -ban %s", dcc[idx].nick,
             dcc[idx].u.chat->con_chan, ban);
      dprintf(idx, "Removed %s channel ban: %s\n", chname, ban);
      add_mode(chan, '-', 'b', ban);
      return;
    }
    for (b = chan->channel.ban; b && b->mask && b->mask[0]; b = b->next) {
      if (!rfc_casecmp(b->mask, ban)) {
        add_mode(chan, '-', 'b', b->mask);
        dprintf(idx, "%s '%s' on %s.\n", IRC_REMOVEDBAN, b->mask, chan->dname);
        putlog(LOG_CMDS, "*", "#%s# (%s) -ban %s [on channel]",
               dcc[idx].nick, dcc[idx].u.chat->con_chan, ban);
        return;
      }
    }
  }
  dprintf(idx, "No such ban.\n");
}

static void cmd_mns_exempt(struct userrec *u, int idx, char *par)
{
  int console = 0, i = 0, j;
  struct chanset_t *chan = NULL;
  char s[UHOSTLEN], *exempt, *chname, *mask;
  masklist *e;

  if (!use_exempts) {
    dprintf(idx, "This command can only be used with use-exempts enabled.\n");
    return;
  }
  if (!par[0]) {
    dprintf(idx, "Usage: -exempt <hostmask|exempt #> [channel]\n");
    return;
  }
  exempt = newsplit(&par);
  if (par[0] && strchr(CHANMETA, par[0]))
    chname = newsplit(&par);
  else {
    chname = dcc[idx].u.chat->con_chan;
    console = 1;
  }
  if (chname || !(u->flags & USER_MASTER)) {
    if (!chname)
      chname = dcc[idx].u.chat->con_chan;
    get_user_flagrec(u, &user, chname);

    if (private(user, findchan_by_dname(chname), PRIV_OP)) {
      dprintf(idx, "No such channel.\n");
      return;
    }

    if ((!chan_op(user) && (!glob_op(user) || chan_deop(user)))) {
      dprintf(idx, "You don't have access to remove exempts on %s.\n", chname);
      return;
    }
  }
  strncpyz(s, exempt, sizeof s);
  if (console) {
    i = u_delexempt(NULL, s, (u->flags & USER_MASTER));
    if (i > 0) {
      if (lastdeletedmask)
        mask = lastdeletedmask;
      else
        mask = s;
      putlog(LOG_CMDS, "*", "#%s# -exempt %s", dcc[idx].nick, mask);
      dprintf(idx, "%s: %s\n", IRC_REMOVEDEXEMPT, mask);
      for (chan = chanset; chan != NULL; chan = chan->next)
        add_mode(chan, '-', 'e', mask);
      return;
    }
  }
  /* Channel-specific exempt? */
  if (chname)
    chan = findchan_by_dname(chname);
  if (!chan) {
    dprintf(idx, "Invalid channel.\n");
    return;
  }
  if (str_isdigit(exempt)) {
    i = atoi(exempt);
    /* substract the numer of global exempts to get the number of the channel exempt */
    egg_snprintf(s, sizeof s, "%d", i);
    j = u_delexempt(0, s, 0);
    if (j < 0) {
      egg_snprintf(s, sizeof s, "%d", -j);
      j = u_delexempt(chan, s, 1);
      if (j > 0) {
        if (lastdeletedmask)
          mask = lastdeletedmask;
        else
          mask = s;
        putlog(LOG_CMDS, "*", "#%s# (%s) -exempt %s", dcc[idx].nick,
               chan->dname, mask);
        dprintf(idx, "Removed %s channel exempt: %s\n", chan->dname, mask);
        add_mode(chan, '-', 'e', mask);
        return;
      }
    }
    i = 0;
    for (e = chan->channel.exempt; e && e->mask && e->mask[0]; e = e->next) {
      if (!u_equals_mask(global_exempts, e->mask) &&
          !u_equals_mask(chan->exempts, e->mask)) {
        i++;
        if (i == -j) {
          add_mode(chan, '-', 'e', e->mask);
          dprintf(idx, "%s '%s' on %s.\n", IRC_REMOVEDEXEMPT,
                  e->mask, chan->dname);
          putlog(LOG_CMDS, "*", "#%s# (%s) -exempt %s [on channel]",
                 dcc[idx].nick, dcc[idx].u.chat->con_chan, exempt);
          return;
        }
      }
    }
  } else {
    j = u_delexempt(chan, exempt, 1);
    if (j > 0) {
      putlog(LOG_CMDS, "*", "#%s# (%s) -exempt %s", dcc[idx].nick,
             dcc[idx].u.chat->con_chan, exempt);
      dprintf(idx, "Removed %s channel exempt: %s\n", chname, exempt);
      add_mode(chan, '-', 'e', exempt);
      return;
    }
    for (e = chan->channel.exempt; e && e->mask && e->mask[0]; e = e->next) {
      if (!rfc_casecmp(e->mask, exempt)) {
        add_mode(chan, '-', 'e', e->mask);
        dprintf(idx, "%s '%s' on %s.\n",
                IRC_REMOVEDEXEMPT, e->mask, chan->dname);
        putlog(LOG_CMDS, "*", "#%s# (%s) -exempt %s [on channel]",
               dcc[idx].nick, dcc[idx].u.chat->con_chan, exempt);
        return;
      }
    }
  }
  dprintf(idx, "No such exemption.\n");
}

static void cmd_mns_invite(struct userrec *u, int idx, char *par)
{
  int console = 0, i = 0, j;
  struct chanset_t *chan = NULL;
  char s[UHOSTLEN], *invite, *chname, *mask;
  masklist *inv;

  if (!use_invites) {
    dprintf(idx, "This command can only be used with use-invites enabled.\n");
    return;
  }
  if (!par[0]) {
    dprintf(idx, "Usage: -invite <hostmask|invite #> [channel]\n");
    return;
  }
  invite = newsplit(&par);
  if (par[0] && strchr(CHANMETA, par[0]))
    chname = newsplit(&par);
  else {
    chname = dcc[idx].u.chat->con_chan;
    console = 1;
  }
  if (chname || !(u->flags & USER_MASTER)) {
    if (!chname)
      chname = dcc[idx].u.chat->con_chan;
    get_user_flagrec(u, &user, chname);

    if (private(user, findchan_by_dname(chname), PRIV_OP)) {
      dprintf(idx, "No such channel.\n");
      return;
    }

    if ((!chan_op(user) && (!glob_op(user) || chan_deop(user)))) {
      dprintf(idx, "You don't have access to remove invites on %s.\n", chname);
      return;
    }
  }
  strncpyz(s, invite, sizeof s);
  if (console) {
    i = u_delinvite(NULL, s, (u->flags & USER_MASTER));
    if (i > 0) {
      if (lastdeletedmask)
        mask = lastdeletedmask;
      else
        mask = s;
      putlog(LOG_CMDS, "*", "#%s# -invite %s", dcc[idx].nick, mask);
      dprintf(idx, "%s: %s\n", IRC_REMOVEDINVITE, mask);
      for (chan = chanset; chan != NULL; chan = chan->next)
        add_mode(chan, '-', 'I', mask);
      return;
    }
  }
  /* Channel-specific invite? */
  if (chname)
    chan = findchan_by_dname(chname);
  if (!chan) {
    dprintf(idx, "Invalid channel.\n");
    return;
  }
  if (str_isdigit(invite)) {
    i = atoi(invite);
    /* substract the numer of global invites to get the number of the channel invite */
    egg_snprintf(s, sizeof s, "%d", i);
    j = u_delinvite(0, s, 0);
    if (j < 0) {
      egg_snprintf(s, sizeof s, "%d", -j);
      j = u_delinvite(chan, s, 1);
      if (j > 0) {
        if (lastdeletedmask)
          mask = lastdeletedmask;
        else
          mask = s;
        putlog(LOG_CMDS, "*", "#%s# (%s) -invite %s", dcc[idx].nick,
               chan->dname, mask);
        dprintf(idx, "Removed %s channel invite: %s\n", chan->dname, mask);
        add_mode(chan, '-', 'I', mask);
        return;
      }
    }
    i = 0;
    for (inv = chan->channel.invite; inv && inv->mask && inv->mask[0];
         inv = inv->next) {
      if (!u_equals_mask(global_invites, inv->mask) &&
          !u_equals_mask(chan->invites, inv->mask)) {
        i++;
        if (i == -j) {
          add_mode(chan, '-', 'I', inv->mask);
          dprintf(idx, "%s '%s' on %s.\n", IRC_REMOVEDINVITE,
                  inv->mask, chan->dname);
          putlog(LOG_CMDS, "*", "#%s# (%s) -invite %s [on channel]",
                 dcc[idx].nick, dcc[idx].u.chat->con_chan, invite);
          return;
        }
      }
    }
  } else {
    j = u_delinvite(chan, invite, 1);
    if (j > 0) {
      putlog(LOG_CMDS, "*", "#%s# (%s) -invite %s", dcc[idx].nick,
             dcc[idx].u.chat->con_chan, invite);
      dprintf(idx, "Removed %s channel invite: %s\n", chname, invite);
      add_mode(chan, '-', 'I', invite);
      return;
    }
    for (inv = chan->channel.invite; inv && inv->mask && inv->mask[0];
         inv = inv->next) {
      if (!rfc_casecmp(inv->mask, invite)) {
        add_mode(chan, '-', 'I', inv->mask);
        dprintf(idx, "%s '%s' on %s.\n",
                IRC_REMOVEDINVITE, inv->mask, chan->dname);
        putlog(LOG_CMDS, "*", "#%s# (%s) -invite %s [on channel]",
               dcc[idx].nick, dcc[idx].u.chat->con_chan, invite);
        return;
      }
    }
  }
  dprintf(idx, "No such invite.\n");
}

static void cmd_bans(struct userrec *u, int idx, char *par)
{
  if (!egg_strcasecmp(par, "all")) {
    putlog(LOG_CMDS, "*", "#%s# bans all", dcc[idx].nick);
    tell_bans(idx, 1, "");
  } else {
    putlog(LOG_CMDS, "*", "#%s# bans %s", dcc[idx].nick, par);
    tell_bans(idx, 0, par);
  }
}

static void cmd_exempts(struct userrec *u, int idx, char *par)
{
  if (!use_exempts) {
    dprintf(idx, "This command can only be used with use-exempts enabled.\n");
    return;
  }
  if (!egg_strcasecmp(par, "all")) {
    putlog(LOG_CMDS, "*", "#%s# exempts all", dcc[idx].nick);
    tell_exempts(idx, 1, "");
  } else {
    putlog(LOG_CMDS, "*", "#%s# exempts %s", dcc[idx].nick, par);
    tell_exempts(idx, 0, par);
  }
}

static void cmd_invites(struct userrec *u, int idx, char *par)
{
  if (!use_invites) {
    dprintf(idx, "This command can only be used with use-invites enabled.\n");
    return;
  }
  if (!egg_strcasecmp(par, "all")) {
    putlog(LOG_CMDS, "*", "#%s# invites all", dcc[idx].nick);
    tell_invites(idx, 1, "");
  } else {
    putlog(LOG_CMDS, "*", "#%s# invites %s", dcc[idx].nick, par);
    tell_invites(idx, 0, par);
  }
}

static void cmd_info(struct userrec *u, int idx, char *par)
{
  char s[512], *chname, *s1;
  int locked = 0;

  if (!use_info) {
    dprintf(idx, "Info storage is turned off.\n");
    return;
  }
  s1 = get_user(&USERENTRY_INFO, u);
  if (s1 && s1[0] == '@')
    locked = 1;
  if (par[0] && strchr(CHANMETA, par[0])) {
    chname = newsplit(&par);
    if (!findchan_by_dname(chname)) {
      dprintf(idx, "No such channel.\n");
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
	dprintf(idx, "Info on %s: %s\n", chname, s1);
	dprintf(idx, "Use 'info %s none' to remove it.\n", chname);
      } else {
	dprintf(idx, "Default info: %s\n", s1);
	dprintf(idx, "Use 'info none' to remove it.\n");
      }
    } else
      dprintf(idx, "No info has been set for you.\n");
    putlog(LOG_CMDS, "*", "#%s# info %s", dcc[idx].nick, chname ? chname : "");
    return;
  }
  if (locked && !(u && (u->flags & USER_MASTER))) {
    dprintf(idx, "Your info line is locked.  Sorry.\n");
    return;
  }
  if (!egg_strcasecmp(par, "none")) {
    if (chname) {
      par[0] = 0;
      set_handle_chaninfo(userlist, dcc[idx].nick, chname, NULL);
      dprintf(idx, "Removed your info line on %s.\n", chname);
      putlog(LOG_CMDS, "*", "#%s# info %s none", dcc[idx].nick, chname);
    } else {
      set_user(&USERENTRY_INFO, u, NULL);
      dprintf(idx, "Removed your default info line.\n");
      putlog(LOG_CMDS, "*", "#%s# info none", dcc[idx].nick);
    }
    return;
  }
/*  if (par[0] == '@')    This is stupid, and prevents a users info from being locked */
/*    par++;              without .tcl, or a tcl script, aka, 'half-assed' -poptix 4Jun01 */
  if (chname) {
    set_handle_chaninfo(userlist, dcc[idx].nick, chname, par);
    dprintf(idx, "Your info on %s is now: %s\n", chname, par);
    putlog(LOG_CMDS, "*", "#%s# info %s ...", dcc[idx].nick, chname);
  } else {
    set_user(&USERENTRY_INFO, u, par);
    dprintf(idx, "Your default info is now: %s\n", par);
    putlog(LOG_CMDS, "*", "#%s# info ...", dcc[idx].nick);
  }
}

static void cmd_chinfo(struct userrec *u, int idx, char *par)
{
  char *handle, *chname;
  struct userrec *u1;

  if (!use_info) {
    dprintf(idx, "Info storage is turned off.\n");
    return;
  }
  handle = newsplit(&par);
  if (!handle[0]) {
    dprintf(idx, "Usage: chinfo <handle> [channel] <new-info>\n");
    return;
  }
  u1 = get_user_by_handle(userlist, handle);
  if (!u1) {
    dprintf(idx, "No such user.\n");
    return;
  }
  if (par[0] && strchr(CHANMETA, par[0])) {
    chname = newsplit(&par);
    if (!findchan_by_dname(chname)) {
      dprintf(idx, "No such channel.\n");
      return;
    }
  } else
    chname = 0;
  if ((u1->flags & USER_BOT) && !(u->flags & USER_MASTER)) {
    dprintf(idx, "You have to be master to change bots info.\n");
    return;
  }
  if ((u1->flags & USER_OWNER) && !(u->flags & USER_OWNER)) {
    dprintf(idx, "You can't change info for the bot owner.\n");
    return;
  }
  if (chname) {
    get_user_flagrec(u, &user, chname);
    get_user_flagrec(u1, &victim, chname);
    if ((chan_owner(victim) || glob_owner(victim)) &&
	!(glob_owner(user) || chan_owner(user))) {
      dprintf(idx, "You can't change info for the channel owner.\n");
      return;
    }
  }
  putlog(LOG_CMDS, "*", "#%s# chinfo %s %s %s", dcc[idx].nick, handle,
	 chname ? chname : par, chname ? par : "");
  if (!egg_strcasecmp(par, "none"))
    par[0] = 0;
  if (chname) {
    set_handle_chaninfo(userlist, handle, chname, par);
    if (par[0] == '@')
      dprintf(idx, "New info (LOCKED) for %s on %s: %s\n", handle, chname,
	      &par[1]);
    else if (par[0])
      dprintf(idx, "New info for %s on %s: %s\n", handle, chname, par);
    else
      dprintf(idx, "Wiped info for %s on %s\n", handle, chname);
  } else {
    set_user(&USERENTRY_INFO, u1, par[0] ? par : NULL);
    if (par[0] == '@')
      dprintf(idx, "New default info (LOCKED) for %s: %s\n", handle, &par[1]);
    else if (par[0])
      dprintf(idx, "New default info for %s: %s\n", handle, par);
    else
      dprintf(idx, "Wiped default info for %s\n", handle);
  }
}

static void cmd_slowjoin(struct userrec *u, int idx, char *par)
{
  int intvl = 0, delay = 0, count = 1;
  char *chname;
  char buf[2048], buf2[1048];
  struct chanset_t *chan;
  tand_t *bot;
  char *p;

  /* slowjoin #chan 60 */
  putlog(LOG_CMDS, "*", "#%s# slowjoin %s", dcc[idx].nick, par);
  chname = newsplit(&par);
  p = newsplit(&par);
  intvl = atoi(p);
  if (!chname[0] || !p[0]) {
    dprintf(idx, "Usage: slowjoin <channel> <interval-seconds> [channel options]\n");
    return;
  }
  if (intvl < 10) {
    dprintf(idx, "Interval must be at least 10 seconds\n");
    return;
  }
  if ((chan = findchan_by_dname(chname))) {
    dprintf(idx, "Already on %s\n", chan->dname);
    return;
  }
  if (!strchr(CHANMETA, chname[0])) {
    dprintf(idx, "Invalid channel name\n");
    return;
  }
  strcpy(buf, "+inactive ");
  if (par[0])
    strncat(buf, par, sizeof(buf));
  if (tcl_channel_add(0, chname, buf) == TCL_ERROR) {
    dprintf(idx, "Invalid channel.\n");
    return;
  }

  chan = findchan_by_dname(chname);
  if (!chan) {
    dprintf(idx, "Hmmm... Channel didn't get added. Weird *shrug*\n");
    return;
  }
  sprintf(buf2, "cjoin %s %s", chan->dname, buf);
  putallbots(buf2);
#ifdef HUB
  count=0;
#else /* !HUB */
  count=1;
#endif /* HUB */
  for (bot = tandbot; bot; bot = bot->next) {
    struct userrec *ubot;
    char tmp[100];
    
    ubot = get_user_by_handle(userlist, bot->bot);
    if (ubot) {
      /* Variation: 60 secs intvl should be 60 +/- 15 */
      if (bot_hublevel(ubot) < 999) {
	sprintf(tmp, "sj %s 0\n", chan->dname);
      } else {
	int v = (random() % (intvl / 2)) - (intvl / 4);
	delay += intvl;
	sprintf(tmp, "sj %s %i\n", chan->dname, delay + v);
	count++;
      }
      putbot(ubot->handle, tmp);
    }
  }
  dprintf(idx, "%i bots joining %s during the next %i seconds\n", count, chan->dname, delay);
  chan->status &= ~CHAN_INACTIVE;
#ifdef LEAF
  if (shouldjoin(chan)) 
    dprintf(DP_MODE, "JOIN %s %s\n", chan->name, chan->key_prot);
#endif /* LEAF */
}

static void cmd_slowpart(struct userrec *u, int idx, char *par)
{
  int intvl = 0, delay = 0, count = 1;
  char *chname;
  struct chanset_t *chan;
  tand_t *bot;
  char *p;

  /* slowpart #chan 60 */
  putlog(LOG_CMDS, "*", "#%s# slowpart %s", dcc[idx].nick, par);
  chname = newsplit(&par);
  p = newsplit(&par);
  intvl = atoi(p);
  if (!chname[0] || !p[0]) {
    dprintf(idx, "Usage: slowpart <channel> <interval-seconds>\n");
    return;
  }
  if (intvl < 10) {
    dprintf(idx, "Interval must be at least 10 seconds\n");
    return;
  }
  if (!(chan = findchan_by_dname(chname))) {
    dprintf(idx, "Not on %s\n", chan->dname);
    return;
  }
  remove_channel(chan);
#ifdef HUB
  write_userfile(-1);
#endif /* HUB */
  dprintf(idx, "Channel %s removed from the bot.\n", chname);
  dprintf(idx, "This includes any channel specific bans, invites, exemptions and user records that you set.\n");

  if (findchan_by_dname(chname)) {
    dprintf(idx, "Hmmm... Channel didn't get removed. Weird *shrug*\n");
    return;
  }
#ifdef HUB
  count=0;
#else /* !HUB */
  count=1;
#endif /* HUB */
  for (bot = tandbot; bot; bot = bot->next) {
    char tmp[100];
    struct userrec *ubot;

    ubot = get_user_by_handle(userlist, bot->bot);
      /* Variation: 60 secs intvl should be 60 +/- 15 */
      if (ubot) {
        if (bot_hublevel(ubot) < 999) {
  	  sprintf(tmp, "sp %s 0\n", chname);
        } else {
  	  int v = (random() % (intvl / 2)) - (intvl / 4);
  	  delay += intvl;
  	  sprintf(tmp, "sp %s %i\n", chname, delay + v);
  	  count++;
        }
        putbot(ubot->handle, tmp);
      }  
  }
  dprintf(idx, "%i bots parting %s during the next %i seconds\n", count, chname, delay);
#ifdef LEAF
  dprintf(DP_MODE, "PART %s\n", chname);
#endif /* LEAF */
}

static void cmd_stick_yn(int idx, char *par, int yn)
{
  int i = 0, j;
  struct chanset_t *chan, *achan;
  char *stick_type, s[UHOSTLEN], chname[81];
  module_entry *me;

  stick_type = newsplit(&par);
  strncpyz(s, newsplit(&par), sizeof s);
  strncpyz(chname, newsplit(&par), sizeof chname);
  if (egg_strcasecmp(stick_type, "exempt") &&
      egg_strcasecmp(stick_type, "invite") &&
      egg_strcasecmp(stick_type, "ban")) {
    strncpyz(chname, s, sizeof chname);
    strncpyz(s, stick_type, sizeof s);
  }
  if (!s[0]) {
    dprintf(idx, "Usage: %sstick [ban/exempt/invite] <hostmask or number> [channel]\n",
            yn ? "" : "un");
    return;
  }
  /* Now deal with exemptions */
  if (!egg_strcasecmp(stick_type, "exempt")) {
    if (!use_exempts) {
      dprintf(idx, "This command can only be used with use-exempts enabled.\n");
      return;
    }
    if (!chname[0]) {
      i = u_setsticky_exempt(NULL, s,
                             (dcc[idx].user->flags & USER_MASTER) ? yn : -1);
      if (i > 0) {
        putlog(LOG_CMDS, "*", "#%s# %sstick exempt %s",
               dcc[idx].nick, yn ? "" : "un", s);
        dprintf(idx, "%stuck exempt: %s\n", yn ? "S" : "Uns", s);
        return;
      }
      strncpyz(chname, dcc[idx].u.chat->con_chan, sizeof chname);
    }
    /* Channel-specific exempt? */
    if (!(chan = findchan_by_dname(chname))) {
      dprintf(idx, "No such channel.\n");
      return;
    }
    get_user_flagrec(dcc[idx].user, &user, chan->dname);
    if (private(user, chan, PRIV_OP)) {
      dprintf(idx, "No such channel.\n");
      return;
    }
    if (str_isdigit(s)) {
      /* substract the numer of global exempts to get the number of the channel exempt */
      j = u_setsticky_exempt(NULL, s, -1);
      if (j < 0)
        egg_snprintf(s, sizeof s, "%d", -j);
    }
    j = u_setsticky_exempt(chan, s, yn);
    if (j > 0) {
      putlog(LOG_CMDS, "*", "#%s# %sstick exempt %s %s", dcc[idx].nick,
             yn ? "" : "un", s, chname);
      dprintf(idx, "%stuck %s exempt: %s\n", yn ? "S" : "Uns", chname, s);
      return;
    }
    dprintf(idx, "No such exempt.\n");
    return;
  /* Now the invites */
  } else if (!egg_strcasecmp(stick_type, "invite")) {
    if (!use_invites) {
      dprintf(idx, "This command can only be used with use-invites enabled.\n");
      return;
    }
    if (!chname[0]) {
      i = u_setsticky_invite(NULL, s,
                             (dcc[idx].user->flags & USER_MASTER) ? yn : -1);
      if (i > 0) {
        putlog(LOG_CMDS, "*", "#%s# %sstick invite %s",
               dcc[idx].nick, yn ? "" : "un", s);
        dprintf(idx, "%stuck invite: %s\n", yn ? "S" : "Uns", s);
        return;
      }
      strncpyz(chname, dcc[idx].u.chat->con_chan, sizeof chname);
    }
    /* Channel-specific invite? */
    if (!(chan = findchan_by_dname(chname))) {
      dprintf(idx, "No such channel.\n");
      return;
    }
    get_user_flagrec(dcc[idx].user, &user, chan->dname);
    if (private(user, chan, PRIV_OP)) {
      dprintf(idx, "No such channel.\n");
      return;
    }
    if (str_isdigit(s)) {
      /* substract the numer of global invites to get the number of the channel invite */
      j = u_setsticky_invite(NULL, s, -1);
      if (j < 0)
        egg_snprintf(s, sizeof s, "%d", -j);
    }
    j = u_setsticky_invite(chan, s, yn);
    if (j > 0) {
      putlog(LOG_CMDS, "*", "#%s# %sstick invite %s %s", dcc[idx].nick,
             yn ? "" : "un", s, chname);
      dprintf(idx, "%stuck %s invite: %s\n", yn ? "S" : "Uns", chname, s);
      return;
    }
    dprintf(idx, "No such invite.\n");
    return;
  }
  if (!chname[0]) {
    i = u_setsticky_ban(NULL, s,
                        (dcc[idx].user->flags & USER_MASTER) ? yn : -1);
    if (i > 0) {
      putlog(LOG_CMDS, "*", "#%s# %sstick ban %s",
             dcc[idx].nick, yn ? "" : "un", s);
      dprintf(idx, "%stuck ban: %s\n", yn ? "S" : "Uns", s);
      if ((me = module_find("irc", 0, 0)))
	for (achan = chanset; achan != NULL; achan = achan->next)
	  (me->funcs[IRC_CHECK_THIS_BAN])(achan, s, yn);
      return;
    }
    strncpyz(chname, dcc[idx].u.chat->con_chan, sizeof chname);
  }
  /* Channel-specific ban? */
  if (!(chan = findchan_by_dname(chname))) {
    dprintf(idx, "No such channel.\n");
    return;
  }
  get_user_flagrec(dcc[idx].user, &user, chan->dname);
  if (private(user, chan, PRIV_OP)) {
    dprintf(idx, "No such channel.\n");
    return;
  }
  if (str_isdigit(s)) {
    /* substract the numer of global bans to get the number of the channel ban */
    j = u_setsticky_ban(NULL, s, -1);
    if (j < 0)
      egg_snprintf(s, sizeof s, "%d", -j);
  }
  j = u_setsticky_ban(chan, s, yn);
  if (j > 0) {
    putlog(LOG_CMDS, "*", "#%s# %sstick ban %s %s", dcc[idx].nick,
           yn ? "" : "un", s, chname);
    dprintf(idx, "%stuck %s ban: %s\n", yn ? "S" : "Uns", chname, s);
    if ((me = module_find("irc", 0, 0)))
      (me->funcs[IRC_CHECK_THIS_BAN])(chan, s, yn);
    return;
  }
  dprintf(idx, "No such ban.\n");
}


static void cmd_stick(struct userrec *u, int idx, char *par)
{
  cmd_stick_yn(idx, par, 1);
}

static void cmd_unstick(struct userrec *u, int idx, char *par)
{
  cmd_stick_yn(idx, par, 0);
}

static void cmd_pls_chrec(struct userrec *u, int idx, char *par)
{
  char *nick, *chn;
  struct chanset_t *chan;
  struct userrec *u1;
  struct chanuserrec *chanrec;

  if (!par[0]) {
    dprintf(idx, "Usage: +chrec <user> [channel]\n");
    return;
  }
  nick = newsplit(&par);
  u1 = get_user_by_handle(userlist, nick);
  if (!u1) {
    dprintf(idx, "No such user.\n");
    return;
  }
  if (!par[0])
    chan = findchan_by_dname(dcc[idx].u.chat->con_chan);
  else {
    chn = newsplit(&par);
    chan = findchan_by_dname(chn);
  }
  if (!chan) {
    dprintf(idx, "No such channel.\n");
    return;
  }
  get_user_flagrec(u, &user, chan->dname);
  get_user_flagrec(u1, &victim, chan->dname);
  if (private(user, chan, PRIV_OP)) {
    dprintf(idx, "No such channel.\n");
    return;
  }
  if ((!glob_master(user) && !chan_master(user)) ||  /* drummer */
      (chan_owner(victim) && !chan_owner(user) && !glob_owner(user)) ||
      (glob_owner(victim) && !glob_owner(user))) {
    dprintf(idx, "You have no permission to do that.\n");
    return;
  }
  chanrec = get_chanrec(u1, chan->dname);
  if (chanrec) {
    dprintf(idx, "User %s already has a channel record for %s.\n",
	    nick, chan->dname);
    return;
  }
  putlog(LOG_CMDS, "*", "#%s# +chrec %s %s", dcc[idx].nick,
	 nick, chan->dname);
  add_chanrec(u1, chan->dname);
  dprintf(idx, "Added %s channel record for %s.\n", chan->dname, nick);
}

static void cmd_mns_chrec(struct userrec *u, int idx, char *par)
{
  char *nick, *chn = NULL;
  struct userrec *u1;
  struct chanuserrec *chanrec;

  if (!par[0]) {
    dprintf(idx, "Usage: -chrec <user> [channel]\n");
    return;
  }
  nick = newsplit(&par);
  u1 = get_user_by_handle(userlist, nick);
  if (!u1) {
    dprintf(idx, "No such user.\n");
    return;
  }
  if (!par[0]) {
    struct chanset_t *chan;

    chan = findchan_by_dname(dcc[idx].u.chat->con_chan);
    if (chan)
      chn = chan->dname;
    else {
      dprintf(idx, "Invalid console channel.\n");
      return;
    }
  } else
    chn = newsplit(&par);
  get_user_flagrec(u, &user, chn);
  get_user_flagrec(u1, &victim, chn);
  if (private(user, findchan_by_dname(chn), PRIV_OP)) {
    dprintf(idx, "No such channel.\n");
    return;
  }
  if ((!glob_master(user) && !chan_master(user)) ||  /* drummer */
      (chan_owner(victim) && !chan_owner(user) && !glob_owner(user)) ||
      (glob_owner(victim) && !glob_owner(user))) {
    dprintf(idx, "You have no permission to do that.\n");
    return;
  }
  chanrec = get_chanrec(u1, chn);
  if (!chanrec) {
    dprintf(idx, "User %s doesn't have a channel record for %s.\n", nick, chn);
    return;
  }
  putlog(LOG_CMDS, "*", "#%s# -chrec %s %s", dcc[idx].nick, nick, chn);
  del_chanrec(u1, chn);
  dprintf(idx, "Removed %s channel record from %s.\n", chn, nick);
}

static void cmd_cycle(struct userrec *u, int idx, char *par)
{
  char *chname;
  char buf2[1024];
  int delay = 10;
  struct chanset_t *chan;

  putlog(LOG_CMDS, "*", "#%s# cycle %s", dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, "Usage: cycle [%s]<channel> [delay]\n", CHANMETA);
    dprintf(idx, "rejoin delay defaults to '10'\n");
    return;
  }

  chname = newsplit(&par);
  chan = findchan_by_dname(chname);
  if (!chan) {
    dprintf(idx, "%s it not a valid channel.\n", chname);
    return;
  }
  if (par[0])
    delay = atoi(newsplit(&par));

  sprintf(buf2, "cycle %s %d", chname, delay); //this just makes the bot PART
  putallbots(buf2);
#ifdef LEAF
  do_chanset(chan, "+inactive", 2);
  dprintf(DP_SERVER, "PART %s\n", chan->name);
  chan->channel.jointime = ((now + delay) - server_lag);
#endif /* LEAF */
}

static void cmd_down(struct userrec *u, int idx, char *par)
{
  char *chname;
  char buf2[1024];
  struct chanset_t *chan;

  putlog(LOG_CMDS, "*", "#%s# down %s", dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, "Usage: down [%s]<channel>\n", CHANMETA);
    return;
  }

  chname = newsplit(&par);
  chan = findchan_by_dname(chname);
  if (!chan) {
    dprintf(idx, "%s it not a valid channel.\n", chname);
    return;
  }
  
  sprintf(buf2, "down %s", chan->dname);
  putallbots(buf2);
#ifdef LEAF
  add_mode(chan, '-', 'o', botname);
  chan->channel.no_op = (now + 10);
#endif /* LEAF */
  
}

static void cmd_pls_chan(struct userrec *u, int idx, char *par)
{
  char *chname;
  char buf2[1024];
  struct chanset_t *chan;

  putlog(LOG_CMDS, "*", "#%s# +chan %s", dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, "Usage: +chan [%s]<channel> [options]\n", CHANMETA);
    return;
  }

  chname = newsplit(&par);
  sprintf(buf2, "cjoin %s %s", chname, par);
  putallbots(buf2);
  if (findchan_by_dname(chname)) {
    dprintf(idx, "That channel already exists!\n");
    return;
  } else if ((chan = findchan(chname))) {
    /* This prevents someone adding a channel by it's unique server
     * name <cybah>
     */
    dprintf(idx, "That channel already exists as %s!\n", chan->dname);
    return;
  }

  if (tcl_channel_add(0, chname, par) == TCL_ERROR) /* drummer */
    dprintf(idx, "Invalid channel or channel options.\n");
  else {
#ifdef HUB
    write_userfile(-1);
#endif /* HUB */
  }
}

static void cmd_mns_chan(struct userrec *u, int idx, char *par)
{
  char *chname;
  char buf2[1024];
  struct chanset_t *chan;
  int i;

  putlog(LOG_CMDS, "*", "#%s# -chan %s", dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, "Usage: -chan [%s]<channel>\n", CHANMETA);
    return;
  }
  chname = newsplit(&par);

  sprintf(buf2, "cpart %s", chname);
  putallbots(buf2);

  chan = findchan_by_dname(chname);
  if (!chan) {
    if ((chan = findchan(chname)))
      dprintf(idx, "That channel exists with a short name of %s, use that.\n",
              chan->dname);
    else
      dprintf(idx, "That channel doesn't exist!\n");
    return;
  }

  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type->flags & DCT_CHAT) &&
	!rfc_casecmp(dcc[i].u.chat->con_chan, chan->dname)) {
      dprintf(i, "%s is no longer a valid channel, changing your console to '*'\n",
	      chname);
      strcpy(dcc[i].u.chat->con_chan, "*");
    }
  remove_channel(chan);
#ifdef HUB
  write_userfile(-1);
#endif /* HUB */
  dprintf(idx, "Channel %s removed from the bot.\n", chname);
  dprintf(idx, "This includes any channel specific bans, invites, exemptions and user records that you set.\n");
}

/* thanks Excelsior */
void modesetting(int idx, char *work, int *cnt, char *name, int state)
{
  char tmp[100];
  if (((*cnt) < 3) && (!name || (name && !name[0]))) (*cnt) = 3; 		/* empty buffer if no (char *) name */
  (*cnt)++;
  if (*cnt > 4) {
    *cnt = 1;
    work[0] = 0;
  }
  if (!work[0])
    sprintf(work, "  ");
  if (name && name[0]) {
    egg_snprintf(tmp, sizeof tmp, "%s%-17s", state ? "\002+\002" : "\002-\002", name);
    strcat(work, tmp);
  }
  if (*cnt >= 4)
    dprintf(idx, "%s\n", work);
}


#define MSET(name, state) modesetting(idx, work, &cnt, name, state)
static void cmd_chaninfo(struct userrec *u, int idx, char *par)
{
  char *chname, work[512];
  struct chanset_t *chan;
  int ii, tmp, cnt = 0;
  struct udef_struct *ul;

  if (!par[0]) {
    chname = dcc[idx].u.chat->con_chan;
    if (chname[0] == '*') {
      dprintf(idx, "Your console channel is invalid.\n");
      return;
    }
  } else {
    chname = newsplit(&par);
    get_user_flagrec(u, &user, chname);
    if (!glob_master(user) && !chan_master(user)) {
      dprintf(idx, "You don't have access to %s.\n", chname);
      return;
    }
  }
  if (!(chan = findchan_by_dname(chname)))
    dprintf(idx, "No such channel defined.\n");
  else {
    dprintf(idx, "Settings for dynamic channel %s:\n", chan->dname);
    get_mode_protect(chan, work);
    dprintf(idx, "Protect modes (chanmode): %s\n", work[0] ? work : "None");
/* Chanchar template
 *  dprintf(idx, "String temp: %s\n", chan->temp[0] ? chan->temp : "NULL");
 */
    if (chan->idle_kick)
      dprintf(idx, "Idle Kick after (idle-kick): %d\n", chan->idle_kick);
    else
      dprintf(idx, "Idle Kick after (idle-kick): DON'T!\n");
    if (chan->limitraise)
      dprintf(idx, "Limit raise: %d\n", chan->limitraise);
    else
      dprintf(idx, "Limit raise: disabled\n");
    if (chan->stopnethack_mode)
      dprintf(idx, "stopnethack-mode: %d\n", chan->stopnethack_mode);
    else
      dprintf(idx, "stopnethack: DON'T!\n");
    if (chan->revenge_mode)
      dprintf(idx, "revenge-mode: %d\n", chan->revenge_mode);
    else
      dprintf(idx, "revenge-mode: 0\n");
    if (chan->closed_ban)
      dprintf(idx, "closed-ban: %d\n", chan->closed_ban);
    else
      dprintf(idx, "closed-ban: 0\n");
/* Chanint template
 *  if (chan->temp)
 *   dprintf(idx, "temp: %d\n", chan->temp);
 * else
 *   dprintf(idx, temp: 0\n");
 */
    if (chan->ban_time)
      dprintf(idx, "ban-time: %d\n", chan->ban_time);
    else
      dprintf(idx, "ban-time: 0\n");
    if (chan->exempt_time)
      dprintf(idx, "exempt-time: %d\n", chan->exempt_time);
    else
      dprintf(idx, "exempt-time: 0\n");
    if (chan->invite_time)
      dprintf(idx, "invite-time: %d\n", chan->invite_time);
    else
      dprintf(idx, "invite-time: 0\n");
    dprintf(idx, "Other modes:\n");
    work[0] = 0;
    MSET("bitch",		channel_bitch(chan));
    MSET("closed",		channel_closed(chan));
    MSET("cycle",		channel_cycle(chan));
    MSET("enforcebans", 	channel_enforcebans(chan));
    MSET("fastop",		channel_fastop(chan));
    MSET("inactive",		channel_inactive(chan));
    MSET("manop",		channel_manop(chan));
    MSET("nodesynch",		channel_nodesynch(chan));
    MSET("nomop",		channel_nomop(chan));
    MSET("private",		channel_private(chan));
    MSET("protectops",		channel_protectops(chan));
    MSET("revenge",		channel_revenge(chan));
    MSET("revengebot",		channel_revengebot(chan));
    MSET("take",		channel_take(chan));
    MSET("voice",		channel_voice(chan));
    MSET("", 0);
    MSET("dynamicbans",		channel_dynamicbans(chan));
    MSET("userbans",		!channel_nouserbans(chan));
    MSET("dynamicexempts",	channel_dynamicexempts(chan));
    MSET("userexempts",		!channel_nouserexempts(chan));
    MSET("dynamicinvites",	channel_dynamicinvites(chan));
    MSET("userinvites",		!channel_nouserinvites(chan));
    MSET("", 0);
    work[0] = 0;

/* Chanflag template
 *          (chan->status & CHAN_TEMP) ? '+' : '-',
 * also include %ctemp in dprintf.
 */


    ii = 1;
    tmp = 0;
    for (ul = udef; ul; ul = ul->next)
      if (ul->defined && ul->type == UDEF_FLAG) {
	int	work_len;

        if (!tmp) {
          dprintf(idx, "User defined channel flags:\n");
          tmp = 1;
        }
	if (ii == 1)
	  egg_snprintf(work, sizeof work, "    ");
	work_len = strlen(work);
        egg_snprintf(work + work_len, sizeof(work) - work_len, " %c%s",
		     getudef(ul->values, chan->dname) ? '+' : '-', ul->name);
        ii++;
        if (ii > 4) {
          dprintf(idx, "%s\n", work);
          ii = 1;
        }
      }
    if (ii > 1)
      dprintf(idx, "%s\n", work);

    work[0] = 0;
    ii = 1;
    tmp = 0;
    for (ul = udef; ul; ul = ul->next)
      if (ul->defined && ul->type == UDEF_INT) {
	int	work_len = strlen(work);

        if (!tmp) {
          dprintf(idx, "User defined channel settings:\n");
          tmp = 1;
        }
        egg_snprintf(work + work_len, sizeof(work) - work_len, "%s: %d   ",
		     ul->name, getudef(ul->values, chan->dname));
        ii++;
        if (ii > 4) {
          dprintf(idx, "%s\n", work);
	  work[0] = 0;
          ii = 1;
        }
      }
    if (ii > 1)
      dprintf(idx, "%s\n", work);

    dprintf(idx, "flood settings: chan ctcp join kick deop nick\n");
    dprintf(idx, "number:          %3d  %3d  %3d  %3d  %3d  %3d\n",
	    chan->flood_pub_thr, chan->flood_ctcp_thr,
	    chan->flood_join_thr, chan->flood_kick_thr,
	    chan->flood_deop_thr, chan->flood_nick_thr);
    dprintf(idx, "time  :          %3d  %3d  %3d  %3d  %3d  %3d\n",
	    chan->flood_pub_time, chan->flood_ctcp_time,
	    chan->flood_join_time, chan->flood_kick_time,
	    chan->flood_deop_time, chan->flood_nick_time);
    putlog(LOG_CMDS, "*", "#%s# chaninfo %s", dcc[idx].nick, chname);
  }
}

static void cmd_chanset(struct userrec *u, int idx, char *par)
{
  char *chname = NULL, answers[512], *parcpy;
  char *list[2], *bak, *buf;
  struct chanset_t *chan = NULL;
  int all = 0;

  putlog(LOG_CMDS, "*", "#%s# chanset %s", dcc[idx].nick, par);

  if (!par[0])
    dprintf(idx, "Usage: chanset [%schannel|*] <settings>\n", CHANMETA);
  else {
    if (strlen(par) > 2 && par[0] == '*' && par[1] == ' ') {
      all = 1;
      get_user_flagrec(u, &user, chanset ? chanset->dname : "");
      if (!glob_master(user)) {
	dprintf(idx, "You need to be a global master to use %schanset *.\n", dcc_prefix);
	return;
      }
      newsplit(&par);
    } else {
      if (strchr(CHANMETA, par[0])) {
        chname = newsplit(&par);
        get_user_flagrec(u, &user, chname);
        if (!glob_master(user) && !chan_master(user)) {
	  dprintf(idx, "You don't have access to %s. \n", chname);
	  return;
	} else if (!(chan = findchan_by_dname(chname)) && (chname[0] != '+')) {
	  dprintf(idx, "That channel doesn't exist!\n");
	  return;
	} else if ((strstr(par, "+private") || strstr(par, "-private")) && (!glob_owner(user))) {
	  dprintf(idx, "You don't have access to set +/-private on %s (halting command due to lazy coder).\n", chname);
	  return;
        }
	if (!chan) {
	  if (par[0])
	    *--par = ' ';
	  par = chname;
	}
      }
      if (!par[0] || par[0] == '*') {
        dprintf(idx, "Usage: chanset [%schannel] <settings>\n", CHANMETA);
        return;
      }
      if (!chan &&
          !(chan = findchan_by_dname(chname = dcc[idx].u.chat->con_chan))) {
        dprintf(idx, "Invalid console channel.\n");
        return;
      }
    }
    if (all)
      chan = chanset;
    bak = par;
    buf = nmalloc(strlen(par) + 1);
    while (chan) {
      chname = chan->dname;
      strcpy(buf, bak);
      par = buf;
      list[0] = newsplit(&par);
      answers[0] = 0;
      while (list[0][0]) {
	if (list[0][0] == '+' || list[0][0] == '-' ||
	    (!strcmp(list[0], "dont-idle-kick"))) {
	  if (tcl_channel_modify(0, chan, 1, list) == TCL_OK) {
	    strcat(answers, list[0]);
	    strcat(answers, " ");
	  } else if (!all || !chan->next)
	    dprintf(idx, "Error trying to set %s for %s, invalid mode.\n",
		    list[0], all ? "all channels" : chname);
	  list[0] = newsplit(&par);
	  continue;
	}
	/* The rest have an unknown amount of args, so assume the rest of the
	 * line is args. Woops nearly made a nasty little hole here :) we'll
	 * just ignore any non global +n's trying to set the need-commands.
	 */
	/* chanints */
	list[1] = par;
	/* Par gets modified in tcl channel_modify under some
  	 * circumstances, so save it now.
	 */
	parcpy = nmalloc(strlen(par) + 1);
	strcpy(parcpy, par);
        if (tcl_channel_modify(0, chan, 2, list) == TCL_OK) {
	  strcat(answers, list[0]);
	  strcat(answers, " { ");
	  strcat(answers, parcpy);
	  strcat(answers, " }");
	} else if (!all || !chan->next)
	  dprintf(idx, "Error trying to set %s for %s, invalid option\n", list[0], all ? "all channels" : chname);
        nfree(parcpy);
	break;
      }
      if (!all && answers[0]) {
        struct chanset_t *my_chan;
        if ((my_chan = findchan_by_dname(chname)))
          do_chanset(my_chan, bak, 0);
	dprintf(idx, "Successfully set modes { %s } on %s.\n", answers, chname);
      }
      if (!all)
        chan = NULL;
      else
        chan = chan->next;
    }
    if (all && answers[0]) {
      do_chanset(NULL, bak, 0);		/* NULL does all */
      dprintf(idx, "Successfully set modes { %s } on all channels.\n", answers);
    }
    nfree(buf);
  }
}

/* DCC CHAT COMMANDS
 *
 * Function call should be:
 *    int cmd_whatever(idx,"parameters");
 *
 * NOTE: As with msg commands, the function is responsible for any logging.
 */
static cmd_t C_dcc_irc[] =
{
  {"+ban",	"o|o",	(Function) cmd_pls_ban,		NULL},
  {"+exempt",	"o|o",	(Function) cmd_pls_exempt,	NULL},
  {"+invite",	"o|o",	(Function) cmd_pls_invite,	NULL},
  {"+chan",	"n",	(Function) cmd_pls_chan,	NULL},
  {"+chrec",	"m|m",	(Function) cmd_pls_chrec,	NULL},
  {"-ban",	"o|o",	(Function) cmd_mns_ban,		NULL},
  {"-chan",	"n",	(Function) cmd_mns_chan,	NULL},
  {"-chrec",	"m|m",	(Function) cmd_mns_chrec,	NULL},
  {"-exempt",	"o|o",	(Function) cmd_mns_exempt,	NULL},
  {"-invite",	"o|o",	(Function) cmd_mns_invite,	NULL},
  {"bans",	"o|o",	(Function) cmd_bans,		NULL},
  {"exempts",	"o|o",	(Function) cmd_exempts,		NULL},
  {"invites",	"o|o",	(Function) cmd_invites,		NULL},
  {"chaninfo",	"m|m",	(Function) cmd_chaninfo,	NULL},
  {"chanset",	"m|m",	(Function) cmd_chanset,		NULL},
  {"chinfo",	"m|m",	(Function) cmd_chinfo,		NULL},
  {"cycle", 	"n|n",	(Function) cmd_cycle,		NULL},
  {"down",	"n|n",	(Function) cmd_down,		NULL},
  {"info",	"",	(Function) cmd_info,		NULL},
  {"slowjoin",  "n",    (Function) cmd_slowjoin,        NULL},
  {"slowpart",  "n|n",  (Function) cmd_slowpart,        NULL},
  {"stick",	"o|o",	(Function) cmd_stick,		NULL},
  {"unstick",	"o|o",	(Function) cmd_unstick,		NULL},
  {NULL,	NULL,	NULL,				NULL}
};
