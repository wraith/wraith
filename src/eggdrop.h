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

#define HANDLEN		  9	/* valid values 9->NICKMAX		*/
#define NICKMAX		 32	/* valid values HANDLEN->32		*/


/* Handy string lengths */

#define UHOSTMAX    291 + NICKMAX /* 32 (ident) + 3 (\0, !, @) + NICKMAX */
#define DIRMAX		512	/* paranoia				*/
#define LOGLINEMAX	867	/* for misc.c/putlog() <cybah>		*/
#define BADHANDCHARS	"-,+*=:!.@#;$%&"

#define MAX_BOTS     500
#define SERVLEN      60

/*
 *     The 'configure' script should make this next part automatic,
 *     so you shouldn't need to adjust anything below.
 */

#define NICKLEN         NICKMAX + 1
#define UHOSTLEN        UHOSTMAX + 1
#define DIRLEN          DIRMAX + 1
#define LOGLINELEN	LOGLINEMAX + 1
#define NOTENAMELEN     ((HANDLEN * 2) + 1)
#define BADNICKCHARS	"-,+*=:!.@#;$%&"

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


/***********************************************************************/



/* chan & global */
#define FLOOD_PRIVMSG    0
#define FLOOD_NOTICE     1
#define FLOOD_CTCP       2
#define FLOOD_NICK       3
#define FLOOD_JOIN       4
#define FLOOD_KICK       5
#define FLOOD_DEOP       6

#define FLOOD_CHAN_MAX   7
#define FLOOD_GLOBAL_MAX 3


/* Logfile display flags
 */
#define LOG_MSGS     0x000001	/* m   msgs/notice/ctcps		*/
#define LOG_PUBLIC   0x000002	/* p   public msg/notice/ctcps		*/
#define LOG_JOIN     0x000004	/* j   channel joins/parts/etc		*/
#define LOG_MODES    0x000008	/* k   mode changes/kicks/bans		*/
#define LOG_CMDS     0x000010	/* c   user dcc or msg commands		*/
#define LOG_MISC     0x000020	/* o   other misc bot things		*/
#define LOG_BOTS     0x000040	/* b   bot notices			*/
#define LOG_RAW      0x000080	/* r   raw server stuff coming in	*/
#define LOG_FILES    0x000100	/* x   file transfer commands and stats	*/
#define LOG_ERRORS   0x000200	/* e   misc errors               	*/
#define LOG_ERROR    0x000200	/* e   misc errors               	*/
#define LOG_GETIN    0x000400	/* g   op system. (Getin)			*/
#define LOG_WARN     0x000800	/* u   warnings			*/
#define LOG_WARNING  0x000800	/* u   warnings			*/

//the rest of these can be used for new console modes....
#define LOG_LEV4     0x001000	/* 4   user log level			*/
#define LOG_LEV5     0x002000	/* 5   user log level			*/
#define LOG_LEV6     0x004000	/* 6   user log level			*/
#define LOG_LEV7     0x008000	/* 7   user log level			*/
#define LOG_LEV8     0x010000	/* 8   user log level			*/
#define LOG_SERV     0x020000	/* s   server information		*/
#define LOG_DEBUG    0x040000	/* d   debug				*/
#define LOG_WALL     0x080000	/* w   wallops				*/
#define LOG_SRVOUT   0x100000	/* v   server output			*/
#define LOG_BOTNET   0x200000	/* t   botnet traffic			*/
#define LOG_BOTSHARE 0x400000	/* h   share traffic			*/
#define LOG_ALL      0x7fffff	/* (dump to all logfiles)		*/


/* Fake idx's for dprintf - these should be ridiculously large +ve nums
 */
#define DP_STDOUT       0x7FF1
#define DP_LOG          0x7FF2
#define DP_SERVER       0x7FF3
#define DP_HELP         0x7FF4
#define DP_STDERR       0x7FF5
#define DP_MODE         0x7FF6
#define DP_MODE_NEXT    0x7FF7
#define DP_SERVER_NEXT  0x7FF8
#define DP_HELP_NEXT    0x7FF9

#endif				/* _EGG_EGGDROP_H */
