
/* 
 * users.c -- handles:
 *   testing and enforcing ignores
 *   adding and removing ignores
 *   listing ignores
 *   auto-linking bots
 *   sending and receiving a userfile from a bot
 *   listing users ('.whois' and '.match')
 *   reading the user file
 * 
 * dprintf'ized, 9nov1995
 * 
 * $Id: users.c,v 1.17 2000/01/17 16:14:45 per Exp $
 */

/* 
 * Copyright (C) 1997  Robey Pointer
 * Copyright (C) 1999, 2000  Eggheads
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

#include "main.h"
#include "users.h"
#include "chan.h"
#include "hook.h"
#include "tandem.h"
char natip[121] = "";

#include <netinet/in.h>
#include <arpa/inet.h>
char spaces[33] = "                                 ";
char spaces2[33] = "                                 ";

extern struct dcc_t *dcc;
extern struct userrec *userlist,
 *lastuser;
extern struct chanset_t *chanset;
extern int dcc_total,
  noshare,
  use_silence;
extern char botnetnick[];

#ifdef G_USETCL
extern Tcl_Interp *interp;
#endif
extern time_t now;

int ignore_time = 2;		/* how many minutes will ignores last? */
int gban_total = 0;		/* Total number of global bans */
int gexempt_total = 0;		/* Total number of global exempts */
int ginvite_total = 0;		/* Total number of global invites */

/* is this nick!user@host being ignored? */
int match_ignore(char *uhost)
{
  struct igrec *ir;

  for (ir = global_ign; ir; ir = ir->next)
    if (wild_match(ir->igmask, uhost))
      return 1;
  return 0;
}

int equals_ignore(char *uhost)
{
  struct igrec *u = global_ign;

  for (; u; u = u->next)
    if (!rfc_casecmp(u->igmask, uhost)) {
      if (u->flags & IGREC_PERM)
	return 2;
      else
	return 1;
    }
  return 0;
}

int delignore(char *ign)
{
  int i,
    j;
  struct igrec **u;
  struct igrec *t;

  Context;

  i = 0;
  if (!strchr(ign, '!') && (j = atoi(ign))) {
    for (u = &global_ign, j--; *u && j; u = &((*u)->next), j--);
    if (*u) {
      strcpy(ign, (*u)->igmask);
      i = 1;
    }
  } else {
    /* find the matching host, if there is one */
    for (u = &global_ign; *u && !i; u = &((*u)->next))
      if (!rfc_casecmp(ign, (*u)->igmask)) {
	i = 1;
	break;
      }
  }
  if (i) {
    if (!noshare)
      shareout(NULL, STR("-i %s\n"), ign);
    nfree((*u)->igmask);
    if ((*u)->msg)
      nfree((*u)->msg);
    if ((*u)->user)
      nfree((*u)->user);
    t = *u;
    *u = (*u)->next;
    nfree(t);
  }
  return i;
}

void addignore(char *ign, char *from, char *mnote, time_t expire_time)
{
  struct igrec *p;

  if (equals_ignore(ign))
    delignore(ign);		/* remove old ignore */
  p = user_malloc(sizeof(struct igrec));

  p->next = global_ign;
  global_ign = p;
  p->expire = expire_time;
  p->added = now;
  p->flags = expire_time ? 0 : IGREC_PERM;
  p->igmask = user_malloc(strlen(ign) + 1);
  strcpy(p->igmask, ign);
  p->user = user_malloc(strlen(from) + 1);
  strcpy(p->user, from);
  p->msg = user_malloc(strlen(mnote) + 1);
  strcpy(p->msg, mnote);
  if (!noshare)
    shareout(NULL, STR("+i %s %lu %c %s %s\n"), ign, expire_time - now, (p->flags & IGREC_PERM) ? 'p' : '-', from, mnote);
}

/* take host entry from ignore list and display it ignore-style */
void display_ignore(int idx, int number, struct igrec *ignore)
{
  char dates[81],
    s[41];

  if (ignore->added) {
    daysago(now, ignore->added, s);
    sprintf(dates, STR("Started %s"), s);
  } else
    dates[0] = 0;
  if (ignore->flags & IGREC_PERM)
    strcpy(s, STR("(perm)"));
  else {
    char s1[41];

    days(ignore->expire, now, s1);
    sprintf(s, STR("(expires %s)"), s1);
  }
  if (number >= 0)
    dprintf(idx, STR("  [%3d] %s %s\n"), number, ignore->igmask, s);
  else
    dprintf(idx, STR("IGNORE: %s %s\n"), ignore->igmask, s);
  if (ignore->msg && ignore->msg[0])
    dprintf(idx, STR("        %s: %s\n"), ignore->user, ignore->msg);
  else
    dprintf(idx, STR("        placed by %s\n"), ignore->user);
  if (dates[0])
    dprintf(idx, STR("        %s\n"), dates);
}

/* list the ignores and how long they've been active */
void tell_ignores(int idx, char *match)
{
  struct igrec *u = global_ign;
  int k = 1;

  if (u == NULL) {
    dprintf(idx, STR("No ignores.\n"));
    return;
  }
  dprintf(idx, STR("Currently ignoring:\n"));
  for (; u; u = u->next) {
    if (match[0]) {
      if (wild_match(match, u->igmask) || wild_match(match, u->msg) || wild_match(match, u->user))
	display_ignore(idx, k, u);
      k++;
    } else
      display_ignore(idx, k++, u);
  }
}

/* check for expired timed-ignores */
void check_expired_ignores()
{
  struct igrec **u = &global_ign;

  if (!*u)
    return;
  while (*u) {
    if (!((*u)->flags & IGREC_PERM) && (now >= (*u)->expire)) {
      log(LCAT_INFO, STR("No longer ignoring %s (expired)"), (*u)->igmask);
      if (use_silence) {
	char *p;

	/* possibly an ircu silence was added for this user */
	p = strchr((*u)->igmask, '!');
	if (p == NULL)
	  p = (*u)->igmask;
	else
	  p++;
	dprintf(DP_SERVER, STR("SILENCE -%s\n"), p);
      }
      delignore((*u)->igmask);
    } else {
      u = &((*u)->next);
    }
  }
}

/*        Channel mask loaded from user file. This function is
 *      add(ban|invite|exempt)_fully merged into one. <cybah>
 */
void addmask_fully(struct chanset_t *chan, struct maskrec **m, struct maskrec **global, char *mask, char *from, char *note, time_t expire_time, int flags, time_t added, time_t last)
{
  struct maskrec *p = user_malloc(sizeof(struct maskrec));
  struct maskrec **u = (chan) ? m : global;

  p->next = *u;
  *u = p;
  p->expire = expire_time;
  p->added = added;
  p->lastactive = last;
  p->flags = flags;
  p->mask = user_malloc(strlen(mask) + 1);
  strcpy(p->mask, mask);
  p->user = user_malloc(strlen(from) + 1);
  strcpy(p->user, from);
  p->desc = user_malloc(strlen(note) + 1);
  strcpy(p->desc, note);
}

void restore_chanban(struct chanset_t *chan, char *host)
{
  char *expi,
   *add,
   *last,
   *user,
   *desc;
  int flags = 0;

  expi = strchr(host, ':');
  if (expi) {
    *expi = 0;
    expi++;
    if (*expi == '+') {
      flags |= MASKREC_PERM;
      expi++;
    }
    add = strchr(expi, ':');
    if (add) {
      *add = 0;
      add++;
      if (*add == '+') {
	last = strchr(add, ':');
	if (last) {
	  *last = 0;
	  last++;
	  user = strchr(last, ':');
	  if (user) {
	    *user = 0;
	    user++;
	    desc = strchr(user, ':');
	    if (desc) {
	      *desc = 0;
	      desc++;
	      addmask_fully(chan, &chan->bans, &global_bans, host, user, desc, atoi(expi), flags, atoi(add), atoi(last));
	      return;
	    }
	  }
	}
      } else {
	desc = strchr(add, ':');
	if (desc) {
	  *desc = 0;
	  desc++;
	  addmask_fully(chan, &chan->bans, &global_bans, host, add, desc, atoi(expi), flags, now, 0);
	  return;
	}
      }
    }
  }
  log(LCAT_ERROR, STR("*** Malformed banline for %s."), chan ? chan->name : STR("global_bans"));
}

void restore_chanexempt(struct chanset_t *chan, char *host)
{
  char *expi,
   *add,
   *last,
   *user,
   *desc;
  int flags = 0;

  expi = strchr(host, ':');
  if (expi) {
    *expi = 0;
    expi++;
    if (*expi == '+') {
      flags |= MASKREC_PERM;
      expi++;
    }
    add = strchr(expi, ':');
    if (add) {
	*add = 0;
      add++;
      if (*add == '+') {
	last = strchr(add, ':');
	if (last) {
	  *last = 0;
	  last++;
	  user = strchr(last, ':');
	  if (user) {
	    *user = 0;
	    user++;
	    desc = strchr(user, ':');
	    if (desc) {
	      *desc = 0;
	      desc++;
	      addmask_fully(chan, &chan->exempts, &global_exempts, host, user, desc, atoi(expi), flags, atoi(add), atoi(last));
	      return;
	    }
	  }
	}
      } else {
	desc = strchr(add, ':');

	if (desc) {
	  *desc = 0;
	  desc++;
	  addmask_fully(chan, &chan->exempts, &global_exempts, host, add, desc, atoi(expi), flags, now, 0);
	  return;
	}
      }
    }
  }
  log(LCAT_ERROR, STR("*** Malformed exemptline for %s."), chan ? chan->name : STR("global_exempts"));
}

void restore_chaninvite(struct chanset_t *chan, char *host)
{
  char *expi,
   *add,
   *last,
   *user,
   *desc;
  int flags = 0;

  expi = strchr(host, ':');
  if (expi) {
    *expi = 0;
    expi++;
    if (*expi == '+') {
      flags |= MASKREC_PERM;
      expi++;
    }
    add = strchr(expi, ':');
    if (add) {
	*add = 0;
      add++;
      if (*add == '+') {
	last = strchr(add, ':');
	if (last) {
	  *last = 0;
	  last++;
	  user = strchr(last, ':');
	  if (user) {
	    *user = 0;
	    user++;
	    desc = strchr(user, ':');
	    if (desc) {
	      *desc = 0;
	      desc++;
	      addmask_fully(chan, &chan->invites, &global_invites, host, user, desc, atoi(expi), flags, atoi(add), atoi(last));
	      return;
	    }
	  }
	}
      } else {
	desc = strchr(add, ':');

	if (desc) {
	  *desc = 0;
	  desc++;
	  addmask_fully(chan, &chan->invites, &global_invites, host, add, desc, atoi(expi), flags, now, 0);
	  return;
	}
      }
    }
  }
  log(LCAT_ERROR, STR("*** Malformed inviteline for %s."), chan ? chan->name : STR("global_invites"));
}

void restore_ignore(char *host)
{
  char *expi,
   *user,
   *added,
   *desc;
  int flags = 0;
  struct igrec *p;

  expi = strchr(host, ':');
  if (expi) {
    *expi = 0;
    expi++;
    if (*expi == '+') {
      flags |= IGREC_PERM;
      expi++;
    }
    user = strchr(expi, ':');
    if (user) {
      *user = 0;
      user++;
      added = strchr(user, ':');
      if (added) {
	*added = 0;
	added++;
	desc = strchr(added, ':');
	if (desc) {
	  *desc = 0;
	  desc++;
	} else
	  desc = NULL;
      } else {
	added = "0";
	desc = NULL;
      }
      p = user_malloc(sizeof(struct igrec));

      p->next = global_ign;
      global_ign = p;
      p->expire = atoi(expi);
      p->added = atoi(added);
      p->flags = flags;
      p->igmask = user_malloc(strlen(host) + 1);
      strcpy(p->igmask, host);
      p->user = user_malloc(strlen(user) + 1);
      strcpy(p->user, user);
      if (desc) {
	p->msg = user_malloc(strlen(desc) + 1);
	strcpy(p->msg, desc);
      } else
	p->msg = NULL;
      return;
    }
  }
  log(LCAT_ERROR, STR("*** Malformed ignore line."));
}

void tell_user(int idx, struct userrec *u, int master)
{
  char s[81],
    s1[81];
  int n,
    l = HANDLEN - strlen(u->handle);
  time_t now2;
  struct chanuserrec *ch;
  struct user_entry *ue;
  struct laston_info *li;
  struct flag_record fr = { FR_GLOBAL, 0, 0, 0, 0 };

  Context;
  fr.global = u->flags;

  fr.udef_global = u->flags_udef;
  build_flags(s, &fr, NULL);
  n = 0;
  li = get_user(&USERENTRY_LASTON, u);
  if (!li || !li->laston)
    strcpy(s1, STR("never"));
  else {
    now2 = now - li->laston;
    strcpy(s1, ctime(&li->laston));
    if (now2 > 86400) {
      s1[7] = 0;
      strcpy(&s1[11], &s1[4]);
      strcpy(s1, &s1[8]);
    } else {
      s1[16] = 0;
      strcpy(s1, &s1[11]);
    }
  }
  Context;
  spaces[l] = 0;
  dprintf(idx, STR("%s%s %-5s%5d %-15s %s (%-10.10s)\n"), u->handle, spaces, get_user(&USERENTRY_PASS, u) ? STR("yes") : "no", n, s, s1, (li && li->lastonplace) ? li->lastonplace : STR("nowhere"));
  spaces[l] = ' ';
  /* channel flags? */
  Context;
  ch = u->chanrec;
  while (ch != NULL) {
    fr.match = FR_CHAN | FR_GLOBAL;
    get_user_flagrec(dcc[idx].user, &fr, ch->channel);
    if (glob_op(fr) || chan_op(fr)) {
      if (ch->laston == 0L)
	strcpy(s1, STR("never"));
      else {
	now2 = now - (ch->laston);
	strcpy(s1, ctime(&(ch->laston)));
	if (now2 > 86400) {
	  s1[7] = 0;
	  strcpy(&s1[11], &s1[4]);
	  strcpy(s1, &s1[8]);
	} else {
	  s1[16] = 0;
	  strcpy(s1, &s1[11]);
	}
      }
      fr.match = FR_CHAN;
      fr.chan = ch->flags;
      fr.udef_chan = ch->flags_udef;
      build_flags(s, &fr, NULL);
      spaces[HANDLEN - 9] = 0;
      dprintf(idx, STR("%s  %-18s %-15s %s\n"), spaces, ch->channel, s, s1);
      spaces[HANDLEN - 9] = ' ';
      if (ch->info != NULL)
	dprintf(idx, STR("    INFO: %s\n"), ch->info);
    }
    ch = ch->next;
  }
  /* user-defined extra fields */
  Context;
  for (ue = u->entries; ue; ue = ue->next)
    if (!ue->name && ue->type->display)
      ue->type->display(idx, ue);
}

/* show user by ident */
void tell_user_ident(int idx, char *id, int master)
{
  struct userrec *u;

  u = get_user_by_handle(userlist, id);
  if (u == NULL)
    u = get_user_by_host(id);
  if (u == NULL) {
    dprintf(idx, STR("Can't find anyone matching that.\n"));
    return;
  }
  spaces[HANDLEN - 6] = 0;
  dprintf(idx, STR("HANDLE%s PASS NOTES FLAGS           LAST\n"), spaces);
  spaces[HANDLEN - 6] = ' ';
  tell_user(idx, u, master);
}

/* match string:
 * wildcard to match nickname or hostmasks
 * +attr to find all with attr */
void tell_users_match(int idx, char *mtch, int start, int limit, int master, char *chname)
{
  struct userrec *u = userlist;
  int fnd = 0,
    cnt,
    nomns = 0,
    flags = 0;
  struct list_type *q;
  struct flag_record user,
    pls,
    mns;

  Context;
  dprintf(idx, STR("*** Matching '%s':\n"), mtch);
  cnt = 0;
  spaces[HANDLEN - 6] = 0;
  dprintf(idx, STR("HANDLE%s PASS NOTES FLAGS           LAST\n"), spaces);
  spaces[HANDLEN - 6] = ' ';
  if (start > 1)
    dprintf(idx, STR("(skipping first %d)\n"), start - 1);
  if (strchr("+-&|", *mtch)) {
    user.match = pls.match = FR_GLOBAL | FR_CHAN;
    break_down_flags(mtch, &pls, &mns);
    mns.match = pls.match ^ (FR_AND | FR_OR);
    if (!mns.global &&!mns.udef_global && !mns.chan && !mns.udef_chan) {
      nomns = 1;
      if (!pls.global &&!pls.udef_global && !pls.chan && !pls.udef_chan) {
	/* happy now BB you weenie :P */
	dprintf(idx, STR("Unknown flag specified for matching!!\n"));
	return;
      }
    }
    chname = "*";
    flags = 1;
  }
  while (u != NULL) {
    if (flags) {
      get_user_flagrec(u, &user, chname);
      if (flagrec_eq(&pls, &user)) {
	if (nomns || !flagrec_eq(&mns, &user)) {
	  cnt++;
	  if ((cnt <= limit) && (cnt >= start))
	    tell_user(idx, u, master);
	  if (cnt == limit + 1)
	    dprintf(idx, STR("(more than %d matches; list truncated)\n"), limit);
	}
      }
    } else if (wild_match(mtch, u->handle)) {
      cnt++;
      if ((cnt <= limit) && (cnt >= start))
	tell_user(idx, u, master);
      if (cnt == limit + 1)
	dprintf(idx, STR("(more than %d matches; list truncated)\n"), limit);
    } else {
      fnd = 0;
      for (q = get_user(&USERENTRY_HOSTS, u); q; q = q->next) {
	if ((wild_match(mtch, q->extra)) && (!fnd)) {
	  cnt++;
	  fnd = 1;
	  if ((cnt <= limit) && (cnt >= start)) {
	    tell_user(idx, u, master);
	  }
	  if (cnt == limit + 1)
	    dprintf(idx, STR("(more than %d matches; list truncated)\n"), limit);
	}
      }
    }
    u = u->next;
  }
  dprintf(idx, STR("--- Found %d match%s.\n"), cnt, cnt == 1 ? "" : "es");
}

/* 
 * tagged lines in the user file:
 * * OLD:
 * #  (comment)
 * ;  (comment)
 * -  hostmask(s)
 * +  email
 * *  dcc directory
 * =  comment
 * :  info line
 * .  xtra (Tcl)
 * !  channel-specific
 * !! global laston
 * :: channel-specific bans
 * NEW:
 * *ban global bans
 * *ignore global ignores
 * ::#chan channel bans
 * - entries in each
 * <handle> begin user entry
 * --KEY INFO - info on each
 * NEWER:
 * % exemptmask(s)
 * @ Invitemask(s) (& Config)
 * *exempt global exempts
 * *Invite global Invites
 * && channel-specific exempts
 * &&#chan channel exempts
 * $$ channel-specific Invites
 * $$#chan channel Invites
 */

int noxtra = 0;

int stream_readuserfile(stream str, char * key, struct userrec **ret) {
  char *p,
    buf[512],
    lasthand[512],
   *attr,
   *pass,
   *code,
    s1[512],
   *s;
  struct userrec *bu,
   *u = NULL;
  struct chanset_t *cst = NULL;
  int i;
  char ignored[512];
  struct flag_record fr;
  struct chanuserrec *cr;

  Context;
  bu = (*ret);
  ignored[0] = 0;
  if (bu == userlist) {
    clear_chanlist();
    lastuser = NULL;
    global_bans = NULL;
    global_ign = NULL;
    global_exempts = NULL;
    global_invites = NULL;
  }
  lasthand[0] = 0;
  noshare = noxtra = 1;
  Context;
  /* read opening comment */
  
  stream_gets(str, buf, 511);
  buf[511]=0;
  s=strchr(buf, '\n');
  if (s)
    *s=0;
  s = decrypt_string(key, buf);
  if (s[1] < '4') {
    fatal(STR("Corrupted userfile"), 0);
  }
  if (s[1] > '4')
    fatal(STR("Invalid userfile"), 0);
  nfree(s);
  gban_total = 0;
  gexempt_total = 0;
  ginvite_total = 0;
  while (stream_getpos(str)<stream_size(str)) {
    stream_gets(str, buf, 511);
    buf[511]=0;
    s=strchr(buf, '\n');
    if (s)
      *s=0;
    s = decrypt_string(key, buf);
    strncpy0(buf, s, sizeof(buf));
    nfree(s);
    s = buf;
    if ((s[0] != '#') && (s[0] != ';') && (s[0])) {
      code = newsplit(&s);
      rmspace(s);
      if (!strcmp(code, "-")) {
	if (lasthand[0]) {
	  if (u) {		/* only break it down if there a real users */
	    p = strchr(s, ',');
	    while (p != NULL) {
	      splitc(s1, s, ',');
	      rmspace(s1);
	      if (s1[0])
		set_user(&USERENTRY_HOSTS, u, s1);
	      p = strchr(s, ',');
	    }
	  }
	  /* channel bans are never stacked with , */
	  if (s[0]) {
	    if (lasthand[0] && (strchr(CHANMETA, lasthand[0]) != NULL))
	      restore_chanban(cst, s);
	    else if (lasthand[0] == '*') {
	      if (lasthand[1] == 'i') {
		restore_ignore(s);
#ifdef G_DCCPASS
	      } else if (lasthand[1] == 'C') {
		set_cmd_pass(s, 1);
#endif
	      } else {
		restore_chanban(NULL, s);
		gban_total++;
	      }
	    } else if (lasthand[0]) {
	      set_user(&USERENTRY_HOSTS, u, s);
	    }
	  }
	}
      } else if (strcmp(code, "%") == 0) {	/* exemptmasks */
	if (lasthand[0]) {
	  if (s[0]) {
	    if ((lasthand[0] == '#') || (lasthand[0] == '+')) {
	      restore_chanexempt(cst, s);
	    } else if (lasthand[0] == '*') {
	      if (lasthand[1] == 'e') {
		restore_chanexempt(NULL, s);
		gexempt_total++;
	      } else if (lasthand[1] == 'C') {
		set_log_info(s);
#ifdef HUB
		botnet_send_logsettings_broad(-1, s);
#endif
	      }
	    }
	  }
	}
      } else if (strcmp(code, "@") == 0) {	/* Invitemasks */
	if (lasthand[0]) {
	  if (s[0]) {
	    if ((lasthand[0] == '#') || (lasthand[0] == '+')) {
	      restore_chaninvite(cst, s);
	    } else if (lasthand[0] == '*') {
	      if (lasthand[1] == 'I') {
		restore_chaninvite(NULL, s);
		ginvite_total++;
	      } else if (lasthand[1] == 'C') {
		userfile_cfg_line(s);
	      }
	    }
	  }
	}
      } else if (!strcmp(code, "!")) {
	/* ! #chan laston flags [info] */
	char *chname,
	  *st,
	  *fl;
	
	if (u) {
	  chname = newsplit(&s);
	  st = newsplit(&s);
	  fl = newsplit(&s);
	  rmspace(s);
	  fr.match = FR_CHAN;
	  break_down_flags(fl, &fr, 0);
	  if (findchan(chname)) {
	    for (cr = u->chanrec; cr; cr = cr->next)
	      if (!rfc_casecmp(cr->channel, chname))
		break;
	    if (!cr) {
	      cr = (struct chanuserrec *)
		user_malloc(sizeof(struct chanuserrec));
	      
	      cr->next = u->chanrec;
	      u->chanrec = cr;
	      strncpy0(cr->channel, chname, sizeof(cr->channel));
	      cr->laston = atoi(st);
	      cr->flags = fr.chan;
	      cr->flags_udef = fr.udef_chan;
	      if (s[0]) {
		cr->info = (char *) user_malloc(strlen(s) + 1);
		strcpy(cr->info, s);
	      } else
		cr->info = NULL;
	    }
	  }
	}
      } else if (!strncmp(code, "::", 2)) {
	/* channel-specific bans */
	strcpy(lasthand, &code[2]);
	if (!findchan(lasthand)) {
	  strcpy(s1, lasthand);
	  strcat(s1, " ");
	  if (strstr(ignored, s1) == NULL) {
	    strcat(ignored, lasthand);
	    strcat(ignored, " ");
	  }
	  lasthand[0] = 0;
	  u = 0;
	} else {
	  /* Remove all bans for this channel to avoid dupes */
	  /* NOTE only remove bans for when getting a userfile
	   * from another bot & that channel is shared */
	  cst = findchan(lasthand);
	  clear_masks(cst->bans);
	  cst->bans = NULL;
	}
      } else if (strncmp(code, "&&", 2) == 0) {
	/* channel-specific exempts */
	strcpy(lasthand, &code[2]);
	if (!findchan(lasthand)) {
	  strcpy(s1, lasthand);
	  strcat(s1, " ");
	  if (strstr(ignored, s1) == NULL) {
	    strcat(ignored, lasthand);
	    strcat(ignored, " ");
	  }
	  lasthand[0] = 0;
	  u = 0;
	} else {
	  /* Remove all exempts for this channel to avoid dupes */
	  /* NOTE only remove exempts for when getting a userfile
	   * from another bot & that channel is shared */
	  cst = findchan(lasthand);
	  clear_masks(cst->exempts);
	  cst->exempts = NULL;
	}
      } else if (strncmp(code, "$$", 2) == 0) {
	/* channel-specific invites */
	strcpy(lasthand, &code[2]);
	if (!findchan(lasthand)) {
	  strcpy(s1, lasthand);
	  strcat(s1, " ");
	  if (strstr(ignored, s1) == NULL) {
	    strcat(ignored, lasthand);
	    strcat(ignored, " ");
	  }
	  lasthand[0] = 0;
	  u = 0;
	} else {
	  /* Remove all invites for this channel to avoid dupes */
	  /* NOTE only remove invites for when getting a userfile
	   * from another bot & that channel is shared */
	  cst = findchan(lasthand);
	  clear_masks(cst->invites);
	  cst->invites = NULL;
	}
      } else if (!strncmp(code, "--", 2)) {
	/* new format storage */
	struct user_entry *ue;
	int ok = 0;
	
	Context;
	if (u) {
	  ue = u->entries;
	  for (; ue && !ok; ue = ue->next)
	    if (ue->name && !strcasecmp(code + 2, ue->name)) {
	      struct list_type *list;
	      
	      list = user_malloc(sizeof(struct list_type));
	      
	      list->next = NULL;
	      list->extra = user_malloc(strlen(s) + 1);
	      strcpy(list->extra, s);
	      list_append((&ue->u.list), list);
	      ok = 1;
	    }
	  if (!ok) {
	    ue = user_malloc(sizeof(struct user_entry));
	    
	    ue->name = user_malloc(strlen(code + 1));
	    ue->type = NULL;
	    strcpy(ue->name, code + 2);
	    ue->u.list = user_malloc(sizeof(struct list_type));
	    
	    ue->u.list->next = NULL;
	    ue->u.list->extra = user_malloc(strlen(s) + 1);
	    strcpy(ue->u.list->extra, s);
	    list_insert((&u->entries), ue);
	  }
	}
      } else if (!rfc_casecmp(code, STR("*ban"))) {
	strcpy(lasthand, code);
	u = NULL;
      } else if (!rfc_casecmp(code, STR("*ignore"))) {
	strcpy(lasthand, code);
	u = NULL;
      } else if (!rfc_casecmp(code, STR("*exempt"))) {
	strcpy(lasthand, code);
	u = NULL;
      } else if (!rfc_casecmp(code, STR("*Invite"))) {
	strcpy(lasthand, code);
	u = NULL;
      } else if (!rfc_casecmp(code, STR("*Config"))) {
	strcpy(lasthand, code);
	u = NULL;
      } else if (code[0] == '*') {
	lasthand[0] = 0;
	u = NULL;
      } else {
	pass = newsplit(&s);
	attr = newsplit(&s);
	rmspace(s);
	if (!attr[0] || !pass[0]) {
	  log(LCAT_ERROR, STR("* Corrupted userfile '%s'!"), code);
	  lasthand[0] = 0;
	} else {
	  u = get_user_by_handle(bu, code);
	  if (u && !(u->flags & USER_UNSHARED)) {
	    log(LCAT_ERROR, STR("* Duplicate user record '%s'!"), code);
	    lasthand[0] = 0;
	    u = NULL;
	  } else if (u) {
	    lasthand[0] = 0;
	    u = NULL;
	  } else {
	    fr.match = FR_GLOBAL;
	    break_down_flags(attr, &fr, 0);
	    strcpy(lasthand, code);
	    cst = NULL;
	    if (strlen(code) > HANDLEN)
	      code[HANDLEN] = 0;
	    if (strlen(pass) > 20) {
	      log(LCAT_ERROR, STR("* Corrupted password reset for '%s'"), code);
	      strcpy(pass, "-");
	    }
	    bu = adduser(bu, code, 0, pass, sanity_check(fr.global &USER_VALID));
	    
	    u = get_user_by_handle(bu, code);
	    for (i = 0; i < dcc_total; i++)
	      if (!strcasecmp(code, dcc[i].nick))
		dcc[i].user = u;
	    u->flags_udef = fr.udef_global;
	    /* if s starts with '/' it's got file info */
	  }
	}
      }
    }
  }
  Context;
  (*ret) = bu;
  if (ignored[0]) {
    log(LCAT_ERROR, STR("Ignored bans dor channel(s) %s"), ignored);
  }
  log(LCAT_INFO, STR("Userfile loaded, unpacking..."));
  Context;
  for (u = bu; u; u = u->next) {
    struct user_entry *e;

    if (!(u->flags & USER_BOT) && !strcasecmp(u->handle, botnetnick)) {
      log(LCAT_ERROR, STR("(!) I have an user record, but without +b"));
      /* u->flags |= USER_BOT; */
    }

    for (e = u->entries; e; e = e->next)
      if (e->name) {
	struct user_entry_type *uet = find_entry_type(e->name);

	if (uet) {
	  e->type = uet;
	  uet->unpack(u, e);
	  nfree(e->name);
	  e->name = NULL;
	}
      }
  }
  noshare = noxtra = 0;
  Context;
  /* process the user data *now* */
  return 1;

}

int readuserfile(char *file, char *key, struct userrec **ret)
{
  stream s;
  char buf[512];
  FILE *f;
  int res;
  f = fopen(file, "r");
  if (f == NULL)
    return 0;
  
  s=stream_create();
  while (!feof(f)) {
    fgets(buf, 511, f);
    buf[511]=0;
    stream_printf(s, "%s", buf);
  }
  fclose(f);
  stream_seek(s, SEEK_SET, 0);
  res = stream_readuserfile(s, key, ret);
  stream_kill(s);
  return res;
  

#ifdef OLDCODE
  char *p,
    buf[512],
    lasthand[512],
   *attr,
   *pass,
   *code,
    s1[512],
   *s;
  FILE *f;
  struct userrec *bu,
   *u = NULL;
  struct chanset_t *cst = NULL;
  int i;
  char ignored[512];
  struct flag_record fr;
  struct chanuserrec *cr;

  Context;
  bu = (*ret);
  ignored[0] = 0;
  if (bu == userlist) {
    clear_chanlist();
    lastuser = NULL;
    global_bans = NULL;
    global_ign = NULL;
    global_exempts = NULL;
    global_invites = NULL;
  }
  lasthand[0] = 0;
  f = fopen(file, "r");
  if (f == NULL)
    return 0;
  noshare = noxtra = 1;
  Context;
  /* read opening comment */
  fgets(buf, 180, f);
  s = decrypt_string(key, buf);
  if (s[1] < '4') {
    fatal(STR("Corrupted userfile"), 0);
  }
  if (s[1] > '4')
    fatal(STR("Invalid userfile"), 0);
  nfree(s);
  gban_total = 0;
  gexempt_total = 0;
  ginvite_total = 0;
  while (!feof(f)) {
    fgets(buf, 511, f);
    s = decrypt_string(key, buf);
    strncpy0(buf, s, sizeof(buf));
    nfree(s);
    s = buf;
    if (!feof(f)) {
      if ((s[0] != '#') && (s[0] != ';') && (s[0])) {
	code = newsplit(&s);
	rmspace(s);
	if (!strcmp(code, "-")) {
	  if (lasthand[0]) {
	    if (u) {		/* only break it down if there a real users */
	      p = strchr(s, ',');
	      while (p != NULL) {
		splitc(s1, s, ',');
		rmspace(s1);
		if (s1[0])
		  set_user(&USERENTRY_HOSTS, u, s1);
		p = strchr(s, ',');
	      }
	    }
	    /* channel bans are never stacked with , */
	    if (s[0]) {
	      if (lasthand[0] && (strchr(CHANMETA, lasthand[0]) != NULL))
		restore_chanban(cst, s);
	      else if (lasthand[0] == '*') {
		if (lasthand[1] == 'i') {
		  restore_ignore(s);
#ifdef G_DCCPASS
		} else if (lasthand[1] == 'C') {
		  set_cmd_pass(s, 1);
#endif
		} else {
		  restore_chanban(NULL, s);
		  gban_total++;
		}
	      } else if (lasthand[0]) {
		set_user(&USERENTRY_HOSTS, u, s);
	      }
	    }
	  }
	} else if (strcmp(code, "%") == 0) {	/* exemptmasks */
	  if (lasthand[0]) {
	    if (s[0]) {
	      if ((lasthand[0] == '#') || (lasthand[0] == '+')) {
		restore_chanexempt(cst, s);
	      } else if (lasthand[0] == '*') {
		if (lasthand[1] == 'e') {
		  restore_chanexempt(NULL, s);
		  gexempt_total++;
		} else if (lasthand[1] == 'C') {
		  set_log_info(s);
#ifdef HUB
		  botnet_send_logsettings_broad(-1, s);
#endif
		}
	      }
	    }
	  }
	} else if (strcmp(code, "@") == 0) {	/* Invitemasks */
	  if (lasthand[0]) {
	    if (s[0]) {
	      if ((lasthand[0] == '#') || (lasthand[0] == '+')) {
		restore_chaninvite(cst, s);
	      } else if (lasthand[0] == '*') {
		if (lasthand[1] == 'I') {
		  restore_chaninvite(NULL, s);
		  ginvite_total++;
		} else if (lasthand[1] == 'C') {
		  userfile_cfg_line(s);
		}
	      }
	    }
	  }
	} else if (!strcmp(code, "!")) {
	  /* ! #chan laston flags [info] */
	  char *chname,
	   *st,
	   *fl;

	  if (u) {
	    chname = newsplit(&s);
	    st = newsplit(&s);
	    fl = newsplit(&s);
	    rmspace(s);
	    fr.match = FR_CHAN;
	    break_down_flags(fl, &fr, 0);
	    if (findchan(chname)) {
	      for (cr = u->chanrec; cr; cr = cr->next)
		if (!rfc_casecmp(cr->channel, chname))
		  break;
	      if (!cr) {
		cr = (struct chanuserrec *)
		  user_malloc(sizeof(struct chanuserrec));

		cr->next = u->chanrec;
		u->chanrec = cr;
		strncpy0(cr->channel, chname, sizeof(cr->channel));
		cr->laston = atoi(st);
		cr->flags = fr.chan;
		cr->flags_udef = fr.udef_chan;
		if (s[0]) {
		  cr->info = (char *) user_malloc(strlen(s) + 1);
		  strcpy(cr->info, s);
		} else
		  cr->info = NULL;
	      }
	    }
	  }
	} else if (!strncmp(code, "::", 2)) {
	  /* channel-specific bans */
	  strcpy(lasthand, &code[2]);
	  if (!findchan(lasthand)) {
	    strcpy(s1, lasthand);
	    strcat(s1, " ");
	    if (strstr(ignored, s1) == NULL) {
	      strcat(ignored, lasthand);
	      strcat(ignored, " ");
	    }
	    lasthand[0] = 0;
	    u = 0;
	  } else {
	    /* Remove all bans for this channel to avoid dupes */
	    /* NOTE only remove bans for when getting a userfile
	     * from another bot & that channel is shared */
	    cst = findchan(lasthand);
	    clear_masks(cst->bans);
	    cst->bans = NULL;
	  }
	} else if (strncmp(code, "&&", 2) == 0) {
	  /* channel-specific exempts */
	  strcpy(lasthand, &code[2]);
	  if (!findchan(lasthand)) {
	    strcpy(s1, lasthand);
	    strcat(s1, " ");
	    if (strstr(ignored, s1) == NULL) {
	      strcat(ignored, lasthand);
	      strcat(ignored, " ");
	    }
	    lasthand[0] = 0;
	    u = 0;
	  } else {
	    /* Remove all exempts for this channel to avoid dupes */
	    /* NOTE only remove exempts for when getting a userfile
	     * from another bot & that channel is shared */
	    cst = findchan(lasthand);
	    clear_masks(cst->exempts);
	    cst->exempts = NULL;
	  }
	} else if (strncmp(code, "$$", 2) == 0) {
	  /* channel-specific invites */
	  strcpy(lasthand, &code[2]);
	  if (!findchan(lasthand)) {
	    strcpy(s1, lasthand);
	    strcat(s1, " ");
	    if (strstr(ignored, s1) == NULL) {
	      strcat(ignored, lasthand);
	      strcat(ignored, " ");
	    }
	    lasthand[0] = 0;
	    u = 0;
	  } else {
	    /* Remove all invites for this channel to avoid dupes */
	    /* NOTE only remove invites for when getting a userfile
	     * from another bot & that channel is shared */
	    cst = findchan(lasthand);
	    clear_masks(cst->invites);
	    cst->invites = NULL;
	  }
	} else if (!strncmp(code, "--", 2)) {
	  /* new format storage */
	  struct user_entry *ue;
	  int ok = 0;

	  Context;
	  if (u) {
	    ue = u->entries;
	    for (; ue && !ok; ue = ue->next)
	      if (ue->name && !strcasecmp(code + 2, ue->name)) {
		struct list_type *list;

		list = user_malloc(sizeof(struct list_type));

		list->next = NULL;
		list->extra = user_malloc(strlen(s) + 1);
		strcpy(list->extra, s);
		list_append((&ue->u.list), list);
		ok = 1;
	      }
	    if (!ok) {
	      ue = user_malloc(sizeof(struct user_entry));

	      ue->name = user_malloc(strlen(code + 1));
	      ue->type = NULL;
	      strcpy(ue->name, code + 2);
	      ue->u.list = user_malloc(sizeof(struct list_type));

	      ue->u.list->next = NULL;
	      ue->u.list->extra = user_malloc(strlen(s) + 1);
	      strcpy(ue->u.list->extra, s);
	      list_insert((&u->entries), ue);
	    }
	  }
	} else if (!rfc_casecmp(code, STR("*ban"))) {
	  strcpy(lasthand, code);
	  u = NULL;
	} else if (!rfc_casecmp(code, STR("*ignore"))) {
	  strcpy(lasthand, code);
	  u = NULL;
	} else if (!rfc_casecmp(code, STR("*exempt"))) {
	  strcpy(lasthand, code);
	  u = NULL;
	} else if (!rfc_casecmp(code, STR("*Invite"))) {
	  strcpy(lasthand, code);
	  u = NULL;
	} else if (!rfc_casecmp(code, STR("*Config"))) {
	  strcpy(lasthand, code);
	  u = NULL;
	} else if (code[0] == '*') {
	  lasthand[0] = 0;
	  u = NULL;
	} else {
	  pass = newsplit(&s);
	  attr = newsplit(&s);
	  rmspace(s);
	  if (!attr[0] || !pass[0]) {
	    log(LCAT_ERROR, STR("* Corrupted userfile '%s'!"), code);
	    lasthand[0] = 0;
	  } else {
	    u = get_user_by_handle(bu, code);
	    if (u && !(u->flags & USER_UNSHARED)) {
	      log(LCAT_ERROR, STR("* Duplicate user record '%s'!"), code);
	      lasthand[0] = 0;
	      u = NULL;
	    } else if (u) {
	      lasthand[0] = 0;
	      u = NULL;
	    } else {
	      fr.match = FR_GLOBAL;
	      break_down_flags(attr, &fr, 0);
	      strcpy(lasthand, code);
	      cst = NULL;
	      if (strlen(code) > HANDLEN)
		code[HANDLEN] = 0;
	      if (strlen(pass) > 20) {
		log(LCAT_ERROR, STR("* Corrupted password reset for '%s'"), code);
		strcpy(pass, "-");
	      }
	      bu = adduser(bu, code, 0, pass, sanity_check(fr.global &USER_VALID));

	      u = get_user_by_handle(bu, code);
	      for (i = 0; i < dcc_total; i++)
		if (!strcasecmp(code, dcc[i].nick))
		  dcc[i].user = u;
	      u->flags_udef = fr.udef_global;
	      /* if s starts with '/' it's got file info */
	    }
	  }
	}
      }
    }
  }
  Context;
  fclose(f);
  (*ret) = bu;
  if (ignored[0]) {
    log(LCAT_ERROR, STR("Ignored bans dor channel(s) %s"), ignored);
  }
  log(LCAT_INFO, STR("Userfile loaded, unpacking..."));
  Context;
  for (u = bu; u; u = u->next) {
    struct user_entry *e;

    if (!(u->flags & USER_BOT) && !strcasecmp(u->handle, botnetnick)) {
      log(LCAT_ERROR, STR("(!) I have an user record, but without +b"));
      /* u->flags |= USER_BOT; */
    }

    for (e = u->entries; e; e = e->next)
      if (e->name) {
	struct user_entry_type *uet = find_entry_type(e->name);

	if (uet) {
	  e->type = uet;
	  uet->unpack(u, e);
	  nfree(e->name);
	  e->name = NULL;
	}
      }
  }
  noshare = noxtra = 0;
  Context;
  /* process the user data *now* */
  return 1;
#endif
}

void link_pref_val(struct userrec *u, char *val)
{

/* val must be HANDLEN + 4 chars minimum */
  struct bot_addr *ba;

  val[0] = 'Z';
  val[1] = 0;
  if (!u)
    return;
  if (!(u->flags & USER_BOT))
    return;
  ba = get_user(&USERENTRY_BOTADDR, u);
  if (!ba)
    return;
  if (!ba->hublevel)
    return;
  sprintf(val, STR("%02d%s"), ba->hublevel, u->handle);
}

struct userrec *next_hub(struct userrec *current, char *lowval, char *highval)
{

/*
  starting at "current" or "userlist" if NULL, find next bot with a 
  link_pref_val higher than "lowval" and lower than "highval"
  If none found return bot with best overall link_pref_val
  If still not found return NULL
*/
  char thisval[NICKLEN + 4],
    bestmatchval[NICKLEN + 4] = "z",
    bestallval[NICKLEN + 4] = "z";
  struct userrec *cur = NULL,
   *bestmatch = NULL,
   *bestall = NULL;

  if (current)
    cur = current->next;
  else
    cur = userlist;
  while (cur != current) {
    if (!cur)
      cur = userlist;
    if (cur == current)
      break;
    if ((cur->flags & USER_BOT) && (strcmp(cur->handle, botnetnick))) {
      link_pref_val(cur, thisval);
      if ((strcmp(thisval, lowval) < 0) && (strcmp(thisval, highval) > 0) &&(strcmp(thisval, bestmatchval) < 0)) {
	strcpy(bestmatchval, thisval);
	bestmatch = cur;
      }
      if ((strcmp(thisval, lowval) < 0)
	  && (strcmp(thisval, bestallval) < 0)) {
	strcpy(bestallval, thisval);
	bestall = cur;
      }
    }
    cur = cur->next;
  }
  if (bestmatch)
    return bestmatch;
  if (bestall)
    return bestall;
  return NULL;
}

#ifdef HUB
void autolink_cycle_hub(char *start)
{
  struct userrec *u = NULL;
  int i;
  char bestval[HANDLEN + 4],
    curval[HANDLEN + 4],
    myval[HANDLEN + 4];

  u = get_user_by_handle(userlist, botnetnick);
  link_pref_val(u, myval);
  strcpy(bestval, myval);
  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type == &DCC_BOT_NEW)
      return;
    if (dcc[i].type == &DCC_FORK_BOT)
      return;
    if (dcc[i].type == &DCC_BOT) {
      if (dcc[i].status & (STAT_SHARE | STAT_OFFERED | STAT_SENDING | STAT_GETTING)) {
	link_pref_val(dcc[i].user, curval);

	if (strcmp(myval, curval)<0) {
	  /* I should be aggressive to this one */
	  if (dcc[i].status & STAT_AGGRESSIVE) {
	    log(LCAT_ERROR, STR("Passively sharing with %s but should be aggressive"), 
		dcc[i].user->handle);
	    log(LCAT_DEBUG, STR("My linkval: %s - %s linkval: %s"), myval, dcc[i].nick, curval);
	    botunlink(-2, dcc[i].nick, STR("Marked passive, should be aggressive"));
	    return;
	  }
	} else {
	  /* I should be passive to this one */
	  if (!(dcc[i].status & STAT_AGGRESSIVE)) {
	    log(LCAT_ERROR, STR("Aggressively sharing with %s but should be passive"), 
		dcc[i].user->handle);
	    log(LCAT_DEBUG, STR("My linkval: %s - %s linkval: %s"), myval, dcc[i].nick, curval);
	    botunlink(-2, dcc[i].nick, STR("Marked aggressive, should be passive"));
	    return;
	  }
	  if (strcmp(curval, bestval) < 0)
	    strcpy(bestval, curval);
	}
      } else {
	botunlink(-2, dcc[i].nick, STR("Linked but not sharing?"));
      }
    }
  }
  if (start)
    u = get_user_by_handle(userlist, start);
  else
    u = NULL;
  if (u) {
    link_pref_val(u, curval);
    if (strcmp(bestval, curval) < 0) {
      /* This shouldn't happen. Getting a failed link attempt (start!=NULL)
         while a dcc scan indicates we *are* connected to a better bot than
         the one we failed a link to.
       */
      log(LCAT_ERROR, STR("Failed link attempt to %s but connected to %s already???"), u->handle, (char *) &bestval[3]);
      return;
    }
  } else
    strcpy(curval, "0");
  u = next_hub(u, bestval, curval);
  if ((u) && (!in_chain(u->handle)))
    botlink("", -3, u->handle);
}
#endif

#ifdef LEAF
typedef struct hublist_entry {
  struct hublist_entry *next;
  struct userrec *u;
} tag_hublist_entry;

int botlinkcount = 0;

void autolink_cycle_leaf(char *start)
{
  struct userrec *u = NULL,
   *ul = NULL;
  struct hublist_entry *hl = NULL,
   *hl2 = NULL;
  struct bot_addr *ba;
  char uplink[HANDLEN + 1],
    avoidbot[HANDLEN + 1],
    curhub[HANDLEN + 1];
  int i,
    hlc;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0 };

  /* Reset connection attempts if we ain't called due to a failed link */
  if (!start)
    botlinkcount = 0;
  u = get_user_by_handle(userlist, botnetnick);
  ba = get_user(&USERENTRY_BOTADDR, u);
  if (ba && (ba->uplink[0])) {
    strncpy0(uplink, ba->uplink, sizeof(uplink));
  } else
    uplink[0] = 0;

  curhub[0] = 0;
  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type == &DCC_BOT_NEW)
      return;
    if (dcc[i].type == &DCC_FORK_BOT)
      return;
    if (dcc[i].type == &DCC_BOT) {
      strcpy(curhub, dcc[i].nick);
      break;
    }
  }

  if (curhub[0]) {
    /* we got a hub */
    if (!strcmp(curhub, uplink))
      /* Connected to uplink, nothing more to do */
      return;

    if (start)
      /* Failed a link... let's just wait for next regular call */
      return;

    if (uplink[0]) {
      /* Trying the uplink */
      botlink("", -3, uplink);
      return;
    }

    /* we got a hub currently, and no set uplink. Stay here */
    return;
  } else {
    /* no hubs connected... pick one */
    if (!start) {
      /* Regular interval call, no previous failed link */
      if (uplink[0]) {
	/* We have a set uplink, try it */
	botlink("", -3, uplink);
	return;
      }
      /* No preferred uplink, we need a random bot */
      avoidbot[0] = 0;
    } else {
      /* We got a failed link... */
      botlinkcount++;
      if (botlinkcount >= 3)
	/* tried 3+ random hubs without success, wait for next regular interval call */
	return;
      /* We need a random bot but *not* the last we tried */
      strcpy(avoidbot, start);
    }
  }

  /* Pick a random hub, but avoid 'avoidbot' */

  ul = userlist;
  hlc = 0;
  while (ul) {
    get_user_flagrec(ul, &fr, NULL);
    if (glob_bot(fr) && strcmp(ul->handle, botnetnick) && strcmp(u->handle, avoidbot) && (bot_hublevel(ul) < 999)) {
      hl2 = hl;
      hl = nmalloc(sizeof(struct hublist_entry));
      bzero(hl, sizeof(struct hublist_entry));

      hl->next = hl2;
      hlc++;
      hl->u = ul;
    }
    ul = ul->next;
  }
  hlc = rand() % hlc;
  while (hl) {
    if (!hlc)
      botlink("", -3, hl->u->handle);
    hlc--;
    hl2 = hl->next;
    nfree(hl);
    hl = hl2;
  }
}
#endif

void autolink_cycle(char *start)
{
  Context;
#ifdef HUB
  autolink_cycle_hub(start);
#endif
#ifdef LEAF
  autolink_cycle_leaf(start);
#endif
}
