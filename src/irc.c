
/* 
 * irc.c -- part of irc.mod
 *   support for channels withing the bot 
 * 
 * $Id: irc.c,v 1.36 2000/01/08 21:23:16 per Exp $
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

#define MAKING_IRC
#include "main.h"
#include "tandem.h"
#include "irc.h"
#include "server.h"
#include "channels.h"
#include "users.h"
#include "hook.h"
#include <ctype.h>
/*
#ifdef HAVE_UNAME
#include <sys/utsname.h>
#endif
*/

#define op_bots (CFG_OPBOTS.gdata ? atoi(CFG_OPBOTS.gdata) : 1)
#define invite_bots (CFG_INVITEBOTS.gdata ? atoi(CFG_INVITEBOTS.gdata) : 1)
#define key_bots (CFG_KEYBOTS.gdata ? atoi(CFG_KEYBOTS.gdata) : 1)
#define limit_bots (CFG_LIMITBOTS.gdata ? atoi(CFG_LIMITBOTS.gdata) : 1)
#define unban_bots (CFG_UNBANBOTS.gdata ? atoi(CFG_UNBANBOTS.gdata) : 1)
#define lag_threshold (CFG_LAGTHRESHOLD.gdata ? atoi(CFG_LAGTHRESHOLD.gdata) : 15)
#define opreq_count (CFG_OPREQUESTS.gdata ? atoi( CFG_OPREQUESTS.gdata ) : 2)
#define opreq_seconds (CFG_OPREQUESTS.gdata ? atoi( strchr(CFG_OPREQUESTS.gdata, ':') + 1 ) : 5)


int
  host_synced = 0;

struct cfg_entry 
  CFG_OPBOTS,
  CFG_KEYBOTS,
  CFG_INVITEBOTS,
  CFG_LIMITBOTS,
  CFG_UNBANBOTS,
  CFG_LAGTHRESHOLD,
  CFG_OPTIMESLACK,
#ifdef G_AUTOLOCK
  CFG_KILLTHRESHOLD,
  CFG_LOCKTHRESHOLD,
  CFG_FIGHTTHRESHOLD,
#endif
  CFG_OPREQUESTS;
#ifdef LEAF
p_tcl_bind_list H_topc,
  H_splt,
  H_sign,
  H_rejn,
  H_part,
  H_pub,
  H_pubm;
p_tcl_bind_list H_nick,
  H_mode,
  H_kick,
  H_join;

extern int timesync;
extern struct dcc_t *dcc;
extern struct userrec *userlist,
 *lastuser;
extern char tempdir[],
  botnetnick[],
  botname[],
  natip[],
  hostname[],
  netpass[];
extern char origbotname[],
  botuser[],
  ver[];
extern char helpdir[],
  version[],
  localkey[],
  owner[];
extern int reserved_port,
  noshare,
  dcc_total,
  egg_numver,
  use_silence,
  role;
extern int use_console_r,
  ignore_time,
  debug_output,
  gban_total,
  make_userfile;
extern int gexempt_total,
  ginvite_total;
extern int default_flags,
  max_dcc,
  share_greet,
  password_timeout;
extern int min_dcc_port,
  max_dcc_port;			/* dw */
extern int use_invites,
  use_exempts;			/* Jason/drummer */
extern int force_expire;	/* Rufus */
extern int do_restart,
  ban_time,
  exempt_time,
  invite_time;
extern time_t now,
  online_since;
extern struct chanset_t *chanset;
extern int protect_readonly;
int cmd_die(),
  xtra_kill(),
  xtra_unpack();

/*
char bot_realname[121];
char autoaway[121];
*/
char kickprefix[20];
char bankickprefix[20];

int ctcp_mode;
int net_type;
int strict_host;

/* time to wait for user to return for net-split */
int wait_split = 300;
int max_bans = 20;
int max_exempts = 20;
int max_invites = 20;
int max_modes = 30;
int bounce_bans = 1;
int bounce_exempts = 0;
int bounce_invites = 0;
int bounce_modes = 0;
int no_chanrec_info = 0;
int modesperline = 4;		/* number of modes per line to send */
int mode_buf_len = 200;		/* maximum bytes to send in 1 mode */
int use_354 = 0;		/* use ircu's short 354 /who responses */
int kick_method = 1;		/* how many kicks does the irc network

				   * support at once? */
				/* 0 = as many as possible. Ernst 18/3/1998 */

int kick_bogus = 0;
int ban_bogus = 0;

int kick_fun = 0;
int ban_fun = 0;
int prevent_mixing = 1;		/* to prevent mixing old/new modes */

int rfc_compliant = 1;		/* net-type changing modify this, but

				 * requires ircmod reload. drummer/9/12/1999 */


void check_tcl_kickmode(char *, char *, struct userrec *, char *, char *, char *, p_tcl_bind_list);
void check_tcl_joinpart(char *, char *, struct userrec *, char *, p_tcl_bind_list);
void check_tcl_signtopcnick(char *, char *, struct userrec *u, char *, char *, p_tcl_bind_list);
void irc_gotmode(char *from, char *msg);

void makeplaincookie(char *chname, char *nick, char *buf)
{
  /*
     plain cookie:
     Last 6 digits of time
     Last 5 chars of nick 
     Last 4 regular chars of chan
   */
  char work[256],
    work2[256];
  int i,
    n;

  sprintf(work, STR("%010li"), (now + timesync));
  strcpy(buf, (char *) &work[4]);
  work[0] = 0;
  if (strlen(nick) < 5)
    while (strlen(work) + strlen(nick) < 5)
      strcat(work, " ");
  else
    strcpy(work, (char *) &nick[strlen(nick) - 5]);
  strcat(buf, work);

  n = 3;
  for (i = strlen(chname) - 1; (i >= 0) && (n >= 0); i--)
    if (((unsigned char) chname[i] < 128) && ((unsigned char) chname[i] > 32)) {
      work2[n] = tolower(chname[i]);
      n--;
    }
  while (n >= 0)
    work2[n--] = ' ';
  work2[4] = 0;
  strcat(buf, work2);
}

void makeopline(struct chanset_t *chan, char *nick, char *buf)
{
  char plaincookie[20],
    enccookie[48],
   *p,
    nck[20],
    key[200];
  memberlist * m;
  m=ismember(chan, nick);
  if (m)
    strcpy(nck, m->nick);
  else
    strcpy(nck, nick);
  makeplaincookie(chan->name, nck, plaincookie);
  strcpy(key, botname);
  strcat(key, netpass);
  p = encrypt_string(key, plaincookie);
  strcpy(enccookie, p);
  nfree(p);
  p = enccookie + strlen(enccookie) - 1;
  while (*p == '.')
    *p-- = 0;
  sprintf(buf, STR("MODE %s +o-b %s *!*@[%s]\n"), chan->name, nck, enccookie);
}

void getin_request(char *botnick, char *code, char *par)
{
  /*
     o #chan nick
     i #chan nick
     k #chan
     l #chan
     u #chan
   */
  char *tmp,
   *chname,
   *nck = NULL,
   *p,
   *p2;
  struct chanset_t *chan;
  memberlist *mem = NULL;
  struct userrec *user;
  char nick[256];
  char s[256];
  int lim,
    sendi = 0;
  struct maskrec **mr,
   *tmr;
  struct maskstruct *b;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0 };

  tmp = nmalloc(strlen(par) + 1);
  strcpy(tmp, par);
  chname = strchr(tmp, ' ');
  if (!chname) {
    nfree(tmp);
    return;
  }
  *chname++ = 0;
  nck = strchr(chname, ' ');
  if (nck) {
    *nck++ = 0;
  }
  chan = findchan(chname);
  user = get_user_by_handle(userlist, botnick);
  if (nck) {
    mem = chan ? ismember(chan, nck) : NULL;
    strncpy0(nick, nck, sizeof(nick));
  } else {
    nick[0] = 0;
  }

  if (par[0] == 'o') {
    if (!nick[0]) {
      log(LCAT_WARNING, STR("opreq from %s/??? on %s - No nick specified - SHOULD NOT HAPPEN"), botnick, chname);
      log(LCAT_GETIN, STR("opreq from %s/??? on %s - No nick specified - SHOULD NOT HAPPEN"), botnick, chname);
      nfree(tmp);
      return;
    }
    if (!chan) {
      log(LCAT_GETIN, STR("opreq from %s/%s on %s - Channel %s don't exist"), botnick, nick, chname, chname);
      nfree(tmp);
      return;
    }
    nfree(tmp);
    if (!mem) {
      log(LCAT_GETIN, STR("opreq from %s/%s on %s - %s isn't on %s"), botnick, nick, chan->name, nick, chan->name);
      return;
    }
    if (!user) {
      log(LCAT_GETIN, STR("opreq from %s/%s on %s - No user called %s in userlist"), botnick, nick, chan->name, botnick);
      return;
    }
    if (mem->user != user) {
      log(LCAT_GETIN, STR("opreq from %s/%s on %s - %s don't match %s"), botnick, nick, chan->name, nick, botnick);
      return;
    }
    get_user_flagrec(user, &fr, NULL);
    if (!glob_op(fr)) {
      log(LCAT_GETIN, STR("opreq from %s/%s on %s - %s isn't global +o"), botnick, nick, chan->name, botnick);
      return;
    }
    if (chan_hasop(mem)) {
      log(LCAT_GETIN, STR("opreq from %s/%s on %s - %s already has ops"), botnick, nick, chan->name, nick);
      return;
    }
    if (chan_issplit(mem)) {
      log(LCAT_GETIN, STR("opreq from %s/%s on %s - %s is split"), botnick, nick, chan->name, nick);
      return;
    }
    if (!me_op(chan)) {
      log(LCAT_GETIN, STR("opreq from %s/%s on %s - I haven't got ops"), botnick, nick, chan->name);
      return;
    }
    if (chan_sentop(mem)) {
      log(LCAT_GETIN, STR("opreq from %s/%s on %s - Already sent a +o"), botnick, nick, chan->name);
      return;
    }
    if (server_lag > lag_threshold) {
      log(LCAT_GETIN, STR("opreq from %s/%s on %s - I'm too lagged"), botnick, nick, chan->name);
      return;
    }
    if (getting_users()) {
      log(LCAT_GETIN, STR("opreq from %s/%s on %s - I'm getting userlist right now"), botnick, nick, chan->name);
      return;
    }
    if (
#ifdef G_FASTOP
	channel_fastop(chan) || 
#endif
#ifdef G_TAKE
	channel_take(chan) ||
#endif
	0) 
      {
      add_mode(chan, '+', 'o', nick);
    } else {
      tmp = nmalloc(strlen(chan->name) + 200);
      makeopline(chan, nick, tmp);
      dprintf(DP_MODE, tmp);
      mem->flags |= SENTOP;
      nfree(tmp);
    }
    log(LCAT_GETIN, STR("opreq from %s/%s on %s - Opped"), botnick, nick, chan->name);
  } else if (par[0] == 'i') {
    if (!nick[0]) {
      log(LCAT_WARNING, STR("invreq from %s/??? for %s - No nick specified - SHOULD NOT HAPPEN"), botnick, chname);
      log(LCAT_GETIN, STR("invreq from %s/??? for %s - No nick specified - SHOULD NOT HAPPEN"), botnick, chname);
      nfree(tmp);
      return;
    }
    if (!chan) {
      log(LCAT_GETIN, STR("invreq from %s/%s for %s - Channel %s don't exist"), botnick, nick, chname, chname);
      nfree(tmp);
      return;
    }
    nfree(tmp);
    if (mem) {
      log(LCAT_GETIN, STR("invreq from %s/%s for %s - %s is already on %s"), botnick, nick, chan->name, nick, chan->name);
      return;
    }
    if (!user) {
      log(LCAT_GETIN, STR("invreq from %s/%s for %s - No user called %s in userlist"), botnick, nick, chan->name, botnick);
      return;
    }
    get_user_flagrec(user, &fr, NULL);
    if (!glob_op(fr)) {
      log(LCAT_GETIN, STR("invreq from %s/%s for %s - %s isn't global +o"), botnick, nick, chan->name, botnick);
      return;
    }
    if (!me_op(chan)) {
      log(LCAT_GETIN, STR("invreq from %s/%s for %s - I haven't got ops"), botnick, nick, chan->name);
      return;
    }
    if (server_lag > lag_threshold) {
      log(LCAT_GETIN, STR("invreq from %s/%s for %s - I'm too lagged"), botnick, nick, chan->name);
      return;
    }
    if (getting_users()) {
      log(LCAT_GETIN, STR("invreq from %s/%s for %s - I'm getting userlist right now"), botnick, nick, chan->name);
      return;
    }
    sendi = 1;
    log(LCAT_GETIN, STR("invreq from %s/%s for %s - Invited"), botnick, nick, chan->name);
  } else if (par[0] == 'k') {
    if (!chan) {
      log(LCAT_GETIN, STR("keyreq from %s for %s - Channel %s don't exist"), botnick, chname, chname);
      nfree(tmp);
      return;
    }
    nfree(tmp);
    if (mem) {
      log(LCAT_GETIN, STR("keyreq from %s for %s - %s is already on %s"), botnick, chan->name, nick, chan->name);
      return;
    }
    if (!user) {
      log(LCAT_GETIN, STR("keyreq from %s for %s - No user called %s in userlist"), botnick, chan->name, botnick);
      return;
    }
    get_user_flagrec(user, &fr, NULL);
    if (!glob_op(fr)) {
      log(LCAT_GETIN, STR("keyreq from %s for %s - %s don't match %s"), botnick, chan->name, botnick);
      return;
    }
    if (getting_users()) {
      log(LCAT_GETIN, STR("keyreq from %s/%s for %s - I'm getting userlist right now"), botnick, chan->name);
      return;
    }
    if (!(channel_pending(chan) || channel_active(chan))) {
      log(LCAT_GETIN, STR("keyreq from %s for %s - I'm not on %s now"), botnick, chan->name, chan->name);
      return;
    }
    strcpy(s, getchanmode(chan));
    p = (char *) &s;
    p2 = newsplit(&p);
    if (!strchr(p2, 'k')) {
      log(LCAT_GETIN, STR("keyreq from %s for %s - %s isn't +k"), botnick, chan->name, chan->name);
      return;
    }
    tmp = nmalloc(strlen(chan->name) + strlen(p) + 10);
    sprintf(tmp, STR("gi K %s %s"), chan->name, p);
    botnet_send_zapf(nextbot(botnick), botnetnick, botnick, tmp);
    log(LCAT_GETIN, STR("keyreq from %s for %s - Sent key (%s)"), botnick, chan->name, p);
    nfree(tmp);
  } else if (par[0] == 'K') {
    if (!chan) {
      log(LCAT_GETIN, STR("Got key for nonexistant channel %s from %s"), chname, botnick);
      nfree(tmp);
      return;
    }
    nfree(tmp);
    if (!shouldjoin(chan)) {
      log(LCAT_GETIN, STR("Got key for %s from %s - I shouldn't be on that chan?!?"), chan->name, botnick);
    } else {
      if (!(channel_pending(chan) || channel_active(chan))) {
	log(LCAT_GETIN, STR("Got key for %s from %s (%s)- joining"), chan->name, botnick, nick);
	dprintf(DP_MODE, STR("JOIN %s %s\n"), chan->name, nick);
      } else {
	log(LCAT_GETIN, STR("Got key for %s from %s - I'm already in the channel"), chan->name, botnick);
      }
    }
  } else if (par[0] == 'l') {
    if (!chan) {
      log(LCAT_GETIN, STR("limitreq from %s/%s for %s - Channel %s don't exist"), botnick, nick, chname, chname);
      nfree(tmp);
      return;
    }
    nfree(tmp);
    if (mem) {
      log(LCAT_GETIN, STR("limitreq from %s/%s for %s - %s is already on %s"), botnick, nick, chan->name, botnick, chan->name);
      return;
    }
    if (!user) {
      log(LCAT_GETIN, STR("limitreq from %s/%s for %s - No user called %s in userlist"), botnick, nick, chan->name, botnick);
      return;
    }
    get_user_flagrec(user, &fr, NULL);
    if (!glob_op(fr)) {
      log(LCAT_GETIN, STR("limitreq from %s/%s for %s - %s isn't global +o"), botnick, nick, chan->name, botnick);
      return;
    }
    if (getting_users()) {
      log(LCAT_GETIN, STR("limitreq from %s/%s for %s - I'm getting userlist right now"), botnick, nick, chan->name);
      return;
    }
    if (!(channel_pending(chan) || channel_active(chan))) {
      log(LCAT_GETIN, STR("limitreq from %s/%s for %s - I'm not on %s right now"), botnick, nick, chan->name, chan->name);
      return;
    }
    if (!me_op(chan)) {
      log(LCAT_GETIN, STR("limitreq from %s/%s for %s - I'm not opped"), botnick, nick, chan->name);
      return;
    }
    strcpy(s, getchanmode(chan));
    p = (char *) &s;
    p2 = newsplit(&p);
    if (!strchr(p2, 'l')) {
      log(LCAT_GETIN, STR("limitreq from %s/%s for %s - %s isn't +l"), botnick, nick, chan->name, chan->name);
      return;
    }
    lim = chan->channel.members + 10;
    sendi = 1;
    dprintf(DP_MODE, STR("MODE %s +l %i\n"), chan->name, lim);
    log(LCAT_GETIN, STR("limitreq from %s/%s on %s - Raised limit to %d"), botnick, nick, chan->name, lim, nick);
  } else if (par[0] == 'u') {
    if (!chan) {
      log(LCAT_GETIN, STR("unbanreq from %s/%s for %s - Channel %s don't exist"), botnick, nick, chname, chname);
      nfree(tmp);
      return;
    }
    nfree(tmp);
    if (mem) {
      log(LCAT_GETIN, STR("unbanreq from %s/%s for %s - %s is already on %s"), botnick, nick, chan->name, botnick, chan->name);
      return;
    }
    if (!user) {
      log(LCAT_GETIN, STR("unbanreq from %s/%s for %s - No user called %s in userlist"), botnick, nick, chan->name, botnick);
      return;
    }
    get_user_flagrec(user, &fr, NULL);
    if (!glob_op(fr)) {
      log(LCAT_GETIN, STR("unbanreq from %s/%s for %s - %s isn't global +o"), botnick, nick, chan->name, botnick);
      return;
    }
    if (getting_users()) {
      log(LCAT_GETIN, STR("unbanreq from %s/%s for %s - I'm getting userlist right now"), botnick, nick, chan->name);
      return;
    }
    if (!(channel_pending(chan) || channel_active(chan))) {
      log(LCAT_GETIN, STR("unbanreq from %s/%s for %s - I'm not on %s right now"), botnick, nick, chan->name, chan->name);
      return;
    }
    if (!me_op(chan)) {
      log(LCAT_GETIN, STR("unbanreq from %s/%s for %s - I'm not opped"), botnick, nick, chan->name);
      return;
    }
    mr = &global_bans;
    while (*mr) {
      if (wild_match((*mr)->mask, nick)) {
	if (!noshare) {
	  shareout(NULL, STR("-b %s\n"), (*mr)->mask);
	}
	log(LCAT_GETIN, STR("unbanreq from %s/%s on %s: removed permanent global ban %s"), botnick, nick, chan->name, (*mr)->mask);
	gban_total--;
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
      if (wild_match((*mr)->mask, nick)) {
	if (!noshare) {
	  shareout(NULL, STR("-bc %s %s\n"), chan->name, (*mr)->mask);
	}
	log(LCAT_GETIN, STR("unbanreq from %s/%s on %s: removed permanent channel ban %s"), botnick, nick, chan->name, (*mr)->mask);
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
    lim = 0;
    for (b = chan->channel.ban; b->mask[0]; b = b->next) {
      if (wild_match(b->mask, nick)) {
	dprintf(DP_MODE, STR("MODE %s -b %s\n"), chan->name, b->mask);
	log(LCAT_GETIN, STR("unbanreq from %s/%s on %s: removed active ban %s"), botnick, nick, chan->name, b->mask);
	sendi = 1;
      }
    }
    *strchr(nick, '!') = 0;
  }
  if (sendi)
    dprintf(DP_MODE, STR("INVITE %s %s\n"), nick, chan->name);

}

void request_op(struct chanset_t *chan)
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
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0 };
  chan->channel.do_opreq=0;
  if (me_op(chan))
    return;
  /* check server lag */
  if (server_lag > lag_threshold) {
    log(LCAT_GETIN, STR("Not asking for ops on %s - I'm too lagged"), chan->name);
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
    log(LCAT_GETIN, STR("Delaying opreq for %s - Maximum of %d:%d reached"), chan->name, opreq_count, opreq_seconds);
    return;
  }
  i = 0;
  ml = chan->channel.member;
  myserv[0] = 0;
  while ((i < MAX_BOTS) && (ml) && ml->nick[0]) {
    /* If bot, linked, global op & !split & chanop & (chan is reserver | bot isn't +a) -> 
       add to temp list */
    if ((i < MAX_BOTS) && (ml->user)) {
      get_user_flagrec(ml->user, &fr, NULL);
      if ((bot_hublevel(ml->user)==999) && (glob_bot(fr)) && (glob_op(fr)) && (!glob_deop(fr)) 
	  && (chan_hasop(ml)) && (!chan_issplit(ml))
	  && (nextbot(ml->user->handle) >= 0))
	botops[i++] = ml;
    }
    if (!strcmp(ml->nick, botname))
      if (ml->server)
	strcpy(myserv, ml->server);
    ml = ml->next;
  }
  if (!i) {
    log(LCAT_GETIN, STR("Noone to ask for ops on %s"), chan->name);
    return;
  }

  /* first scan for bots on my server, ask first found for ops */
  cnt = op_bots;
  sprintf(s, STR("gi o %s %s"), chan->name, botname);
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
    i2 = rand() % i;
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
  log(LCAT_GETIN, STR("Requested ops on %s from %s"), chan->name, l);
  nfree(l);
}

void request_key(struct chanset_t *chan)
{
  char s[255],
   *l;
  int i = 0;
  int cnt,
    n;
  struct userrec *botops[MAX_BOTS];
  struct userrec *user;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0 };

  for (user = userlist; user && (i < MAX_BOTS); user = user->next) {
    get_user_flagrec(user, &fr, NULL);
    if (glob_bot(fr) && glob_op(fr) && (nextbot(user->handle) >= 0) 
#ifdef G_BACKUP
	&& (!glob_backupbot(fr) || channel_backup(chan))
#endif
	) {
      botops[i++] = user;
    }
  }
  if (!i) {
    log(LCAT_GETIN, STR("No bots linked, can't request key for %s"), chan->name);
    return;
  }
  cnt = key_bots;
  sprintf(s, STR("gi k %s"), chan->name);
  l = nmalloc(cnt * 30);
  l[0] = 0;
  while (cnt) {
    n = rand() % i;
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
      if (i < key_bots)
	cnt--;
    }
  }
  log(LCAT_GETIN, STR("Requesting key for %s from %s"), chan->name, l);
  nfree(l);
}

void request_invite(struct chanset_t *chan)
{
  char s[255],
   *l;
  int i = 0;
  int cnt,
    n;
  struct userrec *botops[MAX_BOTS];
  struct userrec *user;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0 };

  for (user = userlist; user && (i < MAX_BOTS); user = user->next) {
    get_user_flagrec(user, &fr, NULL);
    if (glob_bot(fr) && glob_op(fr) && (nextbot(user->handle) >= 0) 
#ifdef G_BACKUP
	&& (!glob_backupbot(fr) || channel_backup(chan))
#endif
	) {
      botops[i++] = user;
    }
  }
  if (!i) {
    log(LCAT_GETIN, STR("No bots linked, can't request invite for %s"), chan->name);
    return;
  }
  cnt = invite_bots;
  sprintf(s, STR("gi i %s %s"), chan->name, botname);
  l = nmalloc(cnt * 30);
  l[0] = 0;
  while (cnt) {
    n = rand() % i;
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
      if (i < invite_bots)
	cnt--;
    }
  }
  log(LCAT_GETIN, STR("Requesting invite to %s from %s"), chan->name, l);
  nfree(l);
}

void request_unban(struct chanset_t *chan)
{
  char s[255],
   *l;
  int i = 0;
  int cnt,
    n;
  struct userrec *botops[MAX_BOTS];
  struct userrec *user;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0 };

  for (user = userlist; user && (i < MAX_BOTS); user = user->next) {
    get_user_flagrec(user, &fr, NULL);
    if (glob_bot(fr) && glob_op(fr) && (nextbot(user->handle) >= 0) 
#ifdef G_BACKUP
	&& (!glob_backupbot(fr) || channel_backup(chan))
#endif
	) {
      botops[i++] = user;
    }
  }
  if (!i) {
    log(LCAT_GETIN, STR("No bots linked, can't request unban on %s"), chan->name);
    return;
  }
  cnt = unban_bots;
  sprintf(s, STR("gi u %s %s!%s"), chan->name, botname, botuserhost);
  l = nmalloc(cnt * 30);
  l[0] = 0;
  while (cnt) {
    n = rand() % i;
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
      if (i < unban_bots)
	cnt--;
    }
  }
  log(LCAT_GETIN, STR("Requesting unban on %s from %s"), chan->name, l);
  nfree(l);
}

void request_limit(struct chanset_t *chan)
{
  char s[255],
   *l;
  int i = 0;
  int cnt,
    n;
  struct userrec *botops[MAX_BOTS];
  struct userrec *user;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0 };

  for (user = userlist; user && (i < MAX_BOTS); user = user->next) {
    get_user_flagrec(user, &fr, NULL);
    if (glob_bot(fr) && glob_op(fr) && (nextbot(user->handle) >= 0) 
#ifdef G_BACKUP
	&& (!glob_backupbot(fr) || channel_backup(chan))
#endif
	) {
      botops[i++] = user;
    }
  }
  if (!i) {
    log(LCAT_GETIN, STR("No bots linked, can't request limit raise on %s"), chan->name);
    return;
  }
  cnt = limit_bots;
  sprintf(s, STR("gi l %s %s"), chan->name, botname);
  l = nmalloc(cnt * 30);
  l[0] = 0;
  while (cnt) {
    n = rand() % i;
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
      if (i < limit_bots)
	cnt--;
    }
  }
  log(LCAT_GETIN, STR("Requesting limit raise on %s from %s"), chan->name, l);
  nfree(l);
}

#define PRIO_DEOP 1
#define PRIO_KICK 2

char * kickreason(int kind) {
  int r;
  r=rand();
  switch (kind) {
  case KICK_BANNED:
    switch (r % 5) {
    case 0: return STR("bye");
    case 1: return STR("banned");
    case 2: return STR("bummer");
    case 3: return STR("go away");
    case 4: return STR("cya around looser");
    }
  case KICK_KUSER:
    switch (r % 4) {
    case 0: return STR("not wanted");
    case 1: return STR("something tells me you're annoying");
    case 2: return STR("don't bug me looser");
    case 3: return STR("creep");
    }
  case KICK_KICKBAN:
    switch (r % 4) {
    case 0: return STR("gone");
    case 1: return STR("stupid");
    case 2: return STR("looser");
    case 3: return STR("...");
    }     
  case KICK_MASSDEOP:
    switch (r % 7) {
    case 0: return STR("spammer!");
    case 1: return STR("easy on the modes now");
    case 2: return STR("mode this");
    case 3: return STR("nice try");
    case 4: return STR("really?");
    case 5: return STR("mIRC sux for mdop kiddo");
    case 6: return STR("scary... really scary...");
    }
  case KICK_BADOP:
    switch (r % 5) {
    case 0: return STR("neat...");
    case 1: return STR("oh, no you don't. go away.");
    case 2: return STR("didn't you forget something now?");
    case 3: return STR("no");
    case 4: return STR("hijack this");
    }
  case KICK_BADOPPED:
    switch (r % 5) {
    case 0: return STR("buggar off kid");
    case 1: return STR("asl?");
    case 2: return STR("whoa... what a hacker... skills!");
    case 3: return STR("yes! yes! yes! hit me baby one more time!");
    case 4: return STR("with your skills, you're better off jacking off than hijacking");
    }
  case KICK_MANUALOP:
    switch (r % 4) {
    case 0: return STR("naughty kid");
    case 1: return STR("didn't someone tell you that is bad?");
    case 2: return STR("want perm?");
    case 3: return STR("see how much good that did you?");
    }
  case KICK_MANUALOPPED:
    switch (r % 8) {
    case 0: return STR("your pal got mean friends. like me.");
    case 1: return STR("uhh now.. don't wake me up...");
    case 2: return STR("hi hun. missed me?");
    case 3: return STR("spammer! die!");
    case 4: return STR("boo!");
    case 5: return STR("that @ was useful, don't ya think?");
    case 6: return STR("not in my book");
    case 7: return STR("lol, really?");
    }
  case KICK_LOCKED:
    switch (r % 17) {
    case 0: return STR("locked");
    case 1: return STR("later");
    case 2: return STR("closed for now");
    case 3: return STR("sorry, but it's getting late, locking channel. cya around");
    case 4: return STR("better safe than sorry");
    case 5: return STR("cleanup, come back later");
    case 6: return STR("this channel is closed");
    case 7: return STR("shutting down for now");
    case 8: return STR("lockdown");
    case 9: return STR("reopening later");
    case 10: return STR("not for the public atm");
    case 11: return STR("private channel for now");
    case 12: return STR("might reopen soon, might reopen later");
    case 13: return STR("you're not supposed to be here right now");
    case 14: return STR("sorry, closed");
    case 15: return STR("try us later, atm we're locked down");
    case 16: return STR("closed. try tomorrow");
    }
  case KICK_FLOOD:
    switch (r % 5) {
    case 0: return STR("so much bullshit in such a short time. amazing.");
    case 1: return STR("slow down. i'm trying to read here.");
    case 2: return STR("uhm... you actually think irc is for talking?");
    case 3: return STR("talk talk talk");
    case 4: return STR("blabbering are we?");
    }
  case KICK_NICKFLOOD:
    switch (r % 5) {
    case 0: return STR("make up your mind?");
    case 1: return STR("be schizofrenic elsewhere");
    case 2: return STR("I'm loosing track of you... not!");
    case 3: return STR("that is REALLY annoying");
    case 4: return STR("try this: /NICK looser");
    }
  case KICK_KICKFLOOD:
    switch (r % 5) {
    case 0: return STR("easier to just leave if you wan't to be alone");
    case 1: return STR("cool down");
    case 2: return STR("don't be so damned aggressive. that's my job.");
    case 3: return STR("kicking's fun, isn't it?");
    case 4: return STR("what's the rush?");
    }
  case KICK_BOGUSUSERNAME:
    return STR("bogus username");
  case KICK_MEAN:
    switch (r % 10) {
    case 0: return STR("hey! that wasn't very nice!");
    case 1: return STR("don't fuck with my pals");
    case 2: return STR("meanie!");
    case 3: return STR("I can be a bitch too...");
    case 4: return STR("leave the bots alone, will ya?");
    case 5: return STR("not very clever");
    case 6: return STR("watch it");
    case 7: return STR("fuck off");
    case 8: return STR("easy now. that's a friend.");
    case 9: return STR("abuse of power. leave that to me, will ya?");      
    }
  case KICK_BOGUSKEY:
    return STR("I have a really hard time reading that key");
  default:
    return "!";    
  }

}

void priority_do(struct chanset_t * chan, int opsonly, int action) {
  memberlist *m;
  int ops=0, targets=0, bpos=0, tpos=0, ft=0, ct=0, actions=0, sent=0;
  if (!me_op(chan))
    return;
  if (channel_pending(chan))
    return;
  for (m=chan->channel.member; m && m->nick[0]; m=m->next) {
    if (!m->user) {
      char s[256];
      sprintf(s, STR("%s!%s"), m->nick, m->userhost);
      m->user=get_user_by_host(s);
    }
    if (m->user && ((m->user->flags & (USER_BOT | USER_OP))==(USER_BOT | USER_OP))) {
      ops++;
      if (!strcmp(m->nick, botname))
	bpos=(ops-1);
    } else if (!opsonly || chan_hasop(m)) {
      struct flag_record fr={FR_GLOBAL | FR_CHAN, 0, 0, 0, 0};
      if (m->user)
	get_user_flagrec(m->user, &fr, chan->name);
      if (chan_deop(fr) || glob_deop(fr) || ( !chan_op(fr) && !glob_op(fr) )) {
	targets++;
      }
    }
  }
  if (!targets || !ops)
    return;
  ft = (bpos * targets) / ops;
  ct = ((bpos +2) * targets + (ops - 1)) / ops;
  ct = (ct-ft+1);
  if (ct>20)
    ct=20;
  while (ft>=targets)
    ft -= targets;
  actions=0;
  sent=0;
  for (m=chan->channel.member; m && m->nick[0]; m=m->next) {
    if (!opsonly || chan_hasop(m)) {
      struct flag_record fr={FR_GLOBAL | FR_CHAN, 0, 0, 0, 0};
      if (m->user)
	get_user_flagrec(m->user, &fr, chan->name);
      if (chan_deop(fr) || glob_deop(fr) || ( !chan_op(fr) && !glob_op(fr) )) {
	if (tpos>=ft) {
	  if ((action==PRIO_DEOP) && !chan_sentdeop(m)) {
	    actions++;
	    sent++;
	    add_mode(chan, '-', 'o', m->nick);
	    if (actions>=ct) {
	      flush_mode(chan, QUICK);
	      return;
	    }
	  } else if ((action==PRIO_KICK) && !chan_sentkick(m) ) {
	    actions++;
	    sent++;
	    dprintf(DP_MODE, STR("KICK %s %s :%s%s\n"), chan->name, m->nick, kickprefix, kickreason(KICK_LOCKED));
	    m->flags |= SENTKICK;
	    if (actions>=ct)
	      return;
	  }
	}
	tpos++;
      }
    }
  }

  ct = ct - actions;
  if (ct>ft)
    ct=ft;
  ft=0;
  actions=0;
  tpos=0;
  for (m=chan->channel.member; m && m->nick[0]; m=m->next) {
    if (!opsonly || chan_hasop(m)) {
      struct flag_record fr={FR_GLOBAL | FR_CHAN, 0, 0, 0, 0};
      if (m->user)
	get_user_flagrec(m->user, &fr, chan->name);
      if (chan_deop(fr) || glob_deop(fr) || ( !chan_op(fr) && !glob_op(fr) )) {
	if (tpos>=ft) {
	  if ((action==PRIO_DEOP) && !chan_sentdeop(m)) {
	    actions++;
	    sent++;
	    add_mode(chan, '-', 'o', m->nick);
	    if ((actions>=ct) || (sent>20)) {
	      flush_mode(chan, QUICK);
	      return;
	    }
	  } else if ((action==PRIO_KICK) && !chan_sentkick(m) ) {
	    actions++;
	    dprintf(DP_MODE, STR("KICK %s %s :%s%s\n"), chan->name, m->nick, kickprefix, kickreason(KICK_LOCKED));
	    m->flags |= SENTKICK;
	    if ((actions>=ct) || (sent>5))
	      return;
	  }
	}
	tpos++;
      }
    }
  }
}

int target_priority(struct chanset_t * chan, memberlist *target, int opsonly) {
  memberlist *m;
  int ops=0, targets=0, bpos=0, ft=0, ct=0, tp= (-1), pos=0;
  for (m=chan->channel.member; m && m->nick[0]; m=m->next) {
    if (m->user && ((m->user->flags & (USER_BOT | USER_OP))==(USER_BOT | USER_OP))) {
      ops++;
      if (!strcmp(m->nick, botname))
	bpos=ops;
    } else if (!opsonly || chan_hasop(m)) {
      struct flag_record fr={FR_GLOBAL | FR_CHAN, 0, 0, 0, 0};
      if (m->user)
	get_user_flagrec(m->user, &fr, chan->name);
      if (chan_deop(fr) || glob_deop(fr) || ( !chan_op(fr) && !glob_op(fr) )) {
	targets++;
      }
    }
    if (m==target)
      tp=pos;
    pos++;
  }
  if (!targets || !ops || (tp<0))
    return 0;
  ft = (bpos * targets) / ops;
  ct = ((bpos +2) * targets + (ops - 1)) / ops;
  ct = (ct-ft+1);
  if (ct>20)
    ct=20;
  while (ft>=targets) {
    ft -= targets;
  }
  if (ct >= targets) {
    if ((tp>=ft) || (tp<= (ct % targets)))
      return 1;
  } else {
    if ((tp>=ft) && (tp<=ct))
      return 1;
  }
  return 0;
}

void channel_check_locked(struct chanset_t * chan) {
  /* Kick one random nonop */  
  if (!me_op(chan))
    return;
  if (!strchr(getchanmode(chan), 'i'))
    dprintf(DP_MODE, STR("MODE %s +i\n"), chan->name);
  priority_do(chan, 0, PRIO_KICK);
}


void getin_3secondly()
{
  struct chanset_t *ch = chanset;
  while (ch) {
    if ((channel_pending(ch) || channel_active(ch)) && (!me_op(ch)))
      request_op(ch);
    ch = ch->next;
  }
}


void irc_10secondly() {
  struct chanset_t *ch = chanset;

  for (ch=chanset;ch;ch=ch->next) 
    if (channel_locked(ch))
      channel_check_locked(ch);
}

/*
 * chan.c -- part of irc.mod
 *   almost everything to do with channel manipulation
 *   telling channel status
 *   'who' response
 *   user kickban, kick, op, deop
 *   idle kicking
 * 
 * dprintf'ized, 27oct1995
 * multi-channel, 8feb1996
 * 
 * $Id: chan.c,v 1.55 2000/01/22 23:31:54 per Exp $
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

/* new ctcp stuff */
time_t irc_last_ctcp = (time_t) 0L;
int irc_count_ctcp = 0;

/* new gotinvite stuff */
time_t last_invtime = (time_t) 0L;
char last_invchan[300] = "";

/* returns a pointer to a new channel member structure */
memberlist *newmember(struct chanset_t *chan, char * nick)
{
  memberlist *x, *lx, *n;

  x = chan->channel.member;
  lx=NULL;
  while (x && x->nick[0] && (rfc_casecmp(x->nick, nick)<0)) {
    lx=x;
    x = x->next;
  }
  n = (memberlist *) channel_malloc(sizeof(memberlist));
  n->next = NULL;
  strncpy0(n->nick, nick, sizeof(n->nick));
  n->split = 0L;
  n->last = 0L;
  n->delay = 0L;
  if (!lx) {
    n->next = chan->channel.member;
    chan->channel.member=n;
  } else {
    n->next = lx->next;
    lx->next = n;
  }
  chan->channel.members++;
  return n;
}

void update_idle(char *chname, char *nick)
{
  memberlist *m;
  struct chanset_t *chan;

  chan = findchan(chname);
  if (chan) {
    m = ismember(chan, nick);
    if (m)
      m->last = now;
  }
}

/* what the channel's mode CURRENTLY is */
char *getchanmode(struct chanset_t *chan)
{
  static char s[121];
  int atr,
    i;

  s[0] = '+';
  i = 1;
  atr = chan->channel.mode;
  if (atr & CHANINV)
    s[i++] = 'i';
  if (atr & CHANPRIV)
    s[i++] = 'p';
  if (atr & CHANSEC)
    s[i++] = 's';
  if (atr & CHANMODER)
    s[i++] = 'm';
  if (atr & CHANTOPIC)
    s[i++] = 't';
  if (atr & CHANNOMSG)
    s[i++] = 'n';
  if (atr & CHANANON)
    s[i++] = 'a';
  if (atr & CHANKEY)
    s[i++] = 'k';
  if (chan->channel.maxmembers > -1)
    s[i++] = 'l';
  s[i] = 0;
  if (chan->channel.key[0])
    i += sprintf(s + i, STR(" %s"), chan->channel.key);
  if (chan->channel.maxmembers > -1)
    sprintf(s + i, STR(" %d"), chan->channel.maxmembers);
  return s;
}

/*        Check a channel and clean-out any more-specific matching masks.
 *      Moved all do_ban(), do_exempt() and do_invite() into this single
 *      function as the code bloat is starting to get rediculous <cybah>
 */
void do_mask(struct chanset_t *chan, struct maskstruct *m, char *mask, char Mode)
{
  while (m && m->mask[0]) {
    if (wild_match(mask, m->mask) && rfc_casecmp(mask, m->mask)) {
      add_mode(chan, '-', Mode, m->mask);
    }

    m = m->next;
  }

  add_mode(chan, '+', Mode, mask);
  flush_mode(chan, QUICK);
}

/* this is a clone of detect_flood, but works for channel specificity now
 * and handles kick & deop as well */
int detect_chan_flood(char *floodnick, char *floodhost, char *from, struct chanset_t *chan, int which, char *victim)
{
  char h[UHOSTLEN],
    ftype[12],
   *p;
  struct userrec *u;
  memberlist *m;
  int thr = 0,
    lapse = 0;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };

  if (!chan || (which < 0) || (which >= FLOOD_CHAN_MAX))
    return 0;
  m = ismember(chan, floodnick);
  /* let's not fight against non-existant channel members and
   * IRC services like ChanServ  (Fabian) */
  if ((!m) && (which != FLOOD_JOIN))
    return 0;
  get_user_flagrec(get_user_by_host(from), &fr, chan->name);
  Context;
  if (glob_bot(fr))
    return 0;
  /* determine how many are necessary to make a flood */
  switch (which) {
  case FLOOD_PRIVMSG:
  case FLOOD_NOTICE:
    thr = chan->flood_pub_thr;
    lapse = chan->flood_pub_time;
    strcpy(ftype, STR("pub"));
    break;
  case FLOOD_CTCP:
    thr = chan->flood_ctcp_thr;
    lapse = chan->flood_ctcp_time;
    strcpy(ftype, STR("pub"));
    break;
  case FLOOD_JOIN:
  case FLOOD_NICK:
    thr = chan->flood_join_thr;
    lapse = chan->flood_join_time;
    if (which == FLOOD_JOIN)
      strcpy(ftype, STR("join"));
    else
      strcpy(ftype, STR("nick"));
    break;
  case FLOOD_DEOP:
    thr = chan->flood_deop_thr;
    lapse = chan->flood_deop_time;
    strcpy(ftype, STR("deop"));
    break;
  case FLOOD_KICK:
    thr = chan->flood_kick_thr;
    lapse = chan->flood_kick_time;
    strcpy(ftype, STR("kick"));
    break;
  }
  if ((thr == 0) || (lapse == 0))
    return 0;			/* no flood protection */
  /* okay, make sure i'm not flood-checking myself */
  if (match_my_nick(floodnick))
    return 0;
  if (!strcasecmp(floodhost, botuserhost))
    return 0;
  /* my user@host (?) */
  if ((which == FLOOD_KICK) || (which == FLOOD_DEOP))
    p = floodnick;
  else {
    p = strchr(floodhost, '@');
    if (p) {
      p++;
    }
    if (!p)
      return 0;
  }
  if (rfc_casecmp(chan->floodwho[which], p)) {	/* new */
    strncpy0(chan->floodwho[which], p, 81);
    chan->floodtime[which] = now;
    chan->floodnum[which] = 1;
    return 0;
  }
  if (chan->floodtime[which] < now - lapse) {
    /* flood timer expired, reset it */
    chan->floodtime[which] = now;
    chan->floodnum[which] = 1;
    return 0;
  }
  /* deop'n the same person, sillyness ;) - so just ignore it */
  Context;
  if (which == FLOOD_DEOP) {
    if (!rfc_casecmp(chan->deopd, victim))
      return 0;
    else
      strcpy(chan->deopd, victim);
  }
  chan->floodnum[which]++;
  Context;
  if (chan->floodnum[which] >= thr) {	/* FLOOD */
    /* reset counters */
    chan->floodnum[which] = 0;
    chan->floodtime[which] = 0;
    chan->floodwho[which][0] = 0;
    if (which == FLOOD_DEOP)
      chan->deopd[0] = 0;
    u = get_user_by_host(from);
    if (check_tcl_flud(floodnick, from, u, ftype, chan->name))
      return 0;
    switch (which) {
    case FLOOD_PRIVMSG:
    case FLOOD_NOTICE:
    case FLOOD_CTCP:
      /* flooding chan! either by public or notice */
      if (me_op(chan) && !chan_sentkick(m)) {
	log(LCAT_BOTMODE, STR("Channel flood from %s -- kicking"), floodnick);
	dprintf(DP_MODE, STR("KICK %s %s :%s%s\n"), chan->name, floodnick, kickprefix, kickreason(KICK_FLOOD));
	m->flags |= SENTKICK;
      }
      return 1;
    case FLOOD_JOIN:
    case FLOOD_NICK:
      simple_sprintf(h, STR("*!*@%s"), p);
      if (!isbanned(chan, h) && me_op(chan)) {

/*      add_mode(chan, '-', 'o', splitnick(&from)); *//* useless - arthur2 */
	do_mask(chan, chan->channel.ban, h, 'b');
      }
      if ((u_match_mask(global_bans, from))
	  || (u_match_mask(chan->bans, from)))
	return 1;		/* already banned */
      if (which == FLOOD_JOIN)
	log(LCAT_BOTMODE, STR("JOIN flood from @%s! Banning."), p);
      else
	log(LCAT_BOTMODE, STR("NICK flood from @%s! Banning."), p);
      strcpy(ftype + 4, STR(" flood"));
      u_addban(chan, h, origbotname, ftype, now + (60 * ban_time), 0);
      Context;
      /* don't kick user if exempted */
      if (!channel_enforcebans(chan) && me_op(chan)
	  && !isexempted(chan, h)) {
	char s[UHOSTLEN];

	m = chan->channel.member;

	while (m && m->nick[0]) {
	  sprintf(s, STR("%s!%s"), m->nick, m->userhost);
	  if (wild_match(h, s) && (m->joined >= chan->floodtime[which]) && !chan_sentkick(m) && !match_my_nick(m->nick)) {
	    m->flags |= SENTKICK;
	    dprintf(DP_SERVER, STR("KICK %s %s :%s%s\n"), chan->name, m->nick, kickprefix, kickreason(KICK_NICKFLOOD));

	  }
	  m = m->next;
	}
      }
      return 1;
    case FLOOD_KICK:
      if (me_op(chan) && !chan_sentkick(m)) {
	log(LCAT_BOTMODE, STR("Kicking %s, for mass kick."), floodnick);
	dprintf(DP_MODE, STR("KICK %s %s :%s%s\n"), chan->name, floodnick, kickprefix, kickreason(KICK_KICKFLOOD));
	m->flags |= SENTKICK;
      }
      return 1;
    case FLOOD_DEOP:
      if (me_op(chan) && !chan_sentkick(m)) {
	log(LCAT_BOTMODE, STR("Mass deop on %s by %s"), chan->name, from);
	dprintf(DP_MODE, STR("KICK %s %s :%s%s\n"), chan->name, floodnick, kickprefix, kickreason(KICK_MASSDEOP));
	
	m->flags |= SENTKICK;
      }
      if (u) {
	char s[256];
	sprintf(s, STR("Mass deop on %s by %s"), chan->name, from);
	deflag_user(u, DEFLAG_MDOP, s);
      }
      return 1;
    }
  }
  Context;
  return 0;
}

/* given a [nick!]user@host, place a quick ban on them on a chan */
char *quickban(struct chanset_t *chan, char *uhost)
{
  static char s1[512];

  maskhost(uhost, s1);
  if ((strlen(s1) != 1) && (strict_host == 0))
    s1[2] = '*';		/* arthur2 */
  do_mask(chan, chan->channel.ban, s1, 'b');
  return s1;
}

/* kicks any user (except friends/masters) with certain mask from channel
 * with a specified comment.  Ernst 18/3/1998 */
void kick_all(struct chanset_t *chan, char *hostmask, char *comment)
{
  memberlist *m;
  char kicknick[512],
    s[UHOSTLEN];

  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };
  int k,
    l,
    flushed;

  Context;
  if (!me_op(chan))
    return;
  k = 0;
  flushed = 0;
  kicknick[0] = 0;
  m = chan->channel.member;
  while (m && m->nick[0]) {
    get_user_flagrec(m->user, &fr, chan->name);
    sprintf(s, STR("%s!%s"), m->nick, m->userhost);
    if (!chan_sentkick(m) && wild_match(hostmask, s) && !match_my_nick(m->nick) && (!glob_bot(fr))) {
      if (!flushed) {
	/* we need to kick someone, flush eventual bans first */
	Context;
	flush_mode(chan, QUICK);
	flushed += 1;
      }
      m->flags |= SENTKICK;	/* mark as pending kick */
      if (kicknick[0])
	strcat(kicknick, ",");
      strcat(kicknick, m->nick);
      k += 1;
      l = strlen(chan->name) + strlen(kicknick) + strlen(comment) + 5;
      if ((kick_method != 0 && k == kick_method) || (l > 480)) {
	dprintf(DP_SERVER, STR("KICK %s %s :%s%s\n"), chan->name, kicknick, kickprefix, comment);
	k = 0;
	kicknick[0] = 0;
      }
    }
    m = m->next;
  }
  if (k > 0)
    dprintf(DP_SERVER, STR("KICK %s %s :%s%s\n"), chan->name, kicknick, kickprefix, comment);
  Context;
}

/* if any bans match this wildcard expression, refresh them on the channel */
void refresh_ban_kick(struct chanset_t *chan, char *user, char *nick)
{
  struct maskrec *u;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };
  memberlist *m;
  char c[512];			/* the ban comment */

  int cycle;

  for (cycle = 0; cycle < 2; cycle++) {
    if (cycle)
      u = chan->bans;
    else
      u = global_bans;
    for (; u; u = u->next) {
      if (wild_match(u->mask, user)) {
	m = ismember(chan, nick);
	if (!m)
	  continue;		/* skip non-existant nick */
	get_user_flagrec(m->user, &fr, chan->name);
	/*	if (!glob_friend(fr) && !chan_friend(fr))
	  add_mode(chan, '-', 'o', nick);    */
	do_mask(chan, chan->channel.ban, u->mask, 'b');
	u->lastactive = now;
	c[0] = 0;
	if (u->desc && (u->desc[0] != '@')) {
	  sprintf(c, STR("banned: %s"), u->desc);
	}
	kick_all(chan, u->mask, c[0] ? c : STR("banned"));
	return;			/* drop out on 1st ban */
      }
    }
  }
}

/* This is a bit cumbersome at the moment, but it works... Any improvements then 
 * feel free to have a go.. Jason */
void refresh_exempt(struct chanset_t *chan, char *user)
{
  struct maskrec *e;
  struct maskstruct *b;
  int cycle;

  for (cycle = 0; cycle < 2; cycle++) {
    e = (cycle) ? chan->exempts : global_exempts;

    while (e) {
      if (wild_match(user, e->mask) || wild_match(e->mask, user)) {
	b = chan->channel.ban;
	while (b && b->mask[0]) {
	  if (wild_match(b->mask, user) || wild_match(user, b->mask)) {
	    if (e->lastactive < now - 60 && !isexempted(chan, e->mask)) {
	      do_mask(chan, chan->channel.exempt, e->mask, 'e');
	      e->lastactive = now;
	      return;
	    }
	  }
	  b = b->next;
	}
      }
      e = e->next;
    }
  }
}

void refresh_invite(struct chanset_t *chan, char *user)
{
  struct maskrec *i;
  int cycle;

  for (cycle = 0; cycle < 2; cycle++) {
    i = (cycle) ? chan->invites : global_invites;

    while (i) {
      if (wild_match(i->mask, user) && (chan->channel.mode & CHANINV)) {
	if (i->lastactive < now - 60 && !isinvited(chan, i->mask)) {
	  do_mask(chan, chan->channel.invite, i->mask, 'I');
	  i->lastactive = now;
	  return;
	}
      }

      i = i->next;
    }
  }
}

/* enforce all channel bans in a given channel.  Ernst 18/3/1998 */
void enforce_bans(struct chanset_t *chan)
{
  char me[UHOSTLEN];
  struct maskstruct *b = chan->channel.ban;

  Context;
  if (!me_op(chan))
    return;			/* can't do it */
  simple_sprintf(me, STR("%s!%s"), botname, botuserhost);
  /* go through all bans, kicking the users */
  while (b && b->mask[0]) {
    if (!wild_match(b->mask, me))
      if (!isexempted(chan, b->mask))
	kick_all(chan, b->mask, STR("banned"));
    b = b->next;
  }
}

/* since i was getting a ban list, i assume i'm chop */

/* recheck_bans makes sure that all who are 'banned' on the userlist are
 * actually in fact banned on the channel */
void recheck_bans(struct chanset_t *chan)
{
  struct maskrec *u;
  int i;

  for (i = 0; i < 2; i++)
    for (u = i ? chan->bans : global_bans; u; u = u->next)
      if (!isbanned(chan, u->mask) && (!channel_dynamicbans(chan)))
	add_mode(chan, '+', 'b', u->mask);
}

/* recheck_exempts makes sure that all who are exempted on the userlist are
   actually in fact exempted on the channel */
void recheck_exempts(struct chanset_t *chan)
{
  struct maskrec *e;
  struct maskstruct *b;
  int i;

  for (i = 0; i < 2; i++) {
    for (e = i ? chan->exempts : global_exempts; e; e = e->next) {
      if (!isexempted(chan, e->mask) && (!channel_dynamicexempts(chan)))
	add_mode(chan, '+', 'e', e->mask);
      b = chan->channel.ban;
      while (b && b->mask[0]) {
	if ((wild_match(b->mask, e->mask) || wild_match(e->mask, b->mask))
	    && !isexempted(chan, e->mask))
	  add_mode(chan, '+', 'e', e->mask);
	b = b->next;
      }
    }
  }
}

/* recheck_invite */
void recheck_invites(struct chanset_t *chan)
{
  struct maskrec *ir;
  int i;

  for (i = 0; i < 2; i++) {
    for (ir = i ? chan->invites : global_invites; ir; ir = ir->next) {
      /* if invite isn't set and (channel is not dynamic invites and not invite
       * only) or invite is sticky */
      if (!isinvited(chan, ir->mask) && ((!channel_dynamicinvites(chan) && !(chan->channel.mode & CHANINV))))
	add_mode(chan, '+', 'I', ir->mask);
    }
  }
}

/*        Resets the masks on the channel. This is resetbans(), resetexempts()
 *      and resetinvites() all merged together for less bloat. <cybah>
 */
void resetmasks(struct chanset_t *chan, struct maskstruct *m, struct maskrec *mrec, struct maskrec *global_masks, char Mode)
{
  if (!me_op(chan))
    return;			/* can't do it */

  /* remove masks we didn't put there */
  while (m && m->mask[0]) {
    if (!u_equals_mask(global_masks, m->mask)
	&& !u_equals_mask(mrec, m->mask))
      add_mode(chan, '-', Mode, m->mask);

    m = m->next;
  }

  /* make sure the intended masks are still there */
  switch (Mode) {
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
    log(LCAT_ERROR, STR("Invalid mode '%c' in resetmasks()"), Mode);
    break;
  }
}

void enforce_bitch(struct chanset_t * chan) {
  priority_do(chan, 1, PRIO_DEOP);
}

/* things to do when i just became a chanop: */
void recheck_channel(struct chanset_t *chan, int dobans)
{
  memberlist *m;
  char s[UHOSTLEN];
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };
  int cur,
    pls,
    mns,
    botops=0, botnonops=0, nonbotops=0;
  static int stacking = 0;
  
  if (stacking)
    return;			/* wewps */
  if (!userlist)		/* bot doesnt know anybody */
    return;			/* it's better not to deop everybody */
  stacking++;
  /* okay, sort through who needs to be deopped. */
  log(LCAT_DEBUG, STR("recheck_channel %s"), chan->name);
  Context;
  for (m=chan->channel.member;m && m->nick[0];m=m->next) {
    if (!m->user) {
      sprintf(s, STR("%s!%s"), m->nick, m->userhost);
      m->user = get_user_by_host(s);
    }
    if (m && m->user) {
      if ((m->user->flags & (USER_BOT | USER_OP)) == (USER_BOT | USER_OP)) {
	if (chan_hasop(m))
	  botops++;
	else
	  botnonops++;
      } else if (chan_hasop(m)) {
	nonbotops++;
      }
    } else if (chan_hasop(m)) {
      nonbotops++;
    }
  }
#ifdef G_TAKE
  /* Massop if less than 4/5ths of the bots are opped, massdeop otherwise */
  if (channel_take(chan)) {
    if (botnonops && (((botops*5) / (botnonops + botops))<4)) {
      int actions=0;
      for (m=chan->channel.member;m && m->nick[0];m=m->next) {
	struct flag_record fr = {FR_CHAN | FR_GLOBAL, 0, 0, 0, 0};
	get_user_flagrec(m->user, &fr, chan->name);
	if (glob_bot(fr) && glob_op(fr) && !chan_hasop(m)) {
	  actions++;
	  add_mode(chan, '+', 'o', m->nick);
	  if (actions>=20) {
	    stacking--;
	    flush_mode(chan, QUICK);
	    return;
	  }
	}
      }
    }
    if (nonbotops) {
      enforce_bitch(chan);
      stacking--;
      return;
    }
  }
#endif
  /* don't do much if i'm lonely opped bot. Maybe they won't notice me? :P */
  if (botops==1) {
    stacking--;
    return;
  }
  if (channel_bitch(chan) || channel_locked(chan))
    enforce_bitch(chan);
  m = chan->channel.member;
  while (m && m->nick[0]) {
    sprintf(s, STR("%s!%s"), m->nick, m->userhost);
    get_user_flagrec(m->user, &fr, chan->name);
    /* ignore myself */
    if (!match_my_nick(m->nick)) {
      /* if channel user is current a chanop */
      if (chan_hasop(m)) {
	/* if user is channel deop */
	if (chan_deop(fr) ||
	    /* OR global deop and NOT channel op */
	    (glob_deop(fr) && !chan_op(fr))) {
	  /* de-op! */
	  if (target_priority(chan, m, 1)) {
	    add_mode(chan, '-', 'o', m->nick);
	  }
	}
      }
      /* now lets look at de-op'd ppl */
      /* now lets check 'em vs bans */
      /* if we're enforcing bans */
      if (channel_enforcebans(chan) &&
	  /* & they match a ban */
	  (u_match_mask(global_bans, s) || u_match_mask(chan->bans, s))) {
	/* bewm */
	refresh_ban_kick(chan, s, m->nick);
      }
      /* ^ will use the ban comment */
      if (u_match_mask(global_exempts, s) || u_match_mask(chan->exempts, s)) {
	refresh_exempt(chan, s);
      }
      /* check vs invites */
      if (u_match_mask(global_invites, s) || u_match_mask(chan->invites, s))
	refresh_invite(chan, s);
      /* are they +k ? */
      if (chan_kick(fr) || glob_kick(fr)) {
	quickban(chan, m->userhost);
	dprintf(DP_SERVER, STR("KICK %s %s :%s%s\n"), chan->name, m->nick, bankickprefix, kickreason(KICK_KUSER));
	m->flags |= SENTKICK;
      }
      if (channel_locked(chan))
	channel_check_locked(chan);
    }
    m = m->next;
  }
  if (dobans) {
    recheck_bans(chan);
    recheck_invites(chan);
    recheck_exempts(chan);
  }
  if (dobans && channel_enforcebans(chan))
    enforce_bans(chan);
  flush_mode(chan, QUICK);
  pls = chan->mode_pls_prot;
  mns = chan->mode_mns_prot;
  cur = chan->channel.mode;
  if (channel_locked(chan)) {
    pls |= CHANINV;
    mns &= ~CHANINV;
  }
  if (!(chan->status & CHAN_ASKEDMODES)) {
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
    if ((chan->limit_prot != (-1)) && (chan->channel.maxmembers == -1)) {
      sprintf(s, "%d", chan->limit_prot);
      add_mode(chan, '+', 'l', s);
    } else if ((mns & CHANLIMIT) && (chan->channel.maxmembers >= 0))
      add_mode(chan, '-', 'l', "");
    if (chan->key_prot[0]) {
      if (rfc_casecmp(chan->channel.key, chan->key_prot) != 0) {
	if (chan->channel.key[0])
	  add_mode(chan, '-', 'k', chan->channel.key);
	add_mode(chan, '+', 'k', chan->key_prot);
      }
    } else if ((mns & CHANKEY) && (chan->channel.key))
      add_mode(chan, '-', 'k', chan->channel.key);
  }
  if ((chan->status & CHAN_ASKEDMODES) && dobans && shouldjoin(chan))	
    /* spot on guppy, this just keeps the checking sane */
    dprintf(DP_SERVER, STR("MODE %s\n"), chan->name);
  stacking--;
}

/* got 324: mode status */

/* <server> 324 <to> <channel> <mode> */
int irc_got324(char *from, char *msg)
{
  int i = 1,
    ok = 0;
  char *p,
   *q,
   *chname;
  struct chanset_t *chan;

  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (!chan) {
    log(LCAT_ERROR, STR("%s: Unexpected mode"), chname);
    dprintf(DP_SERVER, STR("PART %s\n"), chname);
    return 0;
  }
  if (chan->status & CHAN_ASKEDMODES)
    ok = 1;
  chan->status &= ~CHAN_ASKEDMODES;
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
      if (p != NULL) {		/* test for null key assignment */
	p++;
	q = strchr(p, ' ');
	if (q != NULL) {
	  *q = 0;
	  set_key(chan, p);
	  strcpy(p, q + 1);
	} else {
	  set_key(chan, p);
	  *p = 0;
	}
      }
      if ((chan->channel.mode & CHANKEY) && !(chan->channel.key[0]))
	chan->status |= CHAN_ASKEDMODES;
    }
    if (msg[i] == 'l') {
      p = strchr(msg, ' ');
      if (p != NULL) {		/* test for null limit assignment */
	p++;
	q = strchr(p, ' ');
	if (q != NULL) {
	  *q = 0;
	  chan->channel.maxmembers = atoi(p);
	  strcpy(p, q + 1);
	} else {
	  chan->channel.maxmembers = atoi(p);
	  *p = 0;
	}
      }
    }
    i++;
  }
  if (ok)
    recheck_channel(chan, 0);
  return 0;
}

void memberlist_reposition(struct chanset_t * chan, memberlist * target) {
  /* Move target from it's current position to it's correct sorted position */
  memberlist *old = NULL, *m = NULL;
  if (chan->channel.member==target) {
    chan->channel.member=target->next;
  } else {
    for (m=chan->channel.member;m && m->nick[0];m=m->next) {
      if (m->next==target) {
	m->next=target->next;
	break;
      }
    }
  }
  target->next=NULL;
  for (m=chan->channel.member;m && m->nick[0];m=m->next) {
    if (rfc_casecmp(m->nick, target->nick)>0) {
      if (old) {
	target->next = m;
	old->next = target;
      } else {
	target->next=chan->channel.member;
	chan->channel.member = target;
      }
      return;
    }
    old=m;
  }
  if (old) {
    target->next = old->next;
    old->next=target;
  } else {
    target->next = NULL;
    chan->channel.member = target;
  }
}


int irc_got352or4(struct chanset_t *chan, char *user, char *host, char *serv, char *nick, char *flags)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };
  char userhost[UHOSTLEN];
  memberlist *m;
  int waschanop;

  m = ismember(chan, nick);	/* In my channel list copy? */
  if (!m) {			/* Nope, so update */
    m = newmember(chan, nick);	/* Get a new channel entry */
    m->joined = m->split = m->delay = 0L;	/* Don't know when he joined */
    m->flags = 0;		/* No flags for now */
    m->last = now;		/* Last time I saw him */
  }
  if (serv) {
    struct chanset_t * ch;
    memberlist * ml;
    strncpy0(m->server, serv, SERVLEN);
    /* Propagate server to other channel memlists... might save us a WHO #chan */
    for (ch=chanset;ch;ch=ch->next) {
      if (ch!=chan) {
	for (ml=ch->channel.member;ml && ml->nick[0];ml=ml->next) {
	  if (!strcmp(ml->nick, m->nick)) {
	    strcpy(ml->server, m->server);
	    break;
	  }
	}
      }
    }
  } else
    m->server[0] = 0;
  /* Store the userhost */
  simple_sprintf(m->userhost, STR("%s@%s"), user, host);
  simple_sprintf(userhost, STR("%s!%s"), nick, m->userhost);

  /* Combine n!u@h */
  m->user = NULL;		/* No handle match (yet) */
  if (match_my_nick(nick)) {	/* Is it me? */
    strcpy(botuserhost, m->userhost);	/* Yes, save my own userhost */
    m->joined = now;		/* set this to keep the whining masses happy */
  }
  waschanop = me_op(chan);	/* Am I opped here? */
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
  if (!(m->flags & (CHANVOICE | CHANOP)))
    m->flags |= STOPWHO;
  if (match_my_nick(nick) && !waschanop && me_op(chan))
    recheck_channel(chan, 1);
  if (match_my_nick(nick) && any_ops(chan) && !me_op(chan))
    chan->channel.do_opreq=1;
  
  m->user = get_user_by_host(userhost);
  get_user_flagrec(m->user, &fr, chan->name);
  /* are they a chanop, and me too */
  if (chan_hasop(m) && me_op(chan) &&
      /* are they a channel or global de-op */
      (chan_deop(fr) || (glob_deop(fr) && !chan_op(fr))) && 
      !match_my_nick(nick) && target_priority(chan, m, 1)) {
    add_mode(chan, '-', 'o', nick);
  }
  /* if channel is enforce bans */
  if (channel_enforcebans(chan) &&
      /* and user matches a ban */
      (u_match_mask(global_bans, userhost) || u_match_mask(chan->bans, userhost)) &&
      /* and it's not me, and i'm an op */
      !match_my_nick(nick) && me_op(chan)
      && target_priority(chan, m, 0)) {	
    dprintf(DP_SERVER, STR("KICK %s %s :%s%s\n"), chan->name, nick, bankickprefix, kickreason(KICK_BANNED));
    m->flags |= SENTKICK;
  }
  /* if the user is a +k */
  else if ((chan_kick(fr) || glob_kick(fr)) &&
	   /* and it's not me :) who'd set me +k anyway, a sicko? */
	   /* and if im an op */
	   !match_my_nick(nick) && me_op(chan) && target_priority(chan, m, 0)) {
    /* cya later! */
    quickban(chan, userhost);
    dprintf(DP_SERVER, STR("KICK %s %s :%s%s\n"), chan->name, nick, bankickprefix, kickreason(KICK_KUSER));
    m->flags |= SENTKICK;
  }
  return 0;
}

/* got a 352: who info! */
int irc_got352(char *from, char *msg)
{
  char *nick,
   *user,
   *host,
   *chname,
   *flags,
   *serv;
  struct chanset_t *chan;

  Context;
  newsplit(&msg);		/* Skip my nick - effeciently */
  chname = newsplit(&msg);	/* Grab the channel */
  chan = findchan(chname);	/* See if I'm on channel */
  if (chan) {			/* Am I? */
    user = newsplit(&msg);	/* Grab the user */
    host = newsplit(&msg);	/* Grab the host */
    serv = newsplit(&msg);	/* And the server */
    nick = newsplit(&msg);	/* Grab the nick */
    flags = newsplit(&msg);	/* Grab the flags */
    irc_got352or4(chan, user, host, serv, nick, flags);
  }
  return 0;
}

/* got a 354: who info! - iru style */
int irc_got354(char *from, char *msg)
{
  char *nick,
   *user,
   *host,
   *chname,
   *flags;
  struct chanset_t *chan;

  Context;
  if (use_354) {
    newsplit(&msg);		/* Skip my nick - effeciently */
    if (msg[0] && (strchr(CHANMETA, msg[0]) != NULL)) {
      chname = newsplit(&msg);	/* Grab the channel */
      chan = findchan(chname);	/* See if I'm on channel */
      if (chan) {		/* Am I? */
	user = newsplit(&msg);	/* Grab the user */
	host = newsplit(&msg);	/* Grab the host */
	nick = newsplit(&msg);	/* Grab the nick */
	flags = newsplit(&msg);	/* Grab the flags */
	irc_got352or4(chan, user, host, NULL, nick, flags);
      }
    }
  }
  return 0;
}

/* got 315: end of who */

/* <server> 315 <to> <chan> :End of /who */
int irc_got315(char *from, char *msg)
{
  char *chname;
  struct chanset_t *chan;

  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  /* may have left the channel before the who info came in */
  if (!chan || !channel_pending(chan))
    return 0;
  /* finished getting who list, can now be considered officially ON CHANNEL */
  chan->status |= CHAN_ACTIVE;
  chan->status &= ~CHAN_PEND;
  /* am *I* on the channel now? if not, well d0h. */
  if (!ismember(chan, botname)) {
    log(LCAT_ERROR, STR("Oops, I'm not really on %s"), chan->name);
    clear_channel(chan, 1);
    chan->status &= ~CHAN_ACTIVE;
    dprintf(DP_MODE, STR("JOIN %s %s\n"), chan->name, chan->key_prot);
  } else {
    if (me_op(chan))
      recheck_channel(chan, 0);
    else
      request_op(chan);
  }
  /* do not check for i-lines here. */
  return 0;
}

/* got 367: ban info */

/* <server> 367 <to> <chan> <ban> [placed-by] [timestamp] */
int irc_got367(char *from, char *msg)
{
  char s[UHOSTLEN],
   *ban,
   *who,
   *chname;
  struct chanset_t *chan;
  struct userrec *u;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };

  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (!chan || !(channel_pending(chan) || channel_active(chan)))
    return 0;
  ban = newsplit(&msg);
  who = newsplit(&msg);
  /* extended timestamp format? */
  if (who[0])
    newban(chan, ban, who);
  else
    newban(chan, ban, STR("existent"));
  simple_sprintf(s, STR("%s!%s"), botname, botuserhost);
  if (wild_match(ban, s))
    add_mode(chan, '-', 'b', ban);
  u = get_user_by_host(ban);
  if (u) {			/* why bother check no-user :) - of if Im not
				 * an op */
    get_user_flagrec(u, &fr, chan->name);
    if (chan_op(fr) || (glob_op(fr) && !chan_deop(fr)))
      add_mode(chan, '-', 'b', ban);
    /* these will be flushed by 368: end of ban info */
  }
  return 0;
}

/* got 368: end of ban list */

/* <server> 368 <to> <chan> :etc */
int irc_got368(char *from, char *msg)
{
  struct chanset_t *chan;
  char *chname;

  /* ok, now add bans that i want, which aren't set yet */
  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (chan) {
    chan->status &= ~CHAN_ASKEDBANS;
    if (channel_clearbans(chan))
      resetbans(chan);
    else {
      recheck_bans(chan);
    }
  }
  /* if i sent a mode -b on myself (deban) in got367, either */
  /* resetbans() or recheck_bans() will flush that */
  return 0;
}

/* got 348: ban exemption info */

/* <server> 348 <to> <chan> <exemption>  */
int irc_got348(char *from, char *msg)
{
  char *exempt,
   *who,
   *chname;
  struct chanset_t *chan;

  if (use_exempts == 0)
    return 0;
  newsplit(&msg);
  chname = newsplit(&msg);

  chan = findchan(chname);
  if (!chan || !(channel_pending(chan) || channel_active(chan)))
    return 0;
  exempt = newsplit(&msg);
  who = newsplit(&msg);
  /* extended timestamp format? */
  if (who[0])
    newexempt(chan, exempt, who);
  else
    newexempt(chan, exempt, STR("existent"));
  return 0;
}

/* got 349: end of ban exemption list */

/* <server> 349 <to> <chan> :etc */
int irc_got349(char *from, char *msg)
{
  struct chanset_t *chan;
  char *chname;

  if (use_exempts == 1) {
    newsplit(&msg);
    chname = newsplit(&msg);
    chan = findchan(chname);
    if (chan) {
      chan->ircnet_status &= ~CHAN_ASKED_EXEMPTS;
      if (channel_clearbans(chan))
	resetexempts(chan);
      else {
	recheck_exempts(chan);
      }
    }

  }
  return 0;
}

/* got 346: invite exemption info */

/* <server> 346 <to> <chan> <exemption> */
int irc_got346(char *from, char *msg)
{
  char *invite,
   *who,
   *chname;
  struct chanset_t *chan;

  if (use_invites == 0)
    return 0;
  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (!chan || !(channel_pending(chan) || channel_active(chan)))
    return 0;
  invite = newsplit(&msg);
  who = newsplit(&msg);
  /* extended timestamp format? */
  if (who[0])
    newinvite(chan, invite, who);
  else
    newinvite(chan, invite, STR("existent"));
  return 0;
}

/* got 347: end of invite exemption list */

/* <server> 347 <to> <chan> :etc */
int irc_got347(char *from, char *msg)
{
  struct chanset_t *chan;
  char *chname;

  if (use_invites == 1) {
    newsplit(&msg);
    chname = newsplit(&msg);
    chan = findchan(chname);
    if (chan) {
      chan->ircnet_status &= ~CHAN_ASKED_INVITED;
      if (channel_clearbans(chan))
	resetinvites(chan);
      else {
	recheck_invites(chan);
      }
    }
  }
  return 0;
}

/* too many channels */
int irc_got405(char *from, char *msg)
{
  char *chname;

  newsplit(&msg);
  chname = newsplit(&msg);
  log(LCAT_ERROR, STR("I'm on too many channels--can't join: %s"), chname);
  return 0;
}

/* got 471: can't join channel, full */
int irc_got471(char *from, char *msg)
{
  char *chname;
  struct chanset_t *chan;

  newsplit(&msg);
  chname = newsplit(&msg);
  log(LCAT_INFO, STR("Channel full--can't join: %s"), chname);
  chan = findchan(chname);
  if (chan)
    request_limit(chan);
  return 0;
}

/* got 473: can't join channel, invite only */
int irc_got473(char *from, char *msg)
{
  char *chname;
  struct chanset_t *chan;

  newsplit(&msg);
  chname = newsplit(&msg);
  log(LCAT_INFO, STR("Channel invite only--can't join: %s"), chname);
  chan = findchan(chname);
  if (chan)
    request_invite(chan);
  return 0;
}

/* got 474: can't join channel, banned */
int irc_got474(char *from, char *msg)
{
  char *chname;
  struct chanset_t *chan;

  newsplit(&msg);
  chname = newsplit(&msg);
  log(LCAT_INFO, STR("Banned from channel--can't join: %s"), chname);
  chan = findchan(chname);
  if (chan)
    request_unban(chan);
  return 0;
}

/* got 442: not on channel */
int irc_got442(char *from, char *msg)
{
  char *chname;
  struct chanset_t *chan;

  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (chan) {
    if (shouldjoin(chan)) {
      log(LCAT_ERROR, STR("Server says I'm not on channel: %s"), chname);
      clear_channel(chan, 1);
      chan->status &= ~CHAN_ACTIVE;
      dprintf(DP_MODE, STR("JOIN %s %s\n"), chan->name, chan->key_prot);
    }
  }
  return 0;
}

/* got 475: can't goin channel, bad key */
int irc_got475(char *from, char *msg)
{
  char *chname;
  struct chanset_t *chan;

  newsplit(&msg);
  chname = newsplit(&msg);
  log(LCAT_INFO, STR("Bad key--can't join: %s"), chname);
  chan = findchan(chname);
  if (chan)
    request_key(chan);
  return 0;
}

/* got invitation */
int irc_gotinvite(char *from, char *msg)
{
  char *nick;
  struct chanset_t *chan;

  newsplit(&msg);
  fixcolon(msg);
  nick = splitnick(&from);
  if (!rfc_casecmp(last_invchan, msg)) {
    if (now - last_invtime < 30)
      return 0;			/* two invites to the same channel in
				 * 30 seconds? */
  }
  log(LCAT_GETIN, STR("%s!%s invited me to %s"), nick, from, msg);
  strncpy0(last_invchan, msg, 299);
  last_invtime = now;
  chan = findchan(msg);
  if (chan && shouldjoin(chan)
      && !(channel_pending(chan)
	   || channel_active(chan)))
    dprintf(DP_MODE, STR("JOIN %s %s\n"), chan->name, chan->key_prot);
  return 0;
}

/* set the topic */
void set_topic(struct chanset_t *chan, char *k)
{
  if (chan->channel.topic)
    nfree(chan->channel.topic);
  if (k && k[0]) {
    chan->channel.topic = (char *) channel_malloc(strlen(k) + 1);
    strcpy(chan->channel.topic, k);
  } else
    chan->channel.topic = NULL;
}

/* topic change */
int irc_gottopic(char *from, char *msg)
{
  char *nick,
   *chname;
  memberlist *m;
  struct chanset_t *chan;
  struct userrec *u;

  chname = newsplit(&msg);
  fixcolon(msg);
  u = get_user_by_host(from);
  nick = splitnick(&from);
  chan = findchan(chname);
  if (chan) {
    log(LCAT_INFO, STR("Topic changed on %s by %s!%s: %s"), chname, nick, from, msg);
    m = ismember(chan, nick);
    if (m != NULL)
      m->last = now;
    set_topic(chan, msg);
    check_tcl_topc(nick, from, u, chname, msg);
  }
  return 0;
}

/* 331: no current topic for this channel */

/* <server> 331 <to> <chname> :etc */
int irc_got331(char *from, char *msg)
{
  char *chname;
  struct chanset_t *chan;

  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (chan) {
    set_topic(chan, NULL);
    check_tcl_topc("*", "*", NULL, chname, "");
  }
  return 0;
}

/* 332: topic on a channel i've just joined */

/* <server> 332 <to> <chname> :topic goes here */
int irc_got332(char *from, char *msg)
{
  struct chanset_t *chan;
  char *chname;

  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (chan) {
    fixcolon(msg);
    set_topic(chan, msg);
    check_tcl_topc("*", "*", NULL, chname, msg);
  }
  return 0;
}

void do_embedded_mode(struct chanset_t *chan, char *nick, memberlist * m, char *mode)
{
  struct flag_record fr = { 0, 0, 0, 0, 0 };
  int servidx = findanyidx(serv);

  while (*mode) {
    switch (*mode) {
    case 'o':
      check_tcl_mode(dcc[servidx].host, "", NULL, chan->name, "+o", nick);
      got_op(chan, "", dcc[servidx].host, nick, NULL, &fr);
      break;
    case 'v':
      check_tcl_mode(dcc[servidx].host, "", NULL, chan->name, "+v", nick);
      m->flags |= CHANVOICE;
      break;
    }
    mode++;
  }
  if (chan->channel.do_opreq)
    request_op(chan);
}

/* join */
int irc_gotjoin(char *from, char *chname)
{
  char *nick,
   *p,
   *newmode,
    buf[UHOSTLEN],
   *uhost = buf;
  int ok = 1;
  struct chanset_t *chan;
  memberlist *m;
  struct maskstruct *b,
   *e;
  struct userrec *u;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };

  Context;
  fixcolon(chname);
  /* ircd 2.9 sometimes does '#chname^Gmodes' when returning from splits */
  newmode = NULL;
  p = strchr(chname, 7);
  if (p != NULL) {
    *p = 0;
    newmode = (++p);
  }
  chan = findchan(chname);
  if (!chan || !shouldjoin(chan)) {
    log(LCAT_ERROR, STR("joined %s but didn't want to!"), chname);
    dprintf(DP_MODE, STR("PART %s\n"), chname);
  } else if (!channel_pending(chan)) {
    strcpy(uhost, from);
    nick = splitnick(&uhost);
    detect_chan_flood(nick, uhost, from, chan, FLOOD_JOIN, NULL);
    /* grab last time joined before we update it */
    u = get_user_by_host(from);
    get_user_flagrec(u, &fr, chname);
    if (!channel_active(chan) && !match_my_nick(nick)) {
      /* uh, what?!  i'm on the channel?! */
      log(LCAT_ERROR, STR("confused bot: guess I'm on %s and didn't realize it"), chname);
      chan->status |= CHAN_ACTIVE;
      chan->status &= ~CHAN_PEND;
      reset_chan_info(chan);
    } else {
      m = ismember(chan, nick);
      if (m && m->split && !strcasecmp(m->userhost, uhost)) {
	check_tcl_rejn(nick, uhost, u, chan->name);
	m->split = 0;
	m->last = now;
	m->delay = 0L;
	m->flags = (chan_hasop(m) ? WASOP : 0);
	m->user = u;
	set_handle_laston(chname, u, now);
	m->flags |= STOPWHO;
	if (newmode) {
	  log(LCAT_CHANNEL, STR("%s (%s) returned to %s (with +%s)."), nick, uhost, chname, newmode);
	  do_embedded_mode(chan, nick, m, newmode);
	} else
	  log(LCAT_CHANNEL, STR("%s (%s) returned to %s."), nick, uhost, chname);
      } else {
	if (m)
	  killmember(chan, nick);
	m = newmember(chan, nick);
	m->joined = now;
	m->split = 0L;
	m->flags = 0;
	m->last = now;
	m->delay = 0L;
	strcpy(m->userhost, uhost);
	m->user = u;
	m->flags |= STOPWHO;
	check_tcl_join(nick, uhost, u, chname);
	if (newmode)
	  do_embedded_mode(chan, nick, m, newmode);
	if (match_my_nick(nick)) {
	  /* it was me joining! */
	  if (newmode)
	    log(LCAT_CHANNEL, STR("%s joined %s (with +%s)."), nick, chname, newmode);
	  else
	    log(LCAT_CHANNEL, STR("%s joined %s."), nick, chname);
	  reset_chan_info(chan);
	} else {
	  struct chanuserrec *cr;

	  if (newmode)
	    log(LCAT_CHANNEL, STR("%s (%s) joined %s (with +%s)."), nick, uhost, chname, newmode);
	  else
	    log(LCAT_CHANNEL, STR("%s (%s) joined %s."), nick, uhost, chname);
	  /* don't re-display greeting if they've been on the channel
	   * recently */
	  if (u) {
	    struct laston_info *li = 0;

	    cr = get_chanrec(m->user, chan->name);
	    if (!cr && no_chanrec_info)
	      li = get_user(&USERENTRY_LASTON, m->user);
	  }
	  set_handle_laston(chname, u, now);
	}
      }
      /* ok, the op-on-join,etc, tests...first only both if Im opped */
      if (me_op(chan)) {
	for (p = m->userhost; *p; p++)
	  if (((unsigned char) *p) < 32) {
	    if (ban_bogus)
	      u_addban(chan, quickban(chan, uhost), origbotname, STR("bogus username"), now + (60 * ban_time), 0);
	    if (kick_bogus) {
	      dprintf(DP_MODE, STR("KICK %s %s :%s%s\n"), chname, nick, bankickprefix, kickreason(KICK_BOGUSUSERNAME));
	      m->flags |= SENTKICK;
	    }
	    if (kick_bogus || (ban_bogus && channel_enforcebans(chan)))
	      return 0;
	  }
	if (channel_enforcebans(chan) && !chan_op(fr) && !glob_op(fr)
	    && !glob_friend(fr) && !chan_friend(fr)) {
	  for (b = chan->channel.ban; b->mask[0]; b = b->next) {
	    if (wild_match(b->mask, from)) {
	      if (use_exempts)
		for (e = chan->channel.exempt; e->mask[0]; e = e->next)
		  if (wild_match(e->mask, from))
		    ok = 0;
	      if (ok && !chan_sentkick(m)) {
		dprintf(DP_SERVER, STR("KICK %s %s :%s%s\n"), chname, m->nick, bankickprefix, kickreason(KICK_BANNED));
		m->flags |= SENTKICK;
		return 0;
	      }
	    }
	  }
	}
	/* Check for and reset exempts and invites
	   * this will require further checking to account for when
	   * to use the various modes */
	if (u_match_mask(global_invites, from)
	    || u_match_mask(chan->invites, from))
	  refresh_invite(chan, from);
	/* if it matches a ban, dispose of them */
	if (u_match_mask(global_bans, from)
	    || u_match_mask(chan->bans, from)) {
	  refresh_ban_kick(chan, from, nick);
	  refresh_exempt(chan, from);
	  /* likewise for kick'ees */
	} else if (glob_kick(fr) || chan_kick(fr)) {
	  quickban(chan, from);
	  refresh_exempt(chan, from);
	  dprintf(DP_MODE, STR("KICK %s %s :%s%s\n"), chname, nick, bankickprefix, kickreason(KICK_KUSER));
	  m->flags |= SENTKICK;
	}
      }
    }
  }
  return 0;
}

/* part */
int irc_gotpart(char *from, char *chname)
{
  char *nick;
  struct chanset_t *chan;
  struct userrec *u;

  fixcolon(chname);
  chan = findchan(chname);
  if (chan && !shouldjoin(chan)) {
    clear_channel(chan, 1);
    chan->status &= ~(CHAN_ACTIVE | CHAN_PEND);
    return 0;
  }
  if (chan && !channel_pending(chan)) {
    u = get_user_by_host(from);
    nick = splitnick(&from);
    if (!channel_active(chan)) {
      /* whoa! */
      log(LCAT_ERROR, STR("confused bot: guess I'm on %s and didn't realize it"), chname);
      chan->status |= CHAN_ACTIVE;
      chan->status &= ~CHAN_PEND;
      reset_chan_info(chan);
    }
    check_tcl_part(nick, from, u, chname);
    set_handle_laston(chname, u, now);
    killmember(chan, nick);
    log(LCAT_CHANNEL, STR("%s (%s) left %s."), nick, from, chname);
    /* if it was me, all hell breaks loose... */
    if (match_my_nick(nick)) {
      clear_channel(chan, 1);
      chan->status &= ~(CHAN_ACTIVE | CHAN_PEND);
      if (shouldjoin(chan))
	dprintf(DP_MODE, STR("JOIN %s %s\n"), chan->name, chan->key_prot);
    } else
      check_lonely_channel(chan);
  }
  return 0;
}

/* kick */
int irc_gotkick(char *from, char *msg)
{
  char *nick,
   *whodid,
   *chname,
    s1[UHOSTLEN],
    buf[UHOSTLEN],
   *uhost = buf;
  memberlist *m;
  struct chanset_t *chan;
  struct userrec *u,
   *u2 = NULL;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };

  chname = newsplit(&msg);
  chan = findchan(chname);
  if (chan && channel_active(chan)) {
#ifdef G_AUTOLOCK
    chan->fighting++;
#endif
    nick = newsplit(&msg);
    fixcolon(msg);
    u = get_user_by_host(from);
    strcpy(uhost, from);
    whodid = splitnick(&uhost);
    detect_chan_flood(whodid, uhost, from, chan, FLOOD_KICK, nick);
    m = ismember(chan, whodid);
    if (m)
      m->last = now;
    check_tcl_kick(whodid, uhost, u, chname, nick, msg);
    get_user_flagrec(u, &fr, chname);
    m = ismember(chan, nick);
    if (m) {
      simple_sprintf(s1, STR("%s!%s"), m->nick, m->userhost);
      u2 = get_user_by_host(s1);
      set_handle_laston(chname, u2, now);
    }
    if (u && (u->flags & USER_BOT))
      log(LCAT_BOTMODE, STR("%s kicked from %s by %s: %s"), s1, chname, from, msg);
    else {
#ifdef G_MEAN
      if (u2 && (u2->flags & USER_BOT) && (channel_mean(chan))) {
	if (ROLE_KICK_MEAN) {
	  dprintf(DP_MODE, STR("KICK %s %s :%s%s\n"), chan->name, whodid, kickprefix, kickreason(KICK_MEAN));
	  if (u) {
	    char tmp[256];

	    sprintf(tmp, STR("%s!%s KICK %s %s :%s"), nick, from, chan->name, nick, msg);
	    deflag_user(u, DEFLAG_MEAN_KICK, tmp);
	  } else
	    log(LCAT_WARNING, STR("%s/%s kicked bot %s/%s on +mean channel %s"), whodid, u ? u->handle : "*", nick, u2->handle, chan->name);
	}
      }
#endif
      log(LCAT_USERMODE, STR("%s kicked from %s by %s: %s"), s1, chname, from, msg);
    }
    /* kicked ME?!? the sods! */
    if (match_my_nick(nick)) {
      chan->status &= ~(CHAN_ACTIVE | CHAN_PEND);
      dprintf(DP_MODE, STR("JOIN %s %s\n"), chan->name, chan->key_prot);
      clear_channel(chan, 1);
    } else {
      killmember(chan, nick);
      check_lonely_channel(chan);
    }
  }
  return 0;
}

/* nick change */
int irc_gotnick(char *from, char *msg)
{
  char *nick,
    s1[UHOSTLEN],
    buf[UHOSTLEN],
   *uhost = buf;
  memberlist *m,
   *mm;
  struct chanset_t *chan;
  struct userrec *u;

  u = get_user_by_host(from);
  strcpy(uhost, from);
  nick = splitnick(&uhost);
  fixcolon(msg);
  chan = chanset;
  while (chan) {
    m = ismember(chan, nick);
    if (m) {
      log(LCAT_CHANNEL, STR("Nick change: %s -> %s"), nick, msg);
      m->last = now;
      if (rfc_casecmp(nick, msg)) {
	/* not just a capitalization change */
	mm = ismember(chan, msg);
	if (mm) {
	  /* someone on channel with old nick?! */
	  if (mm->split)
	    log(LCAT_CHANNEL, STR("Possible future nick collision: %s"), mm->nick);
	  else
	    log(LCAT_ERROR, STR("* Bug: nick change to existing nick"));
	  killmember(chan, mm->nick);
	}
      }
      /* banned? */
      /* compose a nick!user@host for the new nick */
      sprintf(s1, STR("%s!%s"), msg, uhost);
      /* enforcing bans & haven't already kicked them? */
      if (channel_enforcebans(chan) && chan_sentkick(m) && (u_match_mask(global_bans, s1) || u_match_mask(chan->bans, s1)))
	refresh_ban_kick(chan, s1, msg);
      strcpy(m->nick, msg);
      memberlist_reposition(chan, m);
      detect_chan_flood(msg, uhost, from, chan, FLOOD_NICK, NULL);
      /* any pending kick to the old nick is lost. Ernst 18/3/1998 */
      if (chan_sentkick(m))
	m->flags &= ~SENTKICK;
      check_tcl_nick(nick, uhost, u, chan->name, msg);
    }
    chan = chan->next;
  }
  clear_chanlist();		/* cache is meaningless now */
  return 0;
}

#ifdef G_SPLITHIJACK
void check_should_cycle(struct chanset_t *chan)
{
  /* 
     If there are other ops split off and i'm the only op on this side of split, cycle
   */
  memberlist *ml;
  int localops = 0,
    localbotops = 0,
    splitops = 0,
    splitbotops = 0,
    localnonops = 0;

  if (!me_op(chan))
    return;
  ml = chan->channel.member;
  while (ml) {
    if (chan_hasop(ml)) {
      if (chan_issplit(ml)) {
	splitops++;
	if (ml->user && (ml->user->flags & USER_BOT))
	  splitbotops++;
      } else {
	localops++;
	if (ml->user && (ml->user->flags & USER_BOT))
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
    ml = ml->next;
  }
  if (splitbotops > 5) {
    /* I'm only one opped here... and other side has some ops... so i'm cycling */
    if (localnonops) {
      /* need to unset any +kil first */
      dprintf(DP_MODE, STR("MODE %s -ilk %s\nPART %s\nJOIN %s\n"), chan->name, chan->key ? chan->key : "", chan->name, chan->name);
    } else
      dprintf(DP_MODE, STR("PART %s\nJOIN %s\n"), chan->name, chan->name);
  }
}
#endif

#ifdef OLDCODE
#ifdef G_AUTOLOCK
void check_should_lock(struct chanset_t * chan) {
  /* If less than lock-threshold bots are opped in the chan,
     lock it */
  memberlist * ml;
  int oppedbots=0;
  if (!lock_threshold)
    return;
  for (ml=chan->channel.member;ml;ml=ml->next) {
    if (ml->user && (ml->user->flags & USER_BOT) && (ml->flags & CHANOP)) {
      oppedbots++;
      if (oppedbots >= lock_threshold)
	return;
    }
  }
  do_channel_modify(chan, STR("+locked chanmode +stni"));
#ifdef G_BACKUP
  chan->backup_time=now+30;
#endif
}
#endif
#endif

/* signoff, similar to part */
int irc_gotquit(char *from, char *msg)
{
  char *nick,
    *p;
  int split = 0;
  memberlist *m;
  struct chanset_t *chan;
  struct userrec *u;

  u = get_user_by_host(from);
  nick = splitnick(&from);
  fixcolon(msg);
  /* Fred1: instead of expensive wild_match on signoff, quicker method */
  /* determine if signoff string matches STR("%.% %.%"), and only one space */
  p = strchr(msg, ' ');
  if (p && (p == strrchr(msg, ' '))) {
    char *z1,
     *z2;

    *p = 0;
    z1 = strchr(p + 1, '.');
    z2 = strchr(msg, '.');
    if (z1 && z2 && (*(z1 + 1) != 0) && (z1 - 1 != p) && (z2 + 1 != p) && (z2 != msg)) {
      /* server split, or else it looked like it anyway */
      /* (no harm in assuming) */
      split = 1;
    } else
      *p = ' ';
  }
  for (chan = chanset; chan; chan = chan->next) {
    m = ismember(chan, nick);
    if (m) {
      set_handle_laston(chan->name, u, now);
      if (split) {
	m->split = now;
	check_tcl_splt(nick, from, u, chan->name);
	log(LCAT_CHANNEL, STR("%s (%s) got netsplit."), nick, from);
      } else {
	check_tcl_sign(nick, from, u, chan->name, msg);
	log(LCAT_CHANNEL, STR("%s (%s) left irc: %s"), nick, from, msg);
	killmember(chan, nick);
	check_lonely_channel(chan);
      }
#ifdef G_SPLITHIJACK
      check_should_cycle(chan);
#endif
    }
  }
  return 0;
}

/* private message */
int irc_gotmsg(char *from, char *msg)
{
  char *to,
   *realto,
    buf[UHOSTLEN],
   *nick,
    buf2[512],
   *uhost = buf;
  char *p,
   *p1,
   *code,
   *ctcp;
  int ctcp_count = 0;
  struct chanset_t *chan;
  int ignoring;
  struct userrec *u;
  memberlist *m;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };

  if (!strchr(STR("&#+@$"), msg[0]))
    return 0;
  ignoring = match_ignore(from);
  to = newsplit(&msg);
  realto = (to[0] == '@') ? to + 1 : to;
  chan = findchan(realto);
  if (!chan)
    return 0;			/* privmsg to an unknown channel ?? */
  fixcolon(msg);
  strcpy(uhost, from);
  nick = splitnick(&uhost);
  /* only check if flood-ctcp is active */
  if (flud_ctcp_thr && detect_avalanche(msg)) {
    u = get_user_by_host(from);
    get_user_flagrec(u, &fr, chan->name);
    /* discard -- kick user if it was to the channel */
    if (me_op(chan)) {
      m = ismember(chan, nick);
      if (m && !chan_sentkick(m)) {
	if (ban_fun)
	  u_addban(chan, quickban(chan, uhost), origbotname, STR("that was fun"), now + (60 * ban_time), 0);
	if (kick_fun) {
	  dprintf(DP_SERVER, STR("KICK %s %s :%sthat was fun\n"), realto, nick, kickprefix);	/* this can induce kickflood - arthur2 */
	  m->flags |= SENTKICK;
	}
      }
    }
    if (!ignoring) {
      log(LCAT_WARNING, STR("Avalanche from %s!%s in %s - ignoring"), nick, uhost, realto);
      p = strchr(uhost, '@');
      if (p)
	p++;
      else
	p = uhost;
      simple_sprintf(buf2, STR("*!*@%s"), p);
      addignore(buf2, origbotname, STR("ctcp avalanche"), now + (60 * ignore_time));
    }
    return 0;
  }
  /* check for CTCP: */
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
      strcpy(ctcp, p1);
      strcpy(p1 - 1, p + 1);
      detect_chan_flood(nick, uhost, from, chan, strncmp(ctcp, STR("ACTION "), 7) ? FLOOD_CTCP : FLOOD_PRIVMSG, NULL);
      /* respond to the first answer_ctcp */
      p = strchr(msg, 1);
      if (ctcp_count < answer_ctcp) {
	ctcp_count++;
	if (ctcp[0] != ' ') {
	  code = newsplit(&ctcp);
	  u = get_user_by_host(from);
	  if (!ignoring || trigger_on_ignore) {
	    if (!check_tcl_ctcp(nick, uhost, u, to, code, ctcp))
	      update_idle(realto, nick);
	    if (!ignoring) {
	      /* hell! log DCC, it's too a channel damnit! */
	      if (strcmp(code, STR("ACTION")) == 0) {
		log(LCAT_PUBLIC, STR("Action: %s %s"), nick, ctcp);
	      } else {
		log(LCAT_PUBLIC, STR("CTCP %s: %s from %s (%s) to %s"), code, ctcp, nick, from, to);
	      }
	    }
	  }
	}
      }
    }
  }
  /* send out possible ctcp responses */
  if (ctcp_reply[0]) {
    if (ctcp_mode != 2) {
      dprintf(DP_SERVER, STR("NOTICE %s :%s\n"), nick, ctcp_reply);
    } else {
      if (now - irc_last_ctcp > flud_ctcp_time) {
	dprintf(DP_SERVER, STR("NOTICE %s :%s\n"), nick, ctcp_reply);
	irc_count_ctcp = 1;
      } else if (irc_count_ctcp < flud_ctcp_thr) {
	dprintf(DP_SERVER, STR("NOTICE %s :%s\n"), nick, ctcp_reply);
	irc_count_ctcp++;
      }
      irc_last_ctcp = now;
    }
  }
  if (msg[0]) {

/* if (!ignoring) *//* removed by Eule 17.7.99 */
    detect_chan_flood(nick, uhost, from, chan, FLOOD_PRIVMSG, NULL);
    if (!ignoring || trigger_on_ignore) {
      if (check_tcl_pub(nick, uhost, realto, msg))
	return 0;
      check_tcl_pubm(nick, uhost, realto, msg);
    }
    if (!ignoring) {
      if (to[0] == '@')
	log(LCAT_PUBLIC, STR("@<%s> %s"), nick, msg);
      else
	log(LCAT_PUBLIC, STR("<%s> %s"), nick, msg);
    }
    update_idle(realto, nick);
  }
  return 0;
}

/* private notice */
int irc_gotnotice(char *from, char *msg)
{
  char *to,
   *realto,
   *nick,
    buf2[512],
   *p,
   *p1,
    buf[512],
   *uhost = buf;
  char *ctcp,
   *code;
  struct userrec *u;
  memberlist *m;
  struct chanset_t *chan;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };
  int ignoring;

  if (!strchr(CHANMETA "@", *msg))
    return 0;
  ignoring = match_ignore(from);
  to = newsplit(&msg);
  realto = (*to == '@') ? to + 1 : to;
  chan = findchan(realto);
  if (!chan)
    return 0;			/* notice to an unknown channel ?? */
  fixcolon(msg);
  strcpy(uhost, from);
  nick = splitnick(&uhost);
  if (flud_ctcp_thr && detect_avalanche(msg)) {
    u = get_user_by_host(from);
    get_user_flagrec(u, &fr, chan->name);
    /* discard -- kick user if it was to the channel */
    if (me_op(chan)) {
      m = ismember(chan, nick);
      if (m || !chan_sentkick(m)) {
	if (ban_fun)
	  u_addban(chan, quickban(chan, uhost), origbotname, STR("that was fun"), now + (60 * ban_time), 0);
	if (kick_fun) {
	  dprintf(DP_SERVER, STR("KICK %s %s :%sthat was fun\n"), realto, nick, bankickprefix);	/* this can induce kickflood - arthur2 */
	  m->flags |= SENTKICK;
	}
      }
    }
    if (!ignoring)
      log(LCAT_WARNING, STR("Avalanche from %s"), from);
    return 0;
  }
  /* check for CTCP: */
  p = strchr(msg, 1);
  while (p && *p) {
    p++;
    p1 = p;
    while ((*p != 1) && *p)
      p++;
    if (*p == 1) {
      *p = 0;
      ctcp = buf2;
      strcpy(ctcp, p1);
      strcpy(p1 - 1, p + 1);
      p = strchr(msg, 1);

/* if (!ignoring) *//* removed by Eule 17.7.99 */
      detect_chan_flood(nick, uhost, from, chan, strncmp(ctcp, STR("ACTION "), 7) ? FLOOD_CTCP : FLOOD_PRIVMSG, NULL);
      if (ctcp[0] != ' ') {
	code = newsplit(&ctcp);
	u = get_user_by_host(from);
	if (!ignoring || trigger_on_ignore) {
	  check_tcl_ctcr(nick, uhost, u, to, code, msg);
	  if (!ignoring) {
	    log(LCAT_PUBLIC, STR("CTCP reply %s: %s from %s (%s) to %s"), code, msg, nick, from, to);
	    update_idle(realto, nick);
	  }
	}
      }
    }
  }
  if (msg[0]) {

/* if (!ignoring) *//* removed by Eule 17.7.99 */
    detect_chan_flood(nick, uhost, from, chan, FLOOD_NOTICE, NULL);
    if (!ignoring)
      log(LCAT_PUBLIC, STR("-%s:%s- %s"), nick, to, msg);
    update_idle(realto, nick);
  }
  return 0;
}

/* update the add/rem_builtins in irc.c if you add to this list!! */
cmd_t irc_raw[] = {
  {"324", "", (Function) irc_got324, "irc:324"}
  ,
  {"352", "", (Function) irc_got352, "irc:352"}
  ,
  {"354", "", (Function) irc_got354, "irc:354"}
  ,
  {"315", "", (Function) irc_got315, "irc:315"}
  ,
  {"367", "", (Function) irc_got367, "irc:367"}
  ,
  {"368", "", (Function) irc_got368, "irc:368"}
  ,
  {"405", "", (Function) irc_got405, "irc:405"}
  ,
  {"471", "", (Function) irc_got471, "irc:471"}
  ,
  {"473", "", (Function) irc_got473, "irc:473"}
  ,
  {"474", "", (Function) irc_got474, "irc:474"}
  ,
  {"442", "", (Function) irc_got442, "irc:442"}
  ,
  {"475", "", (Function) irc_got475, "irc:475"}
  ,
  {"INVITE", "", (Function) irc_gotinvite, "irc:invite"}
  ,
  {"TOPIC", "", (Function) irc_gottopic, "irc:topic"}
  ,
  {"331", "", (Function) irc_got331, "irc:331"}
  ,
  {"332", "", (Function) irc_got332, "irc:332"}
  ,
  {"JOIN", "", (Function) irc_gotjoin, "irc:join"}
  ,
  {"PART", "", (Function) irc_gotpart, "irc:part"}
  ,
  {"KICK", "", (Function) irc_gotkick, "irc:kick"}
  ,
  {"NICK", "", (Function) irc_gotnick, "irc:nick"}
  ,
  {"QUIT", "", (Function) irc_gotquit, "irc:quit"}
  ,
  {"PRIVMSG", "", (Function) irc_gotmsg, "irc:msg"}
  ,
  {"NOTICE", "", (Function) irc_gotnotice, "irc:notice"}
  ,
  {"MODE", "", (Function) irc_gotmode, "irc:mode"}
  ,

/* Added for IRCnet +e/+I support - Daemus 2/4/1999 */
  {"346", "", (Function) irc_got346, "irc:346"}
  ,
  {"347", "", (Function) irc_got347, "irc:347"}
  ,
  {"348", "", (Function) irc_got348, "irc:348"}
  ,
  {"349", "", (Function) irc_got349, "irc:349"}
  ,
  {0, 0, 0, 0}
};

/* 
 * mode.c -- part of irc.mod
 *   queueing and flushing mode changes made by the bot
 *   channel mode changes and the bot's reaction to them
 *   setting and getting the current wanted channel modes
 * 
 * dprintf'ized, 12dec1995
 * multi-channel, 6feb1996
 * stopped the bot deopping masters and bots in bitch mode, pteron 23Mar1997
 * 
 * $Id: mode.c,v 1.36 2000/01/22 23:31:54 per Exp $
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

/* reversing this mode? */
int reversing = 0;

#define PLUS    1
#define MINUS   2
#define CHOP    4
#define BAN     8
#define VOICE   16
#define EXEMPT  32
#define INVITE  64

struct flag_record irc_user = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };

struct flag_record irc_victim = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };
void dequeue_op_deop(struct chanset_t * chan);

void flush_mode(struct chanset_t *chan, int pri)
{
  char *p,
    out[512],
    post[512];
  int i,
    ok = 0;
  dequeue_op_deop(chan);
  p = out;
  post[0] = 0;
  if (chan->pls[0])
    *p++ = '+';
  for (i = 0; i < strlen(chan->pls); i++)
    *p++ = chan->pls[i];
  if (chan->mns[0])
    *p++ = '-';
  for (i = 0; i < strlen(chan->mns); i++)
    *p++ = chan->mns[i];
  chan->pls[0] = 0;
  chan->mns[0] = 0;
  chan->bytes = 0;
  chan->compat = 0;
  ok = 0;
  /* +k or +l ? */
  if (chan->key[0]) {
    if (!ok) {
      *p++ = '+';
      ok = 1;
    }
    *p++ = 'k';
    strcat(post, chan->key);
    strcat(post, " ");
  }
  if (chan->limit != (-1)) {
    if (!ok) {
      *p++ = '+';
      ok = 1;
    }
    *p++ = 'l';
    sprintf(&post[strlen(post)], STR("%d "), chan->limit);
  }
  chan->limit = (-1);
  chan->key[0] = 0;
  /* do -b before +b to avoid server ban overlap ignores */
  for (i = 0; i < modesperline; i++)
    if ((chan->cmode[i].type & MINUS) && (chan->cmode[i].type & BAN)) {
      if (ok < 2) {
	*p++ = '-';
	ok = 2;
      }
      *p++ = 'b';
      strcat(post, chan->cmode[i].op);
      strcat(post, " ");
      nfree(chan->cmode[i].op);
      chan->cmode[i].op = NULL;
    }
  ok &= 1;
  for (i = 0; i < modesperline; i++)
    if (chan->cmode[i].type & PLUS) {
      if (!ok) {
	*p++ = '+';
	ok = 1;
      }
      *p++ = ((chan->cmode[i].type & BAN) ? 'b' : ((chan->cmode[i].type & CHOP) ? 'o' : ((chan->cmode[i].type & EXEMPT) ? 'e' : ((chan->cmode[i].type & INVITE) ? 'I' : 'v'))));
      strcat(post, chan->cmode[i].op);
      strcat(post, " ");
      nfree(chan->cmode[i].op);
      chan->cmode[i].op = NULL;
    }
  ok = 0;
  /* -k ? */
  if (chan->rmkey[0]) {
    if (!ok) {
      *p++ = '-';
      ok = 1;
    }
    *p++ = 'k';
    strcat(post, chan->rmkey);
    strcat(post, " ");
  }
  chan->rmkey[0] = 0;
  for (i = 0; i < modesperline; i++)
    if ((chan->cmode[i].type & MINUS) && !(chan->cmode[i].type & BAN)) {
      if (!ok) {
	*p++ = '-';
	ok = 1;
      }
      *p++ = ((chan->cmode[i].type & CHOP) ? 'o' : ((chan->cmode[i].type & EXEMPT) ? 'e' : ((chan->cmode[i].type & INVITE) ? 'I' : 'v')));
      strcat(post, chan->cmode[i].op);
      strcat(post, " ");
      nfree(chan->cmode[i].op);
      chan->cmode[i].op = NULL;
    }
  *p = 0;
  for (i = 0; i < modesperline; i++)
    chan->cmode[i].type = 0;
  if (post[0] && post[strlen(post) - 1] == ' ')
    post[strlen(post) - 1] = 0;
  if (post[0]) {
    strcat(out, " ");
    strcat(out, post);
  }
  if (out[0]) {
    if (pri == QUICK)
      dprintf(DP_MODE, STR("MODE %s %s\n"), chan->name, out);
    else
      dprintf(DP_SERVER, STR("MODE %s %s\n"), chan->name, out);
  }
}


void dequeue_op_deop(struct chanset_t * chan) {
  char lines[4096];
  char modechars[10];
  char nicks[128];
  int i=0, cnt=0;
  lines[0]=0;
  modechars[0]=0;
  nicks[0]=0;
  while ((i<20) && (chan->opqueue[i].target)) {
    strcat(nicks, " ");
    strcat(nicks, chan->opqueue[i].target);
    if (!modechars[0]) 
      strcat(modechars, " +");
    strcat(modechars, "o");
    cnt++;
    if (!(cnt % 4)) {
      strcat(lines, STR("MODE "));
      strcat(lines, chan->name);
      strcat(lines, modechars);
      strcat(lines, nicks);
      strcat(lines, "\n");
      modechars[0]=0;
      nicks[0]=0;
    }
    nfree(chan->opqueue[i].target);
    chan->opqueue[i].target = NULL;
    i++;
  }
  if (modechars[0] && chan->deopqueue[0].target)
    strcat(modechars, "-");
  i=0;
  while ((i<20) && (chan->deopqueue[i].target)) {
    strcat(nicks, " ");
    strcat(nicks, chan->deopqueue[i].target);
    if (!modechars[0]) 
      strcat(modechars, " -");
    strcat(modechars, "o");
    cnt++;
    if (!(cnt % 4)) {
      strcat(lines, STR("MODE "));
      strcat(lines, chan->name);
      strcat(lines, modechars);
      strcat(lines, nicks);
      strcat(lines, "\n");
      modechars[0]=0;
      nicks[0]=0;
      if (cnt>=24) {
	dprintf(DP_MODE, lines);
	lines[0]=0;
      }
    }
    nfree(chan->deopqueue[i].target);
    chan->deopqueue[i].target = NULL;
    i++;
  }
  if (cnt % 4) {
    strcat(lines, STR("MODE "));
    strcat(lines, chan->name);
    strcat(lines, modechars);
    strcat(lines, nicks);
    strcat(lines, "\n");
  }
  if (lines[0])
    dprintf(DP_MODE, lines);
}

void queue_op(struct chanset_t * chan, char * op) {
  int n;
  memberlist * m;
  for (n=0;n<20;n++) {
    if (!chan->opqueue[n].target) {
      chan->opqueue[n].target = nmalloc(strlen(op)+1);
      strcpy(chan->opqueue[n].target, op);
      m = ismember(chan, op);
      if (m) 
	m->flags |= SENTOP;
      if (n==19)
	dequeue_op_deop(chan);
      return;
    }
  }
}

void queue_deop(struct chanset_t * chan, char * op) {
  int n;
  memberlist * m;
  for (n=0;n<20;n++) {
    if (!chan->deopqueue[n].target) {
      chan->deopqueue[n].target = nmalloc(strlen(op)+1);
      strcpy(chan->deopqueue[n].target, op);
      m = ismember(chan, op);
      if (m) 
	m->flags |= SENTDEOP;
      if (n==19)
	dequeue_op_deop(chan);
      return;
    }
  }
}

/* queue a channel mode change */
void add_mode(struct chanset_t *chan, char plus, char mode, char *op)
{
  int i,
    type,
    ok,
    l;
  int bans,
    exempts,
    invites,
    modes;
  struct maskstruct *m;
  memberlist *mx;
  char s[21];

  Context;
  if (!me_op(chan))
    return;			/* no point in queueing the mode */
  if (mode == 'o') {
    if (plus=='+')
      queue_op(chan, op);
    else
      queue_deop(chan, op);
    return;
  }
  if (mode == 'v') {
    mx = ismember(chan, op);
    if (!mx)
      return;
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

  if ((mode == 'b') || (mode == 'v') || (mode == 'e') || (mode == 'I')) {
    type = (plus == '+' ? PLUS : MINUS) | (mode == 'o' ? CHOP : (mode == 'b' ? BAN : (mode == 'v' ? VOICE : (mode == 'e' ? EXEMPT : INVITE))));
    /* if -b'n a non-existant ban...nuke it */
    if ((plus == '-') && (mode == 'b'))

/* FIXME: some network remove overlapped bans, IrcNet doesnt (poptix/drummer)*/
      /*if (!isbanned(chan, op)) */
      if (!ischanban(chan, op))
	return;
    /* if there are already max_bans bans on the channel, don't try to add 
     * one more */
    Context;
    bans = 0;
    for (m = chan->channel.ban; m && m->mask[0]; m = m->next)
      bans++;
    Context;
    if ((plus == '+') && (mode == 'b'))
      if (bans >= max_bans) {
	return;
      }
    /* if there are already max_exempts exemptions on the channel, don't
     * try to add one more */
    Context;
    exempts = 0;
    for (m = chan->channel.exempt; m && m->mask[0]; m = m->next)
      exempts++;
    Context;
    if ((plus == '+') && (mode == 'e'))
      if (exempts >= max_exempts)
	return;
    /* if there are already max_invites invitations on the channel, don't
     * try to add one more */
    Context;
    invites = 0;
    for (m = chan->channel.invite; m && m->mask[0]; m = m->next)
      invites++;
    Context;
    if ((plus == '+') && (mode == 'I'))
      if (invites >= max_invites)
	return;
    /* if there are already max_modes +b/+e/+I modes on the channel, don't 
     * try to add one more */
    modes = bans + exempts + invites;
    if ((modes >= max_modes) && (plus == '+') && ((mode == 'b') || (mode == 'e') || (mode == 'I')))
      return;
    /* op-type mode change */
    for (i = 0; i < modesperline; i++)
      if ((chan->cmode[i].type == type) && (chan->cmode[i].op != NULL) && (!rfc_casecmp(chan->cmode[i].op, op)))
	return;			/* already in there :- duplicate */
    ok = 0;			/* add mode to buffer */
    l = strlen(op) + 1;
    if ((chan->bytes + l) > mode_buf_len)
      flush_mode(chan, NORMAL);
    for (i = 0; i < modesperline; i++)
      if ((chan->cmode[i].type == 0) && (!ok)) {
	chan->cmode[i].type = type;
	chan->cmode[i].op = (char *) channel_malloc(l);
	chan->bytes += l;	/* add 1 for safety */
	strcpy(chan->cmode[i].op, op);
	ok = 1;
      }
    ok = 0;			/* check for full buffer */
    for (i = 0; i < modesperline; i++)
      if (chan->cmode[i].type == 0)
	ok = 1;
    if (!ok)
      flush_mode(chan, NORMAL);	/* full buffer!  flush modes */
    if ((mode == 'b') && (plus == '+') && channel_enforcebans(chan))
      enforce_bans(chan);

/* recheck_channel(chan,0); */
    return;
  }
  /* +k ? store key */
  if ((plus == '+') && (mode == 'k'))
    strcpy(chan->key, op);
  /* -k ? store removed key */
  else if ((plus == '-') && (mode == 'k'))
    strcpy(chan->rmkey, op);
  /* +l ? store limit */
  else if ((plus == '+') && (mode == 'l'))
    chan->limit = atoi(op);
  else {
    /* typical mode changes */
    if (plus == '+')
      strcpy(s, chan->pls);
    else
      strcpy(s, chan->mns);
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
}

/**********************************************************************/

/* horrible code to parse mode changes */

/* no, it's not horrible, it just looks that way */

void got_key(struct chanset_t *chan, char *nick, char *from, char *key)
{
  int bogus = 0,
    i;
  memberlist *m;

  if ((!nick[0]) && (bounce_modes))
    reversing = 1;
  set_key(chan, key);
  for (i = 0; i < strlen(key); i++)
    if (((key[i] < 32) || (key[i] == 127)) && (key[i] != 2) && (key[i] != 31) && (key[i] != 22))
      bogus = 1;
  if (bogus && !match_my_nick(nick)) {
    m = ismember(chan, nick);
    if (m) {
      dprintf(DP_MODE, STR("KICK %s %s :%s%s\n"), chan->name, nick, kickprefix, kickreason(KICK_BOGUSKEY));
      m->flags |= SENTKICK;
      if ((m->user) && (m->user->flags & USER_BOT))
	log(LCAT_BOTMODE, STR("Bogus channel key on %s!"), chan->name);
      else
	log(LCAT_USERMODE, STR("Bogus channel key on %s!"), chan->name);
    }
  }
  if (((reversing) && !(chan->key_prot[0])) || (bogus) || ((chan->mode_mns_prot & CHANKEY) && !(glob_master(irc_user) || glob_bot(irc_user)
												|| chan_master(irc_user)))) {
    if (strlen(key) != 0) {
      add_mode(chan, '-', 'k', key);
    } else {
      add_mode(chan, '-', 'k', "");
    }
  }
}

void got_op(struct chanset_t *chan, char *nick, char *from, char *who, struct userrec *opu, struct flag_record *opper)
{
  memberlist *m;
  char s[UHOSTLEN];
  struct userrec *u;
  int check_chan = 0;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0 };
#ifdef G_AUTOLOCK
  chan->fighting++;
#endif
  m = ismember(chan, who);
  if (!m) {
    log(LCAT_ERROR, STR("* Mode change on %s for nonexistant %s!"), chan->name, who);
    dprintf(DP_MODE, STR("WHO %s\n"), who);
    return;
  }
  /* Did *I* just get opped? */
  if (!me_op(chan) && match_my_nick(who))
    check_chan = 1;

  if (!m->user) {
    simple_sprintf(s, STR("%s!%s"), m->nick, m->userhost);
    u = get_user_by_host(s);
  } else
    u = m->user;

  get_user_flagrec(u, &irc_victim, chan->name);
  /* flags need to be set correctly right from the beginning now, so that
   * add_mode() doesn't get irritated  (Fabian) */
  m->flags |= CHANOP;
  if (u) {
    get_user_flagrec(u, &fr, NULL);
    if (glob_bot(fr) && !me_op(chan))
      chan->channel.do_opreq=1;
  }
  check_tcl_mode(nick, from, opu, chan->name, "+o", who);
  /* added new meaning of WASOP:
   * in mode binds it means: was he op before get (de)opped
   * (stupid IrcNet allows opped users to be opped again and
   *  opless users to be deopped)
   * script now can use [wasop nick chan] proc to check
   * if user was op or wasnt  (drummer) */
  m->flags &= ~SENTOP;

  /* I'm opped, and the opper isn't me */
  if (me_op(chan) && !match_my_nick(who) &&
      nick[0]) {
    if ((channel_bitch(chan) || channel_locked(chan)
#ifdef G_TAKE
	 || channel_take(chan)
#endif
	 ) && !glob_op(irc_victim)
	&& !chan_op(irc_victim)) {
      if (target_priority(chan, m, 1))
	add_mode(chan, '-', 'o', who);
    } else if (chan_deop(irc_victim) || glob_deop(irc_victim))
      add_mode(chan, '-', 'o', who);
    else if (reversing)
      add_mode(chan, '-', 'o', who);
  } else if (reversing && !match_my_nick(who))
    add_mode(chan, '-', 'o', who);
  if (!nick[0] && me_op(chan) && !match_my_nick(who)) {
    if (chan_deop(irc_victim)
	|| (glob_deop(irc_victim) && !chan_op(irc_victim))) {
      m->flags |= FAKEOP;
      add_mode(chan, '-', 'o', who);
    }
  }
  m->flags |= WASOP;
  if (check_chan)
    recheck_channel(chan, 1);
}

void got_deop(struct chanset_t *chan, char *nick, char *from, char *who, struct userrec *opu)
{
  memberlist *m;
  char s[UHOSTLEN],
    s1[UHOSTLEN];
  struct userrec *u;
  int had_op; 
#ifdef G_MEAN
  int shoulddeflag = 0;
  char tmp[256];
#endif
#ifdef G_AUTOLOCK
  chan->fighting++;
#endif
  m = ismember(chan, who);
  if (!m) {
    log(LCAT_ERROR, STR("* Mode change on %s for nonexistant %s!"), chan->name, who);
    dprintf(DP_MODE, STR("WHO %s\n"), who);
    return;
  }

  simple_sprintf(s, STR("%s!%s"), m->nick, m->userhost);
  simple_sprintf(s1, STR("%s!%s"), nick, from);
  u = get_user_by_host(s);
  get_user_flagrec(u, &irc_victim, chan->name);

  had_op = chan_hasop(m);
  /* flags need to be set correctly right from the beginning now, so that
   * add_mode() doesn't get irritated  (Fabian) */
  m->flags &= ~(CHANOP | SENTDEOP | FAKEOP);
#ifdef G_MEAN
  if (channel_mean(chan) && u && (u->flags & USER_BOT)
      && (!opu || !(opu->flags & USER_BOT))) {
    if (ROLE_KICK_MEAN) {
      dprintf(DP_MODE, STR("KICK %s %s :%s%s\n"), chan->name, nick, kickprefix, kickreason(KICK_MEAN));
      if (opu) {
	sprintf(tmp, STR("%s MODE %s -o %s"), s1, chan->name, m->nick);
	shoulddeflag = 1;
      } else
	log(LCAT_WARNING, STR("%s/%s deopped bot %s/%s on +mean channel %s"), nick, opu ? opu->handle : "*", m->nick, u->handle, chan->name);
    }
  }
#endif
  check_tcl_mode(nick, from, opu, chan->name, "-o", who);
  /* check comments in got_op()  (drummer) */
  m->flags &= ~WASOP;

  /* deop'd someone on my oplist? */
  if (me_op(chan)) {
    int ok = 1;

    if (glob_master(irc_victim) || chan_master(irc_victim))
      ok = 0;
    else if ((glob_op(irc_victim) || glob_friend(irc_victim))
	     && !chan_deop(irc_victim))
      ok = 0;
    else if (chan_op(irc_victim) || chan_friend(irc_victim))
      ok = 0;
    if (!ok && !match_my_nick(nick) && rfc_casecmp(who, nick) && had_op && !match_my_nick(who)) {	/* added 25mar1996, robey */
      /* reop? NEVER! (einie) */
      if (reversing)
	add_mode(chan, '+', 'o', who);
    }
  }

  if (!nick[0])
    log(LCAT_USERMODE, STR("TS resync (%s): %s deopped by %s"), chan->name, who, from);
  /* check for mass deop */
  if (nick[0])
    detect_chan_flood(nick, from, s1, chan, FLOOD_DEOP, who);
  /* having op hides your +v status -- so now that someone's lost ops,
   * check to see if they have +v */
  if (!(m->flags & (CHANVOICE | STOPWHO))) {
    dprintf(DP_HELP, STR("WHO %s\n"), m->nick);
    m->flags |= STOPWHO;
  }
  /* was the bot deopped? */
  if (match_my_nick(who)) {
    /* cancel any pending kicks and modes */
    memberlist *m2 = chan->channel.member;

    while (m2 && m2->nick[0]) {
      m2->flags &= ~(SENTKICK | SENTDEOP | SENTOP | SENTVOICE | SENTDEVOICE);
      m2 = m2->next;
    }
    chan->channel.do_opreq=1;
    if (!nick[0])
      log(LCAT_USERMODE, STR("TS resync deopped me on %s :("), chan->name);
  }
#ifdef G_MEAN
  if (shoulddeflag)
    deflag_user(opu, DEFLAG_MEAN_DEOP, tmp);	
#endif
}

void got_ban(struct chanset_t *chan, char *nick, char *from, char *who)
{
  char me[UHOSTLEN],
    s[UHOSTLEN],
    s1[UHOSTLEN];
  int check;
  memberlist *m;
  struct userrec *u;
#ifdef G_AUTOLOCK
  chan->fighting++;
#endif
  simple_sprintf(me, STR("%s!%s"), botname, botuserhost);
  simple_sprintf(s, STR("%s!%s"), nick, from);
  newban(chan, who, s);
  check = 1;
  if (wild_match(who, me) && me_op(chan)) {
    /* First of all let's check whether some luser banned us ++rtc */
    if (match_my_nick(nick)) {
      /* Bot banned itself -- doh! ++rtc */
      log(LCAT_ERROR, STR("Uh, banned myself on %s, reversing..."), chan->name);
    }
    reversing = 1;
    check = 0;
  } else if (!match_my_nick(nick)) {	/* it's not my ban */
    struct userrec * u2 = NULL;
    m = chan->channel.member;
    while (m && m->nick[0]) {
      sprintf(s1, STR("%s!%s"), m->nick, m->userhost);
      if (wild_match(who, s1)) {
	u2 = get_user_by_host(s1);
	if (u2 && (u2->flags & USER_BOT))
	  break;
      }
      u2=NULL;
      m=m->next;
    }
#ifdef G_MEAN
    if (channel_mean(chan) && !glob_bot(irc_user) && u2) {
      if (ROLE_KICK_MEAN) {
	dprintf(DP_MODE, STR("KICK %s %s :%s%s\n"), chan->name, nick, kickprefix, kickreason(KICK_MEAN));
	u = get_user_by_host(from);
	if (u) {
	  char tmp[256];
	  sprintf(tmp, STR("%s MODE %s %s"), from, chan->name, who);
	  deflag_user(u, DEFLAG_MEAN_BAN, tmp);
	  return;
	} else {
	  log(LCAT_WARNING, STR("%s banned a bot on +mean channel %s"), nick, chan->name);
	}
      }
    }
#endif
    if (channel_nouserbans(chan) && nick[0] && !glob_bot(irc_user) && !glob_master(irc_user) && !chan_master(irc_user)) {
      /* no bans made by users */
      add_mode(chan, '-', 'b', who);
      return;
    }
    /* don't enforce a server ban right away -- give channel users a chance
     * to remove it, in case it's fake */
    if (!nick[0]) {
      check = 0;
      if (bounce_modes)
	reversing = 1;
    }
    /* does this remotely match against any of our hostmasks? */
    /* just an off-chance... */
    u = get_user_by_host(who);
    if (u) {
      get_user_flagrec(u, &irc_victim, chan->name);
      if (glob_friend(irc_victim)
	  || (glob_op(irc_victim) && !chan_deop(irc_victim))
	  || chan_friend(irc_victim) || chan_op(irc_victim)) {
	if (!glob_master(irc_user) && !glob_bot(irc_user)
	    && !chan_master(irc_user))

/* reversing = 1; *//* arthur2 - 99/05/31 */
	  check = 0;
	if (glob_master(irc_victim) || chan_master(irc_victim))
	  check = 0;
      }
    } else {
      /* banning an oplisted person who's on the channel? */
      m = chan->channel.member;
      while (m && m->nick[0]) {
	sprintf(s1, STR("%s!%s"), m->nick, m->userhost);
	if (wild_match(who, s1)) {
	  u = get_user_by_host(s1);
	  if (u) {
	    get_user_flagrec(u, &irc_victim, chan->name);
	    if (glob_friend(irc_victim) || (glob_op(irc_victim) && !chan_deop(irc_victim))
		|| chan_friend(irc_victim) || chan_op(irc_victim)) {
	      /* remove ban on +o/f/m user, unless placed by another +m/b */
	      if (!glob_master(irc_user) && !glob_bot(irc_user) && !chan_master(irc_user)) {
		if (!isexempted(chan, s1)) {
		  /* Crotale - if the irc_victim is not +e, then ban is removed */
		  if (target_priority(chan, m, 0))
		    add_mode(chan, '-', 'b', who);
		  check = 0;
		}
	      }
	      if (glob_master(irc_victim) || chan_master(irc_victim))
		check = 0;
	    }
	  }
	}
	m = m->next;
      }
    }
  }
  /* If a ban is set on an exempted user then we might as well set exemption
   * at the same time */
  refresh_exempt(chan, who);
  /* if ((u_equals_exempt(global_exempts,who) || u_equals_exempt(chan->exempts, who))) {
   * add_mode(chan, '+', 'e' , who);
   *} */
  if (check && channel_enforcebans(chan))
    kick_all(chan, who, STR("banned"));
  /* is it a server ban from nowhere? */
  if (reversing || (bounce_bans && (!nick[0]) && (!u_equals_mask(global_bans, who) || !u_equals_mask(chan->bans, who)) && (check)))
    add_mode(chan, '-', 'b', who);
}

void got_unban(struct chanset_t *chan, char *nick, char *from, char *who, struct userrec *u)
{
  struct maskstruct *b,
   *old;
#ifdef G_AUTOLOCK
  chan->fighting++;
#endif
  b = chan->channel.ban;
  old = NULL;
  while (b->mask[0] && rfc_casecmp(b->mask, who)) {
    old = b;
    b = b->next;
  }
  if (b->mask[0]) {
    if (old)
      old->next = b->next;
    else
      chan->channel.ban = b->next;
    nfree(b->mask);
    nfree(b->who);
    nfree(b);
  }
  if ((u_equals_mask(global_bans, who) || u_equals_mask(chan->bans, who)) && me_op(chan) && !channel_dynamicbans(chan)) {
    /* that's a permban! */
    if (glob_bot(irc_user)) {
      /* sharebot -- do nothing */
    } else if ((glob_op(irc_user) && !chan_deop(irc_user))
	       || chan_op(irc_user)) {
      dprintf(DP_HELP, STR("NOTICE %s :%s is permbanned"), nick, who);
    } else
      add_mode(chan, '+', 'b', who);
  }
}

void got_exempt(struct chanset_t *chan, char *nick, char *from, char *who)
{
  int check;
  char me[UHOSTLEN],
    s[UHOSTLEN];

  Context;
  simple_sprintf(me, STR("%s!%s"), botname, botuserhost);
  simple_sprintf(s, STR("%s!%s"), nick, from);
  newexempt(chan, who, s);
  check = 1;
  if (!match_my_nick(nick)) {	/* it's not my exemption */
    if (channel_nouserexempts(chan) && nick[0] && !glob_bot(irc_user) && !glob_master(irc_user) && !chan_master(irc_user)) {
      /* no exempts made by users */
      add_mode(chan, '-', 'e', who);
      return;
    }
    if ((!nick[0]) && (bounce_modes))
      reversing = 1;
  }
  if (reversing || (bounce_exempts && !nick[0] && (!u_equals_mask(global_exempts, who)
						   || !u_equals_mask(chan->exempts, who))
		    && (check)))
    add_mode(chan, '-', 'e', who);
}

void got_unexempt(struct chanset_t *chan, char *nick, char *from, char *who, struct userrec *u)
{
  struct maskstruct *e,
   *old;
  struct maskstruct *b;
  int match = 0;

  Context;
  e = chan->channel.exempt;
  old = NULL;
  while (e && e->mask[0] && rfc_casecmp(e->mask, who)) {
    old = e;
    e = e->next;
  }
  if (e && e->mask[0]) {
    if (old)
      old->next = e->next;
    else
      chan->channel.exempt = e->next;
    nfree(e->mask);
    nfree(e->who);
    nfree(e);
  }
  /* if exempt was removed by master then leave it else check for bans */
  if (!nick[0] && glob_bot(irc_user) && !glob_master(irc_user)
      && !chan_master(irc_user)) {
    b = chan->channel.ban;
    while (b->mask[0] && !match) {
      if (wild_match(b->mask, who) || wild_match(who, b->mask)) {
	add_mode(chan, '+', 'e', who);
	match = 1;
      } else
	b = b->next;
    }
  }
  if ((u_equals_mask(global_exempts, who)
       || u_equals_mask(chan->exempts, who)) && me_op(chan)
      && !channel_dynamicexempts(chan)) {
    /* that's a permexempt! */
    if (glob_bot(irc_user)) {
      /* sharebot -- do nothing */
    } else
      add_mode(chan, '+', 'e', who);
  }
}

void got_invite(struct chanset_t *chan, char *nick, char *from, char *who)
{
  char me[UHOSTLEN],
    s[UHOSTLEN];
  int check;

  simple_sprintf(me, STR("%s!%s"), botname, botuserhost);
  simple_sprintf(s, STR("%s!%s"), nick, from);
  newinvite(chan, who, s);
  check = 1;
  if (!match_my_nick(nick)) {	/* it's not my invitation */
    if (channel_nouserinvites(chan) && nick[0] && !glob_bot(irc_user) && !glob_master(irc_user) && !chan_master(irc_user)) {
      /* no exempts made by users */
      add_mode(chan, '-', 'I', who);
      return;
    }
    if ((!nick[0]) && (bounce_modes))
      reversing = 1;
  }
  if (reversing || (bounce_invites && (!nick[0]) && (!u_equals_mask(global_invites, who)
						     || !u_equals_mask(chan->invites, who))
		    && (check)))
    add_mode(chan, '-', 'I', who);
}

void got_uninvite(struct chanset_t *chan, char *nick, char *from, char *who, struct userrec *u)
{
  struct maskstruct *inv,
   *old;

  inv = chan->channel.invite;
  old = NULL;
  while (inv->mask[0] && rfc_casecmp(inv->mask, who)) {
    old = inv;
    inv = inv->next;
  }
  if (inv->mask[0]) {
    if (old)
      old->next = inv->next;
    else
      chan->channel.invite = inv->next;
    nfree(inv->mask);
    nfree(inv->who);
    nfree(inv);
  }
  if (!nick[0] && glob_bot(irc_user) && !glob_master(irc_user)
      && !chan_master(irc_user)
      && (chan->channel.mode & CHANINV))
    add_mode(chan, '+', 'I', who);
  if ((u_equals_mask(global_invites, who)
       || u_equals_mask(chan->invites, who)) && me_op(chan)
      && !channel_dynamicinvites(chan)) {
    /* that's a perminvite! */
    if (glob_bot(irc_user)) {
      /* sharebot -- do nothing */
    } else
      add_mode(chan, '+', 'I', who);
  }
}

/* a pain in the ass: mode changes */
void irc_gotmode(char *from, char *msg)
{
  char *nick,
   *ch,
   *op,
   *chg;
  char s[UHOSTLEN];
  char ms2[3];
  int z;
  struct userrec *u;
  memberlist *m;
  struct chanset_t *chan;

  /* usermode changes? */
  if (msg[0] && (strchr(CHANMETA, msg[0]) != NULL)) {
    ch = newsplit(&msg);
    chg = newsplit(&msg);
    reversing = 0;
    chan = findchan(ch);
    if (!chan) {
      log(LCAT_INFO, STR("Oops. Someone made me join %s... leaving..."), ch);
      dprintf(DP_SERVER, STR("PART %s\n"), ch);
    } else if (!channel_pending(chan)) {
      z = strlen(msg);
      if (msg[--z] == ' ')	/* i hate cosmetic bugs :P -poptix */
	msg[z] = 0;
      u = get_user_by_host(from);
      get_user_flagrec(u, &irc_user, ch);
      if (glob_bot(irc_user))
	log(LCAT_BOTMODE, STR("%s: mode change '%s %s' by %s"), ch, chg, msg, from);
      else
	log(LCAT_USERMODE, STR("%s: mode change '%s %s' by %s"), ch, chg, msg, from);
      nick = splitnick(&from);
      m = ismember(chan, nick);
      if (m)
	m->last = now;
      ms2[0] = '+';
      ms2[2] = 0;
      while ((ms2[1] = *chg)) {
	int todo = 0;

	switch (*chg) {
	case '+':
	  ms2[0] = '+';
	  break;
	case '-':
	  ms2[0] = '-';
	  break;
	case 'i':
	  todo = CHANINV;
	  if ((!nick[0]) && (bounce_modes))
	    reversing = 1;
	  break;
	case 'p':
	  todo = CHANPRIV;
	  if ((!nick[0]) && (bounce_modes))
	    reversing = 1;
	  break;
	case 's':
	  todo = CHANSEC;
	  if ((!nick[0]) && (bounce_modes))
	    reversing = 1;
	  break;
	case 'm':
	  todo = CHANMODER;
	  if ((!nick[0]) && (bounce_modes))
	    reversing = 1;
	  break;
	case 't':
	  todo = CHANTOPIC;
	  if ((!nick[0]) && (bounce_modes))
	    reversing = 1;
	  break;
	case 'n':
	  todo = CHANNOMSG;
	  if ((!nick[0]) && (bounce_modes))
	    reversing = 1;
	  break;
	case 'a':
	  todo = CHANANON;
	  if ((!nick[0]) && (bounce_modes))
	    reversing = 1;
	  break;
	case 'q':
	  todo = CHANQUIET;
	  if ((!nick[0]) && (bounce_modes))
	    reversing = 1;
	  break;
	case 'l':
	  if ((!nick[0]) && (bounce_modes))
	    reversing = 1;
	  if (ms2[0] == '-') {
	    check_tcl_mode(nick, from, u, chan->name, ms2, "");
	    if ((reversing) && (chan->channel.maxmembers != (-1))) {
	      simple_sprintf(s, "%d", chan->channel.maxmembers);
	      add_mode(chan, '+', 'l', s);
	    } else if ((chan->limit_prot != (-1)) && !glob_master(irc_user) && !chan_master(irc_user)) {
	      simple_sprintf(s, "%d", chan->limit_prot);
	      add_mode(chan, '+', 'l', s);
	    }
	    chan->channel.maxmembers = (-1);
	  } else {
	    op = newsplit(&msg);
	    fixcolon(op);
	    if (op == '\0') {
	      break;
	    }
	    chan->channel.maxmembers = atoi(op);
	    check_tcl_mode(nick, from, u, chan->name, ms2, int_to_base10(chan->channel.maxmembers));
	    if (((reversing) && !(chan->mode_pls_prot & CHANLIMIT)) || ((chan->mode_mns_prot & CHANLIMIT) && !glob_master(irc_user) && !chan_master(irc_user))) {
	      if (chan->channel.maxmembers == 0)
		add_mode(chan, '+', 'l', "23");	/* wtf? 23 ??? */
	      add_mode(chan, '-', 'l', "");
	    }
	    if ((chan->limit_prot != chan->channel.maxmembers) && (chan->mode_pls_prot & CHANLIMIT) && (chan->limit_prot != (-1)) &&	/* arthur2 */
		!glob_master(irc_user) && !chan_master(irc_user)) {
	      simple_sprintf(s, "%d", chan->limit_prot);
	      add_mode(chan, '+', 'l', s);
	    }
	  }
	  break;
	case 'k':
	  if (ms2[0] == '+')
	    chan->channel.mode |= CHANKEY;
	  else
	    chan->channel.mode &= ~CHANKEY;
	  op = newsplit(&msg);
	  fixcolon(op);
	  if (op == '\0') {
	    break;
	  }
	  if (ms2[0] == '+')
	    got_key(chan, nick, from, op);
	  else {
	    if ((reversing) && (chan->channel.key[0]))
	      add_mode(chan, '+', 'k', chan->channel.key);
	    else if ((chan->key_prot[0]) && !glob_master(irc_user)
		     && !chan_master(irc_user))
	      add_mode(chan, '+', 'k', chan->key_prot);
	    set_key(chan, NULL);
	  }
	  break;
	case 'o':
	  op = newsplit(&msg);
	  fixcolon(op);
	  if (ms2[0] == '+')
	    got_op(chan, nick, from, op, u, &irc_user);
	  else
	    got_deop(chan, nick, from, op, u);
	  break;
	case 'v':
	  op = newsplit(&msg);
	  fixcolon(op);
	  m = ismember(chan, op);
	  if (!m) {
	    log(LCAT_USERMODE, STR("* Mode change on %s for nonexistant %s!"), chan->name, op);
	    dprintf(DP_MODE, STR("WHO %s\n"), op);
	  } else {
	    get_user_flagrec(m->user, &irc_victim, chan->name);
	    if (ms2[0] == '+') {
	      m->flags &= ~SENTVOICE;
	      m->flags |= CHANVOICE;
	      check_tcl_mode(nick, from, u, chan->name, ms2, op);
	    } else {
	      m->flags &= ~SENTDEVOICE;
	      m->flags &= ~CHANVOICE;
	      check_tcl_mode(nick, from, u, chan->name, ms2, op);
	    }
	  }
	  break;
	case 'b':
	  op = newsplit(&msg);
	  fixcolon(op);
	  check_tcl_mode(nick, from, u, chan->name, ms2, op);
	  if (ms2[0] == '+')
	    got_ban(chan, nick, from, op);
	  else
	    got_unban(chan, nick, from, op, u);
	  break;
	case 'e':
	  op = newsplit(&msg);
	  fixcolon(op);
	  check_tcl_mode(nick, from, u, chan->name, ms2, op);
	  if (ms2[0] == '+')
	    got_exempt(chan, nick, from, op);
	  else
	    got_unexempt(chan, nick, from, op, u);
	  break;
	case 'I':
	  op = newsplit(&msg);
	  fixcolon(op);
	  check_tcl_mode(nick, from, u, chan->name, ms2, op);
	  if (ms2[0] == '+')
	    got_invite(chan, nick, from, op);
	  else
	    got_uninvite(chan, nick, from, op, u);
	  break;
	}
	if (todo) {
	  check_tcl_mode(nick, from, u, chan->name, ms2, "");
	  if (ms2[0] == '+')
	    chan->channel.mode |= todo;
	  else
	    chan->channel.mode &= ~todo;
	  if ((((ms2[0] == '+') && (chan->mode_mns_prot & todo)) || ((ms2[0] == '-') && (chan->mode_pls_prot & todo))) && !glob_master(irc_user) && !chan_master(irc_user))
	    add_mode(chan, ms2[0] == '+' ? '-' : '+', *chg, "");
	  else if (reversing && ((ms2[0] == '+') || (chan->mode_pls_prot & todo)) && ((ms2[0] == '-') || (chan->mode_mns_prot & todo)))
	    add_mode(chan, ms2[0] == '+' ? '-' : '+', *chg, "");
	}
	chg++;
      }
      if (chan->channel.do_opreq) 
	request_op(chan);
      if (!me_op(chan) && !nick[0])
	chan->status |= CHAN_ASKEDMODES;
    }
  }
}

/* 
 * chancmds.c -- part of irc.mod
 *   handles commands direclty relating to channel interaction
 * 
 * $Id: cmdsirc.c,v 1.19 2000/01/08 21:23:16 per Exp $
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

/* Do we have any flags that will allow us ops on a channel? */
struct chanset_t *has_op(int idx, char *chname)
{
  struct chanset_t *chan;

  Context;
  if (chname && chname[0]) {
    chan = findchan(chname);
    if (!chan) {
      dprintf(idx, STR("No such channel.\n"));
      return 0;
    }
  } else {
    dprintf(idx, STR("No channel specified\n"));
    return 0;
  }
  get_user_flagrec(dcc[idx].user, &irc_user, chname);
  if (chan_op(irc_user) || (glob_op(irc_user) && !chan_deop(irc_user)))
    return chan;
  dprintf(idx, STR("You are not a channel op on %s.\n"), chan->name);
  return 0;
}

void cmd_act(struct userrec *u, int idx, char *par)
{
  char *chname;
  struct chanset_t *chan;
  memberlist *m;

  log(LCAT_COMMAND, STR("#%s# act %s"), dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, STR("Usage: act channel <action>\n"));
    return;
  }
  if (strchr(CHANMETA, par[0]) != NULL)
    chname = newsplit(&par);
  else
    chname = 0;
  if (!(chan = has_op(idx, chname)))
    return;
  m = ismember(chan, botname);
  if (!m) {
    dprintf(idx, STR("Cannot say to %s: I'm not on that channel.\n"), chan->name);
    return;
  }
  if ((chan->channel.mode & CHANMODER) && !(m->flags & (CHANOP | CHANVOICE))) {
    dprintf(idx, STR("Cannot say to %s, it is moderated.\n"), chan->name);
    return;
  }
  dprintf(DP_HELP, STR("PRIVMSG %s :\001ACTION %s\001\n"), chan->name, par);
  dprintf(idx, STR("Action to %s: %s\n"), chan->name, par);
}

void cmd_msg(struct userrec *u, int idx, char *par)
{
  char *nick;

  log(LCAT_COMMAND, STR("#%s# msg %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: msg <nick> <message>\n"));
  } else {
    nick = newsplit(&par);
    dprintf(DP_HELP, STR("PRIVMSG %s :%s\n"), nick, par);
    dprintf(idx, STR("Msg to %s: %s\n"), nick, par);
  }
}

void cmd_say(struct userrec *u, int idx, char *par)
{
  char *chname;
  struct chanset_t *chan;
  memberlist *m;

  log(LCAT_COMMAND, STR("#%s# say %s"), dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, STR("Usage: say channel <message>\n"));
    return;
  }
  if (strchr(CHANMETA, par[0]) != NULL)
    chname = newsplit(&par);
  else
    chname = 0;
  if (!(chan = has_op(idx, chname)))
    return;
  m = ismember(chan, botname);
  if (!m) {
    dprintf(idx, STR("Cannot say to %s: I'm not on that channel.\n"), chan->name);
    return;
  }
  if ((chan->channel.mode & CHANMODER) && !(m->flags & (CHANOP | CHANVOICE))) {
    dprintf(idx, STR("Cannot say to %s, it is moderated.\n"), chan->name);
    return;
  }
  dprintf(DP_HELP, STR("PRIVMSG %s :%s\n"), chan->name, par);
  dprintf(idx, STR("Said to %s: %s\n"), chan->name, par);
}

void cmd_kickban(struct userrec *u, int idx, char *par)
{
  struct chanset_t *chan;
  char *chname,
   *nick,
   *s1;
  memberlist *m;
  char s[1024];
  char bantype = 0;

  log(LCAT_COMMAND, STR("#%s# kickban %s"), dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, STR("Usage: kickban channel [-|@]<nick> [reason]\n"));
    return;
  }
  if (strchr(CHANMETA, par[0]) != NULL)
    chname = newsplit(&par);
  else
    chname = 0;

  if (!(chan = has_op(idx, chname)))
    return;
  if (!me_op(chan)) {
    dprintf(idx, STR("I can't help you now because I'm not a channel op on %s.\n"), chan->name);
    return;
  }
  nick = newsplit(&par);
  if ((nick[0] == '@') || (nick[0] == '-')) {
    bantype = nick[0];
    nick++;
  }
  if (match_my_nick(nick)) {
    log(LCAT_WARNING, STR("%s attempted to .kickban me"), dcc[idx].nick);
    dprintf(idx, STR("You're trying to pull a Run?\n"));
    return;
  } else {
    m = ismember(chan, nick);
    if (!m) {
      dprintf(idx, STR("%s is not on %s\n"), nick, chan->name);
    } else {
      simple_sprintf(s, STR("%s!%s"), m->nick, m->userhost);
      u = get_user_by_host(s);
      get_user_flagrec(u, &irc_victim, chan->name);
      if ((chan_op(irc_victim)
	   || (glob_op(irc_victim) && !chan_deop(irc_victim)))
	  && !(chan_master(irc_user) || glob_master(irc_user))) {
	log(LCAT_WARNING, STR("%s attempted to .kickban +o %s on %s"), dcc[idx].nick, nick, chan->name);
	dprintf(idx, STR("%s is a legal op.\n"), nick);
	return;
      }
      if ((chan_master(irc_victim) || glob_master(irc_victim)) && !(glob_owner(irc_user) || chan_owner(irc_user))) {
	log(LCAT_WARNING, STR("%s attempted to .kickban +m %s on %s"), dcc[idx].nick, nick, chan->name);
	dprintf(idx, STR("%s is a %s master.\n"), nick, chan->name);
	return;
      }
      if (glob_bot(irc_victim)
	  && !(glob_owner(irc_victim) || chan_owner(irc_victim))) {
	log(LCAT_WARNING, STR("%s attempted to .kickban the bot %s on %s"), dcc[idx].nick, nick, chan->name);
	dprintf(idx, STR("%s is another channel bot!\n"), nick);
	return;
      }
      if (m->flags & CHANOP)
	add_mode(chan, '-', 'o', m->nick);
      switch (bantype) {
      case '@':
	s1 = strchr(s, '@');
	s1 -= 3;
	s1[0] = '*';
	s1[1] = '!';
	s1[2] = '*';
	break;
      case '!':
	s1 = strchr(s, '-');
	s1[1] = '*';
	s1--;
	s1[0] = '*';
	break;
      default:
	s1 = quickban(chan, m->userhost);
	break;
      }
      if (bantype == '@' || bantype == '-')
	do_mask(chan, chan->channel.ban, s1, 'b');
      if (!par[0])
	par = kickreason(KICK_BANNED);
      dprintf(DP_SERVER, STR("KICK %s %s :%s%s\n"), chan->name, m->nick, bankickprefix, par);
      m->flags |= SENTKICK;
      u_addban(chan, s1, dcc[idx].nick, par, now + (60 * ban_time), 0);
      dprintf(idx, STR("Okay, done.\n"));
    }
  }
}

void cmd_find(struct userrec *u, int idx, char *par)
{
  struct chanset_t *chan;
  memberlist * m;
  memberlist ** found = NULL;
  struct chanset_t ** cfound = NULL;
  int fcount=0;

  log (LCAT_COMMAND, STR("#%s# find %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: find <hostmask>\n"));
    return;
  }
  for (chan=chanset;chan;chan=chan->next) {
    for (m=chan->channel.member;m && m->nick[0];m=m->next) {
      char tmp[256];
      sprintf(tmp, STR("%s!%s"), m->nick, m->userhost ? m->userhost : STR("(null)"));
      if (wild_match(par, tmp)) {
	fcount++;
	if (!found) {
	  found=nmalloc(sizeof(memberlist *) * 100);
	  cfound=nmalloc(sizeof(struct chanset_t *) * 100);
	}
	found[fcount-1]=m;
	cfound[fcount-1]=chan;
	if (fcount==100)
	  break;
      }
    }
    if (fcount==100)
      break;
  }
  if (fcount) {
    char tmp[1024];
    int findex, i;
    for (findex=0;findex<fcount;findex++) {
      if (found[findex]) {
	sprintf(tmp, STR("%s!%s on %s"), found[findex]->nick, found[findex]->userhost, cfound[findex]->name);
	for (i=findex+1;i<fcount;i++) {
	  if (found[i] && (!strcmp(found[i]->nick, found[findex]->nick))) {
	    sprintf(tmp + strlen(tmp), STR(", %s"), cfound[i]->name);
	    found[i]=NULL;
	  }
	}
	dprintf(idx, STR("%s\n"), tmp);
      }
    }
    nfree(found);
    nfree(cfound);
  } else {
    dprintf(idx, STR("No matches for %s on any channels\n"), par);
  }
}

void cmd_voice(struct userrec *u, int idx, char *par)
{
  struct chanset_t *chan;
  char *nick;
  memberlist *m;
  char s[1024];

  log(LCAT_COMMAND, STR("#%s# voice %s"), dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, STR("Usage: voice <nick> channel\n"));
    return;
  }
  nick = newsplit(&par);
  if (!(chan = has_op(idx, par)))
    return;
  if (!me_op(chan)) {
    dprintf(idx, STR("I can't help you now because I'm not a chan op on %s.\n"), chan->name);
    return;
  }
  m = ismember(chan, nick);
  if (!m) {
    dprintf(idx, STR("%s is not on %s.\n"), nick, chan->name);
    return;
  }
  simple_sprintf(s, STR("%s!%s"), m->nick, m->userhost);
  add_mode(chan, '+', 'v', nick);
  dprintf(idx, STR("Gave voice to %s on %s\n"), nick, chan->name);
}

void cmd_devoice(struct userrec *u, int idx, char *par)
{
  struct chanset_t *chan;
  char *nick;
  memberlist *m;
  char s[1024];

  log(LCAT_COMMAND, STR("#%s# devoice %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: devoice <nick> channel\n"));
    return;
  }
  nick = newsplit(&par);
  if (!(chan = has_op(idx, par)))
    return;
  if (!me_op(chan)) {
    dprintf(idx, STR("I can't do that right now I'm not a chan op on %s.\n"), chan->name);
    return;
  }
  m = ismember(chan, nick);
  if (!m) {
    dprintf(idx, STR("%s is not on %s.\n"), nick, chan->name);
    return;
  }
  simple_sprintf(s, STR("%s!%s"), m->nick, m->userhost);
  add_mode(chan, '-', 'v', nick);
  dprintf(idx, STR("Devoiced %s on %s\n"), nick, chan->name);
}

void do_op(char *nick, struct chanset_t *chan)
{
  if (channel_fastop(chan)) {
    add_mode(chan, '+', 'o', nick);
  } else {
    char *tmp = nmalloc(strlen(chan->name) + 200);

    makeopline(chan, nick, tmp);
    dprintf(DP_MODE, tmp);
    nfree(tmp);
  }
}

/*
  Send "canop nick user@host #chan" to all linked bots  
  Set opreqtime in memstruct
  When 50% of linked bots have OKed and noone have denied, op the user.
  If opreqtime+5 is reached before 
*/

void cmd_op(struct userrec *u, int idx, char *par)
{
  struct chanset_t *ch;
  char *nick;
  memberlist *m;
  struct userrec *u2;
  char s[1024];
  int goodop = 0;
  struct flag_record fr = { 0, 0, 0, 0, 0 };
  struct flag_record fr2 = { 0, 0, 0, 0, 0 };

  log(LCAT_COMMAND, STR("#%s# op %s"), u->handle, par);
  nick = newsplit(&par);
  if ((!par) || (!par[0])) {
    dprintf(idx, STR("Usage: op <nick> <channelmask>\n"));
    return;
  }
  for (ch = chanset; ch; ch = ch->next) {
    if (wild_match(par, ch->name)) {
      m = ismember(ch, nick);
      if (m) {
	simple_sprintf(s, STR("%s!%s"), m->nick, m->userhost);
	u2 = get_user_by_host(s);
	if (!u2) {
	  log(LCAT_WARNING, STR("%s attempted .op %s %s - I don't know %s"), u->handle, nick, par, s);
	  return;
	}
	bzero(&fr2, sizeof(fr2));
	bzero(&fr, sizeof(fr));
	fr2.match = FR_CHAN | FR_GLOBAL;
	fr.match = FR_CHAN | FR_GLOBAL;
	get_user_flagrec(u2, &fr2, ch->name);
	get_user_flagrec(u, &fr, ch->name);
	if ((glob_op(fr) || chan_op(fr)) && !glob_deop(fr)
	    && !chan_deop(fr) && (glob_op(fr2) || chan_op(fr2))
	    && !glob_deop(fr2) && !chan_deop(fr2)) {
	  if (glob_bot(fr2)) {
	    log(LCAT_WARNING, STR("%s tried to .op %s which is a bot"), u->handle, s);
	    dprintf(idx, STR("Don't op bots. Let them handle it themselves\n"));
	  } else {
	    goodop = 1;
	    if (me_op(ch)) {
	      log(LCAT_BOTMODE, STR("Opped %s (%s) on %s by request of %s"), s, u2->handle, ch->name, u->handle);
	      dprintf(idx, STR("Opped %s on %s\n"), s, ch->name);
	      stats_add(u, 0, 1);
	      do_op(nick, ch);
	    } else {
	      dprintf(idx, STR("I'm not opped on %s\n"), ch->name);
	    }
	  }
	}
      }
    }
  }
  if (!goodop)
    log(LCAT_WARNING, STR("%s did .op %s %s - Failed for all channels"), u->handle, nick, par);
}

void cmd_mdop(struct userrec *u, int idx, char *par)
{
  char *p;
  int force_bots = 0,
    force_alines = 0,
    force_slines = 0,
    force_overlap = 0;
  int overlap = 0,
    bitch = 0,
    simul = 0;
  int needed_deops,
    max_deops,
    bots,
    deops,
    sdeops;
  memberlist **chanbots = NULL,
  **targets = NULL,
   *m;
  int chanbotcount = 0,
    targetcount = 0,
    tpos = 0,
    bpos = 0,
    i;
  struct chanset_t *chan;
  char work[1024];

  log(LCAT_COMMAND, STR("#%s# mdop %s"), dcc[idx].nick, par);
  if (!par[0] || (par[0] != '#')) {
    dprintf(idx, STR("Usage: .mdop #channel [bots=n] [alines=n] [slines=n] [overlap=n] [bitch] [simul]\n"));
    dprintf(idx, STR("  bots    : Number of bots to use for mdop.\n"));
    dprintf(idx, STR("  alines  : Number of MODE lines to assume each participating bot will get through.\n"));
    dprintf(idx, STR("  slines  : Number of MODE lines each participating bot will send.\n"));
    dprintf(idx, STR("  overlap : Number of times to deop each target nick (using alines for calc).\n"));
    dprintf(idx, STR("  bitch   : Set the channel +bitch after mdop.\n"));
    dprintf(idx, STR("  simul   : Simulate the mdop. Who would do what will be shown in DCC\n"));
    dprintf(idx, STR("bots, alines, slines and overlap are dependant on each other, set them wrong and\n"));
    dprintf(idx, STR("the bot will complain. Defaults are alines=3, slines=5, overlap=2. alines will be\n"));
    dprintf(idx, STR("increased up to 5 if there are not enough bots available.\n"));
    dprintf(idx, STR("The bot you .mdop on will never participate in the deopping\n"));
    return;
  }
  p = newsplit(&par);
  chan = findchan(p);
  if (!chan) {
    dprintf(idx, STR("%s isn't in my chanlist\n"), p);
    return;
  }

  m = ismember(chan, botname);
  if (!m) {
    dprintf(idx, STR("I'm not on %s\n"), chan->name);
    return;
  }
  if (!(m->flags & CHANOP)) {
    dprintf(idx, STR("I'm not opped on %s\n"), chan->name);
    return;
  }

  targets = nmalloc(chan->channel.members * sizeof(memberlist *));
  bzero(targets, chan->channel.members * sizeof(memberlist *));

  chanbots = nmalloc(chan->channel.members * sizeof(memberlist *));
  bzero(chanbots, chan->channel.members * sizeof(memberlist *));

  for (m = chan->channel.member; m; m = m->next)
    if (m->flags & CHANOP) {
      if (!m->user)
	targets[targetcount++] = m;
      else if (((m->user->flags & (USER_BOT | USER_OP)) == (USER_BOT | USER_OP))
	       && (strcmp(botnetnick, m->user->handle))
	       && (nextbot(m->user->handle) >= 0))
	chanbots[chanbotcount++] = m;
      else if (!(m->user->flags & USER_OP))
	targets[targetcount++] = m;
    }
  if (!chanbotcount) {
    dprintf(idx, STR("No bots opped on %s\n"), chan->name);
    nfree(targets);
    nfree(chanbots);
    return;
  }
  if (!targetcount) {
    dprintf(idx, STR("Noone to deop on %s\n"), chan->name);
    nfree(targets);
    nfree(chanbots);
    return;
  }
  while (par && par[0]) {
    p = newsplit(&par);
    if (!strncmp(p, STR("bots="), 5)) {
      p += 5;
      force_bots = atoi(p);
      if ((force_bots < 1) || (force_bots > chanbotcount)) {
	dprintf(idx, STR("bots must be within 1-%i\n"), chanbotcount);
	nfree(targets);
	nfree(chanbots);
	return;
      }
    } else if (!strncmp(p, STR("alines="), 7)) {
      p += 7;
      force_alines = atoi(p);
      if ((force_alines < 1) || (force_alines > 5)) {
	dprintf(idx, STR("alines must be within 1-5\n"));
	nfree(targets);
	nfree(chanbots);
	return;
      }
    } else if (!strncmp(p, STR("slines="), 7)) {
      p += 7;
      force_slines = atoi(p);
      if ((force_slines < 1) || (force_slines > 6)) {
	dprintf(idx, STR("slines must be within 1-6\n"));
	nfree(targets);
	nfree(chanbots);
	return;
      }
    } else if (!strncmp(p, STR("overlap="), 8)) {
      p += 8;
      force_overlap = atoi(p);
      if ((force_overlap < 1) || (force_overlap > 8)) {
	dprintf(idx, STR("overlap must be within 1-8\n"));
	nfree(targets);
	nfree(chanbots);
	return;
      }
    } else if (!strcmp(p, STR("bitch"))) {
      bitch = 1;
    } else if (!strcmp(p, STR("simul"))) {
      simul = 1;
    } else {
      dprintf(idx, STR("Unrecognized mdop option %s\n"), p);
      nfree(targets);
      nfree(chanbots);
      return;
    }
  }

  overlap = (force_overlap ? force_overlap : 2);
  needed_deops = (overlap * targetcount);
  max_deops = ((force_bots ? force_bots : chanbotcount) * (force_alines ? force_alines : 5) * 4);

  if (needed_deops > max_deops) {
    if (overlap == 1)
      dprintf(idx, STR("Not enough bots.\n"));
    else
      dprintf(idx, STR("Not enough bots. Try with overlap=1\n"));
    nfree(targets);
    nfree(chanbots);
    return;
  }

  /* ok it's possible... now let's figure out how */
  if (force_bots && force_alines) {
    /* not much choice... overlap should not autochange */
    bots = force_bots;
    deops = force_alines * 4;
  } else {
    if (force_bots) {
      /* calc needed deops per bot */
      bots = force_bots;
      deops = (needed_deops + (bots - 1)) / bots;
    } else if (force_alines) {
      deops = force_alines * 4;
      bots = (needed_deops + (deops - 1)) / deops;
    } else {
      deops = 12;
      bots = (needed_deops + (deops - 1)) / deops;
      if (bots > chanbotcount) {
	deops = 16;
	bots = (needed_deops + (deops - 1)) / deops;
      }
      if (bots > chanbotcount) {
	deops = 20;
	bots = (needed_deops + (deops - 1)) / deops;
      }
      if (bots > chanbotcount) {
	log(LCAT_ERROR, STR("Totally fucked calculations in cmd_mdop. this CAN'T happen."));
	dprintf(idx, STR("Something's wrong... bug the coder\n"));
	nfree(targets);
	nfree(chanbots);
	return;
      }
    }
  }

  if (force_slines)
    sdeops = force_slines * 4;
  else
    sdeops = 20;
  if (sdeops < deops)
    sdeops = deops;

  dprintf(idx, STR("Mass deop of %s\n"), chan->name);
  dprintf(idx, STR("  %d bots used for deop\n"), bots);
  dprintf(idx, STR("  %d assumed deops per participating bot\n"), deops);
  dprintf(idx, STR("  %d max deops per participating bot\n"), sdeops);
  dprintf(idx, STR("  %d assumed deops per target nick\n"), overlap);

  /* now use bots/deops to distribute nicks to deop */
  while (bots) {
    if (!simul)
      sprintf(work, STR("dp %s"), chan->name);
    else
      work[0] = 0;
    for (i = 0; i < deops; i++) {
      strcat(work, " ");
      strcat(work, targets[tpos]->nick);
      tpos++;
      if (tpos >= targetcount)
	tpos = 0;
    }
    if (sdeops > deops) {
      int atpos;

      atpos = tpos;
      for (i = 0; i < (sdeops - deops); i++) {
	strcat(work, " ");
	strcat(work, targets[atpos]->nick);
	atpos++;
	if (atpos >= targetcount)
	  atpos = 0;
      }
    }
    if (simul)
      dprintf(idx, STR("%s deops%s\n"), chanbots[bpos]->nick, work);
    else
      botnet_send_zapf(nextbot(chanbots[bpos]->user->handle), botnetnick, chanbots[bpos]->user->handle, work);
    bots--;
    bpos++;
  }
  if (bitch) {
    chan->status |= CHAN_BITCH;
    send_channel_sync(chan->name, NULL);
  }
  nfree(targets);
  nfree(chanbots);
  return;
}

void mdop_request(char *botnick, char *code, char *par)
{
  char *chname,
   *p;
  char work[2048];

  chname = newsplit(&par);
  work[0] = 0;
  while (par[0]) {
    int cnt = 0;

    strcat(work, STR("MODE "));
    strcat(work, chname);
    strcat(work, STR(" -oooo"));
    while ((cnt < 4) && par[0]) {
      p = newsplit(&par);
      strcat(work, " ");
      strcat(work, p);
      cnt++;
    }
    strcat(work, "\n");
  }
  tputs(serv, work, strlen(work));
}

void cmd_deop(struct userrec *u, int idx, char *par)
{
  struct chanset_t *chan;
  char *nick;
  memberlist *m;
  char s[121];

  log(LCAT_COMMAND, STR("#%s# deop %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: deop <nick> channel\n"));
    return;
  }
  nick = newsplit(&par);
  if (!(chan = has_op(idx, par)))
    return;
  if (!me_op(chan)) {
    dprintf(idx, STR("I can't help you now because I'm not a chan op on %s.\n"), chan->name);
    return;
  }
  m = ismember(chan, nick);
  if (!m) {
    dprintf(idx, STR("%s is not on %s.\n"), nick, chan->name);
    return;
  }
  if (match_my_nick(nick)) {
    log(LCAT_WARNING, STR("%s attempted to deop me on %s"), dcc[idx].nick, chan->name);
    dprintf(idx, STR("I'm not going to deop myself.\n"));
    return;
  }
  simple_sprintf(s, STR("%s!%s"), m->nick, m->userhost);
  u = get_user_by_host(s);
  get_user_flagrec(u, &irc_victim, chan->name);
  if ((chan_master(irc_victim) || glob_master(irc_victim)) && !(chan_owner(irc_user) || glob_owner(irc_user))) {
    log(LCAT_WARNING, STR("%s attempted to deop +m %s on %s"), dcc[idx].nick, nick, chan->name);
    dprintf(idx, STR("%s is a master for %s\n"), m->nick, chan->name);
    return;
  }
  if ((chan_op(irc_victim)
       || (glob_op(irc_victim) && !chan_deop(irc_victim)))
      && !(chan_master(irc_user) || glob_master(irc_user))) {
    log(LCAT_WARNING, STR("%s attempted to deop +o %s on %s"), dcc[idx].nick, nick, chan->name);
    dprintf(idx, STR("%s has the op flag for %s\n"), m->nick, chan->name);
    return;
  }
  add_mode(chan, '-', 'o', nick);
  dprintf(idx, STR("Took op from %s on %s\n"), nick, chan->name);
}

void cmd_kick(struct userrec *u, int idx, char *par)
{
  struct chanset_t *chan;
  char *chname,
   *nick;
  memberlist *m;
  char s[121];

  log(LCAT_COMMAND, STR("#%s# kick %s"), dcc[idx].nick, par);
  if (!par[0]) {
    dprintf(idx, STR("Usage: kick channel <nick> [reason]\n"));
    return;
  }
  if (strchr(CHANMETA, par[0]) != NULL)
    chname = newsplit(&par);
  else
    chname = 0;
  if (!(chan = has_op(idx, chname)))
    return;
  if (!me_op(chan)) {
    dprintf(idx, STR("I can't help you now because I'm not a channel op on %s.\n"), chan->name);
    return;
  }
  nick = newsplit(&par);
  if (!par[0])
    par = STR("request");
  if (match_my_nick(nick)) {
    log(LCAT_WARNING, STR("%s attempted to .kick me on %s"), dcc[idx].nick, chan->name);
    dprintf(idx, STR("But I don't WANT to kick myself!\n"));
    return;
  }
  m = ismember(chan, nick);
  if (!m) {
    dprintf(idx, STR("%s is not on %s\n"), nick, chan->name);
    return;
  }
  simple_sprintf(s, STR("%s!%s"), m->nick, m->userhost);
  u = get_user_by_host(s);
  get_user_flagrec(u, &irc_victim, chan->name);
  if ((chan_op(irc_victim)
       || (glob_op(irc_victim) && !chan_deop(irc_victim)))
      && !(chan_master(irc_user) || glob_master(irc_user))) {
    log(LCAT_WARNING, STR("%s attempted to .kick +o %s on %s"), dcc[idx].nick, nick, chan->name);
    dprintf(idx, STR("%s is a legal op.\n"), nick);
    return;
  }
  if ((chan_master(irc_victim) || glob_master(irc_victim)) && !(glob_owner(irc_user) || chan_owner(irc_user))) {
    log(LCAT_WARNING, STR("%s attempted to .kick +m %s on %s"), dcc[idx].nick, nick, chan->name);
    dprintf(idx, STR("%s is a %s master.\n"), nick, chan->name);
    return;
  }
  if (glob_bot(irc_victim)
      && !(glob_owner(irc_victim) || chan_owner(irc_victim))) {
    log(LCAT_WARNING, STR("%s attempted to .kick the bot %s on %s"), dcc[idx].nick, nick, chan->name);
    dprintf(idx, STR("%s is another channel bot!\n"), nick);
    return;
  }
  dprintf(DP_SERVER, STR("KICK %s %s :%s%s\n"), chan->name, m->nick, kickprefix, par);
  m->flags |= SENTKICK;
  dprintf(idx, STR("Okay, done.\n"));
}

void cmd_invite(struct userrec *u, int idx, char *par)
{
  struct chanset_t *chan;
  memberlist *m;
  char *nick;

  log(LCAT_COMMAND, STR("#%s# invite %s"), dcc[idx].nick, par);

  if (!par[0])
    par = dcc[idx].nick;	/* doh, it's been without this since .9 ! */
  /* (1.2.0+pop3) - poptix */
  nick = newsplit(&par);
  if (!(chan = has_op(idx, par)))
    return;
  if (!me_op(chan)) {
    if (chan->channel.mode & CHANINV) {
      dprintf(idx, STR("I'm not chop on %s, so I can't invite anyone.\n"), chan->name);
      return;
    }
    if (!channel_active(chan)) {
      dprintf(idx, STR("I'm not on %s right now!\n"), chan->name);
      return;
    }
  }
  m = ismember(chan, nick);
  if (m && !chan_issplit(m)) {
    dprintf(idx, STR("%s is already on %s!\n"), nick, chan->name);
    return;
  }
  dprintf(DP_SERVER, STR("INVITE %s %s\n"), nick, chan->name);
  dprintf(idx, STR("Inviting %s to %s.\n"), nick, chan->name);
}

void cmd_chops(struct userrec *u, int idx, char *par) {
  struct chanset_t * chan;
  char * chname;
  char buf[256];
  memberlist * m;
  int cnt;
  log(LCAT_COMMAND, STR("#%s# chops %s"), dcc[idx].nick, par);
  if (!has_op(idx, par))
    return;
  chname = newsplit(&par);
  chan=findchan(chname);
  if (chan == NULL) {
    dprintf(idx, STR("Not active on channel %s\n"), chname);
    return;
  }
  if (channel_pending(chan)) {
    dprintf(idx, STR("Processing channel %s\n"), chan->name);
    return;
  } else if (!channel_active(chan)) {
    dprintf(idx, STR("Desiring channel %s\n"), chan->name);
    return;
  }
  m=chan->channel.member;
  buf[0]=0;
  cnt=0;
  while (m && m->nick[0]) {
    if (chan_hasop(m)) {
      if (!cnt)
	sprintf(buf, STR("%s ops: %s"), chan->name, m->nick);
      else if (cnt<9)
	sprintf(buf + strlen(buf), STR(", %s"), m->nick);
      else {
	sprintf(buf + strlen(buf), STR(", %s\n"), m->nick);
	dprintf(idx, buf);
      }
      cnt++;
      if (cnt==10)
	cnt=0;
    }
    m=m->next;
  }
  if (cnt)
    dprintf(idx, "%s\n", buf);
}

void cmd_nops(struct userrec *u, int idx, char *par) {
  struct chanset_t * chan;
  char * chname;
  char buf[256];
  memberlist * m;
  int cnt;
  log(LCAT_COMMAND, STR("#%s# nops %s"), dcc[idx].nick, par);
  if (!has_op(idx, par))
    return;
  chname = newsplit(&par);
  chan=findchan(chname);
  if (chan == NULL) {
    dprintf(idx, STR("Not active on channel %s\n"), chname);
    return;
  }
  if (channel_pending(chan)) {
    dprintf(idx, STR("Processing channel %s\n"), chan->name);
    return;
  } else if (!channel_active(chan)) {
    dprintf(idx, STR("Desiring channel %s\n"), chan->name);
    return;
  }
  m=chan->channel.member;
  buf[0]=0;
  cnt=0;
  while (m && m->nick[0]) {
    if (!chan_hasop(m)) {
      if (!cnt)
	sprintf(buf, STR("%s nonops: %s"), chan->name, m->nick);
      else if (cnt<9)
	sprintf(buf + strlen(buf), STR(", %s"), m->nick);
      else {
	sprintf(buf + strlen(buf), STR(", %s\n"), m->nick);
	dprintf(idx, buf);
      }
      cnt++;
      if (cnt==10)
	cnt=0;
    }
    m=m->next;
  }
  if (cnt)
    dprintf(idx, "%s\n", buf);
}

void cmd_channel(struct userrec *u, int idx, char *par)
{
  char handle[20],
    s[121],
    s1[121],
    atrflag,
    chanflag[2],
   *chname;
  struct chanset_t *chan;
  int i;
  memberlist *m;
  static char spaces[33] = "                              ";
  static char spaces2[33] = "                              ";
  int len,
    len2;

  log(LCAT_COMMAND, STR("#%s# channel %s"), dcc[idx].nick, par);

  if (!has_op(idx, par))
    return;
  chname = newsplit(&par);
  if (!chname[0])
    chan = NULL;
  else
    chan = findchan(chname);
  if (chan == NULL) {
    dprintf(idx, STR("Not active on channel %s\n"), chname);
    return;
  }
  strcpy(s, getchanmode(chan));
  if (channel_pending(chan))
    sprintf(s1, STR("Processing channel %s"), chan->name);
  else if (channel_active(chan))
    sprintf(s1, STR("Channel %s"), chan->name);
  else
    sprintf(s1, STR("Desiring channel %s"), chan->name);
  dprintf(idx, STR("%s, %d member%s, mode %s:\n"), s1, chan->channel.members, chan->channel.members == 1 ? "" : "s", s);
  if (chan->channel.topic)
    dprintf(idx, STR("Channel Topic: %s\n"), chan->channel.topic);
  m = chan->channel.member;
  i = 0;
  if (channel_active(chan)) {
    dprintf(idx, STR("(n = owner, m = master, o = op, d = deop, b = bot)\n"));
    spaces[NICKMAX - 9] = 0;
    spaces2[HANDLEN - 9] = 0;
    dprintf(idx, STR(" NICKNAME %s HANDLE   %s JOIN   IDLE  SERVER                   USER@HOST\n"), spaces, spaces2);
    spaces[NICKMAX - 9] = ' ';
    spaces2[HANDLEN - 9] = ' ';
    while (m && m->nick[0]) {
      if (m->joined > 0) {
	strcpy(s, ctime(&(m->joined)));
	if ((now - (m->joined)) > 86400) {
	  strcpy(s1, &s[4]);
	  strcpy(s, &s[8]);
	  strcpy(&s[2], s1);
	  s[5] = 0;
	} else {
	  strcpy(s, &s[11]);
	  s[5] = 0;
	}
      } else
	strcpy(s, STR(" --- "));
      if (m->user == NULL) {
	sprintf(s1, STR("%s!%s"), m->nick, m->userhost);
	m->user = get_user_by_host(s1);
      }
      if (m->user == NULL) {
	strcpy(handle, "*");
      } else {
	strcpy(handle, m->user->handle);
      }
      get_user_flagrec(m->user, &irc_user, chan->name);
      /* determine status char to use */
      if (glob_bot(irc_user))
	atrflag = 'b';
      else if (glob_owner(irc_user))
	atrflag = 'N';
      else if (chan_owner(irc_user))
	atrflag = 'n';
      else if (glob_master(irc_user))
	atrflag = 'M';
      else if (chan_master(irc_user))
	atrflag = 'm';
      else if (glob_deop(irc_user))
	atrflag = 'D';
      else if (chan_deop(irc_user))
	atrflag = 'd';
      else if (glob_op(irc_user))
	atrflag = 'O';
      else if (chan_op(irc_user))
	atrflag = 'o';
      else if (glob_quiet(irc_user))
	atrflag = 'Q';
      else if (chan_quiet(irc_user))
	atrflag = 'q';
      else if (glob_gvoice(irc_user))
	atrflag = 'G';
      else if (chan_gvoice(irc_user))
	atrflag = 'g';
      else if (glob_voice(irc_user))
	atrflag = 'V';
      else if (chan_voice(irc_user))
	atrflag = 'v';
      else
	atrflag = ' ';
      if (chan_hasop(m))
	chanflag[1] = '@';
      else if (chan_hasvoice(m))
	chanflag[1] = '+';
      else
	chanflag[1] = ' ';
      if (m->flags & OPER)
	chanflag[0] = 'O';
      else
	chanflag[0] = ' ';
      spaces[len = (NICKMAX - strlen(m->nick))] = 0;
      spaces2[len2 = (HANDLEN - strlen(handle))] = 0;
      if (chan_issplit(m))
	dprintf(idx, STR("%c%c%s%s %s%s %s %c     <- netsplit, %lus, %s\n"), chanflag[0], chanflag[1], m->nick, spaces, handle, spaces2, s, atrflag, now - (m->split), m->server ? m->server : STR("(unknown server"));
      else if (!rfc_casecmp(m->nick, botname))
	dprintf(idx, STR("%c%c%s%s %s%s %s %c      %-23s <- it's me!\n"), chanflag[0], chanflag[1], m->nick, spaces, handle, spaces2, s, atrflag, m->server?m->server:STR("(unknown server)"));
      else {
	/* determine idle time */
	if (now - (m->last) > 86400)
	  sprintf(s1, STR("%2lud"), ((now - (m->last)) / 86400));
	else if (now - (m->last) > 3600)
	  sprintf(s1, STR("%2luh"), ((now - (m->last)) / 3600));
	else if (now - (m->last) > 180)
	  sprintf(s1, STR("%2lum"), ((now - (m->last)) / 60));
	else
	  strcpy(s1, "   ");
	dprintf(idx, STR("%c%c%s%s %s%s %s %c %s  %-23s %s\n"), chanflag[0], chanflag[1], m->nick, spaces, handle, spaces2, s, atrflag, s1, m->server ? m->server : STR("(unknown server)"), m->userhost);
      }
      spaces[len] = ' ';
      spaces2[len2] = ' ';
      if (chan_fakeop(m))
	dprintf(idx, STR("    (FAKE CHANOP GIVEN BY SERVER)\n"));
      if (chan_sentop(m))
	dprintf(idx, STR("    (pending +o -- I'm lagged)\n"));
      if (chan_sentdeop(m))
	dprintf(idx, STR("    (pending -o -- I'm lagged)\n"));
      if (chan_sentkick(m))
	dprintf(idx, STR("    (pending kick -- I'm lagged)\n"));
      m = m->next;
    }
  }
  dprintf(idx, STR("End of channel info\n"));
}

void cmd_topic(struct userrec *u, int idx, char *par)
{
  struct chanset_t *chan;

  log(LCAT_COMMAND, STR("#%s# topic %s"), dcc[idx].nick, par);
  if (par[0] && (strchr(CHANMETA, par[0]) != NULL)) {
    char *chname = newsplit(&par);

    chan = has_op(idx, chname);
  } else
    chan = has_op(idx, "");
  if (chan) {
    if (!par[0]) {
      if (chan->channel.topic) {
	dprintf(idx, STR("The topic for %s is: %s\n"), chan->name, chan->channel.topic);
      } else {
	dprintf(idx, STR("No topic is set for %s\n"), chan->name);
      }
    } else if (channel_optopic(chan) && !me_op(chan)) {
      dprintf(idx, STR("I'm not a channel op on %s and the channel is +t.\n"), chan->name);
    } else {
      dprintf(DP_SERVER, STR("TOPIC %s :%s\n"), chan->name, par);
      dprintf(idx, STR("Changing topic...\n"));
    }
  }
}

void cmd_resetbans(struct userrec *u, int idx, char *par)
{
  char *chname;
  struct chanset_t *chan;

  log(LCAT_COMMAND, STR("#%s# resetbans %s"), dcc[idx].nick, par);
  chname = newsplit(&par);
  rmspace(chname);

  if (chname[0]) {
    chan = findchan(chname);
    if (!chan) {
      dprintf(idx, STR("That channel doesnt exist!\n"));
      return;
    }
    get_user_flagrec(u, &irc_user, chname);
  } else {
    dprintf(idx, STR("No channel specified\n"));
    return;
  }
  if (glob_op(irc_user) || chan_op(irc_user)) {
    dprintf(idx, STR("Resetting bans on %s...\n"), chan->name);
    resetbans(chan);
  }
}

void cmd_resetexempts(struct userrec *u, int idx, char *par)
{
  char *chname;
  struct chanset_t *chan;

  log(LCAT_COMMAND, STR("#%s# resetexempts %s"), dcc[idx].nick, par);
  chname = newsplit(&par);
  rmspace(chname);

  if (chname[0]) {
    chan = findchan(chname);
    if (!chan) {
      dprintf(idx, STR("That channel doesnt exist!\n"));
      return;
    }
    get_user_flagrec(u, &irc_user, chname);
  } else {
    dprintf(idx, STR("No channel specified\n"));
    return;
  }
  if (glob_op(irc_user) || chan_op(irc_user)) {
    dprintf(idx, STR("Resetting exemptions on %s...\n"), chan->name);
    resetexempts(chan);
  }
}

void cmd_resetinvites(struct userrec *u, int idx, char *par)
{
  char *chname;
  struct chanset_t *chan;

  log(LCAT_COMMAND, STR("#%s# resetinvites %s"), dcc[idx].nick, par);
  chname = newsplit(&par);
  rmspace(chname);

  if (chname[0]) {
    chan = findchan(chname);
    if (!chan) {
      dprintf(idx, STR("That channel doesnt exist!\n"));
      return;
    }
    get_user_flagrec(u, &irc_user, chname);
  } else {
    dprintf(idx, STR("No channel specified\n"));
    return;
  }
  if (glob_op(irc_user) || chan_op(irc_user)) {
    dprintf(idx, STR("Resetting invitations on %s...\n"), chan->name);
    resetinvites(chan);
  }
}

void cmd_reset(struct userrec *u, int idx, char *par)
{
  struct chanset_t *chan;

  log(LCAT_COMMAND, STR("#%s# reset %s"), dcc[idx].nick, par);
  if (par[0]) {
    chan = findchan(par);
    if (!chan)
      dprintf(idx, STR("I don't monitor that channel.\n"));
    else {
      get_user_flagrec(u, &irc_user, par);
      if (!glob_master(irc_user) && !chan_master(irc_user)) {
	dprintf(idx, STR("You are not a master on %s.\n"), chan->name);
      } else if (!channel_active(chan)) {
	dprintf(idx, STR("Im not on %s at the moment!\n"), chan->name);
      } else {
	dprintf(idx, STR("Resetting channel info for %s...\n"), par);
	reset_chan_info(chan);
      }
    }
  } else if (!(u->flags & USER_MASTER)) {
    dprintf(idx, STR("You are not a Bot Master.\n"));
  } else {
    dprintf(idx, STR("Resetting channel info for all channels...\n"));
    chan = chanset;
    while (chan != NULL) {
      if (channel_active(chan))
	reset_chan_info(chan);
      chan = chan->next;
    }
  }
}

/* update the add/rem_builtins in irc.c if you add to this list!! */
cmd_t irc_dcc[] = {
  {"reset", "p", (Function) cmd_reset, NULL}
  ,
  {"resetbans", "p", (Function) cmd_resetbans, NULL}
  ,
  {"resetexempts", "p", (Function) cmd_resetexempts, NULL}
  ,
  {"resetinvites", "p", (Function) cmd_resetinvites, NULL}
  ,
  {"act", "p", (Function) cmd_act, NULL}
  ,
  {"channel", "p", (Function) cmd_channel, NULL}
  ,
  {"chops", "p", (Function) cmd_chops, NULL},
  {"nops", "p", (Function) cmd_nops, NULL},
  {"deop", "p", (Function) cmd_deop, NULL}
  ,
  {"find", "o", (Function) cmd_find, NULL}
  ,
  {"invite", "p", (Function) cmd_invite, NULL}
  ,
  {"kick", "p", (Function) cmd_kick, NULL}
  ,
  {"kickban", "p", (Function) cmd_kickban, NULL}
  ,
  {"mdop", "n", (Function) cmd_mdop, NULL}
  ,
  {"msg", "o", (Function) cmd_msg, NULL}
  ,
  {"voice", "p", (Function) cmd_voice, NULL}
  ,
  {"devoice", "p", (Function) cmd_devoice, NULL}
  ,
  {"op", "p", (Function) cmd_op, NULL}
  ,
  {"say", "p", (Function) cmd_say, NULL}
  ,
  {"topic", "p", (Function) cmd_topic, NULL}
  ,
  {0, 0, 0, 0}
};

/* 
 * tclirc.c -- part of irc.mod
 * 
 * $Id: tclirc.c,v 1.12 2000/01/08 21:23:16 per Exp $
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

#ifdef G_USETCL

/* streamlined by answer */
int tcl_chanlist STDVAR { char s1[121];
  int f;
  memberlist *m;
  struct userrec *u;
  struct chanset_t *chan;
  struct flag_record plus = { FR_CHAN | FR_GLOBAL, 0, 0, 0, 0 }, minus = {
  FR_CHAN | FR_GLOBAL, 0, 0, 0, 0}, user = {
  FR_CHAN | FR_GLOBAL, 0, 0, 0, 0};

  BADARGS(2, 3, STR(" channel ?flags?"));
  Context;
  chan = findchan(argv[1]);
  if (!chan) {
    Tcl_AppendResult(irp, STR("invalid channel: "), argv[1], NULL);
    return TCL_ERROR;
  }
  Context;
  m = chan->channel.member;
  if (argc == 2) {
    /* no flag restrictions so just whiz it thru quick */
    while (m && m->nick[0]) {
      Tcl_AppendElement(irp, m->nick);
      m = m->next;
    }
    return TCL_OK;
  }
  break_down_flags(argv[2], &plus, &minus);
  f = (minus.global ||minus.udef_global || minus.chan || minus.udef_chan);
  /* return empty set if asked for flags but flags don't exist */
  if (!plus.global &&!plus.udef_global && !plus.chan && !plus.udef_chan && !f)
    return TCL_OK;
  minus.match = plus.match ^ (FR_AND | FR_OR);
  while (m && m->nick[0]) {
    simple_sprintf(s1, STR("%s!%s"), m->nick, m->userhost);
    u = get_user_by_host(s1);
    get_user_flagrec(u, &user, argv[1]);
    user.match = plus.match;
    if (flagrec_eq(&plus, &user)) {
      if (!f || !flagrec_eq(&minus, &user))
	Tcl_AppendElement(irp, m->nick);
    }
    m = m->next;
  }
  return TCL_OK;
}

int tcl_botisop STDVAR { struct chanset_t *chan;

    BADARGS(2, 2, STR(" channel"));
    chan = findchan(argv[1]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("illegal channel: "), argv[1], NULL);
    return TCL_ERROR;
  }
  if (me_op(chan))
      Tcl_AppendResult(irp, "1", NULL);

  else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_botisvoice STDVAR { struct chanset_t *chan;
  memberlist *mx;

    BADARGS(2, 2, STR(" channel"));
    chan = findchan(argv[1]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("illegal channel: "), argv[1], NULL);
    return TCL_ERROR;
  }
  if ((mx = ismember(chan, botname)) && chan_hasvoice(mx))
      Tcl_AppendResult(irp, "1", NULL);

  else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_botonchan STDVAR { struct chanset_t *chan;

    BADARGS(2, 2, STR(" channel"));
    chan = findchan(argv[1]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("illegal channel: "), argv[1], NULL);
    return TCL_ERROR;
  }
  if (ismember(chan, botname))
      Tcl_AppendResult(irp, "1", NULL);

  else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_isop STDVAR { struct chanset_t *chan;
  memberlist *mx;

    BADARGS(3, 3, STR(" nick channel"));
    chan = findchan(argv[2]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("illegal channel: "), argv[2], NULL);
    return TCL_ERROR;
  }
  if ((mx = ismember(chan, argv[1])) && chan_hasop(mx))
      Tcl_AppendResult(irp, "1", NULL);

  else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_isvoice STDVAR { struct chanset_t *chan;
  memberlist *mx;

    BADARGS(3, 3, STR(" nick channel"));
    chan = findchan(argv[2]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("illegal channel: "), argv[2], NULL);
    return TCL_ERROR;
  }
  if ((mx = ismember(chan, argv[1])) && chan_hasvoice(mx))
      Tcl_AppendResult(irp, "1", NULL);

  else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_wasop STDVAR { struct chanset_t *chan;
  memberlist *mx;

    BADARGS(3, 3, STR(" nick channel"));
    chan = findchan(argv[2]);
  if (!chan) {
    Tcl_AppendResult(irp, STR("illegal channel: "), argv[2], NULL);
    return TCL_ERROR;
  }
  if ((mx = ismember(chan, argv[1])) && chan_wasop(mx))
      Tcl_AppendResult(irp, "1", NULL);

  else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_onchan STDVAR { struct chanset_t *chan;

    BADARGS(3, 3, STR(" nickname channel"));
    chan = findchan(argv[2]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("illegal channel: "), argv[2], NULL);
    return TCL_ERROR;
  }
  if (!ismember(chan, argv[1]))
      Tcl_AppendResult(irp, "0", NULL);

  else
    Tcl_AppendResult(irp, "1", NULL);
  return TCL_OK;
}

int tcl_handonchan STDVAR { struct chanset_t *chan;
  struct userrec *u;

    BADARGS(3, 3, STR(" handle channel"));
    chan = findchan(argv[2]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("illegal channel: "), argv[2], NULL);
    return TCL_ERROR;
  }
  if ((u = get_user_by_handle(userlist, argv[1])))
    if (hand_on_chan(chan, u)) {
      Tcl_AppendResult(irp, "1", NULL);
      return TCL_OK;
    }
  Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_ischanban STDVAR { struct chanset_t *chan;

    BADARGS(3, 3, STR(" ban channel"));
    chan = findchan(argv[2]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("illegal channel: "), argv[2], NULL);
    return TCL_ERROR;
  }
  if (ischanban(chan, argv[1]))
      Tcl_AppendResult(irp, "1", NULL);

  else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_ischanexempt STDVAR { struct chanset_t *chan;

    BADARGS(3, 3, STR(" exempt channel"));
    chan = findchan(argv[2]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("illegal channel: "), argv[2], NULL);
    return TCL_ERROR;
  }
  if (ischanexempt(chan, argv[1]))
      Tcl_AppendResult(irp, "1", NULL);

  else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_ischaninvite STDVAR { struct chanset_t *chan;

    BADARGS(3, 3, STR(" invite channel"));
    chan = findchan(argv[2]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("illegal channel: "), argv[2], NULL);
    return TCL_ERROR;
  }
  if (ischaninvite(chan, argv[1]))
      Tcl_AppendResult(irp, "1", NULL);

  else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_getchanhost STDVAR { struct chanset_t *chan;
  struct chanset_t *thechan = NULL;
  memberlist *m;

    Context;
    BADARGS(2, 3, STR(" nickname ?channel?"));	/* drummer */
  if (argv[2]) {
    thechan = findchan(argv[2]);
    if (!thechan) {
      Tcl_AppendResult(irp, STR("illegal channel: "), argv[2], NULL);
      return TCL_ERROR;
    }
  }
  Context;
  chan = chanset;
  while (chan != NULL) {
    m = ismember(chan, argv[1]);
    if (m && ((chan == thechan) || (thechan == NULL))) {
      Tcl_AppendResult(irp, m->userhost, NULL);
      return TCL_OK;
    }
    chan = chan->next;
  }
  return TCL_OK;
}

int tcl_onchansplit STDVAR { struct chanset_t *chan;
  memberlist *m;

    BADARGS(3, 3, STR(" nickname channel"));
  if (!(chan = findchan(argv[2]))) {
    Tcl_AppendResult(irp, STR("illegal channel: "), argv[2], NULL);
    return TCL_ERROR;
  }
  m = ismember(chan, argv[1]);

  if (m && chan_issplit(m))
    Tcl_AppendResult(irp, "1", NULL);
  else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

int tcl_maskhost STDVAR { char new[121];
    BADARGS(2, 2, STR(" nick!user@host"));
    maskhost(argv[1], new);
    Tcl_AppendResult(irp, new, NULL);
    return TCL_OK;
} int tcl_getchanidle STDVAR { memberlist *m;
  struct chanset_t *chan;
  char s[20];
  int x;

    BADARGS(3, 3, STR(" nickname channel"));
  if (!(chan = findchan(argv[2]))) {
    Tcl_AppendResult(irp, STR("invalid channel: "), argv[2], NULL);
    return TCL_ERROR;
  }
  m = ismember(chan, argv[1]);

  if (m) {
    x = (now - (m->last)) / 60;
    simple_sprintf(s, "%d", x);
    Tcl_AppendResult(irp, s, NULL);
    return TCL_OK;
  }
  Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
}

inline int tcl_chanmasks(struct maskstruct *m, Tcl_Interp * irp)
{
  char *list[3],
    work[20],
   *p;

  while (m && m->mask && m->mask[0]) {
    list[0] = m->mask;
    list[1] = m->who;
    simple_sprintf(work, STR("%lu"), now - m->timer);
    list[2] = work;
    p = Tcl_Merge(3, list);
    Tcl_AppendElement(irp, p);
    Tcl_Free((char *) p);
    m = m->next;
  }

  return TCL_OK;
}

int tcl_chanbans STDVAR { struct chanset_t *chan;

    BADARGS(2, 2, STR(" channel"));
    chan = findchan(argv[1]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("illegal channel: "), argv[2], NULL);
    return TCL_ERROR;
  }
  return tcl_chanmasks(chan->channel.ban, irp);
}

int tcl_chanexempts STDVAR { struct chanset_t *chan;

    BADARGS(2, 2, STR(" channel"));
    chan = findchan(argv[1]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("illegal channel: "), argv[2], NULL);
    return TCL_ERROR;
  }
  return tcl_chanmasks(chan->channel.exempt, irp);
}

int tcl_chaninvites STDVAR { struct chanset_t *chan;

    BADARGS(2, 2, STR(" channel"));
    chan = findchan(argv[1]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("illegal channel: "), argv[2], NULL);
    return TCL_ERROR;
  }
  return tcl_chanmasks(chan->channel.invite, irp);
}

int tcl_getchanmode STDVAR { struct chanset_t *chan;

    BADARGS(2, 2, STR(" channel"));
    chan = findchan(argv[1]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("invalid channel: "), argv[1], NULL);
    return TCL_ERROR;
  }
  Tcl_AppendResult(irp, getchanmode(chan), NULL);

  return TCL_OK;
}

int tcl_getchanjoin STDVAR { struct chanset_t *chan;
  char s[21];
  memberlist *m;

    BADARGS(3, 3, STR(" nick channel"));
    chan = findchan(argv[2]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("invalid cahnnel: "), argv[2], NULL);
    return TCL_ERROR;
  }
  m = ismember(chan, argv[1]);

  if (m == NULL) {
    Tcl_AppendResult(irp, argv[1], STR(" is not on "), argv[2], NULL);
    return TCL_ERROR;
  }
  sprintf(s, STR("%lu"), m->joined);
  Tcl_AppendResult(irp, s, NULL);
  return TCL_OK;
}

/* flushmode <chan> */
int tcl_flushmode STDVAR { struct chanset_t *chan;

    BADARGS(2, 2, STR(" channel"));
    chan = findchan(argv[1]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("invalid channel: "), argv[1], NULL);
    return TCL_ERROR;
  }
  flush_mode(chan, NORMAL);

  return TCL_OK;
}

int tcl_pushmode STDVAR { struct chanset_t *chan;
  char plus,
    mode;

    BADARGS(3, 4, STR(" channel mode ?arg?"));
    chan = findchan(argv[1]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("invalid channel: "), argv[1], NULL);
    return TCL_ERROR;
  }
  plus = argv[2][0];

  mode = argv[2][1];
  if ((plus != '+') && (plus != '-')) {
    mode = plus;
    plus = '+';
  }
  if (!(((mode >= 'a') && (mode <= 'z')) || ((mode >= 'A') && (mode <= 'Z')))) {
    Tcl_AppendResult(irp, STR("invalid mode: "), argv[2], NULL);
    return TCL_ERROR;
  }
  if (argc < 4) {
    if (strchr(STR("bvoeIk"), mode) != NULL) {
      Tcl_AppendResult(irp, STR("modes b/v/o/e/I/k/l require an argument"), NULL);
      return TCL_ERROR;
    } else if (plus == '+' && mode == 'l') {
      Tcl_AppendResult(irp, STR("modes b/v/o/e/I/k/l require an argument"), NULL);
      return TCL_ERROR;
    }
  }
  if (argc == 4)
    add_mode(chan, plus, mode, argv[3]);
  else
    add_mode(chan, plus, mode, "");
  return TCL_OK;
}

int tcl_resetbans STDVAR { struct chanset_t *chan;

    BADARGS(2, 2, STR(" channel"));
    chan = findchan(argv[1]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("invalid channel "), argv[1], NULL);
    return TCL_ERROR;
  }
  resetbans(chan);

  return TCL_OK;
}

int tcl_resetexempts STDVAR { struct chanset_t *chan;

    BADARGS(2, 2, STR(" channel"));
    chan = findchan(argv[1]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("invalid channel "), argv[1], NULL);
    return TCL_ERROR;
  }
  resetexempts(chan);

  return TCL_OK;
}

int tcl_resetinvites STDVAR { struct chanset_t *chan;

    BADARGS(2, 2, STR(" channel"));
    chan = findchan(argv[1]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("invalid channel "), argv[1], NULL);
    return TCL_ERROR;
  }
  resetinvites(chan);

  return TCL_OK;
}

int tcl_resetchan STDVAR { struct chanset_t *chan;

    Context;
    BADARGS(2, 2, STR(" channel"));
    chan = findchan(argv[1]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("invalid channel "), argv[1], NULL);
    return TCL_ERROR;
  }
  reset_chan_info(chan);

  return TCL_OK;
}

int tcl_topic STDVAR { struct chanset_t *chan;

    Context;
    BADARGS(2, 2, STR(" channel"));
    chan = findchan(argv[1]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("invalid channel "), argv[1], NULL);
    return TCL_ERROR;
  }
  Tcl_AppendResult(irp, chan->channel.topic, NULL);

  return TCL_OK;
}

int tcl_hand2nick STDVAR { memberlist *m;
  char s[161];
  struct chanset_t *chan;
  struct chanset_t *thechan = NULL;
  struct userrec *u;

    Context;
    BADARGS(2, 3, STR(" handle ?channel?"));	/* drummer */
  if (argv[2]) {
    chan = findchan(argv[2]);
    thechan = chan;
    if (chan == NULL) {
      Tcl_AppendResult(irp, STR("invalid channel: "), argv[2], NULL);
      return TCL_ERROR;
    }
  } else {
    chan = chanset;
  }
  Context;
  while ((chan != NULL) && ((thechan == NULL) || (thechan == chan))) {
    m = chan->channel.member;
    while (m && m->nick[0]) {
      simple_sprintf(s, STR("%s!%s"), m->nick, m->userhost);
      u = get_user_by_host(s);
      if (u && !strcasecmp(u->handle, argv[1])) {
	Tcl_AppendResult(irp, m->nick, NULL);
	return TCL_OK;
      }
      m = m->next;
    }
    chan = chan->next;
  }
  return TCL_OK;		/* blank */
}

int tcl_nick2hand STDVAR { memberlist *m;
  char s[161];
  struct chanset_t *chan;
  struct chanset_t *thechan = NULL;
  struct userrec *u;

    Context;
    BADARGS(2, 3, STR(" nick ?channel?"));	/* drummer */
  if (argv[2]) {
    chan = findchan(argv[2]);
    thechan = chan;
    if (chan == NULL) {
      Tcl_AppendResult(irp, STR("invalid channel: "), argv[2], NULL);
      return TCL_ERROR;
    }
  } else {
    chan = chanset;
  }
  Context;
  while ((chan != NULL) && ((thechan == NULL) || (thechan == chan))) {
    m = ismember(chan, argv[1]);
    if (m) {
      simple_sprintf(s, STR("%s!%s"), m->nick, m->userhost);
      u = get_user_by_host(s);
      Tcl_AppendResult(irp, u ? u->handle : "*", NULL);
      return TCL_OK;
    }
    chan = chan->next;
  }
  Context;
  return TCL_OK;		/* blank */
}

/*  sends an optimal number of kicks per command (as defined by
 *  kick_method) to the server, simialer to kick_all.  Fabian */
int tcl_putkick STDVAR { struct chanset_t *chan;
  int k = 0,
    l;
  char kicknick[512],
   *nick,
   *p,
   *comment = NULL;
  memberlist *m;

    Context;
    BADARGS(3, 4, STR(" channel nick?s? ?comment?"));
    chan = findchan(argv[1]);
  if (chan == NULL) {
    Tcl_AppendResult(irp, STR("illegal channel: "), argv[1], NULL);
    return TCL_ERROR;
  }
  if (argc == 4)
      comment = argv[3];

  else
    comment = "";
  if (!me_op(chan)) {
    Tcl_AppendResult(irp, STR("need op"), NULL);
    return TCL_ERROR;
  }

  kicknick[0] = 0;
  p = argv[2];
  /* loop through all given nicks */
  while (p) {
    nick = p;
    p = strchr(nick, ',');	/* search for beginning of next nick */
    if (p) {
      *p = 0;
      p++;
    }

    m = ismember(chan, nick);
    if (!m)
      continue;			/* skip non-existant nicks */
    m->flags |= SENTKICK;	/* mark as pending kick */
    if (kicknick[0])
      strcat(kicknick, ",");
    strcat(kicknick, nick);	/* add to local queue */
    k++;

    /* check if we should send the kick command yet */
    l = strlen(chan->name) + strlen(kicknick) + strlen(comment);
    if (((kick_method != 0) && (k == kick_method)) || (l > 480)) {
      dprintf(DP_SERVER, STR("KICK %s %s :%s%s\n"), chan->name, kicknick, kickprefix, comment);
      k = 0;
      kicknick[0] = 0;
    }
  }
  /* clear out all pending kicks in our local kick queue */
  if (k > 0)
    dprintf(DP_SERVER, STR("KICK %s %s :%s%s\n"), chan->name, kicknick, kickprefix, comment);
  Context;
  return TCL_OK;
}

tcl_cmds tclchan_cmds[] = {
  {"chanlist", tcl_chanlist}
  ,
  {"botisop", tcl_botisop}
  ,
  {"botisvoice", tcl_botisvoice}
  ,
  {"isop", tcl_isop}
  ,
  {"isvoice", tcl_isvoice}
  ,
  {"wasop", tcl_wasop}
  ,
  {"onchan", tcl_onchan}
  ,
  {"handonchan", tcl_handonchan}
  ,
  {"ischanban", tcl_ischanban}
  ,
  {"ischanexempt", tcl_ischanexempt}
  ,
  {"ischaninvite", tcl_ischaninvite}
  ,
  {"getchanhost", tcl_getchanhost}
  ,
  {"onchansplit", tcl_onchansplit}
  ,
  {"maskhost", tcl_maskhost}
  ,
  {"getchanidle", tcl_getchanidle}
  ,
  {"chanbans", tcl_chanbans}
  ,
  {"chanexempts", tcl_chanexempts}
  ,
  {"chaninvites", tcl_chaninvites}
  ,
  {"hand2nick", tcl_hand2nick}
  ,
  {"nick2hand", tcl_nick2hand}
  ,
  {"getchanmode", tcl_getchanmode}
  ,
  {"getchanjoin", tcl_getchanjoin}
  ,
  {"flushmode", tcl_flushmode}
  ,
  {"pushmode", tcl_pushmode}
  ,
  {"resetbans", tcl_resetbans}
  ,
  {"resetexempts", tcl_resetexempts}
  ,
  {"resetinvites", tcl_resetinvites}
  ,
  {"resetchan", tcl_resetchan}
  ,
  {"topic", tcl_topic}
  ,
  {"botonchan", tcl_botonchan}
  ,
  {"putkick", tcl_putkick}
  ,
  {0, 0}
};

#endif

/* set the key */
void set_key(struct chanset_t *chan, char *k)
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

#ifdef G_USETCL
int hand_on_chan(struct chanset_t *chan, struct userrec *u)
{
  char s[UHOSTLEN];
  memberlist *m = chan->channel.member;

  while (m && m->nick[0]) {
    sprintf(s, STR("%s!%s"), m->nick, m->userhost);
    if (u == get_user_by_host(s))
      return 1;
    m = m->next;
  }
  return 0;
}
#endif

/* adds a ban, exempt or invite mask to the list
 * m should be chan->channel.(exempt|invite|ban)
 */
void newmask(struct maskstruct *m, char *s, char *who)
{
  while (m->mask[0] && rfc_casecmp(m->mask, s))
    m = m->next;
  if (m->mask[0])
    return;			/* already existent mask */

  m->next = (struct maskstruct *) channel_malloc(sizeof(struct maskstruct));

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

/* removes a nick from the channel member list (returns 1 if successful) */
int killmember(struct chanset_t *chan, char *nick)
{
  memberlist *x,
   *old;

  x = chan->channel.member;
  old = NULL;
  while (x && x->nick[0] && rfc_casecmp(x->nick, nick)) {
    old = x;
    x = x->next;
  }
  if (!x || !x->nick[0]) {
    if (!channel_pending(chan))
      log(LCAT_ERROR, STR("(!) killmember(%s) -> nonexistent"), nick);
    return 0;
  }
  if (old)
    old->next = x->next;
  else
    chan->channel.member = x->next;
  nfree(x);
  chan->channel.members--;
  /* The following two errors should NEVER happen. We will try to correct
   * them though, to keep the bot from crashing. */
  if (chan->channel.members < 0) {
    log(LCAT_ERROR, STR("(!) BUG: number of members is negative: %d"), chan->channel.members);
    chan->channel.members = 0;
    x = chan->channel.member;
    while (x && x->nick[0]) {
      chan->channel.members++;
      x = x->next;
    }
    log(LCAT_ERROR, STR("(!) actually I know of %d members."), chan->channel.members);
  }
  if (!chan->channel.member) {
    log(LCAT_ERROR, STR("(!) BUG: memberlist is NULL"));
    chan->channel.member = (memberlist *) channel_malloc(sizeof(memberlist));
    chan->channel.member->nick[0] = 0;
    chan->channel.member->next = NULL;
  }
  return 1;
}

/* am i a chanop? */
int me_op(struct chanset_t *chan)
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

/* are there any ops on the channel? */
int any_ops(struct chanset_t *chan)
{
  memberlist *x = chan->channel.member;

  while (x && x->nick[0] && !chan_hasop(x))
    x = x->next;
  if (!x || !x->nick[0])
    return 0;
  return 1;
}

/* reset the channel information */
void reset_chan_info(struct chanset_t *chan)
{
  /* don't reset the channel if we're already resetting it */
  Context;
  if (!shouldjoin(chan)) {
    dprintf(DP_MODE, STR("PART %s\n"), chan->name);
    return;
  }
  if (!channel_pending(chan)) {
    clear_channel(chan, 1);
    chan->status |= CHAN_PEND;
    chan->status &= ~(CHAN_ACTIVE | CHAN_ASKEDMODES);
    if (!(chan->status & CHAN_ASKEDBANS)) {
      chan->status |= CHAN_ASKEDBANS;
      dprintf(DP_MODE, STR("MODE %s +b\n"), chan->name);
    }
    if (!(chan->ircnet_status & CHAN_ASKED_EXEMPTS) && use_exempts == 1) {
      chan->ircnet_status |= CHAN_ASKED_EXEMPTS;
      dprintf(DP_MODE, STR("MODE %s +e\n"), chan->name);
    }
    if (!(chan->ircnet_status & CHAN_ASKED_INVITED) && use_invites == 1) {
      chan->ircnet_status |= CHAN_ASKED_INVITED;
      dprintf(DP_MODE, STR("MODE %s +I\n"), chan->name);
    }
    /* these 2 need to get out asap, so into the mode queue */
    dprintf(DP_MODE, STR("MODE %s\n"), chan->name);
    if (use_354)
      dprintf(DP_MODE, STR("WHO %s %%c%%h%%n%%u%%f\n"), chan->name);
    else
      dprintf(DP_MODE, STR("WHO %s\n"), chan->name);
    /* this is not so critical, so slide it into the standard q */
    dprintf(DP_SERVER, STR("TOPIC %s\n"), chan->name);
    /* clear_channel nuked the data...so */
  }
}

/* log the channel members */
void log_chans()
{
  struct maskstruct *b;
  memberlist *m;
  struct chanset_t *chan;
  int chops,
    bans,
    invites,
    exempts;

  for (chan = chanset; chan != NULL; chan = chan->next) {
    if (channel_active(chan) && channel_logstatus(chan) && shouldjoin(chan)) {
      chops = 0;
      for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
	if (chan_hasop(m))
	  chops++;
      }

      bans = 0;
      for (b = chan->channel.ban; b->mask[0]; b = b->next)
	bans++;

      exempts = 0;
      for (b = chan->channel.exempt; b->mask[0]; b = b->next)
	exempts++;

      invites = 0;
      for (b = chan->channel.invite; b->mask[0]; b = b->next)
	invites++;

      log(LCAT_INFO, STR("%-10s: %d member%c (%d chop%s, %2d ba%s %s"),
	  chan->name, chan->channel.members, chan->channel.members == 1 ? ' ' : 's', chops, chops == 1 ? ")" : "s)", bans, bans == 1 ? "n" : "ns", me_op(chan) ? "" : "(not op'd)");
      if ((use_invites == 1) || (use_exempts == 1)) {
	log(LCAT_INFO, STR("%-10s: %d exemptio%s, %d invit%s"), chan->name, exempts, exempts == 1 ? "n" : "ns", invites, invites == 1 ? "e" : "es");
      }
    }
  }
}

/*  if i'm the only person on the channel, and i'm not op'd,
 *  might as well leave and rejoin. If i'm NOT the only person
 *  on the channel, but i'm still not op'd, demand ops */
void check_lonely_channel(struct chanset_t *chan)
{
  memberlist *m;
  char s[UHOSTLEN];
  int i = 0;
  static int whined = 0;

  Context;
  if (channel_pending(chan) || !channel_active(chan) || me_op(chan) || !shouldjoin(chan))
    return;
  m = chan->channel.member;
  /* count non-split channel members */
  while (m && m->nick[0]) {
    if (!chan_issplit(m))
      i++;
    m = m->next;
  }
  if ((i == 1) && channel_cycle(chan)) {
    if (chan->name[0] != '+') {	/* Its pointless to cycle + chans for ops */
      log(LCAT_INFO, STR("Trying to cycle %s to regain ops."), chan->name);
      dprintf(DP_MODE, STR("PART %s\nJOIN %s %s\n"), chan->name, chan->name, chan->key_prot);
      whined = 0;
    }
  } else if (any_ops(chan)) {
    whined = 0;
    request_op(chan);
  } else {
    /* other people here, but none are ops */
    /* are there other bots?  make them LEAVE. */
    int ok = 1;

    if (!whined) {
      if (chan->name[0] != '+')	/* Once again, + is opless.
				   * complaining about no ops when without
				   * special help(services), we cant get
				   * them - Raist */
	log(LCAT_INFO, STR("%s is active but has no ops :("), chan->name);
      whined = 1;
    }
    m = chan->channel.member;
    while (m && m->nick[0]) {
      struct userrec *u;

      sprintf(s, STR("%s!%s"), m->nick, m->userhost);
      u = get_user_by_host(s);
      if (!match_my_nick(m->nick) && (!u || !(u->flags & USER_BOT)))
	ok = 0;
      m = m->next;
    }
    if (ok) {
      /* ALL bots!  make them LEAVE!!! */
      m = chan->channel.member;
      while (m && m->nick[0]) {
	if (!match_my_nick(m->nick))
	  dprintf(DP_SERVER, STR("PRIVMSG %s :go %s\n"), m->nick, chan->name);
	m = m->next;
      }
    } else {
      /* some humans on channel, but still op-less */
      request_op(chan);
    }
  }
}

void check_servers() {
  struct chanset_t * chan;
  memberlist * m;
  for (chan=chanset;chan;chan=chan->next) {
    for (m=chan->channel.member;m && m->nick[0];m=m->next) 
      if (chan_hasop(m) && (!m->server || !m->server[0])) {
	dprintf(DP_HELP, STR("WHO %s\n"), chan->name);
	return;
      }
  }
}

#ifdef G_AUTOLOCK
void check_netfight() {
  struct chanset_t * chan;
  char tmp[256];
  int limit = atoi(CFG_FIGHTTHRESHOLD.gdata ? CFG_FIGHTTHRESHOLD.gdata : "0");
  for (chan=chanset;chan;chan=chan->next) {
    if (limit) {
      if (chan->fighting>limit) {
	if (!channel_bitch(chan) || !channel_locked(chan)) {
	  log(LCAT_WARNING, STR("Autolocking %s - channel fight\n"), chan->name);
	  strcpy(tmp, STR("+bitch +locked"));
	  do_channel_modify(chan, tmp);
	  dprintf(DP_MODE, STR("TOPIC %s :Autolocked - channel fight\n"), chan->name);
	}
      }
    }
    chan->fighting=0;
  }
}
#endif

void check_expired_chanstuff()
{
  struct maskstruct *b,
   *e;
  memberlist *m,
   *n;
  char s[UHOSTLEN],
   *snick,
   *sfrom;
  struct chanset_t *chan;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };
  static int count = 4;
  int ok = 0;
  struct userrec * buser;
  if (!server_online)
    return;
  for (chan = chanset; chan; chan = chan->next) {
    if (!(chan->status & (CHAN_ACTIVE | CHAN_PEND)) && shouldjoin(chan) && server_online)
      dprintf(DP_MODE, STR("JOIN %s %s\n"), chan->name, chan->key_prot);
    if ((chan->status & (CHAN_ACTIVE | CHAN_PEND)) && !shouldjoin(chan))
      dprintf(DP_MODE, STR("PART %s\n"), chan->name);
    if (channel_dynamicbans(chan) && me_op(chan) && shouldjoin(chan) && ismember(chan, botname)) {
      for (b = chan->channel.ban; b->mask[0]; b = b->next) {
	if ((ban_time != 0) && ((now - b->timer) > (60 * ban_time))) {
	  strcpy(s, b->who);
	  sfrom = s;
	  snick = splitnick(&sfrom);

	  if (force_expire || channel_clearbans(chan) || !(snick[0] && strcasecmp(sfrom, botuserhost) && (m = ismember(chan, snick)) && m->user && (m->user->flags & USER_BOT) && chan_hasop(m))) {
	    log(LCAT_BOTMODE, STR("(%s) Channel ban on %s expired."), chan->name, b->mask);
	    add_mode(chan, '-', 'b', b->mask);
	    b->timer = now;
	  }
	}
      }
    }
    if (use_exempts == 1) {
      if (channel_dynamicexempts(chan) && me_op(chan)) {
	for (e = chan->channel.exempt; e->mask[0]; e = e->next) {
	  if ((exempt_time != 0) && ((now - e->timer) > (60 * exempt_time))) {
	    strcpy(s, e->who);
	    sfrom = s;
	    snick = splitnick(&sfrom);
	    if (force_expire || channel_clearbans(chan) || !(snick[0] && strcasecmp(sfrom, botuserhost) && (m = ismember(chan, snick)) && m->user && (m->user->flags & USER_BOT) && chan_hasop(m))) {
	      /* Check to see if it matches a ban */
	      /* Leave this extra logging in for now. Can be removed later
	       * Jason */
	      int match = 0;

	      b = chan->channel.ban;
	      while (b->mask[0] && !match) {
		if (wild_match(b->mask, e->mask)
		    || wild_match(e->mask, b->mask))
		  match = 1;
		else
		  b = b->next;
	      }
	      if (match) {
		log(LCAT_BOTMODE, STR("(%s) Channel exemption %s NOT expired. Ban still set!"), chan->name, e->mask);
	      } else {
		log(LCAT_BOTMODE, STR("(%s) Channel exemption on %s expired."), chan->name, e->mask);
		add_mode(chan, '-', 'e', e->mask);
	      }
	      e->timer = now;
	    }
	  }
	}
      }
    }

    if (use_invites == 1) {
      if (channel_dynamicinvites(chan) && me_op(chan)) {
	for (b = chan->channel.invite; b->mask[0]; b = b->next) {
	  if ((invite_time != 0) && ((now - b->timer) > (60 * invite_time))) {
	    strcpy(s, b->who);
	    sfrom = s;
	    snick = splitnick(&sfrom);

	    if (force_expire || channel_clearbans(chan) || !(snick[0] && strcasecmp(sfrom, botuserhost) && (m = ismember(chan, snick)) && m->user && (m->user->flags & USER_BOT) && chan_hasop(m))) {
	      if ((chan->channel.mode & CHANINV) && isinvited(chan, b->mask)) {
		/* Leave this extra logging in for now. Can be removed later
		 * Jason */
		log(LCAT_BOTMODE, STR("(%s) Channel invitation %s NOT expired. i mode still set!"), chan->name, b->mask);
	      } else {
		log(LCAT_BOTMODE, STR("(%s) Channel invitation on %s expired."), chan->name, b->mask);
		add_mode(chan, '-', 'I', b->mask);
	      }
	      b->timer = now;
	    }
	  }
	}
      }
    }
    m = chan->channel.member;
    while (m && m->nick[0]) {
      if (m->split) {
	n = m->next;
	if (!channel_active(chan))
	  killmember(chan, m->nick);
	else if ((now - m->split) > wait_split) {
	  sprintf(s, STR("%s!%s"), m->nick, m->userhost);
	  m->user = get_user_by_host(s);
	  check_tcl_sign(m->nick, m->userhost, m->user, chan->name, STR("lost in the netsplit"));
	  log(LCAT_INFO, STR("%s (%s) got lost in the net-split."), m->nick, m->userhost);
	  killmember(chan, m->nick);
	}
	m = n;
      } else
	m = m->next;
    }
    if (channel_active(chan) && me_op(chan) && (chan->idle_kick)) {
      m = chan->channel.member;
      while (m && m->nick[0]) {
	if ((now - m->last) >= (chan->idle_kick * 60) && !match_my_nick(m->nick)) {
	  sprintf(s, STR("%s!%s"), m->nick, m->userhost);
	  m->user = get_user_by_host(s);
	  get_user_flagrec(m->user, &fr, chan->name);
	  if (!(glob_bot(fr) || glob_friend(fr) || (glob_op(fr) && !glob_deop(fr)) || chan_friend(fr) || chan_op(fr))) {
	    dprintf(DP_SERVER, STR("KICK %s %s :%sidle %d min\n"), chan->name, m->nick, kickprefix, chan->idle_kick);
	    m->flags |= SENTKICK;
	  }
	}
	m = m->next;
      }
    }

    buser = get_user_by_handle(userlist, botnetnick);
    if (channel_active(chan) && me_op(chan) && (buser) && (buser->flags & USER_DOVOICE)) {
      for (m=chan->channel.member; m && m->nick[0]; m=m->next) {
	if (m->user) {
	  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0, 0};
	  get_user_flagrec(m->user, &fr, chan->name);
	  if ((glob_voice(fr) || chan_voice(fr)) && !glob_quiet(fr) && !chan_quiet(fr) && 
	      !chan_hasop(m) && !chan_hasvoice(m)) 
	    add_mode(chan, '+', 'v', m->nick);
	}
      }
    }
    check_lonely_channel(chan);
  }
  if (min_servs > 0) {
    for (chan = chanset; chan; chan = chan->next)
      if (channel_active(chan) && (chan->channel.members == 1))
	ok = 1;
    if (ok) {
      count++;
      if (count >= 5) {
	dprintf(DP_SERVER, STR("LUSERS\n"));
	count = 0;
      }
    }
  }
}

#ifdef G_USETCL
int channels_6char STDVAR { Function F = (Function) cd;
  char x[20];

    BADARGS(7, 7, STR(" nick user@host handle desto/chan keyword/nick text"));
    CHECKVALIDITY(channels_6char);
    sprintf(x, "%d", F(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]));
    Tcl_AppendResult(irp, x, NULL);
    return TCL_OK;
} int channels_5char STDVAR { Function F = (Function) cd;

    BADARGS(6, 6, STR(" nick user@host handle channel text"));
    CHECKVALIDITY(channels_5char);
    F(argv[1], argv[2], argv[3], argv[4], argv[5]);
    return TCL_OK;
} int channels_4char STDVAR { Function F = (Function) cd;

    BADARGS(5, 5, STR(" nick uhost hand chan/param"));
    CHECKVALIDITY(channels_4char);
    F(argv[1], argv[2], argv[3], argv[4]);
    return TCL_OK;
}
#else
int channels_6char(void *func, void *dummy, int argc, char *argv[])
{
  Function F = (Function) func;

  return F(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
}

int channels_5char(void *func, void *dummy, int argc, char *argv[])
{
  Function F = (Function) func;

  return F(argv[1], argv[2], argv[3], argv[4], argv[5]);
}

int channels_4char(void *func, void *dummy, int argc, char *argv[])
{
  Function F = (Function) func;

  return F(argv[1], argv[2], argv[3], argv[4]);
}
#endif

void check_tcl_joinpart(char *nick, char *uhost, struct userrec *u, char *chname, p_tcl_bind_list table)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };
  char args[1024];

  Context;
  simple_sprintf(args, STR("%s %s!%s"), chname, nick, uhost);
  get_user_flagrec(u, &fr, chname);
#ifdef G_USETCL
  Tcl_SetVar(interp, STR("_jp1"), nick, 0);
  Tcl_SetVar(interp, STR("_jp2"), uhost, 0);
  Tcl_SetVar(interp, STR("_jp3"), u ? u->handle : "*", 0);
  Tcl_SetVar(interp, STR("_jp4"), chname, 0);
  Context;
  check_tcl_bind(table, args, &fr, STR(" $_jp1 $_jp2 $_jp3 $_jp4"), MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
#else
  check_tcl_bind(table, args, &fr, make_bind_param(4, nick, uhost, u ? u->handle : "*", chname), MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
#endif
  Context;
}

void check_tcl_signtopcnick(char *nick, char *uhost, struct userrec *u, char *chname, char *reason, p_tcl_bind_list table)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };
  char args[1024];

  Context;
  if (table == H_sign)
    simple_sprintf(args, STR("%s %s!%s"), chname, nick, uhost);
  else
    simple_sprintf(args, STR("%s %s"), chname, reason);
  get_user_flagrec(u, &fr, chname);
#ifdef G_USETCL
  Tcl_SetVar(interp, STR("_stnm1"), nick, 0);
  Tcl_SetVar(interp, STR("_stnm2"), uhost, 0);
  Tcl_SetVar(interp, STR("_stnm3"), u ? u->handle : "*", 0);
  Tcl_SetVar(interp, STR("_stnm4"), chname, 0);
  Tcl_SetVar(interp, STR("_stnm5"), reason, 0);
  Context;
  check_tcl_bind(table, args, &fr, STR(" $_stnm1 $_stnm2 $_stnm3 $_stnm4 $_stnm5"), MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
#else
  check_tcl_bind(table, args, &fr, make_bind_param(5, nick, uhost, u ? u->handle : "*", chname, reason), MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
#endif
  Context;
}

void check_tcl_kickmode(char *nick, char *uhost, struct userrec *u, char *chname, char *dest, char *reason, p_tcl_bind_list table)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };
  char args[512];

  Context;
  get_user_flagrec(u, &fr, chname);
  if (table == H_mode)
    simple_sprintf(args, STR("%s %s"), chname, dest);
  else
    simple_sprintf(args, STR("%s %s %s"), chname, dest, reason);
#ifdef G_USETCL
  Tcl_SetVar(interp, STR("_kick1"), nick, 0);
  Tcl_SetVar(interp, STR("_kick2"), uhost, 0);
  Tcl_SetVar(interp, STR("_kick3"), u ? u->handle : "*", 0);
  Tcl_SetVar(interp, STR("_kick4"), chname, 0);
  Tcl_SetVar(interp, STR("_kick5"), dest, 0);
  Tcl_SetVar(interp, STR("_kick6"), reason, 0);
  Context;
  check_tcl_bind(table, args, &fr, STR(" $_kick1 $_kick2 $_kick3 $_kick4 $_kick5 $_kick6"), MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
#else
  check_tcl_bind(table, args, &fr, make_bind_param(6, nick, uhost, u ? u->handle : "*", chname, dest, reason), MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
#endif
  Context;
}

int check_tcl_pub(char *nick, char *from, char *chname, char *msg)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };
  int x;
  char buf[512],
   *args = buf,
   *cmd,
    host[161],
   *hand;
  struct userrec *u;

  Context;
  strcpy(args, msg);
  cmd = newsplit(&args);
  simple_sprintf(host, STR("%s!%s"), nick, from);
  u = get_user_by_host(host);
  hand = u ? u->handle : "*";
  get_user_flagrec(u, &fr, chname);
#ifdef G_USETCL
  Tcl_SetVar(interp, STR("_pub1"), nick, 0);
  Tcl_SetVar(interp, STR("_pub2"), from, 0);
  Tcl_SetVar(interp, STR("_pub3"), hand, 0);
  Tcl_SetVar(interp, STR("_pub4"), chname, 0);
  Tcl_SetVar(interp, STR("_pub5"), args, 0);
  Context;
  x = check_tcl_bind(H_pub, cmd, &fr, STR(" $_pub1 $_pub2 $_pub3 $_pub4 $_pub5"), MATCH_EXACT | BIND_USE_ATTR | BIND_HAS_BUILTINS);
#else
  x = check_tcl_bind(H_pub, cmd, &fr, make_bind_param(5, nick, from, hand, chname, args), MATCH_EXACT | BIND_USE_ATTR | BIND_HAS_BUILTINS);
#endif
  Context;
  if (x == BIND_NOMATCH)
    return 0;
  if (x == BIND_EXEC_LOG)
    log(LCAT_COMMAND, STR("<<%s>> !%s! %s %s"), nick, hand, cmd, args);
  return 1;
}

void check_tcl_pubm(char *nick, char *from, char *chname, char *msg)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };
  char buf[1024],
    host[161];
  struct userrec *u;

  Context;
  simple_sprintf(buf, STR("%s %s"), chname, msg);
  simple_sprintf(host, STR("%s!%s"), nick, from);
  u = get_user_by_host(host);
  get_user_flagrec(u, &fr, chname);
#ifdef G_USETCL
  Tcl_SetVar(interp, STR("_pubm1"), nick, 0);
  Tcl_SetVar(interp, STR("_pubm2"), from, 0);
  Tcl_SetVar(interp, STR("_pubm3"), u ? u->handle : "*", 0);
  Tcl_SetVar(interp, STR("_pubm4"), chname, 0);
  Tcl_SetVar(interp, STR("_pubm5"), msg, 0);
  Context;
  check_tcl_bind(H_pubm, buf, &fr, STR(" $_pubm1 $_pubm2 $_pubm3 $_pubm4 $_pubm5"), MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
#else
  check_tcl_bind(H_pubm, buf, &fr, make_bind_param(5, nick, from, u ? u->handle : "*", chname, msg), MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
#endif
  Context;
}

cmd_t irc_bot[] = {
  {"dp", "", (Function) mdop_request, NULL},
  {"gi", "", (Function) getin_request, NULL},
  {0, 0, 0, 0}
};

#ifdef G_USETCL
tcl_ints myints[] = {
  {"learn-users", &learn_users, 0},	/* arthur2 */
  {"wait-split", &wait_split, 0},
  {"wait-info", &wait_info, 0},
  {"bounce-bans", &bounce_bans, 0},
  {"bounce-exempts", &bounce_exempts, 0},
  {"bounce-invites", &bounce_invites, 0},
  {"modes-per-line", &modesperline, 0},
  {"mode-buf-length", &mode_buf_len, 0},
  {"use-354", &use_354, 0},
  {"kick-method", &kick_method, 0},
  {"kick-bogus", &kick_bogus, 0},
  {"ban-bogus", &ban_bogus, 0},
  {"kick-fun", &kick_fun, 0},
  {"ban-fun", &ban_fun, 0},
  {"invite-key", &invite_key, 0},
  {"no-chanrec-info", &no_chanrec_info, 0},
  {"max-bans", &max_bans, 0},
  {"max-exempts", &max_exempts, 0},
  {"max-invites", &max_invites, 0},
  {"max-modes", &max_modes, 0},
  {"net-type", &net_type, 0},
  {"strict-host", &strict_host, 0},	/* arthur2 */
  {"ctcp-mode", &ctcp_mode, 0},	/* arthur2 */
  {"prevent-mixing", &prevent_mixing, 0},
  {"rfc-compliant", &rfc_compliant, 0},
  {"lag-threshold", &lag_threshold, 0},
  {"op-bots", &op_bots, 0},
  {"key-bots", &key_bots, 0},
  {"invite-bots", &invite_bots, 0},
  {"unban-bots", &unban_bots, 0},
  {"limit-bots", &limit_bots, 0},
  {0, 0, 0}			/* arthur2 */
};

tcl_coups mycoups[] = {
  {"op-requests", &opreq_count, &opreq_seconds},
  {0, 0, 0}
};
#endif

/* for EVERY channel */
void flush_modes()
{
  struct chanset_t *chan;
  memberlist *m;

  chan = chanset;
  while (chan != NULL) {
    m = chan->channel.member;
    while (m && m->nick[0]) {
      if ((m->delay) && (now - m->delay) > 4) {
	add_mode(chan, '+', 'o', m->nick);
	m->delay = 0L;
      }
      m = m->next;
    }
    flush_mode(chan, NORMAL);
    chan = chan->next;
  }
}

void irc_report(int idx, int details)
{
  struct flag_record fr = {
    FR_GLOBAL | FR_CHAN, 0, 0, 0, 0
  };
  char ch[1024],
    q[160],
   *p;
  int k,
    l;
  struct chanset_t *chan;

  strcpy(q, STR("Channels: "));
  k = 10;
  for (chan = chanset; chan; chan = chan->next) {
    if (idx != DP_STDOUT)
      get_user_flagrec(dcc[idx].user, &fr, chan->name);
    if ((idx == DP_STDOUT) || glob_master(fr) || chan_master(fr)) {
      p = NULL;
      if (shouldjoin(chan)) {
	if (!(chan->status & CHAN_ACTIVE))
	  p = STR("trying");
	else if (chan->status & CHAN_PEND)
	  p = STR("pending");
	else if (!me_op(chan))
	  p = STR("want ops!");
      }
      /*     else
         p = MISC_INACTIVE; */
      l = simple_sprintf(ch, STR("%s%s%s%s, "), chan->name, p ? "(" : "", p ? p : "", p ? ")" : "");
      if ((k + l) > 70) {
	dprintf(idx, STR("   %s\n"), q);
	strcpy(q, "          ");
	k = 10;
      }
      k += my_strcpy(q + k, ch);
    }
  }
  if (k > 10) {
    q[k - 2] = 0;
    dprintf(idx, STR("    %s\n"), q);
  }
}

void do_nettype()
{
  switch (net_type) {
  case 0:			/* Efnet */
    kick_method = 1;
    modesperline = 4;
    use_354 = 0;
    use_silence = 0;
    use_exempts = 0;
    use_invites = 0;
    rfc_compliant = 1;
    break;
  case 1:			/* Ircnet */
    kick_method = 4;
    modesperline = 3;
    use_354 = 0;
    use_silence = 0;
    use_exempts = 1;
    use_invites = 1;
    rfc_compliant = 1;
    break;
  case 2:			/* Undernet */
    kick_method = 1;
    modesperline = 6;
    use_354 = 1;
    use_silence = 1;
    use_exempts = 0;
    use_invites = 0;
    rfc_compliant = 1;
    break;
  case 3:			/* Dalnet */
    kick_method = 1;
    modesperline = 6;
    use_354 = 0;
    use_silence = 0;
    use_exempts = 0;
    use_invites = 0;
    rfc_compliant = 0;
    break;
  case 4:			/* new +e/+I Efnet hybrid */
    kick_method = 1;
    modesperline = 4;
    use_354 = 0;
    use_silence = 0;
    use_exempts = 1;
    use_invites = 1;
    rfc_compliant = 1;
    break;
  default:
    break;
  }
  /* Update all rfc_ function pointers */
  add_hook(HOOK_RFC_CASECMP, (Function) rfc_compliant);
}

#ifdef G_USETCL
char *traced_nettype(ClientData cdata, Tcl_Interp * irp, char *name1, char *name2, int flags)
{
  do_nettype();
  return NULL;
}

char *traced_rfccompliant(ClientData cdata, Tcl_Interp * irp, char *name1, char *name2, int flags)
{
  /* This hook forces eggdrop core to change the rfc_ match function
   * links to point to the rfc compliant versions if rfc_compliant
   * is 1, or to the normal version if it's 0 */
  add_hook(HOOK_RFC_CASECMP, (Function) rfc_compliant);
  return NULL;
}
#endif

void raise_limit(struct chanset_t * chan) {
  int l;
  if (!me_op(chan)) 
    return;
  l=chan->channel.members + chan->limitraise;
  if (l != chan->channel.maxmembers) {
    dprintf(DP_MODE, STR("MODE %s +l %d\n"), chan->name, l);
  }
}

#endif /* #ifdef LEAF */
void getin_describe(struct cfg_entry *cfgent, int idx)
{
  if (!strcmp(cfgent->name, STR("op-bots"))) {
    dprintf(idx, STR("op-bots is number of bots to ask every time a oprequest is to be made\n"));
  } else if (!strcmp(cfgent->name, STR("limit-bots"))) {
    dprintf(idx, STR("limit-bots is number of bots to ask every time a limit raise request is to be made\n"));
  } else if (!strcmp(cfgent->name, STR("invite-bots"))) {
    dprintf(idx, STR("invite-bots is number of bots to ask every time a invite request is to be made\n"));
  } else if (!strcmp(cfgent->name, STR("key-bots"))) {
    dprintf(idx, STR("key-bots is number of bots to ask every time a key request is to be made\n"));
  } else if (!strcmp(cfgent->name, STR("unban-bots"))) {
    dprintf(idx, STR("invite-bots is number of bots to ask every time a unban request is to be made\n"));
  } else if (!strcmp(cfgent->name, STR("op-requests"))) {
    dprintf(idx, STR("op-requests (requests:seconds) limits how often the bot will ask for ops\n"));
  } else if (!strcmp(cfgent->name, STR("lag-threshold"))) {
    dprintf(idx, STR("lag-threshold is maximum acceptable server lag for the bot to send/honor requests\n"));
  } else if (!strcmp(cfgent->name, STR("op-time-slack"))) {
    dprintf(idx, STR("op-time-slack is number of seconds a opcookies encoded time value can be off from the bots current time\n"));
  } else if (!strcmp(cfgent->name, STR("lock-threshold"))) {
    dprintf(idx, STR("Format H:L. When at least H hubs but L or less leafs are linked, lock all channels\n"));
  } else if (!strcmp(cfgent->name, STR("kill-threshold"))) {
    dprintf(idx, STR("When more than kill-threshold bots have been killed/k-lined the last minute, channels are locked\n"));
  } else if (!strcmp(cfgent->name, STR("fight-threshold"))) {
    dprintf(idx, STR("When more than fight-threshold ops/deops/kicks/bans/unbans altogether have happened on a channel in one minute, the channel is locked\n"));
  } else {
    dprintf(idx, STR("No description for %s ???\n"), cfgent->name);
    log(LCAT_ERROR, STR("Request to describe unrecognized getin config entry %s"), cfgent->name);
  }
}

void getin_changed(struct cfg_entry * cfgent, char * oldval, int * valid) {
  int i;
  if (!cfgent->gdata)
    return;
  *valid=0;
  if (!strcmp(cfgent->name, STR("op-requests"))) {
    int L,
      R;
    char * value = cfgent->gdata;
    L = atoi(value);
    value = strchr(value, ':');
    if (!value)
      return;
    value++;
    R = atoi(value);
    if ((R >= 60) || (R<=3) || (L<1) || (L>R))
      return;
    *valid=1;
    return;
  }
  if (!strcmp(cfgent->name, STR("lock-threshold"))) {
    int L,
      R;
    char * value = cfgent->gdata;
    L = atoi(value);
    value = strchr(value, ':');
    if (!value)
      return;
    value++;
    R = atoi(value);
    if ((R >= 1000) || (R<0) || (L<0) || (L>100))
      return;
    *valid=1;
    return;
  }
  i=atoi(cfgent->gdata);
  if (!strcmp(cfgent->name, STR("op-bots"))) {
    if ( (i<1) || (i>10))
      return;
  } else if (!strcmp(cfgent->name, STR("invite-bots"))) {
    if ( (i<1) || (i>10))
      return;
  } else if (!strcmp(cfgent->name, STR("key-bots"))) {
    if ( (i<1) || (i>10))
      return;
  } else if (!strcmp(cfgent->name, STR("limit-bots"))) {
    if ( (i<1) || (i>10))
      return;
  } else if (!strcmp(cfgent->name, STR("unban-bots"))) {
    if ( (i<1) || (i>10))
      return;
  } else if (!strcmp(cfgent->name, STR("lag-threshold"))) {
    if ( (i<3) || (i>60))
      return;
  } else if (!strcmp(cfgent->name, STR("fight-threshold"))) {
    if (i && ((i < 50) || (i>1000)))
      return;
  } else if (!strcmp(cfgent->name, STR("kill-threshold"))) {
    if ( (i<0) || (i>=200))
      return;
  } else if (!strcmp(cfgent->name, STR("op-time-slack"))) {
    if ( (i<30) || (i>1200))
      return;
  }
  *valid=1;
  return;
}


struct cfg_entry CFG_OPBOTS = {
  "op-bots", CFGF_GLOBAL, NULL, NULL,
  getin_changed, NULL, getin_describe
};

struct cfg_entry CFG_INVITEBOTS = {
  "invite-bots", CFGF_GLOBAL, NULL, NULL,
  getin_changed, NULL, getin_describe
};

struct cfg_entry CFG_LIMITBOTS = {
  "limit-bots", CFGF_GLOBAL, NULL, NULL,
  getin_changed, NULL, getin_describe
};

struct cfg_entry CFG_KEYBOTS = {
  "key-bots", CFGF_GLOBAL, NULL, NULL,
  getin_changed, NULL, getin_describe
};

struct cfg_entry CFG_UNBANBOTS = {
  "unban-bots", CFGF_GLOBAL, NULL, NULL,
  getin_changed, NULL, getin_describe
};


struct cfg_entry CFG_LAGTHRESHOLD = {
  "lag-threshold", CFGF_GLOBAL, NULL, NULL,
  getin_changed, NULL, getin_describe
};

struct cfg_entry CFG_OPREQUESTS = {
  "op-requests", CFGF_GLOBAL, NULL, NULL,
  getin_changed, NULL, getin_describe
};

struct cfg_entry CFG_OPTIMESLACK = {
  "op-time-slack", CFGF_GLOBAL, NULL, NULL,
  getin_changed, NULL, getin_describe
};


#ifdef G_AUTOLOCK
struct cfg_entry CFG_LOCKTHRESHOLD = {
  "lock-threshold", CFGF_GLOBAL, NULL, NULL,
  getin_changed, NULL, getin_describe
};

struct cfg_entry CFG_KILLTHRESHOLD = {
  "kill-threshold", CFGF_GLOBAL, NULL, NULL,
  getin_changed, NULL, getin_describe
};

struct cfg_entry CFG_FIGHTTHRESHOLD = {
  "fight-threshold", CFGF_GLOBAL, NULL, NULL,
  getin_changed, NULL, getin_describe
};

#endif

int irc_expmem()
{
  return 0;
}

void init_irc()
{
#ifdef LEAF
  struct chanset_t *chan;
#endif

  add_cfg(&CFG_OPBOTS);
  add_cfg(&CFG_INVITEBOTS);
  add_cfg(&CFG_UNBANBOTS);
  add_cfg(&CFG_LIMITBOTS);
  add_cfg(&CFG_KEYBOTS);
  add_cfg(&CFG_LAGTHRESHOLD);
  add_cfg(&CFG_OPREQUESTS);
  add_cfg(&CFG_OPTIMESLACK);
#ifdef G_AUTOLOCK
  add_cfg(&CFG_LOCKTHRESHOLD);
  add_cfg(&CFG_KILLTHRESHOLD);
  add_cfg(&CFG_FIGHTTHRESHOLD);
#endif
#ifdef LEAF
  for (chan = chanset; chan; chan = chan->next) {
    if (shouldjoin(chan))
      dprintf(DP_MODE, STR("JOIN %s %s\n"), chan->name, chan->key_prot);
    chan->status &= ~(CHAN_ACTIVE | CHAN_PEND | CHAN_ASKEDBANS);
    chan->ircnet_status &= ~(CHAN_ASKED_INVITED | CHAN_ASKED_EXEMPTS);
  }
  Context;
  add_hook(HOOK_3SECONDLY, (Function) getin_3secondly);
  add_hook(HOOK_10SECONDLY, (Function) irc_10secondly);
  add_hook(HOOK_30SECONDLY, (Function) check_expired_chanstuff);
#ifdef G_AUTOLOCK
  add_hook(HOOK_MINUTELY, (Function) check_netfight);
#endif
  add_hook(HOOK_MINUTELY, (Function) check_servers);
  add_hook(HOOK_5MINUTELY, (Function) log_chans);
  add_hook(HOOK_IDLE, (Function) flush_modes);
#ifdef G_USETCL
  Tcl_TraceVar(interp, STR("net-type"), TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS, traced_nettype, NULL);
  Tcl_TraceVar(interp, STR("rfc-compliant"), TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS, traced_rfccompliant, NULL);
  Context;
  add_tcl_ints(myints);
  add_tcl_coups(mycoups);
  add_tcl_commands(tclchan_cmds);
#endif
  add_builtins(H_dcc, irc_dcc);
  add_builtins(H_raw, irc_raw);
  add_builtins(H_bot, irc_bot);
  Context;
  H_topc = add_bind_table(STR("topc"), HT_STACKABLE, channels_5char);
  H_splt = add_bind_table(STR("splt"), HT_STACKABLE, channels_4char);
  H_sign = add_bind_table(STR("sign"), HT_STACKABLE, channels_5char);
  H_rejn = add_bind_table(STR("rejn"), HT_STACKABLE, channels_4char);
  H_part = add_bind_table(STR("part"), HT_STACKABLE, channels_4char);
  H_nick = add_bind_table(STR("nick"), HT_STACKABLE, channels_5char);
  H_mode = add_bind_table(STR("mode"), HT_STACKABLE, channels_6char);
  H_kick = add_bind_table(STR("kick"), HT_STACKABLE, channels_6char);
  H_join = add_bind_table(STR("join"), HT_STACKABLE, channels_4char);
  H_pubm = add_bind_table(STR("pubm"), HT_STACKABLE, channels_5char);
  H_pub = add_bind_table(STR("pub"), 0, channels_5char);
  Context;
  do_nettype();
  Context;
  return;
#endif
}




