/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2014 Bryan Drewery
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/* enclink.c
 *
 */


#include "common.h"
#include "enclink.h"
#include "dcc.h"
#include "net.h"
#include "misc.h"

#include <stdarg.h>

static void ghost_link_case(int idx, direction_t direction)
{
  int snum = findanysnum(dcc[idx].sock);

  if (likely(snum >= 0)) {
    char initkey[33] = "", *tmp2 = NULL;
    char *keyp = NULL, *nick1 = NULL, *nick2 = NULL;
    in_port_t port = 0;
    const char salt1[] = SALT1;
    const char salt2[] = SALT2;

    if (direction == TO) {
      keyp = socklist[snum].ikey;
      nick1 = strdup(dcc[idx].nick);
      for (int j = 0; j < dcc_total; j++) {
       if (dcc[j].type && dcc[j].sock == dcc[idx].u.relay->sock && dcc[j].type == &DCC_RELAYING) {
         nick2 = strdup(dcc[j].nick);
         break;
       }
      }
      if (!nick2)
        nick2 = strdup(conf.bot->nick);
      port = htons(dcc[idx].port);
    } else if (direction == FROM) {
      keyp = socklist[snum].okey;
      nick1 = strdup(conf.bot->nick);
      nick2 = strdup(dcc[idx].nick);

      struct sockaddr_in sa;
      socklen_t socklen = sizeof(sa);

      bzero(&sa, socklen);
      getsockname(socklist[snum].sock, (struct sockaddr *) &sa, &socklen);
      if (sa.sin_family == AF_UNIX)
        port = 0;
      else
        port = sa.sin_port;
    }

    /* initkey-gen */
    /* salt1 salt2 port mynick conf.bot->nick */
    char tmp[SALT1LEN + 1 + SALT2LEN + 1 + 4 + 1 + HANDLEN + 1 + HANDLEN + 1] = "";
    simple_snprintf(tmp, sizeof(tmp), STR("%s@%s@%4x@%s@%s"), salt1, salt2, port, strtoupper(nick1), strtoupper(nick2));
    free(nick1);
    free(nick2);
    strlcpy(keyp, SHA1(tmp), ENC_KEY_LEN + 1);
#ifdef DEBUG
    putlog(LOG_DEBUG, "@", "Link hash for %s: %s", dcc[idx].nick, tmp);
    putlog(LOG_DEBUG, "@", "outkey (%zu): %s", strlen(keyp), keyp);
#endif
    OPENSSL_cleanse(tmp, sizeof(tmp));
    SHA1(NULL);

    if (direction == FROM) {
      make_rand_str(initkey, 32);       /* set the initial out/in link key to random chars. */
      socklist[snum].oseed = random();
      socklist[snum].iseed = socklist[snum].oseed;
      tmp2 = encrypt_string(salt2, initkey);
      putlog(LOG_BOTS, "*", STR("Sending encrypted link handshake to %s..."), dcc[idx].nick);

      socklist[snum].encstatus = 1;
      socklist[snum].gz = 1;

      link_send(idx, STR("elink %s %d\n"), tmp2, socklist[snum].oseed);
      free(tmp2);
      strlcpy(socklist[snum].okey, initkey, ENC_KEY_LEN + 1);
      strlcpy(socklist[snum].ikey, initkey, ENC_KEY_LEN + 1);
      OPENSSL_cleanse(initkey, sizeof(initkey));
    } else {
      socklist[snum].encstatus = 1;
      socklist[snum].gz = 1;
    }
  } else {
    putlog(LOG_MISC, "*", STR("Couldn't find socket for %s connection?? Shouldn't happen :/"), dcc[idx].nick);
    killsock(dcc[idx].sock);
    lostdcc(idx);
  }
}

static int
prand(int *seed, int range)
{
  long long i1 = *seed;

  i1 = (i1 * 0x08088405 + 1) & 0xFFFFFFFF;
  *seed = i1;
  return ((i1 * range) >> 32);
}

static void
rotate_key(char* key, int& seed)
{
  if (seed) {
    *(uint32_t *) & key[0] = prand(&seed, 0xFFFFFFFF);
    *(uint32_t *) & key[4] = prand(&seed, 0xFFFFFFFF);
    *(uint32_t *) & key[8] = prand(&seed, 0xFFFFFFFF);
    *(uint32_t *) & key[12] = prand(&seed, 0xFFFFFFFF);

    if (!seed)
      seed++;
  }
}

static int ghost_read(int snum, char *src)
{
  char *line = decrypt_string(socklist[snum].ikey, src);

  strcpy(src, line);
  OPENSSL_cleanse(line, strlen(line) + 1);
  free(line);
  rotate_key(socklist[snum].ikey, socklist[snum].iseed);
  return OK;
}

static const char *ghost_write(int snum, const char *src, size_t *len)
{
  static char buf[SGRAB + 14] = "";
  char *srcbuf = NULL, *line = NULL, *eol = NULL, *eline = NULL;

  const size_t bufsiz = *len + 9 + 1;
  srcbuf = (char *) calloc(1, bufsiz);
  strlcpy(srcbuf, src, bufsiz);
  line = srcbuf;
  buf[0] = 0;

  eol = strchr(line, '\n');
  while (eol) {
    *eol++ = 0;
    eline = encrypt_string(socklist[snum].okey, line);
    rotate_key(socklist[snum].okey, socklist[snum].oseed);
    strlcat(buf, eline, sizeof(buf));
    free(eline);
    *len = strlcat(buf, "\n", sizeof(buf));
    line = eol;
    eol = strchr(line, '\n');
  }
  if (line[0]) {
    eline = encrypt_string(socklist[snum].okey, line);
    rotate_key(socklist[snum].okey, socklist[snum].oseed);
    strlcat(buf, eline, sizeof(buf));
    free(eline);
    *len = strlcat(buf, "\n", sizeof(buf));
  }
  OPENSSL_cleanse(srcbuf, bufsiz);
  free(srcbuf);

  return buf;
}

void ghost_parse(int idx, int snum, char *buf)
{
  /* putlog(LOG_DEBUG, "*", "Got elink: %s %s", code, buf); */
  /* Set the socket key and we're linked */

  char *code = newsplit(&buf);

  if (!strcasecmp(code, STR("elink"))) {
    const char salt2[] = SALT2;
    char *tmp = decrypt_string(salt2, newsplit(&buf));

    strlcpy(socklist[snum].okey, tmp, ENC_KEY_LEN + 1);
    OPENSSL_cleanse(tmp, strlen(tmp));
    free(tmp);

    strlcpy(socklist[snum].ikey, socklist[snum].okey, ENC_KEY_LEN + 1);

    socklist[snum].iseed = atoi(buf);
    socklist[snum].oseed = atoi(buf);
    putlog(LOG_BOTS, "*", STR("Handshake with %s succeeded, we're linked."), dcc[idx].nick);
    link_done(idx);
  }
}

void link_send(int idx, const char *format, ...)
{
  char s[2001] = "";
  va_list va;

  va_start(va, format);
  egg_vsnprintf(s, sizeof(s) - 1, format, va);
  va_end(va);
  remove_crlf(s);

  dprintf(-dcc[idx].sock, STR("neg! %s\n"), s);
}

void link_done(int idx)
{
  dprintf(-dcc[idx].sock, STR("neg.\n"));
}

int link_find_by_type(int type)
{
  int i = 0;

  for (i = 0; enclink[i].name; i++)
    if (type == enclink[i].type)
      return i;

  return -1;
}

void link_link(int idx, int type, int i, direction_t direction)
{
  if (i == -1 && type != -1) {
    for (i = 0; enclink[i].name; i++) {
      if (enclink[i].type == type)
        break;
    }
  }

  if (i != -1 && enclink[i].link)
    (enclink[i].link) (idx, direction);
  else if (direction == TO)		/* problem finding function, just assume we're done */
    link_done(idx);

  return;
}

int link_read(int snum, char *buf)
{
  int i = socklist[snum].enclink;

  if (i != -1 && enclink[i].read)
    return (enclink[i].read) (snum, buf);

  return -1;
}

const char *link_write(int snum, const char *buf, size_t *len)
{
  int i = socklist[snum].enclink;

  if (i != -1 && enclink[i].write)
    return ((enclink[i].write) (snum, buf, len));

  return buf;
}

void link_challenge_to(int idx, char *buf) {
  int snum = findanysnum(dcc[idx].sock);

  if (snum >= 0) {
    char *rand = newsplit(&buf), *tmp = strdup(buf), *tmpp = tmp, *p = NULL;
    int i = -1;

    while ((p = newsplit(&tmp))[0]) {
      if (str_isdigit(p)) {
        int type = atoi(p);

        /* pick the first (lowest num) one that we share */
        i = link_find_by_type(type);

        if (i != -1)
          break;
      }
    }
    free(tmpp);

    // No shared type!
    if (i == -1) {
      sdprintf(STR("No shared cipher with %s"), dcc[idx].nick);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
    }

    sdprintf(STR("Choosing '%s' (%d/%d) for link to %s"), enclink[i].name, enclink[i].type, i, dcc[idx].nick);
    link_hash(idx, rand);
    dprintf(-dcc[idx].sock, STR("neg %s %d %s\n"), dcc[idx].shahash, enclink[i].type, dcc[idx].nick);
    socklist[snum].enclink = i;
    link_link(idx, -1, i, TO);
  }
}

void link_hash(int idx, char *rand)
{
  char hash[60] = "";

  /* nothing fancy, just something simple that can stop people from playing */
  simple_snprintf(hash, sizeof(hash), STR("enclink%s"), rand);
  strlcpy(dcc[idx].shahash, SHA1(hash), SHA_HASH_LENGTH + 1);
  SHA1(NULL);
  OPENSSL_cleanse(hash, sizeof(hash));
  return;
}

void link_parse(int idx, char *buf)
{
  int snum = findanysnum(dcc[idx].sock);
  int i = socklist[snum].enclink;

  if (i >= 0 && enclink[i].parse)
    (enclink[i].parse) (idx, snum, buf);

  return;
}

void link_get_method(int idx)
{
  if (!dcc[idx].type)
    return;

  int n = dcc[idx].u.enc->method_number;

  if (enclink[n].name)
    dcc[idx].u.enc->method = &(enclink[n]);

  dcc[idx].u.enc->method_number++;
}

/* the order of entries here determines which will be picked */
struct enc_link enclink[] = {
  { "ghost+case3", LINK_GHOSTCASE3, ghost_link_case, ghost_write, ghost_read, ghost_parse },
  { "cleartext", LINK_CLEARTEXT, NULL, NULL, NULL, NULL },
  { NULL, 0, NULL, NULL, NULL, NULL }
};
/* vim: set sts=2 sw=2 ts=8 et: */
