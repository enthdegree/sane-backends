/* $Id$
   SnapScan backend data sources (implementation) */

#ifdef TEMPORARY
  SANE_Status status = SANE_STATUS_GOOD;
  FDSource *ps = (FDSource*)pself;
  SANE_Int remaining = *plen;

  while (remaining > 0 &&
	 pself->remaining(pself) > 0 &&
	 status == SANE_STATUS_GOOD)
    {
    }

  *plen -= remaining;
  return status;
#endif

static
SANE_Status Source_init (Source *pself, SnapScan_Scanner *pss,
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

static
SANE_Int Source_bytesPerLine (Source *pself)
{
  return pself->pss->bytes_per_line;
}

static
SANE_Int Source_pixelsPerLine (Source *pself)
{
  return pself->pss->pixels_per_line;
}


/**********************************************************************/
/* the base sources */

typedef enum {SCSI_SRC, FD_SRC, BUF_SRC} BaseSourceType;


#define SCSISOURCE_BAD_TIME -1

typedef struct {
  SOURCE_GUTS;
  SANE_Int scsi_buf_pos;	/* current position in scsi buffer */
  SANE_Int scsi_buf_max;	/* data limit */
  SANE_Int absolute_max;	/* largest possible data read */
  struct timeval time;		/* time of last scsi read (usec) */
} SCSISource;

static
SANE_Int SCSISource_remaining (Source *pself)
{
  SCSISource *ps = (SCSISource*)pself;
  return ps->pss->bytes_remaining + (ps->scsi_buf_max - ps->scsi_buf_pos);
}

static
SANE_Status SCSISource_get (Source *pself, SANE_Byte *pbuf, SANE_Int *plen)
{
  SCSISource *ps = (SCSISource *)pself;
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Int remaining = *plen;
  while (remaining > 0 &&
	 pself->remaining(pself) > 0 &&
	 status == SANE_STATUS_GOOD)
    {
      SANE_Int ndata = ps->scsi_buf_max - ps->scsi_buf_pos;
      if (ndata == 0)
	/* read more data */
	{
	  struct timeval oldtime = ps->time;
	  if (ps->time.tv_sec != SCSISOURCE_BAD_TIME &&
	      gettimeofday(&(ps->time), NULL) == 0)
	    /* estimate number of lines to read from the elapsed time
	       since the last read and the scanner's reported speed */
	    {
	      double msecs =
		(ps->time.tv_sec - oldtime.tv_sec) * 1000.0 +
		(ps->time.tv_usec - oldtime.tv_usec) / 1000.0;
	      ps->pss->expected_read_bytes =
		((int)(msecs/ps->pss->ms_per_line)) * ps->pss->bytes_per_line;
	    }
	  else
	    /* use the "lines_per_read" values */
	    {
	      SANE_Int lines;
	      if (is_colour_mode(actual_mode(ps->pss)) == SANE_TRUE)
		lines = ps->pss->rgb_lpr;
	      else
		lines = ps->pss->gs_lpr;
	      ps->pss->expected_read_bytes = lines * ps->pss->bytes_per_line;
	    }
	  ps->pss->expected_read_bytes =
	    MIN(ps->pss->expected_read_bytes, ps->pss->bytes_remaining);
	  ps->pss->expected_read_bytes =
	    MIN(ps->pss->expected_read_bytes, ps->absolute_max);
	  ps->scsi_buf_pos = 0;
	  ps->scsi_buf_max = 0;
	  status = scsi_read (ps->pss, READ_IMAGE);
	  if (status == SANE_STATUS_GOOD)
	    {
	      ps->scsi_buf_max = ps->pss->read_bytes;
	      ndata = ps->pss->read_bytes;
	      ps->pss->bytes_remaining -= ps->pss->read_bytes;
	    }
	  else
	    break;
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

static
SANE_Status SCSISource_done (Source *pself)
{
  return SANE_STATUS_GOOD;
}


static
SANE_Status SCSISource_init (SCSISource *pself, SnapScan_Scanner *pss)
{
  SANE_Status status =
    Source_init ((Source*)pself, pss,
		 SCSISource_remaining,
		 Source_bytesPerLine, Source_pixelsPerLine,
		 SCSISource_get, SCSISource_done);
  if (status == SANE_STATUS_GOOD)
    {
      pself->scsi_buf_max = 0;
      pself->scsi_buf_pos = 0;
      pself->absolute_max =
	(SCANNER_BUF_SZ / pss->bytes_per_line) * pss->bytes_per_line;
      if(gettimeofday(&(pself->time), NULL) != 0)
	{
	  DBG (DL_MAJOR_ERROR, "%s: error in gettimeofday(): %s\n",
	       __FUNCTION__, sys_errlist[errno]);
	  pself->time.tv_sec = SCSISOURCE_BAD_TIME;
	  pself->time.tv_usec = SCSISOURCE_BAD_TIME;
	}
    }
  return status;
}


/* File sources */

typedef struct {
  SOURCE_GUTS;
  int fd;
} FDSource;

static SANE_Int
FDSource_remaining (Source *pself)
{
  return pself->pss->bytes_remaining;
}

#ifdef TEMPORARY
  SANE_Status status = SANE_STATUS_GOOD;
  FDSource *ps = (FDSource*)pself;
  SANE_Int remaining = *plen;

  while (remaining > 0 &&
	 pself->remaining(pself) > 0 &&
	 status == SANE_STATUS_GOOD)
    {
    }

  *plen -= remaining;
  return status;
#endif

static SANE_Status
FDSource_get (Source *pself, SANE_Byte *pbuf, SANE_Int *plen)
{
  SANE_Status status = SANE_STATUS_GOOD;
  FDSource *ps = (FDSource*)pself;
  SANE_Int remaining = *plen;

  while (remaining > 0 &&
	 pself->remaining(pself) > 0 &&
	 status == SANE_STATUS_GOOD)
    {
      SANE_Int bytes_read = read (ps->fd, pbuf, remaining);
      if (bytes_read == -1)
	{
	  if (errno == EAGAIN)
	    /* no data currently available */
	    break;
	  else
	    /* it's an IO error */
	    {
	      DBG (DL_MAJOR_ERROR, "%s: read failed: %s\n",
		   __FUNCTION__, sys_errlist[errno]);
	      status = SANE_STATUS_IO_ERROR;
	    }
	}
      else if (bytes_read == 0)
	break;
      else
	{
	  ps->pss->bytes_remaining -= bytes_read;
	  remaining -= bytes_read;
	  pbuf += bytes_read;
	}
    }

  *plen -= remaining;
  return status;
}

static SANE_Status
FDSource_done (Source *pself)
{
  close(((FDSource*)pself)->fd);
  return SANE_STATUS_GOOD;
}

static SANE_Status
FDSource_init (FDSource *pself, SnapScan_Scanner *pss, int fd)
{
  SANE_Status status =
    Source_init ((Source*)pself, pss,
		 FDSource_remaining,
		 Source_bytesPerLine, Source_pixelsPerLine,
		 FDSource_get, FDSource_done);
  if (status == SANE_STATUS_GOOD)
    {
      pself->fd = fd;
    }
  return status;
}


/* buffer sources simply read from a pre-filled buffer; we have these
   so that we can include source chain processing overhead in the
   measure_transfer_rate() function */

typedef struct {
  SOURCE_GUTS;
  SANE_Byte *buf;
  SANE_Int buf_size;
  SANE_Int buf_pos;
} BufSource;

static SANE_Int
BufSource_remaining (Source *pself)
{
  BufSource *ps = (BufSource*)pself;
  return  ps->buf_size - ps->buf_pos;
}

static SANE_Status
BufSource_get (Source *pself, SANE_Byte *pbuf, SANE_Int *plen)
{
  BufSource *ps = (BufSource*)pself;
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Int to_move = MIN(*plen, pself->remaining(pself));
  if (to_move == 0)
    status = SANE_STATUS_EOF;
  else
    {
      memcpy (pbuf, ps->buf + ps->buf_pos, to_move);
      ps->buf_pos += to_move;
      *plen = to_move;
    }
  return status;
}

static SANE_Status
BufSource_done (Source *pself)
{
  return SANE_STATUS_GOOD;
}

static SANE_Status
BufSource_init (BufSource *pself, SnapScan_Scanner *pss,
		SANE_Byte *buf, SANE_Int buf_size) 
{
  SANE_Status status =
    Source_init ((Source*)pself, pss,
		 BufSource_remaining,
		 Source_bytesPerLine, Source_pixelsPerLine,
		 BufSource_get, BufSource_done);
  if (status == SANE_STATUS_GOOD)
    {
      pself->buf = buf;
      pself->buf_size = buf_size;
      pself->buf_pos = 0;
    }
  return status;
}


/* base source creation */

static
SANE_Status create_base_source (SnapScan_Scanner *pss, BaseSourceType st,
				Source **pps)
{
  SANE_Status status = SANE_STATUS_GOOD;
  *pps = NULL;
  switch (st)
    {
    case SCSI_SRC:
      *pps = (Source*)malloc(sizeof(SCSISource));
      if (*pps == NULL)
	{
	  DBG (DL_MAJOR_ERROR, "failed to allocate SCSISource");
	  status = SANE_STATUS_NO_MEM;
	}
      else
	status = SCSISource_init ((SCSISource*)*pps, pss);
      break;
    case FD_SRC:
      *pps = (Source*)malloc(sizeof(FDSource));
      if (*pps == NULL)
	{
	  DBG (DL_MAJOR_ERROR, "failed to allocate FDSource");
	  status = SANE_STATUS_NO_MEM;
	}
      else
	status = FDSource_init ((FDSource*)*pps, pss, pss->rpipe[0]);
      break;
    case BUF_SRC:
      *pps = (Source*)malloc(sizeof(BufSource));
      if (*pps == NULL)
	{
	  DBG (DL_MAJOR_ERROR, "failed to allocate BufSource");
	  status = SANE_STATUS_NO_MEM;
	}
      else
	status = BufSource_init ((BufSource*)*pps, pss,
				 pss->buf, pss->read_bytes);
      break;
    default:
      DBG (DL_MAJOR_ERROR, "illegal base source type %d", st);
      break;
    }
  return status;
}


/**********************************************************************/
/* the transformer sources */

#define TX_SOURCE_GUTS \
  SOURCE_GUTS;\
  Source *psub			/* sub-source */

typedef struct {
  TX_SOURCE_GUTS;
} TxSource;

static SANE_Int
TxSource_remaining (Source *pself)
{
  TxSource *ps = (TxSource*)pself;
  return ps->psub->remaining(ps->psub);
}

static SANE_Int
TxSource_bytesPerLine (Source *pself)
{
  TxSource *ps = (TxSource*)pself;
  return ps->psub->bytesPerLine(ps->psub);
}

static SANE_Int
TxSource_pixelsPerLine (Source *pself)
{
  TxSource *ps = (TxSource*)pself;
  return ps->psub->pixelsPerLine(ps->psub);
}

static SANE_Status
TxSource_get (Source *pself, SANE_Byte *pbuf, SANE_Int *plen)
{
  TxSource *ps = (TxSource*)pself;
  return ps->psub->get(ps->psub, pbuf, plen);
}

static SANE_Status
TxSource_done (Source *pself)
{
  TxSource *ps = (TxSource*)pself;
  SANE_Status status = ps->psub->done(ps->psub);
  free (ps->psub);
  return status;
}

static SANE_Status
TxSource_init (TxSource *pself, SnapScan_Scanner *pss,
	       SourceRemaining remaining,
	       SourceBytesPerLine bytesPerLine,
	       SourcePixelsPerLine pixelsPerLine,
	       SourceGet get, SourceDone done, Source *psub)
{
  SANE_Status status =
    Source_init ((Source*)pself, pss,
		 remaining, bytesPerLine, pixelsPerLine,
		 get, done);
  if (status == SANE_STATUS_GOOD)
    pself->psub = psub;
  return status;
}


/* the expander makes three-channel, one-bit, raw scanner data into
   8-bit data; it is used to support the bilevel colour scanning mode */

typedef struct {
  TX_SOURCE_GUTS;
  SANE_Byte *ch_buf;		/* channel buffer */
  SANE_Int   ch_size;		/* channel buffer size = #bytes in a channel */
  SANE_Int   ch_ndata;		/* actual #bytes in channel buffer */
  SANE_Int   ch_pos;		/* position in buffer */
  SANE_Int   bit;		/* current bit */
  SANE_Int   last_bit;		/* current last bit (counting down) */
  SANE_Int   last_last_bit;	/* last bit in the last byte of the channel */
} Expander;

static SANE_Int
Expander_remaining (Source *pself)
{
  Expander *ps = (Expander*)pself;
  SANE_Int sub_remaining = TxSource_remaining(pself);
  SANE_Int sub_bits_per_channel = TxSource_pixelsPerLine(pself);
  SANE_Int whole_channels = sub_remaining / ps->ch_size;
  SANE_Int result = whole_channels * sub_bits_per_channel;

  if (ps->ch_pos < ps->ch_size)
    {
      SANE_Int bits_covered =
	MAX((ps->ch_pos - 1) * 8, 0) + 7 - ps->bit;
      result += sub_bits_per_channel - bits_covered;
    }

  return result;
}

static SANE_Int
Expander_bytesPerLine (Source *pself)
{
  return TxSource_pixelsPerLine(pself) * 3;
}

static SANE_Status
Expander_get (Source *pself, SANE_Byte *pbuf, SANE_Int *plen)
{
  Expander *ps = (Expander*)pself;
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Int remaining = *plen;

  while (remaining > 0 &&
	 pself->remaining(pself) > 0)
    {
      if (ps->ch_pos == ps->ch_ndata)
	/* we need more data; try to get the remainder of the current
	   channel, or else the next channel */
	{
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
      *pbuf = ((ps->ch_buf[ps->ch_pos] >> ps->bit) & 0x01) ? 0xff : 0x00;
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
	ps->bit--;
    }

  *plen -= remaining;
  return status;
}

static SANE_Status
Expander_done (Source *pself)
{
  Expander *ps = (Expander*)pself;
  SANE_Status status = TxSource_done(pself);
  free(ps->ch_buf);
  ps->ch_buf = NULL;
  ps->ch_size = 0;
  ps->ch_pos = 0;
  return status;
}

static SANE_Status
Expander_init (Expander *pself, SnapScan_Scanner *pss, Source *psub)
{
  SANE_Status status =
    TxSource_init ((TxSource*)pself, pss,
		   Expander_remaining, Expander_bytesPerLine,
		   TxSource_pixelsPerLine,
		   Expander_get, Expander_done, psub);
  if (status == SANE_STATUS_GOOD)
    {
      pself->ch_size = TxSource_bytesPerLine((Source*)pself) / 3;
      pself->ch_buf = (SANE_Byte*)malloc(pself->ch_size);
      if (pself->ch_buf == NULL)
	{
	  DBG (DL_MAJOR_ERROR, "%s: couldn't allocate channel buffer.\n",
	       __FUNCTION__);
	  status = SANE_STATUS_NO_MEM;
	}
      else
	{
	  pself->ch_ndata = 0;
	  pself->ch_pos = 0;
	  pself->last_last_bit = pself->pixelsPerLine((Source*)pself) % 8;
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

static SANE_Status
create_Expander (SnapScan_Scanner *pss, Source *psub, Source **pps)
{
  SANE_Status status = SANE_STATUS_GOOD;
  *pps = (Source*)malloc(sizeof(Expander));
  if (*pps == NULL)
    {
      DBG (DL_MAJOR_ERROR, "%s: failed to allocate Expander.\n",
	   __FUNCTION__);
      status = SANE_STATUS_NO_MEM;
    }
  else
    status = Expander_init ((Expander*)*pps, pss, psub);
  return status;
}


/* the RGB router assumes 8-bit RGB data arranged in contiguous
   channels, possibly with R-G and R-B offsets, and rearranges the
   data into SANE RGB frame format

   NOTE: at first, assume NO R-G and R-B offsets; thus, the buffer
   is not circular, and cb_bot is fixed at 0 */

typedef struct {
  SANE_Int offset;		/* offset in the master buffer */
  SANE_Int size;		/* size of a channel (bytes) */
  SANE_Int ndata;		/* total bytes present for this channel */
  SANE_Int pos;			/* current position */
} Channel;

typedef struct {
  TX_SOURCE_GUTS;
  SANE_Byte *cbuf;		/* circular line buffer */
  SANE_Int cb_size;		/* size of the circular buffer */
  SANE_Int cb_bot;		/* bottom of the circular buffer */
  SANE_Int cb_top;		/* top of the circular buffer */
  Channel channels[3];		/* the channel book-keeping stuff */
  SANE_Int ch;			/* current channel */
} RGBRouter;

static inline SANE_Int
Channel_remaining (Channel *pc)
{
  return pc->ndata - pc->pos;
}
 
static SANE_Int
RGBRouter_remaining (Source *pself)
{
  RGBRouter *ps = (RGBRouter*)pself;
  return (TxSource_remaining(pself) +
	  Channel_remaining(&(ps->channels[0])) +
	  Channel_remaining(&(ps->channels[1])) +
	  Channel_remaining(&(ps->channels[2])));
}


static inline void
Channel_update_ndata (Channel *pc, SANE_Int cb_top)
{
  SANE_Int t = cb_top - pc->offset + 1;
  if (t <= 0)
    pc->ndata = 0;
  else
    pc->ndata = MIN(t, pc->size);
}

static SANE_Status
RGBRouter_get (Source *pself, SANE_Byte *pbuf, SANE_Int *plen)
{
  RGBRouter *ps = (RGBRouter*)pself;
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Int remaining = *plen;

  while (remaining > 0 &&
	 pself->remaining(pself) > 0)
    {
      Channel *pc = &(ps->channels[ps->ch]);

      if (pc->pos == pc->ndata)
	/* try to get more data */
	{
	  SANE_Int ndata = ps->cb_size - (ps->cb_top + 1);
	  if (ndata == 0)
	    {
	      ndata = ps->cb_size;
	      ps->cb_top = -1;
	      ps->channels[0].pos = 0;
	      ps->channels[1].pos = 0;
	      ps->channels[2].pos = 0;
	    }
	  status = TxSource_get (pself, ps->cbuf + ps->cb_top + 1, &ndata);
	  if (status != SANE_STATUS_GOOD)
	    break;
	  if (ndata == 0)
	    break;
	  ps->cb_top += ndata;
	  Channel_update_ndata(&(ps->channels[0]), ps->cb_top);
	  Channel_update_ndata(&(ps->channels[1]), ps->cb_top);
	  Channel_update_ndata(&(ps->channels[2]), ps->cb_top);
	  if (pc->pos == pc->ndata)
	    /* not enough data to continue with this channel */
	    break;
	}
      *pbuf = ps->cbuf[pc->offset + pc->pos];
      pbuf++;
      pc->pos++;
      remaining--;
      ps->ch = (ps->ch + 1) % 3;
    }

  *plen -= remaining;
  return status;
}

static SANE_Status
RGBRouter_done (Source *pself)
{
  RGBRouter *ps = (RGBRouter*)pself;
  SANE_Status status = TxSource_done (pself);
  free(ps->cbuf);
  ps->cbuf = NULL;
  ps->cb_bot = 0;
  ps->cb_top = -1;
  return status;
}


static SANE_Status
RGBRouter_init (RGBRouter *pself, SnapScan_Scanner *pss, Source *psub)
{
  SANE_Status status =
    TxSource_init ((TxSource*)pself, pss,
		   RGBRouter_remaining,
		   TxSource_bytesPerLine, TxSource_pixelsPerLine,
		   RGBRouter_get, RGBRouter_done, psub);
  if (status == SANE_STATUS_GOOD)
    {
      pself->cb_size = pself->bytesPerLine((Source*)pself);
      pself->cbuf = (SANE_Byte*)malloc(pself->cb_size);
      if (pself->cbuf == NULL)
	{
	  DBG (DL_MAJOR_ERROR, "%s: failed to allocate circular buffer.\n",
	       __FUNCTION__);
	  status = SANE_STATUS_NO_MEM;
	}
      else
	{
	  SANE_Int ch_size = pself->cb_size / 3;
	  SANE_Int ch;
	  pself->cb_bot = 0;
	  pself->cb_top = -1;
	  for (ch = 0; ch < 3; ch++)
	    {
	      Channel *pc = &(pself->channels[ch]);
	      pc->pos = 0;
	      pc->ndata = 0;
	      pc->size = ch_size;
	    }
	  pself->channels[0].offset = 0;
	  pself->channels[1].offset = ch_size;
	  pself->channels[2].offset = 2 * ch_size;
	  pself->ch = 0;
	}
    }
  return status;
}

static SANE_Status
create_RGBRouter (SnapScan_Scanner *pss, Source *psub, Source **pps)
{
  SANE_Status status = SANE_STATUS_GOOD;
  *pps = (Source*)malloc(sizeof(RGBRouter));
  if (*pps == NULL)
    {
      DBG (DL_MAJOR_ERROR, "%s: failed to allocate RGBRouter.\n",
	   __FUNCTION__);
      status = SANE_STATUS_NO_MEM;
    }
  else
    status = RGBRouter_init ((RGBRouter*)*pps, pss, psub);
  return status;
}



/* an Inverter is used to invert the bits in a lineart image */

typedef struct {
  TX_SOURCE_GUTS;
} Inverter;

static SANE_Status
Inverter_get (Source *pself, SANE_Byte *pbuf, SANE_Int *plen)
{
  SANE_Status status = TxSource_get (pself, pbuf, plen);
  if (status == SANE_STATUS_GOOD)
    {
      int i;
      for (i = 0; i < *plen; i++)
	pbuf[i] ^= 0xff;
    }
  return status;
}

static SANE_Status
Inverter_init (Inverter *pself, SnapScan_Scanner *pss, Source *psub)
{
  return  TxSource_init ((TxSource*)pself, pss,
			 TxSource_remaining,
			 TxSource_bytesPerLine, TxSource_pixelsPerLine,
			 Inverter_get, TxSource_done, psub);
}


static SANE_Status
create_Inverter (SnapScan_Scanner *pss, Source *psub, Source **pps)
{
  SANE_Status status = SANE_STATUS_GOOD;
  *pps = (Source*)malloc(sizeof(Inverter));
  if (*pps == NULL)
    {
      DBG (DL_MAJOR_ERROR, "%s: failed to allocate Inverter.\n",
	   __FUNCTION__);
      status = SANE_STATUS_NO_MEM;
    }
  else
    status = Inverter_init ((Inverter*)*pps, pss, psub);
  return status;
}



/* source chain creation */

static
SANE_Status
create_source_chain (SnapScan_Scanner *pss, BaseSourceType bst, Source **pps)
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
	  /* the snapscan creates a negative image by
	     default... so for the user interface to make sense,
	     the internal meaning of "negative" is reversed */
	  if (pss->negative == SANE_FALSE)
	    status = create_Inverter (pss, *pps, pps);
	  break;
	default:
	  DBG (DL_MAJOR_ERROR, "%s: bad mode value %d (internal error)\n",
	       __FUNCTION__, mode);
	  status = SANE_STATUS_INVAL;
	  break;
	}
    }
  return status;
}

