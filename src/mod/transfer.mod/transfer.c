/*
 * transfer.c -- part of transfer.mod
 *
 */
/*
 * Small code snippets related to REGET/RESEND support were taken from
 * BitchX, copyright by panasync.
 */

#include "src/common.h"
#include "src/cmds.h"
#include "src/misc_file.h"
#include "src/misc.h"
#include "src/main.h"
#include "src/userrec.h"
#include "src/userent.h"
#include "src/tandem.h"
#include "src/net.h"
#include "src/users.h"

#define MAKING_TRANSFER
#include "transfer.h"

#include "src/mod/share.mod/share.h"
#include "src/mod/update.mod/update.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

extern int		bupdating;

#ifdef HUB
static int copy_to_tmp = 1;	/* Copy files to /tmp before transmitting? */
#endif /* HUB */
static int wait_dcc_xfer = 40;	/* Timeout time on DCC xfers */
static int dcc_limit = 4;	/* Maximum number of simultaneous file
				   downloads allowed */
static unsigned int dcc_block = 0;	/* Size of one dcc block */
static int quiet_reject = 1;        /* Quietly reject dcc chat or sends from
                                   users without access? */

/*
 * Prototypes
 */
struct dcc_table DCC_SEND;
#ifdef HUB
static void wipe_tmp_filename(char *, int);
static int at_limit(char *);
static void dcc_get_pending(int, char *, int);
struct dcc_table DCC_GET;
struct dcc_table DCC_GET_PENDING;
#endif /* HUB */

static fileq_t *fileq = NULL;


/*
 *   Misc functions
 */
#ifdef HUB
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
#endif /* HUB */

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

  fflush(dcc[idx].u.xfer->f);
  fsync(fileno(dcc[idx].u.xfer->f));
  
  fclose(dcc[idx].u.xfer->f);
  if (dcc[idx].u.xfer->length == dcc[idx].status) {
    int l;

    /* Success */
    ok = 0;
    if (!strcmp(dcc[idx].nick, "*users")) {
      finish_share(idx);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
    } else if (!strcmp(dcc[idx].nick, "*binary")) {
      finish_update(idx);
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
      putlog(LOG_MISC | LOG_FILES, "*", TRANSFER_FAILED_MOVE, nfn, ofn);

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
      chatout("*** %s %s\n", dcc[y].nick, s);

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

#ifdef HUB
/* Determine byte order. Used for resend DCC startup packets.
 */
static __inline__ u_8bit_t byte_order_test(void)
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
__inline__ static void handle_resend_packet(int idx, transfer_reget *reget_data)
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
    egg_memcpy(&(dcc[idx].u.xfer->buf[dcc[idx].u.xfer->sofar]), buf, len);
    dcc[idx].u.xfer->sofar += len;
    return;
  /* Waiting for the 8 bit reget packet? */
  } else if (dcc[idx].u.xfer->type == XFER_RESEND_PEND) {
    /* The 8 bit packet is complete now. Parse it. */
    if (w == 8) {
      transfer_reget reget_data;

      egg_memcpy(&reget_data, dcc[idx].u.xfer->buf, dcc[idx].u.xfer->sofar);
      egg_memcpy(&reget_data + dcc[idx].u.xfer->sofar, buf, len);
      handle_resend_packet(idx, &reget_data);
      cmp = dcc[idx].u.xfer->offset;
    } else
      return;
    /* Fall through! */
  /* No, only want 4 bit ack responses. */
  } else {
    /* Complete packet? */
    if (w == 4) {
      egg_memcpy(bbuf, dcc[idx].u.xfer->buf, dcc[idx].u.xfer->sofar);
      egg_memcpy(&(bbuf[dcc[idx].u.xfer->sofar]), buf, len);
    } else {
      p = ((w - 1) & ~3) - dcc[idx].u.xfer->sofar;
      w = w - ((w - 1) & ~3);
      if (w < 4) {
	egg_memcpy(dcc[idx].u.xfer->buf, &(buf[p]), w);
	return;
      }
      egg_memcpy(bbuf, &(buf[p]), w);
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
      putlog(LOG_BOTS, "*", TRANSFER_TRY_SKIP_AHEAD);
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
      int x, y = 0;

      for (x = 0; x < dcc_total; x++)
	if (!egg_strcasecmp(dcc[x].nick, dcc[idx].host) &&
	    (dcc[x].type->flags & DCT_BOT))
	  y = x;
      if (y != 0)
	dcc[y].status &= ~STAT_SENDING;

      putlog(LOG_BOTS, "*", TRANSFER_COMPLETED_USERFILE, dcc[y].nick);
      unlink(dcc[idx].u.xfer->filename);
      /* Any sharebot things that were queued: */
      dprintf(y, "s !\n");
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
      /* Download is credited to the user who requested it
       * (not the user who actually received it)
       */
      putlog(LOG_FILES, "*",TRANSFER_FINISHED_DCCSEND, dcc[idx].u.xfer->origname, dcc[idx].nick);
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
    /* Call `lost' DCC trigger now.
     */
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
#endif /* HUB */

void dcc_send(int idx, char *buf, int len)
{
  char s[SGRAB + 2] = "", *b = NULL;
  unsigned long sent;

  fwrite(buf, len, 1, dcc[idx].u.xfer->f);

  fflush(dcc[idx].u.xfer->f);
  fsync(fileno(dcc[idx].u.xfer->f));

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

#ifdef HUB
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

    p = strrchr(dcc[i].u.xfer->origname, '/');
    dprintf(DP_HELP, TRANSFER_NOTICE_TIMEOUT,
	    dcc[i].nick, p ? p + 1 : dcc[i].u.xfer->origname);

    /* Call DCC `timeout' trigger now.
     */
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
#endif /* HUB */

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

#ifdef HUB
void display_dcc_get(int idx, char *buf)
{
  if (dcc[idx].status == dcc[idx].u.xfer->length)
    sprintf(buf, TRANSFER_SEND, dcc[idx].u.xfer->acked,
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
#endif /* HUB */

void display_dcc_send(int idx, char *buf)
{
  sprintf(buf,TRANSFER_SEND, dcc[idx].status,
	  dcc[idx].u.xfer->length, dcc[idx].u.xfer->origname);
}

void display_dcc_fork_send(int idx, char *buf)
{
  sprintf(buf, TRANSFER_CONN_SEND);
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

#ifdef HUB
static void outdone_dcc_xfer(int idx)
{
  if (dcc[idx].u.xfer->block_pending)
    dcc[idx].u.xfer->block_pending =
	    pump_file_to_sock(dcc[idx].u.xfer->f, dcc[idx].sock,
			      dcc[idx].u.xfer->block_pending);
}
#endif /* HUB */

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
  out_dcc_xfer,
  NULL
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
  out_dcc_xfer,
  NULL
};

void dcc_fork_send(int idx, char *x, int y)
{
  char s1[121] = "";

  if (dcc[idx].type != &DCC_FORK_SEND)
    return;
  dcc[idx].type = &DCC_SEND;
  dcc[idx].status = 0;
  dcc[idx].u.xfer->start_time = now;
  egg_snprintf(s1, sizeof s1, "%s!%s", dcc[idx].nick, dcc[idx].host);
  if (strcmp(dcc[idx].nick, "*users") && strcmp(dcc[idx].nick, "*binary"))
    putlog(LOG_MISC, "*", TRANSFER_DCC_CONN, dcc[idx].u.xfer->origname, s1);
}

#ifdef HUB
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
  outdone_dcc_xfer,
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
  out_dcc_xfer,
  NULL
};

static void dcc_get_pending(int idx, char *buf, int len)
{
  IP ip;
  port_t port;
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
    long unsigned int l;

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
  int zz, i;
  port_t port;
  char *nfn = NULL, *buf = NULL;
  long dccfilesize;
  FILE *f = NULL, *dccfile = NULL;
 
  sdprintf("raw_dcc_resend_send()");
  zz = (-1);
  dccfile = fopen(filename,"rb");
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
  f = fopen(filename, "rb");
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
/*
static int raw_dcc_resend(char *filename, char *nick, char *from, char *dir)
{
  return raw_dcc_resend_send(filename, nick, from, dir, 1);
}
*/

/* Starts a DCC_SEND connection.
 */
int raw_dcc_send(char *filename, char *nick, char *from, char *dir)
{
  return raw_dcc_resend_send(filename, nick, from, dir, 0);
}
#endif /* HUB */

#ifdef LEAF
/*
 *    CTCP functions
 */

/* This handles DCC RESUME requests.
 */
/* NOT EVEN USED :D 
static int ctcp_DCC_RESUME(char *nick, char *from, char *handle, char *object, char *keyword, char *text)
{
  char *action = NULL, *fn = NULL, buf[SGRAB + 2] = "", *msg = buf;
  int i, port;
  unsigned long offset;

  strcpy(msg, text);
  action = newsplit(&msg);
  if (egg_strcasecmp(action, "RESUME"))
    return BIND_RET_LOG;
  fn = newsplit(&msg);
  port = atoi(newsplit(&msg));
  offset = my_atoul(newsplit(&msg));
  // Search for existing SEND 
  for (i = 0; i < dcc_total; i++)
    if ((dcc[i].type == &DCC_GET_PENDING) &&
	(!rfc_casecmp(dcc[i].nick, nick)) && (dcc[i].port == port))
      break;
  // No matching transfer found?
  if (i == dcc_total)
    return BIND_RET_LOG;

  if (dcc[i].u.xfer->length <= offset) {
    char *p = strrchr(dcc[i].u.xfer->origname, '/');

    dprintf(DP_HELP,TRANSFER_DCC_IGNORED, nick, p ? p + 1 : dcc[i].u.xfer->origname);
    return BIND_RET_LOG;
  }
  dcc[i].u.xfer->type = XFER_RESUME_PEND;
  dcc[i].u.xfer->offset = offset;
  dprintf(DP_HELP, "PRIVMSG %s :\001DCC ACCEPT %s %d %u\001\n", nick, fn, port, offset);
  // Now we wait for the client to connect.
  return BIND_RET_BREAK;
}

static cmd_t transfer_ctcps[] =
{
  {"DCC",	"",	ctcp_DCC_RESUME,	"transfer:DCC"},
  {NULL,	NULL,	NULL,			NULL}
};
*/
/* Add our CTCP bindings if the server module is loaded. */
static int server_transfer_setup(char *mod)
{
  /* add_builtins("ctcp", transfer_ctcps); */
  return 1;
}
#endif /* LEAF */
/*
 *   Module functions
 */

void transfer_report(int idx, int details)
{
  if (details) {
    dprintf(idx,TRANSFER_STAT_BLOCK,
	    dcc_block, (dcc_block == 0) ? " (turbo dcc)" : "", dcc_limit);
  }
}

void transfer_init()
{
  fileq = NULL;
#ifdef LEAF
  server_transfer_setup(NULL);
#endif /* LEAF */
}
