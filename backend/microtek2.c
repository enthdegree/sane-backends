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

   This file implements a SANE backend for Microtek scanners with
   SCSI-2 command set.

   (feedback to:  bernd@aquila.muc.de)

 ***************************************************************************/


#ifdef _AIX
# include "../include/lalloca.h"   /* MUST come first for AIX! */
#endif

#include "../include/sane/config.h"
#include "../include/lalloca.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#include <math.h>

#ifdef HAVE_AUTHORIZATION
#include <sys/stat.h>
#endif

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_scsi.h"
#include "../include/sane/saneopts.h"

#define BACKEND_NAME microtek2
#include "../include/sane/sanei_backend.h"

#include "microtek2.h"



#ifdef HAVE_AUTHORIZATION
static SANE_Auth_Callback auth_callback;
#endif

static int md_num_devices = 0;          /* number of devices from config file */
static Microtek2_Device *md_first_dev = NULL;        /* list of known devices */
static Microtek2_Scanner *ms_first_handle = NULL;    /* list of open scanners */

/* options that can be configured in the config file */
static Config_Options md_options = { 1.0, "off", "off", "off", "off", "off"};
static Config_Temp *md_config_temp = NULL;
static int md_dump = 0;                 /* from config file: */
                                        /* 1: inquiry + scanner attributes */
                                        /* 2: + all scsi commands and data */
                                        /* 3: + all scan data */
static int md_dump_clear = 1;


/*---------- sane_cancel() ---------------------------------------------------*/

void
sane_cancel (SANE_Handle handle)
{
    Microtek2_Scanner *ms = handle;

    DBG(30, "sane_cancel: handle=%p\n", handle);

    if ( ms->scanning == SANE_TRUE )
      {
        cancel_scan(ms);
        cleanup_scanner(ms);
      }
    ms->cancelled = SANE_TRUE;
    ms->fd[0] = ms->fd[1] = -1;
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

    free((void *) ms);
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


    md = ms->dev;
    val = &ms->val[0];
    sod = &ms->sod[0];

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
              case OPT_BITDEPTH:
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
              case OPT_CALIB_BACKEND:
              case OPT_LIGHTLID35:
              case OPT_GAMMA_BIND:
              case OPT_AUTOADJUST:
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

              /* button options */
              case OPT_TOGGLELAMP:
                return SANE_STATUS_GOOD;

              /* others */
              case OPT_NUM_OPTS:
                *(SANE_Word *) value = NUM_OPTIONS;
                return SANE_STATUS_GOOD;

              default:
                return SANE_STATUS_UNSUPPORTED;
            }
          /* NOTREACHED */
          /* break; */

        case SANE_ACTION_SET_VALUE:
          if ( ! SANE_OPTION_IS_SETTABLE(sod[option].cap) )
            {
              DBG(10, "sane_control_option: trying to set unsettable option\n");
              return SANE_STATUS_INVAL;
            }

          /* do not check OPT_BR_Y, xscanimage sometimes tries to set */
          /* it to a too large value; bug in xscanimage ? */
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
              case SANE_TYPE_BUTTON:
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
                    return SANE_STATUS_GOOD;
              case OPT_DISABLE_BACKTRACK:
              case OPT_CALIB_BACKEND:
              case OPT_LIGHTLID35:
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

              case OPT_BITDEPTH:
                /* If the bitdepth has changed we must change the size of */
                /* the gamma table if the device does not support gamma */
                /* tables. This will hopefully cause no trouble if the */
                /* mode is one bit */

                if ( md->model_flags & MD_NO_GAMMA )
                  {
                    int max_gamma_value;
                    int size;
                    int color;
                    int i;

                    size = (int) pow(2.0, (double) val[OPT_BITDEPTH].w) - 1;
                    max_gamma_value = size - 1;
                    for ( color = 0; color < 4; color++ )
                      {
                        for ( i = 0; i < max_gamma_value; i++ )
                            md->custom_gamma_table[color][i] = (SANE_Int) i;
                      }
                    md->custom_gamma_range.max = (SANE_Int) max_gamma_value;
                    sod[OPT_GAMMA_CUSTOM].size = size * sizeof (SANE_Int);
                    sod[OPT_GAMMA_CUSTOM_R].size = size * sizeof (SANE_Int);
                    sod[OPT_GAMMA_CUSTOM_G].size = size * sizeof (SANE_Int);
                    sod[OPT_GAMMA_CUSTOM_B].size = size * sizeof (SANE_Int);

                    if ( info )
                        *info |= SANE_INFO_RELOAD_OPTIONS;

                  }

                if ( info )
                    *info |= SANE_INFO_RELOAD_PARAMS;
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

                /* Options with side effects need special treatment. They are */
                /* reset, even if they were set by set_option_dependencies(): */
                /* if we have more than one color depth activate this option */

                if ( md->bitdepth_list[0] == 1 )
                    sod[OPT_BITDEPTH].cap |= SANE_CAP_INACTIVE;
                if ( strncmp(md->opts.auto_adjust, "off", 3) == 0 )
                    sod[OPT_AUTOADJUST].cap |= SANE_CAP_INACTIVE;

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

              case OPT_TOGGLELAMP:
                status = scsi_read_system_status(md, -1);
                if ( status != SANE_STATUS_GOOD )
                    return SANE_STATUS_IO_ERROR;

                md->status.flamp ^= 1;
                status = scsi_send_system_status(md, -1);
                if ( status != SANE_STATUS_GOOD )
                    return SANE_STATUS_IO_ERROR;
                return SANE_STATUS_GOOD;

              case OPT_AUTOADJUST:
                if ( info )
                    *info |= SANE_INFO_RELOAD_OPTIONS;

                if ( ms->val[option].w == SANE_FALSE )
                    ms->sod[OPT_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;
                else
                    ms->sod[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;

                return SANE_STATUS_GOOD;

              default:
                return SANE_STATUS_UNSUPPORTED;
            }
#if 0
          break;
#endif
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

        if ( md_first_dev->shading_table_w )
          {
            free((void *) md_first_dev->shading_table_w);
            md_first_dev->shading_table_w = NULL;
          }

        if ( md_first_dev->shading_table_d )
          {
            free((void *) md_first_dev->shading_table_d);
            md_first_dev->shading_table_d = NULL;
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
    return SANE_STATUS_GOOD;
}


/*---------- sane_get_option_descriptor() ------------------------------------*/

const SANE_Option_Descriptor *
sane_get_option_descriptor(SANE_Handle handle, SANE_Int n)
{
    Microtek2_Scanner *ms = handle;

    DBG(255, "sane_get_option_descriptor: handle=%p, opt=%d\n", handle, n);

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
            case MS_MODE_LINEARTFAKE:
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


    DBG(30, "sane_get_select_fd: ms=%p\n", ms);

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
    Microtek2_Device *md;
    FILE *fp;
    char dev_name[PATH_MAX];
    int match;
    SANE_Auth_Callback trash;

    DBG_INIT();
    DBG(1, "sane_init: Microtek2 (v%d.%d) says hello...\n",
           MICROTEK2_MAJOR, MICROTEK2_MINOR);

    if ( version_code )
        *version_code = SANE_VERSION_CODE(V_MAJOR, V_MINOR, 0);

#ifdef HAVE_AUTHORIZATION
    auth_callback = authorize;
#else
    trash = authorize;     /* prevents compiler warning "unused variable" */
#endif

    match = 0;
    fp = sanei_config_open(MICROTEK2_CONFIG_FILE);
    if ( fp == NULL )
        DBG(10, "sane_init: file not opened: '%s'\n", MICROTEK2_CONFIG_FILE);
    else
      {
        /* check config file for devices and associated options */
        parse_config_file(fp, &md_config_temp);

        while ( sanei_config_read(dev_name, sizeof(dev_name), fp) )
          {
            /* ignore empty lines and comments */
            if ( dev_name[0] != '#' && strlen(dev_name) != 0 )
              {
                if ( md_config_temp )
                  {
                    if ( strcmp(dev_name, md_config_temp->device ) == 0 )
                        match = 1;
                  }

                sanei_config_attach_matching_devices(dev_name, attach_one);

                if ( match )
                  {
                    match = 0;
                    if ( md_config_temp->next )
                        md_config_temp = md_config_temp->next;
                  }
              }
          }

        fclose(fp);
      }

    if ( md_first_dev == NULL )
      {
        /* config file not found or no valid entry; default to /dev/scanner */
        /* instead of insisting on config file */
        add_device_list("/dev/scanner", &md);
        if ( md )
            attach(md);
      }
    return SANE_STATUS_GOOD;
}


/*---------- sane_open() -----------------------------------------------------*/

SANE_Status
sane_open(SANE_String_Const name, SANE_Handle *handle)
{
    SANE_Status status;
    Microtek2_Scanner *ms;
    Microtek2_Device *md;
#ifdef HAVE_AUTHORIZATION
    struct stat st;
    int rc;
#endif


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
    ms->scanning = SANE_FALSE;
    ms->cancelled = SANE_FALSE;
    ms->current_pass = 0;
    ms->sfd = -1;
    ms->pid = -1;
    ms->fp = NULL;
    ms->gamma_table = NULL;
    ms->buf.src_buf = ms->buf.src_buffer[0] = ms->buf.src_buffer[1] = NULL;
    ms->control_bytes = NULL;
    ms->shading_image = NULL;
    ms->condensed_shading_w = NULL;
    ms->condensed_shading_d = NULL;

    init_options(ms, MD_SOURCE_FLATBED);

    /* insert scanner into linked list */
    ms->next = ms_first_handle;
    ms_first_handle = ms;

    *handle = ms;

#ifdef HAVE_AUTHORIZATION
    /* check whether the file with the passwords exists. If it doesn´t */
    /* exist, we don´t use any authorization */

    rc = stat(PASSWD_FILE, &st);
    if ( rc == -1 && errno == ENOENT )
        return SANE_STATUS_GOOD;
    else
      {
        status = do_authorization(md->name);
        return status;
      }
#else
    return SANE_STATUS_GOOD;
#endif
}


/*---------- sane_read() -----------------------------------------------------*/

SANE_Status
sane_read(SANE_Handle handle, SANE_Byte *buf, SANE_Int maxlen, SANE_Int *len )
{
    Microtek2_Scanner *ms = handle;
    SANE_Status status;
    ssize_t nread;


    DBG(30, "sane_read: handle=%p, buf=%p, maxlen=%d\n", handle, buf, maxlen);

    *len = 0;

    if ( ! ms->scanning || ms->cancelled )
      {
        if ( ms->cancelled )
          {
            /* cancel_scan(ms); */
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
    SANE_Status status;
    Microtek2_Scanner *ms = handle;
    Microtek2_Device *md;
    Microtek2_Info *mi;
    u_int8_t *pos;
    int color;
    int strip_lines;
    int rc;
    int i;

    DBG(30, "sane_start: handle=0x%p\n", handle);

    md = ms->dev;
    mi = &md->info[md->scan_source];

    if (ms->sfd < 0) {      /* first or only pass of this scan */

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

        if ( ms->val[OPT_CALIB_BACKEND].w == SANE_TRUE
             && ! (md->model_flags & MD_PHANTOM336CX_TYPE_SHADING ) )
          {
            if ( ! md->status.sskip || md->shading_table_w == NULL )
              {
                status = read_shading_image(ms);
                if ( status != SANE_STATUS_GOOD )
                    goto cleanup;
              }
          }

        status = get_scan_parameters(ms);
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
        status = scsi_read_system_status(md, ms->sfd);
        if ( status != SANE_STATUS_GOOD )
            goto cleanup;

        if ( ms->no_backtracking )
            md->status.ntrack |= MD_NTRACK_ON;
        else
            md->status.ntrack &= ~MD_NTRACK_ON;

        status = scsi_send_system_status(md, ms->sfd);
        if ( status != SANE_STATUS_GOOD )
            goto cleanup;

        /* calculate gamma: we assume, that the gamma values are transferred */
        /* with one send gamma command, even if it is a 3 pass scanner */
        if ( md->model_flags & MD_NO_GAMMA )
          {
            ms->lut_size = (int) pow(2.0, (double) ms->depth);
            ms->lut_entry_size = ms->depth > 8 ? 2 : 1;
          }
        else
          {
            get_lut_size(mi, &ms->lut_size, &ms->lut_entry_size);
          }
        ms->lut_size_bytes = ms->lut_size * ms->lut_entry_size;
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
            calculate_gamma(ms, pos, color, ms->gamma_mode);
          }

        /* Some models ignore the settings for the exposure time, */
        /* so we must do it ourselves. Apparently this seems to be */
        /* the case for all models that have the chunky data format */

        if ( mi->data_format == MI_DATAFMT_CHUNKY )
            set_exposure(ms);

        if ( ! (md->model_flags & MD_NO_GAMMA) )
          {
            status = scsi_send_gamma(ms, 3 * ms->lut_size_bytes);
            if ( status != SANE_STATUS_GOOD )
                goto cleanup;
          }

        status = scsi_set_window(ms, 1);
        if ( status != SANE_STATUS_GOOD )
            goto cleanup;

#if 0
        if ( ms->calib_backend
             && ! (md->model_flags & MD_PHANTOM336CX_TYPE_SHADING )
             && ! (md->model_flags & MD_READ_CONTROL_BIT) )
          {
            status = scsi_send_shading(ms,
                                       3 * ms->lut_entry_size * mi->geo_width,
                                       0);
            if ( status != SANE_STATUS_GOOD )
                goto cleanup;
          }
#endif

        ms->scanning = SANE_TRUE;
        ms->cancelled = SANE_FALSE;
      }

    ++ms->current_pass;

    status = scsi_read_image_info(ms);
    if ( status != SANE_STATUS_GOOD )
        goto cleanup;

    /* calculate maximum number of lines to read */
    strip_lines = (int) ((double) ms->y_resolution_dpi * md->opts.strip_height);
    if ( strip_lines == 0 )
        strip_lines = 1;

    /* calculate number of lines that fit into the source buffer */
    ms->src_max_lines = MIN(sanei_scsi_max_request_size / ms->bpl,
                                                 (u_int32_t)strip_lines);
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

    /* allocate a temporary buffer for the data, if auto_adjust threshold */
    /* is selected. */

    if ( ms->auto_adjust == 1 )
      {
        ms->temporary_buffer = (u_int8_t *) malloc(ms->remaining_bytes);
        if ( ms->temporary_buffer == NULL )
          {
            DBG(1, "sane_start: malloc() for temporary buffer failed\n");
            goto cleanup;
          }
      }
    else
        ms->temporary_buffer = NULL;

    /* some data formats have additional information in a scan line, which */
    /* is not transferred to the frontend; real_bpl is the number of bytes */
    /* per line, that is copied into the frontend's buffer */
    ms->real_bpl = (u_int32_t) ceil( ((double) ms->ppl *
                                      (double) ms->bits_per_pixel_out) / 8.0 );
    if ( mi->onepass && ms->mode == MS_MODE_COLOR )
        ms->real_bpl *= 3;

    ms->real_remaining_bytes = ms->real_bpl * ms->src_remaining_lines;


    /* Apparently the V300 and the Phantom 33[06]cx don't know */
    /* "read image status" at all */
    if ( ! (md->model_flags & MD_NO_RIS_COMMAND) )
      {
        status = scsi_wait_for_image(ms);
        if ( status  != SANE_STATUS_GOOD )
            goto cleanup;
      }

    if ( md->model_flags & MD_PHANTOM336CX_TYPE_SHADING )
      {
        status = scsi_read_system_status(md, ms->sfd);
        if ( status != SANE_STATUS_GOOD )
            goto cleanup;

        if ( ! md->status.sskip )    /* don't skip shading */
          {
            /* TBD */
          }
      }


    if ( ms->val[OPT_CALIB_BACKEND].w == SANE_TRUE
         && ( md->model_flags & MD_READ_CONTROL_BIT ) )
      {
        /* read control bits. For some reason there are some bits */
        /* more than one would expect and the number of control bytes */
        /* is stored per model in md->n_control_bytes. */
        ms->n_control_bytes = md->n_control_bytes;
        ms->control_bytes = (u_int8_t *) malloc(ms->n_control_bytes);
        if ( ms->control_bytes == NULL )
          {
            DBG(1, "sane_start: malloc() for control bits failed\n");
            status = SANE_STATUS_NO_MEM;
            goto cleanup;
          }
        status = scsi_read_control_bits(ms, ms->sfd);
        if ( status != SANE_STATUS_GOOD )
            goto cleanup;

        /* experimental */
        status = condense_shading(ms);

      }

    if ( ms->lightlid35 )
      {
        status = scsi_read_system_status(md, ms->sfd);
        if ( status != SANE_STATUS_GOOD )
            goto cleanup;

        md->status.flamp &= ~MD_FLAMP_ON;
        status = scsi_send_system_status(md, ms->sfd);
        if ( status != SANE_STATUS_GOOD )
            goto cleanup;

      }

    /* calculate sane parameters */
    status = calculate_sane_params(ms);
    if ( status != SANE_STATUS_GOOD )
        goto cleanup;

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


    if ( (hdev = strdup(dev_name)) == NULL)
      {
        DBG(5, "add_device_list: malloc() for hdev failed\n");
        return SANE_STATUS_NO_MEM;
      }

    len = strlen(hdev);
    if ( hdev[len - 1] == '\n' )
        hdev[--len] = '\0';

    DBG(30, "add_device_list: device='%s'\n", hdev);

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
    md->shading_table_w = NULL;
    md->shading_table_d = NULL;
    strncpy(md->name, hdev, PATH_MAX - 1);
    if ( md_config_temp )
        md->opts = md_config_temp->opts;
    else
        md->opts = md_options;
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
    SANE_Status status;


    DBG(30, "attach: device='%s'\n", md->name);

    status = scsi_inquiry(&(md->info[MD_SOURCE_FLATBED]), md->name);
    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "attach: '%s'\n", sane_strstatus(status));
        return status;
      }

    /* Here we should insert a function, that stores all the relevant */
    /* information in the info structure in a more conveniant format */
    /* in the device structure, e.g. the model name with a trailing '\0'. */

    status = check_inquiry(md, &model_string);
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

    if ( MI_LUTCAP_NONE( md->info[MD_SOURCE_FLATBED].lut_cap) )
        /* no gamma tables */
        md->model_flags |= MD_NO_GAMMA;

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
        /* The Phantom 636cx indicates in its attributes that it supports */
        /* slides, but it doesn't. Thus this command would fail. */

        if ( ! (md->model_flags & MD_NO_SLIDE_MODE) )
          {
            status = scsi_read_attributes(&md->info[0],
                                          md->name,
                                          MD_SOURCE_SLIDE);
            if ( status != SANE_STATUS_GOOD )
                return status;
          }
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

    return SANE_STATUS_GOOD;
}


/*---------- auto_adjust_proc_data() -----------------------------------------*/

static SANE_Status
auto_adjust_proc_data(Microtek2_Scanner *ms, u_int8_t **temp_current)
{
    Microtek2_Device *md;
    Microtek2_Info *mi;
    SANE_Status status;
    u_int8_t *from;
    u_int32_t line;
    u_int32_t lines;
    u_int32_t pixel;
    u_int32_t threshold;
    int right_to_left;


    DBG(30, "auto_adjust_proc_data: ms=%p, temp_current=%p\n",
            ms, *temp_current);

    md = ms->dev;
    mi = &md->info[md->scan_source];
    right_to_left = mi->direction & MI_DATSEQ_RTOL;

    memcpy(*temp_current, ms->buf.src_buf, ms->transfer_length);
    *temp_current += ms->transfer_length;
    threshold = 0;
    status = SANE_STATUS_GOOD;

    if ( ms->src_remaining_lines == 0 ) /* we have read all the image data, */
      {                                 /* calculate threshold value */
        for ( pixel = 0; pixel < ms->remaining_bytes; pixel++ )
            threshold += *(ms->temporary_buffer + pixel);

        threshold /= ms->remaining_bytes;
        lines = ms->remaining_bytes / ms->bpl;
        for ( line = 0; line < lines; line++ )
          {
            from = ms->temporary_buffer + line * ms->bpl;
            if ( right_to_left == 1 )
                from += ms->ppl - 1;
            status = lineartfake_copy_pixels(from,
                                             ms->ppl,
                                             (u_int8_t) threshold,
                                             right_to_left,
                                             ms->fp);
          }
        *temp_current = NULL;
      }

    return status;
}


/*---------- calculate_gamma() -----------------------------------------------*/

static SANE_Status
calculate_gamma(Microtek2_Scanner *ms, u_int8_t *pos, int color, char *mode)
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


    DBG(30, "calculate_gamma: ms=%p, pos=%p, color=%d, mode=%s\n",
             ms, pos, color, mode);

    md = ms->dev;
    mi = &md->info[md->scan_source];

    /* does this work everywhere ? */
    if ( md->model_flags & MD_NO_GAMMA )
      {
        factor = 1;
        mult = (double) (ms->lut_size - 1);
      }
    else
      {
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
      }

#if 0
    factor = ms->lut_size / (int) pow(2.0, (double) ms->depth);
    mult = pow(2.0, (double) ms->depth) - 1.0;  /* depending on output size */
#endif

    steps = (double) (ms->lut_size - 1);      /* depending on input size */

    DBG(30, "calculate_gamma: factor=%d, mult =%f, steps=%f, mode=%s\n",
             factor, mult, steps, ms->val[OPT_GAMMA_MODE].s);


    if ( strcmp(mode, MD_GAMMAMODE_SCALAR) == 0 )
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

            if ( ms->lut_entry_size == 2 )
                *((u_int16_t *) pos + i) = (u_int16_t) val;
            else
                *((u_int8_t *) pos + i) = (u_int8_t) val;
          }
      }
    else if ( strcmp(mode, MD_GAMMAMODE_CUSTOM) == 0 )
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
    else if ( strcmp(mode, MD_GAMMAMODE_LINEAR) == 0 )
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


/*---------- calculate_sane_params() -----------------------------------------*/

static SANE_Status
calculate_sane_params(Microtek2_Scanner *ms)
{
    Microtek2_Device *md;
    Microtek2_Info *mi;


    DBG(30, "calculate_sane_params: ms=%p\n", ms);

    md = ms->dev;
    mi = &md->info[md->scan_source];

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
            DBG(1, "calculate_sane_params: invalid pass number %d\n",
                    ms->current_pass);
            return SANE_STATUS_IO_ERROR;
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
    status = scsi_read_image(ms, (u_int8_t *) NULL);
    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "cancel_scan: cancel failed: '%s'\n", sane_strstatus(status));
        status = SANE_STATUS_IO_ERROR;
      }
    else
        status = SANE_STATUS_CANCELLED;

    close(ms->fd[1]);
    kill(ms->pid, SIGTERM);
    waitpid(ms->pid, NULL, 0);

    return status;
}


/*---------- check_option() --------------------------------------------------*/

static void
check_option(char *cp, Config_Options *co)
{
    /* This function analyses options in the config file */

    char *endptr;

    /* When this function is called, it is already made sure that this */
    /* is an option line, i.e. a line that starts with ´option´ */

    cp = (char *) sanei_config_skip_whitespace(cp);     /* skip blanks */
    cp = (char *) sanei_config_skip_whitespace(cp + 6); /* skip "option" */
    if ( strncmp(cp, "dump", 4) == 0 && isspace(cp[4]) )
      {
        cp = (char *) sanei_config_skip_whitespace(cp + 4);
        if ( *cp )
          {
            md_dump = (int) strtol(cp, &endptr, 10);
            if ( md_dump > 4 || md_dump < 0 )
              {
                md_dump = 1;
                DBG(30, "check_option: setting dump to %d\n", md_dump);
              }
            cp = (char *) sanei_config_skip_whitespace(endptr);
            if ( *cp )
              {
                /* something behind the option value or value wrong */
                md_dump = 1;
                DBG(30, "check_option: option value wrong\n");
              }
          }
        else
          {
            DBG(30, "check_option: missing option value\n");
            /* reasonable fallback */
            md_dump = 1;
          }
      }
    else if ( strncmp(cp, "strip-height", 12) == 0 && isspace(cp[12]) )
      {
        cp = (char *) sanei_config_skip_whitespace(cp + 12);
        if ( *cp )
          {
            co->strip_height = strtod(cp, &endptr);
            DBG(30, "check_option: setting strip_height to %f\n",
                     co->strip_height);
            if ( co->strip_height <= 0.0 )
                co->strip_height = 14.0;
            cp = (char *) sanei_config_skip_whitespace(endptr);
            if ( *cp )
              {
                /* something behind the option value or value wrong */
                co->strip_height = 14.0;
                DBG(30, "check_option: option value wrong: %f\n",
                         co->strip_height);
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
            co->no_backtracking = "on";
          }
        else if ( strncmp(cp, "off", 3) == 0 )
          {
            cp = (char *) sanei_config_skip_whitespace(cp + 3);
            co->no_backtracking = "off";
          }
        else
            co->no_backtracking = "off";

        if ( *cp )
          {
            /* something behind the option value or value wrong */
            co->no_backtracking = "off";
            DBG(30, "check_option: option value wrong: %s\n", cp);
          }
      }
    else if ( strncmp(cp, "lightlid-35", 11) == 0
              && isspace(cp[11]) )
      {
        cp = (char *) sanei_config_skip_whitespace(cp + 11);
        if ( strncmp(cp, "on", 2) == 0 )
          {
            cp = (char *) sanei_config_skip_whitespace(cp + 2);
            co->lightlid35 = "on";
          }
        else if ( strncmp(cp, "off", 3) == 0 )
          {
            cp = (char *) sanei_config_skip_whitespace(cp + 3);
            co->lightlid35 = "off";
          }
        else
            co->lightlid35 = "off";

        if ( *cp )
          {
            /* something behind the option value or value wrong */
            co->lightlid35 = "off";
            DBG(30, "check_option: option value wrong: %s\n", cp);
          }
      }
    else if ( strncmp(cp, "toggle-lamp", 11) == 0
              && isspace(cp[11]) )
      {
        cp = (char *) sanei_config_skip_whitespace(cp + 11);
        if ( strncmp(cp, "on", 2) == 0 )
          {
            cp = (char *) sanei_config_skip_whitespace(cp + 2);
            co->toggle_lamp = "on";
          }
        else if ( strncmp(cp, "off", 3) == 0 )
          {
            cp = (char *) sanei_config_skip_whitespace(cp + 3);
            co->toggle_lamp = "off";
          }
        else
            co->toggle_lamp = "off";

        if ( *cp )
          {
            /* something behind the option value or value wrong */
            co->toggle_lamp = "off";
            DBG(30, "check_option: option value wrong: %s\n", cp);
          }
      }
    else if ( strncmp(cp, "lineart-autoadjust", 18) == 0
              && isspace(cp[18]) )
      {
        cp = (char *) sanei_config_skip_whitespace(cp + 18);
        if ( strncmp(cp, "on", 2) == 0 )
          {
            cp = (char *) sanei_config_skip_whitespace(cp + 2);
            co->auto_adjust = "on";
          }
        else if ( strncmp(cp, "off", 3) == 0 )
          {
            cp = (char *) sanei_config_skip_whitespace(cp + 3);
            co->auto_adjust = "off";
          }
        else
            co->auto_adjust = "off";

        if ( *cp )
          {
            /* something behind the option value or value wrong */
            co->auto_adjust = "off";
            DBG(30, "check_option: option value wrong: %s\n", cp);
          }
      }
    else if ( strncmp(cp, "backend-calibration", 19) == 0
              && isspace(cp[19]) )
      {
        cp = (char *) sanei_config_skip_whitespace(cp + 19);
        if ( strncmp(cp, "on", 2) == 0 )
          {
            cp = (char *) sanei_config_skip_whitespace(cp + 2);
            co->backend_calibration = "on";
          }
        else if ( strncmp(cp, "off", 3) == 0 )
          {
            cp = (char *) sanei_config_skip_whitespace(cp + 3);
            co->backend_calibration = "off";
          }
        else
            co->backend_calibration = "off";

        if ( *cp )
          {
            /* something behind the option value or value wrong */
            co->backend_calibration = "off";
            DBG(30, "check_option: option value wrong: %s\n", cp);
          }
      }
    else
        DBG(30, "check_option: invalid option in '%s'\n", cp);
}


/*---------- check_inquiry() -------------------------------------------------*/

static SANE_Status
check_inquiry(Microtek2_Device *md, SANE_String *model_string)
{
    Microtek2_Info *mi;

    DBG(30, "check_inquiry: md=%p\n", md);


    mi = &md->info[MD_SOURCE_FLATBED];
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

    if ( strncasecmp("MICROTEK", mi->vendor, INQ_VENDOR_L) != 0
         && strncmp("        ", mi->vendor, INQ_VENDOR_L) != 0
         && strncmp("AGFA    ", mi->vendor, INQ_VENDOR_L) != 0 )
      {
        DBG(1, "check_inquiry: Device is not a Microtek, but '%.*s'\n",
                INQ_VENDOR_L, mi->vendor);
        return SANE_STATUS_IO_ERROR;
      }

    switch (mi->model_code)
      {
        case 0x81:
          *model_string = "ScanMaker 4";
          break;
        case 0x85:
          *model_string = "ScanMaker V300";
          /* The ScanMaker V300 rejects the "read image status" command */
          /* and returns some values for the "read image info" command */
          /* in only two bytes */
          md->model_flags |= MD_NO_RIS_COMMAND | MD_RII_TWO_BYTES;
          break;
        case 0x8a:
          *model_string = "ScanMaker 9600XL";
          break;
        case 0x8c:
          *model_string = "ScanMaker 630 / ScanMaker V600";
          break;
        case 0x8d:
          *model_string = "ScanMaker 330";
          break;
        case 0x91:
          *model_string = "ScanMaker X6 / Phantom 636";
          /* The X6 indicates a data format of segregated data in TMA mode */
          /* but actually transfers as chunky data */
          md->model_flags |= MD_DATA_FORMAT_WRONG;
          if ( md->revision == 1.00 )
              md->model_flags |= MD_OFFSET_2;
          break;
        case 0x92:
          *model_string = "E3+ / Vobis HighScan";
          break;
        case 0x93:
          *model_string = "ScanMaker 330";
          break;
        case 0x94:
          *model_string = "Phantom 330cx / Phantom 336cx";
          /* These models reject the "read image status" command */
          md->model_flags |= MD_NO_RIS_COMMAND | MD_PHANTOM336CX_TYPE_SHADING;
          md->n_control_bytes = 320;
          md->shading_length = 18;
          break;
        case 0x97:
          *model_string = "ScanMaker 636";
          break;
        case 0x98:
          *model_string = "ScanMaker X6EL";
          if ( md->revision == 1.00 )
              md->model_flags |= MD_OFFSET_2;
          break;
        case 0x99:
          *model_string = "ScanMaker X6USB";
          if ( md->revision == 1.00 )
              md->model_flags |= MD_OFFSET_2;
          md->model_flags |= MD_X6_SHORT_TRANSFER;
          break;
        case 0x9a:
          *model_string = "Phantom 636cx / C6";
          /* The Phantom 636cx says it supports the SLIDE mode, but it */
          /* doesn't. Thus inquring the attributes for slide mode would */
          /* fail. Also it does not accept gamma tables. Apparently */
          /* it reads the control bits and does not accept shading tables */
          md->model_flags |= MD_NO_SLIDE_MODE
                             | MD_READ_CONTROL_BIT
                             | MD_NO_GAMMA
                             | MD_PHANTOM_C6;
          md->n_control_bytes = 647;
          md->shading_length = 18;
          break;
        case 0x9d:
          *model_string = "AGFA Duoscan T1200";
          break;
        case 0xa3:
          *model_string = "ScanMaker V6USL";
          /* The V6USL does not accept gamma tables */
          md->model_flags |= MD_NO_GAMMA;
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
        int scale1;
        int scale2;
        u_int16_t val16;

        scale1 = 16 - depth;
        scale2 = 2 * depth - 16;
        for ( pixel = 0; pixel < pixels; pixel++ )
          {
            for ( color = 0; color < 3; color++ )
              {
                val16 = *(u_int16_t *) from;
                val16 = ( val16 << scale1 ) | ( val16 >> scale2 );
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
    SANE_Status status;
    Microtek2_Device *md;
    Microtek2_Info *mi;
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
    /* bpl = 3*ppl (even number of pixels) or bpl=3*ppl+1 (odd number of */
    /* pixels. Even worse: On different models it is different at which */
    /* position in a scanline the image data starts. bpl_ppl_diff tries */
    /* to fix this. */

    if ( (md->model_flags & MD_OFFSET_2) && pad == 1 )
        bpl_ppl_diff = 2;
    else
        bpl_ppl_diff = 0;

#if 0
    if ( md->revision == 1.00 && mi->model_code != 0x81 )
        bpl_ppl_diff = ms->bpl - ( 3 * ms->ppl * bpp ) - pad;
    else
        bpl_ppl_diff = ms->bpl - ( 3 * ms->ppl * bpp );

    if ( md->revision > 1.00 )
        bpl_ppl_diff = ms->bpl - ( 3 * ms->ppl * bpp );
    else
        bpl_ppl_diff = ms->bpl - ( 3 * ms->ppl * bpp ) - pad;
#endif

    from = ms->buf.src_buf;

    DBG(30, "chunky_proc_data: lines=%d, bpl=%d, ppl=%d, bpp=%d, depth=%d"
            " junk=%d\n", ms->src_lines_to_read, ms->bpl, ms->ppl,
             bpp, ms->depth, bpl_ppl_diff);

    for ( line = 0; line < (u_int32_t)ms->src_lines_to_read; line++ )
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

    if ( ms->temporary_buffer )
      {
        free((void *) ms->temporary_buffer);
        ms->temporary_buffer = NULL;
      }

    if ( ms->gamma_table )
      {
        free((void *) ms->gamma_table);
        ms->gamma_table = NULL;
      }

    if ( ms->control_bytes )
      {
        free((void *) ms->control_bytes);
        ms->control_bytes = NULL;
      }

    if ( ms->condensed_shading_w )
      {
        free((void *) ms->condensed_shading_w);
        ms->condensed_shading_w = NULL;
      }

    if ( ms->condensed_shading_d )
      {
        free((void *) ms->condensed_shading_d);
        ms->condensed_shading_d = NULL;
      }

    return;
}


/*---------- condense_shading() ----------------------------------------------*/

static SANE_Status
condense_shading(Microtek2_Scanner *ms)
{
    /* This function extracts the relevant shading pixels from */
    /* the shading image according to the 1's in the result of */
    /* 'read control bits', and stores them in a memory block. */
    /* We will then have as many shading pixels as there are */
    /* pixels per line. The order of the pixels in the condensed */
    /* shading data block will always be left to right. */

    Microtek2_Device *md;
    Microtek2_Info *mi;
    u_int32_t length;
    u_int8_t *to_w[3];             /* This is a pointer to where we copy */
    u_int8_t *to_d[3];             /* the shading data (per color) and */
                                   /* for white and dark compensation */
    int color;
    int byte;
    int bit;
    int count_1s;
    int step;
    int i;


    DBG(30, "condense_shading: ms=%p\n", ms);

    md = ms->dev;
    mi = &md->info[md->scan_source];
    get_lut_size(mi, &ms->lut_size, &ms->lut_entry_size);
    length = 3 * ms->ppl * ms->lut_entry_size;
    step = ( mi->direction & MI_DATSEQ_RTOL ) ? -1 : 1;

    ms->condensed_shading_w = (u_int8_t *) malloc(length);
    if ( ms->condensed_shading_w == NULL )
      {
        DBG(1, "condense_shading: malloc for white table failed\n");
        return SANE_STATUS_NO_MEM;
      }

    if ( ! MI_WHITE_SHADING_ONLY(mi->shtrnsferequ)
         || (md->model_flags & MD_PHANTOM_C6) )
      {
        if ( ms->condensed_shading_d == NULL )
          {
            ms->condensed_shading_d = (u_int8_t *) malloc(length);
            if ( ms->condensed_shading_d == NULL )
              {
                DBG(1, "condense_shading: malloc for dark table failed\n");
                return SANE_STATUS_NO_MEM;
              }
          }
      }

    for ( color = 0; color < 3; color++ )
      {
        if ( mi->direction & MI_DATSEQ_RTOL )    /* scanning direction is */
          {                                      /* right to left */
            to_w[color] = ms->condensed_shading_w
                          + ( color + 1 ) * ms->ppl * ms->lut_entry_size
                          - ms->lut_entry_size;

            if ( ms->condensed_shading_d != NULL )
                to_d[color] = ms->condensed_shading_d
                              + ( color + 1 ) * ms->ppl * ms->lut_entry_size
                              - ms->lut_entry_size;
          }
        else
          {
            to_w[color] = ms->condensed_shading_w
                          + color * ms->ppl * ms->lut_entry_size;
            if ( ms->condensed_shading_d != NULL )
                to_d[color] = ms->condensed_shading_w
                              + color * ms->ppl * ms->lut_entry_size;
          }
      }

    count_1s = 0;
    i = 0;
    for ( byte = 0; byte < (int)ms->n_control_bytes; byte++ )
      {
        for ( bit = 0; bit < 8; bit++ )
          {
            /* in lineart mode there are more 1' in the control bytes */
            /* than we have pixels per line. */

            if ( count_1s >= (int)ms->ppl )
                break;

            /* experimental */
            /* I would have expected, that the relevant range in the */
            /* control bytes start at the beginning of the control byte */
            /* sequence. However, when scanning with maximum resolution */
            /* there are two bytes of 0's at the beginning. Also, I would */
            /* have expected, that when scanning with maximum resolution */
            /* there would be a consecutive number of <CCD pixels> 1's, but */
            /* in this case it starts with 0xFCFFFF.. */
#if 0
            if ( GETBIT(ms->control_bytes[byte], bit) == 1 )
#endif
            if ( GETBIT(ms->control_bytes[byte + 2], bit) == 1 )
              {
                for ( color = 0; color < 3; color++ )
                  {
                    if ( ms->lut_entry_size == 2 )
                      {
                        *((u_int16_t *) to_w[color] + count_1s * step) =
                                          *((u_int16_t *) md->shading_table_w
                                          + color * mi->geo_width
                                          + 8 * byte + bit);
                        if ( ms->condensed_shading_d != NULL )
                            *((u_int16_t *) to_d[color] + count_1s * step) =
                                            *((u_int16_t *) md->shading_table_d
                                            + color * mi->geo_width
                                            + 8 * byte + bit);
                      }
                    else
                      {
                        *(to_w[color] + count_1s * step) =
                                                      *(md->shading_table_w
                                                      + color * mi->geo_width
                                                      + 8 * byte + bit);
                        if ( ms->condensed_shading_d != NULL )
                            *(to_d[color] + count_1s * step) =
                                                        *(md->shading_table_d
                                                        + color * mi->geo_width
                                                        + 8 * byte + bit);
                      }
                  }
                ++count_1s;
              }
          }
      }

    DBG(30, "condense_shading: number of 1's in controlbytes: %d\n", count_1s);

    return SANE_STATUS_GOOD;

}

#ifdef HAVE_AUTHORIZATION
/*---------- do_authorization() ----------------------------------------------*/

static SANE_Status
do_authorization(char *ressource)
{
    /* This function implements a simple authorization function. It looks */
    /* up an entry in the file SANE_PATH_CONFIG_DIR/auth. Such an entry */
    /* must be of the form device:user:password where password is a crypt() */
    /* encrypted password. If several users are allowed to access a device */
    /* an entry must be created for each user. If no entry exists for device */
    /* or the file does not exist no authentication is neccessary. If the */
    /* file exists, but can´t be opened the authentication fails */

    SANE_Status status;
    FILE *fp;
    int device_found;
    char username[SANE_MAX_USERNAME_LEN];
    char password[SANE_MAX_PASSWORD_LEN];
    char line[MAX_LINE_LEN];
    char *linep;
    char *device;
    char *user;
    char *passwd;
    char *p;


    DBG(30, "do_authorization: ressource=%s\n", ressource);

    if ( auth_callback == NULL )  /* frontend does not require authorization */
        return SANE_STATUS_GOOD;

    /* first check if an entry exists in for this device. If not, we don´t */
    /* use authorization */

    fp = fopen(PASSWD_FILE, "r");
    if ( fp == NULL )
      {
        if ( errno == ENOENT )
          {
            DBG(1, "do_authorization: file not found: %s\n", PASSWD_FILE);
            return SANE_STATUS_GOOD;
          }
        else
          {
            DBG(1, "do_authorization: fopen() failed, errno=%d\n", errno);
            return SANE_STATUS_ACCESS_DENIED;
          }
      }

    linep = &line[0];
    device_found = 0;
    while ( fgets(line, MAX_LINE_LEN, fp) )
      {
        p = index(linep, SEPARATOR);
        if ( p )
          {
            *p = '\0';
            device = linep;
            if ( strcmp(device, ressource) == 0 )
              {
                DBG(2, "equal\n");
                device_found = 1;
                break;
              }
          }
      }

    if ( ! device_found )
      {
        fclose(fp);
        return SANE_STATUS_GOOD;
      }

    fseek(fp, 0L, SEEK_SET);

    (*auth_callback) (ressource, username, password);

    status = SANE_STATUS_ACCESS_DENIED;
    do
      {
        fgets(line, MAX_LINE_LEN, fp);
        if ( ! ferror(fp) && ! feof(fp) )
          {
            /* neither strsep(3) nor strtok(3) seem to work on my system */
            p = index(linep, SEPARATOR);
            if ( p == NULL )
                continue;
            *p = '\0';
            device = linep;
            if ( strcmp( device, ressource) != 0 ) /* not a matching entry */
                continue;

            linep = ++p;
            p = index(linep, SEPARATOR);
            if ( p == NULL )
                continue;

            *p = '\0';
            user = linep;
            if ( strncmp(user, username, SANE_MAX_USERNAME_LEN) != 0 )
                continue;                  /* username doesn´t match */

            linep = ++p;
            /* rest of the line is considered to be the password */
            passwd = linep;
            /* remove newline */
            *(passwd + strlen(passwd) - 1) = '\0';
            p = crypt(password, SALT);
            if ( strcmp(p, passwd) == 0 )
              {
                /* authentication ok */
                status = SANE_STATUS_GOOD;
                break;
              }
            else
                continue;
          }
      } while ( ! ferror(fp) && ! feof(fp) );
    fclose(fp);

    return status;
}
#endif


/*---------- read_shading_image() --------------------------------------------*/

static SANE_Status
read_shading_image(Microtek2_Scanner *ms)
{
    SANE_Status status;
    Microtek2_Device *md;
    Microtek2_Info *mi;
    u_int32_t lines;
    u_int8_t *buf;
    int max_lines;
    int lines_to_read;


    DBG(30, "read_shading_image: ms=%p\n", ms);

    md = ms->dev;
    mi = &md->info[0];

    status = scsi_read_system_status(md, ms->sfd);
    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "read_shading_image: read_system_status failed: '%s'\n",
               sane_strstatus(status));
        return status;
      }

    md->status.ntrack |= MD_NTRACK_ON;
    md->status.ncalib &= ~MD_NCALIB_ON;
    if ( md->model_flags & MD_PHANTOM_C6 )
      {
        md->status.reserved04 |= MD_RESERVED04_ON;
        md->status.reserved17 |= MD_RESERVED17_ON;
      }

    if ( MI_WHITE_SHADING_ONLY(mi->shtrnsferequ) )
        md->status.flamp |= MD_FLAMP_ON;

    status = scsi_send_system_status(md, ms->sfd);
    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "read_shading_image: send_system_status failed: '%s'\n",
               sane_strstatus(status));
        return status;
      }

    get_calib_params(ms);

    status = scsi_set_window(ms, 1);
    if ( status != SANE_STATUS_GOOD )
        return status;

    status = scsi_read_image_info(ms);
    if ( status != SANE_STATUS_GOOD )
        return status;


    /* Apparently the V300 and the Phantom 33[06]cx don't know */
    /* "read image status" at all */
    if ( mi->model_code != 0x85 && mi->model_code != 0x94 )
      {
        status = scsi_wait_for_image(ms);
        if ( status != SANE_STATUS_GOOD )
            return status;
      }

    /* check whether shading is necessary (SShad set) */
    status = scsi_read_system_status(md, ms->sfd);
    if ( status != SANE_STATUS_GOOD )
        return status;

    if ( md->status.sskip )
      {
        /* abort scan */
        ms->transfer_length = 0;
        status = scsi_read_image(ms, NULL);
        return status;
      }

    ms->shading_image = malloc(ms->bpl * ms->src_remaining_lines);
    if ( ms->shading_image == NULL )
      {
        DBG(1, "read_shading_image: malloc for buffer failed\n");
        return SANE_STATUS_NO_MEM;
      }

    buf = ms->shading_image;

    if ( ! MI_WHITE_SHADING_ONLY(mi->shtrnsferequ)
         || (md->model_flags & MD_PHANTOM_C6) )
      {
        DBG(30, "read_shading_image: reading black data\n");

        /* turn off the lamp */
        md->status.flamp &= ~MD_FLAMP_ON;
        status = scsi_send_system_status(md, ms->sfd);
        if ( status != SANE_STATUS_GOOD )
          {
            DBG(1, "read_shading_image: send system status (turn off the lamp "
                   " failed: '%s'\n", sane_strstatus(status));
            return status;
          }

        max_lines = sanei_scsi_max_request_size / ms->bpl;
        if ( max_lines == 0 )
          {
            DBG(1, "read_shading_image: buffer too small\n");
            return SANE_STATUS_IO_ERROR;
          }
        lines = ms->src_remaining_lines;
        while ( ms->src_remaining_lines > 0 )
          {
            lines_to_read = MIN(max_lines, ms->src_remaining_lines);
            ms->src_buffer_size = lines_to_read * ms->bpl;
            ms->transfer_length = ms->src_buffer_size;

            status = scsi_read_image(ms, buf);
            if ( status != SANE_STATUS_GOOD )
              {
                DBG(1, "read_shading_image: read image failed: '%s'\n",
                        sane_strstatus(status));
                return status;
              }

            ms->src_remaining_lines -= lines_to_read;
            buf += ms->src_buffer_size;
          }
        /* experimental */
        status =  prepare_shading_data(ms, lines, &md->shading_table_d);
        if ( status != SANE_STATUS_GOOD )
            return status;


        /* Some models use "read_control bit", and the shading must be */
        /* applied by the backend, others send shading data to the device */
        if ( ! (md->model_flags & MD_READ_CONTROL_BIT) )
          {
            status =  shading_function(ms, md->shading_table_d);
            if ( status != SANE_STATUS_GOOD )
                return status;

            ms->word = ms->lut_entry_size == 2 ? 1 : 0;
            ms->current_color = MS_COLOR_ALL;
            status = scsi_send_shading(ms,
                                       md->shading_table_d,
                                       3 * ms->lut_entry_size * mi->geo_width,
                                       1);
            if ( status != SANE_STATUS_GOOD )
                return status;
          }

        /* According to the doc NCalib must be set for white shading data */
        /* if we have a black and a white shading correction ?? */

        /* first steps of a white shading correction */

        md->status.ncalib |= MD_NCALIB_ON;
        md->status.flamp |= MD_FLAMP_ON;
        md->status.ntrack |= MD_NTRACK_ON;
        status = scsi_send_system_status(md, ms->sfd);
        if ( md->model_flags & MD_PHANTOM_C6 )
            md->status.reserved04 &= ~MD_RESERVED04_ON;

        if ( status != SANE_STATUS_GOOD )
          {
            DBG(1, "read_shading_image: send_system_status failed: '%s'\n",
                   sane_strstatus(status));
            return status;
          }

        status = scsi_set_window(ms, 1);
        if ( status != SANE_STATUS_GOOD )
            return status;

        status = scsi_read_image_info(ms);
        if ( status != SANE_STATUS_GOOD )
            return status;

        status = scsi_wait_for_image(ms);
        if ( status != SANE_STATUS_GOOD )
            return status;

      }

    /* white shading correction */

    DBG(30, "read_shading_image: reading white data\n");

    buf = ms->shading_image;
    max_lines = sanei_scsi_max_request_size / ms->bpl;
    if ( max_lines == 0 )
      {
        DBG(1, "read_shading_image: buffer too small\n");
        return SANE_STATUS_IO_ERROR;
      }
    lines = ms->src_remaining_lines;
    while ( ms->src_remaining_lines > 0 )
      {
        lines_to_read = MIN(max_lines, ms->src_remaining_lines);
        ms->src_buffer_size = lines_to_read * ms->bpl;
        ms->transfer_length = ms->src_buffer_size;

        status = scsi_read_image(ms, buf);
        if ( status != SANE_STATUS_GOOD )
            return status;

        ms->src_remaining_lines -= lines_to_read;
        buf += ms->src_buffer_size;
      }

    status =  prepare_shading_data(ms, lines, &md->shading_table_w);
    if ( status != SANE_STATUS_GOOD )
        return status;

    /* Some models use "read_control bit" and the shading correction must be */
    /* applied by the backend, others send shading data to the device */
    if ( ! (md->model_flags & MD_READ_CONTROL_BIT) )
      {
        status =  shading_function(ms, md->shading_table_w);
        if ( status != SANE_STATUS_GOOD )
            return status;

        ms->word = ms->lut_entry_size == 2 ? 1 : 0;
        ms->current_color = MS_COLOR_ALL;

        status = scsi_send_shading(ms,
                                   md->shading_table_w,
                                   3 * ms->lut_entry_size * mi->geo_width,
                                   0);
        if ( status != SANE_STATUS_GOOD )
            return status;
      }

    md->status.ncalib |= MD_NCALIB_ON;
    md->status.ntrack &= ~MD_NTRACK_ON;
    md->status.flamp |= MD_FLAMP_ON;
    if ( md->model_flags & MD_PHANTOM_C6 )
      {
        md->status.reserved04 &= ~MD_RESERVED04_ON;
        md->status.reserved17 &= ~MD_RESERVED17_ON;
      }

    status = scsi_send_system_status(md, ms->sfd);
    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "read_shading_image: send_system_status failed: %s\n",
               sane_strstatus(status));
        return status;
      }

    free((void *) ms->shading_image);
    ms->shading_image = NULL;

    ms->rawdat = 0;
    ms->stay = 0;

    return SANE_STATUS_GOOD;

}


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
        int scale1;
        int scale2;
        u_int16_t val16;

        if ( depth > 8 )
          {
            scale1 = 16 - depth;
            scale2 = 2 * depth - 16;
            for ( color = 0; color < 3; color++ )
              {

                val16 = *(u_int16_t *) from[color];
                val16 = ( val16 << scale1 ) | ( val16 >> scale2 );
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
    char outputline[100];
    char *outbuf;

    if ( ! info[0] )
        info = "No additional info available";    

    DBG(30, "dump_area: %s\n", info);
   
    outbuf = outputline;
    o_limit = (len + BPL - 1) / BPL;
    for ( o = 0; o < o_limit; o++)
      {
        sprintf(outbuf, "  %4d: ", o * BPL);
        outbuf += 8; 
        for ( i=0; i < BPL && (o * BPL + i ) < len; i++)
          {
            if ( i == BPL / 2 )
              { 
                sprintf(outbuf, " ");
                outbuf +=1;
              }
            sprintf(outbuf, "%02x", area[o * BPL + i]); 
            outbuf += 2;
          }
        
        sprintf(outbuf, "%*s",  2 * ( 2 + BPL - i), " " );
        outbuf += (2 * ( 2 + BPL - i));
        sprintf(outbuf, "%s",  (i == BPL / 2) ? " " : "");
        outbuf += ((i == BPL / 2) ? 1 : 0);

        for ( i = 0; i < BPL && (o * BPL + i ) < len; i++)
          {
            if ( i == BPL / 2 )
              {
                sprintf(outbuf, " ");
                outbuf += 1;
              }
            sprintf(outbuf, "%c", isprint(area[o * BPL + i])
                                  ? area[o * BPL + i]
                                  : '.');
            outbuf += 1;
          }
        outbuf = outputline;
        DBG(1, "%s\n", outbuf);
      }

    return SANE_STATUS_GOOD;
}


/*---------- dump_area2() ----------------------------------------------------*/

static SANE_Status
dump_area2(u_int8_t *area, int len, char *info)
{

#define BPL    16               /* bytes per line to print */

    int i, linelength;
    char outputline[100];
    char *outbuf;
    linelength = BPL * 3;

    if ( ! info[0] )
        info = "No additional info available";    
    
    DBG(1, "[%s]\n", info);

    outbuf = outputline;
    for ( i = 0; i < len; i++)
      {
        sprintf(outbuf, "%02x,", *(area + i));
        outbuf += 3;
        if ( ((i+1)%BPL == 0) || (i == len-1) )
           {
             outbuf = outputline;
             DBG(1, "%s\n", outbuf);
           }
      }

    return SANE_STATUS_GOOD;
}

/*---------- dump_attributes() -----------------------------------------------*/


static SANE_Status 
dump_attributes(Microtek2_Info *mi)
{
  /* dump all we know about the scanner */

  int i;

  DBG(30, "dump_attributes: mi=%p\n", mi);
  DBG(1, "\n");
  DBG(1, "Scanner attributes from device structure\n");
  DBG(1, "========================================\n");
  DBG(1, "Scanner ID...\n");
  DBG(1, "~~~~~~~~~~~~~\n");
  DBG(1, "  Vendor Name%15s: '%s'\n", " ", mi->vendor);
  DBG(1, "  Model Name%16s: '%s'\n", " ", mi->model);
  DBG(1, "  Revision%18s: '%s'\n", " ", mi->revision);
  DBG(1, "  Model Code%16s: 0x%02x\n"," ", mi->model_code);
  switch(mi->model_code) 
    {
      case 0x80: DBG(1, "%60s", "Redondo\n"); break;
      case 0x81: DBG(1, "%60s", "Aruba\n"); break;
      case 0x82: DBG(1, "%60s", "Bali\n"); break;
      case 0x83: DBG(1, "%60s", "Washington\n"); break;
      case 0x84: DBG(1, "%60s", "Manhattan\n"); break;
      case 0x85: DBG(1, "%60s", "TR3\n"); break;
      case 0x86: DBG(1, "%60s", "CCP\n"); break;
      case 0x87: DBG(1, "%60s", "Scanmaker V\n"); break;
      case 0x88: DBG(1, "%60s", "Scanmaker VI\n"); break;
      case 0x89: DBG(1, "%60s", "A3-400\n"); break;
      case 0x8a: DBG(1, "%60s", "MRS-1200A3 - ScanMaker 9600XL\n"); break;
      case 0x8b: DBG(1, "%60s", "Watt\n"); break;
      case 0x8c: DBG(1, "%60s", "TR6\n"); break;
      case 0x8d: DBG(1, "%60s", "Tr3 10-bit\n"); break;
      case 0x8e: DBG(1, "%60s", "CCB\n"); break;
      case 0x8f: DBG(1, "%68s", "Sun Rise\n"); break;
      case 0x90: DBG(1, "%60s", "ScanMaker E3 10-bit\n"); break;
      case 0x91: DBG(1, "%60s", "X6\n"); break;
      case 0x92: DBG(1, "%60s", "E3+ or Vobis Highscan\n"); break;
      case 0x93: DBG(1, "%60s", "ScanMaker 330\n"); break;
      case 0x94: DBG(1, "%60s", "Phantom 330cx or Phantom 336cx\n"); break;
      case 0x97: DBG(1, "%60s", "ScanMaker 636\n"); break;
      case 0x98: DBG(1, "%60s", "ScanMaker X6EL\n"); break;
      case 0x9a: DBG(1, "%60s", "Phantom 636cx / C6\n"); break;
      case 0x9d: DBG(1, "%60s", "AGFA DuoScan T1200\n"); break;
      case 0xa3: DBG(1, "%60s", "ScanMaker V6USL\n"); break;
      default:   DBG(1, "%60s", "Unknown\n"); break;
    }
  DBG(1, "  Device Type Code%10s: 0x%02x (%s),\n", " ",
                  mi->device_type,
	          mi->device_type & MI_DEVTYPE_SCANNER ?
                  "Scanner" : "Unknown type");
          
  switch (mi->scanner_type) 
    {
      case MI_TYPE_FLATBED:
          DBG(1, "  Scanner type%33s%s", " ", " Flatbed scanner\n");
          break;
      case MI_TYPE_TRANSPARENCY:
          DBG(1, "  Scanner type%33s%s", " ", " Transparency scanner\n");
          break;
      case MI_TYPE_SHEEDFEED:
          DBG(1, "  Scanner type%33s%s", " ", " Sheet feed scanner\n");
          break;
      default:
          DBG(1, "  Scanner type%33s%s", " ", " Unknown\n");
          break;
    }
  
  DBG(1, "  Supported options%9s: Automatic document feeder: %s\n",
	          " ", mi->option_device & MI_OPTDEV_ADF ? "Yes" : "No");
  DBG(1, "%31sTransparency media adapter: %s\n",
	          " ", mi->option_device & MI_OPTDEV_TMA ? "Yes" : "No");
  DBG(1, "%31sAuto paper detecting: %s\n",
	          " ", mi->option_device & MI_OPTDEV_ADP ? "Yes" : "No");
  DBG(1, "%31sAdvanced picture system: %s\n",
	          " ", mi->option_device & MI_OPTDEV_APS ? "Yes" : "No");
  DBG(1, "%31sStripes: %s\n",
	          " ", mi->option_device & MI_OPTDEV_STRIPE ? "Yes" : "No");
  DBG(1, "%31sSlides: %s\n",
	          " ", mi->option_device & MI_OPTDEV_SLIDE ? "Yes" : "No");
  DBG(1, "  Scan button%15s: %s\n", " ", mi->scnbuttn ? "Yes" : "No"); 
         
  DBG(1, "\n");
  DBG(1, "  Imaging Capabilities...\n");
  DBG(1, "  ~~~~~~~~~~~~~~~~~~~~~~~\n");
  DBG(1, "  Color scanner%6s: %s\n", " ", (mi->color) ? "Yes" : "No");
  DBG(1, "  Number passes%6s: %d pass%s\n", " ",
                  (mi->onepass) ? 1 : 3,
                  (mi->onepass) ? "" : "es");
  DBG(1, "  Resolution%9s: X-max: %5d dpi\n%35sY-max: %5d dpi\n",
                  " ", mi->max_xresolution, " ",mi->max_yresolution);
  DBG(1, "  Geometry%11s: Geometric width: %5d pts (%2.2f'')\n", " ",
          mi->geo_width, (float) mi->geo_width / (float) mi->opt_resolution); 
  DBG(1, "%23sGeometric height:%5d pts (%2.2f'')\n", " ",
          mi->geo_height, (float) mi->geo_height / (float) mi->opt_resolution);
  DBG(1, "  Optical resolution%1s: %d\n", " ", mi->opt_resolution);

  DBG(1, "  Modes%14s: Lineart:     %s\n%35sHalftone:     %s\n", " ",
	          (mi->scanmode & MI_HASMODE_LINEART) ? " Yes" : " No", " ",
	          (mi->scanmode & MI_HASMODE_HALFTONE) ? "Yes" : "No");

  DBG(1, "%23sGray:     %s\n%35sColor:     %s\n", " ",
	          (mi->scanmode & MI_HASMODE_GRAY) ? "    Yes" : "    No", " ",
	          (mi->scanmode & MI_HASMODE_COLOR) ? "   Yes" : "   No");

  DBG(1, "  Depths%14s: Nibble Gray:  %s\n",
	          " ", (mi->depth & MI_HASDEPTH_NIBBLE) ? "Yes" : "No");
  DBG(1, "%23s10-bit-color: %s\n",
                  " ", (mi->depth & MI_HASDEPTH_10) ? "Yes" : "No");
  DBG(1, "%23s12-bit-color: %s\n", " ",
	          (mi->depth & MI_HASDEPTH_12) ? "Yes" : "No");
  DBG(1, "  d/l of HT pattern%2s: %s\n",
                  " ", (mi->has_dnldptrn) ? "Yes" : "No");
  DBG(1, "  Builtin HT pattern%1s: %d\n", " ", mi->grain_slct);
  
  if ( MI_LUTCAP_NONE(mi->lut_cap) ) 
      DBG(1, "  LUT capabilities   : None\n"); 
  if ( mi->lut_cap & MI_LUTCAP_256B )
      DBG(1, "  LUT capabilities   :  256 bytes\n"); 
  if ( mi->lut_cap & MI_LUTCAP_1024B )
      DBG(1, "  LUT capabilities   : 1024 bytes\n"); 
  if ( mi->lut_cap & MI_LUTCAP_1024W )
      DBG(1, "  LUT capabilities   : 1024 words\n"); 
  if ( mi->lut_cap & MI_LUTCAP_4096B )
      DBG(1, "  LUT capabilities   : 4096 bytes\n"); 
  if ( mi->lut_cap & MI_LUTCAP_4096W )
      DBG(1, "  LUT capabilities   : 4096 words\n"); 
  DBG(1, "\n");
  DBG(1, "  Miscellaneous capabilities...\n");
  DBG(1, "  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
  if ( mi->onepass) 
    {
      switch(mi->data_format) 
        {
	  case MI_DATAFMT_CHUNKY:
              DBG(1, "  Data format        :%s",
                     " Chunky data, R, G & B in one pixel\n");
              break;
	  case MI_DATAFMT_LPLCONCAT:
              DBG(1, "  Data format        :%s",
                     " Line by line in concatenated sequence,\n");
              DBG(1, "%23swithout color indicator\n", " ");
              break;
	  case MI_DATAFMT_LPLSEGREG:
              DBG(1, "  Data format        :%s",
                     " Line by line in segregated sequence,\n");
              DBG(1, "%23swith color indicator\n", " ");
              break;
	  case MI_DATAFMT_WORDCHUNKY:
              DBG(1, "  Data format        : Word chunky data\n");
              break;
          default:
              DBG(1, "  Data format        : Unknown\n");
          break;
        }
    } 
  else
      DBG(1, "No information with 3-pass scanners\n");

  DBG(1, "  Color Sequence%17s: \n", " ");
  for ( i = 0; i < RSA_COLORSEQUENCE_L; i++) 
    {
      switch(mi->color_sequence[i]) 
        {
	  case MI_COLSEQ_RED:   DBG(1,"%34s%s\n", " ","R"); break;
	  case MI_COLSEQ_GREEN: DBG(1,"%34s%s\n", " ","G"); break;
	  case MI_COLSEQ_BLUE:  DBG(1,"%34s%s\n", " ","B"); break;
        }
    }
  DBG(1, "  Scanning direction%13s: ", " ");
  if ( mi->direction & MI_DATSEQ_RTOL )
      DBG(1, "Right to left\n");
  else
      DBG(1, "Left to right\n");
  DBG(1, "  CCD gap%24s: %d lines\n", " ", mi->ccd_gap);
  DBG(1, "  CCD pixels%21s: %d\n", " ", mi->ccd_pixels);
  DBG(1, "  Calib white stripe location%4s: %d\n",
                  " ",  mi->calib_white);
  DBG(1, "  Max calib space%16s: %d\n", " ", mi->calib_space);
  DBG(1, "  Number of lens%17s: %d\n", " ", mi->nlens);
  DBG(1, "  Max number of windows%10s: %d\n", " ", mi->nwindows);
  DBG(1, "  Shading transfer function%6s: %d\n", " ",mi->shtrnsferequ);
  DBG(1, "  Red balance%20s: %d\n", " ", mi->balance[0]);
  DBG(1, "  Green balance%18s: %d\n", " ", mi->balance[1]);
  DBG(1, "  Blue balance%19s: %d\n", " " , mi->balance[2]);
  DBG(1, "  Buffer type%20s: %s\n",
                  " ",  mi->buftype ? "Ping-Pong" : "Ring");
  DBG(1, "  FEPROM%25s: %s\n", " ", mi->feprom ? "Yes" : "No");
                  
  md_dump_clear = 0;
  return SANE_STATUS_GOOD;
}
/*---------- get_calib_params() ----------------------------------------------*/

static void
get_calib_params(Microtek2_Scanner *ms)
{
    Microtek2_Device *md;
    Microtek2_Info *mi;


    DBG(30, "get_calib_params: handle=%p\n", ms);

    md = ms->dev;
    mi = &md->info[0];   /* must be changed */

    ms->x_resolution_dpi = mi->opt_resolution;
    /* this is somehow arbitrary */
    ms->y_resolution_dpi =  mi->opt_resolution;
    ms->x1_dots = 0;
    ms->y1_dots = mi->calib_white;
    ms->width_dots = mi->geo_width;
    ms->height_dots = mi->calib_space;

    /* experimental */
    ms->height_dots = 18;

#if 0
    ms->height_dots = (int) ((double) mi->calib_space
                      / ( (double) mi->opt_resolution
                      / (double) ms->y_resolution_dpi));
#endif

    ms->mode = MS_MODE_COLOR;
    if ( mi->depth & MI_HASDEPTH_12 )
        ms->depth = 12;
    else if ( mi->depth & MI_HASDEPTH_10 )
        ms->depth = 10;
    else
        ms->depth = 8;
    ms->stay = 1;
    ms->stay = 0;
    ms->rawdat = 1;
    ms->quality = 0;
    ms->fastscan = 0;
    ms->scan_source = 0;
    ms->brightness_m = ms->brightness_r = ms->brightness_g =
                       ms->brightness_b = 128;
    ms->exposure_m = ms->exposure_r = ms->exposure_g = ms->exposure_b = 0;
    ms->contrast_m = ms->contrast_r = ms->contrast_g = ms->contrast_b = 128;
    ms->shadow_m = ms->shadow_r = ms->shadow_g = ms->shadow_b = 0;
    ms->midtone_m = ms->midtone_r = ms->midtone_g = ms->midtone_b = 128;
    ms->highlight_m = ms->highlight_r = ms->highlight_g = ms->highlight_b = 255;

    return;
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
    /* If lineart_fake is set to !=0, we need the parameters for a */
    /* grayscale scan, because the scanner has no lineart mode */

    Microtek2_Device *md;
    Microtek2_Info *mi;

    DBG(30, "get_scan_mode_and_depth: handle=%p\n", ms);

    md = ms->dev;
    mi = &md->info[md->scan_source];

    if ( strcmp(ms->val[OPT_MODE].s, MD_MODESTRING_COLOR) == 0 )
        *mode = MS_MODE_COLOR;
    else if ( strcmp(ms->val[OPT_MODE].s, MD_MODESTRING_GRAY) == 0 )
        *mode = MS_MODE_GRAY;
    else if ( strcmp(ms->val[OPT_MODE].s, MD_MODESTRING_HALFTONE) == 0)
        *mode = MS_MODE_HALFTONE;
    else if ( strcmp(ms->val[OPT_MODE].s, MD_MODESTRING_LINEART) == 0 )
      {
        if ( MI_LINEART_NONE(mi->scanmode)
             || ms->val[OPT_AUTOADJUST].w == SANE_TRUE )
            *mode = MS_MODE_LINEARTFAKE;
        else
            *mode = MS_MODE_LINEART;
      }
    else
      {
        DBG(1, "get_scan_mode_and_depth: Unknown mode %s\n",
                ms->val[OPT_MODE].s);
        return SANE_STATUS_INVAL;
      }

    if ( strcmp(ms->val[OPT_MODE].s, MD_MODESTRING_COLOR) == 0
         || strcmp(ms->val[OPT_MODE].s, MD_MODESTRING_GRAY) == 0 )
      {
        if ( ms->val[OPT_BITDEPTH].w == MD_DEPTHVAL_12 )
          {
            *depth = 12;
            *bits_per_pixel_in = *bits_per_pixel_out = 16;
          }
        else if ( ms->val[OPT_BITDEPTH].w == MD_DEPTHVAL_10 )
          {
            *depth = 10;
            *bits_per_pixel_in = *bits_per_pixel_out = 16;
          }
        else if ( ms->val[OPT_BITDEPTH].w ==  MD_DEPTHVAL_8 )
          {
            *depth = 8;
            *bits_per_pixel_in = *bits_per_pixel_out = 8;
          }
        else if ( ms->val[OPT_MODE].w == MD_DEPTHVAL_4 )
          {
            *depth = 4;
            *bits_per_pixel_in = 4;
            *bits_per_pixel_out = 8;
          }
      }
    else if ( strcmp(ms->val[OPT_MODE].s, MD_MODESTRING_HALFTONE) == 0  )
      {
        *depth = 1;
        *bits_per_pixel_in = *bits_per_pixel_out = 1;
      }
    else                   /* lineart */
      {
        *bits_per_pixel_out = 1;
        if ( MI_LINEART_NONE(mi->scanmode)
             || ms->val[OPT_AUTOADJUST].w == SANE_TRUE )
          {
            *depth = 8;
            *bits_per_pixel_in = 8;
          }
        else
          {
            *depth = 1;
            *bits_per_pixel_in = 1;
          }
      }

#if 0
    if ( ms->val[OPT_PREVIEW].w == SANE_TRUE )
      {
        if ( *depth > 8 )
          {
            *depth = 8;
            *bits_per_pixel_in = *bits_per_pixel_out = 8;
          }
      }
#endif

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

    /* turn off the lamp during a scan */
    if ( ms->val[OPT_LIGHTLID35].w == SANE_TRUE )
        ms->lightlid35 = 1;
    else
        ms->lightlid35 = 0;

    /* automatic adjustment of threshold */
    if ( ms->val[OPT_AUTOADJUST].w == SANE_TRUE)
        ms->auto_adjust = 1;
    else
        ms->auto_adjust = 0;

    /* color calibration by backend */
    if ( ms->val[OPT_CALIB_BACKEND].w == SANE_TRUE )
        ms->calib_backend = 1;
    else
        ms->calib_backend = 0;

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
    ms->exposure_m = (u_int8_t) (ms->val[OPT_EXPOSURE].w / 2);
    ms->exposure_r = (u_int8_t) (ms->val[OPT_EXPOSURE_R].w / 2);
    ms->exposure_g = (u_int8_t) (ms->val[OPT_EXPOSURE_G].w / 2);
    ms->exposure_b = (u_int8_t) (ms->val[OPT_EXPOSURE_B].w / 2);

    ms->gamma_mode = strdup( (char *) ms->val[OPT_GAMMA_MODE].s);

    return SANE_STATUS_GOOD;
}


/*---------- get_lut_size() --------------------------------------------------*/

static SANE_Status
get_lut_size(Microtek2_Info *mi, int *max_lut_size, int *lut_entry_size)
{
    /* returns the maximum lookup table size. A device might indicate */
    /* several lookup table sizes. */

    DBG(30, "get_lut_size: mi=%p\n", mi);

    *max_lut_size = 0;
    *lut_entry_size = 0;

    /* Normally this function is used for both gamma and shading tables */
    /* If, however, the device indicates, that it does not support */
    /* gamma tables, we set these values as if the device has a maximum */
    /* bitdepth of 12, and these values are only used to determine the */
    /* size of the shading table */
    if ( MI_LUTCAP_NONE(mi->lut_cap) )
      {
        *max_lut_size = 4096;
        *lut_entry_size = 2;
        return SANE_STATUS_GOOD;
      }

    if ( mi->lut_cap & MI_LUTCAP_256B )
      {
        *max_lut_size = 256;
        *lut_entry_size = 1;
      }
    if ( mi->lut_cap & MI_LUTCAP_1024B )
      {
        *max_lut_size = 1024;
        *lut_entry_size = 1;
      }
    if ( mi->lut_cap & MI_LUTCAP_1024W )
      {
        *max_lut_size = 1024;
        *lut_entry_size = 2;
      }
    if ( mi->lut_cap & MI_LUTCAP_4096B )
      {
        *max_lut_size = 4096;
        *lut_entry_size = 1;
      }
    if ( mi->lut_cap & MI_LUTCAP_4096W )
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
    /* whereas the option descriptors and option values that never */
    /* change are not */

    SANE_Option_Descriptor *sod;
    SANE_Status status;
    Microtek2_Option_Value *val;
    Microtek2_Device *md;
    Microtek2_Info *mi;
    int tablesize;
    int option_size;
    int max_gamma_value;
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
    if ( mi->option_device & MI_OPTDEV_STRIPE )
      {
        md->scansource_list[++i] = (SANE_String) MD_SOURCESTRING_STRIPE;
        if ( current_scan_source == MD_SOURCE_STRIPE )
            val[OPT_SOURCE].s = (SANE_String) strdup(md->scansource_list[i]);
      }

    /* Comment this out as long as I do not know in which bit */
    /* it is indicated, whether a slide adapter is connected */
#if 0
    if ( mi->option_device & MI_OPTDEV_SLIDE )
      {
        md->scansource_list[++i] = (SANE_String) MD_SOURCESTRING_SLIDE;
        if ( current_scan_source == MD_SOURCE_SLIDE )
            val[OPT_SOURCE].s = (SANE_String) strdup(md->scansource_list[i]);
      }
#endif

    md->scansource_list[++i] = NULL;

    /* Scan mode */
    if ( val[OPT_MODE].s )
        free((void *) val[OPT_MODE].s);

    i = 0;
    if ( (mi->scanmode & MI_HASMODE_COLOR) )
      {
        md->scanmode_list[i] = (SANE_String) MD_MODESTRING_COLOR;
        val[OPT_MODE].s = strdup(md->scanmode_list[i]);
        ++i;
      }

    if ( mi->scanmode & MI_HASMODE_GRAY )
      {
        md->scanmode_list[i] = (SANE_String) MD_MODESTRING_GRAY;
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

    /* Always enable a lineart mode. Some models (X6, FW 1.40) say */
    /* that they have no lineart mode. In this case we will do a grayscale */
    /* scan and convert it to onebit data */
    md->scanmode_list[i] = (SANE_String) MD_MODESTRING_LINEART;
    if ( ! (mi->scanmode & MI_HASMODE_COLOR )
        && ! (mi->scanmode & MI_HASMODE_GRAY )
        && ! (mi->scanmode & MI_HASMODE_HALFTONE ) )
        val[OPT_MODE].s = strdup(md->scanmode_list[i]);
    ++i;
    md->scanmode_list[i] = NULL;


    /* bitdepth */
    i = 0;

#if 0
    if ( mi->depth & MI_HASDEPTH_NIBBLE )
        md->bitdepth_list[++i] = (SANE_Int) MD_DEPTHVAL_4;
#endif

    md->bitdepth_list[++i] = (SANE_Int) MD_DEPTHVAL_8;
    if ( mi->depth & MI_HASDEPTH_10 )
        md->bitdepth_list[++i] = (SANE_Int) MD_DEPTHVAL_10;
    if ( mi->depth & MI_HASDEPTH_12 )
        md->bitdepth_list[++i] = (SANE_Int) MD_DEPTHVAL_12;
    md->bitdepth_list[0] = i;
    if (  md->bitdepth_list[1] == (SANE_Int) MD_DEPTHVAL_8 )
        val[OPT_BITDEPTH].w = md->bitdepth_list[1];
    else
        val[OPT_BITDEPTH].w = md->bitdepth_list[2];

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
    md->gammamode_list[i++] = (SANE_String) MD_GAMMAMODE_LINEAR;
    md->gammamode_list[i++] = (SANE_String) MD_GAMMAMODE_SCALAR;
    md->gammamode_list[i++] = (SANE_String) MD_GAMMAMODE_CUSTOM;
    if ( val[OPT_GAMMA_MODE].s )
        free((void *) val[OPT_GAMMA_MODE].s);
    val[OPT_GAMMA_MODE].s = strdup(md->gammamode_list[0]);

    md->gammamode_list[i] = NULL;

    /* bind gamma */
    val[OPT_GAMMA_BIND].w = SANE_TRUE;
    val[OPT_GAMMA_SCALAR].w = MD_GAMMA_DEFAULT;
    val[OPT_GAMMA_SCALAR_R].w = MD_GAMMA_DEFAULT;
    val[OPT_GAMMA_SCALAR_G].w = MD_GAMMA_DEFAULT;
    val[OPT_GAMMA_SCALAR_B].w = MD_GAMMA_DEFAULT;

    /* If the device supports gamma tables, we allocate memory according */
    /* to lookup table capabilities, otherwise we allocate 4096 elements */
    /* which is sufficient for a color depth of 12. If the device */
    /* does not support gamma tables, we fill the table according to */
    /* the actual bit depth, i.e. 256 entries with a range of 0..255 */
    /* if the actual bit depth is 8, for example. This will hopefully*/
    /* make no trouble if the bit depth is 1. */
    if ( md->model_flags & MD_NO_GAMMA )
      {
        tablesize = 4096;
        option_size = (int) pow(2.0, (double) val[OPT_BITDEPTH].w );
        max_gamma_value = option_size - 1;
      }
    else
      {
        tablesize = md->max_lut_size;
        option_size = tablesize;
        max_gamma_value = md->max_lut_size - 1;
      }

    for ( color = 0; color < 4; color++ )
      {
        /* index 0 is used if bind gamma == true, index 1 to 3 */
        /* if bind gamma == false */
        if ( md->custom_gamma_table[color] )
            free((void *) md->custom_gamma_table[color]);
        md->custom_gamma_table[color] =
                              (SANE_Int *) malloc(tablesize * sizeof(SANE_Int));
        if ( md->custom_gamma_table[color] == NULL )
          {
            DBG(1, "init_options: malloc for custom gamma table failed\n");
            return SANE_STATUS_NO_MEM;
          }

        for ( i = 0; i < max_gamma_value; i++ )
            md->custom_gamma_table[color][i] = i;
      }

    md->custom_gamma_range.min = 0;
    md->custom_gamma_range.max =  max_gamma_value;
    md->custom_gamma_range.quant = 1;

    sod[OPT_GAMMA_CUSTOM].size = option_size * sizeof (SANE_Int);
    sod[OPT_GAMMA_CUSTOM_R].size = option_size * sizeof (SANE_Int);
    sod[OPT_GAMMA_CUSTOM_G].size = option_size * sizeof (SANE_Int);
    sod[OPT_GAMMA_CUSTOM_B].size = option_size * sizeof (SANE_Int);

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

    /* special options */
    val[OPT_RESOLUTION_BIND].w = SANE_TRUE;

    /* enable/disable option for backtracking */
    val[OPT_DISABLE_BACKTRACK].w = SANE_FALSE;

    /* enable/disable calibration by backend */
    val[OPT_CALIB_BACKEND].w = SANE_FALSE;

    /* turn off the lamp during a scan */
    val[OPT_LIGHTLID35].w = SANE_FALSE;

    /* auto adjustment of threshold during a lineart scan */
    val[OPT_AUTOADJUST].w = SANE_FALSE;

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

        /* Scan source */
        sod[OPT_SOURCE].name = SANE_NAME_SCAN_SOURCE;
        sod[OPT_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
        sod[OPT_SOURCE].desc = SANE_DESC_SCAN_SOURCE;
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

        /* Bit depth */
        sod[OPT_BITDEPTH].name = SANE_NAME_BIT_DEPTH;
        sod[OPT_BITDEPTH].title = SANE_TITLE_BIT_DEPTH;
        sod[OPT_BITDEPTH].desc = SANE_DESC_BIT_DEPTH;
        sod[OPT_BITDEPTH].type = SANE_TYPE_INT;
        sod[OPT_BITDEPTH].unit = SANE_UNIT_BIT;
        sod[OPT_BITDEPTH].size = sizeof(SANE_Int);
        /* if we have only 8 bit color deactivate this option */
        if ( md->bitdepth_list[0] == 1 )
            sod[OPT_BITDEPTH].cap |= SANE_CAP_INACTIVE;
        sod[OPT_BITDEPTH].constraint_type = SANE_CONSTRAINT_WORD_LIST;
        sod[OPT_BITDEPTH].constraint.word_list = md->bitdepth_list;

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

        /* automatically adjust threshold for a lineart scan */
        sod[OPT_AUTOADJUST].name = M_NAME_AUTOADJUST;
        sod[OPT_AUTOADJUST].title = M_TITLE_AUTOADJUST;
        sod[OPT_AUTOADJUST].desc = M_DESC_AUTOADJUST;
        sod[OPT_AUTOADJUST].type = SANE_TYPE_BOOL;
        sod[OPT_AUTOADJUST].size = sizeof(SANE_Bool);
        sod[OPT_AUTOADJUST].constraint_type = SANE_CONSTRAINT_NONE;
        if ( strncmp(md->opts.auto_adjust, "off", 3) == 0 )
            sod[OPT_AUTOADJUST].cap |= SANE_CAP_INACTIVE;

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
        sod[OPT_GAMMA_SCALAR].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_SCALAR].constraint.range = &md->scalar_gamma_range;

        sod[OPT_GAMMA_SCALAR_R].name = M_NAME_GAMMA_SCALAR_R;
        sod[OPT_GAMMA_SCALAR_R].title = M_TITLE_GAMMA_SCALAR_R;
        sod[OPT_GAMMA_SCALAR_R].desc = M_DESC_GAMMA_SCALAR_R;
        sod[OPT_GAMMA_SCALAR_R].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_SCALAR_R].constraint.range = &md->scalar_gamma_range;

        sod[OPT_GAMMA_SCALAR_G].name = M_NAME_GAMMA_SCALAR_G;
        sod[OPT_GAMMA_SCALAR_G].title = M_TITLE_GAMMA_SCALAR_G;
        sod[OPT_GAMMA_SCALAR_G].desc = M_DESC_GAMMA_SCALAR_G;
        sod[OPT_GAMMA_SCALAR_G].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_SCALAR_G].constraint.range = &md->scalar_gamma_range;

        sod[OPT_GAMMA_SCALAR_B].name = M_NAME_GAMMA_SCALAR_B;
        sod[OPT_GAMMA_SCALAR_B].title = M_TITLE_GAMMA_SCALAR_B;
        sod[OPT_GAMMA_SCALAR_B].desc = M_DESC_GAMMA_SCALAR_B;
        sod[OPT_GAMMA_SCALAR_B].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_SCALAR_B].constraint.range = &md->scalar_gamma_range;

        sod[OPT_GAMMA_CUSTOM].name = SANE_NAME_GAMMA_VECTOR;
        sod[OPT_GAMMA_CUSTOM].title = SANE_TITLE_GAMMA_VECTOR;
        sod[OPT_GAMMA_CUSTOM].desc = SANE_DESC_GAMMA_VECTOR;
        sod[OPT_GAMMA_CUSTOM].type = SANE_TYPE_INT;
        sod[OPT_GAMMA_CUSTOM].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM].size = option_size * sizeof (SANE_Int);
        sod[OPT_GAMMA_CUSTOM].constraint.range = &md->custom_gamma_range;

        sod[OPT_GAMMA_CUSTOM_R].name = SANE_NAME_GAMMA_VECTOR_R;
        sod[OPT_GAMMA_CUSTOM_R].title = SANE_TITLE_GAMMA_VECTOR_R;
        sod[OPT_GAMMA_CUSTOM_R].desc = SANE_DESC_GAMMA_VECTOR_R;
        sod[OPT_GAMMA_CUSTOM_R].type = SANE_TYPE_INT;
        sod[OPT_GAMMA_CUSTOM_R].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM_R].size = option_size * sizeof (SANE_Int);
        sod[OPT_GAMMA_CUSTOM_R].constraint.range = &md->custom_gamma_range;

        sod[OPT_GAMMA_CUSTOM_G].name = SANE_NAME_GAMMA_VECTOR_G;
        sod[OPT_GAMMA_CUSTOM_G].title = SANE_TITLE_GAMMA_VECTOR_G;
        sod[OPT_GAMMA_CUSTOM_G].desc = SANE_DESC_GAMMA_VECTOR_G;
        sod[OPT_GAMMA_CUSTOM_G].type = SANE_TYPE_INT;
        sod[OPT_GAMMA_CUSTOM_G].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM_G].size = option_size * sizeof (SANE_Int);
        sod[OPT_GAMMA_CUSTOM_G].constraint.range = &md->custom_gamma_range;

        sod[OPT_GAMMA_CUSTOM_B].name = SANE_NAME_GAMMA_VECTOR_B;
        sod[OPT_GAMMA_CUSTOM_B].title = SANE_TITLE_GAMMA_VECTOR_B;
        sod[OPT_GAMMA_CUSTOM_B].desc = SANE_DESC_GAMMA_VECTOR_B;
        sod[OPT_GAMMA_CUSTOM_B].type = SANE_TYPE_INT;
        sod[OPT_GAMMA_CUSTOM_B].cap |= SANE_CAP_INACTIVE;
        sod[OPT_GAMMA_CUSTOM_B].size = option_size * sizeof (SANE_Int);
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

        sod[OPT_EXPOSURE_R].name = SANE_NAME_SCAN_EXPOS_TIME_R;
        sod[OPT_EXPOSURE_R].title = SANE_TITLE_SCAN_EXPOS_TIME_R;
        sod[OPT_EXPOSURE_R].desc = "Allows to lengthen the exposure time "
                                   "for the red channel";
        sod[OPT_EXPOSURE_R].type = SANE_TYPE_INT;
        sod[OPT_EXPOSURE_R].unit = SANE_UNIT_PERCENT;
        sod[OPT_EXPOSURE_R].size = sizeof(SANE_Int);
        sod[OPT_EXPOSURE_R].constraint.range = &md->exposure_range;

        sod[OPT_EXPOSURE_G].name = SANE_NAME_SCAN_EXPOS_TIME_G;
        sod[OPT_EXPOSURE_G].title = SANE_TITLE_SCAN_EXPOS_TIME_G;
        sod[OPT_EXPOSURE_G].desc = "Allows to lengthen the exposure time "
                                   "for the green channel";
        sod[OPT_EXPOSURE_G].type = SANE_TYPE_INT;
        sod[OPT_EXPOSURE_G].unit = SANE_UNIT_PERCENT;
        sod[OPT_EXPOSURE_G].size = sizeof(SANE_Int);
        sod[OPT_EXPOSURE_G].constraint.range = &md->exposure_range;

        sod[OPT_EXPOSURE_B].name = SANE_NAME_SCAN_EXPOS_TIME_B;
        sod[OPT_EXPOSURE_B].title = SANE_TITLE_SCAN_EXPOS_TIME_B;
        sod[OPT_EXPOSURE_B].desc = "Allows to lengthen the exposure time "
                                   "for the blue channel";
        sod[OPT_EXPOSURE_B].type = SANE_TYPE_INT;
        sod[OPT_EXPOSURE_B].unit = SANE_UNIT_PERCENT;
        sod[OPT_EXPOSURE_B].size = sizeof(SANE_Int);
        sod[OPT_EXPOSURE_B].constraint.range = &md->exposure_range;

        /* The Scan Mode Group */
        sod[OPT_SPECIAL].title = "Special options";
        sod[OPT_SPECIAL].type = SANE_TYPE_GROUP;
        sod[OPT_SPECIAL].desc = "";
        sod[OPT_SPECIAL].cap = SANE_CAP_ADVANCED;
        sod[OPT_SPECIAL].constraint_type = SANE_CONSTRAINT_NONE;

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
        if ( strncmp(md->opts.no_backtracking, "off", 3) == 0 )
            sod[OPT_DISABLE_BACKTRACK].cap |= SANE_CAP_INACTIVE;

        /* calibration by driver */
        sod[OPT_CALIB_BACKEND].name = M_NAME_CALIBBACKEND;
        sod[OPT_CALIB_BACKEND].title = M_TITLE_CALIBBACKEND;
        sod[OPT_CALIB_BACKEND].desc = M_DESC_CALIBBACKEND;
        sod[OPT_CALIB_BACKEND].type = SANE_TYPE_BOOL;
        sod[OPT_CALIB_BACKEND].size = sizeof(SANE_Bool);
        sod[OPT_CALIB_BACKEND].cap |= SANE_CAP_ADVANCED;
        sod[OPT_CALIB_BACKEND].constraint_type = SANE_CONSTRAINT_NONE;
        if ( strncmp(md->opts.backend_calibration, "off", 3) == 0 )
            sod[OPT_CALIB_BACKEND].cap |= SANE_CAP_INACTIVE;

        /* turn off the lamp of the flatbed during a scan */
        sod[OPT_LIGHTLID35].name = M_NAME_LIGHTLID35;
        sod[OPT_LIGHTLID35].title = M_TITLE_LIGHTLID35;
        sod[OPT_LIGHTLID35].desc = M_DESC_LIGHTLID35;
        sod[OPT_LIGHTLID35].type = SANE_TYPE_BOOL;
        sod[OPT_LIGHTLID35].size = sizeof(SANE_Bool);
        sod[OPT_LIGHTLID35].cap |= SANE_CAP_ADVANCED;
        sod[OPT_LIGHTLID35].constraint_type = SANE_CONSTRAINT_NONE;
        if ( strncmp(md->opts.lightlid35, "off", 3) == 0 )
            sod[OPT_LIGHTLID35].cap |= SANE_CAP_INACTIVE;

        /* toggle the lamp of the flatbed */
        sod[OPT_TOGGLELAMP].name = M_NAME_TOGGLELAMP;
        sod[OPT_TOGGLELAMP].title = M_TITLE_TOGGLELAMP;
        sod[OPT_TOGGLELAMP].desc = M_DESC_TOGGLELAMP;
        sod[OPT_TOGGLELAMP].type = SANE_TYPE_BUTTON;
        sod[OPT_TOGGLELAMP].size = 0;
        sod[OPT_TOGGLELAMP].cap |= SANE_CAP_ADVANCED;
        sod[OPT_TOGGLELAMP].constraint_type = SANE_CONSTRAINT_NONE;
        if ( strncmp(md->opts.toggle_lamp, "off", 3) == 0 )
            sod[OPT_TOGGLELAMP].cap |= SANE_CAP_INACTIVE;

      }

    status = set_option_dependencies(sod, val);
    if ( status != SANE_STATUS_GOOD )
        return status;

    return SANE_STATUS_GOOD;
}


/*---------- lineartfake_copy_pixels() ---------------------------------------*/

static SANE_Status
lineartfake_copy_pixels(u_int8_t *from,
                        u_int32_t pixels,
                        u_int8_t threshold,
                        int right_to_left,
                        FILE *fp)
{
    u_int32_t pixel;
    u_int32_t bit;
    u_int8_t dest;
    u_int8_t val;
    int step;


    DBG(30, "lineartfake_copy_pixels: from=%p,pixels=%d,threshold=%d,file=%p\n",
             from, pixels, threshold, fp);

    bit = 0;
    dest = 0;
    step = right_to_left == 1 ? -1 : 1;
    for ( pixel = 0; pixel < pixels; pixel++ )
      {
        val = ( *from < threshold ) ? 1 : 0;
        dest = ( dest << 1 ) | val;
        bit = ++bit % 8;
        if ( bit == 0 )                   /* 8 input bytes processed */
          {
            fputc((char) dest, fp);
            dest = 0;
          }
        from += step;
      }

    if ( bit != 0 )
      {
        dest <<= 7 - bit;
        fputc((char) dest, fp);
      }

    return SANE_STATUS_GOOD;
}


/*---------- lineartfake_proc_data() -----------------------------------------*/

static SANE_Status
lineartfake_proc_data(Microtek2_Scanner *ms)
{
    Microtek2_Device *md;
    Microtek2_Info *mi;
    SANE_Status status;
    u_int8_t *from;
    int right_to_left;


    DBG(30, "lineartfake_proc_data: lines=%d, bpl=%d, ppl=%d, depth=%d\n",
             ms->src_lines_to_read, ms->bpl, ms->ppl, ms->depth);

    md = ms->dev;
    mi = &md->info[md->scan_source];
    right_to_left = mi->direction & MI_DATSEQ_RTOL;

    if ( right_to_left == 1 )
      {
        from = ms->buf.src_buf + ms->ppl - 1;
        if ( ms->ppl % 2 == 1 )
            /* we have a trailing junk byte */
            --from;
      }
    else
        from = ms->buf.src_buf;

    do
      {
        status = lineartfake_copy_pixels(from,
                                         ms->ppl,
                                         ms->threshold,
                                         right_to_left,
                                         ms->fp);
        if ( status != SANE_STATUS_GOOD )
            return status;

        from += ms->bpl;
        --ms->src_lines_to_read;
      } while ( ms->src_lines_to_read > 0 );

    return SANE_STATUS_GOOD;
}


/*---------- lplconcat_copy_pixels() -----------------------------------------*/

static SANE_Status
lplconcat_copy_pixels(Microtek2_Scanner *ms,
                      u_int8_t **from,
                      int right_to_left,
                      int gamma_by_backend)
{
    Microtek2_Device *md;
    Microtek2_Info *mi;
    u_int32_t pixels;
    u_int32_t pixel;
    u_int16_t val16;
    u_int8_t val8;
    u_int8_t *gamma[3];
    float s_d;             /* dark shading pixel */
    float s_w;             /* white shading pixel */
    float f[3];            /* balance factor */
    int cs[3];             /* take color sequence in shading */
                           /*  data into account */
    int depth;
    int color;
    int step;
    int i;


    DBG(30, "lplconcat_copy_pixels: ms=%p, righttoleft=%d, gamma=%d,\n",
             ms, right_to_left, gamma_by_backend);

    md = ms->dev;
    mi = &md->info[md->scan_source];

    pixels = ms->ppl;
    depth = ms->depth;

    step = ( right_to_left == 1 ) ? -1 : 1;
    if ( gamma_by_backend )
      {
        i =  ( depth > 8 ) ? 2 : 1;
        for ( color = 0; color < 3; color++ )
            gamma[color] = ms->gamma_table + i * (int) pow(2.0, (double) depth);
      }

    /* experimental */
    for (color = 0; color < 3; color++ )
      {
        f[color] = (float) mi->balance[color] / 256.0;
        cs[color] = mi->color_sequence[color];
      }

    if ( depth > 8 )
      {
        int scale1;
        int scale2;


        step *= 2;
        scale1 = 16 - depth;
        scale2 = 2 * depth - 16;
        for ( pixel = 0; pixel < pixels; pixel++ )
          {
            for ( color = 0; color < 3; color++ )
              {
                val16 = *(u_int16_t *) from[color];

                /* experimental */
                /* if the device uses read_control_bit we must apply */
                /* a shading correction ourselves according to the */
                /* control bits */
                if ( md->model_flags & MD_READ_CONTROL_BIT
                     && ms->calib_backend )
                  {
                    s_d = (float) *((u_int16_t *) ms->condensed_shading_d +
                          cs[color] * pixels + pixel);
                    s_w = (float) *((u_int16_t *) ms->condensed_shading_w +
                          cs[color] * pixels + pixel);
                    if ( val16 < (u_int16_t) s_d )
                        val16 = (u_int16_t) s_d;
                    val16 = (u_int16_t) ((4096.0
                                          * ((float) val16 - s_d)
                                          / (s_w - s_d))
                                          * f[color] );
                  }

                /* apply gamma if needed */
                if ( gamma_by_backend )
                    val16 = *(u_int16_t *) gamma[color + val16];
                val16 = ( val16 << scale1 ) | ( val16 >> scale2 );
                fwrite((void *) &val16, 2, 1, ms->fp);
                from[color] += step;
              }
          }
      }
    else if ( depth == 8 )
      {
        for ( pixel = 0; pixel < pixels; pixel++ )
          {
            for ( color = 0; color < 3; color++ )
              {
                val8 = *from[color];
                /* experimental */
                if ( (md->model_flags & MD_READ_CONTROL_BIT)
                     && ms->calib_backend )
                  {
                    s_d = (float) *((u_int16_t *) ms->condensed_shading_d +
                                                  cs[color] * pixels + pixel);
                    s_w = (float) *((u_int16_t *) ms->condensed_shading_w +
                                                  cs[color] * pixels + pixel);
                    if ( val8 < (u_int8_t) s_d )
                        val8 = (u_int8_t) s_d;
                    /* experimental */
                    val8 = (u_int8_t) MIN(255.0, (4096.0
                                      * (((float) val8) - s_d )
                                      / (s_w - s_d))
                                      * f[color] );
                  }

                /* apply gamma correction if needed */
                if ( gamma_by_backend )
                    val8 = gamma[color][val8];

                fputc((char) val8, ms->fp);
                from[color] += step;
              }
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
    Microtek2_Device *md;
    Microtek2_Info *mi;
    u_int32_t line;
    u_int8_t *from[3];
    u_int8_t *save_from[3];
    int color;
    int bpp;
    int pad;
    int gamma_by_backend;
    int right_to_left;       /* 0: left to right, right to left */


    DBG(30, "lplconcat_proc_data: ms=%p\n", ms);

    /* This data format seems to honour the color sequence indicator */

    md = ms->dev;
    mi = &md->info[md->scan_source];

    bpp = ms->bits_per_pixel_out / 8;
    pad = (ms->ppl * bpp) % 2;
    right_to_left = mi->direction & MI_DATSEQ_RTOL;
    gamma_by_backend =  md->model_flags & MD_NO_GAMMA ? 1 : 0;

    if ( right_to_left == 1 )
      {
        for ( color = 0; color < 3; color++ )
          {
            from[color] = ms->buf.src_buf
                          + ( mi->color_sequence[color] + 1 ) * ms->ppl * bpp
                          - bpp
                          - pad;
          }
      }
    else
        for ( color = 0; color < 3; color++ )
            from[color] = ms->buf.src_buf + mi->color_sequence[color] * ms->ppl;


    for ( line = 0; line < (u_int32_t)ms->src_lines_to_read; line++ )
      {
        for ( color = 0 ; color < 3; color++ )
            save_from[color] = from[color];

        status = lplconcat_copy_pixels(ms,
                                       from,
                                       right_to_left,
                                       gamma_by_backend);
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
gray_copy_pixels(u_int8_t *from,
                 u_int32_t pixels,
                 int depth,
                 FILE *fp,
                 int right_to_left,
                 u_int8_t *gamma_table,
                 int gamma_by_backend)
{
    u_int32_t pixel;
    int step;


    DBG(30, "gray_copy_pixels: pixels=%d, from=%p, fp=%p, depth=%d\n",
             pixels, from, fp, depth);

    step = right_to_left == 1 ? -1 : 1;
    if ( depth > 8 )
      {
        int scale1;
        int scale2;
        step *= 2;
        scale1 = 16 - depth;
        scale2 = 2 * depth - 16;
        for ( pixel = 0; pixel < pixels; pixel++ )
          {
            u_int16_t val16;

            val16 = *(u_int16_t *) from;
            /* apply gamma if needed */
            if ( gamma_by_backend )
                val16 = *((u_int16_t *) gamma_table + val16);
            val16 = ( val16 << scale1 ) | ( val16 >> scale2 );
            fwrite((void *) &val16, 2, 1, fp);
            from += step;
          }
      }
    else if ( depth == 8 )
      {
        u_int8_t val8;

        for ( pixel = 0; pixel < pixels; pixel++ )
          {
            val8 = *from;
            if ( gamma_by_backend )
                val8 =  gamma_table[val8];
            fputc((char) val8, fp);
            from += step;
          }
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
    Microtek2_Device *md;
    Microtek2_Info *mi;
    u_int8_t *from;
    int gamma_by_backend;
    int right_to_left;   /* for scanning direction */


    DBG(30, "gray_proc_data: lines=%d, bpl=%d, ppl=%d, depth=%d\n",
             ms->src_lines_to_read, ms->bpl, ms->ppl, ms->depth);

    md = ms->dev;
    mi = &md->info[0];
    from = ms->buf.src_buf;

    gamma_by_backend =  md->model_flags & MD_NO_GAMMA ? 1 : 0;

    right_to_left = mi->direction & MI_DATSEQ_RTOL;
    if ( right_to_left == 1 )
      {
        from = ms->buf.src_buf + ms->ppl - (ms->depth + 7) / 8;
        if ( ms->depth <= 8 && ms->ppl % 2 == 1 )
            /* we have a trailing junk byte */
            --from;
      }

    do
      {
        status = gray_copy_pixels(from,
                                  ms->ppl,
                                  ms->depth,
                                  ms->fp,
                                  right_to_left,
                                  ms->gamma_table,
                                  gamma_by_backend);
        if ( status != SANE_STATUS_GOOD )
            return status;

        from += ms->bpl;
        --ms->src_lines_to_read;
      } while ( ms->src_lines_to_read > 0 );

    return SANE_STATUS_GOOD;
}


#if 0
/*---------- gray_set_exposure() ---------------------------------------------*/

static void
gray_set_exposure(u_int8_t *from,
                  u_int32_t pixels,
                  u_int8_t depth,
                  u_int8_t exposure)
{
    u_int32_t pixel;
    u_int32_t val32;
    u_int32_t maxval;

    DBG(30, "gray_set_exposure: from=%p, ppl=%d, depth=%d, exp=%d\n",
            from, pixels, depth, exposure);


    if ( depth > 8 )
      {
        maxval = (1 << depth) - 1;
        for ( pixel = 0; pixel < pixels; pixel++ )
          {
            val32 = (u_int32_t) *((u_int16_t *) from + pixel);
            val32 = MIN(val32 + val32 * (2 * exposure / 100), maxval);
            *((u_int16_t *) from + pixel) = (u_int16_t) val32;
          }
      }
    else
      {
        for ( pixel = 0; pixel < pixels; pixel++ )
          {
            val32 = (u_int32_t) from[pixel];
            val32 = MIN(val32 + val32 * (2 * (u_int32_t) exposure / 100), 0xff);

            from[pixel] = (u_int8_t) val32;
          }
      }
    return;
}
#endif


/*---------- parse_config_file() ---------------------------------------------*/

static void
parse_config_file(FILE *fp, Config_Temp **ct)
{
    /* builds a list of device names with associated options from the */
    /* config file for later use, when building the list of devices. */
    /* ct->device = NULL indicates global options (valid for all devices */

    char s[PATH_MAX];
    Config_Options global_opts;
    Config_Temp *hct1;
    Config_Temp *hct2;


    DBG(30, "parse_config_file: fp=%p\n", fp);

    *ct = hct1 = NULL;

    /* first read global options and store them in global_opts */
    /* initialize global_opts with default values */

    global_opts = md_options;

    while ( sanei_config_read(s, sizeof(s), fp) )
      {
        if ( *s == '#' || *s == '\n' )  /* ignore empty lines and comments */
            continue;

        if ( strncmp((char *) sanei_config_skip_whitespace(s),
                     "option ", 7) == 0
          || strncmp((char *) sanei_config_skip_whitespace(s),
                     "option\t", 7) == 0 )

            check_option(s, &global_opts);
        else                /* it is considered a new device */
            break;
      }

    if ( ferror(fp) || feof(fp) )
      {
        if ( ferror(fp) )
            DBG(1, "parse_config_file: fread failed: errno=%d\n", errno);

        return;
      }

    while ( ! feof(fp) && ! ferror(fp) )
      {
        if ( *s == '#' || *s == '\n' )  /* ignore empty lines and comments */
          {
            sanei_config_read(s, sizeof(s), fp);
            continue;
          }

        if ( strncmp((char *) sanei_config_skip_whitespace(s),
                     "option ", 7) == 0
          || strncmp((char *) sanei_config_skip_whitespace(s),
                        "option\t", 7) == 0 )
          {
            /* when we enter this loop for the first time we allocate */
            /* memory, because the line surely contains a device name, */
            /* so hct1 is always != NULL at this point */
            check_option(s, &hct1->opts);
          }


        else                /* it is considered a new device */
          {
            hct2 = (Config_Temp *) malloc(sizeof(Config_Temp));
            if ( hct2 == NULL )
              {
                DBG(1, "parse_config_file: malloc() failed\n");
                return;
              }

            if ( *ct == NULL )   /* first element */
                *ct = hct1 = hct2;

            hct1->next = hct2;
            hct1 = hct2;

            hct1->device = strdup(s);
            hct1->opts = global_opts;
            hct1->next = NULL;
          }
        sanei_config_read(s, sizeof(s), fp);
      }
    /* set filepointer to the beginning of the file */
    fseek(fp, 0L, SEEK_SET);
    return;
}


/*---------- prepare_shading_data() ------------------------------------------*/

static SANE_Status
prepare_shading_data(Microtek2_Scanner *ms, u_int32_t lines, u_int8_t **data)
{
    /* This function calculates one line of black and white shading data */
    /* from the shading image. At the end we have one line, and the */
    /* color sequence is R-G-B. */

    Microtek2_Device *md;
    Microtek2_Info *mi;
    u_int32_t length;
    u_int32_t value;
    int line;
    int color;
    int colseq;
    int i;


    DBG(30, "prepare_shading_data: ms=%p, lines=%d, *data=%p\n",
            ms, lines, *data);


    md = ms->dev;
    mi = &md->info[0];

    get_lut_size(mi, &ms->lut_size, &ms->lut_entry_size);
    length = 3 * ms->lut_entry_size * mi->geo_width;

    if ( *data == NULL )
      {
        *data = (u_int8_t *) malloc(length);
        if ( *data == NULL )
          {
            DBG(1, "prepare_shading_data: malloc for shading table failed\n");
            return SANE_STATUS_NO_MEM;
          }
      }

    switch( mi->data_format )
      {
        case MI_DATAFMT_LPLCONCAT:
          if ( ms->lut_entry_size == 1 )
            {
              DBG(1, "prepare_shading_data: wordsize == 1 unsupported\n");
              return SANE_STATUS_IO_ERROR;
            }
          for ( color = 0; color < 3; color++ )
            {
              colseq = mi->color_sequence[color];

              for ( i = 0; i < mi->geo_width; i++ )
                {
                  value = 0;
                  for ( line = 0; line < (int)lines; line++ )
                      value += *((u_int16_t *) ms->shading_image
                               + line * 3 * mi->geo_width
                               + colseq * mi->geo_width
                               + i);

                  value /= lines;
                  *((u_int16_t *) *data + colseq * mi->geo_width + i) =
                                                 MIN(0xffff, (u_int16_t) value);
                }
            }
          break;

        case MI_DATAFMT_CHUNKY:
          if ( ms->lut_entry_size == 1 )
            {
              DBG(1, "prepare_shading_data: wordsize == 1 unsupported\n");
              return SANE_STATUS_IO_ERROR;
            }
          for ( color = 0; color < 3; color++ )
            {
              for ( i = 0; i < mi->geo_width; i++ )
                {
                  value = 0;
                  for ( line = 0; line < (int)lines; line++ )
                      value += *((u_int16_t *) ms->shading_image
                               + line * 3 * mi->geo_width
                               + 3 * i
                               + color);

                  value /= lines;
                  *((u_int16_t *) *data + color * mi->geo_width + i) =
                                                 MIN(0xffff, (u_int16_t) value);
                }
            }

          break;

        default:
          DBG(1, "prepare_shading_data: Unsupported data format 0x%02x\n",
                  mi->data_format);
          return SANE_STATUS_IO_ERROR;
      }

    return SANE_STATUS_GOOD;
}



/*---------- proc_onebit_data() ----------------------------------------------*/

static SANE_Status
proc_onebit_data(Microtek2_Scanner *ms)
{
    Microtek2_Device *md;
    Microtek2_Info *mi;
    u_int32_t bytes_to_copy;     /* bytes per line to copy */
    u_int32_t line;
    u_int32_t byte;
    u_int32_t ppl;
    u_int8_t *from;
    u_int8_t to;
    int right_to_left;
    int bit;
    int toindex;


    DBG(30, "proc_onebit_data: ms=%p\n", ms);

    md = ms->dev;
    mi = &md->info[md->scan_source];
    from = ms->buf.src_buf;
    bytes_to_copy = ( ms->ppl + 7 ) / 8 ;
    right_to_left = mi->direction & MI_DATSEQ_RTOL;

    DBG(30, "proc_onebit_data: bytes_to_copy=%d, lines=%d\n",
             bytes_to_copy, ms->src_lines_to_read);

    line = 0;
    to = 0;
    do
      {
        /* in onebit mode black and white colors are inverted */
        if ( right_to_left )
          {
            /* If the direction is right_to_left, we must skip some */
            /* trailing bits at the end of the scan line and invert the */
            /* bit sequence. We copy 8 bits into a byte, but these bits */
            /* are normally not byte aligned. */

            /* Determine the position of the first bit to copy */
            ppl = ms->ppl;
            byte = ( ppl + 7 ) / 8 - 1;
            bit = ppl % 8 - 1;
            to = 0;
            toindex = 8;

            while ( ppl > 0 )
              {
                to |= GETBIT(from[byte], bit);
                --toindex;
                if ( toindex == 0 )
                  {
                    fputc( (char) ~to, ms->fp);
                    toindex = 8;
                    to = 0;
                  }
                else
                    to <<= 1;

                --bit;
                if ( bit < 0 )
                  {
                    bit = 7;
                    --byte;
                  }
                --ppl;
              }
            /* print the last byte of the line, if it was not */
            /*  completely filled */
            bit = ms->ppl % 8;
            if ( bit != 0 )
                fputc( (char) ~(to << (7 - bit)), ms->fp);
          }
        else
            for ( byte = 0; byte < bytes_to_copy; byte++ )
                fputc( (char) ~from[byte], ms->fp);

        from += ms->bpl;

      } while ( ++line < (u_int32_t)ms->src_lines_to_read );

    return SANE_STATUS_GOOD;
}


/*---------- reader_process() ------------------------------------------------*/

static SANE_Status
reader_process(Microtek2_Scanner *ms)
{
    SANE_Status status;
    Microtek2_Info *mi;
    Microtek2_Device *md;
    struct SIGACTION act;
    sigset_t sigterm_set;
    static u_int8_t *temp_current = NULL;


    DBG(30, "reader_process: ms=%p\n", ms);

    md = ms->dev;
    mi = &md->info[md->scan_source];
    close(ms->fd[0]);

    sigemptyset (&sigterm_set);
    sigaddset (&sigterm_set, SIGTERM);
    memset (&act, 0, sizeof (act));
    act.sa_handler = signal_handler;
    sigaction (SIGTERM, &act, 0);

    ms->fp = fdopen(ms->fd[1], "w");
    if ( ms->fp == NULL )
      {
        DBG(1, "reader_process: fdopen() failed, errno=%d\n", errno);
        return SANE_STATUS_IO_ERROR;
      }

    if ( ms->auto_adjust == 1 )
      {
        if ( temp_current == NULL )
            temp_current = ms->temporary_buffer;
      }

    while ( ms->src_remaining_lines > 0 )
      {

        ms->src_lines_to_read = MIN(ms->src_remaining_lines, ms->src_max_lines);
        ms->transfer_length = ms->src_lines_to_read * ms->bpl;

        DBG(30, "reader_process: transferlength=%d, lines=%d, linelength=%d, "
                "real_bpl=%d, srcbuf=%p\n", ms->transfer_length,
                 ms->src_lines_to_read, ms->bpl, ms->real_bpl, ms->buf.src_buf);

        sigprocmask (SIG_BLOCK, &sigterm_set, 0);

        status = scsi_read_image(ms, ms->buf.src_buf);
        sigprocmask (SIG_UNBLOCK, &sigterm_set, 0);
        if ( status != SANE_STATUS_GOOD )
            return SANE_STATUS_IO_ERROR;

        ms->src_remaining_lines -= ms->src_lines_to_read;

#if 0
        /* test, output color indicator */
        for ( i = 0 ; i < ms->transfer_length; i = i + ms->bpl)
            DBG(1,"'%c' '%c' '%c'\n", *(ms->buf.src_buf + i),
                    *(ms->buf.src_buf + i + ms->bpl / 3),
                    *(ms->buf.src_buf + i + ms->bpl / 3 * 2));
#endif

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
            case MS_MODE_LINEARTFAKE:
              if ( ms->auto_adjust == 1 )
                  status = auto_adjust_proc_data(ms, &temp_current);
              else
                  status = lineartfake_proc_data(ms);

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
    SANE_Status status;
    Microtek2_Device *md;
    Microtek2_Info *mi;
    char colormap[] = "RGB";
    u_int8_t *from;
    u_int32_t lines_to_deliver;
    int bpp;                    /* bytes per pixel */
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
          }
      }

    ms->buf.free_lines -= ms->src_lines_to_read;
    save_current_src = ms->buf.current_src;
    if ( (SANE_Int)ms->buf.free_lines < ms->src_max_lines )
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

#if 0
    /* if we don´t have a gamma table return immediately */
    if ( ! val[OPT_GAMMA_MODE].s )
       return SANE_STATUS_GOOD;
#endif

    if ( strcmp(val[OPT_MODE].s, MD_MODESTRING_COLOR) == 0 )
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
    else if ( strcmp(val[OPT_MODE].s, MD_MODESTRING_GRAY) == 0 )
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


/*---------- set_exposure() --------------------------------------------------*/

static void
set_exposure(Microtek2_Scanner *ms)
{
    /* This function manipulates the colors according to the exposure time */
    /* settings on models where they are ignored. Currently this seems to */
    /* be the case for all models with the data format ´chunky data´. They */
    /* all have tables with two byte gamma output, so for now we ignore */
    /* gamma tables with one byte output */

    Microtek2_Device *md;
    Microtek2_Info *mi;
    int color;
    int size;
    int depth;
    int maxval;
    u_int32_t val32;
    u_int32_t byte;
    u_int8_t *from;
    u_int8_t exposure;
    u_int8_t exposure_rgb[3];


    DBG(30, "set_exposure: ms=%p\n", ms);

    md = ms->dev;
    mi = &md->info[md->scan_source];

    if ( ms->lut_entry_size == 1 )
      {
        DBG(1, "set_exposure: 1 byte gamma output tables currently ignored\n");
        return;
      }

    if ( mi->depth & MI_HASDEPTH_12 )
        depth = 12;
    else if ( mi->depth & MI_HASDEPTH_10 )
        depth = 10;
    else
        depth = 8;

    maxval = ( 1 << depth ) - 1;

    from = ms->gamma_table;
    size = ms->lut_size;

    /* first master channel, apply transformation to all colors */
    exposure = ms->exposure_m;
    for ( byte = 0; byte < (u_int32_t)ms->lut_size; byte++ )
      {
        for ( color = 0; color < 3; color++)
          {
            val32 = (u_int32_t) *((u_int16_t *) from + color * size + byte);
            val32 = MIN(val32 + val32
                     * (2 * (u_int32_t) exposure / 100), (u_int32_t)maxval);
            *((u_int16_t *) from + color * size + byte) = (u_int16_t) val32;
          }
      }

    /* and now apply transformation to each channel */

    exposure_rgb[0] = ms->exposure_r;
    exposure_rgb[1] = ms->exposure_g;
    exposure_rgb[2] = ms->exposure_b;
    for ( color = 0; color < 3; color++ )
      {
        for ( byte = 0; byte < (u_int32_t)size; byte++ )
          {
            val32 = (u_int32_t) *((u_int16_t *) from + color * size + byte);
            val32 = MIN(val32 + val32
                         * (2 * (u_int32_t) exposure_rgb[color] / 100),
                                                             (u_int32_t)maxval);
            *((u_int16_t *) from + color * size + byte) = (u_int16_t) val32;
          }
      }

    return;
}


#if 0
/*---------- chunky_set_exposure() -------------------------------------------*/

static void
chunky_set_exposure(u_int8_t *from,
                    u_int32_t pixels,
                    u_int8_t depth,
                    u_int8_t exposure,
                    int color)
{
    u_int32_t pixel;
    u_int32_t val32;
    u_int32_t maxval;

    DBG(30, "chunky_set_exposure: from=%p, ppl=%d, depth=%d, exp=%d, "
            "color=%d\n", from, pixels, depth, exposure, color);


    if ( depth > 8 )
      {
        maxval = (1 << depth) - 1;
        switch ( color )
          {
            case MS_COLOR_ALL:
              for ( pixel = 0; pixel < 3 * pixels; pixel++ )
                {
                  val32 = (u_int32_t) *((u_int16_t *) from + pixel);
                  val32 = MIN(val32 + val32 * (2 * exposure / 100), maxval);
                  *((u_int16_t *) from + pixel) = (u_int16_t) val32;
                }
              break;
            default:
              for ( pixel = 0; pixel < pixels; pixel++ )
                {
                  val32 = (u_int32_t) *((u_int16_t *) from + 3 * pixel + color);
                  val32 = MIN(val32 + val32
                          * (2 * (u_int32_t) exposure / 100), maxval);
                  *((u_int16_t *) from + 3 * pixel + color) = (u_int16_t) val32;
                }
              break;
          }
      }
    else
      {
        switch ( color )
          {
            case MS_COLOR_ALL:
              for ( pixel = 0; pixel < 3 * pixels; pixel++ )
                {
                  val32 = (u_int32_t) from[pixel];
                  val32 = MIN(val32 + val32
                          * (2 * (u_int32_t) exposure / 100), 0xff);
                  from[pixel] = (u_int8_t) val32;
                }
              break;

            default:
              for ( pixel = 0; pixel < pixels; pixel++ )
                {
                  val32 = (u_int32_t) from[3 * pixel + color];
                  val32 = MIN(val32 + val32
                          * (2 * (u_int32_t) exposure / 100), 0xff);
                  from[3 * pixel + color] = (u_int8_t) val32;
                }
              break;
          }
      }
    return;
}
#endif


/*---------- set_option_dependencies() ---------------------------------------*/

static SANE_Status
set_option_dependencies(SANE_Option_Descriptor *sod,
                        Microtek2_Option_Value *val)
{

    DBG(40, "set_option_dependencies: val=%p, sod=%p, mode=%s\n",
             val, sod, val[OPT_MODE].s);


    if ( strcmp(val[OPT_MODE].s, MD_MODESTRING_COLOR) == 0 )
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
        sod[OPT_BITDEPTH].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_AUTOADJUST].cap |= SANE_CAP_INACTIVE;

        /* reset options values that are inactive to their default */
        val[OPT_THRESHOLD].w = MD_THRESHOLD_DEFAULT;
      }
    else if ( strcmp(val[OPT_MODE].s, MD_MODESTRING_GRAY) == 0 )
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
        sod[OPT_BITDEPTH].cap &= ~SANE_CAP_INACTIVE;
        sod[OPT_AUTOADJUST].cap |= SANE_CAP_INACTIVE;

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
        sod[OPT_BITDEPTH].cap |= SANE_CAP_INACTIVE;
        sod[OPT_AUTOADJUST].cap |= SANE_CAP_INACTIVE;

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
        val[OPT_THRESHOLD].w = MD_THRESHOLD_DEFAULT;
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
        sod[OPT_BITDEPTH].cap |= SANE_CAP_INACTIVE;
        sod[OPT_AUTOADJUST].cap &= ~SANE_CAP_INACTIVE;

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
      {
        restore_gamma_options(sod, val);
      }

    return SANE_STATUS_GOOD;
}


/*---------- shading_function() ----------------------------------------------*/

static SANE_Status
shading_function(Microtek2_Scanner *ms, u_int8_t *data)
{
    Microtek2_Device *md;
    Microtek2_Info *mi;
    u_int32_t value;
    int color;
    int i;


    DBG(40, "shading_function: ms=%p, data=%p\n", ms, data);

    md = ms->dev;
    mi = &md->info[md->scan_source];


    if ( ms->lut_entry_size == 1 )
      {
        DBG(1, "shading_function: wordsize = 1 unsupported\n");
         return SANE_STATUS_IO_ERROR;
      }

    for ( color = 0; color < 3; color++ )
      {
        for ( i = 0; i < mi->geo_width; i++)
          {
            value = *((u_int16_t *) data + color * mi->geo_width + i);
            switch ( mi->shtrnsferequ )
              {
                case 0x00:
                  /* output == input */
                  break;

                case 0x01:
                  value = (ms->lut_size * ms->lut_size) / value;
                  *((u_int16_t *) data + color * mi->geo_width + i) =
                                               MIN(0xffff, (u_int16_t) value);
                  break;

                case 0x11:
                  value = (ms->lut_size * ms->lut_size)
                           / (u_int32_t) ( (double) value
                                           * ((double) mi->balance[color]
                                             / 256.0));
                  *((u_int16_t *) data + color * mi->geo_width + i) =
                                               MIN(0xffff, (u_int16_t) value);
                  break;
                default:
                  DBG(1, "Unsupported shading transfer function 0x%02x\n",
                  mi->shtrnsferequ );
                  break;
              }
          }
      }

    return SANE_STATUS_GOOD;
}


/*---------- signal_handler() ------------------------------------------------*/

static RETSIGTYPE
signal_handler (int signal)
{
  if ( signal == SIGTERM )
    {
      sanei_scsi_req_flush_all ();
      _exit (SANE_STATUS_GOOD);
    }
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
        int scale1;
        int scale2;
        u_int16_t val16;

        scale1 = 16 - depth;
        scale2 = 2 * depth - 16;
        for ( pixel = 0; pixel < pixels; pixel++ )
          {
            for ( color = 0; color < 3; color++ )
              {
                val16 = *(u_int16_t *) from;
                val16 = ( val16 << scale1 ) | ( val16 >> scale2 );
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
    SANE_Status status;
    u_int8_t *from;
    u_int32_t line;


    DBG(30, "wordchunky_proc_data: ms=%p\n", ms);

    from = ms->buf.src_buf;
    for ( line = 0; line < (u_int32_t)ms->src_lines_to_read; line++ )
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
    SANE_Status status;


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
scsi_send_gamma(Microtek2_Scanner *ms, u_int16_t length)
{
    SANE_Bool endiantype;
    SANE_Status status;
    size_t size;
    u_int8_t *cmd;


    DBG(30, "scsi_send_gamma: pos=%p, size=%d, word=%d, color=%d\n",
             ms->gamma_table, ms->lut_size_bytes, ms->word, ms->current_color);

    cmd = (u_int8_t *) alloca(SG_CMD_L + length);
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
    SG_SET_TRANSFERLENGTH(cmd, length);
    memcpy(cmd + SG_CMD_L, ms->gamma_table, length);
    size = length;

    if ( md_dump >= 2 )
        dump_area2(cmd, SG_CMD_L, "sendgammacmd");
    if ( md_dump >= 3 )
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
    SANE_Status status;
    u_int8_t cmd[INQ_CMD_L];
    u_int8_t *result;
    u_int8_t inqlen;
    size_t size;
    int sfd;


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
    if (md_dump >= 2 )
        dump_area2(cmd, sizeof(cmd), "inquiry");

    status = sanei_scsi_cmd(sfd, cmd, sizeof(cmd), result, &size);
    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "scsi_inquiry: cmd '%s'\n", sane_strstatus(status));
        sanei_scsi_close(sfd);
        return status;
      }
    sanei_scsi_close(sfd);

    if (md_dump >= 2 )
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
    SANE_Status status;
    Microtek2_Info *mi;
    u_int8_t readattributes[RSA_CMD_L];
    u_int8_t result[RSA_TRANSFERLENGTH];
    size_t size;
    int sfd;


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

    if (md_dump >= 2 )
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

    /* The X6 appears to lie about the data format for a TMA */
    if ( (&pmi[0])->model_code == 0x91 )
        result[0] &= 0xfd;

#if 0
    result[13] &= 0xfe; /* simulate no lineart */
#endif

    /* copy all the stuff into the info structure */
    RSA_COLOR(mi->color, result);
    RSA_ONEPASS(mi->onepass, result);
    RSA_SCANNERTYPE(mi->scanner_type, result);
    RSA_FEPROM(mi->feprom, result);
    RSA_DATAFORMAT(mi->data_format, result);
    RSA_COLORSEQUENCE(mi->color_sequence, result);
    RSA_DATSEQ(mi->direction, result);
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
    RSA_REDBALANCE(mi->balance[0], result);
    RSA_GREENBALANCE(mi->balance[1], result);
    RSA_BLUEBALANCE(mi->balance[2], result);
    RSA_APSMAXFRAMES(mi->aps_maxframes, result);

    if (md_dump >= 2 )
        dump_area2((u_int8_t *) result, sizeof(result),
                   "scannerattributesresults");
    if ( md_dump >= 1 && md_dump_clear )
        dump_attributes(mi);

    return SANE_STATUS_GOOD;
}


/*---------- scsi_read_control_bits() ----------------------------------------*/

static SANE_Status
scsi_read_control_bits(Microtek2_Scanner *ms, int sfd)
{
    Microtek2_Device *md;
    SANE_Status status;
    u_int8_t cmd[RCB_CMD_L];
    int open_fd = 0;

    DBG(30, "scsi_read_control_bits: ms=%p, fd=%d\n", ms, sfd);

    md = ms->dev;

    if ( sfd == -1 )
      {
        open_fd = 1;
        status = sanei_scsi_open(md->name, &sfd, scsi_sense_handler, 0);
        if ( status != SANE_STATUS_GOOD )
          {
            DBG(1, "scsi_read_control_bits: open '%s'\n",
                    sane_strstatus(status));
            return status;
          }
      }

    RCB_SET_CMD(cmd);
    RCB_SET_LENGTH(cmd, ms->n_control_bytes);

    if ( md_dump >= 2)
        dump_area2(cmd, RCB_CMD_L, "readcontrolbits");

    status = sanei_scsi_cmd(sfd,
                            cmd,
                            sizeof(cmd),
                            ms->control_bytes,
                            &ms->n_control_bytes);

    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "scsi_read_system_status: cmd '%s'\n", sane_strstatus(status));
        sanei_scsi_close(sfd);
        return status;
      }

    if ( open_fd == 1 )
        sanei_scsi_close(sfd);

    if ( md_dump >= 2)
        dump_area2(ms->control_bytes,
                   ms->n_control_bytes,
                   "readcontrolbitsresult");

    return SANE_STATUS_GOOD;
}


/*---------- scsi_set_window() -----------------------------------------------*/

static SANE_Status
scsi_set_window(Microtek2_Scanner *ms, int n) {   /* n windows, not yet */
                                                  /* implemented */
    SANE_Status status;
    u_int8_t *setwindow;
    int size;


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
    SW_STAY(POS, ms->stay);
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

    if ( md_dump >= 2 )
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

    if ( md_dump >= 2)
        dump_area2(cmd, RII_CMD_L, "readimageinfo");

    size = sizeof(result);
    status = sanei_scsi_cmd(ms->sfd, cmd, sizeof(cmd), result, &size);
    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "scsi_read_image_info: '%s'\n", sane_strstatus(status));
        return status;
      }

    if ( md_dump >= 2)
        dump_area2(result, size, "readimageinforesult");

    /* The V300 returns some values in only two bytes */
    if ( md->model_flags & MD_RII_TWO_BYTES )
      {
        RII_GET_V300_WIDTHPIXEL(ms->ppl, result);
        RII_GET_V300_WIDTHBYTES(ms->bpl, result);
        RII_GET_V300_HEIGHTLINES(ms->src_remaining_lines, result);
        RII_GET_V300_REMAINBYTES(ms->remaining_bytes, result);
      }
    else
      {
        RII_GET_WIDTHPIXEL(ms->ppl, result);
        RII_GET_WIDTHBYTES(ms->bpl, result);
        RII_GET_HEIGHTLINES(ms->src_remaining_lines, result);
        RII_GET_REMAINBYTES(ms->remaining_bytes, result);
      }

    DBG(30, "scsi_read_image_info: ppl=%d, bpl=%d, lines=%d, remain=%d\n",
             ms->ppl, ms->bpl, ms->src_remaining_lines, ms->remaining_bytes);

    return SANE_STATUS_GOOD;
}


/*---------- scsi_read_image() -----------------------------------------------*/

static SANE_Status
scsi_read_image(Microtek2_Scanner *ms, u_int8_t *buffer)
{
    u_int8_t cmd[RI_CMD_L];
    SANE_Bool endiantype;
    SANE_Status status;
    size_t size;
    Microtek2_Device *md;


    DBG(30, "scsi_read_image:  ms=%p, buffer=%p\n", ms, buffer);

    ENDIAN_TYPE(endiantype)
    RI_SET_CMD(cmd);
    RI_SET_PCORMAC(cmd, endiantype);
    RI_SET_COLOR(cmd, ms->current_color);
    RI_SET_TRANSFERLENGTH(cmd, ms->transfer_length);

    DBG(30, "scsi_read_image: transferlength=%d\n", ms->transfer_length);

    if ( md_dump >= 2 )
        dump_area2(cmd, RI_CMD_L, "readimagecmd");

    md = ms->dev;
    if (md->model_flags & MD_X6_SHORT_TRANSFER)
        size = ms->transfer_length;
    else
        size = ms->src_buffer_size;
    status = sanei_scsi_cmd(ms->sfd, cmd, sizeof(cmd), buffer, &size);
                         /* ms->buf.src_buffer[ms->buf.current_src], &size);*/

    if ( status != SANE_STATUS_GOOD )
        DBG(1, "scsi_read_image: '%s'\n", sane_strstatus(status));

    if ( md_dump > 3 )
        dump_area2(buffer, ms->transfer_length, "readimageresult");

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

    if ( md_dump >= 2 )
        dump_area2(cmd, sizeof(cmd), "readimagestatus");

    status = sanei_scsi_cmd(ms->sfd, cmd, sizeof(cmd), 0, NULL);
    if ( status != SANE_STATUS_GOOD )
        DBG(1, "scsi_read_image_status: '%s'\n", sane_strstatus(status));

    return status;
}


/*---------- scsi_send_shading () --------------------------------------------*/

static SANE_Status
scsi_send_shading(Microtek2_Scanner *ms,
                  u_int8_t * shading_data,
                  u_int32_t length,
                  u_int8_t dark)
{
    SANE_Bool endiantype;
    SANE_Status status;
    size_t size;
    u_int8_t *cmd;


    DBG(30, "scsi_send_shading: pos=%p, size=%d, word=%d, color=%d, dark=%d\n",
             ms->dev->shading_table_w, length, ms->word, ms->current_color,
             dark);

    cmd = (u_int8_t *) malloc(SSI_CMD_L + length);
    if ( cmd == NULL )
      {
        DBG(1, "scsi_send_shading: Couldn't get buffer for shading table\n");
        return SANE_STATUS_IO_ERROR;
      }

    SSI_SET_CMD(cmd);
    ENDIAN_TYPE(endiantype)
    SSI_SET_PCORMAC(cmd, endiantype);
    SSI_SET_COLOR(cmd, ms->current_color);
    SSI_SET_DARK(cmd, dark);
    SSI_SET_WORD(cmd, ms->word);
    SSI_SET_TRANSFERLENGTH(cmd, length);
    memcpy(cmd + SSI_CMD_L, shading_data, length);
    size = length;

    if ( md_dump >= 2 )
        dump_area2(cmd, SSI_CMD_L, "sendshading");
    if ( md_dump >= 3 )
        dump_area2(cmd + SSI_CMD_L, size, "sendshading");

    status = sanei_scsi_cmd(ms->sfd, cmd, size + SSI_CMD_L, NULL, 0);
    if ( status != SANE_STATUS_GOOD )
        DBG(1, "scsi_send_shading: '%s'\n", sane_strstatus(status));
    free((void *) cmd);

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

    if ( md_dump >= 2)
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

    if ( md_dump >= 2)
        dump_area2(result, size, "readsystemstatusresult");

    md->status.sskip = RSS_SSKIP(result);
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

#if 0

static SANE_Status
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

    if ( md_dump >= 2 )
        dump_area2(buffer, size, "requestsenseresult");

    dump_area(buffer, RQS_LENGTH(buffer), "RequestSense");
    asl = RQS_ASL(buffer);
    if ( (as_info_length = RQS_ASINFOLENGTH(buffer)) > 0 )
        DBG(25, "scsi_request_sense: info '%.*s'\n",
                as_info_length, RQS_ASINFO(buffer));

    return SANE_STATUS_GOOD;
}
#endif


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
    SSS_RESERVED04(pos, md->status.reserved04);
    SSS_NTRACK(pos, md->status.ntrack);
    SSS_NCALIB(pos, md->status.ncalib);
    SSS_TLAMP(pos, md->status.tlamp);
    SSS_FLAMP(pos, md->status.flamp);
    SSS_RESERVED17(pos, md->status.reserved17);
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

    if ( md_dump >= 2)
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

#if 0
    return SANE_STATUS_GOOD;
#endif
}


/*---------- scsi_test_unit_ready() ------------------------------------------*/

static SANE_Status
scsi_test_unit_ready(Microtek2_Device *md)
{
    SANE_Status status;
    u_int8_t tur[TUR_CMD_L];
    int sfd;


    DBG(30, "scsi_test_unit_ready: md=%s\n", md->name);

    TUR_CMD(tur);
    status = sanei_scsi_open(md->name, &sfd, scsi_sense_handler, 0);
    if ( status != SANE_STATUS_GOOD )
      {
        DBG(1, "scsi_test_unit_ready: open '%s'\n", sane_strstatus(status));
        return status;
      }

    if ( md_dump >= 2 )
        dump_area2(tur, sizeof(tur), "testunitready");

    status = sanei_scsi_cmd(sfd, tur, sizeof(tur), NULL, 0);
    if ( status != SANE_STATUS_GOOD )
        DBG(1, "scsi_test_unit_ready: cmd '%s'\n", sane_strstatus(status));

    sanei_scsi_close(sfd);
    return status;
}
