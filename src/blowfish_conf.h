#ifndef _H_BLOWFISH
#define _H_BLOWFISH
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#define oMAXKEYBYTES 56
#define obf_N 16
#define noErr 0
#define DATAERROR -1
#define KEYBYTES 8
#define UBYTE_08bits unsigned char
#define UWORD_16bits unsigned short
#define nmalloc(x) n_malloc((x),__FILE__,__LINE__)
#define SIZEOF_INT 4
#define SIZEOF_LONG 4
#if SIZEOF_INT==4
#define UWORD_32bits unsigned int
#else
#if SIZEOF_LONG==4
#define UWORD_32bits unsigned long
#endif
#endif
#ifdef WORDS_BIGENDIAN
union aword
{
  UWORD_32bits word;
  UBYTE_08bits byte[4];
  struct
  {
    unsigned int byte0:8;
    unsigned int byte1:8;
    unsigned int byte2:8;
    unsigned int byte3:8;
  } w;
};
#endif
#ifndef WORDS_BIGENDIAN
union aword
{
  UWORD_32bits word;
  UBYTE_08bits byte[4];
  struct
  {
    unsigned int byte3:8;
    unsigned int byte2:8;
    unsigned int byte1:8;
    unsigned int byte0:8;
  } w;
};
#endif
#endif
