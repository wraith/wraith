/* enclink.c
 *
 */

#include "common.h"
#include "enclink.h"
#include "dcc.h"
#include "net.h"
#include "misc.h"

static void link_ghost(int idx, direction_t direction)
{
  int snum = findanysnum(dcc[idx].sock);

  if (snum >= 0) {
    char initkey[33] = "", *tmp2 = NULL;
    char tmp[256] = "";
    char *keyp = NULL, *nick1 = NULL, *nick2 = NULL;
    size_t key_len = 0;
    port_t port = 0;

    if (direction == TO) {
      keyp = socklist[snum].ikey;
      key_len = sizeof(socklist[snum].ikey);
      nick1 = dcc[idx].nick;
      nick2 = conf.bot->nick;

      struct sockaddr_in sa;
      socklen_t socklen = sizeof(sa);

      egg_bzero(&sa, socklen);
      getsockname(socklist[snum].sock, (struct sockaddr *) &sa, &socklen);
      port = sa.sin_port;
    } else if (direction == FROM) {
      keyp = socklist[snum].okey;
      key_len = sizeof(socklist[snum].okey);
      nick1 = conf.bot->nick;
      nick2 = dcc[idx].nick;
      port = htons(dcc[idx].port);
    }

    /* initkey-gen */
    /* bdhash port mynick conf.bot->nick */
    sprintf(tmp, "%s@%4x@%s@%s", settings.bdhash, port, nick1, nick2);
    strncpyz(keyp, SHA1(tmp), key_len);
    putlog(LOG_DEBUG, "@", "Link hash for %s: %s", dcc[idx].nick, tmp);
    putlog(LOG_DEBUG, "@", "outkey (%d): %s", strlen(keyp), keyp);

    if (direction == FROM) {
      make_rand_str(initkey, 32);       /* set the initial out/in link key to random chars. */
      socklist[snum].oseed = random();
      socklist[snum].iseed = socklist[snum].oseed;
      tmp2 = encrypt_string(settings.salt2, initkey);
      putlog(LOG_BOTS, "*", "Sending encrypted link handshake to %s...", dcc[idx].nick);

      socklist[snum].encstatus = 1;
      socklist[snum].gz = 1;

      dprintf(idx, "elink %s %d\n", tmp2, socklist[snum].oseed);
      free(tmp2);
      strcpy(socklist[snum].okey, initkey);
      strcpy(socklist[snum].ikey, initkey);
    } else {
      socklist[snum].encstatus = 1;
      socklist[snum].gz = 1;
    }
  } else {
    putlog(LOG_MISC, "*", "Couldn't find socket for %s connection?? Shouldn't happen :/", dcc[idx].nick);
    killsock(dcc[idx].sock);
    lostdcc(idx);
  }
}

void enclink_call(int idx, int type, direction_t direction)
{
  int i = 0;

  for (i = 0; enclink[i].name; i++) {
    if (enclink[i].type == type) {
      (enclink[i].func) (idx, direction);
      return;
    }
  }
  return;
}

struct enc_link enclink[] = {
  { "ghost", LINK_GHOST, link_ghost },
  { NULL, 0, NULL }
};
