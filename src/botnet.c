
/* 
 * botnet.c -- handles:
 *   keeping track of which bot's connected where in the chain
 *   dumping a list of bots or a bot tree to a user
 *   channel name associations on the party line
 *   rejecting a bot
 *   linking, unlinking, and relaying to another bot
 *   pinging the bots periodically and checking leaf status
 * 
 * dprintf'ized, 28nov1995
 * 
 * $Id: botnet.c,v 1.11 2000/01/17 16:14:44 per Exp $
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

#include "main.h"
#include "tandem.h"

extern char spaces[],
  spaces2[];
extern int dcc_total,
  backgrd,
  connect_timeout,
  max_dcc,
  cfg_count,
  egg_numver;
extern struct userrec *userlist;
extern struct chanset_t * chanset;
extern struct cfg_entry ** cfg;
extern struct dcc_t *dcc;
extern time_t now;
#ifdef G_AUTOLOCK
extern struct cfg_entry CFG_LOCKTHRESHOLD;
#endif

#ifdef G_USETCL
extern Tcl_Interp *interp;
#endif
tand_t *tandbot;		/* keep track of tandem bots on the botnet */
party_t *party;			/* keep track of people on the botnet */
int maxparty = 50;		/* maximum space for party line members

				   * currently */
int tands = 0;			/* number of bots on the botnet */
int parties = 0;		/* number of people on the botnet */
char botnetnick[HANDLEN + 1] = "";	/* botnet nickname */
int share_unlinks = 1;		/* allow remote unlinks of my sharebots ? */

int expmem_botnet()
{
  int size = 0,
    i;
  tand_t *bot;

  for (bot = tandbot; bot; bot = bot->next)
    size += sizeof(tand_t);
  Context;
  size += (maxparty * sizeof(party_t));
  for (i = 0; i < parties; i++) {
    if (party[i].away)
      size += strlen(party[i].away) + 1;
    if (party[i].from)
      size += strlen(party[i].from) + 1;
  }
  return size;
}

void init_bots()
{
  tandbot = NULL;
  /* grab space for 50 bots for now -- expand later as needed */
  maxparty = 50;
  party = (party_t *) nmalloc(maxparty * sizeof(party_t));
}

tand_t *findbot(char *who)
{
  tand_t *ptr = tandbot;

  while (ptr) {
    if (!strcasecmp(ptr->bot, who))
      return ptr;
    ptr = ptr->next;
  }
  return NULL;
}

/* add a tandem bot to our chain list */
void addbot(char *who, char *from, char *next, char flag, int vernum)
{
  tand_t **ptr = &tandbot,
   *ptr2;

  Context;
  while (*ptr) {
    if (!strcasecmp((*ptr)->bot, who)) {
      log(LCAT_BOT, STR("!!! Duplicate botnet bot entry!!"));
      log(LCAT_ERROR, STR("!!! Duplicate botnet bot entry!!"));
    }
    ptr = &((*ptr)->next);
  }
  ptr2 = nmalloc(sizeof(tand_t));
  strncpy0(ptr2->bot, who, HANDLEN+1);
  ptr2->share = flag;
  ptr2->ver = vernum;
  ptr2->next = *ptr;
  *ptr = ptr2;
  /* may be via itself */
  ptr2->via = findbot(from);
  if (!strcasecmp(next, botnetnick))
    ptr2->uplink = (tand_t *) 1;
  else
    ptr2->uplink = findbot(next);
  tands++;
}

#ifdef HUB
#ifdef G_BACKUP
void check_should_backup() {
  struct chanset_t * chan;
  for (chan=chanset; chan; chan = chan->next) {
    if (chan->backup_time && (chan->backup_time<now) && !channel_backup(chan)) {
      do_channel_modify(chan, STR("+backup"));
      chan->backup_time = 0;
    }
  }
}
#endif

#ifdef G_AUTOLOCK
void check_should_lock() {
  char * p = CFG_LOCKTHRESHOLD.gdata;
  tand_t * bot;
  int H, L, hc, lc;
  struct chanset_t * chan;
  if (!p) 
    return;
  H=atoi(p);
  p=strchr(p, ':');
  if (!p) 
    return;
  p++;
  L=atoi(p);
  if ((H<=0) || (L<=0))
    return;
  hc=1;
  lc=0;
  for (bot = tandbot; bot; bot = bot->next) {
    struct userrec * u = get_user_by_handle(userlist, bot->bot);
    if (u) {
      if (bot_hublevel(u)<999)
        hc++;
      else
	lc++;
    }
  }
  if ((hc>=H) && (lc<=L)) {
    for (chan=chanset;chan;chan=chan->next) {
      if (!channel_locked(chan)) {
	do_channel_modify(chan, STR("+locked chanmode +stni"));
#ifdef G_BACKUP
	chan->backup_time=now+30;
#endif
      }
    }
  }

}
#endif
#endif

void updatebot(int idx, char *who, char share, int vernum)
{
  tand_t *ptr = findbot(who);

  Context;
  if (ptr) {
    if (share)
      ptr->share = share;
    if (vernum)
      ptr->ver = vernum;
    botnet_send_update(idx, ptr);
  }
}

/* for backward 1.0 compatibility: */

/* grab the (first) sock# for a user on another bot */
int partysock(char *bot, char *nick)
{
  int i;

  for (i = 0; i < parties; i++) {
    if ((!strcasecmp(party[i].bot, bot)) && (!strcasecmp(party[i].nick, nick)))
      return party[i].sock;
  }
  return 0;
}

/* new botnet member */
int addparty(char *bot, char *nick, int chan, char flag, int sock, char *from, int *idx)
{
  int i;

  Context;
  for (i = 0; i < parties; i++) {
    /* just changing the channel of someone already on? */
    if (!strcasecmp(party[i].bot, bot) && (party[i].sock == sock)) {
      int oldchan = party[i].chan;

      party[i].chan = chan;
      party[i].timer = now;
      if (from[0]) {
	if (flag == ' ')
	  flag = '-';
	party[i].flag = flag;
	if (party[i].from)
	  nfree(party[i].from);
	party[i].from = nmalloc(strlen(from) + 1);
	strcpy(party[i].from, from);
      }
      *idx = i;
      return oldchan;
    }
  }
  /* new member */
  if (parties == maxparty) {
    maxparty += 50;
    party = (party_t *) nrealloc((void *) party, maxparty * sizeof(party_t));
  }
  strncpy0(party[parties].nick, nick, HANDLEN+1);
  strncpy0(party[parties].bot, bot, HANDLEN+1);
  party[parties].chan = chan;
  party[parties].sock = sock;
  party[parties].status = 0;
  party[parties].away = 0;
  party[parties].timer = now;	/* cope. */
  if (from[0]) {
    if (flag == ' ')
      flag = '-';
    party[parties].flag = flag;
    party[parties].from = nmalloc(strlen(from) + 1);
    strcpy(party[parties].from, from);
  } else {
    party[parties].flag = ' ';
    party[parties].from = nmalloc(10);
    strcpy(party[parties].from, STR("(unknown)"));
  }
  *idx = parties;
  parties++;
  return -1;
}

/* alter status flags for remote party-line user */
void partystat(char *bot, int sock, int add, int rem)
{
  int i;

  for (i = 0; i < parties; i++) {
    if ((!strcasecmp(party[i].bot, bot)) && (party[i].sock == sock)) {
      party[i].status |= add;
      party[i].status &= ~rem;
    }
  }
}

/* other bot is sharing idle info */
void partysetidle(char *bot, int sock, int secs)
{
  int i;

  for (i = 0; i < parties; i++) {
    if ((!strcasecmp(party[i].bot, bot)) && (party[i].sock == sock)) {
      party[i].timer = (now - (time_t) secs);
    }
  }
}

/* return someone's chat channel */
int getparty(char *bot, int sock)
{
  int i;

  for (i = 0; i < parties; i++) {
    if (!strcasecmp(party[i].bot, bot) && (party[i].sock == sock)) {
      return i;
    }
  }
  return -1;
}

/* un-idle someone */
int partyidle(char *bot, char *nick)
{
  int i,
    ok = 0;

  for (i = 0; i < parties; i++) {
    if ((!strcasecmp(party[i].bot, bot)) && (!strcasecmp(party[i].nick, nick))) {
      party[i].timer = now;
      ok = 1;
    }
  }
  return ok;
}

/* change someone's nick */
int partynick(char *bot, int sock, char *nick)
{
  char work[HANDLEN + 1];
  int i;

  for (i = 0; i < parties; i++) {
    if (!strcasecmp(party[i].bot, bot) && (party[i].sock == sock)) {
      strcpy(work, party[i].nick);
      strncpy0(party[i].nick, nick, HANDLEN+1);
      strcpy(nick, work);
      return i;
    }
  }
  return -1;
}

/* set away message */
void partyaway(char *bot, int sock, char *msg)
{
  int i;

  for (i = 0; i < parties; i++) {
    if ((!strcasecmp(party[i].bot, bot)) && (party[i].sock == sock)) {
      if (party[i].away)
	nfree(party[i].away);
      if (msg[0]) {
	party[i].away = nmalloc(strlen(msg) + 1);
	strcpy(party[i].away, msg);
      } else
	party[i].away = 0;
    }
  }
}

/* remove a tandem bot from the chain list */
void rembot(char *who)
{
  tand_t **ptr = &tandbot,
   *ptr2;

  Context;
  while (*ptr) {
    if (!strcasecmp((*ptr)->bot, who))
      break;
    ptr = &((*ptr)->next);
  }
  if (!*ptr)
    /* may have just .unlink *'d */
    return;
  check_tcl_disc(who);

  ptr2 = *ptr;
  *ptr = ptr2->next;
  nfree(ptr2);
  tands--;
}

void remparty(char *bot, int sock)
{
  int i;

  Context;
  for (i = 0; i < parties; i++)
    if ((!strcasecmp(party[i].bot, bot)) && (party[i].sock == sock)) {
      parties--;
      if (party[i].from)
	nfree(party[i].from);
      if (party[i].away)
	nfree(party[i].away);
      if (i < parties) {
	strcpy(party[i].bot, party[parties].bot);
	strcpy(party[i].nick, party[parties].nick);
	party[i].chan = party[parties].chan;
	party[i].sock = party[parties].sock;
	party[i].flag = party[parties].flag;
	party[i].status = party[parties].status;
	party[i].timer = party[parties].timer;
	party[i].from = party[parties].from;
	party[i].away = party[parties].away;
      }
    }
}

/* cancel every user that was on a certain bot */
void rempartybot(char *bot)
{
  int i;

  for (i = 0; i < parties; i++)
    if (!strcasecmp(party[i].bot, bot)) {
      if (party[i].chan >= 0)
	check_tcl_chpt(bot, party[i].nick, party[i].sock, party[i].chan);
      remparty(bot, party[i].sock);
      i--;
    }
}

/* remove every bot linked 'via' bot <x> */
void unvia(int idx, tand_t * who)
{
  tand_t *bot,
   *bot2;

  if (!who)
    return;			/* safety */
  rempartybot(who->bot);
  bot = tandbot;
  while (bot) {
    if (bot->uplink == who) {
      unvia(idx, bot);
      bot2 = bot->next;
      rembot(bot->bot);
      bot = bot2;
    } else
      bot = bot->next;
  }
}

void besthub(char *hub)
{
  tand_t *ptr = tandbot;
  struct userrec *u,
   *besthub = NULL;
  char bestlval[20],
    lval[20];

  hub[0] = 0;
  strcpy(bestlval, "z");
  while (ptr) {
    u = get_user_by_handle(userlist, ptr->bot);
    if (u) {
      link_pref_val(u, lval);
      if (strcmp(lval, bestlval) < 0) {
	strcpy(bestlval, lval);
	besthub = u;
      }
    }
    ptr = ptr->next;
  }
  if (besthub)
    strcpy(hub, besthub->handle);
  return;
}

/* return index into dcc list of the bot that connects us to bot <x> */
int nextbot(char *who)
{
  int j;
  tand_t *bot = findbot(who);

  if (!bot)
    return -1;

  for (j = 0; j < dcc_total; j++)
    if (bot->via && !strcasecmp(bot->via->bot, dcc[j].nick) && (dcc[j].type == &DCC_BOT))
      return j;
  return -1;			/* we're not connected to 'via' */
}

/* return name of the bot that is directly connected to bot X */
char *lastbot(char *who)
{
  tand_t *bot = findbot(who);

  if (!bot)
    return "*";
  else if (bot->uplink == (tand_t *) 1)
    return botnetnick;
  else
    return bot->uplink->bot;
}

/* modern version of 'whom' (use local data) */
void answer_local_whom(int idx, int chan)
{
  char c,
    idle[40],
    spaces2[33] = "                               ";
  int i,
    len,
    len2;

  Context;
  if (chan == (-1))
    dprintf(idx, STR("Users across the botnet (+: Party line, *: Local channel)\n"));
  else if (chan > 0) {
    dprintf(idx, STR("Users on channel %s%d:\n"), (chan < 100000) ? "" : "*", chan % 100000);
  }
  spaces[HANDLEN - 9] = 0;
  dprintf(idx, STR(" Nick     %s   Bot      %s  Host\n"), spaces, spaces);
  dprintf(idx, STR("----------%s   ---------%s  --------------------\n"), spaces, spaces);
  spaces[HANDLEN - 9] = ' ';
  for (i = 0; i < dcc_total; i++)
    if (dcc[i].type == &DCC_CHAT) {
      if ((chan == (-1)) || ((chan >= 0) && (dcc[i].u.chat->channel == chan))) {
	c = geticon(i);
	if (c == '-')
	  c = ' ';
	if (now - dcc[i].timeval > 300) {
	  unsigned long days,
	    hrs,
	    mins;

	  days = (now - dcc[i].timeval) / 86400;
	  hrs = ((now - dcc[i].timeval) - (days * 86400)) / 3600;
	  mins = ((now - dcc[i].timeval) - (hrs * 3600)) / 60;
	  if (days > 0)
	    sprintf(idle, STR(" [idle %lud%luh]"), days, hrs);
	  else if (hrs > 0)
	    sprintf(idle, STR(" [idle %luh%lum]"), hrs, mins);
	  else
	    sprintf(idle, STR(" [idle %lum]"), mins);
	} else
	  idle[0] = 0;
	spaces[len = HANDLEN - strlen(dcc[i].nick)] = 0;
	spaces2[len2 = HANDLEN - strlen(botnetnick)] = 0;
	dprintf(idx, STR("%c%s%s %c %s%s  %s%s\n"), c, dcc[i].nick, spaces,
		(dcc[i].u.chat->channel == 0) && (chan == (-1)) ? '+' : (dcc[i].u.chat->channel > 100000) && (chan == (-1)) ? '*' : ' ', botnetnick, spaces2, dcc[i].host, idle);
	spaces[len] = ' ';
	spaces2[len2] = ' ';
	if (dcc[i].u.chat->away != NULL)
	  dprintf(idx, STR("   AWAY: %s\n"), dcc[i].u.chat->away);
      }
    }
  for (i = 0; i < parties; i++) {
    if ((chan == (-1)) || ((chan >= 0) && (party[i].chan == chan))) {
      c = party[i].flag;
      if (c == '-')
	c = ' ';
      if (party[i].timer == 0L)
	strcpy(idle, STR(" [idle?]"));
      else if (now - party[i].timer > 300) {
	unsigned long days,
	  hrs,
	  mins;

	days = (now - party[i].timer) / 86400;
	hrs = ((now - party[i].timer) - (days * 86400)) / 3600;
	mins = ((now - party[i].timer) - (hrs * 3600)) / 60;
	if (days > 0)
	  sprintf(idle, STR(" [idle %lud%luh]"), days, hrs);
	else if (hrs > 0)
	  sprintf(idle, STR(" [idle %luh%lum]"), hrs, mins);
	else
	  sprintf(idle, STR(" [idle %lum]"), mins);
      } else
	idle[0] = 0;
      spaces[len = HANDLEN - strlen(party[i].nick)] = 0;
      spaces2[len2 = HANDLEN - strlen(party[i].bot)] = 0;
      dprintf(idx, STR("%c%s%s %c %s%s  %s%s\n"), c, party[i].nick, spaces, (party[i].chan == 0) && (chan == (-1)) ? '+' : ' ', party[i].bot, spaces2, party[i].from, idle);
      spaces[len] = ' ';
      spaces2[len2] = ' ';
      if (party[i].status & PLSTAT_AWAY)
	dprintf(idx, STR("   AWAY: %s\n"), safe_str(party[i].away));
    }
  }
}

/* show z a list of all bots connected */
void tell_bots(int idx)
{
  char s[512];
  int i;
  tand_t *bot;

  if (!tands) {
    dprintf(idx, STR("No bots linked\n"));
    return;
  }
  strcpy(s, botnetnick);
  i = strlen(botnetnick);

  for (bot = tandbot; bot; bot = bot->next) {
    if (i > (500 - HANDLEN)) {
      dprintf(idx, STR("Bots: %s\n"), s);
      s[0] = 0;
      i = 0;
    }
    if (i) {
      s[i++] = ',';
      s[i++] = ' ';
    }
    strcpy(s + i, bot->bot);
    i += strlen(bot->bot);
  }
  if (s[0])
    dprintf(idx, STR("Bots: %s\n"), s);
  dprintf(idx, STR("(Total: %d)\n"), tands + 1);
}

/* show a simpleton bot tree */
void tell_bottree(int idx, int showver)
{
  char s[161];
  tand_t *last[20],
   *this,
   *bot,
   *bot2 = NULL;
  int lev = 0,
    more = 1,
    mark[20],
    ok,
    cnt,
    i,
    imark;
  char work[1024];
  int tothops = 0;

  if (tands == 0) {
    dprintf(idx, STR("No bots linked\n"));
    return;
  }
  s[0] = 0;
  i = 0;

  for (bot = tandbot; bot; bot = bot->next)
    if (!bot->uplink) {
      if (i) {
	s[i++] = ',';
	s[i++] = ' ';
      }
      strcpy(s + i, bot->bot);
      i += strlen(bot->bot);
    }
  if (s[0])
    dprintf(idx, STR("(No trace info for: %s)\n"), s);
  if (showver)
    dprintf(idx, STR("%s (%d.%d.%d.%d)\n"), botnetnick, egg_numver / 1000000, egg_numver % 1000000 / 10000, egg_numver % 10000 / 100, egg_numver % 100);
  else
    dprintf(idx, STR("%s\n"), botnetnick);
  this = (tand_t *) 1;
  work[0] = 0;
  while (more) {
    if (lev == 20) {
      dprintf(idx, STR("\nTree too complex!\n"));
      return;
    }
    cnt = 0;
    tothops += lev;
    for (bot = tandbot; bot; bot = bot->next)
      if (bot->uplink == this)
	cnt++;
    if (cnt) {
      imark = 0;
      for (i = 0; i < lev; i++) {
	if (mark[i])
	  strcpy(work + imark, STR("  |  "));
	else
	  strcpy(work + imark, STR("     "));
	imark += 5;
      }
      if (cnt > 1)
	strcpy(work + imark, STR("  |-"));
      else
	strcpy(work + imark, STR("  `-"));
      s[0] = 0;
      bot = tandbot;
      while (!s[0]) {
	if (bot->uplink == this) {
	  if (bot->ver) {
	    i = sprintf(s, STR("%c%s"), bot->share, bot->bot);
	    if (showver)
	      sprintf(s + i, STR(" (%d.%d.%d.%d)"), bot->ver / 1000000, bot->ver % 1000000 / 10000, bot->ver % 10000 / 100, bot->ver % 100);
	  } else {
	    sprintf(s, STR("-%s"), bot->bot);
	  }
	} else
	  bot = bot->next;
      }
      dprintf(idx, STR("%s%s\n"), work, s);
      if (cnt > 1)
	mark[lev] = 1;
      else
	mark[lev] = 0;
      work[0] = 0;
      last[lev] = this;
      this = bot;
      lev++;
      more = 1;
    } else {
      while (cnt == 0) {
	/* no subtrees from here */
	if (lev == 0) {
	  dprintf(idx, STR("(( tree error ))\n"));
	  return;
	}
	ok = 0;
	for (bot = tandbot; bot; bot = bot->next) {
	  if (bot->uplink == last[lev - 1]) {
	    if (this == bot)
	      ok = 1;
	    else if (ok) {
	      cnt++;
	      if (cnt == 1) {
		bot2 = bot;
		if (bot->ver) {
		  i = sprintf(s, STR("%c%s"), bot->share, bot->bot);
		  if (showver)
		    sprintf(s + i, STR(" (%d.%d.%d.%d)"), bot->ver / 1000000, bot->ver % 1000000 / 10000, bot->ver % 10000 / 100, bot->ver % 100);
		} else {
		  sprintf(s, STR("-%s"), bot->bot);
		}
	      }
	    }
	  }
	}
	if (cnt) {
	  imark = 0;
	  for (i = 1; i < lev; i++) {
	    if (mark[i - 1])
	      strcpy(work + imark, STR("  |  "));
	    else
	      strcpy(work + imark, STR("     "));
	    imark += 5;
	  }
	  more = 1;
	  if (cnt > 1)
	    dprintf(idx, STR("%s  |-%s\n"), work, s);
	  else
	    dprintf(idx, STR("%s  `-%s\n"), work, s);
	  this = bot2;
	  work[0] = 0;
	  if (cnt > 1)
	    mark[lev - 1] = 1;
	  else
	    mark[lev - 1] = 0;
	} else {
	  /* this was the last child */
	  lev--;
	  if (lev == 0) {
	    more = 0;
	    cnt = 999;
	  } else {
	    more = 1;
	    this = last[lev];
	  }
	}
      }
    }
  }
  /* hop information: (9d) */
  dprintf(idx, STR("Average hops: %3.1f, total bots: %d\n"), ((float) tothops) / ((float) tands), tands + 1);
}

/* dump list of links to a new bot */
void dump_links(int z)
{
  register int i,
    l;
  char x[1024];
  tand_t *bot;

  Context;
  for (bot = tandbot; bot; bot = bot->next) {
    char *p;

    if (bot->uplink == (tand_t *) 1)
      p = botnetnick;
    else
      p = bot->uplink->bot;
    l = simple_sprintf(x, STR("n %s %s %c%D\n"), bot->bot, p, bot->share, bot->ver);
    tputs(dcc[z].sock, x, l);
  }
  Context;
  /* dump party line members */
  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type == &DCC_CHAT) {
      if ((dcc[i].u.chat->channel >= 0) && (dcc[i].u.chat->channel < 100000)) {
	l = simple_sprintf(x, STR("j !%s %s %D %c%D %s\n"), botnetnick, dcc[i].nick, dcc[i].u.chat->channel, geticon(i), dcc[i].sock, dcc[i].host);
	tputs(dcc[z].sock, x, l);
	l = simple_sprintf(x, STR("i %s %D %D %s\n"), botnetnick, dcc[i].sock, now - dcc[i].timeval, dcc[i].u.chat->away ? dcc[i].u.chat->away : "");
	tputs(dcc[z].sock, x, l);
      }
    }
  }
  Context;
  for (i = 0; i < parties; i++) {
    l = simple_sprintf(x, STR("j %s %s %D %c%D %s\n"), party[i].bot, party[i].nick, party[i].chan, party[i].flag, party[i].sock, party[i].from);
    tputs(dcc[z].sock, x, l);
    if ((party[i].status & PLSTAT_AWAY) || (party[i].timer != 0)) {
      l = simple_sprintf(x, STR("i %s %D %D %s\n"), party[i].bot, party[i].sock, now - party[i].timer, party[i].away ? party[i].away : "");
      tputs(dcc[z].sock, x, l);
    }
  }
  Context;
}

int in_chain(char *who)
{
  if (findbot(who))
    return 1;
  if (!strcasecmp(who, botnetnick))
    return 1;
  return 0;
}

/* break link with a tandembot */
int botunlink(int idx, char *nick, char *reason)
{
  register int i;

  Context;
  if (nick[0] == '*')
    dprintf(idx, STR("Unlinking all bots...\n"));
  for (i = 0; i < dcc_total; i++) {
    if ((nick[0] == '*') || !strcasecmp(dcc[i].nick, nick)) {
      if (dcc[i].type == &DCC_FORK_BOT) {
	if (idx >= 0)
	  dprintf(idx, STR("Killed link attempt to: %s -> %s.\n"), dcc[i].nick, dcc[i].host);
	log(LCAT_BOT, STR("Killed link attempt to: %s -> %s:%d"), dcc[i].nick, dcc[i].host, dcc[i].port);
	killsock(dcc[i].sock);
	lostdcc(i);
	if (nick[0] != '*')
	  return 1;
      } else if (dcc[i].type == &DCC_BOT_NEW) {
	if (idx >= 0)
	  dprintf(idx, STR("No longer trying to link: %s.\n"), dcc[i].nick);
	log(LCAT_BOT, STR("%s %s @ %s:%d"), STR("Stopped trying to link"), dcc[i].nick, dcc[i].host, dcc[i].port);
	killsock(dcc[i].sock);
	lostdcc(i);
	if (nick[0] != '*')
	  return 1;
	else
	  i--;
      } else if (dcc[i].type == &DCC_BOT) {
	char s[1024];

	Context;

	if (idx >= 0)
	  dprintf(idx, STR("Breaking link with %s.\n"), dcc[i].nick);
	else if ((idx == -3) && (b_status(i) & STAT_SHARE) && !share_unlinks)
	  return -1;
	dprintf(i, STR("bye\n"));
	if (reason && reason[0]) {
	  simple_sprintf(s, STR("Unlinked from %s (%s)"), dcc[i].nick, reason);
	} else {
	  simple_sprintf(s, STR("Unlinked from %s"), dcc[i].nick);
	}
	log(LCAT_BOT, STR("%s\n"), s);
	botnet_send_unlinked(i, dcc[i].nick, s);
	killsock(dcc[i].sock);
	lostdcc(i);
	if (nick[0] != '*')
	  return 1;
	else
	  i--;
      }
    }
  }
  Context;
  if ((idx >= 0) && (nick[0] != '*'))
    dprintf(idx, STR("Not connected to that bot\n"));
  if (nick[0] == '*') {
    dprintf(idx, STR("Smooshing bot tables...\n"));
    while (tandbot)
      rembot(tandbot->bot);
    while (parties) {
      parties--;
      /* Assert? */
      if (party[i].chan >= 0)
	check_tcl_chpt(party[i].bot, party[i].nick, party[i].sock, party[i].chan);
    }
  }
  return 0;
}

/* link to another bot */
int botlink(char *linker, int idx, char *nick)
{
  struct bot_addr *bi;
  struct userrec *u;
  register int i;

  Context;
  u = get_user_by_handle(userlist, nick);
  if (!u || !(u->flags & USER_BOT)) {
    if (idx >= 0)
      dprintf(idx, STR("%s is not a known bot\n"), nick);
  } else if (!strcasecmp(nick, botnetnick)) {
    if (idx >= 0)
      dprintf(idx, STR("Link to myself?\n"));
  } else if (in_chain(nick) && (idx != -3)) {
    if (idx >= 0)
      dprintf(idx, STR("That bot is already linked\n"));
  } else {
    for (i = 0; i < dcc_total; i++)
      if ((dcc[i].user == u) && ((dcc[i].type == &DCC_FORK_BOT) || (dcc[i].type == &DCC_BOT_NEW))) {
	if (idx >= 0)
	  dprintf(idx, STR("Already linking to that bot\n"));
	return 0;
      }
    /* address to connect to is in 'info' */
    bi = (struct bot_addr *) get_user(&USERENTRY_BOTADDR, u);
    if (!bi || !strlen(bi->address) || !bi->telnet_port) {
      if (idx >= 0) {
	dprintf(idx, STR("Invalid telnet address:port for '%s'.\n"), nick);
	dprintf(idx, STR("Use: .chaddr %s <address>:<port#>[/<relay-port#>]\n"));
      }
    } else if (dcc_total == max_dcc) {
      if (idx >= 0)
	dprintf(idx, STR("Sorry\n"));
    } else {
      Context;
      correct_handle(nick);
      i = new_dcc(&DCC_FORK_BOT, sizeof(struct bot_info));

      dcc[i].port = bi->telnet_port;
      strcpy(dcc[i].nick, nick);
      strcpy(dcc[i].host, bi->address);
      dcc[i].timeval = now;
      strcpy(dcc[i].u.bot->linker, linker);
      strcpy(dcc[i].u.bot->version, STR("(primitive bot)"));
      if (idx > -2)
	log(LCAT_BOT, STR("Linking to %s at %s:%d ..."), nick, bi->address, bi->telnet_port);
      dcc[i].u.bot->numver = idx;
      dcc[i].timeval = now;
      dcc[i].u.bot->port = dcc[i].port;	/* remember where i started */
      dcc[i].sock = getsock(SOCK_STRONGCONN);
      dcc[i].user = u;
      if (open_telnet_raw(dcc[i].sock, bi->address, dcc[i].port) < 0)
	failed_link(i);
      return 1;
    }
  }
  return 0;
}

void failed_tandem_relay(int idx)
{
  int uidx = (-1),
    i;

  Context;
  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type == &DCC_PRE_RELAY) && (dcc[i].u.relay->sock == dcc[idx].sock))
      uidx = i;
  if (uidx < 0) {
    log(LCAT_ERROR, STR("Can't find user for relay!  %d -> %d"), dcc[idx].sock, dcc[idx].u.relay->sock);
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }
  if (dcc[idx].port >= dcc[idx].u.relay->port + 3) {
    struct chat_info *ci = dcc[uidx].u.relay->chat;

    dprintf(uidx, STR("Could not link to %s.\n"), dcc[idx].nick);
    nfree(dcc[uidx].u.relay);
    dcc[uidx].u.chat = ci;
    dcc[uidx].type = &DCC_CHAT;
    dcc[uidx].status = dcc[uidx].u.relay->old_status;
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }
  killsock(dcc[idx].sock);
  dcc[idx].sock = getsock(SOCK_STRONGCONN);
  dcc[uidx].u.relay->sock = dcc[idx].sock;
  dcc[idx].port++;
  dcc[idx].timeval = now;
  if (open_telnet_raw(dcc[idx].sock, dcc[idx].host, dcc[idx].port) < 0)
    failed_tandem_relay(idx);
}

/* relay to another tandembot */
void tandem_relay(int idx, char *nick, register int i)
{
  struct chat_info *ci;
  struct userrec *u;
  struct bot_addr *bi;

  Context;
  u = get_user_by_handle(userlist, nick);
  if (!u || !(u->flags & USER_BOT)) {
    dprintf(idx, STR("%s is not a known bot\n"), nick);
    return;
  }
  if (!strcasecmp(nick, botnetnick)) {
    dprintf(idx, STR("Relay to myself?\n"));
    return;
  }
  /* address to connect to is in 'info' */
  bi = (struct bot_addr *) get_user(&USERENTRY_BOTADDR, u);
  if (!bi) {
    dprintf(idx, STR("Invalid telnet address:port for '%s'.\n"), nick);
    return;
  }
  if (dcc_total == max_dcc) {
    dprintf(idx, STR("Sorry\n"));
    return;
  }
  i = new_dcc(&DCC_FORK_RELAY, sizeof(struct relay_info));
  dcc[i].u.relay->chat = get_data_ptr(sizeof(struct chat_info));

  dcc[i].port = bi->relay_port;
  dcc[i].u.relay->port = dcc[i].port;
  dcc[i].addr = 0L;
  strcpy(dcc[i].nick, nick);
  dcc[i].user = u;
  strcpy(dcc[i].host, bi->address);
  dcc[i].u.relay->chat->away = NULL;
  dcc[i].u.relay->old_status = dcc[i].status;
  dcc[i].status = 0;
  dcc[i].timeval = now;
  dcc[i].u.relay->chat->msgs_per_sec = 0;
  dcc[i].u.relay->chat->buffer = NULL;
  dcc[i].u.relay->chat->max_line = 0;
  dcc[i].u.relay->chat->line_count = 0;
  dcc[i].u.relay->chat->current_lines = 0;
  dprintf(idx, STR("Connecting to %s @ %s:%d ...\n"), nick, bi->address, bi->relay_port);
  dprintf(idx, STR("(Type *BYE* on a line by itself to abort.)\n"));
  ci = dcc[idx].u.chat;
  dcc[idx].u.relay = get_data_ptr(sizeof(struct relay_info));

  dcc[idx].u.relay->chat = ci;
  dcc[idx].type = &DCC_PRE_RELAY;
  dcc[i].sock = getsock(SOCK_STRONGCONN);
  dcc[idx].u.relay->sock = dcc[i].sock;
  dcc[i].u.relay->sock = dcc[idx].sock;
  dcc[i].timeval = now;
  if (open_telnet_raw(dcc[i].sock, dcc[i].host, dcc[i].port) < 0)
    failed_tandem_relay(i);
}

/* input from user before connect is ready */
void pre_relay(int idx, char *buf, register int i)
{
  register int tidx = (-1);

  Context;
  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type == &DCC_FORK_RELAY) && (dcc[i].u.relay->sock == dcc[idx].sock))
      tidx = i;
  if (tidx < 0) {
    log(LCAT_ERROR, STR("Can't find user for relay  %d -> %d"), dcc[i].sock, dcc[i].u.relay->sock);
    killsock(dcc[i].sock);
    lostdcc(i);
    return;
  }
  Context;
  if (!strcasecmp(buf, STR("*bye*"))) {
    /* disconnect */
    struct chat_info *ci = dcc[idx].u.relay->chat;

    dprintf(idx, STR("Aborting relay attempt to %s.\n"), dcc[tidx].nick);
    dprintf(idx, STR("You are now back on %s.\n\n"), botnetnick);
    log(LCAT_ERROR, STR("Relay aborted: %s -> %s"), dcc[idx].nick, dcc[tidx].nick);
    nfree(dcc[idx].u.relay);
    dcc[idx].u.chat = ci;
    dcc[idx].status = dcc[idx].u.relay->old_status;
    dcc[idx].type = &DCC_CHAT;
    killsock(dcc[tidx].sock);
    lostdcc(tidx);
    return;
  }
  Context;
}

/* user disconnected before her relay had finished connecting */
void failed_pre_relay(int idx)
{
  register int tidx = (-1),
    i;

  Context;
  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type == &DCC_FORK_RELAY) && (dcc[i].u.relay->sock == dcc[idx].sock))
      tidx = i;
  if (tidx < 0) {
    log(LCAT_ERROR, STR("Can't find user for relay!  %d -> %d"), dcc[i].sock, dcc[i].u.relay->sock);
    killsock(dcc[i].sock);
    lostdcc(i);
    return;
  }
  log(LCAT_ERROR, STR("Lost DCC connection to [%s]%s/%d"), dcc[idx].nick, dcc[idx].host, dcc[idx].port);
  log(LCAT_ERROR, STR("(Dropping relay attempt to %s)"), dcc[tidx].nick);
  if ((dcc[tidx].sock != STDOUT) || backgrd) {
    if (idx > tidx) {
      int t = tidx;

      tidx = idx;
      idx = t;
    }
    killsock(dcc[tidx].sock);
    lostdcc(tidx);
  } else {
    fatal(STR("Lost my terminal??!?!?!"), 0);
  }
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

void cont_tandem_relay(int idx, char *buf, register int i)
{
  register int uidx = (-1);
  struct relay_info *ri;

  Context;
  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type == &DCC_PRE_RELAY) && (dcc[i].u.relay->sock == dcc[idx].sock))
      uidx = i;
  if (uidx < 0) {
    log(LCAT_ERROR, STR("Can't find user for relay!  %d -> %d"), dcc[i].sock, dcc[i].u.relay->sock);
    killsock(dcc[i].sock);
    lostdcc(i);
    return;
  }
  dcc[idx].type = &DCC_RELAY;
  dcc[idx].u.relay->sock = dcc[uidx].sock;
  dcc[uidx].u.relay->sock = dcc[idx].sock;
  dprintf(uidx, STR("Success!\n\nNOW CONNECTED TO RELAY BOT %s ...\n"), dcc[idx].nick);
  dprintf(uidx, STR("(You can type *BYE* to prematurely close the connection.)\n\n"));
  log(LCAT_CONN, STR("Relay link: %s -> %s"), dcc[uidx].nick, dcc[idx].nick);
  ri = dcc[uidx].u.relay;	/* YEAH */
  dcc[uidx].type = &DCC_CHAT;
  dcc[uidx].u.chat = ri->chat;
  if (dcc[uidx].u.chat->channel >= 0) {
    chanout_but(-1, dcc[uidx].u.chat->channel, STR("*** %s left the party line\n"), dcc[uidx].nick);
    if (dcc[uidx].u.chat->channel < 100000)
      botnet_send_part_idx(uidx, NULL);
    check_tcl_chpt(botnetnick, dcc[uidx].nick, dcc[uidx].sock, dcc[uidx].u.chat->channel);
  }
  check_tcl_chof(dcc[uidx].nick, dcc[uidx].sock);
  dcc[uidx].type = &DCC_RELAYING;
  dcc[uidx].u.relay = ri;
}

void eof_dcc_relay(int idx)
{
  register int j;
  struct chat_info *ci;

  for (j = 0; dcc[j].sock != dcc[idx].u.relay->sock; j++);
  /* in case echo was off, turn it back on: */
  if (dcc[j].status & STAT_TELNET)
    dprintf(j, STR("\377\374\001\r\n"));
  log(LCAT_CONN, STR("Ended relay link: %s -> %s"), dcc[j].nick, dcc[idx].nick);
  dprintf(j, STR("\n\n*** RELAY CONNECTION DROPPED.\nYou are now back on %s\n"), botnetnick);
  ci = dcc[j].u.relay->chat;
  nfree(dcc[j].u.relay);
  dcc[j].u.chat = ci;
  dcc[j].type = &DCC_CHAT;
  if (dcc[j].u.chat->channel >= 0) {
    chanout_but(-1, dcc[j].u.chat->channel, STR("*** %s rejoined the party line.\n"), dcc[j].nick);
    Context;
    if (dcc[j].u.chat->channel < 100000)
      botnet_send_join_idx(j, -1);
  }
  dcc[j].status = dcc[j].u.relay->old_status;
  check_tcl_chon(dcc[j].nick, dcc[j].sock);
  check_tcl_chjn(botnetnick, dcc[j].nick, dcc[j].u.chat->channel, geticon(j), dcc[j].sock, dcc[j].host);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

void eof_dcc_relaying(int idx)
{
  register int j,
    x = dcc[idx].u.relay->sock;

  log(LCAT_CONN, STR("Lost DCC connection to [%s]%s/%d"), dcc[idx].nick, dcc[idx].host, dcc[idx].port);
  killsock(dcc[idx].sock);
  lostdcc(idx);
  for (j = 0; (dcc[j].sock != x) || (dcc[j].type == &DCC_FORK_RELAY); j++);
  log(LCAT_CONN, STR("(Dropping relay link to %s)"), dcc[j].nick);
  killsock(dcc[j].sock);
  lostdcc(j);			/* drop connection to the bot */
}

void dcc_relay(int idx, char *buf, int j)
{
  unsigned char *p = (unsigned char *) buf;
  int mark;

  for (j = 0; (dcc[j].sock != dcc[idx].u.relay->sock) || (dcc[j].type != &DCC_RELAYING); j++);
  /* if redirecting to a non-telnet user, swallow telnet codes */
  if (!(dcc[j].status & STAT_TELNET)) {
    while (*p != 0) {
      while ((*p != 255) && (*p != 0))
	p++;			/* search for IAC */
      if (*p == 255) {
	mark = 2;
	if (!*(p + 1))
	  mark = 1;		/* bogus */
	if ((*(p + 1) >= 251) || (*(p + 1) <= 254)) {
	  mark = 3;
	  if (!*(p + 2))
	    mark = 2;		/* bogus */
	}
	strcpy((char *) p, (char *) (p + mark));
      }
    }
    if (!buf[0])
      dprintf(-dcc[idx].u.relay->sock, STR(" \n"));
    else
      dprintf(-dcc[idx].u.relay->sock, STR("%s\n"), buf);
    return;
  }
  /* telnet user */
  if (!buf[0])
    dprintf(-dcc[idx].u.relay->sock, STR(" \r\n"));
  else
    dprintf(-dcc[idx].u.relay->sock, STR("%s\r\n"), buf);
}

void dcc_relaying(int idx, char *buf, int j)
{
  struct chat_info *ci;

  if (strcasecmp(buf, STR("*bye*"))) {
    dprintf(-dcc[idx].u.relay->sock, STR("%s\n"), buf);
    return;
  }
  for (j = 0; (dcc[j].sock != dcc[idx].u.relay->sock) || (dcc[j].type != &DCC_RELAY); j++);
  /* in case echo was off, turn it back on: */
  if (dcc[idx].status & STAT_TELNET)
    dprintf(idx, STR("\377\374\001\r\n"));
  dprintf(idx, STR("\n(Breaking connection to %s.)\n"), dcc[j].nick);
  dprintf(idx, STR("You are now back on %s.\n\n"), botnetnick);
  log(LCAT_CONN, STR("Relay broken: %s -> %s"), dcc[idx].nick, dcc[j].nick);
  if (dcc[idx].u.relay->chat->channel >= 0) {
    chanout_but(-1, dcc[idx].u.relay->chat->channel, STR("*** %s joined the party line.\n"), dcc[idx].nick);
    Context;
    if (dcc[idx].u.relay->chat->channel < 100000)
      botnet_send_join_idx(idx, -1);
  }
  ci = dcc[idx].u.relay->chat;
  nfree(dcc[idx].u.relay);
  dcc[idx].u.chat = ci;
  dcc[idx].type = &DCC_CHAT;
  dcc[idx].status = dcc[idx].u.relay->old_status;
  check_tcl_chon(dcc[idx].nick, dcc[idx].sock);
  if (dcc[idx].u.chat->channel >= 0)
    check_tcl_chjn(botnetnick, dcc[idx].nick, dcc[idx].u.chat->channel, geticon(idx), dcc[idx].sock, dcc[idx].host);
  killsock(dcc[j].sock);
  lostdcc(j);
}

void display_relay(int i, char *other)
{
  sprintf(other, STR("rela  -> sock %d"), dcc[i].u.relay->sock);
}

void display_relaying(int i, char *other)
{
  sprintf(other, STR(">rly  -> sock %d"), dcc[i].u.relay->sock);
}

void display_tandem_relay(int i, char *other)
{
  strcpy(other, STR("other  rela"));
}

void display_pre_relay(int i, char *other)
{
  strcpy(other, STR("other  >rly"));
}

int expmem_relay(void *x)
{
  register struct relay_info *p = (struct relay_info *) x;
  int tot = sizeof(struct relay_info);

  if (p->chat)
    tot += DCC_CHAT.expmem(p->chat);
  return tot;
}

void kill_relay(int idx, void *x)
{
  register struct relay_info *p = (struct relay_info *) x;

  if (p->chat)
    DCC_CHAT.kill(idx, p->chat);
  nfree(p);
}

struct dcc_table DCC_RELAY = {
  "RELAY",
  0,				/* flags */
  eof_dcc_relay,
  dcc_relay,
  0,
  0,
  display_relay,
  expmem_relay,
  kill_relay,
  0
};

void out_relay(int idx, char *buf, void *x)
{
  register struct relay_info *p = (struct relay_info *) x;

  if (p && p->chat)
    DCC_CHAT.output(idx, buf, p->chat);
  else
    tputs(dcc[idx].sock, buf, strlen(buf));
}

struct dcc_table DCC_RELAYING = {
  "RELAYING",
  0,				/* flags */
  eof_dcc_relaying,
  dcc_relaying,
  0,
  0,
  display_relaying,
  expmem_relay,
  kill_relay,
  out_relay
};

struct dcc_table DCC_FORK_RELAY = {
  "FORK_RELAY",
  0,				/* flags */
  failed_tandem_relay,
  cont_tandem_relay,
  &connect_timeout,
  failed_tandem_relay,
  display_tandem_relay,
  expmem_relay,
  kill_relay,
  0
};

struct dcc_table DCC_PRE_RELAY = {
  "PRE_RELAY",
  0,				/* flags */
  failed_pre_relay,
  pre_relay,
  0,
  0,
  display_pre_relay,
  expmem_relay,
  kill_relay,
  0
};

/* once every 30 sec, send 'ping' to each bot -- no exceptions */
void check_botnet_pings()
{
  int i;

  Context;
  for (i = 0; i < dcc_total; i++)
    if (dcc[i].type == &DCC_BOT)
      if (dcc[i].status & STAT_PINGED) {
	char s[1024];
	log(LCAT_BOT, STR("Ping timeout: %s"), dcc[i].nick);
	simple_sprintf(s, STR("Ping timeout: %s"), dcc[i].nick);
	botnet_send_unlinked(i, dcc[i].nick, s);
	killsock(dcc[i].sock);
	lostdcc(i);
      }
  Context;
  for (i = 0; i < dcc_total; i++)
    if (dcc[i].type == &DCC_BOT) {
      botnet_send_ping(i);
      dcc[i].status |= STAT_PINGED;
    }
  Context;
  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type == &DCC_BOT) && (dcc[i].status & STAT_LEAF)) {
      tand_t *bot,
       *via = findbot(dcc[i].nick);

      for (bot = tandbot; bot; bot = bot->next) {
	if ((via == bot->via) && (bot != via)) {
	  /* not leaflike behavior */
	  if (dcc[i].status & STAT_WARNED) {
	    char s[1024];

	    log(LCAT_BOT, STR("Disconnected from: %s (unleaflike behavior)."), dcc[i].nick);
	    log(LCAT_WARNING, STR("Disconnected from: %s (unleaflike behavior)."), dcc[i].nick);
	    dprintf(i, STR("bye\n"));
	    simple_sprintf(s, STR("Disconnected from: %s (unleaflike behavior)"), dcc[i].nick);
	    botnet_send_unlinked(i, dcc[i].nick, s);
	    killsock(dcc[i].sock);
	    lostdcc(i);
	  } else {
	    botnet_send_reject(i, botnetnick, NULL, bot->bot, NULL, NULL);
	    dcc[i].status |= STAT_WARNED;
	  }
	} else
	  dcc[i].status &= ~STAT_WARNED;
      }
    }
  Context;
#ifdef HUB
#ifdef G_AUTOLOCK
  check_should_lock();
#endif
#ifdef G_BACKUP
  check_should_backup();
#endif
#endif
}

void zapfbot(int idx)
{
  char s[1024];

  simple_sprintf(s, STR("Dropped bot: %s"), dcc[idx].nick);
  
  log(LCAT_BOT, STR("%s.\n"), s);
  botnet_send_unlinked(idx, dcc[idx].nick, s);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

void restart_chons()
{
  int i;

  /* dump party line members */
  Context;
  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type == &DCC_CHAT) {
      check_tcl_chon(dcc[i].nick, dcc[i].sock);
      check_tcl_chjn(botnetnick, dcc[i].nick, dcc[i].u.chat->channel, geticon(i), dcc[i].sock, dcc[i].host);
    }
  }
  for (i = 0; i < parties; i++) {
    check_tcl_chjn(party[i].bot, party[i].nick, party[i].chan, party[i].flag, party[i].sock, party[i].from);
  }
  Context;
}

void lower_bot_linked(int idx)
{
  int i;
  botnet_send_logsettings(idx);
  for (i=0;i<cfg_count;i++)
    botnet_send_cfg(idx, cfg[i]);
  send_channel_sync(dcc[idx].nick, NULL);
}

void higher_bot_linked(int idx)
{

}









