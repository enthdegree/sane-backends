/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 David Mosberger-Tang
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
   If you do not wish that, delete this exception notice.  */

#include "../include/sane/config.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei_wire.h"

void
sanei_w_space (Wire *w, size_t howmuch)
{
  size_t nbytes, left_over;
  int fd = w->io.fd;
  ssize_t nread, nwritten;

  if (w->status != 0)
    return;

  if (w->buffer.curr + howmuch > w->buffer.end)
    {
      switch (w->direction)
	{
	case WIRE_ENCODE:
	  nbytes = w->buffer.curr - w->buffer.start;
	  w->buffer.curr = w->buffer.start;
	  while (nbytes > 0)
	    {
	      nwritten = (*w->io.write) (fd, w->buffer.curr, nbytes);
	      if (nwritten < 0)
		{
		  w->status = errno;
		  return;
		}
	      w->buffer.curr += nwritten;
	      nbytes -= nwritten;
	    }
	  w->buffer.curr = w->buffer.start;
	  w->buffer.end = w->buffer.start + w->buffer.size;
	  break;

	case WIRE_DECODE:
	  left_over = w->buffer.end - w->buffer.curr;

	  if ((signed)left_over < 0)
	    return;
	  
	  if (left_over)
	    memmove (w->buffer.start, w->buffer.curr, left_over);
	  w->buffer.curr = w->buffer.start;
	  w->buffer.end = w->buffer.start + left_over;

	  do {
	      nread = (*w->io.read) (fd, w->buffer.end,
				     w->buffer.size - left_over);
	      if (nread <= 0)
		{
		  if (nread == 0)
		    errno = EINVAL;
		  w->status = errno;
		  return;
		}
	      left_over += nread;
	      w->buffer.end += nread;
	  } while (left_over < howmuch);
	  break;

	case WIRE_FREE:
	  break;
	}
    }
}

void
sanei_w_void (Wire *w)
{
}

void
sanei_w_array (Wire *w, SANE_Word *len_ptr, void **v, WireCodecFunc w_element,
	       size_t element_size)
{
  SANE_Word len;
  char *val;
  int i;

  if (w->direction == WIRE_FREE)
    {
      if (*len_ptr && *v)
	{
	  val = *v;
	  for (i = 0; i < *len_ptr; ++i)
	    {
	      (*w_element) (w, val);
	      val += element_size;
	    }
	  free (*v);
	}
      return;
    }

  if (w->direction == WIRE_ENCODE)
    len = *len_ptr;

  sanei_w_word (w, &len);

  if (w->direction == WIRE_DECODE)
    {
      *len_ptr = len;
      if (len)
	{
	  *v = malloc (len * element_size);
	  if (*v == 0)
	    {
	      /* Malloc failed, so return an error. */
	      w->status = ENOMEM;
	      return;
	    }
	}
      else
	*v = 0;
    }

  val = *v;
  for (i = 0; i < len; ++i)
    {
      (*w_element) (w, val);
      val += element_size;
    }
}

void
sanei_w_ptr (Wire *w, void **v, WireCodecFunc w_value, size_t value_size)
{
  SANE_Word is_null;

  if (w->direction == WIRE_FREE)
    {
      if (*v && value_size)
	{
	  (*w_value) (w, *v);
	  free (*v);
	}
      return;
    }
  if (w->direction == WIRE_ENCODE)
    is_null = (*v == 0);

  sanei_w_word (w, &is_null);

  if (!is_null)
    {
      if (w->direction == WIRE_DECODE)
	{
	  *v = malloc (value_size);
	  if (*v == 0)
	    {
	      /* Malloc failed, so return an error. */
	      w->status = ENOMEM;
	      return;
	    }
	}
      (*w_value) (w, *v);
    }
  else if (w->direction == WIRE_DECODE)
    *v = 0;
}

void
sanei_w_byte (Wire *w, SANE_Byte *v)
{
  (*w->codec.w_byte) (w, v);
}

void
sanei_w_char (Wire *w, SANE_Char *v)
{
  (*w->codec.w_char) (w, v);
}

void
sanei_w_word (Wire *w, SANE_Word *v)
{
  (*w->codec.w_word) (w, v);
}

void
sanei_w_string (Wire *w, SANE_String *v)
{
  (*w->codec.w_string) (w, v);
}

void
sanei_w_status (Wire *w, SANE_Status *v)
{
  SANE_Word word = *v;

  sanei_w_word (w, &word);
  if (w->direction == WIRE_DECODE)
    *v = word;
}

void
sanei_w_bool (Wire *w, SANE_Bool *v)
{
  SANE_Word word = *v;

  sanei_w_word (w, &word);
  if (w->direction == WIRE_DECODE)
    *v = word;
}

void
sanei_w_constraint_type (Wire *w, SANE_Constraint_Type *v)
{
  SANE_Word word = *v;

  sanei_w_word (w, &word);
  if (w->direction == WIRE_DECODE)
    *v = word;
}

void
sanei_w_value_type (Wire *w, SANE_Value_Type *v)
{
  SANE_Word word = *v;

  sanei_w_word (w, &word);
  if (w->direction == WIRE_DECODE)
    *v = word;
}

void
sanei_w_unit (Wire *w, SANE_Unit *v)
{
  SANE_Word word = *v;

  sanei_w_word (w, &word);
  if (w->direction == WIRE_DECODE)
    *v = word;
}

void
sanei_w_action (Wire *w, SANE_Action *v)
{
  SANE_Word word = *v;

  sanei_w_word (w, &word);
  if (w->direction == WIRE_DECODE)
    *v = word;
}

void
sanei_w_frame (Wire *w, SANE_Frame *v)
{
  SANE_Word word = *v;

  sanei_w_word (w, &word);
  if (w->direction == WIRE_DECODE)
    *v = word;
}

void
sanei_w_range (Wire *w, SANE_Range *v)
{
  sanei_w_word (w, &v->min);
  sanei_w_word (w, &v->max);
  sanei_w_word (w, &v->quant);
}

void
sanei_w_device (Wire *w, SANE_Device *v)
{
  sanei_w_string (w, (SANE_String *) &v->name);
  sanei_w_string (w, (SANE_String *) &v->vendor);
  sanei_w_string (w, (SANE_String *) &v->model);
  sanei_w_string (w, (SANE_String *) &v->type);
}

void
sanei_w_device_ptr (Wire *w, SANE_Device **v)
{
  sanei_w_ptr (w, (void **) v,
	       (WireCodecFunc) sanei_w_device, sizeof (**v));
}

void
sanei_w_option_descriptor (Wire *w, SANE_Option_Descriptor *v)
{
  SANE_Word len;

  sanei_w_string (w, (SANE_String *) &v->name);
  sanei_w_string (w, (SANE_String *) &v->title);
  sanei_w_string (w, (SANE_String *) &v->desc);
  sanei_w_value_type (w, &v->type);
  sanei_w_unit (w, &v->unit);
  sanei_w_word (w, &v->size);
  sanei_w_word (w, &v->cap);
  sanei_w_constraint_type (w, &v->constraint_type);
  
  switch (v->constraint_type)
    {
    case SANE_CONSTRAINT_NONE:
      break;

    case SANE_CONSTRAINT_RANGE:
      sanei_w_ptr (w, (void **) &v->constraint.range,
		   (WireCodecFunc) sanei_w_range, sizeof (SANE_Range));
      break;

    case SANE_CONSTRAINT_WORD_LIST:
      if (w->direction != WIRE_DECODE)
	len = v->constraint.word_list[0] + 1;
      sanei_w_array (w, &len, (void **) &v->constraint.word_list,
		     w->codec.w_word, sizeof(SANE_Word));
      break;

    case SANE_CONSTRAINT_STRING_LIST:
      if (w->direction != WIRE_DECODE)
	{
	  for (len = 0; v->constraint.string_list[len]; ++len);
	  ++len;	/* send NULL string, too */
	}
      sanei_w_array (w, &len, (void **) &v->constraint.string_list,
		     w->codec.w_string, sizeof(SANE_String));
      break;
    }
}

void
sanei_w_option_descriptor_ptr (Wire *w, SANE_Option_Descriptor **v)
{
  sanei_w_ptr (w, (void **) v,
	       (WireCodecFunc) sanei_w_option_descriptor, sizeof (**v));
}

void
sanei_w_parameters (Wire *w, SANE_Parameters *v)
{
  sanei_w_frame (w, &v->format);
  sanei_w_bool (w, &v->last_frame);
  sanei_w_word (w, &v->bytes_per_line);
  sanei_w_word (w, &v->pixels_per_line);
  sanei_w_word (w, &v->lines);
  sanei_w_word (w, &v->depth);
}

static void
flush (Wire *w)
{
  if (w->direction == WIRE_ENCODE)
    sanei_w_space (w, w->buffer.size + 1);
  else if (w->direction == WIRE_DECODE)
    w->buffer.curr = w->buffer.end = w->buffer.start;
}

void
sanei_w_set_dir (Wire *w, WireDirection dir)
{
  flush (w);
  w->direction = dir;
  flush (w);
}

void
sanei_w_call (Wire *w,
	      SANE_Word procnum,
	      WireCodecFunc w_arg, void *arg,
	      WireCodecFunc w_reply, void *reply)
{
  w->status = 0;
  sanei_w_set_dir (w, WIRE_ENCODE);

  sanei_w_word (w, &procnum);
  (*w_arg) (w, arg);

  if (w->status == 0)
    {
      sanei_w_set_dir (w, WIRE_DECODE);
      (*w_reply) (w, reply);
    }
}

void
sanei_w_reply (Wire *w, WireCodecFunc w_reply, void *reply)
{
  w->status = 0;
  sanei_w_set_dir (w, WIRE_ENCODE);
  (*w_reply) (w, reply);
  flush (w);
}

void
sanei_w_free (Wire *w, WireCodecFunc w_reply, void *reply)
{
  WireDirection saved_dir = w->direction;

  w->direction = WIRE_FREE;
  (*w_reply) (w, reply);
  w->direction = saved_dir;
}

void
sanei_w_init (Wire *w, void (*codec_init_func)(Wire *))
{
  w->status = 0;
  w->direction = WIRE_ENCODE;
  w->buffer.size = 8192;
  w->buffer.start = malloc (w->buffer.size);

  if (w->buffer.start == 0)
    /* Malloc failed, so return an error. */
    w->status = ENOMEM;

  w->buffer.curr = w->buffer.start;
  w->buffer.end = w->buffer.start + w->buffer.size;
  if (codec_init_func != 0)
    (*codec_init_func) (w);
}

void
sanei_w_exit (Wire *w)
{
  if (w->buffer.start)
    free(w->buffer.start);
  w->buffer.start = 0;
  w->buffer.size = 0;
}
