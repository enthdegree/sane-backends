/* sane - Scanner Access Now Easy.
   Copyright (C) 2001 Stéphane Voltz <svoltz@wanadoo.fr>
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

   This file implements a SANE backend for Umax PP flatbed scanners.  */



#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include "../include/sane/config.h"
#include "../include/sane/sanei_debug.h"
#include <errno.h>

#ifdef HAVE_LINUX_PPDEV_H
#include <sys/ioctl.h>
#include <linux/parport.h>
#include <linux/ppdev.h>
#endif

#if defined HAVE_SYS_IO_H && defined HAVE_IOPERM
# include <sys/io.h>		/* GNU libc based Linux */
#elif HAVE_ASM_IO_H
# include <asm/io.h>		/* older Linux */
#elif HAVE_SYS_HW_H
# include <sys/hw.h>		/* OS/2 */
#else
#define IO_SUPPORT_MISSING
#endif

#include "umax_pp_low.h"

#ifndef __IO__
#define __IO__

#define DATA   		gPort+0x00
#define STATUS 		gPort+0x01
#define CONTROL		gPort+0x02
#define EPPADR 		gPort+0x03
#define EPPDATA		gPort+0x04

#define ECPCONTROL	gPort+0x402


#endif

static int Fonc001 (void);
static int FoncSendWord (int *cmd);

static void SetEPPMode (int mode);
static int GetEPPMode (void);
static void SetModel (int model);
static int GetModel (void);
static int RingScanner (void);
static int TestVersion (int no);

static int SendCommand (int cmd);
static void SPPResetLPT (void);
static int SendWord (int *cmd);
static int SendData (int *cmd, int len);
static int ReceiveData (int *cmd, int len);
static int SendLength (int *cmd, int len);


static void Init001 (void);
static int Init002 (int arg);
static int Init005 (int arg);
static int Init021 (void);
static int Init022 (void);

static void NibbleReadBuffer (int size, unsigned char *dest);
static void WriteBuffer (int size, unsigned char *source);
static void EPPReadBuffer (int size, unsigned char *dest);
static void EPPWriteBuffer (int size, unsigned char *source);
static void EPPRead32Buffer (int size, unsigned char *dest);
static int PausedReadBuffer (int size, unsigned char *dest);
static void EPPWrite32Buffer (int size, unsigned char *source);


static int SlowNibbleRegisterRead (int reg);
static int EPPRegisterRead (int reg);
static void ClearRegister (int reg);
static void WriteSlow (int reg, int value);
static void EPPRegisterWrite (int reg, int value);

static int Prologue (void);
static int Epilogue (void);

static int CmdSet (int cmd, int len, int *buffer);
static int CmdGet (int cmd, int len, int *buffer);
static int CmdSetGet (int cmd, int len, int *buffer);


static int CmdGetBuffer (int cmd, int len, unsigned char *buffer);
static int CmdGetBuffer32 (int cmd, int len, unsigned char *buffer);
static int CmdGetBlockBuffer (int cmd, int len, int window,
			      unsigned char *buffer);

static void Bloc2Decode (int *op);
static void Bloc8Decode (int *op);


#define WRITESLOW(x,y) \
	WriteSlow((x),(y)); \
	DBG(16,"WriteSlow(0x%X,0x%X) passed...   (%s:%d)\n",(x),(y),__FILE__,__LINE__);

#define SLOWNIBBLEREGISTEREAD(x,y) \
	tmp=SlowNibbleRegisterRead(x);\
	if(tmp!=y)\
	{\
		DBG(0,"SlowNibbleRegisterRead: found 0x%X expected 0x%X (%s:%d)\n",tmp,y,__FILE__,__LINE__);\
		/*return 0;*/ \
	}\
	DBG(16,"SlowNibbleRegisterRead(0x%X)=0x%X passed... (%s:%d)\n",x,y,__FILE__,__LINE__);


#define EPPREGISTERWRITE(x,y) \
	EPPRegisterWrite((x),(y)); \
	DBG(16,"EPPRegisterWrite(0x%X,0x%X) passed...   (%s:%d)\n",(x),(y),__FILE__,__LINE__);

#define EPPREGISTERREAD(x,y) \
	tmp=EPPRegisterRead(x);\
	if(tmp!=y)\
	{\
		DBG(0,"EPPRegisterRead, found 0x%X expected 0x%X (%s:%d)\n",tmp,y,__FILE__,__LINE__);\
		return 0;\
	}\
	DBG(16,"EPPRegisterRead(0x%X)=0x%X passed... (%s:%d)\n",x,y,__FILE__,__LINE__);


#define TRACE(level,msg)	DBG(level, msg"  (%s:%d)\n",__FILE__,__LINE__);


#define CMDSYNC(x)	if(sanei_umax_pp_CmdSync(x)!=1)\
			{\
				DBG(0,"CmdSync(0x%02X) failed (%s:%d)\n",x,__FILE__,__LINE__);\
				return(0);\
			}\
			TRACE(16,"CmdSync() passed ...")

#define CMDSETGET(cmd,len,sent) if(CmdSetGet(cmd,len,sent)!=1)\
				{\
					DBG(0,"CmdSetGet(0x%02X,%d,sent) failed (%s:%d)\n",cmd,len,__FILE__,__LINE__);\
					return(0);\
				}\
				TRACE(16,"CmdSetGet() passed ...")

#define YOFFSET		40
#define YOFFSET1220P	40
#define YOFFSET2000P	40



#define COMPLETIONWAIT	if(CompletionWait()==0)\
			{\
				DBG(0,"CompletionWait() failed (%s:%d)\n",__FILE__,__LINE__);\
				return(0);\
			}\
			TRACE(16,"CompletionWait() passed ...")

#define MOVE(x,y,t)	if(Move(x,y,t)==0)\
			{\
				DBG(0,"Move(%d,%d,buffer) failed (%s:%d)\n",x,y,__FILE__,__LINE__);\
				return(0);\
			}\
			TRACE(16,"Move() passed ...")

#define CMDGETBUF(cmd,len,sent) if(CmdGetBuffer(cmd,len,sent)!=1)\
				{\
					DBG(0,"CmdGetBuffer(0x%02X,%ld,buffer) failed (%s:%d)\n",cmd,(long)len,__FILE__,__LINE__);\
					return(0);\
				}\
				TRACE(16,"CmdGetBuffer() passed ...")

#define CMDGETBUF32(cmd,len,sent) if(CmdGetBuffer32(cmd,len,sent)!=1)\
				{\
					DBG(0,"CmdGetBuffer32(0x%02X,%ld,buffer) failed (%s:%d)\n",cmd,(long)len,__FILE__,__LINE__);\
					return(0);\
				}\
				TRACE(16,"CmdGetBuffer32() passed ...")

#define CMDSET(cmd,len,sent) if(CmdSet(cmd,len,sent)!=1)\
				{\
					DBG(0,"CmdSet(0x%02X,%d,sent) failed (%s:%d)\n",cmd,len,__FILE__,__LINE__);\
					return(0);\
				}\
				TRACE(16,"CmdSet() passed ...")

#define CMDGET(cmd,len,sent) if(CmdGet(cmd,len,sent)!=1)\
				{\
					DBG(0,"CmdGet(0x%02X,%d,read) failed (%s:%d)\n",cmd,len,__FILE__,__LINE__);\
					return(0);\
				}\
				TRACE(16,"CmdGet() passed ...")



static int gPort = 0x378;

/* global control vars */
static int gControl = 0;
static int gData = 0;
static int g674 = 0;		/* semble indiquer qu'on utilise les IRQ */
static int g67D = 0;
static int g67E = 0;
static int gEPAT = 0;		/* signals fast mode ? */
static int g6FE = 0;

/* default gamma translation table */
static int ggamma[256] =
  { 0x00, 0x06, 0x0A, 0x0D, 0x10, 0x12, 0x14, 0x17, 0x19, 0x1B, 0x1D, 0x1F,
  0x21, 0x23, 0x24, 0x26, 0x28, 0x2A, 0x2B, 0x2D, 0x2E, 0x30, 0x31, 0x33,
  0x34, 0x36, 0x37, 0x39, 0x3A, 0x3B, 0x3D, 0x3E, 0x40, 0x41, 0x42, 0x43,
  0x45, 0x46, 0x47, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4F, 0x50, 0x51, 0x52,
  0x53, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5E, 0x5F, 0x60,
  0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C,
  0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
  0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84,
  0x85, 0x86, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
  0x90, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x97, 0x98, 0x99,
  0x9A, 0x9B, 0x9C, 0x9D, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA2, 0xA3,
  0xA4, 0xA5, 0xA6, 0xA7, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAC, 0xAD,
  0xAE, 0xAF, 0xB0, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB4, 0xB5, 0xB6, 0xB7,
  0xB8, 0xB8, 0xB9, 0xBA, 0xBB, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xBF, 0xC0,
  0xC1, 0xC2, 0xC2, 0xC3, 0xC4, 0xC5, 0xC5, 0xC6, 0xC7, 0xC8, 0xC8, 0xC9,
  0xCA, 0xCB, 0xCB, 0xCC, 0xCD, 0xCE, 0xCE, 0xCF, 0xD0, 0xD1, 0xD1, 0xD2,
  0xD3, 0xD4, 0xD4, 0xD5, 0xD6, 0xD6, 0xD7, 0xD8, 0xD9, 0xD9, 0xDA, 0xDB,
  0xDC, 0xDC, 0xDD, 0xDE, 0xDE, 0xDF, 0xE0, 0xE1, 0xE1, 0xE2, 0xE3, 0xE3,
  0xE4, 0xE5, 0xE6, 0xE6, 0xE7, 0xE8, 0xE8, 0xE9, 0xEA, 0xEA, 0xEB, 0xEC,
  0xEC, 0xED, 0xEE, 0xEF, 0xEF, 0xF0, 0xF1, 0xF1, 0xF2, 0xF3, 0xF3, 0xF4,
  0xF5, 0xF5, 0xF6, 0xF7, 0xF7, 0xF8, 0xF9, 0xF9, 0xFA, 0xFB, 0xFB, 0xFC,
  0xFD, 0xFE, 0xFE, 0xFF
};


/* default gamma translation table */
static int *ggGreen = ggamma;
static int *ggBlue = ggamma;
static int *ggRed = ggamma;
static int gParport = 0;
static int gCancel = 0;


/*****************************************************************************/
/*                    output one byte on given port                          */
/*****************************************************************************/
static void Outb (int port, int value);

/*****************************************************************************/
/*         ouput 'size' bytes stored in 'source' on given port               */
/*****************************************************************************/
static void Outsb (int port, unsigned char *source, int size);

/*****************************************************************************/
/*       ouput 'size' 32 bits words stored in 'source' on given port         */
/*****************************************************************************/
static void Outsw (int port, unsigned char *source, int size);


/*****************************************************************************/
/*                   input one byte from given port                          */
/*****************************************************************************/
static int Inb (int port);

/*****************************************************************************/
/*       input 'size' bytes from given port ans store them in 'dest'         */
/*****************************************************************************/
static void Insb (int port, unsigned char *dest, int size);

/*****************************************************************************/
/*     input 'size' 32 bits word from given port ans store them in 'dest'    */
/*****************************************************************************/
static void Insw (int port, unsigned char *dest, int size);


/*
 * returns 1 if succeds in getting base addr via /proc
 *         0 on failure
 *
 * on successfull return, *addr will hold parport base addr
 */
int
sanei_parport_info (int number, int *addr)
{
  char name[256];
  FILE *fic = NULL;
  char buffer[64], val[16];
  int baseadr, ecpadr;

  /* try 2.4 first */
  sprintf (name, "/proc/sys/dev/parport/parport%d/base-addr", number);
  memset (buffer, 0, 64);
  memset (val, 0, 16);
  fic = fopen (name, "rb");
  if (fic == NULL)
    {
      /* open failure, try 2.2 */
      sprintf (name, "/proc/parport/%d/hardware", number);
      fic = fopen (name, "rb");
      if (fic == NULL)
	{			/* no proc at all */
	  DBG (1, "sanei_parport_info(): no /proc \n");
	  return (0);
	}
      fread (buffer, 64, 1, fic);
      fclose (fic);
      sscanf (buffer, "base: %s", val);
      baseadr = strtol (val, NULL, 16);

    }
  else
    {
      fread (buffer, 64, 1, fic);
      fclose (fic);
      if (sscanf (buffer, "%d %d", &baseadr, &ecpadr) < 1)
	{
	  /* empty base file */
	  return (0);
	}
      *addr = baseadr;
    }
  return (1);
}




/*
 * gain direct acces to IO port, and set parport to the 'right' mode
 * returns 1 on success, 0 an failure
 */


int
sanei_umax_pp_InitPort (int port)
{
  int fd;
#if ((defined HAVE_IOPERM)||(defined HAVE_LINUX_PPDEV_H))
  int mode;
#endif
#ifdef HAVE_LINUX_PPDEV_H
  char strmodes[160];
  char parport_name[16];
  int i;
  int addr;
  int found = 0;
#endif

  /* since this function must be called before */
  /* any other, we put debug init here         */
  DBG_INIT ();

  /* init global var holding port value */
  gPort = port;

#ifdef IO_SUPPORT_MISSING
  DBG (1, "*** Direct I/O unavailable, giving up ***\n");
  return (0);
#else


#ifdef HAVE_LINUX_PPDEV_H
  /* ppdev opening and configuration                               */
  /* we start with /dev/parport0 and go through all /dev/parportx  */
  /* until we find the right one                                   */
  i = 0;
  found = 0;
  sprintf (parport_name, "/dev/parport%d", i);
  fd = open (parport_name, O_RDWR | O_NOCTTY);
  while ((fd != -1) && (!found))
    {
      /* claim port */
      if (ioctl (fd, PPCLAIM))
	{
	  DBG (1, "umax_pp: cannot claim port '%s'\n", parport_name);
	}
      else
	{
	  /* we check if parport does EPP or ECP */
#ifdef PPGETMODES
	  if (ioctl (fd, PPGETMODES, &mode))
	    {
	      DBG (16, "umax_pp: ppdev couldn't gave modes for port '%s'\n",
		   parport_name);
	    }
	  else
	    {
	      sprintf (strmodes, "\n");
	      if (mode & PARPORT_MODE_COMPAT)
		sprintf (strmodes, "%s\t\tPARPORT_MODE_COMPAT\n", strmodes);
	      if (mode & PARPORT_MODE_PCSPP)
		sprintf (strmodes, "%s\t\tPARPORT_MODE_PCSPP\n", strmodes);
	      if (mode & PARPORT_MODE_EPP)
		sprintf (strmodes, "%s\t\tPARPORT_MODE_EPP\n", strmodes);
	      if (mode & PARPORT_MODE_ECP)
		sprintf (strmodes, "%s\t\tPARPORT_MODE_ECP\n", strmodes);
	      DBG (32, "parport modes: %X\n", mode);
	      DBG (32, "parport modes: %s\n", strmodes);
	      if (!(mode & PARPORT_MODE_EPP) && !(mode & PARPORT_MODE_ECP))
		{
		  DBG (1,
		       "port 0x%X does not have EPP or ECP, giving up ...\n",
		       port);
		  mode = IEEE1284_MODE_COMPAT;
		  ioctl (fd, PPSETMODE, &mode);
		  ioctl (fd, PPRELEASE);
		  close (fd);
		  return (0);
		}
	    }

#else
	  DBG (16,
	       "umax_pp: ppdev used to build SANE doesn't have PPGETMODES.\n");
#endif
	  /* prefered mode is EPP */
	  mode = IEEE1284_MODE_EPP;
	  if (ioctl (fd, PPSETMODE, &mode))
	    {
	      DBG (16,
		   "umax_pp: ppdev couldn't set mode to IEEE1284_MODE_EPP for '%s'\n",
		   parport_name);

	      mode = IEEE1284_MODE_ECP;
	      if (ioctl (fd, PPSETMODE, &mode))
		{
		  DBG (16,
		       "umax_pp: ppdev couldn't set mode to IEEE1284_MODE_ECP for '%s'\n",
		       parport_name);
		  DBG (1,
		       "port 0x%X can't be set to EPP or ECP, giving up ...\n",
		       port);

		  mode = IEEE1284_MODE_COMPAT;
		  ioctl (fd, PPSETMODE, &mode);
		  ioctl (fd, PPRELEASE);
		  close (fd);
		  return (0);
		}
	      else
		{
		  DBG (16,
		       "umax_pp: mode set to PARPORT_MODE_ECP for '%s'\n",
		       parport_name);
		}
	    }
	  else
	    {
	      DBG (16,
		   "umax_pp: mode set to PARPORT_MODE_EPP for '%s'\n",
		   parport_name);
	    }


	  /* find the base addr of ppdev */
	  if (sanei_parport_info (i, &addr))
	    {
	      if (gPort == addr)
		{
		  found = 1;
		  DBG (1, "Using /proc info\n");
		}
	    }

	  /* release port */
	  if(!found)
	  {
	      mode = IEEE1284_MODE_COMPAT;
	      ioctl (fd, PPSETMODE, &mode);
	      ioctl (fd, PPRELEASE);
	  }
	}



      /* next parport */
      if (!found)
	{
	  close (fd);
	  i++;
	  sprintf (parport_name, "/dev/parport%d", i);
	  fd = open (parport_name, O_RDONLY | O_NOCTTY);
	}
    }

  if (!found)
    {
      DBG (1, "no relevant /dev/parportx found...\n");
    }
  else
    {
      DBG (1, "Using /dev/parport%d ...\n", i);
      sanei_umax_pp_setparport (fd);
      return (1); 
    }
#endif /* HAVE_LINUX_PPDEV_H */


#ifdef HAVE_SYS_HW_H
  /* gainig io perm under OS/2                    */
  /* IOPL must has been raised to 3 in config.sys */
  /* for EMX portaccess always succeeds           */
  if (!found)
    {
      _portaccess (port, port + 7);
      return (1);
    }
#endif


  /* FreeBSD and NetBSD with compatibility opion 9 */
  /* opening /dev/io raise IOPL to level 3         */
  fd = open ("/dev/io", O_RDWR);
  if (errno == EACCES)
    {
      /* /dev/io exist but process hasn't the right permission */
      DBG (1, "/dev/io could not gain access to 0x%X\n", port);
      return (0);
    }
  if ((errno == ENXIO) || (errno == ENOENT))
    {
      /* /dev/io does not exist */
      DBG (16, "no '/dev/io' device\n");
    }
  else if (errno != 0)
    {
      /* /dev/io we get an unexpected error */
      DBG (1, "opening '/dev/io' got unxepected errno=%d\n", errno);
      return (0);
    }
  else
    return (1);

#ifdef HAVE_IOPERM
  if (ioperm (port, 8, 1) != 0)
    {
      DBG (1, "ioperm could not gain access to 0x%X\n", port);
      return (0);
    }
  /* ECP i/o range */
  if (iopl (3) != 0)
    {
      DBG (1, "iopl could not raise IO permission to level 3\n");
      return (0);
    }
  mode = getuid ();
  setreuid (mode, mode);
  mode = getgid ();
  setregid (mode, mode);
#endif


#endif /* IO_SUPPORT_MISSING */
  return (1);
}






static void
Outb (int port, int value)
{
#ifndef IO_SUPPORT_MISSING

#ifdef HAVE_LINUX_PPDEV_H
  int fd, rc;
  unsigned char val;
#endif

#ifdef HAVE_SYS_HW_H
  _outp8 (port, value);
#else

#ifdef HAVE_LINUX_PPDEV_H
  fd = sanei_umax_pp_getparport ();
  val = (unsigned char) value;
  if (fd > 0)
    {
      /* there should be ECPCONTROL that doesn't go through ppdev */
      /* it will leave when all the I/O will be done with ppdev   */
      switch (port - gPort)
	{
	case 0:
	  rc = ioctl (fd, PPWDATA, &val);
	  return;
	case 2:
	  if (val & 0x20)
	    rc = ioctl (fd, PPDATADIR, &val);
	  else
	    rc = ioctl (fd, PPWCONTROL, &val);
	  return;
	case 0x402:
	  break;
	default:
	  DBG (16, "Outb(0x%03X,0x%02X) escaped ppdev\n", port, value);
	}
    }
#endif /* HAVE_LINUX_PPDEV_H */

  outb (value, port);

#endif /* HAVE_SYS_HW_H      */
#endif /* IO_SUPPORT_MISSING */
}





static int
Inb (int port)
{
  int res = 0xFF;
#ifdef HAVE_LINUX_PPDEV_H
  int fd, rc;
  unsigned char val;
#endif

#ifndef IO_SUPPORT_MISSING

#ifdef HAVE_SYS_HW_H
  res = _inp8 (port) & 0xFF;
#else

#ifdef HAVE_LINUX_PPDEV_H
  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {
      /* there should be ECPCONTROL that doesn't go through ppdev */
      /* it will leave when all the I/O will be done with ppdev   */
      switch (port - gPort)
	{
	case 0:
	  rc = ioctl (fd, PPRDATA, &val);
	  res = val;
	  return res;

	case 1:
	  rc = ioctl (fd, PPRSTATUS, &val);
	  res = val;
	  return res;

	case 2:
	  rc = ioctl (fd, PPRCONTROL, &val);
	  res = val;
	  return res;

	case 0x402:
	  break;

	default:
	  DBG (16, "Inb(0x%03X) escaped ppdev\n", port);
	}
    }
#endif /* HAVE_LINUX_PPDEV_H */

  res = inb (port) & 0xFF;

#endif /* HAVE_SYS_HW_H */

#endif /* IO_SUPPORT_MISSING */
  return res;
}


static void
Insb (int port, unsigned char *dest, int size)
{
#ifndef IO_SUPPORT_MISSING
#ifndef __i386__
  int i;
  for (i = 0; i < size; i++)
    dest[i] = Inb (port);
#else
#ifdef HAVE_SYS_HW_H
  _inps8 (port, dest, size);
#else
  insb (port, dest, size);
#endif
#endif
#endif
}

static void
Outsb (int port, unsigned char *source, int size)
{
#ifndef IO_SUPPORT_MISSING
#ifndef __i386__
  int i;

  for (i = 0; i < size; i++)
    Outb (port, source[i]);
#else
#ifdef HAVE_SYS_HW_H
  _outps8 (port, source, size);
#else
  outsb (port, source, size);
#endif
#endif
#endif
}



/* size = nb words */
static void
Insw (int port, unsigned char *dest, int size)
{
#ifndef IO_SUPPORT_MISSING
#ifndef __i386__
  int i;

  for (i = 0; i < size * 4; i++)
    dest[i] = Inb (port);
#else
#ifdef HAVE_SYS_HW_H
  _inps32 (port, (unsigned long *) dest, size);
#else
  insl (port, dest, size);
#endif
#endif
#endif
}

static void
Outsw (int port, unsigned char *source, int size)
{
#ifndef IO_SUPPORT_MISSING
#ifndef __i386__
  int i;

  for (i = 0; i < size * 4; i++)
    Outb (port, source[i]);
#else
#ifdef HAVE_SYS_HW_H
  _outps32 (port, (unsigned long *) source, size);
#else
  outsw (port, source, size);
#endif
#endif
#endif
}


/* we're trying to gather information on the scanner here, */
/* and published it through an easy interface              */
static int scannerStatus = 0;
static int epp32 = 1;
static int model = 0x15;
static int astra = 1220;

int
sanei_umax_pp_ScannerStatus (void)
{
  /* 0x07 variant returns status with bit 0 or 1 allways set to 1 */
  /* so we mask it out                                            */
  return (scannerStatus & 0xFC);
}

static int
GetEPPMode (void)
{
  if (epp32)
    return (32);
  return (8);
}

static void
SetEPPMode (int mode)
{
  if (mode == 8)
    epp32 = 0;
  else
    epp32 = 1;
}

static int
GetModel (void)
{
  return (model);
}

static void
SetModel (int mod)
{
  model = mod;
}


int
sanei_umax_pp_getparport (void)
{
  return (gParport);
}

void
sanei_umax_pp_setparport (int fd)
{
  gParport = fd;
}

int
sanei_umax_pp_getport (void)
{
  return (gPort);
}

void
sanei_umax_pp_setport (int port)
{
  gPort = port;
}

int
sanei_umax_pp_getastra (void)
{
  return (astra);
}

void
sanei_umax_pp_setastra (int mod)
{
  astra = mod;
}

static int
NibbleRead (void)
{
  int res;
  int tmp;

  res = Inb (STATUS);
  res = Inb (STATUS);
  res = res & 0xF0;
  Outb (CONTROL, 5);
  Outb (CONTROL, 5);
  Outb (CONTROL, 5);
  Outb (CONTROL, 5);
  Outb (CONTROL, 5);
  Outb (CONTROL, 5);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);

  tmp = Inb (STATUS);
  tmp = Inb (STATUS);
  tmp = (tmp & 0xF0) >> 4;
  res = res | tmp;
  Outb (CONTROL, 5);
  Outb (CONTROL, 5);
  Outb (CONTROL, 5);
  Outb (CONTROL, 5);
  Outb (CONTROL, 5);
  Outb (CONTROL, 5);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);

  return res;
}


/******************************************************************************/
/* WriteSlow: write value in register, slow method                            */
/******************************************************************************/
static void
WriteSlow (int reg, int value)
{
  /* select register */
  Outb (DATA, reg | 0x60);
  Outb (DATA, reg | 0x60);
  Outb (CONTROL, 0x01);
  Outb (CONTROL, 0x01);
  Outb (CONTROL, 0x01);

  /* send value */
  Outb (DATA, value);
  Outb (DATA, value);
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x04);
}



/*****************************************************************************/
/* Send command returns 0 on failure, 1 on success                           */
/*****************************************************************************/
static int
SendCommand (int cmd)
{
  int control;
  int tmp;
  int val;
  int i;
  int gReadBuffer[256];		/* read buffer for command 0x10 */


  if (g674 != 0)
    {
      DBG (0, "No scanner attached, SendCommand(0x%X) failed\n", cmd);
      return 0;
    }

  control = Inb (CONTROL) & 0x3F;
  tmp = cmd & 0xF8;


  if ((g67D != 1) && (tmp != 0xE0) && (tmp != 0x20) && (tmp != 0x40)
      && (tmp != 0xD0) && (tmp != 0x08) && (tmp != 0x48))
    {
      Outb (CONTROL, 4);	/* reset printer */
    }
  else
    {
      val = control & 0x1F;
      if (g67D != 1)
	val = val & 0xF;	/* no IRQ */
      val = val | 0x4;
      Outb (CONTROL, val);
      Outb (CONTROL, val);
    }

  /* send sequence */
  Outb (DATA, 0x22);
  Outb (DATA, 0x22);
  Outb (DATA, 0xAA);
  Outb (DATA, 0xAA);
  Outb (DATA, 0x55);
  Outb (DATA, 0x55);
  Outb (DATA, 0x00);
  Outb (DATA, 0x00);
  Outb (DATA, 0xFF);
  Outb (DATA, 0xFF);
  Outb (DATA, 0x87);
  Outb (DATA, 0x87);
  Outb (DATA, 0x78);
  Outb (DATA, 0x78);
  Outb (DATA, cmd);
  Outb (DATA, cmd);

  cmd = cmd & 0xF8;

  if ((g67D == 1) && (cmd == 0xE0))
    {
      val = Inb (CONTROL);
      val = (val & 0xC) | 0x01;
      Outb (CONTROL, val);
      Outb (CONTROL, val);
      val = val & 0xC;
      Outb (CONTROL, val);
      Outb (CONTROL, val);
      goto SendCommandEnd;
    }

  if ((cmd != 8) && (cmd != 0x48))
    {
      tmp = Inb (CONTROL);
      tmp = Inb (CONTROL);
      tmp = tmp & 0x1E;
      if (g67D != 1)
	tmp = tmp & 0xE;
      Outb (CONTROL, tmp);
      Outb (CONTROL, tmp);
    }

  if (cmd == 0x10)
    {
      tmp = NibbleRead ();
      tmp = tmp * 256 + NibbleRead ();
      goto SendCommandEnd;
    }

  if (cmd == 8)
    {
      if (g67D == 1)
	{
	  i = 0;
	  while (i < g67E)
	    {
	      tmp = Inb (CONTROL);
	      tmp = Inb (CONTROL);
	      tmp = (tmp & 0x1E) | 0x1;
	      Outb (CONTROL, tmp);
	      Outb (CONTROL, tmp);
	      gReadBuffer[i] = Inb (STATUS);
	      tmp = tmp & 0x1E;
	      Outb (CONTROL, tmp);
	      Outb (CONTROL, tmp);

	      /* next read */
	      i++;
	      if (i < g67E)
		{
		  Outb (DATA, i | 8);
		  Outb (DATA, i | 8);
		}
	    }
	  goto SendCommandEnd;
	}
      else
	{
	  DBG (0, "UNEXPLORED BRANCH %s:%d\n", __FILE__, __LINE__);
	  return 0;
	}
    }

  /*  */
  if (cmd == 0)
    {
      i = 0;
      do
	{
	  tmp = Inb (CONTROL);
	  tmp = (tmp & 0xE) | 0x1;
	  Outb (CONTROL, tmp);
	  Outb (CONTROL, tmp);
	  tmp = tmp & 0xE;
	  Outb (CONTROL, tmp);
	  Outb (CONTROL, tmp);

	  i++;
	  if (i < g67E)
	    {
	      Outb (DATA, i);
	      Outb (DATA, i);
	    }
	}
      while (i < g67E);
      goto SendCommandEnd;
    }

  if (cmd == 0x48)
    {
      /* this case is unneeded, should fit with the rest */
      tmp = Inb (CONTROL) & 0x1E;
      if (g67D != 1)
	tmp = tmp & 0xE;
      Outb (CONTROL, tmp | 0x1);
      Outb (CONTROL, tmp | 0x1);
      Outb (CONTROL, tmp);
      Outb (CONTROL, tmp);
      goto SendCommandEnd;
    }

  /*  */
  tmp = Inb (CONTROL) & 0x1E;
  if (g67D != 1)
    tmp = tmp & 0xE;
  Outb (CONTROL, tmp | 0x1);
  Outb (CONTROL, tmp | 0x1);
  Outb (CONTROL, tmp);
  Outb (CONTROL, tmp);

  /* success */
SendCommandEnd:

  if (cmd == 0x48)
    Outb (CONTROL, (control & 0xF) | 0x4);
  if (cmd == 0x30)
    Outb (CONTROL, (gControl & 0xF) | 0x4);

  /* end signature */
  Outb (DATA, 0xFF);
  Outb (DATA, 0xFF);

  if (cmd == 8)
    {
      Outb (CONTROL, control);
      return 1;
    }

  if (cmd == 0x30)
    {
      Outb (CONTROL, gControl);
      return 1;
    }

  if (cmd != 0xE0)
    Outb (CONTROL, control);

  return 1;
}


static void
ClearRegister (int reg)
{

  /* choose register */
  Outb (DATA, reg);
  Outb (DATA, reg);
  Outb (CONTROL, 1);
  Outb (CONTROL, 1);
  if ((g6FE == 0) || (g674 != 0))
    {
      Outb (CONTROL, 1);
      Outb (CONTROL, 1);
      Outb (CONTROL, 1);
      Outb (CONTROL, 1);
    }

  /* clears it by not sending new value */
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
}

static void
SPPResetLPT (void)
{
  Outb (CONTROL, 0x04);
}


static int
SlowNibbleRegisterRead (int reg)
{
  int low, high;


  /* send register number */
  Outb (DATA, reg);
  Outb (DATA, reg);

  /* get low nibble */
  Outb (CONTROL, 1);
  Outb (CONTROL, 3);
  Outb (CONTROL, 3);
  Outb (CONTROL, 3);
  Outb (CONTROL, 3);
  low = Inb (STATUS);
  low = Inb (STATUS);

  /* get high nibble */
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
  high = Inb (STATUS);
  high = Inb (STATUS);

  /* merge nibbles and return */
  high = (high & 0xF0) | ((low & 0xF0) >> 4);
  return (high);
}


static void
NibbleReadBuffer (int size, unsigned char *dest)
{
  int high;
  int low;
  int i;
  int count;
  int bytel, byteh;

  /* init transfer */
  Outb (DATA, 7);
  Outb (DATA, 7);
  Outb (CONTROL, 1);
  Outb (CONTROL, 1);
  Outb (CONTROL, 3);
  Outb (CONTROL, 3);
  Outb (CONTROL, 3);
  Outb (DATA, 0xFF);
  Outb (DATA, 0xFF);
  count = (size - 2) / 2;
  i = 0;
  bytel = 0x06;			/* signals low byte of word  */
  byteh = 0x07;			/* signals high byte of word */

  /* read loop */
  while (count > 0)
    {
      /* low nibble byte 0 */
      Outb (CONTROL, bytel);
      Outb (CONTROL, bytel);
      Outb (CONTROL, bytel);
      low = Inb (STATUS);
      if ((low & 0x08) == 0)
	{
	  /* high nibble <> low nibble */
	  Outb (CONTROL, bytel & 0x05);
	  Outb (CONTROL, bytel & 0x05);
	  Outb (CONTROL, bytel & 0x05);
	  high = Inb (STATUS);
	}
      else
	{
	  high = low;
	}
      low = low & 0xF0;
      high = high & 0xF0;
      dest[i] = (unsigned char) (high | (low >> 4));
      i++;

      /* low nibble byte 1 */
      Outb (CONTROL, byteh);
      Outb (CONTROL, byteh);
      Outb (CONTROL, byteh);
      low = Inb (STATUS);
      if ((low & 0x08) == 0)
	{
	  /* high nibble <> low nibble */
	  Outb (CONTROL, byteh & 0x05);
	  Outb (CONTROL, byteh & 0x05);
	  Outb (CONTROL, byteh & 0x05);
	  high = Inb (STATUS);
	}
      else
	{
	  high = low;
	}
      low = low & 0xF0;
      high = high & 0xF0;
      dest[i] = (unsigned char) (high | (low >> 4));
      i++;

      /* next read */
      count--;
    }

  /* final read        */
  /* low nibble byte 0 */
  Outb (CONTROL, bytel);
  Outb (CONTROL, bytel);
  Outb (CONTROL, bytel);
  low = Inb (STATUS);
  if ((low & 0x08) == 0)
    {
      /* high nibble <> low nibble */
      Outb (CONTROL, bytel & 0x05);
      Outb (CONTROL, bytel & 0x05);
      Outb (CONTROL, bytel & 0x05);
      high = Inb (STATUS);
    }
  else
    {
      high = low;
    }
  low = low & 0xF0;
  high = high & 0xF0;
  dest[i] = (unsigned char) (high | (low >> 4));
  i++;

  /* uneven size need an extra read */
  if ((size & 0x01) == 1)
    {
      /* low nibble byte 1 */
      Outb (CONTROL, byteh);
      Outb (CONTROL, byteh);
      Outb (CONTROL, byteh);
      low = Inb (STATUS);
      if ((low & 0x08) == 0)
	{
	  /* high nibble <> low nibble */
	  Outb (CONTROL, byteh & 0x05);
	  Outb (CONTROL, byteh & 0x05);
	  Outb (CONTROL, byteh & 0x05);
	  high = Inb (STATUS);
	}
      else
	{
	  high = low;
	}
      low = low & 0xF0;
      high = high & 0xF0;
      dest[i] = (unsigned char) (high | (low >> 4));
      i++;

      /* swap high/low word */
      count = bytel;
      bytel = byteh;
      byteh = count;
    }

  /* final byte ... */
  Outb (DATA, 0xFD);
  Outb (DATA, 0xFD);
  Outb (DATA, 0xFD);

  /* low nibble */
  Outb (CONTROL, byteh);
  Outb (CONTROL, byteh);
  Outb (CONTROL, byteh);
  low = Inb (STATUS);
  if ((low & 0x08) == 0)
    {
      /* high nibble <> low nibble */
      Outb (CONTROL, byteh & 0x05);
      Outb (CONTROL, byteh & 0x05);
      Outb (CONTROL, byteh & 0x05);
      high = Inb (STATUS);
    }
  else
    {
      high = low;
    }
  low = low & 0xF0;
  high = high & 0xF0;
  dest[i] = (unsigned char) (high | (low >> 4));
  i++;

  /* reset port */
  Outb (DATA, 0x00);
  Outb (DATA, 0x00);
  Outb (CONTROL, 0x04);
}

static void
WriteBuffer (int size, unsigned char *source)
{
  int i;
  int count;
  int val;

  /* init buffer write */
  i = 0;
  count = size / 2;
  Outb (DATA, 0x67);
  Outb (CONTROL, 0x01);
  Outb (CONTROL, 0x01);
  Outb (CONTROL, 0x05);

  /* write loop */
  while (count > 0)
    {
      /* low byte of word */
      val = source[i];
      i++;
      Outb (DATA, val);
      Outb (DATA, val);
      Outb (CONTROL, 0x04);
      Outb (CONTROL, 0x04);
      Outb (CONTROL, 0x04);
      Outb (CONTROL, 0x04);

      /* high byte of word */
      val = source[i];
      i++;
      Outb (DATA, val);
      Outb (DATA, val);
      Outb (CONTROL, 0x05);
      Outb (CONTROL, 0x05);
      Outb (CONTROL, 0x05);
      Outb (CONTROL, 0x05);

      /* next write */
      count--;
    }

  /* termination sequence */
  Outb (CONTROL, 0x05);
  Outb (CONTROL, 0x05);
  Outb (CONTROL, 0x05);
  Outb (CONTROL, 0x05);
  Outb (CONTROL, 0x07);
  Outb (CONTROL, 0x07);
  Outb (CONTROL, 0x07);
  Outb (CONTROL, 0x07);
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x04);
}




static void
Init001 (void)
{
  int i;
  int val;
  int status;

  ClearRegister (0);
  Outb (CONTROL, 0x0C);
  if (g674 != 0)
    {
      Outb (CONTROL, 0x0C);
      Outb (CONTROL, 0x0C);
      Outb (CONTROL, 0x0C);
    }
  Outb (DATA, 0x40);
  if (g674 != 0)
    {
      Outb (DATA, 0x40);
      Outb (DATA, 0x40);
      Outb (DATA, 0x40);
    }
  Outb (CONTROL, 0x06);
  Outb (CONTROL, 0x06);
  Outb (CONTROL, 0x06);
  if (g674 != 0)
    {
      Outb (CONTROL, 0x06);
      Outb (CONTROL, 0x06);
      Outb (CONTROL, 0x06);
    }

  /* sync loop */
  i = 256;
  do
    {
      status = Inb (STATUS);
      i--;
    }
  while ((i > 0) && ((status & 0x40)));
  val = 0x0C;
  if (i > 0)
    {
      Outb (CONTROL, 0x07);
      Outb (CONTROL, 0x07);
      Outb (CONTROL, 0x07);
      if (g674 != 0)
	{
	  Outb (CONTROL, 0x07);
	  Outb (CONTROL, 0x07);
	  Outb (CONTROL, 0x07);
	}
      val = 0x04;
      Outb (CONTROL, val);
      Outb (CONTROL, val);
      Outb (CONTROL, val);
      if (g674 != 0)
	{
	  Outb (CONTROL, val);
	  Outb (CONTROL, val);
	  Outb (CONTROL, val);
	}
    }
  val = val | 0x0C;
  Outb (CONTROL, val);
  Outb (CONTROL, val);
  Outb (CONTROL, val);
  if (g674 != 0)
    {
      Outb (CONTROL, val);
      Outb (CONTROL, val);
      Outb (CONTROL, val);
    }
  val = val & 0xF7;
  Outb (CONTROL, val);
  Outb (CONTROL, val);
  Outb (CONTROL, val);
  if (g674 != 0)
    {
      Outb (CONTROL, val);
      Outb (CONTROL, val);
      Outb (CONTROL, val);
    }
}

/* SPP register reading */
static int
Init002 (int arg)
{
  int data;

  if (arg == 1)
    return (0);
  Outb (DATA, 0x0B);
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x0C);
  Outb (CONTROL, 0x0C);
  Outb (CONTROL, 0x0C);
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x24);
  Outb (CONTROL, 0x24);
  Outb (CONTROL, 0x26);
  Outb (CONTROL, 0x26);

  data = Inb (DATA);
  Outb (CONTROL, 0x04);
  if (data == gEPAT)
    {
      return (1);
    }
  return (0);
}



static int
EPPRegisterRead (int reg)
{
#ifdef HAVE_LINUX_PPDEV_H
  int fd, mode, rc, ex;
  unsigned char breg, bval;
#endif
  int control;
  int value;


#ifdef HAVE_LINUX_PPDEV_H
  /* check we have ppdev working */
  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {
      breg = (unsigned char) (reg);
      mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
      rc = ioctl (fd, PPGETMODE, &ex);
      rc = ioctl (fd, PPSETMODE, &mode);
      rc = write (fd, &breg, 1);

      mode = 1;			/* data_reverse */
      rc = ioctl (fd, PPDATADIR, &mode);

      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
      rc = read (fd, &bval, 1);
      value = bval;

      mode = 0;			/* forward */
      rc = ioctl (fd, PPDATADIR, &mode);
      rc = ioctl (fd, PPSETMODE, &ex);

      return value;
    }
  /* if not, direct harware access */
#endif

  Outb (EPPADR, reg);
  control = Inb (CONTROL);
  control = (control & 0x1F) | 0x20;
  Outb (CONTROL, control);
  value = Inb (EPPDATA);
  control = Inb (CONTROL);
  control = control & 0x1F;
  Outb (CONTROL, control);
  return (value);
}


static void
EPPRegisterWrite (int reg, int value)
{
#ifdef HAVE_LINUX_PPDEV_H
  int fd, mode, rc, ex;
  unsigned char breg, bval;
#endif

  reg = reg | 0x40;

#ifdef HAVE_LINUX_PPDEV_H
  /* check we have ppdev working */
  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {
      breg = (unsigned char) (reg);
      mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
      rc = ioctl (fd, PPGETMODE, &ex);
      rc = ioctl (fd, PPSETMODE, &mode);
      rc = write (fd, &breg, 1);

      bval = (unsigned char) (value);
      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
      rc = write (fd, &bval, 1);

      return;
    }
  /* if not, direct harware access */
#endif
  Outb (EPPADR, reg);
  Outb (EPPDATA, value);
}

static void
EPPBlockMode (int flag)
{
#ifdef HAVE_LINUX_PPDEV_H
  int fd, mode, rc, ex;
  unsigned char bval;

  /* check we have ppdev working */
  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {
      bval = (unsigned char) (flag);
      mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
      rc = ioctl (fd, PPGETMODE, &ex);
      rc = ioctl (fd, PPSETMODE, &mode);
      rc = write (fd, &bval, 1);
      rc = ioctl (fd, PPSETMODE, &ex);

      return;
    }
  /* if not, direct harware access */
#endif
  Outb (EPPADR, flag);
}

static void
EPPReadBuffer (int size, unsigned char *dest)
{
#ifdef HAVE_LINUX_PPDEV_H
  int fd, mode, rc, ex, exf;
#endif
  int control;

#ifdef HAVE_LINUX_PPDEV_H
  /* check we have ppdev working */
  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {
      EPPBlockMode (0x80);
      rc = ioctl (fd, PPGETMODE, &ex);
      rc = ioctl (fd, PPGETFLAGS, &exf);
      mode = 1;			/* data_reverse */
      rc = ioctl (fd, PPDATADIR, &mode);
      mode = PP_FASTREAD;
      rc = ioctl (fd, PPSETFLAGS, &mode);
      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
      rc = read (fd, dest, size-1);

      mode = 0;			/* forward */
      rc = ioctl (fd, PPDATADIR, &mode);
      EPPBlockMode (0xA0);

      mode = 1;			/* data_reverse */
      rc = ioctl (fd, PPDATADIR, &mode);
      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
      rc = read (fd, dest+ size-1,1);

      mode = 0;			/* forward */
      rc = ioctl (fd, PPDATADIR, &mode);
      rc = ioctl (fd, PPSETMODE, &ex);
      rc = ioctl (fd, PPSETFLAGS, &exf);

      return;
    }
  /* if not, direct harware access */
#endif

  EPPBlockMode (0x80);
  control = Inb (CONTROL);
  Outb (CONTROL, (control & 0x1F) | 0x20);	/* reverse */
  Insb (EPPDATA, dest, size - 1);
  control = Inb (CONTROL);

  Outb (CONTROL, (control & 0x1F));		/* forward */
  EPPBlockMode (0xA0);
  control = Inb (CONTROL);

  Outb (CONTROL, (control & 0x1F) | 0x20);	/* reverse */
  Insb (EPPDATA, (unsigned char *) (dest + size - 1), 1);

  control = Inb (CONTROL);
  Outb (CONTROL, (control & 0x1F));		/* forward */
}


static void
EPPWriteBuffer (int size, unsigned char *source)
{
#ifdef HAVE_LINUX_PPDEV_H
  int fd, mode, rc;
#endif


  EPPBlockMode (0xC0);
#ifdef HAVE_LINUX_PPDEV_H
  /* check we have ppdev working */
  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {
      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
      rc = write (fd, source, size);
      return;
    }
  /* if not, direct harware access */
#endif
  Outsb (EPPDATA, source, size);
}


/* returns 0 if mode OK, else -1 */
static int
CheckEPAT (void)
{
  int version;

  version = EPPRegisterRead (0x0B);
  if (version == 0xC7)
    return (0);
  DBG (0, "CheckEPAT: expected EPAT version 0xC7, got 0x%X! (%s:%d)\n",
       version, __FILE__, __LINE__);
  return (-1);

}

static int
Init005 (int arg)
{
  int count = 5;
  int res;

  while (count > 0)
    {
      EPPRegisterWrite (0x0A, arg);
      Outb (DATA, 0xFF);
      res = EPPRegisterRead (0x0A);

      /* failed ? */
      if (res != arg)
	return (1);

      /* ror arg */
      res = arg & 0x01;
      arg = arg / 2;
      if (res == 1)
	arg = arg | 0x80;

      /* next loop */
      count--;
    }
  return (0);
}


static void
Init020 (void)
{
  int control;
  int data;

  Outb (DATA, 0x04);
  Outb (CONTROL, 0x0C);
  data = Inb (DATA);
  control = Inb (CONTROL);
  Outb (CONTROL, control & 0x1F);
  control = Inb (CONTROL);
  Outb (CONTROL, control & 0x1F);
}

/* 1 OK, 0 failure */
static int
Init021 (void)
{
  Init020 ();
  if (SendCommand (0xE0) != 1)
    {
      DBG (0, "Init021: SendCommand(0xE0) failed! (%s:%d)\n", __FILE__,
	   __LINE__);
      return (0);
    }
  ClearRegister (0);
  Init001 ();
  return (1);
}

/* 1 OK, 0 failure */
static int
Init022 (void)
{
  if (Init021 () != 1)
    {
      DBG (0, "Init022: Init021() failed! (%s:%d)\n", __FILE__, __LINE__);
      return (0);
    }
  return (1);
}



static void
EPPRead32Buffer (int size, unsigned char *dest)
{
#ifdef HAVE_LINUX_PPDEV_H
  int fd, mode, rc, ex, exf;
#endif
  int control;

#ifdef HAVE_LINUX_PPDEV_H
  /* check we have ppdev working */
  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {
      EPPBlockMode (0x80);
      rc = ioctl (fd, PPGETMODE, &ex);
      rc = ioctl (fd, PPGETFLAGS, &exf);
      mode = 1;			/* data_reverse */
      rc = ioctl (fd, PPDATADIR, &mode);
      mode = PP_FASTREAD;
      rc = ioctl (fd, PPSETFLAGS, &mode);
      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
      rc = read (fd, dest, size-4);

      rc = read (fd, dest+size-4, 3);

      mode = 0;			/* forward */
      rc = ioctl (fd, PPDATADIR, &mode);
      EPPBlockMode (0xA0);

      mode = 1;			/* data_reverse */
      rc = ioctl (fd, PPDATADIR, &mode);
      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
      rc = read (fd, dest+ size-1,1);

      mode = 0;			/* forward */
      rc = ioctl (fd, PPDATADIR, &mode);
      rc = ioctl (fd, PPSETMODE, &ex);
      rc = ioctl (fd, PPSETFLAGS, &exf);

      return;
    }
  /* if not, direct harware access */
#endif

  EPPBlockMode (0x80);

  control = Inb (CONTROL);
  Outb (CONTROL, (control & 0x1F) | 0x20);
  Insw (EPPDATA, dest, size / 4 - 1);

  Insb (EPPDATA, (unsigned char *) (dest + size - 4), 3);
  control = Inb (CONTROL);
  Outb (CONTROL, (control & 0x1F));

  EPPBlockMode (0xA0);
  control = Inb (CONTROL);
  Outb (CONTROL, (control & 0x1F) | 0x20);

  Insb (EPPDATA, (unsigned char *) (dest + size - 1), 1);
  control = Inb (CONTROL);
  Outb (CONTROL, (control & 0x1F));
}

static void
EPPWrite32Buffer (int size, unsigned char *source)
{
#ifdef HAVE_LINUX_PPDEV_H
  int fd, mode, rc, ex;
#endif
  if ((size % 4) != 0)
    {
      DBG (0, "EPPWrite32Buffer: size %% 4 != 0!! (%s:%d)\n", __FILE__,
	   __LINE__);
    }
  EPPBlockMode (0xC0);
#ifdef HAVE_LINUX_PPDEV_H
  /* check we have ppdev working */
  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {
      mode = PP_FASTWRITE;
      rc = ioctl (fd, PPGETFLAGS, &ex);
      rc = ioctl (fd, PPSETFLAGS, &mode);
      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
      rc = write (fd, source, size);
      rc = ioctl (fd, PPSETFLAGS, &ex);
      return;
    }
  /* if not, direct harware access */
#endif
  Outsw (EPPDATA, source, size / 4);
}






/* returns 0 if ERROR cleared in STATUS within 1024 inb, else 1 */
static int
WaitOnError (void)
{
  int c = 0;
  int count = 1024;
  int status;

  do
    {
      do
	{
	  status = Inb (STATUS) & 0x08;
	  if (status != 0)
	    {
	      count--;
	      if (count == 0)
		c = 1;
	    }
	}
      while ((count > 0) && (status != 0));
      if (status == 0)
	{
	  status = Inb (STATUS) & 0x08;
	  if (status == 0)
	    c = 0;
	}
    }
  while ((status != 0) && (c == 0));
  return (c);
}



#ifdef HAVE_LINUX_PPDEV_H
/* read up to size bytes, returns bytes read */
static int
ParportPausedReadBuffer (int size, unsigned char *dest)
{
  unsigned char status;
  int error;
  int word;
  int bread;
  int c;
  int fd,rc,mode,ex,exf;

  /* init */
  bread = 0;
  error = 0;
  fd=sanei_umax_pp_getparport();
      rc = ioctl (fd, PPGETMODE, &ex);
      rc = ioctl (fd, PPGETFLAGS, &exf);

      mode = 1;			/* data_reverse */
      rc = ioctl (fd, PPDATADIR, &mode);

  if ((size & 0x03) != 0)
    {
      while ((!error) && ((size & 0x03) != 0))
	{
          mode = PP_FASTREAD;
          rc = ioctl (fd, PPSETFLAGS, &mode);
          mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
          rc = ioctl (fd, PPSETMODE, &mode);
          rc = read (fd, dest, 1);
	  size--;
	  dest++;
	  bread++;
	  rc = ioctl (fd, PPRSTATUS, &status);
	  error = status & 0x08;
	}
      if (error)
	{
	  DBG (0, "Read error (%s:%d)\n", __FILE__, __LINE__);
	  return (0);
	}
    }

  /* from here, we read 1 byte, then size/4-1 32 bits words, and then
     3 bytes, pausing on ERROR bit of STATUS */
  size -= 4;

  /* sanity test, seems to be wrongly handled ... */
  if (size == 0)
    {
      DBG (0, "case not handled! (%s:%d)\n", __FILE__, __LINE__);
      return (0);
    }

  word = 0;
  error = 0;
  bread += size;
  do
    {
      do
	{
          rc = read (fd, dest, 1);
	  size--;
	  dest++;
	readstatus:
	  if (size > 0)
	    {
	      rc = ioctl (fd, PPRSTATUS, &status);
	      word = status & 0x10;
	      error = status & 0x08;
	    }
	}
      while ((size > 0) && (!error) && (!word));
    }
  while ((size < 4) && (!error) && (size > 0));

  /* here size=0 or error=8 or word=0x10 */
  if ((word) && (!error) && (size))
    {
      rc = read (fd, dest, 4);
      dest += 4;
      size -= 4;
      if (size != 0)
	error = 0x08;
    }
  if (!error)
    {
      c = 0;
      rc = ioctl (fd, PPRSTATUS, &status);
      error = status & 0x08;
      if (error)
	c = WaitOnError ();
    }
  else
    {				/* 8282 */
      c = WaitOnError ();
      if (c == 0)
	goto readstatus;
    }
  if (c == 1)
    {
      bread -= size;
    }
  else
    {
      bread += 3;
      size = 3;
      do
	{
	  do
	    {
      	      rc = read (fd, dest, 1);
	      dest++;
	      size--;
	      if (size)
		{
                  rc = ioctl (fd, PPRSTATUS, &status);
		  error = status & 0x08;
		  if (!error)
		  {
                    rc = ioctl (fd, PPRSTATUS, &status);
		    error = status & 0x08;
		  }
		}
	    }
	  while ((size > 0) && (!error));
	  c = 0;
	  if (error)
	    c = WaitOnError ();
	}
      while ((size > 0) && (c == 0));
    }

  /* end reading */
  mode = 0;			/* forward */
  rc = ioctl (fd, PPDATADIR, &mode);
  EPPBlockMode (0xA0);

  mode = 1;			/* data_reverse */
  rc = ioctl (fd, PPDATADIR, &mode);
  rc = read (fd, dest, 1);
  bread++;

  mode = 0;			/* forward */
  rc = ioctl (fd, PPDATADIR, &mode);
  rc = ioctl (fd, PPSETMODE, &ex);
  rc = ioctl (fd, PPSETFLAGS, &exf);
  return (bread);
}
#endif


/* read up to size bytes, returns bytes read */
static int
DirectPausedReadBuffer (int size, unsigned char *dest)
{
  int control;
  int status;
  int error;
  int word;
  int read;
  int c;

  /* init */
  read = 0;
  error = 0;
  control = Inb (CONTROL) & 0x1F;
  Outb (CONTROL, control | 0x20);
  if ((size & 0x03) != 0)
    {
      /* 8174 */
      while ((!error) && ((size & 0x03) != 0))
	{
	  Insb (EPPDATA, dest, 1);
	  size--;
	  dest++;
	  read++;
	  status = Inb (STATUS) & 0x1F;
	  error = status & 0x08;
	}
      if (error)
	{
	  DBG (0, "Read error (%s:%d)\n", __FILE__, __LINE__);
	  return (0);
	}
    }

  /* from here, we read 1 byte, then size/4-1 32 bits words, and then
     3 bytes, pausing on ERROR bit of STATUS */
  size -= 4;

  /* sanity test, seems to be wrongly handled ... */
  if (size == 0)
    {
      DBG (0, "case not handled! (%s:%d)\n", __FILE__, __LINE__);
      return (0);
    }

  word = 0;
  error = 0;
  read += size;
  do
    {
      do
	{
	  Insb (EPPDATA, dest, 1);
	  size--;
	  dest++;
	readstatus:
	  if (size > 0)
	    {
	      status = Inb (STATUS) & 0x1F;
	      word = status & 0x10;
	      error = status & 0x08;
	    }
	}
      while ((size > 0) && (!error) && (!word));
    }
  while ((size < 4) && (!error) && (size > 0));

  /* here size=0 or error=8 or word=0x10 */
  if ((word) && (!error) && (size))
    {
      if (epp32)
	Insw (EPPDATA, dest, 1);
      else
	Insb (EPPDATA, dest, 4);
      dest += 4;
      size -= 4;
      if (size != 0)
	error = 0x08;
    }
  if (!error)
    {
      c = 0;
      error = Inb (STATUS) & 0x08;
      if (error)
	c = WaitOnError ();
    }
  else
    {				/* 8282 */
      c = WaitOnError ();
      if (c == 0)
	goto readstatus;
    }
  if (c == 1)
    {
      read -= size;
    }
  else
    {
      read += 3;
      size = 3;
      do
	{
	  do
	    {
	      Insb (EPPDATA, dest, 1);
	      dest++;
	      size--;
	      if (size)
		{
		  error = Inb (STATUS) & 0x08;
		  if (!error)
		    error = Inb (STATUS) & 0x08;
		}
	    }
	  while ((size > 0) && (!error));
	  c = 0;
	  if (error)
	    c = WaitOnError ();
	}
      while ((size > 0) && (c == 0));
    }

  /* end reading */
  control = Inb (CONTROL) & 0x1F;
  Outb (CONTROL, control);
  EPPBlockMode (0xA0);
  control = Inb (CONTROL) & 0x1F;
  Outb (CONTROL, control | 0x20);
  Insb (EPPDATA, dest, 1);
  read++;
  control = Inb (CONTROL) & 0x1F;
  Outb (CONTROL, control);
  return (read);
}


int PausedReadBuffer (int size, unsigned char *dest)
{
#ifdef HAVE_LINUX_PPDEV_H
  if(sanei_umax_pp_getparport ()>0)
  	return(ParportPausedReadBuffer(size,dest));
#endif
  return(DirectPausedReadBuffer(size,dest));
}




/* returns 1 on success, 0 otherwise */
static int
SendWord1220P (int *cmd)
{
  int i;
  int reg;
  int try = 0;

  /* send header */
  reg = EPPRegisterRead (0x19) & 0xF8;
retry:
  EPPRegisterWrite (0x1C, 0x55);
  reg = EPPRegisterRead (0x19) & 0xF8;

  EPPRegisterWrite (0x1C, 0xAA);
  reg = EPPRegisterRead (0x19) & 0xF8;

  /* sync when needed */
  if ((reg & 0x08) == 0x00)
    {
      reg = EPPRegisterRead (0x1C);
      if ((reg & 0x10) != 0x10)
	{
	  DBG (0, "SendWord failed (reg1C=0x%02X)   (%s:%d)\n", reg, __FILE__,
	       __LINE__);
	  return (0);
	}
      for (i = 0; i < 10; i++)
	{
	  usleep (1000);
	  reg = EPPRegisterRead (0x19) & 0xF8;
	  if (reg != 0xC8)
	    {
	      DBG (0, "Unexpected reg19=0x%2X  (%s:%d)\n", reg, __FILE__,
		   __LINE__);
	    }
	}
      do
	{
	  if ((reg != 0xC0) && (reg != 0xC8))
	    {
	      DBG (0, "Unexpected reg19=0x%2X  (%s:%d)\n", reg, __FILE__,
		   __LINE__);
	    }
	  /* 0xF0 certainly means error */
	  if ((reg == 0xC0) || (reg == 0xD0))
	    {
	      try++;
	      goto retry;
	    }
	  reg = EPPRegisterRead (0x19) & 0xF8;
	}
      while (reg != 0xC8);
    }

  /* send word */
  i = 0;
  while ((reg == 0xC8) && (cmd[i] != -1))
    {
      EPPRegisterWrite (0x1C, cmd[i]);
      i++;
      reg = EPPRegisterRead (0x19) & 0xF8;
    }
  TRACE (16, "SendWord() passed ");
  if ((reg != 0xC0) && (reg != 0xD0))
    {
      DBG (0, "SendWord failed  got 0x%02X instead of 0xC0 or 0xD0 (%s:%d)\n",
	   reg, __FILE__, __LINE__);
      DBG (0, "Blindly going on .....\n");
    }
  if (((reg == 0xC0) || (reg == 0xD0)) && (cmd[i] != -1))
    {
      DBG (0, "SendWord failed: short send  (%s:%d)\n", __FILE__, __LINE__);
      return (0);
    }
  reg = EPPRegisterRead (0x1C);
  DBG (16, "SendWord, reg1C=0x%02X (%s:%d)\n", reg, __FILE__, __LINE__);
  /* model 0x07 has always the last bit set to 1, and even bit 1 */
  scannerStatus = reg & 0xFC;
  reg = reg & 0x10;
  if (reg != 0x10)
    {
      DBG (0, "SendWord failed: acknowledge not received (%s:%d)\n", __FILE__,
	   __LINE__);
      return (0);
    }
  if (try)
    {
      DBG (0, "SendWord retry success (retry %d time%s) ... (%s:%d)\n", try,
	   (try > 1) ? "s" : "", __FILE__, __LINE__);
    }
  return (1);
}


/* returns 1 on success, 0 otherwise */
static int
SendWord610P (int *cmd)
{
  int i;
  int tmp;

  Outb (CONTROL, 0x05);
  tmp = Inb (CONTROL);
  Outb (CONTROL, 0x04);

  /* send magic tag */
  tmp = Inb (STATUS);
  if (tmp != 0xC8)
    {
      DBG (0,
	   "SendWord610P failed, expected tmp=0xC8 , found 0x%02X (%s:%d)\n",
	   tmp, __FILE__, __LINE__);
      return (0);
    }
  tmp = Inb (CONTROL);
  Outb (CONTROL, 0x04);
  Outb (EPPDATA, 0x55);

  tmp = Inb (STATUS);
  if (tmp != 0xC8)
    {
      DBG (0,
	   "SendWord610P failed, expected tmp=0xC8 , found 0x%02X (%s:%d)\n",
	   tmp, __FILE__, __LINE__);
      return (0);
    }
  tmp = Inb (CONTROL);
  Outb (CONTROL, 0x04);
  Outb (EPPDATA, 0xAA);

  tmp = Inb (CONTROL);
  Outb (CONTROL, 0xA4);
  for (i = 0; i < 9; i++)
    {
      tmp = Inb (STATUS);
      if (tmp != 0xC8)
	{
	  DBG (0,
	       "SendWord610P failed, expected tmp=0xC8 , found 0x%02X (%s:%d)\n",
	       tmp, __FILE__, __LINE__);
	  return (0);
	}
    }

  i = 0;
  while ((tmp == 0xC8) && (cmd[i] != -1))
    {
      tmp = Inb (STATUS);
      Inb (CONTROL);
      Outb (CONTROL, 0x04);
      Outb (EPPDATA, cmd[i]);
    }

  /* end */
  Outb (DATA, 0xFF);
  tmp = Inb (CONTROL);
  Outb (CONTROL, 0xA4);
  tmp = Inb (STATUS);
  if ((tmp != 0xC0) && (tmp != 0xD0))
    {
      DBG (0,
	   "SendWord610P failed  got 0x%02X instead of 0xC0 or 0xD0 (%s:%d)\n",
	   tmp, __FILE__, __LINE__);
      DBG (0, "Blindly going on .....\n");
    }
  tmp = Inb (EPPDATA);
  scannerStatus = tmp;
  return (1);
}

/*
 * 0: failure
 * 1: success
 */
static int
SendWord (int *cmd)
{
  switch (sanei_umax_pp_getastra ())
    {
    case 610:
      return (SendWord610P (cmd));
    case 1220:
    case 1600:
    case 2000:
      return (SendWord1220P (cmd));
    }
  return (0);
}



/******************************************************************************/
/* RingScanner: returns 1 if scanner present, else 0                          */
/******************************************************************************/
static int
RingScanner (void)
{
  int status;
  int data;
  int control;
  int ret = 1;

  /* save state */
  data = Inb (DATA);
  control = Inb (CONTROL) & 0x1F;

  /* send -irq,+reset */
  Outb (CONTROL, (control & 0xF) | 0x4);

  /* unhandled case */
  if (g674 == 1)
    {
      DBG (1, "OUCH! %s:%d\n", __FILE__, __LINE__);
      return (0);
    }

  /* send ring string */
  Outb (DATA, 0x22);
  Outb (DATA, 0x22);
  Outb (DATA, 0xAA);
  Outb (DATA, 0xAA);
  Outb (DATA, 0x55);
  Outb (DATA, 0x55);
  Outb (DATA, 0x00);
  Outb (DATA, 0x00);
  Outb (DATA, 0xFF);
  Outb (DATA, 0xFF);

  /* OK ? */
  status = Inb (STATUS);
  if ((status & 0xB8) != 0xB8)
    {
      DBG (1, "status %d doesn't match! %s:%d\n", status, __FILE__, __LINE__);
      ret = 0;
    }

  /* if OK send 0x87 */
  if (ret)
    {
      Outb (DATA, 0x87);
      Outb (DATA, 0x87);
      status = Inb (STATUS);
      if ((status & 0xB8) != 0x18)
	{
	  DBG (1, "status %d doesn't match! %s:%d\n", status, __FILE__,
	       __LINE__);
	  ret = 0;
	}
    }

  /* if OK send 0x78 */
  if (ret)
    {
      Outb (DATA, 0x78);
      Outb (DATA, 0x78);
      status = Inb (STATUS);
      if ((status & 0x30) != 0x30)
	{
	  DBG (1, "status %d doesn't match! %s:%d\n", status, __FILE__,
	       __LINE__);
	  ret = 0;
	}
    }

  /* ring OK, send termination */
  if (ret)
    {
      Outb (DATA, 0x08);
      Outb (DATA, 0x08);
      Outb (DATA, 0xFF);
      Outb (DATA, 0xFF);
    }

  /* restore state */
  Outb (CONTROL, control);
  Outb (DATA, data);
  return (ret);
}

/*****************************************************************************/
/* test some version       : returns 1 on success, 0 otherwise               */
/*****************************************************************************/


static int
TestVersion (int no)
{
  int data;
  int status;
  int control;
  int count;
  int tmp;

  data = Inb (DATA);
  control = Inb (CONTROL) & 0x3F;
  Outb (CONTROL, (control & 0x1F) | 0x04);

  /* send magic sequence */
  Outb (DATA, 0x22);
  Outb (DATA, 0x22);
  Outb (DATA, 0x22);
  Outb (DATA, 0x22);
  Outb (DATA, 0xAA);
  Outb (DATA, 0xAA);
  Outb (DATA, 0xAA);
  Outb (DATA, 0xAA);
  Outb (DATA, 0xAA);
  Outb (DATA, 0xAA);
  Outb (DATA, 0x55);
  Outb (DATA, 0x55);
  Outb (DATA, 0x55);
  Outb (DATA, 0x55);
  Outb (DATA, 0x55);
  Outb (DATA, 0x55);
  Outb (DATA, 0x00);
  Outb (DATA, 0x00);
  Outb (DATA, 0x00);
  Outb (DATA, 0x00);
  Outb (DATA, 0x00);
  Outb (DATA, 0x00);
  Outb (DATA, 0xFF);
  Outb (DATA, 0xFF);
  Outb (DATA, 0xFF);
  Outb (DATA, 0xFF);
  Outb (DATA, 0xFF);
  Outb (DATA, 0xFF);
  Outb (DATA, 0x87);
  Outb (DATA, 0x87);
  Outb (DATA, 0x87);
  Outb (DATA, 0x87);
  Outb (DATA, 0x87);
  Outb (DATA, 0x87);
  Outb (DATA, 0x78);
  Outb (DATA, 0x78);
  Outb (DATA, 0x78);
  Outb (DATA, 0x78);
  Outb (DATA, 0x78);
  Outb (DATA, 0x78);
  tmp = no | 0x88;
  Outb (DATA, tmp);
  Outb (DATA, tmp);
  Outb (DATA, tmp);
  Outb (DATA, tmp);
  Outb (DATA, tmp);
  Outb (DATA, tmp);

  /* test status */
  status = Inb (STATUS);
  status = Inb (STATUS);
  if ((status & 0xB8) != 0)
    {
      /* 1600P fails here */
      DBG (64, "status %d doesn't match! %s:%d\n", status, __FILE__,
	   __LINE__);
      Outb (CONTROL, control);
      Outb (DATA, data);
      return 0;
    }

  count = 0xF0;
  do
    {
      tmp = no | 0x80;
      Outb (DATA, tmp);
      Outb (DATA, tmp);
      Outb (DATA, tmp);
      Outb (DATA, tmp);
      Outb (DATA, tmp);
      Outb (DATA, tmp);
      tmp = no | 0x88;
      Outb (DATA, tmp);
      Outb (DATA, tmp);
      Outb (DATA, tmp);
      Outb (DATA, tmp);
      Outb (DATA, tmp);
      Outb (DATA, tmp);

      /* command received ? */
      status = Inb (STATUS);
      status = ((status << 1) & 0x70) | (status & 0x80);
      if (status != count)
	{
	  /* since failure is expected, we dont't alaways print */
	  /* this message ...                                   */
	  DBG (2, "status %d doesn't match count 0x%X! %s:%d\n", status,
	       count, __FILE__, __LINE__);
	  Outb (CONTROL, control);
	  Outb (DATA, data);
	  return (0);
	}

      /* next */
      count -= 0x10;
    }
  while (count > 0);

  /* restore port , successful exit */
  Outb (CONTROL, control);
  Outb (DATA, data);
  return 1;
}


/* sends len bytes to scanner        */
/* needs data channel to be set up   */
/* returns 1 on success, 0 otherwise */
static int
SendLength (int *cmd, int len)
{
  int i;
  int reg, wait;
  int try = 0;

  /* send header */
retry:
  wait = EPPRegisterRead (0x19) & 0xF8;

  EPPRegisterWrite (0x1C, 0x55);
  reg = EPPRegisterRead (0x19) & 0xF8;

  EPPRegisterWrite (0x1C, 0xAA);
  reg = EPPRegisterRead (0x19) & 0xF8;

  /* sync when needed */
  if ((wait & 0x08) == 0x00)
    {
      reg = EPPRegisterRead (0x1C);
      while ((reg & 0x10) != 0x10)
	{
	  DBG (0,
	       "SendLength failed, expected reg & 0x10=0x10 , found 0x%02X (%s:%d)\n",
	       reg, __FILE__, __LINE__);
	  if (try > 10)
	    {
	      DBG (0, "Aborting...\n");
	      return (0);
	    }
	  else
	    {
	      DBG (0, "Retrying ...\n");
	    }
	  /* resend */
	  Epilogue ();
	  Prologue ();
	  try++;
	  goto retry;
	}
      for (i = 0; i < 10; i++)
	{
	  reg = EPPRegisterRead (0x19) & 0xF8;
	  if (reg != 0xC8)
	    {
	      DBG (0, "Unexpected reg19=0x%2X  (%s:%d)\n", reg, __FILE__,
		   __LINE__);
	      /* 0xF0 certainly means error */
	      if ((reg == 0xC0) || (reg == 0xD0) || (reg == 0x80))
		{
		  /* resend */
		  try++;
		  if (try > 20)
		    {
		      DBG (0, "SendLength retry failed (%s:%d)\n", __FILE__,
			   __LINE__);
		      return (0);
		    }

		  Epilogue ();
		  SendCommand (0x00);
		  SendCommand (0xE0);
		  Outb (DATA, 0x00);
		  Outb (CONTROL, 0x01);
		  Outb (CONTROL, 0x04);
		  SendCommand (0x30);

		  Prologue ();
		  goto retry;
		}
	    }
	}
      do
	{
	  if ((reg != 0xC0) && (reg != 0xD0) && (reg != 0xC8))
	    {
	      /* status has changed while waiting */
	      /* but it's too early               */
	      DBG (0, "Unexpected reg19=0x%2X  (%s:%d)\n", reg, __FILE__,
		   __LINE__);
	    }
	  /* 0xF0 certainly means error */
	  if ((reg == 0xC0) || (reg == 0xD0) || (reg == 0x80))
	    {
	      /* resend */
	      try++;
	      Epilogue ();

	      SendCommand (0x00);
	      SendCommand (0xE0);
	      Outb (DATA, 0x00);
	      Outb (CONTROL, 0x01);
	      Outb (CONTROL, 0x04);
	      SendCommand (0x30);

	      Prologue ();
	      goto retry;
	    }
	  reg = EPPRegisterRead (0x19) & 0xF8;
	}
      while (reg != 0xC8);
    }

  /* send bytes */
  i = 0;
  while ((reg == 0xC8) && (i < len))
    {
      /* write byte */
      EPPRegisterWrite (0x1C, cmd[i]);
      reg = EPPRegisterRead (0x19) & 0xF8;

      /* 1B handling: escape it to confirm value  */
      if (cmd[i] == 0x1B)
	{
	  EPPRegisterWrite (0x1C, cmd[i]);
	  reg = EPPRegisterRead (0x19) & 0xF8;
	}
      i++;
    }
  DBG (16, "SendLength, reg19=0x%02X (%s:%d)\n", reg, __FILE__, __LINE__);
  if ((reg != 0xC0) && (reg != 0xD0))
    {
      DBG (0,
	   "SendLength failed  got 0x%02X instead of 0xC0 or 0xD0 (%s:%d)\n",
	   reg, __FILE__, __LINE__);
      DBG (0, "Blindly going on .....\n");
    }

  /* check if 'finished status' received too early */
  if (((reg == 0xC0) || (reg == 0xD0)) && (i != len))
    {
      DBG (0, "SendLength failed: sent only %d bytes out of %d (%s:%d)\n", i,
	   len, __FILE__, __LINE__);
      return (0);
    }


  reg = EPPRegisterRead (0x1C);
  DBG (16, "SendLength, reg1C=0x%02X (%s:%d)\n", reg, __FILE__, __LINE__);

  /* model 0x07 has always the last bit set to 1 */
  scannerStatus = reg & 0xFC;
  reg = reg & 0x10;
  if (reg != 0x10)
    {
      DBG (0, "SendLength failed: acknowledge not received (%s:%d)\n",
	   __FILE__, __LINE__);
      return (0);
    }
  if (try)
    {
      DBG (0, "SendLength retry success (retry %d time%s) ... (%s:%d)\n", try,
	   (try > 1) ? "s" : "", __FILE__, __LINE__);
    }
  return (1);
}


/* sends data bytes to scanner       */
/* needs data channel to be set up   */
/* returns 1 on success, 0 otherwise */
static int
SendData (int *cmd, int len)
{
  int i;
  int reg;

  /* send header */
  reg = EPPRegisterRead (0x19) & 0xF8;

  /* send bytes */
  i = 0;
  while ((reg == 0xC8) && (i < len))
    {
      /* write byte */
      EPPRegisterWrite (0x1C, cmd[i]);
      reg = EPPRegisterRead (0x19) & 0xF8;

      /* 1B handling: escape it to confirm value  */
      if (cmd[i] == 0x1B)
	{
	  EPPRegisterWrite (0x1C, 0x1B);
	  reg = EPPRegisterRead (0x19) & 0xF8;
	}

      /* escape 55 AA pattern by adding 1B */
      if ((i < len - 1) && (cmd[i] == 0x55) && (cmd[i + 1] == 0xAA))
	{
	  EPPRegisterWrite (0x1C, 0x1B);
	  reg = EPPRegisterRead (0x19) & 0xF8;
	}

      /* next value */
      i++;
    }
  DBG (16, "SendData, reg19=0x%02X (%s:%d)\n", reg, __FILE__, __LINE__);
  if ((reg != 0xC0) && (reg != 0xD0))
    {
      DBG (0, "SendData failed  got 0x%02X instead of 0xC0 or 0xD0 (%s:%d)\n",
	   reg, __FILE__, __LINE__);
      DBG (0, "Blindly going on .....\n");
    }

  /* check if 'finished status' received too early */
  if (((reg == 0xC0) || (reg == 0xD0)) && (i < len))
    {
      DBG (0, "SendData failed: sent only %d bytes out of %d (%s:%d)\n", i,
	   len, __FILE__, __LINE__);
      return (0);
    }


  reg = EPPRegisterRead (0x1C);
  DBG (16, "SendData, reg1C=0x%02X (%s:%d)\n", reg, __FILE__, __LINE__);

  /* model 0x07 has always the last bit set to 1 */
  scannerStatus = reg & 0xFC;
  reg = reg & 0x10;
  if (reg != 0x10)
    {
      DBG (0, "SendData failed: acknowledge not received (%s:%d)\n", __FILE__,
	   __LINE__);
      return (0);
    }
  return (1);
}


/* receive data bytes from scanner   */
/* needs data channel to be set up   */
/* returns 1 on success, 0 otherwise */
/* uses PausedReadBuffer             */
static int
PausedReadData (int size, unsigned char *dest)
{
  int reg;
  int tmp;
  int read;

  EPPREGISTERWRITE (0x0E, 0x0D);
  EPPREGISTERWRITE (0x0F, 0x00);
  reg = EPPRegisterRead (0x19) & 0xF8;
  if ((reg != 0xC0) && (reg != 0xD0))
    {
      DBG (0, "Unexpected reg19: 0x%02X instead of 0xC0 or 0xD0 (%s:%d)\n",
	   reg, __FILE__, __LINE__);
      return (0);
    }
  EPPREGISTERREAD (0x0C, 0x04);
  EPPREGISTERWRITE (0x0C, 0x44);	/* sets data direction ? */
  EPPBlockMode (0x80);
  read = PausedReadBuffer (size, dest);
  if (read < size)
    {
      DBG (16,
	   "PausedReadBuffer(%d,dest) failed, only got %d bytes (%s:%d)\n",
	   size, read, __FILE__, __LINE__);
      return (0);
    }
  DBG (16, "PausedReadBuffer(%d,dest) passed (%s:%d)\n", size, __FILE__,
       __LINE__);
  EPPREGISTERWRITE (0x0E, 0x0D);
  EPPREGISTERWRITE (0x0F, 0x00);
  return (1);
}



/* receive data bytes from scanner   */
/* needs data channel to be set up   */
/* returns 1 on success, 0 otherwise */
static int
ReceiveData (int *cmd, int len)
{
  int i;
  int reg;

  /* send header */
  reg = EPPRegisterRead (0x19) & 0xF8;

  /* send bytes */
  i = 0;
  while (((reg == 0xD0) || (reg == 0xC0)) && (i < len))
    {
      /* write byte */
      cmd[i] = EPPRegisterRead (0x1C);
      reg = EPPRegisterRead (0x19) & 0xF8;
      i++;
    }
  DBG (16, "ReceiveData, reg19=0x%02X (%s:%d)\n", reg, __FILE__, __LINE__);
  if ((reg != 0xC0) && (reg != 0xD0))
    {
      DBG (0, "SendData failed  got 0x%02X instead of 0xC0 or 0xD0 (%s:%d)\n",
	   reg, __FILE__, __LINE__);
      DBG (0, "Blindly going on .....\n");
    }

  /* check if 'finished status' received to early */
  if (((reg == 0xC0) || (reg == 0xD0)) && (i != len))
    {
      DBG (0,
	   "ReceiveData failed: received only %d bytes out of %d (%s:%d)\n",
	   i, len, __FILE__, __LINE__);
      return (0);
    }


  reg = EPPRegisterRead (0x1C);
  DBG (16, "ReceiveData, reg1C=0x%02X (%s:%d)\n", reg, __FILE__, __LINE__);

  /* model 0x07 has always the last bit set to 1 */
  scannerStatus = reg & 0xFC;
  reg = reg & 0x10;
  if (reg != 0x10)
    {
      DBG (0, "ReceiveData failed: acknowledge not received (%s:%d)\n",
	   __FILE__, __LINE__);
      return (0);
    }
  return (1);
}


/* 1=success, 0 failed */
static int
Fonc001 (void)
{
  int i;
  int res;
  int reg;

  res = 1;
  while (res == 1)
    {
      EPPRegisterWrite (0x1A, 0x0C);
      EPPRegisterWrite (0x18, 0x40);

      /* send 0x06 */
      EPPRegisterWrite (0x1A, 0x06);
      for (i = 0; i < 10; i++)
	{
	  reg = EPPRegisterRead (0x19) & 0xF8;
	  if ((reg & 0x78) == 0x38)
	    {
	      res = 0;
	      break;
	    }
	}
      if (res == 1)
	{
	  EPPRegisterWrite (0x1A, 0x00);
	  EPPRegisterWrite (0x1A, 0x0C);
	}
    }

  /* send 0x07 */
  EPPRegisterWrite (0x1A, 0x07);
  res = 1;
  for (i = 0; i < 10; i++)
    {
      reg = EPPRegisterRead (0x19) & 0xF8;
      if ((reg & 0x78) == 0x38)
	{
	  res = 0;
	  break;
	}
    }
  if (res != 0)
    return (0);

  /* send 0x04 */
  EPPRegisterWrite (0x1A, 0x04);
  res = 1;
  for (i = 0; i < 10; i++)
    {
      reg = EPPRegisterRead (0x19) & 0xF8;
      if ((reg & 0xF8) == 0xF8)
	{
	  res = 0;
	  break;
	}
    }
  if (res != 0)
    return (0);

  /* send 0x05 */
  EPPRegisterWrite (0x1A, 0x05);
  res = 1;
  for (i = 0; i < 10; i++)
    {
      reg = EPPRegisterRead (0x1A);
      if (reg == 0x05)
	{
	  res = 0;
	  break;
	}
    }
  if (res != 0)
    return (0);

  /* end */
  EPPRegisterWrite (0x1A, 0x84);
  return (1);
}







/* 1 OK, 0 failed */
static int
FoncSendWord (int *cmd)
{
  int reg, tmp;

  Init022 ();
  reg = EPPRegisterRead (0x0B);
  if (reg != gEPAT)
    {
      /* return -1 */
      DBG (0, "Error! expected reg0B=0x%02X, found 0x%02X! (%s:%d) \n", gEPAT,
	   reg, __FILE__, __LINE__);
      return (0);
    }
  reg = EPPRegisterRead (0x0D);
  reg = (reg & 0xE8) | 0x43;
  EPPRegisterWrite (0x0D, reg);
  EPPREGISTERWRITE (0x0C, 0x04);
  reg = EPPRegisterRead (0x0A);
  if (reg != 0x00)
    {
      DBG (16, "Warning! expected reg0A=0x00, found 0x%02X! (%s:%d) \n", reg,
	   __FILE__, __LINE__);
    }
  EPPREGISTERWRITE (0x0A, 0x1C);
  EPPREGISTERWRITE (0x08, 0x21);
  EPPREGISTERWRITE (0x0E, 0x0F);
  EPPREGISTERWRITE (0x0F, 0x0C);
  EPPREGISTERWRITE (0x0A, 0x1C);
  EPPREGISTERWRITE (0x0E, 0x10);
  EPPREGISTERWRITE (0x0F, 0x1C);
  if (SendWord (cmd) == 0)
    {
      DBG (0, "SendWord(cmd) failed (%s:%d)\n", __FILE__, __LINE__);
    }

  /* termination sequence */
  EPPREGISTERWRITE (0x0A, 0x00);
  EPPREGISTERREAD (0x0D, 0x40);
  EPPREGISTERWRITE (0x0D, 0x00);
  if (GetModel () != 0x07)
    SendCommand (40);
  SendCommand (30);
  Outb (DATA, 0x04);
  tmp = Inb (CONTROL) & 0x1F;
  Outb (CONTROL, tmp);
  return (1);
}


int
CmdSetDataBuffer (int *data)
{
  int cmd1[] = { 0x00, 0x00, 0x22, 0x88, -1 };	/* 34 bytes write on channel 8 */
  int cmd2[] =
    { 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C, 0x00, 0x03, 0xC1, 0x80,
    0x00, 0x20, 0x02, 0x00, 0x16, 0x41, 0xE0, 0xAC, 0x03, 0x03, 0x00, 0x00,
    0x46, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, -1
  };
  int cmd3[] = { 0x00, 0x08, 0x00, 0x84, -1 };	/* 2048 bytes size write on channel 4 (data) */
  int cmd4[] = { 0x00, 0x08, 0x00, 0xC4, -1 };	/* 2048 bytes size read on channel 4 (data) */
  int i;
  unsigned char dest[2048];

  /* CmdSet(8,34,cmd2), but without prologue/epilogue */
  /* set block length to 34 bytes on 'channel 8' */
  SendWord (cmd1);
  DBG (16, "SendWord(cmd1) passed (%s:%d) \n", __FILE__, __LINE__);

  /* SendData */
  SendData (cmd2, 0x22);
  DBG (16, "SendData(cmd2) passed (%s:%d) \n", __FILE__, __LINE__);

  if (DBG_LEVEL >= 128)
    {
      Bloc8Decode (cmd2);
    }

  /* set block length to 2048, write on 'channel 4' */
  SendWord (cmd3);
  DBG (16, "SendWord(cmd3) passed (%s:%d) \n", __FILE__, __LINE__);

  if (SendData (data, 2048) == 0)
    {
      DBG (0, "SendData(data,%d) failed (%s:%d)\n", 2048, __FILE__, __LINE__);
      return (0);
    }
  TRACE (16, "SendData(data,2048) passed ...");

  /* read back all data sent to 'channel 4' */
  SendWord (cmd4);
  DBG (16, "SendWord(cmd4) passed (%s:%d) \n", __FILE__, __LINE__);

  if (PausedReadData (2048, dest) == 0)
    {
      DBG (16, "PausedReadData(2048,dest) failed (%s:%d)\n", __FILE__,
	   __LINE__);
      return (0);
    }
  DBG (16, "PausedReadData(2048,dest) passed (%s:%d)\n", __FILE__, __LINE__);

  /* dest should hold the same datas than donnees */
  for (i = 0; i < 2047; i++)
    {
      if (data[i] != (int) (dest[i]))
	{
	  DBG
	    (0,
	     "Warning data read back differs: expected %02X found dest[%d]=%02X ! (%s:%d)\n",
	     data[i], i, dest[i], __FILE__, __LINE__);
	}
    }
  return (1);
}




/* free scanner and parallel port
 0: failure
 1: success 
*/
int
sanei_umax_pp_ReleaseScanner (void)
{
  int reg;

  EPPREGISTERWRITE (0x0A, 0x00);
  reg = EPPRegisterRead (0x0D);
  reg = (reg & 0xBF);
  EPPRegisterWrite (0x0D, reg);
  if (GetModel () != 0x07)
    {
      if (SendCommand (0x40) == 0)
	{
	  DBG (0, "SendCommand(0x40) (%s:%d) failed ...\n", __FILE__,
	       __LINE__);
	  return (0);
	}
    }
  if (SendCommand (0x30) == 0)
    {
      DBG (0, "SendCommand(0x30) (%s:%d) failed ...\n", __FILE__, __LINE__);
      return (0);
    }
  DBG (1, "ReleaseScanner() done ...\n");

  return (1);
}



/* 1: OK
   0: end session failed */

int
sanei_umax_pp_EndSession (void)
{
  int tmp;
  int control;
  int data;
  int reg;
  int zero[5] = { 0, 0, 0, 0, -1 };


  data = Inb (DATA);
  control = Inb (CONTROL) & 0x1F;
  Outb (CONTROL, control);
  control = Inb (CONTROL) & 0x1F;
  Outb (CONTROL, control);

  g67D = 1;

  if (SendCommand (0xE0) == 0)
    {
      DBG (0, "SendCommand(0xE0) (%s:%d) failed ...\n", __FILE__, __LINE__);
      return (0);
    }
  DBG (16, "SendCommand(0xE0) passed... (%s:%d)\n", __FILE__, __LINE__);

  g6FE = 1;
  g674 = 0;
  ClearRegister (0);
  Init001 ();
  DBG (16, "Init001() passed... (%s:%d)\n", __FILE__, __LINE__);

  EPPREGISTERREAD (0x0B, 0xC7);
  reg = EPPRegisterRead (0x0D);
  reg = (reg | 0x43);
  EPPRegisterWrite (0x0D, reg);
  EPPREGISTERWRITE (0x0C, 0x04);
  reg = EPPRegisterRead (0x0A);
  if (reg != 0x00)
    {
      if (reg != 0x1C)
	DBG (0, "Expected 0x00 found 0x%02X .... (%s:%d)\n", reg, __FILE__,
	     __LINE__);
      else
	{
	  DBG (16, "Previous probe detected .... (%s:%d)\n", __FILE__,
	       __LINE__);
	}
    }
  EPPREGISTERWRITE (0x0A, 0x1C);
  EPPREGISTERWRITE (0x08, 0x21);
  EPPREGISTERWRITE (0x0E, 0x0F);
  EPPREGISTERWRITE (0x0F, 0x0C);
  EPPREGISTERWRITE (0x0A, 0x1C);
  EPPREGISTERWRITE (0x0E, 0x10);
  EPPREGISTERWRITE (0x0F, 0x1C);

  if (SendWord (zero) == 0)
    {
      DBG (16, "SendWord(zero) failed (%s:%d)\n", __FILE__, __LINE__);
    }
  Epilogue ();

  sanei_umax_pp_CmdSync (0xC2);
  sanei_umax_pp_CmdSync (0x00);	/* cancels any pending operation */
  sanei_umax_pp_CmdSync (0x00);	/* cancels any pending operation */
  sanei_umax_pp_ReleaseScanner ();

  /* restore port state */
  Outb (DATA, 0x04);
  Outb (CONTROL, gControl);

  /* OUF */
  DBG (1, "End session done ...\n");
  return (1);
}


/* 1: OK
   2: homing happened
   3: scanner busy
   0: init failed 

   init transport layer
   init scanner
*/

int
sanei_umax_pp_InitScanner (int recover)
{
  int i, j;
  int status;
  int readcmd[64];
  int sentcmd[64];





  /* in umax1220u, this buffer is opc[16] */
  j = 0;
  sentcmd[j] = 0x02;
  j++;
  sentcmd[j] = 0x80;
  j++;
  sentcmd[j] = 0x00;
  j++;
  sentcmd[j] = 0x70;
  j++;
  sentcmd[j] = 0x00;
  j++;
  sentcmd[j] = 0x00;
  j++;
  sentcmd[j] = 0x00;
  j++;
  sentcmd[j] = 0x2F;
  j++;
  sentcmd[j] = 0x2F;
  j++;
  sentcmd[j] = 0x07;
  j++;
  sentcmd[j] = 0x00;
  j++;
  sentcmd[j] = 0x00;
  j++;
  sentcmd[j] = 0x00;
  j++;
  sentcmd[j] = 0x80;
  j++;
  sentcmd[j] = 0xF0;
  j++;				/* F0 means light on, 90 light off */
  if (GetModel () == 0x07)
    {
      sentcmd[j] = 0x00;
      j++;
    }
  else
    {
      sentcmd[j] = 0x18;
      j++;
    }
  sentcmd[j] = -1;
  /* fails here if there is an unfinished previous scan */
  if (CmdSetGet (0x02, j, sentcmd) != 1)
    {
      DBG (0, "CmdSetGet(0x02,j,sentcmd) failed (%s:%d)\n", __FILE__,
	   __LINE__);
      return (0);
    }
  DBG (16, "CmdSetGet(0x02,j,sentcmd) passed ... (%s:%d)\n", __FILE__,
       __LINE__);

  /* needs some init */
  if (sentcmd[15] == 0x18)
    {
      sentcmd[15] = 0x00;	/* was 0x18 */
      if (CmdSetGet (0x02, j, sentcmd) != 1)
	{
	  DBG (0, "CmdSetGet(0x02,j,sentcmd) failed (%s:%d)\n", __FILE__,
	       __LINE__);
	  return (0);
	}
      DBG (16, "CmdSetGet(0x02,j,sentcmd) passed ... (%s:%d)\n", __FILE__,
	   __LINE__);

      /* in umax1220u, this buffer does not exist */
      j = 0;
      sentcmd[j] = 0xA0;
      j++;
      sentcmd[j] = 0xA1;
      j++;
      sentcmd[j] = 0xA2;
      j++;
      sentcmd[j] = 0xA3;
      j++;
      sentcmd[j] = 0xA4;
      j++;
      sentcmd[j] = 0xA5;
      j++;
      sentcmd[j] = 0xA6;
      j++;
      sentcmd[j] = 0xA7;
      j++;
      sentcmd[j] = -1;
      if (CmdSetGet (0x01, j, sentcmd) != 1)
	{
	  DBG (0, "CmdSetGet(0x02,j,sentcmd) failed (%s:%d)\n", __FILE__,
	       __LINE__);
	  return (0);
	}
      DBG (16, "CmdSetGet(0x01,j,sentcmd) passed ... (%s:%d)\n", __FILE__,
	   __LINE__);
    }


  /* ~ opb3: inquire status */
  if (CmdGet (0x08, 36, readcmd) == 0)
    {
      DBG (0, "CmdGet(0x08,36,readcmd) failed (%s:%d)\n", __FILE__, __LINE__);
      return (0);
    }
  if (DBG_LEVEL >= 32)
    {
      Bloc8Decode (readcmd);
    }
  DBG (16, "CmdGet(0x08,36,readcmd) passed (%s:%d)\n", __FILE__, __LINE__);

  /* is the scanner busy parking ? */
  status = sanei_umax_pp_ScannerStatus ();
  DBG (8, "INQUIRE SCANNER STATUS IS 0x%02X  (%s:%d)\n", status, __FILE__,
       __LINE__);
  if ((!recover) && (status & MOTOR_BIT) == 0x00)
    {
      DBG (1, "Warning: scanner motor on, giving up ...  (%s:%d)\n", __FILE__,
	   __LINE__);
      return (3);
    }

  /* head homing needed ? */
  if ((readcmd[34] != 0x1A) || (recover == 1))
    {				/* homing needed, readcmd[34] should be 0x48 */
      int op01[17] =
	{ 0x01, 0x00, 0x32, 0x70, 0x00, 0x00, 0x60, 0x2F, 0x17, 0x05, 0x00,
	0x00, 0x00, 0x80, 0xE4, 0x00, -1
      };
      int op05[17] =
	{ 0x01, 0x00, 0x01, 0x70, 0x00, 0x00, 0x60, 0x2F, 0x13, 0x05, 0x00,
	0x00, 0x00, 0x80, 0xF0, 0x00, -1
      };
      int op02[37] =
	{ 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C, 0x00, 0x04, 0x40,
	0x01, 0x00, 0x20, 0x02, 0x00, 0x16, 0x00, 0x70, 0x9F, 0x06, 0x00,
	0x00, 0xF6, 0x4D, 0xA0, 0x00, 0x8B, 0x49, 0x2A, 0xE9, 0x68, 0xDF,
	0x0B, 0x1A, 0x00, -1
      };
      int op04[37] =
	{ 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C, 0x00, 0x03, 0xC1,
	0x80, 0x00, 0x20, 0x02, 0x00, 0x16, 0x80, 0x15, 0x78, 0x03, 0x03,
	0x00, 0x00, 0x46, 0xA0, 0x00, 0x8B, 0x49, 0x2A, 0xE9, 0x68, 0xDF,
	0x0B, 0x1A, 0x00, -1
      };
      int op03[9] = { 0x00, 0x00, 0x00, 0xAA, 0xCC, 0xEE, 0xFF, 0xFF, -1 };

      CMDSYNC (0xC2);
      CMDSETGET (0x02, 16, op01);
      CMDSETGET (0x08, 36, op02);

      if (!recover)
	{
	  CMDSYNC (0xC2);
	  CMDSYNC (0x00);
	  CMDSETGET (0x04, 8, op03);
	  CMDSYNC (0x40);
	  do
	    {
	      sleep (1);
	      CMDSYNC (0xC2);
	    }
	  while ((sanei_umax_pp_ScannerStatus () & 0x90) != 0x90);

	  op01[2] = 0x1E;
	  op01[9] = 0x01;
	  CMDSETGET (0x02, 16, op01);
	  CMDSETGET (0x08, 36, op02);
	  CMDSYNC (0x00);
	  CMDSYNC (0x00);
	  CMDSETGET (0x04, 8, op03);

	  CMDSYNC (0x40);
	  do
	    {
	      sleep (1);
	      CMDSYNC (0xC2);
	    }
	  while ((sanei_umax_pp_ScannerStatus () & 0x90) != 0x90);
	  CMDSYNC (0x00);
	}

      for (i = 0; i < 4; i++)
	{

	  do
	    {
	      usleep (500000);
	      CMDSYNC (0xC2);
	      status = sanei_umax_pp_ScannerStatus ();
	      status = status & 0x10;
	    }
	  while (status != 0x10);	/* was 0x90 */
	  CMDSETGET (0x02, 16, op05);
	  CMDSETGET (0x08, 36, op04);
	  CMDSYNC (0x40);
	  status = sanei_umax_pp_ScannerStatus ();
	  DBG (16, "loop %d passed, status=0x%02X (%s:%d)\n", i, status,
	       __FILE__, __LINE__);
	}



      /* get head back home ... */
      do
	{
	  i++;
	  do
	    {
	      usleep (500000);
	      CMDSYNC (0xC2);
	      status = sanei_umax_pp_ScannerStatus ();
	      status = status & 0x10;
	    }
	  while (status != 0x10);	/* was 0x90 */
	  CMDSETGET (0x02, 16, op05);
	  CMDSETGET (0x08, 36, op04);
	  CMDSYNC (0x40);
	  status = sanei_umax_pp_ScannerStatus ();
	  DBG (16, "loop %d passed, status=0x%02X (%s:%d)\n", i, status,
	       __FILE__, __LINE__);
	}
      while ((status & MOTOR_BIT) == 0x00);	/* 0xD0 when head is back home */

      do
	{
	  usleep (500000);
	  CMDSYNC (0xC2);
	}
      while ((sanei_umax_pp_ScannerStatus () & 0x90) != 0x90);


      /* don't do automatic home sequence on recovery */
      if (!recover)
	{
	  CMDSYNC (0x00);
	  op01[2] = 0x1A;
	  op01[3] = 0x74;	/* was 0x70 */
	  op01[9] = 0x05;	/* initial value */
	  op01[14] = 0xF4;	/* was 0xE4 */
	  CMDSETGET (0x02, 16, op01);
	  CMDSETGET (0x08, 36, op02);
	  CMDSYNC (0xC2);
	  CMDSYNC (0x00);
	  CMDSETGET (0x04, 8, op03);
	  CMDSYNC (0x40);

	  /* wait for automatic homing sequence */
	  /* to complete, thus avoiding         */
	  /* scanning too early                 */
	  do
	    {
	      /* the sleep is here to prevent */
	      /* excessive CPU usage, can be  */
	      /* removed, if we don't care    */
	      sleep (3);
	      CMDSYNC (0xC2);
	      DBG (16, "PARKING polling status is 0x%02X   (%s:%d)\n",
		   sanei_umax_pp_ScannerStatus (), __FILE__, __LINE__);
	    }
	  while (sanei_umax_pp_ScannerStatus () == 0x90);
	}

      /* signal homing */
      return (2);
    }


  /* end ... */
  DBG (1, "Scanner init done ...\n");
  return (1);
}


/* 
	1: OK
   	2: failed, try again
   	0: init failed 

	initialize the transport layer
   
   */

static int
InitTransport610P (int recover)
{
  int control;
  int data;
  int zero[5] = { 0, 0, 0, 0, -1 };


  /* recover is not used yet, but will */
  recover = 0;			/* quit compiler quiet .. */

  /* set port state */
  data = Inb (DATA);
  control = Inb (CONTROL) & 0x0C;
  Outb (CONTROL, control);
  control = Inb (CONTROL) & 0x0C;
  Outb (CONTROL, control);
  gControl = 0x0C;

  g67D = 1;
  if (SendCommand (0xE0) == 0)
    {
      DBG (0, "SendCommand(0xE0) (%s:%d) failed ...\n", __FILE__, __LINE__);
      return (0);
    }
  DBG (16, "SendCommand(0xE0) passed...\n");
  g6FE = 1;
  ClearRegister (0);
  DBG (16, "ClearRegister(0) passed...\n");

  /* sync */
  Prologue ();
  if (SendWord (zero) == 0)
    {
      DBG (0, "SendWord(zero) failed (%s:%d)\n", __FILE__, __LINE__);
      return (0);
    }
  DBG (16, "SendWord(zero) passed (%s:%d)\n", __FILE__, __LINE__);

  Epilogue ();

  /* OK ! */
  DBG (1, "InitTransport610P done ...\n");
  return (1);
}

/* 
	1: OK
   	2: failed, try again
   	0: init failed 

	initialize the transport layer
   
   */

static int
InitTransport1220P (int recover)
{
  int i, j;
  int control;
  int data;
  int reg;
  unsigned char *dest = NULL;
  int zero[5] = { 0, 0, 0, 0, -1 };
  int model;

  /* init cancel handling */
  /* InitCancel(); */

  /* recover is not used yet, but will */
  recover = 0;			/* quit compiler quiet .. */

  /* set port state */
  data = Inb (DATA);
  control = Inb (CONTROL) & 0x0C;
  Outb (CONTROL, control);
  control = Inb (CONTROL) & 0x0C;
  Outb (CONTROL, control);
  gControl = 0x0C;

  g67D = 1;
  if (SendCommand (0xE0) == 0)
    {
      DBG (0, "SendCommand(0xE0) (%s:%d) failed ...\n", __FILE__, __LINE__);
      return (0);
    }
  DBG (16, "SendCommand(0xE0) passed...\n");
  g6FE = 1;
  ClearRegister (0);
  DBG (16, "ClearRegister(0) passed...\n");
  Init001 ();
  DBG (16, "Init001() passed... (%s:%d)\n", __FILE__, __LINE__);
  gEPAT = 0xC7;
  reg = EPPRegisterRead (0x0B);
  if (reg != gEPAT)
    {
      DBG (16, "Error! expected reg0B=0x%02X, found 0x%02X! (%s:%d) \n",
	   gEPAT, reg, __FILE__, __LINE__);
      DBG (16, "Scanner needs probing ... \n");
      sanei_umax_pp_ReleaseScanner ();
      if (sanei_umax_pp_ProbeScanner (recover) != 1)
	{
	  return (0);
	}
      else
	{
	  sanei_umax_pp_ReleaseScanner ();
	  return (2);		/* signals retry InitTransport() */
	}
    }

  reg = EPPRegisterRead (0x0D);
  reg = (reg & 0xE8) | 0x43;
  EPPRegisterWrite (0x0D, reg);
  EPPREGISTERWRITE (0x0C, 0x04);
  reg = EPPRegisterRead (0x0A);
  if (reg != 0x00)
    {
      if (reg != 0x1C)
	{
	  DBG (0, "Warning! expected reg0A=0x00, found 0x%02X! (%s:%d) \n",
	       reg, __FILE__, __LINE__);
	}
      else
	{
	  DBG (16, "Scanner in idle state .... (%s:%d)\n", __FILE__,
	       __LINE__);
	}
    }

  /* model detection: redone since we might not be probing each time ... */
  /* write addr in 0x0E, read value at 0x0F                              */
  EPPREGISTERWRITE (0x0E, 0x01);
  model = EPPRegisterRead (0x0F);
  SetModel (model);

  EPPREGISTERWRITE (0x0A, 0x1C);
  EPPREGISTERWRITE (0x08, 0x21);
  EPPREGISTERWRITE (0x0E, 0x0F);
  EPPREGISTERWRITE (0x0F, 0x0C);

  EPPREGISTERWRITE (0x0A, 0x1C);
  EPPREGISTERWRITE (0x0E, 0x10);
  EPPREGISTERWRITE (0x0F, 0x1C);
  EPPREGISTERWRITE (0x0A, 0x11);


  dest = (unsigned char *) (malloc (65536));
  if (dest == NULL)
    {
      DBG (0, "Failed to allocate 64 Ko !\n");
      return (0);
    }
  for (i = 0; i < 256; i++)
    {
      dest[i * 2] = i;
      dest[i * 2 + 1] = 0xFF - i;
      dest[512 + i * 2] = i;
      dest[512 + i * 2 + 1] = 0xFF - i;
    }
  for (i = 0; i < 150; i++)
    {
      if (GetEPPMode () == 32)
	{
	  EPPWrite32Buffer (0x400, dest);
	  DBG (16,
	       "Loop %d: EPPWrite32Buffer(0x400,dest) passed... (%s:%d)\n", i,
	       __FILE__, __LINE__);
	}
      else
	{
	  EPPWriteBuffer (0x400, dest);
	  DBG (16, "Loop %d: EPPWriteBuffer(0x400,dest) passed... (%s:%d)\n",
	       i, __FILE__, __LINE__);
	}
    }
  EPPREGISTERWRITE (0x0A, 0x18);
  EPPREGISTERWRITE (0x0A, 0x11);
  for (i = 0; i < 150; i++)
    {
      if (GetEPPMode () == 32)
	{
	  EPPRead32Buffer (0x400, dest);
	}
      else
	{
	  EPPReadBuffer (0x400, dest);
	}
      for (j = 0; j < 256; j++)
	{
	  if (dest[j * 2] != j)
	    {
	      DBG (0,
		   "Altered buffer value at %03X, expected %02X, found %02X\n",
		   j * 2, j, dest[j * 2]);
	      return (0);
	    }
	  if (dest[j * 2 + 1] != 0xFF - j)
	    {
	      DBG
		(0,
		 "Altered buffer value at %03X, expected %02X, found %02X\n",
		 j * 2 + 1, 0xFF - j, dest[j * 2 + 1]);
	      return (0);
	    }
	  if (dest[512 + j * 2] != j)
	    {
	      DBG (0,
		   "Altered buffer value at %03X, expected %02X, found %02X\n",
		   512 + j * 2, j, dest[512 + j * 2]);
	      return (0);
	    }
	  if (dest[512 + j * 2 + 1] != 0xFF - j)
	    {
	      DBG
		(0,
		 "Altered buffer value at %03X, expected 0x%02X, found 0x%02X\n",
		 512 + j * 2 + 1, 0xFF - j, dest[512 + j * 2 + 1]);
	      return (0);
	    }
	}
      if (GetEPPMode () == 32)
	DBG (16, "Loop %d: EPPRead32Buffer(0x400,dest) passed... (%s:%d)\n",
	     i, __FILE__, __LINE__);
      else
	DBG (16, "Loop %d: EPPReadBuffer(0x400,dest) passed... (%s:%d)\n", i,
	     __FILE__, __LINE__);
    }
  EPPREGISTERWRITE (0x0A, 0x18);
  if (Fonc001 () != 1)
    {
      DBG (0, "Fonc001() failed ! (%s:%d) \n", __FILE__, __LINE__);
      return (0);
    }
  DBG (16, "Fonc001() passed ...  (%s:%d) \n", __FILE__, __LINE__);

  /* sync */
  if (SendWord (zero) == 0)
    {
      DBG (0, "SendWord(zero) failed (%s:%d)\n", __FILE__, __LINE__);
      return (0);
    }
  DBG (16, "SendWord(zero) passed (%s:%d)\n", __FILE__, __LINE__);
  Epilogue ();

  /* OK ! */
  free (dest);
  DBG (1, "InitTransport1220P done ...\n");
  return (1);
}

/* 
	1: OK
   	2: failed, try again
   	0: init failed 

	initialize the transport layer
   
   */

int
sanei_umax_pp_InitTransport (int recover)
{
  switch (sanei_umax_pp_getastra ())
    {
    case 610:
      return (InitTransport610P (recover));
    case 1220:
    case 1600:
    case 2000:
      return (InitTransport1220P (recover));
    }
  return (0);
}


/*
 * returns 1 if testing OK
 * 0 if it fails
 * send value if value != 0
 */
static int
Test610P (int value)
{
  int data, control, val;

  /* save state */
  data = Inb (DATA);
  control = Inb (CONTROL) & 0x1F;
  Outb (CONTROL, control);

  Outb (DATA, 0x22);
  usleep (10000);
  Outb (DATA, 0x22);
  usleep (10000);
  Outb (DATA, 0x22);
  usleep (10000);
  Outb (DATA, 0x22);
  usleep (10000);
  Outb (DATA, 0x22);
  usleep (10000);
  Outb (DATA, 0x22);
  usleep (10000);
  Outb (DATA, 0x22);
  usleep (10000);
  Outb (DATA, 0x22);
  usleep (10000);
  Outb (DATA, 0xAA);
  usleep (10000);
  Outb (DATA, 0xAA);
  usleep (10000);
  Outb (DATA, 0xAA);
  usleep (10000);
  Outb (DATA, 0xAA);
  usleep (10000);
  Outb (DATA, 0xAA);
  usleep (10000);
  Outb (DATA, 0xAA);
  usleep (10000);
  Outb (DATA, 0xAA);
  usleep (10000);
  Outb (DATA, 0xAA);
  usleep (10000);
  Outb (DATA, 0x55);
  usleep (10000);
  Outb (DATA, 0x55);
  usleep (10000);
  Outb (DATA, 0x55);
  usleep (10000);
  Outb (DATA, 0x55);
  usleep (10000);
  Outb (DATA, 0x55);
  usleep (10000);
  Outb (DATA, 0x55);
  usleep (10000);
  Outb (DATA, 0x55);
  usleep (10000);
  Outb (DATA, 0x55);
  usleep (10000);
  Outb (DATA, 0x00);
  usleep (10000);
  Outb (DATA, 0x00);
  usleep (10000);
  Outb (DATA, 0x00);
  usleep (10000);
  Outb (DATA, 0x00);
  usleep (10000);
  Outb (DATA, 0x00);
  usleep (10000);
  Outb (DATA, 0x00);
  usleep (10000);
  Outb (DATA, 0x00);
  usleep (10000);
  Outb (DATA, 0x00);
  usleep (10000);
  Outb (DATA, 0xFF);
  usleep (10000);
  Outb (DATA, 0xFF);
  usleep (10000);
  Outb (DATA, 0xFF);
  usleep (10000);
  Outb (DATA, 0xFF);
  usleep (10000);
  Outb (DATA, 0xFF);
  usleep (10000);
  Outb (DATA, 0xFF);
  usleep (10000);
  Outb (DATA, 0xFF);
  usleep (10000);
  Outb (DATA, 0xFF);
  usleep (10000);
  if (value)
    {
      Outb (DATA, value);
      usleep (10000);
      Outb (DATA, value);
      usleep (10000);
      Outb (DATA, value);
      usleep (10000);
      Outb (DATA, value);
      usleep (10000);
      Outb (DATA, value);
      usleep (10000);
      Outb (DATA, value);
      usleep (10000);
      Outb (DATA, value);
      usleep (10000);
      Outb (DATA, value);
      usleep (10000);
    }
  val = Inb (STATUS);
  usleep (10000);
  Outb (DATA, data);
  Outb (CONTROL, control);
  return (1);
}



/*
 * returns 1 if testing OK
 * 0 if it fails
 */
static int
In256 (void)
{
  int val, i, tmp;

  Outb (CONTROL, 0x04);
  usleep (10000);
  Outb (CONTROL, 0x0C);
  usleep (10000);
  val = Inb (STATUS);
  Outb (CONTROL, 0x0E);
  usleep (10000);
  Outb (CONTROL, 0x0E);
  usleep (10000);
  Outb (CONTROL, 0x0E);
  usleep (10000);
  tmp = val;
  i = 0;
  while ((tmp == val) && (i < 256))
    {
      tmp = Inb (STATUS);
      usleep (10000);
      i++;
    }
  if (tmp != val)
    {
      DBG (1, "Error, expected status 0x%02X, got 0x%02X (%s:%d)\n", val, tmp,
	   __FILE__, __LINE__);
      return (0);
    }
  Outb (CONTROL, 0x04);
  usleep (10000);
  Outb (CONTROL, 0x04);
  usleep (10000);
  return (1);
}

/* 1: OK
   0: probe failed */

static int
Probe610P (int recover)
{
  int tmp, i;
  /*int zero[5] = { 0, 0, 0, 0, -1 }; to be used */

  /* recover is not used yet, but will */
  recover = 0;			/* quit compiler quiet .. */

  /* make sure we won't try 1220/200P later */
  sanei_umax_pp_setastra (610);
  if (!Test610P (0x87))
    {
      DBG (1, "Ring610P(0x87) failed (%s:%d)\n", __FILE__, __LINE__);
      return (0);
    }
  else
    {
      DBG (16, "Ring610P(0x87) passed...\n");
    }
  if (!In256 ())
    {
      DBG (1, "In256() failed (%s:%d)\n", __FILE__, __LINE__);
      return (0);
    }
  else
    {
      DBG (16, "In256() passed...\n");
    }


  /* test PS2 mode */

  tmp = SlowNibbleRegisterRead (0x0B);
  if (tmp != 0x88)
    {				/* tmp = 0x88 for 610P */
      DBG (1, "Found 0x%X expected 0x88  (%s:%d)\n", tmp, __FILE__, __LINE__);
    }

  /* clear register 3 */
  ClearRegister (3);
  DBG (16, "ClearRegister(3) passed...\n");

  /* wait ? */
  i = 65535;
  while (i > 0)
    {
      tmp = Inb (DATA);
      tmp = Inb (DATA);
      i--;
    }
  DBG (16, "FFFF in loop passed...\n");

  ClearRegister (0);
  DBG (16, "ClearRegister(0) passed... (%s:%d)\n", __FILE__, __LINE__);
  fflush (stdout);

  /* no EPP32 for 610P */
  SetEPPMode (8);

  /* XXX STEF XXX : insert init trans + re homing */

  /* successfull end ... */
  DBG (1, "UMAX Astra 610P detected\n");
  DBG (1, "Probe610P done ...\n");
  return (1);
}


/* 1: OK
   0: probe failed */

int
sanei_umax_pp_ProbeScanner (int recover)
{
  int tmp, i, j;
  int reg;
  unsigned char *dest = NULL;
  int initbuf[2049];
  int voidbuf[2049];
  int val;
  int zero[5] = { 0, 0, 0, 0, -1 };
  int model;

  /* save and set CONTROL */
  tmp = (Inb (CONTROL)) & 0x1F;
  tmp = (Inb (CONTROL)) & 0x1F;
  Outb (CONTROL, tmp);
  Outb (CONTROL, tmp);

  tmp = Inb (DATA);
  tmp = Inb (CONTROL) & 0x3F;
  Outb (CONTROL, tmp);

  tmp = Inb (CONTROL) & 0x3F;
  tmp = Inb (DATA);
  tmp = Inb (CONTROL) & 0x3F;
  tmp = Inb (DATA);

  /*  any scanner ? */
  tmp = RingScanner ();
  if (tmp)
    {
      i = 0;
      while ((i < 50) && (tmp))
	{
	  tmp = RingScanner ();
	  i++;
	}
    }
  if (!tmp)
    {
      DBG (1, "No 1220P/2000P scanner detected by 'RingScanner()'...\n");
      DBG (1, "Trying 610P...\n");
      return (Probe610P (recover));
    }
  DBG (16, "RingScanner passed...\n");

  gControl = Inb (CONTROL) & 0x3F;
  g67D = 1;
  if (SendCommand (0x30) == 0)
    {
      DBG (0, "SendCommand(0x30) (%s:%d) failed ...\n", __FILE__, __LINE__);
      return (0);
    }
  DBG (16, "SendCommand(0x30) passed ... (%s:%d)\n", __FILE__, __LINE__);
  g67E = 4;			/* bytes to read */
  if (SendCommand (0x00) == 0)
    {
      DBG (0, "SendCommand(0x00) (%s:%d) failed ...\n", __FILE__, __LINE__);
      return (0);
    }
  DBG (16, "SendCommand(0x00) passed... (%s:%d)\n", __FILE__, __LINE__);
  g67E = 0;			/* bytes to read */
  if (TestVersion (0) == 0)
    {
      DBG (0, "TestVersion(0) (%s:%d) failed ...\n", __FILE__, __LINE__);
      return (0);
    }
  DBG (16, "TestVersion(0) passed...\n");
  /* must fail for 1220P and 2000P */
  if (TestVersion (1) == 0)	/* software doesn't do it for model 0x07 */
    {				/* but it works ..                       */
      DBG (16, "TestVersion(1) failed (expected) ... (%s:%d)\n", __FILE__,
	   __LINE__);
    }
  else
    {
      DBG (0, "Unexpected success on TestVersion(1) ... (%s:%d)\n", __FILE__,
	   __LINE__);
    }
  if (TestVersion (0) == 0)
    {
      DBG (0, "TestVersion(0) (%s:%d) failed ...\n", __FILE__, __LINE__);
      return (0);
    }
  DBG (16, "TestVersion(0) passed...\n");
  /* must fail */
  if (TestVersion (1) == 0)
    {
      DBG (16, "TestVersion(1) failed (expected) ... (%s:%d)\n", __FILE__,
	   __LINE__);
    }
  else
    {
      DBG (0, "Unexpected success on TestVersion(1) ... (%s:%d)\n", __FILE__,
	   __LINE__);
    }

  Outb (DATA, 0x04);
  Outb (CONTROL, 0x0C);
  Outb (DATA, 0x04);
  Outb (CONTROL, 0x0C);

  gControl = Inb (CONTROL) & 0x3F;
  Outb (CONTROL, gControl & 0xEF);



  if (SendCommand (0x40) == 0)
    {
      DBG (0, "SendCommand(0x40) (%s:%d) failed ...\n", __FILE__, __LINE__);
      return (0);
    }
  DBG (16, "SendCommand(0x40) passed...\n");
  if (SendCommand (0xE0) == 0)
    {
      DBG (0, "SendCommand(0xE0) (%s:%d) failed ...\n", __FILE__, __LINE__);
      return (0);
    }
  DBG (16, "SendCommand(0xE0) passed...\n");

  ClearRegister (0);
  DBG (16, "ClearRegister(0) passed...\n");

  SPPResetLPT ();
  DBG (16, "SPPResetLPT() passed...\n");

  Outb (CONTROL, 4);
  Outb (CONTROL, 4);


  /* test PS2 mode */

  tmp = SlowNibbleRegisterRead (0x0B);
  if (tmp == 0xC7)
    {
      /* epat C7 detected */
      DBG (16, "SlowNibbleRegisterRead(0x0B)=0x%X passed...\n", tmp);

      WriteSlow (8, 0);
      DBG (16, "WriteSlow(8,0) passed...   (%s:%d)\n", __FILE__, __LINE__);

      tmp = SlowNibbleRegisterRead (0x0A);
      if (tmp != 0x00)
	{
	  if (tmp == 0x1C)
	    {
	      DBG (16, "Previous probe detected ... (%s:%d)\n", __FILE__,
		   __LINE__);
	    }
	  else
	    {
	      DBG (0, "Found 0x%X expected 0x00  (%s:%d)\n", tmp, __FILE__,
		   __LINE__);
	    }
	}
      DBG (16, "SlowNibbleRegisterRead(0x0A)=0x%X passed ...(%s:%d)\n", tmp,
	   __FILE__, __LINE__);

    }
  else
    {
      DBG (0, "Found 0x%X expected 0xC7 or 0x88  (%s:%d)\n", tmp, __FILE__,
	   __LINE__);
      return 0;
    }

  /* clear register 3 */
  ClearRegister (3);
  DBG (16, "ClearRegister(3) passed...\n");

  /* wait ? */
  i = 65535;
  while (i > 0)
    {
      tmp = Inb (DATA);
      tmp = Inb (DATA);
      i--;
    }
  DBG (16, "FFFF in loop passed...\n");

  ClearRegister (0);
  DBG (16, "ClearRegister(0) passed... (%s:%d)\n", __FILE__, __LINE__);
  fflush (stdout);

  /* 1220/2000P branch */
  WRITESLOW (0x0E, 1);

  /* register 0x0F used only once: model number ? Or ASIC revision ? */
  /* comm mode ?                                                     */
  model = SlowNibbleRegisterRead (0x0F);
  DBG (1, "UMAX Astra 1220/1600/2000 P ASIC detected (mode=%d)\n", model);
  SetModel (model);
  DBG (16, "SlowNibbleRegisterRead(0x0F) passed... (%s:%d)\n", __FILE__,
       __LINE__);

  /* scanner powered off */
  if (model == 0x1B)
    {
      DBG (0, "Register 0x0F=0x1B, scanner powered off ! (%s:%d)\n", __FILE__,
	   __LINE__);
      return 0;
    }


  if ((model != 0x1F) && (model != 0x07))
    {
      DBG
	(0,
	 "Unexpected value for for register 0x0F, expected 0x07 or 0x1F, got 0x%02X ! (%s:%d)\n",
	 model, __FILE__, __LINE__);
      DBG (0, "There is a new scanner revision in town, or a bug ....\n");
    }

  WRITESLOW (0x08, 0x02);
  WRITESLOW (0x0E, 0x0F);
  WRITESLOW (0x0F, 0x0C);
  WRITESLOW (0x0C, 0x04);
  tmp = SlowNibbleRegisterRead (0x0D);
  if ((tmp != 0x00) && (tmp != 0x40))
    {
      DBG
	(0,
	 "Unexpected value for for register 0x0D, expected 0x00 or 0x40, got 0x%02X ! (%s:%d)\n",
	 tmp, __FILE__, __LINE__);
    }
  WRITESLOW (0x0D, 0x1B);
  switch (model)
    {
    case 0x1F:
      WRITESLOW (0x12, 0x14);
      SLOWNIBBLEREGISTEREAD (0x12, 0x10);
      break;
    case 0x07:
      WRITESLOW (0x12, 0x00);
      SLOWNIBBLEREGISTEREAD (0x12, 0x00);
      /* we may get 0x20, in this case some color aberration may occur */
      /* must depend on the parport */
      /* model 0x07 + 0x00=>0x20=2000P */
      break;
    default:
      WRITESLOW (0x12, 0x00);
      SLOWNIBBLEREGISTEREAD (0x12, 0x20);
      break;
    }
  SLOWNIBBLEREGISTEREAD (0x0D, 0x18);
  SLOWNIBBLEREGISTEREAD (0x0C, 0x04);
  SLOWNIBBLEREGISTEREAD (0x0A, 0x00);
  WRITESLOW (0x0E, 0x0A);
  WRITESLOW (0x0F, 0x00);
  WRITESLOW (0x0E, 0x0D);
  WRITESLOW (0x0F, 0x00);

  /* write/read full buffer */
  for (i = 0; i < 256; i++)
    {
      WRITESLOW (0x0A, i);
      SLOWNIBBLEREGISTEREAD (0x0A, i);
      WRITESLOW (0x0A, 0xFF - i);
      SLOWNIBBLEREGISTEREAD (0x0A, 0xFF - i);
    }

  /* end test for nibble byte/byte mode */

  /* now we try nibble buffered mode */
  WRITESLOW (0x13, 0x01);
  WRITESLOW (0x13, 0x00);	/*reset something */
  WRITESLOW (0x0A, 0x11);

  /* read buffer */
  dest = (unsigned char *) (malloc (65536));
  for (i = 0; i < 10; i++)	/* 10 ~ 11 ? */
    {
      NibbleReadBuffer (0x400, dest);
      DBG (16, "Loop %d: NibbleReadBuffer passed ... (%s:%d)\n", i, __FILE__,
	   __LINE__);
    }

  /* write buffer */
  for (i = 0; i < 10; i++)
    {
      WriteBuffer (0x400, dest);
      DBG (16, "Loop %d: WriteBuffer passed ... (%s:%d)\n", i, __FILE__,
	   __LINE__);
    }

  SLOWNIBBLEREGISTEREAD (0x0C, 0x04);
  WRITESLOW (0x13, 0x01);
  WRITESLOW (0x13, 0x00);
  WRITESLOW (0x0A, 0x18);
  Outb (CONTROL, 4);
  SLOWNIBBLEREGISTEREAD (0x0A, 0x18);
  WRITESLOW (0x08, 0x40);
  WRITESLOW (0x08, 0x60);
  WRITESLOW (0x08, 0x22);


  /* test EPP MODE */
  SetEPPMode (8);
  ClearRegister (0);
  DBG (16, "ClearRegister(0) passed... (%s:%d)\n", __FILE__, __LINE__);
  WRITESLOW (0x08, 0x22);
  Init001 ();
  DBG (16, "Init001() passed... (%s:%d)\n", __FILE__, __LINE__);
  gEPAT = 0xC7;
  Init002 (0);
  DBG (16, "Init002(0) passed... (%s:%d)\n", __FILE__, __LINE__);

  EPPREGISTERWRITE (0x0A, 0);

  /* catch any failure to read back data in EPP mode */
  reg = EPPRegisterRead (0x0A);
  if (reg != 0)
    {
      DBG (0, "EPPRegisterRead, found 0x%X expected 0x00 (%s:%d)\n", reg,
	   __FILE__, __LINE__);
      if (reg == 0xFF)
	{
	  /* EPP mode not set */
	  DBG (0,
	       "*** It appears that EPP data transfer doesn't work    ***\n");
	  DBG (0,
	       "*** Please read SETTING EPP section in sane-umax_pp.5 ***\n");
	}
      return (0);
    }
  else
    {
      DBG (16, "EPPRegisterRead(0x0A)=0x00 passed... (%s:%d)\n", __FILE__,
	   __LINE__);
    }
  EPPREGISTERWRITE (0x0A, 0xFF);
  EPPREGISTERREAD (0x0A, 0xFF);
  for (i = 1; i < 256; i++)
    {
      EPPREGISTERWRITE (0x0A, i);
      EPPREGISTERREAD (0x0A, i);
      EPPREGISTERWRITE (0x0A, 0xFF - i);
      EPPREGISTERREAD (0x0A, 0xFF - i);
    }

  EPPREGISTERWRITE (0x13, 0x01);
  EPPREGISTERWRITE (0x13, 0x00);
  EPPREGISTERWRITE (0x0A, 0x11);

  for (i = 0; i < 10; i++)
    {
      EPPReadBuffer (0x400, dest);
      for (j = 0; j < 512; j++)
	{
	  if (dest[2 * j] != (j % 256))
	    {
	      DBG (0, "Loop %d, char %d EPPReadBuffer failed! (%s:%d)\n", i,
		   j * 2, __FILE__, __LINE__);
	      return (0);
	    }
	  if (dest[2 * j + 1] != (0xFF - (j % 256)))
	    {
	      DBG (0, "Loop %d, char %d EPPReadBuffer failed! (%s:%d)\n", i,
		   j * 2 + 1, __FILE__, __LINE__);
	      return (0);
	    }
	}
      DBG (16, "Loop %d: EPPReadBuffer(0x400,dest) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }

  for (i = 0; i < 10; i++)
    {
      EPPWriteBuffer (0x400, dest);
      DBG (16, "Loop %d: EPPWriteBuffer(0x400,dest) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }


  EPPREGISTERREAD (0x0C, 4);
  EPPREGISTERWRITE (0x13, 0x01);
  EPPREGISTERWRITE (0x13, 0x00);
  EPPREGISTERWRITE (0x0A, 0x18);

  Outb (DATA, 0x0);
  ClearRegister (0);
  Init001 ();

  if (CheckEPAT () != 0)
    return (0);
  DBG (16, "CheckEPAT() passed... (%s:%d)\n", __FILE__, __LINE__);

  tmp = Inb (CONTROL) & 0x1F;
  Outb (CONTROL, tmp);
  Outb (CONTROL, tmp);

  WRITESLOW (0x68, 0x21);
  Init001 ();
  DBG (16, "Init001() passed... (%s:%d)\n", __FILE__, __LINE__);
  WRITESLOW (0x68, 0x21);
  Init001 ();
  DBG (16, "Init001() passed... (%s:%d)\n", __FILE__, __LINE__);
  SPPResetLPT ();


  if (Init005 (0x80))
    {
      DBG (0, "Init005(0x80) failed... (%s:%d)\n", __FILE__, __LINE__);
    }
  DBG (16, "Init005(0x80) passed... (%s:%d)\n", __FILE__, __LINE__);
  if (Init005 (0xEC))
    {
      DBG (0, "Init005(0xEC) failed... (%s:%d)\n", __FILE__, __LINE__);
    }
  DBG (16, "Init005(0xEC) passed... (%s:%d)\n", __FILE__, __LINE__);


  /* write/read buffer loop */
  for (i = 0; i < 256; i++)
    {
      EPPREGISTERWRITE (0x0A, i);
      EPPREGISTERREAD (0x0A, i);
      EPPREGISTERWRITE (0x0A, 0xFF - i);
      EPPREGISTERREAD (0x0A, 0xFF - i);
    }
  DBG (16, "EPP write/read buffer loop passed... (%s:%d)\n", __FILE__,
       __LINE__);

  EPPREGISTERWRITE (0x13, 0x01);
  EPPREGISTERWRITE (0x13, 0x00);
  EPPREGISTERWRITE (0x0A, 0x11);

  /* test EPP32 mode */
  /* we set 32 bits I/O mode first, then step back to */
  /* 8bits if tests fail                              */
  SetEPPMode (32);
  for (i = 0; i < 10; i++)
    {
      EPPRead32Buffer (0x400, dest);
      /* if 32 bit I/O work, we should have a buffer */
      /* filled by 00 FF 01 FE 02 FD 03 FC .....     */
      for (j = 0; j < 0x200; j++)
	{
	  if ((dest[j * 2] != j % 256)
	      || (dest[j * 2 + 1] != 0xFF - (j % 256)))
	    {
	      DBG (1, "Setting EPP I/O to 8 bits ... (%s:%d)\n", __FILE__,
		   __LINE__);
	      SetEPPMode (8);
	    }
	}
      DBG (16, "Loop %d: EPPRead32Buffer(0x400) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }
  DBG (1, "%d bits EPP data transfer\n", GetEPPMode ());


  for (i = 0; i < 10; i++)
    {
      EPPWrite32Buffer (0x400, dest);
      DBG (16, "Loop %d: EPPWrite32Buffer(0x400,dest) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }



  EPPREGISTERREAD (0x0C, 0x04);
  EPPREGISTERWRITE (0x13, 0x01);
  EPPREGISTERWRITE (0x13, 0x00);
  EPPREGISTERWRITE (0x0A, 0x18);
  WRITESLOW (0x68, 0x21);
  Init001 ();
  DBG (16, "Init001() passed... (%s:%d)\n", __FILE__, __LINE__);
  SPPResetLPT ();

  if (Init005 (0x80))
    {
      DBG (0, "Init005(0x80) failed... (%s:%d)\n", __FILE__, __LINE__);
    }
  DBG (16, "Init005(0x80) passed... (%s:%d)\n", __FILE__, __LINE__);
  if (Init005 (0xEC))
    {
      DBG (0, "Init005(0xEC) failed... (%s:%d)\n", __FILE__, __LINE__);
    }
  DBG (16, "Init005(0xEC) passed... (%s:%d)\n", __FILE__, __LINE__);


  /* write/read buffer loop */
  for (i = 0; i < 256; i++)
    {
      EPPREGISTERWRITE (0x0A, i);
      EPPREGISTERREAD (0x0A, i);
      EPPREGISTERWRITE (0x0A, 0xFF - i);
      EPPREGISTERREAD (0x0A, 0xFF - i);
    }
  DBG (16, "EPP write/read buffer loop passed... (%s:%d)\n", __FILE__,
       __LINE__);

  EPPREGISTERWRITE (0x13, 0x01);
  EPPREGISTERWRITE (0x13, 0x00);
  EPPREGISTERWRITE (0x0A, 0x11);


  for (i = 0; i < 10; i++)
    {
      EPPRead32Buffer (0x400, dest);
      DBG (16, "Loop %d: EPPRead32Buffer(0x400) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }

  for (i = 0; i < 10; i++)
    {
      EPPWrite32Buffer (0x400, dest);
      DBG (16, "Loop %d: EPPWrite32Buffer(0x400,dest) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }
  EPPREGISTERREAD (0x0C, 0x04);
  EPPREGISTERWRITE (0x13, 0x01);
  EPPREGISTERWRITE (0x13, 0x00);
  EPPREGISTERWRITE (0x0A, 0x18);
  WRITESLOW (0x68, 0x21);
  g6FE = 1;
  Init001 ();
  DBG (16, "Init001() passed... (%s:%d)\n", __FILE__, __LINE__);


  reg = EPPRegisterRead (0x0D);
  reg = (reg & 0xE8) | 0x43;
  EPPRegisterWrite (0x0D, reg);

  EPPREGISTERWRITE (0x0A, 0x18);
  EPPREGISTERWRITE (0x0E, 0x0F);
  EPPREGISTERWRITE (0x0F, 0x0C);

  EPPREGISTERWRITE (0x0A, 0x1C);
  EPPREGISTERWRITE (0x0E, 0x10);
  EPPREGISTERWRITE (0x0F, 0x1C);


  reg = EPPRegisterRead (0x0D);
  reg = EPPRegisterRead (0x0D);
  reg = EPPRegisterRead (0x0D);
  reg = (reg & 0xB7) | 0x03;
  EPPRegisterWrite (0x0D, reg);
  DBG (16, "(%s:%d) passed \n", __FILE__, __LINE__);

  reg = EPPRegisterRead (0x12);	/* 0x10 for model 0x0F, 0x20 for model 0x07 */
  reg = reg & 0xEF;
  EPPRegisterWrite (0x12, reg);
  DBG (16, "(%s:%d) passed \n", __FILE__, __LINE__);

  reg = EPPRegisterRead (0x0A);
  if (reg != 0x1C)
    {
      DBG (0, "Warning! expected reg0A=0x1C, found 0x%02X! (%s:%d) \n", reg,
	   __FILE__, __LINE__);
    }
  DBG (16, "(%s:%d) passed \n", __FILE__, __LINE__);
  Init021 ();
  DBG (16, "Init021() passed... (%s:%d)\n", __FILE__, __LINE__);


  /* some sort of countdown, some warming-up ? */
  /* maybe some pauses are needed              */
  /* if (model == 0x07) */
  {
    EPPREGISTERWRITE (0x0A, 0x00);
    reg = EPPRegisterRead (0x0D);
    reg = (reg & 0xE8);
    EPPRegisterWrite (0x0D, reg);
    DBG (16, "(%s:%d) passed \n", __FILE__, __LINE__);
    Init022 ();
    DBG (16, "Init022() passed... (%s:%d)\n", __FILE__, __LINE__);
    reg = EPPRegisterRead (0x0B);
    if (reg != gEPAT)
      {
	DBG (0, "Error! expected reg0B=0x%02X, found 0x%02X! (%s:%d) \n",
	     gEPAT, reg, __FILE__, __LINE__);
	return (0);
      }

    reg = EPPRegisterRead (0x0D);
    reg = (reg & 0xE8) | 0x43;
    EPPRegisterWrite (0x0D, reg);
    EPPREGISTERWRITE (0x0C, 0x04);
    reg = EPPRegisterRead (0x0A);
    if (reg != 0x00)
      {
	DBG (0, "Warning! expected reg0A=0x00, found 0x%02X! (%s:%d) \n",
	     reg, __FILE__, __LINE__);
      }

    EPPREGISTERWRITE (0x0A, 0x1C);
    EPPREGISTERWRITE (0x08, 0x21);
    EPPREGISTERWRITE (0x0E, 0x0F);
    EPPREGISTERWRITE (0x0F, 0x0C);
    usleep (10000);

    EPPREGISTERWRITE (0x0A, 0x1C);
    EPPREGISTERWRITE (0x0E, 0x10);
    EPPREGISTERWRITE (0x0F, 0x1C);
    reg = EPPRegisterRead (0x13);
    if (reg != 0x00)
      {
	DBG (0, "Warning! expected reg13=0x00, found 0x%02X! (%s:%d) \n",
	     reg, __FILE__, __LINE__);
      }
    EPPREGISTERWRITE (0x13, 0x81);
    usleep (10000);
    EPPREGISTERWRITE (0x13, 0x80);

    EPPREGISTERWRITE (0x0E, 0x04);
    EPPREGISTERWRITE (0x0F, 0xFF);
    EPPREGISTERWRITE (0x0E, 0x05);
    EPPREGISTERWRITE (0x0F, 0x03);
    EPPREGISTERWRITE (0x10, 0x66);
    usleep (10000);

    EPPREGISTERWRITE (0x0E, 0x04);
    EPPREGISTERWRITE (0x0F, 0xFF);
    EPPREGISTERWRITE (0x0E, 0x05);
    EPPREGISTERWRITE (0x0F, 0x01);
    EPPREGISTERWRITE (0x10, 0x55);
    usleep (10000);

    EPPREGISTERWRITE (0x0E, 0x04);
    EPPREGISTERWRITE (0x0F, 0xFF);
    EPPREGISTERWRITE (0x0E, 0x05);
    EPPREGISTERWRITE (0x0F, 0x00);
    EPPREGISTERWRITE (0x10, 0x44);
    usleep (10000);

    EPPREGISTERWRITE (0x0E, 0x04);
    EPPREGISTERWRITE (0x0F, 0x7F);
    EPPREGISTERWRITE (0x0E, 0x05);
    EPPREGISTERWRITE (0x0F, 0x00);
    EPPREGISTERWRITE (0x10, 0x33);
    usleep (10000);

    EPPREGISTERWRITE (0x0E, 0x04);
    EPPREGISTERWRITE (0x0F, 0x3F);
    EPPREGISTERWRITE (0x0E, 0x05);
    EPPREGISTERWRITE (0x0F, 0x00);
    EPPREGISTERWRITE (0x10, 0x22);
    usleep (10000);

    EPPREGISTERWRITE (0x0E, 0x04);
    EPPREGISTERWRITE (0x0F, 0x00);
    EPPREGISTERWRITE (0x0E, 0x05);
    EPPREGISTERWRITE (0x0F, 0x00);
    EPPREGISTERWRITE (0x10, 0x11);
    usleep (10000);

    EPPREGISTERWRITE (0x13, 0x81);
    usleep (10000);
    EPPREGISTERWRITE (0x13, 0x80);

    EPPREGISTERWRITE (0x0E, 0x04);
    EPPREGISTERWRITE (0x0F, 0x00);
    EPPREGISTERWRITE (0x0E, 0x05);
    EPPREGISTERWRITE (0x0F, 0x00);
    usleep (10000);

    reg = EPPRegisterRead (0x10);
    DBG (1, "Count-down value is 0x%02X  (%s:%d)\n", reg, __FILE__, __LINE__);
    /* 2 reports of CF, was FF first (typo ?) */
    /* CF seems a valid value                 */
    /* in case of CF, we may have timeout ... */
    /*if (reg != 0x00)
       {
       DBG (0, "Warning! expected reg10=0x00, found 0x%02X! (%s:%d) \n",
       reg, __FILE__, __LINE__);
       } */
    EPPREGISTERWRITE (0x13, 0x00);
  }

  EPPREGISTERWRITE (0x0A, 0x00);
  reg = EPPRegisterRead (0x0D);
  reg = (reg & 0xE8);
  EPPRegisterWrite (0x0D, reg);
  DBG (16, "(%s:%d) passed \n", __FILE__, __LINE__);
  Init022 ();
  DBG (16, "Init022() passed... (%s:%d)\n", __FILE__, __LINE__);
  reg = EPPRegisterRead (0x0B);
  if (reg != gEPAT)
    {
      DBG (0, "Error! expected reg0B=0x%02X, found 0x%02X! (%s:%d) \n", gEPAT,
	   reg, __FILE__, __LINE__);
      return (0);
    }

  reg = EPPRegisterRead (0x0D);
  reg = (reg & 0xE8) | 0x43;
  EPPRegisterWrite (0x0D, reg);
  EPPREGISTERWRITE (0x0C, 0x04);
  reg = EPPRegisterRead (0x0A);
  if (reg != 0x00)
    {
      DBG (0, "Warning! expected reg0A=0x00, found 0x%02X! (%s:%d) \n", reg,
	   __FILE__, __LINE__);
    }

  EPPREGISTERWRITE (0x0A, 0x1C);
  EPPREGISTERWRITE (0x08, 0x21);
  EPPREGISTERWRITE (0x0E, 0x0F);
  EPPREGISTERWRITE (0x0F, 0x0C);
  EPPREGISTERWRITE (0x0A, 0x1C);
  EPPREGISTERWRITE (0x0E, 0x10);
  EPPREGISTERWRITE (0x0F, 0x1C);
  EPPREGISTERWRITE (0x0E, 0x0D);
  EPPREGISTERWRITE (0x0F, 0x00);
  EPPREGISTERWRITE (0x0A, 0x1C);

  /* real start of high level protocol ? */
  if (Fonc001 () != 1)
    {
      DBG (0, "Fonc001() failed ! (%s:%d) \n", __FILE__, __LINE__);
      return (0);
    }
  DBG (16, "Fct001() passed (%s:%d) \n", __FILE__, __LINE__);
  reg = EPPRegisterRead (0x19) & 0xC8;
  /* if reg=E8 or D8 , we have a 'messed' scanner */

  /* 4 tranform buffers + 'void' are sent: 1 B&W, and 3 RGB ? */
  memset (initbuf, 0x00, 2048 * sizeof (int));
  memset (voidbuf, 0x00, 2048 * sizeof (int));

  initbuf[512] = 0xFF;
  initbuf[513] = 0xAA;
  initbuf[514] = 0x55;

  for (j = 0; j < 4; j++)
    {
      for (i = 0; i < 256; i++)
	{
	  voidbuf[512 * j + 2 * i] = i;
	  voidbuf[512 * j + 2 * i] = 0xFF - i;
	}
    }

  /* one pass (B&W ?) */
  if (CmdSetDataBuffer (initbuf) != 1)
    {
      DBG (0, "CmdSetDataBuffer(initbuf) failed ! (%s:%d) \n", __FILE__,
	   __LINE__);
      return (0);
    }
  DBG (16, "CmdSetDataBuffer(initbuf) passed... (%s:%d)\n", __FILE__,
       __LINE__);
  if (CmdSetDataBuffer (voidbuf) != 1)
    {
      DBG (0, "CmdSetDataBuffer(voidbuf) failed ! (%s:%d) \n", __FILE__,
	   __LINE__);
      return (0);
    }
  DBG (16, "CmdSetDataBuffer(voidbuf) passed... (%s:%d)\n", __FILE__,
       __LINE__);

  /* everything above the FF 55 AA tag is 'void' */
  /* it seems that the buffer is reused and only the beginning is initalized */
  for (i = 515; i < 2048; i++)
    initbuf[i] = voidbuf[i];

  /* three pass (RGB ?) */
  for (i = 0; i < 3; i++)
    {
      if (CmdSetDataBuffer (initbuf) != 1)
	{
	  DBG (0, "CmdSetDataBuffer(initbuf) failed ! (%s:%d) \n", __FILE__,
	       __LINE__);
	  return (0);
	}
      DBG (16, "Loop %d: CmdSetDataBuffer(initbuf) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
      if (CmdSetDataBuffer (voidbuf) != 1)
	{
	  DBG (0, "Loop %d: CmdSetDataBuffer(voidbuf) failed ! (%s:%d) \n",
	       __FILE__, __LINE__);
	  return (0);
	}
    }


  /* memory size testing ? */
  /* load 150 Ko in scanner */
  EPPREGISTERWRITE (0x1A, 0x00);
  EPPREGISTERWRITE (0x1A, 0x0C);
  EPPREGISTERWRITE (0x1A, 0x00);
  EPPREGISTERWRITE (0x1A, 0x0C);

  EPPREGISTERWRITE (0x0A, 0x11);	/* start */
  for (i = 0; i < 150; i++)
    {
      EPPWrite32Buffer (0x400, dest);
      DBG (16, "Loop %d: EPPWrite32Buffer(0x400,dest) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }
  EPPREGISTERWRITE (0x0A, 0x18);	/* end */

  /* read them back */
  EPPREGISTERWRITE (0x0A, 0x11);	/*start transfert */
  for (i = 0; i < 150; i++)
    {
      EPPRead32Buffer (0x400, dest);
      DBG (16, "Loop %d: EPPRead32Buffer(0x400,dest) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }
  EPPREGISTERWRITE (0x0A, 0x18);	/*end transfer */



  /* almost CmdSync(0x00) which halts any pending operation */
  if (Fonc001 () != 1)
    {
      DBG (0, "Fonc001() failed ! (%s:%d) \n", __FILE__, __LINE__);
      return (0);
    }
  DBG (16, "Fct001() passed (%s:%d) \n", __FILE__, __LINE__);
  if (SendWord (zero) == 0)
    {
      DBG (0, "SendWord(zero) failed (%s:%d)\n", __FILE__, __LINE__);
      return (0);
    }
  Epilogue ();
  DBG (16, "SendWord(zero) passed (%s:%d)\n", __FILE__, __LINE__);


  /* end transport init */
  /* now high level (connected) protocol begins */
  val = sanei_umax_pp_InitScanner (recover);
  if (val == 0)
    {
      DBG (0, "InitScanner() failed (%s:%d)\n", __FILE__, __LINE__);
      return (0);
    }


  /* if no homing .... */
  if (val == 1)
    {
      CMDSYNC (0);
      CMDSYNC (0xC2);
      CMDSYNC (0);
    }

  /* set port to its initial state */
  Outb (DATA, 0x04);
  Outb (CONTROL, 0x0C);

  free (dest);
  DBG (1, "Probe done ...\n");
  return (1);
}


/* signal handler: simply flags the cancel request */
/*static void
handler (int signum)
{*/
  /* reserve SIGUSR1 for futur use */
  /*if (signum != SIGUSR1)
     gCancel = 1;
     } */


/* set the signal handler to manage signals */
/*static int
InitCancel (void)
{
  signal (SIGINT, handler);
  signal (SIGTERM, handler);
  return (1);
}*/



/* cancel sequence                       */
/* no free, no fclose ... emergency exit */
/*static int
Cancel (void)
{
  if (sanei_umax_pp_Park () == 0)
    {
      DBG (0, "Park failed !!! (%s:%d)\n", __FILE__, __LINE__);
    }
  sanei_umax_pp_EndSession ();
  DBG (1, "Cancel done ...\n");
}*/





static int
deconnect_epat (void)
{
  int tmp;

  EPPREGISTERWRITE (0x0A, 0x00);
  EPPREGISTERREAD (0x0D, 0x40);
  EPPREGISTERWRITE (0x0D, 0x00);
  if (GetModel () != 0x07)
    SendCommand (40);
  SendCommand (30);
  Outb (DATA, gData);
  Outb (CONTROL, gControl);
  return (1);
}


static int
connect_epat (void)
{
  int reg;


  gData = Inb (DATA);
  gControl = Inb (CONTROL) & 0x1F;
  Outb (CONTROL, gControl);
  gControl = Inb (CONTROL) & 0x1F;
  Outb (CONTROL, gControl);
  if (SendCommand (0xE0) != 1)
    {
      DBG (0, "connect_epat: SendCommand(0xE0) failed! (%s:%d)\n", __FILE__,
	   __LINE__);
      /* we try to clean all */
      Epilogue ();
      return (0);
    }
  Outb (DATA, 0x00);
  Outb (CONTROL, 0x01);
  Outb (CONTROL, 0x04);
  ClearRegister (0);
  Init001 ();
  reg = EPPRegisterRead (0x0B);
  if (reg != gEPAT)
    {
      /* ASIC version            is not */
      /* the one expected (epat c7)     */
      DBG (0, "Error! expected reg0B=0x%02X, found 0x%02X! (%s:%d) \n", gEPAT,
	   reg, __FILE__, __LINE__);
      /* we try to clean all */
      Epilogue ();
      return (0);
    }
  reg = EPPRegisterRead (0x0D);
  reg = (reg | 0x43) & 0xEB;
  EPPRegisterWrite (0x0D, reg);
  EPPREGISTERWRITE (0x0C, 0x04);
  reg = EPPRegisterRead (0x0A);
  if (reg != 0x00)
    {
      /* a previous unfinished command */
      /* has left an uncleared value   */
      DBG (0, "Warning! expected reg0A=0x00, found 0x%02X! (%s:%d) \n", reg,
	   __FILE__, __LINE__);
    }
  EPPREGISTERWRITE (0x0A, 0x1C);
  EPPREGISTERWRITE (0x08, 0x21);
  EPPREGISTERWRITE (0x0E, 0x0F);
  EPPREGISTERWRITE (0x0F, 0x0C);
  EPPREGISTERWRITE (0x0A, 0x1C);
  EPPREGISTERWRITE (0x0E, 0x10);
  EPPREGISTERWRITE (0x0F, 0x1C);
  return (1);
}



static int
connect_610P (void)
{
  int tmp;

  gData = Inb (DATA);
  Outb (DATA, 0xAA);
  Outb (CONTROL, 0x0E);
  Inb (CONTROL);
  Inb (CONTROL);
  Outb (DATA, 0x00);
  Outb (CONTROL, 0x0C);
  Inb (CONTROL);
  Inb (CONTROL);
  Outb (DATA, 0x55);
  Outb (CONTROL, 0x0E);
  Inb (CONTROL);
  Inb (CONTROL);
  Outb (DATA, 0xFF);
  Outb (CONTROL, 0x0C);
  Inb (CONTROL);
  Inb (CONTROL);
  Outb (CONTROL, 0x04);
  Inb (CONTROL);
  Inb (CONTROL);
  Outb (DATA, 0x40);
  Outb (CONTROL, 0x06);
  tmp = Inb (STATUS);
  if (tmp != 0x38)
    {
      DBG (0, "Error! expected STATUS=0x38, found 0x%02X! (%s:%d) \n", tmp,
	   __FILE__, __LINE__);
      return (0);
    }
  Outb (CONTROL, 0x07);
  tmp = Inb (STATUS);
  if (tmp != 0x38)
    {
      DBG (0, "Error! expected STATUS=0x38, found 0x%02X! (%s:%d) \n", tmp,
	   __FILE__, __LINE__);
      return (0);
    }
  Outb (CONTROL, 0x04);
  tmp = Inb (STATUS);
  if (tmp != 0xF8)
    {
      DBG (0, "Error! expected STATUS=0xF8, found 0x%02X! (%s:%d) \n", tmp,
	   __FILE__, __LINE__);
      return (0);
    }
  return (1);
}


static int
deconnect_610P (void)
{
  int tmp, i;

  Outb (CONTROL, 0x04);
  i = 0;
  do
    {
      i++;
      tmp = Inb (CONTROL);
      if (tmp != 0x04)
	{
	  DBG (0, "Error! expected CONTROL=0x04, found 0x%02X! (%s:%d) \n",
	       tmp, __FILE__, __LINE__);
	  return (0);
	}
    }
  while ((i < 41) && (tmp == 0x04));

  Outb (DATA, gData);
  return (1);
}

static int
Prologue (void)
{
  switch (sanei_umax_pp_getastra ())
    {
    case 610:
      return (connect_610P ());
    case 1220:
    case 1600:
    case 2000:
      return (connect_epat ());
    }
  return (0);
}



static int
Epilogue (void)
{
  switch (sanei_umax_pp_getastra ())
    {
    case 610:
      return (deconnect_610P ());
    case 1220:
    case 1600:
    case 2000:
      return (deconnect_epat ());
    }
  return (0);
}



static int
CmdSet (int cmd, int len, int *val)
{
  int word[5];
  int i;

  /* cmd 08 as 2 length depending upon model */
  if ((cmd == 8) && (GetModel () == 0x07))
    {
      len = 35;
    }

  /* compute word */
  word[0] = len / 65536;
  word[1] = len / 256 % 256;
  word[2] = len % 256;
  word[3] = (cmd & 0x3F) | 0x80;

  if (!Prologue ())
    {
      DBG (0, "CmdSet: Prologue failed !   (%s:%d)\n", __FILE__, __LINE__);
      return (0);
    }

  /* send data */
  if (SendLength (word, 4) == 0)
    {
      DBG (0, "SendLength(word,4) failed (%s:%d)\n", __FILE__, __LINE__);
      return (0);
    }
  TRACE (16, "SendLength(word,4) passed ...");

  /* head end */
  Epilogue ();

  if (len > 0)
    {
      /* send body */
      if (!Prologue ())
	{
	  DBG (0, "CmdSet: Prologue failed !   (%s:%d)\n", __FILE__,
	       __LINE__);
	}
      if (DBG_LEVEL >= 8)
	{
	  char *str = NULL;

	  str = malloc (3 * len);
	  if (str != NULL)
	    {
	      for (i = 0; i < len; i++)
		{
		  sprintf (str + 3 * i, "%02X ", val[i]);
		}
	      str[3 * i] = 0x00;
	      DBG (8, "String sent     for %02X: %s\n", cmd, str);
	      free (str);
	    }
	  else
	    {
	      TRACE (8, "not enough memory for debugging ...");
	    }
	}

      /* send data */
      if (SendData (val, len) == 0)
	{
	  DBG (0, "SendData(word,%d) failed (%s:%d)\n", len, __FILE__,
	       __LINE__);
	  Epilogue ();
	  return (0);
	}
      TRACE (16, "SendData(val,len) passed ...");
      /* body end */
      Epilogue ();
    }
  return (1);
}

static int
CmdGet (int cmd, int len, int *val)
{
  int word[5];
  int i;

  /* cmd 08 as 2 length depending upon model */
  if ((cmd == 8) && (GetModel () == 0x07))
    {
      len = 35;
    }

  /* compute word */
  word[0] = len / 65536;
  word[1] = len / 256 % 256;
  word[2] = len % 256;
  word[3] = (cmd & 0x3F) | 0x80 | 0x40;	/* 0x40 means 'read' */
  word[4] = -1;

  /* send header */
  if (!Prologue ())
    {
      DBG (0, "CmdGet: Prologue failed !   (%s:%d)\n", __FILE__, __LINE__);
      return (0);
    }

  /* send data */
  if (SendLength (word, 4) == 0)
    {
      DBG (0, "SendLength(word,4) failed (%s:%d)\n", __FILE__, __LINE__);
      return (0);
    }
  TRACE (16, "SendLength(word,4) passed ...");

  /* head end */
  Epilogue ();


  /* send header */
  if (!Prologue ())
    {
      DBG (0, "CmdGet: Prologue failed !   (%s:%d)\n", __FILE__, __LINE__);
      return (0);
    }

  /* get actual data */
  if (ReceiveData (val, len) == 0)
    {
      DBG (0, "ReceiveData(val,len) failed (%s:%d)\n", __FILE__, __LINE__);
      Epilogue ();
      return (0);
    }
  if (DBG_LEVEL >= 8)
    {
      char *str = NULL;

      str = malloc (3 * len);
      if (str != NULL)
	{
	  for (i = 0; i < len; i++)
	    {
	      sprintf (str + 3 * i, "%02X ", val[i]);
	    }
	  str[3 * i] = 0x00;
	  DBG (8, "String received for %02X: %s\n", cmd, str);
	  free (str);
	}
      else
	{
	  TRACE (8, "not enough memory for debugging ...");
	}
    }
  Epilogue ();
  return (1);
}



static int
CmdSetGet (int cmd, int len, int *val)
{
  int *tampon;
  int i;

  /* model revision 0x07 uses 35 bytes buffers */
  /* cmd 08 as 2 length depending upon model */
  if ((cmd == 8) && (GetModel () == 0x07))
    {
      len = 35;
    }

  /* first we send */
  if (CmdSet (cmd, len, val) == 0)
    {
      DBG (0, "CmdSetGet failed !  (%s:%d)\n", __FILE__, __LINE__);
      return (0);
    }

  tampon = (int *) malloc (len * sizeof (int));
  memset (tampon, 0x00, len * sizeof (int));
  if (tampon == NULL)
    {
      DBG (0, "Failed to allocate room for %d int ! (%s:%d)\n", len, __FILE__,
	   __LINE__);
      Epilogue ();
      return (0);
    }

  /* then we receive */
  if (CmdGet (cmd, len, tampon) == 0)
    {
      DBG (0, "CmdSetGet failed !  (%s:%d)\n", __FILE__, __LINE__);
      free (tampon);
      Epilogue ();
      return (0);
    }

  /* check and copy */
  for (i = 0; (i < len) && (val[i] >= 0); i++)
    {
      if (tampon[i] != val[i])
	{
	  DBG
	    (0,
	     "Warning data read back differs: expected %02X found tampon[%d]=%02X ! (%s:%d)\n",
	     val[i], i, tampon[i], __FILE__, __LINE__);
	}
      val[i] = tampon[i];
    }


  /* OK */
  free (tampon);
  return (1);
}


/* 1 OK, 0 failed */
static int
CmdGetBuffer (int cmd, int len, unsigned char *buffer)
{
  int reg, tmp, i;
  int word[5], read;
  int needed;

  /* compute word */
  word[0] = len / 65536;
  word[1] = len / 256 % 256;
  word[2] = len % 256;
  word[3] = (cmd & 0x3F) | 0x80 | 0x40;
  word[4] = -1;

  /* send word: len+addr(?) */
  if (FoncSendWord (word) == 0)
    {
      DBG (0, "FoncSendWord(word) failed (%s:%d)\n", __FILE__, __LINE__);
      return (0);
    }
  DBG (16, "(%s:%d) passed \n", __FILE__, __LINE__);
  Init022 ();
  DBG (16, "Init022() passed... (%s:%d)\n", __FILE__, __LINE__);
  reg = EPPRegisterRead (0x0B);
  if (reg != gEPAT)
    {
      /* return -1 */
      DBG (0, "Error! expected reg0B=0x%02X, found 0x%02X! (%s:%d) \n", gEPAT,
	   reg, __FILE__, __LINE__);
      return (0);
    }
  reg = EPPRegisterRead (0x0D);
  reg = (reg & 0xE8) | 0x43;
  EPPRegisterWrite (0x0D, reg);
  EPPREGISTERWRITE (0x0C, 0x04);
  reg = EPPRegisterRead (0x0A);
  if (reg != 0x00)
    {
      DBG (0, "Warning! expected reg0A=0x00, found 0x%02X! (%s:%d) \n", reg,
	   __FILE__, __LINE__);
    }
  EPPREGISTERWRITE (0x0A, 0x1C);
  EPPREGISTERWRITE (0x08, 0x21);
  EPPREGISTERWRITE (0x0E, 0x0F);
  EPPREGISTERWRITE (0x0F, 0x0C);
  EPPREGISTERWRITE (0x0A, 0x1C);
  EPPREGISTERWRITE (0x0E, 0x10);
  EPPREGISTERWRITE (0x0F, 0x1C);
  EPPREGISTERWRITE (0x0E, 0x0D);
  EPPREGISTERWRITE (0x0F, 0x00);

  i = 0;
  reg = EPPRegisterRead (0x19) & 0xF8;

  /* wait if busy */
  while ((reg & 0x08) == 0x08)
    reg = EPPRegisterRead (0x19) & 0xF8;
  if ((reg != 0xC0) && (reg != 0xD0))
    {
      DBG (0, "CmdGetBuffer failed (%s:%d)\n", __FILE__, __LINE__);
      return (0);
    }

  read = 0;
  reg = EPPRegisterRead (0x0C);
  if (reg != 0x04)
    {
      DBG (0, "CmdGetBuffer failed: unexpected status 0x%02X  ...(%s:%d)\n",
	   reg, __FILE__, __LINE__);
      return (0);
    }
  EPPREGISTERWRITE (0x0C, reg | 0x40);

  /* actual data */
  read = 0;
  while (read < len)
    {
      needed = len - read;
      if (needed > 32768)
	needed = 32768;
      EPPBlockMode (0x80);
      tmp = PausedReadBuffer (needed, buffer + read);
      if (tmp < needed)
	{
	  DBG (64, "CmdGetBuffer only got %d bytes out of %d ...(%s:%d)\n",
	       tmp, needed, __FILE__, __LINE__);
	}
      else
	{
	  DBG (64,
	       "CmdGetBuffer got all %d bytes out of %d , read=%d...(%s:%d)\n",
	       tmp, 32768, read, __FILE__, __LINE__);
	}
      read += tmp;
      DBG (16, "Read %d bytes out of %d (last block is %d bytes) (%s:%d)\n",
	   read, len, tmp, __FILE__, __LINE__);
      if (read < len)
	{
	  /* wait for scanner to be ready */
	  reg = EPPRegisterRead (0x19) & 0xF8;
	  DBG (64, "Status after block read is 0x%02X (%s:%d)\n", reg,
	       __FILE__, __LINE__);
	  if ((reg & 0x08) == 0x08)
	    {
	      int pass = 0;

	      do
		{
		  reg = EPPRegisterRead (0x19) & 0xF8;
		  usleep (100);
		  pass++;
		}
	      while ((pass < 32768) && ((reg & 0x08) == 0x08));
	      DBG (64, "Status after waiting is 0x%02X (pass=%d) (%s:%d)\n",
		   reg, pass, __FILE__, __LINE__);
	      if ((reg != 0xC0) && (reg != 0xD0))
		{
		  DBG (0,
		       "Unexpected status 0x%02X, expected 0xC0 or 0xD0 ! (%s:%d)\n",
		       reg, __FILE__, __LINE__);
		  DBG (0, "Going on...\n");
		}
	    }

	  /* signal we want next data chunk */
	  reg = EPPRegisterRead (0x0C);
	  EPPRegisterWrite (0x0C, reg | 0x40);
	}
    }

  /* OK ! */
  EPPREGISTERWRITE (0x0E, 0x0D);
  EPPREGISTERWRITE (0x0F, 0x00);

  /* epilogue */
  Epilogue ();
  return (1);
}

/* 1 OK, 0 failed */
static int
CmdGetBuffer32 (int cmd, int len, unsigned char *buffer)
{
  int reg, tmp, i;
  int word[5], read;

  /* compute word */
  word[0] = len / 65536;
  word[1] = len / 256 % 256;
  word[2] = len % 256;
  word[3] = (cmd & 0x3F) | 0x80 | 0x40;

  if (!Prologue ())
    {
      DBG (0, "CmdSet: Prologue failed !   (%s:%d)\n", __FILE__, __LINE__);
      return (0);
    }

  /* send data */
  if (SendLength (word, 4) == 0)
    {
      DBG (0, "SendLength(word,4) failed (%s:%d)\n", __FILE__, __LINE__);
      return (0);
    }
  TRACE (16, "SendLength(word,4) passed ...");

  /* head end */
  Epilogue ();

  Prologue ();
  EPPREGISTERWRITE (0x0E, 0x0D);
  EPPREGISTERWRITE (0x0F, 0x00);

  i = 0;
  reg = EPPRegisterRead (0x19) & 0xF8;

  /* wait if busy */
  while ((reg & 0x08) == 0x08)
    reg = EPPRegisterRead (0x19) & 0xF8;
  if ((reg != 0xC0) && (reg != 0xD0))
    {
      DBG (0, "CmdGetBuffer32 failed: unexpected status 0x%02X  ...(%s:%d)\n",
	   reg, __FILE__, __LINE__);
      return (0);
    }
  reg = EPPRegisterRead (0x0C);
  if (reg != 0x04)
    {
      DBG (0, "CmdGetBuffer32 failed: unexpected status 0x%02X  ...(%s:%d)\n",
	   reg, __FILE__, __LINE__);
      return (0);
    }
  EPPREGISTERWRITE (0x0C, reg | 0x40);

  read = 0;
  while (read < len)
    {
      if (read + 1700 < len)
	{
	  tmp = 1700;
	  EPPRead32Buffer (tmp, buffer + read);
	  reg = EPPRegisterRead (0x19) & 0xF8;
	  if ((read + tmp < len) && (reg & 0x08) == 0x08)
	    {
	      do
		{
		  reg = EPPRegisterRead (0x19) & 0xF8;
		}
	      while ((reg & 0x08) == 0x08);
	      if ((reg != 0xC0) && (reg != 0xD0))
		{
		  DBG (0,
		       "Unexpected status 0x%02X, expected 0xC0 or 0xD0 ! (%s:%d)\n",
		       reg, __FILE__, __LINE__);
		  DBG (0, "Going on...\n");
		}
	    }
	  reg = EPPRegisterRead (0x0C);
	  EPPRegisterWrite (0x0C, reg | 0x40);
	  read += tmp;
	}
      else
	{
	  tmp = len - read;
	  EPPRead32Buffer (tmp, buffer + read);
	  read += tmp;
	  if ((read < len))
	    {
	      reg = EPPRegisterRead (0x19) & 0xF8;
	      while ((reg & 0x08) == 0x08)
		{
		  reg = EPPRegisterRead (0x19) & 0xF8;
		}
	    }
	}
    }

  /* OK ! */
  Epilogue ();
  return (1);
}

int
sanei_umax_pp_CmdSync (int cmd)
{
  int word[5];

  if (!Prologue ())
    {
      DBG (0, "CmdSync: Prologue failed !   (%s:%d)\n", __FILE__, __LINE__);
    }

  /* compute word */
  word[0] = 0x00;
  word[1] = 0x00;
  word[2] = 0x00;
  word[3] = cmd;


  /* send data */
  if (SendLength (word, 4) == 0)
    {
      DBG (0, "SendLength(word,4) failed (%s:%d)\n", __FILE__, __LINE__);
      return (0);
    }
  TRACE (16, "SendLength(word,4) passed ...");

  /* end OK */
  Epilogue ();
  return (1);
}


/* numbers of bytes read, else 0 (failed)                             */
/* read data by chunk EXACTLY the width of the scan area in the given */
/* resolution . If a valid file descriptor is given, we write data    */
/* in it according to the color mode, before polling the scanner      */
/* len should not be bigger than 2 Megs				      */

int
CmdGetBlockBuffer (int cmd, int len, int window, unsigned char *buffer)
{
  struct timeval td, tf;
  float elapsed;
  int reg, i;
  int word[5], read;

  /* compute word */
  word[0] = len / 65536;
  word[1] = len / 256 % 256;
  word[2] = len % 256;
  word[3] = (cmd & 0x3F) | 0x80 | 0x40;

  if (!Prologue ())
    {
      DBG (0, "CmdGetBlockBuffer: Prologue failed !   (%s:%d)\n", __FILE__,
	   __LINE__);
    }

  /* send data */
  if (SendLength (word, 4) == 0)
    {
      DBG (0, "SendLength(word,4) failed (%s:%d)\n", __FILE__, __LINE__);
      return (0);
    }
  TRACE (16, "SendLength(word,4) passed ...");
  /* head end */
  Epilogue ();



  if (!Prologue ())
    {
      DBG (0, "CmdGetBlockBuffer: Prologue failed !   (%s:%d)\n", __FILE__,
	   __LINE__);
    }


  EPPREGISTERWRITE (0x0E, 0x0D);
  EPPREGISTERWRITE (0x0F, 0x00);

  i = 0;

  /* init counter */
  read = 0;

  /* read scanner state */
  reg = EPPRegisterRead (0x19) & 0xF8;


  /* read loop */
  while (read < len)
    {
      /* wait for the data to be ready */
      gettimeofday (&td, NULL);
      while ((reg & 0x08) == 0x08)
	{
	  reg = EPPRegisterRead (0x19) & 0xF8;
	  gettimeofday (&tf, NULL);
	  elapsed =
	    ((tf.tv_sec * 1000000 + tf.tv_usec) -
	     (td.tv_sec * 1000000 + td.tv_usec)) / 1000000;
	  if (elapsed > 3)
	    {
	      DBG
		(0,
		 "Time-out (%.2f s) waiting for scanner ... giving up on status 0x%02X !   (%s:%d)\n",
		 elapsed, reg, __FILE__, __LINE__);
	      Epilogue ();
	      return (read);
	    }
	}
      if ((reg != 0xC0) && (reg != 0xD0) && (reg != 0x00))
	{
	  DBG (0,
	       "Unexpected status 0x%02X, expected 0xC0 or 0xD0 ! (%s:%d)\n",
	       reg, __FILE__, __LINE__);
	  DBG (0, "Going on...\n");
	}

      /* signals next chunk */
      reg = EPPRegisterRead (0x0C);
      if (reg != 0x04)
	{
	  DBG (0,
	       "CmdGetBlockBuffer failed: unexpected value reg0C=0x%02X  ...(%s:%d)\n",
	       reg, __FILE__, __LINE__);
	  return (0);
	}
      EPPREGISTERWRITE (0x0C, reg | 0x40);


      /* there is always a full block ready when scanner is ready */
      /* 32 bits I/O read , window must match the width of scan   */
      if (GetEPPMode () == 32)
	EPPRead32Buffer (window, buffer + read);
      else
	EPPReadBuffer (window, buffer + read);
      /* sum bytes read */
      read += window;


      DBG (16, "Read %d bytes out of %d (last block is %d bytes) (%s:%d)\n",
	   read, len, window, __FILE__, __LINE__);

      /* test status after read */
      reg = EPPRegisterRead (0x19) & 0xF8;
    }


  /* wait for the data to be ready */
  gettimeofday (&td, NULL);
  while ((reg & 0x08) == 0x08)
    {
      reg = EPPRegisterRead (0x19) & 0xF8;
      gettimeofday (&tf, NULL);
      elapsed =
	((tf.tv_sec * 1000000 + tf.tv_usec) -
	 (td.tv_sec * 1000000 + td.tv_usec)) / 1000000;
      if (elapsed > 3)
	{
	  DBG
	    (0,
	     "Time-out (%.2f s) waiting for scanner ... giving up on status 0x%02X !   (%s:%d)\n",
	     elapsed, reg, __FILE__, __LINE__);
	  Epilogue ();
	  return (read);
	}
    }
  if ((reg != 0xC0) && (reg != 0xD0) && (reg != 0x00))
    {
      DBG (0, "Unexpected status 0x%02X, expected 0xC0 or 0xD0 ! (%s:%d)\n",
	   reg, __FILE__, __LINE__);
      DBG (0, "Going on...\n");
    }

  EPPREGISTERWRITE (0x0E, 0x0D);
  EPPREGISTERWRITE (0x0F, 0x00);


  /* OK ! */
  Epilogue ();
  return (read);
}

static void
Bloc2Decode (int *op)
{
  int i;
  int scanh;
  int skiph;
  int dpi = 0;
  int dir = 0;
  int color = 0;
  char str[64];

  for (i = 0; i < 16; i++)
    sprintf (str + 3 * i, "%02X ", op[i]);
  str[48] = 0x00;
  DBG (0, "Command bloc 2: %s\n", str);


  scanh = op[0] + (op[1] & 0x3F) * 256;
  skiph = ((op[1] & 0xC0) >> 6) + (op[2] * 4) + ((op[3] & 0x0F) << 10);

  if (op[3] & 0x10)
    dir = 1;
  else
    dir = 0;

  if (op[13] & 0x04)
    color = 1;
  else
    color = 0;

  /* op[6]=0x60 at 600 and 1200 dpi */
  if ((op[8] == 0x17) && (op[9] != 0x05))
    dpi = 150;
  if ((op[8] == 0x17) && (op[9] == 0x05))
    dpi = 300;
  if ((op[9] == 0x05) && (op[14] & 0x08))
    dpi = 1200;
  if ((dpi == 0) && ((op[14] & 0x08) == 0))
    dpi = 600;



  DBG (0, "\t->scan height   =0x%04X (%d)\n", scanh, scanh);
  DBG (0, "\t->skip height   =0x%04X (%d)\n", skiph, skiph);
  DBG (0, "\t->y dpi         =0x%04X (%d)\n", dpi, dpi);
  DBG (0, "\t->channel 1 gain=0x%02X (%d)\n", op[10] & 0x0F, op[10] & 0x0F);
  DBG (0, "\t->channel 2 gain=0x%02X (%d)\n", (op[10] & 0xF0) / 16,
       (op[10] & 0xF0) / 16);
  DBG (0, "\t->channel 3 gain=0x%02X (%d)\n", op[11] & 0x0F, op[11] & 0x0F);
  DBG (0, "\t->channel 1 high=0x%02X (%d)\n", (op[11] / 16) & 0x0F,
       (op[11] / 16) & 0x0F);
  DBG (0, "\t->channel 2 high=0x%02X (%d)\n", (op[12] & 0xF0) / 16,
       (op[12] & 0xF0) / 16);
  DBG (0, "\t->channel 3 high=0x%02X (%d)\n", op[12] & 0x0F, op[12] & 0x0F);
  if (dir)
    DBG (0, "\t->forward direction\n");
  else
    DBG (0, "\t->reverse direction\n");
  if (color)
    DBG (0, "\t->color scan       \n");
  else
    DBG (0, "\t->no color scan    \n");
  DBG (0, "\n");
}


static void
Bloc8Decode (int *op)
{
  int i, bpl;
  int xskip;
  int xend;
  char str[128];

  for (i = 0; i < 36; i++)
    sprintf (str + 3 * i, "%02X ", op[i]);
  str[3 * i] = 0x00;
  DBG (0, "Command bloc 8: %s\n", str);

  xskip = op[17] + 256 * (op[18] & 0x0F);
  if (op[33] & 0x40)
    xskip += 0x1000;
  xend = (op[18] & 0xF0) / 16 + 16 * op[19];
  if (op[33] & 0x80)
    xend += 0x1000;
  bpl = (op[24] - 0x41) * 256 + 8192 * (op[34] & 0x01) + op[23];

  DBG (0, "\t->xskip     =0x%X (%d)\n", xskip, xskip);
  DBG (0, "\t->xend      =0x%X (%d)\n", xend, xend);
  DBG (0, "\t->scan width=0x%X (%d)\n", xend - xskip - 1, xend - xskip - 1);
  DBG (0, "\t->bytes/line=0x%X (%d)\n", bpl, bpl);
  DBG (0, "\n");
}

static int
CompletionWait (void)
{
  CMDSYNC (0x40);
  do
    {
      usleep (100000);
      CMDSYNC (0xC2);
    }
  while ((sanei_umax_pp_ScannerStatus () & 0x90) != 0x90);
  CMDSYNC (0xC2);
  return (1);
}

int
sanei_umax_pp_SetLamp (int on)
{
  int buffer[17];
  int state;



  /* reset? */
  sanei_umax_pp_CmdSync (0x00);
  sanei_umax_pp_CmdSync (0xC2);
  sanei_umax_pp_CmdSync (0x00);

  /* get status */
  CmdGet (0x02, 16, buffer);
  state = buffer[14] & LAMP_STATE;
  buffer[16] = -1;
  if ((state == 0) && (on == 0))
    {
      DBG (0, "Lamp already off ... (%s:%d)\n", __FILE__, __LINE__);
      return (1);
    }
  if ((state) && (on))
    {
      DBG (2, "Lamp already on ... (%s:%d)\n", __FILE__, __LINE__);
      return (1);
    }

  /* set lamp state */
  if (on)
    buffer[14] = buffer[14] | LAMP_STATE;
  else
    buffer[14] = buffer[14] & ~LAMP_STATE;
  if (CmdSetGet (0x02, 16, buffer) != 1)
    {
      DBG (0, "CmdSetGet(0x02,16,buffer) failed (%s:%d)\n", __FILE__,
	   __LINE__);
      return (0);
    }
  DBG (16, "CmdSetGet(0x02,16,buffer) passed ... (%s:%d)\n", __FILE__,
       __LINE__);
  return (1);
}

static int num = 0;
static void
Dump (int len, unsigned char *data, char *name)
{
  FILE *fic;
  char titre[80];

  if (name == NULL)
    {
      sprintf (titre, "dump%04d.bin", num);
      num++;
    }
  else
    {
      sprintf (titre, "%s", name);
    }
  fic = fopen (titre, "wb");
  if (fic == NULL)
    {
      DBG (0, "could not open %s for writing\n", titre);
      return;
    }
  fwrite (data, 1, len, fic);
  fclose (fic);
}


static void
DumpNB (int width, int height, unsigned char *data, char *name)
{
  FILE *fic;
  char titre[80];

  if (name == NULL)
    {
      sprintf (titre, "dump%04d.pnm", num);
      num++;
    }
  else
    {
      sprintf (titre, "%s", name);
    }
  fic = fopen (titre, "wb");
  if (fic == NULL)
    {
      DBG (0, "could not open %s for writing\n", titre);
      return;
    }
  fprintf (fic, "P5\n%d %d\n255\n", width, height);
  fwrite (data, width, height, fic);
  fclose (fic);
}

static void
DumpRVB (int width, int height, unsigned char *data, char *name)
{
  FILE *fic;
  char titre[80];
  int y, x;

  if (name == NULL)
    {
      sprintf (titre, "dump%04d.pnm", num);
      num++;
    }
  else
    {
      sprintf (titre, "%s", name);
    }
  fic = fopen (titre, "wb");
  fprintf (fic, "P6\n%d %d\n255\n", width, height);
  if (fic == NULL)
    {
      DBG (0, "could not open %s for writing\n", titre);
      return;
    }
  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
	{
	  fputc (data[3 * y * width + 2 * width + x], fic);
	  fputc (data[3 * y * width + width + x], fic);
	  fputc (data[3 * y * width + x], fic);
	}
    }
  fclose (fic);
}




static void
ComputeCalibrationData (int color, int dpi, int width, unsigned char *source,
			int *data)
{
  int p, i, l;
  int sum, gn;


  memset (data, 0, (3 * 5100 + 768 + 3) * sizeof (int));


  /* 0 -> 5099 */
  for (i = 0; i < width; i++)
    {				/* red calibration data */
      if (color >= RGB_MODE)
	{
	  /* compute average */
	  sum = 0;
	  for (l = 0; l < 66; l++)
	    sum += source[i + l * 5100 * 3];
	  gn = (int) (((double) (250 * l) / sum - 0.984) * 102.547 + 0.5);
	  if (gn < 0)
	    gn = 0;
	  else if (gn > 255)
	    gn = 255;
	  data[i] = gn;
	}
      else
	data[i] = 0x00;
    }


  /* 5100 -> 10199: green data */
  p = 5100;
  for (i = 0; i < width; i++)
    {
      /* compute average */
      sum = 0;
      for (l = 0; l < 66; l++)
	sum += source[i + l * 5100 * 3 + 5100];
      gn = (int) (((double) (250 * l) / sum - 0.984) * 102.547 + 0.5);
      if (gn < 0)
	gn = 0;
      else if (gn > 255)
	gn = 255;
      data[p + i] = gn;
    }


  /* 10200 -> 15299: blue */
  p = 10200;
  for (i = 0; i < width; i++)
    {
      if (color >= RGB_MODE)
	{
	  /* compute average */
	  sum = 0;
	  for (l = 0; l < 66; l++)
	    sum += source[i + l * 5100 * 3 + 5100 * 2];
	  gn = (int) (((double) (250 * l) / sum - 0.984) * 102.547 + 0.5);
	  if (gn < 0)
	    gn = 0;
	  else if (gn > 255)
	    gn = 255;
	  data[p + i] = gn;
	}
      else
	data[p + i] = 0x00;
    }


  /* gamma tables */
  for (i = 0; i < 256; i++)
    data[15300 + i] = ggRed[i];
  for (i = 0; i < 256; i++)
    data[15300 + 256 + i] = ggGreen[i];
  for (i = 0; i < 256; i++)
    data[15300 + 512 + i] = ggBlue[i];




  if (color >= RGB_MODE)
    {
      switch (dpi)
	{
	case 1200:
	case 600:
	  data[16068] = 0xFF;
	  data[16069] = 0xFF;
	  break;
	case 300:
	  data[16068] = 0xAA;
	  data[16069] = 0xFF;
	  break;
	case 150:
	  data[16068] = 0x88;
	  data[16069] = 0xFF;
	  break;
	case 75:
	  data[16068] = 0x80;
	  data[16069] = 0xAA;
	  break;
	}
    }
  else
    {
      switch (dpi)
	{
	case 1200:
	case 600:
	  data[16068] = 0xFF;
	  data[16069] = 0xFF;
	  break;
	case 300:
	  data[16068] = 0xAA;
	  data[16069] = 0xFF;
	  break;
	case 150:
	  data[16068] = 0x88;
	  data[16069] = 0xAA;
	  break;
	case 75:
	  data[16068] = 0x80;
	  data[16069] = 0x88;
	  break;
	}
    }

  data[16070] = -1;
}

/* move head by the distance given using precision or not */
/* 0: failed  
   1: success					  */
static int
Move (int distance, int precision, unsigned char *buffer)
{
  int header[17] =
    { 0x01, 0x00, 0x00, 0x20, 0x00, 0x00, 0x60, 0x2F, 0x2F, 0x01, 0x00, 0x00,
    0x00, 0x80, 0xA4, 0x00, -1
  };
  int body[37] =
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x04, 0x00, 0x6E, 0xF6, 0x79, 0xBF, 0x01, 0x00, 0x00, 0x00,
    0x46, 0xA0, 0x00, 0x8B, 0x49, 0x2A, 0xE9, 0x68, 0xDF, 0x13, 0x1A, 0x00,
    -1
  };
  int end[9] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, -1 };
  int steps, len;
  unsigned char tmp[0x200];
  unsigned char *ptr;

  if (distance == 0)
    return (0);

  if (buffer == NULL)
    ptr = tmp;
  else
    ptr = buffer;

  /* build commands */
  if (distance < 0)
    {
      /* header */
      steps = -distance - 1;
      header[3] = 0x20;
      header[9] = 0x01;

      /* reverse direction body by default */

      /* end */
      end[1] = 0xFF;
      end[2] = 0xFF;
      end[3] = -1;
      len = 3;
    }
  else
    {
      /* header */
      steps = distance - 1;
      header[3] = 0x70;
      header[9] = 0x05;

      /* body */
      body[2] = 0x04;
      body[4] = 0x02;
      body[7] = 0x0C;
      body[9] = 0x04;
      body[10] = 0x40;
      body[11] = 0x01;
      /* end */
      len = 8;
    }
  if (steps > 0)
    {
      header[1] = (steps << 6) & 0xFF;
      header[2] = (steps >> 2) & 0xFF;
      header[3] = header[3] | ((steps >> 10) & 0x0F);
    }


  /* precision: default header set to precision on */
  if (precision == PRECISION_OFF)
    {
      if (sanei_umax_pp_getastra () == 1600)
	header[8] = 0x15;
      else
	header[8] = 0x17;
      header[14] = 0xAC;
      body[20] = 0x06;
    }
  if (DBG_LEVEL >= 128)
    {
      Bloc2Decode (header);
      Bloc8Decode (body);
    }
  CMDSETGET (0x02, 16, header);
  CMDSETGET (0x08, 36, body);
  if (DBG_LEVEL >= 128)
    {
      Bloc2Decode (header);
      Bloc8Decode (body);
    }
  CMDSYNC (0xC2);
  if (sanei_umax_pp_ScannerStatus () & 0x80)
    {
      CMDSYNC (0x00);
    }
  CMDSETGET (4, len, end);
  COMPLETIONWAIT;
  if (DBG_LEVEL >= 128)
    {
      Dump (0x200, ptr, NULL);
    }
  CMDGETBUF (4, 0x200, ptr);
  DBG (16, "MOVE STATUS IS 0x%02X  (%s:%d)\n", sanei_umax_pp_ScannerStatus (),
       __FILE__, __LINE__);
  CMDSYNC (0x00);
  return (1);
}




/* for each column, finds the row where white/black transition occurs
	then returns the average */
static float
EdgePosition (int width, int height, unsigned char *data)
{
  int ecnt, x, y;
  float epos;
  int d, dmax, dpos;

  epos = 0;
  ecnt = 0;
  for (x = 0; x < width; x++)
    {
      /* first edge: white->black */
      dmax = 0;
      dpos = 0;
      for (y = 1; y < height; y++)
	{
	  d = data[x + (y - 1) * width] - data[x + y * width];
	  if (d > dmax)
	    {
	      dmax = d;
	      dpos = y;
	    }
	}
      if (dmax > 0)
	{
	  epos += dpos;
	  ecnt++;
	}
    }
  if (ecnt == 0)
    epos = 70;
  else
    epos = epos / ecnt;
  return (epos);
}


static int
MoveToOrigin (void)
{
  unsigned char buffer[54000];
  float edge;
  int val, delta;
  int header[17] =
    { 0xB4, 0x00, 0x00, 0x70, 0x00, 0x00, 0x60, 0x2F, 0x2F, 0x05, 0x00, 0x00,
    0x00, 0x80, 0xA4, 0x00, -1
  };
  int body[37] =
    { 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C, 0x00, 0x04, 0x40, 0x01,
    0x00, 0x00, 0x04, 0x00, 0x6E, 0xFB, 0xC4, 0xE5, 0x06, 0x00, 0x00, 0x60,
    0x4D, 0xA0, 0x00, 0x8B, 0x49, 0x2A, 0xE9, 0x68, 0xDF, 0x13, 0x1A, 0x00,
    -1
  };
  int end[9] = { 0x06, 0xF4, 0xFF, 0x81, 0x1B, 0x00, 0x08, 0x00, -1 };
  int opsc03[9] = { 0x00, 0x00, 0x00, 0xAA, 0xCC, 0xEE, 0x80, 0xFF, -1 };

  /* 1600P command set */
  if (sanei_umax_pp_getastra () == 1600)
    {
      header[8] = 0x2B;

      body[29] = 0x1A;
      body[30] = 0xEE;

      end[0] = 0x19;
      end[1] = 0xD5;
      end[4] = 0x1B;
    }

  /* sync scanner then move head by the estimated origin */
  CMDSYNC (0x00);
  CMDSYNC (0xC2);
  CMDSYNC (0x00);
  MOVE (196, PRECISION_OFF, NULL);

  /* scan 2400 'x' by 180 y, B&W at 600 dpi, starting at the   */
  /* first quarter of the x area. This should cover a black    */
  /* line drawn in the middle of a white area, under the piece */
  /* that separates the 'little' window and the scan window    */
  CMDSETGET (0x02, 16, header);
  CMDSETGET (0x08, 36, body);
  if (DBG_LEVEL > 128)
    {
      Bloc2Decode (header);
      Bloc8Decode (body);
    }
  CMDSETGET (0x01, 8, end);

  CMDSYNC (0xC2);
  CMDSYNC (0x00);
  /* signals black & white ? */
  CMDSETGET (0x04, 8, opsc03);
  COMPLETIONWAIT;
  CMDGETBUF (0x04, 54000, buffer);	/* get find position data */
  if (DBG_LEVEL > 128)
    {
      DumpNB (300, 180, buffer, NULL);
    }

  /* detection of 1600P is a by product of origin finding */
  edge = 0.0;
  for (val = 0; val < 54000; val++)
    if (buffer[val] > edge)
      edge = buffer[val];
  DBG (32, "MAX VALUE=%f	(%s:%d)\n", edge, __FILE__, __LINE__);
  if ((edge <= 30) && (sanei_umax_pp_getastra () != 1600))
    {
      DBG (2, "MoveToOrigin() detected a 1600P");
      sanei_umax_pp_setastra (1600);
    }
  edge = EdgePosition (300, 180, buffer);
  /* rounded to lowest integer, since upping origin might lead */
  /* to bump in the other side if doing a preview              */
  val = (int) (edge);

  /* edge is 60 dots (at 600 dpi) from origin */
  /* origin=current pos - 180 + edge + 60     */
  /* grumpf, there is an offset somewhere ... */
  delta = -180 + edge - 8;
  DBG (64, "Edge=%f, val=%d, delta=%d\n", edge, val, delta);

  /* move back to y-coordinate origin */
  MOVE (delta, PRECISION_ON, NULL);

  /* head successfully set to the origin */
  return (1);
}


/* computes color offset and gain */
/* X is red
   Y is blue
   Z is green

   returns if OK, else 0
*/
static int
WarmUp (int color, int *gain)
{
  unsigned char buffer[5300];
  int i, val;
  int opsc02[9] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, -1 };
  int opsc04[9] = { 0x06, 0xF4, 0xFF, 0x81, 0x1B, 0x00, 0x00, 0x00, -1 };
  int opsc10[9] = { 0x06, 0xF4, 0xFF, 0x81, 0x1B, 0x00, 0x08, 0x00, -1 };
  int opsc18[17] =
    { 0x01, 0x00, 0x00, 0x70, 0x00, 0x00, 0x60, 0x2F, 0x2F, 0x00, 0x88, 0x08,
    0x00, 0x80, 0xA4, 0x00, -1
  };
  int opsc38[37] =
    { 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C, 0x00, 0x04, 0x40, 0x01,
    0x00, 0x00, 0x04, 0x00, 0x6E, 0x18, 0x10, 0x03, 0x06, 0x00, 0x00, 0x00,
    0x46, 0xA0, 0x00, 0x8B, 0x49, 0x2A, 0xE9, 0x68, 0xDF, 0x13, 0x1A, 0x00,
    -1
  };
  int opsc39[37] =
    { 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C, 0x00, 0x04, 0x40, 0x01,
    0x00, 0x00, 0x04, 0x00, 0x6E, 0x41, 0x20, 0x24, 0x06, 0x00, 0x00, 0x00,
    0x46, 0xA0, 0x00, 0x8B, 0x49, 0x2A, 0xE9, 0x68, 0xDF, 0x13, 0x1A, 0x00,
    -1
  };
  int opsc40[37] =
    { 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C, 0x00, 0x04, 0x40, 0x01,
    0x00, 0x00, 0x04, 0x00, 0x6E, 0x41, 0x60, 0x4F, 0x06, 0x00, 0x00, 0x00,
    0x46, 0xA0, 0x00, 0x8B, 0x49, 0x2A, 0xE9, 0x68, 0xDF, 0x93, 0x1A, 0x00,
    -1
  };
  int opsc51[17] =
    { 0x09, 0x00, 0x00, 0x70, 0x00, 0x00, 0x60, 0x2F, 0x2F, 0x00, 0xA5, 0x09,
    0x00, 0x40, 0xA4, 0x00, -1
  };
  int opsc48[17] =
    { 0x09, 0x00, 0x00, 0x70, 0x00, 0x00, 0x60, 0x2F, 0x2F, 0x00, 0x00, 0x00,
    0x00, 0x40, 0xA4, 0x00, -1
  };
  float offsetX, offsetY, offsetZ;
  float avgX, avgY, avgZ;


  /* really dirty hack: somethig is buggy in BW mode    */
  /* we override mode with color until the bug is found */
  color = RGB_MODE;

  /* 1600P have a different CCD command block */
  if (sanei_umax_pp_getastra () == 1600)
    {
      opsc04[0] = 0x19;
      opsc04[1] = 0xD5;
      opsc04[4] = 0x1B;
      opsc04[7] = 0x20;

      opsc10[0] = 0x19;
      opsc10[1] = 0xD5;
      opsc10[4] = 0x1B;
      opsc10[7] = 0x20;

      opsc48[8] = 0x2B;
      opsc48[11] = 0x20;
      opsc48[12] = 0x08;
      opsc48[13] = 0x42;
    }

  /* block that repeats : another kind of move .... */
  /* scan one line of 24 bytes at the right/left    */
  /* maybe black calibration since it is out of the */
  /* scanning area                                  */
  /* color calibration: offset detection            */
  /* scan 24 pixels wide, 1 pixel height at 600 dpi */
  /* without moving the head. Pixels are 'under the */
  /* roof', so should be black                      */
  if (color >= RGB_MODE)
    {
      CMDSETGET (2, 0x10, opsc48);
      CMDSETGET (8, 0x24, opsc38);
      CMDSETGET (1, 0x08, opsc04);	/* scan std, no 'enhancing' */
      CMDSYNC (0xC2);
      if (sanei_umax_pp_ScannerStatus () & 0x80)
	{
	  CMDSYNC (0x00);
	}
      CMDSETGET (4, 0x08, opsc02);	/* commit ? */
      COMPLETIONWAIT;
      CMDGETBUF (4, 0x18, buffer);
      if (DBG_LEVEL >= 128)
	Dump (0x18, buffer, NULL);
      val = 0;
      for (i = 0; i < 24; i++)
	val += buffer[i];
      offsetX = (float) val / i;


      CMDSYNC (0x00);
      opsc04[7] = opsc04[7] | 0x10;	/* enhanced ? */
      CMDSETGET (1, 0x08, opsc04);
      COMPLETIONWAIT;
      CMDGETBUF (4, 0x18, buffer);
      val = 0;
      for (i = 0; i < 24; i++)
	val += buffer[i];
      offsetX += (float) val / i;
      if (DBG_LEVEL >= 128)
	Dump (0x18, buffer, NULL);

      /* block that repeats */
      /* must be monochrome since hscan=1 */
      opsc48[0] = 0x01;
      if (sanei_umax_pp_getastra () == 1600)
	{
	  opsc48[12] = 0x0C;
	  opsc48[13] = 0x82;
	}
      else
	{
	  opsc48[12] = 0x04;
	  opsc48[13] = 0x80;
	}
      CMDSETGET (2, 0x10, opsc48);
      CMDSETGET (8, 0x24, opsc38);
      opsc04[7] = opsc04[7] & 0x20;
      CMDSETGET (1, 0x08, opsc04);
      CMDSYNC (0xC2);
      if (sanei_umax_pp_ScannerStatus () & 0x80)
	{
	  CMDSYNC (0x00);
	}
      CMDSETGET (4, 0x08, opsc02);
      COMPLETIONWAIT;
      CMDGETBUF (4, 0x18, buffer);
      if (DBG_LEVEL >= 128)
	Dump (0x18, buffer, NULL);
      val = 0;
      for (i = 0; i < 24; i++)
	val += buffer[i];
      offsetY = (float) val / i;

      CMDSYNC (0x00);
      opsc04[7] = opsc04[7] | 0x10;	/* brightness ? */
      CMDSETGET (1, 0x08, opsc04);
      COMPLETIONWAIT;
      CMDGETBUF (4, 0x18, buffer);
      if (DBG_LEVEL >= 128)
	Dump (0x18, buffer, NULL);
      val = 0;
      for (i = 0; i < 24; i++)
	val += buffer[i];
      offsetY += (float) val / i;
    }

  /* block that repeats */
  if (color < RGB_MODE)
    {
      opsc48[0] = 0x05;		/* B&H height is 5 */
      opsc48[13] = 0xC0;	/* B&W mode */
    }
  else
    {
      opsc48[0] = 0x05;		/* color height is 5 (+4 ?) */
      opsc48[13] = 0xC1;	/* some strange mode ? */
    }
  if (sanei_umax_pp_getastra () == 1600)
    opsc48[13] = opsc48[13] | 0x02;
  CMDSETGET (2, 0x10, opsc48);
  CMDSETGET (8, 0x24, opsc38);
  opsc04[7] = opsc04[7] & 0x20;
  CMDSETGET (1, 0x08, opsc04);
  CMDSYNC (0xC2);
  if (sanei_umax_pp_ScannerStatus () & 0x80)
    {
      CMDSYNC (0x00);
    }
  CMDSETGET (4, 0x08, opsc02);
  COMPLETIONWAIT;
  CMDGETBUF (4, 0x18, buffer);
  if (DBG_LEVEL >= 128)
    Dump (0x18, buffer, NULL);
  val = 0;
  for (i = 0; i < 24; i++)
    val += buffer[i];
  offsetZ = (float) val / i;

  CMDSYNC (0x00);
  opsc04[7] = opsc04[7] | 0x10;
  CMDSETGET (1, 0x08, opsc04);
  COMPLETIONWAIT;
  CMDGETBUF (4, 0x18, buffer);
  if (DBG_LEVEL >= 128)
    Dump (0x18, buffer, NULL);
  val = 0;
  for (i = 0; i < 24; i++)
    val += buffer[i];
  offsetZ += (float) val / i;


	/***********************/
  /* auto gain computing */
	/***********************/

  /* color correction set to 63 06 */
  /* for a start                   */
  *gain = 0x636;
  CMDSETGET (2, 0x10, opsc18);
  CMDSETGET (8, 0x24, opsc39);
  opsc04[7] = opsc04[7] & 0x20;
  opsc04[6] = 0x06;		/* one channel gain value */
  CMDSETGET (1, 0x08, opsc10);	/* was opsc04, extraneaous string */
  /* that prevents using Move .... */
  CMDSYNC (0xC2);
  CMDSYNC (0x00);
  CMDSETGET (4, 0x08, opsc02);
  COMPLETIONWAIT;
  CMDGETBUF (4, 0x200, buffer);
  if (DBG_LEVEL >= 128)
    Dump (0x200, buffer, NULL);


  /* auto correction of gain levels */
  /* first color component X        */
  opsc51[10] = (*gain) / 16;	/* channel 1 & 2 gain */
  opsc51[11] = (*gain) % 16;	/* channel 3 gain     */
  if (color >= RGB_MODE)
    {
      if (sanei_umax_pp_getastra () == 1600)
	{
	  opsc51[11] |= 0x20;
	  opsc51[12] = 0x08;
	  opsc51[13] |= 0x02;

	  opsc04[7] |= 0x20;
	}
      CMDSETGET (2, 0x10, opsc51);
      CMDSETGET (8, 0x24, opsc40);
      if (DBG_LEVEL >= 128)
	{
	  Bloc2Decode (opsc51);
	  Bloc8Decode (opsc40);
	}
      opsc04[6] = (*gain) / 256;
      CMDSETGET (1, 0x08, opsc04);
      CMDSYNC (0xC2);
      CMDSYNC (0x00);
      CMDSETGET (4, 0x08, opsc02);
      COMPLETIONWAIT;
      CMDGETBUF (4, 0x14B4, buffer);
      if (DBG_LEVEL >= 128)
	Dump (0x14B4, buffer, NULL);
      avgX = 0;
      for (i = 0; i < 0x14B4; i++)
	avgX += buffer[i];
      avgX = avgX / i;
      DBG (64, "Somme(X:%02X)=%d, moyenne=%8.4f(%f)\n", opsc04[6],
	   (int) (avgX * i), avgX, avgX - offsetX);
      while ((opsc04[6] < 0x0F) && (avgX - offsetX < 180.0))
	{
	  CMDSYNC (0x00);
	  opsc04[6]++;
	  CMDSETGET (1, 0x000008, opsc04);
	  COMPLETIONWAIT;
	  CMDGETBUF (4, 0x0014B4, buffer);
	  if (DBG_LEVEL >= 128)
	    Dump (0x14B4, buffer, NULL);
	  avgX = 0;
	  for (i = 0; i < 0x14B4; i++)
	    avgX += buffer[i];
	  avgX = avgX / i;
	  DBG (16, "Somme(X:%02X)=%d, moyenne=%8.4f(%f)\n", opsc04[6],
	       (int) (avgX * i), avgX, avgX - offsetX);
	}

      *gain = (*gain & 0xFF) + 256 * (opsc04[6] - 1);
      opsc51[10] = *gain / 16;	/* channel 1 & 2 gain */
      opsc51[11] = *gain % 16;	/* channel 3 gain     */
      opsc51[0] = 0x01;
      opsc51[13] = 0x80;
      if (sanei_umax_pp_getastra () == 1600)
	{
	  opsc51[11] |= 0x20;
	  opsc51[12] = 0x08;
	  opsc51[13] |= 0x02;

	  opsc04[7] |= 0x20;
	}
      CMDSETGET (2, 0x10, opsc51);
      CMDSETGET (8, 0x24, opsc40);
      opsc04[6] = (*gain & 0x00F);
      CMDSETGET (1, 0x08, opsc04);
      CMDSYNC (0xC2);
      CMDSYNC (0x00);
      CMDSETGET (4, 0x08, opsc02);
      COMPLETIONWAIT;
      CMDGETBUF (4, 0x14B4, buffer);
      if (DBG_LEVEL >= 128)
	Dump (0x14B4, buffer, NULL);
      avgY = 0;
      for (i = 0; i < 0x14B4; i++)
	avgY += buffer[i];
      avgY = avgY / i;
      DBG (64, "Somme(Y:%02X)=%d, moyenne=%8.4f(%f)\n", opsc04[6],
	   (int) (avgY * i), avgY, avgY - offsetY);

      while ((opsc04[6] < 0x0F) && (avgY - offsetY < 180.0))
	{
	  CMDSYNC (0x00);
	  opsc04[6]++;
	  CMDSETGET (1, 0x000008, opsc04);
	  COMPLETIONWAIT;
	  CMDGETBUF (4, 0x0014B4, buffer);
	  if (DBG_LEVEL >= 128)
	    Dump (0x14B4, buffer, NULL);
	  avgY = 0;
	  for (i = 0; i < 0x14B4; i++)
	    avgY += buffer[i];
	  avgY = avgY / i;
	  DBG (64, "Somme(Y:%02X)=%d, moyenne=%8.4f(%f)\n", opsc04[6],
	       (int) (avgY * i), avgY, avgY - offsetY);
	}
      *gain = (*gain & 0xFF0) + (opsc04[6] - 1);
    }





  /* component Z: B&W component */
  opsc51[10] = *gain / 16;	/* channel 1 & 2 gain */
  opsc51[11] = *gain % 16;	/* channel 3 gain     */
  if (color < RGB_MODE)
    opsc51[0] = 0x01;		/* in BW, scan zone doesn't have an extra 4 points */
  else
    opsc51[0] = 0x05;		/*  extra 4 points */
  opsc51[13] = 0xC0;		/* B&W */
  if (sanei_umax_pp_getastra () == 1600)
    {
      opsc51[11] |= 0x20;
      opsc51[12] = 0x08;
      opsc51[13] |= 0x02;

      opsc04[7] |= 0x20;
    }
  CMDSETGET (2, 0x10, opsc51);
  if (DBG_LEVEL >= 128)
    {
      Bloc2Decode (opsc51);
    }
  CMDSETGET (8, 0x24, opsc40);
  opsc04[6] = (*gain & 0x0F0) / 16;
  CMDSETGET (1, 0x08, opsc04);
  CMDSYNC (0xC2);
  CMDSYNC (0x00);
  CMDSETGET (4, 0x08, opsc02);
  COMPLETIONWAIT;
  CMDGETBUF (4, 0x14B4, buffer);
  if (DBG_LEVEL >= 128)
    Dump (0x14B4, buffer, NULL);
  avgZ = 0;
  for (i = 0; i < 0x14B4; i++)
    avgZ += buffer[i];
  avgZ = avgZ / i;
  DBG (64, "Somme(Z:%02X)=%d, moyenne=%8.4f(%f)\n", opsc04[6],
       (int) (avgZ * i), avgZ, avgZ - offsetZ);
  while ((opsc04[6] < 0x07) && (avgZ - offsetZ < 180.0))
    {
      CMDSYNC (0x00);
      opsc04[6]++;
      CMDSETGET (1, 0x08, opsc04);
      COMPLETIONWAIT;
      CMDGETBUF (4, 0x0014B4, buffer);
      if (DBG_LEVEL >= 128)
	Dump (0x14B4, buffer, NULL);
      avgZ = 0;
      for (i = 0; i < 0x14B4; i++)
	avgZ += buffer[i];
      avgZ = avgZ / i;
      DBG (64, "Somme(Z:%02X)=%d, moyenne=%8.4f(%f)\n", opsc04[6],
	   (int) (avgZ * i), avgZ, avgZ - offsetZ);
    }
  *gain = (*gain & 0xF0F) + opsc04[6] * 16;
  DBG (1, "Warm-up done ...\n");
  return (1);
}

/* park head: returns 1 on success, 0 otherwise  */
/* distance is in 75 dpi unit                    */
int
sanei_umax_pp_Park (void)
{
  int header[17] =
    { 0x01, 0x00, 0x01, 0x70, 0x00, 0x00, 0x60, 0x2F, 0x13, 0x05, 0x00, 0x00,
    0x00, 0x80, 0xF0, 0x00, -1
  };
  int body[37] =
    { 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C, 0x00, 0x03, 0xC1, 0x80,
    0x00, 0x00, 0x04, 0x00, 0x16, 0x80, 0x15, 0x78, 0x03, 0x03, 0x00, 0x00,
    0x46, 0xA0, 0x00, 0x8B, 0x49, 0x2A, 0xE9, 0x68, 0xDF, 0x1B, 0x1A, 0x00,
    -1
  };
  int status = 0x90;

  CMDSYNC (0x00);

  CMDSETGET (0x02, 16, header);
  if (DBG_LEVEL >= 32)
    Bloc8Decode (body);
  CMDSETGET (0x08, 36, body);
  CMDSYNC (0x40);

  status = sanei_umax_pp_ScannerStatus ();
  DBG (16, "PARKING STATUS is 0x%02X (%s:%d)\n", status, __FILE__, __LINE__);
  DBG (1, "Park command issued ...\n");
  return (1);
}


/* calibrates CCD: returns 1 on success, 0 on failure */
static int
GammaCalibration (int color, int dpi, int gain, int highlight, int width,
		  int *calibration)
{
  int opsc32[17] =
    { 0x4A, 0x00, 0x00, 0x70, 0x00, 0x00, 0x60, 0x00, 0x17, 0x05, 0xA5, 0x08,
    0x00, 0x00, 0xAC, 0x00, -1
  };
  int opsc41[37] =
    { 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C, 0x00, 0x04, 0x40, 0x01,
    0x00, 0x00, 0x04, 0x00, 0x6E, 0x90, 0xD0, 0x47, 0x06, 0x00, 0x00, 0xC4,
    0x5C, 0xA0, 0x00, 0x8B, 0x49, 0x2A, 0xE9, 0x68, 0xDF, 0x93, 0x1B, 0x00,
    -1
  };
  int opscnb[37] =
    { 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C, 0x00, 0x04, 0x40, 0x01,
    0x00, 0x00, 0x04, 0x00, 0x6E, 0x90, 0xD0, 0x47, 0x06, 0x00, 0x00, 0xEC,
    0x54, 0xA0, 0x00, 0x8B, 0x49, 0x2A, 0xE9, 0x68, 0xDF, 0x93, 0x1A, 0x00,
    -1
  };
  int opsc04[9] = { 0x06, 0xF4, 0xFF, 0x81, 0x1B, 0x00, 0x00, 0x00, -1 };
  int opsc02[9] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, -1 };
  int size;
  unsigned char buffer[0x105798];

  /* 1600P have a different CCD command block */
  if (sanei_umax_pp_getastra () == 1600)
    {
      opsc04[0] = 0x19;
      opsc04[1] = 0xD5;
      opsc04[4] = 0x1B;

      opsc41[29] = 0x1A;
      opsc41[30] = 0xEE;
    }

  /* step back by 66 ticks:                     */
  /* since we're going to scan 66 lines of data */
  /* which are going to be used as calibration  */
  /* data                                       */
  /* we are on the white area just before       */
  /* the user scan area                         */
  MOVE (-67, PRECISION_ON, NULL);


  CMDSYNC (0x00);

  /* get calibration data */
  opsc32[10] = gain / 16;
  opsc32[11] = ((highlight / 16) & 0xF0) | (gain % 16);
  opsc32[12] = highlight % 256;
  if (sanei_umax_pp_getastra () == 1600)
    {
      opsc32[13] = 0x03;
    }


  if (color < RGB_MODE)
    {
      opsc32[0] -= 4;
      opsc32[13] = 0xC0;
    }
  CMDSETGET (2, 0x10, opsc32);
  if (DBG_LEVEL >= 64)
    {
      Bloc2Decode (opsc32);
    }
  if (color < RGB_MODE)
    {
      CMDSETGET (8, 0x24, opscnb);
      if (DBG_LEVEL >= 64)
	{
	  Bloc8Decode (opscnb);
	}
      opsc04[6] = 0x85;
    }
  else
    {
      CMDSETGET (8, 0x24, opsc41);
      if (DBG_LEVEL >= 64)
	{
	  Bloc8Decode (opsc41);
	}
      opsc04[6] = 0x0F;
      if (sanei_umax_pp_getastra () == 1600)
	opsc04[7] = 0xC0;
      else
	opsc04[7] = 0x70;
    }

  /* BUG BW noisy here */
  CMDSETGET (1, 0x08, opsc04);
  CMDSYNC (0xC2);
  CMDSYNC (0x00);
  CMDSETGET (4, 0x08, opsc02);	/* opsc03 hangs it */
  COMPLETIONWAIT;

  opsc04[0] = 0x06;
  if (color >= RGB_MODE)
    size = 3 * 5100 * 70;
  else
    size = 5100 * 66;
  if (GetEPPMode () == 32)
    {
      CmdGetBuffer32 (4, size, buffer);
    }
  else
    {
      CMDGETBUF (4, size, buffer);
    }
  if (DBG_LEVEL >= 128)
    {
      Dump (size, buffer, NULL);
      if (color >= RGB_MODE)
	{
	  DumpRVB (5100, 70, buffer, NULL);
	}
      else
	{
	  DumpNB (5100, 66, buffer, NULL);
	}
    }
  ComputeCalibrationData (color, dpi, width, buffer, calibration);
  DBG (1, "Gamma calibration done ...\n");
  return (1);
}


/* returns number of bytes read or 0 on failure */
int
sanei_umax_pp_ReadBlock (long len, int window, int dpi, int last,
			 unsigned char *buffer)
{
  DBG (8, "ReadBlock(%ld,%d,%d,%d)\n", len, window, dpi, last);
  /* EPP block reading is available only when dpi >=600 */
  if (dpi >= 600)
    {
      DBG (8, "CmdGetBlockBuffer(4,%ld,%d);\n", len, window);
      len = CmdGetBlockBuffer (4, len, window, buffer);
      if (len == 0)
	{
	  DBG (0, "CmdGetBlockBuffer(4,%ld,%d) failed (%s:%d)\n", len, window,
	       __FILE__, __LINE__);
	  gCancel = 1;
	}
    }
  else
    {
      DBG (8, "CmdGetBuffer(4,%ld);\n", len);
      if (CmdGetBuffer (4, len, buffer) != 1)
	{
	  DBG (0, "CmdGetBuffer(4,%ld) failed (%s:%d)\n", len, __FILE__,
	       __LINE__);
	  gCancel = 1;
	}
    }
  if (!last)
    {
      /* sync with scanner */
      if (sanei_umax_pp_CmdSync (0xC2) == 0)
	{
	  DBG (0, "Warning CmdSync(0xC2) failed! (%s:%d)\n", __FILE__,
	       __LINE__);
	  DBG (0, "Trying again ... ");
	  if (sanei_umax_pp_CmdSync (0xC2) == 0)
	    {
	      DBG (0, " failed again! (%s:%d)\n", __FILE__, __LINE__);
	      DBG (0, "Aborting ...\n");
	      gCancel = 1;
	    }
	  else
	    DBG (0, " success ...\n");

	}
    }
  return (len);
}

int
sanei_umax_pp_Scan (int x, int y, int width, int height, int dpi, int color,
		    int gain, int highlight)
{
  struct timeval td, tf;
  unsigned char *buffer;
  long int somme, len, read, blocksize;
  float elapsed;
  FILE *fout = NULL;
  int *dest = NULL;
  int bpl, hp;
  int th, tw, bpp;
  int distance = 0, nb;
  int bx, by;

  if (sanei_umax_pp_StartScan
      (x, y, width, height, dpi, color, gain, highlight, &bpp, &tw, &th) == 1)
    {
      COMPLETIONWAIT;

      /* blocksize must be multiple of the number of bytes per line */
      /* max is 2096100                                             */
      /* so blocksize will hold a round number of lines, easing the */
      /* write data to file operation                               */
      /*blocksize=(2096100/bpl)*bpl; */
      bpl = bpp * tw;
      hp = 2096100 / bpl;
      hp = 16776960 / bpl;
      blocksize = hp * bpl;
      nb = 0;
      read = 0;

      /* get scanned data */
      somme = bpp * tw * th;
      DBG (8, "Getting buffer %d*%d*%d=%ld=0x%lX    (%s:%d)  \n", bpp, tw, th,
	   somme, somme, __FILE__, __LINE__);

      /* allocate memory */
      buffer = (unsigned char *) malloc (blocksize);
      if (buffer == NULL)
	{
	  DBG (0, "Failed to allocate %ld bytes, giving up....\n", blocksize);
	  DBG (0, "Try to scan at lower resolution, or a smaller area.\n");
	  gCancel = 1;
	}

      /* open output file */
      fout = fopen ("out.pnm", "wb");
      if (fout == NULL)
	{
	  DBG (0, "Failed to open 'out.pnm', giving up....\n");
	  gCancel = 1;
	}
      else
	{
	  /* write pnm header */
	  if (color >= RGB_MODE)
	    fprintf (fout, "P6\n%d %d\n255\n", tw, th);
	  else
	    fprintf (fout, "P5\n%d %d\n255\n", tw, th);
	}

      /* data reading loop */
      gettimeofday (&td, NULL);
      while ((read < somme) && (!gCancel))
	{
	  /* 2096100 max */
	  if (somme - read > blocksize)
	    len = blocksize;
	  else
	    len = somme - read;
	  len =
	    sanei_umax_pp_ReadBlock (len, tw, dpi, (len < blocksize), buffer);
	  if (len == 0)
	    {
	      DBG (0, "ReadBlock failed, cancelling scan ...\n");
	      gCancel = 1;
	    }

	  read += len;
	  nb++;
	  DBG (8, "Read %ld bytes out of %ld ...\n", read, somme);
	  DBG (8, "Read %d blocks ... \n", nb);


	  /* write partial buffer to file */
	  if (len)
	    {
	      if (color >= RGB_MODE)
		{
		  /* using an image format that doesn't need   */
		  /* reordering would speed up write operation */
		  hp = len / bpl;
		  if (sanei_umax_pp_getastra () != 1600)
		    {
		      for (by = 0; by < hp; by++)
			{
			  for (bx = 0; bx < tw; bx++)
			    {
			      fputc (buffer[3 * by * tw + 2 * tw + bx], fout);
			      fputc (buffer[3 * by * tw + tw + bx], fout);
			      fputc (buffer[3 * by * tw + bx], fout);
			    }
			}
		    }
		  else
		    {
		      for (by = 0; by < hp; by++)
			{
			  for (bx = 0; bx < tw; bx++)
			    {
			      fputc (buffer[3 * by * tw + 2 * tw + bx], fout);
			      fputc (buffer[3 * by * tw + bx], fout);
			      fputc (buffer[3 * by * tw + tw + bx], fout);
			    }
			}
		    }
		}
	      else
		fwrite (buffer, len, 1, fout);
	    }
	}

      gettimeofday (&tf, NULL);

      /* release ressources */
      if (fout != NULL)
	fclose (fout);
      free (dest);
      free (buffer);

      /* scan time are high enough to forget about usec */
      elapsed = tf.tv_sec - td.tv_sec;
      DBG (8, "%ld bytes transfered in %f seconds ( %.2f Kb/s)\n", somme,
	   elapsed, (somme / elapsed) / 1024.0);


      /* park head */
      distance = distance + y + height;
    }				/* if start scan OK */
  else
    {
      DBG (0, "StartScan failed..... \n");
    }

  /* terminate any pending command */
  if (sanei_umax_pp_CmdSync (0x00) == 0)
    {
      DBG (0, "Warning CmdSync(0x00) failed! (%s:%d)\n", __FILE__, __LINE__);
      DBG (0, "Trying again ... ");
      if (sanei_umax_pp_CmdSync (0x00) == 0)
	{
	  DBG (0, " failed again! (%s:%d)\n", __FILE__, __LINE__);
	  DBG (0, "Blindly going on ...\n");
	}
      else
	DBG (0, " success ...\n");

    }

  /* parking */
  if (sanei_umax_pp_Park () == 0)
    DBG (0, "Park failed !!! (%s:%d)\n", __FILE__, __LINE__);


  /* end ... */
  DBG (1, "Scan done ...\n");
  return (1);
}






int
sanei_umax_pp_ParkWait (void)
{
  int status;

  /* check if head is at home */
  do
    {
      sleep (2);
      sanei_umax_pp_CmdSync (0x40);
      status = sanei_umax_pp_ScannerStatus ();
    }
  while ((status & MOTOR_BIT) == 0x00);
  DBG (1, "ParkWait done ...\n");
  return (1);
}






/* starts scan: return 1 on success */
int
sanei_umax_pp_StartScan (int x, int y, int width, int height, int dpi,
			 int color, int gain, int highlight, int *rbpp,
			 int *rtw, int *rth)
{
  unsigned char *buffer;
  int *dest = NULL;
  int state[16];
  int err = 0;
  int calibration[3 * 5100 + 768 + 2 + 1];
  int xdpi, ydpi, bpl, h;
  int th, tw, bpp;
  int distance, i;

  int opsc04[9] = { 0x06, 0xF4, 0xFF, 0x81, 0x1B, 0x00, 0x00, 0x00, -1 };
  int opsc53[17] =
    { 0xA4, 0x80, 0x07, 0x50, 0xEC, 0x03, 0x00, 0x2F, 0x17, 0x07, 0x84, 0x08,
    0x00, 0x00, 0xAC, 0x00, -1
  };
  int opsc35[37] =
    { 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C, 0x00, 0x03, 0xC1, 0x80,
    0x00, 0x00, 0x04, 0x00, 0x16, 0x41, 0xE0, 0xAC, 0x03, 0x03, 0x00, 0x00,
    0x46, 0xA0, 0x00, 0x8B, 0x49, 0x2A, 0xE9, 0x68, 0xDF, 0x13, 0x1A, 0x00,
    -1
  };
  int opscan[37] =
    { 0x00, 0x00, 0xB0, 0x4F, 0xD8, 0xE7, 0xFA, 0x10, 0xEF, 0xC4, 0x3C, 0x71,
    0x0F, 0x00, 0x04, 0x00, 0x6E, 0x61, 0xA1, 0x24, 0xC4, 0x7E, 0x00, 0xAE,
    0x41, 0xA0, 0x0A, 0x8B, 0x49, 0x2A, 0xE9, 0x68, 0xDF, 0x33, 0x1A, 0x00,
    -1
  };

  DBG (8, "StartScan(%d,%d,%d,%d,%d,%d,%X);\n", x, y, width, height, dpi,
       color, gain);
  buffer = (unsigned char *) malloc (2096100);
  if (buffer == NULL)
    {
      DBG (0, "Failed to allocate 2096100 bytes... (%s:%d)\n", __FILE__,
	   __LINE__);
      return (0);
    }

  /* 1600P have a different CCD command block */
  if (sanei_umax_pp_getastra () == 1600)
    {
      opsc04[0] = 0x19;
      opsc04[1] = 0xD5;
      opsc04[4] = 0x1B;
      opsc04[7] = 0x70;

      opscan[29] = 0x1A;
      opscan[30] = 0xEE;

      opsc35[29] = 0x1A;
      opsc35[30] = 0xEE;

      opsc53[13] = 0x03;	/* may be blur filter */
    }

  /* get scanner status */
  if (DBG_LEVEL > 8)
    {
      char str[64];
      CMDGET (0x02, 16, state);
      for (i = 0; i < 16; i++)
	sprintf (str + 3 * i, "%02X ", state[i]);
      str[48] = 0x00;
      DBG (8, "SCANNER STATE=%s\n", str);
    }

  dest = (int *) malloc (65536 * 4);
  if (sanei_umax_pp_getastra () != 1600)
    {
      CMDSETGET (0x08, 36, opsc35);
      CMDSYNC (0xC2);

      if (dest == NULL)
	{
	  DBG (0, "%s:%d failed to allocate 256 Ko !\n", __FILE__, __LINE__);
	  return (0);
	}

      /* init some buffer : default calibration data ? */
      dest[0] = 0x00;
      dest[1] = 0x00;
      dest[2] = 0x00;
      for (i = 0; i < 768; i++)
	dest[i + 3] = i % 256;
      dest[768 + 3] = 0xAA;
      dest[768 + 4] = 0xAA;
      dest[768 + 5] = -1;
      CMDSETGET (0x04, 768 + 5, dest);


      /* check buffer returned */
      for (i = 0; i < 768; i++)
	{
	  if (dest[i + 3] != (i % 256))
	    {
	      DBG
		(0,
		 "Error data altered: byte %d=0x%02X, should be 0x%02X !    (%s:%d)\n",
		 i, dest[i + 3], i % 256, __FILE__, __LINE__);
	      err = 1;
	    }
	}
      if (err)
	return (0);
    }


  /* new part of buffer ... */
  for (i = 0; i < 256; i++)
    {
      dest[i * 2] = i;
      dest[i * 2 + 1] = 0x00;
    }
  CMDSETGET (0x08, 36, opsc35);
  CMDSYNC (0xC2);
  CMDSET (0x04, 512, dest);

  /* another new part ... */
  for (i = 0; i < 256; i++)
    {
      dest[i * 2] = i;
      dest[i * 2 + 1] = 0x04;	/* instead of 0x00 */
    }
  opsc35[2] = 0x06;		/* instead of 0x04, write flag ? */
  CMDSETGET (0x08, 36, opsc35);
  CMDSYNC (0xC2);
  CMDSET (0x04, 512, dest);

  opsc35[2] = 0x04;		/* return to initial value, read flag? */
  CMDSETGET (0x08, 36, opsc35);
  CMDGET (0x04, 512, dest);

  /* check buffer returned */
  /* if 0x4 are still 0x0 (hum..), we got a 1220P, else it is a 2000P */
  for (i = 0; i < 256; i++)
    {
      if ((dest[2 * i] != i)
	  || ((dest[2 * i + 1] != 0x04) && (dest[2 * i + 1] != 0x00)))
	{
	  DBG
	    (0,
	     "Error data altered: expected %d=(0x%02X,0x04), found (0x%02X,0x%02X) !    (%s:%d)\n",
	     i, i, dest[i * 2], dest[i * 2 + 1], __FILE__, __LINE__);
	  err = 1;
	}
    }
  if (err)
    return (0);

  /* find and move to zero */
  if (MoveToOrigin () == 0)
    {
      DBG (0, "MoveToOrign() failed ... (%s:%d)\n", __FILE__, __LINE__);
    }
  else
    {
      DBG (16, "MoveToOrign() passed ... (%s:%d)\n", __FILE__, __LINE__);
    }

  /* 1600P have a different CCD command block */
  if (sanei_umax_pp_getastra () == 1600)
    {
      opsc04[0] = 0x19;
      opsc04[1] = 0xD5;
      opsc04[4] = 0x1B;
      opsc04[7] = 0x70;

      opscan[29] = 0x1A;
      opscan[30] = 0xEE;

      opsc35[29] = 0x1A;
      opsc35[30] = 0xEE;

      opsc53[13] = 0x03;	/* may be blur filter */
    }


  /* adjust gain and color offset */
  /* red*256+green*16+blue        */
  if (gain == 0x0)
    {
      if (WarmUp (color, &gain) == 0)
	{
	  DBG (0, "Warm-up failed !!! (%s:%d)\n", __FILE__, __LINE__);
	  return (0);
	}
    }

  /* x dpi is from 75 to 600 max, any modes */
  if (dpi > 600)
    xdpi = 600;
  else
    xdpi = dpi;


  /* havent't yet found a way to make EPPRead32Buffer work */
  /* with length not multiple of four bytes, so we enlarge */
  /* width to meet this criteria ...                       */
  if ((GetEPPMode () == 32) && (xdpi >= 600) && (width & 0x03))
    {
      width += (4 - (width & 0x03));
      /* in case we go too far on the right */
      if (x + width > 5100)
	{
	  x = 5100 - width;
	}
    }

  /* compute target size */
  th = (height * dpi) / 600;
  tw = (width * xdpi) / 600;

  /* do gamma calibration */
  if (GammaCalibration (color, dpi, gain, highlight, width, calibration) == 0)
    {
      DBG (0, "Gamma calibration failed !!! (%s:%d)\n", __FILE__, __LINE__);
      return (0);
    }
  TRACE (16, "GammaCalibration() passed ...")
    /* it is faster to move at low resolution, then scan */
    /* than scan & move at high resolution               */
    distance = 0;

  /* work around a bug which has yet to be solved */
  y += 8;

  /* move fast to scan target if possible */
  if ((dpi > 150) && (y > 100))
    {
      distance = y;

      /* move at 150 dpi resolution */
      Move (y / 2, PRECISION_OFF, NULL);

      /* keep the remainder for scan */
      y = y % 4;
    }

  /* build final scan command */

  /* round width and height */
  width = (tw * 600) / xdpi;
  height = (th * 600) / dpi;

  ydpi = dpi;
  if (color >= RGB_MODE)
    {
      if (dpi < 150)
	ydpi = 150;
    }
  else
    {
      if (dpi < 300)
	ydpi = 300;
    }

  if (color >= RGB_MODE)
    {
      h = ((height * ydpi) / 600) + 8;
      bpp = 3;
    }
  else
    {
      h = ((height * ydpi) / 600) + 4;
      bpp = 1;
    }
  bpl = (bpp * width * xdpi) / 600;

  /* sets y resolution */
  switch (ydpi)
    {
    case 1200:
      opsc53[6] = 0x60;
      opsc53[8] = 0x5E;		/* old value */
      opsc53[8] = 0x5C;		/* new working value */
      opsc53[9] = 0x05;
      opsc53[14] = opsc53[14] & 0xF0;
      /*opsc53[14] = (opsc53[14] & 0xF0) | 0x04;	 -> 600 dpi ? */
      break;

    case 600:
      opsc53[6] = 0x60;
      opsc53[8] = 0x2F;		/* old value */
      opsc53[8] = 0x2E;		/* new working value */
      opsc53[9] = 0x05;
      opsc53[14] = (opsc53[14] & 0xF0) | 0x04;
      break;

    case 300:
      opsc53[6] = 0x00;
      opsc53[8] = 0x17;
      opsc53[9] = 0x05;
      opsc53[14] = (opsc53[14] & 0xF0) | 0x0C;

      /* si | 0C h=2*w, si | 04 h=w ? */

      break;

    case 150:
      opsc53[6] = 0x00;
      opsc53[8] = 0x17;
      opsc53[9] = 0x07;
      opsc53[14] = (opsc53[14] & 0xF0) | 0x0C;
      break;
    }

  /* channels gain */
  opsc53[10] = gain / 16;
  opsc53[11] = ((highlight / 16) & 0xF0) | (gain % 16);
  opsc53[12] = highlight % 256;

  /* scan height */
  opsc53[0] = h % 256;
  opsc53[1] = h / 256;

  /* y start -1 */
  y = (y * ydpi) / 600 - 1;
  if (y > 0)
    {
      opsc53[1] |= (y % 4) * 64;
      opsc53[2] = (y / 4) % 256;
      opsc53[3] = 0x50 | ((y >> 10) & 0x0F);
    }
  else
    {
      opsc53[2] = 0x00;
      opsc53[3] = 0x50;
    }

  /* x start -1 */
  opscan[17] = (x - 1) % 256;
  opscan[18] = (((x - 1) / 256) & 0x0F);
  opscan[33] = 0x33;
  if (x - 1 > 0x1000)
    opscan[33] |= 0x40;

  /* x end */
  opscan[18] |= (((x + width) % 16) * 16);
  opscan[19] = ((x + width) / 16) % 256;
  if (x + width > 0x1000)
    opscan[33] |= 0x80;

  /* bytes per line */
  opscan[24] = 0x41 + (bpl & 0x1FFF) / 256;
  opscan[23] = bpl % 256;


  if (color >= RGB_MODE)
    {
      opsc53[6] = 0x00;
      opsc53[7] = 0x2F;
      opsc04[6] = 0x8F;
      if (sanei_umax_pp_getastra () == 1600)
	{
	  opsc04[7] = 0x70;
	  opsc53[13] = 0x03;
	}
      else
	{
	  opsc04[7] = 0xA0;
	  opsc53[13] = 0x09;
	}
    }
  else
    {
      opsc53[6] = 0x60;
      opsc53[7] = 0x40;
      opsc53[13] = 0xC0;
      opsc04[6] = 0x80 | ((gain / 16) & 0x0F);
      if (sanei_umax_pp_getastra () == 1600)
	{
	  opsc04[7] = 0x20;
	  opsc53[13] = 0xC3;
	}
      else
	{
	  opsc04[7] = 0xA0;
	  opsc53[13] = 0xC9;
	}
    }


  CMDSYNC (0x00);
  /*opsc53[13] = 0x80;           B&W bit */
  /*opsc53[13] = 0x40;           green bit */
  /*opsc53[13] = 0x20;           red bit */
  /*opsc53[13] = 0x10;           blue bit */
  /* with cmd 01, may be use to do 3 pass scanning ? */
  /* bits 0 to 3 seem related to sharpness */


  CMDSETGET (2, 0x10, opsc53);
  CMDSETGET (8, 0x24, opscan);
  CMDSETGET (1, 0x08, opsc04);
  CMDSYNC (0xC2);
  CMDSET (4, 0x3EC6, calibration);
  COMPLETIONWAIT;


  *rbpp = bpp;
  *rtw = tw;
  *rth = th;

  free (buffer);
  return (1);
}

/* 
 * Check the scanner model. Return 1220 for
 * a 1220P, or 2000 for a 2000P.
 * and 610 for a 610P
 * values less than 610 are errors
 */
int
sanei_umax_pp_CheckModel (void)
{
  int *dest = NULL;
  int state[16];
  int err = 0;
  int i;

  int opsc35[37] =
    { 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C, 0x00, 0x03, 0xC1, 0x80,
    0x00, 0x00, 0x04, 0x00, 0x16, 0x41, 0xE0, 0xAC, 0x03, 0x03, 0x00, 0x00,
    0x46, 0xA0, 0x00, 0x8B, 0x49, 0x2A, 0xE9, 0x68, 0xDF, 0x13, 0x1A, 0x00,
    -1
  };

  /* if we have already detected a scanner different from */
  /* default type, no need to check again                 */
  if (sanei_umax_pp_getastra () != 1220)
    return sanei_umax_pp_getastra ();

  /* get scanner status */
  CMDGET (0x02, 16, state);
  CMDSETGET (0x08, 36, opsc35);
  CMDSYNC (0xC2);

  dest = (int *) malloc (65536 * 4);
  if (dest == NULL)
    {
      DBG (0, "%s:%d failed to allocate 256 Ko !\n", __FILE__, __LINE__);
      return (0);
    }

  /* init some buffer : default calibration data ? */
  dest[0] = 0x00;
  dest[1] = 0x00;
  dest[2] = 0x00;
  for (i = 0; i < 768; i++)
    dest[i + 3] = i % 256;
  dest[768 + 3] = 0xAA;
  dest[768 + 4] = 0xAA;
  dest[768 + 5] = -1;
  CMDSETGET (0x04, 768 + 5, dest);


  /* check buffer returned */
  for (i = 0; i < 768; i++)
    {
      if (dest[i + 3] != (i % 256))
	{
	  DBG
	    (0,
	     "Error data altered: byte %d=0x%02X, should be 0x%02X !    (%s:%d)\n",
	     i, dest[i + 3], i % 256, __FILE__, __LINE__);
	  err = 1;
	}
    }
  if (err)
    return (0);


  /* new part of buffer ... */
  for (i = 0; i < 256; i++)
    {
      dest[i * 2] = i;
      dest[i * 2 + 1] = 0x00;
    }
  CMDSETGET (0x08, 36, opsc35);
  CMDSYNC (0xC2);
  CMDSET (0x04, 512, dest);

  /* another new part ... */
  for (i = 0; i < 256; i++)
    {
      dest[i * 2] = i;
      dest[i * 2 + 1] = 0x04;	/* instead of 0x00 */
    }
  opsc35[2] = 0x06;		/* instead of 0x04, write flag ? */
  CMDSETGET (0x08, 36, opsc35);
  CMDSYNC (0xC2);
  CMDSET (0x04, 512, dest);

  opsc35[2] = 0x04;		/* return to initial value, read flag? */
  CMDSETGET (0x08, 36, opsc35);
  CMDGET (0x04, 512, dest);

  /* check buffer returned */
  /* if 0x4 are still 0x04, we got a 1220P, else it is a 2000P */
  for (i = 0; i < 256; i++)
    {
      if ((dest[2 * i] != i)
	  || ((dest[2 * i + 1] != 0x04) && (dest[2 * i + 1] != 0x00)))
	{
	  DBG
	    (0,
	     "Error data altered: expected %d=(0x%02X,0x04), found (0x%02X,0x%02X) !    (%s:%d)\n",
	     i, i, dest[i * 2], dest[i * 2 + 1], __FILE__, __LINE__);
	  err = 0;
	}
    }

  /* if buffer unchanged, we have a 1600P, or a 1220P */
  /* if data has turned into 0, we have a 2000P       */
  if (dest[1] == 0x00)
    {
      sanei_umax_pp_setastra (2000);
      err = 2000;
    }
  else
    {
      /* detects 1600  by finding black scans */
      MoveToOrigin ();
      err = sanei_umax_pp_getastra ();

      /* parking */
      CMDSYNC (0xC2);
      CMDSYNC (0x00);
      if (sanei_umax_pp_Park () == 0)
	DBG (0, "Park failed !!! (%s:%d)\n", __FILE__, __LINE__);

      /* poll parking */
      do
	{
	  sleep (1);
	  CMDSYNC (0x40);
	}
      while ((sanei_umax_pp_ScannerStatus () & MOTOR_BIT) == 0x00);
    }

  /* return guessed model number */
  CMDSYNC (0x00);
  return (err);
}



/* sets, resets gamma tables */

void
sanei_umax_pp_gamma (int *red, int *green, int *blue)
{
  if (red != NULL)
    {
      ggRed = red;
    }
  else
    {
      ggRed = ggamma;
    }

  if (green != NULL)
    {
      ggGreen = green;
    }
  else
    {
      ggGreen = ggamma;
    }

  if (blue != NULL)
    {
      ggBlue = blue;
    }
  else
    {
      ggBlue = ggamma;
    }
}
