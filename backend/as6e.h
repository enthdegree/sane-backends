/***************************************************************************
                          as6e.h  -  description
                             -------------------
    begin                : Mon Feb 21 2000
    copyright            : (C) 2000 by Eugene Weiss
    email                : eweiss@sas.upenn.edu
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include <sys/stat.h>
#include <sys/types.h>
#include <sane/sane.h>


typedef union
{
	SANE_Word	w;										     /* word */
	SANE_Word	*wa;									       /* word array */
	SANE_String	s;										   /* string */
} Option_Value;

typedef enum
{
	OPT_NUM_OPTS = 0,
	OPT_MODE,
	OPT_RESOLUTION,

	OPT_TL_X,			/* top-left x */
	OPT_TL_Y,			/* top-left y */
	OPT_BR_X,			/* bottom-right x */
	OPT_BR_Y,			/* bottom-right y */

	OPT_BRIGHTNESS,
	OPT_CONTRAST,

	/* must come last */
	NUM_OPTIONS
  } AS6E_Option;

typedef struct
{
	int color;
	int resolution;
	int startpos;
	int stoppos;
	int startline;
	int stopline;
	int ctloutpipe;
	int ctlinpipe;
	int datapipe;
} AS6E_Params;


typedef struct AS6E_Device
{
	struct AS6E_Device *next;
	SANE_Device sane;
} AS6E_Device;



typedef struct AS6E_Scan
{
	struct AS6E_Scan *next;
	SANE_Option_Descriptor options_list[NUM_OPTIONS];
	Option_Value value[NUM_OPTIONS];
	SANE_Bool scanning;
	SANE_Bool cancelled;
	SANE_Parameters sane_params;
	AS6E_Params	as6e_params;
	pid_t child_pid;
	size_t bytes_to_read;
	SANE_Byte *scan_buffer;
	SANE_Byte *line_buffer;
	SANE_Word scan_buffer_count;
	SANE_Word image_counter;
} AS6E_Scan;


#ifndef PATH_MAX
#define PATH_MAX	1024
#endif

#define AS6E_CONFIG_FILE "as6e.conf"

#define READPIPE 0
#define WRITEPIPE 1

#define MM_PER_INCH 25.4

#define SCAN_BUF_SIZE 32768
