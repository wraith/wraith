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

  if (snum >= 0) {
    char initkey[33] = "", *tmp2 = NULL;
    char tmp[70] = "";
    char *keyp = NULL, *nick1 = NULL, *nick2 = NULL;
    port_t port = 0;

    if (direction == TO) {
      keyp = socklist[snum].ikey;
      nick1 = strdup(dcc[idx].nick);
      nick2 = strdup(conf.bot->nick);
      port = htons(dcc[idx].port);
    } else if (direction == FROM) {
      keyp = socklist[snum].okey;
      nick1 = strdup(conf.bot->nick);
      nick2 = strdup(dcc[idx].nick);

      struct sockaddr_in sa;
      socklen_t socklen = sizeof(sa);

      egg_bzero(&sa, socklen);
      getsockname(socklist[snum].sock, (struct sockaddr *) &sa, &socklen);
      port = sa.sin_port;
    }

    /* initkey-gen */
    /* bdhash port mynick conf.bot->nick */
    sprintf(tmp, "%s@%4x@%s@%s", settings.bdhash, port, strtoupper(nick1), strtoupper(nick2));
    free(nick1);
    free(nick2);
    strlcpy(keyp, SHA1(tmp), ENC_KEY_LEN + 1);
#ifdef DEBUG
    putlog(LOG_DEBUG, "@", "Link hash for %s: %s", dcc[idx].nick, tmp);
    putlog(LOG_DEBUG, "@", "outkey (%d): %s", strlen(keyp), keyp);
#endif
    OPENSSL_cleanse(tmp, sizeof(tmp));

    if (direction == FROM) {
      make_rand_str(initkey, 32);       /* set the initial out/in link key to random chars. */
      socklist[snum].oseed = random();
      socklist[snum].iseed = socklist[snum].oseed;
      tmp2 = encrypt_string(settings.salt2, initkey);
      putlog(LOG_BOTS, "*", "Sending encrypted link handshake to %s...", dcc[idx].nick);

      socklist[snum].encstatus = 1;
      socklist[snum].gz = 1;

      link_send(idx, "elink %s %d\n", tmp2, socklist[snum].oseed);
      free(tmp2);
      strlcpy(socklist[snum].okey, initkey, ENC_KEY_LEN + 1);
      strlcpy(socklist[snum].ikey, initkey, ENC_KEY_LEN + 1);
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

static int
prand(int *seed, int range)
{
  long long i1 = *seed;

  i1 = (i1 * 0x08088405 + 1) & 0xFFFFFFFF;
  *seed = i1;
  return ((i1 * range) >> 32);
}

static int ghost_read(int snum, char *src, size_t *len)
{
  char *line = decrypt_string(socklist[snum].ikey, src);

  strcpy(src, line);
  free(line);
  if (socklist[snum].iseed) {
    *(dword *) & socklist[snum].ikey[0] = prand(&socklist[snum].iseed, 0xFFFFFFFF);
    *(dword *) & socklist[snum].ikey[4] = prand(&socklist[snum].iseed, 0xFFFFFFFF);
    *(dword *) & socklist[snum].ikey[8] = prand(&socklist[snum].iseed, 0xFFFFFFFF);
    *(dword *) & socklist[snum].ikey[12] = prand(&socklist[snum].iseed, 0xFFFFFFFF);

    if (!socklist[snum].iseed)
      socklist[snum].iseed++;
  }
//  *len = strlen(src);
  return OK;
}

static char *ghost_write(int snum, char *src, size_t *len)
{
  char *srcbuf = NULL, *buf = NULL, *line = NULL, *eol = NULL, *eline = NULL;
  size_t bufpos = 0;

  srcbuf = (char *) my_calloc(1, *len + 9 + 1);
  strcpy(srcbuf, src);
  line = srcbuf;

  eol = strchr(line, '\n');
  while (eol) {
    *eol++ = 0;
    eline = encrypt_string(socklist[snum].okey, line);
    if (socklist[snum].oseed) {
      *(dword *) & socklist[snum].okey[0] = prand(&socklist[snum].oseed, 0xFFFFFFFF);
      *(dword *) & socklist[snum].okey[4] = prand(&socklist[snum].oseed, 0xFFFFFFFF);
      *(dword *) & socklist[snum].okey[8] = prand(&socklist[snum].oseed, 0xFFFFFFFF);
      *(dword *) & socklist[snum].okey[12] = prand(&socklist[snum].oseed, 0xFFFFFFFF);

      if (!socklist[snum].oseed)
        socklist[snum].oseed++;
    }
    buf = (char *) my_realloc(buf, bufpos + strlen(eline) + 1 + 9);
    strcpy((char *) &buf[bufpos], eline);
    free(eline);
    strcat(buf, "\n");
    bufpos = strlen(buf);
    line = eol;
    eol = strchr(line, '\n');
  }
  if (line[0]) {
    eline = encrypt_string(socklist[snum].okey, line);
    if (socklist[snum].oseed) {
      *(dword *) & socklist[snum].okey[0] = prand(&socklist[snum].oseed, 0xFFFFFFFF);
      *(dword *) & socklist[snum].okey[4] = prand(&socklist[snum].oseed, 0xFFFFFFFF);
      *(dword *) & socklist[snum].okey[8] = prand(&socklist[snum].oseed, 0xFFFFFFFF);
      *(dword *) & socklist[snum].okey[12] = prand(&socklist[snum].oseed, 0xFFFFFFFF);

      if (!socklist[snum].oseed)
        socklist[snum].oseed++;
    }
    buf = (char *) my_realloc(buf, bufpos + strlen(eline) + 1 + 9);
    strcpy((char *) &buf[bufpos], eline);
    free(eline);
    strcat(buf, "\n");
  }
  free(srcbuf);

  *len = strlen(buf);
  return buf;
}

void ghost_parse(int idx, int snum, char *buf)
{
  /* putlog(LOG_DEBUG, "*", "Got elink: %s %s", code, buf); */
  /* Set the socket key and we're linked */

  char *code = newsplit(&buf);

  if (!egg_strcasecmp(code, "elink")) {
    char *tmp = decrypt_string(settings.salt2, newsplit(&buf));

    strlcpy(socklist[snum].okey, tmp, ENC_KEY_LEN + 1);
    strlcpy(socklist[snum].ikey, socklist[snum].okey, ENC_KEY_LEN + 1);
    socklist[snum].iseed = atoi(buf);
    socklist[snum].oseed = atoi(buf);
    putlog(LOG_BOTS, "*", "Handshake with %s succeeded, we're linked.", dcc[idx].nick);
    free(tmp);
    link_done(idx);
  }
}

#ifdef no
static int binary_read(int snum, char *src, size_t *len)
{
  char *line = NULL;

  line = decrypt_binary(socklist[snum].ikey, (unsigned char *) src, len);
  strlcpy(src, line, SGRAB + 10);
  free(line);
  if (socklist[snum].iseed) {
    *(dword *) & socklist[snum].ikey[0] = prand(&socklist[snum].iseed, 0xFFFFFFFF);
    *(dword *) & socklist[snum].ikey[4] = prand(&socklist[snum].iseed, 0xFFFFFFFF);
    *(dword *) & socklist[snum].ikey[8] = prand(&socklist[snum].iseed, 0xFFFFFFFF);
    *(dword *) & socklist[snum].ikey[12] = prand(&socklist[snum].iseed, 0xFFFFFFFF);

    if (!socklist[snum].iseed)
      socklist[snum].iseed++;
  }
//  *len = strlen(src);
  return OK;
}

static char *binary_write(int snum, char *src, size_t *len)
{
  char *srcbuf = NULL, *buf = NULL, *line = NULL, *eol = NULL, *eline = NULL;
  size_t bufpos = 0;

  srcbuf = (char *) my_calloc(1, *len + 9 + 1);
  strcpy(srcbuf, src);
  line = srcbuf;

  eol = strchr(line, '\n');
  while (eol) {
    *eol++ = 0;
    eline = encrypt_binary(socklist[snum.okey, (unsigned char *) line, len);
    if (socklist[snum].oseed) {
      *(dword *) & socklist[snum].okey[0] = prand(&socklist[snum].oseed, 0xFFFFFFFF);
      *(dword *) & socklist[snum].okey[4] = prand(&socklist[snum].oseed, 0xFFFFFFFF);
      *(dword *) & socklist[snum].okey[8] = prand(&socklist[snum].oseed, 0xFFFFFFFF);
      *(dword *) & socklist[snum].okey[12] = prand(&socklist[snum].oseed, 0xFFFFFFFF);

      if (!socklist[snum].oseed)
        socklist[snum].oseed++;
    }
    buf = (char *) my_realloc(buf, bufpos + len + 1 + 9);
    strcpy((char *) &buf[bufpos], eline);
    free(eline);
    strcat(buf, "\n");
    bufpos = strlen(buf);
    line = eol;
    eol = strchr(line, '\n');
  }
  if (line[0]) {
    eline = encrypt_string(socklist[snum].okey, line);
    if (socklist[snum].oseed) {
      *(dword *) & socklist[snum].okey[0] = prand(&socklist[snum].oseed, 0xFFFFFFFF);
      *(dword *) & socklist[snum].okey[4] = prand(&socklist[snum].oseed, 0xFFFFFFFF);
      *(dword *) & socklist[snum].okey[8] = prand(&socklist[snum].oseed, 0xFFFFFFFF);
      *(dword *) & socklist[snum].okey[12] = prand(&socklist[snum].oseed, 0xFFFFFFFF);

      if (!socklist[snum].oseed)
        socklist[snum].oseed++;
    }
    buf = (char *) my_realloc(buf, bufpos + strlen(eline) + 1 + 9);
    strcpy((char *) &buf[bufpos], eline);
    free(eline);
    strcat(buf, "\n");
  }
  free(srcbuf);

  *len = strlen(buf);
  return buf;
}
#endif

void link_send(int idx, const char *format, ...)
{
  char s[2001] = "";
  va_list va;

  va_start(va, format);
  egg_vsnprintf(s, sizeof(s) - 1, format, va);
  va_end(va);
  remove_crlf(s);

  dprintf(-dcc[idx].sock, "neg! %s\n", s);
}

void link_done(int idx)
{
  dprintf(-dcc[idx].sock, "neg.\n");
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

int link_read(int snum, char *buf, size_t *len)
{
  int i = socklist[snum].enclink;

  if (i != -1 && enclink[i].read)
    return (enclink[i].read) (snum, buf, len);

  return -1;
}

char *link_write(int snum, char *buf, size_t *len)
{
  int i = socklist[snum].enclink;

  if (i != -1 && enclink[i].write)
    return ((enclink[i].write) (snum, buf, len));

  return buf;
}

void link_hash(int idx, char *rand)
{
  char hash[60] = "";

  /* nothing fancy, just something simple that can stop people from playing */
  simple_snprintf(hash, sizeof(hash), "enclink%s", rand);
  strlcpy(dcc[idx].shahash, SHA1(hash), SHA_HASH_LENGTH + 1);
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
  { "ghost+case2", LINK_GHOSTCASE2, ghost_link_case, ghost_write, ghost_read, ghost_parse },
// Disabled this one so 1.2.6->1.2.7 will use cleartext, as some 1.2.6 nets have an empty BDHASH
//  { "ghost+case", LINK_GHOSTCASE, ghost_link_case, ghost_write, ghost_read, ghost_parse },
  { "cleartext", LINK_CLEARTEXT, NULL, NULL, NULL, NULL },
  { NULL, 0, NULL, NULL, NULL, NULL }
};
