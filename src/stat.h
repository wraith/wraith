#ifndef _EGG_STAT_H
#define _EGG_STAT_H
#ifndef S_ISDIR
#ifndef S_IFMT
#define S_IFMT	0170000
#endif
#ifndef S_IFDIR
#define S_IFDIR	0040000
#endif
#define S_ISDIR(m)	(((m)&(S_IFMT)) == (S_IFDIR))
#endif
#ifndef S_IFREG
#define S_IFREG	0100000
#endif
#ifndef S_IFLNK
#define S_IFLNK	0120000
#endif
#endif
