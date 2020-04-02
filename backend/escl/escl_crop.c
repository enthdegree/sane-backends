/* sane - Scanner Access Now Easy.

   Copyright (C) 2020 Thierry HUCHARD <thierry@ordissimo.com>

   This file is part of the SANE package.

   SANE is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 3 of the License, or (at your
   option) any later version.

   SANE is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with sane; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   This file implements a SANE backend for eSCL scanners.  */

#define DEBUG_DECLARE_ONLY
#include "../include/sane/config.h"

#include "escl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char *
escl_crop_surface(capabilities_t *scanner,
               unsigned char *surface,
	       int w,
	       int h,
	       int bps,
	       int *width,
	       int *height)
{
    double ratio = 1.0;
    int x_off = 0, x = 0;
    int real_w = 0;
    int y_off = 0, y = 0;
    int real_h = 0;
    unsigned char *surface_crop = NULL;

    DBG( 1, "Escl Image Crop\n");
    ratio = (double)w / (double)scanner->width;
    scanner->width = w;
    if (scanner->pos_x < 0)
           scanner->pos_x = 0;
    if (scanner->pos_x && scanner->width > scanner->pos_x)
       x_off = (int)((double)scanner->pos_x * ratio);
    real_w = scanner->width - x_off;

    scanner->height = h;
    if (scanner->pos_y < 0)
       scanner->pos_y = 0;
    if (scanner->pos_y && scanner->pos_y < scanner->height)
       y_off = (int)((double)scanner->pos_y * ratio);
    real_h = scanner->height - y_off;

    *width = real_w;
    *height = real_h;

    if (x_off > 0 || real_w < scanner->width ||
        y_off > 0 || real_h < scanner->height) {
          surface_crop = (unsigned char *)malloc (sizeof (unsigned char) * real_w
                     * real_h * bps);
	      if(!surface_crop) {
             DBG( 1, "Escl Crop : Surface_crop Memory allocation problem\n");
	         free(surface);
	         surface = NULL;
	         goto finish;
	      }
          for (y = 0; y < real_h; y++)
          {
             for (x = 0; x < real_w; x++)
             {
                surface_crop[(y * real_w * bps) + (x * bps)] =
                    surface[((y + y_off) * w  * bps) + ((x + x_off) * bps)];
                surface_crop[(y * real_w * bps) + (x * bps) + 1] =
                    surface[((y + y_off) * w  * bps) + ((x + x_off) * bps) + 1];
                surface_crop[(y * real_w * bps) + (x * bps) + 2] =
                    surface[((y + y_off) * w  * bps) + ((x + x_off) * bps) + 2];
             }
          }
          free(surface);
	      surface = surface_crop;
    }
    // we don't need row pointers anymore
    scanner->img_data = surface;
    scanner->img_size = (int)(real_w * real_h * bps);
    scanner->img_read = 0;
finish:
    return surface;
}
