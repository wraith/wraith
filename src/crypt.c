/*
 * crypt.c -- handles:
 * psybnc crypt() 
 * File encryption
 *
 */


#include "common.h"
#include "crypt.h"
#include "settings.h"
#include "misc.h"
#include "base64.h"
#include "src/crypto/crypto.h"
#include <stdarg.h>

#define CRYPT_BLOCKSIZE AES_BLOCK_SIZE
#define CRYPT_KEYBITS 256
#define CRYPT_KEYSIZE (CRYPT_KEYBITS >> 3)

AES_KEY e_key, d_key;

static unsigned char *
encrypt_binary(const char *keydata, const unsigned char *in, size_t *inlen)
{
  size_t len = *inlen;
  int blocks = 0, block = 0;
  unsigned char *out = NULL;

  /* First pad indata to CRYPT_BLOCKSIZE multiple */
  if (len % CRYPT_BLOCKSIZE)             /* more than 1 block? */
    len += (CRYPT_BLOCKSIZE - (len % CRYPT_BLOCKSIZE));

  out = calloc(1, len + 1);
  egg_memcpy(out, in, *inlen);
  *inlen = len;

  if (!keydata || !*keydata) {
    /* No key, no encryption */
    egg_memcpy(out, in, len);
  } else {
    char key[CRYPT_KEYSIZE + 1] = "";

    strncpyz(key, keydata, sizeof(key));
    AES_set_encrypt_key(key, CRYPT_KEYBITS, &e_key);
    /* Now loop through the blocks and crypt them */
    blocks = len / CRYPT_BLOCKSIZE;
    for (block = blocks - 1; block >= 0; block--)
      AES_encrypt(&out[block * CRYPT_BLOCKSIZE], &out[block * CRYPT_BLOCKSIZE], &e_key);
  }
  out[len] = 0;
  return out;
}

static unsigned char *
decrypt_binary(const char *keydata, unsigned char *in, size_t len)
{
  int blocks = 0, block = 0;
  unsigned char *out = NULL;

  len -= len % CRYPT_BLOCKSIZE;
  out = calloc(1, len + 1);
  egg_memcpy(out, in, len);

  if (!keydata || !*keydata) {
    /* No key, no decryption */
  } else {
    /* Init/fetch key */
    char key[CRYPT_KEYSIZE + 1] = "";

    strncpyz(key, keydata, sizeof(key));
    AES_set_decrypt_key(key, CRYPT_KEYBITS, &d_key);
    /* Now loop through the blocks and crypt them */
    blocks = len / CRYPT_BLOCKSIZE;

    for (block = blocks - 1; block >= 0; block--)
      AES_decrypt(&out[block * CRYPT_BLOCKSIZE], &out[block * CRYPT_BLOCKSIZE], &d_key);
  }

  return out;
}

char *encrypt_string(const char *keydata, char *in)
{
  size_t len = 0;
  unsigned char *bdata = NULL;
  char *res = NULL;

  len = strlen(in) + 1;
  bdata = encrypt_binary(keydata, in, &len);
  if (keydata && *keydata) {
    res = b64enc(bdata, len);
    free(bdata);
    return res;
  } else {
    return bdata;
  }
}

char *decrypt_string(const char *keydata, char *in)
{
  size_t len = 0;
  char *buf = NULL, *res = NULL;

  len = strlen(in);
  if (keydata && *keydata) {
    buf = b64dec(in, &len);
    res = decrypt_binary(keydata, buf, len);
    free(buf);
    return res;
  } else {
    res = calloc(1, len + 1);
    strcpy(res, in);
    return res;
  }
}

void encrypt_pass(char *s1, char *s2)
{
  char *tmp = NULL;

  if (strlen(s1) > MAXPASSLEN)
    s1[MAXPASSLEN] = 0;
  tmp = encrypt_string(s1, s1);
  strcpy(s2, "+");
  strncat(s2, tmp, MAXPASSLEN);
  s2[MAXPASSLEN] = 0;
  free(tmp);
}

int lfprintf (FILE *stream, char *format, ...)
{
  va_list va;
  char buf[2048], *ln = NULL, *nln = NULL, *tmp = NULL;
  int res;

  buf[0] = 0;
  va_start(va, format);
  egg_vsnprintf(buf, sizeof buf, format, va);
  va_end(va);

  ln = buf;
  while (ln && *ln) {
    if ((nln = strchr(ln, '\n')))
      *nln++ = 0;

    tmp = encrypt_string(settings.salt1, ln);
    res = fprintf(stream, "%s\n", tmp);
    free(tmp);
    if (res == EOF)
      return EOF;
    ln = nln;
  }
  return 0;
}

void Encrypt_File(char *infile, char *outfile)
{
  char *buf = NULL;
  FILE *f = NULL, *f2 = NULL;
  int std = 0;

  if (!strcmp(outfile, "STDOUT"))
    std = 1;
  f  = fopen(infile, "r");
  if(!f)
    return;
  if (!std) {
    f2 = fopen(outfile, "w");
    if (!f2)
      return;
  } else {
    printf("----------------------------------START----------------------------------\n");
  }

  buf = calloc(1, 1024);
  while (fgets(buf, 1024, f) != NULL) {
    remove_crlf(buf);

    if (std)
      printf("%s\n", encrypt_string(settings.salt1, buf));
    else
      lfprintf(f2, "%s\n", buf);
    buf[0] = 0;
  }
  free(buf);
  if (std)
    printf("-----------------------------------END-----------------------------------\n");

  fclose(f);
  if (f2)
    fclose(f2);
}

void Decrypt_File(char *infile, char *outfile)
{
  char *buf = NULL;
  FILE *f = NULL, *f2 = NULL;
  int std = 0;

  if (!strcmp(outfile, "STDOUT"))
    std = 1;
  f  = fopen(infile, "r");
  if (!f)
    return;
  if (!std) {
    f2 = fopen(outfile, "w");
    if (!f2)
      return;
  } else {
    printf("----------------------------------START----------------------------------\n");
  }

  buf = calloc(1, 2048);
  while (fgets(buf, 2048, f) != NULL) {
    char *temps = NULL;

    remove_crlf(buf);
    temps = (char *) decrypt_string(settings.salt1, buf);
    if (!std)
      fprintf(f2, "%s\n",temps);
    else
      printf("%s\n", temps);
    free(temps);
    buf[0] = 0;
  }
  free(buf);
  if (std)
    printf("-----------------------------------END-----------------------------------\n");

  fclose(f);
  if (f2)
    fclose(f2);
}


char *MD5(const char *string) 
{
  static char	  md5string[MD5_HASH_LENGTH + 1] = "";
  unsigned char   md5out[MD5_HASH_LENGTH + 1] = "";
  MD5_CTX ctx;

  MD5_Init(&ctx);
  MD5_Update(&ctx, string, strlen(string));
  MD5_Final(md5out, &ctx);
  strncpyz(md5string, btoh(md5out, MD5_DIGEST_LENGTH), sizeof(md5string));
  OPENSSL_cleanse(&ctx, sizeof(ctx));
  return md5string;
}

char *
MD5FILE(const char *bin)
{
  static char     md5string[MD5_HASH_LENGTH + 1] = "";
  unsigned char   md5out[MD5_HASH_LENGTH + 1] = "", buffer[1024] = "";
  MD5_CTX ctx;
  size_t binsize = 0, len = 0;
  FILE *f = NULL;

  if (!(f = fopen(bin, "rb")))
    return "";

  MD5_Init(&ctx);
  while ((len = fread(buffer, 1, sizeof buffer, f))) {
    binsize += len;
    MD5_Update(&ctx, buffer, len);
  }
  MD5_Final(md5out, &ctx);
  strncpyz(md5string, btoh(md5out, MD5_DIGEST_LENGTH), sizeof(md5string));
  OPENSSL_cleanse(&ctx, sizeof(ctx));

  return md5string;
}

char *SHA1(const char *string)
{
  static char	  sha1string[SHA_HASH_LENGTH + 1] = "";
  unsigned char   sha1out[SHA_HASH_LENGTH + 1] = "";
  SHA_CTX ctx;

  SHA1_Init(&ctx);
  SHA1_Update(&ctx, string, strlen(string));
  SHA1_Final(sha1out, &ctx);
  strncpyz(sha1string, btoh(sha1out, SHA_DIGEST_LENGTH), sizeof(sha1string));
  OPENSSL_cleanse(&ctx, sizeof(ctx));
  return sha1string;
}

/* convert binary hashes to hex */
char *btoh(const unsigned char *md, int len)
{
  int i;
  char buf[100] = "", *ret = NULL;

  for (i = 0; i < len; i+=4) {
    sprintf(&(buf[i << 1]), "%02x", md[i]);
    sprintf(&(buf[(i + 1) << 1]), "%02x", md[i + 1]);
    sprintf(&(buf[(i + 2) << 1]), "%02x", md[i + 2]);
    sprintf(&(buf[(i + 3) << 1]), "%02x", md[i + 3]);
  }

  ret = buf;
  return ret;
}
