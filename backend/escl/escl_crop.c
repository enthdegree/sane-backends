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
	/*
    int x_off = 0, x = 0;
    int real_w = 0;
    int y_off = 0, y = 0;
    int real_h = 0;
    unsigned char *surface_crop = NULL;
    */
/*
    DBG( 1, "Escl Image Crop\n");
    if (w < (int)scanner->caps[scanner->source].width)
           scanner->caps[scanner->source].width = w;
    if (scanner->caps[scanner->source].pos_x < 0)
           scanner->caps[scanner->source].pos_x = 0;

    if (h < (int)scanner->caps[scanner->source].height)
           scanner->caps[scanner->source].height = h;
    if (scanner->caps[scanner->source].pos_x < 0)
           scanner->caps[scanner->source].pos_x = 0;

    DBG( 1, "Escl Image Crop [%dx%d|%dx%d]\n", scanner->caps[scanner->source].pos_x, scanner->caps[scanner->source].pos_y,
		    scanner->caps[scanner->source].width, scanner->caps[scanner->source].height);
    x_off = scanner->caps[scanner->source].pos_x;
    if (x_off >= scanner->caps[scanner->source].width) {
       real_w = scanner->caps[scanner->source].width;
       x_off = 0;
    }
    else
       real_w = scanner->caps[scanner->source].width - x_off;
    y_off = scanner->caps[scanner->source].pos_y;
    if(y_off >= scanner->caps[scanner->source].height) {
       real_h = scanner->caps[scanner->source].height;
       y_off = 0;
    }
    else
       real_h = scanner->caps[scanner->source].height - y_off;
  */
    *width = w; //real_w;
    *height = h; //real_h;
    DBG( 1, "Escl Image Crop [%dx%d]\n", *width, *height);
    /*
    if (x_off > 0 || real_w < scanner->caps[scanner->source].width ||
        y_off > 0 || real_h < scanner->caps[scanner->source].height) {
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
                surface_crop[y * real_w + x] = surface[(y + y_off) * w + x + x_off];
             }
          }
          free(surface);
	  surface = surface_crop;
    }
    */
    // we don't need row pointers anymore
    scanner->img_data = surface;
    scanner->img_size = (int)(w * h * bps);
    // scanner->img_size = (int)(real_w * real_h * bps);
    scanner->img_read = 0;
finish:
    return surface;
}
