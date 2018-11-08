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
 * botnet.c -- handles:
 *   keeping track of which bot's connected where in the chain
 *   dumping a list of bots or a bot tree to a user
 *   channel name associations on the party line
 *   rejecting a bot
 *   linking, unlinking, and relaying to another bot
 *   pinging the bots periodically and checking leaf status
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common.h"
#include "botnet.h"
#include "color.h"
#include "chanprog.h"
#include "net.h"
#include "socket.h"
#include "adns.h"
#include "match.h"
#include "users.h"
#include "misc.h"
#include "userrec.h"
#include "main.h"
#include "dccutil.h"
#include "dcc.h"
#include "botmsg.h"
#include "tandem.h"
#include "core_binds.h"
#include <bdlib/src/String.h>
#include <bdlib/src/Array.h>
#include <algorithm>
#include <unordered_map>

tand_t			*tandbot = NULL;		/* Keep track of tandem bots on the
							   botnet */
party_t			*party = NULL;			/* Keep track of people on the botnet */
int			tands = 0;			/* Number of bots on the botnet */

static int 		maxparty = 50;			/* Maximum space for party line members
							   currently */
static int		parties = 0;			/* Number of people on the botnet */
static int		share_unlinks = 1;		/* Allow remote unlinks of my
							   sharebots? */

void init_party()
{
  party = (party_t *) calloc(1, maxparty * sizeof(party_t));
}

tand_t *findbot(const char *who)
{
  for (tand_t* ptr = tandbot; ptr; ptr = ptr->next)
    if (!strcasecmp(ptr->bot, who))
      return ptr;
  return NULL;
}

extern void counter_clear(const char* botnick);

/* Add a tandem bot to our chain list
 */
void addbot(char *who, char *from, char *next, char flag, int vlocalhub, time_t vbuildts, char *vcommit, char *vversion, int fflags)
{
  tand_t **ptr = &tandbot, *ptr2 = NULL;

  while (*ptr) {
    if (!strcasecmp((*ptr)->bot, who))
      putlog(LOG_BOTS, "*", "!!! Duplicate botnet bot entry!!");
    ptr = &((*ptr)->next);
  }
  ptr2 = (tand_t *) calloc(1, sizeof(tand_t));
  strlcpy(ptr2->bot, who, HANDLEN + 1);
  ptr2->bot[HANDLEN] = 0;
  ptr2->share = flag;
  ptr2->localhub = vlocalhub;
  ptr2->buildts = vbuildts;
  strlcpy(ptr2->commit, vcommit, sizeof(ptr2->commit));
  if (vversion && vversion[0])
    strlcpy(ptr2->version, vversion, 121);
  ptr2->next = *ptr;
  *ptr = ptr2;
  /* May be via itself */
  ptr2->via = findbot(from);
  /* Look up the bot in internal users */
  ptr2->hub = is_hub(who);
  /* Cache user record */
  ptr2->u = userlist ? get_user_by_handle(userlist, who) : NULL;
  ptr2->fflags = fflags;
  if (fflags != -1) {
    char buf[15];

    simple_snprintf(buf, sizeof(buf), "%d", ptr2->fflags);
    set_user(&USERENTRY_FFLAGS, ptr2->u ? ptr2->u : get_user_by_handle(userlist, who), buf);
  }
  if (!strcasecmp(next, conf.bot->nick))
    ptr2->uplink = (tand_t *) 1;
  else
    ptr2->uplink = findbot(next);
  tands++;

  counter_clear(who);
}

#ifdef G_BACKUP
void check_should_backup()
{
  struct chanset_t *chan = NULL;

  for (chan = chanset; chan; chan = chan->next) {
    if (chan->channel.backup_time && (chan->channel.backup_time < now) && !channel_backup(chan)) {
      do_chanset(NULL, chan, STR("+backup"), DO_LOCAL | DO_NET);
      chan->channel.backup_time = 0;
    }
  }
}
#endif /* G_BACKUP */

void updatebot(int idx, char *who, char share, int vlocalhub, time_t vbuildts, char *vcommit, char *vversion, int fflags)
{
  tand_t *ptr = findbot(who);

  if (ptr) {
    if (share)
      ptr->share = share;
    if (vlocalhub)
      ptr->localhub = vlocalhub;
    if (vbuildts)
      ptr->buildts = vbuildts;
    if (vcommit)
      strlcpy(ptr->commit, vcommit, sizeof(ptr->commit));
    if (vversion && vversion[0])
      strlcpy(ptr->version, vversion, 121);
    /* -1 = unknown (do not modify) */
    if (fflags != -1) {
      char buf[15];

      ptr->fflags = fflags;
      simple_snprintf(buf, sizeof(buf), "%d", ptr->fflags);
      set_user(&USERENTRY_FFLAGS, ptr->u ? ptr->u : get_user_by_handle(userlist, who), buf);
    }
    /* Assign flags here */
    botnet_send_update(idx, ptr);
  }
}

/* New botnet member
 */
int addparty(char *bot, char *nick, int chan, char flag, int sock, char *from, int *idx)
{
  int i = 0;

  for (i = 0; i < parties; i++) {
    /* Just changing the channel of someone already on? */
    if (!strcasecmp(party[i].bot, bot) && (party[i].sock == sock)) {
      int oldchan = party[i].chan;

      party[i].chan = chan;
      party[i].timer = now;
      if (from[0]) {
	if (flag == ' ')
	  flag = '-';
	party[i].flag = flag;
	if (party[i].from)
	  free(party[i].from);
        party[i].from = strdup(from);
      }
      *idx = i;
      return oldchan;
    }
  }
  /* New member */
  if (parties == maxparty) {
    maxparty += 50;
    party = (party_t *) realloc((void *) party, maxparty * sizeof(party_t));
  }
  strlcpy(party[parties].nick, nick, sizeof(party[parties].nick));
  strlcpy(party[parties].bot, bot, sizeof(party[parties].bot));
  party[parties].chan = chan;
  party[parties].sock = sock;
  party[parties].status = 0;
  party[parties].away = 0;
  party[parties].timer = now;	/* cope. */
  if (from[0]) {
    if (flag == ' ')
      flag = '-';
    party[parties].flag = flag;
    party[parties].from = strdup(from);
  } else {
    party[parties].flag = ' ';
    party[parties].from = strdup("(unknown)");
  }
  *idx = parties;
  parties++;
  return -1;
}

/* Alter status flags for remote party-line user.
 */
void partystat(char *bot, int sock, int add, int rem)
{
  for (int i = 0; i < parties; i++) {
    if ((!strcasecmp(party[i].bot, bot)) &&
	(party[i].sock == sock)) {
      party[i].status |= add;
      party[i].status &= ~rem;
    }
  }
}

/* Other bot is sharing idle info.
 */
void partysetidle(char *bot, int sock, int secs)
{
  for (int i = 0; i < parties; i++) {
    if ((!strcasecmp(party[i].bot, bot)) &&
	(party[i].sock == sock)) {
      party[i].timer = (now - (time_t) secs);
    }
  }
}

/* Return someone's chat channel.
 */
int getparty(const char *bot, int sock)
{
  for (int i = 0; i < parties; i++) {
    if (!strcasecmp(party[i].bot, bot) &&
	(party[i].sock == sock)) {
      return i;
    }
  }
  return -1;
}

/* Un-idle someone
 */
bool partyidle(char *bot, char *nick)
{
  bool ok = 0;

  for (int i = 0; i < parties; i++) {
    if ((!strcasecmp(party[i].bot, bot)) && (!strcasecmp(party[i].nick, nick))) {
      party[i].timer = now;
      ok = 1;
    }
  }
  return ok;
}

/* Change someone's nick
 */
int partynick(char *bot, int sock, char *nick)
{
  char work[HANDLEN + 1] = "";

  for (int i = 0; i < parties; i++) {
    if (!strcasecmp(party[i].bot, bot) && (party[i].sock == sock)) {
      strlcpy(work, party[i].nick, sizeof(work));
      strlcpy(party[i].nick, nick, sizeof(party[i].nick));
      strlcpy(nick, work, HANDLEN + 1);
      return i;
    }
  }
  return -1;
}

/* Set away message
 */
void partyaway(char *bot, int sock, char *msg)
{
  for (int i = 0; i < parties; i++) {
    if ((!strcasecmp(party[i].bot, bot)) &&
	(party[i].sock == sock)) {
      if (party[i].away)
	free(party[i].away);
      if (msg[0]) {
        party[i].away = strdup(msg);
      } else
	party[i].away = 0;
    }
  }
}

/* Remove a tandem bot from the chain list
 */
void rembot(const char *whoin)
{
  tand_t **ptr = &tandbot, *ptr2 = NULL;
  char *who = strdup(whoin);

  while (*ptr) {
    if (!strcasecmp((*ptr)->bot, who))
      break;
    ptr = &((*ptr)->next);
  }
  if (!*ptr) {
    /* May have just .unlink *'d */
    free(who);
    return;
  }

  struct userrec *u = get_user_by_handle(userlist, (char *) who);

  if (u) {
    noshare++;
    touch_laston(u, "unlinked", now);
    noshare--;
  }

  ptr2 = *ptr;
  *ptr = ptr2->next;
  free(ptr2);
  tands--;

  dupwait_notify(who);
  free(who);
}

void remparty(char *bot, int sock)
{
  for (int i = 0; i < parties; i++)
    if ((!strcasecmp(party[i].bot, bot)) &&
	(party[i].sock == sock)) {
      parties--;
      if (party[i].from)
	free(party[i].from);
      if (party[i].away)
	free(party[i].away);
      if (i < parties) {
	strlcpy(party[i].bot, party[parties].bot, sizeof(party[i].bot));
	strlcpy(party[i].nick, party[parties].nick, sizeof(party[i].nick));
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

/* Cancel every user that was on a certain bot
 */
void rempartybot(char *bot)
{
  for (int i = 0; i < parties; i++)
    if (!strcasecmp(party[i].bot, bot)) {
      remparty(bot, party[i].sock);
      i--;
    }
}

/* Remove every bot linked 'via' bot <x>
 */
void unvia(int idx, tand_t *who)
{
  if (!who)
    return;			/* Safety */

  tand_t *bot = NULL, *bot2 = NULL;

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

/* Return index into dcc list of the bot that connects us to bot <x>
 */
int nextbot(const char *who)
{
  tand_t *bot = findbot(who);

  if (!bot)
    return -1;

  for (int j = 0; j < dcc_total; j++) {
    if (dcc[j].type && bot->via && (dcc[j].type == &DCC_BOT) && !strcasecmp(bot->via->bot, dcc[j].nick))
      return j;
  }

  return -1;			/* We're not connected to 'via' */
}

/* Return name of the bot that is directly connected to bot X
 */
char *lastbot(const char *who)
{
  tand_t *bot = findbot(who);

  if (!bot)
    return "*";
  else if (bot->uplink == (tand_t *) 1)
    return conf.bot->nick;
  else
    return bot->uplink->bot;
}

/* Modern version of 'whom' (use local data)
 */
void answer_local_whom(int idx, int chan)
{
  char format[81] = "", c = 0, idle[40] = "";
  int i, t, nicklen, botnicklen, total = 0;

  if (chan == (-1))
    dprintf(idx, "%s (+: %s, *: %s)\n", "Users across the botnet", "Party line", "Local channel");
  else if (chan > 0)
    dprintf(idx, "%s %s%d:\n", "Users on channel", (chan < GLOBAL_CHANS) ? "" : "*", chan % GLOBAL_CHANS);

  /* Find longest nick and botnick */
  nicklen = botnicklen = 0;
  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type && dcc[i].simul == -1 && dcc[i].type == &DCC_CHAT) {
      if ((chan == (-1)) || ((chan >= 0) && (dcc[i].u.chat->channel == chan))) {
        t = strlen(dcc[i].nick); if(t > nicklen) nicklen = t;
        t = strlen(conf.bot->nick); if(t > botnicklen) botnicklen = t;
      }
    }
  }
  for (i = 0; i < parties; i++) {
    if ((chan == (-1)) || ((chan >= 0) && (party[i].chan == chan))) {
      t = strlen(party[i].nick); if(t > nicklen) nicklen = t;
      t = strlen(party[i].bot); if(t > botnicklen) botnicklen = t;
    }
  }
  if(nicklen < 9) nicklen = 9;
  if(botnicklen < 9) botnicklen = 9;

  if (conf.bot->hub) {
    simple_snprintf(format, sizeof format, "%%-%us   %%-%us  %%s\n", nicklen, botnicklen);
    dprintf(idx, format, " Nick", 	" Bot",      " Host");
    dprintf(idx, format, "----------",	"---------", "--------------------");
    simple_snprintf(format, sizeof format, "%%c%%-%us %%c %%-%us  %%s%%s\n", nicklen, botnicklen);
  } else {
    simple_snprintf(format, sizeof format, "%%-%us\n", nicklen);
    dprintf(idx, format, " Nick");
    dprintf(idx, format, "----------");
    simple_snprintf(format, sizeof format, "%%c%%-%us %%c %%s\n", nicklen);
  }
  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type && dcc[i].simul == -1 && dcc[i].type == &DCC_CHAT) {
      if ((chan == (-1)) || ((chan >= 0) && (dcc[i].u.chat->channel == chan))) {
	c = geticon(i);
	if (c == '-')
	  c = ' ';
	if (now - dcc[i].timeval > 300) {
	  unsigned long mydays, hrs, mins;

	  mydays = (now - dcc[i].timeval) / 86400;
	  hrs = ((now - dcc[i].timeval) - (mydays * 86400)) / 3600;
	  mins = ((now - dcc[i].timeval) - (hrs * 3600)) / 60;
	  if (mydays > 0)
	    simple_snprintf(idle, sizeof(idle), " [idle %lud%luh]", mydays, hrs);
	  else if (hrs > 0)
	    simple_snprintf(idle, sizeof(idle), " [idle %luh%lum]", hrs, mins);
	  else
	    simple_snprintf(idle, sizeof(idle), " [idle %lum]", mins);
	} else
	  idle[0] = 0;

        total++;
        if (conf.bot->hub)
  	  dprintf(idx, format, c, dcc[i].nick, 
		 (dcc[i].u.chat->channel == 0) && (chan == (-1)) ? '+' :
		 (dcc[i].u.chat->channel > GLOBAL_CHANS) &&
		 (chan == (-1)) ? '*' : ' ', conf.bot->nick, dcc[i].host, idle);
        else
          dprintf(idx, format, c, dcc[i].nick, 
	         (dcc[i].u.chat->channel == 0) && (chan == (-1)) ? '+' :
		 (dcc[i].u.chat->channel > GLOBAL_CHANS) &&
		 (chan == (-1)) ? '*' : ' ', idle);

	if (dcc[i].u.chat->away != NULL)
	  dprintf(idx, "   AWAY: %s\n", dcc[i].u.chat->away);
        if (dcc[i].u.chat->su_nick && (dcc[idx].user->flags & USER_ADMIN)) 
          dprintf(idx, "   SU FROM: %s\n", dcc[i].u.chat->su_nick);
      }
    }
  }
  for (i = 0; i < parties; i++) {
    if ((chan == (-1)) || ((chan >= 0) && (party[i].chan == chan))) {
      c = party[i].flag;
      if (c == '-')
	c = ' ';
      if (party[i].timer == 0L)
	strlcpy(idle, " [idle?]", sizeof(idle));
      else if (now - party[i].timer > 300) {
	unsigned long mydays, hrs, mins;

	mydays = (now - party[i].timer) / 86400;
	hrs = ((now - party[i].timer) - (mydays * 86400)) / 3600;
	mins = ((now - party[i].timer) - (hrs * 3600)) / 60;
	if (mydays > 0)
	  simple_snprintf(idle, sizeof(idle), " [idle %lud%luh]", mydays, hrs);
	else if (hrs > 0)
	  simple_snprintf(idle, sizeof(idle), " [idle %luh%lum]", hrs, mins);
	else
	  simple_snprintf(idle, sizeof(idle), " [idle %lum]", mins);
      } else
	idle[0] = 0;
      total++;

      if (conf.bot->hub) 
        dprintf(idx, format, c, party[i].nick, (party[i].chan == 0) && (chan == (-1)) ? '+' : ' ', 
                                party[i].bot, party[i].from, idle);
      else
        dprintf(idx, format, c, party[i].nick, (party[i].chan == 0) && (chan == (-1)) ? '+' : ' ', idle);

      if (party[i].status & PLSTAT_AWAY)
	dprintf(idx, "   AWAY: %s\n", party[i].away ? party[i].away : "");
    }
  }
  dprintf(idx, "Total users: %d\n", total);
}

bool sortNodes(const bd::String nodeA, const bd::String nodeB) {
  const bd::String unknown("(unknown)");
  if (nodeA == unknown) {
    return true;
  } else if (nodeB == unknown) {
    return false;
  }
  // Reverse the domains
  const bd::Array<bd::String> partsA(nodeA.split("."));
  const bd::Array<bd::String> partsB(nodeB.split("."));
  bd::Array<bd::String> reversedPartsA, reversedPartsB;
  bd::String reversedNodeA, reversedNodeB;

  if (partsA.length()) {
    for (size_t i = partsA.length() - 1; i > 0; --i) {
      reversedPartsA << partsA[i - 1];
    }
  }
  if (partsB.length()) {
    for (size_t i = partsB.length() - 1; i > 0; --i) {
      reversedPartsB << partsB[i - 1];
    }
  }
  reversedNodeA = reversedPartsA.join(".");
  reversedNodeB = reversedPartsB.join(".");
  return reversedNodeA < reversedNodeB;
}

/* Show z a list of all bots connected
 */
void
tell_bots(int idx, int up, const char *nodename)
{
  size_t total = 0, maxNodeNameLength = 0;;
  bd::Array<bd::String> nodes;
  std::unordered_map<bd::String, bd::Array<bd::String> > nodeBots;
  bd::Array<bd::String> bots;
  bd::String group;

  if (nodename && nodename[0] == '%') {
    group = nodename + 1;
  }

  // Gather a list of nodes and bots per node, as well as per domain
  for (struct userrec* u = userlist; u; u = u->next) {
    if (u->bot) {
      // If looking for groups, exclude hubs
      if (group.length() && bot_hublevel(u) != 999) {
        continue;
      }
      ++total;
      bd::String botnick(u->handle);
      const bd::Array<bd::String> botgroups((bd::String(var_get_bot_data(u, "groups", true))).split(","));

      // Include this bot?
      const bool group_match = group.length() && botgroups.find(group) != botgroups.npos;
      const char *userNode = (const char*) get_user(&USERENTRY_NODENAME, u);
      const bd::String node(userNode ? userNode : "(unknown)");
      const bool node_match = ((nodename && node.length() && wild_match(nodename, node.c_str())) || !nodename);
      const bool bot_found = u == conf.bot->u || findbot(u->handle);
      const bool up_down_match = (nodename || (!nodename && ((up && bot_found) || (!up && !bot_found))));
      if (group_match || (group.length() == 0 && node_match && up_down_match)) {
        if (nodes.find(node) == nodes.npos) {
          nodes << node;
          if (node.length() > maxNodeNameLength) {
            maxNodeNameLength = node.length();
          }
        }
        if ((group.length() || nodename) && !bot_found) {
          botnick = '*' + botnick;
        }
        nodeBots[node] << botnick;
        bots << botnick;
      }
    }
  }

  if (group.length() == 0 && !nodename) {
    dumplots(idx, nodename ? "Matching: " : (up ? "Up: " : "Down: "), static_cast<bd::String>(bots.join(" ")).c_str());
  } else {
    // Sort by nodes
    std::sort(nodes.begin(), nodes.end(), sortNodes);
    for (size_t i = 0; i < nodes.length(); ++i) {
      const bd::String node(nodes[i]);
      const bd::Array<bd::String> botsInNode(nodeBots[node]);
      dumplots(idx, bd::String::printf("%*s: ", int(maxNodeNameLength), node.c_str()).c_str(), static_cast<bd::String>(botsInNode.join(" ")).c_str());
    }
  }

  if (nodename || group.length()) {
    dprintf(idx, "(Total Matching: %zu/%zu)\n", bots.length(), total);
  } else {
    dprintf(idx, "(Total %s: %zu/%zu)\n", up ? "up" : "down", bots.length(), total);
  }
}

/* Show a simpleton bot tree
 */
void tell_bottree(int idx)
{
  if (tands == 0) {
    dprintf(idx, "%s\n", "No bots linked.");
    return;
  }

  char *color_str = NULL;
  tand_t *last[20] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
  tand_t *thisbot = NULL, *bot = NULL, *bot2 = NULL;
  int lev = 0, more = 1, mark[20], ok, cnt, i = 0, tothops = 0;
  struct userrec *botu = NULL;
  bd::String s, work;

  for (bot = tandbot; bot; bot = bot->next)
    if (!bot->uplink) {
      if (i)
        s += ", ";
      s += bot->bot;
      if (!i)
        i = 1;
    }

  if (bot_hublevel(conf.bot->u) < 999)
    color_str = (char *) YELLOW(idx);
  else if (conf.bot->localhub)
    color_str = (char *) RED(idx);
  else
    color_str = (char *) NULL;

  if (s.length())
    dprintf(idx, "(%s %s)\n", "No trace info for:", s.c_str());
  dprintf(idx, "%s%s%s (%s)\n", color_str ? color_str : "",
                                    conf.bot->nick,
                                    color_str ? COLOR_END(idx) : "",
                                    egg_version);

  thisbot = (tand_t *) 1;
  while (more) {
    if (lev == 20) {
      dprintf(idx, "\n%s\n", "Tree too complex!");
      return;
    }
    cnt = 0;
    tothops += lev;
    for (bot = tandbot; bot; bot = bot->next)
      if (bot->uplink == thisbot)
	cnt++;
    if (cnt) {
      for (i = 0; i < lev; i++) {
	if (mark[i])
          work += "  |  ";
        else
          work += "     ";
      }
      if (cnt > 1)
        work += "  |-";
      else
        work += "  `-";
      s.clear();
      bot = tandbot;
      while (bot && !s) {
	if (bot->uplink == thisbot) {
          botu = get_user_by_handle(userlist, bot->bot);
       
          if (botu && bot_hublevel(botu) < 999)
            color_str = (char *) YELLOW(idx);
          else if (botu && bot->localhub)
            color_str = (char *) RED(idx);
          else
            color_str = (char *) NULL;

          s = bd::String::printf("%c%s%s%s (%s)", bot->share ? bot->share : '-', color_str ? color_str : "",
                                                bot->bot,
                                                color_str ? COLOR_END(idx) : "",
                                                bot->version);
	} else
	  bot = bot->next;
      }
      dprintf(idx, "%s%s\n", work.c_str(), s.c_str());
      if (cnt > 1)
	mark[lev] = 1;
      else
	mark[lev] = 0;
      work.clear();
      last[lev] = thisbot;
      thisbot = bot;
      lev++;
      more = 1;
    } else {
      while (cnt == 0) {
	/* No subtrees from here */
	if (lev == 0) {
	  dprintf(idx, "(( tree error ))\n");
	  return;
	}
	ok = 0;
	for (bot = tandbot; bot; bot = bot->next) {
	  if (bot->uplink == last[lev - 1]) {
	    if (thisbot == bot)
	      ok = 1;
	    else if (ok) {
	      cnt++;
	      if (cnt == 1) {
                botu = get_user_by_handle(userlist, bot->bot);
       
                if (botu && bot_hublevel(botu) < 999)
                  color_str = (char *) YELLOW(idx);
                else if (botu && bot->localhub)
                  color_str = (char *) RED(idx);
                else
                  color_str = (char *) NULL;

		bot2 = bot;
                s = bd::String::printf("%c%s%s%s (%s)", bot->share ? bot->share : '-', color_str ? color_str : "",
                                                      bot->bot,
                                                      color_str ? COLOR_END(idx) : "",
                                                      bot->version);
	      }
	    }
	  }
	}
	if (cnt) {
	  for (i = 1; i < lev; i++) {
	    if (mark[i - 1])
              work += "  |  ";
            else
              work += "     ";
	  }
	  more = 1;
	  if (cnt > 1)
	    dprintf(idx, "%s  |-%s\n", work.c_str(), s.c_str());
	  else
	    dprintf(idx, "%s  `-%s\n", work.c_str(), s.c_str());
	  thisbot = bot2;
          work.clear();
	  if (cnt > 1)
	    mark[lev - 1] = 1;
	  else
	    mark[lev - 1] = 0;
	} else {
	  /* This was the last child */
	  lev--;
	  if (lev == 0) {
	    more = 0;
	    cnt = 999;
	  } else {
	    more = 1;
	    thisbot = last[lev];
	  }
	}
      }
    }
  }
  /* Hop information: (9d) */
  dprintf(idx, "Average hops: %3.1f, total bots: %d\n", ((float) tothops) / ((float) tands), tands + 1);
}

/* Dump list of links to a new bot
 */
void dump_links(int z)
{
  int i;
  size_t l;
  char x[1024] = "";

  if (conf.bot->hub || conf.bot->localhub) {
    tand_t *bot = NULL;
    char *p = NULL;

    for (bot = tandbot; bot; bot = bot->next) {
      if (bot->uplink == (tand_t *) 1)
        p = conf.bot->nick;
      else
        p = bot->uplink->bot;

      l = simple_snprintf(x, sizeof(x), "n %s %s %cD0gc %d %d %s %s\n", bot->bot, p, bot->share, bot->localhub,
                                                        (int) bot->buildts, bot->commit, bot->version);
      tputs(dcc[z].sock, x, l);
    }
  }

  /* Dump party line members */
  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type && dcc[i].type == &DCC_CHAT && dcc[i].simul == -1) {
      if ((dcc[i].u.chat->channel >= 0) && (dcc[i].u.chat->channel < GLOBAL_CHANS)) {
        l = simple_snprintf2(x, sizeof(x), "j !%s %s %D %c%D %s\n", conf.bot->nick, dcc[i].nick, 
                               dcc[i].u.chat->channel, geticon(i), dcc[i].sock, dcc[i].host);
	tputs(dcc[z].sock, x, l);
        l = simple_snprintf2(x, sizeof(x), "i %s %D %D %s\n", conf.bot->nick, 
                              dcc[i].sock, now - dcc[i].timeval, 
                              dcc[i].u.chat->away ? dcc[i].u.chat->away : "");
	tputs(dcc[z].sock, x, l);
      }
    }
  }
  for (i = 0; i < parties; i++) {
    l = simple_snprintf2(x, sizeof(x), "j %s %s %D %c%D %s\n", party[i].bot, party[i].nick, 
                          party[i].chan, party[i].flag, party[i].sock, party[i].from);
    tputs(dcc[z].sock, x, l);
    if ((party[i].status & PLSTAT_AWAY) || (party[i].timer != 0)) {
      l = simple_snprintf2(x, sizeof(x), "i %s %D %D %s\n", party[i].bot, party[i].sock, now - party[i].timer, party[i].away ? party[i].away : "");
      tputs(dcc[z].sock, x, l);
    }
  }
}

int in_chain(const char *who)
{
  if (!strcasecmp(who, conf.bot->nick))
    return 1;
  if (findbot(who))
    return 1;
  return 0;
}

int bots_in_subtree(const tand_t *bot)
{
  if (!bot)
    return 0;

  int nr = 1;
  tand_t *b = NULL;

  for (b = tandbot; b; b = b->next) {
    if (b->uplink == bot) {
      nr += bots_in_subtree(b);
    }
  }
  return nr;
}

int users_in_subtree(const tand_t *bot)
{
  if (!bot)
    return 0;

  int i, nr = 0;
  tand_t *b = NULL;

  for (i = 0; i < parties; i++)
    if (!strcasecmp(party[i].bot, bot->bot))
      nr++;
  for (b = tandbot; b; b = b->next)
    if (b->uplink == bot)
      nr += users_in_subtree(b);
  return nr;
}

/* Break link with a tandembot
 */
int botunlink(int idx, const char *nick, const char *reason)
{
  int i;
  int bots, users;
  tand_t *bot = NULL;

  if (nick[0] == '*')
    dprintf(idx, "%s\n", "Unlinking all bots...");
  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type && ((nick[0] == '*') || !strcasecmp(dcc[i].nick, nick))) {
      if (dcc[i].type == &DCC_FORK_BOT) {
	if (idx >= 0)
	  dprintf(idx, "%s: %s -> %s.\n", "Killed link attempt to",
		  dcc[i].nick, dcc[i].host);
	putlog(LOG_BOTS, "*", "%s: %s -> %s:%d",
	       "Killed link attempt to", dcc[i].nick,
	       dcc[i].host, dcc[i].port);
	killsock(dcc[i].sock);
	lostdcc(i);
	if (nick[0] != '*')
	  return 1;
      } else if (dcc[i].type == &DCC_BOT_NEW) {
	if (idx >= 0)
	  dprintf(idx, "%s %s.\n", "No longer trying to link:",
		  dcc[i].nick);
	putlog(LOG_BOTS, "*", "%s %s @ %s:%d",
	       "Stopped trying to link", dcc[i].nick,
	       dcc[i].host, dcc[i].port);
	killsock(dcc[i].sock);
	lostdcc(i);
	if (nick[0] != '*')
	  return 1;
      } else if (dcc[i].type == &DCC_BOT) {
	char s[1024] = "";

	if (idx >= 0)
	  dprintf(idx, "%s %s.\n", "Breaking link with", dcc[i].nick);
	else if ((idx == -3) && (b_status(i) & STAT_SHARE) && !share_unlinks)
	  return -1;
	bot = findbot(dcc[i].nick);
	bots = bots_in_subtree(bot);
	users = users_in_subtree(bot);
	if (reason && reason[0]) {
	  simple_snprintf(s, sizeof(s), "%s %s (%s) (lost %d bot%s and %d user%s)",
	  		 "Unlinked from:", dcc[i].nick, reason, bots,
			 (bots != 1) ? "s" : "", users, (users != 1) ?
			 "s" : "");
	  dprintf(i, "bye %s\n", reason);
	} else {
	  simple_snprintf(s, sizeof(s), "%s %s (lost %d bot%s and %d user%s)",
	  		 "Unlinked from:", dcc[i].nick, bots, (bots != 1) ?
			 "s" : "", users, (users != 1) ? "s" : "");
	  dprintf(i, "bye No reason\n");
	}
	chatout("*** %s\n", s);
	botnet_send_unlinked(i, dcc[i].nick, s);
	killsock(dcc[i].sock);
	lostdcc(i);
	if (nick[0] != '*')
	  return 1;
      }
    }
  }
  if (idx >= 0 && nick[0] != '*')
    dprintf(idx, "%s\n", "Not connected to that bot.");
  if (nick[0] != '*') {
    bot = findbot(nick);
    if (bot) {
      /* The internal bot list is desynched from the dcc list
         sometimes. While we still search for the bug, provide
         an easy way to clear out those `ghost'-bots.
				       Fabian (2000-08-02)  */
      char *ghost = "BUG!!: Found bot `%s' in internal bot list, but it\n"
		    "   shouldn't have been there! Removing.\n"
		    "   This is a known bug we haven't fixed yet. If this\n"
		    "   bot is the newest eggdrop version available and you\n"
		    "   know a *reliable* way to reproduce the bug, please\n"
		    "   contact us - we need your help!\n";
      if (idx >= 0)
	dprintf(idx, ghost, nick);
      else
	putlog(LOG_MISC, "*", ghost, nick);
      rembot(bot->bot);
      return 1;
    }
  }
  if (nick[0] == '*') {
    dprintf(idx, "%s\n", "Smooshing bot tables and assocs...");
    while (tandbot)
      rembot(tandbot->bot);
    while (parties) {
      parties--;
    }
  }
  return 0;
}

static void botlink_dns_callback(int, void *, const char *,
    const bd::Array<bd::String>&);
static void botlink_real(int);

/* Link to another bot
 */
int botlink(char *linker, int idx, char *nick)
{
  struct userrec *u = get_user_by_handle(userlist, nick);

  if (!u || !u->bot) {
    if (idx >= 0)
      dprintf(idx, "%s %s\n", nick, "is not a known bot.");
  } else if (!strcasecmp(nick, conf.bot->nick)) {
    if (idx >= 0)
      dprintf(idx, "%s\n", "Link to myself?  Oh boy, Freud would have a field day.");
  } else if (in_chain(nick) && (idx != -3)) {
    if (idx >= 0)
      dprintf(idx, "That bot is already connected up.\n");
  } else {
    int i;

    for (i = 0; i < dcc_total; i++) {
      if (dcc[i].type && (dcc[i].user == u) &&
	  ((dcc[i].type == &DCC_FORK_BOT) ||
	   (dcc[i].type == &DCC_BOT_NEW))) {
	if (idx >= 0)
	  dprintf(idx, "Already linking to that bot\n");
	return 0;
      }
    }

    bool unix_domain = 0;

    if (!conf.bot->hub && !conf.bot->localhub && !strcmp(nick, conf.localhub))
      unix_domain = 1;

    /* Address to connect to is in 'info' */
    struct bot_addr *bi = (struct bot_addr *) get_user(&USERENTRY_BOTADDR, u);

    if (!unix_domain && (!bi || !strlen(bi->address) || !bi->telnet_port || (bi->telnet_port <= 0))) {
      if (idx >= 0) {
	dprintf(idx, "Invalid telnet address:port stored for '%s'.\n", nick);
	dprintf(idx, "Use: %schaddr %s <address>:<port#>[/<relay-port#>]\n", (dcc[idx].u.chat->channel >= 0) ? settings.dcc_prefix : "", nick);
      }
    } else if (dcc_total == max_dcc) {
      if (idx >= 0)
	dprintf(idx, "%s\n", "Sorry, too many DCC connections.");
    } else {
      correct_handle(nick);

      char *address = NULL;
      in_port_t port = 0;

      if (unix_domain) {
        address = conf.localhub_socket;
      } else if (bi) {
        address = bi->address;
        port = bi->telnet_port;
      }

      if (idx > -2) {
        if (port)
          putlog(LOG_BOTS, "*", "Linking to %s at %s:%d ...", nick, address, port);
        else
          putlog(LOG_BOTS, "*", "Linking to %s at %s ...", nick, address);
      }

      i = new_dcc(&DCC_DNSWAIT, sizeof(struct dns_info));

      dcc[i].timeval = now;
      dcc[i].port = port;
      dcc[i].user = u;
      strlcpy(dcc[i].nick, nick, sizeof(dcc[i].nick));
      strlcpy(dcc[i].host, address, sizeof(dcc[i].host));
      dcc[i].u.dns->cptr = strdup(linker);
      dcc[i].u.dns->ibuf = idx;
      dcc[i].bot = 1;

      if (unix_domain) {
        botlink_real(i);
      } else {
        int dns_id = egg_dns_lookup(address, 20, botlink_dns_callback, (void *) (long)i);
         /* dns_id
          * -1 means it was cached and the callback already called
          * -2 means it's already being looked up.. try again later .. */
        if (dns_id >= 0)
          dcc[i].dns_id = dns_id;
        else if (dns_id == -2) {
          lostdcc(i);
          return 0;
        }
      }
     
      return 1;
      /* wait for async reply */
    }
  }
  return 0;
}

static void botlink_dns_callback(int id, void *client_data, const char *host,
    const bd::Array<bd::String>& ips)
{
  long data = (long) client_data;
  int i = (int) data;

  Context;

  if (!valid_dns_id(i, id))
    return;

//  if (valid_idx(i)) {
//    idx = dcc[i].u.dns->ibuf;
//  }

  bd::String ip_from_dns;

  if (ips.size()) {
#ifdef USE_IPV6
    /* If IPv6 is available use it, otherwise fall back on IPv4 */
    if (conf.bot->net.v6)
      ip_from_dns = dns_find_ip(ips, AF_INET6);
    if (!ip_from_dns)
#endif /* USE_IPV6 */
    {
      /* Use IPv4 if no ipv6 is available */
      ip_from_dns = dns_find_ip(ips, AF_INET);
    }
  }

  if (!ips.size() || !ip_from_dns) {
    putlog(LOG_BOTS, "*", "Could not link to %s (DNS error).\n", dcc[i].nick);
    lostdcc(i);
    return;
  }

  dcc[i].addr = inet_addr(ip_from_dns.c_str());
  strlcpy(dcc[i].host, ip_from_dns.c_str(), sizeof(dcc[i].host));

  botlink_real(i);
}

static void botlink_real(int i)
{
  int idx = dcc[i].u.dns->ibuf;
  char *linker = strdup(dcc[i].u.dns->cptr);
  free(dcc[i].u.dns->cptr);
  dcc[i].u.dns->cptr = NULL;

  changeover_dcc(i, &DCC_FORK_BOT, sizeof(struct bot_info));
  dcc[i].timeval = now;
  struct bot_info dummy;
  strlcpy(dcc[i].u.bot->version, "(primitive bot)", sizeof(dummy.version));
  strlcpy(dcc[i].u.bot->sysname, "*", 2);
  strlcpy(dcc[i].u.bot->linker, linker, sizeof(dummy.linker));
  dcc[i].u.bot->numver = idx;
  free(linker);

  dcc[i].u.bot->port = dcc[i].port;             /* Remember where i started */
#ifdef USE_IPV6
  int af_type;
  if (dcc[i].port)
    af_type = is_dotted_ip(dcc[i].host);
  else
    af_type = AF_UNIX;
  dcc[i].sock = getsock(SOCK_STRONGCONN, af_type);
#else
  dcc[i].sock = getsock(SOCK_STRONGCONN);
#endif /* USE_IPV6 */

  int open_telnet_return = 0;
  if (dcc[i].sock < 0 || (open_telnet_return = open_telnet_raw(dcc[i].sock, dcc[i].host, dcc[i].port, 0, 1)) < 0) {
    if (open_telnet_return == -1)
      dcc[i].sock = -1;
    failed_link(i);
  }

  /* wait for async reply */
}

static void failed_tandem_relay(int idx)
{
  int uidx = (-1), i;

  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type && (dcc[i].type == &DCC_PRE_RELAY) &&
	(dcc[i].u.relay->sock == dcc[idx].sock))
      uidx = i;
  }
  if (uidx < 0) {
    putlog(LOG_MISC, "*", "Can't find user for relay!  %d -> %d", dcc[idx].sock, dcc[idx].u.relay->sock);
    if (dcc[idx].sock != -1) {
      killsock(dcc[idx].sock);
      dcc[idx].sock = -1;
    }
    lostdcc(idx);
    return;
  }

  struct chat_info *ci = dcc[uidx].u.relay->chat;

  dprintf(uidx, "Could not relay to %s.\n", dcc[idx].nick);
  dcc[uidx].status = dcc[uidx].u.relay->old_status;
  free(dcc[uidx].u.relay);
  dcc[uidx].u.chat = ci;
  dcc[uidx].type = &DCC_CHAT;
  if (dcc[idx].sock != -1) {
    killsock(dcc[idx].sock);
    dcc[idx].sock = -1;
  }
  lostdcc(idx);
  return;
}

static void tandem_relay_dns_callback(int, void *, const char *,
    const bd::Array<bd::String>&);

/* Relay to another tandembot
 */
void tandem_relay(int idx, char *nick, int i)
{
  struct userrec *u = get_user_by_handle(userlist, nick);

  if (!u || !u->bot) {
    dprintf(idx, "%s %s\n", nick, "is not a known bot.");
    return;
  }
  if (!strcasecmp(nick, conf.bot->nick)) {
    dprintf(idx, "%s\n", "Relay to myself?  What on EARTH would be the point?!");
    return;
  }

  /* Address to connect to is in 'info' */
  struct bot_addr *bi = (struct bot_addr *) get_user(&USERENTRY_BOTADDR, u);

  if (!bi || !strlen(bi->address) || !bi->relay_port || (bi->relay_port <= 0)) {
    dprintf(idx, "Invalid telnet address:port stored for '%s'.\n", nick);
    dprintf(idx, "Use: %schaddr %s <address>:<port#>[/<relay-port#>]\n", (dcc[idx].u.chat->channel >= 0) ? settings.dcc_prefix : "", nick);

    return;
  }

  i = new_dcc(&DCC_DNSWAIT, sizeof(struct dns_info));

  if (i < 0) {
    dprintf(idx, "%s\n", "Sorry, too many DCC connections.");
    return;
  }


  dcc[i].port = bi->relay_port;
  dcc[i].addr = 0L;
  strlcpy(dcc[i].nick, nick, sizeof(dcc[i].nick));
  dcc[i].user = u;
  strlcpy(dcc[i].host, bi->address, sizeof(dcc[i].host));
  if (conf.bot->hub) 
    dprintf(idx, "%s %s @ %s:%d ...\n", "Establishing encrypted connection to", nick, bi->address, bi->relay_port);
  dprintf(idx, "(Type *BYE* on a line by itself to abort.)\n");
  dcc[idx].type = &DCC_PRE_RELAY;

  struct chat_info *ci = dcc[idx].u.chat;

  dcc[idx].u.relay = (struct relay_info *) calloc(1, sizeof(struct relay_info));
  dcc[idx].u.relay->chat = ci;
  dcc[idx].u.relay->old_status = dcc[idx].status;
  dcc[idx].u.relay->idx = i;
  dcc[idx].u.relay->sock = -1;

  dcc[i].timeval = now;
  dcc[i].u.dns->ibuf = idx;
  
  int dns_id = egg_dns_lookup(bi->address, 20, tandem_relay_dns_callback, (void *) (long) i);
  if (dns_id >= 0)
    dcc[i].dns_id = dns_id;

  return;
  /* wait for async reply */
}

static void tandem_relay_dns_callback(int id, void *client_data,
    const char *host, const bd::Array<bd::String>& ips)
{
  //64bit hacks
  long data = (long) client_data;
  int i = (int) data, idx = -1;

  Context;

  if (!valid_dns_id(i, id))
    return;

  if (valid_idx(i))
    idx = dcc[i].u.dns->ibuf;
  
  if (idx < 0) {
    putlog(LOG_MISC, "*", "Can't find user for relay!  %d -> %d", i, idx);
    lostdcc(i);
    return;
  }

  bd::String ip_from_dns;

  if (ips.size()) {
#ifdef USE_IPV6
    /* If IPv6 is available use it, otherwise fall back on IPv4 */
    if (conf.bot->net.v6)
      ip_from_dns = dns_find_ip(ips, AF_INET6);
    if (!ip_from_dns)
#endif /* USE_IPV6 */
    {
      /* Use IPv4 if no ipv6 is available */
      ip_from_dns = dns_find_ip(ips, AF_INET);
    }
  }

  if (!ips.size() || !ip_from_dns) {
    struct chat_info *ci = dcc[idx].u.relay->chat;

    dprintf(idx, "Could not relay to %s (DNS error).\n", dcc[i].nick);
    dcc[idx].status = dcc[idx].u.relay->old_status;
    free(dcc[idx].u.relay);
    dcc[idx].u.chat = ci;
    dcc[idx].type = &DCC_CHAT;
    lostdcc(i);
    return;
  }

#ifdef USE_IPV6
  int af = is_dotted_ip(ip_from_dns.c_str());

  dcc[i].sock = getsock(SOCK_STRONGCONN | SOCK_VIRTUAL, af);
#else
  dcc[i].sock = getsock(SOCK_STRONGCONN | SOCK_VIRTUAL);
#endif /* USE_IPV6 */
  if (dcc[i].sock < 0) {
    lostdcc(i);
    dprintf(idx, "No free sockets available.\n");
    return;
  }

  dcc[idx].u.relay->sock = dcc[i].sock;

  changeover_dcc(i, &DCC_FORK_RELAY, sizeof(struct relay_info));

  dcc[i].addr = inet_addr(ip_from_dns.c_str());

  dcc[i].u.relay->chat = (struct chat_info *) calloc(1, sizeof(struct chat_info));
  dcc[i].u.relay->sock = dcc[idx].sock;
  dcc[i].u.relay->port = dcc[i].port;
#ifdef USE_IPV6
  dcc[i].u.relay->af = af;
#endif
  dcc[i].u.relay->chat->away = NULL;
  dcc[i].u.relay->chat->msgs_per_sec = 0;
  dcc[i].u.relay->chat->con_flags = 0;
  dcc[i].u.relay->chat->buffer = NULL;
  dcc[i].u.relay->chat->max_line = 0;
  dcc[i].u.relay->chat->line_count = 0;
  dcc[i].u.relay->chat->current_lines = 0;
  dcc[i].timeval = now;

  if (open_telnet_raw(dcc[i].sock, ip_from_dns.c_str(), dcc[i].port, 0) < 0) {
    dcc[i].sock = -1;
    failed_tandem_relay(i);
  }
}

/* Input from user before connect is ready
 */
static void pre_relay(int idx, char *buf, int len)
{
  int tidx = (-1), i;

  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type && dcc[i].type == &DCC_FORK_RELAY && (dcc[i].u.relay->sock == dcc[idx].sock)) {
      tidx = i;
      break;
    }
  }
  if (tidx < 0) {
    /* Now try to find it among the DNSWAIT sockets instead. */
    for (i = 0; i < dcc_total; i++) {
      if (dcc[i].type && (dcc[i].type == &DCC_DNSWAIT && (dcc[idx].u.relay->idx == i))) {
	tidx = i;
	break;
      }
    }
  }
  if (tidx < 0) {
    putlog(LOG_MISC, "*", "Can't find user for relay!  %d -> %d", dcc[idx].sock, dcc[idx].u.relay->sock);
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }
  if (!strcasecmp(buf, "*bye*")) {
    /* Disconnect */
    struct chat_info *ci = dcc[idx].u.relay->chat;

    dprintf(idx, "Aborting relay attempt to %s.\n", dcc[tidx].nick);
    dprintf(idx, "You are now back on %s.\n\n", conf.bot->nick);
    putlog(LOG_MISC, "*", "Relay aborted: %s -> %s", dcc[idx].nick, dcc[tidx].nick);
    dcc[idx].status = dcc[idx].u.relay->old_status;
    free(dcc[idx].u.relay);
    dcc[idx].u.chat = ci;
    dcc[idx].type = &DCC_CHAT;
    if (dcc[tidx].sock != -1)
      killsock(dcc[tidx].sock);
    lostdcc(tidx);
    return;
  }
}

/* User disconnected before her relay had finished connecting
 */
static void failed_pre_relay(int idx)
{
  int tidx = (-1), i;

  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type && (dcc[i].type == &DCC_FORK_RELAY) &&
	(dcc[i].u.relay->sock == dcc[idx].sock)) {
      tidx = i;
      break;
    }
  }
  if (tidx < 0) {
    /* Now try to find it among the DNSWAIT sockets instead. */
    for (i = 0; i < dcc_total; i++) {
      if (dcc[i].type && (dcc[i].type == &DCC_DNSWAIT && (dcc[idx].u.relay->idx == i))) {
	tidx = i;
	break;
      }
    }
  }
  if (tidx < 0) {
    putlog(LOG_MISC, "*", "Can't find user for relay!  %d -> %d", dcc[idx].sock, dcc[idx].u.relay->sock);
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }
  putlog(LOG_MISC, "*", "%s [%s]%s/%d", "Lost dcc connection to", dcc[idx].nick, dcc[idx].host, dcc[idx].port);
  putlog(LOG_MISC, "*", "(%s %s)", "Dropping relay attempt to", dcc[tidx].nick);
  if ((dcc[tidx].sock != STDOUT) || backgrd) {
    if (idx > tidx) {
      int t = tidx;

      tidx = idx;
      idx = t;
    }
    if (dcc[tidx].sock != -1)
      killsock(dcc[tidx].sock);
    lostdcc(tidx);
  } else {
    fatal("Lost my terminal?!", 0);
  }
  if (dcc[idx].sock != -1)
    killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void cont_tandem_relay(int idx, char *buf, int len)
{
  int uidx = (-1), i;
  struct relay_info *ri = NULL;

  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type && (dcc[i].type == &DCC_PRE_RELAY) && (dcc[i].u.relay->sock == dcc[idx].sock))
      uidx = i;
  }

  if (uidx < 0) {
    putlog(LOG_MISC, "*", "Can't find user for relay!  %d -> %d", dcc[idx].sock, dcc[idx].u.relay->sock);
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }
  dcc[idx].type = &DCC_RELAY;
  dcc[idx].u.relay->sock = dcc[uidx].sock;
  dcc[uidx].u.relay->sock = dcc[idx].sock;
  dprintf(uidx, "%s %s ...\n", "Success!\n\nNOW CONNECTED TO RELAY BOT", dcc[idx].nick);
  dprintf(uidx, "(You can type *BYE* to prematurely close the connection.)\n\n");
  putlog(LOG_MISC, "*", "%s %s -> %s", "Relay link:",
	 dcc[uidx].nick, dcc[idx].nick);

  ri = dcc[uidx].u.relay;
  dcc[uidx].type = &DCC_CHAT;
  dcc[uidx].u.chat = ri->chat;
  if (dcc[uidx].u.chat->channel >= 0) {
    chanout_but(-1, dcc[uidx].u.chat->channel, "*** %s left the party line.\n", dcc[uidx].nick);
    if (dcc[uidx].u.chat->channel < GLOBAL_CHANS)
      botnet_send_part_idx(uidx, NULL);
  }
  check_bind_chof(dcc[uidx].nick, uidx);
  dcc[uidx].type = &DCC_RELAYING;
  dcc[uidx].u.relay = ri;

  dprintf(idx, "+%s\n", dcc[uidx].nick);
}

static void eof_dcc_relay(int idx)
{
  int j;

  for (j = 0; j < dcc_total; j++)
    if (dcc[j].type && dcc[j].sock == dcc[idx].u.relay->sock)
      break;

  if (j == dcc_total || !dcc[j].type) {
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }
  dcc[j].status = dcc[j].u.relay->old_status;
  /* In case echo was off, turn it back on (send IAC WON'T ECHO): */
  if (dcc[j].status & STAT_TELNET)
    dprintf(j, TLN_IAC_C TLN_WONT_C TLN_ECHO_C "\n");
  putlog(LOG_MISC, "*", "%s: %s -> %s", "Ended relay link", dcc[j].nick,
	 dcc[idx].nick);
  dprintf(j, "\n\n*** %s %s\n", "RELAY CONNECTION DROPPED.\nYou are now back on", conf.bot->nick);

  struct chat_info *ci = dcc[j].u.relay->chat;

  free(dcc[j].u.relay);
  dcc[j].u.chat = ci;
  dcc[j].type = &DCC_CHAT;
  if (dcc[j].u.chat->channel >= 0) {
    chanout_but(-1, dcc[j].u.chat->channel, "*** %s %s.\n",
		dcc[j].nick, "rejoined the party line.");
    if (dcc[j].u.chat->channel < GLOBAL_CHANS)
      botnet_send_join_idx(j);
  }
  check_bind_chon(dcc[j].nick, j);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void eof_dcc_relaying(int idx)
{
  int j, x = dcc[idx].u.relay->sock;

  putlog(LOG_MISC, "*", "%s [%s]%s/%d", "Lost dcc connection to", dcc[idx].nick, dcc[idx].host, dcc[idx].port);
  killsock(dcc[idx].sock);
  lostdcc(idx);

  for (j = 0; j < dcc_total; j++)
    if (dcc[j].type && dcc[j].sock == x && dcc[j].type != &DCC_FORK_RELAY)
      break;

  putlog(LOG_MISC, "*", "(%s %s)", "Dropping relay link to", dcc[j].nick);
  killsock(dcc[j].sock);
  lostdcc(j);			/* Drop connection to the bot */
}

static void dcc_relay(int idx, char *buf, int j)
{
  unsigned char *p = (unsigned char *) buf;
  int mark;

  for (j = 0; j < dcc_total; j++)
   if (dcc[j].type && dcc[j].sock == dcc[idx].u.relay->sock && dcc[j].type == &DCC_RELAYING)
     break;

  if (!strncasecmp(buf, STR("neg!"), 4)) {	/* something to parse in enclink.c */
    newsplit(&buf);
    link_parse(idx, buf);
    return;
  } else if (!strncasecmp(buf, STR("neg?"), 4)) {	/* we're connecting to THEM */
    newsplit(&buf);
    link_challenge_to(idx, buf);
    return;
  }

  /* If redirecting to a non-telnet user, swallow telnet codes and
     escape sequences. */
  if (!(dcc[j].status & STAT_TELNET)) {
    while (*p != 0) {
      while (*p && *p != 255 && *p != '\r')
	p++;			/* Search for IAC, escape sequences and CR. */
      if (*p == 255) {
	mark = 2;
	if (!*(p + 1))
	  mark = 1;		/* Bogus */
	if ((*(p + 1) >= 251) || (*(p + 1) <= 254)) {
	  mark = 3;
	  if (!*(p + 2))
	    mark = 2;		/* Bogus */
	}
        memmove((char *) p, (char *) (p + mark), strlen((char *) (p + mark)) + 1);
      } else if (*p == '\r')
        memmove((char *) p, (char *) (p + 1), strlen((char *) p));
    }
    if (!buf[0])
      dprintf(-dcc[idx].u.relay->sock, " \n");
    else
      dprintf(-dcc[idx].u.relay->sock, "%s\n", buf);
    return;
  }
  /* Telnet user */
  if (!buf[0])
    dprintf(-dcc[idx].u.relay->sock, " \r\n");
  else
    dprintf(-dcc[idx].u.relay->sock, "%s\r\n", buf);
}

static void dcc_relaying(int idx, char *buf, int j)
{
  if (strcasecmp(buf, "*bye*")) {
    dprintf(-dcc[idx].u.relay->sock, "%s\n", buf);
    return;
  }
  /* The user want's to abort, so return them to partyline */

  for (j = 0; j < dcc_total; j++)
    if (dcc[j].type && dcc[j].sock == dcc[idx].u.relay->sock && dcc[j].type == &DCC_RELAY)
      break;

  dcc[idx].status = dcc[idx].u.relay->old_status;
  /* In case echo was off, turn it back on (send IAC WON'T ECHO): */
  if (dcc[idx].status & STAT_TELNET)
    dprintf(idx, TLN_IAC_C TLN_WONT_C TLN_ECHO_C "\n");
  dprintf(idx, "\n(%s %s.)\n", "Breaking connection to", dcc[j].nick);
  dprintf(idx, "You are now back on %s.\n\n", conf.bot->nick);
  putlog(LOG_MISC, "*", "%s: %s -> %s", "Relay broken", dcc[idx].nick, dcc[j].nick);
  if (dcc[idx].u.relay->chat->channel >= 0) {
    chanout_but(-1, dcc[idx].u.relay->chat->channel, "*** %s joined the party line.\n", dcc[idx].nick);
    if (dcc[idx].u.relay->chat->channel < GLOBAL_CHANS)
      botnet_send_join_idx(idx);
  }

  struct chat_info *ci = dcc[idx].u.relay->chat;

  free(dcc[idx].u.relay);
  dcc[idx].u.chat = ci;
  dcc[idx].type = &DCC_CHAT;
  check_bind_chon(dcc[idx].nick, idx);
  if (dcc[j].sock != -1)
    killsock(dcc[j].sock);
  lostdcc(j);
}

static void display_relay(int i, char *other, size_t bufsiz)
{
  simple_snprintf(other, bufsiz, "rela  -> sock %d", dcc[i].u.relay->sock);
}

static void display_relaying(int i, char *other, size_t bufsiz)
{
  simple_snprintf(other, bufsiz, ">rly  -> sock %d", dcc[i].u.relay->sock);
}

static void display_tandem_relay(int i, char *other, size_t bufsiz)
{
  strlcpy(other, "other  rela", bufsiz);
}

static void display_pre_relay(int i, char *other, size_t bufsiz)
{
  strlcpy(other, "other  >rly", bufsiz);
}

static void kill_relay(int idx, void *x)
{
  struct relay_info *p = (struct relay_info *) x;

  if (p->chat)
    DCC_CHAT.kill(idx, p->chat);
  free(p);
}

struct dcc_table DCC_RELAY =
{
  "RELAY",
  0,				/* Flags */
  eof_dcc_relay,
  dcc_relay,
  NULL,
  NULL,
  display_relay,
  kill_relay,
  NULL,
  NULL
};

static void out_relay(int idx, char *buf, void *x)
{
  struct relay_info *p = (struct relay_info *) x;

  if (p && p->chat)
    DCC_CHAT.output(idx, buf, p->chat);
  else
    tputs(dcc[idx].sock, buf, strlen(buf));
}

struct dcc_table DCC_RELAYING =
{
  "RELAYING",
  0,				/* Flags */
  eof_dcc_relaying,
  dcc_relaying,
  NULL,
  NULL,
  display_relaying,
  kill_relay,
  out_relay,
  NULL
};

struct dcc_table DCC_FORK_RELAY =
{
  "FORK_RELAY",
  0,				/* Flags */
  failed_tandem_relay,
  cont_tandem_relay,
  &connect_timeout,
  failed_tandem_relay,
  display_tandem_relay,
  kill_relay,
  NULL,
  NULL
};

struct dcc_table DCC_PRE_RELAY =
{
  "PRE_RELAY",
  0,				/* Flags */
  failed_pre_relay,
  pre_relay,
  NULL,
  NULL,
  display_pre_relay,
  kill_relay,
  NULL,
  NULL
};

/* Once a minute, send 'ping' to each bot -- no exceptions
 */
void check_botnet_pings()
{
  int i, bots, users, top_index = 0;
  tand_t *bot = NULL;

  for (i = 0; i < dcc_total; i++) {
   if (dcc[i].type) {
     top_index = i;
    if (dcc[i].type == &DCC_BOT) {
      // Hubs only allow localhubs to link, which CAN link bots now, so this isn't so cut and dry now
#ifdef no
      if (dcc[i].status & STAT_LEAF) {
        tand_t *via = findbot(dcc[i].nick);

        // Check if this leaf has any linked bots
        for (bot = tandbot; bot; bot = bot->next) {
          if ((via == bot->via) && (bot != via)) {
	    /* Not leaflike behavior */
	    if (dcc[i].status & STAT_WARNED) {
	      char s[1024] = "";

	      putlog(LOG_BOTS, "*", "%s %s (%s).", "Disconnected from:", dcc[i].nick, "unleaflike behavior");
              dprintf(i, "bye %s\n", "unleaflike behavior");
	      bot = findbot(dcc[i].nick);
	      bots = bots_in_subtree(bot);
	      users = users_in_subtree(bot);
	      simple_snprintf(s, sizeof(s), "%s %s (%s) (lost %d bot%s and %d user%s)",
	    		   "Disconnected from:", dcc[i].nick, "unleaflike behavior",
			   bots, (bots != 1) ? "s" : "", users, (users != 1) ?
			   "s" : "");
	      chatout("*** %s\n", s);
	      botnet_send_unlinked(i, dcc[i].nick, s);
	      killsock(dcc[i].sock);
	      lostdcc(i);
	    } else {
	      botnet_send_reject(i, conf.bot->nick, NULL, bot->bot, NULL, NULL);
              dcc[i].status |= STAT_WARNED;
            }
	  } else
	    dcc[i].status &= ~STAT_WARNED;
        }
      }
#endif

      if (dcc[i].status & STAT_PINGED) {
        char s[1024] = "";

        putlog(LOG_BOTS, "*", "%s: %s", "Ping timeout", dcc[i].nick);
        bot = findbot(dcc[i].nick);
        bots = bots_in_subtree(bot);
        users = users_in_subtree(bot);
        simple_snprintf(s, sizeof(s), "%s: %s (lost %d bot%s and %d user%s)", "Ping timeout",
  		       dcc[i].nick, bots, (bots != 1) ? "s" : "",
		       users, (users != 1) ? "s" : "");
        chatout("*** %s\n", s);
        botnet_send_unlinked(i, dcc[i].nick, s);
        killsock(dcc[i].sock);
        lostdcc(i);
      } else {
        botnet_send_ping(i);
        dcc[i].status |= STAT_PINGED;
      }
    }
   }
  }

  if (top_index != (dcc_total - 1))
    dcc_total = top_index + 1;
}

void zapfbot(int idx)
{
  char s[1024] = "";
  tand_t *bot = findbot(dcc[idx].nick);
  int bots = bots_in_subtree(bot), users = users_in_subtree(bot);

  simple_snprintf(s, sizeof(s), "%s: %s (lost %d bot%s and %d user%s)", "Dropped bot",
  		 dcc[idx].nick, bots, (bots != 1) ? "s" : "", users,
		 (users != 1) ? "s" : "");
  chatout("*** %s\n", s);
  botnet_send_unlinked(idx, dcc[idx].nick, s);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

/* vim: set sts=2 sw=2 ts=8 et: */
