/*
 * tclegg.h
 *   stuff used by tcl.c and tclhash.c
 *
 */

#ifndef _EGG_TCLEGG_H
#define _EGG_TCLEGG_H

#include "lush.h"		/* Include this here, since it's needed
				   in this file */
typedef struct _tcl_coups {
  char *name;
  int *lptr;
  int *rptr;
} tcl_coups;

#endif				/* _EGG_TCLEGG_H */
