/* sane - Scanner Access Now Easy.

   based on sources acquired from Plustek Inc.
   Copyright (C) 2002 Gerhard Jaeger <g.jaeger@earthling.net>

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

   Interface files for the LM9831/2/3 chip,
   a chip used in many USB scanners.

 */

#ifndef sanei_lm983x_h
#define sanei_lm983x_h

#include "../include/sane/config.h"
#include "../include/sane/sane.h"

#define sanei_lm983x_read_byte(fd, reg, value) \
                                       sanei_lm983x_read (fd, reg, value, 1, 0)

extern void sanei_lm983x_init( void );

extern SANE_Status sanei_lm983x_write_byte( SANE_Int fd,
                                            SANE_Byte reg, SANE_Byte value );

extern SANE_Status sanei_lm983x_write( SANE_Int fd, SANE_Byte reg,
                                       SANE_Byte *buffer, SANE_Word len,
                                                         SANE_Bool increment );

extern SANE_Status sanei_lm983x_read( SANE_Int fd, SANE_Byte reg,
                                      SANE_Byte *buffer, SANE_Word len,
                                                         SANE_Bool increment );

extern SANE_Bool sanei_lm983x_reset( SANE_Int fd );
					    					
#endif /* sanei_lm983x_h */

