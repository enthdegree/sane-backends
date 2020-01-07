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

#include "escl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

struct uploading
{
    const char *read_data;
    size_t size;
};

struct downloading
{
    char *memory;
    size_t size;
};

static const char settings[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"                        \
    "<scan:ScanSettings xmlns:pwg=\"http://www.pwg.org/schemas/2010/12/sm\" xmlns:scan=\"http://schemas.hp.com/imaging/escl/2011/05/03\">" \
    "   <pwg:Version>2.0</pwg:Version>" \
    "   <pwg:ScanRegions>" \
    "      <pwg:ScanRegion>" \
    "          <pwg:ContentRegionUnits>escl:ThreeHundredthsOfInches</pwg:ContentRegionUnits>" \
    "          <pwg:Height>%d</pwg:Height>" \
    "          <pwg:Width>%d</pwg:Width>" \
    "          <pwg:XOffset>%d</pwg:XOffset>" \
    "          <pwg:YOffset>%d</pwg:YOffset>" \
    "      </pwg:ScanRegion>" \
    "   </pwg:ScanRegions>" \
    "   <pwg:DocumentFormat>%s</pwg:DocumentFormat>" \
    "%s" \
    "   <scan:ColorMode>%s</scan:ColorMode>" \
    "   <scan:XResolution>%d</scan:XResolution>" \
    "   <scan:YResolution>%d</scan:YResolution>" \
    "   <pwg:InputSource>%s</pwg:InputSource>" \
    "</scan:ScanSettings>";

static char formatExtJPEG[] =
    "   <scan:DocumentFormatExt>image/jpeg</scan:DocumentFormatExt>";

static char formatExtPNG[] =
    "   <scan:DocumentFormatExt>image/png</scan:DocumentFormatExt>";

static char formatExtTIFF[] =
    "   <scan:DocumentFormatExt>image/tiff</scan:DocumentFormatExt>";

/**
 * \fn static size_t download_callback(void *str, size_t size, size_t nmemb, void *userp)
 * \brief Callback function that stocks in memory the content of the 'job'. Example below :
 *        "Trying 192.168.14.150...
 *         TCP_NODELAY set
 *         Connected to 192.168.14.150 (192.168.14.150) port 80
 *         POST /eSCL/ScanJobs HTTP/1.1
 *         Host: 192.168.14.150
 *         User-Agent: curl/7.55.1
 *         Accept: /
 *         Content-Length: 605
 *         Content-Type: application/x-www-form-urlencoded
 *         upload completely sent off: 605 out of 605 bytes
 *         < HTTP/1.1 201 Created
 *         < MIME-Version: 1.0
 *         < Location: http://192.168.14.150/eSCL/ScanJobs/22b54fd0-027b-1000-9bd0-f4a99726e2fa
 *         < Content-Length: 0
 *         < Connection: close
 *         <
 *         Closing connection 0"
 *
 * \return realsize (size of the content needed -> the 'job')
 */
static size_t
download_callback(void *str, size_t size, size_t nmemb, void *userp)
{
    struct downloading *download = (struct downloading *)userp;
    size_t realsize = size * nmemb;
    char *content = realloc(download->memory, download->size + realsize + 1);

    if (content == NULL) {
        fprintf(stderr, "not enough memory (realloc returned NULL)\n");
        return (0);
    }
    download->memory = content;
    memcpy(&(download->memory[download->size]), str, realsize);
    download->size = download->size + realsize;
    download->memory[download->size] = 0;
    return (realsize);
}

/**
 * \fn char *escl_newjob (capabilities_t *scanner, SANE_String_Const name, SANE_Status *status)
 * \brief Function that, using curl, uploads the data (composed by the scanner capabilities) to the
 *        server to download the 'job' and recover the 'new job' (char *result), in LOCATION.
 *        This function is called in the 'sane_start' function and it's the equivalent of the
 *        following curl command : "curl -v POST -d cap.xml http(s)://'ip':'port'/eSCL/ScanJobs".
 *
 * \return result (the 'new job', situated in LOCATION)
 */
char *
escl_newjob (capabilities_t *scanner, SANE_String_Const name, SANE_Status *status)
{
    CURL *curl_handle = NULL;
    struct uploading *upload = NULL;
    struct downloading *download = NULL;
    const char *scan_jobs = "/eSCL/ScanJobs";
    char cap_data[PATH_MAX] = { 0 };
    char job_cmd[PATH_MAX] = { 0 };
    char *location = NULL;
    char *result = NULL;
    char *temporary = NULL;
    char *f_ext = "";
    char *format_ext = NULL;

    *status = SANE_STATUS_GOOD;
    if (name == NULL || scanner == NULL) {
        *status = SANE_STATUS_NO_MEM;
        return (NULL);
    }
    upload = (struct uploading *)calloc(1, sizeof(struct uploading));
    if (upload == NULL) {
        *status = SANE_STATUS_NO_MEM;
        return (NULL);
    }
    download = (struct downloading *)calloc(1, sizeof(struct downloading));
    if (download == NULL) {
        free(upload);
        *status = SANE_STATUS_NO_MEM;
        return (NULL);
    }
    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();
    if (scanner->caps[scanner->source].format_ext == 1)
    {
       if (!strcmp(scanner->caps[scanner->source].default_format, "image/jpeg"))
          format_ext = formatExtJPEG;
       else if (!strcmp(scanner->caps[scanner->source].default_format, "image/png"))
          format_ext = formatExtPNG;
       else if (!strcmp(scanner->caps[scanner->source].default_format, "image/tiff"))
          format_ext = formatExtTIFF;
       else
          format_ext = f_ext;
    }
    else
      format_ext = f_ext;
    if (curl_handle != NULL) {
        snprintf(cap_data, sizeof(cap_data), settings,
			scanner->caps[scanner->source].height,
			scanner->caps[scanner->source].width,
			0,
			0,
			scanner->caps[scanner->source].default_format,
			format_ext,
			scanner->caps[scanner->source].default_color,
			scanner->caps[scanner->source].default_resolution,
			scanner->caps[scanner->source].default_resolution,
			scanner->Sources[scanner->source]);
        upload->read_data = strdup(cap_data);
        upload->size = strlen(cap_data);
        download->memory = malloc(1);
        download->size = 0;
        strcpy(job_cmd, name);
        strcat(job_cmd, scan_jobs);
        curl_easy_setopt(curl_handle, CURLOPT_URL, job_cmd);
        if (strncmp(name, "https", 5) == 0) {
            curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);
        }
        curl_easy_setopt(curl_handle, CURLOPT_POST, 1L);
        curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, upload->read_data);
        curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, upload->size);
        curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, download_callback);
        curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)download);
        if (curl_easy_perform(curl_handle) != CURLE_OK) {
            fprintf(stderr, "THERE IS NO SCANNER\n");
            *status = SANE_STATUS_INVAL;
        }
        else {
            if (download->memory != NULL) {
                if (strstr(download->memory, "Location:")) {
                    temporary = strrchr(download->memory, '/');
                    if (temporary != NULL) {
                        location = strchr(temporary, '\r');
                        if (location == NULL)
                            location = strchr(temporary, '\n');
                        else {
                            *location = '\0';
                            result = strdup(temporary);
                        }
                    }
                    free(download->memory);
                }
                else {
                    fprintf(stderr, "THERE IS NO LOCATION\n");
                    *status = SANE_STATUS_INVAL;
                }
            }
            else {
                *status = SANE_STATUS_NO_MEM;
                return (NULL);
            }
        }
        curl_easy_cleanup(curl_handle);
    }
    curl_global_cleanup();
    if (upload != NULL)
        free(upload);
    if (download != NULL)
        free(download);
    return (result);
}
