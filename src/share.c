
/* 
 * share.c -- part of share.mod
 * 
 * $Id: share.c,v 1.27 2000/01/08 21:23:17 per Exp $
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

#define MODULE_NAME "share"
#define MAKING_SHARE

#include "main.h"
#include "hook.h"
#include "chan.h"
#include "users.h"
#include "transfer.h"
#include "channels.h"
#include "tandem.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

const int min_share = 1029900;	/* minimum version I will share with */
const int min_exemptinvite = 1032800;	/* eggdrop 1.3.28 supports exempts

					   * and invites */
int private_owner = 0,
  private_global = 0,
  private_user = 0;
char private_globals[50];
int allow_resync = 0;
struct flag_record fr = { 0, 0, 0, 0, 0 };
int resync_time = 900;

#ifdef HUB
#ifdef OLDCODE
void start_sending_users(int);
#endif
extern char ver[],
  origbotname[],
  natip[];
#endif
void shareout_but EGG_VARARGS(struct chanset_t *, arg1);
int flush_tbuf(char *bot);
int can_resync(char *bot);
void q_resync(char *s, struct chanset_t *chan);
void cancel_user_xfer(int, void *);
int private_globals_bitmask();
int got_first_userfile = 0;

extern int noshare,
  dcc_total,
  max_dcc,
  share_greet;
extern struct dcc_t *dcc;
extern time_t now;
extern char netpass[],
  botnetnick[];
extern struct chanset_t *chanset;
extern struct userrec *userlist,
 *lastuser;
extern struct dcc_table DCC_FORK_SEND,
  DCC_GET;

/* store info for sharebots */
struct share_msgq {
  struct chanset_t *chan;
  char *msg;
  struct share_msgq *next;
};

struct tandbuf {
  char bot[HANDLEN + 1];
  time_t timer;
  struct share_msgq *q;
} tbuf[5];

/* botnet commands */

void hook_read_userfile();

void share_chhand(int idx, char *par)
{
  char *hand;
  struct userrec *u;

  if ((dcc[idx].status & STAT_SHARE) && !private_user) {
    hand = newsplit(&par);
    u = get_user_by_handle(userlist, hand);
    if (u && !(u->flags & USER_UNSHARED)) {
      shareout_but(NULL, idx, STR("h %s %s\n"), hand, par);
      noshare = 1;
      change_handle(u, par);
      noshare = 0;
    }
  }
}

void share_chattr(int idx, char *par)
{
  char *hand,
   *atr,
    s[100];
  struct chanset_t *cst;
  struct userrec *u;
  struct flag_record fr2;
  int bfl,
    ofl;

  if ((dcc[idx].status & STAT_SHARE) && !private_user) {
    hand = newsplit(&par);
    u = get_user_by_handle(userlist, hand);
    if (u && !(u->flags & USER_UNSHARED)) {
      atr = newsplit(&par);
      cst = findchan(par);
      if (!par[0] || (cst)) {
	if (!(dcc[idx].status & STAT_GETTING) && (cst || !private_global))
	  shareout_but(cst, idx, STR("a %s %s %s\n"), hand, atr, par);
	noshare = 1;
	if (par[0] && cst) {
	  fr.match = (FR_CHAN);
	  get_user_flagrec(dcc[idx].user, &fr, par);
	  fr.match = FR_CHAN;
	  fr2.match = FR_CHAN;
	  break_down_flags(atr, &fr, 0);
	  get_user_flagrec(u, &fr2, par);
	  set_user_flagrec(u, &fr, par);
	  check_dcc_chanattrs(u, par, fr.chan, fr2.chan);
	  noshare = 0;
	  build_flags(s, &fr, 0);
#ifdef LEAF
	  recheck_channel(cst, 0);
#endif
	} else if (!private_global) {
	  int pgbm = private_globals_bitmask();

	  /* don't let bot flags be altered */
	  fr.match = FR_GLOBAL;
	  break_down_flags(atr, &fr, 0);
	  bfl = u->flags & USER_BOT;
	  ofl = fr.global;
	  fr.global = (fr.global &~pgbm)
	    |(u->flags & pgbm);
	  fr.global = sanity_check(fr.global |bfl);

	  set_user_flagrec(u, &fr, 0);
	  check_dcc_attrs(u, ofl);
	  noshare = 0;
	  build_flags(s, &fr, 0);
	  fr.match = FR_CHAN;
#ifdef LEAF
	  for (cst = chanset; cst; cst = cst->next)
	    recheck_channel(cst, 0);
#endif
	}
	noshare = 0;
      }
    }
  }
}

void share_pls_chrec(int idx, char *par)
{
  char *user;
  struct chanset_t *chan;
  struct userrec *u;

  if ((dcc[idx].status & STAT_SHARE) && !private_user) {
    user = newsplit(&par);
    if ((u = get_user_by_handle(userlist, user))) {
      chan = findchan(par);
      fr.match = (FR_CHAN);
      get_user_flagrec(dcc[idx].user, &fr, par);
      if (chan) {
	noshare = 1;
	shareout_but(chan, idx, STR("+cr %s %s\n"), user, par);
	if (!get_chanrec(u, par)) {
	  add_chanrec(u, par);
	}
	noshare = 0;
      }
    }
  }
}

void share_mns_chrec(int idx, char *par)
{
  char *user;
  struct chanset_t *chan;
  struct userrec *u;

  if ((dcc[idx].status & STAT_SHARE) && !private_user) {
    user = newsplit(&par);
    if ((u = get_user_by_handle(userlist, user))) {
      chan = findchan(par);
      fr.match = (FR_CHAN);
      get_user_flagrec(dcc[idx].user, &fr, par);
      if (chan) {
	noshare = 1;
	del_chanrec(u, par);
	shareout_but(chan, idx, STR("-cr %s %s\n"), user, par);
	noshare = 0;
      }
    }
  }
}

void share_newuser(int idx, char *par)
{
  char *etc,
   *etc2,
   *etc3,
    s[100];
  struct userrec *u;

  if ((dcc[idx].status & STAT_SHARE) && !private_user) {
    etc = newsplit(&par);
    if (!(u = get_user_by_handle(userlist, etc)) || !(u->flags & USER_UNSHARED)) {
      fr.global = 0;

      /* If user already exists, ignore command */
      shareout_but(NULL, idx, STR("n %s %s\n"), etc, private_global ? ((fr.global &USER_BOT) ? "b" : "-") : par);
      if (!u) {
	noshare = 1;
	if (strlen(etc) > HANDLEN)
	  etc[HANDLEN] = 0;
	etc2 = newsplit(&par);
	etc3 = newsplit(&par);
	fr.match = FR_GLOBAL;
	break_down_flags(par, &fr, NULL);

	if (private_global)
	  fr.global &=USER_BOT;

	else {
	  int pgbm = private_globals_bitmask();

	  fr.match = FR_GLOBAL;
	  fr.global &=~pgbm;
	}
	build_flags(s, &fr, 0);
	userlist = adduser(userlist, etc, etc2, etc3, 0);
	/* support for userdefiniedflag share - drummer */
	u = get_user_by_handle(userlist, etc);
	set_user_flagrec(u, &fr, 0);
	fr.match = FR_CHAN;	/* why?? */
	noshare = 0;
      }
    }
  }
}

void share_killuser(int idx, char *par)
{
  struct userrec *u;

  if ((dcc[idx].status & STAT_SHARE) && !private_user && (u = get_user_by_handle(userlist, par)) && !(u->flags & USER_UNSHARED)) {
    noshare = 1;
    if (deluser(par)) {
      shareout_but(NULL, idx, STR("k %s\n"), par);
    }
    noshare = 0;
  }
}

void share_pls_host(int idx, char *par)
{
  char *hand;
  struct userrec *u;

  if ((dcc[idx].status & STAT_SHARE) && !private_user) {
    hand = newsplit(&par);
    if ((u = get_user_by_handle(userlist, hand)) && !(u->flags & USER_UNSHARED)) {
      shareout_but(NULL, idx, STR("+h %s %s\n"), hand, par);
      set_user(&USERENTRY_HOSTS, u, par);
    }
  }
}

void share_pls_bothost(int idx, char *par)
{
  char *hand,
    p[32];
  struct userrec *u;

  Context;
  if ((dcc[idx].status & STAT_SHARE) && !private_user) {
    hand = newsplit(&par);
    if (!(u = get_user_by_handle(userlist, hand)) || (!(u->flags & USER_UNSHARED))) {
      if (!(dcc[idx].status & STAT_GETTING))
	shareout_but(NULL, idx, STR("+bh %s %s\n"), hand, par);
      /* add bot to userlist if not there */
      if (u) {
	if ((u->flags & USER_UNSHARED))
	  return;		/* ignore */
	set_user(&USERENTRY_HOSTS, u, par);
      } else {
	makepass(p);
	userlist = adduser(userlist, hand, par, p, USER_BOT);
      }
    }
  }
}

void share_mns_host(int idx, char *par)
{
  char *hand;
  struct userrec *u;

  Context;
  if ((dcc[idx].status & STAT_SHARE) && !private_user) {
    hand = newsplit(&par);
    if ((u = get_user_by_handle(userlist, hand)) && !(u->flags & USER_UNSHARED)) {
      shareout_but(NULL, idx, STR("-h %s %s\n"), hand, par);
      noshare = 1;
      delhost_by_handle(hand, par);
      noshare = 0;
    }
  }
}

void share_change(int idx, char *par)
{
  char *key,
   *hand;
  struct userrec *u;
  struct user_entry_type *uet;
  struct user_entry *e;

  if ((dcc[idx].status & STAT_SHARE) && !private_user) {
    key = newsplit(&par);
    hand = newsplit(&par);
    if (!(u = get_user_by_handle(userlist, hand)) || !(u->flags & USER_UNSHARED)) {
      if ((uet = find_entry_type(key))) {
	if (!(dcc[idx].status & STAT_GETTING))
	  shareout_but(NULL, idx, STR("c %s %s %s\n"), key, hand, par);
	noshare = 1;
	if (!u && (uet == &USERENTRY_BOTADDR)) {
	  char pass[30];

	  makepass(pass);
	  userlist = adduser(userlist, hand, STR("none"), pass, USER_BOT);
	  u = get_user_by_handle(userlist, hand);
	} else if (!u)
	  return;
	if (uet->got_share) {
	  if (!(e = find_user_entry(uet, u))) {
	    e = user_malloc(sizeof(struct user_entry));

	    e->type = uet;
	    e->name = NULL;
	    e->u.list = NULL;
	    list_insert((&(u->entries)), e);
	  }
	  uet->got_share(u, e, par, idx);
	  if (!e->u.list) {
	    list_delete((struct list_type **) &(u->entries), (struct list_type *) e);
	    nfree(e);
	  }
	}
	noshare = 0;
      }
    }
  }
}

void share_chchinfo(int idx, char *par)
{
  char *hand,
   *chan;
  struct chanset_t *cst;
  struct userrec *u;

  if ((dcc[idx].status & STAT_SHARE) && !private_user) {
    hand = newsplit(&par);
    if ((u = get_user_by_handle(userlist, hand)) && !(u->flags & USER_UNSHARED) && share_greet) {
      chan = newsplit(&par);
      cst = findchan(chan);
      fr.match = (FR_CHAN);
      get_user_flagrec(dcc[idx].user, &fr, chan);
      if (cst) {
	shareout_but(cst, idx, STR("chchinfo %s %s %s\n"), hand, chan, par);
	noshare = 1;
	set_handle_chaninfo(userlist, hand, chan, par);
	noshare = 0;
      }
    }
  }
}

void share_mns_ban(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    shareout_but(NULL, idx, STR("-b %s\n"), par);
    noshare = 1;
    u_delban(NULL, par, 1);
    noshare = 0;
  }
}

void share_mns_exempt(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    shareout_but(NULL, idx, STR("-e %s\n"), par);
    noshare = 1;
    u_delexempt(NULL, par, 1);
    noshare = 0;
  }
}

void share_mns_invite(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    shareout_but(NULL, idx, STR("-inv %s\n"), par);
    noshare = 1;
    u_delinvite(NULL, par, 1);
    noshare = 0;
  }
}

void share_mns_banchan(int idx, char *par)
{
  char *chname;
  struct chanset_t *chan;

  if (dcc[idx].status & STAT_SHARE) {
    chname = newsplit(&par);
    chan = findchan(chname);
    fr.match = (FR_CHAN);
    get_user_flagrec(dcc[idx].user, &fr, chname);
    if (chan) {
      shareout_but(chan, idx, STR("-bc %s %s\n"), chname, par);
      noshare = 1;
      u_delban(chan, par, 1);
      noshare = 0;
    }
  }
}

void share_mns_exemptchan(int idx, char *par)
{
  char *chname;
  struct chanset_t *chan;

  if (dcc[idx].status & STAT_SHARE) {
    chname = newsplit(&par);
    chan = findchan(chname);
    fr.match = (FR_CHAN);
    get_user_flagrec(dcc[idx].user, &fr, chname);
    if (chan) {
      shareout_but(chan, idx, STR("-ec %s %s\n"), chname, par);
      noshare = 1;
      u_delexempt(chan, par, 1);
      noshare = 0;
    }
  }
}

void share_mns_invitechan(int idx, char *par)
{
  char *chname;
  struct chanset_t *chan;

  if (dcc[idx].status & STAT_SHARE) {
    chname = newsplit(&par);
    chan = findchan(chname);
    fr.match = (FR_CHAN);
    get_user_flagrec(dcc[idx].user, &fr, chname);
    if (chan) {
      shareout_but(chan, idx, STR("-invc %s %s\n"), chname, par);
      noshare = 1;
      u_delinvite(chan, par, 1);
      noshare = 0;
    }
  }
}

void share_mns_ignore(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    shareout_but(NULL, idx, STR("-i %s\n"), par);
    noshare = 1;
    delignore(par);
    noshare = 0;
  }
}

void share_pls_ban(int idx, char *par)
{
  time_t expire_time;
  char *ban,
   *tm,
   *from;
  int flags = 0;
#ifdef LEAF
  struct chanset_t * chan;
#endif

  if (dcc[idx].status & STAT_SHARE) {
    shareout_but(NULL, idx, STR("+b %s\n"), par);
    noshare = 1;
    ban = newsplit(&par);
    tm = newsplit(&par);
    from = newsplit(&par);
    if (strchr(from, 'p'))
      flags |= MASKREC_PERM;
    from = newsplit(&par);
    expire_time = (time_t) atoi(tm);
    if (expire_time != 0L)
      expire_time += now;
    u_addban(NULL, ban, from, par, expire_time, flags);
    noshare = 0;
#ifdef LEAF
    for (chan=chanset;chan;chan=chan->next) 
      recheck_channel(chan, 1);
#endif
  }
}

void share_pls_banchan(int idx, char *par)
{
  time_t expire_time;
  int flags = 0;
  struct chanset_t *chan;
  char *ban,
   *tm,
   *chname,
   *from;

  if (dcc[idx].status & STAT_SHARE) {
    ban = newsplit(&par);
    tm = newsplit(&par);
    chname = newsplit(&par);
    chan = findchan(chname);
    fr.match = (FR_CHAN);
    get_user_flagrec(dcc[idx].user, &fr, chname);
    if (chan) {
      shareout_but(chan, idx, STR("+bc %s %s %s %s\n"), ban, tm, chname, par);
      from = newsplit(&par);
      if (strchr(from, 'p'))
	flags |= MASKREC_PERM;
      from = newsplit(&par);
      noshare = 1;
      expire_time = (time_t) atoi(tm);
      if (expire_time != 0L)
	expire_time += now;
      u_addban(chan, ban, from, par, expire_time, flags);
      noshare = 0;
#ifdef LEAF
      recheck_channel(chan, 1);
#endif
    }
  }
}

/* repeat for exempts */
void share_pls_exempt(int idx, char *par)
{
  time_t expire_time;
  char *exempt,
   *tm,
   *from;
  int flags = 0;

  if (dcc[idx].status & STAT_SHARE) {
    shareout_but(NULL, idx, STR("+e %s\n"), par);
    noshare = 1;
    exempt = newsplit(&par);
    tm = newsplit(&par);
    from = newsplit(&par);
    if (strchr(from, 'p'))
      flags |= MASKREC_PERM;
    from = newsplit(&par);
    expire_time = (time_t) atoi(tm);
    if (expire_time != 0L)
      expire_time += now;
    u_addexempt(NULL, exempt, from, par, expire_time, flags);
    noshare = 0;
  }
}

void share_pls_exemptchan(int idx, char *par)
{
  time_t expire_time;
  int flags = 0;
  struct chanset_t *chan;
  char *exempt,
   *tm,
   *chname,
   *from;

  if (dcc[idx].status & STAT_SHARE) {
    exempt = newsplit(&par);
    tm = newsplit(&par);
    chname = newsplit(&par);
    chan = findchan(chname);
    fr.match = (FR_CHAN);
    get_user_flagrec(dcc[idx].user, &fr, chname);
    if (chan) {
      shareout_but(chan, idx, STR("+ec %s %s %s %s\n"), exempt, tm, chname, par);
      from = newsplit(&par);
      if (strchr(from, 'p'))
	flags |= MASKREC_PERM;
      from = newsplit(&par);
      noshare = 1;
      expire_time = (time_t) atoi(tm);
      if (expire_time != 0L)
	expire_time += now;
      u_addexempt(chan, exempt, from, par, expire_time, flags);
      noshare = 0;
    }
  }
}

/* and for invites */
void share_pls_invite(int idx, char *par)
{
  time_t expire_time;
  char *invite,
   *tm,
   *from;
  int flags = 0;

  if (dcc[idx].status & STAT_SHARE) {
    shareout_but(NULL, idx, STR("+inv %s\n"), par);
    noshare = 1;
    invite = newsplit(&par);
    tm = newsplit(&par);
    from = newsplit(&par);
    if (strchr(from, 'p'))
      flags |= MASKREC_PERM;
    from = newsplit(&par);
    expire_time = (time_t) atoi(tm);
    if (expire_time != 0L)
      expire_time += now;
    u_addinvite(NULL, invite, from, par, expire_time, flags);
    noshare = 0;
  }
}

void share_pls_invitechan(int idx, char *par)
{
  time_t expire_time;
  int flags = 0;
  struct chanset_t *chan;
  char *invite,
   *tm,
   *chname,
   *from;

  if (dcc[idx].status & STAT_SHARE) {
    invite = newsplit(&par);
    tm = newsplit(&par);
    chname = newsplit(&par);
    chan = findchan(chname);
    fr.match = (FR_CHAN);
    get_user_flagrec(dcc[idx].user, &fr, chname);
    if (chan) {
      shareout_but(chan, idx, STR("+invc %s %s %s %s\n"), invite, tm, chname, par);
      from = newsplit(&par);
      if (strchr(from, 'p'))
	flags |= MASKREC_PERM;
      from = newsplit(&par);
      noshare = 1;
      expire_time = (time_t) atoi(tm);
      if (expire_time != 0L)
	expire_time += now;
      u_addinvite(chan, invite, from, par, expire_time, flags);
      noshare = 0;
    }
  }
}

/* +ignore <host> +<seconds-left> <from> <note> */
void share_pls_ignore(int idx, char *par)
{
  time_t expire_time;
  char *ign,
   *from,
   *ts;

  if (dcc[idx].status & STAT_SHARE) {
    shareout_but(NULL, idx, STR("+i %s\n"), par);
    noshare = 1;
    ign = newsplit(&par);
    ts = newsplit(&par);
    if (!atoi(ts))
      expire_time = 0L;
    else
      expire_time = now + atoi(ts);
    from = newsplit(&par);
    if (strchr(from, 'p'))
      expire_time = 0;
    from = newsplit(&par);
    if (strlen(from) > HANDLEN + 1)
      from[HANDLEN + 1] = 0;
    par[65] = 0;
    addignore(ign, from, par, expire_time);
    noshare = 0;
  }
}


stream userfile_stream = NULL;

void share_userfilestart(int idx, char * par) {
  if (userfile_stream) 
    stream_kill(userfile_stream);
  userfile_stream = stream_create();
}

void share_userfileend(int idx, char * par) {
  int i;
  struct userrec * u = NULL, *ou;
  struct chanset_t * chan;
  if (!userfile_stream) {
    log(LCAT_ERROR, STR("Got userfile end but no start??"));
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }
  dcc[idx].status &= ~STAT_GETTING;
  dcc[idx].status |= STAT_AGGRESSIVE | STAT_SHARE;
  got_first_userfile=1;
  noshare = 1;
  while (global_bans)
    u_delban(NULL, global_bans->mask, 1);
  while (global_ign)
    delignore(global_ign->igmask);
  while (global_invites)
    u_delinvite(NULL, global_invites->mask, 1);
  while (global_exempts)
    u_delexempt(NULL, global_exempts->mask, 1);
  for (chan = chanset; chan; chan = chan->next) {
    while (chan->bans)
      u_delban(chan, chan->bans->mask, 1);
    while (chan->exempts)
      u_delexempt(chan, chan->exempts->mask, 1);
    while (chan->invites)
      u_delinvite(chan, chan->invites->mask, 1);
  }
  ou = userlist;
  userlist = NULL;
  stream_seek(userfile_stream, SEEK_SET, 0);
  if (stream_readuserfile(userfile_stream, netpass, &u)) {
    for (i = 0; i < dcc_total; i++)
      dcc[i].user = get_user_by_handle(u, dcc[i].nick);
    Context;
    log(LCAT_BOT, STR("Userfile transferred & loaded."));
    clear_chanlist();
    userlist = u;
    trigger_cfg_changed();
    lastuser = NULL;
    clear_userlist(ou);
    reaffirm_owners();	/* make sure my owners are +n */
    updatebot(-1, dcc[idx].nick, '+', 0);
    Context;
  } else {
    userlist=ou;
    log(LCAT_ERROR, STR("Couldn't read new userfile"));
    killsock(dcc[idx].sock);
    lostdcc(idx);
  }
  stream_kill(userfile_stream);
  userfile_stream=NULL;
  noshare = 0;
  hook_read_userfile();
}

void share_userline(int idx, char *par) {
  if (userfile_stream) {
    stream_printf(userfile_stream, "%s\n", par);
  } else {
    log(LCAT_ERROR, STR("Got userfile line outside transfer start/end codes"));
    killsock(dcc[idx].sock);
    lostdcc(idx);
  }
}

#ifdef HUB

void ulsend (int idx, char * code, char * data) {
  char ln[1024];
  sprintf(ln, "s %s %s\n", code, data);
  tputs(dcc[idx].sock, ln, strlen(ln));
}

void send_users(int idx) {
  stream s;
  char buf[2048];
  s=stream_create();
  stream_writeuserfile(s, userlist, netpass, -1);
  stream_seek(s, SEEK_SET, 0);
  ulsend(idx, "ls", "");
  while (stream_getpos(s)<stream_size(s)) {
    stream_gets(s, buf, sizeof(buf));
    buf[sizeof(buf)-1]=0;
    if (strchr(buf, '\n'))
      * (char *) (strchr(buf, '\n')) = 0;
    ulsend(idx, "l", buf);
  }
  ulsend(idx, "le", "");
  stream_kill(s);
}

void share_ufno(int idx, char *par)
{
  log(LCAT_BOT, STR("User file rejected by %s: %s"), dcc[idx].nick, par);
  log(LCAT_WARNING, STR("User file rejected by %s: %s"), dcc[idx].nick, par);
  botunlink(-2, dcc[idx].nick, STR("No sharing - no linking."));
}

void share_ufyes(int idx, char *par)
{
  Context;
  if (dcc[idx].status & STAT_OFFERED) {
    dcc[idx].status &= ~STAT_OFFERED;
    dcc[idx].status |= STAT_SHARE;
    dcc[idx].status |= STAT_SENDING;
    lower_bot_linked(idx);
    send_users(idx);
    dcc[idx].status &= ~STAT_SENDING;
  } else {
    log(LCAT_ERROR, STR("%s accepted user transfer, but i never offered?"), dcc[idx].nick);
    botunlink(-2, dcc[idx].nick, STR("Sharing fucked up - try again"));
  }
}
#endif

void share_userfileq(int idx, char *par)
{
  int i;

  flush_tbuf(dcc[idx].nick);
  if (bot_aggressive_to(dcc[idx].user)) {
    log(LCAT_ERROR, STR("%s offered user transfer - I'm supposed to be aggressive to it"), dcc[idx].nick);
    dprintf(idx, STR("s un I have you marked for Agressive sharing.\n"));
    botunlink(-2, dcc[idx].nick, STR("I'm aggressive to you"));
  } else {
    for (i = 0; i < dcc_total; i++)
      if (dcc[i].type->flags & DCT_BOT) {
	if ((dcc[i].status & STAT_SHARE) && (dcc[i].status & STAT_AGGRESSIVE) && (i != idx)) {
	  log(LCAT_ERROR, STR("%s offered user transfer - I'm already passively sharing with %s"),
	      dcc[idx].nick, dcc[i].nick);
	  dprintf(idx, STR("s un Already sharing.\n"));
	  botunlink(-2, dcc[idx].nick, STR("I'm already passively sharing"));
	  return;
	}
      }
    dprintf(idx, STR("s uy\n"));
    dcc[idx].status |= STAT_SHARE | STAT_GETTING | STAT_AGGRESSIVE;
    log(LCAT_BOT, STR("Downloading users from %s"), dcc[idx].nick);
  }
}

#ifdef OLDCODE
/* ufsend <ip> <port> <length> */
void share_ufsend(int idx, char *par)
{
  char *ip,
   *port;
  char s[1024];
  int i,
    sock;
  FILE *f;

  Context;
  sprintf(s, STR(".share.%s.users"), botnetnick);
  if (!(b_status(idx) & STAT_SHARE)) {
    dprintf(idx, STR("s e You didn't ask; you just started sending.\n"));
    dprintf(idx, STR("s e Ask before sending the userfile.\n"));
    zapfbot(idx);
  } else if (dcc_total == max_dcc) {
    log(LCAT_ERROR, STR("NO MORE DCC CONNECTIONS -- can't grab userfile"));
    dprintf(idx, STR("s e I can't open a DCC to you; I'm full.\n"));
    zapfbot(idx);
  } else if (!(f = fopen(s, "w"))) {
    log(LCAT_ERROR, STR("CAN'T WRITE USERFILE DOWNLOAD FILE!"));
    zapfbot(idx);
  } else {
    sock = getsock(SOCK_BINARY);
    ip = newsplit(&par);
    port = newsplit(&par);
    if (open_telnet_dcc(sock, ip, port) < 0) {
      killsock(sock);
      log(LCAT_BOT, STR("Asynchronous connection failed!"));
      dprintf(idx, STR("s e Can't connect to you!\n"));
      zapfbot(idx);
    } else {
      i = new_dcc(&DCC_FORK_SEND, sizeof(struct xfer_info));

      dcc[i].addr = my_atoul(ip);
      dcc[i].port = atoi(port);
      dcc[i].sock = (-1);
      dcc[i].type = &DCC_FORK_SEND;
      strcpy(dcc[i].nick, STR("*users"));
      strcpy(dcc[i].host, dcc[idx].nick);
      strcpy(dcc[i].u.xfer->filename, s);
      dcc[i].u.xfer->length = atoi(par);
      dcc[idx].status |= STAT_GETTING;
      dcc[i].u.xfer->f = f;
      /* don't buffer this */
      dcc[i].sock = sock;
    }
  }
}
#endif

void share_resyncq(int idx, char *par)
{
  if (!allow_resync)
    dprintf(idx, STR("s rn Not permitting resync.\n"));
  else {
    if (can_resync(dcc[idx].nick)) {
      dprintf(idx, STR("s r!\n"));
      dump_resync(idx);
      dcc[idx].status &= ~STAT_OFFERED;
      dcc[idx].status |= STAT_SHARE;
      log(LCAT_BOT, STR("Resync'd user file with %s"), dcc[idx].nick);
      updatebot(-1, dcc[idx].nick, '+', 0);
    } else if (!bot_aggressive_to(dcc[idx].user)) {
      dprintf(idx, STR("s r!\n"));
      dcc[idx].status &= ~STAT_OFFERED;
      dcc[idx].status |= STAT_SHARE;
      updatebot(-1, dcc[idx].nick, '+', 0);
      log(LCAT_BOT, STR("Resyncing user file from %s"), dcc[idx].nick);
    } else
      dprintf(idx, STR("s rn No resync buffer.\n"));
  }
}

void share_resync(int idx, char *par)
{
  if ((dcc[idx].status & STAT_OFFERED) && can_resync(dcc[idx].nick)) {
    dump_resync(idx);
    dcc[idx].status &= ~STAT_OFFERED;
    dcc[idx].status |= STAT_SHARE;
    updatebot(-1, dcc[idx].nick, '+', 0);
    log(LCAT_BOT, STR("Resync'd user file with %s"), dcc[idx].nick);
  }
}

void share_resync_no(int idx, char *par)
{
  log(LCAT_BOT, STR("Resync refused by %s: %s"), dcc[idx].nick, par);
  flush_tbuf(dcc[idx].nick);
  dprintf(idx, STR("s u?\n"));
}

void share_version(int idx, char *par)
{
  /* cleanup any share flags */
  dcc[idx].status &= ~(STAT_SHARE | STAT_GETTING | STAT_SENDING | STAT_OFFERED | STAT_AGGRESSIVE);
  if (bot_aggressive_to(dcc[idx].user)) {
    if (can_resync(dcc[idx].nick))
      dprintf(idx, STR("s r?\n"));
    else
      dprintf(idx, STR("s u?\n"));
    dcc[idx].status |= STAT_OFFERED;
  } else
    higher_bot_linked(idx);
}

void hook_read_userfile()
{
  int i;

  if (!noshare) {
    for (i = 0; i < dcc_total; i++)
      if ((dcc[i].type->flags & DCT_BOT) && (dcc[i].status & STAT_SHARE) && !(dcc[i].status & STAT_AGGRESSIVE)) {
	/* cancel any existing transfers */
	if (dcc[i].status & STAT_SENDING)
	  cancel_user_xfer(-i, 0);
	dprintf(i, STR("s u?\n"));
	dcc[i].status |= STAT_OFFERED;
      }
  }
}

void share_endstartup(int idx, char *par)
{
  dcc[idx].status &= ~STAT_GETTING;
  /* send to any other sharebots */
  hook_read_userfile();
}

void share_end(int idx, char *par)
{
  log(LCAT_BOT, STR("Ending sharing with %s, (%s)."), dcc[idx].nick, par);
  cancel_user_xfer(-idx, 0);
  dcc[idx].status &= ~(STAT_SHARE | STAT_GETTING | STAT_SENDING | STAT_OFFERED | STAT_AGGRESSIVE);
}

/* these MUST be sorted */
botcmd_t C_share[] = {
  {"!", (Function) share_endstartup},
  {"+b", (Function) share_pls_ban},
  {"+bc", (Function) share_pls_banchan},
  {"+bh", (Function) share_pls_bothost},
  {"+cr", (Function) share_pls_chrec},
  {"+e", (Function) share_pls_exempt},
  {"+ec", (Function) share_pls_exemptchan},
  {"+h", (Function) share_pls_host},
  {"+i", (Function) share_pls_ignore},
  {"+inv", (Function) share_pls_invite},
  {"+invc", (Function) share_pls_invitechan},
  {"-b", (Function) share_mns_ban},
  {"-bc", (Function) share_mns_banchan},
  {"-cr", (Function) share_mns_chrec},
  {"-e", (Function) share_mns_exempt},
  {"-ec", (Function) share_mns_exemptchan},
  {"-h", (Function) share_mns_host},
  {"-i", (Function) share_mns_ignore},
  {"-inv", (Function) share_mns_invite},
  {"-invc", (Function) share_mns_invitechan},
  {"a", (Function) share_chattr},
  {"c", (Function) share_change},
  {"chchinfo", (Function) share_chchinfo},
  {"e", (Function) share_end},
  {"h", (Function) share_chhand},
  {"k", (Function) share_killuser},
  {"l", (Function) share_userline},
  {"le", (Function) share_userfileend},
  {"ls", (Function) share_userfilestart},
  {"n", (Function) share_newuser},
  {"r!", (Function) share_resync},
  {"r?", (Function) share_resyncq},
  {"rn", (Function) share_resync_no},
  {"u?", (Function) share_userfileq},
#ifdef HUB
  {"un", (Function) share_ufno},
#endif
#ifdef HUB
  {"uy", (Function) share_ufyes},
#endif
  {"v", (Function) share_version},
  {0, 0}
};

void sharein_mod(int idx, char *msg)
{
  char *code;
  int f,
    i;

  Context;
  code = newsplit(&msg);
  for (f = 0, i = 0; C_share[i].name && !f; i++) {
    int y = strcasecmp(code, C_share[i].name);

    if (!y)
      /* found a match */
      (C_share[i].func) (idx, msg);
    if (y < 0)
      f = 1;
  }
}

void shareout_mod EGG_VARARGS_DEF(struct chanset_t *, arg1)
{
  int i,
    l;
  char *format;
  char s[601];
  struct chanset_t *chan;

  va_list va;
  chan = EGG_VARARGS_START(struct chanset_t *, arg1, va);

  format = va_arg(va, char *);
  
  strcpy(s, "s ");
#ifdef HAVE_VSNPRINTF
  if ((l = vsnprintf(s + 2, 509, format, va)) < 0)
    s[2 + (l = 509)] = 0;
#else
  l = vsprintf(s + 2, format, va);	/* Seggy: possible overflow */
#endif
  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type->flags & DCT_BOT) && (dcc[i].status & STAT_SHARE) && !(dcc[i].status & (STAT_GETTING | STAT_SENDING))) {
      if (chan) {
	fr.match = (FR_CHAN);
	get_user_flagrec(dcc[i].user, &fr, chan->name);
      }
      tputs(dcc[i].sock, s, l + 2);
    }
  q_resync(s, chan);
  va_end(va);
}

void shareout_but EGG_VARARGS_DEF(struct chanset_t *, arg1)
{
  int i,
    x,
    l;
  char *format;
  char s[601];
  struct chanset_t *chan;

  va_list va;
  chan = EGG_VARARGS_START(struct chanset_t *, arg1, va);
  x = va_arg(va, int);
  format = va_arg(va, char *);

  strcpy(s, "s ");
#ifdef HAVE_VSNPRINTF
  if ((l = vsnprintf(s + 2, 509, format, va)) < 0)
    s[2 + (l = 509)] = 0;
#else
  l = vsprintf(s + 2, format, va);
#endif
  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type->flags & DCT_BOT) && (i != x) && (dcc[i].status & STAT_SHARE) && (!(dcc[i].status & STAT_GETTING)) && (!(dcc[i].status & STAT_SENDING))) {
      if (chan) {
	fr.match = (FR_CHAN);
	get_user_flagrec(dcc[i].user, &fr, chan->name);
      }
      tputs(dcc[i].sock, s, l + 2);
    }
  q_resync(s, chan);
  va_end(va);
}

/***** RESYNC BUFFERS *****/

/* create a tandem buffer for 'bot' */
void new_tbuf(char *bot)
{
  int i;

  for (i = 0; i < 5; i++)
    if (!tbuf[i].bot[0]) {
      /* this one is empty */
      strcpy(tbuf[i].bot, bot);
      tbuf[i].q = NULL;
      tbuf[i].timer = now;
      log(LCAT_BOT, STR("Creating resync buffer for %s"), bot);
      break;
    }
}

/* flush a certain bot's tbuf */
int flush_tbuf(char *bot)
{
  int i;
  struct share_msgq *q;

  for (i = 0; i < 5; i++)
    if (!strcasecmp(tbuf[i].bot, bot)) {
      while (tbuf[i].q) {
	q = tbuf[i].q;
	tbuf[i].q = tbuf[i].q->next;
	nfree(q->msg);
	nfree(q);
      }
      tbuf[i].bot[0] = 0;
      return 1;
    }
  return 0;
}

/* flush all tbufs older than 15 minutes */
void check_expired_tbufs()
{
  int i;
  struct share_msgq *q;

  for (i = 0; i < 5; i++)
    if (tbuf[i].bot[0]) {
      if (now - tbuf[i].timer > resync_time) {
	/* EXPIRED */
	while (tbuf[i].q) {
	  q = tbuf[i].q;
	  tbuf[i].q = tbuf[i].q->next;
	  nfree(q->msg);
	  nfree(q);
	}
	log(LCAT_BOT, STR("Flushing resync buffer for clonebot %s."), tbuf[i].bot);
	tbuf[i].bot[0] = 0;
      }
    }
  /* resend userfile requests */
  for (i = 0; i < dcc_total; i++)
    if (dcc[i].type->flags & DCT_BOT) {
      if (dcc[i].status & STAT_OFFERED) {
	if (now - dcc[i].timeval > 120) {
	  if (dcc[i].user && (bot_aggressive_to(dcc[i].user)))
	    dprintf(i, STR("s u?\n"));
	  /* ^ send it again in case they missed it */
	}
	/* if it's a share bot that hasnt been sharing, ask again */
      } else if (!(dcc[i].status & STAT_SHARE)) {
	if (dcc[i].user && (bot_aggressive_to(dcc[i].user)))
	  dprintf(i, STR("s u?\n"));
	dcc[i].status |= STAT_OFFERED;
      }
    }
}

struct share_msgq *q_addmsg(struct share_msgq *qq, struct chanset_t *chan, char *s)
{
  struct share_msgq *q;
  int cnt;

  if (!qq) {
    q = (struct share_msgq *) nmalloc(sizeof(struct share_msgq));

    q->chan = chan;
    q->next = NULL;
    q->msg = (char *) nmalloc(strlen(s) + 1);
    strcpy(q->msg, s);
    return q;
  }
  cnt = 0;
  for (q = qq; q->next; q = q->next)
    cnt++;
  if (cnt > 1000)
    return NULL;		/* return null: did not alter queue */
  q->next = (struct share_msgq *) nmalloc(sizeof(struct share_msgq));

  q = q->next;
  q->chan = chan;
  q->next = NULL;
  q->msg = (char *) nmalloc(strlen(s) + 1);
  strcpy(q->msg, s);
  return qq;
}

/* add stuff to a specific bot's tbuf */

/*
void q_tbuf(char *bot, char *s, struct chanset_t *chan)
{
  int i;
  struct share_msgq *q;

  for (i = 0; i < 5; i++)
    if (!strcasecmp(tbuf[i].bot, bot)) {
      if (chan) {
	fr.match = (FR_CHAN);
	get_user_flagrec(get_user_by_handle(userlist, bot), &fr, chan->name);
      }
      if ((q = q_addmsg(tbuf[i].q, chan, s)))
	tbuf[i].q = q;
    }
}
*/

/* add stuff to the resync buffers */
void q_resync(char *s, struct chanset_t *chan)
{
  int i;
  struct share_msgq *q;

  for (i = 0; i < 5; i++)
    if (tbuf[i].bot[0]) {
      if (chan) {
	fr.match = (FR_CHAN);
	get_user_flagrec(get_user_by_handle(userlist, tbuf[i].bot), &fr, chan->name);
      }
      if ((q = q_addmsg(tbuf[i].q, chan, s)))
	tbuf[i].q = q;
    }
}

/* is bot in resync list? */
int can_resync(char *bot)
{
  int i;

  for (i = 0; i < 5; i++)
    if (!strcasecmp(bot, tbuf[i].bot))
      return 1;
  return 0;
}

/* dump the resync buffer for a bot */
void dump_resync(int idx)
{
  int i;
  struct share_msgq *q;

  Context;
  for (i = 0; i < 5; i++)
    if (!strcasecmp(dcc[idx].nick, tbuf[i].bot)) {
      while (tbuf[i].q) {
	q = tbuf[i].q;
	tbuf[i].q = tbuf[i].q->next;
	dprintf(idx, "%s", q->msg);
	nfree(q->msg);
	nfree(q);
      }
      tbuf[i].bot[0] = 0;
      break;
    }
}

/* give status report on tbufs */
void status_tbufs(int idx)
{
  int i,
    count,
    off = 0;
  struct share_msgq *q;
  char s[121];

  off = 0;
  for (i = 0; i < 5; i++)
    if (tbuf[i].bot[0] && (off < (110 - HANDLEN))) {
      off += my_strcpy(s + off, tbuf[i].bot);
      count = 0;
      for (q = tbuf[i].q; q; q = q->next)
	count++;
      off += simple_sprintf(s + off, STR(" (%d), "), count);
    }
  if (off) {
    s[off - 2] = 0;
    dprintf(idx, STR("Pending sharebot buffers: %s\n"), s);
  }
}

#ifdef HUB
#ifdef OLDCODE
int write_tmp_userfile(char *fn, char *key, struct userrec *bu, int idx)
{
  FILE *f;
  struct userrec *u;
  int ok = 0;

  if (!(f = fopen(fn, "w")))
    log(LCAT_ERROR, STR("Error writing user file to transfer"));
  else {
    chmod(fn, 0600);		/* make it -rw------- */
    enc_fprintf(f, key, STR("#4v: %s -- %s -- transmit\n"), ver, origbotname);
    ok = 1;
    for (u = bu; u && ok; u = u->next)
      ok = write_user(u, key, f, idx);
    ok = write_bans(f, key, idx);
    ok = write_exempts(f, key, idx);
    ok = write_invites(f, key, idx);
    ok = write_config(f, key, idx);
    fclose(f);
    if (!ok)
      log(LCAT_ERROR, STR("Error writing user file to transfer"));
  }
  return ok;
}
#endif
#endif

/* create a copy of the entire userlist (for sending user lists to clone
 * bots) -- userlist is reversed in the process, which is OK because the
 * receiving bot reverses the list AGAIN when saving */

/* t=1: copy only tandem-bots  --  t=0: copy everything BUT tandem-bots */

/*
struct userrec *dup_userlist(int t)
{
  struct userrec *u, *u1, *retu, *nu;
  struct chanuserrec *ch;
  struct user_entry *ue;
  char *p;

  nu = retu = NULL;
  Context;
  noshare = 1;
  for (u = userlist; u; u = u->next)
    if (((u->flags & (USER_BOT | USER_UNSHARED)) && t) ||
	(!(u->flags & (USER_BOT | USER_UNSHARED)) && !t)) {
      p = get_user(&USERENTRY_PASS, u);
      u1 = adduser(NULL, u->handle, 0, p, u->flags);
      u1->flags_udef = u->flags_udef;
      if (!nu)
	nu = retu = u1;
      else {
	nu->next = u1;
	nu = nu->next;
      }
      for (ch = u->chanrec; ch; ch = ch->next) {
	struct chanuserrec *z = add_chanrec(nu, ch->channel);

	if (z) {
	  z->flags = ch->flags;
	  z->flags_udef = ch->flags_udef;
	  z->laston = ch->laston;
	  set_handle_chaninfo(nu, nu->handle, ch->channel, ch->info);
	}
      }
      for (ue = u->entries; ue; ue = ue->next) {
	Context;
	if (ue->name) {
	  struct list_type *lt;
	  struct user_entry *nue;

	  Context;
	  nue = user_malloc(sizeof(struct user_entry));

	  nue->name = user_malloc(strlen(ue->name) + 1);
	  nue->type = NULL;
	  nue->u.list = NULL;
	  strcpy(nue->name, ue->name);
	  list_insert((&nu->entries), nue);
	  Context;
	  for (lt = ue->u.list; lt; lt = lt->next) {
	    struct list_type *list;

	    Context;
	    list = user_malloc(sizeof(struct list_type));

	    list->next = NULL;
	    list->extra = user_malloc(strlen(lt->extra) + 1);
	    strcpy(list->extra, lt->extra);
	    list_append((&nue->u.list), list);
	    Context;
	  }
	} else {
	  Context;	
	  if (ue->type->dup_user && (t || ue->type->got_share))
	    ue->type->dup_user(nu, u, ue);
	  Context;
	}
      }
    }
  noshare = 0;
  Context;
  return retu;
}
*/

/* erase old user list, switch to new one */
void finish_share(int idx)
{
  struct userrec *u,
   *ou;
  struct chanset_t *chan;

  int i,
    j = -1;

  for (i = 0; i < dcc_total; i++) {
    if (!strcasecmp(dcc[i].nick, dcc[idx].host) && (dcc[i].type->flags & DCT_BOT))
      j = i;
  }
  if (j != -1) {
    /* copy the bots over */
    /* No we don't. duh.
       Context;
       u = dup_userlist(1);
     */
    u = NULL;
    Context;
    noshare = 1;

    /* this is where we remove all global and channel bans/exempts/invites and
     * ignores since they will be replaced by what our hub gives us. */

    fr.match = (FR_CHAN);
    while (global_bans)
      u_delban(NULL, global_bans->mask, 1);
    while (global_ign)
      delignore(global_ign->igmask);
    while (global_invites)
      u_delinvite(NULL, global_invites->mask, 1);
    while (global_exempts)
      u_delexempt(NULL, global_exempts->mask, 1);
    for (chan = chanset; chan; chan = chan->next) {
      get_user_flagrec(dcc[j].user, &fr, chan->name);
      while (chan->bans)
	u_delban(chan, chan->bans->mask, 1);
      while (chan->exempts)
	u_delexempt(chan, chan->exempts->mask, 1);
      while (chan->invites)
	u_delinvite(chan, chan->invites->mask, 1);
    }
    noshare = 0;
    ou = userlist;
    userlist = NULL;		/* do this to prevent .user messups */
    /* read the rest in */
    Context;
    for (i = 0; i < dcc_total; i++)
      dcc[i].user = NULL;
    if (!readuserfile(dcc[idx].u.xfer->filename, netpass, &u))
      log(LCAT_ERROR, STR("CAN'T READ NEW USERFILE"));
    else {
      for (i = 0; i < dcc_total; i++)
	dcc[i].user = get_user_by_handle(u, dcc[i].nick);
      Context;
      log(LCAT_BOT, STR("Userfile transferred & loaded."));
      clear_chanlist();
      userlist = u;
      trigger_cfg_changed();
      lastuser = NULL;
      /* lets migrate old channel flags over (unshared channels see) */
      /* also unshared (got_share == 0) userents */
      fr.match = (FR_CHAN);
      for (u = userlist; u; u = u->next) {
	struct userrec *u2 = get_user_by_handle(ou, u->handle);

	if (u2 && !(u2->flags & USER_UNSHARED)) {
	  struct chanuserrec *cr,
	   *cr2,
	   *cr_old = NULL;
	  struct user_entry *ue;

	  Context;
	  for (cr = u2->chanrec; cr; cr = cr2) {
	    struct chanset_t *chan = findchan(cr->channel);

	    cr2 = cr->next;
	    if (chan) {
	      get_user_flagrec(dcc[j].user, &fr, chan->name);
	      /* shared channel, still keep old laston time */
	      Context;
	      for (cr_old = u->chanrec; cr_old; cr_old = cr_old->next)
		if (!rfc_casecmp(cr_old->channel, cr->channel)) {
		  cr_old->laston = cr->laston;
		  break;
		}
	      cr_old = cr;
	    }
	  }
	  /* any unshared user entries need copying over */
	  Context;
	  for (ue = u2->entries; ue; ue = ue->next)
	    if (ue->type && !ue->type->got_share && ue->type->dup_user)
	      ue->type->dup_user(u, u2, ue);
	} else if (!u2 && private_global) {
	  u->flags = 0;
	  u->flags_udef = 0;
	}
      }
      Context;
      clear_userlist(ou);
      unlink(dcc[idx].u.xfer->filename);	/* done with you! */
      reaffirm_owners();	/* make sure my owners are +n */
      updatebot(-1, dcc[j].nick, '+', 0);
      dcc[j].status &= ~STAT_GETTING;
      Context;
    }
  }
  Context;
}

#ifdef HUB
#ifdef OLDCODE
/* begin the user transfer process */
void start_sending_users(int idx)
{
  char s[1024];
  int i = 1;

  /* send a whole lotta: 's ul userfileline' instead of a separate conn? */

  Context;
  sprintf(s, STR(".share.%s.%lu"), dcc[idx].nick, now);
  Context;
  /* Share bots too... duh. */

/*  u = dup_userlist(0); *//* only non-bots */
  write_tmp_userfile(s, netpass, userlist, idx);
  Context;
  /*  clear_userlist(u); */
  Context;
  if ((i = raw_dcc_send(s, STR("*users"), STR("(users)"), s)) > 0) {
    unlink(s);
    Context;
    dprintf(idx, STR("s e %s\n"), STR("Can't send userfile - internal error"));
    Context;
    log(LCAT_BOT, STR("%s -- can't send userfile"), i == 1 ? STR("NO MORE DCC CONNECTIONS") : i == 2 ? STR("CAN'T OPEN A LISTENING SOCKET") : STR("BAD FILE"));
    dcc[idx].status &= ~(STAT_SHARE | STAT_SENDING | STAT_AGGRESSIVE);
  } else {
    Context;
    updatebot(-1, dcc[idx].nick, '+', 0);
    dcc[idx].status |= STAT_SENDING;
    i = dcc_total - 1;
    strcpy(dcc[i].host, dcc[idx].nick);	/* store bot's nick */
    Context;
    dprintf(idx, STR("s us %lu %d %lu\n"), iptolong(natip[0] ? (IP) inet_addr(natip) : getmyip()), dcc[i].port, dcc[i].u.xfer->length);
    /* wish could unlink the file here to avoid possibly leaving it lying
     * around, but that messes up NFS clients. */
    Context;
  }
}
#endif
#endif

void (*def_dcc_bot_kill) (int, void *) = 0;

void cancel_user_xfer(int idx, void *x)
{
  int i,
    j,
    k = 0;

  Context;
  if (idx < 0) {
    idx = -idx;
    k = 1;
    updatebot(-1, dcc[idx].nick, '-', 0);
  }
  flush_tbuf(dcc[idx].nick);
  if (dcc[idx].status & STAT_SHARE) {
    if (dcc[idx].status & STAT_GETTING) {
      j = 0;
      for (i = 0; i < dcc_total; i++)
	if ((strcasecmp(dcc[i].host, dcc[idx].nick) == 0) && ((dcc[i].type->flags & (DCT_FILETRAN | DCT_FILESEND)) == (DCT_FILETRAN | DCT_FILESEND)))
	  j = i;
      if (j != 0) {
	killsock(dcc[j].sock);
	unlink(dcc[j].u.xfer->filename);
	lostdcc(j);
      }
      log(LCAT_BOT, STR("(Userlist download aborted.)"));
      dcc[idx].status &= ~STAT_GETTING;
    }
    if (dcc[idx].status & STAT_SENDING) {
      j = 0;
      for (i = 0; i < dcc_total; i++)
	if ((!strcasecmp(dcc[i].host, dcc[idx].nick)) && ((dcc[i].type->flags & (DCT_FILETRAN | DCT_FILESEND))
							  == DCT_FILETRAN))
	  j = i;
      if (j != 0) {
	killsock(dcc[j].sock);
	unlink(dcc[j].u.xfer->filename);
	lostdcc(j);
      }
      log(LCAT_BOT, STR("(Userlist transmit aborted.)"));
      dcc[idx].status &= ~STAT_SENDING;
    }
    if (allow_resync && (!(dcc[idx].status & STAT_GETTING)) && (!(dcc[idx].status & STAT_SENDING)))
      new_tbuf(dcc[idx].nick);
  }
  Context;
  if (!k)
    def_dcc_bot_kill(idx, x);
  Context;
}

#ifdef G_USETCL
tcl_ints my_ints[] = {
  {"allow-resync", &allow_resync}
  ,
  {"resync-time", &resync_time}
  ,
  {"private-owner", &private_owner}
  ,
  {"private-global", &private_global}
  ,
  {"private-user", &private_user}
  ,
  {0, 0}
};

tcl_strings my_strings[] = {
  {"private-globals", private_globals, 50, 0}
  ,
  {NULL, NULL, 0, 0}
};

#endif

void cmd_flush(struct userrec *u, int idx, char *par)
{
  if (!par[0])
    dprintf(idx, STR("Usage: flush <botname>\n"));
  else if (flush_tbuf(par))
    dprintf(idx, STR("Flushed resync buffer for %s\n"), par);
  else
    dprintf(idx, STR("There is no resync buffer for that bot.\n"));
}

cmd_t my_cmds[] = {
  {"flush", "n", (Function) cmd_flush, NULL}
  ,
  {0, 0, 0, 0}
};

int share_expmem()
{
  int i,
    tot = 0;
  struct share_msgq *q;

  for (i = 0; i < 5; i++)
    if (tbuf[i].bot[0])
      for (q = tbuf[i].q; q; q = q->next) {
	tot += sizeof(struct share_msgq);

	tot += strlen(q->msg) + 1;
      }
  return tot;
}

void share_report(int idx, int details)
{
  int i,
    j;

  if (details) {
    dprintf(idx, STR("    Share module, using %d bytes.\n"), share_expmem());
    dprintf(idx, STR("    Private owners: %3s   Allow resync: %3s\n"), (private_global || (private_globals_bitmask() & USER_OWNER)) ? "yes" : "no", allow_resync ? "yes" : "no");
    for (i = 0; i < dcc_total; i++)
      if (dcc[i].type == &DCC_BOT) {
	if (dcc[i].status & STAT_GETTING) {
	  int ok = 0;

	  for (j = 0; j < dcc_total; j++)
	    if (((dcc[j].type->flags & (DCT_FILETRAN | DCT_FILESEND))
		 == (DCT_FILETRAN | DCT_FILESEND)) && !strcasecmp(dcc[j].host, dcc[i].nick)) {
	      dprintf(idx, STR("Downloading userlist from %s (%d%% done)\n"), dcc[i].nick, (int) (100.0 * ((float) dcc[j].status) / ((float) dcc[j].u.xfer->length)));
	      ok = 1;
	    }
	  if (!ok)
	    dprintf(idx, STR("Download userlist from %s (negotiating botentries)\n"));
	} else if (dcc[i].status & STAT_SENDING) {
	  for (j = 0; j < dcc_total; j++) {
	    if (((dcc[j].type->flags & (DCT_FILETRAN | DCT_FILESEND))
		 == DCT_FILETRAN)
		&& !strcasecmp(dcc[j].host, dcc[i].nick)) {
	      if (dcc[j].type == &DCC_GET)
		dprintf(idx, STR("Sending userlist to %s (%d%% done)\n"), dcc[i].nick, (int) (100.0 * ((float) dcc[j].status) / ((float) dcc[j].u.xfer->length)));
	      else
		dprintf(idx, STR("Sending userlist to %s (waiting for connect)\n"), dcc[i].nick);
	    }
	  }
	} else if (dcc[i].status & STAT_AGGRESSIVE) {
	  dprintf(idx, STR("    Passively sharing with %s.\n"), dcc[i].nick);
	} else if (dcc[i].status & STAT_SHARE) {
	  dprintf(idx, STR("    Agressively sharing with %s.\n"), dcc[i].nick);
	}
      }
    status_tbufs(idx);
  }
}

void init_share()
{
  int i;

  Context;
  add_hook(HOOK_SHAREOUT, (Function) shareout_mod);
  add_hook(HOOK_SHAREIN, (Function) sharein_mod);
  add_hook(HOOK_MINUTELY, (Function) check_expired_tbufs);
  add_hook(HOOK_READ_USERFILE, (Function) hook_read_userfile);
  for (i = 0; i < 5; i++) {
    tbuf[i].q = NULL;
    tbuf[i].bot[0] = 0;
  }
  def_dcc_bot_kill = DCC_BOT.kill;
  DCC_BOT.kill = cancel_user_xfer;
#ifdef G_USETCL
  add_tcl_ints(my_ints);
  add_tcl_strings(my_strings);
#endif
  add_builtins(H_dcc, my_cmds);
  Context;
}

int private_globals_bitmask()
{
  struct flag_record fr = { FR_GLOBAL, 0, 0, 0, 0 };

  break_down_flags(private_globals, &fr, 0);
  return fr.global |(private_owner ? USER_OWNER : 0);
}
