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
 * mode.c -- part of irc.mod
 *   queuing and flushing mode changes made by the bot
 *   channel mode changes and the bot's reaction to them
 *   setting and getting the current wanted channel modes
 *
 */


#include "src/shell.h"

/* Reversing this mode? */
static bool reversing = 0;
static bool massop = 0;
static bool mdop_reversing = 1;

#  define PLUS    BIT0
#  define MINUS   BIT1
#  define CHOP    BIT2
#  define BAN     BIT3
#  define VOICE   BIT4
#  define EXEMPT  BIT5
#  define INVITE  BIT6

static struct flag_record user = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };
static struct flag_record victim = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };

/*        This implementation wont overrun dst - 'max' is the max bytes that dst
 *      can be, including the null terminator. So if 'dst' is a 128 byte buffer,
 *      pass 128 as 'max'. The function will _always_ null-terminate 'dst'.
 *
 *      Returns: The number of characters appended to 'dst'.
 *
 *  Usage eg.
 *
 *              char    buf[128];
 *              size_t  bufsize = sizeof(buf);
 *
 *              buf[0] = 0, bufsize--;
 *
 *              while (blah && bufsize) {
 *                      bufsize -= egg_strcatn(buf, <some-long-string>, sizeof(buf));
 *              }
 *
 *      <Cybah>
 */
static size_t egg_strcatn(char *dst, const char *src, size_t max)
{
  size_t tmpmax = 0;

  /* find end of 'dst' */
  while (*dst && max > 0) {
    dst++;
    max--;
  }

  /*    Store 'max', so we can use it to workout how many characters were
   *  written later on.
   */
  tmpmax = max;

  /* copy upto, but not including the null terminator */
  while (*src && max > 1) {
    *dst++ = *src++;
    max--;
  }

  /* null-terminate the buffer */
  /*    Don't include the terminating null in our count, as it will cumulate
   *  in loops - causing a headache for the caller.
   */
  return tmpmax - max;
}

static bool
do_op(memberlist *m, struct chanset_t *chan, bool delay, bool force)
{
  if (!me_op(chan) || !m || (!force && (chan_hasop(m) || chan_sentop(m))))
    return 0;

  if (delay) {
    m->delay = now + chan->auto_delay;
    m->flags |= SENTOP;
    return 1;
  }

  if (channel_fastop(chan) || channel_take(chan) || cookies_disabled) {
    add_mode(chan, '+', 'o', m);
  } else if (!connect_bursting) {
    add_cookie(chan, m);
  } else {
    /* Failed to give the mode, so requeue it. */
    do_op(m, chan, 1, 0);
  }
  return 1;
}

static void
flush_cookies(struct chanset_t *chan, int pri)
{
  if (connect_bursting) // Make sure not in burst mode
    return;

  char out[512] = "", *p = out, post[512] = "";
  size_t postsize = sizeof(post) - 1;
  memberlist *nicks[3] = { NULL, NULL, NULL };

  chan->cbytes = 0;

  int nick_i = 0;
  for (unsigned int i = 0; i < (modesperline - 1); i++) {
    if (chan->ccmode[i].op && postsize > strlen(chan->ccmode[i].op)) {
      memberlist* mx = ismember(chan, chan->ccmode[i].op);

      /* We queued them, but they might not be in the channel any more! */
      if (mx) {
        /* Build modes .. */
        *p++ = '+';
        *p++ = 'o';

        /* .. and params */
        postsize -= egg_strcatn(post, chan->ccmode[i].op, sizeof(post));
        postsize -= egg_strcatn(post, " ", sizeof(post));
        nicks[nick_i++] = mx;
      }
      free(chan->ccmode[i].op);
      chan->ccmode[i].op = NULL;
    }
  }

  /* remember to terminate the buffer ('out')... */
  if (out[0]) {
    *p++ = '-';
    *p++ = 'b';
  }
  *p = 0;

  if (post[0]) {
    memberlist* me = ismember(chan, botname);

    if (me) {
      me->user = conf.bot->u;
      me->tried_getuser = 1;
    }

    /* Am I even on the channel? */
    if (!me || !me->user)
      return;

    /* remove the trailing space... */
    size_t myindex = (sizeof(post) - 1) - postsize;

    if (myindex > 0 && post[--myindex] == ' ')
      post[myindex] = 0;

    //myindex is how long post is

    egg_strcatn(&out[0], " ", sizeof(out));
    egg_strcatn(&out[1], post, sizeof(out));
    egg_strcatn(&out[myindex + 1], " ", sizeof(out));
    
    myindex += (p - out) + 2;  //(p-out)=outlen + 2 spaces
    makecookie(&out[myindex], sizeof(out) - myindex, chan->dname, me, nicks[0], nicks[1], nicks[2]);
  }
  if (out[0]) {
    // Enabling this creates a queued cookie and a dumped cookie, resulting in out-of-order counters.
    if (pri == QUICK && 0) {
      char outbuf[201] = "";

      const size_t len = simple_snprintf(outbuf, sizeof(outbuf), "MODE %s %s\r\n", chan->name, out);
      dprintf_real(DP_MODE, outbuf, len, sizeof(outbuf));
      /* dprintf(DP_MODE, "MODE %s %s\n", chan->name, out); */
    } else
      dprintf(DP_MODE, "MODE %s %s\n", chan->name, out);
  }
}

static void
flush_mode(struct chanset_t *chan, int pri)
{
  if (!modesperline)            /* Haven't received 005 yet :) */
    return;

  char out[512] = "", *p = out, post[512] = "";
  size_t postsize = sizeof(post) - 1;
  int plus = 2;                 /* 0 = '-', 1 = '+', 2 = none */
  unsigned int i = 0;

  flush_cookies(chan, pri);

/* dequeue_op_deop(chan); */

/* now does +o first.. */
  if (chan->mns[0]) {
    *p++ = '-', plus = 0;
    for (i = 0; i < strlen(chan->mns); i++)
      *p++ = chan->mns[i];
    chan->mns[0] = 0;
  }

  if (chan->pls[0]) {
    *p++ = '+', plus = 1;
    for (i = 0; i < strlen(chan->pls); i++)
      *p++ = chan->pls[i];
    chan->pls[0] = 0;
  }

  chan->bytes = 0;
  chan->compat = 0;

  /* +k or +l ? */
  if (chan->key && !chan->rmkey) {
    if (plus != 1) {
      *p++ = '+', plus = 1;
    }
    *p++ = 'k';

    postsize -= egg_strcatn(post, chan->key, sizeof(post));
    postsize -= egg_strcatn(post, " ", sizeof(post));

    free(chan->key), chan->key = NULL;
  }

  /* max +l is signed 2^32 on IRCnet at least... so makesure we've got at least
   * a 13 char buffer for '-2147483647 \0'. We'll be overwriting the existing
   * terminating null in 'post', so makesure postsize >= 12.
   */
  if (chan->limit != 0 && postsize >= 12) {
    if (plus != 1) {
      *p++ = '+', plus = 1;
    }
    *p++ = 'l';

    /* 'sizeof(post) - 1' is used because we want to overwrite the old null */
    postsize -= simple_sprintf(&post[(sizeof(post) - 1) - postsize], "%d ", chan->limit);

    chan->limit = 0;
  }

  /* -k ? */
  if (chan->rmkey) {
    if (plus) {
      *p++ = '-', plus = 0;
    }
    *p++ = 'k';

    postsize -= egg_strcatn(post, chan->rmkey, sizeof(post));
    postsize -= egg_strcatn(post, " ", sizeof(post));

    free(chan->rmkey), chan->rmkey = NULL;
  }

  /* Do -{b,e,I} before +{b,e,I} to avoid the server ignoring overlaps */
  for (i = 0; i < modesperline; i++) {
    if (chan->cmode[i].op && (chan->cmode[i].type & MINUS) && postsize > strlen(chan->cmode[i].op)) {
      if (plus) {
        *p++ = '-', plus = 0;
      }

      *p++ = ((chan->cmode[i].type & BAN) ? 'b' :
              ((chan->cmode[i].type & CHOP) ? 'o' :
               ((chan->cmode[i].type & EXEMPT) ? 'e' : ((chan->cmode[i].type & INVITE) ? 'I' : 'v'))));

      postsize -= egg_strcatn(post, chan->cmode[i].op, sizeof(post));
      postsize -= egg_strcatn(post, " ", sizeof(post));

      free(chan->cmode[i].op), chan->cmode[i].op = NULL;
      chan->cmode[i].type = 0;
    }
  }

  /* now do all the + modes... */
  for (i = 0; i < modesperline; i++) {
    if (chan->cmode[i].op && (chan->cmode[i].type & PLUS) && postsize > strlen(chan->cmode[i].op)) {
      if (plus != 1) {
        *p++ = '+', plus = 1;
      }

      *p++ = ((chan->cmode[i].type & BAN) ? 'b' :
              ((chan->cmode[i].type & CHOP) ? 'o' :
               ((chan->cmode[i].type & EXEMPT) ? 'e' : ((chan->cmode[i].type & INVITE) ? 'I' : 'v'))));

      postsize -= egg_strcatn(post, chan->cmode[i].op, sizeof(post));
      postsize -= egg_strcatn(post, " ", sizeof(post));

      free(chan->cmode[i].op), chan->cmode[i].op = NULL;
      chan->cmode[i].type = 0;
    }
  }

  /* remember to terminate the buffer ('out')... */
  *p = 0;

  if (post[0]) {
    /* remove the trailing space... */
    size_t myindex = (sizeof(post) - 1) - postsize;

    if (myindex > 0 && post[myindex - 1] == ' ')
      post[myindex - 1] = 0;

    egg_strcatn(out, " ", sizeof(out));
    egg_strcatn(out, post, sizeof(out));
  }
  if (out[0]) {
    if (pri == QUICK) {
/* floods too much
      char outbuf[201] = "";

      const size_t len = simple_snprintf(outbuf, sizeof(outbuf), "MODE %s %s\r\n", chan->name, out);
      tputs(serv, outbuf, len);
*/
      dprintf(DP_MODE, "MODE %s %s\n", chan->name, out);
    } else
      dprintf(DP_SERVER, "MODE %s %s\n", chan->name, out);
  }
}

/* Queue a channel mode change
 */
void
real_add_mode(struct chanset_t *chan, const char plus, const char mode, const char *op, bool cookie, memberlist *mx)
{
  if (!me_op(chan))
    return;

  if (mode == 'o' || mode == 'v') {
    if (!mx)
      mx = ismember(chan, op);
    if (!mx)
      return;
    if (plus == '-' && mode == 'o') {
      if (chan_sentdeop(mx) || !chan_hasop(mx))
        return;
      mx->flags |= SENTDEOP;
    }
    if (plus == '+' && mode == 'o') {
      if (chan_sentop(mx) || chan_hasop(mx))
        return;
      mx->flags |= SENTOP;
    }
    if (plus == '-' && mode == 'v') {
      if (chan_sentdevoice(mx) || !chan_hasvoice(mx))
        return;
      mx->flags |= SENTDEVOICE;
    }
    if (plus == '+' && mode == 'v') {
      if (chan_sentvoice(mx) || chan_hasvoice(mx))
        return;
      mx->flags |= SENTVOICE;
    }
  }

  int type, modes;
  size_t len = 0;
  unsigned int i;
  masklist *m = NULL;
  char s[21] = "";

  if (chan->compat == 0) {
    if (mode == 'e' || mode == 'I')
      chan->compat = 2;
    else
      chan->compat = 1;
  } else if (mode == 'e' || mode == 'I') {
    if (prevent_mixing && chan->compat == 1)
      flush_mode(chan, NORMAL);
  } else if (prevent_mixing && chan->compat == 2)
    flush_mode(chan, NORMAL);

  if (mode == 'o' || mode == 'b' || mode == 'v' || mode == 'e' || mode == 'I') {
    type = (plus == '+' ? PLUS : MINUS) |
      (mode == 'o' ? CHOP : (mode == 'b' ? BAN : (mode == 'v' ? VOICE : (mode == 'e' ? EXEMPT : INVITE))));

    /*
     * FIXME: Some networks remove overlapped bans,
     *        IRCnet does not (poptix/drummer)
     *
     * Note:  On IRCnet ischanXXX() should be used, otherwise isXXXed().
     */
    if ((plus == '-' && ((mode == 'b' && !ischanban(chan, op)) ||
                         (mode == 'e' && !ischanexempt(chan, op)) ||
                         (mode == 'I' && !ischaninvite(chan, op)))) || (plus == '+' &&
                                                                        ((mode == 'b' && ischanban(chan, op))
                                                                         || (mode == 'e' &&
                                                                             ischanexempt(chan, op)) ||
                                                                         (mode == 'I' &&
                                                                          ischaninvite(chan, op)))))
      return;

    /* If there are already max_bans bans, max_exempts exemptions,
     * max_invites invitations or max_modes +b/+e/+I modes on the
     * channel, don't try to add one more.
     */
    if (plus == '+' && (mode == 'b' || mode == 'e' || mode == 'I')) {
      int bans = 0, exempts = 0, invites = 0;

      for (m = chan->channel.ban; m && m->mask[0]; m = m->next)
        bans++;
      if ((mode == 'b') && (bans >= max_bans))
        return;

      for (m = chan->channel.exempt; m && m->mask[0]; m = m->next)
        exempts++;
      if ((mode == 'e') && (exempts >= max_exempts))
        return;

      for (m = chan->channel.invite; m && m->mask[0]; m = m->next)
        invites++;
      if ((mode == 'I') && (invites >= max_invites))
        return;

      if (bans + exempts + invites >= max_modes)
        return;
    }

    /* op-type mode change */
    /* for cookie ops, use ccmode instead of cmode */
    if (cookie) {
      for (i = 0; i < (modesperline - 1); i++)
        if (chan->ccmode[i].op != NULL && !rfc_casecmp(chan->ccmode[i].op, op))
          return;               /* Already in there :- duplicate */
      len = strlen(op) + 1;
      if (chan->cbytes + len > mode_buf_len)
        flush_mode(chan, NORMAL);
      for (i = 0; i < (modesperline - 1); i++)
        if (!chan->ccmode[i].op) {
          chan->ccmode[i].op = (char *) calloc(1, len);
          chan->cbytes += len;    /* Add 1 for safety */
          strlcpy(chan->ccmode[i].op, op, len);
          break;
        }
    } else {
      for (i = 0; i < modesperline; i++)
        if (chan->cmode[i].type == type && chan->cmode[i].op != NULL && !rfc_casecmp(chan->cmode[i].op, op))
          return;               /* Already in there :- duplicate */
      len = strlen(op) + 1;
      if (chan->bytes + len > mode_buf_len)
        flush_mode(chan, NORMAL);
      for (i = 0; i < modesperline; i++)
        if (chan->cmode[i].type == 0) {
          chan->cmode[i].type = type;
          chan->cmode[i].op = (char *) calloc(1, len);
          chan->bytes += len;     /* Add 1 for safety */
          strlcpy(chan->cmode[i].op, op, len);
          break;
        }
    }
  }

  /* +k ? store key */
  else if (plus == '+' && mode == 'k') {
    if (chan->key)
      free(chan->key);
    len = strlen(op) + 1;
    chan->key = (char *) calloc(1, len);
    strlcpy(chan->key, op, len);
  }
  /* -k ? store removed key */
  else if (plus == '-' && mode == 'k') {
    if (chan->rmkey)
      free(chan->rmkey);
    len = strlen(op) + 1;
    chan->rmkey = (char *) calloc(1, len);
    strlcpy(chan->rmkey, op, len);
  }
  /* +l ? store limit */
  else if (plus == '+' && mode == 'l')
    chan->limit = atoi(op);
  else {
    /* Typical mode changes */
    if (plus == '+')
      strlcpy(s, chan->pls, sizeof(s));
    else
      strlcpy(s, chan->mns, sizeof(s));
    if (!strchr(s, mode)) {
      if (plus == '+') {
        chan->pls[strlen(chan->pls) + 1] = 0;
        chan->pls[strlen(chan->pls)] = mode;
      } else {
        chan->mns[strlen(chan->mns) + 1] = 0;
        chan->mns[strlen(chan->mns)] = mode;
      }
    }
  }
  modes = modesperline;         /* Check for full buffer. */
  for (i = 0; i < modesperline; i++)
    if (chan->cmode[i].type)
      modes--;
  if (include_lk && chan->limit)
    modes--;
  if (include_lk && chan->rmkey)
    modes--;
  if (include_lk && chan->key)
    modes--;
  if (modes < 1)
    flush_mode(chan, NORMAL);   /* Full buffer! Flush modes. */
 
  /* flush full cookie queue */
  modes = modesperline - 1;
  for (i = 0; i < (modesperline - 1); i++)
    if (chan->ccmode[i].op)
      modes--;
  if (modes < 1)
    flush_cookies(chan, NORMAL);
}

/*
 *    Mode parsing functions
 */

static void
got_key(struct chanset_t *chan, char *key)
{
  if (((reversing) && !(chan->key_prot[0])) ||
      ((chan->mode_mns_prot & CHANKEY) && !(glob_master(user) || glob_bot(user) || chan_master(user)))) {
    if (key && key[0]) {
      add_mode(chan, '-', 'k', key);
    } else {
      add_mode(chan, '-', 'k', "");
    }
  }
}

  /* m may not exist */
static void
got_op(struct chanset_t *chan, memberlist *m, memberlist *mv)
{
  bool check_chan = 0;
  const bool me_opped = mv->is_me;
  bool meop = me_op(chan);
  const bool was_op = chan_hasop(mv);

  // No need to punish someone who was already opped.
  if (massop && was_op)
    return;

  /* Did *I* just get opped? */
  if (!meop && me_opped) {
    check_chan = 1;
    meop = 1;
  }

  get_user_flagrec(mv->user, &victim, chan->dname, chan);

  // Did some other bot just get opped, and I'm not opped yet?
  if (mv->user && mv->user->bot && !me_opped && !meop) {
    chan->channel.do_opreq = 1;
  }

  /* Flags need to be set correctly right from the beginning now, so that
   * add_mode() doesn't get irritated.
   */
  mv->flags |= CHANOP;
  /* Added new meaning of WASOP:
   * in mode binds it means: was he op before get (de)opped
   * (stupid IrcNet allows opped users to be opped again and
   *  opless users to be deopped)
   * script now can use [wasop nick chan] proc to check
   * if user was op or wasnt  (drummer)
   */
  mv->flags &= ~SENTOP;

  /* This skips the recheck_channel until we get the end of the WHO list */
  if (channel_pending(chan))
    return;

  /* I'm opped, and the opper isn't me, and it isn't a server op */
  if (m && meop && !me_opped) {
    /* deop if they are +d or it is +bitch */
    int bitch = chan_bitch(chan);

    if (reversing || chk_deop(victim, chan) || (!loading && userlist && bitch && !chk_op(victim, chan))) {     /* chk_op covers +private */
      int num = randint(10);
      char outbuf[101] = "";
      size_t len = 0;
/* should kick the oppee first, then deal with the opper */

#ifdef old
      if (num == 4) {
        len = simple_snprintf(outbuf, sizeof(outbuf), "MODE %s -o %s\r\n", chan->name, mv->nick);
        mv->flags |= SENTDEOP;
      } else if (num == 5) {
        len = simple_snprintf(outbuf, sizeof(outbuf), "MODE %s -o %s\r\n", chan->name, m->nick);
        m->flags |= SENTDEOP;
#endif
      if (num == 5) {
        add_mode(chan, '-', 'o', m);
      } else if (bitch && num == 6) {
        len = simple_snprintf(outbuf, sizeof(outbuf), "KICK %s %s :%s\r\n", chan->name, mv->nick, response(RES_BITCHOPPED));
        mv->flags |= SENTKICK;
      } else if (bitch && num == 7) {
        len = simple_snprintf(outbuf, sizeof(outbuf), "KICK %s %s :%s\r\n", chan->name, m->nick, response(RES_BITCHOP));
        m->flags |= SENTKICK;
      } else
        add_mode(chan, '-', 'o', mv);

      if (len)
        dprintf_real(DP_MODE_NEXT, outbuf, len, sizeof(outbuf));
    }


  } else if (reversing && !me_opped)
    add_mode(chan, '-', 'o', mv);

  /* server op */
  if (!m && meop && !me_opped) {
    if (chk_deop(victim, chan) || (chan_bitch(chan) && !chk_op(victim, chan))) {
      mv->flags |= FAKEOP;
      add_mode(chan, '-', 'o', mv);
    } 
  }
  mv->flags |= WASOP;

  if (check_chan) {
#ifdef no
    /* tell other bots to set jointime to 0 and join */
    char *buf = NULL;
    size_t siz = strlen(chan->dname) + 3 + 1;

    buf = calloc(1, siz);
    simple_snprintf(buf, siz, "jn %s", chan->dname);
    putallbots(buf);
    free(buf);
#endif

    /* check exempts/invites and shit */
    recheck_channel(chan, 2);
  }
}

static void
got_deop(struct chanset_t *chan, memberlist *m, memberlist *mv, char *isserver)
{
  const bool was_op = chan_hasop(mv);

  if (mdop_reversing && !was_op)
    return;

  /* m is NULL if a server made the change */
  get_user_flagrec(mv->user, &victim, chan->dname, chan);

  /* Flags need to be set correctly right from the beginning now, so that
   * add_mode() doesn't get irritated.
   */
  mv->flags &= ~(CHANOP | SENTDEOP | FAKEOP);
  /* Check comments in got_op()  (drummer) */
  mv->flags &= ~WASOP;

  if (channel_pending(chan))
    return;

  /* Deop'd someone on my oplist? */
  if (me_op(chan)) {
    /* do we want to reop victim? */
    if (
        /* I didn't deop them, another bot didn't deop them and they didn't deop themselves. */
        (was_op && ((m && !m->is_me && mv != m && !(m->user && m->user->bot)) || (!m))) && (
          /*
           * reversing
           * They are either an op or this chan is -bitch
           */
          (reversing && (!chan_bitch(chan) || chk_op(victim, chan))) ||
          /* Reop bots to avoid them needing to ask */
          ((chan->role & ROLE_PROTECT) && mv->user && mv->user->bot && chk_op(victim, chan))
        )
       ) {
      /* Then we'll bless the victim */
      do_op(mv, chan, 0, 0);
    }
  }

  if (isserver[0])		/* !m */
    putlog(LOG_MODES, chan->dname, "TS resync (%s): %s deopped by %s", chan->dname, mv->nick, isserver);
  /* Check for mass deop */
  else if (m) {
    detect_chan_flood(m, m->from, chan, FLOOD_DEOP, mv->nick);
  }
  /* Having op hides your +v and +h  status -- so now that someone's lost ops,
   * check to see if they have +v or +h
   */
// WHO CARES
//  if (!channel_take(chan) && !chan_bitch(chan) && !(mv->flags & (CHANVOICE | STOPWHO))) {
//    send_chan_who(DP_HELP, chan);
//    mv->flags |= STOPWHO;
//  }
  /* Was the bot deopped? */
  if (mv->is_me) {
    /* Cancel any pending kicks and modes */
    memberlist *m2 = NULL;

    for (m2 = chan->channel.member; m2 && m2->nick[0]; m2 = m2->next)
      m2->flags &= ~(SENTKICK | SENTDEOP | SENTOP | SENTVOICE | SENTDEVOICE);

    chan->channel.do_opreq = 1;
/*    request_op(chan); */
/* need: op */
    if (!m)
      putlog(LOG_MODES, chan->dname, "TS resync deopped me on %s :(", chan->dname);
  } else {
    // Revenge kick clients that deop our bots
    if (chan->revenge && m && m != mv && mv->user && mv->user->bot && !(m->user && m->user->bot)) {
      if ((chan->role & ROLE_REVENGE) && !chan_sentkick(m) && me_op(chan)) {
        m->flags |= SENTKICK;
        dprintf(DP_MODE_NEXT, "KICK %s %s :%s%s\r\n", chan->name, m->nick, kickprefix, response(RES_REVENGE));
      } else {
        if (m->user) {
          char tmp[128] = "";
          simple_snprintf(tmp, sizeof(tmp), "Deopped bot %s on %s", mv->nick, chan->dname);
          deflag_user(m->user, DEFLAG_EVENT_REVENGE_DEOP, tmp, chan);
        }
      }
    }
  }
}

static void
got_ban(struct chanset_t *chan, memberlist *m, char *mask, char *isserver)
{
  const memberlist *me = ismember(chan, botname);
  char s[UHOSTLEN] = "";

  simple_snprintf(s, sizeof s, "%s!%s", m ? m->nick : "", m ? m->userhost : isserver);
  if (m && newban(chan, mask, m->from))
    return; /* The ban was already set, don't bother with it */

  if (channel_pending(chan) || !me_op(chan))
    return;

  // Make an array of all matching users
  bd::Array<memberlist*> matchedUserMembers;
  memberlist *matched_bot = NULL;

  for (memberlist *mv = chan->channel.member; mv && mv->nick[0]; mv = mv->next) {
    member_getuser(mv);
    if (mv->user) {
      if ((wild_match(mask, mv->from) || match_cidr(mask, mv->from))) {
        if (!matched_bot && mv != m && mv->user->bot) {
          matched_bot = mv;
        }
        if (mv->user && !isexempted(chan, mv->from)) {
          matchedUserMembers << mv;
        }
      }
    }
  }

  // Revenge kick clients that ban our bots
  if (chan->revenge && m && matched_bot && !(m->user && m->user->bot)) {
    if ((chan->role & ROLE_REVENGE) && !chan_sentkick(m)) {
      m->flags |= SENTKICK;
      dprintf(DP_MODE_NEXT, "KICK %s %s :%s%s\r\n", chan->name, m->nick, kickprefix, response(RES_REVENGE));
    } else {
      if (m->user) {
        char tmp[128] = "";
        simple_snprintf(tmp, sizeof(tmp), "Banned bot %s (%s) on %s", matched_bot->nick, mask, chan->dname);
        deflag_user(m->user, DEFLAG_EVENT_REVENGE_BAN, tmp, chan);
      }
    }
  }

  if ((wild_match(mask, me->from) || match_cidr(mask, me->fromip)) && !isexempted(chan, me->from)) {
    add_mode(chan, '-', 'b', mask);
    reversing = 1;
    return;
  }

  if (m && !m->is_me) {
    if (channel_nouserbans(chan) && !glob_bot(user)) {
      add_mode(chan, '-', 'b', mask);
      return;
    }
    /* remove bans on ops unless a master/bot set it */

    for (size_t n = 0; n < matchedUserMembers.size(); ++n) {
      const memberlist *mv = matchedUserMembers[n];
      get_user_flagrec(mv->user, &victim, chan->dname, chan);
      if (!(glob_kick(victim) || chan_kick(victim)) &&
          (((chk_op(victim, chan) && !chan_master(user) && !glob_master(user) && !glob_bot(user)) ||
            (mv->user->bot && findbot(mv->user->handle))))) {
        /* if (target_priority(chan, m, 0)) */
        add_mode(chan, '-', 'b', mask);
        return;
      }
    }
  }
  refresh_exempt(chan, mask);
  /* This looks for bans added through bot and tacks on banned: if a description is found */
  if (m && channel_enforcebans(chan)) {
    maskrec *b = NULL;
    char resn[512] = "";

    /* The point of this cycle crap is to first check chan->bans then global_bans for a reason */
    for (int cycle = 0; cycle < 2; cycle++) {
      for (b = cycle ? chan->bans : global_bans; b; b = b->next) {
        if (wild_match(b->mask, mask) || match_cidr(b->mask, mask)) {
          if (b->desc && b->desc[0] != '@')
            simple_snprintf(resn, sizeof resn, "banned: %s", b->desc);
          else
            resn[0] = 0;
        }
      }
    }
    kick_all(chan, mask, resn[0] ? (const char *) resn : r_banned(chan), m->is_me ? 0 : 1);
  }
  if (!m && (bounce_bans || bounce_modes) &&
      (!u_equals_mask(global_bans, mask) || !u_equals_mask(chan->bans, mask)))
    add_mode(chan, '-', 'b', mask);
}

static void
got_unban(struct chanset_t *chan, memberlist *m, char *mask)
{
  masklist *b = NULL, *old = NULL;

  for (b = chan->channel.ban; b->mask[0] && rfc_casecmp(b->mask, mask); old = b, b = b->next) ;
  if (b->mask[0]) {
    if (old)
      old->next = b->next;
    else
      chan->channel.ban = b->next;
    free(b->mask);
    free(b->who);
    free(b);
  } else {
    /* The ban was not even set, why continue? */
    return;
  }

  if (channel_pending(chan))
    return;

  if (u_sticky_mask(chan->bans, mask) || u_sticky_mask(global_bans, mask)) {
    /* That's a sticky ban! No point in being
     * sticky unless we enforce it!!
     */
    add_mode(chan, '+', 'b', mask);
  }
  if ((u_equals_mask(global_bans, mask) || u_equals_mask(chan->bans, mask)) &&
      me_op(chan) && !channel_dynamicbans(chan)) {
    /* That's a permban! */
    if (!glob_bot(user) && !chk_op(user, chan))
      add_mode(chan, '+', 'b', mask);
  }
}

static void
got_exempt(struct chanset_t *chan, memberlist *m, char *mask, char *isserver)
{
  char s[UHOSTLEN] = "";

  simple_snprintf(s, sizeof(s), "%s!%s", m ? m->nick : "", m ? m->userhost : isserver);
  if (newexempt(chan, mask, s))
    return; /* The exempt was already set, don't bother with it */

  if (channel_pending(chan))
    return;

  if (m && !m->is_me) {   /* It's not my exemption */
    if (channel_nouserexempts(chan) && !glob_bot(user) && !glob_master(user) && !chan_master(user)) {
      /* No exempts made by users */
      add_mode(chan, '-', 'e', mask);
      return;
    }
  }
  if (reversing || (!m && bounce_exempts &&
                    (!u_equals_mask(global_exempts, mask) || !u_equals_mask(chan->exempts, mask))))
    add_mode(chan, '-', 'e', mask);
}

static void
got_unexempt(struct chanset_t *chan, memberlist *m, char *mask)
{
  masklist *e = chan->channel.exempt, *old = NULL;
  masklist *b = NULL;
  int match = 0;

  while (e && e->mask[0] && rfc_casecmp(e->mask, mask)) {
    old = e;
    e = e->next;
  }
  if (e && e->mask[0]) {
    if (old)
      old->next = e->next;
    else
      chan->channel.exempt = e->next;
    free(e->mask);
    free(e->who);
    free(e);
  } else {
    /* The exempt was not even set, why continue? */
    return;
  }

  if (channel_pending(chan))
    return;

  if (u_sticky_mask(chan->exempts, mask) || u_sticky_mask(global_exempts, mask)) {
    /* That's a sticky exempt! No point in being sticky unless we enforce it!!
     */
    add_mode(chan, '+', 'e', mask);
  }
  /* If exempt was removed by master then leave it else check for bans */
  /* FIXME: this is impossible, if server !isbot ? */
  if (!m && glob_bot(user) && !glob_master(user) && !chan_master(user)) {
    b = chan->channel.ban;
    while (b->mask[0] && !match) {
      if (wild_match(b->mask, mask) || wild_match(mask, b->mask)) {
        add_mode(chan, '+', 'e', mask);
        match = 1;
      } else
        b = b->next;
    }
  }
  if ((u_equals_mask(global_exempts, mask) || u_equals_mask(chan->exempts, mask)) &&
      me_op(chan) && !channel_dynamicexempts(chan) && !glob_bot(user))
    add_mode(chan, '+', 'e', mask);

  if (channel_enforcebans(chan))
    enforce_bans(chan);
}

static void
got_invite(struct chanset_t *chan, memberlist *m, char *mask, char *isserver)
{
  char s[UHOSTLEN] = "";

  simple_snprintf(s, sizeof(s), "%s!%s", m ? m->nick : "", m ? m->userhost : isserver);
  if (newinvite(chan, mask, s))
    return; /* The Invite was already set, don't bother with it */

  if (channel_pending(chan))
    return;

  if (m && !m->is_me) {   /* It's not my invitation */
    if (channel_nouserinvites(chan) && !glob_bot(user) && !glob_master(user) && !chan_master(user)) {
      /* No exempts made by users */
      add_mode(chan, '-', 'I', mask);
      return;
    }
  } 

  if (reversing || (bounce_invites && !m &&
                    (!u_equals_mask(global_invites, mask) || !u_equals_mask(chan->invites, mask))))
    add_mode(chan, '-', 'I', mask);
}

static void
got_uninvite(struct chanset_t *chan, memberlist *m, char *mask)
{
  masklist *inv = chan->channel.invite, *old = NULL;

  while (inv->mask[0] && rfc_casecmp(inv->mask, mask)) {
    old = inv;
    inv = inv->next;
  }
  if (inv->mask[0]) {
    if (old)
      old->next = inv->next;
    else
      chan->channel.invite = inv->next;
    free(inv->mask);
    free(inv->who);
    free(inv);
  } else {
    /* The Invite was not even set, why continue? */
    return;
  }

  if (channel_pending(chan))
    return;

  if (u_sticky_mask(chan->invites, mask) || u_sticky_mask(global_invites, mask)) {
    /* That's a sticky invite! No point in being sticky unless we enforce it!!
     */
    add_mode(chan, '+', 'I', mask);
  }
  if (!m && glob_bot(user) && !glob_master(user) && !chan_master(user) && (chan->channel.mode & CHANINV))
    add_mode(chan, '+', 'I', mask);
  if ((u_equals_mask(global_invites, mask) ||
       u_equals_mask(chan->invites, mask)) && me_op(chan) && !channel_dynamicinvites(chan) && !glob_bot(user))
    add_mode(chan, '+', 'I', mask);
}

static memberlist *assert_ismember(struct chanset_t *chan, const char *nick)
{
  memberlist *m = ismember(chan, nick);

  if (m) {
    member_getuser(m);
  } else {
    if (channel_pending(chan))
      return NULL;
    putlog(LOG_MISC, chan->dname, "* Mode change on %s for nonexistant %s!", chan->dname, nick);
    send_chan_who(DP_MODE, chan);
  }

  return m;
}

static int
gotmode(char *from, char *msg)
{
#define msign	modes[i][0]
#define mmode	modes[i][1]
/* 2 = space */
#define mparam	&modes[i][3]
  /* Usermode changes? */
  if (msg[0] && (strchr(CHANMETA, msg[0]) != NULL)) {
    char *ch = newsplit(&msg);

    struct chanset_t *chan = findchan(ch);

    if (unlikely(!chan)) {
      if (unlikely(match_my_nick(ch)))
        return 0;

      putlog(LOG_MISC, "*", "Oops.   Someone made me join %s... leaving...", ch);
      dprintf(DP_SERVER, "PART %s\n", ch);
      return 0;
    }

    /* let's pre-emptively check for mass op/deop, manual ops and cookieops */

    if ((channel_active(chan) || channel_pending(chan))) {
      char isserver[100] = "";
      size_t z = strlen(msg);
      struct userrec *u = NULL;
      memberlist *m = NULL;
      char *nick = NULL;

      if (!strchr(from, '!'))
        strlcpy(isserver, from, sizeof(isserver));

      if (msg[--z] == ' ')      /* I hate cosmetic bugs :P -poptix */
        msg[z] = 0;

      /* Split up from */
      if (!isserver[0]) {
        nick = splitnick(&from);
        if ((m = ismember(chan, nick))) {
          m->last = now;
          member_getuser(m);
          u = m->user;
        } else {
          if (channel_pending(chan))
            return 0;
          dprintf(DP_MODE, "KICK %s %s :Desync\n", chan->name[0] ? chan->name : chan->dname, nick);
          putlog(LOG_MISC, chan->dname, "* Mode change on %s from nonexistant %s!", chan->dname, nick);
          send_chan_who(DP_MODE, chan);
          return 0;
        }
      } else
        nick = splitnick(&from);

      int i = 0, modecnt = 0, ops = 0, deops = 0, bans = 0, unbans = 0;
      bool me_opped = 0;
      char **modes = (char **) calloc(1, sizeof(char *));
      char s[UHOSTLEN] = "";
      memberlist *mv = NULL;


      massop = reversing = 0;

      irc_log(chan, "%s!%s sets mode: %s", nick, from, msg);
      get_user_flagrec(u, &user, ch);


      if (1) { // Place in block to hint chg/sign to be destroyed when done
        /* Split up the mode: #chan modes param param param param */
        char *chg = newsplit(&msg);
        char sign = 0;
        while (*chg) {            /* +MODES PARAM PARAM PARAM ... */
          if (strchr("+-", *chg))
            sign = *chg;
          else {
            char* mp = NULL;

            if (strchr("beIlkov", chg[0])) {
              mp = newsplit(&msg);       /* PARAM as noted above */
              fixcolon(mp);
            }

            /* Just want o's and b's */
            modes = (char **) realloc(modes, (modecnt * sizeof(char *)) + sizeof(char *));
            //      char **modes = (char **) calloc(modesperline + 1, sizeof(char *));

            size_t siz = (mp ? strlen(mp) : 0) + 3 + 1;
            modes[modecnt] = (char *) calloc(1, siz);
            simple_snprintf(modes[modecnt], siz, "%c%c %s", sign, chg[0], mp ? mp : "");

            ++modecnt;
            if (chg[0] == 'o') {
              if (sign == '+') {
                ++ops;
                if (match_my_nick(mp))
                  me_opped = 1;
              } else {
                deops++;
                if (match_my_nick(mp))
                  me_opped = 0;
              }
            } else if (chg[0] == 'b') {
              if (sign == '+')
                ++bans;
              else
                ++unbans;
            }
          }
          ++chg;
        }
      }

      /* take ASAP */
      if (me_opped && !me_op(chan) && channel_take(chan))
        do_take(chan);

      /* Now we got modes[], chan, u, nick, and count of each relevant mode */

      /* check for mdop */
      if (me_op(chan)) {
        char tmp[1024] = "";

        if (!isserver[0] && chan->role && (!u || (u && !u->bot))) {
          if (m && deops >= 3) {
            if (chan->mdop) {
              if ((chan->role & ROLE_PROTECT) && !chan_sentkick(m)) {
                m->flags |= SENTKICK;
                const size_t len = simple_snprintf(tmp, sizeof(tmp), "KICK %s %s :%s%s\r\n", chan->name, m->nick, kickprefix, response(RES_MASSDEOP));
                dprintf_real(DP_MODE_NEXT, tmp, len, sizeof(tmp));
              } else {
                if (u) {
                  simple_snprintf(tmp, sizeof(tmp), "Mass deop on %s by %s", chan->dname, m->nick);
                  deflag_user(u, DEFLAG_EVENT_MDOP, tmp, chan);
                }
              }
              reversing = mdop_reversing = 1;
            }
            if (channel_protect(chan))
              do_protect(chan, "Mass Deop");
          }

          /* check for mop */
          if (ops >= 3) {
            if (chan->mop) {
              if (m && !chan_sentkick(m)) {
                if ((chan->role & ROLE_PROTECT)) {
                  m->flags |= SENTKICK;
                  const size_t len = simple_snprintf(tmp, sizeof(tmp), "KICK %s %s :%s%s\r\n", chan->name, m->nick, kickprefix, response(RES_MANUALOP));
                  dprintf_real(DP_MODE_NEXT, tmp, len, sizeof(tmp));
                } else { 
                  if (u) {
                    simple_snprintf(tmp, sizeof(tmp), "Mass op on %s by %s", chan->dname, m->nick);
                    deflag_user(u, DEFLAG_EVENT_MOP, tmp, chan);
                  }
                }
              }
              // Don't mass deop if protect is set, it'll happen anyway below
              if (channel_bitch(chan) && !channel_protect(chan))
                enforce_bitch(chan);        /* deop quick! */
              else
                reversing = massop = 1; // Reverse via got_op
            }
            if (channel_protect(chan))
              do_protect(chan, "Mass OP");
          }
        }
        if (ops) {
          /* Check cookies */
          if (u && m && u->bot && !channel_fastop(chan) && !channel_take(chan) && !cookies_disabled) {
            int isbadop = 0;
            bool failure = 0;

            /* If no unbans or the -b is not the LAST mode, it's bad. */
            if (unbans != 1 || (strncmp(modes[modecnt - 1], "-b", 2))) {
              isbadop = BC_NOCOOKIE;
              failure = 1;
            } else {
              /* Check the hash for each opped nick and punish the opped client if it fails
               * Punish the opper lastly (and once)
               */
              for (i = 0; i < (modecnt - 1); i++) { /* Don't need to hit the -b */
                if (msign == '+' && mmode == 'o') {
                  mv = assert_ismember(chan, mparam);
                  // Unknown client - I am desycned, don't punish or it may lead to fight
                  if (!mv) continue;

                  const char *cookie = &(modes[modecnt - 1][3]);
                  if ((isbadop = checkcookie(chan->dname, m, mv, cookie, i))) {
                    //if (!failure) { /* First failure */
                    failure = 1;
                    //}

                    /* Kick the opped client */
                    if (randint(7) == (unsigned int) i) {
                      if (!mv || !chan_sentkick(mv)) {
                        if (mv)
                          mv->flags |= SENTKICK;
                        const size_t len = simple_snprintf(tmp, sizeof(tmp), "KICK %s %s :%s%s\r\n", chan->name, mparam, kickprefix, response(RES_BADOPPED));
                        dprintf_real(DP_MODE_NEXT, tmp, len, sizeof(tmp));
                      }
                    }
                  }
                }
              }
            }

            if (unlikely(failure)) { /* One of the hashes failed! */
              /* Did *I* do this heinous act? */
              if (m->is_me) {
                detected(DETECT_HIJACK, "Possible Hijack: bad cookie");
              }
              if (randint(7) == 0) {
                /* Kick opper */
                if (!chan_sentkick(m)) {
                  m->flags |= SENTKICK;
                  const size_t len = simple_snprintf(tmp, sizeof(tmp), "KICK %s %s :%s%s\r\n", chan->name, m->nick, kickprefix, response(RES_BADOP));
                  dprintf_real(DP_MODE_NEXT, tmp, len, sizeof(tmp));
                }
                simple_snprintf(tmp, sizeof(tmp), "%s MODE %s %s", m->from, chan->dname, modes[modecnt - 1]);
                deflag_user(u, DEFLAG_EVENT_BADCOOKIE, tmp, chan);
              }
              /* Do the logging last as it can slow down the KICK pushing */
              putlog(LOG_WARNING, "*", "%s opped in %s with bad cookie(%d): %s", m->nick, chan->dname, isbadop, msg);
              if (isbadop == BC_NOCOOKIE)
                putlog(LOG_WARN, "*", "Missing cookie: %s!%s MODE %s %s", 
                    m->nick, m->userhost, chan->dname, modes[modecnt - 1]);
              else if (isbadop == BC_HASH)
                putlog(LOG_WARN, "*", "Invalid cookie (bad hash): %s!%s MODE %s %s", 
                    m->nick, m->userhost, chan->dname, modes[modecnt - 1]);
              else if (isbadop == BC_SLACK)
                putlog(LOG_WARN, "*", "Invalid cookie (bad time): %s!%s MODE %s %s", 
                    m->nick, m->userhost, chan->dname, modes[modecnt - 1]);
              else if (isbadop == BC_COUNTER)
                putlog(LOG_WARN, "*", "Invalid cookie (bad count): %s!%s MODE %s %s",
                    m->nick, m->userhost, chan->dname, modes[modecnt - 1]);
            } 
#ifdef DEBUG
            else
              putlog(LOG_DEBUG, "@", "Good op: %s", modes[modecnt - 1]);
#endif
          }

          /* manop */
	  if (chan->manop && u && !u->bot) {

	    if (m && (chan->role & ROLE_PROTECT)) {
	      /* Kick opper */
	      if (!chan_sentkick(m)) {
		const size_t len = simple_snprintf(tmp, sizeof(tmp), "KICK %s %s :%s%s\r\n", chan->name, m->nick, kickprefix, response(RES_MANUALOP));
		dprintf_real(DP_MODE_NEXT, tmp, len, sizeof(tmp));
		m->flags |= SENTKICK;
	      }
	      simple_snprintf(tmp, sizeof(tmp), "%s MODE %s %s", m->from, chan->dname, modes[modecnt - 1]);
	      deflag_user(u, DEFLAG_EVENT_MANUALOP, tmp, chan);
	    }

	    if (chan->role & ROLE_KICK) {
	      for (i = 0; i < modecnt; i++) {
		if (msign == '+' && mmode == 'o' && !match_my_nick(mparam)) {
		  mv = ismember(chan, mparam);
		  if (!mv || !chan_sentkick(mv)) {
		    if (mv)
		      mv->flags |= SENTKICK;
		    const size_t len = simple_snprintf(tmp, sizeof(tmp), "KICK %s %s :%s%s\r\n", chan->name, mparam, kickprefix, response(RES_MANUALOPPED));
		    dprintf_real(DP_MODE_NEXT, tmp, len, sizeof(tmp));
		  }
		}
	      }
	    }
          }
        }
      }
      /* Now do the modes again, this time thoroughly... */

      if (m && channel_active(chan) && me_op(chan)) {
        if (chan_fakeop(m)) {
          putlog(LOG_MODES, ch, "Mode change by fake op on %s!  Reversing...", ch);
          dprintf(DP_MODE, "KICK %s %s :%sAbusing ill-gained server ops\n", ch, m->nick, kickprefix);
          m->flags |= SENTKICK;
          reversing = 1;
        } else if (!chan_hasop(m) && !channel_nodesynch(chan)) {
          if (u && u->bot && chk_op(user, chan)) {
            putlog(LOG_MODES, ch, "Mode change by friendly non-chanop on %s!  Opping...", ch);
            do_op(m, chan, 0, 0);
          } else {
            putlog(LOG_MODES, ch, "Mode change by non-chanop on %s!  Reversing...", ch);
            dprintf(DP_MODE, "KICK %s %s :%sAbusing desync\n", ch, m->nick, kickprefix);
            m->flags |= SENTKICK;
            reversing = 1;
          }
        }
      }

      for (i = 0; i < modecnt; i++) {
        int todo = 0;

        if (isserver[0] && bounce_modes)
          reversing = 1;

        switch (mmode) {		/* parse mode */
          case 'i':
            todo = CHANINV;
            if (isserver[0] && (bounce_modes))
              reversing = 1;
            break;
          case 'p':
            todo = CHANPRIV;
            if (isserver[0] && (bounce_modes))
              reversing = 1;
            break;
          case 's':
            todo = CHANSEC;
            if (isserver[0] && (bounce_modes))
              reversing = 1;
            break;
          case 'm':
            todo = CHANMODER;
            if (isserver[0] && (bounce_modes))
              reversing = 1;
            break;
          case 'c':
            todo = CHANNOCLR;
            if (isserver[0] && (bounce_modes))
              reversing = 1;
            break;
          case 'C':
            todo = CHANNOCTCP;
            if (isserver[0] && (bounce_modes))
              reversing = 1;
            break;
          case 'R':
            todo = CHANREGON;
            if (isserver[0] && (bounce_modes))
              reversing = 1;
            break;
          case 'M':
            todo = CHANMODR;
            if (isserver[0] && (bounce_modes))
              reversing = 1;
            break;
          case 'r':
            todo = CHANLONLY;
            if (isserver[0] && (bounce_modes))
              reversing = 1;
            break;
          case 't':
            todo = CHANTOPIC;
            if (isserver[0] && (bounce_modes))
              reversing = 1;
            break;
          case 'n':
            todo = CHANNOMSG;
            if (isserver[0] && (bounce_modes))
              reversing = 1;
            break;
          case 'a':
            todo = CHANANON;
            if (isserver[0] && (bounce_modes))
              reversing = 1;
            break;
          case 'q':
            todo = CHANQUIET;
            if (isserver[0] && (bounce_modes))
              reversing = 1;
            break;
          case 'l':
            if (isserver[0] && (bounce_modes))
              reversing = 1;
            if (msign == '-') {
              if (channel_active(chan)) {
                if ((reversing) && (chan->channel.maxmembers != 0)) {
                  simple_snprintf(s, sizeof(s), "%d", chan->channel.maxmembers);
                  add_mode(chan, '+', 'l', s);
                } else if ((chan->limit_prot != 0) && !glob_master(user) && !chan_master(user)) {
                  simple_snprintf(s, sizeof(s), "%d", chan->limit_prot);
                  add_mode(chan, '+', 'l', s);
                } else {
                  if (chan->limitraise && dolimit(chan) && (!chan_master(user) && !glob_master(user) && !glob_bot(user))) {
                    chan->channel.maxmembers = 0;     /* set this to 0 so a new limit is generated */
                    raise_limit(chan);
                  }
                }
              }
              chan->channel.maxmembers = 0;
            } else {  /* + */
              if (*mparam == '\0')
                break;
              chan->channel.maxmembers = atoi(mparam);
              if (channel_pending(chan))
                break;
              if (((reversing) &&
                    !(chan->mode_pls_prot & CHANLIMIT)) ||
                  ((chan->mode_mns_prot & CHANLIMIT) && 
                   !glob_bot(user) && !glob_master(user) && !chan_master(user)))
                add_mode(chan, '-', 'l', "");
              if ((chan->limit_prot != chan->channel.maxmembers) && (chan->mode_pls_prot & CHANLIMIT) && (chan->limit_prot != 0) && 
                  !glob_bot(user) && !glob_master(user) && !chan_master(user)) {
                simple_snprintf(s, sizeof(s), "%d", chan->limit_prot);
                add_mode(chan, '+', 'l', s);
              }
              if (chan->limitraise && dolimit(chan) && !glob_bot(user) && (!chan_master(user) && !glob_master(user)))
                raise_limit(chan);
            }
            break;
          case 'k':
            if (msign == '+')
              chan->channel.mode |= CHANKEY;
            else
              chan->channel.mode &= ~CHANKEY;
            if (*mparam == '\0')
              break;

            if (msign == '+') {
              my_setkey(chan, mparam);
              if (channel_active(chan))
                got_key(chan, mparam);
            } else {
              if (channel_active(chan)) {
                if ((reversing) && (chan->channel.key[0]))
                  add_mode(chan, '+', 'k', chan->channel.key);
                else if ((chan->key_prot[0]) && !glob_master(user) && !chan_master(user))
                  add_mode(chan, '+', 'k', chan->key_prot);
              }
              my_setkey(chan, NULL);
            }
            break;
          case 'o':
            if (!u || (u && !u->bot))
              chan->channel.fighting++;
            mv = assert_ismember(chan, mparam);
            if (mv) {
              if (msign == '+')
                got_op(chan, m, mv);
              else
                got_deop(chan, m, mv, isserver);
            }
            break;
          case 'v':
            mv = assert_ismember(chan, mparam);

            if (mv) {
              bool dv = 0;

              get_user_flagrec(mv->user, &victim, chan->dname, chan);

              if (msign == '+') {
                if (mv->flags & EVOICE) {
                  if (!chk_op(user, chan) && !chk_voice(mv, victim, chan)) {
                    dv = 1;
                  } else {
                    mv->flags &= ~EVOICE;
                  }
                }
                mv->flags &= ~SENTVOICE;
                mv->flags |= CHANVOICE;
                if (channel_active(chan) && dovoice(chan)) {
                  if (dv || chk_devoice(victim) || (channel_voicebitch(chan) && !chk_voice(mv, victim, chan))) {
                    add_mode(chan, '-', 'v', mv);
                  } else if (reversing) {
                    add_mode(chan, '-', 'v', mv);
                  }
                }
              } else if (msign == '-') {
                mv->flags &= ~SENTDEVOICE;
                mv->flags &= ~CHANVOICE;
                if (channel_active(chan) && dovoice(chan) && !chan_hasop(mv)) {
                  /* revoice +v users */
                  if (chk_voice(mv, victim, chan)) {
                    add_mode(chan, '+', 'v', mv);
                  } else if (reversing) {
                    add_mode(chan, '+', 'v', mv);
                    /* if they arent +v|v and VOICER is m+ then EVOICE them */
                  } else {
                    if (!match_my_nick(nick) && channel_voice(chan) &&
                        (chk_op(user, chan) || glob_bot(user)) &&
                        rfc_casecmp(nick, mparam)) {
                      /* if the user is not +q set them norEVOICE. */
                      if (!chan_quiet(victim)) {
                        set_devoice(chan, mv);
                      }
                    }
                  }
                }
              }
            }
            break;
          case 'b':
            if (!u || (u && !u->bot))
              chan->channel.fighting++;
            if (msign == '+')
              got_ban(chan, m, mparam, isserver);
            else
              got_unban(chan, m, mparam);
            break;
          case 'e':
            if (!u || (u && !u->bot))
              chan->channel.fighting++;
            if (msign == '+')
              got_exempt(chan, m, mparam, isserver);
            else
              got_unexempt(chan, m, mparam);
            break;
          case 'I':
            if (!u || (u && !u->bot))
              chan->channel.fighting++;
            if (msign == '+')
              got_invite(chan, m, mparam, isserver);
            else
              got_uninvite(chan, m, mparam);
            break;
        }
        if (todo) {
          if (msign == '+')
            chan->channel.mode |= todo;
          else
            chan->channel.mode &= ~todo;
          if (channel_active(chan)) {
            if ((((msign == '+') && (chan->mode_mns_prot & todo)) ||
                  ((msign == '-') && (chan->mode_pls_prot & todo))) &&
                !glob_master(user) && !chan_master(user))
              add_mode(chan, msign == '+' ? '-' : '+', mmode, "");
            else if (reversing &&
                ((msign == '+') || (chan->mode_pls_prot & todo)) &&
                ((msign == '-') || (chan->mode_mns_prot & todo)))
              add_mode(chan, msign == '+' ? '-' : '+', mmode, "");
          }
        }
      }
      // If reversing, dump the modes out
      if (reversing)
        flush_mode(chan, QUICK);
      for (i = 0; i < modecnt; i++)
        if (modes[i])
          free(modes[i]);
      free(modes);

      if (chan->channel.do_opreq)
        request_op(chan);
      if (!me_op(chan) && isserver[0])		/* FIXME, WTF IS THIS? */
        chan->ircnet_status |= CHAN_ASKEDMODES;
    }
  }
  return 0;
}

/* vim: set sts=2 sw=2 ts=8 et: */
