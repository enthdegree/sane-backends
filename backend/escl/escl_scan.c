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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#include "../include/sane/sanei.h"

/**
 * \fn static size_t write_callback(void *str, size_t size, size_t nmemb, void *userp)
 * \brief Callback function that writes the image scanned into the temporary file.
 *
 * \return to_write (the result of the fwrite fonction)
 */
static size_t
write_callback(void *str, size_t size, size_t nmemb, void *userp)
{
    size_t to_write = fwrite(str, size, nmemb, (FILE *)userp);

    return (to_write);
}

/**
 * \fn SANE_Status escl_scan(capabilities_t *scanner, SANE_String_Const name, char *result)
 * \brief Function that, after recovering the 'new job', scans the image writed in the
 *        temporary file, using curl.
 *        This function is called in the 'sane_start' function and it's the equivalent of
 *        the following curl command : "curl -s http(s)://'ip:'port'/eSCL/ScanJobs/'new job'/NextDocument > image.jpg".
 *
 * \return status (if everything is OK, status = SANE_STATUS_GOOD, otherwise, SANE_STATUS_NO_MEM/SANE_STATUS_INVAL)
 */
SANE_Status
escl_scan(capabilities_t __sane_unused__ *scanner, SANE_String_Const name, char *result)
{
    CURL *curl_handle = NULL;
    const char *scan_jobs = "/eSCL/ScanJobs";
    const char *scanner_start = "/NextDocument";
    char scan_cmd[PATH_MAX] = { 0 };
    SANE_Status status = SANE_STATUS_GOOD;

    if (name == NULL)
        return (SANE_STATUS_NO_MEM);
    curl_handle = curl_easy_init();
    if (curl_handle != NULL) {
        strcpy(scan_cmd, name);
        strcat(scan_cmd, scan_jobs);
        strcat(scan_cmd, result);
        strcat(scan_cmd, scanner_start);
        curl_easy_setopt(curl_handle, CURLOPT_URL, scan_cmd);
        DBG( 1, "Scan : %s.\n", scan_cmd);
	if (strncmp(name, "https", 5) == 0) {
            DBG( 1, "Ignoring safety certificates, use https\n");
            curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);
        }
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
        scanner->tmp = tmpfile();
        if (scanner->tmp != NULL) {
            curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, scanner->tmp);
            if (curl_easy_perform(curl_handle) != CURLE_OK) {
                status = SANE_STATUS_INVAL;
            }
            else
                curl_easy_cleanup(curl_handle);
            fseek(scanner->tmp, 0, SEEK_SET);
        }
        else
            status = SANE_STATUS_NO_MEM;
    }
    return (status);
}
