#ifdef LEAF
/*
 * irc.c -- part of irc.mod
 *   support for channels within the bot
 *
 */

#define MODULE_NAME "irc"
#define MAKING_IRC
#include "src/mod/module.h"
#include "irc.h"
#include "server.mod/server.h"
#undef serv
#include "channels.mod/channels.h"
#include "blowfish.mod/blowfish.h"
#ifdef HAVE_UNAME
#include <sys/utsname.h>
#endif


#define op_bots (CFG_OPBOTS.gdata ? atoi(CFG_OPBOTS.gdata) : 1)
#define in_bots (CFG_INBOTS.gdata ? atoi(CFG_INBOTS.gdata) : 1)
#define lag_threshold (CFG_LAGTHRESHOLD.gdata ? atoi(CFG_LAGTHRESHOLD.gdata) : 15)
#define opreq_count (CFG_OPREQUESTS.gdata ? atoi( CFG_OPREQUESTS.gdata ) : 2)
#define opreq_seconds (CFG_OPREQUESTS.gdata ? atoi( strchr(CFG_OPREQUESTS.gdata, ':') + 1 ) : 5)
#define op_time_slack (CFG_OPTIMESLACK.gdata ? atoi(CFG_OPTIMESLACK.gdata) : 60)

#define PRIO_DEOP 1
#define PRIO_KICK 2

int host_synced = 0;

struct cfg_entry CFG_OPBOTS,
  CFG_INBOTS,
  CFG_LAGTHRESHOLD,
  CFG_OPTIMESLACK,
#ifdef S_AUTOLOCK
  CFG_FIGHTTHRESHOLD,
#endif /* S_AUTOLOCK */
  CFG_OPREQUESTS;

static Function *global = NULL, *channels_funcs = NULL, *server_funcs = NULL,
                *encryption_funcs = NULL;


static int ctcp_mode;
static int net_type = 0;
static int strict_host;
static int wait_split = 300;		/* Time to wait for user to return from
					   net-split. */
static int max_bans = 25;		/* Modified by net-type 1-4 */
static int max_exempts = 20;
static int max_invites = 20;
static int max_modes = 20;		/* Modified by net-type 1-4 */
static int bounce_bans = 0;
static int bounce_exempts = 0;
static int bounce_invites = 0;
static int bounce_modes = 0;
static int learn_users = 0;
static int wait_info = 180;
static int invite_key = 1;
static int no_chanrec_info = 0;
static int modesperline = 4;		/* Number of modes per line to send. */
static int mode_buf_len = 200;		/* Maximum bytes to send in 1 mode. */
static int use_354 = 0;			/* Use ircu's short 354 /who
					   responses. */
static int kick_method = 1;		/* How many kicks does the irc network
					   support at once?
					   0 = as many as possible.
					       (Ernst 18/3/1998) */
static int kick_fun = 0;
static int ban_fun = 1;
static int keepnick = 1;		/* Keep nick */
static int prevent_mixing = 1;		/* To prevent mixing old/new modes */
static int rfc_compliant = 1;		/* net-type changing modifies this */

static int include_lk = 1;		/* For correct calculation
					   in real_add_mode. */

#include "chan.c"
#include "mode.c"
#include "cmdsirc.c"
#include "msgcmds.c"
#include "tclirc.c"

static void detect_autokick(char *nick, char *uhost, struct chanset_t *chan, char *msg)
{
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0};
  struct userrec *u = NULL;
  int i = 0, tot = 0;

  if (!nick || !nick[0] || !uhost || !uhost[0] || !chan || !msg || !msg[0])
    return;
  
  tot = strlen(msg);
  u = get_user_by_host(uhost);
  get_user_flagrec(u, &fr, chan->dname);

  for(; *msg; ++msg) {
    if (egg_isupper(*msg))
      i++;
  }

//  if ((chan->capslimit)) {
    while (((msg) && *msg)) {
      if (egg_isupper(*msg))
        i++;
      msg++;
    }

//  if (chan->capslimit && ((i / tot) >= chan->capslimit)) {
//dprintf(DP_MODE, "PRIVMSG %s :flood stats for %s: %d/%d are CAP, percentage: %d\n", chan->name, nick, i, tot, (i/tot)*100);
//  if ((((i / tot) * 100) >= 50)) {
//dprintf(DP_HELP, "PRIVMSG %s :cap flood.\n", chan->dname);
//  }

}



void makeopline(struct chanset_t *chan, char *nick, char *buf)
{
  char plaincookie[20], enccookie[48], *p, nck[20], key[200];
  memberlist * m;
  m=ismember(chan, nick);
  if (m)
    strcpy(nck, m->nick);
  else
    strcpy(nck, nick);
  makeplaincookie(chan->dname, nck, plaincookie);
  strcpy(key, botname);
  strcat(key, netpass);
//  putlog(LOG_DEBUG, "*", "Encrypting opline for %s with cookie %s and key %s", nck, plaincookie, key);
  p = encrypt_string(key, plaincookie);
  strcpy(enccookie, p);
  nfree(p);
  sprintf(buf, STR("MODE %s +o-b %s *!*@<%s>\n"), chan->name, nck, enccookie);
}

void getin_request(char *botnick, char *code, char *par)
{

  /*
     opreq = o #chan nick
     inreq = i #chan nick uhost 
     inreq keyreply = K #chan key
   */

  char *tmp = NULL,
   *chname,
   *nck = NULL,
   *hst = NULL,
   *ip4 = NULL,
   *ip6 = NULL,
   *what = NULL,
   *p,
   *p2,
   *p3;
  struct chanset_t *chan;
  memberlist *mem = NULL;
  struct userrec *user;
  char nick[NICKLEN];
  char host[UHOSTLEN];
  char ip4host[UHOSTLEN];
  char ip6host[UHOSTLEN];
  char s[256],
    s2[16];
  int lim,
    curlim,
    sendi = 0;
  struct maskrec **mr,
   *tmr;
  struct maskstruct *b;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0 };

  if (!server_online) 
    return;
  
  if (!par[0] || !par)
    return;

  what = newsplit(&par);

  chname = newsplit(&par);

  if (!chname[0] || !chname)
    return;

  nck = newsplit(&par);
  if (!nck[0])
    return;

  hst = newsplit(&par);

  if (nck[0]) {
    strncpy0(nick, nck, sizeof(nick));
  } else
    nick[0] = 0;

  if (hst[0]) {
    strncpy0(host, hst, sizeof(host));
    ip4 = newsplit(&par);
    if (ip4[0]) {
      char *tmp2;

      tmp = nmalloc(strlen(host)+1);
      strcpy(tmp, host);
      tmp2 = strtok(tmp, "@");
      snprintf(ip4host, sizeof ip4host, "%s@%s", strtok(tmp2,"@") , ip4);
      nfree(tmp);
    } else {
      ip4host[0] = 0;
    }
    ip6 = newsplit(&par);
    if (ip6[0]) {
      char *tmp2;

      tmp = nmalloc(strlen(host)+1);
      strcpy(tmp, host);
      tmp2 = strtok(tmp, "@");
      snprintf(ip6host, sizeof ip6host, "%s@%s", strtok(tmp2,"@") , ip6);
      nfree(tmp);
    } else {
      ip6host[0] = 0;
    }
  } else
    host[0] = 0;


Context;
  if (!(chan = findchan_by_dname(chname)))
    return;

  if (!chan) {
    putlog(LOG_GETIN, "*", STR("getin req from %s for %s which is not a valid channel!"), botnick, chname);
    return;
  }
/*
  if (!ismember(chan, botname)) {
    putlog(LOG_GETIN, "*", STR("getin req from %s for %s - I'm not on %s!"), botnick, chname, chname);
    return;
  }
*/
  user = get_user_by_handle(userlist, botnick);

  if (nick[0])
    mem = ismember(chan, nick);

  if (what[0] == 'o') {
    if (!nick[0]) {
      putlog(LOG_GETIN, "*", STR("opreq from %s/??? on %s - No nick specified - SHOULD NOT HAPPEN"), botnick, chname);
      return;
    }
    if (!chan) {
      putlog(LOG_GETIN, "*", STR("opreq from %s/%s on %s - Channel %s doesn't exist"), botnick, nick, chname, chname);
      return;
    }
    if (!mem) {
      putlog(LOG_GETIN, "*", STR("opreq from %s/%s on %s - %s isn't on %s"), botnick, nick, chan->dname, nick, chan->dname);
      return;
    }
    if (!user) {
      putlog(LOG_GETIN, "*", STR("opreq from %s/%s on %s - No user called %s in userlist"), botnick, nick, chan->dname, botnick);
      return;
    }
    if (mem->user != user) {
      putlog(LOG_GETIN, "*", STR("opreq from %s/%s on %s - %s doesn't match %s"), botnick, nick, chan->dname, nick, botnick);
      return;
    }
    get_user_flagrec(user, &fr, chan->dname);

    if ((!chan_op(fr) && !glob_op(fr)) || (glob_deop(fr) && !chan_op(fr))) {
      putlog(LOG_GETIN, "*", STR("opreq from %s/%s on %s - %s doesnt have +o for chan."), botnick, nick, chan->dname, botnick);
      return;
    }
    if (chan_hasop(mem)) {
      putlog(LOG_GETIN, "*", STR("opreq from %s/%s on %s - %s already has ops"), botnick, nick, chan->dname, nick);
      return;
    }
    if (chan_issplit(mem)) {
      putlog(LOG_GETIN, "*", STR("opreq from %s/%s on %s - %s is split"), botnick, nick, chan->dname, nick);
      return;
    }
    if (!me_op(chan)) {
      putlog(LOG_GETIN, "*", STR("opreq from %s/%s on %s - I haven't got ops"), botnick, nick, chan->dname);
      return;
    }
    if (chan_sentop(mem)) {
      putlog(LOG_GETIN, "*", STR("opreq from %s/%s on %s - Already sent a +o"), botnick, nick, chan->dname);
      return;
    }
    if (server_lag > lag_threshold) {
      putlog(LOG_GETIN, "*", STR("opreq from %s/%s on %s - I'm too lagged"), botnick, nick, chan->dname);
      return;
    }
    if (getting_users()) {
      putlog(LOG_GETIN, "*", STR("opreq from %s/%s on %s - I'm getting userlist right now"), botnick, nick, chan->dname);
      return;
    }

    do_op(nick, chan, 1);
    mem->flags |= SENTOP;

    putlog(LOG_GETIN, "*", STR("opreq from %s/%s on %s - Opped"), botnick, nick, chan->dname);
  } else if (what[0] == 'i') {
Context;
    strcpy(s, getchanmode(chan));
    p = (char *) &s;
    p2 = newsplit(&p);
    p3 = newsplit(&p);
    if (!nick[0]) {
      putlog(LOG_GETIN, "*", STR("inreq from %s/??? for %s - No nick specified - SHOULD NOT HAPPEN"), botnick, chname);
      return;
    }
    if (!chan) {
      putlog(LOG_GETIN, "*", STR("inreq from %s/%s for %s - Channel %s doesn't exist"), botnick, nick, chname, chname);
      return;
    }
    if (mem) {
      putlog(LOG_GETIN, "*", STR("inreq from %s/%s for %s - %s is already on %s"), botnick, nick, chan->dname, nick, chan->dname);
      return;
    }
    if (!user) {
      putlog(LOG_GETIN, "*", STR("inreq from %s/%s for %s - No user called %s in userlist"), botnick, nick, chan->dname, botnick);
      return;
    }
    get_user_flagrec(user, &fr, chan->dname);
    if ((!chan_op(fr) && !glob_op(fr)) || (glob_deop(fr) && !chan_op(fr))) {
      putlog(LOG_GETIN, "*", STR("inreq from %s/%s for %s - %s doesn't have acces for chan."), botnick, nick, chan->dname, botnick);
      return;
    }
    if (server_lag > lag_threshold) {
      putlog(LOG_GETIN, "*", STR("inreq from %s/%s for %s - I'm too lagged"), botnick, nick, chan->dname);
      return;
    }
    if (getting_users()) {
      putlog(LOG_GETIN, "*", STR("inreq from %s/%s for %s - I'm getting userlist right now"), botnick, nick, chan->dname);
      return;
    }
    if (!(channel_pending(chan) || channel_active(chan))) {
      putlog(LOG_GETIN, "*", STR("inreq from %s/%s for %s - I'm not on %s now"), botnick, nick, chan->dname, chan->dname);
      return;
    }
    if (strchr(p2, 'l')) {
      if (!me_op(chan))
	putlog(LOG_GETIN, "*", STR("inreq from %s/%s for %s - I haven't got ops"), botnick, nick, chan->dname);
      else {
	lim = chan->channel.members + 5;
	if (!*p)
	  curlim = atoi(p3);
	else
	  curlim = atoi(p);
	if (curlim > 0 && curlim < lim) {
	  sendi = 1;
	  simple_sprintf(s2, STR("%d"), lim);
	  add_mode(chan, '+', 'l', s2);
	  putlog(LOG_GETIN, "*", STR("inreq from %s/%s for %s - Raised limit to %d"), botnick, nick, chan->dname, lim, nick);
	}
      }
    }
    mr = &global_bans;
    while (*mr) {
      if (wild_match((*mr)->mask, host) || wild_match((*mr)->mask, ip4host) || wild_match((*mr)->mask, ip6host)) {
	if (!noshare) {
	  shareout(NULL, STR("-b %s\n"), (*mr)->mask);
	}
	putlog(LOG_GETIN, "*", STR("inreq from %s/%s for %s - Removed permanent global ban %s"), botnick, nick, chan->dname, (*mr)->mask);
	//gban_total--;
	nfree((*mr)->mask);
	if ((*mr)->desc)
	  nfree((*mr)->desc);
	if ((*mr)->user)
	  nfree((*mr)->user);
	tmr = *mr;
	*mr = (*mr)->next;
	nfree(tmr);
      } else {
	mr = &((*mr)->next);
      }
    }
    mr = &chan->bans;
    while (*mr) {
      if (wild_match((*mr)->mask, host) || wild_match((*mr)->mask, ip4host) || wild_match((*mr)->mask, ip6host)) {
	if (!noshare) {
	  shareout(NULL, STR("-bc %s %s\n"), chan->dname, (*mr)->mask);
	}
	putlog(LOG_GETIN, "*", STR("inreq from %s/%s for %s - Removed permanent channel ban %s"), botnick, nick, chan->dname, (*mr)->mask);
	nfree((*mr)->mask);
	if ((*mr)->desc)
	  nfree((*mr)->desc);
	if ((*mr)->user)
	  nfree((*mr)->user);
	tmr = *mr;
	*mr = (*mr)->next;
	nfree(tmr);
      } else {
	mr = &((*mr)->next);
      }
    }
    for (b = chan->channel.ban; b->mask[0]; b = b->next) {
      if (wild_match(b->mask, host) || wild_match(b->mask, ip4host) || wild_match(b->mask, ip6host)) {
	add_mode(chan, '-', 'b', b->mask);
	putlog(LOG_GETIN, "*", STR("inreq from %s/%s for %s - Removed active ban %s"), botnick, nick, chan->dname, b->mask);
	sendi = 1;
      }
    }
    if (strchr(p2, 'k')) {
      sendi = 0;
      tmp = nmalloc(strlen(chan->dname) + strlen(p3) + 7);
      sprintf(tmp, STR("gi K %s %s"), chan->dname, p3);
      botnet_send_zapf(nextbot(botnick), botnetnick, botnick, tmp);
      putlog(LOG_GETIN, "*", STR("inreq from %s/%s for %s - Sent key (%s)"), botnick, nick, chan->dname, p3);
      nfree(tmp);
    }
    if (strchr(p2, 'i')) {
      if (!me_op(chan))
	putlog(LOG_GETIN, "*", STR("inreq from %s/%s for %s - I haven't got ops"), botnick, nick, chan->dname);
      else {
	sendi = 1;
	putlog(LOG_GETIN, "*", STR("inreq from %s/%s for %s - Invited"), botnick, nick, chan->dname);
      }
    }
    if (sendi && me_op(chan))
      dprintf(DP_MODE, STR("INVITE %s %s\n"), nick, chan->name);
  } else if (what[0] == 'K') {
    if (!chan) {
      putlog(LOG_GETIN, "*", STR("Got key for nonexistant channel %s from %s"), chname, botnick);
      return;
    }
    if (!shouldjoin(chan)) {
      putlog(LOG_GETIN, "*", STR("Got key for %s from %s - I shouldn't be on that chan?!?"), chan->dname, botnick);
    } else {
      if (!(channel_pending(chan) || channel_active(chan))) {
	putlog(LOG_GETIN, "*", STR("Got key for %s from %s (%s) - Joining"), chan->dname, botnick, nick);
	dprintf(DP_MODE, STR("JOIN %s %s\n"), chan->dname, nick);
      } else {
	putlog(LOG_GETIN, "*", STR("Got key for %s from %s - I'm already in the channel"), chan->dname, botnick);
      }
    }
  }
}

void check_hostmask()
{
  char s[UHOSTLEN + 2], *tmp = NULL;
  struct userrec *u = get_user_by_handle(userlist, botnetnick);
  struct list_type *q;

Context;
  if (!server_online || !botuserhost[0])
    return;
  tmp = botuserhost;

  if (!tmp[0] || !tmp[1]) return;
  if (tmp[0] != '~')
    sprintf(s, STR("*!%s"), tmp);
  else {
    tmp++;
    sprintf(s, STR("*!*%s"), tmp);
  }
  for (q = get_user(&USERENTRY_HOSTS, u); q; q = q->next) {
    if (!strcasecmp(q->extra, s))
      return;
  }

  addhost_by_handle(botnetnick, s);
  putlog(LOG_GETIN, "*", STR("Updated my hostmask: %s"), s);
}

static void request_op(struct chanset_t *chan)
{
  int i = 0,
    exp = 0,
    first = 100,
    n,
    cnt,
    i2;
  memberlist *ml;
  memberlist *botops[MAX_BOTS];
  char s[100],
   *l,
    myserv[SERVLEN];
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0 };

Context;
  if (channel_pending(chan) || !shouldjoin(chan) || !channel_active(chan))
    return;
  chan->channel.do_opreq = 0;
  if (me_op(chan))
    return;
  /* check server lag */
  if (server_lag > lag_threshold) {
    putlog(LOG_GETIN, "*", STR("Not asking for ops on %s - I'm too lagged"), chan->dname);
    return;
  }
  /* max opreq_count requests per opreq_seconds sec */
  n = time(NULL);
  while (i < 5) {
    if (n - chan->opreqtime[i] > opreq_seconds) {
      if (first > i)
	first = i;
      exp++;
      chan->opreqtime[i] = 0;
    }
    i++;
  }
  if ((5 - exp) >= opreq_count) {
    putlog(LOG_GETIN, "*", STR("Delaying opreq for %s - Maximum of %d:%d reached"), chan->dname, opreq_count, opreq_seconds);
    return;
  }
  check_hostmask();		/* check, and update hostmask */
  i = 0;
  ml = chan->channel.member;
  myserv[0] = 0;
  while ((i < MAX_BOTS) && (ml) && ml->nick[0]) {
    /* If bot, linked, global op & !split & chanop & (chan is reserver | bot isn't +a) -> 
       add to temp list */
    if ((i < MAX_BOTS) && (ml->user)) {
      get_user_flagrec(ml->user, &fr, NULL);
      if (bot_hublevel(ml->user) == 999 && glob_bot(fr) && glob_op(fr)
	  && !glob_deop(fr) && chan_hasop(ml) && !chan_issplit(ml)
	  && nextbot(ml->user->handle) >= 0)
	botops[i++] = ml;
    }
    if (!strcmp(ml->nick, botname))
      if (ml->server)
	strcpy(myserv, ml->server);
    ml = ml->next;
  }
  if (!i) {
    putlog(LOG_GETIN, "@", STR("No one to ask for ops on %s"), chan->dname);
    return;
  }
  /* first scan for bots on my server, ask first found for ops */
  cnt = op_bots;
  sprintf(s, STR("gi o %s %s"), chan->dname, botname);
  l = nmalloc(cnt * 50);
  l[0] = 0;
  for (i2 = 0; i2 < i; i2++) {
    if (botops[i2]->server && (!strcmp(botops[i2]->server, myserv))) {
      botnet_send_zapf(nextbot(botops[i2]->user->handle), botnetnick, botops[i2]->user->handle, s);
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
    i2 = random() % i;
    if (botops[i2]) {
      botnet_send_zapf(nextbot(botops[i2]->user->handle), botnetnick, botops[i2]->user->handle, s);
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
      if (i < op_bots)
	cnt--;
    }
  }
  putlog(LOG_GETIN, "*", STR("Requested ops on %s from %s"), chan->dname, l);
  nfree(l);
}

static void request_in(struct chanset_t *chan)
{
  char s[255],
   *l;
  int i = 0;
  int cnt,
    n;
  struct userrec *botops[MAX_BOTS];
  struct userrec *user;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0 };

  for (user = userlist; user && (i < MAX_BOTS); user = user->next) {
    get_user_flagrec(user, &fr, NULL);
    if (bot_hublevel(user) == 999 && glob_bot(fr) && glob_op(fr)
#ifdef G_BACKUP
	&& (!glob_backupbot(fr) || channel_backup(chan))
#endif
	&& nextbot(user->handle) >= 0)
      botops[i++] = user;
  }
  if (!i) {
    putlog(LOG_GETIN, "*", STR("No bots linked, can't request help to join %s"), chan->dname);
    return;
  }
  check_hostmask();		/* check, and update hostmask */
  cnt = in_bots;
  sprintf(s, STR("gi i %s %s %s!%s %s %s"), chan->dname, botname, botname, botuserhost, myipstr(4) ? myipstr(4) : "."
                                          , myipstr(6) ? myipstr(6) : ".");
  l = nmalloc(cnt * 30);
  l[0] = 0;
  while (cnt) {
    n = random() % i;
    if (botops[n]) {
      botnet_send_zapf(nextbot(botops[n]->handle), botnetnick, botops[n]->handle, s);
      if (l[0]) {
	strcat(l, ", ");
	strcat(l, botops[n]->handle);
      } else {
	strcpy(l, botops[n]->handle);
      }
      botops[n] = NULL;
      cnt--;
    } else {
      if (i < in_bots)
	cnt--;
    }
  }
  putlog(LOG_GETIN, "*", STR("Requesting help to join %s from %s"), chan->dname, l);
  nfree(l);
}


/* Contains the logic to decide wether we want to punish someone. Returns
 * true (1) if we want to, false (0) if not.
 */
static int want_to_revenge(struct chanset_t *chan, struct userrec *u,
			   struct userrec *u2, char *badnick, char *victim,
			   int mevictim)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };

  /* Do not take revenge upon ourselves. */
  if (match_my_nick(badnick))
    return 0;

  get_user_flagrec(u, &fr, chan->dname);

  /* Kickee didn't kick themself? */
  if (rfc_casecmp(badnick, victim)) {
    /* They kicked me? */
    if (mevictim) {
      /* ... and I'm allowed to take revenge? <snicker> */
      if (channel_revengebot(chan))
        return 1;
    /* Do we revenge for our users ... and do we actually know the victim? */
    } else if (channel_revenge(chan) && u2) {
      struct flag_record fr2 = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };

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
static void punish_badguy(struct chanset_t *chan, char *whobad,
			  struct userrec *u, char *badnick, char *victim,
			  int mevictim, int type)
{
  char reason[1024], ct[81], *kick_msg;
  memberlist *m;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0 };

  m = ismember(chan, badnick);
  if (!m)
    return;
  get_user_flagrec(u, &fr, chan->dname);

  /* Get current time into a string */
  egg_strftime(ct, 7, "%d %b", localtime(&now));

  /* Put together log and kick messages */
  reason[0] = 0;
  switch (type) {
  case REVENGE_KICK:
    kick_msg = IRC_KICK_PROTECT;
    simple_sprintf(reason, "kicked %s off %s", victim, chan->dname);
    break;
  case REVENGE_DEOP:
    simple_sprintf(reason, "deopped %s on %s", victim, chan->dname);
    kick_msg = IRC_DEOP_PROTECT;
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
      putlog(LOG_MISC, "*", "No longer opping %s[%s] (%s)", u->handle, whobad,
	     reason);
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
          strcpy(s1, "bad1");		/* Start with '1' */
      }
      userlist = adduser(userlist, s1, s, "-", 0);
      fr.match = FR_CHAN;
      fr.chan = USER_DEOP;
      fr.udef_chan = 0;
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
    char s[UHOSTLEN], s1[UHOSTLEN];

    splitnick(&whobad);
    maskhost(whobad, s1);
    simple_sprintf(s, "(%s) %s", ct, reason);
    u_addban(chan, s1, botnetnick, s, now + (60 * chan->ban_time), 0);
    if (!mevictim && me_op(chan)) {
      add_mode(chan, '+', 'b', s1);
      flush_mode(chan, QUICK);
    }
  }
  /* Kick the offender */
  if ((chan->revenge_mode > 1) &&
      /* ... or don't we kick ops? */
      (!channel_dontkickops(chan) ||
        !(chan_op(fr) || (glob_op(fr) && !chan_deop(fr)))) &&
      /* ... or have we sent the kick already? */
      !chan_sentkick(m) &&
      /* ... and can I actually do anything about it? */
      me_op(chan) && !mevictim) {
    dprintf(DP_MODE, STR("KICK %s %s :%s%s\n"), chan->name, m->nick, bankickprefix, kickreason(KICK_MEAN));
    m->flags |= SENTKICK;
  }
}

/* Punishes bad guys under certain circumstances using methods as defined
 * by the revenge_mode flag.
 */
static void maybe_revenge(struct chanset_t *chan, char *whobad,
			  char *whovictim, int type)
{
  char *badnick, *victim;
  int mevictim;
  struct userrec *u, *u2;

  if (!chan || (type < 0))
    return;

  /* Get info about offender */
  u = get_user_by_host(whobad);
  badnick = splitnick(&whobad);

  /* Get info about victim */
  u2 = get_user_by_host(whovictim);
  victim = splitnick(&whovictim);
  mevictim = match_my_nick(victim);

  /* Do we want to revenge? */
  if (!want_to_revenge(chan, u, u2, badnick, victim, mevictim))
    return;	/* No, leave them alone ... */

  /* Haha! Do the vengeful thing ... */
  punish_badguy(chan, whobad, u, badnick, victim, mevictim, type);
}

/* Set the key.
 */
static void my_setkey(struct chanset_t *chan, char *k)
{
  nfree(chan->channel.key);
  if (k == NULL) {
    chan->channel.key = (char *) channel_malloc(1);
    chan->channel.key[0] = 0;
    return;
  }
  chan->channel.key = (char *) channel_malloc(strlen(k) + 1);
  strcpy(chan->channel.key, k);
}

/* Adds a ban, exempt or invite mask to the list
 * m should be chan->channel.(exempt|invite|ban)
 */
static void newmask(masklist *m, char *s, char *who)
{
  for (; m && m->mask[0] && rfc_casecmp(m->mask, s); m = m->next);
  if (m->mask[0])
    return;			/* Already existent mask */

  m->next = (masklist *) channel_malloc(sizeof(masklist));
  m->next->next = NULL;
  m->next->mask = (char *) channel_malloc(1);
  m->next->mask[0] = 0;
  nfree(m->mask);
  m->mask = (char *) channel_malloc(strlen(s) + 1);
  strcpy(m->mask, s);
  m->who = (char *) channel_malloc(strlen(who) + 1);
  strcpy(m->who, who);
  m->timer = now;
}

/* Removes a nick from the channel member list (returns 1 if successful)
 */
static int killmember(struct chanset_t *chan, char *nick)
{
  memberlist *x, *old;

  old = NULL;
  for (x = chan->channel.member; x && x->nick[0]; old = x, x = x->next)
    if (!rfc_casecmp(x->nick, nick))
      break;
  if (!x || !x->nick[0]) {
    if (!channel_pending(chan))
      putlog(LOG_MISC, "*", "(!) killmember(%s) -> nonexistent", nick);
    return 0;
  }
  if (old)
    old->next = x->next;
  else
    chan->channel.member = x->next;
  nfree(x);
  chan->channel.members--;

  /* The following two errors should NEVER happen. We will try to correct
   * them though, to keep the bot from crashing.
   */
  if (chan->channel.members < 0) {
     chan->channel.members = 0;
     for (x = chan->channel.member; x && x->nick[0]; x = x->next)
       chan->channel.members++;
     putlog(LOG_MISC, "*", "(!) actually I know of %d members.",
	    chan->channel.members);
  }
  if (!chan->channel.member) {
    chan->channel.member = (memberlist *) channel_malloc(sizeof(memberlist));
    chan->channel.member->nick[0] = 0;
    chan->channel.member->next = NULL;
  }
  return 1;
}

/* Check if I am a chanop. Returns boolean 1 or 0.
 */
static int me_op(struct chanset_t *chan)
{
  memberlist *mx = NULL;

  mx = ismember(chan, botname);
  if (!mx)
    return 0;
  if (chan_hasop(mx))
    return 1;
  else
    return 0;
}
/* Check whether I'm voice. Returns boolean 1 or 0.
 */
static int me_voice(struct chanset_t *chan)
{
  memberlist *mx;

  mx = ismember(chan, botname);
  if (!mx)
    return 0;
  if (chan_hasvoice(mx))
    return 1;
  else
    return 0;
}


/* Check if there are any ops on the channel. Returns boolean 1 or 0.
 */
static int any_ops(struct chanset_t *chan)
{
  memberlist *x;

  for (x = chan->channel.member; x && x->nick[0]; x = x->next)
    if (chan_hasop(x))
      break;
  if (!x || !x->nick[0])
    return 0;
  return 1;
}
/* Reset the channel information.
 */
static void reset_chan_info(struct chanset_t *chan)
{
  int opped = 0;
  /* Don't reset the channel if we're already resetting it */
  if (!shouldjoin(chan)) {
    dprintf(DP_MODE,"PART %s\n", chan->name);
    return;
  }
  if (!channel_pending(chan)) {
    if (me_op(chan))
      opped += 1;
    nfree(chan->channel.key);
    chan->channel.key = (char *) channel_malloc(1);
    chan->channel.key[0] = 0;
    clear_channel(chan, 1);
    chan->status |= CHAN_PEND;
    chan->status &= ~(CHAN_ACTIVE | CHAN_ASKEDMODES);
    if (!(chan->status & CHAN_ASKEDBANS)) {
      chan->status |= CHAN_ASKEDBANS;
      dprintf(DP_MODE, "MODE %s +b\n", chan->name);
    }
#ifdef S_IRCNET
    if (opped) {
/* FIXME: broken +e checking */
putlog(LOG_DEBUG, "@", "I AM +o in %s, need to send +e", chan->dname);
      if (!(chan->ircnet_status & CHAN_ASKED_EXEMPTS) &&
  	  use_exempts == 1) {
putlog(LOG_DEBUG, "@", "I AM NOW CHECKING +e for %s", chan->dname);
        chan->ircnet_status |= CHAN_ASKED_EXEMPTS;
        dprintf(DP_MODE, "MODE %s +e\n", chan->name);
      }
      if (!(chan->ircnet_status & CHAN_ASKED_INVITED) &&
	  use_invites == 1) {
        chan->ircnet_status |= CHAN_ASKED_INVITED;
        dprintf(DP_MODE, "MODE %s +I\n", chan->name);
      }
    }
#endif
    /* These 2 need to get out asap, so into the mode queue */
    dprintf(DP_MODE, "MODE %s\n", chan->name);
    if (use_354)
      dprintf(DP_MODE, "WHO %s %%c%%h%%n%%u%%f\n", chan->name);
    else
      dprintf(DP_MODE, "WHO %s\n", chan->name);
    /* clear_channel nuked the data...so */
  }
}

/* Leave the specified channel and notify registered Tcl procs. This
 * should not be called by itsself.
 */
static void do_channel_part(struct chanset_t *chan)
{
  if (shouldjoin(chan) && chan->name[0]) {
    /* Using chan->name is important here, especially for !chans <cybah> */
    dprintf(DP_SERVER, "PART %s\n", chan->name);

    /* As we don't know of this channel anymore when we receive the server's
       ack for the above PART, we have to notify about it _now_. */
  }
}


/* If i'm the only person on the channel, and i'm not op'd,
 * might as well leave and rejoin. If i'm NOT the only person
 * on the channel, but i'm still not op'd, demand ops.
 */
static void check_lonely_channel(struct chanset_t *chan)
{
  memberlist *m;
  char s[UHOSTLEN];
  int i = 0;
  static int whined = 0;

  if (channel_pending(chan) || !channel_active(chan) || me_op(chan) ||
      !shouldjoin(chan) || (chan->channel.mode & CHANANON))
    return;
  /* Count non-split channel members */
  for (m = chan->channel.member; m && m->nick[0]; m = m->next)
    if (!chan_issplit(m))
      i++;
  if (i == 1 && channel_cycle(chan) && !channel_stop_cycle(chan)) {
    if (chan->name[0] != '+') {	/* Its pointless to cycle + chans for ops */
      putlog(LOG_MISC, "*", "Trying to cycle %s to regain ops.", chan->dname);
      dprintf(DP_MODE, "PART %s\n", chan->name);
      /* If it's a !chan, we need to recreate the channel with !!chan <cybah> */
      dprintf(DP_MODE, "JOIN %s%s %s\n", (chan->dname[0] == '!') ? "!" : "",
	      chan->dname, chan->key_prot);
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
    int ok = 1;
    struct userrec *u;

    if (!whined) {
      /* + is opless. Complaining about no ops when without special
       * help(services), we cant get them - Raist
       */
      if (chan->name[0] != '+')
	putlog(LOG_MISC, "*", "%s is active but has no ops :(", chan->dname);
      whined = 1;
    }
    for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
      sprintf(s, "%s!%s", m->nick, m->userhost);
      u = get_user_by_host(s);
      if (!match_my_nick(m->nick) && (!u || !(u->flags & USER_BOT))) {
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

static void warn_pls_take()
{
  struct chanset_t *chan;
  for (chan = chanset; chan; chan = chan->next)
    if (channel_take(chan) && me_op(chan))
      putlog(LOG_WARN, "*", "%s is set +take, and I'm already opped! Try +bitch instead", chan->dname);
}


void check_servers() {
  struct chanset_t * chan;
  memberlist * m;
  for (chan=chanset;chan;chan=chan->next) {
    for (m=chan->channel.member;m && m->nick[0];m=m->next)
      if (chan_hasop(m) && (!m->server || !m->server[0] || !m->hops)) {
        dprintf(DP_HELP, STR("WHO %s\n"), chan->name);
        return;
      }
  }
}

#ifdef S_AUTOLOCK
void check_netfight()
{
  int limit = atoi(CFG_FIGHTTHRESHOLD.gdata ? CFG_FIGHTTHRESHOLD.gdata : "0");
Context;
  if (limit) {
    struct chanset_t *chan;
    for (chan = chanset; chan; chan = chan->next) {
Context;
      if ((chan->channel.fighting) && (chan->channel.fighting > limit)) {
Context;
        if (!channel_bitch(chan) || !channel_closed(chan)) {
Context;
          putlog(LOG_WARN, "*", STR("Auto-closed %s - channel fight\n"), chan->dname);
Context;
          do_chanset(chan, STR("+bitch +closed"), 1);
Context;
          enforce_closed(chan);
Context;
          dprintf(DP_MODE, STR("TOPIC %s :Auto-closed - channel fight\n"), chan->name);
Context;
        }
Context;
      }
      chan->channel.fighting = 0; 		/* we put this here because we need to clear it once per min */
    }
  }
Context;
}
#endif /* S_AUTOLOCK */


void raise_limit(struct chanset_t * chan) {
  int nl, cl, i, mem, ul, ll;
  char s[50];
  
  if (!chan || !me_op(chan)) {
    return;
  }

  mem = chan->channel.members; //members
  nl = mem + chan->limitraise; //new limit
  cl = chan->channel.maxmembers; //current limit

  i = chan->limitraise / 4;

  /* if the newlimit will be in the range made by these vars, dont change. */
  ul = nl + i; //upper limit range
  ll = nl - i; //lower limit range
  if (cl > ll && cl < ul) {
    return; 			/* the current limit is in the range, so leave it. */
  }

  if (nl != chan->channel.maxmembers) {
    sprintf(s, "%d", nl);
    add_mode(chan, '+', 'l', s);
  }

}

static void check_expired_chanstuff()
{
  masklist *b, *e;
  memberlist *m, *n;
  char s[UHOSTLEN];
  struct chanset_t *chan;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0};

  if (!server_online)
    return;
  for (chan = chanset; chan; chan = chan->next) {
    if (channel_active(chan)) {
      if (me_op(chan)) {
       if (channel_dynamicbans(chan) && chan->ban_time)
	  for (b = chan->channel.ban; b->mask[0]; b = b->next)
           if (now - b->timer > 60 * chan->ban_time &&
		!u_sticky_mask(chan->bans, b->mask) &&
		!u_sticky_mask(global_bans, b->mask) &&
		expired_mask(chan, b->who)) {
	      putlog(LOG_MODES, chan->dname,
		     "(%s) Channel ban on %s expired.",
		     chan->dname, b->mask);
	      add_mode(chan, '-', 'b', b->mask);
	      b->timer = now;
	    }
#ifdef S_IRCNET
       if (use_exempts && channel_dynamicexempts(chan) && chan->exempt_time)
	  for (e = chan->channel.exempt; e->mask[0]; e = e->next)
           if (now - e->timer > 60 * chan->exempt_time &&
		!u_sticky_mask(chan->exempts, e->mask) &&
		!u_sticky_mask(global_exempts, e->mask) &&
		expired_mask(chan, e->who)) {
	      /* Check to see if it matches a ban */
	      int match = 0;

	      for (b = chan->channel.ban; b->mask[0]; b = b->next)
		if (wild_match(b->mask, e->mask) ||
		    wild_match(e->mask, b->mask)) {
		  match = 1;
		  break;
	      }
	      /* Leave this extra logging in for now. Can be removed later
	       * Jason
	       */
	      if (match) {
		putlog(LOG_MODES, chan->dname,
		       "(%s) Channel exemption %s NOT expired. Exempt still set!",
		       chan->dname, e->mask);
	      } else {
		putlog(LOG_MODES, chan->dname,
		       "(%s) Channel exemption on %s expired.",
		       chan->dname, e->mask);
		add_mode(chan, '-', 'e', e->mask);
	      }
	      e->timer = now;
	    }

	if (use_invites && channel_dynamicinvites(chan) &&
           chan->invite_time && !(chan->channel.mode & CHANINV))
	  for (b = chan->channel.invite; b->mask[0]; b = b->next)
           if (now - b->timer > 60 * chan->invite_time &&
		!u_sticky_mask(chan->invites, b->mask) &&
		!u_sticky_mask(global_invites, b->mask) &&
		expired_mask(chan, b->who)) {
	      putlog(LOG_MODES, chan->dname,
		     "(%s) Channel invitation on %s expired.",
		     chan->dname, b->mask);
	      add_mode(chan, '-', 'I', b->mask);
	      b->timer = now;
	    }
#endif
	if (chan->idle_kick)
	  for (m = chan->channel.member; m && m->nick[0]; m = m->next)
	    if (now - m->last >= chan->idle_kick * 60 &&
		!match_my_nick(m->nick) && !chan_issplit(m)) {
	      sprintf(s, "%s!%s", m->nick, m->userhost);
	      get_user_flagrec(m->user ? m->user : get_user_by_host(s),
			       &fr, chan->dname);
              if (!(glob_bot(fr) || (glob_op(fr) && !glob_deop(fr)) || chan_op(fr))) {
		dprintf(DP_SERVER, "KICK %s %s :%sidle %d min\n", chan->name,
			m->nick, kickprefix, chan->idle_kick);
		m->flags |= SENTKICK;
	      }
	    }
      }
      for (m = chan->channel.member; m && m->nick[0]; m = n) {
	n = m->next;
	if (m->split && now - m->split > wait_split) {
	  sprintf(s, "%s!%s", m->nick, m->userhost);
	  putlog(LOG_JOIN, chan->dname,
		 "%s (%s) got lost in the net-split.",
		 m->nick, m->userhost);
	  killmember(chan, m->nick);
	}
	m = n;
      }
     //autovoice of +v users if bot is +y
      if (!loading && channel_active(chan) && me_op(chan) && dovoice(chan)) {
        for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
          sprintf(s, "%s!%s", m->nick, m->userhost);
          if (!m->user)
            m->user = get_user_by_host(s);

          if (m->user) {
            struct flag_record fr2 = { FR_GLOBAL | FR_CHAN, 0, 0 };
            get_user_flagrec(m->user, &fr2, chan->dname);
            if (!private(fr2, chan, PRIV_VOICE) && 
               ((channel_voice(chan) && !chk_devoice(fr2, chan)) ||
                (!channel_voice(chan) && chk_voice(fr2, chan))) &&      
               !glob_bot(fr2) &&
               !chan_hasop(m) && !chan_hasvoice(m) && !(m->flags & EVOICE)) {
              putlog(LOG_DEBUG, "@", "VOICING %s in %s as '%s'", m->nick, chan->dname, m->user->handle);
              add_mode(chan, '+', 'v', m->nick);
            } else if (!glob_bot(fr2) && 
              (chk_devoice(fr2, chan) || (m->flags & EVOICE))) {
              if (!chan_hasop(m) && chan_hasvoice(m))
                add_mode(chan, '-', 'v', m->nick);
            }
          } else if (m->user == NULL && !(m->flags & EVOICE)) {
             if (channel_voice(chan) && !chan_hasop(m) && !chan_hasvoice(m)) {
/* FIXME: remove next line after testing for +v of +q user bug on -host */
               putlog(LOG_DEBUG, "@", "VOICING %s in %s as NULL user (+voice)", m->nick, chan->dname);
               add_mode(chan, '+', 'v', m->nick);
             }
          }
        }
      }
      check_lonely_channel(chan);
    }
    else if (shouldjoin(chan) && !channel_pending(chan))
      dprintf(DP_MODE, "JOIN %s %s\n",
              (chan->name[0]) ? chan->name : chan->dname,
              chan->channel.key[0] ? chan->channel.key : chan->key_prot);
  }
}

static int channels_6char STDVAR
{
  Function F = (Function) cd;
  char x[20];

  BADARGS(7, 7, " nick user@host handle desto/chan keyword/nick text");
  CHECKVALIDITY(channels_6char);
  sprintf(x, "%d", F(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]));
  Tcl_AppendResult(irp, x, NULL);
  return TCL_OK;
}

static int channels_5char STDVAR
{
  Function F = (Function) cd;

  BADARGS(6, 6, " nick user@host handle channel text");
  CHECKVALIDITY(channels_5char);
  F(argv[1], argv[2], argv[3], argv[4], argv[5]);
  return TCL_OK;
}

static int channels_4char STDVAR
{
  Function F = (Function) cd;

  BADARGS(5, 5, " nick uhost hand chan/param");
  CHECKVALIDITY(channels_4char);
  F(argv[1], argv[2], argv[3], argv[4]);
  return TCL_OK;
}

static int channels_2char STDVAR
{
  Function F = (Function) cd;

  BADARGS(3, 3, " channel type");
  CHECKVALIDITY(channels_2char);
  F(argv[1], argv[2]);
  return TCL_OK;
}

#ifdef S_AUTH
static int check_tcl_pubc(char *cmd, char *nick, char *uhost,
                         struct userrec *u, char *args, char *chan)
{
  struct flag_record fr = {FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0, 0};
  char *hand = u ? u->handle : "*";
  int x;

  get_user_flagrec(u, &fr, chan);
  Tcl_SetVar(interp, "_msgc1", nick, 0);
  Tcl_SetVar(interp, "_msgc2", uhost, 0);
  Tcl_SetVar(interp, "_msgc3", hand, 0);
  Tcl_SetVar(interp, "_msgc4", args, 0);
  Tcl_SetVar(interp, "_msgc5", chan, 0);
  x = check_tcl_bind(H_msgc, cmd, &fr, " $_msgc1 $_msgc2 $_msgc3 $_msgc4 $_msgc5",
                     MATCH_EXACT | BIND_HAS_BUILTINS | BIND_USE_ATTR);
  if (x == BIND_EXEC_LOG)
    putlog(LOG_CMDS, "*", "(%s!%s) !%s! %s %s%s %s", nick, uhost, hand, chan, cmdprefix,
           cmd, args);
  return ((x == BIND_MATCHED) || (x == BIND_EXECUTED) || (x == BIND_EXEC_LOG));

}
#endif /* S_AUTH */

static tcl_ints myints[] =
{
  {"learn-users",		&learn_users,		0},	/* arthur2 */
  {"wait-split",		&wait_split,		0},
  {"wait-info",			&wait_info,		0},
  {"bounce-bans",		&bounce_bans,		0},
  {"bounce-exempts",		&bounce_exempts,	0},
  {"bounce-invites",		&bounce_invites,	0},
  {"bounce-modes",		&bounce_modes,		0},
  {"modes-per-line",		&modesperline,		0},
  {"mode-buf-length",		&mode_buf_len,		0},
  {"use-354",			&use_354,		0},
  {"kick-method",		&kick_method,		0},
  {"kick-fun",			&kick_fun,		0},
  {"ban-fun",			&ban_fun,		0},
  {"invite-key",		&invite_key,		0},
  {"no-chanrec-info",		&no_chanrec_info,	0},
  {"max-bans",			&max_bans,		0},
  {"max-exempts",		&max_exempts,		0},
  {"max-invites",		&max_invites,		0},
  {"max-modes",			&max_modes,		0},
  {"net-type",			&net_type,		0},
  {"strict-host",		&strict_host,		0},	/* arthur2 */
  {"ctcp-mode",			&ctcp_mode,		0},	/* arthur2 */
  {"keep-nick",			&keepnick,		0},	/* guppy */
  {"prevent-mixing",		&prevent_mixing,	0},
  {"rfc-compliant",		&rfc_compliant,		0},
  {"include-lk",		&include_lk,		0},
  {NULL,			NULL,			0}	/* arthur2 */
};

/* Flush the modes for EVERY channel.
 */
static void flush_modes()
{
  struct chanset_t *chan;
  memberlist *m;

  if (modesperline > MODES_PER_LINE_MAX)
    modesperline = MODES_PER_LINE_MAX;
  for (chan = chanset; chan; chan = chan->next) {
    for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
      if (m->delay && m->delay <= now) {
	m->delay = 0L;
	m->flags &= ~FULL_DELAY;
        if (chan_sentop(m)) {
          m->flags &= ~SENTOP;
          do_op(m->nick, chan, 0);
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

static void irc_report(int idx, int details)
{
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0, 0};
  char ch[1024], q[160], *p;
  int k, l;
  struct chanset_t *chan;

  strcpy(q, "Channels: ");
  k = 10;
  for (chan = chanset; chan; chan = chan->next) {
    if (idx != DP_STDOUT)
      get_user_flagrec(dcc[idx].user, &fr, chan->dname);
     
    if (!private(fr, chan, PRIV_OP) &&
        (idx == DP_STDOUT || glob_master(fr) || chan_master(fr))) {
      p = NULL;
      if (shouldjoin(chan)) {
	if (chan->status & CHAN_JUPED)
	  p = MISC_JUPED;
	else if (!(chan->status & CHAN_ACTIVE))
	  p = MISC_TRYING;
	else if (chan->status & CHAN_PEND)
	  p = MISC_PENDING;
        else if ((chan->dname[0] != '+') && !me_op(chan))
	  p = MISC_WANTOPS;
      }
      l = simple_sprintf(ch, "%s%s%s%s, ", chan->dname, p ? "(" : "",
			 p ? p : "", p ? ")" : "");
      if ((k + l) > 70) {
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

static void do_nettype()
{
  switch (net_type) {
  case 0:		/* Efnet */
    kick_method = 1;
    modesperline = 4;
    use_354 = 0;
#ifdef S_IRCNET
    use_exempts = 1;
    use_invites = 1;
#endif
    max_bans = 100;
    max_modes = 100;
    rfc_compliant = 1;
    include_lk = 0;
    break;
  case 1:		/* Ircnet */
    kick_method = 4;
    modesperline = 3;
    use_354 = 0;
#ifdef S_IRCNET
    use_exempts = 1;
    use_invites = 1;
#endif
    max_bans = 30;
    max_modes = 30;
    rfc_compliant = 1;
    include_lk = 1;
    break;
  case 2:		/* Undernet */
    kick_method = 1;
    modesperline = 6;
    use_354 = 1;
#ifdef S_IRCNET
    use_exempts = 0;
    use_invites = 0;
#endif
    max_bans = 30;
    max_modes = 30;
    rfc_compliant = 1;
    include_lk = 1;
    break;
  case 3:		/* Dalnet */
    kick_method = 1;
    modesperline = 6;
    use_354 = 0;
#ifdef S_IRCNET
    use_exempts = 0;
    use_invites = 0;
#endif
    max_bans = 100;
    max_modes = 100;
    rfc_compliant = 0;
    include_lk = 1;
    break;
  case 4:		/* hybrid-6+ */
    kick_method = 1;
    modesperline = 4;
    use_354 = 0;
#ifdef S_IRCNET
    use_exempts = 1;
    use_invites = 0;
#endif
    max_bans = 20;
    max_modes = 20;
    rfc_compliant = 1;
    include_lk = 0;
    break;
  default:
    break;
  }
  /* Update all rfc_ function pointers */
  add_hook(HOOK_RFC_CASECMP, (Function) rfc_compliant);
}

#if (((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 4)) || (TCL_MAJOR_VERSION > 8))
static char *traced_nettype(ClientData cdata, Tcl_Interp *irp,
                            CONST char *name1, CONST char *name2, int flags)
#else
static char *traced_nettype(ClientData cdata, Tcl_Interp *irp, char *name1,
                            char *name2, int flags)
#endif
{
  do_nettype();
  return NULL;
}

#if (((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 4)) || (TCL_MAJOR_VERSION > 8))
static char *traced_rfccompliant(ClientData cdata, Tcl_Interp *irp,
                                 CONST char *name1, CONST char *name2,
                                 int flags)
#else
static char *traced_rfccompliant(ClientData cdata, Tcl_Interp *irp,
                                 char *name1, char *name2, int flags)
#endif
{
  /* This hook forces eggdrop core to change the rfc_ match function
   * links to point to the rfc compliant versions if rfc_compliant
   * is 1, or to the normal version if it's 0.
   */
  add_hook(HOOK_RFC_CASECMP, (Function) rfc_compliant);
  return NULL;
}

static int irc_expmem()
{
  return 0;
}

static cmd_t irc_bot[] = {
  {"dp", "", (Function) mdop_request, NULL},
  {"gi", "", (Function) getin_request, NULL},
  {0, 0, 0, 0}
};


static void getin_3secondly()
{
  if (!server_online)
    return;
  else {
    struct chanset_t *ch;
    for (ch = chanset; ch; ch = ch->next) {
      if ((channel_pending(ch) || channel_active(ch)) && (!me_op(ch)))
        request_op(ch);
    }
  }
}

static void irc_10secondly() {
  struct chanset_t *ch = chanset;

  for (ch=chanset;ch;ch=ch->next)
    if (channel_closed(ch))
      enforce_closed(ch);
}

EXPORT_SCOPE char *irc_start();

static Function irc_table[] =
{
  /* 0 - 3 */
  (Function) irc_start,
  (Function) NULL,
  (Function) irc_expmem,
  (Function) irc_report,
  /* 4 - 7 */
  (Function) 0,		
  (Function) 0,		
  (Function) 0,
  (Function) 0,
  /* 8 - 11 */
  (Function) 0,		
  (Function) 0,		
  (Function) 0,		
  (Function) 0,		
  /* 12 - 15 */
  (Function) 0,
  (Function) 0,
  (Function) 0,
  (Function) recheck_channel,
  /* 16 - 19 */
  (Function) me_op,
  (Function) recheck_channel_modes,
  (Function) 0,
  (Function) do_channel_part,
  /* 20 - 23 */
  (Function) check_this_ban,
  (Function) check_this_user,
  (Function) me_voice,
};

void irc_describe(struct cfg_entry *cfgent, int idx)
{
}

void irc_changed(struct cfg_entry *cfgent, char *oldval, int *valid)
{
  int i;

  if (!cfgent->gdata)
    return;
  *valid = 0;
  if (!strcmp(cfgent->name, STR("op-requests"))) {
    int L,
      R;
    char *value = cfgent->gdata;

    L = atoi(value);
    value = strchr(value, ':');
    if (!value)
      return;
    value++;
    R = atoi(value);
    if ((R >= 60) || (R <= 3) || (L < 1) || (L > R))
      return;
    *valid = 1;
    return;
  }
  i = atoi(cfgent->gdata);
  if (!strcmp(cfgent->name, STR("op-bots"))) {
    if ((i < 1) || (i > 10))
      return;
  } else if (!strcmp(cfgent->name, STR("invite-bots"))) {
    if ((i < 1) || (i > 10))
      return;
  } else if (!strcmp(cfgent->name, STR("key-bots"))) {
    if ((i < 1) || (i > 10))
      return;
  } else if (!strcmp(cfgent->name, STR("limit-bots"))) {
    if ((i < 1) || (i > 10))
      return;
  } else if (!strcmp(cfgent->name, STR("unban-bots"))) {
    if ((i < 1) || (i > 10))
      return;
  } else if (!strcmp(cfgent->name, STR("lag-threshold"))) {
    if ((i < 3) || (i > 60))
      return;
  } else if (!strcmp(cfgent->name, STR("fight-threshold"))) {
    if (i && ((i < 50) || (i > 1000)))
      return;
  } else if (!strcmp(cfgent->name, STR("op-time-slack"))) {
    if ((i < 30) || (i > 1200))
      return;
  }
  *valid = 1;
  return;
}


struct cfg_entry CFG_OPBOTS = {
  "op-bots", CFGF_GLOBAL, NULL, NULL,
  irc_changed, NULL, irc_describe
};

struct cfg_entry CFG_INBOTS = {
  "in-bots", CFGF_GLOBAL, NULL, NULL,
  irc_changed, NULL, irc_describe
};

struct cfg_entry CFG_LAGTHRESHOLD = {
  "lag-threshold", CFGF_GLOBAL, NULL, NULL,
  irc_changed, NULL, irc_describe
};

struct cfg_entry CFG_OPREQUESTS = {
  "op-requests", CFGF_GLOBAL, NULL, NULL,
  irc_changed, NULL, irc_describe
};

struct cfg_entry CFG_OPTIMESLACK = {
  "op-time-slack", CFGF_GLOBAL, NULL, NULL,
  irc_changed, NULL, irc_describe
};

#ifdef S_AUTOLOCK
struct cfg_entry CFG_FIGHTTHRESHOLD = {
  "fight-threshold", CFGF_GLOBAL, NULL, NULL,
  irc_changed, NULL, irc_describe
};
#endif /* S_AUTOLOCK */

char *irc_start(Function * global_funcs)
{
  struct chanset_t *chan;

  global = global_funcs;

  module_register(MODULE_NAME, irc_table, 1, 3);
  if (!(server_funcs = module_depend(MODULE_NAME, "server", 1, 0))) {
    module_undepend(MODULE_NAME);
    return "This module requires server module 1.0 or later.";
  }
  if (!(channels_funcs = module_depend(MODULE_NAME, "channels", 1, 0))) {
    module_undepend(MODULE_NAME);
    return "This module requires channels module 1.0 or later.";
  }
  if (!(encryption_funcs = module_depend(MODULE_NAME, "encryption", 0, 0))) {
    module_undepend(MODULE_NAME);
    return "This module requires an encryption modules.";
  }
  add_cfg(&CFG_OPBOTS);
  add_cfg(&CFG_INBOTS);
  add_cfg(&CFG_LAGTHRESHOLD);
  add_cfg(&CFG_OPREQUESTS);
  add_cfg(&CFG_OPTIMESLACK);
#ifdef S_AUTOLOCK
  add_cfg(&CFG_FIGHTTHRESHOLD);
#endif /* S_AUTOLOCK */
  for (chan = chanset; chan; chan = chan->next) {
    if (shouldjoin(chan))
      dprintf(DP_MODE, "JOIN %s %s\n",
              (chan->name[0]) ? chan->name : chan->dname, chan->key_prot);
    chan->status &= ~(CHAN_ACTIVE | CHAN_PEND | CHAN_ASKEDBANS);
#ifdef S_IRCNET
    chan->ircnet_status &= ~(CHAN_ASKED_INVITED | CHAN_ASKED_EXEMPTS);
#endif
  }
  add_hook(HOOK_MINUTELY, (Function) check_expired_chanstuff);
  add_hook(HOOK_MINUTELY, (Function) warn_pls_take);
  add_hook(HOOK_MINUTELY, (Function) check_servers);
  add_hook(HOOK_ADD_MODE, (Function) real_add_mode);
  add_hook(HOOK_IDLE, (Function) flush_modes);
  add_hook(HOOK_3SECONDLY, (Function) getin_3secondly);
  add_hook(HOOK_10SECONDLY, (Function) irc_10secondly);
#ifdef S_AUTOLOCK
  add_hook(HOOK_MINUTELY, (Function) check_netfight);
#endif /* S_AUTOLOCK */
  Tcl_TraceVar(interp, "net-type",
	       TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
	       traced_nettype, NULL);
  Tcl_TraceVar(interp, "rfc-compliant",
	       TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
	       traced_rfccompliant, NULL);
  add_tcl_ints(myints);
  add_builtins(H_bot, irc_bot);
  add_builtins(H_dcc, irc_dcc);
  add_builtins(H_msg, C_msg);
#ifdef S_AUTH
  add_builtins(H_msgc, C_msgc);
#endif /* S_AUTH */
  add_builtins(H_raw, irc_raw);
  add_tcl_commands(tclchan_cmds);
  do_nettype();
  return NULL;
}
#endif
