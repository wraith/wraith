/*
 * share.c -- part of share.mod
 *
 */

#define MODULE_NAME "share"
#define MAKING_SHARE

#include "src/mod/module.h"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <stdarg.h>

#include "src/chan.h"
#include "src/net.h"
#include "src/users.h"
#include "src/egg_timer.h"
#include "src/mod/transfer.mod/transfer.h"
#include "src/mod/channels.mod/channels.h"
#ifdef LEAF
#  include "irc.mod/irc.h"
#endif /* LEAF */

/* Minimum version I will share with. */
static const int min_share		= 1000000;
/* Earliest version that supports exempts and invites. */
static const int min_exemptinvite	= 1000000;
/* Minimum version that supports userfile features. */
static const int min_uffeature		= 1000000;

static Function *global = NULL, *transfer_funcs = NULL, *channels_funcs = NULL;
#ifdef LEAF
static Function *irc_funcs = NULL;
#endif /* LEAF */

static int private_global = 0;
static int private_user = 0;
static char private_globals[50] = "";
static int allow_resync = 0;
static struct flag_record fr = {0, 0, 0, 0, 0, 0};
static int resync_time = 900;
static int overr_local_bots = 1;	/* Override local bots?		    */


struct delay_mode {
  struct delay_mode *next;
  struct chanset_t *chan;
  int plsmns;
  int mode;
  char *mask;
  int seconds;
};

static struct delay_mode *start_delay = NULL;


/* Store info for sharebots */
struct share_msgq {
  struct chanset_t *chan;
  char *msg;
  struct share_msgq *next;
};

typedef struct tandbuf_t {
  char bot[HANDLEN + 1];
  time_t timer;
  struct share_msgq *q;
  struct tandbuf_t *next;
} tandbuf;

tandbuf *tbuf;

/* Prototypes */
static void start_sending_users(int);
static void shareout_but (struct chanset_t *, ...);
static int flush_tbuf(char *);
static int can_resync(char *);
static void dump_resync(int);
static void q_resync(char *, struct chanset_t *);
static void cancel_user_xfer(int, void *);
static int private_globals_bitmask();

#include "share.h"

#include "uf_features.c"

/*
 *   Sup's delay code
 */

static void add_delay(struct chanset_t *chan, int plsmns, int mode, char *mask)
{
  struct delay_mode *d = NULL;

  d = (struct delay_mode *) calloc(1, sizeof(struct delay_mode));

  d->chan = chan;
  d->plsmns = plsmns;
  d->mode = mode;
  d->mask = (char *) calloc(1, strlen(mask) + 1);

  strncpy(d->mask, mask, strlen(mask) + 1);
  d->seconds = (int) (now + randint(20));
  d->next = start_delay;
  start_delay = d;
}

static void del_delay(struct delay_mode *delay)
{
  struct delay_mode *d = NULL, *old = NULL;

  for (d = start_delay; d; old = d, d = d->next) {
    if (d == delay) {
     if (old)
        old->next = d->next;
      else
        start_delay = d->next;
      if (d->mask)
        free(d->mask);
      free(d);
      break;
    }
  }
}

static void check_delay()
{
  struct delay_mode *d = NULL, *dnext = NULL;

  for (d = start_delay; d; d = dnext) {
    dnext = d->next;
    if (d->seconds <= now) {
      add_mode(d->chan, d->plsmns, d->mode, d->mask);
      del_delay(d);
    }
  }
}

/*
 *   Botnet commands
 */

static void share_stick_ban(int idx, char *par)
{
  char *host = NULL, *val = NULL;
  int yn;

  if (dcc[idx].status & STAT_SHARE) {
    host = newsplit(&par);
    val = newsplit(&par);
    yn = atoi(val);
    noshare = 1;
    if (!par[0]) {		/* Global ban */
#ifdef LEAF
      struct chanset_t *chan = NULL;
#endif /* LEAF */
      if (u_setsticky_ban(NULL, host, yn) > 0) {
       putlog(LOG_CMDS, "@", "%s: %s %s", dcc[idx].nick, (yn) ? "stick" : "unstick", host);
	shareout_but(NULL, idx, "s %s %d\n", host, yn);
      }
#ifdef LEAF
      for (chan = chanset; chan != NULL; chan = chan->next)
        check_this_ban(chan, host, yn);
#endif /* LEAF */
    } else {
      struct chanset_t *chan = findchan_by_dname(par);
      struct chanuserrec *cr;

      if ((chan !=NULL) && ((channel_shared(chan) &&
                             ((cr = get_chanrec(dcc[idx].user, par)) &&
                              (bot_aggressive_to(dcc[idx].user)))) ||
                            (1)))
	if (u_setsticky_ban(chan, host, yn) > 0) {
          putlog(LOG_CMDS, "@", "%s: %s %s %s", dcc[idx].nick, (yn) ? "stick" : "unstick", host, par);
	  shareout_but(chan, idx, "s %s %d %s\n", host, yn, chan->dname);
	  noshare = 0;
	  return;
	}
#ifdef LEAF
      if (chan)
        check_this_ban(chan, host, yn);
#endif /* LEAF */
      putlog(LOG_CMDS, "@", "Rejecting invalid sticky exempt: %s on %s%s", host, par, yn ? "" : " (unstick)");
    }
    noshare = 0;
  }
}

/* Same as share_stick_ban, only for exempts.
 */
static void share_stick_exempt(int idx, char *par)
{
  char *host = NULL, *val = NULL;
  int yn;

  if (dcc[idx].status & STAT_SHARE) {
    host = newsplit(&par);
    val = newsplit(&par);
    yn = atoi(val);
    noshare = 1;
    if (!par[0]) {		/* Global exempt */
      if (u_setsticky_exempt(NULL, host, yn) > 0) {
        putlog(LOG_CMDS, "@", "%s: %s %s", dcc[idx].nick, (yn) ? "stick" : "unstick", host);
	shareout_but(NULL, idx, "se %s %d\n", host, yn);
      }
    } else {
      struct chanset_t *chan = findchan_by_dname(par);
      struct chanuserrec *cr = NULL;

      if ((chan != NULL) && ((channel_shared(chan) &&
                             ((cr = get_chanrec(dcc[idx].user, par)) &&
                              (bot_aggressive_to(dcc[idx].user)))) ||
                            (1)))
	if (u_setsticky_exempt(chan, host, yn) > 0) {
          putlog(LOG_CMDS, "@", "%s: stick %s %c %s", dcc[idx].nick, host, yn ? 'y' : 'n', par);
	  shareout_but(chan, idx, "se %s %d %s\n", host, yn, chan->dname);
	  noshare = 0;
	  return;
	}
      putlog(LOG_CMDS, "@", "Rejecting invalid sticky exempt: %s on %s, %c", host, par, yn ? 'y' : 'n');
    }
    noshare = 0;
  }
}

/* Same as share_stick_ban, only for invites.
 */
static void share_stick_invite (int idx, char * par) {
  char *host = NULL, *val = NULL;
  int yn;

  if (dcc[idx].status & STAT_SHARE) {
    host = newsplit(&par);
    val = newsplit(&par);
    yn = atoi(val);
    noshare = 1;
    if (!par[0]) {		/* Global invite */
      if (u_setsticky_invite(NULL, host, yn) > 0) {
        putlog(LOG_CMDS, "@", "%s: %s %s", dcc[idx].nick, (yn) ? "stick" : "unstick", host);
        shareout_but(NULL, idx, "sInv %s %d\n", host, yn);
      }
    } else {
      struct chanset_t *chan = findchan_by_dname(par);
      struct chanuserrec *cr = NULL;

      if ((chan != NULL) && ((channel_shared(chan) &&
                             ((cr = get_chanrec(dcc[idx].user, par)) &&
                              (bot_aggressive_to(dcc[idx].user)))) ||
                            (1)))
	if (u_setsticky_invite(chan, host, yn) > 0) {
          putlog(LOG_CMDS, "@", "%s: %s %s %s", dcc[idx].nick, (yn) ? "stick" : "unstick", host, par);
	  shareout_but(chan, idx, "sInv %s %d %s\n", host, yn, chan->dname);
	  noshare = 0;
	  return;
	}
      putlog(LOG_CMDS, "@", "Rejecting invalid sticky invite: %s on %s%s", host, par, yn ? "" : " (unstick)");
    }
    noshare = 0;
  }
}

static void share_chhand(int idx, char *par)
{
  char *hand = NULL;
  struct userrec *u = NULL;

  if ((dcc[idx].status & STAT_SHARE) && !private_user) {
    hand = newsplit(&par);
    u = get_user_by_handle(userlist, hand);
    if (u && !(u->flags & USER_UNSHARED)) {
      shareout_but(NULL, idx, "h %s %s\n", hand, par);
      noshare = 1;
      if (change_handle(u, par))
	putlog(LOG_CMDS, "@", "%s: handle %s->%s", dcc[idx].nick, hand, par);
      noshare = 0;
    }
  }
}

static void share_chattr(int idx, char *par)
{
  char *hand = NULL, *atr = NULL, s[100] = "";
  struct chanset_t *cst = NULL;
  struct userrec *u = NULL;
  struct flag_record fr2;
  int bfl, ofl;
  module_entry *me = NULL;

  if ((dcc[idx].status & STAT_SHARE) && !private_user) {
    hand = newsplit(&par);
    u = get_user_by_handle(userlist, hand);
    if (u && !(u->flags & USER_UNSHARED)) {
      atr = newsplit(&par);
      cst = findchan_by_dname(par);
      if (!par[0] || (cst && channel_shared(cst))) {
	if (!(dcc[idx].status & STAT_GETTING) && (cst || !private_global))
	  shareout_but(cst, idx, "a %s %s %s\n", hand, atr, par);
	noshare = 1;
	if (par[0] && cst) {
	  fr.match = (FR_CHAN | FR_BOT);
	  get_user_flagrec(dcc[idx].user, &fr, par);
	  if (bot_chan(fr) || bot_global(fr)) {
	    fr.match = FR_CHAN;
	    fr2.match = FR_CHAN;
	    break_down_flags(atr, &fr, 0);
	    get_user_flagrec(u, &fr2, par);
	    fr.chan = (fr2.chan & BOT_AGGRESSIVE) |
	      (fr.chan & ~BOT_AGGRESSIVE);
	    set_user_flagrec(u, &fr, par);
	    check_dcc_chanattrs(u, par, fr.chan, fr2.chan);
	    noshare = 0;
	    build_flags(s, &fr, 0);
	    if (!(dcc[idx].status & STAT_GETTING))
	      putlog(LOG_CMDS, "@", "%s: chattr %s %s %s",
		     dcc[idx].nick, hand, s, par);
	    if ((me = module_find("irc", 0, 0))) {
	      Function *func = me->funcs;

	      (func[IRC_RECHECK_CHANNEL]) (cst, 0);
	    }
	  } else
	    putlog(LOG_CMDS, "*",
		   "Rejected flags for unshared channel %s from %s",
		   par, dcc[idx].nick);
	} else if (!private_global) {
	  int pgbm = private_globals_bitmask();

	  fr.match = FR_GLOBAL;

	  get_user_flagrec(dcc[idx].user, &fr, 0);

	  /* Don't let bot flags be altered */
	  ofl = fr.global;
	  break_down_flags(atr, &fr, 0);
	  bfl = u->flags & USER_BOT;
	  fr.global = (fr.global &~pgbm)
	  |(u->flags & pgbm);
	  fr.global = sanity_check(fr.global |bfl);

	  set_user_flagrec(u, &fr, 0);
	  check_dcc_attrs(u, ofl);
	  noshare = 0;
	  build_flags(s, &fr, 0);
	  fr.match = FR_CHAN;
#ifndef LEAF
	  if (!(dcc[idx].status & STAT_GETTING))
	    putlog(LOG_CMDS, "@", "%s: chattr %s %s", dcc[idx].nick, hand, s);
#endif /* LEAF */
	  if ((me = module_find("irc", 0, 0))) {
	    Function *func = me->funcs;

	    for (cst = chanset; cst; cst = cst->next)
	      (func[IRC_RECHECK_CHANNEL]) (cst, 0);
	  }
	} else
	  putlog(LOG_CMDS, "@", "Rejected global flags for %s from %s",
		 hand, dcc[idx].nick);
        noshare = 0;
      }
    }
  }
}

static void share_pls_chrec(int idx, char *par)
{
  char *user = NULL;
  struct chanset_t *chan = NULL;
  struct userrec *u = NULL;

  if ((dcc[idx].status & STAT_SHARE) && !private_user) {
    user = newsplit(&par);
    if ((u = get_user_by_handle(userlist, user))) {
      chan = findchan_by_dname(par);
      fr.match = (FR_CHAN | FR_BOT);
      get_user_flagrec(dcc[idx].user, &fr, par);
      if (!chan || !channel_shared(chan) ||
	  !(bot_chan(fr) || bot_global(fr)))
	putlog(LOG_CMDS, "*",
	       "Rejected info for unshared channel %s from %s",
	       par, dcc[idx].nick);
      else {
	noshare = 1;
	shareout_but(chan, idx, "+cr %s %s\n", user, par);
	if (!get_chanrec(u, par)) {
	  add_chanrec(u, par);
	  putlog(LOG_CMDS, "@", "%s: +chrec %s %s", dcc[idx].nick, user, par);
	}
	noshare = 0;
      }
    }
  }
}

static void share_mns_chrec(int idx, char *par)
{
  char *user = NULL;
  struct chanset_t *chan = NULL;
  struct userrec *u = NULL;

  if ((dcc[idx].status & STAT_SHARE) && !private_user) {
    user = newsplit(&par);
    if ((u = get_user_by_handle(userlist, user))) {
      chan = findchan_by_dname(par);
      fr.match = (FR_CHAN | FR_BOT);
      get_user_flagrec(dcc[idx].user, &fr, par);
      if (!chan || !channel_shared(chan) ||
	  !(bot_chan(fr) || bot_global(fr)))
	putlog(LOG_CMDS, "*",
	       "Rejected info for unshared channel %s from %s",
	       par, dcc[idx].nick);
      else {
	noshare = 1;
	del_chanrec(u, par);
	shareout_but(chan, idx, "-cr %s %s\n", user, par);
	noshare = 0;
	putlog(LOG_CMDS, "@", "%s: -chrec %s %s", dcc[idx].nick, user, par);
      }
    }
  }
}

static void share_newuser(int idx, char *par)
{
  char *nick = NULL, *host = NULL, *pass = NULL, s[100] = "";
  struct userrec *u = NULL;

  if ((dcc[idx].status & STAT_SHARE) && !private_user) {
    nick = newsplit(&par);
    host = newsplit(&par);
    pass = newsplit(&par);

    if (!(u = get_user_by_handle(userlist, nick)) ||
	!(u->flags & USER_UNSHARED)) {
      fr.global = 0;

      fr.match = FR_GLOBAL;
      break_down_flags(par, &fr, NULL);

      /* If user already exists, ignore command */
      shareout_but(NULL, idx, "n %s %s %s %s\n", nick, host, pass,
		   private_global ? (fr.global & USER_BOT ? "b" : "-") : par);

      if (!u) {
	noshare = 1;
	if (strlen(nick) > HANDLEN)
	  nick[HANDLEN] = 0;

	if (private_global)
	  fr.global &= USER_BOT;
	else {
	  /* It shouldn't be done before sending to other bots? */
	  int pgbm = private_globals_bitmask();

	  fr.match = FR_GLOBAL;
	  fr.global &=~pgbm;
	}

	build_flags(s, &fr, 0);
	userlist = adduser(userlist, nick, host, pass, 0);

	/* Support for userdefinedflag share - drummer */
	u = get_user_by_handle(userlist, nick);
	set_user_flagrec(u, &fr, 0);
	fr.match = FR_CHAN; /* why?? */
	noshare = 0;
#ifndef LEAF
	putlog(LOG_CMDS, "@", "%s: newuser %s %s", dcc[idx].nick, nick, s);
#endif /* LEAF */
      }
    }
  }
}

static void share_killuser(int idx, char *par)
{
  struct userrec *u = NULL;

  /* If user is a share bot, ignore command */
  if ((dcc[idx].status & STAT_SHARE) && !private_user &&
      (u = get_user_by_handle(userlist, par)) &&
      !(u->flags & USER_UNSHARED) &&
      !((u->flags & USER_BOT) && (bot_flags(u) & BOT_SHARE))) {
    noshare = 1;
    if (deluser(par)) {
      shareout_but(NULL, idx, "k %s\n", par);
#ifndef LEAF
      putlog(LOG_CMDS, "@", "%s: killuser %s", dcc[idx].nick, par);
#endif /* LEAF */
    }
    noshare = 0;
  }
}

static void share_pls_host(int idx, char *par)
{
  char *hand = NULL;
  struct userrec *u = NULL;

  if ((dcc[idx].status & STAT_SHARE) && !private_user) {
    hand = newsplit(&par);
    if ((u = get_user_by_handle(userlist, hand)) &&
	!(u->flags & USER_UNSHARED)) {
      shareout_but(NULL, idx, "+h %s %s\n", hand, par);
      set_user(&USERENTRY_HOSTS, u, par);
#ifdef LEAF
      check_this_user(u->handle, 0, NULL);
#else /* !LEAF */
      putlog(LOG_CMDS, "@", "%s: +host %s %s", dcc[idx].nick, hand, par);
#endif /* LEAF */
    }
  }
}

static void share_pls_bothost(int idx, char *par)
{
  char *hand = NULL, p[32] = "";
  struct userrec *u = NULL;

  if ((dcc[idx].status & STAT_SHARE) && !private_user) {
    hand = newsplit(&par);
    if (!(u = get_user_by_handle(userlist, hand)) ||
	!(u->flags & USER_UNSHARED)) {
      if (!(dcc[idx].status & STAT_GETTING))
	shareout_but(NULL, idx, "+bh %s %s\n", hand, par);
      /* Add bot to userlist if not there */
      if (u) {
	if (!(u->flags & USER_BOT))
	  return;		/* ignore */
	set_user(&USERENTRY_HOSTS, u, par);
      } else {
	makepass(p);
	userlist = adduser(userlist, hand, par, p, USER_BOT);
      }
#ifndef LEAF
      if (!(dcc[idx].status & STAT_GETTING))
	putlog(LOG_CMDS, "@", "%s: +host %s %s", dcc[idx].nick, hand, par);
#endif /* LEAF */
    }
  }
}

static void share_mns_host(int idx, char *par)
{
  char *hand = NULL;
  struct userrec *u = NULL;

  if ((dcc[idx].status & STAT_SHARE) && !private_user) {
    hand = newsplit(&par);
    if ((u = get_user_by_handle(userlist, hand)) &&
	!(u->flags & USER_UNSHARED)) {
      shareout_but(NULL, idx, "-h %s %s\n", hand, par);
      noshare = 1;
      delhost_by_handle(hand, par);
      noshare = 0;
#ifdef LEAF
      check_this_user(hand, 2, par);
#else /* !LEAF */
      putlog(LOG_CMDS, "@", "%s: -host %s %s", dcc[idx].nick, hand, par);
#endif /* LEAF */
    }
  }
}

static void share_change(int idx, char *par)
{
  char *key = NULL, *hand = NULL;
  struct userrec *u = NULL;
  struct user_entry_type *uet = NULL;
  struct user_entry *e = NULL;

  if ((dcc[idx].status & STAT_SHARE) && !private_user) {
    key = newsplit(&par);
    hand = newsplit(&par);
    if (!(u = get_user_by_handle(userlist, hand)) ||
	!(u->flags & USER_UNSHARED)) {
      if (!(uet = find_entry_type(key)))
	/* If it's not a supported type, forget it */
	debug2("Ignore ch %s from %s (unknown type)", key, dcc[idx].nick);
      else {
	if (!(dcc[idx].status & STAT_GETTING))
	  shareout_but(NULL, idx, "c %s %s %s\n", key, hand, par);
	noshare = 1;
	if (!u && (uet == &USERENTRY_BOTADDR)) {
	  char pass[30] = "";

	  makepass(pass);
	  userlist = adduser(userlist, hand, "none", pass, USER_BOT);
	  u = get_user_by_handle(userlist, hand);
	} else if (!u)
	  return;
	if (uet->got_share) {
	  if (!(e = find_user_entry(uet, u))) {
	    e = calloc(1, sizeof(struct user_entry));

	    e->type = uet;
	    e->name = NULL;
	    e->u.list = NULL;
	    list_insert((&(u->entries)), e);
	  }
	  uet->got_share(u, e, par, idx);
	  if (!e->u.list) {
	    list_delete((struct list_type **) &(u->entries),
			(struct list_type *) e);
	    free(e);
	  }
	}
	noshare = 0;
      }
    }
  }
}

static void share_chchinfo(int idx, char *par)
{
  char *hand = NULL, *chan = NULL;
  struct chanset_t *cst = NULL;
  struct userrec *u = NULL;

  if ((dcc[idx].status & STAT_SHARE) && !private_user) {
    hand = newsplit(&par);
    if ((u = get_user_by_handle(userlist, hand)) &&
	!(u->flags & USER_UNSHARED)) {
      chan = newsplit(&par);
      cst = findchan_by_dname(chan);
      fr.match = (FR_CHAN | FR_BOT);
      get_user_flagrec(dcc[idx].user, &fr, chan);
      if (!cst || !channel_shared(cst) || !(bot_chan(fr) || bot_global(fr)))
	putlog(LOG_CMDS, "*",
	       "Info line change from %s denied.  Channel %s not shared.",
	       dcc[idx].nick, chan);
      else {
	shareout_but(cst, idx, "chchinfo %s %s %s\n", hand, chan, par);
	noshare = 1;
	set_handle_chaninfo(userlist, hand, chan, par);
	noshare = 0;
	putlog(LOG_CMDS, "@", "%s: change info %s %s", dcc[idx].nick,
	       chan, hand);
      }
    }
  }
}

static void share_mns_ban(int idx, char *par)
{
  struct chanset_t *chan = NULL;

  if (dcc[idx].status & STAT_SHARE) {
    shareout_but(NULL, idx, "-b %s\n", par);
    putlog(LOG_CMDS, "@", "%s: cancel ban %s", dcc[idx].nick, par);
    str_unescape(par, '\\');
    noshare = 1;
    if (u_delban(NULL, par, 1) > 0) {
      for (chan = chanset; chan; chan = chan->next)
	add_delay(chan, '-', 'b', par);
    }
    noshare = 0;
  }
}

static void share_mns_exempt(int idx, char *par)
{
  struct chanset_t *chan = NULL;

  if (dcc[idx].status & STAT_SHARE) {
    shareout_but(NULL, idx, "-e %s\n", par);
    putlog(LOG_CMDS, "@", "%s: cancel exempt %s", dcc[idx].nick, par);
    str_unescape(par, '\\');
    noshare = 1;
    if (u_delexempt(NULL, par, 1) > 0) {
      for (chan = chanset; chan; chan = chan->next)
	add_delay(chan, '-', 'e', par);
    }
    noshare = 0;
  }
}

static void share_mns_invite(int idx, char *par)
{
  struct chanset_t *chan = NULL;

  if (dcc[idx].status & STAT_SHARE) {
    shareout_but(NULL, idx, "-inv %s\n", par);
    putlog(LOG_CMDS, "@", "%s: cancel invite %s", dcc[idx].nick, par);
    str_unescape(par, '\\');
    noshare = 1;
    if (u_delinvite(NULL, par, 1) > 0) {
      for (chan = chanset; chan; chan = chan->next)
	add_delay(chan, '-', 'I', par);
    }
    noshare = 0;
  }
}

static void share_mns_banchan(int idx, char *par)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  if (dcc[idx].status & STAT_SHARE) {
    chname = newsplit(&par);
    chan = findchan_by_dname(chname);
    fr.match = (FR_CHAN | FR_BOT);
    get_user_flagrec(dcc[idx].user, &fr, chname);
    if (!chan || !channel_shared(chan) ||
        !(bot_chan(fr) || bot_global(fr)))
      putlog(LOG_CMDS, "*", "Cancel channel ban %s on %s rejected - channel not shared.", par, chname);
    else {
      shareout_but(chan, idx, "-bc %s %s\n", chname, par);
      putlog(LOG_CMDS, "@", "%s: cancel ban %s on %s", dcc[idx].nick, par, chname);
      str_unescape(par, '\\');
      noshare = 1;
      if (u_delban(chan, par, 1) > 0)
	add_delay(chan, '-', 'b', par);
      noshare = 0;
    }
  }
}

static void share_mns_exemptchan(int idx, char *par)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  if (dcc[idx].status & STAT_SHARE) {
    chname = newsplit(&par);
    chan = findchan_by_dname(chname);
    fr.match = (FR_CHAN | FR_BOT);
    get_user_flagrec(dcc[idx].user,&fr,chname);
    if (!chan || !channel_shared(chan) ||
        !(bot_chan(fr) || bot_global(fr)))
      putlog(LOG_CMDS, "*",
        "Cancel channel exempt %s on %s rejected - channel not shared.",
        par, chname);
    else {
      shareout_but(chan,idx, "-ec %s %s\n", chname, par);
      putlog(LOG_CMDS, "@", "%s: cancel exempt %s on %s", dcc[idx].nick,
	     par, chname);
      str_unescape(par, '\\');
      noshare = 1;
      if (u_delexempt(chan, par, 1) > 0)
	add_delay(chan, '-', 'e', par);
      noshare = 0;
    }
  }
}

static void share_mns_invitechan (int idx, char *par)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  if (dcc[idx].status & STAT_SHARE) {
    chname = newsplit(&par);
    chan = findchan_by_dname(chname);
    fr.match = (FR_CHAN | FR_BOT);
    get_user_flagrec(dcc[idx].user,&fr,chname);
    if (!chan || !channel_shared(chan) ||
        !(bot_chan(fr) || bot_global(fr)))
      putlog(LOG_CMDS, "*",
        "Cancel channel invite %s on %s rejected - channel not shared.",
        par, chname);
    else {
      shareout_but(chan,idx, "-invc %s %s\n", chname, par);
      putlog(LOG_CMDS, "@", "%s: cancel invite %s on %s", dcc[idx].nick,
	     par, chname);
      str_unescape(par, '\\');
      noshare = 1;
      if (u_delinvite(chan, par, 1) > 0)
	add_delay(chan, '-', 'I', par);
      noshare = 0;
    }
  }
}

static void share_mns_ignore(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    shareout_but(NULL, idx, "-i %s\n", par);
    putlog(LOG_CMDS, "@", "%s: cancel ignore %s", dcc[idx].nick, par);
    str_unescape(par, '\\');
    noshare = 1;
    delignore(par);
    noshare = 0;
  }
}

static void share_pls_ban(int idx, char *par)
{
#ifdef LEAF
  struct chanset_t *chan = NULL;
#endif /* LEAF */
  time_t expire_time;
  char *ban = NULL, *tm = NULL, *from = NULL;
  int flags = 0;

  if (dcc[idx].status & STAT_SHARE) {
    shareout_but(NULL, idx, "+b %s\n", par);
    noshare = 1;
    ban = newsplit(&par);
    str_unescape(ban, '\\');
    tm = newsplit(&par);
    from = newsplit(&par);
    if (strchr(from, 's'))
      flags |= MASKREC_STICKY;
    if (strchr(from, 'p'))
      flags |= MASKREC_PERM;
    from = newsplit(&par);
    expire_time = (time_t) atoi(tm);
    if (expire_time != 0L)
      expire_time += now;
    u_addban(NULL, ban, from, par, expire_time, flags);
    putlog(LOG_CMDS, "@", "%s: global ban %s (%s:%s)", dcc[idx].nick, ban,
	   from, par);
    noshare = 0;
#ifdef LEAF
    for (chan = chanset; chan != NULL; chan = chan->next)
      check_this_ban(chan, ban, 0);
#endif /* LEAF */
  }
}

static void share_pls_banchan(int idx, char *par)
{
  time_t expire_time;
  int flags = 0;
  struct chanset_t *chan = NULL;
  char *ban = NULL, *tm = NULL, *chname = NULL, *from = NULL;

  if (dcc[idx].status & STAT_SHARE) {
    ban = newsplit(&par);
    tm = newsplit(&par);
    chname = newsplit(&par);
    chan = findchan_by_dname(chname);
    fr.match = (FR_CHAN | FR_BOT);
    get_user_flagrec(dcc[idx].user, &fr, chname);
    if (!chan || !channel_shared(chan) ||
	!(bot_chan(fr) || bot_global(fr)))
      putlog(LOG_CMDS, "*",
	     "Channel ban %s on %s rejected - channel not shared.",
	     ban, chname);
    else {
      shareout_but(chan, idx, "+bc %s %s %s %s\n", ban, tm, chname, par);
      str_unescape(ban, '\\');
      from = newsplit(&par);
      if (strchr(from, 's'))
	flags |= MASKREC_STICKY;
      if (strchr(from, 'p'))
	flags |= MASKREC_PERM;
      from = newsplit(&par);
      putlog(LOG_CMDS, "@", "%s: ban %s on %s (%s:%s)", dcc[idx].nick,
	     ban, chname, from, par);
      noshare = 1;
      expire_time = (time_t) atoi(tm);
      if (expire_time != 0L)
	expire_time += now;
      u_addban(chan, ban, from, par, expire_time, flags);
      noshare = 0;
#ifdef LEAF
      check_this_ban(chan, ban, 0);
#endif /* LEAF */
    }
  }
}

/* Same as share_pls_ban, only for exempts.
 */
static void share_pls_exempt(int idx, char *par)
{
  time_t expire_time;
  char *exempt = NULL, *tm = NULL, *from = NULL;
  int flags = 0;

  if (dcc[idx].status & STAT_SHARE) {
    shareout_but(NULL,idx, "+e %s\n", par);
    noshare = 1;
    exempt = newsplit(&par);
    str_unescape(exempt, '\\');
    tm = newsplit(&par);
    from = newsplit(&par);
    if (strchr(from,'s'))
      flags |= MASKREC_STICKY;
    if (strchr(from,'p'))
      flags |= MASKREC_PERM;
    from = newsplit(&par);
    expire_time = (time_t) atoi(tm);
    if (expire_time != 0L)
      expire_time += now;
    u_addexempt(NULL,exempt, from, par, expire_time,flags);
    putlog(LOG_CMDS, "@", "%s: global exempt %s (%s:%s)", dcc[idx].nick, exempt,
	   from, par);
    noshare = 0;
  }
}

/* Same as share_pls_banchan, only for exempts.
 */
static void share_pls_exemptchan(int idx, char *par)
{
  time_t expire_time;
  int flags = 0;
  struct chanset_t *chan = NULL;
  char *exempt = NULL, *tm = NULL, *chname = NULL, *from = NULL;

  if (dcc[idx].status & STAT_SHARE) {
    exempt = newsplit(&par);
    tm = newsplit(&par);
    chname = newsplit(&par);
    chan = findchan_by_dname(chname);
    fr.match = (FR_CHAN | FR_BOT);
    get_user_flagrec(dcc[idx].user,&fr,chname);
    if (!chan || !channel_shared(chan) ||
	!(bot_chan(fr) || bot_global(fr)))
      putlog(LOG_CMDS, "*",
	     "Channel exempt %s on %s rejected - channel not shared.",
	     exempt, chname);
    else {
      shareout_but(chan,idx, "+ec %s %s %s %s\n", exempt, tm, chname, par);
      str_unescape(exempt, '\\');
      from = newsplit(&par);
      if (strchr(from,'s'))
	flags |= MASKREC_STICKY;
      if (strchr(from,'p'))
	flags |= MASKREC_PERM;
      from = newsplit(&par);
      putlog(LOG_CMDS, "@", "%s: exempt %s on %s (%s:%s)", dcc[idx].nick,
	     exempt, chname, from, par);
      noshare = 1;
      expire_time = (time_t) atoi(tm);
      if (expire_time != 0L)
	expire_time += now;
      u_addexempt(chan, exempt, from, par, expire_time,flags);
      noshare = 0;
    }
  }
}

/* Same as share_pls_ban, only for invites.
 */
static void share_pls_invite(int idx, char *par)
{
  time_t expire_time;
  char *invite = NULL, *tm = NULL, *from = NULL;
  int flags = 0;

  if (dcc[idx].status & STAT_SHARE) {
    shareout_but(NULL,idx, "+inv %s\n", par);
    noshare = 1;
    invite = newsplit(&par);
    str_unescape(invite, '\\');
    tm = newsplit(&par);
    from = newsplit(&par);
    if (strchr(from,'s'))
      flags |= MASKREC_STICKY;
    if (strchr(from,'p'))
      flags |= MASKREC_PERM;
    from = newsplit(&par);
    expire_time = (time_t) atoi(tm);
    if (expire_time != 0L)
      expire_time += now;
    u_addinvite(NULL,invite, from, par, expire_time,flags);
    putlog(LOG_CMDS, "@", "%s: global invite %s (%s:%s)", dcc[idx].nick,
	   invite, from, par);
    noshare = 0;
  }
}

/* Same as share_pls_banchan, only for invites.
 */
static void share_pls_invitechan(int idx, char *par)
{
  time_t expire_time;
  int flags = 0;
  struct chanset_t *chan = NULL;
  char *invite = NULL, *tm = NULL, *chname = NULL, *from = NULL;

  if (dcc[idx].status & STAT_SHARE) {
    invite = newsplit(&par);
    tm = newsplit(&par);
    chname = newsplit(&par);
    chan = findchan_by_dname(chname);
    fr.match = (FR_CHAN | FR_BOT);
    get_user_flagrec(dcc[idx].user,&fr,chname);
    if (!chan || !channel_shared(chan) ||
	!(bot_chan(fr) || bot_global(fr)))
      putlog(LOG_CMDS, "*",
	     "Channel invite %s on %s rejected - channel not shared.",
	     invite, chname);
    else {
      shareout_but(chan,idx, "+invc %s %s %s %s\n", invite, tm, chname, par);
      str_unescape(invite, '\\');
      from = newsplit(&par);
      if (strchr(from,'s'))
	flags |= MASKREC_STICKY;
      if (strchr(from,'p'))
	flags |= MASKREC_PERM;
      from = newsplit(&par);
      putlog(LOG_CMDS, "@", "%s: invite %s on %s (%s:%s)", dcc[idx].nick,
	     invite, chname, from, par);
      noshare = 1;
      expire_time = (time_t) atoi(tm);
      if (expire_time != 0L)
	expire_time += now;
      u_addinvite(chan, invite, from, par, expire_time,flags);
      noshare = 0;
    }
  }
}

/* +i <host> +<seconds-left> <from> <note>
 */
static void share_pls_ignore(int idx, char *par)
{
  time_t expire_time;
  char *ign = NULL, *from = NULL, *ts = NULL;

  if (dcc[idx].status & STAT_SHARE) {
    shareout_but(NULL, idx, "+i %s\n", par);
    noshare = 1;
    ign = newsplit(&par);
    str_unescape(ign, '\\');
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
    putlog(LOG_CMDS, "@", "%s: ignore %s (%s: %s)",
	   dcc[idx].nick, ign, from, par);
    addignore(ign, from, par, expire_time);
    noshare = 0;
  }
}

static void share_ufno(int idx, char *par)
{
  putlog(LOG_BOTS, "@", "User file rejected by %s: %s",
	 dcc[idx].nick, par);
  dcc[idx].status &= ~STAT_OFFERED;
  if (!(dcc[idx].status & STAT_GETTING))
    dcc[idx].status &= ~(STAT_SHARE | STAT_AGGRESSIVE);
}

static void share_ufyes(int idx, char *par)
{
  if (dcc[idx].status & STAT_OFFERED) {
    dcc[idx].status &= ~STAT_OFFERED;
    dcc[idx].status |= STAT_SHARE;
    dcc[idx].status |= STAT_SENDING;
    uf_features_parse(idx, par);
    lower_bot_linked(idx);

    start_sending_users(idx);
    putlog(LOG_BOTS, "@", "Sending user file send request to %s",
	   dcc[idx].nick);
  }
}

static void share_userfileq(int idx, char *par)
{
  int ok = 1, i;

  flush_tbuf(dcc[idx].nick);

  if (bot_aggressive_to(dcc[idx].user)) {
    putlog(LOG_ERRORS, "*", STR("%s offered user transfer - I'm supposed to be aggressive to it"), dcc[idx].nick);
    dprintf(idx, STR("s un I have you marked for Agressive sharing.\n"));
    botunlink(-2, dcc[idx].nick, STR("I'm aggressive to you"));
  }
  else {
    for (i = 0; i < dcc_total; i++)
      if (dcc[i].type->flags & DCT_BOT) {
	if ((dcc[i].status & STAT_SHARE) &&
	    (dcc[i].status & STAT_AGGRESSIVE) && (i != idx)) {
	  ok = 0;
	  break;
	}
      }
    if (!ok)
      dprintf(idx, "s un Already sharing.\n");
    else {
      dprintf(idx, "s uy %s\n", uf_features_dump(idx));
      /* Set stat-getting to astatic void race condition (robey 23jun1996) */
      dcc[idx].status |= STAT_SHARE | STAT_GETTING | STAT_AGGRESSIVE;
#ifdef HUB
      putlog(LOG_BOTS, "@", "Downloading user file from %s", dcc[idx].nick);
#else /* !HUB */
      putlog(LOG_BOTS, "@", "Downloading user file via uplink.");
#endif /* HUB */
    }
  }
}

/* us <ip> <port> <length>
 */
static void share_ufsend(int idx, char *par)
{
  char *ip = NULL, *port = NULL;
  char s[1024] = "";
  int i, sock;
  FILE *f = NULL;

  egg_snprintf(s, sizeof s, "%s.share.%s.%lu.users", tempdir, conf.bot->nick, now);
  if (!(b_status(idx) & STAT_SHARE)) {
    dprintf(idx, "s e You didn't ask; you just started sending.\n");
    dprintf(idx, "s e Ask before sending the userfile.\n");
    zapfbot(idx);
  } else if (dcc_total == max_dcc) {
    putlog(LOG_MISC, "@", "NO MORE DCC CONNECTIONS -- can't grab userfile");
    dprintf(idx, "s e I can't open a DCC to you; I'm full.\n");
    zapfbot(idx);
  } else if (!(f = fopen(s, "wb"))) {
    putlog(LOG_MISC, "@", "CAN'T WRITE USERFILE DOWNLOAD FILE!");
    zapfbot(idx);
  } else {
    ip = newsplit(&par);
    port = newsplit(&par);
#ifdef USE_IPV6
    sock = getsock(SOCK_BINARY, hostprotocol(ip)); /* Don't buffer this -> mark binary. */
#else
    sock = getsock(SOCK_BINARY); /* Don't buffer this -> mark binary. */
#endif /* USE_IPV6 */
    if (sock < 0 || open_telnet_dcc(sock, ip, port) < 0) {
      killsock(sock);
      putlog(LOG_BOTS, "@", "Asynchronous connection failed!");
      dprintf(idx, "s e Can't connect to you!\n");
      zapfbot(idx);
    } else {
      putlog(LOG_DEBUG, "@", "Connecting to %s:%s for userfile.", ip, port);
      i = new_dcc(&DCC_FORK_SEND, sizeof(struct xfer_info));
      dcc[i].addr = my_atoul(ip);
      dcc[i].port = atoi(port);
      strcpy(dcc[i].nick, "*users");
      dcc[i].u.xfer->filename = strdup(s);
      dcc[i].u.xfer->origname = dcc[i].u.xfer->filename;
      dcc[i].u.xfer->length = atoi(par);
      dcc[i].u.xfer->f = f;
      dcc[i].sock = sock;
      strcpy(dcc[i].host, dcc[idx].nick);

      dcc[idx].status |= STAT_GETTING;
    }
  }
}

static void share_resyncq(int idx, char *par)
{
  if (!allow_resync)
    dprintf(idx, "s rn Not permitting resync.\n");
  else {
    if (can_resync(dcc[idx].nick)) {
      dprintf(idx, STR("s r!\n"));
      dump_resync(idx);
      dcc[idx].status &= ~STAT_OFFERED;
      dcc[idx].status |= STAT_SHARE;
      putlog(LOG_BOTS, "*", STR("Resync'd user file with %s"), dcc[idx].nick);
      updatebot(-1, dcc[idx].nick, '+', 0);
    } else if (!bot_aggressive_to(dcc[idx].user)) {
      dprintf(idx, STR("s r!\n"));
      dcc[idx].status &= ~STAT_OFFERED;
      dcc[idx].status |= STAT_SHARE;
      updatebot(-1, dcc[idx].nick, '+', 0);
      putlog(LOG_BOTS, "@", STR("Resyncing user file from %s"), dcc[idx].nick);
    } else
      dprintf(idx, STR("s rn No resync buffer.\n"));
  }
}

static void share_resync(int idx, char *par)
{
  if ((dcc[idx].status & STAT_OFFERED) && can_resync(dcc[idx].nick)) {
    dump_resync(idx);
    dcc[idx].status &= ~STAT_OFFERED;
    dcc[idx].status |= STAT_SHARE;
    updatebot(-1, dcc[idx].nick, '+', 0);
    putlog(LOG_BOTS, "@", "Resync'd user file with %s", dcc[idx].nick);
  }
}

static void share_resync_no(int idx, char *par)
{
  putlog(LOG_BOTS, "@", "Resync refused by %s: %s", dcc[idx].nick, par);
  flush_tbuf(dcc[idx].nick);
  dprintf(idx, "s u?\n");
}

static void share_version(int idx, char *par)
{
  /* Cleanup any share flags */
  dcc[idx].status &= ~(STAT_SHARE | STAT_GETTING | STAT_SENDING |
		       STAT_OFFERED | STAT_AGGRESSIVE);
  dcc[idx].u.bot->uff_flags = 0;
  if (bot_aggressive_to(dcc[idx].user)) {
    if (can_resync(dcc[idx].nick))
      dprintf(idx, "s r?\n");
    else
      dprintf(idx, "s u?\n");
    dcc[idx].status |= STAT_OFFERED;
  } else
    higher_bot_linked(idx);
}

static void hook_read_userfile()
{
  int i;

  if (!noshare) {
    for (i = 0; i < dcc_total; i++)
      if ((dcc[i].type->flags & DCT_BOT) && (dcc[i].status & STAT_SHARE) &&
	  !(dcc[i].status & STAT_AGGRESSIVE) &&
          (1)) { 
	/* Cancel any existing transfers */
	if (dcc[i].status & STAT_SENDING)
	  cancel_user_xfer(-i, 0);
	dprintf(i, "s u?\n");
	dcc[i].status |= STAT_OFFERED;
      }
  }
}

static void share_endstartup(int idx, char *par)
{
  dcc[idx].status &= ~STAT_GETTING;
  /* Send to any other sharebots */
  hook_read_userfile();
}

static void share_end(int idx, char *par)
{
  putlog(LOG_BOTS, "@", "Ending sharing with %s (%s).", dcc[idx].nick, par);
  cancel_user_xfer(-idx, 0);
  dcc[idx].status &= ~(STAT_SHARE | STAT_GETTING | STAT_SENDING |
		       STAT_OFFERED | STAT_AGGRESSIVE);
  dcc[idx].u.bot->uff_flags = 0;
}

static void share_feats(int idx, char *par)
{
  (int) uf_features_check(idx, par);
}


/* Note: these MUST be sorted. */
static botcmd_t C_share[] =
{
  {"!",		(Function) share_endstartup},
  {"+b",	(Function) share_pls_ban},
  {"+bc",	(Function) share_pls_banchan},
  {"+bh",	(Function) share_pls_bothost},
  {"+cr",	(Function) share_pls_chrec},
  {"+e",	(Function) share_pls_exempt},
  {"+ec",	(Function) share_pls_exemptchan},
  {"+h",	(Function) share_pls_host},
  {"+i",	(Function) share_pls_ignore},
  {"+inv",	(Function) share_pls_invite},
  {"+invc",	(Function) share_pls_invitechan},
  {"-b",	(Function) share_mns_ban},
  {"-bc",	(Function) share_mns_banchan},
  {"-cr",	(Function) share_mns_chrec},
  {"-e",	(Function) share_mns_exempt},
  {"-ec",	(Function) share_mns_exemptchan},
  {"-h",	(Function) share_mns_host},
  {"-i",	(Function) share_mns_ignore},
  {"-inv",	(Function) share_mns_invite},
  {"-invc",	(Function) share_mns_invitechan},
  {"a",		(Function) share_chattr},
  {"c",		(Function) share_change},
  {"chchinfo",	(Function) share_chchinfo},
  {"e",		(Function) share_end},
  {"feats",	(Function) share_feats},
  {"h",		(Function) share_chhand},
  {"k",		(Function) share_killuser},
  {"n",		(Function) share_newuser},
  {"r!",	(Function) share_resync},
  {"r?",	(Function) share_resyncq},
  {"rn",	(Function) share_resync_no},
  {"s",		(Function) share_stick_ban},
  {"se",	(Function) share_stick_exempt},
  {"sInv",	(Function) share_stick_invite},
  {"u?",	(Function) share_userfileq},
  {"un",	(Function) share_ufno},
  {"us",	(Function) share_ufsend},
  {"uy",	(Function) share_ufyes},
  {"v",		(Function) share_version},
  {NULL,	NULL}
};


void sharein(int idx, char *msg)
{
  char *code = NULL;
  int f, i;

  code = newsplit(&msg);
  for (f = 0, i = 0; C_share[i].name && !f; i++) {
    int y = egg_strcasecmp(code, C_share[i].name);

    if (!y)
      /* Found a match */
      (C_share[i].func)(idx, msg);
    if (y < 0)
      f = 1;
  }
}

void shareout(struct chanset_t *chan, ...)
{
  int i, l;
  char *format = NULL, s[601] = "";
  va_list va;

  va_start(va, chan);
  if (!chan || channel_shared(chan)) {
    format = va_arg(va, char *);
    strcpy(s, "s ");
    if ((l = egg_vsnprintf(s + 2, 509, format, va)) < 0)
      s[2 + (l = 509)] = 0;
    for (i = 0; i < dcc_total; i++)
      if ((dcc[i].type->flags & DCT_BOT) &&
	  (dcc[i].status & STAT_SHARE) &&
	  !(dcc[i].status & (STAT_GETTING | STAT_SENDING))) {
	if (chan) {
	  fr.match = (FR_CHAN | FR_BOT);
	  get_user_flagrec(dcc[i].user, &fr, chan->dname);
	}
	if (!chan || bot_chan(fr) || bot_global(fr))
	  tputs(dcc[i].sock, s, l + 2);
      }
    q_resync(s, chan);
  }
  va_end(va);
}

static void shareout_but (struct chanset_t *chan, ...)
{
  int i, x, l;
  char *format = NULL, s[601] = "";
  va_list va;

  va_start(va, chan);
  x = va_arg(va, int);
  format = va_arg(va, char *);

  strcpy(s, "s ");
  if ((l = egg_vsnprintf(s + 2, 509, format, va)) < 0)
    s[2 + (l = 509)] = 0;
  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type->flags & DCT_BOT) && (i != x) &&
	(dcc[i].status & STAT_SHARE) &&
	(!(dcc[i].status & STAT_GETTING)) &&
	(!(dcc[i].status & STAT_SENDING))) {
      if (chan) {
	fr.match = (FR_CHAN | FR_BOT);
	get_user_flagrec(dcc[i].user, &fr, chan->dname);
      }
      if (!chan || bot_chan(fr) || bot_global(fr))
	tputs(dcc[i].sock, s, l + 2);
    }
  q_resync(s, chan);
  va_end(va);
}


/*
 *    Resync buffers
 */

/* Create a tandem buffer for 'bot'.
 */
static void new_tbuf(char *bot)
{
  tandbuf **old = &tbuf, *new = NULL;

  new = calloc(1, sizeof(tandbuf));
  strcpy(new->bot, bot);
  new->q = NULL;
  new->timer = now;
  new->next = *old;
  *old = new;
      putlog(LOG_BOTS, "@", "Creating resync buffer for %s", bot);
}

static void del_tbuf(tandbuf *goner)
{
  struct share_msgq *q = NULL, *r = NULL;
  tandbuf *t = NULL, *old = NULL;

  for (t = tbuf; t; old = t, t = t->next) {
    if (t == goner) {
      if (old)
        old->next = t->next;
      else
        tbuf = t->next;
      for (q = t->q; q && q->msg[0]; q = r) {
	r = q->next;
	free(q->msg);
	free(q);
      }
      free(t);
      break;
    }
  }
}

/* Flush a certain bot's tbuf.
 */
static int flush_tbuf(char *bot)
{
  tandbuf *t = NULL, *tnext = NULL;

  for (t = tbuf; t; t = tnext) {
    tnext = t->next;
    if (!egg_strcasecmp(t->bot, bot)) {
      del_tbuf(t);
      return 1;
    }
  }
  return 0;
}

/* Flush all tbufs older than 15 minutes.
 */
static void check_expired_tbufs()
{
  int i;
  tandbuf *t = NULL, *tnext = NULL;

  for (t = tbuf; t; t = tnext) {
    tnext = t->next;
    if ((now - t->timer) > resync_time) {
	putlog(LOG_BOTS, "@", "Flushing resync buffer for clonebot %s.",
	t->bot);
      del_tbuf(t);
    }
  }
  /* Resend userfile requests */
  for (i = 0; i < dcc_total; i++)
    if (dcc[i].type->flags & DCT_BOT) {
      if (dcc[i].status & STAT_OFFERED) {
	if (now - dcc[i].timeval > 120) {
	  if (dcc[i].user && bot_aggressive_to(dcc[i].user))
	    dprintf(i, "s u?\n");
	  /* ^ send it again in case they missed it */
	}
	/* If it's a share bot that hasnt been sharing, ask again */
      } else if (!(dcc[i].status & STAT_SHARE)) {
	if (dcc[i].user && bot_aggressive_to(dcc[i].user)) {
	  dprintf(i, "s u?\n");
          dcc[i].status |= STAT_OFFERED;
        }
      }
    }
}

static struct share_msgq *q_addmsg(struct share_msgq *qq,
				   struct chanset_t *chan, char *s)
{
  struct share_msgq *q = NULL;
  int cnt;

  if (!qq) {
    q = (struct share_msgq *) calloc(1, sizeof(struct share_msgq));

    q->chan = chan;
    q->next = NULL;
    q->msg = strdup(s);
    return q;
  }
  cnt = 0;
  for (q = qq; q->next; q = q->next)
    cnt++;
  if (cnt > 1000)
    return NULL;		/* Return null: did not alter queue */
  q->next = (struct share_msgq *) calloc(1, sizeof(struct share_msgq));

  q = q->next;
  q->chan = chan;
  q->next = NULL;
  q->msg = strdup(s);
  return qq;
}

/* Add stuff to a specific bot's tbuf.
 */
static void q_tbuf(char *bot, char *s, struct chanset_t *chan)
{
  struct share_msgq *q = NULL;
  tandbuf *t = NULL;

  for (t = tbuf; t && t->bot[0]; t = t->next)
    if (!egg_strcasecmp(t->bot, bot)) {
      if (chan) {
	fr.match = (FR_CHAN | FR_BOT);
	get_user_flagrec(get_user_by_handle(userlist, bot), &fr, chan->dname);
      }
      if ((!chan || bot_chan(fr) || bot_global(fr))
	  && (q = q_addmsg(t->q, chan, s)))
	t->q = q;
      break;
    }
}

/* Add stuff to the resync buffers.
 */
static void q_resync(char *s, struct chanset_t *chan)
{
  struct share_msgq *q = NULL;
  tandbuf *t = NULL;

  for (t = tbuf; t && t->bot[0]; t = t->next) {
      if (chan) {
	fr.match = (FR_CHAN | FR_BOT);
      get_user_flagrec(get_user_by_handle(userlist, t->bot), &fr,
	chan->dname);
      }
      if ((!chan || bot_chan(fr) || bot_global(fr))
	  && (q = q_addmsg(t->q, chan, s)))
      t->q = q;
    }
}

/* Is bot in resync list?
 */
static int can_resync(char *bot)
{
  tandbuf *t = NULL;

  for (t = tbuf; t && t->bot[0]; t = t->next)
    if (!egg_strcasecmp(bot, t->bot))
      return 1;
  return 0;
}

/* Dump the resync buffer for a bot.
 */
static void dump_resync(int idx)
{
  struct share_msgq *q = NULL;
  tandbuf *t = NULL;

  for (t = tbuf; t && t->bot[0]; t = t->next)
    if (!egg_strcasecmp(dcc[idx].nick, t->bot)) {
      for (q = t->q; q && q->msg[0]; q = q->next) {
	dprintf(idx, "%s", q->msg);
      }
      flush_tbuf(dcc[idx].nick);
      break;
    }
}

/* Give status report on tbufs.
 */
static void status_tbufs(int idx)
{
  int count, off = 0;
  struct share_msgq *q = NULL;
  char s[121] = "";
  tandbuf *t = NULL;

  off = 0;
  for (t = tbuf; t && t->bot[0]; t = t->next) 
    if (off < (110 - HANDLEN)) {
      off += my_strcpy(s + off, t->bot);
      count = 0;
      for (q = t->q; q; q = q->next)
	count++;
      off += simple_sprintf(s + off, " (%d), ", count);
    }
  if (off) {
    s[off - 2] = 0;
    dprintf(idx, "Pending sharebot buffers: %s\n", s);
  }
}

static int write_tmp_userfile(char *fn, struct userrec *bu, int idx)
{
  FILE *f = NULL;
  struct userrec *u = NULL;
  int ok = 0;

  if ((f = fopen(fn, "wb"))) {
    chmod(fn, 0600);		/* make it -rw------- */
    lfprintf(f, "#4v: %s -- %s -- transmit\n", ver, conf.bot->nick);
    ok = 1;
    if (!write_chans(f, idx))
     ok = 0;
    if (!write_bans(f, idx))
     ok = 0;
    if (!write_config(f, idx))
      ok = 0;
    /* Only share with bots which support exempts and invites.
     *
     * If UFF is supported, we also check the UFF flags before sharing. If
     * UFF isn't supported, but +I/+e is supported, we just share.
     */
    if ((dcc[idx].u.bot->uff_flags & UFF_EXEMPT)) {
      if (!write_exempts(f, idx)) ok = 0;
    }
    if ((dcc[idx].u.bot->uff_flags & UFF_INVITE)) {
      if (!write_invites(f,idx)) ok = 0;
    }
    for (u = bu; u && ok; u = u->next) {
      if (!write_user(u, f, idx)) {
       ok = 0;
      }
    }
    fclose(f);
  }
  if (!ok)
    putlog(LOG_MISC, "@", USERF_ERRWRITE2);
  return ok;
}


/* Create a copy of the entire userlist (for sending user lists to clone
 * bots) -- userlist is reversed in the process, which is OK because the
 * receiving bot reverses the list AGAIN when saving.
 *
 * t = 0:   copy everything BUT tandem-bots
 * t = 1:   copy only tandem-bots
 * t = 2;   copy all entries
 */
static struct userrec *dup_userlist(int t)
{
  struct userrec *u = NULL, *u1 = NULL, *retu = NULL, *nu = NULL;
  struct chanuserrec *ch = NULL;
  struct user_entry *ue = NULL;
  char *p = NULL;

  nu = retu = NULL;
  noshare = 1;
  for (u = userlist; u; u = u->next)
    /* Only copying non-bot entries? */
    if (((t == 0) && !(u->flags & (USER_BOT | USER_UNSHARED))) ||
	/* ... or only copying bot entries? */
	((t == 1) && (u->flags & (USER_BOT | USER_UNSHARED))) ||
	/* ... or copying everything? */
	(t == 2)) {
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
	if (ue->name) {
	  struct list_type *lt;
	  struct user_entry *nue;

	  nue = calloc(1, sizeof(struct user_entry));
	  nue->type = NULL;
	  nue->u.list = NULL;
          nue->name = strdup(ue->name);
	  list_insert((&nu->entries), nue);
	  for (lt = ue->u.list; lt; lt = lt->next) {
	    struct list_type *list;

	    list = calloc(1, sizeof(struct list_type));
	    list->next = NULL;
            list->extra = strdup(lt->extra);
	    list_append((&nue->u.list), list);
	  }
	} else {
	  if (ue->type->dup_user && (t || ue->type->got_share))
	    ue->type->dup_user(nu, u, ue);
	}
      }
    }
  noshare = 0;
  return retu;
}

/* Erase old user list, switch to new one.
 */
static void finish_share(int idx)
{
  struct userrec *u = NULL, *ou = NULL;
  struct chanset_t *chan = NULL;
  int i, j = -1;

  for (i = 0; i < dcc_total; i++)
    if (!egg_strcasecmp(dcc[i].nick, dcc[idx].host) &&
	(dcc[i].type->flags & DCT_BOT))
      j = i;
  if (j == -1)
    return;

  if (!uff_call_receiving(j, dcc[idx].u.xfer->filename)) {
    char xx[1024] = "";

    putlog(LOG_BOTS, "*", "A uff parsing function failed for the userfile!");
    unlink(dcc[idx].u.xfer->filename);

    dprintf(j, "bye\n");
    egg_snprintf(xx, sizeof xx, "Disconnected %s (uff error)", dcc[j].nick);
    botnet_send_unlinked(j, dcc[j].nick, xx);
    chatout("*** %s\n", xx);

    killsock(dcc[j].sock);
    lostdcc(j);

    return;
  }

  if (dcc[j].u.bot->uff_flags & UFF_OVERRIDE)
    debug1("NOTE: Sharing passively with %s, overriding local bots.", dcc[j].nick);
  else
    /* Copy the bots over. The entries will be used in the new user list. */
    u = dup_userlist(1);

  /*
   * This is where we remove all global and channel bans/exempts/invites and
   * ignores since they will be replaced by what our hub gives us.
   */

  noshare = 1;
  fr.match = (FR_CHAN | FR_BOT);
  while (global_bans)
    u_delban(NULL, global_bans->mask, 1);
  while (global_ign)
    delignore(global_ign->igmask);
  while (global_invites)
    u_delinvite(NULL, global_invites->mask, 1);
  while (global_exempts)
    u_delexempt(NULL, global_exempts->mask, 1);
  for (chan = chanset; chan; chan = chan->next)
    if (channel_shared(chan)) {
      get_user_flagrec(dcc[j].user, &fr, chan->dname);
      if (bot_chan(fr) || bot_global(fr)) {
	while (chan->bans)
	  u_delban(chan, chan->bans->mask, 1);
	while (chan->exempts)
	  u_delexempt(chan, chan->exempts->mask, 1);
	while (chan->invites)
	  u_delinvite(chan, chan->invites->mask, 1);
      }
    }
  noshare = 0;
  ou = userlist;		/* Save old user list			*/
  userlist = (void *) -1;	/* Do this to prevent .user messups	*/

  /* Bot user pointers are updated to point to the new list, all others
   * are set to NULL. If our userfile will be overriden, just set _all_
   * to NULL directly.
   */
  if (u == NULL)
    for (i = 0; i < dcc_total; i++)
      dcc[i].user = NULL;
  else
    for (i = 0; i < dcc_total; i++)
      dcc[i].user = get_user_by_handle(u, dcc[i].nick);
  
  conf.bot->u = NULL;

  /* Read the transferred userfile. Add entries to u, which already holds
   * the bot entries in non-override mode.
   */
  loading = 1;
  checkchans(0); /* flag all the channels.. */
Context;
  if (!readuserfile(dcc[idx].u.xfer->filename, &u)) {
    char xx[1024] = "";

Context;
    unlink(dcc[idx].u.xfer->filename); /* why the fuck was this not here, stupid eggdev team. */
    loading = 0;
    putlog(LOG_MISC, "*", "%s", USERF_CANTREAD);
    clear_userlist(u);		/* Clear new, obsolete, user list.	*/
    clear_chanlist();		/* Remove all user references from the
				   channel lists.			*/
    for (i = 0; i < dcc_total; i++)
      dcc[i].user = get_user_by_handle(ou, dcc[i].nick);
    
    conf.bot->u = get_user_by_handle(ou, conf.bot->nick);

    userlist = ou;		/* Revert to old user list.		*/
    lastuser = NULL;		/* Reset last accessed user ptr.	*/
    checkchans(2); 		/* un-flag the channels, we are keeping them.. */

    dprintf(idx, "bye\n");
    egg_snprintf(xx, sizeof xx, "Disconnected %s (can't read userfile)", dcc[j].nick);
    botnet_send_unlinked(j, dcc[j].nick, xx);
    chatout("*** %s\n", xx);

    killsock(dcc[j].sock);
    lostdcc(j);

    return;
  }
  unlink(dcc[idx].u.xfer->filename); //I mean really, shit fills up the quota fast.

  loading = 0;
  putlog(LOG_BOTS, "@", "%s.", USERF_XFERDONE);

  clear_chanlist();		/* Remove all user references from the
				   channel lists.			*/
  userlist = u;			/* Set new user list.			*/
  lastuser = NULL;		/* Reset last accessed user ptr.	*/

  /*
   * Migrate:
   *   - old channel flags over (unshared channels see)
   *   - unshared (got_share == 0) user entries
   *   - old bot flags and passwords
   */
  fr.match = (FR_CHAN | FR_BOT);
  for (u = userlist; u; u = u->next) {
    struct userrec *u2 = get_user_by_handle(ou, u->handle);

    if ((dcc[j].u.bot->uff_flags & UFF_OVERRIDE) &&
	u2 && (u2->flags & USER_BOT)) {
      /* We knew this bot before, copy flags and the password back over. */
      set_user(&USERENTRY_BOTFL, u, get_user(&USERENTRY_BOTFL, u2));
      set_user(&USERENTRY_PASS, u, get_user(&USERENTRY_PASS, u2));
    } else if ((dcc[j].u.bot->uff_flags & UFF_OVERRIDE) &&
	       (u->flags & USER_BOT)) {
      /* This bot was unknown to us, reset it's flags and password. */
      set_user(&USERENTRY_BOTFL, u, NULL);
      set_user(&USERENTRY_PASS, u, NULL);
    } else if (u2 && !(u2->flags & (USER_BOT | USER_UNSHARED))) {
      struct chanuserrec *cr, *cr_next, *cr_old = NULL;
      struct user_entry *ue;

      if (private_global) {
	u->flags = u2->flags;
	u->flags_udef = u2->flags_udef;
      } else {
	int pgbm = private_globals_bitmask();

	u->flags = (u2->flags & pgbm) | (u->flags & ~pgbm);
      }
      noshare = 1;
      for (cr = u2->chanrec; cr; cr = cr_next) {
	struct chanset_t *mychan = findchan_by_dname(cr->channel);

	cr_next = cr->next;
	if (mychan) {
	  int not_shared = 0;

	  if (!channel_shared(mychan))
	    not_shared = 1;
	  else {
	    get_user_flagrec(dcc[j].user, &fr, mychan->dname);
	    if (!bot_chan(fr) && !bot_global(fr))
	      not_shared = 1;
	  }
	  if (not_shared) {
	    del_chanrec(u, cr->channel);
	    if (cr_old)
	      cr_old->next = cr_next;
	    else
	      u2->chanrec = cr_next;
	    cr->next = u->chanrec;
	    u->chanrec = cr;
	  } else {
	    /* Shared channel, still keep old laston time */
	    for (cr_old = u->chanrec; cr_old; cr_old = cr_old->next)
	      if (!rfc_casecmp(cr_old->channel, cr->channel)) {
		cr_old->laston = cr->laston;
		break;
	      }
	    cr_old = cr;
	  }
	}
      }
      noshare = 0;
      /* Any unshared user entries need copying over */
      for (ue = u2->entries; ue; ue = ue->next)
	if (ue->type && !ue->type->got_share && ue->type->dup_user)
	  ue->type->dup_user(u, u2, ue);
    } else if (!u2 && private_global) {
      u->flags = 0;
      u->flags_udef = 0;
    } else
      u->flags = (u->flags & ~private_globals_bitmask());
  }
  clear_userlist(ou);

  unlink(dcc[idx].u.xfer->filename);	/* Done with you!		*/

  checkchans(1); 			/* remove marked channels */
  trigger_cfg_changed();
  reaffirm_owners();			/* Make sure my owners are +a	*/
  updatebot(-1, dcc[j].nick, '+', 0);
}

/* Begin the user transfer process.
 */
static void start_sending_users(int idx)
{
  struct userrec *u = NULL;
  char share_file[1024] = "", s1[64] = "";
  int i = 1;
  struct chanuserrec *ch = NULL;
  struct chanset_t *cst = NULL;

  egg_snprintf(share_file, sizeof share_file, "%s.share.%s.%lu", tempdir, dcc[idx].nick, now);
  if (dcc[idx].u.bot->uff_flags & UFF_OVERRIDE) {
    debug1("NOTE: Sharing aggressively with %s, overriding its local bots.",
	   dcc[idx].nick);
    u = dup_userlist(2);		/* All entries		*/
  } else
    u = dup_userlist(0);		/* Only non-bots	*/

  write_tmp_userfile(share_file, u, idx);
  clear_userlist(u);

  if (!uff_call_sending(idx, share_file)) { /* compress! */
    unlink(share_file);
    dprintf(idx, "s e %s\n", "uff parsing failed");
    putlog(LOG_BOTS, "*", "uff parsing failed");
    dcc[idx].status &= ~(STAT_SHARE | STAT_SENDING | STAT_AGGRESSIVE);
    return;
  }

  if ((i = raw_dcc_send(share_file, "*users", "(users)", share_file)) > 0) {
    unlink(share_file);
    dprintf(idx, "s e %s\n", USERF_CANTSEND);
    putlog(LOG_BOTS, "@", "%s -- can't send userfile",
	   i == DCCSEND_FULL   ? "NO MORE DCC CONNECTIONS" :
	   i == DCCSEND_NOSOCK ? "CAN'T OPEN A LISTENING SOCKET" :
	   i == DCCSEND_BADFN  ? "BAD FILE" :
	   i == DCCSEND_FEMPTY ? "EMPTY FILE" : "UNKNOWN REASON!");
    dcc[idx].status &= ~(STAT_SHARE | STAT_SENDING | STAT_AGGRESSIVE);
  } else {
    updatebot(-1, dcc[idx].nick, '+', 0);
    dcc[idx].status |= STAT_SENDING;
    i = dcc_total - 1;
    strcpy(dcc[i].host, dcc[idx].nick);		/* Store bot's nick */
    dprintf(idx, "s us %lu %d %lu\n",
	    iptolong(natip[0] ? (IP) inet_addr(natip) : getmyip()),
	    dcc[i].port, dcc[i].u.xfer->length);
    /* Start up a tbuf to queue outgoing changes for this bot until the
     * userlist is done transferring.
     */
    new_tbuf(dcc[idx].nick);
    /* Immediately, queue bot hostmasks & addresses (jump-start) - if we
     * don't override the leaf's local bots.
     */
    if (!(dcc[idx].u.bot->uff_flags & UFF_OVERRIDE)) {
      for (u = userlist; u; u = u->next) {
        if ((u->flags & USER_BOT) && !(u->flags & USER_UNSHARED)) {
	  struct bot_addr *bi = get_user(&USERENTRY_BOTADDR, u);
	  struct list_type *t = NULL;
	  char s2[1024] = "";

	  /* Send hostmasks */
	  for (t = get_user(&USERENTRY_HOSTS, u); t; t = t->next) {
	    egg_snprintf(s2, sizeof s2, "s +bh %s %s\n", u->handle, t->extra);
	    q_tbuf(dcc[idx].nick, s2, NULL);
	  }
	  /* Send address */
	  if (bi)
	    egg_snprintf(s2, sizeof s2, "s c BOTADDR %s %s %d %d\n", u->handle,
			 bi->address, bi->telnet_port, bi->relay_port);
	  q_tbuf(dcc[idx].nick, s2, NULL);
	  fr.match = FR_GLOBAL;
	  fr.global = u->flags;

	  fr.udef_global = u->flags_udef;
	  build_flags(s1, &fr, NULL);
	  egg_snprintf(s2, sizeof s2, "s a %s %s\n", u->handle, s1);
	  q_tbuf(dcc[idx].nick, s2, NULL);
	  for (ch = u->chanrec; ch; ch = ch->next) {
	    if ((ch->flags & ~BOT_SHARE) &&
		((cst = findchan_by_dname(ch->channel)) &&
		 channel_shared(cst))) {
	      fr.match = (FR_CHAN | FR_BOT);
	      get_user_flagrec(dcc[idx].user, &fr, ch->channel);
	      if (bot_chan(fr) || bot_global(fr)) {
	        fr.match = FR_CHAN;
	        fr.chan = ch->flags & ~BOT_SHARE;
	        fr.udef_chan = ch->flags_udef;
	        build_flags(s1, &fr, NULL);
	        egg_snprintf(s2, sizeof s2, "s a %s %s %s\n", u->handle, s1,
			     ch->channel);
	        q_tbuf(dcc[idx].nick, s2, cst);
	      }
	    }
	  }
        }
      }
    }
    q_tbuf(dcc[idx].nick, "s !\n", NULL);
    /* Unlink the file. We don't really care whether this causes problems
     * for NFS setups. It's not worth the trouble.
     */
    unlink(share_file);
  }
}

static void (*def_dcc_bot_kill) (int, void *) = 0;

static void cancel_user_xfer(int idx, void *x)
{
  int i, j, k = 0;

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
	if (!egg_strcasecmp(dcc[i].host, dcc[idx].nick) &&
	    ((dcc[i].type->flags & (DCT_FILETRAN | DCT_FILESEND)) ==
	     (DCT_FILETRAN | DCT_FILESEND)))
	  j = i;
      if (j != 0) {
	killsock(dcc[j].sock);
	unlink(dcc[j].u.xfer->filename);
	lostdcc(j);
      }
      putlog(LOG_BOTS, "@", "(Userlist download aborted.)");
    }
    if (dcc[idx].status & STAT_SENDING) {
      j = 0;
      for (i = 0; i < dcc_total; i++)
	if ((!egg_strcasecmp(dcc[i].host, dcc[idx].nick)) &&
	    ((dcc[i].type->flags & (DCT_FILETRAN | DCT_FILESEND))
	     == DCT_FILETRAN))
	  j = i;
      if (j != 0) {
	killsock(dcc[j].sock);
	unlink(dcc[j].u.xfer->filename);
	lostdcc(j);
      }
      putlog(LOG_BOTS, "@", "(Userlist transmit aborted.)");
    }
    if (allow_resync && (!(dcc[idx].status & STAT_GETTING)) &&
	(!(dcc[idx].status & STAT_SENDING)))
      new_tbuf(dcc[idx].nick);
  }
  if (!k)
    def_dcc_bot_kill(idx, x);
}

static void share_report(int idx, int details)
{
  int i, j;

  if (details) {
    dprintf(idx, "    Private owners: %3s   Allow resync: %3s\n",
	    (private_global || (private_globals_bitmask() & USER_OWNER)) ?
	    "yes" : "no", allow_resync ? "yes" : "no");
    for (i = 0; i < dcc_total; i++)
      if (dcc[i].type == &DCC_BOT) {
	if (dcc[i].status & STAT_GETTING) {
	  int ok = 0;

	  for (j = 0; j < dcc_total; j++)
	    if (((dcc[j].type->flags & (DCT_FILETRAN | DCT_FILESEND))
		 == (DCT_FILETRAN | DCT_FILESEND)) &&
		!egg_strcasecmp(dcc[j].host, dcc[i].nick)) {
	      dprintf(idx, "Downloading userlist from %s (%d%% done)\n",
		      dcc[i].nick,
		      (int) (100.0 * ((float) dcc[j].status) /
			     ((float) dcc[j].u.xfer->length)));
	      ok = 1;
	      break;
	    }
	  if (!ok)
	    dprintf(idx, "Download userlist from %s (negotiating "
		    "botentries)\n", dcc[i].nick);
	} else if (dcc[i].status & STAT_SENDING) {
	  for (j = 0; j < dcc_total; j++) {
	    if (((dcc[j].type->flags & (DCT_FILETRAN | DCT_FILESEND))
		 == DCT_FILETRAN)
		&& !egg_strcasecmp(dcc[j].host, dcc[i].nick)) {
	      if (dcc[j].type == &DCC_GET)
		dprintf(idx, "Sending userlist to %s (%d%% done)\n",
			dcc[i].nick,
			(int) (100.0 * ((float) dcc[j].status) /
			       ((float) dcc[j].u.xfer->length)));
	      else
		dprintf(idx,
			"Sending userlist to %s (waiting for connect)\n",
			dcc[i].nick);
	    }
	  }
	} else if (dcc[i].status & STAT_AGGRESSIVE) {
	  dprintf(idx, "    Passively sharing with %s.\n", dcc[i].nick);
	} else if (dcc[i].status & STAT_SHARE) {
	  dprintf(idx, "    Aggressively sharing with %s.\n", dcc[i].nick);
	}
      }
    status_tbufs(idx);
  }
}

EXPORT_SCOPE char *share_start();

static Function share_table[] =
{
  /* 0 - 3 */
  (Function) share_start,
  (Function) NULL,
  (Function) 0,
  (Function) share_report,
  /* 4 - 7 */
  (Function) finish_share,
  (Function) dump_resync,
  (Function) uff_addtable,
  (Function) uff_deltable
  /* 8 - 11 */
};

char *share_start(Function *global_funcs)
{

  global = global_funcs;

  module_register(MODULE_NAME, share_table, 2, 3);
  if (!(transfer_funcs = module_depend(MODULE_NAME, "transfer", 0, 0))) {
    module_undepend(MODULE_NAME);
    return "This module requires transfer module 2.0 or later.";
  }
  if (!(channels_funcs = module_depend(MODULE_NAME, "channels", 0, 0))) {
    module_undepend(MODULE_NAME);
    return "This module requires channels module 1.0 or later.";
  }
#ifdef LEAF
  if (!(irc_funcs = module_depend(MODULE_NAME, "irc", 0, 0))) {
    module_undepend(MODULE_NAME);
    return "This module requires channels module 1.0 or later.";
  }
#endif /* LEAF */
  timer_create_secs(60, "check_expired_tbufs", (Function) check_expired_tbufs);
  timer_create_secs(1, "check_delay", (Function) check_delay);
  add_hook(HOOK_READ_USERFILE, (Function) hook_read_userfile);
  def_dcc_bot_kill = DCC_BOT.kill;
  DCC_BOT.kill = cancel_user_xfer;
  uff_init();
  uff_addtable(internal_uff_table);
  return NULL;
}

int private_globals_bitmask()
{
  struct flag_record my_fr = {FR_GLOBAL, 0, 0, 0, 0, 0};

  break_down_flags(private_globals, &my_fr, 0);
  return my_fr.global;
}
