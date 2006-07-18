#ifndef _CRYPT_H
#define _CRYPT_H

#include <stdio.h>

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <sys/types.h>
#include "src/crypto/crypto.h"
#include "users.h"

#define SHA_HASH_LENGTH (SHA_DIGEST_LENGTH << 1)
#define MD5_HASH_LENGTH (MD5_DIGEST_LENGTH << 1)

#define SHA1_SALT_LEN 5
#define SHA1_SALTED_LEN (1 + SHA1_SALT_LEN + 1 + SHA_HASH_LENGTH)

char *MD5(const char *);
int md5cmp(const char *, const char*);
char *MD5FILE(const char *);
char *SHA1(const char *);
int sha1cmp(const char *, const char*);

char *encrypt_string(const char *, char *);
char *decrypt_string(const char *, char *);
char *salted_sha1(const char *, const char* = NULL);
char *cryptit (char *);
char *decryptit (char *);
int lfprintf (FILE *, const char *, ...) __attribute__((format(printf, 2, 3)));
void Encrypt_File(char *, char *);
void Decrypt_File(char *, char *);
void btoh(const unsigned char *md, size_t md_len, char *buf, const size_t buf_len);
void do_crypt_console();

#endif /* !_CRYPT_H */
