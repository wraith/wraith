
/* 
 * transfer.c -- part of transfer.mod
 * 
 * $Id: transfer.c,v 1.25 2000/01/14 12:15:30 per Exp $
 */

/* 
 * Copyright (C) 1997  Robey Pointer
 * Copyright (C) 1999, 2000  Eggheads
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

#define MODULE_NAME "transfer"
#define MAKING_TRANSFER
#define MOD_FILESYS

/* sigh sunos */
#include <sys/types.h>
#include <sys/stat.h>
#include "main.h"
#include "eggdrop.h"
#include "hook.h"
#include "channels.h"
#include "tandem.h"
#include "users.h"
#include "transfer.h"

#include <netinet/in.h>
#include <arpa/inet.h>

int copy_to_tmp = 1;		/* copy files to /tmp before transmitting? */
int wait_dcc_xfer = 300;	/* timeout time on DCC xfers */
p_tcl_bind_list H_rcvd,
  H_sent;
int dcc_limit = 3;		/* maximum number of simultaneous file

				   * downloads allowed */
int dcc_block = 1024;
void wipe_tmp_filename(char *fn, int idx);
int at_limit(char *nick);
void dcc_get_pending(int idx, char *buf, int len);

typedef struct zarrf {
  char *dir;			/* starts with '*' -> absolute dir */
  char *file;			/* (otherwise -> dccdir) */
  char nick[NICKLEN];		/* who queued this file */
  char to[NICKLEN];		/* who will it be sent to */
  struct zarrf *next;
} fileq_t;

fileq_t *fileq = NULL;

extern struct userrec *userlist;
extern char tempdir[],
  natip[];
extern struct dcc_t *dcc;
extern time_t now;
extern int dcc_total,
  min_dcc_port,
  max_dcc_port;
extern int reserved_port;
extern p_tcl_bind_list H_ctcp;

#undef MATCH
#define MATCH (match+sofar)
int dcc_maxsize = 2048;
char dccin[3] = "./";

#define DCCSEND_OK     0
#define DCCSEND_FULL   1	/* dcc table is full */
#define DCCSEND_NOSOCK 2	/* can't open a listening socket */
#define DCCSEND_BADFN  3	/* no such file */

/* this function SHAMELESSLY :) pinched from match.c in the original
 * source, see that file for info about the author etc */

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
int wild_match_file(register char *m, register char *n)
{
  char *ma = m,
   *lsm = 0,
   *lsn = 0;
  int match = 1;
  register unsigned int sofar = 0;

  /* take care of null strings (should never match) */
  if ((m == 0) || (n == 0) || (!*n))
    return NOMATCH;
  /* (!*m) test used to be here, too, but I got rid of it.  After all, If
   * (!*n) was false, there must be a character in the name (the second
   * string), so if the mask is empty it is a non-match.  Since the
   * algorithm handles this correctly without testing for it here and this
   * shouldn't be called with null masks anyway, it should be a bit faster
   * this way */
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

/* Replaces all spaces with underscores (' ' -> '_').  The returned buffer
 * needs to be freed after use.
 */
char *replace_spaces(char *fn)
{
  register char *ret,
   *p;

  p = ret = nmalloc(strlen(fn) + 1);
  strcpy(ret, fn);
  while ((p = strchr(p, ' ')) != NULL)
    *p = '_';
  return ret;
}

#ifdef G_USETCL

int builtin_sentrcvd STDVAR { Function F = (Function) cd;

    BADARGS(4, 4, STR(" hand nick path"));

  if (!check_validity(argv[0], builtin_sentrcvd)) {
    Tcl_AppendResult(irp, STR("bad builtin command call!"), NULL);
    return TCL_ERROR;
  }
  F (argv[1], argv[2], argv[3]);

  return TCL_OK;
}
#endif

int expmem_fileq()
{
  fileq_t *q = fileq;
  int tot = 0;

  Context;
  while (q != NULL) {
    tot += strlen(q->dir) + strlen(q->file) + 2 + sizeof(fileq_t);
    q = q->next;
  }
  return tot;
}

/*
void queue_file(char *dir, char *file, char *from, char *to)
{
  fileq_t *q = fileq;

  fileq = (fileq_t *) nmalloc(sizeof(fileq_t));
  fileq->next = q;
  fileq->dir = (char *) nmalloc(strlen(dir) + 1);
  fileq->file = (char *) nmalloc(strlen(file) + 1);
  strcpy(fileq->dir, dir);
  strcpy(fileq->file, file);
  strcpy(fileq->nick, from);
  strcpy(fileq->to, to);
}
*/
void deq_this(fileq_t * this)
{
  fileq_t *q = fileq,
   *last = NULL;

  while (q && (q != this)) {
    last = q;
    q = q->next;
  }
  if (!q)
    return;			/* bogus ptr */
  if (last)
    last->next = q->next;
  else
    fileq = q->next;
  nfree(q->dir);
  nfree(q->file);
  nfree(q);
}

/* remove all files queued to a certain user */
void flush_fileq(char *to)
{
  fileq_t *q = fileq;
  int fnd = 1;

  while (fnd) {
    q = fileq;
    fnd = 0;
    while (q != NULL) {
      if (!strcasecmp(q->to, to)) {
	deq_this(q);
	q = NULL;
	fnd = 1;
      }
      if (q != NULL)
	q = q->next;
    }
  }
}

void send_next_file(char *to)
{
  fileq_t *q = fileq,
   *this = NULL;
  char s[256],
    s1[256];
  int x;

  while (q != NULL) {
    if (!strcasecmp(q->to, to))
      this = q;
    q = q->next;
  }
  if (this == NULL)
    return;			/* none */
  /* copy this file to /tmp */
  if (this->dir[0] == '*')	/* absolute path */
    sprintf(s, STR("%s/%s"), &this->dir[1], this->file);
  else {
    char *p = strchr(this->dir, '*');

    if (p == NULL) {		/* if it's messed up */
      send_next_file(to);
      return;
    }
    p++;
    sprintf(s, STR("%s%s%s"), p, p[0] ? "/" : "", this->file);
    strcpy(this->dir, &(p[atoi(this->dir)]));
  }
  if (copy_to_tmp) {
    sprintf(s1, STR("%s%s"), tempdir, this->file);
    if (copyfile(s, s1) != 0) {
      log(LCAT_ERROR, STR("Refused dcc get %s: copy to %s FAILED!"), this->file, tempdir);
      dprintf(DP_HELP, STR("NOTICE %s :File system is broken; aborting queued files.\n"), this->to);
      strcpy(s, this->to);
      flush_fileq(s);
      return;
    }
  } else
    strcpy(s1, s);
  if (this->dir[0] == '*')
    sprintf(s, STR("%s/%s"), &this->dir[1], this->file);
  else
    sprintf(s, STR("%s%s%s"), this->dir, this->dir[0] ? "/" : "", this->file);
  x = raw_dcc_send(s1, this->to, this->nick, s);
  if (x == 1) {
    wipe_tmp_filename(s1, -1);
    log(LCAT_ERROR, STR("DCC connections full: GET %s [%s]"), s1, this->nick);
    dprintf(DP_HELP, STR("NOTICE %s :DCC connections full; aborting queued files.\n"), this->to);
    strcpy(s, this->to);
    flush_fileq(s);
    return;
  }
  if (x == 2) {
    wipe_tmp_filename(s1, -1);
    log(LCAT_ERROR, STR("DCC socket error: GET %s [%s]"), s1, this->nick);
    dprintf(DP_HELP, STR("NOTICE %s :DCC socket error; aborting queued files.\n"), this->to);
    strcpy(s, this->to);
    flush_fileq(s);
    return;
  }
  if (strcasecmp(this->to, this->nick))
    dprintf(DP_HELP, STR("NOTICE %s :Here is a file from %s ...\n"), this->to, this->nick);
  deq_this(this);
}

void check_tcl_sentrcvd(struct userrec *u, char *nick, char *path, p_tcl_bind_list h)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0, 0 };
  char *hand = u ? u->handle : "*";
  if (!h)
    return;
  Context;
  get_user_flagrec(u, &fr, NULL);
#ifdef G_USETCL
  Tcl_SetVar(interp, STR("_sr1"), hand, 0);
  Tcl_SetVar(interp, STR("_sr2"), nick, 0);
  Tcl_SetVar(interp, STR("_sr3"), path, 0);
#endif
  check_tcl_bind(h, hand, &fr, STR(" $_sr1 $_sr2 $_sr3"), MATCH_MASK | BIND_USE_ATTR | BIND_STACKABLE);
  Context;
}

void eof_dcc_fork_send(int idx)
{
  char s1[121];
  char *s2;

  Context;
  fclose(dcc[idx].u.xfer->f);
  if (!strcmp(dcc[idx].nick, STR("*users"))) {
    int x,
      y = 0;

    for (x = 0; x < dcc_total; x++)
      if ((!strcasecmp(dcc[x].nick, dcc[idx].host)) && (dcc[x].type->flags & DCT_BOT))
	y = x;
    if (y != 0) {
      dcc[y].status &= ~STAT_GETTING;
      dcc[y].status &= ~STAT_SHARE;
    }
    log(LCAT_ERROR, STR("Failed connection; aborted userfile transfer."));
    unlink(dcc[idx].u.xfer->filename);
  } else {
    neterror(s1);
    log(LCAT_ERROR, STR("DCC connection failed: SEND %s (%s!%s)"), dcc[idx].u.xfer->filename, dcc[idx].nick, dcc[idx].host);
    log(LCAT_ERROR, STR("    (%s)"), s1);
    s2 = nmalloc(strlen(tempdir) + strlen(dcc[idx].u.xfer->filename) + 1);
    sprintf(s2, STR("%s%s"), tempdir, dcc[idx].u.xfer->filename);
    unlink(s2);
    nfree(s2);
  }
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

void eof_dcc_send(int idx)
{
  int ok,
    j;
  char ofn[121],
    nfn[121],
    s[1024],
   *hand;
  struct userrec *u;

  Context;
  if (dcc[idx].u.xfer->length == dcc[idx].status) {
    /* success */
    ok = 0;
    fclose(dcc[idx].u.xfer->f);
    if (!strcmp(dcc[idx].nick, STR("*users"))) {
      finish_share(idx);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
    }
    log(LCAT_INFO, STR("Completed dcc send %s from %s!%s"), dcc[idx].u.xfer->filename, dcc[idx].nick, dcc[idx].host);
    simple_sprintf(s, STR("%s!%s"), dcc[idx].nick, dcc[idx].host);
    u = get_user_by_host(s);
    hand = u ? u->handle : "*";
    /* move the file from /tmp */
    /* slwstub - filenames to long segfault and kill the eggdrop
     * NOTE: This is NOT A PTF. Its a circumvention, a workaround
     * I'm not moving the file from the receiving area. You may
     * want to inspect it, shorten the name, give credit or NOT! */
    if (strlen(dcc[idx].u.xfer->filename) > MAX_FILENAME_LENGTH) {
      /* the filename is to long... blow it off */
      log(LCAT_ERROR, STR("Filename %d length. Way To LONG."), strlen(dcc[idx].u.xfer->filename));
      dprintf(DP_HELP, STR("NOTICE %s :Filename %d length Way To LONG!\n"), dcc[idx].nick, strlen(dcc[idx].u.xfer->filename));
      dprintf(DP_HELP, STR("NOTICE %s :To Bad So Sad Your Dad!\n"), dcc[idx].nick);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
    }
    /* slwstub - filenames to long segfault and kill the eggdrop */
    simple_sprintf(ofn, STR("%s%s"), tempdir, dcc[idx].u.xfer->filename);
    simple_sprintf(nfn, STR("%s%s"), dcc[idx].u.xfer->dir, dcc[idx].u.xfer->filename);
    if (movefile(ofn, nfn))
      log(LCAT_ERROR, STR("FAILED move %s from %s ! File lost!"), dcc[idx].u.xfer->filename, tempdir);
    else {
      check_tcl_sentrcvd(u, dcc[idx].nick, nfn, H_rcvd);
    }
    for (j = 0; j < dcc_total; j++)
      if (!ok && (dcc[j].type->flags & (DCT_GETNOTES)) && !strcasecmp(dcc[j].nick, hand)) {
	ok = 1;
	dprintf(j, STR("Thanks for the file!\n"));
      }
    if (!ok)
      dprintf(DP_HELP, STR("NOTICE %s :Thanks for the file!\n"), dcc[idx].nick);
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }
  /* failure :( */
  Context;
  fclose(dcc[idx].u.xfer->f);
  if (!strcmp(dcc[idx].nick, STR("*users"))) {
    int x,
      y = 0;

    for (x = 0; x < dcc_total; x++)
      if ((!strcasecmp(dcc[x].nick, dcc[idx].host)) && (dcc[x].type->flags & DCT_BOT))
	y = x;
    if (y) {
      log(LCAT_ERROR, STR("Lost userfile transfer from %s; aborting."), dcc[y].nick);
      unlink(dcc[idx].u.xfer->filename);
      /* drop that bot */
      dprintf(y, STR("bye\n"));
      simple_sprintf(s, STR("Disconnected %s (aborted userfile transfer)"), dcc[y].nick);
      botnet_send_unlinked(y, dcc[y].nick, s);
      if (y < idx) {
	int t = y;

	y = idx;
	idx = t;
      }
      killsock(dcc[y].sock);
      lostdcc(y);
    }
  } else {
    log(LCAT_ERROR, STR("Lost dcc send %s from %s!%s (%lu/%lu)"), dcc[idx].u.xfer->filename, dcc[idx].nick, dcc[idx].host, dcc[idx].status, dcc[idx].u.xfer->length);
    sprintf(s, STR("%s%s"), tempdir, dcc[idx].u.xfer->filename);
    unlink(s);
    killsock(dcc[idx].sock);
    lostdcc(idx);
  }
}

void dcc_get(int idx, char *buf, int len)
{
  char xnick[NICKLEN],
   *bf;
  unsigned char bbuf[4];
  unsigned long cmp,
    l;
  int w = len + dcc[idx].u.xfer->sofar,
    p = 0;

  Context;
  dcc[idx].timeval = now;

  if (w < 4) {
    my_memcpy(&(dcc[idx].u.xfer->buf[dcc[idx].u.xfer->sofar]), buf, len);
    dcc[idx].u.xfer->sofar += len;
    return;
  } else if (w == 4) {
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
  }				/* go back and read it again, it *does*
				 * make sense ;) */
  dcc[idx].u.xfer->sofar = 0;
  /* this is more compatable than ntohl for machines where an int
   * is more than 4 bytes: */
  cmp = ((unsigned int) (bbuf[0]) << 24) + ((unsigned int) (bbuf[1]) << 16) + ((unsigned int) (bbuf[2]) << 8) + bbuf[3];
  dcc[idx].u.xfer->acked = cmp;
  if ((cmp > dcc[idx].status) && (cmp <= dcc[idx].u.xfer->length)) {
    /* attempt to resume I guess */
    if (!strcmp(dcc[idx].nick, STR("*users"))) {
      log(LCAT_ERROR, STR("!!! Trying to skip ahead on userfile transfer"));
    } else {
      fseek(dcc[idx].u.xfer->f, cmp, SEEK_SET);
      dcc[idx].status = cmp;
      log(LCAT_INFO, STR("Resuming file transfer at %dk for %s to %s"), (int) (cmp / 1024), dcc[idx].u.xfer->filename, dcc[idx].nick);
    }
  }
  if (cmp != dcc[idx].status)
    return;
  if (dcc[idx].status == dcc[idx].u.xfer->length) {
    Context;
    /* successful send, we're done */
    killsock(dcc[idx].sock);
    fclose(dcc[idx].u.xfer->f);
    if (!strcmp(dcc[idx].nick, STR("*users"))) {
      int x,
        y = 0;

      for (x = 0; x < dcc_total; x++)
	if ((!strcasecmp(dcc[x].nick, dcc[idx].host)) && (dcc[x].type->flags & DCT_BOT))
	  y = x;
      if (y != 0)
	dcc[y].status &= ~STAT_SENDING;
      log(LCAT_BOT, STR("Completed userfile transfer to %s."), dcc[y].nick);
      unlink(dcc[idx].u.xfer->filename);
      /* any sharebot things that were queued: */
      dump_resync(y);
      xnick[0] = 0;
    } else {
      struct userrec *u = get_user_by_handle(userlist,
					     dcc[idx].u.xfer->from);
      char *nfn = NULL;

      check_tcl_sentrcvd(u, dcc[idx].nick, dcc[idx].u.xfer->dir, H_sent);
      if (strchr(dcc[idx].u.xfer->filename, ' '))
	nfn = replace_spaces(dcc[idx].u.xfer->filename);
      log(LCAT_INFO, STR("Finished dcc send %s to %s"), nfn ? nfn : dcc[idx].u.xfer->filename, dcc[idx].nick);
      if (nfn)
	nfree(nfn);
      wipe_tmp_filename(dcc[idx].u.xfer->filename, idx);
      strcpy((char *) xnick, dcc[idx].nick);
    }
    lostdcc(idx);
    /* any to dequeue? */
    if (!at_limit(xnick))
      send_next_file(xnick);
    return;
  }
  Context;
  /* note: is this fseek necessary any more? */

/* fseek(dcc[idx].u.xfer->f,dcc[idx].status,0);   */
  l = dcc_block;
  if ((l == 0) || (dcc[idx].status + l > dcc[idx].u.xfer->length))
    l = dcc[idx].u.xfer->length - dcc[idx].status;
  bf = (char *) nmalloc(l + 1);
  fread(bf, l, 1, dcc[idx].u.xfer->f);
  tputs(dcc[idx].sock, bf, l);
  nfree(bf);
  dcc[idx].status += l;
}

void eof_dcc_get(int idx)
{
  char xnick[NICKLEN],
    s[1024];

  Context;
  fclose(dcc[idx].u.xfer->f);
  if (!strcmp(dcc[idx].nick, STR("*users"))) {
    int x,
      y = 0;

    for (x = 0; x < dcc_total; x++)
      if ((!strcasecmp(dcc[x].nick, dcc[idx].host)) && (dcc[x].type->flags & DCT_BOT))
	y = x;
    log(LCAT_ERROR, STR("Lost userfile transfer; aborting."));

/* unlink(dcc[idx].u.xfer->filename); *//* <- already unlinked */
    xnick[0] = 0;
    /* drop that bot */
    dprintf(-dcc[y].sock, STR("bye\n"));
    simple_sprintf(s, STR("Disconnected %s (aborted userfile transfer)"), dcc[y].nick);
    botnet_send_unlinked(y, dcc[y].nick, s);
    if (y < idx) {
      int t = y;

      y = idx;
      idx = t;
    }
    killsock(dcc[y].sock);
    lostdcc(y);
    return;
  } else {
    log(LCAT_ERROR, STR("Lost dcc get %s from %s!%s"), dcc[idx].u.xfer->filename, dcc[idx].nick, dcc[idx].host);
    wipe_tmp_filename(dcc[idx].u.xfer->filename, idx);
    strcpy(xnick, dcc[idx].nick);
  }
  killsock(dcc[idx].sock);
  lostdcc(idx);
  /* send next queued file if there is one */
  if (!at_limit(xnick))
    send_next_file(xnick);
  Context;
}

void dcc_send(int idx, char *buf, int len)
{
  char s[512];
  unsigned long sent;

  Context;

/*
  if (!strcmp(dcc[idx].nick, STR("*users"))) {
    ln = buf;
    while (ln) {
      eol=strchr(ln,'\n');
      if (eol) 
        *eol++=0;
    }
  }
*/
  fwrite(buf, len, 1, dcc[idx].u.xfer->f);

  dcc[idx].status += len;
  /* put in network byte order */
  sent = dcc[idx].status;
  s[0] = (sent / (1 << 24));
  s[1] = (sent % (1 << 24)) / (1 << 16);
  s[2] = (sent % (1 << 16)) / (1 << 8);
  s[3] = (sent % (1 << 8));

/*  if (strcmp(dcc[idx].nick, STR("*users"))) */
  tputs(dcc[idx].sock, s, 4);
  dcc[idx].timeval = now;
  if ((dcc[idx].status > dcc[idx].u.xfer->length) && (dcc[idx].u.xfer->length > 0)) {
    dprintf(DP_HELP, STR("NOTICE %s :Bogus file length.\n"), dcc[idx].nick);
    log(LCAT_ERROR, STR("File too long: dropping dcc send %s from %s!%s"), dcc[idx].u.xfer->filename, dcc[idx].nick, dcc[idx].host);
    fclose(dcc[idx].u.xfer->f);
    sprintf(s, STR("%s%s"), tempdir, dcc[idx].u.xfer->filename);
    unlink(s);
    killsock(dcc[idx].sock);
    lostdcc(idx);
  }
}

#ifdef G_USETCL
int tcl_getfileq STDVAR { char s[512];
  fileq_t *q = fileq;

    BADARGS(2, 2, STR(" handle"));
  while (q != NULL) {
    if (!strcasecmp(q->nick, argv[1])) {
      if (q->dir[0] == '*')
	sprintf(s, STR("%s %s/%s"), q->to, &q->dir[1], q->file);
      else
	sprintf(s, "%s /%s%s%s", q->to, q->dir, q->dir[0] ? "/" : "", q->file);
      Tcl_AppendElement(irp, s);
    }
    q = q->next;
  }
  return TCL_OK;
}

int tcl_dccsend STDVAR { char s[5],
    sys[512],
   *nfn;
  int i;
  FILE *f;

    BADARGS(3, 3, STR(" filename ircnick"));
    f = fopen(argv[1], "r");
  if (f == NULL) {
    /* file not found */
    Tcl_AppendResult(irp, "3", NULL);
    return TCL_OK;
  }
  fclose(f);

  nfn = strrchr(argv[1], '/');
  if (nfn == NULL)
    nfn = argv[1];
  else
    nfn++;
  if (at_limit(argv[2])) {
    /* queue that mother */
    if (nfn == argv[1])
      queue_file("*", nfn, STR("(script)"), argv[2]);
    else {
      nfn--;
      *nfn = 0;
      nfn++;
      sprintf(sys, STR("*%s"), argv[1]);
      queue_file(sys, nfn, STR("(script)"), argv[2]);
    }
    Tcl_AppendResult(irp, "4", NULL);
    return TCL_OK;
  }
  if (copy_to_tmp) {
    sprintf(sys, STR("%s%s"), tempdir, nfn);	/* new filename, in /tmp */
    copyfile(argv[1], sys);
  } else
    strcpy(sys, argv[1]);
  i = raw_dcc_send(sys, argv[2], "*", argv[1]);
  if (i > 0)
    wipe_tmp_filename(sys, -1);
  sprintf(s, "%d", i);
  Tcl_AppendResult(irp, s, NULL);
  return TCL_OK;
}

tcl_cmds mytcls[] = {
  {"dccsend", tcl_dccsend}
  ,
  {"getfileq", tcl_getfileq}
  ,
  {0, 0}
};
#endif

void transfer_get_timeout(int i)
{
  char xx[1024];

  Context;
  if (!strcmp(dcc[i].nick, STR("*users"))) {
    int x,
      y = 0;

    for (x = 0; x < dcc_total; x++)
      if ((!strcasecmp(dcc[x].nick, dcc[i].host)) && (dcc[x].type->flags & DCT_BOT))
	y = x;
    if (y != 0) {
      dcc[y].status &= ~STAT_SENDING;
      dcc[y].status &= ~STAT_SHARE;
    }
    unlink(dcc[i].u.xfer->filename);
    log(LCAT_ERROR, STR("Timeout on userfile transfer."));
    dprintf(y, STR("bye\n"));
    simple_sprintf(xx, STR("Disconnected %s (timed-out userfile transfer)"), dcc[y].nick);
    botnet_send_unlinked(y, dcc[y].nick, xx);
    if (y < i) {
      int t = y;

      y = i;
      i = t;
    }
    killsock(dcc[y].sock);
    lostdcc(y);
    xx[0] = 0;
  } else {
    char *p,
     *nfn,
     *buf = NULL;

    strcpy(xx, dcc[i].u.xfer->filename);
    p = strrchr(xx, '/');
    nfn = p ? p + 1 : xx;
    if (strchr(nfn, ' '))
      nfn = buf = replace_spaces(nfn);
    dprintf(DP_HELP, STR("NOTICE %s :Timeout during transfer, aborting %s.\n"), dcc[i].nick, nfn);
    log(LCAT_ERROR, STR("DCC timeout: GET %s (%s) at %lu/%lu"), nfn, dcc[i].nick, dcc[i].status, dcc[i].u.xfer->length);
    wipe_tmp_filename(dcc[i].u.xfer->filename, i);
    strcpy(xx, dcc[i].nick);
    if (buf)
      nfree(buf);
  }
  killsock(dcc[i].sock);
  lostdcc(i);
  if (!at_limit(xx))
    send_next_file(xx);
}

void tout_dcc_send(int idx)
{
  if (!strcmp(dcc[idx].nick, STR("*users"))) {
    int x,
      y = 0;

    for (x = 0; x < dcc_total; x++)
      if ((!strcasecmp(dcc[x].nick, dcc[idx].host)) && (dcc[x].type->flags & DCT_BOT))
	y = x;
    if (y != 0) {
      dcc[y].status &= ~STAT_GETTING;
      dcc[y].status &= ~STAT_SHARE;
    }
    unlink(dcc[idx].u.xfer->filename);
    log(LCAT_ERROR, STR("Timeout on userfile transfer."));
  } else {
    char xx[1024],
     *nfn,
     *buf = NULL;

    nfn = dcc[idx].u.xfer->filename;
    if (strchr(nfn, ' '))
      nfn = buf = replace_spaces(nfn);
    dprintf(DP_HELP, STR("NOTICE %s :Timeout during transfer, aborting %s.\n"), dcc[idx].nick, nfn);
    log(LCAT_ERROR, STR("DCC timeout: SEND %s (%s) at %lu/%lu"), nfn, dcc[idx].nick, dcc[idx].status, dcc[idx].u.xfer->length);
    sprintf(xx, STR("%s%s"), tempdir, dcc[idx].u.xfer->filename);
    unlink(xx);
    if (buf)
      nfree(buf);
  }
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

void display_dcc_get(int idx, char *buf)
{
  if (dcc[idx].status == dcc[idx].u.xfer->length)
    sprintf(buf, STR("send  (%lu)/%lu\n    Filename: %s\n"), dcc[idx].u.xfer->acked, dcc[idx].u.xfer->length, dcc[idx].u.xfer->filename);
  else
    sprintf(buf, STR("send  %lu/%lu\n    Filename: %s\n"), dcc[idx].status, dcc[idx].u.xfer->length, dcc[idx].u.xfer->filename);
}

void display_dcc_get_p(int idx, char *buf)
{
  sprintf(buf, STR("send  waited %lus    Filename: %s\n"), now - dcc[idx].timeval, dcc[idx].u.xfer->filename);
}

void display_dcc_send(int idx, char *buf)
{
  sprintf(buf, STR("send  %lu/%lu\n    Filename: %s\n"), dcc[idx].status, dcc[idx].u.xfer->length, dcc[idx].u.xfer->filename);
}

void display_dcc_fork_send(int idx, char *buf)
{
  sprintf(buf, STR("conn  send"));
}

int expmem_dcc_xfer(void *x)
{
  return sizeof(struct xfer_info);
}

void kill_dcc_xfer(int idx, void *x)
{
  nfree(x);
}

void out_dcc_xfer(int idx, char *buf, void *x)
{
}

struct dcc_table DCC_SEND = {
  "SEND",
  DCT_FILETRAN | DCT_FILESEND | DCT_VALIDIDX,
  eof_dcc_send,
  dcc_send,
  &wait_dcc_xfer,
  tout_dcc_send,
  display_dcc_send,
  expmem_dcc_xfer,
  kill_dcc_xfer,
  out_dcc_xfer
};

void dcc_fork_send(int idx, char *x, int y);

struct dcc_table DCC_FORK_SEND = {
  "FORK_SEND",
  DCT_FILETRAN | DCT_FORKTYPE | DCT_FILESEND | DCT_VALIDIDX,
  eof_dcc_fork_send,
  dcc_fork_send,
  &wait_dcc_xfer,
  eof_dcc_fork_send,
  display_dcc_fork_send,
  expmem_dcc_xfer,
  kill_dcc_xfer,
  out_dcc_xfer
};

void dcc_fork_send(int idx, char *x, int y)
{
  char s1[121];

  if (dcc[idx].type != &DCC_FORK_SEND)
    return;
  dcc[idx].type = &DCC_SEND;
  sprintf(s1, STR("%s!%s"), dcc[idx].nick, dcc[idx].host);
  if (strcmp(dcc[idx].nick, STR("*users"))) {
    log(LCAT_WARNING, STR("DCC connection: SEND %s (%s)"), dcc[idx].type == &DCC_SEND ? dcc[idx].u.xfer->filename : "", s1);
  }
}

struct dcc_table DCC_GET = {
  "GET",
  DCT_FILETRAN | DCT_VALIDIDX,
  eof_dcc_get,
  dcc_get,
  &wait_dcc_xfer,
  transfer_get_timeout,
  display_dcc_get,
  expmem_dcc_xfer,
  kill_dcc_xfer,
  out_dcc_xfer
};

struct dcc_table DCC_GET_PENDING = {
  "GET_PENDING",
  DCT_FILETRAN | DCT_VALIDIDX,
  eof_dcc_get,
  dcc_get_pending,
  &wait_dcc_xfer,
  transfer_get_timeout,
  display_dcc_get_p,
  expmem_dcc_xfer,
  kill_dcc_xfer,
  out_dcc_xfer
};

void wipe_tmp_filename(char *fn, int idx)
{
  int i,
    ok = 1;

  if (!copy_to_tmp)
    return;
  for (i = 0; i < dcc_total; i++)
    if (i != idx)
      if ((dcc[i].type == &DCC_GET) || (dcc[i].type == &DCC_GET_PENDING))
	if (!strcmp(dcc[i].u.xfer->filename, fn))
	  ok = 0;
  if (ok)
    unlink(fn);
}

/* return true if this user has >= the maximum number of file xfers going */
int at_limit(char *nick)
{
  int i,
    x = 0;

  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type == &DCC_GET) || (dcc[i].type == &DCC_GET_PENDING))
      if (!strcasecmp(dcc[i].nick, nick))
	x++;
  return (x >= dcc_limit);
}

void dcc_get_pending(int idx, char *buf, int len)
{
  unsigned long ip;
  unsigned short port;
  int i;
  char *bf,
    s[UHOSTLEN];

  Context;
  i = answer(dcc[idx].sock, s, &ip, &port, 1);
  killsock(dcc[idx].sock);
  dcc[idx].sock = i;
  dcc[idx].addr = ip;
  dcc[idx].port = (int) port;
  if (dcc[idx].sock == -1) {
    neterror(s);
    dprintf(DP_HELP, STR("NOTICE %s :Bad connection (%s)\n"), dcc[idx].nick, s);
    log(LCAT_ERROR, STR("DCC bad connection: GET %s (%s!%s)"), dcc[idx].u.xfer->filename, dcc[idx].nick, dcc[idx].host);
    lostdcc(idx);
    return;
  }
  /* file was already opened */
  if ((dcc_block == 0) || (dcc[idx].u.xfer->length < dcc_block))
    dcc[idx].status = dcc[idx].u.xfer->length;
  else
    dcc[idx].status = dcc_block;
  dcc[idx].type = &DCC_GET;
  bf = (char *) nmalloc(dcc[idx].status + 1);
  fread(bf, dcc[idx].status, 1, dcc[idx].u.xfer->f);
  tputs(dcc[idx].sock, bf, dcc[idx].status);
  nfree(bf);
  dcc[idx].timeval = now;
  /* leave f open until file transfer is complete */
}

/*
void show_queued_files(int idx)
{
  int i, cnt = 0, len;
  char spaces[] = "                                 ";
  fileq_t *q = fileq;

  while (q != NULL) {
    if (!strcasecmp(q->nick, dcc[idx].nick)) {
      if (!cnt) {
	spaces[HANDLEN - 9] = 0;
	dprintf(idx, STR("  Send to  %s  Filename\n"), spaces);
	dprintf(idx, STR("  ---------%s  --------------------\n"), spaces);
	spaces[HANDLEN - 9] = ' ';
      }
      cnt++;
      spaces[len = HANDLEN - strlen(q->to)] = 0;
      if (q->dir[0] == '*')
	dprintf(idx, STR("  %s%s  %s/%s\n"), q->to, spaces, &q->dir[1],
		q->file);
      else
	dprintf(idx, STR("  %s%s  /%s%s%s\n"), q->to, spaces, q->dir,
		q->dir[0] ? "/" : "", q->file);
      spaces[len] = ' ';
    }
    q = q->next;
  }
  for (i = 0; i < dcc_total; i++) {
    if (((dcc[i].type == &DCC_GET_PENDING) || (dcc[i].type == &DCC_GET)) &&
	((!strcasecmp(dcc[i].nick, dcc[idx].nick)) ||
	 (!strcasecmp(dcc[i].u.xfer->from, dcc[idx].nick)))) {
      char *nfn;

      if (!cnt) {
	spaces[HANDLEN - 9] = 0;
	dprintf(idx, STR("  Send to  %s  Filename\n"), spaces);
	dprintf(idx, STR("  ---------%s  --------------------\n"), spaces);
	spaces[HANDLEN - 9] = ' ';
      }
      nfn = strrchr(dcc[i].u.xfer->filename, '/');
      if (nfn == NULL)
	nfn = dcc[i].u.xfer->filename;
      else
	nfn++;
      cnt++;
      spaces[len = HANDLEN - strlen(dcc[i].nick)] = 0;
      if (dcc[i].type == &DCC_GET_PENDING)
	dprintf(idx, STR("  %s%s  %s  [WAITING]\n"), dcc[i].nick, spaces,
		nfn);
      else
	dprintf(idx, STR("  %s%s  %s  (%.1f%% done)\n"), dcc[i].nick, spaces,
		nfn, (100.0 * ((float) dcc[i].status /
			       (float) dcc[i].u.xfer->length)));
      spaces[len] = ' ';
    }
  }
  if (!cnt)
    dprintf(idx, STR("No files queued up.\n"));
  else
    dprintf(idx, STR("Total: %d\n"), cnt);
}
*/

/*
void fileq_cancel(int idx, char *par)
{
  int fnd = 1, matches = 0, atot = 0, i;
  fileq_t *q;
  char s[256];

  while (fnd) {
    q = fileq;
    fnd = 0;
    while (q != NULL) {
      if (!strcasecmp(dcc[idx].nick, q->nick)) {
	if (q->dir[0] == '*')
	  sprintf(s, STR("%s/%s"), &q->dir[1], q->file);
	else
	  sprintf(s, STR("/%s%s%s"), q->dir, q->dir[0] ? "/" : "", q->file);
	if (wild_match_file(par, s)) {
	  dprintf(idx, STR("Cancelled: %s to %s\n"), s, q->to);
	  fnd = 1;
	  deq_this(q);
	  q = NULL;
	  matches++;
	}
	if ((!fnd) && (wild_match_file(par, q->file))) {
	  dprintf(idx, STR("Cancelled: %s to %s\n"), s, q->to);
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
  for (i = 0; i < dcc_total; i++) {
    if (((dcc[i].type == &DCC_GET_PENDING) || (dcc[i].type == &DCC_GET)) &&
	((!strcasecmp(dcc[i].nick, dcc[idx].nick)) ||
	 (!strcasecmp(dcc[i].u.xfer->from, dcc[idx].nick)))) {
      char *nfn = strrchr(dcc[i].u.xfer->filename, '/');

      if (nfn == NULL)
	nfn = dcc[i].u.xfer->filename;
      else
	nfn++;
      if (wild_match_file(par, nfn)) {
	dprintf(idx, STR("Cancelled: %s  (aborted dcc send)\n"), nfn);
	if (strcasecmp(dcc[i].nick, dcc[idx].nick))
	  dprintf(DP_HELP, STR("NOTICE %s :Transfer of %s aborted by %s\n"), dcc[i].nick,
		  nfn, dcc[idx].nick);
	if (dcc[i].type == &DCC_GET)
	  log(LCAT_ERROR, STR("DCC cancel: GET %s (%s) at %lu/%lu"), nfn,
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
    dprintf(idx, STR("No matches.\n"));
  else
    dprintf(idx, STR("Cancelled %d file%s.\n"), matches, matches > 1 ? "s" : "");
  for (i = 0; i < atot; i++)
    if (!at_limit(dcc[idx].nick))
      send_next_file(dcc[idx].nick);
}
*/
int raw_dcc_send(char *filename, char *nick, char *from, char *dir)
{
  int zz,
    port,
    i;
  char *nfn;
  struct stat ss;
  FILE *f;

  Context;
  port = reserved_port;
  zz = open_listen(&port);
  if (zz == (-1))
    return DCCSEND_NOSOCK;
  nfn = strrchr(filename, '/');
  if (nfn == NULL)
    nfn = filename;
  else
    nfn++;
  f = fopen(filename, "r");
  if (!f)
    return DCCSEND_BADFN;
  if ((i = new_dcc(&DCC_GET_PENDING, sizeof(struct xfer_info))) == -1)
    return DCCSEND_FULL;

  stat(filename, &ss);
  dcc[i].sock = zz;
  dcc[i].addr = (IP) (-559026163);
  dcc[i].port = port;
  strcpy(dcc[i].nick, nick);
  strcpy(dcc[i].host, STR("irc"));
  strcpy(dcc[i].u.xfer->filename, filename);
  strcpy(dcc[i].u.xfer->from, from);
  strcpy(dcc[i].u.xfer->dir, dir);
  dcc[i].u.xfer->length = ss.st_size;
  dcc[i].timeval = now;
  dcc[i].u.xfer->f = f;
  if (nick[0] != '*') {
    char *buf = NULL;

    if (strchr(nfn, ' '))
      nfn = buf = replace_spaces(nfn);
    dprintf(DP_HELP, STR("PRIVMSG %s :\001DCC SEND %s %lu %d %lu\001\n"), nick, nfn, iptolong(natip[0] ? (IP) inet_addr(natip) : getmyip()), port, ss.st_size);
    log(LCAT_INFO, STR("Begin DCC send %s to %s"), nfn, nick);
    if (buf)
      nfree(buf);
  }
  return DCCSEND_OK;
}

#ifdef G_USETCL
tcl_ints myints[] = {
  {"max-dloads", &dcc_limit}
  ,
  {"dcc-block", &dcc_block}
  ,
  {"copy-to-tmp", &copy_to_tmp}
  ,
  {"xfer-timeout", &wait_dcc_xfer}
  ,
  {0, 0}
};
#endif

void filesys_dcc_send(char *nick, char *from, struct userrec *u, char *text)
{
  char *param,
   *ip,
   *prt,
    buf[512],
    s1[512],
   *msg = buf;
  FILE *f;
  int atr = u ? u->flags : 0,
    i,
    j;

  Context;
  strcpy(buf, text);
  param = newsplit(&msg);
  if (!(atr & USER_OWNER)) {
    log(LCAT_WARNING, STR("Refused DCC SEND %s (no access): %s!%s"), param, nick, from);
  } else if (!dccin[0]) {
    dprintf(DP_HELP, STR("NOTICE %s :DCC file transfers not supported.\n"), nick);
    log(LCAT_WARNING, STR("Refused dcc send %s from %s!%s"), param, nick, from);
  } else if (strchr(param, '/')) {
    dprintf(DP_HELP, STR("NOTICE %s :Filename cannot have '/' in it...\n"), nick);
    log(LCAT_WARNING, STR("Refused dcc send %s from %s!%s"), param, nick, from);
  } else {
    ip = newsplit(&msg);
    prt = newsplit(&msg);
    if ((atoi(prt) < min_dcc_port) || (atoi(prt) > max_dcc_port)) {
      /* invalid port range, do clients even use over 5000?? */
      log(LCAT_WARNING, STR("Refused dcc send %s (%s): invalid port"), param, nick);
    } else if (atoi(msg) == 0) {
      dprintf(DP_HELP, STR("NOTICE %s :Sorry, file size info must be included.\n"), nick);
      log(LCAT_WARNING, STR("Refused dcc send %s (%s): no file size"), param, nick);
    } else if (atoi(msg) > (dcc_maxsize * 1024)) {
      dprintf(DP_HELP, STR("NOTICE %s :Sorry, file too large.\n"), nick);
      log(LCAT_ERROR, STR("Refused dcc send %s (%s): file too large"), param, nick);
    } else {
      /* This looks like a good place for a sanity check. */
      if (!sanitycheck_dcc(nick, from, ip, prt))
	return;
      i = new_dcc(&DCC_FORK_SEND, sizeof(struct xfer_info));

      if (i < 0) {
	dprintf(DP_HELP, STR("NOTICE %s :Sorry, too many DCC connections.\n"), nick);
	log(LCAT_ERROR, STR("DCC connections full: SEND %s (%s!%s)"), param, nick, from);
      }
      dcc[i].addr = my_atoul(ip);
      dcc[i].port = atoi(prt);
      dcc[i].sock = -1;
      strcpy(dcc[i].nick, nick);
      strcpy(dcc[i].host, from);
      if (param[0] == '.')
	param[0] = '_';
      strncpy0(dcc[i].u.xfer->filename, param, 120);
      strcpy(dcc[i].u.xfer->dir, dccin);
      dcc[i].u.xfer->length = atoi(msg);
      sprintf(s1, STR("%s%s"), dcc[i].u.xfer->dir, param);
      Context;
      f = fopen(s1, "r");
      if (f) {
	fclose(f);
	dprintf(DP_HELP, STR("NOTICE %s :That file already exists.\n"), nick);
	lostdcc(i);
      } else {
	/* check for dcc-sends in process with the same filename */
	for (j = 0; j < dcc_total; j++)
	  if (j != i) {
	    if ((dcc[j].type->flags & (DCT_FILETRAN | DCT_FILESEND))
		== (DCT_FILETRAN | DCT_FILESEND)) {
	      if (!strcmp(param, dcc[j].u.xfer->filename)) {
		dprintf(DP_HELP, STR("NOTICE %s :That file is already being sent.\n"), nick);
		lostdcc(i);
		return;
	      }
	    }
	  }
	/* put uploads in /tmp first */
	sprintf(s1, STR("%s%s"), tempdir, param);
	dcc[i].u.xfer->f = fopen(s1, "w");
	if (dcc[i].u.xfer->f == NULL) {
	  dprintf(DP_HELP, STR("NOTICE %s :Can't create that file (temp dir error)\n"), nick);
	  lostdcc(i);
	} else {
	  dcc[i].timeval = now;
	  dcc[i].sock = getsock(SOCK_BINARY);
	  if (open_telnet_dcc(dcc[i].sock, ip, prt) < 0) {
	    dcc[i].type->eof(i);
	  }
	}
      }
    }
  }
}

int filesys_DCC_CHAT(char *nick, char *from, char *handle, char *object, char *keyword, char *text)
{
  struct userrec *u = get_user_by_handle(userlist, handle);

  Context;
  if (!strncasecmp(text, STR("SEND "), 5)) {
    filesys_dcc_send(nick, from, u, text + 5);
    return 1;
  }
  return 0;
}

cmd_t myctcp[] = {
  {"DCC", "", filesys_DCC_CHAT, "files:DCC"}
  ,
  {0, 0, 0, 0}
};

int transfer_expmem()
{
  return expmem_fileq();
}

void init_transfer()
{
  fileq = NULL;
  Context;
#ifdef G_USETCL
  add_tcl_commands(mytcls);
  add_tcl_ints(myints);
#endif
#ifdef G_USETCL
  H_rcvd = add_bind_table(STR("rcvd"), HT_STACKABLE, builtin_sentrcvd);
  H_sent = add_bind_table(STR("sent"), HT_STACKABLE, builtin_sentrcvd);
#endif
#ifdef LEAF
  add_builtins(H_ctcp, myctcp);
#endif
}
