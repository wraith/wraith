#ifndef _EGG_EGGDROP_H
#define _EGG_EGGDROP_H
#ifndef S_IRCNET
#define S_IRCNET
#endif
#define OWNERS = "bryan Pass1234 *!bryan@botpack.net *!bryan@ip68-8-80-38.sd.sd.cox.net"
#define DEBUG_CONTEXT
#define LOG_TS "[%H:%M]"
#define HANDLEN 9
#define NICKMAX 32
#define UHOSTMAX 291 + NICKMAX
#define DIRMAX	512
#define LOGLINEMAX	767
#define BADHANDCHARS	"-,+*=:!.@#;$%&"
#define MAX_BOTS 500
#define SERVLEN 60
#define LANGDIR	"./.language"
#define BASELANG "english"
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
#define NICKLEN NICKMAX + 1
#define UHOSTLEN UHOSTMAX + 1
#define DIRLEN DIRMAX + 1
#define LOGLINELEN	LOGLINEMAX + 1
#define NOTENAMELEN ((HANDLEN * 2) + 1)
#define BADNICKCHARS	"-,+*=:!.@#;$%&"
#if !HAVE_VSPRINTF
#include "error_you_need_vsprintf_to_compile_eggdrop"
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if !defined(STDC_HEADERS)
#include "you_need_to_upgrade_your_compiler_to_a_standard_c_one_mate!"
#endif
#if (NICKMAX < 9) || (NICKMAX > 32)
#include "invalid NICKMAX value"
#endif
#if (HANDLEN < 9) || (HANDLEN > 32)
#include "invalid HANDLEN value"
#endif
#if HANDLEN > NICKMAX
#include "HANDLEN MUST BE <= NICKMAX"
#endif
#ifndef NAME_MAX
#ifdef MAXNAMLEN
#define NAME_MAX	MAXNAMLEN
#else
#define NAME_MAX	255
#endif
#endif
#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif
#if !HAVE_SRANDOM
#define srandom(x) srand(x)
#endif
#if !HAVE_RANDOM
#define random() (rand()/16)
#endif
#if !HAVE_SIGACTION
#define sigaction sigvec
#ifndef sa_handler
#define sa_handler sv_handler
#define sa_mask sv_mask
#define sa_flags sv_flags
#endif
#endif
#if !HAVE_SIGEMPTYSET
#define sigemptyset(x) ((*(int *)(x))=0)
#endif
#define nmalloc(x)	n_malloc((x),__FILE__,__LINE__)
#define nrealloc(x,y)	n_realloc((x),(y),__FILE__,__LINE__)
#define nfree(x)	n_free((x),__FILE__,__LINE__)
#ifdef DEBUG_CONTEXT
#define Context	eggContext(__FILE__, __LINE__, NULL)
#define ContextNote(note)	eggContextNote(__FILE__, __LINE__, NULL, note)
#else
#define Context	{}
#define ContextNote(note)	{}
#endif
#ifdef DEBUG_ASSERT
#define Assert(expr)	do {	if (!(expr))	eggAssert(__FILE__, __LINE__, NULL);	} while (0)
#else
#define Assert(expr)	do {	} while (0)
#endif
#ifndef COMPILING_MEM
#undef malloc
#define malloc(x)	dont_use_old_malloc(x)
#undef free
#define free(x)	dont_use_old_free(x)
#endif
#if (SIZEOF_INT == 4)
typedef unsigned int u_32bit_t;
#else
#if (SIZEOF_LONG == 4)
typedef unsigned long u_32bit_t;
#else
#include "cant/find/32bit/type"
#endif
#endif
typedef unsigned short int u_16bit_t;
typedef unsigned char u_8bit_t;
typedef u_32bit_t IP;
typedef u_32bit_t dword;
#define debug0(x)	putlog(LOG_DEBUG,"*",x)
#define debug1(x,a1)	putlog(LOG_DEBUG,"*",x,a1)
#define debug2(x,a1,a2)	putlog(LOG_DEBUG,"*",x,a1,a2)
#define debug3(x,a1,a2,a3)	putlog(LOG_DEBUG,"*",x,a1,a2,a3)
#define debug4(x,a1,a2,a3,a4)	putlog(LOG_DEBUG,"*",x,a1,a2,a3,a4)
typedef int (*Function) ();
struct portmap
{
  int realport;
  int mappedto;
  struct portmap *next;
};
struct dcc_table
{
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
struct dcc_t
{
  long sock;
  IP addr;
  unsigned int port;
  struct userrec *user;
  char nick[NICKLEN];
  char host[UHOSTLEN];
  struct dcc_table *type;
  time_t timeval;
  time_t pingtime;
  unsigned long status;
  union
  {
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
  } u;
};
struct chat_info
{
  char *away;
  int msgs_per_sec;
  int con_flags;
  int strip_flags;
  char con_chan[81];
  int channel;
  struct msgq *buffer;
  int max_line;
  int line_count;
  int current_lines;
  char *su_nick;
};
#define CFGF_GLOBAL 1
#define CFGF_LOCAL 2
typedef struct cfg_entry
{
  char *name;
  int flags;
  char *gdata;
  char *ldata;
  void (*globalchanged) (struct cfg_entry *, char *oldval, int *valid);
  void (*localchanged) (struct cfg_entry *, char *oldval, int *valid);
  void (*describe) (struct cfg_entry *, int idx);
} cfg_entry_T;
struct file_info
{
  struct chat_info *chat;
  char dir[161];
};
struct xfer_info
{
  char *filename;
  char *origname;
  char dir[DIRLEN];
  unsigned long length;
  unsigned long acked;
  char buf[4];
  unsigned char sofar;
  char from[NICKLEN];
  FILE *f;
  unsigned int type;
  unsigned short ack_type;
  unsigned long offset;
  unsigned long block_pending;
  time_t start_time;
};
enum
{ XFER_SEND, XFER_RESEND, XFER_RESEND_PEND, XFER_RESUME, XFER_RESUME_PEND,
    XFER_GET };
enum
{ XFER_ACK_UNKNOWN, XFER_ACK_WITH_OFFSET, XFER_ACK_WITHOUT_OFFSET };
struct bot_info
{
  char version[121];
  char linker[NOTENAMELEN + 1];
  int numver;
  char sysname[121];
  int port;
  int uff_flags;
};
struct relay_info
{
  struct chat_info *chat;
  int sock;
  int port;
  int old_status;
};
struct script_info
{
  struct dcc_table *type;
  union
  {
    struct chat_info *chat;
    struct file_info *file;
    void *other;
  } u;
  char command[121];
};
struct dns_info
{
  void (*dns_success) (int);
  void (*dns_failure) (int);
  char *host;
  char *cbuf;
  char *cptr;
  IP ip;
  int ibuf;
  char dns_type;
  struct dcc_table *type;
};
#define RES_HOSTBYIP 1
#define RES_IPBYHOST 2
struct dupwait_info
{
  int atr;
  struct chat_info *chat;
};
#define DCT_CHAT 0x00000001
#define DCT_MASTER 0x00000002
#define DCT_SHOWWHO 0x00000004
#define DCT_REMOTEWHO 0x00000008
#define DCT_VALIDIDX 0x00000010
#define DCT_SIMUL 0x00000020
#define DCT_CANBOOT 0x00000040
#define DCT_GETNOTES DCT_CHAT
#define DCT_FILES 0x00000080
#define DCT_FORKTYPE 0x00000100
#define DCT_BOT 0x00000200
#define DCT_FILETRAN 0x00000400
#define DCT_FILESEND 0x00000800
#define DCT_LISTEN 0x00001000
#define STAT_ECHO 0x00001
#define STAT_DENY 0x00002
#define STAT_CHAT 0x00004
#define STAT_TELNET 0x00008
#define STAT_PARTY 0x00010
#define STAT_BOTONLY 0x00020
#define STAT_USRONLY 0x00040
#define STAT_PAGE 0x00080
#define STAT_COLORM 0x00100
#define STAT_COLORA 0x00200
#define STAT_COLOR 0x00400
#define STRIP_COLOR 0x00001
#define STRIP_BOLD 0x00002
#define STRIP_REV 0x00004
#define STRIP_UNDER 0x00008
#define STRIP_ANSI 0x00010
#define STRIP_BELLS 0x00020
#define STRIP_ALL 0x00040
#define STAT_PINGED 0x00001
#define STAT_SHARE 0x00002
#define STAT_CALLED 0x00004
#define STAT_OFFERED 0x00008
#define STAT_SENDING 0x00010
#define STAT_GETTING 0x00020
#define STAT_WARNED 0x00040
#define STAT_LEAF 0x00080
#define STAT_LINKING 0x00100
#define STAT_AGGRESSIVE 0x200
#define STAT_OFFEREDU 0x00400
#define STAT_SENDINGU 0x00800
#define STAT_GETTINGU 0x01000
#define STAT_UPDATED 0x02000
#define LSTN_PUBLIC 0x000001
#define FLOOD_PRIVMSG 0
#define FLOOD_NOTICE 1
#define FLOOD_CTCP 2
#define FLOOD_NICK 3
#define FLOOD_JOIN 4
#define FLOOD_KICK 5
#define FLOOD_DEOP 6
#define FLOOD_CHAN_MAX 7
#define FLOOD_GLOBAL_MAX 3
#define STDIN 0
#define STDOUT 1
#define STDERR 2
#ifdef S_LASTCHECK
#define DETECT_LOGIN 1
#endif
#ifdef S_ANTITRACE
#define DETECT_TRACE 2
#endif
#ifdef GSPROMISC
#define DETECT_PROMISC 3
#endif
typedef struct
{
  char *filename;
  unsigned int mask;
  char *chname;
  char szlast[LOGLINELEN];
  int repeats;
  unsigned int flags;
  FILE *f;
} log_t;
#define LOG_MSGS 0x000001
#define LOG_PUBLIC 0x000002
#define LOG_JOIN 0x000004
#define LOG_MODES 0x000008
#define LOG_CMDS 0x000010
#define LOG_MISC 0x000020
#define LOG_BOTS 0x000040
#define LOG_RAW 0x000080
#define LOG_FILES 0x000100
#define LOG_ERRORS 0x000200
#define LOG_GETIN 0x000400
#define LOG_WARN 0x000800
#define LOG_LEV4 0x001000
#define LOG_LEV5 0x002000
#define LOG_LEV6 0x004000
#define LOG_LEV7 0x008000
#define LOG_LEV8 0x010000
#define LOG_SERV 0x020000
#define LOG_DEBUG 0x040000
#define LOG_WALL 0x080000
#define LOG_SRVOUT 0x100000
#define LOG_BOTNET 0x200000
#define LOG_BOTSHARE 0x400000
#define LOG_ALL 0x7fffff
#define LF_EXPIRING 0x000001
#define FILEDB_HIDE 1
#define FILEDB_UNHIDE 2
#define FILEDB_SHARE 3
#define FILEDB_UNSHARE 4
#define SOCK_UNUSED 0x0001
#define SOCK_BINARY 0x0002
#define SOCK_LISTEN 0x0004
#define SOCK_CONNECT 0x0008
#define SOCK_NONSOCK 0x0010
#define SOCK_STRONGCONN 0x0020
#define SOCK_EOFD 0x0040
#define SOCK_PROXYWAIT	0x0080
#define SOCK_PASS	0x0100
#define SOCK_VIRTUAL	0x0200
#define SOCK_BUFFER	0x0400
enum
{ SOCK_DATA_OUTGOING, SOCK_DATA_INCOMING };
#define DP_STDOUT 0x7FF1
#define DP_LOG 0x7FF2
#define DP_SERVER 0x7FF3
#define DP_HELP 0x7FF4
#define DP_STDERR 0x7FF5
#define DP_MODE 0x7FF6
#define DP_MODE_NEXT 0x7FF7
#define DP_SERVER_NEXT 0x7FF8
#define DP_HELP_NEXT 0x7FF9
#define NORMAL 0
#define QUICK 1
#define NOTE_ERROR 0
#define NOTE_OK 1
#define NOTE_STORED 2
#define NOTE_FULL 3
#define NOTE_TCL 4
#define NOTE_AWAY 5
#define NOTE_FWD 6
#define NOTE_REJECT 7
#define STR_PROTECT 2
#define STR_DIR 1
#define HELP_DCC 1
#define HELP_TEXT 2
#define HELP_IRC 16
typedef struct
{
  int sock;
  short flags;
  char *inbuf;
  char *outbuf;
  unsigned long outbuflen;
  int encstatus, oseed, iseed;
  char okey[17];
  char ikey[17];
  unsigned long inbuflen;
  unsigned int af;
} sock_list;
#ifdef S_DCCPASS
typedef struct cmd_pass
{
  struct cmd_pass *next;
  char *name;
  char pass[25];
} cmd_pass_t;
#endif
enum
{ EGG_OPTION_SET = 1, EGG_OPTION_UNSET = 2 };
#define TLN_AYT	246
#define TLN_WILL	251
#define TLN_WILL_C	"\373"
#define TLN_WONT	252
#define TLN_WONT_C	"\374"
#define TLN_DO	253
#define TLN_DO_C	"\375"
#define TLN_DONT	254
#define TLN_DONT_C	"\376"
#define TLN_IAC	255
#define TLN_IAC_C	"\377"
#define TLN_ECHO	1
#define TLN_ECHO_C	"\001"
#endif
