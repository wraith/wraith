#ifndef _USERENT_H
#define _USERENT_H

#include "users.h"

void update_mod(const char *, const char *, const char *, const char *);
void list_type_kill(struct list_type *);
void stats_add(struct userrec *, int, int);
void init_userent(void);

#endif /* !_USERENT_H */
