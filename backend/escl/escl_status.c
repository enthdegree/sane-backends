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
        fprintf(stderr, "not enough memory (realloc returned NULL)\n");
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

/**
 * \fn static void print_xml_s(xmlNode *node, SANE_Status *status)
 * \brief Function that browses the xml file, node by node.
 *        If the node 'State' is found, we are expecting to found in this node the 'Idle'
 *        content (if the scanner is ready to use) and then 'status' = SANE_STATUS_GOOD.
 *        Otherwise, this means that the scanner isn't ready to use.
 */
static void
print_xml_s(xmlNode *node, SANE_Status *status)
{
    int x = 0;

    while (node) {
        if (node->type == XML_ELEMENT_NODE) {
            if (find_nodes_s(node)) {
                if (strcmp((const char *)node->name, "State") == 0)
                    x = 1;
            }
            if (x == 1 && strcmp((const char *)xmlNodeGetContent(node), "Idle") == 0)
                *status = SANE_STATUS_GOOD;
        }
        print_xml_s(node->children, status);
        node = node->next;
    }
}

/**
 * \fn SANE_Status escl_status(SANE_String_Const name)
 * \brief Function that finally recovers the scanner status ('Idle', or not), using curl.
 *        This function is called in the 'sane_open' function and it's the equivalent of
 *        the following curl command : "curl http(s)://'ip':'port'/eSCL/ScannerStatus".
 *
 * \return status (if everything is OK, status = SANE_STATUS_GOOD, otherwise, SANE_STATUS_NO_MEM/SANE_STATUS_INVAL)
 */
SANE_Status
escl_status(SANE_String_Const name)
{
    SANE_Status status;
    CURL *curl_handle = NULL;
    struct idle *var = NULL;
    xmlDoc *data = NULL;
    xmlNode *node = NULL;
    const char *scanner_status = "/eSCL/ScannerStatus";
    char tmp[PATH_MAX] = { 0 };

    if (name == NULL)
        return (SANE_STATUS_NO_MEM);
    var = (struct idle*)calloc(1, sizeof(struct idle));
    if (var == NULL)
        return (SANE_STATUS_NO_MEM);
    var->memory = malloc(1);
    var->size = 0;
    curl_handle = curl_easy_init();
    strcpy(tmp, name);
    strcat(tmp, scanner_status);
    curl_easy_setopt(curl_handle, CURLOPT_URL, tmp);
    if (strncmp(name, "https", 5) == 0) {
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, memory_callback_s);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)var);
    if (curl_easy_perform(curl_handle) != CURLE_OK) {
        fprintf(stderr, "THERE IS NO SCANNER\n");
        status = SANE_STATUS_INVAL;
        goto clean_data;
    }
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
    print_xml_s(node, &status);
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
