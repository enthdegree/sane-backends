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
get_TIFF_data(capabilities_t *scanner, int *width, int *height, int *bps)
{
    TIFF* tif = NULL;
    uint32  w = 0;
    uint32  h = 0;
    unsigned char *surface = NULL;         /*  image data*/
    int components = 4;
    uint32 npixels = 0;
    SANE_Status status = SANE_STATUS_GOOD;

    lseek(fileno(scanner->tmp), 0, SEEK_SET);
    tif = TIFFFdOpen(fileno(scanner->tmp), "temp", "r");
    if (!tif) {
        DBG( 1, "Escl Tiff : Can not open, or not a TIFF file.\n");
        status = SANE_STATUS_INVAL;
	goto close_file;
    }

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
    npixels = w * h;
    surface = (unsigned char*) malloc(npixels * sizeof (uint32));
    if (surface != NULL)
    {
        DBG( 1, "Escl Tiff : raster Memory allocation problem.\n");
        status = SANE_STATUS_INVAL;
	goto close_tiff;
    }

    if (!TIFFReadRGBAImage(tif, w, h, (uint32 *)surface, 0))
    {
        DBG( 1, "Escl Tiff : Problem reading image data.\n");
        status = SANE_STATUS_INVAL;
        free(surface);
	goto close_tiff;
    }

    *bps = components;

    // If necessary, trim the image.
    surface = escl_crop_surface(scanner, surface, w, h, components, width, height);
    if (!surface)  {
        DBG( 1, "Escl Tiff : Surface Memory allocation problem\n");
        status = SANE_STATUS_INVAL;
    }

close_tiff:
    TIFFClose(tif);
close_file:
    if (scanner->tmp)
       fclose(scanner->tmp);
    scanner->tmp = NULL;
    return (status);
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
