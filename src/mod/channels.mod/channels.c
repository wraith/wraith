/*
 * channels.c -- part of channels.mod
 *   support for channels within the bot
 *
 */

#define MODULE_NAME "channels"
#define MAKING_CHANNELS
#include <sys/stat.h>
#include "src/mod/module.h"

static Function *global		= NULL;

static int  use_info;
static int  quiet_save;
static char glob_chanmode[64];		/* Default chanmode (drummer,990731) */
static char *lastdeletedmask;
static struct udef_struct *udef;
static int global_stopnethack_mode;
static int global_revenge_mode;
static int global_idle_kick;		/* Default idle-kick setting. */
static int global_ban_time;
static int global_exempt_time;
static int global_invite_time;

/* Global channel settings (drummer/dw) */
static char glob_chanset[512];

/* Global flood settings */
static int gfld_chan_thr;
static int gfld_chan_time;
static int gfld_deop_thr;
static int gfld_deop_time;
static int gfld_kick_thr;
static int gfld_kick_time;
static int gfld_join_thr;
static int gfld_join_time;
static int gfld_ctcp_thr;
static int gfld_ctcp_time;
static int gfld_nick_thr;
static int gfld_nick_time;

extern int cfg_count;
extern struct cfg_entry **cfg;


#include "channels.h"
#include "cmdschan.c"
#include "tclchan.c"
#include "userchan.c"
#include "udefchan.c"

static void got_cset(char *botnick, char *code, char *par)
{
  int all = 0;
  char *chname = NULL, *list[2], *buf, *bak;
  struct chanset_t *chan = NULL;

  module_entry *me;

  if (!par[0])
   return;
  else {
   if (par[0] == '*' && par[1] == ' ') {
    return;
    all = 1;
    newsplit(&par);
   } else {
    chname = newsplit(&par);
    chan = findchan_by_dname(chname);
    if (!chan)
      return;
   }
  }
  if (all)
   chan = chanset;
//blah - from cmd_chanset
  bak = par;
  buf = nmalloc(strlen(par) + 1);
  while (chan) {
    chname = chan->dname;
    strcpy(buf, bak);
    par = buf;
    list[0] = newsplit(&par);
    while (list[0][0]) {
      if (list[0][0] == '+' || list[0][0] == '-' ||
          (!strcmp(list[0], "dont-idle-kick"))) {
        if (tcl_channel_modify(0, chan, 1, list) == TCL_OK) {
        } else if (!all || !chan->next)
          putlog(LOG_BOTS, "@", "Error trying to set %s for %s, invalid mode.",
                  list[0], all ? "all channels" : chname);
        list[0] = newsplit(&par);
        continue;
      }
      if (strncmp(list[0], "need-", 5)) {
        list[1] = par;
        /* Par gets modified in tcl channel_modify under some
         * circumstances, so save it now.
         */
        if (tcl_channel_modify(0, chan, 2, list) == TCL_OK) {
        } else if (!all || !chan->next)
          putlog(LOG_BOTS, "@", "Error trying to set %s for %s, invalid option\n",
                  list[0], all ? "all channels" : chname);
      }
    break;
    }
    if (chan->status & CHAN_BITCH)
      if ((me = module_find("irc", 0, 0)))
        (me->funcs[IRC_RECHECK_CHANNEL])(chan, 0);
    if (!all)
      chan = NULL;
    else
      chan = chan->next;
    nfree(buf);
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
static void got_cycle(char *botnick, char *code, char *par)
{
  char *chname;
  struct chanset_t *chan;

  if (!par[0])
   return;

  chname = newsplit(&par);
  chan = findchan_by_dname(chname);
  if (!chan)
   return;

  dprintf(DP_SERVER, "PART %s\n", chname);
}

static void got_down(char *botnick, char *code, char *par)
{
  char *chname;
  struct chanset_t *chan;

  if (!par[0])
   return;

  chname = newsplit(&par);
  chan = findchan_by_dname(chname);
  if (!chan)
   return;
 
  add_mode(chan, '-', 'o', botname);

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
#endif
  }
}

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

#ifdef HUB
void rebalance_roles()
{
  struct bot_addr *ba;
  int r[5] = { 0, 0, 0, 0, 0 }, hNdx, lNdx, i;
  struct userrec *u;
  char tmp[10];

Context;
ContextNote("rebalance_roles");
  for (u = userlist; u; u = u->next) {
    if ((u->flags & USER_BOT) && (nextbot(u->handle) >= 0)) {
      ba = get_user(&USERENTRY_BOTADDR, u);
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
    for (u = userlist; u; u = u->next) {
      if ((u->flags & USER_BOT) && (nextbot(u->handle) >= 0)) {
        ba = get_user(&USERENTRY_BOTADDR, u);
        if (ba && (ba->roleid == (hNdx + 1))) {
          ba->roleid = lNdx + 1;
          sprintf(tmp, STR("rl %d %li"), lNdx + 1, (timesync + now));
          botnet_send_zapf(nextbot(u->handle), botnetnick, u->handle, tmp);
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
#endif

static void warn_pls_take() 
{
  struct chanset_t *chan;
  for (chan = chanset; chan; chan = chan->next)
    if (channel_take(chan))
      putlog(LOG_WARN, "@", "%s is set +take!", chan->dname);
}

static void channels_checkslowjoin() {
  struct chanset_t * chan;
  Context;
  for (chan=chanset;chan;chan=chan->next) {
    if ((chan->parttime) && (chan->parttime < now)) {
      chan->parttime = 0;
#ifdef LEAF
      if (channel_active(chan) && !channel_inactive(chan))
        dprintf(DP_MODE, "PART %s\n", chan->dname);
#endif
      remove_channel(chan);
    } else if ((chan->jointime) && (chan->jointime < now)) {
        chan->status &= ~CHAN_INACTIVE;
        chan->jointime=0;
#ifdef LEAF
      if (!channel_inactive(chan))
        dprintf(DP_MODE, "JOIN %s %s\n", chan->dname, chan->key_prot);
#endif
    }
  }
}

static void got_sj(int idx, char * code, char * par) {
  char * chname;
  time_t delay;
  struct chanset_t * chan;
  chname = newsplit(&par);
  delay=atoi(par) + now;
  chan = findchan_by_dname(chname);
  if (chan)
    chan->jointime = delay;
}
static void got_sp(int idx, char * code, char * par) {
  char * chname;
  time_t delay;
  struct chanset_t * chan;
  chname = newsplit(&par);
  delay=atoi(par) + now;
  chan = findchan_by_dname(chname);
  if (chan)
    chan->parttime = delay;
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
      away from under our feet during the check_tcl_part() call. */
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
    if ((!channel_private(chan) || (channel_private(chan) && (chan_op(fr) || glob_owner(fr)))) &&
        ((idx == DP_STDOUT) || glob_master(fr) || chan_master(fr))) {
      s[0] = 0;
      if (channel_bitch(chan))
	strcat(s, "bitch, ");
      if (s[0])
	s[strlen(s) - 2] = 0;
      if (!s[0])
	strcpy(s, MISC_LURKING);
      get_mode_protect(chan, s2);
      if (!channel_inactive(chan)) {
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
#else
	  dprintf(idx, "    %-10s: (%s), enforcing \"%s\"  (%s)\n", chan->dname,
		  "limbo", s2, s);
#endif
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
	if (channel_dontkickops(chan))
	  i += my_strcpy(s + i, "dontkickops ");
	if (channel_revenge(chan))
	  i += my_strcpy(s + i, "revenge ");
	if (channel_revenge(chan))
	  i += my_strcpy(s + i, "revengebot ");
	if (channel_secret(chan))
	  i += my_strcpy(s + i, "secret ");
	if (channel_cycle(chan))
	  i += my_strcpy(s + i, "cycle ");
#ifdef S_IRCNET
	if (channel_dynamicexempts(chan))
	  i += my_strcpy(s + i, "dynamicexempts ");
	if (!channel_nouserexempts(chan))
	  i += my_strcpy(s + i, "userexempts ");
	if (channel_dynamicinvites(chan))
	  i += my_strcpy(s + i, "dynamicinvites ");
	if (!channel_nouserinvites(chan))
	  i += my_strcpy(s + i, "userinvites ");
#endif
	if (channel_inactive(chan))
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
#if (((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 4)) || (TCL_MAJOR_VERSION > 8))
static char *traced_globchanset(ClientData cdata, Tcl_Interp * irp,
                               CONST char *name1, CONST char *name2,
                                int flags)
#else
static char *traced_globchanset(ClientData cdata, Tcl_Interp * irp, 
                                char *name1, char *name2, int flags)
#endif
{
  char *t, *s;
  int i;
  int items;
#if (((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 4)) || (TCL_MAJOR_VERSION > 8))
  CONST char **item, *s2;
#else
  char **item, *s2;
#endif

  if (flags & (TCL_TRACE_READS | TCL_TRACE_UNSETS)) {
    Tcl_SetVar2(interp, name1, name2, glob_chanset, TCL_GLOBAL_ONLY);
    if (flags & TCL_TRACE_UNSETS)
      Tcl_TraceVar(interp, "global-chanset",
	    TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
	    traced_globchanset, NULL);
  } else { /* Write */
    s2 = Tcl_GetVar2(interp, name1, name2, TCL_GLOBAL_ONLY);
    Tcl_SplitList(interp, s2, &items, &item);
    for (i = 0; i<items; i++) {
      if (!(item[i]) || (strlen(item[i]) < 2)) continue;
      s = glob_chanset;
      while (s[0]) {
	t = strchr(s, ' '); /* Can't be NULL coz of the extra space */
	t[0] = 0;
	if (!strcmp(s + 1, item[i] + 1)) {
	  s[0] = item[i][0]; /* +- */
	  t[0] = ' ';
	  break;
	}
	t[0] = ' ';
	s = t + 1;
      }
    }
    if (item) /* hmm it cant be 0 */
      Tcl_Free((char *) item);
    Tcl_SetVar2(interp, name1, name2, glob_chanset, TCL_GLOBAL_ONLY);
  }
  return NULL;
}
cmd_t channels_bot[] = {
  {"cjoin",    "", (Function) got_cjoin, NULL},
  {"cpart",    "", (Function) got_cpart, NULL},
  {"cset",     "", (Function) got_cset,  NULL},
  {"cycle",    "", (Function) got_cycle, NULL},
  {"down",     "", (Function) got_down,  NULL},
  {"rl", "", (Function) got_role, NULL},
  {"sj",       "", (Function) got_sj,    NULL},
  {"sp",       "", (Function) got_sp,    NULL},
/*
#ifdef HUB
  {"o1", "", (Function) got_o1, NULL},
  {"kl", "", (Function) got_kl, NULL},
#endif
  {"ltp", "", (Function) got_locktopic, NULL},
*/
  {0, 0, 0, 0}
};

static tcl_ints my_tcl_ints[] =
{
  {"share-greet",		NULL,				0},
  {"use-info",			&use_info,			0},
  {"quiet-save",		&quiet_save,			0},
  {"global-stopnethack-mode",	&global_stopnethack_mode,	0},
  {"global-revenge-mode",       &global_revenge_mode,           0},
  {"global-idle-kick",		&global_idle_kick,		0},
  {"global-ban-time",           &global_ban_time,               0},
#ifdef S_IRCNET
  {"global-exempt-time",        &global_exempt_time,            0},
  {"global-invite-time",        &global_invite_time,            0},
#endif
  /* keeping [ban|exempt|invite]-time for compatability <Wcc[07/20/02]> */
  {"ban-time",                  &global_ban_time,               0},
#ifdef S_IRCNET
  {"exempt-time",               &global_exempt_time,            0},
  {"invite-time",               &global_invite_time,            0},
#endif
  {NULL,			NULL,				0}
};

static tcl_coups mychan_tcl_coups[] =
{
  {"global-flood-chan",		&gfld_chan_thr,		&gfld_chan_time},
  {"global-flood-deop",		&gfld_deop_thr,		&gfld_deop_time},
  {"global-flood-kick",		&gfld_kick_thr,		&gfld_kick_time},
  {"global-flood-join",		&gfld_join_thr,		&gfld_join_time},
  {"global-flood-ctcp",		&gfld_ctcp_thr,		&gfld_ctcp_time},
  {"global-flood-nick",		&gfld_nick_thr, 	&gfld_nick_time},
  {NULL,			NULL,			NULL}
};

static tcl_strings my_tcl_strings[] =
{
  {"global-chanmode",	glob_chanmode,	64,	0},
  {NULL,		NULL,		0,	0}
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
#ifdef S_IRCNET
  (Function) u_delexempt,
  (Function) u_addexempt,
#else
  (Function) NULL,
  (Function) NULL,
#endif
  (Function) NULL,
  /* 32 - 35 */
  (Function) NULL,/* [32] used to be u_sticky_exempt() <cybah> */
  (Function) NULL,
  (Function) NULL,	/* [34] used to be killchanset().	*/
#ifdef S_IRCNET
  (Function) u_delinvite,
  /* 36 - 39 */
  (Function) u_addinvite,
#else 
  (Function) NULL,
  (Function) NULL,
#endif
  (Function) tcl_channel_add,
  (Function) tcl_channel_modify,
#ifdef S_IRCNET
  (Function) write_exempts,
  /* 40 - 43 */
  (Function) write_invites,
#else
  (Function) NULL,
  (Function) NULL,
#endif
  (Function) ismodeline,
  (Function) initudef,
  (Function) ngetudef,
  /* 44 - 47 */
  (Function) expired_mask,
  (Function) remove_channel,
  (Function) & global_ban_time,
#ifdef S_IRCNET
  (Function) & global_exempt_time,
  /* 48 - 51 */
  (Function) & global_invite_time,
#else
  (Function) NULL,
  (Function) NULL,
#endif
  (Function) write_chans,
  (Function) write_config,
};

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
#ifdef S_IRCNET
  global_exempt_time = 0;
  global_invite_time = 0;
#endif
  strcpy(glob_chanset,
         "+enforcebans "
	 "+dynamicbans "
	 "+userbans "
	 "-bitch "
	 "-protectops "
	 "-revenge "
	 "+cycle "
	 "+dontkickops "
	 "-inactive "
	 "+userexempts "
	 "-dynamicexempts "
	 "+userinvites "
	 "-dynamicinvites "
	 "-revengebot "
	 "+nodesynch "
	 "-closed "
	 "-take "
	 "+manop "
	 "-voice "
         "-private "
	 "-fastop ");
  module_register(MODULE_NAME, channels_table, 1, 0);
#ifdef LEAF
  add_hook(HOOK_MINUTELY, (Function) check_limitraise);
#endif
#ifdef HUB
  add_hook(HOOK_30SECONDLY, (Function) rebalance_roles);
#endif
  add_hook(HOOK_HOURLY, (Function) warn_pls_take);
  add_hook(HOOK_MINUTELY, (Function) check_expired_bans);
#ifdef S_IRCNET
  add_hook(HOOK_MINUTELY, (Function) check_expired_exempts);
  add_hook(HOOK_MINUTELY, (Function) check_expired_invites);
#endif
  add_hook(HOOK_USERFILE, (Function) channels_writeuserfile);
  add_hook(HOOK_3SECONDLY, (Function) channels_checkslowjoin);
  Tcl_TraceVar(interp, "global-chanset",
	       TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
	       traced_globchanset, NULL);
  add_builtins(H_chon, my_chon);
  add_builtins_dcc(H_dcc, C_dcc_irc);
  add_builtins(H_bot, channels_bot);
  add_tcl_commands(channels_cmds);
  add_tcl_strings(my_tcl_strings);
  my_tcl_ints[0].val = &share_greet;
  add_tcl_ints(my_tcl_ints);
  add_tcl_coups(mychan_tcl_coups);
  return NULL;
}
