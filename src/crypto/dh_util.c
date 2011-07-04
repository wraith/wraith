/* dh_util.c
 *
 * Adapted from ZNC-fish
 */

#include "src/libcrypto.h"
#include "src/compat/compat.h"
#include <bdlib/src/String.h>
#include <bdlib/src/base64.h>
#include "dh_util.h"

/*
   int b64toh(lpBase64String, lpDestinationBuffer);
   Converts base64 string b to hexnumber d.
   Returns size of hexnumber in bytes.
   */
int b64toh(char *b, char *d){
  int i,k,l;

  l=strlen(b);
  if (l<2) return 0;
  for (i=l-1;i>-1;i--){
    if (bd::b64_indexes[(unsigned char)(b[i])]==0) l--;
    else break;
  }

  if (l<2) return 0;
  i=0, k=0;
  while (1) {
    i++;
    if (k+1<l) d[i-1]=((bd::b64_indexes[(unsigned char)(b[k])])<<2);
    else break;
    k++;
    if (k<l) d[i-1]|=((bd::b64_indexes[(unsigned char)(b[k])])>>4);
    else break;
    i++;
    if (k+1<l) d[i-1]=((bd::b64_indexes[(unsigned char)(b[k])])<<4);
    else break;
    k++;
    if (k<l) d[i-1]|=((bd::b64_indexes[(unsigned char)(b[k])])>>2);
    else break;
    i++;
    if (k+1<l) d[i-1]=((bd::b64_indexes[(unsigned char)(b[k])])<<6);
    else break;
    k++;
    if (k<l) d[i-1]|=(bd::b64_indexes[(unsigned char)(b[k])]);
    else break;
    k++;
  }
  return i-1;
}

/*
   int htob64(lpHexNumber, lpDestinationBuffer);
   Converts hexnumber h (with length l bytes) to base64 string d.
   Returns length of base64 string.
   */
int htob64(char *h, char *d, unsigned int l){
  unsigned int i,j,k;
  unsigned char m,t;

  if (!l) return 0;
  l<<=3;                              // no. bits
  m=0x80;
  for (i=0,j=0,k=0,t=0; i<l; i++){
    if (h[(i>>3)]&m) t|=1;
    j++;
    if (!(m>>=1)) m=0x80;
    if (!(j%6)) {
      d[k]=bd::b64_charset[t];
      t&=0;
      k++;
    }
    t<<=1;
  }
  m=5-(j%6);
  t<<=m;
  if (m) {
    d[k]=bd::b64_charset[t];
    k++;
  }
  d[k]&=0;
  return strlen(d);
}


// ### new sophie-germain 1080bit prime number ###
//static const char *prime1080 = "++ECLiPSE+is+proud+to+present+latest+FiSH+release+featuring+even+more+security+for+you+++shouts+go+out+to+TMG+for+helping+to+generate+this+cool+sophie+germain+prime+number++++/C32L";
static const char *prime1080 = "FBE1022E23D213E8ACFA9AE8B9DFADA3EA6B7AC7A7B7E95AB5EB2DF858921FEADE95E6AC7BE7DE6ADBAB8A783E7AF7A7FA6A2B7BEB1E72EAE2B72F9FA2BFB2A2EFBEFAC868BADB3E828FA8BADFADA3E4CC1BE7E8AFE85E9698A783EB68FA07A77AB6AD7BEB618ACF9CA2897EB28A6189EFA07AB99A8A7FA9AE299EFA7BA66DEAFEFBEFBF0B7D8B";

// Base16: FBE1022E23D213E8ACFA9AE8B9DFADA3EA6B7AC7A7B7E95AB5EB2DF858921FEADE95E6AC7BE7DE6ADBAB8A783E7AF7A7FA6A2B7BEB1E72EAE2B72F9FA2BFB2A2EFBEFAC868BADB3E828FA8BADFADA3E4CC1BE7E8AFE85E9698A783EB68FA07A77AB6AD7BEB618ACF9CA2897EB28A6189EFA07AB99A8A7FA9AE299EFA7BA66DEAFEFBEFBF0B7D8B
// Base10: 12745216229761186769575009943944198619149164746831579719941140425076456621824834322853258804883232842877311723249782818608677050956745409379781245497526069657222703636504651898833151008222772087491045206203033063108075098874712912417029101508315117935752962862335062591404043092163187352352197487303798807791605274487594646923


void DH1080_gen(bd::String& privateKey, bd::String& publicKeyB64) {
  BIGNUM *b_prime = NULL;
  BIGNUM *b_generator = NULL;

  if (!BN_hex2bn(&b_prime, prime1080)) {
    sdprintf("BAD PRIME");
    return;
  }

  if (!BN_dec2bn(&b_generator, "2")) {
    sdprintf("BAD GENERATOR");
    return;
  }

  DH *dh = NULL;

  dh = DH_new();
  dh->p = b_prime;
  dh->g = b_generator;

  if (!DH_generate_key(dh)) {
    DH_free(dh);
    return;
  }

  // Get private key
  privateKey.resize(BN_num_bytes(dh->priv_key), 0);
  BN_bn2bin(dh->priv_key, reinterpret_cast<unsigned char*>(privateKey.mdata()));

  // Get public key
  bd::String publicKey;
  // Resize as the mdata() modification won't update the internal length, but resize() will
  publicKey.resize(static_cast<size_t>(BN_num_bytes(dh->pub_key)));
  BN_bn2bin(dh->pub_key, reinterpret_cast<unsigned char*>(publicKey.mdata()));;

  // base64 encode
  publicKeyB64 = bd::base64Encode(publicKey);

  DH_free(dh);
}

bool DH1080_comp(const bd::String privateKey, const bd::String theirPublicKeyB64, bd::String& sharedKey) {
  BIGNUM *b_prime = NULL, *b_generator = NULL;

  if (!BN_hex2bn(&b_prime, prime1080)) {
    sharedKey = "Bad prime";
    return false;
  }

  if (!BN_dec2bn(&b_generator, "2")) {
    sharedKey = "Bad generator";
    return false;
  }

  size_t len = 0;
  unsigned char raw_buf[200] = "";
  BIGNUM *b_myPrivkey = NULL, *b_HisPubkey = NULL;
  DH *dh = NULL;


  dh = DH_new();
  dh->p = b_prime;
  dh->g = b_generator;

  // Setup my private key
  b_myPrivkey = BN_bin2bn(reinterpret_cast<const unsigned char*>(privateKey.data()), privateKey.length(), NULL);
  dh->priv_key = b_myPrivkey;

  // Prep their public key
  len = theirPublicKeyB64.length();
  bd::b64dec_buf(reinterpret_cast<const unsigned char*>(theirPublicKeyB64.data()), &len, reinterpret_cast<char*>(raw_buf));
  b_HisPubkey = BN_bin2bn(reinterpret_cast<const unsigned char*>(raw_buf), len, NULL);

  // Compute the Shared key
  char *key = (char *)my_calloc(1, DH_size(dh));
  len = DH_compute_key((unsigned char *)key, b_HisPubkey, dh);
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
  unsigned char SHA256digest[SHA256_DIGEST_LENGTH] = "";

  SHA256_Init(&c);
  SHA256_Update(&c, key, len);
  SHA256_Final(SHA256digest, &c);
  memset(raw_buf, 0, sizeof(raw_buf));
  len = htob64((char *)SHA256digest, (char *)raw_buf, sizeof(SHA256digest));
  sharedKey = bd::String(reinterpret_cast<char *>(raw_buf), len);

  free(key);

  return true;
}
