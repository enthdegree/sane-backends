/* sane - Scanner Access Now Easy.
   Copyright (C) 1996, 1997 David Mosberger-Tang
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

   This file implements a dynamic linking based SANE meta backend.  It
   allows managing an arbitrary number of SANE backends by using
   dynamic linking to load backends on demand.  */
/* ======================================================================

Userspace scan tool for the Microtek 3600 scanner

$Id$

====================================================================== */

#include "sm3600-scantool.h"

/* **********************************************************************

dprintf(DEBUG_XXXX, format, ...)

Put a debug message on STDERR (or whatever). The message is prefixed with
a "debug:" and given, if the current debugging flags contain the given
flag "ulType".

********************************************************************** */

#ifdef INSANE_VERSION
void DBG(int nLevel, const char *szFormat, ...)
{
  szFormat++;
}
#endif

void debug_printf(unsigned long ulType, const char *szFormat, ...)
{
  va_list ap;
  if ((ulDebugMask & ulType)!=ulType) return;
  if (*szFormat=='~')
    szFormat++;
  else
    fprintf(stderr,"debug:");
  va_start(ap,szFormat);
  vfprintf(stderr,szFormat,ap);
  va_end(ap);
}

/* **********************************************************************

SetError(error, format, ...)

The program is aborted, all handles and ressources are freed (this
being global) and the user gets a nice panic screen :-)

********************************************************************** */

int SetError(TInstance *this, int nError, const char *szFormat, ...)
{
  va_list ap;
  if (this->nErrorState) return 0; /* do not overwrite error state */
  this->nErrorState=nError;
  this->szErrorReason=malloc(500);
  
  if (szFormat!=NULL && this->szErrorReason)
    {
      va_start(ap,szFormat);
      vsnprintf(this->szErrorReason,499,szFormat,ap);
      va_end(ap);
      this->szErrorReason[499]='\0';
    }
  return nError;
}

/* **********************************************************************

DumpBuffer(fh,pch,cch)

********************************************************************** */

void DumpBuffer(FILE *fh, const char *pch, int cch)
{
  int i=0;
  while (i<cch)
    {
      if (!(i & 15))
	{
	  if (i) fprintf(fh,"\n");
	  fprintf(fh,"%04X:",i);
	}
      fprintf(fh," %02X",(unsigned char)pch[i]);
      i++;
    }
  fprintf(fh,"\n");
}

/* **********************************************************************

FixExposure()

Exposure is done by adding brightness to the original scan value and
augmenting the result around the middle value of 128.

********************************************************************** */

void FixExposure(unsigned char *pchBuf,
		 int cchBulk,
		 int nBrightness,
		 int nContrast)
{
  int i;
  int nOffB=nBrightness-128;
  int nFakC=(nContrast+128)*100; /* in percent, to get smoother interpolation */
  for (i=0; i<cchBulk; i++)
  {
    int nNew=pchBuf[i];
    nNew=(nNew+nOffB)*nFakC/12800+128;
    if (nNew<0) nNew=0;
    else if (nNew>255) nNew=255;
    pchBuf[i]=(unsigned char)nNew;
  }
}



/* **********************************************************************

FreeState()

Frees all dynamical memory for scan buffering.

********************************************************************** */

TState FreeState(TInstance *this, TState nReturn)
{
  if (this->state.ppchLines)
    {
      int i;
      for (i=0; i<this->state.cBacklog; i++)
	{
	  if (this->state.ppchLines[i])
	    free(this->state.ppchLines[i]);
	}
      free(this->state.ppchLines);
    }
  if (this->state.pchLineOut) free(this->state.pchLineOut);
  if (this->state.pchBuf)     free(this->state.pchBuf);
  this->state.pchBuf    =NULL;
  this->state.pchLineOut=NULL;
  this->state.ppchLines =NULL;
  return nReturn;
}

/* ======================================================================

EndScan()

====================================================================== */

TState EndScan(TInstance *this)
{
  if (!this->state.bScanning) return SANE_STATUS_GOOD;
  /* move slider back to start */
  this->state.bScanning=false;
  FreeState(this,0);
  INST_ASSERT();
  return DoJog(this,-this->state.cyTotalPath);
}

/* ======================================================================

TState CancelScan(TInstance *this)

====================================================================== */

TState CancelScan(TInstance *this)
{
  TBool bCanceled;
  DBG(DEBUG_INFO,"CancelScan() called\n");

  this->state.cyTotalPath-=RegRead(this,R_POS,2);
  DBG(DEBUG_JUNK,"stepping back %d steps\n",this->state.cyTotalPath);
  /* this->state.cyTotalPath=0; */

  usleep(200);
  DoReset(this);
  EndScan(this); /* and step back! */
  
  DBG(DEBUG_JUNK,"cs4: %d\n",(int)this->nErrorState);
  bCanceled=this->state.bCanceled;
  this->state.bCanceled=false; /* re-enable Origination! */
  DoOriginate(this,false); /* have an error here... */
  this->state.bCanceled=bCanceled;
  DBG(DEBUG_JUNK,"cs5: %d\n",(int)this->nErrorState);
  INST_ASSERT();
  DBG(DEBUG_INFO,"cs6: ok.\n");
  return SANE_STATUS_CANCELLED; /* or shall be say GOOD? */
}


/* ======================================================================

ReadChunk()

====================================================================== */

TState ReadChunk(TInstance *this, unsigned char *achOut,
		 int cchMax, int *pcchRead)
{
  /* have we to copy more than we have? */
  /* can the current line fill the buffer ? */
  int rc;
  *pcchRead=0;
  INST_ASSERT();
  if (!this->state.bScanning)
    return SANE_STATUS_CANCELLED; /* deferred cancel? */
  if (this->state.bCanceled) /* deferred cancellation? */
    return CancelScan(this);
  INST_ASSERT();
  /* 22.4.2001: This took me hard, harder, hardest:*/

  /*   We need to fill the line buffer with at least a *rest* of a
       line. A single line will do. */
  /*     Thus, "iLine>0" is a suitable condition. */
  /*   Without the preread, there will a dummy line be read, if the
       target buffer is large enough.*/
  if (this->state.iLine)
    rc=SANE_STATUS_GOOD;
  else
    rc=(*(this->state.ReadProc))(this); /* preread one line */
  if (rc!=SANE_STATUS_GOOD) return rc;
  dprintf(DEBUG_BUFFER,"Chunk-Init: cchMax = %d\n",cchMax);
  while (this->state.iReadPos + cchMax > this->state.cchLineOut)
    {
      int cch;
      /* copy rest of the line into target */
      cch = this->state.cchLineOut - this->state.iReadPos;
      memcpy(achOut,
	     this->state.pchLineOut+this->state.iReadPos,
	     cch);
      cchMax-=cch; /* advance parameters */
      achOut+=cch;
      (*pcchRead)+=cch;
      this->state.iReadPos=0;
      rc=(*(this->state.ReadProc))(this);
      dprintf(DEBUG_BUFFER,"Chunk-Read: cchMax = %d\n",cchMax);
      if (rc!=SANE_STATUS_GOOD)
	return rc; /* should be NOT(!) EOF, but then: good and away! */
    }
  dprintf(DEBUG_BUFFER,"Chunk-Exit: cchMax = %d\n",cchMax);
  if (!cchMax) return SANE_STATUS_GOOD; /* now everything fits! */
  (*pcchRead) += cchMax;
  memcpy(achOut,
	 this->state.pchLineOut+this->state.iReadPos,
	 cchMax);
  this->state.iReadPos += cchMax;
  return SANE_STATUS_GOOD;
}

/* ======================================================================

GetAreaSize()

====================================================================== */

void GetAreaSize(TInstance *this)
{
  /* this->state.cxPixel : pixels, we *want* (after interpolation)
     this->state.cxMax   : pixels, we *need* (before interpolation) */
  int nRefResX,nRefResY;
  nRefResX=nRefResY=this->param.res;
  switch (this->param.res)
    {
    case 75:  nRefResX=100; this->state.nFixAspect=75; break;
    default: this->state.nFixAspect=100; break;
    }
  this->state.cxPixel   =this->param.cx*this->param.res/1200;
  this->state.cyPixel   =this->param.cy*this->param.res/1200;
  this->state.cxMax     =this->state.cxPixel*100/this->state.nFixAspect;
  this->state.cxWindow  =this->state.cxMax*600/nRefResX;
  this->state.cyWindow  =this->state.cyPixel*600/nRefResY;
  dprintf(DEBUG_SCAN,"requesting %d[600] %d[real] %d[raw]\n",
	  this->state.cxWindow,this->state.cxPixel,this->state.cxMax);
}

/* ======================================================================

InitGammaTables()

Init gammy tables and gain tables within controller memory.

====================================================================== */

TState InitGammaTables(TInstance *this)
{
  int           i;
  for (i=0; i<4096; i++)
    {
      this->agammaGray[i]=i;
      this->agammaR[i]=i;
      this->agammaG[i]=i;
      this->agammaB[i]=i;
    }
  return SANE_STATUS_GOOD;
}

#ifdef INSANE_VERSION

/* ======================================================================

DoScanFile()

Top level caller for scantool.

====================================================================== */

#define APP_CHUNK_SIZE   8192

TState DoScanFile(TInstance *this)
{
  int    cx,cy;
  long   lcchRead;
  TState rc;
  char   *achBuf;

  achBuf=malloc(APP_CHUNK_SIZE);
  rc=SANE_STATUS_GOOD; /* make compiler happy */
  rc=InitGammaTables(this);
  if (rc!=SANE_STATUS_GOOD) return rc;
  if (this->mode==color)
    rc=StartScanColor(this);
  else
    rc=StartScanGray(this);
  cx=this->state.cxPixel;
  cy=this->state.cyPixel;
  if (this->bVerbose)
    fprintf(stderr,"scanning %d by %d\n",cx,cy);
  if (this->fhScan && !this->bWriteRaw)
   {
      switch (this->mode)
	{
	case color: fprintf(this->fhScan,"P6\n%d %d\n255\n",cx,cy);
	            break;
	case gray:  fprintf(this->fhScan,"P5\n%d %d\n255\n",cx,cy);
                    break;
	default:    fprintf(this->fhScan,"P4\n%d %d\n",cx,cy);
                    break;
	}
    }
  lcchRead=0L;
  while (!rc)
    {
      int cch;
      cch=0;
      rc=ReadChunk(this,achBuf,APP_CHUNK_SIZE,&cch);
      if (cch>0 && this->fhScan && cch<=APP_CHUNK_SIZE)
	{
	  if (!this->bWriteRaw)
	    fwrite(achBuf,1,cch,this->fhScan);
	  lcchRead+=cch;
	}
     }
  free(achBuf);
  if (this->bVerbose)
    fprintf(stderr,"read %ld image byte(s)\n",lcchRead);
  EndScan(this);
  INST_ASSERT();
  return SANE_STATUS_GOOD;
}

#endif
