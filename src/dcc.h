#ifndef _DCC_H
#define _DCC_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "types.h"
#include "crypt.h"
#include "eggdrop.h"
#include "dns.h"

/* Public structure of all the dcc connections */
struct dcc_table {
  char *name;
  int flags;
  void (*eof) (int);
  void (*activity) (int, char *, int);
  int *timeout_val;
  void (*timeout) ();
  void (*display) (int, char *);
  void (*kill) (int, void *);
  void (*output) (int, char *, void *);
  void (*outdone) (int);
};

struct dcc_t {
  struct dcc_table *type;
  struct userrec *user;
  union {
    struct chat_info *chat;
    struct file_info *file;
    struct edit_info *edit;
    struct xfer_info *xfer;
    struct bot_info *bot;
    struct relay_info *relay;
    struct dns_info *dns;
    struct dupwait_info *dupwait;
    int ident_sock;
    void *other;
  } u;                          /* Special use depending on type        */

  IP addr;                      /* IP address in host byte order         */
  time_t simultime;             /* the time when the simul dcc is initiated, expires after a number of seconds */
  time_t timeval;               /* Use for any timing stuff
                                   - this is used for timeout checking  */
  time_t pingtime;
  unsigned long status;         /* A LOT of dcc types have status
                                   thingos, this makes it more avaliabe */
  long sock;                    /* This should be a long to keep 64-bit
                                   machines sane                         */
  int ssl;                      /* use ssl on this dcc? */
  int simul;                    /* this will hold the idx on the remote bot to return result. */
  int msgc;			/* forward the output back to irc? */
  int whowas;
  port_t port;
#ifdef USE_IPV6
  char addr6[121];              /* easier.. ipv6 address in regular notation (3ffe:80c0:225::) */
#endif /* USE_IPV6 */
  char simulbot[NICKLEN];       /* used for hub->leaf cmd simulation, holds bot that results should be sent to */
  char hash[MD5_HASH_LENGTH + 1];                /* used for dcc authing */
  char nick[NICKLEN];
  char whois[UHOSTLEN];
  char host[UHOSTLEN];
};


struct chat_info {
  struct msgq *buffer;          /* a buffer of outgoing lines
                                   (for .page cmd)                      */
  int max_line;                 /* maximum lines at once                */
  int line_count;               /* number of lines sent since last page */
  int current_lines;            /* number of lines total stored         */
  int msgs_per_sec;             /* used to stop flooding                */
  int con_flags;                /* with console: what to show           */
  int strip_flags;              /* what codes to strip (b,r,u,c,a,g,*)  */
  int channel;                  /* 0=party line, -1=off                 */
  char *away;                   /* non-NULL if user is away             */
  char *su_nick;
  char con_chan[81];            /* with console: what channel to view   */
};

struct file_info {
  struct chat_info *chat;
  char dir[161];
};

struct xfer_info {
  FILE *f;                      /* pointer to file being sent/received     */
  unsigned long int length;
  unsigned long int acked;
  unsigned long int offset;     /* offset from beginning of file, during
                                   resend.                                 */
  unsigned long int block_pending;  	/* bytes of this DCC block which weren't
                                   sent yet.                               */
  unsigned int type;            /* xfer connection type, see enum below    */
  time_t start_time;            /* Time when a xfer was started.           */
  char *filename;
  char *origname;
  unsigned short ack_type;      /* type of ack                             */
  unsigned char sofar;          /* how much of the byte count received     */
  char dir[DIRLEN];             /* used when uploads go to the current dir */
  char from[NICKLEN];           /* [GET] user who offered the file         */
  char buf[4];                  /* you only need 5 bytes!                  */
};

struct bot_info {
  int  numver;
  int  uff_flags;               /* user file feature flags              */
  port_t port;		        /* base port                            */
  char linker[NOTENAMELEN + 1]; /* who requested this link              */
  char sysname[121];
  char version[121];            /* channel/version info                 */
};

struct relay_info {
  struct chat_info *chat;
  long sock;
  int old_status;
  port_t port;
};

struct dupwait_info {
  struct chat_info *chat;       /* holds current chat data              */
  int atr;                      /* the bots attributes                  */
};


/* Flags about dcc types
 */
#define DCT_CHAT      BIT0        /* this dcc type receives botnet
                                           chatter                          */
#define DCT_GETNOTES  DCT_CHAT          /* can receive notes                */
#define DCT_MASTER    BIT1        /* received master chatter          */
#define DCT_SHOWWHO   BIT2        /* show the user in .who            */
#define DCT_REMOTEWHO BIT3        /* show in remote who               */
#define DCT_VALIDIDX  BIT4        /* valid idx for outputting to
                                           in tcl                           */
#define DCT_SIMUL     BIT5        /* can be tcl_simul'd               */
#define DCT_CANBOOT   BIT6        /* can be booted                    */
#define DCT_          BIT7        /* unused */
#define DCT_FORKTYPE  BIT8        /* a forking type                   */
#define DCT_BOT       BIT9        /* a bot connection of some sort... */
#define DCT_FILETRAN  BIT10       /* a file transfer of some sort     */
#define DCT_FILESEND  BIT11       /* a sending file transfer,
                                           getting = !this                  */
#define DCT_LISTEN    BIT12       /* a listening port of some sort    */

/* For dcc chat & files:
 */
#define STAT_ECHO    BIT0    /* echo commands back?                  */
#define STAT_DENY    BIT1    /* bad username (ignore password & deny
                                   access)                              */
#define STAT_CHAT    BIT2    /* in file-system but may return        */
#define STAT_TELNET  BIT3    /* connected via telnet                 */
#define STAT_PARTY   BIT4    /* only on party line via 'p' flag      */
/* skip 2 */
#define STAT_PAGE    BIT7    /* page output to the user              */
#define STAT_COLOR   BIT8    /* Color enabled for user */
#define STAT_BANNER  BIT9    /* show banner on login? */
#define STAT_CHANNELS BIT10  /* show channels on login? */
#define STAT_BOTS    BIT11   /* Show bots linked on login? */
#define STAT_WHOM    BIT12   /* show .whom on login? */

/* For stripping out mIRC codes
 */
#define STRIP_COLOR  BIT0    /* remove mIRC color codes              */
#define STRIP_BOLD   BIT1    /* remove bold codes                    */
#define STRIP_REV    BIT2    /* remove reverse video codes           */
#define STRIP_UNDER  BIT3    /* remove underline codes               */
#define STRIP_ANSI   BIT4    /* remove ALL ansi codes                */
#define STRIP_BELLS  BIT5    /* remote ctrl-g's                      */
#define STRIP_ALL    BIT6    /* remove every damn thing!             */

/* for dcc bot links: */
#define STAT_PINGED  BIT0    /* waiting for ping to return            */
#define STAT_SHARE   BIT1    /* sharing user data with the bot        */
#define STAT_CALLED  BIT2    /* this bot called me                    */
#define STAT_OFFERED BIT3    /* offered her the user file             */
#define STAT_SENDING BIT4    /* in the process of sending a user list */
#define STAT_GETTING BIT5    /* in the process of getting a user list */
#define STAT_WARNED  BIT6    /* warned him about unleaflike behavior  */
#define STAT_LEAF    BIT7    /* this bot is a leaf only               */
#define STAT_LINKING BIT8    /* the bot is currently going through
                                   the linking stage                     */
#define STAT_AGGRESSIVE BIT9 /* aggressively sharing with this bot    */
#define STAT_OFFEREDU BIT10
#define STAT_SENDINGU BIT11
#define STAT_GETTINGU BIT12
#define STAT_UPDATED  BIT13

/* Flags for listening sockets
 */
#define LSTN_PUBLIC  BIT0   /* No access restrictions               */

/* Telnet codes.  See "TELNET Protocol Specification" (RFC 854) and
 * "TELNET Echo Option" (RFC 875) for details.
 */

#define TLN_AYT         246             /* Are You There        */
#define TLN_WILL        251             /* Will                 */
#define TLN_WILL_C      "\373"
#define TLN_WONT        252             /* Won't                */
#define TLN_WONT_C      "\374"
#define TLN_DO          253             /* Do                   */
#define TLN_DO_C        "\375"
#define TLN_DONT        254             /* Don't                */
#define TLN_DONT_C      "\376"
#define TLN_IAC         255             /* Interpret As Command */
#define TLN_IAC_C       "\377"
#define TLN_ECHO        1               /* Echo                 */
#define TLN_ECHO_C      "\001"

extern struct dcc_t 		*dcc;
extern int 			dcc_total;
extern time_t			timesync;
extern char			network[];

extern struct dcc_table DCC_CHAT, DCC_BOT, DCC_LOST, DCC_BOT_NEW,
 DCC_RELAY, DCC_RELAYING, DCC_FORK_RELAY, DCC_PRE_RELAY, DCC_CHAT_PASS,
 DCC_FORK_BOT, DCC_SOCKET, DCC_TELNET_ID, DCC_TELNET_NEW, DCC_TELNET_PW,
 DCC_TELNET, DCC_IDENT, DCC_IDENTWAIT, DCC_DNSWAIT, DCC_IDENTD, DCC_IDENTD_CONNECT;

void send_timesync(int);
void failed_link(int);
void dupwait_notify(const char *);

#endif /* !_DCC_H */
