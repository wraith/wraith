#ifndef _BASE64_H_
#  define _BASE64_H_

#include <sys/types.h>
namespace bd {
  class String;
};

char *int_to_base64(unsigned int);
int base64_to_int(const char *) __attribute__((pure));

bd::String broken_base64Encode(const bd::String&);
char *b64enc(const unsigned char *data, size_t len);

bd::String broken_base64Decode(const bd::String&);
char *b64dec(const unsigned char *data, size_t *len);

#endif /* !_BASE64_H_ */
