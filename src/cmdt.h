/*
 * cmdt.h
 *   stuff for builtin commands
 *
 */

#ifndef _EGG_CMDT_H
#define _EGG_CMDT_H

typedef struct {
  char *name;
  char *flags;
  Function func;
  char *funcname;
} cmd_t;

typedef struct {
  char *name;
  Function func;
} botcmd_t;

#endif				/* _EGG_CMDT_H */
