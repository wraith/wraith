#ifndef _BOTMSG_H
#define _BOTMSG_H

#include "common.h"
#include "set.h"

/* Return codes for add_note */
#define NOTE_ERROR      0       /* error                        */
#define NOTE_OK         1       /* success                      */
#define NOTE_STORED     2       /* not online; stored           */
#define NOTE_FULL       3       /* too many notes stored        */
#define NOTE_TCL        4       /* tcl binding caught it        */
#define NOTE_AWAY       5       /* away; stored                 */
#define NOTE_FWD        6       /* away; forwarded              */

void botnet_send_var(int idx, variable_t *);
void botnet_send_var_broad(int idx, variable_t *);
void putbot(char *, char *);
void putallbots(char *);
int add_note(char *, char *, char *, int, int);

#endif /* !_BOTMSG_H */

