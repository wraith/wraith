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

char *MD5(const char *);
int md5cmp(const char *, const char*);
char *MD5FILE(const char *);
char *SHA1(const char *);
int sha1cmp(const char *, const char*);

unsigned char *encrypt_binary(const char *, unsigned char *, size_t *);
unsigned char *decrypt_binary(const char *, unsigned char *, size_t *);
char *encrypt_string(const char *, char *);
char *decrypt_string(const char *, char *);
void encrypt_cmd_pass(char *, char *);
char *encrypt_pass(struct userrec *, char *);
char *decrypt_pass(struct userrec *);
char *cryptit (char *);
char *decryptit (char *);
int lfprintf (FILE *, const char *, ...) __attribute__((format(printf, 2, 3)));
void Encrypt_File(char *, char *);
void Decrypt_File(char *, char *);
char *btoh(const unsigned char *, size_t);
void do_crypt_console();

#endif /* !_CRYPT_H */
