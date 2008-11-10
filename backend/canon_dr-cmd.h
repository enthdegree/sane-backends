#ifndef CANON_DR_CMD_H
#define CANON_DR_CMD_H

/* 
 * Part of SANE - Scanner Access Now Easy.
 * 
 * Please see to opening comments in canon_dr.c
 */

/****************************************************/

#define USB_HEADER_LEN     12
#define USB_COMMAND_LEN    12
#define USB_STATUS_LEN     4
#define USB_STATUS_OFFSET  0
#define USB_COMMAND_TIME   30000
#define USB_DATA_TIME      30000
#define USB_STATUS_TIME    30000

/*static inline void */
static void
setbitfield (unsigned char *pageaddr, int mask, int shift, int val)
{
  *pageaddr = (*pageaddr & ~(mask << shift)) | ((val & mask) << shift);
}

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

#define set_SCSI_opcode(out, val)          out[0]=val
#define set_SCSI_lun(out, val)             setbitfield(out + 1, 7, 5, val)

/* ==================================================================== */
/* TEST_UNIT_READY */
#define TEST_UNIT_READY_code    0x00
#define TEST_UNIT_READY_len     6

/* ==================================================================== */
/* REQUEST_SENSE */
#define REQUEST_SENSE_code      0x03
#define REQUEST_SENSE_len       6

#define RS_return_size          0x0e
#define set_RS_return_size(icb,val)        icb[0x04]=val

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
/* #define get_RS_offending_byte(b)          getnbyte(b+0x10, 2) */

/* ==================================================================== */
/* INQUIRY */
#define INQUIRY_code            0x12
#define INQUIRY_len             6

#define INQUIRY_std_len         0x30
#define INQUIRY_vpd_len         0x28

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
#define get_IN_std_res_60(in)              getbitfield(in+ 0x12, 1, 7)
#define get_IN_std_res_75(in)              getbitfield(in+ 0x12, 1, 6)
#define get_IN_std_res_100(in)             getbitfield(in+ 0x12, 1, 5)
#define get_IN_std_res_120(in)             getbitfield(in+ 0x12, 1, 4)
#define get_IN_std_res_150(in)             getbitfield(in+ 0x12, 1, 3)
#define get_IN_std_res_160(in)             getbitfield(in+ 0x12, 1, 2)
#define get_IN_std_res_180(in)             getbitfield(in+ 0x12, 1, 1)
#define get_IN_std_res_200(in)             getbitfield(in+ 0x12, 1, 0)
#define get_IN_std_res_240(in)             getbitfield(in+ 0x13, 1, 7)
#define get_IN_std_res_300(in)             getbitfield(in+ 0x13, 1, 6)
#define get_IN_std_res_320(in)             getbitfield(in+ 0x13, 1, 5)
#define get_IN_std_res_400(in)             getbitfield(in+ 0x13, 1, 4)
#define get_IN_std_res_480(in)             getbitfield(in+ 0x13, 1, 3)
#define get_IN_std_res_600(in)             getbitfield(in+ 0x13, 1, 2)
#define get_IN_std_res_800(in)             getbitfield(in+ 0x13, 1, 1)
#define get_IN_std_res_1200(in)            getbitfield(in+ 0x13, 1, 0)
#define get_IN_window_width(in)            getnbyte(in + 0x14, 4)
#define get_IN_window_length(in)           getnbyte(in + 0x18, 4)
#define get_IN_unknown7(in)                getbitfield(in+0x1c, 1, 7)
#define get_IN_unknown6(in)                getbitfield(in+0x1c, 1, 6)
#define get_IN_unknown5(in)                getbitfield(in+0x1c, 1, 5)
#define get_IN_unknown4(in)                getbitfield(in+0x1c, 1, 4)
#define get_IN_multilevel(in)              getbitfield(in+0x1c, 1, 3)
#define get_IN_half_tone(in)               getbitfield(in+0x1c, 1, 2)
#define get_IN_monochrome(in)              getbitfield(in+0x1c, 1, 1)
#define get_IN_overflow(in)                getbitfield(in+0x1c, 1, 0)

/* some scanners need evpd inquiry data manipulated */
#define set_IN_page_length(in,val)             in[0x04]=val

/* ==================================================================== */
/* RESERVE_UNIT */
#define RESERVE_UNIT_code       0x16
#define RESERVE_UNIT_len        6

/* ==================================================================== */
/* RELEASE_UNIT */

#define RELEASE_UNIT_code       0x17
#define RELEASE_UNIT_len        6

/* ==================================================================== */
/* SCAN */
#define SCAN_code               0x1b
#define SCAN_len                6

#define set_SC_xfer_length(sb, val) sb[0x04] = (unsigned char)val

/* ==================================================================== */
/* SET_WINDOW */
#define SET_WINDOW_code         0x24
#define SET_WINDOW_len          10

#define set_SW_xferlen(sb, len) putnbyte(sb + 0x06, len, 3)

#define SW_header_len		8
#define SW_desc_len		0x2c

/* ==================================================================== */
/* GET_WINDOW */
#define GET_WINDOW_code         0x25
#define GET_WINDOW_len          0

/* ==================================================================== */
/* READ */
#define READ_code               0x28
#define READ_len                10

#define set_R_datatype_code(sb, val)    sb[0x02] = val
#define R_datatype_imagedata		0x00
#define R_datatype_pixelsize		0x80
#define R_datatype_counter  		0x84
#define set_R_window_id(sb, val)       sb[0x05] = val
#define set_R_xfer_length(sb, val)     putnbyte(sb + 0x06, val, 3)

/*pixelsize*/
#define R_PSIZE_len                    0x18
#define get_PSIZE_num_x(in)            getnbyte(in + 0x00, 4)
#define get_PSIZE_num_y(in)            getnbyte(in + 0x04, 4)
#define get_PSIZE_paper_w(in)          getnbyte(in + 0x08, 4)
#define get_PSIZE_paper_l(in)          getnbyte(in + 0x0C, 4)

/*counter*/
#define R_COUNTER_len                  0x08
#define get_R_COUNTER_count(in)        getnbyte(in + 0x04, 4)

/* ==================================================================== */
/* SEND */
#define SEND_code               0x2a
#define SEND_len                10

#define set_S_xfer_datatype(sb, val) sb[0x02] = (unsigned char)val
/*#define S_datatype_imagedatai		0x00
#define S_datatype_halftone_mask        0x02
#define S_datatype_gamma_function       0x03*/
#define S_datatype_lut_data             0x83
#define S_datatype_counter  		0x84
/*#define S_datatype_jpg_q_table          0x88*/
#define S_datatype_endorser_data       0x90
/*#define S_EX_datatype_lut		0x01
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

/*lut*/
#define S_lut_header_len   0x0a
#define set_S_lut_order(sb, val)    putnbyte(sb + 2, val, 1)
#define S_lut_order_single 0x10
#define set_S_lut_ssize(sb, val)    putnbyte(sb + 4, val, 2)
#define set_S_lut_dsize(sb, val)    putnbyte(sb + 6, val, 2)
#define S_lut_data_min_len          256
#define S_lut_data_max_len          1024

/*counter*/
#define S_COUNTER_len               0x08
#define set_S_COUNTER_count(sb,val) putnbyte(sb + 0x04, val, 4)

/* ==================================================================== */
/* OBJECT_POSITION */
#define OBJECT_POSITION_code    0x31
#define OBJECT_POSITION_len     10

#define set_OP_autofeed(b,val) setbitfield(b+0x01, 0x07, 0, val)
#define OP_Discharge	0x00
#define OP_Feed	        0x01

/* ==================================================================== */
/* Page codes used by GET/SET SCAN MODE */
#define SM_pc_adf                      0x01
#define SM_pc_tpu                      0x02
#define SM_pc_scan_ctl                 0x20
#define SM_pc_unknown30                0x30
#define SM_pc_unknown32                0x32
#define SM_pc_unknown34                0x34
#define SM_pc_all_pc                   0x3F

/* ==================================================================== */
/* GET SCAN MODE */
#define GET_SCAN_MODE_code               0xd5
#define GET_SCAN_MODE_len                6

#define set_GSM_unkown(sb, val)         sb[0x01] = val
#define set_GSM_page_code(sb, val)      sb[0x02] = val
#define set_GSM_len(sb, val)            sb[0x04] = val

#define GSM_PSIZE_len                   0x5a

/* ==================================================================== */
/* SET SCAN MODE */
#define SET_SCAN_MODE_code               0xd6
#define SET_SCAN_MODE_len                6

#define set_SSM_unkown(sb, val)         sb[0x01] = val
#define set_SSM_page_code(sb, val)      sb[0x02] = val
#define set_SSM_len(sb, val)            sb[0x04] = val

#define SSM_PSIZE_len                   0x14

/* ==================================================================== */
/* window descriptor macros for SET_WINDOW and GET_WINDOW */

#define set_WPDB_wdblen(sb, len) putnbyte(sb + 0x06, len, 2)

/* ==================================================================== */

  /* 0x00 - Window Identifier */
#define set_WD_wid(sb, val) sb[0] = val
#define WD_wid_front 0x00
#define WD_wid_back 0x01

  /* 0x01 - Reserved (bits 7-1), AUTO (bit 0) */
#define set_WD_auto(sb, val) setbitfield(sb + 0x01, 1, 0, val)
#define get_WD_auto(sb)	getbitfield(sb + 0x01, 1, 0)

  /* 0x02,0x03 - X resolution in dpi */
#define set_WD_Xres(sb, val) putnbyte(sb + 0x02, val, 2)
#define get_WD_Xres(sb)	getnbyte(sb + 0x02, 2)

  /* 0x04,0x05 - X resolution in dpi */
#define set_WD_Yres(sb, val) putnbyte(sb + 0x04, val, 2)
#define get_WD_Yres(sb)	getnbyte(sb + 0x04, 2)

  /* 0x06-0x09 - Upper Left X in 1/1200 inch */
#define set_WD_ULX(sb, val) putnbyte(sb + 0x06, val, 4)
#define get_WD_ULX(sb) getnbyte(sb + 0x06, 4)

  /* 0x0a-0x0d - Upper Left Y in 1/1200 inch */
#define set_WD_ULY(sb, val) putnbyte(sb + 0x0a, val, 4)
#define get_WD_ULY(sb) getnbyte(sb + 0x0a, 4)

  /* 0x0e-0x11 - Width in 1/1200 inch */
#define set_WD_width(sb, val) putnbyte(sb + 0x0e, val, 4)
#define get_WD_width(sb) getnbyte(sb + 0x0e, 4)

  /* 0x12-0x15 - Height in 1/1200 inch */
#define set_WD_length(sb, val) putnbyte(sb + 0x12, val, 4)
#define get_WD_length(sb) getnbyte(sb + 0x12, 4)

  /* 0x16 - Brightness */
#define set_WD_brightness(sb, val) sb[0x16] = val
#define get_WD_brightness(sb)  sb[0x16]

  /* 0x17 - Threshold */
#define set_WD_threshold(sb, val) sb[0x17] = val
#define get_WD_threshold(sb)  sb[0x17]

  /* 0x18 - Contrast */
#define set_WD_contrast(sb, val) sb[0x18] = val
#define get_WD_contrast(sb) sb[0x18]

  /* 0x19 - Image Composition (color mode) */
#define set_WD_composition(sb, val)  sb[0x19] = val
#define get_WD_composition(sb) sb[0x19]
#define WD_comp_LA 0
#define WD_comp_HT 1
#define WD_comp_GS 2
#define WD_comp_CL 3
#define WD_comp_CH 4
#define WD_comp_CG 5

  /* 0x1a - Depth */
#define set_WD_bitsperpixel(sb, val) sb[0x1a] = val
#define get_WD_bitsperpixel(sb)	sb[0x1a]

  /* 0x1b,0x1c - Halftone Pattern */
#define set_WD_ht_type(sb, val) sb[0x1b] = val
#define get_WD_ht_type(sb)	sb[0x1b]
#define WD_ht_type_DEFAULT 0
#define WD_ht_type_DITHER 1
#define WD_ht_type_DIFFUSION 2

#define set_WD_ht_pattern(sb, val) sb[0x1c] = val
#define get_WD_ht_pattern(sb)	   sb[0x1c]

  /* 0x1d - Reverse image, reserved area, padding type */
#define set_WD_rif(sb, val)       setbitfield(sb + 0x1d, 1, 7, val)
#define get_WD_rif(sb)	          getbitfield(sb + 0x1d, 1, 7)
#define set_WD_reserved(sb, val)  setbitfield(sb + 0x1d, 1, 5, val)
#define get_WD_reserved(sb)	  getbitfield(sb + 0x1d, 1, 5)

  /* 0x1e,0x1f - Bit ordering */
#define set_WD_bitorder(sb, val) putnbyte(sb + 0x1e, val, 2)
#define get_WD_bitorder(sb)	getnbyte(sb + 0x1e, 2)

  /* 0x20 - compression type */
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
#define set_WD_compress_arg(sb, val)  sb[0x21] = val
#define get_WD_compress_arg(sb) sb[0x21]

  /* 0x22-0x27 - reserved */

  /* 0x28-0x2c - vendor unique */
  /* FIXME: more params here? */

/* ==================================================================== */

#endif
