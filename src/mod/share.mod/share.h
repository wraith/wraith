#ifndef _EGG_MOD_SHARE_SHARE_H
#define _EGG_MOD_SHARE_SHARE_H
#define	UFF_OVERRIDE	0x000001
#define UFF_INVITE	0x000002
#define UFF_EXEMPT	0x000004
#define UFF_CHANS	0x000020
#define UFF_TCL	0x000040
typedef struct
{
  char *feature;
  int flag;
  int (*ask_func) (int);
  int priority;
  int (*snd) (int, char *);
  int (*rcv) (int, char *);
} uff_table_t;
#ifndef MAKING_SHARE
#define finish_share ((void (*) (int))share_funcs[4])
#define dump_resync ((void (*) (int))share_funcs[5])
#define uff_addtable ((void (*) (uff_table_t *))share_funcs[6])
#define uff_deltable ((void (*) (uff_table_t *))share_funcs[7])
#endif
#endif
