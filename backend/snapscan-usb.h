/*
  Snapscan 1212U modifications for the Snapscan SANE backend

  Copyright (C) 2000 Henrik Johansson

  Henrik Johansson (henrikjo@post.urfors.se)

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston,
  MA 02111-1307, USA.

  This file implements USB equivalents to the SCSI routines used by the Snapscan
  backend.
*/

/* $Id$
   SnapScan backend scan data sources */

#ifndef snapscan_usb_h
#define snapscan_usb_h

static SANE_Status snapscani_usb_cmd(int fd, const void *src, size_t src_size,
                         void *dst, size_t * dst_size);
static SANE_Status snapscani_usb_open(const char *dev, int *fdp);
static void snapscani_usb_close(int fd);

/*
 *  USB status codes
 */
#define GOOD                 0x00
#define CHECK_CONDITION      0x01
#define CONDITION_GOOD       0x02
#define BUSY                 0x04
#define INTERMEDIATE_GOOD    0x08
#define INTERMEDIATE_C_GOOD  0x0a
#define RESERVATION_CONFLICT 0x0c
#define COMMAND_TERMINATED   0x11
#define QUEUE_FULL           0x14

#define STATUS_MASK          0x3e

/*
 *  USB transaction status
 */
#define TRANSACTION_COMPLETED 0xfb   /* Scanner considers the transaction done */
#define TRANSACTION_READ 0xf9        /* Scanner has data to deliver */
#define TRANSACTION_WRITE 0xf8       /* Scanner is expecting more data */

/*
 *  Busy queue data structure and prototypes
 */
struct usb_busy_queue {
    int fd;
    void *src;
    size_t src_size;
    struct usb_busy_queue *next;
};

static struct usb_busy_queue *bqhead,*bqtail;
extern int bqelements;
static int enqueue_bq(int fd,const void *src, size_t src_size);
static void dequeue_bq(void);
static int is_queueable(const char *src);

static SANE_Status atomic_usb_cmd(int fd, const void *src, size_t src_size,
                    void *dst, size_t * dst_size);
static SANE_Status usb_open(const char *dev, int *fdp);

static void usb_close(int fd);

static SANE_Status usb_cmd(int fd, const void *src, size_t src_size,
                    void *dst, size_t * dst_size);

#endif

/*
 * $Log$
 * Revision 1.2  2001/10/09 09:45:15  oliverschwartz
 * update snapscan to snapshot 20011008
 *
 * Revision 1.8  2001/09/18 15:01:07  oliverschwartz
 * - Read scanner id string again after firmware upload
 *   to indentify correct model
 * - Make firmware upload work for AGFA scanners
 * - Change copyright notice
 *
 * */
