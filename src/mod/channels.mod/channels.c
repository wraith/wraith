/*
 * channels.c -- part of channels.mod
 *   support for channels within the bot
 *
 */

#define MAKING_CHANNELS
#include "src/common.h"
#include "src/mod/share.mod/share.h"
#ifdef LEAF
#include "src/mod/irc.mod/irc.h"
#include "src/mod/server.mod/server.h"
#endif /* LEAF */
#include "src/chanprog.h"
#include "src/egg_timer.h"
#include "src/misc.h"
#include "src/main.h"
#include "src/color.h"
#include "src/userrec.h"
#include "src/users.h"
#include "src/cfg.h"
#include "src/rfc1459.h"
#include "src/match.h"
#include "src/settings.h"
#include "src/tandem.h"
#include "src/botnet.h"
#include "src/botmsg.h"
#include "src/net.h"
#include "src/tclhash.h"
#include "src/cmds.h"


#include <sys/stat.h>

static int 			use_info;
static char 			glob_chanmode[64];		/* Default chanmode (drummer,990731) */
static int 			global_stopnethack_mode;
static int 			global_revenge_mode;
static int 			global_idle_kick;		/* Default idle-kick setting. */
static int 			global_ban_time;
static int 			global_exempt_time;
static int 			global_invite_time;

/* Global channel settings (drummer/dw) */
char 				glob_chanset[512] = "", cfg_glob_chanset[512] = "";
static char *lastdeletedmask = NULL;

/* Global flood settings */
static int 			gfld_chan_thr;
static int 			gfld_chan_time;
static int 			gfld_deop_thr;
static int 			gfld_deop_time;
static int 			gfld_kick_thr;
static int 			gfld_kick_time;
static int 			gfld_join_thr;
static int 			gfld_join_time;
static int 			gfld_ctcp_thr;
static int 			gfld_ctcp_time;
static int 			gfld_nick_thr;
static int 			gfld_nick_time;

#ifdef S_AUTOLOCK
int 				killed_bots = 0;
#endif /* S_AUTOLOCK */

#include "channels.h"
#include "cmdschan.c"
#include "tclchan.c"
#include "userchan.c"

#ifdef S_AUTOLOCK
#define kill_threshold (CFG_KILLTHRESHOLD.gdata ? atoi(CFG_KILLTHRESHOLD.gdata) : 0)
#endif /* S_AUTOLOCK */
#ifdef S_AUTOLOCK
struct cfg_entry CFG_LOCKTHRESHOLD, CFG_KILLTHRESHOLD;
#endif /* S_AUTOLOCK */

/* This will close channels if the HUB:leaf count is skewed from config setting */
void check_should_lock()
{
#ifdef S_AUTOLOCK
#ifdef HUB
  char *p = CFG_LOCKTHRESHOLD.gdata;
  tand_t *bot = NULL;
  int H, L, hc, lc;
  struct chanset_t *chan = NULL;

  if (!p)
    return;
  H = atoi(p);
  p = strchr(p, ':');
  if (!p)
    return;
  p++;
  L = atoi(p);
  if ((H <= 0) || (L <= 0))
    return;
  hc = 1;
  lc = 0;
  for (bot = tandbot; bot; bot = bot->next) {
    struct userrec *u = get_user_by_handle(userlist, bot->bot);

    if (u) {
      if (bot_hublevel(u) < 999)
        hc++;
      else
        lc++;
    }
  }
  if ((hc >= H) && (lc <= L)) {
    for (chan = chanset; chan; chan = chan->next) {
      if (!channel_closed(chan)) {
        do_chanset(NULL, chan, "+closed chanmode +stni", DO_LOCAL | DO_NET);
#ifdef G_BACKUP
        chan->channel.backup_time = now + 30;
#endif /* G_BACKUP */
      }
    }
  }
#endif /* HUB */
#endif /* S_AUTOLOCK */
}


static void got_cset(char *botnick, char *code, char *par)
{
  int all = 0;
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  if (!par || !par[0])
   return;
  else {
   if (par[0] == '*' && par[1] == ' ') {
    all = 1;
    newsplit(&par);
   } else {
    if (!strchr(CHANMETA, par[0])) {
      putlog(LOG_ERROR, "*", "Got bad cset: bot: %s code: %s par: %s", botnick, code, par);
      return;
    }
    chname = newsplit(&par);
    if (!(chan = findchan_by_dname(chname)))
      return;
   }
  }
  if (all)
   chan = chanset;

  while (chan) {
    chname = chan->dname;
    do_chanset(NULL, chan, par, DO_LOCAL);
#ifdef LEAF
    if (chan->status & CHAN_BITCH)
      recheck_channel(chan, 0);
#endif /* LEAF */
    if (!all)
      chan = NULL;
    else
      chan = chan->next;
  }
}

static void got_cpart(char *botnick, char *code, char *par)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  if (!par[0])
   return;

  chname = newsplit(&par);
  chan = findchan_by_dname(chname);
  if (!chan)
   return;
  remove_channel(chan);
}

static void got_cjoin(char *botnick, char *code, char *par)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  if (!par[0])
   return;

  chname = newsplit(&par);
  if (findchan_by_dname(chname)) {
   return;
  } else if ((chan = findchan(chname))) {
   return;
  }

  if (channel_add(NULL, chname, par) == ERROR) /* drummer */
    putlog(LOG_BOTS, "@", "Invalid channel or channel options from %s for %s", botnick, chname);
  else {
#ifdef HUB
    write_userfile(-1);
#endif /* HUB */
  }
}

#ifdef LEAF
static void got_cycle(char *botnick, char *code, char *par)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;
  int delay = 10;

  if (!par[0])
   return;

  chname = newsplit(&par);
  if (!(chan = findchan_by_dname(chname)))
   return;
  if (par[0])
    delay = atoi(newsplit(&par));
  
  do_chanset(NULL, chan, "+inactive", DO_LOCAL);
  dprintf(DP_SERVER, "PART %s\n", chan->name);
  chan->channel.jointime = ((now + delay) - server_lag); 		/* rejoin in 10 seconds */
}

static void got_down(char *botnick, char *code, char *par)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  if (!par[0])
   return;

  chname = newsplit(&par);
  if (!(chan = findchan_by_dname(chname)))
   return;
 
  chan->channel.no_op = (now + 10);
  add_mode(chan, '-', 'o', botname);
}
#endif /* LEAF */

static void got_role(char *botnick, char *code, char *par)
{
  char *tmp = NULL;

  tmp = newsplit(&par);
  role = atoi(tmp);
  if (par[0]) {
    tmp = newsplit(&par);
    timesync = atoi(tmp) - now;
  }
  putlog(LOG_DEBUG, "@", "Got role index %d", role);
}

void got_kl(char *botnick, char *code, char *par)
{
#ifdef S_AUTOLOCK
  killed_bots++;
  if (kill_threshold && (killed_bots = kill_threshold)) {
    struct chanset_t *ch = NULL;

    for (ch = chanset; ch; ch = ch->next)
      do_chanset(NULL, ch, "+closed +backup +bitch", DO_LOCAL | DO_NET);
  /* FIXME: we should randomize nick here ... */
  }
#endif /* S_AUTOLOCK */
}


#ifdef HUB
void rebalance_roles()
{
  struct bot_addr *ba = NULL;
  int r[5] = { 0, 0, 0, 0, 0 };
  unsigned int hNdx, lNdx, i;
  char tmp[10] = "";

  for (i = 0; i < (unsigned) dcc_total; i++) {
    if (dcc[i].user && (dcc[i].user->flags & USER_BOT)) {
      ba = get_user(&USERENTRY_BOTADDR, dcc[i].user);
      if (ba && (ba->roleid > 0) && (ba->roleid < 5))
        r[(ba->roleid - 1)]++;
    }
  }
  /*
     Find high & low
     while (high-low) > 2
     move from highNdx to lowNdx
   */

  hNdx = 0;
  lNdx = 0;
  for (i = 1; i <= 4; i++) {
    if (r[i] < r[lNdx])
      lNdx = i;
    if (r[i] > r[hNdx])
      hNdx = i;
  }
  while (r[hNdx] - r[lNdx] >= 2) {
    for (i = 0; i < (unsigned) dcc_total; i++) {
      if (dcc[i].user && (dcc[i].user->flags & USER_BOT)) {
        ba = get_user(&USERENTRY_BOTADDR, dcc[i].user);
        if (ba && (ba->roleid == (hNdx + 1))) {
          ba->roleid = lNdx + 1;
          sprintf(tmp, "rl %d %li", lNdx + 1, timesync + now);
          putbot(dcc[i].nick, tmp);
        }
      }
    }
    r[hNdx]--;
    r[lNdx]++;
    hNdx = 0;
    lNdx = 0;
    for (i = 1; i <= 4; i++) {
      if (r[i] < r[lNdx])
        lNdx = i;
      if (r[i] > r[hNdx])
        hNdx = i;
    }
  }
}
#endif /* HUB */

static void channels_10secondly() {
  struct chanset_t *chan = NULL;

  for (chan = chanset; chan; chan = chan->next) {
    /* slowpart */
    if (channel_active(chan) && (chan->channel.parttime) && (chan->channel.parttime < now)) {
      chan->channel.parttime = 0;
#ifdef LEAF
      dprintf(DP_MODE, "PART %s\n", chan->name);
#endif /* LEAF */
      if (chan) /* this should NOT be necesary, but some unforseen bug requires it.. */
        remove_channel(chan);
      break;    /* if we keep looping, we'll segfault. */
    /* slowjoin */
    } else if ((chan->channel.jointime) && (chan->channel.jointime < now)) {
        chan->status &= ~CHAN_INACTIVE;
        chan->channel.jointime = 0;
#ifdef LEAF
      if (shouldjoin(chan) && !channel_active(chan))
        dprintf(DP_MODE, "JOIN %s %s\n", chan->name, chan->key_prot);
    } else if (channel_closed(chan)) {
      enforce_closed(chan);
#endif /* LEAF */
    }
  }
}

static void got_sj(int idx, char *code, char *par) 
{
  char *chname = NULL;
  time_t delay;
  struct chanset_t *chan = NULL;

  chname = newsplit(&par);
  delay = ((atoi(par) + now) - server_lag);
  chan = findchan_by_dname(chname);
  if (chan)
    chan->channel.jointime = delay;
}

static void got_sp(int idx, char *code, char *par) 
{
  char *chname = NULL;
  time_t delay;
  struct chanset_t *chan = NULL;

  chname = newsplit(&par);
  delay = ((atoi(par) + now) - server_lag);
  chan = findchan_by_dname(chname);
  if (chan)
    chan->channel.parttime = delay;
}
/* got_jn
 * We get this when a bot is opped in a +take chan
 * we are to set -inactive, jointime = 0, and join.
 */

static void got_jn(int idx, char *code, char *par)
{
  struct chanset_t *chan = NULL;
  char *chname = NULL;

  chname = newsplit(&par);
  if (!chname || !chname[0]) return;
  if (!(chan = findchan_by_dname(chname))) return;
  if (chan->channel.jointime && channel_inactive(chan)) {
    chan->status &= ~CHAN_INACTIVE;
    chan->channel.jointime = 0;
#ifdef LEAF
    if (shouldjoin(chan) && !channel_active(chan))
     dprintf(DP_MODE, "JOIN %s %s\n", chan->name, chan->key_prot);
#endif /* LEAF */
  }
}

static void set_mode_protect(struct chanset_t *chan, char *set)
{
  int i, pos = 1;
  char *s = NULL, *s1 = NULL;

  /* Clear old modes */
  chan->mode_mns_prot = chan->mode_pls_prot = 0;
  chan->limit_prot = 0;
  chan->key_prot[0] = 0;
  for (s = newsplit(&set); *s; s++) {
    i = 0;
    switch (*s) {
    case '+':
      pos = 1;
      break;
    case '-':
      pos = 0;
      break;
    case 'i':
      i = CHANINV;
      break;
    case 'p':
      i = CHANPRIV;
      break;
    case 's':
      i = CHANSEC;
      break;
    case 'm':
      i = CHANMODER;
      break;
    case 'c':
      i = CHANNOCLR;
      break;
    case 'C':
      i = CHANNOCTCP;
      break;
    case 'R':
      i = CHANREGON;
      break;
    case 'M':
      i = CHANMODR;
      break;
    case 'r':
      i = CHANLONLY;
      break;
    case 't':
      i = CHANTOPIC;
      break;
    case 'n':
      i = CHANNOMSG;
      break;
    case 'a':
      i = CHANANON;
      break;
    case 'q':
      i = CHANQUIET;
      break;
    case 'l':
      i = CHANLIMIT;
      chan->limit_prot = 0;
      if (pos) {
	s1 = newsplit(&set);
	if (s1[0] && !chan->limitraise)
	  chan->limit_prot = atoi(s1);
      }
      break;
    case 'k':
      i = CHANKEY;
      chan->key_prot[0] = 0;
      if (pos) {
	s1 = newsplit(&set);
	if (s1[0])
	  strcpy(chan->key_prot, s1);
      }
      break;
    }
    if (i) {
      if (pos) {
	chan->mode_pls_prot |= i;
	chan->mode_mns_prot &= ~i;
      } else {
	chan->mode_pls_prot &= ~i;
	chan->mode_mns_prot |= i;
      }
    }
  }
  /* Prevents a +s-p +p-s flood  (fixed by drummer) */
  if (chan->mode_pls_prot & CHANSEC)
    chan->mode_pls_prot &= ~CHANPRIV;
}

static void get_mode_protect(struct chanset_t *chan, char *s)
{
  char *p = s, s1[121] = "";
  int ok = 0, i, tst;

  for (i = 0; i < 2; i++) {
    ok = 0;
    if (i == 0) {
      tst = chan->mode_pls_prot;
      if ((tst) || (chan->limit_prot != 0) || (chan->key_prot[0]))
	*p++ = '+';
      if (chan->limit_prot != 0) {
	*p++ = 'l';
	sprintf(&s1[strlen(s1)], "%d ", chan->limit_prot);
      }
      if (chan->key_prot[0]) {
	*p++ = 'k';
	sprintf(&s1[strlen(s1)], "%s ", chan->key_prot);
      }
    } else {
      tst = chan->mode_mns_prot;
      if (tst)
	*p++ = '-';
      if (tst & CHANKEY)
	*p++ = 'k';
      if (tst & CHANLIMIT)
	*p++ = 'l';
    }
    if (tst & CHANINV)
      *p++ = 'i';
    if (tst & CHANPRIV)
      *p++ = 'p';
    if (tst & CHANSEC)
      *p++ = 's';
    if (tst & CHANMODER)
      *p++ = 'm';
    if (tst & CHANNOCLR)
      *p++ = 'c';
    if (tst & CHANNOCTCP)
      *p++ = 'C';
    if (tst & CHANREGON)
      *p++ = 'R';
    if (tst & CHANMODR)
      *p++ = 'M';
    if (tst & CHANLONLY)
      *p++ = 'r';
    if (tst & CHANTOPIC)
      *p++ = 't';
    if (tst & CHANNOMSG)
      *p++ = 'n';
    if (tst & CHANANON)
      *p++ = 'a';
    if (tst & CHANQUIET)
      *p++ = 'q';
  }
  *p = 0;
  if (s1[0]) {
    s1[strlen(s1) - 1] = 0;
    strcat(s, " ");
    strcat(s, s1);
  }
}

/* Returns true if this is one of the channel masks
 */
int ismodeline(masklist *m, char *username)
{
  for (; m && m->mask[0]; m = m->next)  
    if (!rfc_casecmp(m->mask, username))
      return 1;
  return 0;
}

/* Returns true if user matches one of the masklist -- drummer
 */
int ismasked(masklist *m, char *username)
{
  for (; m && m->mask[0]; m = m->next)
    if (wild_match(m->mask, username))
      return 1;
  return 0;
}

/* Unlink chanset element from chanset list.
 */
__inline__ static int chanset_unlink(struct chanset_t *chan)
{
  struct chanset_t *c = NULL, *c_old = NULL;

  for (c = chanset; c; c_old = c, c = c->next) {
    if (c == chan) {
      if (c_old)
	c_old->next = c->next;
      else
	chanset = c->next;
      return 1;
    }
  }
  return 0;
}

/* Completely removes a channel.
 *
 * This includes the removal of all channel-bans, -exempts and -invites, as
 * well as all user flags related to the channel.
 */
void remove_channel(struct chanset_t *chan)
{
   int		 i;
   /* Remove the channel from the list, so that noone can pull it
      away from under our feet during the check_part() call. */
   (void) chanset_unlink(chan);

#ifdef LEAF
   do_channel_part(chan);
#endif /* LEAF */

   clear_channel(chan, 0);
   noshare = 1;
   /* Remove channel-bans */
   while (chan->bans)
     u_delmask('b', chan, chan->bans->mask, 1);
   /* Remove channel-exempts */
   while (chan->exempts)
     u_delmask('e', chan, chan->exempts->mask, 1);
   /* Remove channel-invites */
   while (chan->invites)
     u_delmask('I', chan, chan->invites->mask, 1);
   /* Remove channel specific user flags */
   user_del_chan(chan->dname);
   noshare = 0;
   free(chan->channel.key);
   for (i = 0; i < 6 && chan->cmode[i].op; i++)
     free(chan->cmode[i].op);
   if (chan->key)
     free(chan->key);
   if (chan->rmkey)
     free(chan->rmkey);
   free(chan);
}

/* Bind this to chon and *if* the users console channel == ***
 * then set it to a specific channel
 */
static int channels_chon(char *handle, int idx)
{
  struct flag_record fr = {FR_CHAN | FR_ANYWH | FR_GLOBAL, 0, 0};
  int find, found = 0;
  struct chanset_t *chan = chanset;

  if (dcc[idx].type == &DCC_CHAT) {
    if (!findchan_by_dname(dcc[idx].u.chat->con_chan) &&
	((dcc[idx].u.chat->con_chan[0] != '*') ||
	 (dcc[idx].u.chat->con_chan[1] != 0))) {
      get_user_flagrec(dcc[idx].user, &fr, NULL);
      if (glob_op(fr))
	found = 1;
      if (chan_owner(fr))
	find = USER_OWNER;
      else if (chan_master(fr))
	find = USER_MASTER;
      else
	find = USER_OP;
      fr.match = FR_CHAN;
      while (chan && !found) {
	get_user_flagrec(dcc[idx].user, &fr, chan->dname);
	if (fr.chan & find)
	  found = 1;
	else
	  chan = chan->next;
      }
      if (!chan)
	chan = chanset;
      if (chan)
	strcpy(dcc[idx].u.chat->con_chan, chan->dname);
      else
	strcpy(dcc[idx].u.chat->con_chan, "*");
    }
  }
  return 0;
}

static cmd_t my_chon[] =
{
  {"*",		"",	(Function) channels_chon,	"channels:chon"},
  {NULL,	NULL,	NULL,				NULL}
};

void channels_report(int idx, int details)
{
  struct chanset_t *chan = NULL;
  int i;
  char s[1024] = "", s2[100] = "";
  struct flag_record fr = {FR_CHAN | FR_GLOBAL, 0, 0};

  for (chan = chanset; chan; chan = chan->next) {
    if (idx != DP_STDOUT)
      get_user_flagrec(dcc[idx].user, &fr, chan->dname);
    if (!private(fr, chan, PRIV_OP) && ((idx == DP_STDOUT) || glob_master(fr) || chan_master(fr))) {

      s[0] = 0;

      if (channel_bitch(chan))
	strcat(s, "bitch, ");
      if (s[0])
	s[strlen(s) - 2] = 0;
      if (!s[0])
	strcpy(s, MISC_LURKING);
      get_mode_protect(chan, s2);
      if (channel_closed(chan)) {
        strcat(s2, "i");
        if (chan->closed_private)
          strcat(s2, "p");
      }

      if (shouldjoin(chan)) {
	if (channel_active(chan)) {
	  /* If it's a !chan, we want to display it's unique name too <cybah> */
	  if (chan->dname[0]=='!') {
	    dprintf(idx, "    %-10s: %2d member%s enforcing \"%s\" (%s), "
	            "unique name %s\n", chan->dname, chan->channel.members,
	            (chan->channel.members==1) ? "," : "s,", s2, s, chan->name);
	  } else {
	    dprintf(idx, "    %-10s: %2d member%s enforcing \"%s\" (%s)\n",
	            chan->dname, chan->channel.members,
	            chan->channel.members == 1 ? "," : "s,", s2, s);
	  }
	} else {
#ifdef LEAF
	  dprintf(idx, "    %-10s: (%s), enforcing \"%s\"  (%s)\n", chan->dname,
		  channel_pending(chan) ? "pending" : "not on channel", s2, s);
#else /* !LEAF */
	  dprintf(idx, "    %-10s: (%s), enforcing \"%s\"  (%s)\n", chan->dname,
		  "limbo", s2, s);
#endif /* LEAF */
	}
      } else {
	dprintf(idx, "    %-10s: channel is set +inactive\n",
		chan->dname);
      }
      if (details) {
	s[0] = 0;
	i = 0;
	i += my_strcpy(s + i, "dynamic ");
	if (channel_enforcebans(chan))
	  i += my_strcpy(s + i, "enforcebans ");
	if (channel_dynamicbans(chan))
	  i += my_strcpy(s + i, "dynamicbans ");
	if (!channel_nouserbans(chan))
	  i += my_strcpy(s + i, "userbans ");
	if (channel_bitch(chan))
	  i += my_strcpy(s + i, "bitch ");
	if (channel_protectops(chan))
	  i += my_strcpy(s + i, "protectops ");
	if (channel_revenge(chan))
	  i += my_strcpy(s + i, "revenge ");
	if (channel_revenge(chan))
	  i += my_strcpy(s + i, "revengebot ");
	if (channel_secret(chan))
	  i += my_strcpy(s + i, "secret ");
	if (channel_cycle(chan))
	  i += my_strcpy(s + i, "cycle ");
	if (channel_dynamicexempts(chan))
	  i += my_strcpy(s + i, "dynamicexempts ");
	if (!channel_nouserexempts(chan))
	  i += my_strcpy(s + i, "userexempts ");
	if (channel_dynamicinvites(chan))
	  i += my_strcpy(s + i, "dynamicinvites ");
	if (!channel_nouserinvites(chan))
	  i += my_strcpy(s + i, "userinvites ");
	if (!shouldjoin(chan))
	  i += my_strcpy(s + i, "inactive ");
	if (channel_nodesynch(chan))
	  i += my_strcpy(s + i, "nodesynch ");
        if (channel_closed(chan))
          i += my_strcpy(s + i, "closed ");
        if (channel_take(chan))
          i += my_strcpy(s + i, "take ");
        if (channel_nomop(chan))
          i += my_strcpy(s + i, "nmop ");
        if (channel_manop(chan))
          i += my_strcpy(s + i, "manop ");
        if (channel_voice(chan))
          i += my_strcpy(s + i, "voice ");
        if (channel_autoop(chan))
          i += my_strcpy(s + i, "autoop ");
/* Chanflag template
 *	if (channel_temp(chan))
 *	  i += my_strcpy(s + i, "temp ");
*/
        if (channel_fastop(chan))
          i += my_strcpy(s + i, "fastop ");
        if (channel_private(chan))
          i += my_strcpy(s + i, "private ");

	dprintf(idx, "      Options: %s\n", s);
	if (chan->idle_kick)
	  dprintf(idx, "      Kicking idle users after %d min\n",
		  chan->idle_kick);
        if (chan->limitraise)
          dprintf(idx, "      Raising limit +%d every 2 minutes\n", chan->limitraise);
	if (chan->stopnethack_mode)
	  dprintf(idx, "      stopnethack-mode %d\n",
		  chan->stopnethack_mode);
        if (chan->revenge_mode)
          dprintf(idx, "      revenge-mode %d\n",
                  chan->revenge_mode);
       dprintf(idx, "    Bans last %d mins.\n", chan->ban_time);
       dprintf(idx, "    Exemptions last %d mins.\n", chan->exempt_time);
       dprintf(idx, "    Invitations last %d mins.\n", chan->invite_time);
      }
    }
  }
}

cmd_t channels_bot[] = {
  {"cjoin",	"", 	(Function) got_cjoin, 	NULL},
  {"cpart",	"", 	(Function) got_cpart, 	NULL},
  {"cset",	"", 	(Function) got_cset,  	NULL},
#ifdef LEAF
  {"cycle",	"", 	(Function) got_cycle, 	NULL},
  {"down",	"", 	(Function) got_down,  	NULL},
#endif /* LEAF */
  {"rl",	"", 	(Function) got_role,  	NULL},
  {"kl",	"", 	(Function) got_kl,    	NULL},
  {"sj",	"", 	(Function) got_sj,    	NULL},
  {"sp",	"", 	(Function) got_sp,    	NULL},
  {"jn",	"", 	(Function) got_jn,    	NULL},
/*
#ifdef HUB
  {"o1", "", (Function) got_o1, NULL},
  {"kl", "", (Function) got_kl, NULL},
#endif
  {"ltp", "", (Function) got_locktopic, NULL},
*/
  {NULL, 	NULL, 	NULL, 			NULL}
};

void channels_describe(struct cfg_entry *cfgent, int idx)
{
#ifdef HUB
  if (!strcmp(cfgent->name, "close-threshold")) {
    dprintf(idx, STR("Format H:L. When at least H hubs but L or less leafs are linked, close all channels\n"));
  } else if (!strcmp(cfgent->name, "kill-threshold")) {
    dprintf(idx, STR("When more than kill-threshold bots have been killed/k-lined the last minute, channels are locked\n"));
  } else {
    dprintf(idx, "No description for %s ???\n", cfgent->name);
    putlog(LOG_ERRORS, "*", "channels_describe() called with unknown config entry %s", cfgent->name);
  }
#endif /* HUB */
}

void channels_changed(struct cfg_entry *cfgent, char *oldval, int *valid)
{
  int i;

  if (!cfgent->gdata)
    return;
  *valid = 0;
  if (!strcmp(cfgent->name, "close-threshold")) {
    int L, R;
    char *value = cfgent->gdata;

    L = atoi(value);
    value = strchr(value, ':');
    if (!value)
      return;
    value++;
    R = atoi(value);
    if ((R >= 1000) || (R < 0) || (L < 0) || (L > 100))
      return;
    *valid = 1;
    return;
  }
  i = atoi(cfgent->gdata);
  if (!strcmp(cfgent->name, "kill-threshold")) {
    if ((i < 0) || (i >= 200))
      return;
  } 
  *valid = 1;
  return;
}

#ifdef S_AUTOLOCK
struct cfg_entry CFG_LOCKTHRESHOLD = {
	"close-threshold", CFGF_GLOBAL, NULL, NULL,
	channels_changed, NULL, channels_describe
};

struct cfg_entry CFG_KILLTHRESHOLD = {
	"kill-threshold", CFGF_GLOBAL, NULL, NULL,
	channels_changed, NULL, channels_describe
};
#endif /* S_AUTOLOCK */


#ifdef LEAF
static void check_limitraise() {
  int i = 0;
  static int checklimit = 1;
  struct chanset_t *chan = NULL;

  for (chan = chanset; chan; chan = chan->next, i++) {
    if (i % 2 == checklimit && chan->limitraise && dolimit(chan))
          raise_limit(chan);
  }
  if (checklimit)
    checklimit = 0;
  else
    checklimit = 1;
}
#endif /* LEAF */


void channels_init()
{
  gfld_chan_thr = 0;
  gfld_chan_time = 0;
  gfld_deop_thr = 8;
  gfld_deop_time = 10;
  gfld_kick_thr = 0;
  gfld_kick_time = 0;
  gfld_join_thr = 0;
  gfld_join_time = 0;
  gfld_ctcp_thr = 0;
  gfld_ctcp_time = 0;
  global_idle_kick = 0;
  lastdeletedmask = 0;
  use_info = 1;
  strcpy(glob_chanmode, "nt");
  global_stopnethack_mode = 0;
  global_revenge_mode = 3;
  global_ban_time = 0;
  global_exempt_time = 0;
  global_invite_time = 0;
  strcpy(glob_chanset,
         "+enforcebans "
	 "+dynamicbans "
	 "+userbans "
	 "-bitch "
	 "-protectops "
	 "-revenge "
	 "+cycle "
	 "-inactive "
	 "+userexempts "
	 "-dynamicexempts "
	 "+userinvites "
	 "-dynamicinvites "
	 "-revengebot "
	 "-nodesynch "
	 "-closed "
	 "-take "
	 "+manop "
	 "-voice "
         "-private "
	 "-fastop ");
#ifdef LEAF
  timer_create_secs(60, "check_limitraise", (Function) check_limitraise);
#endif /* LEAF */
#ifdef HUB
  timer_create_secs(30, "rebalance_roles", (Function) rebalance_roles);
#endif /* HUB */
  timer_create_secs(60, "check_expired_bans", (Function) check_expired_bans);
  timer_create_secs(60, "check_expired_exempts", (Function) check_expired_exempts);
  timer_create_secs(60, "check_expired_invites", (Function) check_expired_invites);
  timer_create_secs(10, "channels_10secondly", (Function) channels_10secondly);

  add_builtins("dcc", C_dcc_irc);
  add_builtins("bot", channels_bot);
  add_builtins("chon", my_chon);

#ifdef S_AUTOLOCK
  add_cfg(&CFG_LOCKTHRESHOLD);
  add_cfg(&CFG_KILLTHRESHOLD);
#endif /* S_AUTOLOCK */
}

