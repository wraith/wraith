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

#define BADARGS(nl, nh, example)	do {				\
	if ((argc < (nl)) || (argc > (nh))) {				\
		Tcl_AppendResult(irp, "wrong # args: should be \"",	\
				 argv[0], (example), "\"", NULL);	\
		return TCL_ERROR;					\
	}								\
} while (0)


typedef struct _tcl_coups {
  char *name;
  int *lptr;
  int *rptr;
} tcl_coups;

#endif				/* _EGG_TCLEGG_H */
