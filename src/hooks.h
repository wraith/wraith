#ifndef _HOOKS_H
#define _HOOKS_H

#include "types.h"

#define HOOK_READ_USERFILE        1
#define HOOK_USERFILE             2
#define HOOK_PRE_REHASH           3
#define HOOK_IDLE                 4
#define HOOK_BACKUP               5
#define HOOK_DIE                  6
#define HOOK_DAILY                7
#define REAL_HOOKS                8

#define HOOK_RFC_CASECMP        111

struct hook_entry {
  struct hook_entry *next;
  int (*func) ();
} *hook_list[REAL_HOOKS];

#define call_hook(x) do {                                       \
        register struct hook_entry *p, *pn;                     \
                                                                \
        for (p = hook_list[x]; p; p = pn) {                     \
                pn = p->next;                                   \
                p->func();                                      \
        }                                                       \
} while (0)

void hooks_init();
void add_hook(int, Function);
void del_hook(int, Function);
int call_hook_cccc(int, char *, char *, char *, char *);

#endif /* !_HOOKS_H */
