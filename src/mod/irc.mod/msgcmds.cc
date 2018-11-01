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
 * msgcmds.c -- part of irc.mod
 *   all commands entered via /MSG
 *
 */


#include "src/core_binds.h"
#include <algorithm>
using std::swap;

static int msg_bewm(char *nick, char *host, struct userrec *u, char *par)
{
  struct chanset_t *chan = NULL;

  if (!homechan[0] || !(chan = findchan_by_dname(homechan)))
    return BIND_RET_BREAK;

  if (!channel_active(chan))
    return BIND_RET_BREAK;

  if (match_my_nick(nick))
    return BIND_RET_BREAK;

  bd::String msg;

  if (!u) {
    msg = bd::String::printf(STR("---- (%s!%s) attempted to gain secure invite, but is not a recognized user."), nick, host);
    privmsg(chan->name, msg, DP_SERVER);

    putlog(LOG_CMDS, "*", STR("(%s!%s) !*! BEWM"), nick, host);
    return BIND_RET_BREAK;
  }
  if (u->bot)
    return BIND_RET_BREAK;

  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };

  get_user_flagrec(u, &fr, chan->dname, chan);

  if (!chk_op(fr, chan))  {
    putlog(LOG_CMDS, "*", STR("(%s!%s) !%s! !BEWM"), nick, host, u->handle);
    msg = bd::String::printf(STR("---- %s (%s!%s) attempted to gain secure invite, but is missing a flag."), u->handle, nick, host);
    privmsg(chan->name, msg, DP_SERVER);
    return BIND_RET_BREAK;
  }

  msg = bd::String::printf("\001ACTION has invited \002%s\002 (%s!%s) to %s.\001", u->handle, nick, host, chan->dname);
  privmsg(chan->name, msg, DP_SERVER);

  cache_invite(chan, nick, host, u->handle, 0, 0);
  putlog(LOG_CMDS, "*", STR("(%s!%s) !%s! BEWM"), nick, host, u->handle);

  return BIND_RET_BREAK;
}

static int msg_pass(char *nick, char *host, struct userrec *u, char *par)
{
  char *old = NULL, *mynew = NULL;

  if (match_my_nick(nick))
    return BIND_RET_BREAK;
  if (!u) {
    putlog(LOG_CMDS, "*", "(%s!%s) !*! PASS", nick, host);
    return BIND_RET_BREAK;
  }
  if (u->bot)
    return BIND_RET_BREAK;
  if (!par[0]) {
    notice(nick, u_pass_match(u, "-") ? "You don't have a password set." : "You have a password set.", DP_HELP);
    putlog(LOG_CMDS, "*", "(%s!%s) !%s! PASS?", nick, host, u->handle);
    return BIND_RET_BREAK;
  }
  old = newsplit(&par);
  if (!u_pass_match(u, "-") && !par[0]) {
    putlog(LOG_CMDS, "*", "(%s!%s) !%s! $b!$bPASS...", nick, host, u->handle);
    notice(nick, "You already have a password set.", DP_HELP);
    return BIND_RET_BREAK;
  }
  if (par[0]) {
    if (!u_pass_match(u, old)) {
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! $b!$bPASS...", nick, host, u->handle);
      notice(nick, "Incorrect password.", DP_HELP);
      return BIND_RET_BREAK;
    }
    mynew = newsplit(&par);
  } else {
    mynew = old;
  }

  if (!goodpass(mynew, 0, nick)) {
    putlog(LOG_CMDS, "*", "(%s!%s) !%s! $b!$bPASS...", nick, host, u->handle);
    return BIND_RET_BREAK;
  }

  putlog(LOG_CMDS, "*", "(%s!%s) !%s! PASS", nick, host, u->handle);

  set_user(&USERENTRY_PASS, u, mynew);
  bd::String msg;
  msg = bd::String::printf("%s '%s'.", mynew == old ? "Password set to:" : "Password changed to:", mynew);
  notice(nick, msg, DP_HELP);
  return BIND_RET_BREAK;
}

static int msg_op(char *nick, char *host, struct userrec *u, char *par)
{
  struct chanset_t *chan = NULL;
  char *pass = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };

  if (match_my_nick(nick))
    return BIND_RET_BREAK;
  pass = newsplit(&par);

  /* Check if they used the wrong order */
  if (!u_pass_match(u, pass) && u_pass_match(u, par)) {
    swap(pass, par);
  }

  bd::String msg;

  if (homechan[0]) {
    struct chanset_t *hchan = NULL;

    hchan = findchan_by_dname(homechan);

    if (hchan && channel_active(hchan) && !ismember(hchan, nick)) {
      putlog(LOG_CMDS, "*", "(%s!%s) !*! failed OP %s (not in %s)", nick, host, par, homechan);
      if (par[0]) 
        msg = bd::String::printf("---- (%s!%s) attempted to OP for %s but is not currently in %s.", nick, host, par, homechan);
      else
        msg = bd::String::printf("---- (%s!%s) attempted to OP but is not currently in %s.", nick, host, homechan);
      privmsg(homechan, msg, DP_SERVER);
      return BIND_RET_BREAK;
    }
  }

  if (u_pass_match(u, pass)) {
    if (!u_pass_match(u, "-")) {
      memberlist *m = NULL;

      if (par[0]) {
        chan = findchan_by_dname(par);
        if (chan && channel_active(chan)) {
          m = ismember(chan, nick);
          get_user_flagrec(u, &fr, par, chan);
          if (chk_op(fr, chan)) {
            if (do_op(m, chan, 0, 1)) {
              stats_add(u, 0, 1);
              putlog(LOG_CMDS, "*", "(%s!%s) !%s! OP %s", nick, host, u->handle, par);
              if (manop_warn && chan->manop) {
                msg = bd::String::printf("%s is currently set to punish for manual op.", chan->dname);
                notice(nick, msg, DP_HELP);
              }
            }
          }
          return BIND_RET_BREAK;
        }
      } else {
        int stats = 0;
        for (chan = chanset; chan; chan = chan->next) {
          m = ismember(chan, nick);
          get_user_flagrec(u, &fr, chan->dname, chan);
          if (chk_op(fr, chan)) {
            if (do_op(m, chan, 0, 1)) {
              stats++;
              if (manop_warn && chan->manop) {
                msg = bd::String::printf("%s is currently set to punish for manual op.", chan->dname);
                notice(nick, msg, DP_HELP);
              }
            }
          }
        }
        putlog(LOG_CMDS, "*", "(%s!%s) !%s! OP", nick, host, u->handle);
        if (stats)
          stats_add(u, 0, 1);
        return BIND_RET_BREAK;
      }
    }
  }
  putlog(LOG_CMDS, "*", "(%s!%s) !*! failed OP", nick, host);
  return BIND_RET_BREAK;
}

static int msg_ident(char *nick, char *host, struct userrec *u, char *par)
{
  char s[UHOSTLEN] = "", s1[UHOSTLEN] = "", *pass = NULL, who[HANDLEN + 1] = "";
  struct userrec *u2 = NULL;

  if (match_my_nick(nick) || (u && u->bot))
    return BIND_RET_BREAK;

  pass = newsplit(&par);
  if (!par[0])
    strlcpy(who, nick, sizeof(who));
  else {
    strlcpy(who, par, sizeof(who));
  }
  bd::String msg;
  u2 = get_user_by_handle(userlist, who);
  if (u2 && rfc_casecmp(who, origbotname) && !u2->bot) {
    /* This could be used as detection... */
    if (u_pass_match(u2, "-")) {
      putlog(LOG_CMDS, "*", "(%s!%s) !*! IDENT %s", nick, host, who);
    } else if (!u_pass_match(u2, pass)) {
      putlog(LOG_CMDS, "*", "(%s!%s) !*! failed IDENT %s", nick, host, who);
      return BIND_RET_BREAK;
    } else if (u == u2) {
      notice(nick, "I recognize you there.", DP_HELP);
      return BIND_RET_BREAK;
    } else if (u) {
      msg = bd::String::printf("You're not %s, you're %s.", who, u->handle);
      notice(nick, msg, DP_HELP);
      return BIND_RET_BREAK;
    } else {
      putlog(LOG_CMDS, "*", "(%s!%s) !*! IDENT %s", nick, host, who);
      simple_snprintf(s, sizeof s, "%s!%s", nick, host);
      maskaddr(s, s1, 0); /* *!user@host */
      msg = bd::String::printf("Added hostmask: %s", s1);
      notice(nick, msg, DP_HELP);
      addhost_by_handle(who, s1);
      check_this_user(who, 0, NULL);
      return BIND_RET_BREAK;
    }
  }
  putlog(LOG_CMDS, "*", "(%s!%s) !*! failed IDENT %s", nick, host, who);
  return BIND_RET_BREAK;
}

static int msg_invite(char *nick, char *host, struct userrec *u, char *par)
{
  char *pass = NULL;
  struct chanset_t *chan = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };

  if (match_my_nick(nick))
    return BIND_RET_BREAK;

  pass = newsplit(&par);
  if (u_pass_match(u, pass) && !u_pass_match(u, "-")) {
    if (par[0] == '*') {
      for (chan = chanset; chan; chan = chan->next) {
	get_user_flagrec(u, &fr, chan->dname, chan);
        if (chk_op(fr, chan) && (chan->channel.mode & CHANINV)) {
          cache_invite(chan, nick, host, u->handle, 0, 0);
        }
      }
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! INVITE ALL", nick, host, u->handle);
      return BIND_RET_BREAK;
    }
    bd::String msg;
    if (!(chan = findchan_by_dname(par))) {
      msg = bd::String::printf("Usage: /MSG %s %s <pass> <channel>", botname, msginvite);
      notice(nick, msg, DP_HELP);
      return BIND_RET_BREAK;
    }
    if (!channel_active(chan)) {
      msg = bd::String::printf("%s: Not on that channel right now.", par);
      notice(nick, msg, DP_HELP);
      return BIND_RET_BREAK;
    }
    /* We need to check access here also (dw 991002) */
    get_user_flagrec(u, &fr, par, chan);
    if (chk_op(fr, chan)) {
      cache_invite(chan, nick, host, u->handle, 0, 0);
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! INVITE %s", nick, host, u->handle, par);
      return BIND_RET_BREAK;
    }
  }
  putlog(LOG_CMDS, "*", "(%s!%s) !%s! failed INVITE %s", nick, host, (u ? u->handle : "*"), par);
  return BIND_RET_BREAK;
}

static void logc(const char *cmd, Auth *auth, char *chname, char *par)
{
  if (chname && chname[0])
    putlog(LOG_CMDS, "*", "(%s!%s) !%s! %s %c%s %s", auth->nick.c_str(), auth->host,
        auth->user ? auth->user->handle : "*", chname, auth_prefix[0], cmd, par ? par : "");
  else
    putlog(LOG_CMDS, "*", "(%s!%s) !%s! %c%s %s", auth->nick.c_str(), auth->host,
        auth->user ? auth->user->handle : "*", auth_prefix[0], cmd, par ? par : "");
}
#define LOGC(cmd) logc(cmd, a, chname, par)
  
static int msg_authstart(char *nick, char *host, struct userrec *u, char *par)
{
  if (!u)
    return 0;
  if (match_my_nick(nick))
    return BIND_RET_BREAK;
  if (u && u->bot)
    return BIND_RET_BREAK;

  if (!ischanhub()) {
    putlog(LOG_WARN, "*", STR("(%s!%s) !%s! Attempted AUTH? (I'm not a chathub (+c))"), nick, host, u->handle);
    return BIND_RET_BREAK;
  }

  putlog(LOG_CMDS, "*", STR("(%s!%s) !%s! AUTH?"), nick, host, u->handle);

  Auth *auth = Auth::Find(host);

  if (auth) {
    if (auth->Authed()) {
      notice(nick, "You are already authed.", DP_HELP);
      return 0;
    }
  } else
    auth = new Auth(RfcString(nick), host, u);

  /* Send "auth." if they are recognized, otherwise "auth!" */
  auth->Status(AUTH_PASS);
  bd::String msg;
  msg = bd::String::printf(STR("auth%s %s"), u ? "." : "!", conf.bot->nick);
  privmsg(nick, msg, DP_HELP);

  return BIND_RET_BREAK;
}

static void
AuthFinish(Auth *auth)
{
  putlog(LOG_CMDS, "*", STR("(%s!%s) !%s! +AUTH"), auth->nick.c_str(), auth->host, auth->user ? auth->user->handle : "*");
  auth->Done();
  bd::String msg;
  msg = bd::String::printf(STR("You are now authorized for cmds, see %chelp"), auth_prefix[0]);
  notice(auth->nick.c_str(), msg, DP_HELP);
}

static int msg_auth(char *nick, char *host, struct userrec *u, char *par)
{ 
  char *pass = NULL;

  if (match_my_nick(nick))
    return BIND_RET_BREAK;
  if (u && u->bot)
    return BIND_RET_BREAK;

  Auth *auth = Auth::Find(host);

  if (!auth || auth->Status() != AUTH_PASS || !u)
    return BIND_RET_BREAK;

  pass = newsplit(&par);

  if (u_pass_match(u, pass) && !u_pass_match(u, "-")) {
    auth->user = u;
    if (strlen(auth_key) && get_user(&USERENTRY_SECPASS, u)) {
      putlog(LOG_CMDS, "*", STR("(%s!%s) !%s! AUTH"), nick, host, u->handle);
      auth->Status(AUTH_HASH);
      auth->MakeHash();
      bd::String msg;
      msg = bd::String::printf(STR("-Auth %s %s"), auth->rand, conf.bot->nick);
      privmsg(nick, msg, DP_HELP);
    } else {
      /* no auth_key and/or no SECPASS for the user, don't require a hash auth */
      AuthFinish(auth);
    }
  } else {
    putlog(LOG_CMDS, "*", STR("(%s!%s) !%s! failed AUTH"), nick, host, u->handle);
    delete auth;
  }
  return BIND_RET_BREAK;

}

static int msg_pls_auth(char *nick, char *host, struct userrec *u, char *par)
{
  if (strlen(auth_key) && get_user(&USERENTRY_SECPASS, u)) {
    if (match_my_nick(nick))
      return BIND_RET_BREAK;
    if (u && u->bot)
      return BIND_RET_BREAK;

    Auth *auth = Auth::Find(host);

    if (!auth || auth->Status() != AUTH_HASH)
      return BIND_RET_BREAK;

    if (!strcmp(auth->hash, par)) { /* good hash! */
      AuthFinish(auth);
    } else { /* bad hash! */
      char s[300] = "";

      putlog(LOG_CMDS, "*", STR("(%s!%s) !%s! failed +AUTH"), nick, host, u->handle);
      notice(nick, STR("Invalid hash."), DP_HELP);
      simple_snprintf(s, sizeof(s), "*!%s", host);
      addignore(s, origbotname, STR("Invalid auth hash."), now + (60 * ignore_time));
      delete auth;
    } 
    return BIND_RET_BREAK;
  }
  return BIND_RET_LOG;
}

static int msg_unauth(char *nick, char *host, struct userrec *u, char *par)
{
  if (match_my_nick(nick))
    return BIND_RET_BREAK;
  if (u && u->bot)
    return BIND_RET_BREAK;

  Auth *auth = Auth::Find(host);

  if (!auth)
    return BIND_RET_BREAK;

  delete auth;
  notice(nick, STR("You are now unauthorized."), DP_HELP);
  putlog(LOG_CMDS, "*", STR("(%s!%s) !%s! UNAUTH"), nick, host, u->handle);

  return BIND_RET_BREAK;
}

static int msg_release(char *nick, char *host, struct userrec *u, char *par)
{
  char *pass = NULL;

  if (match_my_nick(nick))
    return BIND_RET_BREAK;
  if (u && u->bot)
    return BIND_RET_BREAK;

  pass = newsplit(&par);

  if (u && u_pass_match(u, pass) && !u_pass_match(u, "-")) {
    struct flag_record fr = {FR_GLOBAL, 0, 0, 0 };
    get_user_flagrec(u, &fr, NULL);

    if (glob_master(fr)) {
      putlog(LOG_CMDS, "*", STR("(%s!%s) !%s! RELEASE"), nick, host, u->handle);
      egg_timeval_t howlong;
      howlong.sec = 5;
      howlong.usec = 0;
      timer_create(&howlong, "Release jupenick", (Function) release_nick);
//      release_nick();
    } else
      putlog(LOG_CMDS, "*", STR("(%s!%s) !%s! failed RELEASE (User it not +m)"), nick, host, u->handle);
  } else
    putlog(LOG_CMDS, "*", STR("(%s!%s) !%s! failed RELEASE"), nick, host, u ? u->handle : "*");
  return BIND_RET_BREAK;

}

/* MSG COMMANDS
 *
 * Function call should be:
 *    int msg_cmd("handle","nick","user@host","params");
 *
 * The function is responsible for any logging. Return 1 if successful,
 * 0 if not.
 */

static cmd_t C_msg[] =
{
  {"auth?",		"",	(Function) msg_authstart,	NULL, LEAF},
  {"auth",		"",	(Function) msg_auth,		NULL, LEAF},
  {"+auth",		"",	(Function) msg_pls_auth,	NULL, LEAF},
  {"unauth",		"",	(Function) msg_unauth,		NULL, LEAF},
  {"ident",   		"",	(Function) msg_ident,		NULL, LEAF},
  {"invite",		"",	(Function) msg_invite,		NULL, LEAF},
  {"op",		"",	(Function) msg_op,		NULL, LEAF},
  {"pass",		"",	(Function) msg_pass,		NULL, LEAF},
  {"release",		"",	(Function) msg_release,		NULL, LEAF},
  {"bewm",		"",	(Function) msg_bewm,		NULL, LEAF},
  {NULL,		NULL,	NULL,				NULL, 0}
};
/* vim: set sts=2 sw=2 ts=8 et: */
