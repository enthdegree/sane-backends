/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 BYTEC GmbH Germany
   Written by Helmut Koeberle, Email: helmut.koeberle@bytec.de
   Modified by Manuel Panea <Manuel.Panea@rzg.mpg.de>
   and Markus Mertinat <Markus.Mertinat@Physik.Uni-Augsburg.DE>

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
   If you do not wish that, delete this exception notice. */

/* This file implements the low-level scsi-commands.  */

static SANE_Status
test_unit_ready (int fd)
{
  static u_char cmd[6];
  int status;
  DBG (31, ">> test_unit_ready\n");

  memset (cmd, 0, sizeof (cmd));
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (31, "<< test_unit_ready\n");
  return (status);
}

#ifdef IMPLEMENT_ALL_SCANNER_SCSI_COMMANDS
static SANE_Status
request_sense (int fd, void *buf, size_t *buf_size)
{
  static u_char cmd[6];
  int status;
  DBG (31, ">> request_sense\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x03;
  cmd[4] = 14;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), buf, buf_size);

  DBG (31, "<< request_sense\n");
  return (status);
}
#endif

static SANE_Status
inquiry (int fd, int evpd, void *buf, size_t *buf_size)
{
  static u_char cmd[6];
  int status;
  DBG (31, ">> inquiry\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x12;
  cmd[1] = evpd;
  cmd[2] = evpd ? 0xf0 : 0;
  cmd[4] = evpd ? 74 : 36;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), buf, buf_size);

  DBG (31, "<< inquiry\n");
  return (status);
}


#if 0
static SANE_Status
mode_select (int fd)
{

  static u_char cmd[6 + 12];
  int status;
  DBG (31, ">> mode_select\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x15;
  cmd[1] = 16;
  cmd[4] = 12;
  cmd[6 + 4] = 3;
  cmd[6 + 5] = 6;
  cmd[6 + 8] = 0x02;
  cmd[6 + 9] = 0x58;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (31, "<< mode_select\n");
  return (status);
}
#endif

static SANE_Status
reserve_unit (int fd)
{

  static u_char cmd[6];
  int status;
  DBG (31, ">> reserve_unit\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x16;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (31, "<< reserve_unit\n");
  return (status);
}

#ifdef IMPLEMENT_ALL_SCANNER_SCSI_COMMANDS
static SANE_Status
release_unit (int fd)
{

  static u_char cmd[6];
  int status;
  DBG (31, ">> release_unit\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x17;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (31, "<< release_unit\n");
  return (status);
}
#endif

static SANE_Status
mode_sense (int fd, void *buf, size_t *buf_size)
{

  static u_char cmd[6];
  int status;
  DBG (31, ">> mode_sense\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x1a;
  cmd[2] = 3;
  cmd[4] = 12;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), buf, buf_size);

  DBG (31, "<< mode_sense\n");
  return (status);
}

static SANE_Status
scan (int fd)
{
  static u_char cmd[6 + 1];
  int status;
  DBG (31, ">> scan\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x1b;
  cmd[4] = 1;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (31, "<< scan\n");
  return (status);
}

#ifdef IMPLEMENT_ALL_SCANNER_SCSI_COMMANDS
static SANE_Status
send_diagnostic (int fd)
{
  static u_char cmd[6];
  int status;
  DBG (31, ">> send_diagnostic\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x1d;
  cmd[1] = 4;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (31, "<< send_diagnostic\n");
  return (status);
}
#endif

static SANE_Status
set_window (int fd, void *data)
{
  static u_char cmd[10 + 72];
  int status;
  DBG (31, ">> set_window\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x24;
  cmd[8] = 72;
  memcpy (cmd + 10, data, 72);
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (31, "<< set_window\n");
  return (status);
}

static SANE_Status
get_window (int fd, void *buf, size_t *buf_size)
{
  static u_char cmd[10];
  int status;
  DBG (31, ">> get_window\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x25;
  cmd[1] = 1;
  cmd[8] = 72;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), buf, buf_size);

  DBG (31, "<< get_window\n");
  return (status);
}

static SANE_Status
read_data (int fd, void *buf, size_t *buf_size)
{
  static u_char cmd[10];
  int status;
  DBG (31, ">> read_data\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x28;
  cmd[6] = *buf_size >> 16;
  cmd[7] = *buf_size >> 8;
  cmd[8] = *buf_size;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), buf, buf_size);

  DBG (31, "<< read_data\n");
  return (status);
}

static SANE_Status
medium_position (int fd)
{
  static u_char cmd[10];
  int status;
  DBG (31, ">> medium_position\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x31;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (31, "<< medium_position\n");
  return (status);
}

#ifdef IMPLEMENT_ALL_SCANNER_SCSI_COMMANDS
static SANE_Status
execute_shading (int fd)
{
  static u_char cmd[10];
  int status;
  DBG (31, ">> execute shading\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0xe2;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (31, "<< execute shading\n");
  return (status);
}
#endif

static SANE_Status
execute_auto_focus (int fd, int mode, int speed, int AE, int count,
		    void *buf, size_t *buf_size)
{
  static u_char cmd[10];
  int status;
  DBG (7, ">> execute auto focus\n");
  DBG (7, ">> focus: mode='%d', speed='%d', AE='%d', count='%d'\n",
       mode, speed, AE, count);

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0xe0;
  cmd[1] = mode;
  cmd[2] = (speed << 6) | AE;
  cmd[4] = count;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), buf, buf_size);

  DBG (7, "<< execute auto focus\n");
  return (status);
}

static SANE_Status
set_adf_mode (int fd, u_char priority)
{
  static u_char cmd[6];
  int status;

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0xd4;
  cmd[4] = 0x01;

  status = sanei_scsi_cmd (fd, cmd, 6, 0, 0);
  if (status == SANE_STATUS_GOOD)
  {
    status = sanei_scsi_cmd (fd, &priority, 1, 0, 0);
  }
  return (status);
}

static SANE_Status
get_scan_mode (int fd, u_char page, void *buf, size_t *buf_size)
{
  static u_char cmd[6];
  int status;
  int PageLen = 0x00;

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0xd5;
  cmd[2] = page;

  switch (page)
  {
    case AUTO_DOC_FEEDER_UNIT:
    case TRANSPARENCY_UNIT:
      cmd[4] = 0x0C + PageLen;
    break;

    case SCAN_CONTROL_CONDITIONS :
      cmd[4] = 0x14 + PageLen;
    break;

    default:
      cmd[4] = 0x24 + PageLen;
    break;
  }

  DBG (31, "get scan mode: cmd[4]='0x%0X'\n", cmd[4]);
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), buf, buf_size);

  DBG (31, "<< get scan mode\n");
  return (status);
}


static SANE_Status
define_scan_mode (int fd, u_char page, void *data)
{
  static u_char cmd[6+20];
  int status, i, cmdlen;
  DBG (31, ">> define scan mode\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0xd6;
  cmd[1] = 0x10;
  cmd[4] = (page == TRANSPARENCY_UNIT) ? 0x0c :
    (page == SCAN_CONTROL_CONDITIONS)  ? 0x14 : 0x24;

  memcpy (cmd + 10, data, (page == TRANSPARENCY_UNIT) ? 8 :
	  (page == SCAN_CONTROL_CONDITIONS)  ? 16 : 24);

  for(i = 0; i < sizeof(cmd); i++)
  {
    DBG (31, "define scan mode: cmd[%d]='0x%0X'\n", i, cmd[i]);
  }
  cmdlen = (page == TRANSPARENCY_UNIT) ? 18 :
    (page == SCAN_CONTROL_CONDITIONS)  ? 26 : 34;

  status = sanei_scsi_cmd (fd, cmd, cmdlen, 0, 0);
  DBG (31, "<< define scan mode\n");
  return (status);
}

static SANE_Status
get_density_curve (int fd, int component, void *buf, size_t *buf_size)
{
  static u_char cmd[10];
  static u_char tbuf[256];
  int status, i;
  DBG (31, ">> get_density_curve\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x28;
  cmd[2] = 0x03;
  cmd[4] = component;
  cmd[5] = 0;
  cmd[6] = 0;
  cmd[7] = 1;
  cmd[8] = 0;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), tbuf, buf_size);

  for (i=0; i<256; i++)
  {
    ((u_char *)buf)[i] = tbuf[i];
  }

  DBG (31, "<< get_density_curve\n");
  return (status);
}

#ifdef IMPLEMENT_ALL_SCANNER_SCSI_COMMANDS
static SANE_Status
get_density_curve_data_format (int fd, void *buf, size_t *buf_size)
{
  static u_char cmd[10];
  int status;
  DBG (31, ">> get_density_curve_data_format\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x28;
  cmd[2] = 0x03;
  cmd[4] = 0xff;
  cmd[5] = 0;
  cmd[6] = 0;
  cmd[7] = 0;
  cmd[8] = 14;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), buf, buf_size);

  DBG (31, "<< get_density_curve_data_format\n");
  return (status);
}
#endif

static SANE_Status
set_density_curve (int fd, int component, void *buf, size_t *buf_size)
{
  static u_char cmd[10+256];
  int status, i;
  DBG (31, ">> set_density_curve\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x2a;
  cmd[2] = 0x03;
  cmd[4] = component;
  cmd[5] = 0;
  cmd[6] = 0;
  cmd[7] = 1;
  cmd[8] = 0;

  for (i=0; i<256; i++)
  {
    cmd[i+10] = ((u_char *)buf)[i];
  }
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), buf, buf_size);

  DBG (31, "<< set_density_curve\n");
  return (status);
}


/* static SANE_Status */
/* set_density_curve_data_format (int fd, void *buf, size_t *buf_size) */
/* { */
/*   static u_char cmd[10]; */
/*   int status, i; */
/*   DBG (31, ">> set_density_curve_data_format\n"); */

/*   memset (cmd, 0, sizeof (cmd)); */
/*   cmd[0] = 0x2a; */
/*   cmd[2] = 0x03; */
/*   cmd[4] = 0xff; */
/*   cmd[5] = 0; */
/*   cmd[6] = 0; */
/*   cmd[7] = 0; */
/*   cmd[8] = 14; */
/*   status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), buf, buf_size); */

/*   DBG (31, "<< set_density_curve_data_format\n"); */
/*   return (status); */
/* } */

#ifdef IMPLEMENT_ALL_SCANNER_SCSI_COMMANDS
static SANE_Status
get_power_on_timer (int fd, void *buf, size_t *buf_size)
{
  static u_char cmd[10];
  int status;
  DBG (31, ">> get power on timer\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0xe3;
  cmd[6] = 1;
  cmd[7] = 0;
  cmd[8] = 0;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), buf, buf_size);

  DBG (31, "<< get power on timer\n");
  return (status);
}
#endif

static SANE_Status
get_film_status (int fd, void *buf, size_t *buf_size)
{
  static u_char cmd[10];
  int status;
  DBG (31, ">> get film status\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0xe1;
  cmd[6] = 0;
  cmd[7] = 0;
  cmd[8] = 4;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), buf, buf_size);

  DBG (31, "<< get film status\n");
  return (status);
}

static SANE_Status
get_data_status (int fd, void *buf, size_t *buf_size)
{
  static u_char cmd[10];
  int status;
  DBG (31, ">> get_data_status\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x34;
  cmd[8] = 28;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), buf, buf_size);

  DBG (31, "<< get_data_status\n");
  return (status);
}

