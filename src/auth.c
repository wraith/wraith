/*
 * auth.c -- handles:
 *   auth system functions
 *   auth system hooks
 */

#include "eggmain.h"
#include "auth.h"
#include "misc.h"
#include "users.h"
#include "crypt.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include "chan.h"
#include "tandem.h"
#include "modules.h"
#include <pwd.h>
#include <errno.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <signal.h>


#include "stat.h"

extern struct userrec 		*userlist;
extern struct dcc_t		*dcc;
extern struct chanset_t		*chanset;
extern time_t			 now;

#ifdef S_AUTH
extern char 			authkey[];
int 				auth_total = 0;
int 				max_auth = 100;
struct auth_t 			*auth = 0;
#endif /* S_AUTH */

#ifdef S_AUTH
void init_auth_max()
{
  if (max_auth < 1)
    max_auth = 1;
  if (auth)
    auth = realloc(auth, sizeof(struct auth_t) * max_auth);
  else
    auth = malloc(sizeof(struct auth_t) * max_auth);
}

static void expire_auths()
{
  int i = 0, idle = 0;
  if (!ischanhub()) return;
  for (i = 0; i < auth_total;i++) {
    if (auth[i].authed) {
      idle = now - auth[i].atime;
      if (idle >= (60 * 60)) {
        removeauth(i);
      }
    }
  }
}
#endif /* S_AUTH */

void init_auth()
{
#ifdef S_AUTH
  init_auth_max();
  add_hook(HOOK_MINUTELY, (Function) expire_auths);
#endif /* S_AUTH */
}

#ifdef S_AUTH
char *makehash(struct userrec *u, char *rand)
{
  char hash[256], *secpass = NULL;
  if (get_user(&USERENTRY_SECPASS, u)) {
    secpass = strdup(get_user(&USERENTRY_SECPASS, u));
    secpass[strlen(secpass)] = 0;
  }
  sprintf(hash, "%s%s%s", rand, (secpass && secpass[0]) ? secpass : "" , (authkey && authkey[0]) ? authkey : "");
  if (secpass)
    free(secpass);

  return md5(hash);
}

int new_auth(void)
{
  int i = auth_total;

  if (auth_total == max_auth)
    return -1;

  auth_total++;
  egg_bzero((char *) &auth[i], sizeof(struct auth_t));
  return i;
}

/* returns 0 if not found, -1 if problem, and > 0 if found. */
int findauth(char *host)
{
  int i = 0;
  if (!host || !host[0])
    return -1;
  for (i = 0; i < auth_total; i++) {
    if (!auth[i].host) {
      putlog(LOG_MISC, "*", "AUTH ENTRY: %d HAS NO HOST??", i);
      continue;
    }
    putlog(LOG_DEBUG, "*", STR("Debug for findauth: checking: %s i: %d :: %s"), host, i, auth[i].host);
    if (auth[i].host && !strcmp(auth[i].host, host)) {
      return i;
    }
  }
  return -1;
}
  
void removeauth(int n)
{
  putlog(LOG_DEBUG, "*", "Removing %s from auth list.", auth[n].host);
  auth_total--;
  if (n < auth_total)
    egg_memcpy(&auth[n], &auth[auth_total], sizeof(struct auth_t));
  else
    egg_bzero(&auth[n], sizeof(struct auth_t)); /* drummer */
}
#endif /* S_AUTH */
