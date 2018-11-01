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
 *
 * chan.c -- part of irc.mod
 *   almost everything to do with channel manipulation
 *   telling channel status
 *   'who' response
 *   user kickban, kick, op, deop
 *   idle kicking
 *
 */

#include <bdlib/src/String.h>
#include <bdlib/src/Array.h>

static time_t last_ctcp = (time_t) 0L;
static int    count_ctcp = 0;
static time_t last_invtime = (time_t) 0L;
static char   last_invchan[300] = "";

typedef struct resolvstruct {
  char *chan;
  char *host;
  bd::String* servers;
  bd::String* server;
} resolv_member;

static void resolv_member_callback(int id, void *client_data, const char *host, bd::Array<bd::String> ips)
{
  resolv_member *r = (resolv_member *) client_data;
  struct chanset_t* chan = NULL;

  if (!r || !r->chan || !r->host || !ips.size() || !(chan = findchan_by_dname(r->chan))) {
    if (r) {
      if (r->chan) free(r->chan);
      if (r->host) free(r->host);
      free(r);
    }
    return;
  }

  memberlist *m = NULL;
  char *pe = NULL, user[15] = "";
  bool matched_user = 0;

  /* Apply lookup results to all matching members by host */
  for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
    if (!m->userip[0] && m->userhost[0]) {
      pe = strchr(m->userhost, '@');
      if (pe && !strcmp(pe + 1, r->host)) {
        strlcpy(user, m->userhost, pe - m->userhost + 1);
        simple_snprintf(m->userip, sizeof(m->userip), "%s@%s", user, bd::String(ips[0]).c_str());
        simple_snprintf(m->fromip, sizeof(m->fromip), "%s!%s", m->nick, m->userip);
        if (!m->user) {
          m->user = get_user_by_host(m->fromip);

          /* Act on this lookup */
          if (m->user)
            check_this_user(m->user->handle, 0, NULL);
        }
        if (m->user)
          matched_user = 1;
      }
    }
  }

  if (!matched_user && channel_rbl(chan))
    resolve_to_rbl(chan, bd::String(ips[0]).c_str());

  free(r->host);
  free(r->chan);
  free(r);
  return;
}


void resolve_to_member(struct chanset_t *chan, char *nick, char *host)
{
  resolv_member *r = (resolv_member *) calloc(1, sizeof(resolv_member));

  r->chan = strdup(chan->dname);
  r->host = strdup(host);

  if (egg_dns_lookup(host, 20, resolv_member_callback, (void *) r) == -2) { //Already querying?
    // Querying on clones will cause the callback to not be called and this nick will be ignored
    // cleanup memory as this chain is not even starting.
    free(r->host);
    free(r->chan);
    free(r);
  }
}

/* RBL */
static void resolve_rbl_callback(int id, void *client_data, const char *host, bd::Array<bd::String> ips)
{
  resolv_member *r = (resolv_member *) client_data;
  struct chanset_t* chan = NULL;

  if (!r || !r->chan || !r->host || !(chan = findchan_by_dname(r->chan)) || !ips.size()) {
    if (r && chan && r->host) {
      // Lookup from the next RBL
      resolve_to_rbl(chan, r->host, r);
    }
    return;
  }

  sdprintf("RBL match for %s:%s: %s", chan->dname, r->host, bd::String(ips[0]).c_str());

  char s1[UHOSTLEN] = "";
  simple_snprintf(s1, sizeof(s1), "*!*@%s", r->host);

  bd::String reason = "Listed in RBL: " + *(r->server);

  if (!(use_exempts && (u_match_mask(global_exempts, s1) || u_match_mask(chan->exempts, s1)))) {

    u_addmask('b', chan, s1, conf.bot->nick, reason.c_str(), now + (60 * (chan->ban_time ? chan->ban_time : 300)), 0);

    if (me_op(chan)) {
      do_mask(chan, chan->channel.ban, s1, 'b');

      memberlist *m = NULL;
      char *pe = NULL;

      /* Apply lookup results to all matching members by host */
      for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
        if (!m->user && !chan_sentkick(m) && m->userip[0]) {
          pe = strchr(m->userip, '@');
          if (pe && !strcmp(pe + 1, r->host)) {
            simple_snprintf(s1, sizeof(s1), "%s!%s", m->nick, m->userhost);
            // Don't kick if exempted
            if (!(use_exempts && (u_match_mask(global_exempts, s1) || u_match_mask(chan->exempts, s1)) &&
                                  isexempted(chan, s1))) {
              m->flags |= SENTKICK;
              dprintf(DP_MODE, "KICK %s %s :%s%s\n", chan->name, m->nick, bankickprefix, reason.c_str());
            }
          }
        }
      }
    }
  }

  sdprintf("Done checking rbl (matched) for %s:%s", chan->dname, r->host);
  delete r->servers;
  delete r->server;
  free(r->host);
  free(r->chan);
  free(r);
  return;
}


void resolve_to_rbl(struct chanset_t *chan, const char *host, resolv_member *r)
{
  // Skip past user@ if present
  char *p = strchr((char*)host, '@');
  if (p)
    host = p + 1;
  if (!r) {
    r = (resolv_member *) calloc(1, sizeof(resolv_member));

    r->chan = strdup(chan->dname);
    r->host = strdup(host);
    r->servers = new bd::String(rbl_servers);
    r->server = new bd::String;
  }

  bd::String rbl_server = newsplit(*(r->servers), ',');

  if (!rbl_server) {
    sdprintf("Done checking rbl (no match) for %s:%s", chan->dname, host);
    delete r->servers;
    delete r->server;
    free(r->host);
    free(r->chan);
    free(r);
    return; //No more servers
  }

  *(r->server) = rbl_server;

  size_t iplen = rbl_server.length() + 1;
  bool v6 = 0;
  if (strchr(host, ':')) {
    v6 = 1;
    iplen += 128 + 1;
  } else
    iplen += strlen(host) + 1;

  char *ip = (char *) calloc(1, iplen);
  if (v6)
    socket_ipv6_to_dots(host, ip);
  else {
    reverse_ip(host, ip);
    strlcat(ip, ".", iplen);
  }
  strlcat(ip, rbl_server.c_str(), iplen);

  if (egg_dns_lookup(ip, 20, resolve_rbl_callback, (void *) r, DNS_A) == -2) { //Already querying?
    // Querying on clones will cause the callback to not be called and this nick will be ignored
    // cleanup memory as this chain is not even starting.
    delete r->servers;
    delete r->server;
    free(r->host);
    free(r->chan);
    free(r);
  }

  free(ip);
}

// Just got a part event (KICK, PART) on me
static void check_rejoin(struct chanset_t* chan) {
  if (shouldjoin(chan))
    force_join_chan(chan);
  else // Out of chan, not rejoining, just clear it
    clear_channel(chan, 1);
}

/* ID length for !channels.
 */
#define CHANNEL_ID_LEN 5

#ifdef NO
static void print_memberlist(memberlist *toprint)
{
  memberlist *m = NULL;

  for (m = toprint; m && m->nick[0]; m = m->next) {
    sdprintf("%s!%s user: %s tried: %d hops: %d", m->nick, m->userhost, m->user ? m->user->handle : "", m->tried_getuser, m->hops);
  }
}
#endif

/* Returns a pointer to a new channel member structure.
 */
static memberlist *newmember(struct chanset_t *chan, char *nick)
{
  memberlist *x = chan->channel.member, 
             *lx = NULL, 
             *n = new memberlist;

  /* This sorts the list */
  while (x && x->nick[0] && (rfc_casecmp(x->nick, nick) < 0)) {
    lx = x;
    x = x->next;
  }

  strlcpy(n->nick, nick, sizeof(n->nick));
  n->rfc_nick = std::make_shared<RfcString>(n->nick);
  n->hops = -1;
  if (!lx) {
    // Free the pseudo-member created in init_channel()
    if (unlikely(!chan->channel.member->nick[0])) {
      free(chan->channel.member);
      chan->channel.member = NULL;
    }

    n->next = chan->channel.member;
    chan->channel.member = n;
  } else {
    n->next = lx->next;
    lx->next = n;
  }

  ++(chan->channel.members);
  n->floodtime = new bd::HashTable<flood_t, time_t>;
  n->floodnum  = new bd::HashTable<flood_t, int>;
  (*chan->channel.hashed_members)[*n->rfc_nick] = n;
  return n;
}

void delete_member(memberlist* m) {
  delete m->floodtime;
  delete m->floodnum;
  delete m;
}

static bool member_getuser(memberlist* m, bool act_on_lookup) {
  if (!m) return 0;
  if (!m->user && !m->tried_getuser) {
    m->user = get_user_by_host(m->from);
    if (!m->user && m->userip[0]) {
      m->user = get_user_by_host(m->fromip);
    }
    m->tried_getuser = 1;

    /* Managed to get the user for a previously unknown user. Act on it! */
    if (act_on_lookup && m->user)
      check_this_user(m->user->handle, 0, NULL);

  }
  return !(m->user == NULL);
}

/* Always pass the channel dname (display name) to this function <cybah>
 */
static void update_idle(char *chname, char *nick)
{
  struct chanset_t *chan = findchan_by_dname(chname);

  if (chan) {
    memberlist *m = ismember(chan, nick);

    if (m)
      m->last = now;
  }
}

/* Returns the current channel mode.
 */
static char *getchanmode(struct chanset_t *chan)
{
  static char s[121] = "";
  int atr = chan->channel.mode;
  size_t i = 1;

  s[0] = '+';
  if (atr & CHANINV)
    s[i++] = 'i';
  if (atr & CHANPRIV)
    s[i++] = 'p';
  if (atr & CHANSEC)
    s[i++] = 's';
  if (atr & CHANMODER)
    s[i++] = 'm';
  if (atr & CHANNOCLR)
    s[i++] = 'c';
  if (atr & CHANNOCTCP)
    s[i++] = 'C';
  if (atr & CHANREGON)
    s[i++] = 'R';
  if (atr & CHANTOPIC)
    s[i++] = 't';
  if (atr & CHANMODR)
    s[i++] = 'M';
  if (atr & CHANLONLY)
    s[i++] = 'r';
  if (atr & CHANNOMSG)
    s[i++] = 'n';
  if (atr & CHANANON)
    s[i++] = 'a';
  if (atr & CHANKEY)
    s[i++] = 'k';
  if (chan->channel.maxmembers != 0)
    s[i++] = 'l';
  s[i] = 0;
  if (chan->channel.key[0])
    i += simple_snprintf(s + i, sizeof(s) - i, " %s", chan->channel.key);
  if (chan->channel.maxmembers != 0)
    simple_snprintf(s + i, sizeof(s) - i, " %d", chan->channel.maxmembers);
  return s;
}

static void check_exemptlist(struct chanset_t *chan, const char *from)
{
  if (!use_exempts)
    return;

  bool ok = 0;

  for (masklist *e = chan->channel.exempt; e->mask[0]; e = e->next)
    if (wild_match(e->mask, from)) {
      add_mode(chan, '-', 'e', e->mask);
      ok = 1;
    }
  if (prevent_mixing && ok)
    flush_mode(chan, QUICK);
}

static void priority_do(struct chanset_t * chan, bool opsonly, int action, bool flush = 1)
{
  if (!me_op(chan))
    return;
  if (channel_pending(chan) || !shouldjoin(chan) || !channel_active(chan))
    return;

  memberlist *m = NULL;
  int ops = 0, targets = 0, bpos = 0, tpos = 0, ft = 0, ct = 0, actions = 0, sent = 0;

  for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
    member_getuser(m);

    if (m->user && m->user->bot && (m->user->flags & USER_OP)) {
      ++ops;
      if (m->is_me)
        bpos = (ops - 1);

    } else if (!opsonly || chan_hasop(m)) {
        struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };
        if (m->user)
          get_user_flagrec(m->user, &fr, chan->dname, chan);

        if (!chk_op(fr, chan))
          ++targets;
    }
  }

  if (!targets || !ops)
    return;

  for (int n = 0; n < 2; ++n) {
    // First pass - Handle my priority targets
    if (n == 0) {
      ft = (bpos * targets) / ops;
      ct = ((bpos + 2) * targets + (ops - 1)) / ops;
      ct = (ct - ft + 1);
      if (ct > 20)
        ct = 20;
      while (ft >= targets)
        ft -= targets;
      actions = 0;
      sent = 0;
    } else {
      // Second pass - Handle all remaining if I am able to
      ct = ct - actions;
      if (ct > ft)
        ct = ft;
      ft = 0;
      actions = 0;
      tpos = 0;
    }
    for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
      if (!opsonly || chan_hasop(m)) {
        struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };

        if (m->user)
          get_user_flagrec(m->user, &fr, chan->dname, chan);

        if (!chk_op(fr, chan)) {
          if (tpos >= ft) {
            if ((action == PRIO_DEOP) && !chan_sentdeop(m)) {
              ++actions;
              ++sent;
              add_mode(chan, '-', 'o', m);
              if (!floodless && (actions >= ct || (n == 1 && sent > 20))) {
                if (flush)
                  flush_mode(chan, QUICK);
                return;
              }
            } else if ((action == PRIO_KICK) && !chan_sentkick(m) &&
                // Check closed-exempt
                !((chan_hasop(m) && chan->closed_exempt_mode == CHAN_FLAG_OP) ||
                  ((chan_hasvoice(m) || chan_hasop(m)) && chan->closed_exempt_mode == CHAN_FLAG_VOICE))) {
              ++actions;
              ++sent;
              if (chan->closed_ban)
                do_closed_kick(chan, m);
              dprintf(DP_MODE, "KICK %s %s :%s%s\n", chan->name, m->nick, kickprefix, response(RES_CLOSED));
              m->flags |= SENTKICK;
              if (!floodless && (actions >= ct || (n == 1 && sent > 5)))
                return;
            }
          }
          ++tpos;
        
        }
      }
    }
  }
}

/* lame code 
static int target_priority(struct chanset_t * chan, memberlist *target, int opsonly) 
{
  memberlist *m;
  int ops = 0, targets = 0, bpos = 0, ft = 0, ct = 0, tp = (-1), pos = 0;

  return 1;
  for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
    if (m->user && ((m->user->flags & (USER_BOT | USER_OP)) == (USER_BOT | USER_OP))) {
      ops++;
      if (m->is_me)
        bpos = ops;
    } else if (!opsonly || chan_hasop(m)) {
      struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };

      if (m->user)
        get_user_flagrec(m->user, &fr, chan->dname);

      if (((glob_deop(fr) && !chan_op(fr)) || chan_deop(fr)) ||
         ((!channel_privchan(chan) && !chan_op(fr) && !glob_op(fr)) || 
         (channel_privchan(chan) && !glob_bot(fr) && !glob_owner(fr) && !chan_op(fr)))) { 
        targets++;
      }
    }
    if (m == target)
      tp = pos;
    pos++;
  }
  if (!targets || !ops || (tp < 0)) {
    return 0;
  }
  ft = (bpos * targets) / ops;
  ct = ((bpos + 2) * targets + (ops - 1)) / ops;
  ct = (ct - ft + 1);
  if (ct > 20)
    ct = 20;
  while (ft >= targets) {
    ft -= targets;
  }
  if (ct >= targets) {
    putlog(LOG_MISC, "*", "%s 1 ct >= targets; ct %d targets %d", target, ct, targets);
    if ((tp >= ft) || (tp <= (ct % targets))) {
      putlog(LOG_MISC, "*", "%s (1) first if, tp %d ft %d ct/targets %d", target, tp, ft, (ct % targets));
      return 1;
    }
  } else {
    putlog(LOG_MISC, "*", "%s 2 else, ct %d targets %d", target, ct, targets);
    if ((tp >= ft) && (tp <= ct)) {
      putlog(LOG_MISC, "*", "%s (1) second if, tp %d ft %d", target, tp, ft);
      return 1;
    }
  }
  putlog(LOG_MISC, "*", "%s (0) returning 0", target);
  return 0;
}
*/

/* Check a channel and clean-out any more-specific matching masks.
 *
 * Moved all do_ban(), do_exempt() and do_invite() into this single function
 * as the code bloat is starting to get rediculous <cybah>
 */
static void do_mask(struct chanset_t *chan, masklist *m, char *mask, char Mode)
{
  for (; m && m->mask[0]; m = m->next)
    if (wild_match(mask, m->mask) && rfc_casecmp(mask, m->mask))
      add_mode(chan, '-', Mode, m->mask);
  add_mode(chan, '+', Mode, mask);
  flush_mode(chan, QUICK);
}

/* This is a clone of detect_flood, but works for channel specificity now
 * and handles kick & deop as well.
 */
static bool detect_chan_flood(memberlist* m, const char *from, struct chanset_t *chan, flood_t which, const char *msg)
{
  /* Do not punish non-existant channel members and IRC services like
   * ChanServ
   */
  if (!chan || (which < 0) || (which >= FLOOD_CHAN_MAX) || !m ||
      !(chan->role & ROLE_FLOOD))
    return 0;

  /* Okay, make sure i'm not flood-checking myself */
  if (m->is_me) {
    return 0;
  }
  if (!strcasecmp(m->userhost, botuserhost))
    return 0;
  /* My user@host (?) */

  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  struct userrec *u = NULL;

  member_getuser(m);
  u = m->user;
  get_user_flagrec(u, &fr, chan->dname, chan);
  if (glob_bot(fr) ||
      ((which == FLOOD_DEOP) &&
       (glob_master(fr) || chan_master(fr))) ||
      ((which == FLOOD_KICK) &&
       (glob_master(fr) || chan_master(fr))) ||
      ((which != FLOOD_DEOP) && (which != FLOOD_KICK) && 
       ( (chk_noflood(fr) || 
         (m && chan->flood_exempt_mode == CHAN_FLAG_OP && chan_hasop(m)) ||
         (m && chan->flood_exempt_mode == CHAN_FLAG_VOICE && (chan_hasvoice(m) || chan_hasop(m))) )
      )))
    return 0;

  char h[UHOSTLEN] = "", ftype[14] = "", *p = NULL;
  int thr = 0, mthr = 0;
  int increment = 1;
  time_t lapse = 0, mlapse = 0;

  /* Determine how many are necessary to make a flood. */
  switch (which) {
  case FLOOD_PRIVMSG:
  case FLOOD_NOTICE:
    thr = chan->flood_pub_thr;
    lapse = chan->flood_pub_time;
    mthr = chan->flood_mpub_thr;
    mlapse = chan->flood_mpub_time;
    strlcpy(ftype, "pub", sizeof(ftype));
    break;
  case FLOOD_BYTES:
    thr = chan->flood_bytes_thr;
    lapse = chan->flood_bytes_time;
    mthr = chan->flood_mbytes_thr;
    mlapse = chan->flood_mbytes_time;
    strlcpy(ftype, "bytes", sizeof(ftype));
    increment = static_cast<int>(strlen(msg));
    break;
  case FLOOD_CTCP:
    thr = chan->flood_ctcp_thr;
    lapse = chan->flood_ctcp_time;
    mthr = chan->flood_mctcp_thr;
    mlapse = chan->flood_mctcp_time;
    strlcpy(ftype, "ctcp", sizeof(ftype));
    break;
  case FLOOD_NICK:
    thr = chan->flood_nick_thr;
    lapse = chan->flood_nick_time;
    strlcpy(ftype, "nick", sizeof(ftype));
    break;
  case FLOOD_JOIN:
  case FLOOD_PART:
    thr = chan->flood_join_thr;
    lapse = chan->flood_join_time;
    strlcpy(ftype, "join", sizeof(ftype));
    break;
  case FLOOD_DEOP:
    thr = chan->flood_deop_thr;
    lapse = chan->flood_deop_time;
    strlcpy(ftype, "deop", sizeof(ftype));
    break;
  case FLOOD_KICK:
    thr = chan->flood_kick_thr;
    lapse = chan->flood_kick_time;
    strlcpy(ftype, "kick", sizeof(ftype));
    break;
  }

  if ((which == FLOOD_KICK) || (which == FLOOD_DEOP))
    p = m->nick;
  else {
    p = strchr(m->userhost, '@');
    if (p) {
      p++;
    }
    if (!p)
      return 0;
  }

  // Track across all clients in the channel
  if (mlapse && mthr) {
    if (!chan->channel.floodtime->contains("all")) {
      (*chan->channel.floodtime)["all"][which] = now;
      (*chan->channel.floodnum)["all"][which] = increment;
      return 0;
    }

    // No point locking down due to OPs flooding.
    if (!chan_hasop(m)) {

      bd::HashTable<flood_t, time_t>      *global_floodtime = &(*chan->channel.floodtime)["all"];
      bd::HashTable<flood_t, int>         *global_floodnum = &(*chan->channel.floodnum)["all"];

      if ((*global_floodtime)[which] < now - mlapse) {
        /* Flood timer expired, reset it */
        (*global_floodtime)[which] = now;
        (*global_floodnum)[which] = increment;
      } else {
        (*global_floodnum)[which] += increment;

        if ((*global_floodnum)[which] >= mthr) {	/* FLOOD */
          /* Reset counters */
          (*global_floodnum).remove(which);
          (*global_floodtime).remove(which);
          if (!chan->channel.drone_set_mode) {
            lockdown_chan(chan, FLOOD_MASS_FLOOD, ftype);
          }
        }
      }
    }
  }

  if ((thr == 0) || (lapse == 0)) {
    return 0;			/* no flood protection */
  }

  // Track individual hosts/clients
  bd::HashTable<flood_t, time_t>      *floodtime; // floodtime[FLOOD_PRIVMSG] = now;
  bd::HashTable<flood_t, int>         *floodnum;  //  floodnum[FLOOD_PRIVMSG] = 1;

  switch (which) {
    // These 2 don't have a persistent member, use chan list
    case FLOOD_JOIN:
    case FLOOD_PART:
      // If not found, add them and start the count for next iteration
      if (!chan->channel.floodtime->contains(m->userhost)) {
        (*chan->channel.floodtime)[m->userhost][which] = now;
        (*chan->channel.floodnum)[m->userhost][which] = increment;
        return 0;
      } else {
        floodtime = &(*chan->channel.floodtime)[m->userhost];
        floodnum = &(*chan->channel.floodnum)[m->userhost];
      }
      break;
    default:
      // Everything else, log to the member
      // If not found, add them and start the count for next iteration
      if (!m->floodtime->contains(which)) {
        (*m->floodtime)[which] = now;
        (*m->floodnum)[which] = increment;
        return 0;
      } else {
        floodtime = m->floodtime;
        floodnum = m->floodnum;
      }
      break;
  }

  if ((*floodtime)[which] < now - lapse) {
    /* Flood timer expired, reset it */
    (*floodtime)[which] = now;
    (*floodnum)[which] = increment;
    return 0;
  }
  (*floodnum)[which] += increment;

  if ((*floodnum)[which] >= thr) {	/* FLOOD */
    /* Reset counters */
    (*floodnum).remove(which);
    (*floodtime).remove(which);
    switch (which) {
    case FLOOD_PRIVMSG:
    case FLOOD_NOTICE:
    case FLOOD_CTCP:
    case FLOOD_BYTES:
      /* Flooding chan! either by public or notice */
      if (!chan_sentkick(m) && me_op(chan)) {
        if (channel_floodban(chan)) {
          putlog(LOG_MODES, chan->dname, "Channel flood from %s -- banning", m->nick);
          char *s1 = quickban(chan, from);
          u_addmask('b', chan, s1, conf.bot->nick, "channel flood", now + (60 * chan->ban_time), 0);
        } else {
          const char *response = punish_flooder(chan, m);
          putlog(LOG_MODES, chan->dname, "Channel flood from %s -- %s", m->nick, response);
        }
      }
      return 1;
    case FLOOD_JOIN:
    case FLOOD_PART:
    case FLOOD_NICK:
      if (use_exempts &&
	  (u_match_mask(global_exempts, from) ||
	   u_match_mask(chan->exempts, from)))
	return 1;
      simple_snprintf(h, sizeof(h), "*!*@%s", p);
      if (!isbanned(chan, h) && me_op(chan)) {
	check_exemptlist(chan, from);
	do_mask(chan, chan->channel.ban, h, 'b');
      }
      if ((u_match_mask(global_bans, from))
	  || (u_match_mask(chan->bans, from)))
	return 1;		/* Already banned */
      if (which == FLOOD_JOIN || which == FLOOD_PART)
	putlog(LOG_MISC | LOG_JOIN, chan->dname, "JOIN flood from @%s!  Banning.", p);
      else
	putlog(LOG_MISC | LOG_JOIN, chan->dname, "NICK flood from @%s!  Banning.", p);
      strlcat(ftype, " flood", sizeof(ftype));
      u_addmask('b', chan, h, conf.bot->nick, ftype, now + (60 * chan->ban_time), 0);
      if (which == FLOOD_PART)
        add_mode(chan, '+', 'b', h);
      if (!channel_enforcebans(chan) && me_op(chan)) {
	  for (m = chan->channel.member; m && m->nick[0]; m = m->next) {	  
	    if (!chan_sentkick(m) && wild_match(h, m->from) &&
		(m->joined >= (*floodtime)[which]) &&
		!m->is_me && me_op(chan)) {
	      m->flags |= SENTKICK;
	      if (which == FLOOD_JOIN)
   	        dprintf(DP_SERVER, "KICK %s %s :%sjoin flood\n", chan->name, m->nick, kickprefix);
	      else
                dprintf(DP_SERVER, "KICK %s %s :%s%s\n", chan->name, m->nick, kickprefix, response(RES_NICKFLOOD));
	    }
	  }
	}
      return 1;
    case FLOOD_KICK:
      if (me_op(chan) && !chan_sentkick(m)) {
	putlog(LOG_MODES, chan->dname, "Kicking %s, for mass kick.", m->nick);
        dprintf(DP_MODE, "KICK %s %s :%s%s\n", chan->name, m->nick, kickprefix, response(RES_KICKFLOOD));
	m->flags |= SENTKICK;
      }
      if (channel_protect(chan))
        do_protect(chan, "Mass Kick");
    return 1;
    case FLOOD_DEOP:
      if (me_op(chan) && !chan_sentkick(m)) {
	putlog(LOG_MODES, chan->dname,
	       "Mass deop on %s by %s", chan->dname, from);
        dprintf(DP_MODE, "KICK %s %s :%s%s\n", chan->name, m->nick, kickprefix, response(RES_MASSDEOP));
	m->flags |= SENTKICK;
      }
      if (u) {
        char s[256] = "";

        simple_snprintf(s, sizeof(s), "Mass deop on %s by %s", chan->dname, from);
        deflag_user(u, DEFLAG_EVENT_MDOP, s, chan);
      }
      if (channel_protect(chan))
        do_protect(chan, "Mass Deop");
      return 1;
    }
  }
  return 0;
}

/* Given a chan/m do all necesary exempt checks and ban. */
static void refresh_ban_kick(struct chanset_t*, memberlist *, const char *);
static void do_closed_kick(struct chanset_t *chan, memberlist *m)
{
  if (!chan || !m) return;

  char *s1 = NULL;

  if (!(use_exempts &&
        (u_match_mask(global_exempts, m->from) ||
         u_match_mask(chan->exempts, m->from)))) {
    if (u_match_mask(global_bans, m->from) || u_match_mask(chan->bans, m->from))
      refresh_ban_kick(chan, m, m->from);

    check_exemptlist(chan, m->from);
    s1 = quickban(chan, m->from);
    u_addmask('b', chan, s1, conf.bot->nick, "joined closed chan", now + (60 * chan->ban_time), 0);
  }
  return;
}

/* Given a nick!user@host, place a quick ban on them on a chan.
 */
static char *quickban(struct chanset_t *chan, const char *from)
{
  static char s1[512] = "";

  maskaddr(from, s1, chan->ban_type);
  do_mask(chan, chan->channel.ban, s1, 'b');
  return s1;
}

/* Kick any user (except friends/masters) with certain mask from channel
 * with a specified comment.  Ernst 18/3/1998
 */
static void kick_all(struct chanset_t *chan, char *hostmask, const char *comment, int bantype)
{
  int flushed = 0;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };

  for (memberlist *m = chan->channel.member; m && m->nick[0]; m = m->next) {
    get_user_flagrec(m->user, &fr, chan->dname, chan);
    if ((wild_match(hostmask, m->from) || match_cidr(hostmask, m->from) ||
          (m->userip[0] && (wild_match(hostmask, m->fromip) || match_cidr(hostmask, m->fromip)))) &&
        !chan_sentkick(m) &&
	!m->is_me && !chan_issplit(m) &&
	!(use_exempts &&
	  ((bantype && (isexempted(chan, m->from) || (chan->ircnet_status & CHAN_ASKED_EXEMPTS))) ||
	   (u_match_mask(global_exempts, m->from) ||
	    u_match_mask(chan->exempts, m->from) ||
            (m->userip[0] &&
             (u_match_mask(global_exempts, m->fromip) ||
              u_match_mask(chan->exempts, m->fromip))))))) {
      if (!flushed) {
	/* We need to kick someone, flush eventual bans first */
	flush_mode(chan, QUICK);
	flushed += 1;
      }
      if (!chan_sentkick(m)) {
        m->flags |= SENTKICK;	/* Mark as pending kick */
        dprintf(DP_MODE, "KICK %s %s :%s%s\n", chan->name, m->nick, kickprefix, comment);
      }
    }
  }
}

/* If any bans match this wildcard expression, refresh them on the channel.
 */
static void refresh_ban_kick(struct chanset_t* chan, memberlist *m, const char *user)
{
  if (!m || chan_sentkick(m))
    return;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  get_user_flagrec(m->user, &fr, chan->dname, chan);

  /* Check global bans in first cycle and channel bans
     in second cycle. */
  for (int cycle = 0; cycle < 2; cycle++) {
    for (maskrec* b = cycle ? chan->bans : global_bans; b; b = b->next) {
      if (wild_match(b->mask, user) || match_cidr(b->mask, user)) {
        if ((chan->role & ROLE_DEOP) && chan_hasop(m))
          add_mode(chan, '-', 'o', m);	/* Guess it can't hurt.	*/
	check_exemptlist(chan, user);
	do_mask(chan, chan->channel.ban, b->mask, 'b');
	b->lastactive = now;
        if (chan->role & ROLE_KICK) {
          char c[512] = "";		/* The ban comment.	*/

          if (b->desc && b->desc[0] != '@')
	    simple_snprintf(c, sizeof(c), "banned: %s", b->desc);
          kick_all(chan, b->mask, c[0] ? (const char *) c : "You are banned", 0);
        }
        return;					/* Drop out on 1st ban.	*/
      } 
    }
  }
}

/* This is a bit cumbersome at the moment, but it works... Any improvements
 * then feel free to have a go.. Jason
 */
static void refresh_exempt(struct chanset_t *chan, char *user)
{
  maskrec *e = NULL;
  masklist *b = NULL;

  /* Check global exempts in first cycle and channel exempts
     in second cycle. */
  for (int cycle = 0; cycle < 2; cycle++) {
    for (e = cycle ? chan->exempts : global_exempts; e; e = e->next) {
      if (wild_match(user, e->mask) || wild_match(e->mask, user) || match_cidr(e->mask, user)) {
        for (b = chan->channel.ban; b && b->mask[0]; b = b->next) {
          if (wild_match(b->mask, user) || wild_match(user, b->mask)) {
            if (e->lastactive < now - 60 && !isexempted(chan, e->mask)) {
              do_mask(chan, chan->channel.exempt, e->mask, 'e');
              e->lastactive = now;
            }
          }
        }
      }
    }
  }
}

static void refresh_invite(struct chanset_t *chan, const char *user)
{
  maskrec *i = NULL;

  /* Check global invites in first cycle and channel invites
     in second cycle. */
  for (int cycle = 0; cycle < 2; cycle++) {
    for (i = cycle ? chan->invites : global_invites; i; i = i->next) {
      if (wild_match(i->mask, user) &&
	  ((i->flags & MASKREC_STICKY) || (chan->channel.mode & CHANINV))) {
        if (i->lastactive < now - 60 && !isinvited(chan, i->mask)) {
          do_mask(chan, chan->channel.invite, i->mask, 'I');
	  i->lastactive = now;
	  return;
	}
      }
    }
  }
}

/* Enforce all channel bans in a given channel.  Ernst 18/3/1998
 */
static void enforce_bans(struct chanset_t *chan)
{
  if (!me_op(chan))
    return;			/* Can't do it :( */

  if ((chan->ircnet_status & CHAN_ASKED_EXEMPTS))
    return;

  const memberlist *me = ismember(chan, botname);

  /* Go through all bans, kicking the users. */
  for (masklist *b = chan->channel.ban; b && b->mask[0]; b = b->next) {
    if (!(wild_match(b->mask, me->from) || match_cidr(b->mask, me->fromip)) && !isexempted(chan, b->mask))
      kick_all(chan, b->mask, "You are banned", 1);
  }
}

/* Make sure that all who are 'banned' on the userlist are actually in fact
 * banned on the channel.
 *
 * Note: Since i was getting a ban list, i assume i'm chop.
 */
static void recheck_bans(struct chanset_t *chan)
{
  maskrec *u = NULL;

  /* Check global bans in first cycle and channel bans
     in second cycle. */
  for (int cycle = 0; cycle < 2; cycle++) {
    for (u = cycle ? chan->bans : global_bans; u; u = u->next)
      if (!isbanned(chan, u->mask) && (!channel_dynamicbans(chan) || (u->flags & MASKREC_STICKY)))
	add_mode(chan, '+', 'b', u->mask);
  }
}

/* Make sure that all who are exempted on the userlist are actually in fact
 * exempted on the channel.
 *
 * Note: Since i was getting an excempt list, i assume i'm chop.
 */
static void recheck_exempts(struct chanset_t *chan)
{
  maskrec *e = NULL;
  masklist *b = NULL;

  /* Check global exempts in first cycle and channel exempts
     in second cycle. */
  for (int cycle = 0; cycle < 2; cycle++) {
    for (e = cycle ? chan->exempts : global_exempts; e; e = e->next) {
      if (!isexempted(chan, e->mask) &&
          (!channel_dynamicexempts(chan) || (e->flags & MASKREC_STICKY)))
        add_mode(chan, '+', 'e', e->mask);
      for (b = chan->channel.ban; b && b->mask[0]; b = b->next) {
        if ((wild_match(b->mask, e->mask) || wild_match(e->mask, b->mask)) &&
            !isexempted(chan, e->mask))
	  add_mode(chan,'+','e',e->mask);
	/* do_mask(chan, chan->channel.exempt, e->mask, 'e');*/
      }
    }
  }
}

/* Make sure that all who are invited on the userlist are actually in fact
 * invited on the channel.
 *
 * Note: Since i was getting an invite list, i assume i'm chop.
 */

static void recheck_invites(struct chanset_t *chan)
{
  maskrec *ir = NULL;

  /* Check global invites in first cycle and channel invites
     in second cycle. */
  for (int cycle = 0; cycle < 2; cycle++)  {
    for (ir = cycle ? chan->invites : global_invites; ir; ir = ir->next) {
      /* If invite isn't set and (channel is not dynamic invites and not invite
       * only) or invite is sticky.
       */
      if (!isinvited(chan, ir->mask) && ((!channel_dynamicinvites(chan) &&
          (chan->channel.mode & CHANINV)) || ir->flags & MASKREC_STICKY))
	add_mode(chan, '+', 'I', ir->mask);
	/* do_mask(chan, chan->channel.invite, ir->mask, 'I');*/
    }
  }
}

/* Resets the masks on the channel.
 */
static void resetmasks(struct chanset_t *chan, masklist *m, maskrec *mrec, maskrec *global_masks, char mode)
{
  if (!me_op(chan))
    return;                     /* Can't do it */

  /* Remove masks we didn't put there */
  for (; m && m->mask[0]; m = m->next) {
    if (!u_equals_mask(global_masks, m->mask) && !u_equals_mask(mrec, m->mask))
      add_mode(chan, '-', mode, m->mask);
  }

  /* Make sure the intended masks are still there */
  switch (mode) {
    case 'b':
      recheck_bans(chan);
      break;
    case 'e':
      recheck_exempts(chan);
      break;
    case 'I':
      recheck_invites(chan);
      break;
    default:
      putlog(LOG_MISC, "*", "(!) Invalid mode '%c' in resetmasks()", mode);
      break;
  }
}

void check_this_ban(struct chanset_t *chan, char *banmask, bool sticky)
{
  if (!me_op(chan))
    return;

  const char *user = NULL;

  for (memberlist *m = chan->channel.member; m && m->nick[0]; m = m->next) {
    for (int i = 0; i < (m->userip[0] ? 2 : 1); ++i) {
      if (i == 0 && m->userip[0]) // Check userip first in case userhost is already an ip
        user = m->fromip;
      else
        user = m->from;
      if ((wild_match(banmask, user)  || match_cidr(banmask, user)) &&
          !(use_exempts &&
            (u_match_mask(global_exempts, user) ||
             u_match_mask(chan->exempts, user)))) {
        refresh_ban_kick(chan, m, user);
        break;
      }
    }
  }
  if (!isbanned(chan, banmask) &&
      (!channel_dynamicbans(chan) || sticky))
    add_mode(chan, '+', 'b', banmask);
}

void check_this_exempt(struct chanset_t *chan, char *mask, bool sticky)
{
  if (!isexempted(chan, mask) && (!channel_dynamicexempts(chan) || sticky))
    add_mode(chan, '+', 'e', mask);
}

void check_this_invite(struct chanset_t *chan, char *mask, bool sticky)
{
  if (!isinvited(chan, mask) && (!channel_dynamicinvites(chan) || sticky))
    add_mode(chan, '+', 'I', mask);
}

void recheck_channel_modes(struct chanset_t *chan)
{
  int cur = chan->channel.mode,
      mns = chan->mode_mns_prot,
      pls = chan->mode_pls_prot;

  if (channel_closed(chan)) {
    if (chan->closed_invite) {
      pls |= CHANINV;
      mns &= ~CHANINV;
    }
    if (chan->closed_private) {
      pls |= CHANPRIV;
      mns &= ~CHANPRIV;
    }
  }
  if (channel_voice(chan) && chan->voice_moderate) {
    pls |= CHANMODER;
    mns &= ~CHANMODER;
  }

  if (!(chan->ircnet_status & CHAN_ASKEDMODES)) {
    if (pls & CHANINV && !(cur & CHANINV))
      add_mode(chan, '+', 'i', "");
    else if (mns & CHANINV && cur & CHANINV)
      add_mode(chan, '-', 'i', "");
    if (pls & CHANPRIV && !(cur & CHANPRIV))
      add_mode(chan, '+', 'p', "");
    else if (mns & CHANPRIV && cur & CHANPRIV)
      add_mode(chan, '-', 'p', "");
    if (pls & CHANSEC && !(cur & CHANSEC))
      add_mode(chan, '+', 's', "");
    else if (mns & CHANSEC && cur & CHANSEC)
      add_mode(chan, '-', 's', "");
    if (pls & CHANMODER && !(cur & CHANMODER))
      add_mode(chan, '+', 'm', "");
    else if (mns & CHANMODER && cur & CHANMODER)
      add_mode(chan, '-', 'm', "");
    if (pls & CHANNOCLR && !(cur & CHANNOCLR))
      add_mode(chan, '+', 'c', "");
    else if (mns & CHANNOCLR && cur & CHANNOCLR)
      add_mode(chan, '-', 'c', "");
    if (pls & CHANNOCTCP && !(cur & CHANNOCTCP))
      add_mode(chan, '+', 'C', "");
    else if (mns & CHANNOCTCP && cur & CHANNOCTCP)
      add_mode(chan, '-', 'C', "");
    if (pls & CHANREGON && !(cur & CHANREGON))
      add_mode(chan, '+', 'R', "");
    else if (mns & CHANREGON && cur & CHANREGON)
      add_mode(chan, '-', 'R', "");
    if (pls & CHANMODR && !(cur & CHANMODR))
      add_mode(chan, '+', 'M', "");
    else if (mns & CHANMODR && cur & CHANMODR)
      add_mode(chan, '-', 'M', "");
    if (pls & CHANLONLY && !(cur & CHANLONLY))
      add_mode(chan, '+', 'r', "");
    else if (mns & CHANLONLY && cur & CHANLONLY)
      add_mode(chan, '-', 'r', "");
    if (pls & CHANTOPIC && !(cur & CHANTOPIC))
      add_mode(chan, '+', 't', "");
    else if (mns & CHANTOPIC && cur & CHANTOPIC)
      add_mode(chan, '-', 't', "");
    if (pls & CHANNOMSG && !(cur & CHANNOMSG))
      add_mode(chan, '+', 'n', "");
    else if ((mns & CHANNOMSG) && (cur & CHANNOMSG))
      add_mode(chan, '-', 'n', "");
    if ((pls & CHANANON) && !(cur & CHANANON))
      add_mode(chan, '+', 'a', "");
    else if ((mns & CHANANON) && (cur & CHANANON))
      add_mode(chan, '-', 'a', "");
    if ((pls & CHANQUIET) && !(cur & CHANQUIET))
      add_mode(chan, '+', 'q', "");
    else if ((mns & CHANQUIET) && (cur & CHANQUIET))
      add_mode(chan, '-', 'q', "");
    if ((chan->limit_prot != 0) && (chan->channel.maxmembers == 0)) {
      char s[11] = "";

      simple_snprintf(s, sizeof(s), "%d", chan->limit_prot);
      add_mode(chan, '+', 'l', s);
    } else if ((mns & CHANLIMIT) && (chan->channel.maxmembers != 0))
      add_mode(chan, '-', 'l', "");
    if (chan->key_prot[0]) {
      if (rfc_casecmp(chan->channel.key, chan->key_prot) != 0) {
        if (chan->channel.key[0])
	  add_mode(chan, '-', 'k', chan->channel.key);
        add_mode(chan, '+', 'k', chan->key_prot);
      }
    } else if ((mns & CHANKEY) && (chan->channel.key[0]))
      add_mode(chan, '-', 'k', chan->channel.key);
  }
}

static void check_this_member(struct chanset_t *chan, memberlist *m,
    struct flag_record *fr)
{
  if (!m || m->is_me || !me_op(chan))
    return;

  /* +d or bitch and not an op
   * we dont check private because +private does not imply bitch. */
  if (chan_hasop(m) && 
      (chk_deop(*fr, chan) ||
       (!loading && userlist && chan_bitch(chan) && !chk_op(*fr, chan)) ) ) {
    /* if (target_priority(chan, m, 1)) */
      add_mode(chan, '-', 'o', m);
  } else if (!chan_hasop(m) && (chan->role & ROLE_OP) && chk_autoop(m, *fr, chan)) {
    do_op(m, chan, 1, 0);
  }
  if (dovoice(chan)) {
    if (chan_hasvoice(m) && !chan_hasop(m)) {
      /* devoice +q users .. */
      if (chk_devoice(*fr) || (channel_voicebitch(chan) && !chk_voice(m, *fr, chan)))
        add_mode(chan, '-', 'v', m);
    } else if (!chan_hasvoice(m) && !chan_hasop(m)) {
      /* voice +v users */
      if (chk_voice(m, *fr, chan)) {
        add_mode(chan, '+', 'v', m);
        if (m->flags & EVOICE)
          m->flags &= ~EVOICE;
      }
    }
  }

  const char *s = NULL;
  for (int i = 0; i < (m->userip[0] ? 2 : 1); ++i) {
    if (i == 0 && m->userip[0]) // Check userip first in case userhost is already an ip
      s = m->fromip;
    else
      s = m->from;

    /* check vs invites */
    if (use_invites &&
        (u_match_mask(global_invites,s) ||
         u_match_mask(chan->invites, s)))
      refresh_invite(chan, s);
    /* don't kickban if permanent exempted */
    if (!(use_exempts &&
          (u_match_mask(global_exempts, s) ||
           u_match_mask(chan->exempts, s)))) {

      /* Are they banned in the internal list? */
      if (u_match_mask(global_bans, s) || u_match_mask(chan->bans, s))
        refresh_ban_kick(chan, m, s);

      /* are they +k ? */
      if (!chan_sentkick(m) && (chan_kick(*fr) || glob_kick(*fr)) && me_op(chan)) {
        char *p = (char *) get_user(&USERENTRY_COMMENT, m->user);

        check_exemptlist(chan, s);
        quickban(chan, s);
        dprintf(DP_SERVER, "KICK %s %s :%s%s\n", chan->name, m->nick, bankickprefix, p ? p : response(RES_KICKBAN));
        m->flags |= SENTKICK;
      }
    }
  }
}


//1 -user
//2 -host
void check_this_user(char *hand, int del, char *host)
{
  memberlist *m = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };

  for (struct chanset_t *chan = chanset; chan; chan = chan->next) {
    for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
      bool check_member = 0;
      bool had_user = m->user ? 1 : 0;
      struct userrec* u;
      bool matches_hand = false;

      m->tried_getuser = 0;
      member_getuser(m);
      u = m->user;
      if (u) {
        matches_hand = (strcasecmp(u->handle, hand) == 0);
      }

      if (u && u->bot && matches_hand) {
        /* Newly discovered bot, or deleted bot which fullfilled a role,
         * need to rebalance. */
        if (!del || (del && (*chan->bot_roles)[u->handle] != 0)) {
          chan->needs_role_rebalance = 1;
        }
      }
      if (m->user && !had_user) // If a member is newly recognized, act on it
        check_member = 1;
      else if (del != 2 && m->user && matches_hand) { //general check / -user, match specified user
        check_member = 1;
        if (del == 1)
          u = NULL; // Pretend user doesn't exist when checking
      } else if (0 && del == 2) { //-host, may now match on a diff user and need to act on them
        /*
        const char *s = NULL;
        for (int i = 0; i < (m->userip[0] ? 2 : 1); ++i) {
          if (i == 0 && m->userip[0]) // Check userip first in case userhost is already an ip
            s = m->fromip;
          else
            s = m->from;
          if (wild_match(host, s) ||
              (i == 0 && m->userip[0] && match_cidr(host, s))) {
            check_member = 1;
            break;
          }
        }
        */
        if (m->user && !had_user)
          check_member = 1;
      }
      if (check_member) {
        get_user_flagrec(u, &fr, chan->dname, chan);
        check_this_member(chan, m, &fr);
      }
    }
  }
}

static void enforce_bitch(struct chanset_t *chan, bool flush = 1) {
  if (!chan || !me_op(chan)) 
    return;
  priority_do(chan, 1, PRIO_DEOP, flush);
}

void enforce_closed(struct chanset_t *chan) {
  if (!chan || !me_op(chan)) 
    return;

  char buf[3] = "", *p = buf;

  if (chan->closed_invite && !(chan->channel.mode & CHANINV) && !(chan->mode_mns_prot & CHANINV))
    *p++ = 'i';
  if (chan->closed_private && !(chan->channel.mode & CHANPRIV) && !(chan->mode_mns_prot & CHANPRIV))
    *p++ = 'p';
  *p = 0;
  if (buf[0])
    dprintf(DP_MODE, "MODE %s +%s\n", chan->name, buf);
  priority_do(chan, 0, PRIO_KICK);
}

inline static char *
take_massopline(char *op, char **to_op)
{
  const size_t modes_len = 31, nicks_len = 151;
  char *nicks = (char *) calloc(1, nicks_len),
       *modes = (char *) calloc(1, modes_len),
       *nick = NULL;
  bool useop = 0;
  static char ret[182] = "";

  memset(ret, 0, sizeof(ret));
  for (unsigned int i = 0; i < modesperline; i++) {
    if (*to_op[0] || op) {
      /* if 'op' then use it, then move on to to_op */
      if (!useop && op) {
        nick = op;
        useop = 1;
      } else if (*to_op[0])
        nick = newsplit(to_op);
      if (nick) {
        strlcat(modes, "+o", modes_len);
        strlcat(nicks, nick, nicks_len);
        if (i != modesperline - 1)
          strlcat(nicks, " ", nicks_len);
      }
    }
  }
  
  strlcat(ret, modes, sizeof(ret));
  strlcat(ret, " ", sizeof(ret));
  strlcat(ret, nicks, sizeof(ret));
  free(modes);
  free(nicks);
  
  return ret;
}

inline static char *
take_makeline(char *op, char *deops, unsigned int deopn, size_t deops_len)
{
  bool opn = op ? 1 : 0;
  unsigned int n = opn + deopn;		/* op + deops */
  unsigned int pos = randint(deopn), i;
  static char ret[151] = "";

  memset(ret, 0, sizeof(ret));
  for (i = 0; i < n; i++) {
    if (opn && i == pos)
      strlcat(ret, "+o", sizeof(ret));
    else if (deopn)
      strlcat(ret, "-o", sizeof(ret));
  }

  strlcat(ret, " ", sizeof(ret));

  for (i = 0; i < n; i++) {
    if (opn && i == pos)
      strlcat(ret, op, sizeof(ret));
    else if (deopn)
      strlcat(ret, newsplit(&deops), sizeof(ret));

    if (i != n - 1)
      strlcat(ret, " ", sizeof(ret));
  }
  return ret;  
}

static void
do_take(struct chanset_t *chan)
{
  char to_deop[2048] = "", *to_deop_ptr = to_deop;
  char to_op[2048] = "", *to_op_ptr = to_op;
  size_t to_op_len = 0, to_deop_len = 0;

  /* Make lists of who needs to be opped, and who needs to be deopped */
  for (memberlist *m = chan->channel.member; m && m->nick[0]; m = m->next) {
    if (!m->is_me) {
      const bool isbot = m->user && m->user->bot ? 1 : 0;

      /* Avoid countless unneeded operations from strcat/strlen */
      if (isbot && !(m->flags & CHANOP)) {
        to_op_len += strlcpy(to_op + to_op_len, m->nick, sizeof(to_op) - to_op_len);
        *(to_op + to_op_len++) = ' ';
      } else if (!isbot && (m->flags & CHANOP)) {
        to_deop_len += strlcpy(to_deop + to_deop_len, m->nick, sizeof(to_deop) - to_deop_len);
        *(to_deop + to_deop_len++) = ' ';
      }
    }
  }
  shuffle(to_op, " ", sizeof(to_op));
  shuffle(to_deop, " ", sizeof(to_deop));

  size_t deops_len = 0;
  size_t work_len = 0;
  short lines = 0;
  const unsigned short max_lines = floodless ? 15 : default_alines;
  char work[2048] = "", *op = NULL, *modeline = NULL, deops[2048] = "";
  unsigned int deopn;

  while (to_op_ptr[0] || to_deop_ptr[0]) {
    deops_len = 0;
    deopn = 0;
    op = NULL;
    modeline = NULL;
    deops[0] = 0;

    if (to_op_ptr[0])
      op = newsplit(&to_op_ptr);

    /* Prepare a list of modesperline-1 nicks for deop */
    for (unsigned int i = 0; i < modesperline; i++) {
      if (to_deop_ptr[0] && ((i < (modesperline - 1)) || (!op))) {
        ++deopn; 
        const char *deop_nick = newsplit(&to_deop_ptr);
        deops_len += strlcpy(deops + deops_len, deop_nick, sizeof(deops) - deops_len);
        *(deops + deops_len++) = ' ';
      }
    }
    deops[deops_len] = 0;
    *(work + work_len++) = 'M';
    *(work + work_len++) = 'O';
    *(work + work_len++) = 'D';
    *(work + work_len++) = 'E';
    *(work + work_len++) = ' ';
    work_len += strlcpy(work + work_len, chan->name, sizeof(work) - work_len);
    *(work + work_len++) = ' ';

    if (deops[0])
      modeline = take_makeline(op, deops, deopn, deops_len);
    else
      modeline = take_massopline(op, &to_op_ptr);

    work_len += strlcpy(work + work_len, modeline, sizeof(work) - work_len);
    *(work + work_len++) = '\r';
    *(work + work_len++) = '\n';
    work[work_len] = 0;

    // Prevent excess flood
    if (++lines >= max_lines) {
      tputs(serv, work, work_len);
      work[0] = 0;
      work_len = 0;
      lines = 0;
    }
  }

  if (work[0])
    tputs(serv, work, work_len);

  if (channel_closed(chan))
    enforce_closed(chan);

  enforce_bitch(chan);		/* hell why not! */

  return;
}

/* Things to do when i just became a chanop:
 */
void recheck_channel(struct chanset_t *chan, int dobans)
{
  static int stacking = 0;

  if (stacking)
    return;			/* wewps */

  if (!userlist || loading)                /* Bot doesnt know anybody */
    return;        		           /* ... it's better not to deop everybody */

  memberlist *m = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  int stop_reset = 0, botops = 0, nonbotops = 0, botnonops = 0;
  bool flush = 0;

  ++stacking;

  putlog(LOG_DEBUG, "*", "recheck_channel(%s, %d)", chan->dname, dobans);

  for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
    bool hasop = chan_hasop(m);

    if (m) {
      if (m->user && m->user->bot && (m->user->flags & USER_OP)) {
        if (hasop)
          ++botops;
        else
          ++botnonops;
      } else if (hasop)
        ++nonbotops;
    }
  }


  /* don't do much if i'm lonely bot. Maybe they won't notice me? :P */
  if (unlikely(botops == 1 && !botnonops)) {
    if (chan_bitch(chan) || channel_closed(chan))
      putlog(LOG_MISC, "*", "Opped in %s, not checking +closed/+bitch until more bots arrive.", chan->dname);
  } else {
    /* if the chan is +closed, mass deop first, safer that way. */
    if (chan_bitch(chan) || channel_closed(chan)) {
      flush = 1;
      enforce_bitch(chan, 0);
    }

    if (channel_closed(chan))
      enforce_closed(chan);
  }

  /* this can all die, we want to enforce +bitch/+take first :) */
  if (!channel_take(chan)) {

    /* This is a bad hack for +e/+I */
    if (dobans == 2 && chan->channel.members > 1) {
      get_channel_masks(chan);
    }

    //Check +d/+O/+k
    for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
      member_getuser(m);
      get_user_flagrec(m->user, &fr, chan->dname, chan);
      //Already a bot opped, dont bother resetting masks
      if (glob_bot(fr) && chan_hasop(m) && !m->is_me)
        stop_reset = 1;
      check_this_member(chan, m, &fr);
    }

    //Only reset masks if the bot has already received the ban list before (meaning it has already been opped once)
    //Ie, don't set bans without knowing what they are! (asked for above on first op)
    if (dobans && (chan->ircnet_status & CHAN_HAVEBANS)) {
      if (channel_nouserbans(chan) && !stop_reset)
        resetbans(chan);
      else
        recheck_bans(chan);
      if (use_invites && !(chan->ircnet_status & CHAN_ASKED_INVITES)) {
        if (channel_nouserinvites(chan) && !stop_reset)
          resetinvites(chan);
        else
          recheck_invites(chan);
      }
      if (use_exempts && !(chan->ircnet_status & CHAN_ASKED_EXEMPTS)) {
        if (channel_nouserexempts(chan) && !stop_reset)
          resetexempts(chan);
        else
          recheck_exempts(chan);
      } else {
        if (channel_enforcebans(chan)) 
          enforce_bans(chan);
      }

      flush = 1;
    }

    // Do this here as the above only runs after already having been opped and having gotten bans.
    if (dobans) {
      if ((chan->ircnet_status & CHAN_ASKEDMODES) && !channel_inactive(chan))
        dprintf(DP_MODE, "MODE %s\n", chan->name);
      recheck_channel_modes(chan);
    }

    if (flush)
      flush_mode(chan, QUICK);
  }
  --stacking;
}

static int got001(char *from, char *msg)
{
  //Just connected, cleanup some vars
  chained_who.clear();
  return 0;
}

#ifdef CACHE
/* got 302: userhost
 * <server> 302 <to> :<nick??user@host>
 */
static int got302(char *from, char *msg)
{
  char *p = NULL, *nick = NULL, *uhost = NULL;

  cache_t *cache = NULL;
  cache_chan_t *cchan = NULL;

  newsplit(&msg);
  fixcolon(msg);
  
  p = strchr(msg, '=');
  if (!p)
    p = strchr(msg, '*');
  if (!p)
    return 0;
  *p = 0;
  nick = msg;
  p += 2;		/* skip =|* plus the next char */
  uhost = p;

  if ((p = strchr(uhost, ' ')))
    *p = 0;

  if ((cache = cache_find(nick))) {
    if (!cache->uhost[0])
    strlcpy(cache->uhost, uhost, sizeof(cache->uhost));

    if (!cache->handle[0]) {
      char s[UHOSTLEN] = "";
      struct userrec *u = NULL;

      simple_snprintf(s, sizeof(s), "%s!%s", nick, uhost);
      if ((u = get_user_by_host(s)))
        strlcpy(cache->handle, u->handle, sizeof(cache->handle));
    }
    cache->timeval = now;
 
    /* check if we should invite this client to chans */
    for (cchan = cache->cchan; cchan && cchan->dname[0]; cchan = cchan->next) {
      if (cchan->invite) {
        dprintf(DP_SERVER, "INVITE %s %s\n", nick, cchan->dname);
        cchan->invite = 0;
        cchan->invited = 1;
      }
      if (cchan->ban) {
        cchan->ban = 0;
        dprintf(DP_MODE_NEXT, "MODE %s +b *!%s\n", cchan->dname, uhost);
      }
    }
  }
  return 0;
}
#endif

#ifdef CACHE
/* got341 invited
 * <server> 341 <to> <nick> <channel>
 */
static int got341(char *from, char *msg)
{
  char *nick = NULL, *chname = NULL;
  cache_t *cache = NULL;
  cache_chan_t *cchan = NULL;

  newsplit(&msg);
  nick = newsplit(&msg);
  chname = newsplit(&msg);

  struct chanset_t *chan = findchan(chname);

  if (!chan) {
    putlog(LOG_MISC, "*", "%s: %s", "Hmm, mode info from a channel I'm not on", chname);
    dprintf(DP_SERVER, "PART %s\n", chname);
    return 0;
  }

  cache = cache_find(nick);

  if (cache) {
    for (cchan = cache->cchan; cchan && cchan->dname; cchan = cchan->next) {
      if (!rfc_casecmp(cchan->dname, chan->dname)) {
        if (!cache->uhost[0] || !cchan->invited)
          goto hijack;

        cache->timeval = now;
        notice_invite(chan, cache->handle[0] ? cache->handle : NULL, nick, cache->uhost, cchan->op);

//        cache_del_chan_parm(cache, cache->cchan);
        break;
      }
    }
  }

  if (!cache || !cchan)
    goto hijack;

  return 0;

  hijack:

  if (!cache)
    cache = cache_new(nick);
  if (!cchan)
    cchan = cache_chan_add(cache, chan->dname);

  if (!cache->uhost[0]) {
    dprintf(DP_MODE_NEXT, "MODE %s +b %s!*@*\n", chan->name, nick);
    cchan->ban = 1;
    dprintf(DP_MODE_NEXT, "USERHOST %s\n", nick);
  } else {
    dprintf(DP_MODE_NEXT, "MODE %s +b *!*%s\n", chan->name, cache->uhost);
  }
  putlog(LOG_MISC, "*", "HIJACKED invite detected: %s to %s", nick, chan->dname);
  bd::String msg;
  msg = bd::String::printf("ALERT! \002%s was invited via a hijacked connection/process.\002", nick);
  privmsg(chan->name, msg, DP_MODE_NEXT);
  return 0;
}
#endif /* CACHE */

/* got 710: knock
 * <server> 710 <to> <channel> <from> :<reason?>
 * ratbox :irc.umich.edu 710 #wraith #wraith bryand!bryan@oper.blessed.net :has asked for an invite.
 * csircd :irc.nac.net 710 * bryan_!bryan@pluto.xzibition.com has knocked on channel #wraith for an invite
 *
 */
static int got710(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;
  char buf[UHOSTLEN] = "", *uhost = buf, *nick;

  chname = newsplit(&msg);
  if (!strcmp(chname, "*")) {
    //csircd
    uhost = newsplit(&msg);
    //has knocked on channel #CHAN for an invite
    newsplit(&msg); //has
    newsplit(&msg); //knocked
    newsplit(&msg); //on
    newsplit(&msg); //channel
    chname = newsplit(&msg);
  } else {
    //Hybrid/ratbox
    newsplit(&msg); //not used
    uhost = newsplit(&msg);
  }

  if (!strchr(CHANMETA, chname[0]) || !strchr(uhost, '!'))
    return 0;

  chan = findchan(chname);

  if (!chan->knock_flags || !(chan->role & ROLE_INVITE))
    return 0;

  struct userrec *u = get_user_by_host(uhost);

  if (!u)
    return 0;

  nick = splitnick(&uhost);

  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };

  get_user_flagrec(u, &fr, chan->dname, chan);

  // PASSING: +o and op || +v and op/voice || user
  if (!((chan->knock_flags == CHAN_FLAG_OP && chk_op(fr, chan)) ||
       (chan->knock_flags == CHAN_FLAG_VOICE && (chk_op(fr, chan) || chk_voice(NULL, fr, chan))) ||
       (chan->knock_flags == CHAN_FLAG_USER)) ||
      chan_kick(fr) || glob_kick(fr)) {
    return 0;
  }

//  dprintf(DP_HELP, "PRIVMSG %s :%s knocked, inviting.. (%s)\n", chname, nick, u ? u->handle : "");
  cache_invite(chan, nick, uhost, u ? u->handle : NULL, 0, 0);

  return 0;
}

/* got 324: mode status
 * <server> 324 <to> <channel> <mode>
 */
static int got324(char *from, char *msg)
{
  char *chname = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  if (match_my_nick(chname))
      return 0;

  struct chanset_t *chan = findchan(chname);

  if (!chan) {
    putlog(LOG_MISC, "*", "%s: %s", "Hmm, mode info from a channel I'm not on", chname);
    dprintf(DP_SERVER, "PART %s\n", chname);
    return 0;
  }

  // Sanity check, there may not be a mode returned if the server is sending bad data.
  if (!msg[0]) return 0;

  int i = 1;
  bool ok = 0;
  char *p = NULL, *q = NULL;

  if (chan->ircnet_status & CHAN_ASKEDMODES)
    ok = 1;
  chan->ircnet_status &= ~CHAN_ASKEDMODES;
  chan->channel.mode = 0;
  while (msg[i] != 0) {
    if (msg[i] == 'i')
      chan->channel.mode |= CHANINV;
    if (msg[i] == 'p')
      chan->channel.mode |= CHANPRIV;
    if (msg[i] == 's')
      chan->channel.mode |= CHANSEC;
    if (msg[i] == 'm')
      chan->channel.mode |= CHANMODER;
    if (msg[i] == 'c')
      chan->channel.mode |= CHANNOCLR;
    if (msg[i] == 'C')
      chan->channel.mode |= CHANNOCTCP;
    if (msg[i] == 'R')
      chan->channel.mode |= CHANREGON;
    if (msg[i] == 'M')
      chan->channel.mode |= CHANMODR;
    if (msg[i] == 'r')
      chan->channel.mode |= CHANLONLY;
    if (msg[i] == 't')
      chan->channel.mode |= CHANTOPIC;
    if (msg[i] == 'n')
      chan->channel.mode |= CHANNOMSG;
    if (msg[i] == 'a')
      chan->channel.mode |= CHANANON;
    if (msg[i] == 'q')
      chan->channel.mode |= CHANQUIET;
    if (msg[i] == 'k') {
      chan->channel.mode |= CHANKEY;
      p = strchr(msg, ' ');
      if (p != NULL) {		/* Test for null key assignment */
	p++;
	q = strchr(p, ' ');
	if (q != NULL) {
	  *q = 0;
	  my_setkey(chan, p);
	  strcpy(p, q + 1);
	} else {
	  my_setkey(chan, p);
	  *p = 0;
	}
      }
      if ((chan->channel.mode & CHANKEY) && (!chan->channel.key[0] ||
	  !strcmp("*", chan->channel.key)))
	/* Undernet use to show a blank channel key if one was set when
	 * you first joined a channel; however, this has been replaced by
	 * an asterisk and this has been agreed upon by other major IRC 
	 * networks so we'll check for an asterisk here as well 
	 * (guppy 22Dec2001) */ 
        chan->ircnet_status |= CHAN_ASKEDMODES;
    }
    if (msg[i] == 'l') {
      p = strchr(msg, ' ');
      if (p != NULL) {		/* test for null limit assignment */
	p++;
	q = strchr(p, ' ');
	if (q != NULL) {
	  *q = 0;
	  chan->channel.maxmembers = atoi(p);
/*	  strcpy(p, q + 1); */
          simple_sprintf(p, "%s", q + 1);
	} else {
	  chan->channel.maxmembers = atoi(p);
	  *p = 0;
	}
      }
    }
    i++;
  }
  if (ok)
    recheck_channel_modes(chan);
  return 0;
}

static void memberlist_reposition(struct chanset_t *chan, memberlist *target) {
  /* Move target from it's current position to it's correct sorted position */
  memberlist *old = NULL, *m = NULL;
  if (chan->channel.member == target) {
    chan->channel.member = target->next;
  } else {
    for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
      if (m->next == target) {
        m->next = target->next;
        break;
      }
    }
  }
  target->next = NULL;
  for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
    if (rfc_casecmp(m->nick, target->nick)>0) {
      if (old) {
        target->next = m;
        old->next = target;
      } else {
        target->next = chan->channel.member;
        chan->channel.member = target;
      }
      return;
    }
    old = m;
  }
  if (old) {
    target->next = old->next;
    old->next = target;
  } else {
    target->next = NULL;
    chan->channel.member = target;
  }
}


static int got352or4(struct chanset_t *chan, char *user, char *host, char *nick, char *flags, int hops, char* realname, char* ip)
{
  memberlist *m = ismember(chan, nick);	/* in my channel list copy? */
//  bool waschanop = 0;
//  struct chanset_t *ch = NULL;
//  memberlist *ml = NULL;

  if (!m) {			/* Nope, so update */
    m = newmember(chan, nick);	/* Get a new channel entry */
    m->last = now;		/* Last time I saw him */

    /* Store the userhost */
    simple_snprintf(m->userhost, sizeof(m->userhost), "%s@%s", user, host);
    simple_snprintf(m->from, sizeof(m->from), "%s!%s", m->nick, m->userhost);
    member_update_from_cache(chan, m);

    if (!m->userip[0]) {
      if (ip)
        simple_snprintf(m->userip, sizeof(m->userip), "%s@%s", user, ip);
      else if (is_dotted_ip(host))
        simple_snprintf(m->userip, sizeof(m->userip), "%s@%s", user, host);
      simple_snprintf(m->fromip, sizeof(m->fromip), "%s!%s", m->nick, m->userip);
    }
  }


  m->hops = hops;

  /* Propagate hops to other channel memlists... might save us a WHO #chan */
/* FIXME: great concept, HORRIBLE CPU USAGE 
  for (ch = chanset; ch; ch = ch->next) {
    if (ch != chan) {
      for (ml = ch->channel.member; ml && ml->nick[0]; ml = ml->next) {
        if (!strcmp(ml->nick, m->nick)) {
          ml->hops = m->hops;
          break;
        }
      }
    }
  }
*/

//  waschanop = me_op(chan);      /* Am I opped here? */

  if (strchr(flags, '@') != NULL)	/* Flags say he's opped? */
    m->flags |= (CHANOP | WASOP);	/* Yes, so flag in my table */
  else
    m->flags &= ~(CHANOP | WASOP);
  if (strchr(flags, '*'))
    m->flags |= OPER;
  else
    m->flags &= ~OPER;
  if (strchr(flags, '+') != NULL)	/* Flags say he's voiced? */
    m->flags |= CHANVOICE;	/* Yes */
  else
    m->flags &= ~CHANVOICE;
//  if (!(m->flags & (CHANVOICE | CHANOP)))
//    m->flags |= STOPWHO;

  member_getuser(m);

  //This bot is set +r, so resolve.
  if (unlikely(doresolv(chan))) {
    if (!m->userip[0])
      resolve_to_member(chan, nick, host);
    else if (!m->user && m->userip[0] && channel_rbl(chan))
      resolve_to_rbl(chan, m->userip);
  }

  // If there's an opped bot, queue op to request_op
  if (m->user && m->user->bot && chan_hasop(m)) {
    chan->channel.do_opreq = 1;
  }

  return 0;
}

/* got a 352: who info!
 */
static int got352(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);		/* Skip my nick - effeciently */
  chname = newsplit(&msg);	/* Grab the channel */
  chan = findchan(chname);	/* See if I'm on channel */
  if (chan) {			/* Am I? */
    char *nick = NULL, *user = NULL, *host = NULL, *flags = NULL, *hops = NULL, *realname = NULL;
    int real_hops = 1;

    user = newsplit(&msg);	/* Grab the user */
    host = newsplit(&msg);	/* Grab the host */
    newsplit(&msg);		/* skip the server */
    nick = newsplit(&msg);	/* Grab the nick */
    flags = newsplit(&msg);	/* Grab the flags */
    hops = newsplit(&msg);	/* grab server hops */
    if (hops[0] == ':')
      ++hops;			/* Skip the : */
    real_hops = atoi(hops);
    realname = newsplit(&msg);	/* realname/gecos */
    got352or4(chan, user, host, nick, flags, real_hops, realname, NULL);
  }
  return 0;
}

/* got a 354: who info! - iru style
 */
static int got354(char *from, char *msg)
{
  if (use_354) {
    newsplit(&msg);		/* Skip my nick - effeciently */
    if (msg[0] && (strchr(CHANMETA, msg[0]) != NULL)) {
      char *chname = newsplit(&msg);	/* Grab the channel */
      struct chanset_t *chan = findchan(chname);	/* See if I'm on channel */

      if (chan) {		/* Am I? */
        char *nick = NULL, *user = NULL, *host = NULL, *flags = NULL, *hops = NULL, *realname = NULL, *ip = NULL;
        int real_hops = 1;

	user = newsplit(&msg);	/* Grab the user */
        ip = newsplit(&msg);    /** Get the numeric IP :) */
	host = newsplit(&msg);	/* Grab the host */
	nick = newsplit(&msg);	/* Grab the nick */
	flags = newsplit(&msg);	/* Grab the flags */
        hops = newsplit(&msg);  /* server hops */
        if (hops[0] == ':')
          ++hops;			/* Skip the : */
        real_hops = atoi(hops);
        realname = newsplit(&msg); /* realname/gecos */
	got352or4(chan, user, host, nick, flags, real_hops, realname, ip);
      }
    }
  }
  return 0;
}

/* got 315: end of who
 * <server> 315 <to> <chan> :End of /who
 */
static int got315(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);

  putlog(LOG_DEBUG, "*", "END who %s", chname);

  if (!chained_who.empty()) {
    // Send off next WHO request
    while (1) {
      if (chained_who.empty()) break;
      // Dequeue the next chan in the chain
      chan = findchan(chained_who.front().c_str());
      chained_who.pop_front();
      if (chan) {
        if (!strcmp(chan->name, chname)) continue; // First reply got queued too
        if (!shouldjoin(chan)) continue; // No longer care about this channel
        // Somehow got the WHO already
        if (channel_active(chan) && !channel_pending(chan)) continue;
        send_chan_who(chained_who_idx, chan);
        break;
      }
    }
  }

  chan = findchan(chname);

  /* May have left the channel before the who info came in */
  if (!chan || !channel_pending(chan) || !shouldjoin(chan))
    return 0;

  /* Finished getting who list, can now be considered officially ON CHANNEL */
  chan->ircnet_status |= CHAN_ACTIVE;
  chan->ircnet_status &= ~(CHAN_PEND | CHAN_JOINING);
  memberlist* me = ismember(chan, botname);
  /* Am *I* on the channel now? if not, well d0h. */
  if (!me) {
    putlog(LOG_MISC | LOG_JOIN, chan->dname, "Oops, I'm not really on %s.", chan->dname);
    force_join_chan(chan);
  } else {
    me->is_me = 1;
    if (!me->joined)
      me->joined = now;				/* set this to keep the whining masses happy */
    rebalance_roles_chan(chan);
    if (me_op(chan))
      recheck_channel(chan, 2);
    else if (chan->channel.members == 1)
      chan->ircnet_status |= CHAN_STOP_CYCLE;
    else if (chan->channel.do_opreq) {
      request_op(chan);
    }
  }
  /* do not check for i-lines here. */
  return 0;
}

/* got 367: ban info
 * <server> 367 <to> <chan> <ban> [placed-by] [timestamp]
 */
static int got367(char *from, char *origmsg)
{
  char *chname = NULL, buf[511] = "", *msg = NULL;
  struct chanset_t *chan = NULL;

  strlcpy(buf, origmsg, sizeof(buf));
  msg = buf;
  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (!chan || !(channel_pending(chan) || channel_active(chan)))
    return 0;
  
  char *ban = newsplit(&msg), *who = newsplit(&msg);

  /* Extended timestamp format? */
  if (who[0])
    newban(chan, ban, who);
  else
    newban(chan, ban, "existent");
  return 0;
}

/* got 368: end of ban list
 * <server> 368 <to> <chan> :etc
 */
static int got368(char *from, char *msg)
{
  struct chanset_t *chan = NULL;
  char *chname = NULL;

  /* Okay, now add bans that i want, which aren't set yet */
  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (chan) {
    chan->ircnet_status &= ~CHAN_ASKEDBANS;
    chan->ircnet_status |= CHAN_HAVEBANS;

    if (channel_nouserbans(chan))
      resetbans(chan);
    else
      recheck_bans(chan);
  }
  /* If i sent a mode -b on myself (deban) in got367, either
   * resetbans() or recheck_bans() will flush that.
   */
  return 0;
}

/* got 348: ban exemption info
 * <server> 348 <to> <chan> <exemption>
 */
static int got348(char *from, char *origmsg)
{
  if (use_exempts == 0)
    return 0;

  char *chname = NULL, buf[511] = "", *msg = NULL;
  struct chanset_t *chan = NULL;

  strlcpy(buf, origmsg, sizeof(buf));
  msg = buf;
  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (!chan || !(channel_pending(chan) || channel_active(chan)))
    return 0;
  
  char *exempt = newsplit(&msg), *who = newsplit(&msg);

  /* Extended timestamp format? */
  if (who[0])
    newexempt(chan, exempt, who);
  else
    newexempt(chan, exempt, "existent");
  return 0;
}

/* got 349: end of ban exemption list
 * <server> 349 <to> <chan> :etc
 */
static int got349(char *from, char *msg)
{
  if (use_exempts == 1) {
    struct chanset_t *chan = NULL;
    char *chname = NULL;

    newsplit(&msg);
    chname = newsplit(&msg);
    chan = findchan(chname);
    if (chan) {
      putlog(LOG_DEBUG, "*", "END +e %s", chan->dname);
      chan->ircnet_status &= ~CHAN_ASKED_EXEMPTS;
      
      if (channel_nouserexempts(chan))
        resetexempts(chan);
      else
        recheck_exempts(chan);

      if (channel_enforcebans(chan))
        enforce_bans(chan);
    }
  }
  return 0;
}

static void got353(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg); /* my nick */
  newsplit(&msg); /*    *|@|=  */
  chname = newsplit(&msg);
  chan = findchan(chname);
  fixcolon(msg);
  irc_log(chan, "%s", msg);
}

/* got 346: invite exemption info
 * <server> 346 <to> <chan> <exemption>
 */
static int got346(char *from, char *origmsg)
{
  if (use_invites == 0)
    return 0;

  char *chname = NULL, buf[511] = "", *msg = NULL;
  struct chanset_t *chan = NULL;

  strlcpy(buf, origmsg, sizeof(buf));
  msg = buf;
  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (!chan || !(channel_pending(chan) || channel_active(chan)))
    return 0;
  
  char *invite = newsplit(&msg), *who = newsplit(&msg);

  /* Extended timestamp format? */
  if (who[0])
    newinvite(chan, invite, who);
  else
    newinvite(chan, invite, "existent");
  return 0;
}

/* got 347: end of invite exemption list
 * <server> 347 <to> <chan> :etc
 */
static int got347(char *from, char *msg)
{
  if (use_invites == 1) {
    struct chanset_t *chan = NULL;
    char *chname = NULL;

    newsplit(&msg);
    chname = newsplit(&msg);
    chan = findchan(chname);
    if (chan) {
      chan->ircnet_status &= ~CHAN_ASKED_INVITES;

      if (channel_nouserinvites(chan))
        resetinvites(chan);
      else
        recheck_invites(chan);
    }
  }
  return 0;
}

/* Too many channels.
 */
static int got405(char *from, char *msg)
{
  char *chname = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  putlog(LOG_MISC, "*", "I'm on too many channels--can't join: %s", chname);
  return 0;
}

/* This is only of use to us with !channels. We get this message when
 * attempting to join a non-existant !channel... The channel must be
 * created by sending 'JOIN !!<channel>'. <cybah>
 *
 * 403 - ERR_NOSUCHCHANNEL
 */
static int got403(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  if (chname && chname[0]=='!') {
    chan = findchan_by_dname(chname);
    if (!chan) {
      chan = findchan(chname);
      if (!chan)
        return 0;       /* Ignore it */
      /* We have the channel unique name, so we have attempted to join
       * a specific !channel that doesnt exist. Now attempt to join the
       * channel using it's short name.
       */
      putlog(LOG_MISC, "*",
             "Unique channel %s does not exist... Attempting to join with "
             "short name.", chname);
      chan->name[0] = 0;
      join_chan(chan);
    } else {
      /* We have found the channel, so the server has given us the short
       * name. Prefix another '!' to it, and attempt the join again...
       */
      putlog(LOG_MISC, "*",
             "Channel %s does not exist... Attempting to create it.", chname);
      dprintf(DP_SERVER, "JOIN !%s\n", chan->dname);
    }
  }
  return 0;
}

/* got 471: can't join channel, full
 */
static int got471(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  /* !channel short names (also referred to as 'description names'
   * can be received by skipping over the unique ID.
   */
  if ((chname[0] == '!') && (strlen(chname) > CHANNEL_ID_LEN)) {
    chname += CHANNEL_ID_LEN;
    chname[0] = '!';
  }
  /* We use dname because name is first set on JOIN and we might not
   * have joined the channel yet.
   */
  chan = findchan_by_dname(chname);
  if (chan) {
    chan->ircnet_status &= ~CHAN_JOINING;
    putlog(LOG_JOIN, chan->dname, "Channel full--can't join: %s", chan->dname);
    request_in(chan);
/* need: limit */
    chan = findchan_by_dname(chname); 
    if (!chan)
      return 0;
  } else
    putlog(LOG_JOIN, chname, "Channel full--can't join: %s", chname);
  return 0;
}

/* got 473: can't join channel, invite only
 */
static int got473(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  /* !channel short names (also referred to as 'description names'
   * can be received by skipping over the unique ID.
   */
  if ((chname[0] == '!') && (strlen(chname) > CHANNEL_ID_LEN)) {
    chname += CHANNEL_ID_LEN;
    chname[0] = '!';
  }
  /* We use dname because name is first set on JOIN and we might not
   * have joined the channel yet.
   */
  chan = findchan_by_dname(chname);
  if (chan) {
    chan->ircnet_status &= ~CHAN_JOINING;
    putlog(LOG_JOIN, chan->dname, "Channel invite only--can't join: %s", chan->dname);
    request_in(chan);
/* need: invite */
    chan = findchan_by_dname(chname); 
    if (!chan)
      return 0;
  } else
    putlog(LOG_JOIN, chname, "Channel invite only--can't join: %s", chname);
  return 0;
}

/* got 474: can't join channel, banned
 */
static int got474(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  /* !channel short names (also referred to as 'description names'
   * can be received by skipping over the unique ID.
   */
  if ((chname[0] == '!') && (strlen(chname) > CHANNEL_ID_LEN)) {
    chname += CHANNEL_ID_LEN;
    chname[0] = '!';
  }
  /* We use dname because name is first set on JOIN and we might not
   * have joined the channel yet.
   */
  chan = findchan_by_dname(chname);
  if (chan) {
    chan->ircnet_status &= ~CHAN_JOINING;
    putlog(LOG_JOIN, chan->dname, "Banned from channel--can't join: %s", chan->dname);
    request_in(chan);
/* need: unban */
    chan = findchan_by_dname(chname); 
    if (!chan)
      return 0;
  } else
    putlog(LOG_JOIN, chname, "Banned from channel--can't join: %s", chname);
  return 0;
}

/* got 475: can't join channel, bad key
 */
static int got475(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  /* !channel short names (also referred to as 'description names'
   * can be received by skipping over the unique ID.
   */
  if ((chname[0] == '!') && (strlen(chname) > CHANNEL_ID_LEN)) {
    chname += CHANNEL_ID_LEN;
    chname[0] = '!';
  }
  /* We use dname because name is first set on JOIN and we might not
   * have joined the channel yet.
   */
  chan = findchan_by_dname(chname);
  if (chan && shouldjoin(chan)) {
    chan->ircnet_status &= ~CHAN_JOINING;
    putlog(LOG_JOIN, chan->dname, "Bad key--can't join: %s", chan->dname);
    if (chan->channel.key[0]) {
      my_setkey(chan, NULL);
      join_chan(chan);
    } else {
      request_in(chan);
/* need: key */
    }
  } else
    putlog(LOG_JOIN, chname, "Bad key--can't join: %s", chname);
  return 0;
}

/* got 478: Channel ban list is full
 * [@] irc.blessed.net 478 wtest #wraith-devel *!*@host.com :Channel ban list is full
 */
static int got478(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);

  /* !channel short names (also referred to as 'description names'
   * can be received by skipping over the unique ID.
   */
  if ((chname[0] == '!') && (strlen(chname) > CHANNEL_ID_LEN)) {
    chname += CHANNEL_ID_LEN;
    chname[0] = '!';
  }
  /* We use dname because name is first set on JOIN and we might not
   * have joined the channel yet.
   */
  chan = findchan_by_dname(chname);
  if (chan && shouldjoin(chan)) {
    // Only lockdown if not already locked down
    if (!chan->channel.drone_set_mode) {
      lockdown_chan(chan, FLOOD_BANLIST);
    }
  }
  return 0;
}

/* got invitation
 */
static int gotinvite(char *from, char *msg)
{
  char *nick = NULL;
  struct chanset_t *chan = NULL;
  bool flood = 0;

  newsplit(&msg);
  fixcolon(msg);
  nick = splitnick(&from);
  /* Two invites to the same channel in 10 seconds? */
  if (!rfc_casecmp(last_invchan, msg))
    if (now - last_invtime < 10)
      flood = 1;
  if (!flood)
    putlog(LOG_MISC, "*", "%s!%s invited me to %s", nick, from, msg);
  strlcpy(last_invchan, msg, sizeof(last_invchan));
  last_invtime = now;
  chan = findchan(msg);
  if (!chan)
    /* Might be a short-name */
    chan = findchan_by_dname(msg);

  if (chan) {
    if (channel_pending(chan) || channel_active(chan))
      notice(nick, "I'm already here.", DP_HELP);
    else if (shouldjoin(chan))
      join_chan(chan);
  }
  return 0;
}

/* Set the topic.
 */
static void set_topic(struct chanset_t *chan, char *k)
{
  if (chan->channel.topic)
    free(chan->channel.topic);
  if (k && k[0]) {
    size_t tlen = strlen(k) + 1;
    chan->channel.topic = (char *) calloc(1, tlen);
    strlcpy(chan->channel.topic, k, tlen);
  } else
    chan->channel.topic = NULL;
}

/* Topic change.
 */
static int gottopic(char *from, char *msg)
{
  char *chname = newsplit(&msg), *nick = NULL;
  struct chanset_t *chan = NULL;

  fixcolon(msg);
  nick = splitnick(&from);
  chan = findchan(chname);
  if (chan) {
    memberlist *m = ismember(chan, nick);

    irc_log(chan, "%s!%s changed topic to: %s", nick, from, msg);
    if (m != NULL)
      m->last = now;
    set_topic(chan, msg);
  }
  return 0;
}

/* 331: no current topic for this channel
 * <server> 331 <to> <chname> :etc
 */
static int got331(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (chan) {
    set_topic(chan, NULL);
  }
  return 0;
}

/* 332: topic on a channel i've just joined
 * <server> 332 <to> <chname> :topic goes here
 */
static int got332(char *from, char *msg)
{
  struct chanset_t *chan = NULL;
  char *chname = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (chan) {
    fixcolon(msg);
    set_topic(chan, msg);
  }
  return 0;
}

/* Got a join
 */
static int gotjoin(char *from, char *chname)
{
  struct chanset_t *chan = NULL;

  fixcolon(chname);
  chan = findchan(chname);
  if (!chan && chname[0] == '!') {
    /* As this is a !channel, we need to search for it by display (short)
     * name now. This will happen when we initially join the channel, as we
     * dont know the unique channel name that the server has made up. <cybah>
     */
    size_t l_chname = strlen(chname);

    if (l_chname > (CHANNEL_ID_LEN + 1)) {
      char* ch_dname = (char *) calloc(1, l_chname + 1);
      simple_snprintf(ch_dname, l_chname + 2, "!%s", chname + (CHANNEL_ID_LEN + 1));
      chan = findchan_by_dname(ch_dname);
      if (!chan) {
      /* Hmm.. okay. Maybe the admin's a genius and doesn't know the
       * difference between id and descriptive channel names. Search
       * the channel name in the dname list using the id-name.
       */
        chan = findchan_by_dname(chname);
        if (chan) {
          /* Duh, I was right. Mark this channel as inactive and log
           * the incident.
           */
          chan->status |= CHAN_INACTIVE;
          putlog(LOG_MISC, "*", "Deactivated channel %s, because it uses "
                                "an ID channel-name. Use the descriptive name instead.", chname);
          dprintf(DP_SERVER, "PART %s\n", chname);

          free(ch_dname);
          return 0;
        }
      }
      free(ch_dname);
    }
  } else if (!chan) {
    /* As this is not a !chan, we need to search for it by display name now.
     * Unlike !chan's, we dont need to remove the unique part.
     */
    chan = findchan_by_dname(chname);
  }

  char *nick = NULL, buf[UHOSTLEN] = "", *uhost = buf;

  strlcpy(uhost, from, sizeof(buf));
  nick = splitnick(&uhost);

  if (!chan || (chan && !shouldjoin(chan))) {
    if (match_my_nick(nick)) {
      putlog(LOG_WARN, "*", "joined %s but didn't want to!", chname);
      dprintf(DP_MODE, "PART %s\n", chname);
    }
  } else if (!channel_pending(chan)) {
    char *host = NULL;

    chan->ircnet_status &= ~CHAN_STOP_CYCLE;

    if ((host = strchr(uhost, '@')))
      ++host;

    if (!channel_active(chan) && !match_my_nick(nick)) {
      /* uh, what?!  i'm on the channel?! */
      putlog(LOG_ERROR, "*", "confused bot: guess I'm on %s and didn't realize it", chan->dname);
      chan->ircnet_status |= CHAN_ACTIVE;
      chan->ircnet_status &= ~(CHAN_PEND | CHAN_JOINING);
      reset_chan_info(chan);
    } else {
      memberlist *m = ismember(chan, nick);
      bool splitjoin = 0;

      /* Net-join */
      if (m && m->split && !strcasecmp(m->userhost, uhost)) {
        splitjoin = 1;
	m->split = 0;
        --(chan->channel.splitmembers);
	m->last = now;
	m->delay = 0L;
	m->flags = (chan_hasop(m) ? WASOP : 0);
        /* New bot available for roles, rebalance. */
        if (is_bot(m->user)) {
          chan->needs_role_rebalance = 1;
        }
	set_handle_laston(chan->dname, m->user, now);
//	m->flags |= STOPWHO;
        irc_log(chan, "%s returned from netsplit", m->nick);

        if (!m->user) {
          member_getuser(m);
 
          if (!m->user && !m->userip[0] && doresolv(chan)) {
            if (is_dotted_ip(host)) {
              strlcpy(m->userip, uhost, sizeof(m->userip));
              simple_snprintf(m->fromip, sizeof(m->fromip), "%s!%s", m->nick, m->userip);
              if (channel_rbl(chan))
                resolve_to_rbl(chan, m->userip);
            } else
              resolve_to_member(chan, nick, host);
          }
        }
      } else {
	if (m)
	  killmember(chan, nick);
	m = newmember(chan, nick);
	m->joined = m->last = now;
	strlcpy(m->userhost, uhost, sizeof(m->userhost));
        member_update_from_cache(chan, m);
        simple_snprintf(m->from, sizeof(m->from), "%s!%s", m->nick, m->userhost);
        if (is_dotted_ip(host)) {
          strlcpy(m->userip, uhost, sizeof(m->userip));
          simple_snprintf(m->fromip, sizeof(m->fromip), "%s!%s", m->nick, m->userip);
        }
        member_getuser(m);

        if (!m->userip[0] && doresolv(chan))
          resolve_to_member(chan, nick, host);
        else if (!m->user && m->userip[0] && doresolv(chan) && channel_rbl(chan))
          resolve_to_rbl(chan, m->userip);

//	m->flags |= STOPWHO;

	if (match_my_nick(nick)) {
          m->is_me = 1;
	  /* It was me joining! Need to update the channel record with the
	   * unique name for the channel (as the server see's it). <cybah>
	   */
	  strlcpy(chan->name, chname, sizeof(chan->name));
	  chan->ircnet_status &= ~CHAN_JUPED;

          /* ... and log us joining. Using chan->dname for the channel is
	   * important in this case. As the config file will never contain
	   * logs with the unique name.
           */
	  if (chname[0] == '!')
            irc_log(chan, "Joined. (%s)", chname);
	  else
            irc_log(chan, "Joined.");
	  if (!match_my_nick(chname))
 	    reset_chan_info(chan);
	} else {
          irc_log(chan, "Join: %s (%s)", nick, uhost);
          detect_chan_flood(m, from, chan, FLOOD_JOIN);
          /* New bot available for roles, rebalance. */
          if (is_bot(m->user)) {
            chan->needs_role_rebalance = 1;
          }
	  set_handle_laston(chan->dname, m->user, now);
	}
      }

      /* ok, the op-on-join,etc, tests...first only both if Im opped */
      if (me_op(chan)) {
        struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };

        get_user_flagrec(m->user, &fr, chan->dname, chan);

        bool is_op = chk_op(fr, chan);

        /* Check for a mass join */
        if (chan->role & ROLE_FLOOD &&
            !splitjoin && chan->flood_mjoin_time && chan->flood_mjoin_thr && !is_op) {
          if (chan->channel.drone_jointime < now - chan->flood_mjoin_time) {      //expired, reset counter
            chan->channel.drone_joins = 0;
          }
          ++chan->channel.drone_joins;
          chan->channel.drone_jointime = now;

          if (!chan->channel.drone_set_mode && chan->channel.drone_joins >= chan->flood_mjoin_thr) {  //flood from dronenet, let's attempt to set +im
            lockdown_chan(chan, FLOOD_MASSJOIN);
          }
        }


	/* Check for and reset exempts and invites.
	 *
	 * This will require further checking to account for when to use the
	 * various modes.
	 */
	if (u_match_mask(global_invites,from) ||
	    u_match_mask(chan->invites, from))
	  refresh_invite(chan, from);

	if (chan->role & ROLE_BAN &&
            !(use_exempts && (u_match_mask(global_exempts,from) || u_match_mask(chan->exempts, from)))) {
          if (channel_enforcebans(chan) && !chan_sentkick(m) && !is_op &&
              !(use_exempts && (isexempted(chan, from) || (chan->ircnet_status & CHAN_ASKED_EXEMPTS)))) {
            for (masklist* b = chan->channel.ban; b->mask[0]; b = b->next) {
              if (wild_match(b->mask, from) || match_cidr(b->mask, from)) {
                dprintf(DP_SERVER, "KICK %s %s :%s%s\n", chname, m->nick, bankickprefix, r_banned(chan));
                m->flags |= SENTKICK;
                return 0;
              }
            }
          }
	  /* If it matches a ban, dispose of them. */
	  if (u_match_mask(global_bans, from) || u_match_mask(chan->bans, from)) {
	    refresh_ban_kick(chan, m, from);
	  /* Likewise for kick'ees */
	  } else if (!chan_sentkick(m) && (glob_kick(fr) || chan_kick(fr))) {
	    check_exemptlist(chan, from);
	    quickban(chan, from);
            dprintf(DP_MODE, "KICK %s %s :%s%s\n", chname, m->nick, bankickprefix, response(RES_KICKBAN));
	    m->flags |= SENTKICK;
	  }
	}
        bool op = 0;
#ifdef CACHE
        cache_t *cache = cache_find(nick);

        if (cache) {
          cache_chan_t *cchan = NULL;

          if (strcasecmp(cache->uhost, m->userhost)) {


          }

          for (cchan = cache->cchan; cchan && cchan->dname[0]; cchan = cchan->next) {
            if (!rfc_casecmp(cchan->dname, chan->dname)) {
              if (cchan->op) {
                op = 1;
                cchan->op = 0;
              }
              break;
            }
          }
        }
#endif /* CACHE */
        if (!splitjoin) {
          /* Autoop */
          if (!chan_hasop(m) && 
               (op || 
               ((chan->role & ROLE_OP) && chk_autoop(m, fr, chan)))) {
            do_op(m, chan, 1, 0);
          }

          /* +v or +voice */
          if (!chan_hasvoice(m) && !glob_bot(fr) && dovoice(chan)) {
            if (m->user) {
              if (!(m->flags & EVOICE) &&
                  (
                   /* +voice: Voice all clients who are not flag:+q. If the chan is +voicebitch, only op flag:+v clients */
                   (channel_voice(chan) && !chk_devoice(fr) && (!channel_voicebitch(chan) || (channel_voicebitch(chan) && chk_voice(m, fr, chan)))) ||
                   /* Or, if the channel is -voice but they still qualify to be voiced */
                   (!channel_voice(chan) && !privchan(fr, chan, PRIV_VOICE) && chk_voice(m, fr, chan))
                  )
                 ) {
                m->delay = now + chan->auto_delay;
                m->flags |= SENTVOICE;
              }
            } else if (!m->user && channel_voice(chan) && !channel_voicebitch(chan) && voice_ok(m, chan)) {
              m->delay = now + chan->auto_delay;
              m->flags |= SENTVOICE;
            }
          }
        }
      }
    }
  }

  return 0;
}

/* Got a part
 */
static int gotpart(char *from, char *msg)
{
  char *nick = NULL, *chname = NULL;
  struct chanset_t *chan = NULL;
  char buf[UHOSTLEN] = "", *uhost = buf;

  chname = newsplit(&msg);
  fixcolon(chname);
  fixcolon(msg);
  chan = findchan(chname);

  strlcpy(uhost, from, sizeof(buf));
  nick = splitnick(&uhost);

  if (chan && !shouldjoin(chan) && match_my_nick(nick)) {
    irc_log(chan, "Parting");    
    clear_channel(chan, 1);
    return 0;
  }
  if (chan && !channel_pending(chan)) {
    memberlist* m = ismember(chan, nick);
    struct userrec *u = (m && m->user) ? m->user : get_user_by_host(from);

    if (!channel_active(chan)) {
      /* whoa! */
      putlog(LOG_ERRORS, "*", "confused bot: guess I'm on %s and didn't realize it", chan->dname);
      chan->ircnet_status |= CHAN_ACTIVE;
      chan->ircnet_status &= ~(CHAN_PEND | CHAN_JOINING);
      reset_chan_info(chan);
    }
    /* This bot fullfilled a role, need to rebalance. */
    if (u && u->bot && (*chan->bot_roles)[u->handle] != 0) {
      chan->needs_role_rebalance = 1;
    }
    set_handle_laston(chan->dname, u, now);

    if (m) {
      detect_chan_flood(m, from, chan, FLOOD_PART);
      killmember(chan, nick);
    }
    if (msg[0])
      irc_log(chan, "Part: %s (%s) [%s]", nick, uhost, msg);
    else
      irc_log(chan, "Part: %s (%s)", nick, uhost);
    /* If it was me, all hell breaks loose... */
    if (match_my_nick(nick)) {
      check_rejoin(chan);
    } else
      check_lonely_channel(chan);
  }
  return 0;
}

/* Got a kick
 */
static int gotkick(char *from, char *origmsg)
{
  char buf2[511], *msg = buf2, *chname = NULL;
  struct chanset_t *chan = NULL;

  strlcpy(buf2, origmsg, sizeof(buf2));
  msg = buf2;
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (!chan)
    return 0;

  char *nick = newsplit(&msg);

  if (match_my_nick(nick) && channel_pending(chan)) {
    check_rejoin(chan);
    return 0; /* rejoin if kicked before getting needed info <Wcc[08/08/02]> */
  }
  if (channel_active(chan)) {
    char *whodid = NULL, buf[UHOSTLEN] = "", *uhost = buf;
    memberlist *m = NULL, *mv = NULL;
    struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };

    fixcolon(msg);
    strlcpy(uhost, from, sizeof(buf));
    whodid = splitnick(&uhost);

    chan = findchan(chname);
    if (!chan)
      return 0;     

    m = ismember(chan, whodid);
    if (m) {
      detect_chan_flood(m, from, chan, FLOOD_KICK);

      m->last = now;
      member_getuser(m);
      if (m->user) {
        /* This _needs_ to use chan->dname <cybah> */
        get_user_flagrec(m->user, &fr, chan->dname, chan);
        set_handle_laston(chan->dname, m->user, now);
      }
    }

    if ((!m || !m->user) || (m && m->user && !m->user->bot)) {
      chan->channel.fighting++;
    }

    mv = ismember(chan, nick);

    member_getuser(mv);
    if (mv->user) {
      // Revenge kick clients that kick our bots
      if (chan->revenge && !mv->is_me && m && m != mv && mv->user->bot && !(m->user && m->user->bot)) {
        if ((chan->role & ROLE_REVENGE) && !chan_sentkick(m) && me_op(chan)) {
          m->flags |= SENTKICK;
          dprintf(DP_MODE_NEXT, "KICK %s %s :%s%s\r\n", chan->name, m->nick, kickprefix, response(RES_REVENGE));
        } else {
          if (m->user) {
            char tmp[128] = "";
            simple_snprintf(tmp, sizeof(tmp), "Kicked bot %s on %s", mv->nick, chan->dname);
            deflag_user(m->user, DEFLAG_EVENT_REVENGE_KICK, tmp, chan);
          }
        }
      }

      set_handle_laston(chan->dname, mv->user, now);
      /* This bot fullfilled a role, need to rebalance. */
      if (mv->user->bot && (*chan->bot_roles)[mv->user->handle] != 0) {
        chan->needs_role_rebalance = 1;
      }
    }
    irc_log(chan, "%s!%s was kicked by %s (%s)", mv->nick, mv->userhost, from, msg);
    /* Kicked ME?!? the sods! */
    if (mv->is_me) {
      check_rejoin(chan);
    } else {
      killmember(chan, nick);
      check_lonely_channel(chan);
    }
  }
  return 0;
}

/* Got a nick change
 */
static int gotnick(char *from, char *msg)
{
  char *nick = NULL, s1[UHOSTLEN] = "", buf[UHOSTLEN] = "", *uhost = buf;
  memberlist *m = NULL, *mm = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };

  strlcpy(uhost, from, sizeof(buf));
  nick = splitnick(&uhost);
  fixcolon(msg);
  irc_log(NULL, "[%s] Nick change: %s -> %s", samechans(nick, ","), nick, msg);
  clear_chanlist_member(nick);	/* Cache for nick 'nick' is meaningless now. */

  Auth *auth = Auth::Find(uhost);
  if (auth)
    auth->NewNick(msg);

  const RfcString rfc_nick(nick);
  auto new_rfc_nick = std::make_shared<RfcString>(msg);

  /* Compose a nick!user@host for the new nick */
  simple_snprintf(s1, sizeof(s1), "%s!%s", msg, uhost);

  /* Users can match by nick, so a recheck is needed */
  struct userrec *u = get_user_by_host(s1);

  for (struct chanset_t *chan = chanset; chan; chan = chan->next) {
    m = ismember(chan, rfc_nick);

    if (m) {
      m->user = u;
      m->last = now;
      /* Not just a capitalization change */
      if (rfc_casecmp(nick, msg)) {
        /* Someone on channel with old nick?! */
	if ((mm = ismember(chan, *new_rfc_nick)))
	  killmember(chan, mm->nick, false);
      }

      chan->channel.hashed_members->remove(*m->rfc_nick);
      strlcpy(m->nick, msg, sizeof(m->nick));
      m->rfc_nick = new_rfc_nick;
      (*chan->channel.hashed_members)[*m->rfc_nick] = m;
      strlcpy(m->from, s1, sizeof(m->from));

      /*
       * Banned?
       */

      memberlist_reposition(chan, m);

      detect_chan_flood(m, from, chan, FLOOD_NICK);

      /* Any pending kick or mode to the old nick is lost. */
      m->flags &= ~(SENTKICK | SENTDEOP | SENTOP |
          SENTVOICE | SENTDEVOICE);

      /* nick-ban or nick is +k or something? */
      get_user_flagrec(m->user, &fr, chan->dname, chan);
      check_this_member(chan, m, &fr);
    }
  }
  return 0;
}

void check_should_cycle(struct chanset_t *chan)
{
  if (!me_op(chan))
    return;

  //If there are other ops split off and i'm the only op on this side of split, cycle
  int localops = 0, localbotops = 0, splitops = 0, splitbotops = 0, localnonops = 0;

  for (memberlist *ml = chan->channel.member; ml && ml->nick[0]; ml = ml->next) {
    if (chan_hasop(ml)) {
      if (chan_issplit(ml)) {
        splitops++;
        if ((ml->user) && ml->user->bot)
          splitbotops++;
      } else {
        localops++;
        if ((ml->user) && ml->user->bot)
          localbotops++;
        if (localbotops >= 2)
          return;
        if (localops > localbotops)
          return;
      }
    } else {
      if (!chan_issplit(ml))
        localnonops++;
    }
  }
  if (splitbotops > 5) {
    sdprintf("Cycling %s", chan->dname);
    /* I'm only one opped here... and other side has some ops... so i'm cycling */
    if (localnonops) {
      /* need to unset any +kil first */
      dprintf(DP_MODE, "MODE %s -ilk %s\n", chan->name[0] ? chan->name : chan->dname, (chan->channel.key && chan->channel.key[0]) ? chan->channel.key : "");
      dprintf(DP_MODE, "PART %s\n", chan->name[0] ? chan->name : chan->dname);
    } else
      dprintf(DP_MODE, "PART %s\n", chan->name[0] ? chan->name : chan->dname);
  }
}


/* Signoff, similar to part.
 */
static int gotquit(char *from, char *msg)
{
  char *nick = NULL, *p = NULL;
  bool split = 0;
  memberlist *m = NULL;
  char from2[NICKMAX + UHOSTMAX + 1] = "";
  struct userrec *u = NULL;

  strlcpy(from2, from, sizeof(from2));
#ifdef TCL
  u = get_user_by_host(from2);
#endif
  nick = splitnick(&from);
  fixcolon(msg);
  /* Fred1: Instead of expensive wild_match on signoff, quicker method.
   *        Determine if signoff string matches "%.% %.%", and only one
   *        space.
   */
  p = strchr(msg, ' ');
  if (p && (p == strrchr(msg, ' '))) {
    char *z1 = NULL, *z2 = NULL;

    *p = 0;
    z1 = strchr(p + 1, '.');
    z2 = strchr(msg, '.');
    if (z1 && z2 && (*(z1 + 1) != 0) && (z1 - 1 != p) &&
	(z2 + 1 != p) && (z2 != msg)) {
      /* Server split, or else it looked like it anyway (no harm in
       * assuming)
       */
      split = 1;
    } else
      *p = ' ';
  }
  if (msg[0])
    irc_log(NULL, "[%s] Quits %s (%s) (%s)", samechans(nick, ","), nick, from, msg);
  else
    irc_log(NULL, "[%s] Quits %s (%s)", samechans(nick, ","), nick, from);

  for (struct chanset_t* chan = chanset; chan; chan = chan->next) {
    if (!channel_active(chan))
      continue;

    m = ismember(chan, nick);
    if (m) {
      member_getuser(m);
      u = m->user;
      if (u) {
        if (u->bot) {
          counter_clear(u->handle);
          /* This bot fullfilled a role, need to rebalance. */
          if ((*chan->bot_roles)[u->handle] != 0) {
            chan->needs_role_rebalance = 1;
          }
        }
        set_handle_laston(chan->dname, u, now); /* If you remove this, the bot will crash when the user record in question
						   is removed/modified during the tcl binds below, and the users was on more
						   than one monitored channel */
      }
      if (split) {
	m->split = now;
        ++(chan->channel.splitmembers);
        irc_log(chan, "%s (%s) got netsplit.", nick, from);
      } else {
	killmember(chan, nick);
	check_lonely_channel(chan);
      }
      if (channel_cycle(chan))
        check_should_cycle(chan);
    }
  }
  /* Our nick quit? if so, grab it.
   */
  if (keepnick && !match_my_nick(nick))
    nicks_available(nick);
  set_fish_key(nick, "");
#ifdef CACHE
  /* see if they were in our cache at all */
  cache_t *cache = cache_find(nick);

  if (cache) 
    cache_del(nick, cache);
#endif /* CACHE */

  return 0;
}

/* Got a channel message.
 */
static int gotmsg(char *from, char *msg)
{
  if (!strchr("&#!+@$", msg[0]))
    return 0;

  bool ignoring = match_ignore(from);
  char *to = newsplit(&msg), *realto = (to[0] == '@') ? to + 1 : to;
  struct chanset_t *chan = findchan(realto);

  if (!chan)
    return 0;			/* Private msg to an unknown channel?? */


  char buf[UHOSTLEN] = "", *nick = NULL, buf2[512] = "", *uhost = buf, *p = NULL, *p1 = NULL, *code = NULL, *ctcp = NULL;
  int ctcp_count = 0;
  struct userrec *u = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };

  fixcolon(msg);
  strlcpy(uhost, from, sizeof(buf));
  nick = splitnick(&uhost);

  memberlist *m = ismember(chan, nick);

  /* Only check if flood-ctcp is active */
  if (m && flood_ctcp.count && detect_avalanche(msg)) {
    member_getuser(m);
    u = m->user;
    get_user_flagrec(u, &fr, chan->dname, chan);
    /* Discard -- kick user if it was to the channel */
    if (m && me_op(chan) && 
	!chan_sentkick(m) &&
	!(use_exempts && ban_fun &&
	  /* don't kickban if permanent exempted -- Eule */
	  (u_match_mask(global_exempts, from) ||
	   u_match_mask(chan->exempts, from)))) {
      if (ban_fun) {
	check_exemptlist(chan, from);
	u_addmask('b', chan, quickban(chan, from), conf.bot->nick,
               "that was fun, let's do it again!", now + (60 * chan->ban_time), 0);
      }
      if (kick_fun) {
	/* This can induce kickflood - arthur2 */
	dprintf(DP_SERVER, "KICK %s %s :%sthat was fun, let's do it again!\n", chan->name, nick, kickprefix);
	m->flags |= SENTKICK;
      }
    }
    if (!ignoring) {
      putlog(LOG_MODES, "*", "Avalanche from %s!%s in %s - ignoring",
	     nick, uhost, chan->dname);
      p = strchr(uhost, '@');
      if (p)
	p++;
      else
	p = uhost;
      simple_snprintf(buf2, sizeof(buf2), "*!*@%s", p);
      addignore(buf2, conf.bot->nick, "ctcp avalanche", now + (60 * ignore_time));
    }
    return 0;
  }
  /* Check for CTCP: */
  ctcp_reply[0] = 0;
  p = strchr(msg, 1);
  while (p && *p) {
    p++;
    p1 = p;
    while ((*p != 1) && *p)
      p++;
    if (*p == 1) {
      *p = 0;
      ctcp = buf2;
      strlcpy(ctcp, p1, sizeof(buf2));
      strcpy(p1 - 1, p + 1);
      if (m) {
        detect_chan_flood(m, from, chan, strncmp(ctcp, "ACTION ", 7) ? FLOOD_CTCP : FLOOD_PRIVMSG);
        detect_chan_flood(m, from, chan, FLOOD_BYTES, msg);
      }

      /* Respond to the first answer_ctcp */
      p = strchr(msg, 1);
      if (ctcp_count < answer_ctcp) {
	ctcp_count++;
	if (ctcp[0] != ' ') {
	  code = newsplit(&ctcp);
	  u = get_user_by_host(from);
	  if (!ignoring || trigger_on_ignore) {
	    if (check_bind_ctcp(nick, uhost, u, to, code, ctcp) == BIND_RET_LOG) {

	      update_idle(chan->dname, nick);
            }
	      /* Log DCC, it's to a channel damnit! */
	      if (!strcmp(code, "ACTION")) {
                irc_log(chan, "* %s %s", nick, ctcp);
	      } else {
                irc_log(chan, "CTCP %s: from %s (%s) to %s: %s", code, nick, from, to, ctcp);
	      }
	  }
	}
      }
    }
  }
  /* Send out possible ctcp responses */
  if (ctcp_reply[0]) {
    if (ctcp_mode != 2) {
      notice(nick, ctcp_reply, DP_HELP);
    } else {
      if (now - last_ctcp > flood_ctcp.time) {
        notice(nick, ctcp_reply, DP_HELP);
	count_ctcp = 1;
      } else if (count_ctcp < flood_ctcp.count) {
        notice(nick, ctcp_reply, DP_HELP);
	count_ctcp++;
      }
      last_ctcp = now;
    }
  }
  if (msg[0]) {
    int botmatch = 0;
    char *my_msg = NULL, *my_ptr = NULL, *fword = NULL;

    if (me_op(chan) && doflood(chan))
      detect_offense(m, chan, msg);

    if (m) {
      detect_chan_flood(m, from, chan, FLOOD_PRIVMSG);
      detect_chan_flood(m, from, chan, FLOOD_BYTES, msg);
    }
    
    if (auth_chan) {
      my_msg = my_ptr = strdup(msg);
      fword = newsplit(&my_msg);		/* take out first word */
      /* the first word is a wildcard match to our nick. */
      botmatch = wild_match(fword, botname);
      if (botmatch && strcmp(fword, "*"))	
        fword = newsplit(&my_msg);	/* now this will be the command */
      /* is it a cmd? */
      if (auth_prefix[0] && fword && fword[0] && fword[1] && ((botmatch && fword[0] != auth_prefix[0]) || (fword[0] == auth_prefix[0]))) {
        Auth *auth = Auth::Find(uhost);
        if (auth && auth->Authed()) {
          if (fword[0] == auth_prefix[0])
            fword++;
          auth->atime = now;
          check_bind_authc(fword, auth, chan->dname, my_msg);
        }
      }
      free(my_ptr);
    }
    irc_log(chan, "%s<%s> %s", to[0] == '@' ? "@" : "", nick, msg);
    update_idle(chan->dname, nick);
  }
  return 0;
}

/* Got a private notice.
 */
static int gotnotice(char *from, char *msg)
{
  if (!strchr(CHANMETA "@", *msg))
    return 0;
  
  bool ignoring = match_ignore(from);
  char *to = newsplit(&msg), *realto = (*to == '@') ? to + 1 : to;
  struct chanset_t *chan = NULL;

  chan = findchan(realto);
  if (!chan)
    return 0;			/* Notice to an unknown channel?? */

  char *nick = NULL, buf2[512] = "", *p = NULL, *p1 = NULL, buf[512] = "", *uhost = buf;
  char *ctcp = NULL, *code = NULL;
  struct userrec *u = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };

  fixcolon(msg);
  strlcpy(uhost, from, sizeof(buf));
  nick = splitnick(&uhost);
  memberlist *m = ismember(chan, nick);
  if (m) {
    member_getuser(m);
    u = m->user;
  }

  if (flood_ctcp.count && detect_avalanche(msg)) {

    get_user_flagrec(u, &fr, chan->dname, chan);
    /* Discard -- kick user if it was to the channel */
    if (me_op(chan) && m && !chan_sentkick(m) &&
	!(use_exempts && ban_fun &&
	  /* don't kickban if permanent exempted -- Eule */
	  (u_match_mask(global_exempts,from) ||
	   u_match_mask(chan->exempts, from)))) {
      if (ban_fun) {
	check_exemptlist(chan, from);
	u_addmask('b', chan, quickban(chan, from), conf.bot->nick,
               "that was fun, let's do it again!", now + (60 * chan->ban_time), 0);
      }
      if (kick_fun) {
	/* This can induce kickflood - arthur2 */
	dprintf(DP_SERVER, "KICK %s %s :%sthat was fun, let's do it again!\n", chan->name, nick, kickprefix);
	m->flags |= SENTKICK;
      }
    }
    if (!ignoring)
      putlog(LOG_MODES, "*", "Avalanche from %s", from);
    return 0;
  }
  /* Check for CTCP: */
  p = strchr(msg, 1);
  while (p && *p) {
    p++;
    p1 = p;
    while ((*p != 1) && *p)
      p++;
    if (*p == 1) {
      *p = 0;
      ctcp = buf2;
      strlcpy(ctcp, p1, sizeof(buf2));
      strcpy(p1 - 1, p + 1);
      p = strchr(msg, 1);
      if (m) {
        detect_chan_flood(m, from, chan, strncmp(ctcp, "ACTION ", 7) ? FLOOD_CTCP : FLOOD_PRIVMSG);
        detect_chan_flood(m, from, chan, FLOOD_BYTES, msg);
      }

      if (ctcp[0] != ' ') {
	code = newsplit(&ctcp);
	if (!ignoring || trigger_on_ignore) {
	  check_bind_ctcr(nick, uhost, u, chan->dname, code, msg);

	  chan = findchan(realto); 
	  if (!chan)
	    return 0;
          irc_log(chan, "CTCP reply %s: %s from %s (%s) to %s", code, msg, nick, from, chan->dname);
          update_idle(chan->dname, nick);
	}
      }
    }
  }
  if (msg[0]) {
    if (m) {
      detect_chan_flood(m, from, chan, FLOOD_NOTICE);
      detect_chan_flood(m, from, chan, FLOOD_BYTES, msg);
    }

    update_idle(chan->dname, nick);

    if (!ignoring)
      irc_log(chan, "-%s:%s- %s", nick, to, msg);
  }
  return 0;
}

static cmd_t irc_raw[] =
{
  {"001",       "",     (Function) got001,      "irc:001", LEAF},
#ifdef CACHE
  {"302",       "",     (Function) got302,      "irc:302", LEAF},
  {"341",       "",     (Function) got341,      "irc:341", LEAF},
#endif /* CACHE */
  {"324",	"",	(Function) got324,	"irc:324", LEAF},
  {"352",	"",	(Function) got352,	"irc:352", LEAF},
  {"354",	"",	(Function) got354,	"irc:354", LEAF},
  {"315",	"",	(Function) got315,	"irc:315", LEAF},
  {"367",	"",	(Function) got367,	"irc:367", LEAF},
  {"368",	"",	(Function) got368,	"irc:368", LEAF},
  {"403",	"",	(Function) got403,	"irc:403", LEAF},
  {"405",	"",	(Function) got405,	"irc:405", LEAF},
  {"471",	"",	(Function) got471,	"irc:471", LEAF},
  {"473",	"",	(Function) got473,	"irc:473", LEAF},
  {"474",	"",	(Function) got474,	"irc:474", LEAF},
  {"475",	"",	(Function) got475,	"irc:475", LEAF},
  {"478",	"",	(Function) got478,	"irc:478", LEAF},
  {"INVITE",	"",	(Function) gotinvite,	"irc:invite", LEAF},
  {"TOPIC",	"",	(Function) gottopic,	"irc:topic", LEAF},
  {"331",	"",	(Function) got331,	"irc:331", LEAF},
  {"332",	"",	(Function) got332,	"irc:332", LEAF},
  {"JOIN",	"",	(Function) gotjoin,	"irc:join", LEAF},
  {"PART",	"",	(Function) gotpart,	"irc:part", LEAF},
  {"KICK",	"",	(Function) gotkick,	"irc:kick", LEAF},
  {"NICK",	"",	(Function) gotnick,	"irc:nick", LEAF},
  {"QUIT",	"",	(Function) gotquit,	"irc:quit", LEAF},
  {"PRIVMSG",	"",	(Function) gotmsg,	"irc:msg", LEAF},
  {"NOTICE",	"",	(Function) gotnotice,	"irc:notice", LEAF},
  {"MODE",	"",	(Function) gotmode,	"irc:mode", LEAF},
  {"346",	"",	(Function) got346,	"irc:346", LEAF},
  {"347",	"",	(Function) got347,	"irc:347", LEAF},
  {"348",	"",	(Function) got348,	"irc:348", LEAF},
  {"349",	"",	(Function) got349,	"irc:349", LEAF},
  {"353",	"",	(Function) got353,	"irc:353", LEAF},
  {"710",	"",	(Function) got710,	"irc:710", LEAF},
  {NULL,	NULL,	NULL,			NULL, 0}
};
/* vim: set sts=2 sw=2 ts=8 et: */
