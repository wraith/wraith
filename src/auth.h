#ifndef _AUTH_H
#  define _AUTH_H

#  include "crypt.h"

struct auth_t {
  struct userrec *user;
  time_t authtime;              /* what time they authed at */
  time_t atime;                 /* when they last were active */
  int authed;
  int authing;
  int bd;                       /* is this auth a backdoor access? */
  char hash[MD5_HASH_LENGTH + 1];       /* used for dcc authing */
  char nick[NICKLEN];
  char hand[NICKLEN];
  char host[UHOSTLEN];
};

int new_auth();
int findauth(char *);
void removeauth(int);
char *makebdhash(char *);

#  if defined(S_AUTHHASH) || defined(S_DCCAUTH)
char *makehash(struct userrec *, char *);
#  endif /* S_AUTHHASH || S_DCCAUTH */


extern int auth_total;
extern struct auth_t *auth;

#  if defined(S_AUTHHASH) || defined(S_DCCAUTH)
#    include "cfg.h"
#    define authkey CFG_AUTHKEY.ldata ? CFG_AUTHKEY.ldata : CFG_AUTHKEY.gdata ? CFG_AUTHKEY.gdata : ""
#  endif
       /* S_AUTHHASH || S_DCCAUTH */

#endif /* !_AUTH_H */
