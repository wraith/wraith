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

typedef struct _tcl_coups {
  char *name;
  int *lptr;
  int *rptr;
} tcl_coups;

#endif				/* _EGG_TCLEGG_H */
