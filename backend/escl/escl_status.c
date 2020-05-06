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
#include <libxml/parser.h>

struct idle
{
    char *memory;
    size_t size;
};

/**
 * \fn static size_t memory_callback_s(void *contents, size_t size, size_t nmemb, void *userp)
 * \brief Callback function that stocks in memory the content of the scanner status.
 *
 * \return realsize (size of the content needed -> the scanner status)
 */
static size_t
memory_callback_s(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct idle *mem = (struct idle *)userp;

    char *str = realloc(mem->memory, mem->size + realsize + 1);
    if (str == NULL) {
        DBG(1, "not enough memory (realloc returned NULL)\n");
        return (0);
    }
    mem->memory = str;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size = mem->size + realsize;
    mem->memory[mem->size] = 0;
    return (realsize);
}

/**
 * \fn static int find_nodes_s(xmlNode *node)
 * \brief Function that browses the xml file and parses it, to find the xml children node.
 *        --> to recover the scanner status.
 *
 * \return 0 if a xml child node is found, 1 otherwise
 */
static int
find_nodes_s(xmlNode *node)
{
    xmlNode *child = node->children;

    while (child) {
        if (child->type == XML_ELEMENT_NODE)
            return (0);
        child = child->next;
    }
    return (1);
}

static void
print_xml_job_status(xmlNode *node,
                     SANE_Status *processing,
                     SANE_Status *complete,
                     int *image)
{
    while (node) {
        if (node->type == XML_ELEMENT_NODE) {
            if (find_nodes_s(node)) {
                if (strcmp((const char *)node->name, "JobState") == 0) {
                    const char *state = (const char *)xmlNodeGetContent(node);
					if (!strcmp(state, "Processing")) {
                        *processing = SANE_STATUS_GOOD;
					}
					if (!strcmp(state, "Completed")) {
                        *complete = SANE_STATUS_GOOD;
					}
                }
                else if (strcmp((const char *)node->name, "ImagesToTransfer") == 0) {
                    const char *state = (const char *)xmlNodeGetContent(node);
                    *image = atoi(state);
                }
            }
        }
        print_xml_job_status(node->children, processing, complete, image);
        node = node->next;
    }
}

static void
print_xml_feeder_status(xmlNode *node,
                        const char *job,
                        SANE_Status *processing,
                        SANE_Status *complete,
                        int *image)
{
    while (node) {
        if (node->type == XML_ELEMENT_NODE) {
            if (find_nodes_s(node)) {
                if (strcmp((const char *)node->name, "JobUri") == 0) {
                    if (strstr((const char *)xmlNodeGetContent(node), job)) {
						print_xml_job_status(node, processing, complete, image);
						return;
					}
                }
            }
        }
        print_xml_feeder_status(node->children, job, processing, complete, image);
        node = node->next;
    }
}

static void
print_xml_platen_status(xmlNode *node, SANE_Status *status)
{
    while (node) {
        if (node->type == XML_ELEMENT_NODE) {
            if (find_nodes_s(node)) {
                if (strcmp((const char *)node->name, "State") == 0) {
					printf ("State\t");
                    const char *state = (const char *)xmlNodeGetContent(node);
                    if (!strcmp(state, "Idle")) {
						printf("Idle SANE_STATUS_GOOD\n");
                        *status = SANE_STATUS_GOOD;
                    } else if (!strcmp(state, "Processing")) {
						printf("Processing SANE_STATUS_DEVICE_BUSY\n");
                        *status = SANE_STATUS_DEVICE_BUSY;
                    } else {
						printf("%s SANE_STATUS_UNSUPPORTED\n", state);
                        *status = SANE_STATUS_UNSUPPORTED;
                    }
                }
            }
        }
        print_xml_platen_status(node->children, status);
        node = node->next;
    }
}

/**
 * \fn SANE_Status escl_status(const ESCL_Device *device)
 * \brief Function that finally recovers the scanner status ('Idle', or not), using curl.
 *        This function is called in the 'sane_open' function and it's the equivalent of
 *        the following curl command : "curl http(s)://'ip':'port'/eSCL/ScannerStatus".
 *
 * \return status (if everything is OK, status = SANE_STATUS_GOOD, otherwise, SANE_STATUS_NO_MEM/SANE_STATUS_INVAL)
 */
SANE_Status
escl_status(const ESCL_Device *device, int source, char *jobid)
{
    SANE_Status status;
    CURL *curl_handle = NULL;
    struct idle *var = NULL;
    xmlDoc *data = NULL;
    xmlNode *node = NULL;
    const char *scanner_status = "/eSCL/ScannerStatus";

    if (device == NULL)
        return (SANE_STATUS_NO_MEM);
    var = (struct idle*)calloc(1, sizeof(struct idle));
    if (var == NULL)
        return (SANE_STATUS_NO_MEM);
    var->memory = malloc(1);
    var->size = 0;
    curl_handle = curl_easy_init();

    escl_curl_url(curl_handle, device, scanner_status);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, memory_callback_s);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)var);
    CURLcode res = curl_easy_perform(curl_handle);
    if (res != CURLE_OK) {
        DBG( 1, "The scanner didn't respond: %s\n", curl_easy_strerror(res));
        status = SANE_STATUS_INVAL;
        goto clean_data;
    }
    DBG( 10, "eSCL : Status : %s.\n", var->memory);
    data = xmlReadMemory(var->memory, var->size, "file.xml", NULL, 0);
    if (data == NULL) {
        status = SANE_STATUS_NO_MEM;
        goto clean_data;
    }
    node = xmlDocGetRootElement(data);
    if (node == NULL) {
        status = SANE_STATUS_NO_MEM;
        goto clean;
    }
    status = SANE_STATUS_DEVICE_BUSY;
    /* Decode Job status */
    if (source == PLATEN) {
	    print_xml_platen_status(node, &status);
    } else {
	    SANE_Status processing = SANE_STATUS_UNSUPPORTED;
        SANE_Status complete = SANE_STATUS_UNSUPPORTED;
        int image = -1;
	    print_xml_feeder_status(node, jobid, &processing, &complete, &image);
	    if (processing == SANE_STATUS_GOOD  && image == 0 &&
	        complete == SANE_STATUS_UNSUPPORTED)
	           status = SANE_STATUS_EOF;
	    else if (complete == SANE_STATUS_GOOD  && image == -1 &&
	             processing == SANE_STATUS_UNSUPPORTED)
	           status = SANE_STATUS_EOF;
	    else
	           status = SANE_STATUS_GOOD;
    }
    DBG (10, "STATUS : %s\n", sane_strstatus(status));
clean:
    xmlFreeDoc(data);
clean_data:
    xmlCleanupParser();
    xmlMemoryDump();
    curl_easy_cleanup(curl_handle);
    free(var->memory);
    free(var);
    return (status);
}
