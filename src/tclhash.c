
/* 
 * tclhash.c -- handles:
 *   bind and unbind
 *   checking and triggering the various in-bot bindings
 *   listing current bindings
 *   adding/removing new binding tables
 *   (non-Tcl) procedure lookups for msg/dcc/file commands
 *   (Tcl) binding internal procedures to msg/dcc/file commands
 * 
 * Now includes FREE OF CHARGE everything from hash.c, 'cause they
 * were exporting functions to each other and only for each other.
 * dprintf'ized, 15nov1995 (hash.c)
 * dprintf'ized, 4feb1996 (tclhash.c)
 * 
 * $Id: tclhash.c,v 1.14 2000/01/17 16:14:45 per Exp $
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
#include "chan.h"
#include "users.h"
#include "match.c"

#ifdef G_USETCL
extern Tcl_Interp *interp;
#endif
extern struct dcc_t *dcc;
extern struct userrec *userlist;
extern int debug_tcl,
  dcc_total;
extern time_t now;

#ifndef G_USETCL
char bindparms[4096];
int bindalloc = 0;
#endif

p_tcl_bind_list bind_table_list;
p_tcl_bind_list H_chat,
  H_act,
  H_bcst,
  H_chon,
  H_chof;
p_tcl_bind_list H_load,
  H_unld,
  H_link,
  H_disc,
  H_dcc,
  H_chjn,
  H_chpt;
p_tcl_bind_list H_bot,
  H_time,
  H_nkch,
  H_away,
  H_note,
  H_filt,
  H_event;

int builtin_2char();
int builtin_3char();
int builtin_5int();
int builtin_char();
int builtin_chpt();
int builtin_chjn();
int builtin_idxchar();
int builtin_charidx();
int builtin_chat();
int builtin_dcc();

int expmem_tclhash()
{
  struct tcl_bind_list *p = bind_table_list;
  struct tcl_bind_mask *q;
  tcl_cmd_t *c;
  int tot = 0;

#ifndef G_USETCL
  tot = bindalloc;
#endif

  while (p) {
    tot += sizeof(struct tcl_bind_list);

    for (q = p->first; q; q = q->next) {
      tot += sizeof(struct tcl_bind_mask);

      tot += strlen(q->mask) + 1;
      for (c = q->first; c; c = c->next) {
	tot += sizeof(tcl_cmd_t);
	tot += strlen(c->func_name) + 1;
      }
    }
    p = p->next;
  }
  return tot;
}

extern cmd_t C_dcc[];

#ifdef G_USETCL
int tcl_bind();
#endif

void init_bind()
{
  bind_table_list = NULL;
  Context;
#ifdef G_USETCL
  Tcl_CreateCommand(interp, STR("bind"), tcl_bind, (ClientData) 0, NULL);
  Tcl_CreateCommand(interp, STR("unbind"), tcl_bind, (ClientData) 1, NULL);
#endif
  H_unld = add_bind_table(STR("unld"), HT_STACKABLE, builtin_char);
  H_time = add_bind_table(STR("time"), HT_STACKABLE, builtin_5int);
  H_note = add_bind_table(STR("note"), 0, builtin_3char);
  H_nkch = add_bind_table(STR("nkch"), HT_STACKABLE, builtin_2char);
  H_load = add_bind_table(STR("load"), HT_STACKABLE, builtin_char);
  H_link = add_bind_table(STR("link"), HT_STACKABLE, builtin_2char);
  H_filt = add_bind_table(STR("filt"), HT_STACKABLE, builtin_idxchar);
  H_disc = add_bind_table(STR("disc"), HT_STACKABLE, builtin_char);
  H_dcc = add_bind_table(STR("dcc"), 0, builtin_dcc);
  H_chpt = add_bind_table(STR("chpt"), HT_STACKABLE, builtin_chpt);
  H_chon = add_bind_table(STR("chon"), HT_STACKABLE, builtin_charidx);
  H_chof = add_bind_table(STR("chof"), HT_STACKABLE, builtin_charidx);
  H_chjn = add_bind_table(STR("chjn"), HT_STACKABLE, builtin_chjn);
  H_chat = add_bind_table(STR("chat"), HT_STACKABLE, builtin_chat);
  H_bot = add_bind_table(STR("bot"), 0, builtin_3char);
  H_bcst = add_bind_table(STR("bcst"), HT_STACKABLE, builtin_chat);
  H_away = add_bind_table(STR("away"), HT_STACKABLE, builtin_chat);
  H_act = add_bind_table(STR("act"), HT_STACKABLE, builtin_chat);
  Context;
  H_event = add_bind_table(STR("evnt"), HT_STACKABLE, builtin_char);
  add_builtins(H_dcc, C_dcc);
}

void kill_bind()
{
  rem_builtins(H_dcc, C_dcc);
  while (bind_table_list) {
    del_bind_table(bind_table_list);
  }
}

p_tcl_bind_list add_bind_table(char *nme, int flg, Function f)
{
  p_tcl_bind_list p = bind_table_list,
    o = NULL;

  /* Do not allow coders to use bind table names longer than
   * 4 characters.
   */
  Assert(strlen(nme) <= 4);

  while (p) {
    int v = strcasecmp(p->name, nme);

    if (v == 0)
      /* repeat, just return old value */
      return p;
    /* insert at start of list */
    if (v > 0) {
      break;
    } else {
      o = p;
      p = p->next;
    }
  }
  p = nmalloc(sizeof(struct tcl_bind_list));

  p->first = NULL;
  strcpy(p->name, nme);
  p->flags = flg;
  p->func = f;
  if (o) {
    p->next = o->next;
    o->next = p;
  } else {
    p->next = bind_table_list;
    bind_table_list = p;
  }
  return p;
}

void del_bind_table(p_tcl_bind_list which)
{
  p_tcl_bind_list p = bind_table_list,
    o = NULL;

  while (p) {
    if (p == which) {
      tcl_cmd_t *tt,
       *tt1;
      struct tcl_bind_mask *ht,
       *ht1;

      if (o) {
	o->next = p->next;
      } else {
	bind_table_list = p->next;
      }
      /* cleanup code goes here */
      for (ht = p->first; ht; ht = ht1) {
	ht1 = ht->next;
	for (tt = ht->first; tt; tt = tt1) {
	  tt1 = tt->next;
	  nfree(tt->func_name);
	  nfree(tt);
	}
	nfree(ht->mask);
	nfree(ht);
      }
      nfree(p);
      return;
    }
    o = p;
    p = p->next;
  }
  log(LCAT_DEBUG, STR("??? Tried to delete no listed bind table ???"));
}

p_tcl_bind_list find_bind_table(char *nme)
{
  p_tcl_bind_list p = bind_table_list;

  while (p) {
    int v = strcasecmp(p->name, nme);

    if (v == 0)
      return p;
    if (v > 0)
      return NULL;
    p = p->next;
  }
  return NULL;
}

#ifdef G_USETCL
void dump_bind_tables(Tcl_Interp * irp)
{
  p_tcl_bind_list p = bind_table_list;
  int i = 0;

  while (p) {
    if (i)
      Tcl_AppendResult(irp, ", ", NULL);
    else
      i++;
    Tcl_AppendResult(irp, p->name, NULL);
    p = p->next;
  }
}
#endif

int unbind_bind_entry(p_tcl_bind_list typ, char *flags, char *cmd, char *proc)
{
  tcl_cmd_t *tt,
   *last;
  struct tcl_bind_mask *ma,
   *ma1 = NULL;

  for (ma = typ->first; ma; ma = ma->next) {
    int i = strcmp(cmd, ma->mask);

    if (!i)
      break;			/* found it! fall out! */
    ma1 = ma;
  }
  if (ma) {
    last = NULL;
    for (tt = ma->first; tt; tt = tt->next) {
      /* if procs are same, erase regardless of flags */
      if (!strcasecmp(tt->func_name, proc)) {
	/* erase it */
	if (last) {
	  last->next = tt->next;
	} else if (tt->next) {
	  ma->first = tt->next;
	} else {
	  if (ma1)
	    ma1->next = ma->next;
	  else
	    typ->first = ma->next;
	  nfree(ma->mask);
	  nfree(ma);
	}
	nfree(tt->func_name);
	nfree(tt);
	return 1;
      }
      last = tt;
    }
  }
  return 0;			/* no match */
}

/* add command (remove old one if necessary) */
#ifdef G_USETCL
int bind_bind_entry(p_tcl_bind_list typ, char *flags, char *cmd, char *proc)
#else
int bind_bind_entry(p_tcl_bind_list typ, char *flags, char *cmd, char *proc, void *func)
#endif
{
  tcl_cmd_t *tt;
  struct tcl_bind_mask *ma,
   *ma1 = NULL;

  Context;
  for (ma = typ->first; ma; ma = ma->next) {
    int i = strcmp(cmd, ma->mask);

    if (!i)
      break;			/* found it! fall out! */
    ma1 = ma;
  }
  Context;
  if (!ma) {
    ma = nmalloc(sizeof(struct tcl_bind_mask));

    ma->mask = nmalloc(strlen(cmd) + 1);
    strcpy(ma->mask, cmd);
    ma->first = NULL;
    ma->next = typ->first;
    typ->first = ma;
  }
  Context;
  for (tt = ma->first; tt; tt = tt->next) {
    /* already defined? if so replace */
    if (!strcasecmp(tt->func_name, proc)) {
      tt->flags.match = FR_GLOBAL | FR_CHAN;
      break_down_flags(flags, &(tt->flags), NULL);
      return 1;
    }
  }
  Context;
  if (!(typ->flags & HT_STACKABLE) && ma->first) {
    nfree(ma->first->func_name);
    nfree(ma->first);
    ma->first = NULL;
  }
  Context;
  tt = nmalloc(sizeof(tcl_cmd_t));
  tt->func_name = nmalloc(strlen(proc) + 1);
#ifndef G_USETCL
  tt->func = func;
#endif
  tt->next = NULL;
  tt->hits = 0;
  tt->flags.match = FR_GLOBAL | FR_CHAN;
  break_down_flags(flags, &(tt->flags), NULL);
  strcpy(tt->func_name, proc);
  tt->next = ma->first;
  ma->first = tt;
  Context;
  return 1;
}

#ifdef G_USETCL
int tcl_getbinds(p_tcl_bind_list kind, char *name)
{
  tcl_cmd_t *tt;
  struct tcl_bind_mask *be;
  char str[400],
    fstr[400];

  if (!strcmp(name, "*")) {
    for (be = kind->first; be; be = be->next) {
      for (tt = be->first; tt; tt = tt->next) {
	build_flags(fstr, &(tt->flags), NULL);
	sprintf(str, STR("%s %s %s"), be->mask, fstr, tt->func_name);
	Tcl_AppendElement(interp, str);
      }
    }
    return TCL_OK;
  }
  for (be = kind->first; be; be = be->next) {
    if (!strcasecmp(be->mask, name)) {
      for (tt = be->first; tt; tt = tt->next)
	Tcl_AppendElement(interp, tt->func_name);
      return TCL_OK;
    }
  }
  return TCL_OK;
}

int tcl_bind STDVAR { p_tcl_bind_list tp;

  if ((long int) cd == 1) {
    BADARGS(5, 5, STR(" type flags cmd/mask procname"))
  } else {
    BADARGS(4, 5, STR(" type flags cmd/mask ?procname?"))
  }
  tp = find_bind_table(argv[1]);
  if (!tp) {
    Tcl_AppendResult(irp, STR("bad type, should be one of: "), NULL);
    dump_bind_tables(irp);
    return TCL_ERROR;
  }
  if ((long int) cd == 1) {
    if (!unbind_bind_entry(tp, argv[2], argv[3], argv[4])) {
      /* don't error if trying to re-unbind a builtin */
      if ((strcmp(argv[3], &argv[4][5]) != 0) || (argv[4][0] != '*') || (strncmp(argv[1], &argv[4][1], 3) != 0) || (argv[4][4] != ':')) {
	Tcl_AppendResult(irp, STR("no such binding"), NULL);
	return TCL_ERROR;
      }
    }
  } else {
    if (argc == 4)
      return tcl_getbinds(tp, argv[3]);
    bind_bind_entry(tp, argv[2], argv[3], argv[4]);
  }
  Tcl_AppendResult(irp, argv[3], NULL);
  return TCL_OK;
}
#endif

int check_validity(char *nme, Function f)
{
  char *p;
  p_tcl_bind_list t;

  if (*nme != '*')
    return 0;
  if (!(p = strchr(nme + 1, ':')))
    return 0;
  *p = 0;
  t = find_bind_table(nme + 1);
  *p = ':';
  if (!t)
    return 0;
  if (t->func != f)
    return 0;
  return 1;
}

int findanyidx(int z)
{
  int j;

  for (j = 0; j < dcc_total; j++)
    if (dcc[j].sock == z)
      return j;
  return -1;
}

#ifdef G_USETCL
int builtin_3char STDVAR { Function F = (Function) cd;

    BADARGS(4, 4, STR(" from to args"));
    CHECKVALIDITY(builtin_3char);
    F(argv[1], argv[2], argv[3]);
    return TCL_OK;
} int builtin_2char STDVAR { Function F = (Function) cd;

    BADARGS(3, 3, STR(" nick msg"));
    CHECKVALIDITY(builtin_2char);
    F(argv[1], argv[2]);
    return TCL_OK;
} int builtin_5int STDVAR { Function F = (Function) cd;

    BADARGS(6, 6, STR(" min hrs dom mon year"));
    CHECKVALIDITY(builtin_5int);

    F(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
    return TCL_OK;
} int builtin_char STDVAR { Function F = (Function) cd;

    BADARGS(2, 2, STR(" handle"));
    CHECKVALIDITY(builtin_char);
    F(argv[1]);
    return TCL_OK;
} int builtin_chpt STDVAR { Function F = (Function) cd;

    BADARGS(3, 3, STR(" bot nick sock"));
    CHECKVALIDITY(builtin_chpt);
    F(argv[1], argv[2], atoi(argv[3]));
    return TCL_OK;
} int builtin_chjn STDVAR { Function F = (Function) cd;

    BADARGS(6, 6, STR(" bot nick chan# flag&sock host"));
    CHECKVALIDITY(builtin_chjn);
    F(argv[1], argv[2], atoi(argv[3]), argv[4][0], argv[4][0] ? atoi(argv[4] + 1) : 0, argv[5]);
    return TCL_OK;
} int builtin_idxchar STDVAR { Function F = (Function) cd;
  int idx;
  char *r;

    BADARGS(3, 3, STR(" idx args"));
    CHECKVALIDITY(builtin_idxchar);
    idx = findidx(atoi(argv[1]));
  if (idx < 0) {
    Tcl_AppendResult(irp, STR("invalid idx"), NULL);
    return TCL_ERROR;
  }
  r = (((char *(*)()) F) (idx, argv[2]));

  Tcl_ResetResult(irp);
  Tcl_AppendResult(irp, r, NULL);
  return TCL_OK;
}

int builtin_charidx STDVAR { Function F = (Function) cd;
  int idx;

    BADARGS(3, 3, STR(" handle idx"));
    CHECKVALIDITY(builtin_charidx);
    idx = findanyidx(atoi(argv[2]));
  if (idx < 0) {
    Tcl_AppendResult(irp, STR("invalid idx"), NULL);
    return TCL_ERROR;
  }
  Tcl_AppendResult(irp, int_to_base10(F(argv[1], idx)), NULL);

  return TCL_OK;
}

int builtin_chat STDVAR { Function F = (Function) cd;
  int ch;

    BADARGS(4, 4, STR(" handle idx text"));
    CHECKVALIDITY(builtin_chat);
    ch = atoi(argv[2]);
    F(argv[1], ch, argv[3]);
    return TCL_OK;
} 

int builtin_dcc STDVAR { int idx;
  Function F = (Function) cd;

    Context;
    BADARGS(4, 4, STR(" hand idx param"));
    idx = findidx(atoi(argv[2]));
  if (idx < 0) {
    Tcl_AppendResult(irp, STR("invalid idx"), NULL);
    return TCL_ERROR;
  }
  if (F == 0) {
    Tcl_AppendResult(irp, STR("break"), NULL);
    return TCL_OK;
  }
  /* check if it's a password change, if so, don't show the password */
  /* lets clean this up, it's debugging, we dont need pretty formats, just
   * a cover up - and dont even bother with .tcl */
  debug4(STR("tcl: builtin dcc call: %s %s %s %s"), argv[0], argv[1], argv[2], (!strcmp(argv[0] + 5, STR("newpass"))
										|| !strcmp(argv[0] + 5, STR("chpass"))) ? STR("[something]") : argv[3]);
  Context;
  (F) (dcc[idx].user, idx, argv[3]);
  Context;
  Tcl_ResetResult(irp);
  Tcl_AppendResult(irp, "0", NULL);
  Context;
  return TCL_OK;
}

#else

int builtin_3char(void *func, void *dummy, int argc, char *argv[])
{
  Function F = (Function) func;

  return F(argv[1], argv[2], argv[3]);
}

int builtin_2char(void *func, void *dummy, int argc, char *argv[])
{
  Function F = (Function) func;

  return F(argv[1], argv[2]);
}

int builtin_5int(void *func, void *dummy, int argc, char *argv[])
{
  Function F = (Function) func;

  return F(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
}

int builtin_char(void *func, void *dummy, int argc, char *argv[])
{
  Function F = (Function) func;

  return F(argv[1]);
}

int builtin_chpt(void *func, void *dummy, int argc, char *argv[])
{
  Function F = (Function) func;

  return F(argv[1], argv[2], atoi(argv[3]));
}

int builtin_chjn(void *func, void *dummy, int argc, char *argv[])
{
  Function F = (Function) func;

  return F(argv[1], argv[2], atoi(argv[3]), argv[4][0], argv[4][0] ? atoi(argv[4] + 1) : 0, argv[5]);
}

int builtin_idxchar(void *func, void *dummy, int argc, char *argv[])
{
  Function F = (Function) func;
  int idx;

  idx = findidx(atoi(argv[1]));
  if (idx < 0) {
    return 0;
  }
  return F(idx, argv[2]);
}

int builtin_charidx(void *func, void *dummy, int argc, char *argv[])
{
  Function F = (Function) func;
  int idx;

  idx = findanyidx(atoi(argv[2]));
  if (idx < 0) {
    return 0;
  }
  return F(argv[1], idx);
}

int builtin_chat(void *func, void *dummy, int argc, char *argv[])
{
  Function F = (Function) func;
  int ch;

  ch = atoi(argv[2]);
  return F(argv[1], ch, argv[3]);
}

int builtin_dcc(void *func, void *dummy, int argc, char *argv[])
{
  int idx;
  Function F = (Function) func;

  Context;
  idx = findidx(atoi(argv[2]));
  if (idx < 0) {
    return 0;
  }
  if (F == 0) {
    return -1;
  }
  /* check if it's a password change, if so, don't show the password */
  /* lets clean this up, it's debugging, we dont need pretty formats, just
   * a cover up - and dont even bother with .tcl */
  debug4(STR("tcl: builtin dcc call: %s %s %s %s"), argv[0], argv[1], argv[2], (!strcmp(argv[0] + 5, STR("newpass"))
										|| !strcmp(argv[0] + 5, STR("chpass"))) ? STR("[something]") : argv[3]);
  Context;
  F(dcc[idx].user, idx, argv[3]);
  return 0;
}
#endif

#ifdef G_USETCL

/* trigger (execute) a proc */
int trigger_bind(char *proc, char *param)
{
  int x;

  {
    /* 
     * We now try to debug the Tcl_VarEval() call below by remembering both
     * the called proc name and it's parameters. This should render us a bit
     * less helpless when we see context dumps.
     */
    char *buf,
     *msg = STR("TCL proc: %s, param: %s");

    Context;
    buf = nmalloc(strlen(msg) + (proc ? strlen(proc) : 6)
		  + (param ? strlen(param) : 6) + 1);
    sprintf(buf, msg, proc ? proc : STR("<null>"), param ? param : STR("<null>"));
    ContextNote(buf);
    nfree(buf);
  }
  Context;
  x = Tcl_VarEval(interp, proc, param, NULL);
  Context;
  if (x == TCL_ERROR) {
    if (strlen(interp->result) > 400)
      interp->result[400] = 0;
    log(LCAT_ERROR, STR("Tcl error [%s]: %s"), proc, interp->result);
    return BIND_EXECUTED;
  } else {
    if (!strcmp(interp->result, STR("break")))
      return BIND_EXEC_BRK;
    return (atoi(interp->result) > 0) ? BIND_EXEC_LOG : BIND_EXECUTED;
  }
}
#else

int trigger_bind(p_tcl_bind_list bl, tcl_cmd_t * tt, char *param)
{
  char *argv[100];
  int argc,
    i;
  char *paramcpy,
   *psrc,
   *pdst;
  Function F;

  if (param != bindparms) {
    log(LCAT_ERROR, STR("%s: Non-TCL bind params MUST be passed with make_bind_param()"), tt->func_name);
    return BIND_EXEC_BRK;
  }

  paramcpy = nmalloc(4096);
  bindalloc = 4096;
  psrc = param;
  pdst = paramcpy;
  i = 1;
  argv[0] = tt->func_name;
  while (*psrc) {
    strcpy(pdst, psrc);
    argv[i++] = pdst;
    pdst += strlen(pdst) + 1;
    psrc += strlen(psrc) + 1;
  }
  argc = i;
  while (i < 100)
    argv[i++] = "";
  F = bl->func;
  i = F(tt->func, NULL, argc, argv);
  nfree(paramcpy);
  bindalloc = 0;
  if (i > 0)
    return BIND_EXEC_LOG;
  else if (!i)
    return BIND_EXECUTED;
  else
    return BIND_EXEC_BRK;
}
#endif

#ifdef G_USETCL
int check_tcl_bind(p_tcl_bind_list bind, char *match, struct flag_record *atr, char *param, int match_type)
{
  struct tcl_bind_mask *hm,
   *ohm = NULL,
   *hmp = NULL;
  int cnt = 0;
  int ok = 0;
  char *proc = NULL;
  tcl_cmd_t *tt,
   *htt = NULL;
  int f = 0,
    atrok,
    x;

  Context;

  for (hm = bind->first; hm && !f && bind->first; ohm = hm, hm = hm->next) {

    switch (match_type & 0x03) {
    case MATCH_PARTIAL:
      ok = !(strncasecmp(match, hm->mask, strlen(match)));
      break;
    case MATCH_EXACT:
      if (strcasecmp2(match, hm->mask))
	ok = 0;
      else
	ok = 1;
      break;
    case MATCH_CASE:
      ok = !(strcmp(match, hm->mask));
      break;
    case MATCH_MASK:
      ok = wild_match_per((unsigned char *) hm->mask, (unsigned char *) match);
      break;
    }
    if (ok) {
      tt = hm->first;
      if (match_type & BIND_STACKABLE) {
	/* could be multiple triggers */
	while (tt) {
	  if (match_type & BIND_USE_ATTR) {
	    if (match_type & BIND_HAS_BUILTINS)
	      atrok = flagrec_ok(&tt->flags, atr);
	    else
	      atrok = flagrec_eq(&tt->flags, atr);
	  } else
	    atrok = 1;
	  if (atrok) {
	    cnt++;
	    tt->hits++;
	    hmp = ohm;
	    Tcl_SetVar(interp, STR("lastbind"), match, TCL_GLOBAL_ONLY);
	    x = trigger_bind(tt->func_name, param);
	    if ((match_type & BIND_WANTRET) && !(match_type & BIND_ALTER_ARGS) && (x == BIND_EXEC_LOG))
	      return x;
	    if (match_type & BIND_ALTER_ARGS) {
	      if ((interp->result == NULL) || !(interp->result[0]))
		return x;
	      /* this is such an amazingly ugly hack: */
	      Tcl_SetVar(interp, "_a", interp->result, 0);
	    }
	  }
	  tt = tt->next;
	}
	if ((match_type & 3) != MATCH_MASK)
	  f = 1;		/* this will suffice until we have
				 * stackable partials */
      } else {
	if (match_type & BIND_USE_ATTR) {
	  if (match_type & BIND_HAS_BUILTINS)
	    atrok = flagrec_ok(&tt->flags, atr);
	  else
	    atrok = flagrec_eq(&tt->flags, atr);
	} else
	  atrok = 1;
	if (atrok) {
	  cnt++;
	  proc = tt->func_name;
	  htt = tt;
	  hmp = ohm;
	  if (((match_type & 3) != MATCH_PARTIAL) || !strcasecmp(match, hm->mask))
	    cnt = f = 1;
	}
      }
    }
  }
  Context;
  if (cnt == 0)
    return BIND_NOMATCH;
  if (((match_type & 0x03) == MATCH_MASK) || ((match_type & 0x03) == MATCH_CASE))
    return BIND_EXECUTED;
  if ((match_type & 0x3) != MATCH_CASE) {
    if (htt)
      htt->hits++;
    if (hmp) {
      ohm = hmp->next;
      hmp->next = ohm->next;
      ohm->next = bind->first;
      bind->first = ohm;
    }
  }
  if (cnt > 1)
    return BIND_AMBIGUOUS;
  Tcl_SetVar(interp, STR("lastbind"), match, TCL_GLOBAL_ONLY);
  return trigger_bind(proc, param);
}

#else

char *make_bind_param EGG_VARARGS(int, arg1)
/*(int cnt, ...) */
{
  va_list va;
  char *p;
  int cnt;
  bindparms[0] = 0;
  cnt=EGG_VARARGS_START(int, arg1, va);
  p = bindparms;
  while (cnt) {
    sprintf(p, "%s", va_arg(va, char *));

    p += strlen(p) + 1;
    cnt--;
  }
  va_end(va);
  *p = 0;
  return bindparms;
}

int check_tcl_bind(p_tcl_bind_list bind, char *match, struct flag_record *atr, char *param, int match_type)
{
  struct tcl_bind_mask *hm,
   *ohm = NULL,
   *hmp = NULL;
  int cnt = 0;
  int ok = 0;
  char *proc = NULL;
  tcl_cmd_t *tt,
   *htt = NULL;
  int f = 0,
    atrok,
    x;

  Context;

  for (hm = bind->first; hm && !f && bind->first; ohm = hm, hm = hm->next) {

    switch (match_type & 0x03) {
    case MATCH_PARTIAL:
      ok = !(strncasecmp(match, hm->mask, strlen(match)));
      break;
    case MATCH_EXACT:
      if (strcasecmp2(match, hm->mask))
	ok = 0;
      else
	ok = 1;
      break;
    case MATCH_CASE:
      ok = !(strcmp(match, hm->mask));
      break;
    case MATCH_MASK:
      ok = wild_match_per((unsigned char *) hm->mask, (unsigned char *) match);
      break;
    }
    if (ok) {
      tt = hm->first;
      if (match_type & BIND_STACKABLE) {
	/* could be multiple triggers */
	while (tt) {
	  if (match_type & BIND_USE_ATTR) {
	    if (match_type & BIND_HAS_BUILTINS)
	      atrok = flagrec_ok(&tt->flags, atr);
	    else
	      atrok = flagrec_eq(&tt->flags, atr);
	  } else
	    atrok = 1;
	  if (atrok) {
	    char pcpy[sizeof(bindparms)];
	    cnt++;
	    tt->hits++;
	    hmp = ohm;
	    memcpy(&pcpy, &bindparms, sizeof(bindparms));
	    x = trigger_bind(bind, tt, param);
	    memcpy(&bindparms, &pcpy, sizeof(bindparms));
	    if ((match_type & BIND_WANTRET) && !(match_type & BIND_ALTER_ARGS) && (x == BIND_EXEC_LOG))
	      return x;
	    if (match_type & BIND_ALTER_ARGS) {
	      return x;
	    }
	  }
	  tt = tt->next;
	}
	if ((match_type & 3) != MATCH_MASK)
	  f = 1;		/* this will suffice until we have
				 * stackable partials */
      } else {
	if (match_type & BIND_USE_ATTR) {
	  if (match_type & BIND_HAS_BUILTINS)
	    atrok = flagrec_ok(&tt->flags, atr);
	  else
	    atrok = flagrec_eq(&tt->flags, atr);
	} else
	  atrok = 1;
	if (atrok) {
	  cnt++;
	  proc = tt->func_name;
	  htt = tt;
	  hmp = ohm;
	  if (((match_type & 3) != MATCH_PARTIAL) || !strcasecmp(match, hm->mask))
	    cnt = f = 1;
	}
      }
    }
  }
  Context;
  if (cnt == 0)
    return BIND_NOMATCH;
  if (((match_type & 0x03) == MATCH_MASK) || ((match_type & 0x03) == MATCH_CASE))
    return BIND_EXECUTED;
  if ((match_type & 0x3) != MATCH_CASE) {
    if (htt)
      htt->hits++;
    if (hmp) {
      ohm = hmp->next;
      hmp->next = ohm->next;
      ohm->next = bind->first;
      bind->first = ohm;
    }
  }
  if (cnt > 1)
    return BIND_AMBIGUOUS;
  if (htt)
    return trigger_bind(bind, htt, param);
  else
    return BIND_MATCHED;
}
#endif

/* check for tcl-bound dcc command, return 1 if found */

/* dcc: proc-name <handle> <sock> <args...> */
int check_tcl_dcc(char *cmd, int idx, char *args)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };
  int x;
  char s[5];

  Context;
  get_user_flagrec(dcc[idx].user, &fr, NULL);
  sprintf(s, STR("%ld"), dcc[idx].sock);

#ifdef G_DCCPASS
  if (has_cmd_pass(cmd)) {
    char *p,
      work[1024],
      pass[128];

    p = strchr(args, ' ');
    if (p)
      *p = 0;
    strncpy0(pass, args, sizeof(pass));
    if (check_cmd_pass(cmd, pass)) {
      if (p)
	*p = ' ';
      strncpy0(work, args, sizeof(work));
      p = work;
      newsplit(&p);
      strcpy(args, p);
    } else {
      dprintf(idx, STR("Invalid command password. Use .command password arguments\n"));
      log(LCAT_WARNING, STR("%s attempted .%s with missing or incorrect command password"), dcc[idx].nick, cmd);
      return 0;
    }
  }
#endif
#ifdef G_USETCL
  Tcl_SetVar(interp, STR("_dcc1"), dcc[idx].nick, 0);
  Tcl_SetVar(interp, STR("_dcc2"), s, 0);
  Tcl_SetVar(interp, STR("_dcc3"), args, 0);
  Context;
  x = check_tcl_bind(H_dcc, cmd, &fr, STR(" $_dcc1 $_dcc2 $_dcc3"), MATCH_PARTIAL | BIND_USE_ATTR | BIND_HAS_BUILTINS);
#else
  x = check_tcl_bind(H_dcc, cmd, &fr, make_bind_param(3, dcc[idx].nick, s, args), MATCH_PARTIAL | BIND_USE_ATTR | BIND_HAS_BUILTINS);
#endif
  Context;
  if (x == BIND_AMBIGUOUS) {
    dprintf(idx, STR("Ambigous command.\n"));
    return 0;
  }
  if (x == BIND_NOMATCH) {
    dprintf(idx, STR("What?\n"));
    return 0;
  }
  if (x == BIND_EXEC_BRK)
    return 1;			/* quit */
  return 0;
}

void check_tcl_bot(char *nick, char *code, char *param)
{
#ifdef G_USETCL
  Context;
  Tcl_SetVar(interp, STR("_bot1"), nick, 0);
  Tcl_SetVar(interp, STR("_bot2"), code, 0);
  Tcl_SetVar(interp, STR("_bot3"), param, 0);
  check_tcl_bind(H_bot, code, 0, STR(" $_bot1 $_bot2 $_bot3"), MATCH_EXACT);
#else
  check_tcl_bind(H_bot, code, 0, make_bind_param(3, nick, code, param), MATCH_EXACT);
#endif
}

void check_tcl_chonof(char *hand, int sock, p_tcl_bind_list table)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };
  char s[20];
  struct userrec *u = get_user_by_handle(userlist, hand);

  Context;
  touch_laston(u, STR("partyline"), now);
  get_user_flagrec(u, &fr, NULL);
  simple_sprintf(s, "%d", sock);
#ifdef G_USETCL
  Tcl_SetVar(interp, STR("_chonof1"), hand, 0);
  Tcl_SetVar(interp, STR("_chonof2"), s, 0);
  Context;
  check_tcl_bind(table, hand, &fr, STR(" $_chonof1 $_chonof2"), MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE | BIND_WANTRET);
#else
  check_tcl_bind(table, hand, &fr, make_bind_param(2, hand, s), MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE | BIND_WANTRET);
#endif
  Context;
}

void check_tcl_chatactbcst(char *from, int chan, char *text, p_tcl_bind_list ht)
{
  char s[10];

  Context;
  simple_sprintf(s, "%d", chan);
#ifdef G_USETCL
  Tcl_SetVar(interp, STR("_cab1"), from, 0);
  Tcl_SetVar(interp, STR("_cab2"), s, 0);
  Tcl_SetVar(interp, STR("_cab3"), text, 0);
  check_tcl_bind(ht, text, 0, STR(" $_cab1 $_cab2 $_cab3"), MATCH_MASK | BIND_STACKABLE);
#else
  check_tcl_bind(ht, text, 0, make_bind_param(3, from, s, text), MATCH_MASK | BIND_STACKABLE);
#endif
  Context;
}

void check_tcl_nkch(char *ohand, char *nhand)
{
#ifdef G_USETCL
  Context;
  Tcl_SetVar(interp, STR("_nkch1"), ohand, 0);
  Tcl_SetVar(interp, STR("_nkch2"), nhand, 0);
  check_tcl_bind(H_nkch, ohand, 0, STR(" $_nkch1 $_nkch2"), MATCH_MASK | BIND_STACKABLE);
#else
  check_tcl_bind(H_nkch, ohand, 0, make_bind_param(2, ohand, nhand), MATCH_MASK | BIND_STACKABLE);
#endif
  Context;
}

void check_tcl_link(char *bot, char *via)
{
#ifdef G_USETCL
  Context;
  Tcl_SetVar(interp, STR("_link1"), bot, 0);
  Tcl_SetVar(interp, STR("_link2"), via, 0);
  Context;
  check_tcl_bind(H_link, bot, 0, STR(" $_link1 $_link2"), MATCH_MASK | BIND_STACKABLE);
#else
  check_tcl_bind(H_link, bot, 0, make_bind_param(2, bot, via), MATCH_MASK | BIND_STACKABLE);
#endif
  Context;
}

void check_tcl_disc(char *bot)
{
#ifdef G_USETCL
  Context;
  Tcl_SetVar(interp, STR("_disc1"), bot, 0);
  Context;
  check_tcl_bind(H_disc, bot, 0, STR(" $_disc1"), MATCH_MASK | BIND_STACKABLE);
  Context;
#else
  check_tcl_bind(H_disc, bot, 0, make_bind_param(1, bot), MATCH_MASK | BIND_STACKABLE);
#endif
}

void check_tcl_loadunld(char *mod, p_tcl_bind_list table)
{
#ifdef G_USETCL
  Context;
  Tcl_SetVar(interp, STR("_lu1"), mod, 0);
  Context;
  check_tcl_bind(table, mod, 0, STR(" $_lu1"), MATCH_MASK | BIND_STACKABLE);
  Context;
#else
  check_tcl_bind(table, mod, 0, make_bind_param(1, mod), MATCH_MASK | BIND_STACKABLE);
#endif
}

char *check_tcl_filt(int idx, char *text)
{
#ifdef G_USETCL
  char s[10];
  int x;
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0, 0 };

  Context;
  sprintf(s, STR("%ld"), dcc[idx].sock);
  get_user_flagrec(dcc[idx].user, &fr, NULL);
  Tcl_SetVar(interp, STR("_filt1"), s, 0);
  Tcl_SetVar(interp, STR("_filt2"), text, 0);
  Context;
  x = check_tcl_bind(H_filt, text, &fr, STR(" $_filt1 $_filt2"), MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE | BIND_WANTRET | BIND_ALTER_ARGS);
  Context;
  if ((x == BIND_EXECUTED) || (x == BIND_EXEC_LOG)) {
    if ((interp->result == NULL) || (!interp->result[0]))
      return "";
    else
      return interp->result;
  } else
#endif
    return text;
}

#ifdef G_USETCL
void check_tcl_listen(char *cmd, int idx)
{
  char s[10];
  int x;

  Context;
  simple_sprintf(s, "%d", idx);
  Tcl_SetVar(interp, "_n", s, 0);
  Context;
  x = Tcl_VarEval(interp, cmd, STR(" $_n"), NULL);
  Context;
  if (x == TCL_ERROR)
    log(LCAT_ERROR, STR("error on listen: %s"), interp->result);
}
#endif

void check_tcl_chjn(char *bot, char *nick, int chan, char type, int sock, char *host)
{
  struct flag_record fr = { FR_GLOBAL, 0, 0, 0, 0 };
  char s[20],
    t[2],
    u[20];

  Context;
  t[0] = type;
  t[1] = 0;
  switch (type) {
  case '*':
    fr.global = USER_OWNER;

    break;
  case '+':
    fr.global = USER_MASTER;

    break;
  case '@':
    fr.global = USER_OP;

    break;
  }
  simple_sprintf(s, "%d", chan);
  simple_sprintf(u, "%d", sock);
#ifdef G_USETCL
  Tcl_SetVar(interp, STR("_chjn1"), bot, 0);
  Tcl_SetVar(interp, STR("_chjn2"), nick, 0);
  Tcl_SetVar(interp, STR("_chjn3"), s, 0);
  Tcl_SetVar(interp, STR("_chjn4"), t, 0);
  Tcl_SetVar(interp, STR("_chjn5"), u, 0);
  Tcl_SetVar(interp, STR("_chjn6"), host, 0);
  Context;
  check_tcl_bind(H_chjn, s, &fr, STR(" $_chjn1 $_chjn2 $_chjn3 $_chjn4 $_chjn5 $_chjn6"), MATCH_MASK | BIND_STACKABLE);
#else
  check_tcl_bind(H_chjn, s, &fr, make_bind_param(6, bot, nick, s, t, u, host), MATCH_MASK | BIND_STACKABLE);
#endif
  Context;
}

void check_tcl_chpt(char *bot, char *hand, int sock, int chan)
{
  char u[20],
    v[20];

  Context;
  simple_sprintf(u, "%d", sock);
  simple_sprintf(v, "%d", chan);
#ifdef G_USETCL
  Tcl_SetVar(interp, STR("_chpt1"), bot, 0);
  Tcl_SetVar(interp, STR("_chpt2"), hand, 0);
  Tcl_SetVar(interp, STR("_chpt3"), u, 0);
  Tcl_SetVar(interp, STR("_chpt4"), v, 0);
  Context;
  check_tcl_bind(H_chpt, v, 0, STR(" $_chpt1 $_chpt2 $_chpt3 $_chpt4"), MATCH_MASK | BIND_STACKABLE);
#else
  check_tcl_bind(H_chpt, v, 0, make_bind_param(4, bot, hand, u, v), MATCH_MASK | BIND_STACKABLE);
#endif
  Context;
}

void check_tcl_away(char *bot, int idx, char *msg)
{
  char u[20];

  Context;
  simple_sprintf(u, "%d", idx);
#ifdef G_USETCL
  Tcl_SetVar(interp, STR("_away1"), bot, 0);
  Tcl_SetVar(interp, STR("_away2"), u, 0);
  Tcl_SetVar(interp, STR("_away3"), msg ? msg : "", 0);
  Context;
  check_tcl_bind(H_away, bot, 0, STR(" $_away1 $_away2 $_away3"), MATCH_MASK | BIND_STACKABLE);
#else
  check_tcl_bind(H_away, bot, 0, make_bind_param(3, bot, u, msg ? msg : ""), MATCH_MASK | BIND_STACKABLE);
#endif
}

void check_tcl_time(struct tm *tm)
{
#ifdef G_USETCL
  char y[100];

  Context;
  sprintf(y, STR("%02d"), tm->tm_min);
  Tcl_SetVar(interp, STR("_time1"), y, 0);
  sprintf(y, STR("%02d"), tm->tm_hour);
  Tcl_SetVar(interp, STR("_time2"), y, 0);
  sprintf(y, STR("%02d"), tm->tm_mday);
  Tcl_SetVar(interp, STR("_time3"), y, 0);
  sprintf(y, STR("%02d"), tm->tm_mon);
  Tcl_SetVar(interp, STR("_time4"), y, 0);
  sprintf(y, STR("%04d"), tm->tm_year + 1900);
  Tcl_SetVar(interp, STR("_time5"), y, 0);
  sprintf(y, STR("%02d %02d %02d %02d %04d"), tm->tm_min, tm->tm_hour, tm->tm_mday, tm->tm_mon, tm->tm_year + 1900);
  Context;
  check_tcl_bind(H_time, y, 0, STR(" $_time1 $_time2 $_time3 $_time4 $_time5"), MATCH_MASK | BIND_STACKABLE);
  Context;
#else
  char y[100],
    t[5][5];

  sprintf(y, STR("%02d %02d %02d %02d %04d"), tm->tm_min, tm->tm_hour, tm->tm_mday, tm->tm_mon, tm->tm_year + 1900);
  sprintf(t[0], STR("%02d"), tm->tm_min);
  sprintf(t[1], STR("%02d"), tm->tm_hour);
  sprintf(t[2], STR("%02d"), tm->tm_mday);
  sprintf(t[3], STR("%02d"), tm->tm_mon);
  sprintf(t[4], STR("%04d"), tm->tm_year + 1900);

  check_tcl_bind(H_time, y, 0, make_bind_param(5, t[0], t[1], t[2], t[3], t[4]), MATCH_MASK | BIND_STACKABLE);
#endif
}

void check_tcl_event(char *event)
{
#ifdef G_USETCL
  Context;
  Tcl_SetVar(interp, STR("_event1"), event, 0);
  Context;
  check_tcl_bind(H_event, event, 0, STR(" $_event1"), MATCH_EXACT | BIND_STACKABLE);
  Context;
#else
  check_tcl_bind(H_event, event, 0, make_bind_param(1, event), MATCH_EXACT | BIND_STACKABLE);
#endif
}

void tell_binds(int idx, char *name)
{
  struct tcl_bind_mask *hm;
  p_tcl_bind_list p,
    kind;
  int fnd = 0;
  tcl_cmd_t *tt;
  char *s,
   *proc,
    flg[100];
  int showall = 0;

  Context;
  s = strchr(name, ' ');
  if (s) {
    *s = 0;
    s++;
  } else {
    s = name;
  }
  kind = find_bind_table(name);
  if (!strcasecmp(s, STR("all")))
    showall = 1;
  for (p = kind ? kind : bind_table_list; p; p = kind ? 0 : p->next) {
    for (hm = p->first; hm; hm = hm->next) {
      if (!fnd) {
	dprintf(idx, STR("Command bindings:\n"));
	fnd = 1;
	dprintf(idx, STR("  TYPE FLGS     COMMAND              HITS BINDING (TCL)\n"));
      }
      for (tt = hm->first; tt; tt = tt->next) {
	proc = tt->func_name;
	build_flags(flg, &(tt->flags), NULL);
	Context;
	if ((showall) || (proc[0] != '*') || !strchr(proc, ':'))
	  dprintf(idx, STR("  %-4s %-8s %-20s %4d %s\n"), p->name, flg, hm->mask, tt->hits, tt->func_name);
      }
    }
  }
  if (!fnd) {
    if (!kind)
      dprintf(idx, STR("No command bindings.\n"));
    else
      dprintf(idx, STR("No bindings for %s.\n"), name);
  }
}

/* bring the default msg/dcc/fil commands into the Tcl interpreter */
void add_builtins(p_tcl_bind_list table, cmd_t * cc)
{
#ifdef G_USETCL
  int k,
    i;
  char p[1024],
   *l;

  Context;
  for (i = 0; cc[i].name; i++) {
    simple_sprintf(p, STR("*%s:%s"), table->name, cc[i].funcname ? cc[i].funcname : cc[i].name);
    l = (char *) nmalloc(Tcl_ScanElement(p, &k));
    Tcl_ConvertElement(p, l, k | TCL_DONT_USE_BRACES);
    Tcl_CreateCommand(interp, p, table->func, (ClientData) cc[i].func, NULL);
    bind_bind_entry(table, cc[i].flags, cc[i].name, l);
    nfree(l);
    /* create command entry in Tcl interpreter */
  }
#else
  int i;
  char p[1024];

  Context;
  for (i = 0; cc[i].name; i++) {
    simple_sprintf(p, STR("*%s:%s"), table->name, cc[i].funcname ? cc[i].funcname : cc[i].name);
    bind_bind_entry(table, cc[i].flags, cc[i].name, p, cc[i].func);
  }
#endif
}

/* bring the default msg/dcc/fil commands into the Tcl interpreter */
void rem_builtins(p_tcl_bind_list table, cmd_t * cc)
{
#ifdef G_USETCL
  int k,
    i;
  char p[1024],
   *l;

  for (i = 0; cc[i].name; i++) {
    simple_sprintf(p, STR("*%s:%s"), table->name, cc[i].funcname ? cc[i].funcname : cc[i].name);
    l = (char *) nmalloc(Tcl_ScanElement(p, &k));
    Tcl_ConvertElement(p, l, k | TCL_DONT_USE_BRACES);
    Tcl_DeleteCommand(interp, p);
    unbind_bind_entry(table, cc[i].flags, cc[i].name, l);
    nfree(l);
  }
#else
  int i;
  char p[1024];

  for (i = 0; cc[i].name; i++) {
    simple_sprintf(p, STR("*%s:%s"), table->name, cc[i].funcname ? cc[i].funcname : cc[i].name);
    unbind_bind_entry(table, cc[i].flags, cc[i].name, p);
  }
#endif
}
