#ifndef _EGG_CONF_H
#define _EGG_CONF_H

#define CFGT_INT       1  /* Integer */
#define CFGT_STRING    2  /* String */

#define CFGF_GLOBAL  1    /* Accessible as .config */
#define CFGF_LOCAL   2    /* Accessible as .botconfig */

typedef struct cfg_entry {
  char *name;
  int  type;
  int  flags;
  void * gdata;
  void * ldata;
  void (*globalchanged) (struct cfg_entry *, void * oldval, int * valid);
  void (*localchanged) (struct cfg_entry *, void * oldval, int * valid);
  void (*describe) (struct cfg_entry *, int idx);
} cfg_entry_T;

void set_cfg_int (char * target, struct cfg_entry * entry, int data);
void set_cfg_str (char * target, struct cfg_entry * entry, char * data);
void add_cfg(struct cfg_entry * entry) {

#endif
