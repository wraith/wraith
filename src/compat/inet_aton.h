/*
 * inet_aton.h
 *   prototypes for inet_aton.c
 *
 */

#ifndef _EGG_COMPAT_INET_ATON_H
#define _EGG_COMPAT_INET_ATON_H

#include "src/eggmain.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef HAVE_INET_ATON
/* Use our own implementation. */
int egg_inet_aton(const char *cp, struct in_addr *addr);
#else
#  define egg_inet_aton	inet_aton
#endif

#endif	/* !__EGG_COMPAT_INET_ATON_H */
