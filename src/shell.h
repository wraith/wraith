#ifndef _SHELL_H
#define _SHELL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#define ERR_BINSTAT     1
#define ERR_PASSWD      3
#define ERR_WRONGBINDIR 4
#define ERR_DATADIR	5
#define ERR_TMPSTAT     8
#define ERR_TMPMOD      9
#define ERR_WRONGUID    12
#define ERR_WRONGUNAME  13
#define ERR_BADCONF     14
#define ERR_BADBOT	15
#define ERR_BADPASS	16
#define ERR_BOTDISABLED	17
#define ERR_NOBOTS	18
#define ERR_NOBOT	19
#define ERR_NOUSERNAME	20
#define ERR_NOHOMEDIR	21
#define ERR_NOTINIT	22
#define ERR_TOOMANYBOTS 23
#define ERR_LIBS	24
#define ERR_ALREADYINIT 25
#define ERR_MAX         26

#define DETECT_LOGIN 	1
#define DETECT_TRACE 	2
#define DETECT_PROMISC 	3
#define DETECT_PROCESS 	4		/* NOT USED */
#define DETECT_HIJACK 	5

#define DET_IGNORE 	0
#define DET_WARN 	1
#define DET_REJECT 	2
#define DET_DIE 	3
#define DET_SUICIDE 	4

#define DETECTED_LEN	8		/* 'suicide' is longest word */

namespace bd {
  class Stream;
}

void check_maxfiles();
void check_mypid();
void clear_tmp();
const char *homedir(bool = 1);
const char *my_username();
void expand_tilde(char **);
int shell_exec(char *, char *, char **, char **, bool = 0);
int simple_exec(const char* argv[]);
void check_last();
void check_promisc();
void check_trace(int);
void check_crontab();
void crontab_del();
int crontab_exists(bd::Stream* = NULL, bool = 0);
void crontab_create(int);
void detected(int, const char *);
void suicide(const char *);
void werr(int) __attribute__((noreturn));
const char *werr_tostr(int) __attribute__((const));
int det_translate(const char *) __attribute__((pure));
const char *det_translate_num(int) __attribute__((const));
char *shell_escape(const char *);
int mkdir_p(const char *);
extern bool		clear_tmpdir;

#endif /* _SHELL_H */
