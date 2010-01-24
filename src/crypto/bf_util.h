/* bf_util.h
 *
 */

#ifndef _BF_UTIL_H
#define _BF_UTIL_H 1

#include <sys/types.h>

namespace bd {
  class String;
}

bd::String egg_bf_encrypt(bd::String in, const bd::String& key);
bd::String egg_bf_decrypt(bd::String in, const bd::String& key);
#ifdef not_needed
unsigned char *bf_encrypt_ecb_binary(const char *, unsigned char *, size_t *);
unsigned char *bf_decrypt_ecb_binary(const char *, unsigned char *, size_t *);
unsigned char *bf_encrypt_cbc_binary(const char *, unsigned char *, size_t *, unsigned char *);
unsigned char *bf_decrypt_cbc_binary(const char *, unsigned char *, size_t *, unsigned char *);
#endif
#endif
