/*.............................................................................
 * Project : SANE library for Plustek flatbed scanners.
 *.............................................................................
 */

/** @file plustek-usbcalfile.c
 *  @brief Functions for saving/restoring calibration settings
 *
 * Copyright (C) 2001-2005 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.46 - first version
 * - 0.47 - no changes
 * - 0.48 - no changes
 * - 0.49 - a_bRegs is now part of the device structure
 * - 0.50 - cleanup
 * .
 * <hr>
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
 * <hr>
 */
 
typedef struct {
	u_long red_light_on;
	u_long red_light_off;
	u_long green_light_on;
	u_long green_light_off;
	u_long blue_light_on;
	u_long blue_light_off;
	u_long green_pwm_duty;
	
} LightCtrl, *pLightCtrl;

typedef struct {
	u_short     version;

	u_short   red_gain;
	u_short   green_gain;
	u_short   blue_gain;

	u_short   red_offs;
	u_short   green_offs;
	u_short   blue_offs;

	LightCtrl light;

} CalData, *pCalData;

#define _PT_CF_VERSION 0x0001

/** function to read a text file and returns the string which starts which
 *  'id' string.
 *  no duplicate entries where detected, always the first occurance will be
 *  red.
 * @param fp  - file pointer of file to read
 * @param id  - what to search for
 * @param res - where to store the result upon success
 * @return SANE_TRUE on success, SANE_FALSE on any error
 */
static SANE_Bool usb_ReadSpecLine( FILE *fp, char *id, char* res )
{
	char  tmp[1024];
	char *ptr;
	
	/* rewind file pointer */
	if( 0 != fseek( fp, 0L, SEEK_SET)) {
		DBG( _DBG_ERROR, "fseek: %s\n", strerror(errno));
		return SANE_FALSE;
	}

	/* roam through the file and examine each line... */
	while( !feof( fp )) {

		if( NULL != fgets( tmp, 1024, fp )) {

			if( 0 == strncmp( tmp, id, strlen(id))) {

				ptr = &tmp[strlen(id)];
        	    if( '\0' == *ptr )
					break;
					
				strcpy( res, ptr );
				res[strlen(res)-1] = '\0';
				return SANE_TRUE;
			}
		}
	}

	return SANE_FALSE;
}

/**
 */
static char *usb_ReadOtherLines( FILE *fp, char *except )
{
	char  tmp[1024];
	char *ptr, *ptr_base;
	int   len;

	if( 0 != fseek( fp, 0L, SEEK_END))
		return NULL;

	len = ftell(fp);
	
	/* rewind file pointer */
	if( 0 != fseek( fp, 0L, SEEK_SET))
		return NULL;

	if( len == 0 )
		return NULL;

	ptr = (char*)malloc(len);
	if( NULL == ptr )
		return NULL;
		
    ptr_base = ptr;
	*ptr     = '\0';
	/* roam through the file and examine each line... */
	while( !feof( fp )) {

		if( NULL != fgets( tmp, 1024, fp )) {

			if( 0 == strncmp( tmp, "version=", 8 )) 
				continue;
	
			if( 0 != strncmp( tmp, except, strlen(except))) {

				if( strlen( tmp ) > 0 ) {
					strcpy( ptr, tmp );
					ptr += strlen(tmp);
					*ptr = '\0';
				}
			}
		}
	}
	return ptr_base;	
}

/**
 */
static void usb_RestoreCalData( Plustek_Device *dev, CalData *cal )
{
	u_char *regs = dev->usbDev.a_bRegs;

	regs[0x3b] = (u_char)cal->red_gain;
	regs[0x3c] = (u_char)cal->green_gain;
	regs[0x3d] = (u_char)cal->blue_gain;
	regs[0x38] = (u_char)cal->red_offs;
	regs[0x39] = (u_char)cal->green_offs;
	regs[0x3a] = (u_char)cal->blue_offs;

	regs[0x2a] = _HIBYTE((u_short)cal->light.green_pwm_duty);
	regs[0x2b] = _LOBYTE((u_short)cal->light.green_pwm_duty);

	regs[0x2c] = _HIBYTE((u_short)cal->light.red_light_on);
	regs[0x2d] = _LOBYTE((u_short)cal->light.red_light_on);
	regs[0x2e] = _HIBYTE((u_short)cal->light.red_light_off);
	regs[0x2f] = _LOBYTE((u_short)cal->light.red_light_off);

	regs[0x30] = _HIBYTE((u_short)cal->light.green_light_on);
	regs[0x31] = _LOBYTE((u_short)cal->light.green_light_on);
	regs[0x32] = _HIBYTE((u_short)cal->light.green_light_off);
	regs[0x33] = _LOBYTE((u_short)cal->light.green_light_off);

	regs[0x34] = _HIBYTE((u_short)cal->light.blue_light_on);
	regs[0x35] = _LOBYTE((u_short)cal->light.blue_light_on);
	regs[0x36] = _HIBYTE((u_short)cal->light.blue_light_off);
	regs[0x37] = _LOBYTE((u_short)cal->light.blue_light_off);
}

/**
 */
static void usb_CreatePrefix( Plustek_Device *dev, char *pfx )
{
	char       bd[5];
	ScanDef   *scanning = &dev->scanning;
	ScanParam *param    = &scanning->sParam;

	switch( scanning->sParam.bSource ) {

		case SOURCE_Transparency: strcpy( pfx, "tpa-" ); break;
		case SOURCE_Negative:     strcpy( pfx, "neg-" ); break;
		case SOURCE_ADF:          strcpy( pfx, "adf-" ); break;
	    default:                  pfx[0] = '\0'; break;
	}

	sprintf( bd, "%u=", param->bBitDepth );
	if( param->bDataType == SCANDATATYPE_Color )
		strcat( pfx, "color" );
	else
		strcat( pfx, "gray" );

	strcat( pfx, bd );
}

/** function to read and set the calibration data from external file
 */
static SANE_Bool usb_ReadAndSetCalData( Plustek_Device *dev )
{
	char       pfx[20];
	char       tmp[1024];
	u_short    version;
	int        res;
	FILE      *fp;
	CalData    cal;
	SANE_Bool  ret;
	
	DBG( _DBG_INFO, "usb_ReadAndSetCalData()\n" );

	if( NULL == dev->calFile ) {
		DBG( _DBG_ERROR, "- No calibration filename set!\n" );
		return SANE_FALSE;
	}

	DBG( _DBG_INFO, "- Reading calibration data from file\n");
	DBG( _DBG_INFO, "  %s\n", dev->calFile );
	
	fp = fopen( dev->calFile, "r" );
	if( NULL == fp ) {
		DBG( _DBG_ERROR, "File %s not found\n", dev->calFile );
		return SANE_FALSE;
	}

	/* check version */
	if( !usb_ReadSpecLine( fp, "version=", tmp )) {
		DBG( _DBG_ERROR, "Could not find version info!\n" );
		fclose( fp );
		return SANE_FALSE;
	}

	DBG( _DBG_INFO, "- Calibration file version: %s\n", tmp );
	if( 1 != sscanf( tmp, "0x%04hx", &version )) {
		DBG( _DBG_ERROR, "Could not decode version info!\n" );
		fclose( fp );
		return SANE_FALSE;
	}

	if( version != _PT_CF_VERSION ) {
		DBG( _DBG_ERROR, "Versions do not match!\n" );
		fclose( fp );
		return SANE_FALSE;
	}

	usb_CreatePrefix( dev, pfx );	
	
	ret = SANE_FALSE;
	if( usb_ReadSpecLine( fp, pfx, tmp )) {
		DBG( _DBG_INFO, "- Calibration data: %s\n", tmp );

		res = sscanf( tmp, "%hu,%hu,%hu,%hu,%hu,%hu,"
						   "%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
						&cal.red_gain,   &cal.red_offs,
						&cal.green_gain, &cal.green_offs,
						&cal.blue_gain,  &cal.blue_offs,
						&cal.light.red_light_on,   &cal.light.red_light_off,
						&cal.light.green_light_on, &cal.light.green_light_off,
						&cal.light.blue_light_on,  &cal.light.blue_light_off,
						&cal.light.green_pwm_duty );

		if( 13 == res ) {
			usb_RestoreCalData( dev, &cal );
			ret = SANE_TRUE;
		}
	}

	fclose( fp );
	DBG( _DBG_INFO, "usb_ReadAndSetCalData() done -> %u\n", ret );
		
	return ret;
}

/**
 */
static void usb_PrepCalData( Plustek_Device *dev, CalData *cal )
{
	u_char *regs = dev->usbDev.a_bRegs;

	memset( cal, 0, sizeof(CalData));
    cal->version = _PT_CF_VERSION;

	cal->red_gain   = (u_short)regs[0x3b];
	cal->green_gain = (u_short)regs[0x3c];
	cal->blue_gain  = (u_short)regs[0x3d];
	cal->red_offs   = (u_short)regs[0x38];
	cal->green_offs = (u_short)regs[0x39];
	cal->blue_offs  = (u_short)regs[0x3a];

	cal->light.green_pwm_duty  = regs[0x2a] * 256 + regs[0x2b];

	cal->light.red_light_on    = regs[0x2c] * 256 + regs[0x2d];
	cal->light.red_light_off   = regs[0x2e] * 256 + regs[0x2f];
	cal->light.green_light_on  = regs[0x30] * 256 + regs[0x31];
	cal->light.green_light_off = regs[0x32] * 256 + regs[0x33];
	cal->light.blue_light_on   = regs[0x34] * 256 + regs[0x35];
	cal->light.blue_light_off  = regs[0x36] * 256 + regs[0x37];
}

/** function to save/update the calibration data
 */
static void usb_SaveCalData( Plustek_Device *dev )
{
	char       pfx[20];
	char       tmp[1024];
	char       set_tmp[1024];
	char      *other_tmp;
	u_short    version;
	FILE      *fp;
	CalData    cal;
	ScanDef   *scanning = &dev->scanning;

	DBG( _DBG_INFO, "usb_SaveCalData()\n" );

	/* no new data, so skip this step too */
	if( SANE_TRUE == scanning->skipCoarseCalib ) {
		DBG( _DBG_INFO, "- No calibration data to save!\n" );
		return;
	}

	if( NULL == dev->calFile ) {
		DBG( _DBG_ERROR, "- No calibration filename set!\n" );
		return;
	}
	DBG( _DBG_INFO, "- Saving calibration data to file\n" );
	DBG( _DBG_INFO, "  %s\n", dev->calFile );

	usb_PrepCalData ( dev, &cal );
	usb_CreatePrefix( dev, pfx );

	sprintf( set_tmp, "%s%u,%u,%u,%u,%u,%u,"
	                  "%lu,%lu,%lu,%lu,%lu,%lu,%lu\n", pfx,
	                  cal.red_gain,   cal.red_offs,
	                  cal.green_gain, cal.green_offs,
	                  cal.blue_gain,  cal.blue_offs,
	                  cal.light.red_light_on,   cal.light.red_light_off,
	                  cal.light.green_light_on, cal.light.green_light_off,
	                  cal.light.blue_light_on,  cal.light.blue_light_off,
	                  cal.light.green_pwm_duty );

	/* read complete old file if compatible... */
	other_tmp = NULL;
	fp = fopen( dev->calFile, "r+" );
	if( NULL != fp ) {
	
		if( usb_ReadSpecLine( fp, "version=", tmp )) {
			DBG( _DBG_INFO, "- Calibration file version: %s\n", tmp );

			if( 1 == sscanf( tmp, "0x%04hx", &version )) {

				if( version == cal.version ) {
					DBG( _DBG_INFO, "- Versions do match\n" );

					/* read the rest... */
					other_tmp = usb_ReadOtherLines( fp, pfx );
				} else {
					DBG( _DBG_INFO2, "- Versions do not match (0x%04x)\n", version );
				}
			} else {
				DBG( _DBG_INFO2, "- cannot decode version\n" );
			}
		} else {
			DBG( _DBG_INFO2, "- Version not found\n" );
		}
		fclose( fp );
	}
	fp = fopen( dev->calFile, "w+" );
	if( NULL == fp ) {
		DBG( _DBG_ERROR, "- Cannot create file %s\n", dev->calFile );
		DBG( _DBG_ERROR, "- -> %s\n", strerror(errno));
		if( other_tmp ) 
			free( other_tmp );
		return;
	}

	/* rewrite the file again... */
	fprintf( fp, "version=0x%04X\n", cal.version );
	if( strlen( set_tmp ))
		fprintf( fp, "%s", set_tmp );

	if( other_tmp ) {
		fprintf( fp, "%s", other_tmp );
		free( other_tmp );
	}
	fclose( fp );
	DBG( _DBG_INFO, "usb_SaveCalData() done.\n" );
}

/* END PLUSTEK-USBCALFILE.C .................................................*/
