#ifndef _CRYPT_H
#define _CRYPT_H

#ifndef MAKING_MODS
char *md5(const char *);
char *encrypt_string(const char *, char *);
char *decrypt_string(const char *, char *);
void encrypt_pass(char *, char *);
char *cryptit (char *);
char *decryptit (char *);
int lfprintf EGG_VARARGS(FILE *, arg1);
void EncryptFile(char *, char *);
void DecryptFile(char *, char *);
#endif /* !MAKING_MODS */

#endif /* !_CRYPT_H */
