#include "settings.h"

extern char netpass[];

static struct settings_data settings  = {
  "AAAAAAAAAAAAAAAA", "", "BBBBBBBBBBBBBBBB", ""
};

/*
if not builtin, load ./.cfg into settings
  A | Prefix | Data | Suffix | B
  DKey=MD5Sum(A,B,Prefix)
  NKey=MD5Sum(Prefix, Data) ^ Suffix
*/

char localkey[32] = "";
char datakey[32] = "";

void init_settings(char *binname)
{
  char *buffer = NULL,
   *prefix = NULL,
   *data = NULL,
   *suffix = NULL,
   *partb = NULL;
  char *p = NULL;
  FILE *binf = NULL;

  int binsize,
    i;
  MD5_CTX *ctx;
  struct utsname un;

  binf = fopen(binname, "r");
  if (!binf) {
    printf(STR("can't get config (1)\n"));
    exit(1);
  }
  fseek(binf, 0, SEEK_END);
  binsize = ftell(binf);
  fseek(binf, 0, SEEK_SET);
  buffer = nmalloc(binsize);
  if (fread(buffer, 1, binsize, binf) != binsize) {
    bzero(buffer, binsize);
    nfree(buffer);
    fclose(binf);
    printf(STR("can't get config (2)\n"));
    exit(1);
  }
  fclose(binf);
  p = buffer;
  while (p < (buffer + binsize)) {
    if (*((int *) p) == *((int *) &settings.prefix)) {
      if (!memcmp(p, &settings.prefix, 16))
	break;
    }
    p += 4;
  }
  if (p >= buffer + binsize) {
    bzero(buffer, binsize);
    nfree(buffer);
    printf(STR("can't get config (3)\n"));
    exit(1);
  }
  prefix = p;
  p += 16;
  data = p;
  p += (SETTINGS_SIZE - 32);
  suffix = p;
  p += 16;
  partb = p;
  if (memcmp(suffix, settings.suffix, 16)) {
    bzero(buffer, binsize);
    nfree(buffer);
    printf(STR("can't get config (4)\n"));
    exit(1);
  }
  ctx = nmalloc(sizeof(MD5_CTX));
  bzero(&datakey, sizeof(datakey));
  bzero(&un, sizeof(un));
  uname(&un);
  strcpy(partb, un.nodename);
  if (partb[0]) 
    strcat(partb, " ");
#ifdef __FreeBSD__
  strcat(partb, un.release);
#else
  strcat(partb, un.version);
#endif
  p=partb + strlen(partb) - 1;
  while (*p==' ')
    *p-- = 0;
  MD5Init(ctx);
  MD5Update(ctx, buffer, data - buffer);
  MD5Update(ctx, partb, (buffer + binsize - partb));
  MD5Final(datakey, ctx);
  MD5Init(ctx);
  MD5Update(ctx, prefix, SETTINGS_SIZE - 16);
  bzero(buffer, binsize);
  nfree(buffer);
  MD5Final(netpass, ctx);
  nfree(ctx);
  for (i = 0; i < 16; i++) {
    netpass[i] ^= settings.suffix[i];
    if (!datakey[i])
      datakey[i]++;
  }
  netpass[16] = 0;
  datakey[16] = 0;
#ifdef G_ENCFILES
  strcpy(localkey, datakey);
#endif
}

int get_setting(int name, char *value, int vlen)
{
  int i,
    n;
  struct settings_entry *plainbuf = NULL;
  char *p;

  p = (char *) &settings.data;
  while (p < (char *) (&settings.data + SETTINGS_SIZE - 32)) {
    plainbuf = (void *) decrypt_binary(datakey, p, 16);
    if (plainbuf->name == name) {
      n = plainbuf->size;
      bzero(plainbuf, 16);
      nfree(plainbuf);
      plainbuf = (void *) decrypt_binary(datakey, p, n);
      break;
    }
    if (plainbuf->size) {
      p += plainbuf->size;
      bzero(plainbuf, 16);
      nfree(plainbuf);
      plainbuf = NULL;
    } else {
      bzero(plainbuf, 16);
      nfree(plainbuf);
      plainbuf = NULL;
      break;
    }
  }
  if (plainbuf) {
    i = vlen;
    if (i > plainbuf->size)
      i = plainbuf->size;
    memcpy(value, &plainbuf->data, i);
    bzero(plainbuf, plainbuf->size);
    nfree(plainbuf);
    return 1;
  } else {
    bzero(value, vlen);
    return 0;
  }
  return 0;
};
