#ifndef _BASE64_H_
#  define _BASE64_H_

#include <sys/types.h>

char *int_to_base64(unsigned int);
int base64_to_int(char *);

char *b64enc(const unsigned char *data, size_t len);
void b64enc_buf(const unsigned char *data, size_t len, char *dest);

char *b64dec(const unsigned char *data, size_t *len);
void b64dec_buf(const unsigned char *data, size_t *len, char *dest);

#endif /* !_BASE64_H_ */
