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

#define BACKEND_NAME mustek_usb2
#define DEBUG_DECLARE_ONLY
#include "sane/config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "byteorder.h"
#include "sane/sane.h"
#include "sane/sanei_usb.h"
#include "sane/sanei_backend.h"
#include "sane/sanei_debug.h"

#include "mustek_usb2_asic.h"


const ASIC_ModelParams paramsMustekBP2448TAPro = {
  SDRAMCLK_DELAY_12_ns
};

const ASIC_ModelParams paramsMicrotek4800H48U = {
  SDRAMCLK_DELAY_8_ns
};


static SANE_Status PrepareScanChip (ASIC * chip);
static SANE_Status WaitCarriageHome (ASIC * chip);
static SANE_Status WaitUnitReady (ASIC * chip);


/* ---------------------- low level ASIC functions -------------------------- */

static SANE_Status
WriteIOControl (ASIC * chip, unsigned short wValue, unsigned short wIndex,
		unsigned short wLength, SANE_Byte * pBuf)
{
  SANE_Status status;

  status = sanei_usb_control_msg (chip->fd, 0x40, 0x01, wValue, wIndex, wLength,
				  pBuf);
  if (status != SANE_STATUS_GOOD)
    DBG (DBG_ERR, "WriteIOControl error: %s\n", sane_strstatus (status));

  return status;
}

static SANE_Status
ReadIOControl (ASIC * chip, unsigned short wValue, unsigned short wIndex,
	       unsigned short wLength, SANE_Byte * pBuf)
{
  SANE_Status status;

  status = sanei_usb_control_msg (chip->fd, 0xc0, 0x01, wValue, wIndex, wLength,
				  pBuf);
  if (status != SANE_STATUS_GOOD)
    DBG (DBG_ERR, "ReadIOControl error: %s\n", sane_strstatus (status));

  return status;
}

static SANE_Status
ClearFIFO (ASIC * chip)
{
  SANE_Status status;
  SANE_Byte buf[4];
  DBG_ASIC_ENTER ();

  buf[0] = 0;
  buf[1] = 0;
  buf[2] = 0;
  buf[3] = 0;

  status = WriteIOControl (chip, 0x05, 0, 4, buf);
  if (status != SANE_STATUS_GOOD)
    return status;

  status = WriteIOControl (chip, 0xc0, 0, 4, buf);

  DBG_ASIC_LEAVE ();
  return status;
}

static SANE_Status
SwitchBank (ASIC * chip, unsigned short reg)
{
  SANE_Status status;
  SANE_Byte buf[4];
  SANE_Byte bank;
  DBG_ASIC_ENTER ();

  bank = HIBYTE(reg);
  if (bank > SELECT_REGISTER_BANK2)
    {
      DBG (DBG_ERR, "invalid register %d\n", reg);
      return SANE_STATUS_IO_ERROR;
    }

  if (chip->RegisterBankStatus != bank)
    {
      buf[0] = ES01_5F_REGISTER_BANK_SELECT;
      buf[1] = bank;
      buf[2] = ES01_5F_REGISTER_BANK_SELECT;
      buf[3] = bank;
      status = WriteIOControl (chip, 0xb0, 0, 4, buf);
      if (status != SANE_STATUS_GOOD)
	return status;

      chip->RegisterBankStatus = bank;
      DBG (DBG_ASIC, "RegisterBankStatus=%d\n", chip->RegisterBankStatus);
    }

  DBG_ASIC_LEAVE ();
  return SANE_STATUS_GOOD;
}

static SANE_Status
SendData (ASIC * chip, unsigned short reg, SANE_Byte data)
{
  SANE_Status status;
  SANE_Byte buf[4];
  DBG_ASIC_ENTER ();
  DBG (DBG_ASIC, "reg=%x,data=%x\n", reg, data);

  status = SwitchBank (chip, reg);
  if (status != SANE_STATUS_GOOD)
    return status;

  buf[0] = LOBYTE (reg);
  buf[1] = data;
  buf[2] = LOBYTE (reg);
  buf[3] = data;
  status = WriteIOControl (chip, 0xb0, 0, 4, buf);

  DBG_ASIC_LEAVE ();
  return status;
}

static SANE_Status
ReceiveData (ASIC * chip, SANE_Byte * reg)
{
  SANE_Status status;
  SANE_Byte buf[4];
  DBG_ASIC_ENTER ();

  status = ReadIOControl (chip, 0x07, 0, 4, buf);
  *reg = buf[0];

  DBG_ASIC_LEAVE ();
  return status;
}

static SANE_Status
WriteAddressLineForRegister (ASIC * chip, SANE_Byte x)
{
  SANE_Status status;
  SANE_Byte buf[4];
  DBG_ASIC_ENTER ();

  buf[0] = x;
  buf[1] = x;
  buf[2] = x;
  buf[3] = x;
  status = WriteIOControl (chip, 0x04, x, 4, buf);

  DBG_ASIC_LEAVE ();
  return status;
}

static SANE_Status
SetRWSize (ASIC * chip, SANE_Bool isWriteAccess, unsigned int size)
{
  SANE_Status status;
  DBG_ASIC_ENTER ();

  if (!isWriteAccess)
    size >>= 1;

  status = SendData (chip, ES01_7C_DMA_SIZE_BYTE0, BYTE0 (size));
  if (status != SANE_STATUS_GOOD)
    return status;
  status = SendData (chip, ES01_7D_DMA_SIZE_BYTE1, BYTE1 (size));
  if (status != SANE_STATUS_GOOD)
    return status;
  status = SendData (chip, ES01_7E_DMA_SIZE_BYTE2, BYTE2 (size));
  if (status != SANE_STATUS_GOOD)
    return status;
  status = SendData (chip, ES01_7F_DMA_SIZE_BYTE3, BYTE3 (size));

  DBG_ASIC_LEAVE ();
  return status;
}

static SANE_Status
DMARead (ASIC * chip, unsigned int size, SANE_Byte * pData)
{
  SANE_Status status;
  size_t cur_read_size;
  DBG_ASIC_ENTER ();
  DBG (DBG_ASIC, "size=%d\n", size);

  status = ClearFIFO (chip);
  if (status != SANE_STATUS_GOOD)
    return status;

  while (size > 0)
    {
      cur_read_size = (size > DMA_BLOCK_SIZE) ? DMA_BLOCK_SIZE : size;

      status = SetRWSize (chip, SANE_FALSE, cur_read_size);
      if (status != SANE_STATUS_GOOD)
	return status;

      status = WriteIOControl (chip, 0x03, 0, 4, (SANE_Byte *) &cur_read_size);
      if (status != SANE_STATUS_GOOD)
	return status;

      status = sanei_usb_read_bulk (chip->fd, pData, &cur_read_size);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_ERR, "DMA read error\n");
	  return status;
	}

      size -= cur_read_size;
      pData += cur_read_size;
    }

  if (cur_read_size < DMA_BLOCK_SIZE)
    usleep (20000);

  DBG_ASIC_LEAVE ();
  return SANE_STATUS_GOOD;
}

static SANE_Status
DMAWrite (ASIC * chip, unsigned int size, SANE_Byte * pData)
{
  SANE_Status status;
  size_t cur_write_size;
  DBG_ASIC_ENTER ();
  DBG (DBG_ASIC, "size=%d\n", size);

  status = ClearFIFO (chip);
  if (status != SANE_STATUS_GOOD)
    return status;

  while (size > 0)
    {
      cur_write_size = (size > DMA_BLOCK_SIZE) ? DMA_BLOCK_SIZE : size;

      status = SetRWSize (chip, SANE_TRUE, cur_write_size);
      if (status != SANE_STATUS_GOOD)
	return status;

      status = WriteIOControl (chip, 0x02, 0, 4, (SANE_Byte *) &cur_write_size);
      if (status != SANE_STATUS_GOOD)
	return status;

      status = sanei_usb_write_bulk (chip->fd, pData, &cur_write_size);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_ERR, "DMA write error\n");
	  return status;
	}

      size -= cur_write_size;
      pData += cur_write_size;
    }

  status = ClearFIFO (chip);

  DBG_ASIC_LEAVE ();
  return status;
}

static SANE_Status
SendData2Byte (ASIC * chip, unsigned short reg, SANE_Byte data)
{
  SANE_Status status = SANE_STATUS_GOOD;
  DBG_ASIC_ENTER ();

  if (!chip->is2ByteTransfer)
    {
      chip->dataBuf[0] = LOBYTE (reg);
      chip->dataBuf[1] = data;
      chip->is2ByteTransfer = SANE_TRUE;
    }
  else
    {
      chip->dataBuf[2] = LOBYTE (reg);
      chip->dataBuf[3] = data;
      chip->is2ByteTransfer = SANE_FALSE;

      status = SwitchBank (chip, reg);
      if (status != SANE_STATUS_GOOD)
	return status;

      status = WriteIOControl (chip, 0xb0, 0, 4, chip->dataBuf);
    }

  DBG_ASIC_LEAVE ();
  return status;
}


/* ---------------------- ASIC motor functions ----------------------------- */

static SANE_Status
SetRamAddress (ASIC * chip, unsigned int dwStartAddr, unsigned int dwEndAddr,
	       RAM_TYPE AccessTarget)
{
  SANE_Status status;
  DBG_ASIC_ENTER ();

  /* Set start address. Unit is a word. */
  SendData (chip, ES01_A0_HostStartAddr0_7, BYTE0 (dwStartAddr));

  if (AccessTarget == ON_CHIP_FINAL_GAMMA)
    {
      SendData (chip, ES01_A1_HostStartAddr8_15,
		BYTE1 (dwStartAddr) | ACCESS_FINAL_GAMMA_ES01);
      SendData (chip, ES01_A2_HostStartAddr16_21,
		BYTE2 (dwStartAddr) | ACCESS_GAMMA_RAM);
    }
  else if (AccessTarget == ON_CHIP_PRE_GAMMA)
    {
      SendData (chip, ES01_A1_HostStartAddr8_15,
		BYTE1 (dwStartAddr) | ACCESS_PRE_GAMMA_ES01);
      SendData (chip, ES01_A2_HostStartAddr16_21,
		BYTE2 (dwStartAddr) | ACCESS_GAMMA_RAM);
    }
  else  /* DRAM */
    {
      SendData (chip, ES01_A1_HostStartAddr8_15, BYTE1 (dwStartAddr));
      SendData (chip, ES01_A2_HostStartAddr16_21,
		BYTE2 (dwStartAddr) | ACCESS_DRAM);
    }

  /* Set end address. */
  SendData (chip, ES01_A3_HostEndAddr0_7, BYTE0 (dwEndAddr));
  SendData (chip, ES01_A4_HostEndAddr8_15, BYTE1 (dwEndAddr));
  SendData (chip, ES01_A5_HostEndAddr16_21, BYTE2 (dwEndAddr));

  status = ClearFIFO (chip);

  DBG_ASIC_LEAVE ();
  return status;
}

static SANE_Status
RamAccess (ASIC * chip, RAMACCESS * access)
{
  SANE_Status status;
  SANE_Byte a[2];
  DBG_ASIC_ENTER ();

  status = SetRamAddress (chip, access->StartAddress, 0xffffff,
			  access->RamType);
  if (status != SANE_STATUS_GOOD)
    return status;

  SendData (chip, ES01_79_AFEMCLK_SDRAMCLK_DELAY_CONTROL,
	    chip->params->SDRAM_Delay);

  if (access->IsWriteAccess)
    {
      status = DMAWrite (chip, access->RwSize, access->BufferPtr);
      if (status != SANE_STATUS_GOOD)
	return status;

      /* steal read 2 byte */
      usleep (20000);
      access->RwSize = 2;
      access->BufferPtr = a;
      access->IsWriteAccess = SANE_FALSE;
      status = RamAccess (chip, access);
      DBG (DBG_ASIC, "stole 2 bytes!\n");
    }
  else
    {
      status = DMARead (chip, access->RwSize, access->BufferPtr);
    }

  DBG_ASIC_LEAVE ();
  return status;
}

static void
SetMotorCurrentAndPhase (ASIC * chip,
			 MOTOR_CURRENT_AND_PHASE * MotorCurrentAndPhase)
{
  static const SANE_Byte MotorPhaseFullStep[] =
    { 0x08, 0x09, 0x01, 0x00 };
  static const SANE_Byte MotorPhaseHalfStep[] =
    { 0x25, 0x07, 0x24, 0x30, 0x2c, 0x0e, 0x2d, 0x39 };

  SANE_Byte MotorPhaseMask;
  int i;
  DBG_ASIC_ENTER ();

  if (MotorCurrentAndPhase->MotorDriverIs3967 == 1)
    MotorPhaseMask = 0xFE;
  else
    MotorPhaseMask = 0xFF;
  DBG (DBG_ASIC, "MotorPhaseMask=0x%x\n", MotorPhaseMask);

  SendData (chip, ES02_50_MOTOR_CURRENT_CONTORL, DOWN_LOAD_MOTOR_TABLE_ENABLE);

  if (MotorCurrentAndPhase->MoveType == _4_TABLE_SPACE_FOR_FULL_STEP)
    {
      SendData (chip, ES01_AB_PWM_CURRENT_CONTROL,
		MOTOR_PWM_CURRENT_0 | MOTOR1_GPO_VALUE_0);

      for (i = 0; i < 4; i++)
	{
	  SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
			 MotorCurrentAndPhase->MotorCurrent);
	  SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
			 MotorCurrentAndPhase->MotorCurrent);
	  SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
			 MotorPhaseFullStep[i] & MotorPhaseMask);
	}
    }
  else if (MotorCurrentAndPhase->MoveType == _8_TABLE_SPACE_FOR_1_DIV_2_STEP)
    {
      SendData (chip, ES01_AB_PWM_CURRENT_CONTROL,
		MOTOR_PWM_CURRENT_1 | MOTOR1_GPO_VALUE_0);

      for (i = 0; i < 8; i++)
	{
	  SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
			 MotorCurrentAndPhase->MotorCurrent);
	  SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
			 MotorCurrentAndPhase->MotorCurrent);
	  SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
			 MotorPhaseHalfStep[i] & MotorPhaseMask);
	}
    }
  else if (MotorCurrentAndPhase->MoveType == _16_TABLE_SPACE_FOR_1_DIV_4_STEP)
    {
      SendData (chip, ES01_AB_PWM_CURRENT_CONTROL,
		MOTOR_PWM_CURRENT_2 | MOTOR1_GPO_VALUE_0);

      for (i = 0; i < 16; i++)
	{
	  double x = (((i % 4) * M_PI * 90) / 4) / 180;
	  SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A, (SANE_Byte)
			 (MotorCurrentAndPhase->MotorCurrent * sin (x)));
	  SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B, (SANE_Byte)
			 (MotorCurrentAndPhase->MotorCurrent * cos (x)));
	  SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
			 MotorPhaseFullStep[i / 4] & MotorPhaseMask);
	}
    }
  else if (MotorCurrentAndPhase->MoveType == _32_TABLE_SPACE_FOR_1_DIV_8_STEP)
    {
      SendData (chip, ES01_AB_PWM_CURRENT_CONTROL,
		MOTOR_PWM_CURRENT_3 | MOTOR1_GPO_VALUE_0);

      for (i = 0; i < 32; i++)
	{
	  double x = (((i % 8) * M_PI * 90) / 8) / 180;
	  SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A, (SANE_Byte)
			 (MotorCurrentAndPhase->MotorCurrent * sin (x)));
	  SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B, (SANE_Byte)
			 (MotorCurrentAndPhase->MotorCurrent * cos (x)));
	  SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
			 MotorPhaseFullStep[i / 8] & MotorPhaseMask);
	}
    }

  SendData (chip, ES02_50_MOTOR_CURRENT_CONTORL,
	    MotorCurrentAndPhase->MoveType);

  DBG_ASIC_LEAVE ();
}

static SANE_Status
SetMotorTable (ASIC * chip, unsigned int dwTableBaseAddr,
	       unsigned short * MotorTablePtr)
{
  SANE_Status status;
  RAMACCESS access;
  DBG_ASIC_ENTER ();

  access.IsWriteAccess = SANE_TRUE;
  access.RamType = EXTERNAL_RAM;
  access.StartAddress = dwTableBaseAddr;
  access.RwSize = MOTOR_TABLE_SIZE * sizeof (unsigned short);
  access.BufferPtr = (SANE_Byte *) MotorTablePtr;

  status = RamAccess (chip, &access);

  /* tell scan chip the motor table address, unit is 2^14 words */
  SendData (chip, ES01_9D_MotorTableAddrA14_A21,
	    (SANE_Byte) (dwTableBaseAddr >> TABLE_OFFSET_BASE));

  DBG_ASIC_LEAVE ();
  return status;
}

static SANE_Status
SetShadingTable (ASIC * chip, unsigned int dwTableBaseAddr,
		 unsigned int dwTableSize, unsigned short * ShadingTablePtr)
{
  SANE_Status status;
  RAMACCESS access;
  DBG_ASIC_ENTER ();

  access.IsWriteAccess = SANE_TRUE;
  access.RamType = EXTERNAL_RAM;
  access.StartAddress = dwTableBaseAddr;
  access.RwSize = dwTableSize;
  access.BufferPtr = (SANE_Byte *) ShadingTablePtr;

  status = RamAccess (chip, &access);

  /* tell scan chip the shading table address, unit is 2^14 words */
  SendData (chip, ES01_9B_ShadingTableAddrA14_A21,
	    (SANE_Byte) (dwTableBaseAddr >> TABLE_OFFSET_BASE));

  DBG_ASIC_LEAVE ();
  return status;
}

static SANE_Status
MotorMove (ASIC * chip, MOTORMOVE * Move)
{
  SANE_Status status;
  unsigned int motor_steps;
  SANE_Byte temp_motor_action;
  DBG_ASIC_ENTER ();

  status = PrepareScanChip (chip);
  if (status != SANE_STATUS_GOOD)
    return status;

  DBG (DBG_ASIC, "set start/end pixel\n");

  SendData (chip, ES01_B8_ChannelRedExpStartPixelLSB, LOBYTE (100));
  SendData (chip, ES01_B9_ChannelRedExpStartPixelMSB, HIBYTE (100));
  SendData (chip, ES01_BA_ChannelRedExpEndPixelLSB, LOBYTE (101));
  SendData (chip, ES01_BB_ChannelRedExpEndPixelMSB, HIBYTE (101));

  SendData (chip, ES01_BC_ChannelGreenExpStartPixelLSB, LOBYTE (100));
  SendData (chip, ES01_BD_ChannelGreenExpStartPixelMSB, HIBYTE (100));
  SendData (chip, ES01_BE_ChannelGreenExpEndPixelLSB, LOBYTE (101));
  SendData (chip, ES01_BF_ChannelGreenExpEndPixelMSB, HIBYTE (101));

  SendData (chip, ES01_C0_ChannelBlueExpStartPixelLSB, LOBYTE (100));
  SendData (chip, ES01_C1_ChannelBlueExpStartPixelMSB, HIBYTE (100));
  SendData (chip, ES01_C2_ChannelBlueExpEndPixelLSB, LOBYTE (101));
  SendData (chip, ES01_C3_ChannelBlueExpEndPixelMSB, HIBYTE (101));

  /* set motor acceleration steps, max. 511 steps */
  SendData (chip, ES01_E0_MotorAccStep0_7, LOBYTE (Move->AccStep));
  SendData (chip, ES01_E1_MotorAccStep8_8, HIBYTE (Move->AccStep));
  DBG (DBG_ASIC, "AccStep=%d\n", Move->AccStep);

  /* set motor deceleration steps, max. 255 steps */
  SendData (chip, ES01_E5_MotorDecStep, Move->DecStep);
  DBG (DBG_ASIC, "DecStep=%d\n", Move->DecStep);

  /* Set motor uniform speed.
     Only used for UNIFORM_MOTOR_AND_SCAN_SPEED_ENABLE. If you use acc mode,
     these two registers are not used. */
  SendData (chip, ES01_FD_MotorFixedspeedLSB, LOBYTE (Move->FixMoveSpeed));
  SendData (chip, ES01_FE_MotorFixedspeedMSB, HIBYTE (Move->FixMoveSpeed));
  DBG (DBG_ASIC, "FixMoveSpeed=%d\n", Move->FixMoveSpeed);

  /* set motor type */
  SendData (chip, ES01_A6_MotorOption,
	    MOTOR_0_ENABLE | HOME_SENSOR_0_ENABLE | TABLE_DEFINE_ES03);

  /* Set motor speed unit for all motor modes, including uniform. */
  SendData (chip, ES01_F6_MotorControl1,
	    SPEED_UNIT_1_PIXEL_TIME | MOTOR_SYNC_UNIT_1_PIXEL_TIME);

  if (Move->ActionType == ACTION_TYPE_BACKTOHOME)
    {
      DBG (DBG_ASIC, "ACTION_TYPE_BACKTOHOME\n");
      temp_motor_action = MOTOR_BACK_HOME_AFTER_SCAN_ENABLE;
      motor_steps = 30000 * 2;
    }
  else
    {
      DBG (DBG_ASIC, "forward or backward\n");
      temp_motor_action = MOTOR_MOVE_TO_FIRST_LINE_ENABLE;
      motor_steps = Move->FixMoveSteps;

      if (Move->ActionType == ACTION_TYPE_BACKWARD)
	{
	  DBG (DBG_ASIC, "ACTION_TYPE_BACKWARD\n");
	  temp_motor_action |= INVERT_MOTOR_DIRECTION_ENABLE;
	}
    }

  SendData (chip, ES01_94_PowerSaveControl,
	    TIMER_POWER_SAVE_ENABLE |
	    USB_POWER_SAVE_ENABLE | USB_REMOTE_WAKEUP_ENABLE |
	    LED_MODE_FLASH_SLOWLY);

  /* set number of movement steps with fixed speed */
  SendData (chip, ES01_E2_MotorStepOfMaxSpeed0_7, BYTE0 (motor_steps));
  SendData (chip, ES01_E3_MotorStepOfMaxSpeed8_15, BYTE1 (motor_steps));
  SendData (chip, ES01_E4_MotorStepOfMaxSpeed16_19, BYTE2 (motor_steps));
  DBG (DBG_ASIC, "motor_steps=%d\n", motor_steps);

  if (Move->ActionMode == ACTION_MODE_UNIFORM_SPEED_MOVE)
    temp_motor_action |= UNIFORM_MOTOR_AND_SCAN_SPEED_ENABLE;

  SendData (chip, ES01_F3_ActionOption, temp_motor_action);
  SendData (chip, ES01_F4_ActiveTrigger, ACTION_TRIGGER_ENABLE);

  if (Move->ActionType == ACTION_TYPE_BACKTOHOME)
    {
      status = WaitCarriageHome (chip);
      if (status != SANE_STATUS_GOOD)
	return status;
    }

  status = WaitUnitReady (chip);

  DBG_ASIC_LEAVE ();
  return status;
}

static void
SetMotorStepTable (ASIC * chip, MOTORMOVE * MotorStepsTable,
		   unsigned short wStartY, unsigned int dwScanImageSteps,
		   unsigned short wYResolution)
{
  unsigned short wAccSteps = 511;
  unsigned short wForwardSteps = 20;
  SANE_Byte bDecSteps = 255;
  unsigned short wScanAccSteps = 511;
  unsigned short wFixScanSteps = 20;
  SANE_Byte bScanDecSteps = 255;
  unsigned short wScanBackTrackingSteps = 40;
  unsigned short wScanRestartSteps = 40;
  DBG_ASIC_ENTER ();

  switch (wYResolution)
    {
    case 2400:
    case 1200:
      wScanAccSteps = 100;
      bScanDecSteps = 10;
      wScanBackTrackingSteps = 10;
      wScanRestartSteps = 10;
      break;
    case 600:
    case 300:
      wScanAccSteps = 300;
      bScanDecSteps = 40;
      break;
    case 150:
      wScanAccSteps = 300;
      bScanDecSteps = 40;
      break;
    case 100:
    case 75:
    case 50:
      wScanAccSteps = 300;
      bScanDecSteps = 40;
      break;
    }

  /* not including T0,T1 steps */
  if (wStartY < (wAccSteps + wForwardSteps + bDecSteps + wScanAccSteps))
    {
      wAccSteps = 1;
      bDecSteps = 1;
      wFixScanSteps = (short)(wStartY - wScanAccSteps) > 0 ?
	(wStartY - wScanAccSteps) : 0;
      wForwardSteps = 0;

      chip->isMotorMoveToFirstLine = 0;
    }
  else
    {
      wForwardSteps = (short)(wStartY - wAccSteps - (unsigned short) bDecSteps -
	wScanAccSteps - wFixScanSteps) > 0 ?
	(wStartY - wAccSteps - (unsigned short) bDecSteps - wScanAccSteps -
	 wFixScanSteps) : 0;

      chip->isMotorMoveToFirstLine = MOTOR_MOVE_TO_FIRST_LINE_ENABLE;
    }

  dwScanImageSteps += wAccSteps;
  dwScanImageSteps += wForwardSteps;
  dwScanImageSteps += bDecSteps;
  dwScanImageSteps += wScanAccSteps;
  dwScanImageSteps += wFixScanSteps;
  dwScanImageSteps += bScanDecSteps;
  dwScanImageSteps += 2;

  MotorStepsTable->wScanAccSteps = wScanAccSteps;
  MotorStepsTable->bScanDecSteps = bScanDecSteps;

  /* state 1 */
  SendData (chip, ES01_E0_MotorAccStep0_7, LOBYTE (wAccSteps));
  SendData (chip, ES01_E1_MotorAccStep8_8, HIBYTE (wAccSteps));
  /* state 2 */
  SendData (chip, ES01_E2_MotorStepOfMaxSpeed0_7, LOBYTE (wForwardSteps));
  SendData (chip, ES01_E3_MotorStepOfMaxSpeed8_15, HIBYTE (wForwardSteps));
  SendData (chip, ES01_E4_MotorStepOfMaxSpeed16_19, 0);
  /* state 3 */
  SendData (chip, ES01_E5_MotorDecStep, bDecSteps);
  /* state 4 */
  SendData (chip, ES01_AE_MotorSyncPixelNumberM16LSB, 0);
  SendData (chip, ES01_AF_MotorSyncPixelNumberM16MSB, 0);
  /* state 5 */
  SendData (chip, ES01_EC_ScanAccStep0_7, LOBYTE (wScanAccSteps));
  SendData (chip, ES01_ED_ScanAccStep8_8, HIBYTE (wScanAccSteps));
  /* state 6 */
  SendData (chip, ES01_EE_FixScanStepLSB, LOBYTE (wFixScanSteps));
  SendData (chip, ES01_8A_FixScanStepMSB, HIBYTE (wFixScanSteps));
  /* state 8 */
  SendData (chip, ES01_EF_ScanDecStep, bScanDecSteps);
  /* state 10 */
  SendData (chip, ES01_E6_ScanBackTrackingStepLSB,
	    LOBYTE (wScanBackTrackingSteps));
  SendData (chip, ES01_E7_ScanBackTrackingStepMSB,
	    HIBYTE (wScanBackTrackingSteps));
  /* state 15 */
  SendData (chip, ES01_E8_ScanRestartStepLSB, LOBYTE (wScanRestartSteps));
  SendData (chip, ES01_E9_ScanRestartStepMSB, HIBYTE (wScanRestartSteps));
  /* state 19 */
  SendData (chip, ES01_EA_ScanBackHomeExtStepLSB, LOBYTE (100));
  SendData (chip, ES01_EB_ScanBackHomeExtStepMSB, HIBYTE (100));

  /* total motor steps */
  SendData (chip, ES01_F0_ScanImageStep0_7, BYTE0 (dwScanImageSteps));
  SendData (chip, ES01_F1_ScanImageStep8_15, BYTE1 (dwScanImageSteps));
  SendData (chip, ES01_F2_ScanImageStep16_19, BYTE2 (dwScanImageSteps));

  DBG_ASIC_LEAVE ();
}

static void
SetMotorStepTableForCalibration (ASIC * chip, MOTORMOVE * MotorStepsTable,
				 unsigned int dwScanImageSteps)
{
  unsigned short wScanAccSteps = 1;
  SANE_Byte bScanDecSteps = 1;
  unsigned short wFixScanSteps = 0;
  unsigned short wScanBackTrackingSteps = 20;
  DBG_ASIC_ENTER ();

  chip->isMotorMoveToFirstLine = 0;

  dwScanImageSteps += wScanAccSteps;
  dwScanImageSteps += wFixScanSteps;
  dwScanImageSteps += bScanDecSteps;

  MotorStepsTable->wScanAccSteps = wScanAccSteps;
  MotorStepsTable->bScanDecSteps = bScanDecSteps;

  /* state 4 */
  SendData (chip, ES01_AE_MotorSyncPixelNumberM16LSB, 0);
  SendData (chip, ES01_AF_MotorSyncPixelNumberM16MSB, 0);
  /* state 5 */
  SendData (chip, ES01_EC_ScanAccStep0_7, LOBYTE (wScanAccSteps));
  SendData (chip, ES01_ED_ScanAccStep8_8, HIBYTE (wScanAccSteps));
  /* state 6 */
  SendData (chip, ES01_EE_FixScanStepLSB, LOBYTE (wFixScanSteps));
  SendData (chip, ES01_8A_FixScanStepMSB, HIBYTE (wFixScanSteps));
  /* state 8 */
  SendData (chip, ES01_EF_ScanDecStep, bScanDecSteps);
  /* state 10 */
  SendData (chip, ES01_E6_ScanBackTrackingStepLSB,
	    LOBYTE (wScanBackTrackingSteps));
  SendData (chip, ES01_E7_ScanBackTrackingStepMSB,
	    HIBYTE (wScanBackTrackingSteps));
  /* state 15 */
  SendData (chip, ES01_E8_ScanRestartStepLSB,
	    LOBYTE (wScanBackTrackingSteps));
  SendData (chip, ES01_E9_ScanRestartStepMSB,
	    HIBYTE (wScanBackTrackingSteps));

  SendData (chip, ES01_F0_ScanImageStep0_7, BYTE0 (dwScanImageSteps));
  SendData (chip, ES01_F1_ScanImageStep8_15, BYTE1 (dwScanImageSteps));
  SendData (chip, ES01_F2_ScanImageStep16_19, BYTE2 (dwScanImageSteps));

  DBG_ASIC_LEAVE ();
}

static void
CalculateScanMotorTable (CALCULATEMOTORTABLE * pCalculateMotorTable)
{
  unsigned short wEndSpeed, wStartSpeed;
  unsigned short wScanAccSteps;
  SANE_Byte bScanDecSteps;
  unsigned short * pMotorTable;
  long double y;
  unsigned short i;

  wStartSpeed = pCalculateMotorTable->StartSpeed;
  wEndSpeed = pCalculateMotorTable->EndSpeed;
  wScanAccSteps = pCalculateMotorTable->AccStepBeforeScan;
  bScanDecSteps = pCalculateMotorTable->DecStepAfterScan;
  pMotorTable = pCalculateMotorTable->pMotorTable;

  /* motor T0 & T6 acc table */
  for (i = 0; i < 512; i++)
    {
      y = 6000 - 3500;
      y *= pow (0.09, (M_PI_2 * i) / 512) - pow (0.09, (M_PI_2 * 511) / 512);
      y += 4500;
      pMotorTable[i] = htole16 ((unsigned short) y);		/* T0 */
      pMotorTable[i + 512 * 6] = htole16 ((unsigned short) y);	/* T6 */
    }

  /* motor T1 & T7 dec table */
  for (i = 0; i < 256; i++)
    {
      y = 6000 - 3500;
      y *= pow (0.3, (M_PI_2 * i) / 256);
      y = 6000 - y;
      pMotorTable[i + 512] = htole16 ((unsigned short) y);	/* T1 */
      pMotorTable[i + 512 * 7] = htole16 ((unsigned short) y);	/* T7 */
    }

  for (i = 0; i < wScanAccSteps; i++)
    {
      y = wStartSpeed - wEndSpeed;
      y *= pow (0.09, (M_PI_2 * i) / wScanAccSteps) -
	   pow (0.09, (M_PI_2 * (wScanAccSteps - 1)) / wScanAccSteps);
      y += wEndSpeed;
      pMotorTable[i + 512 * 2] = htole16 ((unsigned short) y);	/* T2 */
      pMotorTable[i + 512 * 4] = htole16 ((unsigned short) y);	/* T4 */
    }
  for (i = wScanAccSteps; i < 512; i++)
    {
      pMotorTable[i + 512 * 2] = htole16 (wEndSpeed);	/* T2 */
      pMotorTable[i + 512 * 4] = htole16 (wEndSpeed);	/* T4 */
    }

  for (i = 0; i < (unsigned short) bScanDecSteps; i++)
    {
      y = wStartSpeed - wEndSpeed;
      y *= pow (0.3, (M_PI_2 * i) / bScanDecSteps);
      y = wStartSpeed - y;
      pMotorTable[i + 512 * 3] = htole16 ((unsigned short) y);	/* T3 */
      pMotorTable[i + 512 * 5] = htole16 ((unsigned short) y);	/* T5 */
    }
  for (i = bScanDecSteps; i < 256; i++)
    {
      pMotorTable[i + 512 * 3] = htole16 (wStartSpeed);	/* T3 */
      pMotorTable[i + 512 * 5] = htole16 (wStartSpeed);	/* T5 */
    }
}

static void
CalculateMoveMotorTable (CALCULATEMOTORTABLE * pCalculateMotorTable)
{
  unsigned short wEndSpeed, wStartSpeed;
  unsigned short wScanAccSteps;
  unsigned short * pMotorTable;
  long double y;
  unsigned short i;

  wStartSpeed = pCalculateMotorTable->StartSpeed;
  wEndSpeed = pCalculateMotorTable->EndSpeed;
  wScanAccSteps = pCalculateMotorTable->AccStepBeforeScan;
  pMotorTable = pCalculateMotorTable->pMotorTable;

  for (i = 0; i < 512; i++)
    {
      /* before scan acc table */
      y = (wStartSpeed - wEndSpeed) *
	pow (0.09, (M_PI_2 * i) / 512) + wEndSpeed;
      pMotorTable[i] = htole16 ((unsigned short) y);
      pMotorTable[i + 512 * 2] = htole16 ((unsigned short) y);
      pMotorTable[i + 512 * 4] = htole16 ((unsigned short) y);
      pMotorTable[i + 512 * 6] = htole16 ((unsigned short) y);
    }

  for (i = 0; i < 256; i++)
    {
      y = wStartSpeed - (wStartSpeed - wEndSpeed) *
	pow (0.3, (M_PI_2 * i) / 256);
      pMotorTable[i + 512] = htole16 ((unsigned short) y);
      pMotorTable[i + 512 * 3] = htole16 ((unsigned short) y);
      pMotorTable[i + 512 * 5] = htole16 ((unsigned short) y);
      pMotorTable[i + 512 * 7] = htole16 ((unsigned short) y);
    }

  for (i = 0; i < wScanAccSteps; i++)
    {
      y = (wStartSpeed - wEndSpeed) *
	(pow (0.09, (M_PI_2 * i) / wScanAccSteps) -
	 pow (0.09, (M_PI_2 * (wScanAccSteps - 1)) / wScanAccSteps)) +
	wEndSpeed;
      pMotorTable[i + 512 * 2] = htole16 ((unsigned short) y);
    }
}

static SANE_Byte
CalculateMotorCurrent (unsigned short dwMotorSpeed)
{
  if (dwMotorSpeed < 2000)
    return 255;
  else if (dwMotorSpeed < 3500)
    return 200;
  else if (dwMotorSpeed < 5000)
    return 160;
  else if (dwMotorSpeed < 10000)
    return 70;
  else if (dwMotorSpeed < 17000)
    return 60;
  else	/* >= 17000 */
    return 50;
}

static SANE_Status
SimpleMotorMove (ASIC * chip,
		 unsigned short wStartSpeed, unsigned short wEndSpeed,
		 unsigned int dwFixMoveSpeed, SANE_Byte bMotorCurrent,
		 unsigned int dwTotalSteps, SANE_Byte bActionType)
{
  SANE_Status status;
  unsigned short * MotorTable;
  CALCULATEMOTORTABLE CalMotorTable;
  MOTOR_CURRENT_AND_PHASE CurrentPhase;
  MOTORMOVE Move;
  DBG_ASIC_ENTER ();

  MotorTable = malloc (MOTOR_TABLE_SIZE * sizeof (unsigned short));
  if (!MotorTable)
    {
      DBG (DBG_ASIC, "MotorTable == NULL\n");
      return SANE_STATUS_NO_MEM;
    }

  CalMotorTable.StartSpeed = wStartSpeed;
  CalMotorTable.EndSpeed = wEndSpeed;
  CalMotorTable.AccStepBeforeScan = 511;
  CalMotorTable.pMotorTable = MotorTable;
  CalculateMoveMotorTable (&CalMotorTable);

  CurrentPhase.MoveType = _4_TABLE_SPACE_FOR_FULL_STEP;
  CurrentPhase.MotorDriverIs3967 = 0;
  CurrentPhase.MotorCurrent = bMotorCurrent;
  SetMotorCurrentAndPhase (chip, &CurrentPhase);

  status = SetMotorTable (chip, 0x3000, MotorTable);
  free (MotorTable);
  if (status != SANE_STATUS_GOOD)
    return status;

  Move.ActionType = bActionType;
  if (dwTotalSteps >= 766)
    {
      Move.ActionMode = ACTION_MODE_ACCDEC_MOVE;
      Move.AccStep = 511;
      Move.DecStep = 255;
    }
  else
    {
      Move.ActionMode = ACTION_MODE_UNIFORM_SPEED_MOVE;
      Move.AccStep = 1;
      Move.DecStep = 1;
    }
  Move.FixMoveSteps = dwTotalSteps - (Move.AccStep + Move.DecStep);
  Move.FixMoveSpeed = dwFixMoveSpeed;
  status = MotorMove (chip, &Move);

  DBG_ASIC_LEAVE ();
  return status;
}


/* ---------------------- medium level ASIC functions ---------------------- */

static void
InitTiming (ASIC * chip)
{
  chip->Timing.AFE_ADCCLK_Timing = 0x3c3c3c00;
  chip->Timing.AFE_ADCVS_Timing  = 0x00c00000;
  chip->Timing.AFE_ADCRS_Timing  = 0x00000c00;
  chip->Timing.AFE_ChannelA_LatchPos = 3080;
  chip->Timing.AFE_ChannelB_LatchPos = 3602;
  chip->Timing.AFE_ChannelC_LatchPos = 5634;
  chip->Timing.AFE_ChannelD_LatchPos = 1546;
  chip->Timing.AFE_Secondary_FF_LatchPos = 12;

  chip->Timing.CCD_DummyCycleTiming = 0;
  chip->Timing.PHTG_PulseWidth = 12;
  chip->Timing.PHTG_WaitWidth = 1;
  chip->Timing.PHTG_TimingAdj = 1;
  chip->Timing.PHTG_TimingSetup = 0;
  chip->Timing.ChannelR_StartPixel = 100;
  chip->Timing.ChannelR_EndPixel = 200;
  chip->Timing.ChannelG_StartPixel = 100;
  chip->Timing.ChannelG_EndPixel = 200;
  chip->Timing.ChannelB_StartPixel = 100;
  chip->Timing.ChannelB_EndPixel = 200;

  chip->Timing.CCD_PH2_Timing  = 0x000fff00;
  chip->Timing.CCD_PHRS_Timing = 0x000f0000;
  chip->Timing.CCD_PHCP_Timing = 0x0000f000;
  chip->Timing.CCD_PH1_Timing  = 0xfff00000;

  chip->Timing.wCCDPixelNumber_Full = 11250;
  chip->Timing.wCCDPixelNumber_Half = 7500;
}

static SANE_Status
OpenScanChip (ASIC * chip)
{
  SANE_Status status;
  SANE_Byte x[4];
  DBG_ASIC_ENTER ();

  x[0] = 0x64;
  x[1] = 0x64;
  x[2] = 0x64;
  x[3] = 0x64;
  status = WriteIOControl (chip, 0x90, 0, 4, x);
  if (status != SANE_STATUS_GOOD)
    return status;

  x[0] = 0x65;
  x[1] = 0x65;
  x[2] = 0x65;
  x[3] = 0x65;
  status = WriteIOControl (chip, 0x90, 0, 4, x);
  if (status != SANE_STATUS_GOOD)
    return status;

  x[0] = 0x44;
  x[1] = 0x44;
  x[2] = 0x44;
  x[3] = 0x44;
  status = WriteIOControl (chip, 0x90, 0, 4, x);
  if (status != SANE_STATUS_GOOD)
    return status;

  x[0] = 0x45;
  x[1] = 0x45;
  x[2] = 0x45;
  x[3] = 0x45;
  status = WriteIOControl (chip, 0x90, 0, 4, x);

  DBG_ASIC_LEAVE ();
  return status;
}

static SANE_Status
CloseScanChip (ASIC * chip)
{
  SANE_Status status;
  SANE_Byte x[4];
  DBG_ASIC_ENTER ();

  x[0] = 0x64;
  x[1] = 0x64;
  x[2] = 0x64;
  x[3] = 0x64;
  status = WriteIOControl (chip, 0x90, 0, 4, x);
  if (status != SANE_STATUS_GOOD)
    return status;

  x[0] = 0x65;
  x[1] = 0x65;
  x[2] = 0x65;
  x[3] = 0x65;
  status = WriteIOControl (chip, 0x90, 0, 4, x);
  if (status != SANE_STATUS_GOOD)
    return status;

  x[0] = 0x16;
  x[1] = 0x16;
  x[2] = 0x16;
  x[3] = 0x16;
  status = WriteIOControl (chip, 0x90, 0, 4, x);
  if (status != SANE_STATUS_GOOD)
    return status;

  x[0] = 0x17;
  x[1] = 0x17;
  x[2] = 0x17;
  x[3] = 0x17;
  status = WriteIOControl (chip, 0x90, 0, 4, x);

  DBG_ASIC_LEAVE ();
  return status;
}

static SANE_Status
PrepareScanChip (ASIC * chip)
{
  SANE_Status status;
  DBG_ASIC_ENTER ();

  SendData (chip, ES01_F3_ActionOption, 0);
  SendData (chip, ES01_86_DisableAllClockWhenIdle, 0);
  SendData (chip, ES01_F4_ActiveTrigger, 0);

  status = WaitUnitReady (chip);

  DBG_ASIC_LEAVE ();
  return status;
}

static SANE_Status
TestDRAM (ASIC * chip)
{
  SANE_Status status;
  RAMACCESS access;
  SANE_Byte buf[DRAM_TEST_SIZE];
  unsigned int i;
  DBG_ASIC_ENTER ();

  for (i = 0; i < sizeof (buf); i++)
    buf[i] = i;

  access.IsWriteAccess = SANE_TRUE;
  access.RamType = EXTERNAL_RAM;
  access.StartAddress = 0;
  access.RwSize = sizeof (buf);
  access.BufferPtr = buf;

  status = RamAccess (chip, &access);
  if (status != SANE_STATUS_GOOD)
    return status;

  memset (buf, 0, sizeof (buf));

  access.IsWriteAccess = SANE_FALSE;

  status = RamAccess (chip, &access);
  if (status != SANE_STATUS_GOOD)
    return status;

  for (i = 0; i < sizeof (buf); i++)
    {
      if (buf[i] != i)
	{
	  DBG (DBG_ERR, "DRAM test error at offset %d\n", i);
	  return SANE_STATUS_IO_ERROR;
	}
    }

  DBG_ASIC_LEAVE ();
  return SANE_STATUS_GOOD;
}

static SANE_Status
SafeInitialChip (ASIC * chip)
{
  SANE_Status status;
  DBG_ASIC_ENTER ();

  status = PrepareScanChip (chip);
  if (status != SANE_STATUS_GOOD)
    return status;

  DBG (DBG_ASIC, "isFirstOpenChip=%d\n", chip->isFirstOpenChip);
  if (chip->isFirstOpenChip)
    {
      status = TestDRAM (chip);
      if (status != SANE_STATUS_GOOD)
	return status;
      chip->isFirstOpenChip = SANE_FALSE;
    }

  DBG_ASIC_LEAVE ();
  return status;
}

static SANE_Status
GetChipStatus (ASIC * chip, SANE_Byte Selector, SANE_Byte * ChipStatus)
{
  SANE_Status status;
  DBG_ASIC_ENTER ();

  status = SendData (chip, ES01_8B_Status, Selector);
  if (status != SANE_STATUS_GOOD)
    return status;

  status = WriteAddressLineForRegister (chip, ES01_8B_Status);
  if (status != SANE_STATUS_GOOD)
    return status;

  if (ChipStatus)
    status = ReceiveData (chip, ChipStatus);

  DBG_ASIC_LEAVE ();
  return status;
}

static SANE_Status
IsCarriageHome (ASIC * chip, SANE_Bool * LampHome)
{
  SANE_Status status;
  SANE_Byte temp;
  DBG_ASIC_ENTER ();

  status = GetChipStatus (chip, H1H0L1L0_PS_MJ, &temp);
  if (status != SANE_STATUS_GOOD)
    return status;

  *LampHome = (temp & SENSOR0_DETECTED) ? SANE_TRUE : SANE_FALSE;
  DBG (DBG_ASIC, "LampHome=%d\n", *LampHome);

  DBG_ASIC_LEAVE ();
  return SANE_STATUS_GOOD;
}

static SANE_Status
WaitCarriageHome (ASIC * chip)
{
  SANE_Status status;
  SANE_Bool LampHome;
  int i;
  DBG_ASIC_ENTER ();

  for (i = 0; i < 100; i++)
    {
      status = IsCarriageHome (chip, &LampHome);
      if (status != SANE_STATUS_GOOD)
	return status;
      if (LampHome)
	break;
      usleep (300000);
    }
  DBG (DBG_ASIC, "waited %d s\n", (unsigned short) (i * 0.3));

  status = SendData (chip, ES01_F4_ActiveTrigger, 0);
  if ((status == SANE_STATUS_GOOD) && (i == 100))
    status = SANE_STATUS_DEVICE_BUSY;

  DBG_ASIC_LEAVE ();
  return status;
}

static SANE_Status
WaitUnitReady (ASIC * chip)
{
  SANE_Status status;
  SANE_Byte temp;
  int i;
  DBG_ASIC_ENTER ();

  for (i = 0; i < 300; i++)
    {
      status = GetChipStatus (chip, SCAN_STATE, &temp);
      if (status != SANE_STATUS_GOOD)
	return status;
      if ((temp & 0x1f) == 0)
	break;
      usleep (100000);
    }
  DBG (DBG_ASIC, "waited %d s\n", (unsigned short) (i * 0.1));

  status = SendData (chip, ES01_F4_ActiveTrigger, 0);

  DBG_ASIC_LEAVE ();
  return status;
}


static void
SetCCDTiming (ASIC * chip)
{
  DBG_ASIC_ENTER ();

  SendData (chip, ES01_82_AFE_ADCCLK_TIMING_ADJ_BYTE0,
	    BYTE0 (chip->Timing.AFE_ADCCLK_Timing));
  SendData (chip, ES01_83_AFE_ADCCLK_TIMING_ADJ_BYTE1,
	    BYTE1 (chip->Timing.AFE_ADCCLK_Timing));
  SendData (chip, ES01_84_AFE_ADCCLK_TIMING_ADJ_BYTE2,
	    BYTE2 (chip->Timing.AFE_ADCCLK_Timing));
  SendData (chip, ES01_85_AFE_ADCCLK_TIMING_ADJ_BYTE3,
	    BYTE3 (chip->Timing.AFE_ADCCLK_Timing));

  SendData (chip, ES01_1F0_AFERS_TIMING_ADJ_B0,
	    BYTE0 (chip->Timing.AFE_ADCRS_Timing));
  SendData (chip, ES01_1F1_AFERS_TIMING_ADJ_B1,
	    BYTE1 (chip->Timing.AFE_ADCRS_Timing));
  SendData (chip, ES01_1F2_AFERS_TIMING_ADJ_B2,
	    BYTE2 (chip->Timing.AFE_ADCRS_Timing));
  SendData (chip, ES01_1F3_AFERS_TIMING_ADJ_B3,
	    BYTE3 (chip->Timing.AFE_ADCRS_Timing));

  SendData (chip, ES01_1EC_AFEVS_TIMING_ADJ_B0,
	    BYTE0 (chip->Timing.AFE_ADCVS_Timing));
  SendData (chip, ES01_1ED_AFEVS_TIMING_ADJ_B1,
	    BYTE1 (chip->Timing.AFE_ADCVS_Timing));
  SendData (chip, ES01_1EE_AFEVS_TIMING_ADJ_B2,
	    BYTE2 (chip->Timing.AFE_ADCVS_Timing));
  SendData (chip, ES01_1EF_AFEVS_TIMING_ADJ_B3,
	    BYTE3 (chip->Timing.AFE_ADCVS_Timing));

  SendData (chip, ES01_160_CHANNEL_A_LATCH_POSITION_HB,
	    HIBYTE (chip->Timing.AFE_ChannelA_LatchPos));
  SendData (chip, ES01_161_CHANNEL_A_LATCH_POSITION_LB,
	    LOBYTE (chip->Timing.AFE_ChannelA_LatchPos));

  SendData (chip, ES01_162_CHANNEL_B_LATCH_POSITION_HB,
	    HIBYTE (chip->Timing.AFE_ChannelB_LatchPos));
  SendData (chip, ES01_163_CHANNEL_B_LATCH_POSITION_LB,
	    LOBYTE (chip->Timing.AFE_ChannelB_LatchPos));

  SendData (chip, ES01_164_CHANNEL_C_LATCH_POSITION_HB,
	    HIBYTE (chip->Timing.AFE_ChannelC_LatchPos));
  SendData (chip, ES01_165_CHANNEL_C_LATCH_POSITION_LB,
	    LOBYTE (chip->Timing.AFE_ChannelC_LatchPos));

  SendData (chip, ES01_166_CHANNEL_D_LATCH_POSITION_HB,
	    HIBYTE (chip->Timing.AFE_ChannelD_LatchPos));
  SendData (chip, ES01_167_CHANNEL_D_LATCH_POSITION_LB,
	    LOBYTE (chip->Timing.AFE_ChannelD_LatchPos));

  SendData (chip, ES01_168_SECONDARY_FF_LATCH_POSITION,
	    chip->Timing.AFE_Secondary_FF_LatchPos);

  SendData (chip, ES01_1D0_DUMMY_CYCLE_TIMING_B0,
	    BYTE0 (chip->Timing.CCD_DummyCycleTiming));
  SendData (chip, ES01_1D1_DUMMY_CYCLE_TIMING_B1,
	    BYTE1 (chip->Timing.CCD_DummyCycleTiming));
  SendData (chip, ES01_1D2_DUMMY_CYCLE_TIMING_B2,
	    BYTE2 (chip->Timing.CCD_DummyCycleTiming));
  SendData (chip, ES01_1D3_DUMMY_CYCLE_TIMING_B3,
	    BYTE3 (chip->Timing.CCD_DummyCycleTiming));

  SendData (chip, ES01_1D4_PH1_TIMING_ADJ_B0,
	    BYTE0 (chip->Timing.CCD_PH1_Timing));
  SendData (chip, ES01_1D5_PH1_TIMING_ADJ_B1,
	    BYTE1 (chip->Timing.CCD_PH1_Timing));
  SendData (chip, ES01_1D6_PH1_TIMING_ADJ_B2,
	    BYTE2 (chip->Timing.CCD_PH1_Timing));
  SendData (chip, ES01_1D7_PH1_TIMING_ADJ_B3,
	    BYTE3 (chip->Timing.CCD_PH1_Timing));

  SendData (chip, ES01_D0_PH1_0, 0);
  SendData (chip, ES01_D1_PH2_0, 4);
  SendData (chip, ES01_D4_PHRS_0, 0);
  SendData (chip, ES01_D5_PHCP_0, 0);

  SendData (chip, ES01_1D8_PH2_TIMING_ADJ_B0,
	    BYTE0 (chip->Timing.CCD_PH2_Timing));
  SendData (chip, ES01_1D9_PH2_TIMING_ADJ_B1,
	    BYTE1 (chip->Timing.CCD_PH2_Timing));
  SendData (chip, ES01_1DA_PH2_TIMING_ADJ_B2,
	    BYTE2 (chip->Timing.CCD_PH2_Timing));
  SendData (chip, ES01_1DB_PH2_TIMING_ADJ_B3,
	    BYTE3 (chip->Timing.CCD_PH2_Timing));

  SendData (chip, ES01_1E4_PHRS_TIMING_ADJ_B0,
	    BYTE0 (chip->Timing.CCD_PHRS_Timing));
  SendData (chip, ES01_1E5_PHRS_TIMING_ADJ_B1,
	    BYTE1 (chip->Timing.CCD_PHRS_Timing));
  SendData (chip, ES01_1E6_PHRS_TIMING_ADJ_B2,
	    BYTE2 (chip->Timing.CCD_PHRS_Timing));
  SendData (chip, ES01_1E7_PHRS_TIMING_ADJ_B3,
	    BYTE3 (chip->Timing.CCD_PHRS_Timing));

  SendData (chip, ES01_1E8_PHCP_TIMING_ADJ_B0,
	    BYTE0 (chip->Timing.CCD_PHCP_Timing));
  SendData (chip, ES01_1E9_PHCP_TIMING_ADJ_B1,
	    BYTE1 (chip->Timing.CCD_PHCP_Timing));
  SendData (chip, ES01_1EA_PHCP_TIMING_ADJ_B2,
	    BYTE2 (chip->Timing.CCD_PHCP_Timing));
  SendData (chip, ES01_1EB_PHCP_TIMING_ADJ_B3,
	    BYTE3 (chip->Timing.CCD_PHCP_Timing));

  DBG_ASIC_LEAVE ();
}

static void
SetLineTimeAndExposure (ASIC * chip)
{
  DBG_ASIC_ENTER ();

  SendData (chip, ES01_C4_MultiTGTimesRed, 0);
  SendData (chip, ES01_C5_MultiTGTimesGreen, 0);
  SendData (chip, ES01_C6_MultiTGTimesBlue, 0);

  SendData (chip, ES01_C7_MultiTGDummyPixelNumberLSB, 0);
  SendData (chip, ES01_C8_MultiTGDummyPixelNumberMSB, 0);

  SendData (chip, ES01_C9_CCDDummyPixelNumberLSB, 0);
  SendData (chip, ES01_CA_CCDDummyPixelNumberMSB, 0);

  SendData (chip, ES01_CB_CCDDummyCycleNumber, 0);

  DBG_ASIC_LEAVE ();
}

static void
SetLEDTime (ASIC * chip)
{
  DBG_ASIC_ENTER ();

  SendData (chip, ES01_B8_ChannelRedExpStartPixelLSB,
	    LOBYTE (chip->Timing.ChannelR_StartPixel));
  SendData (chip, ES01_B9_ChannelRedExpStartPixelMSB,
	    HIBYTE (chip->Timing.ChannelR_StartPixel));
  SendData (chip, ES01_BA_ChannelRedExpEndPixelLSB,
	    LOBYTE (chip->Timing.ChannelR_EndPixel));
  SendData (chip, ES01_BB_ChannelRedExpEndPixelMSB,
	    HIBYTE (chip->Timing.ChannelR_EndPixel));

  SendData (chip, ES01_BC_ChannelGreenExpStartPixelLSB,
	    LOBYTE (chip->Timing.ChannelG_StartPixel));
  SendData (chip, ES01_BD_ChannelGreenExpStartPixelMSB,
	    HIBYTE (chip->Timing.ChannelG_StartPixel));
  SendData (chip, ES01_BE_ChannelGreenExpEndPixelLSB,
	    LOBYTE (chip->Timing.ChannelG_EndPixel));
  SendData (chip, ES01_BF_ChannelGreenExpEndPixelMSB,
	    HIBYTE (chip->Timing.ChannelG_EndPixel));

  SendData (chip, ES01_C0_ChannelBlueExpStartPixelLSB,
	    LOBYTE (chip->Timing.ChannelB_StartPixel));
  SendData (chip, ES01_C1_ChannelBlueExpStartPixelMSB,
	    HIBYTE (chip->Timing.ChannelB_StartPixel));
  SendData (chip, ES01_C2_ChannelBlueExpEndPixelLSB,
	    LOBYTE (chip->Timing.ChannelB_EndPixel));
  SendData (chip, ES01_C3_ChannelBlueExpEndPixelMSB,
	    HIBYTE (chip->Timing.ChannelB_EndPixel));

  DBG_ASIC_LEAVE ();
}

void
SetAFEGainOffset (ASIC * chip)
{
  int i, j;
  DBG_ASIC_ENTER ();

  for (i = 0; i < 3; i++)
    {
      SendData (chip, ES01_60_AFE_AUTO_GAIN_OFFSET_RED_LB + (i * 2),
		(chip->AD.Gain[i] << 1) | chip->AD.Direction[i]);
      SendData (chip, ES01_61_AFE_AUTO_GAIN_OFFSET_RED_HB + (i * 2),
		chip->AD.Offset[i]);
    }

  SendData (chip, ES01_2A0_AFE_GAIN_OFFSET_CONTROL, 0x01);

  for (i = 0; i < 3; i++)
    {
      for (j = 0; j < 4; j++)
	{
	  SendData (chip, ES01_2A1_AFE_AUTO_CONFIG_GAIN,
		    (chip->AD.Gain[i] << 1) | chip->AD.Direction[i]);
	  SendData (chip, ES01_2A2_AFE_AUTO_CONFIG_OFFSET,
		    chip->AD.Offset[i]);
	}
    }

  for (i = 0; i < 36; i++)
    {
      SendData (chip, ES01_2A1_AFE_AUTO_CONFIG_GAIN, 0);
      SendData (chip, ES01_2A2_AFE_AUTO_CONFIG_OFFSET, 0);
    }

  SendData (chip, ES01_2A0_AFE_GAIN_OFFSET_CONTROL, 0x00);

  for (i = 0; i < 3; i++)
    SendData (chip, ES01_04_ADAFEPGACH1 + (i * 2), chip->AD.Gain[i]);

  for (i = 0; i < 3; i++)
    {
      if (chip->AD.Direction[i] == DIR_NEGATIVE)
	SendData (chip, ES01_0B_AD9826OffsetRedN + (i * 2), chip->AD.Offset[i]);
      else
	SendData (chip, ES01_0A_AD9826OffsetRedP + (i * 2), chip->AD.Offset[i]);
    }

  DBG_ASIC_LEAVE ();
}

static void
SetScanMode (ASIC * chip, SANE_Byte bScanBits)
{
  SANE_Byte temp_f5_register;
  DBG_ASIC_ENTER ();

  if (bScanBits >= 24)
    temp_f5_register = COLOR_ES02 | GRAY_GREEN_BLUE_ES02;
  else
    temp_f5_register = GRAY_ES02 | GRAY_GREEN_ES02;

  if ((bScanBits == 8) || (bScanBits == 24))
    temp_f5_register |= _8_BITS_ES02;
  else if (bScanBits == 1)
    temp_f5_register |= _1_BIT_ES02;
  else
    temp_f5_register |= _16_BITS_ES02;

  SendData (chip, ES01_F5_ScanDataFormat, temp_f5_register);

  DBG (DBG_ASIC, "F5_ScanDataFormat=0x%x\n", temp_f5_register);
  DBG_ASIC_LEAVE ();
}

static void
SetPackAddress (ASIC * chip, unsigned short wWidth, unsigned short wX,
		double XRatioAdderDouble, double XRatioTypeDouble,
		SANE_Byte bClearPulseWidth, unsigned short * pValidPixelNumber)
{
  unsigned short ValidPixelNumber;
  int i;
  DBG_ASIC_ENTER ();

  ValidPixelNumber = (unsigned short) ((wWidth + 10 + 15) * XRatioAdderDouble);
  ValidPixelNumber &= ~15;

  for (i = 0; i < 16; i++)
    {
      SendData (chip, ES01_2B0_SEGMENT0_OVERLAP_SEGMENT1 + i, 0);
      SendData (chip, ES01_2C0_VALID_PIXEL_PARAMETER_OF_SEGMENT1 + i, 0);
    }

  SendData (chip, ES01_1B0_SEGMENT_PIXEL_NUMBER_LB, LOBYTE (ValidPixelNumber));
  SendData (chip, ES01_1B1_SEGMENT_PIXEL_NUMBER_HB, HIBYTE (ValidPixelNumber));

  SendData (chip, ES01_169_NUMBER_OF_SEGMENT_PIXEL_LB,
	    LOBYTE (ValidPixelNumber));
  SendData (chip, ES01_16A_NUMBER_OF_SEGMENT_PIXEL_HB,
	    HIBYTE (ValidPixelNumber));
  SendData (chip, ES01_16B_BETWEEN_SEGMENT_INVALID_PIXEL, 0);

  SendData (chip, ES01_B6_LineWidthPixelLSB, LOBYTE (ValidPixelNumber));
  SendData (chip, ES01_B7_LineWidthPixelMSB, HIBYTE (ValidPixelNumber));

  SendData (chip, ES01_19A_CHANNEL_LINE_GAP_LB, LOBYTE (ValidPixelNumber));
  SendData (chip, ES01_19B_CHANNEL_LINE_GAP_HB, HIBYTE (ValidPixelNumber));
  DBG (DBG_ASIC, "ValidPixelNumber=%d\n", ValidPixelNumber);

  for (i = 0; i < 36; i++)
    SendData (chip, 0x270 + i, 0);

  SendData (chip, 0x270, BYTE0 (ValidPixelNumber * 2));
  SendData (chip, 0x271, BYTE1 (ValidPixelNumber * 2));
  SendData (chip, 0x272, BYTE2 (ValidPixelNumber * 2));

  SendData (chip, 0x27C, BYTE0 (ValidPixelNumber * 4));
  SendData (chip, 0x27D, BYTE1 (ValidPixelNumber * 4));
  SendData (chip, 0x27E, BYTE2 (ValidPixelNumber * 4));

  SendData (chip, 0x288, BYTE0 (ValidPixelNumber * 6));
  SendData (chip, 0x289, BYTE1 (ValidPixelNumber * 6));
  SendData (chip, 0x28A, BYTE2 (ValidPixelNumber * 6));
  DBG (DBG_ASIC, "channel gap=%d\n", ValidPixelNumber * 2);


  SendData (chip, ES01_B4_StartPixelLSB, LOBYTE (wX));
  SendData (chip, ES01_B5_StartPixelMSB, HIBYTE (wX));


  SendData (chip, ES01_1B9_LINE_PIXEL_NUMBER_LB,
	    LOBYTE (XRatioTypeDouble * (ValidPixelNumber - 1)));
  SendData (chip, ES01_1BA_LINE_PIXEL_NUMBER_HB,
	    HIBYTE (XRatioTypeDouble * (ValidPixelNumber - 1)));

  /* final start read out pixel */
  SendData (chip, ES01_1F4_START_READ_OUT_PIXEL_LB, 0);
  SendData (chip, ES01_1F5_START_READ_OUT_PIXEL_HB, 0);

  if (wWidth > (ValidPixelNumber - 10))
    DBG (DBG_ERR, "read out pixel greater than max pixel! image will shift!\n");

  /* final read pixel width */
  SendData (chip, ES01_1F6_READ_OUT_PIXEL_LENGTH_LB, LOBYTE (wWidth + 9));
  SendData (chip, ES01_1F7_READ_OUT_PIXEL_LENGTH_HB, HIBYTE (wWidth + 9));

  /* data output sequence */
  SendData (chip, ES01_1F8_PACK_CHANNEL_SELECT_B0, 0);
  SendData (chip, ES01_1F9_PACK_CHANNEL_SELECT_B1, 0);
  SendData (chip, ES01_1FA_PACK_CHANNEL_SELECT_B2, 0x18);

  SendData (chip, ES01_1FB_PACK_CHANNEL_SIZE_B0, BYTE0 (ValidPixelNumber * 2));
  SendData (chip, ES01_1FC_PACK_CHANNEL_SIZE_B1, BYTE1 (ValidPixelNumber * 2));
  SendData (chip, ES01_1FD_PACK_CHANNEL_SIZE_B2, BYTE2 (ValidPixelNumber * 2));

  SendData (chip, ES01_16C_LINE_SHIFT_OUT_TIMES_DIRECTION, 0x01);
  SendData (chip, ES01_1CD_DUMMY_CLOCK_NUMBER, 0);
  SendData (chip, ES01_1CE_LINE_SEGMENT_NUMBER, 0x00);
  SendData (chip, ES01_D8_PHTG_EDGE_TIMING_ADJUST, 0x17);

  SendData (chip, ES01_D9_CLEAR_PULSE_WIDTH, bClearPulseWidth);

  SendData (chip, ES01_DA_CLEAR_SIGNAL_INVERTING_OUTPUT, 0x54 | 0x01);
  SendData (chip, ES01_CD_TG_R_CONTROL, 0x3C);
  SendData (chip, ES01_CE_TG_G_CONTROL, 0);
  SendData (chip, ES01_CF_TG_B_CONTROL, 0x3C);

  /* set pack area address */
  SendData (chip, ES01_16D_EXPOSURE_CYCLE1_SEGMENT1_START_ADDR_BYTE0,
	    BYTE0 (PACK_AREA_START_ADDRESS));
  SendData (chip, ES01_16E_EXPOSURE_CYCLE1_SEGMENT1_START_ADDR_BYTE1,
	    BYTE1 (PACK_AREA_START_ADDRESS));
  SendData (chip, ES01_16F_EXPOSURE_CYCLE1_SEGMENT1_START_ADDR_BYTE2,
	    BYTE2 (PACK_AREA_START_ADDRESS));

  for ( i = ES01_170_EXPOSURE_CYCLE1_SEGMENT2_START_ADDR_BYTE0;
       i <= ES01_18E_EXPOSURE_CYCLE3_SEGMENT4_START_ADDR_BYTE0; i += 3)
    {
      SendData (chip, i, BYTE0 (PACK_AREA_START_ADDRESS + 0xC0000));
      SendData (chip, i + 1, BYTE1 (PACK_AREA_START_ADDRESS + 0xC0000));
      SendData (chip, i + 2, BYTE2 (PACK_AREA_START_ADDRESS + 0xC0000));
    }
  DBG (DBG_ASIC, "PACK_AREA_START_ADDRESS=%d\n", PACK_AREA_START_ADDRESS);

  for (i = 0; i < 16; i++)
    SendData (chip, 0x260 + i, 0);
  DBG (DBG_ASIC, "set invalid pixel finished\n");

  /* set pack start address */
  SendData (chip, ES01_19E_PACK_AREA_R_START_ADDR_BYTE0,
	    BYTE0 (PACK_AREA_START_ADDRESS));
  SendData (chip, ES01_19F_PACK_AREA_R_START_ADDR_BYTE1,
	    BYTE1 (PACK_AREA_START_ADDRESS));
  SendData (chip, ES01_1A0_PACK_AREA_R_START_ADDR_BYTE2,
	    BYTE2 (PACK_AREA_START_ADDRESS));

  SendData (chip, ES01_1A1_PACK_AREA_G_START_ADDR_BYTE0,
	    BYTE0 (PACK_AREA_START_ADDRESS + (ValidPixelNumber * 2)));
  SendData (chip, ES01_1A2_PACK_AREA_G_START_ADDR_BYTE1,
	    BYTE1 (PACK_AREA_START_ADDRESS + (ValidPixelNumber * 2)));
  SendData (chip, ES01_1A3_PACK_AREA_G_START_ADDR_BYTE2,
	    BYTE2 (PACK_AREA_START_ADDRESS + (ValidPixelNumber * 2)));

  SendData (chip, ES01_1A4_PACK_AREA_B_START_ADDR_BYTE0,
	    BYTE0 (PACK_AREA_START_ADDRESS + (ValidPixelNumber * 4)));
  SendData (chip, ES01_1A5_PACK_AREA_B_START_ADDR_BYTE1,
	    BYTE1 (PACK_AREA_START_ADDRESS + (ValidPixelNumber * 4)));
  SendData (chip, ES01_1A6_PACK_AREA_B_START_ADDR_BYTE2,
	    BYTE2 (PACK_AREA_START_ADDRESS + (ValidPixelNumber * 4)));

  /* set pack end address */
  SendData (chip, ES01_1A7_PACK_AREA_R_END_ADDR_BYTE0,
	    BYTE0 (PACK_AREA_START_ADDRESS + (ValidPixelNumber * 2 - 1)));
  SendData (chip, ES01_1A8_PACK_AREA_R_END_ADDR_BYTE1,
	    BYTE1 (PACK_AREA_START_ADDRESS + (ValidPixelNumber * 2 - 1)));
  SendData (chip, ES01_1A9_PACK_AREA_R_END_ADDR_BYTE2,
	    BYTE2 (PACK_AREA_START_ADDRESS + (ValidPixelNumber * 2 - 1)));

  SendData (chip, ES01_1AA_PACK_AREA_G_END_ADDR_BYTE0,
	    BYTE0 (PACK_AREA_START_ADDRESS + (ValidPixelNumber * 4 - 1)));
  SendData (chip, ES01_1AB_PACK_AREA_G_END_ADDR_BYTE1,
	    BYTE1 (PACK_AREA_START_ADDRESS + (ValidPixelNumber * 4 - 1)));
  SendData (chip, ES01_1AC_PACK_AREA_G_END_ADDR_BYTE2,
	    BYTE2 (PACK_AREA_START_ADDRESS + (ValidPixelNumber * 4 - 1)));

  SendData (chip, ES01_1AD_PACK_AREA_B_END_ADDR_BYTE0,
	    BYTE0 (PACK_AREA_START_ADDRESS + (ValidPixelNumber * 6 - 1)));
  SendData (chip, ES01_1AE_PACK_AREA_B_END_ADDR_BYTE1,
	    BYTE1 (PACK_AREA_START_ADDRESS + (ValidPixelNumber * 6 - 1)));
  SendData (chip, ES01_1AF_PACK_AREA_B_END_ADDR_BYTE2,
	    BYTE2 (PACK_AREA_START_ADDRESS + (ValidPixelNumber * 6 - 1)));
  DBG (DBG_ASIC, "PACK_AREA_START_ADDRESS + (ValidPixelNumber*2)=%d\n",
       (PACK_AREA_START_ADDRESS + (ValidPixelNumber * 2)));

  SendData (chip, ES01_19C_MAX_PACK_LINE, 2);
  SendData (chip, ES01_19D_PACK_THRESHOLD_LINE, 1);

  if (pValidPixelNumber)
    *pValidPixelNumber = ValidPixelNumber;

  DBG_ASIC_LEAVE ();
}

static void
SetExtraSettings (ASIC * chip, unsigned short wXResolution,
		  unsigned short wCCD_PixelNumber, SANE_Bool bypassShading)
{
  SANE_Byte temp_ff_register = 0;
  DBG_ASIC_ENTER ();

  SendData (chip, ES01_B8_ChannelRedExpStartPixelLSB,
	    LOBYTE (chip->Timing.ChannelR_StartPixel));
  SendData (chip, ES01_B9_ChannelRedExpStartPixelMSB,
	    HIBYTE (chip->Timing.ChannelR_StartPixel));
  SendData (chip, ES01_BA_ChannelRedExpEndPixelLSB,
	    LOBYTE (chip->Timing.ChannelR_EndPixel));
  SendData (chip, ES01_BB_ChannelRedExpEndPixelMSB,
	    HIBYTE (chip->Timing.ChannelR_EndPixel));

  SendData (chip, ES01_BC_ChannelGreenExpStartPixelLSB,
	    LOBYTE (chip->Timing.ChannelG_StartPixel));
  SendData (chip, ES01_BD_ChannelGreenExpStartPixelMSB,
	    HIBYTE (chip->Timing.ChannelG_StartPixel));
  SendData (chip, ES01_BE_ChannelGreenExpEndPixelLSB,
	    LOBYTE (chip->Timing.ChannelG_EndPixel));
  SendData (chip, ES01_BF_ChannelGreenExpEndPixelMSB,
	    HIBYTE (chip->Timing.ChannelG_EndPixel));

  SendData (chip, ES01_C0_ChannelBlueExpStartPixelLSB,
	    LOBYTE (chip->Timing.ChannelB_StartPixel));
  SendData (chip, ES01_C1_ChannelBlueExpStartPixelMSB,
	    HIBYTE (chip->Timing.ChannelB_StartPixel));
  SendData (chip, ES01_C2_ChannelBlueExpEndPixelLSB,
	    LOBYTE (chip->Timing.ChannelB_EndPixel));
  SendData (chip, ES01_C3_ChannelBlueExpEndPixelMSB,
	    HIBYTE (chip->Timing.ChannelB_EndPixel));

  SendData (chip, ES01_B2_PHTGPulseWidth, chip->Timing.PHTG_PulseWidth);
  SendData (chip, ES01_B3_PHTGWaitWidth, chip->Timing.PHTG_WaitWidth);

  SendData (chip, ES01_CC_PHTGTimingAdjust, chip->Timing.PHTG_TimingAdj);
  SendData (chip, ES01_D0_PH1_0, chip->Timing.PHTG_TimingSetup);

  DBG (DBG_ASIC, "ChannelR_StartPixel=%d,ChannelR_EndPixel=%d\n",
       chip->Timing.ChannelR_StartPixel, chip->Timing.ChannelR_EndPixel);

  if (wXResolution == SENSOR_DPI)
    SendData (chip, ES01_DE_CCD_SETUP_REGISTER, EVEN_ODD_ENABLE_ES01);
  else
    SendData (chip, ES01_DE_CCD_SETUP_REGISTER, 0);

  temp_ff_register |= BYPASS_PRE_GAMMA_ENABLE;
  temp_ff_register |= BYPASS_CONVOLUTION_ENABLE;
  temp_ff_register |= BYPASS_MATRIX_ENABLE;
  temp_ff_register |= BYPASS_GAMMA_ENABLE;

  if (bypassShading)
    {
      temp_ff_register |= BYPASS_DARK_SHADING_ENABLE;
      temp_ff_register |= BYPASS_WHITE_SHADING_ENABLE;
    }

  SendData (chip, ES01_FF_SCAN_IMAGE_OPTION, temp_ff_register);
  DBG (DBG_ASIC, "temp_ff_register=0x%x\n", temp_ff_register);

  /* pixel process time */
  SendData (chip, ES01_B0_CCDPixelLSB, LOBYTE (wCCD_PixelNumber));
  SendData (chip, ES01_B1_CCDPixelMSB, HIBYTE (wCCD_PixelNumber));
  SendData (chip, ES01_DF_ICG_CONTROL,
	    BEFORE_PHRS_416_1t_ns | ICG_UNIT_4_PIXEL_TIME);
  DBG (DBG_ASIC, "wCCD_PixelNumber=%d\n", wCCD_PixelNumber);

  SendData (chip, ES01_88_LINE_ART_THRESHOLD_HIGH_VALUE, 128);
  SendData (chip, ES01_89_LINE_ART_THRESHOLD_LOW_VALUE, 127);

  usleep (50000);
  DBG_ASIC_LEAVE ();
}


/* ---------------------- high level ASIC functions ------------------------ */

SANE_Status
Asic_FindDevices (unsigned short wVendorID, unsigned short wProductID,
		  SANE_Status (* attach) (SANE_String_Const devname))
{
  SANE_Status status;
  DBG_ASIC_ENTER ();

  sanei_usb_init ();
  status = sanei_usb_find_devices (wVendorID, wProductID, attach);
  if (status != SANE_STATUS_GOOD)
    DBG (DBG_ERR, "sanei_usb_find_devices failed: %s\n",
	 sane_strstatus (status));

  DBG_ASIC_LEAVE ();
  return status;
}

SANE_Status
Asic_Open (ASIC * chip)
{
  SANE_Status status;
  DBG_ASIC_ENTER ();

  if (chip->firmwarestate > FS_OPENED)
    {
      DBG (DBG_ASIC, "chip already open, fd=%d\n", chip->fd);
      return SANE_STATUS_DEVICE_BUSY;
    }

  sanei_usb_init ();

  status = sanei_usb_open (chip->device_name, &chip->fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_ERR, "sanei_usb_open of %s failed: %s\n",
	   chip->device_name, sane_strstatus (status));
      return status;
    }

  status = OpenScanChip (chip);
  if (status != SANE_STATUS_GOOD)
    {
      sanei_usb_close (chip->fd);
      return status;
    }

  SendData (chip, ES01_94_PowerSaveControl,
	    TIMER_POWER_SAVE_ENABLE |
	    USB_POWER_SAVE_ENABLE | USB_REMOTE_WAKEUP_ENABLE |
	    LED_MODE_FLASH_SLOWLY);
  SendData (chip, ES01_86_DisableAllClockWhenIdle, 0);
  SendData (chip, ES01_79_AFEMCLK_SDRAMCLK_DELAY_CONTROL,
	    chip->params->SDRAM_Delay);

  /* SDRAM initialization sequence */
  SendData (chip, ES01_87_SDRAM_Timing, 0xf1);
  SendData (chip, ES01_87_SDRAM_Timing, 0xa5);
  SendData (chip, ES01_87_SDRAM_Timing, 0x91);
  SendData (chip, ES01_87_SDRAM_Timing, 0x81);
  SendData (chip, ES01_87_SDRAM_Timing, 0xf0);

  chip->firmwarestate = FS_OPENED;

  status = WaitUnitReady (chip);
  if (status != SANE_STATUS_GOOD)
    {
      sanei_usb_close (chip->fd);
      return status;
    }

  status = SafeInitialChip (chip);
  if (status != SANE_STATUS_GOOD)
    {
      sanei_usb_close (chip->fd);
      return status;
    }

  DBG (DBG_INFO, "device %s successfully opened\n", chip->device_name);
  DBG_ASIC_LEAVE ();
  return SANE_STATUS_GOOD;
}

SANE_Status
Asic_Close (ASIC * chip)
{
  SANE_Status status;
  DBG_ASIC_ENTER ();

  if (chip->firmwarestate < FS_OPENED)
    {
      DBG (DBG_ASIC, "scanner is not open\n");
      return SANE_STATUS_GOOD;
    }
  if (chip->firmwarestate > FS_OPENED)
    {
      DBG (DBG_ASIC, "scanner is scanning, trying to stop\n");
      Asic_ScanStop (chip);
    }

  SendData (chip, ES01_86_DisableAllClockWhenIdle, CLOSE_ALL_CLOCK_ENABLE);
  status = CloseScanChip (chip);

  sanei_usb_close (chip->fd);

  chip->firmwarestate = FS_ATTACHED;

  DBG_ASIC_LEAVE ();
  return status;
}

void
Asic_Initialize (ASIC * chip)
{
  DBG_ASIC_ENTER ();

  chip->isFirstOpenChip = SANE_TRUE;
  chip->isUsb20 = SANE_FALSE;
  chip->dwBytesCountPerRow = 0;
  chip->isMotorMoveToFirstLine = MOTOR_MOVE_TO_FIRST_LINE_ENABLE;
  chip->pShadingTable = NULL;
  chip->RegisterBankStatus = 0xff;
  chip->is2ByteTransfer = SANE_FALSE;

  InitTiming (chip);

  chip->firmwarestate = FS_ATTACHED;

  DBG_ASIC_LEAVE ();
}

SANE_Status
Asic_TurnLamp (ASIC * chip, SANE_Bool isLampOn)
{
  DBG_ASIC_ENTER ();

  if (chip->firmwarestate < FS_OPENED)
    {
      DBG (DBG_ERR, "scanner is not open\n");
      return SANE_STATUS_CANCELLED;
    }
  if (chip->firmwarestate > FS_OPENED)
    {
      DBG (DBG_ERR, "scanner is busy\n");
      return SANE_STATUS_DEVICE_BUSY;
    }

  SendData (chip, ES01_99_LAMP_PWM_FREQ_CONTROL, 1);
  if (isLampOn)
    SendData (chip, ES01_90_Lamp0PWM, LAMP0_PWM_DEFAULT);
  else
    SendData (chip, ES01_90_Lamp0PWM, 0);

  DBG_ASIC_LEAVE ();
  return SANE_STATUS_GOOD;
}

SANE_Status
Asic_TurnTA (ASIC * chip, SANE_Bool isTAOn)
{
  DBG_ASIC_ENTER ();

  if (chip->firmwarestate < FS_OPENED)
    {
      DBG (DBG_ERR, "scanner is not open\n");
      return SANE_STATUS_CANCELLED;
    }
  if (chip->firmwarestate > FS_OPENED)
    {
      DBG (DBG_ERR, "scanner is busy\n");
      return SANE_STATUS_DEVICE_BUSY;
    }

  SendData (chip, ES01_99_LAMP_PWM_FREQ_CONTROL, 1);
  if (isTAOn)
    SendData (chip, ES01_91_Lamp1PWM, LAMP1_PWM_DEFAULT);
  else
    SendData (chip, ES01_91_Lamp1PWM, 0);

  DBG_ASIC_LEAVE ();
  return SANE_STATUS_GOOD;
}


static SANE_Byte
GetDummyCycleNumber (ASIC * chip, unsigned short wYResolution,
		     SCANSOURCE lsLightSource)
{
  SANE_Byte bDummyCycleNum = 0;
  if (lsLightSource == SS_REFLECTIVE)
    {
      if (!chip->isUsb20)
	{
	  switch (wYResolution)
	    {
	    case 2400:
	    case 1200:
	      if (chip->dwBytesCountPerRow > 22000)
		bDummyCycleNum = 4;
	      else if (chip->dwBytesCountPerRow > 15000)
		bDummyCycleNum = 3;
	      else if (chip->dwBytesCountPerRow > 10000)
		bDummyCycleNum = 2;
	      else if (chip->dwBytesCountPerRow > 5000)
		bDummyCycleNum = 1;
	      break;
	    case 600:
	    case 300:
	    case 150:
	    case 100:
	      if (chip->dwBytesCountPerRow > 21000)
		bDummyCycleNum = 7;
	      else if (chip->dwBytesCountPerRow > 18000)
		bDummyCycleNum = 6;
	      else if (chip->dwBytesCountPerRow > 15000)
		bDummyCycleNum = 5;
	      else if (chip->dwBytesCountPerRow > 12000)
		bDummyCycleNum = 4;
	      else if (chip->dwBytesCountPerRow > 9000)
		bDummyCycleNum = 3;
	      else if (chip->dwBytesCountPerRow > 6000)
		bDummyCycleNum = 2;
	      else if (chip->dwBytesCountPerRow > 3000)
		bDummyCycleNum = 1;
	      break;
	    case 75:
	    case 50:
	      bDummyCycleNum = 1;
	      break;
	    }
	}
      else
	{
	  switch (wYResolution)
	    {
	    case 2400:
	    case 1200:
	    case 75:
	    case 50:
	      bDummyCycleNum = 1;
	      break;
	    }
	}
    }
  return bDummyCycleNum;
}

SANE_Status
Asic_SetWindow (ASIC * chip, SCANSOURCE lsLightSource,
		SCAN_TYPE ScanType, SANE_Byte bScanBits,
		unsigned short wXResolution, unsigned short wYResolution,
		unsigned short wX, unsigned short wY,
		unsigned short wWidth, unsigned short wHeight)
{
  SANE_Status status;
  unsigned short ValidPixelNumber;
  unsigned short wThinkCCDResolution;
  unsigned short wCCD_PixelNumber;
  unsigned short XRatioTypeWord;
  double XRatioTypeDouble;
  double XRatioAdderDouble;
  MOTORMOVE pMotorStepsTable;
  SANE_Byte bDummyCycleNum;
  unsigned short wMultiMotorStep;
  SANE_Byte bMotorMoveType;
  unsigned int dwLinePixelReport;
  unsigned int StartSpeed, EndSpeed;
  CALCULATEMOTORTABLE CalMotorTable;
  MOTOR_CURRENT_AND_PHASE CurrentPhase;
  unsigned int dwTableBaseAddr, dwEndAddr;
  SANE_Byte isMotorMove;
  SANE_Byte isMotorMoveToFirstLine;
  SANE_Byte isUniformSpeedToScan;
  SANE_Byte isScanBackTracking;
  unsigned short * pMotorTable;
  unsigned int RealTableSize;
  unsigned short wFullBank;
  DBG_ASIC_ENTER ();
  DBG (DBG_ASIC, "lsLightSource=%d,ScanType=%d,bScanBits=%d," \
		 "wXResolution=%d,wYResolution=%d,wX=%d,wY=%d," \
		 "wWidth=%d,wHeight=%d\n",
       lsLightSource, ScanType, bScanBits, wXResolution, wYResolution, wX, wY,
       wWidth, wHeight);

  if (chip->firmwarestate < FS_OPENED)
    {
      DBG (DBG_ERR, "scanner is not open\n");
      return SANE_STATUS_CANCELLED;
    }
  if (chip->firmwarestate > FS_OPENED)
    {
      DBG (DBG_ERR, "scanner is busy\n");
      return SANE_STATUS_DEVICE_BUSY;
    }

  status = PrepareScanChip (chip);
  if (status != SANE_STATUS_GOOD)
    return status;

  SendData (chip, ES01_94_PowerSaveControl,
	    TIMER_POWER_SAVE_ENABLE |
	    USB_POWER_SAVE_ENABLE | USB_REMOTE_WAKEUP_ENABLE |
	    LED_MODE_FLASH_SLOWLY | 0x40 | 0x80);

  chip->dwBytesCountPerRow = (unsigned int) wWidth * (bScanBits / 8);
  DBG (DBG_ASIC, "dwBytesCountPerRow=%d\n", chip->dwBytesCountPerRow);

  if (ScanType == SCAN_TYPE_NORMAL)
    bDummyCycleNum = GetDummyCycleNumber (chip, wYResolution, lsLightSource);
  else
    bDummyCycleNum = 1;

  SetCCDTiming (chip);

  if (wXResolution > (SENSOR_DPI / 2))
    {
      wThinkCCDResolution = SENSOR_DPI;
      SendData (chip, ES01_98_GPIOControl8_15, 0x01);
      SendData (chip, ES01_96_GPIOValue8_15, 0x01);
    }
  else
    {
      wThinkCCDResolution = SENSOR_DPI / 2;
      SendData (chip, ES01_98_GPIOControl8_15, 0x01);
      SendData (chip, ES01_96_GPIOValue8_15, 0x00);
    }

  if (lsLightSource == SS_REFLECTIVE)
    {
      if (wXResolution > (SENSOR_DPI / 2))
	wCCD_PixelNumber = chip->Timing.wCCDPixelNumber_Full;
      else
	wCCD_PixelNumber = chip->Timing.wCCDPixelNumber_Half;
    }
  else
    {
      if (ScanType == SCAN_TYPE_NORMAL)
	wCCD_PixelNumber = TA_IMAGE_PIXELNUMBER;
      else
	wCCD_PixelNumber = TA_CAL_PIXELNUMBER;
    }

  SetLineTimeAndExposure (chip);
  SendData (chip, ES01_CB_CCDDummyCycleNumber, bDummyCycleNum);
  SetLEDTime (chip);

  SendData (chip, ES01_74_HARDWARE_SETTING,
	    MOTOR1_SERIAL_INTERFACE_G10_8_ENABLE);

  SendData (chip, ES01_9A_AFEControl, AD9826_AFE);
  SetAFEGainOffset (chip);
  SendData (chip, ES01_DB_PH_RESET_EDGE_TIMING_ADJUST, 0x00);
  SendData (chip, ES01_DC_CLEAR_EDGE_TO_PH_TG_EDGE_WIDTH, 0);
  SendData (chip, ES01_00_ADAFEConfiguration, 0x70);
  SendData (chip, ES01_02_ADAFEMuxConfig, 0x80);

  SendData (chip, ES01_F7_DigitalControl, 0);

  /* calculate X ratio */
  XRatioTypeDouble = (double) wXResolution / wThinkCCDResolution;
  XRatioTypeWord = (unsigned short) (XRatioTypeDouble * 32768);
  XRatioAdderDouble = 1 / XRatioTypeDouble;

  /* 32768 = 1.00000 -> get all pixel */
  SendData (chip, ES01_9E_HorizontalRatio1to15LSB, LOBYTE (XRatioTypeWord));
  SendData (chip, ES01_9F_HorizontalRatio1to15MSB, HIBYTE (XRatioTypeWord));
  DBG (DBG_ASIC,
       "XRatioTypeDouble=%.2f,XRatioAdderDouble=%.2f,XRatioTypeWord=%d\n",
       XRatioTypeDouble, XRatioAdderDouble, XRatioTypeWord);

  /* set motor type */
  if (ScanType == SCAN_TYPE_CALIBRATE_DARK)
    isMotorMove = 0;
  else
    isMotorMove = MOTOR_0_ENABLE;
  SendData (chip, ES01_A6_MotorOption,
	    isMotorMove | HOME_SENSOR_0_ENABLE | TABLE_DEFINE_ES03);
  SendData (chip, ES01_F6_MotorControl1,
	    SPEED_UNIT_1_PIXEL_TIME | MOTOR_SYNC_UNIT_1_PIXEL_TIME);

  if ((ScanType == SCAN_TYPE_NORMAL) && (wYResolution >= SENSOR_DPI))
    bMotorMoveType = _8_TABLE_SPACE_FOR_1_DIV_2_STEP;
  else
    bMotorMoveType = _4_TABLE_SPACE_FOR_FULL_STEP;

  switch (bMotorMoveType)
    {
    default:
    case _4_TABLE_SPACE_FOR_FULL_STEP:
      wMultiMotorStep = 1;
      break;
    case _8_TABLE_SPACE_FOR_1_DIV_2_STEP:
      wMultiMotorStep = 2;
      break;
    case _16_TABLE_SPACE_FOR_1_DIV_4_STEP:
      wMultiMotorStep = 4;
      break;
    case _32_TABLE_SPACE_FOR_1_DIV_8_STEP:
      wMultiMotorStep = 8;
      break;
    }
  wY *= wMultiMotorStep;

  SetScanMode (chip, bScanBits);

  if (lsLightSource == SS_REFLECTIVE)
    SendData (chip, ES01_F8_WHITE_SHADING_DATA_FORMAT,
	      SHADING_3_INT_13_DEC_ES01);
  else
    SendData (chip, ES01_F8_WHITE_SHADING_DATA_FORMAT,
	      SHADING_4_INT_12_DEC_ES01);

  SetPackAddress (chip, wWidth, wX, XRatioAdderDouble, XRatioTypeDouble, 0,
		  &ValidPixelNumber);

  if (ScanType == SCAN_TYPE_NORMAL)
    {
      SetExtraSettings (chip, wXResolution, wCCD_PixelNumber, SANE_FALSE);
      SetMotorStepTable (chip, &pMotorStepsTable, wY,
			 wHeight * SENSOR_DPI / wYResolution * wMultiMotorStep,
			 wYResolution);
    }
  else
    {
      SetExtraSettings (chip, wXResolution, wCCD_PixelNumber, SANE_TRUE);
      SetMotorStepTableForCalibration (chip, &pMotorStepsTable,
			 wHeight * SENSOR_DPI / wYResolution * wMultiMotorStep);
    }

  /* calculate line time */
  dwLinePixelReport = (2 + (chip->Timing.PHTG_PulseWidth + 1) +
		       (chip->Timing.PHTG_WaitWidth + 1) +
		       (wCCD_PixelNumber + 1)) * (bDummyCycleNum + 1);

  EndSpeed = (dwLinePixelReport * wYResolution / SENSOR_DPI) / wMultiMotorStep;
  DBG (DBG_ASIC, "motor time = %d\n", EndSpeed);
  if (EndSpeed > 0xffff)
    {
      DBG (DBG_ASIC, "Motor time overflow!\n");
      return SANE_STATUS_INVAL;
    }

  isMotorMoveToFirstLine = chip->isMotorMoveToFirstLine;
  isUniformSpeedToScan = UNIFORM_MOTOR_AND_SCAN_SPEED_ENABLE;
  isScanBackTracking = 0;
  if (ScanType == SCAN_TYPE_NORMAL)
    {
      if (EndSpeed >= 20000)
	{
	  status = SimpleMotorMove (chip, 5000, 1800, 7000, 200,
				    wY / wMultiMotorStep, ACTION_TYPE_FORWARD);
	  if (status != SANE_STATUS_GOOD)
	    return status;

	  isMotorMoveToFirstLine = 0;
	}
      else
	{
	  isUniformSpeedToScan = 0;
	  isScanBackTracking = SCAN_BACK_TRACKING_ENABLE;
	}
    }
  SendData (chip, ES01_F3_ActionOption, SCAN_ENABLE |
	    isMotorMoveToFirstLine | isScanBackTracking | isUniformSpeedToScan);

  if (EndSpeed > 8000)
    {
      StartSpeed = EndSpeed;
    }
  else
    {
      if (EndSpeed <= 1000)
	StartSpeed = EndSpeed + 4500;
      else
	StartSpeed = EndSpeed + 3500;
    }
  DBG (DBG_ASIC, "StartSpeed=%d,EndSpeed=%d\n", StartSpeed, EndSpeed);

  SendData (chip, ES01_FD_MotorFixedspeedLSB, LOBYTE (EndSpeed));
  SendData (chip, ES01_FE_MotorFixedspeedMSB, HIBYTE (EndSpeed));

  pMotorTable = malloc (MOTOR_TABLE_SIZE * sizeof (unsigned short));
  if (!pMotorTable)
    {
      DBG (DBG_ERR, "pMotorTable == NULL\n");
      return SANE_STATUS_NO_MEM;
    }
  memset (pMotorTable, 0, MOTOR_TABLE_SIZE * sizeof (unsigned short));

  CalMotorTable.StartSpeed = StartSpeed;
  CalMotorTable.EndSpeed = EndSpeed;
  CalMotorTable.AccStepBeforeScan = pMotorStepsTable.wScanAccSteps;
  CalMotorTable.DecStepAfterScan = pMotorStepsTable.bScanDecSteps;
  CalMotorTable.pMotorTable = pMotorTable;

  CurrentPhase.MoveType = bMotorMoveType;
  CurrentPhase.MotorDriverIs3967 = 0;

  if (ScanType == SCAN_TYPE_NORMAL)
    {
      CalculateScanMotorTable (&CalMotorTable);
      CurrentPhase.MotorCurrent = CalculateMotorCurrent (EndSpeed);
    }
  else
    {
      CalculateMoveMotorTable (&CalMotorTable);
      CurrentPhase.MotorCurrent = 200;
    }
  SetMotorCurrentAndPhase (chip, &CurrentPhase);

  DBG (DBG_ASIC, "MotorCurrent=%d,LinePixelReport=%d\n",
       CurrentPhase.MotorCurrent, dwLinePixelReport);

  /* write motor table */
  RealTableSize = MOTOR_TABLE_SIZE;
  dwTableBaseAddr = PACK_AREA_START_ADDRESS - RealTableSize;
  status = SetMotorTable (chip, dwTableBaseAddr, pMotorTable);
  free (pMotorTable);
  if (status != SANE_STATUS_GOOD)
    return status;

  if (ScanType == SCAN_TYPE_NORMAL)
    {
      /* set image buffer range and write shading table */
      RealTableSize += ShadingTableSize (ValidPixelNumber);
      dwTableBaseAddr = PACK_AREA_START_ADDRESS -
			((RealTableSize + (TABLE_BASE_SIZE - 1)) &
			 ~(TABLE_BASE_SIZE - 1));

      status = SetShadingTable (chip, dwTableBaseAddr, chip->dwShadingTableSize,
				chip->pShadingTable);
      if (status != SANE_STATUS_GOOD)
	return status;
    }

  dwEndAddr = dwTableBaseAddr - 1;

  /* empty bank */
  SendData (chip, ES01_FB_BufferEmptySize16WordLSB,
	    LOBYTE (WAIT_BUFFER_ONE_LINE_SIZE >> 4));
  SendData (chip, ES01_FC_BufferEmptySize16WordMSB,
	    HIBYTE (WAIT_BUFFER_ONE_LINE_SIZE >> 4));

  /* full bank */
  wFullBank = (unsigned short)
  	      ((dwEndAddr - ((chip->dwBytesCountPerRow / 2) * 3)) / BANK_SIZE);
  SendData (chip, ES01_F9_BufferFullSize16WordLSB, LOBYTE (wFullBank));
  SendData (chip, ES01_FA_BufferFullSize16WordMSB, HIBYTE (wFullBank));

  status = SetRamAddress (chip, 0, dwEndAddr, EXTERNAL_RAM);

  DBG_ASIC_LEAVE ();
  return status;
}

SANE_Status
Asic_ScanStart (ASIC * chip)
{
  SANE_Status status;
  DBG_ASIC_ENTER ();

  if (chip->firmwarestate < FS_OPENED)
    {
      DBG (DBG_ERR, "scanner is not open\n");
      return SANE_STATUS_CANCELLED;
    }
  if (chip->firmwarestate > FS_OPENED)
    {
      DBG (DBG_ERR, "scanner is busy\n");
      return SANE_STATUS_DEVICE_BUSY;
    }

  status = GetChipStatus(chip, 0x1c | 0x20, NULL);
  if (status != SANE_STATUS_GOOD)
    return status;

  status = ClearFIFO (chip);
  if (status != SANE_STATUS_GOOD)
    return status;

  SendData (chip, ES01_F4_ActiveTrigger, ACTION_TRIGGER_ENABLE);

  chip->firmwarestate = FS_SCANNING;

  DBG_ASIC_LEAVE ();
  return SANE_STATUS_GOOD;
}

SANE_Status
Asic_ScanStop (ASIC * chip)
{
  SANE_Status status;
  SANE_Byte buf[4];
  DBG_ASIC_ENTER ();

  if (chip->firmwarestate < FS_OPENED)
    {
      DBG (DBG_ERR, "scanner is not open\n");
      return SANE_STATUS_CANCELLED;
    }
  if (chip->firmwarestate < FS_SCANNING)
    return SANE_STATUS_GOOD;

  usleep (100 * 1000);

  buf[0] = 0x02;	/* stop */
  buf[1] = 0x02;
  buf[2] = 0x02;
  buf[3] = 0x02;
  status = WriteIOControl (chip, 0xc0, 0, 4, buf);
  if (status != SANE_STATUS_GOOD)
    return status;

  buf[0] = 0x00;	/* clear */
  buf[1] = 0x00;
  buf[2] = 0x00;
  buf[3] = 0x00;
  status = WriteIOControl (chip, 0xc0, 0, 4, buf);
  if (status != SANE_STATUS_GOOD)
    return status;

  status = DMARead (chip, 2, buf);
  if (status != SANE_STATUS_GOOD)
    return status;

  status = PrepareScanChip (chip);
  if (status != SANE_STATUS_GOOD)
    return status;

  status = ClearFIFO (chip);
  if (status != SANE_STATUS_GOOD)
    return status;

  chip->firmwarestate = FS_OPENED;

  DBG_ASIC_LEAVE ();
  return SANE_STATUS_GOOD;
}

SANE_Status
Asic_ReadImage (ASIC * chip, SANE_Byte * pBuffer, unsigned short LinesCount)
{
  SANE_Status status;
  unsigned int dwXferBytes;
  DBG_ASIC_ENTER ();
  DBG (DBG_ASIC, "LinesCount=%d\n", LinesCount);

  if (chip->firmwarestate != FS_SCANNING)
    {
      DBG (DBG_ERR, "scanner is not scanning\n");
      return SANE_STATUS_CANCELLED;
    }

  dwXferBytes = (unsigned int) LinesCount * chip->dwBytesCountPerRow;
  if (dwXferBytes == 0)
    {
      DBG (DBG_ASIC, "dwXferBytes == 0\n");
      return SANE_STATUS_GOOD;
    }

  status = DMARead (chip, dwXferBytes, pBuffer);

  DBG_ASIC_LEAVE ();
  return status;
}

SANE_Status
Asic_CheckFunctionKey (ASIC * chip, SANE_Byte * key)
{
  SANE_Status status;
  SANE_Byte bBuffer_1, bBuffer_2;
  DBG_ASIC_ENTER ();

  if (chip->firmwarestate < FS_OPENED)
    {
      DBG (DBG_ERR, "scanner is not open\n");
      return SANE_STATUS_CANCELLED;
    }
  if (chip->firmwarestate > FS_OPENED)
    {
      DBG (DBG_ERR, "scanner is busy\n");
      return SANE_STATUS_DEVICE_BUSY;
    }

  SendData (chip, ES01_97_GPIOControl0_7, 0x00);
  SendData (chip, ES01_95_GPIOValue0_7, 0x17);
  SendData (chip, ES01_98_GPIOControl8_15, 0x00);
  SendData (chip, ES01_96_GPIOValue8_15, 0x08);

  status = GetChipStatus (chip, GPIO0_7, &bBuffer_1);
  if (status != SANE_STATUS_GOOD)
    return status;
  status = GetChipStatus (chip, GPIO8_15, &bBuffer_2);
  if (status != SANE_STATUS_GOOD)
    return status;

  bBuffer_1 = ~bBuffer_1;
  bBuffer_2 = ~bBuffer_2;

  if (bBuffer_1 & 0x10)
    *key = 1;	/* Scan key pressed */
  else if (bBuffer_1 & 0x01)
    *key = 2;	/* Copy key pressed */
  else if (bBuffer_1 & 0x04)
    *key = 3;	/* Fax key pressed */
  else if (bBuffer_2 & 0x08)
    *key = 4;	/* Email key pressed */
  else if (bBuffer_1 & 0x02)
    *key = 5;	/* Panel key pressed */
  else
    *key = 0;

  DBG (DBG_ASIC, "key=%d\n", *key);
  DBG_ASIC_LEAVE ();
  return SANE_STATUS_GOOD;
}

SANE_Status
Asic_IsTAConnected (ASIC * chip, SANE_Bool * hasTA)
{
  SANE_Status status;
  SANE_Byte bBuffer_1 = 0xff;
  DBG_ASIC_ENTER ();

  if (chip->firmwarestate < FS_OPENED)
    {
      DBG (DBG_ERR, "scanner is not open\n");
      return SANE_STATUS_CANCELLED;
    }
  if (chip->firmwarestate > FS_OPENED)
    {
      DBG (DBG_ERR, "scanner is busy\n");
      return SANE_STATUS_DEVICE_BUSY;
    }

  SendData (chip, ES01_97_GPIOControl0_7, 0x00);
  SendData (chip, ES01_95_GPIOValue0_7, 0x00);
  SendData (chip, ES01_98_GPIOControl8_15, 0x00);
  SendData (chip, ES01_96_GPIOValue8_15, 0x00);

  status = GetChipStatus (chip, GPIO0_7, &bBuffer_1);
  if (status != SANE_STATUS_GOOD)
    return status;

  *hasTA = (~bBuffer_1 & 0x08) ? SANE_TRUE : SANE_FALSE;

  DBG (DBG_ASIC, "hasTA=%d\n", *hasTA);
  DBG_ASIC_LEAVE ();
  return SANE_STATUS_GOOD;
}

SANE_Status
Asic_ReadCalibrationData (ASIC * chip, SANE_Byte * pBuffer,
			  unsigned int dwXferBytes, SANE_Bool separateColors)
{
  SANE_Status status = SANE_STATUS_GOOD;
  SANE_Byte * pCalBuffer;
  unsigned int dwTotalReadData = 0;
  unsigned int dwReadImageData;
  DBG_ASIC_ENTER ();

  if (chip->firmwarestate != FS_SCANNING)
    {
      DBG (DBG_ERR, "scanner is not scanning\n");
      return SANE_STATUS_CANCELLED;
    }

  if (separateColors)
    {
      unsigned int i;

      pCalBuffer = malloc (dwXferBytes);
      if (!pCalBuffer)
	{
	  DBG (DBG_ERR, "pCalBuffer == NULL\n");
	  return SANE_STATUS_NO_MEM;
	}

      while (dwTotalReadData < dwXferBytes)
	{
	  dwReadImageData = dwXferBytes - dwTotalReadData;
	  if (dwReadImageData > 65536)
	    dwReadImageData = 65536;

	  status = DMARead (chip, dwReadImageData,
			    pCalBuffer + dwTotalReadData);
	  if (status != SANE_STATUS_GOOD)
	    break;
	  dwTotalReadData += dwReadImageData;
	}

      dwXferBytes /= 3;
      for (i = 0; i < dwXferBytes; i++)
	{
	  pBuffer[i] = pCalBuffer[i * 3];
	  pBuffer[dwXferBytes + i] = pCalBuffer[i * 3 + 1];
	  pBuffer[dwXferBytes * 2 + i] = pCalBuffer[i * 3 + 2];
	}
      free (pCalBuffer);
    }
  else
    {
      while (dwTotalReadData < dwXferBytes)
	{
	  dwReadImageData = dwXferBytes - dwTotalReadData;
	  if (dwReadImageData > 65536)
	    dwReadImageData = 65536;

	  status = DMARead (chip, dwReadImageData, pBuffer + dwTotalReadData);
	  if (status != SANE_STATUS_GOOD)
	    break;
	  dwTotalReadData += dwReadImageData;
	}
    }

  DBG_ASIC_LEAVE ();
  return status;
}

SANE_Status
Asic_MotorMove (ASIC * chip, SANE_Bool isForward, unsigned int dwTotalSteps)
{
  SANE_Status status;
  SANE_Byte bActionType;
  DBG_ASIC_ENTER ();

  if (chip->firmwarestate < FS_OPENED)
    {
      DBG (DBG_ERR, "scanner is not open\n");
      return SANE_STATUS_CANCELLED;
    }
  if (chip->firmwarestate > FS_OPENED)
    {
      DBG (DBG_ERR, "scanner is busy\n");
      return SANE_STATUS_DEVICE_BUSY;
    }

  bActionType = isForward ? ACTION_TYPE_FORWARD : ACTION_TYPE_BACKWARD;
  status = SimpleMotorMove (chip, 5000, 1800, 7000, 200, dwTotalSteps,
			    bActionType);

  DBG_ASIC_LEAVE ();
  return status;
}

SANE_Status
Asic_CarriageHome (ASIC * chip)
{
  SANE_Status status;
  SANE_Bool LampHome;
  DBG_ASIC_ENTER ();

  if (chip->firmwarestate < FS_OPENED)
    {
      DBG (DBG_ERR, "scanner is not open\n");
      return SANE_STATUS_CANCELLED;
    }
  if (chip->firmwarestate > FS_OPENED)
    {
      DBG (DBG_ERR, "scanner is busy\n");
      return SANE_STATUS_DEVICE_BUSY;
    }

  status = IsCarriageHome (chip, &LampHome);
  if (status != SANE_STATUS_GOOD)
    return status;

  if (!LampHome)
    status = SimpleMotorMove (chip, 5000, 1200, 3000, 220, 766,
			      ACTION_TYPE_BACKTOHOME);

  DBG_ASIC_LEAVE ();
  return status;
}

SANE_Status
Asic_SetShadingTable (ASIC * chip, unsigned short * pWhiteShading,
		      unsigned short * pDarkShading,
		      unsigned short wXResolution, unsigned short wWidth)
{
  unsigned short i, j, n;
  unsigned short wValidPixelNumber;
  double dbXRatioAdderDouble;
  DBG_ASIC_ENTER ();

  if (chip->firmwarestate < FS_OPENED)
    {
      DBG (DBG_ERR, "scanner is not open\n");
      return SANE_STATUS_CANCELLED;
    }
  if (chip->firmwarestate > FS_OPENED)
    {
      DBG (DBG_ERR, "scanner is busy\n");
      return SANE_STATUS_DEVICE_BUSY;
    }

  if (wXResolution > (SENSOR_DPI / 2))
    dbXRatioAdderDouble = SENSOR_DPI / wXResolution;
  else
    dbXRatioAdderDouble = (SENSOR_DPI / 2) / wXResolution;

  wValidPixelNumber = (unsigned short) ((wWidth + 4) * dbXRatioAdderDouble);
  DBG (DBG_ASIC, "wValidPixelNumber=%d\n", wValidPixelNumber);

  /* Free old shading table, if present.
     First 4 and last 5 elements of shading table cannot be used. */
  chip->dwShadingTableSize = ShadingTableSize (wValidPixelNumber) *
    sizeof (unsigned short);
  free (chip->pShadingTable);
  DBG (DBG_ASIC, "allocating a new shading table, size=%d byte\n",
       chip->dwShadingTableSize);
  chip->pShadingTable = malloc (chip->dwShadingTableSize);
  if (!chip->pShadingTable)
    {
      DBG (DBG_ASIC, "pShadingTable == NULL\n");
      return SANE_STATUS_NO_MEM;
    }

  n = 0;
  for (i = 0; i <= (wValidPixelNumber / 40); i++)
    {
      unsigned short numPixel = 40;
      if (i == (wValidPixelNumber / 40))
	numPixel = wValidPixelNumber % 40;

      for (j = 0; j < numPixel; j++)
	{
	  chip->pShadingTable[i * 256 + j * 6] =
	    htole16 (pDarkShading[n * 3]);
	  chip->pShadingTable[i * 256 + j * 6 + 2] =
	    htole16 (pDarkShading[n * 3 + 1]);
	  chip->pShadingTable[i * 256 + j * 6 + 4] =
	    htole16 (pDarkShading[n * 3 + 2]);

	  chip->pShadingTable[i * 256 + j * 6 + 1] =
	    htole16 (pWhiteShading[n * 3]);
	  chip->pShadingTable[i * 256 + j * 6 + 3] = 
	    htole16 (pWhiteShading[n * 3 + 1]);
	  chip->pShadingTable[i * 256 + j * 6 + 5] =
	    htole16 (pWhiteShading[n * 3 + 2]);

	  if ((j % (unsigned short) dbXRatioAdderDouble) ==
		  (dbXRatioAdderDouble - 1))
	    n++;

	  if (i == 0 && j < 4 * dbXRatioAdderDouble)
	    n = 0;
	}
    }

  DBG_ASIC_LEAVE ();
  return SANE_STATUS_GOOD;
}
