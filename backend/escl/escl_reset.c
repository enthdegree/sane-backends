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
 * \fn void escl_scanner(SANE_String_Const name, char *result)
 * \brief Function that resets the scanner after each scan, using curl.
 *        This function is called in the 'sane_cancel' function.
 */
void
escl_scanner(SANE_String_Const name, char *result)
{
    CURL *curl_handle = NULL;
    const char *scan_jobs = "/eSCL/ScanJobs";
    const char *scanner_start = "/NextDocument";
    char scan_cmd[PATH_MAX] = { 0 };
    int i = 0;
    long answer = 0;

    if (name == NULL || result == NULL)
        return;
CURL_CALL:
    curl_handle = curl_easy_init();
    if (curl_handle != NULL) {
        strcpy(scan_cmd, name);
        strcat(scan_cmd, scan_jobs);
        strcat(scan_cmd, result);
        strcat(scan_cmd, scanner_start);
        curl_easy_setopt(curl_handle, CURLOPT_URL, scan_cmd);
        DBG( 1, "Reset Job : %s.\n", scan_cmd);
        if (strncmp(name, "https", 5) == 0) {
            DBG( 1, "Ignoring safety certificates, use https\n");
            curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);
        }
        if (curl_easy_perform(curl_handle) == CURLE_OK) {
            curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &answer);
            if (i < 3 && answer == 503) {
                curl_easy_cleanup(curl_handle);
                i++;
                goto CURL_CALL;
            }
        }
        curl_easy_cleanup(curl_handle);
    }
}
