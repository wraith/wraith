/*
 * This is an OpenSSL-compatible implementation of the RSA Data Security,
 * Inc. MD5 Message-Digest Algorithm.
 *
 * Written by Solar Designer <solar@openwall.com> in 2001, and placed in
 * the public domain.  See md5.c for more information.
 */

#ifndef _MD5_H
#define _MD5_H

#if defined(OPENSSL_SYS_WIN16) || defined(__LP32__)
#define MD5_LONG unsigned long
#elif defined(OPENSSL_SYS_CRAY) || defined(__ILP64__)
#define MD5_LONG unsigned long
#define MD5_LONG_LOG2 3
/*
 * _CRAY note. I could declare short, but I have no idea what impact
 * does it have on performance on none-T3E machines. I could declare
 * int, but at least on C90 sizeof(int) can be chosen at compile time.
 * So I've chosen long...
 *                                      <appro@fy.chalmers.se>
 */
#else
#define MD5_LONG unsigned int
#endif

#define MD5_CBLOCK      64
#define MD5_LBLOCK      (MD5_CBLOCK/4)
#define MD5_DIGEST_LENGTH 16

typedef struct MD5state_st
        {
        MD5_LONG A,B,C,D;
        MD5_LONG Nl,Nh;
        MD5_LONG data[MD5_LBLOCK];
        int num;
        } MD5_CTX;

int MD5_Init(MD5_CTX *c);
int MD5_Update(MD5_CTX *c, const void *data, unsigned long len);
int MD5_Final(unsigned char *md, MD5_CTX *c);
//unsigned char *MD5(const unsigned char *d, unsigned long n, unsigned char *md);
void MD5_Transform(MD5_CTX *c, const unsigned char *b);

#endif /* !_MD5_H */
