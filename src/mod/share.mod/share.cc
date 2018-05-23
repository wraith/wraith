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
 * share.c -- part of share.mod
 *
 */


#include "src/common.h"
#include "src/main.h"
#include "src/rfc1459.h"
#include "src/botmsg.h"
#include "src/misc.h"
#include "src/misc_file.h"
#include "src/cmds.h"
#include "src/chanprog.h"
#include "src/users.h"
#include "src/userrec.h"
#include "src/botnet.h"
#include "src/auth.h"
#include "src/set.h"
#include "src/EncryptedStream.h"
#include <bdlib/src/String.h>

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
#include "src/mod/irc.mod/irc.h"
#include "src/mod/server.mod/server.h"

void irc_init();

static struct flag_record fr = { 0, 0, 0, 0 };

static bd::Stream* stream_in;

struct delay_mode {
  struct delay_mode *next;
  struct chanset_t *chan;
  int plsmns;
  int mode;
  int seconds;
  char *mask;
};

static struct delay_mode *start_delay = NULL;

/* Prototypes */
static void start_sending_users(int);
static void stream_send_users(int);
static void share_read_stream(int, bd::Stream&);
#ifdef __GNUC__
 static void shareout_but(int, const char *, ...)  __attribute__ ((format(printf, 2, 3)));
#else
 static void shareout_but(int, const char *, ...);
#endif
static bool cancel_user_xfer_staylinked = 0;
static void cancel_user_xfer(int, void *);

#include "share.h"

/*
 *    Resync buffers
 */

/* Store info for sharebots */
struct share_msgq {
  struct share_msgq *next;
  char *msg;
};

typedef struct tandbuf_t {
  struct tandbuf_t *next;
  struct share_msgq *q;
  time_t timer;
  char bot[HANDLEN + 1];
} tandbuf;

tandbuf *tbuf = NULL;


/* Create a tandem buffer for 'bot'.
 */
static void new_tbuf(char *bot)
{
  tandbuf **old = &tbuf, *newbuf = NULL;

  newbuf = (tandbuf *) calloc(1, sizeof(tandbuf));
  strlcpy(newbuf->bot, bot, sizeof(newbuf->bot));
  newbuf->q = NULL;
  newbuf->timer = now;
  newbuf->next = *old;
  *old = newbuf;
  putlog(LOG_BOTS, "*", "Creating resync buffer for %s", bot);
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
static bool flush_tbuf(char *bot)
{
  tandbuf *t = NULL, *tnext = NULL;

  for (t = tbuf; t; t = tnext) {
    tnext = t->next;
    if (!strcasecmp(t->bot, bot)) {
      del_tbuf(t);
      return 1;
    }
  }
  return 0;
}

static struct share_msgq *q_addmsg(struct share_msgq *qq, char *s)
{
  struct share_msgq *q = NULL;
  int cnt;
  size_t siz = 0;

  if (!qq) {
    q = (share_msgq *) calloc(1, sizeof *q);

    q->next = NULL;
    siz = strlen(s) + 1;
    q->msg = (char *) calloc(1, siz);
    strlcpy(q->msg, s, siz);
    return q;
  }
  cnt = 0;
  for (q = qq; q->next; q = q->next)
    cnt++;
  if (cnt > 1000)
    return NULL;                /* Return null: did not alter queue */
  q->next = (share_msgq *) calloc(1, sizeof *q->next);

  q = q->next;
  q->next = NULL;
  siz = strlen(s) + 1;
  q->msg = (char *) calloc(1, siz);
  strlcpy(q->msg, s, siz);
  return qq;
}

/* Add stuff to a specific bot's tbuf.
 */
static void q_tbuf(char *bot, char *s)
{
  struct share_msgq *q = NULL;
  tandbuf *t = NULL;

  for (t = tbuf; t && t->bot[0]; t = t->next)
    if (!strcasecmp(t->bot, bot)) {
      if ((q = q_addmsg(t->q, s)))
        t->q = q;
      break;
    }
}

/* Add stuff to the resync buffers.
 */
static void q_resync(char *s)
{
  struct share_msgq *q = NULL;
  tandbuf *t = NULL;

  for (t = tbuf; t && t->bot[0]; t = t->next) {
    if ((q = q_addmsg(t->q, s)))
      t->q = q;
  }
}

static void q_resync_but(char *s, const char *bot)
{
  struct share_msgq *q = NULL;
  tandbuf *t = NULL;

  for (t = tbuf; t && t->bot[0]; t = t->next) {
    if (strcasecmp(t->bot, bot)) {
      if ((q = q_addmsg(t->q, s)))
        t->q = q;
    }
  }
}

/* Dump the resync buffer for a bot.
 */
void dump_resync(int idx)
{
  struct share_msgq *q = NULL;
  tandbuf *t = NULL;

  for (t = tbuf; t && t->bot[0]; t = t->next)
    if (!strcasecmp(dcc[idx].nick, t->bot)) {
      for (q = t->q; q && q->msg[0]; q = q->next) {
        dprintf(idx, "%s", q->msg);
      }
      flush_tbuf(dcc[idx].nick);
      break;
    }
}

/*
 *   Sup's delay code
 */

static void
add_delay(struct chanset_t *chan, int plsmns, int mode, char *mask)
{
  struct delay_mode *d = (struct delay_mode *) calloc(1, sizeof(struct delay_mode));

  d->chan = chan;
  d->plsmns = plsmns;
  d->mode = mode;
  size_t mlen = strlen(mask) + 1;
  d->mask = (char *) calloc(1, mlen);

  strlcpy(d->mask, mask, mlen);
  d->seconds = (int) (now + randint(20));
  d->next = start_delay;
  start_delay = d;
}

static void
del_delay(struct delay_mode *delay)
{
  struct delay_mode *old = NULL;

  for (struct delay_mode *d = start_delay; d; old = d, d = d->next) {
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
  struct delay_mode *dnext = NULL;

  for (struct delay_mode *d = start_delay; d; d = dnext) {
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

static void
share_stick_mask(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    char *types = newsplit(&par);
    const char type = types[0];
    const char *str_type = (type == 'b' ? "ban" : type == 'e' ? "exempt" : "invite");
    char *host = NULL, *val = NULL;
    bool yn = 0;
    struct chanset_t *chan = NULL;
    maskrec *channel_list = NULL;

    host = newsplit(&par);
    val = newsplit(&par);
    yn = atoi(val);
    noshare = 1;

    if (!par[0]) {              /* Global ban */
      channel_list = (type == 'b' ? global_bans : type == 'e' ? global_exempts : global_invites);

      if (u_setsticky_mask(NULL, channel_list, host, yn, type) > 0) {
        if (conf.bot->hub)
          putlog(LOG_CMDS, "@", "%s: %s %s %s", dcc[idx].nick, (yn) ? "stick" : "unstick", str_type, host);
        else
          for (chan = chanset; chan ; chan = chan->next)
            check_this_mask(type, chan, host, yn);
        shareout_but(idx, "ms %c %s %d\n", type, host, yn);
      }
    } else {
      if ((chan = findchan_by_dname(par))) {
          channel_list = (type == 'b' ? chan->bans : type == 'e' ? chan->exempts : chan->invites);
        if (u_setsticky_mask(chan, channel_list, host, yn, type) > 0) {
          if (conf.bot->hub)
            putlog(LOG_CMDS, "@", "%s: %s %s %s %s", dcc[idx].nick, (yn) ? "stick" : "unstick", str_type, host, par);
          else
            check_this_mask(type, chan, host, yn);
          shareout_but(idx, "ms %c %s %d %s\n", type, host, yn, chan->dname);
          noshare = 0;
          return;
        }
      }
      putlog(LOG_CMDS, "@", "Rejecting invalid sticky %s: %s on %s%s", host, par, yn ? "" : " (unstick)", str_type);
    }
    noshare = 0;
  }
}

static void
share_chhand(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    char *hand = NULL;
    struct userrec *u = NULL;

    hand = newsplit(&par);
    u = get_user_by_handle(userlist, hand);
    if (u) {
      int value = 0;

      shareout_but(idx, "h %s %s\n", hand, par);
      noshare = 1;
      value = change_handle(u, par);
      noshare = 0;
      if (value && conf.bot->hub)
        putlog(LOG_CMDS, "@", "%s: handle %s->%s", dcc[idx].nick, hand, par);
    }
  }
}

static void
share_chattr(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    char *hand = NULL, *atr = NULL, s[100] = "";
    struct chanset_t *cst = NULL;
    struct userrec *u = NULL;
    struct flag_record fr2;
    flag_t ofl;

    hand = newsplit(&par);
    u = get_user_by_handle(userlist, hand);
    if (u) {
      atr = newsplit(&par);
      cst = findchan_by_dname(par);
      if (!par[0] || cst) {
        if (!(dcc[idx].status & STAT_GETTING))
          shareout_but(idx, "a %s %s %s\n", hand, atr, par);
        noshare = 1;
        if (par[0] && cst) {
          fr.match = FR_CHAN;
          fr2.match = FR_CHAN;
          if (u->bot) {
            fr.match |= FR_BOT; 
            fr2.match |= FR_BOT;
          }
          break_down_flags(atr, &fr, 0);
          get_user_flagrec(u, &fr2, par);
          set_user_flagrec(u, &fr, par);
          noshare = 0;
          check_dcc_chanattrs(u, par, fr.chan, fr2.chan);
          build_flags(s, &fr, 0);
          if (conf.bot->hub) {
            if (!(dcc[idx].status & STAT_GETTING))
              putlog(LOG_CMDS, "@", "%s: chattr %s %s %s", dcc[idx].nick, hand, s, par);
          } else
            check_this_user(u->handle, 0, NULL);
        } else {
          fr.match = FR_GLOBAL;
          get_user_flagrec(dcc[idx].user, &fr, 0);
          /* Don't let bot flags be altered */
          ofl = fr.global;
          if (u->bot)
            fr.match |= FR_BOT;
          break_down_flags(atr, &fr, 0);
          fr.global = sanity_check(fr.global, u->bot);
          set_user_flagrec(u, &fr, 0);
          noshare = 0;
          check_dcc_attrs(u, ofl);
          build_flags(s, &fr, 0);
          fr.match = FR_CHAN;
          if (conf.bot->hub) {
            if (!(dcc[idx].status & STAT_GETTING))
              putlog(LOG_CMDS, "@", "%s: chattr %s %s", dcc[idx].nick, hand, s);
          } else {
            check_this_user(u->handle, 0, NULL);
          }
        }
        noshare = 0;
      }
      if (conf.bot->hub)
        write_userfile(-1);
    }
  }
}

static void
share_pls_chrec(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    char *user = NULL;
    struct chanset_t *chan = NULL;
    struct userrec *u = NULL;

    user = newsplit(&par);
    if ((u = get_user_by_handle(userlist, user))) {
      chan = findchan_by_dname(par);
      if (chan) {
        noshare = 1;
        shareout_but(idx, "+cr %s %s\n", user, par);
        if (!get_chanrec(u, par)) {
          add_chanrec(u, par);
          if (conf.bot->hub)
            putlog(LOG_CMDS, "@", "%s: +chrec %s %s", dcc[idx].nick, user, par);
        }
        noshare = 0;
      }
    }
  }
}

static void
share_mns_chrec(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    char *user = NULL;
    struct chanset_t *chan = NULL;
    struct userrec *u = NULL;

    user = newsplit(&par);
    if ((u = get_user_by_handle(userlist, user))) {
      chan = findchan_by_dname(par);
      if (chan) {
        noshare = 1;
        del_chanrec(u, par);
        shareout_but(idx, "-cr %s %s\n", user, par);
        noshare = 0;
        if (conf.bot->hub)
          putlog(LOG_CMDS, "@", "%s: -chrec %s %s", dcc[idx].nick, user, par);
        else
          recheck_channel(chan, 0);
      }
    }
  }
}

static void
share_newuser(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    char *nick = NULL, *host = NULL, *pass = NULL;
    bool isbot = 0;
    struct userrec *u = NULL;

    nick = newsplit(&par);
    host = newsplit(&par);
    pass = newsplit(&par);
    
    if (nick[0] == '-') {
      isbot = 1;
      nick++;
    }    
    if (!(u = get_user_by_handle(userlist, nick))) {
      char s[100] = "";

      fr.global = 0;

      fr.match = FR_GLOBAL;
      if (isbot)
        fr.match |= FR_BOT;
      break_down_flags(par, &fr, NULL);

      /* If user already exists, ignore command */
      shareout_but(idx, "n %s%s %s %s %s\n", isbot ? "-" : "", nick, host, pass, par);

      if (strlen(nick) > HANDLEN)
        nick[HANDLEN] = 0;

      fr.match = FR_GLOBAL;
      build_flags(s, &fr, 0);

      noshare = 1;
      userlist = adduser(userlist, nick, host, pass, 0, isbot);

      /* Support for userdefinedflag share - drummer */
      u = get_user_by_handle(userlist, nick);
      set_user_flagrec(u, &fr, 0);
      noshare = 0;
      if (conf.bot->hub)
        putlog(LOG_CMDS, "@", "%s: newuser %s %s", dcc[idx].nick, nick, s);
      write_userfile(-1);
    }
  }
}

static void
share_killuser(int idx, char *par)
{
  struct userrec *u = NULL;

  /* If user is a share bot, ignore command */
  if ((dcc[idx].status & STAT_SHARE) && (u = get_user_by_handle(userlist, par))) {

    noshare = 1;
    check_this_user(u->handle, 1, NULL);
    if (deluser(par)) {
      shareout_but(idx, "k %s\n", par);
      if (conf.bot->hub)
        putlog(LOG_CMDS, "@", "%s: killuser %s", dcc[idx].nick, par);
      write_userfile(-1);
    }
    noshare = 0;
  }
}

static void
share_pls_host(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    char *hand = NULL;
    struct userrec *u = NULL;

    hand = newsplit(&par);
    if ((u = get_user_by_handle(userlist, hand))) {
      shareout_but(idx, "+h %s %s\n", hand, par);
      set_user(&USERENTRY_HOSTS, u, par);
      if (conf.bot->hub)
        putlog(LOG_CMDS, "@", "%s: +host %s %s", dcc[idx].nick, hand, par);
      else
        check_this_user(u->handle, 0, NULL);
    }
  }
}

static void
share_pls_bothost(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    char *hand = NULL;
    struct userrec *u = NULL;

    hand = newsplit(&par);
    u = get_user_by_handle(userlist, hand);
    if (!(dcc[idx].status & STAT_GETTING))
      shareout_but(idx, "+bh %s %s\n", hand, par);
    /* Add bot to userlist if not there */
    if (u) {
      if (!u->bot)
        return;                 /* ignore */
      set_user(&USERENTRY_HOSTS, u, par);
      clear_chanlist();
    } else {
      userlist = adduser(userlist, hand, par, "-", 0, 1);
    }
    if (conf.bot->hub && !(dcc[idx].status & STAT_GETTING))
      putlog(LOG_CMDS, "@", "%s: +host %s %s", dcc[idx].nick, hand, par);
  }
}

static void
share_mns_host(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    char *hand = NULL;
    struct userrec *u = NULL;

    hand = newsplit(&par);
    if ((u = get_user_by_handle(userlist, hand))) {
      shareout_but(idx, "-h %s %s\n", hand, par);
      noshare = 1;
      delhost_by_handle(hand, par);
      noshare = 0;
      if (conf.bot->hub)
        putlog(LOG_CMDS, "@", "%s: -host %s %s", dcc[idx].nick, hand, par);
      else
        check_this_user(hand, 2, par);
    }
  }
}

static void
share_change(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    char *key = NULL, *hand = NULL;
    struct userrec *u = NULL;
    struct user_entry_type *uet = NULL;
    struct user_entry *e = NULL;

    key = newsplit(&par);
    hand = newsplit(&par);

    u = get_user_by_handle(userlist, hand);

    if (!(uet = find_entry_type(key)))
      /* If it's not a supported type, forget it */
      putlog(LOG_ERROR, "*", "Ignore ch %s from %s (unknown type)", key, dcc[idx].nick);
    else {
      if (!(dcc[idx].status & STAT_GETTING))
        shareout_but(idx, "c %s %s %s\n", key, hand, par);
      noshare = 1;
      if (!u && (uet == &USERENTRY_BOTADDR)) {
        char pass[30] = "";

        makepass(pass);
        userlist = adduser(userlist, hand, "none", pass, 0, 1);
        u = get_user_by_handle(userlist, hand);
      } else if (!u) {
        noshare = 0;
        return;
      }

      if (uet->got_share && uet != &USERENTRY_SET) {
        if (!(e = find_user_entry(uet, u))) {
          e = (struct user_entry *) calloc(1, sizeof(struct user_entry));

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
      } else if (uet == &USERENTRY_SET) {
        /*
         * set_set() chains down to set_user() which mimics
         * the above and leads to deleting/freeing 'e' when
         * already done in set_user(). set_gotshare() ignores
         * e.
         */
        uet->got_share(u, NULL, par, idx);
      }
      noshare = 0;
    }

    if (uet == &USERENTRY_BOTADDR) {
      write_userfile(-1);
    }
  }
}

static void
share_clearhosts(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    char *hand = newsplit(&par);
    struct userrec *u = get_user_by_handle(userlist, hand);
    if (u) {
      shareout_but(idx, "ch %s\n", u->handle);
      if (!conf.bot->hub && server_online)
        check_this_user(u->handle, 1, NULL);
      noshare = 1;
      set_user(&USERENTRY_HOSTS, u, (void *) "none");
      noshare = 0;
    }
  }
}

static void
share_chchinfo(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    char *hand = NULL, *chan = NULL;
    struct chanset_t *cst = NULL;
    struct userrec *u = NULL;

    hand = newsplit(&par);
    if ((u = get_user_by_handle(userlist, hand))) {
      chan = newsplit(&par);
      cst = findchan_by_dname(chan);
      if (cst) {
        shareout_but(idx, "chchinfo %s %s %s\n", hand, chan, par);
        noshare = 1;
        set_handle_chaninfo(userlist, hand, chan, par);
        noshare = 0;
        if (conf.bot->hub)
          putlog(LOG_CMDS, "@", "%s: change info %s %s", dcc[idx].nick, chan, hand);
      }
    }
  }
}

static void share_mns_mask(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    char *types = newsplit(&par);
    const char type = types[0];

    shareout_but(idx, "-m %c %s\n", type, par);
    if (conf.bot->hub)
      putlog(LOG_CMDS, "@", "%s: cancel %s %s", dcc[idx].nick, type == 'b' ? "ban" : type == 'e' ? "exempt" : "invite", par);
    str_unescape(par, '\\');
    noshare = 1;
    if (u_delmask(type, NULL, par, 1) > 0) {
      if (!conf.bot->hub) {
        struct chanset_t *chan = NULL;
        masklist *channel_list = NULL;
        
        for (chan = chanset; chan; chan = chan->next) {
          channel_list = (type == 'b' ? chan->channel.ban : type == 'e' ? 
                          chan->channel.exempt : chan->channel.invite);

          if (channel_list && ismasked(channel_list, par))
            add_delay(chan, '-', type, par);
        }
      }
    }
    noshare = 0;
  }
}

static void share_mns_maskchan(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    char *types = newsplit(&par);
    const char type = types[0];

    char *chname = NULL;
    struct chanset_t *chan = NULL;
    int value = 0;

    chname = newsplit(&par);
    chan = findchan_by_dname(chname);
    if (chan) {
      shareout_but(idx, "-mc %c %s %s\n", type, chname, par);
      if (conf.bot->hub)
        putlog(LOG_CMDS, "@", "%s: cancel %s %s on %s", dcc[idx].nick,
            type == 'b' ? "ban" : type == 'e' ? "exempt" : "invite", par, chname);
      str_unescape(par, '\\');
      noshare = 1;
      value = u_delmask(type, chan, par, 1);
      noshare = 0;
      if (!conf.bot->hub && value > 0)
        add_delay(chan, '-', type, par);
    }
  }
}

static void share_pls_mask(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    char *types = newsplit(&par);
    const char type = types[0];

    const char *str_type = (type == 'b' ? "ban" : type == 'e' ? "exempt" : "invite");
    time_t expire_time;
    char *mask = NULL, *tm = NULL, *from = NULL;
    int flags = 0;
    bool stick = 0;

    shareout_but(idx, "+m %c %s\n", type, par);
    mask = newsplit(&par);
    str_unescape(mask, '\\');
    tm = newsplit(&par);
    from = newsplit(&par);
    if (strchr(from, 's')) {
      flags |= MASKREC_STICKY;
      stick = 1;
    }
    if (strchr(from, 'p'))
      flags |= MASKREC_PERM;
    from = newsplit(&par);
    expire_time = (time_t) atoi(tm);
    if (expire_time != 0L)
      expire_time += now;
    noshare = 1;
    u_addmask(type, NULL, mask, from, par, expire_time, flags);
    noshare = 0;
    if (conf.bot->hub)
      putlog(LOG_CMDS, "@", "%s: global %s %s (%s:%s)", dcc[idx].nick, str_type, mask, from, par);
    else {
      for (struct chanset_t *chan = chanset; chan != NULL; chan = chan->next)
        check_this_mask(type, chan, mask, stick);
    }
  }
}

static void share_pls_maskchan(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    char *types = newsplit(&par);
    const char type = types[0];

    const char *str_type = (type == 'b' ? "ban" : type == 'e' ? "exempt" : "invite");
    time_t expire_time;
    int flags = 0;
    char *mask = NULL, *tm = NULL, *chname = NULL, *from = NULL;
    bool stick = 0;
    struct chanset_t *chan = NULL;

    mask = newsplit(&par);
    tm = newsplit(&par);
    chname = newsplit(&par);
    chan = findchan_by_dname(chname);
    if (chan) {
      shareout_but(idx, "+mc %c %s %s %s %s\n", type, mask, tm, chname, par);
      str_unescape(mask, '\\');
      from = newsplit(&par);
      if (strchr(from, 's')) {
        flags |= MASKREC_STICKY;
        stick = 1;
      }
      if (strchr(from, 'p'))
        flags |= MASKREC_PERM;
      from = newsplit(&par);
      if (conf.bot->hub)
        putlog(LOG_CMDS, "@", "%s: %s %s on %s (%s:%s)", dcc[idx].nick, str_type, mask, chname, from, par);
      expire_time = (time_t) atoi(tm);
      if (expire_time != 0L)
        expire_time += now;
      noshare = 1;
      u_addmask(type, chan, mask, from, par, expire_time, flags);
      noshare = 0;
      if (!conf.bot->hub)
        check_this_mask(type, chan, mask, stick);
    }
  }
}

static void
share_mns_ignore(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    shareout_but(idx, "-i %s\n", par);
    str_unescape(par, '\\');
    if (conf.bot->hub)
      putlog(LOG_CMDS, "@", "%s: cancel ignore %s", dcc[idx].nick, par);
    noshare = 1;
    delignore(par);
    noshare = 0;
  }
}

/* +i <host> +<seconds-left> <from> <note>
 */
static void
share_pls_ignore(int idx, char *par)
{
  if (dcc[idx].status & STAT_SHARE) {
    time_t expire_time;
    char *ign = NULL, *from = NULL, *ts = NULL;

    shareout_but(idx, "+i %s\n", par);
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
    if (strlen(from) > HANDLEN)
      from[HANDLEN] = 0;
    par[65] = 0;
    if (conf.bot->hub)
      putlog(LOG_CMDS, "@", "%s: ignore %s (%s: %s)", dcc[idx].nick, ign, from, par);
    noshare = 1;
    addignore(ign, from, (const char *) par, expire_time);
    noshare = 0;
  }
}

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

    dcc[idx].u.bot->uff_flags |= (UFF_OVERRIDE | UFF_INVITE | UFF_EXEMPT);
    dprintf(idx, "s feats overbots invites exempts\n");

    lower_bot_linked(idx);

    if (strstr(par, "chdefault"))
        dcc[idx].u.bot->uff_flags |= UFF_CHDEFAULT;

    if (strstr(par, "stream")) {
      updatebot(-1, dcc[idx].nick, '+', 0, 0, 0, NULL, -1);
      /* Start up a tbuf to queue outgoing changes for this bot until the
       * userlist is done transferring.
       */
      new_tbuf(dcc[idx].nick);
      /* override shit removed here */
      q_tbuf(dcc[idx].nick, "s !\n");
      dcc[idx].status |= STAT_SENDING;
      stream_send_users(idx);
      dump_resync(idx);
      dcc[idx].status &= ~STAT_SENDING;
    } else
      start_sending_users(idx);
    putlog(LOG_BOTS, "@", "Sending user file send request to %s", dcc[idx].nick);
  }
}

static void
share_userfileq(int idx, char *par)
{
  flush_tbuf(dcc[idx].nick);

  if (bot_aggressive_to(dcc[idx].user)) {
    putlog(LOG_ERRORS, "*", "%s offered user transfer - I'm supposed to be aggressive to it [likely a hack]", dcc[idx].nick);
    dprintf(idx, "s un I have you marked for Agressive sharing.\n");
    botunlink(-2, dcc[idx].nick, "I'm aggressive to you");
  } else {
    bool ok = 1;

    for (int i = 0; i < dcc_total; i++)
      if (dcc[i].type && dcc[i].type->flags & DCT_BOT) {
        if ((dcc[i].status & STAT_SHARE) && (dcc[i].status & STAT_AGGRESSIVE) && (i != idx)) {
          ok = 0;
          break;
        }
      }
    if (!ok)
      dprintf(idx, "s un Already sharing.\n");
    else {
      dcc[idx].u.bot->uff_flags |= (UFF_OVERRIDE | UFF_INVITE | UFF_EXEMPT);
      dprintf(idx, "s uy overbots invites exempts stream chdefault\n");
      /* Set stat-getting to astatic void race condition (robey 23jun1996) */
      dcc[idx].status |= STAT_SHARE | STAT_GETTING | STAT_AGGRESSIVE;
      if (conf.bot->hub)
        putlog(LOG_BOTS, "*", "Downloading user file from %s", dcc[idx].nick);
      else
        putlog(LOG_BOTS, "*", "Downloading user file via uplink.");
    }
  }
}

/* us <ip> <port> <length>
 */
static void
share_ufsend(int idx, char *par)
{
  if (bot_aggressive_to(dcc[idx].user)) {
    putlog(LOG_ERRORS, "*", "%s attempted to start sending userfile [compat] - I'm supposed to be aggressive to it [likely a hack]", dcc[idx].nick);
    dprintf(idx, "s un I have you marked for Agressive sharing.\n");
    botunlink(-2, dcc[idx].nick, "I'm aggressive to you");
    return;
  }

  char *port = NULL, *ip = NULL;
  char s[1024] = "";
  int i, sock;
  FILE *f = NULL;

  char rand[7] = "";
  make_rand_str(rand, sizeof(rand) - 1, 0);
  simple_snprintf(s, sizeof(s), "%s.share.%s", tempdir, rand);
  //mktemp(s); //Although safe here, g++ complains too much.

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
    sock = getsock(SOCK_BINARY, AF_INET);
#else
    sock = getsock(SOCK_BINARY);        /* Don't buffer this -> mark binary. */
#endif /* USE_IPV6 */
    int open_telnet_return = 0;
    if (sock < 0 || (open_telnet_return = open_telnet_dcc(sock, ip, port)) < 0) {
      fclose(f);
      if (open_telnet_return != -1 && sock != -1)
        killsock(sock);
      putlog(LOG_BOTS, "@", "Asynchronous connection failed!");
      dprintf(idx, "s e Can't connect to you!\n");
      zapfbot(idx);
    } else {
      putlog(LOG_DEBUG, "@", "Connecting to %s:%s for userfile.", ip, port);
      i = new_dcc(&DCC_FORK_SEND, sizeof(struct xfer_info));
      dcc[i].addr = my_atoul(ip);
      dcc[i].port = atoi(port);
      strlcpy(dcc[i].nick, "*users", sizeof(dcc[i].nick));
      dcc[i].u.xfer->filename = strdup(s);
      dcc[i].u.xfer->origname = dcc[i].u.xfer->filename;
      dcc[i].u.xfer->length = atoi(par);
      dcc[i].u.xfer->f = f;
      dcc[i].sock = sock;
      strlcpy(dcc[i].host, dcc[idx].nick, sizeof(dcc[i].host));
      dcc[idx].status |= STAT_GETTING;
    }
  }
}

static void
share_version(int idx, char *par)
{
  /* Cleanup any share flags */
  dcc[idx].status &= ~(STAT_SHARE | STAT_GETTING | STAT_SENDING | STAT_OFFERED | STAT_AGGRESSIVE);
  dcc[idx].u.bot->uff_flags |= (UFF_OVERRIDE | UFF_INVITE | UFF_EXEMPT);
  if (bot_aggressive_to(dcc[idx].user)) {
    dprintf(idx, "s u?\n");
    dcc[idx].status |= STAT_OFFERED;
  }
  // else higher_bot_linked(idx);
}

void
hook_read_userfile()
{
  if (!noshare) {
    for (int i = 0; i < dcc_total; i++) {
      if (dcc[i].type && (dcc[i].type->flags & DCT_BOT) && (dcc[i].status & STAT_SHARE) && !(dcc[i].status & STAT_AGGRESSIVE)
          && (1)) {
        /* Cancel any existing transfers */
        if (dcc[i].status & STAT_SENDING) {
          cancel_user_xfer_staylinked = 1;
          cancel_user_xfer(i, 0);
        }
        dprintf(i, "s u?\n");
        dcc[i].status |= STAT_OFFERED;
      }
    }
  }
}

static void
share_endstartup(int idx, char *par)
{
  dcc[idx].status &= ~STAT_GETTING;
  // Share any local changes out
  dump_resync(idx);
  /* Send to any other sharebots */
  if (conf.bot->hub || conf.bot->localhub) {
    have_linked_to_hub = 1;
    hook_read_userfile();
  }

  if (!conf.bot->hub) {
    /* Our hostmask may have been updated on connect, but the new userfile may not have it. */
    check_hostmask();
  }
}

static void
share_end(int idx, char *par)
{
  putlog(LOG_BOTS, "*", "Ending sharing with %s (%s).", dcc[idx].nick, par);
  cancel_user_xfer_staylinked = 1;
  cancel_user_xfer(idx, 0);
  dcc[idx].status &= ~(STAT_SHARE | STAT_GETTING | STAT_SENDING | STAT_OFFERED | STAT_AGGRESSIVE);
  dcc[idx].u.bot->uff_flags = 0;
}

static void share_userfile_line(int idx, char *par) {
  if (stream_in && (dcc[idx].status & STAT_GETTING)) {
    char *size = newsplit(&par);

    (*stream_in) << bd::String(par, atoi(size));
    (*stream_in) << '\n';
  }
}

static void share_userfile_start(int idx, char *par) {
  if (bot_aggressive_to(dcc[idx].user)) {
    putlog(LOG_ERRORS, "*", "%s attempted to initiate user transfer - I'm supposed to be aggressive to it [likely a hack]", dcc[idx].nick);
    dprintf(idx, "s un I have you marked for Agressive sharing.\n");
    botunlink(-2, dcc[idx].nick, "I'm aggressive to you");
    return;
  }

  dcc[idx].status |= STAT_GETTING;
  /* Start up a tbuf to queue outgoing changes for this bot until the
   * userlist is done transferring.
   */
  new_tbuf(dcc[idx].nick);
  stream_in = new bd::Stream();
}

static void share_userfile_end(int idx, char *par) {
  if (bot_aggressive_to(dcc[idx].user)) {
    putlog(LOG_ERRORS, "*", "%s attempted to end user transfer - I'm supposed to be aggressive to it [likely a hack]", dcc[idx].nick);
    dprintf(idx, "s un I have you marked for Agressive sharing.\n");
    botunlink(-2, dcc[idx].nick, "I'm aggressive to you");
    return;
  }

  stream_in->seek(0, SEEK_SET);
  share_read_stream(idx, *stream_in);
  delete stream_in;
}

/* Note: these MUST be sorted. */
static botcmd_t C_share[] = {
  {"!", share_endstartup, 0},
  {"+bh", share_pls_bothost, 0},
  {"+cr", share_pls_chrec, 0},
  {"+h", share_pls_host, 0},
  {"+i", share_pls_ignore, 0},
  {"+m", share_pls_mask, 0},
  {"+mc", share_pls_maskchan, 0},
  {"-cr", share_mns_chrec, 0},
  {"-h", share_mns_host, 0},
  {"-i", share_mns_ignore, 0},
  {"-m", share_mns_mask, 0},
  {"-mc", share_mns_maskchan, 0},
  {"a", share_chattr, 0},
  {"c", share_change, 0},
  {"ch", share_clearhosts, 0},
  {"chchinfo", share_chchinfo, 0},
  {"e", share_end, 0},
  {"h", share_chhand, 0},
  {"k", share_killuser, 0},
  {"l", share_userfile_line, 0},
  {"le", share_userfile_end, 0},
  {"ls", share_userfile_start, 0},
  {"ms", share_stick_mask, 0},
  {"n", share_newuser, 0},
  {"u?", share_userfileq, 0},
  {"un", share_ufno, HUB},
  {"us", share_ufsend, 0},
  {"uy", share_ufyes, HUB},
  {"v", share_version, 0},
  {NULL, NULL, 0}
};


void
sharein(int idx, char *msg)
{
  char *code = newsplit(&msg);
  const botcmd_t *cmd = search_botcmd_t((const botcmd_t*)&C_share, code, lengthof(C_share) - 1);
  if (cmd) {
    /* Found a match */
    (cmd->func) (idx, msg);
  }
}

void
shareout(const char *format, ...)
{
  char s[601] = "";
  int l;
  va_list va;

  va_start(va, format);

  strlcpy(s, "s ", 3);
  if ((l = egg_vsnprintf(s + 2, 509, format, va)) < 0)
    s[2 + (l = 509)] = 0;
  va_end(va);

  for (int i = 0; i < dcc_total; i++) {
    if (dcc[i].type && (dcc[i].type->flags & DCT_BOT) && 
         (dcc[i].status & STAT_SHARE) && !(dcc[i].status & (STAT_GETTING | STAT_SENDING))) {
      tputs(dcc[i].sock, s, l + 2);
    }
  }
  q_resync(s);
}

void
shareout_prot(struct userrec *u, const char *format, ...)
{
  char s[601] = "";
  int l;
  va_list va;

  va_start(va, format);

  strlcpy(s, "s ", 3);
  if ((l = egg_vsnprintf(s + 2, 509, format, va)) < 0)
    s[2 + (l = 509)] = 0;
  va_end(va);

  int localhub = nextbot(u->handle);

  for (int i = 0; i < dcc_total; i++) {
    if (dcc[i].type && (dcc[i].type->flags & DCT_BOT) && 
       (dcc[i].status & STAT_SHARE) && !(dcc[i].status & (STAT_GETTING | STAT_SENDING)) &&
       /* only send to hubs, the bot itself, or the localhub in the chain */
       /* SA set_write_userfile */
       (dcc[i].hub || dcc[i].user == u || (localhub == -1 || i == localhub))) {
      tputs(dcc[i].sock, s, l + 2);
    }
  }
  q_resync(s);
}

void
shareout_hub(const char *format, ...)
{
  char s[601] = "";
  int l;
  va_list va;

  va_start(va, format);

  strlcpy(s, "s ", 3);
  if ((l = egg_vsnprintf(s + 2, 509, format, va)) < 0)
    s[2 + (l = 509)] = 0;
  va_end(va);

  for (int i = 0; i < dcc_total; i++) {
    if (dcc[i].type && (dcc[i].type->flags & DCT_BOT) &&
       (dcc[i].status & STAT_SHARE) && !(dcc[i].status & (STAT_GETTING | STAT_SENDING)) &&
       /* only send to hubs */
       dcc[i].hub) {
      tputs(dcc[i].sock, s, l + 2);
    }
  }
  q_resync(s);
}

static void
shareout_but(int x, const char *format, ...)
{
  int l;
  char s[601] = "";
  va_list va;

  va_start(va, format);

  strlcpy(s, "s ", 3);
  if ((l = egg_vsnprintf(s + 2, 509, format, va)) < 0)
    s[2 + (l = 509)] = 0;
  va_end(va);

  for (int i = 0; i < dcc_total; i++) {
    if (dcc[i].type && (dcc[i].type->flags & DCT_BOT) && (i != x) &&
        (dcc[i].status & STAT_SHARE) && (!(dcc[i].status & STAT_GETTING)) &&
        (!(dcc[i].status & STAT_SENDING))) {
      tputs(dcc[i].sock, s, l + 2);
    }
  }
  q_resync_but(s, dcc[x].nick);
}

/* Flush all tbufs older than 15 minutes.
 */
static void
check_expired_tbufs()
{
  tandbuf *t = NULL, *tnext = NULL;

  for (t = tbuf; t; t = tnext) {
    tnext = t->next;
    if ((now - t->timer) > 300) {
      putlog(LOG_BOTS, "*", "Flushing resync buffer for clonebot %s.", t->bot);
      del_tbuf(t);
    }
  }
 
  /* Resend userfile requests */
  for (int i = 0; i < dcc_total; i++) {
    if (dcc[i].type && dcc[i].type->flags & DCT_BOT) {
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

/* Erase old user list, switch to new one.
 */
void
finish_share(int idx)
{
  int i, j = -1;

  for (i = 0; i < dcc_total; i++)
    if (dcc[i].type && (dcc[i].type->flags & DCT_BOT) && !strcasecmp(dcc[i].nick, dcc[idx].host)) {
      j = i;
      break;
    }
  if (j == -1)
    return;

  const char salt1[] = SALT1;
  EncryptedStream stream(salt1);
  stream.loadFile(dcc[idx].u.xfer->filename);
  unlink(dcc[idx].u.xfer->filename);
  share_read_stream(j, stream);

/* compress.mod 
  if (!uncompressfile(dcc[idx].u.xfer->filename)) {
    char xx[1024] = "";

    putlog(LOG_BOTS, "*", "A uff parsing function failed for the userfile!");
    unlink(dcc[idx].u.xfer->filename);

    dprintf(j, "bye\n");
    simple_snprintf(xx, sizeof xx, "Disconnected %s (uff error)", dcc[j].nick);
    botnet_send_unlinked(j, dcc[j].nick, xx);
    chatout("*** %s\n", xx);

    killsock(dcc[j].sock);
    lostdcc(j);

    return;
  }
*/
}
static void share_read_stream(int idx, bd::Stream& stream) {
  struct userrec *u = NULL, *ou = NULL;
  struct chanset_t *chan = NULL;

  /*
   * This is where we remove all global and channel bans/exempts/invites and
   * ignores since they will be replaced by what our hub gives us.
   */

  noshare = 1;

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
  //userlist = (struct userrec *) -1;       /* Do this to prevent .user messups     */
  userlist = NULL;

  clear_cached_users();

  struct cmd_pass *old_cmdpass = cmdpass;
  cmdpass = NULL;

  /* Read the transferred userfile. Add entries to u, which already holds
   * the bot entries in non-override mode.
   */
  loading = 1;
  checkchans(0);                /* flag all the channels.. */
  Context;
  if (!stream_readuserfile(stream, &u)) {   /* read the userfile into 'u' */
    /* FAILURE */
    char xx[1024] = "";

    Context;
    clear_userlist(u);          /* Clear new, obsolete, user list.      */

    userlist = ou;              /* Revert to old user list.             */
    lastuser = NULL;            /* Reset last accessed user ptr.        */

    cache_users();
    cmdpass = old_cmdpass;

    checkchans(2);              /* un-flag the channels, we are keeping them.. */

    /* old userlist is now being used, safe to do this stuff... */
    loading = 0;
    putlog(LOG_MISC, "*", "%s", "CAN'T READ NEW USERFILE");
//    dprintf(idx, "bye\n");
    simple_snprintf(xx, sizeof xx, "Disconnected %s (can't read userfile)", dcc[idx].nick);
    botnet_send_unlinked(idx, dcc[idx].nick, xx);
    chatout("*** %s\n", xx);

    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }

  /* SUCCESS! */

  loading = 0;

  userlist = u;                 /* Set new user list.                   */
  lastuser = NULL;              /* Reset last accessed user ptr.        */
  putlog(LOG_BOTS, "*", "%s.", "Userlist transfer complete; switched over");

  /*
   * Migrate:
   *   - unshared (got_share == 0) user entries
   */
  clear_userlist(ou);

  /* The userfile we received may just be bogus or missing important users */
  load_internal_users();
  add_myself_to_userlist();

  if (conf.bot->localhub)
    add_child_bots();

  cache_users();

  /* Make sure no removed users/bots are still connected. */
  check_stale_dcc_users();

  write_userfile(-1);

  cmdpass_free(old_cmdpass);

  checkchans(1);                /* remove marked channels */
  var_parse_my_botset();
  reaffirm_owners();            /* Make sure my owners are +a   */
  updatebot(-1, dcc[idx].nick, '+', 0, 0, 0, NULL, -1);
  send_sysinfo();

  if (restarting && !keepnick) {
    keepnick = 1;
    rehash_monitor_list();
  }

  /* Prevents the server connect from dumping JOIN #chan */
  restarting = 0;

  /* If this is ever changed, do mind the restarting bool as it will prevent 001 from dumping JOINs.. */
  if (reset_chans) {
    if (reset_chans == 2) {
      irc_init();
      putlog(LOG_DEBUG, "*", "Resetting channel info for all channels...");
      for (chan = chanset; chan; chan = chan->next) {
        if (shouldjoin(chan) && channel_pending(chan)) { // Set when reading socksfile
          chan->ircnet_status &= ~(CHAN_PEND); // Reset flags to force a reset
          reset_chan_info(chan);
        }
      }
    } else
      join_chans();
    reset_chans = 0;
  }
}

/* Begin the user transfer process.
 */
static void
ulsend(int idx, const char* data, size_t datalen)
{
  char buf[1040] = "";

  size_t len = simple_snprintf(buf, sizeof(buf), "s l %zu %s", datalen-1, data);/* -1 for newline */
  tputs(dcc[idx].sock, buf, len);
}

static void
stream_send_users(int idx)
{
  bd::Stream stream;
  bool old = 0;
  /* FIXME: Remove after 1.2.15 */
  if (idx != -1 && !(dcc[idx].u.bot->uff_flags & UFF_CHDEFAULT)) /* channel 'default' */
    old = 2;
  stream_writeuserfile(stream, userlist, old);
  stream.seek(0, SEEK_SET);
  dprintf(idx, "s ls\n");
  bd::String buf;
  while (stream.tell() < stream.length()) {
    buf = stream.getline(1024);
    ulsend(idx, buf.c_str(), buf.length());
  }
  dprintf(idx, "s le\n");
  putlog(LOG_BOTS, "*", "Completed userfile transfer to %s.", dcc[idx].nick);
}

static void
start_sending_users(int idx)
{
  char share_file[1024] = "";
  int i = 1, j = -1;

  char rand[7] = "";
  make_rand_str(rand, sizeof(rand) - 1, 0);
  simple_snprintf(share_file, sizeof(share_file), "%s.share.%s", tempdir, rand);
  //mktemp(share_file); //Although safe here, g++ complains too much.

/* FIXME: REMOVE AFTER 1.2.14 */
  bool old = 0;

  tand_t* bot = idx != -1 ? findbot(dcc[idx].nick) : NULL;
  if (bot && bot->buildts < 1175102242) /* flood-* hacks */
    old = 1;

  /* FIXME: Remove after 1.2.15 */
  if (idx != -1 && !(dcc[idx].u.bot->uff_flags & UFF_CHDEFAULT)) /* channel 'default' */
    old = 2;

  const char salt1[] = SALT1;
  EncryptedStream stream(salt1);
  stream_writeuserfile(stream, userlist, old);
  stream.setFlags(ENC_KEEP_NEWLINES|ENC_AES_256_ECB|ENC_BASE64_BROKEN|ENC_NO_HEADER);
  if (stream.writeFile(share_file)) {
    putlog(LOG_MISC, "*", "ERROR writing user file to transfer.");
    unlink(share_file);
  }

/* compress.mod
  if (!compress_file(share_file, compress_level)) {
    unlink(share_file);
    dprintf(idx, "s e %s\n", "uff parsing failed");
    putlog(LOG_BOTS, "*", "uff parsing failed");
    dcc[idx].status &= ~(STAT_SHARE | STAT_SENDING | STAT_AGGRESSIVE);
    return;
  }
*/

  if ((i = raw_dcc_send(share_file, "*users", "(users)", &j)) > 0) {
    /* FIXME: the bot should be unlinked at this point */
    unlink(share_file);
    dprintf(idx, "s e %s\n", "Can't send userfile to you (internal error)");
    putlog(LOG_BOTS, "*", "%s -- can't send userfile",
           i == DCCSEND_FULL ? "NO MORE DCC CONNECTIONS" :
           i == DCCSEND_NOSOCK ? "CAN'T OPEN A LISTENING SOCKET" :
           i == DCCSEND_BADFN ? "BAD FILE" : i == DCCSEND_FEMPTY ? "EMPTY FILE" : "UNKNOWN REASON!");
    dcc[idx].status &= ~(STAT_SHARE | STAT_SENDING | STAT_AGGRESSIVE);
  } else {
    updatebot(-1, dcc[idx].nick, '+', 0, 0, 0, NULL, -1);
    dcc[idx].status |= STAT_SENDING;
    strlcpy(dcc[j].host, dcc[idx].nick, sizeof(dcc[j].host)); /* Store bot's nick */
    dprintf(idx, "s us %lu %d %lu\n", iptolong(getmyip()), dcc[j].port, dcc[j].u.xfer->length);
    /* Start up a tbuf to queue outgoing changes for this bot until the
     * userlist is done transferring.
     */
    new_tbuf(dcc[idx].nick);
    /* override shit removed here */
    q_tbuf(dcc[idx].nick, "s !\n");
    /* Unlink the file. We don't really care whether this causes problems
     * for NFS setups. It's not worth the trouble.
     */
    unlink(share_file);
  }
}

static void (*def_dcc_bot_kill) (int, void *) = 0;

static void
cancel_user_xfer(int idx, void *x)
{
  int i, j = -1;
  if (cancel_user_xfer_staylinked) {
    /* turn off sharing flag */
    updatebot(-1, dcc[idx].nick, '-', 0, 0, 0, NULL, -1);
  }
  flush_tbuf(dcc[idx].nick);

  if (dcc[idx].status & STAT_SHARE) {
    /* look for any transfers from this bot and kill them */
    if (dcc[idx].status & STAT_GETTING) {
      for (i = 0; i < dcc_total; i++)
        if (dcc[i].type && !strcasecmp(dcc[i].host, dcc[idx].nick) &&
            ((dcc[i].type->flags & (DCT_FILETRAN | DCT_FILESEND)) == (DCT_FILETRAN | DCT_FILESEND))) {
          j = i;
          break;
        }
      if (j >= 0) {
        killsock(dcc[j].sock);
        unlink(dcc[j].u.xfer->filename);
        lostdcc(j);
      }
      putlog(LOG_BOTS, "*", "(Userlist download aborted.)");
    }
    /* look for any transfers we were sending them */
    if (dcc[idx].status & STAT_SENDING) {
      for (i = 0; i < dcc_total; i++)
        if (dcc[i].type && (!strcasecmp(dcc[i].host, dcc[idx].nick)) &&
            ((dcc[i].type->flags & (DCT_FILETRAN | DCT_FILESEND)) == DCT_FILETRAN)) {
          j = i;
          break;
        }
      if (j >= 0) {
        killsock(dcc[j].sock);
        unlink(dcc[j].u.xfer->filename);
        lostdcc(j);
      }
      putlog(LOG_BOTS, "*", "(Userlist transmit aborted.)");
    }
  }
  if (!cancel_user_xfer_staylinked)
    def_dcc_bot_kill(idx, x);
 
  cancel_user_xfer_staylinked = 0;
}

void
share_report(int idx, int details)
{
  int i, j;

  if (details) {
    for (i = 0; i < dcc_total; i++)
      if (dcc[i].type && dcc[i].type == &DCC_BOT) {
        if (dcc[i].status & STAT_GETTING) {
          int ok = 0;

          for (j = 0; j < dcc_total; j++)
            if (dcc[j].type && ((dcc[j].type->flags & (DCT_FILETRAN | DCT_FILESEND))
                 == (DCT_FILETRAN | DCT_FILESEND)) && !strcasecmp(dcc[j].host, dcc[i].nick)) {
              dprintf(idx, "Downloading userlist from %s (%d%% done)\n",
                      conf.bot->hub ? dcc[i].nick : "[botnet]", (int) (100.0 * ((float) dcc[j].status) / ((float) dcc[j].u.xfer->length)));
              ok = 1;
              break;
            }
          if (!ok)
            dprintf(idx, "Download userlist from %s (negotiating " "botentries)\n", conf.bot->hub ? dcc[i].nick : "[botnet]");
        } else if (dcc[i].status & STAT_SENDING) {
          for (j = 0; j < dcc_total; j++) {
            if (dcc[j].type && ((dcc[j].type->flags & (DCT_FILETRAN | DCT_FILESEND))
                 == DCT_FILETRAN)
                && !strcasecmp(dcc[j].host, dcc[i].nick)) {
              if (dcc[j].type == &DCC_GET)
                dprintf(idx, "Sending userlist to %s (%d%% done)\n",
                        dcc[i].nick,
                        (int) (100.0 * ((float) dcc[j].status) / ((float) dcc[j].u.xfer->length)));
              else
                dprintf(idx, "Sending userlist to %s (waiting for connect)\n", dcc[i].nick);
            }
          }
        } else if (dcc[i].status & STAT_AGGRESSIVE) {
          dprintf(idx, "    Passively sharing with %s.\n", conf.bot->hub ? dcc[i].nick : "[botnet]");
        } else if (dcc[i].status & STAT_SHARE) {
          dprintf(idx, "    Aggressively sharing with %s.\n", dcc[i].nick);
        }
      }
  }
}

void
share_init()
{
  if (conf.bot->hub)
    timer_create_secs(60, "check_expired_tbufs", (Function) check_expired_tbufs);
  else
    timer_create_secs(1, "check_delay", (Function) check_delay);
  def_dcc_bot_kill = DCC_BOT.kill;
  DCC_BOT.kill = cancel_user_xfer;
}
/* vim: set sts=2 sw=2 ts=8 et: */
