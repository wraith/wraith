/*
 * eggdrop.h
 *   Eggdrop compile-time settings
 *
 *   IF YOU ALTER THIS FILE, YOU NEED TO RECOMPILE THE BOT.
 *
 */

#ifndef _EGG_EGGDROP_H
#define _EGG_EGGDROP_H

#ifdef HAVE_OPENSSL_SSL_H
# ifndef SSL_INC
#  include <openssl/ssl.h>
#  include <openssl/rand.h>
#  include <openssl/err.h>
#  include <openssl/md5.h>
#  define SSL_INC
# endif /* ! SSL_INC */
# undef HAVE_SSL
/* #define HAVE_SSL 1 */
#endif /* HAVE_OPENSSL_SSL_H */


/*
 * Enable IPv6 support?
 */
#define USE_IPV6

/*
 * Enable IPv6 debugging?
 */
#define DEBUG_IPV6

// If you undefine this, be ready for a good novel of errors. (Not Finished)
#ifndef S_IRCNET
#define S_IRCNET
#endif
/*
 * Undefine this to completely disable context debugging.
 * WARNING: DO NOT send in bug reports if you undefine this!
 */

#define DEBUG_CONTEXT

/*
 * Set the following to the timestamp for the logfile entries.
 * Popular times might be "[%H:%M]" (hour, min), or "[%H:%M:%S]" (hour, min, sec)
 * Read `man strftime' for more formatting options.  Keep it below 32 chars.
 */
#define LOG_TS "[%H:%M]"

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

#define SGRAB 2011         /* How much data to allow through sockets. */

#define PRIV_OP 1
#define PRIV_VOICE 2

#define PRIO_DEOP 1
#define PRIO_KICK 2
#define KICK_BANNED 1
#define KICK_KUSER 2
#define KICK_KICKBAN 3
#define KICK_MASSDEOP 4
#define KICK_BADOP 5
#define KICK_BADOPPED 6
#define KICK_MANUALOP 7
#define KICK_MANUALOPPED 8
#define KICK_CLOSED 9
#define KICK_FLOOD 10
#define KICK_NICKFLOOD 11
#define KICK_KICKFLOOD 12
#define KICK_BOGUSUSERNAME 13
#define KICK_MEAN 14
#define KICK_BOGUSKEY 15

#define ERR_BINSTAT 1
#define ERR_BINMOD 2
#define ERR_PASSWD 3
#define ERR_WRONGBINDIR 4
#define ERR_CONFSTAT 5
#define ERR_TMPSTAT 6
#define ERR_CONFDIRMOD 7
#define ERR_CONFMOD 8
#define ERR_TMPMOD 9
#define ERR_NOCONF 10
#define ERR_CONFBADENC 11
#define ERR_WRONGUID 12
#define ERR_WRONGUNAME 13
#define ERR_BADCONF 14
#define ERR_MAX 15

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


/* Have to use a weird way to make the compiler error out cos not all
 * compilers support #error or error
 */
#if !HAVE_VSPRINTF
#  include "error_you_need_vsprintf_to_compile_eggdrop"
#endif

/* IPv6 sanity checks. */
#ifdef USE_IPV6
#  ifndef HAVE_IPV6
#    undef USE_IPV6
#  endif
#  ifndef HAVE_GETHOSTBYNAME2
#    ifndef HAVE_GETIPNODEBYNAME
#      undef USE_IPV6
#    endif
#  endif
#endif


#if HAVE_UNISTD_H
#  include <unistd.h>
#endif

#if !defined(STDC_HEADERS)
#  include "you_need_to_upgrade_your_compiler_to_a_standard_c_one_mate!"
#endif

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

#if !HAVE_SIGACTION		/* old "weird signals" */
#  define sigaction sigvec
#  ifndef sa_handler
#    define sa_handler sv_handler
#    define sa_mask sv_mask
#    define sa_flags sv_flags
#  endif
#endif

#if !HAVE_SIGEMPTYSET
/* and they probably won't have sigemptyset, dammit */
#  define sigemptyset(x) ((*(int *)(x))=0)
#endif

#if !HAVE_SOCKLEN_T
typedef int socklen_t;
#endif


/*
 *    Handy aliases for memory tracking and core dumps
 */

#define my_bzero(a, b) { char *x = (char *) a; int y = (int) b; while (y--) *x++ = 0; }
#define nmalloc(x)	n_malloc((x),__FILE__,__LINE__)
#define nrealloc(x,y)	n_realloc((x),(y),__FILE__,__LINE__)
#define nfree(x)	n_free((x),__FILE__,__LINE__)
#define killsock(x)	real_killsock((x),__FILE__,__LINE__)

#ifdef DEBUG_CONTEXT
#  define Context		eggContext(__FILE__, __LINE__, NULL)
#  define ContextNote(note)	eggContextNote(__FILE__, __LINE__, NULL, note)
#else
#  define Context		{}
#  define ContextNote(note)	{}
#endif

#ifdef DEBUG_ASSERT
#  define Assert(expr)	do {						\
	if (!(expr))							\
		eggAssert(__FILE__, __LINE__, NULL);			\
} while (0)
#else
#  define Assert(expr)	do {	} while (0)
#endif

#ifndef COMPILING_MEM
#  undef malloc
#  define malloc(x)	dont_use_old_malloc(x)
#  undef free
#  define free(x)	dont_use_old_free(x)
#  undef realloc
#  define realloc(x)    dont_use_old_realloc(x)
#endif /* !COMPILING_MEM */

/* 32 bit type */
#if (SIZEOF_INT == 4)
typedef unsigned int		u_32bit_t;
#else
#  if (SIZEOF_LONG == 4)
typedef unsigned long		u_32bit_t;
#  else
#    include "cant/find/32bit/type"
#  endif
#endif

typedef unsigned short int	u_16bit_t;
typedef unsigned char		u_8bit_t;

/* IP type */
typedef u_32bit_t		IP;

typedef u_32bit_t dword;

#define debug0(x)		putlog(LOG_DEBUG,"*",x)
#define debug1(x,a1)		putlog(LOG_DEBUG,"*",x,a1)
#define debug2(x,a1,a2)		putlog(LOG_DEBUG,"*",x,a1,a2)
#define debug3(x,a1,a2,a3)	putlog(LOG_DEBUG,"*",x,a1,a2,a3)
#define debug4(x,a1,a2,a3,a4)	putlog(LOG_DEBUG,"*",x,a1,a2,a3,a4)

/* These apparently are unsafe without recasting. */
#define egg_isdigit(x)  isdigit((int)  (unsigned char) (x))
#define egg_isxdigit(x) isxdigit((int) (unsigned char) (x))
#define egg_isascii(x)  isascii((int)  (unsigned char) (x))
#define egg_isspace(x)  isspace((int)  (unsigned char) (x))
#define egg_islower(x)  islower((int)  (unsigned char) (x))
#define egg_isupper(x)  isupper((int)  (unsigned char) (x))

/***********************************************************************/

/* It's used in so many places, let's put it here */
typedef int (*Function) ();

/* Public structure for the listening port map */
struct portmap {
  int realport;
  int mappedto;
  struct portmap *next;
};

/* Public structure of all the dcc connections */
struct dcc_table {
  char *name;
  int flags;
  void (*eof) (int);
  void (*activity) (int, char *, int);
  int *timeout_val;
  void (*timeout) ();
  void (*display) (int, char *);
  int (*expmem) (void *);
  void (*kill) (int, void *);
  void (*output) (int, char *, void *);
  void (*outdone) (int);
};

struct userrec;

struct auth_t {
  struct userrec *user;
  char hash[33];                /* used for dcc authing */
  char nick[NICKLEN];
  char host[UHOSTLEN];
  int authed;
  int authing;
  time_t authtime;	 /* what time they authed at */
  time_t atime;	         /* when they last were active */
};

struct dcc_t {
  long sock;			/* This should be a long to keep 64-bit
				   machines sane			 */
  IP addr;			/* IP address in host byte order	 */
#ifdef USE_IPV6
  char addr6[121];              /* easier.. ipv6 address in regular notation (3ffe:80c0:225::) */
#endif /* USE_IPV6 */
  unsigned int port;
  int ssl;			/* use ssl on this dcc? */
  struct userrec *user;
  char simulbot[NICKLEN];	/* used for hub->leaf cmd simulation, holds bot that results should be sent to */
  time_t simultime;		/* the time when the simul dcc is initiated, expires after a number of seconds */
  int simul;			/* this will hold the idx on the remote bot to return result. */
  char hash[33];                /* used for dcc authing */
  char nick[NICKLEN];
  char host[UHOSTLEN];
  struct dcc_table *type;
  time_t timeval;		/* Use for any timing stuff
				   - this is used for timeout checking	*/
  time_t pingtime;
  unsigned long status;		/* A LOT of dcc types have status
				   thingos, this makes it more avaliabe	*/
  union {
    struct chat_info *chat;
    struct file_info *file;
    struct edit_info *edit;
    struct xfer_info *xfer;
    struct bot_info *bot;
    struct relay_info *relay;
    struct script_info *script;
    struct dns_info *dns;
    struct dupwait_info *dupwait;
    int ident_sock;
    void *other;
  } u;				/* Special use depending on type	*/
};

struct chat_info {
  char *away;			/* non-NULL if user is away		*/
  int msgs_per_sec;		/* used to stop flooding		*/
  int con_flags;		/* with console: what to show		*/
  int strip_flags;		/* what codes to strip (b,r,u,c,a,g,*)	*/
  char con_chan[81];		/* with console: what channel to view	*/
  int channel;			/* 0=party line, -1=off			*/
  struct msgq *buffer;		/* a buffer of outgoing lines
				   (for .page cmd)			*/
  int max_line;			/* maximum lines at once		*/
  int line_count;		/* number of lines sent since last page	*/
  int current_lines;		/* number of lines total stored		*/
  char *su_nick;
};
#define CFGF_GLOBAL  1          /* Accessible as .config */
#define CFGF_LOCAL   2          /* Accessible as .botconfig */

typedef struct cfg_entry {
  char *name;
  int flags;
  char *gdata;
  char *ldata;
  void (*globalchanged) (struct cfg_entry *, char *oldval, int *valid);
  void (*localchanged) (struct cfg_entry *, char *oldval, int *valid);
  void (*describe) (struct cfg_entry *, int idx);
} cfg_entry_T;

struct file_info {
  struct chat_info *chat;
  char dir[161];
};

struct xfer_info {
  char *filename;
  char *origname;
  char dir[DIRLEN];		/* used when uploads go to the current dir */
  unsigned long length;
  unsigned long acked;
  char buf[4];			/* you only need 5 bytes!		   */
  unsigned char sofar;		/* how much of the byte count received	   */
  char from[NICKLEN];		/* [GET] user who offered the file	   */
  FILE *f;			/* pointer to file being sent/received	   */
  unsigned int type;		/* xfer connection type, see enum below	   */
  unsigned short ack_type;	/* type of ack				   */
  unsigned long offset;		/* offset from beginning of file, during
				   resend.				   */
  unsigned long block_pending;	/* bytes of this DCC block which weren't
				   sent yet.				   */
  time_t start_time;		/* Time when a xfer was started.	   */
};

enum {				/* transfer connection handling a ...	*/
	XFER_SEND,		/*  ... normal file-send to s.o.	*/
	XFER_RESEND,		/*  ... file-resend to s.o.		*/
	XFER_RESEND_PEND,	/*  ... (as above) and waiting for info	*/
	XFER_RESUME,		/*  ... file-send-resume to s.o.	*/
	XFER_RESUME_PEND,	/*  ... (as above) and waiting for conn */
	XFER_GET		/*  ... file-get from s.o.		*/
};

enum {
	XFER_ACK_UNKNOWN,	/* We don't know how blocks are acked.	*/
	XFER_ACK_WITH_OFFSET,	/* Skipped data is also counted as
				   received.				*/
	XFER_ACK_WITHOUT_OFFSET	/* Skipped data is NOT counted in ack.	*/
};

struct bot_info {
  char version[121];		/* channel/version info			*/
  char linker[NOTENAMELEN + 1];	/* who requested this link		*/
  int  numver;
  char sysname[121];
  int  port;			/* base port				*/
  int  uff_flags;		/* user file feature flags		*/
};

struct relay_info {
  struct chat_info *chat;
  int sock;
  int port;
  int old_status;
};

struct script_info {
  struct dcc_table *type;
  union {
    struct chat_info *chat;
    struct file_info *file;
    void *other;
  } u;
  char command[121];
};

#include <netinet/in.h>
#include <sys/socket.h>

#ifdef USE_IPV6
#define SIZEOF_SOCKADDR(so) ((so).sa.sa_family == AF_INET6 ? sizeof(so.sin6) : sizeof(so.sin))
#else
#define SIZEOF_SOCKADDR(so) (sizeof(so.sin))
#endif /* USE_IPV6 */

#if !defined(IN6_IS_ADDR_V4MAPPED) 
# define IN6_IS_ADDR_V4MAPPED(a) \
        ((((u_int32_t *) (a))[0] == 0) && (((u_int32_t *) (a))[1] == 0) && \
         (((u_int32_t *) (a))[2] == htonl (0xffff)))
#endif /* !defined(IN6_IS_ADDR_V4MAPPED) */

union sockaddr_union {
  struct sockaddr sa;
  struct sockaddr_in sin;
#ifdef USE_IPV6
  struct sockaddr_in6 sin6;
#endif /* USE_IPV6 */
};

struct dns_info {
  void (*dns_success)(int);	/* is called if the dns request succeeds   */
  void (*dns_failure)(int);	/* is called if it fails		   */
  char *host;			/* hostname				   */
  char *cbuf;			/* temporary buffer. Memory will be free'd
				   as soon as dns_info is free'd	   */
  char *cptr;			/* temporary pointer			   */
  IP ip;			/* IP address				   */
  int ibuf;			/* temporary buffer for one integer	   */
  char dns_type;		/* lookup type, e.g. RES_HOSTBYIP	   */
  struct dcc_table *type;	/* type of the dcc table we are making the
				   lookup for				   */
};

/* Flags for dns_type
 */
#define RES_HOSTBYIP  1		/* hostname to IP address		*/
#define RES_IPBYHOST  2		/* IP address to hostname		*/

struct dupwait_info {
  int atr;			/* the bots attributes			*/
  struct chat_info *chat;	/* holds current chat data		*/
};

/* Flags about dcc types
 */
#define DCT_CHAT      0x00000001	/* this dcc type receives botnet
					   chatter			    */
#define DCT_MASTER    0x00000002	/* received master chatter	    */
#define DCT_SHOWWHO   0x00000004	/* show the user in .who	    */
#define DCT_REMOTEWHO 0x00000008	/* show in remote who		    */
#define DCT_VALIDIDX  0x00000010	/* valid idx for outputting to
					   in tcl			    */
#define DCT_SIMUL     0x00000020	/* can be tcl_simul'd		    */
#define DCT_CANBOOT   0x00000040	/* can be booted		    */
#define DCT_GETNOTES  DCT_CHAT		/* can receive notes		    */
#define DCT_  	      0x00000080	/* unused */
#define DCT_FORKTYPE  0x00000100	/* a forking type		    */
#define DCT_BOT       0x00000200	/* a bot connection of some sort... */
#define DCT_FILETRAN  0x00000400	/* a file transfer of some sort	    */
#define DCT_FILESEND  0x00000800	/* a sending file transfer,
					   getting = !this		    */
#define DCT_LISTEN    0x00001000	/* a listening port of some sort    */

/* For dcc chat & files:
 */
#define STAT_ECHO    0x00001	/* echo commands back?			*/
#define STAT_DENY    0x00002	/* bad username (ignore password & deny
				   access)				*/
#define STAT_CHAT    0x00004	/* in file-system but may return	*/
#define STAT_TELNET  0x00008	/* connected via telnet			*/
#define STAT_PARTY   0x00010	/* only on party line via 'p' flag	*/
#define STAT_BOTONLY 0x00020	/* telnet on bots-only connect		*/
#define STAT_USRONLY 0x00040	/* telnet on users-only connect		*/
#define STAT_PAGE    0x00080	/* page output to the user		*/
#define STAT_COLORM  0x00100    /* show crazy colors to user mIRC */
#define STAT_COLORA  0x00200    /* show crazy colors to user ANSI */
#define STAT_COLOR   0x00400    /* Color enabled for user */

/* For stripping out mIRC codes
 */
#define STRIP_COLOR  0x00001	/* remove mIRC color codes		*/
#define STRIP_BOLD   0x00002	/* remove bold codes			*/
#define STRIP_REV    0x00004	/* remove reverse video codes		*/
#define STRIP_UNDER  0x00008	/* remove underline codes		*/
#define STRIP_ANSI   0x00010	/* remove ALL ansi codes		*/
#define STRIP_BELLS  0x00020	/* remote ctrl-g's			*/
#define STRIP_ALL    0x00040	/* remove every damn thing!		*/

/* for dcc bot links: */
#define STAT_PINGED  0x00001	/* waiting for ping to return		 */
#define STAT_SHARE   0x00002	/* sharing user data with the bot	 */
#define STAT_CALLED  0x00004	/* this bot called me			 */
#define STAT_OFFERED 0x00008	/* offered her the user file		 */
#define STAT_SENDING 0x00010	/* in the process of sending a user list */
#define STAT_GETTING 0x00020	/* in the process of getting a user list */
#define STAT_WARNED  0x00040	/* warned him about unleaflike behavior  */
#define STAT_LEAF    0x00080	/* this bot is a leaf only		 */
#define STAT_LINKING 0x00100	/* the bot is currently going through
				   the linking stage			 */
#define STAT_AGGRESSIVE   0x200	/* aggressively sharing with this bot	 */
#define STAT_OFFEREDU 0x00400
#define STAT_SENDINGU 0x00800
#define STAT_GETTINGU 0x01000
#define STAT_UPDATED  0x02000

/* Flags for listening sockets
 */
#define LSTN_PUBLIC  0x000001	/* No access restrictions		*/

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

/* For local console: */
#define STDIN      0
#define STDOUT     1
#define STDERR     2

#ifdef S_LASTCHECK
#define DETECT_LOGIN 1
#endif
#ifdef S_ANTITRACE
#define DETECT_TRACE 2
#endif
#ifdef S_PROMISC
#define DETECT_PROMISC 3
#endif
#ifdef S_PROCESSCHECK
#define DETECT_PROCESS 4
#endif
#ifdef S_HIJACKCHECK
#define DETECT_SIGCONT 5
#endif

#define DET_IGNORE 0
#define DET_WARN 1
#define DET_REJECT 2
#define DET_DIE 3
#define DET_SUICIDE 4
#define DET_NOCHECK 5

#define CONNECT_SSL 1
#define ACCEPT_SSL 2

/* Structure for internal logs */
typedef struct {
  char *filename;
  unsigned int mask;		/* what to send to this log		    */
  char *chname;			/* which channel			    */
  char szlast[LOGLINELEN];	/* for 'Last message repeated n times'
				   stuff in misc.c/putlog() <cybah>	    */
  int repeats;			/* number of times szLast has been repeated */
  unsigned int flags;		/* other flags <rtc>			    */
  FILE *f;			/* existing file			    */
} log_t;

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

/* Internal logfile flags
 */
#define LF_EXPIRING 0x000001	/* Logfile will be closed soon		*/

#define FILEDB_HIDE     1
#define FILEDB_UNHIDE   2
#define FILEDB_SHARE    3
#define FILEDB_UNSHARE  4

/* Socket flags:
 */
#define SOCK_UNUSED     0x0001	/* empty socket				*/
#define SOCK_BINARY     0x0002	/* do not buffer input			*/
#define SOCK_LISTEN     0x0004	/* listening port			*/
#define SOCK_CONNECT    0x0008	/* connection attempt			*/
#define SOCK_NONSOCK    0x0010	/* used for file i/o on debug		*/
#define SOCK_STRONGCONN 0x0020	/* don't report success until sure	*/
#define SOCK_EOFD       0x0040	/* it EOF'd recently during a write	*/
#define SOCK_PROXYWAIT	0x0080	/* waiting for SOCKS traversal		*/
#define SOCK_PASS	0x0100	/* passed on; only notify in case
				   of traffic				*/
#define SOCK_VIRTUAL	0x0200	/* not-connected socket (dont read it!)	*/
#define SOCK_BUFFER	0x0400	/* buffer data; don't notify dcc funcs	*/

/* Flags to sock_has_data
 */
enum {
  SOCK_DATA_OUTGOING,		/* Data in out-queue?			*/
  SOCK_DATA_INCOMING		/* Data in in-queue?			*/
};

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

#define NORMAL          0
#define QUICK           1

/* Return codes for add_note */
#define NOTE_ERROR      0	/* error			*/
#define NOTE_OK         1	/* success			*/
#define NOTE_STORED     2	/* not online; stored		*/
#define NOTE_FULL       3	/* too many notes stored	*/
#define NOTE_TCL        4	/* tcl binding caught it	*/
#define NOTE_AWAY       5	/* away; stored			*/
#define NOTE_FWD        6	/* away; forwarded		*/
#define NOTE_REJECT     7	/* ignore mask matched		*/

#define STR_PROTECT     2
#define STR_DIR         1

#define HELP_DCC        1
#define HELP_TEXT       2
#define HELP_IRC        16

/* This is used by the net module to keep track of sockets and what's
 * queued on them
 */
#define SHA_HASH_LENGTH (SHA_DIGEST_LENGTH * 2)
#define MD5_HASH_LENGTH (MD5_DIGEST_LENGTH * 2)
typedef struct {
  int		 sock;
  short		 flags;
  char		*inbuf;
  char		*outbuf;
  unsigned long  outbuflen;		/* Outbuf could be binary data	*/
  int encstatus;			/* encrypted botlink */
  int oseed;				/* botlink out seed */
  int iseed;				/* botlink in seed */
  char okey[33];			/* botlink enckey: out */
  char ikey[33];			/* botlink enckey: in  */
  int gz; /* gzip compression */
  unsigned long	 inbuflen;		/* Inbuf could be binary data	*/
#ifdef USE_IPV6
  unsigned int af;
#endif /* USE_IPV6 */
#ifdef HAVE_SSL
  SSL 		*ssl;
#endif /* HAVE_SSL */
} sock_list;

#ifdef S_DCCPASS
typedef struct cmd_pass {
  struct cmd_pass *next;
  char *name;
  char pass[25];
} cmd_pass_t;
#endif

enum {
  EGG_OPTION_SET	= 1,	/* Set option(s).		*/
  EGG_OPTION_UNSET	= 2	/* Unset option(s).		*/
};

/* Telnet codes.  See "TELNET Protocol Specification" (RFC 854) and
 * "TELNET Echo Option" (RFC 875) for details.
 */

#define TLN_AYT		246		/* Are You There	*/
#define TLN_WILL	251		/* Will			*/
#define TLN_WILL_C	"\373"
#define TLN_WONT	252		/* Won't		*/
#define TLN_WONT_C	"\374"
#define TLN_DO		253		/* Do			*/
#define TLN_DO_C	"\375"
#define TLN_DONT	254		/* Don't		*/
#define TLN_DONT_C	"\376"
#define TLN_IAC		255		/* Interpret As Command	*/
#define TLN_IAC_C	"\377"
#define TLN_ECHO	1		/* Echo			*/
#define TLN_ECHO_C	"\001"

#endif				/* _EGG_EGGDROP_H */
