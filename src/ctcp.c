
/* 
 * ctcp.c -- part of ctcp.mod
 *   all the ctcp handling (except DCC, it's special ;)
 * 
 * $Id: ctcp.c,v 1.5 2000/01/08 21:23:15 per Exp $
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

#define MODULE_NAME "ctcp"
#define MAKING_CTCP
#include "main.h"
#include "hook.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef HAVE_UNAME
#include <sys/utsname.h>
#endif

#define CLIENTINFO STR("SED VERSION CLIENTINFO USERINFO ERRMSG FINGER TIME ACTION DCC UTC PING ECHO  :Use CLIENTINFO <COMMAND> to get more specific information")
#define CLIENTINFO_SED STR("SED contains simple_encrypted_data")
#define CLIENTINFO_VERSION STR("VERSION shows client type, version and environment")
#define CLIENTINFO_CLIENTINFO STR("CLIENTINFO gives information about available CTCP commands")
#define CLIENTINFO_USERINFO STR("USERINFO returns user settable information")
#define CLIENTINFO_ERRMSG STR("ERRMSG returns error messages")
#define CLIENTINFO_FINGER STR("FINGER shows real name, login name and idle time of user")
#define CLIENTINFO_TIME STR("TIME tells you the time on the user's host")
#define CLIENTINFO_ACTION STR("ACTION contains action descriptions for atmosphere")
#define CLIENTINFO_DCC STR("DCC requests a direct_client_connection")
#define CLIENTINFO_UTC STR("UTC substitutes the local timezone")
#define CLIENTINFO_PING STR("PING returns the arguments it receives")
#define CLIENTINFO_ECHO STR("ECHO returns the arguments it receives")

#define CLOAK_CRACKROCK		0
#define CLOAK_NEONAPPLE		1
#define CLOAK_TUNNELVISION	2
#define CLOAK_ARGON		3
#define CLOAK_EVOLVER		4
#define	CLOAK_PREVAIL		5
#define CLOAK_COUNT		6

#define	AVGAWAYTIME		60
#define AVGHERETIME		5

#define CTCP_FLOOD_MAX          6

int mem_strlower = 0;
int cloak_script = 0;

#ifdef LEAF
char kickprefix[16];
char bankickprefix[16];
char autoaway[256];

extern char ctcp_reply[],
  botname[],
  natip[];
extern char botuser[],
  hostname[],
  botrealname[];
extern struct chanset_t *chanset;
extern time_t now;
extern int dcc_total;
extern struct dcc_t *dcc;
extern p_tcl_bind_list H_ctcp;
extern struct userrec *userlist;

int ctcpcount = 0;

int cloak_awaytime = 0;
int cloak_heretime = 0;

char cloak_bxver[] = "1.0c18";
char cloak_system[] = "Linux 2.2.5-15";

char ctcpversion[400] = "";
char ctcpuserinfo[400] = "";

char script_version[CLOAK_COUNT][100] = {
  "BitchX-%s\2/\2%s :(\2c\2)\37rackrock\37/\2b\2X \37[\0373.0.1á9\37]\37 :\2 Keep it to yourself!\2",
  "bitchx-%s\37(\37%s\37):\37 \2n\2eon\37a\37ppl\2e\2\37/\37\2v\2\60\37.\37\61\64i : \2d\2ont you wish you had it\37?\37",
  "\2b\2itchx-%s :tunnel\2vision\2/\37\61.2\37",
  ".\37.(\37argon\2/\2\61g\37)\37 \2:\2bitchx-%s",
  "\37evolver\37(\2%s\2)\37: bitchx\37(\2%s\2) \37í\37 %s\2/\2%s : eye yam pheerable now!",
  "%s\37!\37%s bitchx-%s \2-\2 prevail\37[\37%s\37]\37 :down with people"
};

#define tolower(x) (((x>='A') && (x<='Z')) ? (x | 32) : x)

char *strlower(char *s)
{
  static char *buf = NULL;
  char *p;

  if (buf)
    nfree(buf);
  mem_strlower = (strlen(s) + 1);
  buf = nmalloc(mem_strlower);
  strcpy(buf, s);
  p = buf;
  while (*p)
    *p++ = tolower(*p);
  return buf;
}

void scriptchanged()
{
  char tmp[500],
    os[250],
    osver[250],
   *p;

  if (cloak_script < 0)
    cloak_script = 0;
  if (cloak_script >= CLOAK_COUNT)
    cloak_script = CLOAK_COUNT - 1;
  strcpy(os, cloak_system);
  p = strchr(os, ' ');
  if (p) {
    *p++ = 0;
    strcpy(osver, p);
  } else
    strcpy(osver, "");
  if (cloak_script == CLOAK_CRACKROCK) {
    sprintf(ctcpversion, script_version[cloak_script], cloak_bxver, cloak_system);
    strcpy(ctcpuserinfo, STR("crack addict, help me."));
    strcpy(autoaway, STR("automatically dead"));
    strcpy(kickprefix, STR("\2c\2/\37k\37: "));
    strcpy(bankickprefix, STR("\2c\2/\37kb\37: "));
  } else if (cloak_script == CLOAK_NEONAPPLE) {
    sprintf(ctcpversion, script_version[cloak_script], cloak_bxver, strlower(cloak_system));
    strcpy(ctcpuserinfo, STR("neon apple"));
    strcpy(autoaway, STR("automatically away after 10 mins \37(\37\2n\2/\37a)\37"));
    strcpy(kickprefix, STR("\37[na\2(\37k\37)\2]\37 "));
    strcpy(bankickprefix, "");
  } else if (cloak_script == CLOAK_TUNNELVISION) {
    strcpy(tmp, cloak_bxver);
    p = tmp;
    p += strlen(tmp) - 1;
    p[1] = p[0];
    p[0] = 0x1F;
    p[2] = 0x1F;
    p[3] = 0;
    sprintf(ctcpversion, script_version[cloak_script], tmp, strlower(cloak_system));
    strcpy(autoaway, STR("auto-gone"));
    strcpy(kickprefix, "");
    strcpy(bankickprefix, "");
  } else if (cloak_script == CLOAK_ARGON) {
    sprintf(ctcpversion, script_version[cloak_script], cloak_bxver);
    strcpy(ctcpuserinfo, " ");
    strcpy(autoaway, STR("\37(\37ar\37)\37 auto-away \37(\3710m\37)\37"));
    strcpy(kickprefix, STR("\37(\37ar\37)\37 "));
    strcpy(bankickprefix, STR("\37(\37ar\37)\37 "));
  } else if (cloak_script == CLOAK_EVOLVER) {
    sprintf(ctcpversion, script_version[cloak_script], STR("02x9"), cloak_bxver, os, osver);
    strcpy(ctcpuserinfo, " ");
    strcpy(autoaway, STR("away\37: [\37\2i\2dle for \37[\3710 minutes\37]] (\37l\2:\2off\37,\37 p\2:\2off\37)\37"));
    strcpy(kickprefix, STR("\37ev\2!\2k\37 "));
    strcpy(bankickprefix, STR("\37ev\2!\2bk\37 "));
  } else if (cloak_script == CLOAK_PREVAIL) {
    sprintf(ctcpversion, script_version[cloak_script], os, osver, cloak_bxver, STR("0123"));
    strcpy(ctcpuserinfo, botrealname);
    strcpy(autoaway, STR("idle 10 minutes \37-\37 gone\37!\37"));
    strcpy(kickprefix, STR("\37[\37pv\37!\37k\37]\37 "));
    strcpy(bankickprefix, STR("\37[\37pv\37!\37bk\37]\37 "));
  }
}

#ifdef G_AUTOAWAY
void sendaway(int isupdate)
{
  char awtime[100] = "";
  int hrs,
    min,
    sec,
    gt;

  gt = time(NULL) - cloak_awaytime;
  hrs = gt / 3600;
  min = (gt % 3600) / 60;
  sec = gt % 60;
  if (cloak_script == CLOAK_CRACKROCK) {
    if (hrs)
      sprintf(awtime, STR("%dh %dm %ds"), hrs, min, sec);
    else if (min)
      sprintf(awtime, STR("%dm %ds"), min, sec);
    else
      sprintf(awtime, STR("%ds"), sec);
    dprintf(DP_SERVER, STR("AWAY :%s\2\37[\2%s\2]\37\2\n"), autoaway, awtime);
  } else if (cloak_script == CLOAK_TUNNELVISION) {
    if (hrs)
      sprintf(awtime, STR("%dh%dm%ds"), hrs, min, sec);
    else if (min)
      sprintf(awtime, STR("%dm%ds"), min, sec);
    else
      sprintf(awtime, STR("%ds"), sec);
    dprintf(DP_SERVER, STR("AWAY :%s \37(\37%s\37)\37\n"), autoaway, awtime);
  } else if (cloak_script == CLOAK_ARGON) {
    if (hrs)
      sprintf(awtime, STR("%dh%dm%ds"), hrs, min, sec);
    else if (min)
      sprintf(awtime, STR("%dm%ds"), min, sec);
    else
      sprintf(awtime, STR("%ds"), sec);
    dprintf(DP_SERVER, STR("AWAY :%s .\2.\2\37(\37%s\37)\37\n"), autoaway, awtime);
  } else if (cloak_script == CLOAK_EVOLVER) {
    if (hrs)
      sprintf(awtime, STR("%dh %dm %ds"), hrs, min, sec);
    else if (min)
      sprintf(awtime, STR("%dm %ds"), min, sec);
    else
      sprintf(awtime, STR("%ds"), sec);
    dprintf(DP_SERVER, STR("AWAY :%s \37[\37gone\2:\2%s\37]\37\n"), autoaway, awtime);
  } else if (cloak_script == CLOAK_PREVAIL) {
    if (hrs)
      sprintf(awtime, STR("%dh%dm%ds"), hrs, min, sec);
    else if (min)
      sprintf(awtime, STR("%dm%ds"), min, sec);
    else
      sprintf(awtime, STR("%ds"), sec);
    dprintf(DP_SERVER, STR("AWAY :%s %s\n"), autoaway, awtime);
  } else if (isupdate)
    return;
}
#endif

void ctcp_minutely()
{
#ifdef G_AUTOAWAY
  int n;
#endif
  ctcpcount = 0;
#ifdef G_AUTOAWAY
  if ((cloak_awaytime == 0) && (cloak_heretime == 0)) {
    cloak_heretime = time(NULL);
    dprintf(DP_MODE, STR("AWAY\n"));
    return;
  }
  n = random();
  if (cloak_awaytime == 0) {
    if (!(n % AVGHERETIME)) {
      cloak_heretime = 0;
      cloak_awaytime = time(NULL) - 600 - random() % 60;
      sendaway(1);
    }
  } else {
    if (!(n % AVGAWAYTIME)) {
      cloak_awaytime = 0;
      cloak_heretime = time(NULL);
      dprintf(DP_SERVER, STR("AWAY :\n"));
    } else {
      sendaway(0);
    }
  }
#endif
}

int ctcp_flooding()
{
  ctcpcount++;
  if (ctcpcount > CTCP_FLOOD_MAX)
    return 1;
  return 0;
}

int ctcp_FINGER(char *nick, char *uhost, char *handle, char *object, char *keyword, char *text)
{
  int idletime;

  Context;
  if (ctcp_flooding())
    return 1;
  if (cloak_awaytime)
    idletime = time(NULL) - cloak_awaytime;
  else
    idletime = time(NULL) - cloak_heretime;
  sprintf(ctcp_reply, STR("%s\001FINGER (%s@%s) Idle %d seconds\001"), ctcp_reply, botuser, hostname, idletime);
  return 1;
}

int ctcp_ECHO_ERRMSG(char *nick, char *uhost, char *handle, char *object, char *keyword, char *text)
{
  char reply[65];

  Context;
  if (ctcp_flooding())
    return 1;
  strncpy0(reply, text, sizeof(reply));
  if (object[0] != '#') {
    sprintf(ctcp_reply, STR("%s\001%s %s\001"), ctcp_reply, keyword, reply);
  }
  return 1;
}

int ctcp_PING(char *nick, char *uhost, char *handle, char *object, char *keyword, char *text)
{
  struct userrec *u = get_user_by_handle(userlist, handle);
  int atr = u ? u->flags : 0;

  Context;
  if (ctcp_flooding())
    return 1;
  if ((atr & USER_OP)) {
    if (strlen(text) <= 80)	/* bitch ignores > 80 */
      simple_sprintf(ctcp_reply, STR("%s\001%s %s\001"), ctcp_reply, keyword, text);
  }
  return 1;
}

int ctcp_VERSION(char *nick, char *uhost, char *handle, char *object, char *keyword, char *text)
{
  Context;
  if (ctcp_flooding())
    return 1;
  if (object[0] != '#') {
    sprintf(ctcp_reply, STR("%s\001VERSION %s\001"), ctcp_reply, ctcpversion);
  }
  return 1;
}

int ctcp_WHOAMI(char *nick, char *uhost, char *handle, char *object, char *keyword, char *text)
{
  Context;
  if (ctcp_flooding())
    return 1;
  if (object[0] != '#') {
    sprintf(ctcp_reply, STR("%s\002BitchX\002: Access Denied"), ctcp_reply);
  }
  return 1;
}

int ctcp_OP(char *nick, char *uhost, char *handle, char *object, char *keyword, char *text)
{
  char chan[512],
   *p;

  Context;
  if (ctcp_flooding())
    return 1;
  if (object[0] != '#') {
    if (!text[0])
      return 1;
    strncpy0(chan, text, sizeof(chan));
    p = strchr(chan, ' ');
    if (p)
      *p = 0;
    sprintf(ctcp_reply, STR("%s\002BitchX\002: I'm not on %s, or I'm not opped"), ctcp_reply, chan);
  }
  return 1;
}

int ctcp_INVITE_UNBAN(char *nick, char *uhost, char *handle, char *object, char *keyword, char *text)
{
  struct chanset_t *chan = chanset;
  char chname[512],
   *p;

  Context;
  if (ctcp_flooding())
    return 1;
  if ((object[0] != '#') && (text[0] == '#')) {
    strncpy0(chname, text, sizeof(chname));
    p = strchr(chname, ' ');
    if (p)
      *p = 0;
    while (chan) {
      if (chan->status & CHAN_ACTIVE) {
	if (!strcasecmp(chan->name, chname)) {
	  sprintf(ctcp_reply, STR("%s\002BitchX\002: Access Denied"), ctcp_reply);
	  return 1;
	}
      }
      chan = chan->next;
    }
    sprintf(ctcp_reply, STR("%s\002BitchX\002: I'm not on that channel"), ctcp_reply);
  }
  return 1;
}

int ctcp_USERINFO(char *nick, char *uhost, char *handle, char *object, char *keyword, char *text)
{
  Context;
  if (ctcp_flooding())
    return 1;
  if (cloak_script == CLOAK_TUNNELVISION)
    strcpy(ctcpuserinfo, botname);
  sprintf(ctcp_reply, STR("%s\001USERINFO %s\001"), ctcp_reply, ctcpuserinfo);
  return 1;
}

int ctcp_CLIENTINFO(char *nick, char *uhosr, char *handle, char *object, char *keyword, char *msg)
{
  char text[250];

  if (ctcp_flooding())
    return 1;
  Context;
  if (!msg[0]) {
    strcpy(text, STR("SED UTC ACTION DCC CDCC BDCC XDCC VERSION CLIENTINFO USERINFO ERRMSG FINGER TIME PING ECHO INVITE "));
    strcat(text, STR("WHOAMI OP OPS UNBAN IDENT XLINK UPTIME :Use CLIENTINFO <COMMAND> to get more specific information"));
  } else if (!strcasecmp(msg, STR("UNBAN")))
    strcpy(text, STR("UNBAN unbans the person from channel"));
  else if (!strcasecmp(msg, STR("OPS")))
    strcpy(text, STR("OPS ops the person if on userlist"));
  else if (!strcasecmp(msg, STR("ECHO")))
    strcpy(text, STR("ECHO returns the arguments it receives"));
  else if (!strcasecmp(msg, STR("WHOAMI")))
    strcpy(text, STR("WHOAMI user list information"));
  else if (!strcasecmp(msg, STR("INVITE")))
    strcpy(text, STR("INVITE invite to channel specified"));
  else if (!strcasecmp(msg, STR("PING")))
    strcpy(text, STR("PING returns the arguments it receives"));
  else if (!strcasecmp(msg, STR("UTC")))
    strcpy(text, STR("UTC substitutes the local timezone"));
  else if (!strcasecmp(msg, STR("XDCC")))
    strcpy(text, STR("XDCC checks cdcc info for you"));
  else if (!strcasecmp(msg, STR("BDCC")))
    strcpy(text, STR("BDCC checks cdcc info for you"));
  else if (!strcasecmp(msg, STR("CDCC")))
    strcpy(text, STR("CDCC checks cdcc info for you"));
  else if (!strcasecmp(msg, STR("DCC")))
    strcpy(text, STR("DCC requests a direct_client_connection"));
  else if (!strcasecmp(msg, STR("ACTION")))
    strcpy(text, STR("ACTION contains action descriptions for atmosphere"));
  else if (!strcasecmp(msg, STR("FINGER")))
    strcpy(text, STR("FINGER shows real name, login name and idle time of user"));
  else if (!strcasecmp(msg, STR("ERRMSG")))
    strcpy(text, STR("ERRMSG returns error messages"));
  else if (!strcasecmp(msg, STR("USERINFO")))
    strcpy(text, STR("USERINFO returns user settable information"));
  else if (!strcasecmp(msg, STR("CLIENTINFO")))
    strcpy(text, STR("CLIENTINFO gives information about available CTCP commands"));
  else if (!strcasecmp(msg, STR("SED")))
    strcpy(text, STR("SED contains simple_encrypted_data"));
  else if (!strcasecmp(msg, "OP"))
    strcpy(text, STR("OP ops the person if on userlist"));
  else if (!strcasecmp(msg, STR("VERSION")))
    strcpy(text, STR("VERSION shows client type, version and environment"));
  else if (!strcasecmp(msg, STR("XLINK")))
    strcpy(text, STR("XLINK x-filez rule"));
  else if (!strcasecmp(msg, STR("IDENT")))
    strcpy(text, STR("IDENT change userhost of userlist"));
  else if (!strcasecmp(msg, STR("TIME")))
    strcpy(text, STR("TIME tells you the time on the user's host"));
  else if (!strcasecmp(msg, STR("UPTIME")))
    strcpy(text, STR("UPTIME my uptime"));
  else {
    sprintf(ctcp_reply, STR("%s\001ERRMSG CLIENTINFO: %s is not a valid function\001"), ctcp_reply, msg);
    return 1;
  }
  sprintf(ctcp_reply, STR("%s001CLIENTINFO %s\001"), ctcp_reply, text);
  return 1;
}

int ctcp_TIME(char *nick, char *uhost, char *handle, char *object, char *keyword, char *text)
{
  char tms[81];

  Context;
  if (ctcp_flooding())
    return 1;
  strcpy(tms, ctime(&now));
  tms[strlen(tms) - 1] = 0;
  simple_sprintf(ctcp_reply, STR("%s\001TIME %s\001"), ctcp_reply, tms);
  return 1;
}

int ctcp_CHAT(char *nick, char *uhost, char *handle, char *object, char *keyword, char *text)
{
  struct userrec *u = get_user_by_handle(userlist, handle);
  int atr = u ? u->flags : 0,
    i,
    ix = (-1);

  Context;
  if (ctcp_flooding())
    return 1;
  if (atr & (USER_PARTY)) {
    for (i = 0; i < dcc_total; i++) {
      if ((dcc[i].type->flags & DCT_LISTEN) && ((!strcmp(dcc[i].nick, STR("(telnet)"))) || (!strcmp(dcc[i].nick, STR("(users)"))))) {
	ix = i;
	/* do me a favour and don't change this back to a CTCP reply,
	 * CTCP replies are NOTICE's this has to be a PRIVMSG
	 * -poptix 5/1/1997 */
	dprintf(DP_SERVER, STR("PRIVMSG %s :\001DCC CHAT chat %lu %u\001\n"), nick, iptolong(natip[0] ? (IP) inet_addr(natip) : getmyip()), dcc[ix].port);
      }
    }
    if (ix < 0)
      simple_sprintf(ctcp_reply, STR("%s\001ERROR no telnet port\001"), ctcp_reply);
  }
  return 1;
}

cmd_t ctcp_ctcp[] = {
  {"CLIENTINFO", "", ctcp_CLIENTINFO, NULL}
  ,
  {"FINGER", "", ctcp_FINGER, NULL}
  ,
  {"WHOAMI", "", ctcp_WHOAMI, NULL}
  ,
  {"OP", "", ctcp_OP, NULL}
  ,
  {"OPS", "", ctcp_OP, NULL}
  ,
  {"INVITE", "", ctcp_INVITE_UNBAN, NULL}
  ,
  {"UNBAN", "", ctcp_INVITE_UNBAN, NULL}
  ,
  {"ERRMSG", "", ctcp_ECHO_ERRMSG, NULL}
  ,
  {"USERINFO", "", ctcp_USERINFO, NULL}
  ,
  {"ECHO", "", ctcp_ECHO_ERRMSG, NULL}
  ,
  {"VERSION", "", ctcp_VERSION, NULL}
  ,
  {"PING", "", ctcp_PING, NULL}
  ,
  {"TIME", "", ctcp_TIME, NULL}
  ,
  {"CHAT", "", ctcp_CHAT, NULL}
  ,
  {0, 0, 0, 0}
};

int ctcp_gotmsg(char *from, char *msg)
{
  Function F;
  char buf[1024],
    buf2[256],
   *code,
   *uhost,
   *target;
  struct userrec *u;
  int i;

  strcpy(buf, msg);
  strcpy(buf2, from);
  msg = buf;
  from = buf2;
  target = newsplit(&msg);
  if (msg[0] == ':')
    msg++;
  if (msg[0] != 1)
    return 0;
  msg++;
  code = newsplit(&msg);
  for (i = 0; ctcp_ctcp[i].name; i++) {
    if (!strcmp(ctcp_ctcp[i].name, code)) {
      u = get_user_by_host(from);
      uhost = strchr(from, '!');
      if (!uhost)
	return 0;
      *uhost++ = 0;
      ctcp_reply[0] = 0;
      F = (Function) ctcp_ctcp[i].func;
      i = F(from, uhost, u ? u->handle : "*", target, code, msg);
      if (ctcp_reply[0])
	dprintf(DP_SERVER, STR("NOTICE %s :\001%s %s\001\n"), from, code, ctcp_reply);
      return i;
    }
  }
  return 0;
}
#endif

void cloak_describe(struct cfg_entry *cfgent, int idx)
{
  dprintf(idx, STR("cloak-script decides which BitchX script the bot cloaks. If set to 6, a random script will be cloaked.\n"));
  dprintf(idx, STR("Available: 0=crackrock, 1=neonapple, 2=tunnelvision, 3=argon, 4=evolver, 5=prevail\n"));
}

void cloak_changed(struct cfg_entry *cfgent, char * oldval, int * valid) {
  char * p;
  int i;
  p = cfgent->ldata ? cfgent->ldata : cfgent->gdata;
  if (!p) 
    return;
  i=atoi(p);
#ifdef LEAF
  if (i>=6)
    i = random() % 6;
#endif
  *valid = ( (i>=0) && (i<=6));
  if (*valid)
    cloak_script = i;
#ifdef LEAF
  scriptchanged();
#endif
}

struct cfg_entry CFG_CLOAK_SCRIPT = {
  "cloak-script", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
  cloak_changed, cloak_changed, cloak_describe
};

int ctcp_expmem()
{
  return mem_strlower;
}

void init_ctcp()
{
#ifdef LEAF
#ifdef HAVE_UNAME
  struct utsname un;
  bzero(&un, sizeof(un));
  if (uname(&un) < 0) {
#endif
    strcpy(cloak_system, STR("Linux 2.2.16"));
#ifdef HAVE_UNAME
  } else {
    sprintf(cloak_system, STR("%s %s"), un.sysname, un.release);
  }
#endif
  Context;
  add_builtins(H_ctcp, ctcp_ctcp);
  add_hook(HOOK_MINUTELY, (Function) ctcp_minutely);
#endif
  add_cfg(&CFG_CLOAK_SCRIPT);
}

