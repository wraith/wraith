/*
 * This is an OpenSSL-compatible implementation of the RSA Data Security,
 * Inc. MD5 Message-Digest Algorithm.
 *
 * Written by Solar Designer <solar@openwall.com> in 2001, and placed in
 * the public domain.  See md5.c for more information.
 */

#ifndef _MD5_H
#define _MD5_H

#define MD5_DIGEST_LENGTH      16

/* Any 32-bit or wider integer data type will do */
typedef unsigned long MD5_u32plus;

typedef struct {
	MD5_u32plus lo, hi;
	MD5_u32plus a, b, c, d;
	unsigned char buffer[64];
	MD5_u32plus block[16];
} MD5_CTX;

void MD5_Init(MD5_CTX *);
void MD5_Update(MD5_CTX *, const void *, unsigned long);
void MD5_Final(unsigned char *, MD5_CTX *);
void MD5_Hex(unsigned char *, char *);

#endif /* !_MD5_H */
