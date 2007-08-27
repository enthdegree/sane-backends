/* sane - Scanner Access Now Easy.

   Copyright (C) 2003, 2004 Henning Meier-Geinitz <henning@meier-geinitz.de>
   Copyright (C) 2005 Stephane Voltz <stef.dev@free.fr>
   Copyright (C) 2006 Laurent Charpentier <laurent_pubs@yahoo.com>
   
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
*/

#ifndef GENESYS_H
#define GENESYS_H

#include "genesys_low.h"

#define ENABLE(OPTION)  s->opt[OPTION].cap &= ~SANE_CAP_INACTIVE
#define DISABLE(OPTION) s->opt[OPTION].cap |=  SANE_CAP_INACTIVE
#define IS_ACTIVE(OPTION) (((s->opt[OPTION].cap) & SANE_CAP_INACTIVE) == 0)

#define GENESYS_CONFIG_FILE "genesys.conf"

/* Maximum time for lamp warm-up */
#define WARMUP_TIME 45

#ifndef SANE_I18N
#define SANE_I18N(text) text
#endif


/** List of SANE options
 */
enum Genesys_Option
{
  OPT_NUM_OPTS = 0,

  OPT_MODE_GROUP,
  OPT_MODE,
  OPT_SOURCE,
  OPT_PREVIEW,
  OPT_BIT_DEPTH,
  OPT_RESOLUTION,

  OPT_GEOMETRY_GROUP,
  OPT_TL_X,			/* top-left x */
  OPT_TL_Y,			/* top-left y */
  OPT_BR_X,			/* bottom-right x */
  OPT_BR_Y,			/* bottom-right y */

  OPT_EXTRAS_GROUP,
  OPT_LAMP_OFF_TIME,
  OPT_THRESHOLD,
  OPT_DISABLE_INTERPOLATION,
  OPT_COLOR_FILTER,
  /* must come last: */
  NUM_OPTIONS
};


/** Scanner object.
 */
typedef struct Genesys_Scanner
{
  struct Genesys_Scanner *next;	    /**< Next scanner in list */
  Genesys_Device *dev;		    /**< Low-level device object */

  /* SANE data */
  SANE_Bool scanning;			   /**< We are currently scanning */
  SANE_Option_Descriptor opt[NUM_OPTIONS]; /**< Option descriptors */
  Option_Value val[NUM_OPTIONS];	   /**< Option values */
  SANE_Parameters params;		   /**< SANE Parameters */
  SANE_Int bpp_list[5];			   /**< */
} Genesys_Scanner;

#endif /* not GENESYS_H */
