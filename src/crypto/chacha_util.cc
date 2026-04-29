#include <openssl/evp.h>
#include <openssl/rand.h>
#include <bdlib/src/String.h>
#include <bdlib/src/base64.h>
#include <cstdlib>
#include <cstring>
#include "src/libcrypto.h"

namespace crypto {

static const size_t CHACHA_KEYLEN = 32;
static const size_t CHACHA_NONCELEN = 12;

static bd::String derive_key(const bd::String& key)
{
  if (key.length() >= CHACHA_KEYLEN)
    return bd::String(key.c_str(), CHACHA_KEYLEN);

  unsigned char full_key[CHACHA_KEYLEN] = {0};
  memcpy(full_key, key.c_str(), key.length());
  return bd::String((char*)full_key, CHACHA_KEYLEN);
}

static void chacha20_ctr(const unsigned char* key, const unsigned char* nonce,
                         const unsigned char* input, unsigned char* output, size_t len)
{
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    memcpy(output, input, len);
    return;
  }

  if (EVP_EncryptInit_ex(ctx, EVP_chacha20(), nullptr, nullptr, nullptr) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    memcpy(output, input, len);
    return;
  }

  int iv_len = EVP_CIPHER_CTX_get_iv_length(ctx);
  unsigned char* full_iv = (unsigned char*)calloc(1, iv_len);
  memcpy(full_iv + (size_t)iv_len - CHACHA_NONCELEN, nonce, CHACHA_NONCELEN);

  if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key, full_iv) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    free(full_iv);
    memcpy(output, input, len);
    return;
  }

  int outlen = 0;
  if (EVP_EncryptUpdate(ctx, output, &outlen, input, len) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    free(full_iv);
    memcpy(output, input, len);
    return;
  }

  free(full_iv);
  EVP_CIPHER_CTX_free(ctx);
}

bd::String encrypt_chacha20(const bd::String& key, const bd::String& data, const bd::String& nonce)
{
  if (!key || key.length() < 16)
    return data;

  bd::String expanded_key = derive_key(key);

  unsigned char nonce_bytes[CHACHA_NONCELEN];

  if (nonce && nonce.length() >= CHACHA_NONCELEN)
    memcpy(nonce_bytes, nonce.c_str(), CHACHA_NONCELEN);
  else if (RAND_bytes(nonce_bytes, CHACHA_NONCELEN) != 1)
    return data;

  size_t total = data.length() + CHACHA_NONCELEN;
  unsigned char* buf = (unsigned char*)malloc(total);
  chacha20_ctr((const unsigned char*)expanded_key.c_str(), nonce_bytes,
               (const unsigned char*)data.c_str(),
               buf, data.length());
  memcpy(buf + data.length(), nonce_bytes, CHACHA_NONCELEN);
  bd::String result((const char*)buf, total);
  OPENSSL_cleanse(buf, total);
  free(buf);

  return bd::base64Encode(result);
}

bd::String decrypt_chacha20(const bd::String& key, const bd::String& data)
{
  if (!key || key.length() < 16)
    return data;

  bd::String decoded = bd::base64Decode(data);

  if (decoded.length() <= CHACHA_NONCELEN)
    return data;

  bd::String expanded_key = derive_key(key);

  size_t ciphertext_len = decoded.length() - CHACHA_NONCELEN;
  if (ciphertext_len > decoded.length())
    return data;
  const unsigned char* nonce = (const unsigned char*)decoded.c_str() + ciphertext_len;

  unsigned char* buf = (unsigned char*)malloc(ciphertext_len);
  chacha20_ctr((const unsigned char*)expanded_key.c_str(), nonce,
               (const unsigned char*)decoded.c_str(),
               buf, ciphertext_len);
  bd::String plaintext((const char*)buf, ciphertext_len);
  OPENSSL_cleanse(buf, ciphertext_len);
  free(buf);

  return plaintext;
}

}
