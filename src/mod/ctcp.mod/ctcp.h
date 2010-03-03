/*
 * ctcp.h -- part of ctcp.mod
 *   all the defines for ctcp.c
 *
 */

#ifndef _EGG_MOD_CTCP_CTCP_H
#define _EGG_MOD_CTCP_CTCP_H

#define CLOAK_COUNT             11 /* The number of scripts currently existing */
#define CLOAK_PLAIN             1 /* This is your plain bitchx client behaviour */
#define CLOAK_CRACKROCK         2
#define CLOAK_NEONAPPLE         3
#define CLOAK_TUNNELVISION      4
#define CLOAK_ARGON             5
#define CLOAK_EVOLVER           6
#define CLOAK_PREVAIL           7
#define CLOAK_CYPRESS           8 /* Now with full theme and customization support */
#define CLOAK_MIRC              9
#define CLOAK_OTHER             10

void ctcp_init();
void scriptchanged();
extern char		kickprefix[], bankickprefix[];
extern int		first_ctcp_check;

#endif				/* _EGG_MOD_CTCP_CTCP_H */
