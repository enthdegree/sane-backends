/* sane - Scanner Access Now Easy.

   Copyright (C) 1997, 1998 Franck Schnefra, Michel Roelofs,
   Emmanuel Blot and Kevin Charter

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

   This file implements a backend for the AGFA SnapScan flatbed
   scanner. */

/*
  This file implements the core of the SnapScan 310/600
  characteristics that defer from the SnapScan 300 model

  It is based on a RGB ring buffer that allows to synchronize
  the chrominance components.
*/

/* $Id$
   SANE SnapScan 310/600 backend extension*/

#undef SNAPSCAN310_FORCE_600_DPI/* define for debugging 310 model */

static SANE_Status rgb_buf_init (SnapScan_Scanner * pss);
static void rgb_buf_clean (SnapScan_Scanner * pss);
static SANE_Int rgb_buf_push_line (SnapScan_Scanner * pss,
				   const SANE_Byte * line,
				   const SANE_Int length);
static void rgb_buf_get_line (SnapScan_Scanner * pss, SANE_Byte * line);
static SANE_Bool rgb_buf_can_get_line (SnapScan_Scanner * pss);
static void rgb_buf_set_diff (SnapScan_Scanner * pss, SANE_Byte g_to_r,
			      SANE_Byte b_to_r);
static SANE_Int transfer_data_diff (u_char * buf, SnapScan_Scanner * pss);

static inline SANE_Int rgb_buf_get_size (SnapScan_Scanner * pss);
static inline SANE_Int rgb_buf_get_line_pos (SnapScan_Scanner * pss);


/* Get the total number of lines of the RGB ring buffer */
static inline SANE_Int
rgb_buf_get_size (SnapScan_Scanner * pss)
{
  return
  (SANE_Int) pss->rgb_buf.g_offset +
  pss->rgb_buf.b_offset +
  pss->rgb_buf.r_offset + 1;
}

/* Get the first good line position from the RGB ring buffer */
static inline SANE_Int
rgb_buf_get_line_pos (SnapScan_Scanner * pss)
{
  return
  pss->rgb_buf.pixel_pos ?
  pss->rgb_buf.line_in - 1 :
  pss->rgb_buf.line_in;
}

/* Initialization of the RGB ring buffer */
static SANE_Status
rgb_buf_init (SnapScan_Scanner * pss)
{
  static const char *me = "rgb_buf_init";

  size_t rgb_buf_bytes;
  pss->rgb_buf.line_in = 0;
  pss->rgb_buf.pixel_pos = 0;
  pss->rgb_buf.line_out = 0;

  /* get the ring buffer size */
  /* number of line = comp. total offset +
                      1 (size of last line) +
                      1 (running new line) */
  rgb_buf_bytes =
    pss->bytes_per_line *
    (rgb_buf_get_size (pss) + 2) *
    sizeof (SANE_Byte);

  /* allocate ring buffer memory */
  pss->rgb_buf.data = (SANE_Byte *) malloc (rgb_buf_bytes);

  if (!pss->rgb_buf.data)
    {
      DBG (DL_MAJOR_ERROR, "%s: rbuf_data cannot be allocated ! Out of memory ?\n", me);
      return SANE_STATUS_NO_MEM;
    }
  else
    {
      DBG (DL_VERBOSE,
	"%s: rgb ring buffer allocated: %d bytes, at %p, comp. offset %d\n",
	   me, (int) rgb_buf_bytes,
	   (void *) pss->rgb_buf.data, (int) rgb_buf_get_size (pss));
    }
  return SANE_STATUS_GOOD;
}

/* Clean up of the RGB ring buffer */
static void
rgb_buf_clean (SnapScan_Scanner * pss)
{
  static const char *me = "rgb_buf_clean";

  /* free previously allocated buffer memory */
  if (pss->rgb_buf.data)
    {
      free (pss->rgb_buf.data);
      DBG (DL_VERBOSE, "%s: release RGB ring buffer at %p\n",
	   me, (void *) pss->rgb_buf.data);
      pss->rgb_buf.data = NULL;
    }

  pss->rgb_buf.line_in = 0;
  pss->rgb_buf.pixel_pos = 0;
  pss->rgb_buf.line_out = 0;
  pss->rgb_buf.g_offset = 0;
  pss->rgb_buf.b_offset = 0;
  pss->rgb_buf.r_offset = 0;
}

/* Add/complete up to 2 lines to the 'top' of the rgb ring buffer */
static SANE_Int
rgb_buf_push_line (SnapScan_Scanner * pss,
		   const SANE_Byte * line,
		   const SANE_Int length)
{
  /* static    const char *me = "rgb_buf_push_line"; */

  SANE_Int max_pixels;		/* maximum number of pixels that can be processed */
  SANE_Int line_pixels;		/* pixels that in the processed line */
  SANE_Int done_pixels;		/* pixels already processed */
  SANE_Bool done_flag;		/* flag set when a full line has been completed */
  SANE_Int index;		/* index to access a line into the ring buffer */

  max_pixels = MIN (length, pss->bytes_per_line);
  done_pixels = 0;
  done_flag = SANE_FALSE;

  while (max_pixels)
    {
      line_pixels = MIN (max_pixels,
			 pss->bytes_per_line - pss->rgb_buf.pixel_pos);

      /* if the line is empty */
      if (0 == pss->rgb_buf.pixel_pos)
	{
	  if (SANE_TRUE == done_flag)
	    {
	      /* prevent against processing more than one full line */
	      break;
	    }

	  ++(pss->rgb_buf.line_in);
	  done_flag = SANE_TRUE;
	}

      index = pss->rgb_buf.line_in;	/* next index */
      index %= rgb_buf_get_size (pss) + 1;	/* convert circular to linear index */
      index *= pss->bytes_per_line;	/* line index -> byte index */

      /* copy the pixel data into the ring buffer */
      memcpy ((void *) (pss->rgb_buf.data + index + pss->rgb_buf.pixel_pos),
	      (const void *) (line + done_pixels),
	      line_pixels);

      done_pixels += line_pixels;
      max_pixels -= line_pixels;
      pss->rgb_buf.pixel_pos += line_pixels;

      /* if a line has been fully completed, job is done */
      if (pss->rgb_buf.pixel_pos >= pss->bytes_per_line)
	{
	  pss->rgb_buf.pixel_pos = 0;
	  done_flag = SANE_TRUE;
	  break;
	}
    }

  return done_pixels;
}

/* Create a plain line from the RGB ring buffer */
static void
rgb_buf_get_line (SnapScan_Scanner * pss, SANE_Byte * line)
{
  /* static const char *me = "rgb_buf_get_line"; */

  SANE_Int modulo;		/* to convert virtual to physical buffer index */
  SANE_Int comp_pixels;		/* number of pixel f a component to process    */
  SANE_Int linear_pixel;	/* pix pos in a interlaced component line      */
  SANE_Int max_offset;		/* total height (in line) of the ring buffer   */
  SANE_Int r_pos;		/* offset of the red component   */
  SANE_Int b_pos;		/* offset of the blue component  */
  SANE_Int g_pos;		/* offset of the green component */
  SANE_Int pixel;		/* pixel loop counter */
  SANE_Int line_pos;		/* first valid line */

  modulo = rgb_buf_get_size (pss) + 1;
  line_pos = rgb_buf_get_line_pos (pss);
  comp_pixels = pss->bytes_per_line / 3;
  linear_pixel = 0;
  max_offset = MAX (MAX (pss->rgb_buf.g_offset, pss->rgb_buf.b_offset), pss->rgb_buf.r_offset);
  r_pos = (line_pos - (max_offset - pss->rgb_buf.r_offset)) % modulo;
  b_pos = (line_pos - (max_offset - pss->rgb_buf.b_offset)) % modulo;
  g_pos = (line_pos - (max_offset - pss->rgb_buf.g_offset)) % modulo;

  r_pos *= pss->bytes_per_line;
  g_pos *= pss->bytes_per_line;
  b_pos *= pss->bytes_per_line;
  g_pos += comp_pixels;		/* planar to interlaced pixels */
  b_pos += 2 * comp_pixels;	/* planar to interlaced pixels */

  /* for each pixel in the line,
     collect the three components and set the pixel RGB value */
#ifdef SNAPSCAN310_FORCE_600_DPI
  if (pss->res != 600)
    {
#endif
      for (pixel = 0; pixel < comp_pixels; pixel++)
	{
	  line[linear_pixel++] = pss->rgb_buf.data[pixel + r_pos];
	  line[linear_pixel++] = pss->rgb_buf.data[pixel + g_pos];
	  line[linear_pixel++] = pss->rgb_buf.data[pixel + b_pos];
	}
#ifdef SNAPSCAN310_FORCE_600_DPI
    }
  else
    {
      /* 310 model seems to produce 300x600 dpi in 600 dpi mode,
       thus, we have to supersample by 2 times within the
       horizontal direction. Linear filtering, or bilinear
       filtering would do better results ... */
      comp_pixels >>= 1;
      for (pixel = 0; pixel < comp_pixels; pixel++)
	{
	  line[linear_pixel++] = pss->rgb_buf.data[pixel + r_pos];
	  line[linear_pixel++] = pss->rgb_buf.data[pixel + g_pos];
	  line[linear_pixel++] = pss->rgb_buf.data[pixel + b_pos];
	  line[linear_pixel++] = pss->rgb_buf.data[pixel + r_pos];
	  line[linear_pixel++] = pss->rgb_buf.data[pixel + g_pos];
	  line[linear_pixel++] = pss->rgb_buf.data[pixel + b_pos];
	}
    }
#endif

  /* increase the valid read line counter */
  ++(pss->rgb_buf.line_out);
}

/* Check if a valid line can be generated from the RGB ring buffer */
static SANE_Bool
rgb_buf_can_get_line (SnapScan_Scanner * pss)
{
  /* static const char *me = "rgb_buf_can_get_line"; */

  return
    (rgb_buf_get_line_pos (pss) - pss->rgb_buf.line_out) >
    (pss->rgb_buf.g_offset + pss->rgb_buf.b_offset + pss->rgb_buf.r_offset) ?
    SANE_TRUE : SANE_FALSE;
}

/* Set the red, green, and blue difference */
static void
rgb_buf_set_diff (SnapScan_Scanner * pss, SANE_Byte g_to_r, SANE_Byte b_to_r)
{
  static char me[] = "rgb_buf_set_diff";

  signed char min_diff;
  signed char g = (g_to_r & 0x80) ? -(g_to_r & 0x7F) : g_to_r;
  signed char b = (b_to_r & 0x80) ? -(b_to_r & 0x7F) : b_to_r;

  min_diff = MIN (MIN (b, g), 0);

  pss->rgb_buf.g_offset = (u_char) (g - min_diff);
  pss->rgb_buf.b_offset = (u_char) (b - min_diff);
  pss->rgb_buf.r_offset = (u_char) (0 - min_diff);

  DBG (DL_VERBOSE, "%s: Chroma offsets Red:%u, Green:%u Blue:%u\n",
       me,
       pss->rgb_buf.r_offset,
       pss->rgb_buf.g_offset,
       pss->rgb_buf.b_offset);
}

static SANE_Int
transfer_data_diff (u_char * buf, SnapScan_Scanner * pss)
{
  static char me[] = "transfer_data_diff";

  SANE_Int done_bytes;		/* bytes that have been processed               */
  SANE_Int line_bytes;		/* bytes processed in the last 'push_line' call */
  SANE_Int remaining_bytes;	/* bytes to be processed                        */
  SANE_Int transferred_bytes;	/* bytes generated by the ring buffer           */

  done_bytes = 0;
  line_bytes = 0;
  remaining_bytes = pss->read_bytes;
  transferred_bytes = 0;

  if (pss->read_bytes > 0)
    {
      /* lines = pss->read_bytes / pss->bytes_per_line; */

      transferred_bytes = 0;
      do
	{
	  line_bytes = rgb_buf_push_line (pss, pss->buf + done_bytes, remaining_bytes);
	  remaining_bytes -= line_bytes;
	  done_bytes += line_bytes;

	  if (rgb_buf_can_get_line (pss))
	    {
	      rgb_buf_get_line (pss, buf + transferred_bytes);
	      transferred_bytes += pss->bytes_per_line;
	    }
	}
      while (remaining_bytes);	/*while ( --lines );*/
    }

  DBG (DL_VERBOSE, "%s: transferred %lu lines (%lu bytes)\n",
       me, (u_long) pss->read_bytes / pss->bytes_per_line,
       (u_long) transferred_bytes);

  return transferred_bytes;
}

/* $Log$
 * Revision 1.1  1999/08/09 18:05:54  pere
 * Initial revision
 *
 * Revision 1.1.3.1  1998/03/10 23:42:01  eblot
 * Debugging: large windows, color preview
 *
 * Revision 1.1.2  1998/03/10 21:45:14  eblot
 * Workaround for the color preview mode
 *
 * Revision 1.1.1  1998/03/10 21:32:07  eblot
 * Workaround for the '4096 bytes' bug
 * */
