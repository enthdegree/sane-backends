/* Copyright (C) 1997 Free Software Foundation, Inc.
This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, see <https://www.gnu.org/licenses/>.  */

#include "../include/sane/config.h"

#ifndef HAVE_STRNDUP

#include <stdlib.h>
#include <string.h>

#include <sys/types.h>

char *
strndup (const char * s, size_t n)
{
  char *clone;

  clone = malloc (n + 1);
  strncpy (clone, s, n);
  clone[n] = '\0';
  return clone;
}

#endif /* !HAVE_STRNDUP */
