/*
 * userchan.c -- part of channels.mod
 *
 */

extern struct cmd_pass *cmdpass;

struct chanuserrec *get_chanrec(struct userrec *u, char *chname)
{
  for (register struct chanuserrec *ch = u->chanrec; ch; ch = ch->next) 
    if (!rfc_casecmp(ch->channel, chname))
      return ch;
  return NULL;
}

struct chanuserrec *add_chanrec(struct userrec *u, char *chname)
{
  if (findchan_by_dname(chname)) {
    struct chanuserrec *ch = (struct chanuserrec *) my_calloc(1, sizeof(struct chanuserrec));

    ch->next = u->chanrec;
    u->chanrec = ch;
    ch->info = NULL;
    ch->flags = 0;
    ch->laston = 0;
    strncpy(ch->channel, chname, 81);
    ch->channel[80] = 0;
    if (!noshare)
      shareout("+cr %s %s\n", u->handle, chname);
    return ch;
  }
  return NULL;
}

void add_chanrec_by_handle(struct userrec *bu, char *hand, char *chname)
{
  struct userrec *u = get_user_by_handle(bu, hand);

  if (!u)
    return;
  if (!get_chanrec(u, chname))
    add_chanrec(u, chname);
}

void get_handle_chaninfo(char *handle, char *chname, char *s)
{
  struct userrec *u = get_user_by_handle(userlist, handle);

  if (u == NULL) {
    s[0] = 0;
    return;
  }

  struct chanuserrec *ch = get_chanrec(u, chname);

  if (ch == NULL) {
    s[0] = 0;
    return;
  }
  if (ch->info == NULL) {
    s[0] = 0;
    return;
  }
  strcpy(s, ch->info);
  return;
}

void set_handle_chaninfo(struct userrec *bu, char *handle, char *chname, char *info)
{
  struct userrec *u = get_user_by_handle(bu, handle);

  if (!u)
    return;

  struct chanuserrec *ch = get_chanrec(u, chname);

  if (!ch) {
    add_chanrec_by_handle(bu, handle, chname);
    ch = get_chanrec(u, chname);
  }
  if (info) {
    if (strlen(info) > 80)
      info[80] = 0;
  }
  if (ch->info != NULL)
    free(ch->info);
  if (info && info[0]) {
    ch->info = strdup(info);
  } else
    ch->info = NULL;

  if ((!noshare) && (bu == userlist)) {
    shareout("chchinfo %s %s %s\n", handle, chname, info ? info : "");
  }
}

void del_chanrec(struct userrec *u, char *chname)
{
  struct chanuserrec *ch = u->chanrec, *lst = NULL;

  while (ch) {
    if (!rfc_casecmp(chname, ch->channel)) {
      if (lst == NULL)
	u->chanrec = ch->next;
      else
	lst->next = ch->next;
      if (ch->info != NULL)
	free(ch->info);
      free(ch);
      if (!noshare)
	shareout("-cr %s %s\n", u->handle, chname);
      return;
    }
    lst = ch;
    ch = ch->next;
  }
}

void set_handle_laston(char *chan, struct userrec *u, time_t n)
{
  if (!u)
    return;
  touch_laston(u, chan, n);

  struct chanuserrec *ch = get_chanrec(u, chan);

  if (!ch)
    return;
  ch->laston = n;
}

/* Is this mask sticky?
 */
int u_sticky_mask(maskrec *u, char *uhost)
{
  for (; u; u = u->next)
    if (!rfc_casecmp(u->mask, uhost))
      return (u->flags & MASKREC_STICKY);
  return 0;
}

/* Set sticky attribute for a mask.
 */
int u_setsticky_mask(struct chanset_t *chan, maskrec *u, char *uhost, bool sticky, const char *botcmd)
{
  int j;

  if (str_isdigit(uhost))
    j = atoi(uhost);
  else
    j = (-1);

  while(u) {
    if (j >= 0)
      j--;

    if (!j || ((j < 0) && !rfc_casecmp(u->mask, uhost))) {
      if (sticky > 0)
	u->flags |= MASKREC_STICKY;
      else if (!sticky)
	u->flags &= ~MASKREC_STICKY;
      else				/* We don't actually want to change, just skip over */
	return 0;
      if (!j)
	strcpy(uhost, u->mask);

      if (!noshare)
        shareout("%s %s %d %s\n", botcmd, uhost, sticky, (chan) ? chan->dname : "");
      return 1;
    }

    u = u->next;
  }

  if (j >= 0)
    return -j;

  return 0;
}

/* Merge of u_equals_ban(), u_equals_exempt() and u_equals_invite().
 *
 * Returns:
 *   0       not a ban
 *   1       temporary ban
 *   2       perm ban
 */
int u_equals_mask(maskrec *u, char *mask)
{
  for (; u; u = u->next)
    if (!rfc_casecmp(u->mask, mask)) {
      if (u->flags & MASKREC_PERM)
        return 2;
      else
        return 1;
    }
  return 0;
}

bool u_match_mask(maskrec *rec, char *mask)
{
  for (; rec; rec = rec->next)
    if (wild_match(rec->mask, mask))
      return 1;
  return 0;
}

int u_delmask(char type, struct chanset_t *c, char *who, int doit)
{
  int j, i = 0;
  char temp[256] = "";
  maskrec **u = NULL, *t;

  if (type == 'b')
    u = c ? &c->bans : &global_bans;
  if (type == 'e')
    u = c ? &c->exempts : &global_exempts;
  if (type == 'I')
    u = c ? &c->invites : &global_invites;

  if (!strchr(who, '!') && str_isdigit(who)) {
    j = atoi(who);
    j--;
    for (; (*u) && j; u = &((*u)->next), j--);
    if (*u) {
      strlcpy(temp, (*u)->mask, sizeof temp);
      i = 1;
    } else
      return -j - 1;
  } else {
    /* Find matching host, if there is one */
    for (; *u && !i; u = &((*u)->next))
      if (!rfc_casecmp((*u)->mask, who)) {
        strlcpy(temp, who, sizeof temp);
	i = 1;
	break;
      }
    if (!*u)
      return 0;
  }
  if (i && doit) {
    if (!noshare) {
      char *mask = str_escape(temp, ':', '\\');

      if (mask) {
	/* Distribute chan bans differently */
	if (c)
	  shareout("-%s %s %s\n", type == 'b' ? "bc" : type == 'e' ? "ec" : "invc", c->dname, mask);
	else
	  shareout("-%s %s\n", type == 'b' ? "b" : type == 'e' ? "e" : "inv", mask);
	free(mask);
      }
    }
    if (lastdeletedmask)
      free(lastdeletedmask);
    lastdeletedmask = (*u)->mask;
    if ((*u)->desc)
      free((*u)->desc);
    if ((*u)->user)
      free((*u)->user);
    t = *u;
    *u = (*u)->next;
    free(t);
  }
  return i;
}

/* Note: If first char of note is '*' it's a sticky mask.
 */
bool u_addmask(char type, struct chanset_t *chan, char *who, char *from, char *note, time_t expire_time, int flags)
{
  char host[1024] = "", s[1024] = "";
  maskrec *p = NULL, *l = NULL, **u = NULL;

  if (type == 'b')
    u = chan ? &chan->bans : &global_bans;
  if (type == 'e')
    u = chan ? &chan->exempts : &global_exempts;
  if (type == 'I')
    u = chan ? &chan->invites : &global_invites;

  strcpy(host, who);
  /* Choke check: fix broken bans (must have '!' and '@') */
  if ((strchr(host, '!') == NULL) && (strchr(host, '@') == NULL))
    strcat(host, "!*@*");
  else if (strchr(host, '@') == NULL)
    strcat(host, "@*");
  else if (strchr(host, '!') == NULL) {
    char *i = strchr(host, '@');

    strcpy(s, i);
    *i = 0;
    strcat(host, "!*");
    strcat(host, s);
  }
#ifdef LEAF
    simple_sprintf(s, "%s!%s", botname, botuserhost);
#else
    simple_sprintf(s, "%s!%s@%s", origbotname, botuser, conf.bot->net.host);
#endif /* LEAF */
  if (s[0] && type == 'b' && wild_match(host, s)) {
    putlog(LOG_MISC, "*", IRC_IBANNEDME);
    return 0;
  }
  if (expire_time == now)
    return 1;

  for (l = *u; l; l = l->next)
    if (!rfc_casecmp(l->mask, host)) {
      p = l;
      break;
    }
    
  /* It shouldn't expire and be sticky also */
  if (note[0] == '*') {
    flags |= MASKREC_STICKY;
    note++;
  }
  if ((expire_time == 0L) || (flags & MASKREC_PERM)) {
    flags |= MASKREC_PERM;
    expire_time = 0L;
  }

  if (p == NULL) {
    p = (maskrec *) my_calloc(1, sizeof(maskrec));
    p->next = *u;
    *u = p;
  }
  else {
    free( p->mask );
    free( p->user );
    free( p->desc );
  }
  p->expire = expire_time;
  p->added = now;
  p->lastactive = 0;
  p->flags = flags;
  p->mask = strdup(host);
  p->user = strdup(from);
  p->desc = strdup(note);
  if (!noshare) {
    char *mask = str_escape(host, ':', '\\');

    if (mask) {
      if (!chan)
	shareout("+%s %s %lu %s%s %s %s\n",
		 type == 'b' ? "b" : type == 'e' ? "e" : "inv",
		 mask, expire_time - now,
		 (flags & MASKREC_STICKY) ? "s" : "",
		 (flags & MASKREC_PERM) ? "p" : "-", from, note);
      else
	shareout("+%s %s %lu %s %s%s %s %s\n",
		 type == 'b' ? "bc" : type == 'e' ? "ec" : "invc",	
		 mask, expire_time - now,
		 chan->dname, (flags & MASKREC_STICKY) ? "s" : "",
		 (flags & MASKREC_PERM) ? "p" : "-", from, note);
      free(mask);
    }
  }
  return 1;
}

/* Take host entry from ban list and display it ban-style.
 */
static void display_ban(int idx, int number, maskrec *ban, struct chanset_t *chan, bool show_inact)
{
  char dates[81] = "", s[41] = "";

  if (ban->added) {
    daysago(now, ban->added, s);
    sprintf(dates, "%s %s", MODES_CREATED, s);
    if (ban->added < ban->lastactive) {
      strcat(dates, ", ");
      strcat(dates, MODES_LASTUSED);
      strcat(dates, " ");
      daysago(now, ban->lastactive, s);
      strcat(dates, s);
    }
  } else
    dates[0] = 0;
  if (ban->flags & MASKREC_PERM)
    strcpy(s, "(perm)");
  else {
    char s1[41] = "";

    days(ban->expire, now, s1);
    sprintf(s, "(expires %s)", s1);
  }
  if (ban->flags & MASKREC_STICKY)
    strcat(s, " (sticky)");
  if (!chan || ischanban(chan, ban->mask)) {
    if (number >= 0) {
      dprintf(idx, "  [%3d] %s %s\n", number, ban->mask, s);
    } else {
      dprintf(idx, "BAN: %s %s\n", ban->mask, s);
    }
  } else if (show_inact) {
    if (number >= 0) {
      dprintf(idx, "! [%3d] %s %s\n", number, ban->mask, s);
    } else {
      dprintf(idx, "BAN (%s): %s %s\n", MODES_INACTIVE, ban->mask, s);
    }
  } else
    return;
  dprintf(idx, "        %s: %s\n", ban->user, ban->desc);
  if (dates[0])
    dprintf(idx, "        %s\n", dates);
}

/* Take host entry from exempt list and display it ban-style.
 */
static void display_exempt(int idx, int number, maskrec *exempt, struct chanset_t *chan, bool show_inact)
{
  char dates[81] = "", s[41] = "";

  if (exempt->added) {
    daysago(now, exempt->added, s);
    sprintf(dates, "%s %s", MODES_CREATED, s);
    if (exempt->added < exempt->lastactive) {
      strcat(dates, ", ");
      strcat(dates, MODES_LASTUSED);
      strcat(dates, " ");
      daysago(now, exempt->lastactive, s);
      strcat(dates, s);
    }
  } else
    dates[0] = 0;
  if (exempt->flags & MASKREC_PERM)
    strcpy(s, "(perm)");
  else {
    char s1[41] = "";

    days(exempt->expire, now, s1);
    sprintf(s, "(expires %s)", s1);
  }
  if (exempt->flags & MASKREC_STICKY)
    strcat(s, " (sticky)");
  if (!chan || ischanexempt(chan, exempt->mask)) {
    if (number >= 0) {
      dprintf(idx, "  [%3d] %s %s\n", number, exempt->mask, s);
    } else {
      dprintf(idx, "EXEMPT: %s %s\n", exempt->mask, s);
    }
  } else if (show_inact) {
    if (number >= 0) {
      dprintf(idx, "! [%3d] %s %s\n", number, exempt->mask, s);
    } else {
      dprintf(idx, "EXEMPT (%s): %s %s\n", MODES_INACTIVE, exempt->mask, s);
    }
  } else
    return;
  dprintf(idx, "        %s: %s\n", exempt->user, exempt->desc);
  if (dates[0])
    dprintf(idx, "        %s\n", dates);
}

/* Take host entry from invite list and display it ban-style.
 */
static void display_invite (int idx, int number, maskrec *invite, struct chanset_t *chan, bool show_inact)
{
  char dates[81] = "", s[41] = "";

  if (invite->added) {
    daysago(now, invite->added, s);
    sprintf(dates, "%s %s", MODES_CREATED, s);
    if (invite->added < invite->lastactive) {
      strcat(dates, ", ");
      strcat(dates, MODES_LASTUSED);
      strcat(dates, " ");
      daysago(now, invite->lastactive, s);
      strcat(dates, s);
    }
  } else
    dates[0] = 0;
  if (invite->flags & MASKREC_PERM)
    strcpy(s, "(perm)");
  else {
    char s1[41] = "";

    days(invite->expire, now, s1);
    sprintf(s, "(expires %s)", s1);
  }
  if (invite->flags & MASKREC_STICKY)
    strcat(s, " (sticky)");
  if (!chan || ischaninvite(chan, invite->mask)) {
    if (number >= 0) {
      dprintf(idx, "  [%3d] %s %s\n", number, invite->mask, s);
    } else {
      dprintf(idx, "INVITE: %s %s\n", invite->mask, s);
    }
  } else if (show_inact) {
    if (number >= 0) {
      dprintf(idx, "! [%3d] %s %s\n", number, invite->mask, s);
    } else {
      dprintf(idx, "INVITE (%s): %s %s\n", MODES_INACTIVE, invite->mask, s);
    }
  } else
    return;
  dprintf(idx, "        %s: %s\n", invite->user, invite->desc);
  if (dates[0])
    dprintf(idx, "        %s\n", dates);
}

static void tell_bans(int idx, bool show_inact, char *match)
{
  int k = 1;
  char *chname = NULL;
  struct chanset_t *chan = NULL;
  maskrec *u = NULL;

  /* Was a channel given? */
  if (match[0]) {
    chname = newsplit(&match);
    if (chname[0] && (strchr(CHANMETA, chname[0]))) {
      chan = findchan_by_dname(chname);
      if (!chan) {
	dprintf(idx, "%s.\n", CHAN_NOSUCH);
	return;
      }
    } else
      match = chname;
  }

  /* don't return here, we want to show global bans even if no chan */
  if (!chan && !(chan = findchan_by_dname(dcc[idx].u.chat->con_chan)) &&
      !(chan = chanset))
    chan = NULL;
  get_user_flagrec(dcc[idx].user, &user, chan->dname);
  if (privchan(user, chan, PRIV_OP)) {
    dprintf(idx, "%s.\n", CHAN_NOSUCH);
    return;
  }

  if (chan && show_inact)
    dprintf(idx, "%s:   (! = %s %s)\n", BANS_GLOBAL,
	    MODES_NOTACTIVE, chan->dname);
  else
    dprintf(idx, "%s:\n", BANS_GLOBAL);
  for (u = global_bans; u; u = u->next) {
    if (match[0]) {
      if ((wild_match(match, u->mask)) ||
	  (wild_match(match, u->desc)) ||
	  (wild_match(match, u->user)))
	display_ban(idx, k, u, chan, 1);
      k++;
    } else
      display_ban(idx, k++, u, chan, show_inact);
  }
  if (chan) {
    if (show_inact)
      dprintf(idx, "%s %s:   (! = %s, * = %s)\n",
	      BANS_BYCHANNEL, chan->dname,
	      MODES_NOTACTIVE2, MODES_NOTBYBOT);
    else
      dprintf(idx, "%s %s:  (* = %s)\n",
	      BANS_BYCHANNEL, chan->dname,
	      MODES_NOTBYBOT);
    for (u = chan->bans; u; u = u->next) {
      if (match[0]) {
	if ((wild_match(match, u->mask)) ||
	    (wild_match(match, u->desc)) ||
	    (wild_match(match, u->user)))
	  display_ban(idx, k, u, chan, 1);
	k++;
      } else
	display_ban(idx, k++, u, chan, show_inact);
    }
    if (chan->status & CHAN_ACTIVE) {
      masklist *b = NULL;
      char s[UHOSTLEN] = "", *s1 = NULL, *s2 = NULL, fill[256] = "";
      int min, sec;

      for (b = chan->channel.ban; b && b->mask[0]; b = b->next) {    
	if ((!u_equals_mask(global_bans, b->mask)) &&
	    (!u_equals_mask(chan->bans, b->mask))) {
	  strcpy(s, b->who);
	  s2 = s;
	  s1 = splitnick(&s2);
	  if (s1[0])
	    sprintf(fill, "%s (%s!%s)", b->mask, s1, s2);
	  else
	    sprintf(fill, "%s (server %s)", b->mask, s2);
	  if (b->timer != 0) {
	    min = (now - b->timer) / 60;
	    sec = (now - b->timer) - (min * 60);
	    sprintf(s, " (active %02d:%02d)", min, sec);
	    strcat(fill, s);
	  }
	  if ((!match[0]) || (wild_match(match, b->mask)))
	    dprintf(idx, "* [%3d] %s\n", k, fill);
	  k++;
	}
      }
    }
  }
  if (k == 1)
    dprintf(idx, "(There are no bans, permanent or otherwise.)\n");
  if ((!show_inact) && (!match[0]))
    dprintf(idx, "%s.\n", BANS_USEBANSALL);
}

static void tell_exempts(int idx, bool show_inact, char *match)
{
  int k = 1;
  char *chname = NULL;
  struct chanset_t *chan = NULL;
  maskrec *u = NULL;

  /* Was a channel given? */
  if (match[0]) {
    chname = newsplit(&match);
    if (chname[0] && strchr(CHANMETA, chname[0])) {
      chan = findchan_by_dname(chname);
      if (!chan) {
	dprintf(idx, "%s.\n", CHAN_NOSUCH);
	return;
      }
    } else
      match = chname;
  }

  /* don't return here, we want to show global exempts even if no chan */
  if (!chan && !(chan = findchan_by_dname(dcc[idx].u.chat->con_chan))
      && !(chan = chanset))
    chan = NULL;

  get_user_flagrec(dcc[idx].user, &user, chan->dname);
  if (privchan(user, chan, PRIV_OP)) {
    dprintf(idx, "%s.\n", CHAN_NOSUCH);
    return;
  }
  if (chan && show_inact)
    dprintf(idx, "%s:   (! = %s %s)\n", EXEMPTS_GLOBAL,
	    MODES_NOTACTIVE, chan->dname);
  else
    dprintf(idx, "%s:\n", EXEMPTS_GLOBAL);
  for (u = global_exempts; u; u = u->next) {
    if (match[0]) {
      if ((wild_match(match, u->mask)) ||
	  (wild_match(match, u->desc)) ||
	  (wild_match(match, u->user)))
	display_exempt(idx, k, u, chan, 1);
      k++;
    } else
      display_exempt(idx, k++, u, chan, show_inact);
  }
  if (chan) {
    if (show_inact)
      dprintf(idx, "%s %s:   (! = %s, * = %s)\n",
	      EXEMPTS_BYCHANNEL, chan->dname,
	      MODES_NOTACTIVE2,
	      MODES_NOTBYBOT);
    else
      dprintf(idx, "%s %s:  (* = %s)\n",
	      EXEMPTS_BYCHANNEL, chan->dname,
	      MODES_NOTBYBOT);
    for (u = chan->exempts; u; u = u->next) {
      if (match[0]) {
	if ((wild_match(match, u->mask)) ||
	    (wild_match(match, u->desc)) ||
	    (wild_match(match, u->user)))
	  display_exempt(idx, k, u, chan, 1);
	k++;
      } else
	display_exempt(idx, k++, u, chan, show_inact);
    }
    if (chan->status & CHAN_ACTIVE) {
      masklist *e = NULL;
      char s[UHOSTLEN] = "", *s1 = NULL, *s2 = NULL , fill[256] = "";
      int min, sec;

      for (e = chan->channel.exempt; e && e->mask[0]; e = e->next) {
	if ((!u_equals_mask(global_exempts,e->mask)) &&
	    (!u_equals_mask(chan->exempts, e->mask))) {
	  strcpy(s, e->who);
	  s2 = s;
	  s1 = splitnick(&s2);
	  if (s1[0])
	    sprintf(fill, "%s (%s!%s)", e->mask, s1, s2);
	  else
	    sprintf(fill, "%s (server %s)", e->mask, s2);
	  if (e->timer != 0) {
	    min = (now - e->timer) / 60;
	    sec = (now - e->timer) - (min * 60);
	    sprintf(s, " (active %02d:%02d)", min, sec);
	    strcat(fill, s);
	  }
	  if ((!match[0]) || (wild_match(match, e->mask)))
	    dprintf(idx, "* [%3d] %s\n", k, fill);
	  k++;
	}
      }
    }
  }
  if (k == 1)
    dprintf(idx, "(There are no ban exempts, permanent or otherwise.)\n");
  if ((!show_inact) && (!match[0]))
    dprintf(idx, "%s.\n", EXEMPTS_USEEXEMPTSALL);
}

static void tell_invites(int idx, bool show_inact, char *match)
{
  int k = 1;
  char *chname = NULL;
  struct chanset_t *chan = NULL;
  maskrec *u = NULL;

  /* Was a channel given? */
  if (match[0]) {
    chname = newsplit(&match);
    if (chname[0] && strchr(CHANMETA, chname[0])) {
      chan = findchan_by_dname(chname);
      if (!chan) {
	dprintf(idx, "%s.\n", CHAN_NOSUCH);
	return;
      }
    } else
      match = chname;
  }

  /* don't return here, we want to show global invites even if no chan */
  if (!chan && !(chan = findchan_by_dname(dcc[idx].u.chat->con_chan))
      && !(chan = chanset))
    chan = NULL;

  get_user_flagrec(dcc[idx].user, &user, chan->dname);
  if (privchan(user, chan, PRIV_OP)) {
    dprintf(idx, "%s.\n", CHAN_NOSUCH);
    return;
  }
  if (chan && show_inact)
    dprintf(idx, "%s:   (! = %s %s)\n", INVITES_GLOBAL,
	    MODES_NOTACTIVE, chan->dname);
  else
    dprintf(idx, "%s:\n", INVITES_GLOBAL);
  for (u = global_invites; u; u = u->next) {
    if (match[0]) {
      if ((wild_match(match, u->mask)) ||
	  (wild_match(match, u->desc)) ||
	  (wild_match(match, u->user)))
	display_invite(idx, k, u, chan, 1);
      k++;
    } else
      display_invite(idx, k++, u, chan, show_inact);
  }
  if (chan) {
    if (show_inact)
      dprintf(idx, "%s %s:   (! = %s, * = %s)\n",
	      INVITES_BYCHANNEL, chan->dname,
	      MODES_NOTACTIVE2,
	      MODES_NOTBYBOT);
    else
      dprintf(idx, "%s %s:  (* = %s)\n",
	      INVITES_BYCHANNEL, chan->dname,
	      MODES_NOTBYBOT);
    for (u = chan->invites; u; u = u->next) {
      if (match[0]) {
	if ((wild_match(match, u->mask)) ||
	    (wild_match(match, u->desc)) ||
	    (wild_match(match, u->user)))
	  display_invite(idx, k, u, chan, 1);
	k++;
      } else
	display_invite(idx, k++, u, chan, show_inact);
    }
    if (chan->status & CHAN_ACTIVE) {
      masklist *i = NULL;
      char s[UHOSTLEN] = "", *s1 = NULL, *s2 = NULL, fill[256] = "";
      int min, sec;

      for (i = chan->channel.invite; i && i->mask[0]; i = i->next) {
	if ((!u_equals_mask(global_invites,i->mask)) &&
	    (!u_equals_mask(chan->invites, i->mask))) {
	  strcpy(s, i->who);
	  s2 = s;
	  s1 = splitnick(&s2);
	  if (s1[0])
	    sprintf(fill, "%s (%s!%s)", i->mask, s1, s2);
	  else
	    sprintf(fill, "%s (server %s)", i->mask, s2);
	  if (i->timer != 0) {
	    min = (now - i->timer) / 60;
	    sec = (now - i->timer) - (min * 60);
	    sprintf(s, " (active %02d:%02d)", min, sec);
	    strcat(fill, s);
	  }
	  if ((!match[0]) || (wild_match(match, i->mask)))
	    dprintf(idx, "* [%3d] %s\n", k, fill);
	  k++;
	}
      }
    }
  }
  if (k == 1)
    dprintf(idx, "(There are no invites, permanent or otherwise.)\n");
  if ((!show_inact) && (!match[0]))
    dprintf(idx, "%s.\n", INVITES_USEINVITESALL);
}

bool write_config(FILE *f, int idx)
{
  putlog(LOG_DEBUG, "@", "Writing config entries...");
  if (lfprintf(f, CONFIG_NAME " - -\n") == EOF) /* Daemus */
      return 0;
  for (int i = 0; i < cfg_count; i++) {
    if ((cfg[i]->flags & CFGF_GLOBAL) && (cfg[i]->gdata)) {
      if (lfprintf(f, "@ %s %s\n", cfg[i]->name, cfg[i]->gdata ? cfg[i]->gdata : "") == EOF)
        return 0;
    }
  }

  for (struct cmd_pass *cp = cmdpass; cp; cp = cp->next)
    if (lfprintf(f, "- %s %s\n", cp->name, cp->pass) == EOF)
      return 0;

  return 1;
}
/* Write the ban lists and the ignore list to a file.
 */
bool write_bans(FILE *f, int idx)
{
  if (global_ign)
    if (lfprintf(f, IGNORE_NAME " - -\n") == EOF)	/* Daemus */
      return 0;

  char *mask = NULL;

  for (struct igrec *i = global_ign; i; i = i->next) {
    mask = str_escape(i->igmask, ':', '\\');
    if (!mask ||
	lfprintf(f, "- %s:%s%lu:%s:%lu:%s\n", mask,
		(i->flags & IGREC_PERM) ? "+" : "", i->expire,
		i->user ? i->user : conf.bot->nick, i->added,
		i->msg ? i->msg : "") == EOF) {
      if (mask)
	free(mask);
      return 0;
    }
    free(mask);
  }
  if (global_bans)
    if (lfprintf(f, BAN_NAME " - -\n") == EOF)	/* Daemus */
      return 0;

  maskrec *b = NULL;

  for (b = global_bans; b; b = b->next) {
    mask = str_escape(b->mask, ':', '\\');
    if (!mask ||
	lfprintf(f, "- %s:%s%lu%s:+%lu:%lu:%s:%s\n", mask,
		(b->flags & MASKREC_PERM) ? "+" : "", b->expire,
		(b->flags & MASKREC_STICKY) ? "*" : "", b->added,
		b->lastactive, b->user ? b->user : conf.bot->nick,
		b->desc ? b->desc : "requested") == EOF) {
      if (mask)
	free(mask);
      return 0;
    }
    free(mask);
  }
  for (struct chanset_t *chan = chanset; chan; chan = chan->next) {
    if (lfprintf(f, "::%s bans\n", chan->dname) == EOF)
      return 0;
    for (b = chan->bans; b; b = b->next) {
      mask = str_escape(b->mask, ':', '\\');
      if (!mask ||
        lfprintf(f, "- %s:%s%lu%s:+%lu:%lu:%s:%s\n", mask,
	        (b->flags & MASKREC_PERM) ? "+" : "", b->expire,
	        (b->flags & MASKREC_STICKY) ? "*" : "", b->added,
	        b->lastactive, b->user ? b->user : conf.bot->nick,
	        b->desc ? b->desc : "requested") == EOF) {
          if (mask)
            free(mask);
          return 0;
        }
      free(mask);
    }
  }
  return 1;
}
/* Write the exemptlists to a file.
 */
bool write_exempts(FILE *f, int idx)
{
  if (global_exempts)
    if (lfprintf(f, EXEMPT_NAME " - -\n") == EOF) /* Daemus */
      return 0;

  maskrec *e = NULL;
  char *mask = NULL;

  for (e = global_exempts; e; e = e->next) {
    mask = str_escape(e->mask, ':', '\\');
    if (!mask ||
        lfprintf(f, "%s %s:%s%lu%s:+%lu:%lu:%s:%s\n", "%", mask,
		(e->flags & MASKREC_PERM) ? "+" : "", e->expire,
		(e->flags & MASKREC_STICKY) ? "*" : "", e->added,
		e->lastactive, e->user ? e->user : conf.bot->nick,
		e->desc ? e->desc : "requested") == EOF) {
      if (mask)
	free(mask);
      return 0;
    }
    free(mask);
  }
  for (struct chanset_t *chan = chanset;chan ;chan = chan->next) {
    if (lfprintf(f, "&&%s exempts\n", chan->dname) == EOF)
      return 0;
    for (e = chan->exempts; e; e = e->next) {
      mask = str_escape(e->mask, ':', '\\');
      if (!mask ||
		lfprintf(f,"%s %s:%s%lu%s:+%lu:%lu:%s:%s\n","%", mask,
		(e->flags & MASKREC_PERM) ? "+" : "", e->expire,
		(e->flags & MASKREC_STICKY) ? "*" : "", e->added,
		e->lastactive, e->user ? e->user : conf.bot->nick,
		e->desc ? e->desc : "requested") == EOF) {
        if (mask)
           free(mask);
         return 0;
      }
      free(mask);
    }
  }
  return 1;
}

/* Write the invitelists to a file.
 */
bool write_invites(FILE *f, int idx)
{

  if (global_invites)
    if (lfprintf(f, INVITE_NAME " - -\n") == EOF) /* Daemus */
      return 0;

  maskrec *ir = NULL;
  char *mask = NULL;

  for (ir = global_invites; ir; ir = ir->next)  {
    mask = str_escape(ir->mask, ':', '\\');
    if (!mask ||
	lfprintf(f,"@ %s:%s%lu%s:+%lu:%lu:%s:%s\n", mask,
		(ir->flags & MASKREC_PERM) ? "+" : "", ir->expire,
		(ir->flags & MASKREC_STICKY) ? "*" : "", ir->added,
		ir->lastactive, ir->user ? ir->user : conf.bot->nick,
		ir->desc ? ir->desc : "requested") == EOF) {
      if (mask)
	free(mask);
      return 0;
    }
    free(mask);
  }
  for (struct chanset_t *chan = chanset; chan; chan = chan->next) {
    if (lfprintf(f, "$$%s invites\n", chan->dname) == EOF)
      return 0;
    for (ir = chan->invites; ir; ir = ir->next) {
      mask = str_escape(ir->mask, ':', '\\');
      if (!mask ||
	      lfprintf(f,"@ %s:%s%lu%s:+%lu:%lu:%s:%s\n", mask,
		      (ir->flags & MASKREC_PERM) ? "+" : "", ir->expire,
		      (ir->flags & MASKREC_STICKY) ? "*" : "", ir->added,
		      ir->lastactive, ir->user ? ir->user : conf.bot->nick,
		      ir->desc ? ir->desc : "requested") == EOF) {
        if (mask)
	  free(mask);
	return 0;
      }
      free(mask);
    }
  }
  return 1;
}

/* Write the channels to the userfile
 */
bool write_chans(FILE *f, int idx)
{
  putlog(LOG_DEBUG, "*", "Writing channels..");

  if (lfprintf(f, CHANS_NAME " - -\n") == EOF) /* Daemus */
    return 0;

  char w[1024] = "";

  for (struct chanset_t *chan = chanset; chan; chan = chan->next) {
    char inactive = 0;

    putlog(LOG_DEBUG, "*", "writing channel %s to userfile..", chan->dname);
    get_mode_protect(chan, w);

    /* if a bot should explicitly NOT join, just set it +inactive ... */
    if (idx >= 0 && !botshouldjoin(dcc[idx].user, chan))
      inactive = '+';
    /* ... otherwise give the bot the *actual* setting */
    else
      inactive = PLSMNS(channel_inactive(chan));

    if (lfprintf(f, "\
+ channel add %s { chanmode { %s } addedby %s addedts %lu idle-kick %d \
bad-cookie %d manop %d mdop %d mop %d \
limit %d stopnethack-mode %d revenge-mode %d flood-chan %d:%lu \
flood-ctcp %d:%lu flood-join %d:%lu flood-kick %d:%lu flood-deop %d:%lu \
flood-nick %d:%lu closed-ban %d closed-invite %d closed-private %d ban-time %lu \
exempt-time %lu invite-time %lu \
%cenforcebans %cdynamicbans %cuserban %cbitch %cprotectops %crevenge \
%crevengebot %cprivate %ccycle %cinactive %cdynamicexempts %cuserexempts \
%cdynamicinvites %cuserinvites %cnodesynch %cclosed %ctake %cmanop %cvoice \
%cfastop %cautoop }\n",
	chan->dname,
	w,
        chan->added_by,
        chan->added_ts,
/* Chanchar template
 *      temp,
 * also include temp %s in dprintf.
 */
	chan->idle_kick, /* idle-kick 0 is same as dont-idle-kick (lcode)*/
	chan->bad_cookie,
	chan->manop,
	chan->mdop,
	chan->mop,
        chan->limitraise,
	chan->stopnethack_mode,
        chan->revenge_mode,
	chan->flood_pub_thr, chan->flood_pub_time,
        chan->flood_ctcp_thr, chan->flood_ctcp_time,
        chan->flood_join_thr, chan->flood_join_time,
        chan->flood_kick_thr, chan->flood_kick_time,
        chan->flood_deop_thr, chan->flood_deop_time,
	chan->flood_nick_thr, chan->flood_nick_time,
        chan->closed_ban,
/* Chanint template
 *      chan->temp,
 * also include temp %d in dprintf
 */
        chan->closed_invite,
        chan->closed_private,
        chan->ban_time,
        chan->exempt_time,
        chan->invite_time,
 	PLSMNS(channel_enforcebans(chan)),
	PLSMNS(channel_dynamicbans(chan)),
	PLSMNS(!channel_nouserbans(chan)),
	PLSMNS(channel_bitch(chan)),
	PLSMNS(channel_protectops(chan)),
	PLSMNS(channel_revenge(chan)),
	PLSMNS(channel_revengebot(chan)),
	PLSMNS(channel_privchan(chan)),
	PLSMNS(channel_cycle(chan)),
        inactive,
	PLSMNS(channel_dynamicexempts(chan)),
	PLSMNS(!channel_nouserexempts(chan)),
 	PLSMNS(channel_dynamicinvites(chan)),
	PLSMNS(!channel_nouserinvites(chan)),
	PLSMNS(channel_nodesynch(chan)),
	PLSMNS(channel_closed(chan)),
	PLSMNS(channel_take(chan)),
	PLSMNS(channel_manop(chan)),
	PLSMNS(channel_voice(chan)),
	PLSMNS(channel_fastop(chan)),
        PLSMNS(channel_autoop(chan))
/* Chanflag template
 * also include a %ctemp above.
 *      PLSMNS(channel_temp(chan)),
 */
        ) == EOF)
          return 0;
  }
  return 1;
}

void channels_writeuserfile()
{
  char s[1024] = "";
  FILE *f = NULL;
  int  ret = 0;

  putlog(LOG_DEBUG, "@", "Writing channel/ban/exempt/invite entries.");
  simple_sprintf(s, "%s~new", userfile);
  f = fopen(s, "a");
  if (f) {
    ret  = write_chans(f, -1);
    ret += write_config(f, -1);
    ret += write_bans(f, -1);
    ret += write_exempts(f, -1);
    ret += write_invites(f, -1);
    fclose(f);
  }
  if (ret < 5)
    putlog(LOG_MISC, "*", USERF_ERRWRITE);
}

/* Expire mask originally set by `who' on `chan'?
 *
 * We might not want to expire masks in all cases, as other bots
 * often tend to immediately reset masks they've listed in their
 * internal ban list, making it quite senseless for us to remove
 * them in the first place.
 *
 * Returns 1 if a mask on `chan' by `who' may be expired and 0 if
 * not.
 */
bool expired_mask(struct chanset_t *chan, char *who)
{
  memberlist *m = NULL, *m2 = NULL;
  char buf[UHOSTLEN] = "", *snick = NULL, *sfrom = NULL;
  struct userrec *u = NULL;

  strcpy(buf, who);
  sfrom = buf;
  snick = splitnick(&sfrom);

  if (!snick[0])
    return 1;

  m = ismember(chan, snick);
  if (!m)
    for (m2 = chan->channel.member; m2 && m2->nick[0]; m2 = m2->next)
      if (!egg_strcasecmp(sfrom, m2->userhost)) {
	m = m2;
	break;
      }

  if (!m || !chan_hasop(m) || !rfc_casecmp(m->nick, botname))
    return 1;

  /* At this point we know the person/bot who set the mask is currently
   * present in the channel and has op.
   */

  if (m->user)
    u = m->user;
  else {
    simple_sprintf(buf, "%s!%s", m->nick, m->userhost);
    u = get_user_by_host(buf);
  }
  /* Do not expire masks set by bots. */
  if (u && u->bot)
    return 0;
  else
    return 1;
}

/* Check for expired timed-bans.
 */

static void check_expired_bans(void)
{
  maskrec *u = NULL, *u2 = NULL;
  struct chanset_t *chan = NULL;
  masklist *b = NULL;

  for (u = global_bans; u; u = u2) { 
    u2 = u->next;
    if (!(u->flags & MASKREC_PERM) && (now >= u->expire)) {
      putlog(LOG_MISC, "*", "%s %s (%s)", BANS_NOLONGER, u->mask, MISC_EXPIRED);
      for (chan = chanset; chan; chan = chan->next)
        for (b = chan->channel.ban; b->mask[0]; b = b->next)
	  if (!rfc_casecmp(b->mask, u->mask) && expired_mask(chan, b->who) && b->timer != now) {
#ifdef LEAF
	    add_mode(chan, '-', 'b', u->mask);
#endif /* LEAF */
	    b->timer = now;
	  }
      u_delmask('b', NULL, u->mask, 1);
    }
  }
  /* Check for specific channel-domain bans expiring */
  for (chan = chanset; chan; chan = chan->next) {
    for (u = chan->bans; u; u = u2) {
      u2 = u->next;
      if (!(u->flags & MASKREC_PERM) && (now >= u->expire)) {
	putlog(LOG_MISC, "*", "%s %s %s %s (%s)", BANS_NOLONGER,
	       u->mask, MISC_ONLOCALE, chan->dname, MISC_EXPIRED);
	for (b = chan->channel.ban; b->mask[0]; b = b->next)
          if (!rfc_casecmp(b->mask, u->mask) && expired_mask(chan, b->who) && b->timer != now) {
#ifdef LEAF
	    add_mode(chan, '-', 'b', u->mask);
#endif /* LEAF */
	    b->timer = now;
	  }
	u_delmask('b', chan, u->mask, 1);
      }
    }
  }
}

/* Check for expired timed-exemptions
 */
static void check_expired_exempts(void)
{
  if (!use_exempts)
    return;

  maskrec *u = NULL, *u2 = NULL;
  struct chanset_t *chan = NULL;
  masklist *b = NULL, *e = NULL;
  int match;

  for (u = global_exempts; u; u = u2) {
    u2 = u->next;
    if (!(u->flags & MASKREC_PERM) && (now >= u->expire)) {
      putlog(LOG_MISC, "*", "%s %s (%s)", EXEMPTS_NOLONGER,
	     u->mask, MISC_EXPIRED);
      for (chan = chanset; chan; chan = chan->next) {
        match = 0;
        b = chan->channel.ban;
        while (b->mask[0] && !match) {
          if (wild_match(b->mask, u->mask) ||
            wild_match(u->mask, b->mask))
            match = 1;
          else
            b = b->next;
        }
        if (match)
          putlog(LOG_MISC, chan->dname,
            "Exempt not expired on channel %s. Ban still set!",
            chan->dname);
	else
	  for (e = chan->channel.exempt; e->mask[0]; e = e->next)
	    if (!rfc_casecmp(e->mask, u->mask) && expired_mask(chan, e->who) && e->timer != now) {
#ifdef LEAF
	      add_mode(chan, '-', 'e', u->mask);
#endif /* LEAF */
	      e->timer = now;
	    }
      }
      u_delmask('e', NULL, u->mask,1);
    }
  }
  /* Check for specific channel-domain exempts expiring */
  for (chan = chanset; chan; chan = chan->next) {
    for (u = chan->exempts; u; u = u2) {
      u2 = u->next;
      if (!(u->flags & MASKREC_PERM) && (now >= u->expire)) {
        match=0;
        b = chan->channel.ban;
        while (b->mask[0] && !match) {
          if (wild_match(b->mask, u->mask) ||
            wild_match(u->mask, b->mask))
            match=1;
          else
            b = b->next;
        }
        if (match)
          putlog(LOG_MISC, chan->dname,
            "Exempt not expired on channel %s. Ban still set!",
            chan->dname);
        else {
          putlog(LOG_MISC, "*", "%s %s %s %s (%s)", EXEMPTS_NOLONGER,
		 u->mask, MISC_ONLOCALE, chan->dname, MISC_EXPIRED);
	  for (e = chan->channel.exempt; e->mask[0]; e = e->next)
	    if (!rfc_casecmp(e->mask, u->mask) && expired_mask(chan, e->who) && e->timer != now) {
#ifdef LEAF
	      add_mode(chan, '-', 'e', u->mask);
#endif /* LEAF */
	      e->timer = now;
	    }
          u_delmask('e', chan, u->mask, 1);
        }
      }
    }
  }
}

/* Check for expired timed-invites.
 */
static void check_expired_invites(void)
{
  if (!use_invites)
    return;

  maskrec *u = NULL, *u2 = NULL;
  struct chanset_t *chan = NULL;
  masklist *b = NULL;

  for (u = global_invites; u; u = u2) {
    u2 = u->next;
    if (!(u->flags & MASKREC_PERM) && (now >= u->expire)) {
      putlog(LOG_MISC, "*", "%s %s (%s)", INVITES_NOLONGER,
	     u->mask, MISC_EXPIRED);
      for (chan = chanset; chan; chan = chan->next)
	if (!(chan->channel.mode & CHANINV))
	  for (b = chan->channel.invite; b->mask[0]; b = b->next)
	    if (!rfc_casecmp(b->mask, u->mask) && expired_mask(chan, b->who) && b->timer != now) {
#ifdef LEAF
	      add_mode(chan, '-', 'I', u->mask);
#endif /* LEAF */
	      b->timer = now;
	    }
      u_delmask('I', NULL, u->mask,1);
    }
  }
  /* Check for specific channel-domain invites expiring */
  for (chan = chanset; chan; chan = chan->next) {
    for (u = chan->invites; u; u = u2) {
      u2 = u->next;
      if (!(u->flags & MASKREC_PERM) && (now >= u->expire)) {
	putlog(LOG_MISC, "*", "%s %s %s %s (%s)", INVITES_NOLONGER,
	       u->mask, MISC_ONLOCALE, chan->dname, MISC_EXPIRED);
	if (!(chan->channel.mode & CHANINV))
	  for (b = chan->channel.invite; b->mask[0]; b = b->next)
	    if (!rfc_casecmp(b->mask, u->mask) && expired_mask(chan, b->who) && b->timer != now) {
#ifdef LEAF
	      add_mode(chan, '-', 'I', u->mask);
#endif /* LEAF */
	      b->timer = now;
	    }
	u_delmask('I', chan, u->mask, 1);
      }
    }
  }
}

void check_expired_masks()
{
  check_expired_bans();
  check_expired_exempts();
  check_expired_invites();
}
