/*.............................................................................
 * Project : SANE library for Plustek flatbed scanners.
 *.............................................................................
 * File:	 plustek.c - the SANE backend for the plustek driver
 *.............................................................................
 *
 * based on Kazuhiro Sasayama previous
 * Work on plustek.[ch] file from the SANE package.
 * original code taken from sane-0.71
 * Copyright (C) 1997 Hypercore Software Design, Ltd.
 * also based on the work done by Rick Bronson
 * Copyright (C) 2000 Gerhard Jaeger <g.jaeger@earthling.net>
 *.............................................................................
 * History:
 * 0.30 - initial version
 * 0.31 - no changes
 * 0.32 - no changes
 * 0.33 - no changes
 * 0.34 - moved some definitions and typedefs to plustek.h
 * 0.35 - removed Y-correction for 12000P model
 *        getting Y-size of scan area from driver
 * 0.36 - disabled Dropout, as this does currently not work
 *        enabled Halftone selection only for Halftone-mode
 *        made the cancel button work by using a child process during read
 *        added version code to driver interface
 *        cleaned up the code
 *        fixed sane compatibility problems
 *        added multiple device support
 *        12bit color-depth are now available for scanimage
 * 0.37 - removed X/Y autocorrection, now correcting the stuff
 *        before scanning
 *        applied Michaels' patch to solve the sane_get_parameter problem
 *        getting X-size of scan area from driver
 *        applied Michaels´ patch for OPT_RESOLUTION (SANE_INFO_INEXACT stuff)
 *
 *.............................................................................
 *
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 *
 * As a special exception, the authors of SANE give permission for
 * additional uses of the libraries contained in this release of SANE.
 *
 * The exception is that, if you link a SANE library with other files
 * to produce an executable, this does not by itself cause the
 * resulting executable to be covered by the GNU General Public
 * License.  Your use of that executable is in no way restricted on
 * account of linking the SANE library code into it.
 *
 * This exception does not, however, invalidate any other reasons why
 * the executable file might be covered by the GNU General Public
 * License.
 *
 * If you submit changes to SANE to the maintainers to be included in
 * a subsequent release, you agree by submitting the changes that
 * those changes may be distributed with this exception intact.
 *
 * If you write modifications of your own for SANE, it is your choice
 * whether to permit this exception to apply to your modifications.
 * If you do not wish that, delete this exception notice.
 */
#ifdef _AIX
# include <lalloca.h>		/* MUST come first for AIX! */
#endif

#include "sane/config.h"
#include <lalloca.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "sane/sane.h"
#include "sane/sanei.h"
#include "sane/saneopts.h"

#define BACKEND_NAME	plustek
#include "sane/sanei_backend.h"
#include "sane/sanei_config.h"

#include "plustek.h"
#include "plustek-share.h"

/*********************** the debug levels ************************************/

#define _DBG_FATAL      0
#define _DBG_ERROR      1
#define _DBG_WARNING    3
#define _DBG_INFO       5
#define _DBG_PROC       7
#define _DBG_SANE_INIT 10
#define _DBG_READ      15

/*********************** other definitions ***********************************/

#define _MODEL_STR_LEN		255

/************************** global vars **************************************/

static int num_devices;
static Plustek_Device  *first_dev;
static Plustek_Scanner *first_handle;
static unsigned long    tsecs = 0;

static ModeParam mode_params[] =
{
  {0, 1, COLOR_BW},
  {0, 1, COLOR_HALFTONE},
  {0, 8, COLOR_256GRAY},
  {1, 8, COLOR_TRUE24},
  {1, 16, COLOR_TRUE32},
  {1, 16, COLOR_TRUE36},
};

static ModeParam mode_9636_params[] =
{
  {0, 1,  COLOR_BW},
  {0, 1,  COLOR_HALFTONE},
  {0, 8,  COLOR_256GRAY},
  {1, 8,  COLOR_TRUE24},
  {1, 16, COLOR_TRUE48},
};

static const SANE_String_Const mode_list[] =
{
	"Binary",
	"Halftone",
	"Gray",
	"Color",
/*	"Color30", */
	NULL
};

static const SANE_String_Const mode_9636_list[] =
{
	"Binary",
	"Halftone",
	"Gray",
	"Color",
	"Color36",
	NULL
};

static const SANE_String_Const ext_mode_list[] =
{
	"Normal",
	"Transparency",
	"Negative",
	NULL
};

static const SANE_String_Const halftone_list[] =
{
	"Dithermap 1",
	"Dithermap 2",
	"Randomize",
	NULL
};

static const SANE_String_Const dropout_list[] =
{
	"None",
	"Red",
	"Green",
	"Blue",
	NULL
};

static const SANE_Range percentage_range =
{
	-100 << SANE_FIXED_SCALE_SHIFT, /* minimum 		*/
	 100 << SANE_FIXED_SCALE_SHIFT, /* maximum 		*/
	   1 << SANE_FIXED_SCALE_SHIFT  /* quantization */
};

/*
 * lens info
 */
static LensInfo lens = {{0,0,0,0,},{0,0,0,0,},{0,0,0,0,},{0,0,0,0,},0,0};

/*
 * see plustek-share.h
 */
MODELSTR;

/* authorization stuff */
static SANE_Auth_Callback auth = NULL;

/*.............................................................................
 * open the driver
 */
static int drvopen( const char *dev_name )
{
	int 		   result;
	int			   handle;
	unsigned short version = _PTDRV_IOCTL_VERSION;

    DBG( _DBG_INFO, "drvopen()\n" );

	if ((handle = open(dev_name, O_RDONLY)) < 0) {
	    DBG(_DBG_ERROR, "open: can't open %s as a device\n", dev_name);
    	return handle;
	}
	
	result = ioctl(handle, _PTDRV_OPEN_DEVICE, &version );
	if( result < 0 ) {
		close( handle );
		DBG( _DBG_ERROR,"ioctl PT_DRV_OPEN_DEVICE failed(%d)\n", result );
        if( -9019 == result )
    		DBG( _DBG_ERROR,"Version problem, please recompile driver!\n" );
		return result;
    }

	tsecs = 0;

	return handle;
}

/*.............................................................................
 * close the driver
 */
static SANE_Status drvclose( int handle )
{
	int int_cnt;

	if( handle >= 0 ) {

	    DBG( _DBG_INFO, "drvclose()\n" );

		if( 0 != tsecs ) {
			DBG( _DBG_INFO, "TIME END 1: %lus\n", time(NULL)-tsecs);
		}

		/*
		 * don't check the return values, simply do it and close the driver
		 */
		int_cnt = 0;
		ioctl( handle, _PTDRV_STOP_SCAN, &int_cnt );
		ioctl( handle, _PTDRV_CLOSE_DEVICE, 0);

		close( handle );
	}

	return SANE_STATUS_GOOD;
}

/*.............................................................................
 * according to the mode and source we return the corresponding mode list
 */
static pModeParam getModeList( Plustek_Scanner *scanner )
{
	pModeParam mp;

	if((MODEL_OP_9636T  == scanner->hw->model) ||
	   (MODEL_OP_9636P  == scanner->hw->model) ||
	   (MODEL_OP_9636PP == scanner->hw->model)) {
		mp = mode_9636_params;	
	} else {
		mp = mode_params;	
	}

	/*
	 * the transparency/negative mode supports only GRAY/COLOR/COLOR32/COLOR48
	 */
	if( 0 != scanner->val[OPT_EXT_MODE].w )
		mp = &mp[_TPAModeSupportMin];

	return mp;
}

/*.............................................................................
 *
 */
static SANE_Status close_pipe( Plustek_Scanner *scanner )
{
	if( scanner->pipe >= 0 ) {

		DBG( _DBG_PROC, "close_pipe\n" );

		close( scanner->pipe );
		scanner->pipe = -1;
	}

	return SANE_STATUS_EOF;
}

/*.............................................................................
 *
 */
static void sig_chldhandler( int signo )
{
	DBG( _DBG_PROC, "Child is down (signal=%d)\n", signo );
}

/*.............................................................................
 * signal handler to kill the child process
 */
static RETSIGTYPE reader_process_sigterm_handler( int signal )
{
	DBG( _DBG_PROC, "reader_process: terminated by signal %d\n", signal );
	_exit( SANE_STATUS_GOOD );
}

/*.............................................................................
 * executed as a child process
 * read the data from the driver and send the to the parent process
 */
static int reader_process( Plustek_Scanner *scanner, int pipe_fd )		
{
	unsigned long	 status;
	unsigned long 	 data_length;
	struct SIGACTION act;

	DBG( _DBG_PROC, "reader_process started\n" );

	/* install the signal handler */
	memset (&act, 0, sizeof(act));			
	act.sa_handler = reader_process_sigterm_handler;
	sigaction (SIGTERM, &act, 0);

	data_length = scanner->params.lines * scanner->params.bytes_per_line;

	DBG( _DBG_PROC, "reader_process:"
					"starting to READ data (%lu bytes)\n", data_length );
	DBG( _DBG_PROC, "buf = 0x%08lx\n", (unsigned long)scanner->buf );

	if( NULL == scanner->buf ) {
		DBG( _DBG_FATAL, "NULL Pointer !!!!\n" );
		return SANE_STATUS_IO_ERROR;
	}
	
	/* here we read all data from the driver... */
	status = (unsigned long)read( scanner->hw->fd, scanner->buf, data_length );

	/* on error, there's no need to clean up, as this is done by the parent */
	if((int)status < 0 ) {
		DBG( _DBG_ERROR, "read failed, error %i\n", errno );
		if( errno == EBUSY )
			return SANE_STATUS_DEVICE_BUSY;

		return SANE_STATUS_IO_ERROR;
    }

	/* send to parent */
	DBG( _DBG_PROC, "sending %lu bytes to parent\n", status );

    write( pipe_fd, scanner->buf, status );

	pipe_fd = -1;

	DBG( _DBG_PROC, "reader_process: finished reading data\n" );
	return SANE_STATUS_GOOD;
}

/*.............................................................................
 * stop the current scan process
 */
static SANE_Status do_cancel( Plustek_Scanner *scanner, SANE_Bool closepipe  )
{
	pid_t res;
	int   int_cnt;

	DBG( _DBG_PROC,"do_cancel\n" );

	scanner->scanning = SANE_FALSE;

	if (scanner->reader_pid > 0) {

		DBG( _DBG_PROC,"killing reader_process\n" );

		/* tell the driver to stop scanning */
		if( -1 != scanner->hw->fd ) {
			int_cnt = 1;
			ioctl(scanner->hw->fd, _PTDRV_STOP_SCAN, &int_cnt );
		}

		/* kill our child process and wait until done */
		kill( scanner->reader_pid, SIGTERM );
		
		res = waitpid( scanner->reader_pid, 0, 0 );

		if( res != scanner->reader_pid )
			DBG( _DBG_PROC,"waitpid() failed !\n");

		scanner->reader_pid = 0;
		DBG( _DBG_PROC,"reader_process killed\n");
	}

	if( SANE_TRUE == closepipe ) {
		close_pipe( scanner );
	}

	drvclose( scanner->hw->fd );

	if( tsecs != 0 ) {
		DBG( _DBG_INFO, "TIME END 2: %lus\n", time(NULL)-tsecs);
		tsecs = 0;
	}

	scanner->hw->fd = -1;

	return SANE_STATUS_CANCELLED;
}

/*.............................................................................
 *
 */
static SANE_Status limitResolution( Plustek_Device *dev )
{
	dev->dpi_range.min = _DEF_DPI;

	/*
	 * CHANGE: limit resolution to max. physical available one
	 *		   Note: the limit for the Asic 96001/3 models is limited to the
	 *				 X-Resolution
	 */
	if( _ASIC_IS_98001 == dev->asic ) {
		dev->dpi_range.max = lens.rDpiY.wPhyMax;
	} else {
		dev->dpi_range.max = lens.rDpiX.wPhyMax;
	}
	dev->dpi_range.quant = 0;
	dev->x_range.min 	 = 0;
	dev->x_range.max 	 = SANE_FIX(dev->max_x);
	dev->x_range.quant 	 = 0;
	dev->y_range.min 	 = 0;
	dev->y_range.max 	 = SANE_FIX(dev->max_y);
	dev->y_range.quant 	 = 0;
	
	return SANE_STATUS_GOOD;
}

/*.............................................................................
 * initialize the options for the backend according to the device we have
 */
static SANE_Status init_options( Plustek_Scanner *s )
{
	int i;

	memset( s->opt, 0, sizeof(s->opt));

	for (i = 0; i < NUM_OPTIONS; ++i) {
		s->opt[i].size = sizeof (SANE_Word);
		s->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }

	s->opt[OPT_NUM_OPTS].name  = SANE_NAME_NUM_OPTIONS;
	s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
	s->opt[OPT_NUM_OPTS].desc  = SANE_DESC_NUM_OPTIONS;
	s->opt[OPT_NUM_OPTS].type  = SANE_TYPE_INT;
	s->opt[OPT_NUM_OPTS].unit  = SANE_UNIT_NONE;
	s->opt[OPT_NUM_OPTS].size  = sizeof(SANE_Word);
	s->opt[OPT_NUM_OPTS].cap   = SANE_CAP_SOFT_DETECT;
	s->opt[OPT_NUM_OPTS].constraint_type = SANE_CONSTRAINT_NONE;
	s->val[OPT_NUM_OPTS].w 	   = NUM_OPTIONS;

	/* "Scan Mode" group: */
	s->opt[OPT_MODE_GROUP].name  = "Scan Mode group";
	s->opt[OPT_MODE_GROUP].title = "Scan Mode";
	s->opt[OPT_MODE_GROUP].desc  = "";
	s->opt[OPT_MODE_GROUP].type  = SANE_TYPE_GROUP;
	s->opt[OPT_MODE_GROUP].cap   = 0;

	/* scan mode */
	s->opt[OPT_MODE].name  = SANE_NAME_SCAN_MODE;
	s->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
	s->opt[OPT_MODE].desc  = SANE_DESC_SCAN_MODE;
	s->opt[OPT_MODE].type  = SANE_TYPE_STRING;
	s->opt[OPT_MODE].size  = 32;
	s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;

	if((MODEL_OP_9636T  == s->hw->model) ||
	   (MODEL_OP_9636P  == s->hw->model) ||
	   (MODEL_OP_9636PP == s->hw->model)) {
		s->opt[OPT_MODE].constraint.string_list = mode_9636_list;
	} else {
		s->opt[OPT_MODE].constraint.string_list = mode_list;
	}
	s->val[OPT_MODE].w = 3;		/* Color */

	/* scan source */
	s->opt[OPT_EXT_MODE].name  = SANE_NAME_SCAN_SOURCE;
	s->opt[OPT_EXT_MODE].title = SANE_TITLE_SCAN_SOURCE;
	s->opt[OPT_EXT_MODE].desc  = PLUSTEK_DESC_SCAN_SOURCE;
	s->opt[OPT_EXT_MODE].type  = SANE_TYPE_STRING;
	s->opt[OPT_EXT_MODE].size  = 32;
	s->opt[OPT_EXT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_EXT_MODE].constraint.string_list = ext_mode_list;
	s->val[OPT_EXT_MODE].w = 0; /* Normal */
	
	/* halftone */
	s->opt[OPT_HALFTONE].name  = SANE_NAME_HALFTONE;
	s->opt[OPT_HALFTONE].title = SANE_TITLE_HALFTONE;
	s->opt[OPT_HALFTONE].desc  = "Selects the halftone.";
	s->opt[OPT_HALFTONE].type  = SANE_TYPE_STRING;
	s->opt[OPT_HALFTONE].size  = 32;
	s->opt[OPT_HALFTONE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_HALFTONE].constraint.string_list = halftone_list;
	s->val[OPT_HALFTONE].w = 0;	/* Standard dithermap */
	s->opt[OPT_HALFTONE].cap |= SANE_CAP_INACTIVE;

	/* dropout */
	s->opt[OPT_DROPOUT].name  = "dropout";
	s->opt[OPT_DROPOUT].title = "Dropout";
	s->opt[OPT_DROPOUT].desc  = "Selects the dropout.";
	s->opt[OPT_DROPOUT].type  = SANE_TYPE_STRING;
	s->opt[OPT_DROPOUT].size  = 32;
	s->opt[OPT_DROPOUT].cap  |= SANE_CAP_ADVANCED;
	s->opt[OPT_DROPOUT].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_DROPOUT].constraint.string_list = dropout_list;
	s->val[OPT_DROPOUT].w = 0;	/* None */
	s->opt[OPT_DROPOUT].cap |= SANE_CAP_INACTIVE;

	/* brightness */
	s->opt[OPT_BRIGHTNESS].name  = SANE_NAME_BRIGHTNESS;
	s->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
	s->opt[OPT_BRIGHTNESS].desc  = "Selects the brightness.";
	s->opt[OPT_BRIGHTNESS].type  = SANE_TYPE_FIXED;
	s->opt[OPT_BRIGHTNESS].unit  = SANE_UNIT_PERCENT;
	s->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_BRIGHTNESS].constraint.range = &percentage_range;
	s->val[OPT_BRIGHTNESS].w     = 0;

	/* contrast */
	s->opt[OPT_CONTRAST].name  = SANE_NAME_CONTRAST;
	s->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
	s->opt[OPT_CONTRAST].desc  = SANE_DESC_CONTRAST;
	s->opt[OPT_CONTRAST].type  = SANE_TYPE_FIXED;
	s->opt[OPT_CONTRAST].unit  = SANE_UNIT_PERCENT;
	s->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_CONTRAST].constraint.range = &percentage_range;
	s->val[OPT_CONTRAST].w     = 0;

	/* resolution */
	s->opt[OPT_RESOLUTION].name  = SANE_NAME_SCAN_RESOLUTION;
	s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
	s->opt[OPT_RESOLUTION].desc  = SANE_DESC_SCAN_RESOLUTION;
	s->opt[OPT_RESOLUTION].type  = SANE_TYPE_INT;
	s->opt[OPT_RESOLUTION].unit  = SANE_UNIT_DPI;

	s->opt[OPT_RESOLUTION].constraint_type  = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_RESOLUTION].constraint.range = &s->hw->dpi_range;
	s->val[OPT_RESOLUTION].w = s->hw->dpi_range.min;

	/* "Geometry" group: */
	s->opt[OPT_GEOMETRY_GROUP].title = "Geometry";
	s->opt[OPT_GEOMETRY_GROUP].name  = "Geometry Group";
	s->opt[OPT_GEOMETRY_GROUP].desc  = "";
	s->opt[OPT_GEOMETRY_GROUP].type  = SANE_TYPE_GROUP;
	s->opt[OPT_GEOMETRY_GROUP].cap   = SANE_CAP_ADVANCED;

	/* top-left x */
	s->opt[OPT_TL_X].name  = SANE_NAME_SCAN_TL_X;
	s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
	s->opt[OPT_TL_X].desc  = SANE_DESC_SCAN_TL_X;
	s->opt[OPT_TL_X].type  = SANE_TYPE_FIXED;
	s->opt[OPT_TL_X].unit  = SANE_UNIT_MM;
	s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_TL_X].constraint.range = &s->hw->x_range;
	s->val[OPT_TL_X].w = SANE_FIX(_DEFAULT_TLX);

	/* top-left y */
	s->opt[OPT_TL_Y].name  = SANE_NAME_SCAN_TL_Y;
	s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
	s->opt[OPT_TL_Y].desc  = SANE_DESC_SCAN_TL_Y;
	s->opt[OPT_TL_Y].type  = SANE_TYPE_FIXED;
	s->opt[OPT_TL_Y].unit  = SANE_UNIT_MM;
	s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_TL_Y].constraint.range = &s->hw->y_range;
	s->val[OPT_TL_Y].w = SANE_FIX(_DEFAULT_TLY);

	/* bottom-right x */
	s->opt[OPT_BR_X].name  = SANE_NAME_SCAN_BR_X;
	s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
	s->opt[OPT_BR_X].desc  = SANE_DESC_SCAN_BR_X;
	s->opt[OPT_BR_X].type  = SANE_TYPE_FIXED;
	s->opt[OPT_BR_X].unit  = SANE_UNIT_MM;
	s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_BR_X].constraint.range = &s->hw->x_range;
	s->val[OPT_BR_X].w = SANE_FIX(_DEFAULT_BRX);

	/* bottom-right y */
	s->opt[OPT_BR_Y].name  = SANE_NAME_SCAN_BR_Y;
	s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
	s->opt[OPT_BR_Y].desc  = SANE_DESC_SCAN_BR_Y;
	s->opt[OPT_BR_Y].type  = SANE_TYPE_FIXED;
	s->opt[OPT_BR_Y].unit  = SANE_UNIT_MM;
	s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_BR_Y].constraint.range = &s->hw->y_range;
	s->val[OPT_BR_Y].w = SANE_FIX(_DEFAULT_BRY);

	/* CHANGE: disable some settings for the 9636T and the other models */
	if(MODEL_OP_9636T != s->hw->model) {
		s->opt[OPT_EXT_MODE].cap |= SANE_CAP_INACTIVE;
	}

	return SANE_STATUS_GOOD;
}

/*.............................................................................
 *
 */
static SANE_Status attach( const char *dev_name, Plustek_Device **devp )
{
	char 	       *str;
	int 		    cntr;
	int			    result;
	int			    handle;
	SANE_Status	    status;
	Plustek_Device *dev;
	ScannerCaps	    scaps;

	DBG(_DBG_SANE_INIT, "attach (%s, %p)\n", dev_name, (void *)devp);

	/* already attached ?*/
	for( dev = first_dev; dev; dev = dev->next )
		if (strcmp (dev->sane.name, dev_name) == 0) {
			if (devp)
		*devp = dev;
		return SANE_STATUS_GOOD;
    }

	/*
	 * go ahead and open the scanner device
	 */
	handle = drvopen( dev_name );
	if( handle < 0 ) {
		return SANE_STATUS_IO_ERROR;
    }

	result = ioctl( handle, _PTDRV_GET_CAPABILITIES, &scaps);
	if( result < 0 ) {
		DBG( _DBG_ERROR, "ioctl _PTDRV_GET_CAPABILITIES failed(%d)\n", result);
		close(handle);
		return SANE_STATUS_IO_ERROR;
    }

	result = ioctl( handle, _PTDRV_GET_LENSINFO, &lens);
	if( result < 0 ) {
		DBG( _DBG_ERROR, "ioctl _PTDRV_GET_LENSINFO failed(%d)\n", result );
		close(handle);
		return SANE_STATUS_IO_ERROR;
	}

	/* did we fail on connection? */
	if (scaps.wIOBase == _NO_BASE ) {
		DBG( _DBG_ERROR, "failed to find Plustek scanner\n" );
		close(handle);
		return SANE_STATUS_INVAL;
    }

	/* allocate some memory for the device */
	dev = malloc( sizeof (*dev));
	if (!dev)
    	return SANE_STATUS_NO_MEM;

	memset (dev, 0, sizeof (*dev));

	dev->fd			 = -1;
	dev->sane.name   = strdup (dev_name);
	dev->sane.vendor = "Plustek";
	dev->sane.type   = "flatbed scanner";


	/* save the info we got from the driver */
	dev->model  = scaps.Model;
	dev->asic   = scaps.AsicID;
	dev->max_x  = scaps.wMaxExtentX*MM_PER_INCH/_MEASURE_BASE;
	dev->max_y  = scaps.wMaxExtentY*MM_PER_INCH/_MEASURE_BASE;

	dev->res_list = (SANE_Int *) calloc(((lens.rDpiX.wMax -_DEF_DPI)/25 + 1),
			     sizeof (SANE_Int));  /* one more to avoid a buffer overflow */

	if (NULL == dev->res_list) {
		DBG( _DBG_ERROR, "alloc fail, resolution problem\n" );
		close(handle);
		return SANE_STATUS_INVAL;
	}

	dev->res_list_size = 0;
	for (cntr = _DEF_DPI; cntr <= lens.rDpiX.wMax; cntr += 25) {
		dev->res_list_size++;
		dev->res_list[dev->res_list_size - 1] = (SANE_Int) cntr;
	}

	status = limitResolution( dev );
	drvclose( handle );

	str = malloc(_MODEL_STR_LEN);
	if( NULL == str ) {
		DBG(_DBG_ERROR,"attach: out of memory\n");
		return SANE_STATUS_NO_MEM;
	}

	str[(_MODEL_STR_LEN-1)] = '\0';
	str[0] = '\0';

	/* error, give asic # */
	if (scaps.Model > sizeof (ModelStr) / sizeof (*ModelStr)) {
		sprintf (str, "ASIC ID = 0x%x",  scaps.AsicID);
	} else {
		sprintf (str, ModelStr[scaps.Model]);  /* lookup model string */
	}

	dev->sane.model = str;
	dev->fd			= handle;

	DBG( _DBG_SANE_INIT, "attach: model = >%s<\n", dev->sane.model );

	++num_devices;
	dev->next = first_dev;
	first_dev = dev;

	if (devp)
    	*devp = dev;

	return SANE_STATUS_GOOD;
}

/*.............................................................................
 *
 */
static SANE_Status attach_one( const char *dev )
{
	DBG( _DBG_SANE_INIT, "attach_one: >%s<\n", dev );

	return attach(dev, 0);
}

/*.............................................................................
 * intialize the backend
 */
SANE_Status sane_init( SANE_Int *version_code, SANE_Auth_Callback authorize )
{
	char   dev_name[PATH_MAX] = "/dev/pt_drv";
	size_t len;
	FILE  *fp;

	DBG_INIT();

#if defined PACKAGE && defined VERSION
	DBG( _DBG_SANE_INIT, "sane_init: " PACKAGE " " VERSION "\n");
#endif

	auth         = authorize;
	first_dev    = NULL;
	first_handle = NULL;
	num_devices  = 0;

	if( version_code != NULL )
		*version_code = SANE_VERSION_CODE(V_MAJOR, V_MINOR, 0);

	fp = sanei_config_open( PLUSTEK_CONFIG_FILE );

	/* default to /dev/pt_drv instead of insisting on config file */
	if( NULL == fp ) {
		return attach("/dev/pt_drv", 0);
	}

	while (sanei_config_read( dev_name, sizeof(dev_name), fp)) {

		DBG( _DBG_SANE_INIT, "sane_init, >%s<\n", dev_name);
		if( dev_name[0] == '#')		/* ignore line comments */
    		continue;
			
		len = strlen(dev_name);
		if( dev_name[len - 1] == '\n' )
           	dev_name[--len] = '\0';
		if( !len)
           	continue;			/* ignore empty lines */

		DBG( _DBG_SANE_INIT, "sane_init, >%s<\n", dev_name);

		sanei_config_attach_matching_devices( dev_name, attach_one );
	}
   	fclose (fp);

	return SANE_STATUS_GOOD;
}

/*.............................................................................
 * cleanup the backend...
 */
void sane_exit( void )
{
	Plustek_Device *dev, *next;

	DBG( _DBG_SANE_INIT, "sane_exit\n" );


	for( dev = first_dev; dev; dev = next ) {
    	next = dev->next;

		if( dev->sane.name )
			free ((void *) dev->sane.name);

		if( dev->sane.model )
			free ((void *) dev->sane.model);

		free (dev);
	}

	auth         = NULL;
	first_dev    = NULL;
	first_handle = NULL;
}

/*.............................................................................
 * return a list of all devices
 */
SANE_Status sane_get_devices(const SANE_Device ***device_list,
														SANE_Bool local_only )
{
	static const SANE_Device **devlist = 0;
	Plustek_Device            *dev;
	int                        i;

	DBG(_DBG_SANE_INIT, "sane_get_devices (%p, %ld)\n", (void *) device_list,
				       (long) local_only);

	/* already called, so cleanup */
	if( devlist )
    	free( devlist );

	devlist = malloc((num_devices + 1) * sizeof (devlist[0]));
	if ( NULL == devlist )
    	return SANE_STATUS_NO_MEM;

	i = 0;
	for (dev = first_dev; i < num_devices; dev = dev->next)
    	devlist[i++] = &dev->sane;
	devlist[i++] = 0;

	*device_list = devlist;
	return SANE_STATUS_GOOD;
}

/*.............................................................................
 * open the sane device
 */
SANE_Status sane_open( SANE_String_Const devicename, SANE_Handle * handle )
{
	SANE_Status      status;
	Plustek_Device  *dev;
	Plustek_Scanner *s;

	DBG( _DBG_SANE_INIT, "sane_open\n" );

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
		/* empty devicename -> use first device */
		dev = first_dev;
	}

	if (!dev)
    	return SANE_STATUS_INVAL;

	s = malloc (sizeof (*s));
	if (!s)
    	return SANE_STATUS_NO_MEM;

	memset(s, 0, sizeof (*s));
	s->pipe     = -1;
	s->hw       = dev;
	s->scanning = SANE_FALSE;

	init_options(s);

	/* insert newly opened handle into list of open handles: */
	s->next      = first_handle;
	first_handle = s;

	*handle = s;
	return SANE_STATUS_GOOD;
}

/*.............................................................................
 *
 */
void sane_close (SANE_Handle handle)
{
	Plustek_Scanner *prev, *s;

	DBG( _DBG_SANE_INIT, "sane_close\n" );

	/* remove handle from list of open handles: */
	prev = 0;

	for( s = first_handle; s; s = s->next ) {
		if( s == handle )
			break;
		prev = s;
	}

	if (!s) {
    	DBG( _DBG_ERROR, "close: invalid handle %p\n", handle);
		return;		
	}

	close_pipe( s );

	if( NULL != s->buf )
		free(s->buf);

	drvclose( s->hw->fd );
	s->hw->fd = -1;

	if (prev)
    	prev->next = s->next;
	else
    	first_handle = s->next;

	free(s);
}

/*.............................................................................
 *
 */
const SANE_Option_Descriptor *
			sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
	Plustek_Scanner *s = (Plustek_Scanner *) handle;

	if (option < 0 || option >= NUM_OPTIONS)
		return NULL;

	return &(s->opt[option]);
}

/*.............................................................................
 *
 */
static const SANE_String_Const *
		search_string_list (const SANE_String_Const * list, SANE_String value)
{
	while (*list != NULL && strcmp (value, *list) != 0)
		++list;

	if (*list == NULL)
		return NULL;

	return list;
}

/*.............................................................................
 * return or set the parameter values, also do some checks
 */
SANE_Status sane_control_option( SANE_Handle handle, SANE_Int option,
							     SANE_Action action, void *value,
							     SANE_Int * info)
{
	Plustek_Scanner *s = (Plustek_Scanner *)handle;
	SANE_Status status;
	const SANE_String_Const *optval;

	if ( s->scanning ) {
		return SANE_STATUS_DEVICE_BUSY;
	}

	if (option < 0 || option >= NUM_OPTIONS)
    	return SANE_STATUS_INVAL;

	if (info != NULL)
		*info = 0;

	switch (action) {
		case SANE_ACTION_GET_VALUE:
			switch (option) {
			case OPT_NUM_OPTS:
			case OPT_RESOLUTION:
			case OPT_TL_X:
			case OPT_TL_Y:
			case OPT_BR_X:
			case OPT_BR_Y:
			  *(SANE_Word *)value = s->val[option].w;
			  break;

			case OPT_CONTRAST:
			case OPT_BRIGHTNESS:
				*(SANE_Word *)value =
								(s->val[option].w << SANE_FIXED_SCALE_SHIFT);
				break;

			case OPT_MODE:
			case OPT_EXT_MODE:
			case OPT_HALFTONE:
			case OPT_DROPOUT:
				strcpy ((char *) value,
					  s->opt[option].constraint.string_list[s->val[option].w]);
				break;
			default:
				return SANE_STATUS_INVAL;
		}
		break;

	case SANE_ACTION_SET_VALUE:
    	status = sanei_constrain_value( s->opt + option, value, info );
		if (status != SANE_STATUS_GOOD) {
			return status;
		}

		optval = NULL;
		if (s->opt[option].constraint_type == SANE_CONSTRAINT_STRING_LIST) {
			optval = search_string_list (s->opt[option].constraint.string_list,
									       (char *) value);
			if (optval == NULL)
	    		return SANE_STATUS_INVAL;
		}

      	switch (option) {
			case OPT_RESOLUTION: {
			    int n;
	    		int min_d = s->hw->res_list[s->hw->res_list_size - 1];
			    int v     = *(SANE_Word *)value;
	    		int best  = v;

			    for (n = 0; n < s->hw->res_list_size; n++) {
					int d = abs (v - s->hw->res_list[n]);

					if (d < min_d) {
					    min_d = d;
				    	best  = s->hw->res_list[n];
					}
				}

	    		s->val[option].w = (SANE_Word)best;

                if(v != best)
                    *(SANE_Word *)value = best;

				if (info != NULL) {
					if( v != best)	
                        *info |= SANE_INFO_INEXACT;
					*info |= SANE_INFO_RELOAD_PARAMS;
     		  	}
			    break;

		  	}
			case OPT_TL_X:
			case OPT_TL_Y:
			case OPT_BR_X:
			case OPT_BR_Y:
				s->val[option].w = *(SANE_Word *)value;
				if (info != NULL)
					*info |= SANE_INFO_RELOAD_PARAMS;
				break;

			case OPT_CONTRAST:
			case OPT_BRIGHTNESS:
    			s->val[option].w =
							((*(SANE_Word *)value) >> SANE_FIXED_SCALE_SHIFT);
				break;

			case OPT_MODE:
				if(mode_params[optval - mode_list].scanmode != COLOR_HALFTONE){
					s->opt[OPT_HALFTONE].cap |= SANE_CAP_INACTIVE;
					s->opt[OPT_CONTRAST].cap &= ~SANE_CAP_INACTIVE;
				} else {
					s->opt[OPT_HALFTONE].cap &= ~SANE_CAP_INACTIVE;
					s->opt[OPT_CONTRAST].cap |= SANE_CAP_INACTIVE;
				}
#if 0
				if (mode_params[optval - mode_list].color) {
					s->opt[OPT_DROPOUT].cap |= SANE_CAP_INACTIVE;
				} else {
					s->opt[OPT_DROPOUT].cap &= ~SANE_CAP_INACTIVE;
				}
#endif
				if (info != NULL)
					*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
			/* fall through */
			case OPT_HALFTONE:
			case OPT_DROPOUT:
				s->val[option].w = optval - s->opt[option].constraint.string_list;
				break;

			case OPT_EXT_MODE:
				s->val[option].w = optval - s->opt[option].constraint.string_list;

				/*
				 * change the area and mode_list when changing the source
				 */
				if( s->val[option].w == 0 ) {

					s->hw->dpi_range.min = _DEF_DPI;

					s->hw->x_range.max = SANE_FIX(s->hw->max_x);
					s->hw->y_range.max = SANE_FIX(s->hw->max_y);
					s->val[OPT_TL_X].w = SANE_FIX(_DEFAULT_TLX);
					s->val[OPT_TL_Y].w = SANE_FIX(_DEFAULT_TLY);
   					s->val[OPT_BR_X].w = SANE_FIX(_DEFAULT_BRX);
					s->val[OPT_BR_Y].w = SANE_FIX(_DEFAULT_BRY);

					if((MODEL_OP_9636T  == s->hw->model) ||
					   (MODEL_OP_9636P  == s->hw->model) ||
					   (MODEL_OP_9636PP == s->hw->model)) {
						s->opt[OPT_MODE].constraint.string_list = mode_9636_list;
					} else {
						s->opt[OPT_MODE].constraint.string_list = mode_list;
					}
					s->val[OPT_MODE].w = 3;		/* _COLOR_TRUE24 */

				} else {

					s->hw->dpi_range.min = _TPAMinDpi;

					if( s->val[option].w == 1 ) {
    					s->hw->x_range.max = SANE_FIX(_TP_X);
						s->hw->y_range.max = SANE_FIX(_TP_Y);
						s->val[OPT_TL_X].w = SANE_FIX(_DEFAULT_TP_TLX);
						s->val[OPT_TL_Y].w = SANE_FIX(_DEFAULT_TP_TLY);
   						s->val[OPT_BR_X].w = SANE_FIX(_DEFAULT_TP_BRX);
						s->val[OPT_BR_Y].w = SANE_FIX(_DEFAULT_TP_BRY);

					} else {
    					s->hw->x_range.max = SANE_FIX(_NEG_X);
						s->hw->y_range.max = SANE_FIX(_NEG_Y);
						s->val[OPT_TL_X].w = SANE_FIX(_DEFAULT_NEG_TLX);
						s->val[OPT_TL_Y].w = SANE_FIX(_DEFAULT_NEG_TLY);
   						s->val[OPT_BR_X].w = SANE_FIX(_DEFAULT_NEG_BRX);
						s->val[OPT_BR_Y].w = SANE_FIX(_DEFAULT_NEG_BRY);
					}

					if( MODEL_OP_9636T == s->hw->model ) {
						s->opt[OPT_MODE].constraint.string_list =
											&mode_9636_list[_TPAModeSupportMin];
					} else {
						s->opt[OPT_MODE].constraint.string_list =
												&mode_list[_TPAModeSupportMin];
					}
					s->val[OPT_MODE].w = 1;		/* _COLOR_TRUE24 */
    			}

				s->opt[OPT_HALFTONE].cap |= SANE_CAP_INACTIVE;
/*				s->opt[OPT_DROPOUT].cap  |= SANE_CAP_INACTIVE;
 */
				s->opt[OPT_CONTRAST].cap &= ~SANE_CAP_INACTIVE;

				if (info != NULL)
					*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
				break;

			default:
				return SANE_STATUS_INVAL;
		}
		break;
	default:
    	return SANE_STATUS_INVAL;
	}

	return SANE_STATUS_GOOD;
}

/*.............................................................................
 * return the current parameter settings
 */
SANE_Status sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
	int    			 ndpi;
	pModeParam  	 mp;
	Plustek_Scanner *s = (Plustek_Scanner *)handle;

	/* if we're calling from within, calc best guess
     * do the same, if sane_get_parameters() is called
     * by a frontend before sane_start() is called
     */
    if ((NULL == params) ||	(s->scanning != SANE_TRUE)) {

		mp = getModeList( s );

		memset( &s->params, 0, sizeof (SANE_Parameters));

		ndpi = s->val[OPT_RESOLUTION].w;

	    s->params.pixels_per_line =	SANE_UNFIX(s->val[OPT_BR_X].w -
									s->val[OPT_TL_X].w) / MM_PER_INCH * ndpi;

    	s->params.lines = SANE_UNFIX( s->val[OPT_BR_Y].w -
									s->val[OPT_TL_Y].w) / MM_PER_INCH * ndpi;

		/* pixels_per_line seems to be 8 * n.  */
		/* s->params.pixels_per_line = s->params.pixels_per_line & ~7; debug only */

	    s->params.last_frame = SANE_TRUE;
    	s->params.depth = mp[s->val[OPT_MODE].w].depth;

		if (mp[s->val[OPT_MODE].w].color) {
			s->params.format = SANE_FRAME_RGB;
			s->params.bytes_per_line = 3 * s->params.pixels_per_line;
		} else {
			s->params.format = SANE_FRAME_GRAY;
			if (s->params.depth == 1)
				s->params.bytes_per_line = (s->params.pixels_per_line + 7) / 8;
			else
				s->params.bytes_per_line = s->params.pixels_per_line *
															s->params.depth / 8;
		}

        /* if sane_get_parameters() was called before sane_start() */
	    /* pass new values to the caller                           */
    	if ((NULL != params) &&	(s->scanning != SANE_TRUE))
	    	*params = s->params;
	} else
		*params = s->params;

	return SANE_STATUS_GOOD;
}

/*.............................................................................
 * initiate the scan process
 */
SANE_Status sane_start( SANE_Handle handle )
{
	Plustek_Scanner *s = (Plustek_Scanner *) handle;
	pModeParam		 mp;

	int			result;
	int 		ndpi;
	int 		left, top;
	int 		width, height;
	int			scanmode;
	int         fds[2];
	ScannerCaps	scaps;
	StartScan	start;
	CmdBlk		cb;
	CropInfo	crop;
	SANE_Status status;
    SANE_Word   tmp;

	DBG( _DBG_SANE_INIT, "sane_start\n" );

	if( s->scanning ) {
		return SANE_STATUS_DEVICE_BUSY;
	}

	status = sane_get_parameters (handle, NULL);
	if (status != SANE_STATUS_GOOD) {
		DBG( _DBG_ERROR, "sane_get_parameters failed\n" );
		return status;
	}

	/*
	 * open the driver and get some information about the scanner
	 */
	s->hw->fd = drvopen ( s->hw->sane.name );
	if( s->hw->fd < 0 ) {
		DBG( _DBG_ERROR,"sane_start: open failed: %d\n", errno );

		if( errno == EBUSY )
			return SANE_STATUS_DEVICE_BUSY;

		return SANE_STATUS_IO_ERROR;
	}

	result = ioctl( s->hw->fd, _PTDRV_GET_CAPABILITIES, &scaps);
	if( result < 0 ) {
		DBG( _DBG_ERROR, "ioctl _PTDRV_GET_CAPABILITIES failed(%d)\n", result);
		close( s->hw->fd );
		return SANE_STATUS_IO_ERROR;
    }
	
	result = ioctl( s->hw->fd, _PTDRV_GET_LENSINFO, &lens);
	if( result < 0 ) {
		DBG( _DBG_ERROR, "ioctl _PTDRV_GET_LENSINFO failed(%d)\n", result );
		close(s->hw->fd);
		return SANE_STATUS_IO_ERROR;
    }

	/* did we fail on connection? */
	if (scaps.wIOBase == _NO_BASE ) {
		DBG( _DBG_ERROR, "failed to find Plustek scanner\n" );
		close(s->hw->fd);
		return SANE_STATUS_INVAL;
	}

	/* All ready to go.  Set image def and see what the scanner
	 * says for crop info.
	 */
	ndpi = s->val[OPT_RESOLUTION].w;

    /* exchange the values as we can't deal with negative heights and so on...*/
    tmp = s->val[OPT_TL_X].w;
    if( tmp > s->val[OPT_BR_X].w ) {
		DBG( _DBG_INFO, "exchanging BR-X - TL-X\n" );
        s->val[OPT_TL_X].w = s->val[OPT_BR_X].w;
        s->val[OPT_BR_X].w = tmp;
    }

    tmp = s->val[OPT_TL_Y].w;
    if( tmp > s->val[OPT_BR_Y].w ) {
		DBG( _DBG_INFO, "exchanging BR-Y - TL-Y\n" );
        s->val[OPT_TL_Y].w = s->val[OPT_BR_Y].w;
        s->val[OPT_BR_Y].w = tmp;
    }

	/* position and extent are always relative to 300 dpi */
	left   = (int)(SANE_UNFIX (s->val[OPT_TL_X].w)*(double)lens.rDpiX.wPhyMax/
							(MM_PER_INCH*((double)lens.rDpiX.wPhyMax/300.0)));
	top    = (int)(SANE_UNFIX (s->val[OPT_TL_Y].w)*(double)lens.rDpiY.wPhyMax/
							(MM_PER_INCH*((double)lens.rDpiY.wPhyMax/300.0)));
	width  = (int)(SANE_UNFIX (s->val[OPT_BR_X].w - s->val[OPT_TL_X].w) *
					(double)lens.rDpiX.wPhyMax /
							(MM_PER_INCH *((double)lens.rDpiX.wPhyMax/300.0)));
	height = (int)(SANE_UNFIX (s->val[OPT_BR_Y].w - s->val[OPT_TL_Y].w) *
					(double)lens.rDpiY.wPhyMax /
							(MM_PER_INCH *((double)lens.rDpiY.wPhyMax/300.0)));

	/*
	 * adjust mode list according to the model we use and the
	 * source we have
	 */
	mp = getModeList( s );

	scanmode = mp[s->val[OPT_MODE].w].scanmode;

	/* clear it out just in case */
	memset (&cb, 0, sizeof (cb));
	cb.ucmd.cInf.ImgDef.xyDpi.x	  = ndpi;
	cb.ucmd.cInf.ImgDef.xyDpi.y   = ndpi;
	cb.ucmd.cInf.ImgDef.crArea.x  = left;  	/* offset from left edge to area you want to scan */
	cb.ucmd.cInf.ImgDef.crArea.y  = top;  	/* offset from top edge to area you want to scan  */
	cb.ucmd.cInf.ImgDef.crArea.cx = width;  /* always relative to 300 dpi */
	cb.ucmd.cInf.ImgDef.crArea.cy = height;
	cb.ucmd.cInf.ImgDef.wDataType = scanmode;

/*
 * CHECK: what about the 10 bit mode
 */
	if( COLOR_TRUE48 == scanmode )
		cb.ucmd.cInf.ImgDef.wBits = OUTPUT_12Bits;
	else if( COLOR_TRUE32 == scanmode )
		cb.ucmd.cInf.ImgDef.wBits = OUTPUT_10Bits;
	else
		cb.ucmd.cInf.ImgDef.wBits = OUTPUT_8Bits;

	cb.ucmd.cInf.ImgDef.dwFlag = SCANDEF_QualityScan;

	switch( s->val[OPT_EXT_MODE].w ) {
		case 1: cb.ucmd.cInf.ImgDef.dwFlag |= SCANDEF_Transparency; break;
		case 2: cb.ucmd.cInf.ImgDef.dwFlag |= SCANDEF_Negative; 	break;
		default: break;
	}

	cb.ucmd.cInf.ImgDef.wLens = scaps.wLens;

	result = ioctl( s->hw->fd, _PTDRV_PUT_IMAGEINFO, &cb);
	if( result < 0 ) {
		DBG( _DBG_ERROR, "ioctl _PTDRV_PUT_IMAGEINFO failed(%d)\n", result );
		close(s->hw->fd);
		return SANE_STATUS_IO_ERROR;
	}

	result = ioctl(s->hw->fd, _PTDRV_GET_CROPINFO, &crop);
	if( result < 0 ) {
	    DBG( _DBG_ERROR, "ioctl _PTDRV_GET_CROPINFO failed(%d)\n", result );
	    close(s->hw->fd);
    	return SANE_STATUS_IO_ERROR;
    }

	/* DataInf.dwAppPixelsPerLine = crop.dwPixelsPerLine;  get calc'd pixels per line */
	s->params.pixels_per_line = crop.dwPixelsPerLine;
	s->params.bytes_per_line  = crop.dwBytesPerLine;  /* get calc'd bytes per line */
	s->params.lines 		  = crop.dwLinesPerArea;  /* get calc'd lines per area */

	/* build a SCANINFO block and get ready to scan it */
	cb.ucmd.sInf.ImgDef.xyDpi.x   = ndpi;
	cb.ucmd.sInf.ImgDef.xyDpi.y   = ndpi;
	cb.ucmd.sInf.ImgDef.crArea.x  = left;
	cb.ucmd.sInf.ImgDef.crArea.y  = top;
	cb.ucmd.sInf.ImgDef.crArea.cx = width;
	cb.ucmd.sInf.ImgDef.crArea.cy = height;
	cb.ucmd.sInf.ImgDef.wDataType = scanmode;

	if( COLOR_TRUE48 == scanmode )
		cb.ucmd.sInf.ImgDef.wBits = OUTPUT_12Bits;
	else if( COLOR_TRUE32 == scanmode )
		cb.ucmd.sInf.ImgDef.wBits = OUTPUT_10Bits;
	else
		cb.ucmd.sInf.ImgDef.wBits = OUTPUT_8Bits;

	cb.ucmd.sInf.ImgDef.dwFlag = (SCANDEF_BuildBwMap | SCANDEF_QualityScan);

/*
 *	cb.ucmd.sInf.ImgDef.dwFlag |= SCANDEF_Inverse;
 */
	switch( s->val[OPT_EXT_MODE].w ) {
		case 1: cb.ucmd.sInf.ImgDef.dwFlag |= SCANDEF_Transparency; break;
		case 2: cb.ucmd.sInf.ImgDef.dwFlag |= SCANDEF_Negative; 	break;
		default: break;
	}

	cb.ucmd.sInf.ImgDef.wLens = 1;

	cb.ucmd.sInf.siBrightness = s->val[OPT_BRIGHTNESS].w;
	cb.ucmd.sInf.siContrast   = s->val[OPT_CONTRAST].w;
	cb.ucmd.sInf.wDither	  = s->val[OPT_HALFTONE].w;

	DBG( _DBG_SANE_INIT, "bright %i contrast %i\n", cb.ucmd.sInf.siBrightness,
			 									      cb.ucmd.sInf.siContrast);

	result = ioctl(s->hw->fd, _PTDRV_SET_ENV, &cb.ucmd.sInf);
	if( result < 0 ) {
		DBG( _DBG_ERROR, "ioctl _PTDRV_SET_ENV failed(%d)\n", result );
		close(s->hw->fd);
		return SANE_STATUS_IO_ERROR;
    }

	result = ioctl(s->hw->fd, _PTDRV_START_SCAN, &start);
	if( result < 0 ) {
		DBG( _DBG_ERROR, "ioctl _PTDRV_START_SCAN failed(%d)\n", result );
		close(s->hw->fd);
		return SANE_STATUS_IO_ERROR;
    }

	DBG( _DBG_SANE_INIT, "dwflag = 0x%lx dwBytesPerLine = %ld, "
		 "dwLinesPerScan = %ld\n",
					 start.dwFlag, start.dwBytesPerLine, start.dwLinesPerScan);

	s->buf = realloc( s->buf, (s->params.lines) * s->params.bytes_per_line );
	if( NULL == s->buf ) {
		DBG( _DBG_ERROR, "realloc failed\n" );
		close(s->hw->fd);
		return SANE_STATUS_NO_MEM;
	}

	s->scanning = SANE_TRUE;

	tsecs = (unsigned long)time(NULL);
	DBG( _DBG_INFO, "TIME START\n" );

	/*
	 * everything prepared, so start the child process and a pipe to communicate
	 * pipe --> fds[0]=read-fd, fds[1]=write-fd
	 */
	if( pipe(fds) < 0 ) {
		DBG( _DBG_ERROR, "ERROR: could not create pipe\n" );
	    s->scanning = SANE_FALSE;
		close(s->hw->fd);
		return SANE_STATUS_IO_ERROR;
	}

	/* create reader routine as new process */
	s->bytes_read = 0;
	s->reader_pid = fork();			

	if( s->reader_pid < 0 ) {
		DBG( _DBG_ERROR, "ERROR: could not create child process\n" );
	    s->scanning = SANE_FALSE;
		close(s->hw->fd);
		return SANE_STATUS_IO_ERROR;
	}

	/* reader_pid = 0 ===> child process */		
	if ( 0 == s->reader_pid ) {									

		sigset_t 		 ignore_set;
		struct SIGACTION act;

		DBG( _DBG_SANE_INIT, "reader process...\n" );

		close(fds[0]);

		sigfillset ( &ignore_set );
		sigdelset  ( &ignore_set, SIGTERM );
		sigprocmask( SIG_SETMASK, &ignore_set, 0 );

		memset   ( &act, 0, sizeof (act));
		sigaction( SIGTERM, &act, 0 );

		status = reader_process( s, fds[1] );

		DBG( _DBG_SANE_INIT, "reader process done, status = %i\n", status );

		/* don't use exit() since that would run the atexit() handlers */
		_exit( status );
	}

	signal( SIGCHLD, sig_chldhandler );

	close(fds[1]);
	s->pipe = fds[0];

	DBG( _DBG_SANE_INIT, "sane_start done\n" );

	return SANE_STATUS_GOOD;
}

/*.............................................................................
 * function to read the data from our child process
 */
SANE_Status sane_read( SANE_Handle handle, SANE_Byte *data,
									   SANE_Int max_length, SANE_Int *length )
{
	Plustek_Scanner *s = (Plustek_Scanner *)handle;
	ssize_t 		 nread;

	*length = 0;

	nread = read( s->pipe, data, max_length );
	DBG( _DBG_READ, "sane_read - read %ld bytes\n", (long)nread );

	if (!(s->scanning)) {
		return do_cancel( s, SANE_TRUE );
	}

	if (nread < 0) {
		if (errno == EAGAIN) {

			if( s->bytes_read ==
				(unsigned long)(s->params.lines * s->params.bytes_per_line)) {
				waitpid( s->reader_pid, 0, 0 );
				s->reader_pid = -1;
				drvclose( s->hw->fd );
				s->hw->fd = -1;
				return close_pipe(s);
			}

			return SANE_STATUS_GOOD;

		} else {
			DBG( _DBG_ERROR, "ERROR: errno=%d\n", errno );
			do_cancel( s, SANE_TRUE );
			return SANE_STATUS_IO_ERROR;
		}
	}

	*length        = nread;
	s->bytes_read += nread;

	if (0 == nread) {
		drvclose( s->hw->fd );
		s->hw->fd = -1;
		return close_pipe(s);
	}

	return SANE_STATUS_GOOD;
}

/*.............................................................................
 * cancel the scanning process
 */
void sane_cancel (SANE_Handle handle)
{
	Plustek_Scanner *s = (Plustek_Scanner *)handle;

	DBG( _DBG_SANE_INIT, "sane_cancel\n" );

	if( s->scanning ) {
		do_cancel( s, SANE_FALSE );
	}
}

/*.............................................................................
 * set the pipe to blocking/non blocking mode
 */
SANE_Status sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
	Plustek_Scanner *s = (Plustek_Scanner *)handle;

	DBG( _DBG_SANE_INIT, "sane_set_io_mode: non_blocking=%d\n",non_blocking );

	if ( !s->scanning ) {
		DBG( _DBG_ERROR, "ERROR: not scanning !\n" );
		return SANE_STATUS_INVAL;
	}

	if (fcntl (s->pipe, F_SETFL, non_blocking ? O_NONBLOCK : 0) < 0) {
		DBG( _DBG_ERROR, "ERROR: can´t set to non-blocking mode !\n" );
		return SANE_STATUS_IO_ERROR;
	}

	DBG( _DBG_SANE_INIT, "sane_set_io_mode done\n" );
	return SANE_STATUS_GOOD;
}

/*.............................................................................
 * return the descriptor if available
 */
SANE_Status sane_get_select_fd( SANE_Handle handle, SANE_Int * fd )
{
	Plustek_Scanner *s = (Plustek_Scanner *)handle;

	DBG( _DBG_SANE_INIT, "sane_get_select_fd\n" );

	if ( !s->scanning ) {
		DBG( _DBG_ERROR, "ERROR: not scanning !\n" );
		return SANE_STATUS_INVAL;
	}

	*fd = s->pipe;

	DBG( _DBG_SANE_INIT, "sane_get_select_fd done\n" );
	return SANE_STATUS_GOOD;
}

/* END PLUSTEK.C ............................................................*/
