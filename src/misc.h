#ifndef _MISC_H
#define _MISC_H

#include "common.h"


/*
 * Set the following to the timestamp for the logfile entries.
 * Popular times might be "[%H:%M]" (hour, min), or "[%H:%M:%S]" (hour, min, sec)
 * Read `man strftime' for more formatting options.  Keep it below 32 chars.
 */
#define LOG_TS "[%H:%M]"


char *wbanner();
void restart(int) __attribute__((noreturn));
int coloridx(int);
char *color(int, int, int);
void shuffle(char *, char *);
void showhelp(int, struct flag_record *, char *);
int listen_all(int, int);
char *replace(const char *, const char *, const char *);
int goodpass(char *, int, char *);
int bot_aggressive_to(struct userrec *);
int updatebin(int, char *, int);
int egg_strcatn(char *dst, const char *src, size_t max);
int my_strcpy(char *, char *);
void maskhost(const char *, char *);
char *stristr(char *, char *);
void splitc(char *, char *, char);
void splitcn(char *, char *, char, size_t);
void remove_crlf(char *);
char *newsplit(char **);
char *splitnick(char **);
void stridx(char *, char *, int);
void dumplots(int, const char *, char *);
void daysago(time_t, time_t, char *);
void days(time_t, time_t, char *);
void daysdur(time_t, time_t, char *);
void show_motd(int);
void show_channels(int, char *);
void show_banner(int);
void make_rand_str(char *, int);
char *str_escape(const char *, const char, const char);
char *strchr_unescape(char *, const char, register const char);
inline void str_unescape(char *, register const char);
int str_isdigit(const char *);
void kill_bot(char *, char *);
char *strtolower(char *);
char *strtoupper(char *);

extern int		server_lag, use_invites, use_exempts;

#endif /* !_MISC_H_ */


