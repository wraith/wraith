/*
 * ctcp.c -- part of ctcp.mod
 *   all the ctcp handling (except DCC, it's special ;)
 *
 */

#include "ctcp.h"
#include "src/common.h"
#include "src/response.h"
#include "src/main.h"
#include "src/cfg.h"
#include "src/chanprog.h"
#include "src/cmds.h"
#include "src/misc.h"
#include "src/net.h"
#include "src/userrec.h"
#include "src/botmsg.h"
#include "src/tclhash.h"
#include "src/egg_timer.h"

#ifdef LEAF
#include "src/mod/server.mod/server.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/utsname.h>
#include <ctype.h>
#endif /* LEAF */

int cloak_script = CLOAK_PLAIN;

#ifdef LEAF
#define AVGAWAYTIME             60
#define AVGHERETIME             5
time_t cloak_awaytime = 0;
time_t cloak_heretime = 0;
time_t listen_time = 0;
char cloak_bxver[10] = "";
char cloak_os[20] = "";
char cloak_osver[100] = "";
char cloak_host[161] = "";
char ctcpversion[200] = "";
char ctcpversion2[200] = "";
char ctcpuserinfo[200] = "";
char autoaway[100] = "";
char kickprefix[25] = "";
char bankickprefix[25] = "";

void scriptchanged()
{
  char tmp[200] = "", *p = NULL;

  ctcpversion[0] = ctcpversion2[0] = ctcpuserinfo[0] = autoaway[0] = kickprefix[0] = bankickprefix[0] = 0;
  
  switch (cloak_script) {
  case CLOAK_PLAIN:
    sprintf(ctcpversion, "\002BitchX-%s\002 by panasync - %s %s : \002Keep it to yourself!\002", cloak_bxver, cloak_os, cloak_osver);
    strcpy(ctcpuserinfo, "");
    strcpy(autoaway, "Auto-Away after 10 mins");
    strcpy(kickprefix, "");
    strcpy(bankickprefix, "");
    break;
  case CLOAK_CRACKROCK:
    sprintf(ctcpversion, "BitchX-%s\002/\002%s %s:(\002c\002)\037rackrock\037/\002b\002X \037[\0373.0.1á9\037]\037 :\002 Keep it to yourself!\002", cloak_bxver, cloak_os, cloak_osver);
    strcpy(ctcpuserinfo, "crack addict, help me.");
    strcpy(autoaway, "automatically dead");
    strcpy(kickprefix, "\002c\002/\037k\037: ");
    strcpy(bankickprefix, "\002c\002/\037kb\037: ");
    break;
  case CLOAK_NEONAPPLE:
    sprintf(tmp, "%s %s", cloak_os, cloak_osver);
    strtolower(tmp);
    sprintf(ctcpversion, "bitchx-%s\037(\037%s\037):\037 \002n\002eon\037a\037ppl\002e\002\037/\037\002v\0020\037.\03714i : \002d\002ont you wish you had it\037?\037", cloak_bxver, tmp);
    strcpy(ctcpuserinfo, "neon apple");
    strcpy(autoaway, "automatically away after 10 mins \037(\037\002n\002/\037a)\037");
    strcpy(kickprefix, "\037[na\002(\037k\037)\002]\037 ");
    strcpy(bankickprefix, "");
    break;
  case CLOAK_TUNNELVISION:
    strcpy(tmp, cloak_bxver);
    p = tmp;
    p += strlen(tmp) - 1;
    p[1] = p[0];
    p[0] = '\037';
    p[2] = '\037';
    p[3] = 0;
    sprintf(ctcpversion, "\002b\002itchx-%s :tunnel\002vision\002/\0371.2\037", tmp);
    strcpy(ctcpuserinfo, "");
    strcpy(autoaway, "auto-gone");
    strcpy(kickprefix, "");
    strcpy(bankickprefix, "");
    break;
  case CLOAK_ARGON:
    sprintf(ctcpversion, ".\037.(\037argon\002/\0021g\037)\037 \002:\002bitchx-%s", cloak_bxver);
    strcpy(ctcpuserinfo, "");
    strcpy(autoaway, "\037(\037ar\037)\037 auto-away \037(\03710m\037)\037");
    strcpy(kickprefix, "\037(\037ar\037)\037 ");
    strcpy(bankickprefix, "\037(\037ar\037)\037 ");
    break;
  case CLOAK_EVOLVER:
    sprintf(ctcpversion, "\037evolver\037(\00202x9\002)\037: bitchx\037(\002%s\002) \037í\037 %s\002/\002%s : eye yam pheerable now!", cloak_bxver, cloak_os, cloak_osver);
    strcpy(ctcpuserinfo, "");
    strcpy(autoaway, "[\037\002i\002dle for \037[\03710 minutes\037]]");
    strcpy(kickprefix, "\037ev\002!\002k\037 ");
    strcpy(bankickprefix, "\037ev\002!\002bk\037 ");
    break;
  case CLOAK_PREVAIL:
    sprintf(ctcpversion, "%s\037!\037%s bitchx-%s \002-\002 prevail\037[\0370123\037]\037 :down with people", cloak_os, cloak_osver, cloak_bxver);
    strcpy(ctcpuserinfo, botrealname);
    strcpy(autoaway, "idle 10 minutes \037-\037 gone\037!\037");
    strcpy(kickprefix, "\037[\037pv\037!\037k\037]\037 ");
    strcpy(bankickprefix, "\037[\037pv\037!\037bk\037]\037 ");
    break;
  case CLOAK_MIRC:
  {
    char mircver[5] = "";
   
    strcpy(mircver, response(RES_MIRCVER));
    sprintf(ctcpversion, "mIRC v%s Khaled Mardam-Bey", mircver);
    if (randint(2) % 2)
      strcpy(ctcpversion2, response(RES_MIRCSCRIPT));
    strcpy(ctcpuserinfo, botrealname);
    strcpy(autoaway, "auto-away after 10 minutes");
    strcpy(kickprefix, "");
    strcpy(bankickprefix, "");
    break;
  }
  case CLOAK_CYPRESS:
  {
    char theme[30] = "";

    switch (randint(25)) { /* 0-19 = script, 20-24 = plain */
    case 0:
      strcpy(theme, " \037.\037.\002BX\002");
      break;
    case 1:
      strcpy(theme, " \037.\037.chl\037o\037rine");
      break;
    case 2:
      strcpy(theme, " \037.\037.\037<\037c\002x\002\037>\037");
      break;
    case 3:
      strcpy(theme, " \037.\037.supercyan");
      break;
    case 4:
      strcpy(theme, " \037.\037.\037c\037yan\002i\002\002\037z\037\002\037e\037d");
      break;
    case 5:
      strcpy(theme, " \037.\037.delusion");
      break;
    case 6:
      strcpy(theme, " \037.\037.\002e\002mbryonic");
      break;
    case 7:
      strcpy(theme, " \037.\037.e\002x\002tra\037.\037terrestr\037i\037al");
      break;
    case 8:
      strcpy(theme, " \037.\037.\002f\002ad\037e\037d");
      break;
    case 9:
      strcpy(theme, " \037.\037.fo\037c\037us");
      break;
    case 10:
      strcpy(theme, " \037.\037.\002h\002ade\037s\037");
      break;
    case 11:
      strcpy(theme, " \037.\037.hellbent\037.\037");
      break;
    case 12:
      strcpy(theme, " \037.\037.illusi\037o\037n");
      break;
    case 13:
      strcpy(theme, " \037.\037.\037j\037ungl\037e\037");
      break;
    case 14:
      strcpy(theme, " \037.\037.\002l\002abry\037i\037nth");
      break;
    case 15:
      strcpy(theme, " \037.\037.nightblue");
      break;
    case 16:
      strcpy(theme, " \037.\037.\037o\037bli\037v\037io\037n\037");
      break;
    case 17:
      strcpy(theme, " \037.\037.ph\002a\002ze");
      break;
    case 18:
      strcpy(theme, " \037.\037.sphere");
      break;
    case 19:
      strcpy(theme, " \037.\037.zip");
      break;
    default:
      strcpy(theme, "");
      break;
    }
    switch (randint(16)) {
    case 0:
      sprintf(ctcpversion, "bitchx\037-\037%s \037/\037 cypress\037.\03701i%s", cloak_bxver, theme);
      break;
    case 1:
      sprintf(ctcpversion, "cypress\037.\03701i%s \037/\037 bitchx\037-\037%s", theme, cloak_bxver);
      break;
    case 2:
      sprintf(tmp, "%s %s", cloak_os, cloak_osver);
      strtolower(tmp);
      sprintf(ctcpversion, "cypress\037.\03701i%s %s\037(\037%s\037)\037 bitchx\037-\037%s)", theme, tmp, cloak_host, cloak_bxver);
      break;
    case 3:
      sprintf(tmp, "%s %s", cloak_os, cloak_osver);
      strtolower(tmp);
      sprintf(ctcpversion, "bitchx\037-\037%s %s\037(\037%s\037)\037 cypress\037.\03701i%s", cloak_bxver, tmp, cloak_host, theme);
      break;
    case 4:
      sprintf(ctcpversion, "%s\002/\002%s: BitchX-%s \002[\002cypress\002]\002 v01i%s", cloak_os, cloak_osver, cloak_bxver, theme);
      break;
    case 5:
      p = replace(cloak_osver, ".", "\037.\037");
      sprintf(tmp, "%s %s", cloak_os, p);
      strtolower(tmp);
      sprintf(ctcpversion, "\037.\037.cypress\037.\03701i%s %s\037(\037%s\037)\037 bitchx\037/\037%s",theme, tmp, cloak_host, cloak_bxver);
      break;
    case 6:
      p = replace(cloak_osver, ".", "\037.\037");
      sprintf(tmp, "%s %s", cloak_os, p);
      strtolower(tmp);
      sprintf(ctcpversion, "cypress\002.\00201i%s\037(\037bitchx\002.\002%s\037)\037\002.\002. %s\037(\037%s\037)\037",theme, cloak_bxver, tmp, cloak_host);
      break;
    case 7:
      p = replace(cloak_osver, ".", "\037.\037");
      sprintf(tmp, "%s %s", cloak_os, p);
      strtolower(tmp);
      sprintf(ctcpversion, "\037.\037.cypress\037.\03701i%s - bitchx\037.\037%s\002/\002%s", theme, cloak_bxver, tmp);
      break;
    case 8:
      sprintf(ctcpversion, "\002BitchX-%s\002 by panasync \002-\002 %s %s", cloak_bxver, cloak_os, cloak_osver);
      break;
    case 9:
      sprintf(ctcpversion, "\037[\037cypress\002/\00201i\037]\037 - %s \037[\037bx\002/\002%s\037]\037", theme, cloak_bxver);
      break;
    case 10:
      sprintf(ctcpversion, "\037[\037\002b\002itchx\002.\002%s\037]\037 \002+\002 \037[\037cypress\002.\00201i\037]\037 %s",cloak_bxver, theme);
      break;
    case 11:
      sprintf(ctcpversion, "\037[\037BitchX\002/\002%s\037(\037cypress\002/\00201i\037)]\037 %s", cloak_bxver, theme);
      break;
    case 12:
      strtolower(cloak_os);
      sprintf(ctcpversion, "bitchx\037/\037%s %s %s \037(\037cypress\037/\03701i\037)\037 %s", cloak_bxver, cloak_os, cloak_osver, theme);
      break;
    case 13:
      sprintf(ctcpversion, "\037.\037.cypress\037/\03701i\037!\037bitchx\037/\037%s\037.\037.%s", cloak_bxver, theme);
      break;
    case 14:
      strtolower(cloak_bxver);
      sprintf(ctcpversion, "cypress\002\037.\037\002\037.\03701i\002/\002bitchx\037.\037\002\037.\037\002%s%s", cloak_bxver, theme);
      break;
    case 15:
      strtolower(cloak_bxver);
      sprintf(ctcpversion, "cypress\037.\03701i\037/\037bx%s \037(\037%s\037)\037", cloak_bxver, theme);
      break;
    }
    strcpy(ctcpuserinfo, "");
    strcpy(autoaway, "autoaway after 40 min");
    strcpy(kickprefix, "\002.\002.\037(\037\002c\002yp\002/\002k\037)\037 ");
    strcpy(bankickprefix, "\002.\002.\037(\037\002c\002yp\002/\002bk\037)\037 ");
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
  case CLOAK_CRACKROCK:
    if (hrs)
      sprintf(awtime, "%dh %dm %ds", hrs, min, sec);
    else if (min)
      sprintf(awtime, "%dm %ds", min, sec);
    else
      sprintf(awtime, "%ds", sec);
    dprintf(DP_HELP, "AWAY :%s\002\037[\002%s\002]\037\002\n", autoaway, awtime);
    break;
  case CLOAK_TUNNELVISION:
    if (hrs)
      sprintf(awtime, "%dh%dm%ds", hrs, min, sec);
    else if (min)
      sprintf(awtime, "%dm%ds", min, sec);
    else
      sprintf(awtime, "%ds", sec);
    dprintf(DP_HELP, "AWAY :%s \037(\037%s\037)\037\n", autoaway, awtime);
    break;
  case CLOAK_ARGON:
    if (hrs)
      sprintf(awtime, "%dh%dm%ds", hrs, min, sec);
    else if (min)
      sprintf(awtime, "%dm%ds", min, sec);
    else
      sprintf(awtime, "%ds", sec);
    dprintf(DP_HELP, "AWAY :%s .\002.\002\037(\037%s\037)\037\n", autoaway, awtime);
    break;
  case CLOAK_EVOLVER:
    if (hrs)
      sprintf(awtime, "%dh %dm %ds", hrs, min, sec);
    else if (min)
      sprintf(awtime, "%dm %ds", min, sec);
    else
      sprintf(awtime, "%ds", sec);
    dprintf(DP_HELP, "AWAY :away\037: %s (\037l\002:\002off\037,\037 p\002:\002off\037)\037 \037[\037gone\002:\002%s\037]\037\n", autoaway, awtime);
    break;
  case CLOAK_PREVAIL:
    if (hrs)
      sprintf(awtime, "%dh%dm%ds", hrs, min, sec);
    else if (min)
      sprintf(awtime, "%dm%ds", min, sec);
    else
      sprintf(awtime, "%ds", sec);
    dprintf(DP_HELP, "AWAY :%s %s\n", autoaway, awtime);
    break;
  case CLOAK_CYPRESS:
    dprintf(DP_HELP, "AWAY :is gone\037.\037. %s \037.\037.\037[\037\002c\002yp\037(\037l\002/\002off\002.\002p\002/\002off)]\n", autoaway);
    break;
  }
}

static void ctcp_minutely()
{
  if (server_online) {
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
      if ((dcc[i].type->flags & DCT_LISTEN) && (!strcmp(dcc[i].nick, "(telnet)") || !strcmp(dcc[i].nick, "(telnet6)"))) {
        putlog(LOG_DEBUG, "*", "Closing listening port %d %s", dcc[i].port, dcc[i].nick);

        killsock(dcc[i].sock);
        lostdcc(i);
        break;
      }
    }
  } else
    listen_time--;
}

static int ctcp_FINGER(char *nick, char *uhost, struct userrec *u, char *object, char *keyword, char *text)
{
  time_t idletime = 0;

  if (cloak_awaytime)
    idletime = now - cloak_awaytime;
  else if (cloak_heretime)
    idletime = now - cloak_heretime;
  dprintf(DP_HELP, "NOTICE %s :\001%s %s (%s@%s) Idle %li second%s\001\n", nick, keyword, "",
                   conf.username ? conf.username : conf.bot->nick, 
                   (strchr(botuserhost, '@') + 1), idletime, idletime == 1 ? "" : "s");
  return BIND_RET_BREAK;
}

static int ctcp_ECHO(char *nick, char *uhost, struct userrec *u, char *object, char *keyword, char *text)
{
  char reply[60] = "";

  strncpyz(reply, text, sizeof(reply));
  dprintf(DP_HELP, "NOTICE %s :\001%s %s\001\n", nick, keyword, reply);
  return BIND_RET_BREAK;
}
static int ctcp_PING(char *nick, char *uhost, struct userrec *u, char *object, char *keyword, char *text)
{
  if (strlen(text) <= 80)       /* bitchx ignores > 80 */
    dprintf(DP_HELP, "NOTICE %s :\001%s %s\001\n", nick, keyword, text);
  return BIND_RET_BREAK;
}

static int ctcp_VERSION(char *nick, char *uhost, struct userrec *u, char *object, char *keyword, char *text)
{
  char s[50] = "";

  if (cloak_script == CLOAK_CYPRESS) {
    switch (randint(8)) {
    case 0:
      strcpy(s, " :should of put the glock down.");
      break;
    case 1:
      strcpy(s, " :hot damn, I didn't want to kill a man.");
      break;
    case 2:
      strcpy(s, " :check me and I'll check ya back.");
      break;
    case 3:
      strcpy(s, " :put the blunt down just for a minute.");
      break;
    case 4:
      strcpy(s, " :tried to jack me, my homie got shot.");
      break;
    case 5:
      strcpy(s, " :insane in the membrane");
      break;
    case 6:
      strcpy(s, " :slow hits from the bong");
      break;
    case 7:
      strcpy(s, " :k\002-\002leet");
      break;
    }
  }
  dprintf(DP_HELP, "NOTICE %s :\001%s %s%s\001\n", nick, keyword, ctcpversion, s);
  if (ctcpversion2[0])
    dprintf(DP_HELP, "NOTICE %s :\001%s %s\001\n", nick, keyword, ctcpversion2);
  return BIND_RET_BREAK;
}

static int ctcp_WHOAMI(char *nick, char *uhost, struct userrec *u, char *object, char *keyword, char *text)
{
  dprintf(DP_HELP, "NOTICE %s :\002BitchX\002: Access Denied\n", nick);
  return BIND_RET_BREAK;
}

static int ctcp_OP(char *nick, char *uhost, struct userrec *u, char *object, char *keyword, char *text)
{
  if (text[0]) {
    char chan[256] = "", *p = NULL;

    strncpyz(chan, text, sizeof(chan));
    p = strchr(chan, ' ');
    if (p)
      *p = 0;
    dprintf(DP_HELP, "NOTICE %s :\002BitchX\002: I'm not on %s or I'm not opped\n", nick, chan);
  }
  return BIND_RET_BREAK;
}

static int ctcp_INVITE_UNBAN(char *nick, char *uhost, struct userrec *u, char *object, char *keyword, char *text)
{
  if (text[0] == '#') {
    struct chanset_t *chan = chanset;
    char chname[256] = "", *p = NULL;

    strncpyz(chname, text, sizeof(chname));
    p = strchr(chname, ' ');
    if (p)
      *p = 0;
    while (chan) {
      if (chan->status & CHAN_ACTIVE) {
        if (!egg_strcasecmp(chan->name, chname)) {
          dprintf(DP_HELP, "NOTICE %s :\002BitchX\002: Access Denied\n", nick);
          return BIND_RET_LOG;
        }
      }
      chan = chan->next;
    }
    dprintf(DP_HELP, "NOTICE %s :\002BitchX\002: I'm not on that channel\n", nick);
  }
  return BIND_RET_BREAK;
}

static int ctcp_USERINFO(char *nick, char *uhost, struct userrec *u, char *object, char *keyword, char *text)
{
  if (cloak_script == CLOAK_TUNNELVISION)
    strcpy(ctcpuserinfo, botname);
  else if (cloak_script == CLOAK_PREVAIL) {
    strcpy(ctcpuserinfo, botname);
    strcat(ctcpuserinfo, " ?");
  }
  dprintf(DP_HELP, "NOTICE %s :\001%s %s\001\n", nick, keyword, ctcpuserinfo);
  return BIND_RET_BREAK;
}

static int ctcp_CLIENTINFO(char *nick, char *uhost, struct userrec *u, char *object, char *keyword, char *text)
{
  char buf[256] = "";

  if (!text[0]) {
    strcpy(buf, "SED UTC ACTION DCC CDCC BDCC XDCC VERSION CLIENTINFO USERINFO ERRMSG FINGER TIME PING ECHO INVITE WHOAMI OP OPS UNBAN IDENT XLINK UPTIME :Use CLIENTINFO <COMMAND> to get more specific information");
  } else if (!egg_strcasecmp(text, "UNBAN"))
    strcpy(buf, "UNBAN unbans the person from channel");
  else if (!egg_strcasecmp(text, "OPS"))
    strcpy(buf, "OPS ops the person if on userlist");
  else if (!egg_strcasecmp(text, "ECHO"))
    strcpy(buf, "ECHO returns the arguments it receives");
  else if (!egg_strcasecmp(text, "WHOAMI"))
    strcpy(buf, "WHOAMI user list information");
  else if (!egg_strcasecmp(text, "INVITE"))
    strcpy(buf, "INVITE invite to channel specified");
  else if (!egg_strcasecmp(text, "PING"))
    strcpy(buf, "PING returns the arguments it receives");
  else if (!egg_strcasecmp(text, "UTC"))
    strcpy(buf, "UTC substitutes the local timezone");
  else if (!egg_strcasecmp(text, "XDCC"))
    strcpy(buf, "XDCC checks cdcc info for you");
  else if (!egg_strcasecmp(text, "BDCC"))
    strcpy(buf, "BDCC checks cdcc info for you");
  else if (!egg_strcasecmp(text, "CDCC"))
    strcpy(buf, "CDCC checks cdcc info for you");
  else if (!egg_strcasecmp(text, "DCC"))
    strcpy(buf, "DCC requests a direct_client_connection");
  else if (!egg_strcasecmp(text, "ACTION"))
    strcpy(buf, "ACTION contains action descriptions for atmosphere");
  else if (!egg_strcasecmp(text, "FINGER"))
    strcpy(buf, "FINGER shows real name, login name and idle time of user");
  else if (!egg_strcasecmp(text, "ERRMSG"))
    strcpy(buf, "ERRMSG returns error messages");
  else if (!egg_strcasecmp(text, "USERINFO"))
    strcpy(buf, "USERINFO returns user settable information");
  else if (!egg_strcasecmp(text, "CLIENTINFO"))
    strcpy(buf, "CLIENTINFO gives information about available CTCP commands");
  else if (!egg_strcasecmp(text, "SED"))
    strcpy(buf, "SED contains simple_encrypted_data");
  else if (!egg_strcasecmp(text, "OP"))
    strcpy(buf, "OP ops the person if on userlist");
  else if (!egg_strcasecmp(text, "VERSION"))
    strcpy(buf, "VERSION shows client type, version and environment");
  else if (!egg_strcasecmp(text, "XLINK"))
    strcpy(buf, "XLINK x-filez rule");
  else if (!egg_strcasecmp(text, "IDENT"))
    strcpy(buf, "IDENT change userhost of userlist");
  else if (!egg_strcasecmp(text, "TIME"))
    strcpy(buf, "TIME tells you the time on the user's host");
  else if (!egg_strcasecmp(text, "UPTIME"))
    strcpy(buf, "UPTIME my uptime");
  else {
    dprintf(DP_HELP, "NOTICE %s :\001ERRMSG %s is not a valid function\001\n", nick, text);
    return BIND_RET_LOG;
  }
  dprintf(DP_HELP, "NOTICE %s :\001%s %s\001\n", nick, keyword, buf);
  return BIND_RET_BREAK;
}

static int ctcp_TIME(char *nick, char *uhost, struct userrec *u, char *object, char *keyword, char *text)
{
  char tms[81] = "";

  strncpyz(tms, ctime(&now), sizeof(tms));
  dprintf(DP_HELP, "NOTICE %s :\001%s %s\001\n", nick, keyword, tms);
  return BIND_RET_BREAK;
}


static int ctcp_CHAT(char *nick, char *uhost, struct userrec *u, char *object, char *keyword, char *text)
{

  if (!ischanhub()) 
    return BIND_RET_LOG;

    if (u_pass_match(u, "-")) {
      simple_sprintf(ctcp_reply, "%s\001ERROR no password set\001", ctcp_reply);
      return BIND_RET_BREAK;
    }

    int ix = -1;

    for (int i = 0; i < dcc_total; i++) {
      if ((dcc[i].type->flags & DCT_LISTEN) && (!strcmp(dcc[i].nick, "(telnet)")))
        ix = i;
    }
    if (dcc_total == max_dcc || (ix < 0 && (ix = listen_all(0, 0)) < 0))
      simple_sprintf(ctcp_reply, "%s\001ERROR no telnet port\001", ctcp_reply);
    else {
      if (listen_time <= 2)
        listen_time++;
      /* do me a favour and don't change this back to a CTCP reply,
       * CTCP replies are NOTICE's this has to be a PRIVMSG
       * -poptix 5/1/1997 */
      dprintf(DP_SERVER, "PRIVMSG %s :\001DCC CHAT chat %lu %u\001\n", nick, iptolong(getmyip()), dcc[ix].port);
    }
    return BIND_RET_BREAK;
}

static cmd_t myctcp[] =
{
  {"CLIENTINFO", 	"", 	(Function) ctcp_CLIENTINFO, 	NULL},
  {"FINGER", 		"", 	(Function) ctcp_FINGER, 		NULL},
  {"WHOAMI", 		"", 	(Function) ctcp_WHOAMI, 		NULL},
  {"OP", 		"", 	(Function) ctcp_OP, 		NULL},
  {"OPS", 		"", 	(Function) ctcp_OP, 		NULL},
  {"INVITE", 		"",	(Function) ctcp_INVITE_UNBAN, 	NULL},
  {"UNBAN", 		"", 	(Function) ctcp_INVITE_UNBAN, 	NULL},
  {"ERRMSG", 		"", 	(Function) ctcp_ECHO, 		NULL},
  {"USERINFO", 		"", 	(Function) ctcp_USERINFO, 		NULL},
  {"ECHO", 		"", 	(Function) ctcp_ECHO, 		NULL},
  {"VERSION", 		"", 	(Function) ctcp_VERSION, 		NULL},
  {"PING", 		"", 	(Function) ctcp_PING, 		NULL},
  {"TIME", 		"", 	(Function) ctcp_TIME, 		NULL},
  {"CHAT",		"",	(Function) ctcp_CHAT,		NULL},
  {NULL,		NULL,	NULL,			NULL}
};
#endif /* LEAF */

#ifdef HUB
static void cloak_describe(struct cfg_entry *cfgent, int idx)
{
  dprintf(idx, STR("cloak-script decides which BitchX script the bot cloaks. If set to 0, a random script will be cloaked.\n"));
  dprintf(idx, STR("Available: 1=plain bitchx, 2=crackrock, 3=neonapple, 4=tunnelvision, 5=argon, 6=evolver, 7=prevail 8=cypress 9=mIRC\n"));
}
#endif /* HUB */

static void cloak_changed(struct cfg_entry *cfgent, int *valid)
{
  char *p = NULL;

  if (!(p = cfgent->ldata ? cfgent->ldata : cfgent->gdata))
    return;

  int i = atoi(p);

#ifdef LEAF
  if (i == 0)
    i = randint(CLOAK_COUNT) + 1;
#endif /* LEAF */
  if ((*valid = ((i >= 0) && (i <= CLOAK_COUNT))))
    cloak_script = i;
#ifdef LEAF
  scriptchanged();
#endif /* LEAF */
}

struct cfg_entry CFG_CLOAK_SCRIPT = {
	"cloak-script", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
	cloak_changed, cloak_changed
#ifdef HUB
	, cloak_describe
#endif /* HUB */
};


void ctcp_init()
{
#ifdef LEAF
  char *p = NULL;
#ifndef CYGWIN_HACKS
  struct utsname un;

  egg_bzero(&un, sizeof(un));
  if (!uname(&un)) {
    strncpyz(cloak_os, un.sysname, sizeof(cloak_os));
    strncpyz(cloak_osver, un.release, sizeof(cloak_osver));
    strncpyz(cloak_host, un.nodename, sizeof(cloak_host));
  } else {
#endif /* !CYGWIN_HACKS */
    /* shit, we have to come up with something ourselves.. */
    switch (randint(2)) {
    case 0:
      strcpy(cloak_os, "Linux");
      strcpy(cloak_osver, "2.4.20");
      break;
    case 1:
      strcpy(cloak_os, "FreeBSD");
      strcpy(cloak_osver, "4.5-STABLE");
      break;
    }
    strcpy(cloak_host, "login");
#ifndef CYGWIN_HACKS
  }
#endif /* !CYGWIN_HACKS */
  if ((p = strchr(cloak_host, '.')))
    *p = 0;

  switch (randint(4)) {
  case 0:
    strcpy(cloak_bxver, "1.0c17");
    break;
  case 1:
    strcpy(cloak_bxver, "1.0c18");
    break;
  case 2:
    strcpy(cloak_bxver, "1.0c19");
    break;
  case 3:
    strcpy(cloak_bxver, "1.0c20cvs+");
    break;
  }
  scriptchanged();

  add_builtins("ctcp", myctcp);

  timer_create_secs(60, "ctcp_minutely", (Function) ctcp_minutely);
#endif /* LEAF */
}
