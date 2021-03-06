/* sane - Scanner Access Now Easy.

   Copyright (C) 2002 Sergey Vlasov <vsu@altlinux.ru>

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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.

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

#ifndef GT68XX_GT6816_H
#define GT68XX_GT6816_H

static SANE_Status
gt6816_check_firmware (GT68xx_Device * dev, SANE_Bool * loaded);

static SANE_Status
gt6816_download_firmware (GT68xx_Device * dev,
			  SANE_Byte * data, SANE_Word size);

static SANE_Status
gt6816_get_power_status (GT68xx_Device * dev, SANE_Bool * power_ok);

static SANE_Status
gt6816_get_ta_status (GT68xx_Device * dev, SANE_Bool * ta_attached);

static SANE_Status
gt6816_lamp_control (GT68xx_Device * dev, SANE_Bool fb_lamp,
		     SANE_Bool ta_lamp);

static SANE_Status gt6816_is_moving (GT68xx_Device * dev, SANE_Bool * moving);

static SANE_Status gt6816_carriage_home (GT68xx_Device * dev);

static SANE_Status gt6816_stop_scan (GT68xx_Device * dev);

static SANE_Status gt6816_document_present (GT68xx_Device * dev, SANE_Bool * present);

#endif /* not GT68XX_GT6816_H */
