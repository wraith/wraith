#ifdef LEAF
#ifdef S_NODELAY
#include <netinet/tcp.h>
#endif
char cursrvname[120] = "";
static time_t last_ctcp = (time_t) 0L;
static int count_ctcp = 0;
static char altnick_char = 0;
static int
gotfake433 (char *from)
{
  int l = strlen (botname) - 1;
  char *oknicks = "-_\\`^[]{}";
  Context;
  if (altnick_char == 0)
    {
      altnick_char = oknicks[0];
      if (l + 1 == NICKMAX)
	{
	  botname[l] = altnick_char;
	}
      else
	{
	  botname[++l] = altnick_char;
	  botname[l + 1] = 0;
	}
    }
  else
    {
      char *p = strchr (oknicks, altnick_char);
      p++;
      if (!*p)
	altnick_char = 'a' + random () % 26;
      else
	altnick_char = (*p);
      botname[l] = altnick_char;
    }
  putlog (LOG_MISC, "*", IRC_BOTNICKINUSE, botname);
  dprintf (DP_MODE, "NICK %s\n", botname);
  return 0;
}
static int
check_tcl_msg (char *cmd, char *nick, char *uhost, struct userrec *u,
	       char *args)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0, 0 };
  char *hand = u ? u->handle : "*";
  int x;
  get_user_flagrec (u, &fr, NULL);
  Tcl_SetVar (interp, "_msg1", nick, 0);
  Tcl_SetVar (interp, "_msg2", uhost, 0);
  Tcl_SetVar (interp, "_msg3", hand, 0);
  Tcl_SetVar (interp, "_msg4", args, 0);
  x =
    check_tcl_bind (H_msg, cmd, &fr, " $_msg1 $_msg2 $_msg3 $_msg4",
		    MATCH_EXACT | BIND_HAS_BUILTINS | BIND_USE_ATTR);
  if (x == BIND_EXEC_LOG)
    putlog (LOG_CMDS, "*", "(%s!%s) !%s! %s %s", nick, uhost, hand, cmd,
	    args);
  return ((x == BIND_MATCHED) || (x == BIND_EXECUTED)
	  || (x == BIND_EXEC_LOG));
}
static void
check_tcl_notc (char *nick, char *uhost, struct userrec *u, char *dest,
		char *arg)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0, 0 };
  get_user_flagrec (u, &fr, NULL);
  Tcl_SetVar (interp, "_notc1", nick, 0);
  Tcl_SetVar (interp, "_notc2", uhost, 0);
  Tcl_SetVar (interp, "_notc3", u ? u->handle : "*", 0);
  Tcl_SetVar (interp, "_notc4", arg, 0);
  Tcl_SetVar (interp, "_notc5", dest, 0);
  check_tcl_bind (H_notc, arg, &fr,
		  " $_notc1 $_notc2 $_notc3 $_notc4 $_notc5",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
} static void
check_tcl_msgm (char *cmd, char *nick, char *uhost, struct userrec *u,
		char *arg)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0, 0 };
  char args[1024];
  if (arg[0])
    simple_sprintf (args, "%s %s", cmd, arg);
  else
    strcpy (args, cmd);
  get_user_flagrec (u, &fr, NULL);
  Tcl_SetVar (interp, "_msgm1", nick, 0);
  Tcl_SetVar (interp, "_msgm2", uhost, 0);
  Tcl_SetVar (interp, "_msgm3", u ? u->handle : "*", 0);
  Tcl_SetVar (interp, "_msgm4", args, 0);
  check_tcl_bind (H_msgm, args, &fr, " $_msgm1 $_msgm2 $_msgm3 $_msgm4",
		  MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
}
static int
check_tcl_raw (char *from, char *code, char *msg)
{
  int x;
  Context;
  Tcl_SetVar (interp, "_raw1", from, 0);
  Tcl_SetVar (interp, "_raw2", code, 0);
  Tcl_SetVar (interp, "_raw3", msg, 0);
  x =
    check_tcl_bind (H_raw, code, 0, " $_raw1 $_raw2 $_raw3",
		    MATCH_EXACT | BIND_STACKABLE | BIND_WANTRET);
  return (x == BIND_EXEC_LOG);
}
static int
check_tcl_ctcpr (char *nick, char *uhost, struct userrec *u, char *dest,
		 char *keyword, char *args, p_tcl_bind_list table)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0, 0 };
  int x;
  get_user_flagrec (u, &fr, NULL);
  Tcl_SetVar (interp, "_ctcpr1", nick, 0);
  Tcl_SetVar (interp, "_ctcpr2", uhost, 0);
  Tcl_SetVar (interp, "_ctcpr3", u ? u->handle : "*", 0);
  Tcl_SetVar (interp, "_ctcpr4", dest, 0);
  Tcl_SetVar (interp, "_ctcpr5", keyword, 0);
  Tcl_SetVar (interp, "_ctcpr6", args, 0);
  x =
    check_tcl_bind (table, keyword, &fr,
		    " $_ctcpr1 $_ctcpr2 $_ctcpr3 $_ctcpr4 $_ctcpr5 $_ctcpr6",
		    MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE |
		    ((table == H_ctcp) ? BIND_WANTRET : 0));
  return (x == BIND_EXEC_LOG) || (table == H_ctcr);
}
static int
check_tcl_wall (char *from, char *msg)
{
  int x;
  Tcl_SetVar (interp, "_wall1", from, 0);
  Tcl_SetVar (interp, "_wall2", msg, 0);
  x =
    check_tcl_bind (H_wall, msg, 0, " $_wall1 $_wall2",
		    MATCH_MASK | BIND_STACKABLE);
  if (x == BIND_EXEC_LOG)
    {
      putlog (LOG_WALL, "*", "!%s! %s", from, msg);
      return 1;
    }
  else
    return 0;
}
static int
check_tcl_flud (char *nick, char *uhost, struct userrec *u, char *ftype,
		char *chname)
{
  int x;
  Tcl_SetVar (interp, "_flud1", nick, 0);
  Tcl_SetVar (interp, "_flud2", uhost, 0);
  Tcl_SetVar (interp, "_flud3", u ? u->handle : "*", 0);
  Tcl_SetVar (interp, "_flud4", ftype, 0);
  Tcl_SetVar (interp, "_flud5", chname, 0);
  x =
    check_tcl_bind (H_flud, ftype, 0,
		    " $_flud1 $_flud2 $_flud3 $_flud4 $_flud5",
		    MATCH_MASK | BIND_STACKABLE | BIND_WANTRET);
  return (x == BIND_EXEC_LOG);
}
static int
match_my_nick (char *nick)
{
  if (!rfc_casecmp (nick, botname))
    return 1;
  return 0;
}
static int
got001 (char *from, char *msg)
{
  struct server_list *x;
  int i, servidx = findanyidx (serv);
  struct chanset_t *chan;
  server_online = now;
  fixcolon (msg);
  strncpyz (botname, msg, NICKLEN);
  altnick_char = 0;
  strncpy0 (cursrvname, from, sizeof (cursrvname));
  dprintf (DP_SERVER, "WHOIS %s\n", botname);
  if (initserver[0])
    do_tcl ("init-server", initserver);
  check_tcl_event ("init-server");
  x = serverlist;
  if (x == NULL)
    return 0;
  if (module_find ("irc", 0, 0))
    for (chan = chanset; chan; chan = chan->next)
      {
	chan->status &= ~(CHAN_ACTIVE | CHAN_PEND);
	if (!channel_inactive (chan))
	  dprintf (DP_SERVER, "JOIN %s %s\n",
		   (chan->name[0]) ? chan->name : chan->dname,
		   chan->channel.key[0] ? chan->channel.key : chan->key_prot);
      }
  if (egg_strcasecmp (from, dcc[servidx].host))
    {
      putlog (LOG_MISC, "*", "(%s claims to be %s; updating server list)",
	      dcc[servidx].host, from);
      for (i = curserv; i > 0 && x != NULL; i--)
	x = x->next;
      if (x == NULL)
	{
	  putlog (LOG_MISC, "*", "Invalid server list!");
	  return 0;
	}
      if (x->realname)
	nfree (x->realname);
      if (strict_servernames == 1)
	{
	  x->realname = NULL;
	  if (x->name)
	    nfree (x->name);
	  x->name = nmalloc (strlen (from) + 1);
	  strcpy (x->name, from);
	}
      else
	{
	  x->realname = nmalloc (strlen (from) + 1);
	  strcpy (x->realname, from);
	}
    }
  return 0;
}
static int
got442 (char *from, char *msg)
{
  char *chname;
  struct chanset_t *chan;
  struct server_list *x;
  int i;
  for (x = serverlist, i = 0; x; x = x->next, i++)
    if (i == curserv)
      {
	if (egg_strcasecmp (from, x->realname ? x->realname : x->name))
	  return 0;
	break;
      }
  newsplit (&msg);
  chname = newsplit (&msg);
  chan = findchan (chname);
  if (chan)
    if (!channel_inactive (chan))
      {
	module_entry *me = module_find ("channels", 0, 0);
	putlog (LOG_MISC, chname, IRC_SERVNOTONCHAN, chname);
	if (me && me->funcs)
	  (me->funcs[CHANNEL_CLEAR]) (chan, 1);
	chan->status &= ~CHAN_ACTIVE;
	dprintf (DP_MODE, "JOIN %s %s\n", chan->name,
		 chan->channel.key[0] ? chan->channel.key : chan->key_prot);
      }
  return 0;
}
static void
nuke_server (char *reason)
{
  if (serv >= 0)
    {
      int servidx = findanyidx (serv);
      if (reason && (servidx > 0))
	dprintf (servidx, "QUIT :%s\n", reason);
      disconnect_server (servidx);
      lostdcc (servidx);
    }
}
static char ctcp_reply[1024] = "";
static int lastmsgs[FLOOD_GLOBAL_MAX];
static char lastmsghost[FLOOD_GLOBAL_MAX][81];
static time_t lastmsgtime[FLOOD_GLOBAL_MAX];
static int
detect_flood (char *floodnick, char *floodhost, char *from, int which)
{
  char *p, ftype[10], h[1024];
  struct userrec *u;
  int thr = 0, lapse = 0, atr;
  u = get_user_by_host (from);
  atr = u ? u->flags : 0;
  if (atr & (USER_BOT | USER_FRIEND))
    return 0;
  switch (which)
    {
    case FLOOD_PRIVMSG:
    case FLOOD_NOTICE:
      thr = flud_thr;
      lapse = flud_time;
      strcpy (ftype, "msg");
      break;
    case FLOOD_CTCP:
      thr = flud_ctcp_thr;
      lapse = flud_ctcp_time;
      strcpy (ftype, "ctcp");
      break;
    }
  if ((thr == 0) || (lapse == 0))
    return 0;
  if (match_my_nick (floodnick))
    return 0;
  if (!egg_strcasecmp (floodhost, botuserhost))
    return 0;
  p = strchr (floodhost, '@');
  if (p)
    {
      p++;
      if (egg_strcasecmp (lastmsghost[which], p))
	{
	  strcpy (lastmsghost[which], p);
	  lastmsgtime[which] = now;
	  lastmsgs[which] = 0;
	  return 0;
	}
    }
  else
    return 0;
  if (lastmsgtime[which] < now - lapse)
    {
      lastmsgtime[which] = now;
      lastmsgs[which] = 0;
      return 0;
    }
  lastmsgs[which]++;
  if (lastmsgs[which] >= thr)
    {
      lastmsgs[which] = 0;
      lastmsgtime[which] = 0;
      lastmsghost[which][0] = 0;
      u = get_user_by_host (from);
      if (check_tcl_flud (floodnick, floodhost, u, ftype, "*"))
	return 0;
      simple_sprintf (h, "*!*@%s", p);
      putlog (LOG_MISC, "*", IRC_FLOODIGNORE1, p);
      addignore (h, botnetnick,
		 (which == FLOOD_CTCP) ? "CTCP flood" : "MSG/NOTICE flood",
		 now + (60 * ignore_time));
    }
  return 0;
}
static int
detect_avalanche (char *msg)
{
  int count = 0;
  unsigned char *p;
  for (p = (unsigned char *) msg; (*p) && (count < 8); p++)
    if ((*p == 7) || (*p == 1))
      count++;
  if (count >= 8)
    return 1;
  else
    return 0;
}
static int
gotmsg (char *from, char *msg)
{
  char *to, buf[UHOSTLEN], *nick, ctcpbuf[512], *uhost = buf, *ctcp;
  char *p, *p1, *code;
  struct userrec *u;
  int ctcp_count = 0;
  int ignoring;
  if (msg[0] && ((strchr (CHANMETA, *msg) != NULL) || (*msg == '@')))
    return 0;
  ignoring = match_ignore (from);
  to = newsplit (&msg);
  fixcolon (msg);
  strcpy (uhost, from);
  nick = splitnick (&uhost);
  if (flud_ctcp_thr && detect_avalanche (msg))
    {
      if (!ignoring)
	{
	  putlog (LOG_MODES, "*", "Avalanche from %s - ignoring", from);
	  p = strchr (uhost, '@');
	  if (p != NULL)
	    p++;
	  else
	    p = uhost;
	  simple_sprintf (ctcpbuf, "*!*@%s", p);
	  addignore (ctcpbuf, botnetnick, "ctcp avalanche",
		     now + (60 * ignore_time));
	}
    }
  ctcp_reply[0] = 0;
  p = strchr (msg, 1);
  while ((p != NULL) && (*p))
    {
      p++;
      p1 = p;
      while ((*p != 1) && (*p != 0))
	p++;
      if (*p == 1)
	{
	  *p = 0;
	  ctcp = strcpy (ctcpbuf, p1);
	  strcpy (p1 - 1, p + 1);
	  if (!ignoring)
	    detect_flood (nick, uhost, from,
			  strncmp (ctcp, "ACTION ",
				   7) ? FLOOD_CTCP : FLOOD_PRIVMSG);
	  p = strchr (msg, 1);
	  if (ctcp_count < answer_ctcp)
	    {
	      ctcp_count++;
	      if (ctcp[0] != ' ')
		{
		  code = newsplit (&ctcp);
		  if ((to[0] == '$') || strchr (to, '.'))
		    {
		      if (!ignoring)
			putlog (LOG_PUBLIC, to,
				"CTCP %s: %s from %s (%s) to %s", code, ctcp,
				nick, uhost, to);
		    }
		  else
		    {
		      u = get_user_by_host (from);
		      if (!ignoring || trigger_on_ignore)
			{
			  if (!check_tcl_ctcp (nick, uhost, u, to, code, ctcp)
			      && !ignoring)
			    {
			      if ((lowercase_ctcp
				   && !egg_strcasecmp (code, "DCC"))
				  || (!lowercase_ctcp
				      && !strcmp (code, "DCC")))
				{
				  code = newsplit (&ctcp);
				  if (!strcmp (code, "CHAT"))
				    {
				      if (!quiet_reject)
					{
					  if (u)
					    dprintf (DP_HELP,
						     "NOTICE %s :%s\n", nick,
						     "I'm not accepting call at the moment.");
					  else
					    dprintf (DP_HELP,
						     "NOTICE %s :%s\n", nick,
						     DCC_NOSTRANGERS);
					}
				      putlog (LOG_MISC, "*", "%s: %s",
					      DCC_REFUSED, from);
				    }
				  else
				    putlog (LOG_MISC, "*",
					    "Refused DCC %s: %s", code, from);
				}
			    }
			  if (!strcmp (code, "ACTION"))
			    {
			      putlog (LOG_MSGS, "*", "Action to %s: %s %s",
				      to, nick, ctcp);
			    }
			  else
			    {
			      putlog (LOG_MSGS, "*",
				      "CTCP %s: %s from %s (%s)", code, ctcp,
				      nick, uhost);
			    }
			}
		    }
		}
	    }
	}
    }
  if (ctcp_reply[0])
    {
      if (ctcp_mode != 2)
	{
	  dprintf (DP_HELP, "NOTICE %s :%s\n", nick, ctcp_reply);
	}
      else
	{
	  if (now - last_ctcp > flud_ctcp_time)
	    {
	      dprintf (DP_HELP, "NOTICE %s :%s\n", nick, ctcp_reply);
	      count_ctcp = 1;
	    }
	  else if (count_ctcp < flud_ctcp_thr)
	    {
	      dprintf (DP_HELP, "NOTICE %s :%s\n", nick, ctcp_reply);
	      count_ctcp++;
	    }
	  last_ctcp = now;
	}
    }
  if (msg[0])
    {
      if ((to[0] == '$') || (strchr (to, '.') != NULL))
	{
	  if (!ignoring)
	    {
	      detect_flood (nick, uhost, from, FLOOD_PRIVMSG);
	      putlog (LOG_MSGS | LOG_SERV, "*", "[%s!%s to %s] %s", nick,
		      uhost, to, msg);
	    }
	}
      else
	{
	  char *code;
	  struct userrec *u;
	  detect_flood (nick, uhost, from, FLOOD_PRIVMSG);
	  u = get_user_by_host (from);
	  code = newsplit (&msg);
	  rmspace (msg);
	  if (!ignoring || trigger_on_ignore)
	    check_tcl_msgm (code, nick, uhost, u, msg);
	  if (!ignoring)
	    if (!check_tcl_msg (code, nick, uhost, u, msg))
	      putlog (LOG_MSGS, "*", "[%s] %s %s", from, code, msg);
	}
    }
  return 0;
}
static int
gotnotice (char *from, char *msg)
{
  char *to, *nick, ctcpbuf[512], *p, *p1, buf[512], *uhost = buf, *ctcp;
  struct userrec *u;
  int ignoring;
  if (msg[0] && ((strchr (CHANMETA, *msg) != NULL) || (*msg == '@')))
    return 0;
  ignoring = match_ignore (from);
  to = newsplit (&msg);
  fixcolon (msg);
  strcpy (uhost, from);
  nick = splitnick (&uhost);
  if (flud_ctcp_thr && detect_avalanche (msg))
    {
      if (!ignoring)
	putlog (LOG_MODES, "*", "Avalanche from %s", from);
      return 0;
    }
  p = strchr (msg, 1);
  while ((p != NULL) && (*p))
    {
      p++;
      p1 = p;
      while ((*p != 1) && (*p != 0))
	p++;
      if (*p == 1)
	{
	  *p = 0;
	  ctcp = strcpy (ctcpbuf, p1);
	  strcpy (p1 - 1, p + 1);
	  if (!ignoring)
	    detect_flood (nick, uhost, from, FLOOD_CTCP);
	  p = strchr (msg, 1);
	  if (ctcp[0] != ' ')
	    {
	      char *code = newsplit (&ctcp);
	      if ((to[0] == '$') || strchr (to, '.'))
		{
		  if (!ignoring)
		    putlog (LOG_PUBLIC, "*",
			    "CTCP reply %s: %s from %s (%s) to %s", code,
			    ctcp, nick, uhost, to);
		}
	      else
		{
		  u = get_user_by_host (from);
		  if (!ignoring || trigger_on_ignore)
		    {
		      check_tcl_ctcr (nick, uhost, u, to, code, ctcp);
		      if (!ignoring)
			putlog (LOG_MSGS, "*",
				"CTCP reply %s: %s from %s (%s) to %s", code,
				ctcp, nick, uhost, to);
		    }
		}
	    }
	}
    }
  if (msg[0])
    {
      if (((to[0] == '$') || strchr (to, '.')) && !ignoring)
	{
	  detect_flood (nick, uhost, from, FLOOD_NOTICE);
	  putlog (LOG_MSGS | LOG_SERV, "*", "-%s (%s) to %s- %s", nick, uhost,
		  to, msg);
	}
      else
	{
	  if ((nick[0] == 0) || (uhost[0] == 0))
	    {
	      if (strncmp (msg, "Highest connection count:", 25))
		putlog (LOG_SERV, "*", "-NOTICE- %s", msg);
	    }
	  else
	    {
	      detect_flood (nick, uhost, from, FLOOD_NOTICE);
	      u = get_user_by_host (from);
	      if (!ignoring || trigger_on_ignore)
		check_tcl_notc (nick, uhost, u, botname, msg);
	      if (!ignoring)
		putlog (LOG_MSGS, "*", "-%s (%s)- %s", nick, uhost, msg);
	    }
	}
    }
  return 0;
}
static int
got251 (char *from, char *msg)
{
  int i;
  char *servs;
  if (min_servs == 0)
    return 0;
  newsplit (&msg);
  fixcolon (msg);
  for (i = 0; i < 8; i++)
    newsplit (&msg);
  servs = newsplit (&msg);
  if (strncmp (msg, "servers", 7))
    return 0;
  while (*servs && (*servs < 32))
    servs++;
  i = atoi (servs);
  if (i < min_servs)
    {
      putlog (LOG_SERV, "*", IRC_AUTOJUMP, min_servs, i);
      nuke_server (IRC_CHANGINGSERV);
    }
  return 0;
}
static int
gotwall (char *from, char *msg)
{
  char *nick;
  int r;
  fixcolon (msg);
  r = check_tcl_wall (from, msg);
  if (r == 0)
    {
      if (strchr (from, '!'))
	{
	  nick = splitnick (&from);
	  putlog (LOG_WALL, "*", "!%s(%s)! %s", nick, from, msg);
	}
      else
	putlog (LOG_WALL, "*", "!%s! %s", from, msg);
    }
  return 0;
}
static void
server_10secondly ()
{
  char *alt;
  if (!server_online)
    return;
  if (keepnick)
    {
      if (strncmp (botname, origbotname, strlen (botname)))
	{
	  alt = get_altbotnick ();
	  if (alt[0] && egg_strcasecmp (botname, alt))
	    dprintf (DP_SERVER, "ISON :%s %s %s\n", botname, origbotname,
		     alt);
	  else
	    dprintf (DP_SERVER, "ISON :%s %s\n", botname, origbotname);
	}
    }
}
static void
minutely_checks ()
{
  static int count = 4;
  int ok = 0;
  struct chanset_t *chan;
  if (!server_online)
    return;
  if (min_servs == 0)
    return;
  for (chan = chanset; chan; chan = chan->next)
    if (channel_active (chan) && chan->channel.members == 1)
      {
	ok = 1;
	break;
      }
  if (!ok)
    return;
  count++;
  if (count >= 5)
    {
      dprintf (DP_SERVER, "LUSERS\n");
      count = 0;
    }
}
static int
gotpong (char *from, char *msg)
{
  newsplit (&msg);
  fixcolon (msg);
  server_lag = now - my_atoul (msg);
  if (server_lag > 99999)
    {
      server_lag = now - lastpingtime;
    }
  return 0;
}
static void
got303 (char *from, char *msg)
{
  char *tmp, *alt;
  int ison_orig = 0, ison_alt = 0;
  if (!keepnick || !strncmp (botname, origbotname, strlen (botname)))
    {
      return;
    }
  newsplit (&msg);
  fixcolon (msg);
  alt = get_altbotnick ();
  tmp = newsplit (&msg);
  if (tmp[0] && !rfc_casecmp (botname, tmp))
    {
      while ((tmp = newsplit (&msg))[0])
	{
	  if (!rfc_casecmp (tmp, origbotname))
	    ison_orig = 1;
	  else if (alt[0] && !rfc_casecmp (tmp, alt))
	    ison_alt = 1;
	}
      if (!ison_orig)
	{
	  if (!nick_juped)
	    putlog (LOG_MISC, "*", IRC_GETORIGNICK, origbotname);
	  dprintf (DP_SERVER, "NICK %s\n", origbotname);
	}
      else if (alt[0] && !ison_alt && rfc_casecmp (botname, alt))
	{
	  putlog (LOG_MISC, "*", IRC_GETALTNICK, alt);
	  dprintf (DP_SERVER, "NICK %s\n", alt);
	}
    }
}
static int
got432 (char *from, char *msg)
{
  char *erroneus;
  newsplit (&msg);
  erroneus = newsplit (&msg);
  if (server_online)
    putlog (LOG_MISC, "*", "NICK IS INVALID: %s (keeping '%s').", erroneus,
	    botname);
  else
    {
      putlog (LOG_MISC, "*", IRC_BADBOTNICK);
      if (!keepnick)
	{
	  makepass (erroneus);
	  erroneus[NICKMAX] = 0;
	  dprintf (DP_MODE, "NICK %s\n", erroneus);
	}
      return 0;
    }
  return 0;
}
static int
got433 (char *from, char *msg)
{
  char *tmp;
  if (server_online)
    {
      newsplit (&msg);
      tmp = newsplit (&msg);
      putlog (LOG_MISC, "*", "NICK IN USE: %s (keeping '%s').", tmp, botname);
      nick_juped = 0;
      return 0;
    }
  gotfake433 (from);
  return 0;
}
static int
got437 (char *from, char *msg)
{
  char *s;
  struct chanset_t *chan;
  newsplit (&msg);
  s = newsplit (&msg);
  if (s[0] && (strchr (CHANMETA, s[0]) != NULL))
    {
      chan = findchan (s);
      if (chan)
	{
	  if (chan->status & CHAN_ACTIVE)
	    {
	      putlog (LOG_MISC, "*", IRC_CANTCHANGENICK, s);
	    }
	  else
	    {
	      if (!channel_juped (chan))
		{
		  putlog (LOG_MISC, "*", IRC_CHANNELJUPED, s);
		  chan->status |= CHAN_JUPED;
		}
	    }
	}
    }
  else if (server_online)
    {
      if (!nick_juped)
	putlog (LOG_MISC, "*", "NICK IS JUPED: %s (keeping '%s').", s,
		botname);
      if (!rfc_casecmp (s, origbotname))
	nick_juped = 1;
    }
  else
    {
      putlog (LOG_MISC, "*", "%s: %s", IRC_BOTNICKJUPED, s);
      gotfake433 (from);
    }
  return 0;
}
static int
got438 (char *from, char *msg)
{
  newsplit (&msg);
  newsplit (&msg);
  fixcolon (msg);
  putlog (LOG_MISC, "*", "%s", msg);
  return 0;
}
static int
got451 (char *from, char *msg)
{
  putlog (LOG_MISC, "*", IRC_NOTREGISTERED1, from);
  nuke_server (IRC_NOTREGISTERED2);
  return 0;
}
static int
goterror (char *from, char *msg)
{
  if (msg[0] == ':')
    msg++;
  putlog (LOG_SERV, "*", "-ERROR from server- %s", msg);
  if (serverror_quit)
    {
      putlog (LOG_SERV, "*", "Disconnecting from server.");
      nuke_server ("Bah, stupid error messages.");
    }
  return 1;
}
static int
gotnick (char *from, char *msg)
{
  char *nick, *alt = get_altbotnick ();
  struct userrec *u;
  u = get_user_by_host (from);
  nick = splitnick (&from);
  fixcolon (msg);
  check_queues (nick, msg);
  if (match_my_nick (nick))
    {
      strncpyz (botname, msg, NICKLEN);
      altnick_char = 0;
      if (!strcmp (msg, origbotname))
	{
	  putlog (LOG_SERV | LOG_MISC, "*", "Regained nickname '%s'.", msg);
	  nick_juped = 0;
	}
      else if (alt[0] && !strcmp (msg, alt))
	putlog (LOG_SERV | LOG_MISC, "*", "Regained alternate nickname '%s'.",
		msg);
      else if (keepnick && strcmp (nick, msg))
	{
	  putlog (LOG_SERV | LOG_MISC, "*", "Nickname changed to '%s'???",
		  msg);
	  if (!rfc_casecmp (nick, origbotname))
	    {
	      putlog (LOG_MISC, "*", IRC_GETORIGNICK, origbotname);
	      dprintf (DP_SERVER, "NICK %s\n", origbotname);
	    }
	  else if (alt[0] && !rfc_casecmp (nick, alt)
		   && egg_strcasecmp (botname, origbotname))
	    {
	      putlog (LOG_MISC, "*", IRC_GETALTNICK, alt);
	      dprintf (DP_SERVER, "NICK %s\n", alt);
	    }
	}
      else
	putlog (LOG_SERV | LOG_MISC, "*", "Nickname changed to '%s'???", msg);
    }
  else if ((keepnick) && (rfc_casecmp (nick, msg)))
    {
      if (!rfc_casecmp (nick, origbotname))
	{
	  putlog (LOG_MISC, "*", IRC_GETORIGNICK, origbotname);
	  dprintf (DP_SERVER, "NICK %s\n", origbotname);
	}
      else if (alt[0] && !rfc_casecmp (nick, alt)
	       && egg_strcasecmp (botname, origbotname))
	{
	  putlog (LOG_MISC, "*", IRC_GETALTNICK, altnick);
	  dprintf (DP_SERVER, "NICK %s\n", altnick);
	}
    }
  return 0;
}
static int
gotmode (char *from, char *msg)
{
  char *ch;
  Context;
  ch = newsplit (&msg);
  if (strchr (CHANMETA, ch[0]) == NULL)
    {
      if (match_my_nick (ch) && check_mode_r)
	{
	  fixcolon (msg);
	  if ((msg[0] == '+') && strchr (msg, 'r'))
	    {
	      int servidx = findanyidx (serv);
	      putlog (LOG_MISC | LOG_JOIN, "*", "%s has me i-lined (jumping)",
		      dcc[servidx].host);
	      nuke_server ("i-lines suck");
	    }
	}
    }
  return 0;
}
static void
disconnect_server (int idx)
{
  if (server_online > 0)
    check_tcl_event ("disconnect-server");
  server_online = 0;
  if (dcc[idx].sock >= 0)
    killsock (dcc[idx].sock);
  dcc[idx].sock = (-1);
  serv = (-1);
  botuserhost[0] = 0;
}
static void
eof_server (int idx)
{
  putlog (LOG_SERV, "*", "%s %s", IRC_DISCONNECTED, dcc[idx].host);
  disconnect_server (idx);
  lostdcc (idx);
} static void
display_server (int idx, char *buf)
{
  sprintf (buf, "%s  (lag: %d)", trying_server ? "conn" : "serv", server_lag);
} static void connect_server (void);
static void
kill_server (int idx, void *x)
{
  module_entry *me;
  disconnect_server (idx);
  if ((me = module_find ("channels", 0, 0)) && me->funcs)
    {
      struct chanset_t *chan;
      for (chan = chanset; chan; chan = chan->next)
	(me->funcs[CHANNEL_CLEAR]) (chan, 1);
    }
}
static void
timeout_server (int idx)
{
  putlog (LOG_SERV, "*", "Timeout: connect to %s", dcc[idx].host);
  disconnect_server (idx);
  lostdcc (idx);
} static void server_activity (int idx, char *msg, int len);
static struct dcc_table SERVER_SOCKET =
  { "SERVER", 0, eof_server, server_activity, NULL, timeout_server,
display_server, NULL, kill_server, NULL };
int
isop (char *mode)
{
  int state = 0, cnt = 0;
  char *p;
  p = mode;
  while ((*p) && (*p != ' '))
    {
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

int
ismdop (char *mode)
{
  int state = 0, cnt = 0;
  char *p;
  p = mode;
  while ((*p) && (*p != ' '))
    {
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
static void
server_activity (int idx, char *msg, int len)
{
  char *from, *code, tmp[1024];
  char sign = '+';
  Context;
  if (trying_server)
    {
      strcpy (dcc[idx].nick, "(server)");
      putlog (LOG_SERV, "*", "Connected to %s", dcc[idx].host);
      trying_server = 0;
      SERVER_SOCKET.timeout_val = 0;
    }
  waiting_for_awake = 0;
  from = "";
  if (msg[0] == ':')
    {
      msg++;
      from = newsplit (&msg);
    }
  code = newsplit (&msg);
  if (!strcmp (code, STR ("MODE")) && (msg[0] == '#') && strchr (from, '!'))
    {
      char *modes[5] = { NULL, NULL, NULL, NULL, NULL };
      char *nfrom, *hfrom;
      int i;
      struct userrec *ufrom = NULL;
      struct chanset_t *chan = NULL;
      char work[1024], *wptr, *p;
      memberlist *m;
      int modecnt = 0, ops = 0, deops = 0, bans = 0, unbans = 0;
      strncpy0 (work, msg, sizeof (work));
      wptr = work;
      p = newsplit (&wptr);
      chan = findchan (p);
      p = newsplit (&wptr);
      while (*p)
	{
	  char *mp;
	  if (*p == '+')
	    sign = '+';
	  else if (*p == '-')
	    sign = '-';
	  else if (strchr (STR ("oblkvIe"), p[0]))
	    {
	      mp = newsplit (&wptr);
	      if (strchr ("ob", p[0]))
		{
		  modes[modecnt] = nmalloc (strlen (mp) + 4);
		  sprintf (modes[modecnt], STR ("%c%c %s"), sign, p[0], mp);
		  modecnt++;
		  if (p[0] == 'o')
		    {
		      if (sign == '+')
			ops++;
		      else
			deops++;
		    }
		  if (p[0] == 'b')
		    {
		      if (sign == '+')
			bans++;
		      else
			unbans++;
		    }
		}
	    }
	  else if (strchr (STR ("pstnmi"), p[0]))
	    {
	    }
	  else
	    {
	      putlog (LOG_ERRORS, "*", STR ("Forgotten modechar: %c"), p[0]);
	    }
	  p++;
	}
      ufrom = get_user_by_host (from);
      strncpy0 (work, from, sizeof (work));
      p = strchr (work, '!');
      *p++ = 0;
      nfrom = work;
      hfrom = p;
      if ((chan) && (deops >= 3))
	{
	  if ((!ufrom) || (!(ufrom->flags & USER_BOT)))
	    {
	      if (ROLE_KICK_MDOP)
		{
		  m = ismember (chan, nfrom);
		  if (!m || !chan_sentkick (m))
		    {
		      sprintf (tmp, STR ("KICK %s %s :%s%s\n"), chan->name,
			       nfrom, kickprefix, kickreason (KICK_MASSDEOP));
		      tputs (serv, tmp, strlen (tmp));
		      if (m)
			m->flags |= SENTKICK;
		    }
		}
	    }
	}
      if (chan && ops && (ufrom) && (ufrom->flags & USER_BOT)
	  && (!channel_fastop (chan)) && (!channel_take (chan)))
	{
	  int isbadop = 0;
	  if ((modecnt != 2) || (strncmp (modes[0], "+o", 2))
	      || (strncmp (modes[1], "-b", 2)))
	    isbadop = 1;
	  else
	    {
	      char enccookie[25], plaincookie[25], key[NICKLEN + 20],
		goodcookie[25];
	      strncpy0 (enccookie, (char *) &(modes[1][8]),
			sizeof (enccookie));
	      p = enccookie + strlen (enccookie) - 1;
	      strcpy (key, nfrom);
	      strcat (key, netpass);
	      p = decrypt_string (key, enccookie);
	      Context;
	      strncpy0 (plaincookie, p, sizeof (plaincookie));
	      nfree (p);
	      makeplaincookie (chan->dname, (char *) (modes[0] + 3),
			       goodcookie);
	      if (strncmp
		  ((char *) &plaincookie[6], (char *) &goodcookie[6], 5))
		isbadop = 2;
	      else
		if (strncmp
		    ((char *) &plaincookie[11], (char *) &goodcookie[11], 5))
		isbadop = 3;
	      else
		{
		  char tmp[20];
		  long optime;
		  int off;
		  sprintf (tmp, STR ("%010li"), (now + timesync));
		  strncpy0 ((char *) &tmp[4], plaincookie, 7);
		  optime = atol (tmp);
		  off = (now + timesync - optime);
		  if (abs (off) > op_time_slack && 0)
		    {
		      isbadop = 4;
		      putlog (LOG_ERRORS, "*",
			      "%s opped with bad ts: %li was off by %li",
			      nfrom, optime, off);
		    }
		}
	    }
	  if (isbadop)
	    {
	      char trg[NICKLEN + 1] = "";
	      int n, i;
	      memberlist *m;
	      Context;
	      switch (role)
		{
		case 0:
		  break;
		case 1:
		  m = ismember (chan, nfrom);
		  if (!m || !chan_sentkick (m))
		    {
		      sprintf (tmp, STR ("KICK %s %s :%s%s\n"), chan->name,
			       nfrom, kickprefix, kickreason (KICK_BADOP));
		      tputs (serv, tmp, strlen (tmp));
		      if (m)
			m->flags |= SENTKICK;
		    }
		  sprintf (tmp, STR ("%s MODE %s"), from, msg);
		  deflag_user (ufrom, DEFLAG_BADCOOKIE, tmp);
		  break;
		default:
		  n = role - 1;
		  i = 0;
		  while ((i < 5) && (n > 0))
		    {
		      if (modes[i] && !strncmp (modes[i], "+o", 2))
			n--;
		      if (n)
			i++;
		    }
		  if (!n)
		    {
		      strcpy (trg, (char *) &modes[i][3]);
		      m = ismember (chan, trg);
		      if (m)
			{
			  if (!(m->flags & CHANOP))
			    {
			      if (!chan_sentkick (m))
				{
				  sprintf (tmp, STR ("KICK %s %s :%s%s\n"),
					   chan->name, trg, kickprefix,
					   kickreason (KICK_BADOPPED));
				  tputs (serv, tmp, strlen (tmp));
				  m->flags |= SENTKICK;
				}
			    }
			}
		    }
		}
	      if (isbadop == 1)
		putlog (LOG_WARN, "*", STR ("Missing cookie: %s MODE %s"),
			from, msg);
	      else if (isbadop == 2)
		putlog (LOG_WARN, "*",
			STR ("Invalid cookie (bad nick): %s MODE %s"), from,
			msg);
	      else if (isbadop == 3)
		putlog (LOG_WARN, "*",
			STR ("Invalid cookie (bad chan): %s MODE %s"), from,
			msg);
	      else if (isbadop == 4)
		putlog (LOG_WARN, "*",
			STR ("Invalid cookie (bad time): %s MODE %s"), from,
			msg);
	    }
	  else
	    putlog (LOG_DEBUG, "*", STR ("Good op: %s"), msg);
	}
      if ((ops) && chan && !channel_manop (chan) && (ufrom)
	  && !(ufrom->flags & USER_BOT))
	{
	  char trg[NICKLEN + 1] = "";
	  int n, i;
	  memberlist *m;
	  switch (role)
	    {
	    case 0:
	      break;
	    case 1:
	      m = ismember (chan, nfrom);
	      if (!m || !chan_sentkick (m))
		{
		  sprintf (tmp, STR ("KICK %s %s :%s%s\n"), chan->name, nfrom,
			   kickprefix, kickreason (KICK_MANUALOP));
		  tputs (serv, tmp, strlen (tmp));
		  if (m)
		    m->flags |= SENTKICK;
		}
	      sprintf (tmp, STR ("%s MODE %s"), from, msg);
	      deflag_user (ufrom, DEFLAG_MANUALOP, tmp);
	      break;
	    default:
	      n = role - 1;
	      i = 0;
	      while ((i < 5) && (n > 0))
		{
		  if (modes[i] && !strncmp (modes[i], "+o", 2))
		    n--;
		  if (n)
		    i++;
		}
	      if (!n)
		{
		  strcpy (trg, (char *) &modes[i][3]);
		  m = ismember (chan, trg);
		  if (m)
		    {
		      if (!(m->flags & CHANOP)
			  && (rfc_casecmp (botname, trg)))
			{
			  if (!chan_sentkick (m))
			    {
			      sprintf (tmp, STR ("KICK %s %s :%s%s\n"),
				       chan->name, trg, kickprefix,
				       kickreason (KICK_MANUALOPPED));
			      tputs (serv, tmp, strlen (tmp));
			      m->flags |= SENTKICK;
			    }
			}
		    }
		  else
		    {
		      sprintf (tmp, STR ("KICK %s %s :%s%s\n"), chan->name,
			       trg, kickprefix,
			       kickreason (KICK_MANUALOPPED));
		      tputs (serv, tmp, strlen (tmp));
		    }
		}
	    }
	}
      for (i = 0; i < 5; i++)
	if (modes[i])
	  nfree (modes[i]);
    }
  if (use_console_r)
    {
      if (!strcmp (code, "PRIVMSG") || !strcmp (code, "NOTICE"))
	{
	  if (!match_ignore (from))
	    putlog (LOG_RAW, "@", "[@] %s %s %s", from, code, msg);
	}
      else
	putlog (LOG_RAW, "@", "[@] %s %s %s", from, code, msg);
    }
  check_tcl_raw (from, code, msg);
}
static int
gotping (char *from, char *msg)
{
  fixcolon (msg);
  dprintf (DP_MODE, "PONG :%s\n", msg);
  return 0;
}
static int
gotkick (char *from, char *msg)
{
  char *nick;
  nick = from;
  if (rfc_casecmp (nick, botname))
    return 0;
  if (use_penalties)
    {
      last_time += 2;
      if (debug_output)
	putlog (LOG_SRVOUT, "*", "adding 2secs penalty (successful kick)");
    }
  return 0;
}
static int
whoispenalty (char *from, char *msg)
{
  struct server_list *x = serverlist;
  int i, ii;
  if (x && use_penalties)
    {
      i = ii = 0;
      for (; x; x = x->next)
	{
	  if (i == curserv)
	    {
	      if ((strict_servernames == 1) || !x->realname)
		{
		  if (strcmp (x->name, from))
		    ii = 1;
		}
	      else
		{
		  if (strcmp (x->realname, from))
		    ii = 1;
		}
	    }
	  i++;
	}
      if (ii)
	{
	  last_time += 1;
	  if (debug_output)
	    putlog (LOG_SRVOUT, "*", "adding 1sec penalty (remote whois)");
	}
    }
  return 0;
}
static int
got311 (char *from, char *msg)
{
  char *n1, *n2, *u, *h;
  n1 = newsplit (&msg);
  n2 = newsplit (&msg);
  u = newsplit (&msg);
  h = newsplit (&msg);
  if (!n1 || !n2 || !u || !h)
    return 0;
  if (match_my_nick (n2))
    egg_snprintf (botuserhost, sizeof botuserhost, "%s@%s", u, h);
  return 0;
}
static cmd_t my_raw_binds[] =
  { {"PRIVMSG", "", (Function) gotmsg, NULL}, {"NOTICE", "",
					       (Function) gotnotice, NULL},
  {"MODE", "", (Function) gotmode, NULL}, {"PING", "", (Function) gotping,
					   NULL}, {"PONG", "",
						   (Function) gotpong, NULL},
  {"WALLOPS", "", (Function) gotwall, NULL}, {"001", "", (Function) got001,
					      NULL}, {"251", "",
						      (Function) got251,
						      NULL}, {"303", "",
							      (Function)
							      got303, NULL},
  {"432", "", (Function) got432, NULL}, {"433", "", (Function) got433, NULL},
  {"437", "", (Function) got437, NULL}, {"438", "", (Function) got438, NULL},
  {"451", "", (Function) got451, NULL}, {"442", "", (Function) got442, NULL},
  {"NICK", "", (Function) gotnick, NULL}, {"ERROR", "", (Function) goterror,
					   NULL}, {"ERROR:", "",
						   (Function) goterror, NULL},
  {"KICK", "", (Function) gotkick, NULL}, {"318", "", (Function) whoispenalty,
					   NULL}, {"311", "",
						   (Function) got311, NULL},
  {NULL, NULL, NULL, NULL} };
static void server_resolve_success (int);
static void server_resolve_failure (int);
static void
connect_server (void)
{
  char pass[121], botserver[UHOSTLEN];
  static int oldserv = -1;
  int servidx;
  unsigned int botserverport = 0;
  waiting_for_awake = 0;
  trying_server = now;
  empty_msgq ();
  if ((oldserv < 0) || (never_give_up))
    oldserv = curserv;
  if (newserverport)
    {
      curserv = (-1);
      strcpy (botserver, newserver);
      botserverport = newserverport;
      strcpy (pass, newserverpass);
      newserver[0] = 0;
      newserverport = 0;
      newserverpass[0] = 0;
      oldserv = (-1);
    }
  else
    pass[0] = 0;
  if (!cycle_time)
    {
      struct chanset_t *chan;
      struct server_list *x = serverlist;
      if (!x)
	{
	  return;
	}
      servidx = new_dcc (&DCC_DNSWAIT, sizeof (struct dns_info));
      if (servidx < 0)
	{
	  putlog (LOG_SERV, "*",
		  "NO MORE DCC CONNECTIONS -- Can't create server connection.");
	  return;
	}
      if (connectserver[0])
	do_tcl ("connect-server", connectserver);
      check_tcl_event ("connect-server");
      next_server (&curserv, botserver, &botserverport, pass);
      putlog (LOG_SERV, "*", "%s %s:%d", IRC_SERVERTRY, botserver,
	      botserverport);
      dcc[servidx].port = botserverport;
      strcpy (dcc[servidx].nick, "(server)");
      strncpyz (dcc[servidx].host, botserver, UHOSTLEN);
      botuserhost[0] = 0;
      nick_juped = 0;
      for (chan = chanset; chan; chan = chan->next)
	chan->status &= ~CHAN_JUPED;
      dcc[servidx].timeval = now;
      dcc[servidx].sock = -1;
      dcc[servidx].u.dns->host =
	get_data_ptr (strlen (dcc[servidx].host) + 1);
      strcpy (dcc[servidx].u.dns->host, dcc[servidx].host);
      dcc[servidx].u.dns->cbuf = get_data_ptr (strlen (pass) + 1);
      strcpy (dcc[servidx].u.dns->cbuf, pass);
      dcc[servidx].u.dns->dns_success = server_resolve_success;
      dcc[servidx].u.dns->dns_failure = server_resolve_failure;
      dcc[servidx].u.dns->dns_type = RES_IPBYHOST;
      dcc[servidx].u.dns->type = &SERVER_SOCKET;
      if (server_cycle_wait)
	cycle_time = server_cycle_wait;
      else
	cycle_time = 0;
      resolvserv = 1;
      server_resolve_success (servidx);
    }
}
static void
server_resolve_failure (int servidx)
{
  serv = -1;
  resolvserv = 0;
  putlog (LOG_SERV, "*", "%s %s (%s)", IRC_FAILEDCONNECT, dcc[servidx].host,
	  IRC_DNSFAILED);
  lostdcc (servidx);
} static void
server_resolve_success (int servidx)
{
  int oldserv = dcc[servidx].u.dns->ibuf, i = 0;
  char s[121], pass[121];
  resolvserv = 0;
  dcc[servidx].addr = dcc[servidx].u.dns->ip;
  strcpy (pass, dcc[servidx].u.dns->cbuf);
  changeover_dcc (servidx, &SERVER_SOCKET, 0);
  serv = open_telnet (dcc[servidx].host, dcc[servidx].port);
  if (serv < 0)
    {
      neterror (s);
      putlog (LOG_SERV, "*", "%s %s (%s)", IRC_FAILEDCONNECT,
	      dcc[servidx].host, s);
      lostdcc (servidx);
      if (oldserv == curserv && !never_give_up)
	fatal ("NO SERVERS WILL ACCEPT MY CONNECTION.", 0);
    }
  else
    {
      dcc[servidx].sock = serv;
      dcc[servidx].timeval = now;
      SERVER_SOCKET.timeout_val = &server_timeout;
      strcpy (botname, origbotname);
      altnick_char = 0;
      if (pass[0])
	dprintf (DP_MODE, "PASS %s\n", pass);
      dprintf (DP_MODE, "NICK %s\n", botname);
      dprintf (DP_MODE, "USER %s . . :%s\n", botuser, botrealname);
#ifdef S_NODELAY
      i = 1;
      setsockopt (serv, 6, TCP_NODELAY, &i, sizeof (i));
#endif
    }
}
#endif
