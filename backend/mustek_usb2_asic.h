/* sane - Scanner Access Now Easy.

   Copyright (C) 2005 Mustek.
   Originally maintained by Mustek
   Author:Roy 2005.5.24

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

   This file implements a SANE backend for the Mustek BearPaw 2448 TA Pro 
   and similar USB2 scanners. */

#ifndef MUSTEK_USB2_ASIC_H
#define MUSTEK_USB2_ASIC_H


/* ---------------------- low level ASIC defines -------------------------- */

#ifdef WORDS_BIGENDIAN
#  define LOBYTE(w)	(SANE_Byte)(((unsigned short)(w) >> 8) & 0xff)
#  define HIBYTE(w)	(SANE_Byte)((unsigned short)(w) & 0xff)
#  define BYTE0(x)	(SANE_Byte)(((unsigned int)(x) >> 24) & 0xff)
#  define BYTE1(x)	(SANE_Byte)(((unsigned int)(x) >> 16) & 0xff)
#  define BYTE2(x)	(SANE_Byte)(((unsigned int)(x) >> 8) & 0xff)
#  define BYTE3(x)	(SANE_Byte)((unsigned int)(x) & 0xff)
#else
#  define LOBYTE(w)	(SANE_Byte)((unsigned short)(w) & 0xff)
#  define HIBYTE(w)	(SANE_Byte)(((unsigned short)(w) >> 8) & 0xff)
#  define BYTE0(x)	(SANE_Byte)((unsigned int)(x) & 0xff)
#  define BYTE1(x)	(SANE_Byte)(((unsigned int)(x) >> 8) & 0xff)
#  define BYTE2(x)	(SANE_Byte)(((unsigned int)(x) >> 16) & 0xff)
#  define BYTE3(x)	(SANE_Byte)(((unsigned int)(x) >> 24) & 0xff)
#endif


typedef enum
{
  FS_ATTACHED,
  FS_OPENED,
  FS_SCANNING
} FIRMWARESTATE;

typedef enum
{
  SS_REFLECTIVE,
  SS_POSITIVE,
  SS_NEGATIVE
} SCANSOURCE;

typedef enum
{
  ACTION_MODE_ACCDEC_MOVE,
  ACTION_MODE_UNIFORM_SPEED_MOVE
} ACTION_MODE;

typedef enum
{
  ACTION_TYPE_BACKWARD,
  ACTION_TYPE_FORWARD,
  ACTION_TYPE_BACKTOHOME
} ACTION_TYPE;

typedef enum
{
  EXTERNAL_RAM,
  ON_CHIP_PRE_GAMMA,
  ON_CHIP_FINAL_GAMMA
} RAM_TYPE;

typedef enum
{
  SCAN_TYPE_NORMAL,
  SCAN_TYPE_CALIBRATE_LIGHT,
  SCAN_TYPE_CALIBRATE_DARK
} SCAN_TYPE;


typedef struct
{
  SANE_Bool IsWriteAccess;
  RAM_TYPE RamType;
  unsigned int StartAddress;	/* only lower 3 bytes used */
  unsigned int RwSize;		/* unit: byte; must be a multiple of 2 */
  SANE_Byte *BufferPtr;
} RAMACCESS;

typedef struct
{
  SANE_Byte MoveType;
  SANE_Byte MotorDriverIs3967;
  SANE_Byte MotorCurrent;
} MOTOR_CURRENT_AND_PHASE;

typedef struct
{
  unsigned short StartSpeed;
  unsigned short EndSpeed;
  unsigned short AccStepBeforeScan;
  SANE_Byte DecStepAfterScan;
  unsigned short * pMotorTable;
} CALCULATEMOTORTABLE;

typedef struct
{
  ACTION_MODE ActionMode;
  ACTION_TYPE ActionType;
  unsigned short FixMoveSpeed;
  unsigned int FixMoveSteps;	/* only lower 3 bytes used */
  unsigned short AccStep;	/* max. value = 511 */
  SANE_Byte DecStep;

  unsigned short wScanAccSteps;
  SANE_Byte bScanDecSteps;
} MOTORMOVE;

typedef struct
{
  SANE_Byte SDRAM_Delay;
} ASIC_ModelParams;

typedef struct
{
  /* AFE */
  unsigned int AFE_ADCCLK_Timing;
  unsigned int AFE_ADCVS_Timing;
  unsigned int AFE_ADCRS_Timing;
  unsigned short AFE_ChannelA_LatchPos;
  unsigned short AFE_ChannelB_LatchPos;
  unsigned short AFE_ChannelC_LatchPos;
  unsigned short AFE_ChannelD_LatchPos;
  SANE_Byte AFE_Secondary_FF_LatchPos;

  /* sensor */
  unsigned int CCD_DummyCycleTiming;
  SANE_Byte PHTG_PulseWidth;
  SANE_Byte PHTG_WaitWidth;
  unsigned short ChannelR_StartPixel;
  unsigned short ChannelR_EndPixel;
  unsigned short ChannelG_StartPixel;
  unsigned short ChannelG_EndPixel;
  unsigned short ChannelB_StartPixel;
  unsigned short ChannelB_EndPixel;
  SANE_Byte PHTG_TimingAdj;
  SANE_Byte PHTG_TimingSetup;

  unsigned int CCD_PHRS_Timing;
  unsigned int CCD_PHCP_Timing;
  unsigned int CCD_PH1_Timing;
  unsigned int CCD_PH2_Timing;

  unsigned short wCCDPixelNumber_Full;
  unsigned short wCCDPixelNumber_Half;
} Timings;

typedef struct
{
  SANE_Byte Gain[3];
  SANE_Byte Offset[3];
  SANE_Bool Direction[3];
} ADConverter;

typedef struct
{
  SANE_String_Const device_name;
  int fd;	/* file descriptor of scanner */

  const ASIC_ModelParams * params;

  FIRMWARESTATE firmwarestate;
  SANE_Bool isFirstOpenChip;	/* == SANE_FALSE after first Asic_Open */
  SANE_Bool isUsb20;

  unsigned int dwBytesCountPerRow;

  Timings Timing;
  ADConverter AD;

  SANE_Byte isMotorMoveToFirstLine;

  unsigned int dwShadingTableSize;
  unsigned short * pShadingTable;

  SANE_Byte RegisterBankStatus;
  SANE_Bool is2ByteTransfer;
  SANE_Byte dataBuf[4];
} ASIC;


/* debug levels */
#define DBG_CRIT 	0	/* critical errors that should be printed even
				   if user hasn't enabled debugging -- use 
				   with care and only after sane_open has been 
				   called */
#define DBG_ERR 	1	/* other errors */
#define DBG_WARN 	2	/* unusual conditions that may not be fatal */
#define DBG_INFO 	3	/* information useful for the educated user */
#define DBG_DET 	4	/* more detailed information */
#define DBG_FUNC 	5	/* start and exits of high level functions */
#define DBG_ASIC 	6	/* starts and exits of low level functions */
#define DBG_DBG 	10	/* useful only for tracing bugs */

#define DBG_ASIC_ENTER()	DBG (DBG_ASIC, "%s: enter\n", __FUNCTION__)
#define DBG_ASIC_LEAVE()	DBG (DBG_ASIC, "%s: leave\n", __FUNCTION__)
#define DBG_ENTER()		DBG (DBG_FUNC, "%s: enter\n", __FUNCTION__)
#define DBG_LEAVE()		DBG (DBG_FUNC, "%s: leave\n", __FUNCTION__)


#define DMA_BLOCK_SIZE (32 * 1024)
#define DRAM_TEST_SIZE 64
#define DRAM_1Mx16_SIZE (1024 * 1024)
#define PACK_AREA_START_ADDRESS ((DRAM_1Mx16_SIZE / 4) * 3)
#define ShadingTableSize(x) (((x + 10) * 6) + (((x + 10) * 6) / 240) * 16)
#define WAIT_BUFFER_ONE_LINE_SIZE (11000 * 6)
#define BANK_SIZE 64
#define MOTOR_TABLE_SIZE (512 * 8)
#define TABLE_OFFSET_BASE 14
#define TABLE_BASE_SIZE (1 << TABLE_OFFSET_BASE)

#define LAMP0_PWM_DEFAULT 255
#define LAMP1_PWM_DEFAULT 255

#define TA_CAL_PIXELNUMBER 50000
#define TA_IMAGE_PIXELNUMBER 61000
#define SENSOR_DPI 1200


/**************************** ASIC registers ***********************/

/* ES01_XX = Easy Scan_01 register hex xx */
#define		ES01_00_AFEReg0				0x00
#define		ES01_01_AFEReg1				0x01
#define		ES01_02_AFEReg2				0x02
#define		ES01_03_AFEReg3				0x03
#define		ES01_04_AFEReset			0x04
#define		ES01_05_AFEReg4				0x05

#define		ES01_20_DACRed				0x20
#define		ES01_21_DACGreen			0x21
#define		ES01_22_DACBlue				0x22
#define		ES01_24_SignRed				0x24
#define		ES01_25_SignGreen			0x25
#define		ES01_26_SignBlue			0x26
#define		ES01_28_PGARed				0x28
#define		ES01_29_PGAGreen			0x29
#define		ES01_2A_PGABlue				0x2A

#define		ES01_00_ADAFEConfiguration		0x00
#define		ES01_02_ADAFEMuxConfig			0x02
#define		ES01_04_ADAFEPGACH1			0x04
#define		ES01_06_ADAFEPGACH2			0x06
#define		ES01_08_ADAFEPGACH3			0x08

#define		ES01_0C_ADAFEOffsetCH1P			0x0C
#define		ES01_0D_ADAFEOffsetCH1N			0x0D
#define		ES01_0E_ADAFEOffsetCH2P			0x0E
#define		ES01_0F_ADAFEOffsetCH2N			0x0F
#define		ES01_10_ADAFEOffsetCH3P			0x10
#define		ES01_11_ADAFEOffsetCH3N			0x11

#define		ES01_00_AD9826Configuration		0x00
#define		ES01_02_AD9826MuxConfig			0x02
#define		ES01_04_AD9826PGARed			0x04
#define		ES01_06_AD9826PGAGreen			0x06
#define		ES01_08_AD9826PGABlue			0x08
#define		ES01_0A_AD9826OffsetRedP		0x0A
#define		ES01_0B_AD9826OffsetRedN		0x0B
#define		ES01_0C_AD9826OffsetGreenP		0x0C
#define		ES01_0D_AD9826OffsetGreenN		0x0D
#define		ES01_0E_AD9826OffsetBlueP		0x0E
#define		ES01_0F_AD9826OffsetBlueN		0x0F

#define		ES02_50_MOTOR_CURRENT_CONTORL		0x50
		/* bit[0] */
#define	DOWN_LOAD_MOTOR_TABLE_ENABLE				0x01
		/* bit[3:1] */
#define	_4_TABLE_SPACE_FOR_FULL_STEP				0x00
#define	_8_TABLE_SPACE_FOR_1_DIV_2_STEP				0x02
#define	_16_TABLE_SPACE_FOR_1_DIV_4_STEP			0x06
#define	_32_TABLE_SPACE_FOR_1_DIV_8_STEP			0x0E
		/* bit[4] */
#define	MOTOR_TABLE_ADDR_SHOW_IN_FIRST_PIXEL_OF_LINE_ENABLE	0x10
		/* bit[5] */
#define	MOTOR_CURRENT_TABLE_ADDRESS_BIT4_TO_BIT0_ENABLE		0x20

#define		ES02_51_MOTOR_PHASE_TABLE_1		0x51
#define		ES02_52_MOTOR_CURRENT_TABLE_A		0x52
#define		ES02_53_MOTOR_CURRENT_TABLE_B		0x53

#define		ES01_5F_REGISTER_BANK_SELECT		0x5F
		/* bit[1:0] */
#define SELECT_REGISTER_BANK0			0x00
#define	SELECT_REGISTER_BANK1			0x01
#define	SELECT_REGISTER_BANK2			0x02

/* AFE auto configuration */
#define		ES01_60_AFE_AUTO_GAIN_OFFSET_RED_LB	0x60
		/* bit[0] */
#define DIR_POSITIVE	0x00
#define DIR_NEGATIVE	0x01
#define		ES01_61_AFE_AUTO_GAIN_OFFSET_RED_HB	0x61
#define		ES01_62_AFE_AUTO_GAIN_OFFSET_GREEN_LB	0x62
#define		ES01_63_AFE_AUTO_GAIN_OFFSET_GREEN_HB	0x63
#define		ES01_64_AFE_AUTO_GAIN_OFFSET_BLUE_LB	0x64
#define		ES01_65_AFE_AUTO_GAIN_OFFSET_BLUE_HB	0x65

#define		ES01_74_HARDWARE_SETTING		0x74
		/* bit[4:0] */
#define	MOTOR1_SERIAL_INTERFACE_G10_8_ENABLE	0x01
#define	LED_OUT_G11_ENABLE			0x02
#define	SLAVE_SERIAL_INTERFACE_G15_14_ENABLE	0x04
#define	SHUTTLE_CCD_ENABLE			0x08
#define	HARDWARE_RESET_ESIC_AFE_ENABLE		0x10

#define		ES01_79_AFEMCLK_SDRAMCLK_DELAY_CONTROL	0x79
		/* bit[3:0] */
#define	AFEMCLK_DELAY_0_ns			0x00
#define	AFEMCLK_DELAY_2_ns			0x01
#define	AFEMCLK_DELAY_4_ns			0x02
#define	AFEMCLK_DELAY_6_ns			0x03
#define	AFEMCLK_DELAY_8_ns			0x04
#define	AFEMCLK_DELAY_10_ns			0x05
#define	AFEMCLK_DELAY_12_ns			0x06
#define	AFEMCLK_DELAY_14_ns			0x07
#define	AFEMCLK_DELAY_16_ns			0x08
#define	AFEMCLK_DELAY_18_ns			0x09
#define	AFEMCLK_DELAY_20_ns			0x0A
		/* bit[7:4] */
#define	SDRAMCLK_DELAY_0_ns			0x00
#define	SDRAMCLK_DELAY_2_ns			0x10
#define	SDRAMCLK_DELAY_4_ns			0x20
#define	SDRAMCLK_DELAY_6_ns			0x30
#define	SDRAMCLK_DELAY_8_ns			0x40
#define	SDRAMCLK_DELAY_10_ns			0x50
#define	SDRAMCLK_DELAY_12_ns			0x60
#define	SDRAMCLK_DELAY_14_ns			0x70
#define	SDRAMCLK_DELAY_16_ns			0x80
#define	SDRAMCLK_DELAY_18_ns			0x90
#define	SDRAMCLK_DELAY_20_ns			0xA0

#define		ES01_7C_DMA_SIZE_BYTE0			0x7C
#define		ES01_7D_DMA_SIZE_BYTE1			0x7D
#define		ES01_7E_DMA_SIZE_BYTE2			0x7E
#define		ES01_7F_DMA_SIZE_BYTE3			0x7F

#define		ES01_82_AFE_ADCCLK_TIMING_ADJ_BYTE0	0x82
#define		ES01_83_AFE_ADCCLK_TIMING_ADJ_BYTE1	0x83
#define		ES01_84_AFE_ADCCLK_TIMING_ADJ_BYTE2	0x84
#define		ES01_85_AFE_ADCCLK_TIMING_ADJ_BYTE3	0x85

#define		ES01_86_DisableAllClockWhenIdle		0x86
		/* bit[0] */
#define	CLOSE_ALL_CLOCK_ENABLE			0x01

#define		ES01_87_SDRAM_Timing			0x87

#define		ES01_88_LINE_ART_THRESHOLD_HIGH_VALUE	0x88
#define		ES01_89_LINE_ART_THRESHOLD_LOW_VALUE	0x89

#define		ES01_8A_FixScanStepMSB			0x8A

#define		ES01_8B_Status				0x8B
		/* bit[4:0] */
#define	H1H0L1L0_PS_MJ				0x00
#define   SENSOR0_DETECTED	0x10
#define   SENSOR1_DETECTED	0x20
#define   SENSOR0AND1_DETECTED	0x30
#define	SCAN_STATE				0x01
#define	GPIO0_7					0x02
#define	GPIO8_15				0x03
#define	AVAILABLE_BANK_COUNT0_7			0x04
#define	AVAILABLE_BANK_COUNT8_15		0x05
#define	RAM_ADDRESS_POINTER0_7			0x06
#define	RAM_ADDRESS_POINTER8_15			0x07
#define	RAM_ADDRESS_POINTER16_19		0x08
#define	CARRIAGE_POS_DURING_SCAN0_7		0x09
#define	CARRIAGE_POS_DURING_SCAN8_15		0x0A
#define	CARRIAGE_POS_DURING_SCAN16_19		0x0B
#define	LINE_TIME0_7				0x0C
#define	LINE_TIME8_15				0x0D
#define	LINE_TIME16_19				0x0E
#define	LAST_COMMAND_ADDRESS			0x0F
#define	LAST_COMMAND_DATA			0x10
#define	SERIAL_READ_REGISTER_0			0x11
#define	SERIAL_READ_REGISTER_1			0x12
#define	SERIAL_READ_REGISTER_2			0x13
#define	SERIAL_READ_REGISTER_3			0x14
#define	MOTOR_STEP_TRIGGER_POSITION7_0		0x15
#define	MOTOR_STEP_TRIGGER_POSITION15_8		0x16
#define	MOTOR_STEP_TRIGGER_POSITION23_16	0x17
#define	CHIP_STATUS_A				0x18	/* reserved */
#define	CHIP_STRING_0				0x19	/* 0x45 'E' */
#define	CHIP_STRING_1				0x1A	/* 0x53 'S' */
#define	CHIP_STRING_2				0x1B	/* 0x43 'C' */
#define	CHIP_STRING_3				0x1C	/* 0x41 'A' */
#define	CHIP_STRING_4				0x1D	/* 0x4E 'N' */
#define	CHIP_STRING_5				0x1E	/* 0x30 '0' */
#define	CHIP_STRING_6				0x1F	/* 0x31 '1' */

#define		ES01_8C_RestartMotorSynPixelNumberM16LSB	0x8C
#define		ES01_8D_RestartMotorSynPixelNumberM16MSB	0x8D
#define		ES01_90_Lamp0PWM			0x90
#define		ES01_91_Lamp1PWM			0x91
#define		ES01_92_TimerPowerSaveTime		0x92
#define		ES01_93_MotorWatchDogTime		0x93

#define		ES01_94_PowerSaveControl		0x94
		/* bit[2:0] */
#define	TIMER_POWER_SAVE_ENABLE			0x01
#define	USB_POWER_SAVE_ENABLE			0x02
#define	USB_REMOTE_WAKEUP_ENABLE		0x04
		/* bit[5:4] */
#define	LED_MODE_ON				0x00
#define	LED_MODE_OFF				0x10
#define	LED_MODE_FLASH_SLOWLY			0x20
#define	LED_MODE_FLASH_QUICKLY			0x30

#define		ES01_95_GPIOValue0_7			0x95
#define		ES01_96_GPIOValue8_15			0x96
#define		ES01_97_GPIOControl0_7			0x97
#define		ES01_98_GPIOControl8_15			0x98
#define		ES01_99_LAMP_PWM_FREQ_CONTROL		0x99

#define		ES01_9A_AFEControl			0x9A
		/* bit[0] */
#define	ADAFE_AFE				0x00
#define	AD9826_AFE				0x01
		/* bit[1] */
#define	AUTO_CHANGE_AFE_GAIN_OFFSET_ENABLE	0x02

#define		ES01_9B_ShadingTableAddrA14_A21		0x9B
#define		ES01_9C_ShadingTableAddrODDA12_A19	0x9C
#define		ES01_9D_MotorTableAddrA14_A21		0x9D
#define		ES01_9E_HorizontalRatio1to15LSB		0x9E
#define		ES01_9F_HorizontalRatio1to15MSB		0x9F
#define		ES01_A0_HostStartAddr0_7		0xA0

#define		ES01_A1_HostStartAddr8_15		0xA1
		/* bit[3] */
#define	ACCESS_PRE_GAMMA_ES01			0x08
#define	ACCESS_FINAL_GAMMA_ES01			0x00

#define		ES01_A2_HostStartAddr16_21		0xA2
		/* bit[7] */
#define	ACCESS_DRAM				0x00
#define	ACCESS_GAMMA_RAM			0x80

#define		ES01_A3_HostEndAddr0_7			0xA3
#define		ES01_A4_HostEndAddr8_15			0xA4
#define		ES01_A5_HostEndAddr16_21		0xA5

#define		ES01_A6_MotorOption			0xA6
		/* bit[1:0] */
#define	MOTOR_0_ENABLE				0x01
#define	MOTOR_1_ENABLE				0x02
		/* bit[3:2] */
#define	HOME_SENSOR_0_ENABLE			0x00
#define	HOME_SENSOR_1_ENABLE			0x04
#define	HOME_SENSOR_BOTH_ENABLE			0x08
#define	HOME_SENSOR_0_INVERT_ENABLE		0x0C
		/* bit[7:4] */
#define	UNIPOLAR_FULL_STEP_2003_ES03		0x00
#define	BIPOLAR_FULL_2916_ES03			0x10
#define	BIPOLAR_FULL_3955_3966_ES03		0x20
#define	UNIPOLAR_PWM_2003_ES03			0x30
#define	BIPOLAR_3967_ES03			0x40
#define	TABLE_DEFINE_ES03			0x50

#define		ES01_A7_MotorPWMOnTimePhasA		0xA7
#define		ES01_A8_MotorPWMOnTimePhasB		0xA8
#define		ES01_A9_MotorPWMOffTimePhasA		0xA9
#define		ES01_AA_MotorPWMOffTimePhasB		0xAA

#define		ES01_AB_PWM_CURRENT_CONTROL		0xAB
		/* bit[1:0] */
#define	MOTOR_PWM_CURRENT_0			0x00
#define	MOTOR_PWM_CURRENT_1			0x01
#define	MOTOR_PWM_CURRENT_2			0x02
#define	MOTOR_PWM_CURRENT_3			0x03
		/* bit[3:2] */
#define	MOTOR1_GPO_VALUE_0			0x00
#define	MOTOR1_GPO_VALUE_1			0x04
#define	MOTOR1_GPO_VALUE_2			0x08
#define	MOTOR1_GPO_VALUE_3			0x0C
		/* bit[5:4] */
#define	GPO_OUTPUT_ENABLE			0x10
#define	SERIAL_PORT_CONTINUOUS_OUTPUT_ENABLE	0x20

#define		ES01_AC_MotorPWMJamRangeLSB		0xAC
#define		ES01_AD_MotorPWMJamRangeMSB		0xAD
#define		ES01_AE_MotorSyncPixelNumberM16LSB	0xAE
#define		ES01_AF_MotorSyncPixelNumberM16MSB	0xAF
#define		ES01_B0_CCDPixelLSB			0xB0
#define		ES01_B1_CCDPixelMSB			0xB1
#define		ES01_B2_PHTGPulseWidth			0xB2
#define		ES01_B3_PHTGWaitWidth			0xB3
#define		ES01_B4_StartPixelLSB			0xB4
#define		ES01_B5_StartPixelMSB			0xB5
#define		ES01_B6_LineWidthPixelLSB		0xB6
#define		ES01_B7_LineWidthPixelMSB		0xB7

#define		ES01_B8_ChannelRedExpStartPixelLSB	0xB8
#define		ES01_B9_ChannelRedExpStartPixelMSB	0xB9
#define		ES01_BA_ChannelRedExpEndPixelLSB	0xBA
#define		ES01_BB_ChannelRedExpEndPixelMSB	0xBB
#define		ES01_BC_ChannelGreenExpStartPixelLSB	0xBC
#define		ES01_BD_ChannelGreenExpStartPixelMSB	0xBD
#define		ES01_BE_ChannelGreenExpEndPixelLSB	0xBE
#define		ES01_BF_ChannelGreenExpEndPixelMSB	0xBF
#define		ES01_C0_ChannelBlueExpStartPixelLSB	0xC0
#define		ES01_C1_ChannelBlueExpStartPixelMSB	0xC1
#define		ES01_C2_ChannelBlueExpEndPixelLSB	0xC2
#define		ES01_C3_ChannelBlueExpEndPixelMSB	0xC3

#define		ES01_C4_MultiTGTimesRed			0xC4
#define		ES01_C5_MultiTGTimesGreen		0xC5
#define		ES01_C6_MultiTGTimesBlue		0xC6
#define		ES01_C7_MultiTGDummyPixelNumberLSB	0xC7
#define		ES01_C8_MultiTGDummyPixelNumberMSB	0xC8
#define		ES01_C9_CCDDummyPixelNumberLSB		0xC9
#define		ES01_CA_CCDDummyPixelNumberMSB		0xCA
#define		ES01_CB_CCDDummyCycleNumber		0xCB

#define		ES01_CC_PHTGTimingAdjust		0xCC
		/* bit[0] */
#define	PHTG_INVERT_OUTPUT_ENABLE		0x01
		/* bit[1] */
#define	TWO_TG					0x02
#define	MULTI_TG				0x00
		/* bit[3:2] */
#define	CCD_PIXEL_MODE_RED			0x0C
#define	CCD_LINE_MODE_RED_00			0x00
#define	CCD_LINE_MODE_RED_01			0x04
#define	CCD_LINE_MODE_RED_10			0x08
		/* bit[5:4] */
#define	CCD_PIXEL_MODE_GREEN			0x30
#define	CCD_LINE_MODE_GREEN_00			0x00
#define	CCD_LINE_MODE_GREEN_01			0x40
#define	CCD_LINE_MODE_GREEN_10			0x80
		/* bit[7:6] */
#define	CCD_PIXEL_MODE_BLUE			0xC0
#define	CCD_LINE_MODE_BLUE_00			0x00
#define	CCD_LINE_MODE_BLUE_01			0x40
#define	CCD_LINE_MODE_BLUE_10			0x80

#define		ES01_CD_TG_R_CONTROL			0xCD
#define		ES01_CE_TG_G_CONTROL			0xCE
#define		ES01_CF_TG_B_CONTROL			0xCF

#define		ES01_D9_CLEAR_PULSE_WIDTH		0xD9
#define		ES01_DA_CLEAR_SIGNAL_INVERTING_OUTPUT	0xDA
#define		ES01_DB_PH_RESET_EDGE_TIMING_ADJUST	0xDB
#define		ES01_DC_CLEAR_EDGE_TO_PH_TG_EDGE_WIDTH	0xDC
#define		ES01_D0_PH1_0				0xD0
#define		ES01_D1_PH2_0				0xD1
#define		ES01_D2_PH1B_0				0xD2
#define		ES01_D4_PHRS_0				0xD4
#define		ES01_D5_PHCP_0				0xD5
#define		ES01_D6_AFE_VSAMP_0			0xD6
#define		ES01_D7_AFE_RSAMP_0			0xD7

#define		ES01_D8_PHTG_EDGE_TIMING_ADJUST		0xD8

#define		ES01_CD_PH1_0				0xCD
#define		ES01_CE_PH1_1				0xCE
#define		ES01_CF_PH2_0				0xCF
#define		ES01_D0_PH2_1				0xD0
#define		ES01_D1_PH1B_0				0xD1
#define		ES01_D2_PH1B_1				0xD2
#define		ES01_D3_PH2B_0				0xD3
#define		ES01_D4_PH2B_1				0xD4
#define		ES01_D5_PHRS_0				0xD5
#define		ES01_D6_PHRS_1				0xD6
#define		ES01_D7_PHCP_0				0xD7
#define		ES01_D8_PHCP_1				0xD8
#define		ES01_D9_AFE_VSAMP_0			0xD9
#define		ES01_DA_AFE_VSAMP_1			0xDA
#define		ES01_DB_AFE_RSAMP_0			0xDB
#define		ES01_DC_AFE_RSAMP_1			0xDC
#define		ES01_DD_PH1234_IN_DUMMY_TG		0xDD

#define		ES01_DE_CCD_SETUP_REGISTER		0xDE
		/* bit[7:0] */
#define	LINE_SCAN_MODE_ENABLE_ES01		0x01
#define	CIS_SENSOR_MODE_ENABLE_ES01		0x02
#define	CIS_LED_OUTPUT_RtoGtoB_ES01		0x04
#define	CIS_LED_INVERT_OUTPUT_ES01		0x08
#define	ACC_IN_IDLE_ENABLE_ES01			0x10
#define	EVEN_ODD_ENABLE_ES01			0x20
#define	ALTERNATE_EVEN_ODD_ENABLE_ES01		0x40
#define	RESET_CCD_STATE_ENABLE_ES01		0x80

#define		ES01_DF_ICG_CONTROL			0xDF
		/* bit[2:0] */
#define	BEFORE_PHRS_0_ns			0x00
#define	BEFORE_PHRS_416_7t_ns			0x01
#define	BEFORE_PHRS_416_6t_ns			0x02
#define	BEFORE_PHRS_416_5t_ns			0x03
#define	BEFORE_PHRS_416_4t_ns			0x04
#define	BEFORE_PHRS_416_3t_ns			0x05
#define	BEFORE_PHRS_416_2t_ns			0x06
#define	BEFORE_PHRS_416_1t_ns			0x07
		/* bit[6:4] */
#define	ICG_UNIT_1_PIXEL_TIME			0x00
#define	ICG_UNIT_4_PIXEL_TIME			0x10
#define	ICG_UNIT_8_PIXEL_TIME			0x20
#define	ICG_UNIT_16_PIXEL_TIME			0x30
#define	ICG_UNIT_32_PIXEL_TIME			0x40
#define	ICG_UNIT_64_PIXEL_TIME			0x50
#define	ICG_UNIT_128_PIXEL_TIME			0x60
#define	ICG_UNIT_256_PIXEL_TIME			0x70

#define		ES01_E0_MotorAccStep0_7			0xE0
#define		ES01_E1_MotorAccStep8_8			0xE1
#define		ES01_E2_MotorStepOfMaxSpeed0_7		0xE2
#define		ES01_E3_MotorStepOfMaxSpeed8_15		0xE3
#define		ES01_E4_MotorStepOfMaxSpeed16_19	0xE4
#define		ES01_E5_MotorDecStep			0xE5
#define		ES01_E6_ScanBackTrackingStepLSB		0xE6
#define		ES01_E7_ScanBackTrackingStepMSB		0xE7
#define		ES01_E8_ScanRestartStepLSB		0xE8
#define		ES01_E9_ScanRestartStepMSB		0xE9
#define		ES01_EA_ScanBackHomeExtStepLSB		0xEA
#define		ES01_EB_ScanBackHomeExtStepMSB		0xEB
#define		ES01_EC_ScanAccStep0_7			0xEC
#define		ES01_ED_ScanAccStep8_8			0xED
#define		ES01_EE_FixScanStepLSB			0xEE
#define		ES01_EF_ScanDecStep			0xEF
#define		ES01_F0_ScanImageStep0_7		0xF0
#define		ES01_F1_ScanImageStep8_15		0xF1
#define		ES01_F2_ScanImageStep16_19		0xF2

#define		ES01_F3_ActionOption			0xF3
		/* bit[7:0] */
#define	MOTOR_MOVE_TO_FIRST_LINE_ENABLE		0x01
#define	MOTOR_BACK_HOME_AFTER_SCAN_ENABLE	0x02
#define	SCAN_ENABLE				0x04
#define	SCAN_BACK_TRACKING_ENABLE		0x08
#define	INVERT_MOTOR_DIRECTION_ENABLE		0x10
#define	UNIFORM_MOTOR_AND_SCAN_SPEED_ENABLE	0x20
#define STATIC_SCAN_ENABLE_ES01			0x40
#define	MOTOR_TEST_LOOP_ENABLE			0x80

#define		ES01_F4_ActiveTrigger			0xF4
		/* bit[0] */
#define	ACTION_TRIGGER_ENABLE			0x01

#define		ES01_F5_ScanDataFormat			0xF5
		/* bit[0] */
#define	COLOR_ES02				0x00
#define	GRAY_ES02				0x01
		/* bit[2:1] */
#define	_8_BITS_ES02				0x00
#define	_16_BITS_ES02				0x02
#define	_1_BIT_ES02				0x04
		/* bit[5:4] */
#define	GRAY_RED_ES02				0x00
#define	GRAY_GREEN_ES02				0x10
#define	GRAY_BLUE_ES02				0x20
#define	GRAY_GREEN_BLUE_ES02			0x30

#define		ES01_F6_MotorControl1			0xF6
		/* bit[2:0] */
#define	SPEED_UNIT_1_PIXEL_TIME			0x00
#define	SPEED_UNIT_4_PIXEL_TIME			0x01
#define	SPEED_UNIT_8_PIXEL_TIME			0x02
#define	SPEED_UNIT_16_PIXEL_TIME		0x03
#define	SPEED_UNIT_32_PIXEL_TIME		0x04
#define	SPEED_UNIT_64_PIXEL_TIME		0x05
#define	SPEED_UNIT_128_PIXEL_TIME		0x06
#define	SPEED_UNIT_256_PIXEL_TIME		0x07
		/* bit[5:4] */
#define	MOTOR_SYNC_UNIT_1_PIXEL_TIME		0x00
#define	MOTOR_SYNC_UNIT_16_PIXEL_TIME		0x10
#define	MOTOR_SYNC_UNIT_64_PIXEL_TIME		0x20
#define	MOTOR_SYNC_UNIT_256_PIXEL_TIME		0x30

#define		ES01_F7_DigitalControl			0xF7
		/* bit[0] */
#define	DIGITAL_REDUCE_ENABLE			0x01
		/* bit[3:1] */
#define	DIGITAL_REDUCE_1_1			0x00
#define	DIGITAL_REDUCE_1_2			0x02
#define	DIGITAL_REDUCE_1_4			0x04
#define	DIGITAL_REDUCE_1_8			0x06
#define	DIGITAL_REDUCE_1_16			0x08

#define		ES01_F8_WHITE_SHADING_DATA_FORMAT	0xF8
		/* bit[1:0] */
#define	SHADING_2_INT_14_DEC_ES01		0x00
#define	SHADING_3_INT_13_DEC_ES01		0x01
#define	SHADING_4_INT_12_DEC_ES01		0x02
#define	SHADING_5_INT_11_DEC_ES01		0x03

#define		ES01_F9_BufferFullSize16WordLSB		0xF9
#define		ES01_FA_BufferFullSize16WordMSB		0xFA
#define		ES01_FB_BufferEmptySize16WordLSB	0xFB
#define		ES01_FC_BufferEmptySize16WordMSB	0xFC
#define		ES01_FD_MotorFixedspeedLSB		0xFD
#define		ES01_FE_MotorFixedspeedMSB		0xFE

#define		ES01_FF_SCAN_IMAGE_OPTION		0xFF
		/* bit[7:0] */
#define	OUTPUT_HORIZONTAL_PATTERN_ENABLE	0x01
#define	OUTPUT_VERTICAL_PATTERN_ENABLE		0x02
#define	BYPASS_DARK_SHADING_ENABLE		0x04
#define	BYPASS_WHITE_SHADING_ENABLE		0x08
#define	BYPASS_PRE_GAMMA_ENABLE			0x10
#define	BYPASS_CONVOLUTION_ENABLE		0x20
#define	BYPASS_MATRIX_ENABLE			0x40
#define	BYPASS_GAMMA_ENABLE			0x80

/*******************************************************************/

#define		ES01_160_CHANNEL_A_LATCH_POSITION_HB	0x160
#define		ES01_161_CHANNEL_A_LATCH_POSITION_LB	0x161
#define		ES01_162_CHANNEL_B_LATCH_POSITION_HB	0x162
#define		ES01_163_CHANNEL_B_LATCH_POSITION_LB	0x163
#define		ES01_164_CHANNEL_C_LATCH_POSITION_HB	0x164
#define		ES01_165_CHANNEL_C_LATCH_POSITION_LB	0x165
#define		ES01_166_CHANNEL_D_LATCH_POSITION_HB	0x166
#define		ES01_167_CHANNEL_D_LATCH_POSITION_LB	0x167

#define		ES01_168_SECONDARY_FF_LATCH_POSITION	0x168

#define		ES01_169_NUMBER_OF_SEGMENT_PIXEL_LB	0x169
#define		ES01_16A_NUMBER_OF_SEGMENT_PIXEL_HB	0x16A

#define		ES01_16B_BETWEEN_SEGMENT_INVALID_PIXEL	0x16B
#define		ES01_16C_LINE_SHIFT_OUT_TIMES_DIRECTION	0x16C	/* bit[3:0] */

#define		ES01_16D_EXPOSURE_CYCLE1_SEGMENT1_START_ADDR_BYTE0	0x16D
#define		ES01_16E_EXPOSURE_CYCLE1_SEGMENT1_START_ADDR_BYTE1	0x16E
#define		ES01_16F_EXPOSURE_CYCLE1_SEGMENT1_START_ADDR_BYTE2	0x16F	/* bit[3:0] */

#define		ES01_170_EXPOSURE_CYCLE1_SEGMENT2_START_ADDR_BYTE0	0x170
#define		ES01_171_EXPOSURE_CYCLE1_SEGMENT2_START_ADDR_BYTE1	0x171
#define		ES01_172_EXPOSURE_CYCLE1_SEGMENT2_START_ADDR_BYTE2	0x172	/* bit[3:0] */

#define		ES01_173_EXPOSURE_CYCLE1_SEGMENT3_START_ADDR_BYTE0	0x173
#define		ES01_174_EXPOSURE_CYCLE1_SEGMENT3_START_ADDR_BYTE1	0x174
#define		ES01_175_EXPOSURE_CYCLE1_SEGMENT3_START_ADDR_BYTE2	0x175	/* bit[3:0] */

#define		ES01_176_EXPOSURE_CYCLE1_SEGMENT4_START_ADDR_BYTE0	0x176
#define		ES01_177_EXPOSURE_CYCLE1_SEGMENT4_START_ADDR_BYTE1	0x177
#define		ES01_178_EXPOSURE_CYCLE1_SEGMENT4_START_ADDR_BYTE2	0x178	/* bit[3:0] */

#define		ES01_179_EXPOSURE_CYCLE2_SEGMENT1_START_ADDR_BYTE0	0x179
#define		ES01_17A_EXPOSURE_CYCLE2_SEGMENT1_START_ADDR_BYTE1	0x17A
#define		ES01_17B_EXPOSURE_CYCLE2_SEGMENT1_START_ADDR_BYTE2	0x17B	/* bit[3:0] */

#define		ES01_17C_EXPOSURE_CYCLE2_SEGMENT2_START_ADDR_BYTE0	0x17C
#define		ES01_17D_EXPOSURE_CYCLE2_SEGMENT2_START_ADDR_BYTE1	0x17D
#define		ES01_17E_EXPOSURE_CYCLE2_SEGMENT2_START_ADDR_BYTE2	0x17E	/* bit[3:0] */

#define		ES01_17F_EXPOSURE_CYCLE2_SEGMENT3_START_ADDR_BYTE0	0x17F
#define		ES01_180_EXPOSURE_CYCLE2_SEGMENT3_START_ADDR_BYTE1	0x180
#define		ES01_181_EXPOSURE_CYCLE2_SEGMENT3_START_ADDR_BYTE2	0x181	/* bit[3:0] */

#define		ES01_182_EXPOSURE_CYCLE2_SEGMENT4_START_ADDR_BYTE0	0x182
#define		ES01_183_EXPOSURE_CYCLE2_SEGMENT4_START_ADDR_BYTE1	0x183
#define		ES01_184_EXPOSURE_CYCLE2_SEGMENT4_START_ADDR_BYTE2	0x184	/* bit[3:0] */

#define		ES01_185_EXPOSURE_CYCLE3_SEGMENT1_START_ADDR_BYTE0	0x185
#define		ES01_186_EXPOSURE_CYCLE3_SEGMENT1_START_ADDR_BYTE1	0x186
#define		ES01_187_EXPOSURE_CYCLE3_SEGMENT1_START_ADDR_BYTE2	0x187	/* bit[3:0] */

#define		ES01_188_EXPOSURE_CYCLE3_SEGMENT2_START_ADDR_BYTE0	0x188
#define		ES01_189_EXPOSURE_CYCLE3_SEGMENT2_START_ADDR_BYTE1	0x189
#define		ES01_18A_EXPOSURE_CYCLE3_SEGMENT2_START_ADDR_BYTE2	0x18A	/* bit[3:0] */

#define		ES01_18B_EXPOSURE_CYCLE3_SEGMENT3_START_ADDR_BYTE0	0x18B
#define		ES01_18C_EXPOSURE_CYCLE3_SEGMENT3_START_ADDR_BYTE1	0x18C
#define		ES01_18D_EXPOSURE_CYCLE3_SEGMENT3_START_ADDR_BYTE2	0x18D	/* bit[3:0] */

#define		ES01_18E_EXPOSURE_CYCLE3_SEGMENT4_START_ADDR_BYTE0	0x18E
#define		ES01_18F_EXPOSURE_CYCLE3_SEGMENT4_START_ADDR_BYTE1	0x18F
#define		ES01_190_EXPOSURE_CYCLE3_SEGMENT4_START_ADDR_BYTE2	0x190	/* bit[3:0] */

#define		ES01_191_CHANNEL_GAP1_BYTE0	0x191
#define		ES01_192_CHANNEL_GAP1_BYTE1	0x192
#define		ES01_193_CHANNEL_GAP1_BYTE2	0x193	/* bit[3:0] */

#define		ES01_194_CHANNEL_GAP2_BYTE0	0x194
#define		ES01_195_CHANNEL_GAP2_BYTE1	0x195
#define		ES01_196_CHANNEL_GAP2_BYTE2	0x196	/* bit[3:0] */

#define		ES01_197_CHANNEL_GAP3_BYTE0	0x197
#define		ES01_198_CHANNEL_GAP3_BYTE1	0x198
#define		ES01_199_CHANNEL_GAP3_BYTE2	0x199	/* bit[3:0] */

#define		ES01_19A_CHANNEL_LINE_GAP_LB	0x19A
#define		ES01_19B_CHANNEL_LINE_GAP_HB	0x19B

#define		ES01_19C_MAX_PACK_LINE	0x19C	/* bit[5:0] */

#define		ES01_19D_PACK_THRESHOLD_LINE	0x19D

#define		ES01_19E_PACK_AREA_R_START_ADDR_BYTE0	0x19E
#define		ES01_19F_PACK_AREA_R_START_ADDR_BYTE1	0x19F
#define		ES01_1A0_PACK_AREA_R_START_ADDR_BYTE2	0x1A0	/* bit[3:0] */

#define		ES01_1A1_PACK_AREA_G_START_ADDR_BYTE0	0x1A1
#define		ES01_1A2_PACK_AREA_G_START_ADDR_BYTE1	0x1A2
#define		ES01_1A3_PACK_AREA_G_START_ADDR_BYTE2	0x1A3	/* bit[3:0] */

#define		ES01_1A4_PACK_AREA_B_START_ADDR_BYTE0	0x1A4
#define		ES01_1A5_PACK_AREA_B_START_ADDR_BYTE1	0x1A5
#define		ES01_1A6_PACK_AREA_B_START_ADDR_BYTE2	0x1A6	/* bit[3:0] */

#define		ES01_1A7_PACK_AREA_R_END_ADDR_BYTE0	0x1A7
#define		ES01_1A8_PACK_AREA_R_END_ADDR_BYTE1	0x1A8
#define		ES01_1A9_PACK_AREA_R_END_ADDR_BYTE2	0x1A9	/* bit[3:0] */

#define		ES01_1AA_PACK_AREA_G_END_ADDR_BYTE0	0x1AA
#define		ES01_1AB_PACK_AREA_G_END_ADDR_BYTE1	0x1AB
#define		ES01_1AC_PACK_AREA_G_END_ADDR_BYTE2	0x1AC	/* bit[3:0] */

#define		ES01_1AD_PACK_AREA_B_END_ADDR_BYTE0	0x1AD
#define		ES01_1AE_PACK_AREA_B_END_ADDR_BYTE1	0x1AE
#define		ES01_1AF_PACK_AREA_B_END_ADDR_BYTE2	0x1AF	/* bit[3:0] */

#define		ES01_1B0_SEGMENT_PIXEL_NUMBER_LB	0x1B0
#define		ES01_1B1_SEGMENT_PIXEL_NUMBER_HB	0x1B1

#define		ES01_1B2_OVERLAP_AND_HOLD_PIXEL_NUMBER	0x1B2

#define		ES01_1B3_CONVOLUTION_A	0x1B3
#define		ES01_1B4_CONVOLUTION_B	0x1B4
#define		ES01_1B5_CONVOLUTION_C	0x1B5
#define		ES01_1B6_CONVOLUTION_D	0x1B6
#define		ES01_1B7_CONVOLUTION_E	0x1B7
#define		ES01_1B8_CONVOLUTION_F	0x1B8	/* bit[2:0] */

#define		ES01_1B9_LINE_PIXEL_NUMBER_LB	0x1B9
#define		ES01_1BA_LINE_PIXEL_NUMBER_HB	0x1BA

#define		ES01_1BB_MATRIX_A_LB	0x1BB
#define		ES01_1BC_MATRIX_A_HB	0x1BC	/* bit[3:0] */
#define		ES01_1BD_MATRIX_B_LB	0x1BD
#define		ES01_1BE_MATRIX_B_HB	0x1BE	/* bit[3:0] */
#define		ES01_1BF_MATRIX_C_LB	0x1BF
#define		ES01_1C0_MATRIX_C_HB	0x1C0	/* bit[3:0] */
#define		ES01_1C1_MATRIX_D_LB	0x1C1
#define		ES01_1C2_MATRIX_D_HB	0x1C2	/* bit[3:0] */
#define		ES01_1C3_MATRIX_E_LB	0x1C3
#define		ES01_1C4_MATRIX_E_HB	0x1C4	/* bit[3:0] */
#define		ES01_1C5_MATRIX_F_LB	0x1C5
#define		ES01_1C6_MATRIX_F_HB	0x1C6	/* bit[3:0] */
#define		ES01_1C7_MATRIX_G_LB	0x1C7
#define		ES01_1C8_MATRIX_G_HB	0x1C8	/* bit[3:0] */
#define		ES01_1C9_MATRIX_H_LB	0x1C9
#define		ES01_1CA_MATRIX_H_HB	0x1CA	/* bit[3:0] */
#define		ES01_1CB_MATRIX_I_LB	0x1CB
#define		ES01_1CC_MATRIX_I_HB	0x1CC	/* bit[3:0] */

#define		ES01_1CD_DUMMY_CLOCK_NUMBER	0x1CD	/* bit[3:0] */
#define		ES01_1CE_LINE_SEGMENT_NUMBER	0x1CE

#define		ES01_1D0_DUMMY_CYCLE_TIMING_B0	0x1D0
#define		ES01_1D1_DUMMY_CYCLE_TIMING_B1	0x1D1
#define		ES01_1D2_DUMMY_CYCLE_TIMING_B2	0x1D2
#define		ES01_1D3_DUMMY_CYCLE_TIMING_B3	0x1D3

#define		ES01_1D4_PH1_TIMING_ADJ_B0	0x1D4
#define		ES01_1D5_PH1_TIMING_ADJ_B1	0x1D5
#define		ES01_1D6_PH1_TIMING_ADJ_B2	0x1D6
#define		ES01_1D7_PH1_TIMING_ADJ_B3	0x1D7

#define		ES01_1D8_PH2_TIMING_ADJ_B0	0x1D8
#define		ES01_1D9_PH2_TIMING_ADJ_B1	0x1D9
#define		ES01_1DA_PH2_TIMING_ADJ_B2	0x1DA
#define		ES01_1DB_PH2_TIMING_ADJ_B3	0x1DB

#define		ES01_1DC_PH3_TIMING_ADJ_B0	0x1DC
#define		ES01_1DD_PH3_TIMING_ADJ_B1	0x1DD
#define		ES01_1DE_PH3_TIMING_ADJ_B2	0x1DE
#define		ES01_1DF_PH3_TIMING_ADJ_B3	0x1DF

#define		ES01_1E0_PH4_TIMING_ADJ_B0	0x1E0
#define		ES01_1E1_PH4_TIMING_ADJ_B1	0x1E1
#define		ES01_1E2_PH4_TIMING_ADJ_B2	0x1E2
#define		ES01_1E3_PH4_TIMING_ADJ_B3	0x1E3

#define		ES01_1E4_PHRS_TIMING_ADJ_B0	0x1E4
#define		ES01_1E5_PHRS_TIMING_ADJ_B1	0x1E5
#define		ES01_1E6_PHRS_TIMING_ADJ_B2	0x1E6
#define		ES01_1E7_PHRS_TIMING_ADJ_B3	0x1E7

#define		ES01_1E8_PHCP_TIMING_ADJ_B0	0x1E8
#define		ES01_1E9_PHCP_TIMING_ADJ_B1	0x1E9
#define		ES01_1EA_PHCP_TIMING_ADJ_B2	0x1EA
#define		ES01_1EB_PHCP_TIMING_ADJ_B3	0x1EB

#define		ES01_1EC_AFEVS_TIMING_ADJ_B0	0x1EC
#define		ES01_1ED_AFEVS_TIMING_ADJ_B1	0x1ED
#define		ES01_1EE_AFEVS_TIMING_ADJ_B2	0x1EE
#define		ES01_1EF_AFEVS_TIMING_ADJ_B3	0x1EF

#define		ES01_1F0_AFERS_TIMING_ADJ_B0	0x1F0
#define		ES01_1F1_AFERS_TIMING_ADJ_B1	0x1F1
#define		ES01_1F2_AFERS_TIMING_ADJ_B2	0x1F2
#define		ES01_1F3_AFERS_TIMING_ADJ_B3	0x1F3

#define		ES01_1F4_START_READ_OUT_PIXEL_LB	0x1F4
#define		ES01_1F5_START_READ_OUT_PIXEL_HB	0x1F5
#define		ES01_1F6_READ_OUT_PIXEL_LENGTH_LB	0x1F6
#define		ES01_1F7_READ_OUT_PIXEL_LENGTH_HB	0x1F7

#define		ES01_1F8_PACK_CHANNEL_SELECT_B0	0x1F8
#define		ES01_1F9_PACK_CHANNEL_SELECT_B1	0x1F9
#define		ES01_1FA_PACK_CHANNEL_SELECT_B2	0x1FA
#define		ES01_1FB_PACK_CHANNEL_SIZE_B0	0x1FB
#define		ES01_1FC_PACK_CHANNEL_SIZE_B1	0x1FC
#define		ES01_1FD_PACK_CHANNEL_SIZE_B2	0x1FD

/* AFE gain offset control - ROM code version 0.10 */
#define		ES01_2A0_AFE_GAIN_OFFSET_CONTROL	0x2A0
#define		ES01_2A1_AFE_AUTO_CONFIG_GAIN		0x2A1
#define		ES01_2A2_AFE_AUTO_CONFIG_OFFSET		0x2A2

#define		ES01_2B0_SEGMENT0_OVERLAP_SEGMENT1	0x2B0
#define		ES01_2B1_SEGMENT1_OVERLAP_SEGMENT2	0x2B1
#define		ES01_2B2_SEGMENT2_OVERLAP_SEGMENT3	0x2B2

#define		ES01_2C0_VALID_PIXEL_PARAMETER_OF_SEGMENT1	0x2C0
#define		ES01_2C1_VALID_PIXEL_PARAMETER_OF_SEGMENT2	0x2C1
#define		ES01_2C2_VALID_PIXEL_PARAMETER_OF_SEGMENT3	0x2C2
#define		ES01_2C3_VALID_PIXEL_PARAMETER_OF_SEGMENT4	0x2C3
#define		ES01_2C4_VALID_PIXEL_PARAMETER_OF_SEGMENT5	0x2C4
#define		ES01_2C5_VALID_PIXEL_PARAMETER_OF_SEGMENT6	0x2C5
#define		ES01_2C6_VALID_PIXEL_PARAMETER_OF_SEGMENT7	0x2C6
#define		ES01_2C7_VALID_PIXEL_PARAMETER_OF_SEGMENT8	0x2C7
#define		ES01_2C8_VALID_PIXEL_PARAMETER_OF_SEGMENT9	0x2C8
#define		ES01_2C9_VALID_PIXEL_PARAMETER_OF_SEGMENT10	0x2C9
#define		ES01_2CA_VALID_PIXEL_PARAMETER_OF_SEGMENT11	0x2CA
#define		ES01_2CB_VALID_PIXEL_PARAMETER_OF_SEGMENT12	0x2CB
#define		ES01_2CC_VALID_PIXEL_PARAMETER_OF_SEGMENT13	0x2CC
#define		ES01_2CD_VALID_PIXEL_PARAMETER_OF_SEGMENT14	0x2CD
#define		ES01_2CE_VALID_PIXEL_PARAMETER_OF_SEGMENT15	0x2CE
#define		ES01_2CF_VALID_PIXEL_PARAMETER_OF_SEGMENT16	0x2CF


extern const ASIC_ModelParams paramsMustekBP2448TAPro;
extern const ASIC_ModelParams paramsMicrotek4800H48U;

void SetAFEGainOffset (ASIC * chip);

SANE_Status Asic_FindDevices (unsigned short wVendorID,
			      unsigned short wProductID,
			      SANE_Status (* attach)
					  (SANE_String_Const devname));
SANE_Status Asic_Open (ASIC * chip);
SANE_Status Asic_Close (ASIC * chip);
void Asic_Initialize (ASIC * chip);
SANE_Status Asic_TurnLamp (ASIC * chip, SANE_Bool isLampOn);
SANE_Status Asic_TurnTA (ASIC * chip, SANE_Bool isTAOn);
SANE_Status Asic_SetWindow (ASIC * chip, SCANSOURCE lsLightSource,
			    SCAN_TYPE ScanType, SANE_Byte bScanBits,
			    unsigned short wXResolution,
			    unsigned short wYResolution,
			    unsigned short wX, unsigned short wY,
			    unsigned short wWidth, unsigned short wHeight);
SANE_Status Asic_ScanStart (ASIC * chip);
SANE_Status Asic_ScanStop (ASIC * chip);
SANE_Status Asic_ReadImage (ASIC * chip, SANE_Byte * pBuffer,
			    unsigned short LinesCount);
SANE_Status Asic_CheckFunctionKey (ASIC * chip, SANE_Byte * key);
SANE_Status Asic_IsTAConnected (ASIC * chip, SANE_Bool *hasTA);
SANE_Status Asic_ReadCalibrationData (ASIC * chip, SANE_Byte * pBuffer,
				      unsigned int dwXferBytes,
				      SANE_Bool separateColors);
SANE_Status Asic_MotorMove (ASIC * chip, SANE_Bool isForward,
			    unsigned int dwTotalSteps);
SANE_Status Asic_CarriageHome (ASIC * chip);
SANE_Status Asic_SetShadingTable (ASIC * chip, unsigned short * pWhiteShading,
				  unsigned short * pDarkShading,
				  unsigned short wXResolution,
				  unsigned short wWidth);

#endif
