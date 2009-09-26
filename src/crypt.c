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
#include <bdlib/src/String.h>

char *encrypt_string(const char *keydata, char *in)
{
  size_t len = 0;
  unsigned char *bdata = NULL;
  char *res = NULL;

  len = strlen(in);
  bdata = aes_encrypt_ecb_binary(keydata, (unsigned char *) in, &len);
  if (keydata && *keydata) {
    res = b64enc(bdata, &len);
    OPENSSL_cleanse(bdata, len);
    free(bdata);
    return res;
  } else {
    return (char *) bdata;
  }
}

/**
 * @brief Encrypt a string
 * @param key The key to encrypt with
 * @param data The string to encrypt
 * @return A new, encrypted string
 */
bd::String encrypt_string(const bd::String& key, const bd::String& data) {
  if (!key) return data;
  size_t len = data.length();
  char *bdata = (char*) aes_encrypt_ecb_binary(key.c_str(), (unsigned char*) data.c_str(), &len);
  bd::String encrypted(bdata, len);
  free(bdata);
  return encrypted;
}

char *decrypt_string(const char *keydata, char *in)
{
  size_t len = strlen(in);
  char *buf = NULL, *res = NULL;

  if (keydata && *keydata) {
    buf = b64dec((const unsigned char *) in, &len);
    res = (char *) aes_decrypt_ecb_binary(keydata, (unsigned char *) buf, &len);
    OPENSSL_cleanse(buf, len);
    free(buf);
    return res;
  } else {
    res = (char *) my_calloc(1, len + 1);
    strlcpy(res, in, len + 1);
    return res;
  }
}

/**
 * @brief Decrypt a string
 * @param key The key to decrypt with
 * @param data The string to decrypt
 * @return A new, decrypted string
 */
bd::String decrypt_string(const bd::String& key, const bd::String& data) {
  if (!key) return data;
  size_t len = data.length();
  char *bdata = (char*) aes_decrypt_ecb_binary(key.c_str(), (unsigned char*) data.c_str(), &len);
  bd::String decrypted(bdata, len);
  free(bdata);
  return decrypted;
}

char *salted_sha1(const char *in, const char* saltin)
{
  char *tmp = NULL, buf[101] = "", *ret = NULL;
  size_t ret_size = 0;


  /* Create a 5 byte salt */
  char salt[SHA1_SALT_LEN + 1] = "";
  if (saltin) {
    strlcpy(salt, saltin, sizeof(salt));
  } else {
    make_rand_str(salt, sizeof(salt) - 1);
  }

  /* SHA1 the salt+password */
  simple_snprintf(buf, sizeof(buf), STR("%s%s"), salt, in);
  tmp = SHA1(buf);

  ret_size = SHA1_SALTED_LEN + 1;
  ret = (char *) my_calloc(1, ret_size);
  simple_snprintf(ret, ret_size, STR("+%s$%s"), salt, tmp);

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
  btoh(md5out, MD5_DIGEST_LENGTH, md5string, MD5_HASH_LENGTH + 1);
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
  btoh(md5out, MD5_DIGEST_LENGTH, md5string, sizeof(md5string));
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
  btoh(sha1out, SHA_DIGEST_LENGTH, sha1string, SHA_HASH_LENGTH + 1);
  OPENSSL_cleanse(&ctx, sizeof(ctx));

  if (n == 5) n = 0;

  return sha1string;
}

int sha1cmp(const char *hash, const char *string) {
  int n = strcmp(hash, SHA1(string));
  SHA1(NULL);
  return n;
}

void btoh(const unsigned char *md, size_t md_len, char *buf, const size_t buf_len)
{
#define doblock(n) simple_snprintf(&(buf[(i + n) << 1]), buf_len - ((i + n) << 1), "%02x", md[i + n]);
  for (size_t i = 0; i < md_len; i+=4) {
    doblock(0);
    doblock(1);
    doblock(2);
    doblock(3);
  }
}
