/* sane - Scanner Access Now Easy.
   Copyright (C) 1998-2001 Yuri Dario
   Copyright (C) 2003 Gerhard Jaeger
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

   Helper functions for the OS/2 port (using threads instead of forked
   processes). Don't use them in the backends, they are used automatically by
   macros.

   Added pthread support (as found in hp-handle.c) - Gerhard Jaeger
*/

#include "../include/sane/config.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#define BACKEND_NAME sanei_thread      /**< name of this module for debugging */

#include "../include/sane/sane.h"
#include "../include/sane/sanei_debug.h"
#include "../include/sane/sanei_thread.h"

#ifndef _VAR_NOT_USED
# define _VAR_NOT_USED(x)	((x)=(x))
#endif
      
/** for init issues - here only for the debug output
 */
void
sanei_thread_init( void )
{
	DBG_INIT();
}

#ifdef HAVE_OS2_H

#define INCL_DOSPROCESS
#include <os2.h>

/*
 * starts a new thread or process
 * parameters:
 * star  address of reader function
 * args  pointer to scanner data structure
 *
*/
int
sanei_thread_begin( void (*start)(void *arg), void* args )
{
   return _beginthread( start, NULL, 1024*1024, args );
}

int
sanei_thread_kill( int pid, int sig)
{
   return DosKillThread( pid);
}

int
sanei_thread_waitpid( int pid, int *stat_loc, int options)
{
  if (stat_loc)
    *stat_loc = 0;
  return pid; /* DosWaitThread( (TID*) &pid, DCWW_WAIT);*/
}

int
sanei_thread_wait( int *stat_loc)
{
   *stat_loc = 0;
   return -1;  /* return error because I don't know child pid */
}

#else /* HAVE_OS2_H */

#ifdef USE_PTHREAD
# include <pthread.h>
#else
# include <sys/wait.h>
#endif

int
sanei_thread_begin( void (*start)(void *arg), void* args )
{
	int       pid;
#ifdef USE_PTHREAD
	pthread_t thread;

	pid = pthread_create( &thread, NULL, start, args );

	if ( pid != 0 ) {
		DBG( 1, "pthread_create() failed with %d\n", pid );
		return -1;
	}

	DBG( 2, "pthread_create() created thread %d\n", (int)thread );
	return (int)thread;
#else
	pid = fork();

	if( pid < 0 ) {
		DBG( 1, "fork() failed\n" );
		return -1;
	}

	if( pid == 0 ) {

    	/* run in child context... */
	    int status = 0;
	    
        start( args );
		
		/* don't use exit() since that would run the atexit() handlers */
		_exit( status );
	}

	/* parents return */
	return pid;
#endif
}

int
sanei_thread_kill( int pid, int sig)
{
	DBG(2, "sanei_thread_kill() will kill %d\n", (int)pid);
#ifdef USE_PTHREAD
	return pthread_kill((pthread_t)pid, sig );
#else
	return kill( pid, sig );
#endif
}

int
sanei_thread_waitpid( int pid, int *stat_loc, int options )
{
	if (stat_loc)
		*stat_loc = 0;

#ifdef USE_PTHREAD

	_VAR_NOT_USED( options );

	if( 0 == pthread_join((pthread_t)pid, (void*)stat_loc ))
		pthread_detach((pthread_t)pid );
	return pid;
#else
	return waitpid( pid, stat_loc, options );
#endif
}

SANE_Bool
sanei_thread_is_forked( void )
{
#ifdef USE_PTHREAD
	return SANE_FALSE;
#else
	return SANE_TRUE;
#endif
}

#endif /* HAVE_OS2_H */

