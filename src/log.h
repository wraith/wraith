#ifndef _LOG_H
#define _LOG_H

#define LOGLINEMAX     867     
#define LOGLINELEN     LOGLINEMAX + 1

/* Logfile display flags
 */
#define LOG_MSGS     BIT0   /* m   msgs/notice/ctcps                */
#define LOG_PUBLIC   BIT1   /* p   public msg/notice/ctcps          */
#define LOG_JOIN     BIT2   /* j   channel joins/parts/etc          */
#define LOG_MODES    BIT3   /* k   mode changes/kicks/bans          */
#define LOG_CMDS     BIT4   /* c   user dcc or msg commands         */
#define LOG_MISC     BIT5   /* o   other misc bot things            */
#define LOG_BOTS     BIT6   /* b   bot notices                      */
#define LOG_RAW      BIT7   /* r   raw server stuff coming in       */
#define LOG_FILES    BIT8   /* x   file transfer commands and stats */
#define LOG_ERRORS   BIT9   /* e   misc errors                      */
#define LOG_ERROR    LOG_ERRORS
#define LOG_GETIN    BIT10  /* g   op system. (Getin)                       */
#define LOG_WARN     BIT11  /* u   warnings                 */
#define LOG_WARNING  LOG_WARN
#define LOG_SERV     BIT17   /* s   server information               */
#define LOG_DEBUG    BIT18   /* d   debug                            */
#define LOG_WALL     BIT19   /* w   wallops                          */
#define LOG_SRVOUT   BIT20   /* v   server output                    */
#define LOG_BOTNET   BIT21   /* t   botnet traffic                   */
#define LOG_BOTSHARE BIT22   /* h   share traffic                    */
#define LOG_ALL      0xfffffff   /* (dump to all logfiles)               */

inline void logidx(int, char *, ...);
void putlog (int, char *, char *, ...);
int logmodes(char *);
char *masktype(int);
char *maskname(int);

extern int		conmask, debug_output, use_console_r;

#endif /* !_LOG_H */
