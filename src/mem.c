/*
 * mem.c -- handles:
 *   memory allocation and deallocation
 *   garble strings
 *
 */

#include "eggmain.h"


#define STR(x) x

void *my_malloc(int size)
{
  void *x;

  x = (void *) malloc(size);
  if (x == NULL) 
    fatal("Memory allocation failed", 0);
  return x;
}

void *my_realloc(void *ptr, int size)
{
  void *x;

  if (!ptr)
    return my_malloc(size);

  x = (void *) realloc(ptr, size);
  if (x == NULL && size > 0)
    return NULL;
  return x;
}

void my_free(void *ptr)
{
  if (ptr == NULL)
    return;
  free(ptr);
}

#ifdef S_GARBLESTRINGS
#define GARBLE_BUFFERS 40
unsigned char *garble_buffer[GARBLE_BUFFERS] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

int garble_ptr = (-1);

char *degarble(int len, char *g)
{
  int i;
  unsigned char x;

  garble_ptr++;
  if (garble_ptr == GARBLE_BUFFERS)
    garble_ptr = 0;
  if (garble_buffer[garble_ptr])
    free(garble_buffer[garble_ptr]);
  garble_buffer[garble_ptr] = malloc(len + 1);
  x = 0xFF;
  for (i = 0; i < len; i++) {
    garble_buffer[garble_ptr][i] = g[i] ^ x;
    x = garble_buffer[garble_ptr][i];
  }
  garble_buffer[garble_ptr][len] = 0;
  return (char *) garble_buffer[garble_ptr];
}
#endif /* S_GARBLESTRINGS */

