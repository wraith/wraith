#ifndef _LIBCRYPTO_H
#define _LIBCRYPTO_H

#include ".defs/libcrypto_pre.h"

#include <openssl/crypto.h>
#include <openssl/aes.h>
#include <openssl/blowfish.h>
#include <openssl/md5.h>
#include <openssl/sha.h>

#include ".defs/libcrypto_post.h"

#include "src/crypto/aes_util.h"
#include "src/crypto/bf_util.h"
#include "src/crypto/dh_util.h"

#include "common.h"
#include "dl.h"
#include <bdlib/src/String.h>

typedef void (*AES_cbc_encrypt_t)(const unsigned char*, unsigned char*, const size_t, const AES_KEY*, unsigned char*, const int);
typedef void (*AES_decrypt_t)(const unsigned char*, unsigned char*, const AES_KEY*);
typedef void (*AES_encrypt_t)(const unsigned char*, unsigned char*, const AES_KEY*);
typedef int (*AES_set_decrypt_key_t)(const unsigned char*, const int, AES_KEY*);
typedef int (*AES_set_encrypt_key_t)(const unsigned char*, const int, AES_KEY*);

typedef void (*BF_decrypt_t)(BF_LONG*, const BF_KEY*);
typedef void (*BF_encrypt_t)(BF_LONG*, const BF_KEY*);
typedef void (*BF_set_key_t)(BF_KEY*, int, const unsigned char*);

typedef char* (*ERR_error_string_t)(unsigned long, char*);
typedef void (*ERR_free_strings_t)(void);
typedef unsigned long (*ERR_get_error_t)(void);

typedef void (*OPENSSL_cleanse_t)(void*, size_t);

typedef const char* (*RAND_file_name_t)(char*, size_t);
typedef int (*RAND_load_file_t)(const char*, long);
typedef void (*RAND_seed_t)(const void*, int);
typedef int (*RAND_status_t)(void);
typedef int (*RAND_write_file_t)(const char*);

typedef int (*MD5_Final_t)(unsigned char*, MD5_CTX*);
typedef int (*MD5_Init_t)(MD5_CTX*);
typedef int (*MD5_Update_t)(MD5_CTX*, const void*, size_t);
typedef int (*SHA1_Final_t)(unsigned char*, SHA_CTX*);
typedef int (*SHA1_Init_t)(SHA_CTX*);
typedef int (*SHA1_Update_t)(SHA_CTX*, const void*, size_t);
typedef int (*SHA256_Final_t)(unsigned char*, SHA256_CTX *);
typedef int (*SHA256_Init_t)(SHA256_CTX*);
typedef int (*SHA256_Update_t)(SHA256_CTX*, const void*, size_t);

typedef BIGNUM* (*BN_bin2bn_t)(const unsigned char*, int, BIGNUM*);
typedef BIGNUM* (*BN_dup_t)(const BIGNUM*);
typedef int (*BN_bn2bin_t)(const BIGNUM*, unsigned char*);
typedef int (*BN_dec2bn_t)(BIGNUM**, const char*);
typedef int (*BN_hex2bn_t)(BIGNUM**, const char*);
typedef int (*BN_num_bits_t)(const BIGNUM*);
typedef void (*BN_clear_free_t)(BIGNUM*);

typedef int (*DH_compute_key_t)(unsigned char*, const BIGNUM*, DH*);
typedef void (*DH_free_t)(DH*);
typedef int (*DH_generate_key_t)(DH*);
typedef DH* (*DH_new_t)(void);
typedef int (*DH_size_t)(const DH*);


int load_libcrypto();
int unload_libcrypto();

#endif /* !_LIBCRYPTO_H */
