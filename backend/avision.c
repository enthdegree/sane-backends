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
                "Martin Jelínek" <mates@sirrah.troja.mff.cuni.cz>
   
   Additional Contributers:
                "Gunter Wagner"
                  (some fixes and the transparency option)
   
   Very much thanks to:
                Avision INC for the documentation we got! ;-)
   
   ChangeLog:
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
   
   2001-12-11: Martin Jelínek
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
#define BACKEND_BUILD 15  /* avision backend BUILD version */

Avision_HWEntry Avision_Device_List  [] =
  { {"AVISION", "AV630CS",       0},
    {"AVISION", "AV620CS",       0},
    {"AVISION", "AV 6240",       0},
    {"HP",      "ScanJet 5300C", 1},
    {"HP",      "ScanJet 5370C", 1},
    {"HP",      "ScanJet 5470C", 1},
    {"hp",      "scanjet 7400c", 1},
    {"MINOLTA", "FS-V1",         1},
    {NULL,      NULL,           -1} }; /* last one detection */

#define A4_X_RANGE 8.5   /* used when scanner returns invalid range fields ... */
#define A4_Y_RANGE 11.8

#ifndef PATH_MAX
# define PATH_MAX 1024
#endif

#define AVISION_CONFIG_FILE "avision.conf"

#define MM_PER_INCH (25.4)

static int num_devices;
static Avision_Device* first_dev;
static Avision_Scanner* first_handle;

static SANE_Bool disable_gamma_table = SANE_FALSE; /* disable the usage of a custom gamma-table */
static SANE_Bool force_a4 = SANE_FALSE; /* force scanable areas to ISO(DIN) A4 */
static SANE_Bool allow_usb = SANE_FALSE; /* allow USB scanners */

static const SANE_String_Const mode_list[] =
  {
    "Line Art", "Dithered", "Gray", "Color", 0
  };

/* avision_res will be overwritten in init_options() !!! */

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

static int make_mode (char *mode)
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
wait_ready (int fd)
{
  SANE_Status status;
  int i;
  
  for (i = 0; i < 1000; ++i)
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
	  usleep (100000);	/* retry after 100ms */
	  break;

	case SANE_STATUS_GOOD:
	  return status;
	}
    }
  DBG (1, "wait_ready: timed out after %d attempts\n", i);
  return SANE_STATUS_INVAL;
}


static SANE_Status
sense_handler (int fd, u_char* sense, void* arg)
{
  /*MCC*/
  
  int i;
  
  SANE_Status status;
  
  SANE_Bool ASC_switch;
  
  DBG (3, "sense_handler\n");
  
  ASC_switch = SANE_FALSE;
  
  switch (sense[0])
    {
    case 0x00:
      status = SANE_STATUS_GOOD;
	    
    default:
      DBG (1, "sense_handler: got unknown sense code %02x\n", sense[0]);
      status = SANE_STATUS_IO_ERROR;
    }
  
  for (i = 0; i < 21; i++)
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
attach (const char* devname, Avision_Device** devp)
{
  unsigned char result [INQ_LEN];
  int fd;
  Avision_Device* dev;
  SANE_Status status;
  size_t size;

  char mfg [9];
  char model [17];
  char rev [5];
  
  unsigned int i;
  SANE_Bool found;

  DBG (3, "attach:\n");

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
    sanei_scsi_close (fd);
    return status;
  }

  status = wait_ready (fd);
  sanei_scsi_close (fd);
  if (status != SANE_STATUS_GOOD)
    return status;
  
  /* copy string information - and zero terminate them c-style */
  memcpy (&mfg, result + 8, 8);
  mfg [8] = 0;
  memcpy (&model, result + 16, 16);
  model [16] = 0;
  memcpy (&rev, result + 32, 4);
  rev [4] = 0;
  
  /* shorten strings ( -1 for last index; -1 for last 0; >0 because one char at least)) */
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
       &mfg, &model, &rev);
  
  i = 0;
  found = 0;
  while (Avision_Device_List [i].mfg != NULL) {
    if (strcmp (mfg, Avision_Device_List [i].mfg) == 0 && 
	(strcmp (model, Avision_Device_List [i].model) == 0) ) {
      found = 1;
      DBG (1, "FOUND\n");
      break;
    }
    ++i;
  }
  
  if (!found) {
    DBG (1, "attach: model is not in the list of supported model!\n");
    DBG (1, "attach: You might want to report this output.\n");
    return SANE_STATUS_INVAL;
  }
  
  if (Avision_Device_List [i].usb && !allow_usb) {
    DBG (1, "attach: The attached scanner is a USB model are not suported, yet!\n");
    DBG (1, "attach: Use the allow-usb option in the config file to try anyway ...\n");
    return SANE_STATUS_INVAL;
  }
  
  dev = malloc (sizeof (*dev));
  if (!dev)
    return SANE_STATUS_NO_MEM;
  
  memset (dev, 0, sizeof (*dev));
  
  dev->sane.name   = strdup (devname);
  dev->sane.vendor = strdup (mfg);
  dev->sane.model  = strdup (model);

  dev->is_usb = Avision_Device_List [i].usb;
  
  DBG (6, "RAW-Data:\n");
  for (i=0; i<sizeof(result); i++) {
    DBG (6, "result [%2d] %1d%1d%1d%1d%1d%1d%1d%1db %3oo %3dd %2xx\n", i, BIT(result[i],7), 
         BIT(result[i],6), BIT(result[i],5), BIT(result[i],4), BIT(result[i],3), BIT(result[i],2),
         BIT(result[i],1), BIT(result[i],0), result[i], result[i], result[i]);
  }
  
  DBG (3, "attach: [8-15]  Vendor id.:      \"%8.8s\"\n", result+8);
  DBG (3, "attach: [16-31] Product id.:     \"%8.8s\"\n", result+16);
  DBG (3, "attach: [32-35] Product rev.:    \"%4.4s\"\n", result+32);
  
  i = (result[39] >> 4) & 0x7;
  DBG (3, "attach: [36]    Bitfield:%s%s%s%s%s%s%s\n",
       BIT(result[36],7)?" ADF":"",
       (i==0)?" B&W only":"",
       BIT(i, 1)?" 3-pass color":"",
       BIT(i, 2)?" 1-pass color":"",
       BIT(i, 3) && BIT(i, 1) ?" 1-pass color (ScanPartner only)":"",
       BIT(result[36],3)?" IS_NOT_FLATBED:":"",
       (result[36] & 0x7) == 0 ? " RGB_COLOR_PLANE":"RESERVED????!!");
  
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
  	BIT(result[62],7)?" Flatbed (w.o. ADF??)":"", /* not really in spec but resonable */
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
  DBG (3, "                (FUBA: not yet recognized unit ...)\n", (result[87]<<8)+result[88]);
  
  DBG (3, "attach: [89-90] Res. in Ex. mode:      %d dpi\n", (result[89]<<8)+result[90]);
  
  DBG (3, "attach: [92]    Buttons:  %d\n", result[92]);
  
  DBG (3, "attach: [93]    ESA4:%s%s%s%s%s%s%s%s\n",
  	BIT(result[51],7)?" SUPP_ACCESSORIES_DETECT":"",
        BIT(result[51],6)?" ADF_IS_BGR_ORDERED":"",
        BIT(result[51],5)?" NO_SINGLE_CHANNEL_GRAY_MODE":"",
        BIT(result[51],4)?" SUPP_FLASH_UPDATE":"",
  	BIT(result[51],3)?" SUPP_ASIC_UPDATE":"",
  	BIT(result[51],2)?" SUPP_LIGHT_DETECT":"",
        BIT(result[51],1)?" SUPP_READ_PRNU_DATA":"",
        BIT(result[51],0)?" SCAN_FLATBED_MIRRORED":"");
  
  if (BIT(result[62],3) ) {
    /* The coolscan backend says "slide scanner", the canon says "film scanner"
       deciding on a name would be useful? */
    dev->sane.type   = "slide scanner";
  }
  else {
    dev->sane.type   = "flatbed scanner";
  }
  
  dev->inquiry_new_protocol = BIT(result[39],2);
  dev->inquiry_needs_calibration = BIT(result[50],4);
  dev->inquiry_needs_gamma = BIT(result[50],3);
  dev->inquiry_needs_software_colorpack = BIT(result[50],5);
  
  dev->inquiry_grey_res = ((result[40]<<8)+result[41]);
  dev->inquiry_color_res = ((result[44]<<8)+result[45]);
  
  /* Get max X and max Y ...*/
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
  
  if (!dev->inquiry_new_protocol) {
    dev->inquiry_x_range /= 300;
    dev->inquiry_y_range /= 300;
    dev->inquiry_adf_x_range /= 300;
    dev->inquiry_adf_y_range /= 300;
    dev->inquiry_transp_x_range /= 300;
    dev->inquiry_transp_y_range /= 300;
  } else {
    dev->inquiry_x_range /= dev->inquiry_color_res;
    dev->inquiry_y_range /= dev->inquiry_color_res;
    dev->inquiry_adf_x_range /= dev->inquiry_color_res;
    dev->inquiry_adf_y_range /= dev->inquiry_color_res;
    dev->inquiry_transp_x_range /= dev->inquiry_color_res;
    dev->inquiry_transp_y_range /= dev->inquiry_color_res;
  }
  
  /* check if x/y range is vaild :-((( */
  if (dev->inquiry_x_range == 0 || dev->inquiry_y_range == 0) {
    DBG (1, "Inquiry x/y-range is invaild! Using defauld %fx%finch (ISO A4).\n", A4_X_RANGE, A4_Y_RANGE);
    dev->inquiry_x_range = A4_X_RANGE * MM_PER_INCH;
    dev->inquiry_y_range = A4_Y_RANGE * MM_PER_INCH;
  }
  else
    DBG (3, "Inquiry x/y-range seems to be vaild!\n");
  
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
 
  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;
  
  /* TEMP HACKING */
  /*
  DBG (3, "attach: reopening %s\n", devname);
  status = sanei_scsi_open (devname, &fd, sense_handler, 0);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "attach: open failed (%s)\n", sane_strstatus (status));
    return SANE_STATUS_INVAL;
  }
  status = test_hacks (fd);
  
  status = wait_ready (fd);
  sanei_scsi_close (fd);
  if (status != SANE_STATUS_GOOD)
    return status;
  
  */
  
  if (devp)
    *devp = dev;
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
perform_calibration (Avision_Scanner *s)
{
  /* read stuff */
  struct command_read rcmd;
  size_t nbytes;
  SANE_Status status;
  
  unsigned int i;
  /* unsigned int color; */
  
  unsigned int CALIB_PIXELS_PER_LINE;
  unsigned int CALIB_BYTES_PER_CHANNEL;
  unsigned int CALIB_LINE_COUNT;
  
  SANE_Byte result [16];
  SANE_Byte* data;
  
  /* send stuff */
  struct command_send scmd;
  unsigned char *cmd;  
  
  DBG (3, "test get calibration format.\n");

  nbytes = sizeof (result);
  
  memset (&rcmd, 0, sizeof (rcmd));
  rcmd.opc = AVISION_SCSI_READ;
  
  rcmd.datatypecode = 0x60; /* get calibration info */
  /* rcmd.calibchn = 0; */
  rcmd.datatypequal [0] = 0x0d;
  rcmd.datatypequal [1] = 0x0a;
  set_triple (rcmd.transferlen, nbytes);
  
  DBG (3, "read_data: bytes %d\n", nbytes);
  status = sanei_scsi_cmd (s->fd, &rcmd, sizeof (rcmd), result, &nbytes);
  if (status != SANE_STATUS_GOOD || nbytes != sizeof (result)) {
    DBG (1, "attach: inquiry failed (%s)\n", sane_strstatus (status));
    return status;
  }
  
  DBG (6, "RAW-Data:\n");
  for (i=0; i<nbytes; i++) {
    DBG (6, "result [%2d] %1d%1d%1d%1d%1d%1d%1d%1db %3oo %3dd %2xx\n", i, BIT(result[i],7), 
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
  
  CALIB_PIXELS_PER_LINE = ( (result[0]<<8)+result[1] );
  CALIB_BYTES_PER_CHANNEL = result[2];
  CALIB_LINE_COUNT = result[3];
  
  /* try to get calibration data */
  
  nbytes = CALIB_PIXELS_PER_LINE * CALIB_BYTES_PER_CHANNEL * CALIB_LINE_COUNT; /* *3 for three channels does not work on an av630cs ?? */
  
  data = malloc (nbytes);
  if (!data)
    return SANE_STATUS_NO_MEM;
  
  memset (&rcmd, 0, sizeof (rcmd));
  rcmd.opc = AVISION_SCSI_READ;
  
  rcmd.datatypecode = 0x62; /* 66: dark, 62: color data */
  /* rcmd.calibchn = color; */
  rcmd.datatypequal [0] = 0x0d;
  rcmd.datatypequal [1] = 0x0a;
  set_triple (rcmd.transferlen, nbytes);
  
  DBG (3, "read_data: %d bytes calibration data\n", nbytes);
  
  status = sanei_scsi_cmd (s->fd, &rcmd, sizeof (rcmd), data, &nbytes);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "attach: calibration data read failed (%s)\n", sane_strstatus (status));
    return status;
  }
  
  DBG (10, "RAW-Calibration-Data (%d bytes):\n", nbytes);
  for (i=0; i<nbytes; i++) {
    DBG (10, "data [%2d] %3d\n", i, data[i]);
  }
  
  /* sending some calibration data ... */
  
  nbytes = CALIB_PIXELS_PER_LINE * CALIB_BYTES_PER_CHANNEL * 3;
  
  memset (&scmd, 0, sizeof (scmd));
  scmd.opc = AVISION_SCSI_SEND;
  scmd.datatypecode = 0x82; /* send calibration data */
  set_double (scmd.datatypequal, 0x12); /* send color-calib. data */
  set_triple (scmd.transferlen, nbytes);
  
  cmd = malloc (sizeof (scmd) + nbytes);
  if (!cmd)
    return SANE_STATUS_NO_MEM;
  
  /* build cmd */
  memset (cmd, 0, sizeof (scmd) + nbytes);
  memcpy (cmd, &scmd, sizeof (scmd));
  /*memcpy (cmd + sizeof (scmd), data, nbytes);*/
  
  DBG (3, "send_data: %d bytes (+header) calibration data\n", nbytes);
  
  /*for (i=0; i<nbytes; i++) {
    DBG (3, "cmd [%2d] %2xh\n", i, cmd[i]);
  }*/
  
  status = sanei_scsi_cmd (s->fd, cmd, sizeof (scmd) + nbytes, 0, 0);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "attach: send_data (%s)\n", sane_strstatus (status));
    return status;
  }

  free (cmd);
  free (data);
  
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
#define GAMMA_TABLE_SIZE 4096

  SANE_Status status;
  struct gamma_cmd
  {
    struct command_send cmd;
    unsigned char gamma_data [GAMMA_TABLE_SIZE];
  };
  
  struct gamma_cmd* cmd;
  
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
  
  /* should we sent a custom gamma-table ?? */
  if (! disable_gamma_table)
    {
      DBG (4, "gamma-table is used\n");
      for (color = 0; color < 3; color++)
	{
	  cmd = malloc (sizeof (*cmd) );
	  if (!cmd)
	    return SANE_STATUS_NO_MEM;
	  
	  memset (cmd, 0, sizeof (*cmd) );
	  
	  cmd->cmd.opc = AVISION_SCSI_SEND;
	  cmd->cmd.datatypecode = 0x81; /* 0x81 for download gama table */
	  set_double (cmd->cmd.datatypequal, color); /* color: 0=red; 1=green; 2=blue */
	  set_triple (cmd->cmd.transferlen, GAMMA_TABLE_SIZE);
	  
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
	      /* emulate brightness, contrast ...
	       * taken from the GIMP source - I'll optimize it when it is
	       * known to work
	       */
	      
	      v1 /= 255;
	      v2 /= 255;
	      
	      v1 = (brightness_contrast_func (brightness, contrast, v1) );
	      v2 = (brightness_contrast_func (brightness, contrast, v2) );
	      
	      v1 *= 255;
	      v2 *= 255;
	      for (k = 0; k < 8; k++, i++)
		cmd->gamma_data [i] = ( ( (unsigned char)v1 * (8 - k)) + ( (unsigned char)v2 * k) ) / 8;
	    }
	  /* fill the gamma table */
	  for (i = 2048; i < 4096; i++)
	    cmd->gamma_data [i] = cmd->gamma_data [2047];
	  
	  
	  /* HP test hack */
	  /*
	    for (i = 0; i < 2048; i++)
	    cmd->gamma_data [2048 + i] = cmd->gamma_data [i];
	  */
	  status = sanei_scsi_cmd (s->fd, cmd, sizeof (*cmd), 0, 0);
	  
	  free (cmd);
	}
    } /* end if not disable_gamma_table */
  else /* disable_gamma_table */
    {
      DBG (4, "gamma-table is disabled -> not used\n");
      status = SANE_STATUS_GOOD;
    }
  
  return status;
}

static SANE_Status
set_window (Avision_Scanner* s)
{
  SANE_Status status;
  struct
  {
    struct command_set_window cmd;
    struct command_set_window_window_header window_header;
    struct command_set_window_window_descriptor window_descriptor;
  } cmd;
   
  DBG (3, "set_windows\n");

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
     
  /* upper left corner coordinates */
  set_quad (cmd.window_descriptor.ulx, s->avdimen.tlx);
  set_quad (cmd.window_descriptor.uly, s->avdimen.tly);

  /* width and length in inch/1200 */
  set_quad (cmd.window_descriptor.width, s->avdimen.wid);
  set_quad (cmd.window_descriptor.length, s->avdimen.len);

  /* width and length in bytes */
  set_double (cmd.window_descriptor.linewidth, s->params.bytes_per_line );
  set_double (cmd.window_descriptor.linecount, s->params.lines );

  cmd.window_descriptor.bitset1 = 0x60;
  cmd.window_descriptor.bitset1 |= s->val[OPT_SPEED].w;
    
  cmd.window_descriptor.bitset2 = 0x00;

  /* quality scan option switch */
  if( s->val[OPT_QSCAN].w == SANE_TRUE )
    {
      cmd.window_descriptor.bitset2 |= AV_QSCAN_ON; /* Q_SCAN ON */
    }

  /* quality calibration option switch */
  if ( s->val[OPT_QCALIB].w == SANE_TRUE )
    {
      cmd.window_descriptor.bitset2  |= AV_QCALIB_ON;  /* Q_CALIB ON */
    }
  /* transparency switch */
  if ( s->val[OPT_TRANS].w == SANE_TRUE )
    {
      cmd.window_descriptor.bitset2 |= AV_TRANS_ON; /* Set to transparency mode */
    }
    
  /* fixed value
   */
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
      cmd.window_descriptor.bpp = 1;
      cmd.window_descriptor.image_comp = 0;
      cmd.window_descriptor.bitset1 &= 0xC7;
      break;
	
    case DITHERED:
      cmd.window_descriptor.bpp = 1;
      cmd.window_descriptor.image_comp = 1;
      cmd.window_descriptor.bitset1 &= 0xC7;
      break;
	
    case GREYSCALE:
      cmd.window_descriptor.bpp = 8;
      cmd.window_descriptor.image_comp = 2;
      cmd.window_descriptor.bitset1 &= 0xC7;
      /* cmd.window_descriptor.bitset1 |= 0x30; */ /* what is it for Gunter ?? - doesn't work */
      break;
	
    case TRUECOLOR:
      cmd.window_descriptor.bpp = 8;
      cmd.window_descriptor.image_comp = 5;
      break;
	
    default:
      DBG (3, "Invalid mode. %d\n", s->mode);
      return SANE_STATUS_INVAL;
    }

  /* set window command
   */
  status = sanei_scsi_cmd (s->fd, &cmd, sizeof (cmd), 0, 0);

  /* back to caller 
   */
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

static SANE_Status
wait_4_light (Avision_Scanner *s)
{
  /* read stuff */
  struct command_read rcmd;
  char *light_status[] = { "off", "on", "warming up", "needs warm up test", 
		           "light check error", "RESERVED" };
  
  size_t nbytes;
  SANE_Status status;
  SANE_Byte result;

  nbytes = 1;
 
  memset (&rcmd, 0, sizeof (rcmd));
  rcmd.opc = AVISION_SCSI_READ;

  rcmd.datatypecode = 0xa0; /* get light status */
  
  DBG (3, "getting light status.\n");

  rcmd.datatypequal [0] = 0x0d;
  rcmd.datatypequal [1] = 0x0a;
  set_triple (rcmd.transferlen, nbytes);
  
  DBG (3, "read_data: bytes %d\n", nbytes);
  
  do {
    status = sanei_scsi_cmd (s->fd, &rcmd, sizeof (rcmd), &result, &nbytes);
  
    if (status != SANE_STATUS_GOOD || nbytes != sizeof (result)) {
      DBG (1, "test_light: inquiry failed (%s)\n", sane_strstatus (status));
      return status;
    }
    
    DBG (1, "Light status: command is %d. Result is %s\n", status, light_status[(result>4)?5:result]);
    if (result != 1) sleep (3);
  } while (result !=1);
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
go_home (Avision_Scanner *s)
{
  SANE_Status status;
  
  /* for film scanners! Check adf's as well */
  char cmd[] =
    {0x32, 0, 0x02, 0, 0, 0};/* Object position */
  
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

  return SANE_STATUS_EOF;
}

static SANE_Status
do_cancel (Avision_Scanner *s)
{
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
      /* release the device ? */
      /* go_home (s); */
      
      sanei_scsi_close (s->fd);
      s->fd = -1;
    }

  return SANE_STATUS_CANCELLED;
}

static SANE_Status
read_data (Avision_Scanner* s, SANE_Byte* buf, int lines, int bpl)
{
  struct command_read rcmd;
  size_t nbytes;
  SANE_Status status;

  DBG (3, "read_data\n");
  
  nbytes = bpl * lines;
  memset (&rcmd, 0, sizeof (rcmd));
  rcmd.opc = AVISION_SCSI_READ;
  rcmd.datatypecode = 0x00; /* read image data */
  rcmd.datatypequal [0] = 0x0d;
  rcmd.datatypequal [1] = 0x0a;
  set_triple (rcmd.transferlen, nbytes);
  
  DBG (3, "read_data: bytes %d\n", nbytes );

  status = sanei_scsi_cmd (s->fd, &rcmd, sizeof (rcmd), buf, &nbytes);
  
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
  
  /* init the SANE option from the scanner inquiry data */
  
  dev->x_range.max = SANE_FIX ( (int)dev->inquiry_x_range);
  dev->x_range.quant = 0;
  dev->y_range.max = SANE_FIX ( (int)dev->inquiry_y_range);
  dev->y_range.quant = 0;
  
  dev->dpi_range.min = 50;
  dev->dpi_range.quant = 1;
  dev->dpi_range.max = dev->inquiry_color_res;
  
  dev->speed_range.min = (SANE_Int)0;
  dev->speed_range.max = (SANE_Int)4;
  dev->speed_range.quant = (SANE_Int)1;
  
  s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].desc = "";
  s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  s->val[OPT_NUM_OPTS].w = NUM_OPTIONS;
  
  /* "Mode" group: */
  s->opt[OPT_MODE_GROUP].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_MODE_GROUP].desc = ""; /* for groups only title and type are vaild */
  s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MODE_GROUP].cap = 0;
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

  /* "Geometry" group: */
  s->opt[OPT_GEOMETRY_GROUP].title = "Geometry";
  s->opt[OPT_GEOMETRY_GROUP].desc = ""; /* for groups only title and type are vaild */
  s->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;
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
  s->opt[OPT_ENHANCEMENT_GROUP].desc = ""; /* for groups only title and type are vaild */
  s->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ENHANCEMENT_GROUP].cap = 0;
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
  SANE_Byte *data;
  int lines_per_buffer, bpl;
  SANE_Status status;
  sigset_t sigterm_set;
  FILE *fp;

  DBG (3, "reader_process\n");

  sigemptyset (&sigterm_set);
  sigaddset (&sigterm_set, SIGTERM);

  fp = fdopen (fd, "w");
  if (!fp)
    return 1;

  bpl = s->params.bytes_per_line;
  /* the "/2" is a test if scanning gets a bit faster ... ?!? ;-) 
     (see related discussions on sane-ml some years ago) */
  lines_per_buffer = sanei_scsi_max_request_size / bpl / 2;
  if (!lines_per_buffer)
    return 2;			/* resolution is too high */

  /* Limit the size of a single transfer to one inch. 
     XXX Add a stripsize option. */
  if (lines_per_buffer > s->val[OPT_RESOLUTION].w )
    lines_per_buffer = s->val[OPT_RESOLUTION].w;

  DBG (3, "lines_per_buffer=%d, bytes_per_line=%d\n", lines_per_buffer, bpl);

  data = malloc (lines_per_buffer * bpl);

  for (s->line = 0; s->line < s->params.lines; s->line += lines_per_buffer) {
    if (s->line + lines_per_buffer > s->params.lines)
      /* do the last few lines: */
      lines_per_buffer = s->params.lines - s->line;
	 
    sigprocmask (SIG_BLOCK, &sigterm_set, 0);
    status = read_data (s, data, lines_per_buffer, bpl);
    sigprocmask (SIG_UNBLOCK, &sigterm_set, 0);
    if (status != SANE_STATUS_GOOD) {
      DBG (1, "reader_process: read_data failed with status=%d\n", status);
      return 3;
    }
    DBG (3, "reader_process: read %d lines\n", lines_per_buffer);
    
    if ((s->mode == TRUECOLOR) || (s->mode == GREYSCALE)) 
      {
	fwrite (data, lines_per_buffer, bpl, fp);
      } 
    else 
      {
	/* in singlebit mode, the scanner returns 1 for black. ;-( --EUR */
	int i;
      
	for (i = 0; i < lines_per_buffer * bpl; ++i)
	  {
	    fputc (~data[i], fp);
	  }
      }
  }
  fclose (fp);

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
  char dev_name[PATH_MAX];
  size_t len;
  FILE *fp;
  
  char line[PATH_MAX];
  const char* cp = 0;
  char* word = 0;
  int linenumber = 0;
  
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
      word = 0;
      linenumber++;
      
      DBG(5, "sane_init: parsing config line \"%s\"\n",
	  line);
      
      cp = sanei_config_get_string (line, &word);
      
      if (!word || cp == line)
	{
	  DBG(5, "sane_init: config file line %d: ignoring empty line\n",
	      linenumber);
	  continue;
	}
      if (word[0] == '#')
	{
	  DBG(5, "sane_init: config file line %d: ignoring comment line\n",
	      linenumber);
	  free (word);
	  word = 0;
	  continue;
	}
                    
      if (strcmp (word, "option") == 0)
      	{
	  free (word);
	  word = 0;
	  cp = sanei_config_get_string (cp, &word);
	  
	  if (strcmp (word, "disable-gamma-table") == 0)
	    {
	      DBG(3, "sane_init: config file line %d: disable-gamma-table\n",
		      linenumber);
	      disable_gamma_table = SANE_TRUE;
	    }
	  if (strcmp (word, "force-a4") == 0)
	    {
	      DBG(3, "sane_init: config file line %d: enabling force-a4\n",
		      linenumber);
	      force_a4 = SANE_TRUE;
	    }
	  if (strcmp (word, "allow-usb") == 0)
	    {
	      DBG(3, "sane_init: config file line %d: enabling allow-usb\n",
		      linenumber);
	      allow_usb = SANE_TRUE;
	    }
	  if (word)
	    free (word);
	  word = 0;
	}
      else
	{
	  DBG(4, "sane_init: config file line %d: trying to attach `%s'\n",
	      linenumber, line);
	  
	  sanei_config_attach_matching_devices (line, attach_one);
	  if (word)
	    free (word);
	  word = 0;
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
    free ((void *) dev->sane.name);
    free ((void *) dev->sane.model);
    free (dev);
  }
}

SANE_Status
sane_get_devices (const SANE_Device ***device_list, SANE_Bool local_only)
{
  static const SANE_Device **devlist = 0;
  Avision_Device *dev;
  int i;

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
  Avision_Device *dev;
  SANE_Status status;
  Avision_Scanner *s;
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

  free (handle);
}


const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  Avision_Scanner *s = handle;
  
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
	}
    }
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
	}
    }
  return SANE_STATUS_INVAL;
}

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters *params)
{
  Avision_Scanner *s = handle;
     
  DBG (3, "sane_get_parameters\n");
          
  if (!s->scanning) 
    {
      double tlx,tly,brx,bry,res;
      
      tlx = 1200 * 10.0 * SANE_UNFIX(s->val[OPT_TL_X].w)/254.0;
      tly = 1200 * 10.0 * SANE_UNFIX(s->val[OPT_TL_Y].w)/254.0;
      brx = 1200 * 10.0 * SANE_UNFIX(s->val[OPT_BR_X].w)/254.0;
      bry = 1200 * 10.0 * SANE_UNFIX(s->val[OPT_BR_Y].w)/254.0;
      res = s->val[OPT_RESOLUTION].w;
          
      s->avdimen.tlx = tlx;
      s->avdimen.tly = tly;
      s->avdimen.brx = brx;
      s->avdimen.bry = bry;
      s->avdimen.res = res;
      s->avdimen.wid = ((s->avdimen.brx-s->avdimen.tlx)/4)*4;  
      s->avdimen.len = ((s->avdimen.bry-s->avdimen.tly)/4)*4;  
      s->avdimen.pixelnum = (( s->avdimen.res * s->avdimen.wid ) / 4800)*4;
      s->avdimen.linenum  = (( s->avdimen.res * s->avdimen.len ) / 4800)*4;
      
      DBG (1, "tlx: %f, tly: %f, brx %f, bry %f\n", tlx, tly, brx, bry);
      
      if (s->avdimen.tlx == 0)
	{
	  s->avdimen.tlx +=  4;
	  s->avdimen.wid -=  4;
	}
      s->avdimen.tlx =  (s->avdimen.tlx / 4) * 4;

      if (s->avdimen.tly == 0)
	{
	  s->avdimen.tly +=  4;
	}

      s->params.pixels_per_line = s->avdimen.pixelnum;
      s->params.lines           = s->avdimen.linenum;

      memset (&s->params, 0, sizeof (s->params));
  
      if (s->avdimen.res > 0 && s->avdimen.wid > 0 && s->avdimen.len > 0) 
	{
	  s->params.pixels_per_line = s->avdimen.pixelnum;
	  s->params.lines = s->avdimen.linenum;

	}
      switch (s->mode)
	{
	case THRESHOLDED:
	  {
	    s->params.format = SANE_FRAME_GRAY;                   
	    s->avdimen.pixelnum = (s->avdimen.pixelnum / 32) * 32;
	    s->params.pixels_per_line = s->avdimen.pixelnum;
	    s->params.bytes_per_line = s->avdimen.pixelnum /8;
	    s->params.depth = 1;
	    break;
	  }
	case DITHERED:
	  {
	    s->params.format = SANE_FRAME_GRAY;
	    s->avdimen.pixelnum = (s->avdimen.pixelnum / 32) * 32;
	    s->params.pixels_per_line = s->avdimen.pixelnum;
	    s->params.bytes_per_line = s->avdimen.pixelnum /8;
	    s->params.depth = 1;
	    break;
	  }
	case GREYSCALE:
	  {
	    s->params.format = SANE_FRAME_GRAY;
	    s->params.bytes_per_line  = s->avdimen.pixelnum;
	    s->params.pixels_per_line = s->avdimen.pixelnum;
	    s->params.depth = 8;
	    break;             
	  }
	case TRUECOLOR:
	  {
	    s->params.format = SANE_FRAME_RGB;
	    s->params.bytes_per_line = s->avdimen.pixelnum * 3;
	    s->params.pixels_per_line = s->avdimen.pixelnum;
	    s->params.depth = 8;
	    break;
	  }
	}
    }
     
  s->params.last_frame =  SANE_TRUE;
     
  if (params)
    {
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

  /* Fisrt make sure there is no scan running!!! */
  
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
      DBG (1, "open: open of %s failed: %s\n",
	   s->hw->sane.name, sane_strstatus (status));
      return status;
    }
  }
  
  /* first reserve unit */
  status = reserve_unit (s);
  if (status != SANE_STATUS_GOOD)
    DBG (1, "reserve_unit failed\n");
  
  status = wait_ready (s->fd);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "open: wait_ready() failed: %s\n", sane_strstatus (status));
    goto stop_scanner_and_return;
  }
  
  wait_4_light (s);
  
  status = set_window (s);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "open: set scan window command failed: %s\n",
	 sane_strstatus (status));
    goto stop_scanner_and_return;
  }
  
  if (dev->inquiry_new_protocol) {
    /* TODO: do the calibration here
     * If a) the scanner is not initialised
     *    b) the use asks for it?
     *    d) there is some change in the media b&w / colour?
     * perform_calibration(s->fd);
     */
  }
  
  status = set_gamma (s);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "open: set gamma failed: %s\n",
	 sane_strstatus (status));
    goto stop_scanner_and_return;
  }
  
  s->scanning = SANE_TRUE;

  status = start_scan (s);
  if (status != SANE_STATUS_GOOD) {
    goto stop_scanner_and_return;
  }
  
  s->line = 0;

  if (pipe (fds) < 0)
    {
      return SANE_STATUS_IO_ERROR;
    }

  s->reader_pid = fork ();
  if (s->reader_pid == 0) 
    {
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
  
  if (dev->inquiry_new_protocol) {
    status = go_home(s);
    if (status != SANE_STATUS_GOOD)
      return status;
  }
  
  return SANE_STATUS_GOOD;

 stop_scanner_and_return:
  do_cancel (s);
  
  if (dev->inquiry_new_protocol) {
    status = go_home(s);
    if (status != SANE_STATUS_GOOD)
      return status;
  }
  
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
  DBG (3, "sane_read:read %ld bytes\n", (long) nread);

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
