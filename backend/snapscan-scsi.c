/* $Id$
   SnapScan backend scsi command functions */

/* scanner scsi commands */


/* a sensible sense handler, courtesy of Franck;
   the last argument is expected to be a pointer to the associated
   SnapScan_Scanner structure */
static SANE_Status
sense_handler (int scsi_fd, u_char * result, void *arg)
{
  static char me[] = "sense_handler";
  SnapScan_Scanner *pss = (SnapScan_Scanner *) arg;
  u_char sense, asc, ascq;
  char *sense_str = NULL, *as_str = NULL;
  SANE_Status status = SANE_STATUS_GOOD;

  DBG (DL_CALL_TRACE, "%s(%ld, %p, %p)\n", me, (long) scsi_fd,
       (void *) result, (void *) arg);

  sense = result[2] & 0x0f;
  asc = result[12];
  ascq = result[13];
  if (pss)
    {
      pss->asi1 = result[18];
      pss->asi2 = result[19];
    }

  if ((result[0] & 0x80) == 0)
    {
      DBG (DL_DATA_TRACE, "%s: sense key is invalid.\n", me);
      return SANE_STATUS_GOOD;	/* sense key invalid */
    }

  switch (sense)
    {
    case 0x00:			/* no sense */
      sense_str = "No sense.";
      break;
    case 0x02:			/* not ready */
      sense_str = "Not ready.";
      if (asc == 0x04 && ascq == 0x01)
	/* warming up; byte 18 contains remaining seconds */
	{
	  as_str = "Logical unit is in process of becoming ready.";
	  status = SANE_STATUS_DEVICE_BUSY;
	}
      break;
    case 0x04:			/* hardware error */
      sense_str = "Hardware error.";
      /* byte 18 and 19 detail the hardware problems */
      status = SANE_STATUS_IO_ERROR;
      break;
    case 0x05:			/* illegal request */
      sense_str = "Illegal request.";
      if (asc == 0x25 && ascq == 0x00)
	{
	  as_str = "Logical unit not supported.";
	}
      status = SANE_STATUS_IO_ERROR;
      break;
    case 0x09:			/* process error */
      sense_str = "Process error.";
      if (asc == 0x00 && ascq == 0x05)
	/* no documents in ADF */
	{
	  as_str = "End of data detected.";
	  status = SANE_STATUS_NO_DOCS;
	}
      else if (asc == 0x3b && ascq == 0x05)
	/* paper jam in ADF */
	{
	  as_str = "Paper jam.";
	  status = SANE_STATUS_JAMMED;
	}
      else if (asc == 0x3b && ascq == 0x09)
	/* scanning area exceeds end of paper in ADF */
	{
	  as_str = "Read past end of medium.";
	  status = SANE_STATUS_EOF;
	}
      break;
    default:
      DBG (DL_MINOR_ERROR, "%s: no handling for sense %x.\n", me, sense);
      break;
    }

  if (pss)
    {
      pss->sense_str = sense_str;
      pss->as_str = as_str;
    }
  return status;
}


static
SANE_Status
open_scanner (SnapScan_Scanner * pss)
{
  SANE_Status status;
  DBG (DL_CALL_TRACE, "open_scanner\n");
  if (!pss->opens)
    status = sanei_scsi_open (pss->devname, &(pss->fd),
			      sense_handler, (void *) pss);
  else				/* already open */
    status = SANE_STATUS_GOOD;
  if (status == SANE_STATUS_GOOD)
    pss->opens++;
  return status;
}

static
void
close_scanner (SnapScan_Scanner * pss)
{
  DBG (DL_CALL_TRACE, "close_scanner\n");
  if (pss->opens)
    {
      pss->opens--;
      if (!pss->opens)
	sanei_scsi_close (pss->fd);
    }
}


/* SCSI commands */
#define TEST_UNIT_READY        0x00
#define INQUIRY                0x12
#define SEND                   0x2a
#define SET_WINDOW             0x24
#define SCAN                   0x1b
#define READ                   0x28
#define REQUEST_SENSE          0x03
#define RESERVE_UNIT           0x16
#define RELEASE_UNIT           0x17
#define SEND_DIAGNOSTIC        0x1d
#define GET_DATA_BUFFER_STATUS 0x34


#define SCAN_LEN 6
#define READ_LEN 10

/*  buffer tools */

static void
zero_buf (u_char * buf, size_t len)
{
  size_t i;
  for (i = 0; i < len; i++)
    {
      buf[i] = 0x00;
    }
}


#define BYTE_SIZE 8

static u_short
u_char_to_u_short (u_char * pc)
{
  u_short r = 0;
  r |= pc[0];
  r = r << BYTE_SIZE;
  r |= pc[1];
  return r;
}

static void
u_short_to_u_charp (u_short x, u_char * pc)
{
  pc[0] = (0xff00 & x) >> BYTE_SIZE;
  pc[1] = (0x00ff & x);
}

static void
u_int_to_u_char3p (u_int x, u_char * pc)
{
  pc[0] = (0xff0000 & x) >> 2 * BYTE_SIZE;
  pc[1] = (0x00ff00 & x) >> BYTE_SIZE;
  pc[2] = (0x0000ff & x);
}

static void
u_int_to_u_char4p (u_int x, u_char * pc)
{
  pc[0] = (0xff000000 & x) >> 3 * BYTE_SIZE;
  pc[1] = (0x00ff0000 & x) >> 2 * BYTE_SIZE;
  pc[2] = (0x0000ff00 & x) >> BYTE_SIZE;
  pc[3] = (0x000000ff & x);
}

/* Convert 'STRING   ' to 'STRING' by adding 0 after last non-space */
static void
remove_trailing_space (char *s)
{
  int position;

  if (NULL == s)
    return;

  for (position = strlen (s); position > 0 && ' ' == s[position - 1];
       position--);
  s[position] = 0;
}

#define INQUIRY_LEN 6
#define INQUIRY_RET_LEN 120

#define INQUIRY_VENDOR  8	/* Offset in reply data to vendor name */
#define INQUIRY_PRODUCT 16	/* Offset in reply data to product id */
#define INQUIRY_REV 32		/* Offset in reply data to revision level */
#define INQUIRY_PRL2 36		/* Product Revision Level 2 (AGFA) */
#define INQUIRY_HCFG 37		/* Hardware Configuration (AGFA) */
#define INQUIRY_PIX_PER_LINE  42	/* Pixels per scan line (AGFA) */
#define INQUIRY_BYTE_PER_LINE 44	/* Bytes per scan line (AGFA) */
#define INQUIRY_NUM_LINES     46	/* number of scan lines (AGFA) */
#define INQUIRY_OPT_RES       48	/* optical resolution (AGFA) */
#define INQUIRY_SCAN_SPEED    51	/* scan speed (AGFA) */
#define INQUIRY_EXPTIME1      52	/* exposure time, first digit (AGFA) */
#define INQUIRY_EXPTIME2      53	/* exposure time, second digit (AGFA) */
#define INQUIRY_G2R_DIFF      54	/* green to red difference (AGFA) */
#define INQUIRY_B2R_DIFF      55	/* green to red difference (AGFA) */
#define INQUIRY_FIRMWARE      96	/* firmware date and time (AGFA) */


/* a mini-inquiry reads only the first 36 bytes of inquiry data, and
   returns the vendor(7 chars) and model(16 chars); vendor and model
   must point to character buffers of size at least 8 and 17
   respectively */

static
SANE_Status
mini_inquiry (int fd, char *vendor, char *model)
{
  static const char *me = "mini_inquiry";
  size_t read_bytes;
  char cmd[] =
  {INQUIRY, 0, 0, 0, 36, 0};
  char data[36];
  SANE_Status status;

  read_bytes = 36;

  DBG (DL_CALL_TRACE, "%s\n", me);
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), data, &read_bytes);
  CHECK_STATUS (status, me, "sanei_scsi_cmd");

  memcpy (vendor, data + 8, 7);
  vendor[7] = 0;
  memcpy (model, data + 16, 16);
  model[16] = 0;

  remove_trailing_space (vendor);
  remove_trailing_space (model);

  return SANE_STATUS_GOOD;
}


static
SANE_Status
inquiry (SnapScan_Scanner * pss)
{
  static const char *me = "inquiry";

  SANE_Status status;
  pss->read_bytes = INQUIRY_RET_LEN;

  zero_buf (pss->cmd, MAX_SCSI_CMD_LEN);
  pss->cmd[0] = INQUIRY;
  pss->cmd[4] = INQUIRY_RET_LEN;

  DBG (DL_CALL_TRACE, "%s\n", me);
  status = sanei_scsi_cmd (pss->fd, pss->cmd, INQUIRY_LEN,
			   pss->buf, &pss->read_bytes);
  CHECK_STATUS (status, me, "sanei_scsi_cmd");

  /* record current parameters */

  pss->actual_res =
    u_char_to_u_short (pss->buf + INQUIRY_OPT_RES);
  pss->pixels_per_line =
    u_char_to_u_short (pss->buf + INQUIRY_PIX_PER_LINE);
  pss->bytes_per_line =
    u_char_to_u_short (pss->buf + INQUIRY_BYTE_PER_LINE);
  pss->lines =
    u_char_to_u_short (pss->buf + INQUIRY_NUM_LINES);
  /* effective buffer size must be a whole number of scan lines */
  if (pss->lines)
    {
      pss->buf_sz =
	(SCANNER_BUF_SZ / pss->bytes_per_line) * pss->bytes_per_line;
    }
  else
    {
      pss->buf_sz = 0;
    }
  pss->bytes_remaining = pss->bytes_per_line * pss->lines;
  pss->expected_read_bytes = 0;
  pss->read_bytes = 0;
  pss->hconfig = pss->buf[INQUIRY_HCFG];

  {
    char exptime[4] =
    {' ', '.', ' ', 0};
    exptime[0] = (char) (pss->buf[INQUIRY_EXPTIME1] + '0');
    exptime[2] = (char) (pss->buf[INQUIRY_EXPTIME2] + '0');
    pss->ms_per_line = atof (exptime) * (float) pss->buf[INQUIRY_SCAN_SPEED];
  }

#ifdef OBSOLETE
  switch (pss->pdev->model)
    {
    case SNAPSCAN310:
    case SNAPSCAN600:
    case SNAPSCAN1236S:
    case VUEGO310S:		/* WG changed */
    case PRISA620S:		/* GP added */
      rgb_buf_set_diff (pss,
			pss->buf[INQUIRY_G2R_DIFF],
			pss->buf[INQUIRY_B2R_DIFF]);
      break;
    default:
      break;
    }
#else
  pss->g_offset = 0;
  pss->b_offset = 0;
  pss->r_offset = 0;
#endif

  DBG (DL_DATA_TRACE, "%s: pixels per scan line = %lu\n",
       me, (u_long) pss->pixels_per_line);
  DBG (DL_DATA_TRACE, "%s: bytes per scan line = %lu\n",
       me, (u_long) pss->bytes_per_line);
  DBG (DL_DATA_TRACE, "%s: number of scan lines = %lu\n",
       me, (u_long) pss->lines);
  DBG (DL_DATA_TRACE, "%s: effective buffer size = %lu bytes, %lu lines\n",
       me, (u_long) pss->buf_sz,
       (u_long) (pss->buf_sz ? pss->buf_sz / pss->lines : 0));
  DBG (DL_DATA_TRACE, "%s: expected total scan data: %lu bytes\n", me,
       (u_long) pss->bytes_remaining);

  return status;
}

static
SANE_Status
test_unit_ready (SnapScan_Scanner * pss)
{
  static const char *me = "test_unit_ready";
  char cmd[] =
  {TEST_UNIT_READY, 0, 0, 0, 0, 0};
  SANE_Status status;

  DBG (DL_CALL_TRACE, "%s\n", me);
  status = sanei_scsi_cmd (pss->fd, cmd, sizeof (cmd), NULL, NULL);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DL_MAJOR_ERROR, "%s: scsi command error: %s\n",
	   me, sane_strstatus (status));
    }
  return status;
}


static
void
reserve_unit (SnapScan_Scanner * pss)
{
  static const char *me = "reserve_unit";
  char cmd[] =
  {RESERVE_UNIT, 0, 0, 0, 0, 0};
  SANE_Status status;

  DBG (DL_CALL_TRACE, "%s\n", me);
  status = sanei_scsi_cmd (pss->fd, cmd, sizeof (cmd), NULL, NULL);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DL_MAJOR_ERROR, "%s: scsi command error: %s\n",
	   me, sane_strstatus (status));
    }
}

static
void
release_unit (SnapScan_Scanner * pss)
{
  static const char *me = "release_unit";
  char cmd[] =
  {RELEASE_UNIT, 0, 0, 0, 0, 0};
  SANE_Status status;

  DBG (DL_CALL_TRACE, "%s\n", me);
  status = sanei_scsi_cmd (pss->fd, cmd, sizeof (cmd), NULL, NULL);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DL_MAJOR_ERROR, "%s: scsi command error: %s\n",
	   me, sane_strstatus (status));
    }
}


#define SEND_LENGTH 10
#define DTC_HALFTONE 0x02
#define DTC_GAMMA 0x03
#define DTC_SPEED 0x81
#define DTCQ_HALFTONE_BW8 0x00
#define DTCQ_HALFTONE_COLOR8 0x01
#define DTCQ_HALFTONE_BW16 0x80
#define DTCQ_HALFTONE_COLOR16 0x81
#define DTCQ_GAMMA_GRAY8 0x00
#define DTCQ_GAMMA_RED8 0x01
#define DTCQ_GAMMA_GREEN8 0x02
#define DTCQ_GAMMA_BLUE8 0x03
#define DTCQ_GAMMA_GRAY10 0x80
#define DTCQ_GAMMA_RED10 0x81
#define DTCQ_GAMMA_GREEN10 0x82
#define DTCQ_GAMMA_BLUE10 0x83

static
SANE_Status
send (SnapScan_Scanner * pss, u_char dtc, u_char dtcq)
{
  static char me[] = "send";
  SANE_Status status;
  u_short tl;			/* transfer length */

  DBG (DL_CALL_TRACE, "%s\n", me);

  zero_buf (pss->buf, SEND_LENGTH);

  switch (dtc)
    {
    case DTC_HALFTONE:		/* halftone mask */
      switch (dtcq)
	{
	case DTCQ_HALFTONE_BW8:
	  tl = 64;		/* bw 8x8 table */
	  break;
	case DTCQ_HALFTONE_COLOR8:
	  tl = 3 * 64;		/* rgb 8x8 tables */
	  break;
	case DTCQ_HALFTONE_BW16:
	  tl = 256;		/* bw 16x16 table */
	  break;
	case DTCQ_HALFTONE_COLOR16:
	  tl = 3 * 256;		/* rgb 16x16 tables */
	  break;
	default:
	  DBG (DL_MAJOR_ERROR, "%s: bad halftone data type qualifier 0x%x\n",
	       me, dtcq);
	  return SANE_STATUS_INVAL;
	}
      break;
    case DTC_GAMMA:		/* gamma function */
      switch (dtcq)
	{
	case DTCQ_GAMMA_GRAY8:	/* 8-bit tables */
	case DTCQ_GAMMA_RED8:
	case DTCQ_GAMMA_GREEN8:
	case DTCQ_GAMMA_BLUE8:
	  tl = 256;
	  break;
	case DTCQ_GAMMA_GRAY10:	/* 10-bit tables */
	case DTCQ_GAMMA_RED10:
	case DTCQ_GAMMA_GREEN10:
	case DTCQ_GAMMA_BLUE10:
	  tl = 1024;
	  break;
	default:
	  DBG (DL_MAJOR_ERROR, "%s: bad gamma data type qualifier 0x%x\n",
	       me, dtcq);
	  return SANE_STATUS_INVAL;
	}
      break;
    case DTC_SPEED:		/* static transfer speed */
      tl = 2;
      break;
    default:
      DBG (DL_MAJOR_ERROR, "%s: unsupported data type code 0x%x\n",
	   me, (unsigned) dtc);
      return SANE_STATUS_INVAL;
    }

  pss->buf[0] = SEND;
  pss->buf[2] = dtc;
  pss->buf[5] = dtcq;
  pss->buf[7] = (tl >> 8) & 0xff;
  pss->buf[8] = tl & 0xff;

  status = sanei_scsi_cmd (pss->fd, pss->buf, SEND_LENGTH + tl,
			   NULL, NULL);
  CHECK_STATUS (status, me, "sane_scsi_cmd");
  return status;
}


#define SET_WINDOW_LEN 10
#define SET_WINDOW_HEADER 10	/* header starts */
#define SET_WINDOW_HEADER_LEN 8
#define SET_WINDOW_DESC 18	/* window descriptor starts */
#define SET_WINDOW_DESC_LEN 48
#define SET_WINDOW_TRANSFER_LEN 56
#define SET_WINDOW_TOTAL_LEN 66
#define SET_WINDOW_RET_LEN   0	/* no returned data */

#define SET_WINDOW_P_TRANSFER_LEN 6
#define SET_WINDOW_P_DESC_LEN 6

#define SET_WINDOW_P_WIN_ID 0
#define SET_WINDOW_P_XRES   2
#define SET_WINDOW_P_YRES   4
#define SET_WINDOW_P_TLX    6
#define SET_WINDOW_P_TLY    10
#define SET_WINDOW_P_WIDTH  14
#define SET_WINDOW_P_LENGTH 18
#define SET_WINDOW_P_BRIGHTNESS   22
#define SET_WINDOW_P_THRESHOLD    23
#define SET_WINDOW_P_CONTRAST     24
#define SET_WINDOW_P_COMPOSITION  25
#define SET_WINDOW_P_BITS_PER_PIX 26
#define SET_WINDOW_P_HALFTONE_PATTERN  27
#define SET_WINDOW_P_PADDING_TYPE      29
#define SET_WINDOW_P_BIT_ORDERING      30
#define SET_WINDOW_P_COMPRESSION_TYPE  32
#define SET_WINDOW_P_COMPRESSION_ARG   33
#define SET_WINDOW_P_HALFTONE_FLAG     35
#define SET_WINDOW_P_DEBUG_MODE        40
#define SET_WINDOW_P_GAMMA_NO          41
#define SET_WINDOW_P_OPERATION_MODE    42
#define SET_WINDOW_P_RED_UNDER_COLOR   43
#define SET_WINDOW_P_BLUE_UNDER_COLOR  45
#define SET_WINDOW_P_GREEN_UNDER_COLOR 44

static
SANE_Status
set_window (SnapScan_Scanner * pss)
{
  static const char *me = "set_window";
  SANE_Status status;

  u_char *pc;

  DBG (DL_CALL_TRACE, "%s\n", me);
  zero_buf (pss->cmd, MAX_SCSI_CMD_LEN);

  /* basic command */
  pc = pss->cmd;
  pc[0] = SET_WINDOW;
  u_int_to_u_char3p ((u_int) SET_WINDOW_TRANSFER_LEN,
		     pc + SET_WINDOW_P_TRANSFER_LEN);

  /* header; we support only one window */
  pc += SET_WINDOW_LEN;
  u_short_to_u_charp (SET_WINDOW_DESC_LEN, pc + SET_WINDOW_P_DESC_LEN);

  /* the sole window descriptor */
  pc += SET_WINDOW_HEADER_LEN;
  pc[SET_WINDOW_P_WIN_ID] = 0;
  u_short_to_u_charp (pss->res, pc + SET_WINDOW_P_XRES);
  u_short_to_u_charp (pss->res, pc + SET_WINDOW_P_YRES);
  {
    unsigned tlxp =
      (unsigned) (pss->actual_res * IN_PER_MM * SANE_UNFIX (pss->tlx));
    unsigned tlyp =
      (unsigned) (pss->actual_res * IN_PER_MM * SANE_UNFIX (pss->tly));
    unsigned brxp =
      (unsigned) (pss->actual_res * IN_PER_MM * SANE_UNFIX (pss->brx));
    unsigned bryp =
      (unsigned) (pss->actual_res * IN_PER_MM * SANE_UNFIX (pss->bry));
    unsigned tmp;

    /* we don't guard against brx < tlx and bry < tly in the options */
    if (brxp < tlxp)
      {
	tmp = tlxp;
	tlxp = brxp;
	brxp = tmp;
      }

    if (bryp < tlyp)
      {
	tmp = tlyp;
	tlyp = bryp;
	bryp = tmp;
      }

    u_int_to_u_char4p (tlxp, pc + SET_WINDOW_P_TLX);
    u_int_to_u_char4p (tlyp, pc + SET_WINDOW_P_TLY);
    u_int_to_u_char4p (MAX (((unsigned) (brxp - tlxp)), 75),
		       pc + SET_WINDOW_P_WIDTH);
    u_int_to_u_char4p (MAX (((unsigned) (bryp - tlyp)), 75),
		       pc + SET_WINDOW_P_LENGTH);
  }
#ifdef INOPERATIVE
  pc[SET_WINDOW_P_BRIGHTNESS] =
    (u_char) (255.0 * ((pss->bright + 100) / 200.0));
#endif
  pc[SET_WINDOW_P_THRESHOLD] =
    (u_char) (255.0 * (pss->threshold / 100.0));
#ifdef INOPERATIVE
  pc[SET_WINDOW_P_CONTRAST] =
    (u_char) (255.0 * ((pss->contrast + 100) / 200.0));
#endif
  {
    SnapScan_Mode mode = pss->mode;
    u_char bpp;

    if (pss->preview)
      mode = pss->preview_mode;

    bpp = pss->pdev->depths[mode];

    switch (mode)
      {
      case MD_COLOUR:
	pc[SET_WINDOW_P_COMPOSITION] = 0x05;	/* multi-level RGB */
	bpp *= 3;
	break;
      case MD_BILEVELCOLOUR:
	if (pss->halftone)
	  pc[SET_WINDOW_P_COMPOSITION] = 0x04;	/* halftone RGB */
	else
	  pc[SET_WINDOW_P_COMPOSITION] = 0x03;	/* bi-level RGB */
	bpp *= 3;
	break;
      case MD_GREYSCALE:
	pc[SET_WINDOW_P_COMPOSITION] = 0x02;	/* grayscale */
	break;
      default:
	if (pss->halftone)
	  pc[SET_WINDOW_P_COMPOSITION] = 0x01;	/* b&w halftone */
	else
	  pc[SET_WINDOW_P_COMPOSITION] = 0x00;	/* b&w (lineart) */
	break;
      }

    pc[SET_WINDOW_P_BITS_PER_PIX] = bpp;
    DBG (DL_DATA_TRACE, "%s: bits-per-pixel set to %d\n", me, (int) bpp);
  }
  /* the RIF bit is the high bit of the padding type */
  pc[SET_WINDOW_P_PADDING_TYPE] = 0x00 /*| (pss->negative ? 0x00 : 0x80) */ ;
  pc[SET_WINDOW_P_HALFTONE_PATTERN] = 0;
  pc[SET_WINDOW_P_HALFTONE_FLAG] = 0x80;	/* always set; image
						   composition
						   determines whether
						   halftone is
						   actually used */

  u_short_to_u_charp (0x0000, pc + SET_WINDOW_P_BIT_ORDERING);	/* used? */
  pc[SET_WINDOW_P_COMPRESSION_TYPE] = 0;	/* none */
  pc[SET_WINDOW_P_COMPRESSION_ARG] = 0;		/* none applicable */
  pc[SET_WINDOW_P_DEBUG_MODE] = 2;	/* use full 128k buffer */
  pc[SET_WINDOW_P_GAMMA_NO] = 0x01;	/* downloaded table */
  pc[SET_WINDOW_P_OPERATION_MODE] = (pss->preview) ? 0x20 : 0x60;
  pc[SET_WINDOW_P_RED_UNDER_COLOR] = 0xff;	/* defaults */
  pc[SET_WINDOW_P_BLUE_UNDER_COLOR] = 0xff;
  pc[SET_WINDOW_P_GREEN_UNDER_COLOR] = 0xff;

  DBG (DL_CALL_TRACE, "%s\n", me);

  status = sanei_scsi_cmd (pss->fd, pss->cmd, SET_WINDOW_TOTAL_LEN,
			   NULL, NULL);
  CHECK_STATUS (status, me, "sanei_scsi_cmd");
  return status;
}

static
SANE_Status
scan (SnapScan_Scanner * pss)
{
  static const char *me = "scan";
  SANE_Status status;

  DBG (DL_CALL_TRACE, "%s\n", me);
  zero_buf (pss->cmd, MAX_SCSI_CMD_LEN);
  pss->cmd[0] = SCAN;

  DBG (DL_CALL_TRACE, "%s\n", me);

  status = sanei_scsi_cmd (pss->fd, pss->cmd, SCAN_LEN, NULL, NULL);
  CHECK_STATUS (status, me, "sanei_scsi_cmd");
  return status;
}

/* supported read operations */

#define READ_IMAGE     0x00
#define READ_TRANSTIME 0x80

/* number of bytes expected must be in pss->expected_read_bytes */
static
SANE_Status
scsi_read (SnapScan_Scanner * pss, u_char read_type)
{
  static const char *me = "scsi_read";
  SANE_Status status;

  DBG (DL_CALL_TRACE, "%s\n", me);
  zero_buf (pss->cmd, MAX_SCSI_CMD_LEN);
  pss->cmd[0] = READ;
  pss->cmd[2] = read_type;
  u_int_to_u_char3p (pss->expected_read_bytes, pss->cmd + 6);

  pss->read_bytes = pss->expected_read_bytes;

  status = sanei_scsi_cmd (pss->fd, pss->cmd, READ_LEN,
			   pss->buf, &pss->read_bytes);
  CHECK_STATUS (status, me, "sanei_scsi_cmd");
  return status;
}


static
SANE_Status
request_sense (SnapScan_Scanner * pss)
{
  static const char *me = "request_sense";
  size_t read_bytes = 0;
  u_char cmd[] =
  {REQUEST_SENSE, 0, 0, 0, 20, 0};
  u_char data[20];
  SANE_Status status;

  read_bytes = 20;

  DBG (DL_CALL_TRACE, "%s\n", me);
  status = sanei_scsi_cmd (pss->fd, cmd, sizeof (cmd), data, &read_bytes);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DL_MAJOR_ERROR, "%s: scsi command error: %s\n",
	   me, sane_strstatus (status));
    }
  else
    {
      status = sense_handler (pss->fd, data, (void *) pss);
    }
  return status;
}


static
SANE_Status
send_diagnostic (SnapScan_Scanner * pss)
{
  static const char *me = "send_diagnostic";
  u_char cmd[] =
  {SEND_DIAGNOSTIC, 0x04, 0, 0, 0, 0};	/* self-test */
  SANE_Status status;

  if (pss->pdev->model == PRISA620S)	/* GP added */
    return SANE_STATUS_GOOD;

  DBG (DL_CALL_TRACE, "%s\n", me);

  status = sanei_scsi_cmd (pss->fd, cmd, sizeof (cmd), NULL, NULL);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DL_MAJOR_ERROR, "%s: scsi command error: %s\n",
	   me, sane_strstatus (status));
    }
  return status;
}


#if 0

/* get_data_buffer_status: fetch the scanner's current data buffer
   status. If wait == 0, the scanner must respond immediately;
   otherwise it will wait until there is data available in the buffer
   before reporting. */

#define GET_DATA_BUFFER_STATUS_LEN 10
#define DESCRIPTOR_LENGTH 12

static
SANE_Status
get_data_buffer_status (SnapScan_Scanner * pss, int wait)
{
  static const char *me = "get_data_buffer_status";
  SANE_Status status;

  pss->read_bytes = DESCRIPTOR_LENGTH;	/* one status descriptor only */

  DBG (DL_CALL_TRACE, "%s\n", me);

  zero_buf (pss->cmd, MAX_SCSI_CMD_LEN);
  pss->cmd[0] = GET_DATA_BUFFER_STATUS;


  if (wait)
    {
      pss->cmd[1] = 0x01;
    }
  u_short_to_u_charp (DESCRIPTOR_LENGTH, pss->cmd + 7);

  status = sanei_scsi_cmd (pss->fd, pss->cmd, GET_DATA_BUFFER_STATUS_LEN,
			   pss->buf, &pss->read_bytes);
  CHECK_STATUS (status, me, "sanei_scsi_cmd");
  return status;
}

#endif


static
SANE_Status
wait_scanner_ready (SnapScan_Scanner * pss)
{
  static char me[] = "wait_scanner_ready";
  SANE_Status status;
  int retries;

  DBG (DL_CALL_TRACE, "%s\n", me);

  for (retries = 5; retries; retries--)
    {
      status = test_unit_ready (pss);
      if (status == SANE_STATUS_GOOD)
	{
	  status = request_sense (pss);
	  switch (status)
	    {
	    case SANE_STATUS_GOOD:
	      return status;
	      break;
	    case SANE_STATUS_DEVICE_BUSY:
	      /* first additional sense byte contains time to wait */
	      {
		int delay = pss->asi1 + 1;
		DBG (DL_INFO, "%s: scanner warming up. Waiting %ld seconds.\n",
		     me, (long) delay);
		sleep (delay);
	      }
	      break;
	    case SANE_STATUS_IO_ERROR:
	      /* hardware error; bail */
	      DBG (DL_MAJOR_ERROR, "%s: hardware error detected.\n", me);
	      return status;
	      break;		/* not reached */
	    default:
	      DBG (DL_MAJOR_ERROR,
		   "%s: unhandled request_sense result; trying again.\n",
		   me);
	      break;
	    }
	}
    }

  return status;
}

