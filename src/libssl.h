#ifndef _LIBSSL_H
#define _LIBSSL_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"
#include "dl.h"
#include <bdlib/src/String.h>

#include ".defs/libssl_pre.h"

#ifdef EGG_SSL_EXT
# ifndef EGG_SSL_INCS
#  include <openssl/ssl.h>
#  define EGG_SSL_INCS 1
# endif
#endif

typedef DH* (*dh_callback_t)(SSL*, int, int);

#include ".defs/libssl_post.h"

typedef void (*SSL_CTX_set_tmp_dh_callback_t)(SSL_CTX*, dh_callback_t);

int load_libssl();
int unload_libssl();

#endif /* !_LIBSSL_H */
