/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2014 Bryan Drewery
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/* base64.c: base64 encoding/decoding
 *
 */


#include <stdlib.h>
#include <stdio.h>
#include "base64.h"
#include <bdlib/src/String.h>

static char *b64enc_bd(const unsigned char *data, size_t *len);
static char *b64dec_bd(const unsigned char *data, size_t *len);
static void b64enc_buf(const unsigned char *data, size_t len, char *dest);
static void b64dec_buf(const unsigned char *data, size_t *len, char *dest);
static void b64dec_bd_buf(const unsigned char *data, size_t *len, char *dest);

static const char base64[65] = ".\\0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
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

static const char base64to[256] =
{
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0, 0,
  0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
  15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 62, 0, 63, 0, 0, 0, 26, 27, 28,
  29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
  49, 50, 51, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


int base64_to_int(const char *buf)
{
  int i = 0;

  while (*buf) {
    i = i << 6;
    i += base64to[(int) *buf];
    buf++;
  }
  return i;
}

static const char tobase64array[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789[]";

char *int_to_base64(unsigned int val)
{
  static char buf_base64[12] = "";

  buf_base64[11] = 0;
  if (!val) {
    buf_base64[10] = 'A';
    return buf_base64 + 10;
  }

  int i = 11;

  while (val) {
    i--;
    buf_base64[i] = tobase64array[val & 0x3f];
    val = val >> 6;
  }

  return buf_base64 + i;
}

/* These are all broken */

#define NUM_ASCII_BYTES 3
#define NUM_ENCODED_BYTES 4

char *
b64enc(const unsigned char *data, size_t len)
{
  char *dest = (char *) calloc(1, (len << 2) / 3 + 4 + 1);

  b64enc_buf(data, len, dest);
  return (dest);
}

/**
 * @brief Base64 encode a string
 * @param string The string to encode
 * @return A new, encoded string
 */
bd::String broken_base64Encode(const bd::String& string) {
  size_t len = string.length();
  char *p = b64enc_bd((unsigned char*) string.data(), &len);
  bd::String encoded(p, len);
  free(p);
  return encoded;
}

/**
 * @brief Base64 decode a string
 * @param string The string to decode
 * @return A new, decoded string
 */
bd::String broken_base64Decode(const bd::String& string) {
  size_t len = string.length();
  char *p = b64dec_bd((unsigned char*) string.data(), &len);
  bd::String decoded(p, len);
  free(p);
  return decoded;
}


/* Encode 3 8-bit bytes to 4 6-bit characters */
static void
b64enc_buf(const unsigned char *data, size_t len, char *dest)
{
#define DB(x) ((unsigned char) ((x + i) < len ? data[x + i] : 0))
  size_t t, i;

  /* 4-byte blocks */
  for (i = 0, t = 0; i < len; i += NUM_ASCII_BYTES, t += NUM_ENCODED_BYTES) {
    dest[t] = base64[DB(0) >> 2];
    dest[t + 1] = base64[((DB(0) & 3) << 4) | (DB(1) >> 4)];
    dest[t + 2] = base64[((DB(1) & 0x0F) << 2) | (DB(2) >> 6)];
    dest[t + 3] = base64[(DB(2) & 0x3F)];
  }
#undef DB
  dest[t] = 0;
}


char *
b64dec(const unsigned char *data, size_t *len)
{
  char *dest = (char *) calloc(1, ((*len * 3) >> 2) + 6 + 1);

  b64dec_buf(data, len, dest);
  return (dest);
}

static void
b64dec_buf(const unsigned char *data, size_t *len, char *dest)
{
#define DB(x) ((unsigned char) (x + i < *len ? base64r[(unsigned char) data[x + i]] : 0))
  size_t t, i;

  for (i = 0, t = 0; i < *len; i += 4, t += 3) {
    dest[t] = (DB(0) << 2) + (DB(1) >> 4);
    dest[t + 1] = ((DB(1) & 0x0F) << 4) + (DB(2) >> 2);
    dest[t + 2] = ((DB(2) & 3) << 6) + DB(3);
  };
#undef DB
  t += 3;
  t -= (t % 4);
  dest[t] = 0;
  *len = t;
}

/* These are adapated for use with bd::String */

static char *
b64enc_bd(const unsigned char *data, size_t *len)
{
  size_t dlen = (((*len + (NUM_ASCII_BYTES - 1)) / NUM_ASCII_BYTES) * NUM_ENCODED_BYTES);
  char *dest = (char *) calloc(1, dlen + 1);
  b64enc_buf(data, *len, dest);
  *len = dlen;
  return (dest);
}


/* Decode 4 6-bit characters to 3 8-bit bytes */
static void
b64dec_bd_buf(const unsigned char *data, size_t *len, char *dest)
{
#define DB(x) ((unsigned char) (x + i < *len ? base64r[(unsigned char) data[x + i]] : 0))
  size_t t, i;
  int pads = 0;

  for (i = 0, t = 0; i < *len; i += NUM_ENCODED_BYTES, t += NUM_ASCII_BYTES) {

    dest[t] = (DB(0) << 2) + (DB(1) >> 4);
    dest[t + 1] = ((DB(1) & 0x0F) << 4) + (DB(2) >> 2);
    dest[t + 2] = ((DB(2) & 3) << 6) + DB(3);
    /* Check for nulls (padding) - the >= check is because binary data might contain VALID NULLS */
    if ((i + NUM_ENCODED_BYTES) >= *len) {
      if (dest[t] == 0) ++pads;
      if (dest[t+1] == 0) ++pads;
      if (dest[t+2] == 0) ++pads;
    }

  };
#undef DB

  *len = t - pads;
  dest[*len] = 0;
}

static char *
b64dec_bd(const unsigned char *data, size_t *len)
{
  char *dest = (char *) calloc(1, ((*len * 3) >> 2) + 6 + 1);
  b64dec_bd_buf(data, len, dest);
  return dest;
}
/* vim: set sts=2 sw=2 ts=8 et: */
