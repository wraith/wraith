/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2008 Bryan Drewery
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


#ifdef DEBUG_CONTEXT
/* Context storage for fatal crashes */
char    cx_file[16][16];
char    cx_note[16][16];
int     cx_line[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int     cx_ptr = 0;
#endif /* DEBUG_CONTEXT */

void setlimits()
{
#ifndef CYGWIN_HACKS
  struct rlimit plim, fdlim, corelim;
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
  plim.rlim_cur = 60;
  plim.rlim_max = 60;
  corelim.rlim_cur = 0;
  corelim.rlim_max = 0;
#else /* DEBUG */
  plim.rlim_cur = 500;
  plim.rlim_max = 500;
  corelim.rlim_cur = RLIM_INFINITY;
  corelim.rlim_max = RLIM_INFINITY;
#endif /* !DEBUG */
  setrlimit(RLIMIT_CORE, &corelim);
#ifndef __sun__
  setrlimit(RLIMIT_NPROC, &plim);
#endif
  fdlim.rlim_cur = 300;
  fdlim.rlim_max = 300;
  setrlimit(RLIMIT_NOFILE, &fdlim);
#endif /* !CYGWIN_HACKS */
}

void init_debug()
{
  for (int i = 0; i < 16; i ++)
    Context;

#ifdef DEBUG_CONTEXT
  bzero(&cx_file, sizeof cx_file);
  bzero(&cx_note, sizeof cx_note);
#endif /* DEBUG_CONTEXT */
}

void sdprintf (const char *format, ...)
{
#ifndef DEBUG
  if (sdebug) {
#endif
    char s[2001] = "";
    va_list va;

    va_start(va, format);
    egg_vsnprintf(s, sizeof(s), format, va);
    va_end(va);
    
    remove_crlf(s);

#ifdef DEBUG
    if (sdebug) {
#endif
      if (!backgrd)
        dprintf(DP_STDOUT, "[D:%d] %s%s%s\n", mypid, BOLD(-1), s, BOLD_END(-1));
      else
        printf("[D:%d] %s%s%s\n", mypid, BOLD(-1), s, BOLD_END(-1));
#ifdef DEBUG
    }
    logfile(LOG_DEBUG, s);
#else
  }
#endif
}

#ifdef DEBUG_CONTEXT

#define CX(ptr) cx_file[ptr] && cx_file[ptr][0] ? cx_file[ptr] : "", cx_line[ptr], cx_note[ptr] && cx_note[ptr][0] ? cx_note[ptr] : ""
static void write_debug()
{
  char tmpout[150] = "";

  simple_snprintf(tmpout, sizeof tmpout, "* Last 3 contexts: %s/%d [%s], %s/%d [%s], %s/%d [%s]",
                                  CX(cx_ptr - 2), CX(cx_ptr - 1), CX(cx_ptr));
  putlog(LOG_MISC, "*", "%s (Paste to bryan)", tmpout);
  printf("%s\n", tmpout);
}
#endif /* DEBUG_CONTEXT */

#ifndef DEBUG
static void got_bus(int) __attribute__ ((noreturn));
#endif /* DEBUG */

static void got_bus(int z)
{
  signal(SIGBUS, SIG_DFL);
#ifdef DEBUG_CONTEXT
  write_debug();
#endif
  fatal("BUS ERROR -- CRASHING!", 1);
#ifdef DEBUG
  raise(SIGBUS);
#else
  exit(1);
#endif /* DEBUG */
}

#ifndef DEBUG
static void got_segv(int) __attribute__ ((noreturn));
#endif /* DEBUG */

static void got_segv(int z)
{
  segfaulted = 1;
  alarm(0);		/* dont let anything jump out of this signal! */
  signal(SIGSEGV, SIG_DFL);
#ifdef DEBUG_CONTEXT
  write_debug();
#endif
  fatal("SEGMENT VIOLATION -- CRASHING!", 1);
#ifdef DEBUG
  raise(SIGSEGV);
#else
  exit(1);
#endif /* DEBUG */
}

static void got_fpe(int) __attribute__ ((noreturn));

static void got_fpe(int z)
{
#ifdef DEBUG_CONTEXT
  write_debug();
#endif
  fatal("FLOATING POINT ERROR -- CRASHING!", 0);
  exit(1);		/* for GCC noreturn */
}

static void got_term(int) __attribute__ ((noreturn));

static void got_term(int z)
{
  if (conf.bot->hub)
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
#ifdef DEBUG_CONTEXT
  write_debug();
#endif
  fatal("GOT SIGABRT -- CRASHING!", 1);
#ifdef DEBUG
  raise(SIGSEGV);
#else
  exit(1);
#endif /* DEBUG */
}

#ifndef CYGWIN_HACKS
static void got_cont(int z)
{
  detected(DETECT_HIJACK, "POSSIBLE HIJACK DETECTED (!! MAY BE BOX REBOOT !!)");
}
#endif /* !CYGWIN_HACKS */

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
#ifdef DEBUG_CONTEXT
  putlog(LOG_MISC, "*", "* Context: %s/%d [%s]", cx_file[cx_ptr], cx_line[cx_ptr],
                         (cx_note[cx_ptr][0]) ? cx_note[cx_ptr] : "");
#endif /* DEBUG_CONTEXT */
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
  signal(SIGBUS, got_bus);
  signal(SIGSEGV, got_segv);
  signal(SIGFPE, got_fpe);
  signal(SIGTERM, got_term);
#ifndef CYGWIN_HACKS
  signal(SIGCONT, got_cont);
#endif /* !CYGWIN_HACKS */
  signal(SIGABRT, got_abort);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGILL, got_ill);
  signal(SIGALRM, got_alarm);
  signal(SIGHUP, got_hup);
  signal(SIGUSR1, got_usr1);
}

#ifdef DEBUG_CONTEXT
/* Context */
void eggContext(const char *file, int line)
{
  char x[31] = "", *p = strrchr(file, '/');

  strlcpy(x, p ? p + 1 : file, sizeof x);
  cx_ptr = ((cx_ptr + 1) & 15);
  strcpy(cx_file[cx_ptr], x);
  cx_line[cx_ptr] = line;
  cx_note[cx_ptr][0] = 0;
}

/* Called from the ContextNote macro.
 */
void eggContextNote(const char *file, int line, const char *note)
{
  char x[31] = "", *p = strrchr(file, '/');

  strlcpy(x, p ? p + 1 : file, sizeof x);
  cx_ptr = ((cx_ptr + 1) & 15);
  strcpy(cx_file[cx_ptr], x);
  cx_line[cx_ptr] = line;
  strlcpy(cx_note[cx_ptr], note, sizeof cx_note[cx_ptr]);
}
#endif

