#ifndef _BINARY_H
#  define _BINARY_H

typedef struct encdata_struct {
  char prefix[16];
  char data[512];
} encdata_t;


extern encdata_t encdata;

#  define WRITE_MD5 	1
#  define GET_MD5		2

char *bin_md5(const char *, int);
#endif /* !_BINARY_H */
