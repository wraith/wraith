#include "common.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

char *cfgfile = NULL;

#define strncpyz(_target, _source, _len)        do {                    \
        strncpy((_target), (_source), (_len) - 1);                      \
        (_target)[(_len) - 1] = 0;                                      \
} while (0)


#define LISTSEPERATORS  ",=:; "

struct cfg_struct {
  char packname[PACKNAMELEN + 1];
  char shellhash[MD5_HASH_LENGTH + 1];
  char bdhash[MD5_HASH_LENGTH + 1];
  char owners[1024];
  char hubs[1024];
  char owneremail[1024];
  char salt1[SALT1LEN + 1];
  char salt2[SALT2LEN + 1];
  char dccprefix[2];
} cfg;

char *step_thru_file(FILE *fd)
{
  char tempBuf[1024] = "", *retStr = NULL;

  if (fd == NULL) {
    return NULL;
  }
  retStr = NULL;
  while (!feof(fd)) {
    fgets(tempBuf, sizeof(tempBuf), fd);
    if (!feof(fd)) {
      if (retStr == NULL) {
        retStr = calloc(1, strlen(tempBuf) + 2);
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
  char *ibuf = NULL, *obuf = NULL;

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

char *randstring(int len)
{
  int j, r = 0;
  static char s[100] = "";

  for (j = 0; j < len; j++) {
    r = rand();
    if (r % 3 == 0)
      s[j] = '0' + (rand() % 10);
    else if (r % 3 == 1)
      s[j] = 'a' + (rand() % 26);
    else if (r % 3 == 2)
      s[j] = 'A' + (rand() % 26);
  }
  s[len] = '\0';
  return s;
}

void dosalt(char *salt1, char *salt2) {
  FILE *f = NULL;

  f = fopen("src/salt.h.tmp", "w");
  fprintf(f, "#define STR(x) x\n");
  fprintf(f, "#define SALT1 STR(\"%s\")\n", salt1);
  fprintf(f, "#define SALT2 STR(\"%s\")\n", salt2);
  fflush(f);
  fclose(f);
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
        if (!egg_strcasecmp(buffer, "packname")) {
          strncpyz(cfg.packname, trim(p), sizeof cfg.packname);
          printf(".");
        } else if (!egg_strcasecmp(buffer, "shellhash")) {
          strncpyz(cfg.shellhash, trim(p), sizeof cfg.shellhash);
          printf(".");
        } else if (!egg_strcasecmp(buffer, "bdhash")) {
          strncpyz(cfg.bdhash, trim(p), sizeof cfg.bdhash);
          printf(".");
        } else if (!egg_strcasecmp(buffer, "dccprefix")) {
          strncpyz(cfg.dccprefix, trim(p), sizeof cfg.dccprefix);
          printf(".");
        } else if (!egg_strcasecmp(buffer, "owner")) {
          strcat(cfg.owners, trim(p));
          strcat(cfg.owners, ",");
          printf(".");
        } else if (!egg_strcasecmp(buffer, "owneremail")) {
          strcat(cfg.owneremail, trim(p));
          strcat(cfg.owneremail, ",");
          printf(".");
        } else if (!egg_strcasecmp(buffer, "hub")) {
          strcat(cfg.hubs, trim(p));
          strcat(cfg.hubs, ",");
          printf(".");
        } else if (!egg_strcasecmp(buffer, "salt1")) {
          strcat(cfg.salt1, trim(p));
          printf(".");
        } else if (!egg_strcasecmp(buffer, "salt2")) {
          strcat(cfg.salt2, trim(p));
          printf(".");
        } else {
          printf("%s %s\n", buffer, p);
          printf(",");
        }
      }
    }
    buffer = NULL;
  }
  if (f) fclose(f);
  if (cfg.salt1 && cfg.salt2 && cfg.salt1[2] && cfg.salt2[2]) {
    dosalt(cfg.salt1, cfg.salt2);
  } else { /* we need to generate the SALTS */
    char salt1[SALT1LEN + 1] = "", salt2[SALT2LEN + 1] = "";
    time_t now = time(NULL);
    srand(now % (getpid() + getppid()));
    printf("Creating Salts");
    if ((f = fopen(cfgfile, "a")) == NULL) {
      printf("Cannot open cfgfile for appending.. aborting\n");
      exit(1);
    }
    strcat(salt1, randstring(SALT1LEN));
    strcat(salt2, randstring(SALT2LEN));
    salt1[sizeof salt1] = salt2[sizeof salt2] = 0;
    fprintf(f, "SALT1 %s\n", salt1);
    fprintf(f, "SALT2 %s\n", salt2);
    fflush(f);
    fclose(f);
    dosalt(salt1, salt2);
  }
  printf(" Success\n");
  return 1;
}

void tellconfig(struct cfg_struct *tcfg)
{
  printf("packname: %s\n", tcfg->packname);
  printf("shellhash: %s\n", tcfg->shellhash);
  printf("bdhash: %s\n", tcfg->bdhash);
  printf("dccprefix: %s\n", tcfg->dccprefix);
  printf("owners: %s\n", tcfg->owners);
  printf("owneremails: %s\n", tcfg->owneremail);
  printf("hubs: %s\n", tcfg->hubs);
}

int checkconfig()
{
  return 1;
}

int writeconfig(char **argv)
{
  FILE *f = NULL;

  f = fopen(argv[2], "w");
  if (!f) {
    printf("Error: Can't open '\%s' for writing\n", argv[1]);
    exit(1);
  }
fprintf(f, " \
/* DO NOT EDIT THIS FILE, EDIT %s INSTEAD */\n\
#include <stdio.h> \n\
#include <stdlib.h> \n\
#include <string.h> \n\
#include \"common.h\"\n\
#include \"debug.h\"\n\
\n\
char packname[512] = \"\", shellhash[33] = \"\", bdhash[33] = \"\", dcc_prefix[2] = \"\", \
*owners = NULL, *hubs = NULL, *owneremail = NULL;\n\n", cfgfile);

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
  owners = calloc(1, strlen(_OWNERS) + 1);\n\
  hubs = calloc(1, strlen(_HUBS) + 1);\n\
  owneremail = calloc(1, strlen(_OWNEREMAIL) + 1);\n\
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

void binwrite(char *fname) 
{
  FILE *f = NULL;

  f = fopen(fname, "wb");
//       size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
  fwrite(&cfg, sizeof(struct cfg_struct), 1, f);
  fflush(f);
  fclose(f);
  exit(1);
}

void binread(char *fname)
{
  FILE *f = NULL;
  struct cfg_struct mycfg;

  f = fopen(fname, "rb");
//       size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
  fread(&mycfg, sizeof(struct cfg_struct), 1, f);
  fclose(f);
  tellconfig(&mycfg);
  exit(1);

}

int main(int argc, char **argv) {
//binread(argv[1]);
  cfgfile = strdup(argv[1]);
  if (!loadconfig(argv)) return 1;
  if (!checkconfig()) return 1;
tellconfig(&cfg);
//  binwrite(argv[2]);
  if (!writeconfig(argv)) return 1;
  return 0;
}
