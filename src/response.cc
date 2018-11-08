/*
 * response.c -- handles:
 *
 *   What else?!
 *
 */

#include "common.h"
#include "response.h"
#include "responses.cc"
#include <bdlib/src/HashTable.h>
#include <bdlib/src/String.h>
#include <vector>

static bd::HashTable<bd::String, std::vector<const char*> > res_map;

void
init_responses()
{
  for (size_t i = 0; i < sizeof(res)/sizeof(res[0]); ++i)
    res_map[res[i].name] = std::vector<const char*>(res[i].res,
        res[i].res + res[i].size);
}

const char *
response(response_t type)
{
  return res_map[type][randint(res_map[type].size())];
}
/* vim: set sts=2 sw=2 ts=8 et: */
