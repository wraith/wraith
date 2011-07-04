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
void DH1080_gen(bd::String& privateKey, bd::String& publicKeyB64);
bool DH1080_comp(const bd::String privateKey, const bd::String theirPublicKeyB64, bd::String& sharedKey);
#endif
