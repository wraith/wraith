/*
 * transfer.h -- part of transfer.mod
 *
 */

#ifndef _EGG_MOD_TRANSFER_TRANSFER_H
#define _EGG_MOD_TRANSFER_TRANSFER_H

enum dccsend_types {
  DCCSEND_OK = 0,
  DCCSEND_FULL,		/* DCC table is full			*/
  DCCSEND_NOSOCK,	/* Can not open a listening socket	*/
  DCCSEND_BADFN,	/* No such file				*/
  DCCSEND_FEMPTY	/* File is empty			*/
};

enum {                          /* transfer connection handling a ...   */
        XFER_SEND,              /*  ... normal file-send to s.o.        */
        XFER_RESEND,            /*  ... file-resend to s.o.             */
        XFER_RESEND_PEND,       /*  ... (as above) and waiting for info */
        XFER_RESUME,            /*  ... file-send-resume to s.o.        */
        XFER_RESUME_PEND,       /*  ... (as above) and waiting for conn */
        XFER_GET                /*  ... file-get from s.o.              */
};

enum {
        XFER_ACK_UNKNOWN,       /* We don't know how blocks are acked.  */
        XFER_ACK_WITH_OFFSET,   /* Skipped data is also counted as
                                   received.                            */
        XFER_ACK_WITHOUT_OFFSET /* Skipped data is NOT counted in ack.  */
};

int raw_dcc_send(char *, char *, char *, int *);

#ifdef MAKING_TRANSFER
#define TRANSFER_REGET_PACKETID 0xfeab

typedef struct {
  uint32_t byte_offset;	/* Number of bytes to skip relative to
				   the file beginning			*/
  uint16_t packet_id;		/* Identification ID, should be equal
	 			   to TRANSFER_REGET_PACKETID		*/
  uint8_t  byte_order;		/* Byte ordering, see byte_order_test()	*/
} transfer_reget;
#endif				/* MAKING_TRANSFER */

extern struct dcc_table 		DCC_SEND, DCC_FORK_SEND;
extern struct dcc_table			DCC_GET, DCC_GET_PENDING;

#endif				/* _EGG_MOD_TRANSFER_TRANSFER_H */
