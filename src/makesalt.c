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
    if (r % 4 == 0)
      s[j] = '0' + (random() % 10);
    else if (r % 4 == 1)
      s[j] = 'a' + (random() % 26);
    else if (r % 4 == 2)
      s[j] = 'A' + (random() % 26);
    else
      s[j] = '!' + (random() % 15);

    if (s[j] == 33 || s[j] == 37 || s[j] == 34 || s[j] == 40 || s[j] == 41 || s[j] == 38 || s[j] == 36) //no % ( ) &
      s[j] = 35;
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
  saltlen2 = 32;

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
  fprintf(saltfd,"/* SALT1 is for local files */\n",saltlen1);
  fprintf(saltfd,"#define SALT1 %c%s%c\n",34,randstring(saltlen1),34);
  fprintf(saltfd,"\n");
  fprintf(saltfd,"/* SALT2 is for botlink  */\n",saltlen2);
  fprintf(saltfd,"#define SALT2 %c%s%c\n",34,randstring(saltlen2),34);
  fprintf(saltfd,"\n");
  fclose(saltfd);
  printf("Salt File created.\n");
  exit (0);
}
