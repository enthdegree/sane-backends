/*.............................................................................
 * Project : SANE library for Plustek parallelport flatbed scanners.
 *.............................................................................
 */

/* @file plustek-pp_misc.c
 * @brief here we have some helpful functions
*
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 1998 Plustek Inc.
 * Copyright (C) 2000-2003 Gerhard Jaeger <gerhard@gjaeger.de>
 * also based on the work done by Rick Bronson
 *
 * History:
 * - 0.30 - initial version
 * - 0.31 - no changes
 * - 0.32 - moved the parport functions inside this module
 *        - now using the information, the parport-driver provides
 *        - for selecting the port-mode this driver uses
 * - 0.33 - added code to use faster portmodes
 * - 0.34 - added sample code for changing from ECP to PS/2 bidi mode
 * - 0.35 - added Kevins' changes (new function miscSetFastMode())
 *        - moved function initPageSettings() to module models.c
 * - 0.36 - added random generator
 *        - added additional debug messages
 *        - changed prototype of MiscInitPorts()
 *        - added miscPreemptionCallback()
 * - 0.37 - changed inb_p/outb_p to macro calls (kernel-mode)
 *        - added MiscGetModelName()
 *        - added miscShowPortModes()
 * - 0.38 - fixed a small bug in MiscGetModelName()
 * - 0.39 - added forceMode support
 * - 0.40 - no changes
 * - 0.41 - merged Kevins' patch to make EPP(ECP) work
 * - 0.42 - changed get_fast_time to _GET_TIME
 *        - changed include names
 * .
 * <hr>
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
 * <hr>
 */
#include "plustek-pp_scan.h"

/*************************** some definitions ********************************/

#ifndef __KERNEL__
# define PPA_PROBE_SPP   0x0001
# define PPA_PROBE_PS2   0x0002
# define PPA_PROBE_ECR   0x0010
# define PPA_PROBE_EPP17 0x0100
# define PPA_PROBE_EPP19 0x0200
#else

/*
 * the parport driver in Kernel 2.4 has changed. It does report the
 * possible modes in a different, more general way. As long, as
 * we do not use the parport-module change mode facility, I assume
 * the following correlations
 */
#if defined LINUX_24 || defined LINUX_26
# define PARPORT_MODE_PCPS2		PARPORT_MODE_TRISTATE
# define PARPORT_MODE_PCEPP		PARPORT_MODE_EPP
# define PARPORT_MODE_PCECPPS2	PARPORT_MODE_TRISTATE
# define PARPORT_MODE_PCECPEPP  PARPORT_MODE_EPP
# define PARPORT_MODE_PCECR		PARPORT_MODE_ECP
#endif
#endif

#define _A 16807         /* multiplier */
#define _M 2147483647L   /* 2**31 - 1 */
#define _Q 127773L       /* m div a */
#define _R 2836          /* m mod a */

/*************************** some local vars *********************************/

static int  port_feature;
static long randomnum = 1;

#ifdef __KERNEL__
static int portIsClaimed[_MAX_PTDEVS] = { [0 ... (_MAX_PTDEVS-1)] = 0 };

MODELSTR;	/* is a static char array (see plustek-share.h) */

#else
static int portIsClaimed[_MAX_PTDEVS] = { 0, 0, 0, 0 };
#endif

/*************************** local functions *********************************/

#ifdef __KERNEL__

/** display the avaialable port-modes
 */
#ifdef DEBUG
static void miscShowPortModes( int modes )
{
	DBG( DBG_LOW, "parport-modi:" );

	if( modes & PARPORT_MODE_PCSPP )
		DBG( DBG_LOW, " SPP" );

	if( modes & PARPORT_MODE_PCPS2 )
		DBG( DBG_LOW, " PS/2" );

	if( modes & PARPORT_MODE_PCEPP )
		DBG( DBG_LOW, " EPP" );

	if( modes & PARPORT_MODE_PCECR )
		DBG( DBG_LOW, " ECP" );

	if( modes & PARPORT_MODE_PCECPEPP )
		DBG( DBG_LOW, " EPP(ECP)" );

	if( modes & PARPORT_MODE_PCECPPS2 )
		DBG( DBG_LOW, " PS/2(ECP)" );

	DBG( DBG_LOW, "\n" );
}
#endif

/** probe the parallel port
 */
static int initPortProbe( pScanData ps )
{
    int retv = 0;

	/* clear the controls */
	ps->IO.lastPortMode = 0xFFFF;

	if( NULL != ps->pardev )
		retv = ps->pardev->port->modes;
    return retv;
}

/** will be called by the parport module when we already have access, but
 * another module wants access to the port...
 */
static int miscPreemptionCallback( pVoid data )
{
	pScanData ps = (pScanData)data;

	if( NULL != ps ) {

		/* never release during scanning */
		if( ps->DataInf.dwScanFlag & _SCANNER_SCANNING ) {
			DBG( DBG_LOW, "no way!!!\n" );
			return 1;
		}
	}

	/* let the port go...*/
	return 0;
}

/** depending on the reported possible port modes, we try to set a faster mode
 * than SPP
 */
static int miscSetFastMode( pScanData ps )
{
	UChar a, b;

	/*
	 *  when previously found the EPP mode, break right here
	 */
	if (( _PORT_EPP == ps->IO.portMode ) && (!(port_feature & PARPORT_MODE_PCECR)))
 		return _OK;

 	/* CHECK REMOVE: from here we should have SPP (Paranoia Code !) */
	if (( _PORT_SPP != ps->IO.portMode ) && (!(port_feature & PARPORT_MODE_PCECR)))
 		return _OK;

	DBG(DBG_LOW, "Trying faster mode...\n" );

	/*
	 * ECP mode usually has sub-modes of EPP and/or PS2.
	 * First we try to set EPP
	 */
    if((port_feature & PARPORT_MODE_PCECR) &&
									(port_feature & PARPORT_MODE_PCECPEPP)){

        DBG(DBG_LOW, "Attempting to set EPP from ECP mode.\n" );

        a = _INB_ECTL(ps);  				/* get current ECR				*/
		ps->IO.lastPortMode = a;	     	/* save it for restoring later	*/
        a = (a & 0x1F) | 0x80;  	     	/* set to EPP					*/
        _OUTB_ECTL(ps, a);					/* write it back				*/
		_DO_UDELAY(1);

		/*
		 * It is probably unnecessary to
		 * do this check but it makes me feel better
		 */
        b = _INB_ECTL(ps);					/* check to see if port set */
        if( a == b ) {
            DBG( DBG_LOW, "Port is set to (ECP) EPP mode.\n" );
            ps->IO.portMode = _PORT_EPP;
			return _OK;

        } else {
            DBG( DBG_LOW, "Port could not be set to (ECP) EPP mode. "
														"Using SPP mode.\n" );
            _OUTB_ECTL(ps,(Byte)ps->IO.lastPortMode); 		/* restore */
			_DO_UDELAY(1);
		    ps->IO.portMode = _PORT_SPP;

			/* go ahead and try with other settings...*/
        }
    }

	/* If port cannot be set to EPP, try PS2 */
    if((port_feature & PARPORT_MODE_PCECR) &&
									(port_feature & PARPORT_MODE_PCECPPS2)) {

        DBG(DBG_LOW, "Attempting to set PS2 from ECPPS2 mode.\n" );

        a = _INB_ECTL(ps);  				/* get current ECR				*/
		ps->IO.lastPortMode = a;	     	/* save it for restoring later 	*/

		/* set to Fast Centronics/bi-directional/PS2 */
        a = (a & 0x1F) | 0x20;  	     	
        _OUTB_ECTL(ps,a);					/* write it back */
		_DO_UDELAY(1);

		/*
		 * It is probably unnecessary to do this check
		 * but it makes me feel better
		 */
        b = _INB_ECTL(ps);					/* check to see if port set */
        if (a == b) {
            DBG(DBG_LOW, "Port is set to (ECP) PS2 bidirectional mode.\n");
            ps->IO.portMode = _PORT_BIDI;
			return _OK;
        } else {
        	DBG(DBG_LOW, "Port could not be set to (ECP) PS2 mode. "
														"Using SPP mode.\n");
            _OUTB_ECTL(ps,ps->IO.lastPortMode);	/* restore */
			_DO_UDELAY(1);
		    ps->IO.portMode = _PORT_SPP;

			/* next mode, last attempt... */
        }
    }

	/*
	 * Some BIOS/cards have only a Bi-directional/PS2 mode (no EPP).
	 * Make one last attemp to set to PS2 mode.
	 */
    if ( port_feature & PARPORT_MODE_PCPS2 ){

        DBG(DBG_LOW, "Attempting to set PS2 mode.\n" );

        a = _INB_CTRL(ps); 		    /* get current setting of control register*/
		ps->IO.lastPortMode = a;	/* save it for restoring later            */
        a = a | 0x20;  			    /* set bit 5 of control reg              */
		_OUTB_CTRL(ps,a); 		/* set to Fast Centronics/bi-directional/PS2 */
		_DO_UDELAY(1);
        a = 0;

		_OUTB_DATA(ps,0x55);
		_DO_UDELAY(1);
		if ((inb(ps->IO.portBase)) != 0x55)	/* read data */
			a++;

		_OUTB_DATA(ps,0xAA);
		_DO_UDELAY(1);

		if (_INB_DATA(ps) != 0xAA)   /* read data */
			a++;

        if (2 == a) {
            DBG(DBG_LOW, "Port is set to PS2 bidirectional mode.\n");
            ps->IO.portMode = _PORT_BIDI;
			return _OK;
        }
        else {
            DBG(DBG_LOW, "Port could not be set to PS2 mode. "
														"Using SPP mode.\n");
            _OUTB_CTRL(ps,(Byte)ps->IO.lastPortMode);		/* restore */
			_DO_UDELAY(1);
            ps->IO.portMode = _PORT_SPP;
        }
    }

	/*
	 * reaching this point, we're back in SPP mode and there's no need
 	 * to restore at shutdown...
	 */
	ps->IO.lastPortMode = 0xFFFF;

	return _OK;
}

/** check the state of the par-port and switch to EPP-mode if possible
 */
static int miscSetPortMode( pScanData ps )
{
	/* try to detect the port settings, SPP seems to work in any case ! */
	port_feature = initPortProbe( ps );

#ifdef DEBUG
	miscShowPortModes( port_feature );
#endif

	switch( ps->IO.forceMode ) {

		case 1:
			DBG( DBG_LOW, "Use of SPP-mode enforced\n" );
			ps->IO.portMode = _PORT_SPP;
			return _OK;
			break;

        case 2:
	    	DBG( DBG_LOW, "Use of EPP-mode enforced\n" );
	        ps->IO.portMode = _PORT_EPP;
    		return _OK;
            break;

        default:
            break;
	}

	if( !(port_feature & PARPORT_MODE_PCEPP)) {

		if( !(port_feature & PARPORT_MODE_PCSPP )) {
			_PRINT("\nThis Port supports not the  SPP- or EPP-Mode\n" );
			_PRINT("Please activate SPP-Mode, EPP-Mode or\nEPP + ECP-Mode!\n");
			return _E_NOSUPP;
		} else {
			DBG(DBG_LOW, "Using SPP-mode\n" );
		    ps->IO.portMode = _PORT_SPP;
		}
    } else {
		DBG(DBG_LOW, "Using EPP-mode\n" );
	    ps->IO.portMode = _PORT_EPP;
	}

	/* else try to set to a faster mode than SPP */
	return miscSetFastMode( ps );
}
#endif

/** miscNextLongRand() -- generate 2**31-2 random numbers
**
**  public domain by Ray Gardner
**
**  based on "Random Number Generators: Good Ones Are Hard to Find",
**  S.K. Park and K.W. Miller, Communications of the ACM 31:10 (Oct 1988),
**  and "Two Fast Implementations of the 'Minimal Standard' Random
**  Number Generator", David G. Carta, Comm. ACM 33, 1 (Jan 1990), p. 87-88
**
**  linear congruential generator f(z) = 16807 z mod (2 ** 31 - 1)
**
**  uses L. Schrage's method to avoid overflow problems
*/
static Long miscNextLongRand( Long seed )
{
	ULong lo, hi;

    lo = _A * (Long)(seed & 0xFFFF);
    hi = _A * (Long)((ULong)seed >> 16);
    lo += (hi & 0x7FFF) << 16;
    if (lo > _M) {

		lo &= _M;
        ++lo;
	}
    lo += hi >> 15;
    if (lo > _M) {
		lo &= _M;
        ++lo;
	}

	return (Long)lo;
}

/** initialize the random number generator
 */
static void miscSeedLongRand( ULong seed )
{
	randomnum = seed ? (seed & _M) : 1;  /* nonzero seed */
}

/************************ exported functions *********************************/

/** allocate and initialize some memory for the scanner structure
 */
_LOC pScanData MiscAllocAndInitStruct( void )
{
	pScanData ps;

	ps = (pScanData)_KALLOC(sizeof(ScanData), GFP_KERNEL);

	if( NULL != ps ) {
		MiscReinitStruct( ps );
	}

	DBG( DBG_HIGH, "ScanData = 0x%08lx\n", (ULong)ps );
	return ps;	
}

/** re-initialize the memory for the scanner structure
 */
_LOC int MiscReinitStruct( pScanData ps )
{
	if( NULL == ps )
		return _E_NULLPTR;

	memset( ps, 0, sizeof(ScanData));
		
 	/*
	 * first init all constant stuff in ScanData
	 */
	ps->sCaps.Version = ((_PTDRV_V1 << 8) | _PTDRV_V0);

	ps->bCurrentSpeed = 1;
	ps->pbMapRed      =  ps->a_bMapTable;
	ps->pbMapGreen    = &ps->a_bMapTable[256];
	ps->pbMapBlue     = &ps->a_bMapTable[512];
	ps->sCaps.wIOBase = _NO_BASE;

	/* use memory address to seed the generator */
	miscSeedLongRand((Long)ps);

	DBG( DBG_HIGH, "Init settings done\n" );
	return _OK;
}

/** in USER-Mode:   probe the specified port and try to get the port-mode
 *  in KERNEL-Mode: only use the modes, the driver returns
 */
_LOC int MiscInitPorts( pScanData ps, int port )
{
#ifdef __KERNEL__
	int status;

	if( NULL == ps )
		return _E_NULLPTR;

    /*
     * Get access to the ports
     */
    ps->IO.portBase = (UShort)port;

	status = miscSetPortMode(ps);

	if( _OK != status ) {
		ps->sCaps.wIOBase = _NO_BASE;
		ps->IO.portBase = _NO_BASE;
		return status;
	}

	/*
 	 * the port settings
	 */
    ps->IO.pbSppDataPort = (UShort)port;
    ps->IO.pbStatusPort  = (UShort)port+1;
    ps->IO.pbControlPort = (UShort)port+2;
    ps->IO.pbEppDataPort = (UShort)port+4;
    
#else
	int mode;
	
	if( NULL == ps )
		return _E_NULLPTR;

	if( SANE_STATUS_GOOD != sanei_pp_getmode( ps->pardev, &mode )) {
		DBG(DBG_HIGH, "Cannot get port mode!\n" );
		return _E_NO_PORT;
	}

	if( mode & SANEI_PP_MODE_SPP ) {
		DBG(DBG_LOW, "Setting SPP-mode\n" );
		ps->IO.portMode = _PORT_SPP;
	}
	if( mode & SANEI_PP_MODE_BIDI ) {
		DBG(DBG_LOW, "Setting PS/2-mode\n" );
		ps->IO.portMode = _PORT_BIDI;
	}
	if( mode & SANEI_PP_MODE_EPP ) {
		DBG(DBG_LOW, "Setting EPP-mode\n" );
		ps->IO.portMode = _PORT_EPP;
	}
	if( mode & SANEI_PP_MODE_ECP ) {
		DBG(DBG_HIGH, "ECP detected --> not supported\n" );
		return _E_NOSUPP;
	}

	_VAR_NOT_USED( port );
#endif
	return _OK;
}

/** here we restore the port
 */
_LOC void MiscRestorePort( pScanData ps )
{
#ifdef __KERNEL__
	if( 0 == ps->IO.pbSppDataPort )
		return;
#endif

    DBG(DBG_LOW,"MiscRestorePort()\n");

	/* don't restore if not necessary */
	if( 0xFFFF == ps->IO.lastPortMode ) {
	    DBG(DBG_LOW,"- no need to restore portmode !\n");
		return;
	}

    /*Restore Port-Mode*/
#ifdef __KERNEL__
	if (port_feature & PARPORT_MODE_PCECR ){
		_OUTB_ECTL( ps, (Byte)ps->IO.lastPortMode );
		_DO_UDELAY(1);
    } else {
		_OUTB_CTRL( ps, (Byte)ps->IO.lastPortMode );
		_DO_UDELAY(1);
    }
#else
    if (port_feature & PPA_PROBE_ECR ){
		_OUTB_ECTL(ps,ps->IO.lastPortMode);
    }
#endif
}

/** starts a timer
 */
_LOC _INL void MiscStartTimer( pTimerDef timer , unsigned long us)
{
    struct timeval start_time;

#ifdef __KERNEL__
	_GET_TIME( &start_time );
#else
	gettimeofday(&start_time, NULL);	
#endif

    *timer =  start_time.tv_sec * 1e6 + start_time.tv_usec + us;
}

/** checks for timeout
 */
_LOC _INL int MiscCheckTimer( pTimerDef timer )
{
    struct timeval current_time;

#ifdef __KERNEL__
	_GET_TIME( &current_time );
#else
	gettimeofday(&current_time, NULL);
#endif

    if (current_time.tv_sec * 1e6 + current_time.tv_usec > *timer) {
		return _E_TIMEOUT;
    } else {
#ifdef __KERNEL__       
		schedule();
/*#else
		sched_yield();
*/
#endif
		return _OK;
	}
}

/** checks the function pointers
 */
#ifdef DEBUG
_LOC Bool MiscAllPointersSet( pScanData ps )
{
	ULong  i;
	pULong ptr;

	for( ptr = (pULong)&ps->OpenScanPath, i = 1;
		 ptr <= (pULong)&ps->ReadOneImageLine; ptr++, i++ ) {

		if( NULL == (pVoid)*ptr ) {
			DBG( DBG_HIGH, "Function pointer not set (pos = %lu) !\n", i );
			return _FALSE;
		}
	}

	return _TRUE;
}
#endif

/** registers this driver to use port "portAddr" (KERNEL-Mode only)
 */
_LOC int MiscRegisterPort( pScanData ps, int portAddr )
{
#ifndef __KERNEL__
	DBG( DBG_LOW, "Assigning port handle %i\n", portAddr );
    ps->pardev = portAddr;
#else
	struct parport *pp;

	DBG( DBG_LOW, "Requested port at 0x%02x\n", portAddr );

	pp 		   = parport_enumerate();
	ps->pardev = NULL;

	if( NULL == pp ) {
		return _E_PORTSEARCH;
	}

	/*
	 * go through the list
	 */
	for( ps->pp = NULL; NULL != pp; ) {

		if( pp->base == (unsigned long)portAddr ) {
			DBG( DBG_LOW, "Requested port (0x%02x) found\n", portAddr );
			DBG( DBG_LOW, "Port mode reported: (0x%04x)\n",  pp->modes );
			ps->pp = pp;
			break;
		}

		pp = pp->next;
	}

	if( NULL == ps->pp ) {
		return _E_NO_PORT;
	}

	/*
	 * register this device
	 */
	ps->pardev = parport_register_device( ps->pp, "Plustek Driver",
						    miscPreemptionCallback, NULL, NULL, 0, (pVoid)ps );

	if( NULL == ps->pardev ) {
		return _E_REGISTER;
	}

	DBG( DBG_LOW, "Port for device %lu registered\n", ps->devno );
#endif

	portIsClaimed[ps->devno] = 0;

	return _OK;
}

/** unregisters the port from driver (KERNEL-Mode only)
 */
_LOC void MiscUnregisterPort( pScanData ps )
{
#ifdef __KERNEL__
	if( NULL != ps->pardev ) {
		DBG( DBG_LOW, "Port unregistered\n" );
		parport_unregister_device( ps->pardev );
	}
#else
	sanei_pp_close( ps->pardev );
#endif
}

/** try to claim the port (KERNEL-Mode only)
 */
_LOC int MiscClaimPort( pScanData ps )
{
#ifdef __KERNEL__
	if( 0 == portIsClaimed[ps->devno] ) {

		DBG( DBG_HIGH, "Try to claim the parport\n" );
		if( 0 != parport_claim( ps->pardev ))
			return _E_BUSY;
	}
#else
	sanei_pp_claim( ps->pardev );
#endif
	portIsClaimed[ps->devno]++;

	return _OK;
}

/** release previously claimed port (KERNEL-Mode only)
 */
_LOC void MiscReleasePort( pScanData ps )
{
	if( portIsClaimed[ps->devno] > 0 ) {
		portIsClaimed[ps->devno]--;

		if( 0 == portIsClaimed[ps->devno] ) {
			DBG( DBG_HIGH, "Releasing parport\n" );
#ifdef __KERNEL__
			parport_release( ps->pardev );
#else
			sanei_pp_release( ps->pardev );
#endif
		}
	}
}

/*.............................................................................
 * get random number
 */
_LOC Long MiscLongRand( void )
{
	randomnum = miscNextLongRand( randomnum );

	return randomnum;
}

/*.............................................................................
 * according to the id, the function returns a pointer to the model name
 */
_LOC const char *MiscGetModelName( UShort id )
{
	DBG( DBG_HIGH, "MiscGetModelName - id = %i\n", id );

	if( MODEL_OP_PT12 < id )
		return ModelStr[0];

	return ModelStr[id];
}

/* END PLUSTEK-PP_MISC.C ....................................................*/
