#include "main.h"
extern struct dcc_t *dcc;
typedef struct lang_st
{
  struct lang_st *next;
  char *lang;
  char *section;
} lang_sec;
typedef struct lang_pr
{
  struct lang_pr *next;
  char *lang;
} lang_pri;
typedef struct lang_t
{
  int idx;
  char *text;
  struct lang_t *next;
} lang_tab;
static lang_tab *langtab[64];
static lang_sec *langsection = NULL;
static lang_pri *langpriority = NULL;
static int add_message (int, char *);
static void read_lang (char *);
void add_lang_section (char *);
int del_lang_section (char *);
int exist_lang_section (char *);
static char *get_specific_langfile (char *, lang_sec *);
static char *get_langfile (lang_sec *);
void
add_lang (char *lang)
{
  lang_pri *lp = langpriority, *lpo = NULL;
  while (lp)
    {
      if (!strcmp (lang, lp->lang))
	{
	  if (!lpo)
	    return;
	  lpo->next = lp->next;
	  lp->next = lpo;
	  langpriority = lp;
	  return;
	}
      lpo = lp;
      lp = lp->next;
    }
  lp = nmalloc (sizeof (lang_pri));
  lp->lang = nmalloc (strlen (lang) + 1);
  strcpy (lp->lang, lang);
  lp->next = NULL;
  if (langpriority)
    lp->next = langpriority;
  langpriority = lp;
  debug1 ("LANG: Language loaded: %s", lang);
}
static int
add_message (int lidx, char *ltext)
{
  lang_tab *l = langtab[lidx & 63];
  while (l)
    {
      if (l->idx && (l->idx == lidx))
	{
	  nfree (l->text);
	  l->text = nmalloc (strlen (ltext) + 1);
	  strcpy (l->text, ltext);
	  return 1;
	}
      if (!l->next)
	break;
      l = l->next;
    }
  if (l)
    {
      l->next = nmalloc (sizeof (lang_tab));
      l = l->next;
    }
  else
    l = langtab[lidx & 63] = nmalloc (sizeof (lang_tab));
  l->idx = lidx;
  l->text = nmalloc (strlen (ltext) + 1);
  strcpy (l->text, ltext);
  l->next = 0;
  return 0;
}
static void
read_lang (char *langfile)
{
  FILE *FLANG;
  char lbuf[512];
  char *ltext = NULL;
  char *ctmp, *ctmp1;
  int lidx;
  int lline = 0;
  int lskip;
  int ltexts = 0;
  int ladd = 0, lupdate = 0;
  FLANG = fopen (langfile, "r");
  if (FLANG == NULL)
    {
      putlog (LOG_MISC, "*", "LANG: unexpected: reading from file %s failed.",
	      langfile);
      return;
    }
  lskip = 0;
  while (fgets (lbuf, 511, FLANG))
    {
      lline++;
      if (lbuf[0] != '#' || lskip)
	{
	  ltext = nrealloc (ltext, 512);
	  if (sscanf (lbuf, "%s", ltext) != EOF)
	    {
#ifdef LIBSAFE_HACKS
	      if (sscanf (lbuf, "0x%x,%500c", &lidx, ltext) != 1)
		{
#else
	      if (sscanf (lbuf, "0x%x,%500c", &lidx, ltext) != 2)
		{
#endif
		  putlog (LOG_MISC, "*", "Malformed text line in %s at %d.",
			  langfile, lline);
		}
	      else
		{
		  ltexts++;
		  ctmp = strchr (ltext, '\n');
		  *ctmp = 0;
		  while (ltext[strlen (ltext) - 1] == '\\')
		    {
		      ltext[strlen (ltext) - 1] = 0;
		      if (fgets (lbuf, 511, FLANG))
			{
			  lline++;
			  ctmp = strchr (lbuf, '\n');
			  *ctmp = 0;
			  ltext =
			    nrealloc (ltext,
				      strlen (lbuf) + strlen (ltext) + 1);
			  strcpy (strchr (ltext, 0), lbuf);
			}
		    }
		}
	      ctmp = ltext;
	      ctmp1 = ltext;
	      while (*ctmp1)
		{
		  if ((*ctmp1 == '\\') && (*(ctmp1 + 1) == 'n'))
		    {
		      *ctmp = '\n';
		      ctmp1++;
		    }
		  else if ((*ctmp1 == '\\') && (*(ctmp1 + 1) == 't'))
		    {
		      *ctmp = '\t';
		      ctmp1++;
		    }
		  else
		    *ctmp = *ctmp1;
		  ctmp++;
		  ctmp1++;
		}
	      *ctmp = '\0';
	      if (add_message (lidx, ltext))
		{
		  lupdate++;
		}
	      else
		ladd++;
	    }
	}
      else
	{
	  ctmp = strchr (lbuf, '\n');
	  if (lskip && (strlen (lbuf) == 1 || *(ctmp - 1) != '\\'))
	    lskip = 0;
	}
    }
  nfree (ltext);
  fclose (FLANG);
  debug3 ("LANG: %d messages of %d lines loaded from %s", ltexts, lline,
	  langfile);
  debug2 ("LANG: %d adds, %d updates to message table", ladd, lupdate);
}

int
exist_lang_section (char *section)
{
  lang_sec *ls;
  for (ls = langsection; ls; ls = ls->next)
    if (!strcmp (section, ls->section))
      return 1;
  return 0;
}

void
add_lang_section (char *section)
{
  char *langfile = NULL;
  lang_sec *ls, *ols = NULL;
  int ok = 0;
  for (ls = langsection; ls; ols = ls, ls = ls->next)
    if (!strcmp (section, ls->section))
      return;
  ls = nmalloc (sizeof (lang_sec));
  ls->section = nmalloc (strlen (section) + 1);
  strcpy (ls->section, section);
  ls->lang = NULL;
  ls->next = NULL;
  if (ols)
    ols->next = ls;
  else
    langsection = ls;
  debug1 ("LANG: Section loaded: %s", section);
  langfile = get_specific_langfile (BASELANG, ls);
  if (langfile)
    {
      read_lang (langfile);
      nfree (langfile);
      ok = 1;
    }
  langfile = get_langfile (ls);
  if (!langfile)
    {
      return;
    }
  read_lang (langfile);
  nfree (langfile);
}

int
del_lang_section (char *section)
{
  lang_sec *ls, *ols;
  for (ls = langsection, ols = NULL; ls; ols = ls, ls = ls->next)
    if (ls->section && !strcmp (ls->section, section))
      {
	if (ols)
	  ols->next = ls->next;
	else
	  langsection = ls->next;
	nfree (ls->section);
	if (ls->lang)
	  nfree (ls->lang);
	nfree (ls);
	debug1 ("LANG: Section unloaded: %s", section);
	return 1;
      }
  return 0;
}
static char *
get_specific_langfile (char *language, lang_sec * sec)
{
  char *ldir = getenv ("EGG_LANGDIR");
  char *langfile;
  FILE *sfile = NULL;
  if (!ldir)
    ldir = LANGDIR;
  langfile =
    nmalloc (strlen (ldir) + strlen (sec->section) + strlen (language) + 8);
  sprintf (langfile, "%s/%s.%s.lang", ldir, sec->section, language);
  sfile = fopen (langfile, "r");
  if (sfile)
    {
      fclose (sfile);
      sec->lang = nrealloc (sec->lang, strlen (language) + 1);
      strcpy (sec->lang, language);
      return langfile;
    }
  nfree (langfile);
  return NULL;
}
static char *
get_langfile (lang_sec * sec)
{
  char *langfile;
  lang_pri *lp;
  for (lp = langpriority; lp; lp = lp->next)
    {
      if (sec->lang && !strcmp (sec->lang, lp->lang))
	return NULL;
      langfile = get_specific_langfile (lp->lang, sec);
      if (langfile)
	return langfile;
    }
  if (sec->lang)
    nfree (sec->lang);
  sec->lang = NULL;
  return NULL;
}
static char text[512];
char *
get_language (int idx)
{
  lang_tab *l;
  if (!idx)
    return "MSG-0-";
  for (l = langtab[idx & 63]; l; l = l->next)
    if (idx == l->idx)
      return l->text;
  egg_snprintf (text, sizeof text, "MSG%03X", idx);
  return text;
}

int
expmem_language ()
{
  lang_tab *l;
  lang_sec *ls;
  lang_pri *lp;
  int i, size = 0;
  for (i = 0; i < 64; i++)
    for (l = langtab[i]; l; l = l->next)
      {
	size += sizeof (lang_tab);
	size += (strlen (l->text) + 1);
      }
  for (ls = langsection; ls; ls = ls->next)
    {
      size += sizeof (lang_sec);
      if (ls->section)
	size += strlen (ls->section) + 1;
      if (ls->lang)
	size += strlen (ls->lang) + 1;
    }
  for (lp = langpriority; lp; lp = lp->next)
    {
      size += sizeof (lang_pri);
      if (lp->lang)
	size += strlen (lp->lang) + 1;
    }
  return size;
}

void
init_language (int flag)
{
  int i;
  char *deflang;
  if (flag)
    {
      for (i = 0; i < 32; i++)
	langtab[i] = 0;
      add_lang (BASELANG);
      deflang = getenv ("EGG_LANG");
      if (deflang)
	add_lang (deflang);
      add_lang_section ("core");
    }
}
