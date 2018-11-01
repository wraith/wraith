/*
 * garble.c -- handles:
 *   garble strings
 *
 */


#include "common.h"
#include "garble.h"
#include "main.h"

#define GARBLE_BUFFERS 40
static unsigned char *garble_buffer[GARBLE_BUFFERS] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

int garble_ptr = (-1);

const char *degarble(int len, const char *g)
{
  unsigned char x = 0;

  garble_ptr++;
  if (garble_ptr == GARBLE_BUFFERS)
    garble_ptr = 0;
  if (garble_buffer[garble_ptr])
    free(garble_buffer[garble_ptr]);
  garble_buffer[garble_ptr] = (unsigned char *) calloc(1, len + 1);
  x = 0xFF;
  for (int i = 0; i < len; i++) {
    garble_buffer[garble_ptr][i] = g[i] ^ x;
    x = garble_buffer[garble_ptr][i];
  }
  garble_buffer[garble_ptr][len] = 0;
  return (const char *) garble_buffer[garble_ptr];
}

/* vim: set sts=2 sw=2 ts=8 et: */
