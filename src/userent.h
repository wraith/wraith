#ifndef _USERENT_H
#define _USERENT_H

#include "users.h"

#ifndef MAKING_MODS
void update_mod(char *, char *, char *, char *);
void list_type_kill(struct list_type *);
void stats_add(struct userrec *, int, int);
#endif /* !MAKING_MODS */

#endif /* !_USERENT_H */
