/*
 * share.c -- part of share.mod
 *
 */

#include "src/common.h"
#include "src/main.h"
#include "src/rfc1459.h"
#include "src/botmsg.h"
#include "src/misc.h"
#include "src/cmds.h"
#include "src/chanprog.h"
#include "src/users.h"
#include "src/userrec.h"
#include "src/botnet.h"

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
#  include "src/mod/irc.mod/irc.h"
#endif /* LEAF */

static struct flag_record fr = { 0, 0, 0, 0 };

#ifdef LEAF
struct delay_mode {
  struct delay_mode *next;
  struct chanset_t *chan;
  int plsmns;
  int mode;
  char *mask;
  int seconds;
};

static struct delay_mode *start_delay = NULL;
#endif /* LEAF */

/* Prototypes */
#ifdef HUB
static void start_sending_users(int);
#endif /* HUB */
static void shareout_but(struct chanset_t *, ...);
static void cancel_user_xfer(int, void *);

#include "share.h"

/*
 *   Sup's delay code
 */

#ifdef LEAF
static void
add_delay(struct chanset_t *chan, int plsmns, int mode, char *mask)
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

static void
del_delay(struct delay_mode *delay)
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

static void
check_delay()
{
  struct delay_mode *d = NULL, *dnext = NULL;

  for (d = start_delay; d; d = dnext) {
    dnext = d->next;
    if (d->seconds <= now) {
#ifdef LEAF
      add_mode(d->chan, d->plsmns, d->mode, d->mask);
#endif /* LEAF */
      del_delay(d);
    }
  }
}
#endif /* LEAF */

/*
 *   Botnet commands
 */

static void
share_stick_ban(int idx, char *par)
{
  char *host = NULL, *val = NULL;
  int yn;

  if (dcc[idx].status & STAT_SHARE) {
    host = newsplit(&par);
    val = newsplit(&par);
    yn = atoi(val);
    noshare = 1;
    if (!par[0]) {              /* Global ban */
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

      if ((chan != NULL) && (cr = get_chanrec(dcc[idx].user, par)))
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
static void
share_stick_exempt(int idx, char *par)
{
  char *host = NULL, *val = NULL;
  int yn;

  if (dcc[idx].status & STAT_SHARE) {
    host = newsplit(&par);
    val = newsplit(&par);
    yn = atoi(val);
    noshare = 1;
    if (!par[0]) {              /* Global exempt */
      if (u_setsticky_exempt(NULL, host, yn) > 0) {
        putlog(LOG_CMDS, "@", "%s: %s %s", dcc[idx].nick, (yn) ? "stick" : "unstick", host);
        shareout_but(NULL, idx, "se %s %d\n", host, yn);
      }
    } else {
      struct chanset_t *chan = findchan_by_dname(par);
      struct chanuserrec *cr = NULL;

      if ((chan != NULL) && (cr = get_chanrec(dcc[idx].user, par)))
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
static void
share_stick_invite(int idx, char *par)
{
  char *host = NULL, *val = NULL;
  int yn;

  if (dcc[idx].status & STAT_SHARE) {
    host = newsplit(&par);
    val = newsplit(&par);
    yn = atoi(val);
    noshare = 1;
    if (!par[0]) {              /* Global invite */
      if (u_setsticky_invite(NULL, host, yn) > 0) {
        putlog(LOG_CMDS, "@", "%s: %s %s", dcc[idx].nick, (yn) ? "stick" : "unstick", host);
        shareout_but(NULL, idx, "sInv %s %d\n", host, yn);
      }
    } else {
      struct chanset_t *chan = findchan_by_dname(par);
      struct chanuserrec *cr = NULL;

      if ((chan != NULL) && (cr = get_chanrec(dcc[idx].user, par)))
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

static void
share_chhand(int idx, char *par)
{
  char *hand = NULL;
  struct userrec *u = NULL;

  if (dcc[idx].status & STAT_SHARE) {
    hand = newsplit(&par);
    u = get_user_by_handle(userlist, hand);
    if (u) {
      shareout_but(NULL, idx, "h %s %s\n", hand, par);
      noshare = 1;
      if (change_handle(u, par))
        putlog(LOG_CMDS, "@", "%s: handle %s->%s", dcc[idx].nick, hand, par);
      noshare = 0;
    }
  }
}

static void
share_chattr(int idx, char *par)
{
  char *hand = NULL, *atr = NULL, s[100] = "";
  struct chanset_t *cst = NULL;
  struct userrec *u = NULL;
  struct flag_record fr2;
  flag_t bfl, ofl;

  if (dcc[idx].status & STAT_SHARE) {
    hand = newsplit(&par);
    u = get_user_by_handle(userlist, hand);
    if (u) {
      atr = newsplit(&par);
      cst = findchan_by_dname(par);
      if (!par[0] || cst) {
        if (!(dcc[idx].status & STAT_GETTING))
          shareout_but(cst, idx, "a %s %s %s\n", hand, atr, par);
        noshare = 1;
        if (par[0] && cst) {
          fr.match = (FR_CHAN | FR_BOT);
          get_user_flagrec(dcc[idx].user, &fr, par);
          fr.match = FR_CHAN;
          fr2.match = FR_CHAN;
          break_down_flags(atr, &fr, 0);
          set_user_flagrec(u, &fr, par);
          get_user_flagrec(u, &fr2, par);
          check_dcc_chanattrs(u, par, fr.chan, fr2.chan);
          noshare = 0;
          build_flags(s, &fr, 0);
          if (!(dcc[idx].status & STAT_GETTING))
            putlog(LOG_CMDS, "@", "%s: chattr %s %s %s", dcc[idx].nick, hand, s, par);
#ifdef LEAF
          recheck_channel(cst, 0);
#endif /* LEAF */
        } else {
          fr.match = FR_GLOBAL;
          get_user_flagrec(dcc[idx].user, &fr, 0);
          /* Don't let bot flags be altered */
          ofl = fr.global;

          break_down_flags(atr, &fr, 0);
          bfl = u->flags & USER_BOT;

          fr.global = sanity_check(fr.global |bfl);

          set_user_flagrec(u, &fr, 0);
          check_dcc_attrs(u, ofl);
          noshare = 0;
          build_flags(s, &fr, 0);
          fr.match = FR_CHAN;
#ifdef HUB
          if (!(dcc[idx].status & STAT_GETTING))
            putlog(LOG_CMDS, "@", "%s: chattr %s %s", dcc[idx].nick, hand, s);
#endif /* HUB */
#ifdef LEAF
          for (cst = chanset; cst; cst = cst->next)
            recheck_channel(cst, 0);
#endif /* LEAF */
        }
        noshare = 0;
      }
    }
  }
}

static void
share_pls_chrec(int idx, char *par)
{
  char *user = NULL;
  struct chanset_t *chan = NULL;
  struct userrec *u = NULL;

  if (dcc[idx].status & STAT_SHARE) {
    user = newsplit(&par);
    if ((u = get_user_by_handle(userlist, user))) {
      chan = findchan_by_dname(par);
      fr.match = (FR_CHAN | FR_BOT);
      get_user_flagrec(dcc[idx].user, &fr, par);
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

static void
share_mns_chrec(int idx, char *par)
{
  char *user = NULL;
  struct chanset_t *chan = NULL;
  struct userrec *u = NULL;

  if (dcc[idx].status & STAT_SHARE) {
    user = newsplit(&par);
    if ((u = get_user_by_handle(userlist, user))) {
      chan = findchan_by_dname(par);
      fr.match = (FR_CHAN | FR_BOT);
      get_user_flagrec(dcc[idx].user, &fr, par);
      noshare = 1;
      del_chanrec(u, par);
      shareout_but(chan, idx, "-cr %s %s\n", user, par);
      noshare = 0;
      putlog(LOG_CMDS, "@", "%s: -chrec %s %s", dcc[idx].nick, user, par);
    }
  }
}

static void
share_newuser(int idx, char *par)
{
  char *nick = NULL, *host = NULL, *pass = NULL, s[100] = "";
  struct userrec *u = NULL;

  if (dcc[idx].status & STAT_SHARE) {
    nick = newsplit(&par);
    host = newsplit(&par);
    pass = newsplit(&par);

    fr.global = 0;

    fr.match = FR_GLOBAL;
    break_down_flags(par, &fr, NULL);

    /* If user already exists, ignore command */
    shareout_but(NULL, idx, "n %s %s %s %s\n", nick, host, pass, par);

    if (!u) {
      noshare = 1;
      if (strlen(nick) > HANDLEN)
        nick[HANDLEN] = 0;

      fr.match = FR_GLOBAL;
      build_flags(s, &fr, 0);
      userlist = adduser(userlist, nick, host, pass, 0);

      /* Support for userdefinedflag share - drummer */
      u = get_user_by_handle(userlist, nick);
      set_user_flagrec(u, &fr, 0);
      fr.match = FR_CHAN;       /* why?? */
      noshare = 0;
#ifndef LEAF
      putlog(LOG_CMDS, "@", "%s: newuser %s %s", dcc[idx].nick, nick, s);
#endif /* LEAF */
    }
  }
}

static void
share_killuser(int idx, char *par)
{
  /* If user is a share bot, ignore command */
  if (dcc[idx].status & STAT_SHARE) {
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

static void
share_pls_host(int idx, char *par)
{
  char *hand = NULL;
  struct userrec *u = NULL;

  if (dcc[idx].status & STAT_SHARE) {
    hand = newsplit(&par);
    if ((u = get_user_by_handle(userlist, hand))) {
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

static void
share_pls_bothost(int idx, char *par)
{
  char *hand = NULL, p[32] = "";
  struct userrec *u = NULL;

  if (dcc[idx].status & STAT_SHARE) {
    hand = newsplit(&par);

    if (!(dcc[idx].status & STAT_GETTING))
      shareout_but(NULL, idx, "+bh %s %s\n", hand, par);
    /* Add bot to userlist if not there */
    if (u) {
      if (!(u->flags & USER_BOT))
        return;                 /* ignore */
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

static void
share_mns_host(int idx, char *par)
{
  char *hand = NULL;
  struct userrec *u = NULL;

  if (dcc[idx].status & STAT_SHARE) {
    hand = newsplit(&par);
    if ((u = get_user_by_handle(userlist, hand))) {
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

static void
share_change(int idx, char *par)
{
  char *key = NULL, *hand = NULL;
  struct userrec *u = NULL;
  struct user_entry_type *uet = NULL;
  struct user_entry *e = NULL;

  if (dcc[idx].status & STAT_SHARE) {
    key = newsplit(&par);
    hand = newsplit(&par);
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
          list_delete((struct list_type **) &(u->entries), (struct list_type *) e);
          free(e);
        }
      }
      noshare = 0;
    }
  }
}

static void
share_chchinfo(int idx, char *par)
{
  char *hand = NULL, *chan = NULL;
  struct chanset_t *cst = NULL;
  struct userrec *u = NULL;

  if (dcc[idx].status & STAT_SHARE) {
    hand = newsplit(&par);
    if ((u = get_user_by_handle(userlist, hand))) {
      chan = newsplit(&par);
      cst = findchan_by_dname(chan);
      fr.match = (FR_CHAN | FR_BOT);
      get_user_flagrec(dcc[idx].user, &fr, chan);
      shareout_but(cst, idx, "chchinfo %s %s %s\n", hand, chan, par);
      noshare = 1;
      set_handle_chaninfo(userlist, hand, chan, par);
      noshare = 0;
      putlog(LOG_CMDS, "@", "%s: change info %s %s", dcc[idx].nick, chan, hand);
    }
  }
}

static void
share_mns_ban(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    shareout_but(NULL, idx, "-b %s\n", par);
    putlog(LOG_CMDS, "@", "%s: cancel ban %s", dcc[idx].nick, par);
    str_unescape(par, '\\');
    noshare = 1;
    if (u_delmask('b', NULL, par, 1) > 0) {
#ifdef LEAF
      struct chanset_t *chan = NULL;

      for (chan = chanset; chan; chan = chan->next)
        add_delay(chan, '-', 'b', par);
#endif /* LEAF */
    }
    noshare = 0;
  }
}

static void
share_mns_exempt(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    shareout_but(NULL, idx, "-e %s\n", par);
    putlog(LOG_CMDS, "@", "%s: cancel exempt %s", dcc[idx].nick, par);
    str_unescape(par, '\\');
    noshare = 1;
    if (u_delmask('e', NULL, par, 1) > 0) {
#ifdef LEAF
      struct chanset_t *chan = NULL;

      for (chan = chanset; chan; chan = chan->next)
        add_delay(chan, '-', 'e', par);
#endif /* LEAF */
    }
    noshare = 0;
  }
}

static void
share_mns_invite(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    shareout_but(NULL, idx, "-inv %s\n", par);
    putlog(LOG_CMDS, "@", "%s: cancel invite %s", dcc[idx].nick, par);
    str_unescape(par, '\\');
    noshare = 1;
    if (u_delmask('I', NULL, par, 1) > 0) {
#ifdef LEAF
      struct chanset_t *chan = NULL;

      for (chan = chanset; chan; chan = chan->next)
        add_delay(chan, '-', 'I', par);
#endif /* LEAF */
    }
    noshare = 0;
  }
}

static void
share_mns_banchan(int idx, char *par)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  if (dcc[idx].status & STAT_SHARE) {
    chname = newsplit(&par);
    chan = findchan_by_dname(chname);
    fr.match = (FR_CHAN | FR_BOT);
    get_user_flagrec(dcc[idx].user, &fr, chname);
    shareout_but(chan, idx, "-bc %s %s\n", chname, par);
    putlog(LOG_CMDS, "@", "%s: cancel ban %s on %s", dcc[idx].nick, par, chname);
    str_unescape(par, '\\');
    noshare = 1;
#ifdef LEAF
    if (u_delmask('b', chan, par, 1) > 0)
      add_delay(chan, '-', 'b', par);
#endif /* LEAF */
#ifdef HUB
    u_delmask('b', chan, par, 1);
#endif /* HUB */
    noshare = 0;
  }
}

static void
share_mns_exemptchan(int idx, char *par)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  if (dcc[idx].status & STAT_SHARE) {
    chname = newsplit(&par);
    chan = findchan_by_dname(chname);
    fr.match = (FR_CHAN | FR_BOT);
    get_user_flagrec(dcc[idx].user, &fr, chname);
    shareout_but(chan, idx, "-ec %s %s\n", chname, par);
    putlog(LOG_CMDS, "@", "%s: cancel exempt %s on %s", dcc[idx].nick, par, chname);
    str_unescape(par, '\\');
    noshare = 1;
#ifdef LEAF
    if (u_delmask('e', chan, par, 1) > 0)
      add_delay(chan, '-', 'e', par);
#endif /* LEAF */
#ifdef HUB
    u_delmask('e', chan, par, 1);
#endif /* HUB */

    noshare = 0;
  }
}

static void
share_mns_invitechan(int idx, char *par)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  if (dcc[idx].status & STAT_SHARE) {
    chname = newsplit(&par);
    chan = findchan_by_dname(chname);
    fr.match = (FR_CHAN | FR_BOT);
    get_user_flagrec(dcc[idx].user, &fr, chname);
    shareout_but(chan, idx, "-invc %s %s\n", chname, par);
    putlog(LOG_CMDS, "@", "%s: cancel invite %s on %s", dcc[idx].nick, par, chname);
    str_unescape(par, '\\');
    noshare = 1;
#ifdef LEAF
    if (u_delmask('I', chan, par, 1) > 0)
      add_delay(chan, '-', 'I', par);
#endif /* LEAF */
#ifdef HUB
    u_delmask('I', chan, par, 1);
#endif /* HUB */

    noshare = 0;
  }
}

static void
share_mns_ignore(int idx, char *par)
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

static void
share_pls_ban(int idx, char *par)
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
    u_addmask('b', NULL, ban, from, par, expire_time, flags);
    putlog(LOG_CMDS, "@", "%s: global ban %s (%s:%s)", dcc[idx].nick, ban, from, par);
    noshare = 0;
#ifdef LEAF
    for (chan = chanset; chan != NULL; chan = chan->next)
      check_this_ban(chan, ban, 0);
#endif /* LEAF */
  }
}

static void
share_pls_banchan(int idx, char *par)
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
    shareout_but(chan, idx, "+bc %s %s %s %s\n", ban, tm, chname, par);
    str_unescape(ban, '\\');
    from = newsplit(&par);
    if (strchr(from, 's'))
      flags |= MASKREC_STICKY;
    if (strchr(from, 'p'))
      flags |= MASKREC_PERM;
    from = newsplit(&par);
    putlog(LOG_CMDS, "@", "%s: ban %s on %s (%s:%s)", dcc[idx].nick, ban, chname, from, par);
    noshare = 1;
    expire_time = (time_t) atoi(tm);
    if (expire_time != 0L)
      expire_time += now;
    u_addmask('b', chan, ban, from, par, expire_time, flags);
    noshare = 0;
#ifdef LEAF
    check_this_ban(chan, ban, 0);
#endif /* LEAF */
  }
}

/* Same as share_pls_ban, only for exempts.
 */
static void
share_pls_exempt(int idx, char *par)
{
  time_t expire_time;
  char *exempt = NULL, *tm = NULL, *from = NULL;
  int flags = 0;

  if (dcc[idx].status & STAT_SHARE) {
    shareout_but(NULL, idx, "+e %s\n", par);
    noshare = 1;
    exempt = newsplit(&par);
    str_unescape(exempt, '\\');
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
    u_addmask('e', NULL, exempt, from, par, expire_time, flags);
    putlog(LOG_CMDS, "@", "%s: global exempt %s (%s:%s)", dcc[idx].nick, exempt, from, par);
    noshare = 0;
  }
}

/* Same as share_pls_banchan, only for exempts.
 */
static void
share_pls_exemptchan(int idx, char *par)
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
    get_user_flagrec(dcc[idx].user, &fr, chname);
    shareout_but(chan, idx, "+ec %s %s %s %s\n", exempt, tm, chname, par);
    str_unescape(exempt, '\\');
    from = newsplit(&par);
    if (strchr(from, 's'))
      flags |= MASKREC_STICKY;
    if (strchr(from, 'p'))
      flags |= MASKREC_PERM;
    from = newsplit(&par);
    putlog(LOG_CMDS, "@", "%s: exempt %s on %s (%s:%s)", dcc[idx].nick, exempt, chname, from, par);
    noshare = 1;
    expire_time = (time_t) atoi(tm);
    if (expire_time != 0L)
      expire_time += now;
    u_addmask('e', chan, exempt, from, par, expire_time, flags);
    noshare = 0;
  }
}

/* Same as share_pls_ban, only for invites.
 */
static void
share_pls_invite(int idx, char *par)
{
  time_t expire_time;
  char *invite = NULL, *tm = NULL, *from = NULL;
  int flags = 0;

  if (dcc[idx].status & STAT_SHARE) {
    shareout_but(NULL, idx, "+inv %s\n", par);
    noshare = 1;
    invite = newsplit(&par);
    str_unescape(invite, '\\');
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
    u_addmask('I', NULL, invite, from, par, expire_time, flags);
    putlog(LOG_CMDS, "@", "%s: global invite %s (%s:%s)", dcc[idx].nick, invite, from, par);
    noshare = 0;
  }
}

/* Same as share_pls_banchan, only for invites.
 */
static void
share_pls_invitechan(int idx, char *par)
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
    get_user_flagrec(dcc[idx].user, &fr, chname);
    shareout_but(chan, idx, "+invc %s %s %s %s\n", invite, tm, chname, par);
    str_unescape(invite, '\\');
    from = newsplit(&par);
    if (strchr(from, 's'))
      flags |= MASKREC_STICKY;
    if (strchr(from, 'p'))
      flags |= MASKREC_PERM;
    from = newsplit(&par);
    putlog(LOG_CMDS, "@", "%s: invite %s on %s (%s:%s)", dcc[idx].nick, invite, chname, from, par);
    noshare = 1;
    expire_time = (time_t) atoi(tm);
    if (expire_time != 0L)
      expire_time += now;
    u_addmask('I', chan, invite, from, par, expire_time, flags);
    noshare = 0;
  }
}

/* +i <host> +<seconds-left> <from> <note>
 */
static void
share_pls_ignore(int idx, char *par)
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
    putlog(LOG_CMDS, "@", "%s: ignore %s (%s: %s)", dcc[idx].nick, ign, from, par);
    addignore(ign, from, par, expire_time);
    noshare = 0;
  }
}

#ifdef HUB
static void
share_ufno(int idx, char *par)
{
  putlog(LOG_BOTS, "@", "User file rejected by %s: %s", dcc[idx].nick, par);
  dcc[idx].status &= ~STAT_OFFERED;
  if (!(dcc[idx].status & STAT_GETTING))
    dcc[idx].status &= ~(STAT_SHARE | STAT_AGGRESSIVE);
}

static void
share_ufyes(int idx, char *par)
{
  if (dcc[idx].status & STAT_OFFERED) {
    dcc[idx].status &= ~STAT_OFFERED;
    dcc[idx].status |= STAT_SHARE;
    dcc[idx].status |= STAT_SENDING;

    dcc[idx].u.bot->uff_flags |= (UFF_OVERRIDE | UFF_INVITE | UFF_EXEMPT );
    dprintf(idx, "s feats overbots invites exempts\n");

    lower_bot_linked(idx);

    start_sending_users(idx);
    putlog(LOG_BOTS, "@", "Sending user file send request to %s", dcc[idx].nick);
  }
}
#endif /* HUB */

static void
share_userfileq(int idx, char *par)
{
  int ok = 1, i;

  if (bot_aggressive_to(dcc[idx].user)) {
    putlog(LOG_ERRORS, "*", "%s offered user transfer - I'm supposed to be aggressive to it", dcc[idx].nick);
    dprintf(idx, "s un I have you marked for Agressive sharing.\n");
    botunlink(-2, dcc[idx].nick, "I'm aggressive to you");
  } else {
    for (i = 0; i < dcc_total; i++)
      if (dcc[i].type->flags & DCT_BOT) {
        if ((dcc[i].status & STAT_SHARE) && (dcc[i].status & STAT_AGGRESSIVE) && (i != idx)) {
          ok = 0;
          break;
        }
      }
    if (!ok)
      dprintf(idx, "s un Already sharing.\n");
    else {
      dcc[idx].u.bot->uff_flags |= (UFF_OVERRIDE | UFF_INVITE | UFF_EXEMPT );
      dprintf(idx, "s uy overbots invites exempts\n");
      /* Set stat-getting to astatic void race condition (robey 23jun1996) */
      dcc[idx].status |= STAT_SHARE | STAT_GETTING | STAT_AGGRESSIVE;
#ifdef HUB
      putlog(LOG_BOTS, "*", "Downloading user file from %s", dcc[idx].nick);
#else /* !HUB */
      putlog(LOG_BOTS, "*", "Downloading user file via uplink.");
#endif /* HUB */
    }
  }
}

/* us <ip> <port> <length>
 */
static void
share_ufsend(int idx, char *par)
{
  char *ip = NULL, *port = NULL;
  char s[1024] = "";
  int i, sock;
  FILE *f = NULL;

  egg_snprintf(s, sizeof s, "%s.share.%s.%li.users", tempdir, conf.bot->nick, now);
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
    sock = getsock(SOCK_BINARY, hostprotocol(ip));      /* Don't buffer this -> mark binary. */
#else
    sock = getsock(SOCK_BINARY);        /* Don't buffer this -> mark binary. */
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

static void
share_version(int idx, char *par)
{
  /* Cleanup any share flags */
  dcc[idx].status &= ~(STAT_SHARE | STAT_GETTING | STAT_SENDING | STAT_OFFERED | STAT_AGGRESSIVE);
  dcc[idx].u.bot->uff_flags |= (UFF_OVERRIDE | UFF_INVITE | UFF_EXEMPT );
  if (bot_aggressive_to(dcc[idx].user)) {
    dprintf(idx, "s u?\n");
    dcc[idx].status |= STAT_OFFERED;
  }                             /*else
                                 * higher_bot_linked(idx); */
}

#ifdef HUB
void
hook_read_userfile()
{
  int i;

  if (!noshare) {
    for (i = 0; i < dcc_total; i++)
      if ((dcc[i].type->flags & DCT_BOT) && (dcc[i].status & STAT_SHARE) && !(dcc[i].status & STAT_AGGRESSIVE) && (1)) {
        /* Cancel any existing transfers */
        if (dcc[i].status & STAT_SENDING)
          cancel_user_xfer(-i, 0);
        dprintf(i, "s u?\n");
        dcc[i].status |= STAT_OFFERED;
      }
  }
}
#endif /* HUB */

static void
share_endstartup(int idx, char *par)
{
  dcc[idx].status &= ~STAT_GETTING;
  /* Send to any other sharebots */
#ifdef HUB
  hook_read_userfile();
#endif /* HUB */
}

static void
share_end(int idx, char *par)
{
  putlog(LOG_BOTS, "*", "Ending sharing with %s (%s).", dcc[idx].nick, par);
  cancel_user_xfer(-idx, 0);
  dcc[idx].status &= ~(STAT_SHARE | STAT_GETTING | STAT_SENDING | STAT_OFFERED | STAT_AGGRESSIVE);
  dcc[idx].u.bot->uff_flags = 0;
}

/* Note: these MUST be sorted. */
static botcmd_t C_share[] = {
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
  {"n", (Function) share_newuser},
  {"s", (Function) share_stick_ban},
  {"se", (Function) share_stick_exempt},
  {"sInv", (Function) share_stick_invite},
  {"u?", (Function) share_userfileq},
#ifdef HUB
  {"un", (Function) share_ufno},
#endif /* HUB */
  {"us", (Function) share_ufsend},
#ifdef HUB
  {"uy", (Function) share_ufyes},
#endif /* HUB */
  {"v", (Function) share_version},
  {NULL, NULL}
};


void
sharein(int idx, char *msg)
{
  char *code = NULL;
  int f, i;

  code = newsplit(&msg);
  for (f = 0, i = 0; C_share[i].name && !f; i++) {
    int y = egg_strcasecmp(code, C_share[i].name);

    if (!y)
      /* Found a match */
      (C_share[i].func) (idx, msg);
    if (y < 0)
      f = 1;
  }
}

void
shareout(struct chanset_t *chan, ...)
{
  int i, l;
  char *format = NULL, s[601] = "";
  va_list va;

  va_start(va, chan);
  format = va_arg(va, char *);

  strcpy(s, "s ");
  if ((l = egg_vsnprintf(s + 2, 509, format, va)) < 0)
    s[2 + (l = 509)] = 0;
  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type->flags & DCT_BOT) && (dcc[i].status & STAT_SHARE) && !(dcc[i].status & (STAT_GETTING | STAT_SENDING))) {
      if (chan) {
        fr.match = (FR_CHAN | FR_BOT);
        get_user_flagrec(dcc[i].user, &fr, chan->dname);
      }
      tputs(dcc[i].sock, s, l + 2);
    }
  va_end(va);
}

static void
shareout_but(struct chanset_t *chan, ...)
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
        (dcc[i].status & STAT_SHARE) && (!(dcc[i].status & STAT_GETTING)) && (!(dcc[i].status & STAT_SENDING))) {
      if (chan) {
        fr.match = (FR_CHAN | FR_BOT);
        get_user_flagrec(dcc[i].user, &fr, chan->dname);
      }
      tputs(dcc[i].sock, s, l + 2);
    }
  va_end(va);
}

#ifdef HUB
/* Flush all tbufs older than 15 minutes.
 */
static void
check_expired_tbufs()
{
  int i;

  /* Resend userfile requests */
  for (i = 0; i < dcc_total; i++) {
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
}

static int
write_tmp_userfile(char *fn, const struct userrec *bu, int idx)
{
  FILE *f = NULL;
  struct userrec *u = NULL;
  int ok = 0;

  if ((f = fopen(fn, "wb"))) {
    time_t tt;
    char s1[81] = "";

    chmod(fn, 0600);            /* make it -rw------- */
    tt = now;
    strcpy(s1, ctime(&tt));
    lfprintf(f, "#4v: %s -- %s -- written %s", ver, conf.bot->nick, s1);

    ok = 1;
    if (!write_chans(f, idx))
      ok = 0;
    if (!write_config(f, idx))
      ok = 0;
    if (!write_bans(f, idx))
      ok = 0;
    if (!write_exempts(f, idx))
      ok = 0;
    if (!write_invites(f, idx))
      ok = 0;

    for (u = (struct userrec *) bu; u && ok; u = u->next) {
      if (!write_user(u, f, idx)) {
        ok = 0;
      }
    }
    fclose(f);
  }
  if (!ok)
    putlog(LOG_MISC, "*", USERF_ERRWRITE2);
  return ok;
}
#endif /* HUB */

/* Erase old user list, switch to new one.
 */
void
finish_share(int idx)
{
  struct userrec *u = NULL, *ou = NULL;
  struct chanset_t *chan = NULL;
  int i, j = -1;

  for (i = 0; i < dcc_total; i++)
    if (!egg_strcasecmp(dcc[i].nick, dcc[idx].host) && (dcc[i].type->flags & DCT_BOT))
      j = i;
  if (j == -1)
    return;

/* compress.mod 
  if (!uncompressfile(dcc[idx].u.xfer->filename)) {
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
*/

  /*
   * This is where we remove all global and channel bans/exempts/invites and
   * ignores since they will be replaced by what our hub gives us.
   */

  noshare = 1;
  fr.match = (FR_CHAN | FR_BOT);

  while (global_bans)
    u_delmask('b', NULL, global_bans->mask, 1);
  while (global_ign)
    delignore(global_ign->igmask);
  while (global_invites)
    u_delmask('I', NULL, global_invites->mask, 1);
  while (global_exempts)
    u_delmask('e', NULL, global_exempts->mask, 1);

  for (chan = chanset; chan; chan = chan->next) {
    while (chan->bans)
      u_delmask('b', chan, chan->bans->mask, 1);
    while (chan->exempts)
      u_delmask('e', chan, chan->exempts->mask, 1);
    while (chan->invites)
      u_delmask('I', chan, chan->invites->mask, 1);
  }
  noshare = 0;
  ou = userlist;                /* Save old user list                   */
  userlist = (void *) -1;       /* Do this to prevent .user messups     */

  /* Bot user pointers are updated to point to the new list, all others
   * are set to NULL. If our userfile will be overriden, just set _all_
   * to NULL directly.
   */
  for (i = 0; i < dcc_total; i++)
    dcc[i].user = NULL;

  if (conf.bot->u)
    conf.bot->u = NULL;

  /* Read the transferred userfile. Add entries to u, which already holds
   * the bot entries in non-override mode.
   */
  loading = 1;
  checkchans(0);                /* flag all the channels.. */
  Context;
  if (!readuserfile(dcc[idx].u.xfer->filename, &u)) {		/* read the userfile into 'u' */
    /* FAILURE */
    char xx[1024] = "";

    Context;
    unlink(dcc[idx].u.xfer->filename);
    clear_userlist(u);          /* Clear new, obsolete, user list.      */
    clear_chanlist();           /* Remove all user references from the
                                 * channel lists.                       */
    for (i = 0; i < dcc_total; i++)
      dcc[i].user = get_user_by_handle(ou, dcc[i].nick);

    conf.bot->u = get_user_by_handle(ou, conf.bot->nick);

    userlist = ou;              /* Revert to old user list.             */
    lastuser = NULL;            /* Reset last accessed user ptr.        */

    checkchans(2);              /* un-flag the channels, we are keeping them.. */

    /* old userlist is now being used, safe to do this stuff... */
    loading = 0;
    putlog(LOG_MISC, "*", "%s", USERF_CANTREAD);
    dprintf(idx, "bye\n");
    egg_snprintf(xx, sizeof xx, "Disconnected %s (can't read userfile)", dcc[j].nick);
    botnet_send_unlinked(j, dcc[j].nick, xx);
    chatout("*** %s\n", xx);

    killsock(dcc[j].sock);
    lostdcc(j);

    return;
  }

  /* SUCCESS! */

  unlink(dcc[idx].u.xfer->filename);    

  loading = 0;

  clear_chanlist();             /* Remove all user references from the
                                 * channel lists.                       */
  userlist = u;                 /* Set new user list.                   */
  lastuser = NULL;              /* Reset last accessed user ptr.        */
  putlog(LOG_BOTS, "*", "%s.", USERF_XFERDONE);

  /*
   * Migrate:
   *   - unshared (got_share == 0) user entries
   */
  clear_userlist(ou);
  
  unlink(dcc[idx].u.xfer->filename);    /* Done with you!               */

  checkchans(1);                /* remove marked channels */
  trigger_cfg_changed();
  reaffirm_owners();            /* Make sure my owners are +a   */
  updatebot(-1, dcc[j].nick, '+', 0, 0, NULL);
}

#ifdef HUB
/* Begin the user transfer process.
 */
static void
start_sending_users(int idx)
{
  char share_file[1024] = "";
  int i = 1;

  egg_snprintf(share_file, sizeof share_file, "%s.share.%s.%li", tempdir, dcc[idx].nick, now);

  write_tmp_userfile(share_file, userlist, idx);

/* compress.mod
  if (!compress_file(share_file, compress_level)) {
    unlink(share_file);
    dprintf(idx, "s e %s\n", "uff parsing failed");
    putlog(LOG_BOTS, "*", "uff parsing failed");
    dcc[idx].status &= ~(STAT_SHARE | STAT_SENDING | STAT_AGGRESSIVE);
    return;
  }
*/

  if ((i = raw_dcc_send(share_file, "*users", "(users)", share_file)) > 0) {
    unlink(share_file);
    dprintf(idx, "s e %s\n", USERF_CANTSEND);
    putlog(LOG_BOTS, "@", "%s -- can't send userfile",
           i == DCCSEND_FULL ? "NO MORE DCC CONNECTIONS" :
           i == DCCSEND_NOSOCK ? "CAN'T OPEN A LISTENING SOCKET" :
           i == DCCSEND_BADFN ? "BAD FILE" : i == DCCSEND_FEMPTY ? "EMPTY FILE" : "UNKNOWN REASON!");
    dcc[idx].status &= ~(STAT_SHARE | STAT_SENDING | STAT_AGGRESSIVE);
  } else {
    updatebot(-1, dcc[idx].nick, '+', 0, 0, NULL);
    dcc[idx].status |= STAT_SENDING;
    i = dcc_total - 1;
    strcpy(dcc[i].host, dcc[idx].nick); /* Store bot's nick */
    dprintf(idx, "s us %lu %d %lu\n", iptolong(natip[0] ? (IP) inet_addr(natip) : getmyip()), dcc[i].port, dcc[i].u.xfer->length);

    /* Unlink the file. We don't really care whether this causes problems
     * for NFS setups. It's not worth the trouble.
     */
    unlink(share_file);
  }
}
#endif /* HUB */

static void (*def_dcc_bot_kill) (int, void *) = 0;

static void
cancel_user_xfer(int idx, void *x)
{
  int i, j, k = 0;

  if (idx < 0) {
    idx = -idx;
    k = 1;
    updatebot(-1, dcc[idx].nick, '-', 0, 0, NULL);
  }
  if (dcc[idx].status & STAT_SHARE) {
    if (dcc[idx].status & STAT_GETTING) {
      j = 0;
      for (i = 0; i < dcc_total; i++)
        if (!egg_strcasecmp(dcc[i].host, dcc[idx].nick) &&
            ((dcc[i].type->flags & (DCT_FILETRAN | DCT_FILESEND)) == (DCT_FILETRAN | DCT_FILESEND)))
          j = i;
      if (j != 0) {
        killsock(dcc[j].sock);
        unlink(dcc[j].u.xfer->filename);
        lostdcc(j);
      }
      putlog(LOG_BOTS, "*", "(Userlist download aborted.)");
    }
    if (dcc[idx].status & STAT_SENDING) {
      j = 0;
      for (i = 0; i < dcc_total; i++)
        if ((!egg_strcasecmp(dcc[i].host, dcc[idx].nick)) && ((dcc[i].type->flags & (DCT_FILETRAN | DCT_FILESEND))
                                                              == DCT_FILETRAN))
          j = i;
      if (j != 0) {
        killsock(dcc[j].sock);
        unlink(dcc[j].u.xfer->filename);
        lostdcc(j);
      }
      putlog(LOG_BOTS, "*", "(Userlist transmit aborted.)");
    }
  }
  if (!k)
    def_dcc_bot_kill(idx, x);
}

void
share_report(int idx, int details)
{
  int i, j;

  if (details) {
    for (i = 0; i < dcc_total; i++)
      if (dcc[i].type == &DCC_BOT) {
        if (dcc[i].status & STAT_GETTING) {
          int ok = 0;

          for (j = 0; j < dcc_total; j++)
            if (((dcc[j].type->flags & (DCT_FILETRAN | DCT_FILESEND))
                 == (DCT_FILETRAN | DCT_FILESEND)) && !egg_strcasecmp(dcc[j].host, dcc[i].nick)) {
              dprintf(idx, "Downloading userlist from %s (%d%% done)\n",
                      dcc[i].nick, (int) (100.0 * ((float) dcc[j].status) / ((float) dcc[j].u.xfer->length)));
              ok = 1;
              break;
            }
          if (!ok)
            dprintf(idx, "Download userlist from %s (negotiating " "botentries)\n", dcc[i].nick);
#ifdef HUB
        } else if (dcc[i].status & STAT_SENDING) {
          for (j = 0; j < dcc_total; j++) {
            if (((dcc[j].type->flags & (DCT_FILETRAN | DCT_FILESEND))
                 == DCT_FILETRAN)
                && !egg_strcasecmp(dcc[j].host, dcc[i].nick)) {
              if (dcc[j].type == &DCC_GET)
                dprintf(idx, "Sending userlist to %s (%d%% done)\n",
                        dcc[i].nick, (int) (100.0 * ((float) dcc[j].status) / ((float) dcc[j].u.xfer->length)));
              else
                dprintf(idx, "Sending userlist to %s (waiting for connect)\n", dcc[i].nick);
            }
          }
        } else if (dcc[i].status & STAT_AGGRESSIVE) {
          dprintf(idx, "    Passively sharing with %s.\n", dcc[i].nick);
        } else if (dcc[i].status & STAT_SHARE) {
          dprintf(idx, "    Aggressively sharing with %s.\n", dcc[i].nick);
#endif /* HUB */
        }
      }
  }
}

void
share_init()
{
#ifdef HUB
  timer_create_secs(60, "check_expired_tbufs", (Function) check_expired_tbufs);
#endif /* HUB */
#ifdef LEAF
  timer_create_secs(1, "check_delay", (Function) check_delay);
#endif /* LEAF */
  def_dcc_bot_kill = DCC_BOT.kill;
  DCC_BOT.kill = cancel_user_xfer;
}
