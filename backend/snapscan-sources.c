/* sane - Scanner Access Now Easy.
 
   Copyright (C) 1997, 1998 Franck Schnefra, Michel Roelofs,
   Emmanuel Blot, Mikko Tyolajarvi, David Mosberger-Tang, Wolfgang Goeller,
   Petter Reinholdtsen, Gary Plewa, and Kevin Charter
 
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
 
   This file is a component of the implementation of a backend for many
   of the the AGFA SnapScan and Acer Vuego/Prisa flatbed scanners. */


/* $Id$
   SnapScan backend data sources (implementation) */

#ifndef __FUNCTION__
#define __FUNCTION__ "(undef)"
#endif

static SANE_Status Source_init (Source *pself,
                                SnapScan_Scanner *pss,
                                SourceRemaining remaining,
                                SourceBytesPerLine bytesPerLine,
                                SourcePixelsPerLine pixelsPerLine,
                                SourceGet get,
                                SourceDone done)
{
    pself->pss = pss;
    pself->remaining = remaining;
    pself->bytesPerLine = bytesPerLine;
    pself->pixelsPerLine = pixelsPerLine;
    pself->get = get;
    pself->done = done;
    return SANE_STATUS_GOOD;
}

/* these are defaults, normally used only by base sources */

static SANE_Int Source_bytesPerLine (Source *pself)
{
    return pself->pss->bytes_per_line;
}

static SANE_Int Source_pixelsPerLine (Source *pself)
{
    return pself->pss->pixels_per_line;
}

/**********************************************************************/

/* the base sources */
typedef enum
{
    SCSI_SRC,
    FD_SRC,
    BUF_SRC
} BaseSourceType;

#define SCSISOURCE_BAD_TIME -1

typedef struct
{
    SOURCE_GUTS;
    SANE_Int scsi_buf_pos;	/* current position in scsi buffer */
    SANE_Int scsi_buf_max;	/* data limit */
    SANE_Int absolute_max;	/* largest possible data read */
    struct timeval time;	/* time of last scsi read (usec) */
} SCSISource;

static SANE_Int SCSISource_remaining (Source *pself)
{
    SCSISource *ps = (SCSISource *) pself;
    return ps->pss->bytes_remaining + (ps->scsi_buf_max - ps->scsi_buf_pos);
}

static SANE_Status SCSISource_get (Source *pself,
                                   SANE_Byte *pbuf,
				   SANE_Int *plen)
{
    SCSISource *ps = (SCSISource *) pself;
    SANE_Status status = SANE_STATUS_GOOD;
    SANE_Int remaining = *plen;
    while (remaining > 0
           &&
           pself->remaining(pself) > 0
	   &&
           status == SANE_STATUS_GOOD)
    {
        SANE_Int ndata = ps->scsi_buf_max - ps->scsi_buf_pos;
        if (ndata == 0)
        {
            /* read more data */
            struct timeval oldtime = ps->time;
            if (ps->time.tv_sec != SCSISOURCE_BAD_TIME
                &&
                gettimeofday(&(ps->time), NULL) == 0)
	    {
                /* estimate number of lines to read from the elapsed time
                   since the last read and the scanner's reported speed */
                double msecs = (ps->time.tv_sec - oldtime.tv_sec)*1000.0
		             + (ps->time.tv_usec - oldtime.tv_usec)/1000.0;
                ps->pss->expected_read_bytes =
                    ((int) (msecs/ps->pss->ms_per_line))*ps->pss->bytes_per_line;
            }
            else
            {
                /* use the "lines_per_read" values */
                SANE_Int lines;

                if (is_colour_mode(actual_mode(ps->pss)) == SANE_TRUE)
                    lines = ps->pss->rgb_lpr;
                else
                    lines = ps->pss->gs_lpr;
                ps->pss->expected_read_bytes = lines * ps->pss->bytes_per_line;
            }
            ps->pss->expected_read_bytes = MIN(ps->pss->expected_read_bytes,
	                                       ps->pss->bytes_remaining);
            ps->pss->expected_read_bytes = MIN(ps->pss->expected_read_bytes,
	                                       (size_t) ps->absolute_max);
            ps->scsi_buf_pos = 0;
            ps->scsi_buf_max = 0;
            status = scsi_read (ps->pss, READ_IMAGE);
            if (status != SANE_STATUS_GOOD)
                break;
            ps->scsi_buf_max = ps->pss->read_bytes;
            ndata = ps->pss->read_bytes;
            ps->pss->bytes_remaining -= ps->pss->read_bytes;
        }
        ndata = MIN(ndata, remaining);
        memcpy (pbuf, ps->pss->buf + ps->scsi_buf_pos, (size_t)ndata);
        pbuf += ndata;
        ps->scsi_buf_pos += ndata;
        remaining -= ndata;
    }
    *plen -= remaining;
    return status;
}

static SANE_Status SCSISource_done (Source *pself)
{
    UNREFERENCED_PARAMETER(pself);
    return SANE_STATUS_GOOD;
}

static SANE_Status SCSISource_init (SCSISource *pself, SnapScan_Scanner *pss)
{
    SANE_Status status = Source_init ((Source *) pself, pss,
                                      SCSISource_remaining,
                                      Source_bytesPerLine,
		                      Source_pixelsPerLine,
                                      SCSISource_get,
		                      SCSISource_done);
    if (status == SANE_STATUS_GOOD)
    {
        pself->scsi_buf_max = 0;
        pself->scsi_buf_pos = 0;
        pself->absolute_max =
            (SCANNER_BUF_SZ/pss->bytes_per_line)*pss->bytes_per_line;
        if (gettimeofday(&(pself->time), NULL) != 0)
        {
            DBG (DL_MAJOR_ERROR,
	         "%s: error in gettimeofday(): %s\n",
                 __FUNCTION__,
		 strerror(errno));
            pself->time.tv_sec = SCSISOURCE_BAD_TIME;
            pself->time.tv_usec = SCSISOURCE_BAD_TIME;
        }
    }
    return status;
}

/* File sources */

typedef struct
{
    SOURCE_GUTS;
    int fd;
} FDSource;

static SANE_Int FDSource_remaining (Source *pself)
{
    return pself->pss->bytes_remaining;
}

static SANE_Status FDSource_get (Source *pself, SANE_Byte *pbuf, SANE_Int *plen)
{
    SANE_Status status = SANE_STATUS_GOOD;
    FDSource *ps = (FDSource *) pself;
    SANE_Int remaining = *plen;

    while (remaining > 0
           &&
           pself->remaining(pself) > 0
	   &&
           status == SANE_STATUS_GOOD)
    {
        SANE_Int bytes_read = read (ps->fd, pbuf, remaining);
        if (bytes_read == -1)
        {
            if (errno == EAGAIN)
	    {
                /* No data currently available */
                break;
            }
	    /* It's an IO error */
            DBG (DL_MAJOR_ERROR,
	         "%s: read failed: %s\n",
                 __FUNCTION__,
		 strerror(errno));
            status = SANE_STATUS_IO_ERROR;
        }
        else if (bytes_read == 0)
	{
            break;
	}
        ps->pss->bytes_remaining -= bytes_read;
        remaining -= bytes_read;
        pbuf += bytes_read;
    }

    *plen -= remaining;
    return status;
}

static SANE_Status FDSource_done (Source *pself)
{
    close(((FDSource *) pself)->fd);
    return SANE_STATUS_GOOD;
}

static SANE_Status FDSource_init (FDSource *pself,
                                  SnapScan_Scanner *pss,
				  int fd)
{
    SANE_Status status = Source_init ((Source *) pself,
                                      pss,
                                      FDSource_remaining,
                                      Source_bytesPerLine,
		                      Source_pixelsPerLine,
                                      FDSource_get,
		                      FDSource_done);
    if (status == SANE_STATUS_GOOD)
        pself->fd = fd;
    return status;
}


/* buffer sources simply read from a pre-filled buffer; we have these
   so that we can include source chain processing overhead in the
   measure_transfer_rate() function */

typedef struct
{
    SOURCE_GUTS;
    SANE_Byte *buf;
    SANE_Int buf_size;
    SANE_Int buf_pos;
} BufSource;

static SANE_Int BufSource_remaining (Source *pself)
{
    BufSource *ps = (BufSource *) pself;
    return  ps->buf_size - ps->buf_pos;
}

static SANE_Status BufSource_get (Source *pself,
                                  SANE_Byte *pbuf,
                                  SANE_Int *plen)
{
    BufSource *ps = (BufSource *) pself;
    SANE_Status status = SANE_STATUS_GOOD;
    SANE_Int to_move = MIN(*plen, pself->remaining(pself));
    if (to_move == 0)
    {
        status = SANE_STATUS_EOF;
    }
    else
    {
        memcpy (pbuf, ps->buf + ps->buf_pos, to_move);
        ps->buf_pos += to_move;
        *plen = to_move;
    }
    return status;
}

static SANE_Status BufSource_done (Source *pself)
{
    UNREFERENCED_PARAMETER(pself);
    return SANE_STATUS_GOOD;
}

static SANE_Status BufSource_init (BufSource *pself,
                                   SnapScan_Scanner *pss,
                                   SANE_Byte *buf,
				   SANE_Int buf_size)
{
    SANE_Status status = Source_init ((Source *) pself,
				      pss,
                                      BufSource_remaining,
                                      Source_bytesPerLine,
		                      Source_pixelsPerLine,
                                      BufSource_get,
		                      BufSource_done);
    if (status == SANE_STATUS_GOOD)
    {
        pself->buf = buf;
        pself->buf_size = buf_size;
        pself->buf_pos = 0;
    }
    return status;
}

/* base source creation */

static SANE_Status create_base_source (SnapScan_Scanner *pss,
                                       BaseSourceType st,
                                       Source **pps)
{
    SANE_Status status = SANE_STATUS_GOOD;
    *pps = NULL;
    switch (st)
    {
    case SCSI_SRC:
        *pps = (Source *) malloc(sizeof(SCSISource));
        if (*pps == NULL)
        {
            DBG (DL_MAJOR_ERROR, "failed to allocate SCSISource");
            status = SANE_STATUS_NO_MEM;
        }
        else
	{
            status = SCSISource_init ((SCSISource *) *pps, pss);
        }
	break;
    case FD_SRC:
        *pps = (Source *) malloc(sizeof(FDSource));
        if (*pps == NULL)
        {
            DBG (DL_MAJOR_ERROR, "failed to allocate FDSource");
            status = SANE_STATUS_NO_MEM;
        }
        else
	{
            status = FDSource_init ((FDSource *) *pps, pss, pss->rpipe[0]);
        }
	break;
    case BUF_SRC:
        *pps = (Source *) malloc(sizeof(BufSource));
        if (*pps == NULL)
        {
            DBG (DL_MAJOR_ERROR, "failed to allocate BufSource");
            status = SANE_STATUS_NO_MEM;
        }
        else
	{
            status = BufSource_init ((BufSource *) *pps,
                                     pss,
                                     pss->buf,
                                     pss->read_bytes);
        }
	break;
    default:
        DBG (DL_MAJOR_ERROR, "illegal base source type %d", st);
        break;
    }
    return status;
}

/**********************************************************************/

/* The transformer sources */

#define TX_SOURCE_GUTS \
SOURCE_GUTS;\
Source *psub			/* sub-source */

typedef struct
{
    TX_SOURCE_GUTS;
} TxSource;

static SANE_Int TxSource_remaining (Source *pself)
{
    TxSource *ps = (TxSource *) pself;
    return ps->psub->remaining(ps->psub);
}

static SANE_Int TxSource_bytesPerLine (Source *pself)
{
    TxSource *ps = (TxSource *) pself;
    return ps->psub->bytesPerLine(ps->psub);
}

static SANE_Int TxSource_pixelsPerLine (Source *pself)
{
    TxSource *ps = (TxSource *) pself;
    return ps->psub->pixelsPerLine(ps->psub);
}

static SANE_Status TxSource_get (Source *pself, SANE_Byte *pbuf, SANE_Int *plen)
{
    TxSource *ps = (TxSource *) pself;
    return ps->psub->get(ps->psub, pbuf, plen);
}

static SANE_Status TxSource_done (Source *pself)
{
    TxSource *ps = (TxSource *) pself;
    SANE_Status status = ps->psub->done(ps->psub);
    free(ps->psub);
    ps->psub = NULL;
    return status;
}

static SANE_Status TxSource_init (TxSource *pself,
                                  SnapScan_Scanner *pss,
                                  SourceRemaining remaining,
                                  SourceBytesPerLine bytesPerLine,
                                  SourcePixelsPerLine pixelsPerLine,
                                  SourceGet get,
				  SourceDone done,
				  Source *psub)
{
    SANE_Status status = Source_init((Source *) pself,
	                             pss,
                                     remaining,
		                     bytesPerLine,
                                     pixelsPerLine,
                                     get,
		                     done);
    if (status == SANE_STATUS_GOOD)
        pself->psub = psub;
    return status;
}

/* The expander makes three-channel, one-bit, raw scanner data into
   8-bit data. It is used to support the bilevel colour scanning mode */

typedef struct
{
    TX_SOURCE_GUTS;
    SANE_Byte *ch_buf;		/* channel buffer */
    SANE_Int   ch_size;		/* channel buffer size = #bytes in a channel */
    SANE_Int   ch_ndata;	/* actual #bytes in channel buffer */
    SANE_Int   ch_pos;		/* position in buffer */
    SANE_Int   bit;		/* current bit */
    SANE_Int   last_bit;	/* current last bit (counting down) */
    SANE_Int   last_last_bit;	/* last bit in the last byte of the channel */
} Expander;

static SANE_Int Expander_remaining (Source *pself)
{
    Expander *ps = (Expander *) pself;
    SANE_Int sub_remaining = TxSource_remaining(pself);
    SANE_Int sub_bits_per_channel = TxSource_pixelsPerLine(pself);
    SANE_Int whole_channels = sub_remaining/ps->ch_size;
    SANE_Int result = whole_channels*sub_bits_per_channel;

    if (ps->ch_pos < ps->ch_size)
    {
        SANE_Int bits_covered = MAX((ps->ch_pos - 1)*8, 0) + 7 - ps->bit;
        result += sub_bits_per_channel - bits_covered;
    }

    return result;
}

static SANE_Int Expander_bytesPerLine (Source *pself)
{
    return TxSource_pixelsPerLine(pself)*3;
}

static SANE_Status Expander_get (Source *pself, SANE_Byte *pbuf, SANE_Int *plen)
{
    Expander *ps = (Expander *) pself;
    SANE_Status status = SANE_STATUS_GOOD;
    SANE_Int remaining = *plen;

    while (remaining > 0
           &&
           pself->remaining(pself) > 0)
    {
        if (ps->ch_pos == ps->ch_ndata)
        {
            /* we need more data; try to get the remainder of the current
               channel, or else the next channel */
            SANE_Int ndata = ps->ch_size - ps->ch_ndata;
            if (ndata == 0)
            {
                ps->ch_ndata = 0;
                ps->ch_pos = 0;
                ndata = ps->ch_size;
            }
            status = TxSource_get(pself, ps->ch_buf + ps->ch_pos, &ndata);
            if (status != SANE_STATUS_GOOD)
                break;
            if (ndata == 0)
                break;
            ps->ch_ndata += ndata;
            if (ps->ch_pos == (ps->ch_size - 1))
                ps->last_bit = ps->last_last_bit;
            else
                ps->last_bit = 0;
            ps->bit = 7;
        }
        *pbuf = ((ps->ch_buf[ps->ch_pos] >> ps->bit) & 0x01) ? 0xFF : 0x00;
        pbuf++;
        remaining--;

        if (ps->bit == ps->last_bit)
        {
            ps->bit = 7;
            ps->ch_pos++;
            if (ps->ch_pos == (ps->ch_size - 1))
                ps->last_bit = ps->last_last_bit;
            else
                ps->last_bit = 0;
        }
        else
	{
            ps->bit--;
	}
    }

    *plen -= remaining;
    return status;
}

static SANE_Status Expander_done (Source *pself)
{
    Expander *ps = (Expander *) pself;
    SANE_Status status = TxSource_done(pself);
    free(ps->ch_buf);
    ps->ch_buf = NULL;
    ps->ch_size = 0;
    ps->ch_pos = 0;
    return status;
}

static SANE_Status Expander_init (Expander *pself,
				  SnapScan_Scanner *pss,
				  Source *psub)
{
    SANE_Status status = TxSource_init((TxSource *) pself,
                                       pss,
                                       Expander_remaining,
                                       Expander_bytesPerLine,
                                       TxSource_pixelsPerLine,
                                       Expander_get,
                                       Expander_done,
                                       psub);
    if (status == SANE_STATUS_GOOD)
    {
        pself->ch_size = TxSource_bytesPerLine((Source *) pself)/3;
        pself->ch_buf = (SANE_Byte *) malloc(pself->ch_size);
        if (pself->ch_buf == NULL)
        {
            DBG (DL_MAJOR_ERROR,
	         "%s: couldn't allocate channel buffer.\n",
                 __FUNCTION__);
            status = SANE_STATUS_NO_MEM;
        }
        else
        {
            pself->ch_ndata = 0;
            pself->ch_pos = 0;
            pself->last_last_bit = pself->pixelsPerLine((Source *) pself)%8;
            if (pself->last_last_bit == 0)
                pself->last_last_bit = 7;
            pself->last_last_bit = 7 - pself->last_last_bit;
            pself->bit = 7;
            if (pself->ch_size > 1)
                pself->last_bit = 0;
            else
                pself->last_bit = pself->last_last_bit;
        }
    }
    return status;
}

static SANE_Status create_Expander (SnapScan_Scanner *pss,
                                    Source *psub,
				    Source **pps)
{
    SANE_Status status = SANE_STATUS_GOOD;
    *pps = (Source *) malloc(sizeof(Expander));
    if (*pps == NULL)
    {
        DBG (DL_MAJOR_ERROR,
	     "%s: failed to allocate Expander.\n",
             __FUNCTION__);
        status = SANE_STATUS_NO_MEM;
    }
    else
    {
        status = Expander_init ((Expander *) *pps, pss, psub);
    }
    return status;
}

/* the RGB router assumes 8-bit RGB data arranged in contiguous
   channels, possibly with R-G and R-B offsets, and rearranges the
   data into SANE RGB frame format */

typedef struct
{
    TX_SOURCE_GUTS;
    SANE_Byte *cbuf;		/* circular line buffer */
    SANE_Byte *xbuf;		/* single line buffer */
    SANE_Int pos;		/* current position in xbuf */
    SANE_Int cb_size;		/* size of the circular buffer */
    SANE_Int cb_line_size;	/* size of a line in the circular buffer */
    SANE_Int cb_start;		/* start of valid data in the circular buffer */
    SANE_Int ch_offset[3];	/* offset in cbuf */
} RGBRouter;

static SANE_Int RGBRouter_remaining (Source *pself)
{
    RGBRouter *ps = (RGBRouter *) pself;
    if (ps->cb_start < 0)
    	return (TxSource_remaining(pself) - ps->cb_size + ps->cb_line_size);
    return (TxSource_remaining(pself) + ps->cb_line_size - ps->pos);
}

static SANE_Status RGBRouter_get (Source *pself,
                                  SANE_Byte *pbuf,
                                  SANE_Int *plen)
{
    RGBRouter *ps = (RGBRouter *) pself;
    SANE_Status status = SANE_STATUS_GOOD;
    SANE_Int remaining = *plen;
    SANE_Byte *s;
    SANE_Int i;
    SANE_Int r;
    SANE_Int g;
    SANE_Int b;

    while (remaining > 0  &&  pself->remaining(pself) > 0)
    {
        if (ps->pos >= ps->cb_line_size)
        {
            /* Try to get more data */
            SANE_Int ndata = (ps->cb_start < 0)  ?  ps->cb_size  :  ps->cb_line_size;
	    SANE_Int start = (ps->cb_start < 0)  ?  0  :  ps->cb_start;
            SANE_Int ndata2;
	    SANE_Int ndata3;

	    ndata2 = ndata;
	    ndata3 = 0;
	    do
	    {
            	status = TxSource_get (pself, ps->cbuf + start + ndata3, &ndata2);
            	if (status != SANE_STATUS_GOOD  ||  ndata2 == 0)
		{
		    *plen -= remaining;
		    return status;
		}
		ndata3 += ndata2;
		ndata2 = ndata - ndata3;
	    }
	    while (ndata3 < ndata);
            ps->cb_start = (start + ndata3)%ps->cb_size;
	    s = ps->xbuf;
	    r = (ps->cb_start + ps->ch_offset[0])%ps->cb_size;
	    g = (ps->cb_start + ps->ch_offset[1])%ps->cb_size;
	    b = (ps->cb_start + ps->ch_offset[2])%ps->cb_size;
            for (i = 0;  i < ps->cb_line_size/3;  i++)
	    {
	        *s++ = ps->cbuf[r++];
	        *s++ = ps->cbuf[g++];
	        *s++ = ps->cbuf[b++];
	    }
	    ps->pos = 0;
        }
	/* Repack the whole scan line now */
	while (remaining > 0  &&  ps->pos < ps->cb_line_size)
	{
            *pbuf++ = ps->xbuf[ps->pos++];
            remaining--;
	}
    }

    *plen -= remaining;
    return status;
}

static SANE_Status RGBRouter_done (Source *pself)
{
    RGBRouter *ps = (RGBRouter *) pself;
    SANE_Status status = TxSource_done(pself);

    free(ps->cbuf);
    free(ps->xbuf);
    ps->cbuf = NULL;
    ps->cb_start = -1;
    ps->pos = 0;
    return status;
}

static SANE_Status RGBRouter_init (RGBRouter *pself,
                                   SnapScan_Scanner *pss,
				   Source *psub)
{
    SANE_Status status = TxSource_init((TxSource *) pself,
                                       pss,
                                       RGBRouter_remaining,
                                       TxSource_bytesPerLine,
		                       TxSource_pixelsPerLine,
                                       RGBRouter_get,
		                       RGBRouter_done,
		                       psub);
    if (status == SANE_STATUS_GOOD)
    {
	SANE_Int lines_in_buffer = 1;
        SANE_Int ch;

	/* Size the buffer to accomodate the necessary number of scan lines
	   to cater for the offset between R, G and B */
	lines_in_buffer = 0;
	for (ch = 0;  ch < 3;  ch++)
	{
	    if (pss->chroma_offset[ch] > lines_in_buffer)
	        lines_in_buffer = pss->chroma_offset[ch];
	}
	lines_in_buffer++;

        pself->cb_line_size = pself->bytesPerLine((Source *) pself);
        pself->cb_size = pself->cb_line_size*lines_in_buffer;
        pself->pos = pself->cb_line_size;
        pself->cbuf = (SANE_Byte *) malloc(pself->cb_size);
        pself->xbuf = (SANE_Byte *) malloc(pself->cb_line_size);
        if (pself->cbuf == NULL  ||  pself->xbuf == NULL)
        {
            DBG (DL_MAJOR_ERROR,
                 "%s: failed to allocate circular buffer.\n",
                 __FUNCTION__);
            status = SANE_STATUS_NO_MEM;
        }
        else
        {
            SANE_Int ch;

            pself->cb_start = -1;
            for (ch = 0;  ch < 3;  ch++)
            {
		pself->ch_offset[ch] = pss->chroma_offset[ch]*pself->bytesPerLine((Source *) pself)
		                     + ch*(pself->bytesPerLine((Source *) pself)/3);
            }
        }
    }
    return status;
}

static SANE_Status create_RGBRouter (SnapScan_Scanner *pss,
                                     Source *psub,
				     Source **pps)
{
    SANE_Status status = SANE_STATUS_GOOD;
    *pps = (Source *) malloc(sizeof(RGBRouter));
    if (*pps == NULL)
    {
        DBG (DL_MAJOR_ERROR,
	     "%s: failed to allocate RGBRouter.\n",
             __FUNCTION__);
        status = SANE_STATUS_NO_MEM;
    }
    else
    {
        status = RGBRouter_init ((RGBRouter *) *pps, pss, psub);
    }
    return status;
}

/* An Inverter is used to invert the bits in a lineart image */

typedef struct
{
    TX_SOURCE_GUTS;
} Inverter;

static SANE_Status Inverter_get (Source *pself, SANE_Byte *pbuf, SANE_Int *plen)
{
    SANE_Status status = TxSource_get (pself, pbuf, plen);
    if (status == SANE_STATUS_GOOD)
    {
        int i;

        for (i = 0;  i < *plen;  i++)
            pbuf[i] ^= 0xFF;
    }
    return status;
}

static SANE_Status Inverter_init (Inverter *pself,
                                  SnapScan_Scanner *pss,
				  Source *psub)
{
    return  TxSource_init ((TxSource *) pself,
                           pss,
                           TxSource_remaining,
                           TxSource_bytesPerLine,
			   TxSource_pixelsPerLine,
                           Inverter_get,
			   TxSource_done,
			   psub);
}

static SANE_Status create_Inverter (SnapScan_Scanner *pss,
                                    Source *psub,
				    Source **pps)
{
    SANE_Status status = SANE_STATUS_GOOD;
    *pps = (Source *) malloc(sizeof(Inverter));
    if (*pps == NULL)
    {
        DBG (DL_MAJOR_ERROR,
             "%s: failed to allocate Inverter.\n",
             __FUNCTION__);
        status = SANE_STATUS_NO_MEM;
    }
    else
    {
        status = Inverter_init ((Inverter *) *pps, pss, psub);
    }
    return status;
}

/* Source chain creation */

static SANE_Status create_source_chain (SnapScan_Scanner *pss,
                                        BaseSourceType bst,
					Source **pps)
{
    SANE_Status status = create_base_source (pss, bst, pps);
    if (status == SANE_STATUS_GOOD)
    {
        SnapScan_Mode mode = actual_mode(pss);
        switch (mode)
        {
        case MD_COLOUR:
            status = create_RGBRouter (pss, *pps, pps);
            break;
        case MD_BILEVELCOLOUR:
            status = create_Expander (pss, *pps, pps);
            if (status == SANE_STATUS_GOOD)
                status = create_RGBRouter (pss, *pps, pps);
            break;
        case MD_GREYSCALE:
            break;
        case MD_LINEART:
            /* The SnapScan creates a negative image by
               default... so for the user interface to make sense,
               the internal meaning of "negative" is reversed */
            if (pss->negative == SANE_FALSE)
                status = create_Inverter (pss, *pps, pps);
            break;
        default:
            DBG (DL_MAJOR_ERROR,
	         "%s: bad mode value %d (internal error)\n",
                 __FUNCTION__,
                 mode);
            status = SANE_STATUS_INVAL;
            break;
        }
    }
    return status;
}

/*
 * $Log$
 * Revision 1.3  2000/08/12 15:09:35  pere
 * Merge devel (v1.0.3) into head branch.
 *
 * Revision 1.1.2.2  2000/07/13 04:47:45  pere
 * New snapscan backend version dated 20000514 from Steve Underwood.
 *
 * Revision 1.2.1  2000/05/14 13:30:20  coppice
 * Added history log to pre-existing code.Some reformatting.
 * R, G and B images now merge correctly. There are still some outstanding
 * issues in this area, but its a lot more usable than before.
 * */
