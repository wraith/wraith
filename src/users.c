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
 */

#include "common.h"
#include "users.h"
#include "rfc1459.h"
#include "src/mod/share.mod/share.h"
#include "dcc.h"
#include "settings.h"
#include "userrec.h"
#include "misc.h"
#include "cfg.h"
#include "match.h"
#include "main.h"
#include "chanprog.h"
#include "dccutil.h"
#include "crypt.h"
#include "botnet.h"
#include "chan.h"
#include "tandem.h"
#include "src/mod/channels.mod/channels.h"
#include "src/mod/notes.mod/notes.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef HUB
#include "misc_file.h"
#endif /* HUB */

char natip[121] = "";
char userfile[121] = "";	/* where the user records are stored */
int ignore_time = 10;		/* how many minutes will ignores last? */

/* is this nick!user@host being ignored? */
int match_ignore(char *uhost)
{
  struct igrec *ir = NULL;

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
  int i, j;
  struct igrec **u = NULL, *t = NULL;
  char temp[256] = "";

  i = 0;
  if (!strchr(ign, '!') && (j = atoi(ign))) {
    for (u = &global_ign, j--; *u && j; u = &((*u)->next), j--);
    if (*u) {
      strncpyz(temp, (*u)->igmask, sizeof temp);
      i = 1;
    }
  } else {
    /* find the matching host, if there is one */
    for (u = &global_ign; *u && !i; u = &((*u)->next))
      if (!rfc_casecmp(ign, (*u)->igmask)) {
        strncpyz(temp, ign, sizeof temp);
	i = 1;
	break;
      }
  }
  if (i) {
    if (!noshare) {
      char *mask = str_escape(temp, ':', '\\');

      if (mask) {
	shareout("-i %s\n", mask);
	free(mask);
      }
    }
    free((*u)->igmask);
    if ((*u)->msg)
      free((*u)->msg);
    if ((*u)->user)
      free((*u)->user);
    t = *u;
    *u = (*u)->next;
    free(t);
  }
  return i;
}

void addignore(char *ign, char *from, char *mnote, time_t expire_time)
{
  struct igrec *p = NULL, *l = NULL;

  for (l = global_ign; l; l = l->next)
    if (!rfc_casecmp(l->igmask, ign)) {
      p = l;
      break;
    }

  if (p == NULL) {
    p = (struct igrec *) calloc(1, sizeof(struct igrec));
    p->next = global_ign;
    global_ign = p;
  } else {
    free(p->igmask);
    free(p->user);
    free(p->msg);
  }

  p->expire = expire_time;
  p->added = now;
  p->flags = expire_time ? 0 : IGREC_PERM;
  p->igmask = strdup(ign);
  p->user = strdup(from);
  p->msg = strdup(mnote);
  if (!noshare) {
    char *mask = str_escape(ign, ':', '\\');

    if (mask) {
      shareout("+i %s %li %c %s %s\n", mask, expire_time - now, (p->flags & IGREC_PERM) ? 'p' : '-', from, mnote);
      free(mask);
    }
  }
}

/* take host entry from ignore list and display it ignore-style */
void display_ignore(int idx, int number, struct igrec *ignore)
{
  char dates[81] = "", s[41] = "";

  if (ignore->added) {
    daysago(now, ignore->added, s);
    sprintf(dates, "Started %s", s);
  } else
    dates[0] = 0;
  if (ignore->flags & IGREC_PERM)
    strcpy(s, "(perm)");
  else {
    char s1[41] = "";

    days(ignore->expire, now, s1);
    sprintf(s, "(expires %s)", s1);
  }
  if (number >= 0)
    dprintf(idx, "  [%3d] %s %s\n", number, ignore->igmask, s);
  else
    dprintf(idx, "IGNORE: %s %s\n", ignore->igmask, s);
  if (ignore->msg && ignore->msg[0])
    dprintf(idx, "        %s: %s\n", ignore->user, ignore->msg);
  else
    dprintf(idx, "        %s %s\n", MODES_PLACEDBY, ignore->user);
  if (dates[0])
    dprintf(idx, "        %s\n", dates);
}

/* list the ignores and how long they've been active */
void tell_ignores(int idx, char *match)
{
  struct igrec *u = global_ign;
  int k = 1;

  if (u == NULL) {
    dprintf(idx, "No ignores.\n");
    return;
  }
  dprintf(idx, "%s:\n", IGN_CURRENT);
  for (; u; u = u->next) {
    if (match[0]) {
      if (wild_match(match, u->igmask) ||
	  wild_match(match, u->msg) ||
	  wild_match(match, u->user))
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
      putlog(LOG_MISC, "*", "%s %s (%s)", IGN_NOLONGER, (*u)->igmask, MISC_EXPIRED);
      delignore((*u)->igmask);
    } else {
      u = &((*u)->next);
    }
  }
}

/*        Channel mask loaded from user file. This function is
 *      add(ban|invite|exempt)_fully merged into one. <cybah>
 */
static void addmask_fully(struct chanset_t *chan, maskrec **m, maskrec **global,
                         char *mask, char *from,
			 char *note, time_t expire_time, int flags,
			 time_t added, time_t last)
{
  maskrec *p = (maskrec *) calloc(1, sizeof(maskrec));
  maskrec **u = (chan) ? m : global;

  p->next = *u;
  *u = p;
  p->expire = expire_time;
  p->added = added;
  p->lastactive = last;
  p->flags = flags;
  p->mask = strdup(mask);
  p->user = strdup(from);
  p->desc = strdup(note);
}

static void restore_chanban(struct chanset_t *chan, char *host)
{
  char *expi = NULL, *add = NULL, *last = NULL, *user = NULL, *desc = NULL;
  int flags = 0;

  expi = strchr_unescape(host, ':', '\\');
  if (expi) {
    if (*expi == '+') {
      flags |= MASKREC_PERM;
      expi++;
    }
    add = strchr(expi, ':');
    if (add) {
      if (add[-1] == '*') {
	flags |= MASKREC_STICKY;
	add[-1] = 0;
      } else
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
	      addmask_fully(chan, &chan->bans, &global_bans, host, user,
			    desc, atoi(expi), flags, atoi(add), atoi(last));
	      return;
	    }
	  }
	}
      } else {
	desc = strchr(add, ':');
	if (desc) {
	  *desc = 0;
	  desc++;
	  addmask_fully(chan, &chan->bans, &global_bans, host, add, desc,
			atoi(expi), flags, now, 0);
	  return;
	}
      }
    }
  }
  putlog(LOG_MISC, "*", "*** Malformed banline for %s.", chan ? chan->dname : "global_bans");
}

static void restore_chanexempt(struct chanset_t *chan, char *host)
{
  char *expi = NULL, *add = NULL, *last = NULL, *user = NULL, *desc = NULL;
  int flags = 0;

  expi = strchr_unescape(host, ':', '\\');
  if (expi) {
      if (*expi == '+') {
	flags |= MASKREC_PERM;
	expi++;
      }
    add = strchr(expi, ':');
    if (add) {
      if (add[-1] == '*') {
	flags |= MASKREC_STICKY;
	add[-1] = 0;
      } else
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
	      addmask_fully(chan, &chan->exempts, &global_exempts, host, user,
			    desc, atoi(expi), flags, atoi(add), atoi(last));
	      return;
	    }
	  }
	}
      } else {
	desc = strchr(add, ':');
	if (desc) {
	  *desc = 0;
	  desc++;
	  addmask_fully(chan, &chan->exempts, &global_exempts, host, add,
			desc, atoi(expi), flags, now, 0);
	  return;
	}
      }
    }
  }
  putlog(LOG_MISC, "*", "*** Malformed exemptline for %s.", chan ? chan->dname : "global_exempts");
}

static void restore_chaninvite(struct chanset_t *chan, char *host)
{
  char *expi = NULL, *add = NULL, *last = NULL, *user = NULL, *desc = NULL;
  int flags = 0;

  expi = strchr_unescape(host, ':', '\\');
  if (expi) {
    if (*expi == '+') {
      flags |= MASKREC_PERM;
      expi++;
    }
    add = strchr(expi, ':');
    if (add) {
      if (add[-1] == '*') {
	flags |= MASKREC_STICKY;
	add[-1] = 0;
      } else
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
	      addmask_fully(chan, &chan->invites, &global_invites, host, user,
			    desc, atoi(expi), flags, atoi(add), atoi(last));
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
  putlog(LOG_MISC, "*", "*** Malformed inviteline for %s.", chan ? chan->dname : "global_invites");
}

static void restore_ignore(char *host)
{
  char *expi = NULL, *user = NULL, *added = NULL, *desc = NULL;
  int flags = 0;
  struct igrec *p = NULL;

  expi = strchr_unescape(host, ':', '\\');
  if (expi) {
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
      p = (struct igrec *) calloc(1, sizeof(struct igrec));

      p->next = global_ign;
      global_ign = p;
      p->expire = atoi(expi);
      p->added = atoi(added);
      p->flags = flags;
      p->igmask = strdup(host);
      p->user = strdup(user);
      if (desc) {
        p->msg = strdup(desc);
      } else
	p->msg = NULL;
      return;
    }
  }
  putlog(LOG_MISC, "*", "*** Malformed ignore line.");
}

static void 
tell_user(int idx, struct userrec *u)
{
  char s[81] = "", s1[81] = "", format[81] = "";
  int n = 0;
  time_t now2;
  struct chanuserrec *ch = NULL;
  struct chanset_t *chan = NULL;
  struct user_entry *ue = NULL;
  struct laston_info *li = NULL;
  struct flag_record fr = {FR_GLOBAL, 0, 0, 0 };

  n = num_notes(u->handle);

  fr.global = u->flags;
  build_flags(s, &fr, NULL);
  li = get_user(&USERENTRY_LASTON, u);
  if (!li || !li->laston)
    strcpy(s1, "never");
  else {
    now2 = now - li->laston;
    if (now2 > 86400)
      egg_strftime(s1, 7, "%d %b", gmtime(&li->laston));
    else
      egg_strftime(s1, 6, "%H:%M", gmtime(&li->laston));
  }
  if (!u->bot) {
    egg_snprintf(format, sizeof format, "%%-%us %%-5s%%5d %%-15s %%s (%%-10.10s)\n", HANDLEN);
    dprintf(idx, format, u->handle, get_user(&USERENTRY_PASS, u) ? "yes" : "no", n, s, s1, (li && li->lastonplace) ? li->lastonplace : "nowhere");
  } else {	/* BOT */
    egg_snprintf(format, sizeof format, "%%-%us %%-8s %%s (%%-10.10s)\n", HANDLEN);
    dprintf(idx, format, u->handle, s, s1, (li && li->lastonplace) ? li->lastonplace : "nowhere");
  }  
  /* channel flags? */
  for (ch = u->chanrec; ch; ch = ch->next) {
    fr.match = FR_CHAN | FR_GLOBAL;
    chan = findchan_by_dname(ch->channel);
    get_user_flagrec(dcc[idx].user, &fr, ch->channel);
    if (!channel_private(chan) || (channel_private(chan) && (chan_op(fr) || glob_owner(fr)))) {
      if (glob_op(fr) || chan_op(fr)) {
        if (ch->laston == 0L)
  	  strcpy(s1, "never");
        else {
  	  now2 = now - (ch->laston);
	  if (now2 > 86400)
	    egg_strftime(s1, 7, "%d %b", gmtime(&ch->laston));
	  else
	    egg_strftime(s1, 6, "%H:%M", gmtime(&ch->laston));
        }
        fr.match = FR_CHAN;
        fr.chan = ch->flags;
        build_flags(s, &fr, NULL);
        egg_snprintf(format, sizeof format, "%%%us  %%-18s %%-15s %%s\n", HANDLEN-9);
        dprintf(idx, format, " ", ch->channel, s, s1);
        if (ch->info != NULL)
  	  dprintf(idx, "    INFO: %s\n", ch->info);
      }
    }
  }
  /* user-defined extra fields */
  for (ue = u->entries; ue; ue = ue->next)
    if (!ue->name && ue->type->display)
      ue->type->display(idx, ue, u);
}

/* show user by ident */
void tell_user_ident(int idx, char *id)
{
  char format[81] = "";
  struct userrec *u = NULL;
  struct flag_record user = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };

  get_user_flagrec(dcc[idx].user, &user, NULL);

  u = get_user_by_handle(userlist, id);
  if (u == NULL)
    u = get_user_by_host(id);

  if (u == NULL || (u && !whois_access(dcc[idx].user, u))) {
    dprintf(idx, "%s.\n", USERF_NOMATCH);
    return;
  }
  if (u->bot) {
    egg_snprintf(format, sizeof format, "%%-%us FLAGS    LAST\n", HANDLEN);
    dprintf(idx, format, "BOTNICK");
  } else {
    egg_snprintf(format, sizeof format, "%%-%us PASS NOTES FLAGS           LAST\n", HANDLEN);
    dprintf(idx, format, "HANDLE");
  }
  tell_user(idx, u);
}

/* match string:
 * wildcard to match nickname or hostmasks
 * +attr to find all with attr */
void tell_users_match(int idx, char *mtch, int start, int limit, char *chname, int isbot)
{
  char format[81] = "";
  struct userrec *u = NULL;
  int fnd = 0, cnt, nomns = 0, flags = 0;
  struct list_type *q = NULL;
  struct flag_record user, pls, mns;

  dprintf(idx, "*** %s '%s':\n", MISC_MATCHING, mtch);
  cnt = 0;
  if (isbot) {
    egg_snprintf(format, sizeof format, "%%-%us FLAGS    LAST\n", HANDLEN);
    dprintf(idx, format, "BOTNICK");
  } else {
    egg_snprintf(format, sizeof format, "%%-%us PASS NOTES FLAGS           LAST\n", HANDLEN);
    dprintf(idx, format, "HANDLE");
  }
  if (start > 1)
    dprintf(idx, "(%s %d)\n", MISC_SKIPPING, start - 1);
  if (strchr("+-&|", *mtch)) {
    user.match = pls.match = FR_GLOBAL | FR_CHAN;
    break_down_flags(mtch, &pls, &mns);
    mns.match = pls.match ^ (FR_AND | FR_OR);
    if (!mns.global && !mns.chan) {
      nomns = 1;
      if (!pls.global && !pls.chan) {
	/* happy now BB you weenie :P */
	dprintf(idx, "Unknown flag specified for matching!!\n");
	return;
      }
    }
    if (!chname || !chname[0])
      chname = dcc[idx].u.chat->con_chan;
    flags = 1;
  }
  for (u = userlist; u; u = u->next) {
    if (!whois_access(dcc[idx].user, u) || (isbot && !u->bot) || (!isbot && u->bot)) {
      continue;
    } else if (flags) {
      get_user_flagrec(u, &user, chname);
      if (flagrec_eq(&pls, &user)) {
	if (nomns || !flagrec_eq(&mns, &user)) {
	  cnt++;
	  if ((cnt <= limit) && (cnt >= start))
	    tell_user(idx, u);
	  if (cnt == limit + 1)
	    dprintf(idx, MISC_TRUNCATED, limit);
	}
      }
    } else if (wild_match(mtch, u->handle)) {
      cnt++;
      if ((cnt <= limit) && (cnt >= start))
	tell_user(idx, u);
      if (cnt == limit + 1)
	dprintf(idx, MISC_TRUNCATED, limit);
    } else {
      fnd = 0;
      for (q = get_user(&USERENTRY_HOSTS, u); q; q = q->next) {
	if ((wild_match(mtch, q->extra)) && (!fnd)) {
	  cnt++;
	  fnd = 1;
	  if ((cnt <= limit) && (cnt >= start)) {
	    tell_user(idx, u);
	  }
	  if (cnt == limit + 1)
	    dprintf(idx, MISC_TRUNCATED, limit);
	}
      }
    }
  }
  dprintf(idx, MISC_FOUNDMATCH, cnt, cnt == 1 ? "" : MISC_MATCH_PLURAL);
}

#ifdef HUB
void backup_userfile()
{
  char s[DIRMAX] = "", s2[DIRMAX] = "";

  putlog(LOG_MISC, "*", USERF_BACKUP);
  egg_snprintf(s, sizeof s, "%s.u.0", tempdir);
  egg_snprintf(s2, sizeof s2, "%s.u.1", tempdir);
  movefile(s, s2);
  copyfile(userfile, s);
}
#endif /* HUB */

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
 * + denotes tcl command
 * <handle> begin user entry
 * --KEY INFO - info on each
 * NEWER:
 * % exemptmask(s)
 * @ Invitemask(s)
 * *exempt global exempts
 * *Invite global Invites
 * && channel-specific exempts
 * &&#chan channel exempts
 * $$ channel-specific Invites
 * $$#chan channel Invites
 */

int readuserfile(const char *file, struct userrec **ret)
{
  char *p = NULL, buf[1024] = "", lasthand[512] = "", *attr = NULL, *pass = NULL;
  char *code = NULL, s1[1024] = "", *s = NULL, cbuf[1024] = "", *temps = NULL, ignored[512] = "";
  FILE *f = NULL;
  struct userrec *bu = NULL, *u = NULL;
  struct chanset_t *cst = NULL;
  struct flag_record fr;
  struct chanuserrec *cr = NULL;
  int i, line = 0;

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
  noshare = 1;
  /* read opening comment */
  s = buf;
  fgets(cbuf, 180, f);
  remove_crlf(cbuf);
  temps = (char *) decrypt_string(settings.salt1, cbuf);
  egg_snprintf(s, 180, "%s", temps);
  free(temps);
  if (s[1] < '4') {
    fatal(USERF_OLDFMT, 0);
  }
  if (s[1] > '4')
    fatal(USERF_INVALID, 0);
  while (!feof(f)) {
    s = buf;
    fgets(cbuf, 1024, f);
    remove_crlf(cbuf);
    temps = (char *) decrypt_string(settings.salt1, cbuf);
    egg_snprintf(s, 1024, "%s", temps);
    free(temps);
    if (!feof(f)) {
      line++;
      if (s[0] != '#' && s[0] != ';' && s[0]) {
	code = newsplit(&s);
	rmspace(s);
	if (!strcmp(code, "-")) {
	  if (!lasthand[0])
	    continue;		/* Skip this entry.	*/
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
	    if (lasthand[0] && strchr(CHANMETA, lasthand[0]) != NULL)
	      restore_chanban(cst, s);
	    else if (lasthand[0] == '*') {
	      if (lasthand[1] == IGNORE_NAME[1])
		restore_ignore(s);
              else if (lasthand[1] == CONFIG_NAME[1]) {
                set_cmd_pass(s, 1);
              }
	      else
		restore_chanban(NULL, s);
	    } else if (lasthand[0])
	      set_user(&USERENTRY_HOSTS, u, s);
	  }
	} else if (!strcmp(code, "%")) { /* exemptmasks */
	  if (!lasthand[0])
	    continue;		/* Skip this entry.	*/
	  if (s[0]) {
	    if (lasthand[0] == '#' || lasthand[0] == '+')
	      restore_chanexempt(cst,s);
	    else if (lasthand[0] == '*')
	      if (lasthand[1] == EXEMPT_NAME[1])
		restore_chanexempt(NULL, s);
	  }
	} else if (!strcmp(code, "@")) { /* Invitemasks */
	  if (!lasthand[0])
	    continue;		/* Skip this entry.	*/
	  if (s[0]) {
	    if (lasthand[0] == '#' || lasthand[0] == '+') {
	      restore_chaninvite(cst,s);
	    } else if (lasthand[0] == '*') {
	      if (lasthand[1] == INVITE_NAME[1]) {
		restore_chaninvite(NULL, s);
              } else if (lasthand[1] == CONFIG_NAME[1]) {
                userfile_cfg_line(s);
              }
            }
	  }
	} else if (!strcmp(code, "!")) {
	  /* ! #chan laston flags [info] */
	  char *chname = NULL, *st = NULL, *fl = NULL;

	  if (u) {
	    chname = newsplit(&s);
	    st = newsplit(&s);
	    fl = newsplit(&s);
	    rmspace(s);
	    fr.match = FR_CHAN;
	    break_down_flags(fl, &fr, 0);
	    if (findchan_by_dname(chname)) {
	      for (cr = u->chanrec; cr; cr = cr->next)
		if (!rfc_casecmp(cr->channel, chname))
		  break;
	      if (!cr) {
		cr = (struct chanuserrec *) calloc(1, sizeof(struct chanuserrec));

		cr->next = u->chanrec;
		u->chanrec = cr;
		strncpyz(cr->channel, chname, 80);
		cr->laston = atoi(st);
		cr->flags = fr.chan;
		if (s[0]) {
                  cr->info = strdup(s);
		} else
		  cr->info = NULL;
	      }
	    }
	  }
        } else if (!strcmp(code, "+")) {
         if (s[0] && lasthand[0] == '*' && lasthand[1] == CHANS_NAME[1]) {
           char *options = NULL, *chan = NULL, *my_ptr = NULL;
           char resultbuf[2048] = "";

           options = my_ptr = strdup(s);

           newsplit(&options);
           newsplit(&options);
           chan = newsplit(&options);

           /* hack to remove { } */
           newsplit(&options);
           options[strlen(options) - 1] = 0;

           if (channel_add(resultbuf, chan, options) != OK) {
             putlog(LOG_MISC, "*", "Channel parsing error in userfile on line %d", line);
             free(my_ptr);
             fclose(f);
             return 0;
           }
           free(my_ptr);
         }
	} else if (!strncmp(code, "::", 2)) {
	  /* channel-specific bans */
	  strcpy(lasthand, &code[2]);
	  u = NULL;
	  if (!findchan_by_dname(lasthand)) {
	    strcpy(s1, lasthand);
	    strcat(s1, " ");
	    if (strstr(ignored, s1) == NULL) {
	      strcat(ignored, lasthand);
	      strcat(ignored, " ");
	    }
	    lasthand[0] = 0;
	  } else {
	    /* Remove all bans for this channel to avoid dupes */
	    /* NOTE only remove bans for when getting a userfile
	     * from another bot & that channel is shared */
	    cst = findchan_by_dname(lasthand);
            clear_masks(cst->bans);
	    cst->bans = NULL;
	  }
	} else if (!strncmp(code, "&&", 2)) {
	  /* channel-specific exempts */
	  strcpy(lasthand, &code[2]);
	  u = NULL;
	  if (!findchan_by_dname(lasthand)) {
	    strcpy(s1, lasthand);
	    strcat(s1, " ");
	    if (strstr(ignored, s1) == NULL) {
	      strcat(ignored, lasthand);
	      strcat(ignored, " ");
	    }
	    lasthand[0] = 0;
	  } else {
	    /* Remove all exempts for this channel to avoid dupes */
	    /* NOTE only remove exempts for when getting a userfile
	     * from another bot & that channel is shared */
	    cst = findchan_by_dname(lasthand);
	    clear_masks(cst->exempts);
	    cst->exempts = NULL;
	  }
	} else if (!strncmp(code, "$$", 2)) {
	  /* channel-specific invites */
	  strcpy(lasthand, &code[2]);
	  u = NULL;
	  if (!findchan_by_dname(lasthand)) {
	    strcpy(s1, lasthand);
	    strcat(s1, " ");
	    if (strstr(ignored, s1) == NULL) {
	      strcat(ignored, lasthand);
	      strcat(ignored, " ");
	    }
	    lasthand[0] = 0;
	  } else {
	    /* Remove all invites for this channel to avoid dupes */
	    /* NOTE only remove invites for when getting a userfile
	     * from another bot & that channel is shared */
	    cst = findchan_by_dname(lasthand);
	    clear_masks(cst->invites);
            cst->invites = NULL;
	  }
	} else if (!strncmp(code, "--", 2)) {
	  if (u) {
	    /* new format storage */
	    struct user_entry *ue = NULL;
	    int ok = 0;

	    for (ue = u->entries; ue && !ok; ue = ue->next)
	      if (ue->name && !egg_strcasecmp(code + 2, ue->name)) {
		struct list_type *list = NULL;

		list = (struct list_type *) calloc(1, sizeof(struct list_type));

		list->next = NULL;
                list->extra = strdup(s);
		list_append((&ue->u.list), list);
		ok = 1;
	      }
	    if (!ok) {
	      ue = (struct user_entry *) calloc(1, sizeof(struct user_entry));

	      ue->name = (char *) calloc(1, strlen(code + 1));
	      ue->type = NULL;
	      strcpy(ue->name, code + 2);
	      ue->u.list = (struct list_type *) calloc(1, sizeof(struct list_type));

	      ue->u.list->next = NULL;
              ue->u.list->extra = strdup(s);
	      list_insert((&u->entries), ue);
	    }
	  }
	} else if (!rfc_casecmp(code, BAN_NAME)) {
	  strcpy(lasthand, code);
	  u = NULL;
	} else if (!rfc_casecmp(code, IGNORE_NAME)) {
	  strcpy(lasthand, code);
	  u = NULL;
	} else if (!rfc_casecmp(code, EXEMPT_NAME)) {
	  strcpy(lasthand, code);
	  u = NULL;
	} else if (!rfc_casecmp(code, INVITE_NAME)) {
	  strcpy(lasthand, code);
	  u = NULL;
        } else if (!rfc_casecmp(code, CHANS_NAME)) {
          strcpy(lasthand, code);
          u = NULL;
        } else if (!rfc_casecmp(code, CONFIG_NAME)) {
          strcpy(lasthand, code);
          u = NULL;  
	} else if (code[0] == '*') {
	  lasthand[0] = 0;
	  u = NULL;
	} else {		/* its a user ! */
	  pass = newsplit(&s);
	  attr = newsplit(&s);
	  rmspace(s);
	  if (!attr[0] || !pass[0]) {
	    putlog(LOG_MISC, "*", "* %s line: %d!", USERF_CORRUPT, line);
	    lasthand[0] = 0;
            fclose(f);
            return 0;
	  } else {
            int isbot = 0;

            if (code[0] == '-') {
              code++;
              isbot++;
            }

	    u = get_user_by_handle(bu, code);
	    if (u) {
	      putlog(LOG_ERROR, "@", "* %s '%s'!", USERF_DUPE, code);
	      lasthand[0] = 0;
	      u = NULL;
	    } else {
	      fr.match = FR_GLOBAL;
	      break_down_flags(attr, &fr, 0);
	      strcpy(lasthand, code);
	      cst = NULL;
/* FIXME: remove after 1.2 */
              if (fr.global & USER_BOT) {	/* this should pick up the old +b flag for now */
                isbot++;
                fr.global &= ~USER_BOT;
              }
              
	      if (strlen(code) > HANDLEN)
		code[HANDLEN] = 0;
	      if (strlen(pass) > 20) {
		putlog(LOG_MISC, "*", "* %s '%s'", USERF_BROKEPASS, code);
		strcpy(pass, "-");
	      }
	      bu = adduser(bu, code, 0, pass, sanity_check(fr.global, isbot), isbot);

	      u = get_user_by_handle(bu, code);
	      for (i = 0; i < dcc_total; i++)
		if (!egg_strcasecmp(code, dcc[i].nick))
		  dcc[i].user = u;
        
              if (!egg_strcasecmp(code, conf.bot->nick))
                conf.bot->u = u;

	      /* if s starts with '/' it's got file info */
	    }
	  }
	}
      }
    }
  }
  fclose(f);
  (*ret) = bu;
  if (ignored[0]) {
    putlog(LOG_MISC, "*", "%s %s", USERF_IGNBANS, ignored);
  }
  putlog(LOG_MISC, "*", "Userfile loaded, unpacking...");
  for (u = bu; u; u = u->next) {
    struct user_entry *e = NULL;

    if (!u->bot && !egg_strcasecmp (u->handle, conf.bot->nick)) {
      putlog(LOG_MISC, "*", "(!) I have a user record, but am not classified as a BOT!");
      u->bot = 1;
      /* u->flags |= USER_BOT; */
    }

    for (e = u->entries; e; e = e->next)
      if (e->name) {
	struct user_entry_type *uet = find_entry_type(e->name);

	if (uet) {
	  e->type = uet;
	  uet->unpack(u, e);
	  free(e->name);
	  e->name = NULL;
	}
      }
  }
  /* process the user data *now* */
#ifdef LEAF
  unlink(userfile);
#endif /* LEAF */
  noshare = 0;
  return 1;
}

void link_pref_val(struct userrec *u, char *val)
{

/* val must be HANDLEN + 4 chars minimum */
  struct bot_addr *ba = NULL;

  val[0] = 'Z';
  val[1] = 0;
  
  if (!u)
    return;

  if (!u->bot)
    return;
  if (!(ba = get_user(&USERENTRY_BOTADDR, u))) {
    return;
  }
  if (!ba->hublevel) {
    return;
  }
  sprintf(val, "%02d%s", ba->hublevel, u->handle);
}

/*
  starting at "current" or "userlist" if NULL, find next bot with a
  link_pref_val higher than "lowval" and lower than "highval"
  If none found return bot with best overall link_pref_val
  If still not found return NULL
*/
struct userrec *next_hub(struct userrec *current, char *lowval, char *highval)
{
  char thisval[NICKLEN + 4] = "", bestmatchval[NICKLEN + 4] = "z", bestallval[NICKLEN + 4] = "z";
  struct userrec *cur = NULL, *bestmatch = NULL, *bestall = NULL;

  if (current)
    cur = current->next;
  else
    cur = userlist;
  while (cur != current) {
    if (!cur)
      cur = userlist;
    if (cur == current)
      break;
    if (cur->bot && (strcmp(cur->handle, conf.bot->nick))) {
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
void autolink_cycle(char *start)
{
  char bestval[HANDLEN + 4] = "", curval[HANDLEN + 4] = "", myval[HANDLEN + 4] = "";
  struct userrec *u = NULL;
  tand_t *bot = NULL;
  int i;

  link_pref_val(conf.bot->u, myval);
  strcpy(bestval, myval);
  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type == &DCC_BOT_NEW)
      return;
    if (dcc[i].type == &DCC_FORK_BOT)
      return;
    if (dcc[i].type == &DCC_BOT) {
      if (dcc[i].status & (STAT_OFFEREDU | STAT_GETTINGU | STAT_SENDINGU))
        continue; /* lets let the binary have it's peace. */

      if ((bot = findbot(dcc[i].nick)) && bot->buildts != buildts)
        continue; /* same thing. */

      if (dcc[i].status & (STAT_SHARE | STAT_OFFERED | STAT_SENDING | STAT_GETTING)) {
	link_pref_val(dcc[i].user, curval);

	if (strcmp(myval, curval) < 0) {
	  /* I should be aggressive to this one */
	  if (dcc[i].status & STAT_AGGRESSIVE) {
	    putlog(LOG_MISC, "*", "Passively sharing with %s but should be aggressive", dcc[i].user->handle);
	    putlog(LOG_DEBUG, "*", "My linkval: %s - %s linkval: %s", myval, dcc[i].nick, curval);
	    botunlink(-2, dcc[i].nick, "Marked passive, should be aggressive");
	    return;
	  }
	} else {
	  /* I should be passive to this one */
	  if (!(dcc[i].status & STAT_AGGRESSIVE)) {
	    putlog(LOG_MISC, "*", "Aggressively sharing with %s but should be passive", dcc[i].user->handle);
	    putlog(LOG_DEBUG, "*", "My linkval: %s - %s linkval: %s", myval, dcc[i].nick, curval);
	    botunlink(-2, dcc[i].nick, "Marked aggressive, should be passive");
	    return;
	  }
	  if (strcmp(curval, bestval) < 0)
	    strcpy(bestval, curval);
	}
      } else {
  	  botunlink(-2, dcc[i].nick, "Linked but not sharing?");
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
      putlog(LOG_BOTS, "*",  "Failed link attempt to %s but connected to %s already???", u->handle, (char *) &bestval[2]);
      return;
    }
  } else
    strcpy(curval, "0");

  u = next_hub(u, bestval, curval);
  if ((u) && (!in_chain(u->handle)))
    botlink("", -3, u->handle);
}
#endif /* HUB */

#ifdef LEAF
typedef struct hublist_entry {
  struct hublist_entry *next;
  struct userrec *u;
} tag_hublist_entry;

int botlinkcount = 0;

void autolink_cycle(char *start)
{
  struct userrec *u = NULL;
  struct hublist_entry *hl = NULL, *hl2 = NULL;
  struct bot_addr *my_ba = NULL;
  char uplink[HANDLEN + 1] = "", avoidbot[HANDLEN + 1] = "", curhub[HANDLEN + 1] = "";
  int i, hlc;
  struct flag_record fr = { FR_GLOBAL, 0, 0, 0 };

  /* Reset connection attempts if we ain't called due to a failed link */
  if (!start)
    botlinkcount = 0;

  my_ba = get_user(&USERENTRY_BOTADDR, conf.bot->u);
  if (my_ba && (my_ba->uplink[0])) {
    strncpyz(uplink, my_ba->uplink, sizeof(uplink));
  } else {
    uplink[0] = 0;
  }
  curhub[0] = 0;
  for (i = 0; i < dcc_total; i++) {
    if ((dcc[i].type == &DCC_BOT_NEW) || (dcc[i].type == &DCC_FORK_BOT))
      return;
    if (dcc[i].type == &DCC_BOT) {
      strcpy(curhub, dcc[i].nick);
      break;
    }
  }

  if (curhub[0]) {
    /* we are linked to a bot (hub) */
    if (uplink[0] && !strcmp(curhub, uplink))
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
  hlc = 0;
  for (u = userlist; u; u = u->next) {
    get_user_flagrec(u, &fr, NULL);
    if (glob_bot(fr) && strcmp(u->handle, conf.bot->nick) && strcmp(u->handle, avoidbot) && (bot_hublevel(u) < 999)) {
      putlog(LOG_DEBUG, "@", "Adding %s to hublist", u->handle);
      hl2 = hl;
      hl = (struct hublist_entry *) calloc(1, sizeof(struct hublist_entry));

      hl->next = hl2;
      hlc++;
      hl->u = u;
    }
  }
  putlog(LOG_DEBUG, "@", "Picking random hub from %d hubs", hlc);
  /* This is mainly a sanity check if the userfile gets fucked :P */
  if (!hlc)
   fatal("userlist died!", 0);
  hlc = randint(hlc);
  putlog(LOG_DEBUG, "@", "Picked #%d for hub", hlc);
  while (hl) {
    if (!hlc) {
      putlog(LOG_DEBUG, "@", "Which is bot: %s", hl->u->handle);
      botlink("", -3, hl->u->handle);
    }
    hlc--;
    hl2 = hl->next;
    free(hl);
    hl = hl2;
  }
}
#endif /* LEAF */

