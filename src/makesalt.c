/*
 * makesalt.c -- handles:
 * making the salt for the encryption.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

/* Create a string with random letters and digits
 */
char *randstring(int len)
{
  int j, r = 0;
  static char s[100];

  for (j = 0; j < len; j++) {
    r = random();
    if (r % 3 == 0)
      s[j] = '0' + (random() % 10);
    else if (r % 3 == 1)
      s[j] = 'a' + (random() % 26);
    else if (r % 3 == 2)
      s[j] = 'A' + (random() % 26);
  }
  s[len] = '\0';
  return s;
}

int main(void)
{
  FILE *saltfd;
  int saltlen1;
  int saltlen2;
  time_t now = time(NULL);
  srandom(now % (getpid() + getppid()));
  saltlen1 = 32;
  saltlen2 = 16;

  if ((saltfd = fopen("pack/salt.h", "r"))!= NULL) {
    fclose(saltfd);
    printf("Using existent Salt-File\n"); 
    exit(0);
  }
  printf("Creating Salt File\n");
  if ((saltfd = fopen("pack/salt.h", "w")) == NULL) {
    printf("Cannot created Salt-File.. aborting\n");
    exit(1);
  }
  fprintf(saltfd, "#define STR(x) x\n");
  fprintf(saltfd, "/* SALT1 is for local files */\n");
  fprintf(saltfd, "#define SALT1 STR(\"%s\")\n", randstring(saltlen1));
  fprintf(saltfd, "/* SALT2 is for botlink  */\n");
  fprintf(saltfd, "#define SALT2 STR(\"%s\")\n", randstring(saltlen2));
  fclose(saltfd);
  printf("Salt File created.\n");
  exit (0);
}
