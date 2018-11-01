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
#include "src/binds.h"
#include "src/cmds.h"
#include <bdlib/src/String.h>


#include <sys/stat.h>

static bool 			use_info = 1;
static char 			glob_chanmode[64] = "nt";		/* Default chanmode (drummer,990731) */
static interval_t 			global_ban_time;
static interval_t			global_exempt_time;
static interval_t 			global_invite_time;


static char *lastdeletedmask = NULL;

static int 			killed_bots = 0;

#include "channels.h"
#include "cmdschan.cc"
#include "chanmisc.cc"
#include "userchan.cc"

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

  bool all = 0, isdefault = 0;
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  if (par[0] == '*' && par[1] == ' ') {
    all = 1;
    newsplit(&par);
   } else {
     chname = newsplit(&par);
     if (!strcasecmp(chname, "default"))
       isdefault = 1;
     else if (!strchr(CHANMETA, chname[0])) {
       putlog(LOG_ERROR, "*", "Got bad cset: bot: %s code: %s par: %s %s", botnick, code, chname, par);
       return;
     }
     if (isdefault)
       chan = chanset_default;
     else if (!(chan = findchan_by_dname(chname)))
       return;
   }

  if (all)
   chan = NULL;
  do_chanset(NULL, chan, par, DO_LOCAL);
}

/* returns 1 if botn is in bots */

static int
parsebots(char *bots, char *botn) {
  if (!strcmp(bots, "*")) {
    return 1;
  } else {
    char *list = strdup(bots), *bot = strtok(list, ",");

    while(bot && *bot) {
      if (!strcasecmp(bot, botn))
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

void rcmd_chans(char *fbot, char *fhand, char *fidx) {
  if (conf.bot->hub)
    return;

  struct chanset_t *chan = NULL;
  char buf[1024] = "", reply[1024] = "";

  if (server_online) {
    for (chan = chanset; chan; chan = chan->next) {
      if (!channel_active(chan) && (shouldjoin(chan) || chan->channel.jointime)) {
        if (buf[0])
          strlcat(buf, " ", sizeof(buf));
        strlcat(buf, chan->dname, sizeof(buf));
      }
    }

    if (buf[0])
      simple_snprintf(reply, sizeof(reply), "[%s] I am not in: %s", cursrvname, buf);
  } else
    simple_snprintf(reply, sizeof(reply), "I am not online.");

  if (reply[0])
    botnet_send_cmdreply(conf.bot->nick, fbot, fhand, fidx, reply);
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

      options = (char *) calloc(1, size);
      simple_snprintf(options, size, "%s +inactive", par);
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
  char result[RESULT_LEN] = "";

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

  interval_t delay = 10;

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

void got_kl(char *botnick, char *code, char *par)
{
  killed_bots++;
  if (kill_threshold && (killed_bots == kill_threshold)) {
    for (struct chanset_t *ch = chanset; ch; ch = ch->next)
      do_chanset(NULL, ch, "+closed +bitch +backup", DO_LOCAL | DO_NET);
  /* FIXME: we should randomize nick here ... */
  }
}

static int
check_slowjoinpart(struct chanset_t *chan)
{
  /* slowpart */
  if (chan->channel.parttime && (chan->channel.parttime < now)) {
    chan->channel.parttime = 0;
    dprintf(DP_MODE, "PART %s\n", chan->name);
    if (chan) /* this should NOT be necesary, but some unforseen bug requires it.. */
      remove_channel(chan);
    return 1;		/* if we keep looping, we'll segfault. */
  /* slowjoin */
  } else if ((chan->channel.jointime) && (chan->channel.jointime < now)) {
      chan->status &= ~CHAN_INACTIVE;
      chan->channel.jointime = 0;
    if (shouldjoin(chan))
      join_chan(chan);
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
      if (!conf.bot->hub && check_slowjoinpart(chan))	/* if 1 is returned, chan was removed. */
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

  if (chan) {
    if (conf.bot->hub) {
      chan->status &= ~CHAN_INACTIVE;
      write_userfile(-1);
    } else
      chan->channel.jointime = ((atoi(par) + now) - server_lag);
  }
}

static void got_sp(int idx, char *code, char *par) 
{
  struct chanset_t *chan = findchan_by_dname(newsplit(&par));

  if (chan) {
    if (conf.bot->hub) {
      remove_channel(chan);
      write_userfile(-1);
    } else
      chan->channel.parttime = ((atoi(par) + now) - server_lag);
  }
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
    if (!conf.bot->hub && shouldjoin(chan))
      join_chan(chan);
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
	  strlcpy(chan->key_prot, s1, sizeof(chan->key_prot));
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

  if (chan->mode_mns_prot & CHANPRIV && chan->closed_private)
    chan->closed_private = 0;

  if (chan->mode_mns_prot & CHANINV && chan->closed_invite)
    chan->closed_invite = 0;

  if (chan->mode_mns_prot & CHANMODER && chan->voice_moderate)
    chan->voice_moderate = 0;
}

static void get_mode_protect(struct chanset_t *chan, char *s, size_t ssiz)
{
  char *p = s, s1[121] = "";
  int tst;

  for (int i = 0; i < 2; i++) {
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
    strlcat(s, " ", ssiz);
    strlcat(s, s1, ssiz);
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

/* Completely removes a channel.
 *
 * This includes the removal of all channel-bans, -exempts and -invites, as
 * well as all user flags related to the channel.
 */
void remove_channel(struct chanset_t *chan)
{
   if (chan != chanset_default) {
     irc_log(chan, "Parting");
     /* Remove the channel from the list, so that noone can pull it
        away from under our feet during the check_part() call. */
     list_delete((struct list_type **) &chanset, (struct list_type *) chan);
     chanset_by_dname.remove(chan->dname);

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
   }
   free(chan->channel.key);
   if (chan->key)
     free(chan->key);
   if (chan->rmkey)
     free(chan->rmkey);
   if (chan->groups) {
     delete(chan->groups);
   }
   delete chan->bot_roles;
   delete chan->role_bots;
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
	get_user_flagrec(dcc[idx].user, &fr, chan->dname, chan);
	if (fr.chan & find)
	  found = 1;
	else
	  chan = chan->next;
      }
      if (!chan)
	chan = chanset;

      struct chat_info dummy;
      if (chan)
	strlcpy(dcc[idx].u.chat->con_chan, chan->dname, sizeof(dummy.con_chan));
      else
	strlcpy(dcc[idx].u.chat->con_chan, "*", 2);
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
      get_user_flagrec(dcc[idx].user, &fr, chan->dname, chan);
    if (!privchan(fr, chan, PRIV_OP) && ((idx == DP_STDOUT) || glob_master(fr) || chan_master(fr))) {

      s[0] = 0;

      if (chan_bitch(chan))
	strlcat(s, "bitch, ", sizeof(s));
      if (s[0])
	s[strlen(s) - 2] = 0;
      if (!s[0])
	strlcpy(s, "lurking", sizeof(s));
      get_mode_protect(chan, s2, sizeof(s2));
      if (channel_closed(chan)) {
        if (chan->closed_invite)
          strlcat(s2, "i", sizeof(s2));
        if (chan->closed_private)
          strlcat(s2, "p", sizeof(s2));
      }
      if (channel_voice(chan) && chan->voice_moderate) {
        strlcat(s2, "m", sizeof(s2));
      }

      if (conf.bot->hub || shouldjoin(chan)) {
	if (channel_active(chan)) {
	  /* If it's a !chan, we want to display it's unique name too <cybah> */
	  if (chan->dname[0]=='!') {
	    dprintf(idx, "    %-20s: %2d member%s enforcing \"%s\" (%s), "
	            "unique name %s\n", chan->dname, chan->channel.members,
	            (chan->channel.members==1) ? "," : "s,", s2, s, chan->name);
	  } else {
	    dprintf(idx, "    %-20s: %2d member%s enforcing \"%s\" (%s)\n",
	            chan->dname, chan->channel.members,
	            chan->channel.members == 1 ? "," : "s,", s2, s);
	  }
	} else {
          if (!conf.bot->hub)
            dprintf(idx, "    %-20s: (%s), enforcing \"%s\"  (%s)\n", chan->dname,
		  channel_pending(chan) ? "pending" : "not on channel", s2, s);
          else
            dprintf(idx, "    %-20s: (%s), enforcing \"%s\"  (%s)\n", chan->dname,
		  "limbo", s2, s);
	}
      } else {
	dprintf(idx, "    %-20s: channel is set +inactive\n",
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
        if (HAVE_TAKE && channel_take(chan))
          i += my_strcpy(s + i, "take ");
        if (channel_voice(chan))
          i += my_strcpy(s + i, "voice ");
        if (channel_autoop(chan))
          i += my_strcpy(s + i, "autoop ");
        if (channel_rbl(chan))
          i += my_strcpy(s + i, "rbl ");
        if (channel_voicebitch(chan))
          i += my_strcpy(s + i, "voicebitch ");
        if (channel_protect(chan))
          i += my_strcpy(s + i, "protect ");
/* Chanflag template
 *	if (channel_temp(chan))
 *	  i += my_strcpy(s + i, "temp ");
*/
        if (channel_botbitch(chan))
          i += my_strcpy(s + i, "botbitch ");
        if (channel_backup(chan))
          i += my_strcpy(s + i, "backup ");
        if (channel_fastop(chan))
          i += my_strcpy(s + i, "fastop ");
        if (channel_privchan(chan))
          i += my_strcpy(s + i, "private ");

	dprintf(idx, "      Options: %s\n", s);
        if (chan->limitraise)
          dprintf(idx, "      Raising limit +%d every 2 minutes\n", chan->limitraise);
       dprintf(idx, "    Bans last %d mins.\n", chan->ban_time);
       dprintf(idx, "    Exemptions last %d mins.\n", chan->exempt_time);
       dprintf(idx, "    Invitations last %d mins.\n", chan->invite_time);
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
/* vim: set sts=2 sw=2 ts=8 et: */
