/*
 * response.c -- handles:
 *
 *   What else?!
 *
 */

#include "common.h"
#include "response.h"
#include "responses.cc"

const char *
response(response_t type)
{
  return res[type].res[randint(res[type].size)];
}
/* vim: set sts=2 sw=2 ts=8 et: */
