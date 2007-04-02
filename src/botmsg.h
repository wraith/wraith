#ifndef _BOTMSG_H
#define _BOTMSG_H

#include "common.h"
#include "set.h"

/* Return codes for add_note */
#define NOTE_ERROR      0       /* error                        */
#define NOTE_OK         1       /* success                      */

void botnet_send_var(int idx, variable_t *);
void botnet_send_var_broad(int idx, variable_t *);
void putbot(const char *, char *);
void putallbots(const char *);
int add_note(char *, char *, char *, int, int);

#endif /* !_BOTMSG_H */

