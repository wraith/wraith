/* aes_util.h
 *
 */

#ifndef _AES_UTIL_H
#define _AES_UTIL_H 1

#include <sys/types.h>

unsigned char *aes_encrypt_ecb_binary(const char *, unsigned char *, size_t *);
unsigned char *aes_decrypt_ecb_binary(const char *, unsigned char *, size_t *);
unsigned char *aes_encrypt_cbc_binary(const char *, unsigned char *, size_t *, unsigned char *);
unsigned char *aes_decrypt_cbc_binary(const char *, unsigned char *, size_t *, unsigned char *);
#endif
