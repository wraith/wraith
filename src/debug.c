/*
 * debug.c -- handles:
 *   signal handling
 *   Context handling
 *   debug funtions
 *
 */


#include "common.h"
#include "debug.h"
#include "chanprog.h"
#include "net.h"
#include "shell.h"
#include "color.h"
#include "userrec.h"
#include "main.h"
#include "dccutil.h"
#include <setjmp.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/mman.h>
#define PAGESIZE 4096

int		sdebug = 0;             /* enable debug output? */


#ifdef DEBUG_CONTEXT
/* Context storage for fatal crashes */
char    cx_file[16][15]; 
char    cx_note[16][21];
int     cx_line[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int     cx_ptr = 0;
#endif /* DEBUG_CONTEXT */

void setlimits()
{
#ifndef CYGWIN_HACKS
  struct rlimit plim, fdlim, corelim;
#ifndef DEBUG_MEM
/*  struct rsslim, stacklim;
  rsslim.rlim_cur = 30720;
  rsslim.rlim_max = 30720;
  setrlimit(RLIMIT_RSS, &rsslim);
  stacklim.rlim_cur = 30720;
  stacklim.rlim_max = 30720;
  setrlimit(RLIMIT_STACK, &stacklim);
*/
  /* do NOT dump a core. */
  plim.rlim_cur = 40;
  plim.rlim_max = 40;
  corelim.rlim_cur = 0;
  corelim.rlim_max = 0;
#else /* DEBUG_MEM */
  plim.rlim_cur = 80;
  plim.rlim_max = 80;
  corelim.rlim_cur = RLIM_INFINITY;
  corelim.rlim_max = RLIM_INFINITY;
#endif /* !DEBUG_MEM */
  setrlimit(RLIMIT_CORE, &corelim);
  setrlimit(RLIMIT_NPROC, &plim);
  fdlim.rlim_cur = 200;
  fdlim.rlim_max = 200;
  setrlimit(RLIMIT_NOFILE, &fdlim);
#endif /* !CYGWIN_HACKS */
}

void init_debug()
{
  int i = 0;
  for (i = 0; i < 16; i ++)
    Context;

#ifdef DEBUG_CONTEXT
  egg_bzero(&cx_file, sizeof cx_file);
  egg_bzero(&cx_note, sizeof cx_note);
#endif /* DEBUG_CONTEXT */
}

void sdprintf (char *format, ...)
{
  if (sdebug) {
    char s[2001] = "";
    va_list va;

    va_start(va, format);
    egg_vsnprintf(s, 2000, format, va);
    va_end(va);
    if (!backgrd)
      dprintf(DP_STDOUT, "[D:%d] %s%s%s\n", getpid(), BOLD(-1), s, BOLD_END(-1));
    else
      printf("[D:%d] %s%s%s\n", getpid(), BOLD(-1), s, BOLD_END(-1));
  }
}


#ifdef DEBUG_CONTEXT

#define CX(ptr) cx_file[ptr] && cx_file[ptr][0] ? cx_file[ptr] : "", cx_line[ptr], cx_note[ptr] && cx_note[ptr][0] ? cx_note[ptr] : ""

static void write_debug()
{
  int x;
  char s[25] = "", tmpout[150] = "", buf[DIRMAX] = "";
  int y;

  egg_snprintf(tmpout, sizeof tmpout, "* Last 3 contexts: %s/%d [%s], %s/%d [%s], %s/%d [%s]",
                                  CX(cx_ptr - 2), CX(cx_ptr - 1), CX(cx_ptr));
  putlog(LOG_MISC, "*", "%s", tmpout);
  printf("%s\n", tmpout);
  sprintf(buf, "%sDEBUG", tempdir);
  x = creat(buf, 0600);
  setsock(x, SOCK_NONSOCK);
  if (x < 0) {
    putlog(LOG_MISC, "*", "* Failed to write DEBUG");
  } else {
    char date[80] = "";
    strncpyz(s, ctime(&now), sizeof s);
    dprintf(-x, "Debug (%s) written %s\n", ver, s);

    egg_strftime(date, sizeof date, "%c %Z", gmtime(&buildts));
    dprintf(-x, "Build: %s (%lu)\n", date, buildts);

#ifndef CYGWIN_HACKS
    stackdump(-x);
#endif /* !CYGWIN_HACKS */
    dprintf(-x, "Context: ");
    cx_ptr = cx_ptr & 15;
    for (y = ((cx_ptr + 1) & 15); y != cx_ptr; y = ((y + 1) & 15))
      dprintf(-x, "%s/%d, [%s]\n         ", CX(y));
    dprintf(-x, "%s/%d [%s]\n\n", CX(cx_ptr));
    tell_dcc(-x);
    dprintf(-x, "\n");
    tell_netdebug(-x);
    killsock(x);
    close(x);
#ifndef CYGWIN_HACKS
    {
      char *w = NULL, *who = NULL, *ps = NULL, *uname = NULL, 
           *id = NULL, *ls = NULL, *debug = NULL, *msg = NULL, buf2[DIRMAX] = "";

      egg_strftime(date, sizeof date, "%c %Z", gmtime(&now));
      shell_exec("w", NULL, &w, NULL);
      shell_exec("who", NULL, &who, NULL);
      shell_exec("ps cux", NULL, &ps, NULL);
      shell_exec("uname -a", NULL, &uname, NULL);
      shell_exec("id", NULL, &id, NULL);
      shell_exec("ls -al", NULL, &ls, NULL);
      buf2[0] = 0;
      sprintf(buf2, "cat %s", buf);
      shell_exec(buf2, NULL, &debug, NULL);

      msg = calloc(1, strlen(date) + strlen(id) + strlen(uname) + strlen(w) + strlen(who) + strlen(ps) + strlen(ls) + strlen(debug) + 50);

      sprintf(msg, "%s\n%s\n%s\n\n%s\n%s\n\n%s\n\n-----%s-----\n\n\n\n%s", date, id, uname, w, who, ps, ls, debug);
      email("Debug output", msg, EMAIL_TEAM);
      free(msg);
    }
    putlog(LOG_MISC, "*", "* Emailed DEBUG to development team...");
#endif /* !CYGWIN_HACKS */
    unlink(buf);
  }
}
#endif /* DEBUG_CONTEXT */

static void got_bus(int z)
{
  signal(SIGBUS, SIG_DFL);
#ifdef DEBUG_CONTEXT
  write_debug();
#endif
  fatal("BUS ERROR -- CRASHING!", 1);
#ifdef DEBUG_MEM
  kill(getpid(), SIGBUS);
#else
  exit(1);
#endif /* DEBUG_MEM */
}

#ifndef CYGWIN_HACKS
struct stackframe {
  struct stackframe *ebp;
  unsigned long addr;
};

/*
  CALL x
  PUSH EBP
  MOV EBP, ESP

  0x10: EBP
  0x14: EIP

 */

static int
canaccess(void *addr)
{
  addr = (void *) (((unsigned long) addr / PAGESIZE) * PAGESIZE);
  if (mprotect(addr, PAGESIZE, PROT_READ | PROT_WRITE | PROT_EXEC))
    if (errno != EACCES)
      return 0;
  return 1;
};

struct stackframe *sf = NULL;
int stackdepth = 0;

void
stackdump(int idx)
{
  __asm__("movl %EBP, %EAX");
  __asm__("movl %EAX, sf");
  if (idx == 0)
    putlog(LOG_MISC, "*", "STACK DUMP (%%ebp)");
  else
    dprintf(idx, "STACK DUMP (%%ebp)\n");

  while (canaccess(sf) && stackdepth < 20 && sf->ebp) {
    if (idx == 0)
      putlog(LOG_MISC, "*", " %02d: 0x%08lx/0x%08lx", stackdepth, (unsigned long) sf->ebp, sf->addr);
    else
      dprintf(idx, " %02d: 0x%08lx/0x%08lx\n", stackdepth, (unsigned long) sf->ebp, sf->addr);
    sf = sf->ebp;
    stackdepth++;
  }
  stackdepth = 0;
  sf = NULL;
  sleep(1);
};
#endif /* !CYGWIN_HACKS */

static void got_segv(int z)
{
  alarm(0);		/* dont let anything jump out of this signal! */
  signal(SIGSEGV, SIG_DFL);
  /* stackdump(0); */
#ifdef DEBUG_CONTEXT
  write_debug();
#endif
  fatal("SEGMENT VIOLATION -- CRASHING!", 1);
#ifdef DEBUG_MEM
  kill(getpid(), SIGSEGV);
#else
  exit(1);
#endif /* DEBUG_MEM */
}

static void got_fpe(int z)
{
#ifdef DEBUG_CONTEXT
  write_debug();
#endif
  fatal("FLOATING POINT ERROR -- CRASHING!", 0);
}

static void got_term(int z)
{
#ifdef HUB
  write_userfile(-1);
#endif
  fatal("Received SIGTERM", 0);
}

static void got_abort(int z)
{
  signal(SIGABRT, SIG_DFL);
#ifdef DEBUG_CONTEXT
  write_debug();
#endif
  fatal("GOT SIGABRT -- CRASHING!", 1);
#ifdef DEBUG_MEM
  kill(getpid(), SIGSEGV);
#else
  exit(1);
#endif /* DEBUG_MEM */
}

static void got_cont(int z)
{
  detected(DETECT_SIGCONT, "POSSIBLE HIJACK DETECTED");
}

/* A call to resolver (gethostbyname, etc) timed out
 */
static void got_alarm(int) __attribute__((noreturn));

static void got_alarm(int z)
{
  longjmp(alarmret, 1);

  /* -Never reached- */
}

/* Got ILL signal -- log context and continue
 */
static void got_ill(int z)
{
#ifdef DEBUG_CONTEXT
  putlog(LOG_MISC, "*", "* Context: %s/%d [%s]", cx_file[cx_ptr], cx_line[cx_ptr],
                         (cx_note[cx_ptr][0]) ? cx_note[cx_ptr] : "");
#endif /* DEBUG_CONTEXT */
}

static void got_hup(int) __attribute__((noreturn));

static void
got_hup(int z)
{
  putlog(LOG_MISC, "*", "GOT SIGHUP -- RESTARTING");
  restart(-1);
}

void init_signals() 
{
  signal(SIGBUS, got_bus);
  signal(SIGSEGV, got_segv);
  signal(SIGFPE, got_fpe);
  signal(SIGTERM, got_term);
  signal(SIGCONT, got_cont);
  signal(SIGABRT, got_abort);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGILL, got_ill);
  signal(SIGALRM, got_alarm);
  signal(SIGHUP, got_hup);
}

#ifdef DEBUG_CONTEXT
/* Context */
void eggContext(const char *file, int line, const char *module)
{
  char x[31] = "", *p = NULL;

  p = strrchr(file, '/');
  if (!module) {
    strncpyz(x, p ? p + 1 : file, sizeof x);
  } else
    egg_snprintf(x, 31, "%s:%s", module, p ? p + 1 : file);
  cx_ptr = ((cx_ptr + 1) & 15);
  strcpy(cx_file[cx_ptr], x);
  cx_line[cx_ptr] = line;
  cx_note[cx_ptr][0] = 0;
}

/* Called from the ContextNote macro.
 */
void eggContextNote(const char *file, int line, const char *module, const char *note)
{
  char x[31] = "", *p = NULL;

  p = strrchr(file, '/');
  if (!module) {
    strncpyz(x, p ? p + 1 : file, sizeof x);
  } else {
    egg_snprintf(x, 31, "%s:%s", module, p ? p + 1 : file);
  }
  cx_ptr = ((cx_ptr + 1) & 15);
  strcpy(cx_file[cx_ptr], x);
  cx_line[cx_ptr] = line;
  strncpyz(cx_note[cx_ptr], note, sizeof cx_note[cx_ptr]);
}
#endif

#ifdef DEBUG_ASSERT
/* Called from the Assert macro.
 */
void eggAssert(const char *file, int line, const char *module)
{
  if (!module)
    putlog(LOG_MISC, "*", "* In file %s, line %u", file, line);
  else
    putlog(LOG_MISC, "*", "* In file %s:%s, line %u", module, file, line);
#ifdef DEBUG_CONTEXT
  write_debug();
#endif /* DEBUG_CONTEXT */
  fatal("ASSERT FAILED -- CRASHING!", 1);
}
#endif /* DEBUG_ASSERT */
