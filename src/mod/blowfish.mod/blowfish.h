#ifndef _EGG_MOD_BLOWFISH_BLOWFISH_H
#define _EGG_MOD_BLOWFISH_BLOWFISH_H
#define MAXKEYBYTES	56
#define bf_N	16
#define noErr 0
#define DATAERROR	-1
#define KEYBYTES 8
union aword
{
  u_32bit_t word;
  u_8bit_t byte[4];
  struct
  {
#ifdef WORDS_BIGENDIAN
    unsigned int byte0:8;
    unsigned int byte1:8;
    unsigned int byte2:8;
    unsigned int byte3:8;
#else
    unsigned int byte3:8;
    unsigned int byte2:8;
    unsigned int byte1:8;
    unsigned int byte0:8;
#endif
  } w;
};
#endif
