/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 Andreas Beck
   This file is part of the SANE package.

   SANE is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License, or (at your
   option) any later version.

   SANE is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with sane; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The SANE network daemon.  This is the counterpart to the NET
   backend.
 */

#ifdef _AIX
# include <lalloca.h>		/* MUST come first for AIX! */
#endif

#include <sane/config.h>
#include <lalloca.h>
#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#ifdef HAVE_LIBC_H
# include <libc.h>		/* NeXTStep/OpenStep */
#endif

#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#include <netinet/in.h>

#include <stdarg.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#include <sane/sane.h>
#include <sane/sanei.h>
#include <sane/sanei_net.h>
#include <sane/sanei_codec_bin.h>
#include <sane/sanei_config.h>

#include "../include/sane/sanei_auth.h"

#ifndef EXIT_SUCCESS
# define EXIT_SUCCESS   0
#endif

#ifndef IN_LOOPBACK
# define IN_LOOPBACK(addr) (addr == 0x7f000001L)
#endif

#ifndef MAXHOSTNAMELEN
# define MAXHOSTNAMELEN 120
#endif

#define SANED_CONFIG_FILE "saned.conf"



typedef struct
{
  u_int inuse:1;		/* is this handle in use? */
  u_int scanning:1;		/* are we scanning? */
  u_int docancel:1;		/* cancel the current scan */
  SANE_Handle handle;		/* backends handle */
}
Handle;

static SANE_Net_Procedure_Number current_request;
static const char *prog_name;
static int can_authorize;
static Wire wire;
static int num_handles;
static int debug;
static Handle *handle;
static union
{
  int w;
  u_char ch;
}
byte_order;

/* The default-user name.  This is not used to imply any rights.  All
   it does is save a remote user some work by reducing the amount of
   text s/he has to type when authentication is requested.  */
static const char *default_username = "saned-user";
static char hostname[MAXHOSTNAMELEN];
static char *remote_hostname;

#ifndef _PATH_HEQUIV
# define _PATH_HEQUIV   "/etc/hosts.equiv"
#endif

static const char *config_file_names[] = {
  _PATH_HEQUIV, SANED_CONFIG_FILE
};

/* forward declarations: */
static void process_request (Wire * w);

#define DBG	saned_debug_call

static void
saned_debug_call (int level, const char *fmt, ...)
{
#ifndef NDEBUG
  va_list ap;
  va_start (ap, fmt);
  if (debug >= level)
    vsyslog (LOG_DEBUG, fmt, ap);
  va_end (ap);
#endif
}


static void
reset_watchdog (void)
{
  if (!debug)
    alarm (300);
}

static void
auth_callback (SANE_String_Const res,
	       SANE_Char username[SANE_MAX_USERNAME_LEN],
	       SANE_Char password[SANE_MAX_PASSWORD_LEN])
{
  SANE_Net_Procedure_Number procnum;
  SANE_Authorization_Req req;
  SANE_Word word, ack = 0;

  memset (username, 0, sizeof (username));
  memset (password, 0, sizeof (password));

  if (!can_authorize)
    {
      syslog (LOG_ERR,
	      "auth_callback(resource=%s) during non-authorizable RPC\n",
	      res);
      return;
    }

  switch (current_request)
    {
    case SANE_NET_OPEN:
      {
	SANE_Open_Reply reply;

	memset (&reply, 0, sizeof (reply));
	reply.resource_to_authorize = (char *) res;
	sanei_w_reply (&wire, (WireCodecFunc) sanei_w_open_reply, &reply);
      }
      break;

    case SANE_NET_CONTROL_OPTION:
      {
	SANE_Control_Option_Reply reply;

	memset (&reply, 0, sizeof (reply));
	reply.resource_to_authorize = (char *) res;
	sanei_w_reply (&wire,
		       (WireCodecFunc) sanei_w_control_option_reply, &reply);
      }
      break;

    case SANE_NET_START:
      {
	SANE_Start_Reply reply;

	memset (&reply, 0, sizeof (reply));
	reply.resource_to_authorize = (char *) res;
	sanei_w_reply (&wire, (WireCodecFunc) sanei_w_start_reply, &reply);
      }
      break;

    default:
      syslog (LOG_ERR, "auth_callback(resource=%s) for request %d\n",
	      res, current_request);
      break;
    }
  reset_watchdog ();

  sanei_w_set_dir (&wire, WIRE_DECODE);
  sanei_w_word (&wire, &word);
  procnum = word;
  if (procnum != SANE_NET_AUTHORIZE)
    {
      syslog (LOG_ERR,
	      "auth_callback(resource=%s): bad procedure number %d\n", res,
	      procnum);
      return;
    }

  sanei_w_authorization_req (&wire, &req);
  if (req.username)
    strcpy (username, req.username);
  if (req.password)
    strcpy (password, req.password);
  if (!req.resource || strcmp (req.resource, res) != 0)
    {
      syslog (LOG_WARNING,
	      "auth_callback(resource=%s): got auth for resource %s\n",
	      res, req.resource);
    }
  sanei_w_free (&wire, (WireCodecFunc) sanei_w_authorization_req, &req);
  sanei_w_reply (&wire, (WireCodecFunc) sanei_w_word, &ack);
}

static void
quit (int signum)
{
  static int running = 0;
  int i;

  if (signum)
    DBG (1, "received signal %d\n", signum);

  if (running)
    {
      DBG (1, "quit is already active, returning\n");
      return;
    }
  running = 1;

  for (i = 0; i < num_handles; ++i)
    if (handle[i].inuse)
      sane_close (handle[i].handle);

  sane_exit ();
  syslog (LOG_INFO, "exiting\n");
  closelog ();
  exit (EXIT_SUCCESS);		/* This is a nowait-daemon. */
}

static SANE_Word
get_free_handle (void)
{
# define ALLOC_INCREMENT        16
  static int h, last_handle_checked = -1;

  if (num_handles > 0)
    {
      h = last_handle_checked + 1;
      do
	{
	  if (h >= num_handles)
	    h = 0;
	  if (!handle[h].inuse)
	    {
	      last_handle_checked = h;
	      memset (handle + h, 0, sizeof (handle[0]));
	      handle[h].inuse = 1;
	      return h;
	    }
	  ++h;
	}
      while (h != last_handle_checked);
    }

  /* we're out of handles---alloc some more: */
  last_handle_checked = num_handles - 1;
  num_handles += ALLOC_INCREMENT;
  if (handle)
    handle = realloc (handle, num_handles * sizeof (handle[0]));
  else
    handle = malloc (num_handles * sizeof (handle[0]));
  if (!handle)
    return -1;
  memset (handle + last_handle_checked + 1, 0,
	  ALLOC_INCREMENT * sizeof (handle[0]));
  return get_free_handle ();
# undef ALLOC_INCREMENT
}

static void
close_handle (int h)
{
  if (handle[h].inuse)
    sane_close (handle[h].handle);
  handle[h].inuse = 0;
}

static SANE_Word
decode_handle (Wire * w, const char *op)
{
  SANE_Word h;

  sanei_w_word (w, &h);
  if (w->status || (unsigned) h >= num_handles || !handle[h].inuse)
    {
      syslog (LOG_WARNING,
	      "%s: error while decoding handle argument (h=%d, %s)\n",
	      op, h, strerror (w->status));
      return -1;
    }
  return h;
}

/* Check hostnames ignoring case as DNS should. */
static SANE_Status
check_host (int fd)
{
  struct sockaddr_in sin;
  int i, j, access_ok = 0;
  struct hostent *he;
  char rhost[1024];
  int len;
  FILE *fp;

  if (gethostname (hostname, sizeof (hostname)) < 0)
    {
      DBG (1, "gethostname: %s\n", strerror (errno));
      return SANE_STATUS_INVAL;
    }

  /* first, check whether we allow connections from the peer-host: */
  len = sizeof (sin);
  if (getpeername (fd, (struct sockaddr *) &sin, &len) < 0)
    {
      DBG (1, "getpeername: %s\n", strerror (errno));
      return SANE_STATUS_INVAL;
    }
  he = gethostbyaddr ((const char *) &sin.sin_addr,
		      sizeof (sin.sin_addr), sin.sin_family);
  if (!he)
    {
      DBG (1, "gethostbyaddr: %s\n", strerror (errno));
      return SANE_STATUS_INVAL;
    }

  remote_hostname = strdup (he->h_name);

  /* always allow access from local host: */

  if (IN_LOOPBACK (ntohl (sin.sin_addr.s_addr)))
    return SANE_STATUS_GOOD;

  if (strcasecmp (hostname, he->h_name) == 0)
    return SANE_STATUS_GOOD;

  /* check alias list: */
  for (i = 0; he->h_aliases[i]; ++i)
    if (strcasecmp (hostname, he->h_aliases[i]) == 0)
      return SANE_STATUS_GOOD;

  /* must be a remote host: check contents of PATH_NET_CONFIG or
     /etc/hosts.equiv if former doesn't exist: */

  for (j = 0; j < NELEMS (config_file_names); ++j)
    {
      if (config_file_names[j][0] == '/')
	fp = fopen (config_file_names[j], "r");
      else
	fp = sanei_config_open (config_file_names[j]);
      if (!fp)
	continue;

      while (!access_ok && sanei_config_read (rhost, sizeof (rhost), fp))
	{
	  if (rhost[0] == '#')	/* ignore line comments */
	    continue;
	  len = strlen (rhost);
	  if (rhost[len - 1] == '\n')
	    rhost[--len] = '\0';

	  if (!len)
	    continue;		/* ignore empty lines */

	  if (strcasecmp (rhost, he->h_name) == 0 || strcmp (rhost, "+") == 0)
	    access_ok = 1;
	  for (i = 0; he->h_aliases[i]; ++i)
	    if (strcasecmp (rhost, he->h_aliases[i]) == 0)
	      {
		access_ok = 1;
		break;
	      }
	}
      fclose (fp);
      if (access_ok)
	return SANE_STATUS_GOOD;
    }
  return SANE_STATUS_INVAL;
}

static int
init (Wire * w)
{
  SANE_Word word, be_version_code;
  SANE_Init_Reply reply;
  SANE_Status status;
  SANE_Init_Req req;

  reset_watchdog ();

  sanei_w_set_dir (w, WIRE_DECODE);
  sanei_w_word (w, &word);	/* decode procedure number */
  sanei_w_init_req (w, &req);
  w->version = SANEI_NET_PROTOCOL_VERSION;

  if (w->status || word != SANE_NET_INIT)
    {
      syslog (LOG_WARNING, "init: bad status=%d or procnum=%d\n",
	      w->status, word);
      return -1;
    }
  if (req.username)
    default_username = strdup (req.username);

  sanei_w_free (w, (WireCodecFunc) sanei_w_init_req, &req);

  reply.version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR,
					  SANEI_NET_PROTOCOL_VERSION);

  status = check_host (w->io.fd);

  syslog (LOG_NOTICE, "access by %s@%s %s\n",
	  default_username, remote_hostname,
	  (status == SANE_STATUS_GOOD) ? "accepted" : "rejected");

  if (status == SANE_STATUS_GOOD)
    {
      status = sane_init (&be_version_code, auth_callback);
      if (status != SANE_STATUS_GOOD)
	DBG (1, "failed to initialize backend (%s)\n",
	     sane_strstatus (status));

      if (SANE_VERSION_MAJOR (be_version_code) != V_MAJOR)
	{
	  syslog (LOG_ERR,
		  "unexpected backend major version %d (expected %d)\n",
		  SANE_VERSION_MAJOR (be_version_code), V_MAJOR);
	  status = SANE_STATUS_INVAL;
	}
    }
  reply.status = status;
  if (status != SANE_STATUS_GOOD)
    reply.version_code = 0;
  sanei_w_reply (w, (WireCodecFunc) sanei_w_init_reply, &reply);

  if (w->status || status != SANE_STATUS_GOOD)
    return -1;

  return 0;
}

static int
start_scan (Wire * w, int h, SANE_Start_Reply * reply)
{
  struct sockaddr_in sin;
  SANE_Handle be_handle;
  int fd, len;

  be_handle = handle[h].handle;

  len = sizeof (sin);
  if (getsockname (w->io.fd, (struct sockaddr *) &sin, &len) < 0)
    {
      syslog (LOG_ERR, "start_scan: failed to obtain socket address (%s)\n",
	      strerror (errno));
      reply->status = SANE_STATUS_IO_ERROR;
      return -1;
    }

  fd = socket (AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    {
      syslog (LOG_ERR, "start_scan: failed to obtain data socket (%s)\n",
	      strerror (errno));
      reply->status = SANE_STATUS_IO_ERROR;
      return -1;
    }

  sin.sin_port = 0;
  if (bind (fd, (struct sockaddr *) &sin, len) < 0)
    {
      syslog (LOG_ERR, "start_scan: failed to bind address (%s)\n",
	      strerror (errno));
      reply->status = SANE_STATUS_IO_ERROR;
      return -1;
    }

  if (listen (fd, 1) < 0)
    {
      syslog (LOG_ERR, "start_scan: failed to make socket listen (%s)\n",
	      strerror (errno));
      reply->status = SANE_STATUS_IO_ERROR;
      return -1;
    }

  if (getsockname (fd, (struct sockaddr *) &sin, &len) < 0)
    {
      syslog (LOG_ERR, "start_scan: failed to obtain socket address (%s)\n",
	      strerror (errno));
      reply->status = SANE_STATUS_IO_ERROR;
      return -1;
    }

  reply->port = ntohs (sin.sin_port);

  DBG (3, "start_scan: using port %d for data\n", reply->port);

  reply->status = sane_start (be_handle);
  if (reply->status == SANE_STATUS_GOOD)
    {
      handle[h].scanning = 1;
      handle[h].docancel = 0;
    }

  return fd;
}

static int
store_reclen (SANE_Byte * buf, size_t buf_size, int i, size_t reclen)
{
  buf[i++] = (reclen >> 24) & 0xff;
  if (i >= buf_size)
    i = 0;
  buf[i++] = (reclen >> 16) & 0xff;
  if (i >= buf_size)
    i = 0;
  buf[i++] = (reclen >> 8) & 0xff;
  if (i >= buf_size)
    i = 0;
  buf[i++] = (reclen >> 0) & 0xff;
  if (i >= buf_size)
    i = 0;
  return i;
}

static void
do_scan (Wire * w, int h, int data_fd)
{
  int num_fds, be_fd = -1, reader, writer, bytes_in_buf, status_dirty = 0;
  SANE_Handle be_handle = handle[h].handle;
  struct timeval tv, *timeout = 0;
  fd_set rd_set, rd_mask, wr_set, wr_mask;
  SANE_Byte buf[8192];
  SANE_Status status;
  long int nwritten;
  SANE_Int length;
  size_t nbytes;

  FD_ZERO (&rd_mask);
  FD_SET (w->io.fd, &rd_mask);
  num_fds = w->io.fd + 1;

  FD_ZERO (&wr_mask);
  FD_SET (data_fd, &wr_mask);
  if (data_fd >= num_fds)
    num_fds = data_fd + 1;

  sane_set_io_mode (be_handle, SANE_TRUE);
  if (sane_get_select_fd (be_handle, &be_fd) == SANE_STATUS_GOOD)
    {
      FD_SET (be_fd, &rd_mask);
      if (be_fd >= num_fds)
	num_fds = be_fd + 1;
    }
  else
    {
      memset (&tv, 0, sizeof (tv));
      timeout = &tv;
    }

  status = SANE_STATUS_GOOD;
  reader = writer = bytes_in_buf = 0;
  do
    {
      rd_set = rd_mask;
      wr_set = wr_mask;
      if (select (num_fds, &rd_set, &wr_set, 0, timeout) < 0)
	{
	  if (be_fd >= 0 && errno == EBADF)
	    {
	      /* This normally happens when a backend closes a select
	         filedescriptor when reaching the end of file.  So
	         pass back this status to the client: */
	      FD_CLR (be_fd, &rd_mask);
	      be_fd = -1;
	      status = SANE_STATUS_EOF;
	      status_dirty = 1;
	      continue;
	    }
	  else
	    {
	      status = SANE_STATUS_IO_ERROR;
	      DBG (1, "do_scan: select failed (%s)\n", strerror (errno));
	      break;
	    }
	}

      if (bytes_in_buf)
	{
	  if (FD_ISSET (data_fd, &wr_set))
	    {
	      if (bytes_in_buf > 0)
		{
		  /* write more input data */
		  nbytes = bytes_in_buf;
		  if (writer + nbytes > sizeof (buf))
		    nbytes = sizeof (buf) - writer;
		  nwritten = write (data_fd, buf + writer, nbytes);
		  DBG (4, "do_scan: wrote %ld bytes to client\n", nwritten);
		  if (nwritten < 0)
		    {
		      syslog (LOG_WARNING, "do_scan: write failed (%s)\n",
			      strerror (errno));
		      status = SANE_STATUS_CANCELLED;
		      break;
		    }
		  bytes_in_buf -= nwritten;
		  writer += nwritten;
		  if (writer == sizeof (buf))
		    writer = 0;
		}
	    }
	}
      else if (status == SANE_STATUS_GOOD
	       && (timeout || FD_ISSET (be_fd, &rd_set)))
	{
	  int i;

	  /* get more input data */

	  /* reserve 4 bytes to store the length of the data record: */
	  i = reader;
	  reader += 4;
	  if (reader >= sizeof (buf))
	    reader = 0;

	  assert (bytes_in_buf == 0);
	  nbytes = sizeof (buf) - 4;
	  if (reader + nbytes > sizeof (buf))
	    nbytes = sizeof (buf) - reader;

	  status = sane_read (be_handle, buf + reader, nbytes, &length);
	  DBG (4, "do_scan: read %d bytes from scanner\n", length);

	  reset_watchdog ();

	  reader += length;
	  if (reader >= sizeof (buf))
	    reader = 0;
	  bytes_in_buf += length + 4;

	  if (status != SANE_STATUS_GOOD)
	    {
	      reader = i;	/* restore reader index */
	      status_dirty = 1;
	    }
	  else
	    store_reclen (buf, sizeof (buf), i, length);
	}

      if (status_dirty && sizeof (buf) - bytes_in_buf >= 5)
	{
	  status_dirty = 0;
	  reader = store_reclen (buf, sizeof (buf), reader, 0xffffffff);
	  buf[reader] = status;
	  bytes_in_buf += 5;
	}

      if (FD_ISSET (w->io.fd, &rd_set))
	{
	  DBG (4, "do_scan: processing RPC request on fd %d\n", w->io.fd);
	  process_request (w);
	  if (handle[h].docancel)
	    break;
	}
    }
  while (status == SANE_STATUS_GOOD || bytes_in_buf > 0 || status_dirty);
  DBG (2, "do_scan: done, status=%s\n", sane_strstatus (status));
  handle[h].docancel = 0;
  handle[h].scanning = 0;
}

static void
process_request (Wire * w)
{
  SANE_Handle be_handle;
  SANE_Word h, word;
  int i;

  sanei_w_set_dir (w, WIRE_DECODE);
  sanei_w_word (w, &word);	/* decode procedure number */
  current_request = word;

  DBG (2, "process_request: got request %d\n", current_request);

  switch (current_request)
    {
    case SANE_NET_GET_DEVICES:
      {
	SANE_Get_Devices_Reply reply;

	reply.status =
	  sane_get_devices ((const SANE_Device ***) &reply.device_list,
			    SANE_TRUE);
	sanei_w_reply (w, (WireCodecFunc) sanei_w_get_devices_reply, &reply);
      }
      break;

    case SANE_NET_OPEN:
      {
	SANE_Open_Reply reply;
	SANE_Handle be_handle;
	SANE_String name, resource;

	sanei_w_string (w, &name);
	if (w->status)
	  {
	    syslog (LOG_WARNING, "open: error while decoding args (%s)\n",
		    strerror (w->status));
	    return;
	  }

	can_authorize = 1;

	resource = strdup (name);

	if (strchr (resource, ':'))
	  *(strchr (resource, ':')) = 0;

	if (sanei_authorize (resource, "saned", auth_callback) !=
	    SANE_STATUS_GOOD)
	  {


	    free (resource);
	    memset (&reply, 0, sizeof (reply));	/* avoid leaking bits */
	    reply.status = SANE_STATUS_ACCESS_DENIED;

	  }
	else
	  {

	    free (resource);

	    memset (&reply, 0, sizeof (reply));	/* avoid leaking bits */
	    reply.status = sane_open (name, &be_handle);
	  }
	if (reply.status == SANE_STATUS_GOOD)
	  {
	    h = get_free_handle ();
	    if (h < 0)
	      reply.status = SANE_STATUS_NO_MEM;
	    else
	      {
		handle[h].handle = be_handle;
		reply.handle = h;
	      }
	  }

	can_authorize = 0;

	sanei_w_reply (w, (WireCodecFunc) sanei_w_open_reply, &reply);
      }
      break;

    case SANE_NET_CLOSE:
      {
	SANE_Word ack = 0;

	h = decode_handle (w, "close");
	close_handle (h);
	sanei_w_reply (w, (WireCodecFunc) sanei_w_word, &ack);
      }
      break;

    case SANE_NET_GET_OPTION_DESCRIPTORS:
      {
	SANE_Option_Descriptor_Array opt;

	h = decode_handle (w, "get_option_descriptors");
	if (h < 0)
	  return;
	be_handle = handle[h].handle;
	sane_control_option (be_handle, 0, SANE_ACTION_GET_VALUE,
			     &opt.num_options, 0);

	opt.desc = malloc (opt.num_options * sizeof (opt.desc[0]));
	for (i = 0; i < opt.num_options; ++i)
	  opt.desc[i] = (SANE_Option_Descriptor *)
	    sane_get_option_descriptor (be_handle, i);

	sanei_w_reply (w,
		       (WireCodecFunc) sanei_w_option_descriptor_array, &opt);

	free (opt.desc);
      }
      break;

    case SANE_NET_CONTROL_OPTION:
      {
	SANE_Control_Option_Req req;
	SANE_Control_Option_Reply reply;

	sanei_w_control_option_req (w, &req);
	if (w->status || (unsigned) req.handle >= num_handles
	    || !handle[req.handle].inuse)
	  {
	    syslog (LOG_WARNING,
		    "control_option: error while decoding args h=%d "
		    "(%s)\n", req.handle, strerror (w->status));
	    return;
	  }

	can_authorize = 1;

	memset (&reply, 0, sizeof (reply));	/* avoid leaking bits */
	be_handle = handle[req.handle].handle;
	reply.status = sane_control_option (be_handle, req.option,
					    req.action, req.value,
					    &reply.info);
	reply.value_type = req.value_type;
	reply.value_size = req.value_size;
	reply.value = req.value;

	can_authorize = 0;

	sanei_w_reply (w,
		       (WireCodecFunc) sanei_w_control_option_reply, &reply);
      }
      break;

    case SANE_NET_GET_PARAMETERS:
      {
	SANE_Get_Parameters_Reply reply;

	h = decode_handle (w, "get_parameters");
	if (h < 0)
	  return;
	be_handle = handle[h].handle;

	reply.status = sane_get_parameters (be_handle, &reply.params);

	sanei_w_reply (w,
		       (WireCodecFunc) sanei_w_get_parameters_reply, &reply);
      }
      break;

    case SANE_NET_START:
      {
	SANE_Start_Reply reply;
	int fd = -1, data_fd;

	h = decode_handle (w, "start");
	if (h < 0)
	  return;

	memset (&reply, 0, sizeof (reply));	/* avoid leaking bits */
	reply.byte_order = SANE_NET_LITTLE_ENDIAN;
	if (byte_order.w != 1)
	  reply.byte_order = SANE_NET_BIG_ENDIAN;

	if (handle[h].scanning)
	  reply.status = SANE_STATUS_DEVICE_BUSY;
	else
	  fd = start_scan (w, h, &reply);

	sanei_w_reply (w, (WireCodecFunc) sanei_w_start_reply, &reply);

	if (reply.status == SANE_STATUS_GOOD)
	  {
	    data_fd = accept (fd, 0, 0);	/* XXX may want to verify peer */
	    close (fd);
	    if (data_fd < 0)
	      {
		sane_cancel (handle[h].handle);
		handle[h].scanning = 0;
		handle[h].docancel = 0;
		syslog (LOG_ERR, "process_request: accept failed! (%s)\n",
			strerror (errno));
		return;
	      }
	    fcntl (data_fd, F_SETFL, 1);	/* set non-blocking */
	    shutdown (data_fd, 0);
	    do_scan (w, h, data_fd);
	    close (data_fd);
	  }
      }
      break;

    case SANE_NET_CANCEL:
      {
	SANE_Word ack = 0;

	h = decode_handle (w, "cancel");
	sane_cancel (handle[h].handle);
	handle[h].docancel = 1;
	sanei_w_reply (w, (WireCodecFunc) sanei_w_word, &ack);
      }
      break;

    case SANE_NET_EXIT:
      quit (0);
      break;

    case SANE_NET_INIT:
    case SANE_NET_AUTHORIZE:
    default:
      syslog (LOG_WARNING,
	      "process_request: received unexpected procedure number %d\n",
	      current_request);
      quit (0);
    }
}

int
main (int argc, char *argv[])
{
  int fd, on = 1;
#ifdef TCP_NODELAY
  int level = -1;
#endif

  openlog ("saned", LOG_PID | LOG_CONS, LOG_DAEMON);

  prog_name = strrchr (argv[0], '/');
  if (prog_name)
    ++prog_name;
  else
    prog_name = argv[0];

  byte_order.w = 0;
  byte_order.ch = 1;

  sanei_w_init (&wire, sanei_codec_bin_init);
  wire.io.read = read;
  wire.io.write = write;

  if (argc == 2 && strncmp (argv[1], "-d", 2) == 0)
    {
      /* don't operate in daemon mode: wait for connection request: */
      struct sockaddr_in sin;
      struct servent *serv;
      short port;

      debug = 1;
      if (argv[1][2])
	debug = atoi (argv[1] + 2);

      memset (&sin, 0, sizeof (sin));

      serv = getservbyname ("sane", "tcp");
      if (serv)
	port = serv->s_port;
      else
	{
	  port = htons (6566);
	  fprintf (stderr, "%s: could not find `sane' service (%s)\n"
		   "%s: using default port %d\n", prog_name, strerror (errno),
		   prog_name, ntohs (port));
	}
      sin.sin_family = AF_INET;
      sin.sin_addr.s_addr = INADDR_ANY;
      sin.sin_port = port;

      fd = socket (AF_INET, SOCK_STREAM, 0);

      if (setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on)))
	syslog (LOG_ERR, "failed to put socket in SO_REUSEADDR mode (%s)",
		strerror (errno));

      if (bind (fd, (struct sockaddr *) &sin, sizeof (sin)) < 0)
	{
	  perror ("bind");
	  exit (1);
	}
      if (listen (fd, 1) < 0)
	{
	  perror ("listen");
	  exit (1);
	}
      wire.io.fd = accept (fd, 0, 0);
      if (wire.io.fd < 0)
	{
	  perror ("accept");
	  exit (1);
	}
      close (fd);
    }
  else
    /* use filedescriptor opened by inetd: */
#ifdef HAVE_OS2_H
    /* under OS/2, the socket handle is passed as argument on the command
       line; the socket handle is relative to IBM TCP/IP, so a call
       to impsockethandle() is required to add it to the EMX runtime */
  if (argc == 2)
    {
      wire.io.fd = _impsockhandle (atoi (argv[1]), 0);
      if (wire.io.fd == -1)
	perror ("impsockhandle");
    }
  else
#endif /* HAVE_OS2_H */
    wire.io.fd = 1;

  signal (SIGALRM, quit);
  signal (SIGPIPE, quit);

#ifdef TCP_NODELAY
# ifdef SOL_TCP
  level = SOL_TCP;
# else /* !SOL_TCP */
  /* Look up the protocol level in the protocols database. */
  {
    struct protoent *p;
    p = getprotobyname ("tcp");
    if (p == 0)
      {
	DBG (1, "connect_dev: cannot look up `tcp' protocol number");
      }
    else
      level = p->p_proto;
  }
# endif	/* SOL_TCP */
  if (level == -1
      || setsockopt (wire.io.fd, level, TCP_NODELAY, &on, sizeof (on)))
    syslog (LOG_ERR, "failed to put socket in TCP_NODELAY mode (%s)",
	    strerror (errno));
#endif /* !TCP_NODELAY */

  if (init (&wire) < 0)
    quit (0);

  while (1)
    {
      reset_watchdog ();
      process_request (&wire);
    }
}
