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

   Helper functions for the OS/2 port (using threads instead of forked
   processes). Don't use them in the backends, they are used automatically by
   macros.
*/
   
#ifndef sanei_thread_h
#define sanei_thread_h
#include "../include/sane/config.h"

extern int sanei_thread_begin( void (*start)(void *arg), void* arg_list);
extern int sanei_thread_kill( int pid, int sig);
extern int sanei_thread_waitpid( int pid, int *stat_loc, int options);
extern int sanei_thread_wait( int *stat_loc);

static void os2_reader_process( void* data);

#ifdef HAVE_OS2_H
   /*
      use preprocessor for routing process-related function to
      OS/2 threaded code: in this way, Unix backends requires only
      minimal code changes.
   */
#define fork() 			sanei_thread_begin( os2_reader_process)
#define kill( a, b)		sanei_thread_kill( a,b)
#define waitpid( a, b, c)	sanei_thread_waitpid( a, b, c)
#endif

#endif /* sanei_thread_h */
