/* sane - Scanner Access Now Easy.
 
   Copyright (C) 1997, 1998 Franck Schnefra, Michel Roelofs,
   Emmanuel Blot, Mikko Tyolajarvi, David Mosberger-Tang, Wolfgang Goeller,
   Petter Reinholdtsen, Gary Plewa, and Kevin Charter
 
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
 
   This file is a component of the implementation of a backend for many
   of the the AGFA SnapScan and Acer Vuego/Prisa flatbed scanners. */


/* $Id$
   SnapScan backend utility functions */

static inline SnapScan_Mode actual_mode (SnapScan_Scanner *pss)
{
    if (pss->preview == SANE_TRUE)
        return pss->preview_mode;
    return pss->mode;
}


static inline int is_colour_mode (SnapScan_Mode m)
{
    return (m == MD_COLOUR) || (m == MD_BILEVELCOLOUR);
}

/*
 * $Log$
 * Revision 1.3  2000/08/12 15:09:35  pere
 * Merge devel (v1.0.3) into head branch.
 *
 * Revision 1.2.2.1  2000/07/13 04:47:46  pere
 * New snapscan backend version dated 20000514 from Steve Underwood.
 *
 * Revision 1.2.1  2000/05/14 13:30:20  coppice
 * Added history log to pre-existing code.Some reformatting.
 * Actually, this file hardly seems worth splitting out from the rest!
 * */
