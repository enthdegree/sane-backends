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
   AV 630 / 620 (CS) ...) and some Avision (OEM) USB scanners (like the HP 5300,
   7400, Minolta FS-V1 ...) with the AVISION SCSI-2 command set.
   
   Copyright 1999, 2000, 2001, 2002 by
                "René Rebe" <rene.rebe@gmx.net>
                "Meino Christian Cramer" <mccramer@s.netic.de>
                "Jose Paulo Moitinho de Almeida" <moitinho@civil.ist.utl.pt>
      
   Additional Contributers:
                "Gunter Wagner"
                  (some fixes and the transparency option)
                "Martin Jelínek" <mates@sirrah.troja.mff.cuni.cz>
                   nice attach debug output
                "Marcin Siennicki" <m.siennicki@cloos.pl>
                   found some typos
                "Frank Zago" <fzago@greshamstorage.com>
                   Mitsubishi IDs and report
   
   Very much thanks to:
                Oliver Neukum who sponsored a HP 5300 USB scanner !!! ;-)
                Avision INC for the documentation we got! ;-)
   
   ChangeLog:
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
#define BACKEND_BUILD 27  /* avision backend BUILD version */

static Avision_HWEntry Avision_Device_List  [] =
  {
    /* All Avision except 630, 620 and 6240 untested ... */
    
    { "AVISION", "AV100CS", 
      "Avision", "AV100CS",
      AV_SCSI, AV_SHEETFEED},
    
    { "AVISION", "AV100IIICS", 
      "Avision", "AV100IIICS",
      AV_SCSI, AV_FLATBED},
    
    { "AVISION", "AV100S", 
      "Avision", "AV100S",
      AV_SCSI, AV_FLATBED},
    
    { "AVISION", "AV240SC", 
      "Avision", "AV240SC",
      AV_SCSI, AV_FLATBED},
    
    { "AVISION", "AV260CS", 
      "Avision", "AV260CS",
      AV_SCSI, AV_FLATBED},
    
    { "AVISION", "AV360CS", 
      "Avision", "AV360CS",
      AV_SCSI, AV_FLATBED},
    
    { "AVISION", "AV363CS", 
      "Avision", "AV363CS",
      AV_SCSI, AV_FLATBED},
    
    { "AVISION", "AV420CS", 
      "Avision", "AV420CS",
      AV_SCSI, AV_FLATBED},
    
    { "AVISION", "AV6120", 
      "Avision", "AV6120",
      AV_SCSI, AV_FLATBED},
    
    { "AVISION", "AV620CS",
      "Avision", "AV620CS",
      AV_SCSI, AV_FLATBED},
    
    { "AVISION", "AV630CS",
      "Avision", "AV630CS",
      AV_SCSI, AV_FLATBED },
    
    { "AVISION", "AV630CSL",
      "Avision", "AV630CSL",
      AV_SCSI, AV_FLATBED },
    
    { "AVISION", "AV6240",
      "Avision", "AV6240",
      AV_SCSI, AV_FLATBED},
    
    { "AVISION", "AV660S",
      "Avision", "AV660S",
      AV_SCSI, AV_FLATBED},
    
    { "AVISION", "AV680S",
      "Avision", "AV680S",
      AV_SCSI, AV_FLATBED},
    
    { "AVISION", "AV800S",
      "Avision", "AV800S",
      AV_SCSI, AV_FLATBED},
    
    { "AVISION", "AV810C",
      "Avision", "AV810C",
      AV_SCSI, AV_FLATBED},
    
    { "AVISION", "AV820",
      "Avision", "AV820",
      AV_SCSI, AV_FLATBED},
    
    { "AVISION", "AV820C",
      "Avision", "AV820C",
      AV_SCSI, AV_FLATBED},
    
    { "AVISION", "AV880",
      "Avision", "AV880",
      AV_SCSI, AV_FLATBED},
    
    { "AVISION", "AV880C",
      "Avision", "AV880C",
      AV_SCSI, AV_FLATBED},
    
    { "AVISION", "AVA3",
      "Avision", "AVA3",
      AV_SCSI, AV_FLATBED},
    
    /* and possibly more avisions */
    
    { "HP",      "ScanJet 5300C",
      "Hewlett-Packard", "ScanJet 5300C",
      AV_USB, AV_FLATBED},
    
    { "HP",      "ScanJet 5370C",
      "Hewlett-Packard", "ScanJet 5370C",
      AV_USB, AV_FLATBED},
    
    { "hp",      "scanjet 7400c",
      "Hewlett-Packard", "ScanJet 7400c",
      AV_USB, AV_FLATBED},
    
    /* needs firmware and possibly more adaptions */
    /*{"UMAX",    "Astra 4500",    AV_USB, AV_FLATBED},
      {"UMAX",    "Astra 6700",    AV_USB, AV_FLATBED}, */

    { "MINOLTA", "FS-V1",
      "Minolta", "FS-V1",
      AV_USB, AV_FILM},
    
    /* possibly all Minolta film-scanners ? */
    
    /* Only the SS600 is tested ... */
    
    { "MITSBISH", "MCA-ADFC",
      "Mitsubishi", "MCA-ADFC",
      AV_SCSI, AV_SHEETFEED},
    
    { "MITSBISH", "MCA-S1200C",
      "Mitsubishi", "S1200C", 
      AV_SCSI, AV_SHEETFEED},
    
    { "MITSBISH", "MCA-S600C",
      "Mitsubishi", "S600C",
      AV_SCSI, AV_SHEETFEED},
    
    { "MITSBISH", "SS600",
      "Mitsubishi", "SS600", 
      AV_SCSI, AV_SHEETFEED},
    
    /* The next are all untested ... */
    
    { "FCPA", "ScanPartner",
      "Fujitsu", "ScanPartner",
      AV_SCSI, AV_SHEETFEED},

    { "FCPA", "ScanPartner 10",
      "Fujitsu", "ScanPartner 10",
      AV_SCSI, AV_SHEETFEED},
    
    { "FCPA", "ScanPartner 10C",
      "Fujitsu", "ScanPartner 10C",
      AV_SCSI, AV_SHEETFEED},
    
    { "FCPA", "ScanPartner 15C",
      "Fujitsu", "ScanPartner 15C",
      AV_SCSI, AV_SHEETFEED},
    
    { "FCPA", "ScanPartner 300C",
      "Fujitsu", "ScanPartner 300C",
      AV_SCSI, AV_SHEETFEED},
    
    { "FCPA", "ScanPartner 600C",
      "Fujitsu", "ScanPartner 600C",
      AV_SCSI, AV_SHEETFEED},
    
    { "FCPA", "ScanPartner Jr",
      "Fujitsu", "ScanPartner Jr",
      AV_SCSI, AV_SHEETFEED},
    
    { "FCPA", "ScanStation",
      "Fujitsu", "ScanStation",
      AV_SCSI, AV_SHEETFEED},
    
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
      0, 0} 
  };

/* used when scanner returns invalid range fields ... */
#define A4_X_RANGE 8.5
#define A4_Y_RANGE 11.8
#define SHEETFEED_Y_RANGE 14.0

#ifndef PATH_MAX
# define PATH_MAX 1024
#endif

#define AVISION_CONFIG_FILE "avision.conf"

#define MM_PER_INCH (25.4)

static int num_devices;
static Avision_Device* first_dev;
static Avision_Scanner* first_handle;
static const SANE_Device** devlist = 0;

static SANE_Bool disable_gamma_table = SANE_FALSE; /* disable the usage of a custom gamma-table */
static SANE_Bool disable_calibration = SANE_FALSE; /* disable the calibration */
static SANE_Bool force_a4 = SANE_FALSE; /* force scanable areas to ISO(DIN) A4 */

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

static int make_mode (char* mode)
{
  DBG (3, "make_mode\n");
  
  if (strcmp (mode, "Line Art") == 0)
    return THRESHOLDED;
  if (strcmp (mode, "Dithered") == 0)
    return DITHERED;
  else if (strcmp (mode, "Gray") == 0)
    return GREYSCALE;
  else if (strcmp (mode, "Color") == 0)
    return TRUECOLOR;
  
  return -1;
}

static SANE_Status
sense_handler (int fd, u_char* sense, void* arg)
{
  int i;
  
  SANE_Status status;
  SANE_Bool ASC_switch = SANE_FALSE;

  fd = fd; /* silence gcc */
  arg = arg; /* silence gcc */

  DBG (3, "sense_handler\n");
  
  switch (sense[0])
    {
    case 0x00:
      status = SANE_STATUS_GOOD;
      break;
    default:
      DBG (1, "sense_handler: got unknown sense code %02x\n", sense[0]);
      status = SANE_STATUS_IO_ERROR;
    }
  
  for (i = 0; i < 21; ++ i)
    {
      DBG (1, "%d:[%x]\n", i, sense[i]);
    }
  
  if (sense[VALID_BYTE] & VALID)
    {
      switch (sense[ERRCODE_BYTE] & ERRCODEMASK)
	{
	case ERRCODESTND:
	  {
	    DBG (5, "SENSE: STANDARD ERROR CODE\n");
	    break;
	  }
	case ERRCODEAV:
	  {
	    DBG (5, "SENSE: AVISION SPECIFIC ERROR CODE\n");
	    break;
	  }
	}
      
      switch (sense[SENSEKEY_BYTE] & SENSEKEY_MASK)
	{
	case NOSENSE          :
	  {
	    DBG (5, "SENSE: NO SENSE\n");
	    break;
	  }
	case NOTREADY         :
	  {
	    DBG (5, "SENSE: NOT READY\n");
	    break;
	  }
	case MEDIUMERROR      :
	  {
	    DBG (5, "SENSE: MEDIUM ERROR\n");
	    break;
	  }
	case HARDWAREERROR    :
	  {
	    DBG (5, "SENSE: HARDWARE FAILURE\n");
	    break;
	  }
	case ILLEGALREQUEST   :
	  {
	    DBG (5, "SENSE: ILLEGAL REQUEST\n");
	    ASC_switch = SANE_TRUE;                   
	    break;
	  }
	case UNIT_ATTENTION   :
	  {
	    DBG (5, "SENSE: UNIT ATTENTION\n");
	    break;
	  }
	case VENDORSPEC       :
	  {
	    DBG (5, "SENSE: VENDOR SPECIFIC\n");
	    break;
	  }
	case ABORTEDCOMMAND   :
	  {
	    DBG (5, "SENSE: COMMAND ABORTED\n");
	    break;
	  }
	}
      
      if (sense[EOS_BYTE] & EOSMASK)
	{
	  DBG (5, "SENSE: END OF SCAN\n");
	}
      else
	{
	  DBG (5, "SENSE: SCAN HAS NOT YET BEEN COMPLETED\n");
	}

      if (sense[ILI_BYTE] & INVALIDLOGICLEN)
	{
	  DBG (5, "SENSE: INVALID LOGICAL LENGTH\n");
	}
      
      if ((sense[ASC_BYTE] != 0) && (sense[ASCQ_BYTE] != 0))
	{
	  if (sense[ASC_BYTE] == ASCFILTERPOSERR)
	    {
	      DBG (5, "X\n");
	    }

	  if (sense[ASCQ_BYTE] == ASCQFILTERPOSERR)
	    {
	      DBG (5, "X\n");
	    }

	  if (sense[ASCQ_BYTE] == ASCQFILTERPOSERR)
	    {
	      DBG (5, "SENSE: FILTER POSITIONING ERROR\n");
	    }

	  if ((sense[ASC_BYTE] == ASCFILTERPOSERR) && 
	      (sense[ASCQ_BYTE] == ASCQFILTERPOSERR))
	    {
	      DBG (5, "SENSE: FILTER POSITIONING ERROR\n");
	    }

	  if ((sense[ASC_BYTE] == ASCADFPAPERJAM) && 
	      (sense[ASCQ_BYTE] == ASCQADFPAPERJAM ))
	    {
	      DBG (5, "ADF Paper Jam\n");
	    }
	  if ((sense[ASC_BYTE] == ASCADFCOVEROPEN) &&
	      (sense[ASCQ_BYTE] == ASCQADFCOVEROPEN))
	    {
	      DBG (5, "ADF Cover Open\n");
	    }
	  if ((sense[ASC_BYTE] == ASCADFPAPERCHUTEEMPTY) &&
	      (sense[ASCQ_BYTE] == ASCQADFPAPERCHUTEEMPTY))
	    {
	      DBG (5, "ADF Paper Chute Empty\n");
	    }
	  if ((sense[ASC_BYTE] == ASCINTERNALTARGETFAIL) &&
	      (sense[ASCQ_BYTE] == ASCQINTERNALTARGETFAIL))
	    {
	      DBG (5, "Internal Target Failure\n");
	    }
	  if ((sense[ASC_BYTE] == ASCSCSIPARITYERROR) &&
	      (sense[ASCQ_BYTE] == ASCQSCSIPARITYERROR))
	    {
	      DBG (5, "SCSI Parity Error\n");
	    }
	  if ((sense[ASC_BYTE] == ASCINVALIDCOMMANDOPCODE) &&
	      (sense[ASCQ_BYTE] == ASCQINVALIDCOMMANDOPCODE))
	    {
	      DBG (5, "Invalid Command Operation Code\n");
	    }
	  if ((sense[ASC_BYTE] == ASCINVALIDFIELDCDB) &&
	      (sense[ASCQ_BYTE] == ASCQINVALIDFIELDCDB))
	    {
	      DBG (5, "Invalid Field in CDB\n");
	    }
	  if ((sense[ASC_BYTE] == ASCLUNNOTSUPPORTED) &&
	      (sense[ASCQ_BYTE] == ASCQLUNNOTSUPPORTED))
	    {
	      DBG (5, "Logical Unit Not Supported\n");
	    }
	  if ((sense[ASC_BYTE] == ASCINVALIDFIELDPARMLIST) &&
	      (sense[ASCQ_BYTE] == ASCQINVALIDFIELDPARMLIST))
	    {
	      DBG (5, "Invalid Field in parameter List\n");
	    }
	  if ((sense[ASC_BYTE] == ASCINVALIDCOMBINATIONWIN) &&
	      (sense[ASCQ_BYTE] == ASCQINVALIDCOMBINATIONWIN))
	    {
	      DBG (5, "Invalid Combination of Window Specified\n");
	    }
	  if ((sense[ASC_BYTE] == ASCMSGERROR) &&
	      (sense[ASCQ_BYTE] == ASCQMSGERROR))
	    {
	      DBG (5, "Message Error\n");
	    }
	  if ((sense[ASC_BYTE] == ASCCOMMCLREDANOTHINITIATOR) &&
	      (sense[ASCQ_BYTE] == ASCQCOMMCLREDANOTHINITIATOR))
	    {
	      DBG (5, "Command Cleared By Another Initiator.\n");
	    }
	  if ((sense[ASC_BYTE] == ASCIOPROCTERMINATED) &&
	      (sense[ASCQ_BYTE] == ASCQIOPROCTERMINATED))
	    {
	      DBG (5, "I/O process Terminated.\n");
	    }
	  if ((sense[ASC_BYTE] == ASCINVBITIDMSG) &&
	      (sense[ASCQ_BYTE] == ASCQINVBITIDMSG))
	    {
	      DBG (5, "Invalid Bit in Identify Message\n");
	    }
	  if ((sense[ASC_BYTE] == ASCINVMSGERROR) &&
	      (sense[ASCQ_BYTE] == ASCQINVMSGERROR))
	    {
	      DBG (5, "Invalid Message Error\n");
	    }
	  if ((sense[ASC_BYTE] == ASCLAMPFAILURE) &&
	      (sense[ASCQ_BYTE] == ASCQLAMPFAILURE))
	    {
	      DBG (5, "Lamp Failure\n");
	    }
	  if ((sense[ASC_BYTE] == ASCMECHPOSERROR) &&
	      (sense[ASCQ_BYTE] == ASCQMECHPOSERROR))
	    {
	      DBG (5, "Mechanical Positioning Error\n");
	    }
	  if ((sense[ASC_BYTE] == ASCPARAMLISTLENERROR) &&
	      (sense[ASCQ_BYTE] == ASCQPARAMLISTLENERROR))
	    {
	      DBG (5, "Parameter List Length Error\n");
	    }
	  if ((sense[ASC_BYTE] == ASCPARAMNOTSUPPORTED) &&
	      (sense[ASCQ_BYTE] == ASCQPARAMNOTSUPPORTED))
	    {
	      DBG (5, "Parameter Not Supported\n");
	    }
	  if ((sense[ASC_BYTE]  == ASCPARAMVALINVALID  ) &&
	      (sense[ASCQ_BYTE] == ASCQPARAMVALINVALID )  )
	    {
	      DBG (5, "Parameter Value Invalid\n");
	    }
	  if ((sense[ASC_BYTE]  == ASCPOWERONRESET) &&
	      (sense[ASCQ_BYTE] == ASCQPOWERONRESET)) 
	    {
	      DBG (5, "Power-on, Reset, or Bus Device Reset Occurred\n");
	    }
	  if ((sense[ASC_BYTE] == ASCSCANHEADPOSERROR) &&
	      (sense[ASCQ_BYTE] == ASCQSCANHEADPOSERROR))
	    {
	      DBG (5, "SENSE: FILTER POSITIONING ERROR\n");
	    }
	}
      else
	{
	  DBG (5, "No Additional Sense Information\n");
	}
      
      if (ASC_switch == SANE_TRUE)
	{
	  if (sense[SKSV_BYTE] & SKSVMASK)
	    {
	      if (sense[CD_BYTE] & CDMASK)
		{
		  DBG (5, "SENSE: ERROR IN COMMAND PARAMETER...\n");
		}
	      else
		{
		  DBG (5, "SENSE: ERROR IN DATA PARAMETER...\n");
		}
	      
	      if (sense[BPV_BYTE] & BPVMASK)
		{
		  DBG (5, "BIT %d ERRORNOUS OF\n", (int)sense[BITPOINTER_BYTE] & BITPOINTERMASK);
		}
                    
	      DBG (5, "ERRORNOUS BYTE %d \n", (int)sense[BYTEPOINTER_BYTE1] );
                    
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
wait_4_light (Avision_Scanner *s)
{
  /* read stuff */
  struct command_read rcmd;
  char* light_status[] = { "off", "on", "warming up", "needs warm up test", 
		           "light check error", "RESERVED" };
  
  SANE_Status status;
  u_int8_t result;
  int try;
  unsigned int size = 1;
  
  DBG (3, "getting light status.\n");
  
  memset (&rcmd, 0, sizeof (rcmd));
  
  rcmd.opc = AVISION_SCSI_READ;
  rcmd.datatypecode = 0xa0; /* get light status */
  rcmd.datatypequal [0] = 0x0d;
  rcmd.datatypequal [1] = 0x0a;
  set_triple (rcmd.transferlen, size);
  
  DBG (5, "read_data: bytes %d\n", size);
  
  for (try = 0; try < 10; ++ try) {
    status = sanei_scsi_cmd (s->fd, &rcmd, sizeof (rcmd), &result, &size);
    
    if (status != SANE_STATUS_GOOD || size != sizeof (result)) {
      DBG (1, "wait_4_light: read failed (%s)\n", sane_strstatus (status));
      return status;
    }
    
    DBG (3, "Light status: command is %d. Result is %s\n",
	 status, light_status[(result>4)?5:result]);
    
    if (result == 1) {
      return SANE_STATUS_GOOD;
    }
    
    sleep (1);
  }
  
  DBG (1, "wait_4_light: timed out after %d attempts\n", try);
  
  return SANE_STATUS_GOOD;
}

static size_t
max_string_size (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  int i;

  DBG (3, "max_string_size\n");

  for (i = 0; strings[i]; ++i)
    {
      size = strlen (strings[i]) + 1;
      if (size > max_size)
	max_size = size;
    }
  return max_size;
}

static SANE_Status
constrain_value (Avision_Scanner *s, SANE_Int option, void *value,
		 SANE_Int *info)
{
  DBG (3, "constrain_value\n");
  return sanei_constrain_value (s->opt + option, value, info);
}

static SANE_Status
get_frame_info (int fd, int *number_of_frames, int *frame, int *holder_type)
{
  /* read stuff */
  struct command_read rcmd;
  unsigned int size;
  SANE_Status status;
  u_int8_t result[8];
  unsigned int i;

  frame = frame; /* silence gcc */

  DBG (3, "get_frame_info\n");
 
  size = sizeof (result);
 
  memset (&rcmd, 0, sizeof (rcmd));
  rcmd.opc = AVISION_SCSI_READ;
 
  rcmd.datatypecode = 0x87; /* film holder sense */
  rcmd.datatypequal [0] = 0x0d;
  rcmd.datatypequal [1] = 0x0a;
  set_triple (rcmd.transferlen, size);
 
  status = sanei_scsi_cmd (fd, &rcmd, sizeof (rcmd), result, &size);
  if (status != SANE_STATUS_GOOD || size != sizeof (result)) {
    DBG (1, "get_frame_info: read failed (%s)\n", sane_strstatus (status));
    return (status);
  }
 
  DBG (6, "RAW-Data:\n");
  for (i = 0; i < size; ++ i) {
    DBG (6, "get_frame_info: result [%2d] %1d%1d%1d%1d%1d%1d%1d%1db %3oo %3dd %2xx\n",
	 i,
	 BIT(result[i],7), BIT(result[i],6), BIT(result[i],5), BIT(result[i],4),
	 BIT(result[i],3), BIT(result[i],2), BIT(result[i],1), BIT(result[i],0),
	 result[i], result[i], result[i]);
  }
  
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
     can be changde between scans. */
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
    ++model_num;
  }
  
  if (!found) {
    DBG (1, "attach: model is not in the list of supported model!\n");
    DBG (1, "attach: You might want to report this output. To:\n");
    DBG (1, "attach: rene.rebe@gmx.net (Backend author)\n");
    
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

  DBG (6, "RAW-Data:\n");
  for (i=0; i<sizeof(result); i++) {
    DBG (6, "result [%2d] %1d%1d%1d%1d%1d%1d%1d%1db %3oo %3dd %2xx\n", i,
	 BIT(result[i],7), BIT(result[i],6), BIT(result[i],5), BIT(result[i],4),
	 BIT(result[i],3), BIT(result[i],2), BIT(result[i],1), BIT(result[i],0),
	 result[i], result[i], result[i]);
  }
  
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
       (result[36] & 0x7) == 0 ? " RGB_COLOR_PLANE":"RESERVED?");
  
  DBG (3, "attach: [37]    Optical res.:    %d00 dpi\n", result[37]);
  DBG (3, "attach: [38]    Maximum res.:    %d00 dpi\n", result[38]);
  
  DBG (3, "attach: [39]    Bitfield1:%s%s%s%s%s\n",
       BIT(result[39],7)?" TRANS":"",
       BIT(result[39],6)?" Q_SCAN":"",
       BIT(result[39],5)?" EXTENDET_RES":"",
       BIT(result[39],4)?" SUPPORTS_CALIB":"",
       BIT(result[39],2)?" NEW_PROTOCOL":"");
  
  DBG (3, "attach: [40-41] X res. in gray:  %d dpi\n", (result[40]<<8)+result[41]);
  DBG (3, "attach: [42-43] Y res. in gray:  %d dpi\n", (result[42]<<8)+result[43]);
  DBG (3, "attach: [44-45] X res. in color: %d dpi\n", (result[44]<<8)+result[45]);
  DBG (3, "attach: [46-47] Y res. in color: %d dpi\n", (result[46]<<8)+result[47]);
  DBG (3, "attach: [48-49] USB max read:    %d\n", (result[48]<<8)+result[49]);

  DBG (3, "attach: [50]    ESA1:%s%s%s%s%s%s%s%s\n",
       BIT(result[50],7)?" LIGHT":"",
       BIT(result[50],6)?" BUTTON":"",
       BIT(result[50],5)?" NEED_SW_COLORPACK":"",
       BIT(result[50],4)?" SW_CALIB":"",
       BIT(result[50],3)?" NEED_SW_GAMMA":"",
       BIT(result[50],2)?" KEEPS_GAMMA":"",
       BIT(result[50],1)?" KEEPS_WINDOW_CMD":"",
       BIT(result[50],0)?" XYRES_DIFFERENT":"");
  DBG (3, "attach: [51]    ESA2:%s%s%s%s%s%s%s%s\n",
       BIT(result[51],7)?" EXPOSURE_CTRL":"",
       BIT(result[51],6)?" NEED_SW_TRIGGER_CAL":"",
       BIT(result[51],5)?" NEED_WHITE_PAPER":"",
       BIT(result[51],4)?" SUPP_QUALITY_SPEED_CAL":"",
       BIT(result[51],3)?" NEED_TRANSP_CAL":"",
       BIT(result[51],2)?" HAS_PUSH_BUTTON":"",
       BIT(result[51],1)?" NEW_CAL_METHOD":"",
       BIT(result[51],0)?" ADF_MIRRORS":"");
  DBG (3, "attach: [52]    ESA3:%s%s%s%s%s%s%s\n",
       BIT(result[52],7)?" GRAY_WHITE":"",
       BIT(result[52],5)?" TET":"", /* huh ?!? */
       BIT(result[52],4)?" 3x3COL_TABLE":"",
       BIT(result[52],3)?" 1x3FILTER":"",
       BIT(result[52],2)?" INDEX_COLOR":"",
       BIT(result[52],1)?" POWER_SAVING_TIMER":"",
       BIT(result[52],0)?" NVM_DATA_REC":"");
   
  /* print some more scanner features/params */
  DBG (3, "attach: [53]    line difference (software color pack): %d\n", result[53]);
  DBG (3, "attach: [54]    color mode pixel boundary: %d\n", result[54]);
  DBG (3, "attach: [55]    grey mode pixel boundary: %d\n", result[55]);
  DBG (3, "attach: [56]    4bit grey mode pixel boundary: %d\n", result[56]);
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
       BIT(result[62],7)?" Flatbed (w.o. ADF?)":"", /* not really in spec but resonable */
       BIT(result[62],6)?" Roller (ADF)":"",
       BIT(result[62],5)?" Flatbed (ADF)":"",
       BIT(result[62],4)?" Roller scanner (w.o. ADF)":"",
       BIT(result[62],3)?" Film scanner":"");
  
  DBG (3, "attach: [77-78] Max X of transparency: %d FUBA\n", (result[77]<<8)+result[78]);
  DBG (3, "attach: [79-80] May Y of transparency: %d FUBA\n", (result[79]<<8)+result[80]);
  
  DBG (3, "attach: [81-82] Max X of flatbed:      %d FUBA\n", (result[81]<<8)+result[82]);
  DBG (3, "attach: [83-84] May Y of flatbed:      %d FUBA\n", (result[83]<<8)+result[84]);
  
  DBG (3, "attach: [85-86] Max X of ADF:          %d FUBA\n", (result[85]<<8)+result[86]);
  DBG (3, "attach: [87-88] May Y of ADF:          %d FUBA\n", (result[87]<<8)+result[88]);
  DBG (3, "attach: (FUBA: inch/300 for flatbed, and inch/max_res for film scanners?)\n");
  
  DBG (3, "attach: [89-90] Res. in Ex. mode:      %d dpi\n", (result[89]<<8)+result[90]);
  
  DBG (3, "attach: [92]    Buttons:  %d\n", result[92]);
  
  DBG (3, "attach: [93]    ESA4:%s%s%s%s%s%s%s%s\n",
       BIT(result[93],7)?" SUPP_ACCESSORIES_DETECT":"",
       BIT(result[93],6)?" ADF_IS_BGR_ORDERED":"",
       BIT(result[93],5)?" NO_SINGLE_CHANNEL_GRAY_MODE":"",
       BIT(result[93],4)?" SUPP_FLASH_UPDATE":"",
       BIT(result[93],3)?" SUPP_ASIC_UPDATE":"",
       BIT(result[93],2)?" SUPP_LIGHT_DETECT":"",
       BIT(result[93],1)?" SUPP_READ_PRNU_DATA":"",
       BIT(result[93],0)?" SCAN_FLATBED_MIRRORED":"");
  
  /* if (BIT(result[62],3) ) { */ /* most scanners report film functionallity ... */
  switch(dev->hw->scanner_type) {
  case AV_FLATBED:
	  dev->sane.type   = "flatbed scanner";
	  break;
  case AV_FILM:
	  dev->sane.type   = "film scanner";
	  break;
  case AV_SHEETFEED:
	  dev->sane.type   =  "sheetfed scanner";
	  break;
  }
  
  dev->inquiry_new_protocol = BIT (result[39],2);
  dev->inquiry_needs_calibration = BIT (result[50],4);
  dev->inquiry_needs_gamma = BIT (result[50],3);
  dev->inquiry_needs_software_colorpack = BIT (result[50],5);
  
  dev->inquiry_color_boundary = result[54];
  if (dev->inquiry_color_boundary == 0)
    dev->inquiry_color_boundary = 8;
  
  dev->inquiry_grey_boundary = result[55];
  if (dev->inquiry_grey_boundary == 0)
    dev->inquiry_grey_boundary = 8;
 
  dev->inquiry_line_difference = result[53];
  
  switch(dev->hw->scanner_type) {
  case AV_FLATBED:
  case AV_SHEETFEED:
    dev->inquiry_optical_res = result[37] * 100;
    if (dev->inquiry_optical_res == 0) {
      if (dev->hw->scanner_type == AV_SHEETFEED) {
	DBG (1, "Inquiry optical resolution is invalid, using 300 dpi (sf)!\n");
	dev->inquiry_optical_res = 300;
      }
      else {
	DBG (1, "Inquiry optical resolution is invalid, using 600 dpi (def)!\n");
	dev->inquiry_optical_res = 600;
      }
    }
    dev->inquiry_max_res = result[38] * 100;
    if (dev->inquiry_max_res == 0) {
      DBG (1, "Inquiry max resolution is invalid, using 1200 dpi!\n");
      dev->inquiry_max_res = 1200;
    }
    break;
  case AV_FILM:
    {
      int opt_x = (result[44]<<8) + result[45];
      int opt_y = (result[46]<<8) + result[47];
      dev->inquiry_optical_res = (opt_x < opt_y) ? opt_x : opt_y;
      dev->inquiry_max_res = dev->inquiry_optical_res;
      DBG (1, "attach: optical resolution set to: %d dpi\n", dev->inquiry_optical_res);
    }
  }
  
  /* Get max X and max Y ... */
  dev->inquiry_x_range = ( ( (unsigned int)result[81] << 8)
			   + (unsigned int)result[82] ) * MM_PER_INCH;
  dev->inquiry_y_range = ( ( (unsigned int)result[83] << 8)
			   + (unsigned int)result[84] ) * MM_PER_INCH;

  dev->inquiry_adf_x_range = ( ( (unsigned int)result[85] << 8)
			       + (unsigned int)result[86] ) * MM_PER_INCH;
  dev->inquiry_adf_y_range = ( ( (unsigned int)result[87] << 8)
			       + (unsigned int)result[88] ) * MM_PER_INCH;
  
  dev->inquiry_transp_x_range = ( ( (unsigned int)result[77] << 8)
				  + (unsigned int)result[78] ) * MM_PER_INCH;
  dev->inquiry_transp_y_range = ( ( (unsigned int)result[79] << 8)
				  + (unsigned int)result[80] ) * MM_PER_INCH;
  
  if (dev->hw->scanner_type != AV_FILM) {
    dev->inquiry_x_range /= 300;
    dev->inquiry_y_range /= 300;
    dev->inquiry_adf_x_range /= 300;
    dev->inquiry_adf_y_range /= 300;
    dev->inquiry_transp_x_range /= 300;
    dev->inquiry_transp_y_range /= 300;
  } else {
    /* ZP: The right number is 2820, wether it is 40-41, 42-43, 44-45,
     * 46-47 or 89-90 I don't know but I would bet for the last !
     * RR: OK. We use it via the optical_res which we need anyway ...
     */
    dev->inquiry_x_range /= dev->inquiry_optical_res;
    dev->inquiry_y_range /= dev->inquiry_optical_res;
    dev->inquiry_adf_x_range /= dev->inquiry_optical_res;
    dev->inquiry_adf_y_range /= dev->inquiry_optical_res;
    dev->inquiry_transp_x_range /= dev->inquiry_optical_res;
    dev->inquiry_transp_y_range /= dev->inquiry_optical_res;
  }
  
  /* check if x/y range is valid :-((( */
  if (dev->inquiry_x_range == 0 || dev->inquiry_y_range == 0) {
    if (dev->hw->scanner_type == AV_SHEETFEED) {
      DBG (1, "Inquiry x/y-range is invalid! Using defauld %fx%finch (sheetfeed).\n",
	   A4_X_RANGE, SHEETFEED_Y_RANGE);
      dev->inquiry_x_range = A4_X_RANGE * MM_PER_INCH;
      dev->inquiry_y_range = SHEETFEED_Y_RANGE * MM_PER_INCH;
    }
    else {
      DBG (1, "Inquiry x/y-range is invalid! Using defauld %fx%finch (ISO A4).\n",
	   A4_X_RANGE, A4_Y_RANGE);
      
      dev->inquiry_x_range = A4_X_RANGE * MM_PER_INCH;
      dev->inquiry_y_range = A4_Y_RANGE * MM_PER_INCH;
    }
  }
  else
    DBG (3, "Inquiry x/y-range seems to be valid!\n");
    if (force_a4) {
    DBG (1, "option: \"force_a4\" found! Using defauld %fx%finch (ISO A4).\n",
	 A4_X_RANGE, A4_Y_RANGE);
    dev->inquiry_x_range = A4_X_RANGE * MM_PER_INCH;
    dev->inquiry_y_range = A4_Y_RANGE * MM_PER_INCH;
  }
  
  DBG (1, "attach: range:        %f x %f\n",
       dev->inquiry_x_range, dev->inquiry_y_range);
  DBG (1, "attach: adf_range:    %f x %f\n",
       dev->inquiry_adf_x_range, dev->inquiry_adf_y_range);
  DBG (1, "attach: transp_range: %f x %f\n",
       dev->inquiry_transp_x_range, dev->inquiry_transp_y_range);
 
  /* For a film scanner try to retrieve additional frame information */
  if (dev->hw->scanner_type == AV_FILM) {
    get_frame_info (fd, &dev->frame_range.max,
		    &dev->current_frame,
		    &dev->holder_type);
    dev->frame_range.min = 1;
    dev->frame_range.quant = 1;
  }
  
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
perform_calibration (Avision_Scanner* s)
{
  SANE_Status status;
  
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
  
  DBG (3, "perform_calibration: get calibration format\n");

  size = sizeof (result);
  
  memset (&rcmd, 0, sizeof (rcmd));
  rcmd.opc = AVISION_SCSI_READ;
  rcmd.datatypecode = 0x60; /* get calibration format */
  /* rcmd.calibchn = 0; */
  rcmd.datatypequal [0] = 0x0d;
  rcmd.datatypequal [1] = 0x0a;
  set_triple (rcmd.transferlen, size);
  
  DBG (3, "perform_calibration: read_data: %d bytes\n", size);
  status = sanei_scsi_cmd (s->fd, &rcmd, sizeof (rcmd), result, &size);
  if (status != SANE_STATUS_GOOD || size != sizeof (result) ) {
    DBG (1, "perform_calibration: read calib. info failt (%s)\n", sane_strstatus (status) );
    return status;
  }
  
  DBG (5, "RAW-Data:\n");
  for (i = 0; i < size; ++ i) {
    DBG (5, "result [%2d] %1d%1d%1d%1d%1d%1d%1d%1db %3oo %3dd %2xx\n", i, BIT(result[i],7), 
         BIT(result[i],6), BIT(result[i],5), BIT(result[i],4), BIT(result[i],3), BIT(result[i],2),
         BIT(result[i],1), BIT(result[i],0), result[i], result[i], result[i]);
  }
  
  DBG (3, "calib_info: [0-1]  pixels per line %d\n", (result[0]<<8)+result[1]);
  DBG (3, "calib_info: [2]    bytes per channel %d\n", result[2]);
  DBG (3, "calib_info: [3]    line count %d\n", result[3]);
  
  DBG (3, "calib_info: [4]   FLAG:%s%s%s\n",
       result[4] == 1?" MUST_DO_CALIBRATION":"",
       result[4] == 2?" SCAN_IMAGE_DOES_CALIBRATION":"",
       result[4] == 3?" NEEDS_NO_CALIBRATION":"");
  
  DBG (3, "calib_info: [5]    Ability1:%s%s%s%s%s%s%s%s\n",
        BIT(result[5],7)?" NONE_PACKED":" PACKED",
  	BIT(result[5],6)?" INTERPOLATED":"",
  	BIT(result[5],5)?" SEND_REVERSED":"",
  	BIT(result[5],4)?" PACKED_DATA":"",
  	BIT(result[5],3)?" COLOR_CALIB":"",
  	BIT(result[5],2)?" DARK_CALIB":"",
  	BIT(result[5],1)?" NEEDS_WHITE_BLACK_SHADING_DATA":"",
  	BIT(result[5],0)?" NEEDS_2_CALIBS":"");
  
  DBG (3, "calib_info: [6]    R gain: %d\n", result[6]);
  DBG (3, "calib_info: [7]    G gain: %d\n", result[7]);
  DBG (3, "calib_info: [8]    B gain: %d\n", result[8]);
  
  /* (try) to get calibration data:
   * the read command 63 (read channel data hangs for HP 5300 ?
   */
  
  {
    unsigned int calib_pixels_per_line = ( (result[0] << 8) + result[1] );
    unsigned int calib_bytes_per_channel = result[2];
    unsigned int calib_line_count = result[3];
    
    /* Limit to max scsi transfer size ???
    unsigned int used_lines = sanei_scsi_max_request_size /
      (calib_pixels_per_line * calib_bytes_per_channel);
    */
    
    unsigned int used_lines = calib_line_count;
    
    DBG (3, "perform_calibration: using %d lines\n", used_lines);
    
    calib_size = calib_pixels_per_line * calib_bytes_per_channel * used_lines;
    DBG (3, "perform_calibration: read %d bytes\n", calib_size);
    
    calib_data = malloc (calib_size);
    if (!calib_data)
      return SANE_STATUS_NO_MEM;
    
    memset (&rcmd, 0, sizeof (rcmd));
    rcmd.opc = AVISION_SCSI_READ;
    /* 66: dark, 62: color, (63: channel HANGS) data */
    rcmd.datatypecode = 0x62;
    /* only needed for single channel mode - which hangs the HP 5300 */
    /* rcmd.calibchn = color; */
    rcmd.datatypequal [0] = 0x0d;
    rcmd.datatypequal [1] = 0x0a;
    set_triple (rcmd.transferlen, calib_size);
    
    DBG (1, "perform_calibration: %p %d\n", calib_data, calib_size);
    status = sanei_scsi_cmd (s->fd, &rcmd, sizeof (rcmd), calib_data, &calib_size);
    if (status != SANE_STATUS_GOOD) {
      DBG (1, "perform_calibration: calibration data read failed (%s)\n", sane_strstatus (status));
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
    
    DBG (3, "perform_calibration: computing: %d bytes calibration data\n", size);
    
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
	  
	  for (line = 0; line < lines; ++line) {
	    if ( (line * offset) + i >= calib_size)
	      DBG (3, "perform_calibration: BUG: src out of range!!! %d\n", (line * offset) + i);
	    else
	      avg += calib_data [ (line * offset) + i];
	  }
	  
	  factor = (lines * 255 - (avg * 0.85) ) * 255 / lines;
	  
	  DBG (8, "pixel: %d: avg: %f, factor: %lx\n", i, avg, factor);
	  if ( (i * 2) + 1 < out_size) {
	    out_data [(i * 2)] = factor;
	    out_data [(i * 2) + 1] = factor >> 8;
	  }
	  else {
	    DBG (3, "perform_calibration: BUG: dest out of range!!! %d\n", (i * 2) + 1);
	  }
	}
    }
    
    if (1) /* send data the tested way */
      {
	DBG (3, "perform_calibration: all channels in one command\n");

	memset (&scmd, 0, sizeof (scmd));
	scmd.opc = AVISION_SCSI_SEND;
	scmd.datatypecode = 0x82; /* send calibration data */
	/* 0,1,2: color; 11: dark; 12 color calib data */
	set_double (scmd.datatypequal, 0x12);
	set_triple (scmd.transferlen, out_size);
	
	status = sanei_scsi_cmd2 (s->fd, &scmd, sizeof (scmd), out_data, out_size, 0, 0);
	if (status != SANE_STATUS_GOOD) {
	  DBG (3, "perform_calibration: send_data (%s)\n", sane_strstatus (status));
	  return status;
	}
      }
    else /* send data in an alternative way (hangs for the HP 5300 ...) */
      {
	u_int8_t* converted_out_data;
	
	DBG (3, "perform_calibration: channels in single commands\n");
	
	converted_out_data = malloc (out_size / 3);
	if (!out_data)
	  return SANE_STATUS_NO_MEM;
	
	for (color = 0; color < 3; ++ color)
	  {
	    int conv_out_size = calib_pixels_per_line * 2;
	    
	    DBG (3, "perform_calibration: channel: %i\n", color);
	    
	    for (i = 0; i < calib_pixels_per_line; ++ i) {
	      int conv_pixel = i * 2;
	      int out_pixel = ((i * 3) + color) * 2;
	      
	      converted_out_data [conv_pixel] = out_data [out_pixel];
	      converted_out_data [conv_pixel + 1] = out_data [out_pixel + 1];
	    }
	    
	    DBG (3, "perform_calibration: sending now %i bytes \n", conv_out_size);
	    
	    memset (&scmd, 0, sizeof (scmd));
	    scmd.opc = AVISION_SCSI_SEND;
	    scmd.datatypecode = 0x82; /* send calibration data */
	    
	    /* 0,1,2: color; 11: dark; 12 color calib data */
	    set_double (scmd.datatypequal, color);
	    set_triple (scmd.transferlen, conv_out_size);
	    
	    status = sanei_scsi_cmd2 (s->fd, &scmd, sizeof (scmd),
				      converted_out_data, conv_out_size, 0, 0);
	    if (status != SANE_STATUS_GOOD) {
	      DBG (3, "perform_calibration: send_data (%s)\n", sane_strstatus (status));
	      return status;
	    }
	  }
	
	free (converted_out_data);
      }
    
    free (calib_data); /* crashed some times ??? */
    free (out_data);
  } /* end compute calibratin data */
  
  DBG (3, "perform_calibration: return\n");
  return SANE_STATUS_GOOD;
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
  
#define GAMMA_TABLE_SIZE 4096
  struct gamma_cmd
  {
    struct command_send cmd;
    u_int8_t gamma_data [GAMMA_TABLE_SIZE];
  };
  
  struct gamma_cmd* scmd;
  
  int color; /* current color */
  int i; /* big table index */
  int j; /* little table index */
  int k; /* big table sub index */
  double v1, v2;
  
  double brightness;
  double contrast;
  
  DBG (3, "set_gamma\n");
  
  /* prepare for emulating contrast, brightness ... via the gamma-table */
  brightness = SANE_UNFIX (s->val[OPT_BRIGHTNESS].w);
  brightness /= 100;
  contrast = SANE_UNFIX (s->val[OPT_CONTRAST].w);
  contrast /= 100;
  
  DBG (3, "brightness: %f, contrast: %f\n", brightness, contrast);
  
  
  for (color = 0; color < 3; ++ color)
    {
      scmd = malloc (sizeof (*scmd) );
      if (!scmd)
	return SANE_STATUS_NO_MEM;
      memset (scmd, 0, sizeof (*scmd) );
	  
      scmd->cmd.opc = AVISION_SCSI_SEND;
      scmd->cmd.datatypecode = 0x81; /* 0x81 for download gama table */
      set_double (scmd->cmd.datatypequal, color); /* color: 0=red; 1=green; 2=blue */
      set_triple (scmd->cmd.transferlen, GAMMA_TABLE_SIZE);
	  
      i = 0; /* big table index */
      for (j = 0; j < 256; j++) /* little table index */
	{
	  /* calculate mode dependent values v1 and v2
	   * v1 <- current value for table
	   * v2 <- next value for table (for interpolation)
	   */
	  switch (s->mode)
	    {
	    case TRUECOLOR:
	      {
		v1 = (double) (s->gamma_table [0][j] + s->gamma_table [1 + color][j] ) / 2;
		if (j == 255)
		  v2 = (double) v1;
		else
		  v2 = (double) (s->gamma_table [0][j + 1] + s->gamma_table [1 + color][j + 1] ) / 2;
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
	      
	      /* emulate brightness, contrast (at least the Avision AV6[2,3]0 are not able to
	       * do this in hardware ... --EUR - taken from the GIMP source - I'll optimize
	       * it when it is known to work (and I have time)
	       */
	  v1 /= 255;
	  v2 /= 255;
	      
	  v1 = (brightness_contrast_func (brightness, contrast, v1) );
	  v2 = (brightness_contrast_func (brightness, contrast, v2) );
	      
	  v1 *= 255;
	  v2 *= 255;
	      
	  if (!dev->inquiry_new_protocol) {
	    for (k = 0; k < 8; k++, i++)
	      scmd->gamma_data [i] = ( ( (u_int8_t)v1 * (8 - k)) + ( (u_int8_t)v2 * k) ) / 8;
	  }
	  else {
	    for (k = 0; k < 16; k++, i++)
	      scmd->gamma_data [i] = ( ( (u_int8_t)v1 * (16 - k)) + ( (u_int8_t)v2 * k) ) / 16;
	  }
	}
      /* fill the gamma table - if 11bit (old protocol) table */
      if (!dev->inquiry_new_protocol) {
	for (i = 2048; i < 4096; i++)
	  scmd->gamma_data [i] = scmd->gamma_data [2047];
      }
	  
      status = sanei_scsi_cmd (s->fd, scmd, sizeof (*scmd), 0, 0);
	  
      free (scmd);
    }
  
  return status;
}

static SANE_Status
set_window (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  SANE_Status status;
  struct
  {
    struct command_set_window cmd;
    struct command_set_window_window_header window_header;
    struct command_set_window_window_descriptor window_descriptor;
  } cmd;
   
  DBG (3, "set_window\n");
  
  /* wipe out anything */
  memset (&cmd, 0, sizeof (cmd) );
  
  /* command setup */
  cmd.cmd.opc = AVISION_SCSI_SET_WINDOW;
  set_triple (cmd.cmd.transferlen,
	      sizeof (cmd.window_header) + sizeof (cmd.window_descriptor) );
  set_double (cmd.window_header.desclen, sizeof (cmd.window_descriptor) );
  
  /* resolution parameters */
  set_double (cmd.window_descriptor.xres, s->avdimen.res);
  set_double (cmd.window_descriptor.yres, s->avdimen.res);
     
  /* upper left corner coordinates in inch/1200 */
  set_quad (cmd.window_descriptor.ulx, s->avdimen.tlx);
  set_quad (cmd.window_descriptor.uly, s->avdimen.tly);
  
  /* width and length in inch/1200 */
  set_quad (cmd.window_descriptor.width, s->avdimen.width);
  set_quad (cmd.window_descriptor.length, s->avdimen.length);

  /* width and length in bytes */
  set_double (cmd.window_descriptor.linewidth, s->params.bytes_per_line);
  if (s->mode == TRUECOLOR && dev->inquiry_needs_software_colorpack) {
    set_double (cmd.window_descriptor.linecount,
		s->params.lines + s->avdimen.line_difference);
  }
  else {
    set_double (cmd.window_descriptor.linecount,
		s->params.lines);
  }
  
  cmd.window_descriptor.bitset1 = 0x60;
  cmd.window_descriptor.bitset1 |= s->val[OPT_SPEED].w;
  
  cmd.window_descriptor.bitset2 = 0x00;
  
  /* quality scan option switch */
  if (s->val[OPT_QSCAN].w == SANE_TRUE) {
    cmd.window_descriptor.bitset2 |= AV_QSCAN_ON; /* Q_SCAN ON */
  }
  
  /* quality calibration option switch */
  if (s->val[OPT_QCALIB].w == SANE_TRUE) {
    cmd.window_descriptor.bitset2 |= AV_QCALIB_ON; /* Q_CALIB ON */
  }
  
  /* transparency switch */
  if (s->val[OPT_TRANS].w == SANE_TRUE) {
    cmd.window_descriptor.bitset2 |= AV_TRANS_ON; /* Set to transparency mode */
  }
  
  /* fixed value */
  cmd.window_descriptor.pad_type = 3;
  cmd.window_descriptor.vendor_specid = 0xFF;
  cmd.window_descriptor.paralen = 9;

  /* currently also fixed
   * (and unsopported by all Avision scanner I know ...)
   */
  cmd.window_descriptor.highlight = 0xFF;
  cmd.window_descriptor.shadow = 0x00;
  
  /* this is normaly unsupported by avsion scanner.
   * I clear this only to be shure if an other scanner knows about it ...
   * we do this via the gamma table ...
   */
  cmd.window_descriptor.thresh = 128;
  cmd.window_descriptor.brightness = 128; 
  cmd.window_descriptor.contrast = 128;
  
  /* mode dependant settings */
  switch (s->mode)
    {
    case THRESHOLDED:
      cmd.window_descriptor.bpc = 1;
      cmd.window_descriptor.image_comp = 0;
      cmd.window_descriptor.bitset1 &= 0xC7;
      break;
    
    case DITHERED:
      cmd.window_descriptor.bpc = 1;
      cmd.window_descriptor.image_comp = 1;
      cmd.window_descriptor.bitset1 &= 0xC7;
      break;
      
    case GREYSCALE:
      cmd.window_descriptor.bpc = 8;
      cmd.window_descriptor.image_comp = 2;
      cmd.window_descriptor.bitset1 &= 0xC7;
      /* RR: what is it for Gunter ?? - doesn't work */
      /* cmd.window_descriptor.bitset1 |= 0x30; */
      break;
      
    case TRUECOLOR:
      cmd.window_descriptor.bpc = 8;
      cmd.window_descriptor.image_comp = 5;
      break;
      
    default:
      DBG (1, "Invalid mode. %d\n", s->mode);
      return SANE_STATUS_INVAL;
    }
  
  DBG (3, "set_window: sending command\n");
  status = sanei_scsi_cmd (s->fd, &cmd, sizeof (cmd), 0, 0);
  
  return status;
}

static SANE_Status
reserve_unit (Avision_Scanner *s)
{
  char cmd[] =
    {0x16, 0, 0, 0, 0, 0};
  SANE_Status status;
  
  status = sanei_scsi_cmd (s->fd, cmd, sizeof (cmd), 0, 0);
  return status;
}

static SANE_Status
release_unit (Avision_Scanner *s)
{
  char cmd[] =
    {0x17, 0, 0, 0, 0, 0};
  SANE_Status status;
  
  status = sanei_scsi_cmd (s->fd, cmd, sizeof (cmd), 0, 0);
  return status;
}

/* Check if a sheet is present. For Sheetfeed scanners only. */
static SANE_Status
check_paper (Avision_Scanner *s)
{
  char cmd[] = {0x08, 0, 0, 0, 1, 0};
  SANE_Status status;
  char buf[1];
  size_t size = 1;
  
  status = sanei_scsi_cmd2 (s->fd, cmd, sizeof (cmd),
			    NULL, 0,
			    buf, &size);

  if (status == SANE_STATUS_GOOD) {
    if (buf[0] == 0) 
      status = SANE_STATUS_NO_DOCS;
  }
  
  return status;
}

static SANE_Status
go_home (Avision_Scanner *s)
{
  SANE_Status status;
  
  /* for film scanners! Check adf's as well */
  char cmd[] =
    {0x31, 0x02, 0, 0, 0, 0, 0, 0, 0, 0}; /* Object position */
  
  DBG (3, "go_home\n");
  
  status = sanei_scsi_cmd (s->fd, cmd, sizeof(cmd), 0, 0);
  return status;
}

static SANE_Status
start_scan (Avision_Scanner *s)
{
  struct command_scan cmd;
    
  DBG (3, "start_scan\n");

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
    
  return sanei_scsi_cmd (s->fd, &cmd, sizeof (cmd), 0, 0);
}

static SANE_Status
do_eof (Avision_Scanner *s)
{
  int exit_status;
  
  DBG (3, "do_eof\n");
  
  if (s->pipe >= 0)
    {
      close (s->pipe);
      s->pipe = -1;
    }
  wait (&exit_status); /* without a wait() call you will produce
			  defunct childs */

  s->reader_pid = 0;

  return SANE_STATUS_EOF;
}

static SANE_Status
do_cancel (Avision_Scanner *s)
{
  SANE_Status status;
  
  DBG (3, "do_cancel\n");

  s->scanning = SANE_FALSE;
  
  /* do_eof (s); needed? */

  if (s->reader_pid > 0)
    {
      int exit_status;

      /* ensure child knows it's time to stop: */
      kill (s->reader_pid, SIGTERM);
      while (wait (&exit_status) != s->reader_pid);
      s->reader_pid = 0;
    }
  
  if (s->fd >= 0)
    {
      /* release the device */
      status = release_unit (s);
      
      /* go_home (s); */
      
      sanei_scsi_close (s->fd);
      s->fd = -1;
    }
  
  return SANE_STATUS_CANCELLED;
}

static SANE_Status
read_data (Avision_Scanner* s, SANE_Byte* buf, unsigned int count)
{
  struct command_read rcmd;
  SANE_Status status;

  DBG (3, "read_data: %d\n", count);
  
  memset (&rcmd, 0, sizeof (rcmd));
  
  rcmd.opc = AVISION_SCSI_READ;
  rcmd.datatypecode = 0x00; /* read image data */
  rcmd.datatypequal [0] = 0x0d;
  rcmd.datatypequal [1] = 0x0a;
  set_triple (rcmd.transferlen, count);
  
  status = sanei_scsi_cmd (s->fd, &rcmd, sizeof (rcmd), buf, &count);
  
  return status;
}

static SANE_Status
init_options (Avision_Scanner* s)
{
  Avision_Device* dev = s->hw;
  int i;
  
  DBG (3, "init_options\n");
  
  memset (s->opt, 0, sizeof (s->opt));
  memset (s->val, 0, sizeof (s->val));

  for (i = 0; i < NUM_OPTIONS; ++i) {
    s->opt[i].size = sizeof (SANE_Word);
    s->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }
  
  /* Init the SANE option from the scanner inquiry data */
  
  dev->x_range.max = SANE_FIX ( (int)dev->inquiry_x_range);
  dev->x_range.quant = 0;
  dev->y_range.max = SANE_FIX ( (int)dev->inquiry_y_range);
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
  
  s->mode = make_mode (s->val[OPT_MODE].s);
  
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

  /* transparency adapter. */
  s->opt[OPT_TRANS].name = "transparency";
  s->opt[OPT_TRANS].title = "Enable transparency adapter";
  s->opt[OPT_TRANS].desc = "Switch transparency mode on. Which mainly switches the scanner " \
    "lamp off. (Hint: This can also be used to switch the scanner lamp off when you don't " \
    "use the scanner in the next time. Simply enable this option and do a preview.)";
  s->opt[OPT_TRANS].type = SANE_TYPE_BOOL;
  s->opt[OPT_TRANS].unit = SANE_UNIT_NONE;
  s->val[OPT_TRANS].w = SANE_FALSE;
  if (dev->hw->scanner_type == AV_SHEETFEED)
	  s->opt[OPT_TRANS].cap |= SANE_CAP_INACTIVE;
  
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
  s->opt[OPT_QSCAN].desc   = "Turn on quality scanning (slower & better)";
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
reader_process (Avision_Scanner *s, int fd)
{
  Avision_Device* dev = s->hw;
  
  /* the fat strip we currently puzzle together to perform software-colorpack and more */
  u_int8_t* stripe_data;
  /* the corrected output data */
  u_int8_t* out_data;
  
  /* the glocal complex params */
  int bytes_per_line;
  int lines_per_stripe;
  int lines_per_output;
  int max_bytes_per_read;
  
  /* the dumb params for the data reader */
  int needed_bytes;
  int processed_bytes;
  int bytes_in_stripe;
  
  SANE_Status status;
  sigset_t sigterm_set;
  FILE* fp;
  
  DBG (3, "reader_process\n");
  
  sigemptyset (&sigterm_set);
  sigaddset (&sigterm_set, SIGTERM);
  
  fp = fdopen (fd, "w");
  if (!fp)
    return 1;
  
  bytes_per_line = s->params.bytes_per_line;
  lines_per_stripe = s->avdimen.line_difference * 2;
  if (lines_per_stripe == 0)
    lines_per_stripe = 8; /* TODO: perform some nice max 1/2 inch computation */
  
  lines_per_output = lines_per_stripe - s->avdimen.line_difference;
  
  /* the "/2" might makes scans faster - because it should leave some space
     in the SCSI buffers (scanner, kernel, ...) empty to send _ahead_ ... */
  max_bytes_per_read = sanei_scsi_max_request_size / 2;
  
  DBG (3, "bytes_per_line=%d, lines_per_stripe=%d, lines_per_output=%d\n",
       bytes_per_line, lines_per_stripe, lines_per_output);
  
  DBG (3, "max_bytes_per_read=%d, stripe_data size=%d\n",
       max_bytes_per_read, bytes_per_line * lines_per_stripe);
  
  stripe_data = malloc (bytes_per_line * lines_per_stripe);
  out_data = malloc (bytes_per_line * lines_per_output);
  
  s->line = 0;
  
  /* calculate params for the brain-daed reader */
  needed_bytes = bytes_per_line * s->params.lines;
  if (s->mode == TRUECOLOR)
    needed_bytes +=  bytes_per_line * s->avdimen.line_difference;
  
  processed_bytes = 0;
  bytes_in_stripe = 0;
  
  DBG (3, "total needed_bytes=%d\n", needed_bytes);
  
  while (s->line < s->params.lines)
    {
      int usefull_bytes;
      
      /* fille the stripe buffer */
      while (processed_bytes < needed_bytes && bytes_in_stripe < lines_per_stripe * bytes_per_line)
	{
	  int this_read = (bytes_per_line * lines_per_stripe) - bytes_in_stripe;
	  
	  /* limit reads to max_read and global data boundaries */
	  if (this_read > max_bytes_per_read)
	    this_read = max_bytes_per_read;
	  
	  if (processed_bytes + this_read > needed_bytes)
	    this_read = needed_bytes - processed_bytes;
	  
	  DBG (5, "this read: %d\n", this_read);
	  
	  sigprocmask (SIG_BLOCK, &sigterm_set, 0);
	  status = read_data (s, stripe_data + bytes_in_stripe, this_read);
	  sigprocmask (SIG_UNBLOCK, &sigterm_set, 0);
	  
	  if (status != SANE_STATUS_GOOD) {
	    DBG (1, "reader_process: read_data failed with status=%d\n", status);
	    return 3;
	  }
	  
	  bytes_in_stripe += this_read;
	  processed_bytes += this_read;
	}
      
      DBG (5, "reader_process: buffer filled\n");
      
      /* perform output convertion */
      
      usefull_bytes = bytes_in_stripe;
      if (s->mode == TRUECOLOR)
	usefull_bytes -= s->avdimen.line_difference * bytes_per_line;
      
      DBG (5, "reader_process: usefull_bytes %i\n", usefull_bytes);

      if (s->mode == TRUECOLOR) {
	/* software color pack */
	if (s->avdimen.line_difference > 0) {
	  int c, i;
	  int pixels = usefull_bytes / 3;
	  int c_offset = (s->avdimen.line_difference / 3) * bytes_per_line;
	  
	  for (c = 0; c < 3; ++ c)
	    for (i = 0; i < pixels; ++ i)
	      out_data [i * 3 + c] = stripe_data [i * 3 + (c * c_offset) + c];
	}
	else {
	  memcpy (out_data, stripe_data, usefull_bytes);
	}
      }
      else if (s->mode == GREYSCALE) {
	memcpy (out_data, stripe_data, usefull_bytes);
      }
      else {
	/* in singlebit mode, the avision scanners return 1 for black. ;-( --EUR */ 
	int i;
	for (i = 0; i < usefull_bytes; ++ i)
	  out_data[i] = ~ stripe_data [i];
      }
      
      /* I know that this is broken if not a multiple of bytes_per_line is in the buffer.
	 Shouldn't happen */
      fwrite (out_data, bytes_per_line, usefull_bytes / bytes_per_line, fp);
      
      /* cleanup stripe buffer */
      bytes_in_stripe -= usefull_bytes;
      if (bytes_in_stripe > 0)
	memcpy (stripe_data, stripe_data + usefull_bytes, bytes_in_stripe);
      
      s->line += usefull_bytes / bytes_per_line;
      DBG (5, "reader_process: end loop\n");
    } /* end while not all lines */
  
  if (dev->inquiry_new_protocol) {
    status = go_home(s);
    if (status != SANE_STATUS_GOOD)
      return status;
  }
  
  fclose (fp);
  
  free (stripe_data);
  free (out_data);
  
  status = release_unit (s);
  if (status != SANE_STATUS_GOOD)
    DBG (1, "release_unit failed\n");
  
  return 0;
}


static SANE_Status
attach_one (const char *dev)
{
  attach (dev, 0);
  return SANE_STATUS_GOOD;
}


SANE_Status
sane_init (SANE_Int* version_code, SANE_Auth_Callback authorize)
{
  /* char dev_name[PATH_MAX]; */
  FILE *fp;
  
  char line[PATH_MAX];
  const char* cp = 0;
  char* word;
  int linenumber = 0;

  authorize = authorize; /* silence gcc */

  DBG (3, "sane_init\n");

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
      linenumber++;
      
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
	  if (strcmp (word, "force-a4") == 0)
	    {
	      DBG(3, "sane_init: config file line %d: enabling force-a4\n",
		      linenumber);
	      force_a4 = SANE_TRUE;
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
  Avision_Device *dev, *next;

  DBG (3, "sane_exit\n");

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

  DBG (3, "sane_get_devices\n");

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

  DBG (3, "sane_open\n");

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
  memset (s, 0, sizeof (*s));
  s->fd = -1;
  s->pipe = -1;
  s->hw = dev;
  for (i = 0; i < 4; ++i)
    for (j = 0; j < 256; ++j)
      s->gamma_table[i][j] = j;


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
  Avision_Scanner *prev, *s;
  int i;

  DBG (3, "sane_close\n");
  DBG (3, " \n");

  /* remove handle from list of open handles: */
  prev = 0;
  for (s = first_handle; s; s = s->next) {
    if (s == handle)
      break;
    prev = s;
  }

  if (!s) {
    DBG (1, "close: invalid handle %p\n", handle);
    return;		/* oops, not a handle we know about */
  }

  if (s->scanning)
    do_cancel (handle);

  if (prev)
    prev->next = s->next;
  else
    first_handle = s->next;

  for (i = 1; i < NUM_OPTIONS; i++) {
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
  
  DBG (3, "sane_get_option_descriptor\n");

  if ((unsigned) option >= NUM_OPTIONS)
    return 0;
  return s->opt + option;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void* val, SANE_Int* info)
{
  Avision_Scanner* s = handle;
  SANE_Status status;
  SANE_Word cap;

  DBG (3, "sane_control_option\n");

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
    } /* end if GET_VALUE*/
  else if (action == SANE_ACTION_SET_VALUE)
    {
      if (!SANE_OPTION_IS_SETTABLE (cap))
	return SANE_STATUS_INVAL;
      
      status = constrain_value (s, option, val, info);
      if (status != SANE_STATUS_GOOD)
	return status;
      
      switch (option)
	{
	  /* (mostly) side-effect-free word options: */
	case OPT_RESOLUTION:
	case OPT_SPEED:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  /* fall through */
	case OPT_PREVIEW:
	case OPT_BRIGHTNESS:
	case OPT_CONTRAST:
	case OPT_QSCAN:    
	case OPT_QCALIB:
	case OPT_TRANS:
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
	  
	case OPT_MODE:
	  {
	    if (s->val[option].s)
	      free (s->val[option].s);
	    
	    s->val[option].s = strdup (val);
	    s->mode = make_mode (s->val[OPT_MODE].s);
	    
	    /* set to mode specific values */
	    
	    /* the gamma table related */
	    if (!disable_gamma_table)
	      {
		if (s->mode == TRUECOLOR) {
		  s->opt[OPT_GAMMA_VECTOR].cap   &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
		}
		else /* grey or mono */
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
	      s->hw->current_frame = frame;
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
  
  DBG (3, "sane_get_parameters\n");
          
  if (!s->scanning) 
    {
      s->avdimen.res = s->val[OPT_RESOLUTION].w;
      DBG (1, "res: %i\n", s->avdimen.res);
      
      /* the specs say they are specified in inch/1200 ... */
      s->avdimen.tlx = (double) 1200 * SANE_UNFIX (s->val[OPT_TL_X].w) / MM_PER_INCH;
      s->avdimen.tly = (double) 1200 * SANE_UNFIX (s->val[OPT_TL_Y].w) / MM_PER_INCH;
      s->avdimen.brx = (double) 1200 * SANE_UNFIX (s->val[OPT_BR_X].w) / MM_PER_INCH;
      s->avdimen.bry = (double) 1200 * SANE_UNFIX (s->val[OPT_BR_Y].w) / MM_PER_INCH;
      
      DBG (1, "tlx: %ld, tly: %ld, brx %ld, bry %ld\n", s->avdimen.tlx, s->avdimen.tly,
	   s->avdimen.brx, s->avdimen.bry);
      
      s->avdimen.tlx -= s->avdimen.tlx % dev->inquiry_color_boundary;
      s->avdimen.tly -= s->avdimen.tly % dev->inquiry_color_boundary;
      s->avdimen.brx -= s->avdimen.brx % dev->inquiry_color_boundary;
      s->avdimen.bry -= s->avdimen.bry % dev->inquiry_color_boundary;
      
      s->avdimen.width = (s->avdimen.brx - s->avdimen.tlx);
      s->avdimen.length = (s->avdimen.bry - s->avdimen.tly);
      
      s->avdimen.line_difference =  dev->inquiry_line_difference * s->avdimen.res / dev->inquiry_optical_res;
      
      DBG (1, "line_difference: %i\n", s->avdimen.line_difference);
      
      memset (&s->params, 0, sizeof (s->params));
      s->params.pixels_per_line = s->avdimen.width * s->avdimen.res / 1200;
      /* Needed for the AV 6[2,3]0 - they procude very poor quality in low
       * resolutions without this boundary */
      s->params.pixels_per_line -= s->params.pixels_per_line % 4;
      
      s->params.lines = s->avdimen.length * s->avdimen.res / 1200;
      
      DBG (1, "tlx: %ld, tly: %ld, brx %ld, bry %ld\n", s->avdimen.tlx, s->avdimen.tly,
	   s->avdimen.width, s->avdimen.length);
      
      DBG (1, "pixel_per_line: %i, lines: %i\n",
	   s->params.pixels_per_line, s->params.lines);
      
      switch (s->mode)
	{
	case THRESHOLDED:
	  {
	    s->params.pixels_per_line -= s->params.pixels_per_line % 32;
	    s->params.format = SANE_FRAME_GRAY;
	    s->params.bytes_per_line = s->params.pixels_per_line / 8;
	    s->params.depth = 1;
	    break;
	  }
	case DITHERED:
	  {
	    s->params.pixels_per_line -= s->params.pixels_per_line % 32;
	    s->params.format = SANE_FRAME_GRAY;
	    s->params.bytes_per_line = s->params.pixels_per_line / 8;
	    s->params.depth = 1;
	    break;
	  }
	case GREYSCALE:
	  {
	    s->params.format = SANE_FRAME_GRAY;
	    s->params.bytes_per_line = s->params.pixels_per_line;
	    s->params.depth = 8;
	    break;             
	  }
	case TRUECOLOR:
	  {
	    s->params.format = SANE_FRAME_RGB;
	    s->params.bytes_per_line =  s->params.pixels_per_line * 3;
	    s->params.depth = 8;
	    break;
	  }
	}
    }
  
  s->params.last_frame =  SANE_TRUE;
  
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
  
  DBG (3, "sane_start\n");

  /* First make sure there is no scan running!!! */
  
  if (s->scanning)
    return SANE_STATUS_DEVICE_BUSY;

  /* Second make sure we have a current parameter set.  Some of the
     parameters will be overwritten below, but that's OK.  */
  status = sane_get_parameters (s, 0);

  if (status != SANE_STATUS_GOOD) {
    return status;
  }
  
  if (s->fd < 0) {
    status = sanei_scsi_open (s->hw->sane.name, &s->fd, sense_handler, 0);
    if (status != SANE_STATUS_GOOD) {
      DBG (1, "sane_start: open of %s failed: %s\n",
	   s->hw->sane.name, sane_strstatus (status));
      return status;
    }
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
  
  if (dev->hw->scanner_type == AV_SHEETFEED) {
    status = check_paper(s);
    if (status != SANE_STATUS_GOOD) {
      DBG (1, "sane_start: check_paper() failed: %s\n", sane_strstatus (status));
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
  
  /* Only perform the calibration for newer scanners - it is not needed
     for my Avision AV 630 - and also not even work ... */
  
  if (dev->inquiry_new_protocol) {
    /* TODO: do the calibration here
     * If a) the scanner is not initialised
     *    b) the user asks for it?
     *    c) there is some change in the media b&w / colour?
     */
    if (!disable_calibration)
      {
	status = perform_calibration (s);
	if (status != SANE_STATUS_GOOD) {
	  DBG (1, "sane_start: perform calibration failed: %s\n",
	       sane_strstatus (status));
	  goto stop_scanner_and_return;
	}
      }
    else
      DBG (1, "sane_start: calibration disabled - skipped\n");
  }
  
  if (!disable_gamma_table)
    {
      status = set_gamma (s);
      if (status != SANE_STATUS_GOOD) {
	DBG (1, "sane_start: set gamma failed: %s\n",
	     sane_strstatus (status));
	goto stop_scanner_and_return;
      }
    }
  else
    DBG (1, "sane_start: gamma-table disabled - skipped\n");
  
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
  
  /* cancel the scan nicely and do a go_home for the new_protocol */
  do_cancel (s);
  
  return status;
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte* buf, SANE_Int max_len, SANE_Int* len)
{
  Avision_Scanner* s = handle;
  ssize_t nread;

  DBG (3, "sane_read\n");

  *len = 0;

  nread = read (s->pipe, buf, max_len);
  DBG (3, "sane_read: read %d bytes\n", nread);

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

  DBG (3, "sane_cancel\n");
  
  if (s->scanning)
    do_cancel (s);
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  Avision_Scanner* s = handle;
  
  DBG (3, "sane_set_io_mode\n");
  if (!s->scanning)
    return SANE_STATUS_INVAL;
  
  if (fcntl (s->pipe, F_SETFL, non_blocking ? O_NONBLOCK : 0) < 0)
    return SANE_STATUS_IO_ERROR;
  
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int *fd)
{
  Avision_Scanner *s = handle;

  DBG (3, "sane_get_select_fd\n");

  if (!s->scanning)
    return SANE_STATUS_INVAL;

  *fd = s->pipe;
  return SANE_STATUS_GOOD;
}
