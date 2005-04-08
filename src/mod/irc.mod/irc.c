/*
 * irc.c -- part of irc.mod
 *   support for channels within the bot
 *
 */


#include "src/common.h"
#define MAKING_IRC
#include "irc.h"
#include "src/match.h"
#include "src/settings.h"
#include "src/tandem.h"
#include "src/net.h"
#include "src/botnet.h"
#include "src/botmsg.h"
#include "src/main.h"
#include "src/response.h"
#include "src/cfg.h"
#include "src/userrec.h"
#include "src/misc.h"
#include "src/rfc1459.h"
#include "src/chanprog.h"
#include "src/auth.h"
#include "src/userrec.h"
#include "src/tclhash.h"
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
static bool keepnick = 1;        /* Keep nick */
static bool prevent_mixing = 1;  /* To prevent mixing old/new modes */
static bool include_lk = 1;      /* For correct calculation
                                 * in real_add_mode. */

#include "chan.c"
#include "mode.c"
#include "cmdsirc.c"
#include "msgcmds.c"

static void
detect_autokick(char *nick, char *uhost, struct chanset_t *chan, char *msg)
{
  if (!nick || !nick[0] || !uhost || !uhost[0] || !chan || !msg || !msg[0])
    return;

  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  struct userrec *u = get_user_by_host(uhost);
  int i = 0;
  //size_t tot = strlen(msg);

  get_user_flagrec(u, &fr, chan->dname);

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

void notice_invite(struct chanset_t *chan, char *handle, char *nick, char *uhost, bool op) {
  char fhandle[21] = "";
  const char *ops = " (auto-op)";

  if (handle)
    simple_sprintf(fhandle, "\002%s\002 ", handle);
  putlog(LOG_MISC, "*", "Invited %s%s(%s%s%s) to %s.", handle ? handle : "", handle ? " " : "", nick, uhost ? "!" : "", uhost ? uhost : "", chan->dname);
  dprintf(DP_MODE, "PRIVMSG %s :\001ACTION has invited %s(%s%s%s) to %s.%s\001\n",
    chan->name, fhandle, nick, uhost ? "!" : "", uhost ? uhost : "", chan->dname, op ? ops : "");
}

#ifdef CACHE
static cache_t *cache_new(char *nick)
{
  cache_t *cache = (cache_t *) my_calloc(1, sizeof(cache_t));

  cache->next = NULL;
  strcpy(cache->nick, nick);
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
  strcpy(cchan->dname, chname);
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
  if (host && cache->uhost[0] && egg_strcasecmp(cache->uhost, host))
    cache->uhost[0] = 0;

  if (host && !cache->uhost[0])
    strcpy(cache->uhost, host);

  /* if we find they have a handle but it doesnt match the new handle, wipe it */
  if (handle && cache->handle[0] && egg_strcasecmp(cache->handle, handle))
    cache->handle[0] = 0;

  if (handle && !cache->handle[0])
    strcpy(cache->handle, handle);

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

static char *
makecookie(char *chn, char *bnick)
{
  char *buf = NULL, randstring[5] = "", ts[11] = "", *chname = NULL, *hash = NULL, tohash[50] = "";

  chname = strdup(chn);

  make_rand_str(randstring, 4);

  /* &ts[4] is now last 6 digits of time */
  sprintf(ts, "%010li", now + timesync);
  
  /* Only use first 3 chars of chan */
  if (strlen(chname) > 2)
    chname[3] = 0;
  strtoupper(chname);

  simple_sprintf(tohash, "%c%s%s%s%s%c", settings.salt2[0], bnick, chname, &ts[4], randstring, settings.salt2[15]);
  hash = MD5(tohash);
  buf = (char *) my_calloc(1, 20);
  simple_sprintf(buf, "%c%c%c!%s@%s", hash[8], hash[16], hash[18], randstring, ts);
  /* sprintf(buf, "%c/%s!%s@%X", hash[16], randstring, ts, BIT31); */
  free(chname);
  return buf;
}

static int
checkcookie(char *chn, char *bnick, char *cookie)
{
  char randstring[5] = "", ts[11] = "", *chname = NULL, *hash = NULL, tohash[50] = "", *p = NULL;
  time_t optime = 0;

  chname = strdup(chn);
  p = cookie;
  p += 4; /* has! */
  strlcpy(randstring, p, sizeof(randstring));
  p += 5; /* rand@ */
  /* &ts[4] is now last 6 digits of time */
  strlcpy(ts, p, sizeof(ts));
  optime = atol(ts);

  /* Only use first 3 chars of chan */
  if (strlen(chname) > 2)
    chname[3] = 0;
  strtoupper(chname);
  /* hash!rand@ts */
  simple_sprintf(tohash, "%c%s%s%s%s%c", settings.salt2[0], bnick, chname, &ts[4], randstring, settings.salt2[15]);
  hash = MD5(tohash);
  if (!(hash[8] == cookie[0] && hash[16] == cookie[1] && hash[18] == cookie[2]))
    return BC_HASH;
  if (((now + timesync) - optime) > 600)
    return BC_SLACK;
  return 0;
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

  struct chanset_t *chan = NULL;
  char *what = newsplit(&par), *chname = newsplit(&par);

  if (!chname[0] || !chname)
    return;

  if (!(chan = findchan_by_dname(chname))) {
    putlog(LOG_GETIN, "*", "getin req from %s for %s which is not a valid channel!", botnick, chname);
    return;
  }

  char *tmp = newsplit(&par);		/* nick */

  if (!tmp[0])
    return;


  struct userrec *u = NULL;
  memberlist *mem = NULL;
  char nick[NICKLEN] = "", uhost[UHOSTLEN] = "", uip[UHOSTLEN] = "";
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };

  strlcpy(nick, tmp, sizeof(nick));

  tmp = newsplit(&par);			/* userhost */

  if (tmp[0])
    strlcpy(uhost, tmp, sizeof(uhost));

  tmp = newsplit(&par);		/* userip */
  if (tmp[0])
    egg_snprintf(uip, sizeof uip, "%s!%s", nick, tmp);
    
  /*
   * if (!ismember(chan, botname)) {
   * putlog(LOG_GETIN, "*", "getin req from %s for %s - I'm not on %s!", botnick, chname, chname);
   * return;
   * }
   */

  const char *type = what[0] == 'o' ? "op" : "in", *desc = what[0] == 'o' ? "on" : "for";


  u = get_user_by_handle(userlist, botnick);
  if (!u) {
    putlog(LOG_GETIN, "*", "%sreq from %s/%s %s %s - No user called %s in userlist", type, botnick, nick, desc, 
                           chan->dname, botnick);
    return;
  }

  if (server_lag > LAG_THRESHOLD) {
    putlog(LOG_GETIN, "*", "%sreq from %s/%s %s %s - I'm too lagged", type, botnick, nick, desc, chan->dname);
    return;
  }
  if (what[0] != 'K') {
    if (!(channel_pending(chan) || channel_active(chan))) {
      putlog(LOG_GETIN, "*", "%sreq from %s/%s %s %s - I'm not on %s now", type, botnick, nick, desc, 
                             chan->dname, chan->dname);
      return;
    }
  }
  if (!chan) {
    putlog(LOG_GETIN, "*", "%sreq from %s/%s %s %s - Channel %s doesn't exist", type, botnick, nick, 
                            desc, chname, chname);
    return;
  }
  mem = ismember(chan, nick);

  if (mem && chan_issplit(mem)) {
    putlog(LOG_GETIN, "*", "%sreq from %s/%s %s %s - %s is split", type, botnick, nick, desc, chan->dname, nick);
    return;
  }

  if (what[0] == 'o') {
    if (!mem) {
      putlog(LOG_GETIN, "*", "opreq from %s/%s on %s - %s isn't on %s", botnick, nick, chan->dname, nick, chan->dname);
      return;
    }

    if (!mem->user && !mem->tried_getuser) {
      mem->user = get_user_by_host(uhost);
      mem->tried_getuser = 1;
    }

    if (mem->user != u) {
      putlog(LOG_GETIN, "*", "opreq from %s/%s on %s - %s doesn't match %s", botnick, nick, chan->dname, nick, botnick);
      return;
    }
    get_user_flagrec(u, &fr, chan->dname);

    if (!chk_op(fr, chan)) {
      putlog(LOG_GETIN, "*", "opreq from %s/%s on %s - %s doesnt have +o for chan.", botnick, nick, chan->dname, botnick);
      return;
    }
    if (chan_hasop(mem)) {
      putlog(LOG_GETIN, "*", "opreq from %s/%s on %s - %s already has ops", botnick, nick, chan->dname, nick);
      return;
    }
    if (!me_op(chan)) {
      putlog(LOG_GETIN, "*", "opreq from %s/%s on %s - I haven't got ops", botnick, nick, chan->dname);
      return;
    }
    if (chan_sentop(mem)) {
      putlog(LOG_GETIN, "*", "opreq from %s/%s on %s - Already sent a +o", botnick, nick, chan->dname);
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

#ifdef no
    if (mem->user != u) {
      putlog(LOG_GETIN, "*", "inreq from %s/%s for %s - %s doesn't match %s", botnick, nick, chan->dname, nick, botnick);
      return;
    }
#endif
    get_user_flagrec(u, &fr, chan->dname);
    if (!chk_op(fr, chan) || chan_kick(fr) || glob_kick(fr)) {
      putlog(LOG_GETIN, "*", "inreq from %s/%s for %s - %s doesn't have acces for chan.", botnick, nick, chan->dname, botnick);
      return;
    }

    char s[256] = "", *p = NULL, *p2 = NULL, *p3 = NULL;
    bool sendi = 0;

    strcpy(s, getchanmode(chan));
    p = (char *) &s;
    p2 = newsplit(&p);
    p3 = newsplit(&p);

    if (strchr(p2, 'l')) {
      if (!me_op(chan)) {
        putlog(LOG_GETIN, "*", "inreq from %s/%s for %s - I haven't got ops", botnick, nick, chan->dname);
        return;
      } else {
        int lim = chan->channel.members + 5, curlim;

        if (!*p)
          curlim = atoi(p3);
        else
          curlim = atoi(p);
        if (curlim > 0 && curlim < lim) {
          char s2[16] = "";

          sendi = 1;
          simple_sprintf(s2, "%d", lim);
          add_mode(chan, '+', 'l', s2);
          putlog(LOG_GETIN, "*", "inreq from %s/%s for %s - Raised limit to %d", botnick, nick, chan->dname, lim);
        }
      }
    }

    struct maskrec **mr = NULL, *tmr = NULL;

    mr = &global_bans;
    while (*mr) {
      if (wild_match((*mr)->mask, uhost) || 
          wild_match((*mr)->mask, uip) || 
          match_cidr((*mr)->mask, uip)) {
        if (!noshare) {
          shareout("-m b %s\n", (*mr)->mask);
        }
        putlog(LOG_GETIN, "*", "inreq from %s/%s for %s - Removed permanent global ban %s", botnick, nick,
               chan->dname, (*mr)->mask);
        /*gban_total--; */
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
    mr = &chan->bans;
    while (*mr) {
      if (wild_match((*mr)->mask, uhost) || 
          wild_match((*mr)->mask, uip) || 
          match_cidr((*mr)->mask, uip)) {
        if (!noshare) {
          shareout("-mc b %s %s\n", chan->dname, (*mr)->mask);
        }
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
    if (strchr(p2, 'k')) {
      sendi = 0;
      tmp = (char *) my_calloc(1, strlen(chan->dname) + strlen(p3) + 7);
      simple_sprintf(tmp, "gi K %s %s", chan->dname, p3);
      putbot(botnick, tmp);
      putlog(LOG_GETIN, "*", "inreq from %s/%s for %s - Sent key (%s)", botnick, nick, chan->dname, p3);
      free(tmp);
    }
    if (strchr(p2, 'i')) {
      if (!me_op(chan)) {
        putlog(LOG_GETIN, "*", "inreq from %s/%s for %s - I haven't got ops", botnick, nick, chan->dname);
        return;
      } else {
        sendi = 1;
        putlog(LOG_GETIN, "*", "inreq from %s/%s for %s - Invited", botnick, nick, chan->dname);
      }
    }
    if (sendi && me_op(chan))
      dprintf(DP_MODE, "INVITE %s %s\n", nick, chan->name);
  } else if (what[0] == 'K') {
    if (!chan) {
      putlog(LOG_GETIN, "*", "Got key for nonexistant channel %s from %s", chname, botnick);
      return;
    }
    if (!shouldjoin(chan)) {
      putlog(LOG_GETIN, "*", "Got key for %s from %s - I shouldn't be on that chan?!?", chan->dname, botnick);
    } else {
      if (!(channel_pending(chan) || channel_active(chan))) {
        putlog(LOG_GETIN, "*", "Got key for %s from %s (%s) - Joining", chan->dname, botnick, nick);
        dprintf(DP_MODE, "JOIN %s %s\n", chan->dname, nick);
        chan->status |= CHAN_JOINING;
      } else {
        putlog(LOG_GETIN, "*", "Got key for %s from %s - I'm already in the channel", chan->dname, botnick);
      }
    }
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
  if (server_lag > LAG_THRESHOLD) {
    putlog(LOG_GETIN, "*", "Not asking for ops on %s - I'm too lagged", chan->dname);
    return;
  }

  int i = 0, my_exp = 0, first = 100;
  time_t n = now;

  /* max OPREQ_COUNT requests per OPREQ_SECONDS sec */
  while (i < 5) {
    if (n - chan->opreqtime[i] > OPREQ_SECONDS) {
      if (first > i)
        first = i;
      my_exp++;
      chan->opreqtime[i] = 0;
    }
    i++;
  }
  if ((5 - my_exp) >= OPREQ_COUNT) {
    putlog(LOG_GETIN, "*", "Delaying opreq for %s - Maximum of %d:%d reached", chan->dname, OPREQ_COUNT,
           OPREQ_SECONDS);
    return;
  }
  int cnt = OP_BOTS, i2;
  memberlist *ml = NULL;
  memberlist *botops[MAX_BOTS];
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  char s[UHOSTLEN] = "";

  for (i = 0, ml = chan->channel.member; (i < MAX_BOTS) && ml && ml->nick[0]; ml = ml->next) {
    /* If bot, linked, global op & !split & chanop & (chan is reserver | bot isn't +a) -> 
     * add to temp list */
    if (!ml->user && !ml->tried_getuser) {
      simple_sprintf(s, "%s!%s", ml->nick, ml->userhost);
      ml->user = get_user_by_host(s);
      ml->tried_getuser = 1;
    }
    if ((i < MAX_BOTS) && ml->user && ml->user->bot && 
        findbot(ml->user->handle) && chan_hasop(ml) && !chan_issplit(ml)) {

      get_user_flagrec(ml->user, &fr, NULL);
      if (chk_op(fr, chan))
        botops[i++] = ml;
    }
  }
  if (!i) {
    if (channel_active(chan) && !channel_pending(chan))
      putlog(LOG_GETIN, "*", "No one to ask for ops on %s", chan->dname);
    return;
  }

  char *l = (char *) my_calloc(1, cnt * 50);

  /* first scan for bots on my server, ask first found for ops */
  simple_sprintf(s, "gi o %s %s", chan->dname, botname);

  /* look for bots 0-1 hops away */
  for (i2 = 0; i2 < i; i2++) {
    if (botops[i2]->hops < 2) {
      putbot(botops[i2]->user->handle, s);
      chan->opreqtime[first] = n;
      if (l[0]) {
        strcat(l, ", ");
        strcat(l, botops[i2]->user->handle);
      } else {
        strcpy(l, botops[i2]->user->handle);
      }
      strcat(l, "/");
      strcat(l, botops[i2]->nick);
      botops[i2] = NULL;
      cnt--;
      break;
    }
  }

  /* Pick random op and ask for ops */
  while (cnt) {
    i2 = randint(i);
    if (botops[i2]) {
      putbot(botops[i2]->user->handle, s);
      chan->opreqtime[first] = n;
      if (l[0]) {
        strcat(l, ", ");
        strcat(l, botops[i2]->user->handle);
      } else {
        strcpy(l, botops[i2]->user->handle);
      }
      strcat(l, "/");
      strcat(l, botops[i2]->nick);
      cnt--;
      botops[i2] = NULL;
    } else {
      if (i < OP_BOTS)
        cnt--;
    }
  }
  putlog(LOG_GETIN, "*", "Requested ops on %s from %s", chan->dname, l);
  free(l);
}

static void
request_in(struct chanset_t *chan)
{
  int i = 0;
  struct userrec *botops[MAX_BOTS];
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0 };

  for (struct userrec *u = userlist; u && (i < MAX_BOTS); u = u->next) {
    get_user_flagrec(u, &fr, NULL);
    if (bot_hublevel(u) == 999 && glob_bot(fr) && chk_op(fr, chan)
#ifdef G_BACKUP
        && (!glob_backupbot(fr) || channel_backup(chan))
#endif
        /* G_BACKUP */
        && nextbot(u->handle) >= 0)
      botops[i++] = u;
  }
  if (!i) {
    putlog(LOG_GETIN, "*", "No bots linked, can't request help to join %s", chan->dname);
    return;
  }

  char s[255] = "", *l = (char *) my_calloc(1, IN_BOTS * 30);
  int cnt = IN_BOTS, n;

  simple_sprintf(s, "gi i %s %s %s!%s %s", chan->dname, botname, botname, botuserhost, botuserip);
  while (cnt) {
    n = randint(i);
    if (botops[n]) {
      putbot(botops[n]->handle, s);
      if (l[0]) {
        strcat(l, ", ");
        strcat(l, botops[n]->handle);
      } else {
        strcpy(l, botops[n]->handle);
      }
      botops[n] = NULL;
      cnt--;
    } else {
      if (i < IN_BOTS)
        cnt--;
    }
  }
  putlog(LOG_GETIN, "*", "Requesting help to join %s from %s", chan->dname, l);
  free(l);
}


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

  get_user_flagrec(u, &fr, chan->dname);

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

      get_user_flagrec(u2, &fr2, chan->dname);
      /* Protecting friends? */
      /* Protecting ops? */
      if ((channel_protectops(chan) &&
           /* ... and kicked is valid op? */
           (chan_op(fr2) || (glob_op(fr2) && !chan_deop(fr2)))))
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

  get_user_flagrec(u, &fr, chan->dname);

  /* Get current time into a string */
  egg_strftime(ct, sizeof ct, "%d %b %Z", gmtime(&now));

  /* Put together log and kick messages */
  reason[0] = 0;
  switch (type) {
    case REVENGE_KICK:
      kick_msg = "don't kick my friends, bud";
      simple_sprintf(reason, "kicked %s off %s", victimstr, chan->dname);
      break;
    case REVENGE_DEOP:
      simple_sprintf(reason, "deopped %s on %s", victimstr, chan->dname);
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
    if (chan_op(fr) || (glob_op(fr) && !chan_deop(fr))) {
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
      simple_sprintf(s, "(%s) %s", ct, reason);
      putlog(LOG_MISC, "*", "Now deopping %s[%s] (%s)", u->handle, whobad, s);
    }
    /* ... or creating new user and setting that to deop */
    else {
      strcpy(s1, whobad);
      maskhost(s1, s);
      strcpy(s1, badnick);
      /* If that handle exists use "badX" (where X is an increasing number)
       * instead.
       */
      while (get_user_by_handle(userlist, s1)) {
        if (!strncmp(s1, "bad", 3)) {
          int i;

          i = atoi(s1 + 3);
          simple_sprintf(s1 + 3, "%d", i + 1);
        } else
          strcpy(s1, "bad1");   /* Start with '1' */
      }
      userlist = adduser(userlist, s1, s, "-", 0, 0);
      fr.match = FR_CHAN;
      fr.chan = USER_DEOP;
      u = get_user_by_handle(userlist, s1);
      if ((mx = ismember(chan, badnick)))
        mx->user = u;
      set_user_flagrec(u, &fr, chan->dname);
      simple_sprintf(s, "(%s) %s (%s)", ct, reason, whobad);
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
    simple_sprintf(s, "(%s) %s", ct, reason);
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
  chan->channel.key = (char *) my_calloc(1, strlen(k) + 1);
  strcpy(chan->channel.key, k);
}

/* Adds a ban, exempt or invite mask to the list
 * m should be chan->channel.(exempt|invite|ban)
 */
static void
new_mask(masklist *m, char *s, char *who)
{
  for (; m && m->mask[0] && rfc_casecmp(m->mask, s); m = m->next) ;
  if (m->mask[0])
    return;                     /* Already existent mask */

  m->next = (masklist *) my_calloc(1, sizeof(masklist));
  m->next->next = NULL;
  m->next->mask = (char *) my_calloc(1, 1);
  free(m->mask);
  m->mask = (char *) my_calloc(1, strlen(s) + 1);
  strcpy(m->mask, s);
  m->who = (char *) my_calloc(1, strlen(who) + 1);
  strcpy(m->who, who);
  m->timer = now;
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
static void
reset_chan_info(struct chanset_t *chan)
{
  /* Don't reset the channel if we're already resetting it */
  if (!shouldjoin(chan)) {
    dprintf(DP_MODE, "PART %s\n", chan->name);
    return;
  }

  if (!channel_pending(chan)) {
    bool opped = 0;

    if (me_op(chan))
      opped = 1;
    free(chan->channel.key);
    chan->channel.key = (char *) my_calloc(1, 1);
    clear_channel(chan, 1);
    chan->status |= CHAN_PEND;
    chan->status &= ~(CHAN_ACTIVE | CHAN_ASKEDMODES | CHAN_JOINING);
    if (!(chan->status & CHAN_ASKEDBANS)) {
      chan->status |= CHAN_ASKEDBANS;
      dprintf(DP_MODE, "MODE %s +b\n", chan->name);
    }
    if (opped && !channel_take(chan)) {
      if (!(chan->ircnet_status & CHAN_ASKED_EXEMPTS) && use_exempts == 1) {
        chan->ircnet_status |= CHAN_ASKED_EXEMPTS;
        dprintf(DP_MODE, "MODE %s +e\n", chan->name);
      }
      if (!(chan->ircnet_status & CHAN_ASKED_INVITES) && use_invites == 1) {
        chan->ircnet_status |= CHAN_ASKED_INVITES;
        dprintf(DP_MODE, "MODE %s +I\n", chan->name);
      }
    }
    /* These 2 need to get out asap, so into the mode queue */
    dprintf(DP_MODE, "MODE %s\n", chan->name);
    if (use_354)
      dprintf(DP_MODE, "WHO %s %%c%%h%%n%%u%%f\n", chan->name);
    else
      dprintf(DP_MODE, "WHO %s\n", chan->name);
    /* clear_channel nuked the data...so */
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
      simple_sprintf(s, "%s!%s", m->nick, m->userhost);
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
      dprintf(DP_HELP, "WHO %s\n", chan->name);
      break;                    /* lets just do one chan at a time to save from flooding */
    }
  }
}

static void
check_netfight(struct chanset_t *chan)
{
  int limit = atoi(CFG_FIGHTTHRESHOLD.gdata ? CFG_FIGHTTHRESHOLD.gdata : "0");

  if (limit) {
    if ((chan->channel.fighting) && (chan->channel.fighting > limit)) {
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

  int nl = chan->channel.members + chan->limitraise;	/* new limit */
  int i = chan->limitraise >> 2;			/* DIV 4 */
  /* if the newlimit will be in the range made by these vars, dont change. */
  int ul = nl + i;					/* upper limit */
  int ll = nl - i;					/* lower limit */

  if ((chan->channel.maxmembers > ll) && (chan->channel.maxmembers < ul))
    return;                     /* the current limit is in the range, so leave it. */

  if (nl != chan->channel.maxmembers) {
    char s[5] = "";

    simple_sprintf(s, "%d", nl);
    add_mode(chan, '+', 'l', s);
  }

}

static void
check_expired_chanstuff(struct chanset_t *chan)
{
  if (channel_active(chan)) {
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
        simple_sprintf(s, "%s!%s", m->nick, m->userhost);
        putlog(LOG_JOIN, chan->dname, "%s (%s) got lost in the net-split.", m->nick, m->userhost);
        killmember(chan, m->nick);
        continue;
      }

      if (me_op(chan)) {
        if (chan->idle_kick) {
          if (now - m->last >= chan->idle_kick * 60 && !match_my_nick(m->nick) && !chan_issplit(m)) {
            simple_sprintf(s, "%s!%s", m->nick, m->userhost);
            get_user_flagrec(m->user ? m->user : get_user_by_host(s), &fr, chan->dname);
            if (!(glob_bot(fr) || (glob_op(fr) && !glob_deop(fr)) || chan_op(fr))) {
              dprintf(DP_SERVER, "KICK %s %s :%sidle %d min\n", chan->name, m->nick, kickprefix, chan->idle_kick);
              m->flags |= SENTKICK;
            }
          }
        } else if (dovoice(chan) && !loading) {      /* autovoice of +v users if bot is +y */
          if (!chan_hasop(m) && !chan_hasvoice(m)) {
            if (!m->user && !m->tried_getuser) {
              simple_sprintf(s, "%s!%s", m->nick, m->userhost);
              m->user = get_user_by_host(s);
              m->tried_getuser = 1;
            }

            if (m->user) {
              get_user_flagrec(m->user, &fr, chan->dname);
              if (!glob_bot(fr)) {
                if (!(m->flags & EVOICE) && !privchan(fr, chan, PRIV_VOICE) &&
                   ((channel_voice(chan) && !chk_devoice(fr)) ||
                   (!channel_voice(chan) && chk_voice(fr, chan)))) {
                  add_mode(chan, '+', 'v', m->nick);
                } else if ((chk_devoice(fr) || (m->flags & EVOICE))) {
                  add_mode(chan, '-', 'v', m->nick);
                }
              }
            } else if (!m->user && !(m->flags & EVOICE) && channel_voice(chan)) {
              add_mode(chan, '+', 'v', m->nick);
            }
          }
        }
      }
      m = n;
    }
    check_lonely_channel(chan);
  } else if (shouldjoin(chan) && !channel_pending(chan) && !channel_joining(chan)) {
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
                            auth->handle, chname, cmdprefix, cmd, args);
    else
      putlog(LOG_CMDS, "*", "(%s!%s) !%s! %c%s %s", auth->nick, auth->host, auth->handle, cmdprefix, cmd, args);
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
  char ch[1024] = "", q[160] = "", *p = NULL;
  int k = 10;
  size_t len;

  strcpy(q, "Channels: ");
  for (struct chanset_t *chan = chanset; chan; chan = chan->next) {
    if (idx != DP_STDOUT)
      get_user_flagrec(dcc[idx].user, &fr, chan->dname);

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
      len = simple_sprintf(ch, "%s%s%s%s, ", chan->dname, p ? "(" : "", p ? p : "", p ? ")" : "");
      if ((k + len) > 70) {
        dprintf(idx, "   %s\n", q);
        strcpy(q, "          ");
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
  {"dp", "", (Function) mdop_request, NULL, LEAF},
  {"gi", "", (Function) getin_request, NULL, LEAF},
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
}
