/* This file simply returns number of seconds since epoch in UTC 
 */

#include <time.h>
#include <stdio.h>

int main() {
  time_t now = time(NULL);
  now = mktime(gmtime(&now));
  printf("%lu\n", mktime(localtime(&now)));
  printf("%lu\n", gmtime(&now));
  printf("%lu\n", now);
  return 0;
}
