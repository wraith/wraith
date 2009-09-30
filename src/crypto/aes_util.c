/* aes_util.c
 *
 */

#include "aes.h"
#include "src/compat/compat.h"

#define CRYPT_BLOCKSIZE AES_BLOCK_SIZE
#define CRYPT_KEYBITS 256
#define CRYPT_KEYSIZE (CRYPT_KEYBITS >> 3)

AES_KEY e_key, d_key;

unsigned char *
aes_encrypt_ecb_binary(const char *keydata, unsigned char *in, size_t *inlen)
{
  size_t len = *inlen;
  int blocks = 0, block = 0;
  unsigned char *out = NULL;

  /* First pad indata to CRYPT_BLOCKSIZE multiple */
  if (len % CRYPT_BLOCKSIZE)             /* more than 1 block? */
    len += (CRYPT_BLOCKSIZE - (len % CRYPT_BLOCKSIZE));

  out = (unsigned char *) my_calloc(1, len + 1);
  memcpy(out, in, *inlen);
  *inlen = len;

  if (!keydata || !*keydata) {
    /* No key, no encryption */
    memcpy(out, in, len);
  } else {
    char key[CRYPT_KEYSIZE + 1] = "";

    strlcpy(key, keydata, sizeof(key));
    AES_set_encrypt_key((const unsigned char *) key, CRYPT_KEYBITS, &e_key);
    /* Now loop through the blocks and crypt them */
    blocks = len / CRYPT_BLOCKSIZE;
    for (block = blocks - 1; block >= 0; --block)
      AES_encrypt(&out[block * CRYPT_BLOCKSIZE], &out[block * CRYPT_BLOCKSIZE], &e_key);
    OPENSSL_cleanse(key, sizeof(key));
    OPENSSL_cleanse(&e_key, sizeof(e_key));
  }
  out[len] = 0;
  return out;
}

unsigned char *
aes_decrypt_ecb_binary(const char *keydata, unsigned char *in, size_t *len)
{
  int blocks = 0, block = 0;
  unsigned char *out = NULL;

  *len -= *len % CRYPT_BLOCKSIZE;
  out = (unsigned char *) my_calloc(1, *len + 1);
  memcpy(out, in, *len);

  if (!keydata || !*keydata) {
    /* No key, no decryption */
  } else {
    /* Init/fetch key */
    char key[CRYPT_KEYSIZE + 1] = "";

    strlcpy(key, keydata, sizeof(key));
    AES_set_decrypt_key((const unsigned char *) key, CRYPT_KEYBITS, &d_key);
    /* Now loop through the blocks and decrypt them */
    blocks = *len / CRYPT_BLOCKSIZE;

    for (block = blocks - 1; block >= 0; --block)
      AES_decrypt(&out[block * CRYPT_BLOCKSIZE], &out[block * CRYPT_BLOCKSIZE], &d_key);
    OPENSSL_cleanse(key, sizeof(key));
    OPENSSL_cleanse(&d_key, sizeof(d_key));
  }

  *len = strlen((char*) out);
  out[*len] = 0;
  return out;
}
