#include "main.h"
#include "salt.h"
unsigned char *hashdot (unsigned int r);
unsigned int unhashdot (unsigned char *hash);
char crybu[2000];
char *
psycrypt (char *st)
{
  char *pte;
  char *ptt;
  char *pts1, *pts2;
  char *pt;
  char *hpt;
  char hbuf[3];
  int res;
  int slen = 0;
  unsigned int tslt1 = CODE1;
  unsigned int tslt2 = CODE2;
  int p1, p2, p3, p4, p5;
  int erg;
  int de = 0;
  memset (crybu, 0x0, sizeof (crybu));
  pt = crybu;
  pte = pt;
  ptt = st;
  if (*ptt == '+')
    {
      ptt++;
      de = 1;
    }
  else
    {
      *pte++ = '+';
    }
  pts1 = slt1 + SA1;
  pts2 = slt2 + SA2;
  while (*ptt != 0)
    {
      if (slen > 1990)
	break;
      if (tslt1 > 255 || tslt1 < 0)
	tslt1 = CODE1;
      if (tslt2 > 255 || tslt2 < 0)
	tslt2 = CODE2;
      if (*pts1 == 0)
	pts1 = slt1;
      if (*pts2 == 0)
	pts2 = slt2;
      res = 0;
      if (de)
	{
	  hbuf[0] = *ptt++;
	  hbuf[1] = *ptt;
	  hbuf[2] = 0;
	  p1 = unhashdot (hbuf);
	  p2 = *pts1;
	  p3 = tslt1;
	  p4 = *pts2;
	  p5 = tslt2;
	  erg = p1 - p2 - p3 + p4 - p5;
	  *pte = erg;
	  res = erg;
	}
      else
	{
	  p1 = *ptt;
	  p2 = *pts1;
	  p3 = tslt1;
	  p4 = *pts2;
	  p5 = tslt2;
	  res = p1;
	  erg = p1 + p2 + p3 - p4 + p5;
	  hpt = hashdot (erg);
	  *pte++ = hpt[0];
	  slen++;
	  *pte = hpt[1];
	}
      tslt1--;
      res = res / 10;
      tslt2 = tslt2 + res;
      pte++;
      ptt++;
      pts1++;
      pts2++;
      slen = slen + 1;
    }
  *pte = 0;
  return pt;
}

char *
cryptit (char *tocipher)
{
  if (*tocipher == '+')
    return tocipher;
  else
    return psycrypt (tocipher);
}

char *
decryptit (char *todecipher)
{
  if (todecipher[0] == '+')
    return psycrypt (todecipher);
  else
    return todecipher;
}
unsigned char base[] =
  "'`0123456789abcdefghijklmnopqrstuvxyzABCDEFGHIJKLMNOPQRSTUVWXYZ@$=&*-#";
int baselen = 67;
unsigned char xres[3];
unsigned char *
hashdot (unsigned int r)
{
  unsigned int cnt;
  unsigned int hh = 0;
  unsigned int hl = 0;
  cnt = r;
  for (; cnt > 0; cnt--)
    {
      hl++;
      if (hl == baselen)
	{
	  hl = 0;
	  hh++;
	}
    }
  xres[0] = base[hh];
  xres[1] = base[hl];
  xres[2] = 0;
  return xres;
}

int wrong = 0;
unsigned int
unhashdot (unsigned char *hash)
{
  unsigned int lf = baselen;
  unsigned int erg = 0;
  unsigned long ln = 0;
  wrong = 0;
  while (ln < baselen && base[ln] != hash[0])
    {
      ln++;
    }
  if (ln != baselen)
    {
      erg = ln * lf;
    }
  else
    {
      wrong = 1;
    }
  ln = 0;
  while (ln < baselen && base[ln] != hash[1])
    {
      ln++;
    }
  if (ln != baselen)
    {
      erg = erg + ln;
    }
  else
    {
      wrong = 1;
    }
  return erg;
}
extern char netpass[];
int
lfprintf (FILE * f, char *fmt, ...)
{
  va_list va;
  char outbuf[8192];
  char *tptr, *tptr2, *temps1, *temps2;
  va_start (va, fmt);
  vsnprintf (outbuf, sizeof (outbuf), fmt, va);
  tptr2 = outbuf;
  if (strchr (outbuf, '\n'))
    {
      while ((tptr = strchr (outbuf, '\n')))
	{
	  *tptr = 0;
	  Context;
	  temps1 = (char *) encrypt_string (netpass, tptr2);
	  Context;
	  if (fprintf (f, "%s\n", cryptit (temps1)) == EOF)
	    {
	      nfree (temps1);
	      return -1;
	    }
	  nfree (temps1);
	  tptr++;
	  tptr2 = tptr;
	}
    }
  else
    {
      temps2 = (char *) encrypt_string (netpass, outbuf);
      fprintf (f, "%s", cryptit (temps2));
      nfree (temps2);
      return -1;
    }
  va_end (va);
  return 0;
}
