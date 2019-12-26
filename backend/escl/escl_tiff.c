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
    int bps = 4;
    uint32 npixels = 0;

    lseek(fileno(scanner->tmp), 0, SEEK_SET);
    tif = TIFFFdOpen(fileno(scanner->tmp), "temp", "r");
    if (!tif) {
        fprintf(stderr, "Can not open, or not a TIFF file.\n");
        return (SANE_STATUS_INVAL);
    }

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
    npixels = width * height;
    raster = (unsigned char*) malloc(npixels * sizeof (uint32));
    if (raster != NULL)
    {	    
        fprintf(stderr, "Memory allocation problem.\n");
        return (SANE_STATUS_INVAL);
    }

    if (!TIFFReadRGBAImage(tif, width, height, (uint32 *)raster, 0))
    {
        fprintf(stderr, "Problem reading image data.\n");
        return (SANE_STATUS_INVAL);
    }
    *w = (int)width;
    *h = (int)height;
    *components = bps;
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
