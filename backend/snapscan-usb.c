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

  As a special exception, the authors of SANE give permission for
  additional uses of the libraries contained in this release of SANE.

  The exception is that, if you link a SANE library with other files
  to produce an executable, this does not by itself cause the
  resulting executable to be covered by the GNU General Public
  License.  Your use of that executable is in no way restricted on
  account of linking the SANE library code into it.

  This exception does not, however, invalidate any other reasons why
  the executable file might be covered by the GNU General Public
  License.

  If you submit changes to SANE to the maintainers to be included in
  a subsequent release, you agree by submitting the changes that
  those changes may be distributed with this exception intact.

  If you write modifications of your own for SANE, it is your choice
  whether to permit this exception to apply to your modifications.
  If you do not wish that, delete this exception notice.

  This file implements USB equivalents to the SCSI routines used by the Snapscan
  backend.
  
  History

  0.1        2000-02-01

     First version released

  0.2   2000-02-12

     The Send Diagnostics SCSI command seems to hang some 1212U scanners.
     Bypassing this command fixes the problem. This bug was reported by
     Dmitri (dmitri@advantrix.com).

  0.3   2000-02-13

     The "Set window" command returns with status "Device busy" when the
     scanner is busy. One consequence is that some frontends exits with an
     error message if it's started when the scanner is warming up.
     A solution was suggested by Dmitri (dmitri@advantrix.com)
     The idea is that a SCSI command which returns "device busy" is stored
     in a "TODO" queue. The send command function is modified to first send
     commands in the queue before the intended command.
     So far this strategy has worked flawlessly. Thanks Dmitri!
*/

/* $Id$
   SnapScan backend scan data sources */

#include <sys/ipc.h>
#include <sys/sem.h>

#include "snapscan-usb.h"

/* check for union semun */
#if defined(HAVE_UNION_SEMUN)
/* union semun is defined by including <sys/sem.h> */
#else
/* according to X/OPEN we have to define it ourselves */
union semun {
   int val;                    /* value for SETVAL */
   struct semid_ds *buf;       /* buffer for IPC_STAT, IPC_SET */
   unsigned short int *array;  /* array for GETALL, SETALL */
   struct seminfo *__buf;      /* buffer for IPC_INFO */
};
#endif

/* Global variables */

static int sem_id;
static struct sembuf sem_wait = { 0, -1, 0 };
static struct sembuf sem_signal = { 0, 1, 0 };
static sense_handler_type usb_sense_handler;
static void* usb_pss;

/* Forward declarations */
static SANE_Status usb_request_sense(SnapScan_Scanner *pss);

static SANE_Status snapscani_usb_cmd(int fd, const void *src, size_t src_size,
                    void *dst, size_t * dst_size)
{
    static const char me[] = "snapscani_usb_cmd";
    int status;

    DBG (DL_CALL_TRACE, "%s(%d,0x%x,%d,0x%x,0x%x (%d))\n", me,
         fd,(int)src,src_size,(int)dst,(int)dst_size,dst_size ? *dst_size : 0);

    while(bqhead) {
        status = atomic_usb_cmd(fd, bqhead->src, bqhead->src_size, NULL, NULL);
        if(status == SANE_STATUS_DEVICE_BUSY) {
            if(is_queueable(src)) {
                enqueue_bq(fd,src,src_size);
                return SANE_STATUS_GOOD;
            } else {
                sleep(1);
                continue;
            }
        }
        dequeue_bq();
    }

    status = atomic_usb_cmd(fd,src,src_size,dst,dst_size);

    if ((status == SANE_STATUS_DEVICE_BUSY) && is_queueable(src) ) {
        enqueue_bq(fd,src,src_size);
        return SANE_STATUS_GOOD;
    }

    return status;
}

static SANE_Status atomic_usb_cmd(int fd, const void *src, size_t src_size,
                    void *dst, size_t * dst_size)
{
    static const char me[] = "atomic_usb_cmd";

    int status;
    sigset_t all,oldset;

    DBG (DL_CALL_TRACE, "%s(%d,0x%x,%d,0x%x,0x%x (%d))\n", me,
         fd,(int)src,src_size,(int)dst,(int)dst_size,dst_size ? *dst_size : 0);

    /* Prevent the calling process from being killed */
    sigfillset(&all);
    sigprocmask(SIG_BLOCK, &all, &oldset);

    /* Make sure we are alone */
    semop(sem_id, &sem_wait, 1);

    status = usb_cmd(fd,src,src_size,dst,dst_size);

    semop(sem_id, &sem_signal, 1);

    /* Now it is ok to be killed */
    sigprocmask(SIG_SETMASK, &oldset, NULL);

    return status;

}

static SANE_Status snapscani_usb_open(const char *dev, int *fdp,
    sense_handler_type sense_handler, void* pss)
{
    static const char me[] = "snapscani_usb_open";

    DBG (DL_CALL_TRACE, "%s(%s)\n", me, dev);

    if((sem_id = semget( ftok(dev,0x12), 1, IPC_CREAT | 0660 )) == -1) {
        DBG (DL_MAJOR_ERROR, "%s: Can't get semaphore\n", me);
        return SANE_STATUS_INVAL;
    }
    semop(sem_id, &sem_signal, 1);
    usb_sense_handler=sense_handler;
    usb_pss = pss;
    return sanei_usb_open(dev, fdp);
}


static void snapscani_usb_close(int fd) {
    static const char me[] = "snapscani_usb_close";
    static union semun dummy_semun_arg;

    DBG (DL_CALL_TRACE, "%s(%d)\n", me, fd);
    semctl(sem_id, 0, IPC_RMID, dummy_semun_arg);
    sanei_usb_close(fd);
}


static int usb_cmdlen(int cmd)
{
    switch(cmd) {
    case TEST_UNIT_READY:
    case INQUIRY:
    case SCAN:
    case REQUEST_SENSE:
    case RESERVE_UNIT:
    case RELEASE_UNIT:
    case SEND_DIAGNOSTIC:
        return 6;
    case SEND:
    case SET_WINDOW:
    case READ:
    case GET_DATA_BUFFER_STATUS:
        return 10;
    }
    return 0;
}

static char *usb_debug_data(char *str,const char *data, int len) {
    char tmpstr[10];
    int i;

    str[0]=0;
    for(i=0; i < (len < 10 ? len : 10); i++) {
        sprintf(tmpstr," 0x%02x",((int)data[i]) & 0xff);
        if(i%16 == 0 && i != 0)
            strcat(str,"\n");
        strcat(str,tmpstr);
    }
    if(i < len)
        strcat(str," ...");
    return str;
}

#define RETURN_ON_FAILURE(x) if((status = x) != SANE_STATUS_GOOD) return status;

static SANE_Status usb_write(int fd, const void *buf, size_t n) {
    char dbgmsg[16384];
    SANE_Status status;
    size_t bytes_written = n;

    static const char me[] = "usb_write";
    DBG(DL_DATA_TRACE, "%s: writing: %s\n",me,usb_debug_data(dbgmsg,buf,n));

    status = sanei_usb_write_bulk(fd, (const SANE_Byte*)buf, &bytes_written);
    if(bytes_written != n) {
        DBG (DL_MAJOR_ERROR, "%s Only %d bytes written\n",me,bytes_written);
        status = SANE_STATUS_IO_ERROR;
    }
    return status;
}

static SANE_Status usb_read(SANE_Int fd, void *buf, size_t n) {
    char dbgmsg[16384];
    static const char me[] = "usb_read";
    SANE_Status status;
    size_t bytes_read = n;

    status = sanei_usb_read_bulk(fd, (SANE_Byte*)buf, &bytes_read);
    if (bytes_read != n) {
        DBG (DL_MAJOR_ERROR, "%s Only %d bytes read\n",me,bytes_read);
        status = SANE_STATUS_IO_ERROR;
    }

    DBG(DL_DATA_TRACE, "%s: reading: %s\n",me,usb_debug_data(dbgmsg,buf,n));
    return status;
}

static SANE_Status usb_read_status(int fd, int *scsistatus, int *transaction_status)
{
    static const char me[] = "usb_read_status";
    unsigned char status_buf[8];
    int scsistat;
    int status;

    RETURN_ON_FAILURE(usb_read(fd,status_buf,8));

    if(transaction_status)
        *transaction_status = status_buf[0];

    scsistat = (status_buf[1] & STATUS_MASK) >> 1;

    if(scsistatus)
        *scsistatus = scsistat;

    switch(scsistat) {
    case GOOD:
        return SANE_STATUS_GOOD;
    case CHECK_CONDITION:
        if (usb_pss != NULL) {
            return usb_request_sense(usb_pss);
        } else {
            DBG (DL_MAJOR_ERROR, "%s: scanner structure not set, returning default error\n",
                me);
            return SANE_STATUS_DEVICE_BUSY;
        }
        break;
    case BUSY:
        return SANE_STATUS_DEVICE_BUSY;
    default:
        return SANE_STATUS_IO_ERROR;
    }
}


static SANE_Status usb_cmd(int fd, const void *src, size_t src_size,
                    void *dst, size_t * dst_size)
{
  static const char me[] = "usb_cmd";
  int status,tstatus;
  int cmdlen,datalen;

  DBG (DL_CALL_TRACE, "%s(%d,0x%x,%d,0x%x,0x%x (%d))\n", me,
       fd,(int)src,src_size,(int)dst,(int)dst_size,dst_size ? *dst_size : 0);


  /* Since the  "Send Diagnostic" command isn't supported by
     all Snapscan USB-scanners it's disabled .
  */
  if(((const char *)src)[0] == SEND_DIAGNOSTIC)
      return(SANE_STATUS_GOOD);

  cmdlen = usb_cmdlen(*((const char *)src));
  datalen = src_size - cmdlen;

  DBG(DL_DATA_TRACE, "%s: cmdlen=%d, datalen=%d\n",me,cmdlen,datalen);

  /* Send command to scanner */
  RETURN_ON_FAILURE( usb_write(fd,src,cmdlen) );

  /* Read status */
  RETURN_ON_FAILURE( usb_read_status(fd, NULL, &tstatus) );

  /* Send data only if the scanner is expecting it */
  if(datalen > 0 && (tstatus == TRANSACTION_WRITE)) {
      /* Send data to scanner */
      RETURN_ON_FAILURE( usb_write(fd, ((const SANE_Byte *) src) + cmdlen, datalen) );

      /* Read status */
      RETURN_ON_FAILURE( usb_read_status(fd, NULL, &tstatus) );
  }

  /* Receive data only when new data is waiting */
  if(dst_size && *dst_size && (tstatus == TRANSACTION_READ)) {
      RETURN_ON_FAILURE( usb_read(fd,dst,*dst_size) );

      /* Read status */
      RETURN_ON_FAILURE( usb_read_status(fd, NULL, &tstatus) );
  }

  if(tstatus != TRANSACTION_COMPLETED) {
      if(tstatus == TRANSACTION_WRITE)
          DBG(DL_MAJOR_ERROR,
              "%s: The transaction should now be completed, but the scanner is expecting more data" ,me);
      else
          DBG(DL_MAJOR_ERROR,
              "%s: The transaction should now be completed, but the scanner has more data to send" ,me);
      return SANE_STATUS_IO_ERROR;
  }

  return status;
}

/* Busy queue data structures and function implementations*/

static int is_queueable(const char *src)
{
    switch(src[0]) {
    case SEND:
    case SET_WINDOW:
    case SEND_DIAGNOSTIC:
        return 1;
    default:
        return 0;
    }
}

static struct usb_busy_queue *bqhead=NULL,*bqtail=NULL;
static int bqelements=0;

static int enqueue_bq(int fd,const void *src, size_t src_size)
{
    static const char me[] = "enqueue_bq";
    struct usb_busy_queue *bqe;

    DBG (DL_CALL_TRACE, "%s(%d,%p,%d)\n", me, fd,src,src_size);

    if((bqe = malloc(sizeof(struct usb_busy_queue))) == NULL)
        return -1;

    if((bqe->src = malloc(src_size)) == NULL)
        return -1;

    memcpy(bqe->src,src,src_size);
    bqe->src_size=src_size;

    bqe->next=NULL;

    if(bqtail) {
        bqtail->next=bqe;
        bqtail = bqe;
    } else
        bqhead = bqtail = bqe;

    bqelements++;
    DBG(DL_DATA_TRACE, "%s: Busy queue: elements=%d, bqhead=%p, bqtail=%p\n",
        me,bqelements,(void*)bqhead,(void*)bqtail);
    return 0;
}

static void dequeue_bq()
{
    static const char me[] = "dequeue_bq";
    struct usb_busy_queue *tbqe;

    DBG (DL_CALL_TRACE, "%s()\n", me);

    if(!bqhead)
        return;

    tbqe = bqhead;
    bqhead = bqhead->next;
    if(!bqhead)
        bqtail=NULL;

    if(tbqe->src)
        free(tbqe->src);
    free(tbqe);

    bqelements--;
    DBG(DL_DATA_TRACE, "%s: Busy queue: elements=%d, bqhead=%p, bqtail=%p\n",
        me,bqelements,(void*)bqhead,(void*)bqtail);
}

static SANE_Status usb_request_sense(SnapScan_Scanner *pss) {
    static const char *me = "usb_request_sense";
    size_t read_bytes = 0;
    u_char cmd[] = {REQUEST_SENSE, 0, 0, 0, 20, 0};
    u_char data[20];
    SANE_Status status;

    read_bytes = 20;

    DBG (DL_CALL_TRACE, "%s\n", me);
    status = usb_cmd (pss->fd, cmd, sizeof (cmd), data, &read_bytes);
    if (status != SANE_STATUS_GOOD)
    {
        DBG (DL_MAJOR_ERROR, "%s: usb command error: %s\n",
             me, sane_strstatus (status));
    }
    else
    {
        if (usb_sense_handler) {
            status = usb_sense_handler (pss->fd, data, (void *) pss);
        } else {
            DBG (DL_MAJOR_ERROR, "%s: No sense handler for USB\n", me);
            status = SANE_STATUS_UNSUPPORTED;
        }
    }
    return status;
}

/*
 * $Log$
 * Revision 1.13  2003/11/08 09:50:27  oliver-guest
 * Fix TPO scanning range for Epson 1670
 *
 * Revision 1.12  2003/07/26 17:16:55  oliverschwartz
 * Changed licence to GPL + SANE exception for snapscan-usb.[ch]
 *
 * Revision 1.11  2002/07/12 23:29:06  oliverschwartz
 * SnapScan backend 1.4.15
 *
 * Revision 1.21  2002/07/12 22:52:42  oliverschwartz
 * use sanei_usb_read_bulk() and sanei_usb_write_bulk()
 *
 * Revision 1.20  2002/04/27 14:36:25  oliverschwartz
 * Pass a char as 'proj' argument for ftok()
 *
 * Revision 1.19  2002/04/10 21:00:33  oliverschwartz
 * Make bqelements static
 *
 * Revision 1.18  2002/03/24 12:16:09  oliverschwartz
 * Better error report in usb_read
 *
 * Revision 1.17  2002/02/05 19:32:39  oliverschwartz
 * Only define union semun if not already defined in <sys/sem.h>. Fixes
 * compilation bugs on Irix and FreeBSD. Fixed by Henning Meier-Geinitz.
 *
 * Revision 1.16  2002/01/14 21:11:56  oliverschwartz
 * Add workaround for bug semctl() call in libc for PPC
 *
 * Revision 1.15  2001/12/09 23:06:44  oliverschwartz
 * - use sense handler for USB if scanner reports CHECK_CONDITION
 *
 * Revision 1.14  2001/11/16 20:23:16  oliverschwartz
 * Merge with sane-1.0.6
 *   - Check USB vendor IDs to avoid hanging scanners
 *   - fix bug in dither matrix computation
 *
 * Revision 1.13  2001/10/09 22:34:23  oliverschwartz
 * fix compiler warnings
 *
 * Revision 1.12  2001/09/18 15:01:07  oliverschwartz
 * - Read scanner id string again after firmware upload
 *   to indentify correct model
 * - Make firmware upload work for AGFA scanners
 * - Change copyright notice
 *
 * */

