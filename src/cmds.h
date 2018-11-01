#ifndef _CMDS_H
#define _CMDS_H

#include "types.h"

typedef struct {
  const char          *name;
  const char          *flags;
  Function      func;
  const char          *funcname;
  int type;
} cmd_t;

typedef struct {
  int type;
  const char *cmd;
  size_t garble_len;
  const char *desc;
} help_t;

typedef struct {
        const char *name;
        struct flag_record     flags;
        int type;
} mycmds;

typedef struct {
  const char *name;
//  Function func;
  void (*func) (int, char *);
  int type;
} botcmd_t;

const botcmd_t *search_botcmd_t(const botcmd_t *table, const char* key, size_t elements);

typedef struct cmd_pass {
  struct cmd_pass *next;
  char *name;
  char pass[SHA1_SALTED_LEN + 1];
} cmd_pass_t;

extern mycmds 		cmdlist[]; 
extern int		cmdi;

static inline bool
__attribute__((pure))
is_restricted_cmd(const char* name)
{
  if (name) {
    if (!HAVE_MDOP && !strcasecmp(name, "mmode"))
      return 1;
  }
  return 0;
}

#define findhelp(x) findcmd(x, 1)
const help_t *findcmd(const char *lookup, bool care_about_type) __attribute__((pure));
int check_dcc_attrs(struct userrec *, flag_t);
int check_dcc_chanattrs(struct userrec *, char *, flag_t, flag_t);
int stripmodes(const char *) __attribute__((pure));
const char *stripmasktype(int);
void gotremotecmd(char * forbot, char * frombot, char * fromhand, char * fromidx, char * cmd);
void gotremotereply(char * frombot, char * tohand, char * toidx, char * ln);

#endif /* !_CMDS_H */
