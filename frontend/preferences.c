/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 David Mosberger-Tang
   This file is part of the SANE package.

   SANE is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   SANE is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with sane; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <preferences.h>
#include <sane/sane.h>
#include <sane/sanei.h>
#include <sane/sanei_wire.h>
#include <sane/sanei_codec_ascii.h>

#define POFFSET(field)	((char *) &((Preferences *) 0)->field - (char *) 0)
#define PFIELD(p,offset,type)	(*((type *)(((char *)(p)) + (offset))))

Preferences preferences =
  {
       0,		/* no preferred device (must be 0 or malloced!) */
       0,		/* no default filename */
       0,		/* advanced user */
       1,		/* tooltips enabled */
    10.0,		/* length unit */
       1,		/* preserve_preview */
       0,		/* preview_own_cmap */
     1.0		/* preview_gamma */
  };

static void w_string (Wire *w, Preferences *p, long offset);
static void w_double (Wire *w, Preferences *p, long offset);
static void w_int (Wire *w, Preferences *p, long offset);

static struct
  {
    SANE_String name;
    void (*codec) (Wire *w, Preferences *p, long offset);
    long offset;
  }
desc[] =
  {
    {"device", w_string, POFFSET(device)},
    {"filename", w_string, POFFSET(filename)},
    {"advanced", w_int, POFFSET(advanced)},
    {"tool-tips", w_int, POFFSET(tooltips_enabled)},
    {"length-unit", w_double, POFFSET(length_unit)},
    {"preserve-preview", w_int, POFFSET(preserve_preview)},
    {"preview-own-cmap", w_int, POFFSET(preview_own_cmap)},
    {"preview-gamma", w_double, POFFSET(preview_gamma)},
  };

static void
w_string (Wire *w, Preferences *p, long offset)
{
  SANE_String string;

  if (w->direction == WIRE_ENCODE)
    string = PFIELD (p, offset, char *);

  sanei_w_string (w, &string);

  if (w->direction == WIRE_DECODE)
    {
      if (w->status == 0)
	{
	  const char **field;

	  field = &PFIELD (p, offset, const char *);
	  if (*field)
	    free ((char *) *field);
	  *field = string ? strdup (string) : 0;
	}
      sanei_w_free (w, (WireCodecFunc) sanei_w_string, &string);
    }
}

static void
w_double (Wire *w, Preferences *p, long offset)
{
  SANE_Word word;

  if (w->direction == WIRE_ENCODE)
    word = SANE_FIX (PFIELD (p, offset, double));

  sanei_w_word (w, &word);

  if (w->direction == WIRE_DECODE)
    {
      if (w->status == 0)
	PFIELD (p, offset, double) = SANE_UNFIX (word);
      sanei_w_free (w, (WireCodecFunc) sanei_w_word, &word);
    }
}

static void
w_int (Wire *w, Preferences *p, long offset)
{
  SANE_Word word;

  if (w->direction == WIRE_ENCODE)
    word = PFIELD (p, offset, int);

  sanei_w_word (w, &word);

  if (w->direction == WIRE_DECODE)
    {
      if (w->status == 0)
	PFIELD (p, offset, int) = word;
      sanei_w_free (w, (WireCodecFunc) sanei_w_word, &word);
    }
}

void
preferences_save (int fd)
{
  Wire w;
  int i;

  w.io.fd = fd;
  w.io.read = read;
  w.io.write = write;
  sanei_w_init (&w, sanei_codec_ascii_init);
  sanei_w_set_dir (&w, WIRE_ENCODE);

  for (i = 0; i < NELEMS (desc); ++i)
    {
      sanei_w_string (&w, &desc[i].name);
      (*desc[i].codec) (&w, &preferences, desc[i].offset);
    }

  sanei_w_set_dir (&w, WIRE_DECODE);	/* flush it out */
}

void
preferences_restore (int fd)
{
  SANE_String name;
  Wire w;
  int i;

  w.io.fd = fd;
  w.io.read = read;
  w.io.write = write;
  sanei_w_init (&w, sanei_codec_ascii_init);
  sanei_w_set_dir (&w, WIRE_DECODE);

  while (1)
    {
      sanei_w_space (&w, 3);
      if (w.status)
	return;

      sanei_w_string (&w, &name);
      if (w.status || !name)
	return;

      for (i = 0; i < NELEMS (desc); ++i)
	{
	  if (strcmp (name, desc[i].name) == 0)
	    {
	      (*desc[i].codec) (&w, &preferences, desc[i].offset);
	      break;
	    }
	}
    }
}
