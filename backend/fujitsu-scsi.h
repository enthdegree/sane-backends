#ifndef FUJITSU_SCSI_H
#define FUJITSU_SCSI_H

/* 
 * Part of SANE - Scanner Access Now Easy.
 * 
 * Please see to opening comments in fujitsu.c
 */

/****************************************************/

#define USB_COMMAND_CODE   0x43
#define USB_COMMAND_LEN    0x1F
#define USB_COMMAND_OFFSET 0x13
#define USB_COMMAND_TIME   10000
#define USB_DATA_TIME      10000
#define USB_STATUS_CODE    0x53
#define USB_STATUS_LEN     0x0D
#define USB_STATUS_OFFSET  0x09
#define USB_STATUS_TIME    10000

/*static inline void */
static void
setbitfield (unsigned char *pageaddr, int mask, int shift, int val)
{
  *pageaddr = (*pageaddr & ~(mask << shift)) | ((val & mask) << shift);
}

/* ------------------------------------------------------------------------- */

/*static inline void */
/*static  void
resetbitfield (unsigned char *pageaddr, int mask, int shift, int val) \
{
  *pageaddr = (*pageaddr & ~(mask << shift)) | (((!val) & mask) << shift);
}
*/

/* ------------------------------------------------------------------------- */

/*static inline int */
static int
getbitfield (unsigned char *pageaddr, int mask, int shift)
{
  return ((*pageaddr >> shift) & mask);
}

/* ------------------------------------------------------------------------- */

static int
getnbyte (unsigned char *pnt, int nbytes)
{
  unsigned int result = 0;
  int i;

#ifdef DEBUG
  assert (nbytes < 5);
#endif
  for (i = 0; i < nbytes; i++)
    result = (result << 8) | (pnt[i] & 0xff);
  return result;
}

/* ------------------------------------------------------------------------- */

/*static inline void */
static void
putnbyte (unsigned char *pnt, unsigned int value, unsigned int nbytes)
{
  int i;

#ifdef DEBUG
  assert (nbytes < 5);
#endif
  for (i = nbytes - 1; i >= 0; i--)

    {
      pnt[i] = value & 0xff;
      value = value >> 8;
    }
}


/* ==================================================================== */
/* SCSI commands */

typedef struct
{
  unsigned char *cmd;
  int size;
}
scsiblk;

/* ==================================================================== */

#define RESERVE_UNIT            0x16
#define RELEASE_UNIT            0x17
#define INQUIRY                 0x12
#define REQUEST_SENSE           0x03
#define SEND_DIAGNOSTIC         0x1d
#define TEST_UNIT_READY         0x00
#define SET_WINDOW              0x24
#define SET_SUBWINDOW           0xc0
#define OBJECT_POSITION         0x31
#define SEND                    0x2a
#define READ                    0x28
#define MODE_SELECT             0x15
#define MODE_SENSE              0x1a
#define SCAN                    0x1b
#define IMPRINTER               0xc1
#define HW_STATUS               0xc2
#define SCANNER_CONTROL         0xf1


/* ==================================================================== */
/*
static unsigned char reserve_unitC[] =
  { RESERVE_UNIT, 0x00, 0x00, 0x00, 0x00, 0x00 };
static scsiblk reserve_unitB = { reserve_unitC, sizeof (reserve_unitC) };

static unsigned char release_unitC[] =
  { RELEASE_UNIT, 0x00, 0x00, 0x00, 0x00, 0x00 };
static scsiblk release_unitB = { release_unitC, sizeof (release_unitC) };
*/
/* ==================================================================== */

static unsigned char scanner_controlC[] =
  { SCANNER_CONTROL, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static scsiblk scanner_controlB = { scanner_controlC, sizeof (scanner_controlC) };

#define set_SC_function(icb, val)              setbitfield(icb + 1, 7, 0, val)
#define SC_function_cancel                     0x04
#define SC_function_lamp_on                    0x05
#define SC_function_lamp_off                   0x03
#define SC_function_lamp_normal                0x06
#define SC_function_lamp_saving                0x07

/* ==================================================================== */

static unsigned char inquiryC[] = { INQUIRY, 0x00, 0x00, 0x00, 0x1f, 0x00 };
static scsiblk inquiryB = { inquiryC, sizeof (inquiryC) };

#define set_IN_evpd(icb, val)              setbitfield(icb + 1, 1, 0, val)
#define set_IN_page_code(icb, val)         icb[0x02]=val
#define set_IN_return_size(icb,val)        icb[0x04]=val
#define set_IN_length(out,n)               out[0x04]=n-5

#define get_IN_periph_qual(in)             getbitfield(in, 0x07, 5)
#define IN_periph_qual_lun                    0x00
#define IN_periph_qual_nolun                  0x03
#define get_IN_periph_devtype(in)          getbitfield(in, 0x1f, 0)
#define IN_periph_devtype_scanner             0x06
#define IN_periph_devtype_unknown             0x1f
#define get_IN_response_format(in)         getbitfield(in + 0x03, 0x07, 0)
#define IN_recognized                         0x02
#define get_IN_vendor(in, buf)             strncpy(buf, (char *)in + 0x08, 0x08)
#define get_IN_product(in, buf)            strncpy(buf, (char *)in + 0x10, 0x010)
#define get_IN_version(in, buf)            strncpy(buf, (char *)in + 0x20, 0x04)
#define get_IN_color_offset(in)            getnbyte (in+0x2A, 2) /* offset between colors */

/* these only in some scanners */
#define get_IN_long_color(in)              getbitfield(in+0x2C, 1, 0)
#define get_IN_long_gray(in)               getbitfield(in+0x2C, 1, 1)

#define get_IN_duplex_3091(in)             getbitfield(in+0x2D, 1, 0)
#define get_IN_bg_front(in)                getbitfield(in+0x2D, 1, 2)
#define get_IN_bg_back(in)                 getbitfield(in+0x2D, 1, 3)
#define get_IN_emulation(in)               getbitfield(in+0x2D, 1, 6)

#define get_IN_duplex_offset(in)           getnbyte (in+0x2E, 2)

/* the VPD response */
#define get_IN_page_length(in)             in[0x04]
#define get_IN_basic_x_res(in)             getnbyte(in + 0x05, 2)
#define get_IN_basic_y_res(in)             getnbyte(in + 0x07, 2)
#define get_IN_step_x_res(in)              getbitfield(in+0x09, 1, 0)
#define get_IN_step_y_res(in)              getbitfield(in+0x09, 1, 4)
#define get_IN_max_x_res(in)               getnbyte(in + 0x0a, 2)
#define get_IN_max_y_res(in)               getnbyte(in + 0x0c, 2)
#define get_IN_min_x_res(in)               getnbyte(in + 0x0e, 2)
#define get_IN_min_y_res(in)               getnbyte(in + 0x10, 2)
#define get_IN_std_res_200(in)             getbitfield(in+ 0x12, 1, 0)
#define get_IN_std_res_180(in)             getbitfield(in+ 0x12, 1, 1)
#define get_IN_std_res_160(in)             getbitfield(in+ 0x12, 1, 2)
#define get_IN_std_res_150(in)             getbitfield(in+ 0x12, 1, 3)
#define get_IN_std_res_120(in)             getbitfield(in+ 0x12, 1, 4)
#define get_IN_std_res_100(in)             getbitfield(in+ 0x12, 1, 5)
#define get_IN_std_res_75(in)              getbitfield(in+ 0x12, 1, 6)
#define get_IN_std_res_60(in)              getbitfield(in+ 0x12, 1, 7)
#define get_IN_std_res_1200(in)            getbitfield(in+ 0x13, 1, 0)
#define get_IN_std_res_800(in)             getbitfield(in+ 0x13, 1, 1)
#define get_IN_std_res_600(in)             getbitfield(in+ 0x13, 1, 2)
#define get_IN_std_res_480(in)             getbitfield(in+ 0x13, 1, 3)
#define get_IN_std_res_400(in)             getbitfield(in+ 0x13, 1, 4)
#define get_IN_std_res_320(in)             getbitfield(in+ 0x13, 1, 5)
#define get_IN_std_res_300(in)             getbitfield(in+ 0x13, 1, 6)
#define get_IN_std_res_240(in)             getbitfield(in+ 0x13, 1, 7)
#define get_IN_window_width(in)            getnbyte(in + 0x14, 4)
#define get_IN_window_length(in)           getnbyte(in + 0x18, 4)
#define get_IN_overflow(in)                getbitfield(in+0x1c, 1, 0)
#define get_IN_monochrome(in)              getbitfield(in+0x1c, 1, 1)
#define get_IN_half_tone(in)               getbitfield(in+0x1c, 1, 2)
#define get_IN_multilevel(in)              getbitfield(in+0x1c, 1, 3)
#define get_IN_monochrome_rgb(in)          getbitfield(in+0x1c, 1, 5)
#define get_IN_half_tone_rgb(in)           getbitfield(in+0x1c, 1, 6)
#define get_IN_multilevel_rgb(in)          getbitfield(in+0x1c, 1, 7)

/* vendor unique section */
#define get_IN_operator_panel(in)          getbitfield(in+0x20, 1, 1)
#define get_IN_barcode(in)                 getbitfield(in+0x20, 1, 2)
#define get_IN_imprinter(in)               getbitfield(in+0x20, 1, 3)
#define get_IN_duplex(in)                  getbitfield(in+0x20, 1, 4)
#define get_IN_transparency(in)            getbitfield(in+0x20, 1, 5)
#define get_IN_flatbed(in)                 getbitfield(in+0x20, 1, 6)
#define get_IN_adf(in)                     getbitfield(in+0x20, 1, 7)

#define get_IN_adbits(in)                  getbitfield(in+0x21, 0x0f, 0)
#define get_IN_buffer_bytes(in)            getnbyte(in + 0x22, 4)

/* more stuff here (std supported commands) */
#define get_IN_has_cmd_msen(in)            getbitfield(in+0x29, 1, 7)

#define get_IN_has_subwindow(in)           getbitfield(in+0x2b, 1, 0) 
#define get_IN_has_endorser(in)            getbitfield(in+0x2b, 1, 1)
#define get_IN_has_hw_status(in)           getbitfield(in+0x2b, 1, 2)
#define get_IN_has_scanner_ctl(in)         getbitfield(in+0x31, 1, 1)

#define get_IN_brightness_steps(in)        getnbyte(in+0x52, 1)
#define get_IN_threshold_steps(in)         getnbyte(in+0x53, 1)
#define get_IN_contrast_steps(in)          getnbyte(in+0x54, 1)

#define get_IN_num_dither_internal(in)     getbitfield(in+0x56, 15, 4)
#define get_IN_num_dither_download(in)     getbitfield(in+0x56, 15, 0)

#define get_IN_num_gamma_internal(in)      getbitfield(in+0x57, 15, 4)
#define get_IN_num_gamma_download(in)      getbitfield(in+0x57, 15, 0)

#define get_IN_ipc_bw_rif(in)          getbitfield(in+0x58, 1, 7)
#define get_IN_ipc_auto1(in)               getbitfield(in+0x58, 1, 6)
#define get_IN_ipc_auto2(in)               getbitfield(in+0x58, 1, 5)
#define get_IN_ipc_outline_extraction(in)  getbitfield(in+0x58, 1, 4)
#define get_IN_ipc_image_emphasis(in)      getbitfield(in+0x58, 1, 3)
#define get_IN_ipc_auto_separation(in)     getbitfield(in+0x58, 1, 2)
#define get_IN_ipc_mirroring(in)           getbitfield(in+0x58, 1, 1)
#define get_IN_ipc_white_level_follow(in)  getbitfield(in+0x58, 1, 0)

#define get_IN_ipc_subwindow(in)           getbitfield(in+0x59, 1, 7)
#define get_IN_ipc_error_diffusion(in)     getbitfield(in+0x59, 1, 6)

#define get_IN_compression_MH(in)          getbitfield(in+0x5a, 1, 7)
#define get_IN_compression_MR(in)          getbitfield(in+0x5a, 1, 6)
#define get_IN_compression_MMR(in)         getbitfield(in+0x5a, 1, 5)
#define get_IN_compression_JBIG(in)        getbitfield(in+0x5a, 1, 4)
#define get_IN_compression_JPG_BASE(in)    getbitfield(in+0x5a, 1, 3)
#define get_IN_compression_JPG_EXT(in)     getbitfield(in+0x5a, 1, 2)
#define get_IN_compression_JPG_INDEP(in)   getbitfield(in+0x5a, 1, 1)

#define get_IN_imprinter_mechanical(in)    getbitfield(in+0x5c, 1, 7)
#define get_IN_imprinter_stamp(in)         getbitfield(in+0x5c, 1, 6)
#define get_IN_imprinter_electrical(in)    getbitfield(in+0x5c, 1, 5)
#define get_IN_imprinter_max_id(in)        getbitfield(in+0x5c, 0x0f, 0)

#define get_IN_imprinter_size(in)          getbitfield(in+0x5d, 3, 0)

#define get_IN_connection(in)       getbitfield(in+0x62, 3, 0)

#define get_IN_x_overscan_size(in)  getnbyte(in + 0x64, 2)
#define get_IN_y_overscan_size(in)  getnbyte(in + 0x66, 2)

/* = some scanners need inquiry data manipulated ====================== */
#define set_IN_page_length(in,val)             in[0x04]=val
/* ==================================================================== */

static unsigned char test_unit_readyC[] =
  { TEST_UNIT_READY, 0x00, 0x00, 0x00, 0x00, 0x00 };
static scsiblk test_unit_readyB =
  { test_unit_readyC, sizeof (test_unit_readyC) };

/* ==================================================================== */

static unsigned char set_windowC[] =
  { SET_WINDOW, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, };
/* opcode,  lun,  _____4 X reserved____,  transfer length, control byte */
static scsiblk set_windowB = { set_windowC, sizeof (set_windowC) };
#define set_SW_xferlen(sb, len) putnbyte(sb + 0x06, len, 3)

/* ==================================================================== */

static unsigned char object_positionC[] =
  { OBJECT_POSITION, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
/* ADF, _____Count_____,  ________Reserved______, Ctl */
static scsiblk object_positionB =
  { object_positionC, sizeof (object_positionC) };

#define set_OP_autofeed(b,val) setbitfield(b+0x01, 0x07, 0, val)
#define OP_Discharge	0x00
#define OP_Feed	0x01

/* ==================================================================== */


static unsigned char sendC[] =
  {SEND, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static scsiblk sendB = {sendC, sizeof (sendC)};

#define set_S_xfer_datatype(sb, val) sb[0x02] = (unsigned char)val
/*#define S_datatype_imagedatai		0x00
#define S_datatype_halftone_mask        0x02
#define S_datatype_gamma_function       0x03*/
#define S_datatype_lut_data             0x83
/*#define S_datatype_jpg_q_table          0x88
#define S_datatype_imprinter_data       0x90
#define S_EX_datatype_lut		0x01
#define S_EX_datatype_shading_data	0xa0
#define S_user_reg_gamma		0xc0
#define S_device_internal_info		0x03
#define set_S_datatype_qual_upper(sb, val) sb[0x04] = (unsigned char)val
#define S_DQ_none	0x00
#define S_DQ_Rcomp	0x06
#define S_DQ_Gcomp	0x07
#define S_DQ_Bcomp	0x08
#define S_DQ_Reg1	0x01
#define S_DQ_Reg2	0x02
#define S_DQ_Reg3	0x03*/
#define set_S_xfer_id(sb, val)    putnbyte(sb + 4, val, 2)
#define set_S_xfer_length(sb, val)    putnbyte(sb + 6, val, 3)

static unsigned char send_lutC[1034];
#define set_S_lut_order(sb, val)    putnbyte(sb + 2, val, 1)
#define S_lut_order_single 0x10
#define set_S_lut_ssize(sb, val)    putnbyte(sb + 4, val, 2)
#define set_S_lut_dsize(sb, val)    putnbyte(sb + 6, val, 2)
#define S_lut_data_offset 0x0a

/*
static unsigned char send_imprinterC[] = 
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
   0x00, 0x00};
static scsiblk send_imprinterB = 
  {send_imprinterC, sizeof(send_imprinterC)};
#define set_imprinter_cnt_dir(sb, val) setbitfield(sb + 0x01, 1, 5, val)
#define S_im_dir_inc 0
#define S_im_dir_dec 1
#define set_imprinter_lap24(sb, val) setbitfield(sb + 0x01, 1, 4, val)
#define S_im_ctr_24bit 1
#define S_im_ctr_16bit 0
#define set_imprinter_cstep(sb, val) setbitfield(sb + 0x01, 0x03, 0, val)
#define set_imprinter_uly(sb, val) putnbyte(sb + 0x06, val, 4)
#define set_imprinter_dirs(sb, val) setbitfield(sb + 0x0c, 0x03, 0, val)
#define S_im_dir_left_right 0
#define S_im_dir_top_bottom 1
#define S_im_dir_right_left 2
#define S_im_dir_bottom_top 3
#define set_imprinter_string_length(sb, len)  putnbyte(sb + 0x11, len, 1)
#define max_imprinter_string_length 40
*/

/*
static unsigned char gamma_user_LUT_LS1K[512] = { 0x00 };
static scsiblk gamma_user_LUT_LS1K_LS1K = 
  { gamma_user_LUT_LS1K, sizeof(gamma_user_LUT_LS1K) };
*/

/* ==================================================================== */
/*
static unsigned char imprinterC[] =
  { IMPRINTER, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static scsiblk imprinterB = { imprinterC, sizeof (imprinterC) };

#define set_IM_xfer_length(sb, val) putnbyte(sb + 0x7, val, 2)

static unsigned char imprinter_descC[] = 
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static scsiblk imprinter_descB = {imprinter_descC, sizeof(imprinter_descC) };
*/
/* enable/disable imprinter printing*/
#define set_IMD_enable(sb, val) setbitfield(sb + 0x01, 1, 7, val)
#define IMD_enable 0
#define IMD_disable 1
/* specifies thes side of a document to be printed */
#define set_IMD_side(sb, val) setbitfield(sb + 0x01, 1, 6, val)
#define IMD_front 0
#define IMD_back 1
/* format of the counter 16/24 bit*/
#define set_IMD_format(sb, val) setbitfield(sb + 0x01, 1, 5, val)
#define IMD_16_bit 0
#define IMD_24_bit 1
/* initial count */
#define set_IMD_initial_count_16(sb, val) putnbyte(sb + 0x02, val, 2)
#define set_IMD_initial_count_24(sb, val) putnbyte(sb + 0x03, val, 3)

/* ==================================================================== */

static unsigned char readC[] =
  { READ, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
/* Type, rsvd, type qual, __xfer length__, Ctl */
static scsiblk readB = { readC, sizeof (readC) };

#define set_R_datatype_code(sb, val) sb[0x02] = val
#define R_datatype_imagedata		0x00
#define R_datatype_pixelsize		0x80
#define set_R_xfer_length(sb, val)     putnbyte(sb + 0x06, val, 3)
#define set_R_window_id(sb, val)       sb[0x05] = val
#define set_R_xfer_length(sb, val)     putnbyte(sb + 0x06, val, 3)
#define get_PSIZE_num_x(in)            getnbyte(in + 0x00, 4)
#define get_PSIZE_num_y(in)            getnbyte(in + 0x04, 4)
#define get_PSIZE_paper_w(in)            getnbyte(in + 0x08, 4)
#define get_PSIZE_paper_l(in)            getnbyte(in + 0x0C, 4)

/* ==================================================================== */

/* page codes used by mode_sense and mode_select */
#define MS_pc_color   0x32 /* color interlacing mode? */
#define MS_pc_prepick 0x33 /* Prepick next adf page */
#define MS_pc_sleep   0x34 /* Sleep mode */
#define MS_pc_duplex  0x35 /* ADF duplex transfer mode */
#define MS_pc_rand    0x36 /* All sorts of device controls */
#define MS_pc_bg      0x37 /* Backing switch control */
#define MS_pc_df      0x38 /* Double feed detection */
#define MS_pc_dropout 0x39 /* Drop out color */
#define MS_pc_buff    0x3a /* Scan buffer control */
#define MS_pc_auto    0x3c /* Auto paper size detection */
#define MS_pc_lamp    0x3d /* Lamp light timer set */
#define MS_pc_jobsep  0x3e /* Detect job separation sheet */
#define MS_pc_all     0x3f /* Only used with mode_sense */

/* ==================================================================== */

static unsigned char mode_senseC[] =
  { MODE_SENSE, 0x00, 0x00, 0x00, 0x00, 0x00 };
static scsiblk mode_senseB = { mode_senseC, sizeof (mode_senseC) };
#define set_MSEN_DBD(b, val)    setbitfield(b, 0x01, 3, (val?1:0))
#define set_MSEN_pc(sb, val)    setbitfield(sb + 0x02, 0x3f, 0, val)
#define set_MSEN_xfer_length(sb, val) sb[0x04] = (unsigned char)val
#define get_MSEN_MUD(b)		getnbyte(b+(0x04+((int)*(b+0x3)))+0x4,2)

/* ==================================================================== */

static unsigned char mode_selectC[] =
  { MODE_SELECT, 0x10, 0x00, 0x00, 0x00, 0x00 };
static scsiblk mode_selectB = { mode_selectC, sizeof (mode_selectC) };
#define set_MSEL_xfer_length(sb, val) sb[0x04] = (unsigned char)val

/* following are combined 4 byte header and 8 or 10 byte page 
 * there is also 'descriptor block' & 'vendor-specific block'
 * but fujitsu seems not to use these */

/* 8 byte page used by all pages except dropout? */
static unsigned char mode_select_8byteC[] = {
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static scsiblk mode_select_8byteB = {
  mode_select_8byteC, sizeof (mode_select_8byteC)
};

/* 10 byte page only used by dropout? */
static unsigned char mode_select_10byteC[] = {
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static scsiblk mode_select_10byteB = {
  mode_select_10byteC, sizeof (mode_select_10byteC)
};

#define set_MSEL_pc(sb, val) sb[0x04]=val

#define set_MSEL_sleep_mode(sb, val) sb[0x06]=val

#define set_MSEL_transfer_mode(sb, val) setbitfield(sb + 0x02, 0x01, 0, val)

#define set_MSEL_bg_enable(sb, val) setbitfield(sb + 6, 1, 7, val)
#define set_MSEL_bg_front(sb, val) setbitfield(sb + 6, 1, 5, val)
#define set_MSEL_bg_back(sb, val) setbitfield(sb + 6, 1, 4, val)
#define set_MSEL_bg_fb(sb, val) setbitfield(sb + 6, 1, 3, val)

#define set_MSEL_df_enable(sb, val) setbitfield(sb + 6, 1, 7, val)
#define set_MSEL_df_continue(sb, val) setbitfield(sb + 6, 1, 6, val)
#define set_MSEL_df_thickness(sb, val) setbitfield(sb + 6, 1, 4, val)
#define set_MSEL_df_length(sb, val) setbitfield(sb + 6, 1, 3, val)
#define set_MSEL_df_diff(sb, val) setbitfield(sb + 6, 3, 0, val)
#define MSEL_df_diff_DEFAULT 0 
#define MSEL_df_diff_10MM 1
#define MSEL_df_diff_15MM 2
#define MSEL_df_diff_20MM 3

#define set_MSEL_dropout_front(sb, val) setbitfield(sb + 0x06, 0x0f, 0, val)
#define set_MSEL_dropout_back(sb, val) setbitfield(sb + 0x06, 0x0f, 4, val)
#define MSEL_dropout_DEFAULT 0
#define MSEL_dropout_GREEN   8
#define MSEL_dropout_RED     9
#define MSEL_dropout_BLUE    11
#define MSEL_dropout_CUSTOM  12

#define set_MSEL_buff_mode(sb, val) setbitfield(sb + 0x06, 0x03, 6, val)

#define set_MSEL_prepick(sb, val) setbitfield(sb + 0x06, 0x03, 6, val)

#define set_MSEL_overscan(sb, val) setbitfield(sb + 0x09, 0x03, 6, val)

/*buffer, prepick, overscan use these*/
#define MSEL_DEFAULT 0
#define MSEL_OFF 2
#define MSEL_ON 3

/* ==================================================================== */

static unsigned char scanC[] = { SCAN, 0x00, 0x00, 0x00, 0x00, 0x00 };
static scsiblk scanB = { scanC, sizeof (scanC) };

#define set_SC_xfer_length(sb, val) sb[0x04] = (unsigned char)val

/* ==================================================================== */

static unsigned char hw_statusC[] =
  { HW_STATUS, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static scsiblk hw_statusB = { hw_statusC, sizeof (hw_statusC) };

#define set_HW_allocation_length(sb, len) putnbyte(sb + 0x07, len, 2)

#define get_HW_top(in)             getbitfield(in+0x02, 1, 7)
#define get_HW_A3(in)              getbitfield(in+0x02, 1, 3)
#define get_HW_B4(in)              getbitfield(in+0x02, 1, 2)
#define get_HW_A4(in)              getbitfield(in+0x02, 1, 1)
#define get_HW_B5(in)              getbitfield(in+0x02, 1, 0)

#define get_HW_hopper(in)          !getbitfield(in+0x03, 1, 7)
#define get_HW_omr(in)             getbitfield(in+0x03, 1, 6)
#define get_HW_adf_open(in)        getbitfield(in+0x03, 1, 5)

#define get_HW_sleep(in)           getbitfield(in+0x04, 1, 7)
#define get_HW_send_sw(in)         getbitfield(in+0x04, 1, 2)
#define get_HW_manual_feed(in)     getbitfield(in+0x04, 1, 1)
#define get_HW_scan_sw(in)         getbitfield(in+0x04, 1, 0)

#define get_HW_function(in)        getbitfield(in+0x05, 0x0f, 0)

#define get_HW_ink_empty(in)       getbitfield(in+0x06, 1, 7)
#define get_HW_double_feed(in)     getbitfield(in+0x06, 1, 0)

#define get_HW_error_code(in)      in[0x07]

#define get_HW_skew_angle(in)      getnbyte(in+0x08, 2)

#define get_HW_ink_remain(in)      in[0x0a]

/* ==================================================================== */

/* We use the same structure for both SET WINDOW and GET WINDOW. */
static unsigned char window_descriptor_headerC[] = {
  0x00, 0x00, 0x00,
  0x00, 0x00, 0x00,		/* reserved */
  0x00, 0x00,			/* Window Descriptor Length */
};
static scsiblk window_descriptor_headerB=
  { window_descriptor_headerC, sizeof (window_descriptor_headerC) };
#define set_WPDB_wdblen(sb, len) putnbyte(sb + 0x06, len, 2)

/* ==================================================================== */

static unsigned char window_descriptor_blockC[] = {
  /* 0x00 - Window Identifier
   *        0x00 for 3096
   *        0x00 (front) or 0x80 (back) for 3091
   */
  0x00,
#define set_WD_wid(sb, val) sb[0] = val
#define WD_wid_front 0x00
#define WD_wid_back 0x80

  /* 0x01 - Reserved (bits 7-1), AUTO (bit 0)
   *        Use 0x00 for 3091, 3096
   */
  0x00,
#define set_WD_auto(sb, val) setbitfield(sb + 0x01, 1, 0, val)
#define get_WD_auto(sb)	getbitfield(sb + 0x01, 1, 0)

  /* 0x02,0x03 - X resolution in dpi
   *        3091 supports 50-300 in steps of 1
   *        3096 suppors 200,240,300,400; or 100-1600 in steps of 4
   *             if image processiong option installed
   */
  0x00, 0x00,
#define set_WD_Xres(sb, val) putnbyte(sb + 0x02, val, 2)
#define get_WD_Xres(sb)	getnbyte(sb + 0x02, 2)

  /* 0x04,0x05 - X resolution in dpi
   *        3091 supports 50-600 in steps of 1; 75,150,300,600 only
   *             in color mode
   *        3096 suppors 200,240,300,400; or 100-1600 in steps of 4
   *             if image processiong option installed
   */
  0x00, 0x00,
#define set_WD_Yres(sb, val) putnbyte(sb + 0x04, val, 2)
#define get_WD_Yres(sb)	getnbyte(sb + 0x04, 2)

  /* 0x06-0x09 - Upper Left X in 1/1200 inch
   */
  0x00, 0x00, 0x00, 0x00,
#define set_WD_ULX(sb, val) putnbyte(sb + 0x06, val, 4)
#define get_WD_ULX(sb) getnbyte(sb + 0x06, 4)

  /* 0x0a-0x0d - Upper Left Y in 1/1200 inch
   */
  0x00, 0x00, 0x00, 0x00,
#define set_WD_ULY(sb, val) putnbyte(sb + 0x0a, val, 4)
#define get_WD_ULY(sb) getnbyte(sb + 0x0a, 4)

  /* 0x0e-0x11 - Width in 1/1200 inch
   *        3091 left+width max 10200
   *        3096 left+width max 14592
   *        also limited to page size, see bytes 0x35ff.
   */
  0x00, 0x00, 0x00, 0x00,
#define set_WD_width(sb, val) putnbyte(sb + 0x0e, val, 4)
#define get_WD_width(sb) getnbyte(sb + 0x0e, 4)

  /* 0x12-0x15 - Height in 1/1200 inch
   *        3091 top+height max 16832
   *        3096 top+height max 20736, also if left+width>13199,
   *             top+height has to be less than 19843
   */
  0x00, 0x00, 0x00, 0x00,
#define set_WD_length(sb, val) putnbyte(sb + 0x12, val, 4)
#define get_WD_length(sb) getnbyte(sb + 0x12, 4)

  /* 0x16 - Brightness
   *        3091 always use 0x00
   *        3096 if in halftone mode, 8 levels supported (01-1F, 20-3F,
   ..., E0-FF)
   *             use 0x00 for user defined dither pattern
   */
  0x00,
#define set_WD_brightness(sb, val) sb[0x16] = val
#define get_WD_brightness(sb)  sb[0x16]

  /* 0x17 - Threshold
   *        3091 0x00 = use floating slice; 0x01..0xff fixed slice
   *             with 0x01=brightest, 0x80=medium, 0xff=darkest;
   *             only effective for line art mode.
   *        3096 0x00 = use "simplified dynamic treshold", otherwise
   *             same as above but resolution is only 64 steps.
   */
  0x00,
#define set_WD_threshold(sb, val) sb[0x17] = val
#define get_WD_threshold(sb)  sb[0x17]

  /* 0x18 - Contrast
   *        3091 - not supported, always use 0x00
   *        3096 - the same
   */
  0x00,
#define set_WD_contrast(sb, val) sb[0x18] = val
#define get_WD_contrast(sb) sb[0x18]

  /* 0x19 - Image Composition (color mode)
   *        3091 - use 0x00 for line art, 0x01 for halftone, 
   *               0x02 for grayscale, 0x05 for color.
   *        3096 - same but minus color.
   */
  0x00,
#define set_WD_composition(sb, val)  sb[0x19] = val
#define get_WD_composition(sb) sb[0x19]
#define WD_comp_LA 0
#define WD_comp_HT 1
#define WD_comp_GS 2
#define WD_comp_CL 3
#define WD_comp_CH 4
#define WD_comp_CG 5

  /* 0x1a - Depth
   *        3091 - use 0x01 for b/w or 0x08 for gray/color
   *        3096 - use 0x01 for b/w or 0x08 for gray
   */
  0x08,
#define set_WD_bitsperpixel(sb, val) sb[0x1a] = val
#define get_WD_bitsperpixel(sb)	sb[0x1a]

  /* 0x1b,0x1c - Halftone Pattern
   *        3091 byte 1b: 00h default(=dither), 01h dither, 
   *                      02h error dispersion
   *                  1c: 00 dark images, 01h dark text+images, 
   *                      02h light images,
   *                      03h light text+images, 80h download pattern
   *        3096: 1b unused; 1c bit 7=1: use downloadable pattern,
   *              bit 7=0: use builtin pattern; rest of byte 1b denotes
   *              pattern number, three builtin and five downloadable
   *              supported; higher numbers = error.
   */
  0x00, 0x00,
#define set_WD_halftone(sb, val) putnbyte(sb + 0x1b, val, 2)
#define get_WD_halftone(sb)	getnbyte(sb + 0x1b, 2)

  /* 0x1d - Reverse image, padding type
   *        3091: bit 7=1: reverse black&white
   *              bits 0-2: padding type, must be 0
   *        3096: the same; bit 7 must be set for gray and not 
   *              set for b/w. 
   */
  0x00,
#define set_WD_rif(sb, val) setbitfield(sb + 0x1d, 1, 7, val)
#define get_WD_rif(sb)	getbitfield(sb + 0x1d, 1, 7)

  /* 0x1e,0x1f - Bit ordering
   *        3091 not supported, use 0x00
   *        3096 not supported, use 0x00
   */
  0x00, 0x00,			/* 0x1e *//* bit ordering */
#define set_WD_bitorder(sb, val) putnbyte(sb + 0x1e, val, 2)
#define get_WD_bitorder(sb)	getnbyte(sb + 0x1e, 2)

  /* 0x20 - compression type
   *          not supported on smaller models, use 0x00
   */
  0x00,
#define set_WD_compress_type(sb, val)  sb[0x20] = val
#define get_WD_compress_type(sb) sb[0x20]
#define WD_cmp_NONE 0
#define WD_cmp_MH   1
#define WD_cmp_MR   2
#define WD_cmp_MMR  3
#define WD_cmp_JBIG 0x80
#define WD_cmp_JPG1 0x81
#define WD_cmp_JPG2 0x82
#define WD_cmp_JPG3 0x83


  /* 0x21 - compression argument
   *          specify "k" parameter with MR compress,
   *          or with JPEG- Q param, 0-7
   */
  0x00,
#define set_WD_compress_arg(sb, val)  sb[0x21] = val
#define get_WD_compress_arg(sb) sb[0x21]

  /* 0x22-0x27 - reserved */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

  /* 0x28 - vendor id code
   *        3091 - use 0xc0
   *        3096 - use 0xc0
   */
  0x00,
#define set_WD_vendor_id_code(sb, val)  sb[0x28] = val
#define get_WD_vendor_id_code(sb) sb[0x28]

  /* 0x29 - pattern setting
   *        3091 - use 0x00
   *        3096 - reserved, use 0x00
   */
  0x00,
#define set_WD_gamma(sb, val)  sb[0x29] = val
#define get_WD_gamma(sb) sb[0x29]
#define WD_gamma_DEFAULT 0
#define WD_gamma_NORMAL  1
#define WD_gamma_SOFT    2
#define WD_gamma_SHARP   3

  /* 0x2a - outline/scanning order
   *        3091 - scanning order. Only 0x00 (line order) supported
   *        3096 - outlining. 0x00=off, 0x80=on. 0x80 only permitted
   *               when image processing option fitted.
   */
  0x00,
#define set_WD_outline(sb, val)  sb[0x2a] = val
#define get_WD_outline(sb) sb[0x2a]
#define set_WD_scanning_order(sb, val)  sb[0x2a] = val
#define get_WD_scanning_order(sb) sb[0x2a]

  /* 0x2b - emphasis/scanning order argument
   *        3091 - scanning order argument. Only 0x00 (RGB) supported
   *        3096 - emphasis. 0x00=off, others only permitted when 
   *               image processing option fitted: 
   *                 0x2F=low emphasis, 0x4F=medium emphasis, 
   *                 0x7F=high emphasis, 0xFF=smoothing
   */
  0x00,
#define set_WD_emphasis(sb, val)  sb[0x2b] = val
#define get_WD_emphasis(sb) sb[0x2b]
#define WD_emphasis_NONE    0x00
#define WD_emphasis_LOW     0x01
#define WD_emphasis_MEDIUM  0x30
#define WD_emphasis_HIGH    0x50
#define WD_emphasis_SMOOTH  0x80

  /* 0x2c - auto separation
   *        3091 - reserved, use 0x00
   *        3096 - auto separation. 0x00=off, 0x80=on. 0x80 only 
   *               permitted when image processing option fitted.
   */
  0x00,
#define set_WD_auto_sep(sb, val)  setbitfield(sb + 0x2c, 1, 7, val)
#define get_WD_auto_sep(sb) getbitfield(sb + 0x2c, 1, 7)

  /* 0x2d - mirroring/single color
   *        3091 - determines which color is used in monochrome
   *               scans: 0x00/0x04=G,0x01=B,0x02=R
   *        3096 - window mirroring. 0x00=off, 0x80=on. 0x80 only 
   *               permitted when image processing option fitted.
   */
  0x00,
#define set_WD_mirroring(sb, val)  setbitfield(sb + 0x2d, 1, 7, val)
#define get_WD_mirroring(sb) getbitfield(sb + 0x2d, 1, 7)
#define set_WD_lamp_color(sb, val)  sb[0x2d] = val
#define get_WD_lamp_color(sb) sb[0x2d]
#define WD_LAMP_DEFAULT 0x00
#define WD_LAMP_BLUE 0x01
#define WD_LAMP_RED 0x02
#define WD_LAMP_GREEN 0x04

  /* 0x2e - variance/bit padding
   *        3091 - unsupported, use 0x00
   *        3096 - variance rate for dynamic treshold. 0x00=default,
   *               0x1f, 0x3f, ... 0xff = small...large
   */
  0x00,
#define set_WD_var_rate_dyn_thresh(sb, val)  sb[0x2e] = val
#define get_WD_var_rate_dyn_thresh(sb) sb[0x2e]

  0x00,				/* 0x2f *//* DTC mode */
#define set_WD_dtc_threshold_curve(sb, val) setbitfield(sb + 0x2f, 7, 0, val)
#define get_WD_dtc_threshold_curve(sb) getbitfield(sb + 0x2f, 7, 0)
#define set_WD_gradation(sb, val) setbitfield(sb + 0x2f, 3, 3, val)
#define get_WD_gradation(sb) getbitfield(sb + 0x2f, 3, 3)
#define WD_gradation_ORDINARY 0
#define WD_gradation_HIGH     2
#define set_WD_smoothing_mode(sb, val) setbitfield(sb + 0x2f, 3, 5, val)
#define get_WD_smoothing_mode(sb) getbitfield(sb + 0x2f, 3, 5)
#define WD_smoothing_OCR    0
#define WD_smoothing_IMAGE  1
#define set_WD_filtering(sb, val) setbitfield(sb + 0x2f, 1, 7, val)
#define get_WD_filtering(sb) getbitfield(sb + 0x2f, 1, 7)
#define WD_filtering_BALLPOINT 0
#define WD_filtering_ORDINARY  1

  0x00,				/* 0x30 *//* DTC mode 2 */
#define set_WD_background(sb, val) setbitfield(sb + 0x30, 1, 0, val)
#define get_WD_background(sb) getbitfield(sb + 0x30, 1, 0)
#define WD_background_WHITE  0
#define WD_background_BLACK  1
#define set_WD_matrix2x2(sb, val) setbitfield(sb + 0x30, 1, 1, val)
#define get_WD_matrix2x2(sb) getbitfield(sb + 0x30, 1, 1)
#define set_WD_matrix3x3(sb, val) setbitfield(sb + 0x30, 1, 2, val)
#define get_WD_matrix3x3(sb) getbitfield(sb + 0x30, 1, 2)
#define set_WD_matrix4x4(sb, val) setbitfield(sb + 0x30, 1, 3, val)
#define get_WD_matrix4x4(sb) getbitfield(sb + 0x30, 1, 3)
#define set_WD_matrix5x5(sb, val) setbitfield(sb + 0x30, 1, 4, val)
#define get_WD_matrix5x5(sb) getbitfield(sb + 0x30, 1, 4)
#define set_WD_noise_removal(sb, val) setbitfield(sb + 0x30, 1, 5, !val)
#define get_WD_noise_removal(sb) !getbitfield(sb + 0x30, 1, 5)

  0x00,				/* 0x31 *//* reserved */


  /* 0x32 - scanning mode/white level follower
   *        3091 - scan mode 0x00=normal, 0x02=high quality
   *        3096 - white level follower, 0x00=default,
   *               0x80 enable (line mode), 0xc0 disable (photo mode)
   */
  0x00,
#define set_WD_white_level_follow(sb, val)  sb[0x32] = val
#define get_WD_white_level_follow(sb) sb[0x32]
#define set_WD_quality(sb, val)  sb[0x32] = val
#define get_WD_quality(sb) sb[0x32]
#define WD_white_level_follow_DEFAULT  0x00
#define WD_white_level_follow_ENABLED  0x80
#define WD_white_level_follow_DISABLED 0xC0

  /* 0x33,0x34 - subwindow list
   *        3091 reserved, use 0x00
   *        3096 bits 0-3 of byte 34 denote use of subwindows 1...4
   */
  0x00, 0x00,
#define set_WD_subwindow_list(sb, val) putnbyte(sb + 0x33, val, 2)
#define get_WD_subwindow_list(sb)	getnbyte(sb + 0x33, 2)

  /* 0x35 - paper size
   *        3091 unsupported, always use 0xc0
   *        3096 if bits 6-7 both set, custom paper size enabled,  
   *             bytes 0x36-0x3d used. Otherwise, a number of 
   *             valid fixed values denote common paper formats.
   */
  0xC0,
#define set_WD_paper_selection(sb, val) setbitfield(sb + 0x35, 3, 6, val)
#define WD_paper_SEL_UNDEFINED     0
#define WD_paper_SEL_NON_STANDARD  3

/* we no longer use these, custom size (0xc0) overrides,
   and more recent scanners only use custom size anyway
#define get_WD_paper_selection(sb)      getbitfield(sb + 0x35, 3, 6)
#define WD_paper_SEL_STANDARD      2

#define set_WD_paper_orientation(sb, val) setbitfield(sb + 0x35, 1, 4, val)
#define get_WD_paper_orientation(sb)      getbitfield(sb + 0x35, 1, 4)
#define WD_paper_PORTRAIT  0
#define WD_paper_LANDSCAPE 1

#define set_WD_paper_size(sb, val)  setbitfield(sb + 0x35, 0x0f, 0, val)
#define get_WD_paper_size(sb)       getbitfield(sb + 0x35, 0x0f, 0)
#define WD_paper_UNDEFINED 0
#define WD_paper_A3        3
#define WD_paper_A4        4
#define WD_paper_A5        5
#define WD_paper_DOUBLE    6
#define WD_paper_LETTER    7
#define WD_paper_B4       12
#define WD_paper_B5       13
#define WD_paper_LEGAL    15
#define WD_paper_CUSTOM   14
*/

  /* 0x36-0x39 - custom paper width
   *        3091 0<w<=10200
   *        3096 0<w<=14592
   */
  0x00, 0x00, 0x00, 0x00,
#define set_WD_paper_width_X(sb, val) putnbyte(sb + 0x36, val, 4)
#define get_WD_paper_width_X(sb)	getnbyte(sb + 0x36, 4)

  /* 0x3a-0x3d - custom paper length
   *        3091 0<w<=16832
   *        3096 0<w<=20736
   */
  0x00, 0x00, 0x00, 0x00,
#define set_WD_paper_length_Y(sb, val) putnbyte(sb+0x3a, val, 4)
#define get_WD_paper_length_Y(sb)	getnbyte(sb+0x3a, 4)

  0X00,				/* 0x3e *//* DTC selection (3091: reserved) */
#define set_WD_dtc_selection(sb, val) setbitfield(sb + 0x3e, 3, 6, val)
#define get_WD_dtc_selection(sb) getbitfield(sb + 0x3e, 3, 6)
#define WD_dtc_selection_DEFAULT    0
#define WD_dtc_selection_DYNAMIC    1
#define WD_dtc_selection_SIMPLIFIED 2
  /* - the rest of this is all zeroes, comments from the 3091 docs. */

  0x00,				/* 0x3f  reserved */
  0x00,				/* 0x40  intial slice (floating slice parameter) */
  0x00,				/* 0x41  paper color ratio (white level slice ratio)  (floating slice parameter) */
  0x00,				/* 0x42  black/white ratio (black/white slice)  (floating slice parameter) */
  0x00,				/* 0x43  Up (+UP count setting) (floating slice parameter) */
  0x00,				/* 0x44  Down (+Down count setting) (floating slice parameter) */
  0x00,				/* 0x45  Lower Limit Slice (floating slice parameter) */
  0x00,				/* 0x46  Compensation Line Interval (floating slice parameter) */
  0x00,				/* 0x47  Reserved */
  0x00,				/* 0x48  Error Diffusion upper limit slice (error diffusion parameter) */
  0x00,				/* 0x49  Error Diffusion lower limit slice (error diffusion parameter) */
  0x00,				/* 0x4a  Reserved */
  0x00,				/* 0x4b  Reserved */
  0x00,				/* 0x4c  Enhancement Setting */
  0x00,				/* 0x4d  Laplacian Gradient Coefficient (enhancement parameter) */
  0x00,				/* 0x4e  Gradient Coefficient (enhancement parameter) */
  0x00,				/* 0x4f  Laplacian Slice (enhancement parameter) */
  0x00,				/* 0x50  Gradient Slice (enhancement parameter) */
  0x00,				/* 0x51  Reserved */
  0x00,				/* 0x52  Primary Scan Ratio Compensation */
  0x00, 0x00, 0x00, 0x00, 0x00,	/* 0x53 - 0x57 reserved */
  0x00, 0x00, 0x00, 0x00, 0x00,	/* 0x58 - 0x5c reserved */
  0x00, 0x00, 0x00		/* 0x5d - 0x5f reserved */
};
static scsiblk window_descriptor_blockB =
  { window_descriptor_blockC, sizeof (window_descriptor_blockC) };
#define max_WDB_size 0xc8

/* ==================================================================== */

#define RS_return_size                    0x12

static unsigned char request_senseC[] =
{REQUEST_SENSE, 0x00, 0x00, 0x00, RS_return_size, 0x00};
static scsiblk request_senseB = {request_senseC, sizeof (request_senseC)};

/* defines for request sense return block */
#define get_RS_information_valid(b)       getbitfield(b + 0x00, 1, 7)
#define get_RS_error_code(b)              getbitfield(b + 0x00, 0x7f, 0)
#define get_RS_filemark(b)                getbitfield(b + 0x02, 1, 7)
#define get_RS_EOM(b)                     getbitfield(b + 0x02, 1, 6)
#define get_RS_ILI(b)                     getbitfield(b + 0x02, 1, 5)
#define get_RS_sense_key(b)               getbitfield(b + 0x02, 0x0f, 0)
#define get_RS_information(b)             getnbyte(b+0x03, 4)	/* normally 0 */
#define get_RS_additional_length(b)       b[0x07]	/* always 10 */
#define get_RS_ASC(b)                     b[0x0c]
#define get_RS_ASCQ(b)                    b[0x0d]
#define get_RS_SKSV(b)                    getbitfield(b+0x0f,1,7) /* valid=0 */
#define get_RS_SKSB(b)                    getnbyte(b+0x0f, 3)

/* when RS is 0x05/0x26 bad bytes listed in sksb */
#define get_RS_offending_byte(b)          getnbyte(b+0x10, 2)

/* 3091 and 3092 use RS instead of ghs. RS must be 0x00/0x80 */
/* in ascq */
#define get_RS_adf_open(in)        getbitfield(in+0x0d, 1, 7)
#define get_RS_send_sw(in)         getbitfield(in+0x0d, 1, 5)
#define get_RS_scan_sw(in)         getbitfield(in+0x0d, 1, 4)
#define get_RS_duplex_sw(in)       getbitfield(in+0x0d, 1, 2)
#define get_RS_top(in)             getbitfield(in+0x0d, 1, 1)
#define get_RS_hopper(in)          getbitfield(in+0x0d, 1, 0)

/* in sksb */
#define get_RS_function(in)        getbitfield(in+0x0f, 0x0f, 3)
#define get_RS_density(in)         getbitfield(in+0x0f, 0x07, 0)

/* ==================================================================== */

#endif
