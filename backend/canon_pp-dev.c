/* sane - Scanner Access Now Easy.
   Copyright (C) 2001-2002 Matthew C. Duggan and Simon Krix
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

   -----

   This file is part of the canon_pp backend, supporting Canon FBX30P 
   and NX40P scanners and also distributed as part of the stand-alone 
   driver.  

   canon_pp-dev.c: $Revision$

   Misc constants for Canon FB330P/FB630P scanners and high-level 
   scan functions.

   Function library for Canon FB330/FB630P Scanners by
   Simon Krix <kinsei@users.sourceforge.net>
   */

#ifdef _AIX
#include <lalloca.h>
#endif

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <malloc.h>
#include <ieee1284.h>
#include "canon_pp-io.h"
#include "canon_pp-dev.h"

#ifdef NOSANE

/* No SANE, Things that only apply to stand-alone */
#include <stdio.h>
#include <stdarg.h>
static void DBG(int level, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}
#else

/* Definitions which only apply to SANE compiles */
#define VERSION "$Revision$"

#define DEBUG_DECLARE_ONLY
#include "canon_pp.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_backend.h"

#endif


/* Constants */
#define ID_FB330P "CANON   IX-03075E       1.00"
#define ID_FB630P "CANON   IX-06075E       1.00"
#define ID_N340P "CANON   IX-03095G       1.00"
#define ID_N640P "CANON   IX-06115G       1.00"

/*const int scanline_count = 6;*/
static const char *header = "#CANONPP";
static const int fileversion = 3;

/* Internal functions */
static unsigned long column_sum(image_segment *image, int x, int colournum);
static int adjust_output(image_segment *image, scan_parameters *scanp, 
		scanner_parameters *scannerp);
static int check8(unsigned char *p, int s);
static int sleep_scanner(struct parport *port);
/* Converts from weird scanner format -> sequential data */
static void convdata(unsigned char *srcbuffer, unsigned char *dstbuffer, 
		int width, int mode);
/* Sets up the scan command. This could use a better name
   (and a rewrite). */
static int scanner_setup_params(unsigned char *buf, scanner_parameters *sp, 
		scan_parameters *scanp);

/* Commands ================================================ */

/* Command_1[] moved to canon_pp-io.c for neatness */


/* Read device ID command */
/* after this 0x26 (38) bytes are read */
static unsigned char command_2[] = { 0xfe, 0x20, 0, 0, 0, 0, 0, 0, 0x26, 0 };
/* Reads 12 bytes of unknown information */
static unsigned char command_3[] = { 0xf3, 0x20, 0, 0, 0, 0, 0, 0, 0x0c, 0 };

/* Scan init command: Always followed immediately by command 42 */
static unsigned char command_41[] = { 0xde, 0x20, 0, 0, 0, 0, 0, 0, 0x2e, 0 };

/* Scan information block */
static unsigned char command_42[45] =
{ 0x11, 0x2c, 0x11, 0x2c, 0x10, 0x4b, 0x10, 0x4b, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0x08, 0x08, 0x01, 0x01, 0x80, 0x01,
	0x80, 0x80, 0x02, 0, 0, 0xc1, 0, 0x08, 0x01, 0x01,
	0, 0, 0, 0, 0
};

/* Read 6 byte buffer status block */
static unsigned char command_5[] = { 0xf3, 0x21, 0, 0, 0, 0, 0, 0, 0x06, 0 };

/* Request a block of image data */
static unsigned char command_6[] = {0xd4, 0x20, 0, 0, 0, 0, 0, 0x09, 0x64, 0};

/* "*SCANEND" command - returns the scanner to transparent mode */
static unsigned char command_7[] =
{ 0x1b, 0x2a, 0x53, 0x43, 0x41, 0x4e, 0x45, 0x4e, 0x44, 0x0d };

/* Reads BLACK calibration image */
static unsigned char command_8[] = {0xf8, 0x20, 0, 0, 0, 0, 0, 0x4a, 0xc4, 0};

/* Unknown command 9 */
static unsigned char command_9[] = {0xc5, 0x20, 0, 0, 0, 0, 0, 0, 0, 0};

/* Reads unknown 30 bytes */
static unsigned char command_10[] = {0xf6, 0x20, 0, 0, 0, 0, 0, 0, 0x20, 0};

/* Reads COLOUR (R,G or B) calibration image */
static unsigned char command_11[] = {0xf9, 0x20, 0, 0, 0, 0, 0, 0x4a, 0xc4, 0};

/* Abort scan */
static unsigned char command_12[] = {0xef, 0x20, 0, 0, 0, 0, 0, 0, 0, 0};

/* Followed by command 14 */
static unsigned char command_13[] = {0xe6, 0x20, 0, 0, 0, 0, 0, 0, 0x20, 0};

#if 0
/* Something about RGB gamma/gain values? Not currently used by this code */
static unsigned char command_14[32] =
{ 0x2, 0x0, 0x3, 0x7f,
	0x2, 0x0, 0x3, 0x7f,
	0x2, 0x0, 0x3, 0x7f,
	0, 0, 0, 0,
	0x12, 0xd1, 0x14, 0x82,
	0, 0, 0, 0,
	0x0f, 0xff, 
	0x0f, 0xff, 
	0x0f, 0xff, 0, 0 };
#endif

/* Scan-related functions =================================== */

int sanei_canon_pp_init_scan(scanner_parameters *sp, scan_parameters *scanp)
{
	/* Command for: Initialise and begin the scan procedure */
	unsigned char command_b[46];

	/* Buffer for buffer info block */
	unsigned char buffer_info_block[6];

	/* The image size the scanner says we asked for 
	   (based on the scanner's replies) */
	int true_scanline_size, true_scanline_count;

	/* The image size we expect to get (based on *scanp) */
	int expected_scanline_size, expected_scanline_count;

	/* Set up the default scan command packet */
	memcpy(command_b, command_42, 45);

	/* Load the proper settings into it */
	scanner_setup_params(command_b, sp, scanp);

	do
	{
		usleep(100000);
		sanei_canon_pp_write(sp->port, 10, command_41);
		command_b[45] = check8(command_b, 45);
		sanei_canon_pp_write(sp->port, 46, command_b);
	}
	while (sanei_canon_pp_check_status(sp->port) == 1);

	/* Ask the scanner about the buffer */
	sanei_canon_pp_write(sp->port, 10, command_5);
	sanei_canon_pp_check_status(sp->port);

	/* Read buffer information block */
	sanei_canon_pp_read(sp->port, 6, buffer_info_block);

	if (check8(buffer_info_block, 6)) 
		DBG(1, "init_scan: ** Warning: Checksum error reading buffer "
				"info block.\n");

	expected_scanline_count = scanp->height;

	switch(scanp->mode)
	{
		case 0: /* greyscale; 10 bits per pixel */
			expected_scanline_size = scanp->width * 1.25; break;
		case 1: /* true-colour; 30 bits per pixel */
			expected_scanline_size = scanp->width * 3.75; break;
		default: 
			DBG(1, "init_scan: Illegal mode %i requested in "
					"init_scan().\n", scanp->mode);
			DBG(1, "This is a bug. Please report it.\n");
			return -1;
	}

	/* The scanner's idea of the length of each scanline in bytes */
	true_scanline_size = (buffer_info_block[0]<<8) | buffer_info_block[1]; 
	/* The scanner's idea of the number of scanlines in total */
	true_scanline_count = (buffer_info_block[2]<<8) | buffer_info_block[3]; 

	if ((expected_scanline_size != true_scanline_size) 
			|| (expected_scanline_count != true_scanline_count))
	{
		DBG(10, "init_scan: Warning: Scanner is producing an image "
				"of unexpected size:\n");
		DBG(10, "expected: %i bytes wide, %i scanlines tall.\n", 
				expected_scanline_size, 
				expected_scanline_count);
		DBG(10, "true: %i bytes wide, %i scanlines tall.\n", 
				true_scanline_size, true_scanline_count);

		if (scanp->mode == 0)
			scanp->width = true_scanline_size / 1.25;
		else
			scanp->width = true_scanline_size / 3.75;				

		scanp->height = true_scanline_count;
	}
	return 0;	
}


/* Wake the scanner, detect it, and fill sp with stuff */
int sanei_canon_pp_initialise(scanner_parameters *sp)
{
	unsigned char unknown12[12];

	/* Hopefully take the scanner out of transparent mode */
	if (sanei_canon_pp_wake_scanner(sp->port))
	{
		DBG(10, "initialise: could not wake scanner\n");
		return 1;
	}

	/* Give it a tenth of a second to prepare */
	usleep(100000); 

	/* This block of code does something unknown but necessary */
	sanei_canon_pp_scanner_init(sp->port);

	/* Read Device ID */
	sanei_canon_pp_write(sp->port, 10, command_2);
	sanei_canon_pp_check_status(sp->port);
	sanei_canon_pp_read(sp->port, 38, (unsigned char *)(sp->id_string));

	/* Read partially unknown data */
	sanei_canon_pp_write(sp->port, 10, command_3);
	sanei_canon_pp_check_status(sp->port);
	sanei_canon_pp_read(sp->port, 12, unknown12);

	if (check8(unknown12, 12))
	{
		DBG(10, "initialise: Checksum error reading Info Block.\n");
		return 2;
	}

	sp->scanheadwidth = (unknown12[2] << 8) | unknown12[3];


	/* Set up various known values */
	if (strncmp(&(sp->id_string[8]), ID_FB330P, sizeof(ID_FB330P)) == 0)
	{
		strcpy(sp->name, "FB330P");
		sp->natural_xresolution = 2;	
		sp->natural_yresolution = 2;	
		sp->scanbedlength = 3508;
	}
	else if (strncmp(&(sp->id_string[8]), ID_FB630P, sizeof(ID_FB630P))==0)
	{
		strcpy(sp->name, "FB630P");
		sp->natural_xresolution = 3;
		sp->natural_yresolution = 3;
		sp->scanbedlength = 8016;
	}
	else if (strncmp(&(sp->id_string[8]), ID_N640P, sizeof(ID_N640P)) == 0)
	{
		strcpy(sp->name, "N640P");
		sp->natural_xresolution = 3;	
		sp->natural_yresolution = 3;	
		sp->scanbedlength = 8016;
	}
	else if (strncmp(&(sp->id_string[8]), ID_N340P, sizeof(ID_N340P)) == 0)
	{
		strcpy(sp->name, "N340P");
		sp->natural_xresolution = 2;	
		sp->natural_yresolution = 2;	
		sp->scanbedlength = 3508;
	}
	else
	{
		if (sp->scanheadwidth == 5104) 
		{
			/* Guess 600dpi scanner */
			strcpy(sp->name, "Unknown 600dpi");
			sp->natural_xresolution = 3; 
			sp->natural_yresolution = 3;	
			sp->scanbedlength = 8016;
		}
		else if (sp->scanheadwidth == 2552) 
		{
			/* Guess 300dpi scanner */
			strcpy(sp->name, "Unknown 300dpi");
			sp->natural_xresolution = 2; 
			sp->natural_yresolution = 2;	
			sp->scanbedlength = 3508;
		}
		else
		{
			/* Guinea Pigs :) */
			strcpy(sp->name, "Unknown (600dpi?)");
			sp->natural_xresolution = 3; 
			sp->natural_yresolution = 3;	
			sp->scanbedlength = 8016;			
		}
	}

	return 0;	
}

/* Shut scanner down */
int sanei_canon_pp_close_scanner(scanner_parameters *sp)
{
	/* Put scanner in transparent mode */	
	sleep_scanner(sp->port);

	/* Free memory (with purchase of memory of equal or greater value) */
	if (sp->blackweight != NULL) 
	{
		free(sp->blackweight);
		sp->blackweight = NULL;
	}
	if (sp->redweight != NULL)
	{
		free(sp->redweight);
		sp->redweight = NULL;
	}
	if (sp->greenweight != NULL)
	{
		free(sp->greenweight);
		sp->greenweight = NULL;
	}
	if (sp->blueweight != NULL)
	{
		free(sp->blueweight);
		sp->blueweight = NULL;
	}

	return 0;	
}

/* Read the calibration information from file */
int sanei_canon_pp_load_weights(const char *filename, scanner_parameters *sp)
{
	int filehandle;
	int cal_data_size = sp->scanheadwidth * sizeof(unsigned long);
	int cal_file_size;

	char buffer[10];
	int temp;

	/* Open file */
	if ((filehandle = open(filename, O_RDONLY)) == -1)
		return -1;

	/* Read header and check it's right */
	temp = read(filehandle, buffer, strlen(header) + 1);
	if ((temp == 0) || strcmp(buffer, header) != 0)
		return -2;

	/* Read and check file version (the calibrate file 
	   format changes from time to time) */
	read(filehandle, &temp, sizeof(int));

	if (temp != fileversion)
		return -3;

	/* Allocate memory for calibration values */
	if (((sp->blueweight = (unsigned long *)malloc(cal_data_size)) == NULL)
			|| ((sp->redweight = (unsigned long *)
					malloc(cal_data_size)) == NULL)
			|| ((sp->greenweight = (unsigned long *)
					malloc(cal_data_size)) == NULL)
			|| ((sp->blackweight = (unsigned long *)
					malloc(cal_data_size)) == NULL)) 
		return -4;

	/* Read width of calibration data */
	read(filehandle, &cal_file_size, sizeof(cal_file_size));

	if (cal_file_size != sp->scanheadwidth)
	{
		close(filehandle);
		return -5;
	}

	/* Read calibration data */
	read(filehandle, sp->blackweight, cal_data_size);
	read(filehandle, sp->redweight, cal_data_size);
	read(filehandle, sp->greenweight, cal_data_size);
	read(filehandle, sp->blueweight, cal_data_size);

	/* Read white-balance/gamma data */
	read(filehandle, &(sp->gamma), 32);

	close(filehandle);	

	return 0;
}

/* Mode is 0 for greyscale source data or 1 for RGB */
static void convert_to_rgb(image_segment *dest, unsigned char *src, 
		int width, int scanlines, int mode)
{
	int curline;

	const int colour_size = width * 1.25;
	const int scanline_size = (mode == 0 ? colour_size : colour_size * 3);

	for (curline = 0; curline < scanlines; curline++)
	{

		if (mode == 0) /* Grey */
		{
			/* Red */
			convdata(src + (curline * scanline_size), 
					dest->image_data + 
					(curline * width *3*2) + 4, width, 2);			
			/* Green */
			convdata(src + (curline * scanline_size), 
					dest->image_data + 
					(curline * width *3*2) + 2, width, 2);
			/* Blue */
			convdata(src + (curline * scanline_size), 
					dest->image_data + 
					(curline * width *3*2), width, 2);
		}
		else if (mode == 1) /* Truecolour */
		{
			/* Red */
			convdata(src + (curline * scanline_size), 
					dest->image_data + 
					(curline * width *3*2) + 4, width, 2);
			/* Green */
			convdata(src + (curline * scanline_size) + colour_size, 
					dest->image_data + 
					(curline * width *3*2) + 2, width, 2);
			/* Blue */
			convdata(src + (curline * scanline_size) + 
					(2 * colour_size), dest->image_data + 
					(curline * width *3*2), width, 2);
		}

	} /* End of scanline loop */

}

int sanei_canon_pp_read_segment(image_segment **dest, scanner_parameters *sp, 
		scan_parameters *scanp, int scanline_number, int do_adjust)
{
	unsigned char *input_buffer;
	image_segment *output_image;

	unsigned char packet_header[4];
	unsigned char packet_request_command[10];

	int read_data_size;
	int scanline_size;
	int error = 0;

	if (scanp->mode == 1) /* RGB */
		scanline_size = scanp->width * 3.75;
	else /* Greyscale */
		scanline_size = scanp->width * 1.25;

	read_data_size = scanline_size * scanline_number;

	/* Allocate output_image struct */
	DBG(100, "read_segment: Allocate output_image struct\n");
	*dest = (image_segment *)malloc(sizeof(image_segment));
	output_image = *dest;

	/* Allocate memory for input buffer */
	DBG(100, "read_segment: Allocate input buffer\n");
	if ((input_buffer = (unsigned char *)malloc(scanline_size * 
					scanline_number)) == NULL)
	{
		DBG(10, "read_segment: Error: Not enough memory for scanner "
				"input buffer\n");
		return -1;
	}

	output_image->width = scanp->width;
	output_image->height = scanline_number;

	/* Allocate memory for dest image segment */
	DBG(100, "read_segment: Dest image segment\n");
	output_image->image_data = (unsigned char *)
		malloc(output_image->width * output_image->height * 3 * 2);

	if (output_image->image_data == NULL)
	{
		DBG(10, "read_segment: Error: Not enough memory for "
				"image data\n");
		return -1;
	}

	/* Set up packet request command */
	memcpy(packet_request_command, command_6, 10);
	packet_request_command[7] = ((read_data_size + 4) & 0xFF00) >> 8;
	packet_request_command[8] = (read_data_size + 4) & 0xFF;

	do
	{

		/* Send packet req. and wait for the scanner's READY signal */
		do
		{
			/* Give the CPU a bit of a rest if we have to wait */
			usleep(100000); 

			/* Send command */
			sanei_canon_pp_write(sp->port, 10, 
					packet_request_command);

		} while (sanei_canon_pp_check_status(sp->port));

		/* Read packet header */
		sanei_canon_pp_read(sp->port, 4, packet_header);

		if ((packet_header[2]<<8) + packet_header[3] != read_data_size)
		{
			DBG(1, "read_segment: Error: Expected data size: %i"
					" bytes.\n", read_data_size);
			DBG(1, "read_segment: Expecting %i bytes times %i "
					"scanlines.\n", 
					scanline_size, scanline_number);
			DBG(1, "read_segment: Actual data size: %i bytes.\n", 
					(packet_header[2] << 8) 
					+ packet_header[3]);
			return -1;
		}

		/* Read scanlines_this_packet scanlines into the input buf */
		error = sanei_canon_pp_read(sp->port, read_data_size, 
				input_buffer);
		if (error)
			DBG(10, "read_segment: Segment read incorrectly, "
					"retrying...\n");
	}
	while (error);

	DBG(100, "read_segment: Convert to RGB\n");
	/* Convert data */
	convert_to_rgb(output_image, input_buffer, scanp->width, 
			scanline_number, scanp->mode);

	/* Adjust pixel readings according to calibration data */
	if (do_adjust) {
		DBG(100, "read_segment: Adjust output\n");
		adjust_output(output_image, scanp, sp);
	}

	return 0;	
}

/* 
check8: Calculates the checksum-8 for s bytes pointed to by p.

For messages from the scanner, this should normally end up returning
0, since the last byte of most packets is the value that makes the 
total up to 0 (or 256 if you're left-handed).
Hence, usage: if (check8(buffer, size)) {DBG(10, "checksum error!\n");} 

Can also be used to generate valid checksums for sending to the scanner.
*/
static int check8(unsigned char *p, int s) {
	int total=0,i;
	for(i=0;i<s;i++)
		total-=(signed char)p[i];
	total &=0xFF;
	return total;
}

/* Converts from scanner format -> linear
   width is in pixels, not bytes. */
/* This function could use a rewrite */
static void convdata(unsigned char *srcbuffer, unsigned char *dstbuffer, 
		int width, int mode)
/* This is a tricky (read: crap) function (read: hack) which is why I probably 
   spent more time commenting it than programming it. The thing to remember 
   here is that the scanner uses interpolated scanlines, so it's
   RRRRRRRGGGGGGBBBBBB not RGBRGBRGBRGBRGB. So, the calling function just 
   increments the destination pointer slightly to handle green, then a bit 
   more for blue. If you don't understand, tough. */
{
	int count;
	int i, j, k;

	for (count = 0; count < width; count++)
	{
		/* The scanner stores data in a bizzare butchered 10-bit 
		   format.  I'll try to explain it in 100 words or less:

		   Scanlines are made up of groups of 4 pixels. Each group of 
		   4 is stored inside 5 bytes. The first 4 bytes of the group 
		   contain the lowest 8 bits of one pixel each (in the right 
		   order). The 5th byte contains the most significant 2 bits 
		   of each pixel in the same order. */

		i = srcbuffer[count + (count >> 2)]; /* Low byte for pixel */
		j = srcbuffer[(((count / 4) + 1) * 5) - 1]; /* "5th" byte */
		j = j >> ((count % 4) * 2); /* Get upper 2 bits of intensity */
		j = j & 0x03; /* Can't hurt */
		/* And the final 10-bit pixel value is: */
		k = (j << 8) | i; 

		/* now we return this as a 16 bit value */
		k = k << 6;

		if (mode == 1) /* Scanner -> Grey */
		{
			dstbuffer[count * 2] = HIGH_BYTE(k);
			dstbuffer[(count * 2) + 1] = LOW_BYTE(k);
		}
		else if (mode == 2) /* Scanner -> RGB */
		{
			dstbuffer[count * 3 * 2] = HIGH_BYTE(k);
			dstbuffer[(count * 3 * 2) + 1] = LOW_BYTE(k);			
		}

	}
}

static int adjust_output(image_segment *image, scan_parameters *scanp, 
		scanner_parameters *scannerp)
/* Needing a good cleanup */
{
	/* light and dark points for the CCD sensor in question 
	 * (stored in file as 0-1024, scaled to 0-65536) */
	unsigned long hi, lo;
	/* The result of our calculations */
	unsigned long result;
	unsigned long temp;
	/* The CCD sensor which read the current pixel - this is a tricky value
	   to get right. */
	int ccd, scaled_xoff;
	/* Loop variables */
	unsigned int scanline, pixelnum, colour;
	unsigned long int pixel_address;

	for (scanline = 0; scanline < image->height; scanline++)
	{
		for (pixelnum = 0; pixelnum < image->width; pixelnum++)
		{
			/* Figure out CCD sensor number */
			/* MAGIC FORMULA ALERT! */
			ccd = (pixelnum << (scannerp->natural_xresolution - 
						scanp->xresolution)) + (1 << 
						(scannerp->natural_xresolution 
						 - scanp->xresolution)) - 1;

			scaled_xoff = scanp->xoffset << 
				(scannerp->natural_xresolution - 
				 scanp->xresolution);

			ccd += scaled_xoff;	

			for (colour = 0; colour < 3; colour++)
			{
				/* Address of pixel under scrutiny */
				pixel_address = 
					(scanline * image->width * 3 * 2) +
					(pixelnum * 3 * 2) + (colour * 2);

				/* Dark value is easy 
				 * Range of lo is 0-18k */
				lo = (scannerp->blackweight[ccd]) * 3;

				/* Light value depends on the colour, 
				 * and is an average in greyscale mode. */
				if (scanp->mode == 1) /* RGB */
				{
					switch (colour)
					{
						case 0: hi = scannerp->redweight[ccd] * 3; 
							break;
						case 1: hi = scannerp->greenweight[ccd] * 3; 
							break;
						default: hi = scannerp->blueweight[ccd] * 3; 
							 break;
					}
				}
				else /* Grey */
				{
					hi = (scannerp->redweight[ccd] + 
							scannerp->greenweight[ccd] + 
							scannerp->blueweight[ccd]);
				}

				/* Check for bad calibration data as it 
				   can cause a divide-by-0 error */
				if (hi <= lo)
				{
					DBG(1, "adjust_output: Bad cal data!"
							" hi: %ld lo: %ld\n"
							"Recalibrate, that "
							"should fix it.\n", 
							hi, lo);
					return -1;
				}

				/* Start with the pixel value in result */
				result = MAKE_SHORT(*(image->image_data + 
							pixel_address), 
						*(image->image_data + 
							pixel_address + 1));

				result = result >> 6; /* Range now = 0-1023 */
				if (scanline == 10)
					DBG(200, "adjust_output: Initial pixel"
							" value: %ld\n", 
							result);
				result *= 54;         /* Range now = 0-54k */

				/* Clip to dark and light values */
				if (result < lo) result = lo;
				if (result > hi) result = hi;

				/* result = (base-lo) * max_value / (hi-lo) */
				temp = result - lo;
				temp *= 65536;
				temp /= (hi - lo);

				/* Clip output  result has been clipped to lo,
				 * and hi >= lo, so temp can't be < 0 */
				if (temp > 65535)
					temp = 65535;
				if (scanline == 10)
				{
					DBG(200, "adjust_output: %d: base = "
							"%lu, result %lu (%lu "
							"- %lu)\n", pixelnum, 
							result, temp, lo, hi);
														}			
					result = temp;

					/* Store the value back where it came 
					 * from (always bigendian) */
					*(image->image_data + pixel_address)
						= HIGH_BYTE(result);
					*(image->image_data + pixel_address+1)
						= LOW_BYTE(result);
			}
		}
	}
	DBG(100, "Finished adjusting output\n");
	return 0;
}


int sanei_canon_pp_calibrate(scanner_parameters *sp, char *cal_file) 
{
	int count, readnum, colournum, scanlinenum;
	int outfile;

	int scanline_size;

	const int scanline_count = 6;
	const int calibration_reads = 3;

	unsigned char command_buffer[10];

	image_segment image;
	unsigned char *databuf;

	/* Calibration data is monochromatic (greyscale format) */
	scanline_size = sp->scanheadwidth * 1.25;

	DBG(40, "Calibrating %ix%i pixels calibration image "
			"(%i bytes each scan).\n", 
			sp->scanheadwidth, scanline_count, 
			scanline_size * scanline_count);

	/* Allocate memory for calibration data */	
	sp->blackweight = (unsigned long *)
		calloc(sizeof(unsigned long), sp->scanheadwidth);
	sp->redweight = (unsigned long *)
		calloc(sizeof(unsigned long), sp->scanheadwidth);
	sp->greenweight = (unsigned long *)
		calloc(sizeof(unsigned long), sp->scanheadwidth);
	sp->blueweight = (unsigned long *)
		calloc(sizeof(unsigned long), sp->scanheadwidth);

	/* The data buffer needs to hold a number of 
	   images (calibration_reads) per colour, each 
	   sp->scanheadwidth x scanline_count */
	databuf = (unsigned char *)
		malloc(scanline_size * scanline_count * calibration_reads * 3);

	/* And we allocate space for converted image data in this
	   image_segment. */
	image.image_data = (unsigned char *)malloc(scanline_count * 
			sp->scanheadwidth * 2 * calibration_reads);
	image.width = sp->scanheadwidth;
	image.height = scanline_count * calibration_reads;

	DBG(40, "Calibrating black level:\n");
	for (readnum = 0; readnum < calibration_reads; readnum++)
	{
		DBG(40, "Scan number %i\n", readnum + 1);
		do
		{
			/* Send the "dark calibration" command */
			memcpy(command_buffer, command_8, 10);

			/* Which includes the size of data we expect the 
			 * scanner to return */
			command_buffer[7] = ((scanline_size * scanline_count) 
					& 0xff00) >> 8;
			command_buffer[8] = (scanline_size * scanline_count) 
				& 0xff;

			sanei_canon_pp_write(sp->port, 10, command_buffer);

		} while (sanei_canon_pp_check_status(sp->port) == 1);

		/* Black reference data */
		sanei_canon_pp_read(sp->port, scanline_size * scanline_count,
				databuf + 
				(readnum * scanline_size * scanline_count));
	}

	/* Convert scanner format to a greyscale 16bpp image */
	for (scanlinenum = 0; 
			scanlinenum < scanline_count * calibration_reads; 
			scanlinenum++)
	{
		convdata(databuf + (scanlinenum * scanline_size), 
				image.image_data + 
				(scanlinenum * sp->scanheadwidth*2), 
				sp->scanheadwidth, 1);
	}

	/* Take column totals */
	for (count = 0; count < sp->scanheadwidth; count++)
	{
		sp->blackweight[count] = column_sum(&image, count, 3) >> 6;
	}

#if 0
	/* DEBUGGING CODE */
	/* Make an RGB image 1 pixel tall from gain values */
	convert_to_rgb(&image, databuf, sp->scanheadwidth, 1, 1);
	outfile = open("blackdata.tga", O_WRONLY | O_TRUNC | O_CREAT, 0666);
	write_tga_header(outfile, sp->scanheadwidth, 1, 8);
	write_tga_lines(outfile, sp->scanheadwidth, 1, image.image_data, 0, 1);
	close(outfile);
#endif

	/* Some unknown commands */
	DBG(40, "Sending Unknown request 1\n");
	do
	{
		usleep(100000);
		sanei_canon_pp_write(sp->port, 10, command_9);
	} while (sanei_canon_pp_check_status(sp->port) == 1);

	DBG(40, "Sending Unknown request 2\n");
	do
	{
		usleep(100000);
		sanei_canon_pp_write(sp->port, 10, command_10);
	} while (sanei_canon_pp_check_status(sp->port) == 1);

	DBG(40, "Reading white-balance/gamma data\n");
	sanei_canon_pp_read(sp->port, 32, sp->gamma);
	for (count = 0; count < 32; count++)
	{
		DBG(40, "%02x ", sp->gamma[count]);
		if (count % 20 == 19) 
			DBG(10, "\n");
	}
	DBG(40, "\n");

	/* Now for the RGB high-points */
	for (colournum = 1; colournum < 4; colournum++)
	{
		for (readnum = 0; readnum < 3; readnum++)
		{
			DBG(10, "Colour number: %i, scan number %i:\n", 
					colournum, readnum + 1);
			do
			{
				memcpy(command_buffer, command_11, 10);
				command_buffer[3] = colournum;
				/* Set up returned data size */
				command_buffer[7] = 
					((scanline_size * scanline_count) 
					 & 0xff00) >> 8;
				command_buffer[8] = (scanline_size * 
						scanline_count) & 0xff;

				sanei_canon_pp_write(sp->port, 10, 
						command_buffer);
			} while (sanei_canon_pp_check_status(sp->port) == 1);

			sanei_canon_pp_read(sp->port, scanline_size * 
					scanline_count, databuf + 
					(readnum * scanline_size * 
					 scanline_count));

		}

		/* Convert colour data from scanner format to RGB data */
		for (scanlinenum = 0; scanlinenum < scanline_count * 
				calibration_reads; scanlinenum++)
		{
			convdata(databuf + (scanlinenum * scanline_size), 
					image.image_data + 
					(scanlinenum * sp->scanheadwidth * 2), 
					sp->scanheadwidth, 1);
		}

		/* Sum each column of the image and store the results in sp */
		for (count = 0; count < sp->scanheadwidth; count++)
		{
			if (colournum == 1)
				sp->redweight[count] = 
					column_sum(&image, count, 3) >> 6;
			else if (colournum == 2)
				sp->greenweight[count] = 
					column_sum(&image, count, 3) >> 6;
			else
				sp->blueweight[count] = 
					column_sum(&image, count, 3) >> 6;	
		}

	}

	/* cal_file == NUL indicates we want an in-memory scan only */
	if (cal_file != NULL)
	{
		outfile = open(cal_file, O_WRONLY | O_TRUNC | O_CREAT, 0666);

		/* Header */
		if (write(outfile, header, strlen(header) + 1) == -1)
			DBG(10, "Write Error on calibration file %s", cal_file);
		if (write(outfile, &fileversion, sizeof(int)) == -1)
			DBG(10, "Write Error on calibration file %s", cal_file);

		/* Data */
		if (write(outfile, &(sp->scanheadwidth), 
					sizeof(sp->scanheadwidth)) == -1)
			DBG(10, "Write Error on calibration file %s", cal_file);
		if (write(outfile, sp->blackweight, sp->scanheadwidth * 
					sizeof(long)) == -1)
			DBG(10, "Write Error on calibration file %s", cal_file);
		if(write(outfile, sp->redweight, sp->scanheadwidth * 
					sizeof(long)) == -1)
			DBG(10, "Write Error on calibration file %s", cal_file);
		if(write(outfile, sp->greenweight, sp->scanheadwidth * 
					sizeof(long)) == -1)
			DBG(10, "Write Error on calibration file %s", cal_file);
		if(write(outfile, sp->blueweight, sp->scanheadwidth * 
					sizeof(long)) == -1)
			DBG(10, "Write Error on calibration file %s", cal_file);
		if(write(outfile, sp->gamma, 32) == -1)
			DBG(10, "Write Error on calibration file %s", cal_file);

		close(outfile);
	}

	free(databuf);
	free(image.image_data);

	return 0;
}

static unsigned long column_sum(image_segment *image, int x, int colournum)
	/* Colournum is 0 for red, 1 for green, 2 for blue, 
	 * 3 for greyscale data.  This gives us a number from 
	 * 0-n*1024 where n is the height of the image */
{
	unsigned int scanline;
	unsigned long total = 0;
	unsigned int temp;

	for (scanline = 0; scanline < image->height; scanline++)
	{
		/* Greyscale 16bpp */
		if (colournum >= 3)
		{
			/* High 2 bits */
			temp = (int)image->image_data[
				(scanline * (image->width) * 2) +
				(x * 2)] << 8;

			/* Low 8 bits */
			temp += image->image_data[
				(scanline * (image->width) * 2) +
				(x * 2) + 1];
		}
		/* RGB 48bpp */
		else
		{		
			/* High 2 bits */
			temp = image->image_data[
				(scanline * image->width * 3 * 2) +
				(x * 3 * 2) + (colournum * 2)] << 8;
			/* Low 8 bits */
			temp |= image->image_data[
				(scanline * image->width * 3 * 2) +
				(x * 3 * 2) + (colournum * 2) + 1];
		}
		total += temp;

	}

	return total;

}


static int scanner_setup_params(unsigned char *buf, scanner_parameters *sp, 
		scan_parameters *scanp)
{
	int scaled_width, scaled_height;
	int scaled_xoff, scaled_yoff;

	/* Natural resolution (I think) */
	if (sp->scanheadwidth == 2552)
	{
		buf[0] = 0x11; /* 300 | 0x1000 */
		buf[1] = 0x2c;
		buf[2] = 0x11;
		buf[3] = 0x2c;
	} else {
		buf[0] = 0x12; /* 600 | 0x1000*/
		buf[1] = 0x58;
		buf[2] = 0x12;
		buf[3] = 0x58;
	}

	scaled_width = scanp->width << 
		(sp->natural_xresolution - scanp->xresolution);
	/* YO! This needs fixing if we ever use yresolution! */
	scaled_height = scanp->height << 
		(sp->natural_xresolution - scanp->xresolution);
	scaled_xoff = scanp->xoffset << 
		(sp->natural_xresolution - scanp->xresolution);
	scaled_yoff = scanp->yoffset << 
		(sp->natural_xresolution - scanp->xresolution);

	/* Input resolution */
	buf[4] = (((75 << scanp->xresolution) & 0xff00) >> 8) | 0x10;
	buf[5] = (75 << scanp->xresolution) & 0xff;
	/* Interpolated resolution */
	buf[6] = (((75 << scanp->xresolution) & 0xff00) >> 8) | 0x10;;
	buf[7] = (75 << scanp->xresolution) & 0xff;

	/* X offset */
	buf[8] = (scaled_xoff & 0xff000000) >> 24;
	buf[9] = (scaled_xoff & 0xff0000) >> 16;
	buf[10] = (scaled_xoff & 0xff00) >> 8;
	buf[11] = scaled_xoff & 0xff;

	/* Y offset */
	buf[12] = (scaled_yoff & 0xff000000) >> 24;
	buf[13] = (scaled_yoff & 0xff0000) >> 16;
	buf[14] = (scaled_yoff & 0xff00) >> 8;
	buf[15] = scaled_yoff & 0xff;

	/* Width of image to be scanned */	
	buf[16] = (scaled_width & 0xff000000) >> 24;
	buf[17] = (scaled_width & 0xff0000) >> 16;
	buf[18] = (scaled_width & 0xff00) >> 8;
	buf[19] = scaled_width & 0xff;

	/* Height of image to be scanned */
	buf[20] = (scaled_height & 0xff000000) >> 24;
	buf[21] = (scaled_height & 0xff0000) >> 16;
	buf[22] = (scaled_height & 0xff00) >> 8;
	buf[23] = scaled_height & 0xff;


	/* These appear to be the only two colour mode possibilities. 
	   Pure black-and-white mode probably just uses greyscale and
	   then gets its contrast adjusted by the driver. I forget. */
	if (scanp->mode == 1) /* Truecolour */
		buf[24] = 0x08;
	else /* Greyscale */
		buf[24] = 0x04;

	return 0;
}

int sanei_canon_pp_abort_scan(scanner_parameters *sp, scan_parameters *scanp)
{
	/* The abort command (hopefully) */
	sanei_canon_pp_write(sp->port, 10, command_12);

	sanei_canon_pp_check_status(sp->port);

	/* Stop cc from warning us about unused params */
	scanp->mode = scanp->mode;

	return 0;
}

/* adjust_gamma: Upload a gamma profile to the scanner */
int sanei_canon_pp_adjust_gamma(scanner_parameters *sp)
{
	sp->gamma[31] = check8(sp->gamma, 31);
	if (sanei_canon_pp_write(sp->port, 10, command_13))
		return -1;
	if (sanei_canon_pp_write(sp->port, 32, sp->gamma))
		return -1;

	return 0;
}

static int sleep_scanner(struct parport *port)
{
	/* *SCANEND Command - puts scanner to sleep */
	sanei_canon_pp_write(port, 10, command_7);
	sanei_canon_pp_check_status(port);

	return 0;
	/* FIXME: I murdered Simon's code here */
	/* expect(port, "Enter Transparent Mode", 0x1f, 0x1f, 1000000); */
}

int sanei_canon_pp_detect(struct parport *port)
{
	/*int caps;*/
	/* This code needs to detect whether or not a scanner is present on 
	 * the port, quickly and reliably. Fast version of 
	 * sanei_canon_pp_initialise() 
	 *
	 * If this detect returns true, a more comprehensive check will 
	 * be conducted
	 * Return values: 
	 * 0 = scanner present
	 * anything else = scanner not present 
	 * PRE: port is open/unclaimed
	 * POST: port is closed/unclaimed
	 */

	/* port is already open, just need to claim it */

	if (ieee1284_claim(port) != E1284_OK)
	{
		DBG(0,"detect: Unable to claim port\n");
		return 2;
	}
	if (sanei_canon_pp_wake_scanner(port))
	{
		DBG(10, "detect: could not wake scanner\n");
		ieee1284_release(port);
		return 3;
	}
	/* sanei_canon_pp_sleep_scanner(port); */
	ieee1284_release(port);
	/* ieee1284_close(port); */

	return 0;
}
