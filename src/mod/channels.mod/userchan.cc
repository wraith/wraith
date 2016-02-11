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
 * userchan.c -- part of channels.mod
 *
 */


#include <bdlib/src/String.h>
#include <bdlib/src/Stream.h>
extern struct cmd_pass *cmdpass;

struct chanuserrec *get_chanrec(struct userrec *u, char *chname)
{
  for (struct chanuserrec *ch = u->chanrec; ch; ch = ch->next)
    if (!rfc_casecmp(ch->channel, chname))
      return ch;
  return NULL;
}

struct chanuserrec *add_chanrec(struct userrec *u, char *chname)
{
  if (findchan_by_dname(chname)) {
    struct chanuserrec *ch = (struct chanuserrec *) calloc(1, sizeof(struct chanuserrec));

    ch->next = u->chanrec;
    u->chanrec = ch;
    ch->info = NULL;
    ch->flags = 0;
    ch->laston = 0;
    strlcpy(ch->channel, chname, sizeof(ch->channel));
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
int u_setsticky_mask(struct chanset_t *chan, maskrec *u, char *uhost, int sticky, const char type)
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
        shareout("ms %c %s %d %s\n", type, uhost, sticky, (chan) ? chan->dname : "");
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

bool u_match_mask(maskrec *rec, const char *mask)
{
  for (; rec; rec = rec->next)
    if (wild_match(rec->mask, mask) || match_cidr(rec->mask, mask))
      return 1;
  return 0;
}

static int count_mask(maskrec *rec)
{
  int ret = 0;

  for (; rec; rec = rec->next)
    ret++;

  return ret;
}

int u_delmask(char type, struct chanset_t *c, char *who, int doit)
{
  int j, i = 0, n_mask = 0;
  char temp[256] = "";
  maskrec **u = NULL, *t;

  if (type == 'b') {
    u = c ? &c->bans : &global_bans;
    n_mask = count_mask(global_bans);
  } else if (type == 'e') {
    u = c ? &c->exempts : &global_exempts;
    n_mask = count_mask(global_exempts);
  } else if (type == 'I') {
    u = c ? &c->invites : &global_invites;
    n_mask = count_mask(global_invites);
  }

  if (!strchr(who, '!') && str_isdigit(who)) {
    j = atoi(who);
    if (j) {
      j--;		/* our list starts at 0 */
      if (c)
        j -= n_mask;	/* subtract out the globals as the number given is globals+j */
      for (; (*u) && j; u = &((*u)->next), j--) 
        ;
      if (*u) {
        strlcpy(temp, (*u)->mask, sizeof temp);
        i = 1;
      } else
        return -j - 1;
    } else
      return 0;
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
          shareout("-mc %c %s %s\n", type, c->dname, mask);
	else
          shareout("-m %c %s\n", type, mask);
	free(mask);
      }
    }
    free(lastdeletedmask);
    lastdeletedmask = (*u)->mask;
    free((*u)->desc);
    free((*u)->user);
    t = *u;
    *u = (*u)->next;
    free(t);
  }
  return i;
}

/* Note: If first char of note is '*' it's a sticky mask.
 */
bool u_addmask(char type, struct chanset_t *chan, char *who, const char *from, const char *note, time_t expire_time, int flags)
{
  char host[UHOSTLEN] = "", s[UHOSTLEN] = "";
  maskrec *p = NULL, *l = NULL, **u = NULL;

  if (type == 'b')
    u = chan ? &chan->bans : &global_bans;
  else if (type == 'e')
    u = chan ? &chan->exempts : &global_exempts;
  else if (type == 'I')
    u = chan ? &chan->invites : &global_invites;

  strlcpy(host, who, sizeof(host));
  /* Choke check: fix broken bans (must have '!' and '@') */
  if ((strchr(host, '!') == NULL) && (strchr(host, '@') == NULL))
    strlcat(host, "!*@*", sizeof(host));
  else if (strchr(host, '@') == NULL)
    strlcat(host, "@*", sizeof(host));
  else if (strchr(host, '!') == NULL) {
    char *i = strchr(host, '@');

    strlcpy(s, i, sizeof(s));
    *i = 0;
    strlcat(host, "!*", sizeof(host));
    strlcat(host, s, sizeof(host));
  }
    if (conf.bot->hub)
      simple_snprintf(s, sizeof(s), "%s!%s@%s", origbotname, botuser, conf.bot->net.host);
    else
      simple_snprintf(s, sizeof(s), "%s!%s", botname, botuserhost);
  if (s[0] && type == 'b' && wild_match(host, s)) {
    putlog(LOG_MISC, "*", "Wanted to ban myself--deflected.");
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
    p = (maskrec *) calloc(1, sizeof(maskrec));
    p->next = *u;
    *u = p;
  }
  else {
    free(p->mask);
    free(p->user);
    free(p->desc);
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
	shareout("+m %c %s %d %s%s %s %s\n",
                 type, mask, (int) (expire_time - now),
		 (flags & MASKREC_STICKY) ? "s" : "",
		 (flags & MASKREC_PERM) ? "p" : "-", from, note);
      else
	shareout("+mc %c %s %d %s %s%s %s %s\n",
		 type, mask, (int) (expire_time - now), chan->dname, 
                 (flags & MASKREC_STICKY) ? "s" : "",
		 (flags & MASKREC_PERM) ? "p" : "-", from, note);
      free(mask);
    }
  }
  return 1;
}

/* Take host entry from ban list and display it ban-style.
 */
static void display_mask(const char type, int idx, int number, maskrec *mask, struct chanset_t *chan, bool show_inact)
{
  char dates[81] = "", s[41] = "";
  const char *str_type = (type == 'b' ? "BAN" : type == 'e' ? "EXEMPT" : "INVITE");

  if (mask->added) {
    daysago(now, mask->added, s, sizeof(s));
    simple_snprintf(dates, sizeof(dates), "Created %s", s);
    if (mask->added < mask->lastactive) {
      strlcat(dates, ", ", sizeof(dates));
      strlcat(dates, "last used", sizeof(dates));
      strlcat(dates, " ", sizeof(dates));
      daysago(now, mask->lastactive, s, sizeof(s));
      strlcat(dates, s, sizeof(dates));
    }
  } else
    dates[0] = 0;
  if (mask->flags & MASKREC_PERM)
    strlcpy(s, "(perm)", sizeof(s));
  else {
    char s1[41] = "";

    days(mask->expire, now, s1, sizeof(s1));
    simple_snprintf(s, sizeof(s), "(expires %s)", s1);
  }
  if (mask->flags & MASKREC_STICKY)
    strlcat(s, " (sticky)", sizeof(s));

  /* always show mask on hubs */
  if (!chan || ischanmask(type, chan, mask->mask) || conf.bot->hub) {
    if (number >= 0) {
      dprintf(idx, "  [%3d] %s %s\n", number, mask->mask, s);
    } else {
      dprintf(idx, "%s: %s %s\n", str_type, mask->mask, s);
    }
  } else if (show_inact) {
    if (number >= 0) {
      dprintf(idx, "! [%3d] %s %s\n", number, mask->mask, s);
    } else {
      dprintf(idx, "%s (inactive): %s %s\n", str_type, mask->mask, s);
    }
  } else
    return;
  dprintf(idx, "        %s: %s\n", mask->user, mask->desc);
  if (dates[0])
    dprintf(idx, "        %s\n", dates);
}

static void tell_masks(const char type, int idx, bool show_inact, char *match, bool all)
{
  int k = 1;
  char *chname = NULL;
  const char *str_type = (type == 'b' ? "ban" : type == 'e' ? "exempt" : "invite");
  maskrec *global_masks = (type == 'b' ? global_bans : type == 'e' ? global_exempts : global_invites);
  struct chanset_t *chan = NULL;
  maskrec *mr = NULL;

  /* Was a channel given? */
  if (match[0]) {
    chname = newsplit(&match);
    if (chname[0] && (strchr(CHANMETA, chname[0]))) {
      chan = findchan_by_dname(chname);
      if (!chan) {
	dprintf(idx, "No such channel defined.\n");
	return;
      }
    } else
      match = chname;
  }

  if (all)
    chan = chanset;

  /* don't return here, we want to show global masks even if no chan */
  if (!chan && !(chan = findchan_by_dname(dcc[idx].u.chat->con_chan)) && !(chan = chanset))
    chan = NULL;

  while (chan) {
  get_user_flagrec(dcc[idx].user, &user, chan->dname, chan);
  if (privchan(user, chan, PRIV_OP)) {
    if (all) goto next;
    dprintf(idx, "No such channel defined.\n");
    return;
  } else if (!chk_op(user, chan)) {
    if (all) goto next;
    dprintf(idx, "You don't have access to view %ss on %s\n", str_type, chan->dname);
    return;
  }


  if (chan && show_inact)
    dprintf(idx, "Global %ss:   (! = not active on %s)\n", str_type, chan->dname);
  else
    dprintf(idx, "Global %ss:\n", str_type);

  for (mr = global_masks; mr; mr = mr->next) {
    if (match[0]) {
      if ((wild_match(match, mr->mask)) ||
	  (wild_match(match, mr->desc)) ||
	  (wild_match(match, mr->user)))
	display_mask(type, idx, k, mr, chan, 1);
      k++;
    } else
      display_mask(type, idx, k++, mr, chan, show_inact);
  }

  if (chan) {
    maskrec *chan_masks = (type == 'b' ? chan->bans : type == 'e' ? chan->exempts : chan->invites);

    if (show_inact)
      dprintf(idx, "Channel %ss for %s:   (! = not active on, * = not placed by bot)\n", str_type, chan->dname);
    else
      dprintf(idx, "Channel %ss for %s:  (* = not placed by bot)\n", str_type, chan->dname);
    for (mr = chan_masks; mr; mr = mr->next) {
      if (match[0]) {
	if ((wild_match(match, mr->mask)) ||
	    (wild_match(match, mr->desc)) ||
	    (wild_match(match, mr->user)))
	  display_mask(type, idx, k, mr, chan, 1);
	k++;
      } else
	display_mask(type, idx, k++, mr, chan, show_inact);
    }
    if (chan->ircnet_status & CHAN_ACTIVE) {
      masklist *ml = NULL;
      masklist *channel_list = (type == 'b' ? chan->channel.ban : type == 'e' ? chan->channel.exempt : chan->channel.invite);

      char s[UHOSTLEN] = "", *s1 = NULL, *s2 = NULL, fill[256] = "";
      int min, sec;

      for (ml = channel_list; ml && ml->mask[0]; ml = ml->next) {    
	if ((!u_equals_mask(global_masks, ml->mask)) &&
	    (!u_equals_mask(chan_masks, ml->mask))) {
	  strlcpy(s, ml->who, sizeof(s));
	  s2 = s;
	  s1 = splitnick(&s2);
	  if (s1[0])
	    simple_snprintf(fill, sizeof(fill), "%s (%s!%s)", ml->mask, s1, s2);
	  else
	    simple_snprintf(fill, sizeof(fill), "%s (server %s)", ml->mask, s2);
	  if (ml->timer != 0) {
	    min = (now - ml->timer) / 60;
	    sec = (now - ml->timer) - (min * 60);
	    simple_snprintf(s, sizeof(s), " (active %02d:%02d)", min, sec);
	    strlcat(fill, s, sizeof(fill));
	  }
	  if ((!match[0]) || (wild_match(match, ml->mask)))
	    dprintf(idx, "* [%3d] %s\n", k, fill);
	  k++;
	}
      }
    }
  }
  next:;
    if (!all)
      chan = NULL;
    else
      chan = chan->next;
  }

  if (k == 1)
    dprintf(idx, "(There are no %ss, permanent or otherwise.)\n", str_type);
  if ((!show_inact) && (!match[0]))
    dprintf(idx, "Use '%ss all' to see the total list.\n", str_type);
}

/* Write the ban lists and the ignore list to a file.
 */
void write_bans(bd::Stream& stream, int idx)
{
  if (global_ign)
    stream << bd::String::printf(IGNORE_NAME " - -\n");

  char *mask = NULL;

  for (struct igrec *i = global_ign; i; i = i->next) {
    mask = str_escape(i->igmask, ':', '\\');
    if (mask) {
	stream << bd::String::printf("- %s:%s%li:%s:%li:%s\n", mask,
		(i->flags & IGREC_PERM) ? "+" : "", (long) i->expire,
		i->user ? i->user : conf.bot->nick, (long) i->added,
		i->msg ? i->msg : "");
        free(mask);
    }
  }
  if (global_bans)
    stream << bd::String::printf(BAN_NAME " - -\n");

  maskrec *b = NULL;

  for (b = global_bans; b; b = b->next) {
    mask = str_escape(b->mask, ':', '\\');
    if (mask) {
	stream << bd::String::printf("- %s:%s%li%s:+%li:%li:%s:%s\n", mask,
		(b->flags & MASKREC_PERM) ? "+" : "", (long) b->expire,
		(b->flags & MASKREC_STICKY) ? "*" : "", (long) b->added,
		(long) b->lastactive, b->user ? b->user : conf.bot->nick,
		b->desc ? b->desc : "requested");
      free(mask);
    }
  }
  for (struct chanset_t *chan = chanset; chan; chan = chan->next) {
    stream << bd::String::printf("::%s bans\n", chan->dname);

    for (b = chan->bans; b; b = b->next) {
      mask = str_escape(b->mask, ':', '\\');
      if (mask) {
        stream << bd::String::printf("- %s:%s%li%s:+%li:%li:%s:%s\n", mask,
	        (b->flags & MASKREC_PERM) ? "+" : "", (long) b->expire,
	        (b->flags & MASKREC_STICKY) ? "*" : "", (long) b->added,
	        (long) b->lastactive, b->user ? b->user : conf.bot->nick,
	        b->desc ? b->desc : "requested");
        free(mask);
      }
    }
  }
}
/* Write the exemptlists to a file.
 */
void write_exempts(bd::Stream& stream, int idx)
{
  if (global_exempts)
    stream << bd::String::printf(EXEMPT_NAME " - -\n");

  maskrec *e = NULL;
  char *mask = NULL;

  for (e = global_exempts; e; e = e->next) {
    mask = str_escape(e->mask, ':', '\\');
    if (mask) {
        stream << bd::String::printf("%s %s:%s%li%s:+%li:%li:%s:%s\n", "%", mask,
		(e->flags & MASKREC_PERM) ? "+" : "", (long) e->expire,
		(e->flags & MASKREC_STICKY) ? "*" : "", (long) e->added,
		(long) e->lastactive, e->user ? e->user : conf.bot->nick,
		e->desc ? e->desc : "requested");
      free(mask);
    }
  }
  for (struct chanset_t *chan = chanset;chan ;chan = chan->next) {
    stream << bd::String::printf("&&%s exempts\n", chan->dname);
    for (e = chan->exempts; e; e = e->next) {
      mask = str_escape(e->mask, ':', '\\');
      if (mask) {
	stream << bd::String::printf("%s %s:%s%li%s:+%li:%li:%s:%s\n","%", mask,
		(e->flags & MASKREC_PERM) ? "+" : "", (long) e->expire,
		(e->flags & MASKREC_STICKY) ? "*" : "", (long) e->added,
		(long) e->lastactive, e->user ? e->user : conf.bot->nick,
		e->desc ? e->desc : "requested");
        free(mask);
      }
    }
  }
}

/* Write the invitelists to a file.
 */
void write_invites(bd::Stream& stream, int idx)
{
  if (global_invites)
    stream << bd::String::printf(INVITE_NAME " - -\n");

  maskrec *ir = NULL;
  char *mask = NULL;

  for (ir = global_invites; ir; ir = ir->next)  {
    mask = str_escape(ir->mask, ':', '\\');
    if (mask) {
      stream << bd::String::printf("@ %s:%s%li%s:+%li:%li:%s:%s\n", mask,
		(ir->flags & MASKREC_PERM) ? "+" : "", (long) ir->expire,
		(ir->flags & MASKREC_STICKY) ? "*" : "", (long) ir->added,
		(long) ir->lastactive, ir->user ? ir->user : conf.bot->nick,
		ir->desc ? ir->desc : "requested");
      free(mask);
    }
  }
  for (struct chanset_t *chan = chanset; chan; chan = chan->next) {
    stream << bd::String::printf("$$%s invites\n", chan->dname);

    for (ir = chan->invites; ir; ir = ir->next) {
      mask = str_escape(ir->mask, ':', '\\');
      if (mask) {
        stream << bd::String::printf("@ %s:%s%li%s:+%li:%li:%s:%s\n", mask,
		      (ir->flags & MASKREC_PERM) ? "+" : "", (long) ir->expire,
		      (ir->flags & MASKREC_STICKY) ? "*" : "", (long) ir->added,
		      (long) ir->lastactive, ir->user ? ir->user : conf.bot->nick,
		      ir->desc ? ir->desc : "requested");
        free(mask);
      }
    }
  }
}

/* Write the channels to the userfile
 */
static void write_chan(bd::Stream& stream, int idx, struct chanset_t* chan)
{
  putlog(LOG_DEBUG, "*", "writing channel %s to userfile..", chan->dname);

  bool force_inactive = 0;

  stream << bd::String::printf("+ channel add %s { ", chan->dname);
  if (chan != chanset_default)
    stream << bd::String::printf("addedby %s addedts %li ", chan->added_by, (long)chan->added_ts);
  stream << channel_to_string(chan, force_inactive);
  stream << bd::String::printf("}\n");
}

bd::String channel_to_string(struct chanset_t* chan, bool force_inactive) {
  char w[1024] = "";

  get_mode_protect(chan, w, sizeof(w));
  return bd::String::printf("\
chanmode { %s } groups { %s } bad-cookie %d manop %d mdop %d mop %d limit %d revenge %d ban-type %d \
flood-chan %d:%d flood-bytes %d:%d flood-ctcp %d:%d flood-join %d:%d \
flood-kick %d:%d flood-deop %d:%d flood-nick %d:%d flood-mjoin %d:%d \
flood-mpub %d:%d flood-mbytes %d:%d flood-mctcp %d:%d \
capslimit %d colorlimit %d closed-ban %d closed-invite %d closed-private %d closed-exempt %d ban-time %d \
exempt-time %d invite-time %d voice-non-ident %d voice-moderate %d auto-delay %d \
flood-exempt %d flood-lock-time %d knock %d fish-key { %s } \
%cenforcebans %cdynamicbans %cuserbans %cbitch %cfloodban \
%cprivate %ccycle %cinactive %cdynamicexempts %cuserexempts \
%cdynamicinvites %cuserinvites %cnodesynch %cclosed %cvoice \
%cfastop %cautoop %cbotbitch %cbackup %crbl %cvoicebitch %cprotect protect-backup %d %c%s",
	w,
/* Chanchar template
 *      temp,
 * also include temp %s in dprintf.
 */
        chan->groups && chan->groups->length() ? static_cast<bd::String>(chan->groups->join(" ")).c_str() : "",
	chan->bad_cookie,
	chan->manop,
	chan->mdop,
	chan->mop,
        chan->limitraise,
	chan->revenge,
        chan->ban_type,
	chan->flood_pub_thr, chan->flood_pub_time,
	chan->flood_bytes_thr, chan->flood_bytes_time,
        chan->flood_ctcp_thr, chan->flood_ctcp_time,
        chan->flood_join_thr, chan->flood_join_time,
        chan->flood_kick_thr, chan->flood_kick_time,
        chan->flood_deop_thr, chan->flood_deop_time,
	chan->flood_nick_thr, chan->flood_nick_time,
	chan->flood_mjoin_thr, chan->flood_mjoin_time,
	chan->flood_mpub_thr, chan->flood_mpub_time,
	chan->flood_mbytes_thr, chan->flood_mbytes_time,
	chan->flood_mctcp_thr, chan->flood_mctcp_time,
        chan->capslimit,
        chan->colorlimit,
        chan->closed_ban,
/* Chanint template
 *      chan->temp,
 * also include temp %d in dprintf
 */
        chan->closed_invite,
        chan->closed_private,
        chan->closed_exempt_mode,
        chan->ban_time,
        chan->exempt_time,
        chan->invite_time,
        chan->voice_non_ident,
        chan->voice_moderate,
        chan->auto_delay,
        chan->flood_exempt_mode,
        chan->flood_lock_time,
        chan->knock_flags,
        chan->fish_key,
 	PLSMNS(channel_enforcebans(chan)),
	PLSMNS(channel_dynamicbans(chan)),
	PLSMNS(!channel_nouserbans(chan)),
	PLSMNS(channel_bitch(chan)),
	PLSMNS(channel_floodban(chan)),
	PLSMNS(channel_privchan(chan)),
	PLSMNS(channel_cycle(chan)),
        force_inactive ? '+' : PLSMNS(channel_inactive(chan)),
	PLSMNS(channel_dynamicexempts(chan)),
	PLSMNS(!channel_nouserexempts(chan)),
 	PLSMNS(channel_dynamicinvites(chan)),
	PLSMNS(!channel_nouserinvites(chan)),
	PLSMNS(channel_nodesynch(chan)),
	PLSMNS(channel_closed(chan)),
	PLSMNS(channel_voice(chan)),
	PLSMNS(channel_fastop(chan)),
        PLSMNS(channel_autoop(chan)),
        PLSMNS(channel_botbitch(chan)),
        PLSMNS(channel_backup(chan)),
        PLSMNS(channel_rbl(chan)),
        PLSMNS(channel_voicebitch(chan)),
        PLSMNS(channel_protect(chan)),
        chan->protect_backup,
	HAVE_TAKE ? PLSMNS(channel_take(chan)) : ' ',
        HAVE_TAKE ? "take " : " "
/* Chanflag template
 * also include a %ctemp above.
 *      PLSMNS(channel_temp(chan)),
 */
  );
}

/* Write the channels to the userfile
 */
void write_chans(bd::Stream& stream, int idx, bool old)
{
  putlog(LOG_DEBUG, "*", "Writing channels..");

  stream << bd::String::printf(CHANS_NAME " - -\n");

  /* FIXME: Remove after 1.2.15 */
  if (!old)
    write_chan(stream, idx, chanset_default);
  for (struct chanset_t *chan = chanset; chan; chan = chan->next)
    write_chan(stream, idx, chan);
}

/* FIXME: remove after 1.2.14 */
/* Write the channels to the userfile
 */
void write_chans_compat(bd::Stream& stream, int idx)
{
  putlog(LOG_DEBUG, "*", "Writing channels..");

  stream << bd::String::printf(CHANS_NAME " - -\n");

  char w[1024] = "";

  for (struct chanset_t *chan = chanset; chan; chan = chan->next) {
    char inactive = 0;

    putlog(LOG_DEBUG, "*", "writing channel %s to userfile..", chan->dname);
    get_mode_protect(chan, w, sizeof(w));

    inactive = PLSMNS(channel_inactive(chan));

    stream << bd::String::printf("\
+ channel add %s { chanmode { %s } addedby %s addedts %li \
bad-cookie %d manop %d mdop %d mop %d limit %d \
flood-chan %d:%d flood-ctcp %d:%d flood-join %d:%d \
flood-kick %d:%d flood-deop %d:%d flood-nick %d:%d \
caps-limit %d color-limit %d closed-ban %d closed-invite %d closed-private %d ban-time %d \
exempt-time %d invite-time %d voice-non-ident %d auto-delay %d \
%cenforcebans %cdynamicbans %cuserbans %cbitch \
%cprivate %ccycle %cinactive %cdynamicexempts %cuserexempts \
%cdynamicinvites %cuserinvites %cnodesynch %cclosed %cvoice \
%cfastop %cautoop %cbotbitch %cbackup %c%s}\n",
	chan->dname,
	w,
        chan->added_by,
        (long)chan->added_ts,
	chan->bad_cookie,
	chan->manop,
	chan->mdop,
	chan->mop,
        chan->limitraise,
	chan->flood_pub_thr, chan->flood_pub_time,
        chan->flood_ctcp_thr, chan->flood_ctcp_time,
        chan->flood_join_thr, chan->flood_join_time,
        chan->flood_kick_thr, chan->flood_kick_time,
        chan->flood_deop_thr, chan->flood_deop_time,
	chan->flood_nick_thr, chan->flood_nick_time,
        chan->capslimit,
        chan->colorlimit,
        chan->closed_ban,
        chan->closed_invite,
        chan->closed_private,
        chan->ban_time,
        chan->exempt_time,
        chan->invite_time,
        chan->voice_non_ident,
        chan->auto_delay,
 	PLSMNS(channel_enforcebans(chan)),
	PLSMNS(channel_dynamicbans(chan)),
	PLSMNS(!channel_nouserbans(chan)),
	PLSMNS(channel_bitch(chan)),
	PLSMNS(channel_privchan(chan)),
	PLSMNS(channel_cycle(chan)),
        inactive,
	PLSMNS(channel_dynamicexempts(chan)),
	PLSMNS(!channel_nouserexempts(chan)),
 	PLSMNS(channel_dynamicinvites(chan)),
	PLSMNS(!channel_nouserinvites(chan)),
	PLSMNS(channel_nodesynch(chan)),
	PLSMNS(channel_closed(chan)),
	PLSMNS(channel_voice(chan)),
	PLSMNS(channel_fastop(chan)),
        PLSMNS(channel_autoop(chan)),
        PLSMNS(channel_botbitch(chan)),
        PLSMNS(channel_backup(chan)),
	HAVE_TAKE ? PLSMNS(channel_take(chan)) : ' ',
        HAVE_TAKE ? "take " : " "
    );
  }
}

void channels_writeuserfile(bd::Stream& stream, int old)
{
  putlog(LOG_DEBUG, "@", "Writing channel/ban/exempt/invite entries.");
  if (old != 1)
    write_chans(stream, -1, old == 0 ? 0 : 1);
  else /* flood-* hacks */
    write_chans_compat(stream, -1);
  write_vars_and_cmdpass(stream, -1);
  write_bans(stream, -1);
  write_exempts(stream, -1);
  write_invites(stream, -1);
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

  strlcpy(buf, who, sizeof(buf));
  sfrom = buf;
  snick = splitnick(&sfrom);

  if (!snick[0])
    return 1;

  m = ismember(chan, snick);
  if (!m)
    for (m2 = chan->channel.member; m2 && m2->nick[0]; m2 = m2->next)
      if (!strcasecmp(sfrom, m2->userhost)) {
	m = m2;
	break;
      }

  if (!m || !chan_hasop(m) || m->is_me)
    return 1;

  /* At this point we know the person/bot who set the mask is currently
   * present in the channel and has op.
   */

  if (m->user)
    u = m->user;
  else {
    simple_snprintf(buf, sizeof(buf), "%s!%s", m->nick, m->userhost);
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

static void check_expired_mask(const char type)
{
  maskrec *u = NULL, *u2 = NULL, *list = NULL;
  struct chanset_t *chan = NULL;
  masklist *b = NULL, *chanlist = NULL;
  const char *str_typing = (type == 'b' ? "banning" : type == 'e' ? "ban exempting" : "inviting");
  bool remove, match;
  
  list = (type == 'b' ? global_bans : type == 'e' ? global_exempts : global_invites);

  for (u = list; u; u = u2) { 
    u2 = u->next;
    if (!(u->flags & MASKREC_PERM) && (now >= u->expire)) {
      putlog(LOG_MISC, "*", "No longer %s %s (expired)", str_typing, u->mask);
     if (!conf.bot->hub) {
      for (chan = chanset; chan; chan = chan->next) {
        remove = 1;			/* hack for 'e' */
        if (type == 'e') {
          match = 0;
          b = chan->channel.ban;
          while (b->mask[0] && !match) {
            if (wild_match(b->mask, u->mask) || wild_match(u->mask, b->mask))
              match = 1;
            else
              b = b->next;
          }
          if (match) {
            putlog(LOG_MISC, chan->dname, "Exempt not expired on channel %s. Ban still set!", chan->dname);
            remove = 0;
          }
        }
        if (remove) {
          chanlist = (type == 'b' ? chan->channel.ban : type == 'e' ? chan->channel.exempt : chan->channel.invite);
          for (b = chanlist; b->mask[0]; b = b->next) {
            if (!rfc_casecmp(b->mask, u->mask) && expired_mask(chan, b->who) && b->timer != now) {
              add_mode(chan, '-', type, u->mask);
              b->timer = now;
	    }
          }
          u_delmask(type, NULL, u->mask, 1);
        }
      }
     } else
       u_delmask(type, NULL, u->mask, 1);
    }
  }
  /* Check for specific channel-domain bans expiring */

  for (chan = chanset; chan; chan = chan->next) {
    list = (type == 'b' ? chan->bans : type == 'e' ? chan->exempts : chan->invites);

    for (u = list; u; u = u2) {
      u2 = u->next;
      if (!(u->flags & MASKREC_PERM) && (now >= u->expire)) {
        remove = 1;
        if (!conf.bot->hub && type == 'e') {
          match = 0;
          b = chan->channel.ban;
          while (b->mask[0] && !match) {
            if (wild_match(b->mask, u->mask) || wild_match(u->mask, b->mask))
              match = 1;
            else
              b = b->next;
          }
          if (match) {
            putlog(LOG_MISC, chan->dname, "Exempt not expired on channel %s. Ban still set!", chan->dname);
            remove = 0;
          }
        }

        if (remove) {
          putlog(LOG_MISC, "*", "No longer %s %s on %s (expired)", str_typing, u->mask, chan->dname);
          if (!conf.bot->hub) {
            chanlist = (type == 'b' ? chan->channel.ban : type == 'e' ? chan->channel.exempt : chan->channel.invite);

            for (b = chanlist; b->mask[0]; b = b->next) {
              if (!rfc_casecmp(b->mask, u->mask) && expired_mask(chan, b->who) && b->timer != now) {
                if (!conf.bot->hub)
                  add_mode(chan, '-', type, u->mask);
                b->timer = now;
              }
            }
          }
          u_delmask(type, chan, u->mask, 1);
        }
      }
    }
  }
}

void check_expired_masks()
{
  check_expired_mask('b');
  if (use_exempts)
    check_expired_mask('e');
  if (use_invites)
    check_expired_mask('I');
}
/* vim: set sts=2 sw=2 ts=8 et: */
