
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

typedef struct {
  int leaf;
  int hub;
  char *name;
  char *txt;
} cmds;

#define BUFSIZE 20240

cmds cmdlist[900];
int cmdi = 0;

char *replace(char *string, char *oldie, char *newbie)
{
  static char newstring[BUFSIZE] = "";
  int str_index, newstr_index, oldie_index, end, new_len, old_len, cpy_len;
  char *c = NULL;

  if (string == NULL) return "";
  if ((c = (char *) strstr(string, oldie)) == NULL) return string;
  new_len = strlen(newbie);
  old_len = strlen(oldie);
  end = strlen(string) - old_len;
  oldie_index = c - string;
  newstr_index = 0;
  str_index = 0;
  while(str_index <= end && c != NULL) {
    cpy_len = oldie_index-str_index;
    strncpy(newstring + newstr_index, string + str_index, cpy_len);
    newstr_index += cpy_len;
    str_index += cpy_len;
    strcpy(newstring + newstr_index, newbie);
    newstr_index += new_len;
    str_index += old_len;
    if((c = (char *) strstr(string + str_index, oldie)) != NULL)
     oldie_index = c - string;
  }
  strcpy(newstring + newstr_index, string + str_index);
  return (newstring);
}

char *step_thru_file(FILE *fd)
{
  char tempBuf[BUFSIZE] = "", *retStr = NULL;

  if (fd == NULL) {
    return NULL;
  }
  retStr = NULL;
  while (!feof(fd)) {
    fgets(tempBuf, sizeof(tempBuf), fd);
    if (!feof(fd)) {
      if (retStr == NULL) {
        retStr = (char *) calloc(1, strlen(tempBuf) + 2);
        strcpy(retStr, tempBuf);
      } else {
        retStr = (char *) realloc(retStr, strlen(retStr) + strlen(tempBuf));
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

  if ((!strncmp(line, "//", 2))) {
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

int my_cmp (const cmds *c1, const cmds *c2)
{
  return strcmp (c1->name, c2->name);
}

int parse_help(char *infile, char *outfile) {
  FILE *in = NULL, *out = NULL;
  char *buffer = NULL, my_buf[BUFSIZE] = "", *fulllist = (char *) calloc(1, 1);
  int skip = 0, line = 0, i = 0, leaf = 0, hub = 0;

  if (!(in = fopen(infile, "r"))) {
    printf("Error: Cannot open '%s' for reading\n", infile);
    return 1;
  }
  printf("Sorting help file '%s'", infile);
  while ((!feof(in)) && ((buffer = step_thru_file(in)) != NULL) ) {

    line++;
    if ((*buffer)) {
      if (strchr(buffer, '\n')) *(char*)strchr(buffer, '\n') = 0;
      if ((skipline(buffer, &skip))) continue;
      if (buffer[0] == ':') { //New cmd 
        char *ifdef = (char *) calloc(1, strlen(buffer) + 1), *p;

        buffer++;
        strcpy(ifdef, buffer);
        p = strchr(ifdef, ':');
        *p = 0;
        if (ifdef && ifdef[0]) {
          if (!strcasecmp(ifdef, "leaf"))
            leaf++;
          else if (!strcasecmp(ifdef, "hub"))
            hub++;
        }

        /* finish last command */
        if (my_buf && my_buf[0]) {
          my_buf[strlen(my_buf)] = 0;
          cmdlist[cmdi].txt = (char *) calloc(1, strlen(my_buf) + 1);
          strcpy(cmdlist[cmdi].txt, my_buf);
          i++;
          cmdi++;
        }
	/* move on to next cmd now */
        p = strchr(buffer, ':');
        p++;
        if (strcmp(p, "end")) {		/* NEXT CMD */
          printf(".");
          my_buf[0] = 0;
          cmdlist[cmdi].leaf = leaf;
          cmdlist[cmdi].hub = hub;
          hub = leaf = 0;
          for (i = 0; i < cmdi; i++ )	/* Eliminate duplicates */
            if (!strcmp(cmdlist[i].name, p)) {
              printf("\b[%s]", p);
              cmdi--;
              my_buf[0] = 0;
              continue;
            }
          cmdlist[cmdi].name = (char *) calloc(1, strlen(p) + 1);
          strcpy(cmdlist[cmdi].name, p);
        } else {			/* END */
          break;
        }
      } else {				/* CMD HELP INFO */
        strcat(my_buf, buffer);
        strcat(my_buf, "\\n");
      }
    }
    buffer = NULL;
  }
  if (in) fclose(in);
  if (!(out = fopen(outfile, "w"))) {
    printf("Error: Cannot open '%s' for writing\n", outfile);
    return 1;
  }
  qsort(cmdlist, cmdi, sizeof(cmds), (int (*)(const void *, const void *)) &my_cmp);

  for (i = 0; i < cmdi; i++ ) {
    fulllist = (char *) realloc(fulllist, strlen(fulllist) + strlen(cmdlist[i].name) + 2);
    strcat(fulllist, cmdlist[i].name);
    strcat(fulllist, " ");
    fprintf(out, ":");
    if (cmdlist[i].leaf) fprintf(out, "leaf");
    else if (cmdlist[i].hub) fprintf(out, "hub");
    fprintf(out, ":%s\n", cmdlist[i].name);
    fprintf(out, "%s", replace(cmdlist[i].txt, "\\n", "\n"));
  }

  fprintf(out, "::end\n");
  if (out) fclose(out);
  printf(" Success\n");
  fulllist[strlen(fulllist)] = 0;
  printf("Sorted (%d): %s\n", cmdi, fulllist);
  return 0;
}

int main(int argc, char **argv) {
  char *in = NULL, *out = NULL;
  int ret = 0;

  if (argc < 3) return 1;
  in = (char *) calloc(1, strlen(argv[1]) + 1);
  strcpy(in, argv[1]);
  out = (char *) calloc(1, strlen(argv[2]) + 1);
  strcpy(out, argv[2]);
  ret = parse_help(in, out);
  free(in);
  free(out);
  return ret;
}

