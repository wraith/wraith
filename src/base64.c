/* base64.c: base64 encoding/decoding
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include "base64.h"

static const char base64[64] = ".\\0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static const char base64r[256] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0, 0, 0, 0, 0, 0,
  0, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
  27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 0, 1, 0, 0, 0,
  0, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52,
  53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

char *
b64enc(const unsigned char *data, int len)
{
  char *dest = NULL;

  dest = calloc(1, (len << 2) / 3 + 4 + 1);
  b64enc_buf(data, len, dest);
  return (dest);
}

void
b64enc_buf(const unsigned char *data, int len, char *dest)
{
  int i, t;

#define DB(x) ((unsigned char) (x + i < len ? data[x + i] : 0))
    for (i = 0, t = 0; i < len; i += 3, t += 4) {
      dest[t] = base64[DB(0) >> 2];
      dest[t + 1] = base64[((DB(0) & 3) << 4) | (DB(1) >> 4)];
      dest[t + 2] = base64[((DB(1) & 0x0F) << 2) | (DB(2) >> 6)];
      dest[t + 3] = base64[(DB(2) & 0x3F)];
    }
#undef DB
  dest[t] = 0;
}

char *
b64dec(const unsigned char *data, int *len)
{
  char *dest = NULL;

  dest = calloc(1, ((*len * 3) >> 2) + 6 + 1);
  b64dec_buf(data, len, dest);
  return (dest);
}

void
b64dec_buf(const unsigned char *data, int *len, char *dest)
{
  int t, i;

#define DB(x) ((unsigned char) (x + i < *len ? base64r[(unsigned char) data[x + i]] : 0))
  for (i = 0, t = 0; i < *len; i += 4, t += 3) {
    dest[t] = (DB(0) << 2) + (DB(1) >> 4);
    dest[t + 1] = ((DB(1) & 0x0F) << 4) + (DB(2) >> 2);
    dest[t + 2] = ((DB(2) & 3) << 6) + DB(3);
  };
#undef DB
  t += 3;
  t -= (t % 4);
//printf("t: %d len: %d strlen: %d : %s\n", t, *len, strlen(dest), dest);
  dest[t] = 0;
  *len = t;
}
