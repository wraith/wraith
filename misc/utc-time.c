/* This file simply returns number of seconds since epoch in UTC 
 */

#include <time.h>
#include <stdio.h>
#include <string.h>

int main() {
  time_t now, nowtm;
  now = time(NULL);
  nowtm = mktime(gmtime(&now));
  printf("%lu\n", nowtm);
  return 0;
}
