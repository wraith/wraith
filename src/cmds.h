#ifndef _CMDS_H
#define _CMDS_H

#include "types.h"

bool is_restricted_cmd(const char*);

typedef struct {
  const char          *name;
  const char          *flags;
  Function      func;
  const char          *funcname;
  int type;
} cmd_t;

typedef struct {
        const char *name;
        struct flag_record     flags;
} mycmds;

typedef struct {
  const char *name;
//  Function func;
  void (*func) (int, char *);
  int type;
} botcmd_t;

typedef struct cmd_pass {
  struct cmd_pass *next;
  char *name;
  char pass[25];
} cmd_pass_t;

extern mycmds 		cmdlist[]; 
extern int		cmdi;

int findcmd(const char *, bool);
int findhelp(const char *);
int check_dcc_attrs(struct userrec *, flag_t);
int check_dcc_chanattrs(struct userrec *, char *, flag_t, flag_t);
int stripmodes(char *);
char *stripmasktype(int);
void gotremotecmd(char * forbot, char * frombot, char * fromhand, char * fromidx, char * cmd);
void gotremotereply(char * frombot, char * tohand, char * toidx, char * ln);

#endif /* !_CMDS_H */
