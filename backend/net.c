/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 David Mosberger-Tang
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

   This file implements a SANE network-based meta backend.  */

/* Please increase version number with every change 
   (don't forget to update net.desc) */
#define NET_VERSION "1.0.3"

#ifdef _AIX
# include "../include/lalloca.h"	/* MUST come first for AIX! */
#endif

#include "../include/sane/config.h"
#include "../include/lalloca.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_LIBC_H
# include <libc.h>		/* NeXTStep/OpenStep */
#endif

#include <sys/time.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <netdb.h>		/* OS/2 needs this _after_ <netinet/in.h>, grrr... */

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/sanei_net.h"
#include "../include/sane/sanei_codec_bin.h"
#include "net.h"

#define BACKEND_NAME    net
#include "../include/sane/sanei_backend.h"

#ifndef PATH_MAX
# define PATH_MAX       1024
#endif

#include "../include/sane/sanei_config.h"
#define NET_CONFIG_FILE "net.conf"

static SANE_Auth_Callback auth_callback;
static Net_Device *first_device;
static Net_Scanner *first_handle;
static const SANE_Device **devlist;
static int saned_port;

static SANE_Status
add_device (const char *name, Net_Device ** ndp)
{
  struct hostent *he;
  Net_Device *nd;

  DBG (1, "add_device: adding backend %s\n", name);

  he = gethostbyname (name);
  if (!he)
    {
      DBG (1, "add_device: can't get address of host %s\n", name);
      return SANE_STATUS_IO_ERROR;
    }

  if (he->h_addrtype != AF_INET)
    {
      DBG (1, "add_device: don't know how to deal with addr family %d\n",
	   he->h_addrtype);
      return SANE_STATUS_INVAL;
    }

  nd = malloc (sizeof (*nd));
  if (!nd)
    return SANE_STATUS_NO_MEM;

  memset (nd, 0, sizeof (*nd));
  nd->name = strdup (name);
  if (!nd->name)
    return SANE_STATUS_NO_MEM;
  nd->addr.sa_family = he->h_addrtype;
  if (nd->addr.sa_family == AF_INET)
    {
      struct sockaddr_in *sin = (struct sockaddr_in *) &nd->addr;
      memcpy (&sin->sin_addr, he->h_addr_list[0], he->h_length);
    }

  nd->ctl = -1;
  nd->next = first_device;
  first_device = nd;
  if (ndp)
    *ndp = nd;
  DBG (2, "add_device: backend %s added\n", name);
  return SANE_STATUS_GOOD;
}

static SANE_Status
connect_dev (Net_Device * dev)
{
  struct sockaddr_in *sin;
  SANE_Word version_code;
  SANE_Init_Reply reply;
  SANE_Status status;
  SANE_Init_Req req;
#ifdef TCP_NODELAY
  int on = 1;
  int level = -1;
#endif

  if (dev->addr.sa_family != AF_INET)
    {
      DBG (1, "connect_dev: don't know how to deal with addr family %d\n",
	   dev->addr.sa_family);
      return SANE_STATUS_IO_ERROR;
    }

  dev->ctl = socket (dev->addr.sa_family, SOCK_STREAM, 0);
  if (dev->ctl < 0)
    {
      DBG (1, "connect_dev: failed to obtain socket (%s)\n",
	   strerror (errno));
      dev->ctl = -1;
      return SANE_STATUS_IO_ERROR;
    }
  sin = (struct sockaddr_in *) &dev->addr;
  sin->sin_port = saned_port;

  if (connect (dev->ctl, &dev->addr, sizeof (dev->addr)) < 0)
    {
      DBG (1, "connect_dev: failed to connect (%s)\n", strerror (errno));
      dev->ctl = -1;
      return SANE_STATUS_IO_ERROR;
    }

#ifdef TCP_NODELAY
# ifdef SOL_TCP
  level = SOL_TCP;
# else /* !SOL_TCP */
  /* Look up the protocol level in the protocols database. */
  {
    struct protoent *p;
    p = getprotobyname ("tcp");
    if (p == 0)
      DBG (1, "connect_dev: cannot look up `tcp' protocol number");
    else
      level = p->p_proto;
  }
# endif	/* SOL_TCP */

  if (level == -1 ||
      setsockopt (dev->ctl, level, TCP_NODELAY, &on, sizeof (on)))
    DBG (1, "connect_dev: failed to put send socket in TCP_NODELAY mode (%s)",
	 strerror (errno));
#endif /* !TCP_NODELAY */

  sanei_w_init (&dev->wire, sanei_codec_bin_init);
  dev->wire.io.fd = dev->ctl;
  dev->wire.io.read = read;
  dev->wire.io.write = write;

  /* exchange version codes with the server: */
  req.version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR,
					SANEI_NET_PROTOCOL_VERSION);
  req.username = getlogin ();
  sanei_w_call (&dev->wire, SANE_NET_INIT,
		(WireCodecFunc) sanei_w_init_req, &req,
		(WireCodecFunc) sanei_w_init_reply, &reply);

  if (dev->wire.status != 0)
    {
      DBG (1, "connect_dev: argument marshalling error (%s)\n",
	   strerror (dev->wire.status));
      goto fail;
    }

  status = reply.status;
  version_code = reply.version_code;

  sanei_w_free (&dev->wire, (WireCodecFunc) sanei_w_init_reply, &reply);

  if (SANE_VERSION_MAJOR (version_code) != V_MAJOR)
    {
      DBG (1, "connect_dev: major version mismatch: got %d, expected %d\n",
	   SANE_VERSION_MAJOR (version_code), V_MAJOR);
      goto fail;
    }
  if (SANE_VERSION_BUILD (version_code) != SANEI_NET_PROTOCOL_VERSION
      && SANE_VERSION_BUILD (version_code) != 2)
    {
      DBG (1, "connect_dev: network protocol version mismatch: "
	   "got %d, expected %d\n",
	   SANE_VERSION_BUILD (version_code), SANEI_NET_PROTOCOL_VERSION);
      goto fail;
    }
  dev->wire.version = SANE_VERSION_BUILD (version_code);
  return SANE_STATUS_GOOD;

fail:
  close (dev->ctl);
  dev->ctl = -1;
  return SANE_STATUS_IO_ERROR;
}

static SANE_Status
fetch_options (Net_Scanner * s)
{
  DBG(3, "fetch_options\n");

  if (s->opt.num_options)
    {
      sanei_w_set_dir (&s->hw->wire, WIRE_FREE);
      s->hw->wire.status = 0;
      sanei_w_option_descriptor_array (&s->hw->wire, &s->opt);
      if (s->hw->wire.status)
	return SANE_STATUS_IO_ERROR;
    }
  sanei_w_call (&s->hw->wire, SANE_NET_GET_OPTION_DESCRIPTORS,
		(WireCodecFunc) sanei_w_word, &s->handle,
		(WireCodecFunc) sanei_w_option_descriptor_array, &s->opt);
  if (s->hw->wire.status)
    return SANE_STATUS_IO_ERROR;

  s->options_valid = 1;
  return SANE_STATUS_GOOD;
}

static SANE_Status
do_cancel (Net_Scanner * s)
{
  s->hw->auth_active = 0;
  if (s->data >= 0)
    {
      close (s->data);
      s->data = -1;
    }
  return SANE_STATUS_CANCELLED;
}

static void
do_authorization (Net_Device * dev, SANE_String resource)
{
  SANE_Authorization_Req req;
  SANE_Char username[SANE_MAX_USERNAME_LEN];
  SANE_Char password[SANE_MAX_PASSWORD_LEN];
  char *net_resource;

  dev->auth_active = 1;

  memset (&req, 0, sizeof (req));

  net_resource = malloc (strlen (resource) + 6 + strlen (dev->name));

  if (net_resource != NULL)
    sprintf (net_resource, "net:%s:%s", dev->name, resource);
  else
    {
      SANE_Word ack;

      DBG (1, "do_authorization: not enough memory\n");
      req.resource = resource;
      sanei_w_call (&dev->wire, SANE_NET_AUTHORIZE,
		    (WireCodecFunc) sanei_w_authorization_req, &req,
		    (WireCodecFunc) sanei_w_word, &ack);

      return;
    }

  if (auth_callback)
    (*auth_callback) (net_resource, username, password);

  free (net_resource);

  if (dev->auth_active)
    {
      SANE_Word ack;

      req.resource = resource;
      req.username = username;
      req.password = password;
      sanei_w_call (&dev->wire, SANE_NET_AUTHORIZE,
		    (WireCodecFunc) sanei_w_authorization_req, &req,
		    (WireCodecFunc) sanei_w_word, &ack);
    }
}

SANE_Status sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  char device_name[PATH_MAX];
  struct servent *serv;
  const char *env;
  size_t len;
  FILE *fp;

  DBG_INIT ();

  auth_callback = authorize;

  /* Return the version number of the sane-backends package to allow
     the frontend to print them. This is done only for net and dll,
     because these backends are usually called by the frontend. */
  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_DLL_V_MAJOR, SANE_DLL_V_MINOR,
				       SANE_DLL_V_BUILD);

  DBG(1, "sane_init: SANE net backend version %s from %s\n", NET_VERSION,
      PACKAGE_VERSION);

  serv = getservbyname ("sane", "tcp");

  if (serv)
    saned_port = serv->s_port;
  else
    {
      saned_port = htons (6566);
      DBG (1, "sane_init: could not find `sane' service (%s); using default "
	   "port %d\n", strerror (errno), ntohs (saned_port));
    }

  fp = sanei_config_open (NET_CONFIG_FILE);
  if (fp)
    {
      while (sanei_config_read (device_name, sizeof (device_name), fp))
	{
	  if (device_name[0] == '#')	/* ignore line comments */
	    continue;
	  len = strlen (device_name);

	  if (!len)
	    continue;		/* ignore empty lines */

	  add_device (device_name, 0);

	}
      fclose (fp);
    }
  else
    DBG(1, "sane_init: could not open config file (%s): %s\n", NET_CONFIG_FILE,
	strerror (errno));

  env = getenv ("SANE_NET_HOSTS");
  if (env)
    {
      char *copy, *next, *host;
      copy = strdup (env);
      next = copy;
      while ((host = strsep (&next, ":")))
	add_device (host, 0);
      free (copy);
    }
  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  Net_Scanner *handle, *next_handle;
  Net_Device *dev, *next_device;
  int i;

  DBG (1, "sane_exit: exiting\n");

  /* first, close all handles: */
  for (handle = first_handle; handle; handle = next_handle)
    {
      next_handle = handle->next;
      sane_close (handle);
    }
  first_handle = 0;

  /* now close all devices: */
  for (dev = first_device; dev; dev = next_device)
    {
      next_device = dev->next;

      DBG (2, "sane_exit: closing dev %p, ctl=%d\n", dev, dev->ctl);

      if (dev->ctl >= 0)
	{
	  sanei_w_call (&dev->wire, SANE_NET_EXIT,
			(WireCodecFunc) sanei_w_void, 0,
			(WireCodecFunc) sanei_w_void, 0);
          sanei_w_exit (&dev->wire);
	  close (dev->ctl);
	}
      if (dev->name)
        free((void *) dev->name);
      free (dev);
    }
  if (devlist)
    {
      for (i = 0; devlist[i]; ++i)
	{
	  if (devlist[i]->vendor)
	    free ((void *) devlist[i]->vendor);
	  if (devlist[i]->model)
	    free ((void *) devlist[i]->model);
	  if (devlist[i]->type)
	    free ((void *) devlist[i]->type);
	  free ((void *) devlist[i]);
	}
      free (devlist);
    }
  DBG (3, "sane_exit: finished.\n");
}

/* Note that a call to get_devices() implies that we'll have to
   connect to all remote hosts.  To avoid this, you can call
   sane_open() directly (assuming you know the name of the
   backend/device).  This is appropriate for the command-line
   interface of SANE, for example.
 */
SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  static int devlist_size = 0, devlist_len = 0;
  static const SANE_Device *empty_devlist[1] = { 0 };
  SANE_Get_Devices_Reply reply;
  SANE_Status status;
  Net_Device *dev;
  char *full_name;
  int i, num_devs;
  size_t len;
#define ASSERT_SPACE(n)                                                    \
  {                                                                        \
    if (devlist_len + (n) > devlist_size)                                  \
      {                                                                    \
        devlist_size += (n) + 15;                                          \
        if (devlist)                                                       \
          devlist = realloc (devlist, devlist_size * sizeof (devlist[0])); \
        else                                                               \
          devlist = malloc (devlist_size * sizeof (devlist[0]));           \
        if (!devlist)                                                      \
          return SANE_STATUS_NO_MEM;                                       \
      }                                                                    \
  }

  DBG(3, "sane_get_devices: local_only = %d\n", local_only);

  if (local_only)
    {
      *device_list = empty_devlist;
      return SANE_STATUS_GOOD;
    }

  if (devlist)
    {
      DBG (2, "sane_get_devices: freeing devlist\n");
      for (i = 0; devlist[i]; ++i)
	{
	  if (devlist[i]->vendor)
	    free ((void *) devlist[i]->vendor);
	  if (devlist[i]->model)
	    free ((void *) devlist[i]->model);
	  if (devlist[i]->type)
	    free ((void *) devlist[i]->type);
	  free ((void *) devlist[i]);
	}
      free (devlist);
      devlist = 0;
    }
  devlist_len = 0;
  devlist_size = 0;

  for (dev = first_device; dev; dev = dev->next)
    {
      if (dev->ctl < 0)
	{
	  status = connect_dev (dev);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (1, "sane_get_devices: ignoring failure to connect to %s\n",
		   dev->name);
	      continue;
	    }
	}
      sanei_w_call (&dev->wire, SANE_NET_GET_DEVICES,
		    (WireCodecFunc) sanei_w_void, 0,
		    (WireCodecFunc) sanei_w_get_devices_reply, &reply);
      if (reply.status != SANE_STATUS_GOOD)
	{
	  DBG (1, "sane_get_devices: ignoring rpc-returned status %s\n",
	       sane_strstatus (reply.status));
	  sanei_w_free (&dev->wire,
			(WireCodecFunc) sanei_w_get_devices_reply, &reply);
	  continue;
	}

      /* count the number of devices for this backend: */
      for (num_devs = 0; reply.device_list[num_devs]; ++num_devs);

      ASSERT_SPACE (num_devs);

      for (i = 0; i < num_devs; ++i)
	{
	  SANE_Device *rdev;
	  char *mem;

	  /* create a new device entry with a device name that is the
	     sum of the backend name a colon and the backend's device
	     name: */
	  len = strlen (dev->name) + 1 + strlen (reply.device_list[i]->name);
	  mem = malloc (sizeof (*dev) + len + 1);
	  if (!mem)
	    return SANE_STATUS_NO_MEM;

	  full_name = mem + sizeof (*dev);
	  strcpy (full_name, dev->name);
	  strcat (full_name, ":");
	  strcat (full_name, reply.device_list[i]->name);

	  rdev = (SANE_Device *) mem;
	  rdev->name = full_name;
	  rdev->vendor = strdup (reply.device_list[i]->vendor);
	  rdev->model = strdup (reply.device_list[i]->model);
	  rdev->type = strdup (reply.device_list[i]->type);

	  devlist[devlist_len++] = rdev;
	}
      /* now free up the rpc return value: */
      sanei_w_free (&dev->wire,
		    (WireCodecFunc) sanei_w_get_devices_reply, &reply);
    }

  /* terminate device list with NULL entry: */
  ASSERT_SPACE (1);
  devlist[devlist_len++] = 0;

  *device_list = devlist;
  DBG (2, "sane_get_devices: finished\n");
  return SANE_STATUS_GOOD;
}

SANE_Status sane_open (SANE_String_Const full_name, SANE_Handle * meta_handle)
{
  SANE_Open_Reply reply;
  const char *dev_name;
  SANE_String nd_name;
  SANE_Status status;
  SANE_Word handle;
  Net_Device *dev;
  Net_Scanner *s;
  int need_auth;

  DBG (3, "sane_open(\"%s\")\n", full_name);

  dev_name = strchr (full_name, ':');
  if (dev_name)
    {
#ifdef strndupa
      nd_name = strndupa (full_name, dev_name - full_name);
#else
      char *tmp;

      tmp = alloca (dev_name - full_name + 1);
      memcpy (tmp, full_name, dev_name - full_name);
      tmp[dev_name - full_name] = '\0';
      nd_name = tmp;
#endif
      ++dev_name;		/* skip colon */
    }
  else
    {
      /* if no colon interpret full_name as the host name; an empty
         device name will cause us to open the first device of that
         host.  */
      nd_name = (char *) full_name;
      dev_name = "";
    }

  if (!nd_name[0])
    /* Unlike other backends, we never allow an empty backend-name.
       Otherwise, it's possible that sane_open("") will result in
       endless looping (consider the case where NET is the first
       backend...) */
    return SANE_STATUS_INVAL;
  else
    for (dev = first_device; dev; dev = dev->next)
      if (strcmp (dev->name, nd_name) == 0)
	break;

  if (!dev)
    {
      status = add_device (nd_name, &dev);
      if (status != SANE_STATUS_GOOD)
	return status;
    }

  if (dev->ctl < 0)
    {
      status = connect_dev (dev);
      if (status != SANE_STATUS_GOOD)
	return status;
    }

  sanei_w_call (&dev->wire, SANE_NET_OPEN,
		(WireCodecFunc) sanei_w_string, &dev_name,
		(WireCodecFunc) sanei_w_open_reply, &reply);
  do
    {
      if (dev->wire.status != 0)
	{
	  DBG (1, "sane_open: open rpc call failed (%s)\n",
	       strerror (dev->wire.status));
	  return SANE_STATUS_IO_ERROR;
	}

      status = reply.status;
      handle = reply.handle;
      need_auth = (reply.resource_to_authorize != 0);

      if (need_auth)
	{
	  do_authorization (dev, reply.resource_to_authorize);

	  sanei_w_free (&dev->wire, (WireCodecFunc) sanei_w_open_reply,
			&reply);

	  sanei_w_set_dir (&dev->wire, WIRE_DECODE);
	  sanei_w_open_reply (&dev->wire, &reply);

	  continue;

	}
      else
	sanei_w_free (&dev->wire, (WireCodecFunc) sanei_w_open_reply, &reply);

      if (need_auth && !dev->auth_active)
	return SANE_STATUS_CANCELLED;

      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "sane_open: remote open failed\n");
	  return reply.status;
	}
    }
  while (need_auth);

  s = malloc (sizeof (*s));
  if (!s)
    return SANE_STATUS_NO_MEM;

  memset (s, 0, sizeof (*s));
  s->hw = dev;
  s->handle = handle;
  s->data = -1;
  s->next = first_handle;
  first_handle = s;
  *meta_handle = s;

  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle handle)
{
  Net_Scanner *prev, *s;
  SANE_Word ack;

  DBG(3, "sane_close: handle %p\n", handle);

  prev = 0;
  for (s = first_handle; s; s = s->next)
    {
      if (s == handle)
	break;
      prev = s;
    }
  if (!s)
    {
      DBG (1, "sane_close: invalid handle %p\n", handle);
      return;			/* oops, not a handle we know about */
    }
  if (prev)
    prev->next = s->next;
  else
    first_handle = s->next;

  if (s->opt.num_options)
    {
      sanei_w_set_dir (&s->hw->wire, WIRE_FREE);
      s->hw->wire.status = 0;
      sanei_w_option_descriptor_array (&s->hw->wire, &s->opt);
      if (s->hw->wire.status)
	DBG(1, "sane_close: couldn't free sanei_w_option_descriptor_array "
	    "(%s)\n", sane_strstatus (s->hw->wire.status));
	}

  sanei_w_call (&s->hw->wire, SANE_NET_CLOSE,
		(WireCodecFunc) sanei_w_word, &s->handle,
		(WireCodecFunc) sanei_w_word, &ack);
  if (s->data >= 0)
    close (s->data);
  free (s);
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  Net_Scanner *s = handle;
  SANE_Status status;

  DBG(3, "sane_get_option_descriptor: option %d\n", option);

  if (!s->options_valid)
    {
      status = fetch_options (s);
      if (status != SANE_STATUS_GOOD)
	return 0;
    }

  if (((SANE_Word) option >= s->opt.num_options) || (option < 0))
    return 0;
  return s->opt.desc[option];
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *value, SANE_Word * info)
{
  Net_Scanner *s = handle;
  SANE_Control_Option_Req req;
  SANE_Control_Option_Reply reply;
  SANE_Status status;
  size_t value_size;
  int need_auth;

  DBG(3, "sane_control_option: option %d, action %d\n", option, action);

  if (!s->options_valid)
    {
      status = fetch_options (s);
      if (status != SANE_STATUS_GOOD)
	return status;
    }
  if (((SANE_Word) option >= s->opt.num_options) || (option < 0))
    return SANE_STATUS_INVAL;

  switch (s->opt.desc[option]->type)
    {
    case SANE_TYPE_BUTTON:
    case SANE_TYPE_GROUP:	/* shouldn't happen... */
      /* the SANE standard defines that the option size of a BUTTON or
         GROUP is IGNORED.  */
      value_size = 0;
      break;
    case SANE_TYPE_STRING: /* strings can be smaller than size */
      value_size = s->opt.desc[option]->size;
      if ((action == SANE_ACTION_SET_VALUE) 
	  && (((SANE_Int) strlen ((SANE_String) value) + 1)
	      < s->opt.desc[option]->size))
	value_size = strlen ((SANE_String) value) + 1;
      break;
    default:
      value_size = s->opt.desc[option]->size;
      break;
    }

  req.handle = s->handle;
  req.option = option;
  req.action = action;
  req.value_type = s->opt.desc[option]->type;
  req.value_size = value_size;
  req.value = value;

  sanei_w_call (&s->hw->wire, SANE_NET_CONTROL_OPTION,
		(WireCodecFunc) sanei_w_control_option_req, &req,
		(WireCodecFunc) sanei_w_control_option_reply, &reply);

  do
    {
      status = reply.status;
      need_auth = (reply.resource_to_authorize != 0);
      if (need_auth)
	{
	  do_authorization (s->hw, reply.resource_to_authorize);
	  sanei_w_free (&s->hw->wire,
			(WireCodecFunc) sanei_w_control_option_reply, &reply);

	  sanei_w_set_dir (&s->hw->wire, WIRE_DECODE);

	  sanei_w_control_option_reply (&s->hw->wire, &reply);
	  continue;

	}
      else if (status == SANE_STATUS_GOOD)
	{
	  if (info)
	    *info = reply.info;
	  if (value_size > 0)
	    {
	      if ((SANE_Word) value_size == reply.value_size)
		memcpy (value, reply.value, reply.value_size);
	      else
		DBG (1, "sane_control_option: size changed from %d to %d\n",
		     s->opt.desc[option]->size, reply.value_size);
	    }

	  if (reply.info & SANE_INFO_RELOAD_OPTIONS)
	    s->options_valid = 0;
	}
      sanei_w_free (&s->hw->wire,
		    (WireCodecFunc) sanei_w_control_option_reply, &reply);
      if (need_auth && !s->hw->auth_active)
	return SANE_STATUS_CANCELLED;
    }
  while (need_auth);
  return status;
}

SANE_Status sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  Net_Scanner *s = handle;
  SANE_Get_Parameters_Reply reply;
  SANE_Status status;

  DBG(3, "sane_get_parameters\n");

  if (!params)
    return SANE_STATUS_INVAL;

  sanei_w_call (&s->hw->wire, SANE_NET_GET_PARAMETERS,
		(WireCodecFunc) sanei_w_word, &s->handle,
		(WireCodecFunc) sanei_w_get_parameters_reply, &reply);

  status = reply.status;
  *params = reply.params;
  sanei_w_free (&s->hw->wire,
		(WireCodecFunc) sanei_w_get_parameters_reply, &reply);

  return status;
}

SANE_Status sane_start (SANE_Handle handle)
{
  Net_Scanner *s = handle;
  SANE_Start_Reply reply;
  struct sockaddr_in sin;
  SANE_Status status;
  int fd, need_auth;
  socklen_t len;
  short port;			/* Internet-specific */

  DBG(3, "sane_start\n");

  if (s->data >= 0)
    return SANE_STATUS_INVAL;

  /* Do this ahead of time so in case anything fails, we can
     recover gracefully (without hanging our server).  */
  len = sizeof (sin);
  if (getpeername (s->hw->ctl, (struct sockaddr *) &sin, &len) < 0)
    {
      DBG (1, "sane_start: getpeername() failed (%s)\n", strerror (errno));
      return SANE_STATUS_IO_ERROR;
    }

  fd = socket (s->hw->addr.sa_family, SOCK_STREAM, 0);
  if (fd < 0)
    {
      DBG (1, "sane_start: socket() failed (%s)\n", strerror (errno));
      return SANE_STATUS_IO_ERROR;
    }

  sanei_w_call (&s->hw->wire, SANE_NET_START,
		(WireCodecFunc) sanei_w_word, &s->handle,
		(WireCodecFunc) sanei_w_start_reply, &reply);
  do
    {

      status = reply.status;
      port = reply.port;
      need_auth = (reply.resource_to_authorize != 0);
      if (need_auth)
	{
	  do_authorization (s->hw, reply.resource_to_authorize);

	  sanei_w_free (&s->hw->wire,
			(WireCodecFunc) sanei_w_start_reply, &reply);

	  sanei_w_set_dir (&s->hw->wire, WIRE_DECODE);

	  sanei_w_start_reply (&s->hw->wire, &reply);

	  continue;
	}
      sanei_w_free (&s->hw->wire, (WireCodecFunc) sanei_w_start_reply,
		    &reply);
      if (need_auth && !s->hw->auth_active)
	return SANE_STATUS_CANCELLED;

      if (status != SANE_STATUS_GOOD)
	{
	  close (fd);
	  return status;
	}
    }
  while (need_auth);

  sin.sin_port = htons (port);

  if (connect (fd, (struct sockaddr *) &sin, len) < 0)
    {
      DBG (1, "sane_start: connect() failed (%s)\n", strerror (errno));
      close (fd);
      return SANE_STATUS_IO_ERROR;
    }
  shutdown (fd, 1);
  s->data = fd;
  s->reclen_buf_offset = 0;
  s->bytes_remaining = 0;
  return status;
}

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * data, SANE_Int max_length,
	   SANE_Int * length)
{
  Net_Scanner *s = handle;
  ssize_t nread;

  DBG(3, "sane_read: max_length = %d\n", max_length);

  if (s->data < 0)
    return SANE_STATUS_CANCELLED;

  *length = 0;

  if (s->data < 0)
    return SANE_STATUS_INVAL;

  if (s->bytes_remaining == 0)
    {
      /* boy, is this painful or what? */

      nread = read (s->data, s->reclen_buf + s->reclen_buf_offset,
		    4 - s->reclen_buf_offset);
      if (nread < 0)
	{
	  if (errno == EAGAIN)
	    return SANE_STATUS_GOOD;
	  else
	    {
	      do_cancel (s);
	      return SANE_STATUS_IO_ERROR;
	    }
	}
      s->reclen_buf_offset += nread;
      if (s->reclen_buf_offset < 4)
	return SANE_STATUS_GOOD;

      s->reclen_buf_offset = 0;
      s->bytes_remaining = (((u_long) s->reclen_buf[0] << 24)
			    | ((u_long) s->reclen_buf[1] << 16)
			    | ((u_long) s->reclen_buf[2] << 8)
			    | ((u_long) s->reclen_buf[3] << 0));
      DBG (3, "sane_read: next record length=%ld bytes\n",
	   (long) s->bytes_remaining);
      if (s->bytes_remaining == (size_t) - 1)
	{
	  char ch;

	  /* turn off non-blocking I/O (s->data will be closed anyhow): */
	  fcntl (s->data, F_SETFL, 0);

	  /* read the status byte: */
	  if (read (s->data, &ch, sizeof (ch)) != 1)
	    ch = SANE_STATUS_IO_ERROR;
	  do_cancel (s);
	  return (SANE_Status) ch;
	}
    }

  if (max_length > (SANE_Int) s->bytes_remaining)
    max_length = s->bytes_remaining;
#if 0
  /* Make sure to get only complete pixel */
  if (0 != max_length % bytes_per_pixel)
    max_length = (max_length / bytes_per_pixel) * bytes_per_pixel;
#endif
  /* XXX How do we handle endian problems */
  nread = read (s->data, data, max_length);
  if (nread < 0)
    {
      if (errno == EAGAIN)
	return SANE_STATUS_GOOD;
      else
	{
	  do_cancel (s);
	  return SANE_STATUS_IO_ERROR;
	}
    }
  s->bytes_remaining -= nread;
  *length = nread;
  return SANE_STATUS_GOOD;
}

void
sane_cancel (SANE_Handle handle)
{
  Net_Scanner *s = handle;
  SANE_Word ack;

  DBG(3, "sane_cancel\n");

  sanei_w_call (&s->hw->wire, SANE_NET_CANCEL,
		(WireCodecFunc) sanei_w_word, &s->handle,
		(WireCodecFunc) sanei_w_word, &ack);
  do_cancel (s);
}

SANE_Status sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  Net_Scanner *s = handle;

  DBG(3, "sane_set_io_mode: non_blocking = %d\n", non_blocking);
  if (s->data < 0)
    return SANE_STATUS_INVAL;

  if (fcntl (s->data, F_SETFL, non_blocking ? O_NONBLOCK : 0) < 0)
    return SANE_STATUS_IO_ERROR;

  return SANE_STATUS_GOOD;
}

SANE_Status sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  Net_Scanner *s = handle;

  DBG(3, "sane_get_select_fd: *fd = %d\n", *fd);

  if (s->data < 0)
    return SANE_STATUS_INVAL;

  *fd = s->data;
  return SANE_STATUS_GOOD;
}
