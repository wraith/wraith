/*
 * bg.c -- handles:
 *   moving the process to the background, i.e. forking, while keeping threads
 *   happy.
 *
 */

#include "common.h"
#include "bg.h"
#include "main.h"
#include <signal.h>


extern time_t	lastfork, now;
extern conf_t	conf;

/* Do everything we normally do after we have split off a new
 * process to the background. This includes writing a PID file
 * and informing the user of the split.
 */
static void bg_do_detach(pid_t p)
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
  printf("%s launched into the background  (pid: %d ppid: %d)\n\n", conf.bot->nick, p, getpid());
#endif /* HUB */

#if HAVE_SETPGID
  setpgid(p, p);
#endif
  exit(0);
}

void bg_do_split(void)
{
  /* Split off a new process.
   */
  int xx = fork();

  if (xx == -1)
    fatal("CANNOT FORK PROCESS.", 0);
  if (xx != 0)
    bg_do_detach(xx);
}

void do_fork() {
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
