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
 * cmdschan.c -- part of channels.mod
 *   commands from a user via dcc that cause server interaction
 *
 */


#include <ctype.h>
#include "src/mod/console.mod/console.h"

static struct flag_record user	 = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
static struct flag_record victim = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };


static void cmd_pls_mask(const char type, int idx, char *par)
{
  const char *cmd = (type == 'b' ? "ban" : type == 'e' ? "exempt" : "invite");

  if (!par[0]) {
usage:
    dprintf(idx, "Usage: +%s <hostmask> [channel] [%%<XdXhXm>] [reason]\n", cmd);
    return;
  }

  char *chname = NULL, *who = NULL, s[UHOSTLEN] = "", s1[UHOSTLEN] = "", *p = NULL, *p_expire = NULL;
  unsigned long int expire_time = 0, expire_foo;
  int sticky = 0;
  struct chanset_t *chan = NULL;

  who = newsplit(&par);
  if (par[0] && strchr(CHANMETA, par[0]))
    chname = newsplit(&par);

  /* Did they mix up the two params? */
  if (!chname && strchr(CHANMETA, who[0])) {
    if (par[0]) {
      chname = who;
      who = newsplit(&par);
    } else {
      goto usage;
    }
  }

  if (chname || !(dcc[idx].user->flags & USER_MASTER)) {
    if (!chname)
      chname = dcc[idx].u.chat->con_chan;
    get_user_flagrec(dcc[idx].user, &user, chname);
    chan = findchan_by_dname(chname);
    /* *shrug* ??? (guppy:10Feb1999) */
    if (!chan || (chan && privchan(user, chan, PRIV_OP))) {
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
      simple_snprintf(s, sizeof s, "%s!*@*", who);	/* Lame nick ban */
    else
      simple_snprintf(s, sizeof s, "*!%s", who);
  } else if (!strchr(who, '@'))
    simple_snprintf(s, sizeof s, "%s@*", who);	/* brain-dead? */
  else
    strlcpy(s, who, sizeof s);
    if (conf.bot->hub)
      simple_snprintf(s1, sizeof s1, "%s!%s@%s", origbotname, botuser, conf.bot->net.host);
    else
      simple_snprintf(s1, sizeof s1, "%s!%s", botname, botuserhost);
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
      putlog(LOG_CMDS, "*", "#%s# (%s) +%s %s %s (%s) (sticky)", dcc[idx].nick, dcc[idx].u.chat->con_chan, cmd, s, chan->dname, par);
      dprintf(idx, "New %s sticky %s: %s (%s)\n", chan->dname, cmd, s, par);
    } else {
      putlog(LOG_CMDS, "*", "#%s# (%s) +%s %s %s (%s)", dcc[idx].nick, dcc[idx].u.chat->con_chan, cmd, s, chan->dname, par);
      dprintf(idx, "New %s %s: %s (%s)\n", chan->dname, cmd, s, par);
    }
    if (!conf.bot->hub) {
      if (type == 'e' || type == 'I')
        add_mode(chan, '+', type, s);
      /* Avoid unnesessary modes if you got +dynamicbans
       */
      else
        check_this_ban(chan, s, sticky);
    } else
      write_userfile(idx);
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
    if (!conf.bot->hub) {
      for (chan = chanset; chan != NULL; chan = chan->next) {
        if (type == 'b')
          check_this_ban(chan, s, sticky);
        else
          add_mode(chan, '+', type, s);
      }
    } else
      write_userfile(idx);
  }
}

static void cmd_pls_ban(int idx, char *par)
{
  cmd_pls_mask('b', idx, par);
}

static void cmd_pls_exempt(int idx, char *par)
{ 
  cmd_pls_mask('e', idx, par);
}

static void cmd_pls_invite(int idx, char *par)
{
  cmd_pls_mask('I', idx, par);
}

static void cmd_mns_mask(const char type, int idx, char *par)
{
  const char *cmd = (type == 'b' ? "ban" : type == 'e' ? "exempt" : "invite");

  if (!par[0]) {
usage:
    dprintf(idx, "Usage: -%s <hostmask> [channel]\n", cmd);
    return;
  }

  int i = 0, j;
  struct chanset_t *chan = NULL;
  char s[UHOSTLEN] = "", *who = NULL, *chname = NULL, *mask = NULL;
  masklist *m = NULL;

  who = newsplit(&par);
  if (par[0] && strchr(CHANMETA, par[0]))
    chname = newsplit(&par);

  /* Did they mix up the two params? */
  if (!chname && strchr(CHANMETA, who[0])) {
    if (par[0]) {
      chname = who;
      who = newsplit(&par);
    } else {
      goto usage;
    }
  }

  if (!chname)
    chname = dcc[idx].u.chat->con_chan;

  if (chname || !(dcc[idx].user->flags & USER_MASTER)) {
    if (!chname)
      chname = dcc[idx].u.chat->con_chan;
    get_user_flagrec(dcc[idx].user, &user, chname);

    if (strchr(CHANMETA, chname[0]) && privchan(user, findchan_by_dname(chname), PRIV_OP)) {
      dprintf(idx, "No such channel.\n");
      return;
    }
    if (!chk_op(user, findchan_by_dname(chname)))
      return;
  }
  strlcpy(s, who, sizeof s);
  i = u_delmask(type, NULL, s, (dcc[idx].user->flags & USER_MASTER));
  if (i > 0) {
    if (lastdeletedmask)
      mask = lastdeletedmask;
    else
      mask = s;
    putlog(LOG_CMDS, "*", "#%s# -%s %s", dcc[idx].nick, cmd, mask);
    dprintf(idx, "%s %s: %s\n", "Removed", cmd, s);
    if (!conf.bot->hub) {
      for (chan = chanset; chan != NULL; chan = chan->next)
        add_mode(chan, '-', type, mask);
    } else
      write_userfile(idx);
    return;
  }
  /* Channel-specific ban? */
  if (chname)
    chan = findchan_by_dname(chname);
  if (chan) {
    m = type == 'b' ? chan->channel.ban : type == 'e' ? chan->channel.exempt : chan->channel.invite;
    if (str_isdigit(who) && (i = atoi(who)) > 0) {
      simple_snprintf(s, sizeof s, "%d", i);
      j = u_delmask(type, chan, s, 1);
      if (j > 0) {
        if (lastdeletedmask)
          mask = lastdeletedmask;
        else
          mask = s;
	putlog(LOG_CMDS, "*", "#%s# (%s) -%s %s", dcc[idx].nick, chan->dname, cmd, mask);
	dprintf(idx, "Removed %s channel %s: %s\n", chan->dname, cmd, mask);
        if (!conf.bot->hub)
          add_mode(chan, '-', type, mask);
        else
          write_userfile(idx);
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
	    dprintf(idx, "%s %s '%s' on %s.\n", "Removed", cmd, m->mask, chan->dname);
	    putlog(LOG_CMDS, "*", "#%s# (%s) -%s %s [on channel]", dcc[idx].nick, chan->dname, cmd, who);

            if (!conf.bot->hub)
              add_mode(chan, '-', type, m->mask);
            else
              write_userfile(idx);
	    return;
	  }
	}
      }
    } else {
      j = u_delmask(type, chan, who, 1);
      if (j > 0) {
	putlog(LOG_CMDS, "*", "#%s# (%s) -%s %s", dcc[idx].nick, dcc[idx].u.chat->con_chan, cmd, who);
	dprintf(idx, "Removed %s channel %s: %s\n", chname, cmd, who);
        if (!conf.bot->hub)
          add_mode(chan, '-', type, who);
        else
          write_userfile(idx);
	return;
      }
      for (; m && m->mask && m->mask[0]; m = m->next) {
	if (!rfc_casecmp(m->mask, who)) {
	  dprintf(idx, "%s %s '%s' on %s.\n", "Removed", cmd, m->mask, chan->dname);
	  putlog(LOG_CMDS, "*", "#%s# (%s) -%s %s [on channel]", dcc[idx].nick, chan->dname, cmd, who);
          if (!conf.bot->hub)
            add_mode(chan, '-', type, m->mask);
          else
            write_userfile(idx);
	  return;
	}
      }
    }
  }
  dprintf(idx, "No such %s.\n", cmd);
}

static void cmd_mns_ban(int idx, char *par)
{
  cmd_mns_mask('b', idx, par);
}

static void cmd_mns_exempt(int idx, char *par)
{
  cmd_mns_mask('e', idx, par);
}

static void cmd_mns_invite(int idx, char *par)
{
  cmd_mns_mask('I', idx, par);
}

static void cmd_masks(const char type, int idx, char *par)
{
  const char *str_type = (type == 'b' ? "ban" : type == 'e' ? "exempt" : "invite");

  if (!strcasecmp(par, "all")) {
    putlog(LOG_CMDS, "*", "#%s# %ss all", dcc[idx].nick, str_type);
    tell_masks(type, idx, 1, "");
  } else if (!strcasecmp(par, "global")) {
    putlog(LOG_CMDS, "*", "#%s# %ss global", dcc[idx].nick, str_type);
    tell_masks(type, idx, 1, "", 1);
  } else {
    putlog(LOG_CMDS, "*", "#%s# %ss %s", dcc[idx].nick, str_type, par);
    tell_masks(type, idx, 0, par);
  }
}

static void cmd_bans(int idx, char *par)
{
  cmd_masks('b', idx, par);
}

static void cmd_exempts(int idx, char *par)
{
  cmd_masks('e', idx, par);
}

static void cmd_invites(int idx, char *par)
{
  cmd_masks('I', idx, par);
}

static void cmd_info(int idx, char *par)
{
  if (!use_info) {
    dprintf(idx, "Info storage is turned off.\n");
    return;
  }
  char s[512] = "", *chname = NULL, *s1 = (char *) get_user(&USERENTRY_INFO, dcc[idx].user);
  bool locked = 0;

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
  if (locked && !(dcc[idx].user && (dcc[idx].user->flags & USER_MASTER))) {
    dprintf(idx, "Your info line is locked.  Sorry.\n");
    return;
  }
  if (!strcasecmp(par, "none")) {
    if (chname) {
      par[0] = 0;
      set_handle_chaninfo(userlist, dcc[idx].nick, chname, NULL);
      dprintf(idx, "Removed your info line on %s.\n", chname);
      putlog(LOG_CMDS, "*", "#%s# info %s none", dcc[idx].nick, chname);
    } else {
      set_user(&USERENTRY_INFO, dcc[idx].user, NULL);
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
    set_user(&USERENTRY_INFO, dcc[idx].user, par);
    dprintf(idx, "Your default info is now: %s\n", par);
    putlog(LOG_CMDS, "*", "#%s# info ...", dcc[idx].nick);
  }
}

static void cmd_chinfo(int idx, char *par)
{

  if (!use_info) {
    dprintf(idx, "Info storage is turned off.\n");
    return;
  }

  char *handle = newsplit(&par);

  if (!handle[0]) {
    dprintf(idx, "Usage: chinfo <handle> [channel] <new-info>\n");
    return;
  }

  struct userrec *u1 = get_user_by_handle(userlist, handle);

  if (!u1 || (u1 && !whois_access(dcc[idx].user, u1))) {
    dprintf(idx, "No such user.\n");
    return;
  }

  char *chname = NULL;

  if (par[0] && strchr(CHANMETA, par[0])) {
    chname = newsplit(&par);
    if (!findchan_by_dname(chname)) {
      dprintf(idx, "No such channel.\n");
      return;
    }
  } else
    chname = 0;
  if (u1->bot && !(dcc[idx].user->flags & USER_MASTER)) {
    dprintf(idx, "You have to be master to change bots info.\n");
    return;
  }
  if ((u1->flags & USER_OWNER) && !(dcc[idx].user->flags & USER_OWNER)) {
    dprintf(idx, "You can't change info for the bot owner.\n");
    return;
  }
  if (chname) {
    get_user_flagrec(dcc[idx].user, &user, chname);
    get_user_flagrec(u1, &victim, chname);
    if ((chan_owner(victim) || glob_owner(victim)) &&
	!(glob_owner(user) || chan_owner(user))) {
      dprintf(idx, "You can't change info for the channel owner.\n");
      return;
    }
  }
  putlog(LOG_CMDS, "*", "#%s# chinfo %s %s %s", dcc[idx].nick, handle, chname ? chname : par, chname ? par : "");
  if (!strcasecmp(par, "none"))
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

    if (conf.bot->hub)
      write_userfile(idx);
  } else {
    set_user(&USERENTRY_INFO, u1, par[0] ? par : NULL);
    if (par[0] == '@')
      dprintf(idx, "New default info (LOCKED) for %s: %s\n", handle, &par[1]);
    else if (par[0])
      dprintf(idx, "New default info for %s: %s\n", handle, par);
    else
      dprintf(idx, "Wiped default info for %s\n", handle);

    if (conf.bot->hub)
      write_userfile(idx);
  }
}

static void cmd_checkchannels(int idx, char *par)
{
  putlog(LOG_CMDS, "*", "#%s# checkchannels", dcc[idx].nick);
  botnet_send_cmd_broad(-1, conf.bot->nick, dcc[idx].nick, idx, "chans");
}

static void cmd_slowjoin(int idx, char *par)
{
  int intvl = 0, delay = 0, count = 0;
  char *chname = NULL, *p = NULL, buf[2048] = "", buf2[RESULT_LEN] = "";
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

  simple_snprintf(buf, sizeof(buf), "+inactive addedby %s addedts %li ", dcc[idx].nick, (long)now);

  if (par[0])
    strlcat(buf, par, sizeof(buf));
  if (channel_add(buf2, chname, buf) == ERROR) {
    dprintf(idx, "Invalid channel or channel options.\n");
    if (buf2[0])
      dprintf(idx, " %s\n", buf2);
    return;
  }
  buf2[0] = 0;

  chan = findchan_by_dname(chname);
  if (!chan) {
    dprintf(idx, "Hmmm... Channel didn't get added. Weird *shrug*\n");
    return;
  }
  simple_snprintf(buf2, sizeof(buf2), "cjoin %s %s", chan->dname, buf);
  putallbots(buf2);
  if (conf.bot->hub)
    count = 0;

  chan->status &= ~CHAN_INACTIVE;

  for (bot = tandbot; bot; bot = bot->next) {
    char tmp[100] = "";
    tmp[0] = 0;    
    if (bot->u) {
      if (bot_hublevel(bot->u) < 999) {
	simple_snprintf(tmp, sizeof(tmp), "sj %s 0", chname);
      } else {
        struct flag_record fr = { FR_CHAN|FR_GLOBAL|FR_BOT, 0, 0, 0 };

        get_user_flagrec(bot->u, &fr, chname);
	/* Only send the 'sj' command if the bot is supposed to be in the channel (backups and such) */
        if (bot_shouldjoin(bot->u, &fr, chan, 1)) {
          /* Variation: 60 secs intvl should be 60 +/- 15 */
          int v = (random() % (intvl / 2)) - (intvl / 4);

          delay += intvl;
          simple_snprintf(tmp, sizeof(tmp), "sj %s %i", chname, delay + v);
          count++;
        }
      }
      if (tmp[0])
        putbot(bot->bot, tmp);
    }
  }

  if (!conf.bot->hub && shouldjoin(chan))
    count++;

  dprintf(idx, "%i bots joining %s during the next %i seconds\n", count, chan->dname, delay);

  if (!conf.bot->hub && shouldjoin(chan))
    join_chan(chan);
}

static void cmd_slowpart(int idx, char *par)
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
    dprintf(idx, "No such channel %s\n", chname);
    return;
  }

  if (conf.bot->hub)
    count = 0;

  for (bot = tandbot; bot; bot = bot->next) {
    char tmp[100] = "";

    tmp[0] = 0;
    if (bot->u) {
      if (bot_hublevel(bot->u) < 999) {		/* HUB */
        simple_snprintf(tmp, sizeof(tmp), "sp %s 0", chname);
      } else {					/* LEAF */
        struct flag_record fr = { FR_CHAN|FR_GLOBAL|FR_BOT, 0, 0, 0 };
        get_user_flagrec(bot->u, &fr, chname);
	/* Only send the 'sp' command if the bot is supposed to be in the channel (backups and such) */
        if (bot_shouldjoin(bot->u, &fr, chan)) {
          /* Variation: 60 secs intvl should be 60 +/- 15 */
          int v = (random() % (intvl / 2)) - (intvl / 4);
          delay += intvl;
          simple_snprintf(tmp, sizeof(tmp), "sp %s %i", chname, delay + v);
          count++;
        }
      }
      if (tmp[0])
        putbot(bot->bot, tmp);
    }  
  }

  remove_channel(chan);
  if (conf.bot->hub)
    write_userfile(-1);
  dprintf(idx, "Channel %s removed from the bot.\n", chname);
  dprintf(idx, "This includes any channel specific bans, invites, exemptions and user records that you set.\n");

  if (findchan_by_dname(chname)) {
    dprintf(idx, "Failed to remove channel.\n");
    return;
  }

  dprintf(idx, "%i bots parting %s during the next %i seconds\n", count, chname, delay);
  if (!conf.bot->hub)
    dprintf(DP_MODE, "PART %s\n", chname);
}

static void cmd_stick_yn(int idx, char *par, int yn)
{
  int i = 0, j;
  struct chanset_t *chan = NULL;
  char *stick_type = NULL, s[UHOSTLEN] = "", chname[81] = "", type = 0, *str_type = NULL;
  maskrec *channel_list = NULL;

  stick_type = newsplit(&par);
  strlcpy(s, newsplit(&par), sizeof s);
  strlcpy(chname, newsplit(&par), sizeof chname);
  if (strcasecmp(stick_type, "exempt") &&
      strcasecmp(stick_type, "invite") &&
      strcasecmp(stick_type, "ban")) {
    strlcpy(chname, s, sizeof chname);
    strlcpy(s, stick_type, sizeof s);
    stick_type = "ban";
  }
  if (!s[0]) {
    dprintf(idx, "Usage: %sstick [ban/exempt/invite] <hostmask or number> [channel]\n", yn ? "" : "un");
    return;
  }
  /* Now deal with exemptions */
  if (!strcasecmp(stick_type, "exempt")) {
    type = 'e';
    str_type = "exempt";
  } else if (!strcasecmp(stick_type, "invite")) {
    type = 'I';
    str_type = "invite";
  } else if (!strcasecmp(stick_type, "ban")) {
    type = 'b';
    str_type = "ban";
  } else
    return;

  if (!chname[0]) {
    channel_list = (type == 'b' ? global_bans : type == 'e' ? global_exempts : global_invites);

    i = u_setsticky_mask(NULL, channel_list, s, (dcc[idx].user->flags & USER_MASTER) ? yn : -1, type);
    if (i > 0) {
      putlog(LOG_CMDS, "*", "#%s# %sstick %s %s", dcc[idx].nick, yn ? "" : "un", str_type, s);
      dprintf(idx, "%stuck %s: %s\n", yn ? "S" : "Uns", str_type, s);

      if (!conf.bot->hub) {
        struct chanset_t *achan = NULL;

        for (achan = chanset; achan != NULL; achan = achan->next)
          check_this_mask(type, achan, s, yn);
      } else
        write_userfile(idx);

      return;
    }
    strlcpy(chname, dcc[idx].u.chat->con_chan, sizeof chname);
  }
  /* Channel-specific mask? */
  if (!(chan = findchan_by_dname(chname))) {
    dprintf(idx, "No such channel.\n");
    return;
  }
  get_user_flagrec(dcc[idx].user, &user, chan->dname);
  if (privchan(user, chan, PRIV_OP)) {
    dprintf(idx, "No such channel.\n");
    return;
  }

  channel_list = (type == 'b' ? chan->bans : type == 'e' ? chan->exempts : chan->invites);

  if (str_isdigit(s)) {
    /* substract the numer of global masks to get the number of the channel masks */
    j = atoi(s);
    j -= count_mask(type == 'b' ? global_bans : type == 'e' ? global_exempts : global_invites);
    simple_snprintf(s, sizeof s, "%d", j);
  }

  j = u_setsticky_mask(chan, channel_list, s, yn, type);
  if (j > 0) {
    putlog(LOG_CMDS, "*", "#%s# %sstick %s %s %s", dcc[idx].nick, yn ? "" : "un", str_type, s, chname);
    dprintf(idx, "%stuck %s %s: %s\n", yn ? "S" : "Uns", chname, str_type, s);
    if (!conf.bot->hub)
      check_this_mask(type, chan, s, yn);
    else
      write_userfile(idx);

    return;
  }
  dprintf(idx, "No such %s.\n", str_type);
}


static void cmd_stick(int idx, char *par)
{
  cmd_stick_yn(idx, par, 1);
}

static void cmd_unstick(int idx, char *par)
{
  cmd_stick_yn(idx, par, 0);
}

static void cmd_pls_chrec(int idx, char *par)
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
  get_user_flagrec(dcc[idx].user, &user, chan->dname);
  get_user_flagrec(u1, &victim, chan->dname);
  if (privchan(user, chan, PRIV_OP)) {
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
  putlog(LOG_CMDS, "*", "#%s# +chrec %s %s", dcc[idx].nick, nick, chan->dname);
  add_chanrec(u1, chan->dname);
  dprintf(idx, "Added %s channel record for %s.\n", chan->dname, nick);
  if (conf.bot->hub)
    write_userfile(idx);
}

static void cmd_mns_chrec(int idx, char *par)
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
  get_user_flagrec(dcc[idx].user, &user, chn);
  get_user_flagrec(u1, &victim, chn);
  if (privchan(user, findchan_by_dname(chn), PRIV_OP)) {
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
  if (conf.bot->hub)
    write_userfile(idx);
}

static void cmd_cycle(int idx, char *par)
{
  char *chname = NULL;
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
    dprintf(idx, "%s is not a valid channel.\n", chname);
    return;
  }
  if (par[0])
    delay = atoi(newsplit(&par));

  if (conf.bot->hub) {
    char buf2[201] = "";

    simple_snprintf(buf2, sizeof(buf2), "cycle %s %d", chname, delay); /* this just makes the bot PART */
    putallbots(buf2);
  } else {
    do_chanset(NULL, chan, "+inactive", DO_LOCAL);
    dprintf(DP_SERVER, "PART %s\n", chan->name);
    chan->channel.jointime = ((now + delay) - server_lag);
  }
  dprintf(idx, "Cycling %s for %d seconds.\n", chan->dname, delay);
}

static void cmd_down(int idx, char *par)
{
  char *chname = NULL, buf2[201] = "";
  struct chanset_t *chan = NULL;

  putlog(LOG_CMDS, "*", "#%s# down %s", dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, "Usage: down [%s]<channel>\n", CHANMETA);
    return;
  }

  chname = newsplit(&par);
  chan = findchan_by_dname(chname);
  if (!chan) {
    dprintf(idx, "%s is not a valid channel.\n", chname);
    return;
  }
  
  simple_snprintf(buf2, sizeof(buf2), "down %s", chan->dname);
  putallbots(buf2);
  if (!conf.bot->hub) {
    add_mode(chan, '-', 'o', botname);
    chan->channel.no_op = (now + 10);
  }
}

static void pls_chan(int idx, char *par, char *bot)
{
  char *chname = NULL, result[RESULT_LEN] = "", buf[2048] = "";
  struct chanset_t *chan = NULL;

  if (!bot)
    putlog(LOG_CMDS, "*", "#%s# +chan %s", dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, "Usage: +chan [%s]<channel> [options]\n", CHANMETA);
    return;
  }

  chname = newsplit(&par);
  simple_snprintf(buf, sizeof(buf), "cjoin %s %s", chname, bot ? bot : "*");		/* +chan makes all bots join */
  if (par[0]) {
    strlcat(buf, " ", sizeof(buf));
    strlcat(buf, par, sizeof(buf));
    strlcat(buf, " ", sizeof(buf));
  }
    
  if (!bot && findchan_by_dname(chname)) {
    dprintf(idx, "That channel already exists!\n");
    return;
  } else if ((chan = findchan(chname)) && !bot) {
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
      dprintf(idx, " %s\n", result);
  } else {
    if ((chan = findchan_by_dname(chname))) {
      char tmp[51] = "";

      simple_snprintf(tmp, sizeof(tmp), "addedby %s addedts %li", dcc[idx].nick, (long) now);
      if (buf[0]) {
        strlcat(buf, " ", sizeof(buf));
        strlcat(buf, tmp, sizeof(buf));
      }
      do_chanset(NULL, chan, buf[0] ? buf : tmp, DO_LOCAL);
      if (!bot) {
        dprintf(idx, "Channel %s added to the botnet.\n", chname);
      } else {
        dprintf(idx, "Channel %s added to the bot: %s\n", chname, bot);
      }
      putallbots(buf);
    }
    if (conf.bot->hub)
      write_userfile(-1);
  }
}

static void cmd_pls_chan(int idx, char *par)
{
  pls_chan(idx, par, NULL);
}

static void mns_chan(int idx, char *par, char *bot)
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

  simple_snprintf(buf2, sizeof(buf2), "cpart %s %s", chname, bot ? bot : "*");
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
    for (i = 0; i < dcc_total; i++) {
      if (dcc[i].type && (dcc[i].type->flags & DCT_CHAT) && 
          !rfc_casecmp(dcc[i].u.chat->con_chan, chan->dname)) {
        dprintf(i, "%s is no longer a valid channel, changing your console to '*'\n", chname);
        strlcpy(dcc[i].u.chat->con_chan, "*", 2);
        console_dostore(i, 0);
      }
    }
    remove_channel(chan);
    if (conf.bot->hub)
      write_userfile(idx);
    dprintf(idx, "Channel %s removed from the botnet.\n", chname);
    dprintf(idx, "This includes any channel specific bans, invites, exemptions and user records that you set.\n");
  } else
    dprintf(idx, "Channel %s removed from the bot: %s\n", chname, bot);
}

static void cmd_mns_chan(int idx, char *par)
{
  mns_chan(idx, par, NULL);
}

/* thanks Excelsior */
#define FLAG_COLS 4
static void show_flag(int idx, char *work, int *cnt, const char *name, unsigned int state, size_t worksiz)
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
    strlcpy(work, "  ", worksiz);
  if (name && name[0]) {
    chr_state[0] = 0;
    if (state) {
      strlcat(chr_state, GREEN(idx), sizeof(chr_state));
      strlcat(chr_state, "+", sizeof(chr_state));
    } else {
      strlcat(chr_state, RED(idx), sizeof(chr_state));
      strlcat(chr_state, "-", sizeof(chr_state));
    }
    strlcat(chr_state, COLOR_END(idx), sizeof(chr_state));
    simple_snprintf(tmp, sizeof tmp, "%s%-17s", chr_state, name);
    strlcat(work, tmp, worksiz);
  }
  if (*cnt >= FLAG_COLS)
    dprintf(idx, "%s\n", work);
}

#define INT_COLS 1
static void show_int(int idx, char *work, int *cnt, const char *desc, int state, const char *yes, const char *no, size_t worksiz)
{
  char tmp[101] = "", chr_state[101] = "";

  simple_snprintf(chr_state, sizeof chr_state, "%d", state);  
  /* empty buffer if no (char *) name */
  if (((*cnt) < (INT_COLS - 1)) && (!desc || (desc && !desc[0]))) (*cnt) = (INT_COLS - 1);
  (*cnt)++;
  if (*cnt > INT_COLS) {
    *cnt = 1;
    work[0] = 0;
  }
  if (!work[0])
    strlcpy(work, "  ", 3);
  /* need to make next line all one char, and then put it into %-30s */
  if (desc && desc[0]) {
    char tmp2[50] = "", tmp3[50] = "";

    strlcat(tmp2, BOLD(idx), sizeof(tmp2));
    if (state && yes) {
      strlcat(tmp2, yes, sizeof(tmp2));
      strlcat(tmp3, " (", sizeof(tmp3));
      strlcat(tmp3, chr_state, sizeof(tmp3));
      strlcat(tmp3, ")", sizeof(tmp3));
    } else if (!state && no) {
      strlcat(tmp2, no, sizeof(tmp2));
      strlcat(tmp3, " (", sizeof(tmp3));
      strlcat(tmp3, chr_state, sizeof(tmp3));
      strlcat(tmp3, ")", sizeof(tmp3));
    } else if ((state && !yes) || (!state && !no)) {
      strlcat(tmp2, chr_state, sizeof(tmp2));
    }
    strlcat(tmp2, BOLD_END(idx), sizeof(tmp2));
    simple_snprintf(tmp, sizeof tmp, "%-30s %-20s %s", desc, tmp2, tmp3[0] ? tmp3 : "");
    strlcat(work, tmp, worksiz);
  }
  if (*cnt >= INT_COLS)
    dprintf(idx, "%s\n", work);
}

#define SHOW_FLAG(name, state) show_flag(idx, work, &cnt, name, state, sizeof(work))
#define SHOW_INT(desc, state, yes, no) show_int(idx, work, &cnt, desc, state, yes, no, sizeof(work))
#define DEFLAG_STR deflag == DEFLAG_KICK ? "Kick" : (deflag == DEFLAG_DEOP ? "Deop" : (deflag == DEFLAG_DELETE ? "Remove" : (deflag == DEFLAG_REACT ? "React" : NULL)))
#define F_STR(x) x == CHAN_FLAG_OP ? "Op" : (x == CHAN_FLAG_VOICE ? "Voice" : (x == CHAN_FLAG_USER ? "User" : NULL))
static void cmd_chaninfo(int idx, char *par)
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
    get_user_flagrec(dcc[idx].user, &user, chname);
    if (!glob_master(user) && !chan_master(user)) {
      dprintf(idx, "You don't have access to %s.\n", chname);
      return;
    }
  }
  if (!strcasecmp(chname, "default"))
    chan = chanset_default;
  else
    chan = findchan_by_dname(chname);
  if (!chan || (chan && privchan(user, chan, PRIV_OP))) {
    dprintf(idx, "No such channel.\n");
    return;
  } else {
    char nick[HANDLEN + 1] = "", date[81] = "";
    int deflag = 0;

    if (chan->added_ts) {
      strftime(date, sizeof date, "%c %Z", gmtime(&(chan->added_ts)));
    } else
      date[0] = 0;
    if (chan->added_by[0])
      strlcpy(nick, chan->added_by, sizeof(nick));
    else
      nick[0] = 0;
    putlog(LOG_CMDS, "*", "#%s# chaninfo %s", dcc[idx].nick, chname);
    if (nick[0] && date[0])
      dprintf(idx, "Settings for channel %s (Added %s by %s%s%s):\n", chan->dname, date, BOLD(idx), nick, BOLD_END(idx));
    else
      dprintf(idx, "Settings for channel %s:\n", chan->dname);
/* FIXME: SHOW_CHAR() here */
    get_mode_protect(chan, work, sizeof(work));
    dprintf(idx, "Protect modes (chanmode): %s\n", work[0] ? work : "None");
    dprintf(idx, "Groups: %s\n", chan->groups && chan->groups->length() ? static_cast<bd::String>(chan->groups->join(" ")).c_str() : "None");
    dprintf(idx, "FiSH Key: %s\n", chan->fish_key[0] ? chan->fish_key : "not set");
//    dprintf(idx, "Protect topic (topic)   : %s\n", chan->topic[0] ? chan->topic : "");
/* Chanchar template
 *  dprintf(idx, "String temp: %s\n", chan->temp[0] ? chan->temp : "NULL");
 */
    dprintf(idx, "Channel flags:\n");
    work[0] = 0;
    SHOW_FLAG("autoop",		channel_autoop(chan));
    SHOW_FLAG("backup",		channel_backup(chan));
    SHOW_FLAG("bitch",		channel_bitch(chan));
    SHOW_FLAG("botbitch",       channel_botbitch(chan));
    SHOW_FLAG("closed",		channel_closed(chan));
    SHOW_FLAG("cycle",		channel_cycle(chan));
    SHOW_FLAG("enforcebans", 	channel_enforcebans(chan));
    SHOW_FLAG("fastop",		channel_fastop(chan));
    SHOW_FLAG("floodban", channel_floodban(chan));
    SHOW_FLAG("inactive",	channel_inactive(chan));
    SHOW_FLAG("nodesynch",	channel_nodesynch(chan));
    SHOW_FLAG("private",	channel_privchan(chan));
    SHOW_FLAG("protect",	channel_protect(chan));
    SHOW_FLAG("rbl",		channel_rbl(chan));
    if (HAVE_TAKE)
      SHOW_FLAG("take",		channel_take(chan));
    SHOW_FLAG("voice",		channel_voice(chan));
    SHOW_FLAG("voicebitch",		channel_voicebitch(chan));
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
 *  SHOW_FLAG("template", channel_template(chan));
 * also include %ctemp in dprintf.
 */

    work[0] = cnt = 0;
/* Chanint template
 * SHOW_INT("Desc: ", integer, "YES", "NO");
 */
    dprintf(idx, "Channel settings:\n");
    deflag = chan->bad_cookie;
    SHOW_INT("Auto-delay: ", chan->auto_delay, NULL, "None");
    SHOW_INT("Bad-cookie:" , chan->bad_cookie, DEFLAG_STR, "Ignore");
    SHOW_INT("Ban-time: ", chan->ban_time, NULL, "Forever");
    SHOW_INT("Ban-type: ", chan->ban_type, NULL, "3");
    SHOW_INT("Closed-ban: ", chan->closed_ban, NULL, "Don't!");
    SHOW_INT("Closed-invite:", chan->closed_invite, NULL, "Don't!");
    SHOW_INT("Closed-Private:", chan->closed_private, NULL, "Don't!");
    SHOW_INT("Closed-Exempt:", chan->closed_exempt_mode, F_STR(chan->closed_exempt_mode), "None");
    SHOW_INT("Exempt-time: ", chan->exempt_time, NULL, "Forever");
    SHOW_INT("Flood-exempt: ", chan->flood_exempt_mode, F_STR(chan->flood_exempt_mode), "None");
    SHOW_INT("Flood-lock-time: ", chan->flood_lock_time, NULL, "Don't");
    SHOW_INT("Caps-Limit(%): ", chan->capslimit, NULL, "None");
    SHOW_INT("Color-Limit: ", chan->colorlimit, NULL, "None");
    SHOW_INT("Invite-time: ", chan->invite_time, NULL, "Forever");
    SHOW_INT("Knock: ", chan->knock_flags, F_STR(chan->knock_flags), "None");
    SHOW_INT("Limit raise (limit): ", chan->limitraise, NULL, "Disabled");
    deflag = chan->manop;
    SHOW_INT("Manop: ", chan->manop, DEFLAG_STR, "Ignore");
    deflag = chan->mdop;
    SHOW_INT("Mdop: ", chan->mdop, DEFLAG_STR, "Ignore");
    deflag = chan->mop;
    SHOW_INT("Mop: ", chan->mop, DEFLAG_STR, "Ignore");
    deflag = chan->revenge;
    SHOW_INT("Revenge: ", chan->revenge, DEFLAG_STR, "Ignore");
    SHOW_INT("Protect-backup: ", chan->protect_backup, "Do!", "Don't!");
    SHOW_INT("Voice-non-ident: ", chan->voice_non_ident, "Do!", "Don't!");
    SHOW_INT("Voice-moderate:", chan->voice_moderate, "Do!", "Don't!");

    dprintf(idx, "Flood settings:   chan bytes ctcp join kick deop nick mjoin mpub mbytes mctcp\n");
    dprintf(idx, "  number:          %3d  %4d  %3d  %3d  %3d  %3d  %3d   %3d  %3d   %4d   %3d\n",
	    chan->flood_pub_thr, chan->flood_bytes_thr, chan->flood_ctcp_thr,
	    chan->flood_join_thr, chan->flood_kick_thr,
	    chan->flood_deop_thr, chan->flood_nick_thr,
            chan->flood_mjoin_thr, chan->flood_mpub_thr,
            chan->flood_mbytes_thr, chan->flood_mctcp_thr);
    dprintf(idx, "  time  :          %3u  %4u  %3u  %3u  %3u  %3u  %3u   %3u  %3u   %4u  %4u\n",
	    chan->flood_pub_time, chan->flood_bytes_time, chan->flood_ctcp_time,
	    chan->flood_join_time, chan->flood_kick_time,
	    chan->flood_deop_time, chan->flood_nick_time,
            chan->flood_mjoin_time, chan->flood_mpub_time,
            chan->flood_mbytes_time, chan->flood_mctcp_time);
  }
}

static void cmd_chanset(int idx, char *par)
{
  char *chname = NULL, result[RESULT_LEN] = "";
  struct chanset_t *chan = NULL;
  int all = 0;

  if (!par[0]) {
    putlog(LOG_CMDS, "*", "#%s# chanset %s", dcc[idx].nick, par);
    dprintf(idx, "Usage: chanset [%schannel|*|default] <settings>\n", CHANMETA);
    return;
  }

  // Determine channel name
  if (strchr(CHANMETA, par[0]) || !strncasecmp(par, "default", 7) || !strncmp(par, "*", 1))
    chname = newsplit(&par);
  else {
    if (strncmp(dcc[idx].u.chat->con_chan, "*", 1) && !(chan = findchan_by_dname(chname = dcc[idx].u.chat->con_chan))) {
      dprintf(idx, "Invalid console channel.\n");
      return;
    }
    chname = dcc[idx].u.chat->con_chan;
  }

  if (chname && chname[0]) {
    if (!strncmp(chname, "*", 1)) {
      all = 1;
      get_user_flagrec(dcc[idx].user, &user, chanset ? chanset->dname : "");
      if (!glob_owner(user)) {
        dprintf(idx, "You need to be a global owner to use '%schanset *'.\n", (dcc[idx].u.chat->channel >= 0) ? settings.dcc_prefix : "");
        return;
      }
    } else if (!strcasecmp(chname, "default")) {
      chan = chanset_default;
    } else
      chan = findchan_by_dname(chname);
  }

  if (!all && !chan && chname && chname[0]) {
    dprintf(idx, "No such channel.\n");
    return;
  }

  if (!par[0]) {
    dprintf(idx, "Usage: chanset [%schannel|*|default] <settings>\n", CHANMETA);
    return;
  }

  if (!all) {
    get_user_flagrec(dcc[idx].user, &user, chan->dname);

    if (privchan(user, chan, PRIV_OP)) {
      dprintf(idx, "No such channel.\n");
      return;
    }

    if (!glob_master(user) && !chan_master(user)) {
      dprintf(idx, "You don't have access to %s. \n", chan->dname);
      return;
    } else if ((strstr(par, "+private") || strstr(par, "-private")) && (!glob_owner(user))) {
      dprintf(idx, "You don't have access to set +/-private on %s (halting command).\n", chan->dname);
      return;
    } else if ((strstr(par, "+backup") || strstr(par, "-backup")) && (!glob_owner(user))) {
      dprintf(idx, "You don't have access to set +/-backup on %s (halting command).\n", chan->dname);
      return;
    } else if ((strstr(par, "+inactive") || strstr(par, "-inactive")) && (!glob_owner(user))) {
      dprintf(idx, "You don't have access to set +/-inactive on %s (halting command).\n", chan->dname);
      return;
    } else if (strstr(par, "groups") && !glob_owner(user)) {
      dprintf(idx, "You don't have access to set groups on %s (halting command).\n", chan->dname);
      return;
    }
  }

  putlog(LOG_CMDS, "*", "#%s# chanset (%s) %s", dcc[idx].nick, all ? "*" : chan->dname, par);
  
  if (do_chanset(result, all ? NULL : chan, par, DO_LOCAL | DO_NET | CMD) == ERROR) {
    dprintf(idx, "Error trying to set { %s } on %s: %s\n", par, all ? "all channels" : chan->dname, result);
    return;
  }

  if (all)
    dprintf(idx, "Successfully set modes { %s } on all channels (Including the default).\n", par);
  else
    dprintf(idx, "Successfully set modes { %s } on %s\n", par, chan->dname);

  if (conf.bot->hub)
    write_userfile(idx);
}

/* DCC CHAT COMMANDS
 *
 * Function call should be:
 *    int cmd_whatever(idx,"parameters");
 *
 * NOTE: As with msg commands, the function is responsible for any logging.
 */
static cmd_t C_dcc_channels[] =
{
  {"+ban",	"o|o",	(Function) cmd_pls_ban,		NULL, AUTH},
  {"+exempt",	"o|o",	(Function) cmd_pls_exempt,	NULL, AUTH},
  {"+invite",	"o|o",	(Function) cmd_pls_invite,	NULL, AUTH},
  {"+chan",	"n",	(Function) cmd_pls_chan,	NULL, 0},
  {"+chrec",	"m|m",	(Function) cmd_pls_chrec,	NULL, 0},
  {"-ban",	"o|o",	(Function) cmd_mns_ban,		NULL, AUTH},
  {"-chan",	"n",	(Function) cmd_mns_chan,	NULL, 0},
  {"-chrec",	"m|m",	(Function) cmd_mns_chrec,	NULL, 0},
  {"-exempt",	"o|o",	(Function) cmd_mns_exempt,	NULL, AUTH},
  {"-invite",	"o|o",	(Function) cmd_mns_invite,	NULL, AUTH},
  {"bans",	"o|o",	(Function) cmd_bans,		NULL, 0},
  {"exempts",	"o|o",	(Function) cmd_exempts,		NULL, 0},
  {"invites",	"o|o",	(Function) cmd_invites,		NULL, 0},
  {"chaninfo",	"m|m",	(Function) cmd_chaninfo,	NULL, 0},
  {"chanset",	"m|m",	(Function) cmd_chanset,		NULL, 0},
  {"chinfo",	"m|m",	(Function) cmd_chinfo,		NULL, 0},
  {"cycle", 	"n|n",	(Function) cmd_cycle,		NULL, AUTH},
  {"down",	"n|n",	(Function) cmd_down,		NULL, AUTH},
  {"info",	"",	(Function) cmd_info,		NULL, 0},
  {"checkchannels", "n",    (Function) cmd_checkchannels,	NULL, HUB},
  {"slowjoin",  "n",    (Function) cmd_slowjoin,	NULL, 0},
  {"slowpart",  "n|n",  (Function) cmd_slowpart,  	NULL, 0},
  {"stick",	"o|o",	(Function) cmd_stick,		NULL, AUTH},
  {"unstick",	"o|o",	(Function) cmd_unstick,		NULL, AUTH},
  {NULL,	NULL,	NULL,			NULL, 0}
};
/* vim: set sts=2 sw=2 ts=8 et: */
