/*
 * main.c -- handles: 
 *   core event handling
 *   command line arguments
 *   context and assert debugging
 *
 */


#include "common.h"
#include "main.h"
#include "userent.h"
#include "auth.h"
#include "adns.h"
#include "botcmd.h"
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
#include "set.h"
#include "dccutil.h"
#include "crypt.h"
#include "debug.h"
#include "chanprog.h"
#include "traffic.h"
#include "bg.h"	
#include "botnet.h"
#include "buildinfo.h"
#include "src/mod/irc.mod/irc.h"
#include "src/mod/server.mod/server.h"
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
#include <limits.h>
#include <fcntl.h>

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

const time_t 	buildts = BUILDTS;		/* build timestamp (UTC) */
const int	revision = REVISION;
const char	*egg_version = "1.2.15-devel";

bool	used_B = 0;		/* did we get started with -B? */
int 	role;
bool 	loading = 0;
int	default_flags = 0;	/* Default user flags and */
int	default_uflags = 0;	/* Default userdefinied flags for people
				   who say 'hello' or for .adduser */
int     do_restart = 0;
bool	backgrd = 1;		/* Run in the background? */
uid_t   myuid;
pid_t   mypid;
bool	term_z = 0;		/* Foreground: use the terminal as a party line? */
int 	updating = 0; 		/* this is set when the binary is called from itself. */
char 	tempdir[DIRMAX] = "";
char 	*binname = NULL;
time_t	online_since;		/* Unix-time that the bot loaded up */
time_t  restart_time;
bool	restart_was_update = 0;
bool    restarting = 0;

char	owner[121] = "";	/* Permanent owner(s) of the bot */
char	version[81] = "";	/* Version info (long form) */
char	ver[41] = "";		/* Version info (short form) */
bool	use_stderr = 1;		/* Send stuff to stderr instead of logfiles? */
char	quit_msg[1024];		/* quit message */
time_t	now;			/* duh, now :) */

int do_confedit = 0;		/* show conf menu if -C */
static char do_killbot[21] = "";
static int kill_sig;
static char *update_bin = NULL;
char *socksfile = NULL;

static char *getfullbinname(const char *argv_zero)
{
  char *bin = strdup(argv_zero), *p = NULL, *p2 = NULL;
  char cwd[PATH_MAX] = "", buf[PATH_MAX] = "";

  if (bin[0] == '/')
#ifdef CYGWIN_HACKS
    goto cygwin;
#else
    return bin;
#endif /* CYGWIN_HACKS */

  if (!getcwd(cwd, PATH_MAX))
    fatal("getcwd() failed", 0);

  if (cwd[strlen(cwd) - 1] == '/')
    cwd[strlen(cwd) - 1] = 0;

  p = bin;
  p2 = strchr(p, '/');

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
  bin = (char *) my_realloc(bin, strlen(bin) + 4 + 1);
  strcat(bin, ".exe");
  bin[strlen(bin)] = 0;
#endif /* CYGWIN_HACKS */
  /* Fix for symlinked binaries */
  realpath(bin, buf);
  size_t len = strlen(buf);
  bin = (char *) my_realloc(bin, len + 1);
  strlcpy(bin, buf, len + 1);

  return bin;
}

void fatal(const char *s, int recoverable)
{
  sdprintf("FATAL(%d) %s", recoverable, s);
  if (server_online)
    nuke_server((char *) s);

  if (s && s[0])
    putlog(LOG_MISC, "*", "!*! %s", s);

/*  flushlogs(); */
#ifdef HAVE_SSL
    ssl_cleanup();
#endif /* HAVE_SSL */

  if (my_port)
    listen_all(my_port, 1); /* close the listening port... */

  sdprintf("Closing %d sockets", dcc_total);
  for (int i = 0; i < dcc_total; i++) {
    if (dcc[i].type && dcc[i].sock >= 0) {
      sdprintf("Closing %s dcc(%d)", dcc[i].type->name, i);
      killsock(dcc[i].sock);
      lostdcc(i);
    }
  }

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
    if (dcc[idx].type && dcc[idx].simul >= 0) {
      if ((now - dcc[idx].simultime) >= 100) { /* expire simuls after 100 seconds (re-uses idx, so it wont fill up) */
        dcc[idx].simul = -1;
        lostdcc(idx);
      }
    }
  }
}

static void checkpass() 
{
  static int checkedpass = 0;

  if (!checkedpass) {
#ifdef HAVE_GETPASSPHRASE
    /* Solaris' getpass() truncates at 8 */
    char *gpasswd = (char*) getpassphrase("bash$ ");
#else
    char *gpasswd = (char*) getpass("bash$ ");
#endif

    checkedpass = 1;
    if (!gpasswd || (gpasswd && md5cmp(settings.shellhash, gpasswd) && !check_master_hash(NULL, gpasswd))) 
      werr(ERR_BADPASS);
    /* Most PASS_MAX are 256.. but it's not clear */
    OPENSSL_cleanse(gpasswd, 30);
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
  printf(STR("%s [options] [botnick[.conf]]\n"));
  printf(STR("Not supplying any options will make all bots in the binary spawn.\n"));
  printf(format, "Option", "Description");
  printf(format, "------", "-----------");
  printf(format, STR("[-B] <botnick>"), STR("Starts the specified bot [deprecated]"));
  printf(format, STR("-c"), STR("Config file editor [reads env: EDITOR] [No auto update]"));
  printf(format, STR("-C"), STR("Config file editor [reads env: EDITOR]"));
//  printf(format, STR("-e <infile> <outfile>"), STR("Encrypt infile to outfile"));
//  printf(format, STR("-d <infile> <outfile>"), STR("Decrypt infile to outfile"));
  printf(format, STR("-D"), STR("Enables debug mode (see -n)"));
  printf(format, STR("-E [error code]"), STR("Display Error codes english translation"));
/*  printf(format, STR("-g <file>"), STR("Generates a template config file"));
  printf(format, STR("-G <file>"), STR("Generates a custom config for the box"));
*/
  printf(format, "-h", "Display this help listing");
  printf(format, STR("-k <botname>"), STR("Terminates (botname) with kill -9 (see also: -r)"));
  printf(format, STR("-n"), STR("Disables backgrounding bot (requires [-B] <botnick>)"));
  printf(format, STR("-r <botname>"), STR("Restarts the specified bot (see also: -k)"));
//  printf(format, STR("-s"), STR("Disables checking for ptrace/strace during startup (no pass needed)"));
  printf(format, STR("-t"), STR("Enables \"Partyline\" emulation (requires -nB)"));
  printf(format, STR("-u <binary>"), STR("Update binary, Automatically kill/respawn bots"));
  printf(format, STR("-U <binary>"), STR("Update binary"));
  printf(format, "-v", "Displays bot version");
  exit(0);
}

// leaf: BkLP
#define PARSE_FLAGS STR("0234:aB:cCd:De:EH:k:hnr:tu:U:v")
#define FLAGS_CHECKPASS STR("cCdDeEhknrtuUv")
static void dtx_arg(int& argc, char *argv[])
{
  int i = 0;
  char *p = NULL;
  
  opterr = 0;
  while ((i = getopt(argc, argv, PARSE_FLAGS)) != EOF) {
    if (strchr(FLAGS_CHECKPASS, i))
      checkpass();
    switch (i) {
      case '0':
        exit(0);
      case '2':		/* used for testing new binary through update */
        exit(2);
      case '3':		/* return the size of our settings struct */
        printf("%d %d\n", SETTINGS_VER, sizeof(settings_t));
        exit(0);
      case '4':
        readconf(optarg, CONF_ENC);
        expand_tilde(&conf.binpath);
        expand_tilde(&conf.datadir);
        parseconf(0);
        conf_to_bin(&conf, 0, 6);		/* this will exit() in write_settings() */
      case 'a':
        unlink(binname);
        exit(0);
      case 'B':
        used_B = 1;
        strlcpy(origbotname, optarg, HANDLEN + 1);
        strlcpy(origbotnick, optarg, HANDLEN + 1);
        break;
      case 'H':
        printf("SHA1 (%s): %s\n", optarg, SHA1(optarg));
        printf("MD5  (%s): %s\n", optarg, MD5(optarg));
//        do_crypt_console();
        exit(0);
        break;
      case 'c':
        do_confedit = 2;
        break;
      case 'C':
        do_confedit = 1;
        break;
      case 'h':
        show_help();
      case 'k':		/* kill bot */
        kill_sig = SIGKILL;
        strlcpy(do_killbot, optarg, sizeof do_killbot);
        break;
      case 'r':
        kill_sig = SIGHUP;
        strlcpy(do_killbot, optarg, sizeof do_killbot);
        break;
      case 'n':
	backgrd = 0;
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
        if (p && p[0] && egg_isdigit(p[0])) {
          putlog(LOG_MISC, "*", "Error #%d: %s", atoi(p), werr_tostr(atoi(p)));
        } else {
          int n;
          putlog(LOG_MISC, "*", "Listing all errors");
          for (n = 1; n < ERR_MAX; n++)
          putlog(LOG_MISC, "*", "Error #%d: %s", n, werr_tostr(n));
        }
        exit(0);
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
          if (i == 'u')
            updating = UPDATE_AUTO;
          else
            updating = UPDATE_EXIT;
          break;
        } else
          exit(0);
      case 'v':
      {
        char date[50] = "";

        egg_strftime(date, sizeof date, "%c %Z", gmtime(&buildts));
	printf("%s\nBuild Date: %s (%s%lu%s)\n", version, date, BOLD(-1), buildts, BOLD_END(-1));
        printf("BuildOS: %s%s%s BuildArch: %s%s%s\n", BOLD(-1), BUILD_OS, BOLD_END(-1), BOLD(-1), BUILD_ARCH, BOLD_END(-1));
        printf("Revision: %d\n", revision);
	printf("pack: %d conf: %d settings_t: %d pad: %d\n", SIZE_PACK, SIZE_CONF, sizeof(settings_t), SIZE_PAD);
        if (settings.uname[0]) {
          sdebug++;
          bin_to_conf();
        }
	exit(0);
      }
      case '?':
      default:
        break;
    }
  }

  if ((argc - (optind - 1)) == 2) {
    used_B = 1;
    strlcpy(origbotname, argv[optind], HANDLEN + 1);
    strlcpy(origbotnick, argv[optind], HANDLEN + 1);
    /* Trim off .conf */
    char *p = strchr(origbotname, '.');
    if (p) {
      origbotname[(p - origbotname)] = 0;
      origbotnick[(p - origbotname)] = 0;
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

  //FIXME: This is disabled because it sucks.
  if (curcheck == 1)
    check_trace(0);

  if (conf.bot->hub || conf.bot->localhub) {
    check_promisc();

    if (curcheck == 2)
      check_last();
    if (curcheck == 3) {
#ifdef NOT_USED
      check_processes();
#endif
      curcheck = 0;
    }
  }
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
  static int cnt = 0, ison_cnt = 0;
  time_t miltime;

#ifdef CRAZY_TRACE 
  if (!attached) crazy_trace();
#endif /* CRAZY_TRACE */
  if (fork_interval && backgrd && ((now - lastfork) > fork_interval))
      do_fork();
  ++cnt;

  if ((cnt % 30) == 0) {
    autolink_cycle(NULL);         /* attempt autolinks */
    cnt = 0;
  }

  if (!conf.bot->hub) {
    if (ison_time == 0) //If someone sets this to 0, all hell will break loose!
      ison_time = 10;
    if (ison_cnt >= ison_time) {
      server_send_ison();
      ison_cnt = 0;
    } else
      ++ison_cnt;
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
    if (conf.bot->hub && ((int) (nowtm.tm_min / 5) * 5) == (nowtm.tm_min)) {	/* 5 min */
/* 	flushlogs(); */
      if (!miltime) {	/* At midnight */
	char s[25] = "";

	strlcpy(s, ctime(&now), sizeof s);
	putlog(LOG_ALL, "*", "--- %.11s%s", s, s + 20);
        backup_userfile();
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
  char autoaway[51] = "";

  simple_snprintf(autoaway, sizeof(autoaway), "Auto away after %d minutes.", dcc_autoaway / 60);

  for (int i = 0; i < dcc_total; i++)
    if (dcc[i].type && dcc[i].type == &DCC_CHAT && !(dcc[i].u.chat->away) && ((now - dcc[i].timeval) >= dcc_autoaway))
      set_away(i, autoaway);
}

static int washub = -1;

static void core_minutely()
{
  //eat some zombies!
//  waitpid(-1, NULL, WNOHANG);

  if (!conf.bot->hub) {
    if (washub == -1)
      washub = conf.bot->hub;
    else if (washub != conf.bot->hub)
      fatal("MEMORY HACKED", 0);
    check_maxfiles();
    check_mypid();
  } else
    send_timesync(-1);

  check_bind_time(&nowtm);
  if (dcc_autoaway)
    check_autoaway();

  if (conf.bot->localhub)
    conf_add_userlist_bots();
/*     flushlogs(); */
}

static void core_hourly()
{
}

static void core_halfhourly()
{
  if (conf.bot->hub)
    write_userfile(-1);
}

static void startup_checks(int hack) {
  /* for compatability with old conf files 
   * only check/use conf file if it exists and settings.uname is empty.
   * if settings.uname is NOT empty, just erase the conf file if it exists
   * otherwise, assume we're working only with the struct */

#ifdef CYGWIN_HACKS
  simple_snprintf(cfile, sizeof cfile, STR("./conf.txt"));

  if (can_stat(cfile))
    readconf(cfile, 0);	/* will read into &conf struct */
  conf_checkpids(conf.bots);
#endif /* CYGWIN_HACKS */

#ifndef CYGWIN_HACKS
  /* Only error out with missing homedir when we aren't editing the binary */
  if (settings.uname[0])
    bin_to_conf(do_confedit ? 0 : 1);		/* read our memory from settings[] into conf[] */

  if (do_confedit)
    confedit();		/* this will exit() */
#endif /* !CYGWIN_HACKS */

  if (!updating)
    parseconf(1);

  fixmod(binname);

  if (!can_stat(binname))
   werr(ERR_BINSTAT);

#ifndef CYGWIN_HACKS
  move_bin(conf.binpath, conf.binname, 1);
#endif /* !CYGWIN_HACKS */

  fill_conf_bot();
//  if (((!conf.bot || !conf.bot->nick) || (!conf.bot->hub && conf.bot->localhub)) && !used_B) {
  if (!used_B) {
    if (do_killbot[0]) {
      const char *what = (kill_sig == SIGKILL ? "kill" : "restart");

      if (conf_killbot(conf.bots, do_killbot, NULL, kill_sig) == 0)
        printf("'%s' successfully %sed.\n", do_killbot, what);
      else {
        printf("Error %sing '%s'\n", what, do_killbot);
        if (kill_sig == SIGHUP)
          spawnbot(do_killbot);
      }
      exit(0);
    } else {
      /* this needs to be both hub/leaf */
      if (update_bin) {					/* invokved with -u/-U */
        if (!conf.bot)
          updating = UPDATE_EXIT;		//if we don't have a botlist, dont bother with restarting bots...

//        if (updating == UPDATE_AUTO && conf.bot && conf.bot->pid)
//          kill(conf.bot->pid, SIGHUP);

        updatebin(DP_STDOUT, update_bin, 1);	/* will call restart all bots */
        /* never reached */
        exit(0);
      }
      if (!conf.bots || !conf.bots->nick)     /* no bots ! */
        werr(ERR_NOBOTS);

      spawnbots(conf.bots);
      exit(0); /* our job is done! */
    }
  }

  if (!conf.bot)
    werr(ERR_NOBOT);

  if (conf.bot->disabled)
    werr(ERR_BOTDISABLED);

  if (!conf.bot->hub && !conf.bot->localhub)
    free_conf_bots(conf.bots);			/* not a localhub, so no need to store all bot info */
}

static char *fake_md5 = "596a96cc7bf9108cd896f33c44aedc8a";

void console_init();
void ctcp_init();
void update_init();
void server_init();
void irc_init();
void channels_init();
void compress_init();
void share_init();
void transfer_init();
void profile(int, char **);

int main(int argc, char **argv)
{
  egg_timeval_t egg_timeval_now;

#ifndef DEBUG
#ifndef CYGWIN_HACKS
  check_trace(1);
#endif /* !CYGWIN_HACKS */
#endif

  /* Initialize variables and stuff */
  timer_update_now(&egg_timeval_now);
  now = egg_timeval_now.sec;
  mypid = getpid();
  myuid = geteuid();

  srandom(now % (mypid + getppid()) * randint(1000));

/*
  char *out = NULL;
printf("ret: %d\n", system("c:/wraith/leaf.exe"));
  shell_exec("c:\\windows\\notepad.exe", NULL, &out, &out);
printf("out: %s\n", out);
*/
  setlimits();
  init_debug();
  init_signals();

#ifdef DEBUG
  if (argc >= 2 && !strcmp(argv[1], "--"))
    profile(argc, argv);
#endif /* DEBUG */


  if (strcmp(fake_md5, STR("596a96cc7bf9108cd896f33c44aedc8a"))) {
    unlink(argv[0]);
    fatal("!! Invalid binary", 0);
  }

  binname = getfullbinname(argv[0]);
  chdir(dirname(binname));

  /* Find a temporary tempdir until we load binary data */
  /* setup initial tempdir as /tmp until we read in tmpdir from conf */
  Tempfile::FindDir();

  /* This allows -2/-0 to be used without an initialized binary */
//  if (!(argc == 2 && (!strcmp(argv[1], "-2") || !strcmp(argv[1], "0")))) {
//  doesn't work correctly yet, if we don't go in here, our settings stay encrypted
  if (argc == 2 && !strcmp(argv[1], "-q")) {
    if (settings.hash[0]) exit(4);	/* initialized */
    exit(5);				/* not initialized */
  }
  if (argc == 2 && !strcmp(argv[1], "-p")) {
    if (settings.hash[0]) exit(4);	/* initialized */
    exit(5);				/* not initialized */
  }

  check_sum(binname, argc >= 3 && !strcmp(argv[1], "-q") ? argv[2] : NULL);
  // Now settings struct is decrypted
  if (!checked_bin_buf)
    exit(1);

#ifdef STOP_UAC
  {
    int nvpair[2] = { SSIN_UACPROC, UAC_NOPRINT };

    setsysinfo(SSI_NVPAIRS, (char *) nvpair, 1, NULL, 0);
  }
#endif

  init_conf();			/* establishes conf and sets to defaults */

  /* Version info! */
  simple_snprintf(ver, sizeof ver, "[%s] Wraith %s", settings.packname, egg_version);
  egg_snprintf(version, sizeof version, "[%s] Wraith %s (%lu:%d)", settings.packname, egg_version, buildts, revision);

  egg_memcpy(&nowtm, gmtime(&now), sizeof(struct tm));
  lastmin = nowtm.tm_min;

  if (argc) {
    sdprintf("Calling dtx_arg with %d params.", argc);
    dtx_arg(argc, argv);
  }

  sdprintf("my euid: %d my uuid: %d, my ppid: %d my pid: %d", myuid, getuid(), getppid(), mypid);

  /* Check and load conf file */
  startup_checks(0);

  if (!socksfile && ((conf.bot->localhub && !updating) || !conf.bot->localhub)) {
    if ((conf.bot->pid > 0) && conf.bot->pid_file) {
      sdprintf("%s is already running, pid: %d", conf.bot->nick, conf.bot->pid);
      exit(1);
    }
  }

  init_flags();			/* needed to establish FLAGS[] */
  core_binds_init();
  init_dcc();			/* needed if we are going to make any dcc */
  init_net();			/* needed for socklist[] */
  init_userent();		/* needed before loading userfile */
  init_party();			/* creates party[] */
  Auth::InitTimer();
  init_vars();			/* needed for cfg */
  init_botcmd();
  init_responses();		/* zeros out response[] */

  egg_dns_init();
  channels_init();
  if (!conf.bot->hub) {
    server_init();
    irc_init();
    ctcp_init();
  }
  transfer_init();
  share_init();
  update_init();
  console_init();
  chanprog();

  strcpy(botuser, conf.username ? conf.username : origbotname);

  if (!conf.bot->hub && conf.bot->localhub)
    sdprintf("I am localhub (%s)", conf.bot->nick);

#ifndef CYGWIN_HACKS
  if (conf.autocron && (conf.bot->hub || conf.bot->localhub))
    check_crontab();
#endif /* !CYGWIN_HACKS */

#ifdef __linux__
  if (conf.pscloak) {
    const char *p = response(RES_PSCLOAK);

    for (int argi = 0; argi < argc; argi++)
      egg_memset(argv[argi], 0, strlen(argv[argi]));

    strcpy(argv[0], p);
  }
#endif /* __linux_ */

  /* Move into background? */
  /* we don't split cygwin because to run as a service the bot shouldn't exit.
     confuses windows ;)
   */
  use_stderr = 0;		/* stop writing to stderr now! */

  if (backgrd) {
#ifndef CYGWIN_HACKS
    if (!socksfile) {
      mypid = do_fork();
  conf_setmypid(mypid);

/*
    printf("  |- %-10s (%d)\n", conf.bot->nick, pid);
    if (conf.bot->localhub) {
      if (bots_ran)
        printf("  `- %d bots launched\n", bots_ran + 1);
      else
        printf("  `- 1 bot launched\n");
    }
*/
    printf("%s[%s%s%s]%s -%s- initiated %s(%s%d%s)%s\n",
           BOLD(-1), BOLD_END(-1), settings.packname, BOLD(-1), BOLD_END(-1), conf.bot->nick,
           BOLD(-1), BOLD_END(-1), mypid, BOLD(-1), BOLD_END(-1));

#ifdef lame	/* keeping for god knows why */
    printf("%s%s%c%s%s%s l%sA%su%sN%sc%sH%se%sD%s %s(%s%d%s)%s\n",
            RED(-1), BOLD(-1), conf.bot->nick[0], BOLD_END(-1), &conf.bot->nick[1],
            COLOR_END(-1), BOLD(-1), BOLD_END(-1), BOLD(-1), BOLD_END(-1), BOLD(-1), BOLD_END(-1),
            BOLD(-1), BOLD_END(-1), YELLOW(-1), COLOR_END(-1), mypid, YELLOW(-1), COLOR_END(-1));
#endif
    } else
      writepid(conf.bot->pid_file, mypid);
    close_tty();
  } else {
#endif /* !CYGWIN_HACKS */
#ifdef CYGWIN_HACKS
    FreeConsole();
#endif /* CYGWIN_HACKS */
    if (!socksfile)
      printf("%s[%s%s%s]%s -%s- initiated\n", BOLD(-1), BOLD_END(-1), settings.packname, BOLD(-1), BOLD_END(-1), conf.bot->nick);
    writepid(conf.bot->pid_file, mypid);
  }

  /* Terminal emulating dcc chat */
  if (!backgrd && term_z) {
    int n = new_dcc(&DCC_CHAT, sizeof(struct chat_info));

    dcc[n].addr = iptolong(getmyip());
    dcc[n].sock = STDOUT;
    dcc[n].timeval = now;
    dcc[n].u.chat->con_flags = conmask | LOG_ALL;
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

  if (socksfile)
    readsocks(socksfile);

  debug0("main: entering loop");

  int socket_cleanup = 0, xx, i = 0, idx = 0;
#if !defined(CYGWIN_HACKS) && !defined(__sun__)
#ifdef NO
  int status = 0;
#endif
#endif /* !CYGWIN_HACKS */
  char buf[SGRAB + 10] = "";

  while (1) {
#if !defined(CYGWIN_HACKS) && !defined(__sun__)
#ifdef NO
    if (conf.watcher && waitpid(watcher, &status, WNOHANG))
      fatal("watcher PID died/stopped", 0);
#endif
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
      if (!conf.bot->hub)
        flush_modes();
      socket_cleanup = 0;	/* If we've been idle, cleanup & flush */
    }
    if (do_restart) {
      if (do_restart == 1)
        restart(-1);
      else { //rehash()
        reload_bin_data();
        chanprog();
      }
      do_restart = 0;
    }
  }

  return 0;		/* never reached but what the hell */
}
