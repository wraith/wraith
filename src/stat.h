/*
 * stat.h
 *  file attributes
 *
 */

#ifndef _EGG_STAT_H
#define _EGG_STAT_H

#include <sys/stat.h>

#ifndef S_ISDIR
#  ifndef S_IFMT
#    define S_IFMT	0170000	    /* Bitmask for the file type bitfields */
#  endif
#  ifndef S_IFDIR
#    define S_IFDIR	0040000	    /* Directory			   */
#  endif
#  define S_ISDIR(m)	(((m)&(S_IFMT)) == (S_IFDIR))
#endif
#ifndef S_IFREG
#  define S_IFREG	0100000     /* Regular file			   */
#endif
#ifndef S_IFLNK
#  define S_IFLNK   	0120000     /* Symbolic link			   */
#endif

#endif			/* _EGG_STAT_H */
