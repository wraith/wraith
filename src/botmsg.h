#ifndef _BOTMSG_H
#define _BOTMSG_H

#include "eggmain.h"
#include "cfg.h"

#ifndef MAKING_MODS
void botnet_send_cfg(int idx, struct cfg_entry *entry);
void botnet_send_cfg_broad(int idx, struct cfg_entry *entry);
void putbot(char *, char *);
void putallbots(char *);
int add_note(char *, char *, char *, int, int);
int simple_sprintf (char *, ...);
void tandout_but (int, ...);
char *int_to_base10(int);
char *unsigned_int_to_base10(unsigned int);
char *int_to_base64(unsigned int);
#endif /* !MAKING_MODS */

#endif /* !_BOTMSG_H */

