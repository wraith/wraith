/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2010 Bryan Drewery
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
 * server.c -- part of server.mod
 *   basic irc server support
 *
 */


#include "src/common.h"
#include "src/set.h"
#include "src/botmsg.h"
#include "src/rfc1459.h"
#include "src/settings.h"
#include "src/match.h"
#include "src/binds.h"
#include "src/users.h"
#include "src/userrec.h"
#include "src/main.h"
#include "src/response.h"
#include "src/misc.h"
#include "src/chanprog.h"
#include "src/net.h"
#include "src/auth.h"
#include "src/adns.h"
#include "src/socket.h"
#include "src/egg_timer.h"
#include "src/mod/channels.mod/channels.h"
#include "src/mod/ctcp.mod/ctcp.h"
#include "src/mod/irc.mod/irc.h"
#include <bdlib/src/Stream.h>
#include <bdlib/src/String.h>
#include <bdlib/src/Array.h>
#include "server.h"
#include <stdarg.h>

int default_alines = 5;		/* How many mode lines are assumed will work before throttling */
bool floodless = 0;		/* floodless iline? */
int ctcp_mode;
int serv = -1;		/* sock # of server currently */
int servidx = -1;		/* idx of server */
char newserver[121] = "";	/* new server? */
in_port_t newserverport = 0;		/* new server port? */
char newserverpass[121] = "";	/* new server password? */
static char serverpass[121] = "";
static time_t trying_server;	/* trying to connect to a server right now? */
int curserv = 999;		/* current position in server list: */
in_port_t curservport = 0;
rate_t flood_msg = { 5, 60 };
rate_t flood_ctcp = { 3, 60 };
char botuserhost[UHOSTLEN] = "";	/* bot's user@host (refreshed whenever the bot joins a channel) */
					/* may not be correct user@host BUT it's how the server sees it */
char botuserip[UHOSTLEN] = "";		/* bot's user@host with the ip. */

time_t release_time = 0;
bool keepnick = 1;		/* keep trying to regain my intended
				   nickname? */
static int nick_juped = 0;	/* True if origbotname is juped(RPL437) (dw) (1 = RESV, 2 = NETSPLIT) */
static int jnick_juped = 0;    /* True if jupenick is juped (1 = RESV, 2 = NETSPLIT) */
time_t tried_jupenick = 0;
time_t tried_nick = 0;
bool use_monitor = 0;
static bool waiting_for_awake;	/* set when i unidle myself, cleared when I get the response */
time_t server_online = 0;	/* server connection time */
char botrealname[121] = "A deranged product of evil coders.";	/* realname of bot */
static interval_t server_timeout = 15;	/* server timeout for connecting */
static const interval_t stoned_timeout = 500;
struct server_list *serverlist = NULL;	/* old-style queue, still used by
					   server list */
interval_t cycle_time;			/* cycle time till next server connect */
in_port_t default_port = 6667;		/* default IRC port */
bool trigger_on_ignore;	/* trigger bindings if user is ignored ? */
int answer_ctcp = 1;		/* answer how many stacked ctcp's ? */
static bool resolvserv;		/* in the process of resolving a server host */
static time_t lastpingtime;	/* IRCNet LAGmeter support -- drummer */
static char stackablecmds[511] = "";
static char stackable2cmds[511] = "";
static egg_timeval_t last_time;
time_t connect_bursting = 0;
static int real_msgburst = 0;
static int real_msgrate = 0;
int flood_count = 0;
int burst = 0;
static bool use_flood_count = 0;
static egg_timeval_t flood_time = {0, 0};
static bool use_penalties;
static int use_fastdeq;
size_t nick_len = 9;			/* Maximal nick length allowed on the network. */
char deaf_char = 0;
bool in_deaf = 0;
char callerid_char = 0;
bool in_callerid = 0;
bool have_cprivmsg = 0;
bool have_cnotice = 0;

static bool double_warned = 0;

static void empty_msgq(void);
static void disconnect_server(int, int);
static void calc_penalty(char *, size_t);
static bool fast_deq(int);
static char *splitnicks(char **);
static void msgq_clear(struct msgq_head *qh);
static int stack_limit = 4;
static bool replaying_cache = 0;

/* New bind tables. */
static bind_table_t *BT_raw = NULL, *BT_msg = NULL;
bind_table_t *BT_ctcr = NULL, *BT_ctcp = NULL;
// Ratbox is (5*8):30, ircd-seven is (5*8):20, try to not push th elimits.
#define SERVER_CONNECT_BURST_TIME 18
#define SERVER_CONNECT_BURST_RATE 5 * 7

#include "servmsg.c"

#define MAXPENALTY 10

// If use_flood_count, don't bother with msgrate, otherwise use the user specified msgrate
#define MSGRATE (use_flood_count ? DEQ_RATE : msgrate)

/* Maximum messages to store in each queue. */
static struct msgq_head mq, hq, modeq, aq, cacheq;

static const struct {
  struct msgq_head* const q;
  const int idx;
  const char* name;
  const char pfx;
  const bool double_msg;
  const bool burst;
  const bool connect_burst;
  const size_t maxqmsg;
} qdsc[5] = {
  { &modeq, 	DP_MODE,	"MODE", 	'm',	0,	1, 	1,	300 },
  { &mq, 	DP_SERVER,	"SERVER", 	's',	0,	1,	1,	300 },
  { &hq, 	DP_HELP,	"HELP", 	'h',	0,	0,	0,	300 },
  { &aq, 	DP_PLAY,	"PLAY", 	'p',	1,	1,	0,	10000 },
  { &cacheq, 	DP_CACHE,	"CACHE", 	'c',	0,	0,	0,	1000 },
};
#define Q_MODE 0
#define Q_SERVER 1
#define Q_HELP 2
#define Q_PLAY 3
#define Q_CACHE 4

#include "cmdsserv.c"


/*
 *     Bot server queues
 */
static bool burst_mode_ok(const char *msg, size_t len) {
  bd::String mode(msg, len);
  bd::Array<bd::String> list(mode.split(' '));
  if (list.length() == 2) return 1;
  if (list.length() == 3) {
    if (!strchr(CHANMETA, bd::String(list[1]).at(0))) return 1;
    else if (bd::String(list[2]).at(0) == 'b') return 1;
  }
  return 0;
}

// Hybrid/ratbox allows bursting 5*8 lines on connect until certain commands are sent, for up to 30 seconds
/*
   BAD:
     JOIN 0
     MODE #chan b
     NICK
     PART
     KICK
     CPRIVMSG
     CNOTICE
     WHO 0/mask
     TIME
     TOPIC
     INVITE
     AWAY
     OPER

   OK:
     WHO *
     WHO !
     WHO #Chan
     WHO NICK
*/
static bool burst_ok(const char* msg, size_t len) {
  if (strstr(msg, "JOIN 0") ||
      (strstr(msg, "MODE") && !burst_mode_ok(msg, len)) ||
      strstr(msg, "NICK") ||
      strstr(msg, "PRIVMSG") ||
      strstr(msg, "NOTICE") ||
      strstr(msg, "PART") ||
      strstr(msg, "KICK") ||
      strstr(msg, "INVITE") ||
      strstr(msg, "AWAY")) {
    sdprintf("BURST MODE VIOLATION!!!: %s\n", msg);
    return 0;
  }
  return 1;
}

/* Called periodically to shove out another queued item.
 *
 * 'mode' queue gets priority now.
 *
 * Most servers will allow 'busts' of upto 5 msgs, so let's put something
 * in to support flushing modeq a little faster if possible.
 * Will send upto 4 msgs from modeq, and then send 1 msg every time
 * it will *not* send anything from hq until the 'burst' value drops
 * down to 0 again (allowing a sudden mq flood to sneak through).
 *
 * ratbox:
 * Every msg sent is added to a count, every second this count decreases by 2.
 * Typical max count is 20, then excess flood is triggered.
 * So after 1 bursts:
 * Count = 5
 * Decaying by 1 every second
 */
void deq_msg()
{
  if (serv < 0)
    return;

  if (timeval_diff(&egg_timeval_now, &flood_time) >= 1000) {
    // Increase flood_count by 1 every msg, but decrease by 2 every second, use this to determine an acceptable burst rate
    if (flood_count > 1)
      flood_count -= 2;
    else if (flood_count == 1)
      flood_count = 0;

    flood_time.sec = egg_timeval_now.sec;
    flood_time.usec = egg_timeval_now.usec;
  }

  /* now < last_time tested 'cause clock adjustments could mess it up */
  if (timeval_diff(&egg_timeval_now, &last_time) >= MSGRATE || now < (last_time.sec - 90)) {
    last_time.sec = egg_timeval_now.sec;
    last_time.usec = egg_timeval_now.usec;

    if (burst > 0) {
      if (use_flood_count) {
        if (flood_count < 5)
          burst = 0;
        else if (flood_count < 10)
          burst -= 4;
        else if (flood_count < 13)
          burst -= 3;
        else if (flood_count < 15)
          burst -= 2;
        else
          --burst;
        if (burst < 0)
          burst = 0;
      } else
        --burst;
    }
  } else
    return;

  struct msgq *q = NULL;

  /* Send upto 'set msgburst' msgs to server if the *critical queue* has anything in it;
   * otherwise, dequeue and burst up to 'set msgburst' messages from the `normal' message
   * queue.
   */
  egg_timeval_t last_time_save = { last_time.sec, last_time.usec };
  bool bursted = 0;
  // -1 here to avoid DP_CACHE
  for(size_t nq = 0; nq < (sizeof(qdsc) / sizeof(qdsc[0])) - 1; ++nq) {
    while (qdsc[nq].q->head &&
        // If burstable queue and can burst, or not a burstable queue and not connect bursting
        ((qdsc[nq].burst && (burst < msgburst && (!connect_bursting || (connect_bursting && qdsc[nq].connect_burst)))) || (!qdsc[nq].burst && !connect_bursting)) &&
        ((last_time.sec - now) < MAXPENALTY)) {
#ifdef not_implemented
      if (deq_kick(qdsc[nq].idx)) {
        ++burst;++flood_count;
        break;
      }
#endif
      if (fast_deq(nq))
        break;
      write_to_server(qdsc[nq].q->head->msg, qdsc[nq].q->head->len);
      ++burst;++flood_count;
      if (debug_output)
        putlog(LOG_SRVOUT, "*", "[%c->] %s", qdsc[nq].pfx, qdsc[nq].q->head->msg);
      --(qdsc[nq].q->tot);
      calc_penalty(qdsc[nq].q->head->msg, qdsc[nq].q->head->len);

      // Shift off dequeued message
      q = qdsc[nq].q->head->next;
      free(qdsc[nq].q->head->msg);
      free(qdsc[nq].q->head);
      qdsc[nq].q->head = q;
      if (qdsc[nq].burst)
        bursted = 1;
      else // Help Queue does not burst, push out 1 line then go to next queue.
        break;
    }
    if (!qdsc[nq].q->head)
      qdsc[nq].q->last = NULL;
  }

  // Do this penalty calc here as it's dependant on burst/flood_count
  if (use_flood_count && !connect_bursting) {
    // The penalty includes a length-based penalty from calc_penalty

    last_time.sec -= (MSGRATE / 1000); // Remove normal msgrate
    // Add 150ms for each current burst
    last_time.usec += (150*burst) * 1000;
    // Add some penalty for each flood_count
    last_time.usec += (40*flood_count) * 1000;
    // Cap the penalty at 1800 and depend more on flood_count
    if (timeval_diff(&last_time, &last_time_save) > 1800) {
      last_time.sec = last_time_save.sec;
      last_time.usec = 1800 * 1000;
    }
    // If lagging, raise the penalty up to avoid TCP burst/excess flood
    if (server_lag > 5)
      last_time.sec += 2;
#ifdef DEBUG
    if (timeval_diff(&last_time, &last_time_save))
      sdprintf("PENALTY (%d): %lims", flood_count, timeval_diff(&last_time, &last_time_save));
#endif
  }
#ifdef DEBUG
  else if (connect_bursting && bursted)
    sdprintf("BURSTING!!!!!\n");
#endif

}

static void calc_penalty(char * msg, size_t len)
{
  if (connect_bursting)
    return;

  char *cmd = NULL, *par1 = NULL, *par2 = NULL, *par3 = NULL;
  register int penalty, i, ii;

  cmd = newsplit(&msg);
  if (msg)
    i = strlen(msg);
  else
    i = strlen(cmd);
  if (!use_penalties) {
    // Add some penalty for large messages
    if (use_flood_count)
      last_time.usec += long(((double)i / 300.0) * (1000*1000));
    else
      last_time.usec += long(((double)i / 120.0) * (1000*1000));
    return;
  }

  last_time.sec -= (MSGRATE / 1000); // Remove normal msgrate

  penalty = (1 + i / 100);
  if (!strcasecmp(cmd, "KICK")) {
    par1 = newsplit(&msg); /* channel */
    par2 = newsplit(&msg); /* victim(s) */
    par3 = splitnicks(&par2);
    penalty++;
    while (strlen(par3) > 0) {
      par3 = splitnicks(&par2);
      penalty++;
    }
    ii = penalty;
    par3 = splitnicks(&par1);
    while (strlen(par1) > 0) {
      par3 = splitnicks(&par1);
      penalty += ii;
    }
  } else if (!strcasecmp(cmd, "MODE")) {
    i = 0;
    par1 = newsplit(&msg); /* channel */
    par2 = newsplit(&msg); /* mode(s) */
    if (!strlen(par2))
      i++;
    while (strlen(par2) > 0) {
      if (strchr("ntimps", par2[0]))
        i += 3;
      else if (!strchr("+-", par2[0]))
        i += 1;
      par2++;
    }
    while (strlen(msg) > 0) {
      newsplit(&msg);
      i += 2;
    }
    ii = 0;
    while (strlen(par1) > 0) {
      splitnicks(&par1);
      ii++;
    }
    penalty += (ii * i);
  } else if (!strcasecmp(cmd, "TOPIC")) {
    penalty++;
    par1 = newsplit(&msg); /* channel */
    par2 = newsplit(&msg); /* topic */
    if (strlen(par2) > 0) {  /* topic manipulation => 2 penalty points */
      penalty += 2;
      par3 = splitnicks(&par1);
      while (strlen(par1) > 0) {
        par3 = splitnicks(&par1);
        penalty += 2;
      }
    }
  } else if (!strcasecmp(cmd, "PRIVMSG") ||
	     !strcasecmp(cmd, "NOTICE")) {
    par1 = newsplit(&msg); /* channel(s)/nick(s) */
    /* Add one sec penalty for each recipient */
    while (strlen(par1) > 0) {
      splitnicks(&par1);
      penalty++;
    }
  } else if (!strcasecmp(cmd, "WHO")) {
    par1 = newsplit(&msg); /* masks */
    par2 = par1;
    while (strlen(par1) > 0) {
      par2 = splitnicks(&par1);
      if (strlen(par2) > 4)   /* long WHO-masks receive less penalty */
        penalty += 3;
      else
        penalty += 5;
    }
  } else if (!strcasecmp(cmd, "AWAY")) {
    if (strlen(msg) > 0)
      penalty += 2;
    else
      penalty += 1;
  } else if (!strcasecmp(cmd, "INVITE")) {
    /* Successful invite receives 2 or 3 penalty points. Let's go
     * with the maximum.
     */
    penalty += 3;
  } else if (!strcasecmp(cmd, "JOIN")) {
    penalty += 2;
  } else if (!strcasecmp(cmd, "PART")) {
    penalty += 4;
  } else if (!strcasecmp(cmd, "VERSION")) {
    penalty += 2;
  } else if (!strcasecmp(cmd, "TIME")) {
    penalty += 2;
  } else if (!strcasecmp(cmd, "TRACE")) {
    penalty += 2;
  } else if (!strcasecmp(cmd, "NICK")) {
    penalty += 3;
  } else if (!strcasecmp(cmd, "ISON")) {
    penalty += 1;
  } else if (!strcasecmp(cmd, "WHOIS")) {
    penalty += 2;
  } else if (!strcasecmp(cmd, "DNS")) {
    penalty += 2;
  } else
    penalty++; /* just add standard-penalty */
  /* Shouldn't happen, but you never know... */
  if (penalty > 99)
    penalty = 99;
  if (penalty < 2) {
    putlog(LOG_SRVOUT, "*", "Penalty < 2sec, that's impossible!");
    penalty = 2;
  }
  if (debug_output && penalty != 0)
    putlog(LOG_SRVOUT, "*", "Adding penalty: %i", penalty);
  last_time.sec += penalty;
}

char *splitnicks(char **rest)
{
  if (!rest)
    return *rest = "";

  register char *o = *rest, *r = NULL;

  while (*o == ' ')
    o++;
  r = o;
  while (*o && *o != ',')
    o++;
  if (*o)
    *o++ = 0;
  *rest = o;
  return r;
}

void replay_cache(int idx, bd::Stream* stream) {
  if (!cacheq.head) return;

  struct msgq *r = NULL;
  char *p_ptr = NULL, *p = NULL;

  replaying_cache = 1;

  for (r = cacheq.head; r; r = r->next) {
    if (stream) {
      *stream << bd::String::printf(STR("+serv_cache %s\n"), r->msg);
    } else {
      //Create temporary buffer since server_activity may squash the buffer
      p_ptr = p = strdup(r->msg);
      server_activity(idx, p, r->len);
      free(p_ptr);
    }
  }

  replaying_cache = 0;
}

static bool fast_deq(int which)
{
  if (!use_fastdeq)
    return 0;

  struct msgq_head *h = qdsc[which].q;
  struct msgq *m = NULL, *nm = NULL;
  char msgstr[511] = "", nextmsgstr[511] = "", tosend[511] = "", stackable[511] = "",
       *msg = NULL, *nextmsg = NULL, *cmd = NULL, *nextcmd = NULL, *to = NULL, *nextto = NULL, *stckbl = NULL;
  int cmd_count = 0;
  char stack_delim = ',';
  size_t len;
  bool found = 0, doit = 0;
  bd::String victims;

  m = h->head;
  strlcpy(msgstr, m->msg, sizeof msgstr);
  msg = msgstr;
  cmd = newsplit(&msg);
  if (use_fastdeq > 1) {
    strlcpy(stackable, stackablecmds, sizeof stackable);
    stckbl = stackable;
    while (strlen(stckbl) > 0)
      if (!strcasecmp(newsplit(&stckbl), cmd)) {
        found = 1;
        break;
      }
    /* If use_fastdeq is 2, only commands in the list should be stacked. */
    if (use_fastdeq == 2 && !found)
      return 0;
    /* If use_fastdeq is 3, only commands that are _not_ in the list
     * should be stacked.
     */
    if (use_fastdeq == 3 && found)
      return 0;
    /* we check for the stacking method (default=1) */
    strlcpy(stackable, stackable2cmds, sizeof stackable);
    stckbl = stackable;
    while (strlen(stckbl) > 0)
      if (!strcasecmp(newsplit(&stckbl), cmd)) {
        stack_delim = ' ';
        break;
      }    
  }
  to = newsplit(&msg);
  len = strlen(to);
  victims = to;
  while (m) {
    nm = m->next;
    if (!nm)
      break;
    strlcpy(nextmsgstr, nm->msg, sizeof nextmsgstr);
    nextmsg = nextmsgstr;
    nextcmd = newsplit(&nextmsg);
    nextto = newsplit(&nextmsg);
    len = strlen(nextto);
    if ( strcmp(to, nextto) /* we don't stack to the same recipients */
        && !strcmp(cmd, nextcmd) && !strcmp(msg, nextmsg)
        && ((strlen(cmd) + victims.length() + strlen(nextto)
	     + strlen(msg) + 2) < 510)
        && (!stack_limit || cmd_count < stack_limit - 1)) {
      ++cmd_count;
      victims += stack_delim + nextto;

      doit = 1;
      m->next = nm->next;
      if (!nm->next)
        h->last = m;
      free(nm->msg);
      free(nm);
      --(h->tot);
    } else
      m = m->next;
  }
  if (doit) {
    len = simple_snprintf(tosend, sizeof(tosend), "%s %s %s", cmd, victims.c_str(), msg);
    write_to_server(tosend, len);
    ++burst;++flood_count;
    m = h->head->next;
    free(h->head->msg);
    free(h->head);
    h->head = m;
    if (!h->head)
      h->last = 0;
    --(h->tot);

    if (debug_output)
      putlog(LOG_SRVOUT, "*", "[%c=>] %s", qdsc[which].pfx, tosend);

    calc_penalty(tosend, len);
    return 1;
  }
  return 0;
}

/* Clean out the msg queues (like when changing servers).
 */
static void empty_msgq()
{
  for (size_t i = 0; i < (sizeof(qdsc) / sizeof(qdsc[0])); ++i)
    msgq_clear(qdsc[i].q);
  burst = 0;
  flood_count = 0;
  flood_time.sec = flood_time.usec = 0;
}

/* Use when sending msgs... will spread them out so there's no flooding.
 */
void queue_server(int which, char *buf, int len)
{
  /* Don't even BOTHER if there's no server online. */
  if (serv < 0)
    return;

  // If connect bursting, hold off any commands which would end the gracetime (flood_endgrace)
  if (connect_bursting && (which == DP_MODE || which == DP_MODE_NEXT || which == DP_SERVER || which == DP_SERVER_NEXT)) {
    if (!burst_ok(buf, len))
      which = DP_HELP;
  }

  int qnext = 0;
  int which_q = 0;

  switch (which) {
    case DP_MODE_NEXT:
      qnext = 1;
    case DP_MODE:
      which_q = Q_MODE;
      break;
    case DP_SERVER_NEXT:
      qnext = 1;
    case DP_SERVER:
      which_q = Q_SERVER;
      break;
    case DP_HELP_NEXT:
      qnext = 1;
    case DP_HELP:
      which_q = Q_HELP;
      break;
    case DP_PLAY:
      which_q = Q_PLAY;
      break;
    case DP_CACHE:
      which_q = Q_CACHE;
      break;
    default:
      putlog(LOG_MISC, "*", "!!! queuing unknown type to server!!");
      return;
  }

  struct msgq_head *h = qdsc[which_q].q;

  if (h->tot < qdsc[which_q].maxqmsg) {
    /* Don't queue msg if it's already queued?  */
    if (!qdsc[which_q].double_msg) {
      for (struct msgq* tq = qdsc[which_q].q->head; tq; tq = tq->next) {
	if (!strcasecmp(tq->msg, buf)) {
	  if (!double_warned) {
	    if (buf[len - 1] == '\n')
	      buf[len - 1] = 0;
	    putlog(LOG_DEBUG, "*", "msg already queued. skipping: %s", buf);
	    double_warned = 1;
	  }
	  return;
	}
      }
    }

    struct msgq *q = (struct msgq *) my_calloc(1, sizeof(struct msgq));

    if (h->head) {
      if (!qnext) { //Not next, add to end of queue
        h->last->next = q;
        h->last = q;
      } else if (qnext) { //Should be next, insert into front of queue
        q->next = h->head;
        h->head = q;
      }
    } else
      h->head = h->last = q;
    q->len = len;
    q->msg = (char *) my_calloc(1, len + 1);
    strlcpy(q->msg, buf, len + 1);
    ++(h->tot);
    h->warned = 0;
    double_warned = 0;
  } else {
    if (!h->warned)
      putlog(LOG_MISC, "*", "!!! OVER MAXIMUM %s QUEUE", qdsc[which_q].name);
    h->warned = 1;
  }

  if (debug_output && !h->warned)
    putlog(LOG_SRVOUT, "@", "[%s%c] %s", qnext ? "!!" : "!", qdsc[which_q].pfx, buf);

  /* Try flushing immediately */
  deq_msg();
}

/* Add a new server to the server_list.
 */
void add_server(char *ss)
{
  struct server_list *x = NULL, *z = NULL;
  char *p = NULL, *q = NULL;

  for (z = serverlist; z && z->next; z = z->next)
    ;
  while (ss) {
    p = strchr(ss, ',');
    if (p)
      *p++ = 0;
    x = (struct server_list *) my_calloc(1, sizeof(struct server_list));

    x->next = 0;
    x->port = 0;
    if (z)
      z->next = x;
    else
      serverlist = x;
    z = x;
    q = strchr(ss, ':');
    if (!q) {
      x->pass = 0;
      x->name = strdup(ss);
    } else {
#ifdef USE_IPV6
      if (ss[0] == '[') {
        ++ss;
        q = strchr(ss, ']');
        *q++ = 0; /* intentional */
      }
#endif /* USE_IPV6 */
      *q++ = 0;
      x->name = (char *) my_calloc(1, q - ss);
      strlcpy(x->name, ss, q - ss);
      ss = q;
      q = strchr(ss, ':');
      if (!q) {
	x->pass = 0;
      } else {
	*q++ = 0;
        x->pass = strdup(q);
      }
      if (!x->port) {
        x->port = atoi(ss);
      }
    }
    ss = p;
  }
}

/* Clear out the given server_list.
 */
void clearq(struct server_list *xx)
{
  struct server_list *x = NULL;

  while (xx) {
    x = xx->next;
    if (xx->name)
      free(xx->name);
    if (xx->pass)
      free(xx->pass);
    free(xx);
    xx = x;
  }
}

/* Set botserver to the next available server.
 *
 * -> if (*ptr == -1) then jump to that particular server
 */
void next_server(int *ptr, char *servname, in_port_t *port, char *pass)
{
  struct server_list *x = serverlist;

  if (x == NULL)
    return;

  int i = 0;

  /* -1  -->  Go to specified server */
  if (*ptr == (-1)) {
    for (; x; x = x->next) {
      if (x->port == *port) {
	if (!strcasecmp(x->name, servname)) {
	  *ptr = i;
	  return;
	}
      }
      i++;
    }
    /* Gotta add it: */
    x = (struct server_list *) my_calloc(1, sizeof(struct server_list));

    x->next = 0;
    x->name = strdup(servname);
    x->port = *port ? *port : default_port;
    if (pass && pass[0]) {
      x->pass = strdup(pass);
    } else
      x->pass = NULL;
    list_append((struct list_type **) (&serverlist), (struct list_type *) x);
    *ptr = i;
    return;
  }
  /* Find where i am and boogie */
  i = (*ptr);
  while (i > 0 && x != NULL) {
    x = x->next;
    i--;
  }
  if (x != NULL) {
    x = x->next;
    (*ptr)++;
  }				/* Go to next server */
  if (x == NULL) {
    x = serverlist;
    *ptr = 0;
  }				/* Start over at the beginning */
  strcpy(servname, x->name);
  *port = x->port ? x->port : default_port;
  if (x->pass)
    strcpy(pass, x->pass);
  else
    pass[0] = 0;
}

/*
 *     CTCP DCC CHAT functions
 */


static int sanitycheck_dcc(char *nick, char *from, char *ipaddy, char *port)
{
  /* According to the latest RFC, the clients SHOULD be able to handle
   * DNS names that are up to 255 characters long.  This is not broken.
   */

  char badaddress[16];
  in_addr_t ip = my_atoul(ipaddy);
  int prt = atoi(port);

  if (prt < 1) {
    putlog(LOG_MISC, "*", "ALERT: (%s!%s) specified an impossible port of %u!",
           nick, from, prt);
    return 0;
  }
  simple_snprintf(badaddress, sizeof(badaddress), "%u.%u.%u.%u", (ip >> 24) & 0xff, (ip >> 16) & 0xff,
          (ip >> 8) & 0xff, ip & 0xff);
  if (ip < (1 << 24)) {
    putlog(LOG_MISC, "*", "ALERT: (%s!%s) specified an impossible IP of %s!",
           nick, from, badaddress);
    return 0;
  }
  return 1;
}


static void dcc_chat_hostresolved(int);

/* This only handles CHAT requests, otherwise it's handled in filesys.
 */
static int ctcp_DCC_CHAT(char *nick, char *from, struct userrec *u, char *object, char *keyword, char *text)
{
  if (!ischanhub())
    return BIND_RET_LOG;

  char *action = NULL, *param = NULL, *ip = NULL, *prt = NULL;

  action = newsplit(&text);
  param = newsplit(&text);
  ip = newsplit(&text);
  prt = newsplit(&text);

  if (strcasecmp(action, "CHAT") || strcasecmp(object, botname) || !u)
    return BIND_RET_LOG;

  int i;
  bool ok = 1;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0 };

  get_user_flagrec(u, &fr, NULL);

  if (ischanhub() && !glob_chuba(fr))
   ok = 0;
  if (dcc_total == max_dcc) {
    putlog(LOG_MISC, "*", "DCC connections full: %s %s (%s!%s)", "CHAT", param, nick, from);
  } else if (!ok) {
    putlog(LOG_MISC, "*", "%s: %s!%s", ischanhub() ? "Refused DCC chat (no access)" : "Refused DCC chat (I'm not a chathub (+c))", nick, from);
  } else if (u_pass_match(u, "-")) {
    putlog(LOG_MISC, "*", "%s: %s!%s", "Refused DCC chat (no password)", nick, from);
  } else if (atoi(prt) < 1024 || atoi(prt) > 65535) {
    /* Invalid port */
    putlog(LOG_MISC, "*", "%s: CHAT (%s!%s)", "DCC invalid port", nick, from);
  } else {
    if (!sanitycheck_dcc(nick, from, ip, prt))
      return 1;

    i = new_dcc(&DCC_CHAT_PASS, sizeof(struct chat_info));

    if (i < 0) {
      putlog(LOG_MISC, "*", "DCC connection: CHAT (%s!%s)", nick, ip);
      return BIND_RET_BREAK;
    }
    dcc[i].addr = my_atoul(ip);
    dcc[i].port = atoi(prt);
    dcc[i].sock = -1;
    strlcpy(dcc[i].nick, u->handle, sizeof(dcc[i].nick));
    strlcpy(dcc[i].host, from, sizeof(dcc[i].host));
    dcc[i].timeval = now;
    dcc[i].user = u;

    dcc_chat_hostresolved(i);

//    egg_dns_reverse(dcc[i].addr, 20, dcc_chat_dns_callback, (void *) i);
  }
  return BIND_RET_BREAK;
}

//static void tandem_relay_dns_callback(void *client_data, const char *host, char **ips)

static void dcc_chat_hostresolved(int i)
{
  char buf[512] = "", ip[512] = "";
  struct flag_record fr = {FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0 };

  simple_snprintf(buf, sizeof buf, "%d", dcc[i].port);

  simple_snprintf(ip, sizeof ip, "%lu", (0xffffffff & ((long) dcc[i].addr)));
#ifdef USE_IPV6
  dcc[i].sock = getsock(0, AF_INET);
#else
  dcc[i].sock = getsock(0);
#endif /* USE_IPV6 */
  int open_telnet_return = 0;
  if (dcc[i].sock < 0 || (open_telnet_return = open_telnet_dcc(dcc[i].sock, ip, buf)) < 0) {
    if (open_telnet_return == -1)
      dcc[i].sock = -1;
    strlcpy(buf, strerror(errno), sizeof(buf));
    putlog(LOG_MISC, "*", "%s: CHAT (%s!%s)", "DCC connection failed", dcc[i].nick, dcc[i].host);
    putlog(LOG_MISC, "*", "    (%s)", buf);
    if (dcc[i].sock != -1)
      killsock(dcc[i].sock);
    lostdcc(i);
  } else {
    bool ok = 1;

    dcc[i].status = STAT_ECHO;
    get_user_flagrec(dcc[i].user, &fr, NULL);
    if (ischanhub() && !glob_chuba(fr))
     ok = 0;
    if (ok)
      dcc[i].status |= STAT_PARTY;
    struct chat_info dummy;
    strlcpy(dcc[i].u.chat->con_chan, (chanset) ? chanset->dname : "*", sizeof(dummy.con_chan));
    dcc[i].timeval = now;
    /* Ok, we're satisfied with them now: attempt the connect */
    putlog(LOG_MISC, "*", "DCC connection: CHAT (%s!%s)", dcc[i].nick, dcc[i].host);
    dprintf(i, "%s\n", response(RES_USERNAME));
  }
  return;
}

/*
 *     Server timer functions
 */

static void end_burstmode() {
  if (connect_bursting) {
    connect_bursting = 0;
    msgburst = real_msgburst;
    msgrate = real_msgrate;
  }
}

static void server_secondly()
{
  if (cycle_time)
    --cycle_time;
  if (!resolvserv && serv < 0 && !trying_server)
    connect_server();
  else if (server_online) {
    if (keepnick && !use_monitor) {
      static int ison_cnt = 0;

      if (ison_time == 0) //If someone sets this to 0, all hell will break loose!
        ison_time = 10;
      if (ison_cnt >= ison_time) {
        server_send_ison();
        ison_cnt = 0;
      } else
        ++ison_cnt;
    } else if (!keepnick && release_time && ((now - release_time) >= RELEASE_TIME)) {
      release_time = 0;
      keepnick = 1;
      nick_available(1, 0);
    }

    if (!loading) {
      static int cnt_10 = 0;

      // Every 10 seconds
      if (cnt_10 == 9) {
        // Ensure that +D/+f are not conflicting

        if (deaf_char) {
          // +f or auth bots in used need to see channel chatter.
          bool need_chatter = doflood(NULL) || (Auth::ht_host.size() && auth_chan && strlen(auth_prefix));

          // In +D but am +f, need to -D
          if (in_deaf && (need_chatter || !use_deaf)) {
            dprintf(DP_SERVER, "MODE %s -%c\n", botname, deaf_char);
            in_deaf = 0;
          } else if (!in_deaf && use_deaf && !need_chatter) {
            // Not +D but should be, probably had +f removed.
            dprintf(DP_SERVER, "MODE %s +%c\n", botname, deaf_char);
            in_deaf = 1;
          }
        }

        cnt_10 = 0;
      } else
        ++cnt_10;
    }
    if (connect_bursting && (now - SERVER_CONNECT_BURST_TIME) >= connect_bursting) {
      end_burstmode();
      putlog(LOG_DEBUG, "*", "Ending server burst mode");
    }
  }
}

static void server_check_lag()
{
  if (server_online && !waiting_for_awake && !trying_server) {
    dprintf(DP_MODE, "PING :%li\n", (long)now);
    lastpingtime = now;
    waiting_for_awake = 1;
  } else if (servidx != -1 && waiting_for_awake && ((now - lastpingtime) >= stoned_timeout)) {
    // Not checking server_online as this will handle connect timeouts as well where the connect() works, but the server gets stoned afterwards
    disconnect_server(servidx, DO_LOST);
    putlog(LOG_SERV, "*", "Server got stoned; jumping...");
  }
}

void reset_flood()
{
  flood_time.sec = last_time.sec = now - 100;
  flood_time.usec = last_time.usec = 0;
}

static void server_minutely()
{
  if (server_online) {
    // Ratbox sets a nick_delay (default:15min) timer when a nick splits off to prevent collisions,
    // We must check periodically to see if the local server has unjuped our wanted nicks.
    if (keepnick && (jnick_juped == 2 || nick_juped == 2)) {
      nick_available(1, 1);
    }
  }
}

void server_die()
{
  cycle_time = 100;
  if (server_online) {
    dprintf(-serv, "QUIT :%s\n", quit_msg[0] ? quit_msg : "");
    sleep(2); /* Give the server time to understand */
  }
  nuke_server(NULL);
}

/* A report on the module status.
 */
void server_report(int idx, int details)
{
  char s1[64] = "", s[128] = "";

  if (server_online) {
    dprintf(idx, "    Online as: %s%s%s (%s)\n", botname,
	    botuserhost[0] ? "!" : "", botuserhost[0] ? botuserhost : "",
	    botrealname);
    dprintf(idx, "    My userip: %s!%s\n", botname, botuserip);
    if (nick_juped)
      dprintf(idx, "    NICK IS JUPED: %s %s\n", origbotname, keepnick ? "(trying)" : "");
    if (jnick_juped)
      dprintf(idx, "    JUPENICK IS JUPED: %s %s\n", jupenick, keepnick ? "(trying)" : "");
    nick_juped = jnick_juped = 0;
    daysdur(now, server_online, s1, sizeof(s1));
    simple_snprintf(s, sizeof s, "(connected %s)", s1);
    if (server_lag && !waiting_for_awake) {
      if (server_lag == (-1))
	simple_snprintf(s1, sizeof s1, " (bad pong replies)");
      else
	simple_snprintf(s1, sizeof s1, " (lag: %ds)", server_lag);
      strlcat(s, s1, sizeof(s));
    }
  }
  if ((trying_server || server_online) && (servidx != (-1))) {
    dprintf(idx, "    Server %s:%d %s\n", dcc[servidx].host, dcc[servidx].port,
	    trying_server ? "(trying)" : s);
  } else
    dprintf(idx, "    No server currently.\n");

  if (server_online)
    dprintf(idx, "    burst: %d flood_count: %d\n", burst, flood_count);

  for (size_t i = 0; i < (sizeof(qdsc) / sizeof(qdsc[0])); ++i) {
    if (qdsc[i].q->tot)
      dprintf(idx, "    %s queue is at %d%%, %d msgs\n",
          qdsc[i].name,
          (int) ((float) (qdsc[i].q->tot * 100.0) / (float) qdsc[i].maxqmsg),
          (int) qdsc[i].q->tot);
  }
  if (details) {
    dprintf(idx, "    Flood is: %d msg/%ds, %d ctcp/%ds\n",
	    flood_msg.count, flood_msg.time, flood_ctcp.count, flood_ctcp.time);
  }
}

static void msgq_clear(struct msgq_head *qh)
{
  register struct msgq *qq = NULL;

  for (register struct msgq *q = qh->head; q; q = qq) {
    qq = q->next;
    free(q->msg);
    free(q);
  }
  qh->head = qh->last = NULL;
  qh->tot = qh->warned = 0;
}

static cmd_t my_ctcps[] =
{
  {"DCC",	"",	(Function) ctcp_DCC_CHAT,		"server:DCC", LEAF},
  {NULL,	NULL,	NULL,			NULL, 0}
};

void server_init()
{
  strlcpy(botrealname, "A deranged product of evil coders", sizeof(botrealname));

  /*
   * Init of all the variables *must* be done in _start rather than
   * globally.
   */

  BT_msg = bind_table_add("msg", 4, "ssUs", MATCH_FLAGS, 0);
  BT_raw = bind_table_add("raw", 2, "ss", 0, BIND_STACKABLE);
  BT_ctcr = bind_table_add("ctcr", 6, "ssUsss", 0, BIND_STACKABLE);
  BT_ctcp = bind_table_add("ctcp", 6, "ssUsss", 0, BIND_STACKABLE);

  add_builtins("raw", my_raw_binds);
  add_builtins("dcc", C_dcc_serv);
  add_builtins("ctcp", my_ctcps);

  egg_timeval_t howlong;

  howlong.sec = 0;
  howlong.usec = DEQ_RATE * 1000;

  timer_create_repeater(&howlong, "server_queue", (Function) deq_msg);
  timer_create_secs(1, "server_secondly", (Function) server_secondly);
  timer_create_secs(30, "server_check_lag", (Function) server_check_lag);
  timer_create_secs(60, "server_minutely", (Function) server_minutely);
//  timer_create_secs(60, "minutely_checks", (Function) minutely_checks);
}
