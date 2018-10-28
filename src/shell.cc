/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2014 Bryan Drewery
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
 * shell.c -- handles:
 *
 * All shell related functions
 * -shell_exec()
 * -botconfig parsing
 * -check_*()
 * -crontab functions
 *
 */


#include "common.h"
#include "shell.h"
#include "chanprog.h"
#include "set.h"
#include "settings.h"
#include "userrec.h"
#include "net.h"
#include "flags.h"
#include "tandem.h"
#include "main.h"
#include "dccutil.h"
#include "misc.h"
#include "misc_file.h"
#include "bg.h"
#include "stat.h"
#include "users.h"
#include "botnet.h"
#include "src/mod/server.mod/server.h"
#include <bdlib/src/String.h>
#include <bdlib/src/Stream.h>

#include <sys/types.h>
#include <pwd.h>
#include <signal.h>
#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif
#endif
#ifdef HAVE_SYS_PROCCTL_H
#include <sys/procctl.h>
#ifndef PROC_TRACE_CTL
#define PROC_TRACE_CTL		7
#endif
#ifndef PROC_TRACE_STATUS
#define PROC_TRACE_STATUS	8
#endif
#ifndef PROC_TRACE_CTL_DISABLE
#define PROC_TRACE_CTL_DISABLE	2
#endif
#endif	/* HAVE_SYS_PROCCTL_H */
#ifdef HAVE_SYS_PTRACE_H
# include <sys/ptrace.h>
#endif /* HAVE_SYS_PTRACE_H */
#include <sys/wait.h>
#include <sys/utsname.h>
#include <pwd.h>
#include <errno.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include "botmsg.h"

bool clear_tmpdir = 0;

void clear_tmp()
{
  if (!clear_tmpdir)
    return;

  DIR *tmp = NULL;

  if (!(tmp = opendir(tempdir))) 
    return;

  struct dirent *dir_ent = NULL;
  char *file = NULL;
  size_t flen = 0;

  while ((dir_ent = readdir(tmp))) {
    if (strncmp(dir_ent->d_name, ".pid.", 4) && 
        strncmp(dir_ent->d_name, ".u", 2) && 
        strcmp(dir_ent->d_name, ".bin.old") && 
        strncmp(dir_ent->d_name, STR(".socks-"), 7) &&
        strcmp(dir_ent->d_name, ".") && 
        strcmp(dir_ent->d_name, "..")) {

      flen = strlen(dir_ent->d_name) + strlen(tempdir) + 1;
      file = (char *) calloc(1, flen);

      strlcat(file, tempdir, flen);
      strlcat(file, dir_ent->d_name, flen);
      file[strlen(file)] = 0;
      sdprintf("clear_tmp: %s", file);
      unlink(file);
      free(file);
    }
  }
  closedir(tmp);
  return;
}

void check_maxfiles()
{
  int sock = -1, sock1 = -1 , bogus = 0, failed_close = 0;

#ifdef USE_IPV6
  sock1 = getsock(0, AF_INET);		/* fill up any lower avail */
  sock = getsock(0, AF_INET);
#else
  sock1 = getsock(0);
  sock = getsock(0);
#endif

  if (sock1 != -1)
    killsock(sock1);
  
  if (sock == -1) {
    return;
  } else
    killsock(sock);

  bogus = sock - socks_total - 4;	//4 for stdin/stdout/stderr/dns 
 
  if (unlikely(bogus >= 50)) {			/* Attempt to close them */
    sdprintf("SOCK: %d BOGUS: %d SOCKS_TOTAL: %d", sock, bogus, socks_total);

    for (int i = 10; i < sock; i++)	/* dont close lower sockets, they're probably legit */
      if (!findanysnum(i)) {
        if ((close(i)) == -1)			/* try to close the BOGUS fd (likely a KQUEUE) */
          failed_close++;
        else
          bogus--;
      }
    if (bogus >= 150 || failed_close >= 50) {
      if (tands > 0) {
        botnet_send_chat(-1, conf.bot->nick, "Max FD reached, restarting...");
        botnet_send_bye("Max FD reached, restarting...");
      }

      nuke_server("brb");
      cycle_time = 0;
      restart(-1);
    } else if (bogus >= 100 && (bogus % 10) == 0) {
      putlog(LOG_WARN, "*", "* WARNING: $b%d$b bogus file descriptors detected, auto restart at 150", bogus);
    }
  }
}

void check_mypid()
{ 
  pid_t pid = 0;
  
  if (can_stat(conf.bot->pid_file))
    pid = checkpid(conf.bot->nick, NULL);

  if (pid && (pid != getpid()))
    fatal(STR("getpid() does not match pid in file. Possible cloned process, exiting.."), 0);
}



char last_buf[128] = "";

void check_last() {
  if (login == DET_IGNORE)
    return;

  if (conf.username) {
    char *out = NULL, buf[50] = "";

    simple_snprintf(buf, sizeof(buf), STR("last -10 %s"), conf.username);
    if (shell_exec(buf, NULL, &out, NULL, 1)) {
      if (out) {
        char *p = NULL;

        p = strchr(out, '\n');
        if (p)
          *p = 0;
        if (strlen(out) > 10) {
          if (last_buf[0]) {
            if (strncmp(last_buf, out, sizeof(last_buf))) {
              char *work = NULL;
              size_t siz = strlen(out) + 7 + 2 + 1;

              work = (char *) calloc(1, siz);

              simple_snprintf(work, siz, STR("Login: %s"), out);
              detected(DETECT_LOGIN, work);
              free(work);
            }
          }
          strlcpy(last_buf, out, sizeof(last_buf));
        }
        free(out);
      }
    }
  }
}

void check_promisc()
{
#ifdef SIOCGIFCONF
  if (promisc == DET_IGNORE)
    return;

  int sock = socket(AF_INET, SOCK_DGRAM, 0);

  if (sock < 0)
    return;

  struct ifconf ifcnf;
  char buf[1024] = "";

  ifcnf.ifc_len = sizeof(buf);
  ifcnf.ifc_buf = buf;
  if (ioctl(sock, SIOCGIFCONF, &ifcnf) < 0) {
    close(sock);
    return;
  }

  char *reqp = NULL, *end_req = NULL;

  reqp = buf;				/* pointer to start of array */
  end_req = buf + ifcnf.ifc_len;	/* pointer to end of array */
  while (reqp < end_req) { 
    struct ifreq ifreq, *ifr = NULL;

    ifr = reinterpret_cast<struct ifreq *>(reqp);	/* start examining interface */
    ifreq = *ifr;
    if (!ioctl(sock, SIOCGIFFLAGS, &ifreq)) {	/* we can read this interface! */
      /* sdprintf("Examing interface: %s", ifr->ifr_name); */
      if (unlikely(ifreq.ifr_flags & IFF_PROMISC) && strncmp(ifr->ifr_name, "pflog", 5) && strncmp(ifr->ifr_name, "ipfw", 4)) {
        char which[101] = "";

        simple_snprintf(which, sizeof(which), STR("Detected promiscuous mode on interface: %s"), ifr->ifr_name);
        ioctl(sock, SIOCSIFFLAGS, &ifreq);	/* set flags */
        detected(DETECT_PROMISC, which);
	break;
      }
    }
    /* move pointer to next array element (next interface) */
    reqp += sizeof(ifr->ifr_name) + sizeof(ifr->ifr_addr);
  }
  close(sock);
#endif /* SIOCGIFCONF */
}

#ifndef DEBUG
bool traced = 0;

static void got_sigtrap(int z)
{
  traced = 0;
}
#endif

void check_trace(int start)
{
#ifdef DEBUG
#ifdef PR_SET_PTRACER_ANY
  if (start == 1)
    prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);
#endif
  return;
#else
#if defined(HAVE_PROCCTL) && defined(PROC_TRACE_CTL)
  int status = 0;
#endif

  if (start == 0 && trace == DET_IGNORE)
    return;

  pid_t parent = getpid();

#if defined(HAVE_PROCCTL) && defined(PROC_TRACE_CTL)
  /* FreeBSD let's us know if we're being traced already. */
  if (procctl(P_PID, parent, PROC_TRACE_STATUS, &status) == 0 &&
      status > 0) {
    /* status contains the pid of the tracer. Be mean. */
    kill(status, SIGSEGV);
    traced = 1;
  }
#endif

  if (!traced) {
    /* we send ourselves a SIGTRAP, if we recieve, we're not being traced,
     * otherwise we might be. The debugger may smartly just forward the
     * signal if it knows it didn't request it, such as FreeBSD truss
     * after r288903. */
    signal(SIGTRAP, got_sigtrap);
    traced = 1;
    raise(SIGTRAP);
    signal(SIGTRAP, SIG_DFL);
  }

  if (!traced) {
    signal(SIGINT, got_sigtrap);
    traced = 1;
    raise(SIGINT);
    signal(SIGINT, SIG_DFL);
  }

  if (traced) {
    if (start) {
      kill(parent, SIGKILL);
      exit(1);
    } else
      detected(DETECT_TRACE, STR("I'm being traced!"));
  } else {
    if (!start)
      return;

    int tracing_safe = 0;

#if defined(HAVE_PRCTL) && defined(PR_SET_DUMPABLE) && defined(PR_GET_DUMPABLE)
    /* Try to disable ptrace and core dumping entirely. */
    if (prctl(PR_GET_DUMPABLE) == 0 ||
        (prctl(PR_SET_DUMPABLE, 0) == 0 && prctl(PR_GET_DUMPABLE) == 0)) {
      tracing_safe = 1;
    }
#elif defined(HAVE_PROCCTL) && defined(PROC_TRACE_CTL)
    if ((procctl(P_PID, parent, PROC_TRACE_STATUS, &status) == 0 &&
        status == -1) ||
        (status = PROC_TRACE_CTL_DISABLE,
        procctl(P_PID, parent, PROC_TRACE_CTL, &status) == 0)) {
      tracing_safe = 1;
    }
#endif
    if (tracing_safe) {
      /* We're safe! Don't bother with further checks. */
      putlog(LOG_DEBUG, "*", "Ptrace disabled, no longer checking.");
      trace = DET_IGNORE;
      return;
    }

#ifndef __sun__
    int x, i, filedes[2];

    if (pipe(filedes) != 0) {
      /* Could be a temporary failure, don't be harsh. */
      return;
    }

  /* now, let's attempt to ptrace ourself */
    switch ((x = fork())) {
      case -1:
        return;
      case 0:		//child
        char buf[1];

        while (read(filedes[0], buf, sizeof(buf)) != 1)
          ;

        i = ptrace(PT_ATTACH, parent, 0, 0);
        if (i == -1 &&
            /* Linux compat or otherwise removed syscall. Just ignore. */
            errno != ENOSYS &&
        /* EPERM is given on fbsd when security.bsd.unprivileged_proc_debug=0 */
#ifndef __linux__
            errno != EPERM &&
#endif
            errno != EINVAL) {
          if (start) {
            kill(parent, SIGKILL);
            exit(1);
          } else
            detected(DETECT_TRACE, STR("I'm being traced!"));
        } else if (i == 0) {
          waitpid(parent, NULL, 0);
          ptrace(PT_DETACH, parent, (char *) 1, 0);
        }
        exit(0);
      default:		//parent
#ifdef PR_SET_PTRACER
        // Allow the child to debug the parent on Linux 3.4+
        // https://github.com/torvalds/linux/commit/2d514487faf188938a4ee4fb3464eeecfbdcf8eb
        prctl(PR_SET_PTRACER, x, 0, 0, 0);
#endif
        /* Not likely to happen, but make debian FORTIFY_SOURCE happy. */
        if (write(filedes[1], "+", 1) != 1) {
          kill(x, SIGKILL);
        }
        waitpid(x, NULL, 0);
        close(filedes[0]);
        close(filedes[1]);
    }
#endif
  }
#endif	/* !DEBUG */
}

int shell_exec(char *cmdline, char *input, char **output, char **erroutput, bool simple)
{
  if (!cmdline)
    return 0;

  Tempfile *in = NULL, *out = NULL, *err = NULL;
  int x;

  /* Set up temp files */
  in = new Tempfile("in");
  if (!in || in->error) {
//    putlog(LOG_ERRORS, "*" , "exec: Couldn't open '%s': %s", in->file, strerror(errno)); 
    delete in;
    return 0;
  }

  if (input) {
    if (fwrite(input, strlen(input), 1, in->f) != 1) {
//      putlog(LOG_ERRORS, "*", "exec: Couldn't write to '%s': %s", in->file, strerror(errno));
      delete in;
      return 0;
    }
    fseek(in->f, 0, SEEK_SET);
  }

  err = new Tempfile("err");
  if (!err || err->error) {
//    putlog(LOG_ERRORS, "*", "exec: Couldn't open '%s': %s", err->file, strerror(errno));
    delete in;
    delete err;
    return 0;
  }

  out = new Tempfile("out");
  if (!out || out->error) {
//    putlog(LOG_ERRORS, "*", "exec: Couldn't open '%s': %s", out->file, strerror(errno));
    delete in;
    delete err;
    delete out;
    return 0;
  }

  x = fork();
  if (x == -1) {
    putlog(LOG_ERRORS, "*", "exec: fork() failed: %s", strerror(errno));
    delete in;
    delete err;
    delete out;
    return 0;
  }

  if (x) {		/* Parent: wait for the child to complete */
    int st = 0;
    size_t fs = 0;

#ifdef PR_SET_PTRACER
    // Allow the child to debug the parent on Linux 3.4+
    // https://github.com/torvalds/linux/commit/2d514487faf188938a4ee4fb3464eeecfbdcf8eb
    prctl(PR_SET_PTRACER, x, 0, 0, 0);
#endif

    waitpid(x, &st, 0);
    /* child is now complete, read the files into buffers */
    delete in;
    fflush(out->f);
    fflush(err->f);
    if (erroutput) {
      char *buf = NULL;

      fseek(err->f, 0, SEEK_END);
      fs = ftell(err->f);
      if (fs == 0) {
        (*erroutput) = NULL;
      } else {
        buf = (char *) calloc(1, fs + 1);
        fseek(err->f, 0, SEEK_SET);
        if (!fread(buf, 1, fs, err->f))
          fs = 0;
        buf[fs] = 0;
        (*erroutput) = buf;
      }
    }
    delete err;
    if (output) {
      char *buf = NULL;

      fseek(out->f, 0, SEEK_END);
      fs = ftell(out->f);
      if (fs == 0) {
        (*output) = NULL;
      } else {
        buf = (char *) calloc(1, fs + 1);
        fseek(out->f, 0, SEEK_SET);
        if (!fread(buf, 1, fs, out->f))
          fs = 0;
        buf[fs] = 0;
        (*output) = buf;
      }
    }
    delete out;
    return 1;
  } else {
    /* Child: make fd's and set them up as std* */
//    int ind, outd, errd;
//    ind = fileno(inpFile);
//    outd = fileno(outFile);
//    errd = fileno(errFile);

    if (dup2(in->fd, STDIN_FILENO) == (-1)) {
      exit(1);
    }
    if (dup2(out->fd, STDOUT_FILENO) == (-1)) {
      exit(1);
    }
    if (dup2(err->fd, STDERR_FILENO) == (-1)) {
      exit(1);
    }

    // Close all sockets
#ifdef HAVE_CLOSEFROM
    closefrom(3);
#else
    for (int fd = 3; fd < MAX_SOCKETS; ++fd)
      close(fd);
#endif

    char *argv[15];
    if (simple) {
      char *p = NULL;
      int n = 0;
      char *mycmdline = strdup(cmdline);

      while (mycmdline[0] && (p = newsplit(&mycmdline)))
        argv[n++] = p;
      argv[n] = NULL;

    } else {
      argv[0] = "/bin/sh";
      argv[1] = "-c";
      argv[2] = cmdline;
      argv[3] = NULL;
    }

    execvp(argv[0], &argv[0]);
    exit(1);
  }
}

int simple_exec(const char* argv[]) {
  pid_t pid, savedpid;
  int status;

  switch ((pid = vfork())) {
    case -1:
      return -1;
    case 0:		//child
      // Close all sockets
#ifdef HAVE_CLOSEFROM
      closefrom(3);
#else
      for (int fd = 3; fd < MAX_SOCKETS; ++fd)
        close(fd);
#endif
      execvp(argv[0], (char* const*) &argv[0]);
      _exit(127);
    default:		//parent
      savedpid = pid;
      do {
        pid = wait4(savedpid, &status, 0, (struct rusage *)0);
      } while (pid == -1 && errno == EINTR);
      return (pid == -1 ? -1 : status);
  }
}

void suicide(const char *msg)
{
  char tmp[512] = "";

  if (!conf.bot->localhub) {
    //im not a localhub, ask the localhub to suicide
    simple_snprintf(tmp, sizeof(tmp), STR("suicide %s"), msg);
    putbot(conf.localhub, tmp);
    return;
  } else {
    //im the localhub, loop thru bots and kill 'em
    putlog(LOG_WARN, "*", STR("Comitting suicide: %s"), msg);
    crontab_del();

    conf_bot *bot = NULL;
    for (bot = conf.bots; bot && bot->nick; bot = bot->next) {
      if (!strcmp(conf.bot->nick, bot->nick))
        continue; //skip myself or i wont be able to remove the rest
      bot->pid = checkpid(bot->nick, bot);
      conf_killbot(conf.bots, NULL, bot, SIGKILL);
      unlink(bot->pid_file);
      deluser(bot->nick);
    }
  }

  if (!conf.bot->hub) {
    nuke_server(STR("kill the infidels!"));
    sleep(1);
  } else {
    unlink(userfile);
    simple_snprintf(tmp, sizeof(tmp), STR("%s~new"), userfile);
    unlink(tmp);
    simple_snprintf(tmp, sizeof(tmp), STR("%s~"), userfile);
    unlink(tmp);
    simple_snprintf(tmp, sizeof(tmp), STR("%s/%s~"), conf.datadir, userfile);
    unlink(tmp);
    simple_snprintf(tmp, sizeof(tmp), STR("%s/.u.0"), conf.datadir);
    unlink(tmp);
    simple_snprintf(tmp, sizeof(tmp), STR("%s/.u.1"), conf.datadir);
    unlink(tmp);
  }

  unlink(binname);
  //Not recursively clearing these dirs as they may be ~USER/ ..
  unlink(conf.datadir); //Probably will fail, shrug
  unlink(tempdir); //Probably will fail too, oh well

  //now deal with myself after the rest of the conf.bots are gone
  deluser(conf.bot->nick);
  unlink(conf.bot->pid_file);
  //and die in agony!
  fatal(msg, 0);
}

void detected(int code, const char *msg)
{
  char tmp[512] = "";
  struct flag_record fr = { FR_GLOBAL, 0, 0, 0 };
  int act = DET_WARN, do_fatal = 0, killbots = 0;
  
  if (code == DETECT_LOGIN)
    act = login;
  if (code == DETECT_TRACE)
    act = trace;
  if (code == DETECT_PROMISC)
    act = promisc;
  if (code == DETECT_HIJACK)
    act = hijack;

  switch (act) {
  case DET_IGNORE:
    break;
  case DET_WARN:
    putlog(LOG_WARN, "*", "%s", msg);
    break;
  case DET_REJECT:
    do_fork();
    putlog(LOG_WARN, "*", STR("Setting myself +d: %s"), msg);
    simple_snprintf(tmp, sizeof(tmp), "+d: %s", msg);
    set_user(&USERENTRY_COMMENT, conf.bot->u, tmp);
    fr.global = USER_DEOP;
    fr.bot = 1;
    set_user_flagrec(conf.bot->u, &fr, 0);
    sleep(1);
    break;
  case DET_DIE:
    putlog(LOG_WARN, "*", STR("Dying: %s"), msg);
    simple_snprintf(tmp, sizeof(tmp), STR("Dying: %s"), msg);
    set_user(&USERENTRY_COMMENT, conf.bot->u, tmp);
    if (!conf.bot->hub)
      nuke_server(STR("BBL"));
    sleep(1);
    killbots++;
    do_fatal++;
    break;
  case DET_SUICIDE:
    suicide(msg);
    break;
  }
  if (killbots && conf.bot->localhub) {
    conf_checkpids(conf.bots);
    conf_killbot(conf.bots, NULL, NULL, SIGKILL);
  }
  if (do_fatal)
    fatal(msg, 0);
}

const char *werr_tostr(int errnum)
{
  switch (errnum) {
  case ERR_BINSTAT:
    return STR("Cannot access binary");
  case ERR_BADPASS:
    return STR("Incorrect password");
  case ERR_PASSWD:
    return STR("Cannot access the global passwd file");
  case ERR_WRONGBINDIR:
    return STR("Wrong directory/binary name");
  case ERR_DATADIR: 
    return STR("Cannot access datadir.");
  case ERR_TMPSTAT:
    return STR("Cannot access tmp directory.");
  case ERR_TMPMOD:
    return STR("Cannot chmod() tmp directory.");
  case ERR_WRONGUID:
    return STR("UID in binary does not match geteuid()");
  case ERR_WRONGUNAME:
    return STR("Uname in binary does not match uname()");
  case ERR_BADCONF:
    return STR("Config file is incomplete");
  case ERR_BADBOT:
    return STR("No such botnick");
  case ERR_BOTDISABLED:
    return STR("Bot is disabled, remove '/' in config");
  case ERR_NOBOTS:
    return STR("There are no bots in the binary! Please use ./binary -C to edit");
  case ERR_NOHOMEDIR:
    return STR("There is no homedir set. Please set one in the binary config with ./binary -C");
  case ERR_NOUSERNAME:
    return STR("There is no username set. Please set one in the binary config with ./binary -C");
  case ERR_NOBOT:
    return STR("I have no bot record but received -B???");
  case ERR_NOTINIT:
    return STR("Binary data is not initialized; try ./binary -C");
  case ERR_TOOMANYBOTS:
    return STR("Too many bots defined. 5 max. Too many will lead to klines.\nSpread out into multiple accounts/shells/ip ranges.");
  case ERR_LIBS:
    return STR("Failed to load required libraries.\nEnsure that 32bit glibc, OpenSSL and libgcc are installed.\nhttps://github.com/wraith/wraith/wiki/Binary-Compatibiliy");
  case ERR_ALREADYINIT:
    return STR("Binary settings already written.");
  default:
    return STR("Unforseen error");
  }

}

void werr(int errnum)
{
/* [1]+  Done                    ls --color=auto -A -CF
   [1]+  Killed                  bash
*/
/*
  int x = 0;
  unsigned long job = randint(2) + 1; */

/*  printf("[%lu] %lu%lu%lu%lu%lu\n", job, randint(2) + 1, randint(8) + 1, randint(8) + 1, errnum); */

/*    printf("\n[%lu]+  Killed                  rm -rf /\r\n", job); */
  /*
  if (checkedpass) {
    printf("[%lu] %d\n", job, getpid());
  }
  */
  /*  printf("\n[%lu]+  Stopped                 %s\r\n", job, basename(binname)); */
#ifdef OBSCURE_ERRORS
  sdprintf(STR("Error %d: %s"), errnum, werr_tostr(errnum));
  printf(STR("*** Error code %d\n\n"), errnum);
  printf(STR("Segmentation fault\n"));
#else
  fprintf(stderr, STR("Error %d: %s\n"), errnum, werr_tostr(errnum));
#endif
  fatal("", 0);
  exit(1); // This is never reached, done for gcc() warnings
}

const char *homedir(bool useconf)
{
  static char homedir_buf[PATH_MAX] = "";

  if (!homedir_buf[0]) {
    if (conf.homedir && useconf)
      simple_snprintf(homedir_buf, sizeof homedir_buf, "%s", conf.homedir);
    else {
    char *home = getenv("HOME");
    if (home && strlen(home))
      strlcpy(homedir_buf, home, sizeof(homedir_buf));
    }
  }
  return homedir_buf[0] ? homedir_buf : NULL;
}

const char *my_username()
{
  static char username[DIRMAX] = "";

  if (!username[0]) {
    char *user = getenv("USER");
    if (user && strlen(user))
      strlcpy(username, user, sizeof(username));
  }
  return username[0] ? username : NULL;
}

int mkdir_p(const char *dir) {
  char *p = NULL, *path = NULL;

  path = p = strdup(dir);

  do {
    p = strchr(p + 1, '/');
    if (p)
      *p = '\0';
    if (can_stat(path) && !is_dir(path))
      unlink(path);
    if (!can_stat(path)) {
      if (mkdir(path, S_IRUSR | S_IWUSR | S_IXUSR)) {
        unlink(path);
        mkdir(path, S_IRUSR | S_IWUSR | S_IXUSR);
      }
    }
    if (p)
      *p = '/';
  } while(p);

  int couldStat = can_stat(path);

  free(path);

  return couldStat;
}

void expand_tilde(char **ptr)
{
  if (!conf.homedir || !conf.homedir[0])
    return;

  char *str = ptr ? *ptr : NULL;

  if (str && strchr(str, '~')) {
    char *p = NULL;
    size_t siz = strlen(str);

    if (str[siz - 1] == '/')
      str[siz - 1] = 0;

    if ((p = replace(str, "~", conf.homedir)))
      str_redup(ptr, p);
    else
      fatal("Unforseen error expanding '~'", 0);
  }
}

void check_crontab()
{
  int i = 0;

  if (!conf.bot->hub && !conf.bot->localhub)
    fatal(STR("something is wrong."), 0);
  if (!(i = crontab_exists())) {
    crontab_create(5);
    if (!(i = crontab_exists()))
      printf(STR("* Error writing crontab entry.\n"));
  }
}

static void crontab_install(bd::Stream& crontab) {
  // Write out the new crontab
  Tempfile new_crontab = Tempfile("crontab");
  crontab.writeFile(new_crontab.fd);

  // Install new crontab
  const char* argv[] = {"crontab", new_crontab.file, 0};
  simple_exec(argv);
}

void crontab_del() {
  bd::Stream crontab;
  if (crontab_exists(&crontab, 1) == 1)
    crontab_install(crontab);
}

int crontab_exists(bd::Stream* crontab, bool excludeSelf) {
  char *out = NULL;
  int ret = -1;

  if (shell_exec("crontab -l", NULL, &out, NULL, 1)) {
    if (out) {
      bd::Stream stream(out);
      bd::String line;

      ret = 0;
      while (stream.tell() < stream.length()) {
        line = stream.getline();
        if (line[0] != '#' && line.find(binname) != bd::String::npos) {
          ret = 1;

          // Need to continue if the existing crontab is requested
          if (crontab && !excludeSelf)
            (*crontab) << line;
          else if (!crontab)
            break;
        } else if (crontab)
          (*crontab) << line;
      }

      free(out);
    } else
      ret = 0;
  }
  return ret;
}

char s1_2[3] = "",s1_8[3] = "",s2_5[3] = "";

void crontab_create(int interval) {
  bd::Stream crontab;

  if (crontab_exists(&crontab) == 1)
    return;

  char buf[1024] = "";
  if (interval == 1)
    strlcpy(buf, "*", 2);
  else {
    int i = 1;
    int si = randint(interval);

    while (i < 60) {
      if (buf[0])
        strlcat(buf, ",", sizeof(buf));
      simple_snprintf(&buf[strlen(buf)], sizeof(buf) - strlen(buf), "%i", (i + si) % 60);
      i += interval;
    }
  }
  simple_snprintf(buf + strlen(buf), sizeof buf, STR(" * * * * %s > /dev/null 2>&1\n"), shell_escape(binname));

  crontab << buf;
  crontab_install(crontab);
}

int det_translate(const char *word)
{
  if (word && word[0]) {
    if (!strcasecmp(word, STR("ignore")))
      return DET_IGNORE;
    else if (!strcasecmp(word, STR("warn")))
      return DET_WARN;
    else if (!strcasecmp(word, STR("reject")))
      return DET_REJECT;
    else if (!strcasecmp(word, STR("die")))
      return DET_DIE;
    else if (!strcasecmp(word, STR("suicide")))
      return DET_SUICIDE;
  }
  return DET_IGNORE;
}

const char *det_translate_num(int num)
{
  switch (num) {
    case DET_IGNORE: return STR("ignore");
    case DET_WARN:   return STR("warn");
    case DET_REJECT: return STR("reject");
    case DET_DIE:    return STR("die");
    case DET_SUICIDE:return STR("suicide");
    default:         return STR("ignore");
  }
  return STR("ignore");
}

char *shell_escape(const char *path)
{
  static char ret1[DIRMAX] = "", ret2[DIRMAX] = "", *ret = NULL;
  static bool alt = 0;
  char *c = NULL;

  if (alt) {
    alt = 0;
    ret = ret1;
  } else {
    alt = 1;
    ret = ret2;
  }

  ret[0] = 0;

  for (c = (char *) path; c && *c; ++c) {
    if (strchr(ESCAPESHELL, *c))
      simple_snprintf(&ret[strlen(ret)], sizeof(ret1) - strlen(ret), "\\%c", *c);
    else
      simple_snprintf(&ret[strlen(ret)], sizeof(ret1) - strlen(ret), "%c", *c);
  }

  return ret;
}

/* vim: set sts=2 sw=2 ts=8 et: */
