
/* 
 * tclmisc.c -- handles:
 *   Tcl stubs for file system commands
 *   Tcl stubs for everything else
 * 
 * dprintf'ized, 1aug1996
 * 
 * $Id: tclmisc.c,v 1.10 2000/01/17 16:14:45 per Exp $
 */

/* 
 * Copyright (C) 1997  Robey Pointer
 * Copyright (C) 1999, 2000  Eggheads
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

#include "main.h"
#include <sys/stat.h>
#include "hook.h"
#include "tandem.h"
#include <errno.h>
#ifdef HAVE_UNAME
#include <sys/utsname.h>
#endif
#ifdef G_USETCL
extern tcl_timer_t *timer,
 *utimer;
extern struct dcc_t *dcc;
extern char origbotname[],
  botnetnick[],
  localkey[];
extern struct userrec *userlist;
extern time_t now;
extern module_entry *module_list;

/***********************************************************************/

int tcl_log STDVAR { char logtext[1024],
    category[128];
    Context;
    BADARGS(3, 3, STR(" kind text"));
    strncpy0(logtext, argv[2], 1024);
    strncpy0(category, argv[1], 128);
    log(argv[1], "%s", logtext);
    return TCL_OK;
} int tcl_timer STDVAR { unsigned long x;
  char s[41];

    Context;
    BADARGS(3, 3, STR(" minutes command"));
  if (atoi(argv[1]) < 0) {
    Tcl_AppendResult(irp, STR("time value must be positive"), NULL);
    return TCL_ERROR;
  }
  if (argv[2][0] != '#') {
    x = add_timer(&timer, atoi(argv[1]), argv[2], 0L);
    sprintf(s, STR("timer%lu"), x);
    Tcl_AppendResult(irp, s, NULL);
  }
  return TCL_OK;
}

int tcl_utimer STDVAR { unsigned long x;
  char s[41];

    Context;
    BADARGS(3, 3, STR(" seconds command"));
  if (atoi(argv[1]) < 0) {
    Tcl_AppendResult(irp, STR("time value must be positive"), NULL);
    return TCL_ERROR;
  }
  if (argv[2][0] != '#') {
    x = add_timer(&utimer, atoi(argv[1]), argv[2], 0L);
    sprintf(s, STR("timer%lu"), x);
    Tcl_AppendResult(irp, s, NULL);
  }
  return TCL_OK;
}

int tcl_killtimer STDVAR { Context;
  BADARGS(2, 2, STR(" timerID"));
  if (strncmp(argv[1], STR("timer"), 5) != 0) {
    Tcl_AppendResult(irp, STR("argument is not a timerID"), NULL);
    return TCL_ERROR;
  }
  if (remove_timer(&timer, atol(&argv[1][5])))
      return TCL_OK;

  Tcl_AppendResult(irp, STR("invalid timerID"), NULL);
  return TCL_ERROR;
}

int tcl_killutimer STDVAR { Context;
  BADARGS(2, 2, STR(" timerID"));
  if (strncmp(argv[1], STR("timer"), 5) != 0) {
    Tcl_AppendResult(irp, STR("argument is not a timerID"), NULL);
    return TCL_ERROR;
  }
  if (remove_timer(&utimer, atol(&argv[1][5])))
      return TCL_OK;

  Tcl_AppendResult(irp, STR("invalid timerID"), NULL);
  return TCL_ERROR;
}

int tcl_duration STDVAR { char s[256];
  time_t sec;
    BADARGS(2, 2, STR(" seconds"));

  if (atol(argv[1]) <= 0) {
    Tcl_AppendResult(irp, STR("0 seconds"), NULL);
    return TCL_OK;
  }
  sec = atoi(argv[1]);

  s[0] = 0;

  if (sec >= 31536000) {
    sprintf(s, STR("%d year"), (int) (sec / 31536000));
    if ((int) (sec / 31536000) > 1)
      strcat(s, "s");
    strcat(s, " ");
    sec -= (((int) (sec / 31536000)) * 31536000);
  }
  if (sec >= 604800) {
    sprintf(&s[strlen(s)], STR("%d week"), (int) (sec / 604800));
    if ((int) (sec / 604800) > 1)
      strcat(s, "s");
    strcat(s, " ");
    sec -= (((int) (sec / 604800)) * 604800);
  }
  if (sec >= 86400) {
    sprintf(&s[strlen(s)], STR("%d day"), (int) (sec / 86400));
    if ((int) (sec / 86400) > 1)
      strcat(s, "s");
    strcat(s, " ");
    sec -= (((int) (sec / 86400)) * 86400);
  }
  if (sec >= 3600) {
    sprintf(&s[strlen(s)], STR("%d hour"), (int) (sec / 3600));
    if ((int) (sec / 3600) > 1)
      strcat(s, "s");
    strcat(s, " ");
    sec -= (((int) (sec / 3600)) * 3600);
  }
  if (sec >= 60) {
    sprintf(&s[strlen(s)], STR("%d minute"), (int) (sec / 60));
    if ((int) (sec / 60) > 1)
      strcat(s, "s");
    strcat(s, " ");
    sec -= (((int) (sec / 60)) * 60);
  }
  if (sec > 0) {
    sprintf(&s[strlen(s)], STR("%d second"), (int) (sec / 1));
    if ((int) (sec / 1) > 1)
      strcat(s, "s");
  }
  Tcl_AppendResult(irp, s, NULL);
  return TCL_OK;
}

int tcl_unixtime STDVAR { char s[20];

    Context;
    BADARGS(1, 1, "");
    sprintf(s, STR("%lu"), (unsigned long) now);
    Tcl_AppendResult(irp, s, NULL);
    return TCL_OK;
} int tcl_timers STDVAR { Context;
  BADARGS(1, 1, "");
  list_timers(irp, timer);
  return TCL_OK;
} int tcl_utimers STDVAR { Context;
  BADARGS(1, 1, "");
  list_timers(irp, utimer);
  return TCL_OK;
} int tcl_ctime STDVAR { time_t tt;
  char s[81];

    Context;
    BADARGS(2, 2, STR(" unixtime"));
    tt = (time_t) atol(argv[1]);
    strcpy(s, ctime(&tt));
    s[strlen(s) - 1] = 0;
    Tcl_AppendResult(irp, s, NULL);
    return TCL_OK;
} int tcl_myip STDVAR { char s[21];

    Context;
    BADARGS(1, 1, "");
    sprintf(s, STR("%lu"), iptolong(getmyip()));
    Tcl_AppendResult(irp, s, NULL);
    return TCL_OK;
} int tcl_rand STDVAR { unsigned long x;
  char s[41];

    Context;
    BADARGS(2, 2, STR(" limit"));
  if (atol(argv[1]) <= 0) {
    Tcl_AppendResult(irp, STR("random limit must be greater than zero"), NULL);
    return TCL_ERROR;
  }
  x = random() % (atol(argv[1]));

  sprintf(s, STR("%lu"), x);
  Tcl_AppendResult(irp, s, NULL);
  return TCL_OK;
}

#ifdef HUB
int tcl_backup STDVAR { Context;
  BADARGS(1, 1, "");
  backup_userfile();
  return TCL_OK;
}
#endif
int tcl_die STDVAR { char s[1024];
  char g[1024];

    Context;
    BADARGS(1, 2, STR(" ?reason?"));
  if (argc == 2) {
    simple_sprintf(s, STR("BOT SHUTDOWN (%s)"), argv[1]);
    simple_sprintf(g, "%s", argv[1]);
  } else {
    simple_sprintf(s, STR("BOT SHUTDOWN (authorized by a canadian)"));
    simple_sprintf(g, STR("EXIT"));
  }
  log(LCAT_BOT, STR("%s\n"), s);
  botnet_send_chat(-1, botnetnick, s);
  botnet_send_bye();
#ifdef HUB
  write_userfile(-1);
#endif
  fatal(g, 0);
  /* should never return, but, to keep gcc happy: */
  return TCL_OK;
}

int tcl_strftime STDVAR { char buf[512];
  struct tm *tm1;
  time_t t;

    Context;
    BADARGS(2, 3, STR(" format ?time?"));
  if (argc == 3)
      t = atol(argv[2]);
  else
      t = now;
    tm1 = localtime(&t);
  if (strftime(buf, sizeof(buf) - 1, argv[1], tm1)) {
    Tcl_AppendResult(irp, buf, NULL);
    return TCL_OK;
  }
  Tcl_AppendResult(irp, STR(" error with strftime"), NULL);

  return TCL_ERROR;
}

int tcl_unames STDVAR { char *unix_n,
   *vers_n;
#ifdef HAVE_UNAME
  struct utsname un;
  if (uname(&un) < 0) {
#endif
    unix_n = STR("*unkown*");
    vers_n = "";
#ifdef HAVE_UNAME
  } else {
    unix_n = un.sysname;
    vers_n = un.release;
  }
#endif
  Tcl_AppendResult(irp, unix_n, " ", vers_n, NULL);
  return TCL_OK;
}

int tcl_modules STDVAR { module_entry *current;
  dependancy *dep;
  char *list[100],
   *list2[2],
   *p;
  char s[40],
    s2[40];
  int i;

    Context;
    BADARGS(1, 1, "");
  for (current = module_list; current; current = current->next) {
    list[0] = current->name;
    simple_sprintf(s, STR("%d.%d"), current->major, current->minor);
    list[1] = s;
    i = 2;
    for (dep = dependancy_list; dep && (i < 100); dep = dep->next) {
      if (dep->needing == current) {
	list2[0] = dep->needed->name;
	simple_sprintf(s2, STR("%d.%d"), dep->major, dep->minor);
	list2[1] = s2;
	list[i] = Tcl_Merge(2, list2);
	i++;
      }
    } p = Tcl_Merge(i, list);
    Tcl_AppendElement(irp, p);
    Tcl_Free((char *) p);
    while (i > 2) {
      i--;
      Tcl_Free((char *) list[i]);
    }
  }
  return TCL_OK;
}

int tcl_isnumeric STDVAR { char *ep;
    Context;
    BADARGS(2, 2, STR(" arg"));
    errno = 0;
    strtol(argv[1], &ep, 0);
  if ((errno) && (argv[1][0] != '\0') && (*ep))
      Tcl_AppendResult(irp, "1", NULL);
  else
      Tcl_AppendResult(irp, "0", NULL);
    return TCL_OK;
} tcl_cmds tclmisc_cmds[] = {
  {"log", tcl_log}
  ,
  {"timer", tcl_timer}
  ,
  {"utimer", tcl_utimer}
  ,
  {"killtimer", tcl_killtimer}
  ,
  {"killutimer", tcl_killutimer}
  ,
  {"unixtime", tcl_unixtime}
  ,
  {"timers", tcl_timers}
  ,
  {"utimers", tcl_utimers}
  ,
  {"ctime", tcl_ctime}
  ,
  {"myip", tcl_myip}
  ,
  {"rand", tcl_rand}
  ,
#ifdef HUB
  {"backup", tcl_backup}
  ,
#endif
  {"exit", tcl_die}
  ,
  {"die", tcl_die}
  ,
  {"strftime", tcl_strftime}
  ,
  {"unames", tcl_unames}
  ,
  {"modules", tcl_modules}
  ,
  {"duration", tcl_duration}
  ,
  {"isnumeric", tcl_isnumeric}
  ,
  {0, 0}
};

#endif
