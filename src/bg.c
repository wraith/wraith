#include "main.h"
#include <signal.h>
#include "bg.h"
#ifdef HAVE_TCL_THREADS
#define SUPPORT_THREADS
#endif
extern char pid_file[], botnetnick[];
#ifdef SUPPORT_THREADS
typedef struct
{
  enum
  { BG_COMM_QUIT, BG_COMM_ABORT, BG_COMM_TRANSFERPF } comm_type;
  union
  {
    struct
    {
      int len;
    } transferpf;
  } comm_data;
} bg_comm_t;
typedef enum
{ BG_NONE = 0, BG_SPLIT, BG_PARENT } bg_state_t;
typedef struct
{
  int comm_recv;
  int comm_send;
  bg_state_t state;
  pid_t child_pid;
} bg_t;
static bg_t bg = { 0 };
#endif
static void
bg_do_detach (pid_t p)
{
  FILE *fp;
  unlink (pid_file);
  fp = fopen (pid_file, "w");
  if (fp != NULL)
    {
      fprintf (fp, "%u\n", p);
      if (fflush (fp))
	{
	  printf (EGG_NOWRITE, pid_file);
	  printf ("  Try freeing some disk space\n");
	  fclose (fp);
	  unlink (pid_file);
	  exit (1);
	}
      fclose (fp);
    }
  else
    printf (EGG_NOWRITE, pid_file);
#ifdef HUB
  printf ("Launched into the background  (pid: %d)\n\n", p);
#else
  printf ("%s launched into the background  (pid: %d)\n\n", botnetnick, p);
#endif
#if HAVE_SETPGID
  setpgid (p, p);
#endif
  exit (0);
}

void
bg_prepare_split (void)
{
#ifdef SUPPORT_THREADS
  pid_t p;
  bg_comm_t message;
  {
    int comm_pair[2];
    if (pipe (comm_pair) < 0)
      fatal ("CANNOT OPEN PIPE.", 0);
    bg.comm_recv = comm_pair[0];
    bg.comm_send = comm_pair[1];
  }
  p = fork ();
  if (p == -1)
    fatal ("CANNOT FORK PROCESS.", 0);
  if (p == 0)
    {
      bg.state = BG_SPLIT;
      return;
    }
  else
    {
      bg.child_pid = p;
      bg.state = BG_PARENT;
    }
  while (read (bg.comm_recv, &message, sizeof (message)) > 0)
    {
      switch (message.comm_type)
	{
	case BG_COMM_QUIT:
	  bg_do_detach (p);
	  break;
	case BG_COMM_ABORT:
	  exit (1);
	  break;
	case BG_COMM_TRANSFERPF:
	  if (message.comm_data.transferpf.len >= 40)
	    message.comm_data.transferpf.len = 40 - 1;
	  if (read (bg.comm_recv, pid_file, message.comm_data.transferpf.len)
	      <= 0)
	    goto error;
	  pid_file[message.comm_data.transferpf.len] = 0;
	  break;
	}
    }
error:fatal ("COMMUNICATION THROUGH PIPE BROKE.", 0);
#endif
}

#ifdef SUPPORT_THREADS
static void
bg_send_pidfile (void)
{
  bg_comm_t message;
  message.comm_type = BG_COMM_TRANSFERPF;
  message.comm_data.transferpf.len = strlen (pid_file);
  if (write (bg.comm_send, &message, sizeof (message)) < 0)
    goto error;
  if (write (bg.comm_send, pid_file, message.comm_data.transferpf.len) < 0)
    goto error;
  return;
error:fatal ("COMMUNICATION THROUGH PIPE BROKE.", 0);
}
#endif
void
bg_send_quit (bg_quit_t q)
{
#ifdef SUPPORT_THREADS
  if (bg.state == BG_PARENT)
    {
      kill (bg.child_pid, SIGKILL);
    }
  else if (bg.state == BG_SPLIT)
    {
      bg_comm_t message;
      if (q == BG_QUIT)
	{
	  bg_send_pidfile ();
	  message.comm_type = BG_COMM_QUIT;
	}
      else
	message.comm_type = BG_COMM_ABORT;
      if (write (bg.comm_send, &message, sizeof (message)) < 0)
	fatal ("COMMUNICATION THROUGH PIPE BROKE.", 0);
    }
#endif
}
void
bg_do_split (void)
{
#ifdef SUPPORT_THREADS
  bg_send_quit (BG_QUIT);
#else
  int xx = fork ();
  if (xx == -1)
    fatal ("CANNOT FORK PROCESS.", 0);
  if (xx != 0)
    bg_do_detach (xx);
#endif
}
