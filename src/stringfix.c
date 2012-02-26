/* stringfix.c:
 *    handles STR("text") for garbling of strings..
 */


/*  dprintf(idx, STR("A"), STR(""), STR("1" ), STR("")); */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define WTF 1524
int help = 0;

void garble(char **inptr, char **outptr)
{
  char *in = *inptr, *out = NULL, *p = NULL, obuf[WTF] = "";
  size_t chars = 0;
  unsigned char x = 0;

  p = in + 5;
  if (*p == '"') {
    sprintf((*outptr), "\"\"");
    *inptr += 7;
    *outptr += 2;
    return;
  }
  while ((*p) && !((*p == '"') && (*(p - 1) != '\\')))
    p++;
  if ((*p == '"') && (*(p - 1) != '\\') && (*(p + 1) == ')')) {
    char *c;

    c = in + 5;
    out = obuf;
    x = 0xFF;
    while (c < p) {
      if (*c == '\\') {
	unsigned char e;

	c++;
	if (*c == 'a')
	  e = 7;
	else if (*c == 'b')
	  e = 8;
	else if (*c == 't')
	  e = 9;
	else if (*c == 'n')
	  e = 10;
	else if (*c == 'v')
	  e = 11;
	else if (*c == 'f')
	  e = 12;
	else if (*c == 'r')
	  e = 13;
	else if ((*c >= '0') && (*c <= '7')) {
	  int cnt = 0;

	  e = 0;
	  while ((*c >= '0') && (*c <= '7') && (cnt < 3)) {
	    e = (e * 8) + (*c - '0');
	    cnt++;
	    c++;
	  }
	  c--;
	} else
	  e = *c;
	sprintf(out, "\\%03o", e ^ x);
	chars++;
	x = e;
	c++;
      } else {
	sprintf(out, "\\%03o", ((unsigned char) *c) ^ x);
	chars++;
	x = *c;
	c++;
      }
      out += 4;
      *out = 0;
    }

    if (help)
      sprintf(*outptr, "%zu, \"%s\"", chars, obuf);
    else
      sprintf(*outptr, "degarble(%zu, \"%s\")", chars, obuf);
    *outptr += strlen(*outptr);
    in = p + 2;
  } else {
    strncpy((*outptr), in, (p - in) + 1);
    *outptr += strlen(*outptr);
    in = p + 1;
  }
  *inptr = in;
}

void processline(char *line)
{
  char tmpin[WTF] = "", tmpout[WTF] = "", *in = NULL, *out = NULL;
  size_t outlen = 0;

  strcpy(tmpin, line); 
  memset((char *) &tmpin[strlen(tmpin)], 0, 20);
  in = tmpin;
  out = tmpout;
  if (*in) {
    while (*in) {
      if (!strncmp(in, "STR(\"", 5)) {
	*out = 0;
	garble(&in, &out);
	*out = 0;
      } else
	*out++ = *in++;
    }
    *out = 0;
  } else
    tmpout[0] = 0;
  outlen = strlen(tmpout);

  fwrite(tmpout, outlen, 1, stdout);
}

int main(int argc, char *argv[])
{
  if (argc == 2)
    help = 1;
  char tempBuf[1024] = "";
  while (!feof(stdin)) {
    if (fgets(tempBuf, sizeof(tempBuf), stdin) && !feof(stdin)) {
      processline(tempBuf);
    }
  }

  return 0;
}
