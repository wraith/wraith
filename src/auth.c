/*
 * auth.c -- handles:
 *   auth system functions
 *   auth system hooks
 */


#include "common.h"
#include "auth.h"
#include "misc.h"
#include "main.h"
#include "settings.h"
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

int auth_total = 0;
static int max_auth = 50;
struct auth_t *auth = NULL;

static void
expire_auths()
{
  if (!ischanhub())
    return;

  time_t idle = 0;

  for (int i = 0; i < auth_total; i++) {
    if (auth[i].authed) {
      idle = now - auth[i].atime;
      if (idle >= (60 * 60)) {
        removeauth(i);
      }
    }
  }
}

void
init_auth()
{
  if (max_auth < 1)
    max_auth = 1;
  if (auth)
    auth = (struct auth_t *) my_realloc(auth, sizeof(struct auth_t) * max_auth);
  else
    auth = (struct auth_t *) my_calloc(1, sizeof(struct auth_t) * max_auth);

  timer_create_secs(60, "expire_auths", (Function) expire_auths);
}

void makehash(int idx, int authi, char *randstring)
{
  char hash[256] = "", *secpass = NULL;
  struct userrec *u = NULL;

  if (idx != -1)
    u = dcc[idx].user;
  else if (authi != -1)
    u = auth[authi].user;

  if (get_user(&USERENTRY_SECPASS, u)) {
    secpass = strdup((char *) get_user(&USERENTRY_SECPASS, u));
    secpass[strlen(secpass)] = 0;
  }
  egg_snprintf(hash, sizeof hash, "%s%s%s", randstring, (secpass && secpass[0]) ? secpass : "", authkey);
  if (secpass)
    free(secpass);

  if (idx != -1) 
    strlcpy(dcc[idx].hash, MD5(hash), sizeof dcc[idx].hash);
  else if (authi != -1)
    strlcpy(auth[authi].hash, MD5(hash), sizeof auth[authi].hash);

  egg_bzero(hash, sizeof(hash));
}

char *
makebdhash(char *randstring)
{
  char hash[256] = "";
  char *bdpass = "bdpass";

  egg_snprintf(hash, sizeof hash, "%s%s%s", randstring, bdpass, settings.packname);
  sdprintf("bdhash: %s", hash);
  return MD5(hash);
}

int
new_auth(void)
{
  if (auth_total == max_auth)
    return -1;

  egg_bzero((struct auth_t *) &auth[auth_total], sizeof(struct auth_t));
  return auth_total++;
}

/* returns 0 if not found, -1 if problem, and > 0 if found. */
int
findauth(char *host)
{
  if (!host || !host[0])
    return -1;

  for (int i = 0; i < auth_total; i++) {
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
