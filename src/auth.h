#ifndef _AUTH_H
#  define _AUTH_H

#  include "cfg.h"
#  include "crypt.h"

struct auth_t {
  struct userrec *user;
  time_t authtime;              /* what time they authed at */
  time_t atime;                 /* when they last were active */
  int authed;
  int authing;
  int bd;                       /* is this auth a backdoor access? */
  int idx;			/* do they have an associated idx? */
  char hash[MD5_HASH_LENGTH + 1];       /* used for dcc authing */
  char rand[50];
  char nick[NICKLEN];
  char hand[NICKLEN];
  char host[UHOSTLEN];
};

int new_auth();
int findauth(char *);
void removeauth(int);
char *makebdhash(char *);
void makehash(int, int, char *);
void init_auth(void);
void check_auth_dcc(int, const char *, const char *);
extern int auth_total;
extern struct auth_t *auth;

#  define authkey CFG_AUTHKEY.ldata ? CFG_AUTHKEY.ldata : CFG_AUTHKEY.gdata ? CFG_AUTHKEY.gdata : ""

#endif /* !_AUTH_H */
