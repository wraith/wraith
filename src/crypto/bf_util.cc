/* bf_util.c
 *
 */

#include "src/libcrypto.h"
#include "src/compat/compat.h"
#include <bdlib/src/String.h>

#define CRYPT_BLOCKSIZE BF_BLOCK

BF_KEY bf_e_key, bf_d_key;

static const char eggdrop_blowfish_base64[65] = "./0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const int eggdrop_blowfish_base64_index[256] = {
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0,  1,
   2,  3,  4,  5,  6,  7,  8,  9, 10, 11, -1, -1, -1, -1, -1, -1,
  -1, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52,
  53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, -1, -1, -1, -1, -1,
  -1, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
  27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

union bf_data {
  struct {
    BF_LONG left;
    BF_LONG right;
  } lr;
  BF_LONG bf_long;
};

/* These were adapted from eggdrop's blowfish.mod as well as dirtirc */

/*
 * @brief Encrypt a string using eggdrop's blowfish
 * @param in The string to encrypt.
 * @param key The key to use.
 * @returns The encrypted string.
 */
bd::String egg_bf_encrypt(bd::String in, const bd::String& key)
{
  /* No key, no encryption */
  if (!key.length()) return in;

  size_t datalen = in.length();
  bd::String out(static_cast<size_t>(datalen * 1.5));
  if (datalen % 8 != 0) {
    datalen += 8 - (datalen % 8);
    in.resize(datalen, 0);
  }
  BF_set_key(&bf_e_key, key.length(), (unsigned char *)key.cbegin());
  bf_data data;
  size_t part;
  unsigned char *s = (unsigned char *)in.cbegin();
  for (size_t i = 0; i < in.length(); i += 8) {
    data.lr.left = *s++ << 24;
    data.lr.left += *s++ << 16;
    data.lr.left += *s++ << 8;
    data.lr.left += *s++;
    data.lr.right = *s++ << 24;
    data.lr.right += *s++ << 16;
    data.lr.right += *s++ << 8;
    data.lr.right += *s++;
    BF_encrypt(&data.bf_long, &bf_e_key);
    for (part = 0; part < 6; part++) {
      out += eggdrop_blowfish_base64[data.lr.right & 0x3f];
      data.lr.right = data.lr.right >> 6;
    }
    for (part = 0; part < 6; part++) {
      out += eggdrop_blowfish_base64[data.lr.left & 0x3f];
      data.lr.left = data.lr.left >> 6;
    }
  }
  return out;
}

/*
 * @brief Decrypt a string using eggdrop's blowfish
 * @param in The string to decrypt.
 * @param key The key to use.
 * @returns The decrypted string if valid. The string passed in if no key is given. A truncated decrypted string in the case of error.
 */
bd::String egg_bf_decrypt(bd::String in, const bd::String& key)
{
  // Skip over '+OK '
  if (in(0, 4) == "+OK ")
    in += static_cast<size_t>(4);
  bd::String out(static_cast<size_t>(in.length() * .9));
  // Too small to process
  if (in.size() < 12) return out;

  // Not valid base64
  if (eggdrop_blowfish_base64_index[int(in[0])] == -1) return out;

  int cut_off = in.length() % 12;
  if (cut_off > 0)
    in.resize(in.length() - cut_off);

  BF_set_key(&bf_d_key, key.length(), (unsigned char *)key.cbegin());
  bf_data data;
  int val;
  size_t part;
  char *s = (char *)in.cbegin();
  for (size_t i = 0; i < in.length(); i += 12) {
    data.lr.left = 0;
    data.lr.right = 0;
    for (part = 0; part < 6; part++) {
      if ((val = eggdrop_blowfish_base64_index[int(*s++)]) == -1) return out;
      data.lr.right |= (char)val << part * 6;
    }
    for (part = 0; part < 6; part++) {
      if ((val = eggdrop_blowfish_base64_index[int(*s++)]) == -1) return out;
      data.lr.left |= (char)val << part * 6;
    }
    BF_decrypt(&data.bf_long, &bf_d_key);
    for (part = 0; part < 4; part++) {
      const char decrypted_char = char((data.lr.left & (0xff << ((3 - part) * 8))) >> ((3 - part) * 8));
      // Don't write NULLs into the string
      if (decrypted_char) {
        out += decrypted_char;
      }
    }
    for (part = 0; part < 4; part++) {
      const char decrypted_char = char((data.lr.right & (0xff << ((3 - part) * 8))) >> ((3 - part) * 8));
      // Don't write NULLs into the string
      if (decrypted_char) {
        out += decrypted_char;
      }
    }
  }

  return out;
}

#ifdef not_needed
unsigned char *
bf_encrypt_ecb_binary(const char *keydata, unsigned char *in, size_t *inlen)
{
  size_t len = *inlen;
  int blocks = 0, block = 0;
  unsigned char *out = NULL;

  /* First pad indata to CRYPT_BLOCKSIZE multiple */
  if (len % CRYPT_BLOCKSIZE)             /* more than 1 block? */
    len += (CRYPT_BLOCKSIZE - (len % CRYPT_BLOCKSIZE));

  out = (unsigned char *) calloc(1, len + 1);
  memcpy(out, in, *inlen);
  *inlen = len;

  if (!keydata || !*keydata) {
    /* No key, no encryption */
    memcpy(out, in, len);
  } else {
    BF_set_key(&bf_e_key, strlen(keydata), (const unsigned char*) keydata);
    /* Now loop through the blocks and crypt them */
    blocks = len / CRYPT_BLOCKSIZE;
    for (block = blocks - 1; block >= 0; --block)
      BF_ecb_encrypt(&out[block * CRYPT_BLOCKSIZE], &out[block * CRYPT_BLOCKSIZE], &bf_e_key, BF_ENCRYPT);
    OPENSSL_cleanse(&bf_e_key, sizeof(bf_e_key));
  }
  out[len] = 0;
  return out;
}

unsigned char *
bf_decrypt_ecb_binary(const char *keydata, unsigned char *in, size_t *len)
{
  int blocks = 0, block = 0;
  unsigned char *out = NULL;

  *len -= *len % CRYPT_BLOCKSIZE;
  out = (unsigned char *) calloc(1, *len + 1);
  memcpy(out, in, *len);

  if (!keydata || !*keydata) {
    /* No key, no decryption */
  } else {
    BF_set_key(&bf_d_key, strlen(keydata), (const unsigned char*) keydata);
    /* Now loop through the blocks and decrypt them */
    blocks = *len / CRYPT_BLOCKSIZE;

    for (block = blocks - 1; block >= 0; --block)
      BF_ecb_encrypt(&out[block * CRYPT_BLOCKSIZE], &out[block * CRYPT_BLOCKSIZE], &bf_d_key, BF_DECRYPT);
    OPENSSL_cleanse(&bf_d_key, sizeof(bf_d_key));
  }

  *len = strlen((char*) out);
  out[*len] = 0;
  return out;
}

unsigned char *
bf_encrypt_cbc_binary(const char *keydata, unsigned char *in, size_t *inlen, unsigned char *ivec)
{
  size_t len = *inlen;
  unsigned char *out = NULL;

  /* First pad indata to CRYPT_BLOCKSIZE multiple */
  if (len % CRYPT_BLOCKSIZE)             /* more than 1 block? */
    len += (CRYPT_BLOCKSIZE - (len % CRYPT_BLOCKSIZE));

  out = (unsigned char *) calloc(1, len + 1);
  *inlen = len;

  if (!keydata || !*keydata) {
    /* No key, no encryption */
    memcpy(out, in, len);
  } else {
    BF_set_key(&bf_e_key, strlen(keydata), (const unsigned char*) keydata);
    BF_cbc_encrypt(in, out, len, &bf_e_key, ivec, BF_ENCRYPT);
    OPENSSL_cleanse(&bf_e_key, sizeof(bf_e_key));
  }
  out[len] = 0;
  return out;
}

unsigned char *
bf_decrypt_cbc_binary(const char *keydata, unsigned char *in, size_t *len, unsigned char* ivec)
{
  unsigned char *out = NULL;

  *len -= *len % CRYPT_BLOCKSIZE;
  out = (unsigned char *) calloc(1, *len + 1);

  if (!keydata || !*keydata) {
    /* No key, no decryption */
  } else {
    BF_set_key(&bf_d_key, strlen(keydata), (const unsigned char*) keydata);
    BF_cbc_encrypt(in, out, *len, &bf_d_key, ivec, BF_DECRYPT);
    OPENSSL_cleanse(&bf_d_key, sizeof(bf_d_key));
  }

  *len = strlen((char*) out);
  out[*len] = 0;
  return out;
}
#endif
/* vim: set sts=2 sw=2 ts=8 et: */
