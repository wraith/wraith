#ifndef _AUTH_H
#define _AUTH_H

#include "crypt.h"

struct auth_t {
  struct userrec *user;
  char hash[MD5_HASH_LENGTH + 1];                /* used for dcc authing */
  char nick[NICKLEN];
  char host[UHOSTLEN];
  int authed;
  int authing;
  time_t authtime;       /* what time they authed at */
  time_t atime;          /* when they last were active */
};

#ifndef MAKING_MODS
int new_auth();
int findauth(char *);
void removeauth(int);
char *makehash(struct userrec *, char *);
#endif /* !MAKING_MODS */

#endif /* !_AUTH_H */
