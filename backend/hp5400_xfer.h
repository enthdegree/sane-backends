/* sane - Scanner Access Now Easy.
   (c) 2003 Martijn van Oosterhout, kleptog@svana.org
   (c) 2002 Bertrik Sikken, bertrik@zonnet.nl

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

   Transport layer for communication with HP5400/5470 scanner.
   
   Add support for bulk transport of data - 19/02/2003 Martijn
*/

static int hp5400_open (const char *filename);
static void hp5400_close (int iHandle);

static int hp5400_command_verify (int iHandle, int iCmd);
static int hp5400_command_read (int iHandle, int iCmd, int iLen, void *pbData);
static int hp5400_command_read_noverify (int iHandle, int iCmd, int iLen,
					 void *pbData);
static int hp5400_command_write (int iHandle, int iCmd, int iLen, void *pbData);
static void hp5400_command_write_noverify (int fd, int iValue, void *pabData,
					   int iSize);
static int hp5400_bulk_read (int iHandle, int size, int block, FILE * file);
static int hp5400_bulk_read_block (int iHandle, int iCmd, void *cmd, int cmdlen,
				   void *buffer, int len);
static int hp5400_bulk_command_write (int iHandle, int iCmd, void *cmd, int cmdlen,
				      int len, int block, char *data);
static int hp5400_isOn (int iHandle);
