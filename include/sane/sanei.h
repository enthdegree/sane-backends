/* sane - Scanner Access Now Easy.
   Copyright (C) 1996 David Mosberger-Tang and Andreas Beck
   This file is part of the SANE package.

   SANE is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   SANE is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with sane; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   This file declares SANE internal routines that are provided to
   simplify backend implementation.  */

#ifndef sanei_h
#define sanei_h

#include <sane/sane.h>

/* A few convenience macros:  */

#define STRINGIFY1(x)	#x
#define STRINGIFY(x)	STRINGIFY1(x)

#define PASTE1(x,y)	x##y
#define PASTE(x,y)	PASTE1(x,y)

#define NELEMS(a)	((int)(sizeof (a) / sizeof (a[0])))

extern SANE_Status sanei_constrain_value (const SANE_Option_Descriptor * opt,
					  void * value, SANE_Word * info);
extern int sanei_save_values (int fd, SANE_Handle device);
extern int sanei_load_values (int fd, SANE_Handle device);

#endif /* sanei_h */
