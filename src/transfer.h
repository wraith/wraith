
/* 
 * transfer.h -- part of transfer.mod
 * 
 * $Id: transfer.h,v 1.6 2000/01/08 21:23:17 per Exp $
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

#ifndef _EGG_MOD_TRANSFER_TRANSFER_H
#define _EGG_MOD_TRANSFER_TRANSFER_H

#define DCCSEND_OK     0
#define DCCSEND_FULL   1	/* dcc table is full */
#define DCCSEND_NOSOCK 2	/* can't open a listening socket */
#define DCCSEND_BADFN  3	/* no such file */
#define MAX_FILENAME_LENGTH 40	/* max file lengh */

int raw_dcc_send(char *, char *, char *, char *);

#endif /* _EGG_MOD_TRANSFER_TRANSFER_H */
