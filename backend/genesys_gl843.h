/* sane - Scanner Access Now Easy.

   Copyright (C) 2010-2013 Stéphane Voltz <stef.dev@free.fr>

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

#include "genesys.h"

#ifndef BACKEND_GENESYS_GL843_H
#define BACKEND_GENESYS_GL843_H

#define SCAN_TABLE 	0 	/* table 1 at 0x4000 */
#define BACKTRACK_TABLE 1 	/* table 2 at 0x4800 */
#define STOP_TABLE 	2 	/* table 3 at 0x5000 */
#define FAST_TABLE 	3 	/* table 4 at 0x5800 */
#define HOME_TABLE 	4 	/* table 5 at 0x6000 */

#define SCAN_FLAG_SINGLE_LINE              0x001
#define SCAN_FLAG_DISABLE_SHADING          0x002
#define SCAN_FLAG_DISABLE_GAMMA            0x004
#define SCAN_FLAG_DISABLE_BUFFER_FULL_MOVE 0x008
#define SCAN_FLAG_IGNORE_LINE_DISTANCE     0x010
#define SCAN_FLAG_DISABLE_LAMP             0x040
#define SCAN_FLAG_DYNAMIC_LINEART          0x080

#define SETREG(adr,val) { dev->reg.init_reg(adr, val); }

#endif // BACKEND_GENESYS_GL843_H
