#ifdef LEAF
/*
 *
 * chan.c -- part of irc.mod
 *   almost everything to do with channel manipulation
 *   telling channel status
 *   'who' response
 *   user kickban, kick, op, deop
 *   idle kicking
 *
 */

static time_t last_ctcp = (time_t) 0L;
static int    count_ctcp = 0;
static time_t last_invtime = (time_t) 0L;
static char   last_invchan[300] = "";

/* ID length for !channels.
 */
#define CHANNEL_ID_LEN 5

/* Returns a pointer to a new channel member structure.
 */
static memberlist *newmember(struct chanset_t *chan, char * nick)
{
  memberlist *x = NULL, *lx = NULL, *n = NULL;

  x = chan->channel.member;
  lx = NULL;
  while (x && x->nick[0] && (rfc_casecmp(x->nick, nick)<0)) {
    lx = x;
    x = x->next;
  }
  n = (memberlist *) calloc(1, sizeof(memberlist));
  n->next = NULL;
  strncpyz(n->nick, nick, sizeof(n->nick));
  n->split = 0L;
  n->last = 0L;
  n->delay = 0L;
  n->hops = -1;
  if (!lx) {
    n->next = chan->channel.member;
    chan->channel.member=n;
  } else {
    n->next = lx->next;
    lx->next = n;
  }
  chan->channel.members++;
  return n;
}

/* Always pass the channel dname (display name) to this function <cybah>
 */
static void update_idle(char *chname, char *nick)
{
  memberlist *m = NULL;
  struct chanset_t *chan = NULL;

  chan = findchan_by_dname(chname);
  if (chan) {
    m = ismember(chan, nick);
    if (m)
      m->last = now;
  }
}

/* Returns the current channel mode.
 */
static char *getchanmode(struct chanset_t *chan)
{
  static char s[121] = "";
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
    i += sprintf(s + i, " %s", chan->channel.key);
  if (chan->channel.maxmembers != 0)
    sprintf(s + i, " %d", chan->channel.maxmembers);
  return s;
}

static void check_exemptlist(struct chanset_t *chan, char *from)
{
  masklist *e = NULL;
  int ok = 0;

  if (!use_exempts)
    return;

  for (e = chan->channel.exempt; e->mask[0]; e = e->next)
    if (wild_match(e->mask, from)) {
      add_mode(chan, '-', 'e', e->mask);
      ok = 1;
    }
  if (prevent_mixing && ok)
    flush_mode(chan, QUICK);
}

void priority_do(struct chanset_t * chan, int opsonly, int action) 
{
  memberlist *m = NULL;
  int ops = 0, targets = 0, bpos = 0, tpos = 0, ft = 0, ct = 0, actions = 0, sent = 0;

  if (!me_op(chan))
    return;
  if (channel_pending(chan) || !shouldjoin(chan) || !channel_active(chan))
    return;
  for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
    if (!m->user) {
      char s[256] = "";

      sprintf(s, "%s!%s", m->nick, m->userhost);
      m->user = get_user_by_host(s);
    }


    if (m->user && ((m->user->flags & (USER_BOT | USER_OP)) == (USER_BOT | USER_OP))) {
      ops++;
      if (!strcmp(m->nick, botname))
        bpos = (ops - 1);

    } else if (!opsonly || chan_hasop(m)) {
        struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0};
        if (m->user)
          get_user_flagrec(m->user, &fr, chan->dname);

        if (((glob_deop(fr) && !chan_op(fr)) || chan_deop(fr)) || /* +d */
           ((!channel_private(chan) && !chan_op(fr) && !glob_op(fr)) || /* simply no +o flag. */
           (channel_private(chan) && !glob_bot(fr) && !glob_owner(fr) && !chan_op(fr)))) { /* private? */
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
  for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
    if (!opsonly || chan_hasop(m)) {
      struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0};

      if (m->user)
        get_user_flagrec(m->user, &fr, chan->dname);
 
      if (((glob_deop(fr) && !chan_op(fr)) || chan_deop(fr)) ||
          ((!channel_private(chan) && !chan_op(fr) && !glob_op(fr)) ||
           (channel_private(chan) && !glob_bot(fr) && !glob_owner(fr) && !chan_op(fr)))) {

        if (tpos >= ft) {
          if ((action == PRIO_DEOP) && !chan_sentdeop(m)) {
            actions++;
            sent++;
            add_mode(chan, '-', 'o', m->nick);
            if (actions >= ct) {
              flush_mode(chan, QUICK);
              return;
            }
          } else if ((action == PRIO_KICK) && !chan_sentkick(m)) {
            actions++;
            sent++;
            if (chan->closed_ban)
              doban(chan, m);
            dprintf(DP_MODE, "KICK %s %s :%s%s\n", chan->name, m->nick, kickprefix, kickreason(KICK_CLOSED));
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
  for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
    if (!opsonly || chan_hasop(m)) {
      struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0};

      if (m->user)
        get_user_flagrec(m->user, &fr, chan->dname);

      if (((glob_deop(fr) && !chan_op(fr)) || chan_deop(fr)) ||
          ((!channel_private(chan) && !chan_op(fr) && !glob_op(fr)) || 
           (channel_private(chan) && !glob_bot(fr) && !glob_owner(fr) && !chan_op(fr)))) {

        if (tpos >= ft) {
          if ((action == PRIO_DEOP) && !chan_sentdeop(m)) {
            actions++;
            sent++;
            add_mode(chan, '-', 'o', m->nick);
            if ((actions >= ct) || (sent > 20)) {
              flush_mode(chan, QUICK);
              return;
            }
          } else if ((action == PRIO_KICK) && !chan_sentkick(m)) {
            actions++;
            if (chan->closed_ban)
              doban(chan, m);
            dprintf(DP_MODE, "KICK %s %s :%s%s\n", chan->name, m->nick, kickprefix, kickreason(KICK_CLOSED));
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

/* lame code 
static int target_priority(struct chanset_t * chan, memberlist *target, int opsonly) 
{
  memberlist *m;
  int ops = 0, targets = 0, bpos = 0, ft = 0, ct = 0, tp = (-1), pos = 0;

  return 1;
  for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
    if (m->user && ((m->user->flags & (USER_BOT | USER_OP)) == (USER_BOT | USER_OP))) {
      ops++;
      if (match_my_nick(m->nick))
        bpos = ops;
    } else if (!opsonly || chan_hasop(m)) {
      struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0};

      if (m->user)
        get_user_flagrec(m->user, &fr, chan->dname);

      if (((glob_deop(fr) && !chan_op(fr)) || chan_deop(fr)) ||
         ((!channel_private(chan) && !chan_op(fr) && !glob_op(fr)) || 
         (channel_private(chan) && !glob_bot(fr) && !glob_owner(fr) && !chan_op(fr)))) { 
        targets++;
      }
    }
    if (m == target)
      tp = pos;
    pos++;
  }
  if (!targets || !ops || (tp < 0)) {
    return 0;
  }
  ft = (bpos * targets) / ops;
  ct = ((bpos + 2) * targets + (ops - 1)) / ops;
  ct = (ct - ft + 1);
  if (ct > 20)
    ct = 20;
  while (ft >= targets) {
    ft -= targets;
  }
  if (ct >= targets) {
    putlog(LOG_MISC, "*", "%s 1 ct >= targets; ct %d targets %d", target, ct, targets);
    if ((tp >= ft) || (tp <= (ct % targets))) {
      putlog(LOG_MISC, "*", "%s (1) first if, tp %d ft %d ct/targets %d", target, tp, ft, (ct % targets));
      return 1;
    }
  } else {
    putlog(LOG_MISC, "*", "%s 2 else, ct %d targets %d", target, ct, targets);
    if ((tp >= ft) && (tp <= ct)) {
      putlog(LOG_MISC, "*", "%s (1) second if, tp %d ft %d", target, tp, ft);
      return 1;
    }
  }
  putlog(LOG_MISC, "*", "%s (0) returning 0", target);
  return 0;
}
*/

/* Check a channel and clean-out any more-specific matching masks.
 *
 * Moved all do_ban(), do_exempt() and do_invite() into this single function
 * as the code bloat is starting to get rediculous <cybah>
 */
static void do_mask(struct chanset_t *chan, masklist *m, char *mask, char Mode)
{
  for (; m && m->mask[0]; m = m->next)
    if (wild_match(mask, m->mask) && rfc_casecmp(mask, m->mask))
      add_mode(chan, '-', Mode, m->mask);
  add_mode(chan, '+', Mode, mask);
  flush_mode(chan, QUICK);
}

/* This is a clone of detect_flood, but works for channel specificity now
 * and handles kick & deop as well.
 */
static int detect_chan_flood(char *floodnick, char *floodhost, char *from,
			     struct chanset_t *chan, int which, char *victim)
{
  char h[UHOSTLEN] = "", ftype[12] = "", *p = NULL;
  struct userrec *u = NULL;
  memberlist *m = NULL;
  int thr = 0, lapse = 0;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0};

  if (!chan || (which < 0) || (which >= FLOOD_CHAN_MAX))
    return 0;
  m = ismember(chan, floodnick);
  /* Do not punish non-existant channel members and IRC services like
   * ChanServ
   */
  if (!m && (which != FLOOD_JOIN))
    return 0;

  get_user_flagrec(get_user_by_host(from), &fr, chan->dname);
  if (glob_bot(fr) ||
      ((which == FLOOD_DEOP) &&
       (glob_master(fr) || chan_master(fr))) ||
      ((which == FLOOD_KICK) &&
       (glob_master(fr) || chan_master(fr))) ||
      ((which != FLOOD_DEOP) && (which != FLOOD_KICK) && 
       (glob_noflood(fr) || chan_noflood(fr))))
    return 0;

  /* Determine how many are necessary to make a flood. */
  switch (which) {
  case FLOOD_PRIVMSG:
  case FLOOD_NOTICE:
    thr = chan->flood_pub_thr;
    lapse = chan->flood_pub_time;
    strcpy(ftype, "pub");
    break;
  case FLOOD_CTCP:
    thr = chan->flood_ctcp_thr;
    lapse = chan->flood_ctcp_time;
    strcpy(ftype, "pub");
    break;
  case FLOOD_NICK:
    thr = chan->flood_nick_thr;
    lapse = chan->flood_nick_time;
    strcpy(ftype, "nick");
    break;
  case FLOOD_JOIN:
    thr = chan->flood_join_thr;
    lapse = chan->flood_join_time;
      strcpy(ftype, "join");
    break;
  case FLOOD_DEOP:
    thr = chan->flood_deop_thr;
    lapse = chan->flood_deop_time;
    strcpy(ftype, "deop");
    break;
  case FLOOD_KICK:
    thr = chan->flood_kick_thr;
    lapse = chan->flood_kick_time;
    strcpy(ftype, "kick");
    break;
  }
  if ((thr == 0) || (lapse == 0))
    return 0;			/* no flood protection */
  /* Okay, make sure i'm not flood-checking myself */
  if (match_my_nick(floodnick))
    return 0;
  if (!egg_strcasecmp(floodhost, botuserhost))
    return 0;
  /* My user@host (?) */
  if ((which == FLOOD_KICK) || (which == FLOOD_DEOP))
    p = floodnick;
  else {
    p = strchr(floodhost, '@');
    if (p) {
      p++;
    }
    if (!p)
      return 0;
  }
  if (rfc_casecmp(chan->floodwho[which], p)) {	/* new */
    strncpy(chan->floodwho[which], p, 81);
    chan->floodwho[which][81] = 0;
    chan->floodtime[which] = now;
    chan->floodnum[which] = 1;
    return 0;
  }
  if (chan->floodtime[which] < now - lapse) {
    /* Flood timer expired, reset it */
    chan->floodtime[which] = now;
    chan->floodnum[which] = 1;
    return 0;
  }
  /* Deop'n the same person, sillyness ;) - so just ignore it */
  if (which == FLOOD_DEOP) {
    if (!rfc_casecmp(chan->deopd, victim))
      return 0;
    else
      strcpy(chan->deopd, victim);
  }
  chan->floodnum[which]++;
  if (chan->floodnum[which] >= thr) {	/* FLOOD */
    /* Reset counters */
    chan->floodnum[which] = 0;
    chan->floodtime[which] = 0;
    chan->floodwho[which][0] = 0;
    if (which == FLOOD_DEOP)
      chan->deopd[0] = 0;
    u = get_user_by_host(from);
    switch (which) {
    case FLOOD_PRIVMSG:
    case FLOOD_NOTICE:
    case FLOOD_CTCP:
      /* Flooding chan! either by public or notice */
      if (!chan_sentkick(m) && me_op(chan)) {
	putlog(LOG_MODES, chan->dname, IRC_FLOODKICK, floodnick);
        dprintf(DP_MODE, "KICK %s %s :%s%s\n", chan->name, floodnick, kickprefix, kickreason(KICK_FLOOD));
	m->flags |= SENTKICK;
      }
      return 1;
    case FLOOD_JOIN:
    case FLOOD_NICK:
      if (use_exempts &&
	  (u_match_mask(global_exempts, from) ||
	   u_match_mask(chan->exempts, from)))
	return 1;
      simple_sprintf(h, "*!*@%s", p);
      if (!isbanned(chan, h) && me_op(chan)) {
	check_exemptlist(chan, from);
	do_mask(chan, chan->channel.ban, h, 'b');
      }
      if ((u_match_mask(global_bans, from))
	  || (u_match_mask(chan->bans, from)))
	return 1;		/* Already banned */
      if (which == FLOOD_JOIN)
	putlog(LOG_MISC | LOG_JOIN, chan->dname, IRC_FLOODIGNORE3, p);
      else
	putlog(LOG_MISC | LOG_JOIN, chan->dname, IRC_FLOODIGNORE4, p);
      strcpy(ftype + 4, " flood");
      u_addmask('b', chan, h, conf.bot->nick, ftype, now + (60 * chan->ban_time), 0);
      if (!channel_enforcebans(chan) && me_op(chan)) {
	  char s[UHOSTLEN];
	  for (m = chan->channel.member; m && m->nick[0]; m = m->next) {	  
	    sprintf(s, "%s!%s", m->nick, m->userhost);
	    if (wild_match(h, s) &&
		(m->joined >= chan->floodtime[which]) &&
		!chan_sentkick(m) && !match_my_nick(m->nick) && me_op(chan)) {
	      m->flags |= SENTKICK;
	      if (which == FLOOD_JOIN)
   	        dprintf(DP_SERVER, "KICK %s %s :%s%s\n", chan->name, m->nick,
		      kickprefix, IRC_JOIN_FLOOD);
	      else
                dprintf(DP_SERVER, "KICK %s %s :%s%s\n", chan->name, m->nick, kickprefix, kickreason(KICK_NICKFLOOD));
	    }
	  }
	}
      return 1;
    case FLOOD_KICK:
      if (me_op(chan) && !chan_sentkick(m)) {
	putlog(LOG_MODES, chan->dname, "Kicking %s, for mass kick.", floodnick);
        dprintf(DP_MODE, "KICK %s %s :%s%s\n", chan->name, floodnick, kickprefix, kickreason(KICK_KICKFLOOD));
	m->flags |= SENTKICK;
      }
    return 1;
    case FLOOD_DEOP:
      if (me_op(chan) && !chan_sentkick(m)) {
	putlog(LOG_MODES, chan->dname,
	       CHAN_MASSDEOP, chan->dname, from);
        dprintf(DP_MODE, "KICK %s %s :%s%s\n", chan->name, floodnick, kickprefix, kickreason(KICK_MASSDEOP));
	m->flags |= SENTKICK;
      }
      if (u) {
        char s[256] = "";

        sprintf(s, "Mass deop on %s by %s", chan->dname, from);
        deflag_user(u, DEFLAG_MDOP, s, chan);
      }
      return 1;
    }
  }
  return 0;
}

/* Given a chan/m do all necesary exempt checks and ban. */
static void refresh_ban_kick(struct chanset_t *, char *, char *);
static void doban(struct chanset_t *chan, memberlist *m)
{
  char s[UHOSTLEN] = "", *s1 = NULL;

  if (!chan || !m) return;

  sprintf(s, "%s!%s", m->nick, m->userhost);

  if (!(use_exempts &&
        (u_match_mask(global_exempts,s) ||
         u_match_mask(chan->exempts, s)))) {
    if (u_match_mask(global_bans, s) || u_match_mask(chan->bans, s))
      refresh_ban_kick(chan, s, m->nick);

    check_exemptlist(chan, s);
    s1 = quickban(chan, m->userhost);
    u_addmask('b', chan, s1, conf.bot->nick, "joined closed chan", now + (60 * chan->ban_time), 0);
  }
  return;
}

/* Given a [nick!]user@host, place a quick ban on them on a chan.
 */
static char *quickban(struct chanset_t *chan, char *uhost)
{
  static char s1[512] = "";

  maskhost(uhost, s1);
  if ((strlen(s1) != 1) && (strict_host == 0))
    s1[2] = '*';		/* arthur2 */
  do_mask(chan, chan->channel.ban, s1, 'b');
  return s1;
}

/* Kick any user (except friends/masters) with certain mask from channel
 * with a specified comment.  Ernst 18/3/1998
 */
static void kick_all(struct chanset_t *chan, char *hostmask, char *comment, int bantype)
{
  memberlist *m = NULL;
  char kicknick[512] = "", s[UHOSTLEN] = "";
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0};
  int k, l, flushed;

  if (!me_op(chan))
    return;
  k = 0;
  flushed = 0;
  kicknick[0] = 0;
  for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
    sprintf(s, "%s!%s", m->nick, m->userhost);
    get_user_flagrec(m->user ? m->user : get_user_by_host(s), &fr, chan->dname);
    if (me_op(chan) &&
	wild_match(hostmask, s) && !chan_sentkick(m) &&
	!match_my_nick(m->nick) && !chan_issplit(m) &&
	!(use_exempts &&
	  ((bantype && (isexempted(chan, s) || (chan->ircnet_status & CHAN_ASKED_EXEMPTS))) ||
	   (u_match_mask(global_exempts, s) ||
	    u_match_mask(chan->exempts, s))))) {
      if (!flushed) {
	/* We need to kick someone, flush eventual bans first */
	flush_mode(chan, QUICK);
	flushed += 1;
      }
      m->flags |= SENTKICK;	/* Mark as pending kick */
      if (kicknick[0])
	strcat(kicknick, ",");
      strcat(kicknick, m->nick);
      k += 1;
      l = strlen(chan->name) + strlen(kicknick) + strlen(comment) + 5;
      if ((kick_method != 0 && k == kick_method) || (l > 480)) {
	dprintf(DP_MODE, "KICK %s %s :%s%s\n", chan->name, kicknick, kickprefix, comment);
	k = 0;
	kicknick[0] = 0;
      }
    }
  }
  if (k > 0) {
    dprintf(DP_MODE, "KICK %s %s :%s%s\n", chan->name, kicknick, kickprefix, comment);
  }
}

/* If any bans match this wildcard expression, refresh them on the channel.
 */
static void refresh_ban_kick(struct chanset_t *chan, char *user, char *nick)
{
  register maskrec *b = NULL;
  memberlist *m = NULL;
  int cycle;

  m = ismember(chan, nick);
  if (!m || chan_sentkick(m))
    return;
  /* Check global bans in first cycle and channel bans
     in second cycle. */
  for (cycle = 0; cycle < 2; cycle++) {
    for (b = cycle ? chan->bans : global_bans; b; b = b->next) {
      if (wild_match(b->mask, user)) {
	struct flag_record	fr = {FR_GLOBAL | FR_CHAN, 0, 0};
	char c[512] = "";		/* The ban comment.	*/
	char s[UHOSTLEN] = "";

	sprintf(s, "%s!%s", m->nick, m->userhost);
	get_user_flagrec(m->user ? m->user : get_user_by_host(s), &fr,
			 chan->dname);
        if (role == 1)
  	  add_mode(chan, '-', 'o', nick);	/* Guess it can't hurt.	*/
	check_exemptlist(chan, user);
	do_mask(chan, chan->channel.ban, b->mask, 'b');
	b->lastactive = now;
	if (b->desc && b->desc[0] != '@')
	  egg_snprintf(c, sizeof c, "%s %s", IRC_PREBANNED, b->desc);
	else
	  c[0] = 0;
        if (role == 2)
  	  kick_all(chan, b->mask, c[0] ? c : IRC_YOUREBANNED, 0);
        return;					/* Drop out on 1st ban.	*/
      } 
    }
  }
}

/* This is a bit cumbersome at the moment, but it works... Any improvements
 * then feel free to have a go.. Jason
 */
static void refresh_exempt(struct chanset_t *chan, char *user)
{
  maskrec *e = NULL;
  masklist *b = NULL;
  int cycle;

  /* Check global exempts in first cycle and channel exempts
     in second cycle. */
  for (cycle = 0; cycle < 2; cycle++) {
    for (e = cycle ? chan->exempts : global_exempts; e; e = e->next) {
      if (wild_match(user, e->mask) || wild_match(e->mask,user)) {
        for (b = chan->channel.ban; b && b->mask[0]; b = b->next) {
          if (wild_match(b->mask, user) || wild_match(user, b->mask)) {
            if (e->lastactive < now - 60 && !isexempted(chan, e->mask)) {
              do_mask(chan, chan->channel.exempt, e->mask, 'e');
              e->lastactive = now;
            }
          }
        }
      }
    }
  }
}

static void refresh_invite(struct chanset_t *chan, char *user)
{
  maskrec *i = NULL;
  int cycle;

  /* Check global invites in first cycle and channel invites
     in second cycle. */
  for (cycle = 0; cycle < 2; cycle++) {
    for (i = cycle ? chan->invites : global_invites; i; i = i->next) {
      if (wild_match(i->mask, user) &&
	  ((i->flags & MASKREC_STICKY) || (chan->channel.mode & CHANINV))) {
        if (i->lastactive < now - 60 && !isinvited(chan, i->mask)) {
          do_mask(chan, chan->channel.invite, i->mask, 'I');
	  i->lastactive = now;
	  return;
	}
      }
    }
  }
}

/* Enforce all channel bans in a given channel.  Ernst 18/3/1998
 */
static void enforce_bans(struct chanset_t *chan)
{
  char me[UHOSTLEN] = "";
  masklist *b = NULL;

  if (!me_op(chan))
    return;			/* Can't do it :( */
  simple_sprintf(me, "%s!%s", botname, botuserhost);
  /* Go through all bans, kicking the users. */
  for (b = chan->channel.ban; b && b->mask[0]; b = b->next) {
    if (!wild_match(b->mask, me))
      if (!isexempted(chan, b->mask) && !(chan->ircnet_status & CHAN_ASKED_EXEMPTS))
	kick_all(chan, b->mask, IRC_YOUREBANNED, 1);
  }
}

/* Make sure that all who are 'banned' on the userlist are actually in fact
 * banned on the channel.
 *
 * Note: Since i was getting a ban list, i assume i'm chop.
 */
static void recheck_bans(struct chanset_t *chan)
{
  maskrec *u = NULL;
  int cycle;

  /* Check global bans in first cycle and channel bans
     in second cycle. */
  for (cycle = 0; cycle < 2; cycle++) {
    for (u = cycle ? chan->bans : global_bans; u; u = u->next)
      if (!isbanned(chan, u->mask) && (!channel_dynamicbans(chan) ||
				       (u->flags & MASKREC_STICKY)))
	add_mode(chan, '+', 'b', u->mask);
  }
}

/* Make sure that all who are exempted on the userlist are actually in fact
 * exempted on the channel.
 *
 * Note: Since i was getting an excempt list, i assume i'm chop.
 */
static void recheck_exempts(struct chanset_t *chan)
{
  maskrec *e = NULL;
  masklist *b = NULL;
  int cycle;

  /* Check global exempts in first cycle and channel exempts
     in second cycle. */
  for (cycle = 0; cycle < 2; cycle++) {
    for (e = cycle ? chan->exempts : global_exempts; e; e = e->next) {
      if (!isexempted(chan, e->mask) &&
          (!channel_dynamicexempts(chan) || (e->flags & MASKREC_STICKY)))
        add_mode(chan, '+', 'e', e->mask);
      for (b = chan->channel.ban; b && b->mask[0]; b = b->next) {
        if ((wild_match(b->mask, e->mask) || wild_match(e->mask, b->mask)) &&
            !isexempted(chan, e->mask))
	  add_mode(chan,'+','e',e->mask);
	/* do_mask(chan, chan->channel.exempt, e->mask, 'e');*/
      }
    }
  }
}

/* Make sure that all who are invited on the userlist are actually in fact
 * invited on the channel.
 *
 * Note: Since i was getting an invite list, i assume i'm chop.
 */

static void recheck_invites(struct chanset_t *chan)
{
  maskrec *ir = NULL;
  int cycle;

  /* Check global invites in first cycle and channel invites
     in second cycle. */
  for (cycle = 0; cycle < 2; cycle++)  {
    for (ir = cycle ? chan->invites : global_invites; ir; ir = ir->next) {
      /* If invite isn't set and (channel is not dynamic invites and not invite
       * only) or invite is sticky.
       */
      if (!isinvited(chan, ir->mask) && ((!channel_dynamicinvites(chan) &&
          !(chan->channel.mode & CHANINV)) || ir->flags & MASKREC_STICKY))
	add_mode(chan, '+', 'I', ir->mask);
	/* do_mask(chan, chan->channel.invite, ir->mask, 'I');*/
    }
  }
}

/* Resets the masks on the channel.
 */
static void resetmasks(struct chanset_t *chan, masklist *m, maskrec *mrec, maskrec *global_masks, char mode)
{
  if (!me_op(chan))
    return;                     /* Can't do it */

  /* Remove masks we didn't put there */
  for (; m && m->mask[0]; m = m->next) {
    if (!u_equals_mask(global_masks, m->mask) && !u_equals_mask(mrec, m->mask))
      add_mode(chan, '-', mode, m->mask);
  }

  /* Make sure the intended masks are still there */
  switch (mode) {
    case 'b':
      recheck_bans(chan);
      break;
    case 'e':
      recheck_exempts(chan);
      break;
    case 'I':
      recheck_invites(chan);
      break;
    default:
      putlog(LOG_MISC, "*", "(!) Invalid mode '%c' in resetmasks()", mode);
      break;
  }
}

void check_this_ban(struct chanset_t *chan, char *banmask, int sticky)
{
  memberlist *m = NULL;
  char user[UHOSTLEN] = "";

  if (!me_op(chan))
    return;
  for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
    sprintf(user, "%s!%s", m->nick, m->userhost);
    if (wild_match(banmask, user) &&
        !(use_exempts &&
          (u_match_mask(global_exempts, user) ||
           u_match_mask(chan->exempts, user))))
      refresh_ban_kick(chan, user, m->nick);
  }
  if (!isbanned(chan, banmask) &&
      (!channel_dynamicbans(chan) || sticky))
    add_mode(chan, '+', 'b', banmask);
}

void check_this_exempt(struct chanset_t *chan, char *mask, int sticky)
{
  if (!isexempted(chan, mask) && (!channel_dynamicexempts(chan) || sticky))
    add_mode(chan, '+', 'e', mask);
}

void check_this_invite(struct chanset_t *chan, char *mask, int sticky)
{
  if (!isinvited(chan, mask) && (!channel_dynamicinvites(chan) || sticky))
    add_mode(chan, '+', 'I', mask);
}

void recheck_channel_modes(struct chanset_t *chan)
{
  int cur = chan->channel.mode,
      mns = chan->mode_mns_prot,
      pls = chan->mode_pls_prot;
  if (channel_closed(chan)) {
    pls |= CHANINV;
    mns &= ~CHANINV;
  }

  if (!(chan->status & CHAN_ASKEDMODES)) {
    if (pls & CHANINV && !(cur & CHANINV))
      add_mode(chan, '+', 'i', "");
    else if (mns & CHANINV && cur & CHANINV)
      add_mode(chan, '-', 'i', "");
    if (pls & CHANPRIV && !(cur & CHANPRIV))
      add_mode(chan, '+', 'p', "");
    else if (mns & CHANPRIV && cur & CHANPRIV)
      add_mode(chan, '-', 'p', "");
    if (pls & CHANSEC && !(cur & CHANSEC))
      add_mode(chan, '+', 's', "");
    else if (mns & CHANSEC && cur & CHANSEC)
      add_mode(chan, '-', 's', "");
    if (pls & CHANMODER && !(cur & CHANMODER))
      add_mode(chan, '+', 'm', "");
    else if (mns & CHANMODER && cur & CHANMODER)
      add_mode(chan, '-', 'm', "");
    if (pls & CHANNOCLR && !(cur & CHANNOCLR))
      add_mode(chan, '+', 'c', "");
    else if (mns & CHANNOCLR && cur & CHANNOCLR)
      add_mode(chan, '-', 'c', "");
    if (pls & CHANNOCTCP && !(cur & CHANNOCTCP))
      add_mode(chan, '+', 'C', "");
    else if (mns & CHANNOCTCP && cur & CHANNOCTCP)
      add_mode(chan, '-', 'C', "");
    if (pls & CHANREGON && !(cur & CHANREGON))
      add_mode(chan, '+', 'R', "");
    else if (mns & CHANREGON && cur & CHANREGON)
      add_mode(chan, '-', 'R', "");
    if (pls & CHANMODR && !(cur & CHANMODR))
      add_mode(chan, '+', 'M', "");
    else if (mns & CHANMODR && cur & CHANMODR)
      add_mode(chan, '-', 'M', "");
    if (pls & CHANLONLY && !(cur & CHANLONLY))
      add_mode(chan, '+', 'r', "");
    else if (mns & CHANLONLY && cur & CHANLONLY)
      add_mode(chan, '-', 'r', "");
    if (pls & CHANTOPIC && !(cur & CHANTOPIC))
      add_mode(chan, '+', 't', "");
    else if (mns & CHANTOPIC && cur & CHANTOPIC)
      add_mode(chan, '-', 't', "");
    if (pls & CHANNOMSG && !(cur & CHANNOMSG))
      add_mode(chan, '+', 'n', "");
    else if ((mns & CHANNOMSG) && (cur & CHANNOMSG))
      add_mode(chan, '-', 'n', "");
    if ((pls & CHANANON) && !(cur & CHANANON))
      add_mode(chan, '+', 'a', "");
    else if ((mns & CHANANON) && (cur & CHANANON))
      add_mode(chan, '-', 'a', "");
    if ((pls & CHANQUIET) && !(cur & CHANQUIET))
      add_mode(chan, '+', 'q', "");
    else if ((mns & CHANQUIET) && (cur & CHANQUIET))
      add_mode(chan, '-', 'q', "");
    if ((chan->limit_prot != 0) && (chan->channel.maxmembers == 0)) {
      char s[50] = "";

      sprintf(s, "%d", chan->limit_prot);
      add_mode(chan, '+', 'l', s);
    } else if ((mns & CHANLIMIT) && (chan->channel.maxmembers != 0))
      add_mode(chan, '-', 'l', "");
    if (chan->key_prot[0]) {
      if (rfc_casecmp(chan->channel.key, chan->key_prot) != 0) {
        if (chan->channel.key[0])
	  add_mode(chan, '-', 'k', chan->channel.key);
        add_mode(chan, '+', 'k', chan->key_prot);
      }
    } else if ((mns & CHANKEY) && (chan->channel.key[0]))
      add_mode(chan, '-', 'k', chan->channel.key);
  }
}

static void check_this_member(struct chanset_t *chan, char *nick, struct flag_record *fr)
{
  memberlist *m = NULL;
  char s[UHOSTLEN] = "", *p = NULL;

  m = ismember(chan, nick);
  if (!m || match_my_nick(nick) || !me_op(chan))
    return;
  if (me_op(chan)) {
    /* +d or bitch and not an op
     * we dont check private because +private does not imply bitch. */
    if (chan_hasop(m) && 
        (chk_deop(*fr, chan) ||
         (!loading && userlist && channel_bitch(chan) && !chk_op(*fr, chan)) ) ) {
      /* if (target_priority(chan, m, 1)) */
        add_mode(chan, '-', 'o', m->nick);
    } else if (!chan_hasop(m) && dovoice(chan) && chk_autoop(*fr, chan)) {
      do_op(m->nick, chan, 1, 0);
    }
    if (dovoice(chan)) {
      if (chan_hasvoice(m) && !chan_hasop(m)) {
        /* devoice +q users .. */
        if (chk_devoice(*fr, chan))
          add_mode(chan, '-', 'v', m->nick);
      } else if (!chan_hasvoice(m) && !chan_hasop(m)) {
        /* voice +v users */
        if (chk_voice(*fr, chan))
          add_mode(chan, '+', 'v', m->nick);
      }
    }
  }

  sprintf(s, "%s!%s", m->nick, m->userhost);
  /* check vs invites */
  if (use_invites &&
      (u_match_mask(global_invites,s) ||
       u_match_mask(chan->invites, s)))
    refresh_invite(chan, s);
  /* don't kickban if permanent exempted */
  if (!(use_exempts &&
        (u_match_mask(global_exempts,s) ||
         u_match_mask(chan->exempts, s)))) {
    if (u_match_mask(global_bans, s) || u_match_mask(chan->bans, s))
      refresh_ban_kick(chan, s, m->nick);
    /* are they +k ? */
    if (!chan_sentkick(m) && (chan_kick(*fr) || glob_kick(*fr)) && me_op(chan)) {
      check_exemptlist(chan, s);
      quickban(chan, m->userhost);
      p = get_user(&USERENTRY_COMMENT, m->user);
      dprintf(DP_SERVER, "KICK %s %s :%s%s\n", chan->name, m->nick, bankickprefix, p ? p : kickreason(KICK_KUSER));
      m->flags |= SENTKICK;
    }
  }
}

void check_this_user(char *hand, int delete, char *host)
{
  char s[UHOSTLEN] = "";
  memberlist *m = NULL;
  struct userrec *u = NULL;
  struct chanset_t *chan = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0};

  for (chan = chanset; chan; chan = chan->next)
    for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
      sprintf(s, "%s!%s", m->nick, m->userhost);
      u = m->user ? m->user : get_user_by_host(s);
      if ((u && !egg_strcasecmp(u->handle, hand) && delete < 2) ||
	  (!u && delete == 2 && wild_match(host, fixfrom(s)))) {
	u = delete ? NULL : u;
	get_user_flagrec(u, &fr, chan->dname);
	check_this_member(chan, m->nick, &fr);
      }
    }
}

static void enforce_bitch(struct chanset_t *chan) {
  if (!chan || !me_op(chan)) return;
  priority_do(chan, 1, PRIO_DEOP);
}

void enforce_closed(struct chanset_t *chan) {
  char buf[1024] = "";

  if (!chan || !me_op(chan)) return;
  if (!(chan->channel.mode & CHANINV))
    strcat(buf, "i");
  if (chan->closed_private && !(chan->channel.mode & CHANPRIV))
    strcat(buf, "p"); 
  if (buf && buf[0])
    dprintf(DP_MODE, "MODE %s +%s\n", chan->name, buf);
  priority_do(chan, 0, PRIO_KICK);
}

static char *
take_massopline(char *op, char **to_op)
{
  char *nick = NULL, *modes = NULL, *nicks = NULL, *ret = NULL;
  int i = 0, useop = 0;

  nicks = calloc(1, 51);
  modes = calloc(1, 10);

  for (i = 0; i < 4; i++) {
    nick = NULL;
    if (*to_op[0] || op) {
      /* if 'op' then use it, then move on to to_op */
      if (!useop && op) {
        nick = op;
        useop++;
      } else if (*to_op[0])
        nick = newsplit(to_op);
      if (nick) {
        strcat(modes, "+o");
        strcat(nicks, nick);
        strcat(nicks, " "); 
      }
    }
  }
  
  ret = calloc(1, 61);
  strcat(ret, modes);
  strcat(ret, " ");
  strcat(ret, nicks);
  free(modes);
  free(nicks);
  
  return ret;
}

static char *
take_makeline(char *op, char *deops, int deopn)
{
  int pos = 0, i = 0, n = 0, opn = op ? 1 : 0;
  char *ret = NULL;

  n = opn + deopn;		/* op + deops */
  ret = calloc(1, 101);
  pos = randint(deopn);
  
  for (i = 0; i < n; i++) {
    if (opn && i == pos)      strcat(ret, "+o");
    else if (deopn)           strcat(ret, "-o");
  }

  strcat(ret, " ");

  for (i = 0; i < n; i++) {
    if (opn && i == pos)
      strcat(ret, op);
    else if (deopn)
      strcat(ret, newsplit(&deops));

    strcat(ret, " ");
  }

  /* putlog(LOG_MISC, "*", "modeline: %s", ret); */
  return ret;  
}

static void
do_take(struct chanset_t *chan)
{
  memberlist *m = NULL;
  char work[512] = "", *to_op = NULL, *to_deop = NULL, *to_op_ptr = NULL, *to_deop_ptr = NULL;
  int lines = 0;

  to_op = to_op_ptr = calloc(1, 2048);
  to_deop = to_deop_ptr = calloc(1, 2048);

  for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
    int hasop = (m->flags & CHANOP), isbot = 0;

    if (m->user && (m->user->flags & USER_BOT))
      isbot++;

    if (rfc_casecmp(m->nick, botname)) {
      if (isbot && !hasop) {
        strcat(to_op, m->nick);
        strcat(to_op, " ");
      } else if (!isbot && hasop) {
        strcat(to_deop, m->nick);
        strcat(to_deop, " ");
      }
    }
  }
  shuffle(to_op, " ");
  shuffle(to_deop, " ");
  /*
  putlog(LOG_MISC, "*", "op: %s", to_op);
  putlog(LOG_MISC, "*", "deop: %s", to_deop);
  */
  while (to_op[0] || to_deop[0]) {
    int deopn = 0, i = 0;
    char *op = NULL, *modeline = NULL, deops[201] = "";

    if (to_op[0])
      op = newsplit(&to_op);

    for (i = 0; i < 4; i ++) {
      if (to_deop[0] && ((i < 3) || (!op))) {
        deopn++; 
        strcat(deops, newsplit(&to_deop)); 
        strcat(deops, " "); 
      }
    }

    strcat(work, "MODE ");
    strcat(work, chan->name);
    strcat(work, " ");

    if (deops[0])
      modeline = take_makeline(op, deops, deopn);
    else
      modeline = take_massopline(op, &to_op);
    strcat(work, modeline);
    strcat(work, "\n");
    lines++;
    free(modeline);

    /* just dump when we have 5 lines because the server is going to throttle after that anyway */
    if (lines == 5) {
      tputs(serv, work, strlen(work));
      work[0] = 0;
      lines = 0;
    }
  }

  if (work[0])
    tputs(serv, work, strlen(work));

  free(to_op_ptr);
  free(to_deop_ptr);

  if (channel_closed(chan))
    enforce_closed(chan);

  enforce_bitch(chan);		/* hell why not! */

  return;
}

time_t last_eI; /* this will stop +e and +I from being checked over and over if the bot is stuck in a 
                 * -o+o loop for some reason, hence possibly causing a SENDQ kill 
                 */

/* Things to do when i just became a chanop:
 */
void recheck_channel(struct chanset_t *chan, int dobans)
{
  memberlist *m = NULL;
  char s[UHOSTLEN] = "";
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0};
  static int stacking = 0;
  int stop_reset = 0, botops = 0, nonbotops = 0, botnonops = 0;


  if (stacking)
    return;			/* wewps */
  if (!userlist || loading)                /* Bot doesnt know anybody */
    return;                     /* ... it's better not to deop everybody */
  stacking++;

  putlog(LOG_DEBUG, "*", "recheck_channel %s", chan->dname);

  for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
    int hasop = chan_hasop(m);

    if (m) {
      if (m->user && m->user->flags & (USER_BOT | USER_OP)) {
        if (hasop)
          botops++;
        else
          botnonops++;
      } else if (hasop)
        nonbotops++;
    }
  }


  /* don't do much if i'm lonely bot. Maybe they won't notice me? :P */
  if (botops == 1 && !botnonops) {
    if (channel_bitch(chan) || channel_closed(chan))
      putlog(LOG_MISC, "*", "Opped in %s, not checking +closed/+bitch until more bots arrive.", chan->dname);
  } else {
    /* if the chan is +closed, mass deop first, safer that way. */
    if (channel_bitch(chan) || channel_closed(chan))
      enforce_bitch(chan);

    if (channel_closed(chan))
      enforce_closed(chan);
  }

  /* this can all die, we want to enforce +bitch/+take first :) */

  /* This is a bad hack for +e/+I */
  if (!channel_take(chan) && me_op(chan) && ((now - last_eI) > 30)) {
    last_eI = now;
    if (!(chan->ircnet_status & CHAN_ASKED_EXEMPTS) &&
        use_exempts == 1) {
        chan->ircnet_status |= CHAN_ASKED_EXEMPTS;
        dprintf(DP_MODE, "MODE %s +e\n", chan->name);
    }
    if (!(chan->ircnet_status & CHAN_ASKED_INVITED) &&
        use_invites == 1) {
      chan->ircnet_status |= CHAN_ASKED_INVITED;
      dprintf(DP_MODE, "MODE %s +I\n", chan->name);
    }
  }

  for (m = chan->channel.member; m && m->nick[0]; m = m->next) { 
    sprintf(s, "%s!%s", m->nick, m->userhost);

    if (!m->user && !m->tried_getuser) {
           m->tried_getuser = 1;
           m->user = get_user_by_host(s);
    }
    get_user_flagrec(m->user, &fr, chan->dname);
      if (glob_bot(fr) && chan_hasop(m) && !match_my_nick(m->nick))
	stop_reset = 1;
      check_this_member(chan, m->nick, &fr);
  }

  if (dobans) {
    if (channel_nouserbans(chan) && !stop_reset)
      resetbans(chan);
    else
      recheck_bans(chan);
    if (use_invites) {
      if (channel_nouserinvites(chan) && !stop_reset)
	resetinvites(chan);
      else
	recheck_invites(chan);
    }
    if (use_exempts) {
      if (channel_nouserexempts(chan) && !stop_reset)
	resetexempts(chan);
      else
	recheck_exempts(chan);
    } else {
      if (channel_enforcebans(chan)) 
        enforce_bans(chan);
    }
    flush_mode(chan, QUICK); 
    if ((chan->status & CHAN_ASKEDMODES) && !channel_inactive(chan)) 
      dprintf(DP_MODE, "MODE %s\n", chan->name);
    recheck_channel_modes(chan);
  }
  stacking--;
}

/* got 324: mode status
 * <server> 324 <to> <channel> <mode>
 */
static int got324(char *from, char *msg)
{
  int i = 1, ok =0;
  char *p = NULL, *q = NULL, *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  if (match_my_nick(chname))
      return 0;

  chan = findchan(chname);
  if (!chan) {
    putlog(LOG_MISC, "*", "%s: %s", IRC_UNEXPECTEDMODE, chname);
    dprintf(DP_SERVER, "PART %s\n", chname);
    return 0;
  }
  if (chan->status & CHAN_ASKEDMODES)
    ok = 1;
  chan->status &= ~CHAN_ASKEDMODES;
  chan->channel.mode = 0;
  while (msg[i] != 0) {
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
    if (msg[i] == 'k') {
      chan->channel.mode |= CHANKEY;
      p = strchr(msg, ' ');
      if (p != NULL) {		/* Test for null key assignment */
	p++;
	q = strchr(p, ' ');
	if (q != NULL) {
	  *q = 0;
	  my_setkey(chan, p);
	  strcpy(p, q + 1);
	} else {
	  my_setkey(chan, p);
	  *p = 0;
	}
      }
      if ((chan->channel.mode & CHANKEY) && (!chan->channel.key[0] ||
	  !strcmp("*", chan->channel.key)))
	/* Undernet use to show a blank channel key if one was set when
	 * you first joined a channel; however, this has been replaced by
	 * an asterisk and this has been agreed upon by other major IRC 
	 * networks so we'll check for an asterisk here as well 
	 * (guppy 22Dec2001) */ 
        chan->status |= CHAN_ASKEDMODES;
    }
    if (msg[i] == 'l') {
      p = strchr(msg, ' ');
      if (p != NULL) {		/* test for null limit assignment */
	p++;
	q = strchr(p, ' ');
	if (q != NULL) {
	  *q = 0;
	  chan->channel.maxmembers = atoi(p);
/*	  strcpy(p, q + 1); */
          sprintf(p, "%s", q + 1);
	} else {
	  chan->channel.maxmembers = atoi(p);
	  *p = 0;
	}
      }
    }
    i++;
  }
  if (ok)
    recheck_channel_modes(chan);
  return 0;
}

static void memberlist_reposition(struct chanset_t * chan, memberlist * target) {
  /* Move target from it's current position to it's correct sorted position */
  memberlist *old = NULL, *m = NULL;
  if (chan->channel.member == target) {
    chan->channel.member = target->next;
  } else {
    for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
      if (m->next == target) {
        m->next = target->next;
        break;
      }
    }
  }
  target->next = NULL;
  for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
    if (rfc_casecmp(m->nick, target->nick)>0) {
      if (old) {
        target->next = m;
        old->next = target;
      } else {
        target->next = chan->channel.member;
        chan->channel.member = target;
      }
      return;
    }
    old = m;
  }
  if (old) {
    target->next = old->next;
    old->next = target;
  } else {
    target->next = NULL;
    chan->channel.member = target;
  }
}


static int got352or4(struct chanset_t *chan, char *user, char *host, char *server, char *nick, char *flags, int hops)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0};
  char userhost[UHOSTLEN] = "";
  memberlist *m = NULL;
  int waschanop;

  m = ismember(chan, nick);	/* In my channel list copy? */
  if (!m) {			/* Nope, so update */
    m = newmember(chan, nick);	/* Get a new channel entry */
    m->joined = m->split = m->delay = 0L;	/* Don't know when he joined */
    m->flags = 0;		/* No flags for now */
    m->last = now;		/* Last time I saw him */
  }
  strcpy(m->nick, nick);	/* Store the nick in list */

  if (server) {
    struct chanset_t *ch = NULL;
    memberlist *ml = NULL;
    strncpyz(m->server, server, SERVLEN);
    /* Propagate server to other channel memlists... might save us a WHO #chan */
    for (ch = chanset; ch; ch = ch->next) {
      if (ch != chan) {
        for (ml = ch->channel.member; ml && ml->nick[0]; ml = ml->next) {
          if (!strcmp(ml->nick, m->nick)) {
            strcpy(ml->server, m->server);
            break;
          }
        }
      }
    }
  } else
    m->server[0] = 0;

  {
    struct chanset_t *ch = NULL;
    memberlist *ml = NULL;
    m->hops = hops;
    /* Propagate hops to other channel memlists... might save us a WHO #chan */
    for (ch = chanset; ch; ch = ch->next) {
      if (ch != chan) {
        for (ml = ch->channel.member; ml && ml->nick[0]; ml = ml->next) {
          if (!strcmp(ml->nick, m->nick)) {
            ml->hops = m->hops;
            break;
          }
        }
      }
    }
  }


  /* Store the userhost */
  simple_sprintf(m->userhost, "%s@%s", user, host);
  simple_sprintf(userhost, "%s!%s", nick, m->userhost);
  /* Combine n!u@h */
  m->user = NULL;		/* No handle match (yet) */
  m->tried_getuser = 0;
  if (match_my_nick(nick)) {	/* Is it me? */
    strcpy(botuserhost, m->userhost);	/* Yes, save my own userhost */
    m->joined = now;		/* set this to keep the whining masses happy */
  }
  waschanop = me_op(chan);      /* Am I opped here? */
  if (strchr(flags, '@') != NULL)	/* Flags say he's opped? */
    m->flags |= (CHANOP | WASOP);	/* Yes, so flag in my table */
  else
    m->flags &= ~(CHANOP | WASOP);
  if (strchr(flags, '*'))
    m->flags |= OPER;
  else
    m->flags &= ~OPER;
  if (strchr(flags, '+') != NULL)	/* Flags say he's voiced? */
    m->flags |= CHANVOICE;	/* Yes */
  else
    m->flags &= ~CHANVOICE;
  if (!(m->flags & (CHANVOICE | CHANOP)))
    m->flags |= STOPWHO;

  if (match_my_nick(nick) && !waschanop && me_op(chan))
    recheck_channel(chan, 1);
  if (match_my_nick(nick) && any_ops(chan) && !me_op(chan))
    chan->channel.do_opreq = 1;

  m->user = get_user_by_host(userhost);
  m->tried_getuser = 1;
  get_user_flagrec(m->user, &fr, chan->dname);
  /* are they a chanop, and me too */
      /* are they a channel or global de-op */
  if (chan_hasop(m) && me_op(chan) && chk_deop(fr, chan) && !match_my_nick(nick)) 
      /* && target_priority(chan, m, 1) */
    add_mode(chan, '-', 'o', nick);

  /* if channel is enforce bans */
  if (channel_enforcebans(chan) &&
      /* and user matches a ban */
      (u_match_mask(global_bans, userhost) || u_match_mask(chan->bans, userhost)) &&
      /* and it's not me, and i'm an op */
      !match_my_nick(nick) && me_op(chan)) {
    /*  && target_priority(chan, m, 0) */
    dprintf(DP_SERVER, "KICK %s %s :%s%s\n", chan->name, nick, bankickprefix, kickreason(KICK_BANNED));
    m->flags |= SENTKICK;
  }
  /* if the user is a +k */
  else if ((chan_kick(fr) || glob_kick(fr)) &&
           /* and it's not me :) who'd set me +k anyway, a sicko? */
           /* and if im an op */
           !match_my_nick(nick) && me_op(chan)) {
           /* && target_priority(chan, m, 0) */
    /* cya later! */
    quickban(chan, userhost);
    dprintf(DP_SERVER, "KICK %s %s :%s%s\n", chan->name, nick, bankickprefix, kickreason(KICK_KUSER));
    m->flags |= SENTKICK;
  }

  return 0;
}

/* got a 352: who info!
 */
static int got352(char *from, char *msg)
{
  char *nick = NULL, *user = NULL, *host = NULL, *chname = NULL, *flags = NULL, *server = NULL, *hops = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);		/* Skip my nick - effeciently */
  chname = newsplit(&msg);	/* Grab the channel */
  chan = findchan(chname);	/* See if I'm on channel */
  if (chan) {			/* Am I? */
    user = newsplit(&msg);	/* Grab the user */
    host = newsplit(&msg);	/* Grab the host */
    server = newsplit(&msg);      /* And the server */
    nick = newsplit(&msg);	/* Grab the nick */
    flags = newsplit(&msg);	/* Grab the flags */
    hops = newsplit(&msg);	/* grab server hops */
    hops++;
    got352or4(chan, user, host, server, nick, flags, atoi(hops));
  }
  return 0;
}

/* got a 354: who info! - iru style
 */
static int got354(char *from, char *msg)
{
  char *nick = NULL, *user = NULL, *host = NULL, *chname = NULL, *flags = NULL, *hops = NULL;
  struct chanset_t *chan = NULL;

  if (use_354) {
    newsplit(&msg);		/* Skip my nick - effeciently */
    if (msg[0] && (strchr(CHANMETA, msg[0]) != NULL)) {
      chname = newsplit(&msg);	/* Grab the channel */
      chan = findchan(chname);	/* See if I'm on channel */
      if (chan) {		/* Am I? */
	user = newsplit(&msg);	/* Grab the user */
	host = newsplit(&msg);	/* Grab the host */
	nick = newsplit(&msg);	/* Grab the nick */
	flags = newsplit(&msg);	/* Grab the flags */
        hops = newsplit(&msg);  /* yay for hops, does iru even have hops?? */
        hops++;
	got352or4(chan, user, host, NULL, nick, flags, atoi(hops));
      }
    }
  }
  return 0;
}

/* got 315: end of who
 * <server> 315 <to> <chan> :End of /who
 */
static int got315(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  /* May have left the channel before the who info came in */
  if (!chan || !channel_pending(chan))
    return 0;
  /* Finished getting who list, can now be considered officially ON CHANNEL */
  chan->status |= CHAN_ACTIVE;
  chan->status &= ~CHAN_PEND;
  /* Am *I* on the channel now? if not, well d0h. */
  if (!ismember(chan, botname)) {
    putlog(LOG_MISC | LOG_JOIN, chan->dname, "Oops, I'm not really on %s.",
	   chan->dname);
    clear_channel(chan, 1);
    chan->status &= ~CHAN_ACTIVE;
    dprintf(DP_MODE, "JOIN %s %s\n",
	    (chan->name[0]) ? chan->name : chan->dname,
	    chan->channel.key[0] ? chan->channel.key : chan->key_prot);
  } else {
    if (me_op(chan))
      recheck_channel(chan, 1);
    else if (chan->channel.members == 1)
      chan->status |= CHAN_STOP_CYCLE;
    else 
      request_op(chan);
  }
  /* do not check for i-lines here. */
  return 0;
}

/* got 367: ban info
 * <server> 367 <to> <chan> <ban> [placed-by] [timestamp]
 */
static int got367(char *from, char *origmsg)
{
  char *ban = NULL, *who = NULL, *chname = NULL, buf[511] = "", *msg = NULL;
  struct chanset_t *chan = NULL;

  strncpy(buf, origmsg, 510);
  buf[510] = 0;
  msg = buf;
  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (!chan || !(channel_pending(chan) || channel_active(chan)))
    return 0;
  ban = newsplit(&msg);
  who = newsplit(&msg);
  /* Extended timestamp format? */
  if (who[0])
    newban(chan, ban, who);
  else
    newban(chan, ban, "existent");
  return 0;
}

/* got 368: end of ban list
 * <server> 368 <to> <chan> :etc
 */
static int got368(char *from, char *msg)
{
  struct chanset_t *chan = NULL;
  char *chname = NULL;

  /* Okay, now add bans that i want, which aren't set yet */
  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (chan)
    chan->status &= ~CHAN_ASKEDBANS;
  /* If i sent a mode -b on myself (deban) in got367, either
   * resetbans() or recheck_bans() will flush that.
   */
  return 0;
}

/* got 348: ban exemption info
 * <server> 348 <to> <chan> <exemption>
 */
static int got348(char *from, char *origmsg)
{
  char *exempt = NULL, *who = NULL, *chname = NULL, buf[511] = "", *msg = NULL;
  struct chanset_t *chan = NULL;

  if (use_exempts == 0)
    return 0;

  strncpy(buf, origmsg, 510);
  buf[510] = 0;
  msg = buf;
  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (!chan || !(channel_pending(chan) || channel_active(chan)))
    return 0;
  exempt = newsplit(&msg);
  who = newsplit(&msg);
  /* Extended timestamp format? */
  if (who[0])
    newexempt(chan, exempt, who);
  else
    newexempt(chan, exempt, "existent");
  return 0;
}

/* got 349: end of ban exemption list
 * <server> 349 <to> <chan> :etc
 */
static int got349(char *from, char *msg)
{
  struct chanset_t *chan = NULL;
  char *chname = NULL;

  if (use_exempts == 1) {
    newsplit(&msg);
    chname = newsplit(&msg);
    chan = findchan(chname);
    if (chan) {
      putlog(LOG_DEBUG, "*", "END +e %s", chan->dname);
      chan->ircnet_status &= ~CHAN_ASKED_EXEMPTS;
      if (channel_enforcebans(chan))
        enforce_bans(chan);
    }
  }
  return 0;
}

/* got 346: invite exemption info
 * <server> 346 <to> <chan> <exemption>
 */
static int got346(char *from, char *origmsg)
{
  char *invite = NULL, *who = NULL, *chname = NULL, buf[511] = "", *msg = NULL;
  struct chanset_t *chan = NULL;

  strncpy(buf, origmsg, 510);
  buf[510] = 0;
  msg = buf;
  if (use_invites == 0)
    return 0;
  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (!chan || !(channel_pending(chan) || channel_active(chan)))
    return 0;
  invite = newsplit(&msg);
  who = newsplit(&msg);
  /* Extended timestamp format? */
  if (who[0])
    newinvite(chan, invite, who);
  else
    newinvite(chan, invite, "existent");
  return 0;
}

/* got 347: end of invite exemption list
 * <server> 347 <to> <chan> :etc
 */
static int got347(char *from, char *msg)
{
  struct chanset_t *chan = NULL;
  char *chname = NULL;

  if (use_invites == 1) {
    newsplit(&msg);
    chname = newsplit(&msg);
    chan = findchan(chname);
    if (chan)
      chan->ircnet_status &= ~CHAN_ASKED_INVITED;
  }
  return 0;
}

/* Too many channels.
 */
static int got405(char *from, char *msg)
{
  char *chname = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  putlog(LOG_MISC, "*", IRC_TOOMANYCHANS, chname);
  return 0;
}

/* This is only of use to us with !channels. We get this message when
 * attempting to join a non-existant !channel... The channel must be
 * created by sending 'JOIN !!<channel>'. <cybah>
 *
 * 403 - ERR_NOSUCHCHANNEL
 */
static int got403(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  if (chname && chname[0]=='!') {
    chan = findchan_by_dname(chname);
    if (!chan) {
      chan = findchan(chname);
      if (!chan)
        return 0;       /* Ignore it */
      /* We have the channel unique name, so we have attempted to join
       * a specific !channel that doesnt exist. Now attempt to join the
       * channel using it's short name.
       */
      putlog(LOG_MISC, "*",
             "Unique channel %s does not exist... Attempting to join with "
             "short name.", chname);
      dprintf(DP_SERVER, "JOIN %s\n", chan->dname);
    } else {
      /* We have found the channel, so the server has given us the short
       * name. Prefix another '!' to it, and attempt the join again...
       */
      putlog(LOG_MISC, "*",
             "Channel %s does not exist... Attempting to create it.", chname);
      dprintf(DP_SERVER, "JOIN !%s\n", chan->dname);
    }
  }
  return 0;
}

/* got 471: can't join channel, full
 */
static int got471(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  /* !channel short names (also referred to as 'description names'
   * can be received by skipping over the unique ID.
   */
  if ((chname[0] == '!') && (strlen(chname) > CHANNEL_ID_LEN)) {
    chname += CHANNEL_ID_LEN;
    chname[0] = '!';
  }
  /* We use dname because name is first set on JOIN and we might not
   * have joined the channel yet.
   */
  chan = findchan_by_dname(chname);
  if (chan) {
    putlog(LOG_JOIN, chan->dname, IRC_CHANFULL, chan->dname);
    request_in(chan);
/* need: limit */
    chan = findchan_by_dname(chname); 
    if (!chan)
      return 0;
  } else
    putlog(LOG_JOIN, chname, IRC_CHANFULL, chname);
  return 0;
}

/* got 473: can't join channel, invite only
 */
static int got473(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  /* !channel short names (also referred to as 'description names'
   * can be received by skipping over the unique ID.
   */
  if ((chname[0] == '!') && (strlen(chname) > CHANNEL_ID_LEN)) {
    chname += CHANNEL_ID_LEN;
    chname[0] = '!';
  }
  /* We use dname because name is first set on JOIN and we might not
   * have joined the channel yet.
   */
  chan = findchan_by_dname(chname);
  if (chan) {
    putlog(LOG_JOIN, chan->dname, IRC_CHANINVITEONLY, chan->dname);
    request_in(chan);
/* need: invite */
    chan = findchan_by_dname(chname); 
    if (!chan)
      return 0;
  } else
    putlog(LOG_JOIN, chname, IRC_CHANINVITEONLY, chname);
  return 0;
}

/* got 474: can't join channel, banned
 */
static int got474(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  /* !channel short names (also referred to as 'description names'
   * can be received by skipping over the unique ID.
   */
  if ((chname[0] == '!') && (strlen(chname) > CHANNEL_ID_LEN)) {
    chname += CHANNEL_ID_LEN;
    chname[0] = '!';
  }
  /* We use dname because name is first set on JOIN and we might not
   * have joined the channel yet.
   */
  chan = findchan_by_dname(chname);
  if (chan) {
    putlog(LOG_JOIN, chan->dname, IRC_BANNEDFROMCHAN, chan->dname);
    request_in(chan);
/* need: unban */
    chan = findchan_by_dname(chname); 
    if (!chan)
      return 0;
  } else
    putlog(LOG_JOIN, chname, IRC_BANNEDFROMCHAN, chname);
  return 0;
}

/* got 475: can't goin channel, bad key
 */
static int got475(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  /* !channel short names (also referred to as 'description names'
   * can be received by skipping over the unique ID.
   */
  if ((chname[0] == '!') && (strlen(chname) > CHANNEL_ID_LEN)) {
    chname += CHANNEL_ID_LEN;
    chname[0] = '!';
  }
  /* We use dname because name is first set on JOIN and we might not
   * have joined the channel yet.
   */
  chan = findchan_by_dname(chname);
  if (chan) {
    putlog(LOG_JOIN, chan->dname, IRC_BADCHANKEY, chan->dname);
    if (chan->channel.key[0]) {
      free(chan->channel.key);
      chan->channel.key = (char *) calloc(1, 1);
      dprintf(DP_MODE, "JOIN %s %s\n", chan->dname, chan->key_prot);
    } else {
      request_in(chan);
/* need: key */
      chan = findchan_by_dname(chname); 
      if (!chan)
        return 0;
    }
  } else
    putlog(LOG_JOIN, chname, IRC_BADCHANKEY, chname);
  return 0;
}

/* got invitation
 */
static int gotinvite(char *from, char *msg)
{
  char *nick = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);
  fixcolon(msg);
  nick = splitnick(&from);
  if (!rfc_casecmp(last_invchan, msg))
    if (now - last_invtime < 30)
      return 0;		/* Two invites to the same channel in 30 seconds? */
  putlog(LOG_MISC, "*", "%s!%s invited me to %s", nick, from, msg);
  strncpy(last_invchan, msg, 299);
  last_invchan[299] = 0;
  last_invtime = now;
  chan = findchan(msg);
  if (!chan)
    /* Might be a short-name */
    chan = findchan_by_dname(msg);

  if (chan && (channel_pending(chan) || channel_active(chan)))
    dprintf(DP_HELP, "NOTICE %s :I'm already here.\n", nick);
  else if (chan && shouldjoin(chan))
    dprintf(DP_MODE, "JOIN %s %s\n", (chan->name[0]) ? chan->name : chan->dname,
            chan->channel.key[0] ? chan->channel.key : chan->key_prot);
  return 0;
}

/* Set the topic.
 */
static void set_topic(struct chanset_t *chan, char *k)
{
  if (chan->channel.topic)
    free(chan->channel.topic);
  if (k && k[0]) {
    chan->channel.topic = (char *) calloc(1, strlen(k) + 1);
    strcpy(chan->channel.topic, k);
  } else
    chan->channel.topic = NULL;
}

/* Topic change.
 */
static int gottopic(char *from, char *msg)
{
  char *nick = NULL, *chname = NULL;
  memberlist *m = NULL;
  struct chanset_t *chan = NULL;
  struct userrec *u = NULL;

  chname = newsplit(&msg);
  fixcolon(msg);
  u = get_user_by_host(from);
  nick = splitnick(&from);
  chan = findchan(chname);
  if (chan) {
    putlog(LOG_JOIN, chan->dname, "Topic changed on %s by %s!%s: %s",
           chan->dname, nick, from, msg);
    m = ismember(chan, nick);
    if (m != NULL)
      m->last = now;
    set_topic(chan, msg);
  }
  return 0;
}

/* 331: no current topic for this channel
 * <server> 331 <to> <chname> :etc
 */
static int got331(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (chan) {
    set_topic(chan, NULL);
  }
  return 0;
}

/* 332: topic on a channel i've just joined
 * <server> 332 <to> <chname> :topic goes here
 */
static int got332(char *from, char *msg)
{
  struct chanset_t *chan = NULL;
  char *chname = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (chan) {
    fixcolon(msg);
    set_topic(chan, msg);
  }
  return 0;
}

/* Got a join
 */
static int gotjoin(char *from, char *chname)
{
  char *nick = NULL, *p = NULL, buf[UHOSTLEN] = "", *uhost = buf;
  char *ch_dname = NULL;
  struct chanset_t *chan = NULL;
  memberlist *m = NULL;
  masklist *b = NULL;
  struct userrec *u = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0};

  fixcolon(chname);
  chan = findchan(chname);
  if (!chan && chname[0] == '!') {
    /* As this is a !channel, we need to search for it by display (short)
     * name now. This will happen when we initially join the channel, as we
     * dont know the unique channel name that the server has made up. <cybah>
     */
    int	l_chname = strlen(chname);

    if (l_chname > (CHANNEL_ID_LEN + 1)) {
      ch_dname = calloc(1, l_chname + 1);
      if (ch_dname) {
	egg_snprintf(ch_dname, l_chname + 2, "!%s",
		     chname + (CHANNEL_ID_LEN + 1));
	chan = findchan_by_dname(ch_dname);
	if (!chan) {
	  /* Hmm.. okay. Maybe the admin's a genius and doesn't know the
	   * difference between id and descriptive channel names. Search
	   * the channel name in the dname list using the id-name.
	   */
	   chan = findchan_by_dname(chname);
	   if (chan) {
	     /* Duh, I was right. Mark this channel as inactive and log
	      * the incident.
	      */
	     chan->status |= CHAN_INACTIVE;
	     putlog(LOG_MISC, "*", "Deactivated channel %s, because it uses "
		    "an ID channel-name. Use the descriptive name instead.",
		    chname);
	     dprintf(DP_SERVER, "PART %s\n", chname);
	     goto exit;
	   }
	}
      }
    }
  } else if (!chan) {
    /* As this is not a !chan, we need to search for it by display name now.
     * Unlike !chan's, we dont need to remove the unique part.
     */
    chan = findchan_by_dname(chname);
  }

  if (!chan || (chan && !shouldjoin(chan))) {
    strcpy(uhost, from);
    nick = splitnick(&uhost);
    if (match_my_nick(nick)) {
      putlog(LOG_MISC, "*", "joined %s but didn't want to!", chname);
      dprintf(DP_MODE, "PART %s\n", chname);
    }
  } else if (!channel_pending(chan)) {
    chan->status &= ~CHAN_STOP_CYCLE;
    strcpy(uhost, from);
    nick = splitnick(&uhost);
    detect_chan_flood(nick, uhost, from, chan, FLOOD_JOIN, NULL);

    chan = findchan(chname);
    if (!chan) {   
      if (ch_dname)
	chan = findchan_by_dname(ch_dname);
      else
	chan = findchan_by_dname(chname);
    }
    if (!chan)
      /* The channel doesn't exist anymore, so get out of here. */
      goto exit;

    /* Grab last time joined before we update it */
    u = get_user_by_host(from);
    get_user_flagrec(u, &fr, chan->dname); /* Lam: fix to work with !channels */
    if (!channel_active(chan) && !match_my_nick(nick)) {
      /* uh, what?!  i'm on the channel?! */
      putlog(LOG_MISC, chan->dname,
	     "confused bot: guess I'm on %s and didn't realize it",
	     chan->dname);
      chan->status |= CHAN_ACTIVE;
      chan->status &= ~CHAN_PEND;
      reset_chan_info(chan);
    } else {
      m = ismember(chan, nick);
      if (m && m->split && !egg_strcasecmp(m->userhost, uhost)) {
	chan = findchan(chname);
	if (!chan) {
	  if (ch_dname)
	    chan = findchan_by_dname(ch_dname);
	  else
	    chan = findchan_by_dname(chname);
        }
        if (!chan)
          /* The channel doesn't exist anymore, so get out of here. */
          goto exit;

	/* The tcl binding might have deleted the current user. Recheck. */
	u = get_user_by_host(from);
	m->split = 0;
	m->last = now;
	m->delay = 0L;
	m->flags = (chan_hasop(m) ? WASOP : 0);
	m->user = u;
	set_handle_laston(chan->dname, u, now);
	m->flags |= STOPWHO;
	putlog(LOG_JOIN, chan->dname, "%s (%s) returned to %s.", nick, uhost,
	       chan->dname);
      } else {
	if (m)
	  killmember(chan, nick);
	m = newmember(chan, nick);
	m->joined = now;
	m->split = 0L;
	m->flags = 0;
	m->last = now;
	m->delay = 0L;
	strcpy(m->nick, nick);
	strcpy(m->userhost, uhost);
	m->user = u;
	m->flags |= STOPWHO;

	/* The tcl binding might have deleted the current user and the
	 * current channel, so we'll now have to re-check whether they
	 * both still exist.
	 */
	chan = findchan(chname);
	if (!chan) {
	  if (ch_dname)
	    chan = findchan_by_dname(ch_dname);
	  else
	    chan = findchan_by_dname(chname);
	}
	if (!chan)
	  /* The channel doesn't exist anymore, so get out of here. */
	  goto exit;

	/* The record saved in the channel record always gets updated,
	   so we can use that. */
	u = m->user;

	if (match_my_nick(nick)) {
	  /* It was me joining! Need to update the channel record with the
	   * unique name for the channel (as the server see's it). <cybah>
	   */
	  strncpy(chan->name, chname, 81);
	  chan->name[80] = 0;
	  chan->status &= ~CHAN_JUPED;

          /* ... and log us joining. Using chan->dname for the channel is
	   * important in this case. As the config file will never contain
	   * logs with the unique name.
           */
	  if (chname[0] == '!')
	    putlog(LOG_JOIN, chan->dname, "%s joined %s (%s)", nick, chan->dname, chname);
	  else
	    putlog(LOG_JOIN, chan->dname, "%s joined %s.", nick,
	           chname);
	  if (!match_my_nick(chname))
 	    reset_chan_info(chan);
	} else {

	  putlog(LOG_JOIN, chan->dname,
		 "%s (%s) joined %s.", nick, uhost, chan->dname);
	  /* Don't re-display greeting if they've been on the channel
	   * recently.
	   */
	  set_handle_laston(chan->dname, u, now);
	}
      }
      /* ok, the op-on-join,etc, tests...first only both if Im opped */

      if (me_op(chan)) {
	/* Check for and reset exempts and invites.
	 *
	 * This will require further checking to account for when to use the
	 * various modes.
	 */
	if (u_match_mask(global_invites,from) ||
	    u_match_mask(chan->invites, from))
	  refresh_invite(chan, from);

	if (!(use_exempts &&
	      (u_match_mask(global_exempts,from) ||
	       u_match_mask(chan->exempts, from)))) {
          if (channel_enforcebans(chan) && !chan_op(fr) && !glob_op(fr) && !chan_sentkick(m) &&
              !(use_exempts && (isexempted(chan, from) || (chan->ircnet_status & CHAN_ASKED_EXEMPTS))) && 
              me_op(chan)) {
            for (b = chan->channel.ban; b->mask[0]; b = b->next) {
              if (wild_match(b->mask, from)) {
                dprintf(DP_SERVER, "KICK %s %s :%s%s\n", chname, m->nick, bankickprefix, kickreason(KICK_BANNED));
                m->flags |= SENTKICK;
                goto exit;
              }
            }
          }
	  /* If it matches a ban, dispose of them. */
	  if (u_match_mask(global_bans, from) ||
	      u_match_mask(chan->bans, from)) {
	    refresh_ban_kick(chan, from, nick);
	  /* Likewise for kick'ees */
	  } else if (!chan_sentkick(m) && (glob_kick(fr) || chan_kick(fr)) &&
		     me_op(chan)) {
	    check_exemptlist(chan, from);
	    quickban(chan, from);
	    p = get_user(&USERENTRY_COMMENT, m->user);
            dprintf(DP_MODE, "KICK %s %s :%s%s\n", chname, nick, bankickprefix, kickreason(KICK_KUSER));
	    m->flags |= SENTKICK;
	  }
	}
        if (!chan_hasop(m) && dovoice(chan) && chk_autoop(fr, chan)) {
          do_op(m->nick, chan, 1, 0);
        }
      }
    }
  }

exit:
  if (ch_dname)
    free(ch_dname);
  return 0;
}

/* Got a part
 */
static int gotpart(char *from, char *msg)
{
  char *nick = NULL, *chname = NULL;
  struct chanset_t *chan = NULL;
  struct userrec *u = NULL;

  chname = newsplit(&msg);
  fixcolon(chname);
  fixcolon(msg);
  chan = findchan(chname);
  if (chan && !shouldjoin(chan)) {
  /* shouldnt this check for match_my_nick? */
    clear_channel(chan, 1);
    chan->status &= ~(CHAN_ACTIVE | CHAN_PEND);
    return 0;
  }
  if (chan && !channel_pending(chan)) {
    u = get_user_by_host(from);
    nick = splitnick(&from);
    if (!channel_active(chan)) {
      /* whoa! */
      putlog(LOG_MISC, chan->dname,
	  "confused bot: guess I'm on %s and didn't realize it", chan->dname);
      chan->status |= CHAN_ACTIVE;
      chan->status &= ~CHAN_PEND;
      reset_chan_info(chan);
    }
    set_handle_laston(chan->dname, u, now);
    chan = findchan(chname);
    if (!chan)
      return 0;

    killmember(chan, nick);
    if (msg[0])
      putlog(LOG_JOIN, chan->dname, "%s (%s) left %s (%s).", nick, from, chan->dname, msg);
    else
      putlog(LOG_JOIN, chan->dname, "%s (%s) left %s.", nick, from, chan->dname);
    /* If it was me, all hell breaks loose... */
    if (match_my_nick(nick)) {
      clear_channel(chan, 1);
      chan->status &= ~(CHAN_ACTIVE | CHAN_PEND);
      if (shouldjoin(chan))
	dprintf(DP_MODE, "JOIN %s %s\n",
	        (chan->name[0]) ? chan->name : chan->dname,
	        chan->channel.key[0] ? chan->channel.key : chan->key_prot);
    } else
      check_lonely_channel(chan);
  }
  return 0;
}

/* Got a kick
 */
static int gotkick(char *from, char *origmsg)
{
  char *nick = NULL, *whodid = NULL, *chname = NULL, s1[UHOSTLEN] = "", buf[UHOSTLEN] = "", *uhost = buf;
  char buf2[511] = "", *msg = NULL;
  memberlist *m = NULL;
  struct chanset_t *chan = NULL;
  struct userrec *u = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0};

  strncpy(buf2, origmsg, 510);
  buf2[510] = 0;
  msg = buf2;
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (!chan)
    return 0;
  nick = newsplit(&msg);
  if (match_my_nick(nick) && channel_pending(chan) && shouldjoin(chan)) {
    chan->status &= ~(CHAN_ACTIVE | CHAN_PEND);
    dprintf(DP_MODE, "JOIN %s %s\n",
            (chan->name[0]) ? chan->name : chan->dname,
            chan->channel.key[0] ? chan->channel.key : chan->key_prot);
    clear_channel(chan, 1);
    return 0; /* rejoin if kicked before getting needed info <Wcc[08/08/02]> */
  }
  if (channel_active(chan)) {
#ifdef S_AUTOLOCK
    chan->channel.fighting++;
#endif /* S_AUTOLOCK */
    fixcolon(msg);
    u = get_user_by_host(from);
    strcpy(uhost, from);
    whodid = splitnick(&uhost);
    detect_chan_flood(whodid, uhost, from, chan, FLOOD_KICK, nick);

    chan = findchan(chname);
    if (!chan)
      return 0;     

    m = ismember(chan, whodid);
    if (m)
      m->last = now;
    /* This _needs_ to use chan->dname <cybah> */
    get_user_flagrec(u, &fr, chan->dname);
    set_handle_laston(chan->dname, u, now);
 
    chan = findchan(chname);
    if (!chan)
      return 0;

    if ((m = ismember(chan, nick))) {
      struct userrec *u2;

      simple_sprintf(s1, "%s!%s", m->nick, m->userhost);
      u2 = get_user_by_host(s1);
      set_handle_laston(chan->dname, u2, now);
      maybe_revenge(chan, from, s1, REVENGE_KICK);
    } else {
      simple_sprintf(s1, "%s!*@could.not.loookup.hostname", nick);
    }
    putlog(LOG_MODES, chan->dname, "%s kicked from %s by %s: %s", s1,
	   chan->dname, from, msg);
    /* Kicked ME?!? the sods! */
    if (match_my_nick(nick) && shouldjoin(chan)) {
      chan->status &= ~(CHAN_ACTIVE | CHAN_PEND);
      dprintf(DP_MODE, "JOIN %s %s\n",
              (chan->name[0]) ? chan->name : chan->dname,
              chan->channel.key[0] ? chan->channel.key : chan->key_prot);
      clear_channel(chan, 1);
    } else {
      killmember(chan, nick);
      check_lonely_channel(chan);
    }
  }
  return 0;
}

/* Got a nick change
 */
static int gotnick(char *from, char *msg)
{
  char *nick = NULL, *chname = NULL, s1[UHOSTLEN] = "", buf[UHOSTLEN] = "", *uhost = buf;
  memberlist *m = NULL, *mm = NULL;
  struct chanset_t *chan = NULL, *oldchan = NULL;
  struct userrec *u = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0};

  strcpy(uhost, from);
  nick = splitnick(&uhost);
  fixcolon(msg);
  clear_chanlist_member(nick);	/* Cache for nick 'nick' is meaningless now. */
  for (chan = chanset; chan; chan = chan->next) {
    oldchan = chan;
    chname = chan->dname; 
    m = ismember(chan, nick);

    if (m) {
      putlog(LOG_JOIN, chan->dname, "Nick change: %s -> %s", nick, msg);
      m->last = now;
      if (rfc_casecmp(nick, msg)) {
	/* Not just a capitalization change */
	mm = ismember(chan, msg);
	if (mm) {
	  /* Someone on channel with old nick?! */
	  if (mm->split)
	    putlog(LOG_JOIN, chan->dname,
		   "Possible future nick collision: %s", mm->nick);
	  else
	    putlog(LOG_MISC, chan->dname,
		   "* Bug: nick change to existing nick");
	  killmember(chan, mm->nick);
	}
      }
      /*
       * Banned?
       */
      /* Compose a nick!user@host for the new nick */
      sprintf(s1, "%s!%s", msg, uhost);
      strcpy(m->nick, msg);
      memberlist_reposition(chan, m);
      detect_chan_flood(msg, uhost, from, chan, FLOOD_NICK, NULL);

      if (!findchan_by_dname(chname)) {
        chan = oldchan;
        continue;
      }
      /* don't fill the serverqueue with modes or kicks in a nickflood */
      if (chan_sentkick(m) || chan_sentdeop(m) || chan_sentop(m) ||
	  chan_sentdevoice(m) || chan_sentvoice(m))
	m->flags |= STOPCHECK;
      /* Any pending kick or mode to the old nick is lost. */
	m->flags &= ~(SENTKICK | SENTDEOP | SENTOP |
		      SENTVOICE | SENTDEVOICE);


      /* make sure they stay devoiced if EVOICE! */

      /* nick-ban or nick is +k or something? */
      if (!chan_stopcheck(m)) {
	get_user_flagrec(m->user ? m->user : get_user_by_host(s1), &fr, chan->dname);
	check_this_member(chan, m->nick, &fr);
      }
      u = get_user_by_host(from); /* make sure this is in the loop, someone could have changed the record
                                     in an earlier iteration of the loop */
      if (!findchan_by_dname(chname)) {
	chan = oldchan;
        continue;
      }
    }
  }
  return 0;
}

#ifdef S_SPLITHIJACK
void check_should_cycle(struct chanset_t *chan)
{
  /*
     If there are other ops split off and i'm the only op on this side of split, cycle
   */
  memberlist *ml = NULL;
  int localops = 0, localbotops = 0, splitops = 0, splitbotops = 0, localnonops = 0;

  if (!me_op(chan))
    return;
  for (ml = chan->channel.member; ml && ml->nick[0]; ml = ml->next) {
    if (chan_hasop(ml)) {
      if (chan_issplit(ml)) {
        splitops++;
        if ((ml->user) && (ml->user->flags & USER_BOT))
          splitbotops++;
      } else {
        localops++;
        if ((ml->user) && (ml->user->flags & USER_BOT))
          localbotops++;
        if (localbotops >= 2)
          return;
        if (localops > localbotops)
          return;
      }
    } else {
      if (!chan_issplit(ml))
        localnonops++;
    }
  }
  if (splitbotops > 5) {
    /* I'm only one opped here... and other side has some ops... so i'm cycling */
    if (localnonops) {
      /* need to unset any +kil first */
      dprintf(DP_MODE, "MODE %s -ilk %s\nPART %s\nJOIN %s\n", chan->name,
                            (chan->channel.key && chan->channel.key[0]) ? chan->channel.key : "",
                             chan->name, chan->name);
    } else
      dprintf(DP_MODE, "PART %s\nJOIN %s\n", chan->name, chan->name);
  }
}
#endif /* S_SPLITHIJACK */


/* Signoff, similar to part.
 */
static int gotquit(char *from, char *msg)
{
  char *nick = NULL, *chname = NULL, *p = NULL;
  int split = 0;
  char from2[NICKMAX + UHOSTMAX + 1] = "";
  memberlist *m = NULL;
  struct chanset_t *chan = NULL, *oldchan = NULL;
  struct userrec *u = NULL;

  strcpy(from2,from);
  u = get_user_by_host(from2);
  nick = splitnick(&from);
  fixcolon(msg);
  /* Fred1: Instead of expensive wild_match on signoff, quicker method.
   *        Determine if signoff string matches "%.% %.%", and only one
   *        space.
   */
  p = strchr(msg, ' ');
  if (p && (p == strrchr(msg, ' '))) {
    char *z1, *z2;

    *p = 0;
    z1 = strchr(p + 1, '.');
    z2 = strchr(msg, '.');
    if (z1 && z2 && (*(z1 + 1) != 0) && (z1 - 1 != p) &&
	(z2 + 1 != p) && (z2 != msg)) {
      /* Server split, or else it looked like it anyway (no harm in
       * assuming)
       */
      split = 1;
    } else
      *p = ' ';
  }
  for (chan = chanset; chan; chan = chan->next) {
    oldchan = chan;
    chname = chan->dname;
    m = ismember(chan, nick);
    if (m) {
      u = get_user_by_host(from2);
      if (u) {
        set_handle_laston(chan->dname, u, now); /* If you remove this, the bot will crash when the user record in question
						   is removed/modified during the tcl binds below, and the users was on more
						   than one monitored channel */
      }
      if (split) {
	m->split = now;
	if (!findchan_by_dname(chname)) {
          chan = oldchan;
	  continue;
        }
	putlog(LOG_JOIN, chan->dname, "%s (%s) got netsplit.", nick,
	       from);
      } else {
	if (!findchan_by_dname(chname)) {
	  chan = oldchan;
	  continue;
	}
	putlog(LOG_JOIN, chan->dname, "%s (%s) left irc: %s", nick,
	       from, msg);
	killmember(chan, nick);
	check_lonely_channel(chan);
      }
#ifdef S_SPLITHIJACK
      if (channel_cycle(chan))
        check_should_cycle(chan);
#endif /* S_SPLITHIJACK */
    }
  }
  /* Our nick quit? if so, grab it.
   */
  if (keepnick) {
    if (!rfc_casecmp(nick, origbotname)) {
      putlog(LOG_MISC, "*", IRC_GETORIGNICK, origbotname);
      dprintf(DP_SERVER, "NICK %s\n", origbotname);
    }
  }
  return 0;
}

/* Got a private message.
 */
static int gotmsg(char *from, char *msg)
{
  char *to = NULL, *realto = NULL, buf[UHOSTLEN] = "", *nick = NULL, buf2[512] = "", *uhost = buf;
  char *p = NULL, *p1 = NULL, *code = NULL, *ctcp = NULL;
  int ctcp_count = 0;
  struct chanset_t *chan = NULL;
  int ignoring;
  struct userrec *u = NULL;
  memberlist *m = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0};

  if (!strchr("&#!+@$", msg[0]))
    return 0;
  ignoring = match_ignore(from);
  to = newsplit(&msg);
  realto = (to[0] == '@') ? to + 1 : to;
  chan = findchan(realto);
  if (!chan)
    return 0;			/* Private msg to an unknown channel?? */
  fixcolon(msg);
  strcpy(uhost, from);
  nick = splitnick(&uhost);
  /* Only check if flood-ctcp is active */
  detect_autokick(nick, uhost, chan, msg);
  if (flud_ctcp_thr && detect_avalanche(msg)) {
    u = get_user_by_host(from);
    get_user_flagrec(u, &fr, chan->dname);
    m = ismember(chan, nick);
    /* Discard -- kick user if it was to the channel */
    if (m && me_op(chan) && 
	!chan_sentkick(m) &&
	!(use_exempts && ban_fun &&
	  /* don't kickban if permanent exempted -- Eule */
	  (u_match_mask(global_exempts, from) ||
	   u_match_mask(chan->exempts, from)))) {
      if (ban_fun) {
	check_exemptlist(chan, from);
	u_addmask('b', chan, quickban(chan, uhost), conf.bot->nick,
               IRC_FUNKICK, now + (60 * chan->ban_time), 0);
      }
      if (kick_fun) {
	/* This can induce kickflood - arthur2 */
	dprintf(DP_SERVER, "KICK %s %s :%s%s\n", chan->name, nick,
		kickprefix, IRC_FUNKICK);
	m->flags |= SENTKICK;
      }
    }
    if (!ignoring) {
      putlog(LOG_MODES, "*", "Avalanche from %s!%s in %s - ignoring",
	     nick, uhost, chan->dname);
      p = strchr(uhost, '@');
      if (p)
	p++;
      else
	p = uhost;
      simple_sprintf(buf2, "*!*@%s", p);
      addignore(buf2, conf.bot->nick, "ctcp avalanche", now + (60 * ignore_time));
    }
    return 0;
  }
  /* Check for CTCP: */
  ctcp_reply[0] = 0;
  p = strchr(msg, 1);
  while (p && *p) {
    p++;
    p1 = p;
    while ((*p != 1) && *p)
      p++;
    if (*p == 1) {
      *p = 0;
      ctcp = buf2;
      strcpy(ctcp, p1);
      strcpy(p1 - 1, p + 1);
      detect_chan_flood(nick, uhost, from, chan,
			strncmp(ctcp, "ACTION ", 7) ?
			FLOOD_CTCP : FLOOD_PRIVMSG, NULL);

      chan = findchan(realto);
      if (!chan)
        return 0;

      /* Respond to the first answer_ctcp */
      p = strchr(msg, 1);
      if (ctcp_count < answer_ctcp) {
	ctcp_count++;
	if (ctcp[0] != ' ') {
	  code = newsplit(&ctcp);
	  u = get_user_by_host(from);
	  if (!ignoring || trigger_on_ignore) {
	    if (check_bind_ctcp(nick, uhost, u, to, code, ctcp) == BIND_RET_LOG) {

	      chan = findchan(realto); 
	      if (!chan)
		return 0;

	      update_idle(chan->dname, nick);
            }
	    if (!ignoring) {
	      /* Log DCC, it's to a channel damnit! */
	      if (!strcmp(code, "ACTION")) {
		putlog(LOG_PUBLIC, chan->dname, "Action: %s %s", nick, ctcp);
	      } else {
		putlog(LOG_PUBLIC, chan->dname,
		       "CTCP %s: %s from %s (%s) to %s", code, ctcp, nick,
		       from, to);
	      }
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
    int dolog = 1, botmatch = 0;
#ifdef S_AUTHCMDS
    int i = 0;
    char *my_msg = NULL, *my_ptr = NULL, *fword = NULL;
#endif /* S_AUTHCMDS */
    /* Check even if we're ignoring the host. (modified by Eule 17.7.99) */
    detect_chan_flood(nick, uhost, from, chan, FLOOD_PRIVMSG, NULL);
    
    chan = findchan(realto);
    if (!chan)
      return 0;
#ifdef S_AUTHCMDS
    my_msg = my_ptr = strdup(msg);

    fword = newsplit(&my_msg);		/* take out first word */
    
    /* the first word is a wildcard match to our nick. */
    botmatch = wild_match(fword, botname);
    if (botmatch && strcmp(fword, "*"))	
      fword = newsplit(&my_msg);	/* now this will be the command */
    /* is it a cmd? */
    if (fword && fword[0] && fword[1] && ((botmatch && fword[0] != cmdprefix) || (fword[0] == cmdprefix))) {
      i = findauth(uhost);
      if (i > -1 && auth[i].authed) {
        if (fword[0] == cmdprefix)
          fword++;
        if (check_bind_pubc(fword, nick, uhost, auth[i].user, my_msg, chan->dname)) {
          auth[i].atime = now;
          dolog--; 		/* don't log */
        }
      }
    }
    free(my_ptr);
#endif /* S_AUTHCMDS */
    if (dolog) {
      if (!ignoring) {
        if (to[0] == '@')
          putlog(LOG_PUBLIC, chan->dname, "@<%s> %s", nick, msg);
        else
          putlog(LOG_PUBLIC, chan->dname, "<%s> %s", nick, msg);
      }
    }
    update_idle(chan->dname, nick);
  }
  return 0;
}

/* Got a private notice.
 */
static int gotnotice(char *from, char *msg)
{
  char *to = NULL, *realto = NULL, *nick = NULL, buf2[512] = "", *p = NULL, *p1 = NULL, buf[512] = "", *uhost = buf;
  char *ctcp = NULL, *code = NULL;
  struct userrec *u = NULL;
  memberlist *m = NULL;
  struct chanset_t *chan = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0};
  int ignoring;

  if (!strchr(CHANMETA "@", *msg))
    return 0;
  ignoring = match_ignore(from);
  to = newsplit(&msg);
  realto = (*to == '@') ? to + 1 : to;
  chan = findchan(realto);
  if (!chan)
    return 0;			/* Notice to an unknown channel?? */
  fixcolon(msg);
  strcpy(uhost, from);
  nick = splitnick(&uhost);
  u = get_user_by_host(from);
  if (flud_ctcp_thr && detect_avalanche(msg)) {
    get_user_flagrec(u, &fr, chan->dname);
    m = ismember(chan, nick);
    /* Discard -- kick user if it was to the channel */
    if (me_op(chan) && m && !chan_sentkick(m) &&
	!(use_exempts && ban_fun &&
	  /* don't kickban if permanent exempted -- Eule */
	  (u_match_mask(global_exempts,from) ||
	   u_match_mask(chan->exempts, from)))) {
      if (ban_fun) {
	check_exemptlist(chan, from);
	u_addmask('b', chan, quickban(chan, uhost), conf.bot->nick,
               IRC_FUNKICK, now + (60 * chan->ban_time), 0);
      }
      if (kick_fun) {
	/* This can induce kickflood - arthur2 */
	dprintf(DP_SERVER, "KICK %s %s :%s%s\n", chan->name, nick,
		kickprefix, IRC_FUNKICK);
	m->flags |= SENTKICK;
      }
    }
    if (!ignoring)
      putlog(LOG_MODES, "*", "Avalanche from %s", from);
    return 0;
  }
  /* Check for CTCP: */
  p = strchr(msg, 1);
  while (p && *p) {
    p++;
    p1 = p;
    while ((*p != 1) && *p)
      p++;
    if (*p == 1) {
      *p = 0;
      ctcp = buf2;
      strcpy(ctcp, p1);
      strcpy(p1 - 1, p + 1);
      p = strchr(msg, 1);
      detect_chan_flood(nick, uhost, from, chan,
			strncmp(ctcp, "ACTION ", 7) ?
			FLOOD_CTCP : FLOOD_PRIVMSG, NULL);

      chan = findchan(realto); 
      if (!chan)
	return 0;

      if (ctcp[0] != ' ') {
	code = newsplit(&ctcp);
	if (!ignoring || trigger_on_ignore) {
	  check_bind_ctcr(nick, uhost, u, chan->dname, code, msg);

	  chan = findchan(realto); 
	  if (!chan)
	    return 0;

	  if (!ignoring) {
	    putlog(LOG_PUBLIC, chan->dname, "CTCP reply %s: %s from %s (%s) to %s",
		   code, msg, nick, from, chan->dname);
	    update_idle(chan->dname, nick);
	  }
	}
      }
    }
  }
  if (msg[0]) {
    /* Check even if we're ignoring the host. (modified by Eule 17.7.99) */
    detect_chan_flood(nick, uhost, from, chan, FLOOD_NOTICE, NULL);

    chan = findchan(realto); 
    if (!chan)
      return 0;

    if (!ignoring || trigger_on_ignore) {
      chan = findchan(realto); 
      if (!chan)
	return 0;
    }

    if (!ignoring)
      putlog(LOG_PUBLIC, chan->dname, "-%s:%s- %s", nick, to, msg);
    update_idle(chan->dname, nick);
  }
  return 0;
}

static cmd_t irc_raw[] =
{
  {"324",	"",	(Function) got324,	"irc:324"},
  {"352",	"",	(Function) got352,	"irc:352"},
  {"354",	"",	(Function) got354,	"irc:354"},
  {"315",	"",	(Function) got315,	"irc:315"},
  {"367",	"",	(Function) got367,	"irc:367"},
  {"368",	"",	(Function) got368,	"irc:368"},
  {"403",	"",	(Function) got403,	"irc:403"},
  {"405",	"",	(Function) got405,	"irc:405"},
  {"471",	"",	(Function) got471,	"irc:471"},
  {"473",	"",	(Function) got473,	"irc:473"},
  {"474",	"",	(Function) got474,	"irc:474"},
  {"475",	"",	(Function) got475,	"irc:475"},
  {"INVITE",	"",	(Function) gotinvite,	"irc:invite"},
  {"TOPIC",	"",	(Function) gottopic,	"irc:topic"},
  {"331",	"",	(Function) got331,	"irc:331"},
  {"332",	"",	(Function) got332,	"irc:332"},
  {"JOIN",	"",	(Function) gotjoin,	"irc:join"},
  {"PART",	"",	(Function) gotpart,	"irc:part"},
  {"KICK",	"",	(Function) gotkick,	"irc:kick"},
  {"NICK",	"",	(Function) gotnick,	"irc:nick"},
  {"QUIT",	"",	(Function) gotquit,	"irc:quit"},
  {"PRIVMSG",	"",	(Function) gotmsg,	"irc:msg"},
  {"NOTICE",	"",	(Function) gotnotice,	"irc:notice"},
  {"MODE",	"",	(Function) gotmode,	"irc:mode"},
  {"346",	"",	(Function) got346,	"irc:346"},
  {"347",	"",	(Function) got347,	"irc:347"},
  {"348",	"",	(Function) got348,	"irc:348"},
  {"349",	"",	(Function) got349,	"irc:349"},
  {NULL,	NULL,	NULL,			NULL}
};
#endif /* LEAF */
