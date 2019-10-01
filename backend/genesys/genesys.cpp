/* sane - Scanner Access Now Easy.

   Copyright (C) 2003, 2004 Henning Meier-Geinitz <henning@meier-geinitz.de>
   Copyright (C) 2004, 2005 Gerhard Jaeger <gerhard@gjaeger.de>
   Copyright (C) 2004-2016 Stéphane Voltz <stef.dev@free.fr>
   Copyright (C) 2005-2009 Pierre Willenbrock <pierre@pirsoft.dnsalias.org>
   Copyright (C) 2006 Laurent Charpentier <laurent_pubs@yahoo.com>
   Copyright (C) 2007 Luke <iceyfor@gmail.com>
   Copyright (C) 2010 Chris Berry <s0457957@sms.ed.ac.uk> and Michael Rickmann <mrickma@gwdg.de>
                 for Plustek Opticbook 3600 support

   Dynamic rasterization code was taken from the epjistsu backend by
   m. allan noah <kitno455 at gmail dot com>

   Software processing for deskew, crop and dspeckle are inspired by allan's
   noah work in the fujitsu backend

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

/*
 * SANE backend for Genesys Logic GL646/GL841/GL842/GL843/GL846/GL847/GL124 based scanners
 */

#define DEBUG_NOT_STATIC

#include "genesys.h"
#include "conv.h"
#include "sanei.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_magic.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <list>
#include <numeric>
#include <exception>
#include <vector>

// Data that we allocate to back SANE_Device objects in s_sane_devices
struct SANE_Device_Data
{
    std::string name;
};

namespace {
    StaticInit<std::list<Genesys_Scanner>> s_scanners;
    StaticInit<std::vector<SANE_Device>> s_sane_devices;
    StaticInit<std::vector<SANE_Device_Data>> s_sane_devices_data;
    StaticInit<std::vector<SANE_Device*>> s_sane_devices_ptrs;
    StaticInit<std::list<Genesys_Device>> s_devices;
} // namespace

static SANE_String_Const mode_list[] = {
  SANE_VALUE_SCAN_MODE_COLOR,
  SANE_VALUE_SCAN_MODE_GRAY,
  /* SANE_TITLE_HALFTONE,  currently unused */
  SANE_VALUE_SCAN_MODE_LINEART,
    nullptr
};

static SANE_String_Const color_filter_list[] = {
  SANE_I18N ("Red"),
  SANE_I18N ("Green"),
  SANE_I18N ("Blue"),
    nullptr
};

static SANE_String_Const cis_color_filter_list[] = {
  SANE_I18N ("Red"),
  SANE_I18N ("Green"),
  SANE_I18N ("Blue"),
  SANE_I18N ("None"),
    nullptr
};

static SANE_String_Const source_list[] = {
  SANE_I18N (STR_FLATBED),
  SANE_I18N (STR_TRANSPARENCY_ADAPTER),
    nullptr
};

static const char* source_list_infrared[] = {
    SANE_I18N(STR_FLATBED),
    SANE_I18N(STR_TRANSPARENCY_ADAPTER),
    SANE_I18N(STR_TRANSPARENCY_ADAPTER_INFRARED),
    nullptr
};

static SANE_Range swdespeck_range = {
  1,
  9,
  1
};

static SANE_Range time_range = {
  0,				/* minimum */
  60,				/* maximum */
  0				/* quantization */
};

static const SANE_Range u12_range = {
  0,				/* minimum */
  4095,				/* maximum */
  0				/* quantization */
};

static const SANE_Range u14_range = {
  0,				/* minimum */
  16383,			/* maximum */
  0				/* quantization */
};

static const SANE_Range u16_range = {
  0,				/* minimum */
  65535,			/* maximum */
  0				/* quantization */
};

static const SANE_Range percentage_range = {
  SANE_FIX (0),			/* minimum */
  SANE_FIX (100),		/* maximum */
  SANE_FIX (1)			/* quantization */
};

static const SANE_Range threshold_curve_range = {
  0,			/* minimum */
  127,		        /* maximum */
  1			/* quantization */
};

/**
 * range for brightness and contrast
 */
static const SANE_Range enhance_range = {
  -100,	/* minimum */
  100,		/* maximum */
  1		/* quantization */
};

/**
 * range for expiration time
 */
static const SANE_Range expiration_range = {
  -1,	        /* minimum */
  30000,	/* maximum */
  1		/* quantization */
};

const Genesys_Sensor& sanei_genesys_find_sensor_any(Genesys_Device* dev)
{
    for (const auto& sensor : *s_sensors) {
        if (dev->model->sensor_id == sensor.sensor_id) {
            return sensor;
        }
    }
    throw std::runtime_error("Given device does not have sensor defined");
}

Genesys_Sensor* find_sensor_impl(Genesys_Device* dev, unsigned dpi, unsigned channels,
                                 ScanMethod scan_method)
{
    for (auto& sensor : *s_sensors) {
        if (dev->model->sensor_id == sensor.sensor_id && sensor.resolutions.matches(dpi) &&
            sensor.matches_channel_count(channels) && sensor.method == scan_method)
        {
            return &sensor;
        }
    }
    return nullptr;
}

bool sanei_genesys_has_sensor(Genesys_Device* dev, unsigned dpi, unsigned channels,
                              ScanMethod scan_method)
{
    return find_sensor_impl(dev, dpi, channels, scan_method) != nullptr;
}

const Genesys_Sensor& sanei_genesys_find_sensor(Genesys_Device* dev, int dpi, unsigned channels,
                                                ScanMethod scan_method)
{
    const auto* sensor = find_sensor_impl(dev, dpi, channels, scan_method);
    if (sensor)
        return *sensor;
    throw std::runtime_error("Given device does not have sensor defined");
}

Genesys_Sensor& sanei_genesys_find_sensor_for_write(Genesys_Device* dev, int dpi,
                                                    unsigned channels,
                                                    ScanMethod scan_method)
{
    auto* sensor = find_sensor_impl(dev, dpi, channels, scan_method);
    if (sensor)
        return *sensor;
    throw std::runtime_error("Given device does not have sensor defined");
}


std::vector<std::reference_wrapper<const Genesys_Sensor>>
    sanei_genesys_find_sensors_all(Genesys_Device* dev, ScanMethod scan_method)
{
    std::vector<std::reference_wrapper<const Genesys_Sensor>> ret;
    for (const Genesys_Sensor& sensor : sanei_genesys_find_sensors_all_for_write(dev, scan_method)) {
        ret.push_back(sensor);
    }
    return ret;
}

std::vector<std::reference_wrapper<Genesys_Sensor>>
    sanei_genesys_find_sensors_all_for_write(Genesys_Device* dev, ScanMethod scan_method)
{
    std::vector<std::reference_wrapper<Genesys_Sensor>> ret;
    for (auto& sensor : *s_sensors) {
        if (dev->model->sensor_id == sensor.sensor_id && sensor.method == scan_method) {
            ret.push_back(sensor);
        }
    }
    return ret;
}

void
sanei_genesys_init_structs (Genesys_Device * dev)
{
    bool gpo_ok = false;
    bool motor_ok = false;
    bool fe_ok = false;

  /* initialize the GPO data stuff */
    for (const auto& gpo : *s_gpo) {
        if (dev->model->gpio_id == gpo.id) {
            dev->gpo = gpo;
            gpo_ok = true;
            break;
        }
    }

    // initialize the motor data stuff
    for (const auto& motor : *s_motors) {
        if (dev->model->motor_id == motor.id) {
            dev->motor = motor;
            motor_ok = true;
            break;
        }
    }

    for (const auto& frontend : *s_frontends) {
        if (dev->model->adc_id == frontend.id) {
            dev->frontend_initial = frontend;
            dev->frontend = frontend;
            fe_ok = true;
            break;
        }
    }

    if (!motor_ok || !gpo_ok || !fe_ok) {
        throw SaneException("bad description(s) for fe/gpo/motor=%d/%d/%d\n",
                            static_cast<unsigned>(dev->model->sensor_id),
                            static_cast<unsigned>(dev->model->gpio_id),
                            static_cast<unsigned>(dev->model->motor_id));
    }

  /* set up initial line distance shift */
  dev->ld_shift_r = dev->model->ld_shift_r;
  dev->ld_shift_g = dev->model->ld_shift_g;
  dev->ld_shift_b = dev->model->ld_shift_b;
}

/* main function for slope creation */
/**
 * This function generates a slope table using the given slope
 * truncated at the given exposure time or step count, whichever comes first.
 * The reached step time is then stored in final_exposure and used for the rest
 * of the table. The summed time of the acceleration steps is returned, and the
 * number of accerelation steps is put into used_steps.
 *
 * @param slope_table    Table to write to
 * @param max_steps      Size of slope_table in steps
 * @param use_steps      Maximum number of steps to use for acceleration
 * @param stop_at        Minimum step time to use
 * @param vstart         Start step time of default slope
 * @param vend           End step time of default slope
 * @param steps          Step count of default slope
 * @param g              Power for default slope
 * @param used_steps     Final number of steps is stored here
 * @param vfinal         Final step time is stored here
 * @return               Time for acceleration
 * @note  All times in pixel time. Correction for other motor timings is not
 *        done.
 */
SANE_Int sanei_genesys_generate_slope_table(std::vector<uint16_t>& slope_table,
				    unsigned int max_steps,
				    unsigned int use_steps,
                                    uint16_t stop_at,
				    uint16_t vstart,
                                    uint16_t vend,
				    unsigned int steps,
                                    double g,
				    unsigned int *used_steps,
				    unsigned int *vfinal)
{
  double t;
  SANE_Int sum = 0;
  unsigned int i;
  unsigned int c = 0;
  uint16_t t2;
  unsigned int dummy;
  unsigned int _vfinal;
  if (!used_steps)
    used_steps = &dummy;
  if (!vfinal)
    vfinal = &_vfinal;

  DBG(DBG_proc, "%s: table size: %d\n", __func__, max_steps);

  DBG(DBG_proc, "%s: stop at time: %d, use %d steps max\n", __func__, stop_at, use_steps);

  DBG(DBG_proc, "%s: target slope: vstart: %d, vend: %d, steps: %d, g: %g\n", __func__, vstart,
      vend, steps, g);

  sum = 0;
  c = 0;
  *used_steps = 0;

    slope_table.clear();
    slope_table.reserve(max_steps);

  if (use_steps < 1)
    use_steps = 1;

  if (stop_at < vstart)
    {
      t2 = vstart;
      for (i = 0; i < steps && i < use_steps - 1 && i < max_steps; i++, c++)
	{
        t = std::pow(static_cast<double>(i) / static_cast<double>(steps - 1), g);
	  t2 = vstart * (1 - t) + t * vend;
            if (t2 < stop_at) {
                break;
            }
            slope_table.push_back(t2);
	  /* DBG (DBG_io, "slope_table[%3d] = %5d\n", c, t2); */
	  sum += t2;
	}
      if (t2 > stop_at)
	{
	  DBG(DBG_warn, "Can not reach target speed(%d) in %d steps.\n", stop_at, use_steps);
	  DBG(DBG_warn, "Expect image to be distorted. Ignore this if only feeding.\n");
	}
      *vfinal = t2;
      *used_steps += i;
      max_steps -= i;
    }
  else
    *vfinal = stop_at;

  for (i = 0; i < max_steps; i++, c++)
    {
        slope_table.push_back(*vfinal);
      /* DBG (DBG_io, "slope_table[%3d] = %5d\n", c, *vfinal); */
    }

  (*used_steps)++;
  sum += *vfinal;

  DBG(DBG_proc, "%s: returns sum=%d, used %d steps, completed\n", __func__, sum, *used_steps);

  return sum;
}

/* Generate slope table for motor movement */
/**
 * This function generates a slope table using the slope from the motor struct
 * truncated at the given exposure time or step count, whichever comes first.
 * The reached step time is then stored in final_exposure and used for the rest
 * of the table. The summed time of the acceleration steps is returned, and the
 * number of accerelation steps is put into used_steps.
 *
 * @param dev            Device struct
 * @param slope_table    Table to write to
 * @param max_step       Size of slope_table in steps
 * @param use_steps      Maximum number of steps to use for acceleration
 * @param step_type      Generate table for this step_type. 0=>full, 1=>half,
 *                       2=>quarter
 * @param exposure_time  Minimum exposure time of a scan line
 * @param yres           Resolution of a scan line
 * @param used_steps     Final number of steps is stored here
 * @param final_exposure Final step time is stored here
 * @return               Time for acceleration
 * @note  all times in pixel time
 */
SANE_Int sanei_genesys_create_slope_table3(Genesys_Device * dev,
                                           std::vector<uint16_t>& slope_table,
                                   int max_step,
				   unsigned int use_steps,
				   int step_type,
                                   int exposure_time,
				   double yres,
				   unsigned int *used_steps,
                                           unsigned int *final_exposure)
{
  unsigned int sum_time = 0;
  unsigned int vtarget;
  unsigned int vend;
  unsigned int vstart;
  unsigned int vfinal;

    DBG(DBG_proc, "%s: step_type = %d, exposure_time = %d, yres = %g\n", __func__,
        step_type, exposure_time, yres);

  /* final speed */
  vtarget = (exposure_time * yres) / dev->motor.base_ydpi;

    vstart = dev->motor.slopes[step_type].maximum_start_speed;
    vend = dev->motor.slopes[step_type].maximum_speed;

  vtarget >>= step_type;
  if (vtarget > 65535)
    vtarget = 65535;

  vstart >>= step_type;
  if (vstart > 65535)
    vstart = 65535;

  vend >>= step_type;
  if (vend > 65535)
    vend = 65535;

  sum_time = sanei_genesys_generate_slope_table (slope_table,
                                                 max_step,
						 use_steps,
						 vtarget,
						 vstart,
						 vend,
                                                 dev->motor.slopes[step_type].minimum_steps << step_type,
                                                 dev->motor.slopes[step_type].g,
                                                 used_steps,
						 &vfinal);

  if (final_exposure)
    *final_exposure = (vfinal * dev->motor.base_ydpi) / yres;

  DBG(DBG_proc, "%s: returns sum_time=%d, completed\n", __func__, sum_time);

  return sum_time;
}


/* alternate slope table creation function        */
/* the hardcoded values (g and vstart) will go in a motor struct */
SANE_Int genesys_create_slope_table2(Genesys_Device* dev, std::vector<uint16_t>& slope_table,
                                     int steps,
			     int step_type, int exposure_time,
                                    bool same_speed, double yres)
{
  double t, g;
  SANE_Int sum = 0;
  int vstart, vend;
  int i;

  DBG(DBG_proc, "%s: %d steps, step_type = %d, "
      "exposure_time = %d, same_speed = %d, yres = %.2f\n", __func__, steps,
      step_type, exposure_time, same_speed, yres);

  /* start speed */
    if (dev->model->motor_id == MotorId::MD_5345) {
      if (yres < dev->motor.base_ydpi / 6)
	vstart = 2500;
      else
	vstart = 2000;
    }
  else
    {
      if (steps == 2)
	vstart = exposure_time;
      else if (steps == 3)
	vstart = 2 * exposure_time;
      else if (steps == 4)
	vstart = 1.5 * exposure_time;
      else if (steps == 120)
	vstart = 1.81674 * exposure_time;
      else
	vstart = exposure_time;
    }

  /* final speed */
  vend = (exposure_time * yres) / (dev->motor.base_ydpi * (1 << step_type));

  /*
     type=1 : full
     type=2 : half
     type=4 : quarter
     vend * type * base_ydpi / exposure = yres
   */

  /* acceleration */
  switch (steps)
    {
    case 255:
      /* test for special case: fast moving slope */
      /* todo: a 'fast' boolean parameter should be better */
      if (vstart == 2000)
	g = 0.2013;
      else
	g = 0.1677;
      break;
    case 120:
      g = 0.5;
      break;
    case 67:
      g = 0.5;
      break;
    case 64:
      g = 0.2555;
      break;
    case 44:
      g = 0.5;
      break;
    case 4:
      g = 0.5;
      break;
    case 3:
      g = 1;
      break;
    case 2:
      vstart = vend;
      g = 1;
      break;
    default:
      g = 0.2635;
    }

  /* if same speed, no 'g' */
  sum = 0;
  if (same_speed)
    {
      for (i = 0; i < 255; i++)
	{
	  slope_table[i] = vend;
	  sum += slope_table[i];
	  DBG (DBG_io, "slope_table[%3d] = %5d\n", i, slope_table[i]);
	}
    }
  else
    {
      for (i = 0; i < steps; i++)
	{
            t = std::pow(static_cast<double>(i) / static_cast<double>(steps - 1), g);
	  slope_table[i] = vstart * (1 - t) + t * vend;
	  DBG (DBG_io, "slope_table[%3d] = %5d\n", i, slope_table[i]);
	  sum += slope_table[i];
	}
      for (i = steps; i < 255; i++)
	{
	  slope_table[i] = vend;
	  DBG (DBG_io, "slope_table[%3d] = %5d\n", i, slope_table[i]);
	  sum += slope_table[i];
	}
    }

  DBG(DBG_proc, "%s: returns sum=%d, completed\n", __func__, sum);

  return sum;
}

/* Generate slope table for motor movement */
/* todo: check details */
SANE_Int sanei_genesys_create_slope_table(Genesys_Device * dev, std::vector<uint16_t>& slope_table,
                                          int steps, int step_type, int exposure_time,
                                          bool same_speed, double yres)
{
  double t;
  double start_speed;
  double g;
  uint32_t time_period;
  int sum_time = 0;
  int i, divider;
  int same_step;

    if (dev->model->motor_id == MotorId::MD_5345 ||
        dev->model->motor_id == MotorId::HP2300 ||
        dev->model->motor_id == MotorId::HP2400)
    {
        return genesys_create_slope_table2(dev, slope_table, steps, step_type, exposure_time,
                                           same_speed, yres);
    }

  DBG(DBG_proc, "%s: %d steps, step_type = %d, exposure_time = %d, same_speed =%d\n", __func__,
      steps, step_type, exposure_time, same_speed);
  DBG(DBG_proc, "%s: yres = %.2f\n", __func__, yres);

  g = 0.6;
  start_speed = 0.01;
  same_step = 4;
  divider = 1 << step_type;

    time_period = static_cast<std::uint32_t>(yres * exposure_time /
                                             dev->motor.base_ydpi /*MOTOR_GEAR */ );
  if ((time_period < 2000) && (same_speed))
    same_speed = false;

  time_period = time_period / divider;

  if (same_speed)
    {
      for (i = 0; i < steps; i++)
	{
            slope_table[i] = static_cast<std::uint16_t>(time_period);
	  sum_time += time_period;

	  DBG (DBG_io, "slope_table[%d] = %d\n", i, time_period);
	}
      DBG(DBG_info, "%s: returns sum_time=%d, completed\n", __func__, sum_time);
      return sum_time;
    }

  if (time_period > MOTOR_SPEED_MAX * 5)
    {
      g = 1.0;
      start_speed = 0.05;
      same_step = 2;
    }
  else if (time_period > MOTOR_SPEED_MAX * 4)
    {
      g = 0.8;
      start_speed = 0.04;
      same_step = 2;
    }
  else if (time_period > MOTOR_SPEED_MAX * 3)
    {
      g = 0.7;
      start_speed = 0.03;
      same_step = 2;
    }
  else if (time_period > MOTOR_SPEED_MAX * 2)
    {
      g = 0.6;
      start_speed = 0.02;
      same_step = 3;
    }

    if (dev->model->motor_id == MotorId::ST24) {
      steps = 255;
        switch (static_cast<int>(yres))
	{
	case 2400:
	  g = 0.1672;
	  start_speed = 1.09;
	  break;
	case 1200:
	  g = 1;
	  start_speed = 6.4;
	  break;
	case 600:
	  g = 0.1672;
	  start_speed = 1.09;
	  break;
	case 400:
	  g = 0.2005;
	  start_speed = 20.0 / 3.0 /*7.5 */ ;
	  break;
	case 300:
	  g = 0.253;
	  start_speed = 2.182;
	  break;
	case 150:
	  g = 0.253;
	  start_speed = 4.367;
	  break;
	default:
	  g = 0.262;
	  start_speed = 7.29;
	}
      same_step = 1;
    }

  if (steps <= same_step)
    {
        time_period = static_cast<std::uint32_t>(yres * exposure_time /
                                                 dev->motor.base_ydpi /*MOTOR_GEAR */ );
      time_period = time_period / divider;

      if (time_period > 65535)
	time_period = 65535;

      for (i = 0; i < same_step; i++)
	{
        slope_table[i] = static_cast<std::uint16_t>(time_period);
	  sum_time += time_period;

	  DBG (DBG_io, "slope_table[%d] = %d\n", i, time_period);
	}

      DBG(DBG_proc, "%s: returns sum_time=%d, completed\n", __func__, sum_time);
      return sum_time;
    }

  for (i = 0; i < steps; i++)
    {
        double j = static_cast<double>(i) - same_step + 1;	/* start from 1/16 speed */

        if (j <= 0) {
            t = 0;
        } else {
            t = std::pow(j / (steps - same_step), g);
        }

        // time required for full steps
        time_period = static_cast<std::uint32_t>(yres * exposure_time /
                                                 dev->motor.base_ydpi /*MOTOR_GEAR */  *
                                                 (start_speed + (1 - start_speed) * t));

      time_period = time_period / divider;
        if (time_period > 65535) {
            time_period = 65535;
        }

        slope_table[i] = static_cast<std::uint16_t>(time_period);
      sum_time += time_period;

      DBG (DBG_io, "slope_table[%d] = %d\n", i, slope_table[i]);
    }

  DBG(DBG_proc, "%s: returns sum_time=%d, completed\n", __func__, sum_time);

  return sum_time;
}

/** @brief computes gamma table
 * Generates a gamma table of the given length within 0 and the given
 * maximum value
 * @param gamma_table gamma table to fill
 * @param size size of the table
 * @param maximum value allowed for gamma
 * @param gamma_max maximum gamma value
 * @param gamma gamma to compute values
 * @return a gamma table filled with the computed values
 * */
void
sanei_genesys_create_gamma_table (std::vector<uint16_t>& gamma_table, int size,
                                  float maximum, float gamma_max, float gamma)
{
    gamma_table.clear();
    gamma_table.resize(size, 0);

  int i;
  float value;

  DBG(DBG_proc, "%s: size = %d, ""maximum = %g, gamma_max = %g, gamma = %g\n", __func__, size,
      maximum, gamma_max, gamma);
  for (i = 0; i < size; i++)
    {
        value = gamma_max * std::pow(static_cast<double>(i) / size, 1.0 / gamma);
      if (value > maximum)
	value = maximum;
      gamma_table[i] = value;
    }
  DBG(DBG_proc, "%s: completed\n", __func__);
}

void sanei_genesys_create_default_gamma_table(Genesys_Device* dev,
                                              std::vector<uint16_t>& gamma_table, float gamma)
{
    int size = 0;
    int max = 0;
    if (dev->model->asic_type == AsicType::GL646) {
        if (dev->model->flags & GENESYS_FLAG_14BIT_GAMMA) {
            size = 16384;
        } else {
            size = 4096;
        }
        max = size - 1;
    } else {
        size = 256;
        max = 65535;
    }
    sanei_genesys_create_gamma_table(gamma_table, size, max, max, gamma);
}

/* computes the exposure_time on the basis of the given vertical dpi,
   the number of pixels the ccd needs to send,
   the step_type and the corresponding maximum speed from the motor struct */
/*
  Currently considers maximum motor speed at given step_type, minimum
  line exposure needed for conversion and led exposure time.

  TODO: Should also consider maximum transfer rate: ~6.5MB/s.
    Note: The enhance option of the scanners does _not_ help. It only halves
          the amount of pixels transfered.
 */
SANE_Int
sanei_genesys_exposure_time2 (Genesys_Device * dev, float ydpi,
                              int step_type, int endpixel, int exposure_by_led)
{
  int exposure_by_ccd = endpixel + 32;
    int exposure_by_motor = (dev->motor.slopes[step_type].maximum_speed * dev->motor.base_ydpi) / ydpi;

  int exposure = exposure_by_ccd;

  if (exposure < exposure_by_motor)
    exposure = exposure_by_motor;

  if (exposure < exposure_by_led && dev->model->is_cis)
    exposure = exposure_by_led;

    DBG(DBG_info, "%s: ydpi=%d, step=%d, endpixel=%d led=%d => exposure=%d\n", __func__,
        static_cast<int>(ydpi), step_type, endpixel, exposure_by_led, exposure);
  return exposure;
}


/* Sends a block of shading information to the scanner.
   The data is placed at address 0x0000 for color mode, gray mode and
   unconditionally for the following CCD chips: HP2300, HP2400 and HP5345
   In the other cases (lineart, halftone on ccd chips not mentioned) the
   addresses are 0x2a00 for dpihw==0, 0x5500 for dpihw==1 and 0xa800 for
   dpihw==2. //Note: why this?

   The data needs to be of size "size", and in little endian byte order.
 */
static void genesys_send_offset_and_shading(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                            uint8_t* data, int size)
{
    DBG_HELPER_ARGS(dbg, "(size = %d)", size);
  int dpihw;
  int start_address;

  /* ASIC higher than gl843 doesn't have register 2A/2B, so we route to
   * a per ASIC shading data loading function if available.
   * It is also used for scanners using SHDAREA */
    if (dev->cmd_set->has_send_shading_data()) {
        dev->cmd_set->send_shading_data(dev, sensor, data, size);
        return;
    }

  /* gl646, gl84[123] case */
  dpihw = dev->reg.get8(0x05) >> 6;

  /* TODO invert the test so only the 2 models behaving like that are
   * tested instead of adding all the others */
  /* many scanners send coefficient for lineart/gray like in color mode */
  if ((dev->settings.scan_mode == ScanColorMode::LINEART ||
       dev->settings.scan_mode == ScanColorMode::HALFTONE)
        && dev->model->sensor_id != SensorId::CCD_PLUSTEK_OPTICBOOK_3800
        && dev->model->sensor_id != SensorId::CCD_KVSS080
        && dev->model->sensor_id != SensorId::CCD_G4050
        && dev->model->sensor_id != SensorId::CCD_CANON_4400F
        && dev->model->sensor_id != SensorId::CCD_CANON_8400F
        && dev->model->sensor_id != SensorId::CCD_CANON_8600F
        && dev->model->sensor_id != SensorId::CCD_DSMOBILE600
        && dev->model->sensor_id != SensorId::CCD_XP300
        && dev->model->sensor_id != SensorId::CCD_DP665
        && dev->model->sensor_id != SensorId::CCD_DP685
        && dev->model->sensor_id != SensorId::CIS_CANON_LIDE_80
        && dev->model->sensor_id != SensorId::CCD_ROADWARRIOR
        && dev->model->sensor_id != SensorId::CCD_HP2300
        && dev->model->sensor_id != SensorId::CCD_HP2400
        && dev->model->sensor_id != SensorId::CCD_HP3670
        && dev->model->sensor_id != SensorId::CCD_5345)	/* lineart, halftone */
    {
        if (dpihw == 0) {		/* 600 dpi */
            start_address = 0x02a00;
        } else if (dpihw == 1) {	/* 1200 dpi */
            start_address = 0x05500;
        } else if (dpihw == 2) {	/* 2400 dpi */
            start_address = 0x0a800;
        } else {			/* reserved */
            throw SaneException("unknown dpihw");
        }
    }
    else { // color
        start_address = 0x00;
    }

    sanei_genesys_set_buffer_address(dev, start_address);

    dev->cmd_set->bulk_write_data(dev, 0x3c, data, size);
}

// ?
void sanei_genesys_init_shading_data(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                     int pixels_per_line)
{
    DBG_HELPER(dbg);

    if (dev->model->flags & GENESYS_FLAG_CALIBRATION_HOST_SIDE) {
        return;
    }

  int channels;
  int i;

  /* these models don't need to init shading data due to the use of specific send shading data
     function */
    if (dev->model->sensor_id==SensorId::CCD_KVSS080 ||
        dev->model->sensor_id==SensorId::CCD_G4050 ||
        dev->model->sensor_id==SensorId::CCD_CANON_4400F ||
        dev->model->sensor_id==SensorId::CCD_CANON_8400F ||
        dev->cmd_set->has_send_shading_data())
    {
        return;
    }

  DBG(DBG_proc, "%s (pixels_per_line = %d)\n", __func__, pixels_per_line);

    // BUG: GRAY shouldn't probably be in the if condition below. Discovered when refactoring
    if (dev->settings.scan_mode == ScanColorMode::GRAY ||
        dev->settings.scan_mode == ScanColorMode::COLOR_SINGLE_PASS)
    {
        channels = 3;
    } else {
        channels = 1;
    }

  // 16 bit black, 16 bit white
  std::vector<uint8_t> shading_data(pixels_per_line * 4 * channels, 0);

  uint8_t* shading_data_ptr = shading_data.data();

  for (i = 0; i < pixels_per_line * channels; i++)
    {
      *shading_data_ptr++ = 0x00;	/* dark lo */
      *shading_data_ptr++ = 0x00;	/* dark hi */
      *shading_data_ptr++ = 0x00;	/* white lo */
      *shading_data_ptr++ = 0x40;	/* white hi -> 0x4000 */
    }

    genesys_send_offset_and_shading(dev, sensor, shading_data.data(),
                                    pixels_per_line * 4 * channels);
}


// Find the position of the reference point: takes gray level 8 bits data and find
// first CCD usable pixel and top of scanning area
void sanei_genesys_search_reference_point(Genesys_Device* dev, Genesys_Sensor& sensor,
                                          const uint8_t* src_data, int start_pixel, int dpi,
                                          int width, int height)
{
    DBG_HELPER(dbg);
  int x, y;
  int current, left, top = 0;
  int size, count;
  int level = 80;		/* edge threshold level */

    // sanity check
    if ((width < 3) || (height < 3)) {
        throw SaneException("invalid width or height");
    }

  /* transformed image data */
  size = width * height;
  std::vector<uint8_t> image2(size, 0);
  std::vector<uint8_t> image(size, 0);

  /* laplace filter to denoise picture */
    std::memcpy(image2.data(), src_data, size);
    std::memcpy(image.data(), src_data, size);	// to initialize unprocessed part of the image buffer

    for (y = 1; y < height - 1; y++) {
        for (x = 1; x < width - 1; x++) {
            image[y * width + x] =
                (image2[(y - 1) * width + x + 1] + 2 * image2[(y - 1) * width + x] +
                image2[(y - 1) * width + x - 1] + 2 * image2[y * width + x + 1] +
                4 * image2[y * width + x] + 2 * image2[y * width + x - 1] +
                image2[(y + 1) * width + x + 1] + 2 * image2[(y + 1) * width + x] +
                image2[(y + 1) * width + x - 1]) / 16;
        }
    }

    image2 = image;
  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file("gl_laplace.pnm", image.data(), 8, 1, width, height);

  /* apply X direction sobel filter
     -1  0  1
     -2  0  2
     -1  0  1
     and finds threshold level
   */
  level = 0;
    for (y = 2; y < height - 2; y++) {
        for (x = 2; x < width - 2; x++) {
            current = image2[(y - 1) * width + x + 1] - image2[(y - 1) * width + x - 1] +
                      2 * image2[y * width + x + 1] - 2 * image2[y * width + x - 1] +
                      image2[(y + 1) * width + x + 1] - image2[(y + 1) * width + x - 1];
	if (current < 0)
	  current = -current;
	if (current > 255)
	  current = 255;
	image[y * width + x] = current;
	if (current > level)
	  level = current;
        }
    }
  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file("gl_xsobel.pnm", image.data(), 8, 1, width, height);

  /* set up detection level */
  level = level / 3;

  /* find left black margin first
     todo: search top before left
     we average the result of N searches */
  left = 0;
  count = 0;
  for (y = 2; y < 11; y++)
    {
      x = 8;
      while ((x < width / 2) && (image[y * width + x] < level))
	{
	  image[y * width + x] = 255;
	  x++;
	}
      count++;
      left += x;
    }
  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file("gl_detected-xsobel.pnm", image.data(), 8, 1, width, height);
  left = left / count;

    // turn it in CCD pixel at full sensor optical resolution
    sensor.ccd_start_xoffset = start_pixel + (left * sensor.optical_res) / dpi;

  /* find top edge by detecting black strip */
  /* apply Y direction sobel filter
     -1 -2 -1
     0  0  0
     1  2  1
   */
  level = 0;
    for (y = 2; y < height - 2; y++) {
        for (x = 2; x < width - 2; x++) {
            current = -image2[(y - 1) * width + x + 1] - 2 * image2[(y - 1) * width + x] -
                      image2[(y - 1) * width + x - 1] + image2[(y + 1) * width + x + 1] +
                      2 * image2[(y + 1) * width + x] + image2[(y + 1) * width + x - 1];
	if (current < 0)
	  current = -current;
	if (current > 255)
	  current = 255;
	image[y * width + x] = current;
	if (current > level)
	  level = current;
      }
    }
  if (DBG_LEVEL >= DBG_data)
    sanei_genesys_write_pnm_file("gl_ysobel.pnm", image.data(), 8, 1, width, height);

  /* set up detection level */
  level = level / 3;

  /* search top of horizontal black stripe : TODO yet another flag */
    if (dev->model->sensor_id == SensorId::CCD_5345
        && dev->model->motor_id == MotorId::MD_5345)
    {
      top = 0;
      count = 0;
      for (x = width / 2; x < width - 1; x++)
	{
	  y = 2;
	  while ((y < height) && (image[x + y * width] < level))
	    {
	      image[y * width + x] = 255;
	      y++;
	    }
	  count++;
	  top += y;
	}
      if (DBG_LEVEL >= DBG_data)
        sanei_genesys_write_pnm_file("gl_detected-ysobel.pnm", image.data(), 8, 1, width, height);
      top = top / count;

      /* bottom of black stripe is of fixed witdh, this hardcoded value
       * will be moved into device struct if more such values are needed */
      top += 10;
        dev->model->y_offset_calib_white = (top * MM_PER_INCH) / dpi;
        DBG(DBG_info, "%s: black stripe y_offset = %f mm \n", __func__,
            dev->model->y_offset_calib_white.value());
    }

  /* find white corner in dark area : TODO yet another flag */
    if ((dev->model->sensor_id == SensorId::CCD_HP2300 && dev->model->motor_id == MotorId::HP2300) ||
        (dev->model->sensor_id == SensorId::CCD_HP2400 && dev->model->motor_id == MotorId::HP2400) ||
        (dev->model->sensor_id == SensorId::CCD_HP3670 && dev->model->motor_id == MotorId::HP3670))
    {
      top = 0;
      count = 0;
      for (x = 10; x < 60; x++)
	{
	  y = 2;
	  while ((y < height) && (image[x + y * width] < level))
	    y++;
	  top += y;
	  count++;
	}
      top = top / count;
        dev->model->y_offset_calib_white = (top * MM_PER_INCH) / dpi;
        DBG(DBG_info, "%s: white corner y_offset = %f mm\n", __func__,
            dev->model->y_offset_calib_white.value());
    }

    DBG(DBG_proc, "%s: ccd_start_xoffset = %d, left = %d, top = %d\n", __func__,
        sensor.ccd_start_xoffset, left, top);
}

void sanei_genesys_calculate_zmod(bool two_table,
                                  uint32_t exposure_time,
                                  const std::vector<uint16_t>& slope_table,
                                  unsigned acceleration_steps,
                                  unsigned move_steps,
                                  unsigned buffer_acceleration_steps,
                                  uint32_t* out_z1, uint32_t* out_z2)
{
    DBG(DBG_info, "%s: two_table=%d\n", __func__, two_table);

    // acceleration total time
    unsigned sum = std::accumulate(slope_table.begin(), slope_table.begin() + acceleration_steps,
                                   0, std::plus<unsigned>());

    /* Z1MOD:
        c = sum(slope_table; reg_stepno)
        d = reg_fwdstep * <cruising speed>
        Z1MOD = (c+d) % exposure_time
    */
    *out_z1 = (sum + buffer_acceleration_steps * slope_table[acceleration_steps - 1]) % exposure_time;

    /* Z2MOD:
        a = sum(slope_table; reg_stepno)
        b = move_steps or 1 if 2 tables
        Z1MOD = (a+b) % exposure_time
    */
    if (!two_table) {
        sum = sum + (move_steps * slope_table[acceleration_steps - 1]);
    } else {
        sum = sum + slope_table[acceleration_steps - 1];
    }
    *out_z2 = sum % exposure_time;
}

static uint8_t genesys_adjust_gain(double* applied_multi, double multi, uint8_t gain)
{
  double voltage, original_voltage;
  uint8_t new_gain = 0;

  DBG(DBG_proc, "%s: multi=%f, gain=%d\n", __func__, multi, gain);

  voltage = 0.5 + gain * 0.25;
  original_voltage = voltage;

  voltage *= multi;

    new_gain = static_cast<std::uint8_t>((voltage - 0.5) * 4);
  if (new_gain > 0x0e)
    new_gain = 0x0e;

  voltage = 0.5 + (new_gain) * 0.25;

  *applied_multi = voltage / original_voltage;

  DBG(DBG_proc, "%s: orig voltage=%.2f, new voltage=%.2f, *applied_multi=%f, new_gain=%d\n",
      __func__, original_voltage, voltage, *applied_multi, new_gain);

  return new_gain;
}


// todo: is return status necessary (unchecked?)
static void genesys_average_white(Genesys_Device* dev, Genesys_Sensor& sensor, int channels,
                                  int channel, uint8_t* data, int size, int *max_average)
{

    DBG_HELPER_ARGS(dbg, "channels=%d, channel=%d, size=%d", channels, channel, size);
  int gain_white_ref, sum, range;
  int average;
  int i;

  range = size / 50;

    if (dev->settings.scan_method == ScanMethod::TRANSPARENCY ||
        dev->settings.scan_method == ScanMethod::TRANSPARENCY_INFRARED)
    {
        gain_white_ref = sensor.fau_gain_white_ref * 256;
    } else {
        gain_white_ref = sensor.gain_white_ref * 256;
    }

  if (range < 1)
    range = 1;

  size = size / (2 * range * channels);

  data += (channel * 2);

  *max_average = 0;

  while (size--)
    {
      sum = 0;
      for (i = 0; i < range; i++)
	{
	  sum += (*data);
	  sum += *(data + 1) * 256;
	  data += (2 * channels);	/* byte based */
	}

      average = (sum / range);
      if (average > *max_average)
	*max_average = average;
    }

  DBG(DBG_proc, "%s: max_average=%d, gain_white_ref = %d, finished\n", __func__, *max_average,
      gain_white_ref);

  if (*max_average >= gain_white_ref)
        throw SaneException(SANE_STATUS_INVAL);
}

/* todo: understand, values are too high */
static int
genesys_average_black (Genesys_Device * dev, int channel,
		       uint8_t * data, int pixels)
{
  int i;
  int sum;
  int pixel_step;

  DBG(DBG_proc, "%s: channel=%d, pixels=%d\n", __func__, channel, pixels);

  sum = 0;

  if (dev->settings.scan_mode == ScanColorMode::COLOR_SINGLE_PASS)
    {
      data += (channel * 2);
      pixel_step = 3 * 2;
    }
  else
    {
      pixel_step = 2;
    }

  for (i = 0; i < pixels; i++)
    {
      sum += *data;
      sum += *(data + 1) * 256;

      data += pixel_step;
    }

  DBG(DBG_proc, "%s = %d\n", __func__, sum / pixels);

    return sum / pixels;
}


// todo: check; it works but the lines 1, 2, and 3 are too dark even with the
// same offset and gain settings?
static void genesys_coarse_calibration(Genesys_Device* dev, Genesys_Sensor& sensor)
{
    DBG_HELPER_ARGS(dbg, "scan_mode = %d", static_cast<unsigned>(dev->settings.scan_mode));
  int size;
  int black_pixels;
  int white_average;
  uint8_t offset[4] = { 0xa0, 0x00, 0xa0, 0x40 };	/* first value isn't used */
  uint16_t white[12], dark[12];
  int i, j;

  black_pixels = sensor.black_pixels
    * dev->settings.xres / sensor.optical_res;

    unsigned channels = dev->settings.get_channels();

  DBG(DBG_info, "channels %d y_size %f xres %d\n", channels, dev->model->y_size.value(),
      dev->settings.xres);
    size = channels * 2 * dev->model->y_size * dev->settings.xres / 25.4;
  /*       1        1               mm                      1/inch        inch/mm */

  std::vector<uint8_t> calibration_data(size);
  std::vector<uint8_t> all_data(size * 4, 1);

    dev->cmd_set->set_fe(dev, sensor, AFE_INIT);

  dev->frontend.set_gain(0, 2);
  dev->frontend.set_gain(1, 2);
  dev->frontend.set_gain(2, 2); // TODO: ?  was 2
  dev->frontend.set_offset(0, offset[0]);
  dev->frontend.set_offset(1, offset[0]);
  dev->frontend.set_offset(2, offset[0]);

  for (i = 0; i < 4; i++)	/* read 4 lines */
    {
      if (i < 3)		/* first 3 lines */
        {
          dev->frontend.set_offset(0, offset[i]);
          dev->frontend.set_offset(1, offset[i]);
          dev->frontend.set_offset(2, offset[i]);
        }

      if (i == 1)		/* second line */
	{
	  double applied_multi;
	  double gain_white_ref;

            if (dev->settings.scan_method == ScanMethod::TRANSPARENCY ||
                dev->settings.scan_method == ScanMethod::TRANSPARENCY_INFRARED)
            {
                gain_white_ref = sensor.fau_gain_white_ref * 256;
            } else {
                gain_white_ref = sensor.gain_white_ref * 256;
            }

            // white and black are defined downwards

            uint8_t gain0 = genesys_adjust_gain(&applied_multi,
                                                gain_white_ref / (white[0] - dark[0]),
                                                dev->frontend.get_gain(0));
            uint8_t gain1 = genesys_adjust_gain(&applied_multi,
                                                gain_white_ref / (white[1] - dark[1]),
                                                dev->frontend.get_gain(1));
            uint8_t gain2 = genesys_adjust_gain(&applied_multi,
                                                gain_white_ref / (white[2] - dark[2]),
                                                dev->frontend.get_gain(2));
            // FIXME: looks like overwritten data. Are the above calculations doing
            // anything at all?
            dev->frontend.set_gain(0, gain0);
            dev->frontend.set_gain(1, gain1);
            dev->frontend.set_gain(2, gain2);
            dev->frontend.set_gain(0, 2);
            dev->frontend.set_gain(1, 2);
            dev->frontend.set_gain(2, 2);

        sanei_genesys_fe_write_data(dev, 0x28, dev->frontend.get_gain(0));
        sanei_genesys_fe_write_data(dev, 0x29, dev->frontend.get_gain(1));
        sanei_genesys_fe_write_data(dev, 0x2a, dev->frontend.get_gain(2));
	}

      if (i == 3)		/* last line */
	{
	  double x, y, rate;

	  for (j = 0; j < 3; j++)
	    {

                x = static_cast<double>(dark[(i - 2) * 3 + j] -
			  dark[(i - 1) * 3 + j]) * 254 / (offset[i - 1] / 2 -
							  offset[i - 2] / 2);
	      y = x - x * (offset[i - 1] / 2) / 254 - dark[(i - 1) * 3 + j];
	      rate = (x - DARK_VALUE - y) * 254 / x + 0.5;

                uint8_t curr_offset = static_cast<uint8_t>(rate);

                if (curr_offset > 0x7f) {
                    curr_offset = 0x7f;
                }
                curr_offset <<= 1;
                dev->frontend.set_offset(j, curr_offset);
	    }
	}
        sanei_genesys_fe_write_data(dev, 0x20, dev->frontend.get_offset(0));
        sanei_genesys_fe_write_data(dev, 0x21, dev->frontend.get_offset(1));
        sanei_genesys_fe_write_data(dev, 0x22, dev->frontend.get_offset(2));

      DBG(DBG_info,
          "%s: doing scan: gain: %d/%d/%d, offset: %d/%d/%d\n", __func__,
          dev->frontend.get_gain(0),
          dev->frontend.get_gain(1),
          dev->frontend.get_gain(2),
          dev->frontend.get_offset(0),
          dev->frontend.get_offset(1),
          dev->frontend.get_offset(2));


        dev->cmd_set->begin_scan(dev, sensor, &dev->calib_reg, false);

      sanei_genesys_read_data_from_scanner(dev, calibration_data.data(), size);

      std::memcpy(all_data.data() + i * size, calibration_data.data(), size);
      if (i == 3)		/* last line */
	{
          std::vector<uint8_t> all_data_8(size * 4 / 2);
	  unsigned int count;

        for (count = 0; count < static_cast<unsigned>(size * 4 / 2); count++) {
            all_data_8[count] = all_data[count * 2 + 1];
        }
        sanei_genesys_write_pnm_file("gl_coarse.pnm", all_data_8.data(), 8, channels, size / 6, 4);
	}

        dev->cmd_set->end_scan(dev, &dev->calib_reg, true);

      if (dev->settings.scan_mode == ScanColorMode::COLOR_SINGLE_PASS)
	{
	  for (j = 0; j < 3; j++)
	    {
            genesys_average_white(dev, sensor, 3, j, calibration_data.data(), size, &white_average);
	      white[i * 3 + j] = white_average;
	      dark[i * 3 + j] =
        genesys_average_black (dev, j, calibration_data.data(),
				       black_pixels);
              DBG(DBG_info, "%s: white[%d]=%d, black[%d]=%d\n", __func__,
                  i * 3 + j, white[i * 3 + j], i * 3 + j, dark[i * 3 + j]);
	    }
	}
      else			/* one color-component modes */
	{
        genesys_average_white(dev, sensor, 1, 0, calibration_data.data(), size, &white_average);
	  white[i * 3 + 0] = white[i * 3 + 1] = white[i * 3 + 2] =
	    white_average;
	  dark[i * 3 + 0] = dark[i * 3 + 1] = dark[i * 3 + 2] =
        genesys_average_black (dev, 0, calibration_data.data(), black_pixels);
	}
    }				/* for (i = 0; i < 4; i++) */

  DBG(DBG_info, "%s: final: gain: %d/%d/%d, offset: %d/%d/%d\n", __func__,
      dev->frontend.get_gain(0),
      dev->frontend.get_gain(1),
      dev->frontend.get_gain(2),
      dev->frontend.get_offset(0),
      dev->frontend.get_offset(1),
      dev->frontend.get_offset(2));
}

/* Averages image data.
   average_data and calibration_data are little endian 16 bit words.
 */
static void genesys_average_data(uint16_t* average_data,
                                 const std::uint16_t* calibration_data,
                      uint32_t lines,
		      uint32_t pixel_components_per_line)
{
  uint32_t x, y;
  uint32_t sum;

  for (x = 0; x < pixel_components_per_line; x++)
    {
      sum = 0;
      for (y = 0; y < lines; y++)
    {
            sum += calibration_data[(x + y * pixel_components_per_line)];
	}
      sum /= lines;
        *average_data++ = sum;
    }
}

/**
 * scans a white area with motor and lamp off to get the per CCD pixel offset
 * that will be used to compute shading coefficient
 * @param dev scanner's device
 */
static void genesys_shading_calibration_impl(Genesys_Device* dev, const Genesys_Sensor& sensor,
                                             std::vector<std::uint16_t>& out_average_data,
                                             bool is_dark, const std::string& log_filename_prefix)
{
    DBG_HELPER(dbg);

    debug_dump(DBG_info, dev->calib_session);

  size_t size;
  uint32_t pixels_per_line;
  uint8_t channels;

  /* end pixel - start pixel */
  pixels_per_line = dev->calib_pixels;
  channels = dev->calib_channels;

  uint32_t out_pixels_per_line = pixels_per_line + dev->calib_pixels_offset;

    // FIXME: we set this during both dark and white calibration. A cleaner approach should
    // probably be used
    dev->average_size = channels * out_pixels_per_line;

    out_average_data.clear();
    out_average_data.resize(dev->average_size);

    if (is_dark && dev->settings.scan_method == ScanMethod::TRANSPARENCY_INFRARED) {
        // FIXME: dark shading currently not supported on infrared transparency scans
        return;
    }

    // FIXME: the current calculation is likely incorrect on non-GL843 implementations,
    // but this needs checking
    if (dev->calib_total_bytes_to_read > 0) {
        size = dev->calib_total_bytes_to_read;
    } else if (dev->model->asic_type == AsicType::GL843) {
        size = channels * 2 * pixels_per_line * dev->calib_lines;
    } else {
        size = channels * 2 * pixels_per_line * (dev->calib_lines + 1);
    }

  std::vector<uint16_t> calibration_data(size / 2);

    bool motor = true;
  if (dev->model->flags & GENESYS_FLAG_SHADING_NO_MOVE)
    {
        motor = false;
    }

    // turn off motor and lamp power for flatbed scanners, but not for sheetfed scanners
    // because they have a calibration sheet with a sufficient black strip
    if (is_dark && !dev->model->is_sheetfed) {
        sanei_genesys_set_lamp_power(dev, sensor, dev->calib_reg, false);
        sanei_genesys_set_motor_power(dev->calib_reg, motor);
    } else {
        sanei_genesys_set_lamp_power(dev, sensor, dev->calib_reg, true);
        sanei_genesys_set_motor_power(dev->calib_reg, motor);
    }

    dev->write_registers(dev->calib_reg);

    if (is_dark) {
        // wait some time to let lamp to get dark
        sanei_genesys_sleep_ms(200);
    } else if (dev->model->flags & GENESYS_FLAG_DARK_CALIBRATION) {
        // make sure lamp is bright again
        // FIXME: what about scanners that take a long time to warm the lamp?
        sanei_genesys_sleep_ms(500);
    }

    bool start_motor = !is_dark;
    dev->cmd_set->begin_scan(dev, sensor, &dev->calib_reg, start_motor);

    sanei_genesys_read_data_from_scanner(dev, reinterpret_cast<std::uint8_t*>(calibration_data.data()),
                                         size);

    dev->cmd_set->end_scan(dev, &dev->calib_reg, true);

    if (dev->model->flags & GENESYS_FLAG_16BIT_DATA_INVERTED) {
        for (std::size_t i = 0; i < size / 2; ++i) {
            auto value = calibration_data[i];
            value = ((value >> 8) & 0xff) | ((value << 8) & 0xff00);
            calibration_data[i] = value;
        }
    }

    std::fill(out_average_data.begin(),
              out_average_data.begin() + dev->calib_pixels_offset * channels, 0);

    genesys_average_data(out_average_data.data() + dev->calib_pixels_offset * channels,
                         calibration_data.data(), dev->calib_lines, pixels_per_line * channels);

    if (DBG_LEVEL >= DBG_data) {
        sanei_genesys_write_pnm_file16((log_filename_prefix + "_shading.pnm").c_str(),
                                       calibration_data.data(),
                                       channels, pixels_per_line, dev->calib_lines);
        sanei_genesys_write_pnm_file16((log_filename_prefix + "_average.pnm").c_str(),
                                       out_average_data.data(),
                                       channels, out_pixels_per_line, 1);
    }
}


static void genesys_dark_shading_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor)
{
    DBG_HELPER(dbg);
    genesys_shading_calibration_impl(dev, sensor, dev->dark_average_data, true, "gl_black_");
}
/*
 * this function builds dummy dark calibration data so that we can
 * compute shading coefficient in a clean way
 *  todo: current values are hardcoded, we have to find if they
 * can be computed from previous calibration data (when doing offset
 * calibration ?)
 */
static void genesys_dummy_dark_shading(Genesys_Device* dev, const Genesys_Sensor& sensor)
{
    DBG_HELPER(dbg);
  uint32_t pixels_per_line;
  uint8_t channels;
  uint32_t skip, xend;
  int dummy1, dummy2, dummy3;	/* dummy black average per channel */

  pixels_per_line = dev->calib_pixels;
  channels = dev->calib_channels;

  uint32_t out_pixels_per_line = pixels_per_line + dev->calib_pixels_offset;

    dev->average_size = channels * out_pixels_per_line;
  dev->dark_average_data.clear();
  dev->dark_average_data.resize(dev->average_size, 0);

  /* we average values on 'the left' where CCD pixels are under casing and
     give darkest values. We then use these as dummy dark calibration */
  if (dev->settings.xres <= sensor.optical_res / 2)
    {
      skip = 4;
      xend = 36;
    }
  else
    {
      skip = 4;
      xend = 68;
    }
    if (dev->model->sensor_id==SensorId::CCD_G4050
     || dev->model->sensor_id==SensorId::CCD_CANON_4400F
     || dev->model->sensor_id==SensorId::CCD_CANON_8400F
     || dev->model->sensor_id==SensorId::CCD_KVSS080)
    {
      skip = 2;
      xend = sensor.black_pixels;
    }

  /* average each channels on half left margin */
  dummy1 = 0;
  dummy2 = 0;
  dummy3 = 0;

    for (unsigned x = skip + 1; x <= xend; x++) {
        dummy1 += dev->white_average_data[channels * x];
        if (channels > 1) {
            dummy2 += dev->white_average_data[channels * x + 1];
            dummy3 += dev->white_average_data[channels * x + 2];
        }
    }

  dummy1 /= (xend - skip);
  if (channels > 1)
    {
      dummy2 /= (xend - skip);
      dummy3 /= (xend - skip);
    }
  DBG(DBG_proc, "%s: dummy1=%d, dummy2=%d, dummy3=%d \n", __func__, dummy1, dummy2, dummy3);

  /* fill dark_average */
    for (unsigned x = 0; x < out_pixels_per_line; x++) {
        dev->dark_average_data[channels * 2 * x] = dummy1;
        if (channels > 1) {
            dev->dark_average_data[channels * 2 * x + 1] = dummy2;
            dev->dark_average_data[channels * 2 * x + 2] = dummy3;
        }
    }
}


static void genesys_repark_sensor_before_shading(Genesys_Device* dev)
{
    if (dev->model->flags & GENESYS_FLAG_SHADING_REPARK) {
        // rewind keeps registers and slopes table intact from previous scan but is not
        // available on all supported chipsets (or may cause scan artifacts, see #7)
        if (dev->cmd_set->has_rewind()) {
            dev->cmd_set->rewind(dev);
        } else {
            dev->cmd_set->slow_back_home(dev, true);
        }

        if (dev->settings.scan_method == ScanMethod::TRANSPARENCY ||
            dev->settings.scan_method == ScanMethod::TRANSPARENCY_INFRARED)
        {
            dev->cmd_set->move_to_ta(dev);
        }
    }
}

static void genesys_repark_sensor_after_white_shading(Genesys_Device* dev)
{
    if (dev->model->flags & GENESYS_FLAG_SHADING_REPARK) {
        dev->cmd_set->slow_back_home(dev, true);
    }
}

static void genesys_white_shading_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor)
{
    DBG_HELPER(dbg);
    genesys_shading_calibration_impl(dev, sensor, dev->white_average_data, false, "gl_white_");
}

// This calibration uses a scan over the calibration target, comprising a black and a white strip.
// (So the motor must be on.)
static void genesys_dark_white_shading_calibration(Genesys_Device* dev,
                                                   const Genesys_Sensor& sensor)
{
    DBG_HELPER_ARGS(dbg, "lines = %zu", dev->calib_lines);
  size_t size;
  uint32_t pixels_per_line;
  uint8_t channels;
  unsigned int x;
  uint32_t dark, white, dark_sum, white_sum, dark_count, white_count, col,
    dif;

  pixels_per_line = dev->calib_pixels;
  channels = dev->calib_channels;

  uint32_t out_pixels_per_line = pixels_per_line + dev->calib_pixels_offset;

    dev->average_size = channels * out_pixels_per_line;

  dev->white_average_data.clear();
  dev->white_average_data.resize(dev->average_size);

  dev->dark_average_data.clear();
  dev->dark_average_data.resize(dev->average_size);

  if (dev->calib_total_bytes_to_read > 0)
    size = dev->calib_total_bytes_to_read;
  else
    size = channels * 2 * pixels_per_line * dev->calib_lines;

  std::vector<uint8_t> calibration_data(size);

    bool motor = true;
  if (dev->model->flags & GENESYS_FLAG_SHADING_NO_MOVE)
    {
        motor = false;
    }

    // turn on motor and lamp power
    sanei_genesys_set_lamp_power(dev, sensor, dev->calib_reg, true);
    sanei_genesys_set_motor_power(dev->calib_reg, motor);

    dev->write_registers(dev->calib_reg);

    dev->cmd_set->begin_scan(dev, sensor, &dev->calib_reg, false);

    sanei_genesys_read_data_from_scanner(dev, calibration_data.data(), size);

    dev->cmd_set->end_scan(dev, &dev->calib_reg, true);

  if (DBG_LEVEL >= DBG_data)
    {
      if (dev->model->is_cis)
        {
          sanei_genesys_write_pnm_file("gl_black_white_shading.pnm", calibration_data.data(),
                                       16, 1, pixels_per_line*channels,
                                       dev->calib_lines);
        }
      else
        {
          sanei_genesys_write_pnm_file("gl_black_white_shading.pnm", calibration_data.data(),
                                       16, channels, pixels_per_line,
                                       dev->calib_lines);
        }
    }


    std::fill(dev->dark_average_data.begin(),
              dev->dark_average_data.begin() + dev->calib_pixels_offset * channels, 0);
    std::fill(dev->white_average_data.begin(),
              dev->white_average_data.begin() + dev->calib_pixels_offset * channels, 0);

    uint16_t* average_white = dev->white_average_data.data() + dev->calib_pixels_offset * channels;
    uint16_t* average_dark = dev->dark_average_data.data() + dev->calib_pixels_offset * channels;

  for (x = 0; x < pixels_per_line * channels; x++)
    {
      dark = 0xffff;
      white = 0;

            for (std::size_t y = 0; y < dev->calib_lines; y++)
	{
	  col = calibration_data[(x + y * pixels_per_line * channels) * 2];
	  col |=
	    calibration_data[(x + y * pixels_per_line * channels) * 2 +
			     1] << 8;

	  if (col > white)
	    white = col;
	  if (col < dark)
	    dark = col;
	}

      dif = white - dark;

      dark = dark + dif / 8;
      white = white - dif / 8;

      dark_count = 0;
      dark_sum = 0;

      white_count = 0;
      white_sum = 0;

            for (std::size_t y = 0; y < dev->calib_lines; y++)
	{
	  col = calibration_data[(x + y * pixels_per_line * channels) * 2];
	  col |=
	    calibration_data[(x + y * pixels_per_line * channels) * 2 +
			     1] << 8;

	  if (col >= white)
	    {
	      white_sum += col;
	      white_count++;
	    }
	  if (col <= dark)
	    {
	      dark_sum += col;
	      dark_count++;
	    }

	}

      dark_sum /= dark_count;
      white_sum /= white_count;

        *average_dark++ = dark_sum;
        *average_white++ = white_sum;
    }

    if (DBG_LEVEL >= DBG_data) {
        sanei_genesys_write_pnm_file16("gl_white_average.pnm", dev->white_average_data.data(),
                                       channels, out_pixels_per_line, 1);
        sanei_genesys_write_pnm_file16("gl_dark_average.pnm", dev->dark_average_data.data(),
                                       channels, out_pixels_per_line, 1);
    }
}

/* computes one coefficient given bright-dark value
 * @param coeff factor giving 1.00 gain
 * @param target desired target code
 * @param value brght-dark value
 * */
static unsigned int
compute_coefficient (unsigned int coeff, unsigned int target, unsigned int value)
{
  int result;

  if (value > 0)
    {
      result = (coeff * target) / value;
      if (result >= 65535)
	{
	  result = 65535;
	}
    }
  else
    {
      result = coeff;
    }
  return result;
}

/** @brief compute shading coefficients for LiDE scanners
 * The dark/white shading is actually performed _after_ reducing
 * resolution via averaging. only dark/white shading data for what would be
 * first pixel at full resolution is used.
 *
 * scanner raw input to output value calculation:
 *   o=(i-off)*(gain/coeff)
 *
 * from datasheet:
 *   off=dark_average
 *   gain=coeff*bright_target/(bright_average-dark_average)
 * works for dark_target==0
 *
 * what we want is these:
 *   bright_target=(bright_average-off)*(gain/coeff)
 *   dark_target=(dark_average-off)*(gain/coeff)
 * leading to
 *  off = (dark_average*bright_target - bright_average*dark_target)/(bright_target - dark_target)
 *  gain = (bright_target - dark_target)/(bright_average - dark_average)*coeff
 *
 * @param dev scanner's device
 * @param shading_data memory area where to store the computed shading coefficients
 * @param pixels_per_line number of pixels per line
 * @param words_per_color memory words per color channel
 * @param channels number of color channels (actually 1 or 3)
 * @param o shading coefficients left offset
 * @param coeff 4000h or 2000h depending on fast scan mode or not (GAIN4 bit)
 * @param target_bright value of the white target code
 * @param target_dark value of the black target code
*/
static void
compute_averaged_planar (Genesys_Device * dev, const Genesys_Sensor& sensor,
			 uint8_t * shading_data,
			 unsigned int pixels_per_line,
			 unsigned int words_per_color,
			 unsigned int channels,
			 unsigned int o,
			 unsigned int coeff,
			 unsigned int target_bright,
			 unsigned int target_dark)
{
  unsigned int x, i, j, br, dk, res, avgpixels, basepixels, val;
  unsigned int fill,factor;

  DBG(DBG_info, "%s: pixels=%d, offset=%d\n", __func__, pixels_per_line, o);

  /* initialize result */
  memset (shading_data, 0xff, words_per_color * 3 * 2);

  /*
     strangely i can write 0x20000 bytes beginning at 0x00000 without overwriting
     slope tables - which begin at address 0x10000(for 1200dpi hw mode):
     memory is organized in words(2 bytes) instead of single bytes. explains
     quite some things
   */
/*
  another one: the dark/white shading is actually performed _after_ reducing
  resolution via averaging. only dark/white shading data for what would be
  first pixel at full resolution is used.
 */
/*
  scanner raw input to output value calculation:
    o=(i-off)*(gain/coeff)

  from datasheet:
    off=dark_average
    gain=coeff*bright_target/(bright_average-dark_average)
  works for dark_target==0

  what we want is these:
    bright_target=(bright_average-off)*(gain/coeff)
    dark_target=(dark_average-off)*(gain/coeff)
  leading to
    off = (dark_average*bright_target - bright_average*dark_target)/(bright_target - dark_target)
    gain = (bright_target - dark_target)/(bright_average - dark_average)*coeff
 */
  res = dev->settings.xres;

    if (sensor.get_ccd_size_divisor_for_dpi(dev->settings.xres) > 1)
    {
        res *= 2;
    }

  /* this should be evenly dividable */
  basepixels = sensor.optical_res / res;

  /* gl841 supports 1/1 1/2 1/3 1/4 1/5 1/6 1/8 1/10 1/12 1/15 averaging */
  if (basepixels < 1)
    avgpixels = 1;
  else if (basepixels < 6)
    avgpixels = basepixels;
  else if (basepixels < 8)
    avgpixels = 6;
  else if (basepixels < 10)
    avgpixels = 8;
  else if (basepixels < 12)
    avgpixels = 10;
  else if (basepixels < 15)
    avgpixels = 12;
  else
    avgpixels = 15;

  /* LiDE80 packs shading data */
    if (dev->model->sensor_id != SensorId::CIS_CANON_LIDE_80) {
      factor=1;
      fill=avgpixels;
    }
  else
    {
      factor=avgpixels;
      fill=1;
    }

  DBG(DBG_info, "%s: averaging over %d pixels\n", __func__, avgpixels);
  DBG(DBG_info, "%s: packing factor is %d\n", __func__, factor);
  DBG(DBG_info, "%s: fill length is %d\n", __func__, fill);

  for (x = 0; x <= pixels_per_line - avgpixels; x += avgpixels)
    {
      if ((x + o) * 2 * 2 + 3 > words_per_color * 2)
	break;

      for (j = 0; j < channels; j++)
	{

	  dk = 0;
	  br = 0;
	  for (i = 0; i < avgpixels; i++)
	    {
                // dark data
                dk += dev->dark_average_data[(x + i + pixels_per_line * j)];
                // white data
                br += dev->white_average_data[(x + i + pixels_per_line * j)];
	    }

	  br /= avgpixels;
	  dk /= avgpixels;

	  if (br * target_dark > dk * target_bright)
	    val = 0;
	  else if (dk * target_bright - br * target_dark >
		   65535 * (target_bright - target_dark))
	    val = 65535;
	  else
            {
	      val = (dk * target_bright - br * target_dark) / (target_bright - target_dark);
            }

          /*fill all pixels, even if only the last one is relevant*/
	  for (i = 0; i < fill; i++)
	    {
	      shading_data[(x/factor + o + i) * 2 * 2 + words_per_color * 2 * j] = val & 0xff;
	      shading_data[(x/factor + o + i) * 2 * 2 + words_per_color * 2 * j + 1] = val >> 8;
	    }

	  val = br - dk;

	  if (65535 * val > (target_bright - target_dark) * coeff)
            {
	      val = (coeff * (target_bright - target_dark)) / val;
            }
	  else
            {
	      val = 65535;
            }

          /*fill all pixels, even if only the last one is relevant*/
	  for (i = 0; i < fill; i++)
	    {
	      shading_data[(x/factor + o + i) * 2 * 2 + words_per_color * 2 * j + 2] = val & 0xff;
	      shading_data[(x/factor + o + i) * 2 * 2 + words_per_color * 2 * j + 3] = val >> 8;
	    }
	}

      /* fill remaining channels */
      for (j = channels; j < 3; j++)
	{
	  for (i = 0; i < fill; i++)
	    {
	      shading_data[(x/factor + o + i) * 2 * 2 + words_per_color * 2 * j    ] = shading_data[(x/factor + o + i) * 2 * 2    ];
	      shading_data[(x/factor + o + i) * 2 * 2 + words_per_color * 2 * j + 1] = shading_data[(x/factor + o + i) * 2 * 2 + 1];
	      shading_data[(x/factor + o + i) * 2 * 2 + words_per_color * 2 * j + 2] = shading_data[(x/factor + o + i) * 2 * 2 + 2];
	      shading_data[(x/factor + o + i) * 2 * 2 + words_per_color * 2 * j + 3] = shading_data[(x/factor + o + i) * 2 * 2 + 3];
	    }
	}
    }
}

static std::array<unsigned, 3> color_order_to_cmat(ColorOrder color_order)
{
    switch (color_order) {
        case ColorOrder::RGB: return {0, 1, 2};
        case ColorOrder::GBR: return {2, 0, 1};
        default:
            throw std::logic_error("Unknown color order");
    }
}

/**
 * Computes shading coefficient using formula in data sheet. 16bit data values
 * manipulated here are little endian. For now we assume deletion scanning type
 * and that there is always 3 channels.
 * @param dev scanner's device
 * @param shading_data memory area where to store the computed shading coefficients
 * @param pixels_per_line number of pixels per line
 * @param channels number of color channels (actually 1 or 3)
 * @param cmat color transposition matrix
 * @param offset shading coefficients left offset
 * @param coeff 4000h or 2000h depending on fast scan mode or not
 * @param target value of the target code
 */
static void compute_coefficients(Genesys_Device * dev,
		      uint8_t * shading_data,
		      unsigned int pixels_per_line,
		      unsigned int channels,
                                 ColorOrder color_order,
		      int offset,
		      unsigned int coeff,
		      unsigned int target)
{
  uint8_t *ptr;			/* contain 16bit words in little endian */
  unsigned int x, c;
  unsigned int val, br, dk;
  unsigned int start, end;

  DBG(DBG_io, "%s: pixels_per_line=%d,  coeff=0x%04x\n", __func__, pixels_per_line, coeff);

    auto cmat = color_order_to_cmat(color_order);

  /* compute start & end values depending of the offset */
  if (offset < 0)
   {
      start = -1 * offset;
      end = pixels_per_line;
   }
  else
   {
     start = 0;
     end = pixels_per_line - offset;
   }

  for (c = 0; c < channels; c++)
    {
      for (x = start; x < end; x++)
	{
	  /* TODO if channels=1 , use filter to know the base addr */
	  ptr = shading_data + 4 * ((x + offset) * channels + cmat[c]);

        // dark data
        dk = dev->dark_average_data[x * channels + c];

        // white data
        br = dev->white_average_data[x * channels + c];

	  /* compute coeff */
	  val=compute_coefficient(coeff,target,br-dk);

	  /* assign it */
	  ptr[0] = dk & 255;
	  ptr[1] = dk / 256;
	  ptr[2] = val & 0xff;
	  ptr[3] = val / 256;

	}
    }
}

/**
 * Computes shading coefficient using formula in data sheet. 16bit data values
 * manipulated here are little endian. Data is in planar form, ie grouped by
 * lines of the same color component.
 * @param dev scanner's device
 * @param shading_data memory area where to store the computed shading coefficients
 * @param factor averaging factor when the calibration scan is done at a higher resolution
 * than the final scan
 * @param pixels_per_line number of pixels per line
 * @param words_per_color total number of shading data words for one color element
 * @param channels number of color channels (actually 1 or 3)
 * @param cmat transcoding matrix for color channel order
 * @param offset shading coefficients left offset
 * @param coeff 4000h or 2000h depending on fast scan mode or not
 * @param target white target value
 */
static void compute_planar_coefficients(Genesys_Device * dev,
			     uint8_t * shading_data,
			     unsigned int factor,
			     unsigned int pixels_per_line,
			     unsigned int words_per_color,
			     unsigned int channels,
                                        ColorOrder color_order,
			     unsigned int offset,
			     unsigned int coeff,
			     unsigned int target)
{
  uint8_t *ptr;			/* contains 16bit words in little endian */
  uint32_t x, c, i;
  uint32_t val, dk, br;

    auto cmat = color_order_to_cmat(color_order);

  DBG(DBG_io, "%s: factor=%d, pixels_per_line=%d, words=0x%X, coeff=0x%04x\n", __func__, factor,
      pixels_per_line, words_per_color, coeff);
  for (c = 0; c < channels; c++)
    {
      /* shading data is larger than pixels_per_line so offset can be neglected */
      for (x = 0; x < pixels_per_line; x+=factor)
	{
	  /* x2 because of 16 bit values, and x2 since one coeff for dark
	   * and another for white */
	  ptr = shading_data + words_per_color * cmat[c] * 2 + (x + offset) * 4;

	  dk = 0;
	  br = 0;

	  /* average case */
	  for(i=0;i<factor;i++)
	  {
                dk += dev->dark_average_data[((x+i) + pixels_per_line * c)];
                br += dev->white_average_data[((x+i) + pixels_per_line * c)];
	  }
	  dk /= factor;
	  br /= factor;

	  val = compute_coefficient (coeff, target, br - dk);

	  /* we duplicate the information to have calibration data at optical resolution */
	  for (i = 0; i < factor; i++)
	    {
	      ptr[0 + 4 * i] = dk & 255;
	      ptr[1 + 4 * i] = dk / 256;
	      ptr[2 + 4 * i] = val & 0xff;
	      ptr[3 + 4 * i] = val / 256;
	    }
	}
    }
  /* in case of gray level scan, we duplicate shading information on all
   * three color channels */
  if(channels==1)
  {
	  memcpy(shading_data+cmat[1]*2*words_per_color,
	         shading_data+cmat[0]*2*words_per_color,
		 words_per_color*2);
	  memcpy(shading_data+cmat[2]*2*words_per_color,
	         shading_data+cmat[0]*2*words_per_color,
		 words_per_color*2);
  }
}

static void
compute_shifted_coefficients (Genesys_Device * dev,
                              const Genesys_Sensor& sensor,
			      uint8_t * shading_data,
			      unsigned int pixels_per_line,
			      unsigned int channels,
                              ColorOrder color_order,
			      int offset,
			      unsigned int coeff,
			      unsigned int target_dark,
			      unsigned int target_bright,
			      unsigned int patch_size)		/* contigous extent */
{
  unsigned int x, avgpixels, basepixels, i, j, val1, val2;
  unsigned int br_tmp [3], dk_tmp [3];
  uint8_t *ptr = shading_data + offset * 3 * 4;                 /* contain 16bit words in little endian */
  unsigned int patch_cnt = offset * 3;                          /* at start, offset of first patch */

    auto cmat = color_order_to_cmat(color_order);

  x = dev->settings.xres;
  if (sensor.get_ccd_size_divisor_for_dpi(dev->settings.xres) > 1)
    x *= 2;							/* scanner is using half-ccd mode */
  basepixels = sensor.optical_res / x;			/*this should be evenly dividable */

      /* gl841 supports 1/1 1/2 1/3 1/4 1/5 1/6 1/8 1/10 1/12 1/15 averaging */
      if (basepixels < 1)
        avgpixels = 1;
      else if (basepixels < 6)
        avgpixels = basepixels;
      else if (basepixels < 8)
        avgpixels = 6;
      else if (basepixels < 10)
        avgpixels = 8;
      else if (basepixels < 12)
        avgpixels = 10;
      else if (basepixels < 15)
        avgpixels = 12;
      else
        avgpixels = 15;
  DBG(DBG_info, "%s: pixels_per_line=%d,  coeff=0x%04x,  averaging over %d pixels\n", __func__,
      pixels_per_line, coeff, avgpixels);

  for (x = 0; x <= pixels_per_line - avgpixels; x += avgpixels) {
    memset (&br_tmp, 0, sizeof(br_tmp));
    memset (&dk_tmp, 0, sizeof(dk_tmp));

    for (i = 0; i < avgpixels; i++) {
      for (j = 0; j < channels; j++) {
                br_tmp[j] += dev->white_average_data[((x + i) * channels + j)];
                dk_tmp[i] += dev->dark_average_data[((x + i) * channels + j)];
      }
    }
    for (j = 0; j < channels; j++) {
      br_tmp[j] /= avgpixels;
      dk_tmp[j] /= avgpixels;

      if (br_tmp[j] * target_dark > dk_tmp[j] * target_bright)
        val1 = 0;
      else if (dk_tmp[j] * target_bright - br_tmp[j] * target_dark > 65535 * (target_bright - target_dark))
        val1 = 65535;
      else
        val1 = (dk_tmp[j] * target_bright - br_tmp[j] * target_dark) / (target_bright - target_dark);

      val2 = br_tmp[j] - dk_tmp[j];
      if (65535 * val2 > (target_bright - target_dark) * coeff)
        val2 = (coeff * (target_bright - target_dark)) / val2;
      else
        val2 = 65535;

      br_tmp[j] = val1;
      dk_tmp[j] = val2;
    }
    for (i = 0; i < avgpixels; i++) {
      for (j = 0; j < channels; j++) {
        * ptr++ = br_tmp[ cmat[j] ] & 0xff;
        * ptr++ = br_tmp[ cmat[j] ] >> 8;
        * ptr++ = dk_tmp[ cmat[j] ] & 0xff;
        * ptr++ = dk_tmp[ cmat[j] ] >> 8;
        patch_cnt++;
        if (patch_cnt == patch_size) {
          patch_cnt = 0;
          val1 = cmat[2];
          cmat[2] = cmat[1];
          cmat[1] = cmat[0];
          cmat[0] = val1;
        }
      }
    }
  }
}

static void genesys_send_shading_coefficient(Genesys_Device* dev, const Genesys_Sensor& sensor)
{
    DBG_HELPER(dbg);

    if (dev->model->flags & GENESYS_FLAG_CALIBRATION_HOST_SIDE) {
        return;
    }

  uint32_t pixels_per_line;
  uint8_t channels;
  int o;
  unsigned int length;		/**> number of shading calibration data words */
  unsigned int factor;
  unsigned int coeff, target_code, words_per_color = 0;

  pixels_per_line = dev->calib_pixels + dev->calib_pixels_offset;
  channels = dev->calib_channels;

  /* we always build data for three channels, even for gray
   * we make the shading data such that each color channel data line is contiguous
   * to the next one, which allow to write the 3 channels in 1 write
   * during genesys_send_shading_coefficient, some values are words, other bytes
   * hence the x2 factor */
  switch (dev->reg.get8(0x05) >> 6)
    {
      /* 600 dpi */
    case 0:
      words_per_color = 0x2a00;
      break;
      /* 1200 dpi */
    case 1:
      words_per_color = 0x5500;
      break;
      /* 2400 dpi */
    case 2:
      words_per_color = 0xa800;
      break;
      /* 4800 dpi */
    case 3:
      words_per_color = 0x15000;
      break;
    }

  /* special case, memory is aligned on 0x5400, this has yet to be explained */
  /* could be 0xa800 because sensor is truly 2400 dpi, then halved because
   * we only set 1200 dpi */
  if(dev->model->sensor_id==SensorId::CIS_CANON_LIDE_80)
    {
      words_per_color = 0x5400;
    }

  length = words_per_color * 3 * 2;

  /* allocate computed size */
  // contains 16bit words in little endian
  std::vector<uint8_t> shading_data(length, 0);

  /* TARGET/(Wn-Dn) = white gain -> ~1.xxx then it is multiplied by 0x2000
     or 0x4000 to give an integer
     Wn = white average for column n
     Dn = dark average for column n
   */
    if (dev->cmd_set->get_gain4_bit(&dev->calib_reg)) {
        coeff = 0x4000;
    } else {
        coeff = 0x2000;
    }

  /* compute avg factor */
  if(dev->settings.xres>sensor.optical_res)
    {
      factor=1;
    }
  else
    {
      factor=sensor.optical_res/dev->settings.xres;
    }

  /* for GL646, shading data is planar if REG01_FASTMOD is set and
   * chunky if not. For now we rely on the fact that we know that
   * each sensor is used only in one mode. Currently only the CIS_XP200
   * sets REG01_FASTMOD.
   */

  /* TODO setup a struct in genesys_devices that
   * will handle these settings instead of having this switch growing up */
  switch (dev->model->sensor_id)
    {
    case SensorId::CCD_XP300:
    case SensorId::CCD_ROADWARRIOR:
    case SensorId::CCD_DP665:
    case SensorId::CCD_DP685:
    case SensorId::CCD_DSMOBILE600:
      target_code = 0xdc00;
      o = 4;
      compute_planar_coefficients (dev,
                   shading_data.data(),
				   factor,
				   pixels_per_line,
				   words_per_color,
				   channels,
                   ColorOrder::RGB,
				   o,
				   coeff,
				   target_code);
      break;
    case SensorId::CIS_XP200:
      target_code = 0xdc00;
      o = 2;
      compute_planar_coefficients (dev,
                   shading_data.data(),
				   1,
				   pixels_per_line,
				   words_per_color,
				   channels,
                   ColorOrder::GBR,
				   o,
				   coeff,
				   target_code);
      break;
    case SensorId::CCD_HP2300:
      target_code = 0xdc00;
      o = 2;
      if(dev->settings.xres<=sensor.optical_res/2)
       {
          o = o - sensor.dummy_pixel / 2;
       }
      compute_coefficients (dev,
                shading_data.data(),
			    pixels_per_line,
			    3,
                            ColorOrder::RGB,
                            o,
                            coeff,
                            target_code);
      break;
    case SensorId::CCD_5345:
      target_code = 0xe000;
      o = 4;
      if(dev->settings.xres<=sensor.optical_res/2)
       {
          o = o - sensor.dummy_pixel;
       }
      compute_coefficients (dev,
                shading_data.data(),
			    pixels_per_line,
			    3,
                            ColorOrder::RGB,
                            o,
                            coeff,
                            target_code);
      break;
    case SensorId::CCD_HP3670:
    case SensorId::CCD_HP2400:
      target_code = 0xe000;
            // offset is dependent on ccd_pixels_per_system_pixel(), but we couldn't use this in
            // common code previously.
            // FIXME: use sensor.ccd_pixels_per_system_pixel()
      if(dev->settings.xres<=300)
        {
                o = -10;
        }
      else if(dev->settings.xres<=600)
        {
                o = -6;
        }
      else
        {
          o = +2;
        }
      compute_coefficients (dev,
                shading_data.data(),
			    pixels_per_line,
			    3,
                            ColorOrder::RGB,
                            o,
                            coeff,
                            target_code);
      break;
    case SensorId::CCD_KVSS080:
    case SensorId::CCD_PLUSTEK_OPTICBOOK_3800:
    case SensorId::CCD_G4050:
    case SensorId::CCD_CANON_4400F:
    case SensorId::CCD_CANON_8400F:
    case SensorId::CCD_CANON_8600F:
    case SensorId::CCD_PLUSTEK_OPTICFILM_7200I:
    case SensorId::CCD_PLUSTEK_OPTICFILM_7300:
    case SensorId::CCD_PLUSTEK_OPTICFILM_7500I:
      target_code = 0xe000;
      o = 0;
      compute_coefficients (dev,
                shading_data.data(),
			    pixels_per_line,
			    3,
                            ColorOrder::RGB,
                            o,
                            coeff,
                            target_code);
      break;
    case SensorId::CIS_CANON_LIDE_700F:
    case SensorId::CIS_CANON_LIDE_100:
    case SensorId::CIS_CANON_LIDE_200:
    case SensorId::CIS_CANON_LIDE_110:
    case SensorId::CIS_CANON_LIDE_120:
    case SensorId::CIS_CANON_LIDE_210:
    case SensorId::CIS_CANON_LIDE_220:
        /* TODO store this in a data struct so we avoid
         * growing this switch */
        switch(dev->model->sensor_id)
          {
          case SensorId::CIS_CANON_LIDE_110:
          case SensorId::CIS_CANON_LIDE_120:
          case SensorId::CIS_CANON_LIDE_210:
          case SensorId::CIS_CANON_LIDE_220:
            target_code = 0xf000;
            break;
          case SensorId::CIS_CANON_LIDE_700F:
            target_code = 0xc000; /* from experimentation */
            break;
          default:
            target_code = 0xdc00;
          }
        words_per_color=pixels_per_line*2;
        length = words_per_color * 3 * 2;
        shading_data.clear();
        shading_data.resize(length, 0);
        compute_planar_coefficients (dev,
                                     shading_data.data(),
                                     1,
                                     pixels_per_line,
                                     words_per_color,
                                     channels,
                                     ColorOrder::RGB,
                                     0,
                                     coeff,
                                     target_code);
      break;
    case SensorId::CIS_CANON_LIDE_35:
      compute_averaged_planar (dev, sensor,
                               shading_data.data(),
                               pixels_per_line,
                               words_per_color,
                               channels,
                               4,
                               coeff,
                               0xe000,
                               0x0a00);
      break;
    case SensorId::CIS_CANON_LIDE_80:
      compute_averaged_planar (dev, sensor,
                               shading_data.data(),
                               pixels_per_line,
                               words_per_color,
                               channels,
                               0,
                               coeff,
			       0xe000,
                               0x0800);
      break;
    case SensorId::CCD_PLUSTEK_OPTICPRO_3600:
      compute_shifted_coefficients (dev, sensor,
                        shading_data.data(),
			            pixels_per_line,
			            channels,
                        ColorOrder::RGB,
			            12,         /* offset */
			            coeff,
 			            0x0001,      /* target_dark */
			            0xf900,      /* target_bright */
			            256);        /* patch_size: contigous extent */
      break;
    default:
        throw SaneException(SANE_STATUS_UNSUPPORTED, "sensor %d not supported",
                            static_cast<unsigned>(dev->model->sensor_id));
      break;
    }

    // do the actual write of shading calibration data to the scanner
    genesys_send_offset_and_shading(dev, sensor, shading_data.data(), length);
}


/**
 * search calibration cache list for an entry matching required scan.
 * If one is found, set device calibration with it
 * @param dev scanner's device
 * @return false if no matching cache entry has been
 * found, true if one has been found and used.
 */
static bool
genesys_restore_calibration(Genesys_Device * dev, Genesys_Sensor& sensor)
{
    DBG_HELPER(dbg);

    // if no cache or no function to evaluate cache entry ther can be no match/
    if (dev->calibration_cache.empty()) {
        return false;
    }

  /* we walk the link list of calibration cache in search for a
   * matching one */
  for (auto& cache : dev->calibration_cache)
    {
        if (dev->cmd_set->is_compatible_calibration(dev, sensor, &cache, false)) {
          dev->frontend = cache.frontend;
          /* we don't restore the gamma fields */
          sensor.exposure = cache.sensor.exposure;

          dev->average_size = cache.average_size;
          dev->calib_pixels = cache.calib_pixels;
          dev->calib_channels = cache.calib_channels;

          dev->dark_average_data = cache.dark_average_data;
          dev->white_average_data = cache.white_average_data;

            if (!dev->cmd_set->has_send_shading_data()) {
            genesys_send_shading_coefficient(dev, sensor);
          }

          DBG(DBG_proc, "%s: restored\n", __func__);
          return true;
	}
    }
  DBG(DBG_proc, "%s: completed(nothing found)\n", __func__);
  return false;
}


static void genesys_save_calibration(Genesys_Device* dev, const Genesys_Sensor& sensor)
{
    DBG_HELPER(dbg);
#ifdef HAVE_SYS_TIME_H
  struct timeval time;
#endif

  auto found_cache_it = dev->calibration_cache.end();
  for (auto cache_it = dev->calibration_cache.begin(); cache_it != dev->calibration_cache.end();
       cache_it++)
    {
        if (dev->cmd_set->is_compatible_calibration(dev, sensor, &*cache_it, true)) {
          found_cache_it = cache_it;
          break;
        }
    }

  /* if we found on overridable cache, we reuse it */
  if (found_cache_it == dev->calibration_cache.end())
    {
      /* create a new cache entry and insert it in the linked list */
      dev->calibration_cache.push_back(Genesys_Calibration_Cache());
      found_cache_it = std::prev(dev->calibration_cache.end());
    }

  found_cache_it->average_size = dev->average_size;

  found_cache_it->dark_average_data = dev->dark_average_data;
  found_cache_it->white_average_data = dev->white_average_data;

  found_cache_it->used_setup = dev->current_setup;
  found_cache_it->params = dev->session.params;
  found_cache_it->frontend = dev->frontend;
  found_cache_it->sensor = sensor;

  found_cache_it->calib_pixels = dev->calib_pixels;
  found_cache_it->calib_channels = dev->calib_channels;

#ifdef HAVE_SYS_TIME_H
    gettimeofday(&time, nullptr);
  found_cache_it->last_calibration = time.tv_sec;
#endif
}

/**
 * does the calibration process for a flatbed scanner
 * - offset calibration
 * - gain calibration
 * - shading calibration
 * @param dev device to calibrate
 */
static void genesys_flatbed_calibration(Genesys_Device* dev, Genesys_Sensor& sensor)
{
    DBG_HELPER(dbg);
  uint32_t pixels_per_line;

    unsigned coarse_res = sensor.optical_res;
    if (dev->settings.yres <= sensor.optical_res / 2) {
        coarse_res /= 2;
    }

    if (dev->model->model_id == ModelId::CANON_8400F ||
        dev->model->model_id == ModelId::CANON_8600F)
    {
        coarse_res = 1200;
    }

  /* do offset calibration if needed */
  if (dev->model->flags & GENESYS_FLAG_OFFSET_CALIBRATION)
    {
      sanei_usb_testing_record_message("offset_calibration");
        dev->cmd_set->offset_calibration(dev, sensor, dev->calib_reg);

      /* since all the registers are set up correctly, just use them */
      sanei_usb_testing_record_message("coarse_gain_calibration");
        dev->cmd_set->coarse_gain_calibration(dev, sensor, dev->calib_reg, coarse_res);
    }
  else
    /* since we have 2 gain calibration proc, skip second if first one was
       used. */
    {
      sanei_usb_testing_record_message("init_regs_for_coarse_calibration");
        dev->cmd_set->init_regs_for_coarse_calibration(dev, sensor, dev->calib_reg);

      sanei_usb_testing_record_message("genesys_coarse_calibration");
        genesys_coarse_calibration(dev, sensor);
    }

  if (dev->model->is_cis)
    {
      /* the afe now sends valid data for doing led calibration */
        sanei_usb_testing_record_message("led_calibration");
        sensor.exposure = dev->cmd_set->led_calibration(dev, sensor, dev->calib_reg);

      /* calibrate afe again to match new exposure */
      if (dev->model->flags & GENESYS_FLAG_OFFSET_CALIBRATION)
	{
          sanei_usb_testing_record_message("offset_calibration");
            dev->cmd_set->offset_calibration(dev, sensor, dev->calib_reg);

	  /* since all the registers are set up correctly, just use them */

          sanei_usb_testing_record_message("coarse_gain_calibration");
        dev->cmd_set->coarse_gain_calibration(dev, sensor, dev->calib_reg, coarse_res);
	}
      else
	/* since we have 2 gain calibration proc, skip second if first one was
	   used. */
	{
          sanei_usb_testing_record_message("init_regs_for_coarse_calibration");
            dev->cmd_set->init_regs_for_coarse_calibration(dev, sensor, dev->calib_reg);

          sanei_usb_testing_record_message("genesys_coarse_calibration");
        genesys_coarse_calibration(dev, sensor);
	}
    }

  /* we always use sensor pixel number when the ASIC can't handle multi-segments sensor */
  if (!(dev->model->flags & GENESYS_FLAG_SIS_SENSOR))
    {
      pixels_per_line = (dev->model->x_size * dev->settings.xres) / MM_PER_INCH;
    }
  else
    {
      pixels_per_line = sensor.sensor_pixels;
    }

  /* send default shading data */
  sanei_usb_testing_record_message("sanei_genesys_init_shading_data");
    sanei_genesys_init_shading_data(dev, sensor, pixels_per_line);

  if (dev->settings.scan_method == ScanMethod::TRANSPARENCY ||
      dev->settings.scan_method == ScanMethod::TRANSPARENCY_INFRARED)
  {
        dev->cmd_set->move_to_ta(dev);
  }

    // shading calibration
    if (dev->model->flags & GENESYS_FLAG_DARK_WHITE_CALIBRATION) {
        sanei_usb_testing_record_message("init_regs_for_shading");
        dev->cmd_set->init_regs_for_shading(dev, sensor, dev->calib_reg);

        sanei_usb_testing_record_message("genesys_dark_white_shading_calibration");
        genesys_dark_white_shading_calibration(dev, sensor);
    } else {
        DBG(DBG_proc, "%s : genesys_dark_shading_calibration dev->calib_reg ", __func__);
        debug_dump(DBG_proc, dev->calib_reg);

        if (dev->model->flags & GENESYS_FLAG_DARK_CALIBRATION) {
            sanei_usb_testing_record_message("init_regs_for_shading");
            dev->cmd_set->init_regs_for_shading(dev, sensor, dev->calib_reg);

            sanei_usb_testing_record_message("genesys_dark_shading_calibration");
            genesys_dark_shading_calibration(dev, sensor);
            genesys_repark_sensor_before_shading(dev);
        }

        sanei_usb_testing_record_message("init_regs_for_shading2");
        dev->cmd_set->init_regs_for_shading(dev, sensor, dev->calib_reg);

        sanei_usb_testing_record_message("genesys_white_shading_calibration");
        genesys_white_shading_calibration(dev, sensor);
        genesys_repark_sensor_after_white_shading(dev);

        if (!(dev->model->flags & GENESYS_FLAG_DARK_CALIBRATION)) {
            genesys_dummy_dark_shading(dev, sensor);
        }
    }

    if (!dev->cmd_set->has_send_shading_data()) {
      sanei_usb_testing_record_message("genesys_send_shading_coefficient");
        genesys_send_shading_coefficient(dev, sensor);
    }
}

/**
 * Does the calibration process for a sheetfed scanner
 * - offset calibration
 * - gain calibration
 * - shading calibration
 * During calibration a predefined calibration sheet with specific black and white
 * areas is used.
 * @param dev device to calibrate
 */
static void genesys_sheetfed_calibration(Genesys_Device* dev, Genesys_Sensor& sensor)
{
    DBG_HELPER(dbg);
    bool forward = true;

    // first step, load document
    dev->cmd_set->load_document(dev);

  /* led, offset and gain calibration are influenced by scan
   * settings. So we set it to sensor resolution */
  dev->settings.xres = sensor.optical_res;
  /* XP200 needs to calibrate a full and half sensor's resolution */
    if (dev->model->sensor_id == SensorId::CIS_XP200 &&
        dev->settings.xres <= sensor.optical_res / 2)
    {
        dev->settings.xres /= 2;
    }

  /* the afe needs to sends valid data even before calibration */

  /* go to a white area */
    try {
        dev->cmd_set->search_strip(dev, sensor, forward, false);
    } catch (...) {
        catch_all_exceptions(__func__, [&](){ dev->cmd_set->eject_document(dev); });
        throw;
    }

  if (dev->model->is_cis)
    {
        dev->cmd_set->led_calibration(dev, sensor, dev->calib_reg);
    }

  /* calibrate afe */
  if (dev->model->flags & GENESYS_FLAG_OFFSET_CALIBRATION)
    {
        dev->cmd_set->offset_calibration(dev, sensor, dev->calib_reg);

      /* since all the registers are set up correctly, just use them */

        dev->cmd_set->coarse_gain_calibration(dev, sensor, dev->calib_reg, sensor.optical_res);
    }
  else
    /* since we have 2 gain calibration proc, skip second if first one was
       used. */
    {
        dev->cmd_set->init_regs_for_coarse_calibration(dev, sensor, dev->calib_reg);

        genesys_coarse_calibration(dev, sensor);
    }

  /* search for a full width black strip and then do a 16 bit scan to
   * gather black shading data */
  if (dev->model->flags & GENESYS_FLAG_DARK_CALIBRATION)
    {
      /* seek black/white reverse/forward */
        try {
            dev->cmd_set->search_strip(dev, sensor, forward, true);
        } catch (...) {
            catch_all_exceptions(__func__, [&](){ dev->cmd_set->eject_document(dev); });
            throw;
        }

        dev->cmd_set->init_regs_for_shading(dev, sensor, dev->calib_reg);

        try {
            genesys_dark_shading_calibration(dev, sensor);
        } catch (...) {
            catch_all_exceptions(__func__, [&](){ dev->cmd_set->eject_document(dev); });
            throw;
        }
        forward = false;
    }


  /* go to a white area */
    try {
        dev->cmd_set->search_strip(dev, sensor, forward, false);
    } catch (...) {
        catch_all_exceptions(__func__, [&](){ dev->cmd_set->eject_document(dev); });
        throw;
    }

  genesys_repark_sensor_before_shading(dev);

    dev->cmd_set->init_regs_for_shading(dev, sensor, dev->calib_reg);

    try {
        genesys_white_shading_calibration(dev, sensor);
        genesys_repark_sensor_after_white_shading(dev);
    } catch (...) {
        catch_all_exceptions(__func__, [&](){ dev->cmd_set->eject_document(dev); });
        throw;
    }

    // in case we haven't black shading data, build it from black pixels of white calibration
    // FIXME: shouldn't we use genesys_dummy_dark_shading() ?
    if (!(dev->model->flags & GENESYS_FLAG_DARK_CALIBRATION)) {
      dev->dark_average_data.clear();
        dev->dark_average_data.resize(dev->average_size, 0x0f0f);
      /* XXX STEF XXX
       * with black point in white shading, build an average black
       * pixel and use it to fill the dark_average
       * dev->calib_pixels
       (sensor.sensor_pixels * dev->settings.xres) / sensor.optical_res,
       dev->calib_lines,
       */
    }

  /* send the shading coefficient when doing whole line shading
   * but not when using SHDAREA like GL124 */
    if (!dev->cmd_set->has_send_shading_data()) {
        genesys_send_shading_coefficient(dev, sensor);
    }

    // save the calibration data
    genesys_save_calibration(dev, sensor);

    // and finally eject calibration sheet
    dev->cmd_set->eject_document(dev);

    // restore settings
    dev->settings.xres = sensor.optical_res;
}

/**
 * does the calibration process for a device
 * @param dev device to calibrate
 */
static void genesys_scanner_calibration(Genesys_Device* dev, Genesys_Sensor& sensor)
{
    DBG_HELPER(dbg);
    if (!dev->model->is_sheetfed) {
        genesys_flatbed_calibration(dev, sensor);
        return;
    }
    genesys_sheetfed_calibration(dev, sensor);
}


/* ------------------------------------------------------------------------ */
/*                  High level (exported) functions                         */
/* ------------------------------------------------------------------------ */

/*
 * wait lamp to be warm enough by scanning the same line until
 * differences between two scans are below a threshold
 */
static void genesys_warmup_lamp(Genesys_Device* dev)
{
    DBG_HELPER(dbg);
  int seconds = 0;
  int pixel;
  int channels, total_size;
  double first_average = 0;
  double second_average = 0;
  int difference = 255;
    int lines = 3;

  const auto& sensor = sanei_genesys_find_sensor_any(dev);

    dev->cmd_set->init_regs_for_warmup(dev, sensor, &dev->reg, &channels, &total_size);
  std::vector<uint8_t> first_line(total_size);
  std::vector<uint8_t> second_line(total_size);

  do
    {
      DBG(DBG_info, "%s: one more loop\n", __func__);
        dev->cmd_set->begin_scan(dev, sensor, &dev->reg, false);

        wait_until_buffer_non_empty(dev);

        try {
            sanei_genesys_read_data_from_scanner(dev, first_line.data(), total_size);
        } catch (...) {
            // FIXME: document why this retry is here
            sanei_genesys_read_data_from_scanner(dev, first_line.data(), total_size);
        }

        dev->cmd_set->end_scan(dev, &dev->reg, true);

      sanei_genesys_sleep_ms(1000);
      seconds++;

        dev->cmd_set->begin_scan(dev, sensor, &dev->reg, false);

        wait_until_buffer_non_empty(dev);

        sanei_genesys_read_data_from_scanner(dev, second_line.data(), total_size);
        dev->cmd_set->end_scan(dev, &dev->reg, true);

      /* compute difference between the two scans */
      for (pixel = 0; pixel < total_size; pixel++)
	{
            // 16 bit data
            if (dev->cmd_set->get_bitset_bit(&dev->reg)) {
	      first_average += (first_line[pixel] + first_line[pixel + 1] * 256);
	      second_average += (second_line[pixel] + second_line[pixel + 1] * 256);
	      pixel++;
	    }
	  else
	    {
	      first_average += first_line[pixel];
	      second_average += second_line[pixel];
	    }
	}
        if (dev->cmd_set->get_bitset_bit(&dev->reg)) {
	  first_average /= pixel;
	  second_average /= pixel;
	  difference = fabs (first_average - second_average);
	  DBG(DBG_info, "%s: average = %.2f, diff = %.3f\n", __func__,
	      100 * ((second_average) / (256 * 256)),
	      100 * (difference / second_average));

	  if (second_average > (100 * 256)
	      && (difference / second_average) < 0.002)
	    break;
	}
      else
	{
	  first_average /= pixel;
	  second_average /= pixel;
	  if (DBG_LEVEL >= DBG_data)
	    {
              sanei_genesys_write_pnm_file("gl_warmup1.pnm", first_line.data(), 8, channels,
                                           total_size / (lines * channels), lines);
              sanei_genesys_write_pnm_file("gl_warmup2.pnm", second_line.data(), 8, channels,
                                           total_size / (lines * channels), lines);
	    }
	  DBG(DBG_info, "%s: average 1 = %.2f, average 2 = %.2f\n", __func__, first_average,
	      second_average);
          /* if delta below 15/255 ~= 5.8%, lamp is considred warm enough */
	  if (fabs (first_average - second_average) < 15
	      && second_average > 55)
	    break;
	}

      /* sleep another second before next loop */
      sanei_genesys_sleep_ms(1000);
      seconds++;
    }
  while (seconds < WARMUP_TIME);

  if (seconds >= WARMUP_TIME)
    {
        throw SaneException(SANE_STATUS_IO_ERROR,
                            "warmup timed out after %d seconds. Lamp defective?", seconds);
    }
  else
    {
      DBG(DBG_info, "%s: warmup succeeded after %d seconds\n", __func__, seconds);
    }
}


// High-level start of scanning
static void genesys_start_scan(Genesys_Device* dev, bool lamp_off)
{
    DBG_HELPER(dbg);
  unsigned int steps, expected;

  /* since not all scanners are set ot wait for head to park
   * we check we are not still parking before starting a new scan */
    if (dev->parking) {
        sanei_genesys_wait_for_home(dev);
    }

    // disable power saving
    dev->cmd_set->save_power(dev, false);

  /* wait for lamp warmup : until a warmup for TRANSPARENCY is designed, skip
   * it when scanning from XPA. */
  if (!(dev->model->flags & GENESYS_FLAG_SKIP_WARMUP)
    && (dev->settings.scan_method == ScanMethod::FLATBED))
    {
        genesys_warmup_lamp(dev);
    }

  /* set top left x and y values by scanning the internals if flatbed scanners */
    if (!dev->model->is_sheetfed) {
      /* do the geometry detection only once */
      if ((dev->model->flags & GENESYS_FLAG_SEARCH_START)
      && (dev->model->y_offset_calib_white == 0))
	{
        dev->cmd_set->search_start_position (dev);

            dev->parking = false;
            dev->cmd_set->slow_back_home (dev, true);
	  dev->scanhead_position_in_steps = 0;
	}
      else
	{
	  /* Go home */
	  /* TODO: check we can drop this since we cannot have the
	     scanner's head wandering here */
            dev->parking = false;
            dev->cmd_set->slow_back_home (dev, true);

	  dev->scanhead_position_in_steps = 0;
	}
    }

  /* move to calibration area for transparency adapter */
    if (dev->settings.scan_method == ScanMethod::TRANSPARENCY ||
        dev->settings.scan_method == ScanMethod::TRANSPARENCY_INFRARED)
    {
        dev->cmd_set->move_to_ta(dev);
    }

  /* load document if needed (for sheetfed scanner for instance) */
    if (dev->model->is_sheetfed) {
        dev->cmd_set->load_document(dev);
    }

    auto& sensor = sanei_genesys_find_sensor_for_write(dev, dev->settings.xres,
                                                       dev->settings.get_channels(),
                                                       dev->settings.scan_method);

    // send gamma tables. They have been set to device or user value
    // when setting option value */
    dev->cmd_set->send_gamma_table(dev, sensor);

  /* try to use cached calibration first */
  if (!genesys_restore_calibration (dev, sensor))
    {
       /* calibration : sheetfed scanners can't calibrate before each scan */
       /* and also those who have the NO_CALIBRATION flag                  */
        if (!(dev->model->flags & GENESYS_FLAG_NO_CALIBRATION) && !dev->model->is_sheetfed) {
            genesys_scanner_calibration(dev, sensor);
          genesys_save_calibration (dev, sensor);
	}
      else
	{
          DBG(DBG_warn, "%s: no calibration done\n", __func__);
	}
    }

  /* build look up table for dynamic lineart */
    if (dev->settings.dynamic_lineart) {
        sanei_genesys_load_lut(dev->lineart_lut, 8, 8, 50, 205, dev->settings.threshold_curve,
                               dev->settings.threshold-127);
    }

    dev->cmd_set->wait_for_motor_stop(dev);

    if (dev->cmd_set->needs_home_before_init_regs_for_scan(dev)) {
        dev->cmd_set->slow_back_home(dev, true);
    }

    if (dev->settings.scan_method == ScanMethod::TRANSPARENCY ||
        dev->settings.scan_method == ScanMethod::TRANSPARENCY_INFRARED)
    {
        dev->cmd_set->move_to_ta(dev);
    }

    dev->cmd_set->init_regs_for_scan(dev, sensor);

  /* no lamp during scan */
    if (lamp_off) {
        sanei_genesys_set_lamp_power(dev, sensor, dev->reg, false);
    }

  /* GL124 is using SHDAREA, so we have to wait for scan to be set up before
   * sending shading data */
    if (dev->cmd_set->has_send_shading_data() &&
        !(dev->model->flags & GENESYS_FLAG_NO_CALIBRATION))
    {
        genesys_send_shading_coefficient(dev, sensor);
    }

    // now send registers for scan
    dev->write_registers(dev->reg);

    // start effective scan
    dev->cmd_set->begin_scan(dev, sensor, &dev->reg, true);

  /*do we really need this? the valid data check should be sufficent -- pierre*/
  /* waits for head to reach scanning position */
  expected = dev->reg.get8(0x3d) * 65536
           + dev->reg.get8(0x3e) * 256
           + dev->reg.get8(0x3f);
  do
    {
      // wait some time between each test to avoid overloading USB and CPU
      sanei_genesys_sleep_ms(100);
        sanei_genesys_read_feed_steps (dev, &steps);
    }
  while (steps < expected);

    wait_until_buffer_non_empty(dev);

    // we wait for at least one word of valid scan data
    // this is also done in sanei_genesys_read_data_from_scanner -- pierre
    if (!dev->model->is_sheetfed) {
      do
	{
          sanei_genesys_sleep_ms(100);
            sanei_genesys_read_valid_words(dev, &steps);
	}
      while (steps < 1);
    }
}

static void genesys_fill_read_buffer(Genesys_Device* dev)
{
    DBG_HELPER(dbg);

  /* for sheetfed scanner, we must check is document is shorter than
   * the requested scan */
    if (dev->model->is_sheetfed) {
        dev->cmd_set->detect_document_end(dev);
    }

    std::size_t size = dev->read_buffer.size() - dev->read_buffer.avail();

  /* due to sensors and motors, not all data can be directly used. It
   * may have to be read from another intermediate buffer and then processed.
   * There are currently 3 intermediate stages:
   * - handling of odd/even sensors
   * - handling of line interpolation for motors that can't have low
   *   enough dpi
   * - handling of multi-segments sensors
   *
   * This is also the place where full duplex data will be handled.
   */
    dev->pipeline_buffer.get_data(size, dev->read_buffer.get_write_pos(size));

    dev->read_buffer.produce(size);
}

/* this function does the effective data read in a manner that suits
   the scanner. It does data reordering and resizing if need.
   It also manages EOF and I/O errors, and line distance correction.
    Returns true on success, false on end-of-file.
*/
static void genesys_read_ordered_data(Genesys_Device* dev, SANE_Byte* destination, size_t* len)
{
    DBG_HELPER(dbg);
  size_t bytes;
  uint8_t *work_buffer_src;
  Genesys_Buffer *src_buffer;

    if (!dev->read_active) {
      *len = 0;
        throw SaneException("read is not active");
    }

    debug_dump(DBG_info, dev->current_setup);
    debug_dump(DBG_info, dev->session.params);

    DBG(DBG_info, "%s: frontend requested %zu bytes\n", __func__, *len);
    DBG(DBG_info, "%s: bytes_to_read=%zu, total_bytes_read=%zu\n", __func__,
        dev->total_bytes_to_read, dev->total_bytes_read);

  /* is there data left to scan */
  if (dev->total_bytes_read >= dev->total_bytes_to_read)
    {
      /* issue park command immediatly in case scanner can handle it
       * so we save time */
        if (!dev->model->is_sheetfed && !(dev->model->flags & GENESYS_FLAG_MUST_WAIT) &&
            !dev->parking)
        {
            dev->cmd_set->slow_back_home(dev, false);
            dev->parking = true;
        }
        throw SaneException(SANE_STATUS_EOF, "nothing more to scan: EOF");
    }

/* convert data */
/*
  0. fill_read_buffer
-------------- read_buffer ----------------------
  1a). (opt)uncis                    (assumes color components to be laid out
                                    planar)
  1b). (opt)reverse_RGB              (assumes pixels to be BGR or BBGGRR))
-------------- lines_buffer ----------------------
  2a). (opt)line_distance_correction (assumes RGB or RRGGBB)
  2b). (opt)unstagger                (assumes pixels to be depth*channels/8
                                      bytes long, unshrinked)
------------- shrink_buffer ---------------------
  3. (opt)shrink_lines             (assumes component separation in pixels)
-------------- out_buffer -----------------------
  4. memcpy to destination (for lineart with bit reversal)
*/
/*FIXME: for lineart we need sub byte addressing in buffers, or conversion to
  bytes at 0. and back to bits at 4.
Problems with the first approach:
  - its not clear how to check if we need to output an incomplete byte
    because it is the last one.
 */
/*FIXME: add lineart support for gl646. in the meantime add logic to convert
  from gray to lineart at the end? would suffer the above problem,
  total_bytes_to_read and total_bytes_read help in that case.
 */

    genesys_fill_read_buffer(dev);

  src_buffer = &(dev->read_buffer);

  /* move data to destination */
  bytes = src_buffer->avail();
  if (bytes > *len)
    bytes = *len;
  work_buffer_src = src_buffer->get_read_pos();

    if (dev->session.pipeline_needs_reverse) {
        genesys_reverse_bits(work_buffer_src, destination, bytes);
      *len = bytes;
    }
  else
    {
      memcpy (destination, work_buffer_src, bytes);
      *len = bytes;
    }

  /* avoid signaling some extra data because we have treated a full block
   * on the last block */
  if (dev->total_bytes_read + *len > dev->total_bytes_to_read)
    *len = dev->total_bytes_to_read - dev->total_bytes_read;

  /* count bytes sent to frontend */
  dev->total_bytes_read += *len;

    src_buffer->consume(bytes);

  /* end scan if all needed data have been read */
   if(dev->total_bytes_read >= dev->total_bytes_to_read)
    {
        dev->cmd_set->end_scan(dev, &dev->reg, true);
        if (dev->model->is_sheetfed) {
            dev->cmd_set->eject_document (dev);
        }
    }

    DBG(DBG_proc, "%s: completed, %zu bytes read\n", __func__, bytes);
}



/* ------------------------------------------------------------------------ */
/*                  Start of higher level functions                         */
/* ------------------------------------------------------------------------ */

static size_t
max_string_size (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  SANE_Int i;

  for (i = 0; strings[i]; ++i)
    {
      size = strlen (strings[i]) + 1;
      if (size > max_size)
	max_size = size;
    }
  return max_size;
}

static unsigned pick_resolution(const std::vector<unsigned>& resolutions, unsigned resolution,
                                const char* direction)
{
    DBG_HELPER(dbg);

    if (resolutions.empty())
        throw SaneException("Empty resolution list");

    unsigned best_res = resolutions.front();
    unsigned min_diff = abs_diff(best_res, resolution);

    for (auto it = std::next(resolutions.begin()); it != resolutions.end(); ++it) {
        unsigned curr_diff = abs_diff(*it, resolution);
        if (curr_diff < min_diff) {
            min_diff = curr_diff;
            best_res = *it;
        }
    }

    if (best_res != resolution) {
        DBG(DBG_warn, "%s: using resolution %d that is nearest to %d for direction %s\n",
            __func__, best_res, resolution, direction);
    }
    return best_res;
}

static void calc_parameters(Genesys_Scanner* s)
{
    DBG_HELPER(dbg);
  double tl_x = 0, tl_y = 0, br_x = 0, br_y = 0;

    tl_x = SANE_UNFIX(s->pos_top_left_x);
    tl_y = SANE_UNFIX(s->pos_top_left_y);
    br_x = SANE_UNFIX(s->pos_bottom_right_x);
    br_y = SANE_UNFIX(s->pos_bottom_right_y);

    s->params.last_frame = true;	/* only single pass scanning supported */

    if (s->mode == SANE_VALUE_SCAN_MODE_GRAY || s->mode == SANE_VALUE_SCAN_MODE_LINEART) {
        s->params.format = SANE_FRAME_GRAY;
    } else {
        s->params.format = SANE_FRAME_RGB;
    }

    if (s->mode == SANE_VALUE_SCAN_MODE_LINEART) {
        s->params.depth = 1;
    } else {
        s->params.depth = s->bit_depth;
    }

    s->dev->settings.depth = s->bit_depth;

  /* interpolation */
  s->dev->settings.disable_interpolation = s->disable_interpolation;

  // FIXME: use correct sensor
  const auto& sensor = sanei_genesys_find_sensor_any(s->dev);

    // hardware settings
    if (s->resolution > sensor.optical_res && s->dev->settings.disable_interpolation) {
        s->dev->settings.xres = sensor.optical_res;
    } else {
        s->dev->settings.xres = s->resolution;
    }
    s->dev->settings.yres = s->resolution;

    s->dev->settings.xres = pick_resolution(s->dev->model->xdpi_values, s->dev->settings.xres, "X");
    s->dev->settings.yres = pick_resolution(s->dev->model->ydpi_values, s->dev->settings.yres, "Y");

    s->params.lines = ((br_y - tl_y) * s->dev->settings.yres) / MM_PER_INCH;
    unsigned pixels_per_line = ((br_x - tl_x) * s->dev->settings.xres) / MM_PER_INCH;

  /* we need an even pixels number
   * TODO invert test logic or generalize behaviour across all ASICs */
    if ((s->dev->model->flags & GENESYS_FLAG_SIS_SENSOR) ||
        s->dev->model->asic_type == AsicType::GL847 ||
        s->dev->model->asic_type == AsicType::GL124 ||
        s->dev->model->asic_type == AsicType::GL845 ||
        s->dev->model->asic_type == AsicType::GL846 ||
        s->dev->model->asic_type == AsicType::GL843)
    {
        if (s->dev->settings.xres <= 1200) {
            pixels_per_line = (pixels_per_line / 4) * 4;
        } else if (s->dev->settings.xres < s->dev->settings.yres) {
            // BUG: this is an artifact of the fact that the resolution was twice as large than
            // the actual resolution when scanning above the supported scanner X resolution
            pixels_per_line = (pixels_per_line / 8) * 8;
        } else {
            pixels_per_line = (pixels_per_line / 16) * 16;
        }
    }

  /* corner case for true lineart for sensor with several segments
   * or when xres is doubled to match yres */
    if (s->dev->settings.xres >= 1200 && (
                s->dev->model->asic_type == AsicType::GL124 ||
                s->dev->model->asic_type == AsicType::GL847 ||
                s->dev->current_setup.xres < s->dev->session.params.yres))
    {
        if (s->dev->settings.xres < s->dev->settings.yres) {
            // FIXME: this is an artifact of the fact that the resolution was twice as large than
            // the actual resolution when scanning above the supported scanner X resolution
            pixels_per_line = (pixels_per_line / 8) * 8;
        } else {
            pixels_per_line = (pixels_per_line / 16) * 16;
        }
    }

    unsigned xres_factor = s->resolution / s->dev->settings.xres;

    unsigned bytes_per_line = 0;

  if (s->params.depth > 8)
    {
      s->params.depth = 16;
        bytes_per_line = 2 * pixels_per_line;
    }
  else if (s->params.depth == 1)
    {
        // round down pixel number. This will is lossy operation, at most 7 pixels will be lost
        pixels_per_line = (pixels_per_line / 8) * 8;
        bytes_per_line = pixels_per_line / 8;
    } else {
        bytes_per_line = pixels_per_line;
    }

    if (s->params.format == SANE_FRAME_RGB) {
        bytes_per_line *= 3;
    }

    if (s->mode == SANE_VALUE_SCAN_MODE_COLOR) {
        s->dev->settings.scan_mode = ScanColorMode::COLOR_SINGLE_PASS;
    } else if (s->mode == SANE_VALUE_SCAN_MODE_GRAY) {
        s->dev->settings.scan_mode = ScanColorMode::GRAY;
    } else if (s->mode == SANE_TITLE_HALFTONE) {
        s->dev->settings.scan_mode = ScanColorMode::HALFTONE;
    } else {				/* Lineart */
        s->dev->settings.scan_mode = ScanColorMode::LINEART;
    }

    if (s->source == STR_FLATBED) {
        s->dev->settings.scan_method = ScanMethod::FLATBED;
    } else if (s->source == STR_TRANSPARENCY_ADAPTER) {
        s->dev->settings.scan_method = ScanMethod::TRANSPARENCY;
    } else if (s->source == STR_TRANSPARENCY_ADAPTER_INFRARED) {
        s->dev->settings.scan_method = ScanMethod::TRANSPARENCY_INFRARED;
    }

  s->dev->settings.lines = s->params.lines;
    s->dev->settings.pixels = pixels_per_line;
    s->dev->settings.requested_pixels = pixels_per_line * xres_factor;
    s->params.pixels_per_line = pixels_per_line * xres_factor;
    s->params.bytes_per_line = bytes_per_line * xres_factor;
  s->dev->settings.tl_x = tl_x;
  s->dev->settings.tl_y = tl_y;

    // threshold setting
    s->dev->settings.threshold = 2.55 * (SANE_UNFIX(s->threshold));

    // color filter
    if (s->color_filter == "Red") {
        s->dev->settings.color_filter = ColorFilter::RED;
    } else if (s->color_filter == "Green") {
        s->dev->settings.color_filter = ColorFilter::GREEN;
    } else if (s->color_filter == "Blue") {
        s->dev->settings.color_filter = ColorFilter::BLUE;
    } else {
        s->dev->settings.color_filter = ColorFilter::NONE;
    }

    // true gray
    if (s->color_filter == "None") {
        s->dev->settings.true_gray = 1;
    } else {
        s->dev->settings.true_gray = 0;
    }

  /* dynamic lineart */
    s->dev->settings.dynamic_lineart = false;
  s->dev->settings.threshold_curve=0;
    if (s->dev->settings.scan_mode == ScanColorMode::LINEART) {
        s->dev->settings.dynamic_lineart = true;
    }

    // threshold curve for dynamic rasterization
    s->dev->settings.threshold_curve = s->threshold_curve;

  /* some digital processing requires the whole picture to be buffered */
  /* no digital processing takes place when doing preview, or when bit depth is
   * higher than 8 bits */
  if ((s->swdespeck || s->swcrop || s->swdeskew || s->swderotate ||(SANE_UNFIX(s->swskip)>0))
    && (!s->preview)
    && (s->bit_depth <= 8))
    {
        s->dev->buffer_image = true;
    }
  else
    {
        s->dev->buffer_image = false;
    }

  /* brigthness and contrast only for for 8 bit scans */
  if(s->bit_depth <= 8)
    {
      s->dev->settings.contrast = (s->contrast * 127) / 100;
      s->dev->settings.brightness = (s->brightness * 127) / 100;
    }
  else
    {
      s->dev->settings.contrast=0;
      s->dev->settings.brightness=0;
    }

  /* cache expiration time */
   s->dev->settings.expiration_time = s->expiration_time;
}


static void create_bpp_list (Genesys_Scanner * s, const std::vector<unsigned>& bpp)
{
    s->bpp_list[0] = bpp.size();
    std::reverse_copy(bpp.begin(), bpp.end(), s->bpp_list + 1);
}

/** @brief this function initialize a gamma vector based on the ASIC:
 * Set up a default gamma table vector based on device description
 * gl646: 12 or 14 bits gamma table depending on GENESYS_FLAG_14BIT_GAMMA
 * gl84x: 16 bits
 * gl12x: 16 bits
 * @param scanner pointer to scanner session to get options
 * @param option option number of the gamma table to set
 */
static void
init_gamma_vector_option (Genesys_Scanner * scanner, int option)
{
  /* the option is inactive until the custom gamma control
   * is enabled */
  scanner->opt[option].type = SANE_TYPE_INT;
  scanner->opt[option].cap |= SANE_CAP_INACTIVE | SANE_CAP_ADVANCED;
  scanner->opt[option].unit = SANE_UNIT_NONE;
  scanner->opt[option].constraint_type = SANE_CONSTRAINT_RANGE;
    if (scanner->dev->model->asic_type == AsicType::GL646) {
      if ((scanner->dev->model->flags & GENESYS_FLAG_14BIT_GAMMA) != 0)
	{
	  scanner->opt[option].size = 16384 * sizeof (SANE_Word);
	  scanner->opt[option].constraint.range = &u14_range;
	}
      else
	{			/* 12 bits gamma tables */
	  scanner->opt[option].size = 4096 * sizeof (SANE_Word);
	  scanner->opt[option].constraint.range = &u12_range;
	}
    }
  else
    {				/* other asics have 16 bits words gamma table */
      scanner->opt[option].size = 256 * sizeof (SANE_Word);
      scanner->opt[option].constraint.range = &u16_range;
    }
}

/**
 * allocate a geometry range
 * @param size maximum size of the range
 * @return a pointer to a valid range or nullptr
 */
static SANE_Range* create_range(float size)
{
    SANE_Range* range = reinterpret_cast<SANE_Range*>(std::malloc(sizeof(SANE_Range)));
    if (range != nullptr) {
      range->min = SANE_FIX (0.0);
        range->max = SANE_FIX(size);
      range->quant = SANE_FIX (0.0);
    }
  return range;
}

/** @brief generate calibration cache file nam
 * Generates the calibration cache file name to use.
 * Tries to store the chache in $HOME/.sane or
 * then fallbacks to $TMPDIR or TMP. The filename
 * uses the model name if only one scanner is plugged
 * else is uses the device name when several identical
 * scanners are in use.
 * @param currdev current scanner device
 * @return an allocated string containing a file name
 */
static std::string calibration_filename(Genesys_Device *currdev)
{
    std::string ret;
    ret.resize(PATH_MAX);

  char filename[80];
  unsigned int count;
  unsigned int i;

  /* first compute the DIR where we can store cache:
   * 1 - home dir
   * 2 - $TMPDIR
   * 3 - $TMP
   * 4 - tmp dir
   * 5 - temp dir
   * 6 - then resort to current dir
   */
    char* ptr = std::getenv("HOME");
    if (ptr == nullptr) {
        ptr = std::getenv("USERPROFILE");
    }
    if (ptr == nullptr) {
        ptr = std::getenv("TMPDIR");
    }
    if (ptr == nullptr) {
        ptr = std::getenv("TMP");
    }

  /* now choose filename:
   * 1 - if only one scanner, name of the model
   * 2 - if several scanners of the same model, use device name,
   *     replacing special chars
   */
  count=0;
  /* count models of the same names if several scanners attached */
    if(s_devices->size() > 1) {
        for (const auto& dev : *s_devices) {
            if (dev.model->model_id == currdev->model->model_id) {
                count++;
            }
        }
    }
  if(count>1)
    {
        std::snprintf(filename, sizeof(filename), "%s.cal", currdev->file_name.c_str());
      for(i=0;i<strlen(filename);i++)
        {
          if(filename[i]==':'||filename[i]==PATH_SEP)
            {
              filename[i]='_';
            }
        }
    }
  else
    {
      snprintf(filename,sizeof(filename),"%s.cal",currdev->model->name);
    }

  /* build final final name : store dir + filename */
    if (ptr == nullptr) {
        int size = std::snprintf(&ret.front(), ret.size(), "%s", filename);
        ret.resize(size);
    }
  else
    {
        int size = 0;
#ifdef HAVE_MKDIR
        /* make sure .sane directory exists in existing store dir */
        size = std::snprintf(&ret.front(), ret.size(), "%s%c.sane", ptr, PATH_SEP);
        ret.resize(size);
        mkdir(ret.c_str(), 0700);

        ret.resize(PATH_MAX);
#endif
        size = std::snprintf(&ret.front(), ret.size(), "%s%c.sane%c%s",
                             ptr, PATH_SEP, PATH_SEP, filename);
        ret.resize(size);
    }

    DBG(DBG_info, "%s: calibration filename >%s<\n", __func__, ret.c_str());

    return ret;
}


static void init_options(Genesys_Scanner* s)
{
    DBG_HELPER(dbg);
  SANE_Int option;
  SANE_Word *dpi_list;
  Genesys_Model *model = s->dev->model;
  SANE_Range *x_range, *y_range;

  memset (s->opt, 0, sizeof (s->opt));

  for (option = 0; option < NUM_OPTIONS; ++option)
    {
      s->opt[option].size = sizeof (SANE_Word);
      s->opt[option].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }
  s->opt[OPT_NUM_OPTS].name = SANE_NAME_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;

  /* "Mode" group: */
  s->opt[OPT_MODE_GROUP].name = "scanmode-group";
  s->opt[OPT_MODE_GROUP].title = SANE_I18N ("Scan Mode");
  s->opt[OPT_MODE_GROUP].desc = "";
  s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MODE_GROUP].size = 0;
  s->opt[OPT_MODE_GROUP].cap = 0;
  s->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* scan mode */
  s->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  s->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  s->opt[OPT_MODE].type = SANE_TYPE_STRING;
  s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_MODE].size = max_string_size (mode_list);
  s->opt[OPT_MODE].constraint.string_list = mode_list;
  s->mode = SANE_VALUE_SCAN_MODE_GRAY;

  /* scan source */
  s->opt[OPT_SOURCE].name = SANE_NAME_SCAN_SOURCE;
  s->opt[OPT_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
  s->opt[OPT_SOURCE].desc = SANE_DESC_SCAN_SOURCE;
  s->opt[OPT_SOURCE].type = SANE_TYPE_STRING;
  s->opt[OPT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_SOURCE].size = max_string_size (source_list);
  s->opt[OPT_SOURCE].constraint.string_list = source_list;
  s->source = STR_FLATBED;
  if (model->flags & GENESYS_FLAG_HAS_UTA)
    {
        ENABLE (OPT_SOURCE);
        if (model->flags & GENESYS_FLAG_HAS_UTA_INFRARED) {
            s->opt[OPT_SOURCE].size = max_string_size(source_list_infrared);
            s->opt[OPT_SOURCE].constraint.string_list = source_list_infrared;
        }
    }
  else
    {
      DISABLE (OPT_SOURCE);
    }

  /* preview */
  s->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  s->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  s->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  s->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;
  s->opt[OPT_PREVIEW].unit = SANE_UNIT_NONE;
  s->opt[OPT_PREVIEW].constraint_type = SANE_CONSTRAINT_NONE;
  s->preview = false;

  /* bit depth */
  s->opt[OPT_BIT_DEPTH].name = SANE_NAME_BIT_DEPTH;
  s->opt[OPT_BIT_DEPTH].title = SANE_TITLE_BIT_DEPTH;
  s->opt[OPT_BIT_DEPTH].desc = SANE_DESC_BIT_DEPTH;
  s->opt[OPT_BIT_DEPTH].type = SANE_TYPE_INT;
  s->opt[OPT_BIT_DEPTH].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->opt[OPT_BIT_DEPTH].size = sizeof (SANE_Word);
  s->opt[OPT_BIT_DEPTH].constraint.word_list = 0;
  s->opt[OPT_BIT_DEPTH].constraint.word_list = s->bpp_list;
  create_bpp_list (s, model->bpp_gray_values);
    s->bit_depth = model->bpp_gray_values[0];

    // resolution
    auto resolutions = model->get_resolutions();

    unsigned min_dpi = *std::min_element(resolutions.begin(), resolutions.end());

    dpi_list = reinterpret_cast<SANE_Word*>(std::malloc((resolutions.size() + 1) * sizeof(SANE_Word)));
    if (!dpi_list) {
        throw SaneException(SANE_STATUS_NO_MEM);
    }
    dpi_list[0] = resolutions.size();
    std::copy(resolutions.begin(), resolutions.end(), dpi_list + 1);

  s->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->opt[OPT_RESOLUTION].constraint.word_list = dpi_list;
  s->resolution = min_dpi;

  /* "Geometry" group: */
  s->opt[OPT_GEOMETRY_GROUP].name = SANE_NAME_GEOMETRY;
  s->opt[OPT_GEOMETRY_GROUP].title = SANE_I18N ("Geometry");
  s->opt[OPT_GEOMETRY_GROUP].desc = "";
  s->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_GEOMETRY_GROUP].size = 0;
  s->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

    x_range = create_range(model->x_size);
    if (x_range == nullptr) {
        throw SaneException(SANE_STATUS_NO_MEM);
    }

    y_range = create_range(model->y_size);
    if (y_range == nullptr) {
        throw SaneException(SANE_STATUS_NO_MEM);
    }

  /* top-left x */
  s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  s->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_X].constraint.range = x_range;
  s->pos_top_left_x = 0;

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = y_range;
  s->pos_top_left_y = 0;

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = x_range;
  s->pos_bottom_right_x = x_range->max;

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = y_range;
  s->pos_bottom_right_y = y_range->max;

  /* "Enhancement" group: */
  s->opt[OPT_ENHANCEMENT_GROUP].name = SANE_NAME_ENHANCEMENT;
  s->opt[OPT_ENHANCEMENT_GROUP].title = SANE_I18N ("Enhancement");
  s->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  s->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ENHANCEMENT_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_ENHANCEMENT_GROUP].size = 0;
  s->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* custom-gamma table */
  s->opt[OPT_CUSTOM_GAMMA].name = SANE_NAME_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].title = SANE_TITLE_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].desc = SANE_DESC_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].type = SANE_TYPE_BOOL;
  s->opt[OPT_CUSTOM_GAMMA].cap |= SANE_CAP_ADVANCED;
  s->custom_gamma = false;

  /* grayscale gamma vector */
  s->opt[OPT_GAMMA_VECTOR].name = SANE_NAME_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR].title = SANE_TITLE_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR].desc = SANE_DESC_GAMMA_VECTOR;
  init_gamma_vector_option (s, OPT_GAMMA_VECTOR);

  /* red gamma vector */
  s->opt[OPT_GAMMA_VECTOR_R].name = SANE_NAME_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].title = SANE_TITLE_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].desc = SANE_DESC_GAMMA_VECTOR_R;
  init_gamma_vector_option (s, OPT_GAMMA_VECTOR_R);

  /* green gamma vector */
  s->opt[OPT_GAMMA_VECTOR_G].name = SANE_NAME_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].title = SANE_TITLE_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].desc = SANE_DESC_GAMMA_VECTOR_G;
  init_gamma_vector_option (s, OPT_GAMMA_VECTOR_G);

  /* blue gamma vector */
  s->opt[OPT_GAMMA_VECTOR_B].name = SANE_NAME_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].title = SANE_TITLE_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].desc = SANE_DESC_GAMMA_VECTOR_B;
  init_gamma_vector_option (s, OPT_GAMMA_VECTOR_B);

  /* currently, there are only gamma table options in this group,
   * so if the scanner doesn't support gamma table, disable the
   * whole group */
  if (!(model->flags & GENESYS_FLAG_CUSTOM_GAMMA))
    {
      s->opt[OPT_ENHANCEMENT_GROUP].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_CUSTOM_GAMMA].cap |= SANE_CAP_INACTIVE;
      DBG(DBG_info, "%s: custom gamma disabled\n", __func__);
    }

  /* software base image enhancements, these are consuming as many
   * memory than used by the full scanned image and may fail at high
   * resolution
   */
  /* software deskew */
  s->opt[OPT_SWDESKEW].name = "swdeskew";
  s->opt[OPT_SWDESKEW].title = "Software deskew";
  s->opt[OPT_SWDESKEW].desc = "Request backend to rotate skewed pages digitally";
  s->opt[OPT_SWDESKEW].type = SANE_TYPE_BOOL;
  s->opt[OPT_SWDESKEW].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
  s->swdeskew = false;

  /* software deskew */
  s->opt[OPT_SWDESPECK].name = "swdespeck";
  s->opt[OPT_SWDESPECK].title = "Software despeck";
  s->opt[OPT_SWDESPECK].desc = "Request backend to remove lone dots digitally";
  s->opt[OPT_SWDESPECK].type = SANE_TYPE_BOOL;
  s->opt[OPT_SWDESPECK].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
  s->swdespeck = false;

  /* software despeckle radius */
  s->opt[OPT_DESPECK].name = "despeck";
  s->opt[OPT_DESPECK].title = "Software despeckle diameter";
  s->opt[OPT_DESPECK].desc = "Maximum diameter of lone dots to remove from scan";
  s->opt[OPT_DESPECK].type = SANE_TYPE_INT;
  s->opt[OPT_DESPECK].unit = SANE_UNIT_NONE;
  s->opt[OPT_DESPECK].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_DESPECK].constraint.range = &swdespeck_range;
  s->opt[OPT_DESPECK].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED | SANE_CAP_INACTIVE;
  s->despeck = 1;

  /* crop by software */
  s->opt[OPT_SWCROP].name = "swcrop";
  s->opt[OPT_SWCROP].title = SANE_I18N ("Software crop");
  s->opt[OPT_SWCROP].desc = SANE_I18N ("Request backend to remove border from pages digitally");
  s->opt[OPT_SWCROP].type = SANE_TYPE_BOOL;
  s->opt[OPT_SWCROP].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
  s->opt[OPT_SWCROP].unit = SANE_UNIT_NONE;
  s->swcrop = false;

  /* Software blank page skip */
  s->opt[OPT_SWSKIP].name = "swskip";
  s->opt[OPT_SWSKIP].title = SANE_I18N ("Software blank skip percentage");
  s->opt[OPT_SWSKIP].desc = SANE_I18N("Request driver to discard pages with low numbers of dark pixels");
  s->opt[OPT_SWSKIP].type = SANE_TYPE_FIXED;
  s->opt[OPT_SWSKIP].unit = SANE_UNIT_PERCENT;
  s->opt[OPT_SWSKIP].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_SWSKIP].constraint.range = &(percentage_range);
  s->opt[OPT_SWSKIP].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
  s->swskip = 0;    // disable by default

  /* Software Derotate */
  s->opt[OPT_SWDEROTATE].name = "swderotate";
  s->opt[OPT_SWDEROTATE].title = SANE_I18N ("Software derotate");
  s->opt[OPT_SWDEROTATE].desc = SANE_I18N("Request driver to detect and correct 90 degree image rotation");
  s->opt[OPT_SWDEROTATE].type = SANE_TYPE_BOOL;
  s->opt[OPT_SWDEROTATE].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;
  s->opt[OPT_SWDEROTATE].unit = SANE_UNIT_NONE;
  s->swderotate = false;

  /* Software brightness */
  s->opt[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].type = SANE_TYPE_INT;
  s->opt[OPT_BRIGHTNESS].unit = SANE_UNIT_NONE;
  s->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BRIGHTNESS].constraint.range = &(enhance_range);
  s->opt[OPT_BRIGHTNESS].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->brightness = 0;    // disable by default

  /* Sowftware contrast */
  s->opt[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  s->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  s->opt[OPT_CONTRAST].desc = SANE_DESC_CONTRAST;
  s->opt[OPT_CONTRAST].type = SANE_TYPE_INT;
  s->opt[OPT_CONTRAST].unit = SANE_UNIT_NONE;
  s->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CONTRAST].constraint.range = &(enhance_range);
  s->opt[OPT_CONTRAST].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  s->contrast = 0;  // disable by default

  /* "Extras" group: */
  s->opt[OPT_EXTRAS_GROUP].title = "extras-group";
  s->opt[OPT_EXTRAS_GROUP].title = SANE_I18N ("Extras");
  s->opt[OPT_EXTRAS_GROUP].desc = "";
  s->opt[OPT_EXTRAS_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_EXTRAS_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_EXTRAS_GROUP].size = 0;
  s->opt[OPT_EXTRAS_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* BW threshold */
  s->opt[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
  s->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
  s->opt[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
  s->opt[OPT_THRESHOLD].type = SANE_TYPE_FIXED;
  s->opt[OPT_THRESHOLD].unit = SANE_UNIT_PERCENT;
  s->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_THRESHOLD].constraint.range = &percentage_range;
  s->threshold = SANE_FIX(50);

  /* BW threshold curve */
  s->opt[OPT_THRESHOLD_CURVE].name = "threshold-curve";
  s->opt[OPT_THRESHOLD_CURVE].title = SANE_I18N ("Threshold curve");
  s->opt[OPT_THRESHOLD_CURVE].desc = SANE_I18N ("Dynamic threshold curve, from light to dark, normally 50-65");
  s->opt[OPT_THRESHOLD_CURVE].type = SANE_TYPE_INT;
  s->opt[OPT_THRESHOLD_CURVE].unit = SANE_UNIT_NONE;
  s->opt[OPT_THRESHOLD_CURVE].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_THRESHOLD_CURVE].constraint.range = &threshold_curve_range;
  s->threshold_curve = 50;

  /* disable_interpolation */
  s->opt[OPT_DISABLE_INTERPOLATION].name = "disable-interpolation";
  s->opt[OPT_DISABLE_INTERPOLATION].title =
    SANE_I18N ("Disable interpolation");
  s->opt[OPT_DISABLE_INTERPOLATION].desc =
    SANE_I18N
    ("When using high resolutions where the horizontal resolution is smaller "
     "than the vertical resolution this disables horizontal interpolation.");
  s->opt[OPT_DISABLE_INTERPOLATION].type = SANE_TYPE_BOOL;
  s->opt[OPT_DISABLE_INTERPOLATION].unit = SANE_UNIT_NONE;
  s->opt[OPT_DISABLE_INTERPOLATION].constraint_type = SANE_CONSTRAINT_NONE;
  s->disable_interpolation = false;

  /* color filter */
  s->opt[OPT_COLOR_FILTER].name = "color-filter";
  s->opt[OPT_COLOR_FILTER].title = SANE_I18N ("Color filter");
  s->opt[OPT_COLOR_FILTER].desc =
    SANE_I18N
    ("When using gray or lineart this option selects the used color.");
  s->opt[OPT_COLOR_FILTER].type = SANE_TYPE_STRING;
  s->opt[OPT_COLOR_FILTER].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  /* true gray not yet supported for GL847 and GL124 scanners */
    if (!model->is_cis || model->asic_type==AsicType::GL847 || model->asic_type==AsicType::GL124) {
      s->opt[OPT_COLOR_FILTER].size = max_string_size (color_filter_list);
      s->opt[OPT_COLOR_FILTER].constraint.string_list = color_filter_list;
      s->color_filter = s->opt[OPT_COLOR_FILTER].constraint.string_list[1];
    }
  else
    {
      s->opt[OPT_COLOR_FILTER].size = max_string_size (cis_color_filter_list);
      s->opt[OPT_COLOR_FILTER].constraint.string_list = cis_color_filter_list;
      /* default to "None" ie true gray */
      s->color_filter = s->opt[OPT_COLOR_FILTER].constraint.string_list[3];
    }

    // no support for color filter for cis+gl646 scanners
    if (model->asic_type == AsicType::GL646 && model->is_cis) {
      DISABLE (OPT_COLOR_FILTER);
    }

  /* calibration store file name */
  s->opt[OPT_CALIBRATION_FILE].name = "calibration-file";
  s->opt[OPT_CALIBRATION_FILE].title = SANE_I18N ("Calibration file");
  s->opt[OPT_CALIBRATION_FILE].desc = SANE_I18N ("Specify the calibration file to use");
  s->opt[OPT_CALIBRATION_FILE].type = SANE_TYPE_STRING;
  s->opt[OPT_CALIBRATION_FILE].unit = SANE_UNIT_NONE;
  s->opt[OPT_CALIBRATION_FILE].size = PATH_MAX;
  s->opt[OPT_CALIBRATION_FILE].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT | SANE_CAP_ADVANCED;
  s->opt[OPT_CALIBRATION_FILE].constraint_type = SANE_CONSTRAINT_NONE;
  s->calibration_file.clear();
  /* disable option if ran as root */
#ifdef HAVE_GETUID
  if(geteuid()==0)
    {
      DISABLE (OPT_CALIBRATION_FILE);
    }
#endif

  /* expiration time for calibration cache entries */
  s->opt[OPT_EXPIRATION_TIME].name = "expiration-time";
  s->opt[OPT_EXPIRATION_TIME].title = SANE_I18N ("Calibration cache expiration time");
  s->opt[OPT_EXPIRATION_TIME].desc = SANE_I18N ("Time (in minutes) before a cached calibration expires. "
     "A value of 0 means cache is not used. A negative value means cache never expires.");
  s->opt[OPT_EXPIRATION_TIME].type = SANE_TYPE_INT;
  s->opt[OPT_EXPIRATION_TIME].unit = SANE_UNIT_NONE;
  s->opt[OPT_EXPIRATION_TIME].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_EXPIRATION_TIME].constraint.range = &expiration_range;
  s->expiration_time = 60;  // 60 minutes by default

  /* Powersave time (turn lamp off) */
  s->opt[OPT_LAMP_OFF_TIME].name = "lamp-off-time";
  s->opt[OPT_LAMP_OFF_TIME].title = SANE_I18N ("Lamp off time");
  s->opt[OPT_LAMP_OFF_TIME].desc =
    SANE_I18N
    ("The lamp will be turned off after the given time (in minutes). "
     "A value of 0 means, that the lamp won't be turned off.");
  s->opt[OPT_LAMP_OFF_TIME].type = SANE_TYPE_INT;
  s->opt[OPT_LAMP_OFF_TIME].unit = SANE_UNIT_NONE;
  s->opt[OPT_LAMP_OFF_TIME].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_LAMP_OFF_TIME].constraint.range = &time_range;
  s->lamp_off_time = 15;    // 15 minutes

  /* turn lamp off during scan */
  s->opt[OPT_LAMP_OFF].name = "lamp-off-scan";
  s->opt[OPT_LAMP_OFF].title = SANE_I18N ("Lamp off during scan");
  s->opt[OPT_LAMP_OFF].desc = SANE_I18N ("The lamp will be turned off during scan. ");
  s->opt[OPT_LAMP_OFF].type = SANE_TYPE_BOOL;
  s->opt[OPT_LAMP_OFF].unit = SANE_UNIT_NONE;
  s->opt[OPT_LAMP_OFF].constraint_type = SANE_CONSTRAINT_NONE;
  s->lamp_off = false;

  s->opt[OPT_SENSOR_GROUP].name = SANE_NAME_SENSORS;
  s->opt[OPT_SENSOR_GROUP].title = SANE_TITLE_SENSORS;
  s->opt[OPT_SENSOR_GROUP].desc = SANE_DESC_SENSORS;
  s->opt[OPT_SENSOR_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_SENSOR_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_SENSOR_GROUP].size = 0;
  s->opt[OPT_SENSOR_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  s->opt[OPT_SCAN_SW].name = SANE_NAME_SCAN;
  s->opt[OPT_SCAN_SW].title = SANE_TITLE_SCAN;
  s->opt[OPT_SCAN_SW].desc = SANE_DESC_SCAN;
  s->opt[OPT_SCAN_SW].type = SANE_TYPE_BOOL;
  s->opt[OPT_SCAN_SW].unit = SANE_UNIT_NONE;
  if (model->buttons & GENESYS_HAS_SCAN_SW)
    s->opt[OPT_SCAN_SW].cap =
      SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
  else
    s->opt[OPT_SCAN_SW].cap = SANE_CAP_INACTIVE;

  /* SANE_NAME_FILE is not for buttons */
  s->opt[OPT_FILE_SW].name = "file";
  s->opt[OPT_FILE_SW].title = SANE_I18N ("File button");
  s->opt[OPT_FILE_SW].desc = SANE_I18N ("File button");
  s->opt[OPT_FILE_SW].type = SANE_TYPE_BOOL;
  s->opt[OPT_FILE_SW].unit = SANE_UNIT_NONE;
  if (model->buttons & GENESYS_HAS_FILE_SW)
    s->opt[OPT_FILE_SW].cap =
      SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
  else
    s->opt[OPT_FILE_SW].cap = SANE_CAP_INACTIVE;

  s->opt[OPT_EMAIL_SW].name = SANE_NAME_EMAIL;
  s->opt[OPT_EMAIL_SW].title = SANE_TITLE_EMAIL;
  s->opt[OPT_EMAIL_SW].desc = SANE_DESC_EMAIL;
  s->opt[OPT_EMAIL_SW].type = SANE_TYPE_BOOL;
  s->opt[OPT_EMAIL_SW].unit = SANE_UNIT_NONE;
  if (model->buttons & GENESYS_HAS_EMAIL_SW)
    s->opt[OPT_EMAIL_SW].cap =
      SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
  else
    s->opt[OPT_EMAIL_SW].cap = SANE_CAP_INACTIVE;

  s->opt[OPT_COPY_SW].name = SANE_NAME_COPY;
  s->opt[OPT_COPY_SW].title = SANE_TITLE_COPY;
  s->opt[OPT_COPY_SW].desc = SANE_DESC_COPY;
  s->opt[OPT_COPY_SW].type = SANE_TYPE_BOOL;
  s->opt[OPT_COPY_SW].unit = SANE_UNIT_NONE;
  if (model->buttons & GENESYS_HAS_COPY_SW)
    s->opt[OPT_COPY_SW].cap =
      SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
  else
    s->opt[OPT_COPY_SW].cap = SANE_CAP_INACTIVE;

  s->opt[OPT_PAGE_LOADED_SW].name = SANE_NAME_PAGE_LOADED;
  s->opt[OPT_PAGE_LOADED_SW].title = SANE_TITLE_PAGE_LOADED;
  s->opt[OPT_PAGE_LOADED_SW].desc = SANE_DESC_PAGE_LOADED;
  s->opt[OPT_PAGE_LOADED_SW].type = SANE_TYPE_BOOL;
  s->opt[OPT_PAGE_LOADED_SW].unit = SANE_UNIT_NONE;
  if (model->buttons & GENESYS_HAS_PAGE_LOADED_SW)
    s->opt[OPT_PAGE_LOADED_SW].cap =
      SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
  else
    s->opt[OPT_PAGE_LOADED_SW].cap = SANE_CAP_INACTIVE;

  /* OCR button */
  s->opt[OPT_OCR_SW].name = "ocr";
  s->opt[OPT_OCR_SW].title = SANE_I18N ("OCR button");
  s->opt[OPT_OCR_SW].desc = SANE_I18N ("OCR button");
  s->opt[OPT_OCR_SW].type = SANE_TYPE_BOOL;
  s->opt[OPT_OCR_SW].unit = SANE_UNIT_NONE;
  if (model->buttons & GENESYS_HAS_OCR_SW)
    s->opt[OPT_OCR_SW].cap =
      SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
  else
    s->opt[OPT_OCR_SW].cap = SANE_CAP_INACTIVE;

  /* power button */
  s->opt[OPT_POWER_SW].name = "power";
  s->opt[OPT_POWER_SW].title = SANE_I18N ("Power button");
  s->opt[OPT_POWER_SW].desc = SANE_I18N ("Power button");
  s->opt[OPT_POWER_SW].type = SANE_TYPE_BOOL;
  s->opt[OPT_POWER_SW].unit = SANE_UNIT_NONE;
  if (model->buttons & GENESYS_HAS_POWER_SW)
    s->opt[OPT_POWER_SW].cap =
      SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
  else
    s->opt[OPT_POWER_SW].cap = SANE_CAP_INACTIVE;

  /* extra button */
  s->opt[OPT_EXTRA_SW].name = "extra";
  s->opt[OPT_EXTRA_SW].title = SANE_I18N ("Extra button");
  s->opt[OPT_EXTRA_SW].desc = SANE_I18N ("Extra button");
  s->opt[OPT_EXTRA_SW].type = SANE_TYPE_BOOL;
  s->opt[OPT_EXTRA_SW].unit = SANE_UNIT_NONE;
  if (model->buttons & GENESYS_HAS_EXTRA_SW)
    s->opt[OPT_EXTRA_SW].cap =
      SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
  else
    s->opt[OPT_EXTRA_SW].cap = SANE_CAP_INACTIVE;

  /* calibration needed */
  s->opt[OPT_NEED_CALIBRATION_SW].name = "need-calibration";
  s->opt[OPT_NEED_CALIBRATION_SW].title = SANE_I18N ("Needs calibration");
  s->opt[OPT_NEED_CALIBRATION_SW].desc = SANE_I18N ("The scanner needs calibration for the current settings");
  s->opt[OPT_NEED_CALIBRATION_SW].type = SANE_TYPE_BOOL;
  s->opt[OPT_NEED_CALIBRATION_SW].unit = SANE_UNIT_NONE;
  if (model->buttons & GENESYS_HAS_CALIBRATE)
    s->opt[OPT_NEED_CALIBRATION_SW].cap =
      SANE_CAP_SOFT_DETECT | SANE_CAP_HARD_SELECT | SANE_CAP_ADVANCED;
  else
    s->opt[OPT_NEED_CALIBRATION_SW].cap = SANE_CAP_INACTIVE;

  /* button group */
  s->opt[OPT_BUTTON_GROUP].name = "buttons";
  s->opt[OPT_BUTTON_GROUP].title = SANE_I18N ("Buttons");
  s->opt[OPT_BUTTON_GROUP].desc = "";
  s->opt[OPT_BUTTON_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_BUTTON_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_BUTTON_GROUP].size = 0;
  s->opt[OPT_BUTTON_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* calibrate button */
  s->opt[OPT_CALIBRATE].name = "calibrate";
  s->opt[OPT_CALIBRATE].title = SANE_I18N ("Calibrate");
  s->opt[OPT_CALIBRATE].desc =
    SANE_I18N ("Start calibration using special sheet");
  s->opt[OPT_CALIBRATE].type = SANE_TYPE_BUTTON;
  s->opt[OPT_CALIBRATE].unit = SANE_UNIT_NONE;
  if (model->buttons & GENESYS_HAS_CALIBRATE)
    s->opt[OPT_CALIBRATE].cap =
      SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT | SANE_CAP_ADVANCED |
      SANE_CAP_AUTOMATIC;
  else
    s->opt[OPT_CALIBRATE].cap = SANE_CAP_INACTIVE;

  /* clear calibration cache button */
  s->opt[OPT_CLEAR_CALIBRATION].name = "clear-calibration";
  s->opt[OPT_CLEAR_CALIBRATION].title = SANE_I18N ("Clear calibration");
  s->opt[OPT_CLEAR_CALIBRATION].desc = SANE_I18N ("Clear calibration cache");
  s->opt[OPT_CLEAR_CALIBRATION].type = SANE_TYPE_BUTTON;
  s->opt[OPT_CLEAR_CALIBRATION].unit = SANE_UNIT_NONE;
  s->opt[OPT_CLEAR_CALIBRATION].size = 0;
  s->opt[OPT_CLEAR_CALIBRATION].constraint_type = SANE_CONSTRAINT_NONE;
  s->opt[OPT_CLEAR_CALIBRATION].cap =
    SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT | SANE_CAP_ADVANCED;

  /* force calibration cache button */
  s->opt[OPT_FORCE_CALIBRATION].name = "force-calibration";
  s->opt[OPT_FORCE_CALIBRATION].title = SANE_I18N("Force calibration");
  s->opt[OPT_FORCE_CALIBRATION].desc = SANE_I18N("Force calibration ignoring all and any calibration caches");
  s->opt[OPT_FORCE_CALIBRATION].type = SANE_TYPE_BUTTON;
  s->opt[OPT_FORCE_CALIBRATION].unit = SANE_UNIT_NONE;
  s->opt[OPT_FORCE_CALIBRATION].size = 0;
  s->opt[OPT_FORCE_CALIBRATION].constraint_type = SANE_CONSTRAINT_NONE;
  s->opt[OPT_FORCE_CALIBRATION].cap =
    SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT | SANE_CAP_ADVANCED;

    // ignore offsets option
    s->opt[OPT_IGNORE_OFFSETS].name = "ignore-internal-offsets";
    s->opt[OPT_IGNORE_OFFSETS].title = SANE_I18N("Ignore internal offsets");
    s->opt[OPT_IGNORE_OFFSETS].desc =
        SANE_I18N("Acquires the image including the internal calibration areas of the scanner");
    s->opt[OPT_IGNORE_OFFSETS].type = SANE_TYPE_BUTTON;
    s->opt[OPT_IGNORE_OFFSETS].unit = SANE_UNIT_NONE;
    s->opt[OPT_IGNORE_OFFSETS].size = 0;
    s->opt[OPT_IGNORE_OFFSETS].constraint_type = SANE_CONSTRAINT_NONE;
    s->opt[OPT_IGNORE_OFFSETS].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT |
                                     SANE_CAP_ADVANCED;

    calc_parameters(s);
}

static bool present;

// this function is passed to C API, it must not throw
static SANE_Status
check_present (SANE_String_Const devname) noexcept
{
    DBG_HELPER_ARGS(dbg, "%s detected.", devname);
    present = true;
  return SANE_STATUS_GOOD;
}

static SANE_Status
attach (SANE_String_Const devname, Genesys_Device ** devp, bool may_wait)
{
    DBG_HELPER_ARGS(dbg, "devp %s nullptr, may_wait = %d", devp ? "!=" : "==", may_wait);

    Genesys_Device *dev = nullptr;

    if (devp) {
        *devp = nullptr;
    }

  if (!devname)
    {
        DBG(DBG_error, "%s: devname == nullptr\n", __func__);
      return SANE_STATUS_INVAL;
    }

    for (auto& dev : *s_devices) {
        if (dev.file_name == devname) {
            if (devp)
                *devp = &dev;
            DBG(DBG_info, "%s: device `%s' was already in device list\n", __func__, devname);
            return SANE_STATUS_GOOD;
        }
    }

  DBG(DBG_info, "%s: trying to open device `%s'\n", __func__, devname);

    UsbDevice usb_dev;

    usb_dev.open(devname);
    DBG(DBG_info, "%s: device `%s' successfully opened\n", __func__, devname);

    int vendor, product;
    usb_dev.get_vendor_product(vendor, product);

  /* KV-SS080 is an auxiliary device which requires a master device to be here */
  if(vendor == 0x04da && product == 0x100f)
    {
        present = false;
      sanei_usb_find_devices (vendor, 0x1006, check_present);
      sanei_usb_find_devices (vendor, 0x1007, check_present);
      sanei_usb_find_devices (vendor, 0x1010, check_present);
        if (present == false) {
            throw SaneException("master device not present");
        }
    }

    Genesys_USB_Device_Entry* found_usb_dev = nullptr;
    for (auto& usb_dev : *s_usb_devices) {
        if (usb_dev.vendor == static_cast<unsigned>(vendor) &&
            usb_dev.product == static_cast<unsigned>(product))
        {
            found_usb_dev = &usb_dev;
            break;
        }
    }

    if (found_usb_dev == nullptr) {
      DBG(DBG_error, "%s: vendor 0x%xd product 0x%xd is not supported by this backend\n", __func__,
	   vendor, product);
      return SANE_STATUS_INVAL;
    }

    s_devices->emplace_back();
    dev = &s_devices->back();
    dev->file_name = devname;

    dev->model = &found_usb_dev->model;
    dev->vendorId = found_usb_dev->vendor;
    dev->productId = found_usb_dev->product;
  dev->usb_mode = 0;            /* i.e. unset */
    dev->already_initialized = false;

    DBG(DBG_info, "%s: found %s flatbed scanner %s at %s\n", __func__, dev->model->vendor,
        dev->model->model, dev->file_name.c_str());

    if (devp) {
        *devp = dev;
    }

    usb_dev.close();
    return SANE_STATUS_GOOD;
}

static SANE_Status
attach_one_device_impl(SANE_String_Const devname)
{
  Genesys_Device *dev;
    SANE_Status status = attach(devname, &dev, false);
    if (status != SANE_STATUS_GOOD) {
        DBG(DBG_info, "%s: failed to attach: %s\n", __func__, sane_strstatus(status));
        return status;
    }
    return status;
}

static SANE_Status attach_one_device(SANE_String_Const devname)
{
    return wrap_exceptions_to_status_code(__func__, [=]()
    {
        return attach_one_device_impl(devname);
    });
}

/* configuration framework functions */

// this function is passed to C API, it must not throw
static SANE_Status
config_attach_genesys(SANEI_Config __sane_unused__ *config, const char *devname) noexcept
{
  /* the devname has been processed and is ready to be used
   * directly. Since the backend is an USB only one, we can
   * call sanei_usb_attach_matching_devices straight */
  sanei_usb_attach_matching_devices (devname, attach_one_device);

  return SANE_STATUS_GOOD;
}

/* probes for scanner to attach to the backend */
static SANE_Status
probe_genesys_devices (void)
{
    DBG_HELPER(dbg);
  SANEI_Config config;
  SANE_Status status = SANE_STATUS_GOOD;

    // set configuration options structure : no option for this backend
    config.descriptors = nullptr;
    config.values = nullptr;
  config.count = 0;

  /* generic configure and attach function */
  status = sanei_configure_attach (GENESYS_CONFIG_FILE, &config,
				   config_attach_genesys);

    DBG(DBG_info, "%s: %zu devices currently attached\n", __func__, s_devices->size());
  return status;
}

/**
 * This should be changed if one of the substructures of
   Genesys_Calibration_Cache change, but it must be changed if there are
   changes that don't change size -- at least for now, as we store most
   of Genesys_Calibration_Cache as is.
*/
static const char* CALIBRATION_IDENT = "sane_genesys";
static const int CALIBRATION_VERSION = 14;

bool read_calibration(std::istream& str, Genesys_Device::Calibration& calibration,
                      const std::string& path)
{
    DBG_HELPER(dbg);

    std::string ident;
    serialize(str, ident);

    if (ident != CALIBRATION_IDENT) {
        DBG(DBG_info, "%s: Incorrect calibration file '%s' header\n", __func__, path.c_str());
        return false;
    }

    size_t version;
    serialize(str, version);

    if (version != CALIBRATION_VERSION) {
        DBG(DBG_info, "%s: Incorrect calibration file '%s' version\n", __func__, path.c_str());
        return false;
    }

    calibration.clear();
    serialize(str, calibration);
    return true;
}

/**
 * reads previously cached calibration data
 * from file defined in dev->calib_file
 */
static bool sanei_genesys_read_calibration(Genesys_Device::Calibration& calibration,
                                           const std::string& path)
{
    DBG_HELPER(dbg);

    std::ifstream str;
    str.open(path);
    if (!str.is_open()) {
        DBG(DBG_info, "%s: Cannot open %s\n", __func__, path.c_str());
        return false;
    }

    return read_calibration(str, calibration, path);
}

void write_calibration(std::ostream& str, Genesys_Device::Calibration& calibration)
{
    std::string ident = CALIBRATION_IDENT;
    serialize(str, ident);
    size_t version = CALIBRATION_VERSION;
    serialize(str, version);
    serialize_newline(str);
    serialize(str, calibration);
}

static void write_calibration(Genesys_Device::Calibration& calibration, const std::string& path)
{
    DBG_HELPER(dbg);

    std::ofstream str;
    str.open(path);
    if (!str.is_open()) {
        throw SaneException("Cannot open calibration for writing");
    }
    write_calibration(str, calibration);
}

/** @brief buffer scanned picture
 * In order to allow digital processing, we must be able to put all the
 * scanned picture in a buffer.
 */
static void genesys_buffer_image(Genesys_Scanner *s)
{
    DBG_HELPER(dbg);
  size_t maximum;     /**> maximum bytes size of the scan */
  size_t len;	      /**> length of scanned data read */
  size_t total;	      /**> total of butes read */
  size_t size;	      /**> size of image buffer */
  size_t read_size;   /**> size of reads */
  int lines;	      /** number of lines of the scan */
  Genesys_Device *dev = s->dev;

  /* compute maximum number of lines for the scan */
  if (s->params.lines > 0)
    {
      lines = s->params.lines;
    }
  else
    {
        lines = (dev->model->y_size * dev->settings.yres) / MM_PER_INCH;
    }
  DBG(DBG_info, "%s: buffering %d lines of %d bytes\n", __func__, lines,
      s->params.bytes_per_line);

  /* maximum bytes to read */
  maximum = s->params.bytes_per_line * lines;
    if (s->dev->settings.dynamic_lineart) {
      maximum *= 8;
    }

  /* initial size of the read buffer */
  size =
    ((2048 * 2048) / s->params.bytes_per_line) * s->params.bytes_per_line;

  /* read size */
  read_size = size / 2;

  dev->img_buffer.resize(size);

  /* loop reading data until we reach maximum or EOF */
    total = 0;
    while (total < maximum) {
      len = size - maximum;
      if (len > read_size)
	{
	  len = read_size;
	}

        try {
            genesys_read_ordered_data(dev, dev->img_buffer.data() + total, &len);
        } catch (const SaneException& e) {
            if (e.status() == SANE_STATUS_EOF) {
                // ideally we shouldn't end up here, but because computations are duplicated and
                // slightly different everywhere in the genesys backend, we have no other choice
                break;
            }
            throw;
        }
      total += len;

        // do we need to enlarge read buffer ?
        if (total + read_size > size) {
            size += read_size;
            dev->img_buffer.resize(size);
        }
    }

  /* since digital processing is going to take place,
   * issue head parking command so that the head move while
   * computing so we can save time
   */
    if (!dev->model->is_sheetfed && !dev->parking) {
        dev->cmd_set->slow_back_home(dev, dev->model->flags & GENESYS_FLAG_MUST_WAIT);
      dev->parking = !(s->dev->model->flags & GENESYS_FLAG_MUST_WAIT);
    }

  /* in case of dynamic lineart, we have buffered gray data which
   * must be converted to lineart first */
    if (s->dev->settings.dynamic_lineart) {
      total/=8;
      std::vector<uint8_t> lineart(total);

      genesys_gray_lineart (dev,
                            dev->img_buffer.data(),
                            lineart.data(),
                            dev->settings.pixels,
                            (total*8)/dev->settings.pixels,
                            dev->settings.threshold);
      dev->img_buffer = lineart;
    }

  /* update counters */
  dev->total_bytes_to_read = total;
  dev->total_bytes_read = 0;

  /* update params */
  s->params.lines = total / s->params.bytes_per_line;
  if (DBG_LEVEL >= DBG_io2)
    {
      sanei_genesys_write_pnm_file("gl_unprocessed.pnm", dev->img_buffer.data(), s->params.depth,
                                   s->params.format==SANE_FRAME_RGB ? 3 : 1,
                                   s->params.pixels_per_line, s->params.lines);
    }
}

/* -------------------------- SANE API functions ------------------------- */

SANE_Status
sane_init_impl(SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  DBG_INIT ();
    DBG_HELPER_ARGS(dbg, "authorize %s null", authorize ? "!=" : "==");
    DBG(DBG_init, "SANE Genesys backend from %s\n", PACKAGE_STRING);
#ifdef HAVE_LIBUSB
  DBG(DBG_init, "SANE Genesys backend built with libusb-1.0\n");
#endif
#ifdef HAVE_LIBUSB_LEGACY
  DBG(DBG_init, "SANE Genesys backend built with libusb\n");
#endif

    if (version_code) {
        *version_code = SANE_VERSION_CODE(SANE_CURRENT_MAJOR, SANE_CURRENT_MINOR, 0);
    }

  /* init usb use */
  sanei_usb_init ();

  /* init sanei_magic */
  sanei_magic_init();

  s_scanners.init();
  s_devices.init();
  s_sane_devices.init();
    s_sane_devices_data.init();
  s_sane_devices_ptrs.init();
  genesys_init_sensor_tables();
  genesys_init_frontend_tables();
    genesys_init_gpo_tables();
    genesys_init_motor_tables();
    genesys_init_usb_device_tables();


  DBG(DBG_info, "%s: %s endian machine\n", __func__,
#ifdef WORDS_BIGENDIAN
       "big"
#else
       "little"
#endif
    );

    // cold-plug case :detection of allready connected scanners
    return probe_genesys_devices ();
}


extern "C" SANE_Status sane_init(SANE_Int * version_code, SANE_Auth_Callback authorize)
{
    return wrap_exceptions_to_status_code(__func__, [=]()
    {
        return sane_init_impl(version_code, authorize);
    });
}

void
sane_exit_impl(void)
{
    DBG_HELPER(dbg);

  sanei_usb_exit();

  run_functions_at_backend_exit();
}

void sane_exit()
{
    catch_all_exceptions(__func__, [](){ sane_exit_impl(); });
}

SANE_Status
sane_get_devices_impl(const SANE_Device *** device_list, SANE_Bool local_only)
{
    DBG_HELPER_ARGS(dbg, "local_only = %s", local_only ? "true" : "false");

  /* hot-plug case : detection of newly connected scanners */
  sanei_usb_scan_devices ();
  probe_genesys_devices ();

    s_sane_devices->clear();
    s_sane_devices_data->clear();
    s_sane_devices_ptrs->clear();
    s_sane_devices->reserve(s_devices->size());
    s_sane_devices_data->reserve(s_devices->size());
    s_sane_devices_ptrs->reserve(s_devices->size() + 1);

    for (auto dev_it = s_devices->begin(); dev_it != s_devices->end();) {
        present = false;
        sanei_usb_find_devices(dev_it->vendorId, dev_it->productId, check_present);
        if (present) {
            s_sane_devices->emplace_back();
            s_sane_devices_data->emplace_back();
            auto& sane_device = s_sane_devices->back();
            auto& sane_device_data = s_sane_devices_data->back();
            sane_device_data.name = dev_it->file_name;
            sane_device.name = sane_device_data.name.c_str();
            sane_device.vendor = dev_it->model->vendor;
            sane_device.model = dev_it->model->model;
            sane_device.type = "flatbed scanner";
            s_sane_devices_ptrs->push_back(&sane_device);
            dev_it++;
        } else {
            dev_it = s_devices->erase(dev_it);
        }
    }
    s_sane_devices_ptrs->push_back(nullptr);

    *const_cast<SANE_Device***>(device_list) = s_sane_devices_ptrs->data();

  return SANE_STATUS_GOOD;
}

SANE_Status sane_get_devices(const SANE_Device *** device_list, SANE_Bool local_only)
{
    return wrap_exceptions_to_status_code(__func__, [=]()
    {
        return sane_get_devices_impl(device_list, local_only);
    });
}

SANE_Status
sane_open_impl(SANE_String_Const devicename, SANE_Handle * handle)
{
    DBG_HELPER_ARGS(dbg, "devicename = %s", devicename);
    Genesys_Device* dev = nullptr;

  /* devicename="" or devicename="genesys" are default values that use
   * first available device
   */
  if (devicename[0] && strcmp ("genesys", devicename) != 0)
    {
      /* search for the given devicename in the device list */
        for (auto& d : *s_devices) {
            if (d.file_name == devicename) {
                dev = &d;
                break;
            }
        }

      if (!dev)
	{
	  DBG(DBG_info, "%s: couldn't find `%s' in devlist, trying attach\n", __func__, devicename);
            SANE_Status status = attach(devicename, &dev, true);
            if (status != SANE_STATUS_GOOD) {
                DBG(DBG_info, "%s: failed to attach: %s\n", __func__, sane_strstatus(status));
                return status;
            }
	}
      else
        DBG(DBG_info, "%s: found `%s' in devlist\n", __func__, dev->model->name);
    }
  else
    {
        // empty devicename or "genesys" -> use first device
        if (!s_devices->empty()) {
            dev = &s_devices->front();
            DBG(DBG_info, "%s: empty devicename, trying `%s'\n", __func__, dev->file_name.c_str());
        }
    }

  if (!dev)
    return SANE_STATUS_INVAL;

  if (dev->model->flags & GENESYS_FLAG_UNTESTED)
    {
      DBG(DBG_error0, "WARNING: Your scanner is not fully supported or at least \n");
      DBG(DBG_error0, "         had only limited testing. Please be careful and \n");
      DBG(DBG_error0, "         report any failure/success to \n");
      DBG(DBG_error0, "         sane-devel@alioth-lists.debian.net. Please provide as many\n");
      DBG(DBG_error0, "         details as possible, e.g. the exact name of your\n");
      DBG(DBG_error0, "         scanner and what does (not) work.\n");
    }

    dbg.vstatus("open device '%s'", dev->file_name.c_str());
    dev->usb_dev.open(dev->file_name.c_str());
    dbg.clear();


  s_scanners->push_back(Genesys_Scanner());
  auto* s = &s_scanners->back();

  s->dev = dev;
    s->scanning = false;
    s->dev->parking = false;
    s->dev->read_active = false;
  s->dev->force_calibration = 0;
  s->dev->line_count = 0;
    s->dev->binary = nullptr;

  *handle = s;

    if (!dev->already_initialized) {
        sanei_genesys_init_structs (dev);
    }

    init_options(s);

    sanei_genesys_init_cmd_set(s->dev);

    // FIXME: we create sensor tables for the sensor, this should happen when we know which sensor
    // we will select
    dev->cmd_set->init(dev);

    // some hardware capabilities are detected through sensors
    s->dev->cmd_set->update_hardware_sensors (s);

  /* here is the place to fetch a stored calibration cache */
  if (s->dev->force_calibration == 0)
    {
        auto path = calibration_filename(s->dev);
        s->calibration_file = path;
        s->dev->calib_file = path;
      DBG(DBG_info, "%s: Calibration filename set to:\n", __func__);
      DBG(DBG_info, "%s: >%s<\n", __func__, s->dev->calib_file.c_str());

        catch_all_exceptions(__func__, [&]()
        {
            sanei_genesys_read_calibration(s->dev->calibration_cache, s->dev->calib_file);
        });
    }

    return SANE_STATUS_GOOD;
}

SANE_Status sane_open(SANE_String_Const devicename, SANE_Handle* handle)
{
    return wrap_exceptions_to_status_code(__func__, [=]()
    {
        return sane_open_impl(devicename, handle);
    });
}

void
sane_close_impl(SANE_Handle handle)
{
    DBG_HELPER(dbg);

  /* remove handle from list of open handles: */
  auto it = s_scanners->end();
  for (auto it2 = s_scanners->begin(); it2 != s_scanners->end(); it2++)
    {
      if (&*it2 == handle) {
          it = it2;
          break;
        }
    }
  if (it == s_scanners->end())
    {
      DBG(DBG_error, "%s: invalid handle %p\n", __func__, handle);
      return;			/* oops, not a handle we know about */
    }

  Genesys_Scanner* s = &*it;

  /* eject document for sheetfed scanners */
    if (s->dev->model->is_sheetfed) {
        catch_all_exceptions(__func__, [&](){ s->dev->cmd_set->eject_document(s->dev); });
    }
  else
    {
      /* in case scanner is parking, wait for the head
       * to reach home position */
        if (s->dev->parking) {
            sanei_genesys_wait_for_home(s->dev);
        }
    }

    // enable power saving before leaving
    s->dev->cmd_set->save_power(s->dev, true);

    // here is the place to store calibration cache
    if (s->dev->force_calibration == 0) {
        catch_all_exceptions(__func__, [&](){ write_calibration(s->dev->calibration_cache,
                                                                s->dev->calib_file); });
    }

    s->dev->already_initialized = false;

   /* for an handful of bytes .. */
    std::free(reinterpret_cast<void*>(const_cast<SANE_Word*>(s->opt[OPT_RESOLUTION].constraint.word_list)));
    std::free(reinterpret_cast<void*>(const_cast<SANE_Range*>(s->opt[OPT_TL_X].constraint.range)));
    std::free(reinterpret_cast<void*>(const_cast<SANE_Range*>(s->opt[OPT_TL_Y].constraint.range)));

  s->dev->clear();

    // LAMP OFF : same register across all the ASICs */
    s->dev->write_register(0x03, 0x00);

    catch_all_exceptions(__func__, [&](){ s->dev->usb_dev.clear_halt(); });

    // we need this to avoid these ASIC getting stuck in bulk writes
    catch_all_exceptions(__func__, [&](){ s->dev->usb_dev.reset(); });

    // not freeing s->dev because it's in the dev list
    catch_all_exceptions(__func__, [&](){ s->dev->usb_dev.close(); });

  s_scanners->erase(it);
}

void sane_close(SANE_Handle handle)
{
    catch_all_exceptions(__func__, [=]()
    {
        sane_close_impl(handle);
    });
}

const SANE_Option_Descriptor *
sane_get_option_descriptor_impl(SANE_Handle handle, SANE_Int option)
{
    Genesys_Scanner* s = reinterpret_cast<Genesys_Scanner*>(handle);

    if (static_cast<unsigned>(option) >= NUM_OPTIONS) {
        return nullptr;
    }

  DBG(DBG_io2, "%s: option = %s (%d)\n", __func__, s->opt[option].name, option);
  return s->opt + option;
}


const SANE_Option_Descriptor *
sane_get_option_descriptor(SANE_Handle handle, SANE_Int option)
{
    const SANE_Option_Descriptor* ret = nullptr;
    catch_all_exceptions(__func__, [&]()
    {
        ret = sane_get_option_descriptor_impl(handle, option);
    });
    return ret;
}

/* gets an option , called by sane_control_option */
static SANE_Status
get_option_value (Genesys_Scanner * s, int option, void *val)
{
    DBG_HELPER(dbg);
  unsigned int i;
    SANE_Word* table = nullptr;
  std::vector<uint16_t> gamma_table;
  unsigned option_size = 0;
  SANE_Status status = SANE_STATUS_GOOD;

    const Genesys_Sensor* sensor = nullptr;
    if (sanei_genesys_has_sensor(s->dev, s->dev->settings.xres, s->dev->settings.get_channels(),
                                 s->dev->settings.scan_method))
    {
        sensor = &sanei_genesys_find_sensor(s->dev, s->dev->settings.xres,
                                            s->dev->settings.get_channels(),
                                            s->dev->settings.scan_method);
    }

  switch (option)
    {
      /* geometry */
    case OPT_TL_X:
        *reinterpret_cast<SANE_Word*>(val) = s->pos_top_left_x;
        break;
    case OPT_TL_Y:
        *reinterpret_cast<SANE_Word*>(val) = s->pos_top_left_y;
        break;
    case OPT_BR_X:
        *reinterpret_cast<SANE_Word*>(val) = s->pos_bottom_right_x;
        break;
    case OPT_BR_Y:
        *reinterpret_cast<SANE_Word*>(val) = s->pos_bottom_right_y;
        break;
      /* word options: */
    case OPT_NUM_OPTS:
        *reinterpret_cast<SANE_Word*>(val) = NUM_OPTIONS;
        break;
    case OPT_RESOLUTION:
        *reinterpret_cast<SANE_Word*>(val) = s->resolution;
        break;
    case OPT_BIT_DEPTH:
        *reinterpret_cast<SANE_Word*>(val) = s->bit_depth;
        break;
    case OPT_PREVIEW:
        *reinterpret_cast<SANE_Word*>(val) = s->preview;
        break;
    case OPT_THRESHOLD:
        *reinterpret_cast<SANE_Word*>(val) = s->threshold;
        break;
    case OPT_THRESHOLD_CURVE:
        *reinterpret_cast<SANE_Word*>(val) = s->threshold_curve;
        break;
    case OPT_DISABLE_INTERPOLATION:
        *reinterpret_cast<SANE_Word*>(val) = s->disable_interpolation;
        break;
    case OPT_LAMP_OFF:
        *reinterpret_cast<SANE_Word*>(val) = s->lamp_off;
        break;
    case OPT_LAMP_OFF_TIME:
        *reinterpret_cast<SANE_Word*>(val) = s->lamp_off_time;
        break;
    case OPT_SWDESKEW:
        *reinterpret_cast<SANE_Word*>(val) = s->swdeskew;
        break;
    case OPT_SWCROP:
        *reinterpret_cast<SANE_Word*>(val) = s->swcrop;
        break;
    case OPT_SWDESPECK:
        *reinterpret_cast<SANE_Word*>(val) = s->swdespeck;
        break;
    case OPT_SWDEROTATE:
        *reinterpret_cast<SANE_Word*>(val) = s->swderotate;
        break;
    case OPT_SWSKIP:
        *reinterpret_cast<SANE_Word*>(val) = s->swskip;
        break;
    case OPT_DESPECK:
        *reinterpret_cast<SANE_Word*>(val) = s->despeck;
        break;
    case OPT_CONTRAST:
        *reinterpret_cast<SANE_Word*>(val) = s->contrast;
        break;
    case OPT_BRIGHTNESS:
        *reinterpret_cast<SANE_Word*>(val) = s->brightness;
        break;
    case OPT_EXPIRATION_TIME:
        *reinterpret_cast<SANE_Word*>(val) = s->expiration_time;
        break;
    case OPT_CUSTOM_GAMMA:
        *reinterpret_cast<SANE_Word*>(val) = s->custom_gamma;
        break;

      /* string options: */
    case OPT_MODE:
        std::strcpy(reinterpret_cast<char*>(val), s->mode.c_str());
        break;
    case OPT_COLOR_FILTER:
        std::strcpy(reinterpret_cast<char*>(val), s->color_filter.c_str());
        break;
    case OPT_CALIBRATION_FILE:
        std::strcpy(reinterpret_cast<char*>(val), s->calibration_file.c_str());
        break;
    case OPT_SOURCE:
        std::strcpy(reinterpret_cast<char*>(val), s->source.c_str());
        break;

      /* word array options */
    case OPT_GAMMA_VECTOR:
        if (!sensor)
            throw SaneException("Unsupported scanner mode selected");

        table = reinterpret_cast<SANE_Word*>(val);
        if (s->color_filter == "Red") {
            gamma_table = get_gamma_table(s->dev, *sensor, GENESYS_RED);
        } else if (s->color_filter == "Blue") {
            gamma_table = get_gamma_table(s->dev, *sensor, GENESYS_BLUE);
        } else {
            gamma_table = get_gamma_table(s->dev, *sensor, GENESYS_GREEN);
        }
        option_size = s->opt[option].size / sizeof (SANE_Word);
        if (gamma_table.size() != option_size) {
            throw std::runtime_error("The size of the gamma tables does not match");
        }
        for (i = 0; i < option_size; i++) {
            table[i] = gamma_table[i];
        }
        break;
    case OPT_GAMMA_VECTOR_R:
        if (!sensor)
            throw SaneException("Unsupported scanner mode selected");

        table = reinterpret_cast<SANE_Word*>(val);
        gamma_table = get_gamma_table(s->dev, *sensor, GENESYS_RED);
        option_size = s->opt[option].size / sizeof (SANE_Word);
        if (gamma_table.size() != option_size) {
            throw std::runtime_error("The size of the gamma tables does not match");
        }
        for (i = 0; i < option_size; i++) {
            table[i] = gamma_table[i];
	}
      break;
    case OPT_GAMMA_VECTOR_G:
        if (!sensor)
            throw SaneException("Unsupported scanner mode selected");

        table = reinterpret_cast<SANE_Word*>(val);
        gamma_table = get_gamma_table(s->dev, *sensor, GENESYS_GREEN);
        option_size = s->opt[option].size / sizeof (SANE_Word);
        if (gamma_table.size() != option_size) {
            throw std::runtime_error("The size of the gamma tables does not match");
        }
        for (i = 0; i < option_size; i++) {
            table[i] = gamma_table[i];
        }
      break;
    case OPT_GAMMA_VECTOR_B:
        if (!sensor)
            throw SaneException("Unsupported scanner mode selected");

        table = reinterpret_cast<SANE_Word*>(val);
        gamma_table = get_gamma_table(s->dev, *sensor, GENESYS_BLUE);
        option_size = s->opt[option].size / sizeof (SANE_Word);
        if (gamma_table.size() != option_size) {
            throw std::runtime_error("The size of the gamma tables does not match");
        }
        for (i = 0; i < option_size; i++) {
            table[i] = gamma_table[i];
        }
      break;
      /* sensors */
    case OPT_SCAN_SW:
    case OPT_FILE_SW:
    case OPT_EMAIL_SW:
    case OPT_COPY_SW:
    case OPT_PAGE_LOADED_SW:
    case OPT_OCR_SW:
    case OPT_POWER_SW:
    case OPT_EXTRA_SW:
        s->dev->cmd_set->update_hardware_sensors(s);
        *reinterpret_cast<SANE_Bool*>(val) = s->buttons[genesys_option_to_button(option)].read();
      break;
    case OPT_NEED_CALIBRATION_SW:
        if (!sensor)
            throw SaneException("Unsupported scanner mode selected");

      /* scanner needs calibration for current mode unless a matching
       * calibration cache is found */
        *reinterpret_cast<SANE_Bool*>(val) = SANE_TRUE;
      for (auto& cache : s->dev->calibration_cache)
	{
        if (s->dev->cmd_set->is_compatible_calibration(s->dev, *sensor, &cache, false)) {
                *reinterpret_cast<SANE_Bool*>(val) = SANE_FALSE;
	    }
	}
      break;
    default:
      DBG(DBG_warn, "%s: can't get unknown option %d\n", __func__, option);
    }
  return status;
}

/** @brief set calibration file value
 * Set calibration file value. Load new cache values from file if it exists,
 * else creates the file*/
static void set_calibration_value(Genesys_Scanner* s, const char* val)
{
    DBG_HELPER(dbg);

    std::string new_calib_path = val;
    Genesys_Device::Calibration new_calibration;

    bool is_calib_success = false;
    catch_all_exceptions(__func__, [&]()
    {
        is_calib_success = sanei_genesys_read_calibration(new_calibration, new_calib_path);
    });

    if (!is_calib_success) {
        return;
    }

    s->dev->calibration_cache = std::move(new_calibration);
    s->dev->calib_file = new_calib_path;
    s->calibration_file = new_calib_path;
    DBG(DBG_info, "%s: Calibration filename set to '%s':\n", __func__, new_calib_path.c_str());
}

/* sets an option , called by sane_control_option */
static SANE_Status
set_option_value (Genesys_Scanner * s, int option, void *val,
		  SANE_Int * myinfo)
{
    DBG_HELPER(dbg);
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Word *table;
  unsigned int i;
  SANE_Range *x_range, *y_range;
  unsigned option_size = 0;

  switch (option)
    {
    case OPT_TL_X:
        s->pos_top_left_x = *reinterpret_cast<SANE_Word*>(val);
        calc_parameters(s);
        *myinfo |= SANE_INFO_RELOAD_PARAMS;
        break;
    case OPT_TL_Y:
        s->pos_top_left_y = *reinterpret_cast<SANE_Word*>(val);
        calc_parameters(s);
        *myinfo |= SANE_INFO_RELOAD_PARAMS;
        break;
    case OPT_BR_X:
        s->pos_bottom_right_x = *reinterpret_cast<SANE_Word*>(val);
        calc_parameters(s);
        *myinfo |= SANE_INFO_RELOAD_PARAMS;
        break;
    case OPT_BR_Y:
        s->pos_bottom_right_y = *reinterpret_cast<SANE_Word*>(val);
        calc_parameters(s);
        *myinfo |= SANE_INFO_RELOAD_PARAMS;
        break;
    case OPT_RESOLUTION:
        s->resolution = *reinterpret_cast<SANE_Word*>(val);
        calc_parameters(s);
        *myinfo |= SANE_INFO_RELOAD_PARAMS;
        break;
    case OPT_THRESHOLD:
        s->threshold = *reinterpret_cast<SANE_Word*>(val);
        calc_parameters(s);
        *myinfo |= SANE_INFO_RELOAD_PARAMS;
        break;
    case OPT_THRESHOLD_CURVE:
        s->threshold_curve = *reinterpret_cast<SANE_Word*>(val);
        calc_parameters(s);
        *myinfo |= SANE_INFO_RELOAD_PARAMS;
        break;
    case OPT_SWCROP:
        s->swcrop = *reinterpret_cast<SANE_Word*>(val);
        calc_parameters(s);
        *myinfo |= SANE_INFO_RELOAD_PARAMS;
        break;
    case OPT_SWDESKEW:
        s->swdeskew = *reinterpret_cast<SANE_Word*>(val);
        calc_parameters(s);
        *myinfo |= SANE_INFO_RELOAD_PARAMS;
        break;
    case OPT_DESPECK:
        s->despeck = *reinterpret_cast<SANE_Word*>(val);
        calc_parameters(s);
        *myinfo |= SANE_INFO_RELOAD_PARAMS;
        break;
    case OPT_SWDEROTATE:
        s->swderotate = *reinterpret_cast<SANE_Word*>(val);
        calc_parameters(s);
        *myinfo |= SANE_INFO_RELOAD_PARAMS;
        break;
    case OPT_SWSKIP:
        s->swskip = *reinterpret_cast<SANE_Word*>(val);
        calc_parameters(s);
        *myinfo |= SANE_INFO_RELOAD_PARAMS;
        break;
    case OPT_DISABLE_INTERPOLATION:
        s->disable_interpolation = *reinterpret_cast<SANE_Word*>(val);
        calc_parameters(s);
        *myinfo |= SANE_INFO_RELOAD_PARAMS;
        break;
    case OPT_LAMP_OFF:
        s->lamp_off = *reinterpret_cast<SANE_Word*>(val);
        calc_parameters(s);
        *myinfo |= SANE_INFO_RELOAD_PARAMS;
        break;
    case OPT_PREVIEW:
        s->preview = *reinterpret_cast<SANE_Word*>(val);
        calc_parameters(s);
        *myinfo |= SANE_INFO_RELOAD_PARAMS;
        break;
    case OPT_BRIGHTNESS:
        s->brightness = *reinterpret_cast<SANE_Word*>(val);
        calc_parameters(s);
        *myinfo |= SANE_INFO_RELOAD_PARAMS;
        break;
    case OPT_CONTRAST:
        s->contrast = *reinterpret_cast<SANE_Word*>(val);
        calc_parameters(s);
        *myinfo |= SANE_INFO_RELOAD_PARAMS;
        break;
    case OPT_SWDESPECK:
        s->swdespeck = *reinterpret_cast<SANE_Word*>(val);
        if (s->swdespeck) {
            ENABLE(OPT_DESPECK);
        } else {
            DISABLE(OPT_DESPECK);
        }
        calc_parameters(s);
      *myinfo |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
      break;
    /* software enhancement functions only apply to 8 or 1 bits data */
    case OPT_BIT_DEPTH:
        s->bit_depth = *reinterpret_cast<SANE_Word*>(val);
        if(s->bit_depth>8)
        {
          DISABLE(OPT_SWDESKEW);
          DISABLE(OPT_SWDESPECK);
          DISABLE(OPT_SWCROP);
          DISABLE(OPT_DESPECK);
          DISABLE(OPT_SWDEROTATE);
          DISABLE(OPT_SWSKIP);
          DISABLE(OPT_CONTRAST);
          DISABLE(OPT_BRIGHTNESS);
        }
      else
        {
          ENABLE(OPT_SWDESKEW);
          ENABLE(OPT_SWDESPECK);
          ENABLE(OPT_SWCROP);
          ENABLE(OPT_DESPECK);
          ENABLE(OPT_SWDEROTATE);
          ENABLE(OPT_SWSKIP);
          ENABLE(OPT_CONTRAST);
          ENABLE(OPT_BRIGHTNESS);
        }
        calc_parameters(s);
      *myinfo |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
      break;
    case OPT_SOURCE:
        if (s->source != reinterpret_cast<const char*>(val)) {
            s->source = reinterpret_cast<const char*>(val);

            // change geometry constraint to the new source value
            if (s->source == STR_FLATBED)
            {
              x_range=create_range(s->dev->model->x_size);
              y_range=create_range(s->dev->model->y_size);
            }
          else
            {
              x_range=create_range(s->dev->model->x_size_ta);
              y_range=create_range(s->dev->model->y_size_ta);
            }
        if (x_range == nullptr || y_range == nullptr) {
              return SANE_STATUS_NO_MEM;
            }

          /* assign new values */
          std::free(reinterpret_cast<void*>(const_cast<SANE_Range*>(s->opt[OPT_TL_X].constraint.range)));
          std::free(reinterpret_cast<void*>(const_cast<SANE_Range*>(s->opt[OPT_TL_Y].constraint.range)));
          s->opt[OPT_TL_X].constraint.range = x_range;
          s->pos_top_left_x = 0;
          s->opt[OPT_TL_Y].constraint.range = y_range;
          s->pos_top_left_y = 0;
          s->opt[OPT_BR_X].constraint.range = x_range;
          s->pos_bottom_right_x = x_range->max;
          s->opt[OPT_BR_Y].constraint.range = y_range;
          s->pos_bottom_right_y = y_range->max;

          /* signals reload */
	  *myinfo |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
	}
      break;
    case OPT_MODE:
      s->mode = reinterpret_cast<const char*>(val);

      if (s->mode == SANE_VALUE_SCAN_MODE_LINEART)
	{
	  ENABLE (OPT_THRESHOLD);
	  ENABLE (OPT_THRESHOLD_CURVE);
	  DISABLE (OPT_BIT_DEPTH);
                if (s->dev->model->asic_type != AsicType::GL646 || !s->dev->model->is_cis) {
                    ENABLE(OPT_COLOR_FILTER);
                }
	}
      else
	{
	  DISABLE (OPT_THRESHOLD);
	  DISABLE (OPT_THRESHOLD_CURVE);
          if (s->mode == SANE_VALUE_SCAN_MODE_GRAY)
	    {
                    if (s->dev->model->asic_type != AsicType::GL646 || !s->dev->model->is_cis) {
                        ENABLE(OPT_COLOR_FILTER);
                    }
	      create_bpp_list (s, s->dev->model->bpp_gray_values);
                    s->bit_depth = s->dev->model->bpp_gray_values[0];
	    }
	  else
	    {
	      DISABLE (OPT_COLOR_FILTER);
	      create_bpp_list (s, s->dev->model->bpp_color_values);
                    s->bit_depth = s->dev->model->bpp_color_values[0];
	    }
	}
        calc_parameters(s);

      /* if custom gamma, toggle gamma table options according to the mode */
      if (s->custom_gamma)
	{
          if (s->mode == SANE_VALUE_SCAN_MODE_COLOR)
	    {
	      DISABLE (OPT_GAMMA_VECTOR);
	      ENABLE (OPT_GAMMA_VECTOR_R);
	      ENABLE (OPT_GAMMA_VECTOR_G);
	      ENABLE (OPT_GAMMA_VECTOR_B);
	    }
	  else
	    {
	      ENABLE (OPT_GAMMA_VECTOR);
	      DISABLE (OPT_GAMMA_VECTOR_R);
	      DISABLE (OPT_GAMMA_VECTOR_G);
	      DISABLE (OPT_GAMMA_VECTOR_B);
	    }
	}

      *myinfo |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
      break;
    case OPT_COLOR_FILTER:
      s->color_filter = reinterpret_cast<const char*>(val);
        calc_parameters(s);
      break;
    case OPT_CALIBRATION_FILE:
            if (s->dev->force_calibration == 0) {
                set_calibration_value(s, reinterpret_cast<const char*>(val));
            }
            break;
    case OPT_LAMP_OFF_TIME:
        if (*reinterpret_cast<SANE_Word*>(val) != s->lamp_off_time) {
            s->lamp_off_time = *reinterpret_cast<SANE_Word*>(val);
                s->dev->cmd_set->set_powersaving(s->dev, s->lamp_off_time);
        }
        break;
    case OPT_EXPIRATION_TIME:
        if (*reinterpret_cast<SANE_Word*>(val) != s->expiration_time) {
            s->expiration_time = *reinterpret_cast<SANE_Word*>(val);
            // BUG: this is most likely not intended behavior, found out during refactor
                s->dev->cmd_set->set_powersaving(s->dev, s->expiration_time);
	}
        break;

    case OPT_CUSTOM_GAMMA:
      *myinfo |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
        s->custom_gamma = *reinterpret_cast<SANE_Bool*>(val);

        if (s->custom_gamma) {
          if (s->mode == SANE_VALUE_SCAN_MODE_COLOR)
	    {
	      DISABLE (OPT_GAMMA_VECTOR);
	      ENABLE (OPT_GAMMA_VECTOR_R);
	      ENABLE (OPT_GAMMA_VECTOR_G);
	      ENABLE (OPT_GAMMA_VECTOR_B);
	    }
	  else
	    {
	      ENABLE (OPT_GAMMA_VECTOR);
	      DISABLE (OPT_GAMMA_VECTOR_R);
	      DISABLE (OPT_GAMMA_VECTOR_G);
	      DISABLE (OPT_GAMMA_VECTOR_B);
	    }
	}
      else
	{
	  DISABLE (OPT_GAMMA_VECTOR);
	  DISABLE (OPT_GAMMA_VECTOR_R);
	  DISABLE (OPT_GAMMA_VECTOR_G);
	  DISABLE (OPT_GAMMA_VECTOR_B);
            for (auto& table : s->dev->gamma_override_tables) {
                table.clear();
            }
	}
      break;

    case OPT_GAMMA_VECTOR:
        table = reinterpret_cast<SANE_Word*>(val);
        option_size = s->opt[option].size / sizeof (SANE_Word);

        s->dev->gamma_override_tables[GENESYS_RED].resize(option_size);
        s->dev->gamma_override_tables[GENESYS_GREEN].resize(option_size);
        s->dev->gamma_override_tables[GENESYS_BLUE].resize(option_size);
        for (i = 0; i < option_size; i++) {
            s->dev->gamma_override_tables[GENESYS_RED][i] = table[i];
            s->dev->gamma_override_tables[GENESYS_GREEN][i] = table[i];
            s->dev->gamma_override_tables[GENESYS_BLUE][i] = table[i];
        }
      break;
    case OPT_GAMMA_VECTOR_R:
        table = reinterpret_cast<SANE_Word*>(val);
        option_size = s->opt[option].size / sizeof (SANE_Word);
        s->dev->gamma_override_tables[GENESYS_RED].resize(option_size);
        for (i = 0; i < option_size; i++) {
            s->dev->gamma_override_tables[GENESYS_RED][i] = table[i];
        }
      break;
    case OPT_GAMMA_VECTOR_G:
        table = reinterpret_cast<SANE_Word*>(val);
        option_size = s->opt[option].size / sizeof (SANE_Word);
        s->dev->gamma_override_tables[GENESYS_GREEN].resize(option_size);
        for (i = 0; i < option_size; i++) {
            s->dev->gamma_override_tables[GENESYS_GREEN][i] = table[i];
        }
      break;
    case OPT_GAMMA_VECTOR_B:
        table = reinterpret_cast<SANE_Word*>(val);
        option_size = s->opt[option].size / sizeof (SANE_Word);
        s->dev->gamma_override_tables[GENESYS_BLUE].resize(option_size);
        for (i = 0; i < option_size; i++) {
            s->dev->gamma_override_tables[GENESYS_BLUE][i] = table[i];
        }
      break;
        case OPT_CALIBRATE: {
            auto& sensor = sanei_genesys_find_sensor_for_write(s->dev, s->dev->settings.xres,
                                                               s->dev->settings.get_channels(),
                                                               s->dev->settings.scan_method);
            catch_all_exceptions(__func__, [&]()
            {
                s->dev->cmd_set->save_power(s->dev, false);
                genesys_scanner_calibration(s->dev, sensor);
            });
            catch_all_exceptions(__func__, [&]()
            {
                s->dev->cmd_set->save_power(s->dev, true);
            });
            *myinfo |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
            break;
        }
    case OPT_CLEAR_CALIBRATION:
      s->dev->calibration_cache.clear();

      /* remove file */
      unlink(s->dev->calib_file.c_str());
      /* signals that sensors will have to be read again */
      *myinfo |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
      break;
    case OPT_FORCE_CALIBRATION:
      s->dev->force_calibration = 1;
      s->dev->calibration_cache.clear();
      s->dev->calib_file.clear();

      /* signals that sensors will have to be read again */
      *myinfo |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
      break;

        case OPT_IGNORE_OFFSETS: {
            s->dev->ignore_offsets = true;
            break;
        }
    default:
      DBG(DBG_warn, "%s: can't set unknown option %d\n", __func__, option);
    }
  return status;
}


/* sets and gets scanner option values */
SANE_Status
sane_control_option_impl(SANE_Handle handle, SANE_Int option,
                         SANE_Action action, void *val, SANE_Int * info)
{
    Genesys_Scanner* s = reinterpret_cast<Genesys_Scanner*>(handle);
    auto action_str = (action == SANE_ACTION_GET_VALUE) ? "get" :
                      (action == SANE_ACTION_SET_VALUE) ? "set" :
                      (action == SANE_ACTION_SET_AUTO) ? "set_auto" : "unknown";
    DBG_HELPER_ARGS(dbg, "action = %s, option = %s (%d)", action_str,
                    s->opt[option].name, option);

  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Word cap;
  SANE_Int myinfo = 0;

  if (info)
    *info = 0;

  if (s->scanning)
    {
      DBG(DBG_warn, "%s: don't call this function while scanning (option = %s (%d))\n", __func__,
          s->opt[option].name, option);

      return SANE_STATUS_DEVICE_BUSY;
    }
  if (option >= NUM_OPTIONS || option < 0)
    {
      DBG(DBG_warn, "%s: option %d >= NUM_OPTIONS || option < 0\n", __func__, option);
      return SANE_STATUS_INVAL;
    }

  cap = s->opt[option].cap;

  if (!SANE_OPTION_IS_ACTIVE (cap))
    {
      DBG(DBG_warn, "%s: option %d is inactive\n", __func__, option);
      return SANE_STATUS_INVAL;
    }

  switch (action)
    {
    case SANE_ACTION_GET_VALUE:
      status = get_option_value (s, option, val);
      break;

    case SANE_ACTION_SET_VALUE:
      if (!SANE_OPTION_IS_SETTABLE (cap))
	{
	  DBG(DBG_warn, "%s: option %d is not settable\n", __func__, option);
	  return SANE_STATUS_INVAL;
	}

      status = sanei_constrain_value (s->opt + option, val, &myinfo);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG(DBG_warn, "%s: sanei_constrain_value returned %s\n", __func__,
	      sane_strstatus(status));
	  return status;
	}

      status = set_option_value (s, option, val, &myinfo);
      break;

    case SANE_ACTION_SET_AUTO:
      DBG(DBG_error,
          "%s: SANE_ACTION_SET_AUTO unsupported since no option has SANE_CAP_AUTOMATIC\n",
          __func__);
      status = SANE_STATUS_INVAL;
      break;

    default:
      DBG(DBG_warn, "%s: unknown action %d for option %d\n", __func__, action, option);
      status = SANE_STATUS_INVAL;
      break;
    }

  if (info)
    *info = myinfo;

  return status;
}

SANE_Status sane_control_option(SANE_Handle handle, SANE_Int option,
                                SANE_Action action, void *val, SANE_Int * info)
{
    return wrap_exceptions_to_status_code(__func__, [=]()
    {
        return sane_control_option_impl(handle, option, action, val, info);
    });
}

SANE_Status sane_get_parameters_impl(SANE_Handle handle, SANE_Parameters* params)
{
    DBG_HELPER(dbg);
    Genesys_Scanner* s = reinterpret_cast<Genesys_Scanner*>(handle);

  /* don't recompute parameters once data reading is active, ie during scan */
    if (!s->dev->read_active) {
        calc_parameters(s);
    }
  if (params)
    {
      *params = s->params;

      /* in the case of a sheetfed scanner, when full height is specified
       * we override the computed line number with -1 to signal that we
       * don't know the real document height.
       * We don't do that doing buffering image for digital processing
       */
        if (s->dev->model->is_sheetfed && !s->dev->buffer_image &&
            s->pos_bottom_right_y == s->opt[OPT_BR_Y].constraint.range->max)
        {
	  params->lines = -1;
	}
    }

  return SANE_STATUS_GOOD;
}

SANE_Status sane_get_parameters(SANE_Handle handle, SANE_Parameters* params)
{
    return wrap_exceptions_to_status_code(__func__, [=]()
    {
        return sane_get_parameters_impl(handle, params);
    });
}

SANE_Status sane_start_impl(SANE_Handle handle)
{
    DBG_HELPER(dbg);
    Genesys_Scanner* s = reinterpret_cast<Genesys_Scanner*>(handle);
  SANE_Status status=SANE_STATUS_GOOD;

  if (s->pos_top_left_x >= s->pos_bottom_right_x)
    {
      DBG(DBG_error0, "%s: top left x >= bottom right x --- exiting\n", __func__);
      return SANE_STATUS_INVAL;
    }
  if (s->pos_top_left_y >= s->pos_bottom_right_y)
    {
      DBG(DBG_error0, "%s: top left y >= bottom right y --- exiting\n", __func__);
      return SANE_STATUS_INVAL;
    }

  /* First make sure we have a current parameter set.  Some of the
     parameters will be overwritten below, but that's OK.  */

    calc_parameters(s);
    genesys_start_scan(s->dev, s->lamp_off);

    s->scanning = true;

  /* allocate intermediate buffer when doing dynamic lineart */
    if (s->dev->settings.dynamic_lineart) {
        s->dev->binarize_buffer.clear();
        s->dev->binarize_buffer.alloc(s->dev->settings.pixels);
        s->dev->local_buffer.clear();
        s->dev->local_buffer.alloc(s->dev->binarize_buffer.size() * 8);
    }

  /* if one of the software enhancement option is selected,
   * we do the scan internally, process picture then put it an internal
   * buffer. Since cropping may change scan parameters, we recompute them
   * at the end */
  if (s->dev->buffer_image)
    {
        genesys_buffer_image(s);

      /* check if we need to skip this page, sheetfed scanners
       * can go to next doc while flatbed ones can't */
        if (s->swskip > 0 && IS_ACTIVE(OPT_SWSKIP)) {
          status = sanei_magic_isBlank(&s->params,
                                       s->dev->img_buffer.data(),
                                       SANE_UNFIX(s->swskip));
          if(status == SANE_STATUS_NO_DOCS)
            {
                if (s->dev->model->is_sheetfed) {
                  DBG(DBG_info, "%s: blank page, recurse\n", __func__);
                  return sane_start(handle);
                }
              return status;
            }
        }

        if (s->swdeskew) {
            const auto& sensor = sanei_genesys_find_sensor(s->dev, s->dev->settings.xres,
                                                           s->dev->settings.get_channels(),
                                                           s->dev->settings.scan_method);
            catch_all_exceptions(__func__, [&](){ genesys_deskew(s, sensor); });
        }

        if (s->swdespeck) {
            catch_all_exceptions(__func__, [&](){ genesys_despeck(s); });
        }

        if(s->swcrop) {
            catch_all_exceptions(__func__, [&](){ genesys_crop(s); });
        }

        if(s->swderotate) {
            catch_all_exceptions(__func__, [&](){ genesys_derotate(s); });
        }
    }

  return status;
}

SANE_Status sane_start(SANE_Handle handle)
{
    return wrap_exceptions_to_status_code(__func__, [=]()
    {
        return sane_start_impl(handle);
    });
}

SANE_Status
sane_read_impl(SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len, SANE_Int* len)
{
    DBG_HELPER(dbg);
    Genesys_Scanner* s = reinterpret_cast<Genesys_Scanner*>(handle);
  Genesys_Device *dev;
  size_t local_len;

  if (!s)
    {
      DBG(DBG_error, "%s: handle is null!\n", __func__);
      return SANE_STATUS_INVAL;
    }

  dev=s->dev;
  if (!dev)
    {
      DBG(DBG_error, "%s: dev is null!\n", __func__);
      return SANE_STATUS_INVAL;
    }

  if (!buf)
    {
      DBG(DBG_error, "%s: buf is null!\n", __func__);
      return SANE_STATUS_INVAL;
    }

  if (!len)
    {
      DBG(DBG_error, "%s: len is null!\n", __func__);
      return SANE_STATUS_INVAL;
    }

  *len = 0;

  if (!s->scanning)
    {
      DBG(DBG_warn, "%s: scan was cancelled, is over or has not been initiated yet\n", __func__);
      return SANE_STATUS_CANCELLED;
    }

  DBG(DBG_proc, "%s: start, %d maximum bytes required\n", __func__, max_len);
    DBG(DBG_io2, "%s: bytes_to_read=%zu, total_bytes_read=%zu\n", __func__,
        dev->total_bytes_to_read, dev->total_bytes_read);

  if(dev->total_bytes_read>=dev->total_bytes_to_read)
    {
      DBG(DBG_proc, "%s: nothing more to scan: EOF\n", __func__);

      /* issue park command immediatly in case scanner can handle it
       * so we save time */
        if (!dev->model->is_sheetfed && !(dev->model->flags & GENESYS_FLAG_MUST_WAIT) &&
            !dev->parking)
        {
            dev->cmd_set->slow_back_home(dev, false);
            dev->parking = true;
        }
      return SANE_STATUS_EOF;
    }

  local_len = max_len;

  /* in case of image processing, all data has been stored in
   * buffer_image. So read data from it if it exists, else from scanner */
  if(!dev->buffer_image)
    {
      /* dynamic lineart is another kind of digital processing that needs
       * another layer of buffering on top of genesys_read_ordered_data */
        if (dev->settings.dynamic_lineart) {
          /* if buffer is empty, fill it with genesys_read_ordered_data */
          if(dev->binarize_buffer.avail() == 0)
            {
              /* store gray data */
              local_len=dev->local_buffer.size();
              dev->local_buffer.reset();
                genesys_read_ordered_data(dev, dev->local_buffer.get_write_pos(local_len),
                                          &local_len);
              dev->local_buffer.produce(local_len);

                dev->binarize_buffer.reset();
                genesys_gray_lineart(dev, dev->local_buffer.get_read_pos(),
                                     dev->binarize_buffer.get_write_pos(local_len / 8),
                                     dev->settings.pixels,
                                     local_len/dev->settings.pixels,
                                     dev->settings.threshold);
                dev->binarize_buffer.produce(local_len / 8);
            }

          /* return data from lineart buffer if any, up to the available amount */
          local_len = max_len;
          if (static_cast<std::size_t>(max_len) > dev->binarize_buffer.avail())
            {
              local_len=dev->binarize_buffer.avail();
            }
          if(local_len)
            {
              memcpy(buf, dev->binarize_buffer.get_read_pos(), local_len);
              dev->binarize_buffer.consume(local_len);
            }
        }
      else
        {
            // most usual case, direct read of data from scanner */
            genesys_read_ordered_data(dev, buf, &local_len);
        }
    }
  else /* read data from buffer */
    {
      if(dev->total_bytes_read+local_len>dev->total_bytes_to_read)
        {
          local_len=dev->total_bytes_to_read-dev->total_bytes_read;
        }
      memcpy(buf, dev->img_buffer.data() + dev->total_bytes_read, local_len);
      dev->total_bytes_read+=local_len;
    }

  *len = local_len;
    if (local_len > static_cast<std::size_t>(max_len)) {
      fprintf (stderr, "[genesys] sane_read: returning incorrect length!!\n");
    }
  DBG(DBG_proc, "%s: %d bytes returned\n", __func__, *len);
    return SANE_STATUS_GOOD;
}

SANE_Status sane_read(SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len, SANE_Int* len)
{
    return wrap_exceptions_to_status_code(__func__, [=]()
    {
        return sane_read_impl(handle, buf, max_len, len);
    });
}

void sane_cancel_impl(SANE_Handle handle)
{
    DBG_HELPER(dbg);
    Genesys_Scanner* s = reinterpret_cast<Genesys_Scanner*>(handle);

    // end binary logging if needed
    if (s->dev->binary != nullptr) {
      fclose(s->dev->binary);
        s->dev->binary = nullptr;
    }

    s->scanning = false;
    s->dev->read_active = false;
  s->dev->img_buffer.clear();

  /* no need to end scan if we are parking the head */
    if (!s->dev->parking) {
        s->dev->cmd_set->end_scan(s->dev, &s->dev->reg, true);
    }

  /* park head if flatbed scanner */
    if (!s->dev->model->is_sheetfed) {
        if (!s->dev->parking) {
            s->dev->cmd_set->slow_back_home (s->dev, s->dev->model->flags &
                                                    GENESYS_FLAG_MUST_WAIT);

          s->dev->parking = !(s->dev->model->flags & GENESYS_FLAG_MUST_WAIT);
        }
    }
  else
    {				/* in case of sheetfed scanners, we have to eject the document if still present */
        s->dev->cmd_set->eject_document(s->dev);
    }

  /* enable power saving mode unless we are parking .... */
    if (!s->dev->parking) {
        s->dev->cmd_set->save_power(s->dev, true);
    }

  return;
}

void sane_cancel(SANE_Handle handle)
{
    catch_all_exceptions(__func__, [=]() { sane_cancel_impl(handle); });
}

SANE_Status
sane_set_io_mode_impl(SANE_Handle handle, SANE_Bool non_blocking)
{
    DBG_HELPER_ARGS(dbg, "handle = %p, non_blocking = %s", handle,
                    non_blocking == SANE_TRUE ? "true" : "false");
    Genesys_Scanner* s = reinterpret_cast<Genesys_Scanner*>(handle);

  if (!s->scanning)
    {
      DBG(DBG_error, "%s: not scanning\n", __func__);
      return SANE_STATUS_INVAL;
    }
  if (non_blocking)
    return SANE_STATUS_UNSUPPORTED;
  return SANE_STATUS_GOOD;
}

SANE_Status
sane_set_io_mode(SANE_Handle handle, SANE_Bool non_blocking)
{
    return wrap_exceptions_to_status_code(__func__, [=]()
    {
        return sane_set_io_mode_impl(handle, non_blocking);
    });
}

SANE_Status
sane_get_select_fd_impl(SANE_Handle handle, SANE_Int * fd)
{
    DBG_HELPER_ARGS(dbg, "handle = %p, fd = %p", handle, reinterpret_cast<void*>(fd));
  Genesys_Scanner *s = (Genesys_Scanner*) handle;

  if (!s->scanning)
    {
      DBG(DBG_error, "%s: not scanning\n", __func__);
      return SANE_STATUS_INVAL;
    }
  return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sane_get_select_fd(SANE_Handle handle, SANE_Int * fd)
{
    return wrap_exceptions_to_status_code(__func__, [=]()
    {
        return sane_get_select_fd_impl(handle, fd);
    });
}

GenesysButtonName genesys_option_to_button(int option)
{
    switch (option) {
    case OPT_SCAN_SW: return BUTTON_SCAN_SW;
    case OPT_FILE_SW: return BUTTON_FILE_SW;
    case OPT_EMAIL_SW: return BUTTON_EMAIL_SW;
    case OPT_COPY_SW: return BUTTON_COPY_SW;
    case OPT_PAGE_LOADED_SW: return BUTTON_PAGE_LOADED_SW;
    case OPT_OCR_SW: return BUTTON_OCR_SW;
    case OPT_POWER_SW: return BUTTON_POWER_SW;
    case OPT_EXTRA_SW: return BUTTON_EXTRA_SW;
    default: throw std::runtime_error("Unknown option to convert to button index");
    }
}
