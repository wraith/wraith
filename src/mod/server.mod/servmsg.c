#ifdef LEAF
/*
 * servmsg.c -- part of server.mod
 *
 */

#include <netinet/tcp.h>

#define msgop CFG_MSGOP.ldata ? CFG_MSGOP.ldata : CFG_MSGOP.gdata ? CFG_MSGOP.gdata : ""
#define msgpass CFG_MSGPASS.ldata ? CFG_MSGPASS.ldata : CFG_MSGPASS.gdata ? CFG_MSGPASS.gdata : ""
#define msginvite CFG_MSGINVITE.ldata ? CFG_MSGINVITE.ldata : CFG_MSGINVITE.gdata ? CFG_MSGINVITE.gdata : ""
#define msgident CFG_MSGIDENT.ldata ? CFG_MSGIDENT.ldata : CFG_MSGIDENT.gdata ? CFG_MSGIDENT.gdata : ""


char cursrvname[120] = "";
char curnetwork[120] = "";
static time_t last_ctcp    = (time_t) 0L;
static int    count_ctcp   = 0;
static char   altnick_char = 0;
static unsigned int rolls = 0;
#define ALTCHARS "-_\\`^[]"
#define ROLL_RIGHT
#undef ROLL_LEFT
static int gotfake433(char *from)
{
  size_t len = strlen(botname);
  int use_chr = 1;

  /* First run? */
  if (altnick_char == 0 && !rolls) {
    altnick_char = ALTCHARS[0];
    /* the nick is already as long as it can be. */
    if (len == (unsigned) nick_len) {
      /* make the last char the current altnick_char */
      botname[len - 1] = altnick_char;
    } else {
      /* tack it on to the end */
      botname[len] = altnick_char;
      botname[len + 1] = 0;
    }
  } else {
    char *p = NULL;

    if ((p = strchr(ALTCHARS, altnick_char)))
      p++;
    /* if we haven't been rolling, use the ALTCHARS
     */
    if (!rolls && p && *p) {
      altnick_char = *p;
    /* if we've already rolled, just keep rolling until we've rolled completely 
     * after that, just generate random chars 
     */
    } else if (rolls < len) {
        /* fun BX style rolling, WEEEE */
        char tmp = 0;
        int i = 0;

        altnick_char = 0;
        use_chr = 0;

        if (rolls == 0) {
          strcpy(botname, origbotname);
          len = strlen(botname);
        }
#ifdef ROLL_RIGHT
        tmp = botname[len - 1];
#endif /* ROLL_RIGHT */
#ifdef ROLL_LEFT
        tmp = botname[0]; 
#endif /* ROLL_LEFT */
        if (strchr(BADNICKCHARS, tmp))
          tmp = '_';
        rolls++;
#ifdef ROLL_RIGHT
        for (i = (len - 1); i > 0; i--)
          botname[i] = botname[i - 1];
        botname[0] = tmp;
#endif /* ROLL_RIGHT */
#ifdef ROLL_LEFT
        for (i = 0; i < (len - 1); i++)
          botname[i] = botname[i + 1];
        botname[len - 1] = tmp; 
#endif /* ROLL_LEFT */
        botname[len] = 0;
    } else {
      /* we've tried ALTCHARS, and rolled the nick, just use random chars now */
      altnick_char = 'a' + randint(26);
    }

    if (use_chr && altnick_char) {
      botname[len - 1] = altnick_char;
      botname[len] = 0;
    }
  }

  putlog(LOG_SERV, "*", IRC_BOTNICKINUSE, botname);
  dprintf(DP_SERVER, "NICK %s\n", botname);
  return 0;
}

/* Check for tcl-bound msg command, return 1 if found
 *
 * msg: proc-name <nick> <user@host> <handle> <args...>
 */

static void check_bind_msg(char *cmd, char *nick, char *uhost, struct userrec *u, char *args)
{
  struct flag_record fr = {FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0 };
  int x;

  get_user_flagrec(u, &fr, NULL);
  x = check_bind(BT_msg, cmd, &fr, nick, uhost, u, args);

  if (x & BIND_RET_LOG) 
    putlog(LOG_CMDS, "*", "(%s!%s) !%s! %s %s", nick, uhost, u ? u->handle : "*" , cmd, args);
  else if (x == 0)
    putlog(LOG_MSGS, "*", "[%s!%s] %s %s", nick, uhost, cmd, args);
}

static int check_bind_msgc(char *cmd, char *nick, char *from, struct userrec *u, char *args)
{
  struct flag_record fr = {FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0 };
  int x = 0;

  get_user_flagrec(u, &fr, NULL);
  x = check_bind(BT_msgc, cmd, &fr, nick, from, u, NULL, args);

  if (x & BIND_RET_LOG)
    putlog(LOG_CMDS, "*", "(%s!%s) !%s! %c%s %s", nick, from, u ? u->handle : "*", cmdprefix, cmd, args);

  if (x & BIND_RET_BREAK) return(1);
  return(0);
}

/* Return 1 if processed.
 */
static int check_bind_raw(char *from, char *code, char *msg)
{
  char *p1 = NULL, *p2 = NULL, *myfrom = NULL, *mymsg = NULL;
  int ret = 0;

  myfrom = p1 = strdup(from);
  mymsg = p2 = strdup(msg);

  ret = check_bind(BT_raw, code, NULL, myfrom, mymsg);
  free(p1);
  free(p2);
  return ret;
}


int check_bind_ctcpr(char *nick, char *uhost, struct userrec *u,
                           char *dest, char *keyword, char *args,
                           bind_table_t *table)
{
  struct flag_record fr = {FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0 };
  get_user_flagrec(u, &fr, NULL);
  return check_bind(table, keyword, &fr, nick, uhost, u, dest, keyword, args);
}


inline int match_my_nick(char *nick)
{
  return (!rfc_casecmp(nick, botname));
}

/* 001: welcome to IRC (use it to fix the server name)
 */
static int got001(char *from, char *msg)
{
  struct server_list *x = NULL;
  int i;
  struct chanset_t *chan = NULL;

  server_online = now;
  checked_hostmask = 0;
  fixcolon(msg);
  /* Ok...param #1 of 001 = what server thinks my nick is */
  strncpyz(botname, msg, NICKLEN);
  altnick_char = 0;
  strncpyz(cursrvname, from, sizeof(cursrvname));

  dprintf(DP_SERVER, "WHOIS %s\n", botname); /* get user@host */
  dprintf(DP_SERVER, "MODE %s +iws\n", botname);
  x = serverlist;
  if (x == NULL)
    return 0;			/* Uh, no server list */

  for (chan = chanset; chan; chan = chan->next) {
    chan->status &= ~(CHAN_ACTIVE | CHAN_PEND);
    if (shouldjoin(chan))
      dprintf(DP_SERVER, "JOIN %s %s\n", (chan->name[0]) ? chan->name : chan->dname,
                                         chan->channel.key[0] ? chan->channel.key : chan->key_prot);
  }
  if (egg_strcasecmp(from, dcc[servidx].host)) {
    putlog(LOG_MISC, "*", "(%s claims to be %s; updating server list)", dcc[servidx].host, from);
    for (i = curserv; i > 0 && x != NULL; i--)
      x = x->next;
    if (x == NULL) {
      putlog(LOG_MISC, "*", "Invalid server list!");
      return 0;
    }
    if (x->realname)
      free(x->realname);
    if (strict_servernames == 1) {
      x->realname = NULL;
      if (x->name)
	free(x->name);
      x->name = strdup(from);
    } else {
      x->realname = strdup(from);
    }
  }
  return 0;
}

/* <server> 005 <to> <option> <option> <... option> :are supported by this server */
static int
got005(char *from, char *msg)
{
  char *tmp = NULL;

  newsplit(&msg); /* nick */

  while ((tmp = newsplit(&msg))[0]) {
    char *p = NULL;

    if ((p = strchr(tmp, '=')))
      *p++ = 0;
    if (!egg_strcasecmp(tmp, ":are"))
      break;
    else if (!egg_strcasecmp(tmp, "MODES"))
      modesperline = atoi(p);
    else if (!egg_strcasecmp(tmp, "NICKLEN"))
      nick_len = atoi(p);
    else if (!egg_strcasecmp(tmp, "NETWORK"))
      strncpyz(curnetwork, p, 120);
    else if (!egg_strcasecmp(tmp, "PENALTY"))
      use_penalties = 1;
    else if (!egg_strcasecmp(tmp, "WHOX"))
      use_354 = 1;
    else if (!egg_strcasecmp(tmp, "EXCEPTS"))
      use_exempts = 1;
    else if (!egg_strcasecmp(tmp, "INVEX"))
      use_invites = 1;
    else if (!egg_strcasecmp(tmp, "MAXBANS")) {
      max_bans = atoi(p);
      max_modes = max_bans;
      max_exempts = max_bans;
      max_invites = max_bans;
    }
    else if (!egg_strcasecmp(tmp, "MAXLIST")) {
      char *p2 = NULL;
      
      if ((p2 = strchr(p, ':'))) {
        *p2++ = 0;
        max_modes = atoi(p2);
        if (strchr(p, 'e'))
          max_exempts = max_modes;
        if (strchr(p, 'b'))
          max_bans = max_modes;
        if (strchr(p, 'I'))
          max_invites = max_modes;
      
      }
    }
    else if (!egg_strcasecmp(tmp, "CASEMAPPING")) {
      /* we are default set to rfc1459, so only switch if NOT rfc1459 */
      if (egg_strcasecmp(p, "rfc1459")) {
        rfc_casecmp = egg_strcasecmp;
        rfc_toupper = toupper;
      }
    }
  }
  return 0;
}

/* Got 442: not on channel
 */
static int got442(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;
  struct server_list *x = NULL;
  int i;

  for (x = serverlist, i = 0; x; x = x->next, i++)
    if (i == curserv) {
      if (egg_strcasecmp(from, x->realname ? x->realname : x->name))
	return 0;
      break;
    }
  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (chan)
    if (shouldjoin(chan)) {
      putlog(LOG_MISC, chname, IRC_SERVNOTONCHAN, chname);
      clear_channel(chan, 1);
      chan->status &= ~CHAN_ACTIVE;
      dprintf(DP_MODE, "JOIN %s %s\n", chan->name,
	      chan->channel.key[0] ? chan->channel.key : chan->key_prot);
    }

  return 0;
}

/* Close the current server connection.
 */
void nuke_server(char *reason)
{
  if (serv >= 0 && servidx > 0) {
    if (reason)
      dprintf(servidx, "QUIT :%s\n", reason);

    sleep(2);
    disconnect_server(servidx, DO_LOST);
  }
}

char ctcp_reply[1024] = "";

static int lastmsgs[FLOOD_GLOBAL_MAX];
static char lastmsghost[FLOOD_GLOBAL_MAX][81];
static time_t lastmsgtime[FLOOD_GLOBAL_MAX];

/* Do on NICK, PRIVMSG, NOTICE and JOIN.
 */
static int detect_flood(char *floodnick, char *floodhost, char *from, int which)
{
  char *p = NULL, ftype[10] = "", h[1024] = "";
  struct userrec *u = NULL;
  int thr = 0, lapse = 0, atr;

  u = get_user_by_host(from);
  atr = u ? u->flags : 0;
  if ((u && u->bot) || (atr & USER_NOFLOOD))
    return 0;

  if (findauth(floodhost) > -1) 
    return 0;

  /* Determine how many are necessary to make a flood */
  switch (which) {
  case FLOOD_PRIVMSG:
  case FLOOD_NOTICE:
    thr = flud_thr;
    lapse = flud_time;
    strcpy(ftype, "msg");
    break;
  case FLOOD_CTCP:
    thr = flud_ctcp_thr;
    lapse = flud_ctcp_time;
    strcpy(ftype, "ctcp");
    break;
  }
  if ((thr == 0) || (lapse == 0))
    return 0;			/* No flood protection */
  /* Okay, make sure i'm not flood-checking myself */
  if (match_my_nick(floodnick))
    return 0;
  if (!egg_strcasecmp(floodhost, botuserhost))
    return 0;			/* My user@host (?) */
  p = strchr(floodhost, '@');
  if (p) {
    p++;
    if (egg_strcasecmp(lastmsghost[which], p)) {	/* New */
      strcpy(lastmsghost[which], p);
      lastmsgtime[which] = now;
      lastmsgs[which] = 0;
      return 0;
    }
  } else
    return 0;			/* Uh... whatever. */

  if (lastmsgtime[which] < now - lapse) {
    /* Flood timer expired, reset it */
    lastmsgtime[which] = now;
    lastmsgs[which] = 0;
    return 0;
  }
  lastmsgs[which]++;
  if (lastmsgs[which] >= thr) {	/* FLOOD */
    /* Reset counters */
    lastmsgs[which] = 0;
    lastmsgtime[which] = 0;
    lastmsghost[which][0] = 0;
    u = get_user_by_host(from);
    /* Private msg */
    simple_sprintf(h, "*!*@%s", p);
    putlog(LOG_MISC, "*", IRC_FLOODIGNORE1, p);
    addignore(h, conf.bot->nick, (which == FLOOD_CTCP) ? "CTCP flood" : "MSG/NOTICE flood", now + (60 * ignore_time));
  }
  return 0;
}

/* Check for more than 8 control characters in a line.
 * This could indicate: beep flood CTCP avalanche.
 */
int detect_avalanche(char *msg)
{
  int count = 0;
  unsigned char *p = NULL;

  for (p = (unsigned char *) msg; (*p) && (count < 8); p++)
    if ((*p == 7) || (*p == 1))
      count++;
  if (count >= 8)
    return 1;
  else
    return 0;
}

/* Got a private message.
 */
static int gotmsg(char *from, char *msg)
{
  char *to = NULL, buf[UHOSTLEN] = "", *nick = NULL, ctcpbuf[512] = "", *uhost = buf, 
       *ctcp = NULL, *p = NULL, *p1 = NULL, *code = NULL;
  struct userrec *u = NULL;
  int ctcp_count = 0, i = 0, ignoring = 0;

  if (msg[0] && ((strchr(CHANMETA, *msg) != NULL) ||
     (*msg == '@')))           /* Notice to a channel, not handled here */
    return 0;

  ignoring = match_ignore(from);
  to = newsplit(&msg);
  fixcolon(msg);
  /* Only check if flood-ctcp is active */
  strcpy(uhost, from);
  nick = splitnick(&uhost);
  if (flud_ctcp_thr && detect_avalanche(msg)) {
    if (!ignoring) {
      putlog(LOG_MODES, "*", "Avalanche from %s - ignoring", from);
      p = strchr(uhost, '@');
      if (p != NULL)
	p++;
      else
	p = uhost;
      simple_sprintf(ctcpbuf, "*!*@%s", p);
      addignore(ctcpbuf, conf.bot->nick, "ctcp avalanche", now + (60 * ignore_time));
    }
  }

  /* Check for CTCP: */
  ctcp_reply[0] = 0;
  p = strchr(msg, 1);
  while ((p != NULL) && (*p)) {
    p++;
    p1 = p;
    while ((*p != 1) && (*p != 0))
      p++;
    if (*p == 1) {
      *p = 0;
      ctcp = strcpy(ctcpbuf, p1);
      strcpy(p1 - 1, p + 1);
      if (!ignoring)
        detect_flood(nick, uhost, from, strncmp(ctcp, "ACTION ", 7) ? FLOOD_CTCP : FLOOD_PRIVMSG);
      /* Respond to the first answer_ctcp */
      p = strchr(msg, 1);
      if (ctcp_count < answer_ctcp) {
        ctcp_count++;
	if (ctcp[0] != ' ') {
	  code = newsplit(&ctcp);
	  if ((to[0] == '$') || strchr(to, '.')) {
	    if (!ignoring) {
	      /* Don't interpret */
	      putlog(LOG_PUBLIC, to, "CTCP %s: %s from %s (%s) to %s", code, ctcp, nick, uhost, to);
            }
          } else {
	    u = get_user_by_host(from);
	    if (!ignoring || trigger_on_ignore) {
	      if (check_bind_ctcp(nick, uhost, u, to, code, ctcp) == BIND_RET_LOG && !ignoring) {
                if (!strcmp(code, "DCC")) {
                  /* If it gets this far unhandled, it means that
                   * the user is totally unknown.
                   */
                  code = newsplit(&ctcp);
                  if (!strcmp(code, "CHAT")) {
                    if (!quiet_reject) {
                      if (u)
                        dprintf(DP_HELP, "NOTICE %s :%s\n", nick, "I'm not accepting call at the moment.");
                       else
                        dprintf(DP_HELP, "NOTICE %s :%s\n", nick, DCC_NOSTRANGERS);
                    }
                    if (!ischanhub())
                      putlog(LOG_MISC, "*", "%s: %s", DCC_REFUSEDNC, from);
                    else
                      putlog(LOG_MISC, "*", "%s: %s", DCC_REFUSED, from);
                  } else {
                    putlog(LOG_MISC, "*", "Refused DCC %s: %s", code, from);
                  }
                } else if (!strcmp(code, "CHAT")) {
                  if (!quiet_reject) {
                    if (u)
                      dprintf(DP_HELP, "NOTICE %s :%s\n", nick, "I'm not accepting call at the moment.");
                    else
                      dprintf(DP_HELP, "NOTICE %s :%s\n", nick, DCC_NOSTRANGERS);
                  }
                  if (!ischanhub())
                    putlog(LOG_MISC, "*", "%s: %s", DCC_REFUSEDNC, from);
                  else
                    putlog(LOG_MISC, "*", "%s: %s", DCC_REFUSED, from);
                }
	      }

	      if (!strcmp(code, "ACTION")) {
                putlog(LOG_MSGS, "*", "Action to %s: %s %s", to, nick, ctcp);
              } else {
                putlog(LOG_MSGS, "*", "CTCP %s: %s from %s (%s)", code, ctcp, nick, uhost);
              }			/* I love a good close cascade ;) */
	    }
	  }
	}
      }
    }
  }
  /* Send out possible ctcp responses */
  if (ctcp_reply[0]) {
    if (ctcp_mode != 2) {
      dprintf(DP_HELP, "NOTICE %s :%s\n", nick, ctcp_reply);
    } else {
      if (now - last_ctcp > flud_ctcp_time) {
        dprintf(DP_HELP, "NOTICE %s :%s\n", nick, ctcp_reply);
	count_ctcp = 1;
      } else if (count_ctcp < flud_ctcp_thr) {
        dprintf(DP_HELP, "NOTICE %s :%s\n", nick, ctcp_reply);
	count_ctcp++;
      }
      last_ctcp = now;
    }
  }
  if (msg[0]) {
    if ((to[0] == '$') || (strchr(to, '.') != NULL)) {
      /* Msg from oper */
      if (!ignoring) {
	detect_flood(nick, uhost, from, FLOOD_PRIVMSG);
	/* Do not interpret as command */
	putlog(LOG_MSGS | LOG_SERV, "*", "[%s!%s to %s] %s",nick, uhost, to, msg);
      }
    } else {
      char *my_code = NULL;
      struct userrec *my_u = NULL;

      detect_flood(nick, uhost, from, FLOOD_PRIVMSG);
      my_u = get_user_by_host(from);
      my_code = newsplit(&msg);
      rmspace(msg);
      i = findauth(uhost);
      /* is it a cmd? */

      if (my_code && my_code[0] && my_code[1] && i > -1 && auth[i].authed && my_code[0] == cmdprefix) {
        my_code++;
        my_u = auth[i].user;

        if (check_bind_msgc(my_code, nick, uhost, my_u, msg))
          auth[i].atime = now;
        else
          putlog(LOG_MSGS, "*", "[%s] %c%s %s", from, cmdprefix, my_code, msg);
      } else if ((my_code[0] != cmdprefix || !my_code[1] || i == -1 || !(auth[i].authed))) {
        if (!ignoring) {
          int doit = 1;
          if (!egg_strcasecmp(my_code, "op") || !egg_strcasecmp(my_code, "pass") || !egg_strcasecmp(my_code, "invite") 
              || !egg_strcasecmp(my_code, "ident")
               || !egg_strcasecmp(my_code, msgop) || !egg_strcasecmp(my_code, msgpass) 
               || !egg_strcasecmp(my_code, msginvite) || !egg_strcasecmp(my_code, msgident)) {
            char buf2[10] = "";

            doit = 0;
            if (!egg_strcasecmp(my_code, msgop))
              sprintf(buf2, "op");
            else if (!egg_strcasecmp(my_code, msgpass))
              sprintf(buf2, "pass");
            else if (!egg_strcasecmp(my_code, msginvite))
              sprintf(buf2, "invite");
            else if (!egg_strcasecmp(my_code, msgident))
              sprintf(buf2, "ident");
            if (buf[0])
              check_bind_msg(buf2, nick, uhost, my_u, msg);
          }
          if (doit)
            check_bind_msg(my_code, nick, uhost, my_u, msg);
        }
      }
    }
  }
  return 0;
}

/* Got a private notice.
 */
static int gotnotice(char *from, char *msg)
{
  char *to = NULL, *nick = NULL, ctcpbuf[512] = "", *p = NULL, *p1 = NULL, buf[512] = "", 
       *uhost = buf, *ctcp = NULL, *ctcpmsg = NULL, *ptr = NULL;
  struct userrec *u = NULL;
  int ignoring;

  if (msg[0] && ((strchr(CHANMETA, *msg) != NULL) ||
      (*msg == '@')))           /* Notice to a channel, not handled here */
    return 0;
  ignoring = match_ignore(from);
  to = newsplit(&msg);
  fixcolon(msg);
  strcpy(uhost, from);
  nick = splitnick(&uhost);
  if (flud_ctcp_thr && detect_avalanche(msg)) {
    /* Discard -- kick user if it was to the channel */
    if (!ignoring)
      putlog(LOG_MODES, "*", "Avalanche from %s", from);
    return 0;
  }
  /* Check for CTCP: */
  ctcpmsg = ptr = strdup(msg);
  p = strchr(ctcpmsg, 1);
  while ((p != NULL) && (*p)) {
    p++;
    p1 = p;
    while ((*p != 1) && (*p != 0))
      p++;
    if (*p == 1) {
      *p = 0;
      ctcp = strcpy(ctcpbuf, p1);
      strcpy(p1 - 1, p + 1);
      if (!ignoring)
	detect_flood(nick, uhost, from, FLOOD_CTCP);
      p = strchr(ctcpmsg, 1);
      if (ctcp[0] != ' ') {
	char *code = newsplit(&ctcp);

	if ((to[0] == '$') || strchr(to, '.')) {
	  if (!ignoring)
	    putlog(LOG_PUBLIC, "*",
		   "CTCP reply %s: %s from %s (%s) to %s", code, ctcp, nick, uhost, to);
	} else {
	  u = get_user_by_host(from);
	  if (!ignoring || trigger_on_ignore) {
            check_bind_ctcr(nick, uhost, u, to, code, ctcp);
	    if (!ignoring)
	      /* Who cares? */
	      putlog(LOG_MSGS, "*", "CTCP reply %s: %s from %s (%s) to %s", code, ctcp, nick, uhost, to);
	  }
	}
      }
    }
  }
  free(ptr);
  if (msg[0]) {
    if (((to[0] == '$') || strchr(to, '.')) && !ignoring) {
      detect_flood(nick, uhost, from, FLOOD_NOTICE);
      putlog(LOG_MSGS | LOG_SERV, "*", "-%s (%s) to %s- %s", nick, uhost, to, msg);
    } else {
      /* Server notice? */
      if ((nick[0] == 0) || (uhost[0] == 0)) {
	/* Hidden `250' connection count message from server */
	if (strncmp(msg, "Highest connection count:", 25))
	  putlog(LOG_SERV, "*", "-NOTICE- %s", msg);
      } else {
        detect_flood(nick, uhost, from, FLOOD_NOTICE);
        u = get_user_by_host(from);
        if (!ignoring)
  	      putlog(LOG_MSGS, "*", "-%s (%s)- %s", nick, uhost, msg);
      }
    }
  }
  return 0;
}

/* WALLOPS: oper's nuisance
 */
static int gotwall(char *from, char *msg)
{
  fixcolon(msg);
  putlog(LOG_WALL, "*", "!%s! %s", from, msg);
  return 0;
}

static void server_10secondly()
{
  if (server_online && keepnick) {
    /* NOTE: now that botname can but upto NICKLEN bytes long,
     * check that it's not just a truncation of the full nick.
     */
    if (strncmp(botname, origbotname, strlen(botname))) {
      /* See if my nickname is in use and if if my nick is right.  */
      dprintf(DP_SERVER, "ISON :%s %s\n", botname, origbotname);
    }
  }
}
/* Called once a minute... but if we're the only one on the
 * channel, we only wanna send out "lusers" once every 5 mins.
 */
static void minutely_checks()
{
  /* Only check if we have already successfully logged in.  */
  if (server_online) {
    static int count = 4;
    int ok = 0;
    struct chanset_t *chan = NULL;

    for (chan = chanset; chan; chan = chan->next) {
      if (channel_active(chan) && chan->channel.members == 1) {
        ok = 1;
        break;
      }
    }
    if (!ok)
      return;
    count++;
    if (count >= 5) {
      dprintf(DP_SERVER, "LUSERS\n");
      count = 0;
    }
  }
}

/* Pong from server.
 */
static int gotpong(char *from, char *msg)
{
  newsplit(&msg);
  fixcolon(msg);		/* Scrap server name */

  server_lag = now - my_atoul(msg);
  if (server_lag > 99999) {
    /* IRCnet lagmeter support by drummer */
    server_lag = now - lastpingtime;
  }
  return 0;
}

/* This is a reply on ISON :<current> <orig> [<alt>]
 */
static void got303(char *from, char *msg)
{
  char *tmp = NULL;
  int ison_orig = 0;

  if (!keepnick ||
      !strncmp(botname, origbotname, strlen(botname))) {
    return;
  }
  newsplit(&msg);
  fixcolon(msg);
  tmp = newsplit(&msg);
  if (tmp[0] && !rfc_casecmp(botname, tmp)) {
    while ((tmp = newsplit(&msg))[0]) { /* no, it's NOT == */
      if (!rfc_casecmp(tmp, origbotname))
        ison_orig = 1;
    }
    if (!ison_orig) {
      if (!nick_juped)
        putlog(LOG_MISC, "*", IRC_GETORIGNICK, origbotname);
      dprintf(DP_SERVER, "NICK %s\n", origbotname);
    }
  }
}

/* 432 : Bad nickname
 */
static int got432(char *from, char *msg)
{
  char *erroneus = NULL;

  newsplit(&msg);
  erroneus = newsplit(&msg);
  if (server_online)
    putlog(LOG_MISC, "*", "NICK IS INVALID: %s (keeping '%s').", erroneus,
	   botname);
  else {
    putlog(LOG_MISC, "*", IRC_BADBOTNICK);
    if (!keepnick) {
      makepass(erroneus);
      erroneus[NICKMAX] = 0;
      dprintf(DP_MODE, "NICK %s\n", erroneus);
    }
    return 0;
  }
  return 0;
}

/* 433 : Nickname in use
 * Change nicks till we're acceptable or we give up
 */
static int got433(char *from, char *msg)
{
  /* We are online and have a nickname, we'll keep it */
  if (server_online) {
    char *tmp = NULL;

    newsplit(&msg);
    tmp = newsplit(&msg);
    putlog(LOG_MISC, "*", "NICK IN USE: %s (keeping '%s').", tmp, botname);
    nick_juped = 0;
    return 0;
  }
  gotfake433(from);
  return 0;
}

/* 437 : Nickname juped (IRCnet)
 */
static int got437(char *from, char *msg)
{
  char *s = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);
  s = newsplit(&msg);
  if (s[0] && (strchr(CHANMETA, s[0]) != NULL)) {
    chan = findchan(s);
    if (chan) {
      if (chan->status & CHAN_ACTIVE) {
	putlog(LOG_MISC, "*", IRC_CANTCHANGENICK, s);
      } else {
	if (!channel_juped(chan)) {
	  putlog(LOG_MISC, "*", IRC_CHANNELJUPED, s);
	  chan->status |= CHAN_JUPED;
	}
      }
    }
  } else if (server_online) {
    if (!nick_juped)
      putlog(LOG_MISC, "*", "NICK IS JUPED: %s (keeping '%s').", s, botname);
    if (!rfc_casecmp(s, origbotname))
      nick_juped = 1;
  } else {
    putlog(LOG_MISC, "*", "%s: %s", IRC_BOTNICKJUPED, s);
    gotfake433(from);
  }
  return 0;
}

/* 438 : Nick change too fast
 */
static int got438(char *from, char *msg)
{
  newsplit(&msg);
  newsplit(&msg);
  fixcolon(msg);
  putlog(LOG_MISC, "*", "%s", msg);
  return 0;
}

static int got451(char *from, char *msg)
{
  /* Usually if we get this then we really messed up somewhere
   * or this is a non-standard server, so we log it and kill the socket
   * hoping the next server will work :) -poptix
   */
  /* Um, this does occur on a lagged anti-spoof server connection if the
   * (minutely) sending of joins occurs before the bot does its ping reply.
   * Probably should do something about it some time - beldin
   */
  putlog(LOG_MISC, "*", "%s says I'm not registered, trying next one.", from);
  nuke_server(IRC_NOTREGISTERED2);
  return 0;
}

/* Got error notice
 */
static int goterror(char *from, char *msg)
{
 /* FIXME: fixcolon doesn't do what we need here, this is a temp fix
  * fixcolon(msg);
  */
  if (msg[0] == ':')
    msg++;       
  putlog(LOG_SERV, "*", "-ERROR from server- %s", msg);
  if (serverror_quit) {
    putlog(LOG_SERV, "*", "Disconnecting from server.");
    nuke_server("Bah, stupid error messages.");
  }
  return 1;
}

/* Got nick change.
 */
static int gotnick(char *from, char *msg)
{
  char *nick = NULL, *buf = NULL, *buf_ptr = NULL;
  struct userrec *u = NULL;

  buf = buf_ptr = strdup(from);
  u = get_user_by_host(buf);
  nick = splitnick(&buf);
  fixcolon(msg);

  if (match_my_nick(nick)) {
    /* Regained nick! */
    strncpyz(botname, msg, NICKLEN);
    altnick_char = 0;
    if (!strcmp(msg, origbotname)) {
      putlog(LOG_SERV | LOG_MISC, "*", "Regained nickname '%s'.", msg);
      nick_juped = 0;
    } else if (keepnick && strcmp(nick, msg)) {
      putlog(LOG_SERV | LOG_MISC, "*", "Nickname changed to '%s'???", msg);
      if (!rfc_casecmp(nick, origbotname)) {
        putlog(LOG_MISC, "*", IRC_GETORIGNICK, origbotname);
        dprintf(DP_SERVER, "NICK %s\n", origbotname);
      }
    } else
      putlog(LOG_SERV | LOG_MISC, "*", "Nickname changed to '%s'???", msg);
  } else if ((keepnick) && (rfc_casecmp(nick, msg))) {
    /* Only do the below if there was actual nick change, case doesn't count */
    if (!rfc_casecmp(nick, origbotname)) {
      putlog(LOG_MISC, "*", IRC_GETORIGNICK, origbotname);
      dprintf(DP_SERVER, "NICK %s\n", origbotname);
    }
  }
  free(buf_ptr);
  return 0;
}

static int gotmode(char *from, char *msg)
{
  char *ch = NULL, *buf = NULL, *buf_ptr = NULL;

  buf_ptr = buf = strdup(msg);

  ch = newsplit(&buf);
  /* Usermode changes? */
  if (strchr(CHANMETA, ch[0]) == NULL) {
    if (match_my_nick(ch) && check_mode_r) {
      /* umode +r? - D0H dalnet uses it to mean something different */
      fixcolon(buf);
      if ((buf[0] == '+') && strchr(buf, 'r')) {
	putlog(LOG_MISC | LOG_JOIN, "*", "%s has me i-lined (jumping)", dcc[servidx].host);
	nuke_server("i-lines suck");
      }
    }
  }
  free(buf_ptr);
  return 0;
}


static void disconnect_server(int idx, int dolost)
{
  if ((serv != dcc[idx].sock) && serv >= 0)
    killsock(serv);
  if (dcc[idx].sock >= 0)
    killsock(dcc[idx].sock);

  dcc[idx].sock = -1;
  serv = -1;
  servidx = -1;
  server_online = 0;
  botuserhost[0] = 0;
  if (dolost) {
    trying_server = 0;
    lostdcc(idx);
  }
}

static void eof_server(int idx)
{
  putlog(LOG_SERV, "*", "Disconnected from %s", dcc[idx].host);
  if (ischanhub() && auth_total > 0) {
    int i = 0;

    putlog(LOG_DEBUG, "*", "Removing %d auth entries.", auth_total);
    for (i = 0; i < auth_total; i++)
      removeauth(i);  
  }
  disconnect_server(idx, DO_LOST);
}

static void display_server(int idx, char *buf)
{
  sprintf(buf, "%s  (lag: %d)", trying_server ? "conn" : "serv", server_lag);
}

static void connect_server(void);

static void kill_server(int idx, void *x)
{
  struct chanset_t *chan = NULL;

  disconnect_server(idx, NO_LOST);	/* eof_server will lostdcc() it. */
  for (chan = chanset; chan; chan = chan->next)
     clear_channel(chan, 1);
  /* A new server connection will be automatically initiated in
     about 2 seconds. */
}

static void timeout_server(int idx)
{
  putlog(LOG_SERV, "*", "Timeout: connect to %s", dcc[idx].host);
  disconnect_server(idx, DO_LOST);
}

static void server_activity(int, char *, int);

static struct dcc_table SERVER_SOCKET =
{
  "SERVER",
  0,
  eof_server,
  server_activity,
  NULL,
  timeout_server,
  display_server,
  kill_server,
  NULL,
  NULL
};

int isop(char *mode)
{
  int state = 0, cnt = 0;
  char *p = NULL;

  p = mode;
  while ((*p) && (*p != ' ')) {
    if (*p == '-')
      state = 1;
    else if (*p == '+')
      state = 0;
    else if ((!state) && (*p == 'o'))
      cnt++;
    p++;
  }
  return (cnt >= 1);
}

int ismdop(char *mode)
{
  int state = 0, cnt = 0;
  char *p = NULL;

  p = mode;
  while ((*p) && (*p != ' ')) {
    if (*p == '-')
      state = 1;
    else if (*p == '+')
      state = 0;
    else if ((state) && (*p == 'o'))
      cnt++;
    p++;
  }
  return (cnt >= 3);
}

static void server_activity(int idx, char *msg, int len)
{
  char *from = NULL, *code = NULL;

  if (trying_server) {
    strcpy(dcc[idx].nick, "(server)");
    putlog(LOG_SERV, "*", "Connected to %s", dcc[idx].host);

    trying_server = 0;
    /*
    servidx = idx;
    serv = dcc[idx].sosck;
    */
    SERVER_SOCKET.timeout_val = 0;
  }
  waiting_for_awake = 0;
  from = "";
  if (msg[0] == ':') {
    msg++;
    from = newsplit(&msg);
  }
  code = newsplit(&msg);

  if (use_console_r) {
    if (!strcmp(code, "PRIVMSG") || !strcmp(code, "NOTICE")) {
      if (!match_ignore(from))
	putlog(LOG_RAW, "@", "[@] %s %s %s", from, code, msg);
    } else
      putlog(LOG_RAW, "@", "[@] %s %s %s", from, code, msg);
  }

  /* This has GOT to go into the raw binding table, * merely because this
   * is less effecient.
  */
  check_bind_raw(from, code, msg);
}

static int gotping(char *from, char *msg)
{
  fixcolon(msg);
  dprintf(DP_MODE, "PONG :%s\n", msg);
  return 0;
}

static int gotkick(char *from, char *msg)
{
  char *nick = NULL;

  nick = from;
  if (rfc_casecmp(nick, botname))
    /* Not my kick, I don't need to bother about it. */
    return 0;
  if (use_penalties) {
    last_time += 2;
    if (debug_output)
      putlog(LOG_SRVOUT, "*", "adding 2secs penalty (successful kick)");
  }
  return 0;
}

/* Another sec penalty if bot did a whois on another server.
 */
static int got318_369(char *, char *, int);
static int whoispenalty(char *from, char *msg)
{
  struct server_list *x = serverlist;
  int i, ii;

  if (x && use_penalties) {
    i = ii = 0;
    for (; x; x = x->next) {
      if (i == curserv) {
        if ((strict_servernames == 1) || !x->realname) {
          if (strcmp(x->name, from))
            ii = 1;
        } else {
          if (strcmp(x->realname, from))
            ii = 1;
        }
      }
      i++;
    }
    if (ii) {
      last_time += 1;
      if (debug_output)
        putlog(LOG_SRVOUT, "*", "adding 1sec penalty (remote whois)");
    }
  }

  got318_369(from, msg, 0);
  return 0;
}

static void irc_whois(char *, char *, ...) __attribute__((format(printf, 2, 3)));

static void
irc_whois(char *nick, char *format, ...)
{
  char va_out[2001] = "";
  va_list va;
  int idx;

  va_start(va, format);
  egg_vsnprintf(va_out, sizeof(va_out) - 1, format, va);
  va_end(va);

  for (idx = 0; idx < dcc_total; idx++)
    if (dcc[idx].whois[0] && !rfc_casecmp(nick, dcc[idx].whois))
      dprintf(idx, "%s\n", va_out);
}

/* 311 $me nick username address * :realname */
static int got311(char *from, char *msg)
{
  char *nick = NULL, *username = NULL, *address = NULL;
  
  newsplit(&msg);
  nick = newsplit(&msg);
  username = newsplit(&msg);
  address = newsplit(&msg);
  newsplit(&msg);
  fixcolon(msg);
    
  if (match_my_nick(nick))
    egg_snprintf(botuserhost, sizeof botuserhost, "%s@%s", username, address);

  irc_whois(nick, "$b%s$b [%s@%s]", nick, username, address);
  irc_whois(nick, " ircname  : %s", msg);
  
  return 0;
}

/* 319 $me nick :channels */
static int got319(char *from, char *msg)
{
  char *nick = NULL;

  newsplit(&msg);
  nick = newsplit(&msg);
  fixcolon(msg);

  irc_whois(nick, " channels : %s", msg);

  return 0;
}

/* 312 $me nick server :text */
static int got312(char *from, char *msg)
{
  char *nick = NULL, *server = NULL;

  newsplit(&msg);
  nick = newsplit(&msg);
  server = newsplit(&msg);
  fixcolon(msg);

  irc_whois(nick, " server   : %s [%s]", server, msg);
  return 0;
}

/* 301 $me nick :away msg */
static int got301(char *from, char *msg)
{
  char *nick = NULL;

  newsplit(&msg);
  nick = newsplit(&msg);
  fixcolon(msg);

  irc_whois(nick, " away     : %s", msg);

  return 0;
}

/* 313 $me nick :server text */
static int got313(char *from, char *msg)
{
  char *nick = NULL;
  
  newsplit(&msg);
  nick = newsplit(&msg);
  fixcolon(msg);
 
  irc_whois(nick, "          : $b%s$b", msg);

  return 0;
}

/* 317 $me nick idle signon :idle-eng signon-eng */
static int got317(char *from, char *msg)
{
  char *nick = NULL, date[50] = "";
  time_t idle, signon;
  int mydays, myhours, mymins, mysecs;

  newsplit(&msg);
  nick = newsplit(&msg);
  idle = atol(newsplit(&msg));
  signon = atol(newsplit(&msg));
  fixcolon(msg);

  egg_strftime(date, sizeof date, "%c %Z", gmtime(&signon));

  mydays = idle / 86400;
  idle = idle % 86400;
  myhours = idle / 3600;
  idle = idle % 3600;
  mymins = idle / 60;
  idle = idle % 60;
  mysecs = idle;
  irc_whois(nick, " idle     : %d days %d hours %d mins %d secs [signon: %s]", mydays, myhours, mymins, mysecs, date);

  return 0;
}

static int got369(char *from, char *msg)
{
  return got318_369(from, msg, 1);
}

/* 318/319 $me nick :End of /? */
static int got318_369(char *from, char *msg, int whowas)
{
  char *nick = NULL;
  int idx;

  newsplit(&msg);
  nick = newsplit(&msg);
  fixcolon(msg);

  irc_whois(nick, "%s", msg);
  for (idx = 0; idx < dcc_total; idx++) {
    if (dcc[idx].whois[0] && !rfc_casecmp(dcc[idx].whois, nick) &&
       ((!whowas && !dcc[idx].whowas) || (whowas && dcc[idx].whowas))) {
      dcc[idx].whois[0] = 0;
      dcc[idx].whowas = 0;
    }
  }

  return 0;
}

/* 401 $me nick :text */
static int got401(char *from, char *msg)
{
  char *nick = NULL;
  int idx;

  newsplit(&msg);
  nick = newsplit(&msg);
  fixcolon(msg);
  irc_whois(nick, "%s", msg);
  for (idx = 0; idx < dcc_total; idx++)
    if (dcc[idx].whois[0] && !rfc_casecmp(dcc[idx].whois, nick))
      dcc[idx].whowas = 1;

  dprintf(DP_SERVER, "WHOWAS %s %s\n", nick, from);
 
  return 0;
}

/* 406 $me nick :text */
static int got406(char *from, char *msg)
{
  char *nick = NULL;

  newsplit(&msg);
  nick = newsplit(&msg);
  fixcolon(msg);
  irc_whois(nick, "%s", msg);
 
  return 0;
}
 
static cmd_t my_raw_binds[] =
{
  {"PRIVMSG",	"",	(Function) gotmsg,		NULL},
  {"NOTICE",	"",	(Function) gotnotice,		NULL},
  {"MODE",	"",	(Function) gotmode,		NULL},
  {"PING",	"",	(Function) gotping,		NULL},
  {"PONG",	"",	(Function) gotpong,		NULL},
  {"WALLOPS",	"",	(Function) gotwall,		NULL},
  {"001",	"",	(Function) got001,		NULL},
  {"005",	"",	(Function) got005,		NULL},
  {"303",	"",	(Function) got303,		NULL},
  {"432",	"",	(Function) got432,		NULL},
  {"433",	"",	(Function) got433,		NULL},
  {"437",	"",	(Function) got437,		NULL},
  {"438",	"",	(Function) got438,		NULL},
  {"451",	"",	(Function) got451,		NULL},
  {"442",	"",	(Function) got442,		NULL},
  {"NICK",	"",	(Function) gotnick,		NULL},
  {"ERROR",	"",	(Function) goterror,		NULL},
/* ircu2.10.10 has a bug when a client is throttled ERROR is sent wrong */
  {"ERROR:",	"",	(Function) goterror,		NULL},
  {"KICK",	"",	(Function) gotkick,		NULL},
  /* WHOIS RAWS */
  {"311", 	"", 	(Function) got311, 		NULL},	/* ident host * :realname */
  {"314",	"",	(Function) got311,		NULL},	/* "" -WHOWAS */
  {"319",	"",	(Function) got319,		NULL},	/* :#channels */
  {"312",	"",	(Function) got312,		NULL},	/* server :gecos */
  {"301",	"",	(Function) got301,		NULL},	/* :away msg */
  {"313",	"",	(Function) got313,		NULL},	/* :ircop */
  {"317",	"",	(Function) got317,		NULL},	/* idle, signon :idle-eng, signon-eng */
  {"401",	"",	(Function) got401,		NULL},
  {"406",	"",	(Function) got406,		NULL},
  {"318",	"",	(Function) whoispenalty,	NULL},	/* :End of /WHOIS */
  {"369",	"",	(Function) got369,		NULL},	/* :End of /WHOWAS */
  {NULL,	NULL,	NULL,				NULL}
};

static void server_resolve_success(int);
static void server_resolve_failure(int);

/* Hook up to a server
 */
static void connect_server(void)
{
  char pass[121] = "", botserver[UHOSTLEN] = "";
  int newidx;
  port_t botserverport = 0;

  waiting_for_awake = 0;
  /* trying_server = now; */
  empty_msgq();

  if (newserverport) {		/* cmd_jump was used; connect specified server */
    curserv = -1;		/* Reset server list */
    strcpy(botserver, newserver);
    botserverport = newserverport;
    strcpy(pass, newserverpass);
    newserver[0] = newserverport = newserverpass[0] = 0;
  } 

  if (!cycle_time) {
    struct chanset_t *chan = NULL;
    struct server_list *x = serverlist;

    if (!x)
      return;
 
    trying_server = now;

    newidx = new_dcc(&DCC_DNSWAIT, sizeof(struct dns_info));
    if (newidx < 0) {
      putlog(LOG_SERV, "*", "NO MORE DCC CONNECTIONS -- Can't create server connection.");
      trying_server = 0;
      return;
    }

    next_server(&curserv, botserver, &botserverport, pass);
    putlog(LOG_SERV, "*", "Trying server %s:%d", botserver, botserverport);

    dcc[newidx].port = botserverport;
    strcpy(dcc[newidx].nick, "(server)");
    strncpyz(dcc[newidx].host, botserver, UHOSTLEN);

    botuserhost[0] = 0;

    nick_juped = 0;
    for (chan = chanset; chan; chan = chan->next)
      chan->status &= ~CHAN_JUPED;

    dcc[newidx].timeval = now;
    dcc[newidx].sock = -1;
    dcc[newidx].u.dns->host = calloc(1, strlen(botserver) + 1);
    strcpy(dcc[newidx].u.dns->host, botserver);
    dcc[newidx].u.dns->cbuf = calloc(1, strlen(pass) + 1);
    strcpy(dcc[newidx].u.dns->cbuf, pass);
    dcc[newidx].u.dns->dns_success = server_resolve_success;
    dcc[newidx].u.dns->dns_failure = server_resolve_failure;
    dcc[newidx].u.dns->dns_type = RES_IPBYHOST;
    dcc[newidx].u.dns->type = &SERVER_SOCKET;


    cycle_time = 15;		/* wait 15 seconds before attempting next server connect */

    /* I'm resolving... don't start another server connect request */
    resolvserv = 1;
    /* Resolve the hostname. */
#ifdef USE_IPV6
    server_resolve_success(newidx);
#else
    dcc_dnsipbyhost(botserver);
#endif /* USE_IPV6 */
  }
}

static void server_resolve_failure(int idx)
{
  putlog(LOG_SERV, "*", "Failed connect to %s (DNS lookup failed)", dcc[idx].host);
  resolvserv = 0;
  trying_server = 0;
  lostdcc(idx);
}

static void server_resolve_success(int idx)
{
  char s[121] = "", pass[121] = "";

  resolvserv = 0;
  dcc[idx].addr = dcc[idx].u.dns->ip;
  strcpy(pass, dcc[idx].u.dns->cbuf);
  changeover_dcc(idx, &SERVER_SOCKET, 0);
  identd_open();
#ifdef USE_IPV6
  serv = open_telnet(dcc[idx].host, dcc[idx].port);
#else
  serv = open_telnet(iptostr(htonl(dcc[idx].addr)), dcc[idx].port);
#endif /* USE_IPV6 */
  if (serv < 0) {
    neterror(s);

    putlog(LOG_SERV, "*", "Failed connect to %s (%s)", dcc[idx].host, s);
    trying_server = 0;
    lostdcc(idx);
  } else {
    int i = 1;

    /* set these now so if we fail disconnect_server() can cleanup right. */
    dcc[idx].sock = serv;
    servidx = idx;
    setsockopt(serv, 6, TCP_NODELAY, &i, sizeof(int));
    /* Queue standard login */
    dcc[idx].timeval = now;
    SERVER_SOCKET.timeout_val = &server_timeout;
    /* Another server may have truncated it, so use the original */
    strcpy(botname, origbotname);
    /* Start alternate nicks from the beginning */
    altnick_char = 0;

    if (pass[0]) 
      dprintf(DP_MODE, "PASS %s\n", pass);
    dprintf(DP_MODE, "NICK %s\n", botname);
    dprintf(DP_MODE, "USER %s localhost %s :%s\n", botuser, dcc[idx].host, botrealname);
    /* Wait for async result now */
  }
}
#endif /* LEAF */
