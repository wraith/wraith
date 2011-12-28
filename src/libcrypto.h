#ifndef _LIBCRYPTO_H
#define _LIBCRYPTO_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include ".defs/libcrypto_pre.h"

#include <openssl/crypto.h>
#include <openssl/aes.h>
#include <openssl/blowfish.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include ".defs/libcrypto_post.h"

#include "src/crypto/aes_util.h"
#include "src/crypto/bf_util.h"
#include "src/crypto/dh_util.h"

#include "common.h"
#include "dl.h"
#include <bdlib/src/String.h>



int load_libcrypto();
int unload_libcrypto();

#endif /* !_LIBCRYPTO_H */
