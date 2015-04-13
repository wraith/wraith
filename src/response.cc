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

const char *
r_banned(struct chanset_t *chan)
{
  return response(RES_BANNED);
}
/* vim: set sts=2 sw=2 ts=8 et: */
