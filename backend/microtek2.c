/***************************************************************************
 * SANE - Scanner Access Now Easy.

   microtek2.c 

   This file (C) 1998, 1999 Bernd Schroeder

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

 ***************************************************************************

   This file implements a SANE backend for the Microtek scanners with
   SCSI-2 command set.

   (feedback to:  bernd@aquila.muc.de)

 ***************************************************************************/


#ifdef _AIX
# include <lalloca.h>   /* MUST come first for AIX! */
#endif

#include "sane/config.h"
#include <lalloca.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#include <math.h>

#include <sane/sane.h>
#include <sane/sanei.h>
#include <sane/sanei_config.h>
#include <sane/sanei_scsi.h>
#include <sane/saneopts.h>

#define BACKEND_NAME microtek2
#include <sane/sanei_backend.h>

#include "microtek2.h"



static int md_num_devices = 0;          /* number of devices from config file */
static Microtek2_Device *md_first_dev = NULL;        /* list of known devices */
static Microtek2_Scanner *ms_first_handle = NULL;    /* list of open scanners */
static double md_strip_height = 1.0;    /* inch */
static char *md_no_backtracking = "off"; /* enable/disable option for */
                                         /* backtracking */
static char *md_lightlid35 = "off";     /* enable/disable lightlid35 option */
static int ms_dump_clear = 1;
static int ms_dump = 0;                 /* from config file: */
                                        /* 1: inquiry + scanner attributes */
                                        /* 2: + all scsi commands and data */
                                        /* 3: + all scan data */



/*---------- sane_cancel() ---------------------------------------------------*/

void
sane_cancel (SANE_Handle handle)
{
    Microtek2_Scanner *ms = handle;

    DBG(30, "sane_cancel: handle=%p\n", handle);
    
    ms->cancelled = SANE_TRUE;
    ms->scanning = SANE_FALSE;
}


/*---------- sane_close() ----------------------------------------------------*/

void
sane_close (SANE_Handle handle)
{
    Microtek2_Scanner *ms = handle;

    DBG(30, "sane_close: ms=%p\n", ms);

    if ( ! ms )
        return;

    /* free malloc'ed stuff */
    cleanup_scanner(ms);

    /* remove Scanner from linked list */
    if ( ms_first_handle == ms )
        ms_first_handle = ms->next;
    else 
      {
        Microtek2_Scanner *ts = ms_first_handle;
        while ( (ts != NULL) && (ts->next != ms) )
            ts = ts->next;
        ts->next = ts->next->next; /* == ms->next */
      }
    
    free(ms);
    ms = NULL;
}


/*---------- sane_control_option() -------------------------------------------*/

SANE_Status
sane_control_option(SANE_Handle handle, SANE_Int option, 
                    SANE_Action action, void *value, SANE_Int *info)
{
    Microtek2_Scanner *ms = handle;
    Microtek2_Device *md;
    Microtek2_Option_Value *val;
    SANE_Option_Descriptor *sod;
    SANE_Status status;
    int value_changed;


    md = ms->dev;
    val = &ms->val[0];
    sod = &ms->sod[0];
    value_changed = 0;

    if ( ms->scanning )
        return SANE_STATUS_DEVICE_BUSY;
    
    if ( option < 0 || option >= NUM_OPTIONS ) 
      {
        DBG(10, "sane_control_option: option %d invalid\n", option);
        return SANE_STATUS_INVAL;
      }
    
    if ( ! SANE_OPTION_IS_ACTIVE(ms->sod[option].cap) ) 
      {
        DBG(10, "sane_control_option: option %d not active\n", option);
        return SANE_STATUS_INVAL;
      }

    if ( info )
        *info = 0;

    switch ( action ) 
      {
        case SANE_ACTION_GET_VALUE:
          switch ( option ) 
            {
              /* word options */   
              case OPT_RESOLUTION:
              case OPT_X_RESOLUTION:
              case OPT_Y_RESOLUTION:
              case OPT_THRESHOLD:
              case OPT_TL_X:
              case OPT_TL_Y:
              case OPT_BR_X:
              case OPT_BR_Y:
              case OPT_PREVIEW:
              case OPT_BRIGHTNESS:
              case OPT_CONTRAST:
              case OPT_SHADOW:
              case OPT_SHADOW_R:
              case OPT_SHADOW_G:
              case OPT_SHADOW_B:
              case OPT_MIDTONE:
              case OPT_MIDTONE_R:
              case OPT_MIDTONE_G:
              case OPT_MIDTONE_B:
              case OPT_HIGHLIGHT:
              case OPT_HIGHLIGHT_R:
              case OPT_HIGHLIGHT_G:
              case OPT_HIGHLIGHT_B:
              case OPT_EXPOSURE:
              case OPT_EXPOSURE_R:
              case OPT_EXPOSURE_G:
              case OPT_EXPOSURE_B:
              case OPT_GAMMA_SCALAR:
              case OPT_GAMMA_SCALAR_R:
              case OPT_GAMMA_SCALAR_G:
              case OPT_GAMMA_SCALAR_B:
                *(SANE_Word *) value = val[option].w;
                if (sod[option].type == SANE_TYPE_FIXED )
                    DBG(50, "sane_control_option: opt=%d, act=%d, val=%f\n",
                             option, action, SANE_UNFIX(val[option].w)); 
                else
                    DBG(50, "sane_control_option: opt=%d, act=%d, val=%d\n",
                             option, action, val[option].w); 
                return SANE_STATUS_GOOD;
              
              /* boolean options */
              case OPT_RESOLUTION_BIND:
              case OPT_DISABLE_BACKTRACK:
              case OPT_GAMMA_BIND:
                *(SANE_Bool *) value = val[option].w;
                DBG(50, "sane_control_option: opt=%d, act=%d, val=%d\n",
                         option, action, val[option].w); 
                return SANE_STATUS_GOOD;
              
              /* string options */ 
              case OPT_SOURCE:
              case OPT_MODE:
              case OPT_HALFTONE:
              case OPT_CHANNEL:
              case OPT_GAMMA_MODE:
                strcpy(value, val[option].s);
                DBG(50, "sane_control_option: opt=%d, act=%d, val=%s\n",
                         option, action, val[option].s); 
                return SANE_STATUS_GOOD;
              
              /* word array options */
              case OPT_GAMMA_CUSTOM:
              case OPT_GAMMA_CUSTOM_R:
              case OPT_GAMMA_CUSTOM_G:
              case OPT_GAMMA_CUSTOM_B:
                memcpy(value, val[option].wa, sod[option].size);
                return SANE_STATUS_GOOD;

              /* others */
              case OPT_NUM_OPTS:
                *(SANE_Word *) value = NUM_OPTIONS;
                return SANE_STATUS_GOOD;
 
              default:
                return SANE_STATUS_UNSUPPORTED;
            }
          break;

        case SANE_ACTION_SET_VALUE:
          if ( ! SANE_OPTION_IS_SETTABLE(sod[option].cap) )
            {
              DBG(10, "sane_control_option: trying to set unsettable option\n");
              return SANE_STATUS_INVAL;
            }

          /* do not check OPT_BR_Y, xscanimage sometimes tries to set */
          /* it to a too big value; bug in xscanimage ? */
          if ( option != OPT_BR_Y )
            {
              status = sanei_constrain_value(ms->sod + option, value, info);
              if (status != SANE_STATUS_GOOD)
                {
                  DBG(10, "sane_control_option: invalid option value\n");
                  return status;
                }
            }
 
          switch ( sod[option].type )
            {
              case SANE_TYPE_BOOL:
                DBG(50, "sane_control_option: option=%d, action=%d, value=%d\n",
                         option, action, *(SANE_Int *) value);
                if ( val[option].w == *(SANE_Bool *) value ) /* no change */
                    return SANE_STATUS_GOOD;
                val[option].w = *(SANE_Bool *) value;
                break;
              case SANE_TYPE_INT:
                if ( sod[option].size == sizeof(SANE_Int) )
                  {
                    /* word option */
                    DBG(50, "sane_control_option: option=%d, action=%d, "
                            "value=%d\n", option, action, *(SANE_Int *) value);
                    if ( val[option].w == *(SANE_Int *) value ) /* no change */
                        return SANE_STATUS_GOOD;
                    val[option].w = *(SANE_Int *) value;
                  }
                else
                  {
                    /* word array option */
                    memcpy(val[option].wa, value, sod[option].size);
                  } 
                break;
              case SANE_TYPE_FIXED:
                DBG(50, "sane_control_option: option=%d, action=%d, value=%f\n",
                         option, action, SANE_UNFIX( *(SANE_Fixed *) value));
                if ( val[option].w == *(SANE_Fixed *) value ) /* no change */
                    return SANE_STATUS_GOOD;
                val[option].w = *(SANE_Fixed *) value;
                break;
              case SANE_TYPE_STRING:
                DBG(50, "sane_control_option: option=%d, action=%d, value=%s\n",
                         option, action, (SANE_String) value);
                if ( strcmp(val[option].s, (SANE_String) value) == 0 )
                    return SANE_STATUS_GOOD;         /* no change */
                if ( val[option].s )
                    free((void *) val[option].s);
                val[option].s = strdup(value); 
                if ( val[option].s == NULL ) 
                  {
                    DBG(1, "sane_control_option: strdup failed\n");
                    return SANE_STATUS_NO_MEM;
                  }
                break;
              default:
                DBG(1, "sane_control_option: unknown type %d\n",
                        sod[option].type);
                break;
            }

          switch ( option ) 
            {
              case OPT_RESOLUTION:
              case OPT_X_RESOLUTION:
              case OPT_Y_RESOLUTION:
              case OPT_TL_X:
              case OPT_TL_Y:
              case OPT_BR_X:
              case OPT_BR_Y:
                if ( info )
                    *info |= SANE_INFO_RELOAD_PARAMS;
              case OPT_DISABLE_BACKTRACK:
              case OPT_PREVIEW:
              case OPT_BRIGHTNESS:
              case OPT_THRESHOLD:
              case OPT_CONTRAST:
              case OPT_EXPOSURE:
              case OPT_EXPOSURE_R:
              case OPT_EXPOSURE_G:
              case OPT_EXPOSURE_B:
              case OPT_GAMMA_SCALAR:
              case OPT_GAMMA_SCALAR_R:
              case OPT_GAMMA_SCALAR_G:
              case OPT_GAMMA_SCALAR_B:
              case OPT_GAMMA_CUSTOM:
              case OPT_GAMMA_CUSTOM_R:
              case OPT_GAMMA_CUSTOM_G:
              case OPT_GAMMA_CUSTOM_B:
              case OPT_HALFTONE:
                return SANE_STATUS_GOOD;

              case OPT_SOURCE:
                if ( info )
                    *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
                if ( strcmp(val[option].s, MD_SOURCESTRING_FLATBED) == 0 )
                    md->scan_source = MD_SOURCE_FLATBED;
                else if ( strcmp(val[option].s, MD_SOURCESTRING_TMA) == 0 )
                    md->scan_source = MD_SOURCE_TMA;
                else if ( strcmp(val[option].s, MD_SOURCESTRING_ADF) == 0 )
                    md->scan_source = MD_SOURCE_ADF;
                else if ( strcmp(val[option].s, MD_SOURCESTRING_STRIPE) == 0 )
                    md->scan_source = MD_SOURCE_STRIPE;
                else if ( strcmp(val[option].s, MD_SOURCESTRING_SLIDE) == 0 )
                    md->scan_source = MD_SOURCE_SLIDE;
                else
                  {
                    DBG(1, "sane_control_option: unsupported option %s\n",
                            val[option].s);
                    return SANE_STATUS_UNSUPPORTED;
                  }
 
                init_options(ms, md->scan_source);
                return SANE_STATUS_GOOD;

              case OPT_MODE:
                if ( info )
                    *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;

                status = set_option_dependencies(sod, val);
                if ( status != SANE_STATUS_GOOD )
                    return status;
                return SANE_STATUS_GOOD;

              case OPT_CHANNEL:
                if ( info )
                    *info |= SANE_INFO_RELOAD_OPTIONS;
                if ( strcmp(val[option].s, MD_CHANNEL_MASTER) == 0 )
                  {
                    sod[OPT_SHADOW].cap &= ~SANE_CAP_INACTIVE;           
                    sod[OPT_MIDTONE].cap &= ~SANE_CAP_INACTIVE;           
                    sod[OPT_HIGHLIGHT].cap &= ~SANE_CAP_INACTIVE;   
                    sod[OPT_EXPOSURE].cap &= ~SANE_CAP_INACTIVE;   
                    sod[OPT_SHADOW_R].cap |= SANE_CAP_INACTIVE;           
                    sod[OPT_MIDTONE_R].cap |= SANE_CAP_INACTIVE;           
                    sod[OPT_HIGHLIGHT_R].cap |= SANE_CAP_INACTIVE;   
                    sod[OPT_EXPOSURE_R].cap |= SANE_CAP_INACTIVE;   
                    sod[OPT_SHADOW_G].cap |= SANE_CAP_INACTIVE;           
                    sod[OPT_MIDTONE_G].cap |= SANE_CAP_INACTIVE;           
                    sod[OPT_HIGHLIGHT_G].cap |= SANE_CAP_INACTIVE;   
                    sod[OPT_EXPOSURE_G].cap |= SANE_CAP_INACTIVE;   
                    sod[OPT_SHADOW_B].cap |= SANE_CAP_INACTIVE;           
                    sod[OPT_MIDTONE_B].cap |= SANE_CAP_INACTIVE;           
                    sod[OPT_HIGHLIGHT_B].cap |= SANE_CAP_INACTIVE;   
                    sod[OPT_EXPOSURE_B].cap |= SANE_CAP_INACTIVE;   
                  }
                else if ( strcmp(val[option].s, MD_CHANNEL_RED) == 0 )
                  {
                    sod[OPT_SHADOW].cap |= SANE_CAP_INACTIVE;           
                    sod[OPT_MIDTONE].cap |= SANE_CAP_INACTIVE;           
                    sod[OPT_HIGHLIGHT].cap |= SANE_CAP_INACTIVE;   
                    sod[OPT_EXPOSURE].cap |= SANE_CAP_INACTIVE;   
                    sod[OPT_SHADOW_R].cap &= ~SANE_CAP_INACTIVE;           
                    sod[OPT_MIDTONE_R].cap &= ~SANE_CAP_INACTIVE;           
                    sod[OPT_HIGHLIGHT_R].cap &= ~SANE_CAP_INACTIVE;   
                    sod[OPT_EXPOSURE_R].cap &= ~SANE_CAP_INACTIVE;   
                    sod[OPT_SHADOW_G].cap |= SANE_CAP_INACTIVE;           
                    sod[OPT_MIDTONE_G].cap |= SANE_CAP_INACTIVE;           
                    sod[OPT_HIGHLIGHT_G].cap |= SANE_CAP_INACTIVE;   
                    sod[OPT_EXPOSURE_G].cap |= SANE_CAP_INACTIVE;   
                    sod[OPT_SHADOW_B].cap |= SANE_CAP_INACTIVE;           
                    sod[OPT_MIDTONE_B].cap |= SANE_CAP_INACTIVE;           
                    sod[OPT_HIGHLIGHT_B].cap |= SANE_CAP_INACTIVE;   
                    sod[OPT_EXPOSURE_B].cap |= SANE_CAP_INACTIVE;   
                  }
                else if ( strcmp(val[option].s, MD_CHANNEL_GREEN) == 0 )
                  {
                    sod[OPT_SHADOW].cap |= SANE_CAP_INACTIVE;           
                    sod[OPT_MIDTONE].cap |= SANE_CAP_INACTIVE;           
                    sod[OPT_HIGHLIGHT].cap |= SANE_CAP_INACTIVE;   
                    sod[OPT_EXPOSURE].cap |= SANE_CAP_INACTIVE;   
                    sod[OPT_SHADOW_R].cap |= SANE_CAP_INACTIVE;           
                    sod[OPT_MIDTONE_R].cap |= SANE_CAP_INACTIVE;           
                    sod[OPT_HIGHLIGHT_R].cap |= SANE_CAP_INACTIVE;   
                    sod[OPT_EXPOSURE_R].cap |= SANE_CAP_INACTIVE;   
                    sod[OPT_SHADOW_G].cap &= ~SANE_CAP_INACTIVE;           
                    sod[OPT_MIDTONE_G].cap &= ~SANE_CAP_INACTIVE;           
                    sod[OPT_HIGHLIGHT_G].cap &= ~SANE_CAP_INACTIVE;   
                    sod[OPT_EXPOSURE_G].cap &= ~SANE_CAP_INACTIVE;   
                    sod[OPT_SHADOW_B].cap |= SANE_CAP_INACTIVE;           
                    sod[OPT_MIDTONE_B].cap |= SANE_CAP_INACTIVE;           
                    sod[OPT_HIGHLIGHT_B].cap |= SANE_CAP_INACTIVE;   
                    sod[OPT_EXPOSURE_B].cap |= SANE_CAP_INACTIVE;   
                  }
                else if ( strcmp(val[option].s, MD_CHANNEL_BLUE) == 0 )
                  {
                    sod[OPT_SHADOW].cap |= SANE_CAP_INACTIVE;           
                    sod[OPT_MIDTONE].cap |= SANE_CAP_INACTIVE;           
                    sod[OPT_HIGHLIGHT].cap |= SANE_CAP_INACTIVE;   
                    sod[OPT_EXPOSURE].cap |= SANE_CAP_INACTIVE;   
                    sod[OPT_SHADOW_R].cap |= SANE_CAP_INACTIVE;           
                    sod[OPT_MIDTONE_R].cap |= SANE_CAP_INACTIVE;           
                    sod[OPT_HIGHLIGHT_R].cap |= SANE_CAP_INACTIVE;   
                    sod[OPT_EXPOSURE_R].cap |= SANE_CAP_INACTIVE;   
                    sod[OPT_SHADOW_G].cap |= SANE_CAP_INACTIVE;           
                    sod[OPT_MIDTONE_G].cap |= SANE_CAP_INACTIVE;           
                    sod[OPT_HIGHLIGHT_G].cap |= SANE_CAP_INACTIVE;   
                    sod[OPT_EXPOSURE_G].cap |= SANE_CAP_INACTIVE;   
                    sod[OPT_SHADOW_B].cap &= ~SANE_CAP_INACTIVE;           
                    sod[OPT_MIDTONE_B].cap &= ~SANE_CAP_INACTIVE;           
                    sod[OPT_HIGHLIGHT_B].cap &= ~SANE_CAP_INACTIVE;   
                    sod[OPT_EXPOSURE_B].cap &= ~SANE_CAP_INACTIVE;   
                  }
                return SANE_STATUS_GOOD;
              
              case OPT_GAMMA_MODE:
                restore_gamma_options(sod, val);
                if ( info )
                    *info |= SANE_INFO_RELOAD_OPTIONS;
                return SANE_STATUS_GOOD;

              case OPT_GAMMA_BIND:
                restore_gamma_options(sod, val);
                if ( info )
                    *info |= SANE_INFO_RELOAD_OPTIONS;

                return SANE_STATUS_GOOD;

              case OPT_SHADOW:
              case OPT_SHADOW_R:
              case OPT_SHADOW_G:
              case OPT_SHADOW_B:
                if ( val[option].w >= val[option + 1].w )
                  {
                    val[option + 1].w = val[option].w + 1;
                    if ( info )
                        *info |= SANE_INFO_RELOAD_OPTIONS;
                  }
                if ( val[option + 1].w >= val[option + 2].w ) 
                    val[option + 2].w = val[option + 1].w + 1;

                return SANE_STATUS_GOOD;
        
              case OPT_MIDTONE:
              case OPT_MIDTONE_R:
              case OPT_MIDTONE_G:
              case OPT_MIDTONE_B:
                if ( val[option].w <= val[option - 1].w )
                  {
                    val[option - 1].w = val[option].w - 1;
                    if ( info )
                        *info |= SANE_INFO_RELOAD_OPTIONS;
                  }
                if ( val[option].w >= val[option + 1].w ) 
                  {
                    val[option + 1].w = val[option].w + 1;
                    if ( info )
                        *info |= SANE_INFO_RELOAD_OPTIONS;
                  }

                return SANE_STATUS_GOOD;
        
              case OPT_HIGHLIGHT:
              case OPT_HIGHLIGHT_R:
              case OPT_HIGHLIGHT_G:
              case OPT_HIGHLIGHT_B:
                if ( val[option].w <= val[option - 1].w )
                  {
                    val[option - 1].w = val[option].w - 1;
                    if ( info )
                        *info |= SANE_INFO_RELOAD_OPTIONS;
                  }
                if ( val[option - 1].w <= val[option - 2].w ) 
                    val[option - 2].w = val[option - 1].w - 1;

                return SANE_STATUS_GOOD;

              case OPT_RESOLUTION_BIND:
                if ( ms->val[option].w == SANE_FALSE )
                  {
                    ms->sod[OPT_RESOLUTION].cap |= SANE_CAP_INACTIVE;
                    ms->sod[OPT_X_RESOLUTION].cap &= ~SANE_CAP_INACTIVE;
                    ms->sod[OPT_Y_RESOLUTION].cap &= ~SANE_CAP_INACTIVE;
                  }
                else
                  {
                    ms->sod[OPT_RESOLUTION].cap &= ~SANE_CAP_INACTIVE;
                    ms->sod[OPT_X_RESOLUTION].cap |= SANE_CAP_INACTIVE;
                    ms->sod[OPT_Y_RESOLUTION].cap |= SANE_CAP_INACTIVE;
                  }
                if ( info )
                    *info |= SANE_INFO_RELOAD_OPTIONS;
                return SANE_STATUS_GOOD;
                    
              default:
                return SANE_STATUS_UNSUPPORTED;
            } 
          break; 
        default:
          DBG(1, "sane_control_option: Unsupported action %d\n", action);
          return SANE_STATUS_UNSUPPORTED;
      }
}


/*---------- sane_exit() -----------------------------------------------------*/

void
sane_exit (void)
{
    Microtek2_Device *next;
    int i;

    DBG(30, "sane_exit:\n");

    /* close all leftover Scanners */
    while (ms_first_handle != NULL)
        sane_close(ms_first_handle);
    /* free up device list */
    while (md_first_dev != NULL) 
      {
        next = md_first_dev->next;

        for ( i = 0; i < 4; i++ )
          {
            if ( md_first_dev->custom_gamma_table[i] )
              {
                free((void *) md_first_dev->custom_gamma_table[i]);
                md_first_dev->custom_gamma_table[i] = NULL;
              }
          }

        free((void *) md_first_dev);
        md_first_dev = next;
      }
    sane_get_devices(NULL, SANE_FALSE);     /* free list of SANE_Devices */

    DBG(30, "sane_exit: MICROTEK2 says goodbye.\n");
}


/*---------- sane_get_devices()-----------------------------------------------*/

SANE_Status
sane_get_devices(const SANE_Device ***device_list, SANE_Bool local_only)
{
    /* return a list of available devices; available here means that we get */
    /* a positive response to an 'INQUIRY' and possibly to a */
    /* 'READ SCANNER ATTRIBUTE' call */
    
    static const SANE_Device **sd_list = NULL;
    Microtek2_Device *md;
    SANE_Status status;
    int index;

    DBG(30, "sane_get_devices: local_only=%d\n", local_only);

    /* this is hack to get the list freed with a call from sane_exit() */ 
    if ( device_list == NULL ) 
      {   
        if ( sd_list )
          {
            free(sd_list);
            sd_list=NULL;
          }
        DBG(30, "sane_get_devices: sd_list_freed\n"); 
        return SANE_STATUS_GOOD;                      
      }

    /* first free old list, if there is one; frontend wants a new list */
    if ( sd_list ) 
        free(sd_list);                            /* free array of pointers */ 

    sd_list = (const SANE_Device **) 
               malloc( (md_num_devices + 1) * sizeof(SANE_Device **));

    if ( ! sd_list ) 
      {
        DBG(1, "sane_get_devices: malloc() for sd_list failed\n");
        return SANE_STATUS_NO_MEM;
      } 

    *device_list = sd_list; 
    index = 0;    
    md = md_first_dev;
    while ( md ) 
      {
	status = attach(md);
        if ( status != SANE_STATUS_GOOD ) 
          {
            DBG(10, "sane_get_devices: attach status '%s'\n", 
                     sane_strstatus(status));
            md = md->next;
            continue;
          }
       
        /* check whether unit is ready, if so add it to the list */
        status = scsi_test_unit_ready(md);
        if ( status != SANE_STATUS_GOOD ) 
          {
            DBG(10, "sane_get_devices: test_unit_ready status '%s'\n",
                     sane_strstatus(status));
            md = md->next;                         
            continue;
          }

        sd_list[index] = &md->sane;
        
        ++index;
        md = md->next;                         
      }

    sd_list[index] = NULL;
    DBG(2, "sane_get_devices: returning\n");
    return SANE_STATUS_GOOD;
}


/*---------- sane_get_option_descriptor() ------------------------------------*/

const SANE_Option_Descriptor *
sane_get_option_descriptor(SANE_Handle handle, SANE_Int n)
{
    Microtek2_Scanner *ms = handle;

    DBG(30, "sane_get_option_descriptor: handle=%p, opt=%d\n", handle, n);
             
    if ( n < 0 || n > NUM_OPTIONS ) 
      {
        DBG(30, "sane_get_option_descriptor: invalid option %d\n", n);
        return NULL;
      }

    return &ms->sod[n];
}


/*---------- sane_get_parameters() -------------------------------------------*/

SANE_Status
sane_get_parameters(SANE_Handle handle, SANE_Parameters *params)
{
    Microtek2_Scanner *ms = handle;
    Microtek2_Device *md;
    Microtek2_Option_Value *val;
    Microtek2_Info *mi;
    int mode;  
    int depth;  
    int bits_pp_in;             /* bits per pixel from scanner */
    int bits_pp_out;            /* bits_per_pixel transferred to frontend */
    int bytes_per_line;
    double x_pixel_per_mm;
    double y_pixel_per_mm;
    double x1_pixel;
    double y1_pixel;
    double width_pixel;
    double height_pixel;


    DBG(40, "sane_get_parameters: handle=%p, params=%p\n", handle, params);
             

    md = ms->dev;
    mi = &md->info[md->scan_source];
    val= ms->val;

    if ( ! ms->scanning )            /* get an estimate for the params */
      {

        get_scan_mode_and_depth(ms, &mode, &depth, &bits_pp_in, &bits_pp_out);
   
        switch ( mode ) 
          {
	    case MS_MODE_COLOR:
              if ( mi->onepass ) 
                {
                  ms->params.format = SANE_FRAME_RGB;
                  ms->params.last_frame = SANE_TRUE;
                } 
              else 
                {   
                  ms->params.format = SANE_FRAME_RED;
                  ms->params.last_frame = SANE_FALSE;
                }
              break;
	    case MS_MODE_GRAY:
	    case MS_MODE_HALFTONE:
	    case MS_MODE_LINEART:
              ms->params.format = SANE_FRAME_GRAY;
              ms->params.last_frame = SANE_TRUE;
              break;
            default:
              DBG(1, "sane_get_parameters: Unknown scan mode %d\n", mode);
              break;
          } 

      ms->params.depth = (SANE_Int) bits_pp_out;

      /* calculate lines, pixels per line and bytes per line */
      if ( val[OPT_RESOLUTION_BIND].w == SANE_TRUE )
        {
          x_pixel_per_mm = y_pixel_per_mm =
                SANE_UNFIX(val[OPT_RESOLUTION].w) / MM_PER_INCH;
          DBG(30, "sane_get_parameters: x_res=y_res=%f\n", 
                  SANE_UNFIX(val[OPT_RESOLUTION].w));
        }
      else
        {
          x_pixel_per_mm = SANE_UNFIX(val[OPT_X_RESOLUTION].w) / MM_PER_INCH;
          y_pixel_per_mm = SANE_UNFIX(val[OPT_Y_RESOLUTION].w) / MM_PER_INCH;
          DBG(30, "sane_get_parameters: x_res=%f, y_res=%f\n", 
                  SANE_UNFIX(val[OPT_X_RESOLUTION].w), 
                  SANE_UNFIX(val[OPT_Y_RESOLUTION].w));
        }
      
      DBG(30, "sane_get_parameters: x_ppm=%f, y_ppm=%f\n",
               x_pixel_per_mm, y_pixel_per_mm); 
       
      y1_pixel = SANE_UNFIX(ms->val[OPT_TL_Y].w) * y_pixel_per_mm;
      height_pixel = fabs(SANE_UNFIX(ms->val[OPT_BR_Y].w) * y_pixel_per_mm 
                          - y1_pixel) + 0.5;
      ms->params.lines = (SANE_Int) height_pixel;

      x1_pixel =  SANE_UNFIX(ms->val[OPT_TL_X].w) * x_pixel_per_mm;
      width_pixel = fabs(SANE_UNFIX(ms->val[OPT_BR_X].w) * x_pixel_per_mm 
                         - x1_pixel) + 0.5;
      ms->params.pixels_per_line = (SANE_Int) width_pixel;
                                     
 
      if ( bits_pp_out == 1 )
          bytes_per_line =  (width_pixel + 7 ) / 8;
      else 
        {
          bytes_per_line = ( width_pixel * bits_pp_out ) / 8 ;
          if ( mode == MS_MODE_COLOR && mi->onepass )
	      bytes_per_line *= 3;
        }
      ms->params.bytes_per_line = (SANE_Int) bytes_per_line;
    }  /* if ms->scanning */
  
  if ( params )
     *params = ms->params;     
   
  DBG(30,"sane_get_parameters: format=%d, last_frame=%d, lines=%d\n",
        ms->params.format,ms->params.last_frame, ms->params.lines);
  DBG(30,"sane_get_parameters: depth=%d, ppl=%d, bpl=%d\n",
        ms->params.depth,ms->params.pixels_per_line, ms->params.bytes_per_line);

  return SANE_STATUS_GOOD;
}


/*---------- sane_get_select_fd() --------------------------------------------*/

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int *fd)
{
    Microtek2_Scanner *ms = handle;


    DBG(10, "sane_get_select_fd: ms=%p\n", ms);

    if ( ! ms->scanning )
      {
        DBG(1, "sane_get_select_fd: Scanner not scanning\n");
        return SANE_STATUS_INVAL;
      }

    *fd = (SANE_Int) ms->fd[0];
    return SANE_STATUS_GOOD;
}


/*---------- sane_init() -----------------------------------------------------*/

SANE_Status
sane_init(SANE_Int *version_code, SANE_Auth_Callback authorize)
{
    FILE *fp;
    char dev_name[PATH_MAX];
    Microtek2_Device *md;


    DBG_INIT();
    DBG(1, "sane_init: Microtek2 (v%d.%d) says hello...\n", 
           MICROTEK2_MAJOR, MICROTEK2_MINOR);

    if ( version_code )
        *version_code = SANE_VERSION_CODE(V_MAJOR, V_MINOR, 0);

    fp = sanei_config_open(MICROTEK2_CONFIG_FILE);
    if ( fp == NULL )
        DBG(10, "sane_init: file not opened: '%s'\n", MICROTEK2_CONFIG_FILE);
    else
      { 
        while ( fgets(dev_name, sizeof(dev_name), fp) ) 
            sanei_config_attach_matching_devices(dev_name, attach_one);
          
        fclose(fp);
      }

    if ( md_first_dev == NULL )
      {
        /* config file not found or no valid entry; default to /dev/scanner */
        /* instead of insisting in config file */
	add_device_list("/dev/scanner", &md);
        if ( md )
	    attach(md);
      }
    return(SANE_STATUS_GOOD);
}


/*---------- sane_open() -----------------------------------------------------*/

SANE_Status
sane_open(SANE_String_Const name, SANE_Handle *handle)
{
    Microtek2_Scanner *ms;
    Microtek2_Device *md;
    Microtek2_Info *mi;
    SANE_Status status;
   
 
    DBG(30, "sane_open: device='%s'\n", name);

    *handle = NULL;
    md = md_first_dev;
    
    if ( name ) 
      {
	/* add_device_list() returns a pointer to the device struct if */
        /* the device is known or newly added, else it returns NULL */
        
	status = add_device_list(name, &md);
        if ( status != SANE_STATUS_GOOD ) 
            return status;  
      }

    if ( ! md ) 
      {
        DBG(10, "sane_open: invalid device name '%s'\n", name);
        return SANE_STATUS_INVAL;  
      }
  
    /* attach calls INQUIRY and READ SCANNER ATTRIBUTES */
    status = attach(md);
    if ( status != SANE_STATUS_GOOD ) 
        return status;  
    
    ms = malloc(sizeof(Microtek2_Scanner));
    if ( ms == NULL ) 
      {
        DBG(1, "sane_open: malloc() for ms failed\n");
        return SANE_STATUS_NO_MEM;  
      }
    
    memset(ms, 0, sizeof(Microtek2_Scanner));
    ms->dev = md;
    mi = &md->info[MD_SOURCE_FLATBED];
    ms->scanning = SANE_FALSE;
    ms->cancelled = SANE_FALSE;
    ms->current_pass = 0;
    ms->sfd = -1;
    ms->pid = -1;
    ms->fp = NULL;
    ms->gamma_table = NULL;
    ms->buf.src_buf = ms->buf.src_buffer[0] = ms->buf.src_buffer[1] = NULL;
                    
    init_options(ms, MD_SOURCE_FLATBED);
    
    /* insert scanner into linked list */
    ms->next = ms_first_handle;
    ms_first_handle = ms;
 
    *handle = ms;

    return SANE_STATUS_GOOD;
}


/*---------- sane_read() -----------------------------------------------------*/

SANE_Status
sane_read(SANE_Handle handle, SANE_Byte *buf, SANE_Int maxlen, SANE_Int *len )
{
    Microtek2_Scanner *ms = handle;
    SANE_Status status;
    ssize_t nread;


    DBG(30, "sane_read: handle=%p, buf=%p, maxlen=%d\n", 
             handle, buf, maxlen);

    *len = 0;

    if ( ! ms->scanning || ms->cancelled ) 
      {
        if ( ms->cancelled )
          {
            cancel_scan(ms);
            status = SANE_STATUS_CANCELLED;
          }
        else
          {  
            DBG(15, "sane_read: Scanner %p not scanning\n", ms);
            status = SANE_STATUS_IO_ERROR;
          }
        cleanup_scanner(ms);
        return status;
      }


    nread = read(ms->fd[0], (void *) buf, (int) maxlen);
    if ( nread == -1 )
      {
        if ( errno == EAGAIN )
            return SANE_STATUS_GOOD;
        else
          {
            DBG(1, "sane_read: read() failed, errno=%d\n", errno);
            cleanup_scanner(ms);
            return SANE_STATUS_IO_ERROR;
          }
      }

    if ( nread == 0 )
      {
         cleanup_scanner(ms);
         return SANE_STATUS_EOF;
      }

    *len = (SANE_Int) nread;
    DBG(30, "sane_read: *len=%d\n", *len);
    return SANE_STATUS_GOOD;
}


/*---------- sane_set_io_mode() ---------------------------------------------*/

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
    Microtek2_Scanner *ms = handle;
    int rc;


    DBG(30, "sane_set_io_mode: handle=%p, nonblocking=%d\n",
             handle, non_blocking);

    if ( ! ms->scanning )
      {
        DBG(1, "sane_set_io_mode: Scanner not scanning\n");
        return SANE_STATUS_INVAL;
      }

    rc = fcntl(ms->fd[0], F_SETFL, non_blocking ? O_NONBLOCK : 0);
    if ( rc == -1 )
      {
        DBG(1, "sane_set_io_mode: fcntl() failed\n");
        return SANE_STATUS_INVAL;
      }

  return SANE_STATUS_GOOD;
}



/*---------- sane_start() ----------------------------------------------------*/

SANE_Status
sane_start(SANE_Handle handle)
{
    Microtek2_Scanner *ms = handle;
    Microtek2_Device *md;
    Microtek2_Info *mi;
    SANE_Status status;
    u_int8_t *pos;
    int color;
    int strip_lines;
    int rc;
    int i;

    DBG(30, "sane_start: handle=0x%p\n", handle);
   
    md = ms->dev;
    mi = &md->info[md->scan_source];

    if (ms->sfd < 0) {      /* first or only pass of this scan */

        status = get_scan_parameters(ms);
        if ( status != SANE_STATUS_GOOD )
            goto cleanup;

        /* open device */
        status = sanei_scsi_open (md->sane.name, &ms->sfd, 
                                  scsi_sense_handler, 0);
        if (status != SANE_STATUS_GOOD) 
          {
            DBG(1, "sane_start: scsi_open: '%s'\n", sane_strstatus(status)); 
            goto cleanup;
          }

        status = scsi_read_system_status(md, ms->sfd);
        if ( status != SANE_STATUS_GOOD )
            goto cleanup;

        /* toggle the lamp */
        if ( ms->scan_source == MS_SOURCE_FLATBED
             || ms->scan_source == MS_SOURCE_ADF )
          {
            md->status.flamp |= MD_FLAMP_ON;
            md->status.tlamp &= ~MD_TLAMP_ON;
          }
        else 
          {
            md->status.flamp &= ~MD_FLAMP_ON;
            md->status.tlamp |= MD_TLAMP_ON;
          }

        /* enable/disable backtracking */
        if ( ms->no_backtracking )
            md->status.ntrack |= MD_NTRACK_ON;
        else
            md->status.ntrack &= ~MD_NTRACK_ON;
         
        status = scsi_send_system_status(md, ms->sfd);
        if ( status != SANE_STATUS_GOOD )
            goto cleanup;
   
        /* test, bernd */
        /* status = do_dummy_scan(ms);
        if ( status != SANE_STATUS_GOOD )
            goto cleanup; */

        /* calculate gamma: we assume, that the gamma values are transferred */
        /* with one send gamma command, even if it is a 3 pass scanner */
        get_lut_size(mi, &ms->lut_size, &ms->lut_entry_size);
        ms->lut_size_bytes = ms->lut_size * ms->lut_entry_size;
        if ( ms->lut_size > 0 )
          {
            ms->word = (ms->lut_entry_size == 2);
            ms->gamma_table = (u_int8_t *) malloc(3 * ms->lut_size_bytes );
            if ( ms->gamma_table == NULL )
              {
                DBG(1, "sane_start: malloc for gammatable failed\n");
                status = SANE_STATUS_NO_MEM;
                goto cleanup;
              }
            ms->current_color = MS_COLOR_ALL;
            for ( color = 0; color < 3; color++ )
              {
                pos = ms->gamma_table + color * ms->lut_size_bytes;
                calculate_gamma(ms, pos, color);
              }
            /* scsi_send_gamma() needs this */
            ms->lut_size_bytes *= 3;

            status = scsi_send_gamma(ms);
            if ( status != SANE_STATUS_GOOD )
                goto cleanup;
          }

        status = scsi_set_window(ms, 1);
        if ( status != SANE_STATUS_GOOD )
            goto cleanup;
       
        ms->scanning = SANE_TRUE;
        ms->cancelled = SANE_FALSE;
      }

    ++ms->current_pass; 

    status = scsi_read_image_info(ms);
    if ( status != SANE_STATUS_GOOD )
        goto cleanup;

    /* calculate maximum number of lines to read */
    strip_lines = (int) ((double) ms->y_resolution_dpi * md_strip_height);
    if ( strip_lines == 0 )
        strip_lines = 1;

    /* calculate number of lines that fit into the source buffer */
    ms->src_max_lines = MIN(sanei_scsi_max_request_size / ms->bpl, strip_lines);
    if ( ms->src_max_lines == 0 )
      {
        DBG(1, "sane_start: Scan buffer too small\n");
        status = SANE_STATUS_IO_ERROR;
        goto cleanup;
      }

    /* allocate buffers */
    ms->src_buffer_size = ms->src_max_lines * ms->bpl;       

    if ( ms->mode == MS_MODE_COLOR && mi->data_format == MI_DATAFMT_LPLSEGREG ) 
      {             
        /* In this case the data is not neccessarily in the order RGB */
        /* and there may be different numbers of read red, green and blue */
        /* segments. We allocate a second buffer to read new lines in */
        /* and hold undelivered pixels in the other buffer */
        int extra_buf_size;

        extra_buf_size = 2 * ms->bpl * mi->ccd_gap 
                         * (int) ceil( (double) mi->max_yresolution
                                      / (double) mi->opt_resolution); 
        for ( i = 0; i < 2; i++ )
          {
            if ( ms->buf.src_buffer[i] )
                free((void *) ms->buf.src_buffer[i]);
            ms->buf.src_buffer[i] = (u_int8_t *) malloc(ms->src_buffer_size
                                    + extra_buf_size);
            if ( ms->buf.src_buffer[i] == NULL )
              {
                DBG(1, "sane_start: malloc for scan buffer failed\n");
                status = SANE_STATUS_NO_MEM;
                goto cleanup;
              }
          }
        ms->buf.free_lines = ms->src_max_lines + extra_buf_size / ms->bpl;
        ms->buf.free_max_lines = ms->buf.free_lines;
        ms->buf.src_buf = ms->buf.src_buffer[0];
        ms->buf.current_src = 0;         /* index to current buffer */
      }
    else
      {
        if ( ms->buf.src_buf )
            free((void *) ms->buf.src_buf);
        ms->buf.src_buf = malloc(ms->src_buffer_size);
        if ( ms->buf.src_buf == NULL )
          {
            DBG(1, "sane_start: malloc for scan buffer failed\n");
            status = SANE_STATUS_NO_MEM;
            goto cleanup;
          }
      }

    for ( i = 0; i < 3; i++ )
      {
        ms->buf.current_pos[i] = ms->buf.src_buffer[0];
        ms->buf.planes[0][i] = 0;
        ms->buf.planes[1][i] = 0;
      }

    /* some data formats have additional information in a scan line, which */
    /* is not transferred to the frontend; real_bpl is the number of bytes */
    /* per line, that is copied into the frontend's buffer */
    ms->real_bpl = (u_int32_t) ceil( ((double) ms->ppl * 
                                      (double) ms->bits_per_pixel_out) / 8.0 );
    if ( mi->onepass && ms->mode == MS_MODE_COLOR )
        ms->real_bpl *= 3;

    ms->real_remaining_bytes = ms->real_bpl * ms->src_remaining_lines;


    /* Apparently the V300 doesn't know "read image status" at all */
    if ( mi->model_code != 0x85 )
      {
        status = scsi_wait_for_image(ms);
        if ( status  != SANE_STATUS_GOOD )
            goto cleanup;
      }

    /* calculate sane parameters */
    if ( ! mi->onepass && ms->mode == MS_MODE_COLOR )
      {
        if ( ms->current_pass == 1 )
            ms->params.format = SANE_FRAME_RED;
        else if ( ms->current_pass == 2 )
            ms->params.format = SANE_FRAME_GREEN;
        else if ( ms->current_pass == 3 )
            ms->params.format = SANE_FRAME_BLUE;
        else
          {
            DBG(1, "sane_start: invalid pass number %d\n", ms->current_pass);
            status = SANE_STATUS_IO_ERROR;
            goto cleanup;
          }
      }
    else if ( mi->onepass && ms->mode == MS_MODE_COLOR )
        ms->params.format = SANE_FRAME_RGB;
    else
        ms->params.format = SANE_FRAME_GRAY;

    if ( ! mi->onepass && ms->mode == MS_MODE_COLOR && ms->current_pass < 3 )
        ms->params.last_frame = SANE_FALSE;
    else
        ms->params.last_frame = SANE_TRUE;
    ms->params.lines = ms->src_remaining_lines;
    ms->params.pixels_per_line = ms->ppl;
    ms->params.bytes_per_line = ms->real_bpl;
    ms->params.depth = ms->bits_per_pixel_out;

    /* open a pipe and fork a child process, that actually reads the data */
    rc = pipe(ms->fd);
    if ( rc == -1 )
      {
        DBG(1, "sane_start: pipe failed\n");
        status = SANE_STATUS_IO_ERROR;
        goto cleanup;
      }

    ms->pid = fork();
    if ( ms->pid == -1 )
      {
        DBG(1, "sane_start: fork failed\n");
        status = SANE_STATUS_IO_ERROR;
        goto cleanup;
      }
    else if ( ms->pid == 0 )           /* child process */
        _exit(reader_process(ms));

    close(ms->fd[1]);
    return SANE_STATUS_GOOD;

cleanup:
    cleanup_scanner(ms);
    return status; 
}


/*---------- add_device_list() -----------------------------------------------*/

static SANE_Status
add_device_list(SANE_String_Const dev_name, Microtek2_Device **mdev)
{
    Microtek2_Device *md;
    SANE_String hdev;
    size_t len;
    char *cp;


    if ( (hdev = strdup(dev_name)) == NULL)
      { 
	DBG(5, "add_device_list: malloc() for hdev failed\n");
        return SANE_STATUS_NO_MEM;
      }

    len = strlen(hdev);
    if ( hdev[len - 1] == '\n' )
        hdev[--len] = '\0';

    DBG(30, "add_device_list: device='%s'\n", hdev);

    /* check if options are present */
    cp = (char *) sanei_config_skip_whitespace(hdev);
    if ( *cp == '#' || *cp == '\0' ) 
      {
	DBG(30, "add_device_list: Comment or empty line in %s\n",
                 MICROTEK2_CONFIG_FILE);
        *mdev = NULL;
        return SANE_STATUS_GOOD;       /* ignore empty lines and comments */
      }

    if ( strncmp(cp, "option", 6) == 0 && isspace(cp[6]) )
      {
        char *endptr;

        cp = (char *) sanei_config_skip_whitespace(cp + 6);
        if ( strncmp(cp, "dump", 4) == 0 && isspace(cp[4]) )
          {
            cp = (char *) sanei_config_skip_whitespace(cp + 4);
            if ( *cp )
              {
                ms_dump = (int) strtol(cp, &endptr, 10);
                if ( ms_dump > 4 || ms_dump < 0 )
                  {
                    ms_dump = 1;
                    DBG(30, "add_device_list: setting dump to %d\n", ms_dump);
                  }
                cp = (char *) sanei_config_skip_whitespace(endptr);
                if ( *cp )
                  {     
                    /* something behind the option value or value wrong */
                    ms_dump = 1;
                    DBG(30, "add_device_list: option value wrong: %s\n", hdev);
                  }
              }     
            else
              {
                DBG(30, "add_device_list: missing option value '%s'\n", hdev);
                /* reasonable fallback */
                ms_dump = 1;
              }
          }
        else if ( strncmp(cp, "strip-height", 12) == 0 && isspace(cp[12]) )
          {
            cp = (char *) sanei_config_skip_whitespace(cp + 12);
            if ( *cp )
              {
                md_strip_height = strtod(cp, &endptr);
                DBG(30, "add_device_list: setting strip_height to %f\n",
                         md_strip_height);
                cp = (char *) sanei_config_skip_whitespace(endptr);
                if ( *cp )
                  {     
                    /* something behind the option value or value wrong */
                    md_strip_height = 1.0;
                    DBG(30, "add_device_list: option value wrong: %f\n", 
                             md_strip_height);
                  }
              }
          }
        else if ( strncmp(cp, "no-backtrack-option", 19) == 0
                  && isspace(cp[19]) )
          {
            cp = (char *) sanei_config_skip_whitespace(cp + 19);
            if ( strncmp(cp, "on", 2) == 0 )
              { 
                cp = (char *) sanei_config_skip_whitespace(cp + 2);
                md_no_backtracking = "on";
              }
            else if ( strncmp(cp, "off", 3) == 0 )
              {
                cp = (char *) sanei_config_skip_whitespace(cp + 3);
                md_no_backtracking = "off";
              }
            else
                md_no_backtracking = "off";

            if ( *cp )
              {     
                /* something behind the option value or value wrong */
                md_no_backtracking = "off";
                DBG(30, "add_device_list: option value wrong: %s\n", cp);
              }
          } 
        else if ( strncmp(cp, "lightlid-35", 11) == 0
                  && isspace(cp[11]) )
          {
            cp = (char *) sanei_config_skip_whitespace(cp + 11);
            if ( strncmp(cp, "on", 2) == 0 )
              { 
                cp = (char *) sanei_config_skip_whitespace(cp + 2);
                md_lightlid35 = "on";
              }
            else if ( strncmp(cp, "off", 3) == 0 )
              {
                cp = (char *) sanei_config_skip_whitespace(cp + 3);
                md_lightlid35 = "off";
              }
            else
                md_lightlid35 = "off";

            if ( *cp )
              {     
                /* something behind the option value or value wrong */
                md_lightlid35 = "off";
                DBG(30, "add_device_list: option value wrong: %s\n", cp);
              }
          } 
        else
            DBG(30, "add_device_list: invalid option in '%s'\n", hdev);
        
        *mdev = NULL;
        return SANE_STATUS_GOOD;
      } 
        
        
    /* check, if device is already known */
    md = md_first_dev;
    while ( md ) 
      {
        if ( strcmp(hdev, md->name) == 0 ) 
          {
	    DBG(30, "add_device_list: device '%s' already in list\n", hdev);

            *mdev = md;
            return SANE_STATUS_GOOD;
          }
        md = md->next;
    }

    md = (Microtek2_Device *) malloc(sizeof(Microtek2_Device));
    if ( md == NULL ) 
      {
	DBG(1, "add_device_list: malloc() for md failed\n");
        return SANE_STATUS_NO_MEM;
      }
        
    /* initialize Device and add it at the beginning of the list */
    memset(md, 0, sizeof(Microtek2_Device));
    md->next = md_first_dev;
    md_first_dev = md;
    md->sane.name = NULL;
    md->sane.vendor = NULL;
    md->sane.model = NULL;
    md->sane.type = NULL;
    md->scan_source = MD_SOURCE_FLATBED;
    strncpy(md->name, hdev, PATH_MAX - 1);
    ++md_num_devices;
    *mdev = md;
    free(hdev);

    return SANE_STATUS_GOOD;
}


/*---------- attach() --------------------------------------------------------*/

static SANE_Status
attach(Microtek2_Device *md)
{
    /* This function is called from sane_init() to do the inquiry and to read */
    /* the scanner attributes. If one of these calls fails, or if a new */
    /* device is passed in sane_open() this function may also be called */
    /* from sane_open() or sane_get_devices(). */

    SANE_String model_string; 
    int status;

    
    DBG(30, "attach: device='%s'\n", md->name);

    status = scsi_inquiry(&(md->info[MD_SOURCE_FLATBED]), md->name);
    if ( status != SANE_STATUS_GOOD ) 
      {
	DBG(1, "attach: '%s'\n", sane_strstatus(status));
        return status;     
      }                     
      
    status = check_inquiry(&md->info[MD_SOURCE_FLATBED], &model_string);   
    if ( status != SANE_STATUS_GOOD )
        return status;

    md->sane.name = md->name;
    md->sane.vendor = "Microtek";
    md->sane.model = strdup(model_string); 
    if ( md->sane.model == NULL )
        DBG(1, "attach: strdup for model string failed\n");
    md->sane.type = "flatbed scanner";
    md->revision = strtod(md->info[MD_SOURCE_FLATBED].revision, NULL);

    status = scsi_read_attributes(&md->info[0], md->name, MD_SOURCE_FLATBED);
    if ( status != SANE_STATUS_GOOD ) 
      {
	DBG(1, "attach: '%s'\n", sane_strstatus(status));
        return status;
      }    

    /* check whether the device supports transparency media adapters */
    if ( md->info[MD_SOURCE_FLATBED].option_device & MI_OPTDEV_TMA )
      {
        status = scsi_read_attributes(&md->info[0], md->name, MD_SOURCE_TMA);
        if ( status != SANE_STATUS_GOOD )
            return status;
      }
    
    /* check whether the device supports an ADF */
    if ( md->info[MD_SOURCE_FLATBED].option_device & MI_OPTDEV_ADF )
      {
        status = scsi_read_attributes(&md->info[0], md->name, MD_SOURCE_ADF);
        if ( status != SANE_STATUS_GOOD )
            return status;
      }

    /* check whether the device supports STRIPES */
    if ( md->info[MD_SOURCE_FLATBED].option_device & MI_OPTDEV_STRIPE )
      {
        status = scsi_read_attributes(&md->info[0], md->name, MD_SOURCE_STRIPE);
        if ( status != SANE_STATUS_GOOD )
            return status;
      }

    /* check whether the device supports SLIDES */
    if ( md->info[MD_SOURCE_FLATBED].option_device & MI_OPTDEV_SLIDE )
      {
        status = scsi_read_attributes(&md->info[0], md->name, MD_SOURCE_SLIDE);
        if ( status != SANE_STATUS_GOOD )
            return status;
      }

    status = scsi_read_system_status(md, -1);
    if ( status != SANE_STATUS_GOOD )
        return status;

    return SANE_STATUS_GOOD;
}

                
/*---------- attach_one() ----------------------------------------------------*/

static SANE_Status
attach_one (const char *name)
{
    Microtek2_Device *md;
    Microtek2_Device *md_tmp;
 

    DBG(30, "attach_one: name='%s'\n", name); 

    md_tmp = md_first_dev;
    /* if add_device_list() adds an entry it does this at the beginning */
    /* of the list and thus changes md_first_dev */
    add_device_list(name, &md);
    if ( md_tmp != md_first_dev )
        attach(md);

    DBG(30, "attach_one: returning\n"); 
    return SANE_STATUS_GOOD;
}


/*---------- calculate_gamma() -----------------------------------------------*/

static SANE_Status
calculate_gamma(Microtek2_Scanner *ms, u_int8_t *pos, int color)
{
    Microtek2_Device *md;
    Microtek2_Info *mi;
    double exp;
    double mult;
    double steps;
    unsigned int val;
    int i;
    int factor;           /* take into account the differences between the */
                          /* possible values for the color and the number */
                          /* of bits the scanner works with internally. */
                          /* If depth == 1 handle this as if the maximum */
                          /* the maximum depth was chosen */
    

    DBG(30, "calculate_gamma: ms=%p, pos=%p, color=%d\n", ms, pos, color);

    md = ms->dev;
    mi = &md->info[md->scan_source];

    /* does this work everywhere ? */
    if ( mi->depth & MI_HASDEPTH_12 )
      {
        factor = ms->lut_size / 4096;
        mult = 4095.0;
      }
    else if ( mi->depth & MI_HASDEPTH_10 )
      {
        factor = ms->lut_size / 1024; 
        mult = 1023.0;
      }
    else
      {
        factor = ms->lut_size / 256; 
        mult = 255.0;
      }

    /* factor = ms->lut_size / (int) pow(2.0, (double) ms->depth); */
    /* mult = pow(2.0, (double) ms->depth) - 1.0; */  /* depending on output */

    steps = (double) (ms->lut_size - 1);      /* depending on input size */

    DBG(30, "calculate_gamma: factor=%d, mult =%f, steps=%f, mode=%s\n",
             factor, mult, steps, ms->val[OPT_GAMMA_MODE].s);

    if ( strcmp(ms->val[OPT_GAMMA_MODE].s, MD_GAMMAMODE_SCALAR) == 0 )
      {
        int option; 

        option = OPT_GAMMA_SCALAR;
        /* OPT_GAMMA_SCALAR_R follows OPT_GAMMA_SCALAR directly */
        if ( ms->val[OPT_GAMMA_BIND].w == SANE_TRUE )
            exp = 1.0 / SANE_UNFIX(ms->val[option].w);
        else
            exp = 1.0 / SANE_UNFIX(ms->val[option + color + 1].w);

        for ( i = 0; i < ms->lut_size; i++ )  
          {
            val = (unsigned int) (mult * pow((double) i / steps, exp) + .5);

            /* is the byte ordering correct ???? */
            if ( ms->lut_entry_size == 2 ) 
                *((u_int16_t *) pos + i) = (u_int16_t) val;
            else
                *((u_int8_t *) pos + i) = (u_int8_t) val;
          }
      }
    else if ( strcmp(ms->val[OPT_GAMMA_MODE].s, MD_GAMMAMODE_CUSTOM) == 0 )
      {
        int option;
        SANE_Int *src;

        option = OPT_GAMMA_CUSTOM;
        if ( ms->val[OPT_GAMMA_BIND].w == SANE_TRUE )
            src = ms->val[option].wa;
        else
            src = ms->val[option + color + 1].wa;

        for ( i = 0; i < ms->lut_size; i++ )
          {
            if ( ms->lut_entry_size == 2 )
                *((u_int16_t *) pos + i) = (u_int16_t) (src[i] / factor);
            else
                *((u_int8_t *) pos + i) = (u_int8_t) (src[i] / factor);
          }
      }
    else if ( strcmp(ms->val[OPT_GAMMA_MODE].s, MD_GAMMAMODE_LINEAR) == 0 )
      {
        for ( i = 0; i < ms->lut_size; i++ )
          {
            if ( ms->lut_entry_size == 2 )
                *((u_int16_t *) pos + i) = (u_int16_t) (i / factor);
            else
                *((u_int8_t *) pos + i) = (u_int8_t) (i / factor);
          }
      }

    return SANE_STATUS_GOOD;
}


/*---------- cancel_scan() ---------------------------------------------------*/

static SANE_Status
cancel_scan(Microtek2_Scanner *ms)
{
    SANE_Status status;


    DBG(30, "cancel_scan: ms=%p\n", ms);

    /* READ IMAGE with a transferlength of 0 aborts a scan */
    ms->transfer_length = 0;
    status = scsi_read_image(ms);
    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "cancel_scan: cancel failed: '%s'\n", sane_strstatus(status));
        status = SANE_STATUS_IO_ERROR;
      }
    else
        status = SANE_STATUS_CANCELLED;

    close(ms->fd[0]);
    kill(ms->pid, SIGTERM);
    waitpid(ms->pid, NULL, 0);

    return status;
}


/*---------- check_inquiry() -------------------------------------------------*/

static SANE_Status
check_inquiry(Microtek2_Info *mi, SANE_String *model_string)
{
    DBG(30, "check_inquiry: mi=%p\n", mi);
   
    if ( mi->scsi_version != MI_SCSI_II_VERSION ) 
      {
        DBG(1, "check_inquiry: Device is not a SCSI-II device, but 0x%02x\n",
                mi->scsi_version);
        return SANE_STATUS_IO_ERROR;
      }

    if ( mi->device_type != MI_DEVTYPE_SCANNER )
      {
        DBG(1, "check_inquiry: Device is not a scanner, but 0x%02x\n",
                mi->device_type);
        return SANE_STATUS_IO_ERROR;
      }

    if ( strncmp("MICROTEK", mi->vendor, INQ_VENDOR_L) != 0 
         && strncmp("        ", mi->vendor, INQ_VENDOR_L) != 0 
         && strncmp("AGFA    ", mi->vendor, INQ_VENDOR_L) != 0 )
      {
        DBG(1, "check_inquiry: Device is not a Microtek, but '%.*s'\n",
                INQ_VENDOR_L, mi->vendor);
        return SANE_STATUS_IO_ERROR;
      }
 
    switch (mi->model_code) 
      {
        case 0x85:
          *model_string = "ScanMaker V300";
          break;
        case 0x8a:
          *model_string = "ScanMaker 9600XL";
          break;
        case 0x8c:
          *model_string = "ScanMaker 630";
          break;
        case 0x8d:
          *model_string = "ScanMaker 330";
          break;
        case 0x91:
          *model_string = "ScanMaker X6 / Phantom 636";
          break;
        case 0x92:
          *model_string = "E3+ / Vobis HighScan";
          break;
        case 0x93:
          *model_string = "ScanMaker 330";
          break;
        case 0x97:
          *model_string = "ScanMaker 636";
          break;
        case 0x98:
          *model_string = "ScanMaker X6EL";
          break;
        case 0x9d:
          *model_string = "AGFA Duoscan T1200";
          break;
        case 0xa3:
          *model_string = "ScanMaker V6USL";
          break;
        default:
          DBG(1, "check_inquiry: Model 0x%02x not supported\n", mi->model_code);
          return SANE_STATUS_IO_ERROR;
      }

    return SANE_STATUS_GOOD; 
}


/*---------- chunky_copy_pixels() --------------------------------------------*/

static SANE_Status
chunky_copy_pixels(u_int8_t *from, u_int32_t pixels, int depth, FILE * fp)
{
    u_int32_t pixel;
    int color;

    DBG(30, "chunky_copy_pixels: from=%p, pixels=%d, fp=%p, depth=%d\n",
             from, pixels, fp, depth);

    if ( depth > 8 )
      {
        int scale;
        u_int16_t val16;

        scale = 16 - depth;
        for ( pixel = 0; pixel < pixels; pixel++ )
          {
            for ( color = 0; color < 3; color++ )
              {
                val16 = *(u_int16_t *) from;
                val16 = ( val16 << scale ) | ( val16 & (( 1 << scale ) - 1) );
                fwrite((void *) &val16, 2, 1, fp);
                from += 2;
              }
          }
      }
    else if ( depth == 8 )
        fwrite((void *) from, 3 * pixels, 1, fp);
    else
      {
        DBG(1, "chunky_copy_pixels: Unknown depth %d\n", depth);
        return SANE_STATUS_IO_ERROR;
      }

    return SANE_STATUS_GOOD;
}


/*---------- chunky_proc_data() ----------------------------------------------*/

static SANE_Status                
chunky_proc_data(Microtek2_Scanner *ms)
{
    Microtek2_Device *md;
    Microtek2_Info *mi;
    SANE_Int status;
    u_int32_t line;
    u_int8_t *from;
    int pad;
    int bpp;                    /* bytes per pixel */
    int bits_pp_in;             /* bits per pixel input */
    int bits_pp_out;            /* bits per pixel output */
    int bpl_ppl_diff;


    DBG(30, "chunky_proc_data: ms=%p\n", ms);
             
    md = ms->dev;
    mi = &md->info[md->scan_source];
    bits_pp_in = ms->bits_per_pixel_in;
    bits_pp_out = ms->bits_per_pixel_out;
    pad = (int) ceil( (double) (ms->ppl * bits_pp_in) / 8.0 ) % 2;
    bpp = bits_pp_out / 8;

    /* Some models have 3 * ppl + 6 bytes per line if the number of pixels */
    /* per line is even and 3 * ppl + 3 bytes per line if the number of */
    /* pixels per line is odd. According to the documentation it should be */
    /* bpl = 3*ppl (even number of pixels) or bpl=3*ppl+1 (odd number of */         /* pixels. Even worse: On different models it different at which */             /* position in a scanline the image data starts. bpl_ppl_diff tries */          /* to fix this */    

    if (   (mi->model_code == 0x91     /* X6 */
            && (md->revision == 1.10 || md->revision == 1.20))
        || (mi->model_code == 0x98
            && (md->revision == 1.10 || md->revision == 1.30 
                || md->revision == 1.40)) )
        bpl_ppl_diff = ms->bpl - ( 3 * ms->ppl * bpp );
    else
        bpl_ppl_diff = ms->bpl - ( 3 * ms->ppl * bpp ) - pad;

    from = ms->buf.src_buf;
    
    DBG(30, "chunky_proc_data: lines=%d, bpl=%d, ppl=%d, bpp=%d, depth=%d"
            " junk=%d\n", ms->src_lines_to_read, ms->bpl, ms->ppl,
             bpp, ms->depth, bpl_ppl_diff);
            
    for ( line = 0; line < ms->src_lines_to_read; line++ )
      {
        from += bpl_ppl_diff;
        status = chunky_copy_pixels(from, ms->ppl, ms->depth, ms->fp);
        if ( status != SANE_STATUS_GOOD )
            return status;
        from += ms->bpl - bpl_ppl_diff;
      }

    return SANE_STATUS_GOOD;
}


/*---------- cleanup_scanner() -----------------------------------------------*/

static void
cleanup_scanner(Microtek2_Scanner *ms)
{

    DBG(30, "cleanup_scanner: ms=%p\n", ms);

    if ( ms->sfd != -1 )
        sanei_scsi_close(ms->sfd);

    ms->sfd = -1;
    ms->pid = -1;
    ms->fp = NULL;
    ms->current_pass = 0;
    ms->scanning = SANE_FALSE;
    ms->cancelled = SANE_FALSE;

    /* free buffers */

    if ( ms->buf.src_buffer[0] )
      {
        free((void *) ms->buf.src_buffer[0]);
        ms->buf.src_buffer[0] = NULL;
        ms->buf.src_buf = NULL;
      }

    if ( ms->buf.src_buffer[1] )
      {
        free((void *) ms->buf.src_buffer[1]);
        ms->buf.src_buffer[1] = NULL;
        ms->buf.src_buf = NULL;
      }

    if ( ms->buf.src_buf )
      {
        free((void *) ms->buf.src_buf);
        ms->buf.src_buf = NULL;
      }

    if ( ms->gamma_table )
      {
        free((void *) ms->gamma_table);
        ms->gamma_table = NULL;
      }
}


/*---------- do_dummy_scan() ----------------------------------------------*/

/* test bernd */
/* static SANE_Status
do_calibration(Microtek2_Scanner *ms)
{
    Microtek2_Device *md;
    SANE_Status status;


    DBG(30, "do_calibration: ms=%p\n", ms);

    md = ms->dev;
    status = scsi_read_system_status(md, ms->sfd);
    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "do_calibration: read_system_status failed: %s\n",
               sane_strstatus(status));
        return status;
      }

    md->status.ncalib |= MD_NCALIB_ON;
    status = scsi_send_system_status(md, ms->sfd);
    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "do_calibration: send_system_status failed: %s\n",
               sane_strstatus(status));
        return status;
      }

    status = scsi_read_system_status(md, ms->sfd);
    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "do_calibration: read_system_status failed: %s\n",
               sane_strstatus(status));
        return status;
      }

    md->status.ncalib &= ~MD_NCALIB_ON;
    status = scsi_send_system_status(md, ms->sfd);
    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "do_calibration: send_system_status failed: %s\n",
               sane_strstatus(status));
        return status;
      }

    return SANE_STATUS_GOOD;

}
*/

#if 0
/*---------- do_dummy_scan() ----------------------------------------------*/

static SANE_Status
do_dummy_scan(Microtek2_Scanner *ms)
{
    /* This function looks somehow stupid, but some models */
    /* produce garbage after each successful color scan */
    /* until a B/W scan is performed. So we do a "dummy" B/W scan before */
    /* each scan. */
    Microtek2_Scanner *ms_dummy;
    Microtek2_Device *md_dummy;
    Microtek2_Device *md;
    Microtek2_Info *mi;
    SANE_Status status;


    md = ms->dev;
    mi = &md->info[md->scan_source];
   
    if ( ! ( md->revision >= 1.00 && md->revision <= 1.40 )
           &&  ! ( md->info[MD_SOURCE_FLATBED].model_code == 0x8c ) )
        return SANE_STATUS_GOOD;
 
    DBG(30, "do_dummy_scan: ms=%p\n", ms);

    ms_dummy = (Microtek2_Scanner *) malloc(sizeof(Microtek2_Scanner));
    if ( ms_dummy == NULL )
      {
        DBG(1, "do_dummy_scan: malloc failed\n");
        return SANE_STATUS_NO_MEM;
      }
    memset(ms_dummy, 0, sizeof(Microtek2_Scanner));

    md_dummy = (Microtek2_Device *) malloc(sizeof(Microtek2_Device));
    if ( md_dummy == NULL )
      {
        DBG(1, "do_dummy_scan: malloc failed\n");
        return SANE_STATUS_NO_MEM;
      }
    ms_dummy->sfd = ms->sfd;
    /* scsi_read_image_info() checks the model code, so we need */
    /* something like this; all this is ugly */
    ms_dummy->dev = md;

    status = scsi_read_system_status(md_dummy, ms_dummy->sfd);
    if ( status != SANE_STATUS_GOOD )
        return status;

    /* do an uncalibrated scan */
    md_dummy->status.ncalib = MD_NCALIB_ON;
    status = scsi_send_system_status(md_dummy, ms_dummy->sfd);
    if ( status != SANE_STATUS_GOOD )
        return status;

    /* set window for a dummy scan */
    ms_dummy->x_resolution_dpi = mi->opt_resolution;
    ms_dummy->y_resolution_dpi = mi->opt_resolution;
    ms_dummy->width_dots = mi->geo_width;
    ms_dummy->height_dots = 1;
    ms_dummy->threshold = (u_int8_t) M_THRESHOLD_DEFAULT;
    ms_dummy->fastscan = 1;
    ms_dummy->quality = 1;
    ms_dummy->scan_source = ms->scan_source;
    ms_dummy->brightness_m = ms_dummy->brightness_r = ms_dummy->brightness_g
            = ms_dummy->brightness_b = (u_int8_t) M_BRIGHTNESS_DEFAULT;
    ms_dummy->contrast_m = ms_dummy->contrast_r = ms_dummy->contrast_g
            = ms_dummy->contrast_b = (u_int8_t) M_CONTRAST_DEFAULT;
    ms_dummy->midtone_m = ms_dummy->midtone_r = ms_dummy->midtone_g
            = ms_dummy->midtone_b = (u_int8_t) M_MIDTONE_DEFAULT;
    ms_dummy->highlight_m = ms_dummy->highlight_r = ms_dummy->highlight_g
            = ms_dummy->highlight_b = (u_int8_t) M_HIGHLIGHT_DEFAULT;
   
    ms_dummy->current_color = MS_COLOR_ALL;
 
    if ( md->info[MD_SOURCE_FLATBED].model_code == 0x8c 
         || md->info[MD_SOURCE_FLATBED].model_code == 0x91)
      {
        ms_dummy->depth = 8;
        ms_dummy->mode = MS_MODE_GRAY;
      }
    else
      {
        ms_dummy->depth = 1;
        ms_dummy->mode = MS_MODE_LINEART;
      }

    status = scsi_set_window(ms_dummy, 1);
    if ( status != SANE_STATUS_GOOD )
        return status;

    status = scsi_read_image_info(ms_dummy);
    if ( status != SANE_STATUS_GOOD )
        return status;

    status = scsi_wait_for_image(ms_dummy);
    if ( status != SANE_STATUS_GOOD )
        return status;

    ms_dummy->buf.src_buf = (u_int8_t *) malloc(ms_dummy->remaining_bytes);
    if ( ms_dummy->buf.src_buf == NULL )
      {
        DBG(1, "do_dummy_scan: malloc failed\n");
        return SANE_STATUS_NO_MEM;
      }
    /* bernd test */
    /* ms_dummy->transfer_length = ms_dummy->remaining_bytes; */
    ms_dummy->transfer_length = 0;
    status = scsi_read_image(ms_dummy);
    if ( status != SANE_STATUS_GOOD )
        return status;

    md_dummy->status.ncalib = ~MD_NCALIB_ON;
    status = scsi_send_system_status(md_dummy, ms_dummy->sfd);
    if ( status != SANE_STATUS_GOOD )
        return status;

    free((void *) ms_dummy->buf.src_buf);
    free((void *) ms_dummy);
    free((void *) md_dummy);
    return SANE_STATUS_GOOD;
}
#endif

/*---------- segreg_copy_pixels() --------------------------------------------*/

static SANE_Status
segreg_copy_pixels(u_int8_t **from, u_int32_t pixels, int depth, FILE *fp)
{
    u_int32_t pixel;
    int color;

    
    DBG(30, "segreg_copy_pixels: pixels=%d\n", pixels);

    pixel = 0;
    while ( pixel < pixels )
      {
        int scale ;
        u_int16_t val16;

        if ( depth > 8 )
          {
            scale = 16 - depth;
            for ( color = 0; color < 3; color++ )
              {

                val16 = *(u_int16_t *) from[color];
                val16 = ( val16 << scale ) | ( val16 & (( 1 << scale ) - 1) );
                fwrite((void *) &val16, 2, 1, fp);
                from[color] += 2;
              }
          }
        else if ( depth == 8 )
          {
            for ( color = 0; color < 3; color++ )
              {
                fputc(*from[color], fp);
                from[color]++;
              }
          }
        else
          {
            DBG(1, "segreg_copy_pixels: illegal depth %d\n", depth);
            return SANE_STATUS_INVAL;
          }

        ++pixel;
      }

    return SANE_STATUS_GOOD;

}


/*---------- dump_area() -----------------------------------------------------*/

static SANE_Status
dump_area(u_int8_t *area, int len, char *info)
{
    /* this function dumps control or information blocks */
    
#define BPL    16               /* bytes per line to print */
   
    int i;
    int o;
    int o_limit;

    if ( ! info[0] )
        info = "No additional info available";    

    DBG(30, "dump_area: %s\n", info);
   
    o_limit = (len + BPL - 1) / BPL;
    for ( o = 0; o < o_limit; o++) {
        fprintf(stderr, "  %4d: ", o * BPL); 
        for ( i=0; i < BPL && (o * BPL + i ) < len; i++) {
            if ( i == BPL / 2 ) 
                fprintf(stderr, " ");
            fprintf(stderr, "%02x", area[o * BPL + i]); 
        }
        
        fprintf(stderr, "%*s",  2 * ( 2 + BPL - i), " " );
        fprintf(stderr, "%s",  (i == BPL / 2) ? " " : "");

        for ( i = 0; i < BPL && (o * BPL + i ) < len; i++) {
            if ( i == BPL / 2 )
                fprintf(stderr, " ");
            fprintf(stderr, "%c", isprint(area[o * BPL + i])
                                  ? area[o * BPL + i]
                                  : '.');
        }
        fprintf(stderr, "\n");
    }

    return SANE_STATUS_GOOD;
}


/*---------- dump_area2() ----------------------------------------------------*/

static SANE_Status
dump_area2(u_int8_t *area, int len, char *info)
{
    int i;


    if ( ! info[0] )
        info = "No additional info available";    
    
    fprintf(stderr, "[%s]\n", info);

    for ( i = 0; i < len; i++)
        fprintf(stderr, "%02x", *(area + i));
    
    fprintf(stderr, "\n");

    return SANE_STATUS_GOOD;
}


/*---------- dump_attributes() -----------------------------------------------*/

static SANE_Status 
dump_attributes(Microtek2_Info *mi)
{
  /* dump all we know about the scanner to stderr */

  int i;

  DBG(30, "dump_attributes: mi=%p\n", mi);

  fprintf(stderr, "\n\nScanner attributes from device structure\n");
  fprintf(stderr, "========================================\n");
  fprintf(stderr, "\nScanner ID...\n");
  fprintf(stderr, "~~~~~~~~~~~~~\n");
  fprintf(stderr, "  Vendor Name:        '%s'\n", mi->vendor);
  fprintf(stderr, "  Model Name:         '%s'\n", mi->model);
  fprintf(stderr, "  Revision:           '%s'\n", mi->revision);
	  
  fprintf(stderr, "  Model Code:         0x%02x (", mi->model_code);
  switch(mi->model_code) 
    {
      case 0x80: fprintf(stderr, "Redondo"); break;
      case 0x81: fprintf(stderr, "Aruba"); break;
      case 0x82: fprintf(stderr, "Bali"); break;
      case 0x83: fprintf(stderr, "Washington"); break;
      case 0x84: fprintf(stderr, "Manhattan"); break;
      case 0x85: fprintf(stderr, "TR3"); break;
      case 0x86: fprintf(stderr, "CCP"); break;
      case 0x87: fprintf(stderr, "Scanmaker V"); break;
      case 0x88: fprintf(stderr, "Scanmaker VI"); break;
      case 0x89: fprintf(stderr, "A3-400"); break;
      case 0x8a: fprintf(stderr, "MRS-1200A3 - ScanMaker 9600XL"); break;
      case 0x8b: fprintf(stderr, "Watt"); break;
      case 0x8c: fprintf(stderr, "TR6"); break;
      case 0x8d: fprintf(stderr, "Tr3 10-bit"); break;
      case 0x8e: fprintf(stderr, "CCB"); break;
      case 0x8f: fprintf(stderr, "Sun Rise"); break;
      case 0x90: fprintf(stderr, "ScanMaker E3 10-bit"); break;
      case 0x91: fprintf(stderr, "X6"); break;
      case 0x92: fprintf(stderr, "E3+ or Vobis Highscan"); break;
      case 0x93: fprintf(stderr, "ScanMaker 330"); break;
      case 0x97: fprintf(stderr, "ScanMaker 636"); break;
      case 0x98: fprintf(stderr, "ScanMaker X6EL"); break;
      case 0xa3: fprintf(stderr, "ScanMaker V6USL"); break;
      default:   fprintf(stderr, "Unknown"); break;
    }
  fprintf(stderr, ")\n");
  fprintf(stderr, "  Device Type Code:   0x%02x (%s),\n", 
          mi->device_type,
	  mi->device_type & MI_DEVTYPE_SCANNER ? "Scanner" : "Unknown type");
          
  fprintf(stderr, "  Scanner type:       "); 
  switch (mi->scanner_type) 
    {
      case MI_TYPE_FLATBED: fprintf(stderr, "Flatbed scanner\n");
          break;
      case MI_TYPE_TRANSPARENCY: fprintf(stderr, "Transparency scanner\n");
          break;
      case MI_TYPE_SHEEDFEED: fprintf(stderr, "Sheet feed scanner\n");
          break;
      default: fprintf(stderr, "Unknown\n");
          break;
    }
  
  fprintf(stderr, "  Supported options:  Automatic document feeder: %s\n",
	  mi->option_device & MI_OPTDEV_ADF ? "Yes" : "No");
  fprintf(stderr, "%22sTransparency media adapter: %s\n",
	  " ", mi->option_device & MI_OPTDEV_TMA ? "Yes" : "No");
  fprintf(stderr, "%22sAuto paper detecting: %s\n",
	  " ", mi->option_device & MI_OPTDEV_ADP ? "Yes" : "No");
  fprintf(stderr, "%22sAdvanced picture system: %s\n",
	  " ", mi->option_device & MI_OPTDEV_APS ? "Yes" : "No");
  fprintf(stderr, "%22sStripes: %s\n",
	  " ", mi->option_device & MI_OPTDEV_STRIPE ? "Yes" : "No");
  fprintf(stderr, "%22sSlides: %s\n",
	  " ", mi->option_device & MI_OPTDEV_SLIDE ? "Yes" : "No");
  fprintf(stderr, "  Scan button:        %s\n", mi->scnbuttn ? "Yes" : "No"); 
         
  
  fprintf(stderr, "\nImaging Capabilities...\n");
  fprintf(stderr, "~~~~~~~~~~~~~~~~~~~~~~~\n");
  fprintf(stderr, "  Color scanner:      %s\n", (mi->color) ? "Yes" : "No");
  fprintf(stderr, "  Number passes:      %d pass%s\n", 
          (mi->onepass) ? 1 : 3,
          (mi->onepass) ? "" : "es");
  fprintf(stderr, "  Resolution:         X-max: %5d dpi\n%22sY-max: %5d dpi\n",
          mi->max_xresolution, " ",mi->max_yresolution);
  fprintf(stderr, "  Geometry:           Geometric width: %5d pts (%2.2f'')\n", 
          mi->geo_width, (float) mi->geo_width / (float) mi->opt_resolution); 
  fprintf(stderr, "%22sGeometric height:%5d pts (%2.2f'')\n", " ",
          mi->geo_height, (float) mi->geo_height / (float) mi->opt_resolution);
  fprintf(stderr, "  Optical resol.  : %5d\n", mi->opt_resolution);

  fprintf(stderr, "  Modes:              Lineart:     %s\n%22sHalftone:     %s\n",
	  (mi->scanmode & MI_HASMODE_LINEART) ? " Yes" : " No", " ",
	  (mi->scanmode & MI_HASMODE_HALFTONE) ? "Yes" : "No");

  fprintf(stderr,"%22sGray:     %s\n%22sColor:     %s\n", " ",
	  (mi->scanmode & MI_HASMODE_GRAY) ? "    Yes" : "    No", " ",
	  (mi->scanmode & MI_HASMODE_COLOR) ? "   Yes" : "   No");

  fprintf(stderr, "  Depths:             Nibble Gray:  %s\n",
	  (mi->depth & MI_HASDEPTH_NIBBLE) ? "Yes" : "No");
  fprintf(stderr, "%22s10-bit-color: %s\n", " ",
	  (mi->depth & MI_HASDEPTH_10) ? "Yes" : "No");
  fprintf(stderr, "%22s12-bit-color: %s\n", " ",
	  (mi->depth & MI_HASDEPTH_12) ? "yes" : "No");
  fprintf(stderr, "  d/l of HT pattern:  %s\n",
          (mi->has_dnldptrn) ? "Yes" : "No");
  fprintf(stderr, "  Builtin HT patt.:   %d\n", mi->grain_slct);
  
  fprintf(stderr, "  LUT capabilities:   ");
  if ( MI_LUTCAP_NONE(mi->lut_cap) ) 
      fprintf(stderr, "None\n"); 
  if ( mi->lut_cap & MI_LUTCAP_256B )
      fprintf(stderr, " 256 bytes\n"); 
  if ( mi->lut_cap & MI_LUTCAP_1024B )
      fprintf(stderr, "1024 bytes\n"); 
  if ( mi->lut_cap & MI_LUTCAP_1024W )
      fprintf(stderr, "1024 words\n"); 
  if ( mi->lut_cap & MI_LUTCAP_4096B )
      fprintf(stderr, "4096 bytes\n"); 
  if ( mi->lut_cap & MI_LUTCAP_4096W )
      fprintf(stderr, "4096 words\n"); 
  
  fprintf(stderr, "\nMiscellaneous capabilities...\n");
  fprintf(stderr, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
  fprintf(stderr, "  Data format:        ");
  if ( mi->onepass) 
    {
      switch(mi->data_format) 
        {
	  case MI_DATAFMT_CHUNKY:
              fprintf(stderr, "Chunky data, R, G & B in one pixel\n");
              break;
	  case MI_DATAFMT_LPLCONCAT:
              fprintf(stderr, "Line by line in concatenated sequence,\n");
              fprintf(stderr, "%22swithout color indicator\n", " ");
              break;
	  case MI_DATAFMT_LPLSEGREG:
              fprintf(stderr, "Line by line in segregated sequence,\n");
              fprintf(stderr, "%22swith color indicator\n", " ");
              break;
	  case MI_DATAFMT_WORDCHUNKY:
              fprintf(stderr, "Word chunky data\n");
              break;
          default:
              fprintf(stderr, "Unknown\n");
          break;
        }
    } 
  else
      fprintf(stderr, "No information with 3-pass scanners\n");

  fprintf(stderr, "  Color Sequence:     ");
  for ( i = 0; i < RSA_COLORSEQUENCE_L; i++) 
    {
      switch(mi->color_sequence[i]) 
        {
	  case MI_COLSEQ_RED:   fprintf(stderr,"R"); break;
	  case MI_COLSEQ_GREEN: fprintf(stderr,"G"); break;
	  case MI_COLSEQ_BLUE:  fprintf(stderr,"B"); break;
        }
      if ( i == RSA_COLORSEQUENCE_L - 1)
          fprintf(stderr, "\n");
      else          
          fprintf(stderr, " - ");
    }
  fprintf(stderr, "  CCD gap:            %d lines\n", mi->ccd_gap);
  fprintf(stderr, "  CCD pixels:         %d\n", mi->ccd_pixels);
  fprintf(stderr, "  Calib wh str loc:   %d\n", mi->calib_white);
  fprintf(stderr, "  Max calib space:    %d\n", mi->calib_space);
  fprintf(stderr, "  Number of lens:     %d\n", mi->nlens);
  fprintf(stderr, "  Max no of windows:  %d\n", mi->nwindows);
  fprintf(stderr, "  Sh trnsf func equ:  %d\n", mi->shtrnsferequ);
  fprintf(stderr, "  Buffer type:        %s\n", 
          mi->buftype ? "Ping-Pong" : "Ring");
  fprintf(stderr, "  FEPROM:             %s\n", mi->feprom ? "Yes" : "No");

  ms_dump_clear = 0;
  return SANE_STATUS_GOOD;
}


/*---------- get_scan_mode_and_depth() ---------------------------------------*/

static SANE_Status
get_scan_mode_and_depth(Microtek2_Scanner *ms, 
                        int *mode, 
                        int *depth, 
                        int *bits_per_pixel_in,
                        int *bits_per_pixel_out)
{
    /* This function translates the strings for the possible modes and */
    /* bitdepth into a more conveniant format as needed for SET WINDOW. */
    /* bits_per_pixel is the number of bits per color one pixel needs */
    /* when transferred from the the scanner, bits_perpixel_out is the */
    /* number of bits per color one pixel uses when transferred to the */
    /* frontend. These may be different. For example, with a depth of 4 */
    /* two pixels per byte are transferred from the scanner, but only one */
    /* pixel per byte is transferred to the frontend. */
    
    DBG(30, "get_scan_mode_and_depth: handle=%p\n", ms);

    if ( strcmp(ms->val[OPT_MODE].s, MD_MODESTRING_COLOR36) == 0 )
      {
	*mode = MS_MODE_COLOR;
        *depth = 12;
        *bits_per_pixel_in = *bits_per_pixel_out = 16;
      }
    else if ( strcmp(ms->val[OPT_MODE].s, MD_MODESTRING_COLOR30) == 0 ) 
      {
	*mode = MS_MODE_COLOR;
        *depth = 10;
        *bits_per_pixel_in = *bits_per_pixel_out = 16;
      }
    else if ( strcmp(ms->val[OPT_MODE].s, MD_MODESTRING_COLOR24) == 0 ) 
      {
	*mode = MS_MODE_COLOR;
        *depth = 8;
        *bits_per_pixel_in = *bits_per_pixel_out = 8;
      }
    else if ( strcmp(ms->val[OPT_MODE].s, MD_MODESTRING_GRAY12) == 0 ) 
      {
	*mode = MS_MODE_GRAY;
        *depth = 12;
        *bits_per_pixel_in = *bits_per_pixel_out = 16;
      }
    else if ( strcmp(ms->val[OPT_MODE].s, MD_MODESTRING_GRAY10) == 0 ) 
      {
	*mode = MS_MODE_GRAY;
        *depth = 10;
        *bits_per_pixel_in = *bits_per_pixel_out = 16;
      }
    else if ( strcmp(ms->val[OPT_MODE].s, MD_MODESTRING_GRAY8) == 0 ) 
      {
	*mode = MS_MODE_GRAY;
        *depth = 8;
        *bits_per_pixel_in = *bits_per_pixel_out = 8;
      }
    else if ( strcmp(ms->val[OPT_MODE].s, MD_MODESTRING_HALFTONE) == 0) 
      {
	*mode = MS_MODE_HALFTONE;
        *depth = 1;
        *bits_per_pixel_in = *bits_per_pixel_out = 1;
      }
    else if ( strcmp(ms->val[OPT_MODE].s, MD_MODESTRING_LINEART) == 0 )
      {
	*mode = MS_MODE_LINEART;
        *depth = 1;
        *bits_per_pixel_in = *bits_per_pixel_out = 1;
      }
    else 
      {
        DBG(1, "get_scan_mode_and_depth: Unknown mode %s\n",
                ms->val[OPT_MODE].s);
        return SANE_STATUS_INVAL;
      }

    /* scan preview with no more than 8 bit */
    if ( ms->val[OPT_PREVIEW].w == SANE_TRUE )
      {
        if ( *depth > 8 )
          {
            *depth = 8;
            *bits_per_pixel_in = *bits_per_pixel_out = 8;
          }
      }

    DBG(30, "get_scan_mode_and_depth: mode=%d, depth=%d,"
            " bits_pp_in=%d, bits_pp_out=%d, preview=%d\n",
             *mode, *depth, *bits_per_pixel_in, *bits_per_pixel_out,
             ms->val[OPT_PREVIEW].w);
             
    return SANE_STATUS_GOOD;
}


/*---------- get_scan_parameters () ------------------------------------------*/

static SANE_Status
get_scan_parameters(Microtek2_Scanner *ms)
{
    Microtek2_Device *md;
    Microtek2_Info *mi;
    double dpm;                   /* dots per millimeter */
    int x2_dots;
    int y2_dots;
    int i;


    DBG(30, "get_scan_parameters: handle=%p\n", ms);

    md = ms->dev;
    mi = &md->info[md->scan_source];

    get_scan_mode_and_depth(ms, &ms->mode, &ms->depth,
                            &ms->bits_per_pixel_in, &ms->bits_per_pixel_out);
    
    /* get the scan_source */
    if ( strcmp(ms->val[OPT_SOURCE].s, MD_SOURCESTRING_FLATBED) == 0 )
        ms->scan_source = MS_SOURCE_FLATBED;
    else if ( strcmp(ms->val[OPT_SOURCE].s, MD_SOURCESTRING_ADF) == 0 )
        ms->scan_source = MS_SOURCE_ADF;
    else if ( strcmp(ms->val[OPT_SOURCE].s, MD_SOURCESTRING_TMA) == 0 )
        ms->scan_source = MS_SOURCE_TMA;
    else if ( strcmp(ms->val[OPT_SOURCE].s, MD_SOURCESTRING_STRIPE) == 0 )
        ms->scan_source = MS_SOURCE_STRIPE;
    else if ( strcmp(ms->val[OPT_SOURCE].s, MD_SOURCESTRING_SLIDE) == 0 )
        ms->scan_source = MS_SOURCE_SLIDE;

    /* enable/disable backtracking */
    if ( ms->val[OPT_DISABLE_BACKTRACK].w == SANE_TRUE )
        ms->no_backtracking = 1;
    else
        ms->no_backtracking = 0;

    /* if halftone mode select halftone pattern */
    if ( ms->mode == MS_MODE_HALFTONE )
      {
        i = 0;
        while ( strcmp(md->halftone_mode_list[i], ms->val[OPT_HALFTONE].s) ) 
            ++i;
        ms->internal_ht_index = i;
      }

    /* if lineart get the value for threshold */ 
    if ( ms->mode == MS_MODE_LINEART )   
        ms->threshold = (u_int8_t) ms->val[OPT_THRESHOLD].w;
    else
        ms->threshold = (u_int8_t) M_THRESHOLD_DEFAULT;

    DBG(30, "get_scan_parameters: mode=%d, depth=%d, bpp_in=%d, bpp_out=%d\n",
             ms->mode, ms->depth, ms->bits_per_pixel_in, 
             ms->bits_per_pixel_out);

    /* calculate positions, width and height in dots */
    dpm = (double) mi->opt_resolution / MM_PER_INCH;
    ms->x1_dots = (SANE_Int) ( SANE_UNFIX(ms->val[OPT_TL_X].w) * dpm + 0.5 );
    ms->y1_dots = (SANE_Int) ( SANE_UNFIX(ms->val[OPT_TL_Y].w) * dpm + 0.5 );
    x2_dots = (int) ( SANE_UNFIX(ms->val[OPT_BR_X].w) * dpm + 0.5 );
    y2_dots = (int) ( SANE_UNFIX(ms->val[OPT_BR_Y].w) * dpm + 0.5 );

    /* special case, xscanimage sometimes tries to set OPT_BR_Y */
    /* to a too big value; bug in xscanimage ? */ 
    ms->width_dots = MIN(abs(x2_dots - ms->x1_dots), mi->geo_width);
    ms->height_dots = abs(y2_dots - ms->y1_dots);
    /* ensure a minimum scan area */
    if ( ms->height_dots < 10 )
        ms->height_dots = 10;
    if ( ms->width_dots < 10 )
        ms->width_dots = 10;

    if ( ms->val[OPT_RESOLUTION_BIND].w == SANE_TRUE )
      {
        ms->x_resolution_dpi = 
                    (SANE_Int) (SANE_UNFIX(ms->val[OPT_RESOLUTION].w) + 0.5);
        ms->y_resolution_dpi = 
                    (SANE_Int) (SANE_UNFIX(ms->val[OPT_RESOLUTION].w) + 0.5);
      }
    else
      {
        ms->x_resolution_dpi = 
                    (SANE_Int) (SANE_UNFIX(ms->val[OPT_X_RESOLUTION].w) + 0.5);
        ms->y_resolution_dpi = 
                    (SANE_Int) (SANE_UNFIX(ms->val[OPT_Y_RESOLUTION].w) + 0.5);
      }

    if ( ms->x_resolution_dpi < 10 ) 
        ms->x_resolution_dpi = 10; 
    if ( ms->y_resolution_dpi < 10 ) 
        ms->y_resolution_dpi = 10;

    DBG(30, "get_scan_parameters: yres=%d, x1=%d, width=%d, y1=%d, height=%d\n",
             ms->y_resolution_dpi, ms->x1_dots, ms->width_dots, 
             ms->y1_dots, ms->height_dots);

    /* Preview mode */
    if ( ms->val[OPT_PREVIEW].w == SANE_TRUE )
        ms->fastscan = SANE_TRUE;
    else 
        ms->fastscan = SANE_FALSE;

    ms->quality = SANE_TRUE;
    ms->rawdat = 0;

    /* brightness, contrast, values 1,..,255 */
    ms->brightness_m = (u_int8_t) (SANE_UNFIX(ms->val[OPT_BRIGHTNESS].w) 
                      / SANE_UNFIX(md->percentage_range.max) * 254.0) + 1;
    ms->brightness_r = ms->brightness_g = ms->brightness_b = ms->brightness_m;

    ms->contrast_m = (u_int8_t) (SANE_UNFIX(ms->val[OPT_CONTRAST].w) 
                    / SANE_UNFIX(md->percentage_range.max) * 254.0) + 1;
    ms->contrast_r = ms->contrast_g = ms->contrast_b = ms->contrast_m;

    /* shadow, midtone, highlight, exposure */
    ms->shadow_m = (u_int8_t) ms->val[OPT_SHADOW].w;                
    ms->shadow_r = (u_int8_t) ms->val[OPT_SHADOW_R].w;                
    ms->shadow_g = (u_int8_t) ms->val[OPT_SHADOW_G].w;                
    ms->shadow_b = (u_int8_t) ms->val[OPT_SHADOW_B].w;
    ms->midtone_m = (u_int8_t) ms->val[OPT_MIDTONE].w;                
    ms->midtone_r = (u_int8_t) ms->val[OPT_MIDTONE_R].w;                
    ms->midtone_g = (u_int8_t) ms->val[OPT_MIDTONE_G].w;                
    ms->midtone_b = (u_int8_t) ms->val[OPT_MIDTONE_B].w;                
    ms->highlight_m = (u_int8_t) ms->val[OPT_HIGHLIGHT].w;     
    ms->highlight_r = (u_int8_t) ms->val[OPT_HIGHLIGHT_R].w;     
    ms->highlight_g = (u_int8_t) ms->val[OPT_HIGHLIGHT_G].w;     
    ms->highlight_b = (u_int8_t) ms->val[OPT_HIGHLIGHT_B].w;
    ms->exposure_m = (u_int8_t) ms->val[OPT_EXPOSURE].w / 2;     
    ms->exposure_r = (u_int8_t) ms->val[OPT_EXPOSURE_R].w / 2;     
    ms->exposure_g = (u_int8_t) ms->val[OPT_EXPOSURE_G].w / 2;     
    ms->exposure_b = (u_int8_t) ms->val[OPT_EXPOSURE_B].w / 2;     

    return SANE_STATUS_GOOD;
}


/*---------- get_lut_size() --------------------------------------------------*/

static SANE_Status
get_lut_size(Microtek2_Info *mi, int *max_lut_size, int *lut_entry_size)
{
    /* returns the maximum lookup table size */

    DBG(30, "get_lut_size: mi=%p\n", mi);

    *max_lut_size = 0;
    *lut_entry_size = 0;

    if ( mi->lut_cap  &  MI_LUTCAP_256B )
      {
        *max_lut_size = 256;
        *lut_entry_size = 1;
      }
    if ( mi->lut_cap  &  MI_LUTCAP_1024B )
      {
        *max_lut_size = 1024;
        *lut_entry_size = 1;
      }
    if ( mi->lut_cap  &  MI_LUTCAP_1024W )
      {
        *max_lut_size = 1024;
        *lut_entry_size = 2;
      }
    if ( mi->lut_cap  &  MI_LUTCAP_4096B )
      {
          *max_lut_size = 4096;
          *lut_entry_size = 1;
      }
    if ( mi->lut_cap  &  MI_LUTCAP_4096W )
      {
          *max_lut_size = 4096;
          *lut_entry_size = 2;
      }
    DBG(30, "get_lut_size:  mi=%p, lut_size=%d, lut_word=%d\n",
             mi, *max_lut_size, *lut_entry_size);
    return SANE_STATUS_GOOD;
}


/*---------- init_options() --------------------------------------------------*/

static SANE_Status
init_options(Microtek2_Scanner *ms, u_int8_t current_scan_source)
{
    /* This function is called every time, when the scan source changes. */
    /* The option values, that possibly change, are then reinitialized,  */
    /* whereas the option descriptions and option values that never */
    /* change are not */

    SANE_Option_Descriptor *sod;
    SANE_Status status;
    Microtek2_Option_Value *val;
    Microtek2_Device *md;
    Microtek2_Info *mi;
    int color;
    int i;
    static int first_call = 1;     /* indicates, whether option */
                                   /* descriptors must be initialized */


    DBG(30, "init_options: handle=%p, source=%d\n", ms, current_scan_source);

    sod = ms->sod;
    val = ms->val;
    md = ms->dev;
    mi = &md->info[current_scan_source];

    /* needed for gamma calculation */
    get_lut_size(mi, &md->max_lut_size, &md->lut_entry_size);

    /* calculate new values, where possibly needed */

    /* Scan source */
    if ( val[OPT_SOURCE].s )
        free((void *) val[OPT_SOURCE].s);
    i = 0;
    md->scansource_list[i] = (SANE_String) MD_SOURCESTRING_FLATBED;
    if ( current_scan_source == MD_SOURCE_FLATBED )
        val[OPT_SOURCE].s = (SANE_String) strdup(md->scansource_list[i]);
    if ( md->status.adfcnt )
      {
        md->scansource_list[++i] = (SANE_String) MD_SOURCESTRING_ADF;
        if ( current_scan_source == MD_SOURCE_ADF )
            val[OPT_SOURCE].s = (SANE_String) strdup(md->scansource_list[i]);
      }
    if ( md->status.tmacnt )
      {
        md->scansource_list[++i] = (SANE_String) MD_SOURCESTRING_TMA;
        if ( current_scan_source == MD_SOURCE_TMA )
            val[OPT_SOURCE].s = (SANE_String) strdup(md->scansource_list[i]);
      }
    if ( mi->option_device & MI_OPTDEV_STRIPE 
         && strncmp(md_lightlid35, "on", 2) == 0 )
      {
        md->scansource_list[++i] = (SANE_String) MD_SOURCESTRING_STRIPE;
        if ( current_scan_source == MD_SOURCE_STRIPE )
            val[OPT_SOURCE].s = (SANE_String) strdup(md->scansource_list[i]);
      }
    if ( mi->option_device & MI_OPTDEV_SLIDE
         && strncmp(md_lightlid35, "on", 2) == 0 )
      {
        md->scansource_list[++i] = (SANE_String) MD_SOURCESTRING_SLIDE;
        if ( current_scan_source == MD_SOURCE_SLIDE )
            val[OPT_SOURCE].s = (SANE_String) strdup(md->scansource_list[i]);
      }
    md->scansource_list[++i] = NULL;

    /* Scan mode */
    if ( val[OPT_MODE].s )
        free((void *) val[OPT_MODE].s);

    i = 0;
    if ( (mi->scanmode & MI_HASMODE_COLOR) )
      {
	if ( mi->depth & MI_HASDEPTH_12 )
	    md->scanmode_list[i++] = (SANE_String) MD_MODESTRING_COLOR36;
	if ( mi->depth & MI_HASDEPTH_10 )
	    md->scanmode_list[i++] = (SANE_String) MD_MODESTRING_COLOR30;
	md->scanmode_list[i] = (SANE_String) MD_MODESTRING_COLOR24;
        val[OPT_MODE].s = strdup(md->scanmode_list[i]);
        ++i;
      }

    if ( mi->scanmode & MI_HASMODE_GRAY )
      {
	if ( mi->depth & MI_HASDEPTH_12 )
	    md->scanmode_list[i++] = (SANE_String) MD_MODESTRING_GRAY12;
	if ( mi->depth & MI_HASDEPTH_10 )
	    md->scanmode_list[i++] = (SANE_String) MD_MODESTRING_GRAY10;
	md->scanmode_list[i] = (SANE_String) MD_MODESTRING_GRAY8;
        if ( ! (mi->scanmode & MI_HASMODE_COLOR ) )
            val[OPT_MODE].s = strdup(md->scanmode_list[i]);
        ++i;
      }

    if ( mi->scanmode & MI_HASMODE_HALFTONE ) 
      {
	md->scanmode_list[i] = (SANE_String) MD_MODESTRING_HALFTONE;
        if ( ! (mi->scanmode & MI_HASMODE_COLOR )
            && ! (mi->scanmode & MI_HASMODE_GRAY ) )
            val[OPT_MODE].s = strdup(md->scanmode_list[i]);
        ++i;
      }

    if ( mi->scanmode & MI_HASMODE_LINEART )
      {
	md->scanmode_list[i] = (SANE_String) MD_MODESTRING_LINEART;
        if ( ! (mi->scanmode & MI_HASMODE_COLOR )
            && ! (mi->scanmode & MI_HASMODE_GRAY )
            && ! (mi->scanmode & MI_HASMODE_HALFTONE ) )
            val[OPT_MODE].s = strdup(md->scanmode_list[i]);
        ++i;
      }
    md->scanmode_list[i] = NULL;

    /* Halftone */
    md->halftone_mode_list[0] = (SANE_String) MD_HALFTONE0;
    md->halftone_mode_list[1] = (SANE_String) MD_HALFTONE1;
    md->halftone_mode_list[2] = (SANE_String) MD_HALFTONE2;
    md->halftone_mode_list[3] = (SANE_String) MD_HALFTONE3;
    md->halftone_mode_list[4] = (SANE_String) MD_HALFTONE4;
    md->halftone_mode_list[5] = (SANE_String) MD_HALFTONE5;
    md->halftone_mode_list[6] = (SANE_String) MD_HALFTONE6;
    md->halftone_mode_list[7] = (SANE_String) MD_HALFTONE7;
    md->halftone_mode_list[8] = (SANE_String) MD_HALFTONE8;
    md->halftone_mode_list[9] = (SANE_String) MD_HALFTONE9;
    md->halftone_mode_list[10] = (SANE_String) MD_HALFTONE10;
    md->halftone_mode_list[11] = (SANE_String) MD_HALFTONE11;
    md->halftone_mode_list[12] = NULL;  
    if ( val[OPT_HALFTONE].s )
        free((void *) val[OPT_HALFTONE].s);
    val[OPT_HALFTONE].s = strdup(md->halftone_mode_list[0]);
    
    /* Resolution */
    md->x_res_range_dpi.min = SANE_FIX(10.0);
    md->x_res_range_dpi.max = SANE_FIX(mi->max_xresolution);
    md->x_res_range_dpi.quant = SANE_FIX(1.0);
    val[OPT_RESOLUTION].w = MIN(MD_RESOLUTION_DEFAULT, md->x_res_range_dpi.max);
    val[OPT_X_RESOLUTION].w =
                           MIN(MD_RESOLUTION_DEFAULT,md->x_res_range_dpi.max);
    md->y_res_range_dpi.min = SANE_FIX(10.0);
    md->y_res_range_dpi.max = SANE_FIX(mi->max_yresolution);
    md->y_res_range_dpi.quant = SANE_FIX(1.0);
    val[OPT_Y_RESOLUTION].w = val[OPT_RESOLUTION].w; /* bind is default */
    val[OPT_RESOLUTION_BIND].w = SANE_TRUE;

    /* enable/disable option for backtracking */
    val[OPT_DISABLE_BACKTRACK].w =SANE_FALSE;

    /* Preview mode */
    val[OPT_PREVIEW].w = SANE_FALSE;

    /* Geometry */
    md->x_range_mm.min = SANE_FIX(0.0);
    md->x_range_mm.max = SANE_FIX((double) mi->geo_width 
                                  / (double) mi->opt_resolution
                                  * MM_PER_INCH);
    md->x_range_mm.quant = SANE_FIX(0.0);
    md->y_range_mm.min = SANE_FIX(0.0);
    md->y_range_mm.max = SANE_FIX((double) mi->geo_height 
                                  / (double) mi->opt_resolution
                                  * MM_PER_INCH);
    md->y_range_mm.quant = SANE_FIX(0.0);
    val[OPT_TL_X].w = SANE_FIX(0.0);
    val[OPT_TL_Y].w = SANE_FIX(0.0);
    val[OPT_BR_X].w = md->x_range_mm.max;
    val[OPT_BR_Y].w = md->y_range_mm.max;

    /* Enhancement group */
    val[OPT_BRIGHTNESS].w = MD_BRIGHTNESS_DEFAULT;
    val[OPT_CONTRAST].w = MD_CONTRAST_DEFAULT;
    val[OPT_THRESHOLD].w = MD_THRESHOLD_DEFAULT;

    /* Gamma */
    /* linear gamma must come first */
    i = 0;
    if ( md->max_lut_size > 0 )
      {
        md->gammamode_list[i++] = (SANE_String) MD_GAMMAMODE_LINEAR;
        md->gammamode_list[i++] = (SANE_String) MD_GAMMAMODE_SCALAR;
        md->gammamode_list[i++] = (SANE_String) MD_GAMMAMODE_CUSTOM; 
      }
    md->gammamode_list[i] = NULL;
    if ( val[OPT_GAMMA_MODE].s )
        free((void *) val[OPT_GAMMA_MODE].s);
    val[OPT_GAMMA_MODE].s = strdup(md->gammamode_list[0]);
    val[OPT_GAMMA_BIND].w = SANE_TRUE;
    val[OPT_GAMMA_SCALAR].w = MD_GAMMA_DEFAULT;
    val[OPT_GAMMA_SCALAR_R].w = MD_GAMMA_DEFAULT;
    val[OPT_GAMMA_SCALAR_G].w = MD_GAMMA_DEFAULT;
    val[OPT_GAMMA_SCALAR_B].w = MD_GAMMA_DEFAULT;

    for ( color = 0; color < 4; color++ )
      {
        if ( md->custom_gamma_table[color] )
            free((void *) md->custom_gamma_table[color]);
        md->custom_gamma_table[color] = 
                     (SANE_Int *) malloc(md->max_lut_size * sizeof(SANE_Int));
        if ( md->custom_gamma_table[color] == NULL )
          {
            DBG(1, "init_options: malloc for custom gamma table failed\n");
            return SANE_STATUS_NO_MEM;
          }
        for ( i = 0; i < md->max_lut_size; i++ )
            md->custom_gamma_table[color][i] = i;
      }
    md->custom_gamma_range.min = 0;
    md->custom_gamma_range.max = MAX(0, md->max_lut_size - 1);
    md->custom_gamma_range.quant = 1;
    
    val[OPT_GAMMA_CUSTOM].wa = &md->custom_gamma_table[0][0];
    val[OPT_GAMMA_CUSTOM_R].wa = &md->custom_gamma_table[1][0];
    val[OPT_GAMMA_CUSTOM_G].wa = &md->custom_gamma_table[2][0];
    val[OPT_GAMMA_CUSTOM_B].wa = &md->custom_gamma_table[3][0];

    /* Shadow, midtone, highlight, exposure time */
    md->channel_list[0] = (SANE_String) MD_CHANNEL_MASTER;
    md->channel_list[1] = (SANE_String) MD_CHANNEL_RED;
    md->channel_list[2] = (SANE_String) MD_CHANNEL_GREEN;
    md->channel_list[3] = (SANE_String) MD_CHANNEL_BLUE;
    md->channel_list[4] = NULL;
    if ( val[OPT_CHANNEL].s )
        free((void *) val[OPT_CHANNEL].s);
    val[OPT_CHANNEL].s = strdup(md->channel_list[0]);
    val[OPT_SHADOW].w = MD_SHADOW_DEFAULT;
    val[OPT_SHADOW_R].w = MD_SHADOW_DEFAULT;
    val[OPT_SHADOW_G].w = MD_SHADOW_DEFAULT;
    val[OPT_SHADOW_B].w = MD_SHADOW_DEFAULT;
    val[OPT_MIDTONE].w = MD_MIDTONE_DEFAULT;
    val[OPT_MIDTONE_R].w = MD_MIDTONE_DEFAULT;
    val[OPT_MIDTONE_G].w = MD_MIDTONE_DEFAULT;
    val[OPT_MIDTONE_B].w = MD_MIDTONE_DEFAULT;
    val[OPT_HIGHLIGHT].w = MD_HIGHLIGHT_DEFAULT;
    val[OPT_HIGHLIGHT_R].w = MD_HIGHLIGHT_DEFAULT;
    val[OPT_HIGHLIGHT_G].w = MD_HIGHLIGHT_DEFAULT;
    val[OPT_HIGHLIGHT_B].w = MD_HIGHLIGHT_DEFAULT;
    val[OPT_EXPOSURE].w = MD_EXPOSURE_DEFAULT;
    val[OPT_EXPOSURE_R].w = MD_EXPOSURE_DEFAULT;
    val[OPT_EXPOSURE_G].w = MD_EXPOSURE_DEFAULT;
    val[OPT_EXPOSURE_B].w = MD_EXPOSURE_DEFAULT;

    if ( first_call )
      {
        first_call = 0;

        /* initialize option descriptors and ranges */
       
        /* Percentage range for brightness, contrast */
        md->percentage_range.min = 0 << SANE_FIXED_SCALE_SHIFT;
        md->percentage_range.max = 200 << SANE_FIXED_SCALE_SHIFT;
        md->percentage_range.quant = 1 << SANE_FIXED_SCALE_SHIFT;

        md->threshold_range.min = 1;
        md->threshold_range.max = 255;
        md->threshold_range.quant = 1;

        md->scalar_gamma_range.min = SANE_FIX(0.1);
        md->scalar_gamma_range.max = SANE_FIX(4.0);
        md->scalar_gamma_range.quant = SANE_FIX(0.1);

        md->shadow_range.min = 0;
        md->shadow_range.max = 253;
        md->shadow_range.quant = 1;

        md->midtone_range.min = 1;
        md->midtone_range.max = 254;
        md->midtone_range.quant = 1;

        md->highlight_range.min = 2;
        md->highlight_range.max = 255;
        md->highlight_range.quant = 1;

        md->exposure_range.min = 0;
        md->exposure_range.max = 510;
        md->exposure_range.quant = 2;

        /* default for most options */
        for ( i = 0; i < NUM_OPTIONS; i++ )
          {
            sod[i].type = SANE_TYPE_FIXED;
            sod[i].unit = SANE_UNIT_NONE;
            sod[i].size = sizeof(SANE_Fixed);
            sod[i].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
            sod[i].constraint_type = SANE_CONSTRAINT_RANGE;
          }

        sod[OPT_NUM_OPTS].name = SANE_NAME_NUM_OPTIONS;
        sod[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
        sod[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
        sod[OPT_NUM_OPTS].type = SANE_TYPE_INT;
        sod[OPT_NUM_OPTS].size = sizeof (SANE_Int);
        sod[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
        sod[OPT_NUM_OPTS].constraint_type = SANE_CONSTRAINT_NONE;
        val[OPT_NUM_OPTS].w = NUM_OPTIONS;      /* NUM_OPTIONS is no option */

        /* The Scan Mode Group */
        sod[OPT_MODE_GROUP].title = "Scan Mode";
        sod[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
        sod[OPT_MODE_GROUP].desc = "";
        sod[OPT_MODE_GROUP].cap = 0;
        sod[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

        /*Scan source */
        sod[OPT_SOURCE].name = M_NAME_SCANSOURCE;
        sod[OPT_SOURCE].title = M_TITLE_SCANSOURCE;
        sod[OPT_SOURCE].desc = M_DESC_SCANSOURCE;
        sod[OPT_SOURCE].type = SANE_TYPE_STRING;
        sod[OPT_SOURCE].size = max_string_size(md->scansource_list);
        /* if there is only one scan source, deactivate option */
        if ( md->scansource_list[1] == NULL )
            sod[OPT_SOURCE].cap |= SANE_CAP_INACTIVE;
        sod[OPT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
        sod[OPT_SOURCE].constraint.string_list = md->scansource_list;

        /* Scan mode */
        sod[OPT_MODE].name = SANE_NAME_SCAN_MODE;
        sod[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
        sod[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
        sod[OPT_MODE].type = SANE_TYPE_STRING;
        sod[OPT_MODE].size = max_string_size(md->scanmode_list);
        sod[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
        sod[OPT_MODE].constraint.string_list = md->scanmode_list;
   
        /* Halftone */
        sod[OPT_HALFTONE].name = SANE_NAME_HALFTONE;
        sod[OPT_HALFTONE].title = SANE_TITLE_HALFTONE;
        sod[OPT_HALFTONE].desc = SANE_DESC_HALFTONE;
        sod[OPT_HALFTONE].type = SANE_TYPE_STRING;
        sod[OPT_HALFTONE].size = max_string_size(md->halftone_mode_list);
        sod[OPT_HALFTONE].cap  |= SANE_CAP_INACTIVE;
        sod[OPT_HALFTONE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
        sod[OPT_HALFTONE].constraint.string_list = md->halftone_mode_list;

        /* Resolution */
        sod[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
        sod[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
        sod[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
        sod[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
        sod[OPT_RESOLUTION].constraint.range = &md->x_res_range_dpi;
    
        sod[OPT_X_RESOLUTION].name = "x-" SANE_NAME_SCAN_RESOLUTION;
        sod[OPT_X_RESOLUTION].title = "X-Resolution";
        sod[OPT_X_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
        sod[OPT_X_RESOLUTION].unit = SANE_UNIT_DPI;
        sod[OPT_X_RESOLUTION].cap |= SANE_CAP_INACTIVE;
        sod[OPT_X_RESOLUTION].constraint.range = &md->x_res_range_dpi;

        sod[OPT_Y_RESOLUTION].name = "y-" SANE_NAME_SCAN_RESOLUTION;
        sod[OPT_Y_RESOLUTION].title = "Y-Resolution";
        sod[OPT_Y_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
        sod[OPT_Y_RESOLUTION].unit = SANE_UNIT_DPI;
        sod[OPT_Y_RESOLUTION].cap |= SANE_CAP_INACTIVE;
        sod[OPT_Y_RESOLUTION].constraint.range = &md->y_res_range_dpi;

        sod[OPT_RESOLUTION_BIND].name = SANE_NAME_RESOLUTION_BIND;
        sod[OPT_RESOLUTION_BIND].title = SANE_TITLE_RESOLUTION_BIND;
        sod[OPT_RESOLUTION_BIND].desc = SANE_DESC_RESOLUTION_BIND;
        sod[OPT_RESOLUTION_BIND].type = SANE_TYPE_BOOL;
        sod[OPT_RESOLUTION_BIND].size = sizeof(SANE_Bool);
        sod[OPT_RESOLUTION_BIND].constraint_type = SANE_CONSTRAINT_NONE;

        /* enable/disable option for backtracking */
        sod[OPT_DISABLE_BACKTRACK].name = M_NAME_NOBACKTRACK;
        sod[OPT_DISABLE_BACKTRACK].title = M_TITLE_NOBACKTRACK;
        sod[OPT_DISABLE_BACKTRACK].desc = M_DESC_NOBACKTRACK;
        sod[OPT_DISABLE_BACKTRACK].type = SANE_TYPE_BOOL;
        sod[OPT_DISABLE_BACKTRACK].size = sizeof(SANE_Bool);
        sod[OPT_DISABLE_BACKTRACK].cap |= SANE_CAP_ADVANCED;
        sod[OPT_DISABLE_BACKTRACK].constraint_type = SANE_CONSTRAINT_NONE;
        if ( strcmp(md_no_backtracking, "off") == 0 )
            sod[OPT_DISABLE_BACKTRACK].cap |= SANE_CAP_INACTIVE;

        /* Preview */
        sod[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
        sod[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
        sod[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
        sod[OPT_PREVIEW].type = SANE_TYPE_BOOL;
        sod[OPT_PREVIEW].size = sizeof(SANE_Bool);
        sod[OPT_PREVIEW].constraint_type = SANE_CONSTRAINT_NONE;

        /* Geometry group, for scan area selection */ 
        sod[OPT_GEOMETRY_GROUP].title = "Geometry";
        sod[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
        sod[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;
        sod[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

        sod[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
        sod[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
        sod[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
        sod[OPT_TL_X].unit = SANE_UNIT_MM;
        sod[OPT_TL_X].constraint.range = &md->x_range_mm;

        sod[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
        sod[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
        sod[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
        sod[OPT_TL_Y].unit = SANE_UNIT_MM;
        sod[OPT_TL_Y].constraint.range = &md->y_range_mm;

        sod[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
        sod[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
        sod[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
        sod[OPT_BR_X].unit = SANE_UNIT_MM;
        sod[OPT_BR_X].constraint.range = &md->x_range_mm;
    
        sod[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
        sod[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
        sod[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
        sod[OPT_BR_Y].unit = SANE_UNIT_MM;
        sod[OPT_BR_Y].constraint.range = &md->y_range_mm;

        /* Enhancement group */
        sod[OPT_ENHANCEMENT_GROUP].title = "Enhancement";
        sod[OPT_ENHANCEMENT_GROUP].desc = "";
        sod[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
        sod[OPT_ENHANCEMENT_GROUP].cap = 0;

        sod[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
        sod[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
        sod[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
        sod[OPT_BRIGHTNESS].unit = SANE_UNIT_PERCENT;
        sod[OPT_BRIGHTNESS].constraint.range = &md->percentage_range;

        sod[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
        sod[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
        sod[OPT_CONTRAST].desc = SANE_DESC_CONTRAST;
        sod[OPT_CONTRAST].unit = SANE_UNIT_PERCENT;
        sod[OPT_CONTRAST].constraint.range = &md->percentage_range;

        sod[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
        sod[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
        sod[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
        sod[OPT_THRESHOLD].type = SANE_TYPE_INT;
        sod[OPT_THRESHOLD].size = sizeof(SANE_Int);
        sod[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
        sod[OPT_THRESHOLD].constraint.range = &md->threshold_range;

        /* Gamma */
        sod[OPT_GAMMA_GROUP].title = "Gamma";
        sod[OPT_GAMMA_GROUP].desc = "";
        sod[OPT_GAMMA_GROUP].type = SANE_TYPE_GROUP;
        sod[OPT_GAMMA_GROUP].cap = 0;
        sod[OPT_GAMMA_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

        sod[OPT_GAMMA_MODE].name = M_NAME_GAMMA_MODE;
        sod[OPT_GAMMA_MODE].title = M_TITLE_GAMMA_MODE;
        sod[OPT_GAMMA_MODE].desc = M_DESC_GAMMA_MODE;
        sod[OPT_GAMMA_MODE].type = SANE_TYPE_STRING;
        sod[OPT_GAMMA_MODE].size = max_string_size(md->gammamode_list);
        /* if the scanner doesn't support d/l of lookup tables make this */
        /* option unselectable */
        if ( md->gammamode_list[0] == NULL )
            sod[OPT_GAMMA_MODE].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
        sod[OPT_GAMMA_MODE].constraint.string_list = md->gammamode_list;

        sod[OPT_GAMMA_BIND].name = M_NAME_GAMMA_BIND;
        sod[OPT_GAMMA_BIND].title = M_TITLE_GAMMA_BIND;
        sod[OPT_GAMMA_BIND].desc = M_DESC_GAMMA_BIND;
        sod[OPT_GAMMA_BIND].type = SANE_TYPE_BOOL;
        sod[OPT_GAMMA_BIND].size = sizeof(SANE_Bool);
        sod[OPT_GAMMA_BIND].constraint_type = SANE_CONSTRAINT_NONE;

        /* this is active if gamma_bind == true and gammamode == scalar */
        sod[OPT_GAMMA_SCALAR].name = M_NAME_GAMMA_SCALAR;
        sod[OPT_GAMMA_SCALAR].title = M_TITLE_GAMMA_SCALAR;
        sod[OPT_GAMMA_SCALAR].desc = M_DESC_GAMMA_SCALAR;
        sod[OPT_GAMMA_SCALAR].constraint.range = &md->scalar_gamma_range;

        sod[OPT_GAMMA_SCALAR_R].name = M_NAME_GAMMA_SCALAR_R;
        sod[OPT_GAMMA_SCALAR_R].title = M_TITLE_GAMMA_SCALAR_R;
        sod[OPT_GAMMA_SCALAR_R].desc = M_DESC_GAMMA_SCALAR_R;
        sod[OPT_GAMMA_SCALAR_R].constraint.range = &md->scalar_gamma_range;

        sod[OPT_GAMMA_SCALAR_G].name = M_NAME_GAMMA_SCALAR_G;
        sod[OPT_GAMMA_SCALAR_G].title = M_TITLE_GAMMA_SCALAR_G;
        sod[OPT_GAMMA_SCALAR_G].desc = M_DESC_GAMMA_SCALAR_G;
        sod[OPT_GAMMA_SCALAR_G].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_SCALAR_G].constraint.range = &md->scalar_gamma_range;

        sod[OPT_GAMMA_SCALAR_B].name = M_NAME_GAMMA_SCALAR_B;
        sod[OPT_GAMMA_SCALAR_B].title = M_TITLE_GAMMA_SCALAR_B;
        sod[OPT_GAMMA_SCALAR_B].desc = M_DESC_GAMMA_SCALAR_B;
        sod[OPT_GAMMA_SCALAR_B].constraint.range = &md->scalar_gamma_range;

        sod[OPT_GAMMA_CUSTOM].name = SANE_NAME_GAMMA_VECTOR;
        sod[OPT_GAMMA_CUSTOM].title = SANE_TITLE_GAMMA_VECTOR;
        sod[OPT_GAMMA_CUSTOM].desc = SANE_DESC_GAMMA_VECTOR;
        sod[OPT_GAMMA_CUSTOM].type = SANE_TYPE_INT;
        sod[OPT_GAMMA_CUSTOM].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM].size = md->max_lut_size * sizeof (SANE_Int);
        sod[OPT_GAMMA_CUSTOM].constraint.range = &md->custom_gamma_range;
    
        sod[OPT_GAMMA_CUSTOM_R].name = SANE_NAME_GAMMA_VECTOR_R;
        sod[OPT_GAMMA_CUSTOM_R].title = SANE_TITLE_GAMMA_VECTOR_R;
        sod[OPT_GAMMA_CUSTOM_R].desc = SANE_DESC_GAMMA_VECTOR_R;
        sod[OPT_GAMMA_CUSTOM_R].type = SANE_TYPE_INT;
        sod[OPT_GAMMA_CUSTOM_R].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM_R].size = md->max_lut_size * sizeof (SANE_Int);
        sod[OPT_GAMMA_CUSTOM_R].constraint.range = &md->custom_gamma_range;
    
        sod[OPT_GAMMA_CUSTOM_G].name = SANE_NAME_GAMMA_VECTOR_G;
        sod[OPT_GAMMA_CUSTOM_G].title = SANE_TITLE_GAMMA_VECTOR_G;
        sod[OPT_GAMMA_CUSTOM_G].desc = SANE_DESC_GAMMA_VECTOR_G;
        sod[OPT_GAMMA_CUSTOM_G].type = SANE_TYPE_INT;
        sod[OPT_GAMMA_CUSTOM_G].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM_G].size = md->max_lut_size * sizeof (SANE_Int);
        sod[OPT_GAMMA_CUSTOM_G].constraint.range = &md->custom_gamma_range;
    
        sod[OPT_GAMMA_CUSTOM_B].name = SANE_NAME_GAMMA_VECTOR_B;
        sod[OPT_GAMMA_CUSTOM_B].title = SANE_TITLE_GAMMA_VECTOR_B;
        sod[OPT_GAMMA_CUSTOM_B].desc = SANE_DESC_GAMMA_VECTOR_B;
        sod[OPT_GAMMA_CUSTOM_B].type = SANE_TYPE_INT;
        sod[OPT_GAMMA_CUSTOM_B].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM_B].size = md->max_lut_size * sizeof (SANE_Int);
        sod[OPT_GAMMA_CUSTOM_B].constraint.range = &md->custom_gamma_range;

        /* Shadow, midtone, highlight */
        sod[OPT_SMH_GROUP].title = "Shadow, midtone, highlight, exposure time";
        sod[OPT_SMH_GROUP].desc = "";
        sod[OPT_SMH_GROUP].type = SANE_TYPE_GROUP;
        sod[OPT_SMH_GROUP].cap = 0;
        sod[OPT_SMH_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

        sod[OPT_CHANNEL].name = M_NAME_CHANNEL;
        sod[OPT_CHANNEL].title = M_TITLE_CHANNEL;
        sod[OPT_CHANNEL].desc = M_DESC_CHANNEL;
        sod[OPT_CHANNEL].type = SANE_TYPE_STRING;
        sod[OPT_CHANNEL].size = max_string_size(md->channel_list);
        sod[OPT_CHANNEL].constraint_type = SANE_CONSTRAINT_STRING_LIST;
        sod[OPT_CHANNEL].constraint.string_list = md->channel_list;

        sod[OPT_SHADOW].name = SANE_NAME_SHADOW;
        sod[OPT_SHADOW].title = SANE_TITLE_SHADOW;
        sod[OPT_SHADOW].desc = SANE_DESC_SHADOW;
        sod[OPT_SHADOW].type = SANE_TYPE_INT;
        sod[OPT_SHADOW].size = sizeof(SANE_Int);
        sod[OPT_SHADOW].constraint.range = &md->shadow_range;

        sod[OPT_SHADOW_R].name = SANE_NAME_SHADOW_R;
        sod[OPT_SHADOW_R].title = SANE_TITLE_SHADOW_R;
        sod[OPT_SHADOW_R].desc = SANE_DESC_SHADOW_R;
        sod[OPT_SHADOW_R].type = SANE_TYPE_INT;
        sod[OPT_SHADOW_R].size = sizeof(SANE_Int);
        sod[OPT_SHADOW_R].constraint.range = &md->shadow_range;

        sod[OPT_SHADOW_G].name = SANE_NAME_SHADOW_G;
        sod[OPT_SHADOW_G].title = SANE_TITLE_SHADOW_G;
        sod[OPT_SHADOW_G].desc = SANE_DESC_SHADOW_G;
        sod[OPT_SHADOW_G].type = SANE_TYPE_INT;
        sod[OPT_SHADOW_G].size = sizeof(SANE_Int);
        sod[OPT_SHADOW_G].constraint.range = &md->shadow_range;

        sod[OPT_SHADOW_B].name = SANE_NAME_SHADOW_B;
        sod[OPT_SHADOW_B].title = SANE_TITLE_SHADOW_B;
        sod[OPT_SHADOW_B].desc = SANE_DESC_SHADOW_B;
        sod[OPT_SHADOW_B].type = SANE_TYPE_INT;
        sod[OPT_SHADOW_B].size = sizeof(SANE_Int);
        sod[OPT_SHADOW_B].constraint.range = &md->shadow_range;

        sod[OPT_MIDTONE].name = M_NAME_MIDTONE;
        sod[OPT_MIDTONE].title = M_TITLE_MIDTONE;
        sod[OPT_MIDTONE].desc = M_DESC_MIDTONE;
        sod[OPT_MIDTONE].type = SANE_TYPE_INT;
        sod[OPT_MIDTONE].size = sizeof(SANE_Int);
        sod[OPT_MIDTONE].constraint.range = &md->midtone_range;

        sod[OPT_MIDTONE_R].name = M_NAME_MIDTONE_R;
        sod[OPT_MIDTONE_R].title = M_TITLE_MIDTONE_R;
        sod[OPT_MIDTONE_R].desc = M_DESC_MIDTONE_R;
        sod[OPT_MIDTONE_R].type = SANE_TYPE_INT;
        sod[OPT_MIDTONE_R].size = sizeof(SANE_Int);
        sod[OPT_MIDTONE_R].constraint.range = &md->midtone_range;

        sod[OPT_MIDTONE_G].name = M_NAME_MIDTONE_G;
        sod[OPT_MIDTONE_G].title = M_TITLE_MIDTONE_G;
        sod[OPT_MIDTONE_G].desc = M_DESC_MIDTONE_G;
        sod[OPT_MIDTONE_G].type = SANE_TYPE_INT;
        sod[OPT_MIDTONE_G].size = sizeof(SANE_Int);
        sod[OPT_MIDTONE_G].constraint.range = &md->midtone_range;

        sod[OPT_MIDTONE_B].name = M_NAME_MIDTONE_B;
        sod[OPT_MIDTONE_B].title = M_TITLE_MIDTONE_B;
        sod[OPT_MIDTONE_B].desc = M_DESC_MIDTONE_B;
        sod[OPT_MIDTONE_B].type = SANE_TYPE_INT;
        sod[OPT_MIDTONE_B].size = sizeof(SANE_Int);
        sod[OPT_MIDTONE_B].constraint.range = &md->midtone_range;

        sod[OPT_HIGHLIGHT].name = SANE_NAME_HIGHLIGHT;
        sod[OPT_HIGHLIGHT].title = SANE_TITLE_HIGHLIGHT;
        sod[OPT_HIGHLIGHT].desc = SANE_DESC_HIGHLIGHT;
        sod[OPT_HIGHLIGHT].type = SANE_TYPE_INT;
        sod[OPT_HIGHLIGHT].size = sizeof(SANE_Int);
        sod[OPT_HIGHLIGHT].constraint.range = &md->highlight_range;

        sod[OPT_HIGHLIGHT_R].name = SANE_NAME_HIGHLIGHT_R;
        sod[OPT_HIGHLIGHT_R].title = SANE_TITLE_HIGHLIGHT_R;
        sod[OPT_HIGHLIGHT_R].desc = SANE_DESC_HIGHLIGHT_R;
        sod[OPT_HIGHLIGHT_R].type = SANE_TYPE_INT;
        sod[OPT_HIGHLIGHT_R].size = sizeof(SANE_Int);
        sod[OPT_HIGHLIGHT_R].constraint.range = &md->highlight_range;

        sod[OPT_HIGHLIGHT_G].name = SANE_NAME_HIGHLIGHT_G;
        sod[OPT_HIGHLIGHT_G].title = SANE_TITLE_HIGHLIGHT_G;
        sod[OPT_HIGHLIGHT_G].desc = SANE_DESC_HIGHLIGHT_G;
        sod[OPT_HIGHLIGHT_G].type = SANE_TYPE_INT;
        sod[OPT_HIGHLIGHT_G].size = sizeof(SANE_Int);
        sod[OPT_HIGHLIGHT_G].constraint.range = &md->highlight_range;

        sod[OPT_HIGHLIGHT_B].name = SANE_NAME_HIGHLIGHT_B;
        sod[OPT_HIGHLIGHT_B].title = SANE_TITLE_HIGHLIGHT_B;
        sod[OPT_HIGHLIGHT_B].desc = SANE_DESC_HIGHLIGHT_B;
        sod[OPT_HIGHLIGHT_B].type = SANE_TYPE_INT;
        sod[OPT_HIGHLIGHT_B].size = sizeof(SANE_Int);
        sod[OPT_HIGHLIGHT_B].constraint.range = &md->highlight_range;

        sod[OPT_EXPOSURE].name = SANE_NAME_SCAN_EXPOS_TIME;
        sod[OPT_EXPOSURE].title = SANE_TITLE_SCAN_EXPOS_TIME;
        sod[OPT_EXPOSURE].desc = "Allows to lengthen the exposure time";
        sod[OPT_EXPOSURE].type = SANE_TYPE_INT;
        sod[OPT_EXPOSURE].unit = SANE_UNIT_PERCENT;
        sod[OPT_EXPOSURE].size = sizeof(SANE_Int);
        sod[OPT_EXPOSURE].constraint.range = &md->exposure_range;

        sod[OPT_EXPOSURE_R].name = "exposure-time-red";
        sod[OPT_EXPOSURE_R].title = "Exposure red";
        sod[OPT_EXPOSURE_R].desc = "Allows to lengthen the exposure time "
                                   "for the red channel";
        sod[OPT_EXPOSURE_R].type = SANE_TYPE_INT;
        sod[OPT_EXPOSURE_R].unit = SANE_UNIT_PERCENT;
        sod[OPT_EXPOSURE_R].size = sizeof(SANE_Int);
        sod[OPT_EXPOSURE_R].constraint.range = &md->exposure_range;

        sod[OPT_EXPOSURE_G].name = "exposure-time-green";
        sod[OPT_EXPOSURE_G].title = "Exposure green";
        sod[OPT_EXPOSURE_G].desc = "Allows to lengthen the exposure time "
                                   "for the green channel";
        sod[OPT_EXPOSURE_G].type = SANE_TYPE_INT;
        sod[OPT_EXPOSURE_G].unit = SANE_UNIT_PERCENT;
        sod[OPT_EXPOSURE_G].size = sizeof(SANE_Int);
        sod[OPT_EXPOSURE_G].constraint.range = &md->exposure_range;

        sod[OPT_EXPOSURE_B].name = "exposure-time-blue";
        sod[OPT_EXPOSURE_B].title = "Exposure blue";
        sod[OPT_EXPOSURE_B].desc = "Allows to lengthen the exposure time "
                                   "for the blue channel";
        sod[OPT_EXPOSURE_B].type = SANE_TYPE_INT;
        sod[OPT_EXPOSURE_B].unit = SANE_UNIT_PERCENT;
        sod[OPT_EXPOSURE_B].size = sizeof(SANE_Int);
        sod[OPT_EXPOSURE_B].constraint.range = &md->exposure_range;
      }

    status = set_option_dependencies(sod, val);
    if ( status != SANE_STATUS_GOOD )
        return status;

    return SANE_STATUS_GOOD;
}


/*---------- lplconcat_copy_pixels() -----------------------------------------*/

static SANE_Status
lplconcat_copy_pixels(u_int8_t **from, u_int32_t pixels, int depth, FILE *fp )
{
    int color;
    u_int32_t pixel;
    u_int16_t val16;


    DBG(30, "lplconcat_copy_pixels: from=%p, pixels=%d, depth=%d,\n",
             from, pixels, depth);

    if ( depth > 8 )
      {
        int scale;

        scale = 16 - depth;
        for ( pixel = 0; pixel < pixels; pixel++ )
          {
            for ( color = 0; color < 3; color++ )
              {
                val16 = *(u_int16_t *) from[color];
                val16 = ( val16 << scale ) | ( val16 & (( 1 << scale ) - 1) );
                fwrite((void *) &val16, 2, 1, fp);
                from[color] += 2;
              }
          }
      }
    else if ( depth == 8 )
      {
        for ( pixel = 0; pixel < pixels; pixel++ )
            for ( color = 0; color < 3; color++ )
              {
                fputc((char) *from[color], fp);
                ++from[color];
              } 
      }
    else
      {
        DBG(1, "lplconcat_copy_pixels: Unknown depth %d\n", depth);
        return SANE_STATUS_IO_ERROR;
      }

    return SANE_STATUS_GOOD;
}


/*---------- lplconcat_proc_data() -------------------------------------------*/

static SANE_Status                
lplconcat_proc_data(Microtek2_Scanner *ms)
{
    SANE_Status status;
    u_int32_t line;
    u_int8_t *from[3];
    u_int8_t *save_from[3];
    int color;


    DBG(30, "lplconcat_proc_data: ms=%p\n", ms);
             
    for ( color = 0; color < 3; color++ )
        from[color] = ms->buf.src_buf + color * ms->bpl;

    for ( line = 0; line < ms->src_lines_to_read; line++ )
      {
        for ( color = 0 ; color < 3; color++ )
            save_from[color] = from[color];

        status = lplconcat_copy_pixels(from, ms->ppl, ms->depth, ms->fp);
        if ( status != SANE_STATUS_GOOD )
            return status;

        for ( color = 0; color < 3; color++ )
            from[color] = save_from[color] + ms->bpl;
      }

    return SANE_STATUS_GOOD;
}


/*---------- max_string_size() -----------------------------------------------*/

static size_t
max_string_size (const SANE_String_Const strings[])
{
  size_t size;
  size_t max_size = 0;
  int i;

  for (i = 0; strings[i]; ++i) {
    size = strlen(strings[i]) + 1; /* +1 because NUL counts as part of string */
    if (size > max_size) max_size = size;
  }
  return max_size;
}


/*---------- gray_copy_pixels() ----------------------------------------------*/

static SANE_Status
gray_copy_pixels(u_int8_t *from, u_int32_t pixels, int depth, FILE *fp)
{
    u_int32_t pixel;


    DBG(30, "gray_copy_pixels: pixels=%d, from=%p, fp=%p, depth=%d\n",
             pixels, from, fp, depth);


    if ( depth > 8 )
      {
        int scale;

        scale = 16 - depth;
        for ( pixel = 0; pixel < pixels; pixel++ )
          {
            u_int16_t val;

            val = *(u_int16_t *) from; 
            val = ( val << scale ) | ( val & ( (1 << scale) - 1) ); 
            /* value = (*(u_int16_t *) from) << scale; */
            fwrite((void *) &val, 2, 1, fp);
            from += 2;
          }
      }
    else if ( depth == 8 )
      {
        fwrite((void *) from, pixels, 1, fp);
      }
    else if ( depth == 4 )
      {
        pixel = 0;
        while ( pixel < pixels )
          {
            fputc((char) ( ((*from >> 4) & 0x0f) | (*from & 0xf0) ), fp);
            ++pixel;
            if ( pixel < pixels )
                fputc((char) ( (*from & 0x0f) | ((*from << 4) & 0xf0) ), fp);
            ++from; 
            ++pixel;
          }
      }
    else
      {
        DBG(1, "gray_copy_pixels: Unknown depth %d\n", depth);
        return SANE_STATUS_IO_ERROR;
      }

    return SANE_STATUS_GOOD;
}


/*---------- gray_proc_data() ------------------------------------------------*/

static SANE_Status                
gray_proc_data(Microtek2_Scanner *ms)
{
    SANE_Status status;
    u_int8_t *from;

    DBG(30, "gray_proc_data: lines=%d, bpl=%d, ppl=%d, depth=%d\n",
             ms->src_lines_to_read, ms->bpl, ms->ppl, ms->depth);
 
    from = ms->buf.src_buf;
    do
      {
        status = gray_copy_pixels(from, ms->ppl, ms->depth, ms->fp);
        if ( status != SANE_STATUS_GOOD )
            return status;

        from += ms->bpl;
        --ms->src_lines_to_read;
      } while ( ms->src_lines_to_read > 0 );

    return SANE_STATUS_GOOD;
}


/*---------- proc_onebit_data() ----------------------------------------------*/

static SANE_Status                
proc_onebit_data(Microtek2_Scanner *ms)
{
    u_int32_t bytes_to_copy;     /* bytes per line to copy */
    u_int32_t line;
    u_int32_t byte;
    u_int8_t *from;


    DBG(30, "proc_onebit_data: ms=%p\n", ms);

    bytes_to_copy = ( ms->ppl + 7 ) / 8 ;

    DBG(30, "proc_onebit_data: bytes_to_copy=%d, lines=%d\n",
             bytes_to_copy, ms->src_lines_to_read);

    from = ms->buf.src_buf;
    line = 0;

    do
      {
        /* in onebit mode black and white colors are inverted */
        for ( byte = 0; byte < bytes_to_copy; byte++ )
            fputc( (char) ~from[byte], ms->fp);
        
        from += ms->bpl;
      } while ( ++line < ms->src_lines_to_read );

    return SANE_STATUS_GOOD;
}


/*---------- reader_process() ------------------------------------------------*/

static SANE_Status        
reader_process(Microtek2_Scanner *ms)
{
    Microtek2_Info *mi;
    Microtek2_Device *md;
    SANE_Status status;
    struct SIGACTION act;
    sigset_t sigterm_set;


    DBG(30, "reader_process: ms=%p\n", ms);

    md = ms->dev;
    mi = &md->info[md->scan_source];
    close(ms->fd[0]); 

    sigemptyset (&sigterm_set);
    sigaddset (&sigterm_set, SIGTERM);
    memset (&act, 0, sizeof (act));
    act.sa_handler = sigterm_handler;
    sigaction (SIGTERM, &act, 0);

    ms->fp = fdopen(ms->fd[1], "w");
    if ( ms->fp == NULL )
      {
        DBG(1, "reader_process: fdopen() failed, errno=%d\n", errno);
        return SANE_STATUS_IO_ERROR;
      }
    
    while ( ms->src_remaining_lines > 0 )
      {
       
        ms->src_lines_to_read =
                               MIN(ms->src_remaining_lines, ms->src_max_lines);
        ms->transfer_length = ms->src_lines_to_read * ms->bpl;

        DBG(30, "reader_process: transferlength=%d, lines=%d, linelength=%d, "
                "real_bpl=%d, srcbuf=%p\n", ms->transfer_length,
                 ms->src_lines_to_read, ms->bpl, ms->real_bpl, ms->buf.src_buf);
   
        sigprocmask (SIG_BLOCK, &sigterm_set, 0);
        status = scsi_read_image(ms);
        sigprocmask (SIG_UNBLOCK, &sigterm_set, 0);
        if ( status != SANE_STATUS_GOOD ) 
            return SANE_STATUS_IO_ERROR;

        ms->src_remaining_lines -= ms->src_lines_to_read;

        if ( ms_dump >= 4 )
            dump_area2(ms->buf.src_buf, ms->transfer_length, "scandata");

        /* test, output color indicator */
        /*  for ( i = 0 ; i < ms->transfer_length; i = i + ms->bpl)
            fprintf(stderr,"'%c' '%c' '%c'\n", *(ms->buf.src_buf + i),
                    *(ms->buf.src_buf + i + ms->bpl / 3),
                    *(ms->buf.src_buf + i + ms->bpl / 3 * 2));
        */
        

        /* prepare data for frontend */
        switch (ms->mode) 
          {
	    case MS_MODE_COLOR:
              if ( ! mi->onepass )
                /* TODO */
                {
                  DBG(1, "reader_process: 3 pass not yet supported\n");
                  return SANE_STATUS_IO_ERROR;
                }
              else 
                {
                  switch ( mi->data_format ) 
                    {
		      case MI_DATAFMT_CHUNKY:
                        status = chunky_proc_data(ms);
                        if ( status != SANE_STATUS_GOOD )
                            return status;
                        break;
		      case MI_DATAFMT_LPLCONCAT:
		        status = lplconcat_proc_data(ms);
                        if ( status != SANE_STATUS_GOOD )
                            return status;
                        break;
		      case MI_DATAFMT_LPLSEGREG:
		        status = segreg_proc_data(ms);
                        if ( status != SANE_STATUS_GOOD )
                            return status;
                        break;
		      case MI_DATAFMT_WORDCHUNKY:
                        status = wordchunky_proc_data(ms);
                        if ( status != SANE_STATUS_GOOD )
                            return status;
                        break;
                      default:
                        DBG(1, "reader_process: format %d\n", mi->data_format);
                        return SANE_STATUS_IO_ERROR;
                    }      
                }
              break;   
	    case MS_MODE_GRAY:
	      status = gray_proc_data(ms);
              if ( status != SANE_STATUS_GOOD )
                  return status;
              break;
	    case MS_MODE_HALFTONE:
	    case MS_MODE_LINEART:
              status = proc_onebit_data(ms);
              if ( status != SANE_STATUS_GOOD )
                  return status;
              break;
            default:
              DBG(1, "reader_process: Unknown scan mode %d\n", ms->mode);
              return SANE_STATUS_IO_ERROR;
          }
      }

    fclose(ms->fp);
    return SANE_STATUS_GOOD;
}


/*---------- segreg_proc_data() ----------------------------------------------*/

static SANE_Status                
segreg_proc_data(Microtek2_Scanner *ms)
{
    Microtek2_Device *md;
    Microtek2_Info *mi;
    SANE_Status status;
    char colormap[] = "RGB";
    u_int8_t *from;
    u_int32_t lines_to_deliver;
    int bpp;                  /* bytes per pixel */
    int bpp3;                   /* 3 * bytes per pixel */
    int bpf;                    /* bytes per frame including color indicator */
    int pad;
    int colseq2;
    int color;
    int save_current_src; 
    int frame;


    DBG(30, "segreg_proc_data: ms=%p\n", ms);

    md = ms->dev;
    mi = &md->info[md->scan_source];
    /* take a trailing junk byte into account */
    pad = (int) ceil( (double) (ms->ppl * ms->bits_per_pixel_in) / 8.0 ) % 2;
    bpp = ms->bits_per_pixel_out / 8; /* bits_per_pixel_out is either 8 or 16 */
    bpp3 = 3 * bpp; 
    bpf = ms->bpl / 3;

    DBG(30, "segreg_proc_data: lines=%d, bpl=%d, ppl=%d, bpf=%d, bpp=%d, "
            "depth=%d, pad=%d, freelines=%d\n", ms->src_lines_to_read,
             ms->bpl, ms->ppl, bpf, bpp, ms->depth, pad, ms->buf.free_lines);

    /* determine how many planes of each color are in the source buffer */
    from = ms->buf.src_buf;
    for ( frame = 0; frame < 3 * ms->src_lines_to_read; frame++, from += bpf )
      {
        switch ( *from )
          {
            case 'R':
              ++ms->buf.planes[0][MS_COLOR_RED];
              break;
            case 'G':
              ++ms->buf.planes[0][MS_COLOR_GREEN];
              break;
            case 'B':
              ++ms->buf.planes[0][MS_COLOR_BLUE];
              break;
            default:
              DBG(1, "segreg_proc_data: unknown color indicator (1) "
                     "0x%02x\n", *from);
              return SANE_STATUS_IO_ERROR;
              break;
          }
      }

    ms->buf.free_lines -= ms->src_lines_to_read;
    save_current_src = ms->buf.current_src;
    if ( ms->buf.free_lines < ms->src_max_lines )
      {
        ms->buf.current_src = ++ms->buf.current_src % 2;
        ms->buf.src_buf = ms->buf.src_buffer[ms->buf.current_src];
        ms->buf.free_lines = ms->buf.free_max_lines;
      }
    else
        ms->buf.src_buf += ms->src_lines_to_read * ms->bpl;

    colseq2 = mi->color_sequence[2];
    lines_to_deliver = ms->buf.planes[0][colseq2] + ms->buf.planes[1][colseq2];
    if ( lines_to_deliver == 0 )
        return SANE_STATUS_GOOD;

    DBG(30, "segreg_proc_data: planes[0][0]=%d, planes[0][1]=%d, "
            "planes[0][2]=%d\n", ms->buf.planes[0][0], ms->buf.planes[0][1],
             ms->buf.planes[0][2] ); 
    DBG(30, "segreg_proc_data: planes[1][0]=%d, planes[1][1]=%d, "
            "planes[1][2]=%d\n", ms->buf.planes[1][0], ms->buf.planes[1][1],
             ms->buf.planes[1][2] ); 
             
    while ( lines_to_deliver > 0 )
      {
        for ( color = 0; color < 3; color++ )
          {
            /* get the position of the next plane for each color */
            do
              {
                if ( *ms->buf.current_pos[color] == colormap[color] )
                    break;
                ms->buf.current_pos[color] += bpf;
              } while ( 1 );

            ms->buf.current_pos[color] += 2;    /* skip color indicator */
          }
 
        status = segreg_copy_pixels(ms->buf.current_pos,
                                    ms->ppl,
                                    ms->depth,
                                    ms->fp);
        if ( status != SANE_STATUS_GOOD )
            return status;

        for ( color = 0; color < 3; color++ )
          {
            /* skip a padding byte at the end, if present */
            ms->buf.current_pos[color] += pad;

            if ( ms->buf.planes[1][color] > 0 )
              {
                --ms->buf.planes[1][color];
                if ( ms->buf.planes[1][color] == 0 )
                    /* we have copied from the prehold buffer and are */
                    /* done now, we continue with the source buffer */
                    ms->buf.current_pos[color] =
                                        ms->buf.src_buffer[save_current_src];
              }
            else
              {
                --ms->buf.planes[0][color];
                if ( ms->buf.planes[0][color] == 0
                     && ms->buf.current_src != save_current_src )

                    ms->buf.current_pos[color] =
                                    ms->buf.src_buffer[ms->buf.current_src];
              }
          }
        DBG(1, "planes_to_deliver=%d\n", lines_to_deliver);
        --lines_to_deliver;
      }

    if ( ms->buf.current_src != save_current_src ) 
      {
        for ( color = 0; color < 3; color++ )
          {
            ms->buf.planes[1][color] += ms->buf.planes[0][color];
            ms->buf.planes[0][color] = 0;
          }
      }

    DBG(30, "segreg_proc_data: src_buf=%p, free_lines=%d\n",
             ms->buf.src_buf, ms->buf.free_lines); 

    return SANE_STATUS_GOOD;
}


/*---------- restore_gamma_options() -----------------------------------------*/

static SANE_Status                
restore_gamma_options(SANE_Option_Descriptor *sod, Microtek2_Option_Value *val)
{

    DBG(40, "restore_gamma_options: val=%p, sod=%p\n", val, sod);

    if ( strcmp(val[OPT_MODE].s, MD_MODESTRING_COLOR24) == 0 
         || strcmp(val[OPT_MODE].s, MD_MODESTRING_COLOR30) == 0 
         || strcmp(val[OPT_MODE].s, MD_MODESTRING_COLOR36) == 0 )
      {
        sod[OPT_GAMMA_MODE].cap &= ~SANE_CAP_INACTIVE;
        if ( strcmp(val[OPT_GAMMA_MODE].s, MD_GAMMAMODE_LINEAR) == 0 )
          {
            sod[OPT_GAMMA_BIND].cap |= SANE_CAP_INACTIVE;
            sod[OPT_GAMMA_SCALAR].cap |= SANE_CAP_INACTIVE;
            sod[OPT_GAMMA_SCALAR_R].cap |= SANE_CAP_INACTIVE;
            sod[OPT_GAMMA_SCALAR_G].cap |= SANE_CAP_INACTIVE;
            sod[OPT_GAMMA_SCALAR_B].cap |= SANE_CAP_INACTIVE;
            sod[OPT_GAMMA_CUSTOM].cap |= SANE_CAP_INACTIVE;
            sod[OPT_GAMMA_CUSTOM_R].cap |= SANE_CAP_INACTIVE;
            sod[OPT_GAMMA_CUSTOM_G].cap |= SANE_CAP_INACTIVE;
            sod[OPT_GAMMA_CUSTOM_B].cap |= SANE_CAP_INACTIVE;
          }
        else if ( strcmp(val[OPT_GAMMA_MODE].s, MD_GAMMAMODE_SCALAR) == 0 )
          {
            sod[OPT_GAMMA_BIND].cap &= ~SANE_CAP_INACTIVE;
            if ( val[OPT_GAMMA_BIND].w == SANE_TRUE ) 
              {
                sod[OPT_GAMMA_SCALAR].cap &= ~SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR_R].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR_G].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR_B].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM_R].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM_G].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM_B].cap |= SANE_CAP_INACTIVE;
              }
            else
              {
                sod[OPT_GAMMA_SCALAR].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR_R].cap &= ~SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR_G].cap &= ~SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR_B].cap &= ~SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM_R].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM_G].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM_B].cap |= SANE_CAP_INACTIVE;
              }
          }
        else if ( strcmp(val[OPT_GAMMA_MODE].s, MD_GAMMAMODE_CUSTOM) == 0 )
          {
            sod[OPT_GAMMA_BIND].cap &= ~SANE_CAP_INACTIVE;
            if ( val[OPT_GAMMA_BIND].w == SANE_TRUE ) 
              {
                sod[OPT_GAMMA_CUSTOM].cap &= ~SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM_R].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM_G].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM_B].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR_R].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR_G].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR_B].cap |= SANE_CAP_INACTIVE;
              }
            else
              {
                sod[OPT_GAMMA_CUSTOM].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM_R].cap &= ~SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM_G].cap &= ~SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_CUSTOM_B].cap &= ~SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR_R].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR_G].cap |= SANE_CAP_INACTIVE;
                sod[OPT_GAMMA_SCALAR_B].cap |= SANE_CAP_INACTIVE;
              }
          }
      }
    else if ( strcmp(val[OPT_MODE].s, MD_MODESTRING_GRAY12) == 0 
              || strcmp(val[OPT_MODE].s, MD_MODESTRING_GRAY10) == 0 
              || strcmp(val[OPT_MODE].s, MD_MODESTRING_GRAY8) == 0
              || strcmp(val[OPT_MODE].s, MD_MODESTRING_GRAY2) == 0 )
      {
        sod[OPT_GAMMA_MODE].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_BIND].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_SCALAR_R].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_SCALAR_G].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_SCALAR_B].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM_R].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM_G].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM_B].cap |= SANE_CAP_INACTIVE;
        if ( strcmp(val[OPT_GAMMA_MODE].s, MD_GAMMAMODE_LINEAR) == 0 )
          {
            sod[OPT_GAMMA_SCALAR].cap |= SANE_CAP_INACTIVE;
            sod[OPT_GAMMA_CUSTOM].cap |= SANE_CAP_INACTIVE;
          }
        else if ( strcmp(val[OPT_GAMMA_MODE].s, MD_GAMMAMODE_SCALAR) == 0 )
          {
            sod[OPT_GAMMA_SCALAR].cap &= ~SANE_CAP_INACTIVE;
            sod[OPT_GAMMA_CUSTOM].cap |= SANE_CAP_INACTIVE;
          }
        else if ( strcmp(val[OPT_GAMMA_MODE].s, MD_GAMMAMODE_CUSTOM) == 0 )
          {
            sod[OPT_GAMMA_CUSTOM].cap &= ~SANE_CAP_INACTIVE;
            sod[OPT_GAMMA_SCALAR].cap |= SANE_CAP_INACTIVE;
          }
      }
    else if ( strcmp(val[OPT_MODE].s, MD_MODESTRING_HALFTONE) == 0
              || strcmp(val[OPT_MODE].s, MD_MODESTRING_LINEART) == 0 )
      {
        /* reset gamma to default */
        if ( val[OPT_GAMMA_MODE].s )
            free((void *) val[OPT_GAMMA_MODE].s);
        val[OPT_GAMMA_MODE].s = strdup(MD_GAMMAMODE_LINEAR);         
        sod[OPT_GAMMA_MODE].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_BIND].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_SCALAR].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_SCALAR_R].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_SCALAR_G].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_SCALAR_B].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM_R].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM_G].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM_B].cap |= SANE_CAP_INACTIVE;
      }
    else
        DBG(1, "restore_gamma_options: unknown mode %s\n", val[OPT_MODE].s);
    
    return SANE_STATUS_GOOD;
}

              
/*---------- set_option_dependendencies() ------------------------------------*/

static SANE_Status                
set_option_dependencies(SANE_Option_Descriptor *sod, 
                        Microtek2_Option_Value *val)
{

    DBG(40, "set_option_dependencies: val=%p, sod=%p, mode=%s\n",
             val, sod, val[OPT_MODE].s);

    if ( strcmp(val[OPT_MODE].s, MD_MODESTRING_COLOR24) == 0 
         || strcmp(val[OPT_MODE].s, MD_MODESTRING_COLOR30) == 0 
         || strcmp(val[OPT_MODE].s, MD_MODESTRING_COLOR36) == 0 )
      {
        /* activate brightness,..., deactivate halftone pattern */
        /* and threshold */
        sod[OPT_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_CONTRAST].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_CHANNEL].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_SHADOW].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_MIDTONE].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_HIGHLIGHT].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_EXPOSURE].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_HALFTONE].cap |= SANE_CAP_INACTIVE;
        sod[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;

        /* reset options values that are inactive to their default */
        val[OPT_THRESHOLD].w = MD_THRESHOLD_DEFAULT;
      } 
    else if ( strcmp(val[OPT_MODE].s, MD_MODESTRING_GRAY12) == 0 
              || strcmp(val[OPT_MODE].s, MD_MODESTRING_GRAY10) == 0 
              || strcmp(val[OPT_MODE].s, MD_MODESTRING_GRAY8) == 0
              || strcmp(val[OPT_MODE].s, MD_MODESTRING_GRAY2) == 0 )
      {
        sod[OPT_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_CONTRAST].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_CHANNEL].cap |= SANE_CAP_INACTIVE;
        sod[OPT_SHADOW].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_MIDTONE].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_HIGHLIGHT].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_EXPOSURE].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_HALFTONE].cap |= SANE_CAP_INACTIVE;
        sod[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;

        /* reset options values that are inactive to their default */
        if ( val[OPT_CHANNEL].s )
            free((void *) val[OPT_CHANNEL].s);
        val[OPT_CHANNEL].s = strdup((SANE_String) MD_CHANNEL_MASTER);
      }
    else if ( strcmp(val[OPT_MODE].s, MD_MODESTRING_HALFTONE) == 0 )
      {
        sod[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
        sod[OPT_CONTRAST].cap |= SANE_CAP_INACTIVE;
        sod[OPT_CHANNEL].cap |= SANE_CAP_INACTIVE;
        sod[OPT_SHADOW].cap |= SANE_CAP_INACTIVE;
        sod[OPT_MIDTONE].cap |= SANE_CAP_INACTIVE;
        sod[OPT_HIGHLIGHT].cap |= SANE_CAP_INACTIVE;
        sod[OPT_EXPOSURE].cap |= SANE_CAP_INACTIVE;
        sod[OPT_HALFTONE].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;

        /* reset options values that are inactive to their default */
        val[OPT_BRIGHTNESS].w = MD_BRIGHTNESS_DEFAULT;
        val[OPT_CONTRAST].w = MD_CONTRAST_DEFAULT;
        if ( val[OPT_CHANNEL].s )
            free((void *) val[OPT_CHANNEL].s);
        val[OPT_CHANNEL].s = strdup((SANE_String) MD_CHANNEL_MASTER);
        val[OPT_SHADOW].w = MD_SHADOW_DEFAULT;
        val[OPT_MIDTONE].w = MD_MIDTONE_DEFAULT;
        val[OPT_HIGHLIGHT].w = MD_HIGHLIGHT_DEFAULT;
        val[OPT_EXPOSURE].w = MD_EXPOSURE_DEFAULT;
      }
    else if ( strcmp(val[OPT_MODE].s, MD_MODESTRING_LINEART) == 0 )
      {
        sod[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
        sod[OPT_CONTRAST].cap |= SANE_CAP_INACTIVE;
        sod[OPT_CHANNEL].cap |= SANE_CAP_INACTIVE;
        sod[OPT_SHADOW].cap |= SANE_CAP_INACTIVE;
        sod[OPT_MIDTONE].cap |= SANE_CAP_INACTIVE;
        sod[OPT_HIGHLIGHT].cap |= SANE_CAP_INACTIVE;
        sod[OPT_EXPOSURE].cap |= SANE_CAP_INACTIVE;
        sod[OPT_HALFTONE].cap |= SANE_CAP_INACTIVE;
        sod[OPT_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;

        /* reset options values that are inactive to their default */
        val[OPT_BRIGHTNESS].w = MD_BRIGHTNESS_DEFAULT;
        val[OPT_CONTRAST].w = MD_CONTRAST_DEFAULT;
        if ( val[OPT_CHANNEL].s )
            free((void *) val[OPT_CHANNEL].s);
        val[OPT_CHANNEL].s = strdup((SANE_String) MD_CHANNEL_MASTER);
        val[OPT_SHADOW].w = MD_SHADOW_DEFAULT;
        val[OPT_MIDTONE].w = MD_MIDTONE_DEFAULT;
        val[OPT_HIGHLIGHT].w = MD_HIGHLIGHT_DEFAULT;
        val[OPT_EXPOSURE].w = MD_EXPOSURE_DEFAULT;
      }
    else
      {
        DBG(1, "set_option_dependencies: unknown mode '%s'\n", 
                val[OPT_MODE].s );
        return SANE_STATUS_INVAL;
      }
                
    /* these ones are always inactive if the mode changes */
    sod[OPT_SHADOW_R].cap |= SANE_CAP_INACTIVE;
    sod[OPT_SHADOW_G].cap |= SANE_CAP_INACTIVE;
    sod[OPT_SHADOW_B].cap |= SANE_CAP_INACTIVE;
    sod[OPT_MIDTONE_R].cap |= SANE_CAP_INACTIVE;
    sod[OPT_MIDTONE_G].cap |= SANE_CAP_INACTIVE;
    sod[OPT_MIDTONE_B].cap |= SANE_CAP_INACTIVE;
    sod[OPT_HIGHLIGHT_R].cap |= SANE_CAP_INACTIVE;
    sod[OPT_HIGHLIGHT_G].cap |= SANE_CAP_INACTIVE;
    sod[OPT_HIGHLIGHT_B].cap |= SANE_CAP_INACTIVE;
    sod[OPT_EXPOSURE_R].cap |= SANE_CAP_INACTIVE;
    sod[OPT_EXPOSURE_G].cap |= SANE_CAP_INACTIVE;
    sod[OPT_EXPOSURE_B].cap |= SANE_CAP_INACTIVE;
    val[OPT_SHADOW_R].w = val[OPT_SHADOW_G].w = val[OPT_SHADOW_B].w
            = MD_SHADOW_DEFAULT;
    val[OPT_MIDTONE_R].w = val[OPT_MIDTONE_G].w = val[OPT_MIDTONE_B].w
            = MD_MIDTONE_DEFAULT;
    val[OPT_HIGHLIGHT_R].w = val[OPT_HIGHLIGHT_G].w = val[OPT_HIGHLIGHT_B].w
            = MD_HIGHLIGHT_DEFAULT;
    val[OPT_EXPOSURE_R].w = val[OPT_EXPOSURE_G].w = val[OPT_EXPOSURE_B].w
            = MD_EXPOSURE_DEFAULT;
                
    if ( SANE_OPTION_IS_SETTABLE(sod[OPT_GAMMA_MODE].cap) )
        restore_gamma_options(sod, val);

    return SANE_STATUS_GOOD;
}


/*---------- sigterm_handler()------------------------------------------------*/

static RETSIGTYPE
sigterm_handler (int signal)
{
  sanei_scsi_req_flush_all (); 
  _exit (SANE_STATUS_GOOD);
}


/*---------- wordchunky_copy_pixels() ----------------------------------------*/

static SANE_Status
wordchunky_copy_pixels(u_int8_t *from, u_int32_t pixels, int depth, FILE *fp)
{
    u_int32_t pixel;
    int color;

    DBG(30, "wordchunky_copy_pixels: from=%p, pixels=%d, depth=%d\n",
             from, pixels, depth);

    if ( depth > 8 )
      {
        int scale;
        u_int16_t val16;

        scale = 16 - depth;
        for ( pixel = 0; pixel < pixels; pixel++ )
          {
            for ( color = 0; color < 3; color++ )
              {
                val16 = *(u_int16_t *) from;
                val16 = ( val16 << scale ) | ( val16 & (( 1 << scale ) - 1) );
                fwrite((void *) &val16, 2, 1, fp);
                from += 2;
              }
          }
      }
    else if ( depth == 8 )
      {
        pixel = 0;
        do
          {
            fputc((char ) *from, fp);
            fputc((char) *(from + 2), fp);
            fputc((char) *(from + 4), fp);
            ++pixel;
            if ( pixel < pixels )
              {
                fputc((char) *(from + 1), fp);
                fputc((char) *(from + 3), fp);
                fputc((char) *(from + 5), fp);
                ++pixel;
              }
            from += 6;
          } while ( pixel < pixels );
      }
    else
      {
        DBG(1, "wordchunky_copy_pixels: Unknown depth %d\n", depth);
        return SANE_STATUS_IO_ERROR;
      }

    return SANE_STATUS_GOOD;
}


/*---------- wordchunky_proc_data() ------------------------------------------*/

static SANE_Status                
wordchunky_proc_data(Microtek2_Scanner *ms)
{
    SANE_Int status;
    u_int8_t *from;
    u_int32_t line;


    DBG(30, "wordchunky_proc_data: ms=%p\n", ms);
             
    from = ms->buf.src_buf;
    for ( line = 0; line < ms->src_lines_to_read; line++ )
      {
        status = wordchunky_copy_pixels(from, ms->ppl, ms->depth, ms->fp);
        if ( status != SANE_STATUS_GOOD )
            return status;
        from += ms->bpl;
      }

    return SANE_STATUS_GOOD;
}


/*---------- scsi_wait_for_image() -------------------------------------------*/

static SANE_Status
scsi_wait_for_image(Microtek2_Scanner *ms)
{
    int retry = 60;
    SANE_Int status;


    DBG(30, "scsi_wait_for_image: ms=%p\n", ms);

    while ( retry-- > 0 ) 
      {
        status = scsi_read_image_status(ms);
        if  (status == SANE_STATUS_DEVICE_BUSY ) 
          { 
            sleep(1);
            continue;
          }
        if ( status == SANE_STATUS_GOOD )
            return status;

        /* status != GOOD && != BUSY */
        DBG(1, "scsi_wait_for_image: '%s'\n", sane_strstatus(status));
        return status;
      }
    
    /* BUSY after n retries */   
    DBG(1, "scsi_wait_for_image: '%s'\n", sane_strstatus(status));
    return status;
}


/*---------- scsi_read_gamma() -----------------------------------------------*/

/* currently not used */
/*
static SANE_Status
scsi_read_gamma(Microtek2_Scanner *ms, int color)
{
    u_int8_t readgamma[RG_CMD_L];
    u_int8_t result[3072];
    size_t size;
    SANE_Bool endiantype;
    SANE_Status status;

    RG_CMD(readgamma);
    ENDIAN_TYPE(endiantype);   
    RG_PCORMAC(readgamma, endiantype);
    RG_COLOR(readgamma, color);
    RG_WORD(readgamma, ( ms->dev->lut_entry_size == 1 ) ? 0 : 1);
    RG_TRANSFERLENGTH(readgamma, (color == 3 ) ? 3072 : 1024);

    dump_area(readgamma, 10, "ReadGamma");

    size = sizeof(result);
    status = sanei_scsi_cmd(ms->sfd, readgamma, sizeof(readgamma), 
                            result, &size);
    if ( status != SANE_STATUS_GOOD ) {
        DBG(1, "scsi_read_gamma: (L,R) read_gamma failed: status '%s'\n",
                sane_strstatus(status));
        return status;
    }

    dump_area(result, 3072, "Result");

    return SANE_STATUS_GOOD;
}
*/


/*---------- scsi_send_gamma() -----------------------------------------------*/

static SANE_Status
scsi_send_gamma(Microtek2_Scanner *ms)
{
    Microtek2_Device *md;
    Microtek2_Info *mi;
    SANE_Bool endiantype;
    SANE_Status status;
    size_t size;
    u_int8_t *cmd;

    DBG(30, "scsi_send_gamma: pos=%p, size=%d, word=%d, color=%d\n", 
             ms->gamma_table, ms->lut_size_bytes, ms->word, ms->current_color);

    md = ms->dev;
    mi = &md->info[MD_SOURCE_FLATBED];

    cmd = (u_int8_t *) alloca(SG_CMD_L + ms->lut_size_bytes);
    if ( cmd == NULL )
      {
        DBG(1, "scsi_send_gamma: Couldn't get buffer for gamma table\n");
        return SANE_STATUS_IO_ERROR;
      }
    
    SG_SET_CMD(cmd);
    ENDIAN_TYPE(endiantype)
    SG_SET_PCORMAC(cmd, endiantype);
    SG_SET_COLOR(cmd, ms->current_color);
    SG_SET_WORD(cmd, ms->word);
    SG_SET_TRANSFERLENGTH(cmd, ms->lut_size_bytes);
    memcpy(cmd + SG_CMD_L, ms->gamma_table, ms->lut_size_bytes);
    size = ms->lut_size_bytes;

    if ( ms_dump >= 2 )
        dump_area2(cmd, SG_CMD_L, "sendgammacmd");
    if ( ms_dump >= 3 )
        dump_area2(cmd + SG_CMD_L, size, "sendgammadata");

    status = sanei_scsi_cmd(ms->sfd, cmd, size + SG_CMD_L, NULL, 0);
    if ( status != SANE_STATUS_GOOD ) 
        DBG(1, "scsi_send_gamma: '%s'\n", sane_strstatus(status));

    return status;
}


/*---------- scsi_inquiry() --------------------------------------------------*/

static SANE_Status
scsi_inquiry(Microtek2_Info *mi, char *device)
{
    u_int8_t cmd[INQ_CMD_L];
    u_int8_t *result;
    u_int8_t inqlen;
    size_t size;
    int sfd;
    int status;


    DBG(30, "scsi_inquiry: mi=%p, device='%s'\n", mi, device);
    
    status = sanei_scsi_open(device, &sfd, scsi_sense_handler, 0);
    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "scsi_inquiry: '%s'\n", sane_strstatus(status));
        return status;
      }

    INQ_CMD(cmd);
    INQ_SET_ALLOC(cmd, INQ_ALLOC_L);
    result = (u_int8_t *) alloca(INQ_ALLOC_L);
    if ( result == NULL )
      {
        DBG(1, "scsi_inquiry: malloc failed\n");
        sanei_scsi_close(sfd);
        return SANE_STATUS_NO_MEM;
      }

    size = INQ_ALLOC_L;
    status = sanei_scsi_cmd(sfd, cmd, sizeof(cmd), result, &size);
    if ( status != SANE_STATUS_GOOD ) 
      {
        DBG(1, "scsi_inquiry: '%s'\n", sane_strstatus(status));
        sanei_scsi_close(sfd);
        return status;
      }

    INQ_GET_INQLEN(inqlen, result);
    INQ_SET_ALLOC(cmd, inqlen + INQ_ALLOC_L);
    result = alloca(inqlen + INQ_ALLOC_L);
    if ( result == NULL ) 
      {
        DBG(1, "scsi_inquiry: malloc failed\n");
        sanei_scsi_close(sfd);
        return SANE_STATUS_NO_MEM;
      }
    size = inqlen + INQ_ALLOC_L;   
    if (ms_dump >= 2 )
        dump_area2(cmd, sizeof(cmd), "inquiry");

    status = sanei_scsi_cmd(sfd, cmd, sizeof(cmd), result, &size);
    if ( status != SANE_STATUS_GOOD ) 
      {
        DBG(1, "scsi_inquiry: cmd '%s'\n", sane_strstatus(status));
        sanei_scsi_close(sfd);
        return status;
      }
    sanei_scsi_close(sfd); 

    if (ms_dump >= 2 )
      {
        dump_area2((u_int8_t *) result, size, "inquiryresult");
        dump_area((u_int8_t *) result, size, "inquiryresult");
      }

    /* copy results */
    INQ_GET_QUAL(mi->device_qualifier, result);
    INQ_GET_DEVT(mi->device_type, result);
    INQ_GET_VERSION(mi->scsi_version, result);
    INQ_GET_VENDOR(mi->vendor, result);
    INQ_GET_MODEL(mi->model, result);
    INQ_GET_REV(mi->revision, result);
    INQ_GET_MODELCODE(mi->model_code, result);


    return SANE_STATUS_GOOD;
}


/*---------- scsi_read_attributes() ------------------------------------------*/

static SANE_Status
scsi_read_attributes(Microtek2_Info *pmi, char *device, u_int8_t scan_source)
{
    Microtek2_Info *mi;
    u_int8_t readattributes[RSA_CMD_L];
    u_int8_t result[RSA_TRANSFERLENGTH];
    size_t size;
    int sfd;
    int status;
    

    mi = &pmi[scan_source];

    DBG(30, "scsi_read_attributes: mi=%p, device='%s', source=%d\n",
             mi, device, scan_source);

    RSA_CMD(readattributes); 
    RSA_SETMEDIA(readattributes, scan_source);
    status = sanei_scsi_open(device, &sfd, scsi_sense_handler, 0);
    if ( status != SANE_STATUS_GOOD ) 
      {
        DBG(1, "scsi_read_attributes: open '%s'\n", sane_strstatus(status));
        return status;
      }

    if (ms_dump >= 2 )
        dump_area2(readattributes, sizeof(readattributes), "scannerattributes");
                   
    size = sizeof(result);   
    status = sanei_scsi_cmd(sfd, readattributes,
                            sizeof(readattributes), result, &size);
    if ( status != SANE_STATUS_GOOD ) 
      {
        DBG(1, "scsi_read_attributes: cmd '%s'\n", sane_strstatus(status));
        sanei_scsi_close(sfd);
        return status;
      }

    sanei_scsi_close(sfd); 

    /* The V6USL lies about its capabilities to send gamma tables to */
    /* the scanner, so fake a 4096 word table. */
    if ( mi->model_code == 0xa3 )
        result[16] |= 0x10;

    /* copy all the stuff into the info structure */
    RSA_COLOR(mi->color, result);
    RSA_ONEPASS(mi->onepass, result);
    RSA_SCANNERTYPE(mi->scanner_type, result);
    RSA_FEPROM(mi->feprom, result);
    RSA_DATAFORMAT(mi->data_format, result);
    RSA_COLORSEQUENCE(mi->color_sequence, result);
    RSA_CCDGAP(mi->ccd_gap, result);
    RSA_MAX_XRESOLUTION(mi->max_xresolution, result);
    RSA_MAX_YRESOLUTION(mi->max_yresolution, result);
    RSA_GEOWIDTH(mi->geo_width, result);
    RSA_GEOHEIGHT(mi->geo_height, result);
    RSA_OPTRESOLUTION(mi->opt_resolution, result);
    RSA_DEPTH(mi->depth, result);
    RSA_SCANMODE(mi->scanmode, result);
    RSA_CCDPIXELS(mi->ccd_pixels, result);
    RSA_LUTCAP(mi->lut_cap, result);
    RSA_DNLDPTRN(mi->has_dnldptrn, result);
    RSA_GRAINSLCT(mi->grain_slct, result);
    RSA_SUPPOPT(mi->option_device, result);
    RSA_CALIBWHITE(mi->calib_white, result);
    RSA_CALIBSPACE(mi->calib_space, result);
    RSA_NLENS(mi->nlens, result);
    RSA_NWINDOWS(mi->nwindows, result);
    RSA_SHTRNSFEREQU(mi->shtrnsferequ, result);
    RSA_SCNBTTN(mi->scnbuttn, result);
    RSA_BUFTYPE(mi->buftype, result);

    if (ms_dump >= 2 )
        dump_area2((u_int8_t *) result, sizeof(result), 
                   "scannerattributesresults");
    if ( ms_dump >= 1 && ms_dump_clear )
        dump_attributes(mi);
    
    return SANE_STATUS_GOOD;
}


/*---------- scsi_set_window() -----------------------------------------------*/

static SANE_Status
scsi_set_window(Microtek2_Scanner *ms, int n) {   /* n windows, not yet */
                                                  /* implemented */
    u_int8_t *setwindow;
    int size;
    SANE_Int status;


    DBG(30, "scsi_set_window: ms=%p, wnd=%d\n", ms, n);

    size = SW_CMD_L + SW_HEADER_L + n * SW_BODY_L;
    setwindow = (u_int8_t *) malloc(size);
    if ( setwindow == NULL )
      {
        DBG(1, "scsi_set_window: malloc for setwindow failed\n");
        return SANE_STATUS_NO_MEM;
      }
    memset(setwindow, 0, size);
 
    SW_CMD(setwindow);
    SW_PARAM_LENGTH(setwindow, SW_HEADER_L + n * SW_BODY_L);
    SW_WNDDESCLEN(setwindow + SW_HEADER_P, SW_WNDDESCVAL);

#define POS  (setwindow + SW_BODY_P(n-1))

    SW_WNDID(POS, n-1);
    SW_XRESDPI(POS, ms->x_resolution_dpi);
    SW_YRESDPI(POS, ms->y_resolution_dpi);
    SW_XPOSTL(POS, ms->x1_dots);
    SW_YPOSTL(POS, ms->y1_dots);
    SW_WNDWIDTH(POS, ms->width_dots);
    SW_WNDHEIGHT(POS, ms->height_dots);
    SW_THRESHOLD(POS, ms->threshold);
    SW_IMGCOMP(POS, ms->mode);
    SW_BITSPERPIXEL(POS, ms->depth);
    SW_EXTHT(POS, ms->use_external_ht);
    SW_INTHTINDEX(POS, ms->internal_ht_index);
    SW_RIF(POS, 1); 
    SW_LENS(POS, 0);                                  /* ???? */
    SW_INFINITE(POS, 0);
    SW_STAY(POS, 0);
    SW_RAWDAT(POS, ms->rawdat);
    SW_QUALITY(POS, ms->quality);
    SW_FASTSCAN(POS, ms->fastscan);
    SW_MEDIA(POS, ms->scan_source);
    SW_BRIGHTNESS_M(POS, ms->brightness_m);
    SW_CONTRAST_M(POS, ms->contrast_m);
    SW_EXPOSURE_M(POS, ms->exposure_m);
    SW_SHADOW_M(POS, ms->shadow_m);
    SW_MIDTONE_M(POS, ms->midtone_m);
    SW_HIGHLIGHT_M(POS, ms->highlight_m);
    /* the following properties are only referenced if it's a color scan */
    /* but I guess they don't matter at a gray scan */
    SW_BRIGHTNESS_R(POS, ms->brightness_r);
    SW_CONTRAST_R(POS, ms->contrast_r);
    SW_EXPOSURE_R(POS, ms->exposure_r);
    SW_SHADOW_R(POS, ms->shadow_r);
    SW_MIDTONE_R(POS, ms->midtone_r);
    SW_HIGHLIGHT_R(POS, ms->highlight_r);
    SW_BRIGHTNESS_G(POS, ms->brightness_g);
    SW_CONTRAST_G(POS, ms->contrast_g);
    SW_EXPOSURE_G(POS, ms->exposure_g);
    SW_SHADOW_G(POS, ms->shadow_g);
    SW_MIDTONE_G(POS, ms->midtone_g);
    SW_HIGHLIGHT_G(POS, ms->highlight_g);
    SW_BRIGHTNESS_B(POS, ms->brightness_b);
    SW_CONTRAST_B(POS, ms->contrast_b);
    SW_EXPOSURE_B(POS, ms->exposure_b);
    SW_SHADOW_B(POS, ms->shadow_b);
    SW_MIDTONE_B(POS, ms->midtone_b);
    SW_HIGHLIGHT_B(POS, ms->highlight_b);

    if ( ms_dump >= 2 )
      {
        dump_area2(setwindow, 10, "setwindowcmd");
        dump_area2(setwindow + 10 ,8 , "setwindowheader");
        dump_area2(setwindow + 18 ,61 , "setwindowbody");
      }

    status = sanei_scsi_cmd(ms->sfd, setwindow, size, NULL, 0);
    if ( status != SANE_STATUS_GOOD ) 
        DBG(1, "scsi_set_window: '%s'\n", sane_strstatus(status));
               
    free((void *) setwindow);
    return status;
}


/*---------- scsi_read_image_info() ------------------------------------------*/

static SANE_Status
scsi_read_image_info(Microtek2_Scanner *ms)
{
    u_int8_t cmd[RII_CMD_L];
    u_int8_t result[RII_RESULT_L];
    size_t size;
    SANE_Status status;
    Microtek2_Device *md;
    Microtek2_Info *mi;

    md = ms->dev;
    mi = &md->info[MD_SOURCE_FLATBED];

    DBG(30, "scsi_read_image_info: ms=%p\n", ms);
    
    RII_SET_CMD(cmd);

    if ( ms_dump >= 2)
        dump_area2(cmd, RII_CMD_L, "readimageinfo");
    
    size = sizeof(result);
    status = sanei_scsi_cmd(ms->sfd, cmd, sizeof(cmd), result, &size);
    if ( status != SANE_STATUS_GOOD ) 
      {
        DBG(1, "scsi_read_image_info: '%s'\n", sane_strstatus(status));
        return status;
      }

    if ( ms_dump >= 2)
        dump_area2(result, size, "readimageinforesult");

    /* The V300 returns some values in only two bytes */
    if ( mi->model_code != 0x85 )
      {
        RII_GET_WIDTHPIXEL(ms->ppl, result);
        RII_GET_WIDTHBYTES(ms->bpl, result);
        RII_GET_HEIGHTLINES(ms->src_remaining_lines, result);
        RII_GET_REMAINBYTES(ms->remaining_bytes, result);
      }
    else
      {
        RII_GET_V300_WIDTHPIXEL(ms->ppl, result);
        RII_GET_V300_WIDTHBYTES(ms->bpl, result);
        RII_GET_V300_HEIGHTLINES(ms->src_remaining_lines, result);
        RII_GET_V300_REMAINBYTES(ms->remaining_bytes, result);
      }

    DBG(30, "scsi_read_image_info: ppl=%d, bpl=%d, lines=%d, remain=%d\n",
             ms->ppl, ms->bpl, ms->src_remaining_lines, ms->remaining_bytes);

    return SANE_STATUS_GOOD;
}


/*---------- scsi_read_image() -----------------------------------------------*/

static SANE_Status
scsi_read_image(Microtek2_Scanner *ms)
{
    u_int8_t cmd[RI_CMD_L];
    SANE_Bool endiantype;
    SANE_Status status;
    size_t size;


    DBG(30, "scsi_read_image:  ms=%p\n", ms);

    ENDIAN_TYPE(endiantype)
    RI_SET_CMD(cmd);
    RI_SET_PCORMAC(cmd, endiantype);
    RI_SET_COLOR(cmd, ms->current_color);
    RI_SET_TRANSFERLENGTH(cmd, ms->transfer_length);

    DBG(30, "scsi_read_image: transferlength=%d\n", ms->transfer_length);
   
    if ( ms_dump >= 2 ) 
        dump_area2(cmd, RI_CMD_L, "readimagecmd");
    
    size = ms->src_buffer_size;
    status = sanei_scsi_cmd(ms->sfd, cmd, sizeof(cmd), ms->buf.src_buf, &size);
                         /* ms->buf.src_buffer[ms->buf.current_src], &size);*/
                            
    if ( status != SANE_STATUS_GOOD ) 
        DBG(1, "scsi_read_image: '%s'\n", sane_strstatus(status));
   
    if ( ms_dump > 3 ) 
        dump_area2(ms->buf.src_buf, ms->transfer_length, "readimageresult");
                
    return status;
}


/*---------- scsi_read_image_status() ----------------------------------------*/

static SANE_Status
scsi_read_image_status(Microtek2_Scanner *ms)
{
    u_int8_t cmd[RIS_CMD_L];
    SANE_Status status;
    SANE_Bool endian_type;
    
    DBG(30, "scsi_read_image_status: ms=%p\n", ms);
   
    ENDIAN_TYPE(endian_type)
    RIS_SET_CMD(cmd);
    RIS_SET_PCORMAC(cmd, endian_type);
    RIS_SET_COLOR(cmd, ms->current_color);

    if ( ms_dump >= 2 )
        dump_area2(cmd, sizeof(cmd), "readimagestatus");

    status = sanei_scsi_cmd(ms->sfd, cmd, sizeof(cmd), 0, NULL);
    if ( status != SANE_STATUS_GOOD ) 
        DBG(1, "scsi_read_image_status: '%s'\n", sane_strstatus(status));
   
    return status;
}

/*---------- scsi_read_system_status() ---------------------------------------*/

static SANE_Status
scsi_read_system_status(Microtek2_Device *md, int fd)
{
    u_int8_t cmd[RSS_CMD_L];
    u_int8_t result[RSS_RESULT_L];
    int sfd;
    size_t size;
    SANE_Status status;

    DBG(30, "scsi_read_system_status: md=%p, fd=%d\n", md, fd);
   
    if ( fd == -1 )
      { 
        status = sanei_scsi_open(md->name, &sfd, scsi_sense_handler, 0);
        if ( status != SANE_STATUS_GOOD ) 
          {
            DBG(1, "scsi_read_system_status: open '%s'\n",
                    sane_strstatus(status));
            return status;
          }
      }
    else
      sfd = fd;

    RSS_CMD(cmd);

    if ( ms_dump >= 2)
        dump_area2(cmd, RSS_CMD_L, "readsystemstatus");
    
    size = sizeof(result);
    status = sanei_scsi_cmd(sfd, cmd, sizeof(cmd), result, &size);

    if ( status != SANE_STATUS_GOOD ) 
      {
        DBG(1, "scsi_read_system_status: cmd '%s'\n", sane_strstatus(status));
        sanei_scsi_close(sfd);
        return status;
      }

    if ( fd == -1 )
        sanei_scsi_close(sfd);

    if ( ms_dump >= 2)
        dump_area2(result, size, "readsystemstatusresult");

    md->status.ntrack = RSS_NTRACK(result);
    md->status.ncalib = RSS_NCALIB(result);
    md->status.tlamp = RSS_TLAMP(result);
    md->status.flamp = RSS_FLAMP(result);
    md->status.rdyman= RSS_RDYMAN(result);
    md->status.trdy = RSS_TRDY(result);
    md->status.frdy = RSS_FRDY(result);
    md->status.adp = RSS_RDYMAN(result);
    md->status.detect = RSS_DETECT(result);
    md->status.adptime = RSS_ADPTIME(result);
    md->status.lensstatus = RSS_LENSSTATUS(result);
    md->status.aloff = RSS_ALOFF(result);
    md->status.timeremain = RSS_TIMEREMAIN(result);
    md->status.tmacnt = RSS_TMACNT(result);
    md->status.paper = RSS_PAPER(result);
    md->status.adfcnt = RSS_ADFCNT(result);
    md->status.currentmode = RSS_CURRENTMODE(result);
    md->status.buttoncount = RSS_BUTTONCOUNT(result);
    
    return SANE_STATUS_GOOD;
}


/*---------- scsi_request_sense() --------------------------------------------*/

/* currently not used */
/* static SANE_Status
scsi_request_sense(Microtek2_Scanner *ms)
{
    u_int8_t requestsense[RQS_CMD_L];
    u_int8_t buffer[100];
    SANE_Status status;
    int size;
    int asl;
    int as_info_length;

    DBG(30, "scsi_request_sense: ms=%p\n", ms);
    
    RQS_CMD(requestsense);
    RQS_ALLOCLENGTH(requestsense, 100);

    size = sizeof(buffer);
    status = sanei_scsi_cmd(ms->sfd,  requestsense, sizeof(requestsense), 
                            buffer, &size);
    
    if ( status != SANE_STATUS_GOOD ) 
      {
        DBG(1, "scsi_request_sense: '%s'\n", sane_strstatus(status));
        return status;
      }
           
    if ( ms_dump >= 2 )
        dump_area2(buffer, size, "requestsenseresult");

    dump_area(buffer, RQS_LENGTH(buffer), "RequestSense");
    asl = RQS_ASL(buffer);
    if ( (as_info_length = RQS_ASINFOLENGTH(buffer)) > 0 )
        DBG(25, "scsi_request_sense: info '%.*s'\n",
                as_info_length, RQS_ASINFO(buffer));
    
    return SANE_STATUS_GOOD;
}
*/


/*---------- scsi_send_system_status() ---------------------------------------*/

static SANE_Status
scsi_send_system_status(Microtek2_Device *md, int fd)
{
    u_int8_t cmd[SSS_CMD_L + SSS_DATA_L];
    u_int8_t *pos;
    int sfd;
    SANE_Status status;


    DBG(30, "scsi_send_system_status: md=%p, fd=%d\n", md, fd);

    memset(cmd, 0, SSS_CMD_L + SSS_DATA_L);
    if ( fd == -1 )
      {
        status = sanei_scsi_open(md->name, &sfd, scsi_sense_handler, 0);
        if ( status != SANE_STATUS_GOOD ) 
          {
            DBG(1, "scsi_send_system_status: open '%s'\n",
                    sane_strstatus(status));
            return status;
          }
      }
    else
      sfd = fd;
   
    SSS_CMD(cmd);
    pos = cmd + SSS_CMD_L;
    SSS_NTRACK(pos, md->status.ntrack);
    SSS_NCALIB(pos, md->status.ncalib);
    SSS_TLAMP(pos, md->status.tlamp);
    SSS_FLAMP(pos, md->status.flamp);
    SSS_RDYMAN(pos, md->status.rdyman);
    SSS_TRDY(pos, md->status.trdy);
    SSS_FRDY(pos, md->status.frdy);
    SSS_ADP(pos, md->status.adp);
    SSS_DETECT(pos, md->status.detect);
    SSS_ADPTIME(pos, md->status.adptime);
    SSS_LENSSTATUS(pos, md->status.lensstatus);
    SSS_ALOFF(pos, md->status.aloff);
    SSS_TIMEREMAIN(pos, md->status.timeremain);
    SSS_TMACNT(pos, md->status.tmacnt);
    SSS_PAPER(pos, md->status.paper);
    SSS_ADFCNT(pos, md->status.adfcnt);
    SSS_CURRENTMODE(pos, md->status.currentmode);
    SSS_BUTTONCOUNT(pos, md->status.buttoncount);

    if ( ms_dump >= 2)
        dump_area2(cmd, SSS_CMD_L + SSS_DATA_L, "sendsystemstatus");
    
    status = sanei_scsi_cmd(sfd, cmd, sizeof(cmd), NULL, 0);
    if ( status != SANE_STATUS_GOOD ) 
        DBG(1, "scsi_send_system_status: '%s'\n", sane_strstatus(status));

    if ( fd == -1 )
        sanei_scsi_close(sfd); 
    return status;
}


/*---------- scsi_sense_handler() --------------------------------------------*/

static SANE_Status
scsi_sense_handler (int fd, u_char *sense, void *arg)
{
    int as_info_length;
    u_int8_t sense_key;
    u_int8_t asl;
    u_int8_t asc;
    u_int8_t ascq;
    
    
    DBG(30, "scsi_sense_handler: fd=%d, sense=%p arg=%p\n",fd, sense, arg);
           
    dump_area(sense, RQS_LENGTH(sense), "SenseBuffer");

    sense_key = RQS_SENSEKEY(sense);
    asl = RQS_ASL(sense);
    asc = RQS_ASC(sense);
    ascq = RQS_ASCQ(sense);
    
    if ( (as_info_length = RQS_ASINFOLENGTH(sense)) > 0 )
        DBG(30,"scsi_sense_handler: info: '%*s'\n",
                as_info_length, RQS_ASINFO(sense));
        
    switch ( sense_key ) 
      {
        case RQS_SENSEKEY_NOSENSE:
          return SANE_STATUS_GOOD;

        case RQS_SENSEKEY_HWERR:
          if ( asc == 0x4a && ascq == 0x00 )
              DBG(5, "scsi_sense_handler: Command phase error\n");
          else if ( asc == 0x4b && ascq == 0x00 )
              DBG(5, "scsi_sense_handler: Data phase error\n");
          else if ( asc == 0x40 ) 
            {
              switch ( ascq ) 
                {
                  case RQS_ASCQ_CPUERR:
                    DBG(5, "scsi_sense_handler: CPU error\n");
                    break;
                  case RQS_ASCQ_SRAMERR:
                    DBG(5, "scsi_sense_handler: SRAM error\n");
                    break;
                  case RQS_ASCQ_DRAMERR:
                    DBG(5, "scsi_sense_handler: DRAM error\n");
                    break;
                  case RQS_ASCQ_DCOFF:
                    DBG(5, "scsi_sense_handler: DC Offset error\n");
                    break;
                  case RQS_ASCQ_GAIN:
                    DBG(5, "scsi_sense_handler: Gain error\n");
                    break;
                  case RQS_ASCQ_POS:
                    DBG(5, "scsi_sense_handler: Pos. error\n");
                    break;
                  default:
                    DBG(5, "scsi_sense_handler: Unknown combination of ASC"
                           " (0x%02x) and ASCQ (0x%02x)\n", asc, ascq);
                    break;
                }
            }   
          else if ( asc == 0x00  && ascq == 0x05) 
              DBG(5, "scsi_sense_handler: End of data detected\n");
          else if ( asc == 0x60 && ascq == 0x00 ) 
              DBG(5, "scsi_sense_handler: Lamp failure\n");
          else if ( asc == 0x53 && ascq == 0x00 ) 
            {
              DBG(5, "scsi_sense_handler: ADF paper jam or no paper\n");
              return SANE_STATUS_NO_DOCS;     /* not STATUS_INVAL */
            }
          else if ( asc == 0x3a && ascq == 0x00 )
              DBG(5, "scsi_sense_handler: Media (ADF or TMA) not available\n");
          else if ( asc == 0x03 && ascq == 0x00 ) 
              DBG(5, "scsi_sense_handler: Peripheral device write fault\n");
          else if ( asc == 0x80 && ascq == 0x00 ) 
              DBG(5, "scsi_sense_handler: Target abort scan\n");
          else
              DBG(5, "scsi_sense_handler: Unknown combination of SENSE KEY "
                     "(0x%02x), ASC (0x%02x) and ASCQ (0x%02x)\n", 
                      sense_key, asc, ascq);

          return SANE_STATUS_IO_ERROR;   

        case RQS_SENSEKEY_ILLEGAL:
          if ( asc == 0x2c && ascq == 0x00 )
              DBG(5, "scsi_sense_handler: Command sequence error\n");
          else if ( asc == 0x3d  && ascq == 0x00) 
              DBG(5, "scsi_sense_handler: Invalid bit in IDENTIFY\n");
          else if ( asc == 0x2c && ascq == 0x02 ) 
/* Ok */      DBG(5, "scsi_sense_handler: Invalid comb. of windows specfied\n");
          else if ( asc == 0x20 && ascq == 0x00 ) 
/* Ok */      DBG(5, "scsi_sense_handler: Invalid command opcode\n");
          else if ( asc == 0x24 && ascq == 0x00 ) 
/* Ok */      DBG(5, "scsi_sense_handler: Invalid field in CDB\n");
          else if ( asc == 0x26 && ascq == 0x00 )
              DBG(5, "scsi_sense_handler: Invalid field in the param list\n");
          else if ( asc == 0x49 && ascq == 0x00 ) 
              DBG(5, "scsi_sense_handler: Invalid message error\n");
          else if ( asc == 0x25 && ascq == 0x00 ) 
              DBG(5, "scsi_sense_handler: Unsupported logic. unit\n");
          else if ( asc == 0x00 && ascq == 0x00 ) 
              DBG(5, "scsi_sense_handler:  No additional sense information\n");
/* Ok */  else if ( asc == 0x1a && ascq == 0x00 ) 
              DBG(5, "scsi_sense_handler: Parameter list length error\n");
          else if ( asc == 0x26 && ascq == 0x02 ) 
              DBG(5, "scsi_sense_handler: Parameter value invalid\n");
          else if ( asc == 0x2c && ascq == 0x01 ) 
              DBG(5, "scsi_sense_handler: Too many windows\n");
          else
              DBG(5, "scsi_sense_handler: Unknown combination of SENSE KEY "
                     "(0x%02x), ASC (0x%02x) and ASCQ (0x%02x)\n", 
                      sense_key, asc, ascq);
             
            return SANE_STATUS_IO_ERROR;   

        case RQS_SENSEKEY_VENDOR:
          DBG(5, "scsi_sense_handler: Vendor specific SENSE KEY (0x%02x), "
                 "ASC (0x%02x) and ASCQ (0x%02x)\n", sense_key, asc, ascq);
             
          return SANE_STATUS_IO_ERROR;   

        default:
           DBG(5, "scsi_sense_handler: Unknown sense key (0x%02x)\n",
                   sense_key);
           return SANE_STATUS_IO_ERROR;
    }

    return SANE_STATUS_GOOD;
}


/*---------- scsi_test_unit_ready() ------------------------------------------*/

static SANE_Status
scsi_test_unit_ready(Microtek2_Device *md)
{
    u_int8_t tur[TUR_CMD_L];
    int sfd;
    int status;


    DBG(30, "scsi_test_unit_ready: md=%s\n", md->name);
   
    TUR_CMD(tur); 
    status = sanei_scsi_open(md->name, &sfd, scsi_sense_handler, 0);
    if ( status != SANE_STATUS_GOOD )
      {
	DBG(1, "scsi_test_unit_ready: open '%s'\n", sane_strstatus(status));
	return status;
      }

    if ( ms_dump >= 2 )
        dump_area2(tur, sizeof(tur), "testunitready");

    status = sanei_scsi_cmd(sfd, tur, sizeof(tur), NULL, 0);
    if ( status != SANE_STATUS_GOOD )
        DBG(1, "scsi_test_unit_ready: cmd '%s'\n", sane_strstatus(status));
      
    sanei_scsi_close(sfd); 
    return status;
}
