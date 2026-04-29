#ifndef CRYPTO_CHACHA_UTIL_H
#define CRYPTO_CHACHA_UTIL_H

#include <sys/types.h>

namespace bd {
class String;
}

namespace crypto {

bd::String encrypt_chacha20_poly1305(const bd::String& key, const bd::String& data, const bd::String& nonce);
bd::String decrypt_chacha20_poly1305(const bd::String& key, const bd::String& data);

}

#endif
