/* sane-find-scanner.c

   Copyright (C) 1997-2001 Oliver Rauch, Henning Meier-Geinitz, and others.

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

 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>

#include "../include/sane/config.h"
#include "../include/sane/sanei_scsi.h"
#include "../include/sane/sanei_usb.h"

#define BACKEND_NAME	findscanner
#include "../include/sane/sanei_debug.h"

static const char *prog_name;

static int verbose;

typedef struct
  {
    unsigned char *cmd;
    size_t size;
  }
scsiblk;

#define INQUIRY					0x12
#define set_inquiry_return_size(icb,val)	icb[0x04]=val
#define IN_periph_devtype_cpu			0x03
#define IN_periph_devtype_scanner		0x06
#define get_inquiry_vendor(in, buf)		strncpy(buf, in + 0x08, 0x08)
#define get_inquiry_product(in, buf)		strncpy(buf, in + 0x10, 0x010)
#define get_inquiry_version(in, buf)		strncpy(buf, in + 0x20, 0x04)
#define get_inquiry_periph_devtype(in)		(in[0] & 0x1f)
#define get_inquiry_additional_length(in)	in[0x04]
#define set_inquiry_length(out,n)		out[0x04]=n-5

static unsigned char inquiryC[] =
  {
    INQUIRY, 0x00, 0x00, 0x00, 0xff, 0x00
  };
static scsiblk inquiry =
  {
    inquiryC, sizeof (inquiryC)
  };


static void
usage (char *msg)
{
  fprintf (stderr, "Usage: %s [-hv] [devname ...]\n", prog_name);
  fprintf (stderr, "\t-h: print this help message\n");
  fprintf (stderr, "\t-v: be verbose\n");
  if (msg)
    fprintf (stderr, "\t%s\n", msg);
}

/* if SCSI generic is optional on your OS, and there is a software test
   which will determine if it is included, add it here. If you are sure
   that SCSI generic was found, return 1. If SCSI generic is always
   available in your OS, return 1 */

static int
check_sg (void)
{
#if defined(__linux__)
  /* Assumption: /proc/devices lines are shorter than 256 characters */
  char line[256], driver[256]= "";
  FILE *stream;

  stream= fopen("/proc/devices", "r");
  /* Likely reason for failure, no /proc => probably no SG either */
  if (stream == NULL) return 0;
 
  while (fgets(line, sizeof(line)-1, stream)) {
    if (sscanf(line, " %*d %s\n", driver) > 0 && !strcmp(driver, "sg"))
      break;
  }
  fclose(stream);

  if (strcmp(driver, "sg")) {
    return 0;
  } else {
    return 1;
  }
#endif
  return 1; /* Give up, and assume yes to avoid false negatives */
}

static void 
scanner_do_inquiry (unsigned char *buffer, int sfd)
{
  size_t size;

  DBG (5, "do_inquiry\n");
  memset (buffer, '\0', 256);	/* clear buffer */

  size = 5; /* first get only 5 bytes to get size of inquiry_return_block */
  set_inquiry_return_size (inquiry.cmd, size);
  sanei_scsi_cmd (sfd, inquiry.cmd, inquiry.size, buffer, &size);

  size = get_inquiry_additional_length (buffer) + 5;

  /* then get inquiry with actual size */
  set_inquiry_return_size (inquiry.cmd, size);
  sanei_scsi_cmd (sfd, inquiry.cmd, inquiry.size, buffer, &size);
}

static void 
scanner_identify_scanner (unsigned char *buffer, int sfd, char *devicename)
{
  unsigned char vendor[9];
  unsigned char product[17];
  unsigned char version[5];
  unsigned char *pp;
  unsigned int devtype;
  static char *devtypes[] =
    {
      "disk", "tape", "printer", "processor", "CD-writer",
      "CD-drive", "scanner", "optical-drive", "jukebox",
      "communicator"
    };

  scanner_do_inquiry (buffer, sfd);	/* get inquiry */
  devtype = get_inquiry_periph_devtype (buffer);
  if (!verbose
      && devtype != IN_periph_devtype_scanner
      /* old HP scanners use the CPU id ... */
      && devtype != IN_periph_devtype_cpu)
    return;				/* no, continue searching */

  get_inquiry_vendor ((char *) buffer, (char *) vendor);
  get_inquiry_product ((char *) buffer, (char *) product);
  get_inquiry_version ((char *) buffer, (char *) version);

  pp = &vendor[7];
  vendor[8] = '\0';
  while (pp >= vendor && (*pp == ' ' || *pp >= 127))
    *pp-- = '\0';

  pp = &product[15];
  product[16] = '\0';
  while (pp >= product && (*pp == ' ' || *pp >= 127))
    *pp-- = '\0';

  pp = &version[3];
  version[4] = '\0';
  while (pp >= version && (*pp == ' ' || *(pp - 1) >= 127))
    *pp-- = '\0';

  printf ("%s: found SCSI %s \"%s %s %s\" at device %s\n", prog_name,
	  devtype < NELEMS(devtypes) ? devtypes[devtype] : "unknown device",
	  vendor, product, version, devicename);
  return;
}

int
main (int argc, char **argv)
{
  unsigned char buffer[16384];
  char **dev_list, **usb_dev_list, *dev_name, **ap;
  int sfd;
  SANE_Bool unknown_found = SANE_FALSE;

  prog_name = strrchr (argv[0], '/');
  if (prog_name)
    ++prog_name;
  else
    prog_name = argv[0];

  for (ap = argv + 1; ap < argv + argc; ++ap)
    {
      if ((*ap)[0] != '-')
	break;
      switch ((*ap)[1])
	{
	case '?':
	case 'h':
	  usage (0);
	  exit (0);

	case 'v':
	  ++verbose;
	}
    }
  if (ap < argv + argc)
    {
      dev_list = ap;
      usb_dev_list = ap;
    }
  else
    {
      static char *default_dev_list[] =
        {
#if defined(__sgi)
	  "/dev/scsi/sc0d1l0", "/dev/scsi/sc0d2l0",
	  "/dev/scsi/sc0d3l0", "/dev/scsi/sc0d4l0", 
	  "/dev/scsi/sc0d5l0", "/dev/scsi/sc0d6l0",
	  "/dev/scsi/sc0d7l0", "/dev/scsi/sc0d8l0", 
	  "/dev/scsi/sc0d9l0", "/dev/scsi/sc0d10l0",
	  "/dev/scsi/sc0d11l0", "/dev/scsi/sc0d12l0", 
	  "/dev/scsi/sc0d13l0", "/dev/scsi/sc0d14l0",
	  "/dev/scsi/sc0d15l0",
	  "/dev/scsi/sc1d1l0", "/dev/scsi/sc1d2l0",
	  "/dev/scsi/sc1d3l0", "/dev/scsi/sc1d4l0", 
	  "/dev/scsi/sc1d5l0", "/dev/scsi/sc1d6l0",
	  "/dev/scsi/sc1d7l0", "/dev/scsi/sc1d8l0", 
	  "/dev/scsi/sc1d9l0", "/dev/scsi/sc1d10l0",
	  "/dev/scsi/sc1d11l0", "/dev/scsi/sc1d12l0", 
	  "/dev/scsi/sc1d13l0", "/dev/scsi/sc1d14l0",
	  "/dev/scsi/sc1d15l0",
	  "/dev/scsi/sc2d1l0", "/dev/scsi/sc2d2l0",
	  "/dev/scsi/sc2d3l0", "/dev/scsi/sc2d4l0", 
	  "/dev/scsi/sc2d5l0", "/dev/scsi/sc2d6l0",
	  "/dev/scsi/sc2d7l0", "/dev/scsi/sc2d8l0", 
	  "/dev/scsi/sc2d9l0", "/dev/scsi/sc2d10l0",
	  "/dev/scsi/sc2d11l0", "/dev/scsi/sc2d12l0", 
	  "/dev/scsi/sc2d13l0", "/dev/scsi/sc2d14l0",
	  "/dev/scsi/sc2d15l0",
	  "/dev/scsi/sc3d1l0", "/dev/scsi/sc3d2l0",
	  "/dev/scsi/sc3d3l0", "/dev/scsi/sc3d4l0", 
	  "/dev/scsi/sc3d5l0", "/dev/scsi/sc3d6l0",
	  "/dev/scsi/sc3d7l0", "/dev/scsi/sc3d8l0", 
	  "/dev/scsi/sc3d9l0", "/dev/scsi/sc3d10l0",
	  "/dev/scsi/sc3d11l0", "/dev/scsi/sc3d12l0", 
	  "/dev/scsi/sc3d13l0", "/dev/scsi/sc3d14l0",
	  "/dev/scsi/sc3d15l0",
	  "/dev/scsi/sc4d1l0", "/dev/scsi/sc4d2l0",
	  "/dev/scsi/sc4d3l0", "/dev/scsi/sc4d4l0", 
	  "/dev/scsi/sc4d5l0", "/dev/scsi/sc4d6l0",
	  "/dev/scsi/sc4d7l0", "/dev/scsi/sc4d8l0", 
	  "/dev/scsi/sc4d9l0", "/dev/scsi/sc4d10l0",
	  "/dev/scsi/sc4d11l0", "/dev/scsi/sc4d12l0", 
	  "/dev/scsi/sc4d13l0", "/dev/scsi/sc4d14l0",
	  "/dev/scsi/sc4d15l0",
	  "/dev/scsi/sc5d1l0", "/dev/scsi/sc5d2l0",
	  "/dev/scsi/sc5d3l0", "/dev/scsi/sc5d4l0", 
	  "/dev/scsi/sc5d5l0", "/dev/scsi/sc5d6l0",
	  "/dev/scsi/sc5d7l0", "/dev/scsi/sc5d8l0", 
	  "/dev/scsi/sc5d9l0", "/dev/scsi/sc5d10l0",
	  "/dev/scsi/sc5d11l0", "/dev/scsi/sc5d12l0", 
	  "/dev/scsi/sc5d13l0", "/dev/scsi/sc5d14l0",
	  "/dev/scsi/sc5d15l0",
	  "/dev/scsi/sc6d1l0", "/dev/scsi/sc6d2l0",
	  "/dev/scsi/sc6d3l0", "/dev/scsi/sc6d4l0", 
	  "/dev/scsi/sc6d5l0", "/dev/scsi/sc6d6l0",
	  "/dev/scsi/sc6d7l0", "/dev/scsi/sc6d8l0", 
	  "/dev/scsi/sc6d9l0", "/dev/scsi/sc6d10l0",
	  "/dev/scsi/sc6d11l0", "/dev/scsi/sc6d12l0", 
	  "/dev/scsi/sc6d13l0", "/dev/scsi/sc6d14l0",
	  "/dev/scsi/sc6d15l0",
	  "/dev/scsi/sc7d1l0", "/dev/scsi/sc7d2l0",
	  "/dev/scsi/sc7d3l0", "/dev/scsi/sc7d4l0", 
	  "/dev/scsi/sc7d5l0", "/dev/scsi/sc7d6l0",
	  "/dev/scsi/sc7d7l0", "/dev/scsi/sc7d8l0", 
	  "/dev/scsi/sc7d9l0", "/dev/scsi/sc7d10l0",
	  "/dev/scsi/sc7d11l0", "/dev/scsi/sc7d12l0", 
	  "/dev/scsi/sc7d13l0", "/dev/scsi/sc7d14l0",
	  "/dev/scsi/sc7d15l0",
	  "/dev/scsi/sc8d1l0", "/dev/scsi/sc8d2l0",
	  "/dev/scsi/sc8d3l0", "/dev/scsi/sc8d4l0", 
	  "/dev/scsi/sc8d5l0", "/dev/scsi/sc8d6l0",
	  "/dev/scsi/sc8d7l0", "/dev/scsi/sc8d8l0", 
	  "/dev/scsi/sc8d9l0", "/dev/scsi/sc8d10l0",
	  "/dev/scsi/sc8d11l0", "/dev/scsi/sc8d12l0", 
	  "/dev/scsi/sc8d13l0", "/dev/scsi/sc8d14l0",
	  "/dev/scsi/sc8d15l0",
	  "/dev/scsi/sc9d1l0", "/dev/scsi/sc9d2l0",
	  "/dev/scsi/sc9d3l0", "/dev/scsi/sc9d4l0", 
	  "/dev/scsi/sc9d5l0", "/dev/scsi/sc9d6l0",
	  "/dev/scsi/sc9d7l0", "/dev/scsi/sc9d8l0", 
	  "/dev/scsi/sc9d9l0", "/dev/scsi/sc9d10l0",
	  "/dev/scsi/sc9d11l0", "/dev/scsi/sc9d12l0", 
	  "/dev/scsi/sc9d13l0", "/dev/scsi/sc9d14l0",
	  "/dev/scsi/sc9d15l0",
	  "/dev/scsi/sc10d1l0", "/dev/scsi/sc10d2l0",
	  "/dev/scsi/sc10d3l0", "/dev/scsi/sc10d4l0", 
	  "/dev/scsi/sc10d5l0", "/dev/scsi/sc10d6l0",
	  "/dev/scsi/sc10d7l0", "/dev/scsi/sc10d8l0", 
	  "/dev/scsi/sc10d9l0", "/dev/scsi/sc10d10l0",
	  "/dev/scsi/sc10d11l0", "/dev/scsi/sc10d12l0", 
	  "/dev/scsi/sc10d13l0", "/dev/scsi/sc10d14l0",
	  "/dev/scsi/sc10d15l0",
	  "/dev/scsi/sc11d1l0", "/dev/scsi/sc11d2l0",
	  "/dev/scsi/sc11d3l0", "/dev/scsi/sc11d4l0", 
	  "/dev/scsi/sc11d5l0", "/dev/scsi/sc11d6l0",
	  "/dev/scsi/sc11d7l0", "/dev/scsi/sc11d8l0", 
	  "/dev/scsi/sc11d9l0", "/dev/scsi/sc11d10l0",
	  "/dev/scsi/sc11d11l0", "/dev/scsi/sc11d12l0", 
	  "/dev/scsi/sc11d13l0", "/dev/scsi/sc11d14l0",
	  "/dev/scsi/sc11d15l0",
	  "/dev/scsi/sc12d1l0", "/dev/scsi/sc12d2l0",
	  "/dev/scsi/sc12d3l0", "/dev/scsi/sc12d4l0", 
	  "/dev/scsi/sc12d5l0", "/dev/scsi/sc12d6l0",
	  "/dev/scsi/sc12d7l0", "/dev/scsi/sc12d8l0", 
	  "/dev/scsi/sc12d9l0", "/dev/scsi/sc12d10l0",
	  "/dev/scsi/sc12d11l0", "/dev/scsi/sc12d12l0", 
	  "/dev/scsi/sc12d13l0", "/dev/scsi/sc12d14l0",
	  "/dev/scsi/sc12d15l0",
	  "/dev/scsi/sc13d1l0", "/dev/scsi/sc13d2l0",
	  "/dev/scsi/sc13d3l0", "/dev/scsi/sc13d4l0", 
	  "/dev/scsi/sc13d5l0", "/dev/scsi/sc13d6l0",
	  "/dev/scsi/sc13d7l0", "/dev/scsi/sc13d8l0", 
	  "/dev/scsi/sc13d9l0", "/dev/scsi/sc13d10l0",
	  "/dev/scsi/sc13d11l0", "/dev/scsi/sc13d12l0", 
	  "/dev/scsi/sc13d13l0", "/dev/scsi/sc13d14l0",
	  "/dev/scsi/sc13d15l0",
	  "/dev/scsi/sc14d1l0", "/dev/scsi/sc14d2l0",
	  "/dev/scsi/sc14d3l0", "/dev/scsi/sc14d4l0", 
	  "/dev/scsi/sc14d5l0", "/dev/scsi/sc14d6l0",
	  "/dev/scsi/sc14d7l0", "/dev/scsi/sc14d8l0", 
	  "/dev/scsi/sc14d9l0", "/dev/scsi/sc14d10l0",
	  "/dev/scsi/sc14d11l0", "/dev/scsi/sc14d12l0", 
	  "/dev/scsi/sc14d13l0", "/dev/scsi/sc14d14l0",
	  "/dev/scsi/sc14d15l0",
	  "/dev/scsi/sc15d1l0", "/dev/scsi/sc15d2l0",
	  "/dev/scsi/sc15d3l0", "/dev/scsi/sc15d4l0", 
	  "/dev/scsi/sc15d5l0", "/dev/scsi/sc15d6l0",
	  "/dev/scsi/sc15d7l0", "/dev/scsi/sc15d8l0", 
	  "/dev/scsi/sc15d9l0", "/dev/scsi/sc15d10l0",
	  "/dev/scsi/sc15d11l0", "/dev/scsi/sc15d12l0", 
	  "/dev/scsi/sc15d13l0", "/dev/scsi/sc15d14l0",
	  "/dev/scsi/sc15d15l0",
#elif defined(__EMX__)
	  "b0t0l0", "b0t1l0", "b0t2l0", "b0t3l0",
	  "b0t4l0", "b0t5l0", "b0t6l0", "b0t7l0",
	  "b1t0l0", "b1t1l0", "b1t2l0", "b1t3l0",
	  "b1t4l0", "b1t5l0", "b1t6l0", "b1t7l0",
	  "b2t0l0", "b2t1l0", "b2t2l0", "b2t3l0",
	  "b2t4l0", "b2t5l0", "b2t6l0", "b2t7l0",
	  "b3t0l0", "b3t1l0", "b3t2l0", "b3t3l0",
	  "b3t4l0", "b3t5l0", "b3t6l0", "b3t7l0",
#elif defined(__linux__)
	  "/dev/scanner",
	  "/dev/sg0", "/dev/sg1", "/dev/sg2", "/dev/sg3",
	  "/dev/sg4", "/dev/sg5", "/dev/sg6", "/dev/sg7",
	  "/dev/sg8", "/dev/sg9",
	  "/dev/sga", "/dev/sgb", "/dev/sgc", "/dev/sgd",
	  "/dev/sge", "/dev/sgf", "/dev/sgg", "/dev/sgh",
	  "/dev/sgi", "/dev/sgj", "/dev/sgk", "/dev/sgl",
	  "/dev/sgm", "/dev/sgn", "/dev/sgo", "/dev/sgp",
	  "/dev/sgq", "/dev/sgr", "/dev/sgs", "/dev/sgt",
	  "/dev/sgu", "/dev/sgv", "/dev/sgw", "/dev/sgx",
	  "/dev/sgy", "/dev/sgz",
#elif defined(__NeXT__)
	  "/dev/sg0a", "/dev/sg0b", "/dev/sg0c", "/dev/sg0d",
	  "/dev/sg0e", "/dev/sg0f", "/dev/sg0g", "/dev/sg0h",
	  "/dev/sg1a", "/dev/sg1b", "/dev/sg1c", "/dev/sg1d",
	  "/dev/sg1e", "/dev/sg1f", "/dev/sg1g", "/dev/sg1h",
	  "/dev/sg2a", "/dev/sg2b", "/dev/sg2c", "/dev/sg2d",
	  "/dev/sg2e", "/dev/sg2f", "/dev/sg2g", "/dev/sg2h",
	  "/dev/sg3a", "/dev/sg3b", "/dev/sg3c", "/dev/sg3d",
	  "/dev/sg3e", "/dev/sg3f", "/dev/sg3g", "/dev/sg3h",
#elif defined(_AIX)
	  "/dev/scanner",
	  "/dev/gsc0",  "/dev/gsc1",  "/dev/gsc2",  "/dev/gsc3", 
	  "/dev/gsc4",  "/dev/gsc5",  "/dev/gsc6",  "/dev/gsc7",
	  "/dev/gsc8",  "/dev/gsc9",  "/dev/gsc10", "/dev/gsc11", 
	  "/dev/gsc12", "/dev/gsc13", "/dev/gsc14", "/dev/gsc15", 
#elif defined(__sun)
	  "/dev/scg0a", "/dev/scg0b", "/dev/scg0c", "/dev/scg0d",
	  "/dev/scg0e", "/dev/scg0f", "/dev/scg0g", 
	  "/dev/scg1a", "/dev/scg1b", "/dev/scg1c", "/dev/scg1d",
	  "/dev/scg1e", "/dev/scg1f", "/dev/scg1g",
	  "/dev/scg2a", "/dev/scg2b", "/dev/scg2c", "/dev/scg2d",
	  "/dev/scg2e", "/dev/scg2f", "/dev/scg2g",
	  "/dev/sg/0", "/dev/sg/1", "/dev/sg/2", "/dev/sg/3",
	  "/dev/sg/4", "/dev/sg/5", "/dev/sg/6",
#elif defined(HAVE_CAMLIB_H)
	  "/dev/scanner", "/dev/scanner0", "/dev/scanner1",
	  "/dev/pass0", "/dev/pass1", "/dev/pass2", "/dev/pass3",
	  "/dev/pass4", "/dev/pass5", "/dev/pass6", "/dev/pass7",
#elif defined(__FreeBSD__)
	  "/dev/uk0", "/dev/uk1", "/dev/uk2", "/dev/uk3", "/dev/uk4",
	  "/dev/uk5", "/dev/uk6",
#elif defined(__NetBSD__)
	  "/dev/uk0", "/dev/uk1", "/dev/uk2", "/dev/uk3", "/dev/uk4",
	  "/dev/uk5", "/dev/uk6",
	  "/dev/ss0", 
#elif defined(__hpux__)
	  /* First controller, id 0-8 */
	  "/dev/rscsi/c0t0d0", "/dev/rscsi/c0t1d0", "/dev/rscsi/c0t2d0",
	  "/dev/rscsi/c0t3d0", "/dev/rscsi/c0t4d0", "/dev/rscsi/c0t5d0",
	  "/dev/rscsi/c0t6d0", "/dev/rscsi/c0t7d0", "/dev/rscsi/c0t8d0",
	  /* Second controller id 0-8 */
	  "/dev/rscsi/c1t0d0", "/dev/rscsi/c1t1d0", "/dev/rscsi/c1t2d0",
	  "/dev/rscsi/c1t3d0", "/dev/rscsi/c1t4d0", "/dev/rscsi/c1t5d0",
	  "/dev/rscsi/c1t6d0", "/dev/rscsi/c1t7d0", "/dev/rscsi/c1t8d0",
#endif
	  0
	};
      static char *usb_default_dev_list[] =
        {
	  "/dev/usb/scanner",
	  "/dev/usb/scanner0", "/dev/usb/scanner1",
	  "/dev/usb/scanner2", "/dev/usb/scanner3",
	  "/dev/usb/scanner4", "/dev/usb/scanner5",
	  "/dev/usb/scanner5", "/dev/usb/scanner7",
	  "/dev/usb/scanner8", "/dev/usb/scanner9",
	  "/dev/usb/scanner10", "/dev/usb/scanner11",
	  "/dev/usb/scanner12", "/dev/usb/scanner13",
	  "/dev/usb/scanner14", "/dev/usb/scanner15",
	  "/dev/usbscanner",
	  "/dev/usbscanner0", "/dev/usbscanner1",
	  "/dev/usbscanner2", "/dev/usbscanner3",
	  "/dev/usbscanner4", "/dev/usbscanner5",
	  "/dev/usbscanner6", "/dev/usbscanner7",
	  "/dev/usbscanner8", "/dev/usbscanner9",
	  "/dev/usbscanner10", "/dev/usbscanner11",
	  "/dev/usbscanner12", "/dev/usbscanner13",
	  "/dev/usbscanner14", "/dev/usbscanner15",
	  0
	};

      dev_list = default_dev_list;
      usb_dev_list = usb_default_dev_list;
    }

  printf (
       "# Note that sane-find-scanner will find any scanner that is connected\n"
       "# to a SCSI bus and some scanners that are connected to the Universal\n"
       "# Serial Bus (USB) depending on your OS. It will even find scanners\n"
       "# that are not supported at all by SANE. It won't find a scanner that\n"
       "# is connected to a parallel or proprietary port.\n\n");

  if (getuid ())
    printf (
     "# You may want to run this program as super-user to find all devices.\n"
     "# Once you found the scanner devices, be sure to adjust access\n"
     "# permissions as necessary.\n\n");

  if (verbose)
    printf ("%s: searching for SCSI scanners:\n", prog_name);
  while ((dev_name = *dev_list++))
    {
      int result;

      if (verbose)
	printf ("%s: checking %s...", prog_name, dev_name);

      result = sanei_scsi_open (dev_name, &sfd, NULL, NULL);

      if (verbose)
        {
	  if (result != 0)
	    printf (" failed to open\n");
          else
	    printf (" open ok\n");
        }

      if (result == 0)
	{
	  scanner_identify_scanner (buffer, sfd, dev_name);
	  sanei_scsi_close (sfd);
	}
    }
  if (!check_sg())
    {
    printf (
       "# If your scanner uses SCSI, you must have a driver for your SCSI\n"
       "# adapter and support for SCSI Generic (sg) in your Operating System\n"
       "# in order for the scanner to be used with SANE. If your scanner is\n"
       "# NOT listed above, check that you have installed the drivers.\n\n");
    }

  sanei_usb_init ();
  if (verbose)
    printf ("%s: searching for USB scanners:\n", prog_name);
  while ((dev_name = *usb_dev_list++))
    {
      SANE_Status result;
      SANE_Word vendor, product;
      SANE_Int fd;

      if (verbose)
	printf ("%s: checking %s...", prog_name, dev_name);

      result = sanei_usb_open (dev_name, &fd);

      if (result != SANE_STATUS_GOOD)
	{
	  if (verbose)
	    printf (" failed to open (status %d)\n", result);
	}
      else
	{
	  result = sanei_usb_get_vendor_product (fd, &vendor, &product);
	  if (result == SANE_STATUS_GOOD)
	    {
	      if (verbose)
		printf (" open ok, vendor and product ids were identified\n");
	      printf ("%s: found USB scanner (vendor = 0x%04x, "
		      "product = 0x%04x) at device %s\n", prog_name, vendor,
		      product, dev_name);
	    }
	  else
	    {
	      if (verbose)
		printf (" open ok, but vendor and product could NOT be "
			"identified\n");
	      printf ("%s: found USB scanner (UNKNOWN vendor and product) "
		      "at device %s\n", prog_name, dev_name);
	      unknown_found = SANE_TRUE;
	    }
	  sanei_usb_close (fd);
	}
    }
  if (unknown_found)
    printf ("\n"
    "# `UNKNOWN vendor and product´ means that there seems to be a scanner\n"
    "# at this device file but the vendor and product ids couldn't be \n"
    "# identified. Currently identification only works with Linux versions\n"
    "# >= 2.4.8. \n");

  if (verbose)
    printf ("%s: done\n", prog_name);
  
  return 0;
}

