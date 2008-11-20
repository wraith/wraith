/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2008 Bryan Drewery
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

#include <stdarg.h>

#define PRIO_DEOP 1
#define PRIO_KICK 2

#ifdef CACHE
static cache_t *irccache = NULL;
#endif /* CACHE */

#define do_eI (((now - chan->channel.last_eI) > 30) ? 1 : 0)

static int net_type = 0;
static time_t wait_split = 300;    /* Time to wait for user to return from
                                 * net-split. */
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
static bool include_lk = 1;      /* For correct calculation
                                 * in real_add_mode. */
static hash_table_t *bot_counters = NULL;

static int
voice_ok(memberlist *m, struct chanset_t *chan)
{
  if (m->flags & EVOICE)
    return 0;

  if (m->userhost[0] && !chan->voice_non_ident && m->userhost[0] == '~')
    return 0;

  return 1;
}

#include "chan.c"
#include "mode.c"
#include "cmdsirc.c"
#include "msgcmds.c"

static void
detect_offense(memberlist* m, struct chanset_t *chan, char *msg)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  struct userrec *u = m->user ? m->user : get_user_by_host(m->userhost);
  int i = 0;

  //size_t tot = strlen(msg);

  get_user_flagrec(u, &fr, chan->dname, chan);

  for (; *msg; ++msg) {
    if (egg_isupper(*msg))
      i++;
  }

/*  if ((chan->capslimit)) { */
  while (((msg) && *msg)) {
    if (egg_isupper(*msg))
      i++;
    msg++;
  }

/*
  if (chan->capslimit && ((i / tot) >= chan->capslimit)) {
dprintf(DP_MODE, "PRIVMSG %s :flood stats for %s: %d/%d are CAP, percentage: %d\n", chan->name, nick, i, tot, (i/tot)*100);
  if ((((i / tot) * 100) >= 50)) {
dprintf(DP_HELP, "PRIVMSG %s :cap flood.\n", chan->dname);
  }
*/
}

void unlock_chan(struct chanset_t *chan)
{
  if (chan->channel.drone_set_mode) {
    char buf[3] = "", *p = buf;
    if ((chan->channel.drone_set_mode & CHANINV) && !(chan->mode_pls_prot & CHANINV))
      *p++ = 'i';
    if ((chan->channel.drone_set_mode & CHANMODER) && !(chan->mode_pls_prot & CHANMODER))
      *p++ = 'm';
    *p = 0;
    dprintf(DP_MODE, "MODE %s :-%s\n", chan->name[0] ? chan->name : chan->dname, buf);
  }
  chan->channel.drone_set_mode = 0;
}

void detected_drone_flood(struct chanset_t* chan, memberlist* m) {
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

    dprintf(DP_DUMP, "MODE %s +%s\n", chan->name[0] ? chan->name : chan->dname, buf);
    howlong.sec = chan->flood_lock_time;
    howlong.usec = 0;
    timer_create_complex(&howlong, "unlock", (Function) unlock_chan, (void *) chan, 0);

    putlog(LOG_MISC, "*", "Flood detected in %s! Locking for %d seconds.", chan->dname, chan->flood_lock_time);
  }
}


void notice_invite(struct chanset_t *chan, char *handle, char *nick, char *uhost, bool op) {
  char fhandle[21] = "";
  const char *ops = " (auto-op)";

  if (handle)
    simple_snprintf(fhandle, sizeof(fhandle), "\002%s\002 ", handle);
  putlog(LOG_MISC, "*", "Invited %s%s(%s%s%s) to %s.", handle ? handle : "", handle ? " " : "", nick, uhost ? "!" : "", uhost ? uhost : "", chan->dname);
  dprintf(DP_MODE, "PRIVMSG %s :\001ACTION has invited %s(%s%s%s) to %s.%s\001\n",
    chan->name, fhandle, nick, uhost ? "!" : "", uhost ? uhost : "", chan->dname, op ? ops : "");
}

#ifdef CACHE
static cache_t *cache_new(char *nick)
{
  cache_t *cache = (cache_t *) my_calloc(1, sizeof(cache_t));

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
  cache_chan_t *cchan = (cache_chan_t *) my_calloc(1, sizeof(cache_chan_t));
  
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

const char * cookie_hash(const char* chname, const memberlist* opper, const memberlist* opped, const char* ts, const char* salt, const char* key) {
  char tohash[201] = "";
  const char salt2[] = SALT2;

  simple_snprintf(tohash, sizeof(tohash), STR("%c%s%c%c%c%c%c%s%s%s%s"),
                                     salt2[0], 
                                     ts,
                                     salt[0], salt[1], salt[2], salt[3],
                                     salt2[15],
                                     opper->nick,
                                     opped->nick,
                                     opped->userhost,
                                     key);
#ifdef DEBUG
sdprintf("chname: %s ts: %s salt: %c%c%c%c", chname, ts, salt[0], salt[1], salt[2], salt[3]);
sdprintf("tohash: %s", tohash);
#endif
  const char* md5 = MD5(tohash);
  OPENSSL_cleanse(tohash, sizeof(tohash));
  return md5;
}

#define HASH_INDEX1(_x) (8 + (_x))
#define HASH_INDEX2(_x) (16 + (_x))
#define HASH_INDEX3(_x) (18 + (_x))

void makecookie(char *out, size_t len, const char *chname, const memberlist* opper, const memberlist* m1, const memberlist* m2, const memberlist* m3) {
  char randstring[5] = "", ts[11] = "";
  static unsigned long counter = 0;

  make_rand_str(randstring, 4);
  /* &ts[4] is now last 6 digits of time */
  simple_snprintf(ts, sizeof(ts), "%010li", (long) (now + timesync));
  
  char cookie_clear[101] = "";

  //Increase my counter
  ++counter;
  simple_snprintf2(cookie_clear, sizeof(cookie_clear), STR("%s%s%D"), randstring, &ts[3], counter);

  char key[150] = "";
  simple_snprintf2(key, sizeof(key), "%c%c%c%s%c%c%c%c%c%c%^s%c%c%c%c%c%c%c%s",
                                        randstring[0],
                                        settings.salt1[5],
                                        randstring[3],
                                        opper->user->handle,
                                        randstring[2],
                                        settings.salt1[4],
                                        settings.salt1[0],
                                        settings.salt1[1],
                                        settings.salt1[3],
                                        settings.salt1[6],
                                        chname,
                                        settings.salt1[10],
                                        randstring[1],
                                        settings.salt2[15],
                                        settings.salt2[13],
                                        settings.salt1[10],
                                        settings.salt2[3],
                                        settings.salt2[1],
                                        opper->userhost
                                        );
  const char* hash1 = cookie_hash(chname, opper, m1, &ts[4], randstring, key);
  const char* hash2 = m2 ? cookie_hash(chname, opper, m2, &ts[4], randstring, key) : NULL;
  const char* hash3 = m3 ? cookie_hash(chname, opper, m3, &ts[4], randstring, key) : NULL;
  const char* cookie = encrypt_string(MD5(key), cookie_clear);
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
                         cookie);
  else if (m2)
    simple_snprintf(out, len + 1, STR("%c%c%c%c%c%c!%s@%s"), 
                         hash1[HASH_INDEX1(0)], 
                         hash1[HASH_INDEX2(0)], 
                         hash1[HASH_INDEX3(0)], 
                         hash2[HASH_INDEX1(1)], 
                         hash2[HASH_INDEX2(1)], 
                         hash2[HASH_INDEX3(1)], 
                         randstring, 
                         cookie);
  else
    simple_snprintf(out, len + 1, STR("%c%c%c!%s@%s"), 
                         hash1[HASH_INDEX1(0)], 
                         hash1[HASH_INDEX2(0)], 
                         hash1[HASH_INDEX3(0)], 
                         randstring, 
                         cookie);
#ifdef DEBUG
sdprintf("cookie: %s", out);
#endif
  free((char*)cookie);
}

static int checkcookie(const char *chname, const memberlist* opper, const memberlist* opped, const char *cookie, int indexHint) {
#define HOST(_x) (6 + (_x) + ((hashes << 1) + hashes)) /* x + (hashes * 3) */
#define SALT(_x) (1 + (_x) + ((hashes << 1) + hashes)) /* x + (hashes * 3) */
  /* How many hashes are in the cookie? */
  const size_t hashes = cookie[3] == '!' ? 1 : (cookie[6] == '!' ? 2 : 3);

  char key[150] = "";
  simple_snprintf2(key, sizeof(key), "%c%c%c%s%c%c%c%c%c%c%^s%c%c%c%c%c%c%c%s",
                                        cookie[SALT(0)],
                                        settings.salt1[5],
                                        cookie[SALT(3)],
                                        opper->user->handle,
                                        cookie[SALT(2)],
                                        settings.salt1[4],
                                        settings.salt1[0],
                                        settings.salt1[1],
                                        settings.salt1[3],
                                        settings.salt1[6],
                                        chname,
                                        settings.salt1[10],
                                        cookie[SALT(1)],
                                        settings.salt2[15],
                                        settings.salt2[13],
                                        settings.salt1[10],
                                        settings.salt2[3],
                                        settings.salt2[1],
                                        opper->userhost
                                        );
  char* cleartext = decrypt_string(MD5(key), (char*) &cookie[HOST(0)]);
  char ts[8] = "";
  strlcpy(ts, cleartext + 4, sizeof(ts));
  unsigned long counter = base64_to_int(cleartext + 4 + 7);

  //Lookup counter for the opper
  unsigned long last_counter = 0;
  if (hash_table_find(bot_counters, opper->user->handle, &last_counter) == -1) last_counter = 0;

#ifdef DEBUG
sdprintf("key: %s", key);
sdprintf("plaintext from cookie: %s", cleartext);
sdprintf("ts from cookie: %s", ts);
sdprintf("last counter from %s: %lu", opper->user->handle, last_counter);
sdprintf("counter from cookie: %lu", counter);
#endif

  free(cleartext);

  const time_t optime = atol(ts);
  if ((((now + timesync) % 10000000) - optime) > 3900)
    return BC_SLACK;

  if (counter <= last_counter)
    return BC_COUNTER;

  //Update counter for the opper
  hash_table_insert(bot_counters, opper->user->handle, (void *)(counter));

  const char *salt = &cookie[SALT(0)];
  const char *hash = cookie_hash(chname, opper, opped, &ts[1], salt, key);
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
  if (!server_online)
    return;

  if (!par[0] || !par)
    return;

  char *what = newsplit(&par), *chname = newsplit(&par);

  if (!chname[0] || !chname)
    return;

  char *tmp = newsplit(&par);		/* nick */

  if (!tmp[0])
    return;

  char nick[NICKLEN] = "";
  strlcpy(nick, tmp, sizeof(nick));

  const char *type = what[0] == 'o' ? "op" : "in", *desc = what[0] == 'o' ? "on" : "for";

  struct chanset_t *chan = findchan_by_dname(chname);
  if (!chan) {
    putlog(LOG_GETIN, "*", "%sreq from %s/%s %s %s which is not a valid channel!", type, botnick, nick, desc, chname);
    return;
  }

  struct userrec *u = get_user_by_handle(userlist, botnick);
  if (!u) {
    putlog(LOG_GETIN, "*", "%sreq from %s/%s %s %s - No user called %s in userlist", type, botnick, nick, desc, 
                           chan->dname, botnick);
    return;
  }

  if (server_lag > lag_threshold) {
    putlog(LOG_GETIN, "*", "%sreq from %s/%s %s %s - I'm too lagged", type, botnick, nick, desc, chan->dname);
    return;
  }


  if (what[0] != 'K') {
    if (!(channel_pending(chan) || channel_active(chan))) {
      putlog(LOG_GETIN, "*", "%sreq from %s/%s %s %s - I'm not on %s right now.", type, botnick, nick, desc, 
                             chan->dname, chan->dname);
      return;
    }
  }

  memberlist* mem = ismember(chan, nick);

  if (mem && chan_issplit(mem)) {
    putlog(LOG_GETIN, "*", "%sreq from %s/%s %s %s - %s is split", type, botnick, nick, desc, chan->dname, nick);
    return;
  }

  if (what[0] == 'K') {
    if (!shouldjoin(chan)) {
      putlog(LOG_GETIN, "*", "Got key for %s from %s - I shouldn't be on that chan?!?", chan->dname, botnick);
    } else {
      if (!(channel_pending(chan) || channel_active(chan))) {
        /* Cache the key */
        free(chan->channel.key);
        chan->channel.key = strdup(nick);

        putlog(LOG_GETIN, "*", "Got key for %s from %s (%s) - Joining", chan->dname, botnick, chan->channel.key);
        dprintf(DP_MODE, "JOIN %s %s\n", chan->name[0] ? chan->name : chan->dname, chan->channel.key);
        chan->status |= CHAN_JOINING;
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

    if (!mem->user && !mem->tried_getuser) {
      simple_snprintf(uhost, sizeof(uhost), "%s!%s", nick, mem->userhost);
      mem->user = get_user_by_host(uhost);
      mem->tried_getuser = 1;
    }

    if (mem->user != u) {
      putlog(LOG_GETIN, "*", "opreq from %s/%s on %s - user mismatch: (%s)/(%s)", botnick, nick, chan->dname, u ? u->handle : "*",  mem->user ? mem->user->handle : "*");
      return;
    }

    get_user_flagrec(u, &fr, chan->dname, chan);

    if (!chk_op(fr, chan)) {
      putlog(LOG_GETIN, "*", "opreq from %s/%s on %s - %s doesnt have +o for chan.", botnick, nick, chan->dname, botnick);
      return;
    }

    if (glob_kick(fr) || chan_kick(fr)) {
      putlog(LOG_GETIN, "*", "opreq from %s/%s on %s - %s is currently being autokicked.", botnick, nick, chan->dname, botnick);
      return;
    }

    if (chan->channel.no_op) {
      if (chan->channel.no_op > now)    /* dont op until this time has passed */
        return;
      else
        chan->channel.no_op = 0;
    }
    do_op(nick, chan, 0, 1);

    putlog(LOG_GETIN, "*", "opreq from %s/%s on %s - Opped", botnick, nick, chan->dname);
  } else if (what[0] == 'i') {
    if (mem) {
      putlog(LOG_GETIN, "*", "inreq from %s/%s for %s - %s is already on %s", botnick, nick, chan->dname, nick, chan->dname);
      return;
    }

    get_user_flagrec(u, &fr, chan->dname, chan);

    if (!chk_op(fr, chan) || chan_kick(fr) || glob_kick(fr)) {
      putlog(LOG_GETIN, "*", "inreq from %s/%s for %s - %s doesn't have acces for chan.", botnick, nick, chan->dname, botnick);
      return;
    }

    if (chan->channel.mode & CHANKEY) {
      char *key = chan->channel.key[0] ? chan->channel.key : chan->key_prot;
      size_t siz = strlen(chan->dname) + strlen(key ? key : 0) + 6 + 1;
      tmp = (char *) my_calloc(1, siz);
      simple_snprintf(tmp, siz, "gi K %s %s", chan->dname, key ? key : "");
      putbot(botnick, tmp);
      putlog(LOG_GETIN, "*", "inreq from %s/%s for %s - Sent key (%s)", botnick, nick, chan->dname, key ? key : "");
      free(tmp);
    }

    if (!me_op(chan)) {
      putlog(LOG_GETIN, "*", "inreq from %s/%s for %s - I haven't got ops", botnick, nick, chan->dname);
      return;
    }

    bool sendi = 0;

    if (chan->channel.maxmembers) {
      int lim = chan->channel.members + 5, curlim = chan->channel.maxmembers;
      if (curlim < lim) {
        char s2[6] = "";

        sendi = 1;
        simple_snprintf(s2, sizeof(s2), "%d", lim);
        add_mode(chan, '+', 'l', s2);
        putlog(LOG_GETIN, "*", "inreq from %s/%s for %s - Raised limit to %d", botnick, nick, chan->dname, lim);
      }
    }

    char uip[UHOSTLEN] = "";
    tmp = newsplit(&par);		/* userip */
    if (tmp[0])
      simple_snprintf(uip, sizeof uip, "%s!%s", nick, tmp);

    struct maskrec **mr = NULL, *tmr = NULL;

    /* Check internal global bans */
    mr = &global_bans;
    while (*mr) {
      if (wild_match((*mr)->mask, uhost) || 
          wild_match((*mr)->mask, uip) || 
          match_cidr((*mr)->mask, uip)) {

        if (!noshare)
          shareout("-m b %s\n", (*mr)->mask);

        putlog(LOG_GETIN, "*", "inreq from %s/%s for %s - Removed permanent global ban %s", botnick, nick,
               chan->dname, (*mr)->mask);
        free((*mr)->mask);
        if ((*mr)->desc)
          free((*mr)->desc);
        if ((*mr)->user)
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
      if (wild_match((*mr)->mask, uhost) || 
          wild_match((*mr)->mask, uip) || 
          match_cidr((*mr)->mask, uip)) {
        if (!noshare)
          shareout("-mc b %s %s\n", chan->dname, (*mr)->mask);
        putlog(LOG_GETIN, "*", "inreq from %s/%s for %s - Removed permanent channel ban %s", botnick, nick,
               chan->dname, (*mr)->mask);
        free((*mr)->mask);
        if ((*mr)->desc)
          free((*mr)->desc);
        if ((*mr)->user)
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
      if (wild_match(b->mask, uhost) || 
          wild_match(b->mask, uip) || 
          match_cidr(b->mask, uip)) {
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
  if (!chan || (chan && (channel_pending(chan) || !shouldjoin(chan) || !channel_active(chan) || me_op(chan))))
    return;

  if (chan->channel.no_op) {
    if (chan->channel.no_op > now)      /* dont op until this time has passed */
      return;
    else
      chan->channel.no_op = 0;
  }

  chan->channel.do_opreq = 0;
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

    if (!ml->user && !ml->tried_getuser) {
      simple_snprintf(s, sizeof(s), "%s!%s", ml->nick, ml->userhost);
      ml->user = get_user_by_host(s);
      ml->tried_getuser = 1;
    }

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

  /* first scan for bots on my server, ask first found for ops */
  simple_snprintf(s, sizeof(s), "gi o %s %s", chan->dname, botname);

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
  char* botops[MAX_BOTS];

  for (tand_t* bot = tandbot; bot && (foundBots < MAX_BOTS); bot = bot->next) {
    if (bot->hub || !bot->u)
      continue;

    get_user_flagrec(bot->u, &fr, chan->dname, chan);
    if (bot_shouldjoin(bot->u, &fr, chan) && chk_op(fr, chan))
      botops[foundBots++] = bot->bot;
  }

  if (!foundBots) {
    putlog(LOG_GETIN, "*", "No bots available, can't request help to join %s", chan->dname);
    return;
  }

  int cnt = foundBots < in_bots ? foundBots : in_bots;
  char s[255] = "";
  char l[1024] = "";
  size_t len = 0;

  simple_snprintf(s, sizeof(s), "gi i %s %s %s!%s %s", chan->dname, botname, botname, botuserhost, botuserip);

  shuffleArray(botops, foundBots);
  for (int n = 0; n < cnt; ++n) {
    putbot(botops[n], s);

    if (l[0])
      len += strlcpy(l + len, ", ", sizeof(l) - len);
    len += strlcpy(l + len, botops[n], sizeof(l) - len);
  }
  l[len] = 0;
  putlog(LOG_GETIN, "*", "Requested help to join %s from %s", chan->dname, l);
}


#ifdef REVENGE
/* Contains the logic to decide wether we want to punish someone. Returns
 * true (1) if we want to, false (0) if not.
 */
static bool
want_to_revenge(struct chanset_t *chan, struct userrec *u,
                struct userrec *u2, char *badnick, char *victimstr, bool mevictim)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };

  /* Do not take revenge upon ourselves. */
  if (match_my_nick(badnick))
    return 0;

  get_user_flagrec(u, &fr, chan->dname, chan);

  /* Kickee didn't kick themself? */
  if (rfc_casecmp(badnick, victimstr)) {
    /* They kicked me? */
    if (mevictim) {
      /* ... and I'm allowed to take revenge? <snicker> */
      if (channel_revengebot(chan))
        return 1;
      /* Do we revenge for our users ... and do we actually know the victim? */
    } else if (channel_revenge(chan) && u2) {
      struct flag_record fr2 = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };

      get_user_flagrec(u2, &fr2, chan->dname, chan);
      /* Protecting friends? */
      /* Protecting ops? */
      if ((channel_protectops(chan) && chk_op(fr2, chan))
        return 1;
    }
  }
  return 0;
}

/* Dependant on revenge_mode, punish the offender.
 */
static void
punish_badguy(struct chanset_t *chan, char *whobad,
              struct userrec *u, char *badnick, char *victimstr, bool mevictim, int type)
{
  memberlist *m = ismember(chan, badnick);

  if (!m)
    return;

  char reason[1024] = "", ct[81] = "", *kick_msg = NULL;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };

  get_user_flagrec(u, &fr, chan->dname, chan);

  /* Get current time into a string */
  strftime(ct, sizeof ct, "%d %b %Z", gmtime(&now));

  /* Put together log and kick messages */
  reason[0] = 0;
  switch (type) {
    case REVENGE_KICK:
      kick_msg = "don't kick my friends, bud";
      simple_snprintf(reason, sizeof(reason), "kicked %s off %s", victimstr, chan->dname);
      break;
    case REVENGE_DEOP:
      simple_snprintf(reason, sizeof(reason), "deopped %s on %s", victimstr, chan->dname);
      kick_msg = "don't deop my friends, bud";
      break;
    default:
      kick_msg = "revenge!";
  }
  putlog(LOG_MISC, chan->dname, "Punishing %s (%s)", badnick, reason);

  /* Set the offender +d */
  if ((chan->revenge_mode > 0) &&
      /* ... unless there's no more to do */
      !(chan_deop(fr) || glob_deop(fr))) {
    char s[UHOSTLEN], s1[UHOSTLEN];
    memberlist *mx = NULL;

    /* Removing op */
    if (chk_op(fr, chan)) {
      fr.match = FR_CHAN;
      if (chan_op(fr)) {
        fr.chan &= ~USER_OP;
      } else {
        fr.chan |= USER_DEOP;
      }
      set_user_flagrec(u, &fr, chan->dname);
      putlog(LOG_MISC, "*", "No longer opping %s[%s] (%s)", u->handle, whobad, reason);
    }
    /* ... or just setting to deop */
    else if (u) {
      /* In the user list already, cool :) */
      fr.match = FR_CHAN;
      fr.chan |= USER_DEOP;
      set_user_flagrec(u, &fr, chan->dname);
      simple_snprintf(s, sizeof(s), "(%s) %s", ct, reason);
      putlog(LOG_MISC, "*", "Now deopping %s[%s] (%s)", u->handle, whobad, s);
    }
    /* ... or creating new user and setting that to deop */
    else {
      strlcpy(s1, whobad, sizeof(s1));
      maskhost(s1, s);
      strlcpy(s1, badnick, sizeof(s1));
      /* If that handle exists use "badX" (where X is an increasing number)
       * instead.
       */
      while (get_user_by_handle(userlist, s1)) {
        if (!strncmp(s1, "bad", 3)) {
          int i;

          i = atoi(s1 + 3);
          simple_snprintf(s1 + 3, sizeof(s1) - 3, "%d", i + 1);
        } else
          strlcpy(s1, "bad1", sizeof(s1));   /* Start with '1' */
      }
      userlist = adduser(userlist, s1, s, "-", 0, 0);
      fr.match = FR_CHAN;
      fr.chan = USER_DEOP;
      u = get_user_by_handle(userlist, s1);
      if ((mx = ismember(chan, badnick)))
        mx->user = u;
      set_user_flagrec(u, &fr, chan->dname);
      simple_snprintf(s, sizeof(s), "(%s) %s (%s)", ct, reason, whobad);
      set_user(&USERENTRY_COMMENT, u, (void *) s);
      putlog(LOG_MISC, "*", "Now deopping %s (%s)", whobad, reason);
    }
  }

  /* Always try to deop the offender */
  if (!mevictim)
    add_mode(chan, '-', 'o', badnick);
  /* Ban. Should be done before kicking. */
  if (chan->revenge_mode > 2) {
    char s[UHOSTLEN] = "", s1[UHOSTLEN] = "";

    splitnick(&whobad);
    maskhost(whobad, s1);
    simple_snprintf(s, sizeof(s), "(%s) %s", ct, reason);
    u_addmask('b', chan, s1, conf.bot->nick, s, now + (60 * chan->ban_time), 0);
    if (!mevictim && me_op(chan)) {
      add_mode(chan, '+', 'b', s1);
      flush_mode(chan, QUICK);
    }
  }
  /* Kick the offender */
  if ((chan->revenge_mode > 1) && !chan_sentkick(m) &&
      /* ... and can I actually do anything about it? */
      me_op(chan) && !mevictim) {
    dprintf(DP_MODE, "KICK %s %s :%s%s\n", chan->name, m->nick, bankickprefix, response(RES_MEAN));
    m->flags |= SENTKICK;
  }
}

/* Punishes bad guys under certain circumstances using methods as defined
 * by the revenge_mode flag.
 */
static void
maybe_revenge(struct chanset_t *chan, char *whobad, char *whovictim, int type)
{
  if (!chan || (type < 0))
    return;

  /* Get info about offender */
  struct userrec *u = get_user_by_host(whobad), *u2 = NULL;
  char *badnick = splitnick(&whobad), *victimstr = NULL;
  bool mevictim;

  /* Get info about victim */
  u2 = get_user_by_host(whovictim);
  victimstr = splitnick(&whovictim);
  mevictim = match_my_nick(victimstr);

  /* Do we want to revenge? */
  if (!want_to_revenge(chan, u, u2, badnick, victimstr, mevictim))
    return;                     /* No, leave them alone ... */

  /* Haha! Do the vengeful thing ... */
  punish_badguy(chan, whobad, u, badnick, victimstr, mevictim, type);
}
#endif

/* Set the key.
 */
static void
my_setkey(struct chanset_t *chan, char *k)
{
  free(chan->channel.key);
  if (k == NULL) {
    chan->channel.key = (char *) my_calloc(1, 1);
    return;
  }
  chan->channel.key = strdup(k);
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

  m->next = (masklist *) my_calloc(1, sizeof(masklist));
  m->next->next = NULL;
  m->next->mask = (char *) my_calloc(1, 1);
  free(m->mask);
  m->mask = strdup(s);
  m->who = strdup(who);
  m->timer = now;
  return 0;
}

/* Removes a nick from the channel member list (returns 1 if successful)
 */
static bool
killmember(struct chanset_t *chan, char *nick)
{
  memberlist *x = NULL, *old = NULL;

  for (x = chan->channel.member; x && x->nick[0]; old = x, x = x->next)
    if (!rfc_casecmp(x->nick, nick))
      break;
  if (!x || !x->nick[0]) {
    if (!channel_pending(chan))
      putlog(LOG_MISC, "*", "(!) killmember(%s, %s) -> nonexistent", chan->dname, nick);
    return 0;
  }
  if (old)
    old->next = x->next;
  else
    chan->channel.member = x->next;
  free(x);
  chan->channel.members--;

  /* The following two errors should NEVER happen. We will try to correct
   * them though, to keep the bot from crashing.
   */
  if (chan->channel.members < 0) {
    chan->channel.members = 0;
    for (x = chan->channel.member; x && x->nick[0]; x = x->next)
      chan->channel.members++;
    putlog(LOG_MISC, "*", "(!) actually I know of %d members.", chan->channel.members);
  }
  if (!chan->channel.member) {
    chan->channel.member = (memberlist *) my_calloc(1, sizeof(memberlist));
    chan->channel.member->nick[0] = 0;
    chan->channel.member->next = NULL;
  }
  return 1;
}

/* Check if I am a chanop. Returns boolean 1 or 0.
 */
bool
me_op(struct chanset_t *chan)
{
  memberlist *mx = ismember(chan, botname);

  if (!mx)
    return 0;
  if (chan_hasop(mx))
    return 1;
  else
    return 0;
}

/* Check whether I'm voice. Returns boolean 1 or 0.
 */
static bool
me_voice(struct chanset_t *chan)
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

/* Reset the channel information.
 */
void
reset_chan_info(struct chanset_t *chan)
{
  if (!chan) return;

  if (!chan->name[0])
    strlcpy(chan->name, chan->dname, 81);

  /* Don't reset the channel if we're already resetting it */
  if (!shouldjoin(chan)) {
    dprintf(DP_MODE, "PART %s\n", chan->name);
    return;
  }

  if (!channel_pending(chan)) {
    bool opped = me_op(chan) ? 1 : 0;

    free(chan->channel.key);
    chan->channel.key = (char *) my_calloc(1, 1);
    clear_channel(chan, 1);
    chan->status |= CHAN_PEND;
    chan->status &= ~(CHAN_ACTIVE | CHAN_ASKEDMODES | CHAN_JOINING);
    /* don't bother checking bans if it's +take */
    if (!channel_take(chan)) {
      if (opped) {
        if (!(chan->status & CHAN_ASKEDBANS)) {
          chan->status |= CHAN_ASKEDBANS;
          dprintf(DP_MODE, "MODE %s +b\n", chan->name);
        }

        if (do_eI) {
          chan->channel.last_eI = now;
          if (!(chan->ircnet_status & CHAN_ASKED_EXEMPTS) && use_exempts == 1) {
            chan->ircnet_status |= CHAN_ASKED_EXEMPTS;
            dprintf(DP_MODE, "MODE %s +e\n", chan->name);
          }
          if (!(chan->ircnet_status & CHAN_ASKED_INVITES) && use_invites == 1) {
            chan->ircnet_status |= CHAN_ASKED_INVITES;
            dprintf(DP_MODE, "MODE %s +I\n", chan->name);
          }
        }
      }
    }
    /* These 2 need to get out asap, so into the mode queue */
    dprintf(DP_MODE, "MODE %s\n", chan->name);
    send_chan_who(DP_MODE, chan);
    /* clear_channel nuked the data...so */
    dprintf(DP_MODE, "TOPIC %s\n", chan->name);
  }
}

static void send_chan_who(int queue, struct chanset_t *chan) {
    if (use_354) /* Added benefit of getting numeric IP! :) */
      dprintf(queue, "WHO %s %%c%%h%%n%%u%%f%%r%%d%%i\n", chan->name);
    else
      dprintf(queue, "WHO %s\n", chan->name);
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

  memberlist *m = NULL;
  char s[UHOSTLEN] = "";
  int i = 0;
  static int whined = 0;

  /* Count non-split channel members */
  for (m = chan->channel.member; m && m->nick[0]; m = m->next)
    if (!chan_issplit(m))
      i++;
  if (i == 1 && channel_cycle(chan) && !channel_stop_cycle(chan)) {
    if (chan->name[0] != '+') { /* Its pointless to cycle + chans for ops */
      putlog(LOG_MISC, "*", "Trying to cycle %s to regain ops.", chan->dname);
      dprintf(DP_MODE, "PART %s\n", chan->name);
      /* If it's a !chan, we need to recreate the channel with !!chan <cybah> */
      dprintf(DP_MODE, "JOIN %s%s %s\n", (chan->dname[0] == '!') ? "!" : "", chan->dname, chan->key_prot);
      chan->status |= CHAN_JOINING;
      whined = 0;
    }
  } else if (any_ops(chan)) {
    whined = 0;
    request_op(chan);
/* need: op */
  } else {
    /* Other people here, but none are ops. If there are other bots make
     * them LEAVE!
     */
    bool ok = 1;
    struct userrec *u = NULL;

    if (!whined) {
      /* + is opless. Complaining about no ops when without special
       * help(services), we cant get them - Raist
       */
      if (chan->name[0] != '+')
        putlog(LOG_MISC, "*", "%s is active but has no ops :(", chan->dname);
      whined = 1;
    }
    for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
      simple_snprintf(s, sizeof(s), "%s!%s", m->nick, m->userhost);
      u = get_user_by_host(s);
      if (!match_my_nick(m->nick) && (!u || !u->bot)) {
        ok = 0;
        break;
      }
    }
    if (ok && channel_cycle(chan)) {
      /* ALL bots!  make them LEAVE!!! */
/*
      for (m = chan->channel.member; m && m->nick[0]; m = m->next)
	if (!match_my_nick(m->nick))
	  dprintf(DP_SERVER, "PRIVMSG %s :go %s\n", m->nick, chan->dname);
*/
    } else {
      /* Some humans on channel, but still op-less */
      request_op(chan);
/* need: op */
    }
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
    if (!match_my_nick(m->nick) && chan_hasop(m) && (m->hops == -1)) {
      putlog(LOG_DEBUG, "*", "Updating WHO for '%s' because '%s' is missing data.", chan->dname, m->nick);
      send_chan_who(DP_HELP, chan);
      break;                    /* lets just do one chan at a time to save from flooding */
    }
  }
}

static void
check_netfight(struct chanset_t *chan)
{
  if (fight_threshold) {
    if ((chan->channel.fighting) && (chan->channel.fighting > fight_threshold)) {
      if (!chan_bitch(chan) || !channel_closed(chan)) {
        putlog(LOG_WARN, "*", "Auto-closed %s - channel fight", chan->dname);
        do_chanset(NULL, chan, "+bitch +closed +backup", DO_LOCAL | DO_NET);
        enforce_closed(chan);
        dprintf(DP_MODE, "TOPIC %s :Auto-closed - channel fight\n", chan->name);
      }
    }
  }
  chan->channel.fighting = 0;   /* we put this here because we need to clear it once per min */
}

void
raise_limit(struct chanset_t *chan)
{
  if (!chan || !me_op(chan))
    return;

  /* Don't bother setting limit if the user has set a protect -l */
  if (chan->mode_mns_prot & CHANLIMIT)
    return;

  int nl = chan->channel.members + chan->limitraise;	/* new limit */
  int i = chan->limitraise >> 2;			/* DIV 4 */
  /* if the newlimit will be in the range made by these vars, dont change. */
  int ul = nl + i;					/* upper limit */
  int ll = nl - i;					/* lower limit */

  if ((chan->channel.maxmembers > ll) && (chan->channel.maxmembers < ul))
    return;                     /* the current limit is in the range, so leave it. */

  if (nl != chan->channel.maxmembers) {
    char s[6] = "";

    simple_snprintf(s, sizeof(s), "%d", nl);
    add_mode(chan, '+', 'l', s);
  }

}

static void
check_expired_chanstuff(struct chanset_t *chan)
{
  if ((channel_active(chan) || channel_pending(chan)) && !shouldjoin(chan)) {
    dprintf(DP_MODE, "PART %s\n", chan->name[0] ? chan->name : chan->dname);
  } else if (channel_active(chan)) {
    masklist *b = NULL, *e = NULL;
    memberlist *m = NULL, *n = NULL;
    char s[UHOSTLEN] = "";
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

//    for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
    for (m = chan->channel.member; m && m->nick[0]; m = n) {
      n = m->next;
      if (m->split && now - m->split > wait_split) {
        simple_snprintf(s, sizeof(s), "%s!%s", m->nick, m->userhost);
        putlog(LOG_JOIN, chan->dname, "%s (%s) got lost in the net-split.", m->nick, m->userhost);
        killmember(chan, m->nick);
        continue;
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

      if (me_op(chan)) {
        if (dovoice(chan) && !loading) {      /* autovoice of +v users if bot is +y */
          if (!chan_hasop(m) && !chan_hasvoice(m)) {
            if (!m->user && !m->tried_getuser) {
              simple_snprintf(s, sizeof(s), "%s!%s", m->nick, m->userhost);
              m->user = get_user_by_host(s);
              if (!m->user && m->userip[0]) {
                simple_snprintf(s, sizeof(s), "%s!%s", m->nick, m->userip);
                m->user = get_user_by_host(s);
              }
              m->tried_getuser = 1;
            }

            if (m->user) {
              get_user_flagrec(m->user, &fr, chan->dname, chan);
              if (!glob_bot(fr)) {
                if (!chan_sentvoice(m) && !(m->flags & EVOICE) && 
                    (
                     (channel_voice(chan) && !chk_devoice(fr)) ||
                     (!channel_voice(chan) && !privchan(fr, chan, PRIV_VOICE) && chk_voice(fr, chan))
                    )
                   ) {
                  add_mode(chan, '+', 'v', m->nick);
                } else if ((chk_devoice(fr) || (m->flags & EVOICE))) {
                  add_mode(chan, '-', 'v', m->nick);
                }
              }
            } else if (!chan_sentvoice(m) && !m->user && channel_voice(chan) && voice_ok(m, chan)) {
              add_mode(chan, '+', 'v', m->nick);
            }
          }
        }
      }
      m = n;
    }
    check_lonely_channel(chan);
  } else if (shouldjoin(chan) && !channel_pending(chan)) {
    dprintf(DP_MODE, "JOIN %s %s\n",
            (chan->name[0]) ? chan->name : chan->dname,
            chan->channel.key[0] ? chan->channel.key : chan->key_prot);
    chan->status |= CHAN_JOINING;
  }
}

void
irc_minutely()
{
  for (register struct chanset_t *chan = chanset; chan; chan = chan->next) {
    warn_pls_take(chan);
    if (server_online) {
      check_netfight(chan);
      check_servers(chan);
      check_expired_chanstuff(chan);
    }
  }
}


int check_bind_authc(char *cmd, Auth *auth, char *chname, char *args)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  int x = 0;

  get_user_flagrec(auth->user, &fr, chname);
  x = check_bind(BT_msgc, cmd, &fr, auth, chname, args);


  if (x & BIND_RET_LOG) {
    if (chname)
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! %s %c%s %s", auth->nick, auth->host, 
                            auth->handle, chname, auth_prefix[0], cmd, args);
    else
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! %c%s %s", auth->nick, auth->host, auth->handle, auth_prefix[0], cmd, args);
  }

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

  for (register struct chanset_t *chan = chanset; chan; chan = chan->next) {
    for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
      if (m->delay && m->delay <= now) {
        m->delay = 0L;
        m->flags &= ~FULL_DELAY;
        if (chan_sentop(m)) {
          m->flags &= ~SENTOP;
          do_op(m->nick, chan, 0, 0);
        }
        if (chan_sentvoice(m)) {
          m->flags &= ~SENTVOICE;
          add_mode(chan, '+', 'v', m->nick);
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
        if (chan->status & CHAN_JUPED)
          p = "juped";
        else if (channel_joining(chan))
          p = "joining";
        else if (!(chan->status & CHAN_ACTIVE))
          p = "trying";
        else if (chan->status & CHAN_PEND)
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

static void
do_nettype()
{
  switch (net_type) {
    case 0:                    /* Efnet */
      include_lk = 0;
      break;
    case 1:                    /* Ircnet */
      include_lk = 1;
      break;
    case 2:                    /* Undernet */
      include_lk = 1;
      break;
    case 3:                    /* Dalnet */
      include_lk = 1;
      break;
    case 4:                    /* hybrid-6+ */
      include_lk = 0;
      break;
    default:
      break;
  }
}

static cmd_t irc_bot[] = {
  {"gi", "", (Function) getin_request, NULL, LEAF},
  {"mr", "", (Function) mass_request, NULL, LEAF},
  {NULL, NULL, NULL, NULL, 0}
};

static void
getin_5secondly()
{
  if (!server_online)
    return;

  for (register struct chanset_t *chan = chanset; chan; chan = chan->next) {
    if ((!channel_pending(chan) && channel_active(chan)) && !me_op(chan))
      request_op(chan);
  }
}

void
irc_init()
{
  timer_create_secs(60, "irc_minutely", (Function) irc_minutely);
  timer_create_secs(5, "getin_5secondly", (Function) getin_5secondly);

  /* Add our commands to the imported tables. */
  add_builtins("dcc", irc_dcc);
  add_builtins("bot", irc_bot);
  add_builtins("raw", irc_raw);
  add_builtins("msg", C_msg);
  add_builtins("msgc", C_msgc);

  do_nettype();

  bot_counters = hash_table_create(NULL, NULL, 100, HASH_TABLE_STRINGS);
}
