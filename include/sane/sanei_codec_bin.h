/* sane - Scanner Access Now Easy.
   Copyright (C) 1996 David Mosberger-Tang and Andreas Beck
   This file is part of the SANE package.

   SANE is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License, or (at your
   option) any later version.

   SANE is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with sane; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
/** @file sanei_codec_bin.h
 * Binary codec for network transmissions
 *
 * Transtale data to a byte stream while taking byte order problems into
 * account. This codec is currently used for saned and the network backend.
 *
 * @sa sanei_codec_ascii.h sanei_net.h sanei_wire.h
 */

#ifndef sanei_codec_bin_h
#define sanei_codec_bin_h

/** Initialize the binary codec
 *
 * Set the i/o functions of the Wire to those of the binary codec.
 *
 * @param w Wire
 */
extern void sanei_codec_bin_init (Wire *w);

#endif /* sanei_codec_bin_h */
