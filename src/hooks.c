/*
 * hook.c -- handles:
 *
 * hooks
 *
 */

#include "common.h"
#include "hooks.h"


/*
 *     Various hooks & things
 */

/* The REAL hooks, when these are called, a return of 0 indicates unhandled
 * 1 is handled
 */
struct hook_entry *hook_list[REAL_HOOKS];

void
hooks_init()
{
  int i;

  for (i = 0; i < REAL_HOOKS; i++)
    hook_list[i] = NULL;
}

int
call_hook_cccc(int hooknum, char *a, char *b, char *c, char *d)
{
  struct hook_entry *p, *pn;
  int f = 0;

  if (hooknum >= REAL_HOOKS)
    return 0;
  p = hook_list[hooknum];
  for (p = hook_list[hooknum]; p && !f; p = pn) {
    pn = p->next;
    f = p->func(a, b, c, d);
  }
  return f;
}


/* Hooks, various tables of functions to call on ceratin events
 */
void
add_hook(int hook_num, Function func)
{
  if (hook_num < REAL_HOOKS) {
    struct hook_entry *p = NULL;

    for (p = hook_list[hook_num]; p; p = p->next)
      if (p->func == func)
        return;                 /* Don't add it if it's already there */
    p = calloc(1, sizeof(struct hook_entry));

    p->next = hook_list[hook_num];
    hook_list[hook_num] = p;
    p->func = func;
  }
}

void
del_hook(int hook_num, Function func)
{
  if (hook_num < REAL_HOOKS) {
    struct hook_entry *p = hook_list[hook_num], *o = NULL;

    while (p) {
      if (p->func == func) {
        if (o == NULL)
          hook_list[hook_num] = p->next;
        else
          o->next = p->next;
        free(p);
        break;
      }
      o = p;
      p = p->next;
    }
  }
}
