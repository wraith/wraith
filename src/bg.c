/*
 * bg.c -- handles:
 *   moving the process to the background, i.e. forking, while keeping threads
 *   happy.
 *
 */

#include "common.h"
#include "bg.h"
#include "thread.h"
#include "main.h"
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

time_t lastfork = 0;
pid_t watcher;                  /* my child/watcher */

/* Do everything we normally do after we have split off a new
 * process to the background. This includes writing a PID file
 * and informing the user of the split.
 */
static void bg_do_detach(pid_t) __attribute__ ((noreturn));

static void
bg_do_detach(pid_t p)
{
  FILE *fp = NULL;

  /* Need to attempt to write pid now, not later. */
  unlink(conf.bot->pid_file);
  fp = fopen(conf.bot->pid_file, "w");
  if (fp != NULL) {
    fprintf(fp, "%u\n", p);
    if (fflush(fp)) {
      /* Kill bot incase a botchk is run from crond. */
      printf(EGG_NOWRITE, conf.bot->pid_file);
      printf("  Try freeing some disk space\n");
      fclose(fp);
      unlink(conf.bot->pid_file);
      exit(1);
    }
    fclose(fp);
  } else
    printf(EGG_NOWRITE, conf.bot->pid_file);
#ifdef HUB
  printf("Launched into the background  (pid: %d)\n\n", p);
#else /* LEAF */
  printf("%s launched into the background  (pid: %d)\n\n", conf.bot->nick, p);
#endif /* HUB */

#ifndef CYGWIN_HACKS
  setpgid(p, p);
#endif /* !CYGWIN_HACKS */
  exit(0);
}

static void
init_watcher()
{
  if (conf.watcher) {
    int x = 0;
    pid_t parent = getpid();

    x = fork();

    if (x == -1)
      fatal("Could not fork off a watcher process", 0);
    if (x != 0) {               /* parent [bot] */
      watcher = x;
      /* printf("WATCHER: %d\n", watcher); */
      return;
    } else {                    /* child */
      watcher = getpid();
      /* printf("MY PARENT: %d\n", parent); */
      /* printf("my pid: %d\n", watcher); */
      if (ptrace(PT_ATTACH, parent, 0, 0) == -1)
        fatal("Cannot attach to parent", 0);

      while (1) {
        int status = 0, sig = 0, ret = 0;

        waitpid(parent, &status, 0);
        sig = WSTOPSIG(status);
        if (sig) {
          ret = ptrace(PT_CONTINUE, parent, (char *) 1, sig);
          if (ret == -1)        /* send the signal! */
            fatal("Could not send signal to parent", 0);
          /* printf("Sent signal %s (%d) to parent\n", strsignal(sig), sig); */
        } else {
          ret = ptrace(PT_CONTINUE, parent, (char *) 1, 0);
          if (ret == -1) {
            if (errno == ESRCH) /* parent is gone! */
              exit(0);          /* just exit */
            else
              fatal("Could not continue parent", 0);
          }
        }
      }
    }
  }
}

void
bg_do_split(void)
{
  /* Split off a new process.
   */
  int xx = fork();

  if (xx == -1)
    fatal("CANNOT FORK PROCESS.", 0);
  if (xx != 0)                  /* parent */
    bg_do_detach(xx);
  else                          /* child */
    init_watcher();
/*    init_thread(getpid()); */
#ifdef CRAZY_TRACE
  /* block ptrace? */
#  ifdef __linux__
  ptrace(PTRACE_TRACEME, 0, NULL, NULL);
#  endif
#  ifdef BSD
  ptrace(PT_TRACE_ME, 0, NULL, NULL);
#  endif /* BSD */
#endif /* CRAZY_TRACE */

}

void
do_fork()
{
  int xx;

  xx = fork();
  if (xx == -1)
    return;
  if (xx != 0) {
    FILE *fp;

    unlink(conf.bot->pid_file);
    fp = fopen(conf.bot->pid_file, "w");
    if (fp != NULL) {
      fprintf(fp, "%u\n", xx);
      fclose(fp);
    }
  }
  if (xx) {
#if HAVE_SETPGID
    setpgid(xx, xx);
#endif
    exit(0);
  }
  lastfork = now;
}
