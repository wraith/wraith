
/* 
 * tcl.c -- handles:
 *   the code for every command eggdrop adds to Tcl
 *   Tcl initialization
 *   getting and setting Tcl/eggdrop variables
 * 
 * dprintf'ized, 4feb1996
 * 
 * $Id: tcl.c,v 1.27 2000/01/17 16:14:45 per Exp $
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

#ifdef G_USETCL

/* used for read/write to internal strings */
typedef struct {
  char *str;			/* pointer to actual string in eggdrop */
  int max;			/* max length (negative: read-only var when protect is on) */
  /*   (0: read-only ALWAYS) */
  int flags;			/* 1 = directory */
} strinfo;

typedef struct {
  int *var;
  int ro;
} intinfo;

char whois_fields[121] = "";	/* fields to display in a .whois */
Tcl_Interp *interp;		/* eggdrop always uses the same interpreter */

extern int backgrd,
  flood_telnet_thr,
  flood_telnet_time;
extern int shtime,
  share_greet,
  keep_all_logs;
extern int default_flags,
  conmask,
  switch_logfiles_at,
  connect_timeout;
extern int firewallport,
  reserved_port,
  notify_users_at;
extern int flood_thr,
  ignore_time;
extern char origbotname[],
  botuser[],
  firewall[],
  helpdir[],
  hostname[],
  myip[],
  tempdir[],
  owner[],
  botnetnick[],
  netpass[];
extern int die_on_sighup,
  die_on_sigterm,
  max_logs,
  max_logsize,
  enable_simul;
extern int debug_output,
  identtimeout,
  protect_telnet;
extern int egg_numver,
  share_unlinks,
  dcc_sanitycheck;
extern int tands,
  resolve_timeout,
  default_uflags,
  strict_host;
extern char egg_version[];
extern tcl_timer_t *timer,
 *utimer;
extern time_t online_since;

/* confvar patch by aaronwl */
#endif
extern int dcc_total;
extern struct dcc_t *dcc;

int protect_readonly = 0;	/* turn on/off readonly protection */
int dcc_flood_thr = 3;
int debug_tcl = 0;
int use_silence = 0;
int use_invites = 0;		/* Jason/drummer */
int use_exempts = 0;		/* Jason/drummer */
int force_expire = 0;		/* Rufus */
int remote_boots = 2;
int allow_dk_cmds = 1;
int must_be_owner = 1;

#ifdef HUB
int max_dcc = 200;		/* needs at least 4 or 5 just to get started

				 * 20 should be enough */
#else
int max_dcc = 10;
#endif
int min_dcc_port = 1024;	/* dcc-portrange, min port - dw/guppy */
int max_dcc_port = 65535;	/* dcc-portrange, max port - dw/guppy */
int quick_logs = 0;		/* quick write logs?

				 * flush em every min instead of every 5 */
int par_telnet_flood = 1;	/* trigger telnet flood for +f ppl? - dw */
int quiet_save = 0;		/* quiet-save patch by Lucas */

#ifdef G_USETCL

/* prototypes for tcl */
Tcl_Interp *Tcl_CreateInterp();
#endif

int strtot = 0;

int expmem_tcl()
{
  return strtot;
}

/***********************************************************************/

int findidx(int z)
{
  int j;

  for (j = 0; j < dcc_total; j++)
    if ((dcc[j].sock == z) && (dcc[j].type->flags & DCT_VALIDIDX))
      return j;
  return -1;
}

/**********************************************************************/

int init_dcc_max(),
  init_misc();

#ifdef G_USETCL

/* used for read/write to integer couplets */
typedef struct {
  int *left;			/* left side of couplet */
  int *right;			/* right side */
} coupletinfo;

/* read/write integer couplets (int1:int2) */
char *tcl_eggcouplet(ClientData cdata, Tcl_Interp * irp, char *name1, char *name2, int flags)
{
  char *s,
    s1[41];
  coupletinfo *cp = (coupletinfo *) cdata;

  if (flags & (TCL_TRACE_READS | TCL_TRACE_UNSETS)) {
    sprintf(s1, STR("%d:%d"), *(cp->left), *(cp->right));
    Tcl_SetVar2(interp, name1, name2, s1, TCL_GLOBAL_ONLY);
    if (flags & TCL_TRACE_UNSETS)
      Tcl_TraceVar(interp, name1, TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS, tcl_eggcouplet, cdata);
  } else {			/* writes */
    s = Tcl_GetVar2(interp, name1, name2, TCL_GLOBAL_ONLY);
    if (s != NULL) {
      int nr1,
        nr2;

      if (strlen(s) > 40)
	s[40] = 0;
      sscanf(s, STR("%d%*c%d"), &nr1, &nr2);
      *(cp->left) = nr1;
      *(cp->right) = nr2;
    }
  }
  return NULL;
}

/* read/write normal integer */
char *tcl_eggint(ClientData cdata, Tcl_Interp * irp, char *name1, char *name2, int flags)
{
  char *s,
    s1[40];
  long l;
  intinfo *ii = (intinfo *) cdata;

  if (flags & (TCL_TRACE_READS | TCL_TRACE_UNSETS)) {
    /* special cases */
    if ((int *) ii->var == &conmask)
      strcpy(s1, masktype(conmask));
    else if ((int *) ii->var == &default_flags) {
      struct flag_record fr = { FR_GLOBAL, 0, 0, 0, 0 };
      fr.global = default_flags;

      fr.udef_global = default_uflags;
      build_flags(s1, &fr, 0);
    } else
      sprintf(s1, "%d", *(int *) ii->var);
    Tcl_SetVar2(interp, name1, name2, s1, TCL_GLOBAL_ONLY);
    if (flags & TCL_TRACE_UNSETS)
      Tcl_TraceVar(interp, name1, TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS, tcl_eggint, cdata);
    return NULL;
  } else {			/* writes */
    s = Tcl_GetVar2(interp, name1, name2, TCL_GLOBAL_ONLY);
    if (s != NULL) {
      if ((int *) ii->var == &conmask) {
	if (s[0])
	  conmask = logmodes(s);
	else
	  conmask = LOG_MODES | LOG_MISC | LOG_CMDS;
      } else if ((int *) ii->var == &default_flags) {
	struct flag_record fr = { FR_GLOBAL, 0, 0, 0, 0 };

	break_down_flags(s, &fr, 0);
	default_flags = sanity_check(fr.global);	/* drummer */

	default_uflags = fr.udef_global;
      } else if ((ii->ro == 2) || ((ii->ro == 1) && protect_readonly)) {
	return STR("read-only variable");
      } else {
	if (Tcl_ExprLong(interp, s, &l) == TCL_ERROR)
	  return interp->result;
	if ((int *) ii->var == &max_dcc) {
	  if (l < max_dcc)
	    return STR("you can't DECREASE max-dcc");
	  max_dcc = l;
	  init_dcc_max();
	} else if ((int *) ii->var == &max_logs) {
	  if (l < max_logs)
	    return STR("you can't DECREASE max-logs");
	  max_logs = l;
	  init_misc();
	} else
	  *(ii->var) = (int) l;
      }
    }
    return NULL;
  }
}

/* read/write normal string variable */
char *tcl_eggstr(ClientData cdata, Tcl_Interp * irp, char *name1, char *name2, int flags)
{
  char *s;
  strinfo *st = (strinfo *) cdata;

  if (flags & (TCL_TRACE_READS | TCL_TRACE_UNSETS)) {
    if ((st->str == firewall) && (firewall[0])) {
      char s1[161];

      sprintf(s1, STR("%s:%d"), firewall, firewallport);
      Tcl_SetVar2(interp, name1, name2, s1, TCL_GLOBAL_ONLY);
    } else
      Tcl_SetVar2(interp, name1, name2, st->str, TCL_GLOBAL_ONLY);
    if (flags & TCL_TRACE_UNSETS) {
      Tcl_TraceVar(interp, name1, TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS, tcl_eggstr, cdata);
      if ((st->max <= 0) && (protect_readonly || (st->max == 0)))
	return STR("read-only variable");	/* it won't return the error... */
    }
    return NULL;
  } else {			/* writes */
    if ((st->max <= 0) && (protect_readonly || (st->max == 0))) {
      Tcl_SetVar2(interp, name1, name2, st->str, TCL_GLOBAL_ONLY);
      return STR("read-only variable");
    }
    s = Tcl_GetVar2(interp, name1, name2, TCL_GLOBAL_ONLY);
    if (s != NULL) {
      if (strlen(s) > abs(st->max))
	s[abs(st->max)] = 0;
      if (st->str == botnetnick);
      else if (st->str == firewall) {
	splitc(firewall, s, ':');
	if (!firewall[0])
	  strcpy(firewall, s);
	else
	  firewallport = atoi(s);
      } else
	strcpy(st->str, s);
      if ((st->flags) && (s[0])) {
	if (st->str[strlen(st->str) - 1] != '/')
	  strcat(st->str, "/");
      }
    }
    return NULL;
  }
}

/* add/remove tcl commands */
void add_tcl_commands(tcl_cmds * tab)
{
  int i;

  for (i = 0; tab[i].name; i++)
    Tcl_CreateCommand(interp, tab[i].name, tab[i].func, NULL, NULL);
}

void rem_tcl_commands(tcl_cmds * tab)
{
  int i;

  for (i = 0; tab[i].name; i++)
    Tcl_DeleteCommand(interp, tab[i].name);
}

tcl_strings def_tcl_strings[] = {
  {"botnet-nick", botnetnick, HANDLEN, 0}
  ,
  {"userfile", userfile, 120, STR_PROTECT}
  ,
  {"admin", admin, 120, 0}
  ,
  {"help-path", helpdir, 120, STR_DIR | STR_PROTECT}
  ,
  {"temp-path", tempdir, 120, STR_DIR | STR_PROTECT}
  ,
  {"owner", owner, 120, STR_PROTECT}
  ,
  {"my-hostname", hostname, 120, 0}
  ,
  {"my-ip", myip, 120, 0}
  ,
  {"network", network, 40, 0}
  ,
  {"whois-fields", whois_fields, 120, 0}
  ,
  {"nat-ip", natip, 120, 0}
  ,
  {"username", botuser, 10, 0}
  ,
  {"version", egg_version, 0, 0}
  ,
  {"firewall", firewall, 120, 0}
  ,

/* confvar patch by aaronwl */
  {"netpass", netpass, 100, 0}
  ,
  {0, 0, 0, 0}
};

  /* ints */

tcl_ints def_tcl_ints[] = {
  {"ignore-time", &ignore_time, 0}
  ,
  {"dcc-flood-thr", &dcc_flood_thr, 0}
  ,
  {"hourly-updates", &notify_users_at, 0}
  ,
  {"switch-logfiles-at", &switch_logfiles_at, 0}
  ,
  {"connect-timeout", &connect_timeout, 0}
  ,
  {"reserved-port", &reserved_port, 0}
  ,
  /* booleans (really just ints) */
  {"keep-all-logs", &keep_all_logs, 0}
  ,
  {"uptime", (int *) &online_since, 2},
  {"console", &conmask, 0},
  {"default-flags", &default_flags, 0},
  /* moved from eggdrop.h */
  {"numversion", &egg_numver, 2},
  {"debug-tcl", &debug_tcl, 1},
  {"die-on-sighup", &die_on_sighup, 1},
  {"die-on-sigterm", &die_on_sigterm, 1},
  {"remote-boots", &remote_boots, 1},
  {"max-dcc", &max_dcc, 0},
  {"max-logs", &max_logs, 0},
  {"max-logsize", &max_logsize, 0},
  {"quick-logs", &quick_logs, 0},
  {"enable-simul", &enable_simul, 1},
  {"debug-output", &debug_output, 1},
  {"protect-telnet", &protect_telnet, 0},
  {"dcc-sanitycheck", &dcc_sanitycheck, 0},
  {"dolookups", &dolookups, 0},
  {"sort-users", &sort_users, 0},
  {"ident-timeout", &identtimeout, 0},
  {"share-unlinks", &share_unlinks, 0},
  {"log-time", &shtime, 0},
  {"allow-dk-cmds", &allow_dk_cmds, 0},
  {"resolve-timeout", &resolve_timeout, 0},
  {"must-be-owner", &must_be_owner, 1},
  {"use-silence", &use_silence, 0},	/* arthur2 */
  {"paranoid-telnet-flood", &par_telnet_flood, 0},
  {"use-exempts", &use_exempts, 0},	/* Jason/drummer */
  {"use-invites", &use_invites, 0},	/* Jason/drummer */
  {"quiet-save", &quiet_save, 0},	/* Lucas */
  {"force-expire", &force_expire, 0},	/* Rufus */
  {"strict-host", &strict_host, 0},	/* moved from server.mod & irc.mod */
  {0, 0, 0}			/* arthur2 */
};

tcl_coups def_tcl_coups[] = {
  {"telnet-flood", &flood_telnet_thr, &flood_telnet_time},
  {"dcc-portrange", &min_dcc_port, &max_dcc_port},	/* dw */
  {0, 0, 0}
};

/* set up Tcl variables that will hook into eggdrop internal vars via */

/* trace callbacks */
void init_traces()
{
  add_tcl_coups(def_tcl_coups);
  add_tcl_strings(def_tcl_strings);
  add_tcl_ints(def_tcl_ints);
}

void kill_tcl()
{
  Context;
  rem_tcl_coups(def_tcl_coups);
  rem_tcl_strings(def_tcl_strings);
  rem_tcl_ints(def_tcl_ints);
  kill_bind();
  Tcl_DeleteInterp(interp);
}

extern tcl_cmds tcluser_cmds[],
  tcldcc_cmds[],
  tclmisc_cmds[];

/* not going through Tcl's crazy main() system (what on earth was he
 * smoking?!) so we gotta initialize the Tcl interpreter */
void init_tcl(int argc, char **argv)
{
#ifndef HAVE_PRE7_5_TCL
  int i;
  char pver[1024] = "";
#endif

  Context;
#ifndef HAVE_PRE7_5_TCL
  /* This is used for 'info nameofexecutable'.
   * The filename in argv[0] must exist in a directory listed in
   * the environment variable PATH for it to register anything. */
  Tcl_FindExecutable(argv[0]);
#endif

  /* initialize the interpreter */
  interp = Tcl_CreateInterp();
  Tcl_Init(interp);

#ifdef DEBUG_MEM
  /* initialize Tcl's memory debugging if we have it */
  Tcl_InitMemory(interp);
#endif

  /* set Tcl variable tcl_interactive to 0 */
  Tcl_SetVar(interp, STR("tcl_interactive"), "0", TCL_GLOBAL_ONLY);

  /* initialize binds and traces */
  init_bind();
  init_traces();

  /* isnt this much neater :) */
  add_tcl_commands(tcluser_cmds);
  add_tcl_commands(tcldcc_cmds);
  add_tcl_commands(tclmisc_cmds);

#ifndef HAVE_PRE7_5_TCL
  /* add eggdrop to Tcl's package list */
  for (i = 0; i <= strlen(egg_version); i++) {
    if ((egg_version[i] == ' ') || (egg_version[i] == '+'))
      break;
    pver[strlen(pver)] = egg_version[i];
  }
  Tcl_PkgProvide(interp, STR("eggdrop"), pver);
#endif
}

/**********************************************************************/

void do_tcl(char *whatzit, char *script)
{
  int code;
  FILE *f = 0;

  if (debug_tcl) {
    f = fopen(STR("DEBUG.TCL"), "a");
    if (f != NULL)
      fprintf(f, STR("eval: %s\n"), script);
  }
  Context;
  code = Tcl_Eval(interp, script);
  if (debug_tcl && (f != NULL)) {
    fprintf(f, STR("done eval, result=%d\n"), code);
    fclose(f);
  }
  if (code != TCL_OK) {
    log(LCAT_ERROR, STR("Tcl error in script for '%s':"), whatzit);
    log(LCAT_ERROR, "%s", interp->result);
  }
}

void add_tcl_strings(tcl_strings * list)
{
  int i,
    tmp;
  strinfo *st;

  for (i = 0; list[i].name; i++) {
    st = (strinfo *) nmalloc(sizeof(strinfo));
    strtot += sizeof(strinfo);
    st->max = list[i].length - (list[i].flags & STR_DIR);
    if (list[i].flags & STR_PROTECT)
      st->max = -st->max;
    st->str = list[i].buf;
    st->flags = (list[i].flags & STR_DIR);
    tmp = protect_readonly;
    protect_readonly = 0;
    tcl_eggstr((ClientData) st, interp, list[i].name, NULL, TCL_TRACE_WRITES);
    protect_readonly = tmp;
    tcl_eggstr((ClientData) st, interp, list[i].name, NULL, TCL_TRACE_READS);
    Tcl_TraceVar(interp, list[i].name, TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS, tcl_eggstr, (ClientData) st);
  }
}

void rem_tcl_strings(tcl_strings * list)
{
  int i;
  strinfo *st;

  for (i = 0; list[i].name; i++) {
    st = (strinfo *) Tcl_VarTraceInfo(interp, list[i].name, TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS, tcl_eggstr, NULL);
    Tcl_UntraceVar(interp, list[i].name, TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS, tcl_eggstr, st);
    if (st != NULL) {
      strtot -= sizeof(strinfo);
      nfree(st);
    }
  }
}

void add_tcl_ints(tcl_ints * list)
{
  int i,
    tmp;
  intinfo *ii;

  for (i = 0; list[i].name; i++) {
    ii = nmalloc(sizeof(intinfo));
    strtot += sizeof(intinfo);
    ii->var = list[i].val;
    ii->ro = list[i].readonly;
    tmp = protect_readonly;
    protect_readonly = 0;
    tcl_eggint((ClientData) ii, interp, list[i].name, NULL, TCL_TRACE_WRITES);
    protect_readonly = tmp;
    tcl_eggint((ClientData) ii, interp, list[i].name, NULL, TCL_TRACE_READS);
    Tcl_TraceVar(interp, list[i].name, TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS, tcl_eggint, (ClientData) ii);
  }

}

void rem_tcl_ints(tcl_ints * list)
{
  int i;
  intinfo *ii;

  for (i = 0; list[i].name; i++) {
    ii = (intinfo *) Tcl_VarTraceInfo(interp, list[i].name, TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS, tcl_eggint, NULL);
    Tcl_UntraceVar(interp, list[i].name, TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS, tcl_eggint, (ClientData) ii);
    if (ii) {
      strtot -= sizeof(intinfo);
      nfree(ii);
    }
  }
}

/* allocate couplet space for tracing couplets */
void add_tcl_coups(tcl_coups * list)
{
  coupletinfo *cp;
  int i;

  for (i = 0; list[i].name; i++) {
    cp = (coupletinfo *) nmalloc(sizeof(coupletinfo));
    strtot += sizeof(coupletinfo);
    cp->left = list[i].lptr;
    cp->right = list[i].rptr;

    tcl_eggcouplet((ClientData) cp, interp, list[i].name, NULL, TCL_TRACE_WRITES);
    tcl_eggcouplet((ClientData) cp, interp, list[i].name, NULL, TCL_TRACE_READS);
    Tcl_TraceVar(interp, list[i].name, TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS, tcl_eggcouplet, (ClientData) cp);
  }
}

void rem_tcl_coups(tcl_coups * list)
{
  coupletinfo *cp;
  int i;

  for (i = 0; list[i].name; i++) {
    cp = (coupletinfo *) Tcl_VarTraceInfo(interp, list[i].name, TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS, tcl_eggcouplet, NULL);
    strtot -= sizeof(coupletinfo);
    Tcl_UntraceVar(interp, list[i].name, TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS, tcl_eggcouplet, (ClientData) cp);
    nfree(cp);
  }
}

#endif
