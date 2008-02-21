/* sane - Scanner Access Now Easy.

   Copyright (C) 2003 Oliver Rauch
   Copyright (C) 2003, 2004 Henning Meier-Geinitz <henning@meier-geinitz.de>
   Copyright (C) 2004, 2005 Gerhard Jaeger <gerhard@gjaeger.de>
   Copyright (C) 2004, 2005 Stephane Voltz <stef.dev@free.fr>
   Copyright (C) 2005 Pierre Willenbrock <pierre@pirsoft.dnsalias.org>
   Copyright (C) 2006 Laurent Charpentier <laurent_pubs@yahoo.com>
   Parts of the structs have been taken from the gt68xx backend by
   Sergey Vlasov <vsu@altlinux.ru> et al.
   
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

#ifndef GENESYS_LOW_H
#define GENESYS_LOW_H

#include <stddef.h>
#include "../include/sane/sane.h"

#define DBG_error0      0	/* errors/warnings printed even with devuglevel 0 */
#define DBG_error       1	/* fatal errors */
#define DBG_init        2	/* initialization and scanning time messages */
#define DBG_warn        3	/* warnings and non-fatal errors */
#define DBG_info        4	/* informational messages */
#define DBG_proc        5	/* starting/finishing functions */
#define DBG_io          6	/* io functions */
#define DBG_io2         7	/* io functions that are called very often */
#define DBG_data        8	/* log image data */

#define RIE(function)                                   \
  do { status = function;                               \
    if (status != SANE_STATUS_GOOD) return status;      \
  } while (SANE_FALSE)

#define MM_PER_INCH 25.4

/* Flags */
#define GENESYS_FLAG_UNTESTED     (1 << 0)	/* Print a warning for these scanners */
#define GENESYS_FLAG_14BIT_GAMMA  (1 << 1)	/* use 14bit Gamma table instead of 12 */
#define GENESYS_FLAG_LAZY_INIT    (1 << 2)	/* skip extensive ASIC test at init   */
#define GENESYS_FLAG_USE_PARK     (1 << 3)	/* use genesys_park_head() instead of
						   genesys_slow_back_home()           */
#define GENESYS_FLAG_SKIP_WARMUP  (1 << 4)	/* skip genesys_warmup()              */
#define GENESYS_FLAG_OFFSET_CALIBRATION   (1 << 5)	/* do offset calibration      */
#define GENESYS_FLAG_SEARCH_START (1 << 6)	/* do start search beofre scanning    */
#define GENESYS_FLAG_REPARK       (1 << 7)	/* repark head (and check for lock) by 
						   moving without scanning */
#define GENESYS_FLAG_DARK_CALIBRATION (1 << 8)	/* do dark calibration */
#define GENESYS_FLAG_STAGGERED_LINE   (1 << 9)	/* pixel columns are shifted vertically for hi-res modes */

#define GENESYS_FLAG_MUST_WAIT        (1 << 10)	/* tells wether the scanner 
						   should wait 1 minute after 
						   init before doing anything 
						*/


#define GENESYS_FLAG_ALT_SLOPE_CREATE (1 << 11)	/* use alternative slope
						   creation function */

#define GENESYS_FLAG_DARK_WHITE_CALIBRATION (1 << 12) /*yet another calibration method. does white and dark shading in one run, depending on a black and a white strip*/

/* USB control message values */
#define REQUEST_TYPE_IN		(USB_TYPE_VENDOR | USB_DIR_IN)
#define REQUEST_TYPE_OUT	(USB_TYPE_VENDOR | USB_DIR_OUT)
#define REQUEST_REGISTER	0x0c
#define REQUEST_BUFFER		0x04
#define VALUE_BUFFER		0x82
#define VALUE_SET_REGISTER	0x83
#define VALUE_READ_REGISTER	0x84
#define VALUE_WRITE_REGISTER	0x85
#define VALUE_INIT		0x87
#define GPIO_OUTPUT_ENABLE	0x89
#define GPIO_READ		0x8a
#define GPIO_WRITE		0x8b
#define INDEX			0x00

/* todo: used?
#define VALUE_READ_STATUS	0x86
#define VALUE_BUF_ENDACCESS	0x8C
*/

/* Read/write bulk data/registers */
#define BULK_OUT		0x01
#define BULK_IN			0x00
#define BULK_RAM		0x00
#define BULK_REGISTER		0x11

#define BULKIN_MAXSIZE          0xFE00
#define BULKOUT_MAXSIZE         0xF000

/* AFE values */
#define AFE_INIT       1
#define AFE_SET        2
#define AFE_POWER_SAVE 4

#define LOWORD(x)  ((u_int16_t)(x & 0xffff))
#define HIWORD(x)  ((u_int16_t)(x >> 16))
#define LOBYTE(x)  ((u_int8_t)((x) & 0xFF))
#define HIBYTE(x)  ((u_int8_t)((x) >> 8))

/* Global constants */
/* todo: check if those are the same for every scanner */
#define SYSTEM_CLOCK		32	/* todo: ? */
#define MOTOR_SPEED_MAX		350
													      /*#define MOTOR_GEAR    *//*600 1200 * todo: base y res? --> model */
#define PIXEL_TIME		((double) 24 / SYSTEM_CLOCK)
#define DARK_VALUE		0


typedef struct
{
  SANE_Byte address;
  SANE_Byte value;
} Genesys_Register_Set;

typedef struct
{
  u_int8_t reg[4];
  u_int8_t sign[3];
  u_int8_t offset[3];
  u_int8_t gain[3];
  u_int8_t reg2[3];
} Genesys_Frontend;

typedef struct
{
  int optical_res;
  int black_pixels;
  int dummy_pixel;              /* value of dummy register. */
  int CCD_start_xoffset;	/* last pixel of CCD margin at optical resolution */
  int sensor_pixels;		/* total pixels used by the sensor */
  int fau_gain_white_ref;	/* TA CCD target code (reference gain) */
  int gain_white_ref;		/* CCD target code (reference gain) */
  u_int8_t regs_0x08_0x0b[4];
  u_int8_t regs_0x10_0x1d[14];
  u_int8_t regs_0x52_0x5e[13];
  float red_gamma;
  float green_gamma;
  float blue_gamma;
  u_int16_t *red_gamma_table;
  u_int16_t *green_gamma_table;
  u_int16_t *blue_gamma_table;
} Genesys_Sensor;

typedef struct
{
  u_int8_t value[2];
  u_int8_t enable[2];
} Genesys_Gpo;

typedef struct
{
  SANE_Int maximum_start_speed; /* maximum speed allowed when accelerating from standstill. Unit: pixeltime/step */
  SANE_Int maximum_speed;       /* maximum speed allowed. Unit: pixeltime/step */
  SANE_Int minimum_steps;       /* number of steps used for default curve */
  float g;                      /* power for non-linear acceleration curves. */
/* vs*(1-i^g)+ve*(i^g) where 
   vs = start speed, ve = end speed, 
   i = 0.0 for first entry and i = 1.0 for last entry in default table*/
} Genesys_Motor_Slope;


typedef struct
{
  SANE_Int base_ydpi;		 /* motor base steps. Unit: 1/" */
  SANE_Int optical_ydpi;	 /* maximum resolution in y-direction. Unit: 1/"  */
  SANE_Int max_step_type;        /* maximum step type. 0-2 */
  SANE_Int power_mode_count;        /* number of power modes*/
  Genesys_Motor_Slope slopes[2][3]; /* slopes to derive individual slopes from */
} Genesys_Motor;

typedef enum Genesys_Color_Order
{
  COLOR_ORDER_RGB,
  COLOR_ORDER_BGR
}
Genesys_Color_Order;


#define MAX_SCANNERS 30
#define MAX_RESOLUTIONS 13
#define MAX_DPI 4

#define GENESYS_GL646	 646
#define GENESYS_GL841	 841

/*135 registers for gl841 + 1 null-reg*/
#define GENESYS_MAX_REGS 136

#define DAC_WOLFSON_UMAX 0
#define DAC_WOLFSON_ST12 1
#define DAC_WOLFSON_ST24 2
#define DAC_WOLFSON_5345 3
#define DAC_WOLFSON_HP2400 4
#define DAC_WOLFSON_HP2300 5
#define DAC_CANONLIDE35  6

#define CCD_UMAX         0
#define CCD_ST12         1	/* SONY ILX548: 5340 Pixel  ??? */
#define CCD_ST24         2	/* SONY ILX569: 10680 Pixel ??? */
#define CCD_5345         3
#define CCD_HP2400       4
#define CCD_HP2300       5
#define CCD_CANONLIDE35  6

#define GPO_UMAX         0
#define GPO_ST12         1
#define GPO_ST24         2
#define GPO_5345         3
#define GPO_HP2400       4
#define GPO_HP2300       5
#define GPO_CANONLIDE35  6

#define MOTOR_UMAX       0
#define MOTOR_5345       1
#define MOTOR_ST24       2
#define MOTOR_HP2400     3
#define MOTOR_HP2300     4
#define MOTOR_CANONLIDE35 5


/* Forward typedefs */
typedef struct Genesys_Device Genesys_Device;

/**
 * Scanner command set description.
 *
 * This description contains parts which are common to all scanners with the
 * same command set, but may have different optical resolution and other
 * parameters.
 */
typedef struct Genesys_Command_Set
{
  /** @name Identification */
  /*@{ */

  /** Name of this command set */
  SANE_String_Const name;

  /*@} */

  /** For ASIC initialization */
    SANE_Status (*init) (Genesys_Device * dev);

    SANE_Status (*init_regs_for_warmup) (Genesys_Device * dev,
					 Genesys_Register_Set * regs,
					 int *channels, int *total_size);
    SANE_Status (*init_regs_for_coarse_calibration) (Genesys_Device * dev);
    SANE_Status (*init_regs_for_shading) (Genesys_Device * dev);
    SANE_Status (*init_regs_for_scan) (Genesys_Device * dev);

    SANE_Bool (*get_filter_bit) (Genesys_Register_Set * reg);
    SANE_Bool (*get_lineart_bit) (Genesys_Register_Set * reg);
    SANE_Bool (*get_bitset_bit) (Genesys_Register_Set * reg);
    SANE_Bool (*get_gain4_bit) (Genesys_Register_Set * reg);
    SANE_Bool (*get_fast_feed_bit) (Genesys_Register_Set * reg);

    SANE_Bool (*test_buffer_empty_bit) (SANE_Byte val);
    SANE_Bool (*test_motor_flag_bit) (SANE_Byte val);

  int (*bulk_full_size) (void);

    SANE_Status (*set_fe) (Genesys_Device * dev, u_int8_t set);
    SANE_Status (*set_powersaving) (Genesys_Device * dev, int delay);
    SANE_Status (*save_power) (Genesys_Device * dev, SANE_Bool enable);

  void (*set_motor_power) (Genesys_Register_Set * regs, SANE_Bool set);
  void (*set_lamp_power) (Genesys_Device * dev, 
			  Genesys_Register_Set * regs, 
			  SANE_Bool set);

    SANE_Status (*begin_scan) (Genesys_Device * dev,
			       Genesys_Register_Set * regs,
			       SANE_Bool start_motor);
    SANE_Status (*end_scan) (Genesys_Device * dev,
			     Genesys_Register_Set * regs,
			     SANE_Bool check_stop);

    SANE_Status (*send_gamma_table) (Genesys_Device * dev, SANE_Bool generic);

    SANE_Status (*search_start_position) (Genesys_Device * dev);
    SANE_Status (*offset_calibration) (Genesys_Device * dev);
    SANE_Status (*coarse_gain_calibration) (Genesys_Device * dev, int dpi);
    SANE_Status (*led_calibration) (Genesys_Device * dev);

    SANE_Status (*slow_back_home) (Genesys_Device * dev,
				   SANE_Bool wait_until_home);
    SANE_Status (*park_head) (Genesys_Device * dev,
			      Genesys_Register_Set * reg,
			      SANE_Bool wait_until_home);
    SANE_Status (*bulk_write_register) (Genesys_Device * dev,
					Genesys_Register_Set * reg, 
					size_t elems);
    SANE_Status (*bulk_write_data) (Genesys_Device * dev, u_int8_t addr, 
				    u_int8_t * data, size_t len);

    SANE_Status (*bulk_read_data) (Genesys_Device * dev, u_int8_t addr,
				   u_int8_t * data, size_t len);

} Genesys_Command_Set;

typedef struct Genesys_Model
{
  SANE_String_Const name;
  SANE_String_Const vendor;
  SANE_String_Const model;

  SANE_Int asic_type;		/* ASIC type gl646 or gl841 */
  Genesys_Command_Set *cmd_set;	/* pointers to low level functions */

  SANE_Int xdpi_values[MAX_RESOLUTIONS];	/* possible x resolutions */
  SANE_Int ydpi_values[MAX_RESOLUTIONS];	/* possible y resolutions */
  SANE_Int bpp_gray_values[MAX_DPI];	/* possible depths in gray mode */
  SANE_Int bpp_color_values[MAX_DPI];	/* possible depths in color mode */

  SANE_Fixed x_offset;		/* Start of scan area in mm */
  SANE_Fixed y_offset;		/* Start of scan area in mm */
  SANE_Fixed x_size;		/* Size of scan area in mm */
  SANE_Fixed y_size;		/* Size of scan area in mm */

  SANE_Fixed y_offset_calib;	/* Start of white strip in mm */
  SANE_Fixed x_offset_mark;	/* Start of black mark in mm */

  SANE_Fixed x_offset_ta;	/* Start of scan area in TA mode in mm */
  SANE_Fixed y_offset_ta;	/* Start of scan area in TA mode in mm */
  SANE_Fixed x_size_ta;		/* Size of scan area in TA mode in mm */
  SANE_Fixed y_size_ta;		/* Size of scan area in TA mode in mm */

  SANE_Fixed y_offset_calib_ta;	/* Start of white strip in TA mode in mm */

  /* Line-distance correction (in pixel at optical_ydpi) for CCD scanners */
  SANE_Int ld_shift_r;		/* red */
  SANE_Int ld_shift_g;		/* green */
  SANE_Int ld_shift_b;		/* blue */

  Genesys_Color_Order line_mode_color_order;	/* Order of the CCD/CIS colors */

  SANE_Bool is_cis;		/* Is this a CIS or CCD scanner? */

  SANE_Int ccd_type;		/* which SENSOR type do we have ? */
  SANE_Int dac_type;		/* which DAC do we have ? */
  SANE_Int gpo_type;		/* General purpose output type */
  SANE_Int motor_type;		/* stepper motor type */
  SANE_Word flags;		/* Which hacks are needed for this scanner? */
  /*@} */
  SANE_Int shading_lines;	/* how many lines are used for shading calibration */
  SANE_Int search_lines;	/* how many lines are used to search start position */
} Genesys_Model;


typedef struct
{
  int scan_method;		/* todo: change >=2: Transparency, 0x88: negative film */
  int scan_mode;		/* todo: change 0,1 = lineart, halftone; 2 = gray, 3 = 3pass color, 4=single pass color */
  int xres;			/* dpi */
  int yres;			/* dpi */

  double tl_x;			/* x start on scan table in mm */
  double tl_y;			/* y start on scan table in mm */

  unsigned int lines;		/* number of lines at scan resolution */
  unsigned int pixels;		/* number of pixels at scan resolution */

  unsigned int depth;/* bit depth of the scan */

  /* todo : remove these fields ? */
  int exposure_time;

  unsigned int color_filter;	/* todo: check, may be make it an advanced option */

  /* BW threshold */
  int threshold;

  /* Disable interpolation for xres<yres*/
  int disable_interpolation;
} Genesys_Settings;

typedef struct Genesys_Current_Setup
{
    int pixels;         /* pixel count expected from scanner */
    int lines;          /* line count expected from scanner */
    int depth;          /* depth expected from scanner */
    int channels;       /* channel count expected from scanner */
    int exposure_time;  /* used exposure time */
    float xres;         /* used xres */
    float yres;         /* used yres*/
    SANE_Bool half_ccd; /* half ccd mode */
    SANE_Int stagger;		
    SANE_Int max_shift;	/* max shift of any ccd component, including staggered pixels*/
} Genesys_Current_Setup;

typedef struct Genesys_Buffer
{
  SANE_Byte *buffer;
  size_t size;
  size_t pos;	/* current position in read buffer */
  size_t avail;	/* data bytes currently in buffer */
} Genesys_Buffer;

struct Genesys_Device
{
  SANE_Int dn;
  SANE_String file_name;
  Genesys_Model *model;

  Genesys_Register_Set reg[GENESYS_MAX_REGS];
  Genesys_Register_Set calib_reg[GENESYS_MAX_REGS];
  Genesys_Settings settings;
  Genesys_Frontend frontend;
  Genesys_Sensor sensor;
  Genesys_Gpo gpo;
  Genesys_Motor motor;
  u_int16_t slope_table0[256];
  u_int16_t slope_table1[256];
  time_t init_date;

  u_int8_t *white_average_data;
  u_int8_t *dark_average_data;
  u_int16_t dark[3];

  SANE_Bool already_initialized;
  SANE_Int scanhead_position_in_steps;
  SANE_Int lamp_off_time;

  SANE_Bool read_active;

  Genesys_Buffer read_buffer;
  Genesys_Buffer lines_buffer;
  Genesys_Buffer shrink_buffer;
  Genesys_Buffer out_buffer;

  size_t read_bytes_left;	/* bytes to read from scanner */

  size_t total_bytes_read;	/* total bytes read sent to frontend */
  size_t total_bytes_to_read;	/* total bytes read to be sent to frontend */

  Genesys_Current_Setup current_setup; /* contains the real used values */

  struct Genesys_Device *next;
};

typedef struct Genesys_USB_Device_Entry
{
  SANE_Word vendor;			/**< USB vendor identifier */
  SANE_Word product;			/**< USB product identifier */
  Genesys_Model *model;			/**< Scanner model information */
} Genesys_USB_Device_Entry;

/*--------------------------------------------------------------------------*/
/*       common functions needed by low level specific functions            */
/*--------------------------------------------------------------------------*/

extern Genesys_Register_Set *sanei_genesys_get_address (Genesys_Register_Set *
							regs, SANE_Byte addr);

extern SANE_Byte
sanei_genesys_read_reg_from_set (Genesys_Register_Set * regs,
				 SANE_Byte address);

extern void
sanei_genesys_set_reg_from_set (Genesys_Register_Set * regs,
				SANE_Byte address, SANE_Byte value);

extern SANE_Status
sanei_genesys_read_register (Genesys_Device * dev, u_int8_t reg,
			     u_int8_t * val);

extern SANE_Status
sanei_genesys_write_register (Genesys_Device * dev, u_int8_t reg,
			      u_int8_t val);

extern SANE_Status
sanei_genesys_get_status (Genesys_Device * dev, u_int8_t * status);

extern void sanei_genesys_init_fe (Genesys_Device * dev);

extern void sanei_genesys_init_structs (Genesys_Device * dev);

extern SANE_Status
sanei_genesys_init_shading_data (Genesys_Device * dev, int pixels_per_line);

extern SANE_Status sanei_genesys_read_feed_steps (Genesys_Device * dev,
						  unsigned int *steps);

extern void
sanei_genesys_calculate_zmode2 (SANE_Bool two_table,
				u_int32_t exposure_time,
				u_int16_t * slope_table,
				int reg21,
				int move, int reg22, u_int32_t * z1,
				u_int32_t * z2);

extern void
sanei_genesys_calculate_zmode (Genesys_Device * dev,
			       u_int32_t exposure_time,
			       u_int32_t steps_sum,
			       u_int16_t last_speed, u_int32_t feedl,
			       u_int8_t fastfed, u_int8_t scanfed,
			       u_int8_t fwdstep, u_int8_t tgtime,
			       u_int32_t * z1, u_int32_t * z2);

extern SANE_Status
sanei_genesys_set_buffer_address (Genesys_Device * dev, u_int32_t addr);

extern SANE_Status
sanei_genesys_fe_write_data (Genesys_Device * dev, u_int8_t addr,
			     u_int16_t data);

extern SANE_Int
sanei_genesys_exposure_time2 (Genesys_Device * dev,
			      float ydpi, int step_type, int endpixel,
			      int led_exposure, int power_mode);

extern SANE_Int
sanei_genesys_exposure_time (Genesys_Device * dev, Genesys_Register_Set * reg,
			     int xdpi);

extern SANE_Int
sanei_genesys_create_slope_table (Genesys_Device * dev,
				  u_int16_t * slope_table, int steps,
				  int step_type, int exposure_time,
				  SANE_Bool same_speed, double yres,
				  int power_mode);

SANE_Int
sanei_genesys_create_slope_table3 (Genesys_Device * dev,
				   u_int16_t * slope_table, int max_step,
				   unsigned int use_steps,
				   int step_type, int exposure_time,
				   double yres,
				   unsigned int *used_steps,
				   unsigned int *final_exposure,
				   int power_mode);

extern void
sanei_genesys_create_gamma_table (u_int16_t * gamma_table, int size,
				  float maximum, float gamma_max,
				  float gamma);

extern SANE_Status sanei_genesys_start_motor (Genesys_Device * dev);

extern SANE_Status sanei_genesys_stop_motor (Genesys_Device * dev);

extern SANE_Status
sanei_genesys_search_reference_point (Genesys_Device * dev, u_int8_t * data,
				      int start_pixel, int dpi, int width,
				      int height);

extern SANE_Status
sanei_genesys_write_pnm_file (char *filename, u_int8_t * data, int depth,
			      int channels, int pixels_per_line, int lines);

extern SANE_Status
sanei_genesys_test_buffer_empty (Genesys_Device * dev, SANE_Bool * empty);

extern SANE_Status
sanei_genesys_read_data_from_scanner (Genesys_Device * dev, u_int8_t * data,
				      size_t size);

extern SANE_Status
sanei_genesys_buffer_alloc(Genesys_Buffer * buf, size_t size);

extern SANE_Status
sanei_genesys_buffer_free(Genesys_Buffer * buf);

extern SANE_Byte *
sanei_genesys_buffer_get_write_pos(Genesys_Buffer * buf, size_t size);

extern SANE_Byte *
sanei_genesys_buffer_get_read_pos(Genesys_Buffer * buf);

extern SANE_Status
sanei_genesys_buffer_produce(Genesys_Buffer * buf, size_t size);

extern SANE_Status
sanei_genesys_buffer_consume(Genesys_Buffer * buf, size_t size);


/*---------------------------------------------------------------------------*/
/*                ASIC specific functions declarations                       */
/*---------------------------------------------------------------------------*/
extern SANE_Status sanei_gl646_init_cmd_set (Genesys_Device * dev);
extern SANE_Status sanei_gl841_init_cmd_set (Genesys_Device * dev);

#endif /* not GENESYS_LOW_H */
