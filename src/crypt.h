#ifndef _CRYPT_H
#define _CRYPT_H

#include <stdio.h>

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <sys/types.h>
#include "src/crypto/crypto.h"

#define SHA_HASH_LENGTH (SHA_DIGEST_LENGTH << 1)
#define MD5_HASH_LENGTH (MD5_DIGEST_LENGTH << 1)
#define md5cmp(hash, string)            strcmp(hash, MD5(string))

char *MD5(const char *);
char *MD5FILE(const char *);
char *SHA1(const char *);
char *encrypt_string(const char *, char *);
char *decrypt_string(const char *, char *);
void encrypt_pass(char *, char *);
char *cryptit (char *);
char *decryptit (char *);
int lfprintf (FILE *, const char *, ...) __attribute__((format(printf, 2, 3)));
void Encrypt_File(char *, char *);
void Decrypt_File(char *, char *);
char *btoh(const unsigned char *, size_t);

#endif /* !_CRYPT_H */
