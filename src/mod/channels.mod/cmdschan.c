/*
 * cmdschan.c -- part of channels.mod
 *   commands from a user via dcc that cause server interaction
 *
 */

#include <ctype.h>

static struct flag_record user	 = {FR_GLOBAL | FR_CHAN, 0, 0};
static struct flag_record victim = {FR_GLOBAL | FR_CHAN, 0, 0};


static void cmd_pls_mask(char type, struct userrec *u, int idx, char *par)
{
  char *chname = NULL, *who = NULL, s[UHOSTLEN] = "", s1[UHOSTLEN] = "", *p = NULL, *p_expire = NULL, *cmd = NULL;
  unsigned long int expire_time = 0, expire_foo;
  int sticky = 0;
  struct chanset_t *chan = NULL;

  cmd = type == 'b' ? "ban" : type == 'e' ? "exempt" : "invite";
  if (!par[0]) {
    dprintf(idx, "Usage: +%s <hostmask> [channel] [%%<XdXhXm>] [reason]\n", cmd);
    return;
  }
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
    if (!chan || (chan && private(user, chan, PRIV_OP))) {
      dprintf(idx, "No such channel.\n");
      return;
    } else if (!chk_op(user, chan)) {
      dprintf(idx, "You don't have access to set %ss on %s.\n", cmd, chname);
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
#ifdef LEAF
    egg_snprintf(s1, sizeof s1, "%s!%s", botname, botuserhost);
#else
    egg_snprintf(s1, sizeof s1, "%s!%s@%s", origbotname, botuser, conf.bot->host);
#endif /* LEAF */
  if (type == 'b' && s1[0] && wild_match(s, s1)) {
    dprintf(idx, "I'm not going to ban myself.\n");
    putlog(LOG_CMDS, "*", "#%s# attempted +ban %s", dcc[idx].nick, s);
    return;
  }
  /* IRC can't understand bans longer than 70 characters */
  if (strlen(s) > 70) {
    s[69] = '*';
    s[70] = 0;
  }
  if (chan) {
    u_addmask(type, chan, s, dcc[idx].nick, par, expire_time ? now + expire_time : 0, 0);
    if (par[0] == '*') {
      sticky = 1;
      par++;
      putlog(LOG_CMDS, "*", "#%s# (%s) +%s %s %s (%s) (sticky)",
	     dcc[idx].nick, dcc[idx].u.chat->con_chan, cmd, s, chan->dname, par);
      dprintf(idx, "New %s sticky %s: %s (%s)\n", chan->dname, cmd, s, par);
    } else {
      putlog(LOG_CMDS, "*", "#%s# (%s) +%s %s %s (%s)", dcc[idx].nick,
	     dcc[idx].u.chat->con_chan, cmd, s, chan->dname, par);
      dprintf(idx, "New %s %s: %s (%s)\n", chan->dname, cmd, s, par);
    }
#ifdef LEAF
    if (type == 'e' || type == 'I')
      add_mode(chan, '+', type, s);
    /* Avoid unnesessary modes if you got +dynamicbans
     */
    else
      check_this_ban(chan, s, sticky);
#endif /* LEAF */
  } else {
    u_addmask(type, NULL, s, dcc[idx].nick, par, expire_time ? now + expire_time : 0, 0);
    if (par[0] == '*') {
      sticky = 1;
      par++;
      putlog(LOG_CMDS, "*", "#%s# (GLOBAL) +%s %s (%s) (sticky)", dcc[idx].nick, cmd, s, par);
      dprintf(idx, "New sticky %s: %s (%s)\n", cmd, s, par);
    } else {
      putlog(LOG_CMDS, "*", "#%s# (GLOBAL) +%s %s (%s)", dcc[idx].nick, cmd, s, par);
      dprintf(idx, "New %s: %s (%s)\n", cmd, s, par);
    }
#ifdef LEAF
    for (chan = chanset; chan != NULL; chan = chan->next) {
      if (type == 'b')
        check_this_ban(chan, s, sticky);
      else
        add_mode(chan, '+', type, s);
    }
#endif /* LEAF */
  }
}

static void cmd_pls_ban(struct userrec *u, int idx, char *par)
{
  cmd_pls_mask('b', u, idx, par);
}

static void cmd_pls_exempt(struct userrec *u, int idx, char *par)
{ 
  if (!use_exempts) {
    dprintf(idx, "This command can only be used with use-exempts enabled.\n");
    return;
  }
  cmd_pls_mask('e', u, idx, par);
}

static void cmd_pls_invite(struct userrec *u, int idx, char *par)
{
  if (!use_invites) {
    dprintf(idx, "This command can only be used with use-invites enabled.\n");
    return;
  }
  cmd_pls_mask('I', u, idx, par);
}

static void cmd_mns_mask(char type, struct userrec *u, int idx, char *par)
{
  int i = 0, j;
  struct chanset_t *chan = NULL;
  char s[UHOSTLEN] = "", *who = NULL, *chname = NULL, *cmd = NULL, *mask = NULL;
  masklist *m = NULL;

  cmd = type == 'b' ? "ban" : type == 'e' ? "exempt" : "invite";
  if (!par[0]) {
    dprintf(idx, "Usage: -%s <hostmask> [channel]\n", cmd);
    return;
  }
  who = newsplit(&par);
  if (par[0] && strchr(CHANMETA, par[0]))
    chname = newsplit(&par);
  else
    chname = dcc[idx].u.chat->con_chan;
  if (chname || !(u->flags & USER_MASTER)) {
    if (!chname)
      chname = dcc[idx].u.chat->con_chan;
    get_user_flagrec(u, &user, chname);

    if (strchr(CHANMETA, chname[0]) && private(user, findchan_by_dname(chname), PRIV_OP)) {
      dprintf(idx, "No such channel.\n");
      return;
    }
    if (!chk_op(user, findchan_by_dname(chname)))
      return;
  }
  strncpyz(s, who, sizeof s);
  i = u_delmask(type, NULL, s, (u->flags & USER_MASTER));
  if (i > 0) {
    if (lastdeletedmask)
      mask = lastdeletedmask;
    else
      mask = s;
    putlog(LOG_CMDS, "*", "#%s# -%s %s", dcc[idx].nick, cmd, mask);
    dprintf(idx, "%s %s: %s\n", "Removed", cmd, s);
#ifdef LEAF
    for (chan = chanset; chan != NULL; chan = chan->next)
      add_mode(chan, '-', type, mask);
#endif /* LEAF */
    return;
  }
  /* Channel-specific ban? */
  if (chname)
    chan = findchan_by_dname(chname);
  if (chan) {
    m = type == 'b' ? chan->channel.ban : type == 'e' ? chan->channel.exempt : chan->channel.invite;
    if ((i = atoi(who)) > 0) {
      egg_snprintf(s, sizeof s, "%d", i);
      j = u_delmask(type, chan, s, 1);
      if (j > 0) {
        if (lastdeletedmask)
          mask = lastdeletedmask;
        else
          mask = s;
	putlog(LOG_CMDS, "*", "#%s# (%s) -%s %s", dcc[idx].nick, chan->dname, cmd, mask);
	dprintf(idx, "Removed %1$s channel %2$s: %3$s\n", chan->dname, cmd, mask);
#ifdef LEAF
	add_mode(chan, '-', type, mask);
#endif  /* LEAF */
	return;
      }
      i = 0;
      for (; m && m->mask && m->mask[0]; m = m->next) {
	if ((!u_equals_mask(type == 'b' ? global_bans : type == 'e' ? global_exempts :
	      global_invites, m->mask)) &&
	    (!u_equals_mask(type == 'b' ? chan->bans : type == 'e' ? chan->exempts :
	      chan->invites, m->mask))) {
	  i++;
	  if (i == -j) {
#ifdef LEAF
	    add_mode(chan, '-', type, m->mask);
#endif  /* LEAF */
	    dprintf(idx, "%s %s '%s' on %s.\n", "Removed", cmd, m->mask, chan->dname);
	    putlog(LOG_CMDS, "*", "#%s# (%s) -%s %s [on channel]", dcc[idx].nick, dcc[idx].u.chat->con_chan, cmd, who);
	    return;
	  }
	}
      }
    } else {
      j = u_delmask(type, chan, who, 1);
      if (j > 0) {
	putlog(LOG_CMDS, "*", "#%s# (%s) -%s %s", dcc[idx].nick, dcc[idx].u.chat->con_chan, cmd, who);
	dprintf(idx, "Removed %1$s channel %2$s: %3$s\n", chname, cmd, who);
#ifdef LEAF
	add_mode(chan, '-', type, who);
#endif  /* LEAF */
	return;
      }
      for (; m && m->mask && m->mask[0]; m = m->next) {
	if (!rfc_casecmp(m->mask, who)) {
#ifdef LEAF
	  add_mode(chan, '-', type, m->mask);
#endif  /* LEAF */
	  dprintf(idx, "%s %s '%s' on %s.\n", "Removed", cmd, m->mask, chan->dname);
	  putlog(LOG_CMDS, "*", "#%s# (%s) -%s %s [on channel]", dcc[idx].nick, dcc[idx].u.chat->con_chan, cmd, who);
	  return;
	}
      }
    }
  }
  dprintf(idx, "No such %1$s.\n", cmd);
}

static void cmd_mns_ban(struct userrec *u, int idx, char *par)
{
  cmd_mns_mask('b', u, idx, par);
}

static void cmd_mns_exempt(struct userrec *u, int idx, char *par)
{
  if (!use_exempts) {
    dprintf(idx, "This command can only be used with use-exempts enabled.\n");
    return;
  }
  cmd_mns_mask('e', u, idx, par);
}

static void cmd_mns_invite(struct userrec *u, int idx, char *par)
{
  if (!use_invites) {
    dprintf(idx, "This command can only be used with use-invites enabled.\n");
    return;
  }
  cmd_mns_mask('I', u, idx, par);
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
  char s[512] = "", *chname = NULL, *s1 = NULL;
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
  char *handle = NULL, *chname = NULL;
  struct userrec *u1 = NULL;

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
  if (u1->bot && !(u->flags & USER_MASTER)) {
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
  char *chname = NULL, *p = NULL, buf[2048] = "", buf2[1048] = "";
  struct chanset_t *chan = NULL;
  tand_t *bot = NULL;

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
  if (channel_add(NULL, chname, buf) == ERROR) {
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
    struct userrec *ubot = NULL;
    char tmp[100] = "";
    
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
  char *chname = NULL, *p = NULL;
  struct chanset_t *chan = NULL;
  tand_t *bot = NULL;

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
  count = 0;
#else /* !HUB */
  count = 1;
#endif /* HUB */
  for (bot = tandbot; bot; bot = bot->next) {
    char tmp[100] = "";
    struct userrec *ubot = NULL;

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
  struct chanset_t *chan = NULL;
  char *stick_type = NULL, s[UHOSTLEN] = "", chname[81] = "";

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
    dprintf(idx, "Usage: %sstick [ban/exempt/invite] <hostmask or number> [channel]\n", yn ? "" : "un");
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
      putlog(LOG_CMDS, "*", "#%s# %sstick invite %s %s", dcc[idx].nick, yn ? "" : "un", s, chname);
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
      putlog(LOG_CMDS, "*", "#%s# %sstick ban %s", dcc[idx].nick, yn ? "" : "un", s);
      dprintf(idx, "%stuck ban: %s\n", yn ? "S" : "Uns", s);
#ifdef LEAF
    {
      struct chanset_t *achan = NULL;
 
      for (achan = chanset; achan != NULL; achan = achan->next)
        check_this_ban(achan, s, yn);
    }
#endif /* LEAF */
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
    putlog(LOG_CMDS, "*", "#%s# %sstick ban %s %s", dcc[idx].nick, yn ? "" : "un", s, chname);
    dprintf(idx, "%stuck %s ban: %s\n", yn ? "S" : "Uns", chname, s);
#ifdef LEAF
    check_this_ban(chan, s, yn);
#endif /* LEAF */
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
  char *nick = NULL, *chn = NULL;
  struct chanset_t *chan = NULL;
  struct userrec *u1 = NULL;
  struct chanuserrec *chanrec = NULL;

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
  char *nick = NULL, *chn = NULL;
  struct userrec *u1 = NULL;
  struct chanuserrec *chanrec = NULL;

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
  char *chname = NULL;
  char buf2[1024] = "";
  int delay = 10;
  struct chanset_t *chan = NULL;

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

  sprintf(buf2, "cycle %s %d", chname, delay); /* this just makes the bot PART */
  putallbots(buf2);
#ifdef LEAF
  do_chanset(NULL, chan, "+inactive", DO_LOCAL);
  dprintf(DP_SERVER, "PART %s\n", chan->name);
  chan->channel.jointime = ((now + delay) - server_lag);
#endif /* LEAF */
}

static void cmd_down(struct userrec *u, int idx, char *par)
{
  char *chname = NULL, buf2[1024] = "";
  struct chanset_t *chan = NULL;

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

static void pls_chan(struct userrec *u, int idx, char *par, char *bot)
{
  char *chname = NULL, result[1024] = "", buf[2048] = "";
  struct chanset_t *chan = NULL;

  if (!bot)
    putlog(LOG_CMDS, "*", "#%s# +chan %s", dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, "Usage: +chan [%s]<channel> [options]\n", CHANMETA);
    return;
  }

  chname = newsplit(&par);
  sprintf(buf, "cjoin %s %s %s", chname, bot ? bot : "*", par);		/* +chan makes all bots join */

  if (!bot && findchan_by_dname(chname)) {
    putallbots(buf);
    dprintf(idx, "That channel already exists!\n");
    return;
  } else if ((chan = findchan(chname)) && !bot) {
    putallbots(buf);
    dprintf(idx, "That channel already exists as %s!\n", chan->dname);
    return;
  } else if (strchr(CHANMETA, chname[0]) == NULL) {
    dprintf(idx, "Invalid channel prefix.\n");
    return;
  } else if (strchr(chname, ',') != NULL) {
    dprintf(idx, "Invalid channel name.\n");
    return;
  }
  if (!chan && !findchan_by_dname(chname) && channel_add(result, chname, par) == ERROR) {
    dprintf(idx, "Invalid channel or channel options.\n");
    if (result[0])
      dprintf(idx, "%s\n", result);
  } else {
    if ((chan = findchan_by_dname(chname))) {
      if (!bot) {
        char tmp[51] = "";

        sprintf(tmp, "addedby %s addedts %li", dcc[idx].nick, now);
        if (buf[0])
          sprintf(buf, "%s %s", buf, tmp);
        else
          sprintf(buf, "%s", tmp);
        do_chanset(NULL, chan, tmp, DO_LOCAL);
        dprintf(idx, "Channel %s added to the botnet.\n", chname);
      } else {
        dprintf(idx, "Channel %s added to the bot: %s\n", chname, bot);
      }
      putallbots(buf);
    }
#ifdef HUB
    write_userfile(-1);
#endif /* HUB */
  }
}

static void cmd_pls_chan(struct userrec *u, int idx, char *par)
{
  pls_chan(u, idx, par, NULL);
}

static void cmd_botjoin(struct userrec *u, int idx, char *par)
{
  char *bot = NULL;
  struct userrec *botu = NULL;

  putlog(LOG_CMDS, "*", "#%s# botjoin %s", dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, "Usage: botjoin <bot> [%s]<channel> [options]\n", CHANMETA);
    return;
  }
  bot = newsplit(&par);
  botu = get_user_by_handle(userlist, bot);
  if (botu && botu->bot) {
    pls_chan(u, idx, par, bot);
  } else {
    dprintf(idx, "Error: '%s' is not a bot.\n", bot);
  }
}

static void mns_chan(struct userrec *u, int idx, char *par, char *bot)
{
  char *chname = NULL, buf2[1024] = "";
  struct chanset_t *chan = NULL;
  int i;

  if (!bot)
    putlog(LOG_CMDS, "*", "#%s# -chan %s", dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, "Usage: -chan [%s]<channel>\n", CHANMETA);
    return;
  }
  chname = newsplit(&par);

  sprintf(buf2, "cpart %s %s", chname, bot ? bot : "*");
  if (bot)		/* bot will just set it +inactive */
    putbot(bot, buf2);
  else
    putallbots(buf2);

  chan = findchan_by_dname(chname);
  if (!chan) {
    if ((chan = findchan(chname)))
      dprintf(idx, "That channel exists with a short name of %s, use that.\n", chan->dname);
    else
      dprintf(idx, "That channel doesn't exist!\n");
    return;
  }

  if (!bot) {
    for (i = 0; i < dcc_total; i++)
      if ((dcc[i].type->flags & DCT_CHAT) && !rfc_casecmp(dcc[i].u.chat->con_chan, chan->dname)) {
        dprintf(i, "%s is no longer a valid channel, changing your console to '*'\n", chname);
        strcpy(dcc[i].u.chat->con_chan, "*");
      } 
    remove_channel(chan);
#ifdef HUB
    write_userfile(-1);
#endif /* HUB */
    dprintf(idx, "Channel %s removed from the botnet.\n", chname);
    dprintf(idx, "This includes any channel specific bans, invites, exemptions and user records that you set.\n");
  } else
    dprintf(idx, "Channel %s removed from the bot: %s\n", chname, bot);
}

static void cmd_mns_chan(struct userrec *u, int idx, char *par)
{
  mns_chan(u, idx, par, NULL);
}

static void cmd_botpart(struct userrec *u, int idx, char *par)
{
  char *bot = NULL;
  struct userrec *botu = NULL;

  putlog(LOG_CMDS, "*", "#%s# botpart %s", dcc[idx].nick, par);
  
  if (!par[0]) {
    dprintf(idx, "Usage: botpart <bot> [%s]<channel> [options]\n", CHANMETA);
    return;
  }

  bot = newsplit(&par);
  botu = get_user_by_handle(userlist, bot);
  if (botu && botu->bot) {
    mns_chan(u, idx, par, bot);
  } else {
    dprintf(idx, "Error: '%s' is not a bot.\n", bot);
  }
}

/* thanks Excelsior */
#define FLAG_COLS 4
void show_flag(int idx, char *work, int *cnt, const char *name, unsigned int state)
{
  char tmp[101] = "", chr_state[15] = "";
  /* empty buffer if no (char *) name */
  if (((*cnt) < (FLAG_COLS - 1)) && (!name || (name && !name[0]))) (*cnt) = (FLAG_COLS - 1); 
  (*cnt)++;
  if (*cnt > FLAG_COLS) {
    *cnt = 1;
    work[0] = 0;
  }
  if (!work[0])
    sprintf(work, "  ");
  if (name && name[0]) {
    chr_state[0] = 0;
    if (state) {
      strcat(chr_state, GREEN(idx));
      strcat(chr_state, "+");
    } else {
      strcat(chr_state, RED(idx));
      strcat(chr_state, "-");
    }
    strcat(chr_state, COLOR_END(idx));
    egg_snprintf(tmp, sizeof tmp, "%s%-17s", chr_state, name);
    strcat(work, tmp);
  }
  if (*cnt >= FLAG_COLS)
    dprintf(idx, "%s\n", work);
}

#define INT_COLS 1
void show_int(int idx, char *work, int *cnt, const char *desc, int state, const char *yes, const char *no)
{
  char tmp[101] = "", chr_state[101] = "";

  egg_snprintf(chr_state, sizeof chr_state, "%d", state);  
  /* empty buffer if no (char *) name */
  if (((*cnt) < (INT_COLS - 1)) && (!desc || (desc && !desc[0]))) (*cnt) = (INT_COLS - 1);
  (*cnt)++;
  if (*cnt > INT_COLS) {
    *cnt = 1;
    work[0] = 0;
  }
  if (!work[0])
    sprintf(work, "  ");
  /* need to make next line all one char, and then put it into %-30s */
  if (desc && desc[0]) {
    char tmp2[50] = "", tmp3[50] = "";

    strcat(tmp2, BOLD(idx));
    if (state && yes) {
      strcat(tmp2, yes);
    } else if (!state && no) {
      strcat(tmp2, no);
      strcat(tmp3, " (");
      strcat(tmp3, chr_state);
      strcat(tmp3, ")");
    } else if ((state && !yes) || (!state && !no)) {
      strcat(tmp2, chr_state);
    }
    strcat(tmp2, BOLD_END(idx));
    egg_snprintf(tmp, sizeof tmp, "%-30s %-20s %s", desc, tmp2, (tmp3 && tmp3[0]) ? tmp3 : "");
    strcat(work, tmp);
  }
  if (*cnt >= INT_COLS)
    dprintf(idx, "%s\n", work);
}

#define SHOW_FLAG(name, state) show_flag(idx, work, &cnt, name, state)
#define SHOW_INT(desc, state, yes, no) show_int(idx, work, &cnt, desc, state, yes, no)
static void cmd_chaninfo(struct userrec *u, int idx, char *par)
{
  char *chname = NULL, work[512] = "";
  struct chanset_t *chan = NULL;
  int cnt = 0;

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
    char nick[NICKLEN] = "", date[81] = "";

    if (chan->added_ts) {
#ifndef S_UTCTIME
      egg_strftime(date, sizeof date, "%c %Z", localtime(&(chan->added_ts)));
#else /* !S_UTCTIME */
      egg_strftime(date, sizeof date, "%c %Z", gmtime(&(chan->added_ts)));
#endif /* S_UTCTIME */
    } else
      date[0] = 0;
    if (chan->added_by && chan->added_by[0])
      egg_snprintf(nick, sizeof nick, "%s", chan->added_by);
    else
      nick[0] = 0;
    putlog(LOG_CMDS, "*", "#%s# chaninfo %s", dcc[idx].nick, chname);
    if (nick[0] && date[0])
      dprintf(idx, "Settings for channel %s (Added %s by %s%s%s):\n", chan->dname, date, BOLD(idx), nick, BOLD_END(idx));
    else
      dprintf(idx, "Settings for channel %s:\n", chan->dname);
/* FIXME: SHOW_CHAR() here */
    get_mode_protect(chan, work);
    dprintf(idx, "Protect modes (chanmode): %s\n", work[0] ? work : "None");
    dprintf(idx, "Protect topic (topic)   : %s\n", chan->topic[0] ? chan->topic : "");
/* Chanchar template
 *  dprintf(idx, "String temp: %s\n", chan->temp[0] ? chan->temp : "NULL");
 */
    dprintf(idx, "Channel flags:\n");
    work[0] = 0;
    SHOW_FLAG("autoop",		channel_autoop(chan));
    SHOW_FLAG("bitch",		channel_bitch(chan));
    SHOW_FLAG("closed",		channel_closed(chan));
    SHOW_FLAG("cycle",		channel_cycle(chan));
    SHOW_FLAG("enforcebans", 	channel_enforcebans(chan));
    SHOW_FLAG("fastop",		channel_fastop(chan));
    SHOW_FLAG("inactive",	channel_inactive(chan));
    SHOW_FLAG("manop",		channel_manop(chan));
    SHOW_FLAG("nodesynch",	channel_nodesynch(chan));
    SHOW_FLAG("nomop",		channel_nomop(chan));
    SHOW_FLAG("private",	channel_private(chan));
    SHOW_FLAG("protectops",	channel_protectops(chan));
    SHOW_FLAG("revenge",	channel_revenge(chan));
    SHOW_FLAG("revengebot",	channel_revengebot(chan));
    SHOW_FLAG("take",		channel_take(chan));
    SHOW_FLAG("voice",		channel_voice(chan));
    SHOW_FLAG("", 0);
    SHOW_FLAG("dynamicbans",	channel_dynamicbans(chan));
    SHOW_FLAG("userbans",	!channel_nouserbans(chan));
    SHOW_FLAG("dynamicexempts",	channel_dynamicexempts(chan));
    SHOW_FLAG("userexempts",	!channel_nouserexempts(chan));
    SHOW_FLAG("dynamicinvites",	channel_dynamicinvites(chan));
    SHOW_FLAG("userinvites",	!channel_nouserinvites(chan));
    SHOW_FLAG("", 0);
    work[0] = 0;

/* Chanflag template
 *          (chan->status & CHAN_TEMP) ? '+' : '-',
 * also include %ctemp in dprintf.
 */

    work[0] = cnt = 0;
/* Chanint template
 * SHOW_INT("Desc: ", integer, "YES", "NO");
 */
    dprintf(idx, "Channel settings:\n");
    SHOW_INT("Ban-time: ", chan->ban_time, NULL, "Forever");
    SHOW_INT("Closed-ban: ", chan->closed_ban, NULL, "Don't!");
    SHOW_INT("Closed-Private", chan->closed_private, NULL, "Don't!");
    SHOW_INT("Exempt-time: ", chan->exempt_time, NULL, "Forever");
    SHOW_INT("Idle Kick after (idle-kick): ", chan->idle_kick, "", "Don't!");
    SHOW_INT("Invite-time: ", chan->invite_time, NULL, "Forever");
    SHOW_INT("Limit raise (limit): ", chan->limitraise, NULL, "Disabled");
    SHOW_INT("Revenge-mode: ", chan->revenge_mode, NULL, NULL);
    SHOW_INT("Stopnethack-mode: ", chan->stopnethack_mode, "", "Don't!");

    dprintf(idx, "Flood settings:   chan ctcp join kick deop nick\n");
    dprintf(idx, "  number:          %3d  %3d  %3d  %3d  %3d  %3d\n",
	    chan->flood_pub_thr, chan->flood_ctcp_thr,
	    chan->flood_join_thr, chan->flood_kick_thr,
	    chan->flood_deop_thr, chan->flood_nick_thr);
    dprintf(idx, "  time  :          %3d  %3d  %3d  %3d  %3d  %3d\n",
	    chan->flood_pub_time, chan->flood_ctcp_time,
	    chan->flood_join_time, chan->flood_kick_time,
	    chan->flood_deop_time, chan->flood_nick_time);
  }
}

static void cmd_chanset(struct userrec *u, int idx, char *par)
{
  char *chname = NULL, result[1024] = "";
  struct chanset_t *chan = NULL;
  int all = 0;

  putlog(LOG_CMDS, "*", "#%s# chanset %s", dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, "Usage: chanset [%schannel|*] <settings>\n", CHANMETA);
    return;
  }

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
      } else if ((strstr(par, "+inactive") || strstr(par, "-inactive")) && (!glob_owner(user))) {
        dprintf(idx, "You don't have access to set +/-inactive on %s (halting command due to lazy coder).\n", chname);
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
    if (!chan && !(chan = findchan_by_dname(chname = dcc[idx].u.chat->con_chan))) {
      dprintf(idx, "Invalid console channel.\n");
      return;
    }
  }
  if (do_chanset(result, all ? NULL : chan, par, DO_LOCAL | DO_NET) == ERROR) {
    dprintf(idx, "Error trying to set { %s } on %s: %s\n", par, all ? "all channels" : chan->dname, result);
    return;
  }
  if (all)
    dprintf(idx, "Successfully set modes { %s } on all channels.\n", par);
  else
    dprintf(idx, "Successfully set modes { %s } on %s\n", par, chan->dname);

#ifdef HUB
  write_userfile(idx);
#endif /* HUB */
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
  {"botjoin",	"n",	(Function) cmd_botjoin,		NULL},
  {"botpart",	"n",	(Function) cmd_botpart,		NULL},
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
