/* 
 * misc.c -- handles:
 *   split() maskhost() copyfile() movefile()
 *   dumplots() daysago() days() daysdur()
 *   logging things
 *   queueing output for the bot (msg and help)
 *   resync buffers for sharebots
 *   help system
 *   motd display and %var substitution
 * 
 * dprintf'ized, 12dec1995
 * 
 * $Id: misc.c,v 1.26 2000/01/17 16:14:45 per Exp $
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
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include "chan.h"
#include "hook.h"
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <errno.h>

#ifdef G_ANTITRACE
#include <sys/ptrace.h>
#include <sys/wait.h>
#endif

#ifdef HAVE_UNAME
#include <sys/utsname.h>
#endif

extern struct dcc_t *dcc;
extern struct chanset_t *chanset;
extern char helpdir[],
  version[],
  origbotname[],
  botname[],
  *binname;
extern char ver[],
  botnetnick[];
extern int backgrd,
  use_stderr,
  dcc_total,
  serv;
extern int keep_all_logs,
  quick_logs,
  strict_host;
extern time_t now;
extern struct userrec *userlist;

int helpmem = 0;
int shtime = 1;			/* whether or not to display the time

				 * with console output */
log_t *logs = 0;		/* logfiles */
int max_logs = 5;		/* current maximum log files */
int max_logsize = 0;		/* maximum logfile size, 0 for no limit */
int conmask = LOG_MODES | LOG_CMDS | LOG_MISC;	/* console mask */
int debug_output = 0;		/* display output to server to LOG_SERVEROUT */
int role = 0;			/* Roles 1-4 set by hub and used */
struct hook_entry *hook_list[REAL_HOOKS];
#ifdef G_DCCPASS
struct cmd_pass *cmdpass = NULL;
#endif

struct cfg_entry CFG_MOTD = {
  "motd", CFGF_GLOBAL, NULL, NULL,
  NULL, NULL, NULL
};

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

void detect_describe(struct cfg_entry * cfgent, int idx);
void detect_lchanged(struct cfg_entry * cfgent, char * oldval, int * valid);
void detect_gchanged(struct cfg_entry * cfgent, char * oldval, int * valid);

#ifdef G_LASTCHECK
struct cfg_entry CFG_LOGIN = {
  "login", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
  detect_gchanged, detect_lchanged, detect_describe
};
#endif
#ifdef G_ANTITRACE
struct cfg_entry CFG_TRACE = {
  "trace", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
  detect_gchanged, detect_lchanged, detect_describe
};
#endif
#ifdef G_PROMISC
struct cfg_entry CFG_PROMISC = {
  "promisc", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
  detect_gchanged, detect_lchanged, detect_describe
};
#endif

#ifdef G_PROCESSCHECK
struct cfg_entry CFG_BADPROCESS = {
  "bad-process", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
  detect_gchanged, detect_lchanged, detect_describe
};

struct cfg_entry CFG_PROCESSLIST = {
  "process-list", CFGF_GLOBAL | CFGF_LOCAL, NULL, NULL,
  NULL, NULL, detect_describe
};
#endif



void detect_describe(struct cfg_entry * cfgent, int idx) {
#ifdef G_LASTCHECK
  if (cfgent == &CFG_LOGIN)
    dprintf(idx, STR("login sets how to handle someone logging in to the shell\n"));
  else
#endif
    
#ifdef G_ANTITRACE
  if (cfgent == &CFG_TRACE)
    dprintf(idx, STR("trace sets how to handle someone tracing/debugging the bot\n"));
  else
#endif

#ifdef G_PROMISC
  if (cfgent == &CFG_PROMISC)
    dprintf(idx, STR("promisc sets how to handle when a interface is set to promiscous mode\n"));
  else
#endif

#ifdef G_PROCESSCHECK
  if (cfgent == &CFG_BADPROCESS)
    dprintf(idx, STR("bad-process sets how to handle when a running process not listed in process-list is detected\n"));
  else if (cfgent == &CFG_PROCESSLIST) 
    dprintf(idx, STR("process-list is a comma-separated list of \"expected processes\" running on the bots uid\n"));
  else
#endif
    {
      log(LCAT_ERROR, STR("huh? detect_describe called with unknown config entry\n"));
      return;
    }
  dprintf(idx, STR("Valid settings are: nocheck, ignore, warn, die, reject, suicide\n"));
}

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
int cfg_count=0;
struct cfg_entry ** cfg = NULL;
int cfg_noshare=0;

/* expected memory usage */
int expmem_misc()
{
  int tot = helpmem, i;
#ifdef G_DCCPASS
  struct cmd_pass *cp = NULL;
#endif

  for (i=0;i<cfg_count;i++) {
    tot += sizeof(void *);
    if (cfg[i]->gdata)
      tot += strlen(cfg[i]->gdata) + 1;
    if (cfg[i]->ldata)
      tot += strlen(cfg[i]->ldata) + 1;
  }
  for (i=0;i<REAL_HOOKS;i++) {
    struct hook_entry * p;
    for (p=hook_list[i];p;p=p->next) {
      tot += sizeof(struct hook_entry);
    }
  }
#ifdef G_DCCPASS
  for (cp=cmdpass;cp;cp=cp->next) {
    tot += sizeof(struct cmd_pass) + strlen(cp->name)+1;
  }
#endif
  return tot + (max_logs * sizeof(log_t));
}

void init_misc()
{
  static int last = 0;

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
    /*        Added by cybah  */
    logs[last].szLast[0] = 0;
    logs[last].Repeats = 0;
    /*        Added by rtc  */
    logs[last].flags = 0;
  }
  add_cfg(&CFG_MOTD);
  add_cfg(&CFG_FORKINTERVAL);
#ifdef G_LASTCHECK
  add_cfg(&CFG_LOGIN);
#endif
#ifdef G_ANTITRACE
  add_cfg(&CFG_TRACE);
#endif
#ifdef G_PROMISC
  add_cfg(&CFG_PROMISC);
#endif
#ifdef G_PROCESSCHECK
  add_cfg(&CFG_BADPROCESS);
  add_cfg(&CFG_PROCESSLIST);
#endif
}

/***** MISC FUNCTIONS *****/

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

int my_strcpy(char *a, char *b)
{
  char *c = b;

  while (*b)
    *a++ = *b++;
  *a = *b;
  return b - c;
}

/* split first word off of rest and put it in first */
void splitc(char *first, char *rest, char divider)
{
  char *p;

  p = strchr(rest, divider);
  if (p == NULL) {
    if ((first != rest) && (first != NULL))
      first[0] = 0;
    return;
  }
  *p = 0;
  if (first != NULL)
    strcpy(first, rest);
  if (first != rest)
    strcpy(rest, p + 1);
}

char *splitnick(char **blah)
{
  char *p = strchr(*blah, '!'),
   *q = *blah;

  if (p) {
    *p = 0;
    *blah = p + 1;
    return q;
  }
  return "";
}

char *listsplit(char **rest)
{
  char *o,
   *r;
  int cnt = 0;

  if (!rest)
    return NULL;
  o = *rest;
  while (*o == ' ')
    o++;
  r = o;
  if (*o == '{') {
    o++;
    r = o;
    cnt++;
    while (cnt && *o) {
      if (*o == '{') {
	cnt++;
      } else if (*o == '}') {
	cnt--;
      }
      o++;
    }

    if (cnt == 0) {
      o--;
      *o++ = 0;
      while (*o == ' ')
	o++;
      *rest = o;
      return r;
    } else {
      return (r - 1);
    }
  } else {
    r = o;
    while (*o && (*o != ' '))
      o++;
    if (*o)
      *o++ = 0;
    *rest = o;
    return r;
  }
}

char *newsplit(char **rest)
{
  register char *o,
   *r;

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

/* convert "abc!user@a.b.host" into "*!user@*.b.host"
 * or "abc!user@1.2.3.4" into "*!user@1.2.3.*"  */
void maskhost(char *s, char *nw)
{
  char *p,
   *q,
   *e,
   *f;
  int i;

  *nw++ = '*';
  *nw++ = '!';
  p = (q = strchr(s, '!')) ? q + 1 : s;
  /* strip of any nick, if a username is found, use last 8 chars */
  if ((q = strchr(p, '@'))) {
    if ((q - p) > 9) {
      nw[0] = '*';
      p = q - 7;
      i = 1;
    } else
      i = 0;
    while (*p != '@') {
      if (strchr("~+-^=", *p))
	if (strict_host)
	  nw[i] = '?';
	else
	  i--;
      else
	nw[i] = *p;
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
  /* now q points to the hostname, i point to where to put the mask */
  if (!(p = strchr(q, '.')) || !(e = strchr(p + 1, '.')))
    /* TLD or 2 part host */
    strcpy(nw, q);
  else {
    for (f = e; *f; f++);
    f--;
    if ((*f >= '0') && (*f <= '9')) {	/* numeric IP address */
      while (*f != '.')
	f--;
      strncpy0(nw, q, f - q+1);
      /* No need to nw[f-q]=0 here. */
      nw += (f - q);
      strcpy(nw, ".*");
    } else {			/* normal host >= 3 parts */
      /* ok, people whined at me...how about this? ..
       *    a.b.c  -> *.b.c
       *    a.b.c.d ->  *.b.c.d if tld is a country (2 chars)
       *             OR   *.c.d if tld is com/edu/etc (3 chars)
       *    a.b.c.d.e -> *.c.d.e   etc
       */
      char *x = strchr(e + 1, '.');

      if (!x)
	x = p;
      else if (strchr(x + 1, '.'))
	x = e;
      else if (strlen(x) == 3)
	x = p;
      else
	x = e;
      sprintf(nw, STR("*%s"), x);
    }
  }
}

/* copy a file from one place to another (possibly erasing old copy) */

/* returns 0 if OK, 1 if can't open original file, 2 if can't open new */

/* file, 3 if original file isn't normal, 4 if ran out of disk space */
int copyfile(char *oldpath, char *newpath)
{
  int fi,
    fo,
    x;
  char buf[512];
  struct stat st;

  fi = open(oldpath, O_RDONLY, 0);
  if (fi < 0)
    return 1;
  fstat(fi, &st);
  if (!(st.st_mode & S_IFREG))
    return 3;
  fo = creat(newpath, (int) (st.st_mode & 0777));
  if (fo < 0) {
    close(fi);
    return 2;
  }
  for (x = 1; x > 0;) {
    x = read(fi, buf, 512);
    if (x > 0) {
      if (write(fo, buf, x) < x) {	/* couldn't write */
	close(fo);
	close(fi);
	unlink(newpath);
	return 4;
      }
    }
  }
  close(fo);
  close(fi);
  return 0;
}

int movefile(char *oldpath, char *newpath)
{
  int ret;

#ifdef HAVE_RENAME
  /* try to use rename first */
  if (rename(oldpath, newpath) == 0)
    return 0;
#endif /* HAVE_RENAME */

  /* if that fails, fall back to copying the file */
  ret = copyfile(oldpath, newpath);
  if (ret == 0)
    unlink(oldpath);
  return ret;
}

/* dump a potentially super-long string of text */

/* assume prefix 20 chars or less */
void dumplots(int idx, char *prefix, char *data)
{
  char *p = data,
   *q,
   *n,
    c;

  if (!(*data)) {
    dprintf(idx, STR("%s\n"), prefix);
    return;
  }
  while (strlen(p) > 480) {
    q = p + 480;
    /* search for embedded linefeed first */
    n = strchr(p, '\n');
    if ((n != NULL) && (n < q)) {
      /* great! dump that first line then start over */
      *n = 0;
      dprintf(idx, STR("%s%s\n"), prefix, p);
      *n = '\n';
      p = n + 1;
    } else {
      /* search backwards for the last space */
      while ((*q != ' ') && (q != p))
	q--;
      if (q == p)
	q = p + 480;
      /* ^ 1 char will get squashed cos there was no space -- too bad */
      c = *q;
      *q = 0;
      dprintf(idx, STR("%s%s\n"), prefix, p);
      *q = c;
      p = q + 1;
    }
  }
  /* last trailing bit: split by linefeeds if possible */
  n = strchr(p, '\n');
  while (n != NULL) {
    *n = 0;
    dprintf(idx, STR("%s%s\n"), prefix, p);
    *n = '\n';
    p = n + 1;
    n = strchr(p, '\n');
  }
  if (*p)
    dprintf(idx, STR("%s%s\n"), prefix, p);	/* last trailing bit */
}

/* convert an interval (in seconds) to one of:
 * "19 days ago", "1 day ago", "18:12" */
void daysago(time_t now, time_t then, char *out)
{
  char s[81];

  if (now - then > 86400) {
    int days = (now - then) / 86400;

    sprintf(out, STR("%d day%s ago"), days, (days == 1) ? "" : "s");
    return;
  }
  strcpy(s, ctime(&then));
  s[16] = 0;
  strcpy(out, &s[11]);
}

/* convert an interval (in seconds) to one of:
 * "in 19 days", "in 1 day", "at 18:12" */
void days(time_t now, time_t then, char *out)
{
  char s[81];

  if (now - then > 86400) {
    int days = (now - then) / 86400;

    sprintf(out, STR("in %d day%s"), days, (days == 1) ? "" : "s");
    return;
  }
  strcpy(out, STR("at "));
  strcpy(s, ctime(&now));
  s[16] = 0;
  strcpy(&out[3], &s[11]);
}

/* convert an interval (in seconds) to one of:
 * "for 19 days", "for 1 day", "for 09:10" */
void daysdur(time_t now, time_t then, char *out)
{
  char s[81];
  int hrs,
    mins;

  if (now - then > 86400) {
    int days = (now - then) / 86400;

    sprintf(out, STR("for %d day%s"), days, (days == 1) ? "" : "s");
    return;
  }
  strcpy(out, STR("for "));
  now -= then;
  hrs = (int) (now / 3600);
  mins = (int) ((now - (hrs * 3600)) / 60);
  sprintf(s, STR("%02d:%02d"), hrs, mins);
  strcat(out, s);
}

/* show motd to dcc chatter */
void show_motd(int idx)
{
  if (CFG_MOTD.gdata && * (char *) CFG_MOTD.gdata)
    dprintf(idx, STR("Motd: %s\n"), (char *) CFG_MOTD.gdata);
  else
    dprintf(idx, STR("No motd\n"));
}

/* remove :'s from ignores and bans */
void remove_gunk(char *par)
{
  char *q,
   *p,
   *WBUF = nmalloc(strlen(par) + 1);

  for (p = par, q = WBUF; *p; p++, q++) {
    if (*p == ':')
      q--;
    else
      *q = *p;
  }
  *q = *p;
  strcpy(par, WBUF);
  nfree(WBUF);
}

/* This will return a pointer to the first character after the @ in the
 * string given it.  Possibly it's time to think about a regexp library
 * for eggdrop... */
char *extracthostname(char *hostmask)
{
  char *ptr = strrchr(hostmask, '@');

  if (ptr) {
    ptr = ptr + 1;
    return ptr;
  }
  return "";
}

/* show banner to telnet user, simialer to show_motd() - [seC] */
void show_banner(int idx)
{
  show_motd(idx);
}

/* create a string with random letters and digits */
void make_rand_str(char *s, int len)
{
  int j;

  for (j = 0; j < len; j++) {
    if (random() % 3 == 0)
      s[j] = '0' + (random() % 10);
    else
      s[j] = 'a' + (random() % 26);
  }
  s[len] = 0;
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

void callhook(int n)
{
  call_hook(n);
}

#define GARBLE_BUFFERS 40
unsigned char *garble_buffer[GARBLE_BUFFERS] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

int garble_ptr = (-1);

char *degarble(int len, char *g)
{
  int i;
  unsigned char x;

  garble_ptr++;
  if (garble_ptr == GARBLE_BUFFERS)
    garble_ptr = 0;
  if (garble_buffer[garble_ptr])
    nfree(garble_buffer[garble_ptr]);
  garble_buffer[garble_ptr] = nmalloc(len + 1);
  x = 0xFF;
  for (i = 0; i < len; i++) {
    garble_buffer[garble_ptr][i] = g[i] ^ x;
    x = garble_buffer[garble_ptr][i];
  }
  garble_buffer[garble_ptr][len] = 0;
  return (char *) garble_buffer[garble_ptr];
}

void null_func()
{
}
char *charp_func()
{
  return NULL;
}

int minus_func()
{
  return -1;
}

int false_func()
{
  return 0;
}


void null_share(int idx, char *x)
{
  if ((x[0] == 'u') && (x[1] == 'n')) {
    log(LCAT_BOTS, STR("User file rejected by %s: %s"), dcc[idx].nick, x + 3);
    log(LCAT_ERROR, STR("User file rejected by %s: %s"), dcc[idx].nick, x + 3);
    dcc[idx].status &= ~STAT_OFFERED;
    if (!(dcc[idx].status & STAT_GETTING)) {
      dcc[idx].status &= ~STAT_SHARE;
    }
  } else if ((x[0] != 'v') && (x[0] != 'e'))
    dprintf(idx, STR("s un Not sharing userfile.\n"));
}

/* void (*encrypt_pass) (char *, char *) = 0; */
void (*shareout) () = null_func;
void (*sharein) (int, char *) = null_share;
void (*qserver) (int, char *, int) = (void (*)(int, char *, int)) null_func;
int (*match_noterej) (struct userrec *, char *) = (int (*)(struct userrec *, char *)) false_func;
int (*rfc_casecmp) (const char *, const char *) = _rfc_casecmp;
int (*rfc_ncasecmp) (const char *, const char *, int) = _rfc_ncasecmp;
int (*rfc_toupper) (int) = _rfc_toupper;
int (*rfc_tolower) (int) = _rfc_tolower;

/* hooks, various tables of functions to call on ceratin events */
void add_hook(int hook_num, Function func)
{
  Context;
  if (hook_num < REAL_HOOKS) {
    struct hook_entry *p;

    for (p = hook_list[hook_num]; p; p = p->next)
      if (p->func == func)
	return;			/* dont add it if it's already there */
    p = nmalloc(sizeof(struct hook_entry));
    p->next = hook_list[hook_num];
    hook_list[hook_num] = p;
    p->func = func;
  } else
    switch (hook_num) {
    case HOOK_SHAREOUT:
      shareout = (void (*)()) func;
      break;
    case HOOK_SHAREIN:
      sharein = (void (*)(int, char *)) func;
      break;
    case HOOK_QSERV:
      if (qserver == (void (*)(int, char *, int)) null_func)
	qserver = (void (*)(int, char *, int)) func;
      break;
      /* special hook <drummer> */
    case HOOK_RFC_CASECMP:
      if (func == NULL) {
	rfc_casecmp = strcasecmp;
	rfc_ncasecmp = (int (*)(const char *, const char *, int)) strncasecmp;
	rfc_tolower = tolower;
	rfc_toupper = toupper;
      } else {
	rfc_casecmp = _rfc_casecmp;
	rfc_ncasecmp = _rfc_ncasecmp;
	rfc_tolower = _rfc_tolower;
	rfc_toupper = _rfc_toupper;
      }
      break;
    case HOOK_MATCH_NOTEREJ:
      if (match_noterej == false_func)
	match_noterej = func;
      break;
    }
}

void del_hook(int hook_num, Function func)
{
  Context;
  if (hook_num < REAL_HOOKS) {
    struct hook_entry *p = hook_list[hook_num],
     *o = NULL;

    while (p) {
      if (p->func == func) {
	if (o == NULL)
	  hook_list[hook_num] = p->next;
	else
	  o->next = p->next;
	nfree(p);
	break;
      }
      o = p;
      p = p->next;
    }
  } else
    switch (hook_num) {
    case HOOK_SHAREOUT:
      if (shareout == (void (*)()) func)
	shareout = null_func;
      break;
    case HOOK_SHAREIN:
      if (sharein == (void (*)(int, char *)) func)
	sharein = null_share;
      break;
    case HOOK_QSERV:
      if (qserver == (void (*)(int, char *, int)) func)
	qserver = null_func;
      break;
    case HOOK_MATCH_NOTEREJ:
      if (match_noterej == func)
	match_noterej = false_func;
      break;
    }
}

int call_hook_cccc(int hooknum, char *a, char *b, char *c, char *d)
{
  struct hook_entry *p;
  int f = 0;

  if (hooknum >= REAL_HOOKS)
    return 0;
  p = hook_list[hooknum];
  Context;
  while ((p != NULL) && !f) {
    f = p->func(a, b, c, d);
    p = p->next;
  }
  return f;
}

int bot_aggressive_to(struct userrec *u)
{
  char mypval[20],
    botpval[20];

  link_pref_val(u, botpval);
  link_pref_val(get_user_by_handle(userlist, botnetnick), mypval);
  if (strcmp(mypval, botpval) < 0)
    return 1;
  else
    return 0;
}

void detected(int code, char * msg) {
  char *p = NULL;
  char tmp2[1024];
  struct userrec * u;
  struct flag_record fr={FR_GLOBAL, 0, 0, 0, 0};
  int act;
  u=get_user_by_handle(userlist, botnetnick);
#ifdef G_LASTCHECK
  if (code==DETECT_LOGIN)
    p=(char *) (CFG_LOGIN.ldata ? CFG_LOGIN.ldata : (CFG_LOGIN.gdata ? CFG_LOGIN.gdata : NULL));
#endif
#ifdef G_ANTITRACE
  if (code==DETECT_TRACE)
    p=(char *) (CFG_TRACE.ldata ? CFG_TRACE.ldata : (CFG_TRACE.gdata ? CFG_TRACE.gdata : NULL));
#endif
#ifdef G_PROMISC
  if (code==DETECT_PROMISC)
    p=(char *) (CFG_PROMISC.ldata ? CFG_PROMISC.ldata : (CFG_PROMISC.gdata ? CFG_PROMISC.gdata : NULL));
#endif
#ifdef G_PROCESSCHECK
  if (code==DETECT_PROCESS)
    p=(char *) (CFG_BADPROCESS.ldata ? CFG_BADPROCESS.ldata : (CFG_BADPROCESS.gdata ? CFG_BADPROCESS.gdata : NULL));
#endif
  if (!p)
    act=DET_WARN;
  else if (!strcmp(p, STR("die")))
    act=DET_DIE;
  else if (!strcmp(p, STR("reject")))
    act=DET_REJECT;
  else if (!strcmp(p, STR("suicide")))
    act=DET_SUICIDE;
  else if (!strcmp(p, STR("nocheck")))
    act=DET_NOCHECK;
  else if (!strcmp(p, STR("ignore")))
    act=DET_IGNORE;
  else
    act=DET_WARN;
  switch (act) {
  case DET_IGNORE:
    break;
  case DET_WARN:
    log(LCAT_WARNING, msg);
    break;
  case DET_REJECT:
    log(LCAT_WARNING, STR("Setting myself +d: %s"), msg);
    sprintf(tmp2, STR("+d: %s"), msg);
    set_user(&USERENTRY_COMMENT, u, tmp2);
    get_user_flagrec(u, &fr, 0);
    fr.global = USER_DEOP | USER_BOT;
    set_user_flagrec(u, &fr, 0);
    sleep(1);
    break;
  case DET_DIE:
    log(LCAT_WARNING, STR("Dying: %s"), msg);
    sprintf(tmp2, STR("Dying: %s"), msg);
    set_user(&USERENTRY_COMMENT, u, tmp2);
#ifdef LEAF
    tputs(serv, STR("QUIT :bbl\n"), 10);
#endif
    sleep(1);
    fatal(msg, 0);
    exit(0);
    break;
  case DET_SUICIDE:
    log(LCAT_WARNING, STR("Comitting suicide: %s"), msg);
    sprintf(tmp2, STR("Suicide: %s"), msg);
    set_user(&USERENTRY_COMMENT, u, tmp2);
#ifdef LEAF
    tputs(serv, STR("QUIT :HARAKIRI!\n"), 34);
#endif
    sleep(1);
    unlink(binname);
#ifdef HUB
    p = strrchr(binname, '/');
    p++;
    p=0;
    strcpy(p, ".c");
    unlink(binname);
    strcpy(p, ".u");
    unlink(binname);
    strcpy(p, STR(".cfg"));
    unlink(binname);
    strcpy(p, STR(".v"));
    unlink(binname);
#endif
    fatal(msg, 0);
    exit(0);
    break;
  case DET_NOCHECK:
    break;
  }
}


struct cfg_entry * check_can_set_cfg (char * target, char * entryname) {
  int i;
  struct userrec * u;
  struct cfg_entry * entry = NULL;
  for (i=0;i<cfg_count;i++) 
    if (!strcmp(cfg[i]->name, entryname)) {
      entry=cfg[i];
      break;
    }
  if (!entry)
    return 0;
  if (target) {
    if (!(entry->flags & CFGF_LOCAL))
      return 0;
    if (! (u=get_user_by_handle(userlist, target))) 
      return 0;
    if (!(u->flags & USER_BOT))
      return 0;
  } else {
    if (!(entry->flags & CFGF_GLOBAL))
      return 0;
  }
  return entry;
}

void set_cfg_str (char * target, char * entryname, char * data) {
  struct cfg_entry * entry;
  if (!(entry=check_can_set_cfg(target, entryname)))
    return;
  if (data && !strcmp(data, "-"))
    data=NULL;
  if (data && (strlen(data)>=256))
    data[255] = 0;
  if (target) {
    struct userrec * u = get_user_by_handle(userlist, target);
    struct xtra_key * xk;
    char * olddata = entry->ldata;
    if (u && !strcmp(botnetnick, u->handle)) {
      if (data) {
	entry->ldata=nmalloc(strlen(data)+1);
	strcpy(entry->ldata, data);
      } else
	entry->ldata=NULL;
      if (entry->localchanged) {
	int valid=1;
	entry->localchanged(entry, olddata, &valid);
	if (!valid) {
	  if (entry->ldata)
	    nfree(entry->ldata);
	  entry->ldata=olddata;
	  data=olddata;
	  olddata=NULL;
	}
      }
    }
    xk=user_malloc(sizeof(struct xtra_key));
    bzero(xk, sizeof(struct xtra_key));
    xk->key=user_malloc(strlen(entry->name)+1);
    strcpy(xk->key, entry->name);
    if (data) {
      xk->data=user_malloc(strlen(data)+1);
      strcpy(xk->data, data);
    }
    set_user(&USERENTRY_CONFIG, u, xk);
    if (olddata)
      nfree(olddata);
  } else {
    char * olddata = entry->gdata;
    if (data) {
      entry->gdata = nmalloc(strlen(data)+1);
      strcpy(entry->gdata, data);
    } else
      entry->gdata=NULL;
    if (entry->globalchanged) {
      int valid=1;
      entry->globalchanged(entry, olddata, &valid);
      if (!valid) {
	if (entry->gdata)
	  nfree(entry->gdata);
	entry->gdata=olddata;
	olddata=NULL;
      }
    }
    if (!cfg_noshare)
      botnet_send_cfg_broad(-1, entry);
    if (olddata)
      nfree(olddata);
  }
}

void userfile_cfg_line (char * ln) {
  char * name;
  int i;
  struct cfg_entry * cfgent = NULL;
  name=newsplit(&ln);
  for (i=0;!cfgent && (i<cfg_count);i++)
    if (!strcmp(cfg[i]->name, name))
      cfgent=cfg[i];
  if (cfgent) {
    set_cfg_str(NULL, cfgent->name, ln[0] ? ln : NULL);
  } else
    log(LCAT_ERROR, STR("Unrecognized config entry %s in userfile"), name);

}

void got_config_share (int idx, char * ln) {
  char * name;
  int i;
  struct cfg_entry * cfgent = NULL;
  cfg_noshare++;
  name=newsplit(&ln);
  for (i=0;!cfgent && (i<cfg_count);i++)
    if (!strcmp(cfg[i]->name, name))
      cfgent=cfg[i];
  if (cfgent) {
    set_cfg_str(NULL, cfgent->name, ln[0] ? ln : NULL);
    botnet_send_cfg_broad(idx, cfgent);
  } else
    log(LCAT_ERROR, STR("Unrecognized config entry %s in userfile"), name);
  cfg_noshare--;
}

void add_cfg(struct cfg_entry * entry) {
  cfg = (void *) nrealloc(cfg, sizeof(void *) * (cfg_count+1));
  cfg[cfg_count]=entry;
  cfg_count++;
  entry->ldata=NULL;
  entry->gdata=NULL;
}

void trigger_cfg_changed() {
  int i;
  struct userrec * u;
  struct xtra_key * xk;
  u=get_user_by_handle(userlist, botnetnick);

  for (i=0;i<cfg_count;i++) {
    if (cfg[i]->flags & CFGF_LOCAL) {
      xk=get_user(&USERENTRY_CONFIG, u);
      while (xk && strcmp(xk->key, cfg[i]->name))
	xk=xk->next;
      if (xk) {
	log(LCAT_DEBUG, "trigger_cfg_changed for %s", cfg[i]->name ? cfg[i]->name : "(null)");
	if (!strcmp(cfg[i]->name, xk->key ? xk->key : "")) {
	  set_cfg_str(botnetnick, cfg[i]->name, xk->data);
	}
      }
    }
  }
}

int shell_exec(char * cmdline, char * input, char ** output, char ** erroutput)
{
  FILE *inpFile, *outFile, *errFile;
  char * fname, *p;
  int x;
  if (!cmdline)
    return 0;
  /* Set up temp files */

  fname=nmalloc(strlen(binname)+100);
  strcpy(fname, binname);
  p=strrchr(fname, '/');
  if (!p) {
    nfree(fname);
    log(LCAT_ERROR, STR("exec: Couldn't find bin dir."));
    return 0;
  }
  p++;
  strcpy(p, ".i");
  inpFile = fopen(fname, "w+");
  unlink(fname);
  if (!inpFile) {
    log(LCAT_ERROR, STR("exec: Couldn't open %s"), fname);
    nfree(fname);
    return 0;
  }
  if (input) {
    if (fwrite(input, 1, strlen(input), inpFile)!=strlen(input)) {
      log(LCAT_ERROR, STR("exec: Couldn't write to %s"), fname);
      fclose(inpFile);
      nfree(fname);
      return 0;
    }
    fseek(inpFile, 0, SEEK_SET);
  }
  strcpy(p, ".e");
  errFile = fopen(fname, "w+");
  unlink(fname);
  if (!errFile) {
    log(LCAT_ERROR, STR("exec: Couldn't open %s"), fname);
    nfree(fname);
    fclose(inpFile);
    return 0;
  }
  strcpy(p, ".o");
  outFile = fopen(fname, "w+");
  unlink(fname);
  if (!outFile) {
    log(LCAT_ERROR, STR("exec: Couldn't open %s"), fname);
    nfree(fname);
    fclose(inpFile);
    fclose(errFile);
    return 0;
  }
  nfree(fname);
  x=fork();
  if (x==-1) {
    log(LCAT_ERROR, STR("exec: fork() failed"));
    fclose(inpFile);
    fclose(errFile);
    fclose(outFile);
    return 0;
  }
  if (x) { 
    /* Parent: wait for the child to complete */
    int st=0;
    waitpid(x, &st, 0);
    /* Now read the files into the buffers */
    fclose(inpFile);
    fflush(outFile);
    fflush(errFile);
    if (erroutput) {
      char * buf;
      int fs;
      fseek(errFile, 0, SEEK_END);
      fs=ftell(errFile);
      if (fs==0) {
	(*erroutput) = NULL;
      } else {
	buf=nmalloc(fs+1);
	fseek(errFile, 0, SEEK_SET);
	fread(buf, 1, fs, errFile);
	buf[fs]=0;
	(*erroutput) = buf;
      }
    }
    fclose(errFile);
    if (output) {
      char * buf;
      int fs;
      fseek(outFile, 0, SEEK_END);
      fs=ftell(outFile);
      if (fs==0) {
	(*output) = NULL;
      } else {
	buf=nmalloc(fs+1);
	fseek(outFile, 0, SEEK_SET);
	fread(buf, 1, fs, outFile);
	buf[fs]=0;
	(*output) = buf;
      }
    }
    fclose(outFile);
    return 1;
  } else {
    /* Child: make fd's and set them up as std* */
    int ind, outd, errd;
    char * argv[4];
    ind=fileno(inpFile);
    outd=fileno(outFile);
    errd=fileno(errFile);
    if (dup2(ind, STDIN_FILENO)== (-1)) {
      exit(1);
    }
    if (dup2(outd, STDOUT_FILENO)==(-1)) {
      exit(1);
    }
    if (dup2(errd, STDERR_FILENO)==(-1)) {
      exit(1);
    }
    argv[0]="/bin/sh";
    argv[1]="-c";
    argv[2]=cmdline;
    argv[3]=NULL;
    execvp(argv[0], &argv[0]);
    exit(1);
  }

}


#ifdef G_DCCPASS
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

void set_cmd_pass(char *ln, int shareit)
{
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


char * cuserid (char * str);

char last_buf[128]="";

void check_last() {
#ifdef G_LASTCHECK
  char user[20];
  strncpy0(user, cuserid(NULL) ? cuserid(NULL) : "" , sizeof(user));
  if (user[0]) {
    char * out;
    char buf[50];
    sprintf(buf, STR("last %s"), user);
    if (shell_exec(buf, NULL, &out, NULL)) {
      if (out) {
	char *p;
	p=strchr(out, '\n');
	if (p) 
	  *p=0;
	if (strlen(out)>10) {
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

void check_processes() {
#ifdef G_PROCESSCHECK
  char * proclist, *out, *p, *np, *curp, buf[1024], bin[128];

  proclist = (char *) (CFG_PROCESSLIST.ldata && ((char *) CFG_PROCESSLIST.ldata)[0] ? 
	      CFG_PROCESSLIST.ldata :
	      CFG_PROCESSLIST.gdata && ((char *) CFG_PROCESSLIST.gdata)[0] ? 
	      CFG_PROCESSLIST.gdata : NULL);
  if (!proclist) 
    return;
  if (!shell_exec(STR("ps x"), NULL, &out, NULL))
    return;

  /* Get this binary's filename */
  strncpy0(buf, binname, sizeof(buf));
  p=strrchr(buf, '/');
  if (p) {
    p++;
    strncpy0(bin, p, sizeof(bin));
  } else {
    bin[0] = 0;
  }

  /* Fix up the "permitted processes" list */
  p=nmalloc(strlen(proclist)+strlen(bin)+6);
  strcpy(p, proclist);
  strcat(p, " ");
  strcat(p, bin);
  strcat(p, " ");
  proclist=p;
  curp=out;
  while (curp) {
    np=strchr(curp, '\n');
    if (np)
      *np++=0;
    if (atoi(curp)>0) {
      char *pid, *tty, *stat, *time, cmd[512], line[2048];
      strncpy0(line, curp, sizeof(line));
      /* it's a process line */
      /* Assuming format: pid tty stat time cmd */
      pid=newsplit(&curp);
      tty=newsplit(&curp);
      stat=newsplit(&curp);
      time=newsplit(&curp);
      strncpy0(cmd, curp, sizeof(cmd));
      /* skip any <defunct> procs "/bin/sh -c" crontab stuff and binname crontab stuff*/
      if (!strstr(cmd, STR("<defunct>")) && !strncmp(cmd, STR("/bin/sh -c"), 10) 
	  && !strncmp(cmd, binname, strlen(binname))) {
	/* get rid of any args */
	if ((p=strchr(cmd, ' '))) 
	  *p=0;

	/* remove [] or () */
	if (strlen(cmd)) {
	  p = cmd + strlen(cmd) - 1;
	  if (((cmd[0]=='(') && (*p==')')) || ((cmd[0]=='[') && (*p==']'))) {
	    *p=0;
	    strcpy(buf, cmd+1);
	    strcpy(cmd, buf);
	  }
	}
      
	/* remove path */
	if ((p=strrchr(cmd, '/'))) {
	  p++;
	  strcpy(buf, p);
	  strcpy(cmd, buf);
	}
      
	/* skip "ps" */
	if (strcmp(cmd,"ps")) {
	  /* see if proc's in permitted list */
	  strcat(cmd, " ");
	  if ((p=strstr(proclist, cmd))) {
	    /* Remove from permitted list */
	    while (*p!=' ')
	      *p++=1;
	  } else {
	    char wrk[16384];
	    sprintf(wrk, STR("Unexpected process: %s"), line);
	    detected(DETECT_PROCESS, wrk);
	  }
	}
      }
    }
    curp=np;
  }
  nfree(proclist);
  if (out)
    nfree(out);
#endif
}

void check_promisc()
{
#ifdef G_PROMISC
#ifdef SIOCGIFCONF
  char buf[8192];
  struct ifreq ifreq,
   *ifr;
  struct ifconf ifcnf;
  char *cp,
   *cplim;
  int sock; 
  if (!strcmp((char *) CFG_PROMISC.ldata ? CFG_PROMISC.ldata : 
	      CFG_PROMISC.gdata ? CFG_PROMISC.gdata : "", STR("nocheck")))
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
	detected(DETECT_PROMISC, STR("Detected promiscous mode"));
	return;
      }
    }
  }
  close(sock);
#endif
#endif
}

int traced=0;

void got_trace(int z) {
  traced=0;
}

void check_trace()
{
#ifdef G_ANTITRACE
  int x,
    parent,
    i;
  struct sigaction sv, *oldsv = NULL;
  if (!strcmp((char *) CFG_TRACE.ldata ? CFG_TRACE.ldata : CFG_TRACE.gdata ? CFG_TRACE.gdata : "", STR("nocheck")))
    return;
  parent=getpid();
#ifndef DEBUG_MEM
#ifdef __linux__
  bzero(&sv, sizeof(sv));
  sv.sa_handler=got_trace;
  sigemptyset(&sv.sa_mask);
  oldsv=NULL;
  sigaction(SIGTRAP, &sv, oldsv);
  traced=1;
  asm("int3");
  sigaction(SIGTRAP, oldsv, NULL);
  if (traced)
    detected(DETECT_TRACE, STR("I'm being traced"));
  oldsv=NULL;
  sigaction(SIGINT, &sv, oldsv);
  traced=1;
  kill(getpid(), SIGINT);
  sigaction(SIGINT, oldsv, NULL);
  if (traced)
    detected(DETECT_TRACE, STR("I'm being traced"));
  x = fork();
  if (x == -1)
    return;
  if (x == 0) {
    i = ptrace(PTRACE_ATTACH, parent, 0, 0);
    if (i==(-1)) {
      if (errno==EPERM)
	detected(DETECT_TRACE, STR("I'm being traced"));
    } else {
      waitpid(parent, &i, 0);
      kill(parent, SIGCHLD);
      ptrace(PTRACE_DETACH, parent, 0, 0);
      kill(parent, SIGCHLD);
    }
    exit(0);
  } else
    wait(&i);
#endif

#ifdef __FreeBSD__
  x = fork();
  if (x == -1)
    return;
  if (x == 0) {
    i=ptrace(PT_ATTACH, parent, 0, 0);
    if (i==(-1)) {
      if (errno==EBUSY)
        detected(DETECT_TRACE, STR("I'm being trace"));
    } else {
      wait(&i);
      i=ptrace(PT_CONTINUE, parent, (caddr_t) 1, 0);
      kill(parent, SIGCHLD);      
      wait(&i);
      i=ptrace(PT_DETACH, parent, (caddr_t) 1, 0);
      wait(&i);
    }
    exit(0);
  } else
    waitpid(x, NULL, 0);
#endif

#endif
#endif
}


#define STREAM_CHUNKSIZE 1024

void stream_needsize(stream s, int size) {
  if (size<s->alloc)
    return;
  s->alloc = STREAM_CHUNKSIZE * ((size + STREAM_CHUNKSIZE -1) / STREAM_CHUNKSIZE);
  if (s->alloc>0) 
    s->data=nrealloc(s->data, s->alloc);
  else {
    nfree(s->data);
    s->data=NULL;
  }
}

stream stream_create() {
  stream s;
  s=(stream) nmalloc(sizeof(stream_record));
  bzero(s, sizeof(stream_record));
  return s;
}

void stream_kill(stream s) {
  if (s->data)
    nfree(s->data);
  nfree(s);
}

int stream_seek(stream s, int origin, int offset) {
  int newpos;
  switch (origin) {
  case SEEK_SET:
    newpos = offset;
    break;
  case SEEK_CUR:
    newpos = s->position + offset;
    break;
  case SEEK_END:
    newpos = s->size - offset;
    break;
  default:
    newpos = s->position;
  }
  if (newpos<0) 
    newpos=0;
  else if (newpos>s->size)
    newpos=s->size;
  s->position=newpos;
  return newpos;
}

int stream_getpos(stream s) {
  return s->position;
}

int stream_size(stream s) {
  return s->size;
}

void * stream_buffer(stream s) {
  return (void *) s->data;
}

void stream_truncate(stream s) {
  s->size = s->position;
}

void stream_puts(stream s, char * data) {
  int len=strlen(data);
  stream_needsize(s, s->position + len);
  memcpy(&s->data[s->position], data, len);
  s->position += len;
  s->size = (s->size < s->position) ? s->position : s->size;
}



void stream_printf EGG_VARARGS_DEF(stream, arg1) {

/* (stream s, char * format, ...) { */
  int tmpsize=2048;
  char *tmp=NULL, *format=NULL;
  stream s = NULL;
  va_list va;
  tmp=nmalloc(tmpsize);
  s=EGG_VARARGS_START(stream, arg1, va);
  format=va_arg(va, char *);
  while ( vsnprintf(tmp, tmpsize-1, format, va) == (-1)) {
    nfree(tmp);
    tmpsize = tmpsize*2;
    tmp=nmalloc(tmpsize);
  }
  va_end(va);
  tmp[tmpsize-1]=0;
  stream_puts(s, tmp);
  nfree(tmp);
}

int stream_gets(stream s, char * data, int maxsize) {
  int toread, read = 0;
  char c = 0;
  toread = (maxsize <= (s->size - s->position)) ? maxsize : (s->size - s->position);
  while ((read<toread) && (c!='\n')) {
    c = s->data[s->position++];
    *data++=c;
    read++;
  }
  if ( (read<toread) || (toread<maxsize))
    *data=0;
  return read;
}




