/* This file simply returns number of seconds since epoch in UTC 
 */


#include <time.h>
#include <stdio.h>

int main() {
  time_t now = time(NULL);

  printf("%li\n", mktime(gmtime(&now)));
  return 0;
}

