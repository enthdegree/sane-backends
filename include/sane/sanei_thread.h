/* sane - Scanner Access Now Easy.
   Copyright (C) 2000 Yuri Dario
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

   This file declares a _proposed_ internal SANE interface.  It was
   proposed 2000-02-19 by Yuri Dario to wrap UNIX functions fork(),
   kill(), waitpid() and wait(), which are missing on OS/2.
*/
   
#ifndef sanei_thread_h
#define sanei_thread_h

extern int sanei_thread_begin( void (*start)(void *arg), 
                               void *arg_list);
extern int sanei_thread_kill( int pid, int sig);
extern int sanei_thread_waitpid( int pid, int *stat_loc, int options);
extern int sanei_thread_wait( int *stat_loc);

#endif /* sanei_thread_h */
