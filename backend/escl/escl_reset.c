/* sane - Scanner Access Now Easy.

   Copyright (C) 2019 Touboul Nathane
   Copyright (C) 2019 Thierry HUCHARD <thierry@ordissimo.com>

   This file is part of the SANE package.

   SANE is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 3 of the License, or (at your
   option) any later version.

   SANE is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with sane; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   This file implements a SANE backend for eSCL scanners.  */

#define DEBUG_DECLARE_ONLY
#include "../include/sane/config.h"

#include "escl.h"

#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

/**
 * \fn void escl_scanner(const ESCL_Device *device, char *result)
 * \brief Function that resets the scanner after each scan, using curl.
 *        This function is called in the 'sane_cancel' function.
 */
void
escl_scanner(const ESCL_Device *device, char *result)
{
    CURL *curl_handle = NULL;
    const char *scan_jobs = "/eSCL/ScanJobs";
    const char *scanner_start = "/NextDocument";
    char scan_cmd[PATH_MAX] = { 0 };
    int i = 0;
    long answer = 0;

    if (device == NULL || result == NULL)
        return;
CURL_CALL:
    curl_handle = curl_easy_init();
    if (curl_handle != NULL) {
        snprintf(scan_cmd, sizeof(scan_cmd), "%s%s%s",
                 scan_jobs, result, scanner_start);
        escl_curl_url(curl_handle, device, scan_cmd);
        if (curl_easy_perform(curl_handle) == CURLE_OK) {
            curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &answer);
            i++;
            if (i >= 15) return;
        }
        curl_easy_cleanup(curl_handle);
        if (SANE_STATUS_GOOD != escl_status(device, PLATEN, result))
            goto CURL_CALL;
    }
}
