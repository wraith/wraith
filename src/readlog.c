#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "standalone.h"

#include "md5/global.h"
#include "md5/md5.h"
#include "rijndael.h"

struct encdata_struct {
  char prefix[16];
  char packname[128];
  char logkey[128];
} encdata = { "AAAAAAAAAAAAAAAA", "", "" };


char packname[512]="";
char packpass[512]="";
char bdpass[512]="";
char netkey[512]="";
char owners[8192]="";

void fix_md5(char * s) {
  int i;
  for (i=0;i<16;i++)
    if (!s[i])
      s[i]=1;
  s[16]=0;
}

int main (int argc, char * argv[]) {
  FILE * f;
  unsigned char *p, ln[2048], key[100], *nk;
  MD5_CTX ctx;

  if (argc!=2) {
    printf("Usage: %s logfile\n", argv[0]);
    exit(0);
  }
  printf("%s readlog\n", encdata.packname);
  printf("log password:");
  bzero(key, sizeof(key));
  fgets(key, sizeof(key), stdin);
  p=strchr(key, '\n');
  if (p)
    *p=0;
  MD5Init(&ctx);
  MD5Update(&ctx, key, strlen(key));
  MD5Final(key, &ctx);
  fix_md5(key);
  
  nk = decrypt_string(key, encdata.logkey);
  
  f=fopen(argv[1], "r");
  while (!feof(f)) {
    fgets(ln, sizeof(ln), f);
    if (strchr(ln, '\n'))
      * (char *) strchr(ln, '\n') = 0;
    if (ln[0]) {
      p=decrypt_string(nk, ln);
      printf("%s\n", p);
      nfree(p);
    }
  }
  fclose(f);
  return 0;
}




