/*
 * bg.c -- handles:
 *   moving the process to the background, i.e. forking, while keeping threads
 *   happy.
 *
 */

#include "main.h"
#include <signal.h>


extern char	pid_file[], botnetnick[];

/* Do everything we normally do after we have split off a new
 * process to the background. This includes writing a PID file
 * and informing the user of the split.
 */
static void bg_do_detach(pid_t p)
{
  FILE	*fp;

  /* Need to attempt to write pid now, not later. */
  unlink(pid_file);
  fp = fopen(pid_file, "w");
  if (fp != NULL) {
    fprintf(fp, "%u\n", p);
    if (fflush(fp)) {
      /* Kill bot incase a botchk is run from crond. */
      printf(EGG_NOWRITE, pid_file);
      printf("  Try freeing some disk space\n");
      fclose(fp);
      unlink(pid_file);
      exit(1);
    }
    fclose(fp);
  } else
    printf(EGG_NOWRITE, pid_file);
#ifdef HUB
  printf("Launched into the background  (pid: %d)\n\n", p);
#else
  printf("%s launched into the background  (pid: %d ppid: %d)\n\n", botnetnick, p, getpid());
#endif
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
