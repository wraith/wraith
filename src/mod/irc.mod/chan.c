#ifdef LEAF
static time_t last_ctcp = (time_t) 0L;
static int count_ctcp = 0;
static time_t last_invtime = (time_t) 0L;
static char last_invchan[300] = "";
#define CHANNEL_ID_LEN 5
static memberlist *
newmember (struct chanset_t *chan)
{
  memberlist *x;
  for (x = chan->channel.member; x && x->nick[0]; x = x->next);
  x->next = (memberlist *) channel_malloc (sizeof (memberlist));
  x->next->next = NULL;
  x->next->nick[0] = 0;
  x->next->split = 0L;
  x->next->last = 0L;
  x->next->delay = 0L;
  chan->channel.members++;
  return x;
}
static void
update_idle (char *chname, char *nick)
{
  memberlist *m;
  struct chanset_t *chan;
  chan = findchan_by_dname (chname);
  if (chan)
    {
      m = ismember (chan, nick);
      if (m)
	m->last = now;
    }
}
static char *
getchanmode (struct chanset_t *chan)
{
  static char s[121];
  int atr, i;
  s[0] = '+';
  i = 1;
  atr = chan->channel.mode;
  if (atr & CHANINV)
    s[i++] = 'i';
  if (atr & CHANPRIV)
    s[i++] = 'p';
  if (atr & CHANSEC)
    s[i++] = 's';
  if (atr & CHANMODER)
    s[i++] = 'm';
  if (atr & CHANNOCLR)
    s[i++] = 'c';
  if (atr & CHANNOCTCP)
    s[i++] = 'C';
  if (atr & CHANREGON)
    s[i++] = 'R';
  if (atr & CHANTOPIC)
    s[i++] = 't';
  if (atr & CHANMODR)
    s[i++] = 'M';
  if (atr & CHANLONLY)
    s[i++] = 'r';
  if (atr & CHANNOMSG)
    s[i++] = 'n';
  if (atr & CHANANON)
    s[i++] = 'a';
  if (atr & CHANKEY)
    s[i++] = 'k';
  if (chan->channel.maxmembers != 0)
    s[i++] = 'l';
  s[i] = 0;
  if (chan->channel.key[0])
    i += sprintf (s + i, " %s", chan->channel.key);
  if (chan->channel.maxmembers != 0)
    sprintf (s + i, " %d", chan->channel.maxmembers);
  return s;
}

#ifdef S_IRCNET
static void
check_exemptlist (struct chanset_t *chan, char *from)
{
  masklist *e;
  int ok = 0;
  if (!use_exempts)
    return;
  for (e = chan->channel.exempt; e->mask[0]; e = e->next)
    if (wild_match (e->mask, from))
      {
	add_mode (chan, '-', 'e', e->mask);
	ok = 1;
      }
  if (prevent_mixing && ok)
    flush_mode (chan, QUICK);
}
#endif
void
priority_do (struct chanset_t *chan, int opsonly, int action)
{
  memberlist *m;
  int ops = 0, targets = 0, bpos = 0, tpos = 0, ft = 0, ct = 0, actions =
    0, sent = 0;
  if (!me_op (chan))
    return;
  if (channel_pending (chan))
    return;
  for (m = chan->channel.member; m && m->nick[0]; m = m->next)
    {
      if (!m->user)
	{
	  char s[256];
	  sprintf (s, "%s!%s", m->nick, m->userhost);
	  m->user = get_user_by_host (s);
	}
      if (m->user
	  && ((m->user->flags & (USER_BOT | USER_OP)) ==
	      (USER_BOT | USER_OP)))
	{
	  ops++;
	  if (!strcmp (m->nick, botname))
	    bpos = (ops - 1);
	}
      else if (!opsonly || chan_hasop (m))
	{
	  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };
	  if (m->user)
	    get_user_flagrec (m->user, &fr, chan->dname);
	  if (chan_deop (fr) || glob_deop (fr)
	      || (!chan_op (fr) && !glob_op (fr)))
	    {
	      targets++;
	    }
	}
    }
  if (!targets || !ops)
    return;
  ft = (bpos * targets) / ops;
  ct = ((bpos + 2) * targets + (ops - 1)) / ops;
  ct = (ct - ft + 1);
  if (ct > 20)
    ct = 20;
  while (ft >= targets)
    ft -= targets;
  actions = 0;
  sent = 0;
  for (m = chan->channel.member; m && m->nick[0]; m = m->next)
    {
      if (!opsonly || chan_hasop (m))
	{
	  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };
	  if (m->user)
	    get_user_flagrec (m->user, &fr, chan->dname);
	  if (chan_deop (fr) || glob_deop (fr)
	      || (!chan_op (fr) && !glob_op (fr)))
	    {
	      if (tpos >= ft)
		{
		  if ((action == PRIO_DEOP) && !chan_sentdeop (m))
		    {
		      actions++;
		      sent++;
		      add_mode (chan, '-', 'o', m->nick);
		      if (actions >= ct)
			{
			  flush_mode (chan, QUICK);
			  return;
			}
		    }
		  else if ((action == PRIO_KICK) && !chan_sentkick (m))
		    {
		      actions++;
		      sent++;
		      dprintf (DP_MODE, "KICK %s %s :%s%s\n", chan->name,
			       m->nick, kickprefix, kickreason (KICK_CLOSED));
		      m->flags |= SENTKICK;
		      if (actions >= ct)
			return;
		    }
		}
	      tpos++;
	    }
	}
    }
  ct = ct - actions;
  if (ct > ft)
    ct = ft;
  ft = 0;
  actions = 0;
  tpos = 0;
  for (m = chan->channel.member; m && m->nick[0]; m = m->next)
    {
      if (!opsonly || chan_hasop (m))
	{
	  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };
	  if (m->user)
	    get_user_flagrec (m->user, &fr, chan->dname);
	  if (chan_deop (fr) || glob_deop (fr)
	      || (!chan_op (fr) && !glob_op (fr)))
	    {
	      if (tpos >= ft)
		{
		  if ((action == PRIO_DEOP) && !chan_sentdeop (m))
		    {
		      actions++;
		      sent++;
		      add_mode (chan, '-', 'o', m->nick);
		      if ((actions >= ct) || (sent > 20))
			{
			  flush_mode (chan, QUICK);
			  return;
			}
		    }
		  else if ((action == PRIO_KICK) && !chan_sentkick (m))
		    {
		      actions++;
		      dprintf (DP_MODE, "KICK %s %s :%s%s\n", chan->name,
			       m->nick, kickprefix, kickreason (KICK_CLOSED));
		      m->flags |= SENTKICK;
		      if ((actions >= ct) || (sent > 5))
			return;
		    }
		}
	      tpos++;
	    }
	}
    }
}
static int
target_priority (struct chanset_t *chan, memberlist * target, int opsonly)
{
  memberlist *m;
  int ops = 0, targets = 0, bpos = 0, ft = 0, ct = 0, tp = (-1), pos = 0;
  for (m = chan->channel.member; m && m->nick[0]; m = m->next)
    {
      if (m->user
	  && ((m->user->flags & (USER_BOT | USER_OP)) ==
	      (USER_BOT | USER_OP)))
	{
	  ops++;
	  if (!strcmp (m->nick, botname))
	    bpos = ops;
	}
      else if (!opsonly || chan_hasop (m))
	{
	  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };
	  if (m->user)
	    get_user_flagrec (m->user, &fr, chan->dname);
	  if (chan_deop (fr) || glob_deop (fr)
	      || (!chan_op (fr) && !glob_op (fr)))
	    {
	      targets++;
	    }
	}
      if (m == target)
	tp = pos;
      pos++;
    }
  if (!targets || !ops || (tp < 0))
    return 0;
  ft = (bpos * targets) / ops;
  ct = ((bpos + 2) * targets + (ops - 1)) / ops;
  ct = (ct - ft + 1);
  if (ct > 20)
    ct = 20;
  while (ft >= targets)
    {
      ft -= targets;
    }
  if (ct >= targets)
    {
      if ((tp >= ft) || (tp <= (ct % targets)))
	return 1;
    }
  else
    {
      if ((tp >= ft) && (tp <= ct))
	return 1;
    }
  return 0;
}
static void
do_mask (struct chanset_t *chan, masklist * m, char *mask, char Mode)
{
  for (; m && m->mask[0]; m = m->next)
    if (wild_match (mask, m->mask) && rfc_casecmp (mask, m->mask))
      add_mode (chan, '-', Mode, m->mask);
  add_mode (chan, '+', Mode, mask);
  flush_mode (chan, QUICK);
}
static int
detect_chan_flood (char *floodnick, char *floodhost, char *from,
		   struct chanset_t *chan, int which, char *victim)
{
  char h[UHOSTLEN], ftype[12], *p;
  struct userrec *u;
  memberlist *m;
  int thr = 0, lapse = 0;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
  if (!chan || (which < 0) || (which >= FLOOD_CHAN_MAX))
    return 0;
  m = ismember (chan, floodnick);
  if (!m && (which != FLOOD_JOIN))
    return 0;
  get_user_flagrec (get_user_by_host (from), &fr, chan->dname);
  if (glob_bot (fr)
      || ((which == FLOOD_DEOP) && (glob_master (fr) || chan_master (fr))
	  && (glob_friend (fr) || chan_friend (fr))) || ((which == FLOOD_KICK)
							 && (glob_master (fr)
							     ||
							     chan_master (fr))
							 && (glob_friend (fr)
							     ||
							     chan_friend
							     (fr)))
      || ((which != FLOOD_DEOP) && (which != FLOOD_KICK)
	  && (glob_friend (fr) || chan_friend (fr) || glob_noflood (fr)
	      || chan_noflood (fr))) || (channel_dontkickops (chan)
					 && (chan_op (fr)
					     || (glob_op (fr)
						 && !chan_deop (fr)))))
    return 0;
  switch (which)
    {
    case FLOOD_PRIVMSG:
    case FLOOD_NOTICE:
      thr = chan->flood_pub_thr;
      lapse = chan->flood_pub_time;
      strcpy (ftype, "pub");
      break;
    case FLOOD_CTCP:
      thr = chan->flood_ctcp_thr;
      lapse = chan->flood_ctcp_time;
      strcpy (ftype, "pub");
      break;
    case FLOOD_NICK:
      thr = chan->flood_nick_thr;
      lapse = chan->flood_nick_time;
      strcpy (ftype, "nick");
      break;
    case FLOOD_JOIN:
      thr = chan->flood_join_thr;
      lapse = chan->flood_join_time;
      strcpy (ftype, "join");
      break;
    case FLOOD_DEOP:
      thr = chan->flood_deop_thr;
      lapse = chan->flood_deop_time;
      strcpy (ftype, "deop");
      break;
    case FLOOD_KICK:
      thr = chan->flood_kick_thr;
      lapse = chan->flood_kick_time;
      strcpy (ftype, "kick");
      break;
    }
  if ((thr == 0) || (lapse == 0))
    return 0;
  if (match_my_nick (floodnick))
    return 0;
  if (!egg_strcasecmp (floodhost, botuserhost))
    return 0;
  if ((which == FLOOD_KICK) || (which == FLOOD_DEOP))
    p = floodnick;
  else
    {
      p = strchr (floodhost, '@');
      if (p)
	{
	  p++;
	}
      if (!p)
	return 0;
    }
  if (rfc_casecmp (chan->floodwho[which], p))
    {
      strncpy (chan->floodwho[which], p, 81);
      chan->floodwho[which][81] = 0;
      chan->floodtime[which] = now;
      chan->floodnum[which] = 1;
      return 0;
    }
  if (chan->floodtime[which] < now - lapse)
    {
      chan->floodtime[which] = now;
      chan->floodnum[which] = 1;
      return 0;
    }
  if (which == FLOOD_DEOP)
    {
      if (!rfc_casecmp (chan->deopd, victim))
	return 0;
      else
	strcpy (chan->deopd, victim);
    }
  chan->floodnum[which]++;
  if (chan->floodnum[which] >= thr)
    {
      chan->floodnum[which] = 0;
      chan->floodtime[which] = 0;
      chan->floodwho[which][0] = 0;
      if (which == FLOOD_DEOP)
	chan->deopd[0] = 0;
      u = get_user_by_host (from);
      if (check_tcl_flud (floodnick, floodhost, u, ftype, chan->dname))
	return 0;
      switch (which)
	{
	case FLOOD_PRIVMSG:
	case FLOOD_NOTICE:
	case FLOOD_CTCP:
	  if (!chan_sentkick (m) && me_op (chan))
	    {
	      putlog (LOG_MODES, chan->dname, IRC_FLOODKICK, floodnick);
	      dprintf (DP_MODE, "KICK %s %s :%s\n", chan->name, floodnick,
		       CHAN_FLOOD);
	      m->flags |= SENTKICK;
	    }
	  return 1;
	case FLOOD_JOIN:
	case FLOOD_NICK:
	  if (use_exempts
	      && (u_match_mask (global_exempts, from)
		  || u_match_mask (chan->exempts, from)))
	    return 1;
	  simple_sprintf (h, "*!*@%s", p);
	  if (!isbanned (chan, h) && me_op (chan))
	    {
	      check_exemptlist (chan, from);
	      do_mask (chan, chan->channel.ban, h, 'b');
	    }
	  if ((u_match_mask (global_bans, from))
	      || (u_match_mask (chan->bans, from)))
	    return 1;
	  if (which == FLOOD_JOIN)
	    putlog (LOG_MISC | LOG_JOIN, chan->dname, IRC_FLOODIGNORE3, p);
	  else
	    putlog (LOG_MISC | LOG_JOIN, chan->dname, IRC_FLOODIGNORE4, p);
	  strcpy (ftype + 4, " flood");
	  u_addban (chan, h, botnetnick, ftype, now + (60 * chan->ban_time),
		    0);
	  if (!channel_enforcebans (chan) && me_op (chan))
	    {
	      char s[UHOSTLEN];
	      for (m = chan->channel.member; m && m->nick[0]; m = m->next)
		{
		  sprintf (s, "%s!%s", m->nick, m->userhost);
		  if (wild_match (h, s)
		      && (m->joined >= chan->floodtime[which])
		      && !chan_sentkick (m) && !match_my_nick (m->nick)
		      && me_op (chan))
		    {
		      m->flags |= SENTKICK;
		      if (which == FLOOD_JOIN)
			dprintf (DP_SERVER, "KICK %s %s :%s\n", chan->name,
				 m->nick, IRC_JOIN_FLOOD);
		      else
			dprintf (DP_SERVER, "KICK %s %s :%s\n", chan->name,
				 m->nick, IRC_NICK_FLOOD);
		    }
		}
	    }
	  return 1;
	case FLOOD_KICK:
	  if (me_op (chan) && !chan_sentkick (m))
	    {
	      putlog (LOG_MODES, chan->dname, "Kicking %s, for mass kick.",
		      floodnick);
	      dprintf (DP_MODE, "KICK %s %s :%s\n", chan->name, floodnick,
		       IRC_MASSKICK);
	      m->flags |= SENTKICK;
	    }
	  return 1;
	case FLOOD_DEOP:
	  if (me_op (chan) && !chan_sentkick (m))
	    {
	      putlog (LOG_MODES, chan->dname, CHAN_MASSDEOP, chan->dname,
		      from);
	      dprintf (DP_MODE, "KICK %s %s :%s\n", chan->name, floodnick,
		       CHAN_MASSDEOP_KICK);
	      m->flags |= SENTKICK;
	    }
	  return 1;
	}
    }
  return 0;
}
static char *
quickban (struct chanset_t *chan, char *uhost)
{
  static char s1[512];
  maskhost (uhost, s1);
  if ((strlen (s1) != 1) && (strict_host == 0))
    s1[2] = '*';
  do_mask (chan, chan->channel.ban, s1, 'b');
  return s1;
}
static void
kick_all (struct chanset_t *chan, char *hostmask, char *comment, int bantype)
{
  memberlist *m;
  char kicknick[512], s[UHOSTLEN];
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
  int k, l, flushed;
  if (!me_op (chan))
    return;
  k = 0;
  flushed = 0;
  kicknick[0] = 0;
  for (m = chan->channel.member; m && m->nick[0]; m = m->next)
    {
      sprintf (s, "%s!%s", m->nick, m->userhost);
      get_user_flagrec (m->user ? m->user : get_user_by_host (s), &fr,
			chan->dname);
      if (me_op (chan) && wild_match (hostmask, s) && !chan_sentkick (m)
	  && !match_my_nick (m->nick) && !chan_issplit (m)
	  && !glob_friend (fr) && !chan_friend (fr) && !(use_exempts
							 &&
							 ((bantype
							   &&
							   isexempted (chan,
								       s))
							  ||
							  (u_match_mask
							   (global_exempts, s)
							   ||
							   u_match_mask
							   (chan->exempts,
							    s))))
	  && !(channel_dontkickops (chan)
	       && (chan_op (fr) || (glob_op (fr) && !chan_deop (fr)))))
	{
	  if (!flushed)
	    {
	      flush_mode (chan, QUICK);
	      flushed += 1;
	    }
	  m->flags |= SENTKICK;
	  if (kicknick[0])
	    strcat (kicknick, ",");
	  strcat (kicknick, m->nick);
	  k += 1;
	  l = strlen (chan->name) + strlen (kicknick) + strlen (comment) + 5;
	  if ((kick_method != 0 && k == kick_method) || (l > 480))
	    {
	      dprintf (DP_SERVER, "KICK %s %s :%s\n", chan->name, kicknick,
		       comment);
	      k = 0;
	      kicknick[0] = 0;
	    }
	}
    }
  if (k > 0)
    dprintf (DP_SERVER, "KICK %s %s :%s\n", chan->name, kicknick, comment);
}
static void
refresh_ban_kick (struct chanset_t *chan, char *user, char *nick)
{
  register maskrec *b;
  memberlist *m;
  int cycle;
  m = ismember (chan, nick);
  if (!m || chan_sentkick (m))
    return;
  for (cycle = 0; cycle < 2; cycle++)
    {
      for (b = cycle ? chan->bans : global_bans; b; b = b->next)
	{
	  if (wild_match (b->mask, user))
	    {
	      struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
	      char c[512];
	      char s[UHOSTLEN];
	      sprintf (s, "%s!%s", m->nick, m->userhost);
	      get_user_flagrec (m->user ? m->user : get_user_by_host (s), &fr,
				chan->dname);
	      if (!glob_friend (fr) && !chan_friend (fr))
		{
		  add_mode (chan, '-', 'o', nick);
		  check_exemptlist (chan, user);
		  do_mask (chan, chan->channel.ban, b->mask, 'b');
		  b->lastactive = now;
		  if (b->desc && b->desc[0] != '@')
		    egg_snprintf (c, sizeof c, "%s%s", IRC_PREBANNED,
				  b->desc);
		  else
		    c[0] = 0;
		  kick_all (chan, b->mask, c[0] ? c : IRC_YOUREBANNED, 0);
		  return;
		}
	    }
	}
    }
}
static void
refresh_exempt (struct chanset_t *chan, char *user)
{
  maskrec *e;
  masklist *b;
  int cycle;
  for (cycle = 0; cycle < 2; cycle++)
    {
      for (e = cycle ? chan->exempts : global_exempts; e; e = e->next)
	{
	  if (wild_match (user, e->mask) || wild_match (e->mask, user))
	    {
	      for (b = chan->channel.ban; b && b->mask[0]; b = b->next)
		{
		  if (wild_match (b->mask, user)
		      || wild_match (user, b->mask))
		    {
		      if (e->lastactive < now - 60
			  && !isexempted (chan, e->mask))
			{
			  do_mask (chan, chan->channel.exempt, e->mask, 'e');
			  e->lastactive = now;
			}
		    }
		}
	    }
	}
    }
}
static void
refresh_invite (struct chanset_t *chan, char *user)
{
  maskrec *i;
  int cycle;
  for (cycle = 0; cycle < 2; cycle++)
    {
      for (i = cycle ? chan->invites : global_invites; i; i = i->next)
	{
	  if (wild_match (i->mask, user)
	      && ((i->flags & MASKREC_STICKY)
		  || (chan->channel.mode & CHANINV)))
	    {
	      if (i->lastactive < now - 60 && !isinvited (chan, i->mask))
		{
		  do_mask (chan, chan->channel.invite, i->mask, 'I');
		  i->lastactive = now;
		  return;
		}
	    }
	}
    }
}
static void
enforce_bans (struct chanset_t *chan)
{
  char me[UHOSTLEN];
  masklist *b;
  if (!me_op (chan))
    return;
  simple_sprintf (me, "%s!%s", botname, botuserhost);
  for (b = chan->channel.ban; b && b->mask[0]; b = b->next)
    {
      if (!wild_match (b->mask, me))
	if (!isexempted (chan, b->mask))
	  kick_all (chan, b->mask, IRC_YOUREBANNED, 1);
    }
}
static void
recheck_bans (struct chanset_t *chan)
{
  maskrec *u;
  int cycle;
  for (cycle = 0; cycle < 2; cycle++)
    {
      for (u = cycle ? chan->bans : global_bans; u; u = u->next)
	if (!isbanned (chan, u->mask)
	    && (!channel_dynamicbans (chan) || (u->flags & MASKREC_STICKY)))
	  add_mode (chan, '+', 'b', u->mask);
    }
}

#ifdef S_IRCNET
static void
recheck_exempts (struct chanset_t *chan)
{
  maskrec *e;
  masklist *b;
  int cycle;
  for (cycle = 0; cycle < 2; cycle++)
    {
      for (e = cycle ? chan->exempts : global_exempts; e; e = e->next)
	{
	  if (!isexempted (chan, e->mask)
	      && (!channel_dynamicexempts (chan)
		  || (e->flags & MASKREC_STICKY)))
	    add_mode (chan, '+', 'e', e->mask);
	  for (b = chan->channel.ban; b && b->mask[0]; b = b->next)
	    {
	      if ((wild_match (b->mask, e->mask)
		   || wild_match (e->mask, b->mask))
		  && !isexempted (chan, e->mask))
		add_mode (chan, '+', 'e', e->mask);
	    }
	}
    }
}
static void
recheck_invites (struct chanset_t *chan)
{
  maskrec *ir;
  int cycle;
  for (cycle = 0; cycle < 2; cycle++)
    {
      for (ir = cycle ? chan->invites : global_invites; ir; ir = ir->next)
	{
	  if (!isinvited (chan, ir->mask)
	      &&
	      ((!channel_dynamicinvites (chan)
		&& !(chan->channel.mode & CHANINV))
	       || ir->flags & MASKREC_STICKY))
	    add_mode (chan, '+', 'I', ir->mask);
	}
    }
}
#endif
static void
resetmasks (struct chanset_t *chan, masklist * m, maskrec * mrec,
	    maskrec * global_masks, char mode)
{
  if (!me_op (chan))
    return;
  for (; m && m->mask[0]; m = m->next)
    {
      if (!u_equals_mask (global_masks, m->mask)
	  && !u_equals_mask (mrec, m->mask))
	add_mode (chan, '-', mode, m->mask);
    }
  switch (mode)
    {
    case 'b':
      recheck_bans (chan);
      break;
#ifdef S_IRCNET
    case 'e':
      recheck_exempts (chan);
      break;
    case 'I':
      recheck_invites (chan);
      break;
#endif
    default:
      putlog (LOG_MISC, "*", "(!) Invalid mode '%c' in resetmasks()", mode);
      break;
    }
}
static void
check_this_ban (struct chanset_t *chan, char *banmask, int sticky)
{
  memberlist *m;
  char user[UHOSTLEN];
  if (!me_op (chan))
    return;
  for (m = chan->channel.member; m && m->nick[0]; m = m->next)
    {
      sprintf (user, "%s!%s", m->nick, m->userhost);
      if (wild_match (banmask, user)
	  && !(use_exempts
	       && (u_match_mask (global_exempts, user)
		   || u_match_mask (chan->exempts, user))))
	refresh_ban_kick (chan, user, m->nick);
    }
  if (!isbanned (chan, banmask) && (!channel_dynamicbans (chan) || sticky))
    add_mode (chan, '+', 'b', banmask);
}
static void
recheck_channel_modes (struct chanset_t *chan)
{
  int cur = chan->channel.mode, mns = chan->mode_mns_prot, pls =
    chan->mode_pls_prot;
  if (channel_closed (chan))
    {
      pls |= CHANINV;
      mns &= ~CHANINV;
    }
  if (!(chan->status & CHAN_ASKEDMODES))
    {
      if (pls & CHANINV && !(cur & CHANINV))
	add_mode (chan, '+', 'i', "");
      else if (mns & CHANINV && cur & CHANINV)
	add_mode (chan, '-', 'i', "");
      if (pls & CHANPRIV && !(cur & CHANPRIV))
	add_mode (chan, '+', 'p', "");
      else if (mns & CHANPRIV && cur & CHANPRIV)
	add_mode (chan, '-', 'p', "");
      if (pls & CHANSEC && !(cur & CHANSEC))
	add_mode (chan, '+', 's', "");
      else if (mns & CHANSEC && cur & CHANSEC)
	add_mode (chan, '-', 's', "");
      if (pls & CHANMODER && !(cur & CHANMODER))
	add_mode (chan, '+', 'm', "");
      else if (mns & CHANMODER && cur & CHANMODER)
	add_mode (chan, '-', 'm', "");
      if (pls & CHANNOCLR && !(cur & CHANNOCLR))
	add_mode (chan, '+', 'c', "");
      else if (mns & CHANNOCLR && cur & CHANNOCLR)
	add_mode (chan, '-', 'c', "");
      if (pls & CHANNOCTCP && !(cur & CHANNOCTCP))
	add_mode (chan, '+', 'C', "");
      else if (mns & CHANNOCTCP && cur & CHANNOCTCP)
	add_mode (chan, '-', 'C', "");
      if (pls & CHANREGON && !(cur & CHANREGON))
	add_mode (chan, '+', 'R', "");
      else if (mns & CHANREGON && cur & CHANREGON)
	add_mode (chan, '-', 'R', "");
      if (pls & CHANMODR && !(cur & CHANMODR))
	add_mode (chan, '+', 'M', "");
      else if (mns & CHANMODR && cur & CHANMODR)
	add_mode (chan, '-', 'M', "");
      if (pls & CHANLONLY && !(cur & CHANLONLY))
	add_mode (chan, '+', 'r', "");
      else if (mns & CHANLONLY && cur & CHANLONLY)
	add_mode (chan, '-', 'r', "");
      if (pls & CHANTOPIC && !(cur & CHANTOPIC))
	add_mode (chan, '+', 't', "");
      else if (mns & CHANTOPIC && cur & CHANTOPIC)
	add_mode (chan, '-', 't', "");
      if (pls & CHANNOMSG && !(cur & CHANNOMSG))
	add_mode (chan, '+', 'n', "");
      else if ((mns & CHANNOMSG) && (cur & CHANNOMSG))
	add_mode (chan, '-', 'n', "");
      if ((pls & CHANANON) && !(cur & CHANANON))
	add_mode (chan, '+', 'a', "");
      else if ((mns & CHANANON) && (cur & CHANANON))
	add_mode (chan, '-', 'a', "");
      if ((pls & CHANQUIET) && !(cur & CHANQUIET))
	add_mode (chan, '+', 'q', "");
      else if ((mns & CHANQUIET) && (cur & CHANQUIET))
	add_mode (chan, '-', 'q', "");
      if ((chan->limit_prot != 0) && (chan->channel.maxmembers == 0))
	{
	  char s[50];
	  sprintf (s, "%d", chan->limit_prot);
	  add_mode (chan, '+', 'l', s);
	}
      else if ((mns & CHANLIMIT) && (chan->channel.maxmembers != 0))
	add_mode (chan, '-', 'l', "");
      if (chan->key_prot[0])
	{
	  if (rfc_casecmp (chan->channel.key, chan->key_prot) != 0)
	    {
	      if (chan->channel.key[0])
		add_mode (chan, '-', 'k', chan->channel.key);
	      add_mode (chan, '+', 'k', chan->key_prot);
	    }
	}
      else if ((mns & CHANKEY) && (chan->channel.key[0]))
	add_mode (chan, '-', 'k', chan->channel.key);
    }
}
static void
check_this_member (struct chanset_t *chan, char *nick, struct flag_record *fr)
{
  memberlist *m;
  char s[UHOSTLEN], *p;
  m = ismember (chan, nick);
  if (!m || match_my_nick (nick) || !me_op (chan))
    return;
  if (me_op (chan))
    {
      if (chan_hasop (m)
	  && ((chan_deop (*fr) || (glob_deop (*fr) && !chan_op (*fr)))
	      || (channel_bitch (chan)
		  && (!chan_op (*fr)
		      && !(glob_op (*fr) && !chan_deop (*fr))))))
	{
	  if (target_priority (chan, m, 1))
	    {
	      add_mode (chan, '-', 'o', m->nick);
	    }
	}
      if (chan_hasvoice (m)
	  && (chan_quiet (*fr) || (glob_quiet (*fr) && !chan_voice (*fr))))
	add_mode (chan, '-', 'v', m->nick);
    }
  sprintf (s, "%s!%s", m->nick, m->userhost);
#ifdef S_IRCNET
  if (use_invites
      && (u_match_mask (global_invites, s)
	  || u_match_mask (chan->invites, s)))
    refresh_invite (chan, s);
  if (!
      (use_exempts
       && (u_match_mask (global_exempts, s)
	   || u_match_mask (chan->exempts, s))))
    {
#else
  if (1)
    {
#endif
      if (u_match_mask (global_bans, s) || u_match_mask (chan->bans, s))
	refresh_ban_kick (chan, s, m->nick);
      if (!chan_sentkick (m) && (chan_kick (*fr) || glob_kick (*fr))
	  && me_op (chan))
	{
#ifdef S_IRCNET
	  check_exemptlist (chan, s);
#endif
	  quickban (chan, m->userhost);
	  p = get_user (&USERENTRY_COMMENT, m->user);
	  if (p[0])
	    {
	      dprintf (DP_SERVER, "KICK %s %s :%s\n", chan->name, m->nick,
		       p ? p : IRC_POLITEKICK);
	    }
	  else
	    {
	      dprintf (DP_SERVER, STR ("KICK %s %s :%s%s\n"), chan->name,
		       m->nick, bankickprefix, kickreason (KICK_KUSER));
	    }
	  m->flags |= SENTKICK;
	}
    }
}
static void
check_this_user (char *hand, int delete, char *host)
{
  char s[UHOSTLEN];
  memberlist *m;
  struct userrec *u;
  struct chanset_t *chan;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
  for (chan = chanset; chan; chan = chan->next)
    for (m = chan->channel.member; m && m->nick[0]; m = m->next)
      {
	sprintf (s, "%s!%s", m->nick, m->userhost);
	u = m->user ? m->user : get_user_by_host (s);
	if ((u && !egg_strcasecmp (u->handle, hand) && delete < 2)
	    || (!u && delete == 2 && wild_match (host, fixfrom (s))))
	  {
	    u = delete ? NULL : u;
	    get_user_flagrec (u, &fr, chan->dname);
	    check_this_member (chan, m->nick, &fr);
	  }
      }
}
void
enforce_bitch (struct chanset_t *chan)
{
  priority_do (chan, 1, PRIO_DEOP);
} static void
recheck_channel (struct chanset_t *chan, int dobans)
{
  memberlist *m;
  char s[UHOSTLEN];
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
  static int stacking = 0;
  int botops = 0, botnonops = 0, nonbotops = 0;
  int stop_reset = 0;
  if (stacking)
    return;
  if (!userlist)
    return;
  stacking++;
  putlog (LOG_DEBUG, "*", "recheck_channel %s", chan->dname);
  for (m = chan->channel.member; m && m->nick[0]; m = m->next)
    {
      if (!m->user)
	{
	  sprintf (s, "%s!%s", m->nick, m->userhost);
	  m->user = get_user_by_host (s);
	}
      if (m && m->user)
	{
	  if ((m->user->flags & (USER_BOT | USER_OP)) == (USER_BOT | USER_OP))
	    {
	      if (chan_hasop (m))
		botops++;
	      else
		botnonops++;
	    }
	  else if (chan_hasop (m))
	    {
	      nonbotops++;
	    }
	}
      else if (chan_hasop (m))
	{
	  nonbotops++;
	}
    }
  if (channel_take (chan))
    {
      if (botnonops && (((botops * 5) / (botnonops + botops)) < 4))
	{
	  int actions = 0;
	  for (m = chan->channel.member; m && m->nick[0]; m = m->next)
	    {
	      struct flag_record fr = { FR_CHAN | FR_GLOBAL, 0, 0, 0, 0 };
	      get_user_flagrec (m->user, &fr, chan->dname);
	      if (glob_bot (fr) && glob_op (fr) && !chan_hasop (m))
		{
		  actions++;
		  add_mode (chan, '+', 'o', m->nick);
		  if (actions >= 20)
		    {
		      stacking--;
		      flush_mode (chan, QUICK);
		      return;
		    }
		}
	    }
	}
      if (nonbotops)
	{
	  enforce_bitch (chan);
	  stacking--;
	  return;
	}
    }
  if (botops == 1)
    {
      stacking--;
      return;
    }
  if (channel_bitch (chan) || channel_closed (chan))
    enforce_bitch (chan);
  for (m = chan->channel.member; m && m->nick[0]; m = m->next)
    {
      sprintf (s, "%s!%s", m->nick, m->userhost);
      if (!m->user && !m->tried_getuser)
	{
	  m->tried_getuser = 1;
	  m->user = get_user_by_host (s);
	}
      get_user_flagrec (m->user, &fr, chan->dname);
      if (glob_bot (fr) && chan_hasop (m) && !match_my_nick (m->nick))
	stop_reset = 1;
      check_this_member (chan, m->nick, &fr);
    }
  if (channel_closed (chan))
    channel_check_locked (chan);
  if (dobans)
    {
      if (channel_nouserbans (chan) && !stop_reset)
	resetbans (chan);
      else
	recheck_bans (chan);
#ifdef S_IRCNET
      if (use_invites)
	{
	  if (channel_nouserinvites (chan) && !stop_reset)
	    resetinvites (chan);
	  else
	    recheck_invites (chan);
	}
      if (use_exempts)
	{
	  if (channel_nouserexempts (chan) && !stop_reset)
	    resetexempts (chan);
	  else
	    recheck_exempts (chan);
	}
#endif
      if (channel_enforcebans (chan))
	enforce_bans (chan);
      flush_mode (chan, QUICK);
      if ((chan->status & CHAN_ASKEDMODES) && !channel_inactive (chan))
	dprintf (DP_MODE, "MODE %s\n", chan->name);
      recheck_channel_modes (chan);
    }
  stacking--;
}
static int
got324 (char *from, char *msg)
{
  int i = 1, ok = 0;
  char *p, *q, *chname;
  struct chanset_t *chan;
  newsplit (&msg);
  chname = newsplit (&msg);
  if (match_my_nick (chname))
    return 0;
  chan = findchan (chname);
  if (!chan)
    {
      putlog (LOG_MISC, "*", "%s: %s", IRC_UNEXPECTEDMODE, chname);
      dprintf (DP_SERVER, "PART %s\n", chname);
      return 0;
    }
  if (chan->status & CHAN_ASKEDMODES)
    ok = 1;
  chan->status &= ~CHAN_ASKEDMODES;
  chan->channel.mode = 0;
  while (msg[i] != 0)
    {
      if (msg[i] == 'i')
	chan->channel.mode |= CHANINV;
      if (msg[i] == 'p')
	chan->channel.mode |= CHANPRIV;
      if (msg[i] == 's')
	chan->channel.mode |= CHANSEC;
      if (msg[i] == 'm')
	chan->channel.mode |= CHANMODER;
      if (msg[i] == 'c')
	chan->channel.mode |= CHANNOCLR;
      if (msg[i] == 'C')
	chan->channel.mode |= CHANNOCTCP;
      if (msg[i] == 'R')
	chan->channel.mode |= CHANREGON;
      if (msg[i] == 'M')
	chan->channel.mode |= CHANMODR;
      if (msg[i] == 'r')
	chan->channel.mode |= CHANLONLY;
      if (msg[i] == 't')
	chan->channel.mode |= CHANTOPIC;
      if (msg[i] == 'n')
	chan->channel.mode |= CHANNOMSG;
      if (msg[i] == 'a')
	chan->channel.mode |= CHANANON;
      if (msg[i] == 'q')
	chan->channel.mode |= CHANQUIET;
      if (msg[i] == 'k')
	{
	  chan->channel.mode |= CHANKEY;
	  p = strchr (msg, ' ');
	  if (p != NULL)
	    {
	      p++;
	      q = strchr (p, ' ');
	      if (q != NULL)
		{
		  *q = 0;
		  set_key (chan, p);
		  strcpy (p, q + 1);
		}
	      else
		{
		  set_key (chan, p);
		  *p = 0;
		}
	    }
	  if ((chan->channel.mode & CHANKEY)
	      && (!chan->channel.key[0] || !strcmp ("*", chan->channel.key)))
	    chan->status |= CHAN_ASKEDMODES;
	}
      if (msg[i] == 'l')
	{
	  p = strchr (msg, ' ');
	  if (p != NULL)
	    {
	      p++;
	      q = strchr (p, ' ');
	      if (q != NULL)
		{
		  *q = 0;
		  chan->channel.maxmembers = atoi (p);
		  strcpy (p, q + 1);
		}
	      else
		{
		  chan->channel.maxmembers = atoi (p);
		  *p = 0;
		}
	    }
	}
      i++;
    }
  if (ok)
    recheck_channel_modes (chan);
  return 0;
}
static int
got352or4 (struct chanset_t *chan, char *user, char *host, char *serv,
	   char *nick, char *flags)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };
  char userhost[UHOSTLEN];
  memberlist *m;
  int waschanop;
  m = ismember (chan, nick);
  if (!m)
    {
      m = newmember (chan);
      m->joined = m->split = m->delay = 0L;
      m->flags = 0;
      m->last = now;
    }
  strcpy (m->nick, nick);
  if (serv)
    {
      struct chanset_t *ch;
      memberlist *ml;
      strncpy0 (m->server, serv, SERVLEN);
      for (ch = chanset; ch; ch = ch->next)
	{
	  if (ch != chan)
	    {
	      for (ml = ch->channel.member; ml && ml->nick[0]; ml = ml->next)
		{
		  if (!strcmp (ml->nick, m->nick))
		    {
		      strcpy (ml->server, m->server);
		      break;
		    }
		}
	    }
	}
    }
  else
    m->server[0] = 0;
  simple_sprintf (m->userhost, "%s@%s", user, host);
  simple_sprintf (userhost, "%s!%s", nick, m->userhost);
  m->user = NULL;
  if (match_my_nick (nick))
    {
      strcpy (botuserhost, m->userhost);
      m->joined = now;
    }
  waschanop = me_op (chan);
  if (strchr (flags, '@') != NULL)
    m->flags |= (CHANOP | WASOP);
  else
    m->flags &= ~(CHANOP | WASOP);
  if (strchr (flags, '*'))
    m->flags |= OPER;
  else
    m->flags &= ~OPER;
  if (strchr (flags, '+') != NULL)
    m->flags |= CHANVOICE;
  else
    m->flags &= ~CHANVOICE;
  if (!(m->flags & (CHANVOICE | CHANOP)))
    m->flags |= STOPWHO;
  if (match_my_nick (nick) && !waschanop && me_op (chan))
    recheck_channel (chan, 1);
  if (match_my_nick (nick) && any_ops (chan) && !me_op (chan))
    chan->channel.do_opreq = 1;
  m->user = get_user_by_host (userhost);
  get_user_flagrec (m->user, &fr, chan->dname);
  if (chan_hasop (m) && me_op (chan)
      && (chan_deop (fr) || (glob_deop (fr) && !chan_op (fr)))
      && !match_my_nick (nick) && target_priority (chan, m, 1))
    {
      add_mode (chan, '-', 'o', nick);
    }
  if (channel_enforcebans (chan)
      && (u_match_mask (global_bans, userhost)
	  || u_match_mask (chan->bans, userhost)) && !match_my_nick (nick)
      && me_op (chan) && target_priority (chan, m, 0))
    {
      dprintf (DP_SERVER, STR ("KICK %s %s :%s%s\n"), chan->name, nick,
	       bankickprefix, kickreason (KICK_BANNED));
      m->flags |= SENTKICK;
    }
  else if ((chan_kick (fr) || glob_kick (fr)) && !match_my_nick (nick)
	   && me_op (chan) && target_priority (chan, m, 0))
    {
      quickban (chan, userhost);
      dprintf (DP_SERVER, STR ("KICK %s %s :%s%s\n"), chan->name, nick,
	       bankickprefix, kickreason (KICK_KUSER));
      m->flags |= SENTKICK;
    }
  return 0;
}
static int
got352 (char *from, char *msg)
{
  char *nick, *user, *host, *chname, *flags, *serv;
  struct chanset_t *chan;
  newsplit (&msg);
  chname = newsplit (&msg);
  chan = findchan (chname);
  if (chan)
    {
      user = newsplit (&msg);
      host = newsplit (&msg);
      serv = newsplit (&msg);
      nick = newsplit (&msg);
      flags = newsplit (&msg);
      got352or4 (chan, user, host, serv, nick, flags);
    }
  return 0;
}
static int
got354 (char *from, char *msg)
{
  char *nick, *user, *host, *chname, *flags;
  struct chanset_t *chan;
  if (use_354)
    {
      newsplit (&msg);
      if (msg[0] && (strchr (CHANMETA, msg[0]) != NULL))
	{
	  chname = newsplit (&msg);
	  chan = findchan (chname);
	  if (chan)
	    {
	      user = newsplit (&msg);
	      host = newsplit (&msg);
	      nick = newsplit (&msg);
	      flags = newsplit (&msg);
	      got352or4 (chan, user, host, NULL, nick, flags);
	    }
	}
    }
  return 0;
}
static int
got315 (char *from, char *msg)
{
  char *chname;
  struct chanset_t *chan;
  Context;
  newsplit (&msg);
  chname = newsplit (&msg);
  chan = findchan (chname);
  if (!chan || !channel_pending (chan))
    return 0;
  chan->status |= CHAN_ACTIVE;
  chan->status &= ~CHAN_PEND;
  if (!ismember (chan, botname))
    {
      putlog (LOG_MISC | LOG_JOIN, chan->dname, "Oops, I'm not really on %s.",
	      chan->dname);
      clear_channel (chan, 1);
      chan->status &= ~CHAN_ACTIVE;
      dprintf (DP_MODE, "JOIN %s %s\n",
	       (chan->name[0]) ? chan->name : chan->dname,
	       chan->channel.key[0] ? chan->channel.key : chan->key_prot);
    }
  else
    {
      Context;
      if (me_op (chan))
	recheck_channel (chan, 1);
      else if (chan->channel.members == 1)
	chan->status |= CHAN_STOP_CYCLE;
      else
	request_op (chan);
    }
  return 0;
}
static int
got367 (char *from, char *origmsg)
{
  char *ban, *who, *chname, buf[511], *msg;
  struct chanset_t *chan;
  strncpy (buf, origmsg, 510);
  buf[510] = 0;
  msg = buf;
  newsplit (&msg);
  chname = newsplit (&msg);
  chan = findchan (chname);
  if (!chan || !(channel_pending (chan) || channel_active (chan)))
    return 0;
  ban = newsplit (&msg);
  who = newsplit (&msg);
  if (who[0])
    newban (chan, ban, who);
  else
    newban (chan, ban, "existent");
  return 0;
}
static int
got368 (char *from, char *msg)
{
  struct chanset_t *chan;
  char *chname;
  newsplit (&msg);
  chname = newsplit (&msg);
  chan = findchan (chname);
  if (chan)
    chan->status &= ~CHAN_ASKEDBANS;
  return 0;
}

#ifdef S_IRCNET
static int
got348 (char *from, char *origmsg)
{
  char *exempt, *who, *chname, buf[511], *msg;
  struct chanset_t *chan;
  if (use_exempts == 0)
    return 0;
  strncpy (buf, origmsg, 510);
  buf[510] = 0;
  msg = buf;
  newsplit (&msg);
  chname = newsplit (&msg);
  chan = findchan (chname);
  if (!chan || !(channel_pending (chan) || channel_active (chan)))
    return 0;
  exempt = newsplit (&msg);
  who = newsplit (&msg);
  if (who[0])
    newexempt (chan, exempt, who);
  else
    newexempt (chan, exempt, "existent");
  return 0;
}
static int
got349 (char *from, char *msg)
{
  struct chanset_t *chan;
  char *chname;
  if (use_exempts == 1)
    {
      newsplit (&msg);
      chname = newsplit (&msg);
      chan = findchan (chname);
      if (chan)
	chan->ircnet_status &= ~CHAN_ASKED_EXEMPTS;
    }
  return 0;
}
static int
got346 (char *from, char *origmsg)
{
  char *invite, *who, *chname, buf[511], *msg;
  struct chanset_t *chan;
  strncpy (buf, origmsg, 510);
  buf[510] = 0;
  msg = buf;
  if (use_invites == 0)
    return 0;
  newsplit (&msg);
  chname = newsplit (&msg);
  chan = findchan (chname);
  if (!chan || !(channel_pending (chan) || channel_active (chan)))
    return 0;
  invite = newsplit (&msg);
  who = newsplit (&msg);
  if (who[0])
    newinvite (chan, invite, who);
  else
    newinvite (chan, invite, "existent");
  return 0;
}
static int
got347 (char *from, char *msg)
{
  struct chanset_t *chan;
  char *chname;
  if (use_invites == 1)
    {
      newsplit (&msg);
      chname = newsplit (&msg);
      chan = findchan (chname);
      if (chan)
	chan->ircnet_status &= ~CHAN_ASKED_INVITED;
    }
  return 0;
}
#endif
static int
got405 (char *from, char *msg)
{
  char *chname;
  newsplit (&msg);
  chname = newsplit (&msg);
  putlog (LOG_MISC, "*", IRC_TOOMANYCHANS, chname);
  return 0;
}
static int
got403 (char *from, char *msg)
{
  char *chname;
  struct chanset_t *chan;
  newsplit (&msg);
  chname = newsplit (&msg);
  if (chname && chname[0] == '!')
    {
      chan = findchan_by_dname (chname);
      if (!chan)
	{
	  chan = findchan (chname);
	  if (!chan)
	    return 0;
	  putlog (LOG_MISC, "*",
		  "Unique channel %s does not exist... Attempting to join with "
		  "short name.", chname);
	  dprintf (DP_SERVER, "JOIN %s\n", chan->dname);
	}
      else
	{
	  putlog (LOG_MISC, "*",
		  "Channel %s does not exist... Attempting to create it.",
		  chname);
	  dprintf (DP_SERVER, "JOIN !%s\n", chan->dname);
	}
    }
  return 0;
}
static int
got471 (char *from, char *msg)
{
  char *chname;
  struct chanset_t *chan;
  newsplit (&msg);
  chname = newsplit (&msg);
  if ((chname[0] == '!') && (strlen (chname) > CHANNEL_ID_LEN))
    {
      chname += CHANNEL_ID_LEN;
      chname[0] = '!';
    }
  chan = findchan_by_dname (chname);
  if (chan)
    {
      putlog (LOG_JOIN, chan->dname, IRC_CHANFULL, chan->dname);
      request_in (chan);
      chan = findchan_by_dname (chname);
      if (!chan)
	return 0;
      if (chan->need_limit[0])
	do_tcl ("need-limit", chan->need_limit);
    }
  else
    putlog (LOG_JOIN, chname, IRC_CHANFULL, chname);
  return 0;
}
static int
got473 (char *from, char *msg)
{
  char *chname;
  struct chanset_t *chan;
  newsplit (&msg);
  chname = newsplit (&msg);
  if ((chname[0] == '!') && (strlen (chname) > CHANNEL_ID_LEN))
    {
      chname += CHANNEL_ID_LEN;
      chname[0] = '!';
    }
  chan = findchan_by_dname (chname);
  if (chan)
    {
      putlog (LOG_JOIN, chan->dname, IRC_CHANINVITEONLY, chan->dname);
      request_in (chan);
      chan = findchan_by_dname (chname);
      if (!chan)
	return 0;
      if (chan->need_invite[0])
	do_tcl ("need-invite", chan->need_invite);
    }
  else
    putlog (LOG_JOIN, chname, IRC_CHANINVITEONLY, chname);
  return 0;
}
static int
got474 (char *from, char *msg)
{
  char *chname;
  struct chanset_t *chan;
  newsplit (&msg);
  chname = newsplit (&msg);
  if ((chname[0] == '!') && (strlen (chname) > CHANNEL_ID_LEN))
    {
      chname += CHANNEL_ID_LEN;
      chname[0] = '!';
    }
  chan = findchan_by_dname (chname);
  if (chan)
    {
      putlog (LOG_JOIN, chan->dname, IRC_BANNEDFROMCHAN, chan->dname);
      request_in (chan);
      chan = findchan_by_dname (chname);
      if (!chan)
	return 0;
      if (chan->need_unban[0])
	do_tcl ("need-unban", chan->need_unban);
    }
  else
    putlog (LOG_JOIN, chname, IRC_BANNEDFROMCHAN, chname);
  return 0;
}
static int
got475 (char *from, char *msg)
{
  char *chname;
  struct chanset_t *chan;
  newsplit (&msg);
  chname = newsplit (&msg);
  if ((chname[0] == '!') && (strlen (chname) > CHANNEL_ID_LEN))
    {
      chname += CHANNEL_ID_LEN;
      chname[0] = '!';
    }
  chan = findchan_by_dname (chname);
  if (chan)
    {
      putlog (LOG_JOIN, chan->dname, IRC_BADCHANKEY, chan->dname);
      if (chan->channel.key[0])
	{
	  nfree (chan->channel.key);
	  chan->channel.key = (char *) channel_malloc (1);
	  chan->channel.key[0] = 0;
	  dprintf (DP_MODE, "JOIN %s %s\n", chan->dname, chan->key_prot);
	}
      else
	{
	  request_in (chan);
	  chan = findchan_by_dname (chname);
	  if (!chan)
	    return 0;
	  if (chan->need_key[0])
	    do_tcl ("need-key", chan->need_key);
	}
    }
  else
    putlog (LOG_JOIN, chname, IRC_BADCHANKEY, chname);
  return 0;
}
static int
gotinvite (char *from, char *msg)
{
  char *nick;
  struct chanset_t *chan;
  newsplit (&msg);
  fixcolon (msg);
  nick = splitnick (&from);
  if (!rfc_casecmp (last_invchan, msg))
    if (now - last_invtime < 30)
      return 0;
  putlog (LOG_MISC, "*", "%s!%s invited me to %s", nick, from, msg);
  strncpy (last_invchan, msg, 299);
  last_invchan[299] = 0;
  last_invtime = now;
  chan = findchan (msg);
  if (!chan)
    chan = findchan_by_dname (msg);
  if (chan && (channel_pending (chan) || channel_active (chan)))
    dprintf (DP_HELP, "NOTICE %s :I'm already here.\n", nick);
  else if (chan && !channel_inactive (chan))
    dprintf (DP_MODE, "JOIN %s %s\n",
	     (chan->name[0]) ? chan->name : chan->dname,
	     chan->channel.key[0] ? chan->channel.key : chan->key_prot);
  return 0;
}
static void
set_topic (struct chanset_t *chan, char *k)
{
  if (chan->channel.topic)
    nfree (chan->channel.topic);
  if (k && k[0])
    {
      chan->channel.topic = (char *) channel_malloc (strlen (k) + 1);
      strcpy (chan->channel.topic, k);
    }
  else
    chan->channel.topic = NULL;
}
static int
gottopic (char *from, char *msg)
{
  char *nick, *chname;
  memberlist *m;
  struct chanset_t *chan;
  struct userrec *u;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
  chname = newsplit (&msg);
  fixcolon (msg);
  u = get_user_by_host (from);
  nick = splitnick (&from);
  chan = findchan (chname);
  get_user_flagrec (u, &fr, chname);
  if (chan)
    {
      putlog (LOG_JOIN, chan->dname, "Topic changed on %s by %s!%s: %s",
	      chan->dname, nick, from, msg);
      m = ismember (chan, nick);
      if (m != NULL)
	m->last = now;
      set_topic (chan, msg);
      check_tcl_topc (nick, from, u, chan->dname, msg);
      if (egg_strcasecmp (botname, nick) && !glob_master (fr)
	  && !chan_master (fr))
	check_topic (chan);
    }
  return 0;
}
static int
got331 (char *from, char *msg)
{
  char *chname;
  struct chanset_t *chan;
  newsplit (&msg);
  chname = newsplit (&msg);
  chan = findchan (chname);
  if (chan)
    {
      set_topic (chan, NULL);
      check_tcl_topc ("*", "*", NULL, chan->dname, "");
      check_topic (chan);
    }
  return 0;
}
static int
got332 (char *from, char *msg)
{
  struct chanset_t *chan;
  char *chname;
  newsplit (&msg);
  chname = newsplit (&msg);
  chan = findchan (chname);
  if (chan)
    {
      fixcolon (msg);
      set_topic (chan, msg);
      check_tcl_topc ("*", "*", NULL, chan->dname, msg);
      check_topic (chan);
    }
  return 0;
}
static int
gotjoin (char *from, char *chname)
{
  char *nick, *p, buf[UHOSTLEN], *uhost = buf;
  char *ch_dname = NULL;
  struct chanset_t *chan;
  memberlist *m;
  masklist *b;
  struct userrec *u;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
  fixcolon (chname);
  chan = findchan (chname);
  if (!chan && chname[0] == '!')
    {
      int l_chname = strlen (chname);
      if (l_chname > (CHANNEL_ID_LEN + 1))
	{
	  ch_dname = nmalloc (l_chname + 1);
	  if (ch_dname)
	    {
	      egg_snprintf (ch_dname, l_chname + 2, "!%s",
			    chname + (CHANNEL_ID_LEN + 1));
	      chan = findchan_by_dname (ch_dname);
	      if (!chan)
		{
		  chan = findchan_by_dname (chname);
		  if (chan)
		    {
		      chan->status |= CHAN_INACTIVE;
		      putlog (LOG_MISC, "*",
			      "Deactivated channel %s, because it uses "
			      "an ID channel-name. Use the descriptive name instead.",
			      chname);
		      dprintf (DP_SERVER, "PART %s\n", chname);
		      goto exit;
		    }
		}
	    }
	}
    }
  else if (!chan)
    {
      chan = findchan_by_dname (chname);
    }
  if (!chan || channel_inactive (chan))
    {
      strcpy (uhost, from);
      nick = splitnick (&uhost);
      if (match_my_nick (nick))
	{
	  putlog (LOG_MISC, "*", "joined %s but didn't want to!", chname);
	  dprintf (DP_MODE, "PART %s\n", chname);
	}
    }
  else if (!channel_pending (chan))
    {
      chan->status &= ~CHAN_STOP_CYCLE;
      strcpy (uhost, from);
      nick = splitnick (&uhost);
      detect_chan_flood (nick, uhost, from, chan, FLOOD_JOIN, NULL);
      chan = findchan (chname);
      if (!chan)
	{
	  if (ch_dname)
	    chan = findchan_by_dname (ch_dname);
	  else
	    chan = findchan_by_dname (chname);
	}
      if (!chan)
	goto exit;
      u = get_user_by_host (from);
      get_user_flagrec (u, &fr, chan->dname);
      if (!channel_active (chan) && !match_my_nick (nick))
	{
	  putlog (LOG_MISC, chan->dname,
		  "confused bot: guess I'm on %s and didn't realize it",
		  chan->dname);
	  chan->status |= CHAN_ACTIVE;
	  chan->status &= ~CHAN_PEND;
	  reset_chan_info (chan);
	}
      else
	{
	  m = ismember (chan, nick);
	  if (m && m->split && !egg_strcasecmp (m->userhost, uhost))
	    {
	      check_tcl_rejn (nick, uhost, u, chan->dname);
	      chan = findchan (chname);
	      if (!chan)
		{
		  if (ch_dname)
		    chan = findchan_by_dname (ch_dname);
		  else
		    chan = findchan_by_dname (chname);
		}
	      if (!chan)
		goto exit;
	      u = get_user_by_host (from);
	      m->split = 0;
	      m->last = now;
	      m->delay = 0L;
	      m->flags = (chan_hasop (m) ? WASOP : 0);
	      m->user = u;
	      set_handle_laston (chan->dname, u, now);
	      m->flags |= STOPWHO;
	      putlog (LOG_JOIN, chan->dname, "%s (%s) returned to %s.", nick,
		      uhost, chan->dname);
	    }
	  else
	    {
	      if (m)
		killmember (chan, nick);
	      m = newmember (chan);
	      m->joined = now;
	      m->split = 0L;
	      m->flags = 0;
	      m->last = now;
	      m->delay = 0L;
	      strcpy (m->nick, nick);
	      strcpy (m->userhost, uhost);
	      m->user = u;
	      m->flags |= STOPWHO;
	      check_tcl_join (nick, uhost, u, chan->dname);
	      chan = findchan (chname);
	      if (!chan)
		{
		  if (ch_dname)
		    chan = findchan_by_dname (ch_dname);
		  else
		    chan = findchan_by_dname (chname);
		}
	      if (!chan)
		goto exit;
	      u = m->user;
	      if (match_my_nick (nick))
		{
		  strncpy (chan->name, chname, 81);
		  chan->name[80] = 0;
		  chan->status &= ~CHAN_JUPED;
		  if (chname[0] == '!')
		    putlog (LOG_JOIN, chan->dname, "%s joined %s (%s)", nick,
			    chan->dname, chname);
		  else
		    putlog (LOG_JOIN, chan->dname, "%s joined %s.", nick,
			    chname);
		  if (!match_my_nick (chname))
		    reset_chan_info (chan);
		}
	      else
		{
		  putlog (LOG_JOIN, chan->dname, "%s (%s) joined %s.", nick,
			  uhost, chan->dname);
		  set_handle_laston (chan->dname, u, now);
		}
	    }
	  if (me_op (chan))
	    {
#ifdef S_IRCNET
	      if (u_match_mask (global_invites, from)
		  || u_match_mask (chan->invites, from))
		refresh_invite (chan, from);
	      if (!
		  (use_exempts
		   && (u_match_mask (global_exempts, from)
		       || u_match_mask (chan->exempts, from))))
		{
#else
	      if (1)
		{
#endif
		  if (channel_enforcebans (chan) && !chan_op (fr)
		      && !glob_op (fr) && !glob_friend (fr)
		      && !chan_friend (fr) && !chan_sentkick (m) &&
#ifdef S_IRCNET
		      !(use_exempts && isexempted (chan, from)) &&
#endif
		      me_op (chan))
		    {
		      for (b = chan->channel.ban; b->mask[0]; b = b->next)
			{
			  if (wild_match (b->mask, from))
			    {
			      Context;
			      dprintf (DP_SERVER, STR ("KICK %s %s :%s%s\n"),
				       chname, m->nick, bankickprefix,
				       kickreason (KICK_BANNED));
			      m->flags |= SENTKICK;
			      goto exit;
			    }
			}
		    }
		  if (u_match_mask (global_bans, from)
		      || u_match_mask (chan->bans, from))
		    {
		      refresh_ban_kick (chan, from, nick);
		    }
		  else if (!chan_sentkick (m)
			   && (glob_kick (fr) || chan_kick (fr))
			   && me_op (chan))
		    {
#ifdef S_IRCNET
		      check_exemptlist (chan, from);
#endif
		      quickban (chan, from);
		      Context;
		      p = get_user (&USERENTRY_COMMENT, m->user);
		      dprintf (DP_MODE, STR ("KICK %s %s :%s%s\n"), chname,
			       nick, bankickprefix, kickreason (KICK_KUSER));
		      m->flags |= SENTKICK;
		    }
		}
	    }
	}
    }
exit:if (ch_dname)
    nfree (ch_dname);
  return 0;
}
static int
gotpart (char *from, char *msg)
{
  char *nick, *chname;
  struct chanset_t *chan;
  struct userrec *u;
  chname = newsplit (&msg);
  fixcolon (chname);
  fixcolon (msg);
  chan = findchan (chname);
  if (chan && channel_inactive (chan))
    {
      clear_channel (chan, 1);
      chan->status &= ~(CHAN_ACTIVE | CHAN_PEND);
      return 0;
    }
  if (chan && !channel_pending (chan))
    {
      u = get_user_by_host (from);
      nick = splitnick (&from);
      if (!channel_active (chan))
	{
	  putlog (LOG_MISC, chan->dname,
		  "confused bot: guess I'm on %s and didn't realize it",
		  chan->dname);
	  chan->status |= CHAN_ACTIVE;
	  chan->status &= ~CHAN_PEND;
	  reset_chan_info (chan);
	}
      set_handle_laston (chan->dname, u, now);
      check_tcl_part (nick, from, u, chan->dname, msg);
      chan = findchan (chname);
      if (!chan)
	return 0;
      killmember (chan, nick);
      if (msg[0])
	putlog (LOG_JOIN, chan->dname, "%s (%s) left %s (%s).", nick, from,
		chan->dname, msg);
      else
	putlog (LOG_JOIN, chan->dname, "%s (%s) left %s.", nick, from,
		chan->dname);
      if (match_my_nick (nick))
	{
	  clear_channel (chan, 1);
	  chan->status &= ~(CHAN_ACTIVE | CHAN_PEND);
	  if (!channel_inactive (chan))
	    dprintf (DP_MODE, "JOIN %s %s\n",
		     (chan->name[0]) ? chan->name : chan->dname,
		     chan->channel.key[0] ? chan->channel.key : chan->
		     key_prot);
	}
      else
	check_lonely_channel (chan);
    }
  return 0;
}
static int
gotkick (char *from, char *origmsg)
{
  char *nick, *whodid, *chname, s1[UHOSTLEN], buf[UHOSTLEN], *uhost = buf;
  char buf2[511], *msg;
  memberlist *m;
  struct chanset_t *chan;
  struct userrec *u;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
  strncpy (buf2, origmsg, 510);
  buf2[510] = 0;
  msg = buf2;
  chname = newsplit (&msg);
  chan = findchan (chname);
  if (!chan)
    return 0;
  nick = newsplit (&msg);
  if (match_my_nick (nick) && channel_pending (chan))
    {
      chan->status &= ~(CHAN_ACTIVE | CHAN_PEND);
      dprintf (DP_MODE, "JOIN %s %s\n",
	       (chan->name[0]) ? chan->name : chan->dname,
	       chan->channel.key[0] ? chan->channel.key : chan->key_prot);
      clear_channel (chan, 1);
      return 0;
    }
  if (channel_active (chan))
    {
      fixcolon (msg);
      u = get_user_by_host (from);
      strcpy (uhost, from);
      whodid = splitnick (&uhost);
      detect_chan_flood (whodid, uhost, from, chan, FLOOD_KICK, nick);
      chan = findchan (chname);
      if (!chan)
	return 0;
      m = ismember (chan, whodid);
      if (m)
	m->last = now;
      get_user_flagrec (u, &fr, chan->dname);
      set_handle_laston (chan->dname, u, now);
      check_tcl_kick (whodid, uhost, u, chan->dname, nick, msg);
      chan = findchan (chname);
      if (!chan)
	return 0;
      m = ismember (chan, nick);
      if (m)
	{
	  struct userrec *u2;
	  simple_sprintf (s1, "%s!%s", m->nick, m->userhost);
	  u2 = get_user_by_host (s1);
	  set_handle_laston (chan->dname, u2, now);
	  maybe_revenge (chan, from, s1, REVENGE_KICK);
	}
      putlog (LOG_MODES, chan->dname, "%s kicked from %s by %s: %s", s1,
	      chan->dname, from, msg);
      if (match_my_nick (nick))
	{
	  chan->status &= ~(CHAN_ACTIVE | CHAN_PEND);
	  dprintf (DP_MODE, "JOIN %s %s\n",
		   (chan->name[0]) ? chan->name : chan->dname,
		   chan->channel.key[0] ? chan->channel.key : chan->key_prot);
	  clear_channel (chan, 1);
	}
      else
	{
	  killmember (chan, nick);
	  check_lonely_channel (chan);
	}
    }
  return 0;
}
static int
gotnick (char *from, char *msg)
{
  char *nick, *chname, s1[UHOSTLEN], buf[UHOSTLEN], *uhost = buf;
  memberlist *m, *mm;
  struct chanset_t *chan, *oldchan = NULL;
  struct userrec *u;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
  strcpy (uhost, from);
  nick = splitnick (&uhost);
  fixcolon (msg);
  clear_chanlist_member (nick);
  for (chan = chanset; chan; chan = chan->next)
    {
      oldchan = chan;
      chname = chan->dname;
      m = ismember (chan, nick);
      if (m)
	{
	  putlog (LOG_JOIN, chan->dname, "Nick change: %s -> %s", nick, msg);
	  m->last = now;
	  if (rfc_casecmp (nick, msg))
	    {
	      mm = ismember (chan, msg);
	      if (mm)
		{
		  if (mm->split)
		    putlog (LOG_JOIN, chan->dname,
			    "Possible future nick collision: %s", mm->nick);
		  else
		    putlog (LOG_MISC, chan->dname,
			    "* Bug: nick change to existing nick");
		  killmember (chan, mm->nick);
		}
	    }
	  sprintf (s1, "%s!%s", msg, uhost);
	  strcpy (m->nick, msg);
	  detect_chan_flood (msg, uhost, from, chan, FLOOD_NICK, NULL);
	  if (!findchan_by_dname (chname))
	    {
	      chan = oldchan;
	      continue;
	    }
	  if (chan_sentkick (m) || chan_sentdeop (m) || chan_sentop (m)
	      || chan_sentdevoice (m) || chan_sentvoice (m))
	    m->flags |= STOPCHECK;
	  m->flags &=
	    ~(SENTKICK | SENTDEOP | SENTOP | SENTVOICE | SENTDEVOICE);
	  if (!chan_stopcheck (m))
	    {
	      get_user_flagrec (m->user ? m->user : get_user_by_host (s1),
				&fr, chan->dname);
	      check_this_member (chan, m->nick, &fr);
	    }
	  u = get_user_by_host (from);
	  check_tcl_nick (nick, uhost, u, chan->dname, msg);
	  if (!findchan_by_dname (chname))
	    {
	      chan = oldchan;
	      continue;
	    }
	}
    }
  return 0;
}
static int
gotquit (char *from, char *msg)
{
  char *nick, *chname, *p, *alt;
  int split = 0;
  char from2[NICKMAX + UHOSTMAX + 1];
  memberlist *m;
  struct chanset_t *chan, *oldchan = NULL;
  struct userrec *u;
  strcpy (from2, from);
  u = get_user_by_host (from2);
  nick = splitnick (&from);
  fixcolon (msg);
  p = strchr (msg, ' ');
  if (p && (p == strrchr (msg, ' ')))
    {
      char *z1, *z2;
      *p = 0;
      z1 = strchr (p + 1, '.');
      z2 = strchr (msg, '.');
      if (z1 && z2 && (*(z1 + 1) != 0) && (z1 - 1 != p) && (z2 + 1 != p)
	  && (z2 != msg))
	{
	  split = 1;
	}
      else
	*p = ' ';
    }
  for (chan = chanset; chan; chan = chan->next)
    {
      oldchan = chan;
      chname = chan->dname;
      m = ismember (chan, nick);
      if (m)
	{
	  u = get_user_by_host (from2);
	  if (u)
	    {
	      set_handle_laston (chan->dname, u, now);
	    }
	  if (split)
	    {
	      m->split = now;
	      check_tcl_splt (nick, from, u, chan->dname);
	      if (!findchan_by_dname (chname))
		{
		  chan = oldchan;
		  continue;
		}
	      putlog (LOG_JOIN, chan->dname, "%s (%s) got netsplit.", nick,
		      from);
	    }
	  else
	    {
	      check_tcl_sign (nick, from, u, chan->dname, msg);
	      if (!findchan_by_dname (chname))
		{
		  chan = oldchan;
		  continue;
		}
	      putlog (LOG_JOIN, chan->dname, "%s (%s) left irc: %s", nick,
		      from, msg);
	      killmember (chan, nick);
	      check_lonely_channel (chan);
	    }
	}
    }
  if (keepnick)
    {
      alt = get_altbotnick ();
      if (!rfc_casecmp (nick, origbotname))
	{
	  putlog (LOG_MISC, "*", IRC_GETORIGNICK, origbotname);
	  dprintf (DP_SERVER, "NICK %s\n", origbotname);
	}
      else if (alt[0])
	{
	  if (!rfc_casecmp (nick, alt) && strcmp (botname, origbotname))
	    {
	      putlog (LOG_MISC, "*", IRC_GETALTNICK, alt);
	      dprintf (DP_SERVER, "NICK %s\n", alt);
	    }
	}
    }
  return 0;
}
static int
gotmsg (char *from, char *msg)
{
  char *to, *realto, buf[UHOSTLEN], *nick, buf2[512], *uhost = buf;
  char *p, *p1, *code, *ctcp;
  int ctcp_count = 0;
  struct chanset_t *chan;
  int ignoring;
  struct userrec *u;
  memberlist *m;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
  if (!strchr ("&#!+@$", msg[0]))
    return 0;
  ignoring = match_ignore (from);
  to = newsplit (&msg);
  realto = (to[0] == '@') ? to + 1 : to;
  chan = findchan (realto);
  if (!chan)
    return 0;
  fixcolon (msg);
  strcpy (uhost, from);
  nick = splitnick (&uhost);
  if (flud_ctcp_thr && detect_avalanche (msg))
    {
      u = get_user_by_host (from);
      get_user_flagrec (u, &fr, chan->dname);
      m = ismember (chan, nick);
      if (me_op (chan) && m && !chan_sentkick (m) && !chan_friend (fr)
	  && !glob_friend (fr) && !(channel_dontkickops (chan)
				    && (chan_op (fr)
					|| (glob_op (fr) && !chan_deop (fr))))
	  && !(use_exempts && ban_fun
	       && (u_match_mask (global_exempts, from)
		   || u_match_mask (chan->exempts, from))))
	{
	  if (ban_fun)
	    {
	      check_exemptlist (chan, from);
	      u_addban (chan, quickban (chan, uhost), botnetnick, IRC_FUNKICK,
			now + (60 * chan->ban_time), 0);
	    }
	  if (kick_fun)
	    {
	      dprintf (DP_SERVER, "KICK %s %s :%s\n", chan->name, nick,
		       IRC_FUNKICK);
	      m->flags |= SENTKICK;
	    }
	}
      if (!ignoring)
	{
	  putlog (LOG_MODES, "*", "Avalanche from %s!%s in %s - ignoring",
		  nick, uhost, chan->dname);
	  p = strchr (uhost, '@');
	  if (p)
	    p++;
	  else
	    p = uhost;
	  simple_sprintf (buf2, "*!*@%s", p);
	  addignore (buf2, botnetnick, "ctcp avalanche",
		     now + (60 * ignore_time));
	}
      return 0;
    }
  ctcp_reply[0] = 0;
  p = strchr (msg, 1);
  while (p && *p)
    {
      p++;
      p1 = p;
      while ((*p != 1) && *p)
	p++;
      if (*p == 1)
	{
	  *p = 0;
	  ctcp = buf2;
	  strcpy (ctcp, p1);
	  strcpy (p1 - 1, p + 1);
	  detect_chan_flood (nick, uhost, from, chan,
			     strncmp (ctcp, "ACTION ",
				      7) ? FLOOD_CTCP : FLOOD_PRIVMSG, NULL);
	  chan = findchan (realto);
	  if (!chan)
	    return 0;
	  p = strchr (msg, 1);
	  if (ctcp_count < answer_ctcp)
	    {
	      ctcp_count++;
	      if (ctcp[0] != ' ')
		{
		  code = newsplit (&ctcp);
		  u = get_user_by_host (from);
		  if (!ignoring || trigger_on_ignore)
		    {
		      if (!check_tcl_ctcp (nick, uhost, u, to, code, ctcp))
			{
			  chan = findchan (realto);
			  if (!chan)
			    return 0;
			  update_idle (chan->dname, nick);
			}
		      if (!ignoring)
			{
			  if (!strcmp (code, "ACTION"))
			    {
			      putlog (LOG_PUBLIC, chan->dname,
				      "Action: %s %s", nick, ctcp);
			    }
			  else
			    {
			      putlog (LOG_PUBLIC, chan->dname,
				      "CTCP %s: %s from %s (%s) to %s", code,
				      ctcp, nick, from, to);
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
      detect_chan_flood (nick, uhost, from, chan, FLOOD_PRIVMSG, NULL);
      chan = findchan (realto);
      if (!chan)
	return 0;
      if (!ignoring || trigger_on_ignore)
	{
	  if (check_tcl_pub (nick, uhost, chan->dname, msg))
	    return 0;
	  check_tcl_pubm (nick, uhost, chan->dname, msg);
	  chan = findchan (realto);
	  if (!chan)
	    return 0;
	}
      if (!ignoring)
	{
	  if (to[0] == '@')
	    putlog (LOG_PUBLIC, chan->dname, "@<%s> %s", nick, msg);
	  else
	    putlog (LOG_PUBLIC, chan->dname, "<%s> %s", nick, msg);
	}
      update_idle (chan->dname, nick);
    }
  return 0;
}
static int
gotnotice (char *from, char *msg)
{
  char *to, *realto, *nick, buf2[512], *p, *p1, buf[512], *uhost = buf;
  char *ctcp, *code;
  struct userrec *u;
  memberlist *m;
  struct chanset_t *chan;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };
  int ignoring;
  if (!strchr (CHANMETA "@", *msg))
    return 0;
  ignoring = match_ignore (from);
  to = newsplit (&msg);
  realto = (*to == '@') ? to + 1 : to;
  chan = findchan (realto);
  if (!chan)
    return 0;
  fixcolon (msg);
  strcpy (uhost, from);
  nick = splitnick (&uhost);
  u = get_user_by_host (from);
  if (flud_ctcp_thr && detect_avalanche (msg))
    {
      get_user_flagrec (u, &fr, chan->dname);
      m = ismember (chan, nick);
      if (me_op (chan) && m && !chan_sentkick (m) && !chan_friend (fr)
	  && !glob_friend (fr) && !(channel_dontkickops (chan)
				    && (chan_op (fr)
					|| (glob_op (fr) && !chan_deop (fr))))
	  && !(use_exempts && ban_fun
	       && (u_match_mask (global_exempts, from)
		   || u_match_mask (chan->exempts, from))))
	{
	  if (ban_fun)
	    {
	      check_exemptlist (chan, from);
	      u_addban (chan, quickban (chan, uhost), botnetnick, IRC_FUNKICK,
			now + (60 * chan->ban_time), 0);
	    }
	  if (kick_fun)
	    {
	      dprintf (DP_SERVER, "KICK %s %s :%s\n", chan->name, nick,
		       IRC_FUNKICK);
	      m->flags |= SENTKICK;
	    }
	}
      if (!ignoring)
	putlog (LOG_MODES, "*", "Avalanche from %s", from);
      return 0;
    }
  p = strchr (msg, 1);
  while (p && *p)
    {
      p++;
      p1 = p;
      while ((*p != 1) && *p)
	p++;
      if (*p == 1)
	{
	  *p = 0;
	  ctcp = buf2;
	  strcpy (ctcp, p1);
	  strcpy (p1 - 1, p + 1);
	  p = strchr (msg, 1);
	  detect_chan_flood (nick, uhost, from, chan,
			     strncmp (ctcp, "ACTION ",
				      7) ? FLOOD_CTCP : FLOOD_PRIVMSG, NULL);
	  chan = findchan (realto);
	  if (!chan)
	    return 0;
	  if (ctcp[0] != ' ')
	    {
	      code = newsplit (&ctcp);
	      if (!ignoring || trigger_on_ignore)
		{
		  check_tcl_ctcr (nick, uhost, u, chan->dname, code, msg);
		  chan = findchan (realto);
		  if (!chan)
		    return 0;
		  if (!ignoring)
		    {
		      putlog (LOG_PUBLIC, chan->dname,
			      "CTCP reply %s: %s from %s (%s) to %s", code,
			      msg, nick, from, chan->dname);
		      update_idle (chan->dname, nick);
		    }
		}
	    }
	}
    }
  if (msg[0])
    {
      detect_chan_flood (nick, uhost, from, chan, FLOOD_NOTICE, NULL);
      chan = findchan (realto);
      if (!chan)
	return 0;
      if (!ignoring || trigger_on_ignore)
	{
	  check_tcl_notc (nick, uhost, u, to, msg);
	  chan = findchan (realto);
	  if (!chan)
	    return 0;
	}
      if (!ignoring)
	putlog (LOG_PUBLIC, chan->dname, "-%s:%s- %s", nick, to, msg);
      update_idle (chan->dname, nick);
    }
  return 0;
}
static cmd_t irc_raw[] =
  { {"324", "", (Function) got324, "irc:324"}, {"352", "", (Function) got352,
						"irc:352"}, {"354", "",
							     (Function)
							     got354,
							     "irc:354"},
  {"315", "", (Function) got315, "irc:315"}, {"367", "", (Function) got367,
					      "irc:367"}, {"368", "",
							   (Function) got368,
							   "irc:368"}, {"403",
									"",
									(Function)
									got403,
									"irc:403"},
  {"405", "", (Function) got405, "irc:405"}, {"471", "", (Function) got471,
					      "irc:471"}, {"473", "",
							   (Function) got473,
							   "irc:473"}, {"474",
									"",
									(Function)
									got474,
									"irc:474"},
  {"475", "", (Function) got475, "irc:475"}, {"INVITE", "",
					      (Function) gotinvite,
					      "irc:invite"}, {"TOPIC", "",
							      (Function)
							      gottopic,
							      "irc:topic"},
  {"331", "", (Function) got331, "irc:331"}, {"332", "", (Function) got332,
					      "irc:332"}, {"JOIN", "",
							   (Function) gotjoin,
							   "irc:join"},
  {"PART", "", (Function) gotpart, "irc:part"}, {"KICK", "",
						 (Function) gotkick,
						 "irc:kick"}, {"NICK", "",
							       (Function)
							       gotnick,
							       "irc:nick"},
  {"QUIT", "", (Function) gotquit, "irc:quit"}, {"PRIVMSG", "",
						 (Function) gotmsg,
						 "irc:msg"}, {"NOTICE", "",
							      (Function)
							      gotnotice,
							      "irc:notice"},
  {"MODE", "", (Function) gotmode, "irc:mode"},
#ifdef S_IRCNET
{"346", "", (Function) got346, "irc:346"}, {"347", "", (Function) got347,
					    "irc:347"}, {"348", "",
							 (Function) got348,
							 "irc:348"}, {"349",
								      "",
								      (Function)
								      got349,
								      "irc:349"},
#endif
{NULL, NULL, NULL, NULL}
};
#endif
