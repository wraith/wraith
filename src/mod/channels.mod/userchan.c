/*
 * userchan.c -- part of channels.mod
 *
 */

#ifdef S_DCCPASS
extern struct cmd_pass *cmdpass;
#endif

struct chanuserrec *get_chanrec(struct userrec *u, char *chname)
{
  struct chanuserrec *ch = NULL;

  for (ch = u->chanrec; ch; ch = ch->next) 
    if (!rfc_casecmp(ch->channel, chname))
      return ch;
  return NULL;
}

static struct chanuserrec *add_chanrec(struct userrec *u, char *chname)
{
  struct chanuserrec *ch = NULL;

  if (findchan_by_dname(chname)) {
    ch = calloc(1, sizeof(struct chanuserrec));

    ch->next = u->chanrec;
    u->chanrec = ch;
    ch->info = NULL;
    ch->flags = 0;
    ch->flags_udef = 0;
    ch->laston = 0;
    strncpy(ch->channel, chname, 81);
    ch->channel[80] = 0;
    if (!noshare && !(u->flags & USER_UNSHARED))
      shareout(findchan_by_dname(chname), "+cr %s %s\n", u->handle, chname);
  }
  return ch;
}

static void add_chanrec_by_handle(struct userrec *bu, char *hand, char *chname)
{
  struct userrec *u = NULL;

  u = get_user_by_handle(bu, hand);
  if (!u)
    return;
  if (!get_chanrec(u, chname))
    add_chanrec(u, chname);
}

static void get_handle_chaninfo(char *handle, char *chname, char *s)
{
  struct userrec *u = NULL;
  struct chanuserrec *ch = NULL;

  u = get_user_by_handle(userlist, handle);
  if (u == NULL) {
    s[0] = 0;
    return;
  }
  ch = get_chanrec(u, chname);
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

static void set_handle_chaninfo(struct userrec *bu, char *handle,
				char *chname, char *info)
{
  struct userrec *u = NULL;
  struct chanuserrec *ch = NULL;
  struct chanset_t *cst = NULL;

  u = get_user_by_handle(bu, handle);
  if (!u)
    return;
  ch = get_chanrec(u, chname);
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
  cst = findchan_by_dname(chname);
  if ((!noshare) && (bu == userlist) && !(u->flags & (USER_UNSHARED | USER_BOT))) {
    shareout(cst, "chchinfo %s %s %s\n", handle, chname, info ? info : "");
  }
}

static void del_chanrec(struct userrec *u, char *chname)
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
      if (!noshare && !(u->flags & USER_UNSHARED))
	shareout(findchan_by_dname(chname), "-cr %s %s\n", u->handle, chname);
      return;
    }
    lst = ch;
    ch = ch->next;
  }
}

static void set_handle_laston(char *chan, struct userrec *u, time_t n)
{
  struct chanuserrec *ch = NULL;

  if (!u)
    return;
  touch_laston(u, chan, n);
  ch = get_chanrec(u, chan);
  if (!ch)
    return;
  ch->laston = n;
}

/* Is this mask sticky?
 */
static int u_sticky_mask(maskrec *u, char *uhost)
{
  for (; u; u = u->next)
    if (!rfc_casecmp(u->mask, uhost))
      return (u->flags & MASKREC_STICKY);
  return 0;
}

/* Set sticky attribute for a mask.
 */
static int u_setsticky_mask(struct chanset_t *chan, maskrec *u, char *uhost,
			    int sticky, char *botcmd)
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
        shareout(chan, "%s %s %d %s\n", botcmd, uhost, sticky,
                                        (chan) ? chan->dname : "");
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
static int u_equals_mask(maskrec *u, char *mask)
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

static int u_match_mask(maskrec *rec, char *mask)
{
  for (; rec; rec = rec->next)
    if (wild_match(rec->mask, mask))
      return 1;
  return 0;
}

static int u_delban(struct chanset_t *c, char *who, int doit)
{
  int j, i = 0;
  maskrec *t = NULL;
  maskrec **u = (c) ? &c->bans : &global_bans;
  char temp[256] = "";

  if (!strchr(who, '!') && str_isdigit(who)) {
    j = atoi(who);
    j--;
    for (; (*u) && j; u = &((*u)->next), j--);
    if (*u) {
      strncpyz(temp, (*u)->mask, sizeof temp);
      i = 1;
    } else
      return -j - 1;
  } else {
    /* Find matching host, if there is one */
    for (; *u && !i; u = &((*u)->next))
      if (!rfc_casecmp((*u)->mask, who)) {
        strncpyz(temp, who, sizeof temp);
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
	  shareout(c, "-bc %s %s\n", c->dname, mask);
	else
	  shareout(NULL, "-b %s\n", mask);
	free(mask);
      }
    }
    if (lastdeletedmask)
      free(lastdeletedmask);
    lastdeletedmask = strdup((*u)->mask);
    free((*u)->mask);
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

static int u_delexempt (struct chanset_t * c, char * who, int doit)
{
  int j, i = 0;
  maskrec *t = NULL;
  maskrec **u = c ? &(c->exempts) : &global_exempts;

  if (!strchr(who, '!') && str_isdigit(who)) {
    j = atoi(who);
    j--;
    for (;(*u) && j;u=&((*u)->next),j--);
    if (*u) {
      strcpy(who, (*u)->mask);
      i = 1;
    } else
      return -j-1;
  } else {
    /* Find matching host, if there is one */
    for (;*u && !i;u=&((*u)->next))
      if (!rfc_casecmp((*u)->mask,who)) {
	i = 1;
	break;
      }
    if (!*u)
      return 0;
  }
  if (i && doit) {
    if (!noshare) {
      char *mask = str_escape(who, ':', '\\');

      if (mask) {
	/* Distribute chan exempts differently */
	if (c)
	  shareout(c, "-ec %s %s\n", c->dname, mask);
	else
	  shareout(NULL, "-e %s\n", mask);
	free(mask);
      }
    }
    free((*u)->mask);
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

static int u_delinvite(struct chanset_t *c, char *who, int doit)
{
  int j, i = 0;
  maskrec *t = NULL;
  maskrec **u = c ? &(c->invites) : &global_invites;

  if (!strchr(who, '!') && str_isdigit(who)) {
    j = atoi(who);
    j--;
    for (;(*u) && j;u=&((*u)->next),j--);
    if (*u) {
      strcpy(who, (*u)->mask);
      i = 1;
    } else
      return -j-1;
  } else {
    /* Find matching host, if there is one */
    for (;*u && !i; u = &((*u)->next))
      if (!rfc_casecmp((*u)->mask,who)) {
	i = 1;
	break;
      }
    if (!*u)
      return 0;
  }
  if (i && doit) {
    if (!noshare) {
      char *mask = str_escape(who, ':', '\\');

      if (mask) {
	/* Distribute chan invites differently */
	if (c)
	  shareout(c, "-invc %s %s\n", c->dname, mask);
	else
	  shareout(NULL, "-inv %s\n", mask);
	free(mask);
      }
    }
    free((*u)->mask);
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

/* Note: If first char of note is '*' it's a sticky ban.
 */
static int u_addban(struct chanset_t *chan, char *ban, char *from, char *note,
		    time_t expire_time, int flags)
{
  char host[1024] = "", s[1024] = "";
  maskrec *p = NULL, *l = NULL, **u = chan ? &chan->bans : &global_bans;
  module_entry *me = NULL;

  strcpy(host, ban);
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
  if ((me = module_find("server", 0, 0)) && me->funcs)
    simple_sprintf(s, "%s!%s", me->funcs[SERVER_BOTNAME],
		   me->funcs[SERVER_BOTUSERHOST]);
  else
    simple_sprintf(s, "%s!%s@%s", origbotname, botuser, conf.bot->host);
  if (wild_match(host, s)) {
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
    p = calloc(1, sizeof(maskrec));
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
	shareout(NULL, "+b %s %lu %s%s %s %s\n", mask, expire_time - now,
		 (flags & MASKREC_STICKY) ? "s" : "",
		 (flags & MASKREC_PERM) ? "p" : "-", from, note);
      else
	shareout(chan, "+bc %s %lu %s %s%s %s %s\n", mask, expire_time - now,
		 chan->dname, (flags & MASKREC_STICKY) ? "s" : "",
		 (flags & MASKREC_PERM) ? "p" : "-", from, note);
      free(mask);
    }
  }
  return 1;
}

/* Note: If first char of note is '*' it's a sticky invite.
 */
static int u_addinvite(struct chanset_t *chan, char *invite, char *from,
		       char *note, time_t expire_time, int flags)
{
  char host[1024] = "", s[1024] = "";
  maskrec *p = NULL, *l, **u = chan ? &chan->invites : &global_invites;
  module_entry *me = NULL;

  strcpy(host, invite);
  /* Choke check: fix broken invites (must have '!' and '@') */
  if ((strchr(host, '!') == NULL) && (strchr(host, '@') == NULL))
    strcat(host, "!*@*");
  else if (strchr(host, '@') == NULL)
    strcat(host, "@*");
  else if (strchr(host, '!') == NULL) {
    char * i = strchr(host, '@');
    strcpy(s, i);
    *i = 0;
    strcat(host, "!*");
    strcat(host, s);
  }
  if ((me = module_find("server",0,0)) && me->funcs)
    simple_sprintf(s, "%s!%s", me->funcs[SERVER_BOTNAME],
		   me->funcs[SERVER_BOTUSERHOST]);
  else
    simple_sprintf(s, "%s!%s@%s", origbotname, botuser, conf.bot->host);

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
    p = calloc(1, sizeof(maskrec));
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
	shareout(NULL, "+inv %s %lu %s%s %s %s\n", mask, expire_time - now,
		 (flags & MASKREC_STICKY) ? "s" : "",
		 (flags & MASKREC_PERM) ? "p": "-", from, note);
      else
	shareout(chan, "+invc %s %lu %s %s%s %s %s\n", mask, expire_time - now,
		 chan->dname, (flags & MASKREC_STICKY) ? "s" : "",
		 (flags & MASKREC_PERM) ? "p": "-", from, note);
      free(mask);
    }
  }
  return 1;
}

/* Note: If first char of note is '*' it's a sticky exempt.
 */
static int u_addexempt(struct chanset_t *chan, char *exempt, char *from,
		       char *note, time_t expire_time, int flags)
{
  char host[1024] = "", s[1024] = "";
  maskrec *p = NULL, *l, **u = chan ? &chan->exempts : &global_exempts;
  module_entry *me = NULL;

  strcpy(host, exempt);
  /* Choke check: fix broken exempts (must have '!' and '@') */
  if ((strchr(host, '!') == NULL) && (strchr(host, '@') == NULL))
    strcat(host, "!*@*");
  else if (strchr(host, '@') == NULL)
    strcat(host, "@*");
  else if (strchr(host, '!') == NULL) {
    char * i = strchr(host, '@');
    strcpy(s, i);
    *i = 0;
    strcat(host, "!*");
    strcat(host, s);
  }
  if ((me = module_find("server",0,0)) && me->funcs)
    simple_sprintf(s, "%s!%s", me->funcs[SERVER_BOTNAME],
		   me->funcs[SERVER_BOTUSERHOST]);
  else
    simple_sprintf(s, "%s!%s@%s", origbotname, botuser, conf.bot->host);

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
    p = calloc(1, sizeof(maskrec));
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
	shareout(NULL, "+e %s %lu %s%s %s %s\n", mask, expire_time - now,
		 (flags & MASKREC_STICKY) ? "s" : "",
		 (flags & MASKREC_PERM) ? "p": "-", from, note);
      else
	shareout(chan, "+ec %s %lu %s %s%s %s %s\n", mask, expire_time - now,
		 chan->dname, (flags & MASKREC_STICKY) ? "s" : "",
		 (flags & MASKREC_PERM) ? "p": "-", from, note);
      free(mask);
    }
  }
  return 1;
}

/* Take host entry from ban list and display it ban-style.
 */
static void display_ban(int idx, int number, maskrec *ban, struct chanset_t *chan, int show_inact)
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
static void display_exempt(int idx, int number, maskrec *exempt, struct chanset_t *chan, int show_inact)
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
static void display_invite (int idx, int number, maskrec *invite, struct chanset_t *chan, int show_inact)
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

static void tell_bans(int idx, int show_inact, char *match)
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
  if (private(user, chan, PRIV_OP)) {
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

static void tell_exempts(int idx, int show_inact, char *match)
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
  if (private(user, chan, PRIV_OP)) {
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

static void tell_invites(int idx, int show_inact, char *match)
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
  if (private(user, chan, PRIV_OP)) {
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

static int write_config(FILE *f, int idx)
{
  int i = 0;
#ifdef S_DCCPASS
  struct cmd_pass *cp = NULL;
#endif
  putlog(LOG_DEBUG, "@", "Writing config entries...");
  if (lfprintf(f, CONFIG_NAME " - -\n") == EOF) /* Daemus */
      return 0;
  for (i = 0; i < cfg_count; i++) {
    if ((cfg[i]->flags & CFGF_GLOBAL) && (cfg[i]->gdata)) {
      if (lfprintf(f, "@ %s %s\n", cfg[i]->name, cfg[i]->gdata ? cfg[i]->gdata : "") == EOF)
        return 0;
    }
  }

#ifdef S_DCCPASS
  for (cp = cmdpass; cp; cp = cp->next)
    if (lfprintf(f, "- %s %s\n", cp->name, cp->pass) == EOF)
      return 0;
#endif

  return 1;
}
/* Write the ban lists and the ignore list to a file.
 */
static int write_bans(FILE *f, int idx)
{
  struct chanset_t *chan = NULL;
  maskrec *b = NULL;
  struct igrec *i = NULL;
  char *mask = NULL;

  if (global_ign)
    if (lfprintf(f, IGNORE_NAME " - -\n") == EOF)	/* Daemus */
      return 0;
  for (i = global_ign; i; i = i->next) {
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
  for (chan = chanset; chan; chan = chan->next)
    if ((idx < 0)  || 1) {
      struct flag_record fr = {FR_CHAN | FR_GLOBAL | FR_BOT, 0, 0, 0, 0, 0};

      if (idx >= 0)
	get_user_flagrec(dcc[idx].user, &fr, chan->dname);
      else
	fr.chan = BOT_SHARE;

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
static int write_exempts(FILE *f, int idx)
{
  struct chanset_t *chan = NULL;
  maskrec *e = NULL;
  char *mask = NULL;

  if (global_exempts)
    if (lfprintf(f, EXEMPT_NAME " - -\n") == EOF) /* Daemus */
      return 0;
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
  for (chan = chanset;chan;chan=chan->next)
    if ((idx < 0) || 1) {
      struct flag_record fr = {FR_CHAN | FR_GLOBAL | FR_BOT, 0, 0, 0, 0, 0};

      if (idx >= 0)
	get_user_flagrec(dcc[idx].user,&fr,chan->dname);
      else
	fr.chan = BOT_SHARE;
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
static int write_invites(FILE *f, int idx)
{
  struct chanset_t *chan = NULL;
  maskrec *ir = NULL;
  char *mask = NULL;

  if (global_invites)
    if (lfprintf(f, INVITE_NAME " - -\n") == EOF) /* Daemus */
      return 0;
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
  for (chan = chanset; chan; chan = chan->next)
    if ((idx < 0) || (1)) {
      struct flag_record fr = {FR_CHAN | FR_GLOBAL | FR_BOT, 0, 0, 0, 0, 0};

      if (idx >= 0)
	get_user_flagrec(dcc[idx].user,&fr,chan->dname);
      else
	fr.chan = BOT_SHARE;
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
static int write_chans(FILE *f, int idx)
{
  char w[1024] = "", w2[1024] = "", name[163] = "";
/*  char udefs[2048] = "", buf[2048]; */
/* Chanchar template
 *char temp[121];
 */
  struct chanset_t *chan = NULL;
/*  struct udef_struct *ul; */

  putlog(LOG_DEBUG, "*", "Writing channels..");

  if (lfprintf(f, CHANS_NAME " - -\n") == EOF) /* Daemus */
    return 0;

  for (chan = chanset; chan; chan = chan->next) {
    if ((idx < 0) || (1)) {
      struct flag_record fr = {FR_CHAN | FR_GLOBAL | FR_BOT, 0, 0, 0, 0, 0};
      if (idx >= 0)
        get_user_flagrec(dcc[idx].user,&fr,chan->dname);
      else
        fr.chan = BOT_SHARE;

     putlog(LOG_DEBUG, "*", "writing channel %s to userfile..", chan->dname);

/*     egg_memset(udefs,'\0',2048); */
     convert_element(chan->dname, name);
     get_mode_protect(chan, w);
     convert_element(w, w2);
/* Chanchar template
 *   convert_element(chan->temp, temp);
 */
/* fuck these for now
     for (ul = udef; ul; ul = ul->next) { //put the udefs into one string
       egg_memset(buf,'\0',2048);
       if (ul->defined && ul->name) { 
	if (ul->type == UDEF_FLAG)
	 sprintf(buf, "%c%s%s ", getudef(ul->values, chan->dname) ? '+' : '-', "udef-flag-", ul->name);
	else if (ul->type == UDEF_INT)
	  sprintf(buf, "%s%s %d ", "udef-int-", ul->name, getudef(ul->values, chan->dname));
	else
	  debug1("UDEF-ERROR: unknown type %d", ul->type);
        strcat(udefs,buf);
       }
     }
*/
     if (lfprintf(f, "+ channel add %s { chanmode %s addedby %s addedts %lu idle-kick %d limit %d stopnethack-mode %d \
revenge-mode %d \
flood-chan %d:%d flood-ctcp %d:%d flood-join %d:%d \
flood-kick %d:%d flood-deop %d:%d flood-nick %d:%d \
closed-ban %d \
ban-time %d exempt-time %d invite-time %d \
%cenforcebans %cdynamicbans %cuserbans %cbitch \
%cprotectops %crevenge %crevengebot \
%cprivate %ccycle %cinactive %cdynamicexempts \
%cuserexempts %cdynamicinvites %cuserinvites \
%cnodesynch %cclosed %ctake %cmanop %cvoice \
%cfastop }\n",
	name,
	w2,
        chan->added_by,
        chan->added_ts,
/* Chanchar template
 *      temp,
 * also include temp %s in dprintf.
 */
	chan->idle_kick, /* idle-kick 0 is same as dont-idle-kick (lcode)*/
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
	PLSMNS(channel_private(chan)),
	PLSMNS(channel_cycle(chan)),
	PLSMNS(channel_inactive(chan)),
	PLSMNS(channel_dynamicexempts(chan)),
	PLSMNS(!channel_nouserexempts(chan)),
 	PLSMNS(channel_dynamicinvites(chan)),
	PLSMNS(!channel_nouserinvites(chan)),
	PLSMNS(channel_nodesynch(chan)),
	PLSMNS(channel_closed(chan)),
	PLSMNS(channel_take(chan)),
	PLSMNS(channel_manop(chan)),
	PLSMNS(channel_voice(chan)),
	PLSMNS(channel_fastop(chan))
/* Chanflag template
 * also include a %ctemp above.
 *      PLSMNS(channel_temp(chan)),
 */
        ) == EOF)
          return 0;
    } 
  }
  return 1;
}


static void channels_writeuserfile(void)
{
#ifdef HUB
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
#endif /* HUB */
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
static int expired_mask(struct chanset_t *chan, char *who)
{
  memberlist *m = NULL, *m2 = NULL;
  char buf[UHOSTLEN] = "", *snick = NULL, *sfrom = NULL;
  struct userrec *u = NULL;

  /* Always expire masks, regardless of who set it? */
  if (force_expire)
    return 1;

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
  if (u && u->flags & USER_BOT)
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
      putlog(LOG_MISC, "*", "%s %s (%s)", BANS_NOLONGER,
	     u->mask, MISC_EXPIRED);
      for (chan = chanset; chan; chan = chan->next)
	for (b = chan->channel.ban; b->mask[0]; b = b->next)
	  if (!rfc_casecmp(b->mask, u->mask) &&
	      expired_mask(chan, b->who) && b->timer != now) {
	    add_mode(chan, '-', 'b', u->mask);
	    b->timer = now;
	  }
      u_delban(NULL, u->mask, 1);
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
	  if (!rfc_casecmp(b->mask, u->mask) &&
	      expired_mask(chan, b->who) && b->timer != now) {
	    add_mode(chan, '-', 'b', u->mask);
	    b->timer = now;
	  }
	u_delban(chan, u->mask, 1);
      }
    }
  }
}

/* Check for expired timed-exemptions
 */
static void check_expired_exempts(void)
{
  maskrec *u = NULL, *u2 = NULL;
  struct chanset_t *chan = NULL;
  masklist *b = NULL, *e = NULL;
  int match;

  if (!use_exempts)
    return;
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
	    if (!rfc_casecmp(e->mask, u->mask) &&
		expired_mask(chan, e->who) && e->timer != now) {
	      add_mode(chan, '-', 'e', u->mask);
	      e->timer = now;
	    }
      }
      u_delexempt(NULL, u->mask,1);
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
	    if (!rfc_casecmp(e->mask, u->mask) &&
		expired_mask(chan, e->who) && e->timer != now) {
	      add_mode(chan, '-', 'e', u->mask);
	      e->timer = now;
	    }
          u_delexempt(chan, u->mask, 1);
        }
      }
    }
  }
}

/* Check for expired timed-invites.
 */
static void check_expired_invites(void)
{
  maskrec *u = NULL, *u2 = NULL;
  struct chanset_t *chan = NULL;
  masklist *b = NULL;

  if (!use_invites)
    return;
  for (u = global_invites; u; u = u2) {
    u2 = u->next;
    if (!(u->flags & MASKREC_PERM) && (now >= u->expire)) {
      putlog(LOG_MISC, "*", "%s %s (%s)", INVITES_NOLONGER,
	     u->mask, MISC_EXPIRED);
      for (chan = chanset; chan; chan = chan->next)
	if (!(chan->channel.mode & CHANINV))
	  for (b = chan->channel.invite; b->mask[0]; b = b->next)
	    if (!rfc_casecmp(b->mask, u->mask) &&
		expired_mask(chan, b->who) && b->timer != now) {
	      add_mode(chan, '-', 'I', u->mask);
	      b->timer = now;
	    }
      u_delinvite(NULL, u->mask,1);
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
	    if (!rfc_casecmp(b->mask, u->mask) &&
		expired_mask(chan, b->who) && b->timer != now) {
	      add_mode(chan, '-', 'I', u->mask);
	      b->timer = now;
	    }
	u_delinvite(chan, u->mask, 1);
      }
    }
  }
}
