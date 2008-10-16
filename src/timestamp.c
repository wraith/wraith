#define _XOPEN_SOURCE
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
  if (argc == 3) { //2006-01-01 00:00:00"
    char *Time = NULL;
    const char *Format = "%Y-%m-%d %H:%M:%S";
    struct tm ts;
    size_t siz = strlen(argv[1]) + strlen(argv[2]) + 1 + 1;
    time_t tim = 0;

    Time = calloc(1, siz);
#ifdef __openbsd__
    snprintf(Time, siz, "%s %s", argv[1], argv[2]);
#else
    sprintf(Time, "%s %s", argv[1], argv[2]);
#endif
    strptime(Time, Format, &ts);
    free(Time);
    tim = timegm(&ts);
    printf("%ld\n", tim);
  } else if (argc == 2) { //18734563281
    const time_t tm = atol(argv[1]);
    char s[11] = "";

    strftime(s, 11, "%m.%d.%Y", localtime(&tm));
    printf("%s\n", s);
  } else {
    return 1;
  }

  return 0;
}
