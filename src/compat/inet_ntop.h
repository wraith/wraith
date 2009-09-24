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

#ifndef HAVE_INET_NTOP
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
#endif

#endif /* !_EGG_COMPAT_INET_NTOP_H */
