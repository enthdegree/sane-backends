/* lexmark-x1100.c: scanner-interface file for x1100 Lexmark scanners.

   (C) 2005 Fred Odendaal
   
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

   **************************************************************************/

#include "../include/sane/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"

#include "../include/sane/sanei_usb.h"

#define DEBUG_DECLARE_ONLY
#include "lexmark.h"
#include "../include/sane/sanei_backend.h"



typedef enum
{
  black = 0,
  white
}
region_type;

#define HomeEdgePoint1 1235
#define HomeEdgePoint2 1258
#define HomeTolerance 30

/* F.O. Per device globals - moved to lexmark.h as part of Lexmark_Device */
/* static SANE_Byte *transfer_buffer = NULL; */
/* static size_t bytes_remaining = 0; */
/* static size_t bytes_in_buffer = 0; */
/* static SANE_Byte *read_pointer; */
/* static Read_Buffer *read_buffer = NULL; */
/* static int image_line_no = 0; */



static SANE_Byte shadow_regs[] = {
  0x00, 0x43, 0x9f, 0xa1, 0xa3, 0x9f, 0xa1, 0xa3,
  0x09, 0x0a, 0x06, 0x70, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x01, 0x0f, 0x01, 0x00, 0x01, 0x0f, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xeb, 0xee, 0xf7, 0x01, 0x0f, 0x51, 0x86, 0x11,
  0x48, 0x00, 0x00, 0x01, 0x0b, 0x01, 0x0b, 0x01,
  0x0a, 0x07, 0x20, 0x37, 0x88, 0x08, 0x00, 0x00,
  0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x22, 0x00, 0x12, 0x03, 0x01, 0x80, 0x68, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x70, 0x07, 0x00, 0x00,
  0x00, 0x00, 0x05, 0x00, 0x0e, 0x00, 0x00, 0x00,
  0x00, 0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x05, 0x05, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0xff, 0x02, 0x01, 0x60, 0x80,
  0x00, 0x8c, 0x40, 0x06, 0x0e, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0xcc, 0x27, 0x24, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xb2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x80, 0x83, 0x20, 0x0e, 0x09, 0x00,
  0x04, 0x3a, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x61, 0x0a, 0xed, 0x02, 0x29, 0x0a, 0x0e, 0x29,
  0x03, 0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x00, 0xf8, 0x7f, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* Static x1100 function proto-types */
static SANE_Status x1100_usb_bulk_write (SANE_Int devnum, 
					 SANE_Byte * cmd,
					 size_t * size);
static SANE_Status x1100_usb_bulk_read (SANE_Int devnum, 
					SANE_Byte * buf,
					size_t * size);
static SANE_Status x1100_write_all_regs (SANE_Int devnum);
static SANE_Bool x1100_is_home_line (unsigned char *buffer);
static SANE_Status x1100_get_start_loc (SANE_Int resolution, 
					SANE_Int * vert_start, 
					SANE_Int * hor_start, 
					SANE_Int offset);
static void x1100_rewind (Lexmark_Device * dev);
static SANE_Status x1100_start_mvmt (SANE_Int devnum);
static SANE_Status x1100_stop_mvmt (SANE_Int devnum);
static SANE_Status x1100_clr_c6 (SANE_Int devnum);
static void x1100_set_scan_area (SANE_Int res, 
				 SANE_Bool isColour, 
				 SANE_Int height,
				 SANE_Int width, 
				 SANE_Int offset);

/* Static Read Buffer Proto-types */
static SANE_Status read_buffer_init (Lexmark_Device *dev, int bytesperline);
static SANE_Status read_buffer_free (Read_Buffer *rb);
static size_t read_buffer_bytes_available (Read_Buffer *rb);
static SANE_Status read_buffer_add_byte (Read_Buffer *rb, 
					 SANE_Byte * byte_pointer);
static SANE_Status read_buffer_add_byte_gray (Read_Buffer *rb,
					      SANE_Byte * byte_pointer);
static size_t read_buffer_get_bytes (Read_Buffer *rb, SANE_Byte * buffer, 
				     size_t rqst_size);
static SANE_Bool read_buffer_is_empty (Read_Buffer *rb);

SANE_Status
sanei_lexmark_x1100_init (void)
{

  /* initialize sanei usb functions */
  sanei_usb_init ();
  return SANE_STATUS_GOOD;
}

void
sanei_lexmark_x1100_destroy (Lexmark_Device *dev)
{
  /* free the read buffer */
  if (dev->read_buffer != NULL)
    read_buffer_free (dev->read_buffer);
}


SANE_Status
x1100_usb_bulk_write (SANE_Int devnum, SANE_Byte * cmd, size_t * size)
{
  SANE_Status status;
  size_t cmd_size;

  cmd_size = *size;
  status = sanei_usb_write_bulk (devnum, cmd, size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (5,
	   "x1100_usb_bulk_write: returned %s (size = %ld, expected %d)\n",
	   sane_strstatus (status), (long int) *size, cmd_size);
      /* F.O. should reset the pipe here... */
    }
  return status;
}

SANE_Status
x1100_usb_bulk_read (SANE_Int devnum, SANE_Byte * buf, size_t * size)
{
  SANE_Status status;
  size_t exp_size;

  exp_size = *size;
  status = sanei_usb_read_bulk (devnum, buf, size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (5,
	   "x1100_usb_bulk_read: returned %s (size = %ld, expected %d)\n",
	   sane_strstatus (status), (long int) *size, exp_size);
      /* F.O. should reset the pipe here... */
    }
  return status;
}

SANE_Status
x1100_start_mvmt (SANE_Int devnum)
{
  static SANE_Byte startscan_command_block[] = {
    0x88, 0x2c, 0x00, 0x01, 0x0f,
    0x88, 0xb3, 0x00, 0x01, 0x02,
    0x88, 0xb3, 0x00, 0x01, 0x02,
    0x88, 0xb3, 0x00, 0x01, 0x00,
    0x88, 0xb3, 0x00, 0x01, 0x00,
    0x88, 0xb3, 0x00, 0x01, 0x68,
    0x88, 0xb3, 0x00, 0x01, 0x68,
  };
  size_t cmd_size;

  /* Colour depth = 0f; Start moving scanner: */
  cmd_size = 0x23;
  return x1100_usb_bulk_write (devnum, startscan_command_block, &cmd_size);
}

SANE_Status
x1100_stop_mvmt (SANE_Int devnum)
{
  static SANE_Byte stopscan_command_block[] = {
    0x88, 0xb3, 0x00, 0x01, 0x02,
    0x88, 0xb3, 0x00, 0x01, 0x02,
    0x88, 0xb3, 0x00, 0x01, 0x00,
    0x88, 0xb3, 0x00, 0x01, 0x00
  };
  size_t cmd_size;

  /* Stop scanner - clear reg 0xb3: */
  cmd_size = 0x14;
  return x1100_usb_bulk_write (devnum, stopscan_command_block, &cmd_size);

}

SANE_Status
x1100_clr_c6 (SANE_Int devnum)
{
  static SANE_Byte clearC6_command_block[] = {
    0x88, 0xc6, 0x00, 0x01, 0x00
  };
  size_t cmd_size;

  /* Clear register 0xC6 */
  cmd_size = 0x05;
  return x1100_usb_bulk_write (devnum, clearC6_command_block, &cmd_size);

}

SANE_Status
sanei_lexmark_x1100_open_device (SANE_String_Const devname, SANE_Int * devnum)
{
  /* This function calls the Sane Interface to open this usb device.
     It also needlessly does what the Windows driver does and reads
     the entire register set - this may be removed. */

  SANE_Status result;
  static SANE_Byte command_block[] = { 0x80, 0, 0x00, 0xFF };
  SANE_Byte registers[0x100];
  size_t size;

  result = sanei_usb_open (devname, devnum);
  DBG (2, "sanei_lexmark_x1100_open_device: devnum=%d\n", *devnum);

  /* dump registers F.O. This is needless... */
  size = 4;
  x1100_usb_bulk_write (*devnum, command_block, &size);
  size = 0xFF;
  x1100_usb_bulk_read (*devnum, registers, &size);

  return result;
}

void
sanei_lexmark_x1100_close_device (SANE_Int devnum)
{
  /* This function calls the Sane USB library to close this usb device */

  sanei_usb_close (devnum);
  return;
}


SANE_Status
x1100_write_all_regs (SANE_Int devnum)
{
  /* This function writes the contents of the shadow registers to the 
     scanner. */
  int i;
  size_t size;
  static SANE_Byte command_block1[0xb7];
  static SANE_Byte command_block2[0x4f];
  command_block1[0] = 0x88;
  command_block1[1] = 0x00;
  command_block1[2] = 0x00;
  command_block1[3] = 0xb3;
  for (i = 0; i < 0xb3; i++)
    {
      command_block1[i + 4] = shadow_regs[i];
    }
  command_block2[0] = 0x88;
  command_block2[1] = 0xb4;
  command_block2[2] = 0x00;
  command_block2[3] = 0x4b;
  for (i = 0; i < 0x4b; i++)
    {
      command_block2[i + 4] = shadow_regs[i + 0xb4];
    }
  size = 0xb7;
  x1100_usb_bulk_write (devnum, command_block1, &size);
  size = 0x4f;
  x1100_usb_bulk_write (devnum, command_block2, &size);
  return SANE_STATUS_GOOD;
}

SANE_Bool
x1100_is_home_line (unsigned char *buffer)
{
  /*
     This function assumes the buffer has a size of 2500 bytes.It is 
     destructive to the buffer.

     Here is what it does:

     Go through the buffer finding low and high values, which are computed by 
     comparing to the average: 
     average = (lowest value + highest value)/2
     High bytes are changed to 0xFF (white), lower or equal bytes are changed 
     to 0x00 (black),so that the buffer only contains white (0xFF) or black 
     (0x00) values.

     Next, we go through the buffer. We use a tolerance of 5 bytes on each end
     of the buffer and check a region from bytes 5 to 2495. We start assuming
     we are in a white region and look for the start of a black region. We save
     this index as the transition from white to black. We also save where we 
     change from black back to white. We continue checking for transitions 
     until the end of the check region. If we don't have exactly two 
     transitions when we reach the end we return SANE_FALSE.

     The final check compares the transition indices to the nominal values
     plus or minus the tolerence. For the first transition (white to black
     index) the value must lie in the range 1235-30 (1205) to 1235+30 (1265).
     For the second transition (black to white) the value must lie in the range
     1258-30 (1228) to 1258+30 (1288). If the indices are out of range we 
     return SANE_FALSE. Otherwise, we return SANE_TRUE.
   */


  unsigned char max_byte = 0;
  unsigned char min_byte = 0xFF;
  unsigned char average;
  int i;

  region_type region;
  int transition_counter;
  int index1 = 0;
  int index2 = 0;
  int low_range, high_range;

  /* Find the max and the min */
  for (i = 0; i < 2500; i++)
    {
      if (*(buffer + i) > max_byte)
	max_byte = *(buffer + i);
      if (*(buffer + i) < min_byte)
	min_byte = *(buffer + i);
    }

  /* The average */
  average = ((max_byte + min_byte) / 2);

  /* Set bytes as white (0xFF) or black (0x00) */
  for (i = 0; i < 2500; i++)
    {
      if (*(buffer + i) > average)
	*(buffer + i) = 0xFF;
      else
	*(buffer + i) = 0x00;
    }

  region = white;
  transition_counter = 0;

  /* Go through the check region - bytes 5 to 2495 */
  for (i = 5; i <= 2495; i++)
    {
      /* Check for transition to black */
      if ((region == white) && (*(buffer + i) == 0))
	{
	  if (transition_counter < 2)
	    {
	      region = black;
	      index1 = i;
	      transition_counter++;
	    }
	  else
	    {
	      return SANE_FALSE;
	    }
	}
      /* Check for transition to white */
      else if ((region == black) && (*(buffer + i) == 0xFF))
	{
	  if (transition_counter < 2)
	    {
	      region = white;
	      index2 = i;
	      transition_counter++;
	    }
	  else
	    {
	      return SANE_FALSE;
	    }
	}
    }

  /* Check that the number of transitions is 2 */
  if (transition_counter != 2)
    {
      return SANE_FALSE;
    }

  /* Check that the 1st index is in range */
  low_range = HomeEdgePoint1 - HomeTolerance;
  high_range = HomeEdgePoint1 + HomeTolerance;

  if ((index1 < low_range) || (index1 > high_range))
    {
      return SANE_FALSE;
    }

  /* Check that the 2nd index is in range */
  low_range = HomeEdgePoint2 - HomeTolerance;
  high_range = HomeEdgePoint2 + HomeTolerance;

  if ((index2 < low_range) || (index2 > high_range))
    {
      return SANE_FALSE;
    }

  /* We made it this far, so its a good home line. Return True */
  return SANE_TRUE;

}

void
sanei_lexmark_x1100_move_fwd (SANE_Int distance, Lexmark_Device * dev)
{
  /*
     This function moves the scan head forward with the highest vertical 
     resolution of 1200dpi. The distance moved is given by the distance
     parameter. 

     As an example, given a distance parameter of 600, the scan head will
     move 600/1200", or 1/2" forward.
  */

  static SANE_Byte pollstopmoving_command_block[] =
    { 0x80, 0xb3, 0x00, 0x01 };

  int i;
  size_t cmd_size;
  SANE_Int devnum;
  SANE_Bool scan_head_moving;
  SANE_Byte read_result;

  devnum = dev->devnum;
  /* Clear all the shadow registers */
  for (i = 0; i < 255; i++)
    shadow_regs[i] = 0;
  /* Some regiters are always set the same */
  shadow_regs[0x01] = 0x43;
  shadow_regs[0x0b] = 0x70;
  shadow_regs[0x11] = 0x01;
  shadow_regs[0x12] = 0x0f;
  shadow_regs[0x13] = 0x01;
  shadow_regs[0x15] = 0x01;
  shadow_regs[0x16] = 0x0f;
  shadow_regs[0x1d] = 0x20;
  shadow_regs[0x28] = 0xeb;
  shadow_regs[0x29] = 0xee;
  shadow_regs[0x2a] = 0xf7;
  shadow_regs[0x2b] = 0x01;
  shadow_regs[0x2c] = 0x00;	/*not the same */
  shadow_regs[0x2d] = 0x41;	/*not the same */
  shadow_regs[0x2e] = 0x86;
  shadow_regs[0x30] = 0x48;
  shadow_regs[0x33] = 0x01;
  shadow_regs[0x3a] = 0x20;
  shadow_regs[0x3b] = 0x37;
  shadow_regs[0x3c] = 0x88;
  shadow_regs[0x3d] = 0x08;
  shadow_regs[0x40] = 0x80;
  shadow_regs[0x65] = 0x80;
  shadow_regs[0x72] = 0x05;
  shadow_regs[0x74] = 0x0e;
  shadow_regs[0x8b] = 0x00;	/*not the same */
  shadow_regs[0x8c] = 0x00;	/*not the same */
  shadow_regs[0x8d] = 0x01;
  shadow_regs[0x8e] = 0x60;
  shadow_regs[0x8f] = 0x80;
  shadow_regs[0x94] = 0x0e;
  shadow_regs[0xa3] = 0xcc;
  shadow_regs[0xa4] = 0x27;
  shadow_regs[0xa5] = 0x24;
  shadow_regs[0xb0] = 0xb2;
  shadow_regs[0xc2] = 0x80;
  shadow_regs[0xc4] = 0x20;
  shadow_regs[0xc8] = 0x04;
  shadow_regs[0xf4] = 0xf8;
  shadow_regs[0xf5] = 0x7f;

  /* set grayscale scan + ?  */
  shadow_regs[0x2f] = 0xa1;

  /* set ? */
  shadow_regs[0x34] = 0x50;
  shadow_regs[0x35] = 0x01;
  shadow_regs[0x36] = 0x50;
  shadow_regs[0x37] = 0x01;
  shadow_regs[0x38] = 0x50;
  /* set motor resolution divisor */
  shadow_regs[0x39] = 0x00;
  /* set vertical start/end positions */
  shadow_regs[0x60] = (SANE_Byte) ((distance - 1) & 0x00FF);
  shadow_regs[0x61] = (SANE_Byte) ((distance - 1) >> 8 & 0x00FF);
  shadow_regs[0x62] = (SANE_Byte) (distance & 0x00FF);
  shadow_regs[0x63] = (SANE_Byte) (distance >> 8 & 0x00FF);
  /* set horizontal start position */
  shadow_regs[0x66] = 0x64;
  shadow_regs[0x67] = 0x00;
  /* set horizontal end position */
  shadow_regs[0x6c] = 0xc8;
  shadow_regs[0x6d] = 0x00;
  /* set horizontal resolution */
  shadow_regs[0x79] = 0x40;
  shadow_regs[0x7a] = 0x01;
  /* ???? */
  shadow_regs[0x93] = 0x06;
  /* don't buffer data for this scan */
  shadow_regs[0xb2] = 0x04;
  /* Motor enable & Coordinate space denominator */
  shadow_regs[0xc3] = 0x81;
  /* ? */
  shadow_regs[0xc5] = 0x0a;
  /* Movement direction & step size */
  shadow_regs[0xc6] = 0x09;
  /* step size range2 */
  shadow_regs[0xc9] = 0x3b;
  /* ? */
  shadow_regs[0xca] = 0x0a;
  /* motor curve stuff */

  shadow_regs[0xe2] = 0x09;
  shadow_regs[0xe3] = 0x1a;
  shadow_regs[0xe6] = 0xdc;
  shadow_regs[0xe9] = 0x1b;
  shadow_regs[0xec] = 0x07;
  shadow_regs[0xef] = 0x03;


  /* Stop scanner - clear reg 0xb3: */
  x1100_stop_mvmt (devnum);

  /* Stop scanner - clear reg 0xb3: - Not sure this is necessary */
  x1100_stop_mvmt (devnum);

  /* Clear C6 why?: - Not sure this is necessary  */
  x1100_clr_c6 (devnum);

/* Move Forward without scanning: */
  shadow_regs[0x32] = 0x00;
  x1100_write_all_regs (devnum);
  shadow_regs[0x32] = 0x40;
  x1100_write_all_regs (devnum);

  /* Stop scanner - clear reg 0xb3: */
  x1100_stop_mvmt (devnum);

  /* Colour depth = 0 (F.O Is this from logfile?); Start moving scanner: */
  x1100_start_mvmt (devnum);

  /* Poll for scanner stopped - return value(3:0) = 0: */
  scan_head_moving = SANE_TRUE;
  while (scan_head_moving)
    {
      cmd_size = 0x04;
      x1100_usb_bulk_write (devnum, pollstopmoving_command_block, &cmd_size);
      cmd_size = 0x1;
      x1100_usb_bulk_read (devnum, &read_result, &cmd_size);
      if ((read_result & 0xF) == 0x0)
	{
	  scan_head_moving = SANE_FALSE;
	}
    }
}

SANE_Bool
sanei_lexmark_x1100_search_home_fwd (Lexmark_Device * dev)
{
  /* This function actually searches backwards one line looking for home */

  SANE_Int devnum;
  int i;
  SANE_Byte poll_result[3];
  SANE_Byte *buffer;
  SANE_Byte temp_byte;

  static SANE_Byte command4_block[] = { 0x90, 0x00, 0x00, 0x03 };

  static SANE_Byte command5_block[] = { 0x91, 0x00, 0x09, 0xc4 };

  size_t cmd_size;
  SANE_Bool got_line;
  SANE_Bool ret_val;

  devnum = dev->devnum;

  DBG (2, "sanei_lexmark_x1100_search_home_fwd:\n");


  /* Clear all the shadow registers */
  for (i = 0; i < 255; i++)
    shadow_regs[i] = 0;
  /* Some regiters are always set the same */
  shadow_regs[0x01] = 0x43;
  shadow_regs[0x0b] = 0x70;
  shadow_regs[0x11] = 0x01;
  shadow_regs[0x12] = 0x0f;
  shadow_regs[0x13] = 0x01;
  shadow_regs[0x15] = 0x01;
  shadow_regs[0x16] = 0x0f;
  shadow_regs[0x1d] = 0x20;
  shadow_regs[0x28] = 0xeb;
  shadow_regs[0x29] = 0xee;
  shadow_regs[0x2a] = 0xf7;
  shadow_regs[0x2b] = 0x01;
  shadow_regs[0x2c] = 0x0f;
  shadow_regs[0x2d] = 0x51;
  shadow_regs[0x2e] = 0x86;
  shadow_regs[0x30] = 0x48;
  shadow_regs[0x33] = 0x01;
  shadow_regs[0x3a] = 0x20;
  shadow_regs[0x3b] = 0x37;
  shadow_regs[0x3c] = 0x88;
  shadow_regs[0x3d] = 0x08;
  shadow_regs[0x40] = 0x80;
  shadow_regs[0x65] = 0x80;
  shadow_regs[0x72] = 0x05;
  shadow_regs[0x74] = 0x0e;
  shadow_regs[0x8b] = 0xff;
  shadow_regs[0x8c] = 0x02;
  shadow_regs[0x8d] = 0x01;
  shadow_regs[0x8e] = 0x60;
  shadow_regs[0x8f] = 0x80;
  shadow_regs[0x94] = 0x0e;
  shadow_regs[0xa3] = 0xcc;
  shadow_regs[0xa4] = 0x27;
  shadow_regs[0xa5] = 0x24;
  shadow_regs[0xb0] = 0xb2;
  shadow_regs[0xc2] = 0x80;
  shadow_regs[0xc4] = 0x20;
  shadow_regs[0xc8] = 0x04;
  shadow_regs[0xf4] = 0xf8;
  shadow_regs[0xf5] = 0x7f;
  /* set calibration */
  shadow_regs[0x02] = 0x80;
  shadow_regs[0x03] = 0x80;
  shadow_regs[0x04] = 0x80;
  shadow_regs[0x05] = 0x80;
  shadow_regs[0x06] = 0x80;
  shadow_regs[0x07] = 0x80;
  shadow_regs[0x08] = 0x06;
  shadow_regs[0x09] = 0x06;
  shadow_regs[0x0a] = 0x06;
  /* set grayscale  scan  */
  shadow_regs[0x2f] = 0x21;
  /* set ? */
  shadow_regs[0x34] = 0x04;
  shadow_regs[0x35] = 0x04;
  shadow_regs[0x36] = 0x08;
  shadow_regs[0x37] = 0x08;
  shadow_regs[0x38] = 0x0b;
  /* set motor resolution divisor */
  shadow_regs[0x39] = 0x07;
  /* set vertical start/end positions */
  shadow_regs[0x60] = 0x01;
  shadow_regs[0x61] = 0x00;
  shadow_regs[0x62] = 0x02;
  shadow_regs[0x63] = 0x00;
  /* set # of head moves per CIS read */
  shadow_regs[0x64] = 0x01;
  /* set horizontal start position */
  shadow_regs[0x66] = 0x6a;
  shadow_regs[0x67] = 0x00;
  /* set horizontal end position */
  shadow_regs[0x6c] = 0xf2;
  shadow_regs[0x6d] = 0x13;
  /* set horizontal resolution */
  shadow_regs[0x79] = 0x40;
  shadow_regs[0x7a] = 0x02;
  /* set for ? */
  shadow_regs[0x93] = 0x06;
  /* Motor enable & Coordinate space denominator */
  shadow_regs[0xc3] = 0x01;
  /* Movement direction & step size */
  shadow_regs[0xc6] = 0x01;
  /* step size range2 */
  shadow_regs[0xc9] = 0x3b;
  /* step size range0 */
  shadow_regs[0xe2] = 0x01;
  /* ? */
  shadow_regs[0xe3] = 0x03;


  /* Stop the scanner */
  /* cmd_size=0x14; */
  /* x1100_usb_bulk_write(devnum, command2_block, &cmd_size); */
  x1100_stop_mvmt (devnum);

  /* write regs out twice */
  shadow_regs[0x32] = 0x00;
  x1100_write_all_regs (devnum);
  shadow_regs[0x32] = 0x40;
  x1100_write_all_regs (devnum);

  /* Start Scan */
  /* cmd_size=0x23; */
  /* x1100_usb_bulk_write(devnum, command3_block, &cmd_size);  */
  x1100_start_mvmt (devnum);

  /* Poll the available byte count until not 0 */
  got_line = SANE_FALSE;
  while (!got_line)
    {
      cmd_size = 4;
      x1100_usb_bulk_write (devnum, command4_block, &cmd_size);
      cmd_size = 0x3;
      x1100_usb_bulk_read (devnum, poll_result, &cmd_size);
      if (!
	  (poll_result[0] == 0 && poll_result[1] == 0 && poll_result[2] == 0))
	{
	  /* if result != 00 00 00 we got data */
	  got_line = SANE_TRUE;
	}
    }

  /* create buffer for scan data */
  buffer = calloc (2500, sizeof (char));

  /* Tell the scanner to send the data */
  /* Write: 91 00 09 c4 */
  cmd_size = 4;
  x1100_usb_bulk_write (devnum, command5_block, &cmd_size);
  /* Read it */
  cmd_size = 0x09c4;
  x1100_usb_bulk_read (devnum, buffer, &cmd_size);

  /* Reverse order of bytes in words of buffer */
  for (i = 0; i < 2500; i = i + 2)
    {
      temp_byte = *(buffer + i);
      *(buffer + i) = *(buffer + i + 1);
      *(buffer + i + 1) = temp_byte;
    }

  /* check for home position */
  ret_val = x1100_is_home_line (buffer);

  if (ret_val)
    DBG (2, "sanei_lexmark_x1100_search_home_fwd: !!!HOME POSITION!!!\n");

  /*free the buffer */
  free (buffer);

  return ret_val;
}



SANE_Bool
sanei_lexmark_x1100_search_home_bwd (Lexmark_Device * dev)
{
/* This funtion must only be called if the scan head is past the home dot.
   It could damage the scanner if not.

   This function tells the scanner to do a grayscale scan backwards with a 
   300dpi resolution. It reads 2500 bytes of data between horizontal
   co-ordinates 0x6a and 0x13f2.

   The scan is set to read between vertical co-ordinates from 0x0a to 0x0f46,
   or 3900 lines. This equates to 13" at 300dpi, so we must stop the scan
   before it bangs against the end. A line limit is set so that a maximum of
   0x0F3C (13"*300dpi) lines can be read.

   To read the scan data we create a buffer space large enough to hold 10 
   lines of data. For each read we poll twice, ingnoring the first poll. This 
   is required for timing. We repeat the double poll until there is data
   available. The number of lines (or number of buffers in our buffer space)
   is calculated from the size of the data available from the scanner. The
   number of buffers is calculated as the space required to hold 1.5 times
   the the size of the data available from the scanner.

   After data is read from the scanner each line is checked if it is on the
   home dot. Lines are continued to be read until we are no longer on the home
   dot. */


  SANE_Int devnum;
  int i, j;
  SANE_Byte poll_result[3];
  SANE_Byte *buffer;
  SANE_Byte *buffer_start;
  SANE_Byte temp_byte;

  SANE_Int buffer_limit = 0xF3C;	/* should be a define */
  SANE_Int buffer_count = 0;
  SANE_Int size_requested;
  SANE_Int size_returned;
  SANE_Int no_of_buffers;
  SANE_Int high_byte, mid_byte, low_byte;
  SANE_Int home_line_count;
  SANE_Bool in_home_region;

  static SANE_Byte command4_block[] = { 0x90, 0x00, 0x00, 0x03 };

  static SANE_Byte command5_block[] = { 0x91, 0x00, 0xff, 0xc0 };

  size_t cmd_size;
  SANE_Bool got_line;

  devnum = dev->devnum;

  DBG (2, "sanei_lexmark_x1100_search_home_bwd:\n");
  /* Clear all the shadow registers */
  for (i = 0; i < 255; i++)
    shadow_regs[i] = 0;
  /* Some regiters are always set the same */
  shadow_regs[0x01] = 0x43;
  shadow_regs[0x0b] = 0x70;
  shadow_regs[0x11] = 0x01;
  shadow_regs[0x12] = 0x0f;
  shadow_regs[0x13] = 0x01;
  shadow_regs[0x15] = 0x01;
  shadow_regs[0x16] = 0x0f;
  shadow_regs[0x1d] = 0x20;
  shadow_regs[0x28] = 0xeb;
  shadow_regs[0x29] = 0xee;
  shadow_regs[0x2a] = 0xf7;
  shadow_regs[0x2b] = 0x01;
  shadow_regs[0x2c] = 0x0f;
  shadow_regs[0x2d] = 0x51;
  shadow_regs[0x2e] = 0x86;
  shadow_regs[0x30] = 0x48;
  shadow_regs[0x33] = 0x01;
  shadow_regs[0x3a] = 0x20;
  shadow_regs[0x3b] = 0x37;
  shadow_regs[0x3c] = 0x88;
  shadow_regs[0x3d] = 0x08;
  shadow_regs[0x40] = 0x80;
  shadow_regs[0x65] = 0x80;
  shadow_regs[0x72] = 0x05;
  shadow_regs[0x74] = 0x0e;
  shadow_regs[0x8b] = 0xff;
  shadow_regs[0x8c] = 0x02;
  shadow_regs[0x8d] = 0x01;
  shadow_regs[0x8e] = 0x60;
  shadow_regs[0x8f] = 0x80;
  shadow_regs[0x94] = 0x0e;
  shadow_regs[0xa3] = 0xcc;
  shadow_regs[0xa4] = 0x27;
  shadow_regs[0xa5] = 0x24;
  shadow_regs[0xb0] = 0xb2;
  shadow_regs[0xc2] = 0x80;
  shadow_regs[0xc4] = 0x20;
  shadow_regs[0xc8] = 0x04;
  shadow_regs[0xf4] = 0xf8;
  shadow_regs[0xf5] = 0x7f;
  /* set calibration */
  shadow_regs[0x02] = 0x80;
  shadow_regs[0x03] = 0x80;
  shadow_regs[0x04] = 0x80;
  shadow_regs[0x05] = 0x80;
  shadow_regs[0x06] = 0x80;
  shadow_regs[0x07] = 0x80;
  shadow_regs[0x08] = 0x06;
  shadow_regs[0x09] = 0x06;
  shadow_regs[0x0a] = 0x06;
  /* set grayscale  scan  */
  shadow_regs[0x2f] = 0x21;
  /* set ? */
  shadow_regs[0x34] = 0x07;
  shadow_regs[0x35] = 0x07;
  shadow_regs[0x36] = 0x0f;
  shadow_regs[0x37] = 0x0f;
  shadow_regs[0x38] = 0x15;
  /* set motor resolution divisor */
  shadow_regs[0x39] = 0x03;
  /* set vertical start/end positions */
  shadow_regs[0x60] = 0x0a;
  shadow_regs[0x61] = 0x00;
  shadow_regs[0x62] = 0x46;
  shadow_regs[0x63] = 0x0f;
  /* set # of head moves per CIS read */
  shadow_regs[0x64] = 0x02;
  /* set horizontal start position */
  shadow_regs[0x66] = 0x6a;
  shadow_regs[0x67] = 0x00;
  /* set horizontal end position */
  shadow_regs[0x6c] = 0xf2;
  shadow_regs[0x6d] = 0x13;
  /* set horizontal resolution */
  shadow_regs[0x79] = 0x40;
  shadow_regs[0x7a] = 0x02;
  /* set for ? */
  shadow_regs[0x85] = 0x20;
  shadow_regs[0x93] = 0x06;
  /* Motor enable & Coordinate space denominator */
  shadow_regs[0xc3] = 0x81;
  /* ? */
  shadow_regs[0xc5] = 0x19;
  /* Movement direction & step size */
  shadow_regs[0xc6] = 0x01;
  /* step size range2 */
  shadow_regs[0xc9] = 0x3a;
  /* ? */
  shadow_regs[0xca] = 0x08;
  /* motor curve stuff */
  shadow_regs[0xe0] = 0xe3;
  shadow_regs[0xe1] = 0x18;
  shadow_regs[0xe2] = 0x03;
  shadow_regs[0xe3] = 0x06;
  shadow_regs[0xe4] = 0x2b;
  shadow_regs[0xe5] = 0x17;
  shadow_regs[0xe6] = 0xdc;
  shadow_regs[0xe7] = 0xb3;
  shadow_regs[0xe8] = 0x07;
  shadow_regs[0xe9] = 0x1b;
  shadow_regs[0xec] = 0x07;
  shadow_regs[0xef] = 0x03;

  /* Stop the scanner */
  x1100_stop_mvmt (devnum);

  /* write regs out twice */
  shadow_regs[0x32] = 0x00;
  x1100_write_all_regs (devnum);
  shadow_regs[0x32] = 0x40;
  x1100_write_all_regs (devnum);

  /* Start Scan */
  x1100_start_mvmt (devnum);

  /* create buffer to hold up to 10 lines of  scan data */
  buffer = calloc (10 * 2500, sizeof (char));

  home_line_count = 0;
  in_home_region = SANE_FALSE;

  while (buffer_count < buffer_limit)
    {
      size_returned = 0;
      got_line = SANE_FALSE;
      while (!got_line)
	{
	  /* always poll twice (needed for timing) - disregard 1st poll */
	  cmd_size = 4;
	  x1100_usb_bulk_write (devnum, command4_block, &cmd_size);
	  cmd_size = 0x3;
	  x1100_usb_bulk_read (devnum, poll_result, &cmd_size);
	  cmd_size = 4;
	  x1100_usb_bulk_write (devnum, command4_block, &cmd_size);
	  cmd_size = 0x3;
	  x1100_usb_bulk_read (devnum, poll_result, &cmd_size);
	  if (!
	      (poll_result[0] == 0 && poll_result[1] == 0
	       && poll_result[2] == 0))
	    {
	      /* if result != 00 00 00 we got data */
	      got_line = SANE_TRUE;
	      high_byte = poll_result[2] << 16;
	      mid_byte = poll_result[1] << 8;
	      low_byte = poll_result[0];
	      size_returned = high_byte + mid_byte + low_byte;
	    }
	}

      size_requested = size_returned;
      no_of_buffers = size_returned * 3;
      no_of_buffers = no_of_buffers / 0x9C4;
      no_of_buffers = no_of_buffers >> 1;

      if (no_of_buffers < 1)
	no_of_buffers = 1;
      else if (no_of_buffers > 10)
	no_of_buffers = 10;
      buffer_count = buffer_count + no_of_buffers;
      size_requested = no_of_buffers * 0x9C4;

      /* Tell the scanner to send the data */
      /* Write: 91 <size_requested> */
      command5_block[1] = (SANE_Byte) (size_requested >> 16);
      command5_block[2] = (SANE_Byte) (size_requested >> 8);
      command5_block[3] = (SANE_Byte) (size_requested & 0xFF);

      cmd_size = 4;
      x1100_usb_bulk_write (devnum, command5_block, &cmd_size);
      /* Read it */
      cmd_size = size_requested;
      x1100_usb_bulk_read (devnum, buffer, &cmd_size);

      for (i = 0; i < no_of_buffers; i++)
	{
	  buffer_start = buffer + (i * 0x9c4);
	  /* Reverse order of bytes in words of buffer */
	  for (j = 0; j < 2500; j = j + 2)
	    {
	      temp_byte = *(buffer_start + j);
	      *(buffer_start + j) = *(buffer_start + j + 1);
	      *(buffer_start + j + 1) = temp_byte;
	    }
	  if (x1100_is_home_line (buffer_start))
	    {
	      home_line_count++;
	      in_home_region = SANE_TRUE;
	    }
	  else if (in_home_region)
	    {
	      free (buffer);
	      x1100_stop_mvmt (devnum);
	      return SANE_TRUE;
	    }
	}
    }				/*   end while (buffer_count > buffer_limit); */
  free (buffer);
  x1100_stop_mvmt (devnum);

  /* Move forward to approximately the middle of the dot */
  sanei_lexmark_x1100_move_fwd (0x4c, dev);

  return SANE_FALSE;

}

SANE_Status
x1100_get_start_loc (SANE_Int resolution, SANE_Int * vert_start, 
		     SANE_Int * hor_start, SANE_Int offset)
{
  SANE_Int start_600;

  /* Calculate vertical start distance at 600dpi */
  start_600 = 195 - offset;


  switch (resolution)
    {
    case 75:
      *vert_start = start_600 / 8;
      *hor_start = 0x68;
      break;
    case 150:
      *vert_start = start_600 / 4;
      *hor_start = 0x68;
      break;
    case 300:
      *vert_start = start_600 / 2;
      *hor_start = 0x6a;
      break;
    case 600:
      *vert_start = start_600;
      *hor_start = 0x6b;
      break;
    case 1200:
      *vert_start = start_600 * 2;
      *hor_start = 0x6b;
      break;
    default:
      /* If we're here we have an invalid resolution */
      return SANE_STATUS_INVAL;
    }

  return SANE_STATUS_GOOD;
}

void
x1100_set_scan_area (SANE_Int res, SANE_Bool isColour, SANE_Int pixel_height,
		     SANE_Int pixel_width, SANE_Int offset)
{

  SANE_Status status;
  SANE_Int vert_start;
  SANE_Int hor_start;
  SANE_Int vert_end;
  SANE_Int hor_end;
  SANE_Int hor_res;
  unsigned long vert_dist;

  status =
    x1100_get_start_loc (res, &vert_start, &hor_start, offset);

  /* convert pixel height to vertical location coordinates */
  vert_dist = pixel_height + 2;
  if ((res >= 600) && (isColour))
    vert_dist = vert_dist * 2;
  vert_end = vert_start + vert_dist;
  /* set vertical start position registers */
  shadow_regs[0x60] = vert_start & 0xFF;
  shadow_regs[0x61] = (vert_start >> 8) & 0xFF;
  /* set vertical end position registers */
  shadow_regs[0x62] = (vert_end & 0xFF) + 2;	/*F.O. TEST! */
  shadow_regs[0x63] = (vert_end >> 8) & 0xFF;


  /* convert pixel width to horizontal location coordinates */
  hor_res = res;
  if (hor_res > 600)
    hor_res = 600;
  hor_end = hor_start + (600 * pixel_width) / hor_res;

  /* set horizontal start position registers */
  shadow_regs[0x66] = hor_start & 0xFF;
  shadow_regs[0x67] = (hor_start >> 8) & 0xFF;
  /* set horizontal end position registers */
  shadow_regs[0x6c] = hor_end & 0xFF;
  shadow_regs[0x6d] = (hor_end >> 8) & 0xFF;

  /* Debug */

  DBG (2, "x1100_set_scan_area: vert_start: %x\n", vert_start);
  DBG (2, "x1100_set_scan_area: vert_end: %x\n", vert_end);
  DBG (2, "x1100_set_scan_area: hor_start: %x\n", hor_start);
  DBG (2, "x1100_set_scan_area: hor_end: %x\n", hor_end);

}

SANE_Int
sanei_lexmark_x1100_find_start_line (SANE_Int devnum)
{
  /*
     This function scans forward 59 lines, reading 88 bytes per line from the
     middle of the horizontal line: pixel 0xa84 to pixel 0x9d4. It scans with
     the following parameters:
       dir=fwd 
       mode=grayscale
       h.res=300 dpi
       v.res=600 dpi   
       hor. pixels = (0xa84 - 0x9d4)/2 = 0x58 = 88
       vert. pixels = 0x3e - 0x03 = 0x3b = 59
       data = 88x59=5192=0x1448

     It assumes we are in the start dot, or just before it. We are reading
     enough lines at 600dpi to read past the dot. We return the number of
     entirely white lines read consecutively, so we know how far past the
     dot we are.

     To find the number of consecutive white lines we do the following:

     Byte swap the order of the bytes in the buffer.

     Go through the buffer finding low and high values, which are computed by 
     comparing to the weighted average: 
     weighted_average = (lowest value + (highest value - lowest value)/4)
     Low bytes are changed to 0xFF (white), higher of equal bytes are changed 
     to 0x00 (black),so that the buffer only contains white (0xFF) or black 
     (0x00) values.

     Next, we go through the buffer a line (88 bytes) at a time for 59 lines
     to read the entire buffer. For each byte in a line we check if the
     byte is black. If it is we increment the black byte counter.

     After each line we check the black byte counter. If it is greater than 0 
     we increment the black line count and set the white line count to 0. If
     there were no black bytes in the line we set the black line count to 0
     and increment the white line count.

     When all lines have been processed we return the white line count.
   */


  int blackLineCount = 0;
  int whiteLineCount = 0;
  int blackByteCounter = 0;
  unsigned char max_byte = 0;
  unsigned char min_byte = 0xFF;
  unsigned char weighted_average;
  int i, j;

  SANE_Byte poll_result[3];
  SANE_Byte *buffer;
  SANE_Byte temp_byte;

  static SANE_Byte command4_block[] = { 0x90, 0x00, 0x00, 0x03 };

  static SANE_Byte command5_block[] = { 0x91, 0x00, 0x14, 0x48 };

  size_t cmd_size;
  SANE_Bool got_line;

  /* Clear all the shadow registers */
  for (i = 0; i < 255; i++)
    shadow_regs[i] = 0;
  /* Some regiters are always set the same */
  shadow_regs[0x01] = 0x43;
  shadow_regs[0x0b] = 0x70;
  shadow_regs[0x11] = 0x01;
  shadow_regs[0x12] = 0x0f;
  shadow_regs[0x13] = 0x01;
  shadow_regs[0x15] = 0x01;
  shadow_regs[0x16] = 0x0f;
  shadow_regs[0x1d] = 0x20;
  shadow_regs[0x28] = 0xeb;
  shadow_regs[0x29] = 0xee;
  shadow_regs[0x2a] = 0xf7;
  shadow_regs[0x2b] = 0x01;
  shadow_regs[0x2c] = 0x0f;
  shadow_regs[0x2d] = 0x51;
  shadow_regs[0x2e] = 0x86;
  shadow_regs[0x30] = 0x48;
  shadow_regs[0x33] = 0x01;
  shadow_regs[0x3a] = 0x20;
  shadow_regs[0x3b] = 0x37;
  shadow_regs[0x3c] = 0x88;
  shadow_regs[0x3d] = 0x08;
  shadow_regs[0x40] = 0x80;
  shadow_regs[0x65] = 0x80;
  shadow_regs[0x72] = 0x05;
  shadow_regs[0x74] = 0x0e;
  shadow_regs[0x8b] = 0xff;
  shadow_regs[0x8c] = 0x02;
  shadow_regs[0x8d] = 0x01;
  shadow_regs[0x8e] = 0x60;
  shadow_regs[0x8f] = 0x80;
  shadow_regs[0x94] = 0x0e;
  shadow_regs[0xa3] = 0xcc;
  shadow_regs[0xa4] = 0x27;
  shadow_regs[0xa5] = 0x24;
  shadow_regs[0xb0] = 0xb2;
  shadow_regs[0xc2] = 0x80;
  shadow_regs[0xc4] = 0x20;
  shadow_regs[0xc8] = 0x04;
  shadow_regs[0xf4] = 0xf8;
  shadow_regs[0xf5] = 0x7f;
  /* set calibration */
  shadow_regs[0x02] = 0x80;
  shadow_regs[0x03] = 0x80;
  shadow_regs[0x04] = 0x80;
  shadow_regs[0x05] = 0x80;
  shadow_regs[0x06] = 0x80;
  shadow_regs[0x07] = 0x80;
  shadow_regs[0x08] = 0x06;
  shadow_regs[0x09] = 0x06;
  shadow_regs[0x0a] = 0x06;
  /* set grayscale  scan  */
  shadow_regs[0x2f] = 0x21;
  /* set ? */
  shadow_regs[0x34] = 0x0d;
  shadow_regs[0x35] = 0x0d;
  shadow_regs[0x36] = 0x1d;
  shadow_regs[0x37] = 0x1d;
  shadow_regs[0x38] = 0x29;
  /* set motor resolution divisor */
  shadow_regs[0x39] = 0x01;
  /* set vertical start/end positions */
  shadow_regs[0x60] = 0x03;
  shadow_regs[0x61] = 0x00;
  shadow_regs[0x62] = 0x3e;
  shadow_regs[0x63] = 0x00;
  /* set # of head moves per CIS read */
  shadow_regs[0x64] = 0x01;
  /* set horizontal start position */
  shadow_regs[0x66] = 0xd4;
  shadow_regs[0x67] = 0x09;
  /* set horizontal end position */
  shadow_regs[0x6c] = 0x84;
  shadow_regs[0x6d] = 0x0a;
  /* set horizontal resolution */
  shadow_regs[0x79] = 0x40;
  shadow_regs[0x7a] = 0x02;
  /* set for ? */
  shadow_regs[0x93] = 0x06;
  /* Motor enable & Coordinate space denominator */
  shadow_regs[0xc3] = 0x81;
  /* set for ? */
  shadow_regs[0xc5] = 0x22;
  /* Movement direction & step size */
  shadow_regs[0xc6] = 0x09;
  /* step size range2 */
  shadow_regs[0xc9] = 0x3b;
  /* set for ? */
  shadow_regs[0xca] = 0x1f;
  shadow_regs[0xe0] = 0xf7;
  shadow_regs[0xe1] = 0x16;
  /* step size range0 */
  shadow_regs[0xe2] = 0x87;
  /* ? */
  shadow_regs[0xe3] = 0x13;
  shadow_regs[0xe4] = 0x1b;
  shadow_regs[0xe5] = 0x16;
  shadow_regs[0xe6] = 0xdc;
  shadow_regs[0xe9] = 0x1b;
  shadow_regs[0xec] = 0x07;
  shadow_regs[0xef] = 0x03;

  /* Stop the scanner */
  x1100_stop_mvmt (devnum);

  /* write regs out twice */
  shadow_regs[0x32] = 0x00;
  x1100_write_all_regs (devnum);
  shadow_regs[0x32] = 0x40;
  x1100_write_all_regs (devnum);

  /* Start Scan */
  /* cmd_size=0x23; */
  /* x1100_usb_bulk_write(devnum, command3_block, &cmd_size);  */
  x1100_start_mvmt (devnum);

  /* Poll the available byte count until not 0 */
  got_line = SANE_FALSE;
  while (!got_line)
    {
      cmd_size = 4;
      x1100_usb_bulk_write (devnum, command4_block, &cmd_size);
      cmd_size = 0x3;
      x1100_usb_bulk_read (devnum, poll_result, &cmd_size);
      if (!
	  (poll_result[0] == 0 && poll_result[1] == 0 && poll_result[2] == 0))
	{
	  /* if result != 00 00 00 we got data */
	  got_line = SANE_TRUE;
	}
    }


  /* create buffer for scan data */
  buffer = calloc (5192, sizeof (char));

  /* Tell the scanner to send the data */
  /* Write: 91 00 14 48 */
  cmd_size = 4;
  x1100_usb_bulk_write (devnum, command5_block, &cmd_size);
  /* Read it */
  cmd_size = 0x1448;
  x1100_usb_bulk_read (devnum, buffer, &cmd_size);

  /* Stop the scanner */
  x1100_stop_mvmt (devnum);


  /* Reverse order of bytes in words of buffer */
  for (i = 0; i < 5192; i = i + 2)
    {
      temp_byte = *(buffer + i);
      *(buffer + i) = *(buffer + i + 1);
      *(buffer + i + 1) = temp_byte;
    }
  /* Find the max and the min */
  for (i = 0; i < 5192; i++)
    {
      if (*(buffer + i) > max_byte)
	max_byte = *(buffer + i);
      if (*(buffer + i) < min_byte)
	min_byte = *(buffer + i);
    }

  weighted_average = min_byte + ((max_byte - min_byte) / 4);

  /* Set bytes as black (0x00) or white (0xFF) */
  for (i = 0; i < 5192; i++)
    {
      if (*(buffer + i) > weighted_average)
	*(buffer + i) = 0xFF;
      else
	*(buffer + i) = 0x00;
    }
  /* Go through 59 lines */
  for (j = 0; j < 59; j++)
    {
      blackByteCounter = 0;
      /* Go through 88 bytes per line */
      for (i = 0; i < 88; i++)
	{
	  /* Is byte black? */
	  if (*(buffer + (j * 88) + i) == 0)
	    {
	      blackByteCounter++;
	    }
	} /* end for line*/
      if (blackByteCounter > 0)
	{
	  /* This was a black line */
	  blackLineCount++;
	  whiteLineCount = 0;
	}
      else
	{
	  /* This is a white line */
	  whiteLineCount++;
	  blackLineCount = 0;
	}
    } /* end for buffer */

  return whiteLineCount;

}


SANE_Status
sanei_lexmark_x1100_set_scan_regs (Lexmark_Device * dev, SANE_Int offset)
{
  SANE_Int yres;
  SANE_Bool isColourScan;
  int i;

  /* resolution */
  yres = dev->val[OPT_Y_DPI].w;

  /* colour mode */
  if (strcmp (dev->val[OPT_MODE].s, "Color") == 0)
    isColourScan = SANE_TRUE;
  else
    isColourScan = SANE_FALSE;

  /* Clear all the shadow registers */
  for (i = 0; i < 255; i++)
    shadow_regs[i] = 0;
  /* Some regiters are always set the same */
  shadow_regs[0x01] = 0x43;
  shadow_regs[0x0b] = 0x70;
  shadow_regs[0x11] = 0x01;
  shadow_regs[0x12] = 0x0f;
  shadow_regs[0x13] = 0x01;
  shadow_regs[0x15] = 0x01;
  shadow_regs[0x16] = 0x0f;
  shadow_regs[0x1d] = 0x20;
  shadow_regs[0x28] = 0xeb;
  shadow_regs[0x29] = 0xee;
  shadow_regs[0x2a] = 0xf7;
  shadow_regs[0x2b] = 0x01;
  shadow_regs[0x2c] = 0x0f;
  shadow_regs[0x2d] = 0x51;
  shadow_regs[0x2e] = 0x86;
  shadow_regs[0x30] = 0x48;
  shadow_regs[0x33] = 0x01;
  shadow_regs[0x3a] = 0x20;
  shadow_regs[0x3b] = 0x37;
  shadow_regs[0x3c] = 0x88;
  shadow_regs[0x3d] = 0x08;
  shadow_regs[0x40] = 0x80;
  shadow_regs[0x65] = 0x80;
  shadow_regs[0x72] = 0x05;
  shadow_regs[0x74] = 0x0e;
  shadow_regs[0x8b] = 0xff;
  shadow_regs[0x8c] = 0x02;
  shadow_regs[0x8d] = 0x01;
  shadow_regs[0x8e] = 0x60;
  shadow_regs[0x8f] = 0x80;
  shadow_regs[0x94] = 0x0e;
  shadow_regs[0xa3] = 0xcc;
  shadow_regs[0xa4] = 0x27;
  shadow_regs[0xa5] = 0x24;
  shadow_regs[0xb0] = 0xb2;
  shadow_regs[0xc2] = 0x80;
  shadow_regs[0xc4] = 0x20;
  shadow_regs[0xc8] = 0x04;
  shadow_regs[0xf4] = 0xf8;
  shadow_regs[0xf5] = 0x7f;


  /* size */
  x1100_set_scan_area (yres, isColourScan, dev->pixel_height,
		       dev->pixel_width, offset);

  /*75dpi x 75dpi */
  if (yres == 75)
    {
      DBG (5, "sanei_lexmark_x1100_set_scan_regs(): 75 DPI resolution\n");
      /* set calibration */
      shadow_regs[0x02] = 0xa1;
      shadow_regs[0x03] = 0x9d;
      shadow_regs[0x04] = 0xa3;
      shadow_regs[0x05] = 0xa1;
      shadow_regs[0x06] = 0x9d;
      shadow_regs[0x07] = 0xa3;
      shadow_regs[0x08] = 0x0a;
      shadow_regs[0x09] = 0x0b;
      shadow_regs[0x0a] = 0x06;

      if (isColourScan)
	{
	  /* set colour scan */
	  shadow_regs[0x2f] = 0x11;
	  /* set ? */
	  shadow_regs[0x34] = 0x05;
	  shadow_regs[0x35] = 0x01;
	  shadow_regs[0x36] = 0x05;
	  shadow_regs[0x37] = 0x01;
	  shadow_regs[0x38] = 0x05;
	  /* set motor resolution divisor */
	  shadow_regs[0x39] = 0x0f;
	  /* set vertical start/end positions */
/* 	  shadow_regs[0x60]=0x22; */
/* 	  shadow_regs[0x61]=0x00; */
/* 	  shadow_regs[0x62]=0x1a; */
/* 	  shadow_regs[0x63]=0x01; */
	  /* set ? */
	  shadow_regs[0x80] = 0x0c;
	  shadow_regs[0x81] = 0x0c;
	  shadow_regs[0x82] = 0x09;
	  shadow_regs[0x85] = 0x00;
	  shadow_regs[0x86] = 0x00;
	  shadow_regs[0x87] = 0x00;
	  shadow_regs[0x88] = 0x00;
	  shadow_regs[0x91] = 0x8c;
	  shadow_regs[0x92] = 0x40;
	  shadow_regs[0x93] = 0x06;
	  /* Motor enable & Coordinate space denominator */
	  shadow_regs[0xc3] = 0x83;
	  /*  ? */
	  shadow_regs[0xc5] = 0x0a;
	  /* Movement direction & step size */
	  shadow_regs[0xc6] = 0x09;
	  /*  ? */
	  shadow_regs[0xc9] = 0x3b;
	  shadow_regs[0xca] = 0x01;
	  /* bounds of movement range0 */
	  shadow_regs[0xe0] = 0x2b;
	  shadow_regs[0xe1] = 0x0a;
	  /* step size range0 */
	  shadow_regs[0xe2] = 0x7f;
	  /* ? */
	  shadow_regs[0xe3] = 0x01;
	  /* bounds of movement range1 */
	  shadow_regs[0xe4] = 0xbb;
	  shadow_regs[0xe5] = 0x09;
	  /* step size range1 */
	  shadow_regs[0xe6] = 0x0e;
	  /* bounds of movement range2 */
	  shadow_regs[0xe7] = 0x2b;
	  shadow_regs[0xe8] = 0x03;
	  /* step size range2 */
	  shadow_regs[0xe9] = 0x05;
	  /* bounds of movement range3 */
	  shadow_regs[0xea] = 0xa0;
	  shadow_regs[0xeb] = 0x01;
	  /* step size range3 */
	  shadow_regs[0xec] = 0x01;
	  /* step size range4 */
	  shadow_regs[0xef] = 0x01;

	}
      else
	{
	  /* set grayscale  scan  */
	  shadow_regs[0x2f] = 0x21;
	  /* set ? */
	  shadow_regs[0x34] = 0x02;
	  shadow_regs[0x35] = 0x02;
	  shadow_regs[0x36] = 0x04;
	  shadow_regs[0x37] = 0x04;
	  shadow_regs[0x38] = 0x06;
	  /* set motor resolution divisor */
	  shadow_regs[0x39] = 0x0f;
	  /* set vertical start/end positions */
/* 	  shadow_regs[0x60]=0x14; */
/* 	  shadow_regs[0x61]=0x00; */
/* 	  shadow_regs[0x62]=0x8d; */
/* 	  shadow_regs[0x63]=0x01; */
	  /* set ? only for colour? */
	  shadow_regs[0x80] = 0x00;
	  shadow_regs[0x81] = 0x00;
	  shadow_regs[0x82] = 0x00;
	  shadow_regs[0x85] = 0x20;
	  shadow_regs[0x86] = 0x00;
	  shadow_regs[0x87] = 0x00;
	  shadow_regs[0x88] = 0x00;
	  shadow_regs[0x91] = 0x00;
	  shadow_regs[0x92] = 0x00;
	  shadow_regs[0x93] = 0x06;
	  /* Motor enable & Coordinate space denominator */
	  shadow_regs[0xc3] = 0x81;
	  /*  ? */
	  shadow_regs[0xc5] = 0x10;
	  /* Movement direction & step size */
	  shadow_regs[0xc6] = 0x09;
	  /*  ? */
	  shadow_regs[0xc9] = 0x3b;
	  shadow_regs[0xca] = 0x01;
	  /* bounds of movement range0 */
	  shadow_regs[0xe0] = 0x4d;
	  shadow_regs[0xe1] = 0x1c;
	  /* step size range0 */
	  shadow_regs[0xe2] = 0x71;
	  /* ? */
	  shadow_regs[0xe3] = 0x02;
	  /* bounds of movement range1 */
	  shadow_regs[0xe4] = 0x6d;
	  shadow_regs[0xe5] = 0x15;
	  /* step size range1 */
	  shadow_regs[0xe6] = 0xdc;
	  /* bounds of movement range2 */
	  shadow_regs[0xe7] = 0xad;
	  shadow_regs[0xe8] = 0x07;
	  /* step size range2 */
	  shadow_regs[0xe9] = 0x1b;
	  /* bounds of movement range3 */
	  shadow_regs[0xea] = 0xe1;
	  shadow_regs[0xeb] = 0x03;
	  /* step size range3 */
	  shadow_regs[0xec] = 0x07;
	  /* bounds of movement range4 -only for 75dpi grayscale */
	  shadow_regs[0xed] = 0xc2;
	  shadow_regs[0xee] = 0x02;
	  /* step size range4 */
	  shadow_regs[0xef] = 0x03;
	}

      /* set # of head moves per CIS read */
      shadow_regs[0x64] = 0x01;
      /* set horizontal start position */
/*       shadow_regs[0x66]=0x68; */
/*       shadow_regs[0x67]=0x00; */
      /* set horizontal end position */
/*       shadow_regs[0x6c]=0x78; */
/*       shadow_regs[0x6d]=0x07; */
      /* set horizontal resolution */
      shadow_regs[0x79] = 0x08;
      shadow_regs[0x7a] = 0x08;

    }

  /*150dpi x 150dpi */
  if (yres == 150)
    {
      DBG (5, "sanei_lexmark_x1100_set_scan_regs(): 150 DPI resolution\n");
      /* set calibration */
      shadow_regs[0x02] = 0xa1;
      shadow_regs[0x03] = 0xa1;
      shadow_regs[0x04] = 0xa3;
      shadow_regs[0x05] = 0xa1;
      shadow_regs[0x06] = 0xa1;
      shadow_regs[0x07] = 0xa3;
      shadow_regs[0x08] = 0x0a;
      shadow_regs[0x09] = 0x0a;
      shadow_regs[0x0a] = 0x06;


      if (isColourScan)
	{
	  /* set colour scan */
	  shadow_regs[0x2f] = 0x11;
	  /* set ? */
	  shadow_regs[0x34] = 0x0b;
	  shadow_regs[0x35] = 0x01;
	  shadow_regs[0x36] = 0x0b;
	  shadow_regs[0x37] = 0x01;
	  shadow_regs[0x38] = 0x0a;
	  /* set motor resolution divisor */
	  shadow_regs[0x39] = 0x07;
	  /* set vertical start/end positions */
/* 	  shadow_regs[0x60]=0x22; */
/* 	  shadow_regs[0x61]=0x00;  */
/* 	  shadow_regs[0x62]=0x12; */
/* 	  shadow_regs[0x63]=0x03; */
	  /* set ? */
	  shadow_regs[0x80] = 0x05;
	  shadow_regs[0x81] = 0x05;
	  shadow_regs[0x82] = 0x0a;
	  shadow_regs[0x85] = 0x83;
	  shadow_regs[0x86] = 0x7e;
	  shadow_regs[0x87] = 0xad;
	  shadow_regs[0x88] = 0x35;
	  shadow_regs[0x91] = 0xfe;
	  shadow_regs[0x92] = 0xdf;
	  shadow_regs[0x93] = 0x0e;
	  /* Motor enable & Coordinate space denominator */
	  shadow_regs[0xc3] = 0x83;
	  /*  ? */
	  shadow_regs[0xc5] = 0x0e;
	  /* Movement direction & step size */
	  shadow_regs[0xc6] = 0x09;
	  /*  ? */
	  shadow_regs[0xc9] = 0x3a;
	  shadow_regs[0xca] = 0x03;
	  /* bounds of movement range0 */
	  shadow_regs[0xe0] = 0x61;
	  shadow_regs[0xe1] = 0x0a;
	  /* step size range0 */
	  shadow_regs[0xe2] = 0xed;
	  /* ? */
	  shadow_regs[0xe3] = 0x02;
	  /* bounds of movement range1 */
	  shadow_regs[0xe4] = 0x29;
	  shadow_regs[0xe5] = 0x0a;
	  /* step size range1 */
	  shadow_regs[0xe6] = 0x0e;
	  /* bounds of movement range2 */
	  shadow_regs[0xe7] = 0x29;
	  shadow_regs[0xe8] = 0x03;
	  /* step size range2 */
	  shadow_regs[0xe9] = 0x05;
	  /* bounds of movement range3 */
	  shadow_regs[0xea] = 0x00;
	  shadow_regs[0xeb] = 0x00;
	  /* step size range3 */
	  shadow_regs[0xec] = 0x01;
	  /* step size range4 */
	  shadow_regs[0xef] = 0x01;

	}
      else
	{
	  /* set grayscale  scan */
	  shadow_regs[0x2f] = 0x21;
	  /* set ? */
	  shadow_regs[0x34] = 0x04;
	  shadow_regs[0x35] = 0x04;
	  shadow_regs[0x36] = 0x07;
	  shadow_regs[0x37] = 0x07;
	  shadow_regs[0x38] = 0x0a;
	  /* set motor resolution divisor */
	  shadow_regs[0x39] = 0x07;
	  /* set vertical start/end positions */
/* 	  shadow_regs[0x60]=0x20; */
/* 	  shadow_regs[0x61]=0x00; */
/* 	  shadow_regs[0x62]=0x10; */
/* 	  shadow_regs[0x63]=0x03; */
	  /* set ? only for colour? */
	  shadow_regs[0x80] = 0x00;
	  shadow_regs[0x81] = 0x00;
	  shadow_regs[0x82] = 0x00;
	  shadow_regs[0x85] = 0x00;
	  shadow_regs[0x86] = 0x00;
	  shadow_regs[0x87] = 0x00;
	  shadow_regs[0x88] = 0x00;
	  shadow_regs[0x91] = 0x00;
	  shadow_regs[0x92] = 0x00;
	  shadow_regs[0x93] = 0x06;
	  /* Motor enable & Coordinate space denominator */
	  shadow_regs[0xc3] = 0x81;
	  /*  ? */
	  shadow_regs[0xc5] = 0x16;
	  /* Movement direction & step size */
	  shadow_regs[0xc6] = 0x09;
	  /*  ? */
	  shadow_regs[0xc9] = 0x3b;
	  shadow_regs[0xca] = 0x01;
	  /* bounds of movement range0 */
	  shadow_regs[0xe0] = 0xdd;
	  shadow_regs[0xe1] = 0x18;
	  /* step size range0 */
	  shadow_regs[0xe2] = 0x01;
	  /* ? */
	  shadow_regs[0xe3] = 0x03;
	  /* bounds of movement range1 */
	  shadow_regs[0xe4] = 0x6d;
	  shadow_regs[0xe5] = 0x15;
	  /* step size range1 */
	  shadow_regs[0xe6] = 0xdc;
	  /* bounds of movement range2 */
	  shadow_regs[0xe7] = 0xad;
	  shadow_regs[0xe8] = 0x07;
	  /* step size range2 */
	  shadow_regs[0xe9] = 0x1b;
	  /* bounds of movement range3 */
	  shadow_regs[0xea] = 0xe1;
	  shadow_regs[0xeb] = 0x03;
	  /* step size range3 */
	  shadow_regs[0xec] = 0x07;
	  /* step size range4 */
	  shadow_regs[0xef] = 0x03;
	}

      /* set # of head moves per CIS read */
      shadow_regs[0x64] = 0x01;
      /* set horizontal start position */
/*       shadow_regs[0x66]=0x68; */
/*       shadow_regs[0x67]=0x00; */
      /* set horizontal end position */
/*       shadow_regs[0x6c]=0x70; */
/*       shadow_regs[0x6d]=0x07; */
      /* set horizontal resolution */
      shadow_regs[0x79] = 0x08;
      shadow_regs[0x7a] = 0x04;

    }

  /*300dpi x 300dpi */
  if (yres == 300)
    {
      DBG (5, "sanei_lexmark_x1100_set_scan_regs(): 300 DPI resolution\n");
      /* set calibration */
      shadow_regs[0x02] = 0xa1;
      shadow_regs[0x03] = 0xa1;
      shadow_regs[0x04] = 0xa3;
      shadow_regs[0x05] = 0xa1;
      shadow_regs[0x06] = 0xa1;
      shadow_regs[0x07] = 0xa3;
      shadow_regs[0x08] = 0x0a;
      shadow_regs[0x09] = 0x0a;
      shadow_regs[0x0a] = 0x06;

      if (isColourScan)
	{
	  /* set colour scan */
	  shadow_regs[0x2f] = 0x11;
	  /* set ? */
	  shadow_regs[0x34] = 0x15;
	  shadow_regs[0x35] = 0x01;
	  shadow_regs[0x36] = 0x15;
	  shadow_regs[0x37] = 0x01;
	  shadow_regs[0x38] = 0x14;
	  /* set motor resolution divisor */
	  shadow_regs[0x39] = 0x03;
	  /* set vertical start/end positions */
/* 	  shadow_regs[0x60]=0x44; */
/* 	  shadow_regs[0x61]=0x00;  */
/* 	  shadow_regs[0x62]=0x22; */
/* 	  shadow_regs[0x63]=0x06; */
	  /* set ? */
	  shadow_regs[0x80] = 0x0a;
	  shadow_regs[0x81] = 0x0a;
	  shadow_regs[0x82] = 0x06;
	  shadow_regs[0x85] = 0x83;
	  shadow_regs[0x86] = 0x7e;
	  shadow_regs[0x87] = 0xad;
	  shadow_regs[0x88] = 0x35;
	  shadow_regs[0x91] = 0xfe;
	  shadow_regs[0x92] = 0xdf;
	  shadow_regs[0x93] = 0x0e;
	  /* Motor enable & Coordinate space denominator */
	  shadow_regs[0xc3] = 0x83;
	  /*  ? */
	  shadow_regs[0xc5] = 0x17;
	  /* Movement direction & step size */
	  shadow_regs[0xc6] = 0x09;
	  /*  ? */
	  shadow_regs[0xc9] = 0x3a;
	  shadow_regs[0xca] = 0x0a;
	  /* bounds of movement range0 */
	  shadow_regs[0xe0] = 0x75;
	  shadow_regs[0xe1] = 0x0a;
	  /* step size range0 */
	  shadow_regs[0xe2] = 0xdd;
	  /* ? */
	  shadow_regs[0xe3] = 0x05;
	  /* bounds of movement range1 */
	  shadow_regs[0xe4] = 0x59;
	  shadow_regs[0xe5] = 0x0a;
	  /* step size range1 */
	  shadow_regs[0xe6] = 0x0e;
	  /* bounds of movement range2 */
	  shadow_regs[0xe7] = 0x00;
	  shadow_regs[0xe8] = 0x00;
	  /* step size range2 */
	  shadow_regs[0xe9] = 0x05;
	  /* bounds of movement range3 */
	  shadow_regs[0xea] = 0x00;
	  shadow_regs[0xeb] = 0x00;
	  /* step size range3 */
	  shadow_regs[0xec] = 0x01;
	  /* step size range4 */
	  shadow_regs[0xef] = 0x01;
	}
      else
	{
	  /* set grayscale  scan */
	  shadow_regs[0x2f] = 0x21;
	  /* set ? */
	  shadow_regs[0x34] = 0x08;
	  shadow_regs[0x35] = 0x08;
	  shadow_regs[0x36] = 0x0f;
	  shadow_regs[0x37] = 0x0f;
	  shadow_regs[0x38] = 0x16;
	  /* set motor resolution divisor */
	  shadow_regs[0x39] = 0x03;
	  /* set vertical start/end positions */
/* 	  shadow_regs[0x60]=0x40; */
/* 	  shadow_regs[0x61]=0x00; */
/* 	  shadow_regs[0x62]=0xfe; */
/* 	  shadow_regs[0x63]=0x06; */
	  /* set ? only for colour? */
	  shadow_regs[0x80] = 0x00;
	  shadow_regs[0x81] = 0x00;
	  shadow_regs[0x82] = 0x00;
	  shadow_regs[0x85] = 0x00;
	  shadow_regs[0x86] = 0x00;
	  shadow_regs[0x87] = 0x00;
	  shadow_regs[0x88] = 0x00;
	  shadow_regs[0x91] = 0x00;
	  shadow_regs[0x92] = 0x00;
	  shadow_regs[0x93] = 0x06;
	  /* Motor enable & Coordinate space denominator */
	  shadow_regs[0xc3] = 0x81;
	  /*  ? */
	  shadow_regs[0xc5] = 0x19;
	  /* Movement direction & step size */
	  shadow_regs[0xc6] = 0x09;
	  /*  ? */
	  shadow_regs[0xc9] = 0x3a;
	  shadow_regs[0xca] = 0x08;
	  /* bounds of movement range0 */
	  shadow_regs[0xe0] = 0xe3;
	  shadow_regs[0xe1] = 0x18;
	  /* step size range0 */
	  shadow_regs[0xe2] = 0x03;
	  /* ? */
	  shadow_regs[0xe3] = 0x06;
	  /* bounds of movement range1 */
	  shadow_regs[0xe4] = 0x2b;
	  shadow_regs[0xe5] = 0x17;
	  /* step size range1 */
	  shadow_regs[0xe6] = 0xdc;
	  /* bounds of movement range2 */
	  shadow_regs[0xe7] = 0xb3;
	  shadow_regs[0xe8] = 0x07;
	  /* step size range2 */
	  shadow_regs[0xe9] = 0x1b;
	  /* bounds of movement range3 */
	  shadow_regs[0xea] = 0x00;
	  shadow_regs[0xeb] = 0x00;
	  /* step size range3 */
	  shadow_regs[0xec] = 0x07;
	  /* step size range4 */
	  shadow_regs[0xef] = 0x03;
	}

      /* set # of head moves per CIS read */
      shadow_regs[0x64] = 0x01;
      /* set horizontal start position */
      /*      shadow_regs[0x66]=0x6a; */
/*       shadow_regs[0x67]=0x00; */
      /* set horizontal end position */
/*       shadow_regs[0x6c]=0x72; */
/*       shadow_regs[0x6d]=0x07; */
      /* set horizontal resolution */
      shadow_regs[0x79] = 0x20;
      shadow_regs[0x7a] = 0x02;

    }

  /*600dpi x 600dpi */
  if (yres == 600)
    {
      DBG (5, "sanei_lexmark_x1100_set_scan_regs(): 600 DPI resolution\n");
      /* set calibration */
      shadow_regs[0x02] = 0xa1;
      shadow_regs[0x03] = 0xa1;
      shadow_regs[0x04] = 0xa3;
      shadow_regs[0x05] = 0xa1;
      shadow_regs[0x06] = 0xa1;
      shadow_regs[0x07] = 0xa3;
      shadow_regs[0x08] = 0x0a;
      shadow_regs[0x09] = 0x0a;
      shadow_regs[0x0a] = 0x06;

      if (isColourScan)
	{
	  /* set colour scan */
	  shadow_regs[0x2f] = 0x11;
	  /* set ? */
	  shadow_regs[0x34] = 0x15;
	  shadow_regs[0x35] = 0x01;
	  shadow_regs[0x36] = 0x15;
	  shadow_regs[0x37] = 0x01;
	  shadow_regs[0x38] = 0x14;
	  /* set motor resolution divisor */
	  shadow_regs[0x39] = 0x03;
	  /* set vertical start/end positions */
/* 	  shadow_regs[0x60]=0x14; */
/* 	  shadow_regs[0x61]=0x00;  */
/* 	  shadow_regs[0x62]=0x88; */
/* 	  shadow_regs[0x63]=0x17; */
	  /* set # of head moves per CIS read */
	  shadow_regs[0x64] = 0x02;
	  /* set ? */
	  shadow_regs[0x80] = 0x02;
	  shadow_regs[0x81] = 0x02;
	  shadow_regs[0x82] = 0x08;
	  shadow_regs[0x85] = 0x83;
	  shadow_regs[0x86] = 0x7e;
	  shadow_regs[0x87] = 0xad;
	  shadow_regs[0x88] = 0x35;
	  shadow_regs[0x91] = 0xfe;
	  shadow_regs[0x92] = 0xdf;
	  shadow_regs[0x93] = 0x0e;
	  /* Motor enable & Coordinate space denominator */
	  shadow_regs[0xc3] = 0x86;
	  /*  ? */
	  shadow_regs[0xc5] = 0x27;
	  /* Movement direction & step size */
	  shadow_regs[0xc6] = 0x0c;
	  /*  ? */
	  shadow_regs[0xc9] = 0x3a;
	  shadow_regs[0xca] = 0x1a;
	  /* bounds of movement range0 */
	  shadow_regs[0xe0] = 0x57;
	  shadow_regs[0xe1] = 0x0a;
	  /* step size range0 */
	  shadow_regs[0xe2] = 0xbf;
	  /* ? */
	  shadow_regs[0xe3] = 0x05;
	  /* bounds of movement range1 */
	  shadow_regs[0xe4] = 0x3b;
	  shadow_regs[0xe5] = 0x0a;
	  /* step size range1 */
	  shadow_regs[0xe6] = 0x0e;
	  /* bounds of movement range2 */
	  shadow_regs[0xe7] = 0x00;
	  shadow_regs[0xe8] = 0x00;
	  /* step size range2 */
	  shadow_regs[0xe9] = 0x05;
	  /* bounds of movement range3 */
	  shadow_regs[0xea] = 0x00;
	  shadow_regs[0xeb] = 0x00;
	  /* step size range3 */
	  shadow_regs[0xec] = 0x01;
	  /* step size range4 */
	  shadow_regs[0xef] = 0x01;
	}
      else
	{
	  /* set grayscale  scan */
	  shadow_regs[0x2f] = 0x21;
	  /* set ? */
	  shadow_regs[0x34] = 0x11;
	  shadow_regs[0x35] = 0x11;
	  shadow_regs[0x36] = 0x21;
	  shadow_regs[0x37] = 0x21;
	  shadow_regs[0x38] = 0x31;
	  /* set motor resolution divisor */
	  shadow_regs[0x39] = 0x01;
	  /* set # of head moves per CIS read */
	  shadow_regs[0x64] = 0x01;

	  /* set ? only for colour? */
	  shadow_regs[0x80] = 0x00;
	  shadow_regs[0x81] = 0x00;
	  shadow_regs[0x82] = 0x00;
	  shadow_regs[0x85] = 0x00;
	  shadow_regs[0x86] = 0x00;
	  shadow_regs[0x87] = 0x00;
	  shadow_regs[0x88] = 0x00;
	  shadow_regs[0x91] = 0x00;
	  shadow_regs[0x92] = 0x00;
	  shadow_regs[0x93] = 0x06;
	  /* Motor enable & Coordinate space denominator */
	  shadow_regs[0xc3] = 0x81;
	  /*  ? */
	  shadow_regs[0xc5] = 0x22;
	  /* Movement direction & step size */
	  shadow_regs[0xc6] = 0x09;
	  /*  ? */
	  shadow_regs[0xc9] = 0x3b;
	  shadow_regs[0xca] = 0x1f;
	  /* bounds of movement range0 */
	  shadow_regs[0xe0] = 0xf7;
	  shadow_regs[0xe1] = 0x16;
	  /* step size range0 */
	  shadow_regs[0xe2] = 0x87;
	  /* ? */
	  shadow_regs[0xe3] = 0x13;
	  /* bounds of movement range1 */
	  shadow_regs[0xe4] = 0x1b;
	  shadow_regs[0xe5] = 0x16;
	  /* step size range1 */
	  shadow_regs[0xe6] = 0xdc;
	  /* bounds of movement range2 */
	  shadow_regs[0xe7] = 0x00;
	  shadow_regs[0xe8] = 0x00;
	  /* step size range2 */
	  shadow_regs[0xe9] = 0x1b;
	  /* bounds of movement range3 */
	  shadow_regs[0xea] = 0x00;
	  shadow_regs[0xeb] = 0x00;
	  /* step size range3 */
	  shadow_regs[0xec] = 0x07;
	  /* step size range4 */
	  shadow_regs[0xef] = 0x03;
	}

      /* set horizontal resolution */
      shadow_regs[0x79] = 0x40;
      shadow_regs[0x7a] = 0x01;

    }
  /*600dpi x 1200dpi */
  if (yres == 1200)
    {
      DBG (5, "sanei_lexmark_x1100_set_scan_regs(): 1200 DPI resolution\n");
      /* set calibration */
      shadow_regs[0x02] = 0xa1;
      shadow_regs[0x03] = 0xa1;
      shadow_regs[0x04] = 0xa3;
      shadow_regs[0x05] = 0xa1;
      shadow_regs[0x06] = 0xa1;
      shadow_regs[0x07] = 0xa3;
      shadow_regs[0x08] = 0x0a;
      shadow_regs[0x09] = 0x0a;
      shadow_regs[0x0a] = 0x06;

      if (isColourScan)
	{
	  /* set colour scan */
	  shadow_regs[0x2f] = 0x11;
	  /* set ? */
	  shadow_regs[0x34] = 0x29;
	  shadow_regs[0x35] = 0x01;
	  shadow_regs[0x36] = 0x29;
	  shadow_regs[0x37] = 0x01;
	  shadow_regs[0x38] = 0x28;
	  /* set motor resolution divisor */
	  shadow_regs[0x39] = 0x01;
	  /* set # of head moves per CIS read */
	  shadow_regs[0x64] = 0x02;
	  /* set ? */
	  shadow_regs[0x80] = 0x04;
	  shadow_regs[0x81] = 0x04;
	  shadow_regs[0x82] = 0x08;
	  shadow_regs[0x85] = 0x83;
	  shadow_regs[0x86] = 0x7e;
	  shadow_regs[0x87] = 0xad;
	  shadow_regs[0x88] = 0x35;
	  shadow_regs[0x91] = 0xfe;
	  shadow_regs[0x92] = 0xdf;
	  shadow_regs[0x93] = 0x0e;
	  /* Motor enable & Coordinate space denominator */
	  shadow_regs[0xc3] = 0x86;
	  /*  ? */
	  shadow_regs[0xc5] = 0x41;
	  /* Movement direction & step size */
	  shadow_regs[0xc6] = 0x0c;
	  /*  ? */
	  shadow_regs[0xc9] = 0x3a;
	  shadow_regs[0xca] = 0x40;
	  /* bounds of movement range0 */
	  shadow_regs[0xe0] = 0x00;
	  shadow_regs[0xe1] = 0x00;
	  /* step size range0 */
	  shadow_regs[0xe2] = 0x85;
	  /* ? */
	  shadow_regs[0xe3] = 0x0b;
	  /* bounds of movement range1 */
	  shadow_regs[0xe4] = 0x00;
	  shadow_regs[0xe5] = 0x00;
	  /* step size range1 */
	  shadow_regs[0xe6] = 0x0e;
	  /* bounds of movement range2 */
	  shadow_regs[0xe7] = 0x00;
	  shadow_regs[0xe8] = 0x00;
	  /* step size range2 */
	  shadow_regs[0xe9] = 0x05;
	  /* bounds of movement range3 */
	  shadow_regs[0xea] = 0x00;
	  shadow_regs[0xeb] = 0x00;
	  /* step size range3 */
	  shadow_regs[0xec] = 0x01;
	  /* step size range4 */
	  shadow_regs[0xef] = 0x01;
	}
      else
	{
	  /* set grayscale  scan */
	  shadow_regs[0x2f] = 0x21;
	  /* set ? */
	  shadow_regs[0x34] = 0x22;
	  shadow_regs[0x35] = 0x22;
	  shadow_regs[0x36] = 0x42;
	  shadow_regs[0x37] = 0x42;
	  shadow_regs[0x38] = 0x62;
	  /* set motor resolution divisor */
	  shadow_regs[0x39] = 0x01;
	  /* set # of head moves per CIS read */
	  shadow_regs[0x64] = 0x00;

	  /* set ? only for colour? */
	  shadow_regs[0x80] = 0x00;
	  shadow_regs[0x81] = 0x00;
	  shadow_regs[0x82] = 0x00;
	  shadow_regs[0x85] = 0x00;
	  shadow_regs[0x86] = 0x00;
	  shadow_regs[0x87] = 0x00;
	  shadow_regs[0x88] = 0x00;
	  shadow_regs[0x91] = 0x00;
	  shadow_regs[0x92] = 0x00;
	  shadow_regs[0x93] = 0x06;
	  /* Motor enable & Coordinate space denominator */
	  shadow_regs[0xc3] = 0x81;
	  /*  ? */
	  shadow_regs[0xc5] = 0x41;
	  /* Movement direction & step size */
	  shadow_regs[0xc6] = 0x09;
	  /*  ? */
	  shadow_regs[0xc9] = 0x3a;
	  shadow_regs[0xca] = 0x40;
	  /* bounds of movement range0 */
	  shadow_regs[0xe0] = 0x00;
	  shadow_regs[0xe1] = 0x00;
	  /* step size range0 */
	  shadow_regs[0xe2] = 0xc7;
	  /* ? */
	  shadow_regs[0xe3] = 0x29;
	  /* bounds of movement range1 */
	  shadow_regs[0xe4] = 0x00;
	  shadow_regs[0xe5] = 0x00;
	  /* step size range1 */
	  shadow_regs[0xe6] = 0xdc;
	  /* bounds of movement range2 */
	  shadow_regs[0xe7] = 0x00;
	  shadow_regs[0xe8] = 0x00;
	  /* step size range2 */
	  shadow_regs[0xe9] = 0x1b;
	  /* bounds of movement range3 */
	  shadow_regs[0xea] = 0x00;
	  shadow_regs[0xeb] = 0x00;
	  /* step size range3 */
	  shadow_regs[0xec] = 0x07;
	  /* step size range4 */
	  shadow_regs[0xef] = 0x03;
	}

      /* set horizontal resolution */
      shadow_regs[0x79] = 0x40;
      shadow_regs[0x7a] = 0x01;

    }
  return SANE_STATUS_GOOD;
}

SANE_Status
sanei_lexmark_x1100_start_scan (Lexmark_Device * dev)
{
  SANE_Int devnum;

  static SANE_Byte command4_block[] = { 0x90, 0x00, 0x00, 0x03 };

  static SANE_Byte command5_block[] = { 0x80, 0xb3, 0x00, 0x01 };

  SANE_Byte poll_result[3];
  SANE_Byte read_result;
  SANE_Bool scan_head_moving;
  size_t size;

  devnum = dev->devnum;

  dev->transfer_buffer = NULL;	/* No data xferred yet */
  DBG (2, "sanei_lexmark_x1100_start_scan:\n");


  /* 80 b3 00 01  - poll for scanner not moving */
  scan_head_moving = SANE_TRUE;
  while (scan_head_moving)
    {
      size = 4;
      x1100_usb_bulk_write (devnum, command5_block, &size);
      size = 0x1;
      x1100_usb_bulk_read (devnum, &read_result, &size);
      if ((read_result & 0xF) == 0x0)
	{
	  scan_head_moving = SANE_FALSE;
	}
      /* F.O. Should be a timeout here so we don't hang if something breaks */
    }

  /* Clear C6 */
  x1100_clr_c6 (devnum);
  /* Stop the scanner */
  x1100_stop_mvmt (devnum);

  /*Set regs x2 */
  shadow_regs[0x32] = 0x00;
  x1100_write_all_regs (devnum);
  shadow_regs[0x32] = 0x40;
  x1100_write_all_regs (devnum);

  /* Start Scan */
  x1100_start_mvmt (devnum);

  /* We start with 0 bytes remaining to be read */
  dev->bytes_remaining = 0;
  /* and 0 bytes in the transfer buffer */
  dev->bytes_in_buffer = 0;

  /* Poll the available byte count until not 0 */
  while (1)
    {
      size = 4;
      x1100_usb_bulk_write (devnum, command4_block, &size);
      size = 0x3;
      x1100_usb_bulk_read (devnum, poll_result, &size);
      if (!
	  (poll_result[0] == 0 && poll_result[1] == 0 && poll_result[2] == 0))
	{
	  /* if result != 00 00 00 we got data */

	  /* data_size should be used to set bytes_remaining */
	  /* data_size is set from sane_get_parameters () */
	  dev->bytes_remaining = dev->data_size;
	  /* Initialize the read buffer */
	  read_buffer_init (dev, dev->params.bytes_per_line);
	  return SANE_STATUS_GOOD;

	}
      size = 4;
      /* I'm not sure why the Windows driver does this - probably a timeout? */
      x1100_usb_bulk_write (devnum, command5_block, &size);
      size = 0x1;
      x1100_usb_bulk_read (devnum, &read_result, &size);
      if (read_result != 0x68)
	{
	  dev->bytes_remaining = 0;
	  return SANE_STATUS_IO_ERROR;
	}
    }

  return SANE_STATUS_GOOD;
}

long
sanei_lexmark_x1100_read_scan_data (SANE_Byte * data, SANE_Int size, Lexmark_Device * dev)
{

  SANE_Int devnum;
  SANE_Bool isColourScan;
/*   SANE_Byte temp_byte; */
  static SANE_Byte command1_block[] = { 0x91, 0x00, 0xff, 0xc0 };
  size_t xfer_size, cmd_size, xfer_request, remainder;
/*   unsigned int i; */
  long bytes_read;
  SANE_Bool even_byte;

  DBG (2, "sanei_lexmark_x1100_read_scan_data:\n");

  /* colour mode */
  if (strcmp (dev->val[OPT_MODE].s, "Color") == 0)
    isColourScan = SANE_TRUE;
  else
    isColourScan = SANE_FALSE;

  devnum = dev->devnum;

  /* Check if we have a transfer buffer. Create one and fill it if we don't */
  if (dev->transfer_buffer == NULL)
    {
      if (dev->bytes_remaining > 0)
	{
	  if (dev->bytes_remaining >= MAX_XFER_SIZE)
	    {
	      xfer_request = MAX_XFER_SIZE;
	      xfer_size = MAX_XFER_SIZE;
	      remainder = 0;
	    }
	  else
	    {
	      xfer_request = dev->bytes_remaining;
	      remainder = dev->bytes_remaining % 0x10;
	      xfer_size = dev->bytes_remaining - remainder;
	    }
	  
	  command1_block[2] = (SANE_Byte) (xfer_request >> 8);
	  command1_block[3] = (SANE_Byte) (xfer_request & 0xFF);
	
	  /* Create buffer to hold the amount we will request */
	  dev->transfer_buffer = (SANE_Byte *) malloc (xfer_request);
	  if (dev->transfer_buffer == NULL)
	    return SANE_STATUS_NO_MEM;
	  /* Fill it */
	  /* Write: 91 00 (xfer_size) */
	  cmd_size = 4;
	  x1100_usb_bulk_write (devnum, command1_block, &cmd_size);
	  
	  /* Read: xfer_size  bytes */
	  cmd_size = xfer_request;
	  x1100_usb_bulk_read (devnum, dev->transfer_buffer, &cmd_size);


	  dev->bytes_remaining -= xfer_request;
	  dev->bytes_in_buffer = xfer_request;
	  dev->read_pointer = dev->transfer_buffer;
	  DBG (2, "sanei_lexmark_x1100_read_scan_data:\n");
	  DBG (2, "   Filled a buffer from the scanner\n");
	  DBG (2, "   bytes_remaining: %lu\n", (u_long)dev->bytes_remaining);
	  DBG (2, "   bytes_in_buffer: %lu\n", (u_long)dev->bytes_in_buffer);
	  DBG (2, "   read_pointer: %p\n", dev->read_pointer);
	}
    }

  DBG (5, "\nREAD BUFFER INFO: \n");
  DBG (5, "   write ptr:     %p\n", dev->read_buffer->writeptr);
  DBG (5, "   read ptr:      %p\n", dev->read_buffer->readptr);
  DBG (5, "   max write ptr: %p\n", dev->read_buffer->max_writeptr);
  DBG (5, "   buffer size:   %lu\n", (u_long)dev->read_buffer->size);
  DBG (5, "   line size:     %lu\n", (u_long)dev->read_buffer->linesize);
  DBG (5, "   empty:         %d\n", dev->read_buffer->empty);
  DBG (5, "   line no:       %d\n", dev->read_buffer->image_line_no);


  /* If there is space in the read buffer, copy the transfer buffer over */
  if (read_buffer_bytes_available (dev->read_buffer) >= dev->bytes_in_buffer)
    {
      even_byte = SANE_TRUE;
      while (dev->bytes_in_buffer)
	{
	 
	  /* Colour Scan */
	  if (isColourScan)
	    {
	      if (even_byte)
		read_buffer_add_byte (dev->read_buffer, dev->read_pointer+1);
	      else
		read_buffer_add_byte (dev->read_buffer, dev->read_pointer-1);
	      even_byte = ! even_byte;
	    }
	  /* Gray or Black&White Scan */
	  else
	    {
	      /* read_buffer_add_byte_gray(dev->read_pointer); */
	      read_buffer_add_byte_gray (dev->read_buffer, dev->read_pointer);
	    }

	  dev->read_pointer = dev->read_pointer + sizeof (SANE_Byte);
	  dev->bytes_in_buffer--;
	}
      /* free the transfer buffer */
      free (dev->transfer_buffer);
      dev->transfer_buffer = NULL;
    }

  /* Read blocks out of read buffer */
  bytes_read = read_buffer_get_bytes (dev->read_buffer, data, size);

  DBG (2, "sanei_lexmark_x1100_read_scan_data:\n");
  DBG (2, "    Copying lines from buffer to data\n");
  DBG (2, "    bytes_remaining: %lu\n", (u_long)dev->bytes_remaining);
  DBG (2, "    bytes_in_buffer: %lu\n", (u_long)dev->bytes_in_buffer);
  DBG (2, "    read_pointer: %p\n", dev->read_buffer->readptr);
  DBG (2, "    bytes_read %lu\n", (u_long) bytes_read);

  /* if no more bytes to xfer and read buffer empty we're at the end */
  if ((dev->bytes_remaining == 0) && read_buffer_is_empty (dev->read_buffer))
    {
      if (!dev->eof)
	{
	  DBG (2, "sanei_lexmark_x1100_read_scan_data: EOF- parking the scanner\n"); 
	  dev->eof = SANE_TRUE;
	  x1100_rewind (dev);
	}
      else
	{
	  DBG (2, "ERROR: Why are we trying to set eof more than once?\n");
	}
    }

  return bytes_read;
}

void
x1100_rewind (Lexmark_Device * dev)
{

  SANE_Int new_location;
  SANE_Int location;
  SANE_Int scan_resolution;
  SANE_Int fudge_factor;
  SANE_Bool colour_scan;
  SANE_Int vert_scan_start;
  SANE_Int hor_scan_start;
  SANE_Int vert_scan_dist;
  SANE_Int vert_rewind_dist;
  SANE_Int devnum;

  DBG (3, "x1100_rewind\n");

  /* We rewind at 1200dpi resolution. 
     The variable "vert_scan_dist" is the distance of the scan in the 
     resolution of the scan "scan_resolution".
     vert_rewind_dist = vert_scan_dist * 1200/scan_resolution + fudge_factor
     Note: fudge_factor is different for each resolution and between colour
     and grayscale scans at the same resolution. */

  /* Scan resolution */
  scan_resolution = dev->val[OPT_Y_DPI].w;

  /* Colour mode */
  if (strcmp (dev->val[OPT_MODE].s, "Color") == 0)
    colour_scan = SANE_TRUE;
  else
    colour_scan = SANE_FALSE;

  /* Init fudge factor to 0 */
  fudge_factor = 0x00;

  if (scan_resolution == 75)
    {
      if (colour_scan)
	fudge_factor = 0x90;
      else
	fudge_factor = 0xb0;
    }
  else if (scan_resolution == 150)
    {
      if (colour_scan)
	fudge_factor = 0x78;
      else
	fudge_factor = 0xb0;
    }
  else if (scan_resolution == 300)
    {
      if (colour_scan)
	fudge_factor = 0x60;
      else
	fudge_factor = 0x78;
    }
  else if (scan_resolution == 600)
    {
      if (colour_scan)
	fudge_factor = 0x146;
      else
	fudge_factor = 0x142;
    }
  else if (scan_resolution == 1200)
    {
      if (colour_scan)
	fudge_factor = 0x140;
      else
	fudge_factor = 0x13c;
    }

  x1100_get_start_loc (scan_resolution, &vert_scan_start,
		       &hor_scan_start, 0);
  vert_scan_dist = vert_scan_start + dev->pixel_height + 2;
  vert_rewind_dist = vert_scan_dist * 1200 / scan_resolution + fudge_factor;
  location = vert_rewind_dist - 1;
  new_location = vert_rewind_dist;

  devnum = dev->devnum;


  /* sleep for 10 ms */
  /* usleep(10000); */

  /* park the scanner */
  x1100_clr_c6 (devnum);
  x1100_stop_mvmt (devnum);

  /* sleep for 20 ms */
  usleep(20000);

  x1100_stop_mvmt (devnum);
  x1100_clr_c6 (devnum);

  /* set regs for rewind */
  shadow_regs[0x2f] = 0xa1;
  shadow_regs[0x32] = 0x00;
  shadow_regs[0x39] = 0x00;

  /* all other regs are always the same. these ones change with parameters */
  /* the following 4 regs are the location 61,60 and the location+1 63,62 */

  shadow_regs[0x60] = (SANE_Byte) (location & 0x00FF);
  shadow_regs[0x61] = (SANE_Byte) (location >> 8 & 0x00FF);
  shadow_regs[0x62] = (SANE_Byte) (new_location & 0x00FF);
  shadow_regs[0x63] = (SANE_Byte) (new_location >> 8 & 0x00FF);

/* set regs for rewind */
  shadow_regs[0x79] = 0x40;
  shadow_regs[0xb2] = 0x04;
  shadow_regs[0xc3] = 0x81;
  shadow_regs[0xc6] = 0x01;
  shadow_regs[0xc9] = 0x3b;
  shadow_regs[0xe0] = 0x2b;
  shadow_regs[0xe1] = 0x17;
  shadow_regs[0xe2] = 0xe7;
  shadow_regs[0xe3] = 0x03;
  shadow_regs[0xe6] = 0xdc;
  shadow_regs[0xe7] = 0xb3;
  shadow_regs[0xe8] = 0x07;
  shadow_regs[0xe9] = 0x1b;
  shadow_regs[0xea] = 0x00;
  shadow_regs[0xeb] = 0x00;
  shadow_regs[0xec] = 0x07;
  shadow_regs[0xef] = 0x03;

  /*Set regs x2 */
  shadow_regs[0x32] = 0x00;
  x1100_write_all_regs (devnum);
  shadow_regs[0x32] = 0x40;
  x1100_write_all_regs (devnum);

  /* Move */
  x1100_stop_mvmt (devnum);
  x1100_start_mvmt (devnum);
}


SANE_Status
read_buffer_init (Lexmark_Device *dev, int bytesperline)
{
  size_t no_lines_in_buffer;

  DBG (2, "read_buffer_init: Start\n");

  dev->read_buffer = (Read_Buffer *) malloc (sizeof (Read_Buffer));
  if (dev->read_buffer == NULL)
    return SANE_STATUS_NO_MEM;
  dev->read_buffer->linesize = bytesperline;
  dev->read_buffer->gray_offset = 0;
  dev->read_buffer->max_gray_offset = bytesperline - 1;
  dev->read_buffer->region = RED;
  dev->read_buffer->red_offset = 0;
  dev->read_buffer->green_offset = 1;
  dev->read_buffer->blue_offset = 2;
  dev->read_buffer->max_red_offset = bytesperline - 3;
  dev->read_buffer->max_green_offset = bytesperline - 2;
  dev->read_buffer->max_blue_offset = bytesperline - 1;
  no_lines_in_buffer = 3 * MAX_XFER_SIZE / bytesperline;
  dev->read_buffer->size = bytesperline * no_lines_in_buffer;
  dev->read_buffer->data = (SANE_Byte *) malloc (dev->read_buffer->size);
  if (dev->read_buffer->data == NULL)
    return SANE_STATUS_NO_MEM;
  dev->read_buffer->readptr = dev->read_buffer->data;
  dev->read_buffer->writeptr = dev->read_buffer->data;
  dev->read_buffer->max_writeptr = dev->read_buffer->data +
    (no_lines_in_buffer - 1) * bytesperline;
  dev->read_buffer->empty = SANE_TRUE;
  dev->read_buffer->image_line_no = 0;

  return SANE_STATUS_GOOD;
}

SANE_Status
read_buffer_free (Read_Buffer *read_buffer)
{
  DBG (2, "read_buffer_free:\n");
  if (read_buffer)
    {
      free (read_buffer->data);
      free (read_buffer);
      read_buffer = NULL;
    }
  return SANE_STATUS_GOOD;
}

size_t
read_buffer_bytes_available (Read_Buffer *rb)
{

  DBG (2, "read_buffer_bytes_available:\n");

  if (rb->empty)
    return rb->size;
  else if ((size_t)abs(rb->writeptr - rb->readptr) < rb->linesize)
    return 0;			/* ptrs are less than one line apart */
  else if (rb->writeptr < rb->readptr)
    return (rb->readptr - rb->writeptr - rb->linesize);
  else
    return (rb->size + rb->readptr - rb->writeptr - rb->linesize);
}

SANE_Status
read_buffer_add_byte (Read_Buffer *rb, SANE_Byte * byte_pointer)
{

  /* DBG(2, "read_buffer_add_byte:\n"); */
  /* F.O. Need to fix the endian byte ordering here */

  switch (rb->region)
    {
    case RED:
      *(rb->writeptr + rb->red_offset) = *byte_pointer;
      if (rb->red_offset == rb->max_red_offset)
	{
	  rb->red_offset = 0;
	  rb->region = GREEN;
	}
      else
	rb->red_offset = rb->red_offset + (3 * sizeof (SANE_Byte));
      return SANE_STATUS_GOOD;
    case GREEN:
      *(rb->writeptr + rb->green_offset) = *byte_pointer;
      if (rb->green_offset == rb->max_green_offset)
	{
	  rb->green_offset = 1;
	  rb->region = BLUE;
	}
      else
	rb->green_offset = rb->green_offset + (3 * sizeof (SANE_Byte));
      return SANE_STATUS_GOOD;
    case BLUE:
      *(rb->writeptr + rb->blue_offset) = *byte_pointer;
      if (rb->blue_offset == rb->max_blue_offset)
	{
	  rb->image_line_no++;
	  /* finished a line. read_buffer no longer empty */
	  rb->empty = SANE_FALSE;
	  rb->blue_offset = 2;
	  rb->region = RED;
	  if (rb->writeptr == rb->max_writeptr)
	    rb->writeptr = rb->data;	/* back to beginning of buffer */
	  else
	    rb->writeptr = rb->writeptr + rb->linesize;	/* next line */
	}
      else
	rb->blue_offset = rb->blue_offset + (3 * sizeof (SANE_Byte));
      return SANE_STATUS_GOOD;
    }
  return SANE_STATUS_GOOD;
}

SANE_Status
read_buffer_add_byte_gray (Read_Buffer *rb, SANE_Byte * byte_pointer)
{

  /*  DBG(2, "read_buffer_add_byte_gray:\n"); */

  /* Fix the endian byte ordering here */
  if ((rb->gray_offset & 0x01) == 0)
    {
      /*Even byte number */
      *(rb->writeptr + rb->gray_offset + 1) = *byte_pointer;
    }
  else
    {
      /*Odd byte number */
      *(rb->writeptr + rb->gray_offset - 1) = *byte_pointer;
    }
  if (rb->gray_offset == rb->max_gray_offset)
    {
      rb->image_line_no++;
      /* finished a line. read_buffer no longer empty */
      rb->empty = SANE_FALSE;
      rb->gray_offset = 0;

      if (rb->writeptr == rb->max_writeptr)
	rb->writeptr = rb->data;	/* back to beginning of buffer */
      else
	rb->writeptr = rb->writeptr + rb->linesize;	/* next line */
    }
  else
    rb->gray_offset = rb->gray_offset + (1 * sizeof (SANE_Byte));
  return SANE_STATUS_GOOD;
}


size_t
read_buffer_get_bytes (Read_Buffer *rb, SANE_Byte * buffer, size_t rqst_size)
{
  /* Read_Buffer *rb; */
  size_t available_bytes;

  /* rb = read_buffer; */
  if (rb->empty)
    return 0;
  else if (rb->writeptr > rb->readptr)
    {
      available_bytes = rb->writeptr - rb->readptr;
      if (available_bytes <= rqst_size)
	{
	  /* We can read from the read pointer up to the write pointer */
	  memcpy (buffer, rb->readptr, available_bytes);
	  rb->readptr = rb->writeptr;
	  rb->empty = SANE_TRUE;
	  return available_bytes;
	}
      else
	{
	  /* We can read from the full request size */
	  memcpy (buffer, rb->readptr, rqst_size);
	  rb->readptr = rb->readptr + rqst_size;
	  return rqst_size;
	}
    }
  else
    {
      /* The read pointer is ahead of the write pointer. Its wrapped around. */
      /* We can read to the end of the buffer and make a recursive call to */
      /* read any available lines at the beginning of the buffer */
      available_bytes = rb->data + rb->size - rb->readptr;
      if (available_bytes <= rqst_size)
	{
	  /* We can read from the read pointer up to the end of the buffer */
	  memcpy (buffer, rb->readptr, available_bytes);
	  rb->readptr = rb->data;
	  if (rb->writeptr == rb->readptr)
	    rb->empty = SANE_TRUE;
	  return available_bytes +
	    read_buffer_get_bytes (rb, buffer + available_bytes,
				   rqst_size - available_bytes);
	}
      else
	{
	  /* We can read from the full request size */
	  memcpy (buffer, rb->readptr, rqst_size);
	  rb->readptr = rb->readptr + rqst_size;
	  return rqst_size;
	}
    }
}

SANE_Bool
read_buffer_is_empty (Read_Buffer *read_buffer)
{
  return read_buffer->empty;
}


