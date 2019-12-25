#include "escl.h"

#include "../include/sane/sanei.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if(defined HAVE_LIBPNG)
#include <png.h>
#endif

#include <setjmp.h>


#if(defined HAVE_LIBPNG)

/**
 * \fn SANE_Status escl_sane_decompressor(escl_sane_t *handler)
 * \brief Function that aims to decompress the png image to SANE be able to read the image.
 *        This function is called in the "sane_read" function.
 *
 * \return SANE_STATUS_GOOD (if everything is OK, otherwise, SANE_STATUS_NO_MEM/SANE_STATUS_INVAL)
 */
SANE_Status
get_PNG_data(capabilities_t *scanner, int *w, int *h, int *components)
{
	unsigned int  width = 0;           /* largeur */
	unsigned int  height = 0;          /* hauteur */
	int           bps = 3;  /* composantes d'un texel */
	unsigned char *texels = NULL;         /* données de l'image */
        unsigned int i = 0;
	png_byte magic[8];

	// read magic number
	fread (magic, 1, sizeof (magic), scanner->tmp);
	// check for valid magic number
	if (!png_check_sig (magic, sizeof (magic)))
	{
		fprintf(stderr,"PNG error: is not a valid PNG image!\n");
		return (SANE_STATUS_INVAL);
	}
	// create a png read struct
	png_structp png_ptr = png_create_read_struct
		(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
	{
		return (SANE_STATUS_INVAL);
	}
	// create a png info struct
	png_infop info_ptr = png_create_info_struct (png_ptr);
	if (!info_ptr)
	{
		png_destroy_read_struct (&png_ptr, NULL, NULL);
		return (SANE_STATUS_INVAL);
	}
	// initialize the setjmp for returning properly after a libpng
	//   error occured
	if (setjmp (png_jmpbuf (png_ptr)))
	{
		png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
		if (texels)
		  free (texels);
        fprintf(stderr,"PNG read error.\n");
		return (SANE_STATUS_INVAL);
	}
	// setup libpng for using standard C fread() function
	//   with our FILE pointer
	png_init_io (png_ptr, scanner->tmp);
	// tell libpng that we have already read the magic number
	png_set_sig_bytes (png_ptr, sizeof (magic));

	// read png info
	png_read_info (png_ptr, info_ptr);

	int bit_depth, color_type;
	// get some usefull information from header
	bit_depth = png_get_bit_depth (png_ptr, info_ptr);
	color_type = png_get_color_type (png_ptr, info_ptr);
	// convert index color images to RGB images
	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb (png_ptr);
	else if (color_type != PNG_COLOR_TYPE_RGB && color_type != PNG_COLOR_TYPE_RGB_ALPHA)
	{
        fprintf(stderr,"PNG format not supported.\n");
		return (SANE_STATUS_INVAL);
	}
    if (color_type ==  PNG_COLOR_TYPE_RGB_ALPHA)
        bps = 4;
    else
	    bps = 3;
	if (png_get_valid (png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha (png_ptr);
	if (bit_depth == 16)
		png_set_strip_16 (png_ptr);
	else if (bit_depth < 8)
		png_set_packing (png_ptr);
	// update info structure to apply transformations
	png_read_update_info (png_ptr, info_ptr);
	// retrieve updated information
	png_get_IHDR (png_ptr, info_ptr,
			(png_uint_32*)(&width),
			(png_uint_32*)(&height),
			&bit_depth, &color_type,
			NULL, NULL, NULL);

    *w = (int)width;
    *h = (int)height;
    *components = bps;
	// we can now allocate memory for storing pixel data
	texels = (unsigned char *)malloc (sizeof (unsigned char) * width
			* height * bps);
	png_bytep *row_pointers;
	// setup a pointer array.  Each one points at the begening of a row.
	row_pointers = (png_bytep *)malloc (sizeof (png_bytep) * height);
	for (i = 0; i < height; ++i)
	{
		row_pointers[i] = (png_bytep)(texels +
				((height - (i + 1)) * width * bps));
	}
	// read pixel data using row pointers
	png_read_image (png_ptr, row_pointers);
	// we don't need row pointers anymore
	scanner->img_data = texels;
    scanner->img_size = (int)(width * height * bps);
    scanner->img_read = 0;
	free (row_pointers);
    fclose(scanner->tmp);
    scanner->tmp = NULL;
    return (SANE_STATUS_GOOD);
}
#else

SANE_Status
get_PNG_data(capabilities_t __sane_unused__ *scanner,
              int __sane_unused__ *w,
              int __sane_unused__ *h,
              int __sane_unused__ *bps)
{
}

#endif
