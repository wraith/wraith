/*
 * auth.c -- handles:
 *   auth system functions
 *   auth system hooks
 */

#include "common.h"
#include "auth.h"
#include "misc.h"
#include "main.h"
#include "types.h"
#include "egg_timer.h"
#include "users.h"
#include "crypt.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include "chan.h"
#include "tandem.h"
#include <pwd.h>
#include <errno.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <signal.h>


#include "stat.h"

#ifdef S_AUTHCMDS
int auth_total = 0;
static int max_auth = 100;
struct auth_t *auth = NULL;
#endif /* S_AUTHCMDS */

#if defined(S_AUTHHASH) || defined(S_DCCAUTH)
char authkey[121] = "";         /* This is one of the keys used in the auth hash */
#endif /* S_AUTHHASH || S_DCCAUTH */

#ifdef S_AUTHCMDS
void
init_auth_max()
{
  if (max_auth < 1)
    max_auth = 1;
  if (auth)
    auth = realloc(auth, sizeof(struct auth_t) * max_auth);
  else
    auth = calloc(1, sizeof(struct auth_t) * max_auth);
}

static void
expire_auths()
{
  int i = 0, idle = 0;

  if (!ischanhub())
    return;
  for (i = 0; i < auth_total; i++) {
    if (auth[i].authed) {
      idle = now - auth[i].atime;
      if (idle >= (60 * 60)) {
        removeauth(i);
      }
    }
  }
}
#endif /* S_AUTHCMDS */

void
init_auth()
{
#ifdef S_AUTHCMDS
  init_auth_max();
  timer_create_secs(60, "expire_auths", (Function) expire_auths);
#endif /* S_AUTHCMDS */
}

#if defined(S_AUTHHASH) || defined(S_DCCAUTH)
char *
makehash(struct userrec *u, char *randstring)
{
  char hash[256] = "", *secpass = NULL;

  if (get_user(&USERENTRY_SECPASS, u)) {
    secpass = strdup(get_user(&USERENTRY_SECPASS, u));
    secpass[strlen(secpass)] = 0;
  }
  egg_snprintf(hash, sizeof hash, "%s%s%s", randstring, (secpass && secpass[0]) ? secpass : "", (authkey && authkey[0]) ? authkey : "");
  if (secpass)
    free(secpass);

  return MD5(hash);
}
#endif /* S_AUTHHASH || S_DCCAUTH */

#ifdef S_AUTHCMDS
int
new_auth(void)
{
  int i = auth_total;

  if (auth_total == max_auth)
    return -1;

  auth_total++;
  egg_bzero((char *) &auth[i], sizeof(struct auth_t));
  return i;
}

/* returns 0 if not found, -1 if problem, and > 0 if found. */
int
findauth(char *host)
{
  int i = 0;

  if (!host || !host[0])
    return -1;
  for (i = 0; i < auth_total; i++) {
    if (!auth[i].host) {
      putlog(LOG_MISC, "*", "AUTH ENTRY: %d HAS NO HOST??", i);
      continue;
    }
    if (auth[i].host && !strcmp(auth[i].host, host)) {
      return i;
    }
  }
  return -1;
}

void
removeauth(int n)
{
  putlog(LOG_DEBUG, "*", "Removing %s from auth list.", auth[n].host);
  auth_total--;
  if (n < auth_total)
    egg_memcpy(&auth[n], &auth[auth_total], sizeof(struct auth_t));
  else
    egg_bzero(&auth[n], sizeof(struct auth_t)); /* drummer */
}
#endif /* S_AUTHCMDS */
