#ifndef _OPENSSL_H_
#define _OPENSSL_H_

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"
#include <bdlib/src/String.h>

#include "libssl.h"
#include "libcrypto.h"

#ifdef EGG_SSL_EXT
extern SSL_CTX *ssl_ctx;
extern char *tls_rand_file;
#endif
extern int ssl_use;

int init_openssl();
int uninit_openssl();
#endif
