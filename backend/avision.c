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
   74xx, Minolta FS-V1 ...) or Fujitsu ScanPartner with the AVISION SCSI-2
   command set.
   
   Copyright 2003 by
                "René Rebe" <rene.rebe@gmx.net>
   
   Copyright 2002 by
                "René Rebe" <rene.rebe@gmx.net>
                "Jose Paulo Moitinho de Almeida" <moitinho@civil.ist.utl.pt>
   
   Copyright 1999, 2000, 2001 by
                "René Rebe" <rene.rebe@gmx.net>
                "Meino Christian Cramer" <mccramer@s.netic.de>
   
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
   
   Very much special thanks to:
                Avision INC for the documentation we got! ;-)
                Roberto Dicosmo who sponsored a HP 7530 scanner !!! ;-)
                Oliver Neukum who sponsored a HP 5300 USB scanner
                              and wrote the hpusbscsi kernel module!!! ;-)
                Matthias Wiedemann for lending his HP 7450C for some weeks ;-)
                Avision INC for sponsoring an AV 8000S with ADF !!! ;-)
                All the many other beta-tester and debug-log sender ;-)
   
   ChangeLog:
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
         * wild guess for the HP 5730 problem
   
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
         * fixed gamma table for AV_ASIC_Cx devices (e.g. AV630C)
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
         * cleanup of many details like: reget frame_info in set_frame, resolution
           computation for different scanners, using the scan command for new
           scanners again, changed types to make the gcc happy ...
   
   2002-02-14: Jose Paulo Moitinho de Almeida
         * film holder control update
   
   2002-02-12: René Rebe
         * further calibration testing / implementing
         * merged the film holder control
         * attach and other function cleanup
         * added a timeout to wait_4_light
         * only use the scan command for old protocol scanners (it hangs the HP 7400)
   
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
   
   2001-12-11: Jose P.M. de Almeida
         * fixed some typos
         * updated perform_calibration
         * added go_home
   
   2001-12-10: René Rebe
         * fixed some typos
         * added some TODO notes where we need to call some new_protocol funtions
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
#include <sys/wait.h>
#include <unistd.h>
#include <sys/time.h>

#include <math.h>

#include <sane/sane.h>
#include <sane/sanei.h>
#include <sane/saneopts.h>
#include <sane/sanei_scsi.h>
#include <sane/sanei_config.h>
#include <sane/sanei_backend.h>

#include <avision.h>

/* For timeval... */
#ifdef DEBUG
#include <sys/time.h>
#endif

#define BACKEND_NAME avision
#define BACKEND_BUILD 63  /* avision backend BUILD version */

/* Attention: The comments must be stay as they are - they are
   automatially parsed to generate the SANE avision.desc file, as well
   as HTML online content! */

static Avision_HWEntry Avision_Device_List [] =
  {
    /* All Avision except 630*, 620*, 6240 and 8000 untested ... */
    
    { "AVISION", "AV100CS",
      "Avision", "AV100CS",
      AV_SCSI, AV_SHEETFEED, 0},
    /* interface="SCSI" */
    /* status="untested" */
    
    { "AVISION", "AV100IIICS",
      "Avision", "AV100IIICS",
      AV_SCSI, AV_FLATBED, 0},
    /* interface="SCSI" */
    /* status="untested" */
    
    { "AVISION", "AV100S",
      "Avision", "AV100S",
      AV_SCSI, AV_FLATBED, 0},
    /* interface="SCSI" */
    /* status="untested" */
    
    { "AVISION", "AV240SC",
      "Avision", "AV240SC",
      AV_SCSI, AV_FLATBED, 0},
    /* interface="SCSI" */
    /* status="untested" */
    
    { "AVISION", "AV260CS",
      "Avision", "AV260CS",
      AV_SCSI, AV_FLATBED, 0},
    /* interface="SCSI" */
    /* status="untested" */
    
    { "AVISION", "AV360CS",
      "Avision", "AV360CS",
      AV_SCSI, AV_FLATBED, 0},
    /* interface="SCSI" */
    /* status="untested" */
    
    { "AVISION", "AV363CS",
      "Avision", "AV363CS",
      AV_SCSI, AV_FLATBED, 0},
    /* interface="SCSI" */
    /* status="untested" */
    
    { "AVISION", "AV420CS",
      "Avision", "AV420CS",
      AV_SCSI, AV_FLATBED, 0},
    /* interface="SCSI" */
    /* status="untested" */
    
    { "AVISION", "AV6120",
      "Avision", "AV6120",
      AV_SCSI, AV_FLATBED, 0},
    /* interface="SCSI" */
    /* status="untested" */
    
    { "AVISION", "AV620CS",
      "Avision", "AV620CS",
      AV_SCSI, AV_FLATBED, 0},
    /* interface="SCSI" */
    /* comment="1 pass, 600 dpi" */
    /* status="stable" */
    
    { "AVISION", "AV620CS Plus",
      "Avision", "AV620CS Plus",
      AV_SCSI, AV_FLATBED, 0},
    /* interface="SCSI" */
    /* comment="1 pass, 1200 dpi" */
    /* status="stable" */
    
    { "AVISION", "AV630CS",
      "Avision", "AV630CS",
      AV_SCSI, AV_FLATBED , 0},
    /* interface="SCSI" */
    /* comment="1 pass, 1200 dpi - regularly tested" */
    /* status="stable" */
    
    { "AVISION", "AV630CSL",
      "Avision", "AV630CSL",
      AV_SCSI, AV_FLATBED , 0},
    /* interface="SCSI" */
    /* comment="1 pass, 1200 dpi" */
    /* status="untested" */
    
    { "AVISION", "AV6240",
      "Avision", "AV6240",
      AV_SCSI, AV_FLATBED, 0},
    /* interface="SCSI" */
    /* comment="1 pass, ??? dpi" */
    /* status="stable" */
    
    {"AVISION", "AV600U",
     "Avision", "AV600U",
     AV_USB, AV_FLATBED, 0}, /* c5 calib */
    /* interface="USB" */
    /* comment="1 pass, 1200 dpi" */
    /* status="untested" */
    
    { "AVISION", "AV660S",
      "Avision", "AV660S",
      AV_USB, AV_FLATBED, 0},
    /* interface="USB" */
    /* comment="1 pass, 1200 dpi" */
    /* status="untested" */

    { "AVISION", "AV680S",
      "Avision", "AV680S",
      AV_USB, AV_FLATBED, 0},
    /* interface="USB" */
    /* comment="1 pass, 1200 dpi" */
    /* status="untested" */
    
    {"AVISION", "AV690U",
     "Avision", "AV690U",
     AV_USB, AV_FLATBED, 0}, /* c5 calib */
    /* interface="USB" */
    /* comment="1 pass, 2400 dpi" */
    /* status="untested" */
    
    { "AVISION", "AV800S",
      "Avision", "AV800S",
      AV_SCSI, AV_FLATBED, 0},
    /* interface="SCSI" */
    /* comment="1 pass, 1200 dpi" */
    /* status="untested" */
    
    { "AVISION", "AV810C",
      "Avision", "AV810C",
      AV_SCSI, AV_FLATBED, 0},
    /* interface="SCSI" */
    /* comment="1 pass, 1200 dpi" */
    /* status="untested" */
    
    { "AVISION", "AV820",
      "Avision", "AV820",
      AV_SCSI, AV_FLATBED, 0},
    /* interface="SCSI" */
    /* comment="1 pass, 1200 dpi" */
    /* status="untested" */
    
    { "AVISION", "AV820C",
      "Avision", "AV820C",
      AV_SCSI, AV_FLATBED, 0},
    /* interface="SCSI" */
    /* comment="1 pass, 1200 dpi" */
    /* status="untested" */
    
    { "AVISION", "AV820C Plus",
      "Avision", "AV820C Plus",
      AV_SCSI, AV_FLATBED, 0},
    /* interface="SCSI" */
    /* comment="1 pass, 1200 dpi" */
    /* status="untested" */
    
    {"AVISION", "AV830C",
     "Avision", "AV830C",
     AV_SCSI, AV_FLATBED, 0},
    /* interface="SCSI" */
    /* comment="1 pass, 1200 dpi" */
    /* status="untested" */
    
    {"AVISION", "AV830C Plus",
     "Avision", "AV830C Plus",
     AV_SCSI, AV_FLATBED, 0},
    /* interface="SCSI" */
    /* comment="1 pass, 1200 dpi" */
    /* status="untested" */
    
    { "AVISION", "AV880",
      "Avision", "AV880",
      AV_SCSI, AV_FLATBED, 0},
    /* interface="SCSI" */
    /* comment="1 pass, 1200 dpi" */
    /* status="untested" */
    
    { "AVISION", "AV880C",
      "Avision", "AV880C",
      AV_SCSI, AV_FLATBED, 0},
    /* interface="SCSI" */
    /* comment="1 pass, 1200 dpi" */
    /* status="untested" */
    
    { "AVISION", "AV8000S",
      "Avision", "AV8000S",
      AV_SCSI, AV_FLATBED, 0},
    /* interface="SCSI" */
    /* comment="1 pass, 1200 dpi, A3 - regularly tested" */
    /* status="stable" */
    
    { "AVISION", "AVA3",
      "Avision", "AVA3",
      AV_SCSI, AV_FLATBED, 0},
    /* interface="SCSI" */
    /* comment="1 pass, 600 dpi, A3" */
    /* status="untested" */
    
    /* and possibly more avisions */
    
    { "HP",      "ScanJet 5300C",
      "Hewlett-Packard", "ScanJet 5300C",
      AV_USB, AV_FLATBED, 0},
    /* interface="USB" */
    /* comment="1 pass, 1200 dpi - regularly tested" */
    /* status="stable" */
    
    { "HP",      "ScanJet 5370C",
      "Hewlett-Packard", "ScanJet 5370C",
      AV_USB, AV_FLATBED, 0},
    /* interface="USB" */
    /* comment="1 pass, 2400 dpi" */
    /* status="alpha" */
    
    { "hp",      "scanjet 7400c",
      "Hewlett-Packard", "ScanJet 7400c",
      AV_USB, AV_FLATBED, AV_LIGHT_CHECK_BOGUS},
    /* interface="USB" */
    /* comment="1 pass, 1200 dpi - the HP ScanJet 7450c/7490c are HP ScanJet 7400s plus gadgets" */
    /* status="beta" */
    
    { "MINOLTA", "FS-V1",
      "Minolta", "FS-V1",
      AV_USB, AV_FILM, AV_ONE_CMD_CALIB},
    /* interface="USB" */
    /* comment="1 pass, film-scanner" */
    /* status="alpha" */
    
    /* Minolta Scan Elite II - might be basically a Dual II + IR scan */
    
    /* possibly all Minolta film-scanners ? */
    
    { "MITSBISH", "MCA-ADFC",
      "Mitsubishi", "MCA-ADFC",
      AV_SCSI, AV_SHEETFEED, 0},
    /* interface="SCSI" */
    /* status="untested" */
    
    { "MITSBISH", "MCA-S1200C",
      "Mitsubishi", "S1200C",
      AV_SCSI, AV_SHEETFEED, 0},
    /* interface="SCSI" */
    /* status="untested" */
    
    { "MITSBISH", "MCA-S600C",
      "Mitsubishi", "S600C",
      AV_SCSI, AV_SHEETFEED, 0},
    /* interface="SCSI" */
    /* status="untested" */
    
    { "MITSBISH", "SS600",
      "Mitsubishi", "SS600",
      AV_SCSI, AV_SHEETFEED, 0},
    /* interface="SCSI" */
    /* status="beta" */
    
    /* The next are all untested ... */
    
    { "FCPA", "ScanPartner",
      "Fujitsu", "ScanPartner",
      AV_SCSI, AV_SHEETFEED, 0},
    /* interface="SCSI" */
    /* status="untested" */

    { "FCPA", "ScanPartner 10",
      "Fujitsu", "ScanPartner 10",
      AV_SCSI, AV_SHEETFEED, 0},
    /* interface="SCSI" */
    /* status="untested" */
    
    { "FCPA", "ScanPartner 10C",
      "Fujitsu", "ScanPartner 10C",
      AV_SCSI, AV_SHEETFEED, 0},
    /* interface="SCSI" */
    /* status="untested" */
    
    { "FCPA", "ScanPartner 15C",
      "Fujitsu", "ScanPartner 15C",
      AV_SCSI, AV_SHEETFEED, AV_FUJITSU},
    /* interface="SCSI" */
    /* status="untested" */
    
    { "FCPA", "ScanPartner 300C",
      "Fujitsu", "ScanPartner 300C",
      AV_SCSI, AV_SHEETFEED, 0},
    /* interface="SCSI" */
    /* status="untested" */
    
    { "FCPA", "ScanPartner 600C",
      "Fujitsu", "ScanPartner 600C",
      AV_SCSI, AV_SHEETFEED, 0},
    /* interface="SCSI" */
    /* status="untested" */
    
    { "FCPA", "ScanPartner Jr",
      "Fujitsu", "ScanPartner Jr",
      AV_SCSI, AV_SHEETFEED, 0},
    /* interface="SCSI" */
    /* status="untested" */
    
    { "FCPA", "ScanStation",
      "Fujitsu", "ScanStation",
      AV_SCSI, AV_SHEETFEED, 0},
    /* interface="SCSI" */
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
      NULL, NULL,
      0, 0, 0} 
  };

/* used when scanner returns invalid range fields ... */
#define A4_X_RANGE 8.5
#define A4_Y_RANGE 11.8
#define SHEETFEED_Y_RANGE 14.0

#ifndef PATH_MAX
# define PATH_MAX 1024
#endif

#define AVISION_CONFIG_FILE "avision.conf"

#define MM_PER_INCH 25.4
#define AVISION_BASE_RES 300

/* calibration (shading) defines */

#define INVALID_WHITE_SHADING	0x0000
#define DEFAULT_WHITE_SHADING	0xFFF0
#define WHITE_MAP_RANGE		0x4000
#define MAX_WHITE_SHADING       0xFFFF

#define INVALID_DARK_SHADING	0xFFFF
#define DEFAULT_DARK_SHADING	0x0000

static int num_devices;
static Avision_Device* first_dev;
static Avision_Scanner* first_handle;
static const SANE_Device** devlist = 0;

/* disable the usage of a custom gamma-table */
static SANE_Bool disable_gamma_table = SANE_FALSE;

/* disable the calibration */
static SANE_Bool disable_calibration = SANE_FALSE;

/* use old "r²" calibration method (for tests) */
static SANE_Bool old_calibration = SANE_FALSE;

/* do only one claibration per backend initialization */
static SANE_Bool one_calib_only = SANE_FALSE;

/* force scanable areas to ISO(DIN) A4 */
static SANE_Bool force_a4 = SANE_FALSE;

/* disable normally needed c5 guard */
static SANE_Bool disable_c5_guard = SANE_FALSE;

static const SANE_String_Const mode_list[] =
  {
    "Line Art", "Dithered", "Gray", "Color", 0
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
  
  
#define INQ_LEN	0x60
  
static const u_int8_t inquiry[] =
  {
    AVISION_SCSI_INQUIRY, 0x00, 0x00, 0x00, INQ_LEN, 0x00
  };

static const u_int8_t test_unit_ready[] =
  {
    AVISION_SCSI_TEST_UNIT_READY, 0x00, 0x00, 0x00, 0x00, 0x00
  };

static const u_int8_t scan[] =
  {
    AVISION_SCSI_SCAN, 0x00, 0x00, 0x00, 0x00, 0x00
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

static void debug_print_raw (int dbg_level, char* info, u_int8_t* data,
			     size_t count)
{
  size_t i;
  
  DBG (dbg_level, info);
  for (i = 0; i < count; ++ i) {
    DBG (dbg_level, "  [%d] %1d%1d%1d%1d%1d%1d%1d%1db %3oo %3dd %2xx\n",
	 i,
	 BIT(data[i],7), BIT(data[i],6), BIT(data[i],5), BIT(data[i],4),
	 BIT(data[i],3), BIT(data[i],2), BIT(data[i],1), BIT(data[i],0),
	 data[i], data[i], data[i]);
  }
}

static void debug_print_avdimen (int dbg_level, char* func, Avision_Dimensions* avdimen)
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
  
  DBG (dbg_level, "%s: [0-1]  pixels per line %d\n",
       func, get_double ( &(result[0]) ));
  DBG (dbg_level, "%s: [2]    bytes per channel %d\n", func, result[2]);
  DBG (dbg_level, "%s: [3]    line count %d\n", func, result[3]);
  
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
  
  DBG (dbg_level, "%s: [9-10] R shading target: %d\n",
       func, get_double ( &(result[9]) ) );
  DBG (dbg_level, "%s: [11-12] G shading target: %d\n",
       func, get_double ( &(result[11]) ) );
  DBG (dbg_level, "%s: [13-14] B shading target: %d\n",
       func, get_double ( &(result[13]) ) );
  
  DBG (dbg_level, "%s: [15-16] R dark shading target: %d\n",
       func, get_double ( &(result[15]) ) );
  DBG (dbg_level, "%s: [17-18] G dark shading target: %d\n",
       func, get_double ( &(result[17]) ) );
  DBG (dbg_level, "%s: [19-20] B dark shading target: %d\n",
       func, get_double ( &(result[19]) ) );
}

static void debug_print_window_descriptor (int dbg_level, char* func,
					   command_set_window_window* window)
{
  DBG (dbg_level, "%s: [0]     window_id %d\n", func,
       window->descriptor.winid);
  DBG (dbg_level, "%s: [2-3]   x-axis res %d\n", func,
       get_double (window->descriptor.xres));
  DBG (dbg_level, "%s: [4-5]   y-axis res %d\n", func,
       get_double (window->descriptor.yres));
  DBG (dbg_level, "%s: [6-9]   x-axis upper left %d\n",
       func, get_quad (window->descriptor.ulx));
  DBG (dbg_level, "%s: [10-13] y-axis upper left %d\n",
       func, get_quad (window->descriptor.uly));
  DBG (dbg_level, "%s: [14-17] window width %d\n", func,
       get_quad (window->descriptor.width));
  DBG (dbg_level, "%s: [18-21] window length %d\n", func, 
       get_quad (window->descriptor.length));
  DBG (dbg_level, "%s: [22]    brightness %d\n", func,
       window->descriptor.brightness);
  DBG (dbg_level, "%s: [23]    threshold %d\n", func,
       window->descriptor.threshold);
  DBG (dbg_level, "%s: [24]    contrast %d\n", func,
       window->descriptor.contrast);
  DBG (dbg_level, "%s: [25]    image composition %x\n", func,
       window->descriptor.image_comp);
  DBG (dbg_level, "%s: [26]    bits per channel %d\n", func,
       window->descriptor.bpc);
  DBG (dbg_level, "%s: [27-28] halftone pattern %x\n", func,
       get_double (window->descriptor.halftone));
  DBG (dbg_level, "%s: [29]    padding_and_bitset %x\n", func,
       window->descriptor.padding_and_bitset);
  DBG (dbg_level, "%s: [30-31] bit ordering %x\n", func,
       get_double (window->descriptor.bitordering));
  DBG (dbg_level, "%s: [32]    compression type %x\n", func,
       window->descriptor.compr_type);
  DBG (dbg_level, "%s: [33]    compression argument %x\n", func,
       window->descriptor.compr_arg);
  DBG (dbg_level, "%s: [40]    vendor id %x\n", func,
       window->descriptor.vendor_specific);
  DBG (dbg_level, "%s: [41]    param lenght %d\n", func,
       window->descriptor.paralen);
  DBG (dbg_level, "%s: [42]    bitset %x\n", func,
       window->avision.bitset1);
  DBG (dbg_level, "%s: [43]    highlight %d\n", func,
       window->avision.highlight);
  DBG (dbg_level, "%s: [44]    shadow %d\n", func,
       window->avision.shadow);
  DBG (dbg_level, "%s: [45-46] line-width %d\n", func,
       get_double (window->avision.line_width));
  DBG (dbg_level, "%s: [47-48] line-count %d\n", func,
       get_double (window->avision.line_count));
  DBG (dbg_level, "%s: [49]    bitset %x\n", func,
       window->avision.type.normal.bitset2);
  DBG (dbg_level, "%s: [50]    ir exposure time %d\n",
       func, window->avision.type.normal.ir_exposure_time);
  
  DBG (dbg_level, "%s: [51-52] r exposure %d\n", func,
       get_double (window->avision.type.normal.r_exposure_time));
  DBG (dbg_level, "%s: [53-54] g exposure %d\n", func,
       get_double (window->avision.type.normal.g_exposure_time));
  DBG (dbg_level, "%s: [55-56] b exposure %d\n", func,
       get_double (window->avision.type.normal.b_exposure_time));
  
  DBG (dbg_level, "%s: [57]    bitset %x\n", func,
       window->avision.type.normal.bitset3);
  DBG (dbg_level, "%s: [58]    auto focus %d\n", func,
       window->avision.type.normal.auto_focus);
  DBG (dbg_level, "%s: [59]    line-widht (MSB) %d\n",
       func, window->avision.type.normal.line_width_msb);
  DBG (dbg_level, "%s: [60]    line-count (MSB) %d\n",
       func, window->avision.type.normal.line_count_msb);
  DBG (dbg_level, "%s: [61]    edge threshold %d\n",
       func, window->avision.type.normal.edge_threshold);
}

static
SANE_Status simple_read (Avision_Scanner* s,
			 u_int8_t data_type_code, u_int8_t read_type,
			 size_t* size, u_int8_t* result)
{
  SANE_Status status;
  
  struct command_read rcmd;
  memset (&rcmd, 0, sizeof (rcmd));
  
  rcmd.opc = AVISION_SCSI_READ;
  rcmd.datatypecode = data_type_code;
  rcmd.readtype = read_type;
  set_double (rcmd.datatypequal, s->hw->data_dq);
  set_triple (rcmd.transferlen, *size);

  DBG (3, "simple_read: size: %d\n", *size);
  
  status = sanei_scsi_cmd (s->fd, &rcmd, sizeof(rcmd), result, size);
  return status;
}

#ifdef IS_WRITTEN
static
SANE_Status simple_send (Avision_Scanner* s,
			 u_int8_t data_type_code, u_int8_t read_type,
			 u_int16_t data_type_qual,
			 size_t* size, u_int8_t* result)
{
}
#endif

/* a bubble sort for the calibration - which returns an average
 * of the top 2/3 values */
static u_int16_t
bubble_sort (u_int16_t* sort_data, size_t count)
{
  size_t i, j, k, limit;
  double sum = 0;
  u_int16_t temp;
  
  limit = count / 3;
  
  for (i = 0; i < limit; ++i)
    for (j = (i + 1); j < count; ++j)
      if (sort_data[i] > sort_data[j]) {
	temp = sort_data[i];
	sort_data[i] = sort_data[j];
	sort_data[j] = temp;
      }
  
  for (k = 0, i = limit; i < count; ++ i) {
    sum += sort_data[i];
    ++ k;
  }
  
  if (k > 0) /* if avg to compute */
    return (u_int16_t) (sum / k);
  else
    return (u_int16_t) (sum);
}

static color_mode
make_mode (char* mode)
{
  DBG (3, "make_mode:\n");

  if (strcmp (mode, "Line Art") == 0)
    return AV_THRESHOLDED;
  if (strcmp (mode, "Dithered") == 0)
    return AV_DITHERED;
  else if (strcmp (mode, "Gray") == 0)
    return AV_GRAYSCALE;
  else if (strcmp (mode, "Color") == 0)
    return AV_TRUECOLOR;

  return -1;
}

static int
get_pixel_boundary (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  int boundary;
  
  switch (s->c_mode) {
  case AV_TRUECOLOR:
    boundary = dev->inquiry_color_boundary;
    break;
  case AV_GRAYSCALE:
    boundary = dev->inquiry_gray_boundary;
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
      /*status = SANE_STATUS_GOOD;*/
      text = "ok ?!?";
      break;
    case 0x02:
      text = "NOT READY";
      break;
    case 0x03:
      text = "MEDIUM ERROR (mostly ADF)";
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
      text = "ABORTET COMMAND";
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
    
#define ADDITIONAL_SENSE(asc,ascq,txt) \
case ( (asc << 8) + ascq): text = txt; break
    
    switch ( (asc << 8) + ascq )
      {
	/* normal */
	ADDITIONAL_SENSE (0x00,0x00, "No additional sense information");
	ADDITIONAL_SENSE (0x80,0x01, "ADF paper jam");
	ADDITIONAL_SENSE (0x80,0x02, "ADF cover open");
	ADDITIONAL_SENSE (0x80,0x03, "ADF chute empty");
	ADDITIONAL_SENSE (0x80,0x04, "ADF paper end");
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
	ADDITIONAL_SENSE (0x81,0x00, "ADF front door open");
	ADDITIONAL_SENSE (0x81,0x01, "ADF holder cartrige open");
	ADDITIONAL_SENSE (0x81,0x02, "ADF no film inside");
	ADDITIONAL_SENSE (0x81,0x03, "ADF initial load fail");
	ADDITIONAL_SENSE (0x81,0x04, "ADF film end");
	ADDITIONAL_SENSE (0x81,0x05, "ADF forward feed error");
	ADDITIONAL_SENSE (0x81,0x06, "ADF rewind error");
	ADDITIONAL_SENSE (0x81,0x07, "ADF set unload");
	ADDITIONAL_SENSE (0x81,0x08, "ADF adapter error");
	
	/* maybe Minolta specific */
	ADDITIONAL_SENSE (0x90,0x00, "Scanner busy");
      default:
	text = "Unknown sense code asc: , ascq: "; /* TODO: sprint code here */
      }
    
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

static SANE_Status
wait_ready (int fd)
{
  SANE_Status status;
  int try;
  
  for (try = 0; try < 10; ++ try)
    {
      DBG (3, "wait_ready: sending TEST_UNIT_READY\n");
      status = sanei_scsi_cmd (fd, test_unit_ready, sizeof (test_unit_ready),
			       0, 0);
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
	  sleep (1);
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
  
  if (!dev->inquiry_light_control) {
    DBG (3, "wait_4_light: scanner doesn't support light control.\n");
    return SANE_STATUS_GOOD;
  }

  DBG (3, "wait_4_light: getting light status.\n");
  
  memset (&rcmd, 0, sizeof (rcmd));
  
  rcmd.opc = AVISION_SCSI_READ;
  rcmd.datatypecode = 0xa0; /* get light status */
  set_double (rcmd.datatypequal, dev->data_dq);
  set_triple (rcmd.transferlen, size);
  
  for (try = 0; try < 18; ++ try) {
    
    DBG (5, "wait_4_light: read bytes %d\n", size);
    status = sanei_scsi_cmd (s->fd, &rcmd, sizeof (rcmd), &result, &size);
    
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
      
      /*turn on the light */
      DBG (3, "wait_4_light: setting light status.\n");
      
      memset (&scmd, 0, sizeof (scmd));
      
      scmd.opc = AVISION_SCSI_SEND;
      scmd.datatypecode = 0xa0; /* send light status */
      set_double (scmd.datatypequal, dev->data_dq);
      set_triple (scmd.transferlen, size);
      
      status = sanei_scsi_cmd2 (s->fd, &scmd, sizeof (scmd),
				&light_on, sizeof (light_on), 0, 0);
      
      if (status != SANE_STATUS_GOOD) {
	DBG (1, "wait_4_light: send failed (%s)\n", sane_strstatus (status));
	return status;
      }
    }
    sleep (1);
  }
  
  DBG (1, "wait_4_light: timed out after %d attempts\n", try);
  
  return SANE_STATUS_GOOD;
}

/* C5 hardware scaling parameter. Code by Avision.
   Scan width and length base dpi is not always 1200 dpi.
   Be carefull! */
#ifdef THIS_IS_NEEDED
static void
c5_guard (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  
  int base_dpi = s->avdimen.res - s->avdimen.res % 300;
  int weight, n_width, n_height;
  
  int gray_mode = (s->mode == TRUECOLOR) || (s->mode == GRAYSCALE);
  int boundary = get_pixel_boundary (s);
  
  /* For fix C5 lineart bug. */
  if (!gray_mode) {
    DBG (1, "c5_guard: not gray mode -> boundary = 1\n");
    boundary = 1;
  }
  
  if (base_dpi > dev->inquiry_optical_res)
    base_dpi = dev->inquiry_optical_res;
  else if (s->avdimen.res <= 150)
    base_dpi = 150;
  DBG (1, "c5_guard: base_dpi = %d\n", base_dpi);
  
  weight = (double)base_dpi * 256 / s->avdimen.res;
  DBG (1, "c5_guard: weight = %d\n", weight);
  
  n_width = (256 * (s->avdimen.width - 1) + (weight - 1) ) / weight;
  DBG (1, "c5_guard: n_width = %d\n", n_width);
  
  n_height = (256 * (s->avdimen.height - 1) + (weight - 1) ) / weight;
  DBG (1, "c5_guard: n_height = %d\n", n_height);
  
  /* single bit mode */
  /*
  if (!gray_mode) {
    s->avdimen.width = new_width;
    s->avdimen.brx = s->avdimen.tlx + s->avdimen.width;
  }
  */
  
  /* overwrite with corrected values */
  s->params.lines = n_width;
  s->params.pixels_per_line = n_height;
  
  DBG (1, "c5_guard: Parameters after C5 guard:\n");
  debug_print_avdimen (1, "c5_guard", &s->avdimen);
  debug_print_params (1, "c5_guard", &s->params);
}
#endif

static SANE_Status
get_accessories_info (int fd, SANE_Bool* adf, SANE_Bool* light_box)
{
  /* read stuff */
  struct command_read rcmd;
  size_t size;
  SANE_Status status;
  u_int8_t result[8];
  
  DBG (3, "get_accessories_info\n");
 
  size = sizeof (result);
 
  memset (&rcmd, 0, sizeof (rcmd));
  rcmd.opc = AVISION_SCSI_READ;
 
  rcmd.datatypecode = 0x64; /* detect accessories */
  set_double (rcmd.datatypequal, 0); /* dev->data_dq not available */
  set_triple (rcmd.transferlen, size);
 
  status = sanei_scsi_cmd (fd, &rcmd, sizeof (rcmd), result, &size);
  if (status != SANE_STATUS_GOOD || size != sizeof (result)) {
    DBG (1, "get_accessories_info: read failed (%s)\n",
	 sane_strstatus (status));
    return (status);
  }
 
  debug_print_raw (6, "get_accessories_info: raw data:\n", result, size);
  
  DBG (3, "get_accessories_info: [0]  ADF: %x\n", result [0]);
  DBG (3, "get_accessories_info: [1]  Light Box: %x\n", result [1]);
  
  DBG (3, "get_accessories_info: [2] Bitfield1:%s%s\n",
       BIT(result[39],0)?" Origami ADF":"",
       BIT(result[39],1)?" Oodles ADF":"");
  
  *adf = result [0];
  *light_box = result [1];
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
get_frame_info (int fd, int* number_of_frames, int* frame, int* holder_type)
{
  /* read stuff */
  struct command_read rcmd;
  size_t size;
  SANE_Status status;
  u_int8_t result[8];
  size_t i;
  
  frame = frame; /* silence gcc */
  
  DBG (3, "get_frame_info:\n");
  
  size = sizeof (result);
 
  memset (&rcmd, 0, sizeof (rcmd));
  rcmd.opc = AVISION_SCSI_READ;
 
  rcmd.datatypecode = 0x87; /* film holder sense */
  set_double (rcmd.datatypequal, 0); /* dev->data_dq not available  */
  set_triple (rcmd.transferlen, size);
 
  status = sanei_scsi_cmd (fd, &rcmd, sizeof (rcmd), result, &size);
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

  if (result[0] != 0xff) {
    *number_of_frames = result[2];
  }
  
  *holder_type = result[0];
  DBG(3, "type %x\n", *holder_type);
  
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
  int old_fd;
  
  DBG (3, "set_frame: request frame %d\n", frame);
  
  /* fd may be closed, so we hopen it here */
  
  old_fd = s->fd;
  if (s->fd < 0 ) {
    status = sanei_scsi_open (dev->sane.name, &s->fd, sense_handler, 0);
    if (status != SANE_STATUS_GOOD) {
      DBG (1, "open: open of %s failed: %s\n",
 	   dev->sane.name, sane_strstatus (status));
      return status;
    }
  }
  
  /* Better check the current status of the film holder, because it
     can be changed between scans. */
  status = get_frame_info (s->fd, &dev->frame_range.max, 
			   &dev->current_frame,
			   &dev->holder_type);
  if (status != SANE_STATUS_GOOD)
    return status;
  
  /* No file holder (shouldn't happen) */
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
  
  set_triple (scmd.cmd.transferlen, sizeof (scmd.data) );
  
  scmd.data[0] = dev->holder_type;
  scmd.data[1] = frame; 
  
  status = sanei_scsi_cmd (s->fd, &scmd, sizeof (scmd), 0, 0);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "set_frame: send_data (%s)\n", sane_strstatus (status));
  }  
  
  if (old_fd < 0) {
    sanei_scsi_close (s->fd);
    s->fd = old_fd;
  }
  
  return status;
}

#if 0							/* unused */
static SANE_Status
eject_or_rewind (Avision_Scanner* s)
{
  return (set_frame (s, 0xff) );
}
#endif

static SANE_Status
attach (const char* devname, Avision_Device** devp)
{
  u_int8_t result [INQ_LEN];
  int model_num;

  Avision_Device* dev;
  SANE_Status status;
  int fd;
  size_t size;
  
  char mfg [9];
  char model [17];
  char rev [5];
  
  unsigned int i;
  SANE_Bool found;
  
  DBG (3, "attach: (Version: %i.%i Build: %i)\n", V_MAJOR, V_MINOR, BACKEND_BUILD);
  
  for (dev = first_dev; dev; dev = dev->next)
    if (strcmp (dev->sane.name, devname) == 0) {
      if (devp)
	*devp = dev;
      return SANE_STATUS_GOOD;
    }

  DBG (3, "attach: opening %s\n", devname);
  status = sanei_scsi_open (devname, &fd, sense_handler, 0);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "attach: open failed (%s)\n", sane_strstatus (status));
    return SANE_STATUS_INVAL;
  }

  DBG (3, "attach: sending INQUIRY\n");
  size = sizeof (result);
  status = sanei_scsi_cmd (fd, inquiry, sizeof (inquiry), result, &size);
  if (status != SANE_STATUS_GOOD || size != INQ_LEN) {
    DBG (1, "attach: inquiry failed (%s)\n", sane_strstatus (status));
    goto close_scanner_and_return;
  }

  /* copy string information - and zero terminate them c-style */
  memcpy (&mfg, result + 8, 8);
  mfg [8] = 0;
  memcpy (&model, result + 16, 16);
  model [16] = 0;
  memcpy (&rev, result + 32, 4);
  rev [4] = 0;
  
  /* shorten strings ( -1 for last index
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
  while (Avision_Device_List [model_num].mfg != NULL) {
    if (strcmp (mfg, Avision_Device_List [model_num].mfg) == 0 && 
	(strcmp (model, Avision_Device_List [model_num].model) == 0) ) {
      found = 1;
      DBG (1, "FOUND\n");
      break;
    }
    ++ model_num;
  }
  
  if (!found) {
    DBG (1, "attach: model is not in the list of supported models!\n");
    DBG (1, "attach: You might want to report this output. To:\n");
    DBG (1, "attach: rene@rocklinux.org (Backend author)\n");
    
    status = SANE_STATUS_INVAL;
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
  
  dev->is_calibrated = SANE_FALSE;
  
  debug_print_raw (6, "attach: raw data:\n", result, sizeof (result) );
  
  DBG (3, "attach: [8-15]  Vendor id.:      \"%8.8s\"\n", result+8);
  DBG (3, "attach: [16-31] Product id.:     \"%16.16s\"\n", result+16);
  DBG (3, "attach: [32-35] Product rev.:    \"%4.4s\"\n", result+32);
  
  i = (result[36] >> 4) & 0x7;
  DBG (3, "attach: [36]    Bitfield:%s%s%s%s%s%s%s\n",
       BIT(result[36],7)?" ADF":"",
       (i==0)?" B&W only":"",
       BIT(i, 1)?" 3-pass color":"",
       BIT(i, 2)?" 1-pass color":"",
       BIT(i, 2) && BIT(i, 0) ?" 1-pass color (ScanPartner only)":"",
       BIT(result[36],3)?" IS_NOT_FLATBED:":"",
       (result[36] & 0x7) == 0 ? " RGB_COLOR_PLANE":" RESERVED?");
  
  DBG (3, "attach: [37]    Optical res.:    %d00 dpi\n", result[37]);
  DBG (3, "attach: [38]    Maximum res.:    %d00 dpi\n", result[38]);
  
  DBG (3, "attach: [39]    Bitfield1:%s%s%s%s%s\n",
       BIT(result[39],7)?" TRANS":"",
       BIT(result[39],6)?" Q_SCAN":"",
       BIT(result[39],5)?" EXTENDET_RES":"",
       BIT(result[39],4)?" SUPPORTS_CALIB":"",
       BIT(result[39],2)?" NEW_PROTOCOL":"");
  
  DBG (3, "attach: [40-41] X res. in gray:  %d dpi\n", get_double ( &(result[40]) ));
  DBG (3, "attach: [42-43] Y res. in gray:  %d dpi\n", get_double ( &(result[42]) ));
  DBG (3, "attach: [44-45] X res. in color: %d dpi\n", get_double ( &(result[44]) ));
  DBG (3, "attach: [46-47] Y res. in color: %d dpi\n", get_double ( &(result[46]) ));
  DBG (3, "attach: [48-49] USB max read:    %d\n", get_double ( &(result[48] ) ));

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
       BIT(result[52],5)?" SUPPORTS_TET":"", /* huh ?!? */
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
  
  DBG (3, "attach: [60]    channels per pixel:%s%s\n",
  	BIT(result[60],7)?" 1":"",
  	BIT(result[60],6)?" 3":"");
  DBG (3, "attach: [61]    bits per channel:%s%s%s%s%s%s%s\n", 
       BIT(result[61],7)?" 1":"",
       BIT(result[61],6)?" 4":"",
       BIT(result[61],5)?" 6":"",
       BIT(result[61],4)?" 8":"",
       BIT(result[61],3)?" 10":"",
       BIT(result[61],2)?" 12":"",
       BIT(result[61],1)?" 16":"");
  
  DBG (3, "attach: [62]    scanner type:%s%s%s%s%s\n", 
       BIT(result[62],7)?" Flatbed (w.o.? ADF)":"",
       BIT(result[62],6)?" Roller (ADF)":"",
       BIT(result[62],5)?" Flatbed (ADF)":"",
       BIT(result[62],4)?" Roller (w.o. ADF)":"",
       BIT(result[62],3)?" Film scanner":"");
  
  DBG (3, "attach: [75-75] Max shading target : %x\n", get_double ( &(result[75]) ));
  
  DBG (3, "attach: [77-78] Max X of transparency: %d FUBA\n", get_double ( &(result[77]) ));
  DBG (3, "attach: [79-80] May Y of transparency: %d FUBA\n", get_double ( &(result[79]) ));
  
  DBG (3, "attach: [81-82] Max X of flatbed:      %d FUBA\n", get_double ( &(result[81]) ));
  DBG (3, "attach: [83-84] May Y of flatbed:      %d FUBA\n", get_double ( &(result[83]) ));
  
  DBG (3, "attach: [85-86] Max X of ADF:          %d FUBA\n", get_double ( &(result[85]) ));
  DBG (3, "attach: [87-88] May Y of ADF:          %d FUBA\n", get_double ( &(result[87]) ));
  DBG (3, "attach: (FUBA: inch/300 for flatbed, and inch/max_res for film scanners?)\n");
  
  DBG (3, "attach: [89-90] Res. in Ex. mode:      %d dpi\n", get_double ( &(result[89]) ));
  
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
  
  DBG (3, "attach: [94]    ESA5:%s%s%s%s%s\n",
       BIT(result[94],7)?" IGNORE_LINE_DIFFERENCE_FOR_ADF":"",
       BIT(result[94],6)?" NEEDS_SW_LINE_COLOR_PACK":"",
       BIT(result[94],5)?" SUPPORTS_DUPLEX_SCAN":"",
       BIT(result[94],4)?" INTERLACE_DUPLEX_SCAN":"",
       BIT(result[94],3)?" SUPPORTS_TWO_MODE_ADF_SCANS":"");
  
  /* if (BIT(result[62],3) ) */
  switch (dev->hw->scanner_type) {
  case AV_FLATBED:
    dev->sane.type = "flatbed scanner";
    break;
  case AV_FILM:
    dev->sane.type = "film scanner";
    break;
  case AV_SHEETFEED:
    dev->sane.type =  "sheetfed scanner";
    break;
  }
  
  dev->inquiry_asic_type = (int) result[91];
  dev->inquiry_adf = (SANE_Bool) BIT (result[62], 5);
  
  dev->inquiry_new_protocol = BIT (result[39],2);
  dev->inquiry_detect_accessories = BIT(result[93],7);
  
  dev->inquiry_needs_calibration = BIT (result[50],4);
  
  dev->inquiry_needs_gamma = BIT (result[50],3);
  dev->inquiry_3x3_matrix = BIT (result[52], 4);
  dev->inquiry_needs_software_colorpack = BIT (result[50],5);
  dev->inquiry_needs_line_pack = BIT (result[94], 6);
  dev->inquiry_adf_need_mirror = BIT (result[51], 0);
  
  dev->inquiry_light_detect = BIT (result[93], 2);
  dev->inquiry_light_control = BIT (result[50], 7);
  
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
      if (dev->hw->scanner_type == AV_SHEETFEED) {
	DBG (1, "Inquiry optical resolution is invalid, using 300 dpi (sf)!\n");
	dev->inquiry_optical_res = 300;
      }
      else {
	DBG (1, "Inquiry optical resolution is invalid, using 600 dpi (def)!\n");
	dev->inquiry_optical_res = 600;
      }
    }
  if (dev->inquiry_max_res == 0) {
    DBG (1, "Inquiry max resolution is invalid, using 1200 dpi!\n");
    dev->inquiry_max_res = 1200;
  }
  
  DBG (1, "attach: optical resolution set to: %d dpi\n", dev->inquiry_optical_res);
  DBG (1, "attach: max resolution set to: %d dpi\n", dev->inquiry_max_res);
  
  /* get max x/y ranges for the different modes */
  {
    double base_dpi;
    if (dev->hw->scanner_type != AV_FILM) {
      base_dpi = AVISION_BASE_RES;
    } else {
      /* ZP: The right number is 2820, wether it is 40-41, 42-43, 44-45,
       * 46-47 or 89-90 I don't know but I would bet for the last !
       * r²: OK. We use it via the optical_res which we need anyway ...
       */
      base_dpi = dev->inquiry_optical_res;
    }
    
    dev->inquiry_x_ranges [AV_NORMAL] =
      get_double (&(result[81])) * MM_PER_INCH / base_dpi;
    dev->inquiry_y_ranges [AV_NORMAL] =
      get_double (&(result[83])) * MM_PER_INCH / base_dpi;
  
    dev->inquiry_x_ranges [AV_TRANSPARENT] =
      get_double (&(result[77])) * MM_PER_INCH / base_dpi;
    dev->inquiry_y_ranges [AV_TRANSPARENT] =
      get_double (&(result[79])) * MM_PER_INCH / base_dpi;
  
    dev->inquiry_x_ranges [AV_ADF] =
      get_double (&(result[85])) * MM_PER_INCH / base_dpi;
    dev->inquiry_y_ranges [AV_ADF] =
      get_double (&(result[87])) * MM_PER_INCH / base_dpi;
  }
  
  /* check if x/y ranges are valid :-((( */
  {
    operation_mode mode;

    for (mode = AV_NORMAL; mode < AV_OPERATION_MODE_LAST; ++ mode)
      {
	if (dev->inquiry_x_ranges [mode] != 0 &&
	    dev->inquiry_y_ranges [mode] != 0)
	  {
	    DBG (3, "attach: x/y-range for mode %d is valid!\n", mode);
	    if (force_a4) {
	      DBG (1, "attach: \"force_a4\" found! Using defauld (ISO A4).\n");
	      dev->inquiry_x_ranges [mode] = A4_X_RANGE * MM_PER_INCH;
	      dev->inquiry_y_ranges [mode] = A4_Y_RANGE * MM_PER_INCH;
	    }
	  }
	else /* mode is invaild */
	  {
	    DBG (1, "attach: x/y-range for mode %d is invalid! Using default.\n", mode);
	    
	    dev->inquiry_x_ranges [mode] = A4_X_RANGE * MM_PER_INCH;
	    
	    if (dev->hw->scanner_type == AV_SHEETFEED)
	      dev->inquiry_y_ranges [mode] = SHEETFEED_Y_RANGE * MM_PER_INCH;
	    else
	      dev->inquiry_y_ranges [mode] = A4_Y_RANGE * MM_PER_INCH;
	  }
	DBG (1, "attach: Mode %d range is now: %f x %f mm.\n",
	     mode,
	     dev->inquiry_x_ranges [mode], dev->inquiry_y_ranges [mode]);
      } /* end for all modes */
  }
  
  /* We need a bigger buffer for USB devices, since they seem to have
     a firmware bug and do not support reading the calibration data in
     tiny chunks */
  if (dev->hw->connection_type == AV_USB)
    dev->scsi_buffer_size = 256 * 1024; /* 256 kB for now */
  else
    dev->scsi_buffer_size = sanei_scsi_max_request_size;
  
  /* Try to retrieve additional accessories information */
  if (dev->inquiry_detect_accessories) {
    status = get_accessories_info (fd,
				   &dev->acc_adf,
				   &dev->acc_light_box);
    if (status != SANE_STATUS_GOOD)
      goto close_scanner_and_return;
  }
  
  /* is an ADF unit connected? */
  if (dev->acc_adf)
    dev->is_adf = SANE_TRUE;
  
  /* For a film scanner try to retrieve additional frame information */
  if (dev->hw->scanner_type == AV_FILM) {
    status = get_frame_info (fd,
			     &dev->frame_range.max,
			     &dev->current_frame,
			     &dev->holder_type);
    dev->frame_range.min = 1;
    dev->frame_range.quant = 1;
    
    if (status != SANE_STATUS_GOOD)
      goto close_scanner_and_return;
  }
  
  /* normally the data_dq is 0x0a0d - but newer scanner hang with it ... */
  if (!dev->inquiry_new_protocol)
    dev->data_dq = 0x0a0d;
  else
    dev->data_dq = 0;

  status = wait_ready (fd);
  if (status != SANE_STATUS_GOOD)
    goto close_scanner_and_return;
  
  sanei_scsi_close (fd);
  
  ++ num_devices;
  dev->next = first_dev;
  first_dev = dev;
  if (devp)
    *devp = dev;
  
  return SANE_STATUS_GOOD;
  
 close_scanner_and_return:
  sanei_scsi_close (fd);
  return status;
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
  
  DBG (3, "get_calib_format: read_data: %d bytes\n", size);
  status = sanei_scsi_cmd (s->fd, &rcmd, sizeof (rcmd), result, &size);
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
  
  if ((s->c_mode == AV_TRUECOLOR) || BIT(format->ability1, 3))
    {
      format->lines /= 3;
      format->channels = 3;
    }
  else
    format->channels = 1;
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
get_calib_data (Avision_Scanner* s, u_int8_t data_type,
		u_int8_t* calib_data,
		size_t calib_size, size_t line_size)
{
  SANE_Status status;
  u_int8_t *calib_ptr;
  
  size_t get_size, data_size;
  
  struct command_read rcmd;
  
  DBG (3, "get_calib_data: type %x, size %d, line_size: %d\n",
       data_type, calib_size, line_size);
  
  memset (&rcmd, 0, sizeof (rcmd));
  
  rcmd.opc = AVISION_SCSI_READ;
  rcmd.datatypecode = data_type;
  set_double (rcmd.datatypequal, s->hw->data_dq);
  
  calib_ptr = calib_data;
  get_size = line_size;
  data_size = calib_size;
  
  while (data_size) {
    if (get_size > data_size)
      get_size = data_size;
    data_size -= get_size;
    set_triple (rcmd.transferlen, get_size);
    
    status = sanei_scsi_cmd (s->fd, &rcmd,
			     sizeof (rcmd), calib_ptr, &get_size);
    if (status != SANE_STATUS_GOOD) {
      DBG (1, "get_calib_data: read data failed (%s)\n",
	   sane_strstatus (status));
      return status;
    }
    calib_ptr += get_size;
  }
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
set_calib_data (Avision_Scanner* s, struct calibration_format* format,
	       u_int16_t* dark_data, u_int16_t* white_data)
{
  struct command_send scmd;
  
  SANE_Status status;
  
  u_int8_t dq;
  u_int8_t dq1;
  u_int8_t* out_data;
  
  int i, out_size;
  int element_per_line = format->pixel_per_line * format->channels;
  
  DBG (3, "set_calib_data:\n");
  
  memset (&scmd, 0x00, sizeof (scmd));
  scmd.opc = AVISION_SCSI_SEND;
  scmd.datatypecode = 0x82; /* 0x82 for download calibration data */
  
  /* dark calibration ? */
  if (BIT (format->ability1, 2) ) {
    for (i = 0; i < element_per_line; ++i)
      white_data[i] = (white_data[i] & 0xffc0) | ((dark_data[i] >> 10) & 0x3f);
  }
  
  out_size = format->pixel_per_line * 2;
  
  for (dq1 = 0; dq1 < format->channels; ++dq1)
    {
      if (format->channels == 1)
	dq = 0x11; /* send green channel */
      else
	dq = dq1;
      
      set_double (scmd.datatypequal, dq); /* color: 0=red; 1=green; 2=blue */
      set_triple (scmd.transferlen, out_size);
      
      out_data = (u_int8_t *) & white_data[dq1 * format->pixel_per_line];
      
      DBG (3, "set_calib_data: dq: %d, size: %d\n",
	   dq, out_size);
      
      status = sanei_scsi_cmd2 (s->fd, &scmd, sizeof (scmd),
				out_data, out_size, 0, 0);
      if (status != SANE_STATUS_GOOD) {
	DBG (3, "set_calib_data: send failed (%s)\n", sane_strstatus (status));
	return status;
      }
    } /* end for each channel */
  
  return SANE_STATUS_GOOD;
}

/* Sort data pixel by pixel and average first 2/3 of the data.
   The caller has to free return pointer. R,G,B pixels
   interleave to R,G,B line interleave. */

static u_int16_t*
sort_and_average (struct calibration_format* format, u_int8_t* data)
{
  /* calibration data in 16 bits always */
  /* that is a = b[1] << 8 + b[0] in all system. */
  
  int stride, i, line;
  int elements_per_line;
  
  u_int16_t *sort_data, *avg_data;
  
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
      
      /* copy all lines for pixel i into a linear array */
      for (line = 0; line < format->lines; ++ line) {
	u_int8_t* ptr2 = ptr1 + line * stride; /* pixel */
	
	if (format->bytes_per_channel == 1)
	  sort_data[line] = (u_int16_t) (*ptr2 << 8);
	else
	  sort_data[line] = get_double (ptr2);
      }
      
      avg_data[i] = bubble_sort (sort_data, format->lines);
    }
  
  free ((void *) sort_data);
  return avg_data;
}

/* shading data is 16bits little endian format when send/read from firmware */
static void
compute_dark_shading_data (Avision_Scanner* s,
			   struct calibration_format* format, u_int16_t* data)
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
      if (data[i] > rgb_map_value[i & 0x3]) {
	data[i] -= rgb_map_value[i & 0x3];
      }
      else {
	data[i] = 0;
      }
    }
}

static void
compute_white_shading_data (Avision_Scanner* s,
			    struct calibration_format* format, u_int16_t* data)
{
  int i;
  u_int16_t inquiry_mst = DEFAULT_WHITE_SHADING;
  u_int32_t mst[3], result;
  u_int16_t map_range = WHITE_MAP_RANGE;
  
  int elements_per_line = format->pixel_per_line * format->channels;
  
  DBG (3, "compute_white_shading_data:\n");
  
  if (s->hw->inquiry_max_shading_target != INVALID_WHITE_SHADING)
    inquiry_mst = s->hw->inquiry_max_shading_target << 4;
  
  mst[0] = format->r_shading_target << 8;
  mst[1] = format->g_shading_target << 8;
  mst[2] = format->b_shading_target << 8;
  
  for (i = 0; i < format->channels; ++i) {
    if (mst[i] == INVALID_WHITE_SHADING)
      mst[i] = inquiry_mst;
  }
  
  DBG (3, "compute_white_shading_data: r target: %x\n", mst[0]);
  DBG (3, "compute_white_shading_data: g target: %x\n", mst[1]);
  DBG (3, "compute_white_shading_data: b target: %x\n", mst[2]);
  
  /* some Avision example code was present here until SANE/Avision
   * BUILD 57. */
  
  /* calculate calibration data */
  for (i = 0; i < elements_per_line; ++ i)
    {
      /* calculate calibration value for pixel i */
      if (data[i]) {
	result =
	  (u_int32_t) ((double) mst[i % 3] * map_range / data[i] + 0.5);
      }
      else
	result = DEFAULT_WHITE_SHADING;
      
      if (result > MAX_WHITE_SHADING)
	result = DEFAULT_WHITE_SHADING;
      
      data[i] = (u_int16_t) result;
    }
}

static SANE_Status
old_r_calibration (Avision_Scanner* s)
{
  SANE_Status status;
  
  SANE_Bool send_single_channels;
  
  size_t size;
  
  struct command_read rcmd;
  struct command_send scmd;  
  
  unsigned int i;
  unsigned int color;
  
  size_t calib_size;
  u_int8_t* calib_data;
  size_t out_size;
  u_int8_t* out_data;
  
  u_int8_t result [16];
  
  DBG (3, "old_r_calibration: get calibration format\n");

  size = sizeof (result);
  
  memset (&rcmd, 0, sizeof (rcmd));
  rcmd.opc = AVISION_SCSI_READ;
  rcmd.datatypecode = 0x60; /* get calibration format */
  set_double (rcmd.datatypequal, s->hw->data_dq);
  set_triple (rcmd.transferlen, size);
  
  DBG (3, "old_r_calibration: read_data: %d bytes\n", size);
  status = sanei_scsi_cmd (s->fd, &rcmd, sizeof (rcmd), result, &size);
  if (status != SANE_STATUS_GOOD || size != sizeof (result) ) {
    DBG (1, "old_r_calibration: read calib. info failt (%s)\n",
	 sane_strstatus (status) );
    return status;
  }
  
  debug_print_calib_format (5, "old_r_calibration", result);
  
  send_single_channels = BIT(result[5],0);
  
  /* (try) to get calibration data:
   * the read command 63 (read channel data hangs for HP 5300 ?
   */
  
  {
    unsigned int calib_pixels_per_line = get_double ( &(result[0]) );
    unsigned int calib_bytes_per_channel = result[2];
    unsigned int calib_line_count = result[3];
    
    /* Limit to max scsi transfer size ???
    unsigned int used_lines = sanei_scsi_max_request_size /
      (calib_pixels_per_line * calib_bytes_per_channel);
    */
    
    unsigned int used_lines = calib_line_count;
    
    DBG (3, "old_r_calibration: using %d lines\n", used_lines);
    
    calib_size = calib_pixels_per_line * calib_bytes_per_channel * used_lines;
    DBG (3, "old_r_calibration: read %d bytes\n", calib_size);
    
    calib_data = malloc (calib_size);
    if (!calib_data)
      return SANE_STATUS_NO_MEM;
    
    memset (&rcmd, 0, sizeof (rcmd));
    rcmd.opc = AVISION_SCSI_READ;
    /* 66: dark, 62: color, (63: channel HANGS) data */
    rcmd.datatypecode = 0x62;
    /* only needed for single channel mode - which hangs the HP 5300 */
    /* rcmd.calibchn = color; */
    set_double (rcmd.datatypequal, s->hw->data_dq);
    set_triple (rcmd.transferlen, calib_size);
    
     status = sanei_scsi_cmd (s->fd, &rcmd, sizeof (rcmd), calib_data, &calib_size);
    if (status != SANE_STATUS_GOOD) {
      DBG (1, "old_r_calibration: calibration data read failed (%s)\n", sane_strstatus (status));
      return status;
    }
    
    DBG (10, "RAW-Calibration-Data (%d bytes):\n", calib_size);
    for (i = 0; i < calib_size; ++ i) {
      DBG (10, "calib_data [%2d] %3d\n", i, calib_data [i] );
    }
    
    /* compute data */
    
    out_size = calib_pixels_per_line * 3 * 2; /* *2 because it is 16 bit fixed-point */
    
    out_data = malloc (out_size);
    if (!out_data)
      return SANE_STATUS_NO_MEM;
    
    DBG (3, "old_r_calibration: computing: %d bytes calibration data\n", out_size);
    
    /* compute calibration data */
    {
      /* Is the calib download format:
	 RGBRGBRGBRGBRGBRGB ... */
      
      /* Is the upload format:
	 RR GG BB RR GG BB RR GG BB ...
	 using 16 values for each pixel ? */
      
      /* What I know for sure: The HP 5300 needs 16bit data in R-G-B order ;-) */
      
      unsigned int lines = used_lines / 3;
      unsigned int offset = calib_pixels_per_line * 3;
      
      for (i = 0; i < calib_pixels_per_line * 3; ++ i)
	{
	  unsigned int line;
	  
	  double avg = 0;
	  long factor;
	  
	  for (line = 0; line < lines; ++ line) {
	    if ( (line * offset) + i >= calib_size)
	      DBG (3, "old_r_calibration: BUG: src out of range!!! %d\n", (line * offset) + i);
	    else
	      avg += calib_data [ (line * offset) + i];
	  }
	  avg /= lines;
	  
	  /* we should use some custom per scanner magic here ... */
	  factor = 0xffff*(270.452/(1.+4.24955*avg));
	  if (factor > 0xffff)
	    factor = 0xffff;
	  
	  DBG (8, "pixel: %d: avg: %f, factor: %lx\n", i, avg, factor);
	  if ( (i * 2) + 1 < out_size) {
	    out_data [(i * 2)] = factor;
	    out_data [(i * 2) + 1] = factor >> 8;
	  }
	  else {
	    DBG (3, "old_r_calibration: BUG: dest out of range!!! %d\n", (i * 2) + 1);
	  }
	}
    }
    
    if (!send_single_channels)
      {
	DBG (3, "old_r_calibration: all channels in one command\n");

	memset (&scmd, 0, sizeof (scmd));
	scmd.opc = AVISION_SCSI_SEND;
	scmd.datatypecode = 0x82; /* send calibration data */
	/* 0,1,2: color; 11: dark; 12 color calib data */
	set_double (scmd.datatypequal, 0x12);
	set_triple (scmd.transferlen, out_size);
	
	status = sanei_scsi_cmd2 (s->fd, &scmd, sizeof (scmd),
				  out_data, out_size, 0, 0);
	if (status != SANE_STATUS_GOOD) {
	  DBG (3, "old_r_calibration: send_data failed (%s)\n",
	       sane_strstatus (status));
	  /* not return immediately to free meme at the end */
	}
      }
    /* send data in an alternative way (for HP 7400, and more?) */
    else
      {
	u_int8_t* converted_out_data;
	
	DBG (3, "old_r_calibration: channels in single commands\n");
	
	converted_out_data = malloc (out_size / 3);
	if (!out_data) {
	  status = SANE_STATUS_NO_MEM;
	}
	else {
	  for (color = 0; color < 3; ++ color)
	    {
	      int conv_out_size = calib_pixels_per_line * 2;
	      
	      DBG (3, "old_r_calibration: channel: %i\n", color);
	      
	      for (i = 0; i < calib_pixels_per_line; ++ i) {
		int conv_pixel = i * 2;
		int out_pixel = ((i * 3) + color) * 2;
		
		converted_out_data [conv_pixel] = out_data [out_pixel];
		converted_out_data [conv_pixel + 1] = out_data [out_pixel + 1];
	      }
	      
	      DBG (3, "old_r_calibration: sending now %i bytes \n", conv_out_size);
	      
	      memset (&scmd, 0, sizeof (scmd));
	      scmd.opc = AVISION_SCSI_SEND;
	      scmd.datatypecode = 0x82; /* send calibration data */
	      
	      /* 0,1,2: color; 11: dark; 12 color calib data */
	      set_double (scmd.datatypequal, color);
	      set_triple (scmd.transferlen, conv_out_size);
	      
	      status = sanei_scsi_cmd2 (s->fd, &scmd, sizeof (scmd),
					converted_out_data, conv_out_size, 0, 0);
	      if (status != SANE_STATUS_GOOD) {
		DBG (3, "old_r_calibration: send_data failed (%s)\n", sane_strstatus (status));
		/* not return immediately to free mem at the end */
	      }
	    }
	  free (converted_out_data);
	} /* end else send calib data*/
      }
    
    free (calib_data); /* crashed some times with glibc-2.1.3 ??? */
    free (out_data);
  } /* end compute calibration data */
  
  DBG (3, "old_r_calibration: returns\n");
  return status;
}

static SANE_Status
normal_calibration (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  SANE_Status status;
  
  struct calibration_format calib_format;
  struct command_send scmd;
  
  u_int8_t* calib_data;
  u_int16_t* avg_data;
  
  u_int8_t read_type;
  u_int8_t send_type;
  u_int16_t send_type_q;
  
  size_t line_size, data_size, temp_size;
  
  int i;
  
  DBG (1, "normal_calibration:\n");
  
  /* get calibration format and data */
  status = get_calib_format (s, &calib_format);
  if (status != SANE_STATUS_GOOD)
    return status;
  
  send_type = 0x82; /* download calibration data */
  
  if ((s->c_mode == AV_TRUECOLOR) || BIT(calib_format.ability1, 3)) { /* color mode? */
    DBG (3, "normal_calibration: using color calibration\n");
    read_type = 0x62; /* read color calib data */
    send_type_q = 0x12; /* color calib data */
  }
  else {
    DBG (3, "normal_calibration: using gray calibration\n");
    read_type = 0x61; /* gray calib data */
    send_type_q = 0x11; /* gray/bw calib data */
  }
  
  line_size = calib_format.pixel_per_line * calib_format.bytes_per_channel
    * calib_format.channels;
  
  data_size = line_size * calib_format.lines;
  
  temp_size = calib_format.pixel_per_line * 2 * calib_format.channels;
  
  DBG (3, "normal_calibration: line_size: %d, data_size: %d, temp_size: %d\n",
       line_size, data_size, temp_size);
  
  calib_data = (u_int8_t*) malloc (data_size);
  if (!calib_data) {
    return SANE_STATUS_NO_MEM;
  }
  
  get_calib_data (s, read_type, calib_data,
		  data_size, dev->scsi_buffer_size); /* R²: was data_size or line_size */
  
  if (status != SANE_STATUS_GOOD) {
    free (calib_data);
    return status;
  }
  
  avg_data = sort_and_average (&calib_format, calib_data);
  if (!avg_data) {
    free (calib_data);
    return SANE_STATUS_NO_MEM;
  }
  
  compute_white_shading_data (s, &calib_format, avg_data);
  
  if (! BIT(calib_format.ability1, 0) ) /* send data in one command */
    {
      DBG (3, "normal_calibration: all channels in one command\n");
      DBG (3, "normal_calibration: send_size: %d\n",
	   temp_size);
      
      memset (&scmd, 0, sizeof (scmd) );
      scmd.opc = AVISION_SCSI_SEND;
      scmd.datatypecode = send_type;
      set_double (scmd.datatypequal, send_type_q);
      
      set_triple (scmd.transferlen, temp_size);
      status = sanei_scsi_cmd2 (s->fd, &scmd, sizeof (scmd),
				(char*) avg_data, temp_size, 0, 0);
      /* not return immediately to free mem at the end */
    }
  else /* send data channel by channel (for HP 7400, and other USB ones) */
    {
      int conv_out_size = calib_format.pixel_per_line * 2;
      u_int16_t* conv_out_data;
      
      DBG (3, "normal_calibration: channels in single commands\n");
      
      conv_out_data =  (u_int16_t*) malloc (conv_out_size);
      if (!conv_out_data) {
	status = SANE_STATUS_NO_MEM;
      }
      else {
	int channel;
	for (channel = 0; channel < 3; ++ channel)
	  {
	    DBG (3, "normal_calibration: channel: %i\n", channel);
	    
	    for (i = 0; i < calib_format.pixel_per_line; ++ i)
	      conv_out_data [i] = avg_data [i * 3 + channel];
	    
	    DBG (3, "normal_calibration: sending %i bytes now\n",
		 conv_out_size);
	    
	    memset (&scmd, 0, sizeof (scmd));
	    scmd.opc = AVISION_SCSI_SEND;
	    scmd.datatypecode = 0x82; /* send calibration data */
	    
	    /* 0,1,2: color; 11: dark; 12 color calib data */
	    set_double (scmd.datatypequal, channel);
	    set_triple (scmd.transferlen, conv_out_size);
	    
	    status = sanei_scsi_cmd2 (s->fd, &scmd, sizeof (scmd),
				      conv_out_data, conv_out_size, 0, 0);
	    if (status != SANE_STATUS_GOOD) {
	      DBG (3, "normal_calibration: send_data failed (%s)\n",
		   sane_strstatus (status));
	      /* not return immediately to free mem at the end */
	    }
	  } /* end for each channel */
	free (conv_out_data);
      } /* end else send calib data*/
    }
  
  free ((void*)calib_data);
  
  if (status != SANE_STATUS_GOOD) {
    DBG (3, "normal_calibration: send data  failed (%s)\n",
	 sane_strstatus (status));
    return status;
  }
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
c5_calibration (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  SANE_Status status;
  
  struct calibration_format calib_format;
  
  int calib_data_size, calib_bytes_per_line;
  
  u_int16_t *dark_avg_data, *white_avg_data;
  u_int8_t *calib_tmp_data;
  
  DBG (1, "c5_calibration:\n");
  
  /* get calibration format and data */
  status = get_calib_format (s, &calib_format);
  if (status != SANE_STATUS_GOOD)
    return status;

#ifdef NEW_CODE
  /* check if need do calibration */
  /* prescan always do calibration if need */
  if (calib_format.flags == 1) /* no calib neede */
    return SANE_STATUS_GOOD;
#endif

  /* calculate calibration data size for read from scanner */
  /* size = lines * bytes_per_channel * pixels_per_line * channel */
  calib_bytes_per_line = calib_format.bytes_per_channel *
    calib_format.pixel_per_line * calib_format.channels;
  
  calib_data_size = calib_format.lines * calib_bytes_per_line;
  
  calib_tmp_data = (u_int8_t*) malloc (calib_data_size);
  if (!calib_tmp_data)
    return SANE_STATUS_NO_MEM;
  
  /* check if we need to do dark calibration (shading) */
  if (calib_format.ability1 & 0x04)
    {
      /* read dark calib data */
      status = get_calib_data (s, 0x66, calib_tmp_data, calib_data_size,
			       dev->scsi_buffer_size /* R²: was data_size or line_size */ );
      
      if (status != SANE_STATUS_GOOD) {
	free (calib_tmp_data);
	return status;
      }
      
      /* process dark data: sort and average. */
      dark_avg_data = sort_and_average (&calib_format, calib_tmp_data);
      if (!dark_avg_data) {
	free (calib_tmp_data);
	return SANE_STATUS_NO_MEM;
      }
      compute_dark_shading_data (s, &calib_format, dark_avg_data);
    }
  else
    dark_avg_data = NULL;

  /* do white calibration: read gray or color data */
  status = get_calib_data (s, (calib_format.channels == 3) ? 0x62 : 0x61,
			   calib_tmp_data, calib_data_size,
			   dev->scsi_buffer_size /* R² was: calib_data_size or calib_bytes_per_line */ );
  
  if (status != SANE_STATUS_GOOD) {
    free (calib_tmp_data);
    if (dark_avg_data)
      free (dark_avg_data);
    return status;
  }
  
  white_avg_data = sort_and_average (&calib_format, calib_tmp_data);
  
  if (!white_avg_data) {
    free (calib_tmp_data);
    if (dark_avg_data)
      free (dark_avg_data);
    return SANE_STATUS_NO_MEM;
  }
  
  /* decrease white average data - if dark average data is present */
  if (dark_avg_data) {
    int elements_per_line = calib_format.pixel_per_line * calib_format.channels;
    int i;

    DBG (1, "c5_calibration: dark data present - decreasing white aerage data\n");
    
    for (i = 0; i < elements_per_line; ++ i) {
      white_avg_data[i] -= dark_avg_data[i];
    }
  }
  
  compute_white_shading_data (s, &calib_format, white_avg_data);
  
  status = set_calib_data (s, &calib_format, dark_avg_data, white_avg_data);
  
  free (white_avg_data);
  if (dark_avg_data)
    free (dark_avg_data);
  free (calib_tmp_data);
  
  return status;
}

/* next was taken from the GIMP and is a bit modifyed ... ;-)
 * original Copyright (C) 1995 Spencer Kimball and Peter Mattis
 */
static double 
brightness_contrast_func (double brightness, double contrast, double value)
{
  double  nvalue;
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
set_gamma (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  SANE_Status status;
  
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
      gamma_table_raw_size = 4096;
      gamma_table_size = 2048;
      break;
    case AV_ASIC_C5:
    case AV_ASIC_C6:
      gamma_table_raw_size = gamma_table_size = 256;
      break;
    default:
      gamma_table_raw_size = gamma_table_size = 4096;
    }
  
  gamma_values = gamma_table_size / 256;
  
  DBG (3, "set_gamma: table_raw_size: %d, table_size: %d\n",
       gamma_table_raw_size, gamma_table_size);
  DBG (3, "set_gamma: values: %d, invert_table: %d\n",
       gamma_values, invert_table);
  
  /* prepare for emulating contrast, brightness ... via the gamma-table */
  brightness = SANE_UNFIX (s->val[OPT_BRIGHTNESS].w);
  brightness /= 100;
  contrast = SANE_UNFIX (s->val[OPT_CONTRAST].w);
  contrast /= 100;
  
  DBG (3, "set_gamma: brightness: %f, contrast: %f\n", brightness, contrast);
  
  gamma_data = malloc (gamma_table_raw_size);
  if (!gamma_data)
    return SANE_STATUS_NO_MEM;
  
  memset (&scmd, 0, sizeof (scmd) );
  
  scmd.opc = AVISION_SCSI_SEND;
  scmd.datatypecode = 0x81; /* 0x81 for download gama table */
  set_triple (scmd.transferlen, gamma_table_raw_size);
  
  for (color = 0; color < 3; ++ color)
    {      
      set_double (scmd.datatypequal, color); /* color: 0=red; 1=green; 2=blue */
      
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
	      {
		v1 = (double) (s->gamma_table [0][j] +
			       s->gamma_table [1 + color][j] ) / 2;
		if (j == 255)
		  v2 = (double) v1;
		else
		  v2 = (double) (s->gamma_table [0][j + 1] +
				 s->gamma_table [1 + color][j + 1] ) / 2;
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
	  
	  /* emulate brightness, contrast (at least the Avision AV6[2,3]0 are not
	   * able to do this in hardware ... --EUR - taken from the GIMP source -
	   * I'll optimize it when it is known to work (and I have time)
	   */
	  
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
	  DBG (4, "set_gamma: (old protocol) - filling the table.\n");
	  for ( ; i < gamma_table_raw_size; ++ i)
	    gamma_data [i] = gamma_data [t_i];
	}
      }
      
      DBG (4, "set_gamma: sending %d bytes gamma table.\n",
	   gamma_table_raw_size);
      status = sanei_scsi_cmd2 (s->fd, &scmd, sizeof (scmd),
				gamma_data, gamma_table_raw_size, 0, 0);
    }
  free (gamma_data);
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
  
  struct {
    struct command_set_window cmd;
    struct command_set_window_window window;
  } cmd;
  
  DBG (1, "set_window\n");
  
  if (dev->inquiry_asic_type != AV_ASIC_C5) {
    base_dpi = 1200;
  }
  else {
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
  paralen = sizeof (cmd.window.avision)
    - sizeof (cmd.window.avision.type);
  
  DBG (2, "set_window: base paralen: %d\n", paralen);
  
  if (dev->hw->feature_type & AV_FUJITSU)
    paralen += sizeof (cmd.window.avision.type.fujitsu);
  else if (!dev->inquiry_new_protocol)
    paralen += sizeof (cmd.window.avision.type.old);
  else
    paralen += sizeof (cmd.window.avision.type.normal);
  
  DBG (2, "set_window: paralen: %d\n", paralen);
  
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
  
  /* upper left corner coordinates in inch * base_dpi */
  set_quad (cmd.window.descriptor.ulx, s->avdimen.tlx * base_dpi / s->avdimen.xres);
  set_quad (cmd.window.descriptor.uly, s->avdimen.tly * base_dpi / s->avdimen.yres);
  
  /* width and length in inch * base_dpi */
  set_quad (cmd.window.descriptor.width,
	    s->params.pixels_per_line * base_dpi / s->avdimen.xres);
  set_quad (cmd.window.descriptor.length,
	    (s->params.lines + s->avdimen.line_difference) * base_dpi / s->avdimen.yres);

  /* width and length in _bytes_ */
  set_double (cmd.window.avision.line_width, s->params.bytes_per_line);
  set_double (cmd.window.avision.line_count, s->params.lines + s->avdimen.line_difference);
  
  /* here go the most significant bits if bigger than 16 bit */
  if (dev->inquiry_new_protocol && !(dev->hw->feature_type & AV_FUJITSU) ) {
    DBG (2, "set_window: large data-transfer support (>16bit)!\n");
    cmd.window.avision.type.normal.line_width_msb =
      (s->params.bytes_per_line) >> 16;
    cmd.window.avision.type.normal.line_count_msb =
      (s->params.lines + s->avdimen.line_difference) >> 16;
  }
  
  /* scanner should use our line-width and count */
  SET_BIT (cmd.window.avision.bitset1, 6); 
  
  /* set speed */
  cmd.window.avision.bitset1 = s->val[OPT_SPEED].w & 0x07; /* only 3 bit */
  
  /* ADF scan */
  if (s->val[OPT_ADF].w) {
    SET_BIT (cmd.window.avision.bitset1, 7);
  }
  
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
      if (s->val[OPT_TRANS].w == SANE_TRUE) {
	SET_BIT (cmd.window.avision.type.normal.bitset2, 7);
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
      cmd.window.avision.bitset1 &= 0xC7; /* no filter */
      break;
    
    case AV_DITHERED:
      cmd.window.descriptor.bpc = 1;
      cmd.window.descriptor.image_comp = 1;
      cmd.window.avision.bitset1 &= 0xC7; /* no filter */
      break;
      
    case AV_GRAYSCALE:
      cmd.window.descriptor.bpc = 8;
      cmd.window.descriptor.image_comp = 2;
      cmd.window.avision.bitset1 &= 0xC7;  /* no filter */
      break;
      
    case AV_TRUECOLOR:
      cmd.window.descriptor.bpc = 8;
      cmd.window.descriptor.image_comp = 5;
      cmd.window.avision.bitset1 |= 0x20;  /* rgb one-pass filter */
      break;
      
    default:
      DBG (1, "Invalid mode. %d\n", s->c_mode);
      return SANE_STATUS_INVAL;
    }
  
  debug_print_window_descriptor (5, "set_window", &(cmd.window));
  
  DBG (3, "set_window: sending command. Bytes: %d\n", transferlen);
  status = sanei_scsi_cmd2 (s->fd, &cmd, sizeof (cmd.cmd),
			    &(cmd.window), transferlen, 0, 0);
  
  return status;
}

static SANE_Status
reserve_unit (Avision_Scanner* s)
{
  char cmd[] =
    {AVISION_SCSI_RESERVE_UNIT, 0, 0, 0, 0, 0};
  SANE_Status status;
  
  status = sanei_scsi_cmd (s->fd, cmd, sizeof (cmd), 0, 0);
  return status;
}

static SANE_Status
release_unit (Avision_Scanner* s)
{
  char cmd[] =
    {AVISION_SCSI_RELEASE_UNIT, 0, 0, 0, 0, 0};
  SANE_Status status;
  
  status = sanei_scsi_cmd (s->fd, cmd, sizeof (cmd), 0, 0);
  return status;
}

/* Check if a sheet is present. */
static SANE_Status
media_check (Avision_Scanner* s)
{
  char cmd[] = {AVISION_SCSI_MEDIA_CHECK, 0, 0, 0, 1, 0};
  SANE_Status status;
  char buf[1];
  size_t size = 1;
  
  status = sanei_scsi_cmd (s->fd, cmd, sizeof (cmd),
			   buf, &size);
  
  DBG (1, "media_check: %d\n", buf[0]);
  
  if (status == SANE_STATUS_GOOD) {
    if (!(buf[0] & 0x1))
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
  
  status = sanei_scsi_cmd (s->fd, cmd, sizeof(cmd), 0, 0);
  return status;
}

static SANE_Status
start_scan (Avision_Scanner* s)
{
  struct command_scan cmd;
    
  DBG (3, "start_scan:\n");
  
  memset (&cmd, 0, sizeof (cmd));
  cmd.opc = AVISION_SCSI_SCAN;
  cmd.transferlen = 0; /* r²: Was 1 */
  
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
  
  DBG (3, "start_scan: sending command:\n");
  return sanei_scsi_cmd (s->fd, &cmd, sizeof (cmd), 0, 0);
}

static SANE_Status
do_eof (Avision_Scanner *s)
{
  int exit_status;
  
  DBG (3, "do_eof:\n");
  
  if (s->pipe >= 0) {
    close (s->pipe);
    s->pipe = -1;
  }
  
  /* join our processes - without a wait() you will produce defunct
     childs */
  wait (&exit_status);

  s->reader_pid = 0;

  return SANE_STATUS_EOF;
}

static SANE_Status
do_cancel (Avision_Scanner* s)
{
  DBG (3, "do_cancel:\n");

  s->scanning = SANE_FALSE;
  
  /* do_eof (s); needed? */

  if (s->reader_pid > 0) {
    int exit_status;
    
    /* ensure child knows it's time to stop: */
    kill (s->reader_pid, SIGTERM);
    while (wait (&exit_status) != s->reader_pid);
    s->reader_pid = 0;
  }
  
  if (s->fd >= 0) {
    /* release the device */
    release_unit (s);
    
    sanei_scsi_close (s->fd);
    s->fd = -1;
  }
  
  return SANE_STATUS_CANCELLED;
}

static SANE_Status
read_data (Avision_Scanner* s, SANE_Byte* buf, size_t* count)
{
  struct command_read rcmd;
  SANE_Status status;

  DBG (9, "read_data: %d\n", *count);
  
  memset (&rcmd, 0, sizeof (rcmd));
  
  rcmd.opc = AVISION_SCSI_READ;
  rcmd.datatypecode = 0x00; /* read image data */
  set_double (rcmd.datatypequal, s->hw->data_dq);
  set_triple (rcmd.transferlen, *count);
  
  status = sanei_scsi_cmd (s->fd, &rcmd, sizeof (rcmd), buf, count);
  
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
  
  dev->x_range.max = SANE_FIX ( (int)dev->inquiry_x_ranges[s->o_mode]);
  dev->x_range.quant = 0;
  dev->y_range.max = SANE_FIX ( (int)dev->inquiry_y_ranges[s->o_mode]);
  dev->y_range.quant = 0;
  
  dev->dpi_range.min = 50;
  dev->dpi_range.quant = 10;
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
  s->opt[OPT_MODE].size = max_string_size (mode_list);
  s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_MODE].constraint.string_list = mode_list;
  s->val[OPT_MODE].s = strdup (mode_list[OPT_MODE_DEFAULT]);
  
  s->c_mode = make_mode (s->val[OPT_MODE].s);
  
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
  if (dev->hw->scanner_type == AV_SHEETFEED)
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

  /* transparency adapter */
  s->opt[OPT_TRANS].name = "transparency";
  s->opt[OPT_TRANS].title = "Enable transparency adapter";
  s->opt[OPT_TRANS].desc = "Switch transparency mode on. Which mainly " \
    "switches the scanner lamp off. (Hint: This can also be used to switch " \
    "the scanner lamp off when you don't use the scanner in the next time. " \
    "Simply enable this option and perform a preview.)";
  s->opt[OPT_TRANS].type = SANE_TYPE_BOOL;
  s->opt[OPT_TRANS].unit = SANE_UNIT_NONE;
  s->val[OPT_TRANS].w = SANE_FALSE;
  if (dev->hw->scanner_type == AV_SHEETFEED)
	  s->opt[OPT_TRANS].cap |= SANE_CAP_INACTIVE;
  
  /* ADF adapter */
  s->opt[OPT_ADF].name = "adf";
  s->opt[OPT_ADF].title = "Enable Automatic-Document-Feeder";
  s->opt[OPT_ADF].desc = "This option enables the " \
    "Automatic-Docuement-Feeder of the scanner. This allows you to " \
    "scan multiple pages manually or automatically.";
  s->opt[OPT_ADF].type = SANE_TYPE_BOOL;
  s->opt[OPT_ADF].unit = SANE_UNIT_NONE;
  s->val[OPT_ADF].w = SANE_FALSE;
  if (!dev->is_adf)
    s->opt[OPT_ADF].cap |= SANE_CAP_INACTIVE;
  
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
  if (disable_gamma_table)
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
  if (disable_gamma_table)
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
  if (disable_gamma_table)
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
  if (disable_gamma_table)
    s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GAMMA_VECTOR_B].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_B].size = 256 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR_B].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_B].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR_B].wa = &s->gamma_table[3][0];
  
  /* film holder control */
  if (dev->hw->scanner_type != AV_FILM || dev->holder_type == 0xff)
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

/* This function is executed as a child process.  The reason this is
   executed as a subprocess is because some (most?) generic SCSI
   interfaces block a SCSI request until it has completed.  With a
   subprocess, we can let it block waiting for the request to finish
   while the main process can go about to do more important things
   (such as recognizing when the user presses a cancel button).

   WARNING: Since this is executed as a subprocess, it's NOT possible
   to update any of the variables in the main process (in particular
   the scanner state cannot be updated).  */

static int
reader_process (Avision_Scanner* s, int fd)
{
  Avision_Device* dev = s->hw;
  
  SANE_Status status;
  sigset_t sigterm_set;
  FILE* fp;
  
  /* the complex params */
  size_t bytes_per_line;
  size_t lines_per_stripe;
  size_t lines_per_output;
  size_t max_bytes_per_read;
  size_t half_inch_bytes;
  
  /* the simple params for the data reader */
  size_t stripe_size;
  size_t stripe_fill;
  size_t out_size;
  
  size_t total_size;
  size_t processed_bytes;
  
  /* the fat strip we currently puzzle together to perform software-colorpack and more */
  u_int8_t* stripe_data;
  /* the corrected output data */
  u_int8_t* out_data;
  
  DBG (3, "reader_process:\n");
  
  sigemptyset (&sigterm_set);
  sigaddset (&sigterm_set, SIGTERM);
  
  fp = fdopen (fd, "w");
  if (!fp)
    return 1;
  
  bytes_per_line = s->params.bytes_per_line;
  lines_per_stripe = s->avdimen.line_difference * 2;
  if (lines_per_stripe == 0)
    lines_per_stripe = 8;
  
  lines_per_output = lines_per_stripe - s->avdimen.line_difference;
  
  /* the "/2" might makes scans faster - because it should leave some space
     in the SCSI buffers (scanner, kernel, ...) empty to read/send _ahead_ ... */
  max_bytes_per_read = dev->scsi_buffer_size / 2;
  
  /* perform some nice max 1/2 inch computation to make scan-previews look nicer */
  half_inch_bytes = s->params.bytes_per_line * s->avdimen.yres / 2;
  if (max_bytes_per_read > half_inch_bytes)
    max_bytes_per_read = half_inch_bytes;
  
  stripe_size = bytes_per_line * lines_per_stripe;
  out_size = bytes_per_line * lines_per_output;
  
  DBG (3, "dev->scsi_buffer_size / 2: %d, half_inch_bytes: %d\n",
       dev->scsi_buffer_size / 2, half_inch_bytes);
  
  DBG (3, "bytes_per_line: %d, lines_per_stripe: %d, lines_per_output: %d\n",
       bytes_per_line, lines_per_stripe, lines_per_output);
  
  DBG (3, "max_bytes_per_read: %d, stripe_size: %d, out_size: %d\n",
       max_bytes_per_read, stripe_size, out_size);
  
  stripe_data = malloc (stripe_size);
  out_data = malloc (out_size);
  
  s->line = 0;
  
  /* calculate params for the simple reader */
  total_size = bytes_per_line * (s->params.lines + s->avdimen.line_difference);
  DBG (3, "reader_process: total_size: %d\n", total_size);
  
  processed_bytes = 0;
  stripe_fill = 0;
  
  /* data read loop until all lines are processed */
  while (s->line < s->params.lines)
    {
      int usefull_bytes;
      
      /* fill the stripe buffer */
      while (processed_bytes < total_size && stripe_fill < stripe_size)
	{
	  size_t this_read = stripe_size - stripe_fill;
	  
	  /* limit reads to max_read and global data boundaries */
	  if (this_read > max_bytes_per_read)
	    this_read = max_bytes_per_read;
	  
	  if (processed_bytes + this_read > total_size)
	    this_read = total_size - processed_bytes;
	  
	  DBG (5, "reader_process: processed_bytes: %d, total_size: %d\n",
	       processed_bytes, total_size);
	  
	  DBG (5, "reader_process: this_read: %d\n",
	       this_read);
	  
	  sigprocmask (SIG_BLOCK, &sigterm_set, 0);
	  status = read_data (s, stripe_data + stripe_fill, &this_read);
	  sigprocmask (SIG_UNBLOCK, &sigterm_set, 0);
	  
	  if (status != SANE_STATUS_GOOD) {
	    DBG (1, "reader_process: read_data failed with status: %d\n", status);
	    return 3;
	  }
	  
	  if (this_read == 0) {
	    DBG (1, "reader_process: read_data failed due to lenght zero (EOF)\n");
	    return 4;
	  }
	  
	  stripe_fill += this_read;
	  processed_bytes += this_read;
	}
      
      DBG (5, "reader_process: stripe filled\n");
      
      /* perform output convertions */
      
      usefull_bytes = stripe_fill;
      if (s->c_mode == AV_TRUECOLOR)
	usefull_bytes -= s->avdimen.line_difference * bytes_per_line;
      
      DBG (5, "reader_process: usefull_bytes %i\n", usefull_bytes);
      
      /*
       * Perform needed data convertions (packing, ...) and/or copy the image data.
       */
      
      if (s->c_mode != AV_TRUECOLOR) /* simple copy */
	{
	  memcpy (out_data, stripe_data, usefull_bytes);
	}
      else /* AV_TRUECOLOR */
	{
	  /* WARNING: DO NOT MODIFY MY (HOPEFULLY WELL) OPTMISED
	     ALGORITHMS BELOW, WITHOUT UNDERSTANDING THEM FULLY ! */
	  if (s->avdimen.line_difference > 0) /* color-pack */
	    {
	      int i;
	      int c_offset = (s->avdimen.line_difference / 3) * bytes_per_line;
	      
	      u_int8_t* r_ptr = stripe_data;
	      u_int8_t* g_ptr = stripe_data + c_offset + 1;
	      u_int8_t* b_ptr = stripe_data + 2 * c_offset + 2;
	      
	      for (i = 0; i < usefull_bytes;) {
		out_data [i++] = *r_ptr; r_ptr += 3;
		out_data [i++] = *g_ptr; g_ptr += 3;
		out_data [i++] = *b_ptr; b_ptr += 3;
	      }
	    } /* end color pack */
	  else if (dev->inquiry_needs_line_pack) /* line-pack */
	    { 
	      int i = 0;
	      int l, p;
	      int line_pixels = bytes_per_line / 3;
	      int lines = usefull_bytes / bytes_per_line;
	      
	      for (l = 0; l < lines; ++ l)
		{
		  u_int8_t* r_ptr = stripe_data + (bytes_per_line * l);
		  u_int8_t* g_ptr = r_ptr + line_pixels;
		  u_int8_t* b_ptr = g_ptr + line_pixels;
		  
		  for (p = 0; p < line_pixels; ++ p) {
		    out_data [i++] = *(r_ptr++);
		    out_data [i++] = *(g_ptr++);
		    out_data [i++] = *(b_ptr++);
		  }
		}
	    } /* end line pack */
	  
	  /* TODO: add mirror image support ... */
	  
	  else /* else no packing was required -> simple copy */
	    { 
	      memcpy (out_data, stripe_data, usefull_bytes);
	    }
	} /* end if AV_TRUECOLOR */
      
      /* I know that the next lines are broken if not a multiple of
	 bytes_per_line are in the buffer. Shouldn't happen. */
      fwrite (out_data, bytes_per_line, usefull_bytes / bytes_per_line, fp);
      
      /* save image date in stripe buffer for next next stripe */
      stripe_fill -= usefull_bytes;
      if (stripe_fill > 0)
	memcpy (stripe_data, stripe_data + usefull_bytes, stripe_fill);
      
      s->line += usefull_bytes / bytes_per_line;
      DBG (3, "reader_process: end loop\n");
    } /* end while not all lines */
  
  /* Eject film-holder */
  if (dev->inquiry_new_protocol &&
      (dev->hw->scanner_type == AV_FILM || dev->hw->scanner_type == AV_SHEETFEED))
    {
    status = object_position (s, AVISION_SCSI_OP_GO_HOME);
    if (status != SANE_STATUS_GOOD)
      return status;
  }
  
  fclose (fp);
  
  free (stripe_data);
  free (out_data);
  
  status = release_unit (s);
  if (status != SANE_STATUS_GOOD)
    DBG (1, "reader_process: release_unit failed\n");
  
  return 0;
}

static SANE_Status
attach_one (const char* dev)
{
  attach (dev, 0);
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
  
  authorize = authorize; /* silence gcc */
  
  DBG (3, "sane_init:\n");
  
  DBG_INIT();
  
  if (version_code)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, BACKEND_BUILD);
  
  fp = sanei_config_open (AVISION_CONFIG_FILE);
  if (!fp) {
    /* default to /dev/scanner instead of insisting on config file */
    attach ("/dev/scanner", 0);
    return SANE_STATUS_GOOD;
  }
  
  while (sanei_config_read  (line, sizeof (line), fp))
    {
      word = NULL;
      ++ linenumber;
      
      DBG(5, "sane_init: parsing config line \"%s\"\n",
	  line);
      
      cp = sanei_config_get_string (line, &word);
      
      if (!word || cp == line)
	{
	  DBG(5, "sane_init: config file line %d: ignoring empty line\n",
	      linenumber);
	  free (word);
	  word = NULL;
	  continue;
	}
      if (word[0] == '#')
	{
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
	  
	  if (strcmp (word, "disable-gamma-table") == 0)
	    {
	      DBG(3, "sane_init: config file line %d: disable-gamma-table\n",
		      linenumber);
	      disable_gamma_table = SANE_TRUE;
	    }
	  if (strcmp (word, "disable-calibration") == 0)
	    {
	      DBG(3, "sane_init: config file line %d: disable-calibration\n",
		      linenumber);
	      disable_calibration = SANE_TRUE;
	    }
	  if (strcmp (word, "old-calibration") == 0)
	    {
	      DBG(3, "sane_init: config file line %d: old-calibration\n",
		      linenumber);
	      old_calibration = SANE_TRUE;
	    }
	  if (strcmp (word, "one-calib-only") == 0)
	    {
	      DBG(3, "sane_init: config file line %d: one-calib-only\n",
		      linenumber);
	      one_calib_only = SANE_TRUE;
	    }
	  if (strcmp (word, "force-a4") == 0)
	    {
	      DBG(3, "sane_init: config file line %d: enabling force-a4\n",
		      linenumber);
	      force_a4 = SANE_TRUE;
	    }
	  if (strcmp (word, "disable-c5-guard") == 0)
	    {
	      DBG(3, "sane_init: config file line %d: disabling c5-guard\n",
		      linenumber);
	      disable_c5_guard = SANE_TRUE;
	    }
	}
      else
	{
	  DBG(4, "sane_init: config file line %d: trying to attach `%s'\n",
	      linenumber, line);
	  
	  sanei_config_attach_matching_devices (line, attach_one);
	    free (word);
	  word = NULL;
	}
    }
  
  fclose (fp);
  if (word)
    free (word);
  
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
    free (dev->sane.name);
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

    if (!dev) {
      status = attach (devicename, &dev);
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
  
  /* initialize gamma table */
  memset (s, 0, sizeof (*s));
  s->fd = -1;
  s->pipe = -1;
  s->hw = dev;
  for (i = 0; i < 4; ++ i)
    for (j = 0; j < 256; ++ j)
      s->gamma_table[i][j] = j;
  
  /* the other states (like page, scanning, ...) rely on the
     memset (0) above */
  
  init_options (s);
  
  /* insert newly opened handle into list of open handles: */
  s->next = first_handle;
  first_handle = s;
  
  *handle = s;
  
  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  Avision_Scanner* prev;
  Avision_Scanner* s;
  int i;

  DBG (3, "sane_close:\n\n");
  
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
	case OPT_TRANS:
	case OPT_ADF:
	case OPT_FRAME:
	  
	  *(SANE_Word*) val = s->val[option].w;
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
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
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
	  s->val[option].w = *(SANE_Word*) val;
	  
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  
	  return SANE_STATUS_GOOD;
	  
	  /* options with side-effects - modifying the options: */
	case OPT_TRANS:
	case OPT_ADF:
	  
	  s->val[option].w = *(SANE_Word*) val;
	  
	  if (option == OPT_TRANS && *(SANE_Word*) val) {
	    s->o_mode = AV_TRANSPARENT;
	    s->val[OPT_ADF].w = SANE_FALSE;
	  }
	  else if (option == OPT_ADF && *(SANE_Word*) val) {
	    s->o_mode = AV_ADF;
	    s->val[OPT_TRANS].w = SANE_FALSE;
	  }
	  else
	    s->o_mode = AV_NORMAL;
	  
	  dev->x_range.max = SANE_FIX ( (int)dev->inquiry_x_ranges[s->o_mode]);
	  dev->y_range.max = SANE_FIX ( (int)dev->inquiry_y_ranges[s->o_mode]);
	  
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  
	  return SANE_STATUS_GOOD;
	  
	case OPT_MODE:
	  {
	    if (s->val[option].s)
	      free (s->val[option].s);
	    
	    s->val[option].s = strdup (val);
	    s->c_mode = make_mode (s->val[OPT_MODE].s);
	    
	    /* set to mode specific values */
	    
	    /* the gamma table related */
	    if (!disable_gamma_table)
	      {
		if (s->c_mode == AV_TRUECOLOR) {
		  s->opt[OPT_GAMMA_VECTOR].cap   &= ~SANE_CAP_INACTIVE;
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
  Avision_Device* dev = s->hw;
  
  DBG (3, "sane_get_parameters:\n");
  
  /* during an acutal scan the parameters are used and thus are not alllowed
     to change ... */
  if (!s->scanning)
    {
      int boundary = get_pixel_boundary (s);
      int gray_mode = ((s->c_mode == AV_TRUECOLOR) || (s->c_mode == AV_GRAYSCALE));
      
#ifdef THIS_IS_NEEDED
      /* Official Avision fix C5 lineart bug. ?!? */
      if ((!gray_mode) && (dev->inquiry_asic_type == AV_ASIC_C5))
	boundary = 1;
#endif
      
      DBG (3, "sane_get_parameters: boundary %d, gray_mode: %d\n",
	   boundary, gray_mode);
  
      /* TODO: Allow different x/y resolutions */
      s->avdimen.xres = s->val[OPT_RESOLUTION].w;
      s->avdimen.yres = s->val[OPT_RESOLUTION].w;
      
      DBG (3, "sane_get_parameters: tlx: %f, tly: %f, brx: %f, bry: %f\n",
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
      if (s->c_mode == AV_TRUECOLOR && dev->inquiry_needs_software_colorpack)
	{
	  if (dev->hw->feature_type & AV_NO_LINE_DIFFERENCE) {
	    DBG (1, "sane_get_parameters: Line difference ignored due to device-list!!\n");
	  }
	  else {
	    s->avdimen.line_difference
	      = (dev->inquiry_line_difference * s->avdimen.yres) /
	      dev->inquiry_optical_res;
	    
	    s->avdimen.line_difference -= s->avdimen.line_difference % 3;
	  }
	  
	  s->avdimen.bry += s->avdimen.line_difference;
	  
	  /* limit bry + line_difference to real scan boundary */
	  {
	    long y_max = dev->inquiry_y_ranges[s->o_mode] * s->avdimen.yres / MM_PER_INCH;
	    DBG (3, "sane_get_parameters: y_max: %ld, bry: %ld, line_difference: %d\n",
		 y_max, s->avdimen.bry, s->avdimen.line_difference);
	    
	    if (s->avdimen.bry + s->avdimen.line_difference > y_max) {
	      DBG (1, "sane_get_parameters: bry limitted!\n");
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
      
      debug_print_avdimen (1, "sane_get_parameters", &s->avdimen);
      
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
	case AV_TRUECOLOR:
	  s->params.format = SANE_FRAME_RGB;
	  s->params.bytes_per_line = s->params.pixels_per_line * 3;
	  s->params.depth = 8;
	  break;
	default:
	  DBG (1, "Invalid mode. %d\n", s->c_mode);
	  return SANE_STATUS_INVAL;
	} /* end switch */
      
      s->params.last_frame = SANE_TRUE;
      
      debug_print_params (1, "sane_get_parameters", &s->params);
      
#ifdef THIS_IS_NEEDED
      /* C5 implements hardware scaling.
	 So modify s->params and s->avdimen. */
      
      if (!disable_c5_guard && dev->inquiry_asic_type == AV_ASIC_C5) {
	c5_guard (s);
      }
#endif
    } /* end if not scanning */
  
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
  
  DBG (3, "sane_start: page: %d\n", s->page);
  
  /* First make sure there is no scan running!!! */
  
  if (s->scanning)
    return SANE_STATUS_DEVICE_BUSY;
  
  /* Second make sure we have a current parameter set. Some of the
     parameters will be overwritten below, but that's OK. */
  status = sane_get_parameters (s, &s->params);
  
  if (status != SANE_STATUS_GOOD) {
    return status;
  }
  
  if (s->fd < 0) {
#ifdef HAVE_SANEI_SCSI_OPEN_EXTENDED
    DBG (1, "sane_start: using open_extended\n");
    status = sanei_scsi_open_extended (s->hw->sane.name, &s->fd, sense_handler, 0,
				       &(dev->scsi_buffer_size));
#else
    status = sanei_scsi_open (s->hw->sane.name, &s->fd, sense_handler, 0);
#endif
    if (status != SANE_STATUS_GOOD) {
      DBG (1, "sane_start: open of %s failed: %s\n",
	   s->hw->sane.name, sane_strstatus (status));
      return status;
    }
    DBG (1, "sane_start: got %d scsi_max_reqest_size\n", dev->scsi_buffer_size);
  }
  
  /* first reserve unit */
  status = reserve_unit (s);
  if (status != SANE_STATUS_GOOD)
    DBG (1, "sane_start: reserve_unit failed\n");
  
  status = wait_ready (s->fd);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "sane_start: wait_ready() failed: %s\n", sane_strstatus (status));
    goto stop_scanner_and_return;
  }

  
  /* auto-feed next paper for ADF scanners */
#ifdef NEEDED
  if (dev->inquiry_adf /*&& s->page > 0*/) {
    DBG (1, "sane_start: ADF next paper.\n");
    
    status = object_position (s, AVISION_SCSI_OP_LOAD_PAPER);
    if (status != SANE_STATUS_GOOD)
      DBG(1, "sane_start: load paper failed: %s\n",
	  sane_strstatus (status));
    
    status = object_position (s, AVISION_SCSI_OP_REJECT_PAPER);
    if (status != SANE_STATUS_GOOD)
      DBG(1, "sane_start: reject paper failed: %s\n",
	  sane_strstatus (status));
  }
#endif
  
  /* Check for paper in sheetfeed scanners and for ADF scans */
  if ( (dev->hw->scanner_type == AV_FLATBED && s->val[OPT_ADF].w) ||
       dev->hw->scanner_type == AV_SHEETFEED) {
    status = media_check (s);
    if (status != SANE_STATUS_GOOD) {
      DBG (1, "sane_start: media_check failed: %s\n",
	   sane_strstatus (status));
      goto stop_scanner_and_return;
    }
  }
  
  if (dev->inquiry_new_protocol) {
    wait_4_light (s);
  }
  
  status = set_window (s);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "sane_start: set scan window command failed: %s\n",
	 sane_strstatus (status));
    goto stop_scanner_and_return;
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
  
  /* Only perform the calibration for newer scanners - it is not needed
     for my Avision AV 630 - and also does not even work ... */
  /* needs_calibration is not used for debugging purposes ...  */
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
  
  if (one_calib_only && dev->is_calibrated) {
    DBG (1, "sane_start: already calibrated - skipped\n");
    goto calib_end;
  }
  
  if (s->val[OPT_ADF].w) {
    DBG (1, "sane_start: ADF -> calib skipped\n");
    goto calib_end;
  }

  if (old_calibration)
    status = old_r_calibration (s);
  else if (dev->inquiry_asic_type < AV_ASIC_C5 || disable_c5_guard)
    status = normal_calibration (s);
  else
    status = c5_calibration (s);
  
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "sane_start: perform calibration failed: %s\n",
	 sane_strstatus (status));
    goto stop_scanner_and_return;
  }
  
  dev->is_calibrated = SANE_TRUE;
  
 calib_end:
  
  /* check whether gamma-taable is disabled by the user? */
  if (disable_gamma_table) {
    DBG (1, "sane_start: gamma-table disabled in config - skipped!\n");
    goto gamma_end;
  }
  
  /* gamma-table allowed for this scanner? */
  if (dev->hw->feature_type & AV_NO_GAMMA) {
    DBG (1, "sane_start: gamma-table disabled in device list!!\n");
    goto gamma_end;
  }
  
  status = set_gamma (s);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "sane_start: set gamma failed: %s\n",
	 sane_strstatus (status));
    goto stop_scanner_and_return;
  }
  
 gamma_end:
  
  /* check film holder */
  if (dev->hw->scanner_type == AV_FILM && dev->holder_type == 0xff) {
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
  
  status = start_scan (s);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "sane_start: send start scan faild: %s\n",
	 sane_strstatus (status));
    goto stop_scanner_and_return;
  }
  
  s->scanning = SANE_TRUE;
  s->line = 0;
  
  if (pipe (fds) < 0) {
    return SANE_STATUS_IO_ERROR;
  }
  
  s->reader_pid = fork ();
  if (s->reader_pid == 0)  { /* if child */
    sigset_t ignore_set;
    struct SIGACTION act;
    
    close (fds [0] );
    
    sigfillset (&ignore_set);
    sigdelset (&ignore_set, SIGTERM);
    sigprocmask (SIG_SETMASK, &ignore_set, 0);
    
    memset (&act, 0, sizeof (act));
    sigaction (SIGTERM, &act, 0);
    
    /* don't use exit() since that would run the atexit() handlers... */
    _exit (reader_process (s, fds[1] ) );
  }
  
  close (fds [1] );
  s->pipe = fds [0];
  
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

  DBG (3, "sane_read: max_len: %d\n", max_len);

  *len = 0;

  nread = read (s->pipe, buf, max_len);
  DBG (3, "sane_read: got %d bytes\n", nread);

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
    ++ s->page;
    return do_eof (s);
  }
  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  Avision_Scanner* s = handle;
  
  DBG (3, "sane_cancel\n");
  
  if (s->scanning)
    do_cancel (s);
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  Avision_Scanner* s = handle;
  
  DBG (3, "sane_set_io_mode:\n");
  if (!s->scanning)
    return SANE_STATUS_INVAL;
  
  if (fcntl (s->pipe, F_SETFL, non_blocking ? O_NONBLOCK : 0) < 0)
    return SANE_STATUS_IO_ERROR;
  
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int* fd)
{
  Avision_Scanner* s = handle;
  
  DBG (3, "sane_get_select_fd:\n");
  
  if (!s->scanning)
    return SANE_STATUS_INVAL;
  
  *fd = s->pipe;
  return SANE_STATUS_GOOD;
}
