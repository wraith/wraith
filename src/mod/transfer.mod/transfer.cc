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

#include "src/chanprog.h"
#include <bdlib/src/String.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

extern int		bupdating;

static interval_t wait_dcc_xfer = 40;	/* Timeout time on DCC xfers */
static int dcc_limit = 4;	/* Maximum number of simultaneous file
				   downloads allowed */
static unsigned int dcc_block = 0;	/* Size of one dcc block */

/*
 * Prototypes
 */
static void dcc_get_pending(int, char *, int);


/*
 *   Misc functions
 */

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
				       unsigned long pending_data)
{
  const unsigned long		 buf_len = pending_data >= PMAX_SIZE ?
	  					PMAX_SIZE : pending_data;
  char				*bf = (char *) calloc(1, buf_len);
  unsigned long	 actual_size;

  if (bf) {
    do {
      actual_size = pending_data >= buf_len ? buf_len : pending_data;
      if (fread(bf, actual_size, 1, file)) {
        tputs(sock, bf, actual_size);
        pending_data -= actual_size;
      }
    } while (!sock_has_data(SOCK_DATA_OUTGOING, sock) && pending_data != 0);
    free(bf);
  }
  return pending_data;
}

void eof_dcc_fork_send(int idx)
{
  fclose(dcc[idx].u.xfer->f);
  if (!strcmp(dcc[idx].nick, "*users")) {
    int x, y = -1;

    for (x = 0; x < dcc_total; x++)
      if (dcc[x].type && (dcc[x].type->flags & DCT_BOT) && (!strcasecmp(dcc[x].nick, dcc[idx].host))) {
	y = x;
	break;
      }
    if (y >= 0) {
      dcc[y].status &= ~STAT_GETTING;
      dcc[y].status &= ~STAT_SHARE;
    }
    putlog(LOG_BOTS, "*", "Failed connection; aborted userfile transfer.");
    unlink(dcc[idx].u.xfer->filename);
  } else if (!strcmp(dcc[idx].nick, "*binary")) {
    int x, y = -1;

    for (x = 0; x < dcc_total; x++)
      if (dcc[x].type && (dcc[x].type->flags & DCT_BOT) && (!strcasecmp(dcc[x].nick, dcc[idx].host))) {
	y = x;
	break;
      }
    if (y >= 0) {
      dcc[y].status &= ~STAT_GETTINGU;
    }
    putlog(LOG_BOTS, "*", "Failed binary transfer.");
    unlink(dcc[idx].u.xfer->filename);
  }
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void eof_dcc_send(int idx)
{
  char s[1024] = "";

  fflush(dcc[idx].u.xfer->f);
  fsync(fileno(dcc[idx].u.xfer->f));
  
  fclose(dcc[idx].u.xfer->f);
  if (dcc[idx].u.xfer->length == dcc[idx].status) {
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
  }
  /* Failure :( */
  if (!strcmp(dcc[idx].nick, "*users")) {
    int x, y = -1;

    for (x = 0; x < dcc_total; x++) {
      if (dcc[x].type && (dcc[x].type->flags & DCT_BOT) && (!strcasecmp(dcc[x].nick, dcc[idx].host))) {
	y = x;
        break;
      }
    }
    if (y >= 0) {
      putlog(LOG_BOTS, "*", "Lost userfile transfer from %s; aborting.", dcc[y].nick);
      unlink(dcc[idx].u.xfer->filename);
      /* Drop that bot */
      dprintf(y, "bye\n");
      simple_snprintf(s, sizeof s, "Disconnected %s (aborted userfile transfer)", dcc[y].nick);
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
    int x, y = -1;

    for (x = 0; x < dcc_total; x++)
      if (dcc[x].type && (dcc[x].type->flags & DCT_BOT) && (!strcasecmp(dcc[x].nick, dcc[idx].host))) {
	y = x;
        break;
      }
    if (y >= 0) {
      putlog(LOG_BOTS, "*", "Lost binary transfer from %s; aborting.", dcc[y].nick);
      unlink(dcc[idx].u.xfer->filename);
/* Drop that bot 
      dprintf(y, "bye\n");
      simple_snprintf(s, sizeof s,"Disconnected %s (aborted binary transfer)", dcc[y].nick);
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
  }
}

/* Determine byte order. Used for resend DCC startup packets.
 */
static inline uint8_t byte_order_test(void)
{
  uint16_t test = TRANSFER_REGET_PACKETID;

  if (*((uint8_t *)&test) == ((TRANSFER_REGET_PACKETID & 0xff00) >> 8))
    return 0;
  if (*((uint8_t *)&test) == (TRANSFER_REGET_PACKETID & 0x00ff))
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
    putlog(LOG_FILES, "*", "(!) reget packet from %s for %s is invalid!",
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
  unsigned char bbuf[4] = "";
  unsigned long cmp, l;
  int w = len + dcc[idx].u.xfer->sofar, p = 0;

  dcc[idx].timeval = now;		/* Mark as active		*/

  /* Add bytes to our buffer if we don't have a complete response yet.
   * This is either a 4 bit ack or the 8 bit reget packet.
   */
  if (w < 4 ||
      (w < 8 && dcc[idx].u.xfer->type == XFER_RESEND_PEND)) {
    memcpy(&(dcc[idx].u.xfer->buf[dcc[idx].u.xfer->sofar]), buf, len);
    dcc[idx].u.xfer->sofar += len;
    return;
  /* Waiting for the 8 bit reget packet? */
  } else if (dcc[idx].u.xfer->type == XFER_RESEND_PEND) {
    /* The 8 bit packet is complete now. Parse it. */
    if (w == 8) {
      transfer_reget reget_data;

      memcpy(&reget_data, dcc[idx].u.xfer->buf, dcc[idx].u.xfer->sofar);
      memcpy(&reget_data + dcc[idx].u.xfer->sofar, buf, len);
      handle_resend_packet(idx, &reget_data);
      cmp = dcc[idx].u.xfer->offset;
    } else
      return;
    /* Fall through! */
  /* No, only want 4 bit ack responses. */
  } else {
    /* Complete packet? */
    if (w == 4) {
      memcpy(bbuf, dcc[idx].u.xfer->buf, dcc[idx].u.xfer->sofar);
      memcpy(&(bbuf[dcc[idx].u.xfer->sofar]), buf, len);
    } else {
      p = ((w - 1) & ~3) - dcc[idx].u.xfer->sofar;
      w = w - ((w - 1) & ~3);
      if (w < 4) {
	memcpy(dcc[idx].u.xfer->buf, &(buf[p]), w);
	return;
      }
      memcpy(bbuf, &(buf[p]), w);
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
	   "!! Resuming file transfer behind file end for %s to %s",
	   dcc[idx].u.xfer->origname, dcc[idx].nick);
  } else if (cmp > dcc[idx].status) {
    /* Attempt to resume */
    if (!strcmp(dcc[idx].nick, "*users")) {
      putlog(LOG_BOTS, "*", "!!! Trying to skip ahead on userfile transfer");
    } else if (!strcmp(dcc[idx].nick, "*binary")) {
      putlog(LOG_BOTS, "*","!!! Trying to skip ahead on binary transfer");
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
      int x, y = -1;

      for (x = 0; x < dcc_total; x++)
        if (dcc[x].type && (dcc[x].type->flags & DCT_BOT) && (!strcasecmp(dcc[x].nick, dcc[idx].host))) {
	  y = x;
          break;
        }
      if (y >= 0)
	dcc[y].status &= ~STAT_SENDING;

      putlog(LOG_BOTS, "*", "Completed userfile transfer to %s.", dcc[y].nick);
      unlink(dcc[idx].u.xfer->filename);
      /* Any sharebot things that were queued: */
      dump_resync(y);
    } else if (!strcmp(dcc[idx].nick, "*binary")) {
      int x, y = -1;

      for (x = 0; x < dcc_total; x++)
        if (dcc[x].type && (dcc[x].type->flags & DCT_BOT) && (!strcasecmp(dcc[x].nick, dcc[idx].host))) {
	  y = x;
          break;
        }
      if (y >= 0) {
	dcc[y].status &= ~STAT_SENDINGU;
        dcc[y].status |= STAT_UPDATED;
      }
      putlog(LOG_BOTS, "*", "Completed binary file send to %s", dcc[y].nick);
      bupdating = 0;
    }
    lostdcc(idx);
    return;
  }
  /* Note:  No fseek() needed here, because the file position is kept from
   *        the last run.
   */
  l = dcc_block;
  if (l == 0 || dcc[idx].status + l > dcc[idx].u.xfer->length)
    l = dcc[idx].u.xfer->length - dcc[idx].status;
  dcc[idx].u.xfer->block_pending = pump_file_to_sock(dcc[idx].u.xfer->f, dcc[idx].sock, l);
  dcc[idx].status += l;
}

void eof_dcc_get(int idx)
{
  char s[1024] = "";

  fclose(dcc[idx].u.xfer->f);
  if (!strcmp(dcc[idx].nick, "*users")) {
    int x, y = -1;

    for (x = 0; x < dcc_total; x++)
      if (dcc[x].type && (dcc[x].type->flags & DCT_BOT) && (!strcasecmp(dcc[x].nick, dcc[idx].host))) {
	y = x;
        break;
      }
    putlog(LOG_BOTS, "*", "Lost userfile transfer; aborting.");
    /* Note: no need to unlink the xfer file, as it's already unlinked. */
    /* Drop that bot */
    dprintf(-dcc[y].sock, "bye\n");
    simple_snprintf(s, sizeof s, "Disconnected %s (aborted userfile transfer)",
		 dcc[y].nick);
    botnet_send_unlinked(y, dcc[y].nick, s);
    chatout("*** %s\n", s);
    if (y != idx) {
     killsock(dcc[y].sock);
     lostdcc(y);
    }
    if (dcc[idx].sock != -1) 
      killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  } else if (!strcmp(dcc[idx].nick, "*binary")) {
    int x, y = -1;

    for (x = 0; x < dcc_total; x++)
      if (dcc[x].type && (dcc[x].type->flags & DCT_BOT) && (!strcasecmp(dcc[x].nick, dcc[idx].host))) {
	y = x;
        break;
      }
    putlog(LOG_BOTS, "*", "Lost binary transfer; aborting.");
    /* Note: no need to unlink the xfer file, as it's already unlinked. */
    /* Drop that bot */
    dcc[y].status &= ~STAT_SENDINGU;
    bupdating = 0;
/*
    dprintf(-dcc[y].sock, "bye\n");
    simple_snprintf(s, sizeof s, "Disconnected %s (aborted binary transfer)",
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
  }
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

void dcc_send(int idx, char *buf, int len)
{
  char s[SGRAB + 2] = "", *b = NULL;
  size_t siz = 0;
  unsigned long sent;

  if (!fwrite(buf, len, 1, dcc[idx].u.xfer->f)) {
    putlog(LOG_FILES, "*", "Problem writing file");
    fclose(dcc[idx].u.xfer->f);
    siz = strlen(tempdir) + strlen(dcc[idx].u.xfer->filename) + 1;
    b = (char *) calloc(1, siz);
    strlcpy(b, tempdir, siz);
    strlcat(b, dcc[idx].u.xfer->filename, siz);
    unlink(b);
    free(b);
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }

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
    notice(dcc[idx].nick, "Bogus file length.", DP_HELP);
    putlog(LOG_FILES, "*",
	   "File too long: dropping dcc send %s from %s!%s",
	   dcc[idx].u.xfer->origname, dcc[idx].nick, dcc[idx].host);
    fclose(dcc[idx].u.xfer->f);
    siz = strlen(tempdir) + strlen(dcc[idx].u.xfer->filename) + 1;
    b = (char *) calloc(1, siz);
    strlcpy(b, tempdir, siz);
    strlcat(b, dcc[idx].u.xfer->filename, siz);
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
    int x, y = -1;

    for (x = 0; x < dcc_total; x++)
      if (dcc[x].type && (dcc[x].type->flags & DCT_BOT) && (!strcasecmp(dcc[x].nick, dcc[i].host))) {
	y = x;
        break;
      }
    if (y >= 0) {
      dcc[y].status &= ~STAT_SENDING;
      dcc[y].status &= ~STAT_SHARE;
    }
    unlink(dcc[i].u.xfer->filename);
    putlog(LOG_BOTS, "*","Timeout on userfile transfer.");
    //Bot may have already disconnected, hence y = -1
    if (y != -1) {
      dprintf(y, "bye\n");
      simple_snprintf(xx, sizeof xx,"Disconnected %s (timed-out userfile transfer)", dcc[y].nick);
      botnet_send_unlinked(y, dcc[y].nick, xx);
      chatout("*** %s\n", xx);
    }
    if (y < i) {
      int t = y;

      y = i;
      i = t;
    }
    killsock(dcc[y].sock);
    lostdcc(y);
    xx[0] = 0;
  } else if (strcmp(dcc[i].nick, "*binary") == 0) {
    int x, y = -1;

    for (x = 0; x < dcc_total; x++)
      if (dcc[x].type && (dcc[x].type->flags & DCT_BOT) && (!strcasecmp(dcc[x].nick, dcc[i].host))) {
	y = x;
        break;
      }
    if (y >= 0) {
      dcc[y].status &= ~STAT_SENDINGU;
    }
    putlog(LOG_BOTS, "*","Timeout on binary transfer.");
    if (y != -1) {
      dprintf(y, "bye\n");
      simple_snprintf(xx, sizeof xx,"Disconnected %s (timed-out binary transfer)", dcc[y].nick);
      botnet_send_unlinked(y, dcc[y].nick, xx);
      chatout("*** %s\n", xx);
    }
    if (y < i) {
      int t = y;

      y = i;
      i = t;
    }
    killsock(dcc[y].sock);
    lostdcc(y);
    xx[0] = 0;
  }
  if (i != -1) {
    killsock(dcc[i].sock);
    lostdcc(i);
  }
}

void tout_dcc_send(int idx)
{
  fclose(dcc[idx].u.xfer->f);
  if (!strcmp(dcc[idx].nick, "*users")) {
    int x, y = -1;

    for (x = 0; x < dcc_total; x++)
      if (dcc[x].type && (dcc[x].type->flags & DCT_BOT) && (!strcasecmp(dcc[x].nick, dcc[idx].host))) {
	y = x;
        break;
      }
    if (y >= 0) {
      dcc[y].status &= ~STAT_GETTING;
      dcc[y].status &= ~STAT_SHARE;
    }
    unlink(dcc[idx].u.xfer->filename);
    putlog(LOG_BOTS, "*", "Timeout on userfile transfer.");
  } else if (!strcmp(dcc[idx].nick, "*binary")) {
    int x, y = -1;

    for (x = 0; x < dcc_total; x++)
      if (dcc[x].type && (dcc[x].type->flags & DCT_BOT) && (!strcasecmp(dcc[x].nick, dcc[idx].host))) {
	y = x;
        break;
      }
    if (y >= 0) {
      dcc[y].status &= ~STAT_GETTINGU;
    }
    putlog(LOG_BOTS, "*", "Timeout on binary transfer.");
  }
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

void display_dcc_get(int idx, char *buf, size_t bufsiz)
{
  if (dcc[idx].status == dcc[idx].u.xfer->length)
    simple_snprintf(buf, bufsiz, "send  (%lu)/%lu\n    Filename: %s\n", dcc[idx].u.xfer->acked,
	    dcc[idx].u.xfer->length, dcc[idx].u.xfer->origname);
  else
    simple_snprintf(buf, bufsiz, "send  (%lu)/%lu\n    Filename: %s\n", dcc[idx].status,
	    dcc[idx].u.xfer->length, dcc[idx].u.xfer->origname);
}

void display_dcc_get_p(int idx, char *buf, size_t bufsiz)
{
  simple_snprintf(buf, bufsiz, "send  waited %ds\n    Filename: %s\n", (int) (now - dcc[idx].timeval), dcc[idx].u.xfer->origname);
}

void display_dcc_send(int idx, char *buf, size_t bufsiz)
{
  simple_snprintf(buf, bufsiz, "send  (%lu)/%lu\n    Filename: %s\n", dcc[idx].status, dcc[idx].u.xfer->length, dcc[idx].u.xfer->origname);
}

void display_dcc_fork_send(int idx, char *buf, size_t bufsiz)
{
  simple_snprintf(buf, bufsiz, "conn  send");
}

void kill_dcc_xfer(int idx, void *x)
{
  struct xfer_info *p = (struct xfer_info *) x;

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
  if (dcc[idx].type != &DCC_FORK_SEND)
    return;

  char s1[121] = "";

  dcc[idx].type = &DCC_SEND;
  dcc[idx].status = 0;
  dcc[idx].u.xfer->start_time = now;
  simple_snprintf(s1, sizeof s1, "%s!%s", dcc[idx].nick, dcc[idx].host);
  if (strcmp(dcc[idx].nick, "*users") && strcmp(dcc[idx].nick, "*binary"))
    putlog(LOG_MISC, "*", "DCC connection: SEND %s (%s)", dcc[idx].u.xfer->origname, s1);
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
  in_addr_t ip;
  in_port_t port;
  int i;
  char s[UHOSTLEN] = "";

  i = answer(dcc[idx].sock, s, &ip, &port, 1);
  killsock(dcc[idx].sock);
  dcc[idx].sock = i;
  dcc[idx].addr = ip;
  dcc[idx].port = (int) port;
  if (dcc[idx].sock == -1) {
    bd::String msg;
    msg = bd::String::printf("Bad connection (%s)", strerror(errno));
    notice(dcc[idx].nick, msg, DP_HELP);
    putlog(LOG_FILES, "*", "DCC bad connection: GET %s (%s!%s)",
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

static int raw_dcc_resend_send(char *filename, char *nick, char *from, int resend, int *idx)
{
  int zz = -1;
  int i;
  in_port_t port;
  char *buf = NULL;
  long dccfilesize;
  FILE *f = NULL, *dccfile = NULL;
 
  sdprintf("raw_dcc_resend_send()");
  dccfile = fopen(filename, "rb");
  if (!dccfile) {
    putlog(LOG_MISC, "*", "Failed to open %s: %s", filename, strerror(errno));
    return DCCSEND_FEMPTY;
  }
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
  
  if ((i = new_dcc(&DCC_GET_PENDING, sizeof(struct xfer_info))) == -1)
     return DCCSEND_FULL;
  f = fopen(filename, "rb");
  if (!f)
    return DCCSEND_BADFN;
  dcc[i].sock = zz;
  dcc[i].addr = (in_addr_t) (-559026163);
  dcc[i].port = port;
  strlcpy(dcc[i].nick, nick, sizeof(dcc[i].nick));
  strlcpy(dcc[i].host, "irc", sizeof(dcc[i].host));
  dcc[i].u.xfer->filename = strdup(filename);
  dcc[i].u.xfer->origname = strdup(filename);
  strlcpy(dcc[i].u.xfer->from, from, NICKLEN);
  dcc[i].u.xfer->length = dccfilesize;
  dcc[i].timeval = now;
  dcc[i].u.xfer->f = f;
  dcc[i].u.xfer->type = resend ? XFER_RESEND_PEND : XFER_SEND;

  if (buf)
    free(buf);

  if (idx)
    *idx = i;
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
int raw_dcc_send(char *filename, char *nick, char *from, int *idx)
{
  return raw_dcc_resend_send(filename, nick, from, 0, idx);
}

/*
 *   Module functions
 */

void transfer_report(int idx, int details)
{
  if (details) {
    dprintf(idx,"    DCC block is %d%s, max concurrent d/ls is %d\n",
	    dcc_block, (dcc_block == 0) ? " (turbo dcc)" : "", dcc_limit);
  }
}

void transfer_init()
{
}
/* vim: set sts=2 sw=2 ts=8 et: */
