/*
 * debug.c -- handles:
 *   signal handling
 *   Context handling
 *   debug funtions
 *
 */


#include "common.h"
#include "debug.h"
#include "net.h"
#include "misc.h"
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

extern int 		 sdebug, backgrd, do_restart;
extern char		 tempdir[], origbotname[], ver[];
extern time_t		 now, buildts;
extern jmp_buf           alarmret;

void setlimits()
{
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
}

void init_debug()
{
  int i = 0;
  for (i = 0; i < 16; i ++)
    Context;
}

int     sdebug = 0;             /* enable debug output? */

void sdprintf (char *format, ...)
{
  if (sdebug) {
    char s[2001];
    va_list va;

    va_start(va, format);
    egg_vsnprintf(s, 2000, format, va);
    va_end(va);
    if (!backgrd)
      dprintf(DP_STDOUT, "[D:%d] %s\n", getpid(), s);
    else
      putlog(LOG_MISC, "*", "[D:%d] %s", getpid(), s);
  }
}


#ifdef DEBUG_CONTEXT
/* Context storage for fatal crashes */
char    cx_file[16][30];
char    cx_note[16][256];
int     cx_line[16];
int     cx_ptr = 0;

static int      nested_debug = 0;

void write_debug()
{
  int x;
  char s[25], tmpout[150], buf[DIRMAX];
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
      dprintf(-x, STR("Debug (%s) written %s\n"), ver, s);
      dprintf(-x, STR("Context: "));
      cx_ptr = cx_ptr & 15;
      for (y = ((cx_ptr + 1) & 15); y != cx_ptr; y = ((y + 1) & 15))
        dprintf(-x, "%s/%d,\n         ", cx_file[y], cx_line[y]);
      dprintf(-x, "%s/%d\n\n", cx_file[y], cx_line[y]);
      killsock(x);
      close(x);
    }
    {
      /* Use this lame method because shell_exec() or mail() may have caused another segfault :o */
      char buff[255];

      egg_snprintf(buff, sizeof(buff), "cat << EOFF >> %sbleh\nDEBUG from: %s\n`date`\n`w`\n---\n`who`\n---\n`ls -al`\n---\n`ps ux`\n---\n`uname -a`\n---\n`id`\n---\n`cat %s`\nEOFF", tempdir, origbotname, buf);

      system(buff);
      egg_snprintf(buff, sizeof(buff), "cat %sbleh |mail wraith@shatow.net", tempdir);
      system(buff);
      unlink("bleh");
    }
    unlink(buf);
    exit(1);                    /* Dont even try & tell people about, that may
                                   have caused the fault last time. */
  } else
    nested_debug = 1;

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
    char date[80];
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
    {

      char date[81], *w = NULL, *who = NULL, *ps = NULL, *uname = NULL, *id = NULL, *ls = NULL, *debug = NULL, *msg = NULL, buf2[DIRMAX];

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

      msg = malloc(strlen(date) + strlen(id) + strlen(uname) + strlen(w) + strlen(who) + strlen(ps) + strlen(ls) + strlen(debug) + 50);

      msg[0] = 0;
      sprintf(msg, "%s\n%s\n%s\n\n%s\n%s\n\n%s\n\n-----%s-----\n\n\n\n%s", date, id, uname, w, who, ps, ls, debug);
      email("Debug output", msg, EMAIL_TEAM);
      free(msg);
    }
    unlink(buf);
    putlog(LOG_MISC, "*", "* Emailed DEBUG to development team...");
  }
}
#endif



static void got_bus(int z)
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

static void got_segv(int z)
{
#ifdef DEBUG_CONTEXT
  write_debug();
#endif
  fatal(STR("SEGMENT VIOLATION -- CRASHING!"), 1);
#ifdef SA_RESETHAND
  kill(getpid(), SIGSEGV);
#else
  exit(1);
#endif
}

static void got_fpe(int z)
{
#ifdef DEBUG_CONTEXT
  write_debug();
#endif
  fatal(STR("FLOATING POINT ERROR -- CRASHING!"), 0);
}

static void got_term(int z)
{
#ifdef HUB
  write_userfile(-1);
#endif
  putlog(LOG_MISC, "*", STR("RECEIVED TERMINATE SIGNAL (IGNORING)"));
}

static void got_abort(int z)
{
#ifdef DEBUG_CONTEXT
  write_debug();
#endif
  fatal(STR("GOT SIGABRT -- CRASHING!"), 1);
#ifdef SA_RESETHAND
  kill(getpid(), SIGSEGV);
#else
  exit(1);
#endif
}

#ifdef S_HIJACKCHECK
static void got_cont(int z)
{
  detected(DETECT_SIGCONT, STR("POSSIBLE HIJACK DETECTED"));
}
#endif

static void got_quit(int z)
{
  putlog(LOG_MISC, "*", STR("RECEIVED QUIT SIGNAL (IGNORING)"));
  return;
}

static void got_hup(int z)
{
#ifdef HUB
  write_userfile(-1);
#endif
  putlog(LOG_MISC, "*", STR("Received HUP signal: rehashing..."));
  do_restart = -2;
  return;
}

/* A call to resolver (gethostbyname, etc) timed out
 */
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
  putlog(LOG_MISC, "*", STR("* Context: %s/%d [%s]"), cx_file[cx_ptr], cx_line[cx_ptr],
                         (cx_note[cx_ptr][0]) ? cx_note[cx_ptr] : "");
#endif
}


void init_signals() 
{
  struct sigaction sv;

  /* Set up error traps: */
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
#ifdef S_HIJACKCHECK
  sv.sa_handler = got_cont;
  sigaction(SIGCONT, &sv, NULL);
#endif
  sv.sa_handler = got_abort;
  sigaction(SIGABRT, &sv, NULL);
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
}

#ifdef DEBUG_CONTEXT
/* Context */
void eggContext(const char *file, int line, const char *module)
{
  char x[31], *p;

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
void eggContextNote(const char *file, int line, const char *module,
                    const char *note)
{
  char x[31], *p;

  p = strrchr(file, '/');
  if (!module) {
    strncpyz(x, p ? p + 1 : file, sizeof x);
  } else
    egg_snprintf(x, 31, "%s:%s", module, p ? p + 1 : file);
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
    putlog(LOG_MISC, "*", STR("* In file %s, line %u"), file, line);
  else
    putlog(LOG_MISC, "*", STR("* In file %s:%s, line %u"), module, file, line);
#ifdef DEBUG_CONTEXT
  write_debug();
#endif /* DEBUG_CONTEXT */
  fatal(STR("ASSERT FAILED -- CRASHING!"), 1);
}
#endif /* DEBUG_ASSERT */

