#ifndef _AUTH_H
#define _AUTH_H

#include "crypt.h"

#ifdef S_AUTHCMDS
struct auth_t {
  struct userrec *user;
#ifdef S_AUTHHASH 
  char hash[MD5_HASH_LENGTH + 1];                /* used for dcc authing */
#endif /* S_AUTHHASH */
  char nick[NICKLEN];
  char host[UHOSTLEN];
  int authed;
  int authing;
  time_t authtime;       /* what time they authed at */
  time_t atime;          /* when they last were active */
};
#endif /* S_AUTHCMDS */

#ifndef MAKING_MODS
# ifdef S_AUTHCMDS
int new_auth();
int findauth(char *);
void removeauth(int);
# endif /* S_AUTHCMDS */
# if defined(S_AUTHHASH) || defined(S_DCCAUTH)
char *makehash(struct userrec *, char *);
# endif /* S_AUTHHASH || S_DCCAUTH */
#endif /* !MAKING_MODS */

#endif /* !_AUTH_H */
