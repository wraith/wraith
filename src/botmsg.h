#ifndef _BOTMSG_H
#define _BOTMSG_H

#include "common.h"
#include "cfg.h"

/* Return codes for add_note */
#define NOTE_ERROR      0       /* error                        */
#define NOTE_OK         1       /* success                      */
#define NOTE_STORED     2       /* not online; stored           */
#define NOTE_FULL       3       /* too many notes stored        */
#define NOTE_TCL        4       /* tcl binding caught it        */
#define NOTE_AWAY       5       /* away; stored                 */
#define NOTE_FWD        6       /* away; forwarded              */

void botnet_send_cfg(int idx, struct cfg_entry *entry);
void botnet_send_cfg_broad(int idx, struct cfg_entry *entry);
void putbot(char *, char *);
__inline__ void putallbots(char *);
int add_note(char *, char *, char *, int, int);
size_t simple_sprintf (char *, ...);
char *int_to_base10(int);
char *unsigned_int_to_base10(unsigned int);
char *int_to_base64(unsigned int);

#endif /* !_BOTMSG_H */

