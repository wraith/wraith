#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "standalone.h"

#include "md5/global.h"
#include "md5/md5.h"
#include "rijndael.h"

char packname[512]="";
char packpass[512]="";
char bdpass[512]="";
char netkey[512]="";
char owners[8192]="";
unsigned char key[20];

void fix_md5(char * s) {
  int i;
  for (i=0;i<16;i++) 
    if (!s[i])
      s[i]=1;
  s[16]=0;
}

int fixfile (char * fname, int writeall) {
  FILE * f;
  size_t size;
  char * buf, *p, *p2, *np;
  unsigned char bp[20], ln[2048];
  MD5_CTX ctx;

  f=fopen(fname, "r+");

  if (!f) {
    printf("Can't open %s for editing\n", fname);
    return 0;
  }

  fseek(f, 0, SEEK_END);
  size=ftell(f);
  buf = malloc(size);
  fseek(f, 0, SEEK_SET);
  if (fread(buf, 1, size, f) != size) {
    fclose(f);
    printf("Can't read %s\n", fname);
    return 0;
  }
  p2 = buf;
  while (p2 < (buf + size - 4)) {
    if (!strncmp(p2, "AAAAAAAA", 8))
      break;
    p2 += 4;
  }
  if (p2 >= (buf + size - 4)) {
    fclose(f);
    printf("Can't figure out where to modify %s\n", fname);
    return 0;
  }
  memset(p2, 0, 16);
  p2 += 16;
  strcpy(p2, packname);
  p2 += 128;
  if (writeall) {
    p=encrypt_string(key, "pack.def");
    sprintf(p2, "%s\n", p);
    p2 += strlen(p2);

    sprintf(ln, "pn=%s", packname);
    p=encrypt_string(key, ln);
    sprintf(p2, "%s\n", p);
    p2 += strlen(p2);

    MD5Init(&ctx);
    MD5Update(&ctx, bdpass, strlen(bdpass));
    MD5Final(bp, &ctx);
    fix_md5(bp);
    sprintf(ln, "bp=%s", bp);
    p=encrypt_string(key, ln);
    sprintf(p2, "%s\n", p);
    p2 += strlen(p2);
  
    sprintf(ln, "nk=%s", netkey);
    p=encrypt_string(key, ln);
    sprintf(p2, "%s\n", p);
    p2 += strlen(p2);
  
    p=owners;
    while (p) {
      np=strchr(p, '\n');
      if (np)
	*np++=0;
      sprintf(ln, "o=%s", p);
      p=encrypt_string(key, ln);
      sprintf(p2, "%s\n", p);
      p2 += strlen(p2);
      p=np;
    }
  } else {
    memset(ln, 0, sizeof(ln));
    strcpy(ln, netkey);
    p = encrypt_string(key, ln);
    sprintf(p2, "%s", p);
  }
  
  fseek(f, 0, SEEK_SET);
  if (!fwrite(buf, 1, size, f)==size) {
    printf("Error writing new %s\n", fname);
    return 0;
  }
  printf("Written %s\n", fname);
  free(buf);
  fclose(f);
  return 1;
};

int main (int argc, char * argv[]) {
  FILE * f;
  unsigned char *p, ln[2048];
  MD5_CTX ctx;
  if (argc!=2) {
    printf("Usage: %s configfile\n", argv[0]);
    exit(0);
  }
  f=fopen(argv[1], "r");
  while (!feof(f)) {
    fgets(ln, sizeof(ln), f);
    if (strchr(ln, '\n'))
      * (char *) strchr(ln, '\n') = 0;
    if (ln[0]) {
      p=strchr(ln, ' ');
      while (p && (*p==' '))
	*p++ = 0;
      if (p) {
	if (!strcmp(ln, "pass"))
	  strcpy(packpass, p);
	else if (!strcmp(ln, "bdpass"))
	  strcpy(bdpass, p);
	else if (!strcmp(ln, "packname"))
	  strcpy(packname, p);
	else if (!strcmp(ln, "netkey"))
	  strcpy(netkey, p);
	else if (!strcmp(ln, "owner")) {
	  strcat(owners, p);
	  strcat(owners, "\n");
	} else
	  printf("Invalid line: %s %s\n", ln, p);
      } else {
	printf("Invalid line: %s\n", ln);
      }
    }
  }
  fclose(f);
  if (!packname[0]) {
    printf("No packname\n");
    return 0;
  }
  if (!packpass[0]) {
    printf("No packpass\n");
    return 0;
  }
  if (!bdpass[0]) {
    printf("No bdpass\n");
    return 0;
  }
  if (!netkey[0]) {
    printf("No netkey\n");
    return 0;
  }
  if (!owners[0]) {
    printf("No owners\n");
    return 0;
  }
  MD5Init(&ctx);
  MD5Update(&ctx, packpass, strlen(packpass));
  MD5Final(key, &ctx);
  fix_md5(key);

  fixfile("makebot", 1);
  fixfile("readlog", 0);
    return 0; 

  return 0;
}




