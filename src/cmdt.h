#ifndef _EGG_CMDT_H
#define _EGG_CMDT_H
#define CMD_LEAVE (Function)(-1)
typedef struct
{
  char *name;
  char *flags;
  Function func;
  char *funcname;
} cmd_t;
typedef struct
{
  char *name;
  char *flags;
  Function func;
  char *usage;
  char *desc;
  char *funcname;
} dcc_cmd_t;
typedef struct
{
  char *name;
  Function func;
} botcmd_t;
#endif
