/*
 * main.c -- handles: 
 *   core event handling
 *   command line arguments
 *   context and assert debugging
 *
 */

#include "common.h"
#include "main.h"
#include "adns.h"
#include "color.h"
#include "dcc.h"
#include "misc.h"
#include "binary.h"
#include "response.h"
#include "thread.h"
#include "settings.h"
#include "misc_file.h"
#include "net.h"
#include "users.h"
#include "shell.h"
#include "userrec.h"
#include "tclhash.h"
#include "cfg.h"
#include "dccutil.h"
#include "crypt.h"
#include "debug.h"
#include "chanprog.h"
#include "traffic.h"
#include "bg.h"	
#include "botnet.h"
#include "build.h"
#ifdef LEAF
#include "src/mod/irc.mod/irc.h"
#include "src/mod/server.mod/server.h"
#endif /* LEAF */
#include "src/mod/channels.mod/channels.h"
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#ifdef STOP_UAC				/* osf/1 complains a lot */
# include <sys/sysinfo.h>
# define UAC_NOPRINT			/* Don't report unaligned fixups */
#endif /* STOP_UAC */
#include <sys/file.h>
#include <sys/stat.h>
#include <signal.h>

#include "chan.h"
#include "tandem.h"
#include "egg_timer.h"
#include "core_binds.h"

#ifdef CYGWIN_HACKS
#include <getopt.h>
#endif /* CYGWIN_HACKS */

#ifndef _POSIX_SOURCE
/* Solaris needs this */
#define _POSIX_SOURCE
#endif

extern int		optind;

const time_t 	buildts = CVSBUILD;		/* build timestamp (UTC) */
const char	egg_version[1024] = "1.2";

/* FIXME: remove after 1.2 ??? OR NOT */
int	old_hack = 0;
bool 	localhub = 1; 		/* we set this to 0 if we get a -B */
int 	role;
bool 	loading = 0;
int	default_flags = 0;	/* Default user flags and */
int	default_uflags = 0;	/* Default userdefinied flags for people
				   who say 'hello' or for .adduser */
bool	backgrd = 1;		/* Run in the background? */
uid_t   myuid;
bool	term_z = 0;		/* Foreground: use the terminal as a party line? */
int 	updating = 0; 		/* this is set when the binary is called from itself. */
char 	tempdir[DIRMAX] = "";
char 	*binname = NULL;
time_t	online_since;		/* Unix-time that the bot loaded up */

char	owner[121] = "";	/* Permanent owner(s) of the bot */
char	version[81] = "";	/* Version info (long form) */
char	ver[41] = "";		/* Version info (short form) */
bool	use_stderr = 1;		/* Send stuff to stderr instead of logfiles? */
char	quit_msg[1024];		/* quit message */
time_t	now;			/* duh, now :) */

#define fork_interval atoi( CFG_FORKINTERVAL.ldata ? CFG_FORKINTERVAL.ldata : CFG_FORKINTERVAL.gdata ? CFG_FORKINTERVAL.gdata : "0")
static bool do_confedit = 0;		/* show conf menu if -C */
#ifdef LEAF
static char do_killbot[21] = "";
#endif /* LEAF */
static char *update_bin = NULL;
static bool checktrace = 1;		/* Check for trace when starting up? */


static char *getfullbinname(const char *argv_zero)
{
  char *bin = strdup(argv_zero);

  if (bin[0] == '/')
#ifdef CYGWIN_HACKS
    goto cygwin;
#else
    return bin;
#endif /* CYGWIN_HACKS */

  char cwd[DIRMAX] = "";

  if (!getcwd(cwd, DIRMAX))
    fatal("getcwd() failed", 0);

  if (cwd[strlen(cwd) - 1] == '/')
    cwd[strlen(cwd) - 1] = 0;

  char *p = bin, *p2 = strchr(p, '/');

  while (p) {
    if (p2)
      *p2++ = 0;
    if (!strcmp(p, "..")) {
      p = strrchr(cwd, '/');
      if (p)
        *p = 0;
    } else if (strcmp(p, ".")) {
      strcat(cwd, "/");
      strcat(cwd, p);
    }
    p = p2;
    if (p)
      p2 = strchr(p, '/');
  }
  str_redup(&bin, cwd);
#ifdef CYGWIN_HACKS
  /* tack on the .exe */
  cygwin:
  bin = (char *) realloc(bin, strlen(bin) + 4 + 1);
  strcat(bin, ".exe");
  bin[strlen(bin)] = 0;
#endif /* CYGWIN_HACKS */
  return bin;
}

void fatal(const char *s, int recoverable)
{
#ifdef LEAF
  nuke_server((char *) s);
#endif /* LEAF */

  if (s && s[0])
    putlog(LOG_MISC, "*", "!*! %s", s);

/*  flushlogs(); */
#ifdef HAVE_SSL
    ssl_cleanup();
#endif /* HAVE_SSL */

#ifdef HUB
  listen_all(my_port, 1); /* close the listening port... */
#endif /* HUB */

  for (int i = 0; i < dcc_total; i++)
    if (dcc[i].type && dcc[i].sock >= 0)
      killsock(dcc[i].sock);

  if (!recoverable) {
//    if (conf.bot && conf.bot->pid_file)
//      unlink(conf.bot->pid_file);
    exit(1);
  }
}

static void check_expired_dcc()
{
  for (int i = 0; i < dcc_total; i++)
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

/* this also expires irc dcc_cmd auths */
static void expire_simuls() {
  for (int idx = 0; idx < dcc_total; idx++) {
    if (dcc[idx].type && dcc[idx].simul > 0) {
      if ((now - dcc[idx].simultime) >= 100) { /* expire simuls after 100 seconds (re-uses idx, so it wont fill up) */
        dcc[idx].simul = -1;
        lostdcc(idx);
        return;		/* only safe to do one at a time */
      }
    }
  }
}

static void checkpass() 
{
  static int checkedpass = 0;

  if (!checkedpass) {
    char *gpasswd = NULL;

    gpasswd = (char *) getpass("bash$ ");
    checkedpass = 1;
    if (!gpasswd || (gpasswd && md5cmp(settings.shellhash, gpasswd) && !check_master(gpasswd))) 
      werr(ERR_BADPASS);
  }
}

static void got_ed(char *, char *, char*) __attribute__((noreturn));

static void got_ed(char *which, char *in, char *out)
{
  sdprintf("got_Ed called: -%s i: %s o: %s", which, in, out);
  if (!in || !out)
    fatal(STR("Wrong number of arguments: -e/-d <infile> <outfile/STDOUT>"),0);
  if (!strcmp(in, out))
    fatal("<infile> should NOT be the same name as <outfile>", 0);
  if (!strcmp(which, "e")) {
    Encrypt_File(in, out);
    fatal("File Encryption complete",3);
  } else if (!strcmp(which, "d")) {
    Decrypt_File(in, out);
    fatal("File Decryption complete",3);
  }
  exit(0);
}

static void show_help() __attribute__((noreturn));

static void show_help()
{
  char format[81] = "";

  egg_snprintf(format, sizeof format, "%%-30s %%-30s\n");

  printf(STR("%s\n\n"), version);
  printf(format, "Option", "Description");
  printf(format, "------", "-----------");
  printf(format, STR("-B <botnick>"), STR("Starts the specified bot"));
  printf(format, STR("-C"), STR("Config file menu system"));
  printf(format, STR("-e <infile> <outfile>"), STR("Encrypt infile to outfile"));
  printf(format, STR("-d <infile> <outfile>"), STR("Decrypt infile to outfile"));
  printf(format, STR("-D"), STR("Enables debug mode (see -n)"));
  printf(format, STR("-E [#/all]"), STR("Display Error codes english translation (use 'all' to display all)"));
/*  printf(format, STR("-g <file>"), STR("Generates a template config file"));
  printf(format, STR("-G <file>"), STR("Generates a custom config for the box"));
*/
  printf(format, "-h", "Display this help listing");
  printf(format, STR("-k <botname>"), STR("Terminates (botname) with kill -9"));
  printf(format, STR("-n"), STR("Disables backgrounding first bot in conf"));
  printf(format, STR("-s"), STR("Disables checking for ptrace/strace during startup (no pass needed)"));
  printf(format, STR("-t"), STR("Enables \"Partyline\" emulation (requires -n)"));
  printf(format, STR("-u <binary>"), STR("Update binary, Automatically kill/respawn bots"));
  printf(format, STR("-U <binary>"), STR("Update binary"));
  printf(format, "-v", "Displays bot version");
  exit(0);
}
/* FIXME: remove after 1.2 */
static void startup_checks(int);

#ifdef LEAF
# define PARSE_FLAGS "02B:Cd:De:Eg:G:k:L:P:hnstu:U:v"
#else /* !LEAF */
# define PARSE_FLAGS "02Cd:De:Eg:G:hnstu:U:v"
#endif /* HUB */
#define FLAGS_CHECKPASS "CdDeEgGhkntuUv"
static void dtx_arg(int argc, char *argv[])
{
  int i = 0;
#ifdef LEAF
  int localhub_pid = 0;
#endif /* LEAF */
  char *p = NULL;
  
  opterr = 0;
  while ((i = getopt(argc, argv, PARSE_FLAGS)) != EOF) {
    if (strchr(FLAGS_CHECKPASS, i))
      checkpass();
    switch (i) {
      case '0':
        exit(0);
      case '2':		/* used for testing new binary through update */
        if (settings.uname[0])		/* we're already initialized with data, just exit! */
          exit(2);
	/* FIXME: remove after 1.2 */
	/* ... otherwise, we need to check if ~/.ssh/.known_hosts exists and write to our binary */
        startup_checks(2);
        exit(2);
#ifdef LEAF
      case 'B':
        localhub = 0;
        strncpyz(origbotname, optarg, NICKLEN + 1);
        break;
#endif /* LEAF */
      case 'C':
        do_confedit = 1;
        break;
      case 'h':
        show_help();
#ifdef LEAF
      case 'k':		/* kill bot */
        strncpyz(do_killbot, optarg, sizeof do_killbot);
#endif /* LEAF */
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
        sdprintf("debug enabled");
        break;
      case 'E':
        p = argv[optind];
        if (p && p[0]) {
          if (!strcmp(p, "all")) {
            int n;
            putlog(LOG_MISC, "*", "Listing all errors");
            for (n = 1; n < ERR_MAX; n++)
            putlog(LOG_MISC, "*", "Error #%d: %s", n, werr_tostr(n));
          } else if (egg_isdigit(p[0])) {
            putlog(LOG_MISC, "*", "Error #%d: %s", atoi(p), werr_tostr(atoi(p)));
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
      case 'u':
      case 'U':
        if (optarg) {
          update_bin = strdup(optarg);
          updating = (i == 'u' ? 1 : 2);	/* use 2 if 'U' to not kill/spawn bots. */
          break;
        } else
          exit(0);
      case 'v':
      {
        char date[50] = "";

        egg_strftime(date, sizeof date, "%c %Z", gmtime(&buildts));
	printf("%s\nBuild Date: %s (%lu)\n", version, date, buildts);

        if (settings.uname[0]) {
          sdebug++;
          bin_to_conf();
        }
	exit(0);
      }
#ifdef LEAF
      case 'L':
      {
        localhub_pid = checkpid(optarg, NULL);
        break;
      }
      case 'P':
        if (atoi(optarg) && localhub_pid && (atoi(optarg) != localhub_pid))
          exit(3);
        else
          sdprintf("Updating...");
        localhub = 1;
        updating = 1;
        break;
#endif
      case '?':
      default:
        break;
    }
  }
}

/* Timer info */
static time_t		lastmin = 99;
static struct tm	nowtm;

void core_10secondly()
{
#ifndef CYGWIN_HACKS
  static int curcheck = 0;

  curcheck++;

  if (curcheck == 1)
    check_trace(0);

#ifdef LEAF
  if (localhub) {
#endif /* LEAF */
    check_promisc();

    if (curcheck == 2)
      check_last();
    if (curcheck == 3) {
      check_processes();
      curcheck = 0;
    }
#ifdef LEAF
  }
#endif /* LEAF */
#endif /* !CYGWIN_HACKS */
}

/* Traffic stats
 */
egg_traffic_t traffic;

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

static void core_secondly()
{
  static int cnt = 0;
  time_t miltime;

#ifdef CRAZY_TRACE 
  if (!attached) crazy_trace();
#endif /* CRAZY_TRACE */
  if (fork_interval && backgrd && ((now - lastfork) > fork_interval))
      do_fork();
  cnt++;

  if ((cnt % 30) == 0) {
    autolink_cycle(NULL);         /* attempt autolinks */
    cnt = 0;
  }

  egg_memcpy(&nowtm, gmtime(&now), sizeof(struct tm));
  if (nowtm.tm_min != lastmin) {
    time_t i = 0;

    /* Once a minute */
    lastmin = (lastmin + 1) % 60;
    /* In case for some reason more than 1 min has passed: */
    while (nowtm.tm_min != lastmin) {
      /* Timer drift, dammit */
      debug2("timer: drift (lastmin=%lu, now=%d)", lastmin, nowtm.tm_min);
      i++;
      lastmin = (lastmin + 1) % 60;
    }
    if (i > 1)
      putlog(LOG_MISC, "*", "(!) timer drift -- spun %lu minutes", i);
    miltime = (nowtm.tm_hour * 100) + (nowtm.tm_min);
    if (((int) (nowtm.tm_min / 5) * 5) == (nowtm.tm_min)) {	/* 5 min */
/* 	flushlogs(); */
      if (!miltime) {	/* At midnight */
	char s[25] = "";

	strncpyz(s, ctime(&now), sizeof s);
#ifdef HUB
	putlog(LOG_ALL, "*", "--- %.11s%s", s, s + 20);
        backup_userfile();
#endif /* HUB */
      }
    }
    /* These no longer need checking since they are all check vs minutely
     * settings and we only get this far on the minute.
     */
    if (miltime == 300)
      event_resettraffic();
  }
}

static void check_autoaway()
{
  for (int i = 0; i < dcc_total; i++)
    if (dcc[i].type && dcc[i].type == &DCC_CHAT && !(dcc[i].u.chat->away) && ((now - dcc[i].timeval) >= 600))
      set_away(i, "Auto away after 10 minutes.");
}

static void core_minutely()
{
#ifdef HUB
  send_timesync(-1);
#endif /* HUB */
#ifdef LEAF
  check_maxfiles();
  check_mypid();
#endif
  check_bind_time(&nowtm);
  check_autoaway();
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

/* FIXME: Remove after 1.2 (the hacks) */
static void startup_checks(int hack) {
  int enc = CONF_ENC;

  /* for compatability with old conf files 
   * only check/use conf file if it exists and settings.uname is empty.
   * if settings.uname is NOT empty, just erase the conf file if it exists
   * otherwise, assume we're working only with the struct */

#ifdef LEAF
  egg_snprintf(cfile, sizeof cfile, STR("%s/.known_hosts"), confdir());
#endif /* LEAF */
#ifdef HUB
  egg_snprintf(cfile, sizeof cfile, STR("%s/conf"), confdir());
#endif /* HUB */
#ifdef CYGWIN_HACKS
  egg_snprintf(cfile, sizeof cfile, STR("%s/conf.txt"), confdir());
  enc = 0;
#endif /* CYGWIN_HACKS */

  if (!can_stat(confdir())) {
#ifdef LEAF
/* FIXME: > 1.2 still making confdir() because tmp is inside */
    if (mkdir(confdir(),  S_IRUSR | S_IWUSR | S_IXUSR)) {
      unlink(confdir());
      if (!can_stat(confdir()))
        if (mkdir(confdir(), S_IRUSR | S_IWUSR | S_IXUSR) && !hack)
#endif /* LEAF */
          werr(ERR_CONFSTAT);
#ifdef LEAF
    }
#endif /* LEAF */
  }

  if (fixmod(confdir()) && !hack)
    werr(ERR_CONFDIRMOD);
 
  if (can_stat(cfile) && !settings.uname[0])
    old_hack = 1;
  if (can_stat(cfile) && settings.uname[0])
    unlink(cfile);		/* kill the old one! */

  if (old_hack && hack)
    old_hack = 2;		/* this is so write_settings() will exit(2); */

  if (old_hack && can_stat(cfile) && fixmod(cfile) && !hack)
    werr(ERR_CONFMOD);

  if (!can_stat(tempdir)) {
    if (mkdir(tempdir,  S_IRUSR | S_IWUSR | S_IXUSR)) {
      unlink(tempdir);
      if (!can_stat(tempdir))
        if (mkdir(tempdir, S_IRUSR | S_IWUSR | S_IXUSR) && !hack)
          werr(ERR_TMPSTAT);
    }
  }
  if (fixmod(tempdir) && !hack)
    werr(ERR_TMPMOD);

  /* test tempdir: it's vital */
  if (!hack)
  {
    Tempfile *testdir = new Tempfile("test");
    int result;
   
    fprintf(testdir->f, "\n");
    result = fflush(testdir->f);
    delete testdir;
    if (result)
      fatal(strerror(errno), 0);
  }

  if (old_hack && can_stat(cfile))
    readconf(cfile, enc);	/* will read into &conffile struct */
  else if (settings.uname[0])
    bin_to_conf();		/* read our memory from settings[] into conf[] */

#ifndef CYGWIN_HACKS
  if (do_confedit)
    confedit();		/* this will exit() */
#endif /* !CYGWIN_HACKS */
  if (!old_hack)		/* don't parse, just leave it how it was read in. */
    parseconf();

  if (old_hack)
    conf_to_bin(&conffile);	/* this will exit() in write_settings() */

  if (!can_stat(binname))
   werr(ERR_BINSTAT);
  else if (fixmod(binname))
   werr(ERR_BINMOD);
  
#ifndef CYGWIN_HACKS
  move_bin(conffile.binpath, conffile.binname, 1);
#endif /* !CYGWIN_HACKS */

  fillconf(&conf);
#ifdef LEAF
 /*   printf("%s%s%s\n", BOLD(-1), settings.packname, BOLD_END(-1)); */

  if (localhub) {
    if (do_killbot[0]) {
      if (killbot(do_killbot) == 0)
          printf("'%s' successfully killed.\n", do_killbot);
      else
        printf("Error killing '%s'\n", do_killbot);
      exit(0);
    } else {
#endif /* LEAF */
      /* this needs to be both hub/leaf */
      if (update_bin)	{			/* invoked with -u bin */
        if (updating != 2 && conf.bot->pid) {
          kill(conf.bot->pid, SIGKILL);
          unlink(conf.bot->pid_file);
          writepid(conf.bot->pid_file, getpid());
        }
        updatebin(DP_STDOUT, update_bin, 1);	/* will call restart all bots */
        /* never reached */
      }
#ifdef LEAF
      spawnbots();
      if (updating)
        exit(0); /* just let the timer restart us (our parent) */
    }
  }
  if (!localhub)		/* only clear conf on NON localhubs, we need it for cmd_conf */
    free_conf();
#endif /* LEAF */
}

int init_dcc_max(), init_userent(), init_auth(), init_config(), init_party(),
 init_net(), init_botcmd();

static char *fake_md5 = "596a96cc7bf9108cd896f33c44aedc8a";

void console_init();
void ctcp_init();
void update_init();
void notes_init();
#ifdef LEAF
void server_init();
void irc_init();
#endif /* LEAF */
void channels_init();
void compress_init();
void share_init();
void transfer_init();

int main(int argc, char **argv)
{
  egg_timeval_t egg_timeval_now;

  Context;		/* FIXME: wtf is this here for?, probably some old hack to fix a corrupt heap */
/*
  char *out = NULL;
printf("ret: %d\n", system("c:/wraith/leaf.exe"));
  shell_exec("c:\\windows\\notepad.exe", NULL, &out, &out);
printf("out: %s\n", out);
*/
  setlimits();
  init_debug();
  init_signals();		

  if (strcmp(fake_md5, STR("596a96cc7bf9108cd896f33c44aedc8a"))) {
    unlink(argv[0]);
    fatal("!! Invalid binary", 0);
  }

  binname = getfullbinname(argv[0]);

  /* This allows -2/-0 to be used without an initialized binary */
//  if (!(argc == 2 && (!strcmp(argv[1], "-2") || !strcmp(argv[1], "0")))) {
//  doesn't work correctly yet, if we don't go in here, our settings stay encrypted
    check_sum(binname, argc >= 3 && !strcmp(argv[1], "-p") ? argv[2] : NULL);

    if (!checked_bin_buf)
      exit(1);
//  }
  /* Now settings struct is filled */


#ifdef STOP_UAC
  {
    int nvpair[2] = { SSIN_UACPROC, UAC_NOPRINT };

    setsysinfo(SSI_NVPAIRS, (char *) nvpair, 1, NULL, 0);
  }
#endif

  /* Version info! */
  egg_snprintf(ver, sizeof ver, "[%s] Wraith %s", settings.packname, egg_version);
  egg_snprintf(version, sizeof version, "[%s] Wraith %s (%lu)", settings.packname, egg_version, buildts);

  Context;
  /* Initialize variables and stuff */
  timer_update_now(&egg_timeval_now);
  now = egg_timeval_now.sec;

  egg_memcpy(&nowtm, gmtime(&now), sizeof(struct tm));
  lastmin = nowtm.tm_min;
  srandom(now % (getpid() + getppid()) * randint(1000));
  myuid = geteuid();

#ifdef HUB
  egg_snprintf(userfile, 121, "%s/.u", confdir());
#endif /* HUB */

#ifdef HUB
  egg_snprintf(tempdir, sizeof tempdir, "%s/tmp/", confdir());
#endif /* HUB */
#ifdef LEAF 
  egg_snprintf(tempdir, sizeof tempdir, "%s/.../", confdir());
#endif /* LEAF */
#ifdef CYGWIN_HACKS
  egg_snprintf(tempdir, sizeof tempdir, "%s/tmp/", confdir());
#endif /* CYGWIN_HACKS */
  clear_tmp();		/* clear out the tmp dir, no matter if we are localhub or not */
  /* just load everything now, won't matter if it's loaded if the bot has to suicide on startup */
  init_flags();
  binds_init();
  core_binds_init();
  init_dcc_max();
  init_userent();
  init_party();
  init_net();
  init_auth();
  init_config();
  init_botcmd();
  init_conf();
  init_responses();

  if (argc) {
    sdprintf("Calling dtx_arg with %d params.", argc);
    dtx_arg(argc, argv);
  }

  sdprintf("my euid: %d my uuid: %d, my ppid: %d my pid: %d", geteuid(), myuid, getppid(), getpid());

#ifndef CYGWIN_HACKS
  if (checktrace)
    check_trace(1);
#endif /* !CYGWIN_HACKS */

  /* Check and load conf file */
  startup_checks(0);

  if ((localhub && !updating) || !localhub) {
    if ((conf.bot->pid > 0) && conf.bot->pid_file) {
      sdprintf("%s is already running, pid: %d", conf.bot->nick, conf.bot->pid);
      exit(1);
    }
  }

  egg_dns_init();

  channels_init();
#ifdef LEAF
  server_init();
  irc_init();
#endif /* LEAF */
  transfer_init();
  share_init();
  update_init();
  notes_init();
  console_init();
  ctcp_init();
  chanprog();
#ifdef HUB
  cfg_noshare = 1;
  if (!CFG_CHANSET.gdata)
    set_cfg_str(NULL, "chanset", glob_chanset);
  if (!CFG_SERVPORT.gdata)
    set_cfg_str(NULL, "servport", "6667");
  if (!CFG_REALNAME.gdata)
    set_cfg_str(NULL, "realname", "A deranged product of evil coders.");
  cfg_noshare = 0;
#endif /* HUB */

  strcpy(botuser, origbotname);
  trigger_cfg_changed();

#ifdef LEAF
  if (localhub) {
    sdprintf("I am localhub (%s)", conf.bot->nick);
#endif /* LEAF */
#ifndef CYGWIN_HACKS
    if (conffile.autocron)
      check_crontab();
#endif /* !CYGWIN_HACKS */
#ifdef LEAF
  }
#endif /* LEAF */

#if defined(LEAF) && defined(__linux__)
  if (conf.pscloak) {
    const char *p = response(RES_PSCLOAK);

    for (int argi = 0; argi < argc; argi++)
      egg_memset(argv[argi], 0, strlen(argv[argi]));

    strcpy(argv[0], p);
  }
#endif /* LEAF */

  /* Move into background? */
  /* we don't split cygwin because to run as a service the bot shouldn't exit.
     confuses windows ;)
   */
  use_stderr = 0;		/* stop writing to stderr now! */
  if (backgrd) {
#ifndef CYGWIN_HACKS
    pid_t pid = 0;
  
    pid = do_fork();
/*
    printf("  |- %-10s (%d)\n", conf.bot->nick, pid);
    if (localhub) {
      if (bots_ran)
        printf("  `- %d bots launched\n", bots_ran + 1);
      else
        printf("  `- 1 bot launched\n");
    }
*/
    printf("%s[%s%s%s]%s -%s- initiated %s(%s%d%s)%s\n",
           BOLD(-1), BOLD_END(-1), settings.packname, BOLD(-1), BOLD_END(-1), conf.bot->nick,
           BOLD(-1), BOLD_END(-1), pid, BOLD(-1), BOLD_END(-1));

#ifdef lame	/* keeping for god knows why */
    printf("%s%s%c%s%s%s l%sA%su%sN%sc%sH%se%sD%s %s(%s%d%s)%s\n",
            RED(-1), BOLD(-1), conf.bot->nick[0], BOLD_END(-1), &conf.bot->nick[1],
            COLOR_END(-1), BOLD(-1), BOLD_END(-1), BOLD(-1), BOLD_END(-1), BOLD(-1), BOLD_END(-1),
            BOLD(-1), BOLD_END(-1), YELLOW(-1), COLOR_END(-1), pid, YELLOW(-1), COLOR_END(-1));
#endif
  } else {
#endif /* !CYGWIN_HACKS */
#ifdef CYGWIN_HACKS
    FreeConsole();
#endif /* CYGWIN_HACKS */
    printf("%s[%s%s%s]%s -%s- initiated\n", BOLD(-1), BOLD_END(-1), settings.packname, BOLD(-1), BOLD_END(-1), conf.bot->nick);
    writepid(conf.bot->pid_file, getpid());
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
    dcc[n].user = get_user_by_handle(userlist, dcc[n].nick);
    /* Make sure there's an innocuous HQ user if needed */
    if (!dcc[n].user) {
      userlist = adduser(userlist, dcc[n].nick, "none", "-", USER_ADMIN | USER_OWNER | USER_MASTER | USER_VOICE | USER_OP | USER_PARTY | USER_CHUBA | USER_HUBA, 0);
      dcc[n].user = get_user_by_handle(userlist, dcc[n].nick);
    }
    setsock(STDOUT, 0);          /* Entry in net table */
    dprintf(n, "\n### ENTERING DCC CHAT SIMULATION ###\n\n");
    dcc_chatter(n);
  }

  online_since = now;
  autolink_cycle(NULL);		/* Hurry and connect to tandem bots */
  timer_create_secs(1, "core_secondly", (Function) core_secondly);
  timer_create_secs(10, "check_expired_dcc", (Function) check_expired_dcc);
  timer_create_secs(10, "core_10secondly", (Function) core_10secondly);
  timer_create_secs(30, "expire_simuls", (Function) expire_simuls);
  timer_create_secs(60, "core_minutely", (Function) core_minutely);
  timer_create_secs(60, "check_botnet_pings", (Function) check_botnet_pings);
  timer_create_secs(60, "check_expired_ignores", (Function) check_expired_ignores);
  timer_create_secs(3600, "core_hourly", (Function) core_hourly);
  timer_create_secs(1800, "core_halfhourly", (Function) core_halfhourly);

  debug0("main: entering loop");

  int socket_cleanup = 0, status = 0, xx, i = 0, idx = 0;
  char buf[SGRAB + 10] = "";

  while (1) {
#ifndef CYGWIN_HACKS
    if (conf.watcher && waitpid(watcher, &status, WNOHANG))
      fatal("watcher PID died/stopped", 0);
#endif /* !CYGWIN_HACKS */

    /* Lets move some of this here, reducing the numer of actual
     * calls to periodic_timers
     */
    timer_update_now(&egg_timeval_now);
    now = egg_timeval_now.sec;
    random();			/* jumble things up o_O */
    timer_run();

    /* Only do this every so often. */
    if (!socket_cleanup) {
      socket_cleanup = 5;

      /* Check for server or dcc activity. */
      dequeue_sockets();		
    } else
      socket_cleanup--;

    xx = sockgets(buf, &i);

    if (xx >= 0) {		/* Non-error */
      for (idx = 0; idx < dcc_total; idx++) {
	if (dcc[idx].type && dcc[idx].sock == xx) {
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
      }
    } else if (xx == -1) {	/* EOF from someone */
      if (i == STDOUT && !backgrd)
	fatal("END OF FILE ON TERMINAL", 0);
      for (idx = 0; idx < dcc_total; idx++) {
	if (dcc[idx].type && dcc[idx].sock == i) {
          sdprintf("EOF on '%s' idx: %d", dcc[idx].type ? dcc[idx].type->name : "unknown", idx);
	  if (dcc[idx].type->eof)
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
      }
      if (idx == dcc_total) {
	putlog(LOG_MISC, "*", "(@) EOF socket %d, not a dcc socket, not anything.", i);
	close(i);
	killsock(i);
      }
    } else if (xx == -2 && errno != EINTR) {	/* select() error */
      putlog(LOG_MISC, "*", "* Socket error #%d; recovering.", errno);
      for (i = 0; i < dcc_total; i++) {
	if (dcc[i].type && dcc[i].sock != -1 && (fcntl(dcc[i].sock, F_GETFD, 0) == -1) && (errno = EBADF)) {
	  putlog(LOG_MISC, "*",
		 "DCC socket %d (type %s, name '%s') expired -- pfft",
		 dcc[i].sock, dcc[i].type->name, dcc[i].nick);
	  killsock(dcc[i].sock);
	  lostdcc(i);
	  i--;
	}
      }
    } else if (xx == -3) {
#ifdef LEAF
      flush_modes();
#endif /* LEAF */
      socket_cleanup = 0;	/* If we've been idle, cleanup & flush */
    }
  }

  return 0;		/* never reached but what the hell */
}
