#ifndef _CRYPT_H
#define _CRYPT_H


#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "src/crypto/crypto.h"

#ifdef HAVE_OPENSSL_SSL_H
#  include <openssl/crypto.h>
#  include <openssl/aes.h>
#  include <openssl/sha.h>
//#  include <openssl/md5.h>
#endif /* HAVE_OPENSSL_SSL_H */

#define SHA_HASH_LENGTH (SHA_DIGEST_LENGTH * 2)
#define MD5_HASH_LENGTH (MD5_DIGEST_LENGTH * 2)
#define md5cmp(hash, string)            strcmp(hash, md5(string))


#ifndef MAKING_MODS
char *md5(const char *);
char *encrypt_string(const char *, char *);
char *decrypt_string(const char *, char *);
void encrypt_pass(char *, char *);
char *cryptit (char *);
char *decryptit (char *);
int lfprintf (FILE *, ...);
void EncryptFile(char *, char *);
void DecryptFile(char *, char *);
#endif /* !MAKING_MODS */

#endif /* !_CRYPT_H */
