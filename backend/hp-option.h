/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 Geoffrey T. Dairiki
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

   This file is part of a SANE backend for HP Scanners supporting
   HP Scanner Control Language (SCL).
*/

#ifndef HP_OPTION_H_INCLUDED
#define HP_OPTION_H_INCLUDED
#include "hp.h"

/*
 * Non-standard SANE options
 *
 * FIXME: this should become standard
 */
#ifndef SANE_NAME_AUTO_THRESHOLD
# define SANE_NAME_AUTO_THRESHOLD "auto-threshold"
# define SANE_TITLE_AUTO_THRESHOLD "Auto Threshold"
# define SANE_DESC_AUTO_THRESHOLD \
    "Enable automatic determination of threshold for line-art scans."
#endif

#ifndef SANE_NAME_SMOOTHING
# define SANE_NAME_SMOOTHING "smoothing"
# define SANE_TITLE_SMOOTHING "Smoothing"
# define SANE_DESC_SMOOTHING "Select smoothing filter."
#endif

#ifndef SANE_NAME_UNLOAD_AFTER_SCAN
# define SANE_NAME_UNLOAD_AFTER_SCAN "unload-after-scan"
# define SANE_TITLE_UNLOAD_AFTER_SCAN "Unload media after scan"
# define SANE_DESC_UNLOAD_AFTER_SCAN "Unloads the media after a scan."
#endif

#ifndef SANE_NAME_CHANGE_DOC
# define SANE_NAME_CHANGE_DOC "change-document"
# define SANE_TITLE_CHANGE_DOC "Change document"
# define SANE_DESC_CHANGE_DOC "Change Document."
#endif

#ifndef SANE_NAME_UNLOAD
# define SANE_NAME_UNLOAD "unload"
# define SANE_TITLE_UNLOAD "Unload"
# define SANE_DESC_UNLOAD "Unload Document."
#endif

#ifndef SANE_NAME_CALIBRATE
# define SANE_NAME_CALIBRATE "calibrate"
# define SANE_TITLE_CALIBRATE "Calibrate"
# define SANE_DESC_CALIBRATE "Start calibration process."
#endif

#ifndef SANE_NAME_MEDIA
# define SANE_NAME_MEDIA "media-type"
# define SANE_TITLE_MEDIA "Media"
# define SANE_DESC_MEDIA "Set type of media."
#endif

#ifndef SANE_NAME_PS_EXPOSURE_TIME
# define SANE_NAME_PS_EXPOSURE_TIME "ps-exposure-time"
# define SANE_TITLE_PS_EXPOSURE_TIME "Exposure time"
# define SANE_DESC_PS_EXPOSURE_TIME "A longer exposure time lets the scanner\
 collect more light. Suggested use is 175% for prints,\
 150% for normal slides and \"Negatives\" for\
 negative film. For dark (underexposed) images you can increase this value."
#endif

#ifndef SANE_NAME_MATRIX_TYPE
# define SANE_NAME_MATRIX_TYPE "matrix-type"
# define SANE_TITLE_MATRIX_TYPE "Color Matrix"
/* FIXME: better description */
# define SANE_DESC_MATRIX_TYPE "Set the scanners color matrix."
#endif

#ifndef SANE_NAME_MATRIX_RGB
# define SANE_NAME_MATRIX_RGB "matrix-rgb"
# define SANE_TITLE_MATRIX_RGB "Color Matrix"
# define SANE_DESC_MATRIX_RGB "Custom color matrix."
#endif

#ifndef SANE_NAME_MATRIX_GRAY
# define SANE_NAME_MATRIX_GRAY "matrix-gray"
# define SANE_TITLE_MATRIX_GRAY "Mono Color Matrix"
# define SANE_DESC_MATRIX_GRAY "Custom color matrix for grayscale scans."
#endif

#ifndef SANE_NAME_MIRROR_HORIZ
# define SANE_NAME_MIRROR_HORIZ "mirror-horizontal"
# define SANE_TITLE_MIRROR_HORIZ "Mirror horizontal"
# define SANE_DESC_MIRROR_HORIZ "Mirror image horizontally."
#endif

#ifndef SANE_NAME_MIRROR_VERT
# define SANE_NAME_MIRROR_VERT "mirror-vertical"
# define SANE_TITLE_MIRROR_VERT "Mirror vertical"
# define SANE_DESC_MIRROR_VERT "Mirror image vertically."
#endif

#ifndef SANE_NAME_UPDATE
# define SANE_NAME_UPDATE "update-options"
# define SANE_TITLE_UPDATE "Update options"
# define SANE_DESC_UPDATE "Update options."
#endif

#ifndef SANE_NAME_BUTTON_WAIT
# define SANE_NAME_BUTTON_WAIT   "button-wait"
# define SANE_TITLE_BUTTON_WAIT  "Front button wait"
# define SANE_DESC_BUTTON_WAIT   "Wait to scan for front-panel button push."
# define HP_BUTTON_WAIT_NO       0
# define HP_BUTTON_WAIT_YES      1
#endif

/* Some test stuff to see what undocumented SCL-commands do */
# define SANE_NAME_10470 "10470"
# define SANE_TITLE_10470 "10470"
# define SANE_DESC_10470 "10470."

# define SANE_NAME_10485 "10485"
# define SANE_TITLE_10485 "10485"
# define SANE_DESC_10485 "10485."

# define SANE_NAME_10952 "10952"
# define SANE_TITLE_10952 "10952"
# define SANE_DESC_10952 "10952."

# define SANE_NAME_10967 "10967"
# define SANE_TITLE_10967 "10967"
# define SANE_DESC_10967 "10967."

/*
 * Internal option names which are not presented to the SANE frontend.
 */
#define HP_NAME_HORIZONTAL_DITHER	"__hdither__"
#define HP_NAME_DEVPIX_RESOLUTION	"__devpix_resolution__"
#ifdef ENABLE_7x12_TONEMAPS
# define HP_NAME_RGB_TONEMAP		"__rgb_tonemap__"
#endif
#ifdef FAKE_COLORSEP_MATRIXES
# define HP_NAME_SEPMATRIX		"__sepmatrix__"
#endif

struct hp_choice_s
{
    int		val;
    const char *name;
    hp_bool_t	(*enable)(HpChoice this, HpOptSet optset, HpData data,
                          const HpDeviceInfo *info);
    hp_bool_t	is_emulated:1;
    HpChoice	next;
};

enum hp_scanmode_e
{
    HP_SCANMODE_LINEART	= 0,
    HP_SCANMODE_HALFTONE	= 3,
    HP_SCANMODE_GRAYSCALE	= 4,
    HP_SCANMODE_COLOR	= 5
};

enum hp_dither_type_e {
    HP_DITHER_CUSTOM		= -1,
    HP_DITHER_COARSE		= 0,
    HP_DITHER_FINE		= 1,
    HP_DITHER_BAYER		= 2,
    HP_DITHER_VERTICAL		= 3,
    HP_DITHER_HORIZONTAL
};

enum hp_matrix_type_e {
    HP_MATRIX_AUTO		= -256,
    HP_MATRIX_GREEN		= -257,
    HP_MATRIX_CUSTOM_BW		= -2,
    HP_MATRIX_CUSTOM		= -1,
    HP_MATRIX_RGB		= 0,
    HP_MATRIX_BW		= 1,
    HP_MATRIX_PASS		= 2,
    HP_MATRIX_RED		= 3,
    HP_MATRIX_BLUE		= 4,
    HP_MATRIX_XPA_RGB		= 5,
    HP_MATRIX_XPA_BW		= 6
};

enum hp_mirror_horiz_e {
    HP_MIRROR_HORIZ_CONDITIONAL = -256,
    HP_MIRROR_HORIZ_OFF         = 0,
    HP_MIRROR_HORIZ_ON          = 1
};

enum hp_mirror_vert_e {
    HP_MIRROR_VERT_OFF         = -258,
    HP_MIRROR_VERT_ON          = -257,
    HP_MIRROR_VERT_CONDITIONAL = -256
};

enum hp_media_e {
    HP_MEDIA_NEGATIVE = 1,
    HP_MEDIA_SLIDE = 2,
    HP_MEDIA_PRINT = 3
};

hp_bool_t   sanei_hp_choice_isEnabled (HpChoice this, HpOptSet optset,
                               HpData data, const HpDeviceInfo *info);

SANE_Status sanei_hp_optset_new(HpOptSet * newp, HpScsi scsi, HpDevice dev);
SANE_Status sanei_hp_optset_download (HpOptSet this, HpData data, HpScsi scsi);
SANE_Status sanei_hp_optset_control (HpOptSet this, HpData data,
                               int optnum, SANE_Action action,
			       void * valp, SANE_Int *infop, HpScsi scsi,
                               hp_bool_t immediate);
SANE_Status sanei_hp_optset_guessParameters (HpOptSet this, HpData data,
                               SANE_Parameters * p);
enum hp_scanmode_e sanei_hp_optset_scanmode (HpOptSet this, HpData data);
int sanei_hp_optset_data_width (HpOptSet this, HpData data);
hp_bool_t sanei_hp_optset_isImmediate (HpOptSet this, int optnum);
hp_bool_t sanei_hp_optset_mirror_vert (HpOptSet this, HpData data, HpScsi scsi);
hp_bool_t sanei_hp_optset_start_wait(HpOptSet this, HpData data, HpScsi scsi);
HpScl sanei_hp_optset_scan_type (HpOptSet this, HpData data);
const SANE_Option_Descriptor * sanei_hp_optset_saneoption (HpOptSet this,
                               HpData data, int optnum);

#endif /* HP_OPTION_H_INCLUDED */
