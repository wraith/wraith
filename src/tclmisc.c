/*
 * tclmisc.c -- handles:
 *   Tcl stubs for everything else
 *
 */

#include <sys/stat.h>
#include "main.h"
#include "modules.h"
#include "tandem.h"
#ifdef HAVE_UNAME
#include <sys/utsname.h>
#endif

extern p_tcl_bind_list	 bind_table_list;
extern tcl_timer_t	*timer, *utimer;
extern struct dcc_t	*dcc;
extern char		 origbotname[], botnetnick[], quit_msg[];
extern struct userrec	*userlist;
extern time_t		 now;
extern module_entry	*module_list;
extern int timesync;
extern Tcl_Interp *interp;

int expmem_tclmisc()
{
  int tot = 0;
  return tot;
}

static int tcl_putlog STDVAR
{
  char logtext[501];

  BADARGS(2, 2, " text");
  strncpyz(logtext, argv[1], sizeof logtext);
  putlog(LOG_MISC, "*", "%s", logtext);
  return TCL_OK;
}

static int tcl_unixtime STDVAR
{
  char s[11];

  BADARGS(1, 1, "");
  egg_snprintf(s, sizeof s, "%lu", (unsigned long) now);
  Tcl_AppendResult(irp, s, NULL);
  return TCL_OK;
}

static int tcl_ctime STDVAR
{
  time_t tt;
  char s[25];

  BADARGS(2, 2, " unixtime");
  tt = (time_t) atol(argv[1]);
  strncpyz(s, ctime(&tt), sizeof s);
  Tcl_AppendResult(irp, s, NULL);
  return TCL_OK;
}

static int tcl_strftime STDVAR
{
  char buf[512];
  struct tm *tm1;
  time_t t;

  BADARGS(2, 3, " format ?time?");
  if (argc == 3)
    t = atol(argv[2]);
  else
    t = now;
    tm1 = localtime(&t);
  if (egg_strftime(buf, sizeof(buf) - 1, argv[1], tm1)) {
    Tcl_AppendResult(irp, buf, NULL);
    return TCL_OK;
  }
  Tcl_AppendResult(irp, " error with strftime", NULL);
  return TCL_ERROR;
}

static int tcl_myip STDVAR
{
  char s[16];

  BADARGS(1, 1, "");
  egg_snprintf(s, sizeof s, "%lu", iptolong(getmyip()));
  Tcl_AppendResult(irp, s, NULL);
  return TCL_OK;
}

static int tcl_rand STDVAR
{
  unsigned long x;
  char s[11];

  BADARGS(2, 2, " limit");
  if (atol(argv[1]) <= 0) {
    Tcl_AppendResult(irp, "random limit must be greater than zero", NULL);
    return TCL_ERROR;
  }
  x = random() % (unsigned long) (atol(argv[1]));
  egg_snprintf(s, sizeof s, "%lu", x);
  Tcl_AppendResult(irp, s, NULL);
  return TCL_OK;
}

static int tcl_timesync STDVAR
{
  char buf[50];

  egg_snprintf(buf, sizeof buf, "%li %li %d", (now+timesync), now, timesync);
  Tcl_AppendResult(irp, buf, NULL);
  return TCL_OK;
}

static int tcl_die STDVAR
{
  char s[1024];

  BADARGS(1, 2, " ?reason?");
  if (argc == 2) {
    egg_snprintf(s, sizeof s, "BOT SHUTDOWN (%s)", argv[1]);
    strncpyz(quit_msg, argv[1], 1024);
  } else {
    strncpyz(s, "BOT SHUTDOWN (No reason)", sizeof s);
    quit_msg[0] = 0;
  }
  kill_bot(s, quit_msg[0] ? quit_msg : "EXIT");
  return TCL_OK;
}

tcl_cmds tclmisc_cmds[] =
{
  {"putlog",		tcl_putlog},
  {"unixtime",		tcl_unixtime},
  {"strftime",          tcl_strftime},
  {"ctime",		tcl_ctime},
  {"myip",		tcl_myip},
  {"rand",		tcl_rand},
  {"timesync",		tcl_timesync},
  {"exit",		tcl_die},
  {"die",		tcl_die},
  {NULL,		NULL}
};
