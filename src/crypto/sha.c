/* 
 */


#include "sha.h"
#include "cleanse.h"
#include <stdlib.h>

#define Xupdate(a,ix,ia,ib,ic,id)     ( (a)=(ia^ib^ic^id),    \
                                        ix=(a)=ROTATE((a),1)  \
                                      )
#define K_00_19 0x5a827999UL
#define K_20_39 0x6ed9eba1UL
#define K_40_59 0x8f1bbcdcUL
#define K_60_79 0xca62c1d6UL

#define F_00_19(b,c,d)  ((((c) ^ (d)) & (b)) ^ (d))
#define F_20_39(b,c,d)  ((b) ^ (c) ^ (d))
#define F_40_59(b,c,d)  (((b) & (c)) | (((b)|(c)) & (d)))
#define F_60_79(b,c,d)  F_20_39(b,c,d)

#define BODY_00_15(i,a,b,c,d,e,f,xi) \
        (f)=xi+(e)+K_00_19+ROTATE((a),5)+F_00_19((b),(c),(d)); \
        (b)=ROTATE((b),30);

#define BODY_16_19(i,a,b,c,d,e,f,xi,xa,xb,xc,xd) \
        Xupdate(f,xi,xa,xb,xc,xd); \
        (f)+=(e)+K_00_19+ROTATE((a),5)+F_00_19((b),(c),(d)); \
        (b)=ROTATE((b),30);

#define BODY_20_31(i,a,b,c,d,e,f,xi,xa,xb,xc,xd) \
        Xupdate(f,xi,xa,xb,xc,xd); \
        (f)+=(e)+K_20_39+ROTATE((a),5)+F_20_39((b),(c),(d)); \
        (b)=ROTATE((b),30);

#define BODY_32_39(i,a,b,c,d,e,f,xa,xb,xc,xd) \
        Xupdate(f,xa,xa,xb,xc,xd); \
        (f)+=(e)+K_20_39+ROTATE((a),5)+F_20_39((b),(c),(d)); \
        (b)=ROTATE((b),30);

#define BODY_40_59(i,a,b,c,d,e,f,xa,xb,xc,xd) \
        Xupdate(f,xa,xa,xb,xc,xd); \
        (f)+=(e)+K_40_59+ROTATE((a),5)+F_40_59((b),(c),(d)); \
        (b)=ROTATE((b),30);

#define BODY_60_79(i,a,b,c,d,e,f,xa,xb,xc,xd) \
        Xupdate(f,xa,xa,xb,xc,xd); \
        (f)=xa+(e)+K_60_79+ROTATE((a),5)+F_60_79((b),(c),(d)); \
        (b)=ROTATE((b),30);


#define ROTATE(a,n)  ({ register unsigned int ret;   \
                                asm (                   \
                                "roll %1,%0"            \
                                : "=r"(ret)             \
                                : "I"(n), "0"(a)        \
                                : "cc");                \
                           ret;                         \
                        })

#define BE_FETCH32(a)        ({ register unsigned int l=(a);\
                                asm (                   \
                                "bswapl %0"             \
                                : "=r"(l) : "0"(l));    \
                          l;                            \
                        })


#define REVERSE_FETCH32(a,l)    (                                       \
                l=*(const HASH_LONG *)(a),                              \
                ((ROTATE(l,8)&0x00FF00FF)|(ROTATE((l&0x00FF00FF),24)))  \


#define X(i)   XX##i

void sha1_block_host_order (SHA_CTX *c, const void *d, int num)
        {
        const SHA_LONG *W=d;
        register unsigned long A,B,C,D,E,T;
//#ifndef MD32_XARRAY
        unsigned long     XX0, XX1, XX2, XX3, XX4, XX5, XX6, XX7,
                                XX8, XX9,XX10,XX11,XX12,XX13,XX14,XX15;
//#else
//        SHA_LONG        XX[16];
//#endif

        A=c->h0;
        B=c->h1;
        C=c->h2;
        D=c->h3;
        E=c->h4;

        for (;;)
                {
        BODY_00_15( 0,A,B,C,D,E,T,W[ 0]);
        BODY_00_15( 1,T,A,B,C,D,E,W[ 1]);
        BODY_00_15( 2,E,T,A,B,C,D,W[ 2]);
        BODY_00_15( 3,D,E,T,A,B,C,W[ 3]);
        BODY_00_15( 4,C,D,E,T,A,B,W[ 4]);
        BODY_00_15( 5,B,C,D,E,T,A,W[ 5]);
        BODY_00_15( 6,A,B,C,D,E,T,W[ 6]);
        BODY_00_15( 7,T,A,B,C,D,E,W[ 7]);
        BODY_00_15( 8,E,T,A,B,C,D,W[ 8]);
        BODY_00_15( 9,D,E,T,A,B,C,W[ 9]);
        BODY_00_15(10,C,D,E,T,A,B,W[10]);
        BODY_00_15(11,B,C,D,E,T,A,W[11]);
        BODY_00_15(12,A,B,C,D,E,T,W[12]);
        BODY_00_15(13,T,A,B,C,D,E,W[13]);
        BODY_00_15(14,E,T,A,B,C,D,W[14]);
        BODY_00_15(15,D,E,T,A,B,C,W[15]);

        BODY_16_19(16,C,D,E,T,A,B,X( 0),W[ 0],W[ 2],W[ 8],W[13]);
        BODY_16_19(17,B,C,D,E,T,A,X( 1),W[ 1],W[ 3],W[ 9],W[14]);
        BODY_16_19(18,A,B,C,D,E,T,X( 2),W[ 2],W[ 4],W[10],W[15]);
        BODY_16_19(19,T,A,B,C,D,E,X( 3),W[ 3],W[ 5],W[11],X( 0));

        BODY_20_31(20,E,T,A,B,C,D,X( 4),W[ 4],W[ 6],W[12],X( 1));
        BODY_20_31(21,D,E,T,A,B,C,X( 5),W[ 5],W[ 7],W[13],X( 2));
        BODY_20_31(22,C,D,E,T,A,B,X( 6),W[ 6],W[ 8],W[14],X( 3));
        BODY_20_31(23,B,C,D,E,T,A,X( 7),W[ 7],W[ 9],W[15],X( 4));
        BODY_20_31(24,A,B,C,D,E,T,X( 8),W[ 8],W[10],X( 0),X( 5));
        BODY_20_31(25,T,A,B,C,D,E,X( 9),W[ 9],W[11],X( 1),X( 6));
        BODY_20_31(26,E,T,A,B,C,D,X(10),W[10],W[12],X( 2),X( 7));
        BODY_20_31(27,D,E,T,A,B,C,X(11),W[11],W[13],X( 3),X( 8));
        BODY_20_31(28,C,D,E,T,A,B,X(12),W[12],W[14],X( 4),X( 9));
        BODY_20_31(29,B,C,D,E,T,A,X(13),W[13],W[15],X( 5),X(10));
        BODY_20_31(30,A,B,C,D,E,T,X(14),W[14],X( 0),X( 6),X(11));
        BODY_20_31(31,T,A,B,C,D,E,X(15),W[15],X( 1),X( 7),X(12));

        BODY_32_39(32,E,T,A,B,C,D,X( 0),X( 2),X( 8),X(13));
        BODY_32_39(33,D,E,T,A,B,C,X( 1),X( 3),X( 9),X(14));
        BODY_32_39(34,C,D,E,T,A,B,X( 2),X( 4),X(10),X(15));
        BODY_32_39(35,B,C,D,E,T,A,X( 3),X( 5),X(11),X( 0));
        BODY_32_39(36,A,B,C,D,E,T,X( 4),X( 6),X(12),X( 1));
        BODY_32_39(37,T,A,B,C,D,E,X( 5),X( 7),X(13),X( 2));
        BODY_32_39(38,E,T,A,B,C,D,X( 6),X( 8),X(14),X( 3));
        BODY_32_39(39,D,E,T,A,B,C,X( 7),X( 9),X(15),X( 4));

        BODY_40_59(40,C,D,E,T,A,B,X( 8),X(10),X( 0),X( 5));
        BODY_40_59(41,B,C,D,E,T,A,X( 9),X(11),X( 1),X( 6));
        BODY_40_59(42,A,B,C,D,E,T,X(10),X(12),X( 2),X( 7));
        BODY_40_59(43,T,A,B,C,D,E,X(11),X(13),X( 3),X( 8));
        BODY_40_59(44,E,T,A,B,C,D,X(12),X(14),X( 4),X( 9));
        BODY_40_59(45,D,E,T,A,B,C,X(13),X(15),X( 5),X(10));
        BODY_40_59(46,C,D,E,T,A,B,X(14),X( 0),X( 6),X(11));
        BODY_40_59(47,B,C,D,E,T,A,X(15),X( 1),X( 7),X(12));
        BODY_40_59(48,A,B,C,D,E,T,X( 0),X( 2),X( 8),X(13));
        BODY_40_59(49,T,A,B,C,D,E,X( 1),X( 3),X( 9),X(14));
        BODY_40_59(50,E,T,A,B,C,D,X( 2),X( 4),X(10),X(15));
        BODY_40_59(51,D,E,T,A,B,C,X( 3),X( 5),X(11),X( 0));
        BODY_40_59(52,C,D,E,T,A,B,X( 4),X( 6),X(12),X( 1));
        BODY_40_59(53,B,C,D,E,T,A,X( 5),X( 7),X(13),X( 2));
        BODY_40_59(54,A,B,C,D,E,T,X( 6),X( 8),X(14),X( 3));
        BODY_40_59(55,T,A,B,C,D,E,X( 7),X( 9),X(15),X( 4));
        BODY_40_59(56,E,T,A,B,C,D,X( 8),X(10),X( 0),X( 5));
        BODY_40_59(57,D,E,T,A,B,C,X( 9),X(11),X( 1),X( 6));
        BODY_40_59(58,C,D,E,T,A,B,X(10),X(12),X( 2),X( 7));
        BODY_40_59(59,B,C,D,E,T,A,X(11),X(13),X( 3),X( 8));

        BODY_60_79(60,A,B,C,D,E,T,X(12),X(14),X( 4),X( 9));
        BODY_60_79(61,T,A,B,C,D,E,X(13),X(15),X( 5),X(10));
        BODY_60_79(62,E,T,A,B,C,D,X(14),X( 0),X( 6),X(11));
        BODY_60_79(63,D,E,T,A,B,C,X(15),X( 1),X( 7),X(12));
        BODY_60_79(64,C,D,E,T,A,B,X( 0),X( 2),X( 8),X(13));
        BODY_60_79(65,B,C,D,E,T,A,X( 1),X( 3),X( 9),X(14));
        BODY_60_79(66,A,B,C,D,E,T,X( 2),X( 4),X(10),X(15));
        BODY_60_79(67,T,A,B,C,D,E,X( 3),X( 5),X(11),X( 0));
        BODY_60_79(68,E,T,A,B,C,D,X( 4),X( 6),X(12),X( 1));
        BODY_60_79(69,D,E,T,A,B,C,X( 5),X( 7),X(13),X( 2));
        BODY_60_79(70,C,D,E,T,A,B,X( 6),X( 8),X(14),X( 3));

        BODY_60_79(71,B,C,D,E,T,A,X( 7),X( 9),X(15),X( 4));
        BODY_60_79(72,A,B,C,D,E,T,X( 8),X(10),X( 0),X( 5));
        BODY_60_79(73,T,A,B,C,D,E,X( 9),X(11),X( 1),X( 6));
        BODY_60_79(74,E,T,A,B,C,D,X(10),X(12),X( 2),X( 7));
        BODY_60_79(75,D,E,T,A,B,C,X(11),X(13),X( 3),X( 8));
        BODY_60_79(76,C,D,E,T,A,B,X(12),X(14),X( 4),X( 9));
        BODY_60_79(77,B,C,D,E,T,A,X(13),X(15),X( 5),X(10));
        BODY_60_79(78,A,B,C,D,E,T,X(14),X( 0),X( 6),X(11));
        BODY_60_79(79,T,A,B,C,D,E,X(15),X( 1),X( 7),X(12));

        c->h0=(c->h0+E)&0xffffffffL;
        c->h1=(c->h1+T)&0xffffffffL;
        c->h2=(c->h2+A)&0xffffffffL;
        c->h3=(c->h3+B)&0xffffffffL;
        c->h4=(c->h4+C)&0xffffffffL;

        if (--num <= 0) break;

        A=c->h0;
        B=c->h1;
        C=c->h2;
        D=c->h3;
        E=c->h4;

        W+=SHA_LBLOCK;
                }
        }


void sha1_block_data_order (SHA_CTX *c, const void *p, int num)
        {
        const unsigned char *data=p;
        register unsigned long A,B,C,D,E,T,l;
//#ifndef MD32_XARRAY
        unsigned long     XX0, XX1, XX2, XX3, XX4, XX5, XX6, XX7,
                                XX8, XX9,XX10,XX11,XX12,XX13,XX14,XX15;
//#else
//        SHA_LONG        XX[16];
//#endif

        A=c->h0;
        B=c->h1;
        C=c->h2;
        D=c->h3;
        E=c->h4;

        for (;;)
                {

        HOST_c2l(data,l); X( 0)=l;              HOST_c2l(data,l); X( 1)=l;
        BODY_00_15( 0,A,B,C,D,E,T,X( 0));       HOST_c2l(data,l); X( 2)=l;
        BODY_00_15( 1,T,A,B,C,D,E,X( 1));       HOST_c2l(data,l); X( 3)=l;
        BODY_00_15( 2,E,T,A,B,C,D,X( 2));       HOST_c2l(data,l); X( 4)=l;
        BODY_00_15( 3,D,E,T,A,B,C,X( 3));       HOST_c2l(data,l); X( 5)=l;
        BODY_00_15( 4,C,D,E,T,A,B,X( 4));       HOST_c2l(data,l); X( 6)=l;
        BODY_00_15( 5,B,C,D,E,T,A,X( 5));       HOST_c2l(data,l); X( 7)=l;
        BODY_00_15( 6,A,B,C,D,E,T,X( 6));       HOST_c2l(data,l); X( 8)=l;
        BODY_00_15( 7,T,A,B,C,D,E,X( 7));       HOST_c2l(data,l); X( 9)=l;
        BODY_00_15( 8,E,T,A,B,C,D,X( 8));       HOST_c2l(data,l); X(10)=l;
        BODY_00_15( 9,D,E,T,A,B,C,X( 9));       HOST_c2l(data,l); X(11)=l;
        BODY_00_15(10,C,D,E,T,A,B,X(10));       HOST_c2l(data,l); X(12)=l;
        BODY_00_15(11,B,C,D,E,T,A,X(11));       HOST_c2l(data,l); X(13)=l;
        BODY_00_15(12,A,B,C,D,E,T,X(12));       HOST_c2l(data,l); X(14)=l;
        BODY_00_15(13,T,A,B,C,D,E,X(13));       HOST_c2l(data,l); X(15)=l;
        BODY_00_15(14,E,T,A,B,C,D,X(14));
        BODY_00_15(15,D,E,T,A,B,C,X(15));

        BODY_16_19(16,C,D,E,T,A,B,X( 0),X( 0),X( 2),X( 8),X(13));
        BODY_16_19(17,B,C,D,E,T,A,X( 1),X( 1),X( 3),X( 9),X(14));
        BODY_16_19(18,A,B,C,D,E,T,X( 2),X( 2),X( 4),X(10),X(15));
        BODY_16_19(19,T,A,B,C,D,E,X( 3),X( 3),X( 5),X(11),X( 0));

        BODY_20_31(20,E,T,A,B,C,D,X( 4),X( 4),X( 6),X(12),X( 1));
        BODY_20_31(21,D,E,T,A,B,C,X( 5),X( 5),X( 7),X(13),X( 2));
        BODY_20_31(22,C,D,E,T,A,B,X( 6),X( 6),X( 8),X(14),X( 3));
        BODY_20_31(23,B,C,D,E,T,A,X( 7),X( 7),X( 9),X(15),X( 4));
        BODY_20_31(24,A,B,C,D,E,T,X( 8),X( 8),X(10),X( 0),X( 5));
        BODY_20_31(25,T,A,B,C,D,E,X( 9),X( 9),X(11),X( 1),X( 6));
        BODY_20_31(26,E,T,A,B,C,D,X(10),X(10),X(12),X( 2),X( 7));
        BODY_20_31(27,D,E,T,A,B,C,X(11),X(11),X(13),X( 3),X( 8));
        BODY_20_31(28,C,D,E,T,A,B,X(12),X(12),X(14),X( 4),X( 9));
        BODY_20_31(29,B,C,D,E,T,A,X(13),X(13),X(15),X( 5),X(10));
        BODY_20_31(30,A,B,C,D,E,T,X(14),X(14),X( 0),X( 6),X(11));
        BODY_20_31(31,T,A,B,C,D,E,X(15),X(15),X( 1),X( 7),X(12));

        BODY_32_39(32,E,T,A,B,C,D,X( 0),X( 2),X( 8),X(13));
        BODY_32_39(33,D,E,T,A,B,C,X( 1),X( 3),X( 9),X(14));
        BODY_32_39(34,C,D,E,T,A,B,X( 2),X( 4),X(10),X(15));
        BODY_32_39(35,B,C,D,E,T,A,X( 3),X( 5),X(11),X( 0));
        BODY_32_39(36,A,B,C,D,E,T,X( 4),X( 6),X(12),X( 1));
        BODY_32_39(37,T,A,B,C,D,E,X( 5),X( 7),X(13),X( 2));
        BODY_32_39(38,E,T,A,B,C,D,X( 6),X( 8),X(14),X( 3));
        BODY_32_39(39,D,E,T,A,B,C,X( 7),X( 9),X(15),X( 4));

        BODY_40_59(40,C,D,E,T,A,B,X( 8),X(10),X( 0),X( 5));
        BODY_40_59(41,B,C,D,E,T,A,X( 9),X(11),X( 1),X( 6));
        BODY_40_59(42,A,B,C,D,E,T,X(10),X(12),X( 2),X( 7));
        BODY_40_59(43,T,A,B,C,D,E,X(11),X(13),X( 3),X( 8));
        BODY_40_59(44,E,T,A,B,C,D,X(12),X(14),X( 4),X( 9));
        BODY_40_59(45,D,E,T,A,B,C,X(13),X(15),X( 5),X(10));
        BODY_40_59(46,C,D,E,T,A,B,X(14),X( 0),X( 6),X(11));
        BODY_40_59(47,B,C,D,E,T,A,X(15),X( 1),X( 7),X(12));
        BODY_40_59(48,A,B,C,D,E,T,X( 0),X( 2),X( 8),X(13));
        BODY_40_59(49,T,A,B,C,D,E,X( 1),X( 3),X( 9),X(14));
        BODY_40_59(50,E,T,A,B,C,D,X( 2),X( 4),X(10),X(15));
        BODY_40_59(51,D,E,T,A,B,C,X( 3),X( 5),X(11),X( 0));
        BODY_40_59(52,C,D,E,T,A,B,X( 4),X( 6),X(12),X( 1));
        BODY_40_59(53,B,C,D,E,T,A,X( 5),X( 7),X(13),X( 2));
        BODY_40_59(54,A,B,C,D,E,T,X( 6),X( 8),X(14),X( 3));
        BODY_40_59(55,T,A,B,C,D,E,X( 7),X( 9),X(15),X( 4));
        BODY_40_59(56,E,T,A,B,C,D,X( 8),X(10),X( 0),X( 5));
        BODY_40_59(57,D,E,T,A,B,C,X( 9),X(11),X( 1),X( 6));
        BODY_40_59(58,C,D,E,T,A,B,X(10),X(12),X( 2),X( 7));
        BODY_40_59(59,B,C,D,E,T,A,X(11),X(13),X( 3),X( 8));

        BODY_60_79(60,A,B,C,D,E,T,X(12),X(14),X( 4),X( 9));
        BODY_60_79(61,T,A,B,C,D,E,X(13),X(15),X( 5),X(10));
        BODY_60_79(62,E,T,A,B,C,D,X(14),X( 0),X( 6),X(11));
        BODY_60_79(63,D,E,T,A,B,C,X(15),X( 1),X( 7),X(12));
        BODY_60_79(64,C,D,E,T,A,B,X( 0),X( 2),X( 8),X(13));
        BODY_60_79(65,B,C,D,E,T,A,X( 1),X( 3),X( 9),X(14));
        BODY_60_79(66,A,B,C,D,E,T,X( 2),X( 4),X(10),X(15));
        BODY_60_79(67,T,A,B,C,D,E,X( 3),X( 5),X(11),X( 0));
        BODY_60_79(68,E,T,A,B,C,D,X( 4),X( 6),X(12),X( 1));
        BODY_60_79(69,D,E,T,A,B,C,X( 5),X( 7),X(13),X( 2));
        BODY_60_79(70,C,D,E,T,A,B,X( 6),X( 8),X(14),X( 3));
        BODY_60_79(71,B,C,D,E,T,A,X( 7),X( 9),X(15),X( 4));
        BODY_60_79(72,A,B,C,D,E,T,X( 8),X(10),X( 0),X( 5));
        BODY_60_79(73,T,A,B,C,D,E,X( 9),X(11),X( 1),X( 6));
        BODY_60_79(74,E,T,A,B,C,D,X(10),X(12),X( 2),X( 7));
        BODY_60_79(75,D,E,T,A,B,C,X(11),X(13),X( 3),X( 8));
        BODY_60_79(76,C,D,E,T,A,B,X(12),X(14),X( 4),X( 9));
        BODY_60_79(77,B,C,D,E,T,A,X(13),X(15),X( 5),X(10));
        BODY_60_79(78,A,B,C,D,E,T,X(14),X( 0),X( 6),X(11));
        BODY_60_79(79,T,A,B,C,D,E,X(15),X( 1),X( 7),X(12));

        c->h0=(c->h0+E)&0xffffffffL;
        c->h1=(c->h1+T)&0xffffffffL;
        c->h2=(c->h2+A)&0xffffffffL;
        c->h3=(c->h3+B)&0xffffffffL;
        c->h4=(c->h4+C)&0xffffffffL;

        if (--num <= 0) break;

        A=c->h0;
        B=c->h1;
        C=c->h2;
        D=c->h3;
        E=c->h4;

                }
        }


int SHA1_Init (SHA_CTX *c)
        {
        c->h0=0x67452301UL;
        c->h1=0xefcdab89UL;
        c->h2=0x98badcfeUL;
        c->h3=0x10325476UL;
        c->h4=0xc3d2e1f0UL;
        c->Nl=0;
        c->Nh=0;
        c->num=0;
        return 1;
        }

int SHA1_Update (SHA_CTX *c, const void *data_, unsigned long len)
        {
        const unsigned char *data=data_;
        register SHA_LONG * p;
        register unsigned long l;
        int sw,sc,ew,ec;

        if (len==0) return 1;

        l=(c->Nl+(len<<3))&0xffffffffL;
        /* 95-05-24 eay Fixed a bug with the overflow handling, thanks to
         * Wei Dai <weidai@eskimo.com> for pointing it out. */
        if (l < c->Nl) /* overflow */
                c->Nh++;
        c->Nh+=(len>>29);
        c->Nl=l;

        if (c->num != 0)
                {
                p=c->data;
                sw=c->num>>2;
                sc=c->num&0x03;

                if ((c->num+len) >= SHA_CBLOCK)
                        {
                        l=p[sw]; HOST_p_c2l(data,l,sc); p[sw++]=l;
                        for (; sw<SHA_LBLOCK; sw++)
                                {
                                HOST_c2l(data,l); p[sw]=l;
                                }
                        sha1_block_host_order (c,p,1);
                        len-=(SHA_CBLOCK-c->num);
                        c->num=0;
                        /* drop through and do the rest */
                        }
                else
                        {
                        c->num+=len;
                        if ((sc+len) < 4) /* ugly, add char's to a word */
                                {
                                l=p[sw]; HOST_p_c2l_p(data,l,sc,len); p[sw]=l;
                                }
                        else
                                {
                                ew=(c->num>>2);
                                ec=(c->num&0x03);
                                if (sc)
                                        l=p[sw];
                                HOST_p_c2l(data,l,sc);
                                p[sw++]=l;
                                for (; sw < ew; sw++)
                                        {
                                        HOST_c2l(data,l); p[sw]=l;
                                        }
                                if (ec)
                                        {
                                        HOST_c2l_p(data,l,ec); p[sw]=l;
                                        }
                                }
                        return 1;
                        }
                }

        sw=len/SHA_CBLOCK;
        if (sw > 0)
                {
                /*
                 * Note that HASH_BLOCK_DATA_ORDER_ALIGNED gets defined
                 * only if sizeof(HASH_LONG)==4.
                 */
                if ((((unsigned long)data)%4) == 0)
                        {
                        /* data is properly aligned so that we can cast it: */
                        sha1_block_host_order (c,(SHA_LONG *)data,sw);
                        sw*=SHA_CBLOCK;
                        data+=sw;
                        len-=sw;
                        }
                else
                  {
                  sha1_block_data_order(c,data,sw);
                  sw*=SHA_CBLOCK;
                  data+=sw;
                  len-=sw;
                  }
                }

        if (len!=0)
                {
                p = c->data;
                c->num = len;
                ew=len>>2;      /* words to copy */
                ec=len&0x03;
                for (; ew; ew--,p++)
                        {
                        HOST_c2l(data,l); *p=l;
                        }
                HOST_c2l_p(data,l,ec);
                *p=l;
                }
        return 1;
        }

int SHA1_Final (unsigned char *md, SHA_CTX *c)
        {
        register SHA_LONG *p;
        register unsigned long l;
        register int i,j;
        static const unsigned char end[4]={0x80,0x00,0x00,0x00};
        const unsigned char *cp=end;

        /* c->num should definitly have room for at least one more byte. */
        p=c->data;
        i=c->num>>2;
        j=c->num&0x03;

        l = (j==0) ? 0 : p[i];

        HOST_p_c2l(cp,l,j); p[i++]=l; /* i is the next 'undefined word' */

        if (i>(SHA_LBLOCK-2)) /* save room for Nl and Nh */
                {
                if (i<SHA_LBLOCK) p[i]=0;
                sha1_block_host_order (c,p,1);
                i=0;
                }
        for (; i<(SHA_LBLOCK-2); i++)
                p[i]=0;

        p[SHA_LBLOCK-2]=c->Nh;
        p[SHA_LBLOCK-1]=c->Nl;
        sha1_block_host_order (c,p,1);

        SHA_MAKE_STRING(c,md);

        c->num=0;
        /* clear stuff, HASH_BLOCK may be leaving some stuff on the stack
         * but I'm not worried :-)
         */
        OPENSSL_cleanse((void *)c,sizeof(SHA_CTX));
        return 1;
        }


void SHA1_Transform (SHA_CTX *c, const unsigned char *data)
        {
        sha1_block_data_order (c,data,1);
        }

unsigned char *SHA(const unsigned char *d, unsigned long n, unsigned char *md)
        {
        SHA_CTX c;
        static unsigned char m[SHA_DIGEST_LENGTH];

        if (md == NULL) md=m;
        SHA1_Init(&c);
        SHA1_Update(&c,d,n);
        SHA1_Final(md,&c);
        OPENSSL_cleanse(&c, sizeof(c));
        return(md);
        }

