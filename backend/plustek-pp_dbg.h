/*.............................................................................
 * Project : linux driver for Plustek parallel-port scanners
 *.............................................................................
 * File:	 plustek-pp_dbg.h - definition of some debug macros
 *.............................................................................
 *
 * Copyright (C) 2000-2003 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 *.............................................................................
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __DEBUG_H__
#define __DEBUG_H__

/* uncomment this to have an SW-simulatet 98001 device - don't expect to scan*/
/* #define _ASIC_98001_SIM */

/*
 * the print macros
 */
#ifdef __KERNEL__
# define _PRINT	printk
#endif

/*
 * some debug definitions
 */
#ifdef DEBUG
# ifndef __KERNEL__
#  include <assert.h>
#  define _ASSERT(x)						assert(x)
# endif

# ifndef DBG
#  define DBG(level, msg, args...)		if ((dbg_level) & (level)) {	\
											_PRINT(msg, ##args);		\
										}
# endif
#else
# define _ASSERT(x)
# ifndef DBG
#  define DBG(level, msg, args...)
# endif
#endif

/* different debug level */
#define DBG_LOW         0x01
#define DBG_MEDIUM      0x02
#define DBG_HIGH        0x04
#define DBG_HELPERS     0x08
#define DBG_TIMEOUT     0x10
#define DBG_SCAN        0x20
#define DBG_IO          0x40
#define DBG_IOF         0x80
#define DBG_ALL         0xFF

/*
 * standard debug level
 */
#ifdef DEBUG
static int dbg_level=(DBG_ALL & ~(DBG_IO | DBG_IOF));
#endif

#endif /* guard __DEBUG_H__ */

/* END PLUSTEK-PP_DBG.H .....................................................*/
