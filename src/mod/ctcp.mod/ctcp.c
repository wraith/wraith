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
 * ctcp.c -- part of ctcp.mod
 *   all the ctcp handling (except DCC, it's special ;)
 *
 */


#include "ctcp.h"
#include "src/common.h"
#include "src/response.h"
#include "src/main.h"
#include "src/set.h"
#include "src/chanprog.h"
#include "src/cmds.h"
#include "src/misc.h"
#include "src/net.h"
#include "src/userrec.h"
#include "src/botmsg.h"
#include "src/binds.h"
#include "src/egg_timer.h"
#include "src/mod/server.mod/server.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/utsname.h>
#include <ctype.h>
#include <bdlib/src/String.h>

#define AVGAWAYTIME             60
#define AVGHERETIME             5
time_t cloak_awaytime = 0;
time_t cloak_heretime = 0;
interval_t listen_time = 0;
char cloak_bxver[10] = "";
char cloak_os[20] = "";
char cloak_osver[100] = "";
char cloak_host[161] = "";
char ctcpversion[300] = "";
char ctcpversion2[300] = "";
char ctcpuserinfo[200] = "";
char autoaway[100] = "";
char kickprefix[25] = "";
char bankickprefix[25] = "";

void scriptchanged()
{
  /* Check if this was called from init_vars() before ctcp_init() is called */
  if (!cloak_bxver[0])
    return;

  char tmp[200] = "", *p = NULL;

  ctcpversion[0] = ctcpversion2[0] = ctcpuserinfo[0] = autoaway[0] = kickprefix[0] = bankickprefix[0] = 0;
  
  switch (cloak_script) {
  case CLOAK_PLAIN:
    simple_snprintf(ctcpversion, sizeof(ctcpversion), "\002BitchX-%s\002 by panasync - %s %s : \002Keep it to yourself!\002", cloak_bxver, cloak_os, cloak_osver);
    ctcpuserinfo[0] = kickprefix[0] = bankickprefix[0] = 0;
    strlcpy(autoaway, "Auto-Away after 10 mins", sizeof(autoaway));
    break;
  case CLOAK_CRACKROCK:
    simple_snprintf(ctcpversion, sizeof(ctcpversion), "BitchX-%s\002/\002%s %s:(\002c\002)\037rackrock\037/\002b\002X \037[\0373.0.1á9\037]\037 :\002 Keep it to yourself!\002", cloak_bxver, cloak_os, cloak_osver);
    strlcpy(ctcpuserinfo, "crack addict, help me.", sizeof(ctcpuserinfo));
    strlcpy(autoaway, "automatically dead", sizeof(autoaway));
    strlcpy(kickprefix, "\002c\002/\037k\037: ", sizeof(kickprefix));
    strlcpy(bankickprefix, "\002c\002/\037kb\037: ", sizeof(bankickprefix));
    break;
  case CLOAK_NEONAPPLE:
    simple_snprintf(tmp, sizeof(tmp), "%s %s", cloak_os, cloak_osver);
    strtolower(tmp);
    simple_snprintf(ctcpversion, sizeof(ctcpversion), "bitchx-%s\037(\037%s\037):\037 \002n\002eon\037a\037ppl\002e\002\037/\037\002v\0020\037.\03714i : \002d\002ont you wish you had it\037?\037", cloak_bxver, tmp);
    strlcpy(ctcpuserinfo, "neon apple", sizeof(ctcpuserinfo));
    strlcpy(autoaway, "automatically away after 10 mins \037(\037\002n\002/\037a)\037", sizeof(autoaway));
    strlcpy(kickprefix, "\037[na\002(\037k\037)\002]\037 ", sizeof(kickprefix));
    bankickprefix[0] = 0;
    break;
  case CLOAK_TUNNELVISION:
    strlcpy(tmp, cloak_bxver, sizeof(tmp));
    p = tmp;
    p += strlen(tmp) - 1;
    p[1] = p[0];
    p[0] = '\037';
    p[2] = '\037';
    p[3] = 0;
    simple_snprintf(ctcpversion, sizeof(ctcpversion), "\002b\002itchx-%s :tunnel\002vision\002/\0371.2\037", tmp);
    ctcpuserinfo[0] = kickprefix[0] = bankickprefix[0] = 0;
    strlcpy(autoaway, "auto-gone", sizeof(autoaway));
    break;
  case CLOAK_ARGON:
    simple_snprintf(ctcpversion, sizeof(ctcpversion), ".\037.(\037argon\002/\0021g\037)\037 \002:\002bitchx-%s", cloak_bxver);
    ctcpuserinfo[0] = 0;
    strlcpy(autoaway, "\037(\037ar\037)\037 auto-away \037(\03710m\037)\037", sizeof(autoaway));
    strlcpy(kickprefix, "\037(\037ar\037)\037 ", sizeof(kickprefix));
    strlcpy(bankickprefix, "\037(\037ar\037)\037 ", sizeof(bankickprefix));
    break;
  case CLOAK_EVOLVER:
    simple_snprintf(ctcpversion, sizeof(ctcpversion), "\037evolver\037(\00202x9\002)\037: bitchx\037(\002%s\002) \037í\037 %s\002/\002%s : eye yam pheerable now!", cloak_bxver, cloak_os, cloak_osver);
    ctcpuserinfo[0] = 0;
    strlcpy(autoaway, "[\037\002i\002dle for \037[\03710 minutes\037]]", sizeof(autoaway));
    strlcpy(kickprefix, "\037ev\002!\002k\037 ", sizeof(kickprefix));
    strlcpy(bankickprefix, "\037ev\002!\002bk\037 ", sizeof(bankickprefix));
    break;
  case CLOAK_PREVAIL:
    simple_snprintf(ctcpversion, sizeof(ctcpversion), "%s\037!\037%s bitchx-%s \002-\002 prevail\037[\0370123\037]\037 :down with people", cloak_os, cloak_osver, cloak_bxver);
    strlcpy(ctcpuserinfo, botrealname, sizeof(ctcpuserinfo));
    strlcpy(autoaway, "idle 10 minutes \037-\037 gone\037!\037", sizeof(autoaway));
    strlcpy(kickprefix, "\037[\037pv\037!\037k\037]\037 ", sizeof(kickprefix));
    strlcpy(bankickprefix, "\037[\037pv\037!\037bk\037]\037 ", sizeof(bankickprefix));
    break;
  case CLOAK_MIRC:
  {
    char mircver[5] = "";
   
    strlcpy(mircver, response(RES_MIRCVER), sizeof(mircver));
    simple_snprintf(ctcpversion, sizeof(ctcpversion), "mIRC v%s Khaled Mardam-Bey", mircver);
    if (randint(2) % 2)
      strlcpy(ctcpversion2, response(RES_MIRCSCRIPT), sizeof(ctcpversion2));
    strlcpy(ctcpuserinfo, botrealname, sizeof(ctcpuserinfo));
    strlcpy(autoaway, "auto-away after 10 minutes", sizeof(autoaway));
    kickprefix[0] = bankickprefix[0] = 0;
    break;
  }
  case CLOAK_OTHER:
  {
    strlcpy(ctcpversion, response(RES_OTHERSCRIPT), sizeof(ctcpversion));
    strlcpy(ctcpuserinfo, botrealname, sizeof(ctcpuserinfo));
    strlcpy(autoaway, "auto-away after 10 minutes", sizeof(autoaway));
    kickprefix[0] = bankickprefix[0];
    break;
  }
  case CLOAK_CYPRESS:
  {
    char theme[30] = "";

    switch (randint(25)) { /* 0-19 = script, 20-24 = plain */
    case 0:
      strlcpy(theme, " \037.\037.\002BX\002", sizeof(theme));
      break;
    case 1:
      strlcpy(theme, " \037.\037.chl\037o\037rine", sizeof(theme));
      break;
    case 2:
      strlcpy(theme, " \037.\037.\037<\037c\002x\002\037>\037", sizeof(theme));
      break;
    case 3:
      strlcpy(theme, " \037.\037.supercyan", sizeof(theme));
      break;
    case 4:
      strlcpy(theme, " \037.\037.\037c\037yan\002i\002\002\037z\037\002\037e\037d", sizeof(theme));
      break;
    case 5:
      strlcpy(theme, " \037.\037.delusion", sizeof(theme));
      break;
    case 6:
      strlcpy(theme, " \037.\037.\002e\002mbryonic", sizeof(theme));
      break;
    case 7:
      strlcpy(theme, " \037.\037.e\002x\002tra\037.\037terrestr\037i\037al", sizeof(theme));
      break;
    case 8:
      strlcpy(theme, " \037.\037.\002f\002ad\037e\037d", sizeof(theme));
      break;
    case 9:
      strlcpy(theme, " \037.\037.fo\037c\037us", sizeof(theme));
      break;
    case 10:
      strlcpy(theme, " \037.\037.\002h\002ade\037s\037", sizeof(theme));
      break;
    case 11:
      strlcpy(theme, " \037.\037.hellbent\037.\037", sizeof(theme));
      break;
    case 12:
      strlcpy(theme, " \037.\037.illusi\037o\037n", sizeof(theme));
      break;
    case 13:
      strlcpy(theme, " \037.\037.\037j\037ungl\037e\037", sizeof(theme));
      break;
    case 14:
      strlcpy(theme, " \037.\037.\002l\002abry\037i\037nth", sizeof(theme));
      break;
    case 15:
      strlcpy(theme, " \037.\037.nightblue", sizeof(theme));
      break;
    case 16:
      strlcpy(theme, " \037.\037.\037o\037bli\037v\037io\037n\037", sizeof(theme));
      break;
    case 17:
      strlcpy(theme, " \037.\037.ph\002a\002ze", sizeof(theme));
      break;
    case 18:
      strlcpy(theme, " \037.\037.sphere", sizeof(theme));
      break;
    case 19:
      strlcpy(theme, " \037.\037.zip", sizeof(theme));
      break;
    default:
      theme[0] = 0;
      break;
    }
    switch (randint(16)) {
    case 0:
      simple_snprintf(ctcpversion, sizeof(ctcpversion), "bitchx\037-\037%s \037/\037 cypress\037.\03701i%s", cloak_bxver, theme);
      break;
    case 1:
      simple_snprintf(ctcpversion, sizeof(ctcpversion), "cypress\037.\03701i%s \037/\037 bitchx\037-\037%s", theme, cloak_bxver);
      break;
    case 2:
      simple_snprintf(tmp, sizeof(tmp), "%s %s", cloak_os, cloak_osver);
      strtolower(tmp);
      simple_snprintf(ctcpversion, sizeof(ctcpversion), "cypress\037.\03701i%s %s\037(\037%s\037)\037 bitchx\037-\037%s)", theme, tmp, cloak_host, cloak_bxver);
      break;
    case 3:
      simple_snprintf(tmp, sizeof(tmp), "%s %s", cloak_os, cloak_osver);
      strtolower(tmp);
      simple_snprintf(ctcpversion, sizeof(ctcpversion), "bitchx\037-\037%s %s\037(\037%s\037)\037 cypress\037.\03701i%s", cloak_bxver, tmp, cloak_host, theme);
      break;
    case 4:
      simple_snprintf(ctcpversion, sizeof(ctcpversion), "%s\002/\002%s: BitchX-%s \002[\002cypress\002]\002 v01i%s", cloak_os, cloak_osver, cloak_bxver, theme);
      break;
    case 5:
      p = replace(cloak_osver, ".", "\037.\037");
      simple_snprintf(tmp, sizeof(tmp), "%s %s", cloak_os, p);
      strtolower(tmp);
      simple_snprintf(ctcpversion, sizeof(ctcpversion), "\037.\037.cypress\037.\03701i%s %s\037(\037%s\037)\037 bitchx\037/\037%s",theme, tmp, cloak_host, cloak_bxver);
      break;
    case 6:
      p = replace(cloak_osver, ".", "\037.\037");
      simple_snprintf(tmp, sizeof(tmp), "%s %s", cloak_os, p);
      strtolower(tmp);
      simple_snprintf(ctcpversion, sizeof(ctcpversion), "cypress\002.\00201i%s\037(\037bitchx\002.\002%s\037)\037\002.\002. %s\037(\037%s\037)\037",theme, cloak_bxver, tmp, cloak_host);
      break;
    case 7:
      p = replace(cloak_osver, ".", "\037.\037");
      simple_snprintf(tmp, sizeof(tmp), "%s %s", cloak_os, p);
      strtolower(tmp);
      simple_snprintf(ctcpversion, sizeof(ctcpversion), "\037.\037.cypress\037.\03701i%s - bitchx\037.\037%s\002/\002%s", theme, cloak_bxver, tmp);
      break;
    case 8:
      simple_snprintf(ctcpversion, sizeof(ctcpversion), "\002BitchX-%s\002 by panasync \002-\002 %s %s", cloak_bxver, cloak_os, cloak_osver);
      break;
    case 9:
      simple_snprintf(ctcpversion, sizeof(ctcpversion), "\037[\037cypress\002/\00201i\037]\037 - %s \037[\037bx\002/\002%s\037]\037", theme, cloak_bxver);
      break;
    case 10:
      simple_snprintf(ctcpversion, sizeof(ctcpversion), "\037[\037\002b\002itchx\002.\002%s\037]\037 \002+\002 \037[\037cypress\002.\00201i\037]\037 %s",cloak_bxver, theme);
      break;
    case 11:
      simple_snprintf(ctcpversion, sizeof(ctcpversion), "\037[\037BitchX\002/\002%s\037(\037cypress\002/\00201i\037)]\037 %s", cloak_bxver, theme);
      break;
    case 12:
      strtolower(cloak_os);
      simple_snprintf(ctcpversion, sizeof(ctcpversion), "bitchx\037/\037%s %s %s \037(\037cypress\037/\03701i\037)\037 %s", cloak_bxver, cloak_os, cloak_osver, theme);
      break;
    case 13:
      simple_snprintf(ctcpversion, sizeof(ctcpversion), "\037.\037.cypress\037/\03701i\037!\037bitchx\037/\037%s\037.\037.%s", cloak_bxver, theme);
      break;
    case 14:
      strtolower(cloak_bxver);
      simple_snprintf(ctcpversion, sizeof(ctcpversion), "cypress\002\037.\037\002\037.\03701i\002/\002bitchx\037.\037\002\037.\037\002%s%s", cloak_bxver, theme);
      break;
    case 15:
      strtolower(cloak_bxver);
      simple_snprintf(ctcpversion, sizeof(ctcpversion), "cypress\037.\03701i\037/\037bx%s \037(\037%s\037)\037", cloak_bxver, theme);
      break;
    }
    ctcpuserinfo[0] = 0;
    strlcpy(autoaway, "autoaway after 40 min", sizeof(autoaway));
    strlcpy(kickprefix, "\002.\002.\037(\037\002c\002yp\002/\002k\037)\037 ", sizeof(kickprefix));
    strlcpy(bankickprefix, "\002.\002.\037(\037\002c\002yp\002/\002bk\037)\037 ", sizeof(bankickprefix));
    break;
  }
  }
}

void sendaway()
{
  char awtime[20] = "";
  int hrs, min, sec;
  time_t gt;

  gt = now - cloak_awaytime;
  hrs = gt / 3600;
  min = (gt % 3600) / 60;
  sec = gt % 60;
  switch (cloak_script) {
  case CLOAK_PLAIN:
    dprintf(DP_HELP, "AWAY :is away: (%s) [\002BX\002-MsgLog Off]\n", autoaway);
    break;
  case CLOAK_MIRC:
    dprintf(DP_HELP, "AWAY :is away: (%s)\n", autoaway);
    break;
  case CLOAK_OTHER:
    dprintf(DP_HELP, "AWAY :is away: (%s)\n", autoaway);
    break;
  case CLOAK_CRACKROCK:
    if (hrs)
      simple_snprintf(awtime, sizeof(awtime), "%dh %dm %ds", hrs, min, sec);
    else if (min)
      simple_snprintf(awtime, sizeof(awtime), "%dm %ds", min, sec);
    else
      simple_snprintf(awtime, sizeof(awtime), "%ds", sec);
    dprintf(DP_HELP, "AWAY :%s\002\037[\002%s\002]\037\002\n", autoaway, awtime);
    break;
  case CLOAK_TUNNELVISION:
    if (hrs)
      simple_snprintf(awtime, sizeof(awtime), "%dh%dm%ds", hrs, min, sec);
    else if (min)
      simple_snprintf(awtime, sizeof(awtime), "%dm%ds", min, sec);
    else
      simple_snprintf(awtime, sizeof(awtime), "%ds", sec);
    dprintf(DP_HELP, "AWAY :%s \037(\037%s\037)\037\n", autoaway, awtime);
    break;
  case CLOAK_ARGON:
    if (hrs)
      simple_snprintf(awtime, sizeof(awtime), "%dh%dm%ds", hrs, min, sec);
    else if (min)
      simple_snprintf(awtime, sizeof(awtime), "%dm%ds", min, sec);
    else
      simple_snprintf(awtime, sizeof(awtime), "%ds", sec);
    dprintf(DP_HELP, "AWAY :%s .\002.\002\037(\037%s\037)\037\n", autoaway, awtime);
    break;
  case CLOAK_EVOLVER:
    if (hrs)
      simple_snprintf(awtime, sizeof(awtime), "%dh %dm %ds", hrs, min, sec);
    else if (min)
      simple_snprintf(awtime, sizeof(awtime), "%dm %ds", min, sec);
    else
      simple_snprintf(awtime, sizeof(awtime), "%ds", sec);
    dprintf(DP_HELP, "AWAY :away\037: %s (\037l\002:\002off\037,\037 p\002:\002off\037)\037 \037[\037gone\002:\002%s\037]\037\n", autoaway, awtime);
    break;
  case CLOAK_PREVAIL:
    if (hrs)
      simple_snprintf(awtime, sizeof(awtime), "%dh%dm%ds", hrs, min, sec);
    else if (min)
      simple_snprintf(awtime, sizeof(awtime), "%dm%ds", min, sec);
    else
      simple_snprintf(awtime, sizeof(awtime), "%ds", sec);
    dprintf(DP_HELP, "AWAY :%s %s\n", autoaway, awtime);
    break;
  case CLOAK_CYPRESS:
    dprintf(DP_HELP, "AWAY :is gone\037.\037. %s \037.\037.\037[\037\002c\002yp\037(\037l\002/\002off\002.\002p\002/\002off)]\n", autoaway);
    break;
  }
}

static void ctcp_minutely()
{
  if (irc_autoaway && server_online) {
    if ((cloak_awaytime == 0) && (cloak_heretime == 0)) {
      cloak_heretime = now;
      dprintf(DP_HELP, "AWAY :\n");
      return;
    }

    if (cloak_awaytime == 0) {
      if (!randint(AVGHERETIME)) {
        cloak_heretime = 0;
        cloak_awaytime = now - 600 - randint(60);
        sendaway();
      }
    } else {
      if (!randint(AVGAWAYTIME)) {
        cloak_awaytime = 0;
        cloak_heretime = now;
        dprintf(DP_HELP, "AWAY :\n");
      } else
        sendaway();
    }
  }

  if (listen_time <= 0) {
    for (int i = 0; i < dcc_total; i++) {
      if (dcc[i].type && (dcc[i].type->flags & DCT_LISTEN) && !strcmp(dcc[i].nick, "(telnet)"))
        listen_all(0, 1, 0);
    }
  } else
    listen_time--;
}

static int ctcp_FINGER(char *nick, char *uhost, struct userrec *u, char *object, char *keyword, char *text)
{
  int idletime = 0;

  if (cloak_awaytime)
    idletime = now - cloak_awaytime;
  else if (cloak_heretime)
    idletime = now - cloak_heretime;
  bd::String msg;
  msg = bd::String::printf("\001%s %s (%s) Idle %d second%s\001", keyword, "",
                   botuserhost, (int) idletime, idletime == 1 ? "" : "s");
  notice(nick, msg, DP_HELP);
  return BIND_RET_BREAK;
}

static int ctcp_ECHO(char *nick, char *uhost, struct userrec *u, char *object, char *keyword, char *text)
{
  char reply[60] = "";

  strlcpy(reply, text, sizeof(reply));
  bd::String msg;
  msg = bd::String::printf("\001%s %s\001", keyword, reply);
  notice(nick, msg, DP_HELP);
  return BIND_RET_BREAK;
}
static int ctcp_PING(char *nick, char *uhost, struct userrec *u, char *object, char *keyword, char *text)
{
  if (strlen(text) <= 80) {       /* bitchx ignores > 80 */
    bd::String msg;
    msg = bd::String::printf("\001%s %s\001", keyword, text);
    notice(nick, msg, DP_HELP);
  }
  return BIND_RET_BREAK;
}

int first_ctcp_check = 0;

static int ctcp_VERSION(char *nick, char *uhost, struct userrec *u, char *object, char *keyword, char *text)
{
  char s[50] = "";

  if (cloak_script == CLOAK_CYPRESS) {
    switch (randint(8)) {
    case 0:
      strlcpy(s, " :should of put the glock down.", sizeof(s));
      break;
    case 1:
      strlcpy(s, " :hot damn, I didn't want to kill a man.", sizeof(s));
      break;
    case 2:
      strlcpy(s, " :check me and I'll check ya back.", sizeof(s));
      break;
    case 3:
      strlcpy(s, " :put the blunt down just for a minute.", sizeof(s));
      break;
    case 4:
      strlcpy(s, " :tried to jack me, my homie got shot.", sizeof(s));
      break;
    case 5:
      strlcpy(s, " :insane in the membrane", sizeof(s));
      break;
    case 6:
      strlcpy(s, " :slow hits from the bong", sizeof(s));
      break;
    case 7:
      strlcpy(s, " :k\002-\002leet", sizeof(s));
      break;
    }
  }
  
  int queue = DP_HELP;

  // Send out first few replies right away for dronemons
  if (first_ctcp_check < 2) {
    queue = DP_SERVER;
    ++first_ctcp_check;
  }

  bd::String msg;
  msg = bd::String::printf("\001%s %s%s\001", keyword, ctcpversion, s);
  notice(nick, msg, queue);

  if (ctcpversion2[0]) {
    msg = bd::String::printf("\001%s %s\001", keyword, ctcpversion2);
    notice(nick, msg, DP_HELP);
  }
  return BIND_RET_BREAK;
}

static int ctcp_WHOAMI(char *nick, char *uhost, struct userrec *u, char *object, char *keyword, char *text)
{
  if (cloak_script > 0 && cloak_script < 9) {
    bd::String msg;
    msg = bd::String::printf("\002BitchX\002: Access Denied");
    notice(nick, msg, DP_HELP);
  }
  return BIND_RET_BREAK;
}

static int ctcp_OP(char *nick, char *uhost, struct userrec *u, char *object, char *keyword, char *text)
{
  if (text[0] && cloak_script > 0 && cloak_script < 9) {
    char chan[256] = "", *p = NULL;

    strlcpy(chan, text, sizeof(chan));
    p = strchr(chan, ' ');
    if (p)
      *p = 0;
    bd::String msg;
    msg = bd::String::printf("\002BitchX\002: I'm not on %s or I'm not opped", chan);
    notice(nick, msg, DP_HELP);
  }
  return BIND_RET_BREAK;
}

static int ctcp_INVITE_UNBAN(char *nick, char *uhost, struct userrec *u, char *object, char *keyword, char *text)
{
  if (text[0] == '#' && cloak_script > 0 && cloak_script < 9) {
    struct chanset_t *chan = chanset;
    char chname[256] = "", *p = NULL;
    bd::String msg;

    strlcpy(chname, text, sizeof(chname));
    p = strchr(chname, ' ');
    if (p)
      *p = 0;
    while (chan) {
      if (chan->ircnet_status & CHAN_ACTIVE) {
        if (!strcasecmp(chan->name, chname)) {
          msg = bd::String::printf("\002BitchX\002: Access Denied");
          notice(nick, msg, DP_HELP);
          return BIND_RET_LOG;
        }
      }
      chan = chan->next;
    }
    msg = bd::String::printf("\002BitchX\002: I'm not on that channel");
    notice(nick, msg, DP_HELP);
  }
  return BIND_RET_BREAK;
}

static int ctcp_USERINFO(char *nick, char *uhost, struct userrec *u, char *object, char *keyword, char *text)
{
  if (cloak_script == CLOAK_TUNNELVISION)
    strlcpy(ctcpuserinfo, botname, sizeof(ctcpuserinfo));
  else if (cloak_script == CLOAK_PREVAIL) {
    strlcpy(ctcpuserinfo, botname, sizeof(ctcpuserinfo));
    strlcat(ctcpuserinfo, " ?", sizeof(ctcpuserinfo));
  }
  bd::String msg;
  msg = bd::String::printf("\001%s %s\001", keyword, ctcpuserinfo);
  notice(nick, msg, DP_HELP);
  return BIND_RET_BREAK;
}

static int ctcp_CLIENTINFO(char *nick, char *uhost, struct userrec *u, char *object, char *keyword, char *text)
{
  if (!(cloak_script > 0 && cloak_script < 9))
    return BIND_RET_BREAK;

  char buf[256] = "";
  bd::String msg;

  if (!text[0]) {
    strlcpy(buf, "SED UTC ACTION DCC CDCC BDCC XDCC VERSION CLIENTINFO USERINFO ERRMSG FINGER TIME PING ECHO INVITE WHOAMI OP OPS UNBAN IDENT XLINK UPTIME :Use CLIENTINFO <COMMAND> to get more specific information", sizeof(buf));
  } else if (!strcasecmp(text, "UNBAN"))
    strlcpy(buf, "UNBAN unbans the person from channel", sizeof(buf));
  else if (!strcasecmp(text, "OPS"))
    strlcpy(buf, "OPS ops the person if on userlist", sizeof(buf));
  else if (!strcasecmp(text, "ECHO"))
    strlcpy(buf, "ECHO returns the arguments it receives", sizeof(buf));
  else if (!strcasecmp(text, "WHOAMI"))
    strlcpy(buf, "WHOAMI user list information", sizeof(buf));
  else if (!strcasecmp(text, "INVITE"))
    strlcpy(buf, "INVITE invite to channel specified", sizeof(buf));
  else if (!strcasecmp(text, "PING"))
    strlcpy(buf, "PING returns the arguments it receives", sizeof(buf));
  else if (!strcasecmp(text, "UTC"))
    strlcpy(buf, "UTC substitutes the local timezone", sizeof(buf));
  else if (!strcasecmp(text, "XDCC"))
    strlcpy(buf, "XDCC checks cdcc info for you", sizeof(buf));
  else if (!strcasecmp(text, "BDCC"))
    strlcpy(buf, "BDCC checks cdcc info for you", sizeof(buf));
  else if (!strcasecmp(text, "CDCC"))
    strlcpy(buf, "CDCC checks cdcc info for you", sizeof(buf));
  else if (!strcasecmp(text, "DCC"))
    strlcpy(buf, "DCC requests a direct_client_connection", sizeof(buf));
  else if (!strcasecmp(text, "ACTION"))
    strlcpy(buf, "ACTION contains action descriptions for atmosphere", sizeof(buf));
  else if (!strcasecmp(text, "FINGER"))
    strlcpy(buf, "FINGER shows real name, login name and idle time of user", sizeof(buf));
  else if (!strcasecmp(text, "ERRMSG"))
    strlcpy(buf, "ERRMSG returns error messages", sizeof(buf));
  else if (!strcasecmp(text, "USERINFO"))
    strlcpy(buf, "USERINFO returns user settable information", sizeof(buf));
  else if (!strcasecmp(text, "CLIENTINFO"))
    strlcpy(buf, "CLIENTINFO gives information about available CTCP commands", sizeof(buf));
  else if (!strcasecmp(text, "SED"))
    strlcpy(buf, "SED contains simple_encrypted_data", sizeof(buf));
  else if (!strcasecmp(text, "OP"))
    strlcpy(buf, "OP ops the person if on userlist", sizeof(buf));
  else if (!strcasecmp(text, "VERSION"))
    strlcpy(buf, "VERSION shows client type, version and environment", sizeof(buf));
  else if (!strcasecmp(text, "XLINK"))
    strlcpy(buf, "XLINK x-filez rule", sizeof(buf));
  else if (!strcasecmp(text, "IDENT"))
    strlcpy(buf, "IDENT change userhost of userlist", sizeof(buf));
  else if (!strcasecmp(text, "TIME"))
    strlcpy(buf, "TIME tells you the time on the user's host", sizeof(buf));
  else if (!strcasecmp(text, "UPTIME"))
    strlcpy(buf, "UPTIME my uptime", sizeof(buf));
  else {
    msg = bd::String::printf("\001ERRMSG %s is not a valid function\001", text);
    notice(nick, msg, DP_HELP);
    return BIND_RET_LOG;
  }
  msg = bd::String::printf("\001%s %s\001", keyword, buf);
  notice(nick, msg, DP_HELP);
  return BIND_RET_BREAK;
}

static int ctcp_TIME(char *nick, char *uhost, struct userrec *u, char *object, char *keyword, char *text)
{
  char tms[81] = "";

  strlcpy(tms, ctime(&now), sizeof(tms));
  bd::String msg;
  msg = bd::String::printf("\001%s %s\001", keyword, tms);
  notice(nick, msg, DP_HELP);
  return BIND_RET_BREAK;
}


static int ctcp_CHAT(char *nick, char *uhost, struct userrec *u, char *object, char *keyword, char *text)
{

  if (!ischanhub()) 
    return BIND_RET_LOG;

    if (u_pass_match(u, "-")) {
      strlcat(ctcp_reply, "\001ERROR no password set\001", sizeof(ctcp_reply));
      return BIND_RET_BREAK;
    }

    int ix = -1, i = 0;

    for (i = 0; i < dcc_total; i++) {
      if (dcc[i].type && (dcc[i].type->flags & DCT_LISTEN) && (!strcmp(dcc[i].nick, "(telnet)")))
        ix = i;
    }
    if (!iptolong(getmyip())) {
      simple_snprintf(&ctcp_reply[strlen(ctcp_reply)], sizeof(ctcp_reply) - strlen(ctcp_reply), "\001ERROR no ipv4 ip defined. Use /dcc chat %s\001", botname);
    } else if (dcc_total == max_dcc || (ix < 0 && (ix = listen_all(0, 0, 0)) < 0))
      strlcat(ctcp_reply, "\001ERROR no telnet port\001", sizeof(ctcp_reply));
    else {
      if (listen_time <= 2)
        listen_time++;
      /* do me a favour and don't change this back to a CTCP reply,
       * CTCP replies are NOTICE's this has to be a PRIVMSG
       * -poptix 5/1/1997 */
      bd::String msg;
      msg = bd::String::printf("\001DCC CHAT chat %lu %u\001", iptolong(getmyip()), dcc[ix].port);
      privmsg(nick, msg, DP_SERVER);
    }
    return BIND_RET_BREAK;
}

static cmd_t myctcp[] =
{
  {"CLIENTINFO", 	"", 	(Function) ctcp_CLIENTINFO, 	NULL, LEAF},
  {"FINGER", 		"", 	(Function) ctcp_FINGER, 	NULL, LEAF},
  {"WHOAMI", 		"", 	(Function) ctcp_WHOAMI, 	NULL, LEAF},
  {"OP", 		"", 	(Function) ctcp_OP, 		NULL, LEAF},
  {"OPS", 		"", 	(Function) ctcp_OP, 		NULL, LEAF},
  {"INVITE", 		"",	(Function) ctcp_INVITE_UNBAN, 	NULL, LEAF},
  {"UNBAN", 		"", 	(Function) ctcp_INVITE_UNBAN, 	NULL, LEAF},
  {"ERRMSG", 		"", 	(Function) ctcp_ECHO, 		NULL, LEAF},
  {"USERINFO", 		"", 	(Function) ctcp_USERINFO, 	NULL, LEAF},
  {"ECHO", 		"", 	(Function) ctcp_ECHO, 		NULL, LEAF},
  {"VERSION", 		"", 	(Function) ctcp_VERSION, 	NULL, LEAF},
  {"PING", 		"", 	(Function) ctcp_PING, 		NULL, LEAF},
  {"TIME", 		"", 	(Function) ctcp_TIME, 		NULL, LEAF},
  {"CHAT",		"",	(Function) ctcp_CHAT,		NULL, LEAF},
  {NULL,		NULL,	NULL,			NULL, 0}
};

void ctcp_init()
{
  char *p = NULL;
  struct utsname un;

  bzero(&un, sizeof(un));
  if (!uname(&un)) {
    strlcpy(cloak_os, un.sysname, sizeof(cloak_os));
    strlcpy(cloak_osver, un.release, sizeof(cloak_osver));
    strlcpy(cloak_host, un.nodename, sizeof(cloak_host));
  } else {
    /* shit, we have to come up with something ourselves.. */
    switch (randint(2)) {
    case 0:
      strlcpy(cloak_os, "Linux", sizeof(cloak_os));
      strlcpy(cloak_osver, "2.6.25.5", sizeof(cloak_osver));
      break;
    case 1:
      strlcpy(cloak_os, "FreeBSD", sizeof(cloak_os));
      strlcpy(cloak_osver, "7.0-p4", sizeof(cloak_osver));
      break;
    }
    strlcpy(cloak_host, "login", sizeof(cloak_host));
  }
  if ((p = strchr(cloak_host, '.')))
    *p = 0;

  switch (randint(4)) {
  case 0:
    strlcpy(cloak_bxver, "1.1-final", sizeof(cloak_bxver));
    break;
  case 1:
    strlcpy(cloak_bxver, "1.0c18", sizeof(cloak_bxver));
    break;
  case 2:
    strlcpy(cloak_bxver, "1.0c19", sizeof(cloak_bxver));
    break;
  case 3:
    strlcpy(cloak_bxver, "1.0c20cvs+", sizeof(cloak_bxver));
    break;
  }
  scriptchanged();

  add_builtins("ctcp", myctcp);

  timer_create_secs(60, "ctcp_minutely", (Function) ctcp_minutely);
}
