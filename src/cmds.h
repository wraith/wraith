#ifndef _CMDS_H
#define _CMDS_H

typedef struct {
  char          *name;
  char          *flags;
  Function      func;
  char          *funcname;
  int           nohelp;
} cmd_t;

typedef struct {
  char *name;
  Function func;
} botcmd_t;

#ifndef MAKING_MODS
int check_dcc_attrs(struct userrec *, int);
int check_dcc_chanattrs(struct userrec *, char *, int, int);
int stripmodes(char *);
char *stripmasktype(int);
void gotremotecmd(char * forbot, char * frombot, char * fromhand, char * fromidx, char * cmd);
void gotremotereply(char * frombot, char * tohand, char * toidx, char * ln);
#endif /* !MAKING_MODS */

#endif /* !_CMDS_H */
