#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <bdlib/src/String.h>
#include <bdlib/src/Stream.h>

typedef struct {
  int leaf;
  int hub;
  bd::String* name;
  bd::String* txt;
} cmds;

cmds cmdlist[900];
int cmdi = 0;

int skipline (const char *line, int *skip) {
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
  return c1->name->compare(*(c2->name));
}

int parse_help(const bd::String& infile, const bd::String& outfile) {
  bd::Stream in, out;
  bd::String buffer, ifdef, my_buf, fulllist;
  int skip = 0, i = 0, leaf = 0, hub = 0;

  in.loadFile(infile);
  printf("Sorting help file '%s'", infile.c_str());
  while (in.tell() < in.length()) {
    buffer = in.getline().chomp();

    if ((skipline(buffer.c_str(), &skip))) continue;
    if (buffer[0] == ':') { //New cmd
      ++buffer;
      ifdef = newsplit(buffer, ':');

      if (ifdef.length()) {
        if (ifdef == "leaf")
          leaf++;
        else if (ifdef == "hub")
          hub++;
      }

      /* finish last command */
      if (my_buf.length()) {
        cmdlist[cmdi].txt = new bd::String(my_buf);
        ++i;
        ++cmdi;
        my_buf.clear();
      }
      /* move on to next cmd now */
      if (buffer !=  "end") {		/* NEXT CMD */
        printf(".");
        cmdlist[cmdi].leaf = leaf;
        cmdlist[cmdi].hub = hub;
        hub = leaf = 0;
        for (i = 0; i < cmdi; i++ ) {	/* Eliminate duplicates */
          if (cmdlist[i].name && *(cmdlist[i].name) == buffer) {
            printf("\b[%s]", buffer.c_str());
            --cmdi;
            continue;
          }
        }
        cmdlist[cmdi].name = new bd::String(buffer);
      } else {			/* END */
        break;
      }
    } else {				/* CMD HELP INFO */
      my_buf += buffer + "{NEWLINE}";
    }
  }

  qsort(cmdlist, cmdi, sizeof(cmds), (int (*)(const void *, const void *)) &my_cmp);

  bd::String buf;
  for (i = 0; i < cmdi; i++ ) {
    fulllist += *(cmdlist[i].name) + " ";
    out << ":";
    if (cmdlist[i].leaf) out << "leaf";
    else if (cmdlist[i].hub) out << "hub";
    out << bd::String::printf(":%s\n", cmdlist[i].name->c_str());
    out << cmdlist[i].txt->sub("{NEWLINE}", "\n");
  }

  out << "::end\n";

  out.writeFile(outfile);
  printf(" Success\n");
  printf("Sorted (%d): %s\n", cmdi, fulllist.c_str());
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 3) return 1;

  bd::String in(argv[1]), out(argv[2]);
  return parse_help(in, out);
}

