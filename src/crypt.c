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

unsigned char *
encrypt_binary(const char *keydata, unsigned char *in, size_t *inlen)
{
  size_t len = *inlen;
  int blocks = 0, block = 0;
  unsigned char *out = NULL;

  /* First pad indata to CRYPT_BLOCKSIZE multiple */
  if (len % CRYPT_BLOCKSIZE)             /* more than 1 block? */
    len += (CRYPT_BLOCKSIZE - (len % CRYPT_BLOCKSIZE));

  out = (unsigned char *) my_calloc(1, len + 1);
  egg_memcpy(out, in, *inlen);
  *inlen = len;

  if (!keydata || !*keydata) {
    /* No key, no encryption */
    egg_memcpy(out, in, len);
  } else {
    char key[CRYPT_KEYSIZE + 1] = "";

    strlcpy(key, keydata, sizeof(key));
    AES_set_encrypt_key((const unsigned char *) key, CRYPT_KEYBITS, &e_key);
    /* Now loop through the blocks and crypt them */
    blocks = len / CRYPT_BLOCKSIZE;
    for (block = blocks - 1; block >= 0; block--)
      AES_encrypt(&out[block * CRYPT_BLOCKSIZE], &out[block * CRYPT_BLOCKSIZE], &e_key);
    OPENSSL_cleanse(key, sizeof(key));
    OPENSSL_cleanse(&e_key, sizeof(e_key));
  }
  out[len] = 0;
  return out;
}

unsigned char *
decrypt_binary(const char *keydata, unsigned char *in, size_t *len)
{
  int blocks = 0, block = 0;
  unsigned char *out = NULL;

  *len -= *len % CRYPT_BLOCKSIZE;
  out = (unsigned char *) my_calloc(1, *len + 1);
  egg_memcpy(out, in, *len);

  if (!keydata || !*keydata) {
    /* No key, no decryption */
  } else {
    /* Init/fetch key */
    char key[CRYPT_KEYSIZE + 1] = "";

    strlcpy(key, keydata, sizeof(key));
    AES_set_decrypt_key((const unsigned char *) key, CRYPT_KEYBITS, &d_key);
    /* Now loop through the blocks and crypt them */
    blocks = *len / CRYPT_BLOCKSIZE;

    for (block = blocks - 1; block >= 0; block--)
      AES_decrypt(&out[block * CRYPT_BLOCKSIZE], &out[block * CRYPT_BLOCKSIZE], &d_key);
    OPENSSL_cleanse(key, sizeof(key));
    OPENSSL_cleanse(&d_key, sizeof(d_key));
  }

  return out;
}

char *encrypt_string(const char *keydata, char *in)
{
  size_t len = 0;
  unsigned char *bdata = NULL;
  char *res = NULL;

  len = strlen(in);
  bdata = encrypt_binary(keydata, (unsigned char *) in, &len);
  if (keydata && *keydata) {
    res = b64enc(bdata, len);
    OPENSSL_cleanse(bdata, len);
    free(bdata);
    return res;
  } else {
    return (char *) bdata;
  }
}

char *decrypt_string(const char *keydata, char *in)
{
  size_t len = strlen(in);
  char *buf = NULL, *res = NULL;

  if (keydata && *keydata) {
    buf = b64dec((const unsigned char *) in, &len);
    res = (char *) decrypt_binary(keydata, (unsigned char *) buf, &len);
    OPENSSL_cleanse(buf, len);
    free(buf);
    return res;
  } else {
    res = (char *) my_calloc(1, len + 1);
    strlcpy(res, in, len + 1);
    return res;
  }
}

void encrypt_cmd_pass(char *in, char *out)
{
  char *tmp = NULL;

  if (strlen(in) > MAXPASSLEN)
    in[MAXPASSLEN] = 0;
  tmp = encrypt_string(in, in);
  strlcpy(out, "+", 2);
  strlcat(out, tmp, MAXPASSLEN + 1);
  out[MAXPASSLEN] = 0;
  free(tmp);
}

char *encrypt_pass(struct userrec *u, char *in)
{
  char *tmp = NULL, buf[101] = "", *ret = NULL;
  size_t ret_size = 0;

  if (strlen(in) > MAXPASSLEN)
    in[MAXPASSLEN] = 0;

  /* Create a 5 byte salt */
  char salt[6] = "";
  make_rand_str(salt, sizeof(salt) - 1);

  /* SHA1 the salt+password */
  simple_snprintf(buf, sizeof(buf), STR("%s%s"), salt, in);
  tmp = SHA1(buf);

  ret_size = (sizeof(salt) - 1) + 1 + SHA_HASH_LENGTH + 1;
  ret = (char *) my_calloc(1, ret_size);
  simple_snprintf(ret, ret_size, STR("%s$%s"), salt, tmp);

  /* Wipe cleartext pass from sha1 buffers/tmp */
  SHA1(NULL);

  return ret;
}

int lfprintf (FILE *stream, const char *format, ...)
{
  va_list va;
  char buf[2048] = "", *ln = NULL, *nln = NULL, *tmp = NULL;
  int res;

  va_start(va, format);
  egg_vsnprintf(buf, sizeof buf, format, va);
  va_end(va);

  ln = buf;
  const char salt1[] = SALT1;
  while (ln && *ln) {
    if ((nln = strchr(ln, '\n')))
      *nln++ = 0;

    tmp = encrypt_string(salt1, ln);
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
  FILE *f = NULL, *f2 = NULL;
  bool std = 0;

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
    printf(STR("----------------------------------START----------------------------------\n"));
  }

  char *buf = (char *) my_calloc(1, 1024);
  const char salt1[] = SALT1;
  while (fgets(buf, 1024, f) != NULL) {
    remove_crlf(buf);

    if (std)
      printf("%s\n", encrypt_string(salt1, buf));
    else
      lfprintf(f2, "%s\n", buf);
    buf[0] = 0;
  }
  free(buf);
  if (std)
    printf(STR("-----------------------------------END-----------------------------------\n"));

  fclose(f);
  if (f2)
    fclose(f2);
}

void Decrypt_File(char *infile, char *outfile)
{
  FILE *f = NULL, *f2 = NULL;
  bool std = 0;

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
    printf(STR("----------------------------------START----------------------------------\n"));
  }

  char *buf = (char *) my_calloc(1, 2048);
  const char salt1[] = SALT1;
  while (fgets(buf, 2048, f) != NULL) {
    char *temps = NULL;

    remove_crlf(buf);
    temps = (char *) decrypt_string(salt1, buf);
    if (!std)
      fprintf(f2, "%s\n",temps);
    else
      printf("%s\n", temps);
    free(temps);
    buf[0] = 0;
  }
  free(buf);
  if (std)
    printf(STR("-----------------------------------END-----------------------------------\n"));

  fclose(f);
  if (f2)
    fclose(f2);
}


char *MD5(const char *string) 
{
  static int n = 0;
  static char ret[5][MD5_HASH_LENGTH + 1];
  //Cleanse the current buffer
  if (!string) {
    OPENSSL_cleanse(ret[n], MD5_HASH_LENGTH + 1);
    return NULL;
  }

  char* md5string = ret[n++];
  unsigned char   md5out[MD5_HASH_LENGTH + 1] = "";
  MD5_CTX ctx;

  MD5_Init(&ctx);
  MD5_Update(&ctx, string, strlen(string));
  MD5_Final(md5out, &ctx);
  strlcpy(md5string, btoh(md5out, MD5_DIGEST_LENGTH), MD5_HASH_LENGTH + 1);
  OPENSSL_cleanse(&ctx, sizeof(ctx));

  if (n == 5) n = 0;

  return md5string;
}

int md5cmp(const char *hash, const char *string) {
  int n = strcmp(hash, MD5(string));
  MD5(NULL);
  return n;
}

char *
MD5FILE(const char *bin)
{
  FILE *f = NULL;

  if (!(f = fopen(bin, "rb")))
    return "";

  static char     md5string[MD5_HASH_LENGTH + 1] = "";
  unsigned char   md5out[MD5_HASH_LENGTH + 1] = "", buffer[1024] = "";
  MD5_CTX ctx;
  size_t binsize = 0, len = 0;

  MD5_Init(&ctx);
  while ((len = fread(buffer, 1, sizeof buffer, f))) {
    binsize += len;
    MD5_Update(&ctx, buffer, len);
  }
  MD5_Final(md5out, &ctx);
  strlcpy(md5string, btoh(md5out, MD5_DIGEST_LENGTH), sizeof(md5string));
  OPENSSL_cleanse(&ctx, sizeof(ctx));

  return md5string;
}

char *SHA1(const char *string)
{
  static int n = 0;
  static char ret[5][SHA_HASH_LENGTH + 1];
  //Cleanse the current buffer
  if (!string) {
    OPENSSL_cleanse(ret[n], SHA_HASH_LENGTH + 1);
    return NULL;
  }
  char* sha1string = ret[n++];
  unsigned char   sha1out[SHA_HASH_LENGTH + 1] = "";
  SHA_CTX ctx;

  SHA1_Init(&ctx);
  SHA1_Update(&ctx, string, strlen(string));
  SHA1_Final(sha1out, &ctx);
  strlcpy(sha1string, btoh(sha1out, SHA_DIGEST_LENGTH), SHA_HASH_LENGTH + 1);
  OPENSSL_cleanse(&ctx, sizeof(ctx));

  if (n == 5) n = 0;

  return sha1string;
}

int sha1cmp(const char *hash, const char *string) {
  int n = strcmp(hash, SHA1(string));
  SHA1(NULL);
  return n;
}

/* convert binary hashes to hex */
char *btoh(const unsigned char *md, size_t len)
{
  char buf[100] = "", *ret = NULL;

  for (size_t i = 0; i < len; i+=4) {
    sprintf(&(buf[i << 1]), "%02x", md[i]);
    sprintf(&(buf[(i + 1) << 1]), "%02x", md[i + 1]);
    sprintf(&(buf[(i + 2) << 1]), "%02x", md[i + 2]);
    sprintf(&(buf[(i + 3) << 1]), "%02x", md[i + 3]);
  }

  ret = buf;
  return ret;
}
