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
 * irc.c -- part of irc.mod
 *   support for channels within the bot
 *
 */


#include "src/common.h"
#define MAKING_IRC
#include "irc.h"
#include "src/adns.h"
#include "src/match.h"
#include "src/settings.h"
#include "src/base64.h"
#include "src/tandem.h"
#include "src/net.h"
#include "src/botnet.h"
#include "src/botmsg.h"
#include "src/main.h"
#include "src/response.h"
#include "src/set.h"
#include "src/userrec.h"
#include "src/misc.h"
#include "src/rfc1459.h"
#include "src/socket.h"
#include "src/adns.h"
#include "src/chanprog.h"
#include "src/auth.h"
#include "src/userrec.h"
#include "src/binds.h"
#include "src/userent.h"
#include "src/egg_timer.h"
#include "src/mod/share.mod/share.h"
#include "src/mod/server.mod/server.h"
#include "src/mod/channels.mod/channels.h"
#include "src/mod/ctcp.mod/ctcp.h"
#include <algorithm>
using std::swap;
#include <bdlib/src/String.h>
#include <bdlib/src/HashTable.h>
#include <bdlib/src/base64.h>
#include <deque>

#include <stdarg.h>

#include <math.h>

#define PRIO_DEOP 1
#define PRIO_KICK 2

#ifdef CACHE
static cache_t *irccache = NULL;
#endif /* CACHE */

#define do_eI (((now - chan->channel.last_eI) > 30) ? 1 : 0)

int max_bans;                   /* Modified by net-type 1-4 */
int max_exempts;
int max_invites;
int max_modes;                  /* Modified by net-type 1-4 */
static bool bounce_bans = 0;
static bool bounce_exempts = 0;
static bool bounce_invites = 0;
static bool bounce_modes = 0;
unsigned int modesperline;      /* Number of modes per line to send. */
static size_t mode_buf_len = 200;  /* Maximum bytes to send in 1 mode. */
bool use_354 = 0;                /* Use ircu's short 354 /who
                                 * responses. */
static bool kick_fun = 0;
static bool ban_fun = 1;
static bool prevent_mixing = 1;  /* To prevent mixing old/new modes */
bool include_lk = 1;      /* For correct calculation
                                 * in real_add_mode. */
static bd::HashTable<bd::String, unsigned long> bot_counters;
unsigned long my_cookie_counter = 0;

static std::deque<bd::String> chained_who;
static int chained_who_idx;

static int
voice_ok(memberlist *m, struct chanset_t *chan)
{
  if (m->flags & EVOICE)
    return 0;

  if (m->userhost[0] && !chan->voice_non_ident && m->userhost[0] == '~')
    return 0;

  return 1;
}

#include "chan.cc"
#include "mode.cc"
#include "cmdsirc.cc"
#include "msgcmds.cc"

static int
detect_offense(memberlist* m, struct chanset_t *chan, char *msg)
{
  if (!chan || !msg
      || !(chan->capslimit || chan->colorlimit)
      || chan_sentkick(m)) //sanity check
    return 0;

  member_getuser(m, 0);

  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  get_user_flagrec(m->user, &fr, chan->dname, chan);

  if (glob_bot(fr) ||
      chk_noflood(fr) ||
      (m && chan->flood_exempt_mode == CHAN_FLAG_OP && chan_hasop(m)) ||
      (m && chan->flood_exempt_mode == CHAN_FLAG_VOICE && (chan_hasvoice(m) || chan_hasop(m))))
    return 0;

  int color_count = 0, hit_check = 0, hit_count = 0;
  double caps_percentage = 0, caps_count = 0;
  const double caps_limit = chan->capslimit ? double(chan->capslimit) / 100.0 : double(0);

  // Need to know how long the message is, and want to ignore spaces, so avoid a strlen() and just loop to count
  size_t tot = 0;
  if (caps_limit) {
    char *msg_check = msg;
    while (msg_check && *msg_check) {
      if (!egg_isspace(*msg_check) && *msg_check != 3 && *msg_check != 2) {
        ++tot;
      }
      ++msg_check;
    }

    if (tot >= 30) {
      hit_check = tot/5; //check in-between for hits to save waste of cpu
    } else if (!tot && !chan->colorlimit) {
      // Nothing to do, bail out
      return 0;
    }
  }

  while (msg && *msg) {
    // Skip spaces
    if (egg_isspace(*msg)) {
      ++msg;
      continue;
    }

    if (egg_isupper(*msg)) {
      ++caps_count;
    } else if (*msg == 3 || *msg == 2) {
      ++color_count;
    }

    if (hit_check && !(hit_count % hit_check)) {
      if (caps_limit) {
        caps_percentage = (caps_count)/(double(tot));
        if (caps_percentage >= caps_limit) {
          break;
        }
      }
      if (chan->colorlimit && color_count >= chan->colorlimit) {
        break;
      }
      ++hit_count;
    }
    ++msg;
  }
  if (chan->capslimit && caps_count && tot >= 6) {
    caps_percentage = (caps_count)/(double(tot));
    if (caps_percentage >= caps_limit) {
      const char *response = punish_flooder(chan, m);
      putlog(LOG_MODES, chan->name, "Caps flood (%d%%) from %s -- %s", int(caps_percentage * 100), m->nick, response);
      return 1;
    }
  } else if (chan->colorlimit && color_count) {
    if (color_count >= chan->colorlimit) {
      const char *response = punish_flooder(chan, m);
      putlog(LOG_MODES, chan->name, "Color flood (%d) from %s -- %s", color_count, m->nick, response);
      return 1;
    }
  }
  return 0;
}

void set_devoice(struct chanset_t *chan, memberlist* m) {
  if (!(m->flags & EVOICE)) {
    putlog(LOG_DEBUG, "@", "Giving EVOICE flag to: %s (%s)", m->nick, chan->dname);
    m->flags |= EVOICE;
  }
}

const char* punish_flooder(struct chanset_t* chan, memberlist* m, const char *reason) {
  if (channel_voice(chan) && chan->voice_moderate) {
    if (!chan_sentdevoice(m)) {
      add_mode(chan, '-', 'v', m);
      m->flags |= SENTDEVOICE;
      set_devoice(chan, m);
      return "devoicing";
    } else {
      return "devoiced";
    }
  } else {
    if (!chan_sentkick(m)) {
      dprintf(DP_SERVER, "KICK %s %s :%s%s\n", chan->name, m->nick, kickprefix, reason ? reason : response(RES_FLOOD));
      m->flags |= SENTKICK;
      return "kicking";
    } else {
      return "kicked";
    }
  }
  return "ignoring";
}

void unlock_chan(struct chanset_t *chan)
{
  if (chan->channel.drone_set_mode) {
    char buf[3] = "", *p = buf;
    if ((chan->channel.drone_set_mode & CHANINV) && !(chan->mode_pls_prot & CHANINV))
      *p++ = 'i';
    if ((chan->channel.drone_set_mode & CHANMODER) && !((chan->mode_pls_prot & CHANMODER) || (channel_voice(chan) && chan->voice_moderate)))
      *p++ = 'm';
    *p = 0;
    dprintf(DP_MODE, "MODE %s :-%s\n", chan->name[0] ? chan->name : chan->dname, buf);
  }
  chan->channel.drone_set_mode = 0;
}

void lockdown_chan(struct chanset_t* chan, flood_reason_t reason, const char* flood_type) {
  if (!me_op(chan)) {
    return;
  }
  egg_timeval_t howlong;

  chan->channel.drone_set_mode = 0;

  char buf[3] = "", *p = buf;

  if (!(chan->channel.mode & CHANINV) && !(chan->mode_mns_prot & CHANINV)) {
    chan->channel.drone_set_mode |= CHANINV;
    *p++ = 'i';
  }
  if (!(chan->channel.mode & CHANMODER) && !(chan->mode_mns_prot & CHANMODER)) {
    chan->channel.drone_set_mode |= CHANMODER;
    *p++ = 'm';
  }
  *p = 0;

  if (chan->channel.drone_set_mode && buf[0]) {
    chan->channel.drone_joins = 0;
    chan->channel.drone_jointime = 0;

    dprintf(DP_MODE_NEXT, "MODE %s +%s\n", chan->name[0] ? chan->name : chan->dname, buf);
    howlong.sec = chan->flood_lock_time;
    howlong.usec = 0;
    timer_create_complex(&howlong, "unlock", (Function) unlock_chan, (void *) chan, 0);

    if (reason == FLOOD_MASSJOIN) {
      putlog(LOG_MISC, "*", "Join flood detected in %s! Locking for %d seconds.", chan->dname, chan->flood_lock_time);
    } else if (reason == FLOOD_BANLIST) {
      putlog(LOG_MISC, "*", "Banlist full in %s! Locking for %d seconds.", chan->dname, chan->flood_lock_time);
    } else if (reason == FLOOD_MASS_FLOOD) {
      putlog(LOG_MISC, "*", "Mass flood (%s) detected in %s! Locking for %d seconds.", flood_type, chan->dname, chan->flood_lock_time);
    }
  }
}


void notice_invite(struct chanset_t *chan, char *handle, char *nick, char *uhost, bool op) {
  char fhandle[21] = "";
  const char *ops = " (auto-op)";

  if (handle)
    simple_snprintf(fhandle, sizeof(fhandle), "\002%s\002 ", handle);
  putlog(LOG_MISC, "*", "Invited %s%s(%s%s%s) to %s.", handle ? handle : "", handle ? " " : "", nick, uhost ? "!" : "", uhost ? uhost : "", chan->dname);
  bd::String msg;
  msg = bd::String::printf("\001ACTION has invited %s(%s%s%s) to %s.%s\001",
    fhandle, nick, uhost ? "!" : "", uhost ? uhost : "", chan->dname, op ? ops : "");
  privmsg(chan->name, msg, DP_MODE);
}

#ifdef CACHE
static cache_t *cache_new(char *nick)
{
  cache_t *cache = (cache_t *) calloc(1, sizeof(cache_t));

  cache->next = NULL;
  strlcpy(cache->nick, nick, sizeof(cache->nick));
  cache->uhost[0] = 0;
  cache->handle[0] = 0;
//  cache->user = NULL;
  cache->timeval = now;
  cache->cchan = NULL;
  list_append((struct list_type **) &irccache, (struct list_type *) cache);

  return cache;
}

static cache_chan_t *cache_chan_add(cache_t *cache, char *chname)
{
  cache_chan_t *cchan = (cache_chan_t *) calloc(1, sizeof(cache_chan_t));
  
  cchan->next = NULL;
  strlcpy(cchan->dname, chname, sizeof(cchan->dname));
  cchan->ban = 0;
  cchan->invite = 0;
  cchan->invited = 0;

  list_append((struct list_type **) &(cache->cchan), (struct list_type *) cchan);

  return cchan;
}

static void cache_chan_find(cache_t *cache, cache_chan_t *cchan, char *nick, char *chname)
{
  if (!cache)
    cache = cache_find(nick);

  if (cache) {
    for (cchan = cache->cchan; cchan && cchan->dname[0]; cchan = cchan->next) {
      if (!rfc_casecmp(cchan->dname, chname)) {
        return;
      }
    }
  }

  return;
}


static void cache_chan_del(char *nick, char *chname) {
  cache_t *cache = NULL;
  cache_chan_t *cchan = NULL;

  cache_chan_find(cache, cchan, nick, chname);

  if (cchan) {
    list_delete((struct list_type **) &cache->cchan, (struct list_type *) cchan);
    free(cache);
  }
}
static cache_t *cache_find(char *nick)
{
  cache_t *cache = NULL;

  for (cache = irccache; cache && cache->nick[0]; cache = cache->next)
    if (!rfc_casecmp(cache->nick, nick))
      break;

  return cache;
}

static void cache_del(char *nick, cache_t *cache)
{
  if (!cache)
    cache = cache_find(nick);
 
  if (cache) {
    list_delete((struct list_type **) &irccache, (struct list_type *) cache);
    free(cache);
  }
}

static void cache_debug(void)
{
  cache_t *cache = NULL;
  cache_chan_t *cchan = NULL;

  for (cache = irccache; cache && cache->nick[0]; cache = cache->next) {
    dprintf(DP_MODE, "PRIVMSG #wraith :%s!%s (%s)\n", cache->nick, cache->uhost, cache->handle);
    for (cchan = cache->cchan; cchan && cchan->dname[0]; cchan = cchan->next)
      dprintf(DP_MODE, "PRIVMSG #wraith :%s %d %d %d\n", cchan->dname, cchan->ban, cchan->invite, cchan->invited);
  }
}
#endif /* CACHE */

static void cache_invite(struct chanset_t *chan, char *nick, char *host, char *handle, bool op, bool bot)
{
#ifdef CACHE
  cache_t *cache = NULL;

  if ((cache = cache_find(nick)) == NULL)
    cache = cache_new(nick);

  cache->bot = bot;

  /* if we find they have a host but it doesnt match the new host, wipe it */
  if (host && cache->uhost[0] && strcasecmp(cache->uhost, host))
    cache->uhost[0] = 0;

  if (host && !cache->uhost[0])
    strlcpy(cache->uhost, host, sizeof(cache->uhost));

  /* if we find they have a handle but it doesnt match the new handle, wipe it */
  if (handle && cache->handle[0] && strcasecmp(cache->handle, handle))
    cache->handle[0] = 0;

  if (handle && !cache->handle[0])
    strlcpy(cache->handle, handle, sizeof(cache->handle));

  cache_chan_t *cchan = cache_chan_add(cache, chan->dname);

  cchan->op = op;
    
  if (!host) {
    cchan->invite = 1;
    dprintf(DP_MODE, "USERHOST %s\n", nick);
    return;
  }

  /* if we have a uhost already, it's safe to invite them */
  cchan->invited = 1;
  cchan->invite = 0;
#endif /* CACHE */
  dprintf(DP_SERVER, "INVITE %s %s\n", nick, chan->name);
}

static inline const char * cookie_hash(const char* chname, const memberlist* opper, const memberlist* opped, const char* ts, const char* randstring, const char* key) {
  char tohash[201] = "";
  const char salt2[] = SALT2;

  simple_snprintf(tohash, sizeof(tohash), STR("%c%s%c%c%c\n%c%c%s%s%s"),
                                     salt2[0],
                                     ts,
                                     randstring[0], randstring[1], randstring[2], randstring[3],
                                     salt2[15],
                                     opped->nick,
                                     opped->userhost,
                                     key);
#ifdef DEBUG
sdprintf("chname: %s ts: %s randstring: %c%c%c%c", chname, ts, randstring[0], randstring[1], randstring[2], randstring[3]);
sdprintf("tohash: %s", tohash);
#endif
  const char* md5 = MD5(tohash);
  OPENSSL_cleanse(tohash, sizeof(tohash));
  return md5;
}

static inline void cookie_key(char *key, size_t key_len, const char* randstring, const memberlist *opper, const char *chname) {
  const char salt1[] = SALT1;
  const char salt2[] = SALT2;
  simple_snprintf2(key, key_len, STR("%c%c%c%^s%c%c%c%c%c%c%^s%c%c%c%c%c%c%c%s"),
                                        randstring[0],
                                        salt1[5],
                                        randstring[3],
                                        opper->user->handle,
                                        randstring[2],
                                        salt1[4],
                                        salt1[0],
                                        salt1[1],
                                        salt1[3],
                                        salt1[6],
                                        chname,
                                        salt1[10],
                                        randstring[1],
                                        salt2[15],
                                        salt2[13],
                                        salt1[10],
                                        salt2[3],
                                        salt2[1],
                                        opper->userhost
                                        );
}

#define HASH_INDEX1(_x) (8 + (_x))
#define HASH_INDEX2(_x) (16 + (_x))
#define HASH_INDEX3(_x) (18 + (_x))

void makecookie(char *out, size_t len, const char *chname, const memberlist* opper, const memberlist* m1, const memberlist* m2, const memberlist* m3) {
  char randstring[5] = "", ts[11] = "";

  make_rand_str(randstring, 4);
  /* &ts[4] is now last 6 digits of time */
  simple_snprintf(ts, sizeof(ts), "%010li", (long) (now + timesync));

  char cookie_clear[101] = "";

  //Increase my counter
  ++my_cookie_counter;
  if (my_cookie_counter > (unsigned long)(-500))
    my_cookie_counter = 0;
  simple_snprintf2(cookie_clear, sizeof(cookie_clear), STR("%s%s%D"), randstring, &ts[3], my_cookie_counter);

  char key[150] = "";
  cookie_key(key, sizeof(key), randstring, opper, chname);

  const char* hash1 = cookie_hash(chname, opper, m1, &ts[4], randstring, key);
  const char* hash2 = m2 ? cookie_hash(chname, opper, m2, &ts[4], randstring, key) : NULL;
  const char* hash3 = m3 ? cookie_hash(chname, opper, m3, &ts[4], randstring, key) : NULL;
  bd::String cookie = encrypt_string(MD5(key), bd::String(cookie_clear));
  cookie = bd::base64Encode(cookie);
#ifdef DEBUG
sdprintf("key: %s", key);
sdprintf("cookie_clear: %s", cookie_clear);
sdprintf("hash1: %s", hash1);
if (hash2) sdprintf("hash2: %s", hash2);
if (hash3) sdprintf("hash3: %s", hash3);
#endif

  if (m3)
    simple_snprintf(out, len + 1, STR("%c%c%c%c%c%c%c%c%c!%s@%s"), 
                         hash1[HASH_INDEX1(0)], 
                         hash1[HASH_INDEX2(0)], 
                         hash1[HASH_INDEX3(0)], 
                         hash2[HASH_INDEX1(1)], 
                         hash2[HASH_INDEX2(1)], 
                         hash2[HASH_INDEX3(1)], 
                         hash3[HASH_INDEX1(2)], 
                         hash3[HASH_INDEX2(2)], 
                         hash3[HASH_INDEX3(2)], 
                         randstring, 
                         cookie.c_str());
  else if (m2)
    simple_snprintf(out, len + 1, STR("%c%c%c%c%c%c!%s@%s"), 
                         hash1[HASH_INDEX1(0)], 
                         hash1[HASH_INDEX2(0)], 
                         hash1[HASH_INDEX3(0)], 
                         hash2[HASH_INDEX1(1)], 
                         hash2[HASH_INDEX2(1)], 
                         hash2[HASH_INDEX3(1)], 
                         randstring, 
                         cookie.c_str());
  else
    simple_snprintf(out, len + 1, STR("%c%c%c!%s@%s"), 
                         hash1[HASH_INDEX1(0)], 
                         hash1[HASH_INDEX2(0)], 
                         hash1[HASH_INDEX3(0)], 
                         randstring, 
                         cookie.c_str());
#ifdef DEBUG
sdprintf("cookie: %s", out);
#endif
}

// Clear counter for bot
void counter_clear(const char* botnick) {
  bot_counters[botnick] = 0;
}

static inline int checkcookie(const char *chname, const memberlist* opper, const memberlist* opped, const char *cookie, int indexHint) {
#define HOST(_x) (6 + (_x) + ((hashes << 1) + hashes)) /* x + (hashes * 3) */
#define SALT(_x) (1 + (_x) + ((hashes << 1) + hashes)) /* x + (hashes * 3) */
  /* How many hashes are in the cookie? */
  const size_t hashes = cookie[3] == '!' ? 1 : (cookie[6] == '!' ? 2 : 3);

  char key[150] = "";
  const char *randstring = &cookie[SALT(0)];

  cookie_key(key, sizeof(key), randstring, opper, chname);

  bd::String ciphertext = bd::base64Decode((char*) &cookie[HOST(0)]);
  bd::String cleartext = decrypt_string(MD5(key), ciphertext);
  char ts[8] = "";
  strlcpy(ts, cleartext.c_str() + 4, sizeof(ts));
  unsigned long counter = base64_to_int(cleartext.c_str() + 4 + 7);

  //Lookup counter for the opper
  unsigned long last_counter = 0;
  // Don't check counter for my own ops as it may have incremented in the queue before seeing the last last counter
  bd::String handle;
  if (conf.bot->u != opper->user) {
    handle = opper->user->handle;
    if (bot_counters.contains(handle))
      last_counter = bot_counters[handle];
    else
      last_counter = 0;
  }

#ifdef DEBUG
sdprintf("key: %s", key);
sdprintf("plaintext from cookie: %s", cleartext.c_str());
sdprintf("ts from cookie: %s", ts);
if (indexHint == 0) {
  sdprintf("last counter from %s: %lu", opper->user->handle, last_counter);
  sdprintf("counter from cookie: %lu", counter);
}
#endif

  const time_t optime = atol(ts);
  if ((((now + timesync) % 10000000) - optime) > 3900)
    return BC_SLACK;

  //Only check on the first cookie
  if (indexHint == 0 && conf.bot->u != opper->user) {
    if (counter <= last_counter)
      return BC_COUNTER;

    // graceful overflow
    if (counter > (unsigned long)(-1000))
      counter = 0;

    //Update counter for the opper
    bot_counters[handle] = counter;
  }

  const char *hash = cookie_hash(chname, opper, opped, &ts[1], randstring, key);
#ifdef DEBUG
sdprintf("hash: %s", hash);
#endif

  /* Compare the expected hash to each of the given hashes */
  /* indexHint, Which position of the +ooo are we? (1) could be either index (1) or (2).. but not (0). */

  /* See if any of the cookies match the hash we want */
  for (size_t i = indexHint; i < hashes; ++i) {
    const char *cookie_index = cookie + ((i << 1) + i); /* i * 3 */

    if ((hash[HASH_INDEX1(i)] == cookie_index[0] && 
         hash[HASH_INDEX2(i)] == cookie_index[1] && 
         hash[HASH_INDEX3(i)] == cookie_index[2])) {
      return 0;
    }
  }

  /* None matched -> failure */
  return BC_HASH;
#undef HOST
#undef SALT
}

/*
   opreq = o #chan nick
   inreq = i #chan nick uhost 
   inreq keyreply = K #chan key
 */
void
getin_request(char *botnick, char *code, char *par)
{
  if (unlikely(!server_online))
    return;

  if (unlikely(!par[0] || !par))
    return;

  char *what = newsplit(&par), *chname = newsplit(&par);

  if (unlikely(!chname[0] || !chname))
    return;

  char *tmp = newsplit(&par);		/* nick */

  if (unlikely(!tmp[0]))
    return;

  char nick[NICKLEN] = "";
  strlcpy(nick, tmp, sizeof(nick));

  const char *type = what[0] == 'o' ? "op" : "in", *desc = what[0] == 'o' ? "on" : "for";

  struct chanset_t *chan = findchan_by_dname(chname);
  if (unlikely(!chan)) {
    putlog(LOG_GETIN, "*", "%sreq from %s/%s %s %s which is not a valid channel!", type, botnick, nick, desc, chname);
    return;
  }

  struct userrec *u = get_user_by_handle(userlist, botnick);
  if (unlikely(!u)) {
    putlog(LOG_GETIN, "*", "%sreq from %s/%s %s %s - No user called %s in userlist", type, botnick, nick, desc, 
                           chan->dname, botnick);
    return;
  }

  if (connect_bursting) {
    putlog(LOG_GETIN, "*", "%sreq from %s/%s %s %s - I'm in connect burst mode.", type, botnick, nick, desc, chan->dname);
    return;
  }

  if (server_lag > lag_threshold) {
    putlog(LOG_GETIN, "*", "%sreq from %s/%s %s %s - I'm too lagged", type, botnick, nick, desc, chan->dname);
    return;
  }


  if (what[0] != 'K') {
    if (!(channel_pending(chan) || channel_active(chan))) {
      return;
    }
  }

  memberlist* mem = ismember(chan, nick);

  if (mem && chan_issplit(mem)) {
    putlog(LOG_GETIN, "*", "%sreq from %s/%s %s %s - %s is split", type, botnick, nick, desc, chan->dname, nick);
    return;
  }

  if (what[0] == 'K') {
    if (unlikely(!shouldjoin(chan))) {
      putlog(LOG_GETIN, "*", "Got key for %s from %s - I shouldn't be on that chan?!?", chan->dname, botnick);
    } else {
      if (!(channel_pending(chan) || channel_active(chan))) {
        my_setkey(chan, nick);
        putlog(LOG_GETIN, "*", "Got key for %s from %s (%s) - Joining", chan->dname, botnick, chan->channel.key);
        join_chan(chan);
      } else {
        putlog(LOG_GETIN, "*", "Got key for %s from %s - I'm already in the channel", chan->dname, botnick);
      }
    }
    return;
  }

  char uhost[UHOSTLEN] = "";
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };

  tmp = newsplit(&par);			/* userhost */

  if (tmp[0])
    strlcpy(uhost, tmp, sizeof(uhost));

  if (what[0] == 'o') {
    if (!me_op(chan)) {
      putlog(LOG_GETIN, "*", "opreq from %s/%s on %s - I haven't got ops", botnick, nick, chan->dname);
      return;
    }

    if (!mem) {
      putlog(LOG_GETIN, "*", "opreq from %s/%s on %s - %s isn't on %s", botnick, nick, chan->dname, nick, chan->dname);
      return;
    }

    if (chan_hasop(mem)) {
      putlog(LOG_GETIN, "*", "opreq from %s/%s on %s - %s already has ops", botnick, nick, chan->dname, nick);
      return;
    }

    if (chan_sentop(mem)) {
      putlog(LOG_GETIN, "*", "opreq from %s/%s on %s - Already sent a +o", botnick, nick, chan->dname);
      return;
    }

    member_getuser(mem);

    if (mem->user != u) {
      putlog(LOG_GETIN, "*", "opreq from %s/%s on %s - user mismatch: (%s)/(%s)", botnick, nick, chan->dname, u ? u->handle : "*",  mem->user ? mem->user->handle : "*");
      return;
    }

    get_user_flagrec(u, &fr, chan->dname, chan);

    if (!chk_op(fr, chan)) {
      putlog(LOG_GETIN, "*", "opreq from %s/%s on %s - %s doesnt have +o for chan.", botnick, nick, chan->dname, botnick);
      return;
    }

    if (unlikely(glob_kick(fr) || chan_kick(fr))) {
      putlog(LOG_GETIN, "*", "opreq from %s/%s on %s - %s is currently being autokicked.", botnick, nick, chan->dname, botnick);
      return;
    }

    // Does the remote bot have the same number of clients in its channel as me? And a shared member?
    const char *bot_network = tmp;
    int members = atoi(tmp);

    char *shared_nick = par[0] ? newsplit(&par) : NULL;
    memberlist* shared_member = shared_nick ? ismember(chan, shared_nick) : NULL;
    char *shared_host = par[0] ? newsplit(&par) : NULL;
    if (!shared_nick || !shared_member || !shared_host || (strcasecmp(curnetwork, bot_network) && ((chan->channel.members - chan->channel.splitmembers) != members)) || strcmp(shared_host, shared_member->userhost)) {
      putlog(LOG_GETIN, "*", "opreq from %s/%s on %s - Bot seems to be on a different network '%s' / me: '%s'", botnick, nick, chan->dname, bot_network, curnetwork);
      return;
    }

    if (chan->channel.no_op) {
      if (chan->channel.no_op > now)    /* dont op until this time has passed */
        return;
      else
        chan->channel.no_op = 0;
    }
    do_op(mem, chan, 0, 1);

    putlog(LOG_GETIN, "*", "opreq from %s/%s on %s - Opped", botnick, nick, chan->dname);
  } else if (what[0] == 'i') {
    if (mem) {
      putlog(LOG_GETIN, "*", "inreq from %s/%s for %s - %s is already on %s", botnick, nick, chan->dname, nick, chan->dname);
      return;
    }

    get_user_flagrec(u, &fr, chan->dname, chan);

    if (unlikely(!chk_op(fr, chan) || chan_kick(fr) || glob_kick(fr))) {
      putlog(LOG_GETIN, "*", "inreq from %s/%s for %s - %s doesn't have acces for chan.", botnick, nick, chan->dname, botnick);
      return;
    }

    char uip[UHOSTLEN] = "";
    tmp = newsplit(&par);		/* userip */
    if (tmp[0])
      simple_snprintf(uip, sizeof uip, "%s!%s", nick, tmp);

    char chankey[128] = "";
    tmp = newsplit(&par);		/* what the bot thinks the key is */
    if (tmp[0])
      simple_snprintf(chankey, sizeof(chankey), "%s", tmp);

    if (chan->channel.mode & CHANKEY && chan->channel.key[0] &&
        (!chankey[0] || strcmp(chan->channel.key, chankey))) {
      char *key = chan->channel.key[0] ? chan->channel.key : NULL;
      size_t siz = strlen(chan->dname) + strlen(key ? key : 0) + 6 + 1;
      tmp = (char *) calloc(1, siz);
      simple_snprintf(tmp, siz, "gi K %s %s", chan->dname, key ? key : "");
      putbot(botnick, tmp);
      putlog(LOG_GETIN, "*", "inreq from %s/%s for %s - Sent key (%s)", botnick, nick, chan->dname, key ? key : "");
      free(tmp);
    }

    // Should I respond to this request?
    // If there's 18 eligible bots in the channel, and in-bots is 2, I have a 2/18 chance of replying.
    int eligible_bots = 0;
    for (memberlist* m = chan->channel.member; m && m->nick[0]; m = m->next) {
      if (chan_hasop(m)) {
        member_getuser(m, 0);
        if (m->user && m->user->bot) {
          ++eligible_bots;
        }
      }
    }

    if (!eligible_bots) {
      return;
    }

    if (!((randint(eligible_bots) + 1) <= static_cast<unsigned int>(in_bots))) {
      // Not my turn
      return;
    }

    if (!me_op(chan)) {
      putlog(LOG_GETIN, "*", "inreq from %s/%s for %s - I haven't got ops", botnick, nick, chan->dname);
      return;
    }


    bool sendi = 0;

    if (chan->channel.maxmembers) {
      if (raise_limit(chan, 5)) {
        putlog(LOG_GETIN, "*", "inreq from %s/%s for %s - Raised limit", botnick, nick, chan->dname);
      }
    }

    struct maskrec **mr = NULL, *tmr = NULL;

    /* Check internal global bans */
    mr = &global_bans;
    while (*mr) {
      if (unlikely(wild_match((*mr)->mask, uhost) ||
          wild_match((*mr)->mask, uip) ||
          match_cidr((*mr)->mask, uip))) {

        if (!noshare)
          shareout("-m b %s\n", (*mr)->mask);

        putlog(LOG_GETIN, "*", "inreq from %s/%s for %s - Removed permanent global ban %s", botnick, nick,
               chan->dname, (*mr)->mask);
        free((*mr)->mask);
        free((*mr)->desc);
        free((*mr)->user);
        tmr = *mr;
        *mr = (*mr)->next;
        free(tmr);
      } else {
        mr = &((*mr)->next);
      }
    }

    /* Check internal channel bans */
    mr = &chan->bans;
    while (*mr) {
      if (unlikely(wild_match((*mr)->mask, uhost) ||
          wild_match((*mr)->mask, uip) ||
          match_cidr((*mr)->mask, uip))) {
        if (!noshare)
          shareout("-mc b %s %s\n", chan->dname, (*mr)->mask);
        putlog(LOG_GETIN, "*", "inreq from %s/%s for %s - Removed permanent channel ban %s", botnick, nick,
               chan->dname, (*mr)->mask);
        free((*mr)->mask);
        free((*mr)->desc);
        free((*mr)->user);
        tmr = *mr;
        *mr = (*mr)->next;
        free(tmr);
      } else {
        mr = &((*mr)->next);
      }
    }

    /* Check bans active in channel */
    for (struct maskstruct *b = chan->channel.ban; b->mask[0]; b = b->next) {
      if (unlikely(wild_match(b->mask, uhost) ||
          wild_match(b->mask, uip) ||
          match_cidr(b->mask, uip))) {
        add_mode(chan, '-', 'b', b->mask);
        putlog(LOG_GETIN, "*", "inreq from %s/%s for %s - Removed active ban %s", botnick, nick, chan->dname,
               b->mask);
        sendi = 1;
      }
    }

    if (chan->channel.mode & CHANINV) {
      sendi = 1;
      putlog(LOG_GETIN, "*", "inreq from %s/%s for %s - Invited", botnick, nick, chan->dname);
    }

    if (sendi)
      dprintf(DP_MODE, "INVITE %s %s\n", nick, chan->name[0] ? chan->name : chan->dname);
  }
}

static void
request_op(struct chanset_t *chan)
{
  if (!chan) {
    return;
  }

  if (channel_pending(chan) || !shouldjoin(chan) || !channel_active(chan) || me_op(chan)) {
    chan->channel.do_opreq = 0;
    return;
  }

  if (chan->channel.no_op) {
    if (chan->channel.no_op > now)      /* dont op until this time has passed */
      return;
    else
      chan->channel.no_op = 0;
  }

  /* check server lag */
  if (server_lag > lag_threshold) {
    putlog(LOG_GETIN, "*", "Not asking for ops on %s - I'm too lagged", chan->dname);
    chan->channel.no_op = now + 10;
    return;
  }

  /* Check if my hostmask is recognized (every 10th time) */
  static int check_hostmask_cnt = 10;
  if (check_hostmask_cnt++ == 10) {
    check_hostmask();
    check_hostmask_cnt = 0;
  }

  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_BOT, 0, 0, 0 };

  get_user_flagrec(conf.bot->u, &fr, chan->dname, chan);

  if (!chk_op(fr, chan) || glob_kick(fr) || chan_kick(fr)) {
    putlog(LOG_GETIN, "*", "Not requesting op for %s - I do not have access to that channel.", chan->dname);
    chan->channel.no_op = now + 10;
    return;
  }

  int i = 0, my_exp = 0, first = 100;
  time_t n = now;

  /* max OPREQ_COUNT requests per OPREQ_SECONDS sec */
  while (i < 5) {
    if (n - chan->opreqtime[i] > op_requests.time) {
      if (first > i)
        first = i;
      my_exp++;
      chan->opreqtime[i] = 0;
    }
    i++;
  }
  if ((5 - my_exp) >= op_requests.count) {
    putlog(LOG_GETIN, "*", "Delaying opreq for %s - Maximum of %d:%d reached", chan->dname, op_requests.count,
           op_requests.time);
    chan->channel.no_op = now + op_requests.time + 1;
    return;
  }

  int cnt = op_bots, i2 = 0;
  memberlist *ml = NULL, *botops[MAX_BOTS];
  char s[UHOSTLEN] = "";

  for (i = 0, ml = chan->channel.member; (i < MAX_BOTS) && ml && ml->nick[0]; ml = ml->next) {
    if (!chan_hasop(ml) || chan_issplit(ml))
      continue;

    member_getuser(ml, 1);

    if (!ml->user || !ml->user->bot)
      continue;

    /* Is the bot linked? */
    if (findbot(ml->user->handle))
      botops[i++] = ml;

  }
  if (!i) {
    if (channel_active(chan) && !channel_pending(chan)) {
      chan->channel.no_op = now + op_requests.time;
      putlog(LOG_GETIN, "*", "No one to ask for ops on %s - Delaying requests for %d seconds.", chan->dname, op_requests.time);
    }
    return;
  }

  char l[1024] = "";
  size_t len = 0;

  // Pick a random member to use as verification
  memberlist* shared_member = NULL;
  for (int z = 0; z < 5; ++z) {
    int shared_member_cnt = z < 4 ? randint(chan->channel.members) : 0;
    int shared_idx = 0;

    for (shared_member = chan->channel.member; shared_member && shared_member->nick[0]; shared_member = shared_member->next) {
      if (shared_member->split) continue;
      if (shared_idx >= shared_member_cnt)
        break;
      ++shared_idx;
    }
    if (shared_member) break;
  }
  if (!shared_member) {
    chan->channel.no_op = now + op_requests.time;
    putlog(LOG_GETIN, "*", "Too many split clients on %s - Delaying requests for %d seconds.", chan->dname, op_requests.time);
    return;
  }

  /* first scan for bots on my server, ask first found for ops */
  simple_snprintf(s, sizeof(s), "gi o %s %s %s %s %s", chan->dname, botname, curnetwork, shared_member->nick, shared_member->userhost);

  /* look for bots 0-1 hops away */
  for (i2 = 0; i2 < i; i2++) {
    if (botops[i2]->hops < 2) {
      putbot(botops[i2]->user->handle, s);
      chan->opreqtime[first] = n;
      if (l[0])
        len += strlcpy(l + len, ", ", sizeof(l) - len);
      len += strlcpy(l + len, botops[i2]->user->handle, sizeof(l) - len);
      len += strlcpy(l + len, "/", sizeof(l) - len);
      len += strlcpy(l + len, botops[i2]->nick, sizeof(l) - len);
      botops[i2] = NULL;
      --cnt;
      break;
    }
    //FIXME: Also prefer our localhub or child bots
  }

  /* Pick random op and ask for ops */
  while (cnt) {
    i2 = randint(i);
    if (botops[i2]) {
      putbot(botops[i2]->user->handle, s);
      chan->opreqtime[first] = n;
      if (l[0])
        len += strlcpy(l + len, ", ", sizeof(l) - len);
      len += strlcpy(l + len, botops[i2]->user->handle, sizeof(l) - len);
      len += strlcpy(l + len, "/", sizeof(l) - len);
      len += strlcpy(l + len, botops[i2]->nick, sizeof(l) - len);
      --cnt;
      botops[i2] = NULL;
    } else {
      if (i < op_bots)
        cnt--;
    }
  }
  l[len] = 0;
  putlog(LOG_GETIN, "*", "Requested ops on %s from %s", chan->dname, l);
}

static void
request_in(struct chanset_t *chan)
{
  /* Lag situation */
  if (!shouldjoin(chan))
    return;

  /* Check if my hostmask is recognized (every 10th time) */
  static int check_hostmask_cnt = 10;
  if (check_hostmask_cnt++ == 10) {
    check_hostmask();
    check_hostmask_cnt = 0;
  }

  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_BOT, 0, 0, 0 };

  get_user_flagrec(conf.bot->u, &fr, chan->dname, chan);

  if (!chk_op(fr, chan) || glob_kick(fr) || chan_kick(fr)) {
    putlog(LOG_GETIN, "*", "Not requesting help to join %s - I do not have access to that channel.", chan->dname);
    return;
  }

  int foundBots = 0;
//  char* botops[MAX_BOTS];

  for (tand_t* bot = tandbot; bot && (foundBots < MAX_BOTS); bot = bot->next) {
    if (bot->hub || !bot->u)
      continue;

    get_user_flagrec(bot->u, &fr, chan->dname, chan);
    if (bot_shouldjoin(bot->u, &fr, chan) && chk_op(fr, chan)) {
      ++foundBots;
//      botops[foundBots++] = bot->bot;
    }
  }

  if (!foundBots) {
    putlog(LOG_GETIN, "*", "No bots available, can't request help to join %s", chan->dname);
    return;
  }

  bd::String request(bd::String::printf("gi i %s %s %s!%s %s %s", chan->dname, botname, botname, botuserhost, botuserip, chan->channel.key[0] ? chan->channel.key : ""));
  putallbots(request.c_str());
  putlog(LOG_GETIN, "*", "Requested help to join %s", chan->dname);
}

/* Set the key.
 */
void
my_setkey(struct chanset_t *chan, char *k)
{
  free(chan->channel.key);
  chan->channel.key = k ? strdup(k) : (char *) calloc(1, 1);
}

/* Adds a ban, exempt or invite mask to the list
 * m should be chan->channel.(exempt|invite|ban)
 */
static bool
new_mask(masklist *m, char *s, char *who)
{
  for (; m && m->mask[0] && rfc_casecmp(m->mask, s); m = m->next) ;
  if (m->mask[0])
    return 1;                     /* Already existent mask */

  m->next = (masklist *) calloc(1, sizeof(masklist));
  m->next->next = NULL;
  m->next->mask = (char *) calloc(1, 1);
  free(m->mask);
  m->mask = strdup(s);
  m->who = strdup(who);
  m->timer = now;
  return 0;
}

/* Removes a nick from the channel member list (returns 1 if successful)
 */
static bool
killmember(struct chanset_t *chan, char *nick, bool cacheMember)
{
  memberlist *x = NULL, *old = NULL;

  for (x = chan->channel.member; x && x->nick[0]; old = x, x = x->next)
    if (!rfc_casecmp(x->nick, nick))
      break;
  if (unlikely(!x || !x->nick[0])) {
    if (!channel_pending(chan))
      putlog(LOG_MISC, "*", "(!) killmember(%s, %s) -> nonexistent", chan->dname, nick);
    return 0;
  }
  if (old)
    old->next = x->next;
  else
    chan->channel.member = x->next;

  chan->channel.hashed_members->remove(*x->rfc_nick);

  if (cacheMember) {
    x->last = now;
    x->user = NULL;
    x->next = NULL;
    x->tried_getuser = 0;
    // Don't delete here, will delete when it expires from the cache.
    (*chan->channel.cached_members)[x->userhost] = x;
  } else {
    chan->channel.cached_members->remove(x->userhost);
    delete_member(x);
  }

  --chan->channel.members;

  /* The following two errors should NEVER happen. We will try to correct
   * them though, to keep the bot from crashing.
   */
  if (unlikely(chan->channel.members < 0)) {
    chan->channel.members = 0;
    chan->channel.splitmembers = 0;
    for (x = chan->channel.member; x && x->nick[0]; x = x->next) {
      chan->channel.members++;
      if (x->split)
        ++(chan->channel.splitmembers);
    }
    putlog(LOG_MISC, "*", "(!) actually I know of %d members.", chan->channel.members);
  }
  if (unlikely(!chan->channel.member)) {
    chan->channel.member = (memberlist *) calloc(1, sizeof(memberlist));
    chan->channel.member->nick[0] = 0;
    chan->channel.member->next = NULL;
  }
  return 1;
}

/**
 * Update the member with cached information from a parted/quitted member
 */
static void member_update_from_cache(struct chanset_t* chan, memberlist *m) {
  // Are they in the cache?
  const bd::String userhost(m->userhost);
  if (chan->channel.cached_members->contains(userhost)) {
    memberlist *cached_member = (*chan->channel.cached_members)[userhost];

    // Update important flood tracking information
    swap(m->floodtime, cached_member->floodtime);
    swap(m->floodnum, cached_member->floodnum);

    // Update EVOICE flag
    if (cached_member->flags & EVOICE) {
      m->flags |= EVOICE;
    }

    // No longer need the cached member
    delete_member(cached_member);
    chan->channel.cached_members->remove(userhost);
  }
}

/* Check whether I'm voice. Returns boolean 1 or 0.
 */
bool
me_voice(const struct chanset_t *chan)
{
  memberlist *mx = ismember(chan, botname);

  if (!mx)
    return 0;
  if (chan_hasvoice(mx))
    return 1;
  else
    return 0;
}


/* Check if there are any ops on the channel. Returns boolean 1 or 0.
 */
static bool
any_ops(struct chanset_t *chan)
{
  memberlist *x = NULL;

  for (x = chan->channel.member; x && x->nick[0]; x = x->next)
    if (chan_hasop(x))
      break;
  if (!x || !x->nick[0])
    return 0;
  return 1;
}

static void get_channel_masks(struct chanset_t* chan) {
  bd::String tocheck(size_t(4));
  if (!(chan->ircnet_status & CHAN_ASKEDBANS)) {
    chan->ircnet_status |= CHAN_ASKEDBANS;
    tocheck += 'b';
  }

  if (do_eI) {
    chan->channel.last_eI = now;
    if (!(chan->ircnet_status & CHAN_ASKED_EXEMPTS) && use_exempts == 1) {
      chan->ircnet_status |= CHAN_ASKED_EXEMPTS;
      tocheck += 'e';
    }
    if (!(chan->ircnet_status & CHAN_ASKED_INVITES) && use_invites == 1) {
      chan->ircnet_status |= CHAN_ASKED_INVITES;
      tocheck += 'I';
    }
  }

  if (tocheck.length())
    dprintf(DP_MODE, "MODE %s %s\n", chan->name, tocheck.c_str());
}

/* Reset the channel information.
 */
void
reset_chan_info(struct chanset_t *chan)
{
  if (!chan) return;

  if (!chan->name[0])
    strlcpy(chan->name, chan->dname, sizeof(chan->name));

  /* Don't reset the channel if we're already resetting it */
  if (!shouldjoin(chan)) {
    sdprintf("Resetting %s but I shouldn't be there, parting...", chan->dname);
    dprintf(DP_MODE, "PART %s\n", chan->name);
    return;
  }

  if (!channel_pending(chan)) {
    bool opped = me_op(chan) ? 1 : 0;

    my_setkey(chan, NULL);
    clear_channel(chan, 1);
    chan->ircnet_status |= CHAN_PEND;
    chan->ircnet_status &= ~(CHAN_ACTIVE | CHAN_ASKEDMODES | CHAN_JOINING);
    /* don't bother checking bans if it's +take */
    if (!channel_take(chan)) {
      if (opped) {
        get_channel_masks(chan);
      }
    }
    /* These 2 need to get out asap, so into the mode queue */
    dprintf(DP_MODE, "MODE %s\n", chan->name);
    send_chan_who(DP_MODE, chan, 1);
    /* clear_channel nuked the data...so */
    dprintf(DP_HELP, "TOPIC %s\n", chan->name);//Topic is very low priority
  }
}

static void send_chan_who(int queue, struct chanset_t *chan, bool chain) {
  if (chain) {
    if (std::find(std::begin(chained_who), std::end(chained_who),
          chan->name) != std::end(chained_who))
      chained_who.push_back(chan->name);
    chained_who_idx = queue;
    if (chained_who.size() > 1)
      return;
  }
  if (use_354) /* Added benefit of getting numeric IP! :) */
    dprintf(queue, "WHO %s %%c%%h%%n%%u%%f%%r%%d%%i\n", chan->name);
  else
    dprintf(queue, "WHO %s\n", chan->name);
}

void force_join_chan(struct chanset_t* chan, int idx) {
  chan->ircnet_status = 0;
  join_chan(chan, idx);
}

void join_chan(struct chanset_t* chan, int idx) {
  if (shouldjoin(chan) && !(chan->ircnet_status & (CHAN_ACTIVE | CHAN_PEND | CHAN_JOINING))) {
    dprintf(idx, "JOIN %s %s\n",
        (chan->name[0]) ? chan->name : chan->dname,
        chan->channel.key[0] ? chan->channel.key : chan->key_prot);
    clear_channel(chan, 1);
    chan->ircnet_status |= CHAN_JOINING;
  }
}

/* If i'm the only person on the channel, and i'm not op'd,
 * might as well leave and rejoin. If i'm NOT the only person
 * on the channel, but i'm still not op'd, demand ops.
 */
static void
check_lonely_channel(struct chanset_t *chan)
{
  if (channel_pending(chan) || !channel_active(chan) || me_op(chan) ||
      !shouldjoin(chan) || (chan->channel.mode & CHANANON))
    return;

  static int whined = 0;

  if ((chan->channel.members - chan->channel.splitmembers) == 1 && channel_cycle(chan) && !channel_stop_cycle(chan)) {
    if (chan->name[0] != '+') { /* Its pointless to cycle + chans for ops */
      putlog(LOG_MISC, "*", "Trying to cycle %s to regain ops.", chan->dname);
      dprintf(DP_MODE, "PART %s\n", chan->name);
      // Will auto rejoin once the bot PARTs
      whined = 0;
    }
  } else if (any_ops(chan)) {
    whined = 0;
  } else {
    /* Other people here, but none are ops. If there are other bots make
     * them LEAVE!
     */
    if (!whined) {
      /* + is opless. Complaining about no ops when without special
       * help(services), we cant get them - Raist
       */
      if (chan->name[0] != '+')
        putlog(LOG_MISC, "*", "%s is active but has no ops :(", chan->dname);
      whined = 1;
    }
#ifdef disabled
    memberlist *m = NULL;
    bool ok = 1;

    for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
      member_getuser(m, 0);

      if (!m->is_me && (!m->user || !m->user->bot)) {
        ok = 0;
        break;
      }
    }
    if (ok && channel_cycle(chan)) {
      /* ALL bots!  make them LEAVE!!! */
/*
      for (m = chan->channel.member; m && m->nick[0]; m = m->next)
	if (!m->is_me)
	  dprintf(DP_SERVER, "PRIVMSG %s :go %s\n", m->nick, chan->dname);
*/
    }
#endif
  }
}

static void
warn_pls_take(struct chanset_t *chan)
{
  if (channel_take(chan) && me_op(chan))
    putlog(LOG_WARN, "*", "%s is set +take, and I'm already opped! +take is insecure, try +bitch instead",
           chan->dname);
}

/* FIXME: max sendq will occur. */
static void
check_servers(struct chanset_t *chan)
{
  for (memberlist *m = chan->channel.member; m && m->nick[0]; m = m->next) {
    if (!m->is_me && chan_hasop(m) && (m->hops == -1)) {
      putlog(LOG_DEBUG, "*", "Updating WHO for '%s' because '%s' is missing data.", chan->dname, m->nick);
      send_chan_who(DP_HELP, chan);
      break;                    /* lets just do one chan at a time to save from flooding */
    }
  }
}

static void do_protect(struct chanset_t* chan, const char* reason) {
  // Don't bother with these if already botbitch, already processed it, or it's a hacked bot and +botbitch won't help.
  if (!channel_botbitch(chan)) {
    if (chan->protect_backup) {
      putlog(LOG_MISC, "*", "%s detected in %s: Setting +botbitch/+backup to protect the channel.", reason, chan->dname);
      do_chanset(NULL, chan, "+botbitch +bitch +backup", DO_LOCAL | DO_NET);
    } else {
      putlog(LOG_MISC, "*", "%s detected in %s: Setting +botbitch to protect the channel.", reason, chan->dname);
      do_chanset(NULL, chan, "+botbitch +bitch", DO_LOCAL | DO_NET);
    }
//        enforce_closed(chan);
//        dprintf(DP_MODE, "TOPIC %s :Auto-closed - channel fight\n", chan->name);
//    enforce_bitch(chan);
    reversing = 1; // Reverse any modes which triggered this.
  }
}

static void
check_netfight(struct chanset_t *chan)
{
  if (channel_protect(chan) && fight_threshold) {
    if ((chan->channel.fighting) && (chan->channel.fighting > fight_threshold) && !chan_bitch(chan)) {
      do_protect(chan, "Channel fight");
    }
  }
  chan->channel.fighting = 0;   /* we put this here because we need to clear it once per min */
}

bool
raise_limit(struct chanset_t *chan, int default_limitraise)
{
  if (!chan || !me_op(chan))
    return false;

  /* Don't bother setting limit if the user has set a protect -l */
  if (chan->mode_mns_prot & CHANLIMIT)
    return false;

  const int limitraise = (chan->limitraise ? ((chan->limitraise % 2 == 0) ? chan->limitraise : (chan->limitraise + 1)) : default_limitraise);
  if (limitraise) {
    const int nl = (chan->channel.members - chan->channel.splitmembers) + limitraise;	/* new limit */
    const int i = limitraise >> 2;			/* DIV 4 */
    /* if the newlimit will be in the range made by these vars, dont change. */
    const int ul = nl + i;					/* upper limit */
    const int ll = nl - i;					/* lower limit */

    if ((chan->channel.maxmembers >= ll) && (chan->channel.maxmembers <= ul)) {
      return false;                     /* the current limit is in the range, so leave it. */
    }

    if (nl != chan->channel.maxmembers) {
      char s[6] = "";

      simple_snprintf(s, sizeof(s), "%d", nl);
      add_mode(chan, '+', 'l', s);

      return true;
    }
  }

  return false;
}

void check_shouldjoin(struct chanset_t* chan)
{
  if ((channel_active(chan) || channel_pending(chan)) && !shouldjoin(chan)) {
    sdprintf("Active/Pending in %s but I shouldn't be there, parting...", chan->dname);
    dprintf(DP_SERVER, "PART %s\n", chan->name[0] ? chan->name : chan->dname);
  } else if (shouldjoin(chan)) {
    join_chan(chan);
  }
}

static void
check_expired_chanstuff(struct chanset_t *chan)
{
  memberlist *m = NULL;
  check_shouldjoin(chan);
  if (channel_active(chan) && shouldjoin(chan)) {
    masklist *b = NULL, *e = NULL;
    memberlist *n = NULL;
    struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };

    if (me_op(chan)) {
      if (channel_dynamicbans(chan) && chan->ban_time)
        for (b = chan->channel.ban; b->mask[0]; b = b->next)
          if (now - b->timer > 60 * chan->ban_time &&
              !u_sticky_mask(chan->bans, b->mask) &&
              !u_sticky_mask(global_bans, b->mask) && expired_mask(chan, b->who)) {
            putlog(LOG_MODES, chan->dname, "(%s) Channel ban on %s expired.", chan->dname, b->mask);
            add_mode(chan, '-', 'b', b->mask);
            b->timer = now;
          }
      if (use_exempts && channel_dynamicexempts(chan) && chan->exempt_time)
        for (e = chan->channel.exempt; e->mask[0]; e = e->next)
          if (now - e->timer > 60 * chan->exempt_time &&
              !u_sticky_mask(chan->exempts, e->mask) &&
              !u_sticky_mask(global_exempts, e->mask) && expired_mask(chan, e->who)) {
            /* Check to see if it matches a ban */
            int match = 0;

            for (b = chan->channel.ban; b->mask[0]; b = b->next)
              if (wild_match(b->mask, e->mask) || wild_match(e->mask, b->mask)) {
                match = 1;
                break;
              }
            /* Leave this extra logging in for now. Can be removed later
             * Jason
             */
            if (match) {
              putlog(LOG_MODES, chan->dname,
                     "(%s) Channel exemption %s NOT expired. Exempt still set!", chan->dname, e->mask);
            } else {
              putlog(LOG_MODES, chan->dname, "(%s) Channel exemption on %s expired.", chan->dname, e->mask);
              add_mode(chan, '-', 'e', e->mask);
            }
            e->timer = now;
          }

      if (use_invites && channel_dynamicinvites(chan) && chan->invite_time && !(chan->channel.mode & CHANINV)) {
        for (b = chan->channel.invite; b->mask[0]; b = b->next) {
          if (now - b->timer > 60 * chan->invite_time &&
              !u_sticky_mask(chan->invites, b->mask) &&
              !u_sticky_mask(global_invites, b->mask) && expired_mask(chan, b->who)) {
            putlog(LOG_MODES, chan->dname, "(%s) Channel invitation on %s expired.", chan->dname, b->mask);
            add_mode(chan, '-', 'I', b->mask);
            b->timer = now;
          }
        }
      }
    } /* me_op */

    size_t splitmembers = 0, bot_ops = 0;
    const bool im_opped = me_op(chan);
    for (m = chan->channel.member; m && m->nick[0]; m = n) {
      n = m->next;
      // Update split members
      if (m->split) {
        ++splitmembers;
        if (now - m->split > wait_split) {
          putlog(LOG_JOIN, chan->dname, "%s (%s) got lost in the net-split.", m->nick, m->userhost);
          --(chan->channel.splitmembers);
          killmember(chan, m->nick, false);
          continue;
        }
      }

      //This bot is set +r, so resolve.
      if (!m->userip[0] && doresolv(chan)) {
        char host[UHOSTLEN] = "", *p = NULL;
        p = strchr(m->userhost, '@');
        if (p) {
          ++p;
          strlcpy(host, p, strlen(m->userhost) - (p - host));
          resolve_to_member(chan, m->nick, host);
        }
      }

      if (im_opped) {
        if ((chan->role & (ROLE_OP|ROLE_VOICE)) && !loading && !chan_hasop(m)) {      /* autovoice of +v users if bot is +y */
          get_user_flagrec(m->user, &fr, chan->dname, chan);

          /* Autoop */
          if ((chan->role & ROLE_OP) && !chan_sentop(m) && chk_autoop(m, fr, chan)) {
            do_op(m, chan, 0, 0);
          }

          /* +v or +voice */
          if ((chan->role & ROLE_VOICE) && !chan_hasvoice(m) && !chan_sentvoice(m)) {
            member_getuser(m, 1);

            if (m->user) {
              if (!(m->flags & EVOICE) &&
                  (
                   /* +voice: Voice all clients who are not flag:+q. If the chan is +voicebitch, only op flag:+v clients */
                   (channel_voice(chan) && !chk_devoice(fr) && (!channel_voicebitch(chan) || (channel_voicebitch(chan) && chk_voice(m, fr, chan)))) ||
                   /* Or, if the channel is -voice but they still qualify to be voiced */
                   (!channel_voice(chan) && !privchan(fr, chan, PRIV_VOICE) && chk_voice(m, fr, chan))
                  )
                 ) {
                add_mode(chan, '+', 'v', m);
              }
            } else if (!m->user && channel_voice(chan) && !channel_voicebitch(chan) && voice_ok(m, chan)) {
              add_mode(chan, '+', 'v', m);
            }
          }
        }
      }
      if (m->user && m->user->bot) {
        ++bot_ops;
      }
    }
    // Update minutely
    chan->channel.splitmembers = splitmembers;
    check_lonely_channel(chan);
    if (bot_ops && !im_opped) {
      request_op(chan);
    }

    if (chan->role & ROLE_CHANMODE) {
      recheck_channel_modes(chan);
    }
  }
  // Clear out expired cached members
  if (chan->channel.cached_members && chan->channel.cached_members->size()) {
    bd::Array<bd::String> member_uhosts(chan->channel.cached_members->keys());
    for (size_t i = 0; i < member_uhosts.length(); ++i) {
      const bd::String uhost(member_uhosts[i]);

      m = (*chan->channel.cached_members)[uhost];

      // Delete the expired member
      if (now - m->last > wait_split) {
        delete_member(m);
        chan->channel.cached_members->remove(uhost);
      }
    }
  }
}

void
irc_minutely()
{
  for (struct chanset_t *chan = chanset; chan; chan = chan->next) {
    warn_pls_take(chan);
    if (server_online) {
      if (!channel_pending(chan)) {
        check_netfight(chan);
        check_servers(chan);
      }
      check_expired_chanstuff(chan);
    }
  }
}


int check_bind_authc(char *cmd, Auth *a, char *chname, char *par)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  int x = 0;

  get_user_flagrec(a->user, &fr, chname);

  if (a->GetIdx(chname)) {
    x = check_auth_dcc(a, cmd, par);
  }

  LOGC(cmd);

  if (x & BIND_RET_BREAK)
    return (1);
  return (0);
}

/* Flush the modes for EVERY channel.
 */
void
flush_modes()
{
  if (!modesperline)		/* Haven't received 005 yet :) */
    return;

  memberlist *m = NULL;

  Context;

  for (struct chanset_t *chan = chanset; chan; chan = chan->next) {
    if (!me_op(chan)) continue;
    for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
      if (m->delay && m->delay <= now) {
        m->delay = 0L;
        m->flags &= ~FULL_DELAY;
        if (chan_sentop(m)) {
          m->flags &= ~SENTOP;
          do_op(m, chan, 0, 0);
        }
        if (chan_sentvoice(m)) {
          m->flags &= ~SENTVOICE;
          add_mode(chan, '+', 'v', m);
        }
      }
    }
    flush_mode(chan, NORMAL);
  }
}

void
irc_report(int idx, int details)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  char ch[1024] = "", q[160] = "";
  const char *p = NULL;
  int k = 10;
  size_t len;

  strlcpy(q, "Channels: ", sizeof(q));
  for (struct chanset_t *chan = chanset; chan; chan = chan->next) {
    if (idx != DP_STDOUT)
      get_user_flagrec(dcc[idx].user, &fr, chan->dname, chan);

    if (!privchan(fr, chan, PRIV_OP) && (idx == DP_STDOUT || glob_master(fr) || chan_master(fr))) {
      p = NULL;
      if (shouldjoin(chan)) {
        if (chan->ircnet_status & CHAN_JUPED)
          p = "juped";
        else if (channel_joining(chan))
          p = "joining";
        else if (!(chan->ircnet_status & CHAN_ACTIVE))
          p = "trying";
        else if (chan->ircnet_status & CHAN_PEND)
          p = "pending";
        else if ((chan->dname[0] != '+') && !me_op(chan))
          p = "want ops!";
      }
      len = simple_snprintf(ch, sizeof(ch), "%s%s%s%s, ", chan->dname, p ? "(" : "", p ? p : "", p ? ")" : "");
      if ((k + len) > 70) {
        dprintf(idx, "    %s\n", q);
        strlcpy(q, "           ", sizeof(q));
        k = 10;
      }
      k += my_strcpy(q + k, ch);
    }
  }
  if (k > 10) {
    q[k - 2] = 0;
    dprintf(idx, "    %s\n", q);
  }
}

static void bot_release_nick (char *botnick, char *code, char *par) {
  release_nick(par);
}

static void rebalance_roles_chan(struct chanset_t* chan)
{
  bd::Array<bd::String> bots;
  int *bot_bits;
  short role;
  size_t botcount, mappedbot, omappedbot, botidx, roleidx, rolecount;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  memberlist *m;

  if (chan->needs_role_rebalance == 0) {
    return;
  }

  if (channel_pending(chan) || !channel_active(chan) ||
      !shouldjoin(chan) || (chan->channel.mode & CHANANON)) {
    return;
  }

  /* Gather list of all bots in the channel. */
  /* XXX: Keep this known in chan->bots */
  for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
    if (!member_getuser(m) || !is_bot(m->user) || m->split) {
      continue;
    }

    get_user_flagrec(m->user, &fr, chan->dname, chan);

    /* Only consider bots that can be opped to be roled. */
    if (!chk_op(fr, chan)) {
      continue;
    }
    /* Only consider bots that have the roles feature. */
    if (!(m->user->fflags & FEATURE_ROLES)) {
      continue;
    }
    bots << m->user->handle;
  }
  botcount = bots.length();
  if (botcount == 0)
    return;
  bot_bits = (int*)calloc(botcount, sizeof(bot_bits[0]));

  for (roleidx = 0; role_counts[roleidx].name; roleidx++) {
    /* Map this role to a bot */
    omappedbot = mappedbot = roleidx % botcount;
    rolecount = role_counts[roleidx].count;
    role = role_counts[roleidx].role;

    /* Does the mapped bot have the bit yet? If not, check next bot,
     * on max restart at 0 but avoid looping back to start. */
    while (rolecount > 0) {
      if (!(bot_bits[mappedbot] & role)) {
        bot_bits[mappedbot] |= role;
        --rolecount;
      }

      /* Try next bot */
      ++mappedbot;

      /* Reached the end, wrap around. */
      if (mappedbot == botcount) {
        mappedbot = 0;
      }
      /* Reached original bot, cannot satisfy the role need. */
      if (mappedbot == omappedbot) {
        break;
      }
    }
  }

  /* Reset current bits */
  chan->bot_roles->clear();
  chan->role_bots->clear();

  /* Take bitmask of assigned roles and apply to bots. */
  for (botidx = 0; botidx < botcount; botidx++) {
    if (bot_bits[botidx] != 0) {
      (*chan->bot_roles)[bots[botidx]] = bot_bits[botidx];
    }
  }

  /* Fill role_bots */
  for (roleidx = 0; role_counts[roleidx].name; roleidx++) {
    role = role_counts[roleidx].role;
    /* Find all bots with this role */
    for (botidx = 0; botidx < botcount; botidx++) {
      if (bot_bits[botidx] & role) {
        (*chan->role_bots)[role] << bots[botidx];
      }
    }
  }

  /* Set my own roles */
  chan->role = (*chan->bot_roles)[conf.bot->nick];
  free(bot_bits);
  chan->needs_role_rebalance = 0;
}

static void rebalance_roles()
{
  struct chanset_t* chan = NULL;

  for (chan = chanset; chan; chan = chan->next) {
    rebalance_roles_chan(chan);
  }
}

static cmd_t irc_bot[] = {
  {"gi", "", (Function) getin_request, NULL, LEAF},
  {"mr", "", (Function) mass_request, NULL, LEAF},
  {"rn", "", (Function) bot_release_nick, NULL, LEAF},
  {NULL, NULL, NULL, NULL, 0}
};

void
irc_init()
{
  timer_create_secs(60, "irc_minutely", (Function) irc_minutely);
  timer_create_secs(10, "rebalance_roles", (Function) rebalance_roles);

  /* Add our commands to the imported tables. */
  add_builtins("dcc", irc_dcc);
  add_builtins("bot", irc_bot);
  add_builtins("raw", irc_raw);
  add_builtins("msg", C_msg);
}
/* vim: set sts=2 sw=2 ts=8 et: */
