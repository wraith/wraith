/*
 * transfer.c -- part of transfer.mod
 *
 */
/*
 * Small code snippets related to REGET/RESEND support were taken from
 * BitchX, copyright by panasync.
 */

#define MODULE_NAME "transfer"
#define MAKING_TRANSFER

/* sigh sunos */
#include <sys/types.h>
#include <sys/stat.h>
#include "src/mod/module.h"
#include "src/tandem.h"
#include "src/mod/share.mod/share.h"
#include "src/net.h"

#include "src/users.h"
#include "transfer.h"

#include "src/mod/update.mod/update.h"

#include <netinet/in.h>
#include <arpa/inet.h>

extern int		bupdating;

static bind_table_t *BT_rcvd = NULL, *BT_sent = NULL, *BT_lost = NULL, *BT_tout = NULL;

static Function *global = NULL;

static int copy_to_tmp = 1;	/* Copy files to /tmp before transmitting? */
static int wait_dcc_xfer = 40;	/* Timeout time on DCC xfers */
static int dcc_limit = 4;	/* Maximum number of simultaneous file
				   downloads allowed */
static int dcc_block = 0;	/* Size of one dcc block */
static int quiet_reject = 1;        /* Quietly reject dcc chat or sends from
                                   users without access? */

/*
 * Prototypes
 */
static void stats_add_dnload(struct userrec *, unsigned long);
static void stats_add_upload(struct userrec *, unsigned long);
static void wipe_tmp_filename(char *, int);
static int at_limit(char *);
static void dcc_get_pending(int, char *, int);
struct dcc_table DCC_SEND;
struct dcc_table DCC_GET;
struct dcc_table DCC_GET_PENDING;

static fileq_t *fileq = NULL;


/*
 *   Misc functions
 */

#undef MATCH
#define MATCH (match+sofar)

/* This function SHAMELESSLY :) pinched from match.c in the original
 * source, see that file for info about the author etc.
 */

#define QUOTE '\\'
#define WILDS '*'
#define WILDQ '?'
#define NOMATCH 0
/*
 * wild_match_file(char *ma, char *na)
 *
 * Features:  Forward, case-sensitive, ?, *
 * Best use:  File mask matching, as it is case-sensitive
 */
static int wild_match_file(register char *m, register char *n)
{
  char *ma = m, *lsm = NULL, *lsn = NULL;
  int match = 1;
  register unsigned int sofar = 0;

  /* Take care of null strings (should never match) */
  if ((m == 0) || (n == 0) || (!*n))
    return NOMATCH;
  /* (!*m) test used to be here, too, but I got rid of it.  After all, If
   * (!*n) was false, there must be a character in the name (the second
   * string), so if the mask is empty it is a non-match.  Since the
   * algorithm handles this correctly without testing for it here and this
   * shouldn't be called with null masks anyway, it should be a bit faster
   * this way.
   */
  while (*n) {
    /* Used to test for (!*m) here, but this scheme seems to work better */
    switch (*m) {
    case 0:
      do
	m--;			/* Search backwards      */
      while ((m > ma) && (*m == '?'));	/* For first non-? char  */
      if ((m > ma) ? ((*m == '*') && (m[-1] != QUOTE)) : (*m == '*'))
	return MATCH;		/* nonquoted * = match   */
      break;
    case WILDS:
      do
	m++;
      while (*m == WILDS);	/* Zap redundant wilds   */
      lsm = m;
      lsn = n;			/* Save * fallback spot  */
      match += sofar;
      sofar = 0;
      continue;			/* Save tally count      */
    case WILDQ:
      m++;
      n++;
      continue;			/* Match one char        */
    case QUOTE:
      m++;			/* Handle quoting        */
    }
    if (*m == *n) {		/* If matching           */
      m++;
      n++;
      sofar++;
      continue;			/* Tally the match       */
    }
    if (lsm) {			/* Try to fallback on *  */
      n = ++lsn;
      m = lsm;			/* Restore position      */
      /* Used to test for (!*n) here but it wasn't necessary so it's gone */
      sofar = 0;
      continue;			/* Next char, please     */
    }
    return NOMATCH;		/* No fallbacks=No match */
  }
  while (*m == WILDS)
    m++;			/* Zap leftover *s       */
  return (*m) ? NOMATCH : MATCH;	/* End of both = match   */
}

static void wipe_tmp_filename(char *fn, int idx)
{
  int i, ok = 1;

  if (!copy_to_tmp)
    return;
  for (i = 0; i < dcc_total; i++)
    if (i != idx)
      if (dcc[i].type == &DCC_GET || dcc[i].type == &DCC_GET_PENDING)
	if (!strcmp(dcc[i].u.xfer->filename, fn)) {
	  ok = 0;
	  break;
	}
  if (ok)
    unlink(fn);
}

/* Return true if this user has >= the maximum number of file xfers allowed.
 */
static int at_limit(char *nick)
{
  int i, x = 0;

  for (i = 0; i < dcc_total; i++)
    if (dcc[i].type == &DCC_GET || dcc[i].type == &DCC_GET_PENDING)
      if (!egg_strcasecmp(dcc[i].nick, nick))
	x++;
  return (x >= dcc_limit);
}

/* Replaces all spaces with underscores (' ' -> '_').  The returned buffer
 * needs to be freed after use.
 */
static char *replace_spaces(char *fn)
{
  register char *ret = NULL, *p = NULL;

  p = ret = strdup(fn);
  while ((p = strchr(p, ' ')) != NULL)
    *p = '_';
  return ret;
}



static void check_sentrcvd(struct userrec *u, char *nick, char *path,
			       bind_table_t *table)
{
  struct flag_record fr = {FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0, 0};
  char *hand = u ? u->handle : "*";	/* u might be NULL. */

  get_user_flagrec(u, &fr, NULL);
  check_bind(table, hand, &fr, u, nick, path);
}

static void check_toutlost(struct userrec *u, char *nick, char *path,
			       unsigned long acked, unsigned long length,
				bind_table_t *table)
{
  struct flag_record fr = {FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0, 0};
  char *hand = u ? u->handle : "*";	/* u might be NULL. */

  get_user_flagrec(u, &fr, NULL);
  check_bind(table, hand, &fr, u, nick, path, acked, length);
}

/*
 *    File queue functions
 */

static void queue_file(char *dir, char *file, char *from, char *to)
{
  fileq_t *q = fileq;

  fileq = (fileq_t *) calloc(1, sizeof(fileq_t));
  fileq->next = q;
  fileq->dir = strdup(dir);
  fileq->file = strdup(file);
  strcpy(fileq->nick, from);
  strcpy(fileq->to, to);
}

static void deq_this(fileq_t *this)
{
  fileq_t *q = fileq, *last = NULL;

  while (q && q != this) {
    last = q;
    q = q->next;
  }
  if (!q)
    return;			/* Bogus ptr */
  if (last)
    last->next = q->next;
  else
    fileq = q->next;
  free(q->dir);
  free(q->file);
  free(q);
}

/* Remove all files queued to a certain user.
 */
static void flush_fileq(char *to)
{
  fileq_t *q = fileq;
  int fnd = 1;

  while (fnd) {
    q = fileq;
    fnd = 0;
    while (q != NULL) {
      if (!egg_strcasecmp(q->to, to)) {
	deq_this(q);
	q = NULL;
	fnd = 1;
      }
      if (q != NULL)
	q = q->next;
    }
  }
}

static void send_next_file(char *to)
{
  fileq_t *q = NULL, *this = NULL;
  char *s = NULL, *s1 = NULL;
  int x;

  for (q = fileq; q; q = q->next)
    if (!egg_strcasecmp(q->to, to))
      this = q;
  if (this == NULL)
    return;			/* None */
  /* Copy this file to /tmp */
  if (this->dir[0] == '*') {	/* Absolute path */
    s = calloc(1, strlen(&this->dir[1]) + strlen(this->file) + 2);
    sprintf(s, "%s/%s", &this->dir[1], this->file);
  } else {
    char *p = strchr(this->dir, '*');

    if (p == NULL) {		/* if it's messed up */
      send_next_file(to);
      return;
    }
    p++;
    s = calloc(1, strlen(p) + strlen(this->file) + 2);
    sprintf(s, "%s%s%s", p, p[0] ? "/" : "", this->file);
    strcpy(this->dir, &(p[atoi(this->dir)]));
  }
  if (copy_to_tmp) {
    s1 = calloc(1, strlen(tempdir) + strlen(this->file) + 1);
    sprintf(s1, "%s%s", tempdir, this->file);
    if (copyfile(s, s1) != 0) {
      putlog(LOG_FILES | LOG_MISC, "*",
	     TRANSFER_COPY_FAILED,
	     this->file, tempdir);
      dprintf(DP_HELP,
	      TRANSFER_FILESYS_BROKEN,
	      this->to);
      strcpy(s, this->to);
      flush_fileq(s);
      free(s1);
      free(s);
      return;
    }
  } else {
    s1 = strdup(s);
  }
  if (this->dir[0] == '*') {
    s = realloc(s, strlen(&this->dir[1]) + strlen(this->file) + 2);
    sprintf(s, "%s/%s", &this->dir[1], this->file);
  } else {
    s = realloc(s, strlen(this->dir) + strlen(this->file) + 2);
    sprintf(s, "%s%s%s", this->dir, this->dir[0] ? "/" : "", this->file);
  }
  x = raw_dcc_send(s1, this->to, this->nick, s);
  if (x == DCCSEND_OK) {
    if (egg_strcasecmp(this->to, this->nick))
      dprintf(DP_HELP, TRANSFER_FILE_ARRIVE, this->to,
	      this->nick);
    deq_this(this);
    free(s);
    free(s1);
    return;
  }
  wipe_tmp_filename(s1, -1);
  if (x == DCCSEND_FULL) {
    putlog(LOG_FILES, "*",TRANSFER_LOG_CONFULL, s1, this->nick);
    dprintf(DP_HELP,
	    TRANSFER_NOTICE_CONFULL,
	    this->to);
    strcpy(s, this->to);
    flush_fileq(s);
  } else if (x == DCCSEND_NOSOCK) {
    putlog(LOG_FILES, "*", TRANSFER_LOG_SOCKERR, s1, this->nick);
    dprintf(DP_HELP, TRANSFER_NOTICE_SOCKERR,
	    this->to);
    strcpy(s, this->to);
    flush_fileq(s);
  } else {
    if (x == DCCSEND_FEMPTY) {
      putlog(LOG_FILES, "*", TRANSFER_LOG_FILEEMPTY, this->file);
      dprintf(DP_HELP, TRANSFER_NOTICE_FILEEMPTY,
	      this->to, this->file);
    }
    deq_this(this);
  }
  free(s);
  free(s1);
  return;
}

static void show_queued_files(int idx)
{
  int i, cnt = 0, len;
  char spaces[] = "                                 ";
  fileq_t *q = NULL;

  for (q = fileq; q; q = q->next) {
    if (!egg_strcasecmp(q->nick, dcc[idx].nick)) {
      if (!cnt) {
	spaces[HANDLEN - 9] = 0;
	dprintf(idx, TRANSFER_SEND_TO , spaces);
	dprintf(idx, TRANSFER_LINES , spaces);
	spaces[HANDLEN - 9] = ' ';
      }
      cnt++;
      spaces[len = HANDLEN - strlen(q->to)] = 0;
      if (q->dir[0] == '*')
	dprintf(idx, "  %s%s  %s/%s\n", q->to, spaces, &q->dir[1],
		q->file);
      else
	dprintf(idx, "  %s%s  /%s%s%s\n", q->to, spaces, q->dir,
		q->dir[0] ? "/" : "", q->file);
      spaces[len] = ' ';
    }
  }
  for (i = 0; i < dcc_total; i++) {
    if ((dcc[i].type == &DCC_GET_PENDING || dcc[i].type == &DCC_GET) &&
	(!egg_strcasecmp(dcc[i].nick, dcc[idx].nick) ||
	 !egg_strcasecmp(dcc[i].u.xfer->from, dcc[idx].nick))) {
      char *nfn;

      if (!cnt) {
	spaces[HANDLEN - 9] = 0;
	dprintf(idx, TRANSFER_SEND_TO , spaces);
	dprintf(idx, TRANSFER_LINES , spaces);
	spaces[HANDLEN - 9] = ' ';
      }
      nfn = strrchr(dcc[i].u.xfer->origname, '/');
      if (nfn == NULL)
	nfn = dcc[i].u.xfer->origname;
      else
	nfn++;
      cnt++;
      spaces[len = HANDLEN - strlen(dcc[i].nick)] = 0;
      if (dcc[i].type == &DCC_GET_PENDING)
	dprintf(idx,TRANSFER_WAITING, dcc[i].nick, spaces,
		nfn);
      else
	dprintf(idx,TRANSFER_DONE, dcc[i].nick, spaces,
		nfn, (100.0 * ((float) dcc[i].status /
			       (float) dcc[i].u.xfer->length)));
      spaces[len] = ' ';
    }
  }
  if (!cnt)
    dprintf(idx,TRANSFER_QUEUED_UP);
  else
    dprintf(idx,TRANSFER_TOTAL, cnt);
}

static void fileq_cancel(int idx, char *par)
{
  int fnd = 1, matches = 0, atot = 0, i;
  fileq_t *q = NULL;
  char *s = NULL;

  while (fnd) {
    q = fileq;
    fnd = 0;
    while (q != NULL) {
      if (!egg_strcasecmp(dcc[idx].nick, q->nick)) {
	s = realloc(s, strlen(q->dir) + strlen(q->file) + 3);
	if (q->dir[0] == '*')
	  sprintf(s, "%s/%s", &q->dir[1], q->file);
	else
	  sprintf(s, "/%s%s%s", q->dir, q->dir[0] ? "/" : "", q->file);
	if (wild_match_file(par, s)) {
	  dprintf(idx,TRANSFER_CANCELLED, s, q->to);
	  fnd = 1;
	  deq_this(q);
	  q = NULL;
	  matches++;
	}
	if (!fnd && wild_match_file(par, q->file)) {
	  dprintf(idx,TRANSFER_CANCELLED, s, q->to);
	  fnd = 1;
	  deq_this(q);
	  q = NULL;
	  matches++;
	}
      }
      if (q != NULL)
	q = q->next;
    }
  }
  if (s)
    free(s);
  for (i = 0; i < dcc_total; i++) {
    if ((dcc[i].type == &DCC_GET_PENDING || dcc[i].type == &DCC_GET) &&
	(!egg_strcasecmp(dcc[i].nick, dcc[idx].nick) ||
	 !egg_strcasecmp(dcc[i].u.xfer->from, dcc[idx].nick))) {
      char *nfn = strrchr(dcc[i].u.xfer->origname, '/');

      if (nfn == NULL)
	nfn = dcc[i].u.xfer->origname;
      else
	nfn++;
      if (wild_match_file(par, nfn)) {
	dprintf(idx,TRANSFER_ABORT_DCCSEND, nfn);
	if (egg_strcasecmp(dcc[i].nick, dcc[idx].nick))
	  dprintf(DP_HELP, TRANSFER_NOTICE_ABORT,
		  dcc[i].nick, nfn, dcc[idx].nick);
	if (dcc[i].type == &DCC_GET)
	  putlog(LOG_FILES, "*",TRANSFER_DCC_CANCEL, nfn,
		 dcc[i].nick, dcc[i].status, dcc[i].u.xfer->length);
	wipe_tmp_filename(dcc[i].u.xfer->filename, i);
	atot++;
	matches++;
	killsock(dcc[i].sock);
	lostdcc(i);
      }
    }
  }
  if (!matches)
    dprintf(idx,TRANSFER_NO_MATCHES);
  else
    dprintf(idx, TRANSFER_CANCELLED_FILE, matches, (matches != 1) ? "s" : "");
  for (i = 0; i < atot; i++)
    if (!at_limit(dcc[idx].nick))
      send_next_file(dcc[idx].nick);
}


/*
 *    DCC routines
 */

/* Instead of reading all data intended to go into the DCC block
 * in one go, we read it in PMAX_SIZE chunks, feed it to tputs and
 * continue until we get to know that the network buffer only
 * buffers the data instead of sending it.
 *
 * In that case, we delay further sending until we receive the
 * dcc outdone event.
 *
 * Note: To optimize buffer sizes, we default to PMAX_SIZE, but
 *       allocate a smaller buffer for smaller pending_data sizes.
 */
#define	PMAX_SIZE	4096
static unsigned long pump_file_to_sock(FILE *file, long sock,
				       register unsigned long pending_data)
{
  const unsigned long		 buf_len = pending_data >= PMAX_SIZE ?
	  					PMAX_SIZE : pending_data;
  char				*bf = calloc(1, buf_len);
  register unsigned long	 actual_size;

  if (bf) {
    do {
      actual_size = pending_data >= buf_len ? buf_len : pending_data;
      fread(bf, actual_size, 1, file);
      tputs(sock, bf, actual_size);
      pending_data -= actual_size;
    } while (!sock_has_data(SOCK_DATA_OUTGOING, sock) && pending_data != 0);
    free(bf);
  }
  return pending_data;
}

void eof_dcc_fork_send(int idx)
{
  char s1[121] = "", *s2 = NULL;

  fclose(dcc[idx].u.xfer->f);
  if (!strcmp(dcc[idx].nick, "*users")) {
    int x, y = 0;

    for (x = 0; x < dcc_total; x++)
      if ((!egg_strcasecmp(dcc[x].nick, dcc[idx].host)) &&
	  (dcc[x].type->flags & DCT_BOT)) {
	y = x;
	break;
      }
    if (y != 0) {
      dcc[y].status &= ~STAT_GETTING;
      dcc[y].status &= ~STAT_SHARE;
    }
    putlog(LOG_BOTS, "*", USERF_FAILEDXFER);
    unlink(dcc[idx].u.xfer->filename);
  } else if (!strcmp(dcc[idx].nick, "*binary")) {
    int x, y = 0;

    for (x = 0; x < dcc_total; x++)
      if ((!egg_strcasecmp(dcc[x].nick, dcc[idx].host)) &&
	  (dcc[x].type->flags & DCT_BOT)) {
	y = x;
	break;
      }
    if (y != 0) {
      dcc[y].status &= ~STAT_GETTINGU;
    }
    putlog(LOG_BOTS, "*", "Failed binary transfer.");
  } else {
    neterror(s1);
    if (!quiet_reject)
      dprintf(DP_HELP, "NOTICE %s :%s (%s)\n", dcc[idx].nick,
	      DCC_CONNECTFAILED1, s1);
    putlog(LOG_MISC, "*", "%s: SEND %s (%s!%s)", DCC_CONNECTFAILED2,
	   dcc[idx].u.xfer->origname, dcc[idx].nick, dcc[idx].host);
    putlog(LOG_MISC, "*", "    (%s)", s1);
    s2 = calloc(1, strlen(tempdir) + strlen(dcc[idx].u.xfer->filename) + 1);
    sprintf(s2, "%s%s", tempdir, dcc[idx].u.xfer->filename);
    unlink(s2);
    free(s2);
  }
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void eof_dcc_send(int idx)
{
  int ok, j;
  char *ofn = NULL, *nfn = NULL, s[1024] = "", *hand = NULL;
  struct userrec *u = NULL;

  fclose(dcc[idx].u.xfer->f);
  if (dcc[idx].u.xfer->length == dcc[idx].status) {
    int l;

    /* Success */
    ok = 0;
    if (!strcmp(dcc[idx].nick, "*users")) {
      module_entry *me = module_find("share", 0, 0);

      if (me && me->funcs) {
	Function f = me->funcs[SHARE_FINISH];

	(f) (idx);
      }
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
    } else if (!strcmp(dcc[idx].nick, "*binary")) {
      module_entry *me = module_find("update", 0, 0);

      if (me && me->funcs) {
	Function f = me->funcs[UPDATE_FINISH];

	(f) (idx);
      }
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
    }
    putlog(LOG_FILES, "*", TRANSFER_COMPLETED_DCC,
	   dcc[idx].u.xfer->origname, dcc[idx].nick, dcc[idx].host);
    egg_snprintf(s, sizeof s, "%s!%s", dcc[idx].nick, dcc[idx].host);
    u = get_user_by_host(s);
    hand = u ? u->handle : "*";

    l = strlen(dcc[idx].u.xfer->filename);
    if (l > NAME_MAX) {
      /* The filename is to long... blow it off */
      putlog(LOG_FILES, "*",TRANSFER_FILENAME_TOOLONG , l);
      dprintf(DP_HELP, TRANSFER_NOTICE_FNTOOLONG,
              dcc[idx].nick, l);
      putlog(LOG_FILES, "*", TRANSFER_TOO_BAD );
      dprintf(DP_HELP, TRANSFER_NOTICE_TOOBAD,
              dcc[idx].nick);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
    }
    /* Move the file from /tmp */
    ofn = calloc(1, strlen(tempdir) + strlen(dcc[idx].u.xfer->filename) + 1);
    nfn = calloc(1, strlen(dcc[idx].u.xfer->dir)
		  + strlen(dcc[idx].u.xfer->origname) + 1);
    sprintf(ofn, "%s%s", tempdir, dcc[idx].u.xfer->filename);
    sprintf(nfn, "%s%s", dcc[idx].u.xfer->dir, dcc[idx].u.xfer->origname);
    if (movefile(ofn, nfn))
      putlog(LOG_MISC | LOG_FILES, "*",
	     TRANSFER_FAILED_MOVE, nfn, ofn);
    else {
      /* Add to file database */
      module_entry *fs = module_find("filesys", 0, 0);

      if (fs != NULL) {
	Function f = fs->funcs[FILESYS_ADDFILE];

	f(dcc[idx].u.xfer->dir, dcc[idx].u.xfer->origname, hand);
      }
      stats_add_upload(u, dcc[idx].u.xfer->length);
      check_sentrcvd(u, dcc[idx].nick, nfn, BT_rcvd);
    }
    free(ofn);
    free(nfn);
    for (j = 0; j < dcc_total; j++)
      if (!ok && (dcc[j].type->flags & (DCT_GETNOTES)) &&
	  !egg_strcasecmp(dcc[j].nick, hand)) {
	ok = 1;
	dprintf(j,TRANSFER_THANKS);
      }
    if (!ok)
      dprintf(DP_HELP,TRANSFER_NOTICE_THANKS,
	      dcc[idx].nick);
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }
  /* Failure :( */
  if (!strcmp(dcc[idx].nick, "*users")) {
    int x, y = 0;

    for (x = 0; x < dcc_total; x++)
      if ((!egg_strcasecmp(dcc[x].nick, dcc[idx].host)) &&
	  (dcc[x].type->flags & DCT_BOT))
	y = x;
    if (y) {
      putlog(LOG_BOTS, "*",TRANSFER_USERFILE_LOST,
	     dcc[y].nick);
      unlink(dcc[idx].u.xfer->filename);
      /* Drop that bot */
      dprintf(y, "bye\n");
      egg_snprintf(s, sizeof s,TRANSFER_USERFILE_DISCON,
		   dcc[y].nick);
      botnet_send_unlinked(y, dcc[y].nick, s);
      chatout("*** %s\n", dcc[y].nick, s);

      if (y < idx) {
       int t = y;

       y = idx;
       idx = t;
      }

      if (y != idx) {
       killsock(dcc[y].sock);
       lostdcc(y);
      }
      killsock(dcc[idx].sock);
      lostdcc(idx);
    }
  } else if (!strcmp(dcc[idx].nick, "*binary")) {
    int x, y = 0;

    for (x = 0; x < dcc_total; x++)
      if ((!egg_strcasecmp(dcc[x].nick, dcc[idx].host)) &&
	  (dcc[x].type->flags & DCT_BOT))
	y = x;
    if (y) {
            
      putlog(LOG_BOTS, "*", "Lost binary transfer from %s; aborting.", dcc[y].nick);
      unlink(dcc[idx].u.xfer->filename);
/* Drop that bot 
      dprintf(y, "bye\n");
      egg_snprintf(s, sizeof s,"Disconnected %s (aborted binary transfer)", dcc[y].nick);
      botnet_send_unlinked(y, dcc[y].nick, s);
      chatout("*** %s\n", dcc[y].nick, s);
      if (y != idx) {
       killsock(dcc[y].sock);
       lostdcc(y);
      }
*/
      killsock(dcc[idx].sock);
      lostdcc(idx);
    }
  } else {
    putlog(LOG_FILES, "*",TRANSFER_LOST_DCCSEND,
	   dcc[idx].u.xfer->origname, dcc[idx].nick, dcc[idx].host,
	   dcc[idx].status, dcc[idx].u.xfer->length);
    ofn = calloc(1, strlen(tempdir) + strlen(dcc[idx].u.xfer->filename) + 1);
    sprintf(ofn, "%s%s", tempdir, dcc[idx].u.xfer->filename);
    unlink(ofn);
    free(ofn);
    killsock(dcc[idx].sock);
    lostdcc(idx);
  }
}

/* Determine byte order. Used for resend DCC startup packets.
 */
static inline u_8bit_t byte_order_test(void)
{
  u_16bit_t test = TRANSFER_REGET_PACKETID;

  if (*((u_8bit_t *)&test) == ((TRANSFER_REGET_PACKETID & 0xff00) >> 8))
    return 0;
  if (*((u_8bit_t *)&test) == (TRANSFER_REGET_PACKETID & 0x00ff))
    return 1;
  return 0;
}

/* Parse and handle resend DCC startup packets.
 */
inline static void handle_resend_packet(int idx, transfer_reget *reget_data)
{
  if (byte_order_test() != reget_data->byte_order) {
    /* The sender's byte order does not match our's so we need to switch the
     * bytes first, before we can make use of them.
     */
    reget_data->packet_id = ((reget_data->packet_id & 0x00ff) << 8) |
	    		    ((reget_data->packet_id & 0xff00) >> 8);
    reget_data->byte_offset = ((reget_data->byte_offset & 0xff000000) >> 24) |
	   		      ((reget_data->byte_offset & 0x00ff0000) >> 8)  |
			      ((reget_data->byte_offset & 0x0000ff00) << 8)  |
			      ((reget_data->byte_offset & 0x000000ff) << 24);
  }
  if (reget_data->packet_id != TRANSFER_REGET_PACKETID)
    putlog(LOG_FILES, "*", TRANSFER_REGET_PACKET,
	   dcc[idx].nick, dcc[idx].u.xfer->origname);
  else
    dcc[idx].u.xfer->offset = reget_data->byte_offset;
  dcc[idx].u.xfer->type = XFER_RESEND;
}

/* Handles DCC packets the client sends us. As soon as the last sent dcc
 * block is fully acknowledged we send the next block.
 *
 * Note: The first received packet during reget is a special 8 bit packet
 *       containing special information.
 */
void dcc_get(int idx, char *buf, int len)
{
  char xnick[NICKLEN] = "";
  unsigned char bbuf[4] = "";
  unsigned long cmp, l;
  int w = len + dcc[idx].u.xfer->sofar, p = 0;

  dcc[idx].timeval = now;		/* Mark as active		*/

  /* Add bytes to our buffer if we don't have a complete response yet.
   * This is either a 4 bit ack or the 8 bit reget packet.
   */
  if (w < 4 ||
      (w < 8 && dcc[idx].u.xfer->type == XFER_RESEND_PEND)) {
    my_memcpy(&(dcc[idx].u.xfer->buf[dcc[idx].u.xfer->sofar]), buf, len);
    dcc[idx].u.xfer->sofar += len;
    return;
  /* Waiting for the 8 bit reget packet? */
  } else if (dcc[idx].u.xfer->type == XFER_RESEND_PEND) {
    /* The 8 bit packet is complete now. Parse it. */
    if (w == 8) {
      transfer_reget reget_data;

      my_memcpy(&reget_data, dcc[idx].u.xfer->buf, dcc[idx].u.xfer->sofar);
      my_memcpy(&reget_data + dcc[idx].u.xfer->sofar, buf, len);
      handle_resend_packet(idx, &reget_data);
      cmp = dcc[idx].u.xfer->offset;
    } else
      return;
    /* Fall through! */
  /* No, only want 4 bit ack responses. */
  } else {
    /* Complete packet? */
    if (w == 4) {
      my_memcpy(bbuf, dcc[idx].u.xfer->buf, dcc[idx].u.xfer->sofar);
      my_memcpy(&(bbuf[dcc[idx].u.xfer->sofar]), buf, len);
    } else {
      p = ((w - 1) & ~3) - dcc[idx].u.xfer->sofar;
      w = w - ((w - 1) & ~3);
      if (w < 4) {
	my_memcpy(dcc[idx].u.xfer->buf, &(buf[p]), w);
	return;
      }
      my_memcpy(bbuf, &(buf[p]), w);
    }
    /* This is more compatible than ntohl for machines where an int
     * is more than 4 bytes:
     */
    cmp = ((unsigned int) (bbuf[0]) << 24) +
	  ((unsigned int) (bbuf[1]) << 16) +
	  ((unsigned int) (bbuf[2]) << 8) + bbuf[3];
    dcc[idx].u.xfer->acked = cmp;
  }

  dcc[idx].u.xfer->sofar = 0;
  if (cmp > dcc[idx].u.xfer->length && cmp > dcc[idx].status) {
    /* Attempt to resume, but file is not as long as requested... */
    putlog(LOG_FILES, "*",
	   TRANSFER_BEHIND_FILEEND,
	   dcc[idx].u.xfer->origname, dcc[idx].nick);
  } else if (cmp > dcc[idx].status) {
    /* Attempt to resume */
    if (!strcmp(dcc[idx].nick, "*users")) {
      putlog(LOG_BOTS, "*",TRANSFER_TRY_SKIP_AHEAD);
    } else if (!strcmp(dcc[idx].nick, "*binary")) {
      putlog(LOG_BOTS, "*","!!! Trying to skip ahead on binary transfer");
    } else {
      fseek(dcc[idx].u.xfer->f, cmp, SEEK_SET);
      dcc[idx].status = cmp;
      putlog(LOG_FILES, "*",TRANSFER_RESUME_FILE,
	     (int) (cmp / 1024), dcc[idx].u.xfer->origname,
	     dcc[idx].nick);
    }
  } else {
    if (dcc[idx].u.xfer->ack_type == XFER_ACK_UNKNOWN) {
      if (cmp < dcc[idx].u.xfer->offset)
	/* If we don't start at the top of the file, some clients only tell
	 * us the really received bytes (e.g. bitchx). This seems to be the
	 * case here.
	 */
	dcc[idx].u.xfer->ack_type = XFER_ACK_WITHOUT_OFFSET;
      else
	dcc[idx].u.xfer->ack_type = XFER_ACK_WITH_OFFSET;
    }
    if (dcc[idx].u.xfer->ack_type == XFER_ACK_WITHOUT_OFFSET)
      cmp += dcc[idx].u.xfer->offset;
  }

  if (cmp != dcc[idx].status)
    return;
  if (dcc[idx].status == dcc[idx].u.xfer->length) {
    /* Successful send, we are done */
    killsock(dcc[idx].sock);
    fclose(dcc[idx].u.xfer->f);
    if (!strcmp(dcc[idx].nick, "*users")) {
      module_entry *me = module_find("share", 0, 0);
      int x, y = 0;

      for (x = 0; x < dcc_total; x++)
	if (!egg_strcasecmp(dcc[x].nick, dcc[idx].host) &&
	    (dcc[x].type->flags & DCT_BOT))
	  y = x;
      if (y != 0)
	dcc[y].status &= ~STAT_SENDING;
      putlog(LOG_BOTS, "*",TRANSFER_COMPLETED_USERFILE,
	     dcc[y].nick);
      unlink(dcc[idx].u.xfer->filename);
      /* Any sharebot things that were queued: */
      if (me && me->funcs[SHARE_DUMP_RESYNC])
	((me->funcs)[SHARE_DUMP_RESYNC]) (y);
      xnick[0] = 0;
    } else if (!strcmp(dcc[idx].nick, "*binary")) {
      int x, y = 0;

      for (x = 0; x < dcc_total; x++)
	if (!egg_strcasecmp(dcc[x].nick, dcc[idx].host) &&
	    (dcc[x].type->flags & DCT_BOT))
	  y = x;
      if (y != 0) {
	dcc[y].status &= ~STAT_SENDINGU;
        dcc[y].status |= STAT_UPDATED;
      }
      putlog(LOG_BOTS, "*", "Completed binary file send to %s",
	     dcc[y].nick);
      xnick[0] = 0;
#ifdef HUB
      bupdating = 0;
#endif
    } else {
      module_entry *fs = module_find("filesys", 0, 0);
      struct userrec *u = get_user_by_handle(userlist,
					     dcc[idx].u.xfer->from);
      check_sentrcvd(u, dcc[idx].nick,
			 dcc[idx].u.xfer->dir, BT_sent);
      if (fs != NULL) {
	Function f = fs->funcs[FILESYS_INCRGOTS];

	f(dcc[idx].u.xfer->dir);
      }
      /* Download is credited to the user who requested it
       * (not the user who actually received it)
       */
      stats_add_dnload(u, dcc[idx].u.xfer->length);
      putlog(LOG_FILES, "*",TRANSFER_FINISHED_DCCSEND,
	     dcc[idx].u.xfer->origname, dcc[idx].nick);
      wipe_tmp_filename(dcc[idx].u.xfer->filename, idx);
      strcpy((char *) xnick, dcc[idx].nick);
    }
    lostdcc(idx);
    /* Any to dequeue? */
    if (!at_limit(xnick))
      send_next_file(xnick);
    return;
  }
  /* Note:  No fseek() needed here, because the file position is kept from
   *        the last run.
   */
  l = dcc_block;
  if (l == 0 || dcc[idx].status + l > dcc[idx].u.xfer->length)
    l = dcc[idx].u.xfer->length - dcc[idx].status;
  dcc[idx].u.xfer->block_pending = pump_file_to_sock(dcc[idx].u.xfer->f,
						     dcc[idx].sock, l);
  dcc[idx].status += l;
}

void eof_dcc_get(int idx)
{
  char xnick[NICKLEN] = "", s[1024] = "";

  fclose(dcc[idx].u.xfer->f);
  if (!strcmp(dcc[idx].nick, "*users")) {
    int x, y = 0;

    for (x = 0; x < dcc_total; x++)
      if (!egg_strcasecmp(dcc[x].nick, dcc[idx].host) &&
	  (dcc[x].type->flags & DCT_BOT))
	y = x;
    putlog(LOG_BOTS, "*", TRANSFER_ABORT_USERFILE);
    /* Note: no need to unlink the xfer file, as it's already unlinked. */
    xnick[0] = 0;
    /* Drop that bot */
    dprintf(-dcc[y].sock, "bye\n");
    egg_snprintf(s, sizeof s, TRANSFER_USERFILE_DISCON,
		 dcc[y].nick);
    botnet_send_unlinked(y, dcc[y].nick, s);
    chatout("*** %s\n", s);
    if (y != idx) {
     killsock(dcc[y].sock);
     lostdcc(y);
    }
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  } else if (!strcmp(dcc[idx].nick, "*binary")) {
    int x, y = 0;

    for (x = 0; x < dcc_total; x++)
      if (!egg_strcasecmp(dcc[x].nick, dcc[idx].host) &&
	  (dcc[x].type->flags & DCT_BOT))
	y = x;
    putlog(LOG_BOTS, "*", "Lost binary transfer; aborting.");
    /* Note: no need to unlink the xfer file, as it's already unlinked. */
    xnick[0] = 0;
    /* Drop that bot */
    dcc[y].status &= ~STAT_SENDINGU;
#ifdef HUB
    bupdating = 0;
#endif
/*
    dprintf(-dcc[y].sock, "bye\n");
    egg_snprintf(s, sizeof s, "Disconnected %s (aborted binary transfer)",
		 dcc[y].nick);
    botnet_send_unlinked(y, dcc[y].nick, s);
    chatout("*** %s\n", s);
    if (y != idx) {
     killsock(dcc[y].sock);
     lostdcc(y);
    }
*/
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  } else {
    struct userrec *u = NULL;

    /* Call `lost' DCC trigger now.
     */
    egg_snprintf(s, sizeof s, "%s!%s", dcc[idx].nick, dcc[idx].host);
    u = get_user_by_host(s);
    check_toutlost(u, dcc[idx].nick, dcc[idx].u.xfer->dir,
		       dcc[idx].u.xfer->acked, dcc[idx].u.xfer->length,
		       BT_lost);

    putlog(LOG_FILES, "*",TRANSFER_LOST_DCCGET,
	   dcc[idx].u.xfer->origname, dcc[idx].nick, dcc[idx].host);
    wipe_tmp_filename(dcc[idx].u.xfer->filename, idx);
    strcpy(xnick, dcc[idx].nick);
  }
  killsock(dcc[idx].sock);
  lostdcc(idx);
  /* Send next queued file if there is one */
  if (!at_limit(xnick))
    send_next_file(xnick);
}

void dcc_send(int idx, char *buf, int len)
{
  char s[512] = "", *b = NULL;
  unsigned long sent;

  fwrite(buf, len, 1, dcc[idx].u.xfer->f);
  dcc[idx].status += len;
  /* Put in network byte order */
  sent = dcc[idx].status;
  s[0] = (sent / (1 << 24));
  s[1] = (sent % (1 << 24)) / (1 << 16);
  s[2] = (sent % (1 << 16)) / (1 << 8);
  s[3] = (sent % (1 << 8));
  tputs(dcc[idx].sock, s, 4);
  dcc[idx].timeval = now;
  if (dcc[idx].status > dcc[idx].u.xfer->length &&
      dcc[idx].u.xfer->length > 0) {
    dprintf(DP_HELP,TRANSFER_BOGUS_FILE_LENGTH, dcc[idx].nick);
    putlog(LOG_FILES, "*",
	   TRANSFER_FILE_TOO_LONG,
	   dcc[idx].u.xfer->origname, dcc[idx].nick, dcc[idx].host);
    fclose(dcc[idx].u.xfer->f);
    b = calloc(1, strlen(tempdir) + strlen(dcc[idx].u.xfer->filename) + 1);
    sprintf(b, "%s%s", tempdir, dcc[idx].u.xfer->filename);
    unlink(b);
    free(b);
    killsock(dcc[idx].sock);
    lostdcc(idx);
  }
}

static void transfer_get_timeout(int i)
{
  char xx[1024] = "";

  fclose(dcc[i].u.xfer->f);
  if (strcmp(dcc[i].nick, "*users") == 0) {
    int x, y = 0;

    for (x = 0; x < dcc_total; x++)
      if ((!egg_strcasecmp(dcc[x].nick, dcc[i].host)) &&
	  (dcc[x].type->flags & DCT_BOT))
	y = x;
    if (y != 0) {
      dcc[y].status &= ~STAT_SENDING;
      dcc[y].status &= ~STAT_SHARE;
    }
    unlink(dcc[i].u.xfer->filename);
    putlog(LOG_BOTS, "*",TRANSFER_USERFILE_TIMEOUT);
    dprintf(y, "bye\n");
    egg_snprintf(xx, sizeof xx,TRANSFER_DICONNECT_TIMEOUT,
		 dcc[y].nick);
    botnet_send_unlinked(y, dcc[y].nick, xx);
    chatout("*** %s\n", xx);
    if (y < i) {
      int t = y;

      y = i;
      i = t;
    }
    killsock(dcc[y].sock);
    lostdcc(y);
    xx[0] = 0;
  } else if (strcmp(dcc[i].nick, "*binary") == 0) {
    int x, y = 0;

    for (x = 0; x < dcc_total; x++)
      if ((!egg_strcasecmp(dcc[x].nick, dcc[i].host)) &&
	  (dcc[x].type->flags & DCT_BOT))
	y = x;
    if (y != 0) {
      dcc[y].status &= ~STAT_SENDINGU;
    }
    putlog(LOG_BOTS, "*","Timeout on binary transfer.");
    dprintf(y, "bye\n");
    egg_snprintf(xx, sizeof xx,"Disconnected %s (timed-out binary transfer)",
		 dcc[y].nick);
    botnet_send_unlinked(y, dcc[y].nick, xx);
    chatout("*** %s\n", xx);
    if (y < i) {
      int t = y;

      y = i;
      i = t;
    }
    killsock(dcc[y].sock);
    lostdcc(y);
    xx[0] = 0;
  } else {
    char *p = NULL;
    struct userrec *u = NULL;

    p = strrchr(dcc[i].u.xfer->origname, '/');
    dprintf(DP_HELP, TRANSFER_NOTICE_TIMEOUT,
	    dcc[i].nick, p ? p + 1 : dcc[i].u.xfer->origname);

    /* Call DCC `timeout' trigger now.
     */
    egg_snprintf(xx, sizeof xx, "%s!%s", dcc[i].nick, dcc[i].host);
    u = get_user_by_host(xx);
    check_toutlost(u, dcc[i].nick, dcc[i].u.xfer->dir,
		       dcc[i].u.xfer->acked, dcc[i].u.xfer->length, BT_tout);

    putlog(LOG_FILES, "*",TRANSFER_DCC_GET_TIMEOUT,
	   p ? p + 1 : dcc[i].u.xfer->origname, dcc[i].nick, dcc[i].status,
	   dcc[i].u.xfer->length);
    wipe_tmp_filename(dcc[i].u.xfer->filename, i);
    strcpy(xx, dcc[i].nick);
  }
  killsock(dcc[i].sock);
  lostdcc(i);
  if (!at_limit(xx))
    send_next_file(xx);
}

void tout_dcc_send(int idx)
{
  if (!strcmp(dcc[idx].nick, "*users")) {
    int x, y = 0;

    for (x = 0; x < dcc_total; x++)
      if (!egg_strcasecmp(dcc[x].nick, dcc[idx].host) &&
	  (dcc[x].type->flags & DCT_BOT))
	y = x;
    if (y != 0) {
      dcc[y].status &= ~STAT_GETTING;
      dcc[y].status &= ~STAT_SHARE;
    }
    unlink(dcc[idx].u.xfer->filename);
    putlog(LOG_BOTS, "*", TRANSFER_USERFILE_TIMEOUT);
  } else if (!strcmp(dcc[idx].nick, "*binary")) {
    int x, y = 0;

    for (x = 0; x < dcc_total; x++)
      if (!egg_strcasecmp(dcc[x].nick, dcc[idx].host) &&
	  (dcc[x].type->flags & DCT_BOT))
	y = x;
    if (y != 0) {
      dcc[y].status &= ~STAT_GETTINGU;
    }
    putlog(LOG_BOTS, "*", "Timeout on binary transfer.");

  } else {
    char *buf = NULL;

    dprintf(DP_HELP,TRANSFER_NOTICE_TIMEOUT,
	    dcc[idx].nick, dcc[idx].u.xfer->origname);
    putlog(LOG_FILES, "*",TRANSFER_DCC_SEND_TIMEOUT,
	   dcc[idx].u.xfer->origname, dcc[idx].nick, dcc[idx].status,
	   dcc[idx].u.xfer->length);
    buf = calloc(1, strlen(tempdir) + strlen(dcc[idx].u.xfer->filename) + 1);
    sprintf(buf, "%s%s", tempdir, dcc[idx].u.xfer->filename);
    unlink(buf);
    free(buf);
  }
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

void display_dcc_get(int idx, char *buf)
{
  if (dcc[idx].status == dcc[idx].u.xfer->length)
    sprintf(buf,TRANSFER_SEND, dcc[idx].u.xfer->acked,
	    dcc[idx].u.xfer->length, dcc[idx].u.xfer->origname);
  else
    sprintf(buf,TRANSFER_SEND, dcc[idx].status,
	    dcc[idx].u.xfer->length, dcc[idx].u.xfer->origname);
}

void display_dcc_get_p(int idx, char *buf)
{
  sprintf(buf,TRANSFER_SEND_WAITED, now - dcc[idx].timeval,
	  dcc[idx].u.xfer->origname);
}

void display_dcc_send(int idx, char *buf)
{
  sprintf(buf,TRANSFER_SEND, dcc[idx].status,
	  dcc[idx].u.xfer->length, dcc[idx].u.xfer->origname);
}

void display_dcc_fork_send(int idx, char *buf)
{
  sprintf(buf,TRANSFER_CONN_SEND);
}

void kill_dcc_xfer(int idx, void *x)
{
  register struct xfer_info *p = (struct xfer_info *) x;

  if (p->filename)
    free(p->filename);
  /* We need to check if origname points to filename before
   * attempting to free the memory.
   */
  if (p->origname && p->origname != p->filename)
    free(p->origname);
  free(x);
}

void out_dcc_xfer(int idx, char *buf, void *x)
{
}

static void outdone_dcc_xfer(int idx)
{
  if (dcc[idx].u.xfer->block_pending)
    dcc[idx].u.xfer->block_pending =
	    pump_file_to_sock(dcc[idx].u.xfer->f, dcc[idx].sock,
			      dcc[idx].u.xfer->block_pending);
}

struct dcc_table DCC_SEND =
{
  "SEND",
  DCT_FILETRAN | DCT_FILESEND | DCT_VALIDIDX,
  eof_dcc_send,
  dcc_send,
  &wait_dcc_xfer,
  tout_dcc_send,
  display_dcc_send,
  kill_dcc_xfer,
  out_dcc_xfer
};

void dcc_fork_send(int idx, char *x, int y);

struct dcc_table DCC_FORK_SEND =
{
  "FORK_SEND",
  DCT_FILETRAN | DCT_FORKTYPE | DCT_FILESEND | DCT_VALIDIDX,
  eof_dcc_fork_send,
  dcc_fork_send,
  &wait_dcc_xfer,
  eof_dcc_fork_send,
  display_dcc_fork_send,
  kill_dcc_xfer,
  out_dcc_xfer
};

void dcc_fork_send(int idx, char *x, int y)
{
  char s1[121] = "";

  if (dcc[idx].type != &DCC_FORK_SEND)
    return;
  dcc[idx].type = &DCC_SEND;
  dcc[idx].u.xfer->start_time = now;
  egg_snprintf(s1, sizeof s1, "%s!%s", dcc[idx].nick, dcc[idx].host);
  if (strcmp(dcc[idx].nick, "*users") && strcmp(dcc[idx].nick, "*binary"))
    putlog(LOG_MISC, "*", TRANSFER_DCC_CONN, dcc[idx].u.xfer->origname, s1);
}

struct dcc_table DCC_GET =
{
  "GET",
  DCT_FILETRAN | DCT_VALIDIDX,
  eof_dcc_get,
  dcc_get,
  &wait_dcc_xfer,
  transfer_get_timeout,
  display_dcc_get,
  kill_dcc_xfer,
  out_dcc_xfer,
  outdone_dcc_xfer
};

struct dcc_table DCC_GET_PENDING =
{
  "GET_PENDING",
  DCT_FILETRAN | DCT_VALIDIDX,
  eof_dcc_get,
  dcc_get_pending,
  &wait_dcc_xfer,
  transfer_get_timeout,
  display_dcc_get_p,
  kill_dcc_xfer,
  out_dcc_xfer
};

static void dcc_get_pending(int idx, char *buf, int len)
{
  unsigned long ip;
  unsigned short port;
  int i;
  char s[UHOSTLEN] = "";

  i = answer(dcc[idx].sock, s, &ip, &port, 1);
  killsock(dcc[idx].sock);
  dcc[idx].sock = i;
  dcc[idx].addr = ip;
  dcc[idx].port = (int) port;
  if (dcc[idx].sock == -1) {
    neterror(s);
    dprintf(DP_HELP, TRANSFER_NOTICE_BAD_CONN, dcc[idx].nick, s);
    putlog(LOG_FILES, "*", TRANSFER_LOG_BAD_CONN,
	   dcc[idx].u.xfer->origname, dcc[idx].nick, dcc[idx].host);
    fclose(dcc[idx].u.xfer->f);
    lostdcc(idx);
    return;
  }
  dcc[idx].type = &DCC_GET;
  dcc[idx].u.xfer->ack_type = XFER_ACK_UNKNOWN;

  /*
   * Note: The file was already opened and dcc[idx].u.xfer->f may be
   *       used immediately. Leave it opened until the file transfer
   *       is complete.
   */

  /* Are we resuming? */
  if (dcc[idx].u.xfer->type == XFER_RESUME_PEND) {
    int l;

    if (dcc_block == 0 || dcc[idx].u.xfer->length < dcc_block) {
      l = dcc[idx].u.xfer->length - dcc[idx].u.xfer->offset;
      dcc[idx].status = dcc[idx].u.xfer->length;
    } else {
      l = dcc_block;
      dcc[idx].status = dcc[idx].u.xfer->offset + dcc_block;
    }

    /* Seek forward ... */
    fseek(dcc[idx].u.xfer->f, dcc[idx].u.xfer->offset, SEEK_SET);
    dcc[idx].u.xfer->block_pending = pump_file_to_sock(dcc[idx].u.xfer->f,
						       dcc[idx].sock, l);
    dcc[idx].u.xfer->type = XFER_RESUME;
  } else {
    dcc[idx].u.xfer->offset = 0;

    /* If we're resending the data, wait for the client's response first,
     * before sending anything ourself.
     */
    if (dcc[idx].u.xfer->type != XFER_RESEND_PEND) {
      if (dcc_block == 0 || dcc[idx].u.xfer->length < dcc_block)
        dcc[idx].status = dcc[idx].u.xfer->length;
      else
        dcc[idx].status = dcc_block;
      dcc[idx].u.xfer->block_pending = pump_file_to_sock(dcc[idx].u.xfer->f,
						         dcc[idx].sock,
							 dcc[idx].status);
    } else
      dcc[idx].status = 0;
  }

  dcc[idx].timeval = dcc[idx].u.xfer->start_time = now;
}

/* Starts a new DCC SEND or DCC RESEND connection to `nick', transferring
 * `filename' from `dir'.
 *
 * Use raw_dcc_resend() and raw_dcc_send() instead of this function.
 */

static int raw_dcc_resend_send(char *filename, char *nick, char *from, char *dir, int resend)
{
  int zz, port, i;
  char *nfn = NULL, *buf = NULL;
  long dccfilesize;
  FILE *f = NULL, *dccfile = NULL;
 
  sdprintf("raw_dcc_resend_send()");
  zz = (-1);
  dccfile = fopen(filename,"r");
  fseek(dccfile, 0, SEEK_END);
  dccfilesize = ftell(dccfile);
  fclose(dccfile);
  /* File empty?! */
  if (dccfilesize == 0)
    return DCCSEND_FEMPTY;

  if (conf.portmin > 0 && conf.portmin < conf.portmax) {
    for (port = conf.portmin; port <= conf.portmax; port++)
#ifdef USE_IPV6
      if ((zz = open_listen_by_af(&port, AF_INET)) != -1) /* no idea how we want to handle this -poptix 02/03/03 */
#else
      if ((zz = open_listen(&port)) != -1)
#endif /* USE_IPV6 */
        break;
  } else {
    port = conf.portmin;
#ifdef USE_IPV6
    zz = open_listen_by_af(&port, AF_INET);
#else
    zz = open_listen(&port);
#endif /* USE_IPV6 */
  }

  if (zz == (-1))
    return DCCSEND_NOSOCK;
  nfn = strrchr(dir, '/');
  if (nfn == NULL)
    nfn = dir;
  else
    nfn++;
  f = fopen(filename, "r");
  if (!f)
    return DCCSEND_BADFN;
  if ((i = new_dcc(&DCC_GET_PENDING, sizeof(struct xfer_info))) == -1)
     return DCCSEND_FULL;
  dcc[i].sock = zz;
  dcc[i].addr = (IP) (-559026163);
  dcc[i].port = port;
  strcpy(dcc[i].nick, nick);
  strcpy(dcc[i].host, "irc");
  dcc[i].u.xfer->filename = calloc(1, strlen(filename) + 1);
  strcpy(dcc[i].u.xfer->filename, filename);
  if (strchr(nfn, ' '))
    nfn = buf = replace_spaces(nfn);
  dcc[i].u.xfer->origname = calloc(1, strlen(nfn) + 1);
  strcpy(dcc[i].u.xfer->origname, nfn);
  strncpyz(dcc[i].u.xfer->from, from, NICKLEN);
  strncpyz(dcc[i].u.xfer->dir, dir, DIRLEN);
  dcc[i].u.xfer->length = dccfilesize;
  dcc[i].timeval = now;
  dcc[i].u.xfer->f = f;
  dcc[i].u.xfer->type = resend ? XFER_RESEND_PEND : XFER_SEND;
  if (nick[0] != '*') {
    dprintf(DP_HELP, "PRIVMSG %s :\001DCC %sSEND %s %lu %d %lu\001\n", nick,
	    resend ? "RE" :  "", nfn,
	    iptolong(natip[0] ? (IP) inet_addr(natip) : getmyip()), port,
	    dccfilesize);
    putlog(LOG_FILES, "*",TRANSFER_BEGIN_DCC, resend ? TRANSFER_RE :  "",
	   nfn, nick);
  }
  if (buf)
    free(buf);
  return DCCSEND_OK;
}

/* Starts a DCC RESEND connection.
 */
static int raw_dcc_resend(char *filename, char *nick, char *from, char *dir)
{
  return raw_dcc_resend_send(filename, nick, from, dir, 1);
}

/* Starts a DCC_SEND connection.
 */
int raw_dcc_send(char *filename, char *nick, char *from, char *dir)
{
  return raw_dcc_resend_send(filename, nick, from, dir, 0);
}

/*
 *    fstat functions
 */

static int fstat_unpack(struct userrec *u, struct user_entry *e)
{
  char *par = NULL, *arg = NULL;
  struct filesys_stats *fs = NULL;

  fs = calloc(1, sizeof(struct filesys_stats));
  par = e->u.list->extra;
  arg = newsplit(&par);
  if (arg[0])
    fs->uploads = atoi(arg);
  arg = newsplit(&par);
  if (arg[0])
    fs->upload_ks = atoi(arg);
  arg = newsplit(&par);
  if (arg[0])
    fs->dnloads = atoi(arg);
  arg = newsplit(&par);
  if (arg[0])
    fs->dnload_ks = atoi(arg);

  list_type_kill(e->u.list);
  e->u.extra = fs;
  return 1;
}

static int fstat_pack(struct userrec *u, struct user_entry *e)
{
  register struct filesys_stats *fs = NULL;
  struct list_type *l = calloc(1, sizeof(struct list_type));

  fs = e->u.extra;
  l->extra = calloc(1, 41);
  egg_snprintf(l->extra, 41, "%09u %09u %09u %09u", fs->uploads, fs->upload_ks, fs->dnloads, fs->dnload_ks);
  l->next = NULL;
  e->u.list = l;
  free(fs);
  return 1;
}

static int fstat_write_userfile(FILE *f, struct userrec *u,
				struct user_entry *e)
{
  register struct filesys_stats *fs = NULL;

  fs = e->u.extra;
  if (lfprintf(f, "--FSTAT %09u %09u %09u %09u\n",
	      fs->uploads, fs->upload_ks,
	      fs->dnloads, fs->dnload_ks) == EOF)
    return 0;
  return 1;
}

static int fstat_set(struct userrec *u, struct user_entry *e, void *buf)
{
  register struct filesys_stats *fs = buf;

  if (e->u.extra != fs) {
    if (e->u.extra)
      free(e->u.extra);
    e->u.extra = fs;
  } else if (!fs) /* e->u.extra == NULL && fs == NULL */
    return 1;

  if (!noshare && !(u->flags & (USER_BOT | USER_UNSHARED))) {
    if (fs)
      /* Don't check here for something like
       *  ofs->uploads != fs->uploads || ofs->upload_ks != fs->upload_ks ||
       *  ofs->dnloads != fs->dnloads || ofs->dnload_ks != fs->dnload_ks
       * someone could do:
       *  e->u.extra->uploads = 12345;
       *  fs = calloc(1, sizeof(struct filesys_stats));
       *  memcpy (...e->u.extra...fs...);
       *  set_user(&USERENTRY_FSTAT, u, fs);
       * then we wouldn't detect here that something's changed...
       * --rtc
       */
      shareout (NULL, "ch fstat %09u %09u %09u %09u\n",
	        fs->uploads, fs->upload_ks, fs->dnloads, fs->dnload_ks);
    else
      shareout (NULL, "ch fstat r\n");
  }
  return 1;
}

static int fstat_kill(struct user_entry *e)
{
  if (e->u.extra)
    free(e->u.extra);
  free(e);
  return 1;
}

static void fstat_display(int idx, struct user_entry *e, struct userrec *u)
{
  struct filesys_stats *fs = NULL;

  fs = e->u.extra;
  dprintf(idx, "  FILES: %u download%s (%luk), %u upload%s (%luk)\n",
	  fs->dnloads, (fs->dnloads == 1) ? "" : "s", fs->dnload_ks,
	  fs->uploads, (fs->uploads == 1) ? "" : "s", fs->upload_ks);
}

static int fstat_gotshare(struct userrec *u, struct user_entry *e,
			  char *par, int idx);
static int fstat_dupuser(struct userrec *u, struct userrec *o,
			 struct user_entry *e);
static void stats_add_dnload(struct userrec *u, unsigned long bytes);
static void stats_add_upload(struct userrec *u, unsigned long bytes);

static struct user_entry_type USERENTRY_FSTAT =
{
  NULL,
  fstat_gotshare,
  fstat_dupuser,
  fstat_unpack,
  fstat_pack,
  fstat_write_userfile,
  fstat_kill,
  NULL,
  fstat_set,
  fstat_display,
  "FSTAT"
};

static int fstat_gotshare(struct userrec *u, struct user_entry *e,
			  char *par, int idx)
{
  char *p = NULL;
  struct filesys_stats *fs = NULL;

  noshare = 1;
  switch (par[0]) {
  case 'u':
  case 'd':
    /* No stats_add_up/dnload here, it's already been sent... --rtc */
    break;
  case 'r':
    set_user (&USERENTRY_FSTAT, u, NULL);
    break;
  default:
    if (!(fs = e->u.extra)) {
      fs = calloc(1, sizeof(struct filesys_stats));
    }
    p = newsplit (&par);
    if (p[0])
      fs->uploads = atoi (p);
    p = newsplit (&par);
    if (p[0])
      fs->upload_ks = atoi (p);
    p = newsplit (&par);
    if (p[0])
      fs->dnloads = atoi (p);
    p = newsplit (&par);
    if (p[0])
      fs->dnload_ks = atoi (p);
    set_user(&USERENTRY_FSTAT, u, fs);
    break;
  }
  noshare = 0;
  return 1;
}

static int fstat_dupuser(struct userrec *u, struct userrec *o,
			 struct user_entry *e)
{
  struct filesys_stats *fs = NULL;

  if (e->u.extra) {
    fs = calloc(1, sizeof(struct filesys_stats));
    my_memcpy(fs, e->u.extra, sizeof(struct filesys_stats));

    return set_user(&USERENTRY_FSTAT, u, fs);
  }
  return 0;
}

static void stats_add_dnload(struct userrec *u, unsigned long bytes)
{
  struct user_entry *ue = NULL;
  register struct filesys_stats *fs = NULL;

  if (u) {
    if (!(ue = find_user_entry (&USERENTRY_FSTAT, u)) ||
        !(fs = ue->u.extra)) {
      fs = calloc(1, sizeof(struct filesys_stats));
    }
    fs->dnloads++;
    fs->dnload_ks += ((bytes + 512) / 1024);
    set_user(&USERENTRY_FSTAT, u, fs);
    /* No shareout here, set_user already sends info... --rtc */
  }
}

static void stats_add_upload(struct userrec *u, unsigned long bytes)
{
  struct user_entry *ue = NULL;
  register struct filesys_stats *fs = NULL;

  if (u) {
    if (!(ue = find_user_entry (&USERENTRY_FSTAT, u)) ||
        !(fs = ue->u.extra)) {
      fs = calloc(1, sizeof(struct filesys_stats));
    }
    fs->uploads++;
    fs->upload_ks += ((bytes + 512) / 1024);
    set_user(&USERENTRY_FSTAT, u, fs);
    /* No shareout here, set_user already sends info... --rtc */
  }
}


/*
 *    CTCP functions
 */

/* This handles DCC RESUME requests.
 */
static int ctcp_DCC_RESUME(char *nick, char *from, char *handle,
			   char *object, char *keyword, char *text)
{
  char *action = NULL, *fn = NULL, buf[512] = "", *msg = buf;
  int i, port;
  unsigned long offset;

  strcpy(msg, text);
  action = newsplit(&msg);
  if (egg_strcasecmp(action, "RESUME"))
    return 0;
  fn = newsplit(&msg);
  port = atoi(newsplit(&msg));
  offset = my_atoul(newsplit(&msg));
  /* Search for existing SEND */
  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type == &DCC_GET_PENDING) &&
	(!rfc_casecmp(dcc[i].nick, nick)) && (dcc[i].port == port))
      break;
  /* No matching transfer found? */
  if (i == dcc_total)
    return 0;

  if (dcc[i].u.xfer->length <= offset) {
    char *p = strrchr(dcc[i].u.xfer->origname, '/');

    dprintf(DP_HELP,TRANSFER_DCC_IGNORED,
	    nick, p ? p + 1 : dcc[i].u.xfer->origname);
    return 0;
  }
  dcc[i].u.xfer->type = XFER_RESUME_PEND;
  dcc[i].u.xfer->offset = offset;
  dprintf(DP_HELP, "PRIVMSG %s :\001DCC ACCEPT %s %d %u\001\n", nick,
	  fn, port, offset);
  /* Now we wait for the client to connect. */
  return 1;
}

static cmd_t transfer_ctcps[] =
{
  {"DCC",	"",	ctcp_DCC_RESUME,	"transfer:DCC"},
  {NULL,	NULL,	NULL,			NULL}
};

/* Add our CTCP bindings if the server module is loaded. */
static int server_transfer_setup(char *mod)
{
  add_builtins("ctcp", transfer_ctcps);
  return 1;
}

static cmd_t transfer_load[] =
{
  {"server",	"",	server_transfer_setup,	NULL},
  {NULL,	"",	NULL,			NULL}
};

/*
 *   Module functions
 */

static void transfer_report(int idx, int details)
{
  if (details) {
    dprintf(idx,TRANSFER_STAT_BLOCK,
	    dcc_block, (dcc_block == 0) ? " (turbo dcc)" : "", dcc_limit);
  }
}

EXPORT_SCOPE char *transfer_start();

static Function transfer_table[] =
{
  (Function) transfer_start,
  (Function) NULL,
  (Function) 0,
  (Function) transfer_report,
  /* 4- 7 */
  (Function) & DCC_FORK_SEND,		/* struct dcc_table		*/
  (Function) at_limit,
  (Function) & copy_to_tmp,		/* int				*/
  (Function) fileq_cancel,
  /* 8 - 11 */
  (Function) queue_file,
  (Function) raw_dcc_send,
  (Function) show_queued_files,
  (Function) wild_match_file,
  /* 12 - 15 */
  (Function) wipe_tmp_filename,
  (Function) & DCC_GET,			/* struct dcc_table		*/
  (Function) 0,
  (Function) 0,
  /* 16 - 19 */
  (Function) & USERENTRY_FSTAT,		/* struct user_entry_type	*/
  (Function) & quiet_reject,		/* int				*/
  (Function) raw_dcc_resend,
  (Function) 0,
  /* 20 - 23 */
  (Function) 0,
};

char *transfer_start(Function *global_funcs)
{
  global = global_funcs;

  fileq = NULL;
  module_register(MODULE_NAME, transfer_table, 2, 2);

  add_builtins("load", transfer_load);

  server_transfer_setup(NULL);
  BT_rcvd = bind_table_add("rcvd", 3, "Uss", MATCH_MASK, BIND_STACKABLE);
  BT_sent = bind_table_add("sent", 3, "Uss", MATCH_MASK, BIND_STACKABLE);
  BT_lost = bind_table_add("lost", 5, "Ussii", MATCH_MASK, BIND_STACKABLE);
  BT_tout = bind_table_add("tout", 5, "Ussii", MATCH_MASK, BIND_STACKABLE);

  USERENTRY_FSTAT.get = def_get;
  add_entry_type(&USERENTRY_FSTAT);
  return NULL;
}
