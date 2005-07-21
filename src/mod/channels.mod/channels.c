/*
 * channels.c -- part of channels.mod
 *   support for channels within the bot
 *
 */


#define MAKING_CHANNELS
#include "src/common.h"
#include "src/mod/share.mod/share.h"
#include "src/mod/irc.mod/irc.h"
#include "src/mod/server.mod/server.h"
#include "src/chanprog.h"
#include "src/egg_timer.h"
#include "src/misc.h"
#include "src/main.h"
#include "src/color.h"
#include "src/userrec.h"
#include "src/users.h"
#include "src/set.h"
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

static bool 			use_info = 1;
static char 			glob_chanmode[64] = "nt";		/* Default chanmode (drummer,990731) */
static int 			global_stopnethack_mode;
static int 			global_revenge_mode = 3;
static int 			global_idle_kick;		/* Default idle-kick setting. */
static time_t 			global_ban_time;
static time_t			global_exempt_time;
static time_t 			global_invite_time;


/* Global channel settings (drummer/dw) */
char glob_chanset[512];
static char *lastdeletedmask = NULL;

/* Global flood settings */
static int 			gfld_chan_thr;
static time_t 			gfld_chan_time;
static int 			gfld_deop_thr = 8;
static time_t 			gfld_deop_time = 10;
static int 			gfld_kick_thr;
static time_t 			gfld_kick_time;
static int 			gfld_join_thr;
static time_t 			gfld_join_time;
static int 			gfld_ctcp_thr = 5;
static time_t 			gfld_ctcp_time = 30;
static int			gfld_nick_thr;
static time_t 			gfld_nick_time;

static int 			killed_bots = 0;

#include "channels.h"
#include "cmdschan.c"
#include "tclchan.c"
#include "userchan.c"

/* This will close channels if the HUB:leaf count is skewed from config setting */
static void 
check_should_close()
{
  int H = close_threshold.count, L = close_threshold.time;

  if ((H <= 0) || (L <= 0))
    return;

  int hc = 1, lc = 0;
  struct userrec *u = NULL;

  for (tand_t *bot = tandbot; bot; bot = bot->next) {
    if ((u = get_user_by_handle(userlist, bot->bot))) {
      if (bot_hublevel(u) < 999)
        hc++;
      else
        lc++;
    }
  }
  if ((hc >= H) && (lc <= L)) {
    for (struct chanset_t *chan = chanset; chan; chan = chan->next) {
      if (!channel_closed(chan)) {
        do_chanset(NULL, chan, "+closed chanmode +stni", DO_LOCAL | DO_NET);
#ifdef G_BACKUP
        chan->channel.backup_time = now + 30;
#endif /* G_BACKUP */
      }
    }
  }
}

static void got_cset(char *botnick, char *code, char *par)
{
  if (!par || !par[0])
   return;

  bool all = 0;
  char *chname = NULL;
  struct chanset_t *chan = NULL;

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

  if (all)
   chan = chanset;

  while (chan) {
    chname = chan->dname;
    do_chanset(NULL, chan, par, DO_LOCAL);
    if (!conf.bot->hub && chan->status & CHAN_BITCH)
      recheck_channel(chan, 0);
    if (!all)
      chan = NULL;
    else
      chan = chan->next;
  }
}

/* returns 1 if botn is in bots */

static int
parsebots(char *bots, char *botn) {
  if (!strcmp(bots, "*")) {
    return 1;
  } else {
    char *list = strdup(bots), *bot = strtok(list, ",");

    while(bot && *bot) {
      if (!egg_strcasecmp(bot, botn))
        return 1;
      bot = strtok((char*) NULL, ",");
    }
    free(list);
  }
  return 0;
}

static void got_cpart(char *botnick, char *code, char *par)
{
  if (!par[0])
   return;

  char *chname = newsplit(&par);
  struct chanset_t *chan = NULL;

  if (!(chan = findchan_by_dname(chname)))
   return;

  char *bots = newsplit(&par);
  int match = 0;

  /* if bots is '*' just remove_channel */
  if (!strcmp(bots, "*"))
    match = 0;
  else
    match = parsebots(bots, conf.bot->nick);
 
  if (match)
    do_chanset(NULL, chan, "+inactive", DO_LOCAL);
  else
    remove_channel(chan);

  if (conf.bot->hub)
    write_userfile(-1);
}

static void got_cjoin(char *botnick, char *code, char *par)
{
  if (!par[0])
   return;

  char *chname = newsplit(&par), *options = NULL;
  struct chanset_t *chan = findchan_by_dname(chname);
  int match = 0;

  if (conf.bot->hub) {
    newsplit(&par);	/* hubs ignore the botmatch param */
    options = par;
  } else {
    /* ALL hubs should add the channel, leaf should check the list for a match */
    bool inactive = 0;
    char *bots = newsplit(&par);
    match = parsebots(bots, conf.bot->nick);

    if (strstr(par, "+inactive"))
      inactive = 1;

    if (chan && !match)
      return;

    if (!match) {
      size_t size = strlen(par) + 12 + 1;

      options = (char *) my_calloc(1, size);
      egg_snprintf(options, size, "%s +inactive", par);
    } else if (match && chan && !shouldjoin(chan)) {
      if (!inactive)
        do_chanset(NULL, chan, "-inactive", DO_LOCAL);
      return;
    } else
      options = par;
  }

  if (chan)
    return;
sdprintf("OPTIONS: %s", options);
  char result[1024] = "";

  if (channel_add(result, chname, options) == ERROR) /* drummer */
    putlog(LOG_BOTS, "@", "Invalid channel or channel options from %s for %s: %s", botnick, chname, result);
  if (conf.bot->hub)
    write_userfile(-1);
  if (!match && !conf.bot->hub)
    free(options);
}

static void got_cycle(char *botnick, char *code, char *par)
{
  if (!par[0])
   return;

  char *chname = newsplit(&par);
  struct chanset_t *chan = NULL;

  if (!(chan = findchan_by_dname(chname)))
   return;

  time_t delay = 10;

  if (par[0])
    delay = atoi(newsplit(&par));
  
  do_chanset(NULL, chan, "+inactive", DO_LOCAL);
  dprintf(DP_SERVER, "PART %s\n", chan->name);
  chan->channel.jointime = ((now + delay) - server_lag); 		/* rejoin in 10 seconds */
}

static void got_down(char *botnick, char *code, char *par)
{
  if (!par[0])
   return;

  char *chname = newsplit(&par);
  struct chanset_t *chan = NULL;

  if (!(chan = findchan_by_dname(chname)))
   return;
 
  chan->channel.no_op = (now + 10);
  add_mode(chan, '-', 'o', botname);
}

static void got_role(char *botnick, char *code, char *par)
{
  role = atoi(newsplit(&par));
  putlog(LOG_DEBUG, "@", "Got role index %d", role);
}

void got_kl(char *botnick, char *code, char *par)
{
  killed_bots++;
  if (kill_threshold && (killed_bots == kill_threshold)) {
    for (struct chanset_t *ch = chanset; ch; ch = ch->next)
      do_chanset(NULL, ch, "+closed +bitch +backup", DO_LOCAL | DO_NET);
  /* FIXME: we should randomize nick here ... */
  }
}


static void rebalance_roles()
{
  struct bot_addr *ba = NULL;
  int r[5] = { 0, 0, 0, 0, 0 };
  unsigned int hNdx, lNdx, i;
  char tmp[10] = "";

  for (i = 0; i < (unsigned) dcc_total; i++) {
    if (dcc[i].type && dcc[i].user && dcc[i].user->bot && bot_hublevel(dcc[i].user) == 999) {
      ba = (struct bot_addr *) get_user(&USERENTRY_BOTADDR, dcc[i].user);
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
      if (dcc[i].type && dcc[i].user && dcc[i].user->bot && bot_hublevel(dcc[i].user) == 999) {
        ba = (struct bot_addr *) get_user(&USERENTRY_BOTADDR, dcc[i].user);
        if (ba && (ba->roleid == (hNdx + 1))) {
          ba->roleid = lNdx + 1;
          simple_sprintf(tmp, "rl %d", lNdx + 1);
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

static int
check_slowjoinpart(struct chanset_t *chan)
{
  /* slowpart */
  if (channel_active(chan) && (chan->channel.parttime) && (chan->channel.parttime < now)) {
    chan->channel.parttime = 0;
    if (!conf.bot->hub)
      dprintf(DP_MODE, "PART %s\n", chan->name);
    if (chan) /* this should NOT be necesary, but some unforseen bug requires it.. */
      remove_channel(chan);
    return 1;		/* if we keep looping, we'll segfault. */
  /* slowjoin */
  } else if ((chan->channel.jointime) && (chan->channel.jointime < now)) {
      chan->status &= ~CHAN_INACTIVE;
      chan->channel.jointime = 0;
    if (!conf.bot->hub && shouldjoin(chan) && !channel_active(chan) && !channel_joining(chan)) {
      dprintf(DP_MODE, "JOIN %s %s\n", chan->dname, chan->key_prot);
      chan->status |= CHAN_JOINING;
    }
  } else if (channel_closed(chan)) {
    enforce_closed(chan);
  }
  return 0;
}

static void 
check_limitraise(struct chanset_t *chan) {
  /* only check every other time for now */
  chan->checklimit++;
  if (chan->checklimit == 2) {
    chan->checklimit = 0;
    if (chan->limitraise && dolimit(chan))
      raise_limit(chan);
  }
}

static void
channels_timers()
{
  static int cnt = 0;
  struct chanset_t *chan_n = NULL, *chan = NULL;
  bool reset = 0;

  cnt += 10;		/* function is called every 10 seconds */
  
  for (chan = chanset; chan; chan = chan_n) {
    chan_n = chan->next;

    if ((cnt % 10) == 0) {
      /* 10 seconds */
      if (check_slowjoinpart(chan))	/* if 1 is returned, chan was removed. */
        continue;
    }
    if ((cnt % 60) == 0) {
      /* 60 seconds */
      reset = 1;
      if (!conf.bot->hub)
        check_limitraise(chan);
    }
  }

  if (reset)
    cnt = 0;
}

static void got_sj(int idx, char *code, char *par) 
{
  struct chanset_t *chan = findchan_by_dname(newsplit(&par));

  if (chan)
    chan->channel.jointime = ((atoi(par) + now) - server_lag);
}

static void got_sp(int idx, char *code, char *par) 
{
  struct chanset_t *chan = findchan_by_dname(newsplit(&par));

  if (chan)
    chan->channel.parttime = ((atoi(par) + now) - server_lag);
}

#ifdef no
/* got_jn
 * We get this when a bot is opped in a +take chan
 * we are to set -inactive, jointime = 0, and join.
 */

static void got_jn(int idx, char *code, char *par)
{
  char *chname = newsplit(&par);

  if (!chname || !chname[0]) 
    return;

  struct chanset_t *chan = NULL;

  if (!(chan = findchan_by_dname(chname))) return;
  if (chan->channel.jointime && channel_inactive(chan)) {
    chan->status &= ~CHAN_INACTIVE;
    chan->channel.jointime = 0;
    if (!conf.bot->hub && shouldjoin(chan) && !channel_active(chan) && !channel_joining(chan)) {
      dprintf(DP_MODE, "JOIN %s %s\n", chan->name, chan->key_prot);
      chan->status |= CHAN_JOINING;
    }
  }
}
#endif

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
  int tst;
  bool ok = 0;

  for (int i = 0; i < 2; i++) {
    ok = 0;
    if (i == 0) {
      tst = chan->mode_pls_prot;
      if ((tst) || (chan->limit_prot != 0) || (chan->key_prot[0]))
	*p++ = '+';
      if (chan->limit_prot != 0) {
	*p++ = 'l';
	simple_sprintf(&s1[strlen(s1)], "%d ", chan->limit_prot);
      }
      if (chan->key_prot[0]) {
	*p++ = 'k';
	simple_sprintf(&s1[strlen(s1)], "%s ", chan->key_prot);
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
bool ismodeline(masklist *m, const char *username)
{
  for (; m && m->mask[0]; m = m->next)  
    if (!rfc_casecmp(m->mask, username))
      return 1;
  return 0;
}

/* Returns true if user matches one of the masklist -- drummer
 */
bool ismasked(masklist *m, const char *username)
{
  for (; m && m->mask[0]; m = m->next)
    if (wild_match(m->mask, (char *) username))
      return 1;
  return 0;
}

/* Unlink chanset element from chanset list.
 */
static inline bool chanset_unlink(struct chanset_t *chan)
{
  struct chanset_t *c_old = NULL;

  for (struct chanset_t *c = chanset; c; c_old = c, c = c->next) {
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
   int i;
   
   irc_log(chan, "Parting");
   /* Remove the channel from the list, so that noone can pull it
      away from under our feet during the check_part() call. */
   chanset_unlink(chan);

  /* Using chan->name is important here, especially for !chans <cybah> */
  if (!conf.bot->hub && shouldjoin(chan) && chan->name[0])
    dprintf(DP_SERVER, "PART %s\n", chan->name);

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
   for (i = 0; i < MODES_PER_LINE_MAX && chan->cmode[i].op; i++)
     free(chan->cmode[i].op);
   for (i = 0; i < (MODES_PER_LINE_MAX - 1) && chan->ccmode[i].op; i++)
     free(chan->ccmode[i].op);
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
  struct flag_record fr = {FR_CHAN | FR_ANYWH | FR_GLOBAL, 0, 0, 0 };
  int find;
  bool found = 0;
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
  {"*",		"",	(Function) channels_chon,	"channels:chon", 0},
  {NULL,	NULL,	NULL,				NULL, 0}
};

void channels_report(int idx, int details)
{
  int i;
  char s[1024] = "", s2[100] = "";
  struct flag_record fr = {FR_CHAN | FR_GLOBAL, 0, 0, 0 };

  for (struct chanset_t *chan = chanset; chan; chan = chan->next) {
    if (idx != DP_STDOUT)
      get_user_flagrec(dcc[idx].user, &fr, chan->dname);
    if (!privchan(fr, chan, PRIV_OP) && ((idx == DP_STDOUT) || glob_master(fr) || chan_master(fr))) {

      s[0] = 0;

      if (chan_bitch(chan))
	strcat(s, "bitch, ");
      if (s[0])
	s[strlen(s) - 2] = 0;
      if (!s[0])
	strcpy(s, "lurking");
      get_mode_protect(chan, s2);
      if (channel_closed(chan)) {
        if (chan->closed_invite)
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
          if (!conf.bot->hub)
            dprintf(idx, "    %-10s: (%s), enforcing \"%s\"  (%s)\n", chan->dname,
		  channel_pending(chan) ? "pending" : "not on channel", s2, s);
          else
            dprintf(idx, "    %-10s: (%s), enforcing \"%s\"  (%s)\n", chan->dname,
		  "limbo", s2, s);
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
        if (have_take && channel_take(chan))
          i += my_strcpy(s + i, "take ");
        if (channel_voice(chan))
          i += my_strcpy(s + i, "voice ");
        if (channel_autoop(chan))
          i += my_strcpy(s + i, "autoop ");
/* Chanflag template
 *	if (channel_temp(chan))
 *	  i += my_strcpy(s + i, "temp ");
*/
        if (channel_botbitch(chan))
          i += my_strcpy(s + i, "botbitch ");
        if (channel_fastop(chan))
          i += my_strcpy(s + i, "fastop ");
        if (channel_privchan(chan))
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
       dprintf(idx, "    Bans last %lu mins.\n", chan->ban_time);
       dprintf(idx, "    Exemptions last %lu mins.\n", chan->exempt_time);
       dprintf(idx, "    Invitations last %lu mins.\n", chan->invite_time);
      }
    }
  }
}

cmd_t channels_bot[] = {
  {"cjoin",	"", 	(Function) got_cjoin, 	NULL, 0},
  {"cpart",	"", 	(Function) got_cpart, 	NULL, 0},
  {"cset",	"", 	(Function) got_cset,  	NULL, 0},
  {"cycle",	"", 	(Function) got_cycle, 	NULL, LEAF},
  {"down",	"", 	(Function) got_down,  	NULL, LEAF},
  {"rl",	"", 	(Function) got_role,  	NULL, 0},
  {"kl",	"", 	(Function) got_kl,    	NULL, 0},
  {"sj",	"", 	(Function) got_sj,    	NULL, 0},
  {"sp",	"", 	(Function) got_sp,    	NULL, 0},
//  {"jn",	"", 	(Function) got_jn,    	NULL, 0},
/*
#ifdef HUB
  {"o1", "", (Function) got_o1, NULL, 0},
  {"kl", "", (Function) got_kl, NULL, 0},
#endif
  {"ltp", "", (Function) got_locktopic, NULL, 0},
*/
  {NULL, 	NULL, 	NULL, 			NULL, 0}
};


void channels_init()
{
  timer_create_secs(60, "check_expired_masks", (Function) check_expired_masks);
  if (conf.bot->hub) {
    timer_create_secs(30, "rebalance_roles", (Function) rebalance_roles);
    timer_create_secs(30, "check_should_close", (Function) check_should_close);
#ifdef G_BACKUP
    timer_create_secs(30, "check_should_backup", (Function) check_should_backup);
#endif /* G_BACKUP */
  }
  timer_create_secs(10, "channels_timers", (Function) channels_timers);

  add_builtins("dcc", C_dcc_channels);
  add_builtins("bot", channels_bot);
  add_builtins("chon", my_chon);
}
