#ifndef _SHA_H
#define _SHA_H

#if defined(OPENSSL_SYS_WIN16) || defined(__LP32__)
#define SHA_LONG unsigned long
#elif defined(OPENSSL_SYS_CRAY) || defined(__ILP64__)
#define SHA_LONG unsigned long
#define SHA_LONG_LOG2 3
#else
#define SHA_LONG unsigned int
#endif

#define SHA_LBLOCK      16
#define SHA_CBLOCK      (SHA_LBLOCK*4)  /* SHA treats input data as a
                                         * contiguous array of 32 bit
                                         * wide big-endian values. */
#define SHA_LAST_BLOCK  (SHA_CBLOCK-8)
#define SHA_DIGEST_LENGTH 20


typedef struct SHAstate_st
        {
        SHA_LONG h0,h1,h2,h3,h4;
        SHA_LONG Nl,Nh;
        SHA_LONG data[SHA_LBLOCK];
        int num;
        } SHA_CTX;

int SHA1_Init(SHA_CTX *);
int SHA1_Update(SHA_CTX *, const void *, unsigned long);
int SHA1_Final(unsigned char *, SHA_CTX *);
void SHA1_Transform(SHA_CTX *, const unsigned char *);

#endif /* !_SHA_H */
