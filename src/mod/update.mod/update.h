#ifndef _EGG_MOD_update_update_H
#define _EGG_MOD_update_update_H
typedef struct
{
  char *feature;
  int flag;
  int (*ask_func) (int);
  int priority;
  int (*snd) (int, char *);
  int (*rcv) (int, char *);
} uff_table_t;
#ifndef MAKING_update
#define finish_update ((void (*) (int))update_funcs[4])
#ifdef HUB
#define bupdating (*(int*)update_funcs[8])
#endif
#endif
#endif
