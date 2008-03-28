/*
  Mutex implementation for SnapScan backend
 
  Copyright (C) 2000, 2004 Henrik Johansson, Oliver Schwartz
 
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
  If you do not wish that, delete this exception notice.*/
  
#if defined __BEOS__

#include <OS.h>
#define snapscan_mutex_t sem_id

static int snapscani_mutex_open(snapscan_mutex_t* a_sem, const char* dev UNUSEDARG)
{
    *a_sem = create_sem(1, "snapscan_mutex");
    return 1;
}

static void snapscani_mutex_close(snapscan_mutex_t* a_sem)
{
    delete_sem(*a_sem);
}

static void snapscani_mutex_lock(snapscan_mutex_t* a_sem)
{
    acquire_sem(*a_sem);
}

static void snapscani_mutex_unlock(snapscan_mutex_t* a_sem)
{
    release_sem(*a_sem);
}



#elif defined USE_PTHREAD || defined HAVE_OS2_H

#include <pthread.h>
#define snapscan_mutex_t pthread_mutex_t

static int snapscani_mutex_open(snapscan_mutex_t* sem_id, const char* dev UNUSEDARG)
{
    pthread_mutex_init(sem_id, NULL);
    return 1;
}

static void snapscani_mutex_close(snapscan_mutex_t* sem_id)
{
    pthread_mutex_destroy(sem_id);
}

static void snapscani_mutex_lock(snapscan_mutex_t* sem_id)
{
    pthread_mutex_lock(sem_id);
}

static void snapscani_mutex_unlock(snapscan_mutex_t* sem_id)
{
    pthread_mutex_unlock(sem_id);
}

#else /* defined USE_PTHREAD || defined HAVE_OS2_H */

#include <sys/ipc.h>
#include <sys/sem.h>

#define snapscan_mutex_t int

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
#endif /* defined HAVE_UNION_SEMUN */

static struct sembuf sem_wait = { 0, -1, 0 };
static struct sembuf sem_signal = { 0, 1, 0 };

static int snapscani_mutex_open(snapscan_mutex_t* sem_id, const char* dev)
{
    static const char *me = "snapscani_mutex_open";
    key_t ipc_key;
    int pid, devnum, busnum;

    if (strstr(dev, "libusb:") == dev)
    {
        if (sanei_usb_get_vendor_product_byname(dev, NULL, &pid) != SANE_STATUS_GOOD)
	{
	    DBG (DL_MAJOR_ERROR, "%s: could not obtain USB product ID for device %s\n", me, dev);
	    return 0;
	}

	if (sscanf(dev, "libusb:%d:%d", &busnum, &devnum) != 2)
	{
	    DBG (DL_MAJOR_ERROR, "%s: could not parse device string: %s\n", me, strerror(errno));
	    return 0;
	}

	ipc_key = pid << 16;
	ipc_key |= (busnum & 0xff) << 8;
	ipc_key |= (devnum & 0xff);

	DBG (DL_INFO, "%s: using IPC key 0x%08x for device %s (pid 0x%04x, bus 0x%02x, dev 0x%02x)\n",
	     me, ipc_key, dev, pid, busnum, devnum);
    }
    else
    {
      ipc_key = ftok(dev, 0x12);

	if (ipc_key == -1)
	{
	  DBG (DL_MAJOR_ERROR, "%s: could not obtain IPC key for device %s: %s\n", me, dev, strerror(errno));
	    return 0;
	}
    }

    *sem_id = semget( ipc_key, 1, IPC_CREAT | 0660 );
    if (*sem_id == -1)
    {
        DBG (DL_MAJOR_ERROR, "%s: semget failed: %s\n", me, strerror(errno));
	return 0;
    }

    semop(*sem_id, &sem_signal, 1);
    return 1;
}

static void snapscani_mutex_close(snapscan_mutex_t* sem_id)
{
    static union semun dummy_semun_arg;
    semctl(*sem_id, 0, IPC_RMID, dummy_semun_arg);
}

static void snapscani_mutex_lock(snapscan_mutex_t* sem_id)
{
    semop(*sem_id, &sem_wait, 1);
}

static void snapscani_mutex_unlock(snapscan_mutex_t* sem_id)
{
    semop(*sem_id, &sem_signal, 1);
}

#endif /* defined USE_PTHREAD || defined HAVE_OS2_H */
