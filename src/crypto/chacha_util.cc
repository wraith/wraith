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
static const size_t CHACHA_TAGLEN = 16;

static bd::String derive_key(const bd::String& key)
{
  if (key.length() >= CHACHA_KEYLEN)
    return bd::String(key.c_str(), CHACHA_KEYLEN);

  unsigned char full_key[CHACHA_KEYLEN] = {0};
  memcpy(full_key, key.c_str(), key.length());
  return bd::String((char*)full_key, CHACHA_KEYLEN);
}

bd::String encrypt_chacha20_poly1305(const bd::String& key, const bd::String& data, const bd::String& nonce)
{
  if (!key || key.length() < 16)
    return data;

  bd::String expanded_key = derive_key(key);

  unsigned char nonce_bytes[CHACHA_NONCELEN];
  if (nonce && nonce.length() >= CHACHA_NONCELEN)
    memcpy(nonce_bytes, nonce.c_str(), CHACHA_NONCELEN);
  else if (RAND_bytes(nonce_bytes, CHACHA_NONCELEN) != 1)
    return data;

  const EVP_CIPHER *cipher = EVP_get_cipherbyname("chacha20-poly1305");
  if (!cipher)
    return bd::String();

  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx)
    return data;

  if (EVP_EncryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return data;
  }

  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, CHACHA_NONCELEN, nullptr) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return data;
  }

  if (EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                         (const unsigned char*)expanded_key.c_str(), nonce_bytes) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return data;
  }

  int outlen = 0;
  unsigned char *outbuf = (unsigned char*)malloc(data.length() + CHACHA_TAGLEN);
  if (!outbuf) {
    EVP_CIPHER_CTX_free(ctx);
    return data;
  }

  if (EVP_EncryptUpdate(ctx, outbuf, &outlen,
                        (const unsigned char*)data.c_str(), data.length()) != 1) {
    free(outbuf);
    EVP_CIPHER_CTX_free(ctx);
    return data;
  }

  int finlen = 0;
  if (EVP_EncryptFinal_ex(ctx, outbuf + outlen, &finlen) != 1) {
    free(outbuf);
    EVP_CIPHER_CTX_free(ctx);
    return data;
  }

  unsigned char tag[CHACHA_TAGLEN];
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, CHACHA_TAGLEN, tag) != 1) {
    free(outbuf);
    EVP_CIPHER_CTX_free(ctx);
    return data;
  }

  EVP_CIPHER_CTX_free(ctx);

  size_t total = CHACHA_NONCELEN + outlen + finlen + CHACHA_TAGLEN;
  unsigned char *wire = (unsigned char*)malloc(total);
  if (!wire) {
    free(outbuf);
    return data;
  }

  memcpy(wire, nonce_bytes, CHACHA_NONCELEN);
  memcpy(wire + CHACHA_NONCELEN, outbuf, outlen + finlen);
  memcpy(wire + CHACHA_NONCELEN + outlen + finlen, tag, CHACHA_TAGLEN);
  free(outbuf);

  bd::String result((const char*)wire, total);
  OPENSSL_cleanse(wire, total);
  free(wire);

  return bd::base64Encode(result);
}

bd::String decrypt_chacha20_poly1305(const bd::String& key, const bd::String& data)
{
  if (!key || key.length() < 16)
    return data;

  bd::String decoded = bd::base64Decode(data);

  if (decoded.length() <= CHACHA_NONCELEN + CHACHA_TAGLEN)
    return bd::String();

  bd::String expanded_key = derive_key(key);

  size_t ciphertext_len = decoded.length() - CHACHA_NONCELEN - CHACHA_TAGLEN;
  if (ciphertext_len > decoded.length())
    return bd::String();

  const unsigned char* nonce = (const unsigned char*)decoded.c_str();
  const unsigned char* ciphertext = nonce + CHACHA_NONCELEN;
  const unsigned char* tag = ciphertext + ciphertext_len;

  const EVP_CIPHER *cipher = EVP_get_cipherbyname("chacha20-poly1305");
  if (!cipher)
    return bd::String();

  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx)
    return bd::String();

  if (EVP_DecryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return bd::String();
  }

  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, CHACHA_NONCELEN, nullptr) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return bd::String();
  }

  if (EVP_DecryptInit_ex(ctx, nullptr, nullptr,
                         (const unsigned char*)expanded_key.c_str(), nonce) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return bd::String();
  }

  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, CHACHA_TAGLEN, (void*)tag) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return bd::String();
  }

  unsigned char *outbuf = (unsigned char*)malloc(ciphertext_len + 1);
  if (!outbuf) {
    EVP_CIPHER_CTX_free(ctx);
    return bd::String();
  }

  int outlen = 0;
  if (EVP_DecryptUpdate(ctx, outbuf, &outlen, ciphertext, ciphertext_len) != 1) {
    free(outbuf);
    EVP_CIPHER_CTX_free(ctx);
    return bd::String();
  }

  int finlen = 0;
  if (EVP_DecryptFinal_ex(ctx, outbuf + outlen, &finlen) != 1) {
    free(outbuf);
    EVP_CIPHER_CTX_free(ctx);
    return bd::String();
  }

  EVP_CIPHER_CTX_free(ctx);

  bd::String plaintext((const char*)outbuf, outlen + finlen);
  OPENSSL_cleanse(outbuf, ciphertext_len + 1);
  free(outbuf);

  return plaintext;
}

}
