/* sane - Scanner Access Now Easy.
   Copyright (C) 1996, 1997 David Mosberger-Tang and Andreas Beck
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

   This file declares common option names, titles, and descriptions.  A
   backend is not limited to these options but for the sake of
   consistency it's better to use options declared here when appropriate.
 */
#ifndef saneopts_h
#define saneopts_h

/* This _must_ be the first option (index 0): */
#define SANE_NAME_NUM_OPTIONS		""	/* never settable */
#define SANE_NAME_PREVIEW		"preview"
#define SANE_NAME_GRAY_PREVIEW		"preview-in-gray"
#define SANE_NAME_BIT_DEPTH		"depth"
#define SANE_NAME_SCAN_MODE		"mode"
#define SANE_NAME_SCAN_SPEED		"speed"
#define SANE_NAME_SCAN_SOURCE		"source"
#define SANE_NAME_BACKTRACK		"backtrack"
/* Most user-interfaces will let the user specify the scan area as the
   top-left corner and the width/height of the scan area.  The reason
   the backend interface uses the top-left/bottom-right corner is so
   that the scan area values can be properly constraint independent of
   any other option value.  */
#define SANE_NAME_SCAN_TL_X		"tl-x"
#define SANE_NAME_SCAN_TL_Y		"tl-y"
#define SANE_NAME_SCAN_BR_X		"br-x"
#define SANE_NAME_SCAN_BR_Y		"br-y"
#define SANE_NAME_SCAN_RESOLUTION	"resolution"
#define SANE_NAME_SCAN_X_RESOLUTION	"resolution"
#define SANE_NAME_SCAN_Y_RESOLUTION	"y-resolution"
#define SANE_NAME_CUSTOM_GAMMA		"custom-gamma"
#define SANE_NAME_GAMMA_VECTOR		"gamma-table"
#define SANE_NAME_GAMMA_VECTOR_R	"red-gamma-table"
#define SANE_NAME_GAMMA_VECTOR_G	"green-gamma-table"
#define SANE_NAME_GAMMA_VECTOR_B	"blue-gamma-table"
#define SANE_NAME_BRIGHTNESS		"brightness"
#define SANE_NAME_CONTRAST		"contrast"
#define SANE_NAME_GRAIN_SIZE		"grain"
#define SANE_NAME_HALFTONE		"halftoning"
#define SANE_NAME_BLACK_LEVEL           "black-level"
#define SANE_NAME_WHITE_LEVEL           "white-level"
#define SANE_NAME_SHADOW		"shadow"
#define SANE_NAME_SHADOW_R		"shadow-r"
#define SANE_NAME_SHADOW_G		"shadow-g"
#define SANE_NAME_SHADOW_B		"shadow-b"
#define SANE_NAME_HIGHLIGHT		"highlight"
#define SANE_NAME_HIGHLIGHT_R		"highlight-r"
#define SANE_NAME_HIGHLIGHT_G		"highlight-g"
#define SANE_NAME_HIGHLIGHT_B		"highlight-b"
#define SANE_NAME_HUE			"hue"
#define SANE_NAME_SATURATION		"saturation"
#define SANE_NAME_FILE			"filename"
#define SANE_NAME_HALFTONE_DIMENSION	"halftone-size"
#define SANE_NAME_HALFTONE_PATTERN	"halftone-pattern"
#define SANE_NAME_RESOLUTION_BIND	"resolution-bind"
#define SANE_NAME_NEGATIVE		"negative"
#define SANE_NAME_QUALITY_CAL		"quality-cal"
#define SANE_NAME_DOR			"double-res"
#define SANE_NAME_RGB_BIND		"rgb-bind"
#define SANE_NAME_THRESHOLD		"threshold"
#define SANE_NAME_ANALOG_GAMMA		"analog-gamma"
#define SANE_NAME_ANALOG_GAMMA_R	"analog-gamma-r"
#define SANE_NAME_ANALOG_GAMMA_G	"analog-gamma-g"
#define SANE_NAME_ANALOG_GAMMA_B	"analog-gamma-b"
#define SANE_NAME_ANALOG_GAMMA_BIND	"analog-gamma-bind"
#define SANE_NAME_SMEAR			"smear"
#define SANE_NAME_TEN_BIT_MODE		"ten-bit-mode"
#define SANE_NAME_TWELVE_BIT_MODE	"twelve-bit-mode"
#define SANE_NAME_WARMUP		"warmup"
#define SANE_NAME_RGB_PREVIEW_PATCH	"preview-patch"
#define SANE_NAME_START_SCAN_PATCH	"start-scan-patch"
#define SANE_NAME_CAL_EXPOS_TIME	"cal-exposure-time"
#define SANE_NAME_CAL_EXPOS_TIME_R	"cal-exposure-time-r"
#define SANE_NAME_CAL_EXPOS_TIME_G	"cal-exposure-time-g"
#define SANE_NAME_CAL_EXPOS_TIME_B	"cal-exposure-time-b"
#define SANE_NAME_SCAN_EXPOS_TIME	"scan-exposure-time"
#define SANE_NAME_SCAN_EXPOS_TIME_R	"scan-exposure-time-r"
#define SANE_NAME_SCAN_EXPOS_TIME_G	"scan-exposure-time-g"
#define SANE_NAME_SCAN_EXPOS_TIME_B	"scan-exposure-time-b"
#define SANE_NAME_SELECT_EXPOSURE_TIME	"select-exposure-time"
#define SANE_NAME_CAL_LAMP_DEN		"cal-lamp-density"
#define SANE_NAME_SCAN_LAMP_DEN		"scan-lamp-density"
#define SANE_NAME_SELECT_LAMP_DENSITY	"select-lamp-density"

#define SANE_TITLE_NUM_OPTIONS		"Number of options"
#define SANE_TITLE_PREVIEW		"Preview"
#define SANE_TITLE_GRAY_PREVIEW		"Force monochrome preview"
#define SANE_TITLE_BIT_DEPTH		"Bit depth"
#define SANE_TITLE_SCAN_MODE		"Scan mode"
#define SANE_TITLE_SCAN_SPEED		"Scan speed"
#define SANE_TITLE_SCAN_SOURCE		"Scan source"
#define SANE_TITLE_BACKTRACK		"Force backtracking"
#define SANE_TITLE_SCAN_TL_X		"Top-left x"
#define SANE_TITLE_SCAN_TL_Y		"Top-left y"
#define SANE_TITLE_SCAN_BR_X		"Bottom-right x"
#define SANE_TITLE_SCAN_BR_Y		"Bottom-right y"
#define SANE_TITLE_SCAN_RESOLUTION	"Scan resolution"
#define SANE_TITLE_SCAN_X_RESOLUTION	"X-resolution"
#define SANE_TITLE_SCAN_Y_RESOLUTION	"Y-resolution"
#define SANE_TITLE_CUSTOM_GAMMA		"Use custom gamma table"
#define SANE_TITLE_GAMMA_VECTOR		"Image intensity"
#define SANE_TITLE_GAMMA_VECTOR_R	"Red intensity"
#define SANE_TITLE_GAMMA_VECTOR_G	"Green intensity"
#define SANE_TITLE_GAMMA_VECTOR_B	"Blue intensity"
#define SANE_TITLE_BRIGHTNESS		"Brightness"
#define SANE_TITLE_CONTRAST		"Contrast"
#define SANE_TITLE_GRAIN_SIZE		"Grain size"
#define SANE_TITLE_HALFTONE		"Halftoning"
#define SANE_TITLE_BLACK_LEVEL          "Black level"
#define SANE_TITLE_WHITE_LEVEL          "White level"
#define SANE_TITLE_SHADOW		"Shadow"
#define SANE_TITLE_SHADOW_R		"Shadow for red"
#define SANE_TITLE_SHADOW_G		"Shadow for green"
#define SANE_TITLE_SHADOW_B		"Shadow for blue"
#define SANE_TITLE_HIGHLIGHT		"Highlight"
#define SANE_TITLE_HIGHLIGHT_R		"Highlight for red"
#define SANE_TITLE_HIGHLIGHT_G		"Highlight for green"
#define SANE_TITLE_HIGHLIGHT_B		"Highlight for blue"
#define SANE_TITLE_HUE			"Hue"
#define SANE_TITLE_SATURATION		"Saturation"
#define SANE_TITLE_FILE			"Filename"
#define SANE_TITLE_HALFTONE_DIMENSION	"Halftone pattern size"
#define SANE_TITLE_HALFTONE_PATTERN	"Halftone pattern"
#define SANE_TITLE_RESOLUTION_BIND	"Bind X and Y resolution"
#define SANE_TITLE_NEGATIVE		"Negative"
#define SANE_TITLE_QUALITY_CAL		"Quality calibration"
#define SANE_TITLE_DOR			"Double Optical Resolution"
#define SANE_TITLE_RGB_BIND		"Bind RGB"
#define SANE_TITLE_THRESHOLD		"Threshold"
#define SANE_TITLE_ANALOG_GAMMA		"Analog gamma correction"
#define SANE_TITLE_ANALOG_GAMMA_R	"Analog gamma red"
#define SANE_TITLE_ANALOG_GAMMA_G	"Analog gamma green"
#define SANE_TITLE_ANALOG_GAMMA_B	"Analog gamma blue"
#define SANE_TITLE_ANALOG_GAMMA_BIND    "Bind analog gamma"
#define SANE_TITLE_SMEAR		"Smear"
#define SANE_TITLE_TEN_BIT_MODE		"10 bit-mode"
#define SANE_TITLE_TWELVE_BIT_MODE	"12 bit-mode"
#define SANE_TITLE_WARMUP		"Warmup lamp"
#define SANE_TITLE_RGB_PREVIEW_PATCH	"Preview patch"
#define SANE_TITLE_START_SCAN_PATCH	"Start-scan patch"
#define SANE_TITLE_CAL_EXPOS_TIME	"Cal. exposure-time"
#define SANE_TITLE_CAL_EXPOS_TIME_R	"Cal. exposure-time for red"
#define SANE_TITLE_CAL_EXPOS_TIME_G	"Cal. exposure-time for green"
#define SANE_TITLE_CAL_EXPOS_TIME_B	"Cal. exposure-time for blue"
#define SANE_TITLE_SCAN_EXPOS_TIME	"Scan exposure-time"
#define SANE_TITLE_SCAN_EXPOS_TIME_R	"Scan exposure-time for red"
#define SANE_TITLE_SCAN_EXPOS_TIME_G	"Scan exposure-time for green"
#define SANE_TITLE_SCAN_EXPOS_TIME_B	"Scan exposure-time for blue"
#define SANE_TITLE_SELECT_EXPOSURE_TIME	"Set exposure-time"
#define SANE_TITLE_CAL_LAMP_DEN		"Cal. lamp density"
#define SANE_TITLE_SCAN_LAMP_DEN	"Scan lamp density"
#define SANE_TITLE_SELECT_LAMP_DENSITY	"Set lamp density"

/* Descriptive/help strings for above options: */
#define SANE_DESC_NUM_OPTIONS \
"Read-only option that specifies how many options a specific devices " \
"supports."

#define SANE_DESC_PREVIEW \
"Request a preview-quality scan."

#define SANE_DESC_GRAY_PREVIEW \
"Request that all previews are done in monochrome mode.  On a three-pass " \
" scanner this cuts down the number of passes to one and on a one-pass " \
"scanner, it reduces the memory requirements and scan-time of the preview."

#define SANE_DESC_BIT_DEPTH \
"Number of bits per sample, typical values are 1 for \"line-art\" and 8 " \
"for multibit scans."

#define SANE_DESC_SCAN_MODE \
"Selects the scan mode (e.g., lineart,monochrome, or color)."

#define SANE_DESC_SCAN_SPEED \
"Determines the speed at which the scan proceeds."

#define SANE_DESC_SCAN_SOURCE \
"Selects the scan source (such as a document-feeder)."

#define SANE_DESC_BACKTRACK \
"Controls whether backtracking is forced."

#define SANE_DESC_SCAN_TL_X \
"Top-left x position of scan area."

#define SANE_DESC_SCAN_TL_Y \
"Top-left y position of scan area."

#define SANE_DESC_SCAN_BR_X \
"Bottom-right x position of scan area."

#define SANE_DESC_SCAN_BR_Y \
"Bottom-right y position of scan area."

#define SANE_DESC_SCAN_RESOLUTION \
"Sets the resolution of the scanned image."

#define SANE_DESC_SCAN_X_RESOLUTION \
"Sets the horizontal resolution of the scanned image."

#define SANE_DESC_SCAN_Y_RESOLUTION \
"Sets the vertical resolution of the scanned image."

#define SANE_DESC_CUSTOM_GAMMA \
"Determines whether a builtin or a custom gamma-table should be used."

#define SANE_DESC_GAMMA_VECTOR \
"Gamma-correction table.  In color mode this option equally affects the " \
"red, green, and blue channels simultaneously (i.e., it is an intensity " \
"gamma table)."

#define SANE_DESC_GAMMA_VECTOR_R \
"Gamma-correction table for the red band."

#define SANE_DESC_GAMMA_VECTOR_G \
"Gamma-correction table for the green band."

#define SANE_DESC_GAMMA_VECTOR_B \
"Gamma-correction table for the blue band."

#define SANE_DESC_BRIGHTNESS \
"Controls the brightness of the acquired image."

#define SANE_DESC_CONTRAST \
"Controls the contrast of the acquired image."

#define SANE_DESC_GRAIN_SIZE \
"Selects the \"graininess\" of the acquired image.  Smaller values " \
"result in sharper images."

#define SANE_DESC_HALFTONE \
"Selects whether the acquired image should be halftoned (dithered)."

#define SANE_DESC_BLACK_LEVEL \
"Selects what radiance level should be considered \"black\"."

#define SANE_DESC_WHITE_LEVEL \
"Selects what radiance level should be considered \"white\"."

#define SANE_DESC_SHADOW \
"Selects what radiance level should be considered \"black\"."
#define SANE_DESC_SHADOW_R \
"Selects what red radiance level should be considered \"black\"."
#define SANE_DESC_SHADOW_G \
"Selects what green radiance level should be considered \"black\"."
#define SANE_DESC_SHADOW_B \
"Selects what blue radiance level should be considered \"black\"."

#define SANE_DESC_HIGHLIGHT \
"Selects what radiance level should be considered \"white\"."
#define SANE_DESC_HIGHLIGHT_R \
"Selects what red radiance level should be considered \"full red\"."
#define SANE_DESC_HIGHLIGHT_G \
"Selects what green radiance level should be considered \"full green\"."
#define SANE_DESC_HIGHLIGHT_B \
"Selects what blue radiance level should be considered \"full blue\"."

#define SANE_DESC_HUE \
"Controls the \"hue\" (blue-level) of the acquired image."

#define SANE_DESC_SATURATION \
"The saturation level controls the amount of \"blooming\" that occurs when " \
"acquiring an image with a camera. Larger values cause more blooming."

#define SANE_DESC_FILE "The filename of the image to be loaded."

#define SANE_DESC_HALFTONE_DIMENSION \
"Sets the size of the halftoning (dithering) pattern used when scanning " \
"halftoned images."

#define SANE_DESC_HALFTONE_PATTERN \
"Defines the halftoning (dithering) pattern for scanning halftoned images."

#define SANE_DESC_RESOLUTION_BIND	"Use same values for X and Y resolution"
#define SANE_DESC_NEGATIVE		"Swap black and white"
#define SANE_DESC_QUALITY_CAL		"Do a quality white-calibration"
#define SANE_DESC_DOR			"Use lens that doubles optical resolution"
#define SANE_DESC_RGB_BIND		"In RGB-mode use same values for each color"
#define SANE_DESC_THRESHOLD		"Select minimum-brightness to get a white point"
#define SANE_DESC_ANALOG_GAMMA		"Analog gamma-correction"
#define SANE_DESC_ANALOG_GAMMA_R	"Analog gamma-correction for red"
#define SANE_DESC_ANALOG_GAMMA_G	"Analog gamma-correction for green"
#define SANE_DESC_ANALOG_GAMMA_B	"Analog gamma-correction for blue"
#define SANE_DESC_ANALOG_GAMMA_BIND	"In RGB-mode use same values for each color"
#define SANE_DESC_SMEAR			"Don't care about image smearing problem"
#define SANE_DESC_TEN_BIT_MODE		"Output with 10 bits instead of 8 bits"
#define SANE_DESC_TWELVE_BIT_MODE	"Output with 12 bits instead of 8 bits"
#define SANE_DESC_WARMUP		"Warmup lamp before scanning"
#define SANE_DESC_RGB_PREVIEW_PATCH	"Set preview-bit in rgb-mode"
#define SANE_DESC_START_SCAN_PATCH	"Use special start-scan bits"
#define SANE_DESC_CAL_EXPOS_TIME	"Define exposure-time for calibration"
#define SANE_DESC_CAL_EXPOS_TIME_R	"Define exposure-time for red calibration"
#define SANE_DESC_CAL_EXPOS_TIME_G	"Define exposure-time for green calibration"
#define SANE_DESC_CAL_EXPOS_TIME_B	"Define exposure-time for blue calibration"
#define SANE_DESC_SCAN_EXPOS_TIME	"Define exposure-time for scan"
#define SANE_DESC_SCAN_EXPOS_TIME_R	"Define exposure-time for red scan"
#define SANE_DESC_SCAN_EXPOS_TIME_G	"Define exposure-time for green scan"
#define SANE_DESC_SCAN_EXPOS_TIME_B	"Define exposure-time for blue scan"
#define SANE_DESC_SELECT_EXPOSURE_TIME	"Enable selection of exposure-time"
#define SANE_DESC_CAL_LAMP_DEN		"Define lamp density for calibration"
#define SANE_DESC_SCAN_LAMP_DEN		"Define lamp density for scan"
#define SANE_DESC_SELECT_LAMP_DENSITY	"Enable selection of lamp density"

#endif /* saneopts_h */
