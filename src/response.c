/*
 * response.c -- handles:
 *
 *   What else?!
 *
 */

#include "common.h"
#include "response.h"
#include "main.h"
#include "responses.h"

static response_t response_totals[RES_TYPES + 1];

void
init_responses()
{
  unsigned int i = 0;
  
  for (i = 0; i <= RES_TYPES; i++)
    response_totals[i] = 0;
}

static void
count_responses(response_t type)
{
  unsigned int total = 0;

  for (total = 0; res[type][total]; total++) 
    ;

  response_totals[type] = total;
  /* printf("Type: %d has %d total responses!\n", type, response_totals[type]); */
}

char *
response(response_t type)
{
  if (!type)
    type = randint(RES_TYPES) + 1;

  /* wait to count the totals until it's used for the first time */
  if (!response_totals[type])
    count_responses(type);

  return res[type][randint(response_totals[type])];
}
