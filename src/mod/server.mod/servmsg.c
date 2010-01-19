/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2008 Bryan Drewery
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
 * servmsg.c -- part of server.mod
 *
 */


#include <netinet/tcp.h>

char cursrvname[120] = "";
char curnetwork[120] = "";
static time_t last_ctcp    = (time_t) 0L;
static int    count_ctcp   = 0;
char   altnick_char = 0;
unsigned int rolls = 0;
#define ROLL_RIGHT
#undef ROLL_LEFT
static void rotate_nick(char *nick, char *orignick)
{
  size_t len = strlen(nick);
  int use_chr = 1;

#ifdef DEBUG
  sdprintf("rotate_nick(%s, %s) rolls: %d altnick_char: %c", nick, orignick, rolls, altnick_char);
#endif
  /* First run? */
  if (altnick_char == 0 && !rolls && altchars[0]) {
    altnick_char = altchars[0];
    /* the nick is already as long as it can be. */
    if (len == (unsigned) nick_len) {
      /* make the last char the current altnick_char */
      nick[len - 1] = altnick_char;
    } else {
      /* tack it on to the end */
      nick[len] = altnick_char;
      nick[len + 1] = 0;
    }
  } else {
    char *p = NULL;

    if ((p = strchr(altchars, altnick_char)))
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
        size_t i = 0;

        altnick_char = 0;
        use_chr = 0;

        if (rolls == 0) {
          strlcpy(nick, orignick, sizeof(botname));
          len = strlen(nick);
        }
#ifdef ROLL_RIGHT
        tmp = nick[len - 1];
#endif /* ROLL_RIGHT */
#ifdef ROLL_LEFT
        tmp = nick[0]; 
#endif /* ROLL_LEFT */
        if (strchr(BADHANDCHARS, tmp))
          tmp = '_';
        rolls++;
#ifdef ROLL_RIGHT
        for (i = (len - 1); i > 0; i--)
          nick[i] = nick[i - 1];
        nick[0] = tmp;
#endif /* ROLL_RIGHT */
#ifdef ROLL_LEFT
        for (i = 0; i < (len - 1); i++)
          nick[i] = nick[i + 1];
        nick[len - 1] = tmp; 
#endif /* ROLL_LEFT */
        nick[len] = 0;
    } else {
      /* we've tried ALTCHARS, and rolled the nick, just use random chars now */
      altnick_char = 'a' + randint(26);
    }

    if (use_chr && altnick_char) {
      nick[len - 1] = altnick_char;
      nick[len] = 0;
    }
  }
}

//This is only called on failed nick on connect
static int gotfake433(char *nick)
{
  //Failed to get jupenick on connect, try normal nick
  if (altnick_char == 0 && jupenick[0] && !rfc_casecmp(botname, jupenick)) {
    strlcpy(botname, origbotname, NICKLEN);
  } else //Rotate on failed normal nick
    rotate_nick(botname, origbotname);
  putlog(LOG_SERV, "*", "NICK IN USE: '%s' Trying '%s'", nick, botname);
  dprintf(DP_SERVER, "NICK %s\n", botname);
  return 0;
}

/* Check for tcl-bound msg command, return 1 if found
 *
 * msg: proc-name <nick> <user@host> <handle> <args...>
 */

static void check_bind_msg(const char *cmd, char *nick, char *uhost, struct userrec *u, char *args)
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


bool match_my_nick(char *nick)
{
  return (!rfc_casecmp(nick, botname));
}

void rehash_monitor_list() {
  if (!use_monitor) return;
  dprintf(DP_SERVER, "MONITOR C\n");
  if (jupenick[0])
    dprintf(DP_SERVER, "MONITOR + %s,%s\n", jupenick, origbotname);
  else
    dprintf(DP_SERVER, "MONITOR + %s", origbotname);
}

void rehash_server(const char *servname, const char *nick)
{
  server_online = now;
  altnick_char = 0;
  strlcpy(cursrvname, servname, sizeof(cursrvname));
  if (servidx >= 0)
    curservport = dcc[servidx].port;

  if (nick && nick[0]) {
    strlcpy(botname, nick, NICKLEN);

    dprintf(DP_SERVER, "WHOIS %s\n", botname); /* get user@host */
    dprintf(DP_SERVER, "USERHOST %s\n", botname); /* get user@ip */
    dprintf(DP_SERVER, "MODE %s %s\n", botname, var_get_str_by_name("usermode"));
    rehash_monitor_list();
  }
}

void 
join_chans()
{
  for (register struct chanset_t *chan = chanset; chan; chan = chan->next) {
    if (shouldjoin(chan)) {
      chan->status &= ~(CHAN_ACTIVE | CHAN_PEND | CHAN_JOINING);
      dprintf(DP_SERVER, "JOIN %s %s\n", (chan->name[0]) ? chan->name : chan->dname,
                                         chan->channel.key[0] ? chan->channel.key : chan->key_prot);
      chan->status |= CHAN_JOINING;
    }
  }
}

/* 001: welcome to IRC (use it to fix the server name)
 */
static int got001(char *from, char *msg)
{

  fixcolon(msg);
  rehash_server(from, msg);
  /* Ok...param #1 of 001 = what server thinks my nick is */

  join_chans();

#ifdef no
  if (strcasecmp(from, dcc[servidx].host)) {
    struct server_list *x = serverlist;

    if (x == NULL)
      return 0;			/* Uh, no server list */

    putlog(LOG_MISC, "*", "(%s claims to be %s; updating server list)", dcc[servidx].host, from);
    for (int i = curserv; i > 0 && x != NULL; i--)
      x = x->next;
    if (x == NULL) {
      putlog(LOG_MISC, "*", "Invalid server list!");
      return 0;
    }
  }
#endif
  return 0;
}

/* <server> 004 <to> <servername> <version> <user modes> <channel modes> */
static int
got004(char *from, char *msg)
{
  char *tmp = NULL;

  newsplit(&msg); /* nick */
  newsplit(&msg); /* server */

  //Cache the results for later parsing if needed (after a restart)
  //Sending 'VERSION' does not work on all servers, plus this speeds
  //up a restart by removing the need to wait for the information
  if (!replaying_cache)
    dprintf(DP_CACHE, ":%s 004 . . %s", from, msg);

  tmp = newsplit(&msg);

  /* cookies won't work on ircu or Unreal or snircd */
  if (strstr(tmp, "u2.") || strstr(tmp, "Unreeal") || strstr(tmp, "snircd")) {
    putlog(LOG_DEBUG, "*", "Disabling cookies as they are not supported on %s", cursrvname);
    cookies_disabled = true;
  }

  return 0;
}

/* <server> 005 <to> <option> <option> <... option> :are supported by this server */
static int
got005(char *from, char *msg)
{
  char *tmp = NULL, *p, *p2;

  newsplit(&msg); /* nick */

  //Cache the results for later parsing if needed (after a restart)
  //Sending 'VERSION' does not work on all servers, plus this speeds
  //up a restart by removing the need to wait for the information
  if (!replaying_cache)
    dprintf(DP_CACHE, ":%s 005 . %s", from, msg);

  while ((tmp = newsplit(&msg))[0]) {
    p = NULL;

    if ((p = strchr(tmp, '=')))
      *p++ = 0;
    if (!strcasecmp(tmp, ":are"))
      break;
    else if (!strcasecmp(tmp, "MODES")) {
      modesperline = atoi(p);

      if (modesperline > MODES_PER_LINE_MAX)
        modesperline = MODES_PER_LINE_MAX;
    } else if (!strcasecmp(tmp, "NICKLEN"))
      nick_len = atoi(p);
    else if (!strcasecmp(tmp, "MONITOR")) {
      bool need_to_rehash_monitor = 0;
      if (!use_monitor && !replaying_cache) // Don't rehash if restarting. No need.
        need_to_rehash_monitor = 1;
      use_monitor = 1;
      if (need_to_rehash_monitor)
        rehash_monitor_list();
    }
    else if (!strcasecmp(tmp, "NETWORK"))
      strlcpy(curnetwork, p, 120);
    else if (!strcasecmp(tmp, "PENALTY"))
      use_penalties = 1;
    else if (!strcasecmp(tmp, "WHOX"))
      use_354 = 1;
    else if (!strcasecmp(tmp, "EXCEPTS"))
      use_exempts = 1;
    else if (!strcasecmp(tmp, "INVEX"))
      use_invites = 1;
    else if (!strcasecmp(tmp, "MAXBANS")) {
      max_bans = atoi(p);
      max_modes = max_bans;
      max_exempts = max_bans;
      max_invites = max_bans;
    }
    else if (!strcasecmp(tmp, "MAXLIST")) {
      p2 = NULL;
      
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
    else if (!strcasecmp(tmp, "CASEMAPPING")) {
      /* we are default set to rfc1459, so only switch if NOT rfc1459 */
      if (strcasecmp(p, "rfc1459")) {
        rfc_casecmp = strcasecmp;
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
  int i;
  struct server_list *x = NULL;

  for (x = serverlist, i = 0; x; x = x->next, i++)
    if (i == curserv) {
      if (strcasecmp(from, x->name))
	return 0;
      break;
    }
  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (chan)
    if (shouldjoin(chan) && !channel_joining(chan)) {
      putlog(LOG_MISC, chname, "Server says I'm not on channel: %s", chname);
      clear_channel(chan, 1);
      chan->status &= ~CHAN_ACTIVE;
      chan->status |= CHAN_JOINING;
      dprintf(DP_MODE, "JOIN %s %s\n", chan->name,
	      chan->channel.key[0] ? chan->channel.key : chan->key_prot);
    }

  return 0;
}

/* Close the current server connection.
 */
void nuke_server(const char *reason)
{
  if (serv >= 0 && servidx >= 0) {
    if (reason)
      dprintf(-serv, "QUIT :%s\n", reason);

    sleep(1);
    disconnect_server(servidx, DO_LOST);
  }
}

char ctcp_reply[1024] = "";

static int lastmsgs[FLOOD_GLOBAL_MAX];
static char lastmsghost[FLOOD_GLOBAL_MAX][128];
static time_t lastmsgtime[FLOOD_GLOBAL_MAX];
static int dronemsgs;
static time_t dronemsgtime;
static bool set_pls_g;
static interval_t flood_g_time = 60;

rate_t flood_g = { 6, 2 };

void unset_g(int data)
{
  dprintf(DP_MODE, "MODE %s :-g\n", botname);
  set_pls_g = 0;
}

/* Do on NICK, PRIVMSG, NOTICE and JOIN.
 */
static bool detect_flood(char *floodnick, char *floodhost, char *from, int which)
{
  struct userrec *u = get_user_by_host(from);
  int atr = u ? u->flags : 0;

  if ((u && u->bot) || (atr & USER_NOFLOOD))
    return 0;

  char *p = NULL, ftype[10] = "", h[1024] = "";
  int thr = 0;
  time_t lapse = 0;

  /* Determine how many are necessary to make a flood */
  switch (which) {
  case FLOOD_PRIVMSG:
  case FLOOD_NOTICE:
    thr = flood_msg.count;
    lapse = flood_msg.time;
    strlcpy(ftype, "msg", 4);
    break;
  case FLOOD_CTCP:
    thr = flood_ctcp.count;
    lapse = flood_ctcp.time;
    strlcpy(ftype, "ctcp", 5);
    break;
  }
  if ((thr == 0) || (lapse == 0))
    return 0;			/* No flood protection */
  /* Okay, make sure i'm not flood-checking myself */
  if (match_my_nick(floodnick))
    return 0;
  if (!strcasecmp(floodhost, botuserhost))
    return 0;			/* My user@host (?) */

  //FIXME: hack for +g 

  if (dronemsgtime < now - flood_g.time) {	//expired, reset counter
    dronemsgs = 0;
//    dronemsgtime = now;
  }

  dronemsgs++;
  dronemsgtime = now;

  if (!set_pls_g && dronemsgs >= flood_g.count) {  //flood from dronenet, let's attempt to set +g
    egg_timeval_t howlong;

    set_pls_g = 1;
    dronemsgs = 0;
    dronemsgtime = 0;
    dprintf(DP_DUMP, "MODE %s :+g\n", botname);
    howlong.sec = flood_g_time;
    howlong.usec = 0;
    timer_create(&howlong, "Unset umode +g", (Function) unset_g);
    putlog(LOG_MISC, "*", "Drone flood detected! Setting +g for %d seconds.", flood_g_time);
    return 1;	//ignore the current msg
  }

  p = strchr(floodhost, '@');
  if (p) {
    p++;
    if (strcasecmp(lastmsghost[which], p)) {	/* New */
      strlcpy(lastmsghost[which], p, 128);
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
    simple_snprintf(h, sizeof(h), "*!*@%s", p);
    putlog(LOG_MISC, "*", "Flood from @%s!  Placing on ignore!", p);
    addignore(h, conf.bot->nick, (which == FLOOD_CTCP) ? "CTCP flood" : "MSG/NOTICE flood", now + (60 * ignore_time));
    return 1;
  }
  return 0;
}

/* Check for more than 8 control characters in a line.
 * This could indicate: beep flood CTCP avalanche.
 */
bool detect_avalanche(char *msg)
{
  int count = 0;

  for (unsigned char *p = (unsigned char *) msg; (*p) && (count < 8); p++)
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
  if (msg[0] && ((strchr(CHANMETA, *msg) != NULL) ||
     (*msg == '@')))           /* Notice to a channel, not handled here */
    return 0;

  char *to = NULL, buf[UHOSTLEN] = "", *nick = NULL, ctcpbuf[512] = "", *uhost = buf, 
       *ctcp = NULL, *p = NULL, *p1 = NULL, *code = NULL;
  struct userrec *u = NULL;
  int ctcp_count = 0;
  bool ignoring = match_ignore(from);

  to = newsplit(&msg);

  fixcolon(msg);
  /* Only check if flood-ctcp is active */
  strlcpy(uhost, from, UHOSTLEN);
  nick = splitnick(&uhost);
  if (flood_ctcp.count && detect_avalanche(msg)) {
    if (!ignoring) {
      putlog(LOG_MODES, "*", "Avalanche from %s - ignoring", from);
      p = strchr(uhost, '@');
      if (p != NULL)
	p++;
      else
	p = uhost;
      simple_snprintf(ctcpbuf, sizeof(ctcpbuf), "*!*@%s", p);
      addignore(ctcpbuf, conf.bot->nick, "ctcp avalanche", now + (60 * ignore_time));
      ignoring = 1;
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
      strlcpy(ctcpbuf, p1, sizeof(ctcpbuf));
      ctcp = ctcpbuf;
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
	      putlog(LOG_PUBLIC, to, "CTCP %s from %s (%s) to %s: %s", code, nick, uhost, to, ctcp);
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
                    if (!ischanhub())
                      putlog(LOG_MISC, "*", "%s: %s", "Refused DCC chat (I'm not a chathub (+c))", from);
                    else
                      putlog(LOG_MISC, "*", "%s: %s", "Refused DCC chat (no access)", from);
                  } else {
                    putlog(LOG_MISC, "*", "Refused DCC %s: %s", code, from);
                  }
                } else if (!strcmp(code, "CHAT")) {
                  if (!ischanhub())
                    putlog(LOG_MISC, "*", "%s: %s", "Refused DCC chat (I'm not a chathub (+c))", from);
                  else
                    putlog(LOG_MISC, "*", "%s: %s", "Refused DCC chat (no access)", from);
                }
	      }

	      if (!strcmp(code, "ACTION")) {
                putlog(LOG_MSGS, "*", "* %s (%s): %s", nick, uhost, ctcp);
              } else {
                putlog(LOG_MSGS, "*", "CTCP %s: from %s (%s): %s", code, nick, uhost, ctcp);
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
      if (now - last_ctcp > flood_ctcp.time) {
        dprintf(DP_HELP, "NOTICE %s :%s\n", nick, ctcp_reply);
	count_ctcp = 1;
      } else if (count_ctcp < flood_ctcp.count) {
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
      Auth *auth = NULL;
     
      if (auth_prefix[0])
        auth = Auth::Find(uhost);

      if (!auth && !ignoring)
        detect_flood(nick, uhost, from, FLOOD_PRIVMSG);
      my_code = newsplit(&msg);
      rmspace(msg);
      /* is it a cmd? */

      if (auth_prefix[0] && my_code && my_code[0] && my_code[1] && auth && auth->Authed() && my_code[0] == auth_prefix[0]) {
        my_code++;		//eliminate the prefix
        auth->atime = now;

        if (!check_bind_authc(my_code, auth, NULL, msg))
          putlog(LOG_MSGS, "*", "[%s] %c%s %s", from, auth_prefix[0], my_code, msg);
      } else if (!ignoring && (my_code[0] != auth_prefix[0] || !my_code[1] || !auth || !auth->Authed())) {
        struct userrec *my_u = get_user_by_host(from);
        bool doit = 1;

        if (!strcasecmp(my_code, "op") || !strcasecmp(my_code, "pass") || !strcasecmp(my_code, "invite")
            || !strcasecmp(my_code, "ident")
            || !strcasecmp(my_code, msgop) || !strcasecmp(my_code, msgpass)
            || !strcasecmp(my_code, msginvite) || !strcasecmp(my_code, msgident)) {
          const char *buf2 = NULL;

          doit = 0;
          if (!strcasecmp(my_code, msgop))
            buf2 = "op";
          else if (!strcasecmp(my_code, msgpass))
            buf2 = "pass";
          else if (!strcasecmp(my_code, msginvite))
            buf2 = "invite";
          else if (!strcasecmp(my_code, msgident))
            buf2 = "ident";

          if (buf2)
            check_bind_msg(buf2, nick, uhost, my_u, msg);
          else
            putlog(LOG_MSGS, "*", "(%s!%s) attempted to use invalid msg cmd '%s'", nick, uhost, my_code);
        }
        if (doit)
          check_bind_msg(my_code, nick, uhost, my_u, msg);
      }
    }
  }
  return 0;
}

/* Got a private notice.
 */
static int gotnotice(char *from, char *msg)
{
  if (msg[0] && ((strchr(CHANMETA, *msg) != NULL) ||
      (*msg == '@')))           /* Notice to a channel, not handled here */
    return 0;

  char *to = NULL, *nick = NULL, ctcpbuf[512] = "", *p = NULL, *p1 = NULL, buf[512] = "", 
       *uhost = buf, *ctcp = NULL, *ctcpmsg = NULL, *ptr = NULL;
  struct userrec *u = NULL;
  bool ignoring = match_ignore(from);

  to = newsplit(&msg);
  fixcolon(msg);
  strlcpy(uhost, from, UHOSTLEN);
  nick = splitnick(&uhost);
  if (flood_ctcp.count && detect_avalanche(msg)) {
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
      strlcpy(ctcpbuf, p1, sizeof(ctcpbuf));
      ctcp = ctcpbuf;
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
        if (!server_online && 
            strstr(msg, "*** You are exempt from flood"))
          floodless = 1;
	/* Hidden `250' connection count message from server */
	if (strncmp(msg, "Highest connection count:", 25))
	  putlog(LOG_SERV, "*", "-NOTICE- %s", msg);
      } else if (!ignoring) {
        detect_flood(nick, uhost, from, FLOOD_NOTICE);
        u = get_user_by_host(from);
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

void server_send_ison()
{
  if (server_online && keepnick && !use_monitor) {
    // Only check if we're not on jupenick, or there is no jupenick and we're not on the preferred nick
    bool have_jupenick = jupenick[0] ? match_my_nick(jupenick) : 0;
    if (!have_jupenick) {
      /* See if my nickname is in use and if if my nick is right.  */
      if (jupenick[0] && !have_jupenick)
        dprintf(DP_SERVER, "ISON %s %s\n", origbotname, jupenick);
      else if (!match_my_nick(origbotname)) 
        dprintf(DP_SERVER, "ISON %s\n", origbotname);
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
    bool ok = 0;

    for (register struct chanset_t *chan = chanset; chan; chan = chan->next) {
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

static int nick_which(const char* nick, bool& is_jupe, bool& is_orig) {
  if (jupenick[0] && !rfc_casecmp(nick, jupenick)) {
    is_jupe = 1;
    // If some stupid reason they have the same jupenick/nick, make sure to mark it as on
    if (!rfc_casecmp(nick, origbotname))
      is_orig = 1;
    return 1;
  } else if (!rfc_casecmp(nick, origbotname)) {
    is_orig = 1;
  }
  return 0;
}

static void nick_available(bool is_jupe, bool is_orig) {
  if (jupenick[0] && is_jupe && !match_my_nick((jupenick))) {
    /* Ensure we aren't processing a QUIT/NICK and a MONITOR, or just some screw up */
    if (!tried_jupenick || ((now - tried_jupenick) > 1)) {
      tried_jupenick = now;
      dprintf(DP_DUMP, "NICK %s\n", jupenick);
      if (!jnick_juped)
        putlog(LOG_MISC, "*", "Switching back to jupenick '%s'", jupenick);
    }
    // Don't switch to the nick if already on jupenick
  } else if (is_orig && !match_my_nick(origbotname) && (!jupenick[0] || !match_my_nick(jupenick))) {
    if (!tried_nick || ((now - tried_nick) > 1)) {
      altnick_char = rolls = 0;
      tried_nick = now;
      dprintf(DP_DUMP, "NICK %s\n", origbotname);
      if (!nick_juped)
        putlog(LOG_MISC, "*", "Switching back to nick '%s'", origbotname);
    }
  }
}

void nicks_available(char* buf, char delim, bool buf_contains_available) {
  if (!buf[0] || !keepnick) return;
  bool is_jupe = 0, is_orig = 0;

  char *nick = NULL;
  if (delim) {
    while ((nick = newsplit(&buf, delim))[0]) {
      if (nick_which(nick, is_jupe, is_orig))
        break;
    }
  } else {
    nick = buf;
    nick_which(nick, is_jupe, is_orig);
  }

  if (buf_contains_available == 0) {
    is_jupe = !is_jupe;
    is_orig = !is_orig;
  }

  nick_available(is_jupe, is_orig);
}

/* This is a reply to MONITOR: Offline
 * RPL_MONOFFLINE
 * :<server> 731 <nick> :nick[,nick1]*
 */
static void got731(char* from, char* msg)
{
  char *tmp = newsplit(&msg);
  fixcolon(msg);

  //msg now contains the nick(s) available
  if (tmp[0] && (!strcmp(tmp, "*") || match_my_nick(tmp)))
    nicks_available(msg, ',');
}

/* This is a reply on ISON :<orig> [<jupenick>]
*/
static void got303(char *from, char *msg)
{
  char *tmp = newsplit(&msg);
  fixcolon(msg);
  if (tmp[0] && match_my_nick(tmp))
    nicks_available(msg, ' ', 0);
}

/* 432 : Bad nickname (RESV)
 */
static int got432(char *from, char *msg)
{
  char *erroneus = NULL;

  newsplit(&msg);
  erroneus = newsplit(&msg);

  bool is_jnick = 0, was_juped = 0;

  if (jupenick[0] && !strcmp(erroneus, jupenick)) {
    was_juped = jnick_juped;
    is_jnick = jnick_juped = 1;
  } else {
    was_juped = nick_juped;
    nick_juped = 1;
  }

  if (server_online) {
    if (!was_juped)
      putlog(LOG_MISC, "*", "%sNICK IS INVALID: '%s' (keeping '%s').", is_jnick ? "JUPE" : "", erroneus, botname);
  } else {
    putlog(LOG_MISC, "*", "Server says my %snick '%s' is invalid.", is_jnick ? "jupe" : "", botname);
    if (jupenick[0] && !strcmp(botname, jupenick))
      strlcpy(botname, origbotname, NICKLEN);
    else
      rotate_nick(botname, origbotname);

    dprintf(DP_MODE, "NICK %s\n", botname);
    return 0;
  }
  return 0;
}

/* 433 : Nickname in use
 * Change nicks till we're acceptable or we give up
 */
static char rnick[NICKLEN] = "";
static int got433(char *from, char *msg)
{
  char *tmp = newsplit(&msg);

  /* We are online and have a nickname, we'll keep it */
  //Also make sure we're not juping the jupenick if we shouldn't be.
  //Prefer to be on the 'nick'(origbotname) at all times
  if (server_online) {
    tmp = newsplit(&msg);

    if (tried_jupenick)
      jnick_juped = 0;
    else if (!rfc_casecmp(tmp, origbotname))
      nick_juped = 0;

    tried_nick = 0;

    sdprintf("433: botname: %s tmp: %s rolls: %d altnick_char: %c tried_jupenick: %lu", botname, tmp, rolls, altnick_char, tried_jupenick);
    // Tried jupenick, but already on origbotname (or a rolled nick), stay on it.
    if (tried_jupenick && (match_my_nick(origbotname) || rolls || altnick_char != 0)) {
      putlog(LOG_MISC, "*", "%sNICK IN USE: '%s' (keeping '%s').", tried_jupenick ? "JUPE" : "", tmp, botname);
    } else {  //Else need to find a new nick
      // Failed to get jupenick, not on origbotname now, try for origbotname and rotate from there
      if (tried_jupenick) {
        strlcpy(rnick, origbotname, NICKLEN);
        tried_jupenick = 0;
      } else {
        // Was a failed attempt at a rotated nick, keep rotating on origbotname
        strlcpy(rnick, tmp, NICKLEN);
        rotate_nick(rnick, origbotname);
      }

      // Make sure not trying to set the nick we're already on
      if (!match_my_nick(rnick)) {
        putlog(LOG_MISC, "*", "NICK IN USE: '%s' Trying '%s'", tmp, rnick);
        dprintf(DP_SERVER, "NICK %s\n", rnick);
      } else
        putlog(LOG_MISC, "*", "NICK IN USE: '%s' (keeping '%s')", tmp, botname);
    }
  } else {
    //Hack for different responses:
    //[@] efnet.demon.co.uk 433 * lolwut :Nickname is already in use.
    //[@] irc.efnet.no 433  lolwut :Nickname is already in use.
    if (tmp[0] == '*')
      tmp = newsplit(&msg);
    gotfake433(tmp);
  }
  return 0;
}

/* 437 : Channel/Nickname juped (IRCnet) (Also temp jupe during splits on efnet)
 * :ca.us.irc.xzibition.com 437 test1 bryan :Nick/channel is temporarily unavailable
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
      chan->status &= ~(CHAN_JOINING);
      if (chan->status & CHAN_ACTIVE) {
	putlog(LOG_MISC, "*", "Can't change nickname on %s.  Is my nickname banned?", s);
      } else {
	if (!channel_juped(chan)) {
	  putlog(LOG_MISC, "*", "Channel %s is juped. :(", s);
	  chan->status |= CHAN_JUPED;
	}
      }
    }
  } else if (server_online) {
    if (!rfc_casecmp(s, origbotname)) {
      if (!nick_juped)
        putlog(LOG_MISC, "*", "NICK IS TEMPORARILY UNAVAILABLE: '%s' (keeping '%s').", s, botname);
      nick_juped = 1;
    } else if (jupenick[0] && !rfc_casecmp(s, jupenick)) {
      if (!jnick_juped)
        putlog(LOG_MISC, "*", "JUPENICK IS TEMPORARILY UNAVAILABLE: '%s' (keeping '%s').", s, botname);
      jnick_juped = 1;
    }
  } else {
    putlog(LOG_MISC, "*", "%s: %s", "Nickname has been juped", s);
    gotfake433(s);
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
  nuke_server("The server says we are not registered yet..");
  return 0;
}

/* Got error notice
 */
static int goterror(char *from, char *msg)
{
  if (msg[0] == ':')
    msg++;       
  putlog(LOG_SERV, "*", "-ERROR from server- %s", msg);
  putlog(LOG_SERV, "*", "Disconnecting from server.");
  nuke_server("Bah, stupid error messages.");
  return 1;
}

/* Got nick change.
 */
static int gotnick(char *from, char *msg)
{
  char *nick = NULL, *buf = NULL, *buf_ptr = NULL;
  struct userrec *u = NULL;

  //Done to prevent gotnick in irc.mod getting a mangled from
  buf = buf_ptr = strdup(from);
  //Cache user
  u = get_user_by_host(buf);
  nick = splitnick(&buf);
  fixcolon(msg);

  if (match_my_nick(nick)) {
    /* Regained nick! */
    strlcpy(botname, msg, NICKLEN);

    tried_jupenick = 0;
    tried_nick = 0;

    if (jupenick[0] && !rfc_casecmp(msg, jupenick)) {
      altnick_char = rolls = 0;
      putlog(LOG_SERV | LOG_MISC, "*", "Regained jupenick '%s'.", msg);
      jnick_juped = 0;
    } else if (!strcmp(msg, origbotname)) {
      altnick_char = rolls = 0;
      putlog(LOG_SERV | LOG_MISC, "*", "Regained nickname '%s'.", msg);
      nick_juped = 0;
    } else if (keepnick && strcmp(nick, msg)) {

      // Was this an attempt at a nick thru rotation?
      if (!strcmp(rnick, msg)) {
        putlog(LOG_MISC, "*", "Nickname changed to '%s'", msg);
      } else {
        putlog(LOG_SERV | LOG_MISC, "*", "Nickname changed to '%s'???", msg);
        nicks_available(nick);
      }
    } else
      putlog(LOG_SERV | LOG_MISC, "*", "Nickname changed to '%s'???", msg);
  } else if ((keepnick) && (rfc_casecmp(nick, msg))) { //Ignore case changes
    nicks_available(nick);
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
  use_monitor = 0;
  cookies_disabled = false;
  floodless = 0;
  botuserhost[0] = 0;
  botuserip[0] = 0; 
  if (dolost) {
    Auth::DeleteAll();
    trying_server = 0;
    lostdcc(idx);
  }

  /* Invalidate the cmd_swhois cache callback data */
  for (int i = 0; i < dcc_total; i++) {
    if (dcc[i].type && dcc[i].whois[0]) {
      dcc[i].whois[0] = 0;
      dcc[i].whowas = 0;
    }
  }
}

static void eof_server(int idx)
{
  putlog(LOG_SERV, "*", "Disconnected from %s", dcc[idx].host);
  disconnect_server(idx, DO_LOST);
}

static void display_server(int idx, char *buf, size_t bufsiz)
{
  simple_snprintf(buf, bufsiz, "%s  (lag: %d)", trying_server ? "conn" : "serv", server_lag);
}

static void connect_server(void);

static void kill_server(int idx, void *x)
{
  disconnect_server(idx, NO_LOST);	/* eof_server will lostdcc() it. */

  if (!segfaulted)		// don't bother if we've segfaulted, too many memory calls in this loop
    for (struct chanset_t *chan = chanset; chan; chan = chan->next)
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

struct dcc_table SERVER_SOCKET =
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

static void server_activity(int idx, char *msg, int len)
{
  char *from = NULL, *code = NULL;

  if (trying_server) {
    strlcpy(dcc[idx].nick, "(server)", NICKLEN);
    putlog(LOG_SERV, "*", "Connected to %s", dcc[idx].host);

    trying_server = 0;
    /*
    servidx = idx;
    serv = dcc[idx].sosck;
    */
    SERVER_SOCKET.timeout_val = 0;
  }
  waiting_for_awake = 0;
  if (msg[0] == ':') {
    msg++;
    from = newsplit(&msg);
  } else
    from = "";
 
  code = newsplit(&msg);

  if (!strcmp(code, "PRIVMSG") || !strcmp(code, "NOTICE")) {
    if (!match_ignore(from))
      putlog(LOG_RAW, "@", "[@] %s %s %s", from, code, msg);
  } else
    putlog(LOG_RAW, "@", "[@] %s %s %s", from, code, msg);

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
  if (!match_my_nick(from))
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

  if (x && use_penalties) {
    int i = 0, ii = 0;

    for (; x; x = x->next) {
      if (i == curserv) {
        if (strcmp(x->name, from))
          ii = 1;
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

static void irc_whois(char *, const char *, ...) __attribute__((format(printf, 2, 3)));

static void
irc_whois(char *nick, const char *format, ...)
{
  char va_out[2001] = "";
  va_list va;

  va_start(va, format);
  egg_vsnprintf(va_out, sizeof(va_out) - 1, format, va);
  va_end(va);

  for (int idx = 0; idx < dcc_total; idx++) {
    if (dcc[idx].type && dcc[idx].whois[0] && !rfc_casecmp(nick, dcc[idx].whois)) {
      dprintf(idx, "%s\n", va_out);
      break;
    }
  }
}

void check_hostmask()
{
  if (!server_online || !botuserhost[0])
    return;

  char s[UHOSTLEN + 2] = "";

  simple_snprintf(s, sizeof(s), "*!%s", botuserhost);              /* just add actual user@ident, regardless of ~ */

  /* dont add the host if it conflicts with another in the userlist */
  struct userrec *u = NULL;
  if ((u = host_conflicts(s))) {
    if (u != conf.bot->u) {
      putlog(LOG_WARN, "*", "My automatic hostmask '%s' would conflict with user: '%s'. (Not adding)", s, u->handle);
    } else
      sdprintf("Already have hostmask '%s' added for myself", s);
    return;
  }

  addhost_by_handle(conf.bot->nick, s);

  putlog(LOG_GETIN, "*", "Updated my hostmask: %s", s);
}

/* 311 $me nick username address * :realname */
static int got311(char *from, char *msg)
{
  char *nick = NULL, *username = NULL, *address = NULL, uhost[UHOSTLEN + 1];
  struct userrec *u = NULL;
  
  newsplit(&msg);
  nick = newsplit(&msg);
  username = newsplit(&msg);
  address = newsplit(&msg);
  newsplit(&msg);
  fixcolon(msg);
    
  if (match_my_nick(nick)) {
    simple_snprintf(botuserhost, sizeof botuserhost, "%s@%s", username, address);
    check_hostmask();
  }

  irc_whois(nick, "$b%s$b [%s@%s]", nick, username, address);

  // FIXME: This should incorporate userip and member/client lookups - and should ACT on discovering a user.
  // FIXME: This should also use whois_actually to cache userip
  simple_snprintf(uhost, sizeof uhost, "%s!%s@%s", nick, username, address);
  if ((u = get_user_by_host(uhost))) {
    int idx = 0;
    for (idx = 0; idx < dcc_total; idx++) {
      if (dcc[idx].type && dcc[idx].whois[0] && !rfc_casecmp(nick, dcc[idx].whois)) {
        if (whois_access(dcc[idx].user, u)) {
          irc_whois(nick, " username : $u%s$u", u->handle);
        }
        break;
      }
    }
  }

  irc_whois(nick, " ircname  : %s", msg);
  
  return 0;
}

static char *
hide_chans(const char *nick, struct userrec *u, char *_channels, bool publicOnly)
{
  char *channels = strdup(_channels), *channels_p = channels;
  char *chname = NULL;
  size_t len = strlen(channels) + 100 + 1;
  struct chanset_t *chan = NULL;
  struct flag_record fr = { FR_CHAN | FR_GLOBAL, 0, 0, 0 };

  char *chans = (char *) my_calloc(1, len), *p = NULL;

  while ((chname = newsplit(&channels))[0]) {
    /* skip any modes in front of #chan */
    if (!(p = strchr(chname, '#')))
      if (!(p = strchr(chname, '&')))
        if (!(p = strchr(chname, '!')))
          continue;

    chan = findchan_by_dname(p);

    if (chan && !publicOnly)
     get_user_flagrec(u, &fr, chan->dname, chan);

    if (!chan || 
        
        (!publicOnly && (
          getnick(u->handle, chan) || 
          !(channel_hidden(chan)) || 
          chk_op(fr, chan)
        )) ||
        (publicOnly && !(channel_hidden(chan)))
       ) {
      if (chans[0])
        strlcat(chans, " ", len);
      strlcat(chans, chname, len);
    }
  }
  free(channels_p);
  return chans;
}

/* 319 $me nick :channels */
static int got319(char *from, char *msg)
{
  char *nick = NULL;

  newsplit(&msg);
  nick = newsplit(&msg);
  fixcolon(msg);

  for (int idx = 0; idx < dcc_total; idx++) {
    if (dcc[idx].type && dcc[idx].whois[0] && !rfc_casecmp(nick, dcc[idx].whois)) {
      char *chans = NULL;
      /* Show every channel the user has access to view */
      if ((chans = hide_chans(nick, dcc[idx].user, msg, 0))) {
        irc_whois(nick, " channels : %s", chans);
        free(chans);
      }
      chans = NULL;
      /* Show all of the 'public' channels */
      if ((chans = hide_chans(nick, dcc[idx].user, msg, 1))) {
        irc_whois(nick, " public   : %s", chans);
        free(chans);
      }
      break;
    }
  }

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

  strftime(date, sizeof date, "%c %Z", gmtime(&signon));

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

  newsplit(&msg);
  nick = newsplit(&msg);
  fixcolon(msg);

  irc_whois(nick, "%s", msg);
  for (int idx = 0; idx < dcc_total; idx++) {
    if (dcc[idx].type && dcc[idx].whois[0] && !rfc_casecmp(dcc[idx].whois, nick) &&
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

  newsplit(&msg);
  nick = newsplit(&msg);
  fixcolon(msg);
  irc_whois(nick, "%s", msg);
  for (int idx = 0; idx < dcc_total; idx++)
    if (dcc[idx].type && dcc[idx].whois[0] && !rfc_casecmp(dcc[idx].whois, nick))
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

/* 465 info_ :You are banned from this server- Temporary K-line 1 min. - Testing kline notices (2008/4/3 09.51) */
static int got465(char *from, char *msg)
{
  newsplit(&msg); /* nick */
  fixcolon(msg);
  putlog(LOG_SERV, "*", "I am klined: %s", msg);
  putlog(LOG_SERV, "*", "Disconnecting from server.");
  nuke_server("I am klined!");
  return 1;                                           
}

/* 718 $me nick user@host :msg 
 * for receiving a msg while +g
 */
static int got718(char *from, char *msg)
{
  char *nick = NULL, *uhost = NULL;

  newsplit(&msg);
  nick = newsplit(&msg);
  uhost = newsplit(&msg);
  fixcolon(msg);

  putlog(LOG_WALL, "*", "(+g) !%s!%s! %s", nick, uhost, msg);

  return 0;
}
 
static cmd_t my_raw_binds[] =
{
  {"PRIVMSG",	"",	(Function) gotmsg,		NULL, LEAF},
  {"NOTICE",	"",	(Function) gotnotice,		NULL, LEAF},
  {"MODE",	"",	(Function) gotmode,		NULL, LEAF},
  {"PING",	"",	(Function) gotping,		NULL, LEAF},
  {"PONG",	"",	(Function) gotpong,		NULL, LEAF},
  {"WALLOPS",	"",	(Function) gotwall,		NULL, LEAF},
  {"001",	"",	(Function) got001,		NULL, LEAF},
  {"004",	"",	(Function) got004,		NULL, LEAF},
  {"005",	"",	(Function) got005,		NULL, LEAF},
  {"303",	"",	(Function) got303,		NULL, LEAF},
  {"432",	"",	(Function) got432,		NULL, LEAF},
  {"433",	"",	(Function) got433,		NULL, LEAF},
  {"437",	"",	(Function) got437,		NULL, LEAF},
  {"438",	"",	(Function) got438,		NULL, LEAF},
  {"451",	"",	(Function) got451,		NULL, LEAF},
  {"442",	"",	(Function) got442,		NULL, LEAF},
  {"NICK",	"",	(Function) gotnick,		NULL, LEAF},
  {"ERROR",	"",	(Function) goterror,		NULL, LEAF},
/* ircu2.10.10 has a bug when a client is throttled ERROR is sent wrong */
  {"ERROR:",	"",	(Function) goterror,		NULL, LEAF},
  {"KICK",	"",	(Function) gotkick,		NULL, LEAF},
  /* WHOIS RAWS */
  {"311", 	"", 	(Function) got311, 		NULL, LEAF},	/* ident host * :realname */
  {"314",	"",	(Function) got311,		NULL, LEAF},	/* "" -WHOWAS */
  {"319",	"",	(Function) got319,		NULL, LEAF},	/* :#channels */
  {"312",	"",	(Function) got312,		NULL, LEAF},	/* server :gecos */
  {"301",	"",	(Function) got301,		NULL, LEAF},	/* :away msg */
  {"313",	"",	(Function) got313,		NULL, LEAF},	/* :ircop */
  {"317",	"",	(Function) got317,		NULL, LEAF},	/* idle, signon :idle-eng, signon-eng */
  {"401",	"",	(Function) got401,		NULL, LEAF},
  {"406",	"",	(Function) got406,		NULL, LEAF},
  {"465",	"",	(Function) got465,		NULL, LEAF},	/* RPL_YOUREBANNEDCREEP */
  {"318",	"",	(Function) whoispenalty,	NULL, LEAF},	/* :End of /WHOIS */
  {"369",	"",	(Function) got369,		NULL, LEAF},	/* :End of /WHOWAS */
  {"718",	"",	(Function) got718,		NULL, LEAF},
  {"731",	"",	(Function) got731,		NULL, LEAF},	/* RPL_MONOFFLINE */
  {NULL,	NULL,	NULL,				NULL, 0}
};

static void server_dns_callback(int, void *, const char *, bd::Array<bd::String>);

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
    strlcpy(botserver, newserver, sizeof(botserver));
    botserverport = newserverport;
    strlcpy(pass, newserverpass, sizeof(pass));
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
    strlcpy(dcc[newidx].nick, "(server)", NICKLEN);
    strlcpy(dcc[newidx].host, botserver, UHOSTLEN);

    botuserhost[0] = 0;

    nick_juped = 0;
    jnick_juped = 0;
    tried_jupenick = 0;
    tried_nick = 0;
    rolls = 0;
    altnick_char = 0;
    use_monitor = 0;

    for (chan = chanset; chan; chan = chan->next)
      chan->status &= ~CHAN_JUPED;

    dcc[newidx].timeval = now;
    dcc[newidx].sock = -1;
    dcc[newidx].u.dns->cbuf = strdup(pass);

    cycle_time = 15;		/* wait 15 seconds before attempting next server connect */

    /* I'm resolving... don't start another server connect request */
    resolvserv = 1;
    /* Resolve the hostname. */
    int dns_id = egg_dns_lookup(botserver, 20, server_dns_callback, (void *) (long) newidx);
    if (dns_id >= 0)
      dcc[newidx].dns_id = dns_id;
    /* wait for async reply */
  }
}

static void server_dns_callback(int id, void *client_data, const char *host, bd::Array<bd::String> ips)
{
  //64bit hacks
  long data = (long) client_data;
  int idx = (int) data;

  resolvserv = 0;

  if (!valid_dns_id(idx, id))
    return;

  if (!ips.size()) {
    putlog(LOG_SERV, "*", "Failed connect to %s (DNS lookup failed)", host);
    trying_server = 0;
    lostdcc(idx);
    return;
  }

  my_addr_t addr;
  char *ip = NULL;

  /* FIXME: this is a temporary hack to stop bots from connecting over ipv4 when they should be on ipv6
   * eventually will handle this in open_telnet(ips);
   */
#ifdef USE_IPV6
  if (conf.bot->net.v6) {
    size_t i = 0;
    for (i = 0; i < ips.size(); ++i) {
      if (is_dotted_ip(bd::String(ips[i]).c_str()) == AF_INET6) {
        ip = strdup(bd::String(ips[i]).c_str());
        break;
      }
    }
    if (!ip) {
      putlog(LOG_SERV, "*", "Failed connect to %s (Could not DNS as IPV6)", host);
      trying_server = 0;
      lostdcc(idx);
      return;
    }
  } else
#endif /* USE_IPV6 */
    ip = strdup(bd::String(ips[0]).c_str());

  get_addr(ip, &addr);
 
  if (addr.family == AF_INET)
    dcc[idx].addr = htonl(addr.u.addr.s_addr);

  strlcpy(serverpass, (char *) dcc[idx].u.dns->cbuf, sizeof(serverpass));
  changeover_dcc(idx, &SERVER_SOCKET, 0);

//  identd_open(idx);

  //No proxy, use identd, 2 = spoof ident
  serv = open_telnet(ip, dcc[idx].port, 0, 2);

  if (serv < 0) {
    putlog(LOG_SERV, "*", "Failed connect to %s (%s)", dcc[idx].host, strerror(errno));
    trying_server = 0;
    lostdcc(idx);
  } else {
    int i = 1;

    /* set these now so if we fail disconnect_server() can cleanup right. */
    dcc[idx].sock = serv;
    servidx = idx;
    sdprintf("Connecting to '%s' (serv: %d, servidx: %d)", dcc[idx].host, serv, servidx);
    setsockopt(serv, 6, TCP_NODELAY, &i, sizeof(int));
    /* Queue standard login */
    dcc[idx].timeval = now;
    SERVER_SOCKET.timeout_val = &server_timeout;
    /* Another server may have truncated it, so use the original */
    if (jupenick[0])
      strlcpy(botname, jupenick, sizeof(botname));
    else
      strlcpy(botname, origbotname, sizeof(botname));
    /* Start alternate nicks from the beginning */
    altnick_char = 0;
    /* reset counter so first ctcp is dumped for tcms */
    first_ctcp_check = 0;

    if (serverpass[0])
      dprintf(DP_MODE, "PASS %s\n", serverpass);
    dprintf(DP_MODE, "NICK %s\n", botname);
    dprintf(DP_MODE, "USER %s localhost %s :%s\n", botuser, dcc[idx].host, replace_vars(botrealname));
    /* Wait for async result now */
  }
  free(ip);
}
