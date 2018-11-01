#ifndef _CRYPT_H
#define _CRYPT_H

#include <stdio.h>

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <sys/types.h>
#include "libcrypto.h"
#include "users.h"

#define SHA_HASH_LENGTH (SHA_DIGEST_LENGTH << 1)
#define SHA256_HASH_LENGTH (SHA256_DIGEST_LENGTH << 1)
#define MD5_HASH_LENGTH (MD5_DIGEST_LENGTH << 1)

#define SHA1_SALT_LEN 5
#define SHA1_SALTED_LEN (1 + SHA1_SALT_LEN + 1 + SHA_HASH_LENGTH)

char *MD5(const char *);
int md5cmp(const char *, const char*);
char *SHA1(const char *);
int sha1cmp(const char *, const char*);
char *SHA256(const char *);
int sha256cmp(const char *, const char*);

char *encrypt_string(const char *, char *);
char *decrypt_string(const char *, char *);
char *salted_sha1(const char *, const char* = NULL);
int salted_sha1cmp(const char *, const char*);
void Encrypt_File(char *, char *);
void Decrypt_File(char *, char *);
void btoh(const unsigned char *md, size_t md_len, char *buf, const size_t buf_len);
void do_crypt_console();

#endif /* !_CRYPT_H */
