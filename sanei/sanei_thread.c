/* sane - Scanner Access Now Easy.
   Copyright (C) 2000 Yuri Dario
   This file is part of the SANE package.

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
*/

#include "sane/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_OS2_H
#define INCL_DOSPROCESS
#include <os2.h>
#endif

#include "sane/sanei_thread.h"

/*
 * starts a new thread or process
 * parameters:
 * start        address of reader function
 * arg_list     pointer to scanner data structure
 *
*/
int
sanei_thread_begin( void (*start)(void *arg),
                    void *arg_list)
{
#ifdef HAVE_OS2_H
   return _beginthread( start, NULL, 64*1024, arg_list);
#else
   return fork();
#endif
}

int
sanei_thread_kill( int pid, int sig)
{
#ifdef HAVE_OS2_H
   return DosKillThread( pid);
#else
   return kill( pid, sig);
#endif
}

int
sanei_thread_waitpid( int pid, int *stat_loc, int options)
{
#ifdef HAVE_OS2_H
   return pid; /* DosWaitThread( (TID*) &pid, DCWW_WAIT);*/
#else
   return waitpid( pid, stat_loc, options);
#endif
}

int
sanei_thread_wait( int *stat_loc)
{
#ifdef HAVE_OS2_H
   *stat_loc = 0;
   return -1;  /* return error because I don't know child pid */
#else
   return wait( stat_loc);
#endif
}

