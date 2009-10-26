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
#include <bdlib/src/Stream.h>
#include "EncryptedStream.h"

char *encrypt_string(const char *keydata, char *in)
{
  size_t len = 0;
  unsigned char *bdata = NULL;
  char *res = NULL;

  len = strlen(in);
  bdata = aes_encrypt_ecb_binary(keydata, (unsigned char *) in, &len);
  if (keydata && *keydata) {
    res = b64enc(bdata, len);
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
  OPENSSL_cleanse(bdata, len);
  free(bdata);
  return decrypted;
}

int salted_sha1cmp(const char *salted_hash, const char *string) {
  char* cmp = salted_sha1(string, &salted_hash[1]); //Pass in the salt from the given hash
  int n = strcmp(salted_hash, cmp);
  free(cmp);
  return n;
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

void Encrypt_File(char *infile, char *outfile)
{
  const char salt1[] = SALT1;
  bd::Stream stream_in;
  EncryptedStream stream_out(salt1);

  stream_in.loadFile(infile);
  stream_out << bd::String(stream_in);
  stream_out.writeFile(outfile);
}

void Decrypt_File(char *infile, char *outfile)
{
  const char salt1[] = SALT1;
  bd::Stream stream_out;
  EncryptedStream stream_in(salt1);

  stream_in.loadFile(infile);
  stream_out << bd::String(stream_in);
  stream_out.writeFile(outfile);
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
