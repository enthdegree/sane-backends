/*
 * Prototypes for Epson ESC/I commands
 *
 * Based on Kazuhiro Sasayama previous
 * Work on epson.[ch] file from the SANE package.
 * Please see those files for original copyrights.
 *
 * Copyright (C) 2006 Tower Technologies
 * Author: Alessandro Zummo <a.zummo@towertech.it>
 *
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 */

/* simple scanner commands, ESC <x> */

#define set_focus_position(s,v)		epson2_esc_cmd( s,(s)->hw->cmd->set_focus_position, v)
#define set_color_mode(s,v)		epson2_esc_cmd( s,(s)->hw->cmd->set_color_mode, v)
#define set_data_format(s,v)		epson2_esc_cmd( s,(s)->hw->cmd->set_data_format, v)
#define set_halftoning(s,v)		epson2_esc_cmd( s,(s)->hw->cmd->set_halftoning, v)
#define set_gamma(s,v)			epson2_esc_cmd( s,(s)->hw->cmd->set_gamma, v)
#define set_color_correction(s,v)	epson2_esc_cmd( s,(s)->hw->cmd->set_color_correction, v)
#define set_lcount(s,v)			epson2_esc_cmd( s,(s)->hw->cmd->set_lcount, v)
#define set_bright(s,v)			epson2_esc_cmd( s,(s)->hw->cmd->set_bright, v)
#define mirror_image(s,v)		epson2_esc_cmd( s,(s)->hw->cmd->mirror_image, v)
#define set_speed(s,v)			epson2_esc_cmd( s,(s)->hw->cmd->set_speed, v)
#define set_sharpness(s,v)		epson2_esc_cmd( s,(s)->hw->cmd->set_outline_emphasis, v)
#define set_auto_area_segmentation(s,v)	epson2_esc_cmd( s,(s)->hw->cmd->control_auto_area_segmentation, v)
#define set_film_type(s,v)		epson2_esc_cmd( s,(s)->hw->cmd->set_film_type, v)
#define set_exposure_time(s,v)		epson2_esc_cmd( s,(s)->hw->cmd->set_exposure_time, v)
#define set_bay(s,v)			epson2_esc_cmd( s,(s)->hw->cmd->set_bay, v)
#define set_threshold(s,v)		epson2_esc_cmd( s,(s)->hw->cmd->set_threshold, v)
#define control_extension(s,v)		epson2_esc_cmd( s,(s)->hw->cmd->control_an_extension, v)

SANE_Status set_zoom(Epson_Scanner * s, unsigned char x, unsigned char y);
SANE_Status set_resolution(Epson_Scanner * s, int x, int y);
SANE_Status set_scan_area(Epson_Scanner * s, int x, int y, int width,
			  int height);
SANE_Status set_color_correction_coefficients(Epson_Scanner * s);
SANE_Status set_gamma_table(Epson_Scanner * s);

SANE_Status request_status(SANE_Handle handle, unsigned char *scanner_status);
SANE_Status request_extended_identity(SANE_Handle handle, unsigned char *buf);
SANE_Status request_scanner_status(SANE_Handle handle, unsigned char *buf);
SANE_Status set_scanning_parameter(SANE_Handle handle, unsigned char *buf);
SANE_Status request_command_parameter(SANE_Handle handle, unsigned char *buf);
SANE_Status request_focus_position(SANE_Handle handle,
				   unsigned char *position);
SANE_Status request_push_button_status(SANE_Handle handle,
				       unsigned char *bstatus);
SANE_Status request_identity(SANE_Handle handle, unsigned char **buf, size_t *len);

SANE_Status request_identity2(SANE_Handle handle, unsigned char **buf);
SANE_Status reset(Epson_Scanner * s);
SANE_Status feed(Epson_Scanner * s);
SANE_Status eject(Epson_Scanner * s);
SANE_Status request_extended_status(SANE_Handle handle, unsigned char **data,
				    size_t * data_len);
