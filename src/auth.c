/*
 * auth.c -- handles:
 *   auth system functions
 *   auth system hooks
 */

#include "main.h"
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
#include "bg.h"

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

/* Expected memory usage
 */
int expmem_auth()
{
  int tot = 0;
#ifdef S_AUTH
  tot += sizeof(struct auth_t) * max_auth;
#endif /* S_AUTH */
  return tot;
}

#ifdef S_AUTH
void init_auth_max()
{
  if (max_auth < 1)
    max_auth = 1;
  if (auth)
    auth = nrealloc(auth, sizeof(struct auth_t) * max_auth);
  else
    auth = nmalloc(sizeof(struct auth_t) * max_auth);
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
  MD5_CTX ctx;
  unsigned char md5out[MD5_HASH_LENGTH + 1];
  char md5string[MD5_HASH_LENGTH + 1], hash[256], *ret = NULL, *secpass = NULL;
  if (get_user(&USERENTRY_SECPASS, u)) {
    secpass = nmalloc(strlen(get_user(&USERENTRY_SECPASS, u)) + 1);
    strcpy(secpass, (char *) get_user(&USERENTRY_SECPASS, u));
    secpass[strlen(secpass)] = 0;
  }
  sprintf(hash, "%s%s%s", rand, (secpass && secpass[0]) ? secpass : "" , authkey[0] ? authkey : "");
  if (secpass)
    nfree(secpass);
  MD5_Init(&ctx);
  MD5_Update(&ctx, hash, strlen(hash));
  MD5_Final(md5out, &ctx);
  strncpyz(md5string, btoh(md5out, MD5_DIGEST_LENGTH), sizeof md5string);

  ret = md5string;
  return ret;
}

int new_auth(void)
{
  int i = auth_total;

Context;
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
Context;
  if (!host || !host[0])
    return -1;
Context;
  for (i = 0; i < auth_total; i++) {
Context;
    if (!auth[i].host) {
Context;
      putlog(LOG_MISC, "*", "AUTH ENTRY: %d HAS NO HOST??", i);
      continue;
    }
Context;
    putlog(LOG_DEBUG, "*", STR("Debug for findauth: checking: %s i: %d :: %s"), host, i, auth[i].host);
Context;
    if (auth[i].host && !strcmp(auth[i].host, host)) {
Context;
      return i;
    }
  }
Context;
  return -1;
}
  
void removeauth(int n)
{
Context;
  putlog(LOG_DEBUG, "*", "Removing %s from auth list.", auth[n].host);
  auth_total--;
  if (n < auth_total)
    egg_memcpy(&auth[n], &auth[auth_total], sizeof(struct auth_t));
  else
    egg_bzero(&auth[n], sizeof(struct auth_t)); /* drummer */
}
#endif /* S_AUTH */
