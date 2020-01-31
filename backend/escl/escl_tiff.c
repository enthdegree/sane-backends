/* sane - Scanner Access Now Easy.

   Copyright (C) 2019 Touboul Nathane
   Copyright (C) 2019 Thierry HUCHARD <thierry@ordissimo.com>

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

#include "../include/sane/sanei.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if(defined HAVE_TIFFIO_H)
#include <tiffio.h>
#endif

#include <setjmp.h>


#if(defined HAVE_TIFFIO_H)

/**
 * \fn SANE_Status escl_sane_decompressor(escl_sane_t *handler)
 * \brief Function that aims to decompress the png image to SANE be able to read the image.
 *        This function is called in the "sane_read" function.
 *
 * \return SANE_STATUS_GOOD (if everything is OK, otherwise, SANE_STATUS_NO_MEM/SANE_STATUS_INVAL)
 */
SANE_Status
get_TIFF_data(capabilities_t *scanner, int *w, int *h, int *components)
{
    TIFF* tif = NULL;
    uint32  width = 0;           /* largeur */
    uint32  height = 0;          /* hauteur */
    unsigned char *raster = NULL;         /* données de l'image */
    unsigned char *surface = NULL;         /* données de l'image */
    int bps = 4;
    uint32 npixels = 0;
    int x_off = 0, x = 0;
    int wid = 0;
    int y_off = 0, y = 0;
    int hei = 0;

    lseek(fileno(scanner->tmp), 0, SEEK_SET);
    tif = TIFFFdOpen(fileno(scanner->tmp), "temp", "r");
    if (!tif) {
        DBG( 1, "Escl Tiff : Can not open, or not a TIFF file.\n");
        if (scanner->tmp) {
           fclose(scanner->tmp);
           scanner->tmp = NULL;
        }
        return (SANE_STATUS_INVAL);
    }

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
    npixels = width * height;
    raster = (unsigned char*) malloc(npixels * sizeof (uint32));
    if (raster != NULL)
    {
        DBG( 1, "Escl Tiff : raster Memory allocation problem.\n");
        if (scanner->tmp) {
           fclose(scanner->tmp);
           scanner->tmp = NULL;
        }
        return (SANE_STATUS_INVAL);
    }

    if (!TIFFReadRGBAImage(tif, width, height, (uint32 *)raster, 0))
    {
        DBG( 1, "Escl Tiff : Problem reading image data.\n");
        if (scanner->tmp) {
           fclose(scanner->tmp);
           scanner->tmp = NULL;
        }
        return (SANE_STATUS_INVAL);
    }

    if (width < (unsigned int)scanner->width)
           scanner->width = width;
    if (scanner->pos_x < 0)
           scanner->pos_x = 0;

    if (height < (unsigned int)scanner->height)
           scanner->height = height;
    if (scanner->pos_x < 0)
           scanner->pos_x = 0;

    x_off = scanner->pos_x;
    wid = scanner->width - x_off;
    y_off = scanner->pos_y;
    hei = scanner->height - y_off;
    *w = (int)wid;
    *h = (int)hei;
    *components = bps;
    if (x_off > 0 || wid < scanner->width ||
        y_off > 0 || hei < scanner->height) {
          surface = (unsigned char *)malloc (sizeof (unsigned char) * wid
                     * hei * bps);
          if (surface)
         {
             for (y = 0; y < hei; y++)
             {
                for (x = 0; x < wid; x++)
                {
                   surface[y * wid + x] = raster[(y + y_off) * width + x + x_off];
                }
             }
             free(raster);
         }
         else
            surface = raster;
    }
    else
        surface = raster;
    // we don't need row pointers anymore
    scanner->img_data = raster;
    scanner->img_size = (int)(width * height * bps);
    scanner->img_read = 0;
    TIFFClose(tif);
    fclose(scanner->tmp);
    scanner->tmp = NULL;
    return (SANE_STATUS_GOOD);
}
#else

SANE_Status
get_TIFF_data(capabilities_t __sane_unused__ *scanner,
              int __sane_unused__ *w,
              int __sane_unused__ *h,
              int __sane_unused__ *bps)
{
    return (SANE_STATUS_INVAL);
}

#endif
