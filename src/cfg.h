#ifndef _CFG_H
#define _CFG_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#define CFGF_GLOBAL  1          /* Accessible as .config */
#define CFGF_LOCAL   2          /* Accessible as .botconfig */

typedef struct cfg_entry {
  char *name;
  int flags;
  char *gdata;
  char *ldata;
  void (*globalchanged) (struct cfg_entry *, char *oldval, int *valid);
  void (*localchanged) (struct cfg_entry *, char *oldval, int *valid);
  void (*describe) (struct cfg_entry *, int idx);
} cfg_entry_T;

#ifndef MAKING_MODS
void set_cfg_str(char *, char *, char *);
void add_cfg(struct cfg_entry *);
void got_config_share(int, char *, int);
void userfile_cfg_line(char *);
void trigger_cfg_changed();
#ifdef S_DCCPASS
int check_cmd_pass(const char *, char *);
int has_cmd_pass(const char *);
void set_cmd_pass(char *, int);
#endif /* S_DCCPASS */
#endif /* MAKING_MODS */

#endif /* !_CFG_H */
