/*.............................................................................
 * Project : SANE library for Plustek USB flatbed scanners.
 *.............................................................................
 * File:	 plustek-usbio.c - this one should go out to a sane library
 *.............................................................................
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 2001 Gerhard Jaeger <g.jaeger@earthling.net>
 *.............................................................................
 * History:
 * 0.40 - starting version of the USB support
 *
 *.............................................................................
 *
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 *
 * As a special exception, the authors of SANE give permission for
 * additional uses of the libraries contained in this release of SANE.
 *
 * The exception is that, if you link a SANE library with other files
 * to produce an executable, this does not by itself cause the
 * resulting executable to be covered by the GNU General Public
 * License.  Your use of that executable is in no way restricted on
 * account of linking the SANE library code into it.
 *
 * This exception does not, however, invalidate any other reasons why
 * the executable file might be covered by the GNU General Public
 * License.
 *
 * If you submit changes to SANE to the maintainers to be included in
 * a subsequent release, you agree by submitting the changes that
 * those changes may be distributed with this exception intact.
 *
 * If you write modifications of your own for SANE, it is your choice
 * whether to permit this exception to apply to your modifications.
 * If you do not wish that, delete this exception notice.
 */

#define _UIO(func)                        \
    {                                     \
      SANE_Status status;                 \
      status = func;                      \
      if (status != SANE_STATUS_GOOD) {   \
		DBG( _DBG_ERROR, "UIO error\n" ); \
        return SANE_FALSE;                \
	  }                                   \
    }

/******************** sanei_lm983x functions... will be moved to lib *********/

#define _MIN(a,b)	((a) < (b) ? (a) : (b))
#define _MAX(a,b)	((a) > (b) ? (a) : (b))

#define _CMD_BYTE_CNT	   4
#define _MAX_RETRY         20
#define _LM9831_MAX_REG    0x7f
#define _MAX_TRANSFER_SIZE 60


/*.............................................................................
 *
 */
static SANE_Bool sanei_lm9831_reset( SANE_Int fd )
{
	SANE_Byte cmd_buffer[_CMD_BYTE_CNT];
	SANE_Byte tmp;
	SANE_Int  i;

   	DBG( 7, "sanei_lm9831_reset()\n" );

	for( i = 0; i < _MAX_RETRY; i++ ) {
		
		/* Read the command register and check that the reset bit is not set
		 * If it is set clear it and return false to indicate that
		 * the bit has only now been cleared
		 *
		 * Write the command bytes for a register read
		 * without increment
		 */
		cmd_buffer[0] = 0x01;
		cmd_buffer[1] = 7;
		cmd_buffer[2] = 0;
		cmd_buffer[3] = 1;

		if( _CMD_BYTE_CNT == write( fd, cmd_buffer, _CMD_BYTE_CNT )) {
		
			/* Then read the register in question */
			u_long cbBytes = 1;
			
			if( read( fd, &tmp, cbBytes )) {
			
				if( tmp & 0x20 ) {
				
					SANE_Byte wrt_buffer[_CMD_BYTE_CNT + 1];
					
					/* Write the command bytes for a register read
					 * without increment
					 */
					wrt_buffer[0] = 0x00;
					wrt_buffer[1] = 7;
					wrt_buffer[2] = 0;
					wrt_buffer[3] = 1;
					wrt_buffer[4] = 0; /* <--- The data for the register */

					/* We will attempt to reset it but we really don't do
					 * anything if this fails
					 */
					if( write( fd, wrt_buffer, _CMD_BYTE_CNT + 1)) {
						DBG( 7, "Resetting the LM983x done\n" );
						return SANE_TRUE;
					}	
				}
			}
		}	
	}
	return SANE_FALSE;
}

/*.............................................................................
 *
 */
static SANE_Status
sanei_lm9831_write( SANE_Int fd, SANE_Byte reg, SANE_Byte *buffer,
		    		SANE_Word len, SANE_Bool increment)
{
    SANE_Byte command_buffer[_MAX_TRANSFER_SIZE + _CMD_BYTE_CNT];
    SANE_Int  result;
    SANE_Word bytes, max_len;

  	DBG( 15, "sanei_lm9831_write: fd=%d, reg=%d, len=%d, increment=%d\n", fd,
	         reg, len, increment);
	
	for( bytes = 0; len > 0; ) {

		max_len = _MIN( len, _MAX_TRANSFER_SIZE );

        command_buffer[0] = 0;                  	/* write 			  */
        command_buffer[1] = reg;                	/* LM9831 register	  */
		
		if( increment == SANE_TRUE ) {
			command_buffer[0] += 0x02;				/* increase reg?	  */
			command_buffer[1] += bytes;
		}

		command_buffer[2] = (max_len >> 8) & 0xff;	/* bytes to write MSB */
		command_buffer[3] = max_len & 0xff;			/* bytes to write LSB */

		memcpy( command_buffer + _CMD_BYTE_CNT, buffer + bytes, max_len );

		result = write (fd, command_buffer, max_len + _CMD_BYTE_CNT);
		if( result < 0 ) {
			DBG( 1, "sanei_lm9831_write: write failed (%s)\n",strerror(errno));
			return SANE_STATUS_IO_ERROR;
		}

        if( result != (max_len + _CMD_BYTE_CNT)) {
			DBG( 2, "sanei_lm9831_write: short write (%d/%d)\n",
				 result, max_len + _CMD_BYTE_CNT);

			if( result < _CMD_BYTE_CNT ) {
	      		DBG( 2, "sanei_lm9831_write: couldn't even send command\n" );
       		return SANE_STATUS_IO_ERROR;
	    	}
			DBG( 2, "sanei_lm9831_write: trying again\n" );
		}
		len   -= (result - _CMD_BYTE_CNT);
		bytes += (result - _CMD_BYTE_CNT);
	}		

	DBG(15, "sanei_lm9831_write: succeeded\n");
	return SANE_STATUS_GOOD;
}

/*.............................................................................
 *
 */
static SANE_Status
sanei_lm9831_write_byte (SANE_Int fd, SANE_Byte reg, SANE_Byte value)
{
  return sanei_lm9831_write (fd, reg, &value, 1, 0);
}

/*.............................................................................
 *
 */
static SANE_Status
sanei_lm9831_read (SANE_Int fd, SANE_Byte reg, SANE_Byte *buffer,
		   SANE_Word len, SANE_Bool increment)
{
  SANE_Byte command_buffer[_CMD_BYTE_CNT];
  SANE_Int  result;
  SANE_Word bytes, max_len, read_bytes;

  DBG (15, "sanei_lm9831_read: fd=%d, reg=%d, len=%d, increment=%d\n", fd,
       reg, len, increment);

  if (!buffer)
    {
      DBG (1, "sanei_lm9831_read: error: buffer == NULL\n");
      return SANE_STATUS_NO_MEM;
    }

	for (bytes = 0; len > 0; ) {

		max_len = _MIN(len, 0xFFFF );
		command_buffer[0] = 1;                /* read */
		command_buffer[1] = reg;               /* LM9831 register */
		
		if (increment) {
			command_buffer [0] += 0x02;
			command_buffer [1] += bytes;
		}
		
		command_buffer[2] = (max_len >> 8) & 0xff; /* bytes to read MSB */
		command_buffer[3] = max_len & 0xff;        /* bytes to read LSB */

		DBG (15, "sanei_lm9831_read: writing command: "
	   		"%02x %02x %02x %02x\n", command_buffer[0], command_buffer[1],
	   		command_buffer[2], command_buffer[3]);

		result = write (fd, command_buffer, _CMD_BYTE_CNT);
		if (result < 0) {
			DBG (1, "sanei_lm9831_read: failed while writing command (%s)\n",
	       			strerror(errno));
			return SANE_STATUS_IO_ERROR;
		}

		if (result != _CMD_BYTE_CNT) {
			DBG (2, "sanei_lm9831_read: short write while writing command "
	       			"(%d/_CMD_BYTE_CNT)\n", result);
			return SANE_STATUS_IO_ERROR;
		}

		read_bytes = 0;
		
		do {
			result = read (fd, buffer + bytes + read_bytes, max_len - read_bytes);
			if (result < 0 ) {
			
				/* if we get killed by SIGTERM or so,
				 * we flush the reading stuff
				 */
				if( EINTR == errno ) {
				
					sanei_lm9831_reset( fd );
		      		return SANE_STATUS_EOF;
		      	}	
			
		    	DBG (1, "sanei_lm9831_read: failed while reading (%s)\n",
		   				strerror(errno));
	      		return SANE_STATUS_IO_ERROR;
	    	}
	  		if (result == 0) {
	      		DBG (4, "sanei_lm9831_read: got eof\n");
	      		return SANE_STATUS_EOF;
	    	}
	  		read_bytes += result;
	  		DBG (15, "sanei_lm9831_read: read %d bytes\n", result);
	  		
	  		if (read_bytes != max_len) {
				DBG (2, "sanei_lm9831_read: short read (%d/%d)\n", result,
		   		max_len);
	      		usleep (10000);
	      		DBG (2, "sanei_lm9831_read: trying again\n");
	    	}
		} while (read_bytes < max_len);
      	bytes += (max_len);
      	len -= (max_len);
	}
	DBG(15, "sanei_lm9831_read: succeeded\n");
	return SANE_STATUS_GOOD;
}

#define usbio_ReadReg(fd, reg, value) \
  sanei_lm9831_read (fd, reg, value, 1, 0)

/*.............................................................................
 *
 */
static Bool usbio_WriteReg( SANE_Int handle, SANE_Byte reg, SANE_Byte value )
{
	SANE_Byte data;

	_UIO( sanei_lm9831_write_byte( handle, reg, value));

	/* Flush register 0x02 when register 0x58 is written */
	if( 0x58 == reg ) {

		_UIO( usbio_ReadReg( handle, 2, &data ));

		_UIO( usbio_ReadReg( handle, 2, &data ));
	}

	return SANE_TRUE;
}

/*.............................................................................
 *
 */
static SANE_Status usbio_DetectLM983x( SANE_Int fd, SANE_Byte *version )
{
	SANE_Byte value;

	DBG( _DBG_INFO, "usbio_DetectLM983x\n");

	_UIO( sanei_lm9831_write_byte(fd, 0x07, 0x00));
	_UIO( sanei_lm9831_write_byte(fd, 0x08, 0x02));
	_UIO( usbio_ReadReg(fd, 0x07, &value));
	_UIO( usbio_ReadReg(fd, 0x08, &value));
	_UIO( usbio_ReadReg(fd, 0x69, &value));

	value &= 7;
	if (version)
		*version = value;

	DBG( _DBG_INFO, "usbio_DetectLM983x: found " );

	switch((SANE_Int)value ) {
		
		case 4:	 DBG( _DBG_INFO, "LM9832/3\n" ); break;
		case 3:	 DBG( _DBG_INFO, "LM9831\n" );   break;
		case 2:	 DBG( _DBG_INFO, "LM9830 --> unsupported!!!\n" );
				 return SANE_STATUS_INVAL;
				 break;
		default: DBG( _DBG_INFO, "Unknown chip v%d\n", value );
				 return SANE_STATUS_INVAL;
				 break;
	}
  	
	return SANE_STATUS_GOOD;
}

/*.............................................................................
 *
 */
static SANE_Status usbio_ResetLM983x( pPlustek_Device dev )
{
	SANE_Byte value;
	pHWDef    hw = &dev->usbDev.HwSetting;

	if( hw->fLM9831 ) {

		_UIO( sanei_lm9831_write_byte( dev->fd, 0x07, 0));
		_UIO( sanei_lm9831_write_byte( dev->fd, 0x07,0x20));
		_UIO( sanei_lm9831_write_byte( dev->fd, 0x07, 0));
		_UIO( usbio_ReadReg( dev->fd, 0x07, &value));
		if (value != 0) {
    		DBG( _DBG_ERROR, "usbio_ResetLM983x: "
							 "reset wasn't successful, status=%d\n", value );
			return SANE_STATUS_INVAL;
    	}
	} else {
		_UIO( sanei_lm9831_write_byte( dev->fd, 0x07, 0));
	}

	return SANE_STATUS_GOOD;
}

/* END PLUSTEK-USBIO.C ......................................................*/
