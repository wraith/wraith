/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2010 Bryan Drewery
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

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
#include "binds.h"
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

#ifndef _POSIX_SOURCE
/* Solaris needs this */
#define _POSIX_SOURCE
#endif

extern int		optind;

const time_t 	buildts = BUILDTS;		/* build timestamp (UTC) */
const char	*commit = COMMIT;
const char	*branch = BRANCH;
const char	*egg_version = VERSION;
char	git_version[50] = "";

bool	used_B = 0;		/* did we get started with -B? */
bool	safe_to_log = 0;
int 	role;
bool 	loading = 0;
int	default_flags = 0;	/* Default user flags and */
bool     have_linked_to_hub = 0;  /* Have we ever been linked to a hub? */
int	default_uflags = 0;	/* Default userdefinied flags for people
				   who say 'hello' or for .adduser */
int     do_restart = 0;
bool	backgrd = 1;		/* Run in the background? */
uid_t   myuid;
pid_t   mypid;
bool	term_z = 0;		/* Foreground: use the terminal as a party line? */
int 	updating = 0; 		/* this is set when the binary is called from itself. */
char 	tempdir[PATH_MAX] = "";
char 	*binname = NULL;
time_t	online_since;		/* Unix-time that the bot loaded up */
time_t  restart_time;
bool	restart_was_update = 0;
bool    restarting = 0;

char	owner[121] = "";	/* Permanent owner(s) of the bot */
char	version[151] = "";	/* Version info (long form) */
char	ver[101] = "";		/* Version info (short form) */
bool	use_stderr = 1;		/* Send stuff to stderr instead of logfiles? */
char	quit_msg[1024];		/* quit message */
time_t	now;			/* duh, now :) */
egg_timeval_t egg_timeval_now;

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
    goto have_cwd;

  if (!getcwd(cwd, PATH_MAX))
    fatal(STR("getcwd() failed"), 0);

  if (cwd[0] && cwd[strlen(cwd) - 1] == '/')
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
      strlcat(cwd, "/", sizeof(cwd));
      strlcat(cwd, p, sizeof(cwd));
    }
    p = p2;
    if (p)
      p2 = strchr(p, '/');
  }
  str_redup(&bin, cwd);

have_cwd:
  /* Fix for symlinked binaries */
  if (!realpath(bin, buf))
    fatal(STR("realpath() failed on getting current working directory."), 0);
  size_t len = strlen(buf);
  bin = (char *) my_realloc(bin, len + 1);
  strlcpy(bin, buf, len + 1);

  return bin;
}

void fatal(const char *s, int recoverable)
{
  sdprintf(STR("FATAL(%d) %s"), recoverable, s);
  if (server_online)
    nuke_server((char *) s);

  if (s && s[0])
    putlog(LOG_MISC, "*", STR("!*! %s"), s);

/*  flushlogs(); */

  if (my_port)
    listen_all(my_port, 1, 1); /* close the listening port... */

  if (conf.bot && conf.bot->localhub)
    unlink(conf.localhub_socket);

  sdprintf(STR("Closing %d sockets"), dcc_total);
  for (int i = 0; i < dcc_total; i++) {
    if (!backgrd && i == STDOUT) continue;
    if (dcc[i].type && dcc[i].sock >= 0) {
      sdprintf(STR("Closing %s dcc(%d)"), dcc[i].type->name, i);
      killsock(dcc[i].sock);
      lostdcc(i);
    }
  }

#ifdef EGG_SSL_EXT
  if (ssl_use) {
    unload_ssl();
  }
#endif

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

static int checkedpass = 1;
static void checkpass() 
{
  int (*hash_cmp) (const char *, const char *) = NULL;
  size_t hashlen = strlen(settings.shellhash);

  if (hashlen == 32)
    hash_cmp = md5cmp;
  else if (hashlen == 40)
    hash_cmp = sha1cmp;
  else if (hashlen == 47)
    hash_cmp = salted_sha1cmp;

#define SHELL_PROMPT STR("Enter your binary password: ")
#ifdef HAVE_GETPASSPHRASE
  /* Solaris' getpass() truncates at 8 */
  char *gpasswd = (char*) getpassphrase(SHELL_PROMPT);
#else
  char *gpasswd = (char*) getpass(SHELL_PROMPT);
#endif
  if (gpasswd) {
    checkedpass = hash_cmp(settings.shellhash, gpasswd);

    /* Most PASS_MAX are 256.. but it's not clear */
    OPENSSL_cleanse(gpasswd, 30);
  }

  if (checkedpass) {
    OPENSSL_cleanse(&settings, sizeof(settings_t));
    CLEAR_SALTS;
  }

}

static void got_ed(char *, char *, char*) __attribute__((noreturn));

static void got_ed(char *which, char *in, char *out)
{
  sdprintf(STR("got_Ed called: -%s i: %s o: %s"), which, in, out);
  if (!in || !out)
    fatal(STR("Wrong number of arguments: -e/-d <infile> <outfile>"),0);
  if (!strcmp(in, out))
    fatal(STR("<infile> should NOT be the same name as <outfile>"), 0);
  if (!strcmp(which, "e")) {
    Encrypt_File(in, out);
    fatal(STR("File Encryption complete"),3);
  } else if (!strcmp(which, "d")) {
    Decrypt_File(in, out);
    fatal(STR("File Decryption complete"),3);
  }
  exit(0);
}

static void show_help() __attribute__((noreturn));

static void show_help()
{
  const char format[] = "%-30s %-30s\n";

  printf(STR("%s\n\n"), version);
  printf(STR("%s [options] [botnick[.conf]]\n"), binname);
  printf(STR("Not supplying any options will make all bots in the binary spawn.\n"));
  printf(STR("\n"));
  printf(STR("- http://wraith.botpack.net -\n"));
  printf(STR("\n"));
  printf(format, STR("Option"), STR("Description"));
  printf(format, STR("------"), STR("-----------"));
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
  printf(format, STR("-h"), STR("Display this help listing"));
  printf(format, STR("-k <botname>"), STR("Terminates (botname) with kill -9 (see also: -r)"));
  printf(format, STR("-n"), STR("Disables backgrounding bot (requires [-B] <botnick>)"));
  printf(format, STR("-q <pack.cfg>"), STR("Initialize the binary with the given pack.cfg (Can only be done once per binary)"));
  printf(format, STR("-Q"), STR("Securely initialize the binary by reading the PackConfig from stdin (paste). (Can only be done once per binary)"));
  printf(format, STR("-r <botname>"), STR("Restarts the specified bot (see also: -k)"));
//  printf(format, STR("-s"), STR("Disables checking for ptrace/strace during startup (no pass needed)"));
  printf(format, STR("-t"), STR("Enables \"Partyline\" emulation (requires -nB)"));
  printf(format, STR("-u <binary>"), STR("Update binary, Automatically kill/respawn bots"));
  printf(format, STR("-U <binary>"), STR("Update binary"));
  printf(format, STR("-v"), STR("Displays bot version"));
  exit(0);
}

// leaf: BkLP
#define PARSE_FLAGS STR("0234:aB:cCd:De:EH:k:hnr:tu:U:v")
#define FLAGS_CHECKPASS STR("cCdDeEknrtuU")
static void dtx_arg(int& argc, char *argv[])
{
  int i = 0, checked_pass = 0;
  char *p = NULL;
#ifdef DEBUG
  checked_pass = 1;
#endif
  opterr = 0;
  while ((i = getopt(argc, argv, PARSE_FLAGS)) != EOF) {
    if (strchr(FLAGS_CHECKPASS, i) && !checked_pass) {
      checkpass();
      ++checked_pass;
    }
    switch (i) {
      case '0':
        exit(0);
      case '2':		/* used for testing new binary through update */
        exit(2);
      case '4':
        readconf(optarg, CONF_ENC);
        expand_tilde(&conf.datadir);
        parseconf(0);
        conf_to_bin(&conf, 0, 6);		/* this will exit() in write_settings() */
      case 'a':
        unlink(binname);
        exit(0);
      case 'B':
        used_B = 1;
        strlcpy(origbotname, optarg, sizeof(origbotname));
        strlcpy(origbotnick, optarg, sizeof(origbotnick));
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
          putlog(LOG_MISC, "*", STR("Error #%d: %s"), atoi(p), werr_tostr(atoi(p)));
        } else {
          int n;
          putlog(LOG_MISC, "*", STR("Listing all errors"));
          for (n = 1; n < ERR_MAX; n++)
          putlog(LOG_MISC, "*", STR("Error #%d: %s"), n, werr_tostr(n));
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

        strftime(date, sizeof date, "%c %Z", gmtime(&buildts));
	printf(STR("%s\nBuild Date: %s (%s%lu%s)\n"), version, date, BOLD(-1), buildts, BOLD_END(-1));
        printf(STR("BuildOS: %s%s%s BuildArch: %s%s%s\n"), BOLD(-1), BUILD_OS, BOLD_END(-1), BOLD(-1), BUILD_ARCH, BOLD_END(-1));
        printf(STR("- http://wraith.botpack.net -\n"));
#ifdef DEBUG
	printf(STR("pack: %zu conf: %zu settings_t: %zu prefix: %zu pad: %zu needed padding: %zu\n"), SIZE_PACK, SIZE_CONF, sizeof(settings_t), PREFIXLEN, SIZE_PAD, (16 - ((sizeof(settings_t) - sizeof(settings.padding)) % 16)) % 16);
#endif
        if (settings.dynamic_initialized[0]) {
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
    strlcpy(origbotname, argv[optind], sizeof(origbotname));
    strlcpy(origbotnick, argv[optind], sizeof(origbotnick));
    /* Trim off .conf */
    p = strchr(origbotname, '.');
    if (p) {
      origbotname[(p - origbotname)] = 0;
      origbotnick[(p - origbotname)] = 0;
    }
  }
}

/* Timer info */
static int		lastmin = 99;
static struct tm	nowtm;

void core_10secondly()
{
  static int curcheck = 0;

  ++curcheck;

  //FIXME: This is disabled because it sucks.
  if (curcheck == 1)
    check_trace(0);

  if (conf.bot->hub || conf.bot->localhub) {
    if (tands > 0) {
      check_promisc();

      if (curcheck == 2)
        check_last();
    }

    if (curcheck == 3)
      curcheck = 0;
  }
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

	memset(&traffic.out_today, 0, sizeof(traffic.out_today));
	memset(&traffic.in_today, 0, sizeof(traffic.in_today));
}

static void core_secondly()
{
  static int cnt = 0;
  time_t miltime;

  if (fork_interval && backgrd && ((now - lastfork) > fork_interval))
      do_fork();
  ++cnt;

  if (((conf.bot->localhub || conf.bot->hub) && (cnt % 30) == 0) || (cnt % 5) == 0) {
    autolink_cycle();         /* attempt autolinks */
    cnt = 0;
  }

  memcpy(&nowtm, gmtime(&now), sizeof(struct tm));
  if (nowtm.tm_min != lastmin) {
    int i = 0;

    /* Once a minute */
    lastmin = (lastmin + 1) % 60;
    /* In case for some reason more than 1 min has passed: */
    while (nowtm.tm_min != lastmin) {
      /* Timer drift, dammit */
      debug2("timer: drift (lastmin=%d, now=%d)", lastmin, nowtm.tm_min);
      ++i;
      lastmin = (lastmin + 1) % 60;
    }
    if (i > 1)
      putlog(LOG_MISC, "*", "(!) timer drift -- spun %d minutes", i);
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
      fatal(STR("MEMORY HACKED"), 0);
    check_mypid();
  } else if (conf.bot->hub || conf.bot->localhub)
    send_timesync(-1);

  check_bind_time(&nowtm);
  if (dcc_autoaway)
    check_autoaway();

  if (conf.bot->localhub)
    conf_add_userlist_bots();
/*     flushlogs(); */
}

static void core_halfhourly()
{
  if (conf.bot->hub) {
    write_userfile(-1);

    if (link_cleartext)
      putlog(LOG_WARN, "*", STR("'link_cleartext' currently enabled. This is a security risk. Disable with '%sset link_cleartext 0'"), settings.dcc_prefix);
  } else
    check_maxfiles();
}

static void startup_checks(int hack) {
  /* for compatability with old conf files 
   * only check/use conf file if it exists and settings.homedir is empty.
   * if settings.homedir is NOT empty, just erase the conf file if it exists
   * otherwise, assume we're working only with the struct */

  /* Only error out with missing homedir when we aren't editing the binary */
  if (settings.dynamic_initialized[0])
    bin_to_conf(do_confedit ? 0 : 1);		/* read our memory from settings[] into conf[] */

  if (do_confedit)
    confedit();		/* this will exit() */

  if (!updating)
    parseconf(1);

  fixmod(binname);

  if (!can_stat(binname))
   werr(ERR_BINSTAT);

  fill_conf_bot();
//  if (((!conf.bot || !conf.bot->nick) || (!conf.bot->hub && conf.bot->localhub)) && !used_B) {
  if (!used_B) {
    if (do_killbot[0]) {
      const char *what = (kill_sig == SIGKILL ? "kill" : "restart");

      if (conf_killbot(conf.bots, do_killbot, NULL, kill_sig) == 0)
        printf(STR("'%s' successfully %sed.\n"), do_killbot, what);
      else {
        printf(STR("Error %sing '%s'\n"), what, do_killbot);
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

int main(int argc, char **argv)
{

#ifndef DEBUG
  check_trace(1);
#endif

  if (load_libcrypto()) {
    fatal("Unable to load libcrypto.", 0);
  }
  DH1080_init();

  /* Initialize variables and stuff */
  timer_update_now(&egg_timeval_now);
  now = egg_timeval_now.sec;
  mypid = getpid();
  myuid = geteuid();

  srandom(now % (mypid + getppid()) * randint(1000));

  setlimits();
  init_debug();
  init_signals();

  if (strcmp(fake_md5, STR("596a96cc7bf9108cd896f33c44aedc8a"))) {
    unlink(argv[0]);
    fatal(STR("!! Invalid binary"), 0);
  }

  binname = getfullbinname(argv[0]);
  if (chdir(dirname(binname)))
    werr(ERR_BINSTAT);

  /* Find a temporary tempdir until we load binary data */
  /* setup initial tempdir as /tmp until we read in tmpdir from conf */
  if (Tempfile::FindDir() == ERROR)
    werr(ERR_TMPSTAT);

  /* This allows -2/-0 to be used without an initialized binary */
//  if (!(argc == 2 && (!strcmp(argv[1], "-2") || !strcmp(argv[1], "0")))) {
//  doesn't work correctly yet, if we don't go in here, our settings stay encrypted
  if (argc == 2 && !strcmp(argv[1], STR("-q"))) {
    if (settings.hash[0]) exit(4);	/* initialized */
    exit(5);				/* not initialized */
  }
  if (argc == 2 && !strcmp(argv[1], STR("-p"))) {
    if (settings.hash[0]) exit(4);	/* initialized */
    exit(5);				/* not initialized */
  }

  /* return the size of our settings struct */
  if (argc == 2 && !strcmp(argv[1], STR("-3"))) {
    printf("%d %zu\n", SETTINGS_VER, sizeof(settings_t));
    exit(0);
  }

  {
    bool read_stdin = 0;
    if (argc == 2 && !strcmp(argv[1], STR("-Q")))
      read_stdin =1;
    check_sum(binname, argc >= 3 && !strcmp(argv[1], STR("-q")) ? argv[2] : NULL, read_stdin);
  }
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
  simple_snprintf(ver, sizeof(ver), STR("[%s] Wraith %s"), settings.packname, egg_version);
  simple_snprintf(version, sizeof(version), STR("%s%s (%lu)"), ver,
#ifdef DEBUG
      "(d)",
#else
      "",
#endif
      buildts);

  memcpy(&nowtm, gmtime(&now), sizeof(struct tm));
  lastmin = nowtm.tm_min;

  if (argc)
    dtx_arg(argc, argv);

  for (size_t i = 0; i < PREFIXLEN - 1; ++i)
    if (settings.prefix[i] != SETTINGS_HEADER[0])
      werr(ERR_BADPASS);

  sdprintf(STR("my euid: %d my uuid: %d, my ppid: %d my pid: %d"), myuid, getuid(), getppid(), mypid);

  /* Check and load conf file */
  startup_checks(0);

  if (!socksfile && ((conf.bot->localhub && !updating) || !conf.bot->localhub)) {
    if ((conf.bot->pid > 0) && conf.bot->pid_file) {
      sdprintf(STR("%s is already running, pid: %d"), conf.bot->nick, conf.bot->pid);
      exit(1);
    }
  }

  safe_to_log = 1;

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
    ctcp_init();
  }
  transfer_init();
  share_init();
  update_init();
  console_init();
  chanprog();

  strlcpy(botuser, conf.username ? conf.username : origbotname, sizeof(botuser));

  if (!conf.bot->hub && conf.bot->localhub)
    sdprintf(STR("I am localhub (%s)"), conf.bot->nick);

  if (!sdebug && (conf.autocron && (conf.bot->hub || conf.bot->localhub)))
    check_crontab();

  /* Move into background? */
  use_stderr = 0;		/* stop writing to stderr now! */

  if (backgrd) {
    if (!socksfile) {
      mypid = do_fork();
  conf_setmypid(mypid);

    printf(STR("%s[%s%s%s]%s -%s- initiated %s(%s%d%s)%s\n"),
           BOLD(-1), BOLD_END(-1), settings.packname, BOLD(-1), BOLD_END(-1), conf.bot->nick,
           BOLD(-1), BOLD_END(-1), mypid, BOLD(-1), BOLD_END(-1));

    } else
      writepid(conf.bot->pid_file, mypid);
    close_tty();
  } else {
    if (!socksfile)
      printf(STR("%s[%s%s%s]%s -%s- initiated\n"), BOLD(-1), BOLD_END(-1), settings.packname, BOLD(-1), BOLD_END(-1), conf.bot->nick);
    writepid(conf.bot->pid_file, mypid);
  }

  /* Terminal emulating dcc chat */
  if (!backgrd && term_z) {
    int n = new_dcc(&DCC_CHAT, sizeof(struct chat_info));

    setup_HQ(n);

    setsock(STDOUT, 0);          /* Entry in net table */
    dprintf(n, STR("\n### ENTERING DCC CHAT SIMULATION ###\n\n"));
    dcc_chatter(n);
  }

  online_since = now;
  autolink_cycle();		/* Hurry and connect to tandem bots */
  timer_create_secs(1, STR("core_secondly"), (Function) core_secondly);
  timer_create_secs(10, STR("check_expired_dcc"), (Function) check_expired_dcc);
  timer_create_secs(10, STR("core_10secondly"), (Function) core_10secondly);
  timer_create_secs(30, STR("expire_simuls"), (Function) expire_simuls);
  timer_create_secs(60, STR("core_minutely"), (Function) core_minutely);
  timer_create_secs(60, STR("check_botnet_pings"), (Function) check_botnet_pings);
  timer_create_secs(60, STR("check_expired_ignores"), (Function) check_expired_ignores);
  timer_create_secs(1800, STR("core_halfhourly"), (Function) core_halfhourly);

  if (socksfile)
    readsocks(socksfile);

  if (!conf.bot->hub) {
    // Restarting in new method - don't enable irc.mod until after receiving userfile.
    if (reset_chans != 2)
      irc_init();
  }

  debug0(STR("main: entering loop"));

  while (1) {

    /* Lets move some of this here, reducing the numer of actual
     * calls to periodic_timers
     */
    timer_update_now(&egg_timeval_now);
    now = egg_timeval_now.sec;
    random();			/* jumble things up o_O */
    timer_run();

    if (socket_run() == 1) {
       /* Idle calls */
      if (!conf.bot->hub && server_online) {
        flush_modes();
      }
    }

    if (unlikely(do_restart)) {
      if (do_restart == 1)
        restart(-1);
      else { //rehash()
        reload_bin_data();
        rehash_ip();
      }
      do_restart = 0;
    }
  }

  return 0;		/* never reached but what the hell */
}
