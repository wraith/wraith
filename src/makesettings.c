#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#define strncpyz(_target, _source, _len)        do {                    \
        strncpy((_target), (_source), (_len) - 1);                      \
        (_target)[(_len) - 1] = 0;                                      \
} while (0)


#define LISTSEPERATORS  ",=:; "
#define BADNICKCHARS  ",+*=:!.@#;$%&"
#define SWITCHMETA    "+-"
#define HANDLEN         9
#define NICKMAX         32
#define UHOSTMAX        160
#define MAXPASSLEN      25
#define NETKEYLEN       33
#define PACKNAMELEN     40

struct cfg_struct {
  char packname[PACKNAMELEN];
  char shellhash[33];
  char bdhash[33];
  char dccprefix[2];
  char *owners;
  char *hubs;
  char *owneremail;
  char *pscloak;
  int pscloakn;
} cfg;


char *step_thru_file(FILE *fd)
{
  const int tempBufSize = 1024;
  char tempBuf[tempBufSize];
  char *retStr = NULL;
  if (fd == NULL) {
    return NULL;
  }
  retStr = NULL;
  while (!feof(fd)) {
    fgets(tempBuf, tempBufSize, fd);
    if (!feof(fd)) {
      if (retStr == NULL) {
        retStr = malloc(strlen(tempBuf) + 2);
        strcpy(retStr, tempBuf);
      } else {
        retStr = realloc(retStr, strlen(retStr) + strlen(tempBuf));
        strcat(retStr, tempBuf);
      }
      if (retStr[strlen(retStr)-1] == '\n') {
        retStr[strlen(retStr)-1] = 0;
        break;
      }
    }
  }
  return retStr;
}

char *trim(char *string)
{
  char *ibuf, *obuf;
  if (string) {
    for (ibuf = obuf = string; *ibuf; ) {
      while (*ibuf && (isspace (*ibuf)))
        ibuf++;
      if (*ibuf && (obuf != string))
        *(obuf++) = ' ';
      while (*ibuf && (!isspace (*ibuf)))
        *(obuf++) = *(ibuf++);
    }
    *obuf = '\0';
  }
  return (string);
}

char *lcase(char *string)
{
  int x = 0, length=(strlen(string) + 1);
  static char *tmp;
  tmp = (char *)malloc(length);
  strcpy(tmp, string);
  tmp[length] = 0;
  for(x=0;x<=length;x++)
    tmp[x]=tolower(tmp[x]);
  return (tmp);
}


char *newsplit(char **rest)
{
  register char *o, *r;

  if (!rest)
    return *rest = "";
  o = *rest;
  while (*o == ' ')
    o++;
  r = o;
  while (*o && (*o != ' '))
    o++;
  if (*o)
    *o++ = 0;
  *rest = o;
  return r;
}

int skipline (char *line, int *skip) {
  static int multi = 0;
  if ( (!strncmp(line, "#", 1)) || (!strncmp(line, ";", 1)) || (!strncmp(line, "//", 2)) ) {
    (*skip)++;
  } else if ( (strstr(line, "/*")) && (strstr(line, "*/")) ) {
    multi = 0;
    (*skip)++;
  } else if ( (strstr(line, "/*")) ) {
    (*skip)++;
    multi = 1;
  } else if ( (strstr(line, "*/")) ) {
    multi = 0;
  } else {
    if (!multi) (*skip) = 0;
  }
  return (*skip);
}

int isnumeric(char *string)
{
  char *p = string;
  while ((p) && *p) {
    if ((!isdigit(((char)toupper(*p)))) && (((char)toupper(*p)) != '.')) return 0;
    p++;
  }
  return 1;
}

int validhandle(char *handle) {
  char *p = NULL;
  for (p = handle; *p; p++) {
    if (strchr(BADNICKCHARS, *p)) {
      return 0;
    }
  }
  return 1;
}

int validport(char *port) {
  int p = 0;
  p = atoi(port);
  if ( (p < 1024) || (p > 65535) ) {
    return 0;
  }
  return 1;
}

int validip (char *ip) {
  int c = 0;
  while (*ip) {
    if ( (*ip == '.') || (*ip == ':') ) c++;
    ip++;
  }
  return c;
}

int validhost (char *host) {
  if ((!host) || !*host) return 1;
  if ( ((strlen(host) <= UHOSTMAX) && (strlen(host) >= 1)) && (strstr(host, ".") || strstr(host, ":")) ) {
    return 1;
  }
  return 0;
}

int validuserid (char * uid) {
  if ((strlen(uid) <= NICKMAX) && (strlen(uid) >= 1))
    return 1;
  return 0;
}

int loadconfig(char **argv) {
  FILE *f = NULL;
  char *buffer = NULL, *p = NULL;
  int skip = 0, line = 0;
  f = fopen(argv[1], "r");
  if (!f) {
    printf("Error: Can't open '\%s' for reading\n", argv[1]);
    exit(1);
  }
  printf("Reading '\%s' ", argv[1]);
  while ( (!feof(f)) && ((buffer = step_thru_file(f)) != NULL) ) {
    line++;
    if ( (*buffer) ) {
      if (strchr(buffer, '\n')) *(char*)strchr(buffer, '\n') = 0;
      if ( (skipline(buffer, &skip)) ) continue;
      if (strchr(buffer, '<') || strchr(buffer, '>')) {
        printf(" Failed\n");
        printf("%s:%d: error: Look at your configuration file again...\n", argv[1], line);
        exit(1);
      }
      p = strchr(buffer, ' ');
      while (p && (strchr(LISTSEPERATORS, p[0])))
        *p++ = 0;
      if (p) {
        int size = strlen(trim(p)) + 2;
        if (!strcmp(lcase(buffer), "packname")) {
          strncpyz(cfg.packname, trim(p), sizeof cfg.packname);
          printf(".");
        } else if (!strcmp(lcase(buffer), "shellhash")) {
          strncpyz(cfg.shellhash, trim(p), sizeof cfg.shellhash);
          printf(".");
        } else if (!strcmp(lcase(buffer), "bdhash")) {
          strncpyz(cfg.bdhash, trim(p), sizeof cfg.bdhash);
          printf(".");
        } else if (!strcmp(lcase(buffer), "dccprefix")) {
          strncpyz(cfg.dccprefix, trim(p), sizeof cfg.dccprefix);
          printf(".");
        } else if (!strcmp(lcase(buffer), "owner")) {
          if (cfg.owners && strlen(cfg.owners))
            size += strlen(cfg.owners);

          cfg.owners = realloc(cfg.owners, size);
          strcat(cfg.owners, trim(p));
          strcat(cfg.owners, ",");
          printf(".");
        } else if (!strcmp(lcase(buffer), "owneremail")) {
          if (cfg.owneremail && strlen(cfg.owneremail))
            size += strlen(cfg.owneremail);

          cfg.owneremail = realloc(cfg.owneremail, size);
          strcat(cfg.owneremail, trim(p));
          strcat(cfg.owneremail, ",");
          printf(".");
        } else if (!strcmp(lcase(buffer), "hub")) {
          if (cfg.hubs && strlen(cfg.hubs))
            size += strlen(cfg.hubs);

          cfg.hubs = realloc(cfg.hubs, size);
          strcat(cfg.hubs, trim(p));
          strcat(cfg.hubs, ",");
          printf(".");
        } else if (!strcmp(lcase(buffer), "pscloak")) {
          if (cfg.pscloak && strlen(cfg.pscloak))
            size += strlen(cfg.pscloak);

          cfg.pscloak = realloc(cfg.pscloak, size);
          strcat(cfg.pscloak, trim(p));
          strcat(cfg.pscloak, " ");
          cfg.pscloakn++;
          printf(".");
        } else {
          printf(",");
        }
      }
    }
    buffer = NULL;
  }
  if (f) fclose(f);
  printf(" Success\n");
  return 1;
}

void tellconfig()
{
  printf("packname: %s\n", cfg.packname);
  printf("shellhash: %s\n", cfg.shellhash);
  printf("bdhash: %s\n", cfg.bdhash);
  printf("dccprefix: %s\n", cfg.dccprefix);
  printf("owners: %s\n", cfg.owners);
  printf("owneremails: %s\n", cfg.owneremail);
  printf("hubs: %s\n", cfg.hubs);
  printf("pscloak(%d) %s\n", cfg.pscloakn, cfg.pscloak);
}

void freecfg()
{
  free(cfg.owners);
  free(cfg.hubs);
  free(cfg.owneremail);
  free(cfg.pscloak);
}

int checkconfig()
{

  return 1;
}

char *pscloak (int n)
{
  int i = 0;
  char *ps = malloc(strlen(cfg.pscloak) + 1), *p = NULL;
  strcpy(ps, cfg.pscloak);

  for (i = 0; i < n; i++)
    p = newsplit(&ps);

  return p;
}

int writeconfig(char **argv)
{
  FILE *f = NULL;
  int i = 0;
  f = fopen(argv[2], "w");
  if (!f) {
    printf("Error: Can't open '\%s' for writing\n", argv[1]);
    exit(1);
  }
fprintf(f, " \
/* DO NOT EDIT THIS FILE, EDIT pack/pack.cfg INSTEAD */\n\
#include <stdio.h> \n\
#include <stdlib.h> \n\
#include <string.h> \n\
#include \"main.h\"\n\
\n\
char packname[512], shellhash[33], bdhash[33], dcc_prefix[2], *owners, *hubs, *owneremail;\n\n\
char *progname() {\n\
#ifdef S_PSCLOAK\n");
fprintf(f," \
  switch (random() %% %d) {\n", cfg.pscloakn + 1);

  for (i = 0; i < (cfg.pscloakn + 1); i++)
fprintf(f, " \
    case %d: return STR(\"%s\");\n", i, pscloak(i+1));
fprintf(f, " \
  }\n\
#endif /* S_PSCLOAK */\n\
  return \"wraith\";\n\
}\n\n");


  fprintf(f, "#define _PACKNAME STR(\"%s\")\n", cfg.packname);
  fprintf(f, "#define _DCCPREFIX STR(\"%s\")\n", cfg.dccprefix);
  fprintf(f, "#define _SHELLHASH STR(\"%s\")\n", cfg.shellhash);
  fprintf(f, "#define _BDHASH STR(\"%s\")\n", cfg.bdhash);
  fprintf(f, "#define _OWNERS STR(\"%s\")\n", cfg.owners);
  fprintf(f, "#define _OWNEREMAIL STR(\"%s\")\n", cfg.owneremail);
  fprintf(f, "#define _HUBS STR(\"%s\")\n", cfg.hubs);

fprintf(f, " \n\n\
int init_settings()\n\
{\n\
  owners = my_malloc(strlen(_OWNERS) + 1);\n\
  hubs = my_malloc(strlen(_HUBS) + 1);\n\
  owneremail = my_malloc(strlen(_OWNEREMAIL) + 1);\n\
\n\
  sprintf(owners, _OWNERS);\n\
  sprintf(hubs, _HUBS);\n\
  sprintf(owneremail, _OWNEREMAIL);\n\
  egg_snprintf(packname, sizeof packname, _PACKNAME);\n\
  egg_snprintf(bdhash, sizeof bdhash, _BDHASH);\n\
  egg_snprintf(shellhash, sizeof shellhash, _SHELLHASH);\n\
  egg_snprintf(dcc_prefix, sizeof dcc_prefix, _DCCPREFIX);\n\
  dcc_prefix[1] = 0;\n\
  sdprintf(STR(\"owners: %%s\\nhubs: %%s\\nowneremail: %%s\"), owners, hubs, owneremail);\n\
  sdprintf(STR(\"dcc_prefix: %%s \\nbdhash: %%s \\nshellhash: %%s\"), dcc_prefix, bdhash, shellhash);\n\
  return 1;\n\
}\n");


fflush(f);
fclose(f);
return 1;

}

int main(int argc, char **argv) {
  if (!loadconfig(argv)) return 1;
//  tellconfig();
  if (!checkconfig()) return 1;
  if (!writeconfig(argv)) return 1;
  freecfg();
  return 0;
}
