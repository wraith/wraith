/* aes_util.h
 *
 */

#ifndef _AES_UTIL_H
#define _AES_UTIL_H 1
unsigned char *encrypt_binary(const char *, unsigned char *, size_t *);
unsigned char *decrypt_binary(const char *, unsigned char *, size_t *);
#endif
