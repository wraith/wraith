/* stringfix.c:
 *    handles STR("text") for garbling of strings..
 */

/*  dprintf(idx, STR("A"), STR(""), STR("1" ), STR("")); */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "conf.h"

char tmp[] = { 0x0, 0x1 };

#ifdef S_GARBLESTRINGS
void garble(char **inptr, char **outptr)
{
  char *in = *inptr,
   *out,
   *p = NULL;
  char obuf[16384];
  int chars = 0;
  unsigned char x;

  obuf[0] = 0;
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
    sprintf(*outptr, "degarble(%d,\"%s\")", chars, obuf);
    *outptr += strlen(*outptr);
    in = p + 2;
  } else {
    strncpy((*outptr), in, (p - in) + 1);
    *outptr += strlen(*outptr);
    in = p + 1;
  }
  *inptr = in;
}

char *outbuf = NULL;

void processline(char *line)
{
  char tmpin[16384],
    tmpout[16384];
  char *in,
   *out;

  strcpy(tmpin, line);
  bzero((char *) &tmpin[strlen(tmpin)], 20);
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

  if (outbuf)
    outbuf = realloc(outbuf, strlen(outbuf) + strlen(tmpout) + 10);
  else {
    outbuf = malloc(strlen(tmpout) + 10);
    outbuf[0] = 0;
  }
  strcat(outbuf, tmpout);
  strcat(outbuf, "\n");
}
#endif

int main(int argc, char *argv[0])
{
#ifdef S_GARBLESTRINGS
  FILE *f;
  char *ln,
   *nln;
  int insize;
  char *buf;

  if (argc != 2)
    exit(1);
  f = fopen(argv[1], "r");
  if (!f)
    exit(1);
  fseek(f, 0, SEEK_END);
  insize = ftell(f);
  fseek(f, 0, SEEK_SET);
  buf = malloc(insize + 1);
  fread(buf, 1, insize, f);
  fclose(f);
  buf[insize] = 0;

  ln = buf;
  while (ln) {
    nln = strchr(ln, '\n');
    if (nln)
      *nln++ = 0;
    processline(ln);
    ln = nln;
  }

  f = fopen(argv[1], "w");
  if (f) {
    fwrite(outbuf, 1, strlen(outbuf), f);
    fclose(f);
  }
  /*  printf(outbuf); */
#endif
  exit(0);
}
