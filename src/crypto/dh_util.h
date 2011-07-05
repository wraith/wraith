/* dh_util.h
 *
 */

#ifndef _DH_UTIL_H
#define _DH_UTIL_H 1

#include <sys/types.h>
#include <openssl/dh.h>

namespace bd {
  class String;
}

// Adapated from znc-fish
bd::String fishBase64Encode(const bd::String& str);
bd::String fishBase64Decode(const bd::String& str);
void DH1080_gen(bd::String& privateKey, bd::String& publicKeyB64);
bool DH1080_comp(const bd::String privateKey, const bd::String theirPublicKeyB64, bd::String& sharedKey);
void DH1080_init();
#endif
