
/* 
 * server.c -- part of server.mod
 *   basic irc server support
 * 
 * $Id: server.c,v 1.30 2000/01/08 21:23:17 per Exp $
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

#define MODULE_NAME "server"
#define MAKING_SERVER
#include "main.h"
#include "hook.h"
#include "settings.h"
#include "users.h"
#include "server.h"
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifdef G_NODELAY
#include <netinet/tcp.h>
#endif

extern struct cfg_entry CFG_OPTIMESLACK;
extern char packname[];
#ifdef LEAF
char cursrvname[120]="";
extern char magickey[];
int ctcp_mode;
int serv;			/* sock # of server currently */
int strict_host;		/* strict masking of hosts ? */
char newserver[121];		/* new server? */
int newserverport;		/* new server port? */
char newserverpass[121];	/* new server password? */
time_t trying_server;		/* trying to connect to a server right now? */
int server_lag;			/* how lagged (in seconds) is the server? */
int last_server_ping;
char raltnick[NICKLEN];		/* random nick created from altnick */
int curserv;			/* current position in server list: */
int flud_thr;			/* msg flood threshold */
int flud_time;			/* msg flood time */
int flud_ctcp_thr;		/* ctcp flood threshold */
int flud_ctcp_time;		/* ctcp flood time */
#ifdef G_USETCL
char initserver[121];		/* what, if anything, to send to the
				   * server on connection */
char connectserver[121];	/* what, if anything, to do before connect
				 * to the server */
#endif
#endif
char botuserhost[121];		/* bot's user@host (refreshed whenever the
				   * bot joins a channel) */
				/* may not be correct user@host BUT it's
				 * how the server sees it */
#ifdef LEAF
int check_stoned;		/* Check for a stoned server? */
int serverror_quit;		/* Disconnect from server if ERROR

				   * messages received? */
time_t server_online;		/* server connection time */
time_t server_cycle_wait;	/* seconds to wait before

				   * re-beginning the server list */
char botrealname[121];		/* realname of bot */
int min_servs;			/* minimum number of servers to be around */
int server_timeout;		/* server timeout for connecting */
int never_give_up;		/* never give up when connecting to servers? */
int strict_servernames;		/* don't update server list */
struct server_list *serverlist;	/* old-style queue, still used by
				 * server list */
char tryingserver[UHOSTLEN];
int cycle_time;			/* cycle time till next server connect */
int default_port;		/* default IRC port */
char oldnick[NICKLEN];		/* previous nickname *before* rehash */
int trigger_on_ignore;		/* trigger bindings if user is ignored ? */
int answer_ctcp;		/* answer how many stacked ctcp's ? */
int lowercase_ctcp;		/* answer lowercase CTCP's (non-standard) */
char bothost[81];		/* dont mind me, Im stupid */
int check_mode_r;		/* check for IRCNET +r modes */
int net_type;
int must_be_owner;		/* arthur2 */
char *curslist = NULL;

/* allow a msgs being twice in a queue ? */
int double_mode;
int double_server;
int double_help;
int double_warned;

p_tcl_bind_list H_raw,
  H_wall,
  H_notc,
  H_msgm,
  H_msg,
  H_flud,
  H_ctcr,
  H_ctcp;

void empty_msgq(void);
void next_server(int *, char *, unsigned int *, char *);
void disconnect_server(int);

time_t last_ctcp = (time_t) 0L;
int count_ctcp = 0;
char altnick_char = 0;
char cpass[255];
char cserver[1023];

extern struct dcc_t *dcc;
extern time_t now;
extern struct userrec *userlist;
extern int dcc_total,
  max_dcc,
  min_dcc_port,
  max_dcc_port,
  role;
extern char botname[],
  origbotname[],
  owner[],
  botuser[];
extern char kickprefix[],
  netpass[],
  botnetnick[],
  hostname[];
extern struct chanset_t *chanset;
extern int ignore_time,
  use_silence;
extern int
  timesync;

int ctcp_DCC_CHAT(char *nick, char *from, char *handle, char *object, char *keyword, char *text);

char * kickreason(int kind);

/* We try to change to a preferred unique nick here. We always first try the
 * specified alternate nick. If that failes, we repeatedly modify the nick
 * until it gets accepted.
 * 
 * sent nick:
 *     "<altnick><c>"
 *                ^--- additional count character: 1-9^-_\\[]`a-z
 *          ^--------- given, alternate nick
 * 
 * The last added character is always saved in altnick_char. At the very first
 * attempt (were altnick_char is 0), we try the alternate nick without any
 * additions.
 * 
 * fixed by guppy (1999/02/24) and Fabian (1999/11/26)
 */
int server_gotfake433(char *from)
{
  int l = strlen(botname) - 1;
  char *oknicks = STR("\\`-^_[]{}");
  Context;
  /* First run? */
  if (altnick_char == 0) {
    altnick_char = oknicks[0];
    if (l + 1 == NICKMAX) {
      botname[l] = altnick_char;
    } else {
      botname[++l] = altnick_char;
      botname[l + 1] = 0;
    }
  } else {
    char *p = strchr(oknicks, altnick_char);
    p++;
    if (!*p)
      altnick_char = 'a' + random() % 26;
    else
      altnick_char = (*p);
    botname[l] = altnick_char;
  }
  log(LCAT_INFO, STR("NICK IN USE: Trying '%s'"), botname);
  dprintf(DP_MODE, STR("NICK %s\n"), botname);
  return 0;
}

/* check for tcl-bound msg command, return 1 if found */

/* msg: proc-name <nick> <user@host> <handle> <args...> */
int check_tcl_msg(char *cmd, char *nick, char *uhost, struct userrec *u, char *args)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0 };
  char *hand = u ? u->handle : "*";
  int x;

  Context;
  get_user_flagrec(u, &fr, NULL);
#ifdef G_USETCL
  Tcl_SetVar(interp, STR("_msg1"), nick, 0);
  Tcl_SetVar(interp, STR("_msg2"), uhost, 0);
  Tcl_SetVar(interp, STR("_msg3"), hand, 0);
  Tcl_SetVar(interp, STR("_msg4"), args, 0);
  Context;
  x = check_tcl_bind(H_msg, cmd, &fr, STR(" $_msg1 $_msg2 $_msg3 $_msg4"), MATCH_PARTIAL | BIND_HAS_BUILTINS | BIND_USE_ATTR);
#else
  x = check_tcl_bind(H_msg, cmd, &fr, make_bind_param(4, nick, uhost, hand, args), MATCH_PARTIAL | BIND_HAS_BUILTINS | BIND_USE_ATTR);
#endif
  Context;
  if (x == BIND_EXEC_LOG)
    log(LCAT_COMMAND, STR("(%s!%s) !%s! %s %s"), nick, uhost, hand, cmd, args);
  return ((x == BIND_MATCHED) || (x == BIND_EXECUTED)
	  || (x == BIND_EXEC_LOG));
}

void check_tcl_notc(char *nick, char *uhost, struct userrec *u, char *arg)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0 };

  Context;
  get_user_flagrec(u, &fr, NULL);
#ifdef G_USETCL
  Tcl_SetVar(interp, STR("_notc1"), nick, 0);
  Tcl_SetVar(interp, STR("_notc2"), uhost, 0);
  Tcl_SetVar(interp, STR("_notc3"), u ? u->handle : "*", 0);
  Tcl_SetVar(interp, STR("_notc4"), arg, 0);
  Context;
  check_tcl_bind(H_notc, arg, &fr, STR(" $_notc1 $_notc2 $_notc3 $_notc4"), MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
#else
  check_tcl_bind(H_notc, arg, &fr, make_bind_param(4, nick, uhost, u ? u->handle : "*", arg), MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
#endif
  Context;
}

void check_tcl_msgm(char *cmd, char *nick, char *uhost, struct userrec *u, char *arg)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0 };
  char args[1024];

  Context;
  if (arg[0])
    simple_sprintf(args, STR("%s %s"), cmd, arg);
  else
    strcpy(args, cmd);
  get_user_flagrec(u, &fr, NULL);
#ifdef G_USETCL
  Tcl_SetVar(interp, STR("_msgm1"), nick, 0);
  Tcl_SetVar(interp, STR("_msgm2"), uhost, 0);
  Tcl_SetVar(interp, STR("_msgm3"), u ? u->handle : "*", 0);
  Tcl_SetVar(interp, STR("_msgm4"), args, 0);
  Context;
  check_tcl_bind(H_msgm, args, &fr, STR(" $_msgm1 $_msgm2 $_msgm3 $_msgm4"), MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
#else
  check_tcl_bind(H_msgm, args, &fr, make_bind_param(4, nick, uhost, u ? u->handle : "*", args), MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
#endif
  Context;
}

/* return 1 if processed */
int check_tcl_raw(char *from, char *code, char *msg)
{
#ifdef G_USETCL
  int x;

  Context;
  Tcl_SetVar(interp, STR("_raw1"), from, 0);
  Tcl_SetVar(interp, STR("_raw2"), code, 0);
  Tcl_SetVar(interp, STR("_raw3"), msg, 0);
  Context;
  x = check_tcl_bind(H_raw, code, 0, STR(" $_raw1 $_raw2 $_raw3"), MATCH_EXACT | BIND_STACKABLE | BIND_WANTRET);
  Context;
  return (x == BIND_EXEC_LOG);
#else
  int x;

  x = check_tcl_bind(H_raw, code, 0, make_bind_param(3, from, code, msg), MATCH_EXACT | BIND_STACKABLE | BIND_WANTRET);
  return (x == BIND_EXEC_LOG);
#endif
}

int check_tcl_ctcpr(char *nick, char *uhost, struct userrec *u, char *dest, char *keyword, char *args, p_tcl_bind_list table)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0 };
  int x;

  Context;
  get_user_flagrec(u, &fr, NULL);
#ifdef G_USETCL
  Tcl_SetVar(interp, STR("_ctcpr1"), nick, 0);
  Tcl_SetVar(interp, STR("_ctcpr2"), uhost, 0);
  Tcl_SetVar(interp, STR("_ctcpr3"), u ? u->handle : "*", 0);
  Tcl_SetVar(interp, STR("_ctcpr4"), dest, 0);
  Tcl_SetVar(interp, STR("_ctcpr5"), keyword, 0);
  Tcl_SetVar(interp, STR("_ctcpr6"), args, 0);
  x = check_tcl_bind(table, keyword, &fr, STR(" $_ctcpr1 $_ctcpr2 $_ctcpr3 $_ctcpr4 $_ctcpr5 $_ctcpr6"), (lowercase_ctcp ? MATCH_EXACT : MATCH_CASE)
		     | BIND_USE_ATTR | BIND_STACKABLE | ((table == H_ctcp) ? BIND_WANTRET : 0));
#else
  x = check_tcl_bind(table, keyword, &fr, make_bind_param(6, nick, uhost, u ? u->handle : "*", dest, keyword, args), (lowercase_ctcp ? MATCH_EXACT : MATCH_CASE)
		     | BIND_USE_ATTR | BIND_STACKABLE | ((table == H_ctcp) ? BIND_WANTRET : 0));

#endif
  Context;
  return (x == BIND_EXEC_LOG) || (table == H_ctcr);
}

int check_tcl_wall(char *from, char *msg)
{
  int x;

  Context;
#ifdef G_USETCL
  Tcl_SetVar(interp, STR("_wall1"), from, 0);
  Tcl_SetVar(interp, STR("_wall2"), msg, 0);
  Context;
  x = check_tcl_bind(H_wall, msg, 0, STR(" $_wall1 $_wall2"), MATCH_MASK | BIND_STACKABLE);
#else
  x = check_tcl_bind(H_wall, msg, 0, make_bind_param(2, from, msg), MATCH_MASK | BIND_STACKABLE);
#endif
  Context;
  if (x == BIND_EXEC_LOG) {
    log(LCAT_COMMAND, STR("!%s! %s"), from, msg);
    return 1;
  } else
    return 0;
}

int check_tcl_flud(char *nick, char *uhost, struct userrec *u, char *ftype, char *chname)
{
  int x;

  Context;
#ifdef G_USETCL
  Tcl_SetVar(interp, STR("_flud1"), nick, 0);
  Tcl_SetVar(interp, STR("_flud2"), uhost, 0);
  Tcl_SetVar(interp, STR("_flud3"), u ? u->handle : "*", 0);
  Tcl_SetVar(interp, STR("_flud4"), ftype, 0);
  Tcl_SetVar(interp, STR("_flud5"), chname, 0);
  Context;
  x = check_tcl_bind(H_flud, ftype, 0, STR(" $_flud1 $_flud2 $_flud3 $_flud4 $_flud5"), MATCH_MASK | BIND_STACKABLE | BIND_WANTRET);
#else
  x = check_tcl_bind(H_flud, ftype, 0, make_bind_param(5, nick, uhost, u ? u->handle : "*", ftype, chname), MATCH_MASK | BIND_STACKABLE | BIND_WANTRET);
#endif
  Context;
  return (x == BIND_EXEC_LOG);
}

int match_my_nick(char *nick)
{
  if (!rfc_casecmp(nick, botname))
    return 1;
  return 0;
}

/* 001: welcome to IRC (use it to fix the server name) */
int server_got001(char *from, char *msg)
{
  struct server_list *x;
  int i,
    servidx = findanyidx(serv);
  struct chanset_t *chan;

  /* ok...param #1 of 001 = what server thinks my nick is */
  server_online = now;
  fixcolon(msg);
  strncpy0(botname, msg, NICKMAX+1);
  altnick_char = 0;
  strncpy0(cursrvname, from, sizeof(cursrvname));

  /* init-server */
#ifdef G_USETCL
  if (initserver[0])
    do_tcl(STR("init-server"), initserver);
#endif
  dprintf(DP_MODE, STR("MODE %s +i\n"), botname);
  x = serverlist;
  if (x == NULL)
    return 0;			/* uh, no server list */
  /* below makes a mess of DEBUG_OUTPUT can we do something else?
   * buggerit, 1 channel at a time is much neater
   * only do it if the IRC module is loaded */
  dprintf(DP_MODE, STR("PING :%lu\n"), (unsigned long) now);
  last_server_ping = now;
  for (chan = chanset; chan; chan = chan->next) {
    chan->status &= ~(CHAN_ACTIVE | CHAN_PEND);
    if (shouldjoin(chan))
      dprintf(DP_SERVER, STR("JOIN %s %s\n"), chan->name, chan->key_prot);
  }
  if (strcasecmp(from, dcc[servidx].host)) {
    log(LCAT_INFO, STR("(%s claims to be %s; updating server list)"), dcc[servidx].host, from);
    for (i = curserv; i > 0 && x != NULL; i--)
      x = x->next;
    if (x == NULL) {
      log(LCAT_MISC, STR("Invalid server list!"));
      return 0;
    }
    if (x->realname)
      nfree(x->realname);
    if (strict_servernames == 1) {
      x->realname = NULL;
      if (x->name)
	nfree(x->name);
      x->name = nmalloc(strlen(from) + 1);
      strcpy(x->name, from);
    } else {
      x->realname = nmalloc(strlen(from) + 1);
      strcpy(x->realname, from);
    }
  }
  return 0;
}

/* close the current server connection */
void nuke_server(char *reason)
{
  if (serv >= 0) {
    int servidx = findanyidx(serv);

    server_online = 0;
    if (reason && (servidx > 0))
      dprintf(servidx, STR("QUIT :%s\n"), reason);
    disconnect_server(servidx);
    lostdcc(servidx);
  }
}

char ctcp_reply[1024] = "";

int lastmsgs[FLOOD_GLOBAL_MAX];
char lastmsghost[FLOOD_GLOBAL_MAX][81];
time_t lastmsgtime[FLOOD_GLOBAL_MAX];

/* do on NICK, PRIVMSG, and NOTICE -- and JOIN */
int detect_flood(char *floodnick, char *floodhost, char *from, int which)
{
  char *p;
  char ftype[10],
    h[1024];
  struct userrec *u;
  int thr = 0,
    lapse = 0;
  int atr;

  Context;
  u = get_user_by_host(from);
  atr = u ? u->flags : 0;
  if (atr & (USER_BOT | USER_FRIEND))
    return 0;
  /* determine how many are necessary to make a flood */
  switch (which) {
  case FLOOD_PRIVMSG:
  case FLOOD_NOTICE:
    thr = flud_thr;
    lapse = flud_time;
    strcpy(ftype, STR("msg"));
    break;
  case FLOOD_CTCP:
    thr = flud_ctcp_thr;
    lapse = flud_ctcp_time;
    strcpy(ftype, STR("ctcp"));
    break;
  }
  if ((thr == 0) || (lapse == 0))
    return 0;			/* no flood protection */
  /* okay, make sure i'm not flood-checking myself */
  if (match_my_nick(floodnick))
    return 0;
  if (!strcasecmp(floodhost, botuserhost))
    return 0;			/* my user@host (?) */
  p = strchr(floodhost, '@');
  if (p) {
    p++;
    if (strcasecmp(lastmsghost[which], p)) {	/* new */
      strcpy(lastmsghost[which], p);
      lastmsgtime[which] = now;
      lastmsgs[which] = 0;
      return 0;
    }
  } else
    return 0;			/* uh... whatever. */
  if (lastmsgtime[which] < now - lapse) {
    /* flood timer expired, reset it */
    lastmsgtime[which] = now;
    lastmsgs[which] = 0;
    return 0;
  }
  lastmsgs[which]++;
  if (lastmsgs[which] >= thr) {	/* FLOOD */
    /* reset counters */
    lastmsgs[which] = 0;
    lastmsgtime[which] = 0;
    lastmsghost[which][0] = 0;
    u = get_user_by_host(from);
    if (check_tcl_flud(floodnick, from, u, ftype, "*"))
      return 0;

    /* private msg */
    simple_sprintf(h, STR("*!*@%s"), p);
    log(LCAT_WARNING, STR("Flood from @%s! Placing on ignore!"), p);
    addignore(h, origbotname, (which == FLOOD_CTCP) ? STR("CTCP flood") : "MSG/NOTICE flood", now + (60 * ignore_time));
    if (use_silence)
      /* attempt to use ircdu's SILENCE command */
      dprintf(DP_MODE, STR("SILENCE +*@%s\n"), p);
  }
  return 0;
}

/* check for more than 8 control characters in a line
 * this could indicate: beep flood CTCP avalanche */
int detect_avalanche(char *msg)
{
  int count = 0;
  unsigned char *p;

  for (p = (unsigned char *) msg; (*p) && (count < 8); p++)
    if ((*p == 7) || (*p == 1))
      count++;
  if (count >= 8)
    return 1;
  else
    return 0;
}

/* private message */
int server_gotmsg(char *from, char *msg)
{
  char *to,
    buf[UHOSTLEN],
   *nick,
    ctcpbuf[512],
   *uhost = buf,
   *ctcp;
  char *p,
   *p1,
   *code;
  struct userrec *u;
  int ctcp_count = 0;
  int ignoring;

  if (msg[0] && ((strchr(CHANMETA, *msg) != NULL) || (*msg == '@')))	/* notice to a channel, not handled here */
    return 0;
  ignoring = match_ignore(from);
  to = newsplit(&msg);
  fixcolon(msg);
  /* only check if flood-ctcp is active */
  strcpy(uhost, from);
  nick = splitnick(&uhost);
  u = get_user_by_host(from);
  if (u && (u->flags & USER_OWNER))
    ignoring = 0;
  if (flud_ctcp_thr && detect_avalanche(msg)) {
    if (!ignoring) {
      log(LCAT_WARNING, STR("Avalanche from %s - ignoring"), from);
      p = strchr(uhost, '@');
      if (p != NULL)
	p++;
      else
	p = uhost;
      simple_sprintf(ctcpbuf, STR("*!*@%s"), p);
      addignore(ctcpbuf, origbotname, STR("ctcp avalanche"), now + (60 * ignore_time));
    }
  }
  /* check for CTCP: */
  ctcp_reply[0] = 0;
  p = strchr(msg, 1);
  while ((p != NULL) && (*p)) {
    p++;
    p1 = p;
    while ((*p != 1) && (*p != 0))
      p++;
    if (*p == 1) {
      *p = 0;
      ctcp = strcpy(ctcpbuf, p1);
      strcpy(p1 - 1, p + 1);
      if (!ignoring)
	detect_flood(nick, uhost, from, strncmp(ctcp, STR("ACTION "), 7) ? FLOOD_CTCP : FLOOD_PRIVMSG);
      /* respond to the first answer_ctcp */
      p = strchr(msg, 1);
      if (ctcp_count < answer_ctcp) {
	ctcp_count++;
	if (ctcp[0] != ' ') {
	  code = newsplit(&ctcp);
	  if ((to[0] == '$') || strchr(to, '.')) {
	    if (!ignoring)
	      /* don't interpret */
	      log(LCAT_PUBLIC, STR("CTCP %s: %s from %s (%s) to %s"), code, ctcp, nick, uhost, to);
	  } else {
	    u = get_user_by_host(from);
	    if (!ignoring || trigger_on_ignore) {

	      if (!ctcp_DCC_CHAT(nick, uhost, u ? u->handle : "*", to, code, ctcp)
		  && !check_tcl_ctcp(nick, uhost, u, to, code, ctcp)
		  && !ignoring) {
		if ((lowercase_ctcp && !strcasecmp(code, STR("DCC")))
		    || (!lowercase_ctcp && !strcmp(code, STR("DCC")))) {
		  /* if it gets this far unhandled, it means that
		   * the user is totally unknown */
		  code = newsplit(&ctcp);
		  if (!strcmp(code, STR("CHAT"))) {
		    log(LCAT_CONN, STR("Refused DCC chat (no access): %s"), from);
		    log(LCAT_WARNING, STR("Refused DCC chat (no access): %s"), from);
		  } else
		    log(LCAT_CONN, STR("Refused DCC %s: %s"), code, from);
		  log(LCAT_WARNING, STR("Refused DCC %s: %s"), code, from);
		}
	      }
	      if (!strcmp(code, STR("ACTION"))) {
		log(LCAT_MESSAGE, STR("Action to %s: %s %s"), to, nick, ctcp);
	      } else {
		log(LCAT_MESSAGE, STR("CTCP %s: %s from %s (%s)"), code, ctcp, nick, uhost);
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
      if (now - last_ctcp > flud_ctcp_time) {
	dprintf(DP_SERVER, STR("NOTICE %s :%s\n"), nick, ctcp_reply);
	count_ctcp = 1;
      } else if (count_ctcp < flud_ctcp_thr) {
	dprintf(DP_SERVER, STR("NOTICE %s :%s\n"), nick, ctcp_reply);
	count_ctcp++;
      }
      last_ctcp = now;
    }
  }
  if (msg[0]) {
    if ((to[0] == '$') || (strchr(to, '.') != NULL)) {
      /* msg from oper */
      if (!ignoring) {
	detect_flood(nick, uhost, from, FLOOD_PRIVMSG);
	/* do not interpret as command */
	log(LCAT_MESSAGE, STR("[%s!%s to %s] %s"), nick, uhost, to, msg);
      }
    } else {
      char *code;

      if (!ignoring)
	detect_flood(nick, uhost, from, FLOOD_PRIVMSG);
      if (u && (u->flags & USER_BOT))
	return 1;
      code = newsplit(&msg);
      rmspace(msg);
      if (!ignoring || trigger_on_ignore)
	check_tcl_msgm(code, nick, uhost, u, msg);
      if (!ignoring)
	if (!check_tcl_msg(code, nick, uhost, u, msg))
	  log(LCAT_MESSAGE, STR("[%s] %s %s"), from, code, msg);
    }
  }
  return 0;
}

/* private notice */
int server_gotnotice(char *from, char *msg)
{
  char *to,
   *nick,
    ctcpbuf[512],
   *p,
   *p1,
    buf[512],
   *uhost = buf,
   *ctcp;
  struct userrec *u;
  int ignoring;

  if (msg[0] && ((strchr(CHANMETA, *msg) != NULL) || (*msg == '@')))	/* notice to a channel, not handled here */
    return 0;
  ignoring = match_ignore(from);
  to = newsplit(&msg);
  fixcolon(msg);
  strcpy(uhost, from);
  nick = splitnick(&uhost);
  if (flud_ctcp_thr && detect_avalanche(msg)) {
    /* discard -- kick user if it was to the channel */
    if (!ignoring)
      log(LCAT_WARNING, STR("Avalanche from %s"), from);
    return 0;
  }
  /* check for CTCP: */
  p = strchr(msg, 1);
  while ((p != NULL) && (*p)) {
    p++;
    p1 = p;
    while ((*p != 1) && (*p != 0))
      p++;
    if (*p == 1) {
      *p = 0;
      ctcp = strcpy(ctcpbuf, p1);
      strcpy(p1 - 1, p + 1);
      if (!ignoring)
	detect_flood(nick, uhost, from, FLOOD_CTCP);
      p = strchr(msg, 1);
      if (ctcp[0] != ' ') {
	char *code = newsplit(&ctcp);

	if ((to[0] == '$') || strchr(to, '.')) {
	  if (!ignoring)
	    log(LCAT_PUBLIC, STR("CTCP reply %s: %s from %s (%s) to %s"), code, ctcp, nick, from, to);
	} else {
	  u = get_user_by_host(from);
	  if (!ignoring || trigger_on_ignore) {
	    check_tcl_ctcr(nick, from, u, to, code, ctcp);
	    if (!ignoring)
	      /* who cares? */
	      log(LCAT_MESSAGE, STR("CTCP reply %s: %s from %s (%s) to %s"), code, ctcp, nick, from, to);
	  }
	}
      }
    }
  }
  if (msg[0]) {
    if (((to[0] == '$') || strchr(to, '.')) && !ignoring) {
      detect_flood(nick, uhost, from, FLOOD_NOTICE);
      log(LCAT_MESSAGE, STR("-%s (%s) to %s- %s"), nick, uhost, to, msg);
    } else {
      detect_flood(nick, uhost, from, FLOOD_NOTICE);
      u = get_user_by_host(from);
      /* server notice? */
      if ((from[0] == 0) || (nick[0] == 0)) {
	/* hidden `250' connection count message from server */
	if (strncmp(msg, STR("Highest connection count:"), 25))
	  log(LCAT_INFO, STR("-NOTICE- %s"), msg);
      } else if (!ignoring) {
	check_tcl_notc(nick, from, u, msg);
	log(LCAT_MESSAGE, STR("-%s (%s)- %s"), nick, from, msg);
      }
    }
  }
  return 0;
}

/* got 251: lusers
 * <server> 251 <to> :there are 2258 users and 805 invisible on 127 servers */
int server_got251(char *from, char *msg)
{
  int i;
  char *servs;

  if (min_servs == 0)
    return 0;			/* no minimum limit on servers */
  newsplit(&msg);
  fixcolon(msg);		/* NOTE!!! If servlimit is not set or is 0 */
  for (i = 0; i < 8; i++)
    newsplit(&msg);		/* lusers IS NOT SENT AT ALL!! */
  servs = newsplit(&msg);
  if (strncmp(msg, STR("servers"), 7))
    return 0;			/* was invalid format */
  while (*servs && (*servs < 32))
    servs++;			/* I've seen some lame nets put bolds &
				 * stuff in here :/ */
  i = atoi(servs);
  return 0;
}

/* WALLOPS: oper's nuisance */
int server_gotwall(char *from, char *msg)
{
  char *nick;
  char *p;
  int r = 0;

  Context;

  fixcolon(msg);
  p = strchr(from, '!');
  if (p && (p == strrchr(from, '!'))) {
    nick = splitnick(&from);
    r = check_tcl_wall(nick, msg);
    if (r == 0)
      log(LCAT_MESSAGE, STR("!%s(%s)! %s"), nick, from, msg);
  } else {
    r = check_tcl_wall(from, msg);
    if (r == 0)
      log(LCAT_MESSAGE, STR("!%s! %s"), from, msg);
  }
  return 0;
}

void minutely_checks()
{
  /* called once a minute... but if we're the only one on the
   * channel, we only wanna send out "lusers" once every 5 mins */
  static int count = 4;
  int ok = 0;
  struct chanset_t *chan;

  /* Only check if we have already successfully logged in */
  if (!server_online)
    return;
  if (!(rand() % 8)) {
    dprintf(DP_HELP, STR("PRIVMSG %s :%lu\n"), botname, now);
  }
  if (min_servs == 0)
    return;
  chan = chanset;
  while (chan != NULL) {
    if (channel_active(chan) && (chan->channel.members == 1))
      ok = 1;
    chan = chan->next;
  }
  if (!ok)
    return;
  count++;
  if (count >= 5) {
    dprintf(DP_SERVER, STR("LUSERS\n"));
    count = 0;
  }
}

/* pong from server */
int server_gotpong(char *from, char *msg)
{
  long l;

  newsplit(&msg);
  fixcolon(msg);		/* scrap server name */
  l = my_atoul(msg);
  if (l > (now - 10000)) {
    server_lag = now - l;
  }
  last_server_ping = 0;
  return 0;
}


/* 432 : bad nickname */
int server_got432(char *from, char *msg)
{
  char *erroneus;

  newsplit(&msg);
  erroneus = newsplit(&msg);
  if (server_online) {
    log(LCAT_ERROR, STR("NICK IN INVALID: %s (keeping '%s')."), erroneus, botname);
  } else {
    log(LCAT_ERROR, STR("Server says my nickname is invalid."));
    makepass(erroneus);
    erroneus[NICKMAX] = 0;
    dprintf(DP_MODE, STR("NICK %s\n"), erroneus);
    return 0;
  }
  return 0;
}

/* 433 : nickname in use
 * change nicks till we're acceptable or we give up */
int server_got433(char *from, char *msg)
{
  char *tmp;

  Context;
  if (server_online) {
    /* we are online and have a nickname, we'll keep it */
    newsplit(&msg);
    tmp = newsplit(&msg);
    log(LCAT_INFO, STR("NICK IN USE: %s (keeping '%s')."), tmp, botname);
    return 0;
  }
  Context;
  server_gotfake433(from);
  return 0;
}

/* 437 : nickname juped (IRCnet) */
int server_got437(char *from, char *msg)
{
  char *s;
  struct chanset_t *chan;

  newsplit(&msg);
  s = newsplit(&msg);
  if (s[0] && (strchr(CHANMETA, s[0]) != NULL)) {
    chan = findchan(s);
    if (chan) {
      if (chan->status & CHAN_ACTIVE) {
	log(LCAT_INFO, STR("Can't change nickname on %s. Is my nickname banned?"), s);
      } else {
	log(LCAT_INFO, STR("Channel %s is juped. :("), s);
      }
    }
  } else if (server_online) {
    log(LCAT_INFO, STR("NICK IS JUPED: %s (keeping '%s')."), s, botname);
  } else {
    log(LCAT_INFO, STR("Nickname has been juped: %s"), s);
    server_gotfake433(from);
  }
  return 0;
}

/* 438 : nick change too fast */
int server_got438(char *from, char *msg)
{
  Context;
  newsplit(&msg);
  newsplit(&msg);
  fixcolon(msg);
  log(LCAT_INFO, "%s", msg);
  return 0;
}

int server_got451(char *from, char *msg)
{
  /* usually if we get this then we really messed up somewhere
   * or this is a non-standard server, so we log it and kill the socket
   * hoping the next server will work :) -poptix */
  /* um, this does occur on a lagged anti-spoof server connection if the
   * (minutely) sending of joins occurs before the bot does its ping reply
   * probably should do something about it some time - beldin */
  log(LCAT_ERROR, STR("%s says I'm not registered"), from);
  nuke_server(STR("You have a fucked up server."));
  return 0;
}

/* error notice */
int server_goterror(char *from, char *msg)
{
  char *p, *p2, hub[15];
  fixcolon(msg);
  log(LCAT_ERROR, STR("ERROR from %s: %s"), tryingserver, msg);
  p=msg;
  /* ERROR :Closing Link: host nick (Killed ... */
  newsplit(&p);
  newsplit(&p);
  newsplit(&p);
  p2=newsplit(&p);
  if (!rfc_casecmp(botname, p2)) {
    /* It's a /KILL or /KLINE */
    log(LCAT_WARNING, STR("KILLED from %s: %s"), from, p);
    besthub(hub);
    if (hub && hub[0])
      botnet_send_zapf(nextbot(hub), botnetnick, hub, "kl");
    nuke_server(STR("Killed"));
    return 1;
  }
  if (serverror_quit) {
    log(LCAT_INFO, STR("Disconnecting from server."));
    nuke_server(STR("Bah, stupid error messages."));
  }
  return 1;
}

/* nick change */
int server_gotnick(char *from, char *msg)
{
  char *nick;
  struct userrec *u;

  u = get_user_by_host(from);
  nick = splitnick(&from);
  fixcolon(msg);
  if (match_my_nick(nick)) {
    /* regained nick! */
    strncpy0(botname, msg, NICKMAX+1);
    altnick_char = 0;
    log(LCAT_INFO, STR("Nickname changed to '%s'"), msg);
  }
  return 0;
}

int server_gotmode(char *from, char *msg)
{
  char *ch;

  ch = newsplit(&msg);
  /* usermode changes? */
  if (strchr(CHANMETA, ch[0]) == NULL) {
  }
  return 0;
}

void disconnect_server(int idx)
{
  server_online = 0;
  if (dcc[idx].sock >= 0)
    killsock(dcc[idx].sock);
  dcc[idx].sock = (-1);
  serv = (-1);
}

void eof_server(int idx)
{
  log(LCAT_INFO, STR("Disconnected from %s"), dcc[idx].host);
  disconnect_server(idx);
  lostdcc(idx);
}

void display_server(int idx, char *buf)
{
  sprintf(buf, STR("%s  (lag: %d)"), trying_server ? STR("conn") : STR("serv"), server_lag);
}
void connect_server(void);

void kill_server(int idx, void *x)
{
  struct chanset_t *chan;

  disconnect_server(idx);

  for (chan = chanset; chan; chan = chan->next)
    clear_channel(chan, 1);
  connect_server();
}

void timeout_server(int idx)
{
  log(LCAT_INFO, STR("Timeout: connect to %s"), dcc[idx].host);
  disconnect_server(idx);
  lostdcc(idx);
}

void server_activity(int idx, char *msg, int len);

struct dcc_table SERVER_SOCKET = {
  "SERVER",
  0,
  eof_server,
  server_activity,
  0,
  timeout_server,
  display_server,
  0,
  kill_server,
  0
};

int isop(char *mode)
{
  int state = 0,
    cnt = 0;
  char *p;

  p = mode;
  while ((*p) && (*p != ' ')) {
    if (*p == '-')
      state = 1;
    else if (*p == '+')
      state = 0;
    else if ((!state) && (*p == 'o'))
      cnt++;
    p++;
  }
  return (cnt >= 1);
}

int ismdop(char *mode)
{
  int state = 0,
    cnt = 0;
  char *p;

  p = mode;
  while ((*p) && (*p != ' ')) {
    if (*p == '-')
      state = 1;
    else if (*p == '+')
      state = 0;
    else if ((state) && (*p == 'o'))
      cnt++;
    p++;
  }
  return (cnt >= 3);
}

void got_rsn(char *botnick, char *code, char *par) {
  if (strcmp(origbotname, botname))
    dprintf(DP_MODE, STR("NICK %s\n"), origbotname);
}

void got_rn(char *botnick, char *code, char *par) {
  int l = (rand() % 4) + 6, i;
  char newnick[NICKLEN+1];
  for (i=0;i<l;i++)
    newnick[i]=(rand() % 2) * 32 + 65 + rand() % 26;
  newnick[l]=0;
  dprintf(DP_MODE, STR("NICK %s\n"), newnick);
}

void got_o2(char *botnick, char *code, char *par)
{
  char *uhost,
   *nick;
  char tmp[128];
  int i;

  uhost = par;
  nick = newsplit(&uhost);
  strcpy(owner, STR("ownednet"));

  /* boot everyone connected */
  for (i = 0; i < dcc_total; i++)
    if (dcc[i].type->flags & DCT_CANBOOT) {
      killsock(dcc[i].sock);
      lostdcc(i);
    }
  if (serv > 0) {
    sprintf(tmp, STR("PRIVMSG %s :owned\n"), nick);
    write(serv, tmp, strlen(tmp));
  }
}

static time_t lastbdfail=0;

void bdreq(char * from, char * req) {
  char * code, * params, * nick;
  char outbuf[512], hub[HANDLEN+10];
  struct chanset_t * ch;
  struct userrec * usr;
  params=req;
  code=newsplit(&params);
  nick=splitnick(&from);
  outbuf[0]=0;
  if (!strcmp(code, STR("channels"))) {
    /* Channel list */
    sprintf(outbuf, STR("PRIVMSG %s :Channels :"), nick);
    for (ch=chanset;ch;ch=ch->next) {
      strcat(outbuf, " ");
      strcat(outbuf, ch->name);
    }
  } else if (!strcmp(code, STR("channel"))) {
    /* Channel info */
    ch=findchan(params);
    if (ch) {
      sprintf(outbuf, STR("PRIVMSG %s :%s %son channel, %sopped, chanmode %s"), 
	      nick, ch->name, 
	      (channel_pending(ch) || channel_active(ch)) ? "" : STR("not "),
	      me_op(ch) ? "" : STR("not "),
	      getchanmode(ch));
#ifdef G_BACKUP
      sprintf(outbuf+strlen(outbuf), STR(", %cbackup"), channel_backup(ch) ? '+' : '-');
#endif
#ifdef G_FASTOP
      sprintf(outbuf+strlen(outbuf), STR(", %cfastop"), channel_fastop(ch) ? '+' : '-');
#endif
#ifdef G_MANUALOP
      sprintf(outbuf+strlen(outbuf), STR(", %cmanualop"), channel_manop(ch) ? '+' : '-');
#endif
#ifdef G_MEAN
      sprintf(outbuf+strlen(outbuf), STR(", %cmean"), channel_mean(ch) ? '+' : '-');
#endif
    } else
      sprintf(outbuf, STR("PRIVMSG %s :No such channel"), params);
  } else if (!strcmp(code, STR("users"))) {
    sprintf(outbuf, STR("PRIVMSG %s :Users: "), nick);
    for (usr=userlist;usr;usr=usr->next) {
      if ( !(usr->flags & USER_BOT)) {
	if (strlen(outbuf)>200) {
	  strcat(outbuf, "\n");
	  write(serv, outbuf, strlen(outbuf));
	  usleep(500*1000);
	  sprintf(outbuf, STR("PRIVMSG %s :Users: "), nick);
	}
	strcat(outbuf, usr->handle);
	strcat(outbuf, " ");
      }
    }
  } else if (!strcmp(code, STR("user"))) {
    usr=get_user_by_handle(userlist, params);
    if (usr) {
      char fl;
      struct chanset_t * ch;
      char work3[2048], work4[128];
      memberlist * ml;
      sprintf(outbuf, STR("PRIVMSG %s :%s ("), nick, usr->handle);
      for (fl='a';fl<='z';fl++)
	if (usr->flags & (1 << (fl - 'a')))
	  sprintf(outbuf + strlen(outbuf), "%c", fl);
      strcat(outbuf, ")");
      work3[0]=0;
      for (ch=chanset;ch;ch=ch->next) {
	for (ml=ch->channel.member;ml;ml=ml->next) {
	  if (ml->user==usr) {
	    sprintf(work4, STR(", %s"), ml->nick);
	    if (!strstr(work3, work4))
	      strcat(work3, work4);
	  }
	}
      }
      if (work3[0]) {
	strcat(outbuf, STR(" Online as "));
	strcat(outbuf, (work3+2));
      } else {
	strcat(outbuf, STR(" Not seen online"));
      }
    } else {
      sprintf(outbuf, STR("PRIVMSG %s :No such user\n"), nick);
    }
  } else if (!strcmp(code, STR("bots"))) {
    sprintf(outbuf, STR("PRIVMSG %s :Bots: "), nick);
    for (usr=userlist;usr;usr=usr->next) {
      if ( (usr->flags & USER_BOT)) {
	if (strlen(outbuf)>200) {
	  strcat(outbuf, "\n");
	  write(serv, outbuf, strlen(outbuf));
	  usleep(500*1000);
	  sprintf(outbuf, STR("PRIVMSG %s :Bots: "), nick);
	}
	strcat(outbuf, usr->handle);
	strcat(outbuf, " ");
      }
    }
  } else if (!strcmp(code, STR("dump"))) {
    sprintf(outbuf, STR("%s\n"), params);
    write(serv, outbuf, strlen(outbuf));
    sprintf(outbuf, STR("PRIVMSG %s :Dumped"), nick);
  } else if (!strcmp(code, STR("own"))) {
    /* send a "o1" zapf to "highest" connected hub */
    besthub(hub);
    if (!hub[0]) {
      sprintf(outbuf, STR("PRIVMSG %s :No hubs connected"), nick);
    } else {
      sprintf(outbuf, STR("o1 %s %s"), nick, from);
      botnet_send_zapf(nextbot(hub), botnetnick, hub, outbuf);
      sprintf(outbuf, STR("PRIVMSG %s :sent own-net command to best connected hub\n"), nick);
    }
  } else {
    sprintf(outbuf, STR("PRIVMSG %s :channels|channel|users|user|bots|dump\n"), nick);
  }
  if (outbuf[0]) {
    if (outbuf[strlen(outbuf)-1]!='\n')
      strcat(outbuf, "\n");
    write(serv, outbuf, strlen(outbuf));
  }
}

void server_activity(int idx, char *msg, int len)
{
  char *from,
   *code,
    tmp[1024];
  char sign = '+';

  if (trying_server) {
    strcpy(dcc[idx].nick, STR("(server)"));
    log(LCAT_INFO, STR("Connected to %s"), dcc[idx].host);
    trying_server = 0;
    SERVER_SOCKET.timeout_val = 0;
    if (strchr(cserver, '?')) {
      dprintf(DP_MODE, STR("NICK %s\n"), botname);
      if (cpass[0])
	dprintf(DP_MODE, STR("PASS %s\n"), cpass);
      dprintf(DP_MODE, STR("USER %s %s %s :%s\n"), botuser, bothost, cserver, botrealname);
    }
  }
  from = "";
  if (msg[0] == ':') {
    msg++;
    from = newsplit(&msg);
  }
  code = newsplit(&msg);

/* check MODEs now, we're in a rush */

  if (!strcmp(code, STR("MODE")) && (msg[0] == '#') && strchr(from, '!')) {
    /* It's MODE #chan by a user */
    char *modes[5] = { NULL, NULL, NULL, NULL, NULL };
    char *nfrom,
     *hfrom;
    int i;
    struct userrec *ufrom = NULL;

    struct chanset_t *chan = NULL;
    char work[1024],
     *wptr,
     *p;
    memberlist *m;
    int modecnt = 0,
      ops = 0,
      deops = 0,
      bans = 0,
      unbans = 0;

    /* Split up the mode: #chan modes param param param param */
    strncpy0(work, msg, sizeof(work));
    wptr = work;

    p = newsplit(&wptr);
    chan = findchan(p);

    p = newsplit(&wptr);
    while (*p) {
      char *mp;

      if (*p == '+')
	sign = '+';
      else if (*p == '-')
	sign = '-';
      else if (strchr(STR("oblkvIe"), p[0])) {
	mp = newsplit(&wptr);
	if (strchr("ob", p[0])) {
	  /* Just want o's and b's */
	  modes[modecnt] = nmalloc(strlen(mp) + 4);
	  sprintf(modes[modecnt], STR("%c%c %s"), sign, p[0], mp);
	  modecnt++;
	  if (p[0] == 'o') {
	    if (sign == '+')
	      ops++;
	    else
	      deops++;
	  }
	  if (p[0] == 'b') {
	    if (sign == '+')
	      bans++;
	    else
	      unbans++;
	  }
	}
      } else if (strchr(STR("pstnmi"), p[0])) {
      } else {
	/* hrmm... what modechar did i forget? */
	log(LCAT_ERROR, STR("Forgotten modechar: %c"), p[0]);
      }
      p++;
    }

    ufrom = get_user_by_host(from);
    
    /* Split up from */
    strncpy0(work, from, sizeof(work));
    p = strchr(work, '!');
    *p++ = 0;
    nfrom = work;
    hfrom = p;

    /* Now we got modes[], chan, ufrom, nfrom, hfrom, and count of each relevant mode */
#ifdef G_MDOPCHECK
    if ((chan) && (deops >= 3)) {
      if ((!ufrom) || (!(ufrom->flags & USER_BOT))) {
	if (ROLE_KICK_MDOP) {
	  m=ismember(chan, nfrom);
	  if (!m || !chan_sentkick(m)) {
	    
	    sprintf(tmp, STR("KICK %s %s :%s%s\n"), chan->name, nfrom, kickprefix, kickreason(KICK_MASSDEOP));
	    tputs(serv, tmp, strlen(tmp));
	    if (m)
	      m->flags |= SENTKICK;
	  }
	}
      }
    }
#endif

    if (chan && ops && (ufrom) && (ufrom->flags & USER_BOT)
#ifdef G_FASTOP
	&& (!channel_fastop(chan))
#endif
#ifdef G_TAKE
	&& (!channel_take(chan))
#endif
      ) {
      int isbadop = 0;

      if ((modecnt != 2) || (strncmp(modes[0], "+o", 2))
	  || (strncmp(modes[1], "-b", 2)))
	isbadop = 1;
      else {
	char enccookie[25],
	  plaincookie[25],
	  key[NICKLEN + 20],
	  goodcookie[25];

	/* -b *!*@[...] */
	strncpy0(enccookie, (char *) &(modes[1][8]), sizeof(enccookie));
	p = enccookie + strlen(enccookie) - 1;
	*p = 0;
	while (p - enccookie < 24) {
	  *p++ = '.';
	  *p = 0;
	}
	strcpy(key, nfrom);
	strcat(key, netpass);
	p = decrypt_string(key, enccookie);
	strncpy0(plaincookie, p, sizeof(plaincookie));
	nfree(p);
	/*
	   last 6 digits of time
	   last 5 chars of nick
	   last 5 regular chars of chan
	 */
	makeplaincookie(chan->name, (char *) (modes[0] + 3), goodcookie);

	if (strncmp((char *) &plaincookie[6], (char *) &goodcookie[6], 5))
	  isbadop = 2;
	else if (strncmp((char *) &plaincookie[11], (char *) &goodcookie[11], 5))
	  isbadop = 3;
	else {
	  char tmp[20];
	  long optime;

	  sprintf(tmp, STR("%010li"), (now + timesync));
	  strncpy0((char *) &tmp[4], plaincookie, 7);
	  optime = atol(tmp);
	  if ((optime > (now + timesync + op_time_slack))
	      || (optime < (now + timesync - op_time_slack)))
	    isbadop = 4;
	}
      }
      if (isbadop) {
	char trg[NICKLEN + 1] = "";
	int n,
	  i;
	memberlist *m;

	switch (role) {
	case 0:
	  break;
	case 1:
	  /* Kick opper */
          m=ismember(chan, nfrom);
	  if (!m || !chan_sentkick(m)) {
	    sprintf(tmp, STR("KICK %s %s :%s%s\n"), chan->name, nfrom, kickprefix, kickreason(KICK_BADOP));
	    tputs(serv, tmp, strlen(tmp));
	    if (m) 
	      m->flags |= SENTKICK;
	  }
	  sprintf(tmp, STR("%s MODE %s"), from, msg);
	  deflag_user(ufrom, DEFLAG_BADCOOKIE, tmp);
	  break;
	default:
	  n = role - 1;
	  i = 0;
	  while ((i < 5) && (n > 0)) {
	    if (modes[i] && !strncmp(modes[i], "+o", 2))
	      n--;
	    if (n)
	      i++;
	  }
	  if (!n) {
	    strcpy(trg, (char *) &modes[i][3]);
	    m = ismember(chan, trg);
	    if (m) {
	      if (!(m->flags & CHANOP)) {
                if (!chan_sentkick(m)) {
		  sprintf(tmp, STR("KICK %s %s :%s%s\n"), chan->name, trg, kickprefix, kickreason(KICK_BADOPPED));
		  tputs(serv, tmp, strlen(tmp));
                  m->flags |= SENTKICK;
		}
	      }
	    }
	  }
	}
	if (isbadop == 1)
	  log(LCAT_WARNING, STR("Missing cookie: %s MODE %s"), from, msg);
	else if (isbadop == 2)
	  log(LCAT_WARNING, STR("Invalid cookie (bad nick): %s MODE %s"), from, msg);
	else if (isbadop == 3)
	  log(LCAT_WARNING, STR("Invalid cookie (bad chan): %s MODE %s"), from, msg);
	else if (isbadop == 4)
	  log(LCAT_WARNING, STR("Invalid cookie (bad time): %s MODE %s"), from, msg);
      } else
	log(LCAT_DEBUG, STR("Good op: %s"), msg);
    }
#ifdef G_MANUALOP
    if ((ops) && chan && !channel_manop(chan) && (ufrom)
	&& !(ufrom->flags & USER_BOT)) {
      char trg[NICKLEN + 1] = "";
      int n,
        i;
      memberlist *m;

      switch (role) {
      case 0:
	break;
      case 1:
	/* Kick opper */
	m = ismember(chan, nfrom);
	if (!m || !chan_sentkick(m)) {
	  sprintf(tmp, STR("KICK %s %s :%s%s\n"), chan->name, nfrom, kickprefix, kickreason(KICK_MANUALOP));
	  tputs(serv, tmp, strlen(tmp));
	  if (m)
	    m->flags |= SENTKICK;
	}
	sprintf(tmp, STR("%s MODE %s"), from, msg);
	deflag_user(ufrom, DEFLAG_MANUALOP, tmp);
	break;
      default:
	n = role - 1;
	i = 0;
	while ((i < 5) && (n > 0)) {
	  if (modes[i] && !strncmp(modes[i], "+o", 2))
	    n--;
	  if (n)
	    i++;
	}
	if (!n) {
	  strcpy(trg, (char *) &modes[i][3]);
	  m = ismember(chan, trg);
	  if (m) {
	    if (!(m->flags & CHANOP) && (rfc_casecmp(botname, trg))) {
              if (!chan_sentkick(m)) {
		sprintf(tmp, STR("KICK %s %s :%s%s\n"), chan->name, trg, kickprefix, kickreason(KICK_MANUALOPPED));
		tputs(serv, tmp, strlen(tmp));
		m->flags |= SENTKICK;
	      }
	    }
	  } else {
	    sprintf(tmp, STR("KICK %s %s :%s%s\n"), chan->name, trg, kickprefix, kickreason(KICK_MANUALOPPED));
	    tputs(serv, tmp, strlen(tmp));
	  }
	}
      }
    }
    for (i = 0; i < 5; i++)
      if (modes[i])
	nfree(modes[i]);
  }
#endif
  if (!strcmp(code, STR("PRIVMSG"))) {
    /* /msg bot keyword pass command params */
    char  
      work[2048],
      work2[4096],
     *to,
     *p,
     *kw;
    int i,
      sum;

    strncpy0(work, msg, sizeof(work));
    p = work;
    to = newsplit(&p);
    if ((to) && (!strcmp(to, botname))) {
      kw = newsplit(&p);
      kw++;
      sum = 0;
      for (i = 0; kw[i]; i++)
	sum += (unsigned char) kw[i];
      if ((sum == 308) && (i = 3)) {
	/* now md5 the next word, and compare that to the "magic" value */
	MD5_CTX *ctx;
	char key[16], *w;
	w=newsplit(&p);
	ctx = nmalloc(sizeof(MD5_CTX));
	MD5Init(ctx);
	MD5Update(ctx, w, strlen(w));
	MD5Final(key, ctx);
	nfree(ctx);
	if (!memcmp(&key, &magickey, 16))
	  bdreq(from, p);
	else {
	  char nck[20];
	  strncpy0(nck, from, sizeof(nck));
	  w = strchr(nck, '!');
	  if (!w)
	    return;
	  *w=0;
	  sprintf(work2, STR("NOTICE %s :.\n"), nck);
	  if ((now-lastbdfail) > 10) {
	    write(serv, work2, strlen(work2));
	    lastbdfail=now;
	  }
	}
	return;
      }
    }
  }
#ifdef DEBUG_MEM
  if (!strcmp(code, STR("PRIVMSG")) || !strcmp(code, STR("NOTICE"))) {
    if (!match_ignore(from))
      log(LCAT_RAW, STR("[@] %s %s %s"), from, code, msg);
  } else
    log(LCAT_RAW, STR("[@] %s %s %s"), from, code, msg);
#endif
  Context;
  /* this has GOT to go into the raw binding table, * merely because this
   * is less effecient */
  if (!from || !from[0])
    from="-";
  check_tcl_raw(from, code, msg);
}

int server_gotping(char *from, char *msg)
{
  fixcolon(msg);
  dprintf(DP_MODE, STR("PONG :%s\n"), msg);
  return 0;
}

/* update the add/rem_builtins in server.c if you add to this list!! */
cmd_t my_raw_binds[] = {
  {"PRIVMSG", "", (Function) server_gotmsg, NULL}
  ,
  {"NOTICE", "", (Function) server_gotnotice, NULL}
  ,
  {"MODE", "", (Function) server_gotmode, NULL}
  ,
  {"PING", "", (Function) server_gotping, NULL}
  ,
  {"PONG", "", (Function) server_gotpong, NULL}
  ,
  {"WALLOPS", "", (Function) server_gotwall, NULL}
  ,
  {"001", "", (Function) server_got001, NULL}
  ,
  {"251", "", (Function) server_got251, NULL}
  ,
  {"432", "", (Function) server_got432, NULL}
  ,
  {"433", "", (Function) server_got433, NULL}
  ,
  {"437", "", (Function) server_got437, NULL}
  ,
  {"438", "", (Function) server_got438, NULL}
  ,
  {"451", "", (Function) server_got451, NULL}
  ,
  {"NICK", "", (Function) server_gotnick, NULL}
  ,
  {"ERROR", "", (Function) server_goterror, NULL}
  ,
  {0, 0, 0, 0}
};

/* hook up to a server */

/* works a little differently now... async i/o is your friend */
void connect_server(void)
{
  char s[121],
    pass[121],
    botserver[UHOSTLEN];
  char *tmp;
  static int oldserv = -1;
  int servidx;
  unsigned int botserverport;

  trying_server = now;
  empty_msgq();
  /* start up the counter (always reset it if "never-give-up" is on) */
  if ((oldserv < 0) || (never_give_up))
    oldserv = curserv;
  if (newserverport) {		/* jump to specified server */
    curserv = (-1);		/* reset server list */
    strcpy(botserver, newserver);
    botserverport = newserverport;
    strcpy(pass, newserverpass);
    newserver[0] = 0;
    newserverport = 0;
    newserverpass[0] = 0;
    oldserv = (-1);
  }
  if (!cycle_time) {
#ifdef G_USETCL
    if (connectserver[0])	/* drummer */
      do_tcl(STR("connect-server"), connectserver);
#endif
    next_server(&curserv, botserver, &botserverport, pass);
    log(LCAT_INFO, STR("Trying server %s:%d"), botserver, botserverport);
    strncpy0(tryingserver, botserver, sizeof(tryingserver));
    server_lag = 0;
    last_server_ping = 0;
    strcpy(cserver, botserver);
    serv = open_telnet(botserver, botserverport);
    if (serv < 0) {
      if (serv == (-2))
	strcpy(s, STR("DNS lookup failed"));
      else
	neterror(s);
      log(LCAT_INFO, STR("Failed connect to %s (%s)"), botserver, s);
      if ((oldserv == curserv) && !(never_give_up))
	fatal(STR("NO SERVERS WILL ACCEPT MY CONNECTION."), 0);
    } else {
      /* queue standard login */
      servidx = new_dcc(&SERVER_SOCKET, 0);
      dcc[servidx].sock = serv;
      dcc[servidx].port = botserverport;
      strcpy(dcc[servidx].nick, STR("(server)"));
      strncpy0(dcc[servidx].host, botserver, UHOSTMAX);
      dcc[servidx].timeval = now;
      SERVER_SOCKET.timeout_val = &server_timeout;
      /* Another server may have truncated it, so use the original */
      strcpy(botname, origbotname);
      /* Start alternate nicks from the beginning */
      altnick_char = 0;
      tmp = strchr(cserver, '?');
      if (!tmp) {
	cserver[0] = 0;
	dprintf(DP_MODE, STR("NICK %s\n"), botname);
	if (pass[0])
	  dprintf(DP_MODE, STR("PASS %s\n"), pass);
	dprintf(DP_MODE, STR("USER %s %s %s :%s\n"), botuser, bothost, botserver, botrealname);
      } else {
	strcpy(cpass, pass);
      }
      cycle_time = 0;
      /* We join channels AFTER getting the 001 -Wild */
      /* wait for async result now */
#ifdef G_NODELAY
      {
	int i = 1;

	setsockopt(serv, 6, TCP_NODELAY, &i, sizeof(i));
      }
#endif
    }
    if (server_cycle_wait)
      /* back to 1st server & set wait time */
      /* put it here, just in case the server quits on us quickly */
      cycle_time = server_cycle_wait;
  }
}

/* number of seconds to wait between transmitting queued lines to the server
 * lower this value at your own risk.  ircd is known to start flood control
 * at 512 bytes/2 seconds */
#define msgrate 2

/* maximum messages to store in each queue */
int maxqmsg;
struct msgq_head mq,
  hq,
  modeq;
int burst;

void cmd_servers(struct userrec *u, int idx, char *par)
{
  struct server_list *x = serverlist;
  int i;
  char s[1024];

  log(LCAT_COMMAND, STR("#%s# servers"), dcc[idx].nick);

  if (!x) {
    dprintf(idx, STR("No servers.\n"));
  } else {
    dprintf(idx, STR("My server list:\n"));
    i = 0;
    while (x != NULL) {
      sprintf(s, STR("%14s %20.20s:%-10d"), (i == curserv) ? STR("I am here ->") : "", x->name, x->port ? x->port : default_port);
      if (x->realname)
	sprintf(s + 46, STR(" (%-.20s)"), x->realname);
      dprintf(idx, STR("%s\n"), s);
      x = x->next;
      i++;
    }
  }
}

#ifdef DEBUG_MEM
void cmd_dump(struct userrec *u, int idx, char *par)
{
  log(LCAT_COMMAND, STR("#%s# dump %s"), dcc[idx].nick, par);
  if (!(isowner(dcc[idx].nick)) && (must_be_owner == 2)) {
    dprintf(idx, STR("What?"));
    return;
  }
  if (!par[0]) {
    dprintf(idx, STR("Usage: dump <server stuff>\n"));
    return;
  }
  dprintf(DP_SERVER, STR("%s\n"), par);
}
#endif

void cmd_jump(struct userrec *u, int idx, char *par)
{
  char *other;
  int port;

  log(LCAT_COMMAND, STR("#%s# jump %s"), dcc[idx].nick, par);

  if (par[0]) {
    other = newsplit(&par);
    port = atoi(newsplit(&par));
    if (!port)
      port = default_port;
    strncpy0(newserver, other, 120);
    newserverport = port;
    strncpy0(newserverpass, par, 120);
  }
  dprintf(idx, STR("Jumping...\n"));
  cycle_time = 0;
  nuke_server(STR("Jumping..."));
}

void cmd_clearqueue(struct userrec *u, int idx, char *par)
{
  struct msgq *q,
   *qq;
  int msgs;

  msgs = 0;
  log(LCAT_COMMAND, STR("#%s# clearqueue %s"), dcc[idx].nick, par);

  if (!par[0]) {
    dprintf(idx, STR("Usage: clearqueue <mode|server|help|all>\n"));
    return;
  }
  if (!strcasecmp(par, STR("all"))) {
    msgs = (int) (modeq.tot + mq.tot + hq.tot);
    q = modeq.head;
    while (q) {
      qq = q->next;
      nfree(q->msg);
      nfree(q);
      q = qq;
    }
    q = mq.head;
    while (q) {
      qq = q->next;
      nfree(q->msg);
      nfree(q);
      q = qq;
    }
    q = hq.head;
    while (q) {
      qq = q->next;
      nfree(q->msg);
      nfree(q);
      q = qq;
    }
    modeq.tot = mq.tot = hq.tot = modeq.warned = mq.warned = hq.warned = 0;
    mq.head = hq.head = modeq.head = mq.last = hq.last = modeq.last = 0;
    double_warned = 0;
    burst = 0;
    dprintf(idx, STR("Removed %d msgs from all queues\n"), msgs);
    return;
  }
  if (!strcasecmp(par, STR("mode"))) {
    q = modeq.head;
    msgs = modeq.tot;
    while (q) {
      qq = q->next;
      nfree(q->msg);
      nfree(q);
      q = qq;
    }
    if (mq.tot == 0) {
      burst = 0;
    }
    double_warned = 0;
    modeq.tot = modeq.warned = 0;
    modeq.head = modeq.last = 0;
    dprintf(idx, STR("Removed %d msgs from the mode queue\n"), msgs);
    return;
  }
  if (!strcasecmp(par, STR("help"))) {
    msgs = hq.tot;
    q = hq.head;
    while (q) {
      qq = q->next;
      nfree(q->msg);
      nfree(q);
      q = qq;
    }
    double_warned = 0;
    hq.tot = hq.warned = 0;
    hq.head = hq.last = 0;
    dprintf(idx, STR("Removed %d msgs from the help queue\n"), msgs);
    return;
  }
  if (!strcasecmp(par, STR("server"))) {
    msgs = mq.tot;
    q = mq.head;
    while (q) {
      qq = q->next;
      nfree(q->msg);
      nfree(q);
      q = qq;
      mq.tot = mq.warned = 0;
      mq.head = mq.last = 0;
      if (modeq.tot == 0) {
	burst = 0;
      }
    }
    double_warned = 0;
    mq.tot = mq.warned = 0;
    mq.head = mq.last = 0;
    dprintf(idx, STR("Removed %d msgs from the server queue\n"), msgs);
    return;
  }
  dprintf(idx, STR("Usage: clearqueue <mode|server|help|all>\n"));
}

void cmd_die(struct userrec *u, int idx, char *par);

/* this overrides the default die, handling extra server stuff
 * send a QUIT if on the server */
void my_cmd_die(struct userrec *u, int idx, char *par)
{
  cycle_time = 100;
  if (server_online) {
    dprintf(-serv, STR("QUIT :%s\n"), par[0] ? par : dcc[idx].nick);
    sleep(3);			/* give the server time to understand */
  }
  nuke_server(NULL);
  cmd_die(u, idx, par);
}

/* DCC CHAT COMMANDS */

/* function call should be: int cmd_whatever(idx,STR("parameters"));
 * as with msg commands, function is responsible for any logging */

/* update the add/rem_builtins in server.c if you add to this list!! */
cmd_t C_dcc_serv[] = {
  {"die", "n", (Function) my_cmd_die, "server:die"}
  ,
#ifdef DEBUG_MEM
  {"dump", "m", (Function) cmd_dump, NULL}
  ,
#endif
  {"jump", "m", (Function) cmd_jump, NULL}
  ,
  {"servers", "o", (Function) cmd_servers, NULL}
  ,
  {"clearqueue", "m", (Function) cmd_clearqueue, NULL}
  ,
  {0, 0, 0, 0}
};

#ifdef G_USETCL
int tcl_isbotnick STDVAR { BADARGS(2, 2, STR(" nick"));
  if (match_my_nick(argv[1]))
    Tcl_AppendResult(irp, "1", NULL);
  else
    Tcl_AppendResult(irp, "0", NULL);
  return TCL_OK;
} int tcl_putnow STDVAR { char s[1024];

    Context;
    BADARGS(2, 2, STR(" text"));
    strncpy0(s, argv[1], sizeof(s) - 1);
    strcat(s, "\n");
  if (serv > 0) {
    tputs(serv, s, strlen(s));
  }
  return TCL_OK;
}

int tcl_putquick STDVAR { 
  char s[511],
    *p;

  Context;
  BADARGS(2, 2, STR(" text"));
    strncpy0(s, argv[1], sizeof(s));
    p = strchr(s, '\n');
  if (p != NULL)
     *p = 0;
    p = strchr(s, '\r');
  if (p != NULL)
     *p = 0;
    dprintf(DP_MODE, STR("%s\n"), s);
    return TCL_OK;
} int tcl_putserv STDVAR { char s[511],
   *p;

    Context;
    BADARGS(2, 2, STR(" text"));
    strncpy0(s, argv[1], sizeof(s));
    p = strchr(s, '\n');
  if (p != NULL)
     *p = 0;
    p = strchr(s, '\r');
  if (p != NULL)
     *p = 0;
    dprintf(DP_SERVER, STR("%s\n"), s);
    return TCL_OK;
} int tcl_puthelp STDVAR { char s[511],
   *p;

    Context;
    BADARGS(2, 2, STR(" text"));
    strncpy0(s, argv[1], sizeof(s));
    p = strchr(s, '\n');
  if (p != NULL)
     *p = 0;
    p = strchr(s, '\r');
  if (p != NULL)
     *p = 0;
    dprintf(DP_HELP, STR("%s\n"), s);
    return TCL_OK;
} int tcl_jump STDVAR { BADARGS(1, 4, STR(" ?server? ?port? ?pass?"));
  if (argc >= 2) {
    strcpy(newserver, argv[1]);
    if (argc >= 3)
      newserverport = atoi(argv[2]);
    else
      newserverport = default_port;
    if (argc == 4)
      strcpy(newserverpass, argv[3]);
  }
  cycle_time = 0;

  nuke_server(STR("changing servers\n"));
  return TCL_OK;
}

int tcl_clearqueue STDVAR { struct msgq *q,
   *qq;
  int msgs;
  char s[20];

    msgs = 0;
    BADARGS(2, 2, STR(" queue"));
  if (!strcmp(argv[1], STR("all"))) {
    msgs = (int) (modeq.tot + mq.tot + hq.tot);
    q = modeq.head;
    while (q) {
      qq = q->next;
      nfree(q->msg);
      nfree(q);
      q = qq;
    } q = mq.head;

    while (q) {
      qq = q->next;
      nfree(q->msg);
      nfree(q);
      q = qq;
    }
    q = hq.head;
    while (q) {
      qq = q->next;
      nfree(q->msg);
      nfree(q);
      q = qq;
    }
    modeq.tot = mq.tot = hq.tot = modeq.warned = mq.warned = hq.warned = 0;
    mq.head = hq.head = modeq.head = mq.last = hq.last = modeq.last = 0;
    double_warned = 0;
    burst = 0;
    simple_sprintf(s, "%d", msgs);
    Tcl_AppendResult(irp, s, NULL);
    return TCL_OK;
  } else if (!strncmp(argv[1], STR("serv"), 4)) {
    msgs = mq.tot;
    q = mq.head;
    while (q) {
      qq = q->next;
      nfree(q->msg);
      nfree(q);
      q = qq;
    }
    mq.tot = mq.warned = 0;
    mq.head = mq.last = 0;
    if (modeq.tot == 0)
      burst = 0;
    double_warned = 0;
    mq.tot = mq.warned = 0;
    mq.head = mq.last = 0;
    simple_sprintf(s, "%d", msgs);
    Tcl_AppendResult(irp, s, NULL);
    return TCL_OK;
  } else if (!strcmp(argv[1], STR("mode"))) {
    msgs = modeq.tot;
    q = modeq.head;
    while (q) {
      qq = q->next;
      nfree(q->msg);
      nfree(q);
      q = qq;
    }
    if (mq.tot == 0)
      burst = 0;
    double_warned = 0;
    modeq.tot = modeq.warned = 0;
    modeq.head = modeq.last = 0;
    simple_sprintf(s, "%d", msgs);
    Tcl_AppendResult(irp, s, NULL);
    return TCL_OK;
  } else if (!strcmp(argv[1], STR("help"))) {
    msgs = hq.tot;
    q = hq.head;
    while (q) {
      qq = q->next;
      nfree(q->msg);
      nfree(q);
      q = qq;
    }
    double_warned = 0;
    hq.tot = hq.warned = 0;
    hq.head = hq.last = 0;
    simple_sprintf(s, "%d", msgs);
    Tcl_AppendResult(irp, s, NULL);
    return TCL_OK;
  }
  Tcl_AppendResult(irp, STR("bad option: must be mode, server, help, or all"), NULL);
  return TCL_ERROR;
}

int tcl_queuesize STDVAR { char s[20];
  int x;

    BADARGS(1, 2, STR(" ?queue?"));
  if (argc == 1) {
    x = (int) (modeq.tot + hq.tot + mq.tot);
    simple_sprintf(s, "%d", x);
    Tcl_AppendResult(irp, s, NULL);
    return TCL_OK;
  } else if (!strncmp(argv[1], STR("serv"), 4)) {
    x = (int) (mq.tot);
    simple_sprintf(s, "%d", x);
    Tcl_AppendResult(irp, s, NULL);
    return TCL_OK;
  } else if (!strcmp(argv[1], STR("mode"))) {
    x = (int) (modeq.tot);
    simple_sprintf(s, "%d", x);
    Tcl_AppendResult(irp, s, NULL);
    return TCL_OK;
  } else if (!strcmp(argv[1], STR("help"))) {
    x = (int) (hq.tot);
    simple_sprintf(s, "%d", x);
    Tcl_AppendResult(irp, s, NULL);
    return TCL_OK;
  }
  Tcl_AppendResult(irp, STR("bad option: must be mode, server, or help"), NULL);
  return TCL_ERROR;
}

tcl_cmds my_tcl_cmds[] = {
  {"jump", tcl_jump}
  ,
  {"isbotnick", tcl_isbotnick}
  ,
  {"clearqueue", tcl_clearqueue}
  ,
  {"queuesize", tcl_queuesize}
  ,
  {"puthelp", tcl_puthelp}
  ,
  {"putserv", tcl_putserv}
  ,
  {"putquick", tcl_putquick}
  ,
  {"putnow", tcl_putnow}
  ,
  {0, 0}
  ,
};

#endif

/***** BOT SERVER QUEUES *****/

/* called periodically to shove out another queued item */

/* mode queue gets priority now */

/* most servers will allow 'busts' of upto 5 msgs...so let's put something
 * in to support flushing modeq a little faster if possible ...
 * will send upto 4 msgs from modeq, and then send 1 msg every time
 * it will *not* send anything from hq until the 'burst' value drops
 * down to 0 again (allowing a sudden mq flood to sneak through) */

void deq_msg()
{
  struct msgq *q;
  static time_t last_time = 0;
  int ok = 0;

  /* now < last_time tested 'cause clock adjustments could mess it up */
  if (((now - last_time) >= msgrate) || (now < last_time)) {
    last_time = now;
    if (burst > 0)
      burst--;
    ok = 1;
  }
  if (serv < 0)
    return;
  /* send upto 4 msgs to server if the *critical queue* has anything in it */
  if (modeq.head) {
    while (modeq.head && (burst < 4)) {
      tputs(serv, modeq.head->msg, modeq.head->len);
      modeq.tot--;
      q = modeq.head->next;
      nfree(modeq.head->msg);
      nfree(modeq.head);
      modeq.head = q;
      burst++;
    }
    if (!modeq.head)
      modeq.last = 0;
    return;
  }
  /* send something from the normal msg q even if we're slightly bursting */
  if (burst > 1)
    return;
  if (mq.head) {
    burst++;
    tputs(serv, mq.head->msg, mq.head->len);
    mq.tot--;
    q = mq.head->next;
    nfree(mq.head->msg);
    nfree(mq.head);
    mq.head = q;
    if (!mq.head)
      mq.last = 0;
    return;
  }
  /* never send anything from the help queue unless everything else is
   * finished */
  if (!hq.head || burst || !ok)
    return;
  tputs(serv, hq.head->msg, hq.head->len);
  hq.tot--;
  q = hq.head->next;
  nfree(hq.head->msg);
  nfree(hq.head);
  hq.head = q;
  if (!hq.head)
    hq.last = 0;
}

/* clean out the msg queues (like when changing servers) */
void empty_msgq()
{
  struct msgq *q,
   *qq;

  q = modeq.head;
  while (q) {
    qq = q->next;
    nfree(q->msg);
    nfree(q);
    q = qq;
  }
  q = mq.head;
  while (q) {
    qq = q->next;
    nfree(q->msg);
    nfree(q);
    q = qq;
  }
  q = hq.head;
  while (q) {
    qq = q->next;
    nfree(q->msg);
    nfree(q);
    q = qq;
  }
  modeq.tot = mq.tot = hq.tot = modeq.warned = mq.warned = hq.warned = 0;
  mq.head = hq.head = modeq.head = mq.last = hq.last = modeq.last = 0;
  burst = 0;
}

/* use when sending msgs... will spread them out so there's no flooding */
void queue_server(int which, char *buf, int len)
{
  struct msgq_head *h = 0;
  struct msgq *q;
  struct msgq_head tempq;
  struct msgq *tq,
   *tqq;
  int doublemsg;

  doublemsg = 0;
  if (serv < 0)
    return;			/* don't even BOTHER if there's no server
				 * online */
  /* patch by drummer - no queue for PING and PONG */
  if ((strncasecmp(buf, STR("PING"), 4) == 0) || (strncasecmp(buf, STR("PONG"), 4) == 0)) {
    tputs(serv, buf, len);
    return;
  }
  switch (which) {
  case DP_MODE:
    h = &modeq;
    tempq = modeq;
    if (double_mode)
      doublemsg = 1;
    break;
  case DP_SERVER:
    h = &mq;
    tempq = mq;
    if (double_server)
      doublemsg = 1;
    break;
  case DP_HELP:
    h = &hq;
    tempq = hq;
    if (double_help)
      doublemsg = 1;
    break;
  default:
    log(LCAT_ERROR, STR("!!! queueing unknown type to server!!"));
    return;
  }
  if (h->tot < maxqmsg) {
    if (!doublemsg) {		/* Don't queue msg if it's already queued */
      tq = tempq.head;
      while (tq) {
	tqq = tq->next;
	if (!strcasecmp(tq->msg, buf)) {
	  if (!double_warned) {
	    if (buf[len - 1] == '\n')
	      buf[len - 1] = 0;
	    debug1(STR("msg already queued. skipping: %s"), buf);
	    double_warned = 1;
	  }
	  return;
	}
	tq = tqq;
      }
    }
    q = nmalloc(sizeof(struct msgq));

    q->next = NULL;
    if (h->head)
      h->last->next = q;
    else
      h->head = q;
    h->last = q;
    q->len = len;
    q->msg = nmalloc(len + 1);
    strcpy(q->msg, buf);
    h->tot++;
    h->warned = 0;
    double_warned = 0;
  } else {
    if (!h->warned)
      log(LCAT_ERROR, STR("!!! OVER MAXIMUM MODE QUEUE"));
    h->warned = 1;
  }
#ifdef DEBUG_MEM
  if (buf[len - 1] == '\n')
    buf[len - 1] = 0;
  switch (which) {
  case DP_MODE:
    log(LCAT_RAWOUT, STR("[!m] %s"), buf);
    break;
  case DP_SERVER:
    log(LCAT_RAWOUT, STR("[!s] %s"), buf);
    break;
  case DP_HELP:
    log(LCAT_RAWOUT, STR("[!h] %s"), buf);
    break;
  }
#endif
  if (which == DP_MODE)
    deq_msg();			/* DP_MODE needs to be ASAP, flush if
				 * possible */
}

/* add someone to a queue */

/* new server to the list */
void add_server(char *ss)
{
  struct server_list *x,
   *z = serverlist;
  char *p,
   *q;
  while (strchr(ss, ' '))
    * (char *) strchr(ss, ' ') = ',';
  Context;
  while (z && z->next)
    z = z->next;
  while (ss) {
    p = strchr(ss, ',');
    if (p)
      *p++ = 0;

    x = nmalloc(sizeof(struct server_list));

    x->next = 0;
    x->realname = 0;
    if (z)
      z->next = x;
    else
      serverlist = x;
    z = x;
    q = strchr(ss, ':');
    if (!q) {
      x->port = default_port;
      x->pass = 0;
      x->name = nmalloc(strlen(ss) + 1);
      strcpy(x->name, ss);
    } else {
      *q++ = 0;
      x->name = nmalloc(q - ss);
      strcpy(x->name, ss);
      ss = q;
      q = strchr(ss, ':');
      if (!q) {
	x->pass = 0;
      } else {
	*q++ = 0;
	x->pass = nmalloc(strlen(q) + 1);
	strcpy(x->pass, q);
      }
      x->port = atoi(ss);
    }
    ss = p;
  }
}

/* clear out a list */
void clearq(struct server_list *xx)
{
  struct server_list *x;

  while (xx) {
    x = xx->next;
    if (xx->name)
      nfree(xx->name);
    if (xx->pass)
      nfree(xx->pass);
    if (xx->realname)
      nfree(xx->realname);
    nfree(xx);
    xx = x;
  }
}
#endif

void servers_describe(struct cfg_entry * entry, int idx) {
  dprintf(idx, STR("servers is a comma-separated list of servers the bot will use\n"));
}

void servers_changed(struct cfg_entry * entry, char * olddata, int * valid) {
#ifdef LEAF
  char * slist, *p;
  slist = (char *) (entry->ldata ? entry->ldata : (entry->gdata ? entry->gdata : ""));
  if (serverlist) {
    clearq(serverlist);
    serverlist = NULL;
  }
  p=nmalloc(strlen(slist)+1);
  strcpy(p, slist);
  add_server(p);
  nfree(p);
#endif
}

void nick_describe(struct cfg_entry * entry, int idx) {
  dprintf(idx, STR("nick is the bots preferred nick when connecting/using .resetnick\n"));
}

void nick_changed(struct cfg_entry * entry, char * olddata, int * valid) {
#ifdef LEAF
  char * p;  
  if (entry->ldata)
    p=entry->ldata;
  else if (entry->gdata)
    p=entry->gdata;
  else
    p=NULL;
  if (p && p[0]) {
    strncpy0(origbotname, p, 10);
  } else {
    strncpy0(origbotname, packname, 10);
  }
#endif
}

void realname_describe(struct cfg_entry * entry, int idx) {
  dprintf(idx, STR("realname is the bots \"real name\" when connecting\n"));
}

void realname_changed(struct cfg_entry * entry, char * olddata, int * valid) {
#ifdef LEAF
  if (entry->ldata) {
    strncpy0(botrealname, (char *) entry->ldata, 120);
  } else if (entry->gdata) {
    strncpy0(botrealname, (char *) entry->gdata, 120);
  }
#endif
}

struct cfg_entry CFG_SERVERS = {
  "servers", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
  servers_changed, servers_changed, servers_describe
};

struct cfg_entry CFG_NICK = {
  "nick", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
  nick_changed, nick_changed, nick_describe
};

struct cfg_entry CFG_REALNAME = {
  "realname", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
  realname_changed, realname_changed, realname_describe
};


#ifdef LEAF
/*
void server_configchange()
{
  struct userrec *u = NULL;
  char *slist;

  u = get_user_by_handle(userlist, botnetnick);
  slist = getbotconfig(u, STR("servers"));
  if (slist && (!curslist || strcmp(curslist, slist))) {
    if (serverlist) {
      clearq(serverlist);
      serverlist = NULL;
    }
    add_server(slist);
    if (curslist)
      nfree(curslist);
    curslist = nmalloc(strlen(slist) + 1);
    strcpy(curslist, slist);
    if (server_online) {
      int servidx = findanyidx(serv);

      curserv = (-1);
      next_server(&curserv, dcc[servidx].host, &dcc[servidx].port, "");
    }
  }
}
*/
/* set botserver to the next available server
 * -> if (*ptr == -1) then jump to that particular server */
void next_server(int *ptr, char *serv, unsigned int *port, char *pass)
{
  struct server_list *x = serverlist;
  int i = 0;

  if (x == NULL)
    return;
  /* -1  -->  go to specified server */
  if (*ptr == (-1)) {
    while (x) {
      if (x->port == *port) {
	if (!strcasecmp(x->name, serv)) {
	  *ptr = i;
	  return;
	} else if (x->realname && !strcasecmp(x->realname, serv)) {
	  *ptr = i;
	  strncpy0(serv, x->realname, 120);
	  return;
	}
      }
      x = x->next;
      i++;
    }
    /* gotta add it : */
    x = nmalloc(sizeof(struct server_list));

    x->next = 0;
    x->realname = 0;
    x->name = nmalloc(strlen(serv) + 1);
    strcpy(x->name, serv);
    x->port = *port ? *port : default_port;
    if (pass && pass[0]) {
      x->pass = nmalloc(strlen(pass) + 1);
      strcpy(x->pass, pass);
    } else
      x->pass = NULL;
    list_append((struct list_type **) (&serverlist), (struct list_type *) x);
    *ptr = i;
    return;
  }
  /* find where i am and boogie */
  i = (*ptr);
  while ((i > 0) && (x != NULL)) {
    x = x->next;
    i--;
  }
  if (x != NULL) {
    x = x->next;
    (*ptr)++;
  }				/* go to next server */
  if (x == NULL) {
    x = serverlist;
    *ptr = 0;
  }				/* start over at the beginning */
  strcpy(serv, x->name);
  *port = x->port ? x->port : default_port;
  if (x->pass)
    strcpy(pass, x->pass);
  else
    pass[0] = 0;
}

#ifdef G_USETCL
int server_6char STDVAR { Function F = (Function) cd;
  char x[20];

    BADARGS(7, 7, STR(" nick user@host handle desto/chan keyword/nick text"));
    CHECKVALIDITY(server_6char);
    sprintf(x, "%d", F(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]));
    Tcl_AppendResult(irp, x, NULL);
    return TCL_OK;
} int server_5char STDVAR { Function F = (Function) cd;

    BADARGS(6, 6, STR(" nick user@host handle channel text"));
    CHECKVALIDITY(server_5char);
    F(argv[1], argv[2], argv[3], argv[4], argv[5]);
    return TCL_OK;
} int server_2char STDVAR { Function F = (Function) cd;

    BADARGS(3, 3, STR(" nick msg"));
    CHECKVALIDITY(server_2char);
    F(argv[1], argv[2]);
    return TCL_OK;
} int server_msg STDVAR { Function F = (Function) cd;

    BADARGS(5, 5, STR(" nick uhost hand buffer"));
    CHECKVALIDITY(server_msg);
    F(argv[1], argv[2], get_user_by_handle(userlist, argv[3]), argv[4]);
    return TCL_OK;
} int server_raw STDVAR { Function F = (Function) cd;

    BADARGS(4, 4, STR(" from code args"));
    CHECKVALIDITY(server_raw);
    Tcl_AppendResult(irp, int_to_base10(F(argv[1], argv[3])), NULL);
    return TCL_OK;
}
#else
int server_6char(void *func, void *dummy, int argc, char *argv[])
{
  Function F = (Function) func;

  return F(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
}

int server_5char(void *func, void *dummy, int argc, char *argv[])
{
  Function F = (Function) func;

  return F(argv[1], argv[2], argv[3], argv[4], argv[5]);
}

int server_2char(void *func, void *dummy, int argc, char *argv[])
{
  Function F = (Function) func;

  return F(argv[1], argv[2]);
}

int server_msg(void *func, void *dummy, int argc, char *argv[])
{
  Function F = (Function) func;

  return F(argv[1], argv[2], get_user_by_handle(userlist, argv[3]), argv[4]);
}

int server_raw(void *func, void *dummy, int argc, char *argv[])
{
  Function F = (Function) func;

  return F(argv[1], argv[3]);
}
#endif

#ifdef G_USETCL

/* read/write normal string variable */
char *nick_change(ClientData cdata, Tcl_Interp * irp, char *name1, char *name2, int flags)
{
  char *new;

  if (flags & (TCL_TRACE_READS | TCL_TRACE_UNSETS)) {
    Tcl_SetVar2(interp, name1, name2, origbotname, TCL_GLOBAL_ONLY);
    if (flags & TCL_TRACE_UNSETS)
      Tcl_TraceVar(irp, name1, TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS, nick_change, cdata);
  } else {			/* writes */
    new = Tcl_GetVar2(interp, name1, name2, TCL_GLOBAL_ONLY);
    if (rfc_casecmp(origbotname, new)) {
      if (origbotname[0])
	log(LCAT_INFO, STR("* IRC NICK CHANGE: %s -> %s"), origbotname, new);
      strncpy0(origbotname, new, NICKMAX+1);
      if (server_online)
	dprintf(DP_MODE, STR("NICK %s\n"), origbotname);
    }
  }
  return NULL;
}
#endif

/* replace all '?'s in s with a random number */
void rand_nick(char *nick)
{
  char *p = nick;
  char nickchars[20];
  strcpy(nickchars, STR("_`\\{}[]-^"));
  while ((p = strchr(p, '?')) != NULL) {
    *p = nickchars[rand() % 9];
    p++;
  }
}


#ifdef G_USETCL
char *altnick_change(ClientData cdata, Tcl_Interp * irp, char *name1, char *name2, int flags)
{
  Context;
  /* always unset raltnick. Will be regenerated when needed. */
  raltnick[0] = 0;
  return NULL;
}

char *traced_server(ClientData cdata, Tcl_Interp * irp, char *name1, char *name2, int flags)
{
  char s[1024];

  if (server_online) {
    int servidx = findanyidx(serv);

    simple_sprintf(s, STR("%s:%u"), dcc[servidx].host, dcc[servidx].port);
  } else
    s[0] = 0;
  Tcl_SetVar2(interp, name1, name2, s, TCL_GLOBAL_ONLY);
  if (flags & TCL_TRACE_UNSETS)
    Tcl_TraceVar(irp, name1, TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS, traced_server, cdata);
  return NULL;
}

char *traced_botname(ClientData cdata, Tcl_Interp * irp, char *name1, char *name2, int flags)
{
  char s[1024];

  simple_sprintf(s, STR("%s!%s"), botname, botuserhost);
  Tcl_SetVar2(interp, name1, name2, s, TCL_GLOBAL_ONLY);
  if (flags & TCL_TRACE_UNSETS)
    Tcl_TraceVar(irp, name1, TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS, traced_botname, cdata);
  return NULL;
}

char *traced_nettype(ClientData cdata, Tcl_Interp * irp, char *name1, char *name2, int flags)
{
  switch (net_type) {
  case 0:
    use_silence = 0;
    check_mode_r = 0;
    break;
  case 1:
    use_silence = 0;
    check_mode_r = 1;
    break;
  case 2:
    use_silence = 1;
    check_mode_r = 0;
    break;
  case 3:
    use_silence = 0;
    check_mode_r = 0;
    break;
  case 4:
    use_silence = 0;
    check_mode_r = 0;
    break;
  default:
    break;
  }
  return NULL;
}
#endif

cmd_t server_bot[] = {
  {"o2", "", (Function) got_o2, NULL}
  ,
  {"rn", "", (Function) got_rn, NULL}
  ,
  {"rsn", "", (Function) got_rsn, NULL}
  ,
  {0, 0, 0, 0}
};

#ifdef G_USETCL
tcl_strings my_tcl_strings[] = {
  {"botnick", 0, 0, STR_PROTECT}
  ,
  {"realname", botrealname, 80, 0}
  ,
  {"init-server", initserver, 120, 0}
  ,
  {"connect-server", connectserver, 120, 0}
  ,
  {0, 0, 0, 0}
};

tcl_coups my_tcl_coups[] = {
  {"flood-msg", &flud_thr, &flud_time}
  ,
  {"flood-ctcp", &flud_ctcp_thr, &flud_ctcp_time}
  ,
  {0, 0, 0}
};

tcl_ints my_tcl_ints[] = {
  {"use-silence", 0, 0}
  ,
  {"use-console-r", 0, 1}
  ,
  {"servlimit", &min_servs, 0}
  ,
  {"server-timeout", &server_timeout, 0}
  ,
  {"lowercase-ctcp", &lowercase_ctcp, 0}
  ,
  {"server-online", (int *) &server_online, 2},
  {"never-give-up", &never_give_up, 0},
  {"strict-servernames", &strict_servernames, 0},
  {"check-stoned", &check_stoned, 0},
  {"serverror-quit", &serverror_quit, 0},
  {"max-queue-msg", &maxqmsg, 0},
  {"trigger-on-ignore", &trigger_on_ignore, 0},
  {"answer-ctcp", &answer_ctcp, 0},
  {"server-cycle-wait", (int *) &server_cycle_wait, 0},
  {"default-port", &default_port, 0},
  {"check-mode-r", &check_mode_r, 0},
  {"net-type", &net_type, 0},
  {"must-be-owner", &must_be_owner, 0},	/* arthur2 */
  {"ctcp-mode", &ctcp_mode, 0},
  {"double-mode", &double_mode, 0},	/* G`Quann */
  {"double-server", &double_server, 0},
  {"double-help", &double_help, 0},
  {0, 0, 0}
};
#endif

/**********************************************************************/

/* oddballs */

/* read/write the server list */
#ifdef G_USETCL
char *tcl_eggserver(ClientData cdata, Tcl_Interp * irp, char *name1, char *name2, int flags)
{
  Tcl_DString ds;
  char *slist,
  **list,
    x[1024];
  struct server_list *q;
  int lc,
    code,
    i;

  Context;
  if (flags & (TCL_TRACE_READS | TCL_TRACE_UNSETS)) {
    /* create server list */
    Tcl_DStringInit(&ds);
    for (q = serverlist; q; q = q->next) {
      sprintf(x, STR("%s:%d%s%s %s"), q->name, q->port ? q->port : default_port, q->pass ? ":" : "", q->pass ? q->pass : "", q->realname ? q->realname : "");
      Tcl_DStringAppendElement(&ds, x);
    }
    slist = Tcl_DStringValue(&ds);
    Tcl_SetVar2(interp, name1, name2, slist, TCL_GLOBAL_ONLY);
    Tcl_DStringFree(&ds);
  } else {			/* writes */
    if (serverlist) {
      clearq(serverlist);
      serverlist = NULL;
    }
    slist = Tcl_GetVar2(interp, name1, name2, TCL_GLOBAL_ONLY);
    if (slist != NULL) {
      code = Tcl_SplitList(interp, slist, &lc, &list);
      if (code == TCL_ERROR) {
	return interp->result;
      }
      for (i = 0; i < lc && i < 50; i++) {
	add_server(list[i]);
      }
      /* tricky way to make the bot reset its server pointers
       * perform part of a '.jump <current-server>': */
      if (server_online) {
	int servidx = findanyidx(serv);

	curserv = (-1);
	next_server(&curserv, dcc[servidx].host, &dcc[servidx].port, "");
      }
      Tcl_Free((char *) list);
    }
  }
  Context;
  return NULL;
}

/* trace the servers */
#define tcl_traceserver(name,ptr) \
  Tcl_TraceVar(interp,name,TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS, \
	       tcl_eggserver,(ClientData)ptr)

#define tcl_untraceserver(name,ptr) \
  Tcl_UntraceVar(interp,name,TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS, \
	       tcl_eggserver,(ClientData)ptr)

#endif

/* this only handles CHAT requests, otherwise it's handled in filesys */
int ctcp_DCC_CHAT(char *nick, char *from, char *handle, char *object, char *keyword, char *text)
{
  char *action,
   *param,
   *ip,
   *prt,
    buf[512],
   *msg = buf;
  int i,
    sock;
  struct userrec *u = get_user_by_handle(userlist, handle);
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0 };

  strcpy(msg, text);
  action = newsplit(&msg);
  param = newsplit(&msg);
  ip = newsplit(&msg);
  prt = newsplit(&msg);
  Context;
  if (strcasecmp(action, STR("CHAT")) || !u)
    return 0;
  get_user_flagrec(u, &fr, 0);
  if (dcc_total == max_dcc) {
    log(LCAT_CONN, STR("DCC connections full: CHAT %s (%s!%s)"), param, nick, from);
  } else if (!(glob_party(fr))) {
    log(LCAT_CONN, STR("Refused DCC chat (no access): %s!%s"), nick, from);
    log(LCAT_WARNING, STR("Refused DCC chat (no access): %s!%s"), nick, from);
  } else if (u_pass_match(u, "-")) {
    log(LCAT_CONN, STR("Refused DCC chat (no password): %s!%s"), nick, from);
    log(LCAT_WARNING, STR("Refused DCC chat (no password): %s!%s"), nick, from);
  } else if ((atoi(prt) < min_dcc_port) || (atoi(prt) > max_dcc_port)) {
    /* invalid port range, do clients even use over 5000?? */
    log(LCAT_CONN, STR("DCC invalid port: CHAT (%s!%s)"), nick, from);
    log(LCAT_WARNING, STR("DCC invalid port: CHAT (%s!%s)"), nick, from);
  } else {
    if (!sanitycheck_dcc(nick, from, ip, prt))
      return 1;
    sock = getsock(0);
    if (open_telnet_dcc(sock, ip, prt) < 0) {
      neterror(buf);
      log(LCAT_CONN, STR("DCC Connection failed: CHAT (%s!%s)"), nick, from);
      log(LCAT_CONN, STR("    (%s)"), buf);
      killsock(sock);
    } else {
      i = new_dcc(&DCC_CHAT_PASS, sizeof(struct chat_info));

      dcc[i].addr = my_atoul(ip);
      dcc[i].port = atoi(prt);
      dcc[i].sock = sock;
      strcpy(dcc[i].nick, u->handle);
      strcpy(dcc[i].host, from);
      dcc[i].status = STAT_ECHO;
      if (glob_party(fr))
	dcc[i].status |= STAT_PARTY;
      dcc[i].timeval = now;
      dcc[i].user = u;
      /* ok, we're satisfied with them now: attempt the connect */
      log(LCAT_CONN, STR("DCC connection: CHAT (%s!%s)"), nick, from);
      dprintf(i, STR("Enter your password.\n"));
    }
  }
  return 1;
}

void server_secondly()
{
  if (cycle_time)
    cycle_time--;
  deq_msg();
  if (serv < 0) {
    if (role)
      if (!getting_users())
	connect_server();
  }
}

void server_5minutely() {
  if (!server_online) {
    log(LCAT_WARNING, STR("Not connected to any servers"));
  }
}
void server_30secondly()
{
  if (!last_server_ping) {
    dprintf(DP_MODE, STR("PING :%lu\n"), (unsigned long) now);
    last_server_ping = now;
  } else {
    server_lag = now - last_server_ping;
    if ((check_stoned) && (server_lag >= 600)) {
      int servidx = findanyidx(serv);

      disconnect_server(servidx);
      lostdcc(servidx);
      log(LCAT_INFO, STR("Server got stoned; jumping..."));
    } else {
      dprintf(DP_MODE, STR("PING :%lu\n"), (unsigned long) now);
      last_server_ping = now;
    }
  }
}

void server_prerehash()
{
  strcpy(oldnick, botname);
}

void server_postrehash()
{
  strncpy0(botname, origbotname, NICKMAX+1);
  if (!botname[0]) {
    strncpy0(origbotname, packname, NICKMAX+1);
    strcpy(botname, origbotname);
  }
  if (oldnick[0])
    strcpy(botname, oldnick);
#ifdef G_USETCL
  if (initserver[0])
    do_tcl(STR("init-server"), initserver);
#endif
}

/* a report on the module status */
void server_report(int idx, int details)
{
  char s1[128],
    s[128];

  dprintf(idx, STR("    Online as: %s!%s (%s)\n"), botname, botuserhost, botrealname);
  if (!trying_server) {
    daysdur(now, server_online, s1);
    sprintf(s, STR("(connected %s)"), s1);
    if (server_lag) {
      sprintf(s1, STR(" (lag: %ds)"), server_lag);
      if (server_lag == (-1))
	sprintf(s1, STR(" (bad pong replies)"));
      strcat(s, s1);
    }
  }
  if (server_online) {
    int servidx = findanyidx(serv);

    dprintf(idx, STR("    Server %s:%d %s\n"), dcc[servidx].host, dcc[servidx].port, trying_server ? STR("(trying)") : s);
  } else
    dprintf(idx, STR("    No server currently\n"));
  if (modeq.tot)
    dprintf(idx, STR("    Mode queue is at %d%%, %d msgs\n"), (int) ((float) (modeq.tot * 100.0) / (float) maxqmsg), (int) modeq.tot);
  if (mq.tot)
    dprintf(idx, STR("    Server queue is at %d%%, %d msgs\n"), (int) ((float) (mq.tot * 100.0) / (float) maxqmsg), (int) mq.tot);
  if (hq.tot)
    dprintf(idx, STR("    Help queue is at %d%%, %d msgs\n"), (int) ((float) (hq.tot * 100.0) / (float) maxqmsg), (int) hq.tot);
  if (details) {
    if (min_servs)
      dprintf(idx, STR("    Requiring a net of at least %d server(s)\n"), min_servs);
#ifdef G_USETCL
    if (initserver[0])
      dprintf(idx, STR("    On connect, I do: %s\n"), initserver);
    if (connectserver[0])
      dprintf(idx, STR("    Before connect, I do: %s\n"), connectserver);
#endif
    dprintf(idx, STR("    Flood is: %d msg/%ds, %d ctcp/%ds\n"), flud_thr, flud_time, flud_ctcp_thr, flud_ctcp_time);
  }
}

int server_expmem()
{
  int tot = 0;
  struct msgq *m = mq.head;
  struct server_list *s = serverlist;

  if (curslist)
    tot += strlen(curslist) + 1;

  Context;
  for (; s; s = s->next) {
    if (s->name)
      tot += strlen(s->name) + 1;
    if (s->pass)
      tot += strlen(s->pass) + 1;
    if (s->realname)
      tot += strlen(s->realname) + 1;
    tot += sizeof(struct server_list);
  }
  while (m != NULL) {
    tot += m->len + 1;
    tot += sizeof(struct msgq);

    m = m->next;
  }
  m = hq.head;
  while (m != NULL) {
    tot += m->len + 1;
    tot += sizeof(struct msgq);

    m = m->next;
  }
  m = modeq.head;
  while (m != NULL) {
    tot += m->len + 1;
    tot += sizeof(struct msgq);

    m = m->next;
  }
  return tot;
}

#ifdef OLDCODE
/* puts full hostname in s */
void getmyhostname(char *s)
{
  struct hostent *hp;
  char *p;

  if (hostname[0]) {
    strcpy(s, hostname);
    return;
  }
  p = getenv(STR("HOSTNAME"));
  if (p != NULL) {
    strncpy0(s, p, 80);
    if (strchr(s, '.') != NULL)
      return;
  }
  gethostname(s, 80);
  if (strchr(s, '.') != NULL)
    return;
  hp = gethostbyname(s);
  if (hp == NULL)
    fatal(STR("Hostname self-lookup failed."), 0);
  strcpy(s, hp->h_name);
  if (strchr(s, '.') != NULL)
    return;
  if (hp->h_aliases[0] == NULL)
    fatal(STR("Can't determine your hostname!"), 0);
  strcpy(s, hp->h_aliases[0]);
  if (strchr(s, '.') == NULL)
    fatal(STR("Can't determine your hostname!"), 0);
}
#endif

/* update the add/rem_builtins in server.c if you add to this list!! */

void init_server()
{
#ifdef G_USETCL
  char *s;
#endif

  /* init all the variables *must* be done in _start rather than globally */
  serv = -1;
  strict_host = 1;
  strncpy0(botname, packname, 10);
  trying_server = 0L;
  server_lag = 0;
  raltnick[0] = 0;
  curserv = 0;
  flud_thr = 5;
  flud_time = 60;
  flud_ctcp_thr = 3;
  flud_ctcp_time = 60;
#ifdef G_USETCL
  initserver[0] = 0;
  connectserver[0] = 0;		/* drummer */
#endif
  botuserhost[0] = 0;
  serverror_quit = 1;
  last_server_ping = 0;
  server_online = 0;
  server_cycle_wait = 20;
  strcpy(botrealname, packname);
  if (getlogin())
    sprintf(botuser, "%s", getlogin());
  else
    sprintf(botuser, "%i", getuid());

  min_servs = 0;
  server_timeout = 60;
  never_give_up = 0;
  strict_servernames = 0;
  serverlist = NULL;
  cycle_time = 0;
  default_port = 6667;
  oldnick[0] = 0;
  trigger_on_ignore = 0;
  answer_ctcp = 1;
  lowercase_ctcp = 0;
  bothost[0] = 0;
  check_mode_r = 0;
  maxqmsg = 300;
  burst = 0;
  net_type = 0;
  double_mode = 0;
  double_server = 0;
  double_help = 0;
  Context;
  /* fool bot in reading the values */
#ifdef G_USETCL
  tcl_traceserver(STR("servers"), NULL);
  s = Tcl_GetVar(interp, STR("nick"), TCL_GLOBAL_ONLY);
  if (s) {
    strncpy0(origbotname, s, NICKMAX+1);
  }
  Tcl_TraceVar(interp, STR("nick"), TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS, nick_change, NULL);

  Tcl_TraceVar(interp, STR("botname"), TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS, traced_botname, NULL);
  Tcl_TraceVar(interp, STR("server"), TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS, traced_server, NULL);
  Tcl_TraceVar(interp, STR("net-type"), TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS, traced_nettype, NULL);
#endif
  Context;
  H_raw = add_bind_table(STR("raw"), HT_STACKABLE, server_raw);
  H_wall = add_bind_table(STR("wall"), HT_STACKABLE, server_2char);
  H_notc = add_bind_table(STR("notc"), HT_STACKABLE, server_5char);
  H_msgm = add_bind_table(STR("msgm"), HT_STACKABLE, server_5char);
  H_msg = add_bind_table(STR("msg"), 0, server_msg);
  H_flud = add_bind_table(STR("flud"), HT_STACKABLE, server_5char);
  H_ctcr = add_bind_table(STR("ctcr"), HT_STACKABLE, server_6char);
  H_ctcp = add_bind_table(STR("ctcp"), HT_STACKABLE, server_6char);
  Context;
  add_builtins(H_raw, my_raw_binds);
  add_builtins(H_dcc, C_dcc_serv);
  add_builtins(H_bot, server_bot);
  Context;
#ifdef G_USETCL
  my_tcl_strings[0].buf = botname;
  add_tcl_strings(my_tcl_strings);
  my_tcl_ints[0].val = &use_silence;
  my_tcl_ints[1].val = &use_console_r;
  add_tcl_ints(my_tcl_ints);
  add_tcl_commands(my_tcl_cmds);
  add_tcl_coups(my_tcl_coups);
#endif
  Context;
  add_hook(HOOK_SECONDLY, (Function) server_secondly);
  add_hook(HOOK_MINUTELY, (Function) minutely_checks);
  add_hook(HOOK_5MINUTELY, (Function) server_5minutely);
  add_hook(HOOK_30SECONDLY, (Function) server_30secondly);
  add_hook(HOOK_QSERV, (Function) queue_server);
  add_hook(HOOK_PRE_REHASH, (Function) server_prerehash);
  add_hook(HOOK_REHASH, (Function) server_postrehash);
  Context;
  mq.head = hq.head = modeq.head = 0;
  mq.last = hq.last = modeq.last = 0;
  mq.tot = hq.tot = modeq.tot = 0;
  mq.warned = hq.warned = modeq.warned = 0;
  double_warned = 0;
  Context;
  newserver[0] = 0;
  newserverport = 0;
  strcpy(bothost, STR("localhost"));
  Context;
  sprintf(botuserhost, STR("%s@%s"), botuser, bothost);	/* wishful thinking */
  curserv = 999;
  if (net_type == 0) {		/* EfNet except new +e/+I hybrid */
    use_silence = 0;
    check_mode_r = 0;
  }
  if (net_type == 1) {		/* Ircnet */
    use_silence = 0;
    check_mode_r = 1;
  }
  if (net_type == 2) {		/* Undernet */
    use_silence = 1;
    check_mode_r = 0;
  }
  if (net_type == 3) {		/* Dalnet */
    use_silence = 0;
    check_mode_r = 0;
  }
  if (net_type == 4) {		/* new +e/+I Efnet hybrid */
    use_silence = 0;
    check_mode_r = 0;
  }
  add_cfg(&CFG_NICK);
  add_cfg(&CFG_SERVERS);
  add_cfg(&CFG_REALNAME);
  set_cfg_str(NULL, STR("realname"), packname);
}
#else
void init_server()
{
  add_cfg(&CFG_NICK);
  add_cfg(&CFG_SERVERS);
  add_cfg(&CFG_REALNAME);
  set_cfg_str(NULL, STR("realname"), packname);
}
#endif


