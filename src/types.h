#ifndef _TYPES_H
#define _TYPES_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

/* For local console: */
#define STDIN      0
#define STDOUT     1
#define STDERR     2

/* It's used in so many places, let's put it here */
typedef int (*Function) ();

#if !HAVE_SOCKLEN_T
typedef int socklen_t;
#endif

/* 32 bit type */
#if (SIZEOF_INT == 4)
typedef unsigned int            u_32bit_t;
#else
#  if (SIZEOF_LONG == 4)
typedef unsigned long           u_32bit_t;
#  else
#    include "cant/find/32bit/type"
#  endif
#endif

typedef unsigned short int      u_16bit_t;
typedef unsigned char           u_8bit_t;

/* IP type */
typedef unsigned long		IP;
typedef u_32bit_t 		dword;

/* port */
typedef unsigned short		port_t;

#endif /* !_TYPES_H */
