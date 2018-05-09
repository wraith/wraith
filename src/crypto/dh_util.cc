/* dh_util.c
 *
 * Adapted from ZNC-fish
 */

#include "src/libcrypto.h"
#include "src/compat/compat.h"
#include <bdlib/src/String.h>
#include <bdlib/src/base64.h>
#include "dh_util.h"

static BIGNUM* b_prime = NULL;
static BIGNUM* b_generator = NULL;

void DH1080_init() {
  // ### new sophie-germain 1080bit prime number ###
  //const char *prime1080 = "++ECLiPSE+is+proud+to+present+latest+FiSH+release+featuring+even+more+security+for+you+++shouts+go+out+to+TMG+for+helping+to+generate+this+cool+sophie+germain+prime+number++++/C32L";
  // Base16: FBE1022E23D213E8ACFA9AE8B9DFADA3EA6B7AC7A7B7E95AB5EB2DF858921FEADE95E6AC7BE7DE6ADBAB8A783E7AF7A7FA6A2B7BEB1E72EAE2B72F9FA2BFB2A2EFBEFAC868BADB3E828FA8BADFADA3E4CC1BE7E8AFE85E9698A783EB68FA07A77AB6AD7BEB618ACF9CA2897EB28A6189EFA07AB99A8A7FA9AE299EFA7BA66DEAFEFBEFBF0B7D8B
  // Base10: 12745216229761186769575009943944198619149164746831579719941140425076456621824834322853258804883232842877311723249782818608677050956745409379781245497526069657222703636504651898833151008222772087491045206203033063108075098874712912417029101508315117935752962862335062591404043092163187352352197487303798807791605274487594646923
  const char *prime1080 = "FBE1022E23D213E8ACFA9AE8B9DFADA3EA6B7AC7A7B7E95AB5EB2DF858921FEADE95E6AC7BE7DE6ADBAB8A783E7AF7A7FA6A2B7BEB1E72EAE2B72F9FA2BFB2A2EFBEFAC868BADB3E828FA8BADFADA3E4CC1BE7E8AFE85E9698A783EB68FA07A77AB6AD7BEB618ACF9CA2897EB28A6189EFA07AB99A8A7FA9AE299EFA7BA66DEAFEFBEFBF0B7D8B";

  if (!BN_hex2bn(&b_prime, prime1080)) {
    sdprintf("BAD PRIME");
    return;
  }

  if (!BN_dec2bn(&b_generator, "2")) {
    sdprintf("BAD GENERATOR");
    return;
  }
}

void DH1080_uninit() {
  BN_clear_free(b_prime);
  BN_clear_free(b_generator);
}

/**
 * @brief Encode a string using FiSH's base64 algorithm (from FiSH/mIRC)
 * @note Any = padding is removed, and an 'A' is added if no padding was needed
 * @param bd::String str The string to encode
 * @returns Encoded string
 * @note Adapated from FiSH code
 */
bd::String fishBase64Encode(const bd::String& str) {
  bd::String result(bd::base64Encode(str));

  // No padding, add an A on the end (base64-encoded NULL-terminator)
  if (result.rfind('=') == result.npos) {
    result += 'A';
  } else {
    // Remove padding
    while (result.rfind('=') != result.npos) {
      --result;
    }
  }
  return result;
}

/**
 * @brief Decode a string using FiSH's base64 algorithm (from FiSH/mIRC)
 * @param bd::String str The string to decode
 * @returns Decoded data
 * @note Adapated from FiSH code
 */
bd::String fishBase64Decode(const bd::String& str) {
  bd::String temp(str);

  // Remove the 'A' NULL-terminator if present
  if (temp.length() % 4 == 1 && temp(-1, 1) == 'A') {
    --temp;
  }

  while (temp.length() % 4) {
    temp += '=';
  }

  return bd::base64Decode(temp);
}


void DH1080_gen(bd::String& privateKey, bd::String& publicKeyB64) {
  DH *dh = NULL;
  const BIGNUM *priv_key, *pub_key;

  dh = DH_new();
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
  if (b_prime == NULL || b_generator == NULL ||
      !DH_set0_pqg(dh, BN_dup(b_prime), NULL, BN_dup(b_generator)))
    return;
#else
  dh->p = BN_dup(b_prime);
  dh->g = BN_dup(b_generator);
#endif

  if (!DH_generate_key(dh)) {
    DH_free(dh);
    return;
  }

  // Get private key
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
  DH_get0_key(dh, &pub_key, &priv_key);
#else
  priv_key = dh->priv_key;
  pub_key = dh->pub_key;
#endif
  privateKey.resize(BN_num_bytes(priv_key), 0);
  BN_bn2bin(priv_key, reinterpret_cast<unsigned char*>(privateKey.mdata()));

  // Get public key
  bd::String publicKey;
  // Resize as the mdata() modification won't update the internal length, but resize() will
  publicKey.resize(static_cast<size_t>(BN_num_bytes(pub_key)));
  BN_bn2bin(pub_key, reinterpret_cast<unsigned char*>(publicKey.mdata()));;

  // base64 encode
  publicKeyB64 = fishBase64Encode(publicKey);

  DH_free(dh);
}

bool DH1080_comp(const bd::String privateKey, const bd::String theirPublicKeyB64, bd::String& sharedKey) {
  BIGNUM *b_myPrivkey = NULL, *b_HisPubkey = NULL;
  DH *dh = NULL;


  dh = DH_new();
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
  if (b_prime == NULL || b_generator == NULL ||
      !DH_set0_pqg(dh, BN_dup(b_prime), NULL, BN_dup(b_generator)))
    return false;
#else
  dh->p = BN_dup(b_prime);
  dh->g = BN_dup(b_generator);
#endif

  // Setup my private key
  b_myPrivkey = BN_bin2bn(reinterpret_cast<const unsigned char*>(privateKey.data()), privateKey.length(), NULL);
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
  DH_set0_key(dh, NULL, b_myPrivkey);
#else
  dh->priv_key = b_myPrivkey;
#endif

  // Prep their public key
  bd::String theirPublicKey(fishBase64Decode(theirPublicKeyB64));
  b_HisPubkey = BN_bin2bn(reinterpret_cast<const unsigned char*>(theirPublicKey.data()), theirPublicKey.length(), NULL);

  // Compute the Shared key
  char *key = (char *)calloc(1, DH_size(dh));
  size_t len = DH_compute_key((unsigned char *)key, b_HisPubkey, dh);
  DH_free(dh);
  BN_clear_free(b_HisPubkey);
  if (len == static_cast<size_t>(-1)) {
    // Bad pub key
    unsigned long err = ERR_get_error();
    sdprintf("** DH Error: %s", ERR_error_string(err, NULL));
    free(key);

    sharedKey = ERR_error_string(err, NULL);
    return false;
  }

  SHA256_CTX c;
  bd::String SHA256Digest(static_cast<size_t>(SHA256_DIGEST_LENGTH));
  SHA256Digest.resize(SHA256_DIGEST_LENGTH);

  SHA256_Init(&c);
  SHA256_Update(&c, key, len);
  SHA256_Final(reinterpret_cast<unsigned char*>(SHA256Digest.mdata()), &c);
  sharedKey = fishBase64Encode(SHA256Digest);

  free(key);

  return true;
}
/* vim: set sts=2 sw=2 ts=8 et: */
