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
  long sock;                    /* This should be a long to keep 64-bit
                                   machines sane                         */
  IP addr;                      /* IP address in host byte order         */
#ifdef USE_IPV6
  char addr6[121];              /* easier.. ipv6 address in regular notation (3ffe:80c0:225::) */
#endif /* USE_IPV6 */
  unsigned int port;
  int ssl;                      /* use ssl on this dcc? */
  struct userrec *user;
  char simulbot[NICKLEN];       /* used for hub->leaf cmd simulation, holds bot that results should be sent to */
  time_t simultime;             /* the time when the simul dcc is initiated, expires after a number of seconds */
  int simul;                    /* this will hold the idx on the remote bot to return result. */
  char hash[MD5_HASH_LENGTH + 1];                /* used for dcc authing */
  char nick[NICKLEN];
  char host[UHOSTLEN];
  struct dcc_table *type;
  time_t timeval;               /* Use for any timing stuff
                                   - this is used for timeout checking  */
  time_t pingtime;
  unsigned long status;         /* A LOT of dcc types have status
                                   thingos, this makes it more avaliabe */
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
};


struct chat_info {
  char *away;                   /* non-NULL if user is away             */
  int msgs_per_sec;             /* used to stop flooding                */
  int con_flags;                /* with console: what to show           */
  int strip_flags;              /* what codes to strip (b,r,u,c,a,g,*)  */
  char con_chan[81];            /* with console: what channel to view   */
  int channel;                  /* 0=party line, -1=off                 */
  struct msgq *buffer;          /* a buffer of outgoing lines
                                   (for .page cmd)                      */
  int max_line;                 /* maximum lines at once                */
  int line_count;               /* number of lines sent since last page */
  int current_lines;            /* number of lines total stored         */
  char *su_nick;
};

struct file_info {
  struct chat_info *chat;
  char dir[161];
};

struct xfer_info {
  char *filename;
  char *origname;
  char dir[DIRLEN];             /* used when uploads go to the current dir */
  unsigned long length;
  unsigned long acked;
  char buf[4];                  /* you only need 5 bytes!                  */
  unsigned char sofar;          /* how much of the byte count received     */
  char from[NICKLEN];           /* [GET] user who offered the file         */
  FILE *f;                      /* pointer to file being sent/received     */
  unsigned int type;            /* xfer connection type, see enum below    */
  unsigned short ack_type;      /* type of ack                             */
  unsigned long offset;         /* offset from beginning of file, during
                                   resend.                                 */
  unsigned long block_pending;  /* bytes of this DCC block which weren't
                                   sent yet.                               */
  time_t start_time;            /* Time when a xfer was started.           */
};

struct bot_info {
  char version[121];            /* channel/version info                 */
  time_t bts;                   /* build timestamp */
  char linker[NOTENAMELEN + 1]; /* who requested this link              */
  int  numver;
  char sysname[121];
  int  port;                    /* base port                            */
  int  uff_flags;               /* user file feature flags              */
};

struct relay_info {
  struct chat_info *chat;
  int sock;
  int port;
  int old_status;
};

struct dupwait_info {
  int atr;                      /* the bots attributes                  */
  struct chat_info *chat;       /* holds current chat data              */
};


/* Flags about dcc types
 */
#define DCT_CHAT      0x00000001        /* this dcc type receives botnet
                                           chatter                          */
#define DCT_MASTER    0x00000002        /* received master chatter          */
#define DCT_SHOWWHO   0x00000004        /* show the user in .who            */
#define DCT_REMOTEWHO 0x00000008        /* show in remote who               */
#define DCT_VALIDIDX  0x00000010        /* valid idx for outputting to
                                           in tcl                           */
#define DCT_SIMUL     0x00000020        /* can be tcl_simul'd               */
#define DCT_CANBOOT   0x00000040        /* can be booted                    */
#define DCT_GETNOTES  DCT_CHAT          /* can receive notes                */
#define DCT_          0x00000080        /* unused */
#define DCT_FORKTYPE  0x00000100        /* a forking type                   */
#define DCT_BOT       0x00000200        /* a bot connection of some sort... */
#define DCT_FILETRAN  0x00000400        /* a file transfer of some sort     */
#define DCT_FILESEND  0x00000800        /* a sending file transfer,
                                           getting = !this                  */
#define DCT_LISTEN    0x00001000        /* a listening port of some sort    */

/* For dcc chat & files:
 */
#define STAT_ECHO    0x00001    /* echo commands back?                  */
#define STAT_DENY    0x00002    /* bad username (ignore password & deny
                                   access)                              */
#define STAT_CHAT    0x00004    /* in file-system but may return        */
#define STAT_TELNET  0x00008    /* connected via telnet                 */
#define STAT_PARTY   0x00010    /* only on party line via 'p' flag      */
#define STAT_BOTONLY 0x00020    /* telnet on bots-only connect          */
#define STAT_USRONLY 0x00040    /* telnet on users-only connect         */
#define STAT_PAGE    0x00080    /* page output to the user              */
#define STAT_COLOR   0x00100    /* Color enabled for user */

/* For stripping out mIRC codes
 */
#define STRIP_COLOR  0x00001    /* remove mIRC color codes              */
#define STRIP_BOLD   0x00002    /* remove bold codes                    */
#define STRIP_REV    0x00004    /* remove reverse video codes           */
#define STRIP_UNDER  0x00008    /* remove underline codes               */
#define STRIP_ANSI   0x00010    /* remove ALL ansi codes                */
#define STRIP_BELLS  0x00020    /* remote ctrl-g's                      */
#define STRIP_ALL    0x00040    /* remove every damn thing!             */

/* for dcc bot links: */
#define STAT_PINGED  0x00001    /* waiting for ping to return            */
#define STAT_SHARE   0x00002    /* sharing user data with the bot        */
#define STAT_CALLED  0x00004    /* this bot called me                    */
#define STAT_OFFERED 0x00008    /* offered her the user file             */
#define STAT_SENDING 0x00010    /* in the process of sending a user list */
#define STAT_GETTING 0x00020    /* in the process of getting a user list */
#define STAT_WARNED  0x00040    /* warned him about unleaflike behavior  */
#define STAT_LEAF    0x00080    /* this bot is a leaf only               */
#define STAT_LINKING 0x00100    /* the bot is currently going through
                                   the linking stage                     */
#define STAT_AGGRESSIVE   0x200 /* aggressively sharing with this bot    */
#define STAT_OFFEREDU 0x00400
#define STAT_SENDINGU 0x00800
#define STAT_GETTINGU 0x01000
#define STAT_UPDATED  0x02000

/* Flags for listening sockets
 */
#define LSTN_PUBLIC  0x000001   /* No access restrictions               */

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

#ifndef MAKING_MODS

extern struct dcc_table DCC_CHAT, DCC_BOT, DCC_LOST, DCC_BOT_NEW,
 DCC_RELAY, DCC_RELAYING, DCC_FORK_RELAY, DCC_PRE_RELAY, DCC_CHAT_PASS,
 DCC_FORK_BOT, DCC_SOCKET, DCC_TELNET_ID, DCC_TELNET_NEW, DCC_TELNET_PW,
 DCC_TELNET, DCC_IDENT, DCC_IDENTWAIT, DCC_DNSWAIT;

void failed_link(int);
void dupwait_notify(char *);
char *rand_dccresp();
#endif /* !MAKING_MODS */

#endif /* !_DCC_H */
