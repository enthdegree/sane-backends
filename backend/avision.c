
/*******************************************************************************
 * SANE - Scanner Access Now Easy.

   avision.c

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

   *****************************************************************************

   This backend is based upon the Tamarack backend and adapted to the Avision
   scanners by René Rebe and Meino Cramer.
   
   This file implements a SANE backend for the Avision SCSI Scanners (like the
   AV 630 / 620 (CS) ...) and some Avision (OEM) USB scanners (like the HP 53xx,
   74xx, Minolta FS-V1 ...) or Fujitsu ScanPartner with the AVISION SCSI-2/3
   or USB command set.
   
   Copyright 2002 by
                "René Rebe" <rene@exactcode.de>
                "Jose Paulo Moitinho de Almeida" <moitinho@civil.ist.utl.pt>
   
   Copyright 1999, 2000, 2001 by
                "René Rebe" <rene@exactcode.de>
                "Meino Christian Cramer" <mccramer@s.netic.de>

   Copyright 2003, 2004, 2005 by
                "René Rebe" <rene@exactcode.de>
   
   Additional Contributers:
                "Gunter Wagner"
                  (some fixes and the transparency option)
                "Martin Jelínek" <mates@sirrah.troja.mff.cuni.cz>
                   nice attach debug output
                "Marcin Siennicki" <m.siennicki@cloos.pl>
                   found some typos and contributed fixes for the HP 7400
                "Frank Zago" <fzago@greshamstorage.com>
                   Mitsubishi IDs and report
                Avision INC
                   example code to handle calibration and C5 ASIC specifics
                "Franz Bakan" <fbakan@gmx.net>
                   OS/2 threading support
   
   Many additinoal special thanks to:
                Avision INC for providing protocol documentation.
                Avision INC for sponsoring an AV 8000S with ADF.
                Avision Europ and BHS Binkert for sponsoring several scanners.
                Roberto Di Cosmo who sponsored a HP 5370 scanner.
                Oliver Neukum who sponsored a HP 5300 USB scanner.
                Matthias Wiedemann for lending his HP 7450C for some weeks.
                Compusoft, C.A. Caracas / Venezuela for sponsoring a
                           HP 7450 scanner and so enhanced ADF support.
                Chris Komatsu for the nice ADF scanning observartion.

                All the many other beta-tester and debug-log sender!

                Thanks to all the people and companies above. Without you
                the Avision backend would not be in the shape it is today! ;-)
   
   ChangeLog:
   2005-07-09: René Rebe
         * implemented non-interlaced duplex

   2005-07-04:
         * implemented message passing for e.g. LED values
         * optimized batch scans by only sending the window, gamma, ...
           when the parameters changed (1-2s faster per page)

   2005-06-17: René Rebe
         * implemented interrupt endpoint button read (AV 220)
         * implemented another interrupt endpoint button read variant (AV 210)
 
   2005-02-22: René Rebe
         * fixed init_options gamma vector inactive marking
         * fixed control_option to not return error for button read out even
           when no buttons are marked active (for xsane's error flood)
         * added timeout control as well as extreme usb i/o code path
           obfuscating to get the AV 610 and AV 610 CU past a sane_{open,close}
           cycle (...)
         * changed the backend to always detect the status read out method
           (no per device marking anymore due to new timeout control)
         * hardened the status read outs to retry the read soem times
         * handed retry control of avision_usb_status to the caller, to
           optimize annoying delay caused by unnecessary retries
         * added a height boundary for interlaced duplex scans, since the
           AV 220 only seems to like multiples of 8

   2005-02-14: René Rebe
         * fixed send_gamma to actually abort on error
         * implemented computation and sending an acceleration table for
           C6 ASICs (the AV610 needs it at least for low resolutions)
   
   2005-02-13: René Rebe
         * removed RES_HACK* and converted it to use the higher resolution
           for all C6 and newer ASICs
         * improved 64byte alignment work around for calibration read
         * fixed AV610 non color scans
         * work around most of the 64 byte constrains of some ASICs
         * fixed gamma for the A980 ASIC and hardened the usb_status readouts
           accordingly (the ASIC returns 2 bytes and stalls when not read out)
         * removed the AV_NO_GAMMA marks from the device table

   2005-02-04: René Rebe
         * added Minolta Scan Dual I as well as some conditional overwrites
           for this "old" device not filling the inquiry fields ...

   2005-01-21: René Rebe
         * modified gamma-table interface to what e.g. xsane expect, that is no
	   intensity table for color scans
   
   2005-01-17: René Rebe
         * fixed the 16bit modes to check for 16bit, not just > 8bit
         * check for paper before waiting for the light source (avoids
	   annoying delays)
   
   2005-01-15: René Rebe
         * initialize the gamma table with a value of 2.22
   
   2005-01-13: Adam J. Richter:
         * fixes some debug typos as well as a null pointer condition

   2005-01-08: René Rebe
         * revisited USB I/O protocol logic, including detection of stuck
           status bytes in scanner's transfer FIFO for USB 2.0 bulk status reads
         * reverted to only read one status byte (instead of the 40 Bytes
           stuck data collection try)
         * improved USB sense request
   
   2004-12-21: René Rebe
         * implemented non-multiple of 64 reads for calibration data and ASIC C6
         * added a quirk to scan at least 240 dpi for the AV 610
   
   2004-12-18: René Rebe
         * fixed size of color_list and source_list prevent out of bounds access
         * added AV_FILMSCANNER feature overwrite since I had to notice the film
           scanner bit is quite randomly set by scanners (maybe when transparent
           scans are possible the bit is set, too)
	 * fixed gamma-table for HP 74xx and AV 610 (the Avision SPEC is wrong)
         * fixed compute_white_shading_data if the scanner returned shadig target
           is obviously invalid (AV610 does return junk in ADF mode)
	 * fixed the reader_process buffer size boundaries for certain ASIC bugs
           (AV610)
	 * set a preleminary paper_length in set_window (will be moved a bit later)
   
   2004-12-15: René Rebe
         * improved detecting of bulk vs. interrupt status reads (especially
           forced to stay at one type if once detected)
         * fixed sane_open to check for the error status of additinal_probe, so
           open fails instead of random segfaults due to uninitilize fields later
         * simillar check for the error of wait_4_light
         * do not perform the expensive wait_ready so often
         * wait some time after TEST_UNIT_READY as adviced from Avision
         * only set exposure values for film scanners
         * fixed typos
   
   2004-12-09: René Rebe
         * fixed to fall back to very old bits to determine the available
           channels per pixel (for older SCSI devices such as 630 or 6240)
   
   2004-12-08: René Rebe
         * fixed attach to not segmentation fault for SCSI scanners
         * print the version in sane_init, not just attach, so people can
           verify even if not devices are found
   
   2004-12-06: René Rebe
         * do not reserve and release the device for the whole backend lifetime,
           but only for the ongoing scan (as the windows driver does)
         * unlink the temporary file of duplex scans
   
   2004-12-04: René Rebe
         * duplex deinterlace support
         * stripe buffer 1/2 inch corrections
         * potentially 16bit image data processing fixes
         * fixed segmentation fault for scanners not defining inquiry_bits_per_channel
   
   2004-12-03: René Rebe
         * code rearrancements to be able to announce "-1" as line count
           for ADF scans ...
         * added guessed values for AV220 send_gamma table
         * added W2 ASIC define
         * added Kodak IDs
   
   2004-12-02: René Rebe
         * better EOP/EOF detection
         * no scanenr type hardcoding in the device list
         * (much rework for propper) duplex support
         * proper read_process cleanup (and so release_unit: fixes
           media_check on at least AV220)
         * preparation for advanced media_check in the future
   
   2004-12-01: René Rebe
         * several new IDs
         * removed obsolete device flags
         * added usb status read via bulk instead of interrupt transfers
         * new gamma table defines
         * fixed usb status read to read the status for the sense request
         * real ADF EOF handling
   
   2004-10-20: René Rebe
         * work in the ID table
         * fixed send_gamma for ASIC_OA980
         * removed the use of physical_connection
   
   2004-07-12: René Rebe
         * better button read value reset
         * no SANE_CAP_HARD_SELECT for the button options
   
   2004-07-08: René Rebe
         * implemented botton status readout
         * cleaned wait_4_light code path
   
   2004-06-25: René Rebe
         * removed AV_LIGHTCHECK_BOGUS from HP 74xx - it does work with todays
           code-path
         * fixed color packing
         * fixed more code pathes for 16bit channels
         * improved the default mode choosing to avoid 16bit modes
   
   2004-06-22: René Rebe
         * merged SANE/CVS thread changes and fixed compilation warnings
           (Bug #300399)

   2004-05-05: René Rebe
         * added a AV_FORCE_A3 overwrite (for AVA3)

   2004-03-13: René Rebe
         * removed old unneeded old_r_calibration
         * fixed get_calib_format for 16 bit color mode
         * 16 bit software calibration

   2004-03-02: Jose Paulo Moitinho de Almeida
         * fixed avision_usb_status to handle other *_GOOD conditions
         * fixed set_frame to work over native USB

   2004-02-24: René Rebe
         * removed static default mode - now the best supported mode is used
         * added byte-swapping for 16bit modes (at least this is what xsane
           expects on my UltraSPARC ...

   2004-02-21: René Rebe
         * added color_list* and add_color_mode to make make_color_mode dynamic
	 * save additional parameters in attach ...
	 * added AV_GRAYSCALE16 and AV_TRUECOLOR16 and adapted sane_get_parameters
           and set_window accordingly
	 * removed lingering c5_guard

   2004-02-10: René Rebe
         * removed detailed additional accessories detection out of attach
           and use data_dq in those again
         * simplified set_frame - the device is now guaranteed to be open
         * improved/simplified make_mode and make_source_mode
         * since wait_4_light works reliable and some scanners (e.g. HP 53xx)
           get confused if accessed during lamp warmup wait_4_light now returns
           BUSY when timed out
         * fixed set_window for C6 ASIC scanners (HP 74xx)

   2003-12-02: René Rebe
         * full ADF mirroring (with and without bgr->rgb convertion) support

   2003-11-29: René Rebe
         * remove unecessary states (is_adf, is_calibrated, page)
         * dynamic source_mode creation (to list only those available)
         * some more RES_HACKs and device_list updates
         * save adf_is_bgr_order

   2003-11-23: René Rebe
         * merged (the half-functional) c5_calibration with the
           normal_calibration
   
   2003-11-22: René Rebe
         * do not skip calibration for ADF scans
         * stick more closely on the information provided by the scanner
           for the decision whether to do a calibration or not (the first
           ADF scan for HP 53xx&74xx series needs a calibration!)
   
   2003-11-18: René Rebe
         * fixed some typos and to use more correct max shading targets
           for non-color calibrations
         * corrections to use the correct color or gray calibration method
         * fixed the set_calib_data to not use the multi channel code for
           non-color scans
   
   2003-11-12: René Rebe
         * work do always send at least 10 Bytes to USB devices
         * overworked attach () to send a standard INQUIRY first and
           ask for the firmware status before any other action
   
   2003-10-28: René Rebe
         * merging with the new SANE CVS, converted the .desc to the new
           format (Bug #300146)
         * some textual changes incl. a rewrite of the sane-avision man-page
           (Bugs #300290 and  #300291)
         * added another name overwrite for the Minolta Dimage Scan Dual II
           (Bug #300288)
         * fixed config file reading for non-terminated options (Bug #300196)
         * removed Option_Value since it is now included in sanei_backend.h
   
   2003-10-18: René Rebe
         * added send_nvram
         * renamed set_gamma -> send_gamma
         * added get_firmware_info and get_flash_ram_info
   
   2003-10-15: René Rebe
         * added nv-ram data
   
   2003-10-10: René Rebe
         * adapted the WHITE_MAP_RANGE to 0x4FFF (since 0x4000 results in too
           dark images ...)
   
   2003-10-04: René Rebe
         * fixed ADF model detection bug - could even crash (not functional
           change - just informational debug info)
         * reenabled wait_4_light, it works over usb and is needed if the
           scanner is accessed early (a HP 53xx hangs when accessed during
           the warmup phase ...)
   
   2003-10-01: René Rebe
         * fixed yet another endianness issue in the calibration code
         * removed FUBA comments
         * corrected the transfered gamma-table for the C6 ASIC
         * corrected spelling variation of useful
   
   2003-09-24: René Rebe
         * fixed AV_BIG_SCAN_CMD feature overwrite
   
   2003-09-18: René Rebe
         * added 10 bytes start scan (for e.g. HP 74xxx)
   
   2003-09-10: Chris <chrisk@dsihi.com>
         * added missing break in set_gamma

   2003-08-30: René Rebe
         * added bytes swap debug message to calibration
   
   2003-07-27: René Rebe
         * merged the OPT_SOURCE code properly and so got rid of
           OPT_ADF and OPT_TRANS
         * finally got calibration right?
   
   2003-07-15: Newgen Software Pvt Ltd.
         * added a more common OPT_SOURCE
   
   2003-06-30: René Rebe
         * fixed big-endian issues
   
   2003-05-08: René Rebe
         * fixed crash when config is missing reported by Franz Bakan
   
   2003-03-25: René Rebe
         * urgs, no the 5300/5370 do _not_share the same ID, there are
           at least two different models sold ?!?
         * C5 set_window _hack_
   
   2003-03-24: René Rebe
         * request sense after usb error
         * provide a hack to limit the resolution for some scanners
         * urgs the HP 5300 and 5370 share the same ID ... :-(
   
   2003-03-23: René Rebe
         * finally native USB commands working!
   
   2003-03-19: René Rebe
         * avision_cmd rework for the USB SCSI emulation
   
   2003-03-18: René Rebe
         * started user-space USB support
         * added a bunch of new IDs from the linux kernel hpusbscsi.c
         * renamed attach to attach_scsi and added attach_usb
         * usb config parsing and made an line only containing a device
           name without the "scsi" keyword obsolete!
   
   2003-03-15: René Rebe
         * merged Jose Paulo Moitinho de Almeida Minolta Scan Dual II fixes
         * debug info
   
   2003-03-04: René Rebe
         * merged missing OS/2 line
   
   2003-03-03: René Rebe
         * merged OS/2 threading support
         * more calibratin debug output
         * fixed gamma-table for ASIC_C6 scanner
   
   2003-02-16: René Rebe
         * resolution change forces param-reload
   
   2003-02-15: René Rebe
         * set_window work for more correct data and Fujitsu ScanPartner
           compatibility
   
   2003-02-14: René Rebe
         * send the MSB of linecount and linewidth
         * code-cleanups
   
   2003-02-07: René Rebe
         * started max_request_size utilisation
   
   2003-01-19: René Rebe
         * rewrote sense code handling
         * size optimisation of new sense code
         * media_check work
   
   2002-12-26: René Rebe
         * cleanup of sort_and_average - now used in normal_calibration
         * rewrote compute_*_shading_target
   
   2002-12-25: René Rebe
         * code validation, corrections and cleanups (removed bubble_sort2)
         * duplicate code refactoring in normal_calibration
   
   2002-12-23: René Rebe
         * fixed possible seg-fault and the right shift in normal_calib
         * as well as other tweaks
   
   2002-12-13: René Rebe
         * cleanups and adaptions for HP 7400 (HP 7450)
   
   2002-11-30: René Rebe
         * added meta-info to auto-generate the avision.desc
   
   2002-11-29: René Rebe
         * wild guess for the HP 5370 problem
   
   2002-11-27: René Rebe
         * better range checks and default initialization
         * optimized color-packing algorithm
   
   2002-11-26: René Rebe
         * cleanup of duplicate values - prepare for x/y resolution difference
         * correct scan-window-size for different scan modes
         * limitting of scan-window with line-difference
         * correctly handle read returning with zero bytes as EOF
   
   2002-11-23: René Rebe
         * added ADF option, implemented line_pack
         * option descriptions cleanup
         * dynamic data_type_qualifier
         * disabled c5_guard
   
   2002-11-21/22: René Rebe
         * cleanups and more correct set_window
         * as well as many other cleanups
   
   2002-11-04: René Rebe
         * code and comment cleanups, made c5_guard the default
         * readded old "Rene" calibration (for tests)
         * added old-calibration and disaable-c5-guard config option
         * updated avision.conf and avision.man          
   
   2002-10-30: René Rebe
         * readded reject for film-scanner
         * added sane-flow from umax.c
   
   2002-10-29: René Rebe
         * cleanup of reader_process
         * fix in set_window
   
   2002-10-28: René Rebe
         * common debug prints
         * simplyfied c5_guard and mofed the call to the right location
   
   2002-10-25: René Rebe
         * added AV820C Plus ID
         * enabled preliminary c5_guard code
   
   2002-09-24: René Rebe
         * fixed gamma table for Cx ASIC devices (e.g. AV630C)
         * rewrote window computation and review many places where the avdimen
           are used
   
   2002-09-23: René Rebe
         * reenabled line-difference for hp5370c
   
   2002-09-22: René Rebe
         * cleanups
   
   2002-09-21: René Rebe
         * more debugging output
         * channel by channel feature for normal_calib
   
   2002-09-18: René Rebe
         * finished first normal and c5 calibration integration
         * factoring of the various _sort and read/send calib data helpers
         
   2002-09-17: René Rebe
         * overworked mojor parts of the code due to updated specs from
           Avision
         * replaced grey -> gray since the american form is used by SANE
         * adapted normal calibration from avision example code
         * common code factoring - c5 calibration adaption
   
   2002-09-15: René Rebe
         * added more inquiry info from new Spec
         * improved debug_output_raw, fixed typos
         * added calib2 auto-detection
         * go_home only for film-scanners (not needed and hangs the AV8000S)
   
   2002-09-10: René Rebe
         * added "AV8000S" ID
   
   2002-08-25: René Rebe
         * added "AV620CS Plus" ID and fixed typo in the .conf file
   
   2002-08-14: René Rebe
         * implemented ADF support
         * reworked "go_home" and some indenting
   
   2002-07-12: René Rebe
         * implemented accessories detection
         * code cleanups
   
   2002-07-07: René Rebe
         * manpage and .desc update
   
   2002-06-30: René Rebe
         * limited calibration only if specified in config file
         * more readable calibration decision (using goto ...)
   
   2002-06-24: René Rebe
         * fixed some comment typos
         * fixed the image size calculation
         * fixed gamma_table computation
   
   2002-06-14: René Rebe
         * better debug priority in the reader
         * fixed gamma-table
         * suppressed
   
   2002-06-04: René Rebe
         * fixed possible memory-leak for error situations
         * fixed some compiler warnings
         * introduced new gamma-table send variant
         * introduced calibration on first scan only
   
   2002-06-03: René Rebe
         * maybe fixed the backend for HP 5370 scanners
         * fixed some typos - debug output
   
   2002-05-27: René Rebe
         * marked HP 5370 to be calib v2 and improved the .conf file
         * calibration update
   
   2002-05-15: René Rebe
         * merged HP 7400 fixes
         * reworked gamma_table and some other variable usage fixes
         * merged file holder check in start_scan
   
   2002-04-25: Frank Zago
         * fixed usage of size_t
   
   2002-04-14: Frank Zago
         * fix more memory leaks
         * add the paper test
         * fix a couple bug on the error path in sane_init
   
   2002-04-13: René Rebe
         * added many more scanner IDs
   
   2002-04-11: René Rebe
         * fixed dpi for sheetfeed scanners - other cleanups
         * fixed attach to close the filehandle if no scanner was found
   
   2002-04-08: Frank Zago
         * Device_List reorganization, attach cleanup, gcc warning
           elimination
   
   2002-04-04: René Rebe
         * added the code for 3-channel color calibration
   
   2002-04-03: René Rebe
         * added Mitsubishi IDs
   
   2002-03-25: René Rebe
         * added Jose's new calibration computation
         * prepared Umax IDs
   
   2002-03-03: René Rebe
         * more calibration analyzing and implementing
         * moved set_window after the calibration and gamma-table
         * replaced all unsigned char in stucts with u_int8_t
         * removed braindead ifdef which excluded imortant bits in the
           command_set_window_window_descriptor struct!
         * perform_calibration cleanup
   
   2002-02-19: René Rebe
         * added disable-calibration option
         * some cleanups and man-page and avision.desc update
   
   2002-02-18: René Rebe
         * more calibration hacking/adaption/testing
   
   2002-02-18: Jose Paulo Moitinho de Almeida
         * fixed go_home
         * film holder control update
   
   2002-02-15: René Rebe
         * cleanup of many details like: reget frame_info in set_frame,
           resolution
           computation for different scanners, using the scan command for new
           scanners again, changed types to make the gcc happy ...
   
   2002-02-14: Jose Paulo Moitinho de Almeida
         * film holder control update
   
   2002-02-12: René Rebe
         * further calibration testing / implementing
         * merged the film holder control
         * attach and other function cleanup
         * added a timeout to wait_4_light
         * only use the scan command for old protocol scanners (it hangs the
           HP 7400)
   
   2002-02-10: René Rebe
         * fixed some typos in attach, added version output
   
   2002-02-10: René Rebe
         * next color-pack try, rewrote most of the image data path
         * implemented 12 bit gamma-table (new protocol)
         * removed the allow-usb option
         * fixed 1200 dpi scanning (was a simple option alignment issue)
   
   2002-02-09: René Rebe
         * adapted attach for latest HP scanner tests
         * rewrote the window coordinate computation
         * removed some double, misleading variables
         * rewrote some code
   
   2002-02-08: Jose Paulo Moitinho de Almeida
         * implemented film holder control
   
   2002-01-18: René Rebe
         * removed sane_stop and fixed some names
         * much more _just for fun_ cleanup work
         * fixed sane_cancel to not hang - but cancel a scan
         * introduced a disable-gamma-table option (removed the option stuff)
         * added comments for the options into the avision.conf file
   
   2002-01-17: René Rebe
         * fixed set_window to not call exit
   
   2002-01-16: René Rebe
         * some cleanups and printf removal
   
   2001-12-11: René Rebe
         * added some fixes
   
   2001-12-11: Jose Paulo Moitinho de Almeida
         * fixed some typos
         * updated perform_calibration
         * added go_home
   
   2001-12-10: René Rebe
         * fixed some typos
         * added some TODO notes where we need to call some new_protocol
           funtions
         * updated man-page
   
   2001-12-06: René Rebe
         * redefined Avision_Device layout
         * added allow-usb config option
         * added inquiry parameter saving and handling in the SANE functions
         * removed Avision_Device->pass (3-pass was never implemented ...)
         * merged test_light ();
   
   2001: René Rebe and Martin Jelínek
         * started a real change-log
         * added force-a4 config option
         * added gamma-table support
         * added pretty inquiry data debug output
         * USB and calibration data testing
   
   2000:
         * minor bug fixing
   
   1999:
         * initial write
   
********************************************************************************/

/* SANE-FLOW-DIAGRAMM (from umax.c)
 *
 * - sane_init() : initialize backend, attach scanners(devicename,0)
 * . - sane_get_devices() : query list of scanner-devices
 * . - sane_open() : open a particular scanner-device and attach_scanner(devicename,&dev)
 * . . - sane_set_io_mode : set blocking-mode
 * . . - sane_get_select_fd : get scanner-fd
 * . . - sane_get_option_descriptor() : get option information
 * . . - sane_control_option() : change option values
 * . .
 * . . - sane_start() : start image aquisition
 * . .   - sane_get_parameters() : returns actual scan-parameters
 * . .   - sane_read() : read image-data (from pipe)
 *
 * in ADF mode this is done often:
 * . . - sane_start() : start image aquisition
 * . .   - sane_get_parameters() : returns actual scan-parameters
 * . .   - sane_read() : read image-data (from pipe)
 *
 * . . - sane_cancel() : cancel operation, kill reader_process
 *
 * . - sane_close() : close opened scanner-device, do_cancel, free buffer and handle
 * - sane_exit() : terminate use of backend, free devicename and device-struture
 */

#include <sane/config.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>

#include <math.h>

#define BACKEND_NAME avision
#define BACKEND_BUILD 167 /* avision backend BUILD version */

#include <sane/sane.h>
#include <sane/sanei.h>
#include <sane/saneopts.h>
#include <sane/sanei_thread.h>
#include <sane/sanei_scsi.h>
#include <sane/sanei_usb.h>
#include <sane/sanei_config.h>
#include <sane/sanei_backend.h>

#include <avision.h>

/* For timeval... */
#ifdef DEBUG
#include <sys/time.h>
#endif

/* Attention: The comments must stay as they are - they are automatially parsed
   to generate the SANE avision.desc file, as well as HTML online content! */

static Avision_HWEntry Avision_Device_List [] =
  {
    { "AVISION", "AV100CS",
      0, 0,
      "Avision", "AV100CS",
      0},
    /* status="untested" */
    
    { "AVISION", "AV100IIICS",
      0, 0,
      "Avision", "AV100IIICS",
      0},
    /* status="untested" */
    
    { "AVISION", "AV100S",
      0, 0,
      "Avision", "AV100S",
      0},
    /* status="untested" */

    { "AVISION", "AV120",
      0x0638, 0x0A27,
      "Avision", "AV120",
      AV_INT_BUTTON},
    /* comment="sheetfed scanner" */
    /* status="basic" */

    { "AVISION", "AV210",
      0x0638, 0x0A24,
      "Avision", "AV210",
      AV_INT_BUTTON},
    /* comment="sheetfed scanner" */
    /* status="basic" */
    
    { "AVISION", "AV210",
      0x0638, 0x0A25,
      "Avision", "AV210-2",
      AV_INT_BUTTON},
    /* comment="sheetfed scanner" */
    /* status="basic" */

    { "AVISION", "AVISION AV220",
      0x0638, 0x0A23,
      "Avision", "AV220",
      AV_INT_BUTTON},
    /* comment="duplex! sheetfed scanner" */
    /* status="complete" */

    { "AVISION", "AVISION AV220C2",
      0x0638, 0x0A2A,
      "Avision", "AV220-C2",
      AV_INT_BUTTON},
    /* comment="duplex! sheetfed scanner" */
    /* status="untested" */

    { "AVISION", "AV240SC",
      0, 0,
      "Avision", "AV240SC",
      0},
    /* status="untested" */
    
    { "AVISION", "AV260CS",
      0, 0,
      "Avision", "AV260CS",
      0},
    /* status="untested" */
    
    { "AVISION", "AV360CS",
      0, 0,
      "Avision", "AV360CS",
      0},
    /* status="untested" */
    
    { "AVISION", "AV363CS",
      0, 0,
      "Avision", "AV363CS",
      0},
    /* status="untested" */
    
    { "AVISION", "AV420CS",
      0, 0,
      "Avision", "AV420CS",
      0},
    /* status="untested" */
    
    { "AVISION", "AV6120",
      0, 0,
      "Avision", "AV6120",
      0},
    /* status="untested" */
    
    { "AVISION", "AV610",
      0x638, 0xa19,
      "Avision", "AV610",
      AV_GRAY_CALIB_BLUE | AV_ACCEL_TABLE},
    /* status="good" */
      
    /* { "AVISION", "AVA6", 
      0x638, 0xa22,
      "Avision", "AVA6",
      0}, */
    /* comment="probably a CPU-less device" */
    /* status="untested" */
   
    { "AVision", "DS610CU",
      0x638, 0xa16,
      "Avision", "DS610CU Scancopier",
      0},
    /* comment="1 pass, 600 dpi, A4" */
    /* status="good" */
 
    { "AVISION", "AV620CS",
      0, 0,
      "Avision", "AV620CS",
      0},
    /* comment="1 pass, 600 dpi" */
    /* status="complete" */
    
    { "AVISION", "AV620CS Plus",
      0, 0,
      "Avision", "AV620CS Plus",
      0},
    /* comment="1 pass, 1200 dpi" */
    /* status="complete" */
    
    { "AVISION", "AV630CS",
      0, 0,
      "Avision", "AV630CS",
      0},
    /* comment="1 pass, 1200 dpi - regularly tested" */
    /* status="complete" */
    
    { "AVISION", "AV630CSL",
      0, 0,
      "Avision", "AV630CSL",
      0},
    /* comment="1 pass, 1200 dpi" */
    /* status="untested" */
    
    { "AVISION", "AV6240",
      0, 0,
      "Avision", "AV6240",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="complete" */
    
    { "AVISION", "AV600U",
      0x0638, 0x0A13,
      "Avision", "AV600U",
      0},
    /* comment="1 pass, 600 dpi" */
    /* status="good" */

    { "AVISION", "AV600U Plus",
      0x0638, 0x0A18,
      "Avision", "AV600U Plus",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="untested" */
    
    { "AVISION", "AV660S",
      0, 0,
      "Avision", "AV660S",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="untested" */

    { "AVISION", "AV680S",
      0, 0,
      "Avision", "AV680S",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="untested" */
    
    { "AVISION", "AV690U",
      0, 0,
      "Avision", "AV690U",
      0},
    /* comment="1 pass, 2400 dpi" */
    /* status="untested" */
    
    { "AVISION", "AV800S",
      0, 0,
      "Avision", "AV800S",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="untested" */
    
    { "AVISION", "AV810C",
      0, 0,
      "Avision", "AV810C",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="untested" */
    
    { "AVISION", "AV820",
      0, 0,
      "Avision", "AV820",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="untested" */
    
    { "AVISION", "AV820C",
      0, 0,
      "Avision", "AV820C",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="good" */
    
    { "AVISION", "AV820C Plus",
      0, 0,
      "Avision", "AV820C Plus",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="untested" */
    
    { "AVISION", "AV830C",
      0, 0,
      "Avision", "AV830C",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="untested" */
    
    { "AVISION", "AV830C Plus",
      0, 0,
      "Avision", "AV830C Plus",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="untested" */
    
    { "AVISION", "AV880",
      0, 0,
      "Avision", "AV880",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="untested" */
    
    { "AVISION", "AV880C",
      0, 0,
      "Avision", "AV880C",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="untested" */

    { "AVISION", "AV3200C",
      0, 0,
      "Avision", "AV3200C",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="complete" */

    { "AVISION", "AV3800C",
      0, 0,
      "Avision", "AV3800C",
      0},
    /* comment="1 pass, ??? dpi" */
    /* status="good" */

    { "AVISION", "AV8000S",
      0, 0,
      "Avision", "AV8000S",
      0},
    /* comment="1 pass, 1200 dpi, A3 - regularly tested" */
    /* status="complete" */

    { "AVISION", "AV8300",
       0x0638, 0x0A40,
      "Avision", "AV8300",
      0},
    /* comment="1 pass, 1200 dpi, A3 - duplex!" */
    /* status="complete" */

    { "AVISION", "AV8350",
       0x0638, 0x0A68,
      "Avision", "AV8350",
      0},
    /* comment="1 pass, 1200 dpi, A3 - duplex!" */
    /* status="complete" */

    { "AVISION", "AVA3",
      0, 0,
      "Avision", "AVA3",
      AV_FORCE_A3},
    /* comment="1 pass, 600 dpi, A3" */
    /* status="basic" */

    /* and possibly more avisions ;-) */
    
    { "HP",      "ScanJet 5300C",
      0x03f0, 0x0701,
      "Hewlett-Packard", "ScanJet 5300C",
      0},
    /* comment="1 pass, 2400 dpi - regularly tested - some FW revisions have x-axis image scaling problems over 1200 dpi" */
    /* status="complete" */

    { "HP",      "ScanJet 5370C",
      0x03f0, 0x0701,
      "Hewlett-Packard", "ScanJet 5370C",
      0},
    /* comment="1 pass, 2400 dpi - some FW revisions have x-axis image scaling problems over 1200 dpi" */
    /* status="basic" */
    
    { "hp",      "scanjet 7400c",
      0x03f0, 0x0801,
      "Hewlett-Packard", "ScanJet 7400c",
      AV_FIRMWARE},
    /* comment="1 pass, 2400 dpi - dual USB/SCSI interface" */
    /* status="good" */
    
#ifdef FAKE_ENTRIES_FOR_DESC_GENERATION
    { "hp",      "scanjet 7450c",
      0x03f0, 0x0801,
      "Hewlett-Packard", "ScanJet 7450c",
      AV_FIRMWARE},
    /* comment="1 pass, 2400 dpi - dual USB/SCSI interface - regularly tested" */
    /* status="good" */
    
    { "hp",      "scanjet 7490c",
      0x03f0, 0x0801,
      "Hewlett-Packard", "ScanJet 7490c",
      AV_FIRMWARE},
    /* comment="1 pass, 1200 dpi - dual USB/SCSI interface" */
    /* status="good" */

#endif
    { "HP",      "C9930A",
      0x03f0, 0x0b01,
      "Hewlett-Packard", "ScanJet 8200",
      AV_FIRMWARE},
    /* comment="1 pass, 4800 (?) dpi - USB 2.0" */
    /* status="good" */

#ifdef FAKE_ENTRIES_FOR_DESC_GENERATION
    { "HP",      "C9930A",
      0x03f0, 0x0b01,
      "Hewlett-Packard", "ScanJet 8250",
      AV_FIRMWARE},
    /* comment="1 pass, 4800 (?) dpi - USB 2.0" */
    /* status="good" */

    { "HP",      "C9930A",
      0x03f0, 0x0b01,
      "Hewlett-Packard", "ScanJet 8290",
      AV_FIRMWARE},
    /* comment="1 pass, 4800 (?) dpi - USB 2.0 and SCSI - only SCSI tested so far" */
    /* status="good" */
    
#endif 
    
    { "Minolta", "#2882",
      0, 0,
      "Minolta", "Dimage Scan Dual I",
      AV_FORCE_FILM}, /* not AV_FILMSCANNER (no frame control) */
    /* status="basic" */
    
    { "MINOLTA", "FS-V1",
      0x0638, 0x026a,
      "Minolta", "Dimage Scan Dual II",
      AV_FILMSCANNER}, /* maybe AV_ONE_CALIB_CMD */
    /* comment="1 pass, film-scanner" */
    /* status="good" */
    
    { "MINOLTA", "Elite II",
      0x0686, 0x4004,
      "Minolta", "Elite II",
      AV_FILMSCANNER | AV_ONE_CALIB_CMD},
    /* comment="1 pass, film-scanner" */
    /* status="untested" */
    
    { "MINOLTA", "FS-V3",
      0x0686, 0x400d,
      "Minolta", "Dimage Scan Dual III",
      AV_FILMSCANNER | AV_ONE_CALIB_CMD},
    /* comment="1 pass, film-scanner" */
    /* status="basic" */

    { "QMS", "SC-110",
      0x0638, 0x0a15,
      "Minolta-QMS", "SC-110",
      0},
    /* comment="" */
    /* status="untested" */

    { "QMS", "SC-215",
      0x0638, 0x0a16,
      "Minolta-QMS", "SC-215",
      0},
    /* comment="" */
    /* status="good" */
    
    { "MITSBISH", "MCA-ADFC",
      0, 0,
      "Mitsubishi", "MCA-ADFC",
      0},
    /* status="untested" */
    
    { "MITSBISH", "MCA-S1200C",
      0, 0,
      "Mitsubishi", "S1200C",
      0},
    /* status="untested" */
    
    { "MITSBISH", "MCA-S600C",
      0, 0,
      "Mitsubishi", "S600C",
      0},
    /* status="untested" */
    
    { "MITSBISH", "SS600",
      0, 0,
      "Mitsubishi", "SS600",
      0},
    /* status="good" */
    
    /* The next are all untested ... */
    
    { "FCPA", "ScanPartner",
      0, 0,
      "Fujitsu", "ScanPartner",
      AV_FUJITSU},
    /* status="untested" */

    { "FCPA", "ScanPartner 10",
      0, 0,
      "Fujitsu", "ScanPartner 10",
      AV_FUJITSU},
    /* status="untested" */
    
    { "FCPA", "ScanPartner 10C",
      0, 0,
      "Fujitsu", "ScanPartner 10C",
      AV_FUJITSU},
    /* status="untested" */
    
    { "FCPA", "ScanPartner 15C",
      0, 0,
      "Fujitsu", "ScanPartner 15C",
      AV_FUJITSU},
    /* status="untested" */
    
    { "FCPA", "ScanPartner 300C",
      0, 0,
      "Fujitsu", "ScanPartner 300C",
      0},
    /* status="untested" */
    
    { "FCPA", "ScanPartner 600C",
      0, 0,
      "Fujitsu", "ScanPartner 600C",
      0},
    /* status="untested" */

    { "FCPA", "ScanPartner 620C",
      0, 0,
      "Fujitsu", "ScanPartner 620C",
      0},
    /* status="good" */
    
    { "FCPA", "ScanPartner Jr",
      0, 0,
      "Fujitsu", "ScanPartner Jr",
      0},
    /* status="untested" */
    
    { "FCPA", "ScanStation",
      0, 0,
      "Fujitsu", "ScanStation",
      0},
    /* status="untested" */

    { "Kodak", "i30",
      0, 0,
      "Kodak", "i30",
      0},
    /* status="untested" */

    { "Kodak", "i40",
      0, 0,
      "Kodak", "i40",
      0},
    /* status="untested" */
    
    { "Kodak", "i50",
      0, 0,
      "Kodak", "i50",
      0},
    /* status="untested" */
    
    { "Kodak", "i60",
      0, 0,
      "Kodak", "i60",
      0},
    /* status="untested" */
    
    { "Kodak", "i80",
      0, 0,
      "Kodak", "i80",
      0},
    /* status="untested" */
    
    { "iVina", "1200U",
      0x0638, 0x0268,
      "iVina", "1200U",
      0},
    /* status="untested" */
    
    { "XEROX", "DocuMate252",
      0x04a7, 0x0449,
      "Xerox", "DocuMate252",
      0},
    /* status="untested" */
    
    { "XEROX", "DocuMate262",
      0x04a7, 0x044c,
      "Xerox", "DocuMate262",
      0},
    /* status="untested" */
    
    /* More IDs from the Avision dll:
       ArtiScan ProA3
       FB1065
       FB1265
       PHI860S
       PSDC SCSI
       SCSI Scan 19200
       V6240 */
    
    /* last entry detection */
    { NULL, NULL,
      0, 0,
      NULL, NULL,
      0} 
  };

/* This is just for reference and future review - this is NOT used */

/* static const char c7_flash_ram [] = {
  0x00, 0xff, 0xff, 0x08, 0x53, 0x63, 0x61, 0x6e,
  0x6e, 0x65, 0x6e, 0x14}; */

/* a set of NV-RAM data obtained from the Avision driver */

/*
  Scannen
  SCN2CSS0279LZ
  Initialisierung
  Bereit
  Bitte entriegeln
  Fehler
  SCSI-Adresse:
  Energiesparmodus
  Scannen
  ADF-Papierstau */

/* static const char c7_nvram [] = {
  0xff, 0xff, 0x00, 0x00, 0x01, 0xb0, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c,
  0x00, 0x1c, 0x07, 0xd2, 0x00, 0x03, 0x00, 0x05,
  0x00, 0x03, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8a, 0x7f,
  0x53, 0x43, 0x4e, 0x32, 0x43, 0x53, 0x53, 0x30,
  0x32, 0x37, 0x39, 0x4c, 0x5a, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x49, 0x6e, 0x69, 0x74, 0x69, 0x61,
  0x6c, 0x69, 0x73, 0x69, 0x65, 0x72, 0x75, 0x6e,
  0x67, 0x14, 0x00, 0x20, 0x20, 0x20, 0x42, 0x65,
  0x72, 0x65, 0x69, 0x74, 0x00, 0x42, 0x69, 0x74,
  0x74, 0x65, 0x20, 0x65, 0x6e, 0x74, 0x72, 0x69,
  0x65, 0x67, 0x65, 0x6c, 0x6e, 0x00, 0x46, 0x65,
  0x68, 0x6c, 0x65, 0x72, 0x00, 0x53, 0x43, 0x53,
  0x49, 0x2d, 0x41, 0x64, 0x72, 0x65, 0x73, 0x73,
  0x65, 0x3a, 0x00, 0x45, 0x6e, 0x65, 0x72, 0x67,
  0x69, 0x65, 0x73, 0x70, 0x61, 0x72, 0x6d, 0x6f,
  0x64, 0x75, 0x73, 0x00, 0x53, 0x63, 0x61, 0x6e,
  0x6e, 0x65, 0x6e, 0x14, 0x00, 0x41, 0x44, 0x46,
  0x2d, 0x50, 0x61, 0x70, 0x69, 0x65, 0x72, 0x73,
  0x74, 0x61, 0x75, 0x00, 0x00, 0x00, 0x5c, 0x5c,
  0x2e, 0x5c, 0x55, 0x73, 0x62, 0x73, 0x63, 0x61,
  0x6e, 0x30, 0x00, 0xf5, 0x64, 0x00, 0x28, 0xb8,
  0xf7, 0xbf, 0x34, 0x32, 0x8a, 0x81, 0xf7, 0x41,
  0xf7, 0xbf, 0x90, 0x94, 0xfc, 0xbf, 0x4e, 0xb8,
  0xf7, 0xbf, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,
  0x00, 0x00, 0xb0, 0xf6, 0x57, 0xc1, 0x09, 0x00,
  0x00, 0x00, 0x68, 0x2c, 0xf9, 0xbf, 0xab, 0xf5,
  0x64, 0x00, 0x29, 0x37, 0x04, 0x02, 0x5c, 0x45,
  0x03, 0x02, 0x5c, 0x00, 0x00, 0x00, 0xf9, 0x83,
  0x03, 0x02, 0x2c, 0x37, 0x04, 0x02, 0x9f, 0x7d}; */

/* used when scanner returns invalid range fields ... */
#define A4_X_RANGE 8.5 /* or 8.25 ? */
#define A4_Y_RANGE 11.8
#define A3_X_RANGE 11.8
#define A3_Y_RANGE 16.5 /* or 17 ? */
#define FILM_X_RANGE 1.0 /* really ? */
#define FILM_Y_RANGE 1.0
#define SHEETFEED_Y_RANGE 14.0

#define AVISION_CONFIG_FILE "avision.conf"

#define MM_PER_INCH 25.4
#define AVISION_BASE_RES 300

/* calibration (shading) defines */

#define INVALID_WHITE_SHADING   0x0000
#define DEFAULT_WHITE_SHADING   0xFFF0

#define MAX_WHITE_SHADING       0xFFFF
/* originally the WHITE_MAP_RANGE was 0x4000 - but this always resulted in
 * slightly too dark images - thus I have choosen 0x4FFF ...*/
#define WHITE_MAP_RANGE         0x4FFF
#define WHITE_MAP_RANGE_ORIG    0x4000

#define INVALID_DARK_SHADING    0xFFFF
#define DEFAULT_DARK_SHADING    0x0000

#define read_constrains(s,var) {\
	if (s->hw->inquiry_asic_type == AV_ASIC_C6) {\
		if (var % 64 == 0) var /= 2;\
		if (var % 64 == 0) var -= 2;\
	}\
}\

static int num_devices;
static Avision_Device* first_dev;
static Avision_Scanner* first_handle;
static const SANE_Device** devlist = 0;

/* this is a bit hacky to get extra information in the attach callback */
static Avision_HWEntry* attaching_hw = 0;

/* disable the usage of a custom gamma-table */
static SANE_Bool disable_gamma_table = SANE_FALSE;

/* disable the calibration */
static SANE_Bool disable_calibration = SANE_FALSE;

/* do only one claibration per backend initialization */
static SANE_Bool one_calib_only = SANE_FALSE;

/* force scanable areas to ISO(DIN) A4/A3 */
static SANE_Bool force_a4 = SANE_FALSE;
static SANE_Bool force_a3 = SANE_FALSE;

static SANE_Bool static_calib_list[3] =
  {
    SANE_FALSE, SANE_FALSE, SANE_FALSE
  };

static const SANE_Range u8_range =
  {
    0, /* minimum */
    255, /* maximum */
    0 /* quantization */
  };

static const SANE_Range percentage_range =
  {
    SANE_FIX (-100), /* minimum */
    SANE_FIX (100), /* maximum */
    SANE_FIX (1) /* quantization */
  };

static const SANE_Range abs_percentage_range =
  {
    SANE_FIX (0), /* minimum */
    SANE_FIX (100), /* maximum */
    SANE_FIX (1) /* quantization */
  };

static const u_int8_t test_unit_ready[] =
  {
    AVISION_SCSI_TEST_UNIT_READY, 0x00, 0x00, 0x00, 0x00, 0x00
  };

static const u_int8_t get_status[] =
  {
    AVISION_SCSI_GET_DATA_STATUS, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x0c, 0x00
  };

static size_t
max_string_size (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  int i;
  
  DBG (3, "max_string_size:\n");
  
  for (i = 0; strings[i]; ++ i) {
    size = strlen (strings[i]) + 1;
    if (size > max_size)
      max_size = size;
  }
  return max_size;
}

static SANE_Status
constrain_value (Avision_Scanner* s, SANE_Int option, void* value,
		 SANE_Int* info)
{
  DBG (3, "constrain_value:\n");
  return sanei_constrain_value (s->opt + option, value, info);
}

static void debug_print_raw (int dbg_level, char* info, const u_int8_t* data,
			     size_t count)
{
  size_t i;
  
  DBG (dbg_level, info);
  for (i = 0; i < count; ++ i) {
    DBG (dbg_level, "  [%lu] %1d%1d%1d%1d%1d%1d%1d%1db %3oo %3dd %2xx\n",
	 (u_long) i,
	 BIT(data[i],7), BIT(data[i],6), BIT(data[i],5), BIT(data[i],4),
	 BIT(data[i],3), BIT(data[i],2), BIT(data[i],1), BIT(data[i],0),
	 data[i], data[i], data[i]);
  }
}

static void debug_print_avdimen (int dbg_level, char* func,
				 Avision_Dimensions* avdimen)
{
  DBG (dbg_level, "%s: tlx: %ld, tly: %ld, brx: %ld, bry: %ld\n",
       func, avdimen->tlx, avdimen->tly,
       avdimen->brx, avdimen->bry);
  DBG (dbg_level, "%s: xres: %d, yres: %d, line_difference: %d\n",
       func, avdimen->xres, avdimen->yres, avdimen->line_difference);
}

static void debug_print_params (int dbg_level, char* func, SANE_Parameters* params)
{
  DBG (dbg_level, "%s: pixel_per_line: %d, lines: %d\n",
       func, params->pixels_per_line, params->lines);

  DBG (dbg_level, "%s: depth: %d, bytes_per_line: %d\n",
       func, params->depth, params->bytes_per_line);
}

static void debug_print_calib_format (int dbg_level, char* func,
				      u_int8_t* result)
{
  debug_print_raw (dbg_level + 2, "debug_print_calib_format:\n", result, 32);
  
  DBG (dbg_level, "%s: [0-1]  pixels per line: %d\n",
       func, get_double ( &(result[0]) ));
  DBG (dbg_level, "%s: [2]    bytes per channel: %d\n", func, result[2]);
  DBG (dbg_level, "%s: [3]    line count: %d\n", func, result[3]);
  
  DBG (dbg_level, "%s: [4]    FLAG:%s%s%s\n",
       func,
       result[4] == 1?" MUST_DO_CALIBRATION":"",
       result[4] == 2?" SCAN_IMAGE_DOES_CALIBRATION":"",
       result[4] == 3?" NEEDS_NO_CALIBRATION":"");
  
  DBG (dbg_level, "%s: [5]    Ability1:%s%s%s%s%s%s%s%s\n",
       func,
       BIT(result[5],7)?" NONE_PACKED":" PACKED",
       BIT(result[5],6)?" INTERPOLATED":"",
       BIT(result[5],5)?" SEND_REVERSED":"",
       BIT(result[5],4)?" PACKED_DATA":"",
       BIT(result[5],3)?" COLOR_CALIB":"",
       BIT(result[5],2)?" DARK_CALIB":"",
       BIT(result[5],1)?" NEEDS_WHITE_BLACK_SHADING_DATA":"",
       BIT(result[5],0)?" NEEDS_CALIB_TABLE_CHANNEL_BY_CHANNEL":"");
  
  DBG (dbg_level, "%s: [6]    R gain: %d\n", func, result[6]);
  DBG (dbg_level, "%s: [7]    G gain: %d\n", func, result[7]);
  DBG (dbg_level, "%s: [8]    B gain: %d\n", func, result[8]);
  
  DBG (dbg_level, "%s: [9-10] R shading target: %x\n",
       func, get_double ( &(result[9]) ) );
  DBG (dbg_level, "%s: [11-12] G shading target: %x\n",
       func, get_double ( &(result[11]) ) );
  DBG (dbg_level, "%s: [13-14] B shading target: %x\n",
       func, get_double ( &(result[13]) ) );
  
  DBG (dbg_level, "%s: [15-16] R dark shading target: %x\n",
       func, get_double ( &(result[15]) ) );
  DBG (dbg_level, "%s: [17-18] G dark shading target: %x\n",
       func, get_double ( &(result[17]) ) );
  DBG (dbg_level, "%s: [19-20] B dark shading target: %x\n",
       func, get_double ( &(result[19]) ) );
}

static void debug_print_accel_info (int dbg_level, char* func,
				    u_int8_t* result)
{
  debug_print_raw (dbg_level + 2, "debug_print_accel_info:\n", result, 24);
  
  DBG (dbg_level, "%s: [0-1]   acceleration step count: %d\n",
       func, get_double ( &(result[0]) ));
  DBG (dbg_level, "%s: [2-3]   stable step count: %d\n",
       func, get_double ( &(result[2]) ));
  DBG (dbg_level, "%s: [4-7]   table units: %d\n",
       func, get_quad ( &(result[4]) ));
  DBG (dbg_level, "%s: [8-11]  base units: %d\n",
       func, get_quad ( &(result[8]) ));
  DBG (dbg_level, "%s: [12-13] start speed: %d\n",
       func, get_double ( &(result[12]) ));
  DBG (dbg_level, "%s: [14-15] target speed: %d\n",
       func, get_double ( &(result[14]) ));
  DBG (dbg_level, "%s: [16]    ability:%s%s\n",
       func,
       BIT(result[16],0)?" TWO_BYTES_PER_ELEM":" SINGLE_BYTE_PER_ELEM",
       BIT(result[16],1)?" LOW_HIGH_ORDER":" HIGH_LOW_ORDER");
  DBG (dbg_level, "%s: [17]    table count: %d\n", func, result[17]);
  
}

static void debug_print_window_descriptor (int dbg_level, char* func,
					   command_set_window_window* window)
{
  debug_print_raw (dbg_level + 1, "window_data_header: \n",
		   (u_int8_t*)(&window->header),
		   sizeof(window->header));

  debug_print_raw (dbg_level + 1, "window_descriptor: \n",
		   (u_int8_t*)(&window->descriptor),
		   sizeof(*window) -
		   sizeof(window->header));
  
  DBG (dbg_level, "%s: [0]     window_id: %d\n", func,
       window->descriptor.winid);
  DBG (dbg_level, "%s: [2-3]   x-axis res: %d\n", func,
       get_double (window->descriptor.xres));
  DBG (dbg_level, "%s: [4-5]   y-axis res: %d\n", func,
       get_double (window->descriptor.yres));
  DBG (dbg_level, "%s: [6-9]   x-axis upper left: %d\n",
       func, get_quad (window->descriptor.ulx));
  DBG (dbg_level, "%s: [10-13] y-axis upper left: %d\n",
       func, get_quad (window->descriptor.uly));
  DBG (dbg_level, "%s: [14-17] window width: %d\n", func,
       get_quad (window->descriptor.width));
  DBG (dbg_level, "%s: [18-21] window length: %d\n", func, 
       get_quad (window->descriptor.length));
  DBG (dbg_level, "%s: [22]    brightness: %d\n", func,
       window->descriptor.brightness);
  DBG (dbg_level, "%s: [23]    threshold: %d\n", func,
       window->descriptor.threshold);
  DBG (dbg_level, "%s: [24]    contrast: %d\n", func,
       window->descriptor.contrast);
  DBG (dbg_level, "%s: [25]    image composition: %x\n", func,
       window->descriptor.image_comp);
  DBG (dbg_level, "%s: [26]    bits per channel: %d\n", func,
       window->descriptor.bpc);
  DBG (dbg_level, "%s: [27-28] halftone pattern: %x\n", func,
       get_double (window->descriptor.halftone));
  DBG (dbg_level, "%s: [29]    padding_and_bitset: %x\n", func,
       window->descriptor.padding_and_bitset);
  DBG (dbg_level, "%s: [30-31] bit ordering: %x\n", func,
       get_double (window->descriptor.bitordering));
  DBG (dbg_level, "%s: [32]    compression type: %x\n", func,
       window->descriptor.compr_type);
  DBG (dbg_level, "%s: [33]    compression argument: %x\n", func,
       window->descriptor.compr_arg);
  DBG (dbg_level, "%s: [34-35] paper length: %x\n", func,
       get_double (window->descriptor.paper_length) );
  DBG (dbg_level, "%s: [40]    vendor id: %x\n", func,
       window->descriptor.vendor_specific);
  DBG (dbg_level, "%s: [41]    param lenght: %d\n", func,
       window->descriptor.paralen);
  DBG (dbg_level, "%s: [42]    bitset1: %x\n", func,
       window->avision.bitset1);
  DBG (dbg_level, "%s: [43]    highlight: %d\n", func,
       window->avision.highlight);
  DBG (dbg_level, "%s: [44]    shadow: %d\n", func,
       window->avision.shadow);
  DBG (dbg_level, "%s: [45-46] line-width: %d\n", func,
       get_double (window->avision.line_width));
  DBG (dbg_level, "%s: [47-48] line-count: %d\n", func,
       get_double (window->avision.line_count));
  DBG (dbg_level, "%s: [49]    bitset2: %x\n", func,
       window->avision.type.normal.bitset2);
  DBG (dbg_level, "%s: [50]    ir exposure time: %x\n",
       func, window->avision.type.normal.ir_exposure_time);
  
  DBG (dbg_level, "%s: [51-52] r exposure: %x\n", func,
       get_double (window->avision.type.normal.r_exposure_time));
  DBG (dbg_level, "%s: [53-54] g exposure: %x\n", func,
       get_double (window->avision.type.normal.g_exposure_time));
  DBG (dbg_level, "%s: [55-56] b exposure: %x\n", func,
       get_double (window->avision.type.normal.b_exposure_time));
  
  DBG (dbg_level, "%s: [57]    bitset3: %x\n", func,
       window->avision.type.normal.bitset3);
  DBG (dbg_level, "%s: [58]    auto focus: %d\n", func,
       window->avision.type.normal.auto_focus);
  DBG (dbg_level, "%s: [59]    line-widht (MSB): %d\n",
       func, window->avision.type.normal.line_width_msb);
  DBG (dbg_level, "%s: [60]    line-count (MSB): %d\n",
       func, window->avision.type.normal.line_count_msb);
  DBG (dbg_level, "%s: [61]    edge threshold: %d\n",
       func, window->avision.type.normal.edge_threshold);
}

static u_int32_t create_cksum (Avision_Scanner* s)
{
  u_int32_t x = 0;
  size_t i;
  for (i = 0; i < sizeof(s->params); ++i)
    x += ((unsigned char*) (&s->params)) [i] * i;
  
  x += s->c_mode * i++;
  x += s->source_mode * i++;
  
  return x;
}

static SANE_Status
sense_handler (int fd, u_char* sense, void* arg)
{
  SANE_Status status = SANE_STATUS_IO_ERROR; /* default case */
  
  char* text;
  
  u_int8_t error_code = sense[0] & 0x7f;
  u_int8_t sense_key = sense[2] & 0xf;
  u_int8_t additional_sense = sense[7];
  
  fd = fd; /* silence gcc */
  arg = arg; /* silence gcc */
  
  DBG (3, "sense_handler:\n");
  
  switch (error_code)
    {
    case 0x70:
      text = "standard sense";
      break;
    case 0x7f:
      text = "Avision specific sense";
      break;
    default:
      text = "unknown sense";
    }
  
  debug_print_raw (1, "sense_handler: data:\n", sense, 8 + additional_sense);
  
  /* request valid? */
  if (! sense[0] & (1<<7)) {
    DBG (1, "sense_handler: sense not vaild ...\n");
    return status;
  }
  
  switch (sense_key)
    {
    case 0x00:
      status = SANE_STATUS_GOOD;
      text = "ok ?!?";
      break;
    case 0x02:
      text = "NOT READY";
      break;
    case 0x03:
      text = "MEDIUM ERROR (mostly ADF)";
      status = SANE_STATUS_JAMMED;
      break;
    case 0x04:
      text = "HARDWARE ERROR";
      break;
    case 0x05:
      text = "ILLEGAL REQUEST";
      break;
    case 0x06:
      text = "UNIT ATTENTION";
      break;
    case 0x09:
      text = "VENDOR SPECIFIC";
      break;
    case 0x0b:
      text = "ABORTED COMMAND";
      break;
    default:
      text = "got unknown sense code"; /* TODO: sprint the code ... */
    }
  
  DBG (1, "sense_handler: sense code: %s\n", text);
  
  if (sense[2] & (1<<6))
    DBG (1, "sense_handler: end of scan\n");   
  else 
    DBG (1, "sense_handler: scan has not yet been completed\n");
  
  if (sense[2] & (1<<5))
    DBG (1, "sense_handler: incorrect logical length\n");
  else 
    DBG (1, "sense_handler: correct logical length\n");

  {  
    u_int8_t asc = sense[12];
    u_int8_t ascq = sense[13];
    
#define ADDITIONAL_SENSE(asc,ascq,txt)			\
    case ( (asc << 8) + ascq): text = txt; break
    
    switch ( (asc << 8) + ascq )
      {
	/* normal */
	ADDITIONAL_SENSE (0x00,0x00, "No additional sense information");
      ADDITIONAL_SENSE (0x80,0x01, "ADF paper jam"; status = SANE_STATUS_JAMMED);
	ADDITIONAL_SENSE (0x80,0x02, "ADF cover open"; status = SANE_STATUS_COVER_OPEN);
	ADDITIONAL_SENSE (0x80,0x03, "ADF chute empty"; status = SANE_STATUS_NO_DOCS);
	ADDITIONAL_SENSE (0x80,0x04, "ADF paper end"; status = SANE_STATUS_EOF);
	ADDITIONAL_SENSE (0x44,0x00, "Internal target failure");
	ADDITIONAL_SENSE (0x47,0x00, "SCSI paritx error");
	ADDITIONAL_SENSE (0x20,0x00, "Invaild command");
	ADDITIONAL_SENSE (0x24,0x00, "Invaild field in CDB");
	ADDITIONAL_SENSE (0x25,0x00, "Logical unit not supported");
	ADDITIONAL_SENSE (0x26,0x00, "Invaild field in parameter list");
	ADDITIONAL_SENSE (0x2c,0x02, "Invaild combination of window specified");
	ADDITIONAL_SENSE (0x43,0x00, "Message error");
	ADDITIONAL_SENSE (0x2f,0x00, "Command cleared by another initiator");
	ADDITIONAL_SENSE (0x00,0x06, "I/O process terminated");
	ADDITIONAL_SENSE (0x3d,0x00, "invaild bit in identify message");
	ADDITIONAL_SENSE (0x49,0x00, "invaild message error");
	ADDITIONAL_SENSE (0x60,0x00, "Lamp failure");
	ADDITIONAL_SENSE (0x15,0x01, "Mechanical positioning error");
	ADDITIONAL_SENSE (0x1a,0x00, "parameter list lenght error");
	ADDITIONAL_SENSE (0x26,0x01, "parameter not supported");
	ADDITIONAL_SENSE (0x26,0x02, "parameter not supported");
	ADDITIONAL_SENSE (0x29,0x00, "Power-on, reset or bus device reset occurred");
	ADDITIONAL_SENSE (0x62,0x00, "Scan head positioning error");
	
	/* film scanner */
	ADDITIONAL_SENSE (0x81,0x00, "ADF front door open"; status = SANE_STATUS_COVER_OPEN);
	ADDITIONAL_SENSE (0x81,0x01, "ADF holder cartrige open"; status = SANE_STATUS_COVER_OPEN);
	ADDITIONAL_SENSE (0x81,0x02, "ADF no film inside"; status = SANE_STATUS_NO_DOCS);
	ADDITIONAL_SENSE (0x81,0x03, "ADF initial load fail");
	ADDITIONAL_SENSE (0x81,0x04, "ADF film end"; status = SANE_STATUS_NO_DOCS);
	ADDITIONAL_SENSE (0x81,0x05, "ADF forward feed error");
	ADDITIONAL_SENSE (0x81,0x06, "ADF rewind error");
	ADDITIONAL_SENSE (0x81,0x07, "ADF set unload");
	ADDITIONAL_SENSE (0x81,0x08, "ADF adapter error");
	
	/* maybe Minolta specific */
	ADDITIONAL_SENSE (0x90,0x00, "Scanner busy");
      default:
	text = "Unknown sense code asc: , ascq: "; /* TODO: sprint code here */
      }
    
#undef ADDITIONAL_SENSE
    
    DBG (1, "sense_handler: sese code: %s\n", text);
    
    /* sense code specific for invalid request
     * it is possible to get a detailed error location here ;-)*/
    if (sense_key == 0x05) {
      if (sense[15] & (1<<7) )
	{
	  if (sense[15] & (1<<6) )
	    DBG (1, "sense_handler: error in command parameter\n");
	  else
	    DBG (1, "sense_handler: error in data parameter\n");
	  
	  DBG (1, "sense_handler: error in parameter byte: %d, %x\n",
	       get_double(&(sense[16])),  get_double(&(sense[16])));
	  
	  /* bit pointer valid ?*/
	  if (sense[15] & (1<<3) )
	    DBG (1, "sense_handler: error in command parameter\n");
	  else
	    DBG (1, "sense_handler: bit pointer invalid\n");
	}
    }
  }
  
  return status;
}

/*
 * Avision scsi/usb multiplexers - to keep the code clean:
 */

static SANE_Status
avision_usb_status (Avision_Connection* av_con, int retry, int timeout)
{
  SANE_Status status = 0;
  u_int8_t usb_status[2] = {0, 0};
  size_t count = 0;
  int t_retry = retry;
  
  DBG (4, "avision_usb_status: timeout %d\n", timeout);
#ifndef HAVE_SANEI_USB_SET_TIMEOUT
#error "You must update include/sane/sanei_usb.h and sanei/sanei_usb.c accordingly!"
#endif
  sanei_usb_set_timeout (timeout);

  /* 1st try bulk transfers - they are more lightweight ... */
  for (;
       count == 0 &&
       (av_con->usb_status == AVISION_USB_BULK_STATUS ||
	av_con->usb_status == AVISION_USB_UNTESTED_STATUS) &&
       retry > 0;
       --retry)
    {
      count = sizeof (usb_status);
      
      DBG (5, "==> (bulk read) going down ...\n");
      status = sanei_usb_read_bulk (av_con->usb_dn, usb_status,
				    &count);
      DBG (5, "<== (bulk read) got: %ld, status: %d\n",
	   (u_long)count, usb_status[0]);

      if (count > 0) {
	av_con->usb_status = AVISION_USB_BULK_STATUS;
      }
    }
  
  /* reset retry count ... */
  retry = t_retry;
    
  /* 2nd try interrupt status read - if not yet disabled */
  for (;
       count == 0 &&
       (av_con->usb_status == AVISION_USB_INT_STATUS ||
	av_con->usb_status == AVISION_USB_UNTESTED_STATUS) &&
       retry > 0;
       --retry)
  {
    count = sizeof (usb_status);
    
    DBG (5, "==> (interrupt read) going down ...\n");
    status = sanei_usb_read_int (av_con->usb_dn, usb_status,
				 &count);
    DBG (5, "<== (interrupt read) got: %ld, status: %d\n",
	 (u_long)count, usb_status[0]);
    
    if (count > 0)
      av_con->usb_status = AVISION_USB_INT_STATUS;
  }  
  
  if (status != SANE_STATUS_GOOD)
    return status;
 
  if (count == 0)
    return SANE_STATUS_IO_ERROR;
 
  /* 0 = ok, 2 => request sense, 8 ==> busy, else error */
  switch (usb_status[0])
    {
    case AVISION_USB_GOOD:
      return SANE_STATUS_GOOD;
    case AVISION_USB_REQUEST_SENSE:
      DBG (2, "avision_usb_status: Needs to request sence!\n");
      return SANE_STATUS_INVAL;
    case AVISION_USB_BUSY:
      DBG (2, "avision_usb_status: Busy!\n");
      return SANE_STATUS_DEVICE_BUSY;
    default:
      DBG (1, "avision_usb_status: Unknown!\n");
      return SANE_STATUS_INVAL;
    }
}

static SANE_Status avision_open (const char* device_name,
				 Avision_Connection* av_con,
				 SANEI_SCSI_Sense_Handler sense_handler,
				 void *sense_arg)
{
  if (av_con->connection_type == AV_SCSI) {
    return sanei_scsi_open (device_name, &(av_con->scsi_fd),
			    sense_handler, sense_arg);
  }
  else {
    SANE_Status status;
    status = sanei_usb_open (device_name, &(av_con->usb_dn));
    return status;
  }
}

static SANE_Status avision_open_extended (const char* device_name,
					  Avision_Connection* av_con,
					  SANEI_SCSI_Sense_Handler sense_handler,
					  void *sense_arg, int *buffersize)
{
  if (av_con->connection_type == AV_SCSI) {
    return sanei_scsi_open_extended (device_name, &(av_con->scsi_fd),
				     sense_handler, sense_arg, buffersize);
  }
  else {
    SANE_Status status;
    status = sanei_usb_open (device_name, &(av_con->usb_dn));
    return status;
  }
}

static void avision_close (Avision_Connection* av_con)
{
  if (av_con->connection_type == AV_SCSI) {
    sanei_scsi_close (av_con->scsi_fd);
    av_con->scsi_fd = -1;
  }
  else {
    sanei_usb_close (av_con->usb_dn);
    av_con->usb_dn = -1;
  }
}

static SANE_Bool avision_is_open (Avision_Connection* av_con)
{
  if (av_con->connection_type == AV_SCSI) {
    return av_con->scsi_fd >= 0;
  }
  else {
    return av_con->usb_dn >= 0;
  }
}

static SANE_Status avision_cmd (Avision_Connection* av_con,
				const void* cmd, size_t cmd_size,
				const void* src, size_t src_size,
				void* dst, size_t* dst_size)
{
  if (av_con->connection_type == AV_SCSI) {
    return sanei_scsi_cmd2 (av_con->scsi_fd, cmd, cmd_size,
			    src, src_size, dst, dst_size);
  }
  else {
    SANE_Status status = SANE_STATUS_GOOD;
    
    size_t i, count, out_count;
#define STD_TIMEOUT 1500
    int retry, timeout = STD_TIMEOUT;
    
    /* simply to allow nicer code below */
    const u_int8_t* m_cmd = (const u_int8_t*)cmd;
    const u_int8_t* m_src = (const u_int8_t*)src;
    u_int8_t* m_dst = (u_int8_t*)dst;
    
    /* may I vote for the possibility to use C99 ... */
#define min_usb_size 10
#define max_usb_size 256 * 1024
    
    /* 1st send command data - at least 10 Bytes for USB scanners */
    u_int8_t enlarged_cmd [min_usb_size];
    if (cmd_size < min_usb_size) {
      DBG (1, "filling command to have a length of 10, was: %lu\n", (u_long) cmd_size);
      memcpy (enlarged_cmd, m_cmd, cmd_size);
      memset (enlarged_cmd + cmd_size, 0, min_usb_size - cmd_size);
      m_cmd = enlarged_cmd;
      cmd_size = min_usb_size;
    }
    
    retry = 4;
write_usb_cmd:
    count = cmd_size;
    
    DBG (8, "try to write cmd, count: %lu.\n", (u_long) count);
    sanei_usb_set_timeout (timeout);
    status = sanei_usb_write_bulk (av_con->usb_dn, m_cmd, &count);
      
    DBG (8, "wrote %lu bytes\n", (u_long) count);
    if (status != SANE_STATUS_GOOD || count != cmd_size) {
      DBG (3, "=== Got error %d trying to write, wrote: %d. ===\n", status, count);
      
      if (status != SANE_STATUS_GOOD) /* == SANE_STATUS_EOF) */ {
	DBG (3, "try to read status to clear the FIFO\n");
	status = avision_usb_status (av_con, 1, 500);
	if (status != SANE_STATUS_GOOD) {
	  DBG(3, "=== Got error %d trying to read status. ===\n", status);
	  return SANE_STATUS_IO_ERROR;
	}
	else
	  goto write_usb_cmd;
      } else {
	--retry;
	if (retry == 0)
	  return SANE_STATUS_IO_ERROR;
	DBG(3, "Retrying to send command\n");
	goto write_usb_cmd;
      }
      
      return SANE_STATUS_IO_ERROR;
    }
    
    /* 2nd send command data (if any) */
    for (i = 0; i < src_size;) {
      
      count = src_size - i;
      if (count > max_usb_size)
	count = max_usb_size;
      
      DBG (8, "try to write src, count: %lu.\n", (u_long) count);
      sanei_usb_set_timeout (timeout);
      status = sanei_usb_write_bulk (av_con->usb_dn, &(m_src[i]), &count);
      
      DBG (8, "wrote %lu bytes\n", (u_long) count);
      if (status != SANE_STATUS_GOOD)
	break;
      i += count;
    }
    
    /* 3rd: read the resuling data (payload) (if any) */
    if (status == SANE_STATUS_GOOD && dst != NULL && *dst_size > 0) {
      out_count = 0;
      while (out_count < *dst_size) {
	count = (*dst_size - out_count);
	
	DBG (8, "try to read %lu bytes\n", (u_long) count);
	sanei_usb_set_timeout (timeout);
        status = sanei_usb_read_bulk(av_con->usb_dn, &(m_dst[out_count]),
				     &count);
	DBG (8, "read %lu bytes\n", (u_long) count);
        
	if (status != SANE_STATUS_GOOD)
	  break;
	out_count += count;
	/* try to detect stray status bytes stuck in the FIFO */
	if (out_count < *dst_size && out_count == 1) {
	  DBG (1, "Detected stray status byte (%lu) stuck in the FIFO, retrying.\n",
          (u_long) count);
	  goto write_usb_cmd; /* verified: re-reading data does not work */
	}
      }
    }
    
    /* last: read the device status via a pseudo interrupt transfer
     * this is needed - otherwise the scanner will hang ... */
    {
      timeout=STD_TIMEOUT;
      retry = 6;
      switch (m_cmd[0]) {
        case AVISION_SCSI_INQUIRY:
        case AVISION_SCSI_TEST_UNIT_READY:
          retry = 2;
          break;
        case AVISION_SCSI_RELEASE_UNIT:
          retry = 8;
          timeout=30000;
          break;
        case AVISION_SCSI_SCAN:
	  timeout=5000;
	  retry = 2;
	  break;
        case AVISION_SCSI_READ:
          timeout=5000;
      }
      status = avision_usb_status (av_con, retry, timeout);
      timeout = STD_TIMEOUT;
    }
    /* next i/o hardening attempt - and yes this gets ugly ... */
    if (status != SANE_STATUS_GOOD && status != SANE_STATUS_INVAL)
      goto write_usb_cmd;
   
    if (status == SANE_STATUS_INVAL) {
      struct {
	command_header header;
	u_int8_t pad[4];
      } sense_cmd;
      
      u_int8_t sense_buffer[22];
      
      DBG(3, "Error during status read!\n");
      DBG(3, "=== Try to request sense ===\n");
      
      /* we can not call avision_cmd recursively - we might ending in
	 an endless recursion requesting sense for failing request
	 sense transfers ...*/
      
      memset (&sense_cmd, 0, sizeof (sense_cmd) );
      memset (&sense_buffer, 0, sizeof (sense_buffer) );
      sense_cmd.header.opc = AVISION_SCSI_REQUEST_SENSE;
      sense_cmd.header.len = sizeof (sense_buffer);
      
      count = sizeof(sense_cmd);
      
      DBG (8, "try to write %lu bytes\n", (u_long) count);
      sanei_usb_set_timeout (timeout);
      status = sanei_usb_write_bulk (av_con->usb_dn,
				     (u_int8_t*) &sense_cmd, &count);
      DBG (8, "wrote %lu bytes\n", (u_long) count);
      
      if (status != SANE_STATUS_GOOD) {
	DBG (3, "=== Got error %d trying to request sense! ===\n", status);
      }
      else {
	count = sizeof (sense_buffer);
	
	DBG (8, "try to read %lu bytes sense data\n", (u_long) count);
	sanei_usb_set_timeout (timeout);
	status = sanei_usb_read_bulk(av_con->usb_dn, sense_buffer, &count);
	DBG (8, "read %lu bytes sense data\n", (u_long) count);
	
	/* we need to read out the staus from the scanner i/o buffer */
	status = avision_usb_status (av_con, 1, timeout);
	
	if (status != SANE_STATUS_GOOD)
	  DBG (3, "=== Got error %d trying to read sense! ===\n", status);
	else {
	  /* read complete -> call our sense handler */
	  status = sense_handler (-1, sense_buffer, 0);
	}
      } /* end read sense data */
    } /* end request sense */
    return status;
  } /* end cmd usb */
}

/* A bubble sort for the calibration. It only sorts the first third
 * and returns an average of the top 2/3 values. The input data is
 * 16bit big endian and the count is the count of the words - not
 * bytes! */

static u_int16_t
bubble_sort (u_int8_t* sort_data, size_t count)
{
  size_t i, j, limit, k;
  double sum = 0.0;
  
  limit = count / 3;
  
  for (i = 0; i < limit; ++i)
    {
      u_int16_t ti = 0;
      u_int16_t tj = 0;
      
      for (j = (i + 1); j < count; ++j)
	{
	  ti = get_double ((sort_data + i*2));
	  tj = get_double ((sort_data + j*2));
	  
	  if (ti > tj) {
	    set_double ((sort_data + i*2), tj);
	    set_double ((sort_data + j*2), ti);
	  }
	}
    }
  
  for (k = 0, i = limit; i < count; ++i) {
    sum += get_double ((sort_data + i*2));
    ++ k;
  }
  
  /* DBG (7, "bubble_sort: %d values for average\n", k); */
  
  if (k > 0) /* if avg to compute */
    return (u_int16_t) (sum / k);
  else
    return (u_int16_t) (sum); /* always zero? */
}

static SANE_Status
add_color_mode (Avision_Device* dev, color_mode mode, SANE_String name)
{
  int i;
  DBG (3, "add_color_mode: %d %s\n", mode, name);
  
  for (i = 0; i < AV_COLOR_MODE_LAST; ++i)
    {
      if (dev->color_list [i] == 0) {
	dev->color_list [i] = strdup (name);
	dev->color_list_num [i] = mode;
	return SANE_STATUS_GOOD;
      }
    }
  
  DBG (3, "add_color_mode: failed\n");
  return SANE_STATUS_NO_MEM;
}

static int
last_color_mode (Avision_Device* dev)
{
  int i = 1;
  
  while (dev->color_list [i] != 0 && i < AV_COLOR_MODE_LAST)
    ++i;
  
  /* we are off by one */
  --i;
  
  return i;
}

static color_mode
match_color_mode (Avision_Device* dev, SANE_String name)
{
  int i;
  DBG (3, "match_color_mode:\n");

  for (i = 0; i < AV_COLOR_MODE_LAST; ++i)
    {
      if (dev->color_list [i] != 0 && strcmp (dev->color_list [i], name) == 0) {
	DBG (3, "match_color_mode: found at %d mode: %d\n",
	     i, dev->color_list_num [i]);
	return dev->color_list_num [i];
      }
    }
  
  DBG (3, "match_color_mode: source mode invalid\n");
  return AV_GRAYSCALE;
}

static SANE_Bool
color_mode_is_shaded (color_mode mode)
{
  return mode >= AV_GRAYSCALE;
}

static SANE_Bool
color_mode_is_color (color_mode mode)
{
  return mode >= AV_TRUECOLOR;
}

static SANE_Status
add_source_mode (Avision_Device* dev, source_mode mode, SANE_String name)
{
  int i;
  
  for (i = 0; i < AV_SOURCE_MODE_LAST; ++i)
    {
      if (dev->source_list [i] == 0) {
        dev->source_list [i] = strdup (name);
        dev->source_list_num [i] = mode;
        return SANE_STATUS_GOOD;
      }
    }
  
  return SANE_STATUS_NO_MEM;
}

static source_mode
match_source_mode (Avision_Device* dev, SANE_String name)
{
  int i;
  
  DBG (3, "match_source_mode: \"%s\"\n", name);

  for (i = 0; i < AV_SOURCE_MODE_LAST; ++i)
    {
      if (dev->source_list [i] != 0 && strcmp (dev->source_list [i], name) == 0) {
	DBG (3, "match_source_mode: found at %d mode: %d\n",
	     i, dev->source_list_num [i]);
	return dev->source_list_num [i];
      }
    }
  
  DBG (3, "match_source_mode: source mode invalid\n");
  return AV_NORMAL;
}

static source_mode_dim
match_source_mode_dim (source_mode sm)
{
  DBG (3, "match_source_mode_dim: %d\n", sm);
  
  switch (sm) {
  case AV_NORMAL:
    return AV_NORMAL_DIM;
  case AV_TRANSPARENT:
    return AV_TRANSPARENT_DIM;
  case AV_ADF:
  case AV_ADF_REAR:
  case AV_ADF_DUPLEX:
    return AV_ADF_DIM;
  default:
    DBG (3, "match_source_mode_dim: source mode invalid\n");
    return AV_NORMAL_DIM;
  }
}

static int
get_pixel_boundary (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  int boundary;
  
  switch (s->c_mode) {
  case AV_TRUECOLOR:
  case AV_TRUECOLOR16:
    boundary = dev->inquiry_color_boundary;
    break;
  case AV_TRUECOLOR12:
    boundary = dev->inquiry_color_boundary;
    boundary += 6 - boundary % 6;
    break;
  case AV_GRAYSCALE:
  case AV_GRAYSCALE16:
    boundary = dev->inquiry_gray_boundary;
    break;
  case AV_GRAYSCALE12:
    boundary = dev->inquiry_gray_boundary;
    boundary += 3 - boundary % 3;
    break;
  case AV_DITHERED:
    if (dev->inquiry_asic_type != AV_ASIC_C5)
      boundary = 32;
    else
      boundary = dev->inquiry_dithered_boundary;
    break;
  case AV_THRESHOLDED:
    if (dev->inquiry_asic_type != AV_ASIC_C5)
      boundary = 32;
    else
      boundary = dev->inquiry_thresholded_boundary;
    break;
  default:
    boundary = 8;
  }
  
  return boundary;
}

static SANE_Status
compute_parameters (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;

  int boundary = get_pixel_boundary (s);
  SANE_Bool gray_mode = color_mode_is_shaded (s->c_mode);

  /* ignore the INTERLACED DUPLEX flag for now, since the first duplex
     scanners do not set it - maybe there are only interlacing devices
     anyway */
  if (0)
    s->avdimen.interlaced_duplex = s->source_mode == AV_ADF_DUPLEX &&
                                   dev->inquiry_duplex_interlaced;
  else
    s->avdimen.interlaced_duplex = s->source_mode == AV_ADF_DUPLEX;

#ifdef AVISION_ENHANCED_SANE
  /* quick fix for Microsoft Office Products ... */
  switch (s->c_mode)
  {
    case AV_THRESHOLDED:
    case AV_DITHERED:
       /* our backend already has this restriction - so this line is for
          documentation purposes only */
      boundary = boundary > 32 ? boundary : 32;
      break;
    case AV_GRAYSCALE:
    case AV_GRAYSCALE16:
      boundary = boundary > 4 ? boundary : 4;
      break;
    case AV_GRAYSCALE12:
      boundary = boundary > 6 ? boundary : 6;
      break;
    case AV_TRUECOLOR:
    case AV_TRUECOLOR16:
      /* 12 bytes for 24bit color - 48bit is untested w/ Office */
      boundary = boundary > 4 ? boundary : 4;
      break;
    case AV_TRUECOLOR12:
      boundary = boundary > 6 ? boundary : 6;
      break;
  }
#endif
  
  DBG (3, "sane_compute_parameters:\n");
    
  DBG (3, "sane_compute_parameters: boundary %d, gray_mode: %d, \n",
       boundary, gray_mode);
      
  /* TODO: Implement real different x/y resolutions support */
  s->avdimen.xres = s->val[OPT_RESOLUTION].w;
  s->avdimen.yres = s->val[OPT_RESOLUTION].w;
      
  DBG (3, "sane_compute_parameters: tlx: %f, tly: %f, brx: %f, bry: %f\n",
       SANE_UNFIX (s->val[OPT_TL_X].w), SANE_UNFIX (s->val[OPT_TL_Y].w),
       SANE_UNFIX (s->val[OPT_BR_X].w), SANE_UNFIX (s->val[OPT_BR_Y].w));
      
  /* window parameter in pixel */
  s->avdimen.tlx = s->avdimen.xres * SANE_UNFIX (s->val[OPT_TL_X].w)
    / MM_PER_INCH;
  s->avdimen.tly = s->avdimen.yres * SANE_UNFIX (s->val[OPT_TL_Y].w)
    / MM_PER_INCH;
  s->avdimen.brx = s->avdimen.xres * SANE_UNFIX (s->val[OPT_BR_X].w)
    / MM_PER_INCH;
  s->avdimen.bry = s->avdimen.yres * SANE_UNFIX (s->val[OPT_BR_Y].w)
    / MM_PER_INCH;
      
  /* TODO: limit to real scan boundaries */
      
  /* line difference */
  if (color_mode_is_color (s->c_mode) && dev->inquiry_needs_software_colorpack)
    {
      if (dev->hw->feature_type & AV_NO_LINE_DIFFERENCE) {
	DBG (1, "sane_compute_parameters: Line difference ignored due to device-list!!\n");
      }
      else {
	/*  R² TODO: I made a mistake some months ago. line_difference should
	    be difference per color, unfortunately I'm out of time to review
	    all occurances of line_difference. At some point in the future
	    however this should be corrected thruout the backend ... */
	    
	s->avdimen.line_difference =
	  (dev->inquiry_line_difference * s->avdimen.yres) /
	  dev->inquiry_max_res * 3; /* *3 should be in the other places */
      }
	  
      s->avdimen.bry += s->avdimen.line_difference;
	  
      /* limit bry + line_difference to real scan boundary */
      {
	long y_max = dev->inquiry_y_ranges[s->source_mode_dim] *
	  s->avdimen.yres / MM_PER_INCH;
	DBG (3, "sane_compute_parameters: y_max: %ld, bry: %ld, line_difference: %d\n",
	     y_max, s->avdimen.bry, s->avdimen.line_difference);
	    
	if (s->avdimen.bry + s->avdimen.line_difference > y_max) {
	  DBG (1, "sane_compute_parameters: bry limitted!\n");
	  s->avdimen.bry = y_max - s->avdimen.line_difference;
	}
      }
	  
    } /* end if needs software colorpack */
  else {
    s->avdimen.line_difference = 0;
  }
      
  memset (&s->params, 0, sizeof (s->params));
      
  s->params.pixels_per_line = (s->avdimen.brx - s->avdimen.tlx);
  s->params.pixels_per_line -= s->params.pixels_per_line % boundary;

  s->params.lines = s->avdimen.bry - s->avdimen.tly - s->avdimen.line_difference;
  if (s->avdimen.interlaced_duplex)
    s->params.lines -= s->params.lines % 8;
  
  debug_print_avdimen (1, "sane_compute_parameters", &s->avdimen);
  
  switch (s->c_mode)
    {
    case AV_THRESHOLDED:
      s->params.format = SANE_FRAME_GRAY;
      s->params.bytes_per_line = s->params.pixels_per_line / 8;
      s->params.depth = 1;
      break;
    case AV_DITHERED:
      s->params.format = SANE_FRAME_GRAY;
      s->params.bytes_per_line = s->params.pixels_per_line / 8;
      s->params.depth = 1;
      break;
    case AV_GRAYSCALE:
      s->params.format = SANE_FRAME_GRAY;
      s->params.bytes_per_line = s->params.pixels_per_line;
      s->params.depth = 8;
      break;
    case AV_GRAYSCALE12:
    case AV_GRAYSCALE16:
      s->params.format = SANE_FRAME_GRAY;
      s->params.bytes_per_line = s->params.pixels_per_line * 2;
      s->params.depth = 16;
      break;
    case AV_TRUECOLOR:
      s->params.format = SANE_FRAME_RGB;
      s->params.bytes_per_line = s->params.pixels_per_line * 3;
      s->params.depth = 8;
      break;
    case AV_TRUECOLOR12:
    case AV_TRUECOLOR16:
      s->params.format = SANE_FRAME_RGB;
      s->params.bytes_per_line = s->params.pixels_per_line * 3 * 2;
      s->params.depth = 16;
      break;
    default:
      DBG (1, "Invalid mode. %d\n", s->c_mode);
      return SANE_STATUS_INVAL;
    } /* end switch */
      
  s->params.last_frame = SANE_TRUE;
      
      
  debug_print_params (1, "sane_compute_parameters", &s->params);
  return SANE_STATUS_GOOD;
}

static SANE_Status
wait_ready (Avision_Connection* av_con)
{
  SANE_Status status;
  int try;
  
  for (try = 0; try < 10; ++ try)
    {
      DBG (3, "wait_ready: sending TEST_UNIT_READY\n");
      status = avision_cmd (av_con, test_unit_ready, sizeof (test_unit_ready),
			    0, 0, 0, 0);
      sleep (2);
      
      switch (status)
	{
	default:
	  /* Ignore errors while waiting for scanner to become ready.
	     Some SCSI drivers return EIO while the scanner is
	     returning to the home position.  */
	  DBG (1, "wait_ready: test unit ready failed (%s)\n",
	       sane_strstatus (status));
	  /* fall through */
	case SANE_STATUS_DEVICE_BUSY:
	  break;
	case SANE_STATUS_GOOD:
	  return status;
	}
    }
  DBG (1, "wait_ready: timed out after %d attempts\n", try);
  return SANE_STATUS_INVAL;
}

static SANE_Status
wait_4_light (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  
  /* read stuff */
  struct command_read rcmd;
  char* light_status[] =
    { "off", "on", "warming up", "needs warm up test", 
      "light check error", "RESERVED" };
  
  SANE_Status status;
  u_int8_t result;
  int try;
  size_t size = 1;
  
  DBG (3, "wait_4_light: getting light status.\n");
  
  memset (&rcmd, 0, sizeof (rcmd));
  
  rcmd.opc = AVISION_SCSI_READ;
  rcmd.datatypecode = 0xa0; /* get light status */
  set_double (rcmd.datatypequal, dev->data_dq);
  set_triple (rcmd.transferlen, size);
  
  for (try = 0; try < 90; ++ try) {
    
    DBG (5, "wait_4_light: read bytes %lu\n", (u_long) size);
    status = avision_cmd (&s->av_con, &rcmd, sizeof (rcmd), 0, 0, &result, &size);
    
    if (status != SANE_STATUS_GOOD || size != sizeof (result)) {
      DBG (1, "wait_4_light: read failed (%s)\n", sane_strstatus (status));
      return status;
    }
    
    DBG (3, "wait_4_light: command is %d. Result is %s\n",
	 status, light_status[(result>4)?5:result]);
    
    if (result == 1) {
      return SANE_STATUS_GOOD;
    }
    else if (dev->hw->feature_type & AV_LIGHT_CHECK_BOGUS) {
      DBG (3, "wait_4_light: scanner marked as returning bogus values in device-list!!\n");
      return SANE_STATUS_GOOD;
    }
    else {
      struct command_send scmd;
      u_int8_t light_on = 1;
      
      /* turn on the light */
      DBG (3, "wait_4_light: setting light status.\n");
      
      memset (&scmd, 0, sizeof (scmd));
      
      scmd.opc = AVISION_SCSI_SEND;
      scmd.datatypecode = 0xa0; /* send light status */
      set_double (scmd.datatypequal, dev->data_dq);
      set_triple (scmd.transferlen, size);
      
      status = avision_cmd (&s->av_con, &scmd, sizeof (scmd),
			    &light_on, sizeof (light_on), 0, 0);
      
      if (status != SANE_STATUS_GOOD) {
	DBG (1, "wait_4_light: send failed (%s)\n", sane_strstatus (status));
	return status;
      }
      
#ifdef AVISION_ENHANCED_SANE
      return SANE_STATUS_LAMP_WARMING;
#endif
    }
    sleep (1);
  }
  
  DBG (1, "wait_4_light: timed out after %d attempts\n", try);
  return SANE_STATUS_DEVICE_BUSY;
}

static SANE_Status
get_firmware_status (Avision_Connection* av_con)
{
  /* read stuff */
  struct command_read rcmd;
  size_t size;
  SANE_Status status;
  u_int8_t result[8];
  
  DBG (3, "get_firmware_status\n");
 
  size = sizeof (result);
 
  memset (&rcmd, 0, sizeof (rcmd));
  rcmd.opc = AVISION_SCSI_READ;
 
  rcmd.datatypecode = 0x90; /* firmware status */
  set_double (rcmd.datatypequal, 0); /* dev->data_dq not available */
  set_triple (rcmd.transferlen, size);
 
  status = avision_cmd (av_con, &rcmd, sizeof (rcmd), 0, 0, result, &size);
  if (status != SANE_STATUS_GOOD || size != sizeof (result)) {
    DBG (1, "get_firmware_status: read failed (%s)\n",
	 sane_strstatus (status));
    return (status);
  }
 
  debug_print_raw (6, "get_firmware_status: raw data:\n", result, size);
  
  DBG (3, "get_firmware_status: [0]  needs firmware %x\n", result [0]);  
  DBG (3, "get_firmware_status: [2]  side edge: %d\n",
       get_double ( &(result[1]) ) );
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
get_flash_ram_info (Avision_Connection* av_con)
{
  /* read stuff */
  struct command_read rcmd;
  size_t size;
  SANE_Status status;
  u_int8_t result[40];
  
  DBG (3, "get_flash_ram_info\n");
 
  size = sizeof (result);
 
  memset (&rcmd, 0, sizeof (rcmd));
  rcmd.opc = AVISION_SCSI_READ;
 
  rcmd.datatypecode = 0x6a; /* flash ram information */
  set_double (rcmd.datatypequal, 0); /* dev->data_dq not available */
  set_triple (rcmd.transferlen, size);
 
  status = avision_cmd (av_con, &rcmd, sizeof (rcmd), 0, 0, result, &size);
  if (status != SANE_STATUS_GOOD || size != sizeof (result)) {
    DBG (1, "get_flash_ram_info: read failed (%s)\n",
	 sane_strstatus (status));
    return (status);
  }
 
  debug_print_raw (6, "get_flash_ram_info: raw data:\n", result, size);
  
  DBG (3, "get_flash_ram_info: [0]    data type %x\n", result [0]);  
  DBG (3, "get_flash_ram_info: [1]    Ability1:%s%s%s%s%s%s%s%s\n",
       BIT(result[1],7)?" RESERVED_BIT7":"",
       BIT(result[1],6)?" RESERVED_BIT6":"",
       BIT(result[1],5)?" FONT(r/w)":"",
       BIT(result[1],4)?" FPGA(w)":"",
       BIT(result[1],3)?" FMDBG(r)":"",
       BIT(result[1],2)?" RAWLINE(r)":"",
       BIT(result[1],1)?" FIRMWARE(r/w)":"",
       BIT(result[1],0)?" CTAB(r/w)":"");
  
  DBG (3, "get_flash_ram_info: [2-5]   size CTAB: %d\n",
       get_quad ( &(result[2]) ) );

  DBG (3, "get_flash_ram_info: [6-9]   size FIRMWARE: %d\n",
       get_quad ( &(result[6]) ) );

  DBG (3, "get_flash_ram_info: [10-13] size RAWLINE: %d\n",
       get_quad ( &(result[10]) ) );

  DBG (3, "get_flash_ram_info: [14-17] size FMDBG: %d\n",
       get_quad ( &(result[14]) ) );

  DBG (3, "get_flash_ram_info: [18-21] size FPGA: %d\n",
       get_quad ( &(result[18]) ) );

  DBG (3, "get_flash_ram_info: [22-25] size FONT: %d\n",
       get_quad ( &(result[22]) ) );

  DBG (3, "get_flash_ram_info: [26-29] size RESERVED: %d\n",
       get_quad ( &(result[26]) ) );

  DBG (3, "get_flash_ram_info: [30-33] size RESERVED: %d\n",
       get_quad ( &(result[30]) ) );
  
  return SANE_STATUS_GOOD;
}

#ifdef NEEDED

static SANE_Status
send_nvram_data (Avision_Connection* av_con)
{
  /* read stuff */
  struct command_send scmd;
  size_t size;
  SANE_Status status;
  
  DBG (3, "send_nvram_data\n");
 
  size = sizeof (c7_nvram);
 
  memset (&scmd, 0, sizeof (scmd));
  scmd.opc = AVISION_SCSI_SEND;
  
  scmd.datatypecode = 0x85; /* nvram data */
  set_double (scmd.datatypequal, 0); /* dev->data_dq not available */
  set_triple (scmd.transferlen, size);
 
  status = avision_cmd (av_con, &scmd, sizeof (scmd), &c7_nvram, size,
			0, 0);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "send_nvram_data: send failed (%s)\n",
	 sane_strstatus (status));
    return (status);
  }
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
send_flash_ram_data (Avision_Connection* av_con)
{
  /* read stuff */
  struct command_send scmd;
  size_t size;
  SANE_Status status;
  
  DBG (3, "send_flash_ram_data\n");
 
  size = sizeof (c7_flash_ram);
 
  memset (&scmd, 0, sizeof (scmd));
  scmd.opc = AVISION_SCSI_SEND;
  
  scmd.datatypecode = 0x86; /* flash data */
  set_double (scmd.datatypequal, 0);
  set_triple (scmd.transferlen, size);
 
  status = avision_cmd (av_con, &scmd, sizeof (scmd), &c7_flash_ram, size,
			0, 0);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "send_flash_ram_data: send failed (%s)\n",
	 sane_strstatus (status));
    return (status);
  }
  
  return SANE_STATUS_GOOD;
}
#endif

static SANE_Status
get_accessories_info (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  
  /* read stuff */
  struct command_read rcmd;
  size_t size;
  SANE_Status status;
  u_int8_t result[8];
  
  char* adf_model[] =
    { "Origami", "Oodles", "unknown" };
  
  DBG (3, "get_accessories_info\n");
  
  size = sizeof (result);
 
  memset (&rcmd, 0, sizeof (rcmd));
  rcmd.opc = AVISION_SCSI_READ;
 
  rcmd.datatypecode = 0x64; /* detect accessories */
  set_double (rcmd.datatypequal, dev->data_dq);
  set_triple (rcmd.transferlen, size);
 
  status = avision_cmd (&s->av_con, &rcmd, sizeof (rcmd), 0, 0, result, &size);
  if (status != SANE_STATUS_GOOD || size != sizeof (result)) {
    DBG (1, "get_accessories_info: read failed (%s)\n",
	 sane_strstatus (status));
    return (status);
  }
 
  debug_print_raw (6, "get_accessories_info: raw data:\n", result, size);
  
  DBG (3, "get_accessories_info: [0]  ADF: %x\n", result[0]);
  DBG (3, "get_accessories_info: [1]  Light Box: %x\n", result[1]);
  
  DBG (3, "get_accessories_info: [2]  ADF model: %d (%s)\n",
       result [2], adf_model[ (result[2] < 2) ? result[2] : 2 ]);
  
  dev->inquiry_adf |= result [0];
  dev->inquiry_light_box |= result [1];
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
get_button_status (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  
  /* read stuff */
  struct command_read rcmd;
  size_t size;
  SANE_Status status;
  /* was only 6 in an old SPEC - maybe we need a feature override :-( -ReneR */
  struct {
     u_int8_t press_state;
     u_int8_t buttons[5];
     u_int8_t display; /* AV 220 LED display */
     u_int8_t reserved[9];
  } result;
  
  unsigned int i, n;
  
  DBG (3, "get_button_status:\n");
  
  size = sizeof (result);
  
  /* At time of writing AV220 */
  if (! (dev->hw->feature_type & AV_INT_BUTTON))
    {
      memset (&rcmd, 0, sizeof (rcmd));
      rcmd.opc = AVISION_SCSI_READ;
      
      rcmd.datatypecode = 0xA1; /* button status */
      set_double (rcmd.datatypequal, dev->data_dq);
      set_triple (rcmd.transferlen, size);
      
      status = avision_cmd (&s->av_con, &rcmd, sizeof (rcmd), 0, 0,
			    (u_int8_t*)&result, &size);
      if (status != SANE_STATUS_GOOD || size != sizeof (result)) {
	DBG (1, "get_button_status: read failed (%s)\n", sane_strstatus (status));
	return status;
      }
    }
  else
    {
      /* only try to read the first 8 bytes ...*/
      size = 8;
      
      /* no SCSI equivalent */
      DBG (5, "==> (interrupt read) going down ...\n");
      status = sanei_usb_read_int (s->av_con.usb_dn, (u_int8_t*)&result,
				   &size);
      DBG (5, "==> (interrupt read) got: %d\n", size);
      
      if (status != SANE_STATUS_GOOD) {
	DBG (1, "get_button_status: interrupt read failed (%s)\n",
	     sane_strstatus (status));
	return SANE_STATUS_GOOD;
      }
      
      /* hack to fill in meaningful values for the AV 210 and under some
	 conditions the AV 220 */
      if (size == 1) { /* AV 210 */
	DBG (1, "get_button_status: one byte - assuming AV210\n");
	if (result.press_state > 0) {
	  debug_print_raw (6, "get_button_status: raw data\n",
			   (u_int8_t*)&result, size);
	  result.buttons[0] = result.press_state;
	  result.press_state = 0x80 | 1;
	  size = 2;
	}
      }
      else if (size >= 8 && result.press_state == 0) { /* AV 220 */
	
	debug_print_raw (6, "get_button_status: raw data\n",
		   (u_int8_t*)&result, size);
	
	DBG (1, "get_button_status: zero buttons  - filling values ...\n");
	
	/* simulate button press of the last button ... */
	result.press_state = 0x80 | 1;
	result.buttons[0] = dev->inquiry_buttons; /* 1 based */
      }
    }
  
  debug_print_raw (6, "get_button_status: raw data\n",
		   (u_int8_t*)&result, size);
  
  DBG (3, "get_button_status: [0]  Button status: %x\n", result.press_state);
  for (i = 0; i < 5; ++i)
    DBG (3, "get_button_status: [%d]  Button number %d: %x\n", i+1, i,
	 result.buttons[i]);
  DBG (3, "get_button_status: [7]  Display: %d\n", result.display);
  
  if (result.press_state >> 7) /* for AV220 bit6 is long/short press? */
    {
      n = result.press_state & 0x7F;
      DBG (3, "get_button_status: %d button(s) pressed\n", n);
      
      for (i = 0; i < n && i < 5; ++i) {
	int button = result.buttons[i] - 1; /* 1 based ... */
	DBG (3, "get_button_status: button %d pressed\n", button);
	if (button >= dev->inquiry_buttons ||
	    button >= (1 + OPT_BUTTON_LAST - OPT_BUTTON_0) ) {
	  DBG (1, "get_button_status: button %d not available in backend\n",
	       button);
	}
	else {
	  s->val[OPT_BUTTON_0 + button].w = SANE_TRUE;
	}
      }
      
      sprintf(s->val[OPT_MESSAGE].s, "%d", result.display);
      
      /* reset the button status */
      if (! (dev->hw->feature_type & AV_INT_BUTTON))
	{
	  struct command_send scmd;
	  u_int8_t button_reset = 1;
	  
	  DBG (3, "get_button_status: resetting status\n");
	  
	  memset (&scmd, 0, sizeof (scmd));
	  
	  scmd.opc = AVISION_SCSI_SEND;
	  scmd.datatypecode = 0xA1; /* button control */
	  set_double (scmd.datatypequal, dev->data_dq);
	  set_triple (scmd.transferlen, size);
	  
	  status = avision_cmd (&s->av_con, &scmd, sizeof (scmd),
				&button_reset, sizeof (button_reset), 0, 0);
	  
	  if (status != SANE_STATUS_GOOD) {
	    DBG (1, "get_button_status: send failed (%s)\n",
		 sane_strstatus (status));
	    return status;
	  }
	}
    }
  else
    DBG (3, "get_button_status: no button pressed\n");
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
get_frame_info (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  
  /* read stuff */
  struct command_read rcmd;
  size_t size;
  SANE_Status status;
  u_int8_t result[8];
  size_t i;
  
  DBG (3, "get_frame_info:\n");
  
  size = sizeof (result);
 
  memset (&rcmd, 0, sizeof (rcmd));
  rcmd.opc = AVISION_SCSI_READ;
 
  rcmd.datatypecode = 0x87; /* film holder sense */
  set_double (rcmd.datatypequal, dev->data_dq);
  set_triple (rcmd.transferlen, size);
 
  status = avision_cmd (&s->av_con, &rcmd, sizeof (rcmd), 0, 0, result, &size);
  if (status != SANE_STATUS_GOOD || size != sizeof (result)) {
    DBG (1, "get_frame_info: read failed (%s)\n", sane_strstatus (status));
    return (status);
  }
  
  debug_print_raw (6, "get_frame_info: raw data\n", result, size);
  
  DBG (3, "get_frame_info: [0]  Holder type: %s\n",
       (result[0]==1)?"APS":
       (result[0]==2)?"Film holder (35mm)":
       (result[0]==3)?"Slide holder":
       (result[0]==0xff)?"Empty":"unknown");
  DBG (3, "get_frame_info: [1]  Current frame number: %d\n", result[1]);
  DBG (3, "get_frame_info: [2]  Frame ammount: %d\n", result[2]);
  DBG (3, "get_frame_info: [3]  Mode: %s\n", BIT(result[3],4)?"APS":"Not APS");
  DBG (3, "get_frame_info: [3]  Exposures (if APS): %s\n", 
       ((i=(BIT(result[3],3)<<1)+BIT(result[2],2))==0)?"Unknown":
       (i==1)?"15":(i==2)?"25":"40");
  DBG (3, "get_frame_info: [3]  Film Type (if APS): %s\n", 
       ((i=(BIT(result[1],3)<<1)+BIT(result[0],2))==0)?"Unknown":
       (i==1)?"B&W Negative":(i==2)?"Color slide":"Color Negative");

  dev->holder_type = result[0];
  dev->current_frame = result[1];
 
  dev->frame_range.min = 1;
  dev->frame_range.quant = 1;
  
  if (result[0] != 0xff)
    dev->frame_range.max = result[2];
  else
    dev->frame_range.max = 1;
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
get_duplex_info (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  
  /* read stuff */
  struct command_read rcmd;

  struct {
     u_int8_t mode;
     u_int8_t color_line_difference[2];
     u_int8_t gray_line_difference[2];
     u_int8_t lineart_line_difference[2];
     u_int8_t image_info;
  } result;
  
  size_t size;
  SANE_Status status;
  
  DBG (3, "get_duplex_info:\n");
  
  size = sizeof (result);
 
  memset (&rcmd, 0, sizeof (rcmd));
  rcmd.opc = AVISION_SCSI_READ;
 
  rcmd.datatypecode = 0xB1; /* read duplex info */
  set_double (rcmd.datatypequal, dev->data_dq);
  set_triple (rcmd.transferlen, size);
 
  status = avision_cmd (&s->av_con, &rcmd, sizeof (rcmd), 0, 0,
			&result, &size);
  if (status != SANE_STATUS_GOOD || size != sizeof (result)) {
    DBG (1, "get_duplex_info: read failed (%s)\n", sane_strstatus (status));
    return (status);
  }
  
  debug_print_raw (6, "get_duplex_info: raw data\n", (u_int8_t*)&result, size);
  
  DBG (3, "get_duplex_info: [0]    Mode: %s%s\n",
       BIT(result.mode,0)?"MERGED_PAGES":"",
       BIT(result.mode,1)?"2ND_PAGE_FOLLOWS":"");
  DBG (3, "get_duplex_info: [1-2]  Color line difference: %d\n",
       get_double(result.color_line_difference));
  DBG (3, "get_duplex_info: [3-4]  Gray line difference: %d\n",
       get_double(result.gray_line_difference));
  DBG (3, "get_duplex_info: [5-6]  Lineart line difference: %d\n",
       get_double(result.lineart_line_difference));
  DBG (3, "get_duplex_info: [7]    Mode: %s%s%s%s\n",
       BIT(result.mode,0)?" FLATBED_BGR":" FLATBED_RGB",
       BIT(result.mode,1)?" ADF_BGR":" ADF_RGB",
       BIT(result.mode,2)?" FLATBED_NEEDS_MIRROR_IMAGE":"",
       BIT(result.mode,3)?" ADF_NEEDS_MIRROR_IMAGE":"");
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
set_frame (Avision_Scanner* s, SANE_Word frame)
{
  struct {
    struct command_send cmd;
    u_int8_t data[8];
  } scmd;
  
  Avision_Device* dev = s->hw;
  SANE_Status status;
  
  DBG (3, "set_frame: request frame %d\n", frame);
  
  /* Better check the current status of the film holder, because it
     can be changed between scans. */
  status = get_frame_info (s);
  if (status != SANE_STATUS_GOOD)
    return status;
  
  /* No film holder (shouldn't happen) */
  if (dev->holder_type == 0xff) {
    DBG (1, "set_frame: No film holder!!\n");
    return SANE_STATUS_INVAL;
  }
  
  /* Requesting frame 0xff indicates eject/rewind */
  if (frame != 0xff && (frame < 1 || frame > dev->frame_range.max) ) {
    DBG (1, "set_frame: Illegal frame (%d) requested (min=1, max=%d)\n",
	 frame, dev->frame_range.max); 
    return SANE_STATUS_INVAL;
  }
  
  memset (&scmd, 0, sizeof (scmd));
  scmd.cmd.opc = AVISION_SCSI_SEND;
  scmd.cmd.datatypecode = 0x87; /* send film holder "sense" */
  set_double (scmd.cmd.datatypequal, dev->data_dq);
  set_triple (scmd.cmd.transferlen, sizeof (scmd.data) );
  
  scmd.data[0] = dev->holder_type;
  scmd.data[1] = frame; 
  
  status = avision_cmd (&s->av_con, &scmd.cmd, sizeof (scmd.cmd),
			&scmd.data, sizeof (scmd.data), 0, 0);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "set_frame: send_data (%s)\n", sane_strstatus (status));
  }  

  return status;
}

#if 0 /* unused */
static SANE_Status
eject_or_rewind (Avision_Scanner* s)
{
  return (set_frame (s, 0xff) );
}
#endif

static SANE_Status
attach (SANE_String_Const devname, Avision_ConnectionType con_type,
        Avision_Device** devp)
{
#define STD_INQUIRY_SIZE 0x24
#define AVISION_INQUIRY_SIZE 0x60

  command_header inquiry;
  
  u_int8_t result [AVISION_INQUIRY_SIZE];
  int model_num;

  Avision_Device* dev;
  SANE_Status status;
  size_t size;
  
  Avision_Connection av_con;

  char mfg [9];
  char model [17];
  char rev [5];
  
  unsigned int i;
  char* s;
  SANE_Bool found;
  
  DBG (3, "attach:\n");
  
  for (dev = first_dev; dev; dev = dev->next)
    if (strcmp (dev->sane.name, devname) == 0) {
      if (devp)
	*devp = dev;
      return SANE_STATUS_GOOD;
    }

  av_con.connection_type = con_type;
  if (av_con.connection_type == AV_USB)
    av_con.usb_status = AVISION_USB_UNTESTED_STATUS;
  
  DBG (3, "attach: opening %s\n", devname);
  status = avision_open (devname, &av_con, sense_handler, 0);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "attach: open failed (%s)\n", sane_strstatus (status));
    return SANE_STATUS_INVAL;
  }
  
  /* first: get the standard inquiry? */
  memset (&inquiry, 0, sizeof (inquiry) );
#ifdef MAYBE_OPTIONAL
  inquiry.opc = AVISION_SCSI_INQUIRY;
  inquiry.len = STD_INQUIRY_SIZE;
#else
  inquiry.opc = AVISION_SCSI_INQUIRY;
  inquiry.len = AVISION_INQUIRY_SIZE;
#endif
  DBG (3, "attach: sending standard INQUIRY\n");
  size = inquiry.len;
  if (av_con.connection_type == AV_USB) {
    sanei_usb_set_timeout (1500);
  }
  
  {
    int try = 2;
    
    do {
      status = avision_cmd (&av_con, &inquiry, sizeof (inquiry), 0, 0,
  			    result, &size);
      if (status != SANE_STATUS_GOOD || size != inquiry.len) {
        DBG (1, "attach: standard inquiry failed (%s)\n",
             sane_strstatus (status));
      --try;
      }
    }while (status != SANE_STATUS_GOOD && try > 0);
    if (status != SANE_STATUS_GOOD)
      goto close_scanner_and_return;
  }

  /* copy string information - and zero terminate them c-style */
  memcpy (&mfg, result + 8, 8);
  mfg [8] = 0;
  memcpy (&model, result + 16, 16);
  model [16] = 0;
  memcpy (&rev, result + 32, 4);
  rev [4] = 0;
  
  /* shorten strings (-1 for last index
     -1 for last 0; >0 because one char at least) */
  for (i = sizeof (mfg) - 2; i > 0; i--) {
    if (mfg[i] == 0x20)
      mfg[i] = 0;
    else
      break;
  }
  for (i = sizeof (model) - 2; i > 0; i--) {
    if (model[i] == 0x20)
      model[i] = 0;
    else
      break;
  }
  
  DBG (1, "attach: Inquiry gives mfg=%s, model=%s, product revision=%s.\n",
       mfg, model, rev);
  
  model_num = 0;
  found = 0;
  while (Avision_Device_List [model_num].scsi_mfg != NULL) {
    if ((strcmp (mfg, Avision_Device_List [model_num].scsi_mfg) == 0) && 
	(strcmp (model, Avision_Device_List [model_num].scsi_model) == 0) ) {
      DBG (1, "attach: Found model: %d\n", model_num);
      found = 1;
      break;
    }
    ++ model_num;
  }
  
  if (!found) {
    DBG (1, "attach: model is not in the list of supported models!\n");
    DBG (1, "attach: You might want to report this output. To:\n");
    DBG (1, "attach: rene@exactcode.de (the Avision backend author)\n");
    
    status = SANE_STATUS_INVAL;
    goto close_scanner_and_return;
  }
  
  /* second: maybe ask for the firmware status and flash ram info */
  if (Avision_Device_List [model_num].feature_type & AV_FIRMWARE)
    {
      DBG (3, "attach: reading firmware status\n");
      status = get_firmware_status (&av_con);
      if (status != SANE_STATUS_GOOD) {
	DBG (1, "attach: get firmware status failed (%s)\n",
	     sane_strstatus (status));
	goto close_scanner_and_return;
      }
      
      DBG (3, "attach: reading flash ram info\n");
      status = get_flash_ram_info (&av_con);
      if (status != SANE_STATUS_GOOD) {
	DBG (1, "attach: get flash ram info failed (%s)\n",
	     sane_strstatus (status));
	goto close_scanner_and_return;
      }
      
#ifdef FIRMWARE_DATABASE_INCLUDED
      /* Send new NV-RAM (firmware) data */
      status = send_nvram_data (&av_con);
      if (status != SANE_STATUS_GOOD)
	goto close_scanner_and_return;
#endif
    }
  
  /* third: get the extended Avision inquiry */
  inquiry.len = AVISION_INQUIRY_SIZE;
  
  DBG (3, "attach: sending Avision INQUIRY\n");
  size = inquiry.len;
  status = avision_cmd (&av_con, &inquiry, sizeof (inquiry), 0, 0,
			result, &size);
  if (status != SANE_STATUS_GOOD || size != inquiry.len) {
    DBG (1, "attach: Avision inquiry failed (%s)\n", sane_strstatus (status));
    goto close_scanner_and_return;
  }
  
  dev = malloc (sizeof (*dev));
  if (!dev) {
    status = SANE_STATUS_NO_MEM;
    goto close_scanner_and_return;
  }
  
  memset (dev, 0, sizeof (*dev));

  dev->hw = &Avision_Device_List[model_num];
  
  dev->sane.name   = strdup (devname);
  dev->sane.vendor = dev->hw->real_mfg;
  dev->sane.model  = dev->hw->real_model;
  dev->connection.connection_type = av_con.connection_type;
  dev->connection.usb_status = av_con.usb_status;
  
  debug_print_raw (6, "attach: raw data:\n", result, sizeof (result) );
  
  DBG (3, "attach: [8-15]  Vendor id.:      \"%8.8s\"\n", result+8);
  DBG (3, "attach: [16-31] Product id.:     \"%16.16s\"\n", result+16);
  DBG (3, "attach: [32-35] Product rev.:    \"%4.4s\"\n", result+32);
  
  i = (result[36] >> 4) & 0x7;
  switch (result[36] & 0x07) {
  case 0:
    s = " RGB"; break;
  case 1:
    s = " BGR"; break;
  default:
    s = " unknown (RESERVERD)";
  }
  DBG (3, "attach: [36]    Bitfield:%s%s%s%s%s%s%s color plane\n",
       BIT(result[36],7)?" ADF":"",
       (i==0)?" B&W only":"",
       BIT(i, 1)?" 3-pass color":"",
       BIT(i, 2)?" 1-pass color":"",
       BIT(i, 2) && BIT(i, 0) ?" 1-pass color (ScanPartner only)":"",
       BIT(result[36],3)?" IS_NOT_FLATBED:":"",
       s);

  DBG (3, "attach: [37]    Optical res.:    %d00 dpi\n", result[37]);
  DBG (3, "attach: [38]    Maximum res.:    %d00 dpi\n", result[38]);
  
  DBG (3, "attach: [39]    Bitfield1:%s%s%s%s%s%s\n",
       BIT(result[39],7)?" TRANS":"",
       BIT(result[39],6)?" Q_SCAN":"",
       BIT(result[39],5)?" EXTENDED_RES":"",
       BIT(result[39],4)?" SUPPORTS_CALIB":"",
       BIT(result[39],2)?" NEW_PROTOCOL":"",
       (result[39] & 0x03) == 0x03 ? " AVISION":" OEM");
  
  DBG (3, "attach: [40-41] X res. in gray:  %d dpi\n",
       get_double ( &(result[40]) ));
  DBG (3, "attach: [42-43] Y res. in gray:  %d dpi\n",
       get_double ( &(result[42]) ));
  DBG (3, "attach: [44-45] X res. in color: %d dpi\n",
       get_double ( &(result[44]) ));
  DBG (3, "attach: [46-47] Y res. in color: %d dpi\n",
       get_double ( &(result[46]) ));
  DBG (3, "attach: [48-49] USB max read:    %d\n",
get_double ( &(result[48] ) ));

  DBG (3, "attach: [50]    ESA1:%s%s%s%s%s%s%s%s\n",
       BIT(result[50],7)?" LIGHT_CONTROL":"",
       BIT(result[50],6)?" BUTTON_CONTROL":"",
       BIT(result[50],5)?" NEED_SW_COLORPACK":"",
       BIT(result[50],4)?" SW_CALIB":"",
       BIT(result[50],3)?" NEED_SW_GAMMA":"",
       BIT(result[50],2)?" KEEPS_GAMMA":"",
       BIT(result[50],1)?" KEEPS_WINDOW_CMD":"",
       BIT(result[50],0)?" XYRES_DIFFERENT":"");
  DBG (3, "attach: [51]    ESA2:%s%s%s%s%s%s%s%s\n",
       BIT(result[51],7)?" EXPOSURE_CTRL":"",
       BIT(result[51],6)?" NEED_SW_TRIGGER_CAL":"",
       BIT(result[51],5)?" NEED_WHITE_PAPER_CALIB":"",
       BIT(result[51],4)?" SUPPORTS_QUALITY_SPEED_CAL":"",
       BIT(result[51],3)?" NEED_TRANSP_CAL":"",
       BIT(result[51],2)?" HAS_PUSH_BUTTON":"",
       BIT(result[51],1)?" NEW_CAL_METHOD_3x3_MATRIX_(NO_GAMMA_TABLE)":"",
       BIT(result[51],0)?" ADF_MIRRORS_IMAGE":"");
  DBG (3, "attach: [52]    ESA3:%s%s%s%s%s%s%s%s\n",
       BIT(result[52],7)?" GRAY_WHITE":"",
       BIT(result[52],6)?" SUPPORTS_GAIN_CONTROL":"",
       BIT(result[52],5)?" SUPPORTS_TET":"", /* "Text Enhanced Technology" */
       BIT(result[52],4)?" 3x3COL_TABLE":"",
       BIT(result[52],3)?" 1x3FILTER":"",
       BIT(result[52],2)?" INDEX_COLOR":"",
       BIT(result[52],1)?" POWER_SAVING_TIMER":"",
       BIT(result[52],0)?" NVM_DATA_REC":"");
   
  /* print some more scanner features/params */
  DBG (3, "attach: [53]    line difference (software color pack): %d\n", result[53]);
  DBG (3, "attach: [54]    color mode pixel boundary: %d\n", result[54]);
  DBG (3, "attach: [55]    gray mode pixel boundary: %d\n", result[55]);
  DBG (3, "attach: [56]    4bit gray mode pixel boundary: %d\n", result[56]);
  DBG (3, "attach: [57]    lineart mode pixel boundary: %d\n", result[57]);
  DBG (3, "attach: [58]    halftone mode pixel boundary: %d\n", result[58]);
  DBG (3, "attach: [59]    error-diffusion mode pixel boundary: %d\n", result[59]);
  
  DBG (3, "attach: [60]    channels per pixel:%s%s%s\n",
       BIT(result[60],7)?" 1":"",
       BIT(result[60],6)?" 3":"",
       (result[60] & 0x3F) != 0 ? " RESERVED":"");
  
  DBG (3, "attach: [61]    bits per channel:%s%s%s%s%s%s%s\n", 
       BIT(result[61],7)?" 1":"",
       BIT(result[61],6)?" 4":"",
       BIT(result[61],5)?" 6":"",
       BIT(result[61],4)?" 8":"",
       BIT(result[61],3)?" 10":"",
       BIT(result[61],2)?" 12":"",
       BIT(result[61],1)?" 16":"");
  
  DBG (3, "attach: [62]    scanner type:%s%s%s%s%s%s\n", 
       BIT(result[62],7)?" Flatbed":"",
       BIT(result[62],6)?" Roller (ADF)":"",
       BIT(result[62],5)?" Flatbed (ADF)":"",
       BIT(result[62],4)?" Roller":"", /* does not feed multiple pages, AV25 */
       BIT(result[62],3)?" Film scanner":"",
       BIT(result[62],2)?" Duplex":"");
  
  DBG (3, "attach: [75-75] Max shading target : %x\n",
       get_double ( &(result[75]) ));
  
  DBG (3, "attach: [77-78] Max X of transparency: %d dots * base_dpi\n",
       get_double ( &(result[77]) ));
  DBG (3, "attach: [79-80] May Y of transparency: %d dots * base_dpi\n",
       get_double ( &(result[79]) ));
  
  DBG (3, "attach: [81-82] Max X of flatbed:      %d dots * base_dpi\n",
       get_double ( &(result[81]) ));
  DBG (3, "attach: [83-84] May Y of flatbed:      %d dots * base_dpi\n",
       get_double ( &(result[83]) ));
  
  DBG (3, "attach: [85-86] Max X of ADF:          %d dots * base_dpi\n",
       get_double ( &(result[85]) ));
  DBG (3, "attach: [87-88] May Y of ADF:          %d dots * base_dpi\n",
       get_double ( &(result[87]) )); /* 0xFFFF means unlimited length */
  
  DBG (3, "attach: [89-90] Res. in Ex. mode:      %d dpi\n",
       get_double ( &(result[89]) ));
  
  DBG (3, "attach: [91]    ASIC:     %d\n", result[91]);
   
  DBG (3, "attach: [92]    Buttons:  %d\n", result[92]);
  
  DBG (3, "attach: [93]    ESA4:%s%s%s%s%s%s%s%s\n",
       BIT(result[93],7)?" SUPPORTS_ACCESSORIES_DETECT":"",
       BIT(result[93],6)?" ADF_IS_BGR_ORDERED":"",
       BIT(result[93],5)?" NO_SINGLE_CHANNEL_GRAY_MODE":"",
       BIT(result[93],4)?" SUPPORTS_FLASH_UPDATE":"",
       BIT(result[93],3)?" SUPPORTS_ASIC_UPDATE":"",
       BIT(result[93],2)?" SUPPORTS_LIGHT_DETECT":"",
       BIT(result[93],1)?" SUPPORTS_READ_PRNU_DATA":"",
       BIT(result[93],0)?" FLATBED_MIRRORS_IMAGE":"");
  
  DBG (3, "attach: [94]    ESA5:%s%s%s%s%s%s%s%s\n",
       BIT(result[94],7)?" IGNORE_LINE_DIFFERENCE_FOR_ADF":"",
       BIT(result[94],6)?" NEEDS_SW_LINE_COLOR_PACK":"",
       BIT(result[94],5)?" SUPPORTS_DUPLEX_SCAN":"",
       BIT(result[94],4)?" INTERLACED_DUPLEX_SCAN":"",
       BIT(result[94],3)?" SUPPORTS_TWO_MODE_ADF_SCANS":"",
       BIT(result[94],2)?" SUPPORTS_TUNE_SCAN_LENGTH":"",
       BIT(result[94],1)?" SUPPORTS_SWITCH_STRIP_DE_SCEW":"",
       BIT(result[94],0)?" SEARCHES_LEADING_SIDE_EDGE_BY_FIRMWARE":"");
  
  DBG (3, "attach: [95]    ESA6:%s%s%s%s%s%s%s%s\n",
       BIT(result[95],7)?" SUPPORTS_PAPER_SIZE_AUTO_DETECTION":"",
       BIT(result[95],6)?" SUPPORTS_DO_HOUSEKEEPING":"", /* Kodak i80 only */
       BIT(result[95],5)?" SUPPORTS_PAPER_LENGTH_SETTING":"", /* AV220, Kodak to detect multiple page feed */
       BIT(result[95],4)?" SUPPORTS_PRE_GAMMA_LINEAR_CORRECTION":"",
       BIT(result[95],3)?" SUPPORTS_PREFEEDING":"", /*  OKI S9800 */
       BIT(result[95],2)?" SUPPORTS_GET_BACKGROUND_RASTER":"", /* AV220 */
       BIT(result[95],1)?" SUPPORTS_NVRAM_RESET":"",
       BIT(result[95],0)?" SUPPORTS_BATCH_SCAN":"");
  
  /* I have no idea how film scanner could reliably be detected -ReneR */
  if (dev->hw->feature_type & AV_FILMSCANNER) {
    dev->scanner_type = AV_FILM;
    dev->sane.type = "film scanner";
  }
  else if ( BIT(result[62],6) || BIT(result[62],4) ) {
    dev->scanner_type = AV_SHEETFEED;
    dev->sane.type =  "sheetfed scanner";
  }
  else {
    dev->scanner_type = AV_FLATBED;
    dev->sane.type = "flatbed scanner";
  }

  dev->inquiry_new_protocol = BIT (result[39],2);
  dev->inquiry_asic_type = (int) result[91];

  dev->inquiry_adf = BIT (result[62], 5);
  dev->inquiry_duplex = BIT (result[62],2) || BIT (result[94],5);
  dev->inquiry_duplex_interlaced = BIT (result[94],4);
  /* TODO: ask Avision if this his the right bit ... ,-) */
  dev->inquiry_duplex_mode_two = BIT(result[94],3);

  dev->inquiry_detect_accessories = BIT(result[93],7);
  
  dev->inquiry_needs_calibration = BIT (result[50],4);
  
  dev->inquiry_needs_gamma = BIT (result[50],3);
  dev->inquiry_3x3_matrix = BIT (result[52], 4);
  dev->inquiry_needs_software_colorpack = BIT (result[50],5);
  dev->inquiry_needs_line_pack = BIT (result[94], 6);
  dev->inquiry_adf_need_mirror = BIT (result[51], 0);
  dev->inquiry_adf_bgr_order = BIT(result[93],6);
  
  dev->inquiry_light_detect = BIT (result[93], 2);
  dev->inquiry_light_control = BIT (result[50], 7);
  dev->inquiry_button_control = BIT (result[50], 6);
  
  dev->inquiry_max_shading_target = get_double ( &(result[75]) );
  
  dev->inquiry_color_boundary = result[54];
  if (dev->inquiry_color_boundary == 0)
    dev->inquiry_color_boundary = 8;
  
  dev->inquiry_gray_boundary = result[55];
  if (dev->inquiry_gray_boundary == 0)
    dev->inquiry_gray_boundary = 8;
  
  dev->inquiry_dithered_boundary = result[59];
  if (dev->inquiry_dithered_boundary == 0)
    dev->inquiry_dithered_boundary = 8;
  
  dev->inquiry_thresholded_boundary = result[57];
  if (dev->inquiry_thresholded_boundary == 0)
    dev->inquiry_thresholded_boundary = 8;
  
  dev->inquiry_line_difference = result[53];
  
  if (dev->inquiry_new_protocol) {
    dev->inquiry_optical_res = get_double ( &(result[89]) );
    dev->inquiry_max_res = get_double ( &(result[44]) );
  }
  else {
    dev->inquiry_optical_res = result[37] * 100;
    dev->inquiry_max_res = result[38] * 100;
  }
  
  if (dev->inquiry_optical_res == 0)
    {
      DBG (1, "Inquiry optical resolution is invalid!\n");
      if (dev->hw->feature_type & AV_FORCE_FILM)
	dev->inquiry_optical_res = 2438; /* verify */
      if (dev->scanner_type == AV_SHEETFEED)
	dev->inquiry_optical_res = 300;
      else
	dev->inquiry_optical_res = 600;
    }
  if (dev->inquiry_max_res == 0) {
    DBG (1, "Inquiry max resolution is invalid, using 1200 dpi!\n");
    dev->inquiry_max_res = 1200;
  }
  
  DBG (1, "attach: optical resolution set to: %d dpi\n", dev->inquiry_optical_res);
  DBG (1, "attach: max resolution set to: %d dpi\n", dev->inquiry_max_res);

  if (BIT(result[60],6))
    dev->inquiry_channels_per_pixel = 3;
  else if (BIT(result[60],7))
    dev->inquiry_channels_per_pixel = 1;
  else if ( ((result[36] >> 4) & 0x7) > 0)
    dev->inquiry_channels_per_pixel = 3;
  else
    dev->inquiry_channels_per_pixel = 1;
  
  if (BIT(result[61],1))
    dev->inquiry_bits_per_channel = 16;
  else if (BIT(result[61],2))
    dev->inquiry_bits_per_channel = 12;
  else if (BIT(result[61],3))
    dev->inquiry_bits_per_channel = 10;
  else if (BIT(result[61],4))
    dev->inquiry_bits_per_channel = 8;
  else if (BIT(result[61],5))
    dev->inquiry_bits_per_channel = 6;
  else if (BIT(result[61],6))
    dev->inquiry_bits_per_channel = 4;
  else if (BIT(result[61],7))
    dev->inquiry_bits_per_channel = 1;
  else
    dev->inquiry_bits_per_channel = 8; /* default for old scanners */

  DBG (1, "attach: max channels per pixel: %d, max bits per channel: %d\n",
       dev->inquiry_channels_per_pixel, dev->inquiry_bits_per_channel);
 
  dev->inquiry_buttons = result[92];

  /* get max x/y ranges for the different modes */
  {
    double base_dpi;
    if (dev->scanner_type != AV_FILM) {
      base_dpi = AVISION_BASE_RES;
    } else {
      /* ZP: The right number is 2820, wether it is 40-41, 42-43, 44-45,
       * 46-47 or 89-90 I don't know but I would bet for the last !
       * ReneR: OK. We use it via the optical_res which we need anyway ...
       */
      base_dpi = dev->inquiry_optical_res;
    }
    
    /* .1 to slightly increate the size to match the one of American standard paper
       formats that would otherwise be .1 mm too large to scan ... */
    dev->inquiry_x_ranges [AV_NORMAL_DIM] =
      (double)get_double (&(result[81])) * MM_PER_INCH / base_dpi + .1;
    dev->inquiry_y_ranges [AV_NORMAL_DIM] =
      (double)get_double (&(result[83])) * MM_PER_INCH / base_dpi;
  
    dev->inquiry_x_ranges [AV_TRANSPARENT_DIM] =
      (double)get_double (&(result[77])) * MM_PER_INCH / base_dpi + .1;
    dev->inquiry_y_ranges [AV_TRANSPARENT_DIM] =
      (double)get_double (&(result[79])) * MM_PER_INCH / base_dpi;
  
    dev->inquiry_x_ranges [AV_ADF_DIM] =
      (double)get_double (&(result[85])) * MM_PER_INCH / base_dpi + .1;
    dev->inquiry_y_ranges [AV_ADF_DIM] =
      (double)get_double (&(result[87])) * MM_PER_INCH / base_dpi;
  }
  
  /* check if x/y ranges are valid :-((( */
  {
    source_mode mode;

    for (mode = AV_NORMAL_DIM; mode < AV_SOURCE_MODE_DIM_LAST; ++ mode)
      {
	if (dev->inquiry_x_ranges [mode] != 0 &&
	    dev->inquiry_y_ranges [mode] != 0)
	  {
	    DBG (3, "attach: x/y-range for mode %d is valid!\n", mode);
	    if (force_a4) {
	      DBG (1, "attach: \"force_a4\" found! Using defauld (ISO A4).\n");
	      dev->inquiry_x_ranges [mode] = A4_X_RANGE * MM_PER_INCH;
	      dev->inquiry_y_ranges [mode] = A4_Y_RANGE * MM_PER_INCH;
	    } else if (force_a3) {
	      DBG (1, "attach: \"force_a3\" found! Using defauld (ISO A3).\n");
	      dev->inquiry_x_ranges [mode] = A3_X_RANGE * MM_PER_INCH;
	      dev->inquiry_y_ranges [mode] = A3_Y_RANGE * MM_PER_INCH;
	    }
	  }
	else /* mode is invaild */
	  {
	    DBG (1, "attach: x/y-range for mode %d is invalid! Using a default.\n", mode);
	    if (dev->hw->feature_type & AV_FORCE_A3) {
	      dev->inquiry_x_ranges [mode] = A3_X_RANGE * MM_PER_INCH;
	      dev->inquiry_y_ranges [mode] = A3_Y_RANGE * MM_PER_INCH;
	    }
	    else if (dev->hw->feature_type & AV_FORCE_FILM) {
	      dev->inquiry_x_ranges [mode] = FILM_X_RANGE * MM_PER_INCH;
	      dev->inquiry_y_ranges [mode] = FILM_Y_RANGE * MM_PER_INCH;
	    }
	    else {
	      dev->inquiry_x_ranges [mode] = A4_X_RANGE * MM_PER_INCH;
	      
	      if (dev->scanner_type == AV_SHEETFEED)
		dev->inquiry_y_ranges [mode] = SHEETFEED_Y_RANGE * MM_PER_INCH;
	      else
		dev->inquiry_y_ranges [mode] = A4_Y_RANGE * MM_PER_INCH;
	    }
	  }
	DBG (1, "attach: Mode %d range is now: %f x %f mm.\n",
	     mode,
	     dev->inquiry_x_ranges [mode], dev->inquiry_y_ranges [mode]);
      } /* end for all modes */
  }
  
  /* We need a bigger buffer for USB devices, since they seem to have
     a firmware bug and do not support reading the calibration data in
     tiny chunks */
  if (av_con.connection_type == AV_USB)
    dev->scsi_buffer_size = 1024 * 1024; /* 1 MB for now */
  else
    dev->scsi_buffer_size = sanei_scsi_max_request_size;
  
  /* set the flag for an additional probe at sane_open() */
  dev->additional_probe = SANE_TRUE;
    
  /* normally the data_dq is 0x0a0d - but newer scanner hang with it ... */
  if (!dev->inquiry_new_protocol)
    dev->data_dq = 0x0a0d;
  else
    dev->data_dq = 0;
  
  avision_close (&av_con);
  
  ++ num_devices;
  dev->next = first_dev;
  first_dev = dev;
  if (devp)
    *devp = dev;
  
  return SANE_STATUS_GOOD;
  
 close_scanner_and_return:
  avision_close (&av_con);
  
  return status;
}

static SANE_Status
additional_probe (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  
  /* we should wait until the scanner is ready before we
     perform further actions */
  
  SANE_Status status;
  /* try to retrieve additional accessories information */
  if (dev->inquiry_detect_accessories) {
    status = get_accessories_info (s);
    if (status != SANE_STATUS_GOOD)
      return status;
  }
  
  /* for a film scanner try to retrieve additional frame information */
  if (dev->scanner_type == AV_FILM) {
    status = get_frame_info (s);
    if (status != SANE_STATUS_GOOD)
      return status;
  }
  
  /* if (dev->inquiry_duplex) */ /* AV220 does not seem to support that */
  if (0) {
    status = get_duplex_info (s);
    if (status != SANE_STATUS_GOOD)
      return status;
  }
  
  /* create dynamic *-mode entries */
  
  if (dev->inquiry_bits_per_channel > 0) {
    add_color_mode (dev, AV_THRESHOLDED, "Line Art");
    add_color_mode (dev, AV_DITHERED, "Dithered");
  }

  if (dev->inquiry_bits_per_channel >= 8)
    add_color_mode (dev, AV_GRAYSCALE, "Gray");

  if (dev->inquiry_bits_per_channel == 12)
    add_color_mode (dev, AV_GRAYSCALE12, "12bit Gray");
  
  if (dev->inquiry_bits_per_channel >= 16)
    add_color_mode (dev, AV_GRAYSCALE16, "16bit Gray");
  
  if (dev->inquiry_channels_per_pixel > 1) {
    add_color_mode (dev, AV_TRUECOLOR, "Color");

    if (dev->inquiry_bits_per_channel == 12)
      add_color_mode (dev, AV_TRUECOLOR12, "12bit Color");
    
    if (dev->inquiry_bits_per_channel >= 16)
      add_color_mode (dev, AV_TRUECOLOR16, "16bit Color");
  }
  
  /* now choose the default mode - avoiding the 12/16 bit modes */
  dev->color_list_default = last_color_mode (dev);
  if (dev->inquiry_bits_per_channel > 8 && dev->color_list_default > 0) {
    dev->color_list_default--;
  }
  
  if (dev->scanner_type == AV_SHEETFEED)
    {
      add_source_mode (dev, AV_ADF, "ADF");
    }
  else
    {
      add_source_mode (dev, AV_NORMAL, "Normal");
      
      if (dev->inquiry_light_box)
	add_source_mode (dev, AV_TRANSPARENT, "Transparency");
      
      if (dev->inquiry_adf)
        add_source_mode (dev, AV_ADF, "ADF");
    }
  
  if (dev->inquiry_duplex) {
    add_source_mode (dev, AV_ADF_REAR, "ADF Rear");
    add_source_mode (dev, AV_ADF_DUPLEX, "ADF Duplex");
  }
  
  dev->additional_probe = SANE_FALSE;
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
get_calib_format (Avision_Scanner* s, struct calibration_format* format)
{
  SANE_Status status;
  
  struct command_read rcmd;
  u_int8_t result [32];
  size_t size;
  
  DBG (3, "get_calib_format:\n");

  size = sizeof (result);
  
  memset (&rcmd, 0, sizeof (rcmd));
  rcmd.opc = AVISION_SCSI_READ;
  rcmd.datatypecode = 0x60; /* get calibration format */
  set_double (rcmd.datatypequal, s->hw->data_dq);
  set_triple (rcmd.transferlen, size);
  
  DBG (3, "get_calib_format: read_data: %lu bytes\n", (u_long) size);
  status = avision_cmd (&s->av_con, &rcmd, sizeof (rcmd), 0, 0, result, &size);
  if (status != SANE_STATUS_GOOD || size != sizeof (result) ) {
    DBG (1, "get_calib_format: read calib. info failt (%s)\n",
	 sane_strstatus (status) );
    return status;
  }
  
  debug_print_calib_format (3, "get_calib_format", result);
  
  format->pixel_per_line = get_double (&(result[0]));
  format->bytes_per_channel = result[2];
  format->lines = result[3];
  format->flags = result[4];
  format->ability1 = result[5];
  format->r_gain = result[6];
  format->g_gain = result[7];
  format->b_gain = result[8];
  format->r_shading_target = get_double (&(result[9]));
  format->g_shading_target = get_double (&(result[11]));
  format->b_shading_target = get_double (&(result[13]));
  format->r_dark_shading_target = get_double (&(result[15]));
  format->g_dark_shading_target = get_double (&(result[17]));
  format->b_dark_shading_target = get_double (&(result[19]));
  
  /* now translate to normal! */
  /* firmware return R--RG--GB--B with 3 line count */
  /* software format it as 1 line if true color scan */
  /* only line interleave format to be supported */
  
  if (color_mode_is_color (s->c_mode) || BIT(format->ability1, 3)) {
    format->channels = 3;
    format->lines /= 3; /* line interleave */
  }
  else
    format->channels = 1;
  
  DBG (3, "get_calib_format: channels: %d\n", format->channels);
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
get_calib_data (Avision_Scanner* s, u_int8_t data_type,
		u_int8_t* calib_data,
		size_t calib_size)
{
  SANE_Status status;
  u_int8_t *calib_ptr;
  
  size_t get_size, data_size, chunk_size;
  
  struct command_read rcmd;
  
  chunk_size = calib_size;
  
  DBG (3, "get_calib_data: type %x, size %lu, chunk_size: %lu\n",
       data_type, (u_long) calib_size, (u_long) chunk_size);
  
  memset (&rcmd, 0, sizeof (rcmd));
  
  rcmd.opc = AVISION_SCSI_READ;
  rcmd.datatypecode = data_type;
  set_double (rcmd.datatypequal, s->hw->data_dq);
  
  calib_ptr = calib_data;
  get_size = chunk_size;
  data_size = calib_size;
  
  while (data_size) {
    if (get_size > data_size)
      get_size = data_size;

    read_constrains(s, get_size);

    set_triple (rcmd.transferlen, get_size);

    DBG (3, "get_calib_data: Reading %d bytes calibration data\n", get_size);
 
    status = avision_cmd (&s->av_con, &rcmd,
			  sizeof (rcmd), 0, 0, calib_ptr, &get_size);
    if (status != SANE_STATUS_GOOD) {
      DBG (1, "get_calib_data: read data failed (%s)\n",
	   sane_strstatus (status));
      return status;
    }

    DBG (3, "get_calib_data: Got %d bytes calibration data\n", get_size);

    data_size -= get_size;
    calib_ptr += get_size;
  }
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
set_calib_data (Avision_Scanner* s, struct calibration_format* format,
		u_int8_t* dark_data, u_int8_t* white_data)
{
  Avision_Device* dev = s->hw;
  
  struct command_send scmd;
  
  SANE_Status status;
  
  u_int8_t send_type;
  u_int16_t send_type_q;
  
  int i;
  size_t out_size;
  int elements_per_line = format->pixel_per_line * format->channels;
  
  DBG (3, "set_calib_data:\n");
  
  send_type = 0x82; /* download calibration data */
  
  /* do we use a color mode? */
  if (format->channels > 1) {
    send_type_q = 0x12; /* color calib data */
  }
  else {
    if (dev->hw->feature_type & AV_GRAY_CALIB_BLUE)
      send_type_q = 0x2; /* gray/bw calib data on the blue channel (AV610) */
    else
      send_type_q = 0x11; /* gray/bw calib data */
  }
  
  memset (&scmd, 0x00, sizeof (scmd));
  scmd.opc = AVISION_SCSI_SEND;
  scmd.datatypecode = send_type;
  
  /* data corrections due to dark calibration data merge */
  if (BIT (format->ability1, 2) ) {
    DBG (3, "set_calib_data: merging dark calibration data\n");
    for (i = 0; i < elements_per_line; ++i) {
      u_int16_t value_orig = get_double_le (white_data + i*2);
      u_int16_t value_new = value_orig;
      
      value_new &= 0xffc0;
      value_new |= (get_double_le (dark_data + i*2) >> 10) & 0x3f;
      
      DBG (100, "set_calib_data: element %d, dark difference %d\n",
	   i, value_orig - value_new);
      
      set_double_le ((white_data + i*2), value_new);
    }
  }
  
  out_size = format->pixel_per_line * 2;
  
  /* send data in one command? */
  if (format->channels == 1 ||
      ( (dev->hw->feature_type & AV_ONE_CALIB_CMD) ||
	! BIT(format->ability1, 0) ) )
    /* one command (most scanners) */
    {
      size_t send_size = elements_per_line * 2;
      DBG (3, "set_calib_data: all channels in one command\n");
      DBG (3, "set_calib_data: send_size: %lu\n", (u_long) send_size);
      
      memset (&scmd, 0, sizeof (scmd) );
      scmd.opc = AVISION_SCSI_SEND;
      scmd.datatypecode = send_type;
      set_double (scmd.datatypequal, send_type_q);
      set_triple (scmd.transferlen, send_size);

      status = avision_cmd (&s->av_con, &scmd, sizeof (scmd),
			    (char*) white_data, send_size, 0, 0);
      /* not return immediately to free mem at the end */
    }
  else /* send data channel by channel (some USB ones) */
    {
      int conv_out_size = format->pixel_per_line * 2;
      u_int16_t* conv_out_data; /* here it is save to use 16bit data
				   since we only move wohle words around */
      
      DBG (3, "set_calib_data: channels in single commands\n");
      
      conv_out_data =  (u_int16_t*) malloc (conv_out_size);
      if (!conv_out_data) {
	status = SANE_STATUS_NO_MEM;
      }
      else {
	int channel;
	for (channel = 0; channel < 3; ++ channel)
	  {
	    int i;
	    
	    /* no need for endianness handling since whole word copy */
	    u_int16_t* casted_avg_data = (u_int16_t*) white_data;
	    
	    DBG (3, "set_calib_data_calibration: channel: %i\n", channel);
	    
	    for (i = 0; i < format->pixel_per_line; ++ i)
	      conv_out_data [i] = casted_avg_data [i * 3 + channel];
	    
	    DBG (3, "set_calib_data: sending %i bytes now\n",
		 conv_out_size);
	    
	    memset (&scmd, 0, sizeof (scmd));
	    scmd.opc = AVISION_SCSI_SEND;
	    scmd.datatypecode = send_type; /* send calibration data */
	    set_double (scmd.datatypequal, channel);
	    set_triple (scmd.transferlen, conv_out_size);
	    
	    status = avision_cmd (&s->av_con, &scmd, sizeof (scmd),
				  conv_out_data, conv_out_size, 0, 0);
	    if (status != SANE_STATUS_GOOD) {
	      DBG (3, "set_calib_data: send_data failed (%s)\n",
		   sane_strstatus (status));
	      /* not return immediately to free mem at the end */
	    }
	  } /* end for each channel */
	free (conv_out_data);
      } /* end else send calib data*/
    }
  
  return SANE_STATUS_GOOD;
}

/* Sort data pixel by pixel and average first 2/3 of the data.
   The caller has to free return pointer. R,G,B pixels
   interleave to R,G,B line interleave.
   
   The input data data is in 16 bits little endian, always.
   That is a = b[1] << 8 + b[0] in all system.

   We convert it to SCSI high-endian (big-endian) since we use it all
   over the place anyway .... - Sorry for this mess. */

static u_int8_t*
sort_and_average (struct calibration_format* format, u_int8_t* data)
{
  int stride, i, line;
  int elements_per_line;
  
  u_int8_t *sort_data, *avg_data;
  
  DBG (1, "sort_and_average:\n");
  
  if (!format || !data)
    return NULL;
  
  sort_data = malloc (format->lines * 2);
  if (!sort_data)
    return NULL;
  
  elements_per_line = format->pixel_per_line * format->channels;
  
  avg_data = malloc (elements_per_line * 2);
  if (!avg_data) {
    free (sort_data);
    return NULL;
  }
  
  stride = format->bytes_per_channel * elements_per_line;
  
  /* for each pixel */
  for (i = 0; i < elements_per_line; ++ i)
    {
      u_int8_t* ptr1 = data + i * format->bytes_per_channel;
      u_int16_t temp;
      
      /* copy all lines for pixel i into the linear array sort_data */
      for (line = 0; line < format->lines; ++ line) {
	u_int8_t* ptr2 = ptr1 + line * stride; /* pixel */
	
	if (format->bytes_per_channel == 1)
	  temp = *ptr2 << 8;
	else
	  temp = get_double_le (ptr2);	  /* little-endian! */
	set_double ((sort_data + line*2), temp); /* store big-endian */
	/* DBG (7, "ReneR to sort: %x\n", temp); */
      }
      
      temp = bubble_sort (sort_data, format->lines);
      /* DBG (7, "ReneR averaged: %x\n", temp); */
      set_double ((avg_data + i*2), temp); /* store big-endian */
    }
  
  free ((void *) sort_data);
  return avg_data;
}

/* shading data is 16bits little endian format when send/read from firmware */
static void
compute_dark_shading_data (Avision_Scanner* s,
			   struct calibration_format* format, u_int8_t* data)
{
  u_int16_t map_value = DEFAULT_DARK_SHADING;
  u_int16_t rgb_map_value[3];
  
  int elements_per_line, i;
  
  DBG (3, "compute_dark_shading_data:\n");
  
  if (s->hw->inquiry_max_shading_target != INVALID_DARK_SHADING)
    map_value = s->hw->inquiry_max_shading_target << 8;
  
  rgb_map_value[0] = format->r_dark_shading_target;
  rgb_map_value[1] = format->g_dark_shading_target;
  rgb_map_value[2] = format->b_dark_shading_target;
  
  for (i = 0; i < format->channels; ++i) {
    if (rgb_map_value[i] == INVALID_DARK_SHADING)
      rgb_map_value[i] = map_value;
  }
  
  if (format->channels == 1) {
    rgb_map_value[1] = rgb_map_value[2] = rgb_map_value[0];
  }
  
  elements_per_line = format->pixel_per_line * format->channels;

  /* Check line interleave or pixel interleave. */
  /* It seems no ASIC use line interleave right now. */
  /* Avision SCSI protocol document has bad description. */
  for (i = 0; i < elements_per_line; ++i)
    {
      u_int16_t tmp_data = get_double_le((data + i*2));
      if (tmp_data > rgb_map_value[i % 3]) {
	set_double ((data + i*2), tmp_data - rgb_map_value[i % 3]);
      }
      else {
	set_double ((data + i*2), 0);
      }
    }
}

static void
compute_white_shading_data (Avision_Scanner* s,
			    struct calibration_format* format, u_int8_t* data)
{
  int i;
  u_int16_t inquiry_mst = DEFAULT_WHITE_SHADING;
  u_int16_t mst[3], result;
  
  int elements_per_line = format->pixel_per_line * format->channels;
  
  /* debug counter */
  int values_invalid = 0;
  int values_limitted = 0;
  
  DBG (3, "compute_white_shading_data:\n");
  
  if (s->hw->inquiry_max_shading_target != INVALID_WHITE_SHADING)
    inquiry_mst = s->hw->inquiry_max_shading_target << 4;
  
  mst[0] = format->r_shading_target;
  mst[1] = format->g_shading_target;
  mst[2] = format->b_shading_target;
  
  for (i = 0; i < 3; ++i) {
    if (mst[i] == INVALID_WHITE_SHADING) /* mst[i] > MAX_WHITE_SHADING) */  {
      DBG (3, "compute_white_shading_data: target %d invaild (%x) using inquiry (%x)\n",
	   i, mst[i], inquiry_mst);
      mst[i] = inquiry_mst;
    }
    /* some firmware versions seems to return the bytes swapped? */
    else if (mst[i] < 0x110) {
      u_int8_t* swap_mst = (u_int8_t*) &mst[i];
      u_int8_t low_nibble_mst = swap_mst [0];
      swap_mst [0] = swap_mst[1];
      swap_mst [1] = low_nibble_mst;

      DBG (3, "compute_white_shading_data: target %d: bytes swapped.\n", i);
    }
    if (mst[i] < DEFAULT_WHITE_SHADING / 2) {
      DBG (3, "compute_white_shading_data: target %d: too low (%d) usind default (%d).\n",
	   i, mst[i], DEFAULT_WHITE_SHADING);
      mst[i] = DEFAULT_WHITE_SHADING;
    }
    else
      DBG (3, "compute_white_shading_data: target %d: %x\n", i, mst[0]);
  }
  
  /* some Avision example code was present here until SANE/Avision
   * BUILD 57. */
  
  /* calculate calibration data */
  for (i = 0; i < elements_per_line; ++ i)
    {
      /* calculate calibration value for pixel i */
      u_int16_t tmp_data = get_double((data + i*2));
      
      if (tmp_data == INVALID_WHITE_SHADING) {
       	tmp_data = DEFAULT_WHITE_SHADING;
	++ values_invalid;
      }
      
      result = ( (double) mst[i % 3] * WHITE_MAP_RANGE / (tmp_data + 0.5));
      
      /* sanity check for over-amplification */
      if (result > WHITE_MAP_RANGE * 2) {
	result = WHITE_MAP_RANGE;
	++ values_limitted;
      }
      
      /* for visual debugging ... */
      if (static_calib_list [i % 3] == SANE_TRUE)
	result = 0xA000;
      
      /* the output to the scanner will be 16 bit little endian again */
      set_double_le ((data + i*2), result);
    }
  DBG (3, "compute_white_shading_data: %d invalid, %d limitted\n",
       values_invalid, values_limitted);
}

/* old_r_calibration was here until SANE/Avision BUILD 90 */

static SANE_Status
normal_calibration (Avision_Scanner* s)
{
  SANE_Status status;
  
  struct calibration_format calib_format;
  
  int calib_data_size, calib_bytes_per_line;
  u_int8_t read_type;
  u_int8_t *calib_tmp_data;
  
  DBG (1, "normal_calibration:\n");
  
  /* get calibration format and data */
  status = get_calib_format (s, &calib_format);
  if (status != SANE_STATUS_GOOD)
    return status;

  /* check if need do calibration */
  if (calib_format.flags == 3) { /* needs no calibration */
    DBG (1, "normal_calibration: Scanner claims no calibration needed -> skipped!\n");
    return SANE_STATUS_GOOD;
  }
  
  /* calculate calibration data size for read from scanner */
  /* size = lines * bytes_per_channel * pixels_per_line * channel */
  calib_bytes_per_line = calib_format.bytes_per_channel *
    calib_format.pixel_per_line * calib_format.channels;
  
  calib_data_size = calib_format.lines * calib_bytes_per_line;
  
  calib_tmp_data = malloc (calib_data_size);
  if (!calib_tmp_data)
    return SANE_STATUS_NO_MEM;
  
  /* check if we need to do dark calibration (shading) */
  if (BIT(calib_format.ability1, 3))
    {
      DBG (1, "normal_calibration: reading dark data\n");
      /* read dark calib data */
      status = get_calib_data (s, 0x66, calib_tmp_data, calib_data_size);
      
      if (status != SANE_STATUS_GOOD) {
	free (calib_tmp_data);
	return status;
      }
      
      /* process dark data: sort and average. */
      
      if (s->dark_avg_data) {
	free (s->dark_avg_data);
	s->dark_avg_data = 0;
      }
      s->dark_avg_data = sort_and_average (&calib_format, calib_tmp_data);
      if (!s->dark_avg_data) {
	free (calib_tmp_data);
	return SANE_STATUS_NO_MEM;
      }
      compute_dark_shading_data (s, &calib_format, s->dark_avg_data);
    }
  
  /* do we use a color mode? */
  if (calib_format.channels > 1) {
    DBG (3, "normal_calibration: using color calibration\n");
    read_type = 0x62; /* read color calib data */
  }
  else {
    DBG (3, "normal_calibration: using gray calibration\n");
    read_type = 0x61; /* gray calib data */
  }
  
  /* do white calibration: read gray or color data */
  status = get_calib_data (s, read_type, calib_tmp_data, calib_data_size);
  
  if (status != SANE_STATUS_GOOD) {
    free (calib_tmp_data);
    return status;
  }

  if (s->white_avg_data) {
    free (s->white_avg_data);
    s->white_avg_data = 0;
  }
  s->white_avg_data = sort_and_average (&calib_format, calib_tmp_data);
  if (!s->white_avg_data) {
    free (calib_tmp_data);
    return SANE_STATUS_NO_MEM;
  }
  
  /* decrease white average data (if dark average data is present) */
  if (s->dark_avg_data) {
    int elements_per_line = calib_format.pixel_per_line * calib_format.channels;
    int i;

    DBG (1, "normal_calibration: dark data present - decreasing white aerage data\n");
    
    for (i = 0; i < elements_per_line; ++ i) {
      s->white_avg_data[i] -= s->dark_avg_data[i];
    }
  }
  
  compute_white_shading_data (s, &calib_format, s->white_avg_data);
  
  status = set_calib_data (s, &calib_format,
			   s->dark_avg_data, s->white_avg_data);
  
  free (calib_tmp_data);
  return status;
}

/* next was taken from the GIMP and is a bit modifyed ... ;-)
 * original Copyright (C) 1995 Spencer Kimball and Peter Mattis
 */
static double 
brightness_contrast_func (double brightness, double contrast, double value)
{
  double nvalue;
  double power;
  
  /* apply brightness */
  if (brightness < 0.0)
    value = value * (1.0 + brightness);
  else
    value = value + ((1.0 - value) * brightness);

  /* apply contrast */
  if (contrast < 0.0)
    {
      if (value > 0.5)
	nvalue = 1.0 - value;
      else
	nvalue = value;
      if (nvalue < 0.0)
	nvalue = 0.0;
      nvalue = 0.5 * pow (nvalue * 2.0 , (double) (1.0 + contrast));
      if (value > 0.5)
	value = 1.0 - nvalue;
      else
	value = nvalue;
    }
  else
    {
      if (value > 0.5)
	nvalue = 1.0 - value;
      else
	nvalue = value;
      if (nvalue < 0.0)
	nvalue = 0.0;
      power = (contrast == 1.0) ? 127 : 1.0 / (1.0 - contrast);
      nvalue = 0.5 * pow (2.0 * nvalue, power);
      if (value > 0.5)
	value = 1.0 - nvalue;
      else
	value = nvalue;
    }
  return value;
}

static SANE_Status
send_gamma (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  SANE_Status status = SANE_STATUS_GOOD;
  
  int invert_table = 0;
  
  size_t gamma_table_raw_size;
  size_t gamma_table_size;
  size_t gamma_values;
  
  struct command_send scmd;
  u_int8_t *gamma_data;
  
  int color; /* current color */
  size_t i; /* big table index */
  size_t j; /* little table index */
  size_t k; /* big table sub index */
  double v1, v2;
  
  double brightness;
  double contrast;
  
  if (dev->inquiry_asic_type != AV_ASIC_OA980)
    invert_table = (s->c_mode == AV_THRESHOLDED) || (s->c_mode == AV_DITHERED);
  
  switch (dev->inquiry_asic_type)
    {
    case AV_ASIC_Cx:
    case AV_ASIC_C1: /* from avision code */
      gamma_table_raw_size = 4096;
      gamma_table_size = 2048;
      break;
    case AV_ASIC_C5:
      gamma_table_raw_size = 256;
      gamma_table_size = 256;
      break;
    case AV_ASIC_C6: /* SPEC claims: 256 ... ? */
      gamma_table_raw_size = 512;
      gamma_table_size = 512;
      break;
    case AV_ASIC_OA980:
      gamma_table_raw_size = 4096;
      gamma_table_size = 4096;
      break;
    case AV_ASIC_OA982:
      gamma_table_raw_size = 256;
      gamma_table_size = 256;
      break;
    default:
      gamma_table_raw_size = gamma_table_size = 4096;
    }
  
  gamma_values = gamma_table_size / 256;
  
  DBG (3, "send_gamma: table_raw_size: %lu, table_size: %lu\n",
       (u_long) gamma_table_raw_size, (u_long) gamma_table_size);
  DBG (3, "send_gamma: values: %lu, invert_table: %d\n",
       (u_long) gamma_values, invert_table);
  
  /* prepare for emulating contrast, brightness ... via the gamma-table */
  brightness = SANE_UNFIX (s->val[OPT_BRIGHTNESS].w);
  brightness /= 100;
  contrast = SANE_UNFIX (s->val[OPT_CONTRAST].w);
  contrast /= 100;
  
  DBG (3, "send_gamma: brightness: %f, contrast: %f\n", brightness, contrast);
  
  gamma_data = malloc (gamma_table_raw_size);
  if (!gamma_data)
    return SANE_STATUS_NO_MEM;
  
  memset (&scmd, 0, sizeof (scmd) );
  
  scmd.opc = AVISION_SCSI_SEND;
  scmd.datatypecode = 0x81; /* 0x81 for download gamma table */
  set_triple (scmd.transferlen, gamma_table_raw_size);
  
  for (color = 0; color < 3 && status == SANE_STATUS_GOOD; ++ color)
    {
      /* color: 0=red; 1=green; 2=blue */
      set_double (scmd.datatypequal, color);
      
      i = 0; /* big table index */
      for (j = 0; j < 256; ++ j) /* little table index */
	{
	  /* calculate mode dependent values v1 and v2
	   * v1 <- current value for table
	   * v2 <- next value for table (for interpolation)
	   */
	  switch (s->c_mode)
	    {
	    case AV_TRUECOLOR:
	    case AV_TRUECOLOR12:
	    case AV_TRUECOLOR16:
	      {
		v1 = (double) s->gamma_table [1 + color][j];
		if (j == 255)
		  v2 = (double) v1;
		else
		  v2 = (double) s->gamma_table [1 + color][j + 1];
	      }
	      break;
	    default:
	      /* for all other modes: */
	      {
		v1 = (double) s->gamma_table [0][j];
		if (j == 255)
		  v2 = (double) v1;
		else
		  v2 = (double) s->gamma_table [0][j + 1];
	      }
	    } /*end switch */
	  
	  /* Emulate brightness and contrast (at least the Avision AV6[2,3]0
	   * as well as many others do not have a hardware implementation,
	   * --$. The function was taken from the GIMP source - maybe I'll
	   * optimize it in the future (when I have spare time). */
	  
	  v1 /= 255;
	  v2 /= 255;
	      
	  v1 = (brightness_contrast_func (brightness, contrast, v1) );
	  v2 = (brightness_contrast_func (brightness, contrast, v2) );
	      
	  v1 *= 255;
	  v2 *= 255;
	  
	  if (invert_table) {
	    v1 = 255 - v1;
	    v2 = 255 - v2;
	    if (v1 <= 0)
	      v1 = 0;
	    if (v2 <= 0)
	      v2 = 0;
	  }
	  
	  for (k = 0; k < gamma_values; ++ k, ++ i) {
	    gamma_data [i] = (u_int8_t)
	      (((v1 * (gamma_values - k)) + (v2 * k) ) / (double) gamma_values);
	  }
	}
      /* fill the gamma table - (e.g.) if 11bit (old protocol) table */
      {
	size_t t_i = i-1;
	if (i < gamma_table_raw_size) {
	  DBG (4, "send_gamma: (old protocol) - filling the table.\n");
	  for ( ; i < gamma_table_raw_size; ++ i)
	    gamma_data [i] = gamma_data [t_i];
	}
      }
      
      DBG (4, "send_gamma: sending %lu bytes gamma table.\n",
	   (u_long) gamma_table_raw_size);
      status = avision_cmd (&s->av_con, &scmd, sizeof (scmd),
			    gamma_data, gamma_table_raw_size, 0, 0);
      
      if (status != SANE_STATUS_GOOD) {
	DBG (1, "send_gamma: gamma table upload failed: %s\n",
	     sane_strstatus (status));
      }
    }
  free (gamma_data);
  return status;
}

static SANE_Status
get_acceleration_info (Avision_Scanner* s, struct acceleration_info* info)
{
  SANE_Status status;
  
  struct command_read rcmd;
  u_int8_t result [24];
  size_t size;
  
  DBG (3, "get_acceleration_info:\n");

  size = sizeof (result);
  
  memset (&rcmd, 0, sizeof (rcmd));
  rcmd.opc = AVISION_SCSI_READ;
  rcmd.datatypecode = 0x6c; /* get acceleration information */
  set_double (rcmd.datatypequal, s->hw->data_dq);
  set_triple (rcmd.transferlen, size);
  
  DBG (3, "get_acceleration_info: read_data: %lu bytes\n", (u_long) size);
  status = avision_cmd (&s->av_con, &rcmd, sizeof (rcmd), 0, 0, result, &size);
  if (status != SANE_STATUS_GOOD || size != sizeof (result) ) {
    DBG (1, "get_acceleratoin_info: read accel. info failt (%s)\n",
	 sane_strstatus (status) );
    return status;
  }
  
  debug_print_accel_info (3, "get_acceleration_info", result);
  
  info->accel_step_count = get_double (&(result[0]));
  info->stable_step_count = get_double (&(result[2]));
  info->table_units = get_quad (&(result[4]));
  info->base_units = get_double (&(result[8]));
  info->start_speed = get_double (&(result[12]));
  info->target_speed = get_double (&(result[14]));
  info->ability = result[16];
  info->table_count = result[17];
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
send_acceleration_table (Avision_Scanner* s)
{
  SANE_Status status;
  
  struct command_send scmd;
  int t;
  struct acceleration_info accel_info;
  u_int8_t* table_data;
  
  DBG (3, "send_acceleration_table:\n");
  
  status = get_acceleration_info (s, &accel_info);
  
  /* so far I assume we have one byte tables as used in the C6 ASSIC ... */
  table_data = malloc (accel_info.accel_step_count);
  
  for (t = 0; t < accel_info.table_count && status == SANE_STATUS_GOOD; ++t)
    {
      memset (&scmd, 0x00, sizeof (scmd));
      scmd.opc = AVISION_SCSI_SEND;
      scmd.datatypecode = 0x6c; /* send acceleration table */
      
      set_double (scmd.datatypequal, t);
      set_triple (scmd.transferlen, accel_info.accel_step_count);
      
      /* contruct the table */
      {
	int i;
	int n = accel_info.accel_step_count - accel_info.stable_step_count;
	u_int16_t start_speed = accel_info.start_speed - 20 * t;
	for (i = 0; i < n; ++i)
	  table_data [i] = start_speed -
	    ( ( start_speed - accel_info.target_speed) * i / n);
	
	for (; i < accel_info.accel_step_count; ++i)
	  table_data [i] = accel_info.target_speed;
      }
      
      DBG (1, "send_acceleration_table: sending table %d\n", t);
      
      debug_print_raw (5, "send_acceleration_table: \n",
		       table_data, accel_info.accel_step_count);
      
      status = avision_cmd (&s->av_con, &scmd, sizeof (scmd),
			    (char*) table_data, accel_info.accel_step_count,
                            0, 0);
      if (status != SANE_STATUS_GOOD) {
	DBG (3, "send_acceleration_table: send_data failed (%s)\n",
	     sane_strstatus (status));
      }
    }
  
  free (table_data);
  
  return status;
}

static SANE_Status
set_window (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  SANE_Status status;
  int base_dpi;
  int transferlen;
  int paralen;
  
  int bytes_per_line;
  int line_count;
  
  struct {
    struct command_set_window cmd;
    struct command_set_window_window window;
  } cmd;
  
  DBG (1, "set_window:\n");
  
  /* plain old scanners, the C3 ASIC HP 53xx and the C6 ASIC HP 74xx do use
     1200 as base */
  if (dev->inquiry_asic_type != AV_ASIC_C5) {
    base_dpi = 1200;
  }
  else {
    /* round down to the next multiple of 300 */
    base_dpi = s->avdimen.xres - s->avdimen.xres % 300;
    if (base_dpi > dev->inquiry_optical_res)
      base_dpi = dev->inquiry_optical_res;
    else if (s->avdimen.xres <= 150)
      base_dpi = 150;
  }
  
  DBG (2, "set_window: base_dpi = %d\n", base_dpi);
  
  /* wipe out anything */
  memset (&cmd, 0, sizeof (cmd) );
  cmd.window.descriptor.winid = AV_WINID; /* normally defined to be zero */
  
  /* optional parameter length to use */
  paralen = sizeof (cmd.window.avision) - sizeof (cmd.window.avision.type);
  
  DBG (2, "set_window: base paralen: %d\n", paralen);
  
  if (dev->hw->feature_type & AV_FUJITSU)
    paralen += sizeof (cmd.window.avision.type.fujitsu);
  else if (!dev->inquiry_new_protocol)
    paralen += sizeof (cmd.window.avision.type.old);
  else
    paralen += sizeof (cmd.window.avision.type.normal);
  
  DBG (2, "set_window: final paralen: %d\n", paralen);
  
  transferlen = sizeof (cmd.window)
    - sizeof (cmd.window.avision) + paralen;
  
  DBG (2, "set_window: transferlen: %d\n", transferlen);
  
  /* command setup */
  cmd.cmd.opc = AVISION_SCSI_SET_WINDOW;
  set_triple (cmd.cmd.transferlen, transferlen);
  set_double (cmd.window.header.desclen,
	      sizeof (cmd.window.descriptor) + paralen);
  
  /* resolution parameters */
  set_double (cmd.window.descriptor.xres, s->avdimen.xres);
  set_double (cmd.window.descriptor.yres, s->avdimen.yres);
  
  /* upper left corner x/y as well as width/length in inch * base_dpi
     - avdimen are world pixels */
  set_quad (cmd.window.descriptor.ulx, s->avdimen.tlx * base_dpi / s->avdimen.xres);
  set_quad (cmd.window.descriptor.uly, s->avdimen.tly * base_dpi / s->avdimen.yres);
  
  set_quad (cmd.window.descriptor.width,
	    s->params.pixels_per_line * base_dpi / s->avdimen.xres);
  set_quad (cmd.window.descriptor.length,
	    (s->params.lines + s->avdimen.line_difference) * base_dpi / s->avdimen.yres);
  
  line_count = s->params.lines + s->avdimen.line_difference;
  
  /* interlaced duplex scans are twice as long */
  if (s->avdimen.interlaced_duplex) {
    DBG (2, "set_window: interlaced duplex scan, doubled line count\n");
    line_count *= 2;
  }
  
  /* we always pass 16bit data thru the API - thus we need to translate
     the 12 bit per channel modes */
  switch (s->c_mode)
    {
    case AV_GRAYSCALE12:
      bytes_per_line = s->params.bytes_per_line * 12 / 16;
      break;
    case AV_TRUECOLOR12:
      bytes_per_line = s->params.bytes_per_line * 36 / 48;
      break;
    default:
      bytes_per_line = s->params.bytes_per_line;
    }
  
  set_double (cmd.window.avision.line_width, bytes_per_line);
  set_double (cmd.window.avision.line_count, line_count);
  
  /* here go the most significant bits if bigger than 16 bit */
  if (dev->inquiry_new_protocol && !(dev->hw->feature_type & AV_FUJITSU) ) {
    DBG (2, "set_window: large data-transfer support (>16bit)!\n");
    cmd.window.avision.type.normal.line_width_msb =
      bytes_per_line >> 16;
    cmd.window.avision.type.normal.line_count_msb =
      line_count >> 16;
  }
  
  /* scanner should use our line-width and count */
  SET_BIT (cmd.window.avision.bitset1, 6); 
  
  /* set speed */
  cmd.window.avision.bitset1 |= s->val[OPT_SPEED].w & 0x07; /* only 3 bit */
  
  /* ADF scan */
  
  DBG (3, "set_window: source mode %d source mode dim %d\n",
       s->source_mode, s->source_mode_dim);
  if (s->source_mode_dim == AV_ADF_DIM) {
    int adf_mode = adf_mode; /* silence GCC */
    
    switch (s->source_mode) {
    case AV_ADF:
      adf_mode = 0;
      break;
    case AV_ADF_REAR:
      adf_mode = 1;
      break;
    case AV_ADF_DUPLEX:
      adf_mode = 2;
      break;
    default:
      ; /* silence GCC */
    }
    SET_BIT (cmd.window.avision.bitset1, 7);
    cmd.window.avision.type.normal.bitset3 |= (adf_mode << 3);
  }
  
  /* newer scanners (AV610 and equivalent) need the paper length to detect
     double feeds - they may not work without the field set */
  
  set_double (cmd.window.descriptor.paper_length, (int)((double)30.0*1200));
    
  if ( !(dev->hw->feature_type & AV_FUJITSU) )
    { 
      /* quality scan option switch */
      if (s->val[OPT_QSCAN].w == SANE_TRUE) {
	SET_BIT (cmd.window.avision.type.normal.bitset2, 4);
      }
  
      /* quality calibration option switch */
      if (s->val[OPT_QCALIB].w == SANE_TRUE) {
	SET_BIT (cmd.window.avision.type.normal.bitset2, 3);
      }
      
      /* transparency option switch */
      if (s->source_mode_dim == AV_TRANSPARENT_DIM) {
	SET_BIT (cmd.window.avision.type.normal.bitset2, 7);
      }
      
      if (dev->scanner_type == AV_FILM) {
	set_double (cmd.window.avision.type.normal.r_exposure_time, 0x016d);
	set_double (cmd.window.avision.type.normal.g_exposure_time, 0x016d);
	set_double (cmd.window.avision.type.normal.b_exposure_time, 0x016d);
      }
    }
  
  /* fixed values */
  cmd.window.descriptor.padding_and_bitset = 3;
  cmd.window.descriptor.vendor_specific = 0xFF;
  cmd.window.descriptor.paralen = paralen; /* R² was: 9, later 14 */

  /* This is normaly unsupported by Avsion scanners, and we do this
     via the gamma table - which works for all devices ... */
  cmd.window.descriptor.threshold = 128;
  cmd.window.descriptor.brightness = 128; 
  cmd.window.descriptor.contrast = 128;
  cmd.window.avision.highlight = 0xFF;
  cmd.window.avision.shadow = 0x00;
  
  /* mode dependant settings */
  switch (s->c_mode)
    {
    case AV_THRESHOLDED:
      cmd.window.descriptor.bpc = 1;
      cmd.window.descriptor.image_comp = 0;
      /* cmd.window.avision.bitset1 &= 0xC7;*/ /* no filter */
      break;
    
    case AV_DITHERED:
      cmd.window.descriptor.bpc = 1;
      cmd.window.descriptor.image_comp = 1;
      /* cmd.window.avision.bitset1 &= 0xC7;*/ /* no filter */
      break;
      
    case AV_GRAYSCALE:
      cmd.window.descriptor.bpc = 8;
      cmd.window.descriptor.image_comp = 2;
      /* cmd.window.avision.bitset1 &= 0xC7;*/ /* no filter */
      break;
      
    case AV_GRAYSCALE12:
      cmd.window.descriptor.bpc = 12;
      cmd.window.descriptor.image_comp = 2;
      /* cmd.window.avision.bitset1 &= 0xC7;*/ /* no filter */
      break;

    case AV_GRAYSCALE16:
      cmd.window.descriptor.bpc = 16;
      cmd.window.descriptor.image_comp = 2;
      /* cmd.window.avision.bitset1 &= 0xC7;*/ /* no filter */
      break;
      
    case AV_TRUECOLOR:
      cmd.window.descriptor.bpc = 8;
      cmd.window.descriptor.image_comp = 5;
      cmd.window.avision.bitset1 |= 0x20; /* rgb one-pass filter */
      break;

    case AV_TRUECOLOR12:
      cmd.window.descriptor.bpc = 12;
      cmd.window.descriptor.image_comp = 5;
      cmd.window.avision.bitset1 |= 0x20; /* rgb one-pass filter */
      break;

    case AV_TRUECOLOR16:
      cmd.window.descriptor.bpc = 16;
      cmd.window.descriptor.image_comp = 5;
      cmd.window.avision.bitset1 |= 0x20; /* rgb one-pass filter */
      break;
      
    default:
      DBG (1, "Invalid mode. %d\n", s->c_mode);
      return SANE_STATUS_INVAL;
    }
  
  debug_print_window_descriptor (5, "set_window", &(cmd.window));
  
  DBG (3, "set_window: sending command. Bytes: %d\n", transferlen);
  status = avision_cmd (&s->av_con, &cmd, sizeof (cmd.cmd),
			&(cmd.window), transferlen, 0, 0);
  
  return status;
}

static SANE_Status
reserve_unit (Avision_Scanner* s)
{
  char cmd[] =
    {AVISION_SCSI_RESERVE_UNIT, 0, 0, 0, 0, 0};
  SANE_Status status;
  
  DBG (1, "reserve_unit:\n");
  
  status = avision_cmd (&s->av_con, cmd, sizeof (cmd), 0, 0, 0, 0);
  return status;
}

static SANE_Status
release_unit (Avision_Scanner* s)
{
  char cmd[] =
    {AVISION_SCSI_RELEASE_UNIT, 0, 0, 0, 0, 0};
  SANE_Status status;
  
  DBG (1, "release unit:\n");
  
  status = avision_cmd (&s->av_con, cmd, sizeof (cmd), 0, 0, 0, 0);
  return status;
}

/* Check if a sheet is present. */
static SANE_Status
media_check (Avision_Scanner* s)
{
  char cmd[] = {AVISION_SCSI_MEDIA_CHECK, 0, 0, 0, 1, 0}; /* 1, 4 */
  SANE_Status status;
  u_int8_t result[1]; /* 4 */
  size_t size = sizeof(result);
  
  status = avision_cmd (&s->av_con, cmd, sizeof (cmd),
			0, 0, result, &size);
  
  debug_print_raw (5, "media_check: result\n", result, size);
  
  if (status == SANE_STATUS_GOOD) {
    if (!(result[0] & 0x1))
      status = SANE_STATUS_NO_DOCS;
  }
  
  return status;
}

static SANE_Status
object_position (Avision_Scanner* s, u_int8_t position)
{
  SANE_Status status;
  
  u_int8_t cmd [10];
  
  memset (cmd, 0, sizeof (cmd));
  cmd[0] = AVISION_SCSI_OBJECT_POSITION;
  cmd[1] = position;
  
  DBG (1, "object_position: %d\n", position);
  
  status = avision_cmd (&s->av_con, cmd, sizeof(cmd), 0, 0, 0, 0);
  return status;
}

static SANE_Status
start_scan (Avision_Scanner* s)
{
  struct command_scan cmd;
  
  size_t size = sizeof (cmd);
  
  DBG (3, "start_scan:\n");
  
  memset (&cmd, 0, sizeof (cmd));
  cmd.opc = AVISION_SCSI_SCAN;
  cmd.transferlen = 1;
  
  if (s->val[OPT_PREVIEW].w == SANE_TRUE) {
    cmd.bitset1 |= 0x01<<6;
  }
  else {
    cmd.bitset1 &= ~(0x01<<6);
  }

  if (s->val[OPT_QSCAN].w == SANE_TRUE) {
    cmd.bitset1 |= 0x01<<7;
  }
  else {
    cmd.bitset1 &= ~(0x01<<7);
  }
  
  DBG (3, "start_scan: sending command. Bytes: %lu\n", (u_long) size);
  return avision_cmd (&s->av_con, &cmd, size, 0, 0, 0, 0);
}

static SANE_Status
do_eof (Avision_Scanner *s)
{
  int exit_status;
  
  DBG (3, "do_eof:\n");
  
  s->param_cksum = create_cksum (s);
  /* we can now mark the rear data as valid */
  if (s->avdimen.interlaced_duplex) {
    DBG (3, "do_eof: toggling duplex rear data valid\n");
    s->duplex_rear_valid = !s->duplex_rear_valid;
    DBG (3, "do_eof: duplex rear data valid: %x\n",
	 s->duplex_rear_valid);
  }
  
  if (s->pipe >= 0) {
    close (s->pipe);
    s->pipe = -1;
  }
  
  /* join our processes - without a wait() you will produce defunct
     childs */
  sanei_thread_waitpid (s->reader_pid, &exit_status);

  s->reader_pid = 0;

  DBG (3, "do_eof: returning %d\n", exit_status);
  
  return (SANE_Status)exit_status;
}

static SANE_Status
do_cancel (Avision_Scanner* s)
{
  DBG (3, "do_cancel:\n");
  
  /* TODO: reuse do_eof? */
  
  s->scanning = SANE_FALSE;
  s->duplex_rear_valid = SANE_FALSE;
  
  if (s->reader_pid > 0) {
    int exit_status;
    
    /* ensure child knows it's time to stop: */
    sanei_thread_kill (s->reader_pid);
    sanei_thread_waitpid (s->reader_pid, &exit_status);
    s->reader_pid = 0;
  }
  
  return SANE_STATUS_CANCELLED;
}

static SANE_Status
read_data (Avision_Scanner* s, SANE_Byte* buf, size_t* count)
{
  struct command_read rcmd;
  SANE_Status status;

  DBG (9, "read_data: %lu\n", (u_long) *count);
  
  memset (&rcmd, 0, sizeof (rcmd));
  
  rcmd.opc = AVISION_SCSI_READ;
  rcmd.datatypecode = 0x00; /* read image data */
  set_double (rcmd.datatypequal, s->hw->data_dq);
  set_triple (rcmd.transferlen, *count);
  
  status = avision_cmd (&s->av_con, &rcmd, sizeof (rcmd), 0, 0, buf, count);
  
  return status;
}

static SANE_Status
init_options (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  int i;
  
  DBG (3, "init_options:\n");
  
  memset (s->opt, 0, sizeof (s->opt));
  memset (s->val, 0, sizeof (s->val));

  for (i = 0; i < NUM_OPTIONS; ++ i) {
    s->opt[i].size = sizeof (SANE_Word);
    s->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }
  
  /* Init the SANE option from the scanner inquiry data */
  
  dev->x_range.max = SANE_FIX ( (int)dev->inquiry_x_ranges[s->source_mode_dim]);
  dev->x_range.quant = 0;
  dev->y_range.max = SANE_FIX ( (int)dev->inquiry_y_ranges[s->source_mode_dim]);
  dev->y_range.quant = 0;
  
  if (dev->inquiry_asic_type >= AV_ASIC_C6) {
    DBG (1, "init_options: dpi_range.min set to 75 due to device-list!!\n");
    dev->dpi_range.min = 75;
  }
  else
    dev->dpi_range.min = 50;

  dev->dpi_range.quant = 5;
  dev->dpi_range.max = dev->inquiry_max_res;
  
  dev->speed_range.min = (SANE_Int)0;
  dev->speed_range.max = (SANE_Int)4;
  dev->speed_range.quant = (SANE_Int)1;
  
  s->opt[OPT_NUM_OPTS].name = "";
  s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].desc = "";
  s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  s->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  s->opt[OPT_NUM_OPTS].size = sizeof(SANE_TYPE_INT);
  s->val[OPT_NUM_OPTS].w = NUM_OPTIONS;
  
  /* "Mode" group: */
  s->opt[OPT_MODE_GROUP].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_MODE_GROUP].desc = ""; /* for groups only title and type are valid */
  s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MODE_GROUP].cap = 0;
  s->opt[OPT_MODE_GROUP].size = 0;
  s->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* scan mode */
  s->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  s->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  s->opt[OPT_MODE].type = SANE_TYPE_STRING;
  s->opt[OPT_MODE].size = max_string_size (dev->color_list);
  s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_MODE].constraint.string_list = dev->color_list;
  s->val[OPT_MODE].s = strdup (dev->color_list[dev->color_list_default]);
  
  s->c_mode = match_color_mode (dev, s->val[OPT_MODE].s);

  s->opt[OPT_SOURCE].name = SANE_NAME_SCAN_SOURCE;
  s->opt[OPT_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
  s->opt[OPT_SOURCE].desc = SANE_DESC_SCAN_SOURCE;
  s->opt[OPT_SOURCE].type = SANE_TYPE_STRING;
  s->opt[OPT_SOURCE].size = max_string_size(dev->source_list);
  s->opt[OPT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_SOURCE].constraint.string_list = &dev->source_list[0];
  s->val[OPT_SOURCE].s = strdup(dev->source_list[0]);
  
  s->source_mode = match_source_mode (dev, s->val[OPT_SOURCE].s);
  
  /* resolution */
  s->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_RESOLUTION].constraint.range = &dev->dpi_range;
  s->val[OPT_RESOLUTION].w = OPT_RESOLUTION_DEFAULT;

  /* preview */
  s->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  s->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  s->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  s->opt[OPT_PREVIEW].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  s->val[OPT_PREVIEW].w = 0;

  /* speed option */
  s->opt[OPT_SPEED].name  = SANE_NAME_SCAN_SPEED;
  s->opt[OPT_SPEED].title = SANE_TITLE_SCAN_SPEED;
  s->opt[OPT_SPEED].desc  = SANE_DESC_SCAN_SPEED;
  s->opt[OPT_SPEED].type  = SANE_TYPE_INT;
  s->opt[OPT_SPEED].constraint_type  = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_SPEED].constraint.range = &dev->speed_range; 
  s->val[OPT_SPEED].w = 0;
  if (dev->scanner_type == AV_SHEETFEED)
    s->opt[OPT_SPEED].cap |= SANE_CAP_INACTIVE;

  /* "Geometry" group: */
  s->opt[OPT_GEOMETRY_GROUP].title = "Geometry";
  s->opt[OPT_GEOMETRY_GROUP].desc = ""; /* for groups only title and type are valid */
  s->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_GEOMETRY_GROUP].size = 0;
  s->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* top-left x */
  s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  s->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_X].constraint.range = &dev->x_range;
  s->val[OPT_TL_X].w = 0;

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &dev->y_range;
  s->val[OPT_TL_Y].w = 0;

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &dev->x_range;
  s->val[OPT_BR_X].w = dev->x_range.max;

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &dev->y_range;
  s->val[OPT_BR_Y].w = dev->y_range.max;

  /* "Enhancement" group: */
  s->opt[OPT_ENHANCEMENT_GROUP].title = "Enhancement";
  s->opt[OPT_ENHANCEMENT_GROUP].desc = ""; /* for groups only title and type are valid */
  s->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ENHANCEMENT_GROUP].cap = 0;
  s->opt[OPT_ENHANCEMENT_GROUP].size = 0;
  s->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* brightness */
  s->opt[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].type = SANE_TYPE_FIXED;
  if (disable_gamma_table)
    s->opt[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_BRIGHTNESS].unit = SANE_UNIT_PERCENT;
  s->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BRIGHTNESS].constraint.range = &percentage_range;
  s->val[OPT_BRIGHTNESS].w = SANE_FIX(0);

  /* contrast */
  s->opt[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  s->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  s->opt[OPT_CONTRAST].desc = SANE_DESC_CONTRAST;
  s->opt[OPT_CONTRAST].type = SANE_TYPE_FIXED;
  if (disable_gamma_table)
    s->opt[OPT_CONTRAST].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_CONTRAST].unit = SANE_UNIT_PERCENT;
  s->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CONTRAST].constraint.range = &percentage_range;
  s->val[OPT_CONTRAST].w = SANE_FIX(0);

  /* Quality Scan */
  s->opt[OPT_QSCAN].name   = "quality-scan";
  s->opt[OPT_QSCAN].title  = "Quality scan";
  s->opt[OPT_QSCAN].desc   = "Turn on quality scanning (slower but better).";
  s->opt[OPT_QSCAN].type   = SANE_TYPE_BOOL;
  s->opt[OPT_QSCAN].unit   = SANE_UNIT_NONE;
  s->val[OPT_QSCAN].w      = SANE_TRUE;

  /* Quality Calibration */
  s->opt[OPT_QCALIB].name  = SANE_NAME_QUALITY_CAL;
  s->opt[OPT_QCALIB].title = SANE_TITLE_QUALITY_CAL;
  s->opt[OPT_QCALIB].desc  = SANE_DESC_QUALITY_CAL;
  s->opt[OPT_QCALIB].type  = SANE_TYPE_BOOL;
  s->opt[OPT_QCALIB].unit  = SANE_UNIT_NONE;
  s->val[OPT_QCALIB].w     = SANE_TRUE;

  /* grayscale gamma vector */
  s->opt[OPT_GAMMA_VECTOR].name = SANE_NAME_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR].title = SANE_TITLE_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR].desc = SANE_DESC_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GAMMA_VECTOR].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR].size = 256 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR].wa = &s->gamma_table[0][0];

  /* red gamma vector */
  s->opt[OPT_GAMMA_VECTOR_R].name = SANE_NAME_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].title = SANE_TITLE_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].desc = SANE_DESC_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GAMMA_VECTOR_R].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_R].size = 256 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR_R].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_R].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR_R].wa = &s->gamma_table[1][0];

  /* green gamma vector */
  s->opt[OPT_GAMMA_VECTOR_G].name = SANE_NAME_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].title = SANE_TITLE_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].desc = SANE_DESC_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GAMMA_VECTOR_G].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_G].size = 256 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR_G].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_G].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR_G].wa = &s->gamma_table[2][0];

  /* blue gamma vector */
  s->opt[OPT_GAMMA_VECTOR_B].name = SANE_NAME_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].title = SANE_TITLE_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].desc = SANE_DESC_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GAMMA_VECTOR_B].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_B].size = 256 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR_B].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_B].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR_B].wa = &s->gamma_table[3][0];
 
  if (!disable_gamma_table)
  {
    if (color_mode_is_color (dev->color_list_default)) {
      s->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
      s->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
      s->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
    }
    else {
      s->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
    }
  }

  /* scanner buttons pressed */
  for (i = OPT_BUTTON_0; i <= OPT_BUTTON_LAST; ++i)
    {
      char name [12];
      char title [128];
      sprintf (name, "button %d", i - OPT_BUTTON_0);
      sprintf (title, "Scanner button %d", i - OPT_BUTTON_0);
      s->opt[i].name = strdup (name);
      s->opt[i].title = strdup (title);
      s->opt[i].desc = "This options reflects a front pannel scanner button.";
      s->opt[i].type = SANE_TYPE_BOOL;
      s->opt[i].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
      
      if (i - OPT_BUTTON_0 >= dev->inquiry_buttons)
	s->opt[i].cap |= SANE_CAP_INACTIVE;
      
      s->opt[i].unit = SANE_UNIT_NONE;
      s->opt[i].size = sizeof (SANE_Word);
      s->opt[i].constraint_type = SANE_CONSTRAINT_RANGE;
      s->opt[i].constraint.range = 0;
      s->val[i].w = SANE_FALSE;
    }
  
  /* message, like options set on the scanner, LED no. & co */
  s->opt[OPT_MESSAGE].name = "message";
  s->opt[OPT_MESSAGE].title = "message text from the scanner";
  s->opt[OPT_MESSAGE].desc = "This text contains device specific options controlled on the scanner";
  s->opt[OPT_MESSAGE].type = SANE_TYPE_STRING;
  s->opt[OPT_MESSAGE].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
  s->opt[OPT_MESSAGE].size = 128;
  s->opt[OPT_MESSAGE].constraint_type = SANE_CONSTRAINT_NONE;
  s->val[OPT_MESSAGE].s = malloc(s->opt[OPT_MESSAGE].size);
  s->val[OPT_MESSAGE].s[0] = 0;
  
  /* film holder control */
  if (dev->scanner_type != AV_FILM || dev->holder_type == 0xff)
    s->opt[OPT_FRAME].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_FRAME].name = SANE_NAME_FRAME;
  s->opt[OPT_FRAME].title = SANE_TITLE_FRAME;
  s->opt[OPT_FRAME].desc = SANE_DESC_FRAME;
  s->opt[OPT_FRAME].type = SANE_TYPE_INT;
  s->opt[OPT_FRAME].unit = SANE_UNIT_NONE;
  s->opt[OPT_FRAME].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_FRAME].constraint.range = &dev->frame_range;
  s->val[OPT_FRAME].w = dev->current_frame;
  
  return SANE_STATUS_GOOD;
}

/* This function is executed as a child process. The reason this is
   executed as a subprocess is because some (most?) generic SCSI
   interfaces block a SCSI request until it has completed. With a
   subprocess, we can let it block waiting for the request to finish
   while the main process can go about to do more important things
   (such as recognizing when the user presses a cancel button).

   WARNING: Since this is executed as a subprocess, it's NOT possible
   to update any of the variables in the main process (in particular
   the scanner state cannot be updated).  */

static int
reader_process (void *data)
{
  struct Avision_Scanner *s = (struct Avision_Scanner *) data;
  int fd = s->reader_fds;

  Avision_Device* dev = s->hw;
  
  SANE_Status status;
  SANE_Status exit_status;
  sigset_t sigterm_set;
  sigset_t ignore_set;
  struct SIGACTION act;
  
  FILE* fp;
  FILE* rear_fp = 0; /* used to store the deinterlaced rear data */
  
  /* the complex params */
  size_t bytes_per_line;
  size_t pixels_per_line;
  size_t lines_per_stripe;
  size_t lines_per_output;
  size_t max_bytes_per_read;
  
  SANE_Bool gray_mode;
  
  /* the simple params for the data reader */
  size_t stripe_size;
  size_t stripe_fill;
  size_t out_size;
  
  size_t total_size;
  size_t processed_bytes;
  
  SANE_Bool deinterlace;
  size_t stripe = 0;
  
  /* the fat strip we currently puzzle together to perform software-colorpack
     and more */
  u_int8_t* stripe_data;
  /* the corrected output data */
  u_int8_t* out_data;
  
  DBG (3, "reader_process:\n");
  
  if (sanei_thread_is_forked())
    close (s->pipe);
  
  sigfillset (&ignore_set);
  sigdelset (&ignore_set, SIGTERM);
#if defined (__APPLE__) && defined (__MACH__)
  sigdelset (&ignore_set, SIGUSR2);
#endif
  sigprocmask (SIG_SETMASK, &ignore_set, 0);
  
  memset (&act, 0, sizeof (act));
  sigaction (SIGTERM, &act, 0);
  
  sigemptyset (&sigterm_set);
  sigaddset (&sigterm_set, SIGTERM);
  
  gray_mode = color_mode_is_shaded (s->c_mode);
  deinterlace = s->avdimen.interlaced_duplex;
  
  fp = fdopen (fd, "w");
  if (!fp)
    return SANE_STATUS_NO_MEM;
  
  /* start scan ? */
  if (!deinterlace || (deinterlace && !s->duplex_rear_valid))
    {
      /* reserve unit - in the past we did this in open - but the
	 windows driver does reserve for each scan and some ADF
	 devices need a release for each sheet anyway ... */
      status = reserve_unit (s);
      if (status != SANE_STATUS_GOOD) {
	DBG (1, "reader_process: reserve_unit failed: %s\n",
	     sane_strstatus (status));
	return status;
      }
  
      status = start_scan (s);
      if (status != SANE_STATUS_GOOD) {
	DBG (1, "reader_process: start_scan faild: %s\n",
	     sane_strstatus (status));
	return status;
      }
      
      if (dev->hw->feature_type & AV_ACCEL_TABLE)
          /* (s->hw->inquiry_asic_type == AV_ASIC_C6) */ {
	status = send_acceleration_table (s);
	if (status != SANE_STATUS_GOOD) {
	  DBG (1, "reader_process: send_acceleration_table failed: %s\n",
	       sane_strstatus (status));
	  return status;
	}
      }
    }
  
  /* setup file i/o for deinterlacing scans */
  if (deinterlace)
    {
      DBG (3, "reader_process: opening duplex rear file for writing.\n");
      if (!s->duplex_rear_valid) { /* create new file for writing */
	rear_fp = fopen (s->duplex_rear_fname, "w");
	if (! rear_fp) {
	  fclose (fp);
	  return SANE_STATUS_NO_MEM;
	}
      }
      else { /* open saved rear data */
	DBG (3, "reader_process: opening duplex rear file for reading.\n");
	rear_fp = fopen (s->duplex_rear_fname, "r");
	if (! rear_fp) {
	  fclose (fp);
	  return SANE_STATUS_IO_ERROR;
	}
      }
    }
  
  bytes_per_line = s->params.bytes_per_line;
  pixels_per_line = bytes_per_line;
  
  switch (s->c_mode) {
  case AV_GRAYSCALE12:
    pixels_per_line = pixels_per_line * 12 / 16;
    break;
  case AV_GRAYSCALE16:
    pixels_per_line /= 2;
    break;
  case AV_TRUECOLOR:
    pixels_per_line /= 3;
    break;
  case AV_TRUECOLOR12:
    pixels_per_line = pixels_per_line * 36 / 48;
    break;
  case AV_TRUECOLOR16:
    pixels_per_line /= 6;
    break;
  default:
    ;
  }
  
  /* lines_per_stripe is either the line-difference, some ADF
     deinterlace constant or in the remaining cases just 16 for a
     pleasuant preview */
  
  if (!deinterlace) {
    lines_per_stripe = s->avdimen.line_difference * 2;
  }
  else {
    /* In theory the SPEC has a command to request the value from the scanner.
       However in practice the AV220 does not respond to the command ... */
    lines_per_stripe = 8;
  }
  
  if (lines_per_stripe == 0) {
    lines_per_stripe = 16;
  }
  
  stripe_size = bytes_per_line * lines_per_stripe;
  lines_per_output = lines_per_stripe - s->avdimen.line_difference;
  
  /* The "/2" might make scans faster - because it should leave some
     space in the SCSI buffers (scanner, kernel, ...) empty to
     read/send _ahead_ ... */
  if (s->av_con.connection_type == AV_SCSI)
    max_bytes_per_read = dev->scsi_buffer_size / 2;
  else {
    max_bytes_per_read = (0x10000 / bytes_per_line) * bytes_per_line;
  }
  
  out_size = bytes_per_line * lines_per_output;
  
  DBG (3, "dev->scsi_buffer_size / 2: %d\n",
       dev->scsi_buffer_size / 2);
  
  DBG (3, "bytes_per_line: %lu, pixels_per_line: %lu\n",
       (u_long) bytes_per_line, (u_long) pixels_per_line);
  
  DBG (3, "lines_per_stripe: %lu, lines_per_output: %lu\n",
       (u_long) lines_per_stripe, (u_long) lines_per_output);
  
  DBG (3, "max_bytes_per_read: %lu, stripe_size: %lu, out_size: %lu\n",
       (u_long) max_bytes_per_read, (u_long) stripe_size, (u_long) out_size);
  
  stripe_data = malloc (stripe_size);
  out_data = malloc (out_size);
  
  s->line = 0;
  
  /* calculate params for the simple reader */
  total_size = bytes_per_line * (s->params.lines + s->avdimen.line_difference);
  if (deinterlace && !s->duplex_rear_valid)
    total_size *= 2;
  DBG (3, "reader_process: total_size: %lu\n", (u_long) total_size);
  
  processed_bytes = 0;
  stripe_fill = 0;
  
  /* Data read loop until all data has been processed.  Might exit
     before all lines are transferred for ADF paper end. */
  while (processed_bytes < total_size)
    {
      int useful_bytes;
      
      /* fill the stripe buffer with real data*/
      while (!s->duplex_rear_valid &&
	     processed_bytes < total_size && stripe_fill < stripe_size)
	{
	  size_t this_read = stripe_size - stripe_fill;
	  
	  /* Limit reads to max_bytes_per_read and global data
	     boundaries. Rounded to the next lower multiple of
	     byte_per_lines, otherwise some scanners freeze */
	  if (this_read > max_bytes_per_read)
	    this_read = max_bytes_per_read - max_bytes_per_read % bytes_per_line;

	  if (processed_bytes + this_read > total_size)
	    this_read = total_size - processed_bytes;

          read_constrains(s, this_read);

	  DBG (5, "reader_process: processed_bytes: %lu, total_size: %lu\n",
	       (u_long) processed_bytes, (u_long) total_size);
	  
	  DBG (5, "reader_process: this_read: %lu\n",
	       (u_long) this_read);

	  sigprocmask (SIG_BLOCK, &sigterm_set, 0);
	  status = read_data (s, stripe_data + stripe_fill, &this_read);
	  sigprocmask (SIG_UNBLOCK, &sigterm_set, 0);
	  
	  if (status == SANE_STATUS_EOF || this_read == 0) {
	    DBG (1, "reader_process: read_data failed due to EOF\n");
	    exit_status = SANE_STATUS_EOF;
	    goto cleanup;
	  }
	  
	  if (status != SANE_STATUS_GOOD) {
	    DBG (1, "reader_process: read_data failed with status: %d\n",
		 status);
	    exit_status = status;
	    goto cleanup;
	  }
          
	  stripe_fill += this_read;
	  processed_bytes += this_read;
	}
      
      /* fill the stripe buffer with stored, virtual data */
      if (s->duplex_rear_valid)
	{
	  size_t this_read = stripe_size - stripe_fill;
	  size_t got;
	  
	  /* limit reads to max_read and global data boundaries */
	  if (this_read > max_bytes_per_read)
	    this_read = max_bytes_per_read;
	  
	  if (processed_bytes + this_read > total_size)
	    this_read = total_size - processed_bytes;
	  
	  DBG (5, "reader_process: virtual processed_bytes: %lu, total_size: %lu\n",
	       (u_long) processed_bytes, (u_long) total_size);
	  
	  DBG (5, "reader_process: virtual this_read: %lu\n",
	       (u_long) this_read);
	  
	  got = fread (stripe_data + stripe_fill, 1, this_read, rear_fp);
	  if (got != this_read) {
	    exit_status = SANE_STATUS_EOF;
	    goto cleanup;
	  }
          
	  stripe_fill += this_read;
	  processed_bytes += this_read;
	}
      
      DBG (5, "reader_process: stripe filled\n");
      
      useful_bytes = stripe_fill;
      if (s->c_mode == AV_TRUECOLOR || s->c_mode == AV_TRUECOLOR16)
	useful_bytes -= s->avdimen.line_difference * bytes_per_line;
      
      DBG (3, "reader_process: useful_bytes %i\n", useful_bytes);
      
      /* Save the rear stripes to deinterlace. For some scanners
       (AV220) that is every 2nd stripe - for others (AV83xx) this is
       the 2nd half of the transferred data. */
      if (deinterlace && !s->duplex_rear_valid)
	if ((dev->inquiry_duplex_mode_two && processed_bytes > total_size/2) ||
	    (!(dev->inquiry_duplex_mode_two) && stripe % 2)) {
	DBG (5, "reader_process: rear stripe -> saving to temporary file.\n");
	fwrite (stripe_data, bytes_per_line,
		useful_bytes / bytes_per_line, rear_fp);
	
	stripe_fill -= useful_bytes;
	/* TODO: copy remaining data, if any, to the front for line
	   diff compensation */
	goto iteration_end;
      }
      
      /*
       * Perform needed data convertions (packing, ...) and/or copy the
       * image data.
       */
      
      if (s->c_mode != AV_TRUECOLOR && s->c_mode != AV_TRUECOLOR16)
	/* simple copy */
	{
	  memcpy (out_data, stripe_data, useful_bytes);
	}
      else /* AV_TRUECOLOR* */
	{
	  /* WARNING: DO NOT MODIFY MY (HOPEFULLY WELL) OPTMIZED
	     ALGORITHMS BELOW, WITHOUT UNDERSTANDING THEM FULLY ! */
	  if (s->avdimen.line_difference > 0) /* color-pack */
	    {
	      /* TODO: add 16bit per sample code? */
	      int i;
	      int c_offset = (s->avdimen.line_difference / 3) * bytes_per_line;
	      
	      u_int8_t* r_ptr = stripe_data;
	      u_int8_t* g_ptr = stripe_data + c_offset + 1;
	      u_int8_t* b_ptr = stripe_data + 2 * c_offset + 2;
	      
	      for (i = 0; i < useful_bytes;) {
		out_data [i++] = *r_ptr; r_ptr += 3;
		out_data [i++] = *g_ptr; g_ptr += 3;
		out_data [i++] = *b_ptr; b_ptr += 3;
	      }
	    } /* end color pack */
	  else if (dev->inquiry_needs_line_pack) /* line-pack */
	    { 
	      /* TODO: add 16bit per sample code? */
	      int i = 0;
	      size_t l, p;
	      
	      size_t lines = useful_bytes / bytes_per_line;
	      
	      for (l = 0; l < lines; ++ l)
		{
		  u_int8_t* r_ptr = stripe_data + (bytes_per_line * l);
		  u_int8_t* g_ptr = r_ptr + pixels_per_line;
		  u_int8_t* b_ptr = g_ptr + pixels_per_line;
		  
		  for (p = 0; p < pixels_per_line; ++ p) {
		    out_data [i++] = *(r_ptr++);
		    out_data [i++] = *(g_ptr++);
		    out_data [i++] = *(b_ptr++);
		  }
		}
	    } /* end line pack */
	  else /* else no packing was required -> simple copy */
	    { 
	      memcpy (out_data, stripe_data, useful_bytes);
	    }
	} /* end if AV_TRUECOLOR* */
      
      /* FURTHER POST-PROCESSING ON THE FINAL OUTPUT DATA */
      
      /* maybe mirroring in ADF mode */
      if (s->source_mode_dim == AV_ADF_DIM && dev->inquiry_adf_need_mirror) {
	if ( (s->c_mode != AV_TRUECOLOR) ||
	     (s->c_mode == AV_TRUECOLOR && dev->inquiry_adf_bgr_order) )
	  {
	    /* Mirroring with bgr -> rgb convertion: Just mirror the
	     * whole line */
	    
	    int l;
	    int lines = useful_bytes / bytes_per_line;
	    
	    for (l = 0; l < lines; ++ l)
	      {
		u_int8_t* begin_ptr = out_data + (l * bytes_per_line);
		u_int8_t* end_ptr = begin_ptr + bytes_per_line;
		
		while (begin_ptr < end_ptr) {
		  u_int8_t tmp;
		  tmp = *begin_ptr;
		  *begin_ptr++ = *end_ptr;
		  *end_ptr-- = tmp;
		}
	      }
	  }
	else /* non trival mirroring */
	  {
	    /* Non-trival Mirroring with element swapping */
	    
	    int l;
	    int lines = useful_bytes / bytes_per_line;
	    
	    for (l = 0; l < lines; ++ l)
	      {
		u_int8_t* begin_ptr = out_data + (l * bytes_per_line);
		u_int8_t* end_ptr = begin_ptr + bytes_per_line - 2;
		
		while (begin_ptr < end_ptr) {
		  u_int8_t tmp;
		  
		  /* R */
		  tmp = *begin_ptr;
		  *begin_ptr++ = *end_ptr;
		  *end_ptr++ = tmp;
		  
		  /* G */
		  tmp = *begin_ptr;
		  *begin_ptr++ = *end_ptr;
		  *end_ptr++ = tmp;
		  
		  /* B */
		  tmp = *begin_ptr;
		  *begin_ptr++ = *end_ptr;
		  *end_ptr = tmp;
		  
		  end_ptr -= 5;
		}
	      }
	  }
      } /* end if mirroring needed */

      /* byte swapping and software calibration 16bit mode */
      if (s->c_mode == AV_GRAYSCALE16 || s->c_mode == AV_TRUECOLOR16) {
	
	int l;
	int lines = useful_bytes / bytes_per_line;
	
	u_int8_t* dark_avg_data = s->dark_avg_data;
	u_int8_t* white_avg_data = s->white_avg_data;
	
	u_int8_t* begin_ptr = out_data;
	u_int8_t* end_ptr = begin_ptr + bytes_per_line;
	u_int8_t* line_ptr;
	
	while (begin_ptr < end_ptr) {
	  u_int16_t dark_avg = 0;
	  u_int16_t white_avg = WHITE_MAP_RANGE;
	  
	  if (dark_avg_data)
	    dark_avg = get_double_le (dark_avg_data);
	  if (white_avg_data)
	    white_avg = get_double_le (white_avg_data);
	  
	  line_ptr = begin_ptr;
	  for (l = 0; l < lines; ++ l)
	    {
	      double v = (double) get_double_le (line_ptr);
	      u_int16_t v2;
	      if (0)
		v = (v - dark_avg) * white_avg / WHITE_MAP_RANGE;
	      
	      v2 = v < 0xFFFF ? v : 0xFFFF;
	      
	      /* SANE Standard 3.2.1 "... bytes of each sample value are
		 transmitted in the machine's native byte order."*/
	      *line_ptr = v2;
	      
	      line_ptr += bytes_per_line;
	    }
	  
	  begin_ptr += 2;
	  if (dark_avg_data)
	    dark_avg_data += 2;
	  if (white_avg_data)
	    white_avg_data += 2;
	}
      }
      
      /* I know that the next lines are broken if not a multiple of
	 bytes_per_line are in the buffer. Shouldn't happen ... */
      fwrite (out_data, bytes_per_line, useful_bytes / bytes_per_line, fp);
      
      /* save image date in stripe buffer for next next stripe */
      stripe_fill -= useful_bytes;
      if (stripe_fill > 0)
	memcpy (stripe_data, stripe_data + useful_bytes, stripe_fill);
      
      s->line += useful_bytes / bytes_per_line;
      
    iteration_end:
      DBG (3, "reader_process: end of iteration\n");
      ++stripe;
    } /* end while not all lines or inf. mode */
  
  DBG (3, "reader_process: regular i/o loop finished\n");
  exit_status = SANE_STATUS_EOF;
  
 cleanup:
  
  DBG (3, "reader_process: cleanup\n");
  
  /* maybe we need to fill in some white data */
  if (exit_status == SANE_STATUS_EOF && s->line < s->params.lines) {
    DBG (3, "reader_process: padding with white data\n");
    memset (out_data, gray_mode ? 0xff : 0x00, bytes_per_line);
    
    DBG (6, "reader_process: padding line %d - %d\n",
	 s->line, s->params.lines);
    while (s->line < s->params.lines) {
      fwrite (out_data, bytes_per_line, 1, fp);
      s->line++;
    }
  }
  
  /* Eject film holder and/or release_unit - but only for
     non-duplex-rear / non-virtual scans. */
  if (deinterlace && s->duplex_rear_valid)
    {
      DBG (1, "reader_process: virtual duplex scan - no device cleanup!\n");
    }
  else
    {
      if (dev->inquiry_new_protocol && dev->scanner_type == AV_FILM) {
	status = object_position (s, AVISION_SCSI_OP_GO_HOME);
	if (status != SANE_STATUS_GOOD)
	  DBG (1, "reader_process: object position go-home failed!\n");
      }
      
      status = release_unit (s);
      if (status != SANE_STATUS_GOOD)
	DBG (1, "reader_process: release_unit failed\n");
    }
  
  fclose (fp);
  if (rear_fp)
    fclose (rear_fp);
  
  free (stripe_data);
  free (out_data);
  
  DBG (3, "reader_process: returning success\n");
  return exit_status;
}

/* SANE callback to attach a SCSI device */
static SANE_Status
attach_one_scsi (const char* dev)
{
  attach (dev, AV_SCSI, 0);
  return SANE_STATUS_GOOD;
}

/* SANE callback to attach an USB device */
static SANE_Status
attach_one_usb (const char* dev)
{
  attach (dev, AV_USB, 0);
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_init (SANE_Int* version_code, SANE_Auth_Callback authorize)
{
  FILE* fp;
  
  char line[PATH_MAX];
  const char* cp = 0;
  char* word;
  int linenumber = 0;
  int model_num = 0;  

  authorize = authorize; /* silence gcc */
  
  DBG_INIT();

#ifdef AVISION_STATIC_DEBUG_LEVEL
  DBG_LEVEL = AVISION_STATIC_DEBUG_LEVEL;
#endif
  
  DBG (3, "sane_init:(Version: %i.%i Build: %i)\n",
       V_MAJOR, V_MINOR, BACKEND_BUILD);
  
  /* must come first */
  sanei_usb_init ();
  sanei_thread_init ();

  if (version_code)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, BACKEND_BUILD);
  
  fp = sanei_config_open (AVISION_CONFIG_FILE);
  if (fp <= (FILE*)0)
    {
      DBG(1, "sane_init: No config file present!\n");
    }
  else
    {
      /* first parse the config file */
      while (sanei_config_read  (line, sizeof (line), fp))
	{
	  attaching_hw = 0;
	  
	  word = NULL;
	  ++ linenumber;
      
	  DBG(5, "sane_init: parsing config line \"%s\"\n",
	      line);
      
	  cp = sanei_config_get_string (line, &word);
	  
	  if (!word || cp == line) {
	    DBG(5, "sane_init: config file line %d: ignoring empty line\n",
		linenumber);
	    if (word) {
	      free (word);
	      word = NULL;
	    }
	    continue;
	  }
	  
	  if (!word) {
	    DBG(1, "sane_init: config file line %d: could not be parsed\n",
		linenumber);
	    continue;
	  }
	  
	  if (word[0] == '#') {
	    DBG(5, "sane_init: config file line %d: ignoring comment line\n",
		linenumber);
	    free (word);
	    word = NULL;
	    continue;
	  }
                    
	  if (strcmp (word, "option") == 0)
	    {
	      free (word);
	      word = NULL;
	      cp = sanei_config_get_string (cp, &word);
	  
	      if (strcmp (word, "disable-gamma-table") == 0) {
		DBG(3, "sane_init: config file line %d: disable-gamma-table\n",
		    linenumber);
		disable_gamma_table = SANE_TRUE;
	      }
	      else if (strcmp (word, "disable-calibration") == 0) {
		DBG(3, "sane_init: config file line %d: disable-calibration\n",
		    linenumber);
		disable_calibration = SANE_TRUE;
	      }
	      else if (strcmp (word, "one-calib-only") == 0)	{
		DBG(3, "sane_init: config file line %d: one-calib-only\n",
		    linenumber);
		one_calib_only = SANE_TRUE;
	      }
	      else if (strcmp (word, "force-a4") == 0) {
		DBG(3, "sane_init: config file line %d: enabling force-a4\n",
		    linenumber);
		force_a4 = SANE_TRUE;
	      }
	      else if (strcmp (word, "force-a3") == 0) {
		DBG(3, "sane_init: config file line %d: enabling force-a3\n",
		    linenumber);
		force_a3 = SANE_TRUE;
	      }
	      else if (strcmp (word, "static-red-calib") == 0) {
		DBG(3, "sane_init: config file line %d: static red calibration\n",
		    linenumber);
		static_calib_list [0] = SANE_TRUE;
	      }
	      else if (strcmp (word, "static-green-calib") == 0) {
		DBG(3, "sane_init: config file line %d: static green calibration\n",
		    linenumber);
		static_calib_list [1] = SANE_TRUE;
	      }
	      else if (strcmp (word, "static-blue-calib") == 0) {
		DBG(3, "sane_init: config file line %d: static blue calibration\n",
		    linenumber);
		static_calib_list [2] = SANE_TRUE; 
	      }
	      else
		DBG(1, "sane_init: config file line %d: options unknown!\n",
		    linenumber);
	    }
	  else if (strcmp (word, "usb") == 0) {
	    DBG(2, "sane_init: config file line %d: trying to attach USB:`%s'\n",
		linenumber, line);
	    /* try to attach USB device */
	    sanei_usb_attach_matching_devices (line, attach_one_usb);
	  }
	  else if (strcmp (word, "scsi") == 0) {
	    DBG(2, "sane_init: config file line %d: trying to attach SCSI: %s'\n",
		linenumber, line);
	    
	    /* the last time I verified (2003-03-18) this function
	       only matches SCSI devices ... */
	    sanei_config_attach_matching_devices (line, attach_one_scsi);
	  }
	  else {
	    DBG(1, "sane_init: config file line %d: OBSOLETE !! use the scsi keyword!\n",
		linenumber);
	    DBG(1, "sane_init:   (see man sane-avision for details): trying to attach SCSI: %s'\n",
		line);
	  
	    /* the last time I verified (2003-03-18) this function
	       only matched SCSI devices ... */
	    sanei_config_attach_matching_devices (line, attach_one_scsi);
	  }
	  free (word);
	  word = NULL;
	} /* end while read */
      
      fclose (fp);
      
      if (word)
	free (word);
    } /* end if fp */
  
  /* search for all supported USB devices */
  while (Avision_Device_List [model_num].scsi_mfg != NULL)
    {
      attaching_hw = 0;
      /* also potentionally accessed in the attach_* callbacks */
      attaching_hw = &(Avision_Device_List [model_num]);
      if (attaching_hw->usb_vendor != 0 && attaching_hw->usb_product != 0 )
	{
	  DBG (1, "sane_init: Trying to find USB device %x %x ...\n",
	       attaching_hw->usb_vendor,
	       attaching_hw->usb_product);
	  
	  /* TODO: check return value */
	  if (sanei_usb_find_devices (attaching_hw->usb_vendor,
				      attaching_hw->usb_product,
				      attach_one_usb) != SANE_STATUS_GOOD) {
	    DBG(1, "sane_init: error during USB device detection!\n");
	  }
	}
      ++ model_num;
    } /* end for all devices in supported list */
  
  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  Avision_Device* dev;
  Avision_Device* next;

  DBG (3, "sane_exit:\n");

  for (dev = first_dev; dev; dev = next) {
    next = dev->next;
    free ((void*) dev->sane.name);
    free (dev);
  }
  first_dev = NULL;

  free(devlist);
  devlist = NULL;
}

SANE_Status
sane_get_devices (const SANE_Device*** device_list, SANE_Bool local_only)
{
  Avision_Device* dev;
  int i;

  local_only = local_only; /* silence gcc */

  DBG (3, "sane_get_devices:\n");

  if (devlist)
    free (devlist);

  devlist = malloc ((num_devices + 1) * sizeof (devlist[0]));
  if (!devlist)
    return SANE_STATUS_NO_MEM;

  i = 0;
  for (dev = first_dev; i < num_devices; dev = dev->next)
    devlist[i++] = &dev->sane;
  devlist[i++] = 0;

  *device_list = devlist;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle *handle)
{
  Avision_Device* dev;
  SANE_Status status;
  Avision_Scanner* s;
  int i, j;

  DBG (3, "sane_open:\n");
  
  if (devicename[0]) {
    for (dev = first_dev; dev; dev = dev->next)
      if (strcmp (dev->sane.name, devicename) == 0)
	break;

    if (dev) {
      status = attach (devicename, dev->connection.connection_type, &dev);
      if (status != SANE_STATUS_GOOD)
	return status;
    }
  } else {
    /* empty devicname -> use first device */
    dev = first_dev;
  }

  if (!dev)
    return SANE_STATUS_INVAL;
  
  s = malloc (sizeof (*s));
  if (!s)
    return SANE_STATUS_NO_MEM;
  
  /* initialize ... */
  memset (s, 0, sizeof (*s));
  
  /* initialize connection state */
  s->av_con.connection_type = dev->connection.connection_type;
  s->av_con.usb_status = dev->connection.usb_status;
  s->av_con.scsi_fd = -1;
  s->av_con.usb_dn = -1;
  
  s->pipe = -1;
  s->hw = dev;
  
  /* We initilize the table to a gamma value of 2.22, since this is what
     papers about Colorimetry suggest.

	http://www.poynton.com/GammaFAQ.html

     Avision's driver does default to 2.2 though. */
  {
    const double gamma = 2.22;
    const double one_over_gamma = 1. / gamma;
    
    for (i = 0; i < 4; ++ i)
      for (j = 0; j < 256; ++ j)
	s->gamma_table[i][j] = pow( (double) j / 255, one_over_gamma) * 255;
  }
  
  /* the other states (scanning, ...) rely on the memset (0) above */
  
  /* insert newly opened handle into list of open handles: */
  s->next = first_handle;
  first_handle = s;
  *handle = s;
  
  /* open the device */
  if (! avision_is_open (&s->av_con) ) {
#ifdef HAVE_SANEI_SCSI_OPEN_EXTENDED
    DBG (1, "sane_open: using open_extended\n");
    status = avision_open_extended (s->hw->sane.name, &s->av_con, sense_handler, 0,
				    &(dev->scsi_buffer_size));
#else
    status = avision_open (s->hw->sane.name, &s->av_con, sense_handler, 0);
#endif
    if (status != SANE_STATUS_GOOD) {
      DBG (1, "sane_open: open of %s failed: %s\n",
	   s->hw->sane.name, sane_strstatus (status));
      return status;
    }
    DBG (1, "sane_open: got %d scsi_max_request_size\n", dev->scsi_buffer_size);
  }
  
  /* maybe probe for additional information */
  if (dev->additional_probe)
    {
      status = wait_ready (&s->av_con);
      if (status != SANE_STATUS_GOOD) {
	DBG (1, "sane_open: wait_ready() failed: %s\n", sane_strstatus (status));
	return status;
      }
      status = additional_probe (s);
      if (status != SANE_STATUS_GOOD) {
	DBG (1, "sane_open: additional probe failed: %s\n", sane_strstatus (status));
	return status;
      }
    }
  
  /* initilize the options */
  init_options (s);
  
  if (dev->inquiry_duplex) {
    /* Might need at least *DOS (Windows flavour and OS/2) portability fix
       However I was was told Cygwin does keep care of it. */
    strncpy(s->duplex_rear_fname, "/tmp/avision-rear-XXXXXX", PATH_MAX);
    
    if (! mktemp(s->duplex_rear_fname) ) {
      DBG (1, "sane_open: failed to generate temporary fname for duplex scans\n");
      return SANE_STATUS_NO_MEM;
    }
    else {
      DBG (1, "sane_open: temporary fname for duplex scans: %s\n",
	   s->duplex_rear_fname);
    }
  }
  
  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  Avision_Scanner* prev;
  Avision_Scanner* s = handle;
  int i;

  DBG (3, "sane_close:\n\n");
  
  /* close the device */
  if (avision_is_open (&s->av_con) ) {
    avision_close (&s->av_con);
  }
  
  /* remove handle from list of open handles: */
  prev = 0;
  for (s = first_handle; s; s = s->next) {
    if (s == handle)
      break;
    prev = s;
  }

  /* a handle we know about ? */
  if (!s) {
    DBG (1, "sane_close: invalid handle %p\n", handle);
    return;
  }

  if (s->scanning)
    do_cancel (handle);

  if (prev)
    prev->next = s->next;
  else
    first_handle = s->next;

  for (i = 1; i < NUM_OPTIONS; ++ i) {
    if (s->opt[i].type == SANE_TYPE_STRING && s->val[i].s) {
      free (s->val[i].s);
    }
  }
  
  if (s->white_avg_data)
    free (s->white_avg_data);
  if (s->dark_avg_data)
    free (s->dark_avg_data);

  if (*(s->duplex_rear_fname)) {
    unlink (s->duplex_rear_fname);
    *(s->duplex_rear_fname) = 0;
  }
  
  free (handle);
}

const SANE_Option_Descriptor*
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  Avision_Scanner* s = handle;
  
  DBG (3, "sane_get_option_descriptor:\n");

  if ((unsigned) option >= NUM_OPTIONS)
    return 0;
  return s->opt + option;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void* val, SANE_Int* info)
{
  Avision_Scanner* s = handle;
  Avision_Device* dev = s->hw;
  SANE_Status status;
  SANE_Word cap;
  
  DBG (3, "sane_control_option:\n");

  if (info)
    *info = 0;

  if (s->scanning)
    return SANE_STATUS_DEVICE_BUSY;
  
  if (option >= NUM_OPTIONS)
    return SANE_STATUS_INVAL;

  cap = s->opt[option].cap;

  if (!SANE_OPTION_IS_ACTIVE (cap))
    return SANE_STATUS_INVAL;

  if (action == SANE_ACTION_GET_VALUE)
    {
      switch (option)
	{
	  /* word options: */
	case OPT_PREVIEW:
	  
	case OPT_RESOLUTION:
	case OPT_SPEED:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_NUM_OPTS:
	  
	case OPT_BRIGHTNESS:
	case OPT_CONTRAST:
	case OPT_QSCAN:
	case OPT_QCALIB:
	case OPT_FRAME:
	  
	  *(SANE_Word*) val = s->val[option].w;
	  return SANE_STATUS_GOOD;
	  
	  /* specially treated word options */
	case OPT_BUTTON_0:
          /* I would like to return _INVALID for buttons not present
             however xsane querries them all :-( */
          if (dev->inquiry_button_control)
	    status = get_button_status (s);
	case OPT_BUTTON_1:
	case OPT_BUTTON_2:
	case OPT_BUTTON_3:
	case OPT_BUTTON_4:
	case OPT_BUTTON_5:
	case OPT_BUTTON_6:
	case OPT_BUTTON_7:
	  
	  /* copy the button state */
	  *(SANE_Word*) val = s->val[option].w;
	  /* clear the button state */
	  s->val[option].w = SANE_FALSE;

	  return SANE_STATUS_GOOD;
	  
	  /* word-array options: */
	case OPT_GAMMA_VECTOR:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	  memcpy (val, s->val[option].wa, s->opt[option].size);
	  return SANE_STATUS_GOOD;
	  
	  /* string options: */
	case OPT_MODE:
	case OPT_SOURCE:
	case OPT_MESSAGE:
	  strcpy (val, s->val[option].s);
	  return SANE_STATUS_GOOD;
	} /* end switch option */
    } /* end if GET_ACTION_GET_VALUE */
  else if (action == SANE_ACTION_SET_VALUE)
    {
      if (!SANE_OPTION_IS_SETTABLE (cap))
	return SANE_STATUS_INVAL;
      
      status = constrain_value (s, option, val, info);
      if (status != SANE_STATUS_GOOD)
	return status;
      
      switch (option)
	{
	  /* side-effect-free word options: */
	case OPT_SPEED:
	case OPT_PREVIEW:
	case OPT_BRIGHTNESS:
	case OPT_CONTRAST:
	case OPT_QSCAN:
	case OPT_QCALIB:

	  s->val[option].w = *(SANE_Word*) val;
	  return SANE_STATUS_GOOD;
	  
	  /* side-effect-free word-array options: */
	case OPT_GAMMA_VECTOR:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	  memcpy (s->val[option].wa, val, s->opt[option].size);
	  return SANE_STATUS_GOOD;
		 
	  /* options with side-effects: */
	case OPT_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:

	  s->val[option].w = *(SANE_Word*) val;

	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;

	  return SANE_STATUS_GOOD;

	  /* string options with side-effects: */
	case OPT_SOURCE:
	  
	  if (s->val[option].s) {
	    free(s->val[option].s);
	  }
	  s->val[option].s = strdup(val);
	  s->source_mode = match_source_mode (dev, s->val[option].s);
	  s->source_mode_dim = match_source_mode_dim (s->source_mode);
	  
	  /* set side-effects */
	  dev->x_range.max =
	    SANE_FIX ( dev->inquiry_x_ranges[s->source_mode_dim]);
	  dev->y_range.max =
	    SANE_FIX ( dev->inquiry_y_ranges[s->source_mode_dim]);
	  
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  
	  return SANE_STATUS_GOOD;
	  
	case OPT_MODE:
	  {
	    if (s->val[option].s)
	      free (s->val[option].s);
	    
	    s->val[option].s = strdup (val);
	    s->c_mode = match_color_mode (dev, s->val[OPT_MODE].s);
	    
	    /* set to mode specific values */
	    
	    /* the gamma table related */
	    if (!disable_gamma_table)
	      {
		if (color_mode_is_color (s->c_mode) ) {
		  s->opt[OPT_GAMMA_VECTOR].cap   |= SANE_CAP_INACTIVE;
		  s->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
		}
		else /* gray or mono */
		  {
		    s->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
		    s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
		    s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
		    s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
		  }
	      }		
	    if (info)
	      *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	    return SANE_STATUS_GOOD;
	  }
	case OPT_FRAME:
	  {
	    SANE_Word frame = *((SANE_Word  *) val);
	    
	    status = set_frame (s, frame);
	    if (status == SANE_STATUS_GOOD) {
	      s->val[OPT_FRAME].w = frame;
	      dev->current_frame = frame;
	    }
	    return status;
	  }
	} /* end switch option */
    } /* end else SET_VALUE */
  return SANE_STATUS_INVAL;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters* params)
{
  Avision_Scanner* s = handle;
  
  DBG (3, "sane_get_parameters:\n");
  
  /* during an acutal scan the parameters are used and thus are not alllowed
     to change ... if a scan is running the parameters have already been
     computed from sane_start */
  if (!s->scanning)
    {
      DBG (3, "sane_get_parameters: computing parameters\n");
      compute_parameters (s);
    }

  /* It would be idial to return an estimation when the scan is not yet
     running, and -1 for "unknown" when the scan is running.
     However this confuses the frontends - some just write truncated files
     (scanimage) and other (xscanimage) complain they do nto support a
     "Hand scanner mode" ... -ReneR */
  /* 
     else {
     if (s->source_mode_dim == AV_ADF_DIM ||
     dev->scanner_type == AV_SHEETFEED) {
     DBG (3, "sane_get_parameters: ADF -> setting lines to unknown\n");
     s->params.lines = -1;
     }
     }
  */
  
  if (params) {
    *params = s->params;
  }
  
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_start (SANE_Handle handle)
{
  Avision_Scanner* s = handle;
  Avision_Device* dev = s->hw;
  
  SANE_Status status;
  int fds [2];
  u_int32_t param_cksum;
  DBG (1, "sane_start:\n");
  
  /* Make sure there is no scan running!!! */
  
  if (s->scanning)
    return SANE_STATUS_DEVICE_BUSY;
 
  /* Make sure we have a current parameter set. Some of the
     parameters will be overwritten below, but that's OK. */
  status = sane_get_parameters (s, &s->params);
  if (status != SANE_STATUS_GOOD) {
    return status;
  }
  compute_parameters (s);

  /* Do we use the same parameters? */
  param_cksum = create_cksum (s);

  if (param_cksum == s->param_cksum) {
    DBG (1, "sane_start: same parameters as last scan.\n");
  }
  
  /* Is there some temporarily stored rear page ? */
  if (s->duplex_rear_valid) {
    if (param_cksum != s->param_cksum) {
      DBG (1, "sane_start: virtual duplex rear data outdated due to parameter change!\n");
      s->duplex_rear_valid = SANE_FALSE;
    }
  }
  
  if (s->duplex_rear_valid) {
    DBG (1, "sane_start: virtual duplex rear data valid.\n");
    goto start_scan_end;
  }
  
  /* Check for paper during ADF scans and for sheetfeed scanners. */
  if ( (dev->scanner_type == AV_FLATBED && s->source_mode_dim == AV_ADF_DIM) ||
       dev->scanner_type == AV_SHEETFEED) {
    status = media_check (s);
    if (status != SANE_STATUS_GOOD) {
      DBG (1, "sane_start: media_check failed: %s\n",
	   sane_strstatus (status));
      return status;
    }
    else
      DBG (1, "sane_start: media_check ok\n");
  }
  
  if (param_cksum == s->param_cksum)
    goto start_scan_end;

  status = set_window (s);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "sane_start: set scan window command failed: %s\n",
	 sane_strstatus (status));
    goto stop_scanner_and_return;
  }
  
  /* Check the light early, to return to the GUI and notify the user. */
  if (dev->inquiry_light_control) {
    status = wait_4_light (s);
    if (status != SANE_STATUS_GOOD) {
      return status;
    }
  }
  
#ifdef DEBUG_TEST
  /* debug window size test ... */
  if (dev->inquiry_new_protocol)
    {
      size_t size = 16;
      u_int8_t result[16];
      
      DBG (5, "sane_start: reading scanner window size\n");
      
      status = simple_read (s, 0x80, 0, &size, result);
      
      if (status != SANE_STATUS_GOOD) {
	DBG (1, "sane_start: get pixel size command failed: %s\n",
	     sane_strstatus (status));
	goto stop_scanner_and_return;
      } 
      debug_print_raw (5, "sane_start: pixel_size:", result, size);
      DBG (5, "sane_start: x-pixels: %d, y-pixels: %d\n",
	   get_quad (&(result[0])), get_quad (&(result[4])));
    }
#endif
  
  status = wait_ready (&s->av_con);
  
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "sane_start wait_ready() failed: %s\n", sane_strstatus (status));
    goto stop_scanner_and_return;
  }
  
  /* Only perform the calibration for newer scanners - it is not needed
     for my Avision AV 630 - and also does not even work ... */
  if (!dev->inquiry_new_protocol) {
    DBG (1, "sane_start: old protocol no calibration needed!\n");
    goto calib_end;
  }
  
  if (!dev->inquiry_needs_calibration) {
    DBG (1, "sane_start: due to inquiry no calibration needed!\n");
    goto calib_end;
  }
  
  /* calibration allowed for this scanner? */
  if (dev->hw->feature_type & AV_NO_CALIB) {
    DBG (1, "sane_start: calibration disabled in device list!!\n");
    goto calib_end;
  }
  
  /* check whether claibration is disabled by the user */
  if (disable_calibration) {
    DBG (1, "sane_start: calibration disabled in config - skipped!\n");
    goto calib_end;
  }
  
  /* TODO: do the calibration here if the user asks for it? */ 
  
  /* R² reminder: We must not skip the calibration for ADF scans, some
     scanner (HP 53xx/74xx ASIC series) rely on a calibration data
     read (and will hang otherwise) */
  
  status = normal_calibration (s);
  
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "sane_start: perform calibration failed: %s\n",
	 sane_strstatus (status));
	goto stop_scanner_and_return;
  }
  
 calib_end:
  
  /* check whether gamma-table is disabled by the user? */
  if (disable_gamma_table) {
    DBG (1, "sane_start: gamma-table disabled in config - skipped!\n");
    goto gamma_end;
  }
  
  status = send_gamma (s);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "sane_start: send gamma failed: %s\n",
	 sane_strstatus (status));
    goto stop_scanner_and_return;
  }
  
 gamma_end:
  
  
  /* check film holder */
  if (dev->scanner_type == AV_FILM && dev->holder_type == 0xff) {
    DBG (1, "sane_start: no film holder or APS cassette!\n");
    
    /* Normally "go_home" is executed from the reader process,
       but as it will not start we have to reset things here */
    if (dev->inquiry_new_protocol) {
      status = object_position (s, AVISION_SCSI_OP_GO_HOME);
      if (status != SANE_STATUS_GOOD)
	DBG(1, "sane_start: go home failed: %s\n",
	    sane_strstatus (status));
    }
    goto stop_scanner_and_return;
  }
  
 start_scan_end:
  
  s->scanning = SANE_TRUE;
  
  if (pipe (fds) < 0) {
    return SANE_STATUS_IO_ERROR;
  }
  
  s->pipe = fds[0];
  s->reader_fds = fds[1];
  
#ifdef AVISION_ENHANCED_SANE
  s->reader_pid = 0; /* the thread will be started uppon the first read */
#else
  /* create reader routine as new process or thread */
  DBG (3, "sane_start: starting thread\n");
  s->reader_pid = sanei_thread_begin (reader_process, (void *) s);
  
  if (sanei_thread_is_forked())
    close (s->reader_fds);
#endif
  
  return SANE_STATUS_GOOD;
  
 stop_scanner_and_return:
  
  /* cancel the scan nicely */
  do_cancel (s);
  
  return status;
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte* buf, SANE_Int max_len, SANE_Int* len)
{
  Avision_Scanner* s = handle;
  ssize_t nread;

#ifdef AVISION_ENHANCED_SANE
  /* create reader routine as new process or thread */
  if (s->reader_pid == 0) {
    DBG (3, "sane_read: starting thread\n");
    s->reader_pid = sanei_thread_begin (reader_process, (void *) s);
  
    if (sanei_thread_is_forked())
      close (s->reader_fds);
  }
#endif
  
  DBG (8, "sane_read: max_len: %d\n", max_len);

  *len = 0;

  nread = read (s->pipe, buf, max_len);
  if (nread > 0) {
    DBG (8, "sane_read: got %ld bytes, err: %d %s\n", (long) nread, errno, strerror(errno));
  }
  else {
    DBG (3, "sane_read: got %ld bytes, err: %d %s\n", (long) nread, errno, strerror(errno));
  }

  if (!s->scanning)
    return SANE_STATUS_CANCELLED;
  
  if (nread < 0) {
    if (errno == EAGAIN) {
      return SANE_STATUS_GOOD;
    } else {
      do_cancel (s);
      return SANE_STATUS_IO_ERROR;
    }
  }
  
  *len = nread;
  
  /* if all data is passed through */
  if (nread == 0) {
    s->scanning = SANE_FALSE;
    return do_eof (s);
  }
  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  Avision_Scanner* s = handle;
  
  DBG (3, "sane_cancel:\n");
  
  if (s->scanning)
    do_cancel (s);
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  Avision_Scanner* s = handle;
  
  DBG (3, "sane_set_io_mode:\n");
  if (!s->scanning) {
    DBG (3, "sane_set_io_mode: not yet scanning\n");
    return SANE_STATUS_INVAL;
  }
  
  if (fcntl (s->pipe, F_SETFL, non_blocking ? O_NONBLOCK : 0) < 0)
    return SANE_STATUS_IO_ERROR;
  
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int* fd)
{
  Avision_Scanner* s = handle;
  
  DBG (3, "sane_get_select_fd:\n");
  
  if (!s->scanning){
    DBG (3, "sane_get_select_fd: not yet scanning\n");
    return SANE_STATUS_INVAL;
  }
  
  *fd = s->pipe;
  return SANE_STATUS_GOOD;
}

#ifdef AVISION_ENHANCED_SANE

SANE_Status
sane_avision_media_check (SANE_Handle handle)
{
  Avision_Scanner* s = handle;
  
  DBG (3, "sane_media_check:\n");
  
  return media_check (s);
}

#endif
