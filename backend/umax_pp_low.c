/**
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





#include "../include/sane/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_IO_H
#include <sys/io.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include "../include/sane/sanei_debug.h"
#include <errno.h>

#ifdef HAVE_DEV_PPBUS_PPI_H
#include <dev/ppbus/ppi.h>
#include <dev/ppbus/ppbconf.h>
#endif

#ifdef HAVE_MACHINE_CPUFUNC_H
#include <machine/cpufunc.h>
#endif

#ifdef HAVE_I386_SET_IOPERM
#include <machine/sysarch.h>
#endif

#ifdef HAVE_LINUX_PPDEV_H
#include <sys/ioctl.h>
#include <linux/parport.h>
#include <linux/ppdev.h>
#endif

/*************************************************/
/* here we define sanei_inb/sanei_outb based on  */
/* OS dependant inb/outb definitions             */
/* SANE_INB is defined whenever a valid inb/outb */
/* definition has been found                     */
/* once all these work, it might be moved to     */
/* sanei_pio.c                                   */
/*************************************************/

#ifdef ENABLE_PARPORT_DIRECTIO

#if (! defined SANE_INB ) && ( defined HAVE_SYS_HW_H )	/* OS/2 EMX case */
#define SANE_INB 1
static int
sanei_ioperm (int start, int length, int enable)
{
  if (enable)
    return _portaccess (port, port + length - 1);
  return 0;
}

static unsigned char
sanei_inb (unsigned int port)
{
  return _inp8 (port) & 0xFF;
}

static void
sanei_outb (unsigned int port, unsigned char value)
{
  _outp8 (port, value);
}

static void
sanei_insb (unsigned int port, unsigned char *addr, unsigned long count)
{
  _inps8 (port, (unsigned char *) addr, count);
}

static void
sanei_insl (unsigned int port, unsigned char *addr, unsigned long count)
{
  _inps32 (port, (unsigned long *) addr, count);
}

static void
sanei_outsb (unsigned int port, const unsigned char *addr,
	     unsigned long count)
{
  _outps8 (port, (unsigned char *) addr, count);
}

static void
sanei_outsl (unsigned int port, const unsigned char *addr,
	     unsigned long count)
{
  _outps32 (port, (unsigned long *) addr, count);
}
#endif /* OS/2 EMX case */



#if (! defined SANE_INB ) && ( defined HAVE_MACHINE_CPUFUNC_H )	/* FreeBSD case */
#define SANE_INB 2
static int
sanei_ioperm (int start, int length, int enable)
{
#ifdef HAVE_I386_SET_IOPERM
  return i386_set_ioperm (start, length, enable);
#else
  int fd = 0;

  /* makes compilers happy */
  start = length + enable;
  fd = open ("/dev/io", O_RDONLY);
  if (fd > 0)
    return 0;
  return -1;
#endif
}

static unsigned char
sanei_inb (unsigned int port)
{
  return inb (port);
}

static void
sanei_outb (unsigned int port, unsigned char value)
{
  outb (port, value);
}

static void
sanei_insb (unsigned int port, unsigned char *addr, unsigned long count)
{
  insb (port, addr, count);
}

static void
sanei_insl (unsigned int port, unsigned char *addr, unsigned long count)
{
  insl (port, addr, count);
}

static void
sanei_outsb (unsigned int port, const unsigned char *addr,
	     unsigned long count)
{
  outsb (port, addr, count);
}

static void
sanei_outsl (unsigned int port, const unsigned char *addr,
	     unsigned long count)
{
  outsl (port, addr, count);
}
#endif /* FreeBSD case */


/* linux GCC on i386 */
#if ( ! defined SANE_INB ) && ( defined HAVE_SYS_IO_H ) && ( defined __GNUC__ ) && ( defined __i386__ )
#define SANE_INB 3

static int
sanei_ioperm (int start, int length, int enable)
{
#ifdef HAVE_IOPERM
  return ioperm (start, length, enable);
#else
  /* linux without ioperm ? hum ... */
  /* makes compilers happy */
  start = length + enable;
  return 0;
#endif
}

static unsigned char
sanei_inb (unsigned int port)
{
  return inb (port);
}

static void
sanei_outb (unsigned int port, unsigned char value)
{
  outb (value, port);
}

static void
sanei_insb (unsigned int port, unsigned char *addr, unsigned long count)
{
  insb (port, addr, count);
}

static void
sanei_insl (unsigned int port, unsigned char *addr, unsigned long count)
{
  insl (port, addr, count);
}

static void
sanei_outsb (unsigned int port, const unsigned char *addr,
	     unsigned long count)
{
  outsb (port, addr, count);
}

static void
sanei_outsl (unsigned int port, const unsigned char *addr,
	     unsigned long count)
{
  /* oddly, 32 bit I/O are done with outsw instead of the expected outsl */
  outsw (port, addr, count);
}
#endif /* linux GCC on i386 */


/* linux GCC non i386 */
#if ( ! defined SANE_INB ) && ( defined HAVE_SYS_IO_H ) && ( defined __GNUC__ ) && ( ! defined __i386__ )
#define SANE_INB 4
static int
sanei_ioperm (int start, int length, int enable)
{
#ifdef HAVE_IOPERM
  return ioperm (start, length, enable);
#else
  /* linux without ioperm ? hum ... */
  /* makes compilers happy */
  start = length + enable;
  return 0;
#endif
}

static unsigned char
sanei_inb (unsigned int port)
{
  return inb (port);
}

static void
sanei_outb (unsigned int port, unsigned char value)
{
  outb (value, port);
}

static void
sanei_insb (unsigned int port, unsigned char *addr, unsigned long count)
{
  int i;

  for (i = 0; i < count; i++)
    addr[i] = sanei_inb (port);
}

static void
sanei_insl (unsigned int port, unsigned char *addr, unsigned long count)
{
  for (i = 0; i < count * 4; i++)
    addr[i] = sanei_inb (port);
}

static void
sanei_outsb (unsigned int port, const unsigned char *addr,
	     unsigned long count)
{
  int i;

  for (i = 0; i < count; i++)
    sanei_outb (port, addr[i]);
}

static void
sanei_outsl (unsigned int port, const unsigned char *addr,
	     unsigned long count)
{
  int i;

  for (i = 0; i < count * 4; i++)
    sanei_outb (port, addr[i]);
}
#endif /* linux GCC non i386 */


/* ICC on i386 */
#if ( ! defined SANE_INB ) && ( defined __INTEL_COMPILER ) && ( defined __i386__ )
#define SANE_INB 5
static int
sanei_ioperm (int start, int length, int enable)
{
#ifdef HAVE_IOPERM
  return ioperm (start, length, enable);
#else
  /* ICC without ioperm() ... */
  /* makes compilers happy */
  start = length + enable;
  return 0;
#endif
}
static unsigned char
sanei_inb (unsigned int port)
{
  unsigned char ret;

  __asm__ __volatile__ ("inb %%dx,%%al":"=a" (ret):"d" ((u_int) port));
  return ret;
}

static void
sanei_outb (unsigned int port, unsigned char value)
{
  __asm__ __volatile__ ("outb %%al,%%dx"::"a" (value), "d" ((u_int) port));
}

static void
sanei_insb (unsigned int port, void *addr, unsigned long count)
{
  __asm__ __volatile__ ("rep ; insb":"=D" (addr), "=c" (count):"d" (port),
			"0" (addr), "1" (count));
}

static void
sanei_insl (unsigned int port, void *addr, unsigned long count)
{
  __asm__ __volatile__ ("rep ; insl":"=D" (addr), "=c" (count):"d" (port),
			"0" (addr), "1" (count));
}

static void
sanei_outsb (unsigned int port, const void *addr, unsigned long count)
{
  __asm__ __volatile__ ("rep ; outsb":"=S" (addr), "=c" (count):"d" (port),
			"0" (addr), "1" (count));
}

static void
sanei_outsl (unsigned int port, const void *addr, unsigned long count)
{
  __asm__ __volatile__ ("rep ; outsl":"=S" (addr), "=c" (count):"d" (port),
			"0" (addr), "1" (count));
}

#endif /* ICC on i386 */

/* direct io requested, but no valid inb/oub */
#if ( ! defined SANE_INB) && ( defined ENABLE_PARPORT_DIRECTIO )
#warning "ENABLE_PARPORT_DIRECTIO cannot be used du to lack of inb/out definition"
#undef ENABLE_PARPORT_DIRECTIO
#endif

#endif /* ENABLE_PARPORT_DIRECTIO */
/*
 * no inb/outb without --enable-parport-directio *
 */
#ifndef ENABLE_PARPORT_DIRECTIO
#define SANE_INB 0
static int
sanei_ioperm (int start, int length, int enable)
{
  /* make compilers happy */
  enable = start + length;

  /* returns failure */
  return -1;
}

static unsigned char
sanei_inb (unsigned int port)
{
  /* makes compilers happy */
  port = 0;
  return 255;
}

static void
sanei_outb (unsigned int port, unsigned char value)
{
  /* makes compilers happy */
  port = 0;
  value = 0;
}

static void
sanei_insb (unsigned int port, unsigned char *addr, unsigned long count)
{
  /* makes compilers happy */
  if (addr)
    {
      port = 0;
      count = 0;
    }
}

static void
sanei_insl (unsigned int port, unsigned char *addr, unsigned long count)
{
  /* makes compilers happy */
  if (addr)
    {
      port = 0;
      count = 0;
    }
}

static void
sanei_outsb (unsigned int port, const unsigned char *addr,
	     unsigned long count)
{
  /* makes compilers happy */
  if (addr)
    {
      port = 0;
      count = 0;
    }
}

static void
sanei_outsl (unsigned int port, const unsigned char *addr,
	     unsigned long count)
{
  /* makes compilers happy */
  if (addr)
    {
      port = 0;
      count = 0;
    }
}
#endif /* ENABLE_PARPORT_DIRECTIO is not defined */

/* we need either direct io or ppdev */
#if ! defined ENABLE_PARPORT_DIRECTIO && ! defined HAVE_LINUX_PPDEV_H && ! defined HAVE_DEV_PPBUS_PPI_H
#define IO_SUPPORT_MISSING
#endif


#include "umax_pp_low.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#ifndef __IO__
#define __IO__

#define DATA                   gPort+0x00
#define STATUS                 gPort+0x01
#define CONTROL                gPort+0x02
#define EPPADR                 gPort+0x03
#define EPPDATA                gPort+0x04

#define ECPDATA                gPort+0x400
#define ECR		       gPort+0x402

#define FIFO_WAIT	      1000
#endif

static int Fonc001 (void);
static int FoncSendWord (int *cmd);

static void SetEPPMode (int mode);
static int GetEPPMode (void);
static void SetModel (int model);
static int GetModel (void);
static int RingScanner (int count, unsigned long delay);
static int TestVersion (int no);

static int ProbePS2 (unsigned char *dest);
static int ProbeEPP (unsigned char *dest);
static int ProbeECP (unsigned char *dest);

static int SendCommand (int cmd);
static void SPPResetLPT (void);
static int SendWord (int *cmd);
static int SendData (int *cmd, int len);
static int ReceiveData (int *cmd, int len);
static int SendLength (int *cmd, int len);

static int WaitAck (void);
static void Init001 (void);
static int Init002 (int arg);
static int Init005 (int arg);

/* parport mode setting */
static void ECPCompatMode (void);
static void ECPByteMode (void);

/* block transfer init */
static void ECPSetBuffer (int size);

/* mode dependant operations */
static int PS2Something (int reg);
static void PS2BufferRead (int size, unsigned char *dest);
static void PS2BufferWrite (int size, unsigned char *source);
static int PS2RegisterRead (int reg);
static void PS2RegisterWrite (int reg, int value);

static int EPPConnect (void);
static int EPPRegisterRead (int reg);
static void EPPRegisterWrite (int reg, int value);
static void EPPBufferRead (int size, unsigned char *dest);
static void EPPBufferWrite (int size, unsigned char *source);
static void EPPRead32Buffer (int size, unsigned char *dest);
static void EPPWrite32Buffer (int size, unsigned char *source);

static int ECPConnect (void);
static void ECPDisconnect (void);
static int ECPRegisterRead (int reg);
static void ECPRegisterWrite (int reg, int value);
static int ECPBufferRead (int size, unsigned char *dest);
static void ECPBufferWrite (int size, unsigned char *source);
static int WaitFifoEmpty (void);
static int WaitFifoNotEmpty (void);
static int WaitFifoFull (void);

/* generic operations */
static int Connect (void);
static void Disconnect (void);
static void BufferRead (int size, unsigned char *dest);
static void BufferWrite (int size, unsigned char *source);
static int PausedBufferRead (int size, unsigned char *dest);


static void ClearRegister (int reg);

static int connect_epat (int r08);
static int Prologue (int r08);
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
void sanei_umax_pp_gamma (int *red, int *green, int *blue);


#define WRITESLOW(x,y) \
        PS2RegisterWrite((x),(y)); \
        DBG(16,"PS2RegisterWrite(0x%X,0x%X) passed...   (%s:%d)\n",(x),(y),__FILE__,__LINE__);

#define SLOWNIBBLEREGISTERREAD(x,y) \
        tmp=PS2RegisterRead(x);\
        if(tmp!=y)\
        {\
                DBG(0,"PS2RegisterRead: found 0x%X expected 0x%X (%s:%d)\n",tmp,y,__FILE__,__LINE__);\
                /*return 0;*/ \
        }\
        DBG(16,"PS2RegisterRead(0x%X)=0x%X passed... (%s:%d)\n",x,y,__FILE__,__LINE__);


#define REGISTERWRITE(x,y) \
        RegisterWrite((x),(y)); \
        DBG(16,"RegisterWrite(0x%X,0x%X) passed...   (%s:%d)\n",(x),(y),__FILE__,__LINE__);

#define REGISTERREAD(x,y) \
        tmp=RegisterRead(x);\
        if(tmp!=y)\
        {\
                DBG(0,"RegisterRead, found 0x%X expected 0x%X (%s:%d)\n",tmp,y,__FILE__,__LINE__);\
                return 0;\
        }\
        DBG(16,"RegisterRead(0x%X)=0x%X passed... (%s:%d)\n",x,y,__FILE__,__LINE__);


#define TRACE(level,msg)        DBG(level, msg"  (%s:%d)\n",__FILE__,__LINE__);


#define CMDSYNC(x)        if(sanei_umax_pp_CmdSync(x)!=1)\
                        {\
                                DBG(0,"CmdSync(0x%02X) failed (%s:%d)\n",x,__FILE__,__LINE__);\
                                return 0;\
                        }\
                        TRACE(16,"CmdSync() passed ...")

#define CMDSETGET(cmd,len,sent) if(CmdSetGet(cmd,len,sent)!=1)\
                                {\
                                        DBG(0,"CmdSetGet(0x%02X,%d,sent) failed (%s:%d)\n",cmd,len,__FILE__,__LINE__);\
                                        return 0;\
                                }\
                                TRACE(16,"CmdSetGet() passed ...")

#define YOFFSET                40
#define YOFFSET1220P        40
#define YOFFSET2000P        40



#define COMPLETIONWAIT        if(CompletionWait()==0)\
                        {\
                                DBG(0,"CompletionWait() failed (%s:%d)\n",__FILE__,__LINE__);\
                                return 0;\
                        }\
                        TRACE(16,"CompletionWait() passed ...")

#define MOVE(x,y,t)        if(Move(x,y,t)==0)\
                        {\
                                DBG(0,"Move(%d,%d,buffer) failed (%s:%d)\n",x,y,__FILE__,__LINE__);\
                                return 0;\
                        }\
                        TRACE(16,"Move() passed ...")

#define CMDGETBUF(cmd,len,sent) if(CmdGetBuffer(cmd,len,sent)!=1)\
                                {\
                                        DBG(0,"CmdGetBuffer(0x%02X,%ld,buffer) failed (%s:%d)\n",cmd,(long)len,__FILE__,__LINE__);\
                                        return 0;\
                                }\
                                TRACE(16,"CmdGetBuffer() passed ...")

#define CMDGETBUF32(cmd,len,sent) if(CmdGetBuffer32(cmd,len,sent)!=1)\
                                {\
                                        DBG(0,"CmdGetBuffer32(0x%02X,%ld,buffer) failed (%s:%d)\n",cmd,(long)len,__FILE__,__LINE__);\
                                        return 0;\
                                }\
                                TRACE(16,"CmdGetBuffer32() passed ...")

#define CMDSET(cmd,len,sent) if(CmdSet(cmd,len,sent)!=1)\
                                {\
                                        DBG(0,"CmdSet(0x%02X,%d,sent) failed (%s:%d)\n",cmd,len,__FILE__,__LINE__);\
                                        return 0;\
                                }\
                                TRACE(16,"CmdSet() passed ...")

#define CMDGET(cmd,len,sent) if(CmdGet(cmd,len,sent)!=1)\
                                {\
                                        DBG(0,"CmdGet(0x%02X,%d,read) failed (%s:%d)\n",cmd,len,__FILE__,__LINE__);\
                                        return 0;\
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
static int gECP = 0;

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
static int gAutoSettings = 1;


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



char **
sanei_parport_find_port (void)
{
  char **ports = NULL;
#ifdef ENABLE_PARPORT_DIRECTIO
  int i, addr, ecpaddr;
  int found = 0;
  char name[80], buffer[80];
  FILE *fic = NULL;

  /* direct I/O detection */
  /* linux 2.4 + 2.6 with proc support */
  for (i = 0; i < 4; i++)
    {
      /* try to ensure loading of lp module */
      sprintf (name, "/dev/lp%d", i);
      fic = fopen (name, "wb");
      if (fic != NULL)
	fclose (fic);
      sprintf (name, "/proc/sys/dev/parport/parport%d/base-addr", i);
      fic = fopen (name, "rb");
      if (fic != NULL)
	{
	  fread (buffer, 64, 1, fic);
	  fclose (fic);
	  if (sscanf (buffer, "%d %d", &addr, &ecpaddr) > 0)
	    {
	      DBG (16, "parport at 0x%X\n", addr);
	      ports =
		(char **) realloc (ports, (found + 2) * sizeof (char *));
	      ports[found] = (char *) malloc (19);
	      sprintf (ports[found], "0x%X", addr);
	      found++;
	      ports[found] = NULL;
	    }
	}
    }
#endif
  return ports;
}


char **
sanei_parport_find_device (void)
{
  char *devices[] = { "/dev/ppi0",
    "/dev/ppi1",
    "/dev/ppi2",
    "/dev/ppi3",
    "/dev/parport0",
    "/dev/parport1",
    "/dev/parport2",
    "/dev/parport3",
    NULL
  };
  int i, file;
  int rc = 0;
  int found = 0;
  char **ports = NULL;


  /* device finding */
  i = 0;
  while (devices[i] != NULL)
    {
      DBG (16, "Controling %s: ", devices[i]);
      file = open (devices[i], O_RDWR);
      if (file < 0)
	{
	  switch (errno)
	    {
	    case ENOENT:
#ifdef ENIO
	    case ENXIO:
#endif
#ifdef ENODEV
	    case ENODEV:
#endif
	      DBG (16, "no %s device ...\n", devices[i]);
	      break;
	    case EACCES:
	      DBG (16, "current user cannot use existing %s device ...\n",
		   devices[i]);
	      break;
	    default:
	      perror (devices[i]);
	    }
	}
      else
	{
#ifdef HAVE_LINUX_PPDEV_H
	  /* on kernel < 2.4.23, you have to CLAIM the device 
	   * to check it really exists
	   * we may hang if another program already claimed it
	   */
	  rc = ioctl (file, PPCLAIM);
	  if (rc)
	    {
	      switch (errno)
		{
		case ENOENT:
#ifdef ENXIO
		case ENXIO:
#endif
#ifdef ENODEV
		case ENODEV:
#endif
		  DBG (16, "no %s device ...\n", devices[i]);
		  break;
		case EACCES:
		  DBG (16, "current user cannot use existing %s device ...\n",
		       devices[i]);
		  break;
		default:
		  DBG (16, "errno=%d\n", errno);
		  perror (devices[i]);
		}
	    }
	  else
	    {
	      rc = ioctl (file, PPRELEASE);
	    }
#endif /* HAVE_LINUX_PPDEV_H */
	  close (file);
	  if (!rc)
	    {
	      DBG (16, "adding %s to valid devices ...\n", devices[i]);
	      ports =
		(char **) realloc (ports, (found + 2) * sizeof (char *));
	      ports[found] = strdup (devices[i]);
	      found++;
	      ports[found] = NULL;
	    }
	}

      /* suite */
      i++;
    }
  return ports;
}



/*
 * gain direct acces to IO port, and set parport to the 'right' mode
 * returns 1 on success, 0 an failure
 */


int
sanei_umax_pp_InitPort (int port, char *name)
{
  int fd, ectr;
  int found = 0, ecp = 1;
#if ((defined HAVE_IOPERM)||(defined HAVE_MACHINE_CPUFUNC_H)||(defined HAVE_LINUX_PPDEV_H))
  int mode, modes, rc;
#endif
#ifdef HAVE_LINUX_PPDEV_H
  char strmodes[160];
#endif

  /* since this function must be called before */
  /* any other, we put debug init here         */
  DBG_INIT ();
  DBG (1, "SANE_INB level %d\n", SANE_INB);

  /* sets global vars */
  ggGreen = ggamma;
  ggBlue = ggamma;
  ggRed = ggamma;
  gParport = 0;
  gCancel = 0;
  gAutoSettings = 1;
  gControl = 0;
  gData = 0;
  g674 = 0;
  g67D = 0;
  g67E = 0;
  gEPAT = 0;
  g6FE = 0;
  sanei_umax_pp_setparport (0);


  DBG (1, "sanei_umax_pp_InitPort(0x%X,%s)\n", port, name);
#ifndef ENABLE_PARPORT_DIRECTIO
  if ((name == NULL) || ((name != NULL) && (strlen (name) < 4)))
    {
      DBG (0, "sanei_umax_pp_InitPort cannot use direct hardware access\n");
      DBG (0, "if not compiled with --enable-parport-directio\n");
      return 0;
    }
#endif


  /* init global var holding port value */
  gPort = port;

#ifdef IO_SUPPORT_MISSING
  DBG (1, "*** Direct I/O or ppdev unavailable, giving up ***\n");
  return 0;
#else


#ifdef HAVE_LINUX_PPDEV_H
  if (name != NULL)
    {
      if (strlen (name) > 3)
	{
	  /* ppdev opening and configuration                               */
	  found = 0;
	  fd = open (name, O_RDWR | O_NOCTTY | O_NONBLOCK);
	  if (fd < 0)
	    {
	      switch (errno)
		{
		case ENOENT:
		  DBG (1, "umax_pp: '%s' does not exist \n", name);
		  break;
		case EACCES:
		  DBG (1,
		       "umax_pp: current user has not R/W permissions on '%s' \n",
		       name);
		  break;
		}
	      return 0;

	    }
	  /* claim port */
	  if (ioctl (fd, PPCLAIM))
	    {
	      DBG (1, "umax_pp: cannot claim port '%s'\n", name);
	    }
	  else
	    {
	      /* we check if parport does EPP or ECP */
#ifdef PPGETMODES
	      if (ioctl (fd, PPGETMODES, &modes))
		{
		  DBG (16,
		       "umax_pp: ppdev couldn't gave modes for port '%s'\n",
		       name);
		}
	      else
		{
		  sprintf (strmodes, "\n");
		  if (modes & PARPORT_MODE_PCSPP)
		    sprintf (strmodes, "%s\t\tPARPORT_MODE_PCSPP\n",
			     strmodes);
		  if (modes & PARPORT_MODE_TRISTATE)
		    sprintf (strmodes, "%s\t\tPARPORT_MODE_TRISTATE\n",
			     strmodes);
		  if (modes & PARPORT_MODE_EPP)
		    sprintf (strmodes, "%s\t\tPARPORT_MODE_EPP\n", strmodes);
		  if (modes & PARPORT_MODE_ECP)
		    sprintf (strmodes, "%s\t\tPARPORT_MODE_ECP\n", strmodes);
		  if (modes & PARPORT_MODE_COMPAT)
		    sprintf (strmodes, "%s\t\tPARPORT_MODE_COMPAT\n",
			     strmodes);
		  if (modes & PARPORT_MODE_DMA)
		    sprintf (strmodes, "%s\t\tPARPORT_MODE_DMA\n", strmodes);
		  DBG (32, "parport modes: %X\n", modes);
		  DBG (32, "parport modes: %s\n", strmodes);
		  if (!(modes & PARPORT_MODE_EPP)
		      && !(modes & PARPORT_MODE_ECP))
		    {
		      DBG (1,
			   "port 0x%X does not have EPP or ECP, giving up ...\n",
			   port);
		      mode = IEEE1284_MODE_COMPAT;
		      ioctl (fd, PPSETMODE, &mode);
		      ioctl (fd, PPRELEASE);
		      close (fd);
		      return 0;
		    }
		}

#else
	      DBG (16,
		   "umax_pp: ppdev used to build SANE doesn't have PPGETMODES.\n");
	      /* faking result */
	      modes = 0xFFFFFFFF;
#endif
	      /* prefered mode is EPP */
	      if (modes & PARPORT_MODE_EPP)
		{
		  mode = IEEE1284_MODE_EPP;

		  /* negot allways fail here ... */
		  rc = ioctl (fd, PPNEGOT, &mode);
		  if (rc)
		    {
		      DBG (16,
			   "umax_pp: ppdev couldn't negociate mode IEEE1284_MODE_EPP for '%s' (ignored)\n",
			   name);
		    }
		  if (ioctl (fd, PPSETMODE, &mode))
		    {
		      DBG (16,
			   "umax_pp: ppdev couldn't set mode to IEEE1284_MODE_EPP for '%s'\n",
			   name);
		      /* signal failure for ECP test */
		      mode = 0;
		    }
		  else
		    {
		      DBG (16,
			   "umax_pp: mode set to PARPORT_MODE_EPP for '%s'\n",
			   name);
		    }
		}

	      if ((modes & PARPORT_MODE_ECP) && (mode == 0))
		{
		  mode = IEEE1284_MODE_ECP;
		  rc = ioctl (fd, PPNEGOT, &mode);
		  if (rc)
		    {
		      DBG (16,
			   "umax_pp: ppdev couldn't negociate mode IEEE1284_MODE_ECP for '%s' (ignored)\n",
			   name);
		    }
		  if (ioctl (fd, PPSETMODE, &mode))
		    {
		      DBG (16,
			   "umax_pp: ppdev couldn't set mode to IEEE1284_MODE_ECP for '%s'\n",
			   name);
		      DBG (1,
			   "port 0x%X can't be set to EPP or ECP, giving up ...\n",
			   port);

		      mode = IEEE1284_MODE_COMPAT;
		      ioctl (fd, PPSETMODE, &mode);
		      ioctl (fd, PPRELEASE);
		      close (fd);
		      return 0;
		    }
		  else
		    {
		      gECP = 1;
		      DBG (16,
			   "umax_pp: mode set to PARPORT_MODE_ECP for '%s'\n",
			   name);
		    }
		}


	      /* allways start in compat mode (for probe) */
	      mode = IEEE1284_MODE_COMPAT;
	      ioctl (fd, PPSETMODE, &mode);
	      found = 1;

	    }

	  if (!found)
	    {
	      DBG (1, "device %s does not fit ...\n", name);
	    }
	  else
	    {
	      DBG (1, "Using %s ...\n", name);
	      sanei_umax_pp_setparport (fd);
	      return 1;
	    }
	}
    }
#endif /* HAVE_LINUX_PPDEV_H */


#ifdef HAVE_DEV_PPBUS_PPI_H
/* the ppi device let user access to parallel port on freebsd */
  if (name != NULL)
    {
      if (strlen (name) > 3)
	{
	  /* ppi opening and configuration                               */
	  found = 0;
	  fd = open (name, O_RDONLY);
	  if (fd < 0)
	    {
	      switch (errno)
		{
		case ENOENT:
		  DBG (1, "umax_pp: '%s' does not exist \n", name);
		  break;
		case EACCES:
		  DBG (1,
		       "umax_pp: current user has not read permissions on '%s' \n",
		       name);
		  break;
		}
	      return 0;
	    }
	  else
	    {
	      DBG (1, "Using %s ...\n", name);
	      sanei_umax_pp_setparport (fd);
	      return 1;
	    }
	}
    }
#endif /* HAVE_DEV_PPBUS_PPI_H */

  if (port < 0x400)
    {
      if (sanei_ioperm (port, 8, 1) != 0)
	{
	  DBG (1, "sanei_ioperm() could not gain access to 0x%X\n", port);
	  return 0;
	}
      DBG (1, "sanei_ioperm(0x%X, 8, 1) OK ...\n", port);
    }

#ifdef HAVE_IOPERM
  /* ECP i/o range */
  if (iopl (3) != 0)
    {
      DBG (1, "iopl could not raise IO permission to level 3\n");
      DBG (1, "*NO* ECP support\n");
      ecp = 0;

    }
  else
    {
      /* any ECP out there ? */
      ectr = Inb (ECR);
      if (ectr != 0xFF)
	{
	  gECP = 1;

	}
    }
#else
  ecp = 0;
#endif



#endif /* IO_SUPPORT_MISSING */
  return 1;
}






static void
Outb (int port, int value)
{
#ifndef IO_SUPPORT_MISSING

#ifdef HAVE_LINUX_PPDEV_H
  int fd, rc;
  unsigned char val;


  fd = sanei_umax_pp_getparport ();
  val = (unsigned char) value;
  if (fd > 0)
    {
      /* there should be ECR that doesn't go through ppdev */
      /* it will leave when all the I/O will be done with ppdev   */
      switch (port - gPort)
	{
	case 0:
	  rc = ioctl (fd, PPWDATA, &val);
	  if (rc)
	    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  return;
	case 2:
	  if (val & 0x20)
	    {
	      rc = ioctl (fd, PPDATADIR, &val);
	      if (rc)
		DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n",
		     strerror (errno), __FILE__, __LINE__);
	    }
	  else
	    {
	      rc = ioctl (fd, PPWCONTROL, &val);
	      if (rc)
		DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n",
		     strerror (errno), __FILE__, __LINE__);
	    }
	  return;
	case 0x400:
	case 0x402:
	  break;
	default:
	  DBG (16, "Outb(0x%03X,0x%02X) escaped ppdev\n", port, value);
	}
    }
#endif /* HAVE_LINUX_PPDEV_H */


#ifdef HAVE_DEV_PPBUS_PPI_H
  int fd, rc;
  unsigned char val;


  fd = sanei_umax_pp_getparport ();
  val = (unsigned char) value;
  if (fd > 0)
    {
      switch (port - gPort)
	{
	case 0:
	  rc = ioctl (fd, PPISDATA, &val);
	  if (rc)
	    DBG (0, "ppi ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  return;
	case 1:
	  rc = ioctl (fd, PPISSTATUS, &val);
	  if (rc)
	    DBG (0, "ppi ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  return;
	case 2:
	  rc = ioctl (fd, PPISCTRL, &val);
	  if (rc)
	    DBG (0, "ppi ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  return;
	case 3:
	  rc = ioctl (fd, PPISEPPA, &val);
	  if (rc)
	    DBG (0, "ppi ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  return;
	case 4:
	  rc = ioctl (fd, PPISEPPD, &val);
	  if (rc)
	    DBG (0, "ppi ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  return;
	case 0x402:
	  rc = ioctl (fd, PPISECR, &val);
	  if (rc)
	    DBG (0, "ppi ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  return;
	default:
	  DBG (16, "Outb(0x%03X,0x%02X) escaped ppi\n", port, value);
	}
    }
#endif /* HAVE_DEV_PPBUS_PPI_H */

  sanei_outb (port, value);

#endif /* IO_SUPPORT_MISSING */
}





static int
Inb (int port)
{
  int res = 0xFF;
#ifndef IO_SUPPORT_MISSING

#ifdef HAVE_LINUX_PPDEV_H
  int fd, rc;
  unsigned char val;


  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {
      /* there should be ECR that doesn't go through ppdev */
      /* it will leave when all the I/O will be done with ppdev   */
      switch (port - gPort)
	{
	case 0:
	  rc = ioctl (fd, PPRDATA, &val);
	  if (rc)
	    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  res = val;
	  return res;

	case 1:
	  rc = ioctl (fd, PPRSTATUS, &val);
	  if (rc)
	    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  res = val;
	  return res;

	case 2:
	  rc = ioctl (fd, PPRCONTROL, &val);
	  if (rc)
	    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  res = val;
	  return res;

	case 0x400:
	case 0x402:
	default:
	  DBG (16, "Inb(0x%03X) escaped ppdev\n", port);
	}
    }
#endif /* HAVE_LINUX_PPDEV_H */


#ifdef HAVE_DEV_PPBUS_PPI_H
  int fd, rc;
  unsigned char val;


  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {
      switch (port - gPort)
	{
	case 0:
	  rc = ioctl (fd, PPIGDATA, &val);
	  if (rc)
	    DBG (0, "ppi ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  res = val;
	  return res;
	case 1:
	  rc = ioctl (fd, PPIGSTATUS, &val);
	  if (rc)
	    DBG (0, "ppi ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  res = val;
	  return res;
	case 2:
	  rc = ioctl (fd, PPIGCTRL, &val);
	  if (rc)
	    DBG (0, "ppi ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  res = val;
	  return res;
	case 3:
	  rc = ioctl (fd, PPIGEPPA, &val);
	  if (rc)
	    DBG (0, "ppi ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  res = val;
	  return res;
	case 4:
	  rc = ioctl (fd, PPIGEPPD, &val);
	  if (rc)
	    DBG (0, "ppi ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  res = val;
	  return res;
	case 0x402:
	  rc = ioctl (fd, PPIGECR, &val);
	  if (rc)
	    DBG (0, "ppi ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  res = val;
	  return res;
	default:
	  DBG (16, "Inb(0x%03X) escaped ppi\n", port);
	}
    }
#endif /* HAVE_DEV_PPBUS_PPI_H */

  res = sanei_inb (port);

#endif /* IO_SUPPORT_MISSING */
  return res;
}


static void
Insb (int port, unsigned char *dest, int size)
{
#ifndef IO_SUPPORT_MISSING

#ifdef HAVE_DEV_PPBUS_PPI_H
  int i;
  if (sanei_umax_pp_getparport () > 0)
    {
      for (i = 0; i < size; i++)
	dest[i] = Inb (port);
      return;
    }
#endif /* HAVE_DEV_PPBUS_PPI_H */

  sanei_insb (port, dest, size);

#endif
}

static void
Outsb (int port, unsigned char *source, int size)
{
#ifndef IO_SUPPORT_MISSING

#ifdef HAVE_DEV_PPBUS_PPI_H
  int i;

  if (sanei_umax_pp_getparport () > 0)
    {
      for (i = 0; i < size; i++)
	Outb (port, source[i]);
      return;
    }
#endif /* HAVE_DEV_PPBUS_PPI_H */

  sanei_outsb (port, source, size);

#endif
}



/* size = nb words */
static void
Insw (int port, unsigned char *dest, int size)
{
#ifndef IO_SUPPORT_MISSING

#ifdef HAVE_DEV_PPBUS_PPI_H
  int i;

  if (sanei_umax_pp_getparport () > 0)
    {
      for (i = 0; i < size * 4; i++)
	dest[i] = Inb (port);
      return;
    }
#endif /* HAVE_DEV_PPBUS_PPI_H */

  sanei_insl (port, dest, size);

#endif
}

static void
Outsw (int port, unsigned char *source, int size)
{
#ifndef IO_SUPPORT_MISSING

#ifdef HAVE_DEV_PPBUS_PPI_H
  int i;

  if (sanei_umax_pp_getparport () > 0)
    {
      for (i = 0; i < size * 4; i++)
	Outb (port, source[i]);
      return;
    }
#endif /* HAVE_DEV_PPBUS_PPI_H */

  sanei_outsl (port, source, size);

#endif
}


/* we're trying to gather information on the scanner here, */
/* and published it through an easy interface              */
static int scannerStatus = 0;
static int epp32 = 1;
static int gMode = 0;
static int gProbed = 0;
static int model = 0x15;
static int astra = 0;
static int hasUTA = 0;

int
sanei_umax_pp_UTA (void)
{
  return hasUTA;
}

int
sanei_umax_pp_ScannerStatus (void)
{
  /* 0x07 variant returns status with bit 0 or 1 allways set to 1 */
  /* so we mask it out                                            */
  return scannerStatus & 0xFC;
}

static int
GetEPPMode (void)
{
  if (epp32)
    return 32;
  return 8;
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
  return model;
}

static void
SetModel (int mod)
{
  model = mod;
}


int
sanei_umax_pp_getparport (void)
{
  return gParport;
}

void
sanei_umax_pp_setparport (int fd)
{
  gParport = fd;
}

int
sanei_umax_pp_getport (void)
{
  return gPort;
}

void
sanei_umax_pp_setport (int port)
{
  gPort = port;
}

int
sanei_umax_pp_getastra (void)
{
  return astra;
}

void
sanei_umax_pp_setastra (int mod)
{
  astra = mod;
}

int
sanei_umax_pp_getauto (void)
{
  return gAutoSettings;
}

void
sanei_umax_pp_setauto (int autoset)
{
  gAutoSettings = autoset;
}

/* set parallel port mode to 'compatible'*/
static void
ECPCompatMode (void)
{
  Outb (ECR, 0x00);
  Outb (CONTROL, 0x04);		/* reset ? */
}

/* set parallel port mode to 'bidirectionel'*/
static void
ECPByteMode (void)
{
  Outb (CONTROL, 0x04);
  Outb (ECR, 0x20);
}

/* wait for ack bit */
/* return 1 on success, 0 on error */
static int
WaitAck ()
{
  unsigned char breg = 0;
  int i = 0;

  Outb (CONTROL, 0x0C);		/* select printer + initialize printer */
  Outb (CONTROL, 0x0C);
  Outb (CONTROL, 0x0C);
  breg = Inb (STATUS);
  while ((i < 1024) && ((breg & 0x04) == 0))
    {
      Outb (CONTROL, 0x0E);	/* autolinefeed ?.. */
      Outb (CONTROL, 0x0E);
      Outb (CONTROL, 0x0E);
      breg = Inb (STATUS);
      i++;
      usleep (1000);
    }
  if (i == 1024)
    {
      DBG (1, "WaitAck failed, time-out waiting for Ack (%s:%d)\n",
	   __FILE__, __LINE__);
      /* return 0; seems to be non-blocking ...*/
    }
  Outb (CONTROL, 0x04);		/* printer reset */
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x04);
  return 1;
}

static int
WaitFifoEmpty (void)
{
  int i;
  unsigned char breg;

  breg = Inb (ECR);
  i = 0;
  while ((i < FIFO_WAIT) && ((breg & 0x01) == 0))
    {
      breg = Inb (ECR);
      i++;
      usleep (2000);
    }
  if (i == FIFO_WAIT)
    {
      DBG (0, "WaitFifoEmpty failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return 0;
    }
  return 1;
}

static int
WaitFifoNotEmpty (void)
{
  int i;
  unsigned char breg;

  breg = Inb (ECR);
  i = 0;
  while ((i < FIFO_WAIT) && ((breg & 0x01) != 0))
    {
      breg = Inb (ECR);
      i++;
      usleep (2000);
    }
  if (i == FIFO_WAIT)
    {
      DBG (0, "WaitFifoNotEmpty failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return 0;
    }
  return 1;
}


static int
WaitFifoFull (void)
{
  int i;
  unsigned char breg;

  breg = Inb (ECR);
  i = 0;
  while ((i < FIFO_WAIT) && ((breg & 0x02) == 0))
    {
      breg = Inb (ECR);
      i++;
      usleep (2000);
    }
  if (i == FIFO_WAIT)
    {
      DBG (0, "WaitFifoFull failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return 0;
    }
  return 1;
}

/*
 * surely some register reading in PS2 mode
 * only one nibble is accessed, may be
 * PS2RegisterLowNibbleRead(reg)
 */
static int
PS2Something (int reg)
{
  unsigned char breg, low, high = 0;

  Outb (CONTROL, 0x04);
  Outb (DATA, reg);		/* register number ? */
  Outb (CONTROL, 0x06);
  Outb (CONTROL, 0x06);
  Outb (CONTROL, 0x06);
  breg = Inb (STATUS);
  low = breg;
  breg = breg & 0x08;
  /* surely means register(0x10)=0x0B */
  /* since reg & 0x08 != 0, high and low nibble
   * differ, but we don't care, since we surely expect it
   * to be 0
   */
  if (breg != 0x08)
    {
      DBG (0, "PS2Something failed, expecting 0x08, got 0x%02X (%s:%d)\n",
	   breg, __FILE__, __LINE__);
    }
  Outb (CONTROL, 0x07);
  Outb (CONTROL, 0x07);
  Outb (CONTROL, 0x07);
  Outb (CONTROL, 0x07);
  Outb (CONTROL, 0x07);
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x04);
  if (breg != 0x08)
    high = Inb (STATUS) & 0xF0;
  return high + ((low & 0xF0) >> 4);
}

static int
PS2Read (void)
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
/* PS2RegisterWrite: write value in register, slow method                            */
/******************************************************************************/
static void
PS2RegisterWrite (int reg, int value)
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
  int gBufferRead[256];		/* read buffer for command 0x10 */


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
      tmp = PS2Read ();
      tmp = tmp * 256 + PS2Read ();
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
	      gBufferRead[i] = Inb (STATUS);
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
PS2RegisterRead (int reg)
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
  return high;
}


static void
PS2BufferRead (int size, unsigned char *dest)
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
PS2BufferWrite (int size, unsigned char *source)
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
    return 0;
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
      return 1;
    }
  return 0;
}

/*
 * connecct to the EPAT chip, and
 * prepare command sending
 */
static int
ECPConnect (void)
{
  int ret, control, data;

  /* these 3 lines set to 'inital mode' */
  Outb (ECR, 0x20);
  Outb (DATA, 0x04);		/* gData */
  Outb (CONTROL, 0x0C);		/* gControl */

  Inb (ECR);			/* 0x35 */
  Outb (ECR, 0x20);
  Outb (ECR, 0x20);

  gData = Inb (DATA);
  gControl = Inb (CONTROL);

  data = Inb (DATA);
  control = Inb (CONTROL);
  Outb (CONTROL, control & 0x1F);
  control = Inb (CONTROL);
  Outb (CONTROL, control & 0x1F);
  SendCommand (0xE0);

  Outb (DATA, 0xFF);
  Outb (DATA, 0xFF);
  ClearRegister (0);
  Outb (CONTROL, 0x0C);
  Outb (CONTROL, 0x04);
  ClearRegister (0);
  ret = PS2Something (0x10);
  if (ret != 0x0B)
    {
      DBG (0,
	   "PS2Something returned 0x%02X, 0x0B expected (%s:%d)\n", ret,
	   __FILE__, __LINE__);
      /*return 0; */
    }
  return 1;
}

static void
EPPDisconnect (void)
{
  if (GetModel () != 0x07)
    SendCommand (40);
  SendCommand (30);
  Outb (DATA, gData);
  Outb (CONTROL, gControl);
}

static void
ECPDisconnect (void)
{
  int control;

  if (GetModel () != 0x07)	/* guessed */
    SendCommand (40);		/* guessed */
  SendCommand (0x30);
  control = Inb (CONTROL) | 0x01;
  Outb (CONTROL, control);
  Outb (CONTROL, control);
  control = control & 0x04;
  Outb (CONTROL, control);
  Outb (CONTROL, control);
  control = control | 0x08;
  Outb (CONTROL, control);
  Outb (DATA, 0xFF);
  Outb (DATA, 0xFF);
  Outb (CONTROL, control);
}

static int
ECPRegisterRead (int reg)
{
  unsigned char breg, value;

  if (sanei_umax_pp_getparport () > 0)
    {
      DBG (0,
	   "ECPRegisterRead works only with direct hardware access(%s:%d)\n",
	   __FILE__, __LINE__);
      return 0xFF;
    }

  Outb (CONTROL, 0x4);

  /* ECP FIFO mode, interrupt bit, dma disabled, 
     service bit, fifo full=0, fifo empty=0 */
  Outb (ECR, 0x60);
  if (WaitFifoEmpty () == 0)
    {
      DBG (0, "ECPRegisterRead failed, FIFO time-out (%s:%d)\n",
	   __FILE__, __LINE__);
    }
  breg = Inb (ECR);

  Outb (DATA, reg);
  if (WaitFifoEmpty () == 0)
    {
      DBG (0, "ECPRegisterRead failed, FIFO time-out (%s:%d)\n",
	   __FILE__, __LINE__);
    }
  breg = Inb (ECR);

  /* byte mode, interrupt bit, dma disabled, 
     service bit, fifo full=0, fifo empty=0 */
  Outb (ECR, 0x20);
  Outb (CONTROL, 0x20);		/* data reverse */

  /* ECP FIFO mode, interrupt bit, dma disabled, 
     service bit, fifo full=0, fifo empty=0 */
  Outb (ECR, 0x60);
  if (WaitFifoNotEmpty () == 0)
    {
      DBG (0, "ECPRegisterRead failed, FIFO time-out (%s:%d)\n",
	   __FILE__, __LINE__);
    }
  breg = Inb (ECR);
  value = Inb (ECPDATA);

  /* according to the spec bit 7 and 6 are unused */
  breg = (Inb (CONTROL) & 0x3F);
  if (breg != 0x20)
    {
      DBG (0, "ECPRegisterRead failed, expecting 0x20, got 0x%02X (%s:%d)\n",
	   breg, __FILE__, __LINE__);
    }
  ECPByteMode ();
  return value;
}

static int
EPPRegisterRead (int reg)
{
#ifdef HAVE_LINUX_PPDEV_H
  int fd, mode, rc;
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
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = write (fd, &breg, 1);
      if (rc != 1)
	DBG (0, "ppdev short write (%s:%d)\n", __FILE__, __LINE__);

      mode = 1;			/* data_reverse */
      rc = ioctl (fd, PPDATADIR, &mode);

      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = read (fd, &bval, 1);
      if (rc != 1)
	DBG (0, "ppdev short read (%s:%d)\n", __FILE__, __LINE__);
      value = bval;

      mode = 0;			/* forward */
      rc = ioctl (fd, PPDATADIR, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);

      return value;
    }
  /* if not, direct hardware access */
#endif

  Outb (EPPADR, reg);
  control = Inb (CONTROL);
  control = (control & 0x1F) | 0x20;
  Outb (CONTROL, control);
  value = Inb (EPPDATA);
  control = Inb (CONTROL);
  control = control & 0x1F;
  Outb (CONTROL, control);
  return value;
}

static int
RegisterRead (int reg)
{
  switch (gMode)
    {
    case UMAX_PP_PARPORT_ECP:
      return ECPRegisterRead (reg);
    case UMAX_PP_PARPORT_EPP_HALF:
    case UMAX_PP_PARPORT_EPP:
      return EPPRegisterRead (reg);
    case UMAX_PP_PARPORT_PS2:
      DBG (0, "STEF: gMode PS2 in RegisterRead !!\n");
      return PS2RegisterRead (reg);
    default:
      DBG (0, "STEF: gMode unset in RegisterRead !!\n");
      return 0xFF;
    }
}


static void
ECPRegisterWrite (int reg, int value)
{
  unsigned char breg;

  if (sanei_umax_pp_getparport () > 0)
    {
      DBG (0,
	   "ECPRegisterWrite works only with direct hardware access(%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }

  /* standard mode, interrupt bit, dma disabled, 
     service bit, fifo full=0, fifo empty=0 */
  ECPCompatMode ();

  /* ECP FIFO mode, interrupt bit, dma disabled, 
     service bit, fifo full=0, fifo empty=0 */
  Outb (ECR, 0x60);
  if (WaitFifoEmpty () == 0)
    {
      DBG (0, "ECPRegisterWrite failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);

  Outb (DATA, reg);
  if (WaitFifoEmpty () == 0)
    {
      DBG (0, "ECPRegisterWrite failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);

  Outb (ECPDATA, value);
  if (WaitFifoEmpty () == 0)
    {
      DBG (0, "ECPRegisterWrite failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);
  ECPByteMode ();
  return;
}

static void
EPPRegisterWrite (int reg, int value)
{
#ifdef HAVE_LINUX_PPDEV_H
  int fd, mode, rc;
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
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = write (fd, &breg, 1);

      bval = (unsigned char) (value);
      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = write (fd, &bval, 1);

      return;
    }
  /* if not, direct hardware access */
#endif
  Outb (EPPADR, reg);
  Outb (EPPDATA, value);
}

static void
RegisterWrite (int reg, int value)
{
  switch (gMode)
    {
    case UMAX_PP_PARPORT_PS2:
      PS2RegisterWrite (reg, value);
      DBG (0, "STEF: gMode PS2 in RegisterWrite !!\n");
      break;
    case UMAX_PP_PARPORT_ECP:
      ECPRegisterWrite (reg, value);
      break;
    case UMAX_PP_PARPORT_EPP_HALF:
    case UMAX_PP_PARPORT_EPP:
      EPPRegisterWrite (reg, value);
      break;
    default:
      DBG (0, "STEF: gMode unset in RegisterWrite !!\n");
      break;
    }
}

static void
EPPBlockMode (int flag)
{
#ifdef HAVE_LINUX_PPDEV_H
  int fd, mode, rc;
  unsigned char bval;

  /* check we have ppdev working */
  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {
      bval = (unsigned char) (flag);
      mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = write (fd, &bval, 1);
      return;
    }
#endif
  Outb (EPPADR, flag);
}

static void
EPPBufferRead (int size, unsigned char *dest)
{
#ifdef HAVE_LINUX_PPDEV_H
  int fd, mode, rc, nb;
  unsigned char bval;
#endif
  int control;

#ifdef HAVE_LINUX_PPDEV_H
  /* check we have ppdev working */
  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {

      bval = 0x80;
      mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = write (fd, &bval, 1);

      mode = 1;			/* data_reverse */
      rc = ioctl (fd, PPDATADIR, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
#ifdef PPSETFLAGS
      mode = PP_FASTREAD;
      rc = ioctl (fd, PPSETFLAGS, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
#endif
      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      nb = 0;
      while (nb < size - 1)
	{
	  rc = read (fd, dest + nb, size - 1 - nb);
	  nb += rc;
	}

      mode = 0;			/* forward */
      rc = ioctl (fd, PPDATADIR, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      bval = 0xA0;
      mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = write (fd, &bval, 1);

      mode = 1;			/* data_reverse */
      rc = ioctl (fd, PPDATADIR, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = read (fd, dest + size - 1, 1);

      mode = 0;			/* forward */
      rc = ioctl (fd, PPDATADIR, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);

      return;
    }
  /* if not, direct hardware access */
#endif

  EPPBlockMode (0x80);
  control = Inb (CONTROL);
  Outb (CONTROL, (control & 0x1F) | 0x20);	/* reverse */
  Insb (EPPDATA, dest, size - 1);
  control = Inb (CONTROL);

  Outb (CONTROL, (control & 0x1F));	/* forward */
  EPPBlockMode (0xA0);
  control = Inb (CONTROL);

  Outb (CONTROL, (control & 0x1F) | 0x20);	/* reverse */
  Insb (EPPDATA, (unsigned char *) (dest + size - 1), 1);

  control = Inb (CONTROL);
  Outb (CONTROL, (control & 0x1F));	/* forward */
}


/* block transfer init */
static void
ECPSetBuffer (int size)
{
  static int last = 0;
  unsigned char breg;

  /* routine XX */
  ECPCompatMode ();

  /* we set size only if it has changed */
  /* from last time        */
  if (size == last)
    return;
  last = size;

  /* mode and size setting */
  Outb (ECR, 0x60);		/* fifo mode */
  if (WaitFifoEmpty () == 0)
    {
      DBG (0, "ECPSetBuffer failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);

  Outb (DATA, 0x0E);
  if (WaitFifoEmpty () == 0)
    {
      DBG (0, "ECPSetBuffer failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);

  Outb (ECPDATA, 0x0B);		/* R0E=0x0B */
  if (WaitFifoEmpty () == 0)
    {
      DBG (0, "ECPSetBuffer failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);

  Outb (DATA, 0x0F);		/* R0F=size MSB */
  if (WaitFifoEmpty () == 0)
    {
      DBG (0, "ECPSetBuffer failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);

  Outb (ECPDATA, size / 256);
  if (WaitFifoEmpty () == 0)
    {
      DBG (0, "ECPSetBuffer failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);

  Outb (DATA, 0x0B);		/* R0B=size LSB */
  if (WaitFifoEmpty () == 0)
    {
      DBG (0, "ECPSetBuffer failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);

  Outb (ECPDATA, size % 256);
  if (WaitFifoEmpty () == 0)
    {
      DBG (0, "ECPSetBuffer failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);
  DBG (16, "ECPSetBuffer(%d) passed ...\n",size);
}



static int
ECPBufferRead (int size, unsigned char *dest)
{
  int breg, n, idx, remain;

  idx = 0;
  n = size / 16;
  remain = size - 16 * n;

  /* block transfer */
  breg = Inb (ECR);		/* 0x15,0x75 expected: fifo empty */

  Outb (ECR, 0x20);		/* byte mode */
  Outb (CONTROL, 0x04);

  Outb (ECR, 0x60);		/* FIFO mode */
  if (WaitFifoEmpty () == 0)
    {
      DBG (0, "ECPBufferRead failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return idx;
    }
  breg = Inb (ECR);

  Outb (DATA, 0x80);
  if (WaitFifoEmpty () == 0)
    {
      DBG (0, "ECPBufferRead failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return idx;
    }
  breg = Inb (ECR);		/* 0x75 expected */

  Outb (ECR, 0x20);		/* byte mode */
  Outb (CONTROL, 0x20);		/* data reverse */
  Outb (ECR, 0x60);		/* fifo mode */

  while (n > 0)
    {
      if (WaitFifoFull () == 0)
	{
	  DBG (0,
	       "ECPBufferRead failed, time-out waiting for FIFO idx=%d (%s:%d)\n",
	       idx, __FILE__, __LINE__);
	  return idx;
	}
      Insb (ECPDATA, dest + idx, 16);
      idx += 16;
      n--;
    }

  /* reading trailing bytes */
  while (remain > 0)
    {
      if (WaitFifoNotEmpty () == 0)
	{
	  DBG (0, "ECPBufferRead failed, FIFO time-out (%s:%d)\n",
	       __FILE__, __LINE__);
	}
      dest[idx] = Inb (ECPDATA);
      idx++;
      remain--;
    }

  return idx;
}

static void
EPPBufferWrite (int size, unsigned char *source)
{
#ifdef HAVE_LINUX_PPDEV_H
  int fd, mode, rc;
  unsigned char bval;
#endif


#ifdef HAVE_LINUX_PPDEV_H
  /* check we have ppdev working */
  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {
      bval = 0xC0;
      mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = write (fd, &bval, 1);

      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = write (fd, source, size);
      return;
    }
  /* if not, direct hardware access */
#endif
  EPPBlockMode (0xC0);
  Outsb (EPPDATA, source, size);
}

static void
ECPBufferWrite (int size, unsigned char *source)
{
  unsigned char breg;
  int n, idx;

  /* until we know to handle that case, fail */
  if (size % 16 != 0)
    {
      DBG (0, "ECPBufferWrite failed, size %%16 !=0 (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }

  /* prepare actual transfer */
  Outb (ECR, 0x04);		/* compat mode */
  Outb (CONTROL, 0x04);
  breg = Inb (CONTROL);
  Outb (CONTROL, 0x04);		/* data forward */
  Outb (ECR, 0x60);		/* ECP FIFO mode */
  if (WaitFifoEmpty () == 0)
    {
      DBG (0, "ECPWriteBuffer failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);
  breg = (Inb (STATUS)) & 0xF8;
  n = 0;
  while ((n < 1024) && (breg != 0xF8))
    {
      breg = (Inb (STATUS)) & 0xF8;
      n++;
    }
  if (breg != 0xF8)
    {
      DBG (0,
	   "ECPBufferWrite failed, expected status=0xF8, got 0x%02X (%s:%d)\n",
	   breg, __FILE__, __LINE__);
      return;
    }

  /* wait for FIFO empty (bit 0) */
  if (WaitFifoEmpty () == 0)
    {
      DBG (0, "ECPBufferWrite failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);

  /* block transfer direction 
   * 0x80 means from scanner to PC, 0xC0 means PC to scanner 
   */
  Outb (DATA, 0xC0);

  n = size / 16;
  idx = 0;
  while (n > 0)
    {
      /* wait for FIFO empty */
      if (WaitFifoEmpty () == 0)
	{
	  DBG (0,
	       "ECPBufferWrite failed, time-out waiting for FIFO (%s:%d)\n",
	       __FILE__, __LINE__);
	  return;
	}
      breg = Inb (ECR);

      Outsb (ECPDATA, source + idx * 16, 16);
      idx++;
      n--;
    }


  /* final FIFO check and go to Byte mode */
  if (WaitFifoEmpty () == 0)
    {
      DBG (0, "ECPBufferWrite failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);
  ECPByteMode ();
}

static void
BufferWrite (int size, unsigned char *source)
{
  switch (gMode)
    {
    case UMAX_PP_PARPORT_PS2:
      PS2BufferWrite (size, source);
      DBG (0, "STEF: gMode PS2 in BufferWrite !!\n");
      break;
    case UMAX_PP_PARPORT_ECP:
      ECPBufferWrite (size, source);
      break;
    case UMAX_PP_PARPORT_EPP:
      switch (GetEPPMode ())
	{
	case 32:
	  EPPWrite32Buffer (size, source);
	  break;
	default:
	  EPPBufferWrite (size, source);
	  break;
	}
      break;
    default:
      DBG (0, "STEF: gMode PS2 in BufferWrite !!\n");
      break;
    }
  return;
}

static void
BufferRead (int size, unsigned char *dest)
{
  switch (gMode)
    {
    case UMAX_PP_PARPORT_PS2:
      PS2BufferRead (size, dest);
      DBG (0, "STEF: gMode PS2 in BufferRead !!\n");
      break;
    case UMAX_PP_PARPORT_ECP:
      ECPBufferRead (size, dest);
      break;
    case UMAX_PP_PARPORT_EPP:
      switch (GetEPPMode ())
	{
	case 32:
	  EPPRead32Buffer (size, dest);
	  break;
	default:
	  EPPBufferRead (size, dest);
	  break;
	}
      break;
    default:
      DBG (0, "STEF: gMode unset in BufferRead !!\n");
      break;
    }
  return;
}

static int
Connect (void)
{
  switch (gMode)
    {
    case UMAX_PP_PARPORT_PS2:
      DBG (0, "STEF: unimplemented gMode PS2 in Connect() !!\n");
      return 0;
      break;
    case UMAX_PP_PARPORT_ECP:
      return ECPConnect ();
      break;
    case UMAX_PP_PARPORT_EPP_HALF:
      DBG (0, "STEF: unimplemented gMode EPP_HALF in Connect() !!\n");
      return 0;
      break;
    case UMAX_PP_PARPORT_EPP:
      return EPPConnect ();
    default:
      DBG (0, "STEF: gMode unset in Connect() !!\n");
      break;
    }
  return 0;
}


static void
Disconnect (void)
{
  switch (gMode)
    {
    case UMAX_PP_PARPORT_PS2:
      DBG (0, "STEF: unimplemented gMode PS2 in Disconnect() !!\n");
      break;
    case UMAX_PP_PARPORT_ECP:
      ECPDisconnect ();
      break;
    case UMAX_PP_PARPORT_EPP_HALF:
      DBG (0, "STEF: unimplemented gMode EPP_HALF in Disconnect() !!\n");
      break;
    case UMAX_PP_PARPORT_EPP:
      EPPDisconnect ();
      break;
    default:
      DBG (0, "STEF: gMode unset in Disconnect() !!\n");
      break;
    }
}


/* returns 0 if mode OK, else -1 */
static int
CheckEPAT (void)
{
  int version;

  version = RegisterRead (0x0B);
  if (version == 0xC7)
    return 0;
  DBG (0, "CheckEPAT: expected EPAT version 0xC7, got 0x%X! (%s:%d)\n",
       version, __FILE__, __LINE__);
  return -1;

}

static int
Init005 (int arg)
{
  int count = 5;
  int res;

  while (count > 0)
    {
      RegisterWrite (0x0A, arg);
      Outb (DATA, 0xFF);
      res = RegisterRead (0x0A);

      /* failed ? */
      if (res != arg)
	return 1;

      /* ror arg */
      res = arg & 0x01;
      arg = arg / 2;
      if (res == 1)
	arg = arg | 0x80;

      /* next loop */
      count--;
    }
  return 0;
}

/* 1 OK, 0 failure */
static int
EPPConnect (void)
{
  int control;
  int data;

  /* initial values, don't hardcode */
  Outb (DATA, 0x04);
  Outb (CONTROL, 0x0C);

  data = Inb (DATA);
  control = Inb (CONTROL);
  Outb (CONTROL, control & 0x1F);
  control = Inb (CONTROL);
  Outb (CONTROL, control & 0x1F);

  if (SendCommand (0xE0) != 1)
    {
      DBG (0, "EPPConnect: SendCommand(0xE0) failed! (%s:%d)\n", __FILE__,
	   __LINE__);
      return 0;
    }
  ClearRegister (0);
  Init001 ();
  return 1;
}



static void
EPPRead32Buffer (int size, unsigned char *dest)
{
#ifdef HAVE_LINUX_PPDEV_H
  int fd, mode, rc, nb;
  unsigned char bval;
#endif
  int control;

#ifdef HAVE_LINUX_PPDEV_H
  /* check we have ppdev working */
  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {

      bval = 0x80;
      mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = write (fd, &bval, 1);

      mode = 1;			/* data_reverse */
      rc = ioctl (fd, PPDATADIR, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
#ifdef PPSETFLAGS
      mode = PP_FASTREAD;
      rc = ioctl (fd, PPSETFLAGS, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
#endif
      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      nb = 0;
      while (nb < size - 4)
	{
	  rc = read (fd, dest + nb, size - 4 - nb);
	  nb += rc;
	}

      rc = read (fd, dest + size - 4, 3);

      mode = 0;			/* forward */
      rc = ioctl (fd, PPDATADIR, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      bval = 0xA0;
      mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = write (fd, &bval, 1);

      mode = 1;			/* data_reverse */
      rc = ioctl (fd, PPDATADIR, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = read (fd, dest + size - 1, 1);

      mode = 0;			/* forward */
      rc = ioctl (fd, PPDATADIR, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);

      return;
    }
  /* if not, direct hardware access */
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
  int fd, mode, rc;
  unsigned char bval;
#endif

  if ((size % 4) != 0)
    {
      DBG (0, "EPPWrite32Buffer: size %% 4 != 0!! (%s:%d)\n", __FILE__,
	   __LINE__);
    }
#ifdef HAVE_LINUX_PPDEV_H
  /* check we have ppdev working */
  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {

      bval = 0xC0;
      mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = write (fd, &bval, 1);

#ifdef PPSETFLAGS
      mode = PP_FASTWRITE;
      rc = ioctl (fd, PPSETFLAGS, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
#endif
      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = write (fd, source, size);

      return;
    }
  /* if not, direct hardware access */
#endif
  EPPBlockMode (0xC0);
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
  return c;
}



#ifdef HAVE_LINUX_PPDEV_H
/* read up to size bytes, returns bytes read */
static int
ParportPausedBufferRead (int size, unsigned char *dest)
{
  unsigned char status, bval;
  int error;
  int word;
  int bread;
  int c;
  int fd, rc, mode;

  /* WIP check */
  if (gMode == UMAX_PP_PARPORT_ECP)
    {
      DBG (0, "ECP access not implemented yet (WIP) ! (%s:%d)\n",
	   __FILE__, __LINE__);
    }

  /* init */
  bread = 0;
  error = 0;
  fd = sanei_umax_pp_getparport ();

  mode = 1;			/* data_reverse */
  rc = ioctl (fd, PPDATADIR, &mode);
  if (rc)
    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	 __FILE__, __LINE__);
#ifdef PPSETFLAGS
  mode = PP_FASTREAD;
  rc = ioctl (fd, PPSETFLAGS, &mode);
  if (rc)
    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	 __FILE__, __LINE__);
#endif
  mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
  rc = ioctl (fd, PPSETMODE, &mode);
  if (rc)
    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	 __FILE__, __LINE__);

  if ((size & 0x03) != 0)
    {
      while ((!error) && ((size & 0x03) != 0))
	{
	  rc = read (fd, dest, 1);
	  size--;
	  dest++;
	  bread++;
	  rc = ioctl (fd, PPRSTATUS, &status);
	  if (rc)
	    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  error = status & 0x08;
	}
      if (error)
	{
	  DBG (0, "Read error (%s:%d)\n", __FILE__, __LINE__);
	  return 0;
	}
    }

  /* from here, we read 1 byte, then size/4-1 32 bits words, and then
     3 bytes, pausing on ERROR bit of STATUS */
  size -= 4;

  /* sanity test, seems to be wrongly handled ... */
  if (size == 0)
    {
      DBG (0, "case not handled! (%s:%d)\n", __FILE__, __LINE__);
      return 0;
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
	      if (rc)
		DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n",
		     strerror (errno), __FILE__, __LINE__);
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
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
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
		  if (rc)
		    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n",
			 strerror (errno), __FILE__, __LINE__);
		  error = status & 0x08;
		  if (!error)
		    {
		      rc = ioctl (fd, PPRSTATUS, &status);
		      if (rc)
			DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n",
			     strerror (errno), __FILE__, __LINE__);
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
  if (rc)
    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	 __FILE__, __LINE__);
  bval = 0xA0;
  mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
  rc = ioctl (fd, PPSETMODE, &mode);
  if (rc)
    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	 __FILE__, __LINE__);
  rc = write (fd, &bval, 1);

  mode = 1;			/* data_reverse */
  rc = ioctl (fd, PPDATADIR, &mode);
  if (rc)
    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	 __FILE__, __LINE__);
  mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
  rc = ioctl (fd, PPSETMODE, &mode);
  if (rc)
    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	 __FILE__, __LINE__);
  rc = read (fd, dest, 1);
  bread++;

  mode = 0;			/* forward */
  rc = ioctl (fd, PPDATADIR, &mode);
  if (rc)
    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	 __FILE__, __LINE__);
  return bread;
}
#endif


/* read up to size bytes, returns bytes read */
static int
DirectPausedBufferRead (int size, unsigned char *dest)
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
	  return 0;
	}
    }

  /* from here, we read 1 byte, then size/4-1 32 bits words, and then
     3 bytes, pausing on ERROR bit of STATUS */
  size -= 4;

  /* sanity test, seems to be wrongly handled ... */
  if (size == 0)
    {
      DBG (0, "case not handled! (%s:%d)\n", __FILE__, __LINE__);
      return 0;
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
  return read;
}


int
PausedBufferRead (int size, unsigned char *dest)
{
  EPPBlockMode (0x80);
#ifdef HAVE_LINUX_PPDEV_H
  if (sanei_umax_pp_getparport () > 0)
    return ParportPausedBufferRead (size, dest);
#endif
  /* only EPP hardware access for now */
  if (gMode == UMAX_PP_PARPORT_EPP)
    return DirectPausedBufferRead (size, dest);
  return 0;
}




/* returns 1 on success, 0 otherwise */
static int
SendWord1220P (int *cmd)
{
  int i;
  int reg;
  int try = 0;

  /* send header */
  reg = RegisterRead (0x19) & 0xF8;
retry:
  RegisterWrite (0x1C, 0x55);
  reg = RegisterRead (0x19) & 0xF8;

  RegisterWrite (0x1C, 0xAA);
  reg = RegisterRead (0x19) & 0xF8;

  /* sync when needed */
  if ((reg & 0x08) == 0x00)
    {
      reg = RegisterRead (0x1C);
      DBG (16, "UTA: reg1C=0x%02X   (%s:%d)\n", reg, __FILE__, __LINE__);
      if (((reg & 0x10) != 0x10) && (reg != 0x6B) && (reg != 0xAB)
	  && (reg != 0x23))
	{
	  DBG (0, "SendWord failed (reg1C=0x%02X)   (%s:%d)\n", reg, __FILE__,
	       __LINE__);
	  return 0;
	}
      for (i = 0; i < 10; i++)
	{
	  usleep (1000);
	  reg = RegisterRead (0x19) & 0xF8;
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
	  reg = RegisterRead (0x19) & 0xF8;
	}
      while (reg != 0xC8);
    }

  /* send word */
  i = 0;
  while ((reg == 0xC8) && (cmd[i] != -1))
    {
      RegisterWrite (0x1C, cmd[i]);
      i++;
      reg = RegisterRead (0x19) & 0xF8;
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
      return 0;
    }
  reg = RegisterRead (0x1C);
  DBG (16, "SendWord, reg1C=0x%02X (%s:%d)\n", reg, __FILE__, __LINE__);
  /* model 0x07 has always the last bit set to 1, and even bit 1 */
  /* when UTA is present, we get 0x6B there */
  scannerStatus = reg & 0xFC;
  if (scannerStatus == 0x68)
    hasUTA = 1;

  reg = reg & 0x10;
  if ((reg != 0x10) && (scannerStatus != 0x68) && (scannerStatus != 0xA8))
    {
      DBG (0, "SendWord failed: acknowledge not received (%s:%d)\n", __FILE__,
	   __LINE__);
      return 0;
    }
  if (try)
    {
      DBG (0, "SendWord retry success (retry %d time%s) ... (%s:%d)\n", try,
	   (try > 1) ? "s" : "", __FILE__, __LINE__);
    }
  return 1;
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
      return 0;
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
      return 0;
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
	  return 0;
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
  return 1;
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
      return SendWord610P (cmd);
    case 1220:
    case 1600:
    case 2000:
    default:
      return SendWord1220P (cmd);
    }
}



/******************************************************************************/
/* RingScanner: returns 1 if scanner present, else 0                          */
/******************************************************************************/
/* 
 * in fact this function is really close to CPP macro in
 * /usr/src/linux/drivers/block/paride/epat.c .....
 * we have almost CPP(8)
 */

static int
RingScanner (int count, unsigned long delay)
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
      return 0;
    }

  /* send ring string */
  Outb (DATA, 0x22);
  usleep (delay);
  Outb (DATA, 0x22);
  usleep (delay);
  if (count == 5)
    {
      Outb (DATA, 0x22);
      usleep (delay);
      Outb (DATA, 0x22);
      usleep (delay);
      Outb (DATA, 0x22);
      usleep (delay);
    }
  Outb (DATA, 0xAA);
  usleep (delay);
  Outb (DATA, 0xAA);
  usleep (delay);
  if (count == 5)
    {
      Outb (DATA, 0xAA);
      usleep (delay);
      Outb (DATA, 0xAA);
      usleep (delay);
      Outb (DATA, 0xAA);
      usleep (delay);
    }
  Outb (DATA, 0x55);
  usleep (delay);
  Outb (DATA, 0x55);
  usleep (delay);
  if (count == 5)
    {
      Outb (DATA, 0x55);
      usleep (delay);
      Outb (DATA, 0x55);
      usleep (delay);
      Outb (DATA, 0x55);
      usleep (delay);
    }
  Outb (DATA, 0x00);
  usleep (delay);
  Outb (DATA, 0x00);
  usleep (delay);
  if (count == 5)
    {
      Outb (DATA, 0x00);
      usleep (delay);
      Outb (DATA, 0x00);
      usleep (delay);
      Outb (DATA, 0x00);
      usleep (delay);
    }
  Outb (DATA, 0xFF);
  usleep (delay);
  Outb (DATA, 0xFF);
  usleep (delay);
  if (count == 5)
    {
      Outb (DATA, 0xFF);
      usleep (delay);
      Outb (DATA, 0xFF);
      usleep (delay);
      Outb (DATA, 0xFF);
      usleep (delay);
    }

  /* OK ? */
  status = Inb (STATUS);
  usleep (delay);
  if ((status & 0xB8) != 0xB8)
    {
      DBG (1, "status %d doesn't match! %s:%d\n", status, __FILE__, __LINE__);
      ret = 0;
    }

  /* if OK send 0x87 */
  if (ret)
    {
      Outb (DATA, 0x87);
      usleep (delay);
      Outb (DATA, 0x87);
      usleep (delay);
      if (count == 5)
	{
	  Outb (DATA, 0x87);
	  usleep (delay);
	  Outb (DATA, 0x87);
	  usleep (delay);
	  Outb (DATA, 0x87);
	  usleep (delay);
	}
      status = Inb (STATUS);
      /* status = 126 when scanner not connected .... */
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
      usleep (delay);
      Outb (DATA, 0x78);
      usleep (delay);
      if (count == 5)
	{
	  Outb (DATA, 0x78);
	  usleep (delay);
	  Outb (DATA, 0x78);
	  usleep (delay);
	  Outb (DATA, 0x78);
	  usleep (delay);
	}
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
      usleep (delay);
      Outb (DATA, 0x08);
      usleep (delay);
      if (count == 5)
	{
	  Outb (DATA, 0x08);
	  usleep (delay);
	  Outb (DATA, 0x08);
	  usleep (delay);
	  Outb (DATA, 0x08);
	  usleep (delay);
	}
      Outb (DATA, 0xFF);
      usleep (delay);
      Outb (DATA, 0xFF);
      usleep (delay);
      if (count == 5)
	{
	  Outb (DATA, 0xFF);
	  usleep (delay);
	  Outb (DATA, 0xFF);
	  usleep (delay);
	  Outb (DATA, 0xFF);
	  usleep (delay);
	}
    }

  /* restore state */
  Outb (CONTROL, control);
  Outb (DATA, data);
  return ret;
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
	  return 0;
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
  wait = RegisterRead (0x19) & 0xF8;

  RegisterWrite (0x1C, 0x55);
  reg = RegisterRead (0x19) & 0xF8;

  RegisterWrite (0x1C, 0xAA);
  reg = RegisterRead (0x19) & 0xF8;

  /* sync when needed */
  if ((wait & 0x08) == 0x00)
    {
      reg = RegisterRead (0x1C);
      while (((reg & 0x10) != 0x10) && (reg != 0x6B) && (reg != 0xAB)
	     && (reg != 0x23))
	{
	  DBG (0,
	       "SendLength failed, expected reg & 0x10=0x10 , found 0x%02X (%s:%d)\n",
	       reg, __FILE__, __LINE__);
	  if (try > 10)
	    {
	      DBG (0, "Aborting...\n");
	      return 0;
	    }
	  else
	    {
	      DBG (0, "Retrying ...\n");
	    }
	  /* resend */
	  Epilogue ();
	  Prologue (0x10);
	  try++;
	  goto retry;
	}
      for (i = 0; i < 10; i++)
	{
	  reg = RegisterRead (0x19) & 0xF8;
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
		      return 0;
		    }

		  Epilogue ();
		  SendCommand (0x00);
		  SendCommand (0xE0);
		  Outb (DATA, 0x00);
		  Outb (CONTROL, 0x01);
		  Outb (CONTROL, 0x04);
		  SendCommand (0x30);

		  Prologue (0x10);
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

	      Prologue (0x10);
	      goto retry;
	    }
	  reg = RegisterRead (0x19) & 0xF8;
	}
      while (reg != 0xC8);
    }

  /* send bytes */
  i = 0;
  while ((reg == 0xC8) && (i < len))
    {
      /* write byte */
      RegisterWrite (0x1C, cmd[i]);
      reg = RegisterRead (0x19) & 0xF8;

      /* 1B handling: escape it to confirm value  */
      if (cmd[i] == 0x1B)
	{
	  RegisterWrite (0x1C, cmd[i]);
	  reg = RegisterRead (0x19) & 0xF8;
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
      return 0;
    }


  reg = RegisterRead (0x1C);
  DBG (16, "SendLength, reg1C=0x%02X (%s:%d)\n", reg, __FILE__, __LINE__);

  /* model 0x07 has always the last bit set to 1 */
  scannerStatus = reg & 0xFC;
  reg = reg & 0x10;
  if ((reg != 0x10) && (scannerStatus != 0x68) && (scannerStatus != 0xA8))
    {
      DBG (0, "SendLength failed: acknowledge not received (%s:%d)\n",
	   __FILE__, __LINE__);
      return 0;
    }
  if (try)
    {
      DBG (0, "SendLength retry success (retry %d time%s) ... (%s:%d)\n", try,
	   (try > 1) ? "s" : "", __FILE__, __LINE__);
    }
  return 1;
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
  reg = RegisterRead (0x19) & 0xF8;

  /* send bytes */
  i = 0;
  while ((reg == 0xC8) && (i < len))
    {
      /* write byte */
      RegisterWrite (0x1C, cmd[i]);
      reg = RegisterRead (0x19) & 0xF8;

      /* 1B handling: escape it to confirm value  */
      if (cmd[i] == 0x1B)
	{
	  RegisterWrite (0x1C, 0x1B);
	  reg = RegisterRead (0x19) & 0xF8;
	}

      /* escape 55 AA pattern by adding 1B */
      if ((i < len - 1) && (cmd[i] == 0x55) && (cmd[i + 1] == 0xAA))
	{
	  RegisterWrite (0x1C, 0x1B);
	  reg = RegisterRead (0x19) & 0xF8;
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
      return 0;
    }


  reg = RegisterRead (0x1C);
  DBG (16, "SendData, reg1C=0x%02X (%s:%d)\n", reg, __FILE__, __LINE__);

  /* model 0x07 has always the last bit set to 1 */
  scannerStatus = reg & 0xFC;
  reg = reg & 0x10;
  if ((reg != 0x10) && (scannerStatus != 0x68) && (scannerStatus != 0xA8)
      && (scannerStatus != 0x20))
    {
      DBG (0, "SendData failed: acknowledge not received (%s:%d)\n", __FILE__,
	   __LINE__);
      return 0;
    }
  return 1;
}


/* receive data bytes from scanner   */
/* needs data channel to be set up   */
/* returns 1 on success, 0 otherwise */
/* uses PausedBufferRead             */
static int
PausedReadData (int size, unsigned char *dest)
{
  int reg;
  int tmp;
  int read;

  REGISTERWRITE (0x0E, 0x0D);
  REGISTERWRITE (0x0F, 0x00);
  reg = RegisterRead (0x19) & 0xF8;
  if ((reg != 0xC0) && (reg != 0xD0))
    {
      DBG (0, "Unexpected reg19: 0x%02X instead of 0xC0 or 0xD0 (%s:%d)\n",
	   reg, __FILE__, __LINE__);
      return 0;
    }
  if (gMode == UMAX_PP_PARPORT_ECP)
    {
      REGISTERWRITE (0x1A, 0x44);
    }
  REGISTERREAD (0x0C, 0x04);
  REGISTERWRITE (0x0C, 0x44);	/* sets data direction ? */
  if (gMode == UMAX_PP_PARPORT_ECP)
    {
      ECPCompatMode ();
      ECPSetBuffer (size);
      read = ECPBufferRead (size, dest);
      DBG (16, "ECPBufferRead(%d,dest) passed (%s:%d)\n", size, __FILE__,
	   __LINE__);
      REGISTERWRITE (0x1A, 0x84);
    }
  else
    {
      read = PausedBufferRead (size, dest);
    }
  if (read < size)
    {
      DBG (16,
	   "PausedBufferRead(%d,dest) failed, only got %d bytes (%s:%d)\n",
	   size, read, __FILE__, __LINE__);
      return 0;
    }
  DBG (16, "PausedBufferRead(%d,dest) passed (%s:%d)\n", size, __FILE__,
       __LINE__);
  REGISTERWRITE (0x0E, 0x0D);
  REGISTERWRITE (0x0F, 0x00);
  return 1;
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
  reg = RegisterRead (0x19) & 0xF8;

  /* send bytes */
  i = 0;
  while (((reg == 0xD0) || (reg == 0xC0)) && (i < len))
    {
      /* write byte */
      cmd[i] = RegisterRead (0x1C);
      reg = RegisterRead (0x19) & 0xF8;
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
      return 0;
    }


  reg = RegisterRead (0x1C);
  DBG (16, "ReceiveData, reg1C=0x%02X (%s:%d)\n", reg, __FILE__, __LINE__);

  /* model 0x07 has always the last bit set to 1 */
  scannerStatus = reg & 0xF8;
  reg = reg & 0x10;
  if ((reg != 0x10) && (scannerStatus != 0x68) && (scannerStatus != 0xA8))
    {
      DBG (0, "ReceiveData failed: acknowledge not received (%s:%d)\n",
	   __FILE__, __LINE__);
      return 0;
    }
  return 1;
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
      RegisterWrite (0x1A, 0x0C);
      RegisterWrite (0x18, 0x40);

      /* send 0x06 */
      RegisterWrite (0x1A, 0x06);
      for (i = 0; i < 10; i++)
	{
	  reg = RegisterRead (0x19) & 0xF8;
	  if ((reg & 0x78) == 0x38)
	    {
	      res = 0;
	      break;
	    }
	}
      if (res == 1)
	{
	  RegisterWrite (0x1A, 0x00);
	  RegisterWrite (0x1A, 0x0C);
	}
    }

  /* send 0x07 */
  RegisterWrite (0x1A, 0x07);
  res = 1;
  for (i = 0; i < 10; i++)
    {
      reg = RegisterRead (0x19) & 0xF8;
      if ((reg & 0x78) == 0x38)
	{
	  res = 0;
	  break;
	}
    }
  if (res != 0)
    return 0;

  /* send 0x04 */
  RegisterWrite (0x1A, 0x04);
  res = 1;
  for (i = 0; i < 10; i++)
    {
      reg = RegisterRead (0x19) & 0xF8;
      if ((reg & 0xF8) == 0xF8)
	{
	  res = 0;
	  break;
	}
    }
  if (res != 0)
    return 0;

  /* send 0x05 */
  RegisterWrite (0x1A, 0x05);
  res = 1;
  for (i = 0; i < 10; i++)
    {
      reg = RegisterRead (0x1A);
      if (reg == 0x05)
	{
	  res = 0;
	  break;
	}
    }
  if (res != 0)
    return 0;

  /* end */
  RegisterWrite (0x1A, 0x84);
  return 1;
}







/* 1 OK, 0 failed */
static int
FoncSendWord (int *cmd)
{
  Prologue (0x10);
  if (SendWord (cmd) == 0)
    {
      DBG (0, "SendWord(cmd) failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  Epilogue ();

  return 1;
}


static int
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
      return 0;
    }
  TRACE (16, "SendData(data,2048) passed ...");

  /* read back all data sent to 'channel 4' */
  SendWord (cmd4);
  DBG (16, "SendWord(cmd4) passed (%s:%d) \n", __FILE__, __LINE__);

  if (PausedReadData (2048, dest) == 0)
    {
      DBG (16, "PausedReadData(2048,dest) failed (%s:%d)\n", __FILE__,
	   __LINE__);
      return 0;
    }
  DBG (16, "PausedReadData(2048,dest) passed (%s:%d)\n", __FILE__, __LINE__);

  /* dest should hold the same data than donnees */
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
  return 1;
}




/* free scanner and parallel port
 0: failure
 1: success 
*/
int
sanei_umax_pp_ReleaseScanner (void)
{
  int reg;

  REGISTERWRITE (0x0A, 0x00);
  reg = RegisterRead (0x0D);
  reg = (reg & 0xBF);
  RegisterWrite (0x0D, reg);
  if (gMode == UMAX_PP_PARPORT_ECP)
    return 1;
  if (GetModel () != 0x07)
    {
      if (SendCommand (0x40) == 0)
	{
	  DBG (0, "SendCommand(0x40) (%s:%d) failed ...\n", __FILE__,
	       __LINE__);
	  return 0;
	}
    }
  if (SendCommand (0x30) == 0)
    {
      DBG (0, "SendCommand(0x30) (%s:%d) failed ...\n", __FILE__, __LINE__);
      return 0;
    }
  DBG (1, "ReleaseScanner() done ...\n");

  return 1;
}



/* 1: OK
   0: end session failed */

/* 1: OK
   0: end session failed */

int
sanei_umax_pp_EndSession (void)
{
  int zero[5] = { 0, 0, 0, 0, -1 };

  Prologue (0x00);
  SendWord (zero);
  Epilogue ();
  sanei_umax_pp_CmdSync (0xC2);
  sanei_umax_pp_CmdSync (0x00);	/* cancels any pending operation */
  sanei_umax_pp_CmdSync (0x00);	/* cancels any pending operation */
  sanei_umax_pp_ReleaseScanner ();

  /* restore port state */
  Outb (DATA, gData);
  Outb (CONTROL, gControl);

  /* OUF */
  DBG (1, "End session done ...\n");
  return 1;
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
  /* in umax1220u, this buffer is opc[16] */
  int sentcmd[17] =
    { 0x02, 0x80, 0x00, 0x70, 0x00, 0x00, 0x00, 0x2F, 0x2F, 0x07, 0x00,
    0x00, 0x00, 0x80, 0xF0, 0x00, -1
  };
  int cmdA7[9] = { 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, -1 };


  if (GetModel () == 0x07)
    {
      sentcmd[15] = 0x00;
      j++;
    }
  else
    {
      sentcmd[15] = 0x18;
      j++;
    }

  /* fails here if there is an unfinished previous scan */
  CMDSETGET (0x02, 16, sentcmd);

  /* needs some init */
  if (sentcmd[15] == 0x18)
    {
      sentcmd[15] = 0x00;	/* was 0x18 */
      CMDSETGET (0x02, 16, sentcmd);

      /* in umax1220u, this buffer does not exist */
      CMDSETGET (0x01, 8, cmdA7);
    }


  /* ~ opb3: inquire status */
  CMDGET (0x08, 36, readcmd);
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
      return 3;
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
      /* ECP ICI */
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
      return 2;
    }


  /* end ... */
  DBG (1, "Scanner init done ...\n");
  return 1;
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
      return 0;
    }
  DBG (16, "SendCommand(0xE0) passed...\n");
  g6FE = 1;
  ClearRegister (0);
  DBG (16, "ClearRegister(0) passed...\n");

  /* sync */
  Prologue (0x10);
  if (SendWord (zero) == 0)
    {
      DBG (0, "SendWord(zero) failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  DBG (16, "SendWord(zero) passed (%s:%d)\n", __FILE__, __LINE__);

  Epilogue ();

  /* OK ! */
  DBG (1, "InitTransport610P done ...\n");
  return 1;
}

/* 
        1: OK
           2: failed, try again
           0: init failed 

        initialize the transport layer
   
   */

static int
InitTransport1220P (int recover)	/* ECP OK !! */
{
  int i, j;
  int reg, tmp;
  unsigned char *dest = NULL;
  int zero[5] = { 0, 0, 0, 0, -1 };
  int model, nb;

  /* init cancel handling */
  /* InitCancel(); */

  /* recover is not used yet, but will */
  recover = 0;			/* quit compiler quiet .. */

  Connect ();
  DBG (16, "Connect() passed... (%s:%d)\n", __FILE__, __LINE__);
  gEPAT = 0xC7;
  reg = RegisterRead (0x0B);
  if (reg != gEPAT)
    {
      DBG (16, "Error! expected reg0B=0x%02X, found 0x%02X! (%s:%d) \n",
	   gEPAT, reg, __FILE__, __LINE__);
      DBG (16, "Scanner needs probing ... \n");
      sanei_umax_pp_ReleaseScanner ();
      if (sanei_umax_pp_ProbeScanner (recover) != 1)
	{
	  return 0;
	}
      else
	{
	  sanei_umax_pp_ReleaseScanner ();
	  return 2;		/* signals retry InitTransport() */
	}
    }

  reg = RegisterRead (0x0D);
  reg = (reg & 0xE8) | 0x43;
  RegisterWrite (0x0D, reg);
  REGISTERWRITE (0x0C, 0x04);
  reg = RegisterRead (0x0A);
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
  if (gMode != UMAX_PP_PARPORT_ECP)
    {				/* XXX try to do it in all modes XXX */
      REGISTERWRITE (0x0E, 0x01);
      model = RegisterRead (0x0F);
      SetModel (model);
    }

  REGISTERWRITE (0x0A, 0x1C);
  if (gMode == UMAX_PP_PARPORT_ECP)
    {
      REGISTERWRITE (0x08, 0x10);
    }
  else
    {
      REGISTERWRITE (0x08, 0x21);
    }
  REGISTERWRITE (0x0E, 0x0F);
  REGISTERWRITE (0x0F, 0x0C);

  REGISTERWRITE (0x0A, 0x1C);
  REGISTERWRITE (0x0E, 0x10);
  REGISTERWRITE (0x0F, 0x1C);
  if (gMode == UMAX_PP_PARPORT_ECP)
    {
      REGISTERWRITE (0x0F, 0x00);
    }
  REGISTERWRITE (0x0A, 0x11);

  /* XXX ICI XXX */
  dest = (unsigned char *) (malloc (65536));
  if (dest == NULL)
    {
      DBG (0, "Failed to allocate 64 Ko !\n");
      return 0;
    }
  for (i = 0; i < 256; i++)
    {
      dest[i * 2] = i;
      dest[i * 2 + 1] = 0xFF - i;
      dest[512 + i * 2] = i;
      dest[512 + i * 2 + 1] = 0xFF - i;
    }
  if (gMode == UMAX_PP_PARPORT_ECP)
    {
      nb = 300;
      nb = 150;
    }
  else
    {
      nb = 150;
    }
  for (i = 0; i < nb; i++)
    {
      BufferWrite (0x400, dest);
      DBG (16,
	   "Loop %d: BufferWrite(0x400,dest) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }

  REGISTERWRITE (0x0A, 0x18);
  REGISTERWRITE (0x0A, 0x11);

  for (i = 0; i < nb; i++)
    {
      /* XXX Compat/Byte ??? XXX */
      BufferRead (0x400, dest);
      for (j = 0; j < 256; j++)
	{
	  if (dest[j * 2] != j)
	    {
	      DBG (0,
		   "Altered buffer value at %03X, expected %02X, found %02X\n",
		   j * 2, j, dest[j * 2]);
	      return 0;
	    }
	  if (dest[j * 2 + 1] != 0xFF - j)
	    {
	      DBG
		(0,
		 "Altered buffer value at %03X, expected %02X, found %02X\n",
		 j * 2 + 1, 0xFF - j, dest[j * 2 + 1]);
	      return 0;
	    }
	  if (dest[512 + j * 2] != j)
	    {
	      DBG (0,
		   "Altered buffer value at %03X, expected %02X, found %02X\n",
		   512 + j * 2, j, dest[512 + j * 2]);
	      return 0;
	    }
	  if (dest[512 + j * 2 + 1] != 0xFF - j)
	    {
	      DBG
		(0,
		 "Altered buffer value at %03X, expected 0x%02X, found 0x%02X\n",
		 512 + j * 2 + 1, 0xFF - j, dest[512 + j * 2 + 1]);
	      return 0;
	    }
	}
      DBG (16, "Loop %d: BufferRead(0x400,dest) passed... (%s:%d)\n",
	   i, __FILE__, __LINE__);
    }
  REGISTERWRITE (0x0A, 0x18);
  /* ECP: "HEAVY" reconnect here */
  if (gMode == UMAX_PP_PARPORT_ECP)
    {
      Epilogue ();
      /* 3 line: set to initial parport state ? */
      Outb (ECR, 0x20);
      Outb (DATA, 0x04);
      Outb (CONTROL, 0x0C);

      /* the following is a variant of Connect(); */
      Inb (ECR);
      Inb (ECR);
      Outb (ECR, 0x20);
      Outb (ECR, 0x20);
      Inb (CONTROL);
      Outb (CONTROL, 0x0C);
      Inb (DATA);
      SendCommand (0xE0);
      Outb (DATA, 0XFF);
      Outb (DATA, 0XFF);
      ClearRegister (0);
      WRITESLOW (0x0E, 0x0A);
      SLOWNIBBLEREGISTERREAD (0x0F, 0x08);
      /* resend value OR'ed 0x08 ? */
      WRITESLOW (0x0F, 0x08);
      WRITESLOW (0x08, 0x10);
      Disconnect ();
      Prologue (0x10);
    }

  if (Fonc001 () != 1)
    {
      DBG (0, "Fonc001() failed ! (%s:%d) \n", __FILE__, __LINE__);
      return 0;
    }
  DBG (16, "Fonc001() passed ...  (%s:%d) \n", __FILE__, __LINE__);

  /* sync */
  if (SendWord (zero) == 0)
    {
      DBG (0, "SendWord(zero) failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  DBG (16, "SendWord(zero) passed (%s:%d)\n", __FILE__, __LINE__);
  Epilogue ();

  /* OK ! */
  free (dest);
  DBG (1, "InitTransport1220P done ...\n");
  return 1;
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
      return InitTransport610P (recover);
    case 1220:
    case 1600:
    case 2000:
    default:
      return InitTransport1220P (recover);
    }
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
  return 1;
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
      return 0;
    }
  Outb (CONTROL, 0x04);
  usleep (10000);
  Outb (CONTROL, 0x04);
  usleep (10000);
  return 1;
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
  if (!sanei_umax_pp_getastra ())
    sanei_umax_pp_setastra (610);
  if (!Test610P (0x87))
    {
      DBG (1, "Test610P(0x87) failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  else
    {
      DBG (16, "Test610P(0x87) passed...\n");
    }
  if (!In256 ())
    {
      DBG (1, "In256() failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  else
    {
      DBG (16, "In256() passed...\n");
    }


  /* test PS2 mode */

  tmp = PS2RegisterRead (0x0B);
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
  return 1;
}


  /* 
   * try PS2 mode
   * returns 1 on success, 0 on failure
   */
int
ProbePS2 (unsigned char *dest)
{
  int i, tmp;

  /* write/read full buffer */
  for (i = 0; i < 256; i++)
    {
      WRITESLOW (0x0A, i);
      SLOWNIBBLEREGISTERREAD (0x0A, i);
      WRITESLOW (0x0A, 0xFF - i);
      SLOWNIBBLEREGISTERREAD (0x0A, 0xFF - i);
    }

  /* end test for nibble byte/byte mode */

  /* now we try nibble buffered mode */
  WRITESLOW (0x13, 0x01);
  WRITESLOW (0x13, 0x00);	/*reset something */
  WRITESLOW (0x0A, 0x11);
  for (i = 0; i < 10; i++)	/* 10 ~ 11 ? */
    {
      PS2BufferRead (0x400, dest);
      DBG (16, "Loop %d: PS2BufferRead passed ... (%s:%d)\n", i, __FILE__,
	   __LINE__);
    }

  /* write buffer */
  for (i = 0; i < 10; i++)
    {
      PS2BufferWrite (0x400, dest);
      DBG (16, "Loop %d: PS2BufferWrite passed ... (%s:%d)\n", i, __FILE__,
	   __LINE__);
    }

  SLOWNIBBLEREGISTERREAD (0x0C, 0x04);
  WRITESLOW (0x13, 0x01);
  WRITESLOW (0x13, 0x00);
  WRITESLOW (0x0A, 0x18);

  return 1;
}

  /*
   * try EPP 8 then 32 bits
   * returns 1 on success, 0 on failure
   */
int
ProbeEPP (unsigned char *dest)
{
  int tmp, i, j;
  int reg;

  /* test EPP MODE */
  SetEPPMode (8);
  gMode = UMAX_PP_PARPORT_EPP;
  ClearRegister (0);
  DBG (16, "ClearRegister(0) passed... (%s:%d)\n", __FILE__, __LINE__);
  WRITESLOW (0x08, 0x22);
  Init001 ();
  DBG (16, "Init001() passed... (%s:%d)\n", __FILE__, __LINE__);
  gEPAT = 0xC7;
  Init002 (0);
  DBG (16, "Init002(0) passed... (%s:%d)\n", __FILE__, __LINE__);

  REGISTERWRITE (0x0A, 0);

  /* catch any failure to read back data in EPP mode */
  reg = RegisterRead (0x0A);
  if (reg != 0)
    {
      DBG (0, "RegisterRead, found 0x%X expected 0x00 (%s:%d)\n", reg,
	   __FILE__, __LINE__);
      if (reg == 0xFF)
	{
	  DBG (0,
	       "*** It appears that EPP data transfer doesn't work    ***\n");
	  DBG (0,
	       "*** Please read SETTING EPP section in sane-umax_pp.5 ***\n");
	}
      return 0;
    }
  else
    {
      DBG (16, "RegisterRead(0x0A)=0x00 passed... (%s:%d)\n", __FILE__,
	   __LINE__);
    }
  RegisterWrite (0x0A, 0xFF);
  DBG (16, "RegisterWrite(0x%X,0x%X) passed...   (%s:%d)\n", 0x0A, 0xFF,
       __FILE__, __LINE__);
  REGISTERREAD (0x0A, 0xFF);
  for (i = 1; i < 256; i++)
    {
      REGISTERWRITE (0x0A, i);
      REGISTERREAD (0x0A, i);
      REGISTERWRITE (0x0A, 0xFF - i);
      REGISTERREAD (0x0A, 0xFF - i);
    }

  REGISTERWRITE (0x13, 0x01);
  REGISTERWRITE (0x13, 0x00);
  REGISTERWRITE (0x0A, 0x11);

  for (i = 0; i < 10; i++)
    {
      BufferRead (0x400, dest);
      for (j = 0; j < 512; j++)
	{
	  if (dest[2 * j] != (j % 256))
	    {
	      DBG (0, "Loop %d, char %d BufferRead failed! (%s:%d)\n", i,
		   j * 2, __FILE__, __LINE__);
	      return 0;
	    }
	  if (dest[2 * j + 1] != (0xFF - (j % 256)))
	    {
	      DBG (0, "Loop %d, char %d BufferRead failed! (%s:%d)\n", i,
		   j * 2 + 1, __FILE__, __LINE__);
	      return 0;
	    }
	}
      DBG (16, "Loop %d: BufferRead(0x400,dest) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }

  for (i = 0; i < 10; i++)
    {
      BufferWrite (0x400, dest);
      DBG (16, "Loop %d: BufferWrite(0x400,dest) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }


  REGISTERREAD (0x0C, 4);
  REGISTERWRITE (0x13, 0x01);
  REGISTERWRITE (0x13, 0x00);
  REGISTERWRITE (0x0A, 0x18);

  Outb (DATA, 0x0);
  ClearRegister (0);
  Init001 ();

  if (CheckEPAT () != 0)
    return 0;
  DBG (16, "CheckEPAT() passed... (%s:%d)\n", __FILE__, __LINE__);

  tmp = Inb (CONTROL) & 0x1F;
  Outb (CONTROL, tmp);
  Outb (CONTROL, tmp);

  WRITESLOW (0x08, 0x21);
  Init001 ();
  DBG (16, "Init001() passed... (%s:%d)\n", __FILE__, __LINE__);
  WRITESLOW (0x08, 0x21);
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
      REGISTERWRITE (0x0A, i);
      REGISTERREAD (0x0A, i);
      REGISTERWRITE (0x0A, 0xFF - i);
      REGISTERREAD (0x0A, 0xFF - i);
    }
  DBG (16, "EPP write/read buffer loop passed... (%s:%d)\n", __FILE__,
       __LINE__);

  REGISTERWRITE (0x13, 0x01);
  REGISTERWRITE (0x13, 0x00);
  REGISTERWRITE (0x0A, 0x11);

  /* test EPP32 mode */
  /* we set 32 bits I/O mode first, then step back to */
  /* 8bits if tests fail                              */
  SetEPPMode (32);
  for (i = 0; (i < 10) && (GetEPPMode () == 32); i++)
    {
      BufferRead (0x400, dest);
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
	      /* leave out current loop since an error was detected */
	      break;
	    }
	}
      DBG (16, "Loop %d: BufferRead(0x400) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }
  DBG (1, "%d bits EPP data transfer\n", GetEPPMode ());


  for (i = 0; i < 10; i++)
    {
      BufferWrite (0x400, dest);
      DBG (16, "Loop %d: BufferWrite(0x400,dest) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }



  REGISTERREAD (0x0C, 0x04);
  REGISTERWRITE (0x13, 0x01);
  REGISTERWRITE (0x13, 0x00);
  REGISTERWRITE (0x0A, 0x18);

  WRITESLOW (0x08, 0x21);
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
      REGISTERWRITE (0x0A, i);
      REGISTERREAD (0x0A, i);
      REGISTERWRITE (0x0A, 0xFF - i);
      REGISTERREAD (0x0A, 0xFF - i);
    }
  DBG (16, "EPP write/read buffer loop passed... (%s:%d)\n", __FILE__,
       __LINE__);

  REGISTERWRITE (0x13, 0x01);
  REGISTERWRITE (0x13, 0x00);
  REGISTERWRITE (0x0A, 0x11);


  for (i = 0; i < 10; i++)
    {
      BufferRead (0x400, dest);
      DBG (16, "Loop %d: BufferRead(0x400) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }

  for (i = 0; i < 10; i++)
    {
      BufferWrite (0x400, dest);
      DBG (16, "Loop %d: BufferWrite(0x400,dest) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }
  REGISTERREAD (0x0C, 0x04);
  REGISTERWRITE (0x13, 0x01);
  REGISTERWRITE (0x13, 0x00);
  REGISTERWRITE (0x0A, 0x18);
  gMode=UMAX_PP_PARPORT_EPP;
  return 1;
}

  /*
   * try ECP mode
   * returns 1 on success, 0 on failure
   */
int
ProbeECP (unsigned char *dest)
{
  int i, j, tmp;
  unsigned char breg;

  /* if ECP not available, fail */
  if (gECP != 1)
    {
      DBG (1, "Hardware can't do ECP, giving up (%s:%d) ...\n", __FILE__,
	   __LINE__);
      return 0;
    }
  gMode = UMAX_PP_PARPORT_ECP;

/* clean from EPP failure */
  breg = Inb (CONTROL);
  Outb (CONTROL, breg & 0x04);

/* reset sequence */
  Outb (ECR, 0x20);		/* byte mode */
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x0C);
  Outb (CONTROL, 0x0C);
  Outb (CONTROL, 0x0C);
  Outb (CONTROL, 0x0C);
  for (i = 0; i < 256; i++)
    {
      breg = (Inb (STATUS)) & 0xF8;
      if (breg != 0x48)
	{
	  DBG (0,
	       "ProbeECP() failed at sync step %d, status=0x%02X, expected 0x48 (%s:%d)\n",
	       i, breg, __FILE__, __LINE__);
	  return 0;
	}
    }
  Outb (CONTROL, 0x0E);
  Outb (CONTROL, 0x0E);
  Outb (CONTROL, 0x0E);
  breg = (Inb (STATUS)) & 0xF8;
  if (breg != 0x48)
    {
      DBG (0, "ProbeECP() failed, status=0x%02X, expected 0x48 (%s:%d)\n",
	   breg, __FILE__, __LINE__);
      return 0;
    }
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x04);
  breg = Inb (STATUS);
  breg = (Inb (STATUS)) & 0xF8;
  if (breg != 0xC8)
    {
      DBG (0, "ProbeECP() failed, status=0x%02X, expected 0xC8 (%s:%d)\n",
	   breg, __FILE__, __LINE__);
      return 0;
    }
/* end of reset sequence */

  Outb (DATA, 0x00);
  ClearRegister (0);

/* utile ? semble tester le registre de configuration
 * inb ECR,35
 * inb 77B,FF
 * inb ECR,35
 */
/* routine A */
  breg = Inb (CONTROL);		/* 0x04 évidemment! */
  breg = Inb (ECR);
  breg = Inb (ECR);
  breg = Inb (ECR);
  breg = Inb (ECR);
  breg = Inb (CONTROL);		/* 0x04 évidemment! */
  Outb (ECR, 0x20);		/* byte mode */
  Outb (ECR, 0x20);		/* byte mode */
  breg = Inb (CONTROL);		/* 0x04 évidemment! */
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x04);
  breg = Inb (ECR);		/* 35 expected */
  breg = Inb (ECR);		/* 35 expected */
  breg = Inb (ECR);		/* 35 expected */
  breg = Inb (ECR);		/* 35 expected */
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x04);
  Outb (ECR, 0x20);
  Outb (ECR, 0x20);

  ClearRegister (0);

/* routine C */
  PS2RegisterWrite (0x08, 0x01);

  Outb (CONTROL, 0x0C);
  Outb (CONTROL, 0x04);

  ClearRegister (0);

  breg = PS2Something (0x10);
  if (breg != 0x0B)
    {
      DBG (0, "ProbeECP() failed, reg10=0x%02X, expected 0x0B (%s:%d)\n",
	   breg, __FILE__, __LINE__);
      /* return 0; */
    }


  for (i = 0; i < 256; i++)
    {
      ECPRegisterWrite (0x0A, i);
      breg = ECPRegisterRead (0x0A);
      if (breg != i)
	{
	  DBG (0, "ECPProbe(), loop %d failed (%s:%d)\n", i, __FILE__,
	       __LINE__);
	  return 0;
	}
      ECPRegisterWrite (0x0A, 0xFF - i);
      breg = ECPRegisterRead (0x0A);
      if (breg != 0xFF - i)
	{
	  DBG (0, "ECPProbe(), loop %d failed (%s:%d)\n", i, __FILE__,
	       __LINE__);
	  return 0;
	}
    }
  DBG (16, "ECPProbe(), loop passed (%s:%d)\n", __FILE__, __LINE__);

  ECPRegisterWrite (0x13, 0x01);
  ECPRegisterWrite (0x13, 0x00);
  ECPRegisterWrite (0x0A, 0x11);

  /* there is one buffer transfer size set up */
  /* subsequent reads are done in a row       */
  ECPSetBuffer (0x400);
  for (i = 0; i < 10; i++)
    {
      /* if (i > 0) */
      ECPCompatMode ();

      ECPBufferRead (1024, dest);
      /* check content of the returned buffer */
      for (j = 0; j < 256; j++)
	{
	  if (dest[j * 2] != j)
	    {
	      DBG (0,
		   "Altered buffer value at %03X, expected %02X, found %02X\n",
		   j * 2, j, dest[j * 2]);
	      return 0;
	    }
	  if (dest[j * 2 + 1] != 0xFF - j)
	    {
	      DBG
		(0,
		 "Altered buffer value at %03X, expected %02X, found %02X\n",
		 j * 2 + 1, 0xFF - j, dest[j * 2 + 1]);
	      return 0;
	    }
	  if (dest[512 + j * 2] != j)
	    {
	      DBG (0,
		   "Altered buffer value at %03X, expected %02X, found %02X\n",
		   512 + j * 2, j, dest[512 + j * 2]);
	      return 0;
	    }
	  if (dest[512 + j * 2 + 1] != 0xFF - j)
	    {
	      DBG
		(0,
		 "Altered buffer value at %03X, expected 0x%02X, found 0x%02X\n",
		 512 + j * 2 + 1, 0xFF - j, dest[512 + j * 2 + 1]);
	      return 0;
	    }
	}
      ECPByteMode ();
    }

  for (i = 0; i < 10; i++)
    ECPBufferWrite (1024, dest);

  breg = ECPRegisterRead (0x0C);
  if (breg != 0x04)
    {
      DBG (0, "Warning! expected reg0C=0x04, found 0x%02X! (%s:%d) \n", breg,
	   __FILE__, __LINE__);
    }

  ECPRegisterWrite (0x13, 0x01);
  ECPRegisterWrite (0x13, 0x00);
  ECPRegisterWrite (0x0A, 0x18);

  /* reset printer ? */
  Outb (DATA, 0x00);
  Outb (CONTROL, 0x00);
  Outb (CONTROL, 0x04);

  for (i = 0; i < 3; i++)
    {				/* will go in a function */
      ClearRegister (0);
      if (WaitAck () != 1)
	{
	  DBG (0, "ProbeECP failed because of WaitAck() (%s:%d) \n", __FILE__,
	       __LINE__);
	  /* return 0; may fail without harm ... ??? */
	}
      /* are these 2 out really needed ? */
      PS2RegisterWrite (0x08, 0x01);
      Outb (CONTROL, 0x0C);	/* select + reset */
      Outb (CONTROL, 0x04);	/* reset */
    }

  /* prologue of the 'rotate test' */
  ClearRegister (0);
  breg = PS2Something (0x10);
  if (breg != 0x0B)
    {
      DBG (0,
	   "PS2Something returned 0x%02X, 0x0B expected (%s:%d)\n", breg,
	   __FILE__, __LINE__);
    }
  Outb (CONTROL, 0x04);		/* reset */

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

  for (i = 0; i < 256; i++)
    {
      REGISTERWRITE (0x0A, i);
      REGISTERREAD (0x0A, i);
      REGISTERWRITE (0x0A, 0xFF - i);
      REGISTERREAD (0x0A, 0xFF - i);
    }
  DBG (16, "ECPProbe(), write/read buffer loop passed (%s:%d)\n", __FILE__,
       __LINE__);

  REGISTERWRITE (0x13, 0x01);
  REGISTERWRITE (0x13, 0x00);
  REGISTERWRITE (0x0A, 0x11);

  /* should be a function */
  /* in ProbeEPP(), we begin 32 bit mode test here */
  for (i = 0; i < 10; i++)
    {
      ECPCompatMode ();

      ECPBufferRead (0x400, dest);
      /* check content of the returned buffer */
      for (j = 0; j < 256; j++)
	{
	  if (dest[j * 2] != j)
	    {
	      DBG (0,
		   "Altered buffer value at %03X, expected %02X, found %02X\n",
		   j * 2, j, dest[j * 2]);
	      return 0;
	    }
	  if (dest[j * 2 + 1] != 0xFF - j)
	    {
	      DBG
		(0,
		 "Altered buffer value at %03X, expected %02X, found %02X\n",
		 j * 2 + 1, 0xFF - j, dest[j * 2 + 1]);
	      return 0;
	    }
	  if (dest[512 + j * 2] != j)
	    {
	      DBG (0,
		   "Altered buffer value at %03X, expected %02X, found %02X\n",
		   512 + j * 2, j, dest[512 + j * 2]);
	      return 0;
	    }
	  if (dest[512 + j * 2 + 1] != 0xFF - j)
	    {
	      DBG
		(0,
		 "Altered buffer value at %03X, expected 0x%02X, found 0x%02X\n",
		 512 + j * 2 + 1, 0xFF - j, dest[512 + j * 2 + 1]);
	      return 0;
	    }
	}
      ECPByteMode ();
    }

  for (i = 0; i < 10; i++)
    ECPBufferWrite (1024, dest);

  REGISTERREAD (0x0C, 0x04);
  REGISTERWRITE (0x13, 0x01);
  REGISTERWRITE (0x13, 0x00);
  REGISTERWRITE (0x0A, 0x18);
  WaitAck ();

  return 1;
}



/* 1: OK
   0: probe failed */

int
sanei_umax_pp_ProbeScanner (int recover)
{
  int tmp, i, j;
  int reg, nb;
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
  /* fast detect */
  tmp = RingScanner (2, 0);
  if (!tmp)
    {
      DBG (1, "No scanner detected by 'RingScanner(2,0)'...\n");
      tmp = RingScanner (5, 0);
      if (!tmp)
	{
	  DBG (1, "No scanner detected by 'RingScanner(5,0)'...\n");
	  tmp = RingScanner (5, 10000);
	  if (!tmp)
	    {
	      DBG (1, "No scanner detected by 'RingScanner(5,10000)'...\n");
	      tmp = RingScanner (5, 10000);
	      if (!tmp)
		{
		  DBG (1,
		       "No scanner detected by 'RingScanner(5,10000)'...\n");
		  tmp = Test610P (0x87);
		}
	    }
	}
    }
  if (!tmp)
    {
      DBG (1, "No 1220P/2000P scanner detected by 'RingScanner()'...\n");
      DBG (1, "Trying 610P...\n");
      return Probe610P (recover);
    }
  DBG (16, "RingScanner passed...\n");

  gControl = Inb (CONTROL) & 0x3F;
  g67D = 1;
  if (SendCommand (0x30) == 0)
    {
      DBG (0, "SendCommand(0x30) (%s:%d) failed ...\n", __FILE__, __LINE__);
      return 0;
    }
  DBG (16, "SendCommand(0x30) passed ... (%s:%d)\n", __FILE__, __LINE__);
  g67E = 4;			/* bytes to read */
  if (SendCommand (0x00) == 0)
    {
      DBG (0, "SendCommand(0x00) (%s:%d) failed ...\n", __FILE__, __LINE__);
      return 0;
    }
  DBG (16, "SendCommand(0x00) passed... (%s:%d)\n", __FILE__, __LINE__);
  g67E = 0;			/* bytes to read */
  if (TestVersion (0) == 0)
    {
      DBG (0, "TestVersion(0) (%s:%d) failed ...\n", __FILE__, __LINE__);
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
      return 0;
    }
  DBG (16, "SendCommand(0x40) passed...\n");
  if (SendCommand (0xE0) == 0)
    {
      DBG (0, "SendCommand(0xE0) (%s:%d) failed ...\n", __FILE__, __LINE__);
      return 0;
    }
  DBG (16, "SendCommand(0xE0) passed...\n");

  ClearRegister (0);
  DBG (16, "ClearRegister(0) passed...\n");

  SPPResetLPT ();
  DBG (16, "SPPResetLPT() passed...\n");

  Outb (CONTROL, 4);
  Outb (CONTROL, 4);


  /* test PS2 mode */

  tmp = PS2RegisterRead (0x0B);
  if (tmp == 0xC7)
    {
      /* epat C7 detected */
      DBG (16, "PS2RegisterRead(0x0B)=0x%X passed...\n", tmp);

      PS2RegisterWrite (8, 0);
      DBG (16, "PS2RegisterWrite(8,0) passed...   (%s:%d)\n", __FILE__,
	   __LINE__);

      tmp = PS2RegisterRead (0x0A);
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
      DBG (16, "PS2RegisterRead(0x0A)=0x%X passed ...(%s:%d)\n", tmp,
	   __FILE__, __LINE__);

    }
  else
    {
      DBG (0, "Found 0x%X expected 0xC7 or 0x88  (%s:%d)\n", tmp, __FILE__,
	   __LINE__);
      if (tmp == 0xFF)
	{
	  DBG (0,
	       "It is likely that the hardware adress (0x%X) you specified is wrong\n",
	       gPort);
	}
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
  model = PS2RegisterRead (0x0F);
  DBG (1, "UMAX Astra 1220/1600/2000 P ASIC detected (mode=%d)\n", model);
  SetModel (model);
  DBG (16, "PS2RegisterRead(0x0F) passed... (%s:%d)\n", __FILE__, __LINE__);

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
  tmp = PS2RegisterRead (0x0D);
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
      SLOWNIBBLEREGISTERREAD (0x12, 0x10);
      break;
    case 0x07:
      WRITESLOW (0x12, 0x00);
      SLOWNIBBLEREGISTERREAD (0x12, 0x00);
      /* we may get 0x20, in this case some color aberration may occur */
      /* must depend on the parport */
      /* model 0x07 + 0x00=>0x20=2000P */
      break;
    default:
      WRITESLOW (0x12, 0x00);
      SLOWNIBBLEREGISTERREAD (0x12, 0x20);
      break;
    }
  SLOWNIBBLEREGISTERREAD (0x0D, 0x18);
  SLOWNIBBLEREGISTERREAD (0x0C, 0x04);
  SLOWNIBBLEREGISTERREAD (0x0A, 0x00);
  WRITESLOW (0x0E, 0x0A);
  WRITESLOW (0x0F, 0x00);
  WRITESLOW (0x0E, 0x0D);
  WRITESLOW (0x0F, 0x00);
  dest = (unsigned char *) malloc (65536);
  if (dest == NULL)
    {
      DBG (0, "Failed to allocate 64K (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }

  gMode = UMAX_PP_PARPORT_PS2;
  if (ProbePS2 (dest))
    {				/* PS2 mode works */
      DBG (16, "ProbePS2 passed ... (%s:%d)\n", __FILE__, __LINE__);
      gProbed = UMAX_PP_PARPORT_PS2;
    }

  Outb (CONTROL, 4);
  SLOWNIBBLEREGISTERREAD (0x0A, 0x18);
  WRITESLOW (0x08, 0x40);
  WRITESLOW (0x08, 0x60);
  WRITESLOW (0x08, 0x22);

  gMode = UMAX_PP_PARPORT_EPP;
  if (ProbeEPP (dest))
    {				/* EPP mode works */
      gProbed = UMAX_PP_PARPORT_EPP;
      gMode = UMAX_PP_PARPORT_EPP;
      DBG (16, "ProbeEPP passed ... (%s:%d)\n", __FILE__, __LINE__);
    }
  else
    {				/* EPP fails, try ECP */
      DBG (16, "ProbeEPP failed ... (%s:%d)\n", __FILE__, __LINE__);
      gMode = UMAX_PP_PARPORT_ECP;
      if (ProbeECP (dest))
	{			/* ECP mode works */
	  DBG (16, "ProbeECP passed ... (%s:%d)\n", __FILE__, __LINE__);
	  gProbed = UMAX_PP_PARPORT_ECP;
	}
      else
	{			/* ECP and EPP fail, give up */
	  /* PS2 could be used */
	  DBG (16, "ProbeECP failed ... (%s:%d)\n", __FILE__, __LINE__);
	  DBG (1, "No EPP or ECP mode working, giving up ... (%s:%d)\n",
	       __FILE__, __LINE__);
	  free (dest);
	  return 0;
	}
    }

  /* some operations here may have to go into ProbeEPP/ProbeECP */
  g6FE = 1;
  if (gMode == UMAX_PP_PARPORT_ECP)
    {
      WRITESLOW (0x08, 0x01);
      Outb (CONTROL, 0x0C);
      Outb (CONTROL, 0x04);
      ClearRegister (0);
      tmp = PS2Something (0x10);
      if (tmp != 0x0B)
	{
	  DBG (0,
	       "PS2Something returned 0x%02X, 0x0B expected (%s:%d)\n", tmp,
	       __FILE__, __LINE__);
	}
    }
  else
    {
      WRITESLOW (0x08, 0x21);
      Init001 ();
      DBG (16, "Init001() passed... (%s:%d)\n", __FILE__, __LINE__);
    }


  reg = RegisterRead (0x0D);
  reg = (reg & 0xE8) | 0x43;
  RegisterWrite (0x0D, reg);

  REGISTERWRITE (0x0A, 0x18);
  REGISTERWRITE (0x0E, 0x0F);
  REGISTERWRITE (0x0F, 0x0C);

  REGISTERWRITE (0x0A, 0x1C);
  REGISTERWRITE (0x0E, 0x10);
  REGISTERWRITE (0x0F, 0x1C);


  reg = RegisterRead (0x0D);	/* 0x48 expected */
  reg = RegisterRead (0x0D);
  reg = RegisterRead (0x0D);
  reg = (reg & 0xB7) | 0x03;
  RegisterWrite (0x0D, reg);
  DBG (16, "(%s:%d) passed \n", __FILE__, __LINE__);

  reg = RegisterRead (0x12);	/* 0x10 for model 0x0F, 0x20 for model 0x07 */
  /* 0x00 when in ECP mode ... */
  reg = reg & 0xEF;
  RegisterWrite (0x12, reg);
  DBG (16, "(%s:%d) passed \n", __FILE__, __LINE__);

  reg = RegisterRead (0x0A);
  if (reg != 0x1C)
    {
      DBG (0, "Warning! expected reg0A=0x1C, found 0x%02X! (%s:%d) \n", reg,
	   __FILE__, __LINE__);
    }
  DBG (16, "(%s:%d) passed \n", __FILE__, __LINE__);

  /*Inb(CONTROL);       ECP 0x04 expected */
  Disconnect ();
  DBG (16, "Disconnect() passed... (%s:%d)\n", __FILE__, __LINE__);
  Connect ();
  DBG (16, "Connect() passed... (%s:%d)\n", __FILE__, __LINE__);


  /* some sort of countdown, some warming-up ? */
  /* maybe some pauses are needed              */
  /* if (model == 0x07) */
  {
    /* REGISTERWRITE (0x0A, 0x00);
       reg = RegisterRead (0x0D);
       reg = (reg & 0xE8);
       RegisterWrite (0x0D, reg);
       DBG (16, "(%s:%d) passed \n", __FILE__, __LINE__); */
    Epilogue ();
    Prologue (0x10);

    reg = RegisterRead (0x13);
    if (reg != 0x00)
      {
	DBG (0, "Warning! expected reg13=0x00, found 0x%02X! (%s:%d) \n",
	     reg, __FILE__, __LINE__);
      }
    REGISTERWRITE (0x13, 0x81);
    usleep (10000);
    REGISTERWRITE (0x13, 0x80);
    /* could it be step-motor values ? */
    REGISTERWRITE (0x0E, 0x04);	/* FF->R04 */
    REGISTERWRITE (0x0F, 0xFF);
    REGISTERWRITE (0x0E, 0x05);	/* 03->R05 */
    REGISTERWRITE (0x0F, 0x03);
    REGISTERWRITE (0x10, 0x66);
    usleep (10000);

    REGISTERWRITE (0x0E, 0x04);	/* FF->R04 */
    REGISTERWRITE (0x0F, 0xFF);
    REGISTERWRITE (0x0E, 0x05);	/* 01 ->R05 */
    REGISTERWRITE (0x0F, 0x01);
    REGISTERWRITE (0x10, 0x55);
    usleep (10000);

    REGISTERWRITE (0x0E, 0x04);	/* FF -> R04 */
    REGISTERWRITE (0x0F, 0xFF);
    REGISTERWRITE (0x0E, 0x05);	/* 00 -> R05 */
    REGISTERWRITE (0x0F, 0x00);
    REGISTERWRITE (0x10, 0x44);
    usleep (10000);

    REGISTERWRITE (0x0E, 0x04);	/* 7F -> R04 */
    REGISTERWRITE (0x0F, 0x7F);
    REGISTERWRITE (0x0E, 0x05);	/* 00 -> R05 */
    REGISTERWRITE (0x0F, 0x00);
    REGISTERWRITE (0x10, 0x33);
    usleep (10000);

    REGISTERWRITE (0x0E, 0x04);	/* 3F -> R04 */
    REGISTERWRITE (0x0F, 0x3F);
    REGISTERWRITE (0x0E, 0x05);
    REGISTERWRITE (0x0F, 0x00);	/* 00 -> R05 */
    REGISTERWRITE (0x10, 0x22);
    usleep (10000);

    REGISTERWRITE (0x0E, 0x04);
    REGISTERWRITE (0x0F, 0x00);
    REGISTERWRITE (0x0E, 0x05);
    REGISTERWRITE (0x0F, 0x00);
    REGISTERWRITE (0x10, 0x11);
    usleep (10000);

    REGISTERWRITE (0x13, 0x81);
    usleep (10000);
    REGISTERWRITE (0x13, 0x80);

    REGISTERWRITE (0x0E, 0x04);
    REGISTERWRITE (0x0F, 0x00);
    REGISTERWRITE (0x0E, 0x05);
    REGISTERWRITE (0x0F, 0x00);
    usleep (10000);

    reg = RegisterRead (0x10);
    DBG (1, "Count-down value is 0x%02X  (%s:%d)\n", reg, __FILE__, __LINE__);
    /* 2 reports of CF, was FF first (typo ?) */
    /* CF seems a valid value                 */
    /* in case of CF, we may have timeout ... */
    /*if (reg != 0x00)
       {
       DBG (0, "Warning! expected reg10=0x00, found 0x%02X! (%s:%d) \n",
       reg, __FILE__, __LINE__);
       } */
    REGISTERWRITE (0x13, 0x00);
  }
/* end of countdown */
  DBG (16, "(%s:%d) passed \n", __FILE__, __LINE__);
  /* *NOT* Epilogue(); (when EPP) */
  /*REGISTERWRITE (0x0A, 0x00);
     REGISTERREAD (0x0D, 0x40);
     REGISTERWRITE (0x0D, 0x00); */
  Epilogue ();
  Prologue (0x10);
  REGISTERWRITE (0x0E, 0x0D);
  REGISTERWRITE (0x0F, 0x00);
  REGISTERWRITE (0x0A, 0x1C);

  /* real start of high level protocol ? */
  if (Fonc001 () != 1)
    {
      DBG (0, "Fonc001() failed ! (%s:%d) \n", __FILE__, __LINE__);
      return 0;
    }
  DBG (16, "Fonc001() passed (%s:%d) \n", __FILE__, __LINE__);
  reg = RegisterRead (0x19) & 0xC8;
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
      return 0;
    }
  DBG (16, "CmdSetDataBuffer(initbuf) passed... (%s:%d)\n", __FILE__,
       __LINE__);
  if (CmdSetDataBuffer (voidbuf) != 1)
    {
      DBG (0, "CmdSetDataBuffer(voidbuf) failed ! (%s:%d) \n", __FILE__,
	   __LINE__);
      return 0;
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
	  return 0;
	}
      DBG (16, "Loop %d: CmdSetDataBuffer(initbuf) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
      if (CmdSetDataBuffer (voidbuf) != 1)
	{
	  DBG (0, "Loop %d: CmdSetDataBuffer(voidbuf) failed ! (%s:%d) \n", i,
	       __FILE__, __LINE__);
	  return 0;
	}
    }


  /* memory size testing ? */
  /* load 150 Ko in scanner */
  REGISTERWRITE (0x1A, 0x00);
  REGISTERWRITE (0x1A, 0x0C);
  REGISTERWRITE (0x1A, 0x00);
  REGISTERWRITE (0x1A, 0x0C);


  REGISTERWRITE (0x0A, 0x11);	/* start */
  if (gMode == UMAX_PP_PARPORT_ECP)
    {
      nb = 300;
      nb = 150;
    }
  else
    {
      nb = 150;
    }
  for (i = 0; i < nb; i++)	/* 300 for ECP ??? */
    {
      BufferWrite (0x400, dest);
      DBG (16, "Loop %d: BufferWrite(0x400,dest) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }
  REGISTERWRITE (0x0A, 0x18);	/* end */

  /* read them back */
  REGISTERWRITE (0x0A, 0x11);	/*start transfert */
  if (gMode == UMAX_PP_PARPORT_ECP)
    {
      ECPSetBuffer (0x400); /* XXX ??? XXX */
    }

  for (i = 0; i < nb; i++)	/* 300 for ECP ??? */
    {
      BufferRead (0x400, dest);
      DBG (16, "Loop %d: BufferRead(0x400,dest) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }
  REGISTERWRITE (0x0A, 0x18);	/*end transfer */

  /* fully disconnect, then reconnect */
  if (gMode == UMAX_PP_PARPORT_ECP)
    {
      Epilogue ();
      SendCommand (0xE0);
      Outb (DATA, 0xFF);
      Outb (DATA, 0xFF);
      ClearRegister (0);
      WRITESLOW (0x0E, 0x0A);
      SLOWNIBBLEREGISTERREAD (0x0F, 0x00);
      WRITESLOW (0x0F, 0x08);
      WRITESLOW (0x08, 0x10);	/* 0x10 ?? */
      Prologue (0x10);
    }


  /* almost CmdSync(0x00) which halts any pending operation */
  if (Fonc001 () != 1)
    {
      DBG (0, "Fonc001() failed ! (%s:%d) \n", __FILE__, __LINE__);
      return 0;
    }
  DBG (16, "Fct001() passed (%s:%d) \n", __FILE__, __LINE__);
  if (SendWord (zero) == 0)
    {
      DBG (0, "SendWord(zero) failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  Epilogue ();
  DBG (16, "SendWord(zero) passed (%s:%d)\n", __FILE__, __LINE__);


  /* end transport init */
  /* now high level (connected) protocol begins */
  val = sanei_umax_pp_InitScanner (recover);
  if (val == 0)
    {
      DBG (0, "InitScanner() failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }


  /* if no homing .... */
  if (val == 1)
    {
      CMDSYNC (0);
      CMDSYNC (0xC2);
      CMDSYNC (0);
    }

  /* set port to its initial state */
  Outb (DATA, gData);
  Outb (CONTROL, gControl);

  free (dest);
  DBG (1, "Probe done ...\n");
  return 1;
}


static int
deconnect_epat (void)
{
  int tmp;

  REGISTERWRITE (0x0A, 0x00);
  REGISTERREAD (0x0D, 0x40);
  REGISTERWRITE (0x0D, 0x00);
  Disconnect ();
  return 1;
}


static int
connect_epat (int r08)
{
  int reg;

  if (Connect () != 1)
    {
      DBG (0, "connect_epat: Connect() failed! (%s:%d)\n", __FILE__,
	   __LINE__);
      return 0;
    }

  reg = RegisterRead (0x0B);
  if (reg != gEPAT)
    {
      /* ASIC version            is not */
      /* the one expected (epat c7)     */
      DBG (0, "Error! expected reg0B=0x%02X, found 0x%02X! (%s:%d) \n", gEPAT,
	   reg, __FILE__, __LINE__);
      /* we try to clean all */
      Disconnect ();
      return 0;
    }
  reg = RegisterRead (0x0D);
  reg = (reg | 0x43) & 0xEB;
  REGISTERWRITE (0x0D, reg);
  REGISTERWRITE (0x0C, 0x04);
  reg = RegisterRead (0x0A);
  if (reg != 0x00)
    {
      /* a previous unfinished command */
      /* has left an uncleared value   */
      DBG (0, "Warning! expected reg0A=0x00, found 0x%02X! (%s:%d) \n", reg,
	   __FILE__, __LINE__);
    }
  REGISTERWRITE (0x0A, 0x1C);
  if (r08 != 0)
    {
      if (gMode == UMAX_PP_PARPORT_ECP)
	{
	  REGISTERWRITE (0x08, r08);	/* 0x01 or 0x10 ??? */
	}
      else
	{
	  REGISTERWRITE (0x08, 0x21);
	}
    }
  REGISTERWRITE (0x0E, 0x0F);
  REGISTERWRITE (0x0F, 0x0C);
  REGISTERWRITE (0x0A, 0x1C);
  REGISTERWRITE (0x0E, 0x10);
  REGISTERWRITE (0x0F, 0x1C);
  if (gMode == UMAX_PP_PARPORT_ECP)
    {
      REGISTERWRITE (0x0F, 0x00);
    }
  return 1;
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
      return 0;
    }
  Outb (CONTROL, 0x07);
  tmp = Inb (STATUS);
  if (tmp != 0x38)
    {
      DBG (0, "Error! expected STATUS=0x38, found 0x%02X! (%s:%d) \n", tmp,
	   __FILE__, __LINE__);
      return 0;
    }
  Outb (CONTROL, 0x04);
  tmp = Inb (STATUS);
  if (tmp != 0xF8)
    {
      DBG (0, "Error! expected STATUS=0xF8, found 0x%02X! (%s:%d) \n", tmp,
	   __FILE__, __LINE__);
      return 0;
    }
  return 1;
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
	  return 0;
	}
    }
  while ((i < 41) && (tmp == 0x04));

  Outb (DATA, gData);
  return 1;
}

static int
Prologue (int r08)
{
  switch (sanei_umax_pp_getastra ())
    {
    case 610:
      return connect_610P ();
    case 1220:
    case 1600:
    case 2000:
    default:
      return connect_epat (r08);
    }
}



static int
Epilogue (void)
{
  switch (sanei_umax_pp_getastra ())
    {
    case 610:
      return deconnect_610P ();
    case 1220:
    case 1600:
    case 2000:
    default:
      return deconnect_epat ();
    }
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

  if (!Prologue (0x10))
    {
      DBG (0, "CmdSet: Prologue failed !   (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }

  /* send data */
  if (SendLength (word, 4) == 0)
    {
      DBG (0, "SendLength(word,4) failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  TRACE (16, "SendLength(word,4) passed ...");

  /* head end */
  Epilogue ();

  if (len > 0)
    {
      /* send body */
      if (!Prologue (0x10))
	{
	  DBG (0, "CmdSet: Prologue failed !   (%s:%d)\n", __FILE__,
	       __LINE__);
	}
      if (DBG_LEVEL >= 8)
	{
	  char *str = NULL;

	  str = malloc (3 * len + 1);
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
	  return 0;
	}
      TRACE (16, "SendData(val,len) passed ...");
      /* body end */
      Epilogue ();
    }
  return 1;
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
  if (!Prologue (0x10))
    {
      DBG (0, "CmdGet: Prologue failed !   (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }

  /* send data */
  if (SendLength (word, 4) == 0)
    {
      DBG (0, "SendLength(word,4) failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  TRACE (16, "SendLength(word,4) passed ...");

  /* head end */
  Epilogue ();


  /* send header */
  if (!Prologue (0x10))
    {
      DBG (0, "CmdGet: Prologue failed !   (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }

  /* get actual data */
  if (ReceiveData (val, len) == 0)
    {
      DBG (0, "ReceiveData(val,len) failed (%s:%d)\n", __FILE__, __LINE__);
      Epilogue ();
      return 0;
    }
  if (DBG_LEVEL >= 8)
    {
      char *str = NULL;

      str = malloc (3 * len + 1);
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
  return 1;
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
      return 0;
    }

  tampon = (int *) malloc (len * sizeof (int));
  memset (tampon, 0x00, len * sizeof (int));
  if (tampon == NULL)
    {
      DBG (0, "Failed to allocate room for %d int ! (%s:%d)\n", len, __FILE__,
	   __LINE__);
      Epilogue ();
      return 0;
    }

  /* then we receive */
  if (CmdGet (cmd, len, tampon) == 0)
    {
      DBG (0, "CmdSetGet failed !  (%s:%d)\n", __FILE__, __LINE__);
      free (tampon);
      Epilogue ();
      return 0;
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
  return 1;
}


/* 1 OK, 0 failed */
static int
CmdGetBuffer (int cmd, int len, unsigned char *buffer)	/* ECP OK */
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
      return 0;
    }
  DBG (16, "(%s:%d) passed \n", __FILE__, __LINE__);

  Prologue (0x10);

  REGISTERWRITE (0x0E, 0x0D);
  REGISTERWRITE (0x0F, 0x00);

  i = 0;
  reg = RegisterRead (0x19) & 0xF8;

  /* wait if busy */
  while ((reg & 0x08) == 0x08)
    reg = RegisterRead (0x19) & 0xF8;
  if ((reg != 0xC0) && (reg != 0xD0))
    {
      DBG (0, "CmdGetBuffer failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }

  if (gMode == UMAX_PP_PARPORT_ECP)
    {
      REGISTERWRITE (0x1A, 0x44);
    }

  read = 0;
  reg = RegisterRead (0x0C);
  if (reg != 0x04)
    {
      DBG (0, "CmdGetBuffer failed: unexpected status 0x%02X  ...(%s:%d)\n",
	   reg, __FILE__, __LINE__);
      return 0;
    }
  REGISTERWRITE (0x0C, reg | 0x40);

  /* actual data */
  read = 0;
  while (read < len)
    {
      needed = len - read;
      if (needed > 32768)
	needed = 32768;
      if (gMode == UMAX_PP_PARPORT_ECP)
	{
	  ECPCompatMode ();
	  ECPSetBuffer (needed);
	  tmp = ECPBufferRead (needed, buffer + read);
	  DBG (16, "ECPBufferRead(%d,buffer+read) passed (%s:%d)\n", needed,
	       __FILE__, __LINE__);
	  REGISTERWRITE (0x1A, 0x84);
	}
      else
	{
	  tmp = PausedBufferRead (needed, buffer + read);
	}
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
	  reg = RegisterRead (0x19) & 0xF8;
	  DBG (64, "Status after block read is 0x%02X (%s:%d)\n", reg,
	       __FILE__, __LINE__);
	  if ((reg & 0x08) == 0x08)
	    {
	      int pass = 0;

	      do
		{
		  reg = RegisterRead (0x19) & 0xF8;
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
	  if (gMode == UMAX_PP_PARPORT_ECP)
	    {
	      REGISTERWRITE (0x1A, 0x44);
	    }
	  reg = RegisterRead (0x0C);
	  RegisterWrite (0x0C, reg | 0x40);
	}
    }

  /* OK ! */
  REGISTERWRITE (0x0E, 0x0D);
  REGISTERWRITE (0x0F, 0x00);

  /* epilogue */
  Epilogue ();
  return 1;
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

  if (!Prologue (0x10))
    {
      DBG (0, "CmdSet: Prologue failed !   (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }

  /* send data */
  if (SendLength (word, 4) == 0)
    {
      DBG (0, "SendLength(word,4) failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  TRACE (16, "SendLength(word,4) passed ...");

  /* head end */
  Epilogue ();

  Prologue (0x10);
  REGISTERWRITE (0x0E, 0x0D);
  REGISTERWRITE (0x0F, 0x00);

  i = 0;
  reg = RegisterRead (0x19) & 0xF8;

  /* wait if busy */
  while ((reg & 0x08) == 0x08)
    reg = RegisterRead (0x19) & 0xF8;
  if ((reg != 0xC0) && (reg != 0xD0))
    {
      DBG (0, "CmdGetBuffer32 failed: unexpected status 0x%02X  ...(%s:%d)\n",
	   reg, __FILE__, __LINE__);
      return 0;
    }
  reg = RegisterRead (0x0C);
  if (reg != 0x04)
    {
      DBG (0, "CmdGetBuffer32 failed: unexpected status 0x%02X  ...(%s:%d)\n",
	   reg, __FILE__, __LINE__);
      return 0;
    }
  REGISTERWRITE (0x0C, reg | 0x40);

  read = 0;
  while (read < len)
    {
      if (read + 1700 < len)
	{
	  tmp = 1700;
	  BufferRead (tmp, buffer + read);
	  reg = RegisterRead (0x19) & 0xF8;
	  if ((read + tmp < len) && (reg & 0x08) == 0x08)
	    {
	      do
		{
		  reg = RegisterRead (0x19) & 0xF8;
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
	  reg = RegisterRead (0x0C);
	  RegisterWrite (0x0C, reg | 0x40);
	  read += tmp;
	}
      else
	{
	  tmp = len - read;
	  BufferRead (tmp, buffer + read);
	  read += tmp;
	  if ((read < len))
	    {
	      reg = RegisterRead (0x19) & 0xF8;
	      while ((reg & 0x08) == 0x08)
		{
		  reg = RegisterRead (0x19) & 0xF8;
		}
	    }
	}
    }

  /* OK ! */
  Epilogue ();
  return 1;
}

int
sanei_umax_pp_CmdSync (int cmd)
{
  int word[5];

  /* compute word */
  word[0] = 0x00;
  word[1] = 0x00;
  word[2] = 0x00;
  word[3] = cmd;

  if (!Prologue (0x10))
    {
      DBG (0, "CmdSync: Prologue failed !   (%s:%d)\n", __FILE__, __LINE__);
    }

  /* send data */
  if (SendLength (word, 4) == 0)
    {
      DBG (0, "SendLength(word,4) failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  TRACE (16, "SendLength(word,4) passed ...");

  /* end OK */
  Epilogue ();

  return 1;
}


/* numbers of bytes read, else 0 (failed)                             */
/* read data by chunk EXACTLY the width of the scan area in the given */
/* resolution . If a valid file descriptor is given, we write data    */
/* in it according to the color mode, before polling the scanner      */
/* len should not be bigger than 2 Megs                                      */

int
CmdGetBlockBuffer (int cmd, int len, int window, unsigned char *buffer)
{
#ifdef HAVE_SYS_TIME_H
  struct timeval td, tf;
  float elapsed;
#endif
  int reg, i;
  int word[5], read;

  /* compute word */
  word[0] = len / 65536;
  word[1] = len / 256 % 256;
  word[2] = len % 256;
  word[3] = (cmd & 0x3F) | 0x80 | 0x40;

  if (!Prologue (0x10))
    {
      DBG (0, "CmdGetBlockBuffer: Prologue failed !   (%s:%d)\n", __FILE__,
	   __LINE__);
    }

  /* send data */
  if (SendLength (word, 4) == 0)
    {
      DBG (0, "SendLength(word,4) failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  TRACE (16, "SendLength(word,4) passed ...");
  /* head end */
  Epilogue ();



  if (!Prologue (0x10))
    {
      DBG (0, "CmdGetBlockBuffer: Prologue failed !   (%s:%d)\n", __FILE__,
	   __LINE__);
    }


  REGISTERWRITE (0x0E, 0x0D);
  REGISTERWRITE (0x0F, 0x00);

  i = 0;

  /* init counter */
  read = 0;

  /* read scanner state */
  reg = RegisterRead (0x19) & 0xF8;


  /* read loop */
  while (read < len)
    {
      /* wait for the data to be ready */
#ifdef HAVE_SYS_TIME_H
      gettimeofday (&td, NULL);
#endif
      while ((reg & 0x08) == 0x08)
	{
	  reg = RegisterRead (0x19) & 0xF8;
#ifdef HAVE_SYS_TIME_H
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
	      return read;
	    }
#endif
	}
      if ((reg != 0xC0) && (reg != 0xD0) && (reg != 0x00))
	{
	  DBG (0,
	       "Unexpected status 0x%02X, expected 0xC0 or 0xD0 ! (%s:%d)\n",
	       reg, __FILE__, __LINE__);
	  DBG (0, "Going on...\n");
	}

      /* signals next chunk */
      reg = RegisterRead (0x0C);
      if (reg != 0x04)
	{
	  DBG (0,
	       "CmdGetBlockBuffer failed: unexpected value reg0C=0x%02X  ...(%s:%d)\n",
	       reg, __FILE__, __LINE__);
	  return 0;
	}
      REGISTERWRITE (0x0C, reg | 0x40);


      /* there is always a full block ready when scanner is ready */
      /* 32 bits I/O read , window must match the width of scan   */
      BufferRead (window, buffer + read);

      /* add bytes read */
      read += window;


      DBG (16, "Read %d bytes out of %d (last block is %d bytes) (%s:%d)\n",
	   read, len, window, __FILE__, __LINE__);

      /* test status after read */
      reg = RegisterRead (0x19) & 0xF8;
    }


  /* wait for the data to be ready */
#ifdef HAVE_SYS_TIME_H
  gettimeofday (&td, NULL);
#endif
  while ((reg & 0x08) == 0x08)
    {
      reg = RegisterRead (0x19) & 0xF8;
#ifdef HAVE_SYS_TIME_H
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
	  return read;
	}
#endif
    }
  if ((reg != 0xC0) && (reg != 0xD0) && (reg != 0x00))
    {
      DBG (0, "Unexpected status 0x%02X, expected 0xC0 or 0xD0 ! (%s:%d)\n",
	   reg, __FILE__, __LINE__);
      DBG (0, "Going on...\n");
    }

  REGISTERWRITE (0x0E, 0x0D);
  REGISTERWRITE (0x0F, 0x00);


  /* OK ! */
  Epilogue ();
  return read;
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
  DBG (0, "\t->channel 1 brightness=0x%02X (%d)\n", op[10] & 0x0F,
       op[10] & 0x0F);
  DBG (0, "\t->channel 2 brightness=0x%02X (%d)\n", (op[10] & 0xF0) / 16,
       (op[10] & 0xF0) / 16);
  DBG (0, "\t->channel 3 brightness=0x%02X (%d)\n", op[11] & 0x0F,
       op[11] & 0x0F);
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

  /* byte 14 */
  if (op[14] & 0x20)
    {
      DBG (0, "\t->lamp on    \n");
    }
  else
    {
      DBG (0, "\t->lamp off    \n");
    }
  if (op[14] & 0x04)
    {
      DBG (0, "\t->normal scan (head stops at each row)    \n");
    }
  else
    {
      DBG (0, "\t->move and scan (head doesn't stop at each row)    \n");
    }
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
CompletionWait (void)		/* ECP OK */
{
  CMDSYNC (0x40);
  do
    {
      usleep (100000);
      CMDSYNC (0xC2);
    }
  while ((sanei_umax_pp_ScannerStatus () & 0x90) != 0x90);
  CMDSYNC (0xC2);
  return 1;
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
      return 1;
    }
  if ((state) && (on))
    {
      DBG (2, "Lamp already on ... (%s:%d)\n", __FILE__, __LINE__);
      return 1;
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
      return 0;
    }
  DBG (16, "CmdSetGet(0x02,16,buffer) passed ... (%s:%d)\n", __FILE__,
       __LINE__);
  return 1;
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


/* dump data has received from scanner (red line/green line/blue line)
   to a color pnm file */
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
  if (fic == NULL)
    {
      DBG (0, "could not open %s for writing\n", titre);
      return;
    }
  fprintf (fic, "P6\n%d %d\n255\n", width, height);
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

/* dump a color buffer in a color PNM */
static void
DumpRGB (int width, int height, unsigned char *data, char *name)
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
	  fputc (data[3 * y * width + x * 3], fic);
	  fputc (data[3 * y * width + x * 3 + 1], fic);
	  fputc (data[3 * y * width + x * 3 + 2], fic);
	}
    }
  fclose (fic);
}

static int
EvalGain (int sum, int count)
{
  int gn;
  float pct;
  float avg;


  /*if(getenv("CORRECTION"))                    
     return(atoi(getenv("CORRECTION"))); */

  /* 19000 is a little too bright */
  /* gn = (int) ((double) (18500 * count) / sum - 100 + 0.5); */

  /* after ~ 60 * 10 scans , it looks like 1 step is a 0.57% increase   */
  /* so we take the value and compute the percent increase to reach 250 */
  /* not 255, because we want some room for inaccuracy                  */
  /* pct=100-(value*100)/250                                            */
  /* then correction is pct/0.57                                        */
  avg = (float) (sum) / (float) (count);
  pct = 100.0 - (avg * 100.0) / 250.0;
  gn = (int) (pct / 0.57);

  /* bound checking : there are sightings of >127 values being negative */
  if (gn < 0)
    gn = 0;
  else if (gn > 127)
    gn = 127;
  return gn;
}

static void
ComputeCalibrationData (int color, int dpi, int width, unsigned char *source,
			int *data)
{
  int p, i, l;
  int sum;


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
	  data[i] = EvalGain (sum, l);
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
      data[p + i] = EvalGain (sum, l);
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
	  data[p + i] = EvalGain (sum, l);
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
   1: success                                          */
static int
Move (int distance, int precision, unsigned char *buffer)	/* ECP OK */
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
    return 0;

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
  return 1;
}



/* for each column, finds the row where white/black transition occurs
        then returns the average */
static float
EdgePosition (int width, int height, unsigned char *data)
{
  int ecnt, x, y;
  float epos;
  int d, dmax, dpos, i;
  unsigned char *dbuffer = NULL;

  if (DBG_LEVEL > 128)
    {
      dbuffer = (unsigned char *) malloc (3 * width * height);
      memset (dbuffer, 0x00, 3 * width * height);
    }
  epos = 0;
  ecnt = 0;
  for (x = 0; x < width; x++)
    {
      /* edge: white->black drop  */
      /* loop stops on black area */
      dmax = 0;
      dpos = 0;
      d = 0;
      i = 0;
      for (y = 10; (y < height) && (data[i] > 10); y++)
	{
	  i = x + y * width;
	  d = data[i - width] - data[i];
	  if (d > dmax)
	    {
	      dmax = d;
	      dpos = y;
	    }
	  if ((DBG_LEVEL > 128) && (dbuffer != NULL))
	    {
	      dbuffer[i * 3] = data[i];
	      dbuffer[i * 3 + 1] = data[i];
	      dbuffer[i * 3 + 2] = data[i];
	    }
	}
      epos += dpos;
      ecnt++;
      if ((DBG_LEVEL > 128) && (dbuffer != NULL))
	{
	  dbuffer[(x + dpos * width) * 3] = 0xFF;
	  dbuffer[(x + dpos * width) * 3 + 1] = 0x00;
	  dbuffer[(x + dpos * width) * 3 + 2] = 0x00;
	}
    }
  if (ecnt == 0)
    epos = 70;
  else
    epos = epos / ecnt;
  if ((DBG_LEVEL > 128) && (dbuffer != NULL))
    {
      i = ((int) epos) * width;
      for (x = 0; x < width; x++)
	{
	  dbuffer[(x + i) * 3] = 0x00;
	  dbuffer[(x + i) * 3 + 1] = 0xFF;
	  dbuffer[(x + i) * 3 + 2] = 0xFF;
	}
      for (y = 0; y < height; y++)
	{
	  dbuffer[(width / 2 + y * width) * 3] = 0x00;
	  dbuffer[(width / 2 + y * width) * 3 + 1] = 0xFF;
	  dbuffer[(width / 2 + y * width) * 3 + 2] = 0x00;
	}
      DumpRGB (width, height, dbuffer, NULL);
      free (dbuffer);
    }
  return epos;
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
  DBG (32, "MAX VALUE=%f        (%s:%d)\n", edge, __FILE__, __LINE__);
  if ((edge <= 30) && (sanei_umax_pp_getastra () != 1600))
    {
      DBG (2, "MoveToOrigin() detected a 1600P");
      sanei_umax_pp_setastra (1600);
    }
  edge = EdgePosition (300, 180, buffer);
  /* rounded to lowest integer, since upping origin might lead */
  /* to bump in the other side if doing a full size preview    */
  val = (int) (edge);

  /* edge is 60 dots (at 600 dpi) from origin */
  /* origin=current pos - 180 + edge + 60     */
  /* grumpf, there is an offset somewhere ... */
  delta = -188 + val;
  DBG (64, "Edge=%f, val=%d, delta=%d\n", edge, val, delta);

  /* move back to y-coordinate origin */
  MOVE (delta, PRECISION_ON, NULL);

  /* head successfully set to the origin */
  return 1;
}


/* computes color offset and gain */
/* X is red
   Y is blue
   Z is green

   returns if OK, else 0
*/

static int
WarmUp (int color, int *brightness)
{
  unsigned char buffer[5300];
  int i, val, min, max;
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
  /* auto brightness computing */
	/***********************/

  /* color correction set to 53 05 */
  /* for a start                   */
  *brightness = 0x535;
  CMDSETGET (2, 0x10, opsc18);
  CMDSETGET (8, 0x24, opsc39);
  opsc04[7] = opsc04[7] & 0x20;
  opsc04[6] = 0x06;		/* one channel brightness value */
  CMDSETGET (1, 0x08, opsc10);	/* was opsc04, extraneaous string */
  /* that prevents using Move .... */
  CMDSYNC (0xC2);
  CMDSYNC (0x00);
  CMDSETGET (4, 0x08, opsc02);
  COMPLETIONWAIT;
  CMDGETBUF (4, 0x200, buffer);
  if (DBG_LEVEL >= 128)
    Dump (0x200, buffer, NULL);


  /* auto correction of brightness levels */
  /* first color component X        */
  opsc51[10] = (*brightness) / 16;	/* channel 1 & 2 brightness */
  opsc51[11] = (*brightness) % 16;	/* channel 3 brightness     */
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
      opsc04[6] = (*brightness) / 256;
      CMDSETGET (1, 0x08, opsc04);
      CMDSYNC (0xC2);
      CMDSYNC (0x00);
      CMDSETGET (4, 0x08, opsc02);
      COMPLETIONWAIT;
      CMDGETBUF (4, 0x14B4, buffer);
      if (DBG_LEVEL >= 128)
	Dump (0x14B4, buffer, NULL);
      min = 255;
      max = 0;
      for (i = 0; i < 0x14B4; i++)
	{
	  if (buffer[i] < min)
	    min = buffer[i];
	  if (buffer[i] > max)
	    max = buffer[i];
	}
      while ((opsc04[6] < 0x0F) && (max < 250))
	{
	  CMDSYNC (0x00);
	  opsc04[6]++;
	  CMDSETGET (1, 0x000008, opsc04);
	  COMPLETIONWAIT;
	  CMDGETBUF (4, 0x0014B4, buffer);
	  if (DBG_LEVEL >= 128)
	    Dump (0x14B4, buffer, NULL);
	  min = 255;
	  max = 0;
	  for (i = 0; i < 0x14B4; i++)
	    {
	      if (buffer[i] < min)
		min = buffer[i];
	      if (buffer[i] > max)
		max = buffer[i];
	    }
	}

      *brightness = (*brightness & 0xFF) + 256 * (opsc04[6] - 1);
      opsc51[10] = *brightness / 16;	/* channel 1 & 2 brightness */
      opsc51[11] = *brightness % 16;	/* channel 3 brightness     */
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
      opsc04[6] = (*brightness & 0x00F);
      CMDSETGET (1, 0x08, opsc04);
      CMDSYNC (0xC2);
      CMDSYNC (0x00);
      CMDSETGET (4, 0x08, opsc02);
      COMPLETIONWAIT;
      CMDGETBUF (4, 0x14B4, buffer);
      if (DBG_LEVEL >= 128)
	Dump (0x14B4, buffer, NULL);
      min = 255;
      max = 0;
      for (i = 0; i < 0x14B4; i++)
	{
	  if (buffer[i] < min)
	    min = buffer[i];
	  if (buffer[i] > max)
	    max = buffer[i];
	}

      while ((opsc04[6] < 0x0F) && (max < 250))
	{
	  CMDSYNC (0x00);
	  opsc04[6]++;
	  CMDSETGET (1, 0x000008, opsc04);
	  COMPLETIONWAIT;
	  CMDGETBUF (4, 0x0014B4, buffer);
	  if (DBG_LEVEL >= 128)
	    Dump (0x14B4, buffer, NULL);
	  min = 255;
	  max = 0;
	  for (i = 0; i < 0x14B4; i++)
	    {
	      if (buffer[i] < min)
		min = buffer[i];
	      if (buffer[i] > max)
		max = buffer[i];
	    }
	}
      *brightness = (*brightness & 0xFF0) + (opsc04[6] - 1);
    }





  /* component Z: B&W component */
  opsc51[10] = *brightness / 16;	/* channel 1 & 2 brightness */
  opsc51[11] = *brightness % 16;	/* channel 3 brightness     */
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
  opsc04[6] = (*brightness & 0x0F0) / 16;
  CMDSETGET (1, 0x08, opsc04);
  CMDSYNC (0xC2);
  CMDSYNC (0x00);
  CMDSETGET (4, 0x08, opsc02);
  COMPLETIONWAIT;
  CMDGETBUF (4, 0x14B4, buffer);
  if (DBG_LEVEL >= 128)
    Dump (0x14B4, buffer, NULL);
  min = 255;
  max = 0;
  for (i = 0; i < 0x14B4; i++)
    {
      if (buffer[i] < min)
	min = buffer[i];
      if (buffer[i] > max)
	max = buffer[i];
    }
  while ((opsc04[6] < 0x07) && (max < 250))
    {
      CMDSYNC (0x00);
      opsc04[6]++;
      CMDSETGET (1, 0x08, opsc04);
      COMPLETIONWAIT;
      CMDGETBUF (4, 0x0014B4, buffer);
      if (DBG_LEVEL >= 128)
	Dump (0x14B4, buffer, NULL);
      min = 255;
      max = 0;
      for (i = 0; i < 0x14B4; i++)
	{
	  if (buffer[i] < min)
	    min = buffer[i];
	  if (buffer[i] > max)
	    max = buffer[i];
	}
    }
  *brightness = (*brightness & 0xF0F) + (opsc04[6] - 1) * 16;
  DBG (1, "Warm-up done ...\n");
  return 1;
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
  return 1;
}


/* calibrates CCD: returns 1 on success, 0 on failure */
static int
ColorCalibration (int color, int dpi, int brightness, int contrast, int width,
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

  /* step back by 67 ticks:                     */
  /* since we're going to scan 66 lines of data */
  /* which are going to be used as calibration  */
  /* data                                       */
  /* we are on the white area just before       */
  /* the user scan area                         */
  MOVE (-67, PRECISION_ON, NULL);


  CMDSYNC (0x00);

  /* get calibration data */
  if (sanei_umax_pp_getauto ())
    {				/* auto settings doesn't use contrast */
      contrast = 0x000;
    }
  else
    {				/* manual settings */
      brightness = 0x777;
      contrast = 0x000;
    }
  opsc32[10] = brightness / 16;
  opsc32[11] = brightness % 16 | ((contrast / 16) & 0xF0);
  opsc32[12] = contrast % 256;
  DBG (8, "USING 0x%03X brightness, 0x%03X contrast\n", brightness, contrast);
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
	  DumpRVB (5100, 66, buffer, NULL);
	}
      else
	{
	  DumpNB (5100, 66, buffer, NULL);
	}
    }
  ComputeCalibrationData (color, dpi, width, buffer, calibration);

  DBG (1, "Color calibration done ...\n");
  return 1;
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
  return len;
}

int
sanei_umax_pp_Scan (int x, int y, int width, int height, int dpi, int color,
		    int brightness, int contrast)
{
#ifdef HAVE_SYS_TIME_H
  struct timeval td, tf;
  float elapsed;
#endif
  unsigned char *buffer;
  long int somme, len, read, blocksize;
  FILE *fout = NULL;
  int *dest = NULL;
  int bpl, hp;
  int th, tw, bpp;
  int distance = 0, nb;
  int bx, by;

  if (sanei_umax_pp_StartScan
      (x, y, width, height, dpi, color, brightness, contrast, &bpp, &tw,
       &th) == 1)
    {
      COMPLETIONWAIT;

      /* blocksize must be multiple of the number of bytes per line */
      /* max is 2096100                                             */
      /* so blocksize will hold a round number of lines, easing the */
      /* write data to file operation                               */
      /*blocksize=(2096100/bpl)*bpl; */
      bpl = bpp * tw;
      hp = 16776960 / bpl;	/* 16 Mo buffer (!!) */
      hp = 2096100 / bpl;
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
#ifdef HAVE_SYS_TIME_H
      gettimeofday (&td, NULL);
#endif
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

#ifdef HAVE_SYS_TIME_H
      gettimeofday (&tf, NULL);

      /* scan time are high enough to forget about usec */
      elapsed = tf.tv_sec - td.tv_sec;
      DBG (8, "%ld bytes transfered in %f seconds ( %.2f Kb/s)\n", somme,
	   elapsed, (somme / elapsed) / 1024.0);
#endif

      /* release ressources */
      if (fout != NULL)
	fclose (fout);
      free (dest);
      free (buffer);


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
  return 1;
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
  return 1;
}






/* starts scan: return 1 on success */
int
sanei_umax_pp_StartScan (int x, int y, int width, int height, int dpi,
			 int color, int brightness, int contrast, int *rbpp,
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


#ifdef UMAX_PP_DANGEROUS_EXPERIMENT
  FILE *f = NULL;
  char line[1024], *ptr;
  int *base = NULL;
  int channel;
  int max = 0;
#endif


  DBG (8, "StartScan(%d,%d,%d,%d,%d,%d,%X);\n", x, y, width, height, dpi,
       color, brightness);
  buffer = (unsigned char *) malloc (2096100);
  if (buffer == NULL)
    {
      DBG (0, "Failed to allocate 2096100 bytes... (%s:%d)\n", __FILE__,
	   __LINE__);
      return 0;
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
	  return 0;
	}

      /* init some buffer : default calibration data ? */
      /* looks like a standard gamma table             */
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
	return 0;
    }


  /* new part of buffer ... */
  for (i = 0; i < 256; i++)
    {
      dest[i * 2] = i;
      dest[i * 2 + 1] = 0;
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
    return 0;

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


  /* adjust brightness and color offset */
  /* red*256+green*16+blue        */
  if (sanei_umax_pp_getauto ())
    {
      if (WarmUp (color, &brightness) == 0)
	{
	  DBG (0, "Warm-up failed !!! (%s:%d)\n", __FILE__, __LINE__);
	  return 0;
	}
    }

  /* x dpi is from 75 to 600 max, any modes */
  if (dpi > 600)
    xdpi = 600;
  else
    xdpi = dpi;


  /* EPPRead32Buffer does not work                         */
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
  if (ColorCalibration (color, dpi, brightness, contrast, width, calibration)
      == 0)
    {
      DBG (0, "Gamma calibration failed !!! (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  TRACE (16, "ColorCalibration() passed ...")
    /* it is faster to move at low resolution, then scan */
    /* than scan & move at high resolution               */
    distance = 0;

  /* work around some strange unresolved bug */
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
      opsc53[8] = 0x5E;		/* *WORKING* value */
      opsc53[8] = 0x5F;		/* 5F gives wrong colors ? */
      opsc53[8] = 0x58;
      opsc53[9] = 0x05;
      /* XXX test value XXX opsc53[14] = opsc53[14] & 0xF0;  ~ 0x08 -> scan AND move */
      /* XXX test value XXX opsc53[14] = (opsc53[14] & 0xF0) | 0x04;         -> 600 dpi ? */
      /* XXX test value XXX opsc53[14] = (opsc53[14] & 0xF0) | 0x0C; */
      opsc53[14] = opsc53[14] & 0xF0;	/* *WORKING* 1200 dpi */
      break;

    case 600:
      opsc53[6] = 0x60;
      opsc53[8] = 0x2F;
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

  /* channels brightness */
  opsc53[10] = brightness / 16;
  opsc53[11] = ((contrast / 16) & 0xF0) | (brightness % 16);
  opsc53[12] = contrast % 256;

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
      opsc53[7] = 0x2F;
      /* 00 seems to give better results ?     */
      /* 80 some more brightness, lamp power level ? */
      /* 8x does not make much difference      */
      opsc04[6] = 0x8F;
      if (sanei_umax_pp_getastra () == 1600)
	{
	  opsc04[7] = 0x70;
	  opsc53[13] = 0x03;
	}
      else
	{
	  opsc04[7] = 0xF0;
	  opsc53[13] = 0x09;
	}
    }
  else
    {
      opsc53[7] = 0x40;
      opsc53[13] = 0xC0;
      opsc04[6] = 0x80 | ((brightness / 16) & 0x0F);
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
#ifdef UMAX_PP_DANGEROUS_EXPERIMENT
  /*opsc53[13] = 0x80;           B&W bit */
  /*opsc53[13] = 0x40;           green bit */
  /*opsc53[13] = 0x20;           red bit */
  /*opsc53[13] = 0x10;           blue bit */
  /* with cmd 01, may be use to do 3 pass scanning ? */
  /* bits 0 to 3 seem related to sharpness */
  f = fopen ("/tmp/dangerous.params", "rb");
  if (f != NULL)
    {
      fgets (line, 1024, f);
      while (!feof (f))
	{
	  channel = 0;
	  if (sscanf (line, "CMD%1d", &channel) != 1)
	    channel = 0;
	  switch (channel)
	    {
	    case 0:
	      break;
	    case 1:
	      base = opsc04;
	      max = 8;
	      break;
	    case 2:
	      base = opsc53;
	      max = 16;
	      break;
	    case 8:
	      base = opscan;
	      max = 36;
	      break;
	    default:
	      channel = 0;
	    }
	  printf ("CMD%d BEFORE: ", channel);
	  for (i = 0; i < max; i++)
	    printf ("%02X ", base[i]);
	  printf ("\n");
	  if (channel > 0)
	    {
	      ptr = line + 6;
	      for (i = 0; (i < max) && ((ptr - line) < strlen (line)); i++)
		{
		  if (ptr[0] != '-')
		    {
		      sscanf (ptr, "%X", base + i);
		    }
		  ptr += 3;
		}
	    }
	  printf ("CMD%d AFTER : ", channel);
	  for (i = 0; i < max; i++)
	    printf ("%02X ", base[i]);
	  printf ("\n");
	  fgets (line, 1024, f);
	}
      fclose (f);
    }
#endif


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
  return 1;
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
  if (sanei_umax_pp_getastra ())
    return sanei_umax_pp_getastra ();

  /* get scanner status */
  CMDGET (0x02, 16, state);
  CMDSETGET (0x08, 36, opsc35);
  CMDSYNC (0xC2);

  dest = (int *) malloc (65536 * 4);
  if (dest == NULL)
    {
      DBG (0, "%s:%d failed to allocate 256 Ko !\n", __FILE__, __LINE__);
      return 0;
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
    return 0;


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
      /* we defaults to 1220 */
      sanei_umax_pp_setastra (1220);
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
  return err;
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
