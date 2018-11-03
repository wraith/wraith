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
  // Cap the len calcs at the max NICKLEN for this server
  if (len > nick_len) {
    len = nick_len;
    nick[len] = 0;
  }
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
    strlcpy(botname, origbotname, sizeof(botname));
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

  // Decrypt FiSH before processing
  if (!strcmp(code, "PRIVMSG") || !strcmp(code, "NOTICE")) {
    char* colon = strchr(msg, ':'), *first_word = strchr(msg, ' ');
    bd::String target(msg, first_word - msg);

    ++colon;
    if (colon) {
      if (!strncmp(colon, "+OK ", 4)) {
        bool isValidCipherText;
        char *p = strchr(from, '!');
        const bool target_is_chan = strchr(CHANMETA, target[0]);
        bd::String ciphertext(colon), sharedKey, nick(from, p - from), key_target;

        // If this is a channel msg, decrypt with the channel key
        if (target_is_chan) {
          key_target = target;
        } else {
          // Otherwise decrypt with the nick's key
          key_target = nick;
        }

        const bool have_shared_key = FishKeys.contains(key_target);

        if (have_shared_key) {
          sharedKey = FishKeys[key_target]->sharedKey;
        } else {
          struct userrec *u = get_user_by_host(from);
          if (u) {
            sharedKey = static_cast<char*>(get_user(&USERENTRY_SECPASS, u));
          }
        }

        if (sharedKey.length()) {
          // Decrypt the message before passing along to the binds
          const bd::String decrypted(egg_bf_decrypt(ciphertext, sharedKey));
          // Does the decrypted text make sense? If not, the key is probably invalid, reset it.
          isValidCipherText = true;
          for (size_t i = 0; i < decrypted.length(); ++i) {
            if (!isprint(decrypted[i])) {
              isValidCipherText = false;
              break;
            }
          }
          if (isValidCipherText) {
            bd::String cleartext(bd::String(msg, colon - msg) + decrypted);
            mymsg = p2 = strdup(cleartext.c_str());
          } else if (!target_is_chan && have_shared_key) {
            // Delete the shared key
            fish_data_t* fishData = FishKeys[key_target];
            FishKeys.remove(key_target);
            delete fishData;
          }
        } else {
          isValidCipherText = false;
        }
        if (fish_auto_keyx && !isValidCipherText && !target_is_chan) {
          keyx(nick, "Invalid/Unknown key");
        }
      }
    }
  }
  if (!p2)
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
  return (!rfc_ncasecmp(nick, botname, nick_len));
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
  strlcpy(cursrvname, servname, sizeof(cursrvname));
  if (servidx >= 0)
    curservport = dcc[servidx].port;

  if (nick && nick[0]) {
    strlcpy(botname, nick, sizeof(botname));

    dprintf(DP_MODE, "WHOIS %s\n", botname); /* get user@host */
    dprintf(DP_MODE, "USERHOST %s\n", botname); /* get user@ip */
    dprintf(DP_SERVER, "MODE %s %s\n", botname, var_get_str_by_name("usermode"));
    rehash_monitor_list();
  }
}

void 
join_chans()
{
  for (struct chanset_t *chan = chanset; chan; chan = chan->next) {
    if (shouldjoin(chan))
      force_join_chan(chan, DP_SERVER);
  }
}

/* 001: welcome to IRC (use it to fix the server name)
 */
static int got001(char *from, char *msg)
{

  fixcolon(msg);
  server_online = now;
  waiting_for_awake = 0;
  rehash_server(from, msg);
  /* Ok...param #1 of 001 = what server thinks my nick is */

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

  bool connect_burst = 0;

  /* cookies won't work on ircu or Unreal or snircd */
  cookies_disabled = false;
  if (strstr(tmp, "u2.") || strstr(tmp, "Unreal") || strstr(tmp, "snircd")) {
    putlog(LOG_DEBUG, "*", "Disabling cookies as they are not supported on %s", cursrvname);
    cookies_disabled = true;
  } else if (strstr(tmp, "hybrid") || strstr(tmp, "ratbox") || strstr(tmp, "Charybdis") || strstr(tmp, "ircd-seven")) {
    connect_burst = 1;
    if (!strstr(tmp, "hybrid"))
      use_flood_count = 1;
  }

  if (!replaying_cache && connect_burst) {
    connect_bursting = now;
    msgburst = SERVER_CONNECT_BURST_RATE;
    msgrate = 0;
    reset_flood();
    putlog(LOG_DEBUG, "*", "Server allows connect bursting, bursting for %d seconds", SERVER_CONNECT_BURST_TIME);
  }

  if (!replaying_cache)
    join_chans();

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
    else if (!strcasecmp(tmp, "NETWORK")) {
      strlcpy(curnetwork, p, 120);
      if (!strcasecmp(tmp, "IRCnet")) {
        simple_snprintf(stackablecmds, sizeof(stackablecmds), "INVITE AWAY VERSION NICK");
        simple_snprintf(stackable2cmds, sizeof(stackable2cmds), "USERHOST ISON");
        use_fastdeq = 3;
      } else if (!strcasecmp(tmp, "DALnet")) {
        simple_snprintf(stackablecmds, sizeof(stackablecmds), "PRIVMSG NOTICE PART WHOIS WHOWAS USERHOST ISON WATCH DCCALLOW");
        simple_snprintf(stackable2cmds, sizeof(stackable2cmds), "USERHOST ISON WATCH");
        use_fastdeq = 2;
      } else if (!strcasecmp(tmp, "UnderNet")) {
        simple_snprintf(stackablecmds, sizeof(stackablecmds), "PRIVMSG NOTICE TOPIC PART WHOIS USERHOST USERIP ISON");
        simple_snprintf(stackable2cmds, sizeof(stackable2cmds), "USERHOST USERIP ISON");
        use_fastdeq = 2;
      } else {
        stackablecmds[0] = 0;
        simple_snprintf(stackable2cmds, sizeof(stackable2cmds), "USERHOST ISON");
        use_fastdeq = 0;
      }
    } else if (!strcasecmp(tmp, "PENALTY"))
      use_penalties = 1;
    else if (!strcasecmp(tmp, "WHOX"))
      use_354 = 1;
    else if (!strcasecmp(tmp, "DEAF")) {
      deaf_char = p ? p[0] : 'D';
      if (use_deaf && !in_deaf) {
        dprintf(DP_SERVER, "MODE %s +%c\n", botname, deaf_char);
        in_deaf = 1;
      }
    } else if (!strcasecmp(tmp, "CALLERID")) {
      callerid_char = p ? p[0] : 'g';
      if (use_callerid && !in_callerid) {
        dprintf(DP_SERVER, "MODE %s +%c\n", botname, callerid_char);
        in_callerid = 1;
      }
    } else if (!strcasecmp(tmp, "EXCEPTS"))
      use_exempts = 1;
    else if (!strcasecmp(tmp, "CPRIVMSG"))
      have_cprivmsg = 1;
    else if (!strcasecmp(tmp, "CNOTICE"))
      have_cnotice = 1;
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
        rfc_ncasecmp = strncasecmp;
        rfc_char_equal = char_equal;
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
  if (chan && shouldjoin(chan) && !channel_joining(chan)) {
    putlog(LOG_MISC, chname, "Server says I'm not on channel: %s", chname);
    force_join_chan(chan);
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
    disconnect_server(servidx);
    lostdcc(servidx);
  }
}

char ctcp_reply[1024] = "";

static int lastmsgs[FLOOD_GLOBAL_MAX];
static char lastmsghost[FLOOD_GLOBAL_MAX][128];
static time_t lastmsgtime[FLOOD_GLOBAL_MAX];
static int dronemsgs;
static time_t dronemsgtime;
static interval_t flood_callerid_time = 60;

rate_t flood_callerid = { 6, 2 };

void unset_callerid(int data)
{
  if (in_callerid) {
    dprintf(DP_MODE, "MODE %s :-%c\n", botname, callerid_char);
    in_callerid = 0;
  }
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

  if (dronemsgtime < now - flood_callerid.time) {	//expired, reset counter
    dronemsgs = 0;
//    dronemsgtime = now;
  }

  dronemsgs++;
  dronemsgtime = now;

  if (!in_callerid && dronemsgs >= flood_callerid.count && callerid_char) {  //flood from dronenet, let's attempt to set +g
    egg_timeval_t howlong;

    in_callerid = 1;
    dronemsgs = 0;
    dronemsgtime = 0;
    dprintf(DP_MODE_NEXT, "MODE %s :+%c\n", botname, callerid_char);
    howlong.sec = flood_callerid_time;
    howlong.usec = 0;
    timer_create(&howlong, "Unset CALLERID", (Function) unset_callerid);
    putlog(LOG_MISC, "*", "Drone flood detected! Setting CALLERID for %d seconds.", flood_callerid_time);
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
#ifdef TCL
    u = get_user_by_host(from);
#endif
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
bool
detect_avalanche(const char *msg)
{
  int count = 0;

  for (const unsigned char *p = (const unsigned char *) msg; (*p) && (count < 8); p++)
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

      /* remove the ctcp in msg */
      memmove(p1 - 1, p + 1, strlen(p + 1) + 1);

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
      notice(nick, ctcp_reply, DP_HELP);
    } else {
      if (now - last_ctcp > flood_ctcp.time) {
        notice(nick, ctcp_reply, DP_HELP);
	count_ctcp = 1;
      } else if (count_ctcp < flood_ctcp.count) {
        notice(nick, ctcp_reply, DP_HELP);
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
            || !strcasecmp(my_code, "ident") || !strcasecmp(my_code, "release")
            || !strcasecmp(my_code, msgop) || !strcasecmp(my_code, msgpass) || !strcasecmp(my_code, msgrelease)
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
          else if (!strcasecmp(my_code, msgrelease))
            buf2 = "release";

          if (buf2)
            check_bind_msg(buf2, nick, uhost, my_u, msg);
          else
            putlog(LOG_MSGS, "*", "(%s!%s) attempted to use invalid msg cmd '%s'", nick, uhost, my_code);
        }
        if (doit)
          check_bind_msg(my_code, nick, uhost, my_u, msg);

        if (my_u && FishKeys.contains(nick)) {
          // FiSH paranoid mode. Invalidate the current key and re-key-exchange with the user.
          if (fish_paranoid) {
            keyx(nick, "fish-paranoid is set");
          }
        }
      }
    }
  }
  return 0;
}

// Adapated from ZNC
void handle_DH1080_init(const char* nick, const char* uhost, const char* from, struct userrec* u, const bd::String theirPublicKeyB64) {
  bd::String myPublicKeyB64, myPrivateKey, sharedKey;

  DH1080_gen(myPrivateKey, myPublicKeyB64);
  if (!DH1080_comp(myPrivateKey, theirPublicKeyB64, sharedKey)) {
    sdprintf("Error computing DH1080 for %s: %s", nick, sharedKey.c_str());
    return;
  }

  putlog(LOG_MSGS, "*", "[FiSH] Received DH1080 public key from (%s!%s) - sending mine", nick, uhost);
  fish_data_t* fishData = FishKeys.contains(nick) ? FishKeys[nick] : new fish_data_t;
  fishData->sharedKey.clear();
  notice(nick, "DH1080_FINISH " + myPublicKeyB64, DP_HELP);
  fishData->myPublicKeyB64 = myPublicKeyB64;
  fishData->myPrivateKey = myPrivateKey;
  fishData->sharedKey = sharedKey;
  fishData->key_created_at = now;
  FishKeys[nick] = fishData;
  sdprintf("Set key for %s: %s", nick, sharedKey.c_str());
  return;
}

void handle_DH1080_finish(const char* nick, const char* uhost, const char* from, struct userrec* u, const bd::String theirPublicKeyB64) {
  if (!FishKeys.contains(nick)) {
    putlog(LOG_MSGS, "*", "[FiSH] Unexpected DH1080_FINISH from (%s!%s) - ignoring", nick, uhost);
    return;
  }

  fish_data_t* fishData = FishKeys[nick];
  bd::String sharedKey;

  if (!DH1080_comp(fishData->myPrivateKey, theirPublicKeyB64, sharedKey)) {
    sdprintf("Error computing DH1080 for %s: %s", nick, sharedKey.c_str());
    return;
  }

  putlog(LOG_MSGS, "*", "[FiSH] Key successfully set for (%s!%s)", nick, uhost);
  fishData->sharedKey = sharedKey;
  sdprintf("Set key for %s: %s", nick, sharedKey.c_str());
  return;
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

      /* remove the ctcp in msg */
      memmove(p1 - 1, p + 1, strlen(p + 1) + 1);

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

        bd::String smsg(msg);
        bd::String which = newsplit(smsg);

        if (which == "DH1080_INIT") {
          bd::String theirPublicKeyB64(newsplit(smsg));
          handle_DH1080_init(nick, uhost, from, u, theirPublicKeyB64);
        } else if (which == "DH1080_FINISH") {
          bd::String theirPublicKeyB64(newsplit(smsg));
          handle_DH1080_finish(nick, uhost, from, u, theirPublicKeyB64);
        } else {
          putlog(LOG_MSGS, "*", "-%s (%s)- %s", nick, uhost, msg);
        }
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
  // Only check if we're not on jupenick, or there is no jupenick and we're not on the preferred nick
  bool have_jupenick = jupenick[0] ? match_my_nick(jupenick) : 0;
  if (!have_jupenick) {
    /* See if my nickname is in use and if if my nick is right.  */
    if (jupenick[0] && !have_jupenick) {
      if (use_monitor) // If our nick is juped due to split, we will NOT get a RPL_MONOFFLINE when it is available. So check periodically.
        dprintf(DP_SERVER, "MONITOR S\n");
      else
        dprintf(DP_SERVER, "ISON %s %s\n", origbotname, jupenick);
    } else if (!match_my_nick(origbotname)) {
      if (use_monitor) // If our nick is juped due to split, we will NOT get a RPL_MONOFFLINE when it is available. So check periodically.
        dprintf(DP_SERVER, "MONITOR S\n");
      else
        dprintf(DP_SERVER, "ISON %s\n", origbotname);
    }
  }
}

#ifdef not_used
/* Called once a minute... but if we're the only one on the
 * channel, we only wanna send out "lusers" once every 5 mins.
 */
static void minutely_checks()
{
  /* Only check if we have already successfully logged in.  */
  if (server_online) {
    static int count = 4;
    bool ok = 0;

    for (struct chanset_t *chan = chanset; chan; chan = chan->next) {
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
#endif

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

static void nick_which(const char* nick, bool& is_jupe, bool& is_orig) {
  if (is_orig == 0 && !rfc_ncasecmp(nick, origbotname, nick_len)) {
    is_orig = 1;
  }
  /* It's possible jupenick==nick.  Set both flags. */
  if (is_jupe == 0 && jupenick[0] && !rfc_ncasecmp(nick, jupenick, nick_len)) {
    is_jupe = 1;
  }
}

static void nick_available(bool is_jupe, bool is_orig) {
  if (jupenick[0] && is_jupe && !match_my_nick((jupenick))) {
    /* Ensure we aren't processing a QUIT/NICK and a MONITOR, or just some screw up */
    if (!tried_jupenick || ((now - tried_jupenick) > 2)) {
      tried_jupenick = now;
      dprintf(DP_MODE_NEXT, "NICK %s\n", jupenick);
      if (!jnick_juped)
        putlog(LOG_MISC, "*", "Switching back to jupenick '%s'", jupenick);
    }
    // Don't switch to the nick if already on jupenick
  } else if (is_orig && !match_my_nick(origbotname) && (!jupenick[0] || !match_my_nick(jupenick))) {
    if (!tried_nick || ((now - tried_nick) > 2)) {
      tried_nick = now;
      if (!nick_juped) {
        // Only reset altnick if the nick isn't juped - perfectly fine staying on rotated nick if nick_delay is in effect
        altnick_char = rolls = 0;
        putlog(LOG_MISC, "*", "Switching back to nick '%s'", origbotname);
      }
      dprintf(DP_MODE_NEXT, "NICK %s\n", origbotname);
    }
  }
}

void nicks_available(char* buf, char delim, bool buf_contains_available) {
  if (!buf[0] || !keepnick) return;
  bool is_jupe = 0, is_orig = 0;

  char *nick = NULL;
  if (delim) {
    while ((nick = newsplit(&buf, delim))[0]) {
      nick_which(nick, is_jupe, is_orig);
      if (is_jupe && is_orig) {
        break;
      }
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

void real_release_nick(void *data) {
  if (match_my_nick(jupenick)) {
    altnick_char = rolls = 0;
    tried_nick = now;
    dprintf(DP_MODE, "NICK %s\n", origbotname);
    putlog(LOG_MISC, "*", "Releasing jupenick '%s' and switching back to nick '%s'", jupenick, origbotname);
  }
}

void release_nick(const char* nick) {
  // Only do this if currently on a jupenick
  if (jupenick[0] && ((!nick && match_my_nick(jupenick)) || (nick && !rfc_ncasecmp(jupenick, nick, nick_len)))) {
    keepnick = 0;

    // Delay releasing nick for 2 seconds to allow botnet to receive orders and user to type /NICK
    egg_timeval_t howlong;
    howlong.sec = 2;
    howlong.usec = 0;

    release_time = now;

    timer_create(&howlong, "release_nick", (Function) real_release_nick);

  } else if (!nick)
    putlog(LOG_CMDS, "*", "Not releasing nickname. (Not currently on a jupenick)");
}

/* This is a reply to MONITOR: Online
 * RPL_MONONLINE
 * :<server> 730 <nick|*> :nick!user@host[,nick!user@host]*
 */
static void got730(char* from, char* msg)
{
  char *tmp = newsplit(&msg);
  fixcolon(msg);


  if (tmp[0] && (!strcmp(tmp, "*") || match_my_nick(tmp))) {
    // If any of the nicks online match my jupenick or nick, unset nick_juped and jnick_juped
    // This will prevent the bot from thinking its nick is juped if someone else got it after a netsplit
    char *nick = NULL;
    while ((nick = newsplit(&msg, ','))[0]) {
      bool is_jupe = 0, is_orig = 0;
      nick_which(nick, is_jupe, is_orig);
      if (is_jupe && jnick_juped) jnick_juped = 0;
      else if (is_orig && nick_juped) nick_juped = 0;
    }
  }
}

/* This is a reply to MONITOR: Offline
 * RPL_MONOFFLINE
 * :<server> 731 <nick|*> :nick[,nick1]*
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

/*
 * :<server> 421 <nick> <command> :Unknown command
 */
static void got421(char *from, char *msg)
{
  char *command = NULL;

  newsplit(&msg);	/* nick */
  command = newsplit(&msg);

  if (use_monitor && !strcasecmp(command, "MONITOR")) {
    /* The command doesn't work despite 005 claiming to have it.
     * Disable MONITOR usage to fallback on ISON. */
    use_monitor = 0;
  }
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
    is_jnick = 1;
    jnick_juped = 1;
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
      strlcpy(botname, origbotname, sizeof(botname));
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
    else if (!rfc_ncasecmp(tmp, origbotname, nick_len))
      nick_juped = 0;

    tried_nick = 0;

    sdprintf("433: botname: %s tmp: %s rolls: %d altnick_char: %c tried_jupenick: %li", botname, tmp, rolls, altnick_char, (long)tried_jupenick);
    // Tried jupenick, but already on origbotname (or a rolled nick), stay on it.
    if (tried_jupenick && (match_my_nick(origbotname) || rolls || altnick_char != 0)) {
      putlog(LOG_MISC, "*", "%sNICK IN USE: '%s' (keeping '%s').", tried_jupenick ? "JUPE" : "", tmp, botname);
    } else {  //Else need to find a new nick
      // Failed to get jupenick, not on origbotname now, try for origbotname and rotate from there
      if (tried_jupenick) {
        strlcpy(rnick, origbotname, sizeof(rnick));
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
      chan->ircnet_status &= ~(CHAN_JOINING);
      if (chan->ircnet_status & CHAN_ACTIVE) {
	putlog(LOG_MISC, "*", "Can't change nickname on %s.  Is my nickname banned?", s);
      } else {
	if (!channel_juped(chan)) {
	  putlog(LOG_MISC, "*", "Channel %s is juped. :(", s);
	  chan->ircnet_status |= CHAN_JUPED;
	}
      }
    }
  } else if (server_online) {
    if (!rfc_ncasecmp(s, origbotname, nick_len)) {
      if (!nick_juped)
        putlog(LOG_MISC, "*", "NICK IS TEMPORARILY UNAVAILABLE: '%s' (keeping '%s').", s, botname);
      nick_juped = 2;
    } else if (jupenick[0] && !rfc_ncasecmp(s, jupenick, nick_len)) {
      if (!jnick_juped)
        putlog(LOG_MISC, "*", "JUPENICK IS TEMPORARILY UNAVAILABLE: '%s' (keeping '%s').", s, botname);
      jnick_juped = 2;
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
  putlog(LOG_SERV, "*", "%s says I'm not registered, trying next one.", from);
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
  putlog(LOG_SERV, "*", "Disconnecting from %s.", dcc[servidx].host);
  nuke_server("Bah, stupid error messages.");
  return 1;
}

/* Got nick change.
 */
static int gotnick(char *from, char *msg)
{
  char *nick = NULL, *buf = NULL, *buf_ptr = NULL;

  //Done to prevent gotnick in irc.mod getting a mangled from
  buf = buf_ptr = strdup(from);
  nick = splitnick(&buf);
  fixcolon(msg);

  if (match_my_nick(nick)) {
    /* Regained nick! */
    strlcpy(botname, msg, sizeof(botname));

    tried_jupenick = 0;
    tried_nick = 0;

    if (jupenick[0] && !rfc_ncasecmp(msg, jupenick, nick_len)) {
      altnick_char = rolls = 0;
      putlog(LOG_SERV | LOG_MISC, "*", "Regained jupenick '%s'.", msg);
      jnick_juped = 0;
      nick_juped = 0; // Unset this, no reason for it now.
    } else if (!strncmp(msg, origbotname, nick_len)) {
      altnick_char = rolls = 0;
      putlog(LOG_SERV | LOG_MISC, "*", "Regained nickname '%s'.", msg);
      nick_juped = 0;
    } else if ((keepnick || release_time) && strcmp(nick, msg)) {

      // Was this an attempt at a nick thru rotation?
      if (!strcmp(rnick, msg)) {
        putlog(LOG_MISC, "*", "Nickname changed to '%s'", msg);
      } else {
        putlog(LOG_SERV | LOG_MISC, "*", "Nickname changed to '%s'???", msg);
        nicks_available(nick);
      }
    } else
      putlog(LOG_SERV | LOG_MISC, "*", "Nickname changed to '%s'???", msg);
  } else if ((rfc_casecmp(nick, msg))) { //Ignore case changes
    if (keepnick)
      nicks_available(nick);
    else if (keepnick && release_time) {
      // Someone else has regained the nickname, revert to keeping the nick in case they lose it
      // within the release_time window.
      keepnick = 0;
      release_time = 0;
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
    if (match_my_nick(ch) && !strcmp(curnetwork, "IRCnet")) {
      // Umode +r is restricted on IRCnet, can only chat.
      fixcolon(buf);
      if ((buf[0] == '+') && strchr(buf, 'r')) {
	putlog(LOG_SERV, "*", "%s has me i-lined (jumping)", dcc[servidx].host);
	nuke_server("i-lines suck");
      }
    }
  }
  free(buf_ptr);
  return 0;
}

static void end_burstmode();
void irc_init();

static void disconnect_server(int idx)
{
  server_online = 0;
  if ((serv != dcc[idx].sock) && serv >= 0)
    killsock(serv);
  if (dcc[idx].sock >= 0)
    killsock(dcc[idx].sock);
  dcc[idx].sock = -1;
  serv = -1;
  botuserhost[0] = 0;
  botuserip[0] = 0; 
  /* Features should have a struct that can be bzero'd */
  use_monitor = 0;
  floodless = 0;
  use_penalties = 0;
  use_354 = 0;
  deaf_char = 0;
  in_deaf = 0;
  callerid_char = 0;
  in_callerid = 0;
  use_exempts = 0;
  use_invites = 0;
  have_cprivmsg = 0;
  have_cnotice = 0;
  use_flood_count = 0;
  modesperline = 0;
  trying_server = 0;
  end_burstmode();
  keepnick = 1;
}

static void eof_server(int idx)
{
  putlog(LOG_SERV, "*", "Disconnected from %s (EOF)", dcc[idx].host);
  disconnect_server(idx);
  lostdcc(idx);
}

static void display_server(int idx, char *buf, size_t bufsiz)
{
  simple_snprintf(buf, bufsiz, "%s  (lag: %d)", trying_server ? "conn" : "serv", server_lag);
}

static void connect_server(void);

static void kill_server(int idx, void *x)
{
  disconnect_server(idx);
  Auth::DeleteAll();
  if (reset_chans == 2) {
    irc_init();
  }
  reset_chans = 0;
  /* Invalidate the cmd_swhois cache callback data */
  for (int i = 0; i < dcc_total; i++) {
    if (dcc[i].type && dcc[i].whois[0]) {
      dcc[i].whois[0] = 0;
      dcc[i].whowas = 0;
    }
  }
  if (!segfaulted) { //Avoid if crashed, too many free()/malloc() in here
    for (struct chanset_t *chan = chanset; chan; chan = chan->next) {
      clear_channel(chan, 1);
    }
  }
  servidx = -1;
  /* A new server connection will be automatically initiated in
     about 2 seconds. */
}

static void timeout_server(int idx)
{
  putlog(LOG_SERV, "*", "Timeout: connect to %s", dcc[idx].host);
  disconnect_server(idx);
  lostdcc(idx);
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

  if (unlikely(trying_server)) {
    strlcpy(dcc[idx].nick, "(server)", sizeof(dcc[idx].nick));
    if (ssl_use) {
      putlog(LOG_SERV, "*", "Connected to %s with SSL", dcc[idx].host);
    } else {
      putlog(LOG_SERV, "*", "Connected to %s", dcc[idx].host);
    }

    trying_server = 0;
    /*
    servidx = idx;
    serv = dcc[idx].sosck;
    */
    SERVER_SOCKET.timeout_val = 0;

    // Setup timer for conecting
    waiting_for_awake = 1;
    lastpingtime = now - (stoned_timeout - 30); //30 seconds to reach 001
  } else if (server_online) // Only set once 001 has been received
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
    last_time.sec += 2;
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
      last_time.sec += 1;
      if (debug_output)
        putlog(LOG_SRVOUT, "*", "adding 1sec penalty (remote whois)");
    }
  }

  got318_369(from, msg, 0);
  return 0;
}

static void irc_whois(char *, const char*, const char *, ...) __attribute__((format(printf, 3, 4)));

static void
irc_whois(char *nick, const char* prefix, const char *format, ...)
{
  char va_out[2001] = "";
  va_list va;

  va_start(va, format);
  egg_vsnprintf(va_out, sizeof(va_out) - 1, format, va);
  va_end(va);

  for (int idx = 0; idx < dcc_total; idx++) {
    if (dcc[idx].type && dcc[idx].whois[0] && !rfc_casecmp(nick, dcc[idx].whois)) {
      if (prefix)
        dumplots(idx, prefix, va_out);
      else
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

  irc_whois(nick, NULL, "$b%s$b [%s@%s]", nick, username, address);

  // FIXME: This should incorporate userip and member/client lookups - and should ACT on discovering a user.
  // FIXME: This should also use whois_actually to cache userip
  simple_snprintf(uhost, sizeof uhost, "%s!%s@%s", nick, username, address);
  if ((u = get_user_by_host(uhost))) {
    int idx = 0;
    for (idx = 0; idx < dcc_total; idx++) {
      if (dcc[idx].type && dcc[idx].whois[0] && !rfc_casecmp(nick, dcc[idx].whois)) {
        if (whois_access(dcc[idx].user, u)) {
          irc_whois(nick, " username : ", "$u%s$u", u->handle);
        }
        break;
      }
    }
  }

  irc_whois(nick, " ircname  : ", "%s", msg);
  
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

  char *chans = (char *) calloc(1, len), *p = NULL;

  while ((chname = newsplit(&channels))[0]) {
    /* skip any modes in front of #chan */
    if (!(p = strchr(chname, '#')))
      if (!(p = strchr(chname, '&')))
        if (!(p = strchr(chname, '!')))
          continue;

    chan = findchan(p);

    if (chan && !publicOnly)
     get_user_flagrec(u, &fr, chan->dname, chan);

    if (!chan || 
        
        (!publicOnly && (
          getnick(u->handle, chan)[0] ||
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
        irc_whois(nick, " channels : ", "%s", chans);
        free(chans);
      }
      chans = NULL;
      /* Show all of the 'public' channels */
      if ((chans = hide_chans(nick, dcc[idx].user, msg, 1))) {
        irc_whois(nick, " public   : ", "%s", chans);
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

  irc_whois(nick, " server   : ", "%s [%s]", server, msg);
  return 0;
}

/* 301 $me nick :away msg */
static int got301(char *from, char *msg)
{
  char *nick = NULL;

  newsplit(&msg);
  nick = newsplit(&msg);
  fixcolon(msg);

  irc_whois(nick, " away     : ", "%s", msg);

  return 0;
}

/* got 302: userhost
 * <server> 302 <to> :<nick??user@host>
 */
static int got302(char *from, char *msg)
{
  char *p = NULL, *nick = NULL, *uhost = NULL;

  newsplit(&msg);
  fixcolon(msg);

  p = strchr(msg, '=');
  if (!p)
    p = strchr(msg, '*');
  if (!p)
    return 0;
  *p = 0;
  nick = msg;
  p += 2;		/* skip =|* plus the next char */
  uhost = p;

  if ((p = strchr(uhost, ' ')))
    *p = 0;

  if (match_my_nick(nick)) {
    strlcpy(botuserip, uhost, UHOSTLEN);
    sdprintf("botuserip: %s", botuserip);
    return 0;
  }

  return 0;
}

/* 313 $me nick :server text */
static int got313(char *from, char *msg)
{
  char *nick = NULL;
  
  newsplit(&msg);
  nick = newsplit(&msg);
  fixcolon(msg);
 
  irc_whois(nick, "          : ", "$b%s$b", msg);

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
  irc_whois(nick, " idle     : " , "%d days %d hours %d mins %d secs [signon: %s]", mydays, myhours, mymins, mysecs, date);

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

  irc_whois(nick, NULL, "%s", msg);
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
  irc_whois(nick, NULL, "%s", msg);
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
  irc_whois(nick, NULL, "%s", msg);
 
  return 0;
}

/* 465 info_ :You are banned from this server- Temporary K-line 1 min. - Testing kline notices (2008/4/3 09.51) */
static int got465(char *from, char *msg)
{
  newsplit(&msg); /* nick */
  fixcolon(msg);
  putlog(LOG_SERV, "*", "I am klined: %s", msg);
  putlog(LOG_SERV, "*", "Disconnecting from %s.", dcc[servidx].host);
  nuke_server("I am klined!");
  return 1;                                           
}

/* 718 $me nick user@host :msg 
 * for receiving a msg while +g
 * csircd: :irc.nac.net 718 bryand_ bryand_ bryan bryan@shatow.net :is messaging you, but you have CALLERID enabled (umode +g)
 * ratbox: :irc.servercentral.net 718 bryand_ bryan bryan@shatow.net :is messaging you, and you have umode +g.
 * hybrid: :irc.swepipe.se 718 jjdjff bryan[bryan@shatow.net] :is messaging you, and you are umode +g.
 */
static int got718(char *from, char *msg)
{
  char *nick = NULL, *uhost = NULL;
  char s[UHOSTLEN + 2] = "";

  newsplit(&msg);
  nick = newsplit(&msg);
  if (match_my_nick(nick)) // CSIRCD is stupid.
    nick = newsplit(&msg);
  else if (msg[0] == ':') { // Hybrid is stupid too.
    char *p = strchr(nick, '['), *p2 = NULL;
    if (p) {
      *p++ = 0;
      if ((p2 = strchr(p, ']'))) {
        uhost = p;
        *p2 = 0;
      }
    }
  }

  if (!uhost)
    uhost = newsplit(&msg);
  fixcolon(msg);

  simple_snprintf(s, sizeof(s), "%s!%s", nick, uhost);

  if (match_ignore(s)) {
    return 0;
  }

  if (ischanhub()) {
    struct userrec *u = NULL;

    u = get_user_by_host(s);
    if (u) {
      struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0 };

      get_user_flagrec(u, &fr, NULL);
      if (glob_op(fr) || chan_op(fr) || glob_voice(fr) || chan_voice(fr)) {
        putlog(LOG_WALL, "*", "(CALLERID) !%s! (%s!%s) %s (Accepting user)", u->handle, nick, uhost, msg);
        dprintf(DP_HELP, "ACCEPT %s\n", nick);
        dprintf(DP_HELP, "PRIVMSG %s :You have been accepted. Please send your message again.\n", nick);
        if (fish_auto_keyx) {
          keyx(nick, "Callerid accepted");
        }
      } else {
        putlog(LOG_WALL, "*", "(CALLERID) !%s! (%s!%s) %s (User is not +o or +v)", u->handle, nick, uhost, msg);
      }
    } else {
      putlog(LOG_WALL, "*", "(CALLERID) !*! (%s!%s) %s (User unknown)", nick, uhost, msg);
    }
  } else {
    putlog(LOG_WALL, "*", "(CALLERID) (%s!%s) %s (I'm not a chathub (+c))", nick, uhost, msg);
  }

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
  {"302",       "",     (Function) got302,		NULL, LEAF},
  {"303",	"",	(Function) got303,		NULL, LEAF},
  {"421",	"",	(Function) got421,		NULL, LEAF},
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
  {"730",	"",	(Function) got730,		NULL, LEAF},	/* RPL_MONONLINE */
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
  in_port_t botserverport = 0;

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

    if (ssl_use) {
      putlog(LOG_SERV, "*", "Trying SSL server %s:%d", botserver, botserverport);
    } else {
      putlog(LOG_SERV, "*", "Trying server %s:%d", botserver, botserverport);
    }

    dcc[newidx].port = botserverport;
    strlcpy(dcc[newidx].nick, "(server)", sizeof(dcc[newidx].nick));
    strlcpy(dcc[newidx].host, botserver, sizeof(dcc[newidx].host));

    botuserhost[0] = 0;

    nick_juped = 0;
    jnick_juped = 0;
    tried_jupenick = 0;
    tried_nick = 0;
    altnick_char = rolls = 0;
    use_monitor = 0;
    include_lk = 1;

    for (chan = chanset; chan; chan = chan->next)
      chan->ircnet_status &= ~CHAN_JUPED;

    dcc[newidx].timeval = now;
    dcc[newidx].sock = -1;
    dcc[newidx].u.dns->cbuf = strdup(pass);

    cycle_time = server_cycle_wait;		/* wait N seconds before attempting next server connect */

    /* I'm resolving... don't start another server connect request */
    resolvserv = 1;
    /* Resolve the hostname. */
    int dns_id = egg_dns_lookup(botserver, 20, server_dns_callback, (void *) (long) newidx, conf.bot->net.v6 ? DNS_LOOKUP_AAAA : DNS_LOOKUP_A);
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

  Context;

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
  const char* dns_type = NULL;
  bd::String ip_from_dns;

#ifdef USE_IPV6
  /* If IPv6 is wanted, ensure we are connecting to an IPv6 server, otherwise skip it */
  if (conf.bot->net.v6) {
    ip_from_dns = dns_find_ip(ips, AF_INET6);
    if (!ip_from_dns.length()) {
      dns_type = "IPv6";
      goto fatal_dns;
    }
  } else
#endif /* USE_IPV6 */
  {
    /* If IPv4 is wanted, don't connect to IPv6! */
    ip_from_dns = dns_find_ip(ips, AF_INET);
    if (!ip_from_dns.length()) {
      dns_type = "IPv4";
      goto fatal_dns;
    }
  }
  ip = ip_from_dns.dup();

  get_addr(ip, &addr);
 
  if (addr.family == AF_INET)
    dcc[idx].addr = htonl(addr.u.addr.s_addr);

  strlcpy(serverpass, (char *) dcc[idx].u.dns->cbuf, sizeof(serverpass));
  changeover_dcc(idx, &SERVER_SOCKET, 0);

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
#ifdef EGG_SSL_EXT
    if (ssl_use) { /* kyotou */
      if (net_switch_to_ssl(serv) == 0) {
        putlog(LOG_SERV, "*", "SSL Failed to connect to %s (Error while switching to SSL)", dcc[servidx].host);
        trying_server = 0;
        lostdcc(servidx);
        return;
      }
    }
#endif
    /* Queue standard login */
    dcc[idx].timeval = now;
    SERVER_SOCKET.timeout_val = &server_timeout;
    /* Another server may have truncated it, so use the original */
    if (jupenick[0])
      strlcpy(botname, jupenick, sizeof(botname));
    else
      strlcpy(botname, origbotname, sizeof(botname));
    /* Start alternate nicks from the beginning */
    altnick_char = rolls = 0;
    /* reset counter so first ctcp is dumped for tcms */
    first_ctcp_check = 0;

    // Just connecting, set last queue time to the past.
    reset_flood();
    end_burstmode();
    use_flood_count = 0;
    real_msgburst = msgburst;
    real_msgrate = msgrate;

    if (serverpass[0])
      dprintf(DP_MODE, "PASS %s\n", serverpass);
    dprintf(DP_MODE, "NICK %s\n", botname);
    dprintf(DP_MODE, "USER %s localhost %s :%s\n", botuser, dcc[idx].host, replace_vars(botrealname));
    /* Wait for async result now */
  }

  delete[] ip;
  return;

fatal_dns:
  putlog(LOG_SERV, "*", "Failed connect to %s (Could not DNS as %s)", host, dns_type);
  trying_server = 0;
  lostdcc(idx);
  return;
}
/* vim: set sts=2 sw=2 ts=8 et: */
