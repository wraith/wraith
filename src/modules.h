/*
 * modules.h
 *   support for modules in eggdrop
 *
 * by Darrin Smith (beldin@light.iinet.net.au)
 *
 */

#ifndef _EGG_MODULE_H
#define _EGG_MODULE_H

/* Module related structures
 */
#include "mod/modvals.h"

#ifndef MAKING_NUMMODS

/* Modules specific functions and functions called by eggdrop
 */

void do_module_report(int, int, char *);

int module_register(char *name, Function * funcs,
		    int major, int minor);
const char *module_load(char *module_name);
char *module_unload(char *module_name, char *nick);
module_entry *module_find(char *name, int, int);
Function *module_depend(char *, char *, int major, int minor);
int module_undepend(char *);
void add_hook(int hook_num, Function func);
void del_hook(int hook_num, Function func);
void *get_next_hook(int hook_num, void *func);
extern struct hook_entry {
  struct hook_entry *next;
  int (*func) ();
} *hook_list[REAL_HOOKS];

#define call_hook(x) do {					\
	register struct hook_entry *p, *pn;			\
								\
	for (p = hook_list[x]; p; p = pn) {			\
		pn = p->next;					\
		p->func();					\
	}							\
} while (0)
int call_hook_cccc(int, char *, char *, char *, char *);

#endif

typedef struct _dependancy {
  struct _module_entry *needed;
  struct _module_entry *needing;
  struct _dependancy *next;
  int major;
  int minor;
} dependancy;
extern dependancy *dependancy_list;
#endif				/* _EGG_MODULE_H */
