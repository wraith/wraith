/*
 * tclegg.h
 *   stuff used by tcl.c and tclhash.c
 *
 */

#ifndef _EGG_TCLEGG_H
#define _EGG_TCLEGG_H

#include "lush.h"		/* Include this here, since it's needed
				   in this file */
#ifndef MAKING_MODS
#  include "proto.h"		/* This file needs this */
#endif

/* Used for stub functions:
 */

#define STDVAR		(cd, irp, argc, argv)				\
	ClientData cd;							\
	Tcl_Interp *irp;						\
	int argc;							\
	char *argv[];
#define BADARGS(nl, nh, example)	do {				\
	if ((argc < (nl)) || (argc > (nh))) {				\
		Tcl_AppendResult(irp, "wrong # args: should be \"",	\
				 argv[0], (example), "\"", NULL);	\
		return TCL_ERROR;					\
	}								\
} while (0)


typedef struct _tcl_strings {
  char *name;
  char *buf;
  int length;
  int flags;
} tcl_strings;

typedef struct _tcl_int {
  char *name;
  int *val;
  int readonly;
} tcl_ints;

typedef struct _tcl_coups {
  char *name;
  int *lptr;
  int *rptr;
} tcl_coups;

typedef struct _tcl_cmds {
  char *name;
  Function func;
} tcl_cmds;

void add_tcl_commands(tcl_cmds *);
void rem_tcl_commands(tcl_cmds *);
void add_tcl_strings(tcl_strings *);
void rem_tcl_strings(tcl_strings *);
void add_tcl_coups(tcl_coups *);
void rem_tcl_coups(tcl_coups *);
void add_tcl_ints(tcl_ints *);
void rem_tcl_ints(tcl_ints *);

#endif				/* _EGG_TCLEGG_H */
