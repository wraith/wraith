/*
 * eggdrop.h
 *   Eggdrop compile-time settings
 *
 *   IF YOU ALTER THIS FILE, YOU NEED TO RECOMPILE THE BOT.
 *
 */

#ifndef _EGG_EGGDROP_H
#define _EGG_EGGDROP_H

/*

 * HANDLEN note:
 *       HANDLEN defines the maximum length a handle on the bot can be.
 *       Standard (and minimum) is 9 characters long.
 *
 *       Beware that using lengths over 9 chars is 'non-standard' and if
 *       you wish to link to other bots, they _must_ both have the same
 *       maximum handle length.
 *
 * NICKMAX note:
 *       You should leave this at 32 characters and modify nick-len in the
 *       configuration file instead.
 */

#define HANDLEN		 32	/* valid values 9->NICKMAX		*/
#define NICKMAX		 32	/* valid values HANDLEN->32		*/

#define MAX_SOCKETS	300


/* Handy string lengths */

#define SALT1LEN	32
#define SALT2LEN	16

#define MAXPASSLEN      15
#define PACKNAMELEN     32

#define UHOSTMAX    291 + NICKMAX /* 32 (ident) + 3 (\0, !, @) + NICKMAX */
#define DIRMAX		1024	/* paranoia				*/
#define BADHANDCHARS	"-,+*=:!.@#;$%&"

#define MAX_BOTS     500
#define SERVLEN      60

#define RESULT_LEN 	1024

/*
 *     The 'configure' script should make this next part automatic,
 *     so you shouldn't need to adjust anything below.
 */

#define NICKLEN         NICKMAX + 1
#define UHOSTLEN        UHOSTMAX + 1
#define DIRLEN          DIRMAX + 1
#define NOTENAMELEN     ((HANDLEN << 1) + 1)
#define BADPASSCHARS    "-+"
#define LISTSEPERATORS  ",=:; "
#define BADREPEATEDRAND "*"
#define RANDSPECIAL     "#*+,-./;<=>?[]^_{}|"
#define RANDSPECIALLEN  19

#define RELEASE_TIME	8

#define ESCAPESHELL	"`\\|#*()[]& "

#if (NICKMAX < 9) || (NICKMAX > 32)
#  include "invalid NICKMAX value"
#endif

#if (HANDLEN < 9) || (HANDLEN > 32)
#  include "invalid HANDLEN value"
#endif

#if HANDLEN > NICKMAX
#  include "HANDLEN MUST BE <= NICKMAX"
#endif

/* NAME_MAX is what POSIX defines, but BSD calls it MAXNAMLEN.
 * Use 255 if we can't find anything else.
 */
#ifndef NAME_MAX
#  ifdef MAXNAMLEN
#    define NAME_MAX	MAXNAMLEN
#  else
#    define NAME_MAX	255
#  endif
#endif

/* Almost every module needs some sort of time thingy, so... */
#if TIME_WITH_SYS_TIME
#  include <sys/time.h>
#  include <time.h>
#else
#  if HAVE_SYS_TIME_H
#    include <sys/time.h>
#  else
#    include <time.h>
#  endif
#endif


#if !HAVE_SRANDOM
#  define srandom(x) srand(x)
#endif

#if !HAVE_RANDOM
#  define random() (rand()/16)
#endif

/* Use high-order bits for getting the random integer. With random()
 * modulo would probably be sufficient but on systems lacking random(),
 * the function will be just renamed rand().
 */
//#define randint(n) (unsigned long) (random() / (RANDOM_MAX + 1.0) * n)
#define randint(n) (unsigned long) (random() / (RANDOM_MAX + 1.0) * ((signed) (n) < 0 ? (signed) (-(n)) : (signed) (n)))


/***********************************************************************/

enum {		/* TAKE A GUESS */
  OK,
  ERROR
};

#define FEATURE_1		BIT0
#define FEATURE_2		BIT1

#define HAVE_F1	((conf.features & FEATURE_1))
#define HAVE_F2	((conf.features & FEATURE_2))
#define HAVE_TAKE (1)
#define HAVE_MDOP (1)


#define HUB		BIT0
#define LEAF		BIT1
#define AUTH		BIT2
#define AUTH_CHAN	BIT3
#define AUTH_MSG	BIT4
#define AUTH_NOT	BIT5
#define AUTH_ALL	(AUTH|AUTH_CHAN|AUTH_MSG|AUTH_NOT)
#define have_cmd(name, flags) ((!is_restricted_cmd(name)) && (!(flags & (HUB|LEAF)) || (flags & HUB && conf.bot->hub) || (flags & LEAF && !conf.bot->hub)))


#define FEATURE_ROLES		BIT0
#define ALL_FEATURE_FLAGS	(FEATURE_ROLES)

#endif				/* _EGG_EGGDROP_H */
