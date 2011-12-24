#ifndef _TYPES_H
#define _TYPES_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <sys/types.h>
#include <netinet/in.h>

/* For local console: */
#define STDIN      0
#define STDOUT     1
#define STDERR     2

/* It's used in so many places, let's put it here */
typedef int (*Function) ();

#if !HAVE_SOCKLEN_T
typedef int socklen_t;
#endif

/* port */
typedef in_port_t		port_t;

// Signed so that it can play nice with time_t
typedef int			interval_t;

typedef struct {
  int family;
  union {
    struct in_addr addr;
#ifdef USE_IPV6
    struct in6_addr addr6;
#endif
  } u;
} my_addr_t;

#endif /* !_TYPES_H */
