/*
 * ctcp.c -- part of ctcp.mod
 *   all the ctcp handling (except DCC, it's special ;)
 *
 */

#define MODULE_NAME "ctcp"
#define MAKING_CTCP
#include "ctcp.h"
#include "src/mod/module.h"

#ifdef LEAF
#include "server.mod/server.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef HAVE_UNAME
#include <sys/utsname.h>
#endif
#include <pwd.h>
#include <ctype.h>

static Function *global = NULL, *server_funcs = NULL;
#else
static Function *global = NULL;
#endif

#define CLOAK_COUNT             10 /* The number of scripts currently existing */
#define CLOAK_PLAIN             1 /* This is your plain bitchx client behaviour */
#define CLOAK_CRACKROCK         2
#define CLOAK_NEONAPPLE         3
#define CLOAK_TUNNELVISION      4
#define CLOAK_ARGON             5
#define CLOAK_EVOLVER           6
#define CLOAK_PREVAIL           7
#define CLOAK_CYPRESS           8 /* Now with full theme and customization support */
#define CLOAK_MIRC              9

int cloak_script = CLOAK_PLAIN;

#ifdef LEAF
#ifdef S_AUTOAWAY
#define AVGAWAYTIME             60
#define AVGHERETIME             5
#endif
int cloak_awaytime = 0;
int cloak_heretime = 0;
int listen_time = 0;
char cloak_bxver[10];
char cloak_os[20];
char cloak_osver[100];
char cloak_host[161];
char ctcpversion[400];
char ctcpuserinfo[400];
char autoaway[100];

char *strtolower(char *s)
{
  char *p,
    *p2 = nmalloc(strlen(s) + 1);

  strcpy(p2, s);
  p = p2;
  while (*p) {
    *p = tolower(*p);
    p++;
  }
  return p2;
}

/* HAH
char *underscoredots(char *s) // amazingly silly function...
{
  int size = strlen(s) + 1;
  char *p, *p2;

Context;
  p = nmalloc(size);
  p2 = nmalloc(size);
  strcpy(p, s);
  while (p && *p) {
    if (!strncmp(p, ".", 1)) {
      size += 2;
Context;
      p2 = nrealloc(p2, size);
      strcat(p2, STR("\037.\037"));
    } else
      strncat(p2, p, 1);
    p++;
  }
  return p2;
}
*/

void scriptchanged()
{
  char tmp[200],
    *p;

  switch (cloak_script) {
  case CLOAK_PLAIN:
    sprintf(ctcpversion, STR("\002BitchX-%s\002 by panasync - %s %s : \002Keep it to yourself!\002"), cloak_bxver, cloak_os, cloak_osver);
    strcpy(ctcpuserinfo, "");
    strcpy(autoaway, STR("Auto-Away after 10 mins"));
    strcpy(kickprefix, "");
    strcpy(bankickprefix, "");
    break;
  case CLOAK_CRACKROCK:
    sprintf(ctcpversion, STR("BitchX-%s\002/\002%s %s:(\002c\002)\037rackrock\037/\002b\002X \037[\0373.0.1á9\037]\037 :\002 Keep it to yourself!\002"), cloak_bxver, cloak_os, cloak_osver);
    strcpy(ctcpuserinfo, STR("crack addict, help me."));
    strcpy(autoaway, STR("automatically dead"));
    strcpy(kickprefix, STR("\002c\002/\037k\037: "));
    strcpy(bankickprefix, STR("\002c\002/\037kb\037: "));
    break;
  case CLOAK_NEONAPPLE:
    sprintf(tmp, STR("%s %s"), cloak_os, cloak_osver);
    p = strtolower(tmp);
    sprintf(ctcpversion, STR("bitchx-%s\037(\037%s\037):\037 \002n\002eon\037a\037ppl\002e\002\037/\037\002v\0020\037.\03714i : \002d\002ont you wish you had it\037?\037"), cloak_bxver, p);
    nfree(p);
    strcpy(ctcpuserinfo, STR("neon apple"));
    strcpy(autoaway, STR("automatically away after 10 mins \037(\037\002n\002/\037a)\037"));
    strcpy(kickprefix, STR("\037[na\002(\037k\037)\002]\037 "));
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
    sprintf(ctcpversion, STR("\002b\002itchx-%s :tunnel\002vision\002/\0371.2\037"), tmp);
    strcpy(ctcpuserinfo, "");
    strcpy(autoaway, STR("auto-gone"));
    strcpy(kickprefix, "");
    strcpy(bankickprefix, "");
    break;
  case CLOAK_ARGON:
    sprintf(ctcpversion, STR(".\037.(\037argon\002/\0021g\037)\037 \002:\002bitchx-%s"), cloak_bxver);
    strcpy(ctcpuserinfo, "");
    strcpy(autoaway, STR("\037(\037ar\037)\037 auto-away \037(\03710m\037)\037"));
    strcpy(kickprefix, STR("\037(\037ar\037)\037 "));
    strcpy(bankickprefix, STR("\037(\037ar\037)\037 "));
    break;
  case CLOAK_EVOLVER:
    sprintf(ctcpversion, STR("\037evolver\037(\00202x9\002)\037: bitchx\037(\002%s\002) \037í\037 %s\002/\002%s : eye yam pheerable now!"), cloak_bxver, cloak_os, cloak_osver);
    strcpy(ctcpuserinfo, "");
    strcpy(autoaway, STR("[\037\002i\002dle for \037[\03710 minutes\037]]"));
    strcpy(kickprefix, STR("\037ev\002!\002k\037 "));
    strcpy(bankickprefix, STR("\037ev\002!\002bk\037 "));
    break;
  case CLOAK_PREVAIL:
    sprintf(ctcpversion, STR("%s\037!\037%s bitchx-%s \002-\002 prevail\037[\0370123\037]\037 :down with people"), cloak_os, cloak_osver, cloak_bxver);
    strcpy(ctcpuserinfo, botrealname);
    strcpy(autoaway, STR("idle 10 minutes \037-\037 gone\037!\037"));
    strcpy(kickprefix, STR("\037[\037pv\037!\037k\037]\037 "));
    strcpy(bankickprefix, STR("\037[\037pv\037!\037bk\037]\037 "));
    break;
  case CLOAK_MIRC:
  {
    char mircver[4];

    switch (random() % 7) {
      case 0:
        strcpy(mircver, "6.01");
        break;
      case 1:
        strcpy(mircver, "6.02");
        break;
      case 2:
        strcpy(mircver, "6.03");
        break;
      case 3:
        strcpy(mircver, "6.1");
        break;
      case 4:
        strcpy(mircver, "5.91");
        break;
      case 5:   
        strcpy(mircver, "6.11");
        break;
      case 6:   
        strcpy(mircver, "6.12");
        break;
      default:
        strcpy(mircver, "");
    }
    sprintf(ctcpversion, STR("mIRC v%s Khaled Mardam-Bey"), mircver);
    strcpy(ctcpuserinfo, botrealname);
    strcpy(autoaway, STR("auto-away after 10 minutes"));
    strcpy(kickprefix, "");
    strcpy(bankickprefix, "");
    break;
  }
  case CLOAK_CYPRESS:
  {
    char theme[30];

    switch (random() % 25) { /* 0-19 = script, 20-24 = plain */
    case 0:
      strcpy(theme, STR(" \037.\037.\002BX\002"));
      break;
    case 1:
      strcpy(theme, STR(" \037.\037.chl\037o\037rine"));
      break;
    case 2:
      strcpy(theme, STR(" \037.\037.\037<\037c\002x\002\037>\037"));
      break;
    case 3:
      strcpy(theme, STR(" \037.\037.supercyan"));
      break;
    case 4:
      strcpy(theme, STR(" \037.\037.\037c\037yan\002i\002\002\037z\037\002\037e\037d"));
      break;
    case 5:
      strcpy(theme, STR(" \037.\037.delusion"));
      break;
    case 6:
      strcpy(theme, STR(" \037.\037.\002e\002mbryonic"));
      break;
    case 7:
      strcpy(theme, STR(" \037.\037.e\002x\002tra\037.\037terrestr\037i\037al"));
      break;
    case 8:
      strcpy(theme, STR(" \037.\037.\002f\002ad\037e\037d"));
      break;
    case 9:
      strcpy(theme, STR(" \037.\037.fo\037c\037us"));
      break;
    case 10:
      strcpy(theme, STR(" \037.\037.\002h\002ade\037s\037"));
      break;
    case 11:
      strcpy(theme, STR(" \037.\037.hellbent\037.\037"));
      break;
    case 12:
      strcpy(theme, STR(" \037.\037.illusi\037o\037n"));
      break;
    case 13:
      strcpy(theme, STR(" \037.\037.\037j\037ungl\037e\037"));
      break;
    case 14:
      strcpy(theme, STR(" \037.\037.\002l\002abry\037i\037nth"));
      break;
    case 15:
      strcpy(theme, STR(" \037.\037.nightblue"));
      break;
    case 16:
      strcpy(theme, STR(" \037.\037.\037o\037bli\037v\037io\037n\037"));
      break;
    case 17:
      strcpy(theme, STR(" \037.\037.ph\002a\002ze"));
      break;
    case 18:
      strcpy(theme, STR(" \037.\037.sphere"));
      break;
    case 19:
      strcpy(theme, STR(" \037.\037.zip"));
      break;
    default:
      strcpy(theme, STR(""));
      break;
    }
    switch (random() % 16) {
    case 0:
      sprintf(ctcpversion, STR("bitchx\037-\037%s \037/\037 cypress\037.\03701i%s"), cloak_bxver, theme);
      break;
    case 1:
      sprintf(ctcpversion, STR("cypress\037.\03701i%s \037/\037 bitchx\037-\037%s"), theme, cloak_bxver);
      break;
    case 2:
      sprintf(tmp, STR("%s %s"), cloak_os, cloak_osver);
      p = strtolower(tmp);
      sprintf(ctcpversion, STR("cypress\037.\03701i%s %s\037(\037%s\037)\037 bitchx\037-\037%s)"), theme, p, cloak_host, cloak_bxver);
      nfree(p);
      break;
    case 3:
      sprintf(tmp, STR("%s %s"), cloak_os, cloak_osver);
      p = strtolower(tmp);
      sprintf(ctcpversion, STR("bitchx\037-\037%s %s\037(\037%s\037)\037 cypress\037.\03701i%s"), cloak_bxver, p, cloak_host, theme);
      nfree(p);
      break;
    case 4:
      sprintf(ctcpversion, STR("%s\002/\002%s: BitchX-%s \002[\002cypress\002]\002 v01i%s"), cloak_os, cloak_osver, cloak_bxver, theme);
      break;
    case 5:
//      p = underscoredots(cloak_osver);
//      sprintf(tmp, STR("%s %s"), cloak_os, p);
//      nfree(p);
      sprintf(tmp, STR("%s"), cloak_os);
      p = strtolower(tmp);
      sprintf(ctcpversion, STR("\037.\037.cypress\037.\03701i%s %s\037(\037%s\037)\037 bitchx\037/\037%s"),theme, p, cloak_host, cloak_bxver);
      nfree(p);
      break;
    case 6:
//      p = underscoredots(cloak_osver);
//      sprintf(tmp, STR("%s %s"), cloak_os, p);
//      nfree(p);
      sprintf(tmp, STR("%s"), cloak_os);
      p = strtolower(tmp);
      sprintf(ctcpversion, STR("cypress\002.\00201i%s\037(\037bitchx\002.\002%s\037)\037\002.\002. %s\037(\037%s\037)\037"),theme, cloak_bxver, p, cloak_host);
      nfree(p);
      break;
    case 7:
//      p = underscoredots(cloak_osver);
//      sprintf(tmp, STR("%s %s"), cloak_os, p);
//      nfree(p);
      sprintf(tmp, STR("%s"), cloak_os);
      p = strtolower(tmp);
      sprintf(ctcpversion, STR("\037.\037.cypress\037.\03701i%s - bitchx\037.\037%s\002/\002%s"), theme, cloak_bxver, p);
      nfree(p);
      break;
    case 8:
      sprintf(ctcpversion, STR("\002BitchX-%s\002 by panasync \002-\002 %s %s"), cloak_bxver, cloak_os, cloak_osver);
      break;
    case 9:
      sprintf(ctcpversion, STR("\037[\037cypress\002/\00201i\037]\037 - %s \037[\037bx\002/\002%s\037]\037"), theme, cloak_bxver);
      break;
    case 10:
      sprintf(ctcpversion, STR("\037[\037\002b\002itchx\002.\002%s\037]\037 \002+\002 \037[\037cypress\002.\00201i\037]\037 %s"),cloak_bxver, theme);
      break;
    case 11:
      sprintf(ctcpversion, STR("\037[\037BitchX\002/\002%s\037(\037cypress\002/\00201i\037)]\037 %s"), cloak_bxver, theme);
      break;
    case 12:
      p = strtolower(cloak_os);
      sprintf(ctcpversion, STR("bitchx\037/\037%s %s %s \037(\037cypress\037/\03701i\037)\037 %s"), cloak_bxver, p, cloak_osver, theme);
      nfree(p);
      break;
    case 13:
      sprintf(ctcpversion, STR("\037.\037.cypress\037/\03701i\037!\037bitchx\037/\037%s\037.\037.%s"), cloak_bxver, theme);
      break;
    case 14:
      p = strtolower(cloak_bxver);
      sprintf(ctcpversion, STR("cypress\002\037.\037\002\037.\03701i\002/\002bitchx\037.\037\002\037.\037\002%s%s"), p, theme);
      nfree(p);
      break;
    case 15:
      p = strtolower(cloak_bxver);
      sprintf(ctcpversion, STR("cypress\037.\03701i\037/\037bx%s \037(\037%s\037)\037"), p, theme);
      nfree(p);
      break;
    }
    strcpy(ctcpuserinfo, STR(""));
    strcpy(autoaway, STR("autoaway after 40 min"));
    strcpy(kickprefix, STR("\002.\002.\037(\037\002c\002yp\002/\002k\037)\037 "));
    strcpy(bankickprefix, STR("\002.\002.\037(\037\002c\002yp\002/\002bk\037)\037 "));
    break;
  }
  }
}

#ifdef S_AUTOAWAY
void sendaway()
{
  char awtime[20];
  int hrs,
    min,
    sec,
    gt;

  gt = time(NULL) - cloak_awaytime;
  hrs = gt / 3600;
  min = (gt % 3600) / 60;
  sec = gt % 60;
  switch (cloak_script) {
  case CLOAK_PLAIN:
    dprintf(DP_HELP, STR("AWAY :is away: (%s) [\002BX\002-MsgLog Off]\n"), autoaway);
    break;
  case CLOAK_MIRC:
    dprintf(DP_HELP, STR("AWAY :is away: (%s)\n"), autoaway);
    break;
  case CLOAK_CRACKROCK:
    if (hrs)
      sprintf(awtime, STR("%dh %dm %ds"), hrs, min, sec);
    else if (min)
      sprintf(awtime, STR("%dm %ds"), min, sec);
    else
      sprintf(awtime, STR("%ds"), sec);
    dprintf(DP_HELP, STR("AWAY :%s\002\037[\002%s\002]\037\002\n"), autoaway, awtime);
    break;
  case CLOAK_TUNNELVISION:
    if (hrs)
      sprintf(awtime, STR("%dh%dm%ds"), hrs, min, sec);
    else if (min)
      sprintf(awtime, STR("%dm%ds"), min, sec);
    else
      sprintf(awtime, STR("%ds"), sec);
    dprintf(DP_HELP, STR("AWAY :%s \037(\037%s\037)\037\n"), autoaway, awtime);
    break;
  case CLOAK_ARGON:
    if (hrs)
      sprintf(awtime, STR("%dh%dm%ds"), hrs, min, sec);
    else if (min)
      sprintf(awtime, STR("%dm%ds"), min, sec);
    else
      sprintf(awtime, STR("%ds"), sec);
    dprintf(DP_HELP, STR("AWAY :%s .\002.\002\037(\037%s\037)\037\n"), autoaway, awtime);
    break;
  case CLOAK_EVOLVER:
    if (hrs)
      sprintf(awtime, STR("%dh %dm %ds"), hrs, min, sec);
    else if (min)
      sprintf(awtime, STR("%dm %ds"), min, sec);
    else
      sprintf(awtime, STR("%ds"), sec);
    dprintf(DP_HELP, STR("AWAY :away\037: %s (\037l\002:\002off\037,\037 p\002:\002off\037)\037 \037[\037gone\002:\002%s\037]\037\n"), autoaway, awtime);
    break;
  case CLOAK_PREVAIL:
    if (hrs)
      sprintf(awtime, STR("%dh%dm%ds"), hrs, min, sec);
    else if (min)
      sprintf(awtime, STR("%dm%ds"), min, sec);
    else
      sprintf(awtime, STR("%ds"), sec);
    dprintf(DP_HELP, STR("AWAY :%s %s\n"), autoaway, awtime);
    break;
  case CLOAK_CYPRESS:
    dprintf(DP_HELP, STR("AWAY :is gone\037.\037. %s \037.\037.\037[\037\002c\002yp\037(\037l\002/\002off\002.\002p\002/\002off)]\n"), autoaway);
    break;
  }
}
#endif

static void ctcp_minutely()
{
  int i;

#ifdef S_AUTOAWAY
  int n;

  if ((cloak_awaytime == 0) && (cloak_heretime == 0)) {
    cloak_heretime = time(NULL);
    dprintf(DP_HELP, STR("AWAY :\n"));
    return;
  }
  n = random();
  if (cloak_awaytime == 0) {
    if (!(n % AVGHERETIME)) {
      cloak_heretime = 0;
      cloak_awaytime = time(NULL) - 600 - random() % 60;
      sendaway();
    }
  } else {
    if (!(n % AVGAWAYTIME)) {
      cloak_awaytime = 0;
      cloak_heretime = time(NULL);
      dprintf(DP_HELP, STR("AWAY :\n"));
    } else
      sendaway();
  }
#endif


  if (listen_time <= 0) {
    for (i = 0; i < dcc_total; i++) {
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

static int ctcp_FINGER(char *nick, char *uhost, char *handle, char *object, char *keyword, char *text)
{
  char *p;
  int idletime;
  struct passwd *pwd;

  if (cloak_awaytime)
    idletime = now - cloak_awaytime;
  else if (cloak_heretime)
    idletime = now - cloak_heretime;
  else
    idletime = 0;
  if (!(pwd = getpwuid(geteuid())))
    return 0;
#ifndef GECOS_DELIMITER
#define GECOS_DELIMITER ','
#endif
  if ((p = strchr(pwd->pw_gecos, GECOS_DELIMITER)) != NULL)
    *p = 0;
  dprintf(DP_HELP, STR("NOTICE %s :\001%s %s (%s@%s) Idle %ld second%c\001\n"), nick, keyword, pwd->pw_gecos, pwd->pw_name, (char *) (strchr(botuserhost, '@') + 1), idletime, (idletime == 1) ? "" : "s");
  return 1;
}

static int ctcp_ECHO(char *nick, char *uhost, char *handle, char *object, char *keyword, char *text)
{
  char reply[60];

  strncpy0(reply, text, sizeof(reply));
  dprintf(DP_HELP, STR("NOTICE %s :\001%s %s\001\n"), nick, keyword, reply);
  return 1;
}
static int ctcp_PING(char *nick, char *uhost, char *handle, char *object, char *keyword, char *text)
{

  if (strlen(text) <= 80)       /* bitchx ignores > 80 */
    dprintf(DP_HELP, STR("NOTICE %s :\001%s %s\001\n"), nick, keyword, text);
  return 1;
}

static int ctcp_VERSION(char *nick, char *uhost, char *handle, char *object, char *keyword, char *text)
{
  char s[50] = "";

  if (cloak_script == CLOAK_CYPRESS) {
    switch (random() % 8) {
    case 0:
      strcpy(s, STR(" :should of put the glock down."));
      break;
    case 1:
      strcpy(s, STR(" :hot damn, I didn't want to kill a man."));
      break;
    case 2:
      strcpy(s, STR(" :check me and I'll check ya back."));
      break;
    case 3:
      strcpy(s, STR(" :put the blunt down just for a minute."));
      break;
    case 4:
      strcpy(s, STR(" :tried to jack me, my homie got shot."));
      break;
    case 5:
      strcpy(s, STR(" :insane in the membrane"));
      break;
    case 6:
      strcpy(s, STR(" :slow hits from the bong"));
      break;
    case 7:
      strcpy(s, STR(" :k\002-\002leet"));
      break;
    }
  }
  dprintf(DP_HELP, STR("NOTICE %s :\001%s %s%s\001\n"), nick, keyword, ctcpversion, s);
//if mirc send second reply here..

  return 1;
}

static int ctcp_WHOAMI(char *nick, char *uhost, char *handle, char *object, char *keyword, char *text)
{

  dprintf(DP_HELP, STR("NOTICE %s :\002BitchX\002: Access Denied\n"), nick);
  return 1;
}

static int ctcp_OP(char *nick, char *uhost, char *handle, char *object, char *keyword, char *text)
{
  char chan[256],
   *p;

  if (text[0]) {
    strncpy0(chan, text, sizeof(chan));
    p = strchr(chan, ' ');
    if (p)
      *p = 0;
    dprintf(DP_HELP, STR("NOTICE %s :\002BitchX\002: I'm not on %s or I'm not opped\n"), nick, chan);
  }
  return 1;
}
static int ctcp_INVITE_UNBAN(char *nick, char *uhost, char *handle, char *object, char *keyword, char *text)
{
  struct chanset_t *chan = chanset;
  char chname[256],
   *p;

  if (text[0] == '#') {
    strncpy0(chname, text, sizeof(chname));
    p = strchr(chname, ' ');
    if (p)
      *p = 0;
    while (chan) {
      if (chan->status & CHAN_ACTIVE) {
        if (!egg_strcasecmp(chan->name, chname)) {
          dprintf(DP_HELP, STR("NOTICE %s :\002BitchX\002: Access Denied\n"), nick);
          return 0;
        }
      }
      chan = chan->next;
    }
    dprintf(DP_HELP, STR("NOTICE %s :\002BitchX\002: I'm not on that channel\n"), nick);
  }
  return 1;
}

static int ctcp_USERINFO(char *nick, char *uhost, char *handle, char *object, char *keyword, char *text)
{

  if (cloak_script == CLOAK_TUNNELVISION)
    strcpy(ctcpuserinfo, botname);
  else if (cloak_script == CLOAK_PREVAIL) {
    strcpy(ctcpuserinfo, botname);
    strcat(ctcpuserinfo, " ?");
  }
  dprintf(DP_HELP, STR("NOTICE %s :\001%s %s\001\n"), nick, keyword, ctcpuserinfo);
  return 1;
}

static int ctcp_CLIENTINFO(char *nick, char *uhost, char *handle, char *object, char *keyword, char *msg)
{
  char text[256];

  if (!msg[0]) {
    strcpy(text, STR("SED UTC ACTION DCC CDCC BDCC XDCC VERSION CLIENTINFO USERINFO ERRMSG FINGER TIME PING ECHO INVITE WHOAMI OP OPS UNBAN IDENT XLINK UPTIME :Use CLIENTINFO <COMMAND> to get more specific information"));
  } else if (!egg_strcasecmp(msg, STR("UNBAN")))
    strcpy(text, STR("UNBAN unbans the person from channel"));
  else if (!egg_strcasecmp(msg, STR("OPS")))
    strcpy(text, STR("OPS ops the person if on userlist"));
  else if (!egg_strcasecmp(msg, STR("ECHO")))
    strcpy(text, STR("ECHO returns the arguments it receives"));
  else if (!egg_strcasecmp(msg, STR("WHOAMI")))
    strcpy(text, STR("WHOAMI user list information"));
  else if (!egg_strcasecmp(msg, STR("INVITE")))
    strcpy(text, STR("INVITE invite to channel specified"));
  else if (!egg_strcasecmp(msg, STR("PING")))
    strcpy(text, STR("PING returns the arguments it receives"));
  else if (!egg_strcasecmp(msg, STR("UTC")))
    strcpy(text, STR("UTC substitutes the local timezone"));
  else if (!egg_strcasecmp(msg, STR("XDCC")))
    strcpy(text, STR("XDCC checks cdcc info for you"));
  else if (!egg_strcasecmp(msg, STR("BDCC")))
    strcpy(text, STR("BDCC checks cdcc info for you"));
  else if (!egg_strcasecmp(msg, STR("CDCC")))
    strcpy(text, STR("CDCC checks cdcc info for you"));
  else if (!egg_strcasecmp(msg, STR("DCC")))
    strcpy(text, STR("DCC requests a direct_client_connection"));
  else if (!egg_strcasecmp(msg, STR("ACTION")))
    strcpy(text, STR("ACTION contains action descriptions for atmosphere"));
  else if (!egg_strcasecmp(msg, STR("FINGER")))
    strcpy(text, STR("FINGER shows real name, login name and idle time of user"));
  else if (!egg_strcasecmp(msg, STR("ERRMSG")))
    strcpy(text, STR("ERRMSG returns error messages"));
  else if (!egg_strcasecmp(msg, STR("USERINFO")))
    strcpy(text, STR("USERINFO returns user settable information"));
  else if (!egg_strcasecmp(msg, STR("CLIENTINFO")))
    strcpy(text, STR("CLIENTINFO gives information about available CTCP commands"));
  else if (!egg_strcasecmp(msg, STR("SED")))
    strcpy(text, STR("SED contains simple_encrypted_data"));
  else if (!egg_strcasecmp(msg, "OP"))
    strcpy(text, STR("OP ops the person if on userlist"));
  else if (!egg_strcasecmp(msg, STR("VERSION")))
    strcpy(text, STR("VERSION shows client type, version and environment"));
  else if (!egg_strcasecmp(msg, STR("XLINK")))
    strcpy(text, STR("XLINK x-filez rule"));
  else if (!egg_strcasecmp(msg, STR("IDENT")))
    strcpy(text, STR("IDENT change userhost of userlist"));
  else if (!egg_strcasecmp(msg, STR("TIME")))
    strcpy(text, STR("TIME tells you the time on the user's host"));
  else if (!egg_strcasecmp(msg, STR("UPTIME")))
    strcpy(text, STR("UPTIME my uptime"));
  else {
    dprintf(DP_HELP, STR("NOTICE %s :\001ERRMSG %s is not a valid function\001\n"), nick, msg);
    return 0;
  }
  dprintf(DP_HELP, STR("NOTICE %s :\001%s %s\001\n"), nick, keyword, text);
  return 1;
}

static int ctcp_TIME(char *nick, char *uhost, char *handle, char *object, char *keyword, char *text)
{
  char tms[81];

  strncpy0(tms, ctime(&now), sizeof(tms));
  dprintf(DP_HELP, STR("NOTICE %s :\001%s %s\001\n"), nick, keyword, tms);
  return 1;
}


static int ctcp_CHAT(char *nick, char *uhost, char *handle, char *object, char *keyword, char *text)
{

  struct userrec *u = get_user_by_handle(userlist, handle);
  int i, ix = (-1);

  if (!ischanhub())
    return 0;

    if (u_pass_match(u, "-")) {
      simple_sprintf(ctcp_reply, "%s\001ERROR no password set\001", ctcp_reply);
      return 1;
    }

    for (i = 0; i < dcc_total; i++) {
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
      dprintf(DP_HELP, "PRIVMSG %s :\001DCC CHAT chat %lu %u\001\n", nick, iptolong(getmyip()), dcc[ix].port);
    }



    return 1;
}

static cmd_t myctcp[] =
{
  {"CLIENTINFO", 	"", 	ctcp_CLIENTINFO, 	NULL},
  {"FINGER", 		"", 	ctcp_FINGER, 		NULL},
  {"WHOAMI", 		"", 	ctcp_WHOAMI, 		NULL},
  {"OP", 		"", 	ctcp_OP, 		NULL},
  {"OPS", 		"", 	ctcp_OP, 		NULL},
  {"INVITE", 		"",	ctcp_INVITE_UNBAN, 	NULL},
  {"UNBAN", 		"", 	ctcp_INVITE_UNBAN, 	NULL},
  {"ERRMSG", 		"", 	ctcp_ECHO, 		NULL},
  {"USERINFO", 		"", 	ctcp_USERINFO, 		NULL},
  {"ECHO", 		"", 	ctcp_ECHO, 		NULL},
  {"VERSION", 		"", 	ctcp_VERSION, 		NULL},
  {"PING", 		"", 	ctcp_PING, 		NULL},
  {"TIME", 		"", 	ctcp_TIME, 		NULL},
  {"CHAT",		"",	ctcp_CHAT,		NULL},
  {NULL,		NULL,	NULL,			NULL}
};
#endif /* LEAF */

void cloak_describe(struct cfg_entry *cfgent, int idx)
{
  dprintf(idx, STR("cloak-script decides which BitchX script the bot cloaks. If set to 0, a random script will be cloaked.\n"));
  dprintf(idx, STR("Available: 1=plain bitchx, 2=crackrock, 3=neonapple, 4=tunnelvision, 5=argon, 6=evolver, 7=prevail 8=cypress 9=mIRC\n"));
}

void cloak_changed(struct cfg_entry *cfgent, char *oldval, int *valid)
{
  char *p;
  int i;

  if (!(p = cfgent->ldata ? cfgent->ldata : cfgent->gdata))
    return;
  i = atoi(p);
#ifdef LEAF
  if (i == 0)
    i = (random() % CLOAK_COUNT) + 1;
#endif
  if ((*valid = ((i >= 0) && (i <= CLOAK_COUNT))))
    cloak_script = i;
#ifdef LEAF
  scriptchanged();
#endif
}

struct cfg_entry CFG_CLOAK_SCRIPT = {
  "cloak-script", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
  cloak_changed, cloak_changed, cloak_describe
};


EXPORT_SCOPE char *ctcp_start();

static Function ctcp_table[] =
{
  (Function) ctcp_start,
  (Function) NULL,
  (Function) NULL,
  (Function) NULL,
};

char *ctcp_start(Function * global_funcs)
{
#ifdef LEAF
  char *p;
#ifdef HAVE_UNAME
  struct utsname un;
#endif
#endif
  global = global_funcs;

  module_register(MODULE_NAME, ctcp_table, 1, 0);
#ifdef LEAF
  if (!(server_funcs = module_depend(MODULE_NAME, "server", 1, 0))) {
    module_undepend(MODULE_NAME);
    return "This module requires server module 1.0 or later.";
  }
#ifdef HAVE_UNAME
  egg_bzero(&un, sizeof(un));
  if (!uname(&un)) {
    strncpy0(cloak_os, un.sysname, sizeof(cloak_os));
    strncpy0(cloak_osver, un.release, sizeof(cloak_osver));
    strncpy0(cloak_host, un.nodename, sizeof(cloak_host));
  } else {
#endif /* HAVE_UNAME */
/* shit, we have to come up with something ourselves.. */
    switch (random() % 2) {
    case 0:
      strcpy(cloak_os, STR("Linux"));
      strcpy(cloak_osver, STR("2.4.20"));
      break;
    case 1:
      strcpy(cloak_os, STR("FreeBSD"));
      strcpy(cloak_osver, STR("4.5-STABLE"));
      break;
    }
    strcpy(cloak_host, STR("login"));
#ifdef HAVE_UNAME
  }
#endif /* HAVE_UNAME */
  if ((p = strchr(cloak_host, '.')))
    *p = 0;

  switch (random() % 4) {
  case 0:
    strcpy(cloak_bxver, STR("1.0c17"));
    break;
  case 1:
    strcpy(cloak_bxver, STR("1.0c18"));
    break;
  case 2:
    strcpy(cloak_bxver, STR("1.0c19"));
    break;
  case 3:
    strcpy(cloak_bxver, STR("1.0c20cvs+"));
    break;
  }
  scriptchanged();

  add_builtins(H_ctcp, myctcp);
  add_hook(HOOK_MINUTELY, (Function) ctcp_minutely);
#endif
  add_cfg(&CFG_CLOAK_SCRIPT);
  return NULL;
}

