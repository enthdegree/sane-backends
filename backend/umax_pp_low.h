/* sane - Scanner Access Now Easy.
   Copyright (C) 2001 Stéphane Voltz <svoltz@wanadoo.fr>
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

   This file implements a SANE backend for Umax PP flatbed scanners.  */

#include <stdio.h>
#include "../include/sane/config.h"

/*****************************************************************************/
/*                 set port to 'idle state' and get iopl                     */
/*****************************************************************************/
extern int sanei_umax_pp_InitPort (int port);







extern int sanei_umax_pp_ProbeScanner (int recover);
extern int sanei_umax_pp_InitScanner (int recover);
extern int sanei_umax_pp_InitTransport (int recover);
extern int sanei_umax_pp_ReleaseScanner (void);
extern int sanei_umax_pp_EndSession (void);
extern int sanei_umax_pp_InitCancel (void);
extern int sanei_umax_pp_Cancel (void);
extern int sanei_umax_pp_CheckModel (void);

#ifndef __GLOBALES__

#define RGB_MODE	0x10
#define RGB12_MODE	0x11
#define BW_MODE		0x08
#define BW12_MODE       0x09
#define BW2_MODE        0x04



#define __GLOBALES__
#endif /* __GLOBALES__ */



#ifndef PRECISION_ON
#define PRECISION_ON  1
#define PRECISION_OFF 0
#define LAMP_STATE	0x20

#define MOTOR_BIT     0x40

#endif

extern int sanei_umax_pp_Scan (int x, int y, int width, int height, int dpi,
			       int color, int gain, int highlight);
extern int sanei_umax_pp_Move (int distance, int precision,
			       unsigned char *buffer);
extern int sanei_umax_pp_Lamp (int on);
extern int sanei_umax_pp_CompletionWait (void);
extern int sanei_umax_pp_CommitScan (void);
extern int sanei_umax_pp_Park (void);
extern int sanei_umax_pp_ParkWait (void);
extern int sanei_umax_pp_ReadBlock (long len, int window, int dpi, int last,
				    unsigned char *buffer);
extern int sanei_umax_pp_StartScan (int x, int y, int width, int height,
				    int dpi, int color, int gain,
				    int contrast, int *rbpp, int *rtw,
				    int *rth);

extern void sanei_umax_pp_setport (int port);
extern int sanei_umax_pp_getport (void);
extern void sanei_umax_pp_setparport (int fd);
extern int sanei_umax_pp_getparport (void);
extern int sanei_parport_info (int number, int *addr);
extern void sanei_umax_pp_setastra (int mod);
extern int sanei_umax_pp_getastra (void);
extern int sanei_umax_pp_ScannerStatus (void);
extern int sanei_umax_pp_ReleaseScanner (void);
extern int sanei_umax_pp_EndSession (void);
extern int sanei_umax_pp_ProbeScanner (int recover);


extern int sanei_umax_pp_CmdSync (int cmd);
