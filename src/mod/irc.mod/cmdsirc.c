#ifdef LEAF
static struct chanset_t *
get_channel (int idx, char *chname)
{
  struct chanset_t *chan;
  if (chname && chname[0])
    {
      chan = findchan_by_dname (chname);
      if (chan)
	return chan;
      else
	dprintf (idx, "No such channel.\n");
    }
  else
    {
      chname = dcc[idx].u.chat->con_chan;
      chan = findchan_by_dname (chname);
      if (chan)
	return chan;
      else
	dprintf (idx, "Invalid console channel.\n");
    }
  return 0;
}
static int
has_op (int idx, struct chanset_t *chan)
{
  get_user_flagrec (dcc[idx].user, &user, chan->dname);
  if (chan_op (user) || (glob_op (user) && !chan_deop (user)))
    return 1;
  dprintf (idx, "You are not a channel op on %s.\n", chan->dname);
  return 0;
}
static char *
getnick (char *handle, struct chanset_t *chan)
{
  char s[UHOSTLEN];
  struct userrec *u;
  register memberlist *m;
  for (m = chan->channel.member; m && m->nick[0]; m = m->next)
    {
      egg_snprintf (s, sizeof s, "%s!%s", m->nick, m->userhost);
      if ((u = get_user_by_host (s)) && !egg_strcasecmp (u->handle, handle))
	return m->nick;
    }
  return NULL;
}
static void
cmd_act (struct userrec *u, int idx, char *par)
{
  char *chname;
  struct chanset_t *chan;
  memberlist *m;
  if (!par[0])
    {
      dprintf (idx, "Usage: act [channel] <action>\n");
      return;
    }
  if (strchr (CHANMETA, par[0]) != NULL)
    chname = newsplit (&par);
  else
    chname = 0;
  chan = get_channel (idx, chname);
  if (!chan || !has_op (idx, chan))
    return;
  m = ismember (chan, botname);
  if (!m)
    {
      dprintf (idx, "Cannot say to %s: I'm not on that channel.\n",
	       chan->dname);
      return;
    }
  if ((chan->channel.mode & CHANMODER) && !me_op (chan) && !me_voice (chan))
    {
      dprintf (idx, "Cannot say to %s: It is moderated.\n", chan->dname);
      return;
    }
  putlog (LOG_CMDS, "*", "#%s# (%s) act %s", dcc[idx].nick, chan->dname, par);
  dprintf (DP_HELP, "PRIVMSG %s :\001ACTION %s\001\n", chan->name, par);
  dprintf (idx, "Action to %s: %s\n", chan->dname, par);
}
static void
cmd_msg (struct userrec *u, int idx, char *par)
{
  char *nick;
  nick = newsplit (&par);
  if (!par[0])
    dprintf (idx, "Usage: msg <nick> <message>\n");
  else
    {
      putlog (LOG_CMDS, "*", "#%s# msg %s %s", dcc[idx].nick, nick, par);
      dprintf (DP_HELP, "PRIVMSG %s :%s\n", nick, par);
      dprintf (idx, "Msg to %s: %s\n", nick, par);
    }
}
static void
cmd_say (struct userrec *u, int idx, char *par)
{
  char *chname;
  struct chanset_t *chan;
  memberlist *m;
  if (!par[0])
    {
      dprintf (idx, "Usage: say [channel] <message>\n");
      return;
    }
  if (strchr (CHANMETA, par[0]) != NULL)
    chname = newsplit (&par);
  else
    chname = 0;
  chan = get_channel (idx, chname);
  if (!chan || !has_op (idx, chan))
    return;
  m = ismember (chan, botname);
  if (!m)
    {
      dprintf (idx, "Cannot say to %s: I'm not on that channel.\n",
	       chan->dname);
      return;
    }
  if ((chan->channel.mode & CHANMODER) && !me_op (chan) && !me_voice (chan))
    {
      dprintf (idx, "Cannot say to %s: It is moderated.\n", chan->dname);
      return;
    }
  putlog (LOG_CMDS, "*", "#%s# (%s) say %s", dcc[idx].nick, chan->dname, par);
  dprintf (DP_HELP, "PRIVMSG %s :%s\n", chan->name, par);
  dprintf (idx, "Said to %s: %s\n", chan->dname, par);
}
static void
cmd_kickban (struct userrec *u, int idx, char *par)
{
  struct chanset_t *chan;
  char *chname, *nick, *s1;
  memberlist *m;
  char s[UHOSTLEN];
  char bantype = 0;
  if (!par[0])
    {
      dprintf (idx, "Usage: kickban [channel] [-|@]<nick> [reason]\n");
      return;
    }
  if (strchr (CHANMETA, par[0]) != NULL)
    chname = newsplit (&par);
  else
    chname = 0;
  chan = get_channel (idx, chname);
  if (!chan || !has_op (idx, chan))
    return;
  if (!channel_active (chan))
    {
      dprintf (idx, "I'm not on %s right now!\n", chan->dname);
      return;
    }
  if (!me_op (chan))
    {
      dprintf (idx,
	       "I can't help you now because I'm not a channel op"
	       " on %s.\n", chan->dname);
      return;
    }
  putlog (LOG_CMDS, "*", "#%s# (%s) kickban %s", dcc[idx].nick, chan->dname,
	  par);
  nick = newsplit (&par);
  if ((nick[0] == '@') || (nick[0] == '-'))
    {
      bantype = nick[0];
      nick++;
    }
  if (match_my_nick (nick))
    {
      dprintf (idx, "I'm not going to kickban myself.\n");
      return;
    }
  m = ismember (chan, nick);
  if (!m)
    {
      dprintf (idx, "%s is not on %s\n", nick, chan->dname);
      return;
    }
  egg_snprintf (s, sizeof s, "%s!%s", m->nick, m->userhost);
  u = get_user_by_host (s);
  get_user_flagrec (u, &victim, chan->dname);
  if ((chan_op (victim) || (glob_op (victim) && !chan_deop (victim)))
      && !(chan_master (user) || glob_master (user)))
    {
      dprintf (idx, "%s is a legal op.\n", nick);
      return;
    }
  if ((chan_master (victim) || glob_master (victim))
      && !(glob_owner (user) || chan_owner (user)))
    {
      dprintf (idx, "%s is a %s master.\n", nick, chan->dname);
      return;
    }
  if (glob_bot (victim) && !(glob_owner (user) || chan_owner (user)))
    {
      dprintf (idx, "%s is another channel bot!\n", nick);
      return;
    }
#ifdef S_IRCNET
  if (use_exempts
      && (u_match_mask (global_exempts, s)
	  || u_match_mask (chan->exempts, s)))
    {
      dprintf (idx, "%s is permanently exempted!\n", nick);
      return;
    }
#endif
  if (m->flags & CHANOP)
    add_mode (chan, '-', 'o', m->nick);
  check_exemptlist (chan, s);
  switch (bantype)
    {
    case '@':
      s1 = strchr (s, '@');
      s1 -= 3;
      s1[0] = '*';
      s1[1] = '!';
      s1[2] = '*';
      break;
    case '-':
      s1 = strchr (s, '!');
      s1[1] = '*';
      s1--;
      s1[0] = '*';
      break;
    default:
      s1 = quickban (chan, m->userhost);
      break;
    }
  if (bantype == '@' || bantype == '-')
    do_mask (chan, chan->channel.ban, s1, 'b');
  if (!par[0])
    par = "requested";
  dprintf (DP_SERVER, "KICK %s %s :%s%s\n", chan->name, m->nick,
	   bankickprefix, par);
  m->flags |= SENTKICK;
  u_addban (chan, s1, dcc[idx].nick, par, now + (60 * chan->ban_time), 0);
  dprintf (idx, "Okay, done.\n");
}
static void
cmd_voice (struct userrec *u, int idx, char *par)
{
  struct chanset_t *chan = NULL;
  char *nick;
  int all = 0;
  memberlist *m;
  char s[UHOSTLEN];
  nick = newsplit (&par);
  if (par[0] == '*' && !par[1])
    {
      all = 1;
      newsplit (&par);
    }
  else
    {
      chan = get_channel (idx, par);
      if (!chan || !has_op (idx, chan))
	return;
    }
  if (all)
    chan = chanset;
  putlog (LOG_CMDS, "*", "#%s# (%s) voice %s", dcc[idx].nick,
	  all ? "*" : chan->dname, nick);
  while (chan)
    {
      if (!nick[0] && !(nick = getnick (u->handle, chan)))
	{
	  if (all)
	    goto next;
	  dprintf (idx, "Usage: voice <nick> [channel]\n");
	  return;
	}
      if (!channel_active (chan))
	{
	  if (all)
	    goto next;
	  dprintf (idx, "I'm not on %s right now!\n", chan->dname);
	  return;
	}
      if (!me_op (chan))
	{
	  if (all)
	    goto next;
	  dprintf (idx,
		   "I can't help you now because I'm not a chan op on %s.\n",
		   chan->dname);
	  return;
	}
      m = ismember (chan, nick);
      if (!m)
	{
	  if (all)
	    goto next;
	  dprintf (idx, "%s is not on %s.\n", nick, chan->dname);
	  return;
	}
      egg_snprintf (s, sizeof s, "%s!%s", m->nick, m->userhost);
      add_mode (chan, '+', 'v', nick);
      dprintf (idx, "Gave voice to %s on %s\n", nick, chan->dname);
    next:;
      if (!all)
	chan = NULL;
      else
	chan = chan->next;
    }
}
static void
cmd_devoice (struct userrec *u, int idx, char *par)
{
  struct chanset_t *chan = NULL;
  char *nick;
  int all = 0;
  memberlist *m;
  char s[UHOSTLEN];
  nick = newsplit (&par);
  if (par[0] == '*' && !par[1])
    {
      all = 1;
      newsplit (&par);
    }
  else
    {
      chan = get_channel (idx, par);
      if (!chan || !has_op (idx, chan))
	return;
    }
  if (all)
    chan = chanset;
  putlog (LOG_CMDS, "*", "#%s# (%s) devoice %s", dcc[idx].nick,
	  all ? "*" : chan->dname, nick);
  while (chan)
    {
      if (!nick[0] && !(nick = getnick (u->handle, chan)))
	{
	  if (all)
	    goto next;
	  dprintf (idx, "Usage: devoice <nick> [channel]\n");
	  return;
	}
      if (!channel_active (chan))
	{
	  if (all)
	    goto next;
	  dprintf (idx, "I'm not on %s right now!\n", chan->dname);
	  return;
	}
      if (!me_op (chan))
	{
	  if (all)
	    goto next;
	  dprintf (idx, "I can't do that right now I'm not a chan op on %s",
		   chan->dname);
	  return;
	}
      m = ismember (chan, nick);
      if (!m)
	{
	  if (all)
	    goto next;
	  dprintf (idx, "%s is not on %s.\n", nick, chan->dname);
	  return;
	}
      egg_snprintf (s, sizeof s, "%s!%s", m->nick, m->userhost);
      add_mode (chan, '-', 'v', nick);
      dprintf (idx, "Devoiced %s on %s\n", nick, chan->dname);
    next:;
      if (!all)
	chan = NULL;
      else
	chan = chan->next;
    }
}
void
do_op (char *nick, struct chanset_t *chan)
{
  memberlist *m;
  m = ismember (chan, nick);
  if (!m)
    return;
  if (channel_fastop (chan))
    {
      add_mode (chan, '+', 'o', nick);
    }
  else
    {
      char *tmp = nmalloc (strlen (chan->name) + 200);
      makeopline (chan, nick, tmp);
      dprintf (DP_MODE, tmp);
      nfree (tmp);
}} static void
cmd_op (struct userrec *u, int idx, char *par)
{
  struct chanset_t *chan = NULL;
  char *nick;
  int all = 0;
  memberlist *m;
  char s[UHOSTLEN];
  nick = newsplit (&par);
  if (par[0] == '*' && !par[1])
    {
      all = 1;
      newsplit (&par);
    }
  else
    {
      chan = get_channel (idx, par);
      if (!chan || !has_op (idx, chan))
	return;
    }
  if (all)
    chan = chanset;
  putlog (LOG_CMDS, "*", "#%s# (%s) op %s", dcc[idx].nick,
	  all ? "*" : chan->dname, nick);
  while (chan)
    {
      if (!nick[0] && !(nick = getnick (u->handle, chan)))
	{
	  if (all)
	    goto next;
	  dprintf (idx, "Usage: op <nick> [channel]\n");
	  return;
	}
      if (!channel_active (chan))
	{
	  if (all)
	    goto next;
	  dprintf (idx, "I'm not on %s right now!\n", chan->dname);
	  return;
	}
      if (!me_op (chan))
	{
	  if (all)
	    goto next;
	  dprintf (idx,
		   "I can't help you now because I'm not a chan op on %s.\n",
		   chan->dname);
	  return;
	}
      m = ismember (chan, nick);
      if (!m)
	{
	  if (all)
	    goto next;
	  dprintf (idx, "%s is not on %s.\n", nick, chan->dname);
	  return;
	}
      egg_snprintf (s, sizeof s, "%s!%s", m->nick, m->userhost);
      u = get_user_by_host (s);
      get_user_flagrec (u, &victim, chan->dname);
      if (chan_deop (victim) || (glob_deop (victim) && !glob_op (victim)))
	{
	  dprintf (idx, "%s is currently being auto-deopped  on %s.\n",
		   m->nick, chan->dname);
	  if (all)
	    goto next;
	  return;
	}
      if (channel_bitch (chan)
	  && !(chan_op (victim) || (glob_op (victim) && !chan_deop (victim))))
	{
	  dprintf (idx, "%s is not a registered op on %s.\n", m->nick,
		   chan->dname);
	  if (all)
	    goto next;
	  return;
	}
      dprintf (idx, "Gave op to %s on %s.\n", nick, chan->dname);
      stats_add (u, 0, 1);
      do_op (nick, chan);
    next:;
      if (!all)
	chan = NULL;
      else
	chan = chan->next;
    }
}
void
cmd_mdop (struct userrec *u, int idx, char *par)
{
  char *p;
  int force_bots = 0, force_alines = 0, force_slines = 0, force_overlap = 0;
  int overlap = 0, bitch = 0, simul = 0;
  int needed_deops, max_deops, bots, deops, sdeops;
  memberlist **chanbots = NULL, **targets = NULL, *m;
  int chanbotcount = 0, targetcount = 0, tpos = 0, bpos = 0, i;
  struct chanset_t *chan;
  char work[1024];
  putlog (LOG_CMDS, "*", "#%s# mdop %s", dcc[idx].nick, par);
  if (!par[0] || (par[0] != '#'))
    {
      dprintf (idx,
	       "Usage: mdop #channel [bots=n] [alines=n] [slines=n] [overlap=n] [bitch] [simul]\n");
      dprintf (idx, "  bots    : Number of bots to use for mdop.\n");
      dprintf (idx,
	       "  alines  : Number of MODE lines to assume each participating bot will get through.\n");
      dprintf (idx,
	       "  slines  : Number of MODE lines each participating bot will send.\n");
      dprintf (idx,
	       "  overlap : Number of times to deop each target nick (using alines for calc).\n");
      dprintf (idx, "  bitch   : Set the channel +bitch after mdop.\n");
      dprintf (idx,
	       "  simul   : Simulate the mdop. Who would do what will be shown in DCC\n");
      dprintf (idx,
	       "bots, alines, slines and overlap are dependant on each other, set them wrong and\n");
      dprintf (idx,
	       "the bot will complain. Defaults are alines=3, slines=5, overlap=2. alines will be\n");
      dprintf (idx,
	       "increased up to 5 if there are not enough bots available.\n");
      dprintf (idx,
	       "The bot you mdop on will never participate in the deopping\n");
      return;
    }
  p = newsplit (&par);
  chan = findchan (p);
  if (!chan)
    {
      dprintf (idx, "%s isn't in my chanlist\n", p);
      return;
    }
  m = ismember (chan, botname);
  if (!m)
    {
      dprintf (idx, "I'm not on %s\n", chan->name);
      return;
    }
  if (!(m->flags & CHANOP))
    {
      dprintf (idx, "I'm not opped on %s\n", chan->name);
      return;
    }
  targets = nmalloc (chan->channel.members * sizeof (memberlist *));
  bzero (targets, chan->channel.members * sizeof (memberlist *));
  chanbots = nmalloc (chan->channel.members * sizeof (memberlist *));
  bzero (chanbots, chan->channel.members * sizeof (memberlist *));
  for (m = chan->channel.member; m; m = m->next)
    if (m->flags & CHANOP)
      {
	if (!m->user)
	  targets[targetcount++] = m;
	else
	  if (((m->user->flags & (USER_BOT | USER_OP)) ==
	       (USER_BOT | USER_OP)) && (strcmp (botnetnick, m->user->handle))
	      && (nextbot (m->user->handle) >= 0))
	  chanbots[chanbotcount++] = m;
	else if (!(m->user->flags & USER_OP))
	  targets[targetcount++] = m;
      }
  if (!chanbotcount)
    {
      dprintf (idx, "No bots opped on %s\n", chan->name);
      nfree (targets);
      nfree (chanbots);
      return;
    }
  if (!targetcount)
    {
      dprintf (idx, "Noone to deop on %s\n", chan->name);
      nfree (targets);
      nfree (chanbots);
      return;
    }
  while (par && par[0])
    {
      p = newsplit (&par);
      if (!strncmp (p, "bots=", 5))
	{
	  p += 5;
	  force_bots = atoi (p);
	  if ((force_bots < 1) || (force_bots > chanbotcount))
	    {
	      dprintf (idx, "bots must be within 1-%i\n", chanbotcount);
	      nfree (targets);
	      nfree (chanbots);
	      return;
	    }
	}
      else if (!strncmp (p, "alines=", 7))
	{
	  p += 7;
	  force_alines = atoi (p);
	  if ((force_alines < 1) || (force_alines > 5))
	    {
	      dprintf (idx, "alines must be within 1-5\n");
	      nfree (targets);
	      nfree (chanbots);
	      return;
	    }
	}
      else if (!strncmp (p, "slines=", 7))
	{
	  p += 7;
	  force_slines = atoi (p);
	  if ((force_slines < 1) || (force_slines > 6))
	    {
	      dprintf (idx, "slines must be within 1-6\n");
	      nfree (targets);
	      nfree (chanbots);
	      return;
	    }
	}
      else if (!strncmp (p, "overlap=", 8))
	{
	  p += 8;
	  force_overlap = atoi (p);
	  if ((force_overlap < 1) || (force_overlap > 8))
	    {
	      dprintf (idx, "overlap must be within 1-8\n");
	      nfree (targets);
	      nfree (chanbots);
	      return;
	    }
	}
      else if (!strcmp (p, "bitch"))
	{
	  bitch = 1;
	}
      else if (!strcmp (p, "simul"))
	{
	  simul = 1;
	}
      else
	{
	  dprintf (idx, "Unrecognized mdop option %s\n", p);
	  nfree (targets);
	  nfree (chanbots);
	  return;
	}
    }
  overlap = (force_overlap ? force_overlap : 2);
  needed_deops = (overlap * targetcount);
  max_deops =
    ((force_bots ? force_bots : chanbotcount) *
     (force_alines ? force_alines : 5) * 4);
  if (needed_deops > max_deops)
    {
      if (overlap == 1)
	dprintf (idx, "Not enough bots.\n");
      else
	dprintf (idx, "Not enough bots. Try with overlap=1\n");
      nfree (targets);
      nfree (chanbots);
      return;
    }
  if (force_bots && force_alines)
    {
      bots = force_bots;
      deops = force_alines * 4;
    }
  else
    {
      if (force_bots)
	{
	  bots = force_bots;
	  deops = (needed_deops + (bots - 1)) / bots;
	}
      else if (force_alines)
	{
	  deops = force_alines * 4;
	  bots = (needed_deops + (deops - 1)) / deops;
	}
      else
	{
	  deops = 12;
	  bots = (needed_deops + (deops - 1)) / deops;
	  if (bots > chanbotcount)
	    {
	      deops = 16;
	      bots = (needed_deops + (deops - 1)) / deops;
	    }
	  if (bots > chanbotcount)
	    {
	      deops = 20;
	      bots = (needed_deops + (deops - 1)) / deops;
	    }
	  if (bots > chanbotcount)
	    {
	      putlog (LOG_MISC, "*",
		      "Totally fucked calculations in cmd_mdop. this CAN'T happen.");
	      dprintf (idx, "Something's wrong... bug the coder\n");
	      nfree (targets);
	      nfree (chanbots);
	      return;
	    }
	}
    }
  if (force_slines)
    sdeops = force_slines * 4;
  else
    sdeops = 20;
  if (sdeops < deops)
    sdeops = deops;
  dprintf (idx, "Mass deop of %s\n", chan->name);
  dprintf (idx, "  %d bots used for deop\n", bots);
  dprintf (idx, "  %d assumed deops per participating bot\n", deops);
  dprintf (idx, "  %d max deops per participating bot\n", sdeops);
  dprintf (idx, "  %d assumed deops per target nick\n", overlap);
  while (bots)
    {
      if (!simul)
	sprintf (work, "dp %s", chan->name);
      else
	work[0] = 0;
      for (i = 0; i < deops; i++)
	{
	  strcat (work, " ");
	  strcat (work, targets[tpos]->nick);
	  tpos++;
	  if (tpos >= targetcount)
	    tpos = 0;
	}
      if (sdeops > deops)
	{
	  int atpos;
	  atpos = tpos;
	  for (i = 0; i < (sdeops - deops); i++)
	    {
	      strcat (work, " ");
	      strcat (work, targets[atpos]->nick);
	      atpos++;
	      if (atpos >= targetcount)
		atpos = 0;
	    }
	}
      if (simul)
	dprintf (idx, "%s deops%s\n", chanbots[bpos]->nick, work);
      else
	botnet_send_zapf (nextbot (chanbots[bpos]->user->handle), botnetnick,
			  chanbots[bpos]->user->handle, work);
      bots--;
      bpos++;
    }
  if (bitch)
    {
      char buf2[1024];
      chan->status |= CHAN_BITCH;
      sprintf (buf2, "cset %s +bitch", chan->dname);
      botnet_send_zapf_broad (-1, botnetnick, NULL, buf2);
    }
  nfree (targets);
  nfree (chanbots);
  return;
}

void
mdop_request (char *botnick, char *code, char *par)
{
  char *chname, *p;
  char work[2048];
  chname = newsplit (&par);
  work[0] = 0;
  while (par[0])
    {
      int cnt = 0;
      strcat (work, "MODE ");
      strcat (work, chname);
      strcat (work, " -oooo");
      while ((cnt < 4) && par[0])
	{
	  p = newsplit (&par);
	  strcat (work, " ");
	  strcat (work, p);
	  cnt++;
	}
      strcat (work, "\n");
    }
  tputs (servi, work, strlen (work));
}
static void
cmd_deop (struct userrec *u, int idx, char *par)
{
  struct chanset_t *chan = NULL;
  char *nick;
  int all = 0;
  memberlist *m;
  char s[UHOSTLEN];
  nick = newsplit (&par);
  if (par[0] == '*' && !par[1])
    {
      all = 1;
      newsplit (&par);
    }
  else
    {
      chan = get_channel (idx, par);
      if (!chan || !has_op (idx, chan))
	return;
    }
  if (all)
    chan = chanset;
  putlog (LOG_CMDS, "*", "#%s# (%s) deop %s", dcc[idx].nick,
	  all ? "*" : chan->dname, nick);
  while (chan)
    {
      if (!nick[0] && !(nick = getnick (u->handle, chan)))
	{
	  if (all)
	    goto next;
	  dprintf (idx, "Usage: deop <nick> [channel]\n");
	  return;
	}
      if (!channel_active (chan))
	{
	  if (all)
	    goto next;
	  dprintf (idx, "I'm not on %s right now!\n", chan->dname);
	  return;
	}
      if (!me_op (chan))
	{
	  if (all)
	    goto next;
	  dprintf (idx,
		   "I can't help you now because I'm not a chan op on %s.\n",
		   chan->dname);
	  return;
	}
      m = ismember (chan, nick);
      if (!m)
	{
	  if (all)
	    goto next;
	  dprintf (idx, "%s is not on %s.\n", nick, chan->dname);
	  return;
	}
      if (match_my_nick (nick))
	{
	  if (all)
	    goto next;
	  dprintf (idx, "I'm not going to deop myself.\n");
	  return;
	}
      egg_snprintf (s, sizeof s, "%s!%s", m->nick, m->userhost);
      u = get_user_by_host (s);
      get_user_flagrec (u, &victim, chan->dname);
      if ((chan_master (victim) || glob_master (victim))
	  && !(chan_owner (user) || glob_owner (user)))
	{
	  dprintf (idx, "%s is a master for %s.\n", m->nick, chan->dname);
	  if (all)
	    goto next;
	  return;
	}
      if ((chan_op (victim) || (glob_op (victim) && !chan_deop (victim)))
	  && !(chan_master (user) || glob_master (user)))
	{
	  dprintf (idx, "%s has the op flag for %s.\n", m->nick, chan->dname);
	  if (all)
	    goto next;
	  return;
	}
      add_mode (chan, '-', 'o', nick);
      dprintf (idx, "Took op from %s on %s.\n", nick, chan->dname);
    next:;
      if (!all)
	chan = NULL;
      else
	chan = chan->next;
    }
}
static void
cmd_kick (struct userrec *u, int idx, char *par)
{
  struct chanset_t *chan;
  char *chname, *nick;
  memberlist *m;
  char s[UHOSTLEN];
  if (!par[0])
    {
      dprintf (idx, "Usage: kick [channel] <nick> [reason]\n");
      return;
    }
  if (strchr (CHANMETA, par[0]) != NULL)
    chname = newsplit (&par);
  else
    chname = 0;
  chan = get_channel (idx, chname);
  if (!chan || !has_op (idx, chan))
    return;
  if (!channel_active (chan))
    {
      dprintf (idx, "I'm not on %s right now!\n", chan->dname);
      return;
    }
  if (!me_op (chan))
    {
      dprintf (idx, "I can't help you now because I'm not a channel op %s",
	       "on %s.\n", chan->dname);
      return;
    }
  putlog (LOG_CMDS, "*", "#%s# (%s) kick %s", dcc[idx].nick, chan->dname,
	  par);
  nick = newsplit (&par);
  if (!par[0])
    par = "request";
  if (match_my_nick (nick))
    {
      dprintf (idx, "I'm not going to kick myself.\n");
      return;
    }
  m = ismember (chan, nick);
  if (!m)
    {
      dprintf (idx, "%s is not on %s\n", nick, chan->dname);
      return;
    }
  egg_snprintf (s, sizeof s, "%s!%s", m->nick, m->userhost);
  u = get_user_by_host (s);
  get_user_flagrec (u, &victim, chan->dname);
  if ((chan_op (victim) || (glob_op (victim) && !chan_deop (victim)))
      && !(chan_master (user) || glob_master (user)))
    {
      dprintf (idx, "%s is a legal op.\n", nick);
      return;
    }
  if ((chan_master (victim) || glob_master (victim))
      && !(glob_owner (user) || chan_owner (user)))
    {
      dprintf (idx, "%s is a %s master.\n", nick, chan->dname);
      return;
    }
  if (glob_bot (victim) && !(glob_owner (user) || chan_owner (user)))
    {
      dprintf (idx, "%s is another channel bot!\n", nick);
      return;
    }
  dprintf (DP_SERVER, "KICK %s %s :%s%s\n", chan->name, m->nick, kickprefix,
	   par);
  m->flags |= SENTKICK;
  dprintf (idx, "Okay, done.\n");
}
static void
cmd_invite (struct userrec *u, int idx, char *par)
{
  struct chanset_t *chan = NULL;
  memberlist *m;
  int all = 0;
  char *nick;
  if (!par[0])
    par = dcc[idx].nick;
  nick = newsplit (&par);
  if (par[0] == '*' && !par[1])
    {
      all = 1;
      newsplit (&par);
    }
  else
    {
      chan = get_channel (idx, par);
      if (!chan || !has_op (idx, chan))
	return;
    }
  if (all)
    chan = chanset;
  putlog (LOG_CMDS, "*", "#%s# (%s) invite %s", dcc[idx].nick,
	  all ? "*" : chan->dname, nick);
  while (chan)
    {
      if (!me_op (chan))
	{
	  if (all)
	    goto next;
	  if (chan->channel.mode & CHANINV)
	    {
	      dprintf (idx,
		       "I can't help you now because I'm not a channel op on %s",
		       chan->dname);
	      return;
	    }
	  if (!channel_active (chan))
	    {
	      dprintf (idx, "I'm not on %s right now!\n", chan->dname);
	      return;
	    }
	}
      m = ismember (chan, nick);
      if (m && !chan_issplit (m))
	{
	  if (all)
	    goto next;
	  dprintf (idx, "%s is already on %s!\n", nick, chan->dname);
	  return;
	}
      dprintf (DP_SERVER, "INVITE %s %s\n", nick, chan->name);
      dprintf (idx, "Inviting %s to %s.\n", nick, chan->dname);
    next:;
      if (!all)
	chan = NULL;
      else
	chan = chan->next;
    }
}
static void
cmd_channel (struct userrec *u, int idx, char *par)
{
  char handle[HANDLEN + 1], s[UHOSTLEN], s1[UHOSTLEN], atrflag, chanflag[2];
  struct chanset_t *chan;
  memberlist *m;
  int maxnicklen, maxhandlen;
  char format[81];
  chan = get_channel (idx, par);
  if (!chan || !has_op (idx, chan))
    return;
  putlog (LOG_CMDS, "*", "#%s# (%s) channel", dcc[idx].nick, chan->dname);
  strncpyz (s, getchanmode (chan), sizeof s);
  if (channel_pending (chan))
    egg_snprintf (s1, sizeof s1, "%s %s", IRC_PROCESSINGCHAN, chan->dname);
  else if (channel_active (chan))
    egg_snprintf (s1, sizeof s1, "%s %s", IRC_CHANNEL, chan->dname);
  else
    egg_snprintf (s1, sizeof s1, "%s %s", IRC_DESIRINGCHAN, chan->dname);
  dprintf (idx, "%s, %d member%s, mode %s:\n", s1, chan->channel.members,
	   chan->channel.members == 1 ? "" : "s", s);
  if (chan->channel.topic)
    dprintf (idx, "%s: %s\n", IRC_CHANNELTOPIC, chan->channel.topic);
  if (channel_active (chan))
    {
      maxnicklen = maxhandlen = 0;
      for (m = chan->channel.member; m && m->nick[0]; m = m->next)
	{
	  if (strlen (m->nick) > maxnicklen)
	    maxnicklen = strlen (m->nick);
	  if (m->user)
	    if (strlen (m->user->handle) > maxhandlen)
	      maxhandlen = strlen (m->user->handle);
	}
      if (maxnicklen < 9)
	maxnicklen = 9;
      if (maxhandlen < 9)
	maxhandlen = 9;
      dprintf (idx, "(n = owner, m = master, o = op, d = deop, b = bot)\n");
      egg_snprintf (format, sizeof format, " %%-%us %%-%us %%-6s %%-5s %%s\n",
		    maxnicklen, maxhandlen);
      dprintf (idx, format, "NICKNAME", "HANDLE", " JOIN", "IDLE",
	       "USER@HOST");
      for (m = chan->channel.member; m && m->nick[0]; m = m->next)
	{
	  if (m->joined > 0)
	    {
	      if ((now - (m->joined)) > 86400)
		egg_strftime (s, 6, "%d%b", localtime (&(m->joined)));
	      else
		egg_strftime (s, 6, "%H:%M", localtime (&(m->joined)));
	    }
	  else
	    strncpyz (s, " --- ", sizeof s);
	  if (m->user == NULL)
	    {
	      egg_snprintf (s1, sizeof s1, "%s!%s", m->nick, m->userhost);
	      m->user = get_user_by_host (s1);
	    }
	  if (m->user == NULL)
	    strncpyz (handle, "*", sizeof handle);
	  else
	    strncpyz (handle, m->user->handle, sizeof handle);
	  get_user_flagrec (m->user, &user, chan->dname);
	  if (glob_bot (user) && (glob_op (user) || chan_op (user)))
	    atrflag = 'B';
	  else if (glob_bot (user))
	    atrflag = 'b';
	  else if (glob_owner (user))
	    atrflag = 'N';
	  else if (chan_owner (user))
	    atrflag = 'n';
	  else if (glob_master (user))
	    atrflag = 'M';
	  else if (chan_master (user))
	    atrflag = 'm';
	  else if (glob_deop (user))
	    atrflag = 'D';
	  else if (chan_deop (user))
	    atrflag = 'd';
	  else if (glob_op (user))
	    atrflag = 'O';
	  else if (chan_op (user))
	    atrflag = 'o';
	  else if (glob_quiet (user))
	    atrflag = 'Q';
	  else if (chan_quiet (user))
	    atrflag = 'q';
	  else if (glob_voice (user))
	    atrflag = 'V';
	  else if (chan_voice (user))
	    atrflag = 'v';
	  else if (glob_friend (user))
	    atrflag = 'F';
	  else if (chan_friend (user))
	    atrflag = 'f';
	  else if (glob_kick (user))
	    atrflag = 'K';
	  else if (chan_kick (user))
	    atrflag = 'k';
	  else if (glob_wasoptest (user))
	    atrflag = 'W';
	  else if (chan_wasoptest (user))
	    atrflag = 'w';
	  else if (glob_exempt (user))
	    atrflag = 'E';
	  else if (chan_exempt (user))
	    atrflag = 'e';
	  else
	    atrflag = ' ';
	  if (chan_hasop (m))
	    chanflag[1] = '@';
	  else if (chan_hasvoice (m))
	    chanflag[1] = '+';
	  else
	    chanflag[1] = ' ';
	  if (m->flags & OPER)
	    chanflag[0] = 'O';
	  else
	    chanflag[0] = ' ';
	  if (chan_issplit (m))
	    {
	      egg_snprintf (format, sizeof format,
			    "%%c%%-%us %%-%us %%s %%c     <- netsplit, %%lus\n",
			    maxnicklen, maxhandlen);
	      dprintf (idx, format, chanflag, m->nick, handle, s, atrflag,
		       now - (m->split));
	    }
	  else if (!rfc_casecmp (m->nick, botname))
	    {
	      egg_snprintf (format, sizeof format,
			    "%%c%%-%us %%-%us %%s %%c     <- it's me!\n",
			    maxnicklen, maxhandlen);
	      dprintf (idx, format, chanflag, m->nick, handle, s, atrflag);
	    }
	  else
	    {
	      if (now - (m->last) > 86400)
		egg_snprintf (s1, sizeof s1, "%2lud",
			      ((now - (m->last)) / 86400));
	      else if (now - (m->last) > 3600)
		egg_snprintf (s1, sizeof s1, "%2luh",
			      ((now - (m->last)) / 3600));
	      else if (now - (m->last) > 180)
		egg_snprintf (s1, sizeof s1, "%2lum",
			      ((now - (m->last)) / 60));
	      else
		strncpyz (s1, "   ", sizeof s1);
	      egg_snprintf (format, sizeof format,
			    "%%c%%-%us %%-%us %%s %%c %%s  %%s\n", maxnicklen,
			    maxhandlen);
	      dprintf (idx, format, chanflag, m->nick, handle, s, atrflag, s1,
		       m->userhost);
	    }
	  if (chan_fakeop (m))
	    dprintf (idx, "    (%s)\n", IRC_FAKECHANOP);
	  if (chan_sentop (m))
	    dprintf (idx, "    (%s)\n", IRC_PENDINGOP);
	  if (chan_sentdeop (m))
	    dprintf (idx, "    (%s)\n", IRC_PENDINGDEOP);
	  if (chan_sentkick (m))
	    dprintf (idx, "    (%s)\n", IRC_PENDINGKICK);
	}
    }
  dprintf (idx, "%s\n", IRC_ENDCHANINFO);
}
static void
cmd_topic (struct userrec *u, int idx, char *par)
{
  struct chanset_t *chan;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
  if (par[0] && (strchr (CHANMETA, par[0]) != NULL))
    {
      char *chname = newsplit (&par);
      chan = get_channel (idx, chname);
    }
  else
    chan = get_channel (idx, "");
  if (!chan || !has_op (idx, chan))
    return;
  if (!channel_active (chan))
    {
      dprintf (idx, "I'm not on %s right now!\n", chan->dname);
      return;
    }
  if (!par[0])
    {
      if (chan->channel.topic)
	{
	  dprintf (idx, "The topic for %s is: %s\n", chan->dname,
		   chan->channel.topic);
	}
      else
	{
	  dprintf (idx, "No topic is set for %s\n", chan->dname);
	}
    }
  else if (channel_optopic (chan) && !me_op (chan))
    {
      dprintf (idx, "I'm not a channel op on %s and the channel %s",
	       "is +t.\n", chan->dname);
    }
  else
    {
      if (chan->topic_prot[0])
	{
	  get_user_flagrec (u, &fr, chan->dname);
	  if (!glob_master (fr) && !chan_master (fr))
	    {
	      dprintf (idx, "The topic of %s is protected.\n", chan->dname);
	      return;
	    }
	}
      dprintf (DP_SERVER, "TOPIC %s :%s\n", chan->name, par);
      dprintf (idx, "Changing topic...\n");
      putlog (LOG_CMDS, "*", "#%s# (%s) topic %s", dcc[idx].nick, chan->dname,
	      par);
    }
}
static void
cmd_resetbans (struct userrec *u, int idx, char *par)
{
  struct chanset_t *chan;
  char *chname = newsplit (&par);
  chan = get_channel (idx, chname);
  if (!chan || !has_op (idx, chan))
    return;
  putlog (LOG_CMDS, "*", "#%s# (%s) resetbans", dcc[idx].nick, chan->dname);
  dprintf (idx, "Resetting bans on %s...\n", chan->dname);
  resetbans (chan);
}
static void
cmd_resetexempts (struct userrec *u, int idx, char *par)
{
  struct chanset_t *chan;
  char *chname = newsplit (&par);
  chan = get_channel (idx, chname);
  if (!chan || !has_op (idx, chan))
    return;
  putlog (LOG_CMDS, "*", "#%s# (%s) resetexempts", dcc[idx].nick,
	  chan->dname);
  dprintf (idx, "Resetting exempts on %s...\n", chan->dname);
  resetexempts (chan);
}
static void
cmd_resetinvites (struct userrec *u, int idx, char *par)
{
  struct chanset_t *chan;
  char *chname = newsplit (&par);
  chan = get_channel (idx, chname);
  if (!chan || !has_op (idx, chan))
    return;
  putlog (LOG_CMDS, "*", "#%s# (%s) resetinvites", dcc[idx].nick,
	  chan->dname);
  dprintf (idx, "Resetting resetinvites on %s...\n", chan->dname);
  resetinvites (chan);
}
static void
cmd_adduser (struct userrec *u, int idx, char *par)
{
  char *nick, *hand;
  struct chanset_t *chan;
  memberlist *m = NULL;
  char s[UHOSTLEN], s1[UHOSTLEN], s3[20];
  char tmp[50], s2[30];
  int atr = u ? u->flags : 0;
  int statichost = 0;
  char *p1 = s1;
  putlog (LOG_CMDS, "*", "#%s# adduser %s", dcc[idx].nick, par);
  if ((!par[0]) || ((par[0] == '!') && (!par[1])))
    {
      dprintf (idx, "Usage: adduser <nick> [handle]\n");
      return;
    }
  nick = newsplit (&par);
  if (nick[0] == '!')
    {
      statichost = 1;
      nick++;
    }
  if (!par[0])
    {
      hand = nick;
    }
  else
    {
      char *p;
      int ok = 1;
      for (p = par; *p; p++)
	if ((*p <= 32) || (*p >= 127))
	  ok = 0;
      if (!ok)
	{
	  dprintf (idx, "You can't have strange characters in a nick.\n");
	  return;
	}
      else if (strchr ("-,+*=:!.@#;$", par[0]) != NULL)
	{
	  dprintf (idx, "You can't start a nick with '%c'.\n", par[0]);
	  return;
	}
      hand = par;
    }
  for (chan = chanset; chan; chan = chan->next)
    {
      m = ismember (chan, nick);
      if (m)
	break;
    }
  if (!m)
    {
      dprintf (idx, "%s is not on any channels I monitor\n", nick);
      return;
    }
  if (strlen (hand) > HANDLEN)
    hand[HANDLEN] = 0;
  egg_snprintf (s, sizeof s, "%s!%s", m->nick, m->userhost);
  u = get_user_by_host (s);
  if (u)
    {
      dprintf (idx, "%s is already known as %s.\n", nick, u->handle);
      return;
    }
  u = get_user_by_handle (userlist, hand);
  if (u && (u->flags & (USER_OWNER | USER_MASTER)) && !(atr & USER_OWNER)
      && egg_strcasecmp (dcc[idx].nick, hand))
    {
      dprintf (idx, "You can't add hostmasks to the bot owner/master.\n");
      return;
    }
  if (!statichost)
    maskhost (s, s1);
  else
    {
      strncpyz (s1, s, sizeof s1);
      p1 = strchr (s1, '!');
      if (strchr ("~^+=-", p1[1]))
	{
	  if (strict_host)
	    p1[1] = '?';
	  else
	    {
	      p1[1] = '!';
	      p1++;
	    }
	}
      p1--;
      p1[0] = '*';
    }
  if (!u)
    {
      Context;
      userlist = adduser (userlist, hand, p1, "-", USER_DEFAULT);
      u = get_user_by_handle (userlist, hand);
      sprintf (tmp, STR ("%lu %s"), time (NULL), dcc[idx].nick);
      set_user (&USERENTRY_ADDED, u, tmp);
      make_rand_str (s2, 10);
      set_user (&USERENTRY_PASS, u, s2);
      make_rand_str (s3, 10);
      set_user (&USERENTRY_PASS, u, s3);
      dprintf (idx, "Added [%s]%s with no flags.\n", hand, p1);
      dprintf (idx, STR ("%s's password set to \002%s\002.\n"), hand, s2);
      dprintf (idx, STR ("%s's secpass set to \002%s\002.\n"), hand, s3);
    }
  else
    {
      dprintf (idx, "Added hostmask %s to %s.\n", p1, u->handle);
      addhost_by_handle (hand, p1);
      get_user_flagrec (u, &user, chan->dname);
      check_this_user (hand, 0, NULL);
    }
}
static void
cmd_deluser (struct userrec *u, int idx, char *par)
{
  char *nick, s[UHOSTLEN];
  struct chanset_t *chan;
  memberlist *m = NULL;
  struct flag_record victim =
    { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0, 0 };
  if (!par[0])
    {
      dprintf (idx, "Usage: deluser <nick>\n");
      return;
    }
  nick = newsplit (&par);
  for (chan = chanset; chan; chan = chan->next)
    {
      m = ismember (chan, nick);
      if (m)
	break;
    }
  if (!m)
    {
      dprintf (idx, "%s is not on any channels I monitor\n", nick);
      return;
    }
  get_user_flagrec (u, &user, chan->dname);
  egg_snprintf (s, sizeof s, "%s!%s", m->nick, m->userhost);
  u = get_user_by_host (s);
  if (!u)
    {
      dprintf (idx, "%s is not a valid user.\n", nick);
      return;
    }
  get_user_flagrec (u, &victim, NULL);
  if (isowner (u->handle))
    {
      dprintf (idx, "You can't remove a permanent bot owner!\n");
    }
  else if (glob_admin (victim) && !isowner (dcc[idx].nick))
    {
      dprintf (idx, "You can't remove an admin!\n");
    }
  else if (glob_owner (victim))
    {
      dprintf (idx, "You can't remove a bot owner!\n");
    }
  else if (chan_owner (victim) && !glob_owner (user))
    {
      dprintf (idx, "You can't remove a channel owner!\n");
    }
  else if (chan_master (victim) && !(glob_owner (user) || chan_owner (user)))
    {
      dprintf (idx, "You can't remove a channel master!\n");
    }
  else if (glob_bot (victim) && !glob_owner (user))
    {
      dprintf (idx, "You can't remove a bot!\n");
    }
  else
    {
      char buf[HANDLEN + 1];
      strncpyz (buf, u->handle, sizeof buf);
      buf[HANDLEN] = 0;
      if (deluser (u->handle))
	{
	  dprintf (idx, "Deleted %s.\n", buf);
	  putlog (LOG_CMDS, "*", "#%s# deluser %s [%s]", dcc[idx].nick, nick,
		  buf);
	}
      else
	{
	  dprintf (idx, "Failed.\n");
	}
    }
}
static void
cmd_reset (struct userrec *u, int idx, char *par)
{
  struct chanset_t *chan;
  if (par[0])
    {
      chan = findchan_by_dname (par);
      if (!chan)
	dprintf (idx, "%s\n", IRC_NOMONITOR);
      else
	{
	  get_user_flagrec (u, &user, par);
	  if (!glob_master (user) && !chan_master (user))
	    {
	      dprintf (idx, "You are not a master on %s.\n", chan->dname);
	    }
	  else if (!channel_active (chan))
	    {
	      dprintf (idx, "I'm not on %s at the moment!\n", chan->dname);
	    }
	  else
	    {
	      putlog (LOG_CMDS, "*", "#%s# reset %s", dcc[idx].nick, par);
	      dprintf (idx, "Resetting channel info for %s...\n",
		       chan->dname);
	      reset_chan_info (chan);
	    }
	}
    }
  else if (!(u->flags & USER_MASTER))
    {
      dprintf (idx, "You are not a Bot Master.\n");
    }
  else
    {
      putlog (LOG_CMDS, "*", "#%s# reset all", dcc[idx].nick);
      dprintf (idx, "Resetting channel info for all channels...\n");
      for (chan = chanset; chan; chan = chan->next)
	{
	  if (channel_active (chan))
	    reset_chan_info (chan);
	}
    }
}
static dcc_cmd_t irc_dcc[] =
  { {"act", "o|o", (Function) cmd_act, NULL, NULL}, {"adduser", "m|m",
						     (Function) cmd_adduser,
						     NULL, NULL}, {"channel",
								   "o|o",
								   (Function)
								   cmd_channel,
								   NULL,
								   NULL},
  {"deluser", "m|m", (Function) cmd_deluser, NULL, NULL}, {"deop", "o|o",
							   (Function)
							   cmd_deop, NULL,
							   NULL}, {"devoice",
								   "o|o",
								   (Function)
								   cmd_devoice,
								   NULL,
								   NULL},
  {"invite", "o|o", (Function) cmd_invite, NULL, NULL}, {"kick", "o|o",
							 (Function) cmd_kick,
							 NULL, NULL},
  {"kickban", "o|o", (Function) cmd_kickban, NULL, NULL}, {"mdop", "n",
							   (Function)
							   cmd_mdop, NULL,
							   NULL}, {"msg", "o",
								   (Function)
								   cmd_msg,
								   NULL,
								   NULL},
  {"op", "o|o", (Function) cmd_op, NULL, NULL}, {"reset", "m|m",
						 (Function) cmd_reset, NULL,
						 NULL}, {"resetbans", "o|o",
							 (Function)
							 cmd_resetbans, NULL,
							 NULL},
  {"resetexempts", "o|o", (Function) cmd_resetexempts, NULL, NULL},
  {"resetinvites", "o|o", (Function) cmd_resetinvites, NULL, NULL}, {"say",
								     "o|o",
								     (Function)
								     cmd_say,
								     NULL,
								     NULL},
  {"topic", "o|o", (Function) cmd_topic, NULL, NULL}, {"voice", "o|o",
						       (Function) cmd_voice,
						       NULL, NULL}, {NULL,
								     NULL,
								     NULL,
								     NULL,
								     NULL,
								     NULL} };
#endif
