/* crypto/md5/md5_locl.h */

#include <stdlib.h>
#include <string.h>
#include "md5.h"

#ifndef MD5_LONG_LOG2
#define MD5_LONG_LOG2 2 /* default to 32 bits */
#endif

#ifdef MD5_ASM
# if defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(__INTEL__)
#  define md5_block_host_order md5_block_asm_host_order
# elif defined(__sparc)
   void md5_block_asm_data_order_aligned (MD5_CTX *c, const MD5_LONG *p,int num);
#  define HASH_BLOCK_DATA_ORDER_ALIGNED md5_block_asm_data_order_aligned
# endif
#endif

void md5_block_host_order (MD5_CTX *c, const void *p,int num);
void md5_block_data_order (MD5_CTX *c, const void *p,int num);

#if defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(__INTEL__)
/*
 * *_block_host_order is expected to handle aligned data while
 * *_block_data_order - unaligned. As algorithm and host (x86)
 * are in this case of the same "endianness" these two are
 * otherwise indistinguishable. But normally you don't want to
 * call the same function because unaligned access in places
 * where alignment is expected is usually a "Bad Thing". Indeed,
 * on RISCs you get punished with BUS ERROR signal or *severe*
 * performance degradation. Intel CPUs are in turn perfectly
 * capable of loading unaligned data without such drastic side
 * effect. Yes, they say it's slower than aligned load, but no
 * exception is generated and therefore performance degradation
 * is *incomparable* with RISCs. What we should weight here is
 * costs of unaligned access against costs of aligning data.
 * According to my measurements allowing unaligned access results
 * in ~9% performance improvement on Pentium II operating at
 * 266MHz. I won't be surprised if the difference will be higher
 * on faster systems:-)
 *
 *				<appro@fy.chalmers.se>
 */
#define md5_block_data_order md5_block_host_order
#endif

#define DATA_ORDER_IS_LITTLE_ENDIAN

#define HASH_LONG		MD5_LONG
#define HASH_LONG_LOG2		MD5_LONG_LOG2
#define HASH_CTX		MD5_CTX
#define HASH_CBLOCK		MD5_CBLOCK
#define HASH_LBLOCK		MD5_LBLOCK
#define HASH_UPDATE		MD5_Update
#define HASH_TRANSFORM		MD5_Transform
#define HASH_FINAL		MD5_Final
#define	HASH_MAKE_STRING(c,s)	do {	\
	unsigned long ll;		\
	ll=(c)->A; HOST_l2c(ll,(s));	\
	ll=(c)->B; HOST_l2c(ll,(s));	\
	ll=(c)->C; HOST_l2c(ll,(s));	\
	ll=(c)->D; HOST_l2c(ll,(s));	\
	} while (0)
#define HASH_BLOCK_HOST_ORDER	md5_block_host_order
#if !defined(L_ENDIAN) || defined(md5_block_data_order)
#define	HASH_BLOCK_DATA_ORDER	md5_block_data_order
/*
 * Little-endians (Intel and Alpha) feel better without this.
 * It looks like memcpy does better job than generic
 * md5_block_data_order on copying-n-aligning input data.
 * But frankly speaking I didn't expect such result on Alpha.
 * On the other hand I've got this with egcs-1.0.2 and if
 * program is compiled with another (better?) compiler it
 * might turn out other way around.
 *
 *				<appro@fy.chalmers.se>
 */
#endif

#include "md32_common.h"

/*
#define	F(x,y,z)	(((x) & (y))  |  ((~(x)) & (z)))
#define	G(x,y,z)	(((x) & (z))  |  ((y) & (~(z))))
*/

/* As pointed out by Wei Dai <weidai@eskimo.com>, the above can be
 * simplified to the code below.  Wei attributes these optimizations
 * to Peter Gutmann's SHS code, and he attributes it to Rich Schroeppel.
 */
#define	F(b,c,d)	((((c) ^ (d)) & (b)) ^ (d))
#define	G(b,c,d)	((((b) ^ (c)) & (d)) ^ (c))
#define	H(b,c,d)	((b) ^ (c) ^ (d))
#define	I(b,c,d)	(((~(d)) | (b)) ^ (c))

#define R0(a,b,c,d,k,s,t) { \
	a+=((k)+(t)+F((b),(c),(d))); \
	a=ROTATE(a,s); \
	a+=b; };\

#define R1(a,b,c,d,k,s,t) { \
	a+=((k)+(t)+G((b),(c),(d))); \
	a=ROTATE(a,s); \
	a+=b; };

#define R2(a,b,c,d,k,s,t) { \
	a+=((k)+(t)+H((b),(c),(d))); \
	a=ROTATE(a,s); \
	a+=b; };

#define R3(a,b,c,d,k,s,t) { \
	a+=((k)+(t)+I((b),(c),(d))); \
	a=ROTATE(a,s); \
	a+=b; };
