/*
 * inet_ntop.h --
 *
 *	prototypes for inet_ntop.c
 */

#ifndef _EGG_COMPAT_INET_NTOP_H
#define _EGG_COMPAT_INET_NTOP_H

#include "src/common.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef USE_IPV6
#  ifndef HAVE_INET_NTOP
const char *egg_inet_ntop(int af, const void *src, char *dst, socklen_t size);
#  else
#    define egg_inet_ntop inet_ntop
#  endif
#else
#  define egg_inet_ntop 0
#endif

#endif /* !_EGG_COMPAT_INET_NTOP_H */
