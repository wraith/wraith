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
#include "set.h"
#include "match.h"
#include "main.h"
#include "chanprog.h"
#include "dccutil.h"
#include "crypt.h"
#include "botnet.h"
#include "chan.h"
#include "tandem.h"
#include "src/mod/channels.mod/channels.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include "misc_file.h"
#include "EncryptedStream.h"

char userfile[121] = "";	/* where the user records are stored */
char autolink_failed[HANDLEN + 1] = "";
interval_t ignore_time = 10;		/* how many minutes will ignores last? */
bool	dont_restructure = 0;		/* set when we botlink() to a hub with +U, only stops bot from restructuring */

/* is this nick!user@host being ignored? */
bool match_ignore(char *uhost)
{
  for (struct igrec *ir = global_ign; ir; ir = ir->next)
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

char *delignore(char *ign)
{
  int i = 0, j;
  struct igrec **u = NULL, *t = NULL;
  static char temp[256] = "";

  if (!strchr(ign, '!') && (j = atoi(ign))) {
    for (u = &global_ign, j--; *u && j; u = &((*u)->next), j--)
      ;
    if (*u) {
      strlcpy(temp, (*u)->igmask, sizeof temp);
      i = 1;
    }
  } else {
    /* find the matching host, if there is one */
    for (u = &global_ign; *u && !i; u = &((*u)->next))
      if (!rfc_casecmp(ign, (*u)->igmask)) {
        strlcpy(temp, ign, sizeof temp);
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
    free((*u)->msg);
    free((*u)->user);
    t = *u;
    *u = (*u)->next;
    free(t);
  } else
    temp[0] = 0;

  return temp;
}

void addignore(char *ign, char *from, const char *mnote, time_t expire_time)
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
      shareout("+i %s %d %c %s %s\n", mask, (int) (expire_time - now), (p->flags & IGREC_PERM) ? 'p' : '-', from, mnote);
      free(mask);
    }
  }
}

/* take host entry from ignore list and display it ignore-style */
void display_ignore(int idx, int number, struct igrec *ignore)
{
  char dates[81] = "", s[41] = "";

  if (ignore->added) {
    daysago(now, ignore->added, s, sizeof(s));
    simple_snprintf(dates, sizeof(dates), "Started %s", s);
  } 

  if (ignore->flags & IGREC_PERM)
    strlcpy(s, "(perm)", sizeof(s));
  else {
    char s1[41] = "";

    days(ignore->expire, now, s1, sizeof(s1));
    simple_snprintf(s, sizeof(s), "(expires %s)", s1);
  }
  if (number >= 0)
    dprintf(idx, "  [%3d] %s %s\n", number, ignore->igmask, s);
  else
    dprintf(idx, "IGNORE: %s %s\n", ignore->igmask, s);
  if (ignore->msg && ignore->msg[0])
    dprintf(idx, "        %s: %s\n", ignore->user, ignore->msg);
  else
    dprintf(idx, "        placed by %s\n", ignore->user);
  if (dates[0])
    dprintf(idx, "        %s\n", dates);
}

/* list the ignores and how long they've been active */
void tell_ignores(int idx, char *match)
{
  struct igrec *u = global_ign;

  if (u == NULL) {
    dprintf(idx, "No ignores.\n");
    return;
  }
  dprintf(idx, "%s:\n", "Currently ignoring");

  int k = 1;

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
      putlog(LOG_MISC, "*", "%s %s (expired)", "No longer ignoring", (*u)->igmask);
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

static void restore_chanmask(const char type, struct chanset_t *chan, char *host)
{
  char *expi = NULL, *add = NULL, *last = NULL, *user = NULL, *desc = NULL;
  int flags = 0;
  maskrec **chan_masks = NULL, **global_masks = NULL;
  const char *str_type = (type == 'b' ? "ban" : type == 'e' ? "exempt" : "invite");

  if (type == 'b') {
    if (chan)  chan_masks = &chan->bans;
    global_masks = &global_bans;
  } else if (type == 'e') {
    if (chan)  chan_masks = &chan->exempts;
    global_masks = &global_exempts;
  } else if (type == 'I') {
    if (chan)  chan_masks = &chan->invites;
    global_masks = &global_invites;
  }

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
	      addmask_fully(chan, chan_masks, global_masks, host, user,
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
	  addmask_fully(chan, chan_masks, global_masks, host, add, desc,
			atoi(expi), flags, now, 0);
	  return;
	}
      }
    }
  }
  putlog(LOG_MISC, "*", "*** Malformed %sline for %s%s%s.", str_type, chan ? chan->dname : "global_", chan ? "" : str_type, chan ? "" : "s");
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
  time_t now2;
  struct chanuserrec *ch = NULL;
  struct chanset_t *chan = NULL;
  struct user_entry *ue = NULL;
  struct laston_info *li = (struct laston_info *) get_user(&USERENTRY_LASTON, u);
  struct flag_record fr = {FR_GLOBAL, u->flags, 0, 0 };

  build_flags(s, &fr, NULL);

  if (!li || !li->laston)
    strlcpy(s1, "never", sizeof(s1));
  else {
    now2 = now - li->laston;
    if (now2 > 86400)
      strftime(s1, 7, "%d %b", gmtime(&li->laston));
    else
      strftime(s1, 6, "%H:%M", gmtime(&li->laston));
  }
  if (!u->bot) {
    simple_snprintf(format, sizeof format, "%%-%us %%-4s %%-15s %%s (%%-10.10s)\n", HANDLEN);
    dprintf(idx, format, u->handle, get_user(&USERENTRY_PASS, u) ? "yes" : "no", s, s1, (li && li->lastonplace) ? li->lastonplace : "nowhere");
  } else {	/* BOT */
    simple_snprintf(format, sizeof format, "%%-%us %%-8s %%s (%%-10.10s)\n", HANDLEN);
    dprintf(idx, format, u->handle, s, s1, (li && li->lastonplace) ? li->lastonplace : "nowhere");
  }  
  /* channel flags? */
  for (ch = u->chanrec; ch; ch = ch->next) {
    fr.match = FR_CHAN | FR_GLOBAL;
    chan = findchan_by_dname(ch->channel);
    get_user_flagrec(dcc[idx].user, &fr, ch->channel);
    if (!channel_privchan(chan) || (channel_privchan(chan) && (chan_op(fr) || glob_owner(fr)))) {
      if (glob_op(fr) || chan_op(fr)) {
        if (ch->laston == 0L)
  	  strlcpy(s1, "never", sizeof(s1));
        else {
  	  now2 = now - (ch->laston);
	  if (now2 > 86400)
	    strftime(s1, 7, "%d %b", gmtime(&ch->laston));
	  else
	    strftime(s1, 6, "%H:%M", gmtime(&ch->laston));
        }
        fr.match = FR_CHAN;
        fr.chan = ch->flags;
        build_flags(s, &fr, NULL);
        simple_snprintf(format, sizeof format, "%%%us  %%-18s %%-15s %%s\n", HANDLEN-9);
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
  struct userrec *u = get_user_by_handle(userlist, id);
  struct flag_record user = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };

  get_user_flagrec(dcc[idx].user, &user, NULL);

  if (u == NULL)
    u = get_user_by_host(id);

  if (u == NULL || (u && !whois_access(dcc[idx].user, u))) {
    dprintf(idx, "Can't find anyone matching that, or you have no access to view them.\n");
    return;
  }

  char format[81] = "";

  if (u->bot) {
    simple_snprintf(format, sizeof format, "%%-%us FLAGS    LAST\n", HANDLEN);
    dprintf(idx, format, "BOTNICK");
  } else {
    simple_snprintf(format, sizeof format, "%%-%us PASS FLAGS           LAST\n", HANDLEN);
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
  int fnd = 0, cnt = 0, nomns = 0, flags = 0;
  struct list_type *q = NULL;
  struct flag_record user, pls, mns;

  dprintf(idx, "*** Matching '%s':\n", mtch);
  if (isbot) {
    simple_snprintf(format, sizeof format, "%%-%us FLAGS    LAST\n", HANDLEN);
    dprintf(idx, format, "BOTNICK");
  } else {
    simple_snprintf(format, sizeof format, "%%-%us PASS FLAGS           LAST\n", HANDLEN);
    dprintf(idx, format, "HANDLE");
  }
  if (start > 1)
    dprintf(idx, "(skipping first %d)\n", start - 1);
  if (strchr("+-&|", *mtch)) {
    user.match = pls.match = FR_GLOBAL | FR_CHAN;
    if (isbot) {
      user.match |= FR_BOT;
      pls.match |= FR_BOT;
    }
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
	    dprintf(idx, "(more than %d matches; list truncated)\n", limit);
	}
      }
    } else if (wild_match(mtch, u->handle)) {
      cnt++;
      if ((cnt <= limit) && (cnt >= start))
	tell_user(idx, u);
      if (cnt == limit + 1)
	dprintf(idx, "(more than %d matches; list truncated)\n", limit);
    } else {
      fnd = 0;
      for (q = (struct list_type *) get_user(&USERENTRY_HOSTS, u); q; q = q->next) {
	if ((wild_match(mtch, q->extra)) && (!fnd)) {
	  cnt++;
	  fnd = 1;
	  if ((cnt <= limit) && (cnt >= start)) {
	    tell_user(idx, u);
	  }
	  if (cnt == limit + 1)
	    dprintf(idx, "(more than %d matches; list truncated)\n", limit);
	}
      }
    }
  }
  dprintf(idx, "--- Found %d match%s.\n", cnt, cnt == 1 ? "" : "es");
}

void backup_userfile()
{
  char s[DIRMAX] = "", s2[DIRMAX] = "";

  putlog(LOG_MISC, "*", "Backing up user file...");
  simple_snprintf(s, sizeof s, "%s/.u.0", conf.datadir);
  simple_snprintf(s2, sizeof s2, "%s/.u.1", conf.datadir);
  movefile(s, s2);
  copyfile(userfile, s);
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
  const char salt1[] = SALT1;
  EncryptedStream stream(salt1);
  if (stream.loadFile(file))
    return 1;
  return stream_readuserfile(stream, ret);
}

int stream_readuserfile(bd::Stream& stream, struct userrec **ret)
{
  char *p = NULL, buf[1024] = "", lasthand[512] = "", *attr = NULL, *pass = NULL;
  char *code = NULL, s1[1024] = "", *s = buf, ignored[512] = "";
  struct userrec *bu = NULL, *u = NULL;
  struct chanset_t *cst = NULL;
  struct flag_record fr;
  struct chanuserrec *cr = NULL;
  int i, line = 0;

  bu = (*ret);
  if (bu == userlist) {
    clear_chanlist();
    lastuser = NULL;
    global_bans = NULL;
    global_ign = NULL;
    global_exempts = NULL;
    global_invites = NULL;
  }
  noshare = 1;
  /* read opening comment */
  bd::String str(stream.getline(180));
  if (unlikely(str[1] < '4')) {
    putlog(LOG_MISC, "*", "!*! Empty or malformed userfile.");
    return 0;
  }
  if (unlikely(str[1] > '4')) {
    putlog(LOG_MISC, "*", "Invalid userfile format.");
    return 0;
  }
  while (stream.tell() < stream.length()) {
    s = buf;
    str = stream.getline(sizeof(buf)).chomp();
    strlcpy(s, str.c_str(), std::min(str.length() + 1, sizeof(buf)));
    if (1) {
      line++;
      if (s[0] != '#' && s[0] != ';' && s[0]) {
	code = newsplit(&s);
	rmspace(s);
	if (!strcmp(code, "-")) {	/* ignores/bans */
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
              restore_chanmask('b', cst, s);
	    else if (lasthand[0] == '*') {
	      if (lasthand[1] == IGNORE_NAME[1])
		restore_ignore(s);
              else if (lasthand[1] == SET_NAME[1]) {
                set_cmd_pass(s, 0);		/* no need to share here, if we have a new userfile
						 * then leaf bots under us also get the new userfile */
              }
	      else
                restore_chanmask('b', NULL, s);
	    } else if (lasthand[0])
	      set_user(&USERENTRY_HOSTS, u, s);
	  }
	} else if (!strcmp(code, "%")) { /* exemptmasks */
	  if (!lasthand[0])
	    continue;		/* Skip this entry.	*/
	  if (s[0]) {
	    if (lasthand[0] == '#' || lasthand[0] == '+')
              restore_chanmask('e', cst, s);
	    else if (lasthand[0] == '*')
	      if (lasthand[1] == EXEMPT_NAME[1])
                restore_chanmask('e', NULL, s);
	  }
	} else if (!strcmp(code, "@")) { /* Invitemasks */
	  if (!lasthand[0])
	    continue;		/* Skip this entry.	*/
	  if (s[0]) {
	    if (lasthand[0] == '#' || lasthand[0] == '+') {
              restore_chanmask('I', cst, s);
	    } else if (lasthand[0] == '*') {
	      if (lasthand[1] == INVITE_NAME[1]) {
                restore_chanmask('I', NULL, s);
              } else if (lasthand[1] == SET_NAME[1]) {
                var_userfile_share_line(s, -1, 0);
              }
            }
	  }
	} else if (!strcmp(code, "!")) {	/* user channel record */
	  /* ! #chan laston flags [info] */
	  char *chname = NULL, *st = NULL, *fl = NULL;

	  if (u) {
	    chname = newsplit(&s);
	    st = newsplit(&s);
	    fl = newsplit(&s);
	    rmspace(s);
	    fr.match = FR_CHAN;
            if (u->bot)
              fr.match |= FR_BOT;
	    break_down_flags(fl, &fr, 0);
	    if (findchan_by_dname(chname)) {
	      for (cr = u->chanrec; cr; cr = cr->next)
		if (!rfc_casecmp(cr->channel, chname))
		  break;
	      if (!cr) {
		cr = (struct chanuserrec *) calloc(1, sizeof(struct chanuserrec));

		cr->next = u->chanrec;
		u->chanrec = cr;
		strlcpy(cr->channel, chname, 80);
		cr->laston = atoi(st);
		cr->flags = fr.chan;
		if (s[0]) {
                  cr->info = strdup(s);
		} else
		  cr->info = NULL;
	      }
	    }
	  }
        } else if (!strcmp(code, "+")) {	/* add channel record */
         if (s[0] && lasthand[0] == '*' && lasthand[1] == CHANS_NAME[1]) {
           char *options = NULL, *chan = NULL, *my_ptr = NULL;
           char resultbuf[RESULT_LEN] = "";

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
             noshare = 0;
             return 0;
           }
           free(my_ptr);
         }
	} else if (!strncmp(code, "::", 2)) {	/* channel-specific bans */
	  strlcpy(lasthand, &code[2], sizeof(lasthand));
	  u = NULL;
	  if (!findchan_by_dname(lasthand)) {
	    strlcpy(s1, lasthand, sizeof(s1));
	    strlcat(s1, " ", sizeof(s1));
	    if (strstr(ignored, s1) == NULL) {
	      strlcat(ignored, lasthand, sizeof(ignored));
	      strlcat(ignored, " ", sizeof(ignored));
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
	} else if (!strncmp(code, "&&", 2)) {	/* channel-specific exempts */
	  strlcpy(lasthand, &code[2], sizeof(lasthand));
	  u = NULL;
	  if (!findchan_by_dname(lasthand)) {
	    strlcpy(s1, lasthand, sizeof(s1));
	    strlcat(s1, " ", sizeof(s1));
	    if (strstr(ignored, s1) == NULL) {
	      strlcat(ignored, lasthand, sizeof(ignored));
	      strlcat(ignored, " ", sizeof(ignored));
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
	} else if (!strncmp(code, "$$", 2)) {	/* channel-specific invites */
	  strlcpy(lasthand, &code[2], sizeof(lasthand));
	  u = NULL;
	  if (!findchan_by_dname(lasthand)) {
	    strlcpy(s1, lasthand, sizeof(s1));
	    strlcat(s1, " ", sizeof(s1));
	    if (strstr(ignored, s1) == NULL) {
	      strlcat(ignored, lasthand, sizeof(ignored));
	      strlcat(ignored, " ", sizeof(ignored));
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
	} else if (!strncmp(code, "--", 2)) {	/* user USERENTRY */
	  if (u) {
	    /* new format storage */
	    struct user_entry *ue = NULL;
	    int ok = 0;

	    for (ue = u->entries; ue && !ok; ue = ue->next)
	      if (ue->name && !strcasecmp(code + 2, ue->name)) {
		struct list_type *list = NULL;

		list = (struct list_type *) calloc(1, sizeof(struct list_type));

		list->next = NULL;
                list->extra = strdup(s);
		list_append((&ue->u.list), list);
		ok = 1;
	      }
            /* if we don't have the entry, make it */
	    if (!ok) {
	      ue = (struct user_entry *) calloc(1, sizeof(struct user_entry));

//	      ue->name = (char *) calloc(1, strlen(code + 1));
              ue->name = strdup(code + 2);
	      ue->type = NULL;
//	      strcpy(ue->name, code + 2);
	      ue->u.list = (struct list_type *) calloc(1, sizeof(struct list_type));

	      ue->u.list->next = NULL;
              ue->u.list->extra = strdup(s);
	      list_insert((&u->entries), ue);
	    }
	  }
	} else if (!rfc_casecmp(code, BAN_NAME)) {
	  strlcpy(lasthand, code, sizeof(lasthand));
	  u = NULL;
	} else if (!rfc_casecmp(code, IGNORE_NAME)) {
	  strlcpy(lasthand, code, sizeof(lasthand));
	  u = NULL;
	} else if (!rfc_casecmp(code, EXEMPT_NAME)) {
	  strlcpy(lasthand, code, sizeof(lasthand));
	  u = NULL;
	} else if (!rfc_casecmp(code, INVITE_NAME)) {
	  strlcpy(lasthand, code, sizeof(lasthand));
	  u = NULL;
        } else if (!rfc_casecmp(code, CHANS_NAME)) {
          strlcpy(lasthand, code, sizeof(lasthand));
          u = NULL;
        } else if (!rfc_casecmp(code, SET_NAME)) {
          strlcpy(lasthand, code, sizeof(lasthand));
          u = NULL;  
	} else if (code[0] == '*') {
	  lasthand[0] = 0;
	  u = NULL;
	} else {		/* its a user ! */
	  pass = newsplit(&s);	/* old style passwords */
	  attr = newsplit(&s);
	  rmspace(s);
	  if (unlikely(!attr[0] || !pass[0])) {
	    putlog(LOG_MISC, "*", "* Corrupt user record line: %d!", line);
	    lasthand[0] = 0;
            noshare = 0;
            return 0;
	  } else {
            int isbot = 0;

            if (code[0] == '-') {
              code++;
              isbot++;
            }

	    u = get_user_by_handle(bu, code);
	    if (unlikely(u)) {
	      putlog(LOG_ERROR, "@", "* Duplicate user record '%s'!", code);
	      lasthand[0] = 0;
	      u = NULL;
	    } else {
	      fr.match = FR_GLOBAL;
              if (isbot)
                fr.match |= FR_BOT;
	      break_down_flags(attr, &fr, 0);
	      strlcpy(lasthand, code, sizeof(lasthand));
	      cst = NULL;
              
	      if (strlen(code) > HANDLEN)
		code[HANDLEN] = 0;
	      if (unlikely(strlen(pass) > 20)) {	/* old style passwords */
		putlog(LOG_MISC, "*", "* Corrupted password reset for '%s'", code);
                pass[0] = '-';
                pass[1] = 0;
	      }
	      bu = adduser(bu, code, 0, pass, sanity_check(fr.global, isbot), isbot);

	      u = get_user_by_handle(bu, code);
	      for (i = 0; i < dcc_total; i++)
		if (dcc[i].type && !strcasecmp(code, dcc[i].nick))
		  dcc[i].user = u;
        
              if (!strcasecmp(code, conf.bot->nick))
                conf.bot->u = u;

	      /* if s starts with '/' it's got file info */
	    }
	  }
	}
      }
    }
  }

  (*ret) = bu;
  if (ignored[0]) {
    putlog(LOG_MISC, "*", "Ignored masks for channel(s): %s", ignored);
  }
  putlog(LOG_MISC, "*", "Userfile loaded, unpacking...");
  for (u = bu; u; u = u->next) {
    struct user_entry *e = NULL;

    if (unlikely(!u->bot && !strcasecmp (u->handle, conf.bot->nick))) {
      putlog(LOG_MISC, "*", "(!) I have a user record, but am not classified as a BOT!");
      u->bot = 1;
    }

    for (e = u->entries; e; e = e->next)
      if (likely(e->name)) {
	struct user_entry_type *uet = find_entry_type(e->name);

	if (uet) {
	  e->type = uet;
	  uet->unpack(u, e);
	  free(e->name);
	  e->name = NULL;
	} else
          sdprintf("FAILED TO UNPACK '%s'", e->name);
      }
  }

  /* process the user data *now* */
  if (!conf.bot->hub)
    unlink(userfile);
  noshare = 0;
  return 1;
}

void link_pref_val(struct userrec *u, char *val)
{
/* val must be HANDLEN + 4 chars minimum */
  val[0] = 'Z';
  val[1] = 0;
  
  if (!u)
    return;

  if (!u->bot)
    return;

  struct bot_addr *ba = NULL;

  if (!(ba = (struct bot_addr *) get_user(&USERENTRY_BOTADDR, u))) {
    return;
  }
  if (!ba->hublevel || ba->hublevel == 999) {
    return;
  }
  simple_snprintf(val, HANDLEN + 4, "%02d%s", ba->hublevel, u->handle);
}

/*
  starting at "current" or "userlist" if NULL, find next bot with a
  link_pref_val higher than "lowval" and lower than "highval"
  If none found return bot with best overall link_pref_val
  If still not found return NULL
*/
struct userrec *next_hub(struct userrec *current, char *lowval, char *highval)
{
  char thisval[HANDLEN + 4] = "", bestmatchval[HANDLEN + 4] = "z", bestallval[HANDLEN + 4] = "z";
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
    if (cur->bot && (strcasecmp(cur->handle, conf.bot->nick))) {
      link_pref_val(cur, thisval);
      if ((strcmp(thisval, lowval) < 0) && (strcmp(thisval, highval) > 0) &&(strcmp(thisval, bestmatchval) < 0)) {
        strlcpy(bestmatchval, thisval, sizeof(bestmatchval));
        bestmatch = cur;
      }
      if ((strcmp(thisval, lowval) < 0) && (strcmp(thisval, bestallval) < 0)) {
        strlcpy(bestallval, thisval, sizeof(bestallval));
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

static void autolink_cycle_hub(char *start)
{
  char bestval[HANDLEN + 4] = "", curval[HANDLEN + 4] = "", myval[HANDLEN + 4] = "";
  tand_t *bot = NULL;

  link_pref_val(conf.bot->u, myval);
  strlcpy(bestval, myval, sizeof(bestval));
  for (int i = 0; i < dcc_total; i++) {
   if (dcc[i].type) {
    if (dcc[i].type == &DCC_BOT_NEW)
      return;
    if (dcc[i].type == &DCC_FORK_BOT)
      return;
    if (dcc[i].type == &DCC_BOT) {
      if (dcc[i].status & (STAT_OFFEREDU | STAT_GETTINGU | STAT_SENDINGU))
        continue; /* lets let the binary update have its peace. */

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
	    strlcpy(bestval, curval, sizeof(bestval));
	}
      }
    }
   }
  }

  struct userrec *u = NULL;

  if (start)
    u = get_user_by_handle(userlist, start);

  if (u) {
    link_pref_val(u, curval);
    if (strcmp(bestval, curval) < 0) {
	/* This happens if we're already connected to a good hub but we failed to link to another hub as well
	   can happen if you .link.... but it's nothing FATAL :) */
      /* This shouldn't happen. Getting a failed link attempt (start!=NULL)
         while a dcc scan indicates we *are* connected to a better bot than
         the one we failed a link to.
       */
//      putlog(LOG_BOTS, "*",  "Failed link attempt to %s but connected to %s already???", u->handle, (char *) &bestval[2]);
      return;
    }
  } else
    strlcpy(curval, "0", sizeof(curval));

  /* link to the (highlest level)/best hub */
  u = next_hub(u, bestval, curval);
  if (u && !in_chain(u->handle))
    botlink("", -3, u->handle);
}

typedef struct hublist_entry {
  struct hublist_entry *next;
  struct userrec *u;
} tag_hublist_entry;

int botlinkcount = 0;

static void autolink_random_hub(char *avoidbot) {
  /* Pick a random hub, but avoid 'avoidbot' */
  int hlc = 0;
  struct hublist_entry *hl = NULL, *hl2 = NULL;
  struct userrec *tmpu = NULL;
  struct flag_record fr = { FR_GLOBAL|FR_BOT, 0, 0, 0 };

  for (struct userrec *u = userlist; u; u = u->next) {
    get_user_flagrec(u, &fr, NULL);
    if (glob_bot(fr) && strcasecmp(u->handle, conf.bot->nick) && (bot_hublevel(u) < 999)) {
      if (strcmp(u->handle, avoidbot)) {
        hl2 = hl;
        hl = (struct hublist_entry *) calloc(1, sizeof(struct hublist_entry));

        hl->next = hl2;
        hlc++;
        hl->u = u;
      } else
        tmpu = u;
    }
  }

  /* We probably have 1 hub and avoided it :/ */
  if (!hlc && tmpu) {
    hl2 = hl;
    hl = (struct hublist_entry *) calloc(1, sizeof(struct hublist_entry));
    hl->next = hl2;
    hlc++;
    hl->u = tmpu;
  }
  hlc = randint(hlc);
  while (hl) {
    if (!hlc) {
      botlink("", -3, hl->u->handle);
    }
    hlc--;
    hl2 = hl->next;
    free(hl);
    hl = hl2;
  }
}

static void autolink_cycle_leaf(char *start)
{
  struct bot_addr *my_ba = (struct bot_addr *) get_user(&USERENTRY_BOTADDR, conf.bot->u);
  char uplink[HANDLEN + 1] = "", avoidbot[HANDLEN + 1] = "", curhub[HANDLEN + 1] = "";

  /* Reset connection attempts if we ain't called due to a failed link */
  if (!start)
    botlinkcount = 0;

  if (my_ba && (my_ba->uplink[0])) {
    strlcpy(uplink, my_ba->uplink, sizeof(uplink));
  } 

  for (int i = 0; i < dcc_total; i++) {
   if (dcc[i].type) {
    if ((dcc[i].type == &DCC_BOT_NEW) || (dcc[i].type == &DCC_FORK_BOT))
      return;
    if (dcc[i].hub && dcc[i].type == &DCC_BOT) {
      strlcpy(curhub, dcc[i].nick, sizeof(curhub));
      break;
    }
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

    if (dont_restructure)
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
      strlcpy(avoidbot, start, sizeof(avoidbot));
    }
  }

  autolink_random_hub(avoidbot);
}


void autolink_cycle()
{
  char start[HANDLEN + 1] = "";

  if (autolink_failed[0]) {
    strlcpy(start, autolink_failed, HANDLEN + 1);
    autolink_failed[0] = 0;
  }

  if (conf.bot->hub)
    autolink_cycle_hub(start[0] ? start : NULL);
  else if (conf.bot->localhub)
    autolink_cycle_leaf(start[0] ? start : NULL);
  else { //Connect to the localhub
    if (tands == 0) {
      // Make sure not already trying for the localhub
      for (int i = 0; i < dcc_total; i++) {
       if (dcc[i].type) {
        if ((dcc[i].type == &DCC_BOT_NEW) || (dcc[i].type == &DCC_FORK_BOT))
          return;
       }
      }
      sdprintf("need to link to my localhub: %s\n", conf.localhub);
      botlink("", -3, conf.localhub);
    }
  }
}


void check_stale_dcc_users() 
{
  for (int i = 0; i < dcc_total; ++i) {
    if (!dcc[i].type || !dcc[i].nick[0]) continue;
    

    if (dcc[i].user == NULL && !(dcc[i].user = get_user_by_handle(userlist, dcc[i].nick))) { /* Removed user */
      if (dcc[i].type == &DCC_BOT || dcc[i].type == &DCC_FORK_BOT || dcc[i].type == &DCC_BOT_NEW)
        botunlink(i, dcc[i].nick, "No longer a valid bot.");
      else if (dcc[i].type == &DCC_CHAT) {
        if (!backgrd && term_z && !strcmp(dcc[i].nick, "HQ"))
          setup_HQ(i);
        else
          do_boot(i, "internal", "No longer a valid user.");
      }
    }
  }
}
/* vim: set sts=2 sw=2 ts=8 et: */
