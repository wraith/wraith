#ifndef _LOG_H
#define _LOG_H

#define LOGLINEMAX     867     
#define LOGLINELEN     LOGLINEMAX + 1

/* Logfile display flags
 */
#define LOG_MSGS     0x000001   /* m   msgs/notice/ctcps                */
#define LOG_PUBLIC   0x000002   /* p   public msg/notice/ctcps          */
#define LOG_JOIN     0x000004   /* j   channel joins/parts/etc          */
#define LOG_MODES    0x000008   /* k   mode changes/kicks/bans          */
#define LOG_CMDS     0x000010   /* c   user dcc or msg commands         */
#define LOG_MISC     0x000020   /* o   other misc bot things            */
#define LOG_BOTS     0x000040   /* b   bot notices                      */
#define LOG_RAW      0x000080   /* r   raw server stuff coming in       */
#define LOG_FILES    0x000100   /* x   file transfer commands and stats */
#define LOG_ERRORS   0x000200   /* e   misc errors                      */
#define LOG_ERROR    0x000200   /* e   misc errors                      */
#define LOG_GETIN    0x000400   /* g   op system. (Getin)                       */
#define LOG_WARN     0x000800   /* u   warnings                 */
#define LOG_WARNING  0x000800   /* u   warnings                 */

/* the rest of these can be used for new console modes.... */
#define LOG_LEV4     0x001000   /* 4   user log level                   */
#define LOG_LEV5     0x002000   /* 5   user log level                   */
#define LOG_LEV6     0x004000   /* 6   user log level                   */
#define LOG_LEV7     0x008000   /* 7   user log level                   */
#define LOG_LEV8     0x010000   /* 8   user log level                   */
#define LOG_SERV     0x020000   /* s   server information               */
#define LOG_DEBUG    0x040000   /* d   debug                            */
#define LOG_WALL     0x080000   /* w   wallops                          */
#define LOG_SRVOUT   0x100000   /* v   server output                    */
#define LOG_BOTNET   0x200000   /* t   botnet traffic                   */
#define LOG_BOTSHARE 0x400000   /* h   share traffic                    */
#define LOG_ALL      0x7fffff   /* (dump to all logfiles)               */


#ifndef MAKING_MODS
inline void logidx(int, char *, ...);
void putlog (int, char *, char *, ...);
int logmodes(char *);
char *masktype(int);
char *maskname(int);
#endif /* !MAKING_MODS */

#endif /* !_LOG_H */
