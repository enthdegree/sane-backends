/* sane - Scanner Access Now Easy.
   Copyright (C) 1998-2001 Yuri Dario
   This file is part of the SANE package.

   SANE is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License, or (at your
   option) any later version.

   SANE is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with sane; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

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

/** @file sanei_thread.h
 * Header file to make porting fork, wait, and waitpid easier.
 *
 * This file contains helper functions for the OS/2 port (using threads
 * instead of forked processes). <b>Do not use these functions in the backends
 * directly</b>, they are used automatically by macros. Just include this file
 * in an \#ifdef HAVE_OS2_H statement if you use fork. If your reader process
 * has more than one argument, you also need to implement a
 * os2_reader_process() in your backend. The implementation should call your
 * own reader process. See mustek.c for an example.
 *    
 * The preprocessor is used for routing process-related function to OS/2
 * threaded code: in this way, Unix backends requires only minimal code
 * changes.
 *
 * @sa sanei.h sanei_backend.h
 */

  
#ifndef sanei_thread_h
#define sanei_thread_h
#include "../include/sane/config.h"

/** @name Internal functions 
 * @{
 */
/** <b>Do not use in backends</b>
 *
 * Wrapper for @c fork. 
 */
extern int sanei_thread_begin( void (*start)(void *arg), void* arg_list);

/** <b>Do not use in backends</b>
 *
 * Wrapper for @c kill. 
 */
extern int sanei_thread_kill( int pid, int sig);

/** <b>Do not use in backends</b>
 *
 * Wrapper for @c waitpid. 
 */
extern int sanei_thread_waitpid( int pid, int *stat_loc, int options);

/** <b>Do not use in backends</b>
 *
 * Wrapper for @c wait. 
 */
extern int sanei_thread_wait( int *stat_loc);
/* @} */

/** Reader process function.
 *
 * This wrapper is necessary if a backend's reader process need more than one
 * argument. Add a function to you backend with this name and let it call your
 * own reader process. See mustek.c for an example.
 */
static void os2_reader_process( void* data);

#ifdef HAVE_OS2_H
#define fork() 			sanei_thread_begin( os2_reader_process)
#define kill( a, b)		sanei_thread_kill( a,b)
#define waitpid( a, b, c)	sanei_thread_waitpid( a, b, c)
#endif

#endif /* sanei_thread_h */
