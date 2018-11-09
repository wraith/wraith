/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2014 Bryan Drewery
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
#include "binary.h"
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

bool		sdebug = 0;             /* enable debug output? */
bool		segfaulted = 0;
char	get_buf[GET_BUFS][SGRAB + 5];
size_t	current_get_buf = 0;


void setlimits()
{
  struct rlimit fdlim, corelim;
#ifndef DEBUG
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
#else /* DEBUG */
  corelim.rlim_cur = RLIM_INFINITY;
  corelim.rlim_max = RLIM_INFINITY;
#endif /* !DEBUG */
  setrlimit(RLIMIT_CORE, &corelim);
  fdlim.rlim_cur = MAX_SOCKETS;
  fdlim.rlim_max = MAX_SOCKETS;
  setrlimit(RLIMIT_NOFILE, &fdlim);
}

void sdprintf (const char *format, ...)
{
  char s[2001] = "";
  va_list va;

#ifndef DEBUG
  if (!sdebug)
    return;
#endif
  va_start(va, format);
  egg_vsnprintf(s, sizeof(s), format, va);
  va_end(va);

  remove_crlf(s);

  ContextNote("dbg", s);

  if (sdebug) {
    if (!backgrd)
      dprintf(DP_STDOUT, "[D:%lu] %s%s%s\n", (unsigned long) mypid, BOLD(-1), s, BOLD_END(-1));
    else
      printf("[D:%lu] %s%s%s\n", (unsigned long) mypid, BOLD(-1), s, BOLD_END(-1));
  }
#ifdef DEBUG
  logfile(LOG_DEBUG, s);
#endif
}

#ifdef DEBUG
void _assert(int recoverable, const char *file, int line, const char *function,
    const char *exp, const char *format, ...)
{
    char msg[1024], buf[1024];
    va_list va;

    if (format != NULL) {
      va_start(va, format);
      egg_vsnprintf(msg, sizeof(msg), format, va);
      va_end(va);
    }

    simple_snprintf(buf, sizeof(buf),
        STR("Assertion failed%s: (%s:%d:%s) [%s]%s%s"),
        recoverable == 0 ? "" : " (ignoring)",
        file, line, function, exp,
        format != NULL ? ": ": "",
        format != NULL ? msg : "");
    fatal(buf, recoverable);
}
#endif

static void write_debug(bool fatal = 1)
{
  if (fatal) {
    segfaulted = 1;
    alarm(0);		/* dont let anything jump out of this signal! */
  }

  putlog(LOG_MISC, "*", "** Paste to bryan:");
  putlog(LOG_MISC, "*", "Version: %s", egg_version);
  const size_t cur_buf = (current_get_buf == 0) ? GET_BUFS - 1 : current_get_buf - 1;
  for (size_t i = 0; i < GET_BUFS; i++)
    putlog(LOG_MISC, "*", "%c %02zu: %s", i == cur_buf ? '*' : '_', i, get_buf[i]);
  putlog(LOG_MISC, "*", "** end");

#ifdef DEBUG
  if (fatal) {
    /* Write GDB backtrace */
    char gdb[1024] = "", btfile[256] = "", std_in[101] = "", *out = NULL;

    simple_snprintf(btfile, sizeof(btfile), ".gdb-backtrace-%d", (int)getpid());

    FILE *f = fopen(btfile, "w");

    if (f) {
      strlcpy(std_in, "bt 100\nbt 100 full\ndetach\nquit\n", sizeof(std_in));
      //simple_snprintf(stdin, sizeof(stdin), "detach\n");
      //simple_snprintf(stdin, sizeof(stdin), "q\n");

      simple_snprintf(gdb, sizeof(gdb), "gdb --pid=%d %s", (int)getpid(), binname);
      shell_exec(gdb, std_in, &out, NULL);
      fprintf(f, "%s\n", out);
      fclose(f);
      free(out);
    }

    //enabling core dumps
    struct rlimit limit;
    if (!getrlimit(RLIMIT_CORE, &limit)) {
      limit.rlim_cur = limit.rlim_max;
      setrlimit(RLIMIT_CORE, &limit);
    }
  }
#endif
}

#ifndef DEBUG
static void got_bus(int) __attribute__ ((noreturn));
#endif /* DEBUG */

#ifndef __SANITIZE_ADDRESS__
static void got_bus(int z)
{
  signal(SIGBUS, SIG_DFL);
  write_debug();
  fatal("BUS ERROR -- CRASHING!", 1);
#ifdef DEBUG
  raise(SIGBUS);
#else
  exit(1);
#endif /* DEBUG */
}
#endif /* !__SANITIZE_ADDRESS__ */

#ifndef DEBUG
static void got_segv(int) __attribute__ ((noreturn));
#endif /* DEBUG */

#ifndef __SANITIZE_ADDRESS__
static void got_segv(int z)
{
  signal(SIGSEGV, SIG_DFL);
  write_debug();
  fatal("SEGMENT VIOLATION -- CRASHING!", 1);
#ifdef DEBUG
  raise(SIGSEGV);
#else
  exit(1);
#endif /* DEBUG */
}
#endif /* !__SANITIZE_ADDRESS__ */

#ifndef DEBUG
static void got_fpe(int) __attribute__ ((noreturn));
#endif /* DEBUG */

static void got_fpe(int z)
{
  signal(SIGFPE, SIG_DFL);
  write_debug();
  fatal("FLOATING POINT ERROR -- CRASHING!", 1);
#ifdef DEBUG
  raise(SIGFPE);
#else
  exit(1);
#endif /* DEBUG */
}

static void got_term(int) __attribute__ ((noreturn));

static void got_term(int z)
{
  write_userfile(-1);
  fatal("Received SIGTERM", 0);
  exit(1);		/* for GCC noreturn */
}

#ifndef DEBUG
static void got_abort(int) __attribute__ ((noreturn));
#endif /* DEBUG */

static void got_abort(int z)
{
  signal(SIGABRT, SIG_DFL);
  write_debug();
  fatal("GOT SIGABRT -- CRASHING!", 1);
#ifdef DEBUG
  raise(SIGABRT);
#else
  exit(1);
#endif /* DEBUG */
}

#ifndef DEBUG
static void got_cont(int z)
{
  detected(DETECT_HIJACK, "POSSIBLE HIJACK DETECTED (!! MAY BE BOX REBOOT !!)");
}
#endif

static void got_alarm(int) __attribute__((noreturn));

static void got_alarm(int z)
{
  sdprintf("SIGALARM");
  longjmp(alarmret, 1);

  /* -Never reached- */
}

/* Got ILL signal -- log context and continue
 */
static void got_ill(int z)
{
  write_debug(0);
}

static void
got_hup(int z)
{
  signal(SIGHUP, got_hup);
  putlog(LOG_MISC, "*", "GOT SIGHUP -- RESTARTING");
  do_restart = 1;
//  restart(-1);
}

static void
got_usr1(int z)
{
  signal(SIGUSR1, got_usr1);
  putlog(LOG_DEBUG, "*", "GOT SIGUSR1 -- RECHECKING BINARY");
  do_restart = 2;
//  reload_bin_data();
}

void init_signals() 
{
#ifndef __SANITIZE_ADDRESS__
  signal(SIGBUS, got_bus);
  signal(SIGSEGV, got_segv);
#endif
  signal(SIGFPE, got_fpe);
  signal(SIGTERM, got_term);
#ifndef DEBUG
  signal(SIGCONT, got_cont);
#endif
  signal(SIGABRT, got_abort);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGILL, got_ill);
  signal(SIGALRM, got_alarm);
  signal(SIGHUP, got_hup);
  signal(SIGUSR1, got_usr1);
}
/* vim: set sts=2 sw=2 ts=8 et: */
