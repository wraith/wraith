#ifndef _SHA_H
#define _SHA_H

#define SHA_LONG unsigned int

#define SHA_LBLOCK      16
#define SHA_CBLOCK      (SHA_LBLOCK*4)  /* SHA treats input data as a
                                         * contiguous array of 32 bit
                                         * wide big-endian values. */
#define SHA_LAST_BLOCK  (SHA_CBLOCK-8)
#define SHA_DIGEST_LENGTH 20


#define HOST_c2l(c,l)   (l =(((unsigned long)(*((c)++)))<<24),          \
                         l|=(((unsigned long)(*((c)++)))<<16),          \
                         l|=(((unsigned long)(*((c)++)))<< 8),          \
                         l|=(((unsigned long)(*((c)++)))    ),          \
                         l)
#define HOST_p_c2l(c,l,n)       {                                       \
                        switch (n) {                                    \
                        case 0: l =((unsigned long)(*((c)++)))<<24;     \
                        case 1: l|=((unsigned long)(*((c)++)))<<16;     \
                        case 2: l|=((unsigned long)(*((c)++)))<< 8;     \
                        case 3: l|=((unsigned long)(*((c)++)));         \
                                } }
#define HOST_p_c2l_p(c,l,sc,len) {                                      \
                        switch (sc) {                                   \
                        case 0: l =((unsigned long)(*((c)++)))<<24;     \
                                if (--len == 0) break;                  \
                        case 1: l|=((unsigned long)(*((c)++)))<<16;     \
                                if (--len == 0) break;                  \
                        case 2: l|=((unsigned long)(*((c)++)))<< 8;     \
                                } }
/* NOTE the pointer is not incremented at the end of this */
#define HOST_c2l_p(c,l,n)       {                                       \
                        l=0; (c)+=n;                                    \
                        switch (n) {                                    \
                        case 3: l =((unsigned long)(*(--(c))))<< 8;     \
                        case 2: l|=((unsigned long)(*(--(c))))<<16;     \
                        case 1: l|=((unsigned long)(*(--(c))))<<24;     \
                                } }
#define HOST_l2c(l,c)   (*((c)++)=(unsigned char)(((l)>>24)&0xff),      \
                         *((c)++)=(unsigned char)(((l)>>16)&0xff),      \
                         *((c)++)=(unsigned char)(((l)>> 8)&0xff),      \
                         *((c)++)=(unsigned char)(((l)    )&0xff),      \
                         l)


#define SHA_MAKE_STRING(c,s)   do {    \
        unsigned long ll;               \
        ll=(c)->h0; HOST_l2c(ll,(s));   \
        ll=(c)->h1; HOST_l2c(ll,(s));   \
        ll=(c)->h2; HOST_l2c(ll,(s));   \
        ll=(c)->h3; HOST_l2c(ll,(s));   \
        ll=(c)->h4; HOST_l2c(ll,(s));   \
        } while (0)

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
unsigned char *SHA1(const unsigned char *, unsigned long, unsigned char *);
void SHA1_Transform(SHA_CTX *, const unsigned char *);

#endif /* !_SHA_H */
