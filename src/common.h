/*
 * common.h
 *   include file to include most other include files
 *
 */

#ifndef _COMMON_H
#define _COMMON_H

/* These should be in a common.h, like it or not... */
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "garble.h"
#include "sprintf.h"
#include "conf.h"
#include "debug.h"
#include "eggdrop.h"
#include "flags.h"
#include "log.h"
#include "dccutil.h"
#include "chan.h"
#include "compat/compat.h"

#ifdef CYGWIN_HACKS
#  include <windows.h>
#endif /* CYGWIN_HACKS */
#include <sys/param.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif
#include "lang.h"


#ifdef WIN32
# undef exit
# define exit(x) ExitProcess(x)

/*
# undef system
  int my_system(const char *);		
# define system(_run) 	my_system(_run)
*/
#endif /* WIN32 */

#define BIT0    (uint32_t) 0x000000001
#define BIT1    (uint32_t) 0x000000002
#define BIT2    (uint32_t) 0x000000004
#define BIT3    (uint32_t) 0x000000008
#define BIT4    (uint32_t) 0x000000010
#define BIT5    (uint32_t) 0x000000020
#define BIT6    (uint32_t) 0x000000040
#define BIT7    (uint32_t) 0x000000080
#define BIT8    (uint32_t) 0x000000100
#define BIT9    (uint32_t) 0x000000200
#define BIT10   (uint32_t) 0x000000400
#define BIT11   (uint32_t) 0x000000800
#define BIT12   (uint32_t) 0x000001000
#define BIT13   (uint32_t) 0x000002000
#define BIT14   (uint32_t) 0x000004000
#define BIT15   (uint32_t) 0x000008000
#define BIT16   (uint32_t) 0x000010000
#define BIT17   (uint32_t) 0x000020000
#define BIT18   (uint32_t) 0x000040000
#define BIT19   (uint32_t) 0x000080000
#define BIT20   (uint32_t) 0x000100000
#define BIT21   (uint32_t) 0x000200000
#define BIT22   (uint32_t) 0x000400000
#define BIT23   (uint32_t) 0x000800000
#define BIT24   (uint32_t) 0x001000000
#define BIT25   (uint32_t) 0x002000000
#define BIT26   (uint32_t) 0x004000000
#define BIT27   (uint32_t) 0x008000000
#define BIT28   (uint32_t) 0x010000000
#define BIT29   (uint32_t) 0x020000000
#define BIT30   (uint32_t) 0x040000000
#define BIT31   (uint32_t) 0x080000000


#endif				/* _COMMON_H */
