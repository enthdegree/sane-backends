/* sane - Scanner Access Now Easy.

   Copyright (C) 1997, 1998, 2001 Franck Schnefra, Michel Roelofs,
   Emmanuel Blot, Mikko Tyolajarvi, David Mosberger-Tang, Wolfgang Goeller,
   Petter Reinholdtsen, Gary Plewa, Sebastien Sable, Mikael Magnusson,
   Oliver Schwartz and Kevin Charter

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
   SnapScan backend scsi command functions */

/* scanner scsi commands */

static SANE_Status download_firmware(SnapScan_Scanner * pss);
static SANE_Status wait_scanner_ready (SnapScan_Scanner * pss);

#include "snapscan-usb.h"

/* find model by SCSI ID string or USB vendor/product ID */
static SnapScan_Model snapscani_get_model_id(char* model_str, int fd, SnapScan_Bus bus_type)
{
    static char me[] = "snapscani_get_model_id";
    SnapScan_Model model_id = UNKNOWN;
    SANE_Word vendor_id, product_id;
    int i;

    DBG(DL_CALL_TRACE, "%s(%s, %d, %d)\n",me, model_str, fd, bus_type);
    for (i = 0;  i < known_scanners;  i++)
    {
        if (0 == strcasecmp (model_str, scanners[i].scsi_name))
        {
            model_id = scanners[i].id;
            break;
        }
    }
    /* Also test USB vendor and product ID numbers, since some USB models use
       identical model names.
    */
    if ((bus_type == USB) &&
        (sanei_usb_get_vendor_product(fd, &vendor_id, &product_id) == SANE_STATUS_GOOD))
    {
    	DBG(DL_MINOR_INFO, 
            "%s: looking up scanner for ID 0x%04x,0x%04x.\n",
            me, vendor_id, product_id);           
        for (i = 0; i < known_usb_scanners; i++)
        {
            if ((usb_scanners[i].vendor_id == vendor_id) &&
                (usb_scanners[i].product_id == product_id))
            {
                model_id = usb_scanners[i].id;
                DBG(DL_MINOR_INFO, "%s: scanner identified\n", me);
                break;
            }
        }
    }
    return model_id;
}

/* a sensible sense handler, courtesy of Franck;
   the last argument is expected to be a pointer to the associated
   SnapScan_Scanner structure */
static SANE_Status sense_handler (int scsi_fd, u_char * result, void *arg)
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
        return SANE_STATUS_GOOD;    /* sense key invalid */
    } else {
        DBG (DL_DATA_TRACE, "%s: sense key: 0x%02x, asc: 0x%02x, ascq: 0x%02x, i1: 0x%02x, i2: 0x%02x\n",
        me, sense, asc, ascq, result[18], result[19]);
    }

    switch (sense)
    {
    case 0x00:
        /* no sense */
        sense_str = "No sense.";
        DBG (DL_MINOR_INFO, "%s: %s\n", me, sense_str);
        break;
    case 0x02:
        /* not ready */
        sense_str = "Not ready.";
        DBG (DL_MINOR_INFO, "%s: %s\n", me, sense_str);
        if (asc == 0x04  &&  ascq == 0x01)
        {
            /* warming up; byte 18 contains remaining seconds */
            as_str = "Logical unit is in process of becoming ready.";
            DBG (DL_MINOR_INFO, "%s: %s (%d seconds)\n", me, as_str, result[18]);
            status = SANE_STATUS_DEVICE_BUSY;
        DBG (DL_MINOR_INFO, "%s: %s\n", me, sense_str);
        }
        break;
    case 0x04:
        /* hardware error */
        sense_str = "Hardware error.";
        /* byte 18 and 19 detail the hardware problems */
        DBG (DL_MINOR_INFO, "%s: %s (0x%02x, 0x%02x)\n", me, sense_str, result[18],
            result[19]);
        status = SANE_STATUS_IO_ERROR;
        break;
    case 0x05:
        /* illegal request */
        sense_str = "Illegal request.";
        DBG (DL_MINOR_INFO, "%s: %s\n", me, sense_str);
        if (asc == 0x25 && ascq == 0x00)
            as_str = "Logical unit not supported.";
            DBG (DL_MINOR_INFO, "%s: %s\n", me, as_str);
        status = SANE_STATUS_IO_ERROR;
        break;
    case 0x09:
        /* process error */
        sense_str = "Process error.";
        DBG (DL_MINOR_INFO, "%s: %s\n", me, sense_str);
        if (asc == 0x00 && ascq == 0x05)
        {
            /* no documents in ADF */
            as_str = "End of data detected.";
            DBG (DL_MINOR_INFO, "%s: %s\n", me, as_str);
            status = SANE_STATUS_NO_DOCS;
        }
        else if (asc == 0x3b && ascq == 0x05)
        {
            /* paper jam in ADF */
            as_str = "Paper jam.";
            DBG (DL_MINOR_INFO, "%s: %s\n", me, as_str);
            status = SANE_STATUS_JAMMED;
        }
        else if (asc == 0x3b && ascq == 0x09)
        {
            /* scanning area exceeds end of paper in ADF */
            as_str = "Read past end of medium.";
            DBG (DL_MINOR_INFO, "%s: %s\n", me, as_str);
            status = SANE_STATUS_EOF;
        }
        break;
    case 0x0b:
        /* Aborted command */
        sense_str = "Aborted Command.";
        DBG (DL_MINOR_INFO, "%s: %s\n", me, sense_str);
        status = SANE_STATUS_IO_ERROR;
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


static SANE_Status open_scanner (SnapScan_Scanner *pss)
{
    SANE_Status status;
    DBG (DL_CALL_TRACE, "open_scanner\n");
    if (!pss->opens)
    {
        if(pss->pdev->bus == SCSI)
        {
            status = sanei_scsi_open (pss->devname, &(pss->fd),
                sense_handler, (void *) pss);
        }
        else
        {
            status = snapscani_usb_open (pss->devname, &(pss->fd),
                sense_handler, (void *) pss);
        }
    }
    else
    {
        /* already open */
        status = SANE_STATUS_GOOD;
    }
    if (status == SANE_STATUS_GOOD)
        pss->opens++;
    return status;
}

static void close_scanner (SnapScan_Scanner *pss)
{
    static char me[] = "close_scanner";

    DBG (DL_CALL_TRACE, "%s\n", me);
    if (pss->opens)
    {
        pss->opens--;
        if (!pss->opens)
        {
            if(pss->pdev->bus == SCSI)
            {
                sanei_scsi_close (pss->fd);
            }
            else if(pss->pdev->bus == USB)
            {
                snapscani_usb_close (pss->fd);
            }
        } else {
            DBG(DL_INFO, "%s: handles left: %d\n,",me, pss->opens);
        }
    }
}

static SANE_Status snapscan_cmd(SnapScan_Bus bus, int fd, const void *src,
                size_t src_size, void *dst, size_t * dst_size)
{
    SANE_Status status;
    DBG (DL_CALL_TRACE, "snapscan_cmd\n");
    if(bus == USB)
    {
        status = snapscani_usb_cmd(fd, src, src_size, dst, dst_size);
    }
    else
    {
        status = sanei_scsi_cmd(fd, src, src_size, dst, dst_size);
    }
    return status;
}

/* SCSI commands */
#define TEST_UNIT_READY        0x00
#define INQUIRY                0x12
#define SEND                   0x2A
#define SET_WINDOW             0x24
#define SCAN                   0x1B
#define READ                   0x28
#define REQUEST_SENSE          0x03
#define RESERVE_UNIT           0x16
#define RELEASE_UNIT           0x17
#define SEND_DIAGNOSTIC        0x1D
#define GET_DATA_BUFFER_STATUS 0x34

#define SCAN_LEN 6
#define READ_LEN 10

/*  buffer tools */

static void zero_buf (u_char * buf, size_t len)
{
    size_t i;

    for (i = 0;  i < len;  i++)
        buf[i] = 0x00;
}


#define BYTE_SIZE 8

static u_short u_char_to_u_short (u_char * pc)
{
    u_short r = 0;
    r |= pc[0];
    r = r << BYTE_SIZE;
    r |= pc[1];
    return r;
}

static void u_short_to_u_charp (u_short x, u_char * pc)
{
    pc[0] = (0xFF00 & x) >> BYTE_SIZE;
    pc[1] = (0x00FF & x);
}

static void u_int_to_u_char3p (u_int x, u_char * pc)
{
    pc[0] = (0xFF0000 & x) >> 2 * BYTE_SIZE;
    pc[1] = (0x00FF00 & x) >> BYTE_SIZE;
    pc[2] = (0x0000FF & x);
}

static void u_int_to_u_char4p (u_int x, u_char * pc)
{
    pc[0] = (0xFF000000 & x) >> 3 * BYTE_SIZE;
    pc[1] = (0x00FF0000 & x) >> 2 * BYTE_SIZE;
    pc[2] = (0x0000FF00 & x) >> BYTE_SIZE;
    pc[3] = (0x000000FF & x);
}

/* Convert 'STRING   ' to 'STRING' by adding 0 after last non-space */
static void remove_trailing_space (char *s)
{
    int position;

    if (NULL == s)
        return;

    for (position = strlen (s);
         position > 0  &&  ' ' == s[position - 1];
         position--)
    {
        /* dummy */;
    }
    s[position] = 0;
}

static void check_range (int *v, SANE_Range r)
{
    if (*v < r.min)
    *v = r.min;
    if (*v > r.max)
    *v = r.max;
}

#define INQUIRY_LEN 6
#define INQUIRY_RET_LEN 120

#define INQUIRY_VENDOR        8    /* Offset in reply data to vendor name */
#define INQUIRY_PRODUCT        16    /* Offset in reply data to product id */
#define INQUIRY_REV            32    /* Offset in reply data to revision level */
#define INQUIRY_PRL2        36    /* Product Revision Level 2 (AGFA) */
#define INQUIRY_HCFG        37    /* Hardware Configuration (AGFA) */
#define INQUIRY_HWST        40    /* Hardware status */
#define INQUIRY_HWMI          41    /* HARDWARE Model ID */
#define INQUIRY_PIX_PER_LINE    42    /* Pixels per scan line (AGFA) */
#define INQUIRY_BYTE_PER_LINE    44    /* Bytes per scan line (AGFA) */
#define INQUIRY_NUM_LINES    46    /* number of scan lines (AGFA) */
#define INQUIRY_OPT_RES        48    /* optical resolution (AGFA) */
#define INQUIRY_SCAN_SPEED    51    /* scan speed (AGFA) */
#define INQUIRY_EXPTIME1    52    /* exposure time, first digit (AGFA) */
#define INQUIRY_EXPTIME2    53    /* exposure time, second digit (AGFA) */
#define INQUIRY_G2R_DIFF    54    /* green to red difference (AGFA) */
#define INQUIRY_B2R_DIFF    55    /* green to red difference (AGFA) */
#define INQUIRY_FIRMWARE    96    /* firmware date and time (AGFA) */


/* a mini-inquiry reads only the first 36 bytes of inquiry data, and
   returns the vendor(7 chars) and model(16 chars); vendor and model
   must point to character buffers of size at least 8 and 17
   respectively */

static SANE_Status mini_inquiry (SnapScan_Bus bus, int fd, char *vendor, char *model)
{
    static const char *me = "mini_inquiry";
    size_t read_bytes;
    char cmd[] = {INQUIRY, 0, 0, 0, 36, 0};
    char data[36];
    SANE_Status status;

    read_bytes = 36;

    DBG (DL_CALL_TRACE, "%s\n", me);
    status = snapscan_cmd (bus, fd, cmd, sizeof (cmd), data, &read_bytes);
    CHECK_STATUS (status, me, "snapscan_cmd");

    memcpy (vendor, data + 8, 7);
    vendor[7] = 0;
    memcpy (model, data + 16, 16);
    model[16] = 0;

    remove_trailing_space (vendor);
    remove_trailing_space (model);

    return SANE_STATUS_GOOD;
}

static SANE_Status inquiry (SnapScan_Scanner *pss)
{
    static const char *me = "inquiry";

    SANE_Status status;
    pss->read_bytes = INQUIRY_RET_LEN;

    zero_buf (pss->cmd, MAX_SCSI_CMD_LEN);
    pss->cmd[0] = INQUIRY;
    pss->cmd[4] = INQUIRY_RET_LEN;

    DBG (DL_CALL_TRACE, "%s\n", me);
    status = snapscan_cmd (pss->pdev->bus,
               pss->fd,
               pss->cmd,
               INQUIRY_LEN,
               pss->buf,
               &pss->read_bytes);
    CHECK_STATUS (status, me, "snapscan_cmd");
    /* Download Firmware for USB scanners */
    if ( (pss->pdev->bus == USB) && (*(pss->buf + INQUIRY_HWST) & 0x02) )
    {
        char model[17];

        status = download_firmware(pss);
        CHECK_STATUS (status, me, "download_firmware");
        /* send inquiry command again, wait for scanner to initialize */
        do {
            status = snapscan_cmd (pss->pdev->bus,
                pss->fd,
                pss->cmd,
                INQUIRY_LEN,
                pss->buf,
                &pss->read_bytes);
            if (status == SANE_STATUS_DEVICE_BUSY) {
                DBG (DL_INFO, "%s: Waiting for scanner after firmware upload\n",me);
                sleep(1);
            }
        } while (status == SANE_STATUS_DEVICE_BUSY);
        CHECK_STATUS (status, me, "snapscan_cmd");
        /* The model identifier will change after firmware upload */
        memcpy (model, &pss->buf[INQUIRY_PRODUCT], 16);
        model[16] = 0;
        remove_trailing_space(model);
        DBG (DL_INFO,
            "%s (after firmware upload): Checking if \"%s\" is a supported scanner\n",
            me,
            model);
        /* Check if it is one of our supported models */
        pss->pdev->model = snapscani_get_model_id(model, pss->fd, pss->pdev->bus);
        if (pss->pdev->model == UNKNOWN) {
            DBG (DL_MINOR_ERROR,
                "%s (after firmware upload): \"%s\" is not a supported scanner\n",
                me,
                model);
        }
    }
    /* record current parameters */
    {
        char exptime[4] = {' ', '.', ' ', 0};
        exptime[0] = (char) (pss->buf[INQUIRY_EXPTIME1] + '0');
        exptime[2] = (char) (pss->buf[INQUIRY_EXPTIME2] + '0');
        pss->ms_per_line = atof (exptime)*(float) pss->buf[INQUIRY_SCAN_SPEED];
        DBG (DL_DATA_TRACE, "%s: exposure time: %s ms\n", me, exptime);
        DBG (DL_DATA_TRACE, "%s: ms per line: %f\n", me, pss->ms_per_line);
    }

    switch (pss->pdev->model)
    {
    case SNAPSCAN:
    case ACER300F:
        pss->chroma_offset[R_CHAN] =
        pss->chroma_offset[G_CHAN] =
        pss->chroma_offset[B_CHAN] = 0;
        pss->chroma = 0;
        break;
    default:
    {
        signed char min_diff;
        u_char r_off, g_off, b_off;
        signed char g = (pss->buf[INQUIRY_G2R_DIFF] & 0x80) ? -(pss->buf[INQUIRY_G2R_DIFF] & 0x7F) : pss->buf[INQUIRY_G2R_DIFF];
        signed char b = (pss->buf[INQUIRY_B2R_DIFF] & 0x80) ? -(pss->buf[INQUIRY_B2R_DIFF] & 0x7F) : pss->buf[INQUIRY_B2R_DIFF];
        DBG (DL_DATA_TRACE, "%s: G2R_DIFF: %d\n", me, pss->buf[INQUIRY_G2R_DIFF]);
        DBG (DL_DATA_TRACE, "%s: B2R_DIFF: %d\n", me, pss->buf[INQUIRY_B2R_DIFF]);

        min_diff = MIN (MIN (b, g), 0);
        r_off = (u_char) (0 - min_diff);
        g_off = (u_char) (g - min_diff);
        b_off = (u_char) (b - min_diff);
        pss->chroma_offset[R_CHAN] = r_off;
        pss->chroma_offset[G_CHAN] = g_off;
        pss->chroma_offset[B_CHAN] = b_off;
        pss->chroma = MAX(MAX(r_off, g_off), b_off);
        DBG (DL_DATA_TRACE,
            "%s: Chroma offsets=%d; Red=%u, Green:=%u, Blue=%u\n",
            me, pss->chroma,
            pss->chroma_offset[R_CHAN],
            pss->chroma_offset[G_CHAN],
            pss->chroma_offset[B_CHAN]);
        }
        break;
    }

    pss->actual_res =
        u_char_to_u_short (pss->buf + INQUIRY_OPT_RES);

    pss->pixels_per_line =
        u_char_to_u_short (pss->buf + INQUIRY_PIX_PER_LINE);
    pss->bytes_per_line =
        u_char_to_u_short (pss->buf + INQUIRY_BYTE_PER_LINE);
    pss->lines =
        u_char_to_u_short (pss->buf + INQUIRY_NUM_LINES) - pss->chroma;
    /* effective buffer size must be a whole number of scan lines */
    if (pss->lines)
        pss->buf_sz = (pss->phys_buf_sz/pss->bytes_per_line)*pss->bytes_per_line;
    else
        pss->buf_sz = 0;
    pss->bytes_remaining = pss->bytes_per_line * (pss->lines + pss->chroma);
    pss->expected_read_bytes = 0;
    pss->read_bytes = 0;
    pss->hconfig = pss->buf[INQUIRY_HCFG];
    DBG (DL_DATA_TRACE,
        "%s: pixels per scan line = %lu\n",
        me,
        (u_long) pss->pixels_per_line);
    DBG (DL_DATA_TRACE,
        "%s: bytes per scan line = %lu\n",
        me,
        (u_long) pss->bytes_per_line);
    DBG (DL_DATA_TRACE,
        "%s: number of scan lines = %lu\n",
        me,
        (u_long) pss->lines);
    DBG (DL_DATA_TRACE,
         "%s: effective buffer size = %lu bytes, %lu lines\n",
         me,
        (u_long) pss->buf_sz,
        (u_long) (pss->lines ? pss->buf_sz / pss->lines : 0));
    DBG (DL_DATA_TRACE,
        "%s: expected total scan data: %lu bytes\n",
        me,
        (u_long) pss->bytes_remaining);

    return status;
}

static SANE_Status test_unit_ready (SnapScan_Scanner *pss)
{
    static const char *me = "test_unit_ready";
    char cmd[] = {TEST_UNIT_READY, 0, 0, 0, 0, 0};
    SANE_Status status;

    DBG (DL_CALL_TRACE, "%s\n", me);
    status = snapscan_cmd (pss->pdev->bus, pss->fd, cmd, sizeof (cmd), NULL, NULL);
    CHECK_STATUS (status, me, "snapscan_cmd");
    return status;
}

static void reserve_unit (SnapScan_Scanner *pss)
{
    static const char *me = "reserve_unit";
    char cmd[] = {RESERVE_UNIT, 0, 0, 0, 0, 0};
    SANE_Status status;

    DBG (DL_CALL_TRACE, "%s\n", me);
    status = snapscan_cmd (pss->pdev->bus, pss->fd, cmd, sizeof (cmd), NULL, NULL);
    if (status != SANE_STATUS_GOOD)
    {
        DBG (DL_MAJOR_ERROR,
            "%s: scsi command error: %s\n",
            me,
            sane_strstatus (status));
    }
}

static void release_unit (SnapScan_Scanner *pss)
{
    static const char *me = "release_unit";
    char cmd[] = {RELEASE_UNIT, 0, 0, 0, 0, 0};
    SANE_Status status;

    DBG (DL_CALL_TRACE, "%s\n", me);
    status = snapscan_cmd (pss->pdev->bus, pss->fd, cmd, sizeof (cmd), NULL, NULL);
    if (status != SANE_STATUS_GOOD)
    {
        DBG (DL_MAJOR_ERROR,
            "%s: scsi command error: %s\n",
            me, sane_strstatus (status));
    }
}

#define SEND_LENGTH 10
#define DTC_HALFTONE 0x02
#define DTC_GAMMA 0x03
#define DTC_SPEED 0x81
#define DTC_CALIBRATION 0x82
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

static SANE_Status send (SnapScan_Scanner *pss, u_char dtc, u_char dtcq)
{
    static char me[] = "send";
    SANE_Status status;
    u_short tl;            /* transfer length */

    DBG (DL_CALL_TRACE, "%s\n", me);

    zero_buf (pss->buf, SEND_LENGTH);

    switch (dtc)
    {
    case DTC_HALFTONE:        /* halftone mask */
        switch (dtcq)
        {
        case DTCQ_HALFTONE_BW8:
            tl = 64;        /* bw 8x8 table */
            break;
        case DTCQ_HALFTONE_COLOR8:
            tl = 3 * 64;    /* rgb 8x8 tables */
            break;
        case DTCQ_HALFTONE_BW16:
            tl = 256;        /* bw 16x16 table */
            break;
        case DTCQ_HALFTONE_COLOR16:
            tl = 3 * 256;    /* rgb 16x16 tables */
            break;
        default:
            DBG (DL_MAJOR_ERROR, "%s: bad halftone data type qualifier 0x%x\n",
                 me, dtcq);
            return SANE_STATUS_INVAL;
        }
        break;
    case DTC_GAMMA:        /* gamma function */
        switch (dtcq)
        {
        case DTCQ_GAMMA_GRAY8:    /* 8-bit tables */
        case DTCQ_GAMMA_RED8:
        case DTCQ_GAMMA_GREEN8:
        case DTCQ_GAMMA_BLUE8:
            tl = 256;
            break;
        case DTCQ_GAMMA_GRAY10:    /* 10-bit tables */
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
    case DTC_SPEED:        /* static transfer speed */
        tl = 2;
        break;
    case DTC_CALIBRATION:
        tl = calibration_line_length(pss);
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

    status = snapscan_cmd (pss->pdev->bus, pss->fd, pss->buf, SEND_LENGTH + tl,
                             NULL, NULL);
    CHECK_STATUS (status, me, "snapscan_cmd");
    return status;
}

#define SET_WINDOW_LEN             10
#define SET_WINDOW_HEADER         10    /* header starts */
#define SET_WINDOW_HEADER_LEN         8
#define SET_WINDOW_DESC         18    /* window descriptor starts */
#define SET_WINDOW_DESC_LEN         48
#define SET_WINDOW_TRANSFER_LEN     56
#define SET_WINDOW_TOTAL_LEN         66
#define SET_WINDOW_RET_LEN           0    /* no returned data */

#define SET_WINDOW_P_TRANSFER_LEN     6
#define SET_WINDOW_P_DESC_LEN         6

#define SET_WINDOW_P_WIN_ID         0
#define SET_WINDOW_P_XRES           2
#define SET_WINDOW_P_YRES           4
#define SET_WINDOW_P_TLX            6
#define SET_WINDOW_P_TLY            10
#define SET_WINDOW_P_WIDTH          14
#define SET_WINDOW_P_LENGTH         18
#define SET_WINDOW_P_BRIGHTNESS       22
#define SET_WINDOW_P_THRESHOLD        23
#define SET_WINDOW_P_CONTRAST         24
#define SET_WINDOW_P_COMPOSITION      25
#define SET_WINDOW_P_BITS_PER_PIX     26
#define SET_WINDOW_P_HALFTONE_PATTERN      27
#define SET_WINDOW_P_PADDING_TYPE          29
#define SET_WINDOW_P_BIT_ORDERING          30
#define SET_WINDOW_P_COMPRESSION_TYPE      32
#define SET_WINDOW_P_COMPRESSION_ARG       33
#define SET_WINDOW_P_HALFTONE_FLAG         35
#define SET_WINDOW_P_DEBUG_MODE            40
#define SET_WINDOW_P_GAMMA_NO              41
#define SET_WINDOW_P_OPERATION_MODE        42
#define SET_WINDOW_P_RED_UNDER_COLOR       43
#define SET_WINDOW_P_BLUE_UNDER_COLOR      45
#define SET_WINDOW_P_GREEN_UNDER_COLOR     44

static SANE_Status set_window (SnapScan_Scanner *pss)
{
    static const char *me = "set_window";
    SANE_Status status;
    unsigned char source;
    u_char *pc;
    int pos_factor;

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
    DBG (DL_CALL_TRACE, "%s Resolution: %d\n", me, pss->res);

    pos_factor = pss->actual_res;
    if (pss->pdev->model == PRISA5000) 
    {
        if (pss->res > 600) 
        {
            pos_factor = 1200;
        }
        else
        {
            pos_factor = 600;
        }    
    }

    /* it's an ugly sound if the scanner drives against the rear
       wall, and with changing max values we better be sure */
    check_range(&(pss->brx), pss->pdev->x_range);
    check_range(&(pss->bry), pss->pdev->y_range);
    {
        int tlxp =
            (int) (pos_factor*IN_PER_MM*SANE_UNFIX(pss->tlx));
        int tlyp =
            (int) (pos_factor*IN_PER_MM*SANE_UNFIX(pss->tly));
        int brxp =
            (int) (pos_factor*IN_PER_MM*SANE_UNFIX(pss->brx));
        int bryp =
            (int) (pos_factor*IN_PER_MM*SANE_UNFIX(pss->bry));

        /* Check for brx > tlx and bry > tly */
        if (brxp <= tlxp) {
            tlxp = MAX (0, brxp - 75);
        }
        if (bryp <= tlyp) {
            tlyp = MAX (0, bryp - 75);
        }

        u_int_to_u_char4p (tlxp, pc + SET_WINDOW_P_TLX);
        u_int_to_u_char4p (tlyp, pc + SET_WINDOW_P_TLY);
        u_int_to_u_char4p (MAX (((unsigned) (brxp - tlxp)), 75),
                           pc + SET_WINDOW_P_WIDTH);
        u_int_to_u_char4p (MAX (((unsigned) (bryp - tlyp)), 75),
                           pc + SET_WINDOW_P_LENGTH);
        DBG (DL_INFO, "%s Width:  %d\n", me, brxp-tlxp);
        DBG (DL_INFO, "%s Length: %d\n", me, bryp-tlyp);
    }
    pc[SET_WINDOW_P_BRIGHTNESS] = 128;
    pc[SET_WINDOW_P_THRESHOLD] =
        (u_char) (255.0*(pss->threshold / 100.0));
    pc[SET_WINDOW_P_CONTRAST] = 128;
    {
        SnapScan_Mode mode = pss->mode;
        u_char bpp;

        if (pss->preview)
            mode = pss->preview_mode;

        bpp = pss->pdev->depths[mode];
        DBG (DL_MINOR_INFO, "%s Mode: %d\n", me, mode);
        switch (mode)
        {
        case MD_COLOUR:
            pc[SET_WINDOW_P_COMPOSITION] = 0x05;    /* multi-level RGB */
            bpp *= 3;
            break;
        case MD_BILEVELCOLOUR:
            if (pss->halftone)
                pc[SET_WINDOW_P_COMPOSITION] = 0x04;    /* halftone RGB */
            else
                pc[SET_WINDOW_P_COMPOSITION] = 0x03;    /* bi-level RGB */
            bpp *= 3;
            break;
        case MD_GREYSCALE:
            pc[SET_WINDOW_P_COMPOSITION] = 0x02;    /* grayscale */
            break;
        default:
            if (pss->halftone)
                pc[SET_WINDOW_P_COMPOSITION] = 0x01;    /* b&w halftone */
            else
                pc[SET_WINDOW_P_COMPOSITION] = 0x00;    /* b&w (lineart) */
            break;
        }

        pc[SET_WINDOW_P_BITS_PER_PIX] = bpp;
        DBG (DL_INFO, "%s: bits-per-pixel set to %d\n", me, (int) bpp);
    }
    /* the RIF bit is the high bit of the padding type */
    pc[SET_WINDOW_P_PADDING_TYPE] = 0x00 /*| (pss->negative ? 0x00 : 0x80) */ ;
    pc[SET_WINDOW_P_HALFTONE_PATTERN] = 0;
    pc[SET_WINDOW_P_HALFTONE_FLAG] = 0x80;    /* always set; image
                                            composition
                                            determines whether
                                            halftone is
                                            actually used */

    u_short_to_u_charp (0x0000, pc + SET_WINDOW_P_BIT_ORDERING);    /* used? */
    pc[SET_WINDOW_P_COMPRESSION_TYPE] = 0;    /* none */
    pc[SET_WINDOW_P_COMPRESSION_ARG] = 0;    /* none applicable */
    if(pss->pdev->model != ACER300F
       &&
       pss->pdev->model != SNAPSCAN310
       &&
       pss->pdev->model != PRISA310
       &&
       pss->pdev->model != PRISA610
    ) {
        pc[SET_WINDOW_P_DEBUG_MODE] = 2;        /* use full 128k buffer */
        pc[SET_WINDOW_P_GAMMA_NO] = 0x01;        /* downloaded table */
    }
    source = 0x20;
    if (pss->preview) {
        source |= 0x80; /* no high quality */
    } else {
        source |= 0x40; /* no preview */
    }

    if (pss->source == SRC_TPO) {
        source |= 0x08;
    }
    if (pss->source == SRC_ADF) {
        source |= 0x10;
    }
    pc[SET_WINDOW_P_OPERATION_MODE] = source;
    DBG (DL_MINOR_INFO, "%s: operation mode set to %d\n", me, (int) source);
    pc[SET_WINDOW_P_RED_UNDER_COLOR] = 0xff;    /* defaults */
    pc[SET_WINDOW_P_BLUE_UNDER_COLOR] = 0xff;
    pc[SET_WINDOW_P_GREEN_UNDER_COLOR] = 0xff;

    do {
        status = snapscan_cmd (pss->pdev->bus, pss->fd, pss->cmd,
                  SET_WINDOW_TOTAL_LEN, NULL, NULL);
        if (status == SANE_STATUS_DEVICE_BUSY) {
            DBG (DL_MINOR_INFO, "%s: waiting for scanner to warm up\n", me);
            wait_scanner_ready (pss);
        }
    } while (status == SANE_STATUS_DEVICE_BUSY);

    CHECK_STATUS (status, me, "snapscan_cmd");
    return status;
}

static SANE_Status scan (SnapScan_Scanner *pss)
{
    static const char *me = "scan";
    SANE_Status status;

    DBG (DL_CALL_TRACE, "%s\n", me);
    zero_buf (pss->cmd, MAX_SCSI_CMD_LEN);
    pss->cmd[0] = SCAN;
    status = snapscan_cmd (pss->pdev->bus, pss->fd, pss->cmd, SCAN_LEN, NULL, NULL);
    CHECK_STATUS (status, me, "snapscan_cmd");
    return status;
}

/* supported read operations */

#define READ_IMAGE     0x00
#define READ_TRANSTIME 0x80

/* number of bytes expected must be in pss->expected_read_bytes */
static SANE_Status scsi_read (SnapScan_Scanner *pss, u_char read_type)
{
    static const char *me = "scsi_read";
    SANE_Status status;

    DBG (DL_CALL_TRACE, "%s\n", me);
    zero_buf (pss->cmd, MAX_SCSI_CMD_LEN);
    pss->cmd[0] = READ;
    pss->cmd[2] = read_type;
    u_int_to_u_char3p (pss->expected_read_bytes, pss->cmd + 6);

    pss->read_bytes = pss->expected_read_bytes;

    status = snapscan_cmd (pss->pdev->bus, pss->fd, pss->cmd,
                 READ_LEN, pss->buf, &pss->read_bytes);
    CHECK_STATUS (status, me, "snapscan_cmd");
    return status;
}

/*
static SANE_Status request_sense (SnapScan_Scanner *pss)
{
    static const char *me = "request_sense";
    size_t read_bytes = 0;
    u_char cmd[] = {REQUEST_SENSE, 0, 0, 0, 20, 0};
    u_char data[20];
    SANE_Status status;

    read_bytes = 20;

    DBG (DL_CALL_TRACE, "%s\n", me);
    status = snapscan_cmd (pss->pdev->bus, pss->fd, cmd, sizeof (cmd),
               data, &read_bytes);
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
*/

static SANE_Status send_diagnostic (SnapScan_Scanner *pss)
{
    static const char *me = "send_diagnostic";
    u_char cmd[] = {SEND_DIAGNOSTIC, 0x04, 0, 0, 0, 0};    /* self-test */
    SANE_Status status;

    if (pss->pdev->model == PRISA620
        ||
	pss->pdev->model == PRISA610
	||
	pss->pdev->model == SNAPSCAN1236
        ||
        pss->pdev->model == ARCUS1200) 
    {
        return SANE_STATUS_GOOD;
    }
    DBG (DL_CALL_TRACE, "%s\n", me);

    status = snapscan_cmd (pss->pdev->bus, pss->fd, cmd, sizeof (cmd), NULL, NULL);
    CHECK_STATUS (status, me, "snapscan_cmd");
    return status;
}

static SANE_Status wait_scanner_ready (SnapScan_Scanner *pss)
{
    static char me[] = "wait_scanner_ready";
    SANE_Status status;
    int retries;

    DBG (DL_CALL_TRACE, "%s\n", me);

    /* if the tray is returning to the start position
       no time to wait is returned by the scanner. We'll
       try several times and sleep 1 second between each try. */
    for (retries = 20; retries; retries--)
    {
        status = test_unit_ready (pss);
        switch (status)
        {
        case SANE_STATUS_GOOD:
            return status;
        case SANE_STATUS_DEVICE_BUSY:
            /* first additional sense byte contains time to wait */
            {
                int delay = pss->asi1 + 1;
                DBG (DL_INFO,
                    "%s: scanner warming up. Waiting %ld seconds.\n",
                    me, (long) delay);
                sleep (delay);
            }
            break;
        case SANE_STATUS_IO_ERROR:
            /* hardware error; bail */
            DBG (DL_MAJOR_ERROR, "%s: hardware error detected.\n", me);
            return status;
        case SANE_STATUS_JAMMED:
        case SANE_STATUS_NO_DOCS:
            return status;
        default:
            DBG (DL_MAJOR_ERROR,
                "%s: unhandled request_sense result; trying again.\n",
                me);
            break;
        }
    }

    return status;
}

#define READ_CALIBRATION 0x82
#define NUM_CALIBRATION_LINES 16

static SANE_Status read_calibration_data (SnapScan_Scanner *pss, void *buf, u_char num_lines)
{
    static const char *me = "read_calibration_data";
    SANE_Status status;
    size_t expected_read_bytes = num_lines * calibration_line_length(pss);
    size_t read_bytes;

    DBG (DL_CALL_TRACE, "%s\n", me);
    zero_buf (pss->cmd, MAX_SCSI_CMD_LEN);
    pss->cmd[0] = READ;
    pss->cmd[2] = READ_CALIBRATION;
    pss->cmd[5] = num_lines;
    u_int_to_u_char3p (expected_read_bytes, pss->cmd + 6);
    read_bytes = expected_read_bytes;

    status = snapscan_cmd (pss->pdev->bus, pss->fd, pss->cmd,
                 READ_LEN, buf, &read_bytes);
    CHECK_STATUS (status, me, "snapscan_cmd");

    if(read_bytes != expected_read_bytes) {
    DBG (DL_MAJOR_ERROR, "%s: read %d of %d calibration data\n", me, read_bytes, expected_read_bytes);
    return SANE_STATUS_IO_ERROR;
    }

    return SANE_STATUS_GOOD;
}

static SANE_Status calibrate (SnapScan_Scanner *pss)
{
    int r;
    int c;
    u_char *buf;
    static const char *me = "calibrate";
    SANE_Status status;
    int line_length = calibration_line_length(pss);

    if ((pss->hconfig & HCFG_CAL_ALLOWED) && line_length) {
        int num_lines = pss->phys_buf_sz / line_length;
        if (num_lines > NUM_CALIBRATION_LINES)
        {
            num_lines = NUM_CALIBRATION_LINES;
        } else if (num_lines == 0)
        {
            DBG(DL_MAJOR_ERROR, "%s: scsi request size underflow (< %d bytes)", me, line_length);
            return SANE_STATUS_IO_ERROR;
        }
        buf = (u_char *) malloc(num_lines * line_length);
        if (!buf)
        {
            DBG (DL_MAJOR_ERROR, "%s: out of memory allocating calibration, %d bytes.", me, num_lines * line_length);
            return SANE_STATUS_NO_MEM;
        }

        DBG (DL_MAJOR_ERROR, "%s: reading calibration data (%d lines)\n", me, num_lines);
        status = read_calibration_data(pss, buf, num_lines);
        CHECK_STATUS(status, me, "read_calibration_data");

        for(c=0; c < line_length; c++) {
            u_int sum = 0;
            for(r=0; r < num_lines; r++) {
                sum += buf[c + r * line_length];
            }
            pss->buf[c + SEND_LENGTH] = sum / num_lines;
        }

        status = send (pss, DTC_CALIBRATION, 1);
        CHECK_STATUS(status, me, "send calibration");
        free(buf);
    }
    return SANE_STATUS_GOOD;
}

static SANE_Status download_firmware(SnapScan_Scanner * pss)
{
    static const char *me = "download_firmware";
    unsigned char *pFwBuf, *pCDB;
    char* firmware = NULL;
    FILE *fd;
    size_t bufLength,cdbLength;
    SANE_Status status = SANE_STATUS_GOOD;
    char cModelName[8], cModel[255];
    unsigned char bModelNo;
    int readByte;

    bModelNo =*(pss->buf + INQUIRY_HWMI);
    zero_buf((unsigned char *)cModel, 255);
    sprintf(cModelName, "%d", bModelNo);
    DBG(DL_INFO, "Looking up %s\n", cModelName);
    if (pss->pdev->firmware_filename) {
        firmware = pss->pdev->firmware_filename;
    } else if (default_firmware_filename) {
        firmware = default_firmware_filename;
    } else {
        DBG (0,
            "%s: No firmware entry found in config file %s.\n",
            me,
            SNAPSCAN_CONFIG_FILE
        );
        status = SANE_STATUS_INVAL;
    }
    if (status == SANE_STATUS_GOOD)
    {
        cdbLength = 10;
        DBG(DL_INFO, "Downloading %s\n", firmware);
        fd = fopen(firmware,"r");
        if(fd == NULL)
        {
            DBG (0, "Cannot open firmware file %s.\n", firmware);
            DBG (0, "Edit the firmware file entry in %s.\n", SNAPSCAN_CONFIG_FILE);
            status = SANE_STATUS_INVAL;
        }
        else
        {
            switch (pss->pdev->model)
            {
            case PRISA610:
            case PRISA310:
            case PRISA620:
            case PRISA1240:
            case PRISA640:
            case PRISA4300:
            case PRISA4300_2:
            case PRISA5000:
            case PRISA5300:
                /* ACER firmware files do not contain an info block */
                fseek(fd, 0, SEEK_END);
                bufLength = ftell(fd);
                fseek(fd, 0, SEEK_SET);
                break;
            case PERFECTION1670:
                /* AGFA firmware files contain an info block which
                   specifies the length of the firmware data. The
                   length information is stored at offset 0x64 from
                   end of file */
                {
                    unsigned char size_l, size_h;
                    fseek(fd, -0x64, SEEK_END);
                    fread(&size_l, 1, 1, fd);
                    fread(&size_h, 1, 1, fd);
                    fseek(fd, 0, SEEK_SET);
                    bufLength = (size_h << 8) + size_l;
                }
                break;
            default:
                /* AGFA firmware files contain an info block which
                   specifies the length of the firmware data. The
                   length information is stored at offset 0x5e from
                   end of file */
                {
                    unsigned char size_l, size_h;
                    fseek(fd, -0x5e, SEEK_END);
                    fread(&size_l, 1, 1, fd);
                    fread(&size_h, 1, 1, fd);
                    fseek(fd, 0, SEEK_SET);
                    bufLength = (size_h << 8) + size_l;
                }
                break;
            }

            DBG(DL_INFO, "Size of firmware: %d\n", bufLength);
            pCDB = (unsigned char *)malloc(bufLength + cdbLength);
            pFwBuf = pCDB + cdbLength;
            zero_buf (pCDB, cdbLength);
            readByte = fread(pFwBuf,1,bufLength,fd);

            *pCDB = SEND;
            *(pCDB + 2) = 0x87;
            *(pCDB + 6) = (bufLength >> 16) & 0xff;
            *(pCDB + 7) = (bufLength >> 8) & 0xff;
            *(pCDB + 8) = bufLength  & 0xff;

            status = snapscan_cmd (
                pss->pdev->bus, pss->fd, pCDB, bufLength+cdbLength, NULL, NULL);

            free(pCDB);
            fclose(fd);
        }
    }
    return status;
}

/*
 * $Log$
 * Revision 1.24  2003/10/07 19:41:34  oliver-guest
 * Updates for Epson Perfection 1670
 *
 * Revision 1.23  2003/08/19 21:05:08  oliverschwartz
 * Scanner ID cleanup
 *
 * Revision 1.22  2003/04/30 20:49:39  oliverschwartz
 * SnapScan backend 1.4.26
 *
 * Revision 1.37  2003/04/30 20:42:19  oliverschwartz
 * Added support for Agfa Arcus 1200 (supplied by Valtteri Vuorikoski)
 *
 * Revision 1.36  2003/04/02 21:17:13  oliverschwartz
 * Fix for 1200 DPI with Acer 5000
 *
 * Revision 1.35  2003/02/08 10:45:09  oliverschwartz
 * Use 600 DPI as optical resolution for Benq 5000
 *
 * Revision 1.34  2002/12/10 20:14:12  oliverschwartz
 * Enable color offset correction for SnapScan300
 *
 * Revision 1.33  2002/09/24 16:07:48  oliverschwartz
 * Added support for Benq 5000
 *
 * Revision 1.32  2002/06/06 20:40:01  oliverschwartz
 * Changed default scan area for transparancy unit of SnapScan e50
 *
 * Revision 1.31  2002/05/02 18:28:44  oliverschwartz
 * Added ADF support
 *
 * Revision 1.30  2002/04/27 14:41:22  oliverschwartz
 * Print number of open handles in close_scanner()
 *
 * Revision 1.29  2002/04/10 21:46:48  oliverschwartz
 * Removed illegal character
 *
 * Revision 1.28  2002/04/10 21:01:02  oliverschwartz
 * Disable send_diagnostic() for 1236s
 *
 * Revision 1.27  2002/03/24 12:11:20  oliverschwartz
 * Get name of firmware file in sane_init
 *
 * Revision 1.26  2002/01/23 20:42:41  oliverschwartz
 * Improve recognition of Acer 320U
 * Add sense_handler code for sense code 0x0b
 * Fix for spaces in model strings
 *
 * Revision 1.25  2001/12/12 19:44:59  oliverschwartz
 * Clean up CVS log
 *
 * Revision 1.24  2001/12/09 23:01:00  oliverschwartz
 * - use sense handler for USB
 * - fix scan mode
 *
 * Revision 1.23  2001/12/08 11:53:31  oliverschwartz
 * - Additional logging in sense handler
 * - Fix wait_scanner_ready() if device reports busy
 * - Fix scanning mode (preview/normal)
 *
 * Revision 1.22  2001/11/27 23:16:17  oliverschwartz
 * - Fix color alignment for SnapScan 600
 * - Added documentation in snapscan-sources.c
 * - Guard against TL_X < BR_X and TL_Y < BR_Y
 *
 * Revision 1.21  2001/10/21 08:49:37  oliverschwartz
 * correct number of scan lines for calibration thanks to Mikko Työläjärvi
 *
 * Revision 1.20  2001/10/12 20:54:04  oliverschwartz
 * enable gamma correction for Snapscan 1236, e20 and e50 scanners
 *
 * Revision 1.19  2001/10/11 14:02:10  oliverschwartz
 * Distinguish between e20/e25 and e40/e50
 *
 * Revision 1.18  2001/10/09 22:34:23  oliverschwartz
 * fix compiler warnings
 *
 * Revision 1.17  2001/10/08 19:26:01  oliverschwartz
 * - Disable quality calibration for scanners that do not support it
 *
 * Revision 1.16  2001/10/08 18:22:01  oliverschwartz
 * - Disable quality calibration for Acer Vuego 310F
 * - Use sanei_scsi_max_request_size as scanner buffer size
 *   for SCSI devices
 * - Added new devices to snapscan.desc
 *
 * Revision 1.15  2001/09/18 15:01:07  oliverschwartz
 * - Read scanner id string again after firmware upload
 *   to indentify correct model
 * - Make firmware upload work for AGFA scanners
 * - Change copyright notice
 *
 * Revision 1.14  2001/09/17 10:01:08  sable
 * Added model AGFA 1236U
 *
 * Revision 1.13  2001/09/09 18:06:32  oliverschwartz
 * add changes from Acer (new models; automatic firmware upload for USB scanners); fix distorted colour scans after greyscale scans (call set_window only in sane_start); code cleanup
 *
 * Revision 1.12  2001/04/10 13:00:31  sable
 * Moving sanei_usb_* to snapscani_usb*
 *
 * Revision 1.11  2001/04/10 11:04:31  sable
 * Adding support for snapscan e40 an e50 thanks to Giuseppe Tanzilli
 *
 * Revision 1.10  2001/03/17 22:53:21  sable
 * Applying Mikael Magnusson patch concerning Gamma correction
 * Support for 1212U_2
 *
 * Revision 1.9  2000/11/10 01:01:59  sable
 * USB (kind of) autodetection
 *
 * Revision 1.8  2000/11/01 01:26:43  sable
 * Support for 1212U
 *
 * Revision 1.7  2000/10/30 22:32:20  sable
 * Support for vuego310s vuego610s and 1236s
 *
 * Revision 1.6  2000/10/29 22:44:55  sable
 * Bug correction for 1236s
 *
 * Revision 1.5  2000/10/28 14:16:10  sable
 * Bug correction for SnapScan310
 *
 * Revision 1.4  2000/10/28 14:06:35  sable
 * Add support for Acer300f
 *
 * Revision 1.3  2000/10/15 19:52:06  cbagwell
 * Changed USB support to a 1 line modification instead of multi-file
 * changes.
 *
 * Revision 1.2  2000/10/13 03:50:27  cbagwell
 * Updating to source from SANE 1.0.3.  Calling this versin 1.1
 *
 * */
