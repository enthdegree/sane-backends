/*
 * epson2_net.c - SANE library for Epson scanners.
 *
 * Copyright (C) 2006 Tower Technologies
 * Author: Alessandro Zummo <a.zummo@towertech.it>
 *
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 */

#include "../include/sane/config.h"

#include <sane/sane.h>
#include <sane/saneopts.h>
#include <sane/sanei_debug.h>
#include <sane/sanei_tcp.h>
#include <sane/sanei_config.h>
#include <sane/sanei_backend.h>

#include "epson2.h"
#include "epson2_net.h"

#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef NEED_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <string.h>		/* for memset and memcpy */
#include <stdio.h>


int
sanei_epson_net_read(Epson_Scanner *s, unsigned char *buf, size_t wanted,
		       SANE_Status * status)
{
	size_t size, read = 0;
	unsigned char header[12];

	if (s->netbuf != s->netptr ) {
/*		if (s->netlen == wanted) {*/
			DBG(4, "reading %d from buffer at %p\n", wanted, s->netptr);
			memcpy(buf, s->netptr + s->netlen - wanted, wanted);
			read = wanted;
/*		} */

		free(s->netbuf);
		s->netbuf = s->netptr = NULL;
		s->netlen = 0;

		return read;
	}

	sanei_tcp_read(s->fd, header, 12);

	if (header[0] != 'I' || header[1] != 'S') {
		DBG(1, "header mismatch: %02X %02x\n", header[0], header[1]);
/*		return 0;*/
	}
	size = (header[6] << 24) | (header[7] << 16) | (header[8] << 8) | header[9];

	DBG(4, "%s: wanted %d, got %d\n", __FUNCTION__, wanted, size);

	*status = SANE_STATUS_GOOD;

	if (size == wanted) {
		DBG(4, "direct read\n");
		sanei_tcp_read(s->fd, buf, size);

		if (s->netbuf)
			free(s->netbuf);

		s->netbuf = NULL;
		s->netlen = 0;
		read = wanted;
	} else if (wanted < size && s->netlen == size) {
/*		unsigned int k; */
		sanei_tcp_read(s->fd, s->netbuf, size);

/*                for (k = 0; k < size; k++) {
                        DBG(0, "buf[%d] %02x %c\n", k, s->netbuf[k], isprint(s->netbuf[k]) ? s->netbuf[k] : '.');
                }
*/
		s->netlen = size - wanted;
		s->netptr += wanted;
		read = wanted;
		DBG(4, "0,4 %02x %02x", s->netbuf[0], s->netbuf[4]);
		DBG(4, "storing %d to buffer at %p, next read at %p\n", size, s->netbuf, s->netptr);

		memcpy(buf, s->netbuf , wanted);
	}

	return read;
}

int
sanei_epson_net_write(Epson_Scanner *s, unsigned int cmd, const unsigned char *buf,
			size_t buf_size, size_t reply_len, SANE_Status *status)
{
	unsigned char *h1, *h2, *payload;
	unsigned char *packet = malloc(12 + 8 + buf_size);

	/* XXX check allocation failure */

	h1 = packet;
	h2 = packet + 12;
	payload = packet + 12 + 8;

	if (reply_len) {
		s->netbuf = s->netptr = malloc(reply_len);
		s->netlen = reply_len;
		DBG(8, "allocated %d bytes at %p\n", reply_len, s->netbuf);
	}

	DBG(2, "%s: cmd = %04x, buf = %p, buf_size = %d, reply_len = %d\n", __FUNCTION__, cmd, buf, buf_size, reply_len);

	memset(h1, 0x00, 12);
	memset(h2, 0x00, 8);

	h1[0] = 'I';
	h1[1] = 'S';

	h1[2] = cmd >> 8;
	h1[3] = cmd;

	h1[4] = 0x00;
	h1[5] = 0x0C; /* Don't know what's that */

	DBG(9, "H1 [0-3]: %02x %02x %02x %02x\n", h1[0], h1[1], h1[2], h1[3]);

	if((cmd >> 8) == 0x20) {
		h1[6] = (buf_size + 8) >> 24;
		h1[7] = (buf_size + 8) >> 16;
		h1[8] = (buf_size + 8) >> 8;
		h1[9] = (buf_size + 8);

		DBG(9, "H1 [6-9]: %02x %02x %02x %02x\n", h1[6], h1[7], h1[8], h1[9]);

		h2[0] = (buf_size) >> 24;
		h2[1] = (buf_size) >> 16;
		h2[2] = (buf_size) >> 8;
		h2[3] = buf_size;

		h2[4] = reply_len >> 24;
		h2[5] = reply_len >> 16;
		h2[6] = reply_len >> 8;
		h2[7] = reply_len;

		DBG(9, "H2(%d): %02x %02x %02x %02x\n", buf_size, h2[0], h2[1], h2[2], h2[3]);
		DBG(9, "H2(%d): %02x %02x %02x %02x\n", reply_len, h2[4], h2[5], h2[6], h2[7]);
	}

	if (buf_size && (cmd >> 8) == 0x20) {
		memcpy(payload, buf, buf_size);
		sanei_tcp_write(s->fd, packet, 12 + 8 + buf_size);
	}
	else
		sanei_tcp_write(s->fd, packet, 12);

	free(packet);

	*status = SANE_STATUS_GOOD;
	return buf_size;
}

SANE_Status
sanei_epson_net_lock(struct Epson_Scanner *s)
{
	SANE_Status status;
	unsigned char buf[1];

	sanei_epson_net_write(s, 0x2100, NULL, 0, 0, &status);
	sanei_epson_net_read(s, buf, 1, &status);
	return status;
}

SANE_Status
sanei_epson_net_unlock(struct Epson_Scanner *s)
{
	SANE_Status status;
	unsigned char buf[1];

	sanei_epson_net_write(s, 0x2101, NULL, 0, 0, &status);
	sanei_epson_net_read(s, buf, 1, &status);
	return status;
}
