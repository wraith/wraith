/*
 * language.c -- handles:
 *   language support code
 *
 */

/*
 * DOES:
 *              Nothing <- typical BB code :)
 *
 * ENVIRONMENT VARIABLES:
 *              EGG_LANG       - language to use (default: "english")
 *              EGG_LANGDIR    - directory with all lang files
 *                               (default: "./language")
 * WILL DO:
 *              Upon loading:
 *              o       default loads section core, if possible.
 *              Commands:
 *              DCC .+lang <language>
 *              DCC .-lang <language>
 *              DCC .+lsec <section>
 *              DCC .-lsec <section>
 *              DCC .relang
 *              DCC .ldump
 *              DCC .lstat
 *
 * FILE FORMAT: language.lang
 *              <textidx>,<text>
 * TEXT MESSAGE USAGE:
 *              get_language(<textidx> [,<PARMS>])
 *
 * ADDING LANGUAGES:
 *              o       Copy an existing <section>.<oldlanguage>.lang to a
 *                      new .lang file and modify as needed.
 *                      Use %s or %d where necessary, for plug-in
 *                      insertions of parameters (see core.english.lang).
 *              o       Ensure <section>.<newlanguage>.lang is in the lang
 *                      directory.
 *              o       .+lang <newlanguage>
 * ADDING SECTIONS:
 *              o       Create a <newsection>.english.lang file.
 *              o       Add add_lang_section("<newsection>"); to your module
 *                      startup function.
 *
 */

#include "main.h"

extern struct dcc_t	*dcc;


typedef struct lang_st {
  struct lang_st *next;
  char *lang;
  char *section;
} lang_sec;

typedef struct lang_pr {
  struct lang_pr *next;
  char *lang;
} lang_pri;

typedef struct lang_t {
  int idx;
  char *text;
  struct lang_t *next;
} lang_tab;

static lang_tab	*langtab[64];
static lang_sec	*langsection = NULL;
static lang_pri	*langpriority = NULL;

static int add_message(int, char *);
static void read_lang(char *);
void add_lang_section(char *);
int del_lang_section(char *);
int exist_lang_section(char *);
static char *get_specific_langfile(char *, lang_sec *);
static char *get_langfile(lang_sec *);


/* Add a new preferred language to the list of languages. Newly added
 * languages get the highest priority.
 */
void add_lang(char *lang)
{
  lang_pri *lp = langpriority, *lpo = NULL;

  while (lp) {
    /* The language already exists, moving to the beginning */
    if (!strcmp(lang, lp->lang)) {
      /* Already at the front? */
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

  /* No existing entry, create a new one */
  lp = nmalloc(sizeof(lang_pri));
  lp->lang = nmalloc(strlen(lang) + 1);
  strcpy(lp->lang, lang);
  lp->next = NULL;

  /* If we have other entries, point to the beginning of the old list */
  if (langpriority)
    lp->next = langpriority;
  langpriority = lp;
  debug1("LANG: Language loaded: %s", lang);
}

static int add_message(int lidx, char *ltext)
{
  lang_tab *l = langtab[lidx & 63];

  while (l) {
    if (l->idx && (l->idx == lidx)) {
      nfree(l->text);
      l->text = nmalloc(strlen(ltext) + 1);
      strcpy(l->text, ltext);
      return 1;
    }
    if (!l->next)
      break;
    l = l->next;
  }
  if (l) {
    l->next = nmalloc(sizeof(lang_tab));
    l = l->next;
  } else
    l = langtab[lidx & 63] = nmalloc(sizeof(lang_tab));
  l->idx = lidx;
  l->text = nmalloc(strlen(ltext) + 1);
  strcpy(l->text, ltext);
  l->next = 0;
  return 0;
}

/* Parse a language file
 */
static void read_lang(char *langfile)
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

  FLANG = fopen(langfile, "r");
  if (FLANG == NULL) {
    putlog(LOG_MISC, "*", "LANG: unexpected: reading from file %s failed.",
	   langfile);
    return;
  }

  lskip = 0;
  while (fgets(lbuf, 511, FLANG)) {
    lline++;
    if (lbuf[0] != '#' || lskip) {
      ltext = nrealloc(ltext, 512);
      if (sscanf(lbuf, "%s", ltext) != EOF) {
#ifdef LIBSAFE_HACKS
	if (sscanf(lbuf, "0x%x,%500c", &lidx, ltext) != 1) {
#else
	if (sscanf(lbuf, "0x%x,%500c", &lidx, ltext) != 2) {
#endif
	  putlog(LOG_MISC, "*", "Malformed text line in %s at %d.",
		 langfile, lline);
	} else {
	  ltexts++;
	  ctmp = strchr(ltext, '\n');
	  *ctmp = 0;
	  while (ltext[strlen(ltext) - 1] == '\\') {
	    ltext[strlen(ltext) - 1] = 0;
	    if (fgets(lbuf, 511, FLANG)) {
	      lline++;
	      ctmp = strchr(lbuf, '\n');
	      *ctmp = 0;
	      ltext = nrealloc(ltext, strlen(lbuf) + strlen(ltext) + 1);
	      strcpy(strchr(ltext, 0), lbuf);
	    }
	  }
	}
	/* We gotta fix \n's here as, being arguments to sprintf(),
	 * they won't get translated.
	 */
	ctmp = ltext;
	ctmp1 = ltext;
	while (*ctmp1) {
	  if ((*ctmp1 == '\\') && (*(ctmp1 + 1) == 'n')) {
	    *ctmp = '\n';
	    ctmp1++;
	  } else if ((*ctmp1 == '\\') && (*(ctmp1 + 1) == 't')) {
	    *ctmp = '\t';
	    ctmp1++;
	  } else
	    *ctmp = *ctmp1;
	  ctmp++;
	  ctmp1++;
	}
	*ctmp = '\0';
	if (add_message(lidx, ltext)) {
	  lupdate++;
	} else
	  ladd++;
      }
    } else {
      ctmp = strchr(lbuf, '\n');
      if (lskip && (strlen(lbuf) == 1 || *(ctmp - 1) != '\\'))
	lskip = 0;
    }
  }
  nfree(ltext);
  fclose(FLANG);

  debug3("LANG: %d messages of %d lines loaded from %s", ltexts, lline,
	 langfile);
  debug2("LANG: %d adds, %d updates to message table", ladd, lupdate);
}

/* Returns 1 if the section exists, otherwise 0.
 */
int exist_lang_section(char *section)
{
  lang_sec *ls;

  for (ls = langsection; ls; ls = ls->next)
    if (!strcmp(section, ls->section))
      return 1;
  return 0;
}

/* Add a new language section. e.g. section "core"
 * Load an apropriate language file for the specified section.
 */
void add_lang_section(char *section)
{
  char		*langfile = NULL;
  lang_sec	*ls, *ols = NULL;
  int		 ok = 0;

  for (ls = langsection; ls; ols = ls, ls = ls->next)
    /* Already know of that section? */
    if (!strcmp(section, ls->section))
      return;

  /* Create new section entry */
  ls = nmalloc(sizeof(lang_sec));
  ls->section = nmalloc(strlen(section) + 1);
  strcpy(ls->section, section);
  ls->lang = NULL;
  ls->next = NULL;

  /* Connect to existing list of sections */
  if (ols)
    ols->next = ls;
  else
    langsection = ls;
  debug1("LANG: Section loaded: %s", section);

  /* Always load base language */
  langfile = get_specific_langfile(BASELANG, ls);
  if (langfile) {
    read_lang(langfile);
    nfree(langfile);
    ok = 1;
  }
  /* Now overwrite base language with a more preferred one */
  langfile = get_langfile(ls);
  if (!langfile) {
//    if (!ok)
//      putlog(LOG_MISC, "*", "LANG: No lang files found for section %s.",section);
    return;
  }
  read_lang(langfile);
  nfree(langfile);
}

int del_lang_section(char *section)
{
  lang_sec *ls, *ols;

  for (ls = langsection, ols = NULL; ls; ols = ls, ls = ls->next)
    if (ls->section && !strcmp(ls->section, section)) {
      if (ols)
	ols->next = ls->next;
      else
	langsection = ls->next;
      nfree(ls->section);
      if (ls->lang)
	nfree(ls->lang);
      nfree(ls);
      debug1("LANG: Section unloaded: %s", section);
      return 1;
    }
  return 0;
}

static char *get_specific_langfile(char *language, lang_sec *sec)
{
  char *ldir = getenv("EGG_LANGDIR");
  char *langfile;
  FILE *sfile = NULL;

  if (!ldir)
    ldir = LANGDIR;
  langfile = nmalloc(strlen(ldir) + strlen(sec->section) + strlen(language)+8);
  sprintf(langfile, "%s/%s.%s.lang", ldir, sec->section, language);
  sfile = fopen(langfile, "r");
  if (sfile) {
    fclose(sfile);
    /* Save language used for this section */
    sec->lang = nrealloc(sec->lang, strlen(language) + 1);
    strcpy(sec->lang, language);
    return langfile;
  }
  nfree(langfile);
  return NULL;
}

/* Searches for available language files and returns the file with the
 * most preferred language.
 */
static char *get_langfile(lang_sec *sec)
{
  char *langfile;
  lang_pri *lp;

  for (lp = langpriority; lp; lp = lp->next) {
    /* There is no need to reload the same language */
    if (sec->lang && !strcmp(sec->lang, lp->lang))
      return NULL;
    langfile = get_specific_langfile(lp->lang, sec);
    if (langfile)
      return langfile;
  }
  /* We did not find any files, clear the language field */
  if (sec->lang)
    nfree(sec->lang);
  sec->lang = NULL;
  return NULL;
}

static char text[512];
char *get_language(int idx)
{
  lang_tab *l;

  if (!idx)
    return "MSG-0-";
  for (l = langtab[idx & 63]; l; l = l->next)
    if (idx == l->idx)
      return l->text;
  egg_snprintf(text, sizeof text, "MSG%03X", idx);
  return text;
}

int expmem_language()
{
  lang_tab *l;
  lang_sec *ls;
  lang_pri *lp;
  int i, size = 0;

  for (i = 0; i < 64; i++)
    for (l = langtab[i]; l; l = l->next) {
      size += sizeof(lang_tab);
      size += (strlen(l->text) + 1);
    }
  for (ls = langsection; ls; ls = ls->next) {
    size += sizeof(lang_sec);
    if (ls->section)
      size += strlen(ls->section)+1;
    if (ls->lang)
      size += strlen(ls->lang)+1;
  }
  for (lp = langpriority; lp; lp = lp->next) {
    size += sizeof(lang_pri);
    if (lp->lang)
      size += strlen(lp->lang)+1;
  }
  return size;
}

void init_language(int flag)
{
  int i;
  char *deflang;

  if (flag) {
    for (i = 0; i < 32; i++)
      langtab[i] = 0;
    /* The default language is always BASELANG as language files are
     * gauranteed to exist in that language.
     */
    add_lang(BASELANG);
    /* Let the user choose a different, preferred language */
    deflang = getenv("EGG_LANG");
    if (deflang)
      add_lang(deflang);
    add_lang_section("core");
  } 
}
