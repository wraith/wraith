/*
 * channels.c -- part of channels.mod
 *   support for channels within the bot
 *
 */

#define MODULE_NAME "channels"
#define MAKING_CHANNELS
#include <sys/stat.h>
#include "src/mod/module.h"
#include "irc.mod/irc.h"

static Function *global = NULL, *irc_funcs = NULL;

static int 			use_info;
static int			quiet_save;
static char 			glob_chanmode[64];		/* Default chanmode (drummer,990731) */
static char 			*lastdeletedmask;
static struct udef_struct 	*udef;
static int 			global_stopnethack_mode;
static int 			global_revenge_mode;
static int 			global_idle_kick;		/* Default idle-kick setting. */
static int 			global_ban_time;
static int 			global_exempt_time;
static int 			global_invite_time;

/* Global channel settings (drummer/dw) */
static char 			glob_chanset[512];

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
#include "udefchan.c"

#ifdef S_AUTOLOCK
#define kill_threshold (CFG_KILLTHRESHOLD.gdata ? atoi(CFG_KILLTHRESHOLD.gdata) : 0)
#endif /* S_AUTOLOCK */
#ifdef S_AUTOLOCK
struct cfg_entry CFG_LOCKTHRESHOLD, CFG_KILLTHRESHOLD;
#endif /* S_AUTOLOCK */

/* This will close channels if the HUB:leaf count is skewed from config setting */
static void check_should_lock()
{
#ifdef S_AUTOLOCK
#ifdef HUB
  char *p = CFG_LOCKTHRESHOLD.gdata;
  tand_t *bot;
  int H, L, hc, lc;
  struct chanset_t *chan;

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
        do_chanset(chan, STR("+closed chanmode +stni"), 1);
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
      putlog(LOG_ERROR, "*", "Got bad cset: bot: %s code: %s par: %s\n", botnick, code, par);
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
    do_chanset(chan, par, 2);
    if (chan->status & CHAN_BITCH) {
      module_entry *me;
      if ((me = module_find("irc", 0, 0)))
        (me->funcs[IRC_RECHECK_CHANNEL])(chan, 0);
    }
    if (!all)
      chan = NULL;
    else
      chan = chan->next;
  }
}

static void got_cpart(char *botnick, char *code, char *par)
{
  char *chname;
  struct chanset_t *chan;

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
  char *chname;
  struct chanset_t *chan;

  if (!par[0])
   return;

  chname = newsplit(&par);
  if (findchan_by_dname(chname)) {
   return;
  } else if ((chan = findchan(chname))) {
   return;
  }

  if (tcl_channel_add(0, chname, par) == TCL_ERROR) /* drummer */
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
  char *chname;
  struct chanset_t *chan;
  int delay = 10;

  if (!par[0])
   return;

  chname = newsplit(&par);
  if (!(chan = findchan_by_dname(chname)))
   return;
  if (par[0])
    delay = atoi(newsplit(&par));
  
  do_chanset(chan, "+inactive", 2);
  dprintf(DP_SERVER, "PART %s\n", chan->name);
  chan->channel.jointime = ((now + delay) - server_lag); 		/* rejoin in 10 seconds */
}

static void got_down(char *botnick, char *code, char *par)
{
  char *chname;
  struct chanset_t *chan;

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
  char *tmp;

  tmp = newsplit(&par);
  role = atoi(tmp);
  tmp = newsplit(&par);
  if (tmp[0])
    timesync = atoi(tmp) - now;
  putlog(LOG_DEBUG, "@", "Got role index %i", role);
}

void got_kl(char *botnick, char *code, char *par)
{
#ifdef S_AUTOLOCK
  killed_bots++;
  if (kill_threshold && (killed_bots = kill_threshold)) {
    struct chanset_t *ch;
    for (ch = chanset; ch; ch = ch->next)
      do_chanset(ch, STR("+closed +backup +bitch"), 1);
  /* FIXME: we should randomize nick here ... */
  }
#endif /* S_AUTOLOCK */
}


#ifdef HUB
void rebalance_roles()
{
  struct bot_addr *ba;
  int r[5] = { 0, 0, 0, 0, 0 }, hNdx, lNdx, i;
  char tmp[10];
  tmp[0] = 0;
  for (i = 0; i < dcc_total; i++) {
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
    for (i = 0; i < dcc_total; i++) {
      if (dcc[i].user && (dcc[i].user->flags & USER_BOT)) {
        ba = get_user(&USERENTRY_BOTADDR, dcc[i].user);
        if (ba && (ba->roleid == (hNdx + 1))) {
          ba->roleid = lNdx + 1;
          sprintf(tmp, STR("rl %d %li"), lNdx + 1, (timesync + now));
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

/* FIXME: needs more testing */
static void channels_10secondly() {
  struct chanset_t *chan;
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
#endif /* LEAF */
    } else if (channel_closed(chan)) {
      enforce_closed(chan);
    }
  }
}

static void got_sj(int idx, char *code, char *par) 
{
  char *chname;
  time_t delay;
  struct chanset_t *chan;

  chname = newsplit(&par);
  delay = ((atoi(par) + now) - server_lag);
  chan = findchan_by_dname(chname);
  if (chan)
    chan->channel.jointime = delay;
}

static void got_sp(int idx, char *code, char *par) 
{
  char *chname;
  time_t delay;
  struct chanset_t *chan;

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
  struct chanset_t *chan;
  char *chname;
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

static void *channel_malloc(int size, char *file, int line)
{
  char *p;

#ifdef DEBUG_MEM
  p = ((void *) (global[0] (size, MODULE_NAME, file, line)));
#else
  p = nmalloc(size);
#endif
  egg_bzero(p, size);
  return p;
}

static void set_mode_protect(struct chanset_t *chan, char *set)
{
  int i, pos = 1;
  char *s, *s1;

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
  char *p = s, s1[121];
  int ok = 0, i, tst;

  s1[0] = 0;
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
static int ismodeline(masklist *m, char *user)
{
  for (; m && m->mask[0]; m = m->next)  
    if (!rfc_casecmp(m->mask, user))
      return 1;
  return 0;
}

/* Returns true if user matches one of the masklist -- drummer
 */
static int ismasked(masklist *m, char *user)
{
  for (; m && m->mask[0]; m = m->next)
    if (wild_match(m->mask, user))
      return 1;
  return 0;
}

/* Unlink chanset element from chanset list.
 */
inline static int chanset_unlink(struct chanset_t *chan)
{
  struct chanset_t	*c, *c_old = NULL;

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
static void remove_channel(struct chanset_t *chan)
{
   int		 i;
   module_entry	*me;
   /* Remove the channel from the list, so that noone can pull it
      away from under our feet during the check_part() call. */
   (void) chanset_unlink(chan);

   if ((me = module_find("irc", 0, 0)) != NULL)
     (me->funcs[IRC_DO_CHANNEL_PART])(chan);

   clear_channel(chan, 0);
   noshare = 1;
   /* Remove channel-bans */
   while (chan->bans)
     u_delban(chan, chan->bans->mask, 1);
   /* Remove channel-exempts */
   while (chan->exempts)
     u_delexempt(chan, chan->exempts->mask, 1);
   /* Remove channel-invites */
   while (chan->invites)
     u_delinvite(chan, chan->invites->mask, 1);
   /* Remove channel specific user flags */
   user_del_chan(chan->dname);
   noshare = 0;
   nfree(chan->channel.key);
   for (i = 0; i < 6 && chan->cmode[i].op; i++)
     nfree(chan->cmode[i].op);
   if (chan->key)
     nfree(chan->key);
   if (chan->rmkey)
     nfree(chan->rmkey);
   nfree(chan);
}

/* Bind this to chon and *if* the users console channel == ***
 * then set it to a specific channel
 */
static int channels_chon(char *handle, int idx)
{
  struct flag_record fr = {FR_CHAN | FR_ANYWH | FR_GLOBAL, 0, 0, 0, 0, 0};
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

static char *convert_element(char *src, char *dst)
{
  int flags;

  Tcl_ScanElement(src, &flags);
  Tcl_ConvertElement(src, dst, flags);
  return dst;
}

static cmd_t my_chon[] =
{
  {"*",		"",	(Function) channels_chon,	"channels:chon"},
  {NULL,	NULL,	NULL,				NULL}
};

static void channels_report(int idx, int details)
{
  struct chanset_t *chan;
  int i;
  char s[1024], s2[100];
  struct flag_record fr = {FR_CHAN | FR_GLOBAL, 0, 0, 0, 0, 0};

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

static int expmem_masklist(masklist *m)
{
  int result = 0;

  for (; m; m = m->next) {
    result += sizeof(masklist);
    if (m->mask)
        result += strlen(m->mask) + 1;
    if (m->who)
        result += strlen(m->who) + 1;
  }
  return result;
}

static int channels_expmem()
{
  int tot = 0, i;
  struct chanset_t *chan;

  for (chan = chanset; chan; chan = chan->next) {
    tot += sizeof(struct chanset_t);

    tot += strlen(chan->channel.key) + 1;
    if (chan->channel.topic)
      tot += strlen(chan->channel.topic) + 1;
    tot += (sizeof(struct memstruct) * (chan->channel.members + 1));

    tot += expmem_masklist(chan->channel.ban);
    tot += expmem_masklist(chan->channel.exempt);
    tot += expmem_masklist(chan->channel.invite);

    for (i = 0; i < 6 && chan->cmode[i].op; i++)
      tot += strlen(chan->cmode[i].op) + 1;
    if (chan->key)
      tot += strlen(chan->key) + 1;
    if (chan->rmkey)
      tot += strlen(chan->rmkey) + 1;
  }
  tot += expmem_udef(udef);
  if (lastdeletedmask)
    tot += strlen(lastdeletedmask) + 1;
  return tot;
}

cmd_t channels_bot[] = {
  {"cjoin",    "", (Function) got_cjoin, NULL},
  {"cpart",    "", (Function) got_cpart, NULL},
  {"cset",     "", (Function) got_cset,  NULL},
#ifdef LEAF
  {"cycle",    "", (Function) got_cycle, NULL},
  {"down",     "", (Function) got_down,  NULL},
#endif /* LEAF */
  {"rl",       "", (Function) got_role,  NULL},
  {"kl",       "", (Function) got_kl,    NULL},
  {"sj",       "", (Function) got_sj,    NULL},
  {"sp",       "", (Function) got_sp,    NULL},
  {"jn",       "", (Function) got_jn,    NULL},
/*
#ifdef HUB
  {"o1", "", (Function) got_o1, NULL},
  {"kl", "", (Function) got_kl, NULL},
#endif
  {"ltp", "", (Function) got_locktopic, NULL},
*/
  {0, 0, 0, 0}
};

EXPORT_SCOPE char *channels_start();

static Function channels_table[] =
{
  /* 0 - 3 */
  (Function) channels_start,
  (Function) NULL,
  (Function) channels_expmem,
  (Function) channels_report,
  /* 4 - 7 */
  (Function) u_setsticky_mask,
  (Function) u_delban,
  (Function) u_addban,
  (Function) write_bans,
  /* 8 - 11 */
  (Function) get_chanrec,
  (Function) add_chanrec,
  (Function) del_chanrec,
  (Function) set_handle_chaninfo,
  /* 12 - 15 */
  (Function) channel_malloc,
  (Function) u_match_mask,
  (Function) u_equals_mask,
  (Function) clear_channel,
  /* 16 - 19 */
  (Function) set_handle_laston,
  (Function) NULL, /* [17] used to be ban_time <Wcc[07/19/02]> */
  (Function) & use_info,
  (Function) get_handle_chaninfo,
  /* 20 - 23 */
  (Function) u_sticky_mask,
  (Function) ismasked,
  (Function) add_chanrec_by_handle,
  (Function) NULL, /* [23] used to be isexempted() <cybah> */
  /* 24 - 27 */
  (Function) NULL, /* [24] used to be exempt_time <Wcc[07/19/02]> */
  (Function) NULL, /* [25] used to be isinvited() <cybah> */
  (Function) NULL, /* [26] used to be ban_time <Wcc[07/19/02]> */
  (Function) NULL,
  /* 28 - 31 */
  (Function) NULL, /* [28] used to be u_setsticky_exempt() <cybah> */
  (Function) u_delexempt,
  (Function) u_addexempt,
  (Function) NULL,
  /* 32 - 35 */
  (Function) NULL,/* [32] used to be u_sticky_exempt() <cybah> */
  (Function) NULL,
  (Function) NULL,	/* [34] used to be killchanset().	*/
  (Function) u_delinvite,
  /* 36 - 39 */
  (Function) u_addinvite,
  (Function) tcl_channel_add,
  (Function) tcl_channel_modify,
  (Function) write_exempts,
  /* 40 - 43 */
  (Function) write_invites,
  (Function) ismodeline,
  (Function) initudef,
  (Function) ngetudef,
  /* 44 - 47 */
  (Function) expired_mask,
  (Function) remove_channel,
  (Function) & global_ban_time,
  (Function) & global_exempt_time,
  /* 48 - 51 */
  (Function) & global_invite_time,
  (Function) write_chans,
  (Function) write_config,
  (Function) check_should_lock,
};

void channels_describe(struct cfg_entry *cfgent, int idx)
{
#ifdef HUB
  if (!strcmp(cfgent->name, STR("lock-threshold"))) {
    dprintf(idx, STR("Format H:L. When at least H hubs but L or less leafs are linked, lock all channels\n"));
  } else if (!strcmp(cfgent->name, STR("kill-threshold"))) {
    dprintf(idx, STR("When more than kill-threshold bots have been killed/k-lined the last minute, channels are locked\n"));
  } else {
    dprintf(idx, STR("No description for %s ???\n"), cfgent->name);
    putlog(LOG_ERRORS, "*", STR("channels_describe() called with unknown config entry %s"), cfgent->name);
  }
#endif /* HUB */
}

void channels_changed(struct cfg_entry *cfgent, char *oldval, int *valid)
{
  int i;

  if (!cfgent->gdata)
    return;
  *valid = 0;
  if (!strcmp(cfgent->name, STR("lock-threshold"))) {
    int L,
      R;
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
  if (!strcmp(cfgent->name, STR("kill-threshold"))) {
    if ((i < 0) || (i >= 200))
      return;
  } 
  *valid = 1;
  return;
}

#ifdef S_AUTOLOCK
struct cfg_entry CFG_LOCKTHRESHOLD = {
  "lock-threshold", CFGF_GLOBAL, NULL, NULL,
  channels_changed, NULL, channels_describe
};

struct cfg_entry CFG_KILLTHRESHOLD = {
  "kill-threshold", CFGF_GLOBAL, NULL, NULL,
  channels_changed, NULL, channels_describe
};
#endif /* S_AUTOLOCK */


#ifdef LEAF
int checklimit = 1;
static void check_limitraise() {
  int i = 0;
  struct chanset_t *chan;
  for (chan = chanset; chan; chan = chan->next, i++) {
    if (i % 2 == checklimit) {
      if (chan->limitraise) {
        if (dolimit(chan)) {
          module_entry *me;
          if ((me = module_find("irc", 0, 0)))
            (me->funcs[23])(chan);             /* raise_limit(chan) */
        }
      }
    }
  }
  if (checklimit)
    checklimit=0;
  else
    checklimit=1;
}
#endif /* LEAF */


char *channels_start(Function * global_funcs)
{
  global = global_funcs;


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
  quiet_save = 0;
  strcpy(glob_chanmode, "nt");
  udef = NULL;
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
  module_register(MODULE_NAME, channels_table, 1, 0);
  if (!(irc_funcs = module_depend(MODULE_NAME, "irc", 1, 0))) {
    module_undepend(MODULE_NAME);
    return "This module requires irc module 1.0 or later.";
  }

#ifdef LEAF
  add_hook(HOOK_MINUTELY, (Function) check_limitraise);
#endif /* LEAF */
#ifdef HUB
  add_hook(HOOK_30SECONDLY, (Function) rebalance_roles);
#endif /* HUB */
  add_hook(HOOK_MINUTELY, (Function) check_expired_bans);
  add_hook(HOOK_MINUTELY, (Function) check_expired_exempts);
  add_hook(HOOK_MINUTELY, (Function) check_expired_invites);
  add_hook(HOOK_USERFILE, (Function) channels_writeuserfile);
  add_hook(HOOK_10SECONDLY, (Function) channels_10secondly);

  add_builtins("dcc", C_dcc_irc);
  add_builtins("bot", channels_bot);
  add_builtins("chon", my_chon);

  add_tcl_commands(channels_cmds);
#ifdef S_AUTOLOCK
  add_cfg(&CFG_LOCKTHRESHOLD);
  add_cfg(&CFG_KILLTHRESHOLD);
#endif /* S_AUTOLOCK */
  return NULL;
}

