/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 Andreas Beck
   Copyright (C) 2001 - 2004 Henning Meier-Geinitz
   Copyright (C) 2003 Julien BLACHE <jb@jblache.org>
       AF-independent + IPv6 code

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
# include "../include/lalloca.h"		/* MUST come first for AIX! */
#endif

#include "../include/sane/config.h"
#include "../include/lalloca.h"
#include <sys/types.h>

#if defined(HAVE_GETADDRINFO) && defined (HAVE_GETNAMEINFO)
# define SANED_USES_AF_INDEP
# ifdef HAS_SS_FAMILY
#  define SS_FAMILY(ss) ss.ss_family
# elif defined(HAS___SS_FAMILY)
#  define SS_FAMILY(ss) ss.__ss_family
# else /* fallback to the old, IPv4-only code */
#  undef SANED_USES_AF_INDEP
#  undef ENABLE_IPV6
# endif
#else
# undef ENABLE_IPV6
#endif /* HAVE_GETADDRINFO && HAVE_GETNAMEINFO && HAVE_POLL */

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
#include <arpa/inet.h>

#ifdef SANED_USES_AF_INDEP
# if defined(HAVE_SYS_POLL_H) && defined(HAVE_POLL)
#  include <sys/poll.h>
# else
/* 
 * This replacement poll() using select() is only designed to cover
 * our needs in main(). It should probably be extended...
 */
struct pollfd
{
  int fd;
  short events;
  short revents;
};

#define POLLIN 0x0001

int
poll (struct pollfd *ufds, unsigned int nfds, int timeout)
{
  struct pollfd *fdp;

  fd_set fds;
  int maxfd = 0;
  unsigned int i;
  int ret;

  /* unused */
  timeout = timeout;

  FD_ZERO (&fds);

  for (i = 0, fdp = ufds; i < nfds; i++, fdp++)
    {
      if (fdp->events & POLLIN)
	{
	  FD_SET (fdp->fd, &fds);
	  maxfd = (fdp->fd > maxfd) ? fdp->fd : maxfd;
	}
    }
  
  maxfd++;

  ret = select (maxfd, &fds, NULL, NULL, NULL);

  if (ret < 0)
    return ret;

  for (i = 0, fdp = ufds; i < nfds; i++, fdp++)
    {
      if (fdp->events & POLLIN)
	if (FD_ISSET (fdp->fd, &fds))
	  fdp->revents = POLLIN;
    }

  return ret;
}
# endif /* HAVE_SYS_POLL_H && HAVE_POLL */
#endif /* SANED_USES_AF_INDEP */

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/sanei_net.h"
#include "../include/sane/sanei_codec_bin.h"
#include "../include/sane/sanei_config.h"

#include "../include/sane/sanei_auth.h"

#ifndef EXIT_SUCCESS
# define EXIT_SUCCESS   0
#endif

#ifndef IN_LOOPBACK
# define IN_LOOPBACK(addr) (addr == 0x7f000001L)
#endif

#ifdef ENABLE_IPV6
# define SANE_IN6_IS_ADDR_LOOPBACK(a) \
        (((const u_int32_t *) (a))[0] == 0                                   \
         && ((const u_int32_t *) (a))[1] == 0                                \
         && ((const u_int32_t *) (a))[2] == 0                                \
         && ((const u_int32_t *) (a))[3] == htonl (1)) 

#define SANE_IN6_IS_ADDR_V4MAPPED(a) \
((((const u_int32_t *) (a))[0] == 0)                                 \
 && (((const u_int32_t *) (a))[1] == 0)                              \
 && (((const u_int32_t *) (a))[2] == htonl (0xffff))) 
#endif /* ENABLE_IPV6 */

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
static char *remote_ip;

#ifdef SANED_USES_AF_INDEP
static struct sockaddr_storage remote_address;
static int remote_address_len;
#else
static struct in_addr remote_address;
#endif /* SANED_USES_AF_INDEP */

#ifndef _PATH_HEQUIV
# define _PATH_HEQUIV   "/etc/hosts.equiv"
#endif

static const char *config_file_names[] = {
  _PATH_HEQUIV, SANED_CONFIG_FILE
};

static SANE_Bool log_to_syslog = SANE_TRUE;

/* forward declarations: */
static void process_request (Wire * w);

#define DBG_ERR  1
#define DBG_WARN 2
#define DBG_MSG  3
#define DBG_INFO 4
#define DBG_DBG  5

#define DBG	saned_debug_call

static void
saned_debug_call (int level, const char *fmt, ...)
{
#ifndef NDEBUG
  va_list ap;
  va_start (ap, fmt);
  if (debug >= level)
    {
      if (log_to_syslog)
	{
	  /* print to syslog */
	  vsyslog (LOG_DEBUG, fmt, ap);
	}
      else
	{
	  /* print to stderr */
	  fprintf (stderr, "[saned] ");
	  vfprintf (stderr, fmt, ap);
	}
    }
  va_end (ap);
#endif
}


static void
reset_watchdog (void)
{
  if (!debug)
    alarm (3600);
}

static void
auth_callback (SANE_String_Const res,
	       SANE_Char *username,
	       SANE_Char *password)
{
  SANE_Net_Procedure_Number procnum;
  SANE_Authorization_Req req;
  SANE_Word word, ack = 0;

  memset (username, 0, SANE_MAX_USERNAME_LEN);
  memset (password, 0, SANE_MAX_PASSWORD_LEN);

  if (!can_authorize)
    {
      DBG (DBG_WARN,
	   "auth_callback: called during non-authorizable RPC (resource=%s)\n",
	   res);
      return;
    }

  if (wire.status)
    {
      DBG(DBG_ERR, "auth_callback: bad status %d\n", wire.status);
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
      DBG (DBG_WARN, 
	   "auth_callback: called for unexpected request %d (resource=%s)\n",
	   current_request, res);
      break;
    }

  if (wire.status)
    {
      DBG(DBG_ERR, "auth_callback: bad status %d\n", wire.status);
      return;
    }

  reset_watchdog ();

  sanei_w_set_dir (&wire, WIRE_DECODE);
  sanei_w_word (&wire, &word);

  if (wire.status)
    {
      DBG(DBG_ERR, "auth_callback: bad status %d\n", wire.status);
      return;
    }

  procnum = word;
  if (procnum != SANE_NET_AUTHORIZE)
    {
      DBG (DBG_WARN,
	   "auth_callback: bad procedure number %d "
	   "(expected: %d, resource=%s)\n", procnum, SANE_NET_AUTHORIZE, 
	   res);
      return;
    }

  sanei_w_authorization_req (&wire, &req);
  if (wire.status)
    {
      DBG(DBG_ERR, "auth_callback: bad status %d\n", wire.status);
      return;
    }

  if (req.username)
    strcpy (username, req.username);
  if (req.password)
    strcpy (password, req.password);
  if (!req.resource || strcmp (req.resource, res) != 0)
    {
      DBG (DBG_MSG,
	   "auth_callback: got auth for resource %s (expected resource=%s)\n",
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
    DBG (DBG_ERR, "quit: received signal %d\n", signum);

  if (running)
    {
      DBG (DBG_ERR, "quit: already active, returning\n");
      return;
    }
  running = 1;

  for (i = 0; i < num_handles; ++i)
    if (handle[i].inuse)
      sane_close (handle[i].handle);

  sane_exit ();
  sanei_w_exit (&wire);
  if (handle)
    free (handle);
  DBG (DBG_WARN, "quit: exiting\n");
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
  if (h >= 0 && handle[h].inuse)
    {
      sane_close (handle[h].handle);
      handle[h].inuse = 0;
    }
}

static SANE_Word
decode_handle (Wire * w, const char *op)
{
  SANE_Word h;

  sanei_w_word (w, &h);
  if (w->status || (unsigned) h >= (unsigned) num_handles || !handle[h].inuse)
    {
      DBG (DBG_ERR,
	   "decode_handle: %s: error while decoding handle argument "
	   "(h=%d, %s)\n", op, h, strerror (w->status));
      return -1;
    }
  return h;
}



/* Convert a number of bits to an 8-bit bitmask */
static unsigned int cidrtomask[9] = { 0x00, 0x80, 0xC0, 0xE0, 0xF0,
				      0xF8, 0xFC, 0xFE, 0xFF };

#ifdef SANED_USES_AF_INDEP
static SANE_Bool
check_v4_in_range (struct sockaddr_in *sin, char *base_ip, char *netmask)
{
  int cidr;
  int i, err;
  char *end;
  u_int32_t mask; 
  struct sockaddr_in *base;
  struct addrinfo hints;
  struct addrinfo *res;
  SANE_Bool ret = SANE_FALSE;

  cidr = -1;
  cidr = strtol (netmask, &end, 10);
  
  /* Sanity check on the cidr value */
  if ((cidr < 0) || (cidr > 32) || (end == netmask))
    {
      DBG (DBG_ERR, "check_v4_in_range: invalid CIDR value (%s) !\n", netmask);
      return SANE_FALSE;
    }

  mask = 0;
  cidr -= 8;

  /* Build a bitmask out of the CIDR value */  
  for (i = 3; cidr >= 0; i--)
    {
      mask |= (0xff << (8 * i));
      cidr -= 8;
    }
  
  if (cidr < 0)
    mask |= (cidrtomask[cidr + 8] << (8 * i));

  mask = htonl (mask);

  /* get a sockaddr_in representing the base IP address */
  memset (&hints, 0, sizeof (struct addrinfo));
  hints.ai_flags = AI_NUMERICHOST;
  hints.ai_family = PF_INET;
  
  err = getaddrinfo (base_ip, NULL, &hints, &res);
  if (err)
    {
      DBG (DBG_DBG, "check_v4_in_range: getaddrinfo() failed: %s\n", gai_strerror (err));
      return SANE_FALSE;
    }

  base = (struct sockaddr_in *) res->ai_addr;

  /*
   * Check that the address belongs to the specified subnet, using the bitmask.
   * The address is represented by a 32bit integer.
   */
  if ((base->sin_addr.s_addr & mask) == (sin->sin_addr.s_addr & mask))
    ret = SANE_TRUE;
  
  freeaddrinfo (res);
  
  return ret;
}


# ifdef ENABLE_IPV6

static SANE_Bool
check_v6_in_range (struct sockaddr_in6 *sin6, char *base_ip, char *netmask)
{
  int cidr;
  int i, err;
  unsigned int mask[16];
  char *end;
  struct sockaddr_in6 *base;
  struct addrinfo hints;
  struct addrinfo *res;
  SANE_Bool ret = SANE_TRUE;

  cidr = -1;
  cidr = strtol (netmask, &end, 10);

  /* Sanity check on the cidr value */
  if ((cidr < 0) || (cidr > 128) || (end == netmask))
    {
      DBG (DBG_ERR, "check_v6_in_range: invalid CIDR value (%s) !\n", netmask);
      return SANE_FALSE;
    }

  memset (mask, 0, (16 * sizeof (unsigned int)));
  cidr -= 8;
  
  /* Build a bitmask out of the CIDR value */
  for (i = 0; cidr >= 0; i++)
    {
      mask[i] = 0xff;
      cidr -= 8;
    }
  
  if (cidr < 0)
    mask[i] = cidrtomask[cidr + 8];
  
  /* get a sockaddr_in6 representing the base IP address */
  memset (&hints, 0, sizeof (struct addrinfo));
  hints.ai_flags = AI_NUMERICHOST;
  hints.ai_family = PF_INET6;

  err = getaddrinfo (base_ip, NULL, &hints, &res);
  if (err)
    {
      DBG (DBG_DBG, "check_v6_in_range: getaddrinfo() failed: %s\n", gai_strerror (err));
      return SANE_FALSE;
    }

  base = (struct sockaddr_in6 *) res->ai_addr;

  /*
   * Check that the address belongs to the specified subnet.
   * The address is reprensented by an array of 16 8bit integers.
   */
  for (i = 0; i < 16; i++)
    {
      if ((base->sin6_addr.s6_addr[i] & mask[i]) != (sin6->sin6_addr.s6_addr[i] & mask[i]))
	{
	  ret = SANE_FALSE;
	  break;
	}
    }
  
  freeaddrinfo (res);
  
  return ret;
}
# endif /* ENABLE_IPV6 */
#else /* !SANED_USES_AF_INDEP */
static SANE_Bool
check_v4_in_range (struct in_addr *inaddr, struct in_addr *base, char *netmask)
{
  int cidr;
  int i;
  char *end;
  u_int32_t mask; 
  SANE_Bool ret = SANE_FALSE;

  cidr = -1;
  cidr = strtol (netmask, &end, 10);
  
  /* sanity check on the cidr value */
  if ((cidr < 0) || (cidr > 32) || (end == netmask))
    {
      DBG (DBG_ERR, "check_v4_in_range: invalid CIDR value (%s) !\n", netmask);
      return SANE_FALSE;
    }

  mask = 0;
  cidr -= 8;
  
  /* Build a bitmask out of the CIDR value */
  for (i = 3; cidr >= 0; i--)
    {
      mask |= (0xff << (8 * i));
      cidr -= 8;
    }
  
  if (cidr < 0)
    mask |= (cidrtomask[cidr + 8] << (8 * i));

  mask = htonl (mask);

  /*
   * Check that the address belongs to the specified subnet, using the bitmask.
   * The address is represented by a 32bit integer.
   */
  if ((base->s_addr & mask) == (inaddr->s_addr & mask))
    ret = SANE_TRUE;
  
  return ret;
}
#endif /* SANED_USES_AF_INDEP */



/* Access control */
#ifdef SANED_USES_AF_INDEP
static SANE_Status
check_host (int fd)
{
  struct sockaddr_in *sin = NULL;
#ifdef ENABLE_IPV6
  struct sockaddr_in6 *sin6;
#endif /* ENABLE_IPV6 */
  struct addrinfo hints;
  struct addrinfo *res;
  struct addrinfo *resp;
  int j, access_ok = 0;
  int err;
  char text_addr[64];
#ifdef ENABLE_IPV6
  SANE_Bool IPv4map = SANE_FALSE;
  char *remote_ipv4 = NULL; /* in case we have an IPv4-mapped address (eg ::ffff:127.0.0.1) */
  char *tmp;
  struct addrinfo *remote_ipv4_addr = NULL;
#endif /* ENABLE_IPV6 */
  char config_line_buf[1024];
  char *config_line;
  char *netmask;
  char hostname[MAXHOSTNAMELEN];

  int len;
  FILE *fp;

  /* Get address of remote host */
  remote_address_len = sizeof (remote_address);
  if (getpeername (fd, (struct sockaddr *) &remote_address, (socklen_t *) &remote_address_len) < 0)
    {
      DBG (DBG_ERR, "check_host: getpeername failed: %s\n", strerror (errno));
      remote_ip = strdup ("[error]");
      return SANE_STATUS_INVAL;
    }

  err = getnameinfo ((struct sockaddr *) &remote_address, remote_address_len,
		     hostname, sizeof (hostname), NULL, 0, NI_NUMERICHOST);
  if (err)
    {
      DBG (DBG_DBG, "check_host: getnameinfo failed: %s\n", gai_strerror(err));
      remote_ip = strdup ("[error]");
      return SANE_STATUS_INVAL;
    }
  else
    remote_ip = strdup (hostname);

#ifdef ENABLE_IPV6
  sin6 = (struct sockaddr_in6 *) &remote_address;

  if (SANE_IN6_IS_ADDR_V4MAPPED (sin6->sin6_addr.s6_addr))
    {
      DBG (DBG_DBG, "check_host: detected an IPv4-mapped address\n");
      remote_ipv4 = remote_ip + 7;
      IPv4map = SANE_TRUE;

      memset (&hints, 0, sizeof (struct addrinfo));
      hints.ai_flags = AI_NUMERICHOST;
      hints.ai_family = PF_INET;
      
      err = getaddrinfo (remote_ipv4, NULL, &hints, &res);
      if (err)
	{
	  DBG (DBG_DBG, "check_host: getaddrinfo() failed: %s\n", gai_strerror (err));
	  IPv4map = SANE_FALSE; /* we failed, remote_ipv4_addr points to nothing */
	}
      else
	{
	  remote_ipv4_addr = res;
	  sin = (struct sockaddr_in *)res->ai_addr;
	}
    }
#endif /* ENABLE_IPV6 */

  DBG (DBG_WARN, "check_host: access by remote host: %s\n", remote_ip);

  /* Always allow access from local host. Do it here to avoid DNS lookups
     and reading saned.conf. */

#ifdef ENABLE_IPV6
  if (IPv4map == SANE_TRUE)
    {
      if (IN_LOOPBACK (ntohl (sin->sin_addr.s_addr)))
	{
	  DBG (DBG_MSG,
	       "check_host: remote host is IN_LOOPBACK: access granted\n");
	  freeaddrinfo (remote_ipv4_addr);
	  return SANE_STATUS_GOOD;
	}
      freeaddrinfo (remote_ipv4_addr);
    }
#endif /* ENABLE_IPV6 */

  sin = (struct sockaddr_in *) &remote_address;

  switch (SS_FAMILY(remote_address))
    {
      case AF_INET:
	if (IN_LOOPBACK (ntohl (sin->sin_addr.s_addr)))
	  {
	    DBG (DBG_MSG,
		 "check_host: remote host is IN_LOOPBACK: access granted\n");
	    return SANE_STATUS_GOOD;
	  }
	break;
#ifdef ENABLE_IPV6
      case AF_INET6:
	if (SANE_IN6_IS_ADDR_LOOPBACK (sin6->sin6_addr.s6_addr))
	  {
	    DBG (DBG_MSG,
		 "check_host: remote host is IN6_LOOPBACK: access granted\n");
	    return SANE_STATUS_GOOD;
	  }
	break;
#endif /* ENABLE_IPV6 */
      default:
	break;
    }

  DBG (DBG_DBG, "check_host: remote host is not IN_LOOPBACK"
#ifdef ENABLE_IPV6
       " nor IN6_LOOPBACK"
#endif /* ENABLE_IPV6 */
       "\n");


  /* Get name of local host */
  if (gethostname (hostname, sizeof (hostname)) < 0)
    {
      DBG (DBG_ERR, "check_host: gethostname failed: %s\n", strerror (errno));
      return SANE_STATUS_INVAL;
    }
  DBG (DBG_DBG, "check_host: local hostname: %s\n", hostname);

  /* Get local addresses */
  memset (&hints, 0, sizeof (hints));
  hints.ai_flags = AI_CANONNAME;
#ifdef ENABLE_IPV6
  hints.ai_family = PF_UNSPEC;
#else
  hints.ai_family = PF_INET;
#endif /* ENABLE_IPV6 */

  err = getaddrinfo (hostname, NULL, &hints, &res);
  if (err)
    {
      DBG (DBG_ERR, "check_host: getaddrinfo failed: %s\n",
	   gai_strerror (err));
      return SANE_STATUS_INVAL;
    }
  else
    {
      for (resp = res; resp != NULL; resp = resp->ai_next)
	{
	  DBG (DBG_DBG, "check_host: local hostname(s) (from DNS): %s\n",
	       resp->ai_canonname);
	  
	  err = getnameinfo (resp->ai_addr, resp->ai_addrlen, text_addr,
			     sizeof (text_addr), NULL, 0, NI_NUMERICHOST);
	  if (err)
		strncpy (text_addr, "[error]", 8);

#ifdef ENABLE_IPV6	  
	  if ((strcasecmp (text_addr, remote_ip) == 0) ||
	      ((IPv4map == SANE_TRUE) && (strcmp (text_addr, remote_ipv4) == 0)))
#else
	  if (strcmp (text_addr, remote_ip) == 0)
#endif /* ENABLE_IPV6 */
	    {
	      DBG (DBG_MSG, "check_host: remote host has same addr as local: access granted\n");
	      
	      freeaddrinfo (res);
	      res = NULL;

	      return SANE_STATUS_GOOD;
	    }
	}
      
      freeaddrinfo (res);
      res = NULL;
      
      DBG (DBG_DBG, 
	   "check_host: remote host doesn't have same addr as local\n");
    }

  /* must be a remote host: check contents of PATH_NET_CONFIG or
     /etc/hosts.equiv if former doesn't exist: */
  for (j = 0; j < NELEMS (config_file_names); ++j)
    {
      DBG (DBG_DBG, "check_host: opening config file: %s\n",
	   config_file_names[j]);
      if (config_file_names[j][0] == '/')
	fp = fopen (config_file_names[j], "r");
      else
	fp = sanei_config_open (config_file_names[j]);
      if (!fp)
	{
	  DBG (DBG_MSG,
	       "check_host: can't open config file: %s (%s)\n",
	       config_file_names[j], strerror (errno));
	  continue;
	}
      
      while (!access_ok && sanei_config_read (config_line_buf, 
					      sizeof (config_line_buf), fp))
	{
	  config_line = config_line_buf; /* from now on, use a pointer */
	  DBG (DBG_DBG, "check_host: config file line: `%s'\n", config_line);
	  if (config_line[0] == '#')	/* ignore line comments */
	    continue;
	  len = strlen (config_line);
	  
	  if (!len)
	    continue;		/* ignore empty lines */

	  /* look for a subnet specification */
	  netmask = strchr (config_line, '/');
	  if (netmask != NULL)
	    {
	      *netmask = '\0';
	      netmask++;
	      DBG (DBG_DBG, "check_host: subnet with base IP = %s, CIDR netmask = %s\n",
		   config_line, netmask);
	    }

#ifdef ENABLE_IPV6
	  /* IPv6 addresses are enclosed in [] */
	  if (*config_line == '[')
	    {
	      config_line++;
	      tmp = strchr (config_line, ']');
	      if (tmp == NULL)
		{
		  DBG (DBG_ERR,
		       "check_host: malformed IPv6 address in config file, skipping: [%s\n",
		       config_line);
		  continue;
		}
	      *tmp = '\0';
	    }
#endif /* ENABLE_IPV6 */

	  if (strcmp (config_line, "+") == 0)
	    {
	      access_ok = 1;
	      DBG (DBG_DBG, 
		   "check_host: access granted from any host (`+')\n");
	    }
	  /* compare remote_ip (remote IP address) to the config_line */
	  else if (strcasecmp (config_line, remote_ip) == 0)
	    {
	      access_ok = 1;
	      DBG (DBG_DBG,
		   "check_host: access granted from IP address %s\n", remote_ip);
	    }
#ifdef ENABLE_IPV6
	  else if ((IPv4map == SANE_TRUE) && (strcmp (config_line, remote_ipv4) == 0))
	    {
	      access_ok = 1;
	      DBG (DBG_DBG,
		   "check_host: access granted from IP address %s (IPv4-mapped)\n", remote_ip);
	    }
	  /* handle IP ranges, take care of the IPv4map stuff */
	  else if (netmask != NULL)
	    {
	      if (strchr (config_line, ':') != NULL) /* is a v6 address */
		{
		  if (SS_FAMILY(remote_address) == AF_INET6)
		    {
		      if (check_v6_in_range (sin6, config_line, netmask))
			{
			  access_ok = 1;
			  DBG (DBG_DBG, "check_host: access granted from IP address %s (in subnet [%s]/%s)\n",
			       remote_ip, config_line, netmask);
			}
		    }
		}
	      else /* is a v4 address */
		{
		  if (IPv4map == SANE_TRUE)
		    {
		      /* get a sockaddr_in representing the v4-mapped IP address */
		      memset (&hints, 0, sizeof (struct addrinfo));
		      hints.ai_flags = AI_NUMERICHOST;
		      hints.ai_family = PF_INET;
		      
		      err = getaddrinfo (remote_ipv4, NULL, &hints, &res);
		      if (err)
			DBG (DBG_DBG, "check_host: getaddrinfo() failed: %s\n", gai_strerror (err));
		      else
			sin = (struct sockaddr_in *)res->ai_addr;
		    }

		  if ((SS_FAMILY(remote_address) == AF_INET) ||
		      (IPv4map == SANE_TRUE))
		    {
		      
		      if (check_v4_in_range (sin, config_line, netmask))
			{
			  DBG (DBG_DBG, "check_host: access granted from IP address %s (in subnet %s/%s)\n",
			       ((IPv4map == SANE_TRUE) ? remote_ipv4 : remote_ip), config_line, netmask);
			  access_ok = 1;
			}
		      else
			{
			  /* restore the old sin pointer */
			  sin = (struct sockaddr_in *)&remote_address;
			}
		      
		      if (res != NULL)
			{
			  freeaddrinfo (res);
			  res = NULL;
			}
		    }
		}
	    }
#else /* !ENABLE_IPV6 */
	  /* handle IP ranges */
	  else if (netmask != NULL)
	    {
	      if (check_v4_in_range (sin, config_line, netmask))
		{
		  access_ok = 1;
		  DBG (DBG_DBG, "check_host: access granted from IP address %s (in subnet %s/%s)\n",
		       remote_ip, config_line, netmask);
		}
	    }
#endif /* ENABLE_IPV6 */
	  else
	    {
	      memset (&hints, 0, sizeof (hints));
	      hints.ai_flags = AI_CANONNAME;
#ifdef ENABLE_IPV6
	      hints.ai_family = PF_UNSPEC;
#else
	      hints.ai_family = PF_INET;
#endif /* ENABLE_IPV6 */
	      
	      err = getaddrinfo (config_line, NULL, &hints, &res);
	      if (err)
		{
		  DBG (DBG_DBG, 
		       "check_host: getaddrinfo for `%s' failed: %s\n",
		       config_line, gai_strerror (err));
		  DBG (DBG_MSG, "check_host: entry isn't an IP address "
		       "and can't be found in DNS\n");
		  continue;
		}
	      else
		{
		  for (resp = res; resp != NULL; resp = resp->ai_next)
		    {
		      err = getnameinfo (resp->ai_addr, resp->ai_addrlen, text_addr,
					 sizeof (text_addr), NULL, 0, NI_NUMERICHOST);
		      if (err)
			strncpy (text_addr, "[error]", 8);
		      
		      DBG (DBG_MSG,  
			   "check_host: DNS lookup returns IP address: %s\n", 
			   text_addr); 
		      
#ifdef ENABLE_IPV6			  
		      if ((strcasecmp (text_addr, remote_ip) == 0) ||
			  ((IPv4map == SANE_TRUE) && (strcmp (text_addr, remote_ipv4) == 0)))
#else
		      if (strcmp (text_addr, remote_ip) == 0)
#endif /* ENABLE_IPV6 */
			access_ok = 1;
		      
		      if (access_ok)
			break;
		    }
		  freeaddrinfo (res);
		  res = NULL;
		}
	    }
	}
      fclose (fp);
    }
  
  if (access_ok)
    return SANE_STATUS_GOOD;
  
  return SANE_STATUS_ACCESS_DENIED;
}

#else /* !SANED_USES_AF_INDEP */

static SANE_Status
check_host (int fd)
{
  struct sockaddr_in sin;
  int j, access_ok = 0;
  struct hostent *he;
  char text_addr[64];
  char config_line_buf[1024];
  char *config_line;
  char *netmask;
  char hostname[MAXHOSTNAMELEN];
  char *r_hostname;
  static struct in_addr config_line_address;
  
  int len;
  FILE *fp;

  /* Get address of remote host */
  len = sizeof (sin);
  if (getpeername (fd, (struct sockaddr *) &sin, (socklen_t *) &len) < 0)
    {
      DBG (DBG_ERR, "check_host: getpeername failed: %s\n", strerror (errno));
      remote_ip = strdup ("[error]");
      return SANE_STATUS_INVAL;
    }
  r_hostname = inet_ntoa (sin.sin_addr);
  remote_ip = strdup (r_hostname);
  DBG (DBG_WARN, "check_host: access by remote host: %s\n", 
       remote_ip);
  /* Save remote address for check of control and data connections */
  memcpy (&remote_address, &sin.sin_addr, sizeof (remote_address));

  /* Always allow access from local host. Do it here to avoid DNS lookups
     and reading saned.conf. */
  if (IN_LOOPBACK (ntohl (sin.sin_addr.s_addr)))
    {
      DBG (DBG_MSG,
	   "check_host: remote host is IN_LOOPBACK: access accepted\n");
      return SANE_STATUS_GOOD;
    }
  DBG (DBG_DBG, "check_host: remote host is not IN_LOOPBACK\n");

  /* Get name of local host */
  if (gethostname (hostname, sizeof (hostname)) < 0)
    {
      DBG (DBG_ERR, "check_host: gethostname failed: %s\n", strerror (errno));
      return SANE_STATUS_INVAL;
    }
  DBG (DBG_DBG, "check_host: local hostname: %s\n", hostname);

  /* Get local address */
  he = gethostbyname (hostname);
  if (!he)
    {
      DBG (DBG_ERR, "check_host: gethostbyname failed: %s\n",
	   strerror (errno));
      return SANE_STATUS_INVAL;
    }
  DBG (DBG_DBG, "check_host: local hostname (from DNS): %s\n",
       he->h_name);
  
  if ((he->h_length == 4) || he->h_addrtype == AF_INET)
    {
      if (!inet_ntop (he->h_addrtype, he->h_addr_list[0], text_addr,
		      sizeof (text_addr)))
	strcpy (text_addr, "[error]");
      DBG (DBG_DBG, "check_host: local host address (from DNS): %s\n",
	   text_addr);
      if (memcmp (he->h_addr_list[0], &remote_address.s_addr, 4) == 0)   
	{
	  DBG (DBG_MSG, 
	       "check_host: remote host has same addr as local: "
	       "access accepted\n");
	  return SANE_STATUS_GOOD;
	}
    }
  else
    {
      DBG (DBG_ERR, "check_host: can't get local address "
	   "(only IPv4 is supported)\n");
    }

  DBG (DBG_DBG, 
       "check_host: remote host doesn't have same addr as local\n");

  /* must be a remote host: check contents of PATH_NET_CONFIG or
     /etc/hosts.equiv if former doesn't exist: */
  for (j = 0; j < NELEMS (config_file_names); ++j)
    {
      DBG (DBG_DBG, "check_host: opening config file: %s\n",
	   config_file_names[j]);
      if (config_file_names[j][0] == '/')
	fp = fopen (config_file_names[j], "r");
      else
	fp = sanei_config_open (config_file_names[j]);
      if (!fp)
	{
	  DBG (DBG_MSG,
	       "check_host: can't open config file: %s (%s)\n",
	       config_file_names[j], strerror (errno));
	  continue;
	}
      
      while (!access_ok && sanei_config_read (config_line_buf, 
					      sizeof (config_line_buf), fp))
	{
	  config_line = config_line_buf; /* from now on, use a pointer */
	  DBG (DBG_DBG, "check_host: config file line: `%s'\n", config_line);
	  if (config_line[0] == '#')	/* ignore line comments */
	    continue;
	  len = strlen (config_line);
	  
	  if (!len)
	    continue;		/* ignore empty lines */
	  
	  /* look for a subnet specification */
	  netmask = strchr (config_line, '/');
	  if (netmask != NULL)
	    {
	      *netmask = '\0';
	      netmask++;
	      DBG (DBG_DBG, "check_host: subnet with base IP = %s, CIDR netmask = %s\n",
		   config_line, netmask);
	    }

	  if (strcmp (config_line, "+") == 0)
	    {
	      access_ok = 1;
	      DBG (DBG_DBG, 
		   "check_host: access accepted from any host (`+')\n");
	    }
	  else
	    {
	      if (inet_pton (AF_INET, config_line, &config_line_address) > 0)
		{
		  if (memcmp (&remote_address.s_addr, 
			      &config_line_address.s_addr, 4) == 0)
		    access_ok = 1;
		  else if (netmask != NULL)
		    {
		      if (check_v4_in_range (&remote_address, &config_line_address, netmask))
			{
			  access_ok = 1;
			  DBG (DBG_DBG, "check_host: access granted from IP address %s (in subnet %s/%s)\n",
			       remote_ip, config_line, netmask);
			}
		    }
		}
	      else
		{
		  DBG (DBG_DBG, 
		       "check_host: inet_pton for `%s' failed\n",
		       config_line);
		  he = gethostbyname (config_line);
		  if (!he)
		    {
		      DBG (DBG_DBG, 
			   "check_host: gethostbyname for `%s' failed: %s\n",
			   config_line, strerror (errno));
		      DBG (DBG_MSG, "check_host: entry isn't an IP address "
			   "and can't be found in DNS\n");
		      continue;
		    }
		  if (!inet_ntop (he->h_addrtype, he->h_addr_list[0],
				  text_addr, sizeof (text_addr)))
		    strcpy (text_addr, "[error]");
		  DBG (DBG_MSG, 
		       "check_host: DNS lookup returns IP address: %s\n",
		       text_addr);
		  if (memcmp (&remote_address.s_addr, 
			      he->h_addr_list[0], 4) == 0)
		    access_ok = 1;
		}
	    }
	}
      fclose (fp);
      if (access_ok)
	return SANE_STATUS_GOOD;
    }
  return SANE_STATUS_ACCESS_DENIED;
}

#endif /* SANED_USES_AF_INDEP */

static int
init (Wire * w)
{
  SANE_Word word, be_version_code;
  SANE_Init_Reply reply;
  SANE_Status status;
  SANE_Init_Req req;

  reset_watchdog ();

  status = check_host (w->io.fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_WARN, "init: access by host %s denied\n", remote_ip);
      return -1;
    }
  else
    DBG (DBG_MSG, "init: access granted\n");

  sanei_w_set_dir (w, WIRE_DECODE);
  if (w->status)
    {
      DBG (DBG_ERR, "init: bad status after sanei_w_set_dir: %d\n", w->status);
      return -1;
    }
  
  sanei_w_word (w, &word);	/* decode procedure number */
  if (w->status || word != SANE_NET_INIT)
    {
      DBG (DBG_ERR, "init: bad status=%d or procnum=%d\n",
	   w->status, word);
      return -1;
    }

  sanei_w_init_req (w, &req);
  if (w->status)
    {
      DBG (DBG_ERR, "init: bad status after sanei_w_init_req: %d\n", w->status);
      return -1;
    }

  w->version = SANEI_NET_PROTOCOL_VERSION;
  if (req.username)
    default_username = strdup (req.username);

  sanei_w_free (w, (WireCodecFunc) sanei_w_init_req, &req);
  if (w->status)
    {
      DBG (DBG_ERR, "init: bad status after sanei_w_free: %d\n", w->status);
      return -1;
    }

  reply.version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR,
					  SANEI_NET_PROTOCOL_VERSION);

  DBG (DBG_WARN, "init: access granted to %s@%s\n",
       default_username, remote_ip);

  if (status == SANE_STATUS_GOOD)
    {
      status = sane_init (&be_version_code, auth_callback);
      if (status != SANE_STATUS_GOOD)
	DBG (DBG_ERR, "init: failed to initialize backend (%s)\n",
	     sane_strstatus (status));

      if (SANE_VERSION_MAJOR (be_version_code) != V_MAJOR)
	{
	  DBG (DBG_ERR,
	       "init: unexpected backend major version %d (expected %d)\n",
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

#ifdef SANED_USES_AF_INDEP
static int
start_scan (Wire * w, int h, SANE_Start_Reply * reply)
{
  struct sockaddr_storage ss;
  struct sockaddr_in *sin;
#ifdef ENABLE_IPV6
  struct sockaddr_in6 *sin6;
#endif /* ENABLE_IPV6 */
  SANE_Handle be_handle;
  int fd, len;

  be_handle = handle[h].handle;

  len = sizeof (ss);
  if (getsockname (w->io.fd, (struct sockaddr *) &ss, (socklen_t *) &len) < 0)
    {
      DBG (DBG_ERR, "start_scan: failed to obtain socket address (%s)\n",
	   strerror (errno));
      reply->status = SANE_STATUS_IO_ERROR;
      return -1;
    }

  fd = socket (SS_FAMILY(ss), SOCK_STREAM, 0);
  if (fd < 0)
    {
      DBG (DBG_ERR, "start_scan: failed to obtain data socket (%s)\n",
	   strerror (errno));
      reply->status = SANE_STATUS_IO_ERROR;
      return -1;
    }

  switch (SS_FAMILY(ss))
    {
      case AF_INET:
	sin = (struct sockaddr_in *) &ss;
	sin->sin_port = 0;
	break;
#ifdef ENABLE_IPV6
      case AF_INET6:
	sin6 = (struct sockaddr_in6 *) &ss;
	sin6->sin6_port = 0;
	break;
#endif /* ENABLE_IPV6 */
      default:
	break;
    }

  if (bind (fd, (struct sockaddr *) &ss, len) < 0)
    {
      DBG (DBG_ERR, "start_scan: failed to bind address (%s)\n",
	   strerror (errno));
      reply->status = SANE_STATUS_IO_ERROR;
      return -1;
    }

  if (listen (fd, 1) < 0)
    {
      DBG (DBG_ERR, "start_scan: failed to make socket listen (%s)\n",
	   strerror (errno));
      reply->status = SANE_STATUS_IO_ERROR;
      return -1;
    }

  if (getsockname (fd, (struct sockaddr *) &ss, (socklen_t *) &len) < 0)
    {
      DBG (DBG_ERR, "start_scan: failed to obtain socket address (%s)\n",
	   strerror (errno));
      reply->status = SANE_STATUS_IO_ERROR;
      return -1;
    }

  switch (SS_FAMILY(ss))
    {
      case AF_INET:
	sin = (struct sockaddr_in *) &ss;
	reply->port = ntohs (sin->sin_port);
	break;
#ifdef ENABLE_IPV6
      case AF_INET6:
	sin6 = (struct sockaddr_in6 *) &ss;
	reply->port = ntohs (sin6->sin6_port);
	break;
#endif /* ENABLE_IPV6 */
      default:
	break;
    }

  DBG (DBG_MSG, "start_scan: using port %d for data\n", reply->port);

  reply->status = sane_start (be_handle);
  if (reply->status == SANE_STATUS_GOOD)
    {
      handle[h].scanning = 1;
      handle[h].docancel = 0;
    }

  return fd;
}

#else /* !SANED_USES_AF_INDEP */

static int
start_scan (Wire * w, int h, SANE_Start_Reply * reply)
{
  struct sockaddr_in sin;
  SANE_Handle be_handle;
  int fd, len;

  be_handle = handle[h].handle;

  len = sizeof (sin);
  if (getsockname (w->io.fd, (struct sockaddr *) &sin, (socklen_t *) &len) < 0)
    {
      DBG (DBG_ERR, "start_scan: failed to obtain socket address (%s)\n",
	   strerror (errno));
      reply->status = SANE_STATUS_IO_ERROR;
      return -1;
    }

  fd = socket (AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    {
      DBG (DBG_ERR, "start_scan: failed to obtain data socket (%s)\n",
	   strerror (errno));
      reply->status = SANE_STATUS_IO_ERROR;
      return -1;
    }

  sin.sin_port = 0;
  if (bind (fd, (struct sockaddr *) &sin, len) < 0)
    {
      DBG (DBG_ERR, "start_scan: failed to bind address (%s)\n",
	   strerror (errno));
      reply->status = SANE_STATUS_IO_ERROR;
      return -1;
    }

  if (listen (fd, 1) < 0)
    {
      DBG (DBG_ERR, "start_scan: failed to make socket listen (%s)\n",
	   strerror (errno));
      reply->status = SANE_STATUS_IO_ERROR;
      return -1;
    }

  if (getsockname (fd, (struct sockaddr *) &sin, (socklen_t *) &len) < 0)
    {
      DBG (DBG_ERR, "start_scan: failed to obtain socket address (%s)\n",
	   strerror (errno));
      reply->status = SANE_STATUS_IO_ERROR;
      return -1;
    }

  reply->port = ntohs (sin.sin_port);

  DBG (DBG_MSG, "start_scan: using port %d for data\n", reply->port);

  reply->status = sane_start (be_handle);
  if (reply->status == SANE_STATUS_GOOD)
    {
      handle[h].scanning = 1;
      handle[h].docancel = 0;
    }

  return fd;
}
#endif /* SANED_USES_AF_INDEP */

static int
store_reclen (SANE_Byte * buf, size_t buf_size, int i, size_t reclen)
{
  buf[i++] = (reclen >> 24) & 0xff;
  if (i >= (int) buf_size)
    i = 0;
  buf[i++] = (reclen >> 16) & 0xff;
  if (i >= (int) buf_size)
    i = 0;
  buf[i++] = (reclen >> 8) & 0xff;
  if (i >= (int) buf_size)
    i = 0;
  buf[i++] = (reclen >> 0) & 0xff;
  if (i >= (int) buf_size)
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
  
  DBG (3, "do_scan: start\n");

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
	      /* only set status_dirty if EOF hasn't been already detected */
	      if (status == SANE_STATUS_GOOD) 
		status_dirty = 1; 
	      status = SANE_STATUS_EOF;
	      DBG (DBG_INFO, "do_scan: select_fd was closed --> EOF\n");
	      continue;
	    }
	  else
	    {
	      status = SANE_STATUS_IO_ERROR;
	      DBG (DBG_ERR, "do_scan: select failed (%s)\n", strerror (errno));
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
		  DBG (DBG_INFO, 
		       "do_scan: trying to write %d bytes to client\n",
		       nbytes);
		  nwritten = write (data_fd, buf + writer, nbytes);
		  DBG (DBG_INFO, 
		       "do_scan: wrote %ld bytes to client\n", nwritten);
		  if (nwritten < 0)
		    {
		      DBG (DBG_ERR, "do_scan: write failed (%s)\n",
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
	  if (reader >= (int) sizeof (buf))
	    reader -= sizeof(buf);

	  assert (bytes_in_buf == 0);
	  nbytes = sizeof (buf) - 4;
	  if (reader + nbytes > sizeof (buf))
	    nbytes = sizeof (buf) - reader;

	  DBG (DBG_INFO,
	       "do_scan: trying to read %d bytes from scanner\n", nbytes);
	  status = sane_read (be_handle, buf + reader, nbytes, &length);
	  DBG (DBG_INFO,
	       "do_scan: read %d bytes from scanner\n", length);

	  reset_watchdog ();

	  reader += length;
	  if (reader >= (int) sizeof (buf))
	    reader = 0;
	  bytes_in_buf += length + 4;

	  if (status != SANE_STATUS_GOOD)
	    {
	      reader = i;	/* restore reader index */
	      status_dirty = 1;
	      DBG (DBG_MSG,
		   "do_scan: status = `%s'\n", sane_strstatus(status));
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
	  DBG (DBG_MSG, "do_scan: statuscode `%s' was added to buffer\n", 
	       sane_strstatus(status));
	}

      if (FD_ISSET (w->io.fd, &rd_set))
	{
	  DBG (DBG_MSG,
	       "do_scan: processing RPC request on fd %d\n", w->io.fd);
	  process_request (w);
	  if (handle[h].docancel)
	    break;
	}
    }
  while (status == SANE_STATUS_GOOD || bytes_in_buf > 0 || status_dirty);
  DBG (DBG_MSG, "do_scan: done, status=%s\n", sane_strstatus (status));
  handle[h].docancel = 0;
  handle[h].scanning = 0;
}

static void
process_request (Wire * w)
{
  SANE_Handle be_handle;
  SANE_Word h, word;
  int i;

  DBG (DBG_DBG, "process_request: waiting for request\n");
  sanei_w_set_dir (w, WIRE_DECODE);
  sanei_w_word (w, &word);	/* decode procedure number */

  if (w->status)
    {
      DBG (DBG_ERR,
	   "process_request: bad status %d\n", w->status);
      quit (0);
    }

  current_request = word;

  DBG (DBG_MSG, "process_request: got request %d\n", current_request);

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
	    DBG (DBG_ERR, 
		 "process_request: (open) error while decoding args (%s)\n",
		 strerror (w->status));
	    return;
	  }

	if (!name)
	  {
	    DBG (DBG_ERR, "process_request: (open) device_name == NULL\n");
	    reply.status = SANE_STATUS_INVAL;
	    sanei_w_reply (w, (WireCodecFunc) sanei_w_open_reply, &reply);
	    return;
	  }

	can_authorize = 1;

	resource = strdup (name);
	
	if (strlen(resource) == 0) {

	  const SANE_Device **device_list;

	  DBG(DBG_DBG, "process_request: (open) strlen(resource) == 0\n");
	  free (resource);

	  if ((i = sane_get_devices (&device_list, SANE_TRUE)) != 
	      SANE_STATUS_GOOD) 
	    {
	      DBG(DBG_ERR, "process_request: (open) sane_get_devices failed\n");
	      memset (&reply, 0, sizeof (reply));
	      reply.status = i;
	      sanei_w_reply (w, (WireCodecFunc) sanei_w_open_reply, &reply);
	      break;
	    }

	  if ((device_list == NULL) || (device_list[0] == NULL)) 
	    {
	      DBG(DBG_ERR, "process_request: (open) device_list[0] == 0\n");
	      memset (&reply, 0, sizeof (reply));
	      reply.status = SANE_STATUS_INVAL;
	      sanei_w_reply (w, (WireCodecFunc) sanei_w_open_reply, &reply);
	      break;
	    }

	  resource = strdup (device_list[0]->name);
	  
	}

	if (strchr (resource, ':'))
	  *(strchr (resource, ':')) = 0;

	if (sanei_authorize (resource, "saned", auth_callback) !=
	    SANE_STATUS_GOOD)
	  {
	    DBG (DBG_ERR, "process_request: access to resource `%s' denied\n", 
		 resource);
	    free (resource);
	    memset (&reply, 0, sizeof (reply));	/* avoid leaking bits */
	    reply.status = SANE_STATUS_ACCESS_DENIED;
	  }
	else
	  {
	    DBG (DBG_MSG, "process_request: access to resource `%s' granted\n", 
		 resource);
	    free (resource);
	    memset (&reply, 0, sizeof (reply));	/* avoid leaking bits */
	    reply.status = sane_open (name, &be_handle);
	    DBG (DBG_MSG, "process_request: sane_open returned: %s\n", 
		 sane_strstatus (reply.status));
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
	sanei_w_free (w, (WireCodecFunc) sanei_w_string, &name);
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

	sanei_w_reply (w,(WireCodecFunc) sanei_w_option_descriptor_array,
		       &opt);

	free (opt.desc);
      }
      break;

    case SANE_NET_CONTROL_OPTION:
      {
	SANE_Control_Option_Req req;
	SANE_Control_Option_Reply reply;

	sanei_w_control_option_req (w, &req);
	if (w->status || (unsigned) req.handle >= (unsigned) num_handles
	    || !handle[req.handle].inuse)
	  {
	    DBG (DBG_ERR,
		 "process_rquest: (control_option) "
		 "error while decoding args h=%d (%s)\n"
		 , req.handle, strerror (w->status));
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

	sanei_w_reply (w, (WireCodecFunc) sanei_w_control_option_reply,
		       &reply);
	sanei_w_free (w, (WireCodecFunc) sanei_w_control_option_req, &req);
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

	sanei_w_reply (w, (WireCodecFunc) sanei_w_get_parameters_reply,
		       &reply);
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

#ifdef SANED_USES_AF_INDEP
	if (reply.status == SANE_STATUS_GOOD)
	  {
	    struct sockaddr_storage ss;
	    char text_addr[64];
	    int len;
	    int error;

	    DBG (DBG_MSG, "process_request: waiting for data connection\n");
	    data_fd = accept (fd, 0, 0);
	    close (fd);

	    /* Get address of remote host */
	    len = sizeof (ss);
	    if (getpeername (data_fd, (struct sockaddr *) &ss, (socklen_t *) &len) < 0)
	      {
		DBG (DBG_ERR, "process_request: getpeername failed: %s\n",
		     strerror (errno));
		return;
	      }

	    error = getnameinfo ((struct sockaddr *) &ss, len, text_addr,
				 sizeof (text_addr), NULL, 0, NI_NUMERICHOST);
	    if (error)
	      {
		DBG (DBG_ERR, "process_request: getnameinfo failed: %s\n",
		     gai_strerror (error));
		return;
	      }

	    DBG (DBG_MSG, "process_request: access to data port from %s\n",
		 text_addr);
	    
	    if (strcmp (text_addr, remote_ip) != 0)
	      {
		DBG (DBG_ERR, "process_request: however, only %s is authorized\n",
		     text_addr);
		DBG (DBG_ERR, "process_request: configuration problem or attack?\n");
		close (data_fd);
		data_fd = -1;
		quit (0);
	      }

#else /* !SANED_USES_AF_INDEP */

	if (reply.status == SANE_STATUS_GOOD)
	  {
	    struct sockaddr_in sin;
	    int len;

	    DBG (DBG_MSG, "process_request: waiting for data connection\n");
	    data_fd = accept (fd, 0, 0);
	    close (fd);

	    /* Get address of remote host */
	    len = sizeof (sin);
	    if (getpeername (data_fd, (struct sockaddr *) &sin, 
			     (socklen_t *) &len) < 0)
	      {
		DBG (DBG_ERR, "process_request: getpeername failed: %s\n",
		     strerror (errno));
		return;
	      }

	    if (memcmp (&remote_address, &sin.sin_addr,
			sizeof (remote_address)) != 0)
	      {
		DBG (DBG_ERR, 
		     "process_request: access to data port from %s\n",
		     inet_ntoa (sin.sin_addr));
		DBG (DBG_ERR, 
		     "process_request: however, only %s is authorized\n",
		     inet_ntoa (remote_address));
		DBG (DBG_ERR, 
		     "process_request: configuration problem or attack?\n");
		close (data_fd);
		data_fd = -1;
		quit (0);
	      }
	    else
	      DBG (DBG_MSG, "process_request: access to data port from %s\n",
		   inet_ntoa (sin.sin_addr));
#endif /* SANED_USES_AF_INDEP */

	    if (data_fd < 0)
	      {
		sane_cancel (handle[h].handle);
		handle[h].scanning = 0;
		handle[h].docancel = 0;
		DBG (DBG_ERR, "process_request: accept failed! (%s)\n",
		     strerror (errno));
		return;
	      }
	    fcntl (data_fd, F_SETFL, 1);      /* set non-blocking */
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
	if (h >= 0)
	  {
	    sane_cancel (handle[h].handle);
	    handle[h].docancel = 1;
	  }
	sanei_w_reply (w, (WireCodecFunc) sanei_w_word, &ack);
      }
      break;

    case SANE_NET_EXIT:
      quit (0);
      break;

    case SANE_NET_INIT:
    case SANE_NET_AUTHORIZE:
    default:
      DBG (DBG_ERR,
	   "process_request: received unexpected procedure number %d\n",
	   current_request);
      quit (0);
    }
}

#ifdef SANED_USES_AF_INDEP
int
main (int argc, char *argv[])
{
  int fd = -1;
  int on = 1;
#ifdef TCP_NODELAY
  int level = -1;
#endif

  debug = DBG_WARN;
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

  if (argc == 2 && 
      (strncmp (argv[1], "-d", 2) == 0 || strncmp (argv[1], "-s", 2) == 0))
    {
      /* don't operate in daemon mode: wait for connection request: */
      struct addrinfo *res;
      struct addrinfo *resp;
      struct addrinfo hints;
      struct pollfd *fds = NULL;
      struct pollfd *fdp = NULL;
      int nfds;
      int err;
      int i;
      short sane_port;
      int family;

      if (argv[1][2])
	debug = atoi (argv[1] + 2);
      if (strncmp (argv[1], "-d", 2) == 0)
	log_to_syslog = SANE_FALSE;

      DBG (DBG_WARN, "main: starting debug mode (level %d)\n", debug);

      DBG (DBG_DBG, 
	   "main: trying to get port for service `sane-port' (getaddrinfo)\n");

      memset (&hints, 0, sizeof (struct addrinfo));

      hints.ai_family = PF_UNSPEC;
      hints.ai_flags = AI_PASSIVE;
      hints.ai_socktype = SOCK_STREAM;

      err = getaddrinfo (NULL, "sane-port", &hints, &res);
      if (err)
	{
	  DBG (DBG_WARN, "main: \"sane-port\" service unknown on your host; you should add\n");
	  DBG (DBG_WARN, "main:      sane-port 6566/tcp saned # SANE network scanner daemon\n");
	  DBG (DBG_WARN, "main: to your /etc/services file (or equivalent). Proceeding anyway.\n");
	  err = getaddrinfo (NULL, "6566", &hints, &res);
	  if (err)
	    {
	      DBG (DBG_ERR, "main: getaddrinfo() failed even with numeric port: %s\n",
		   gai_strerror (err));
	      exit (1);
	    }
	}

      for (resp = res, nfds = 0; resp != NULL; resp = resp->ai_next, nfds++)
	;
      
      fds = malloc (nfds * sizeof (struct pollfd));

      if (fds == NULL)
	{
	  DBG (DBG_ERR, "main: not enough memory for fds\n");
	  freeaddrinfo (res);
	  exit (1);
	}

      for (resp = res, i = 0, fdp = fds; resp != NULL; resp = resp->ai_next, i++, fdp++)
	{
	  if (resp->ai_family == AF_INET)
	    {
	      family = 4;
	      sane_port = ntohs (((struct sockaddr_in *) resp->ai_addr)->sin_port);
	    }
#ifdef ENABLE_IPV6
	  else if (resp->ai_family == AF_INET6)
	    {
	      family = 6;
	      sane_port = ntohs (((struct sockaddr_in6 *) resp->ai_addr)->sin6_port);
	    }
#endif /* ENABLE_IPV6 */
	  else
	    {
	      fdp--;
	      nfds--;
	      continue;
	    }

	  DBG (DBG_DBG, "main: [%d] socket () using IPv%d\n", i, family);
	  if ((fd = socket (resp->ai_family, SOCK_STREAM, 0)) < 0)
	    {
	      DBG (DBG_ERR, "main: [%d] socket failed: %s\n", i,
		   strerror (errno));
	      
	      nfds--;
	      fdp--;
	      continue;
	    }
	  
	  DBG (DBG_DBG, "main: [%d] setsockopt ()\n", i);
	  if (setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on)))
	    DBG (DBG_ERR, "main: [%d] failed to put socket in SO_REUSEADDR mode (%s)\n",
		 i, strerror (errno));

	  
	  DBG (DBG_DBG, "main: [%d] bind () to port %d\n", i, sane_port);
	  if (bind (fd, resp->ai_addr, resp->ai_addrlen) < 0)
	    {
	      /*
	       * binding a socket may fail, eg if we already
	       * to the IPv6 addr returned by getaddrinfo (usually the first one),
	       * or if IPv6 isn't supported, but saned was built with IPv6 support
	       */
	      DBG (DBG_ERR, "main: [%d] bind failed: %s\n", i, strerror (errno));

	      close (fd);
	      
	      nfds--;
	      fdp--;
	      continue;
	    }

	  DBG (DBG_DBG, "main: [%d] listen ()\n", i);
	  if (listen (fd, 1) < 0)
	    {
	      DBG (DBG_ERR, "main: [%d] listen failed: %s\n", i, strerror (errno));
	      exit (1);
	    }

	  fdp->fd = fd;
	  fdp->events = POLLIN;
	}

      resp = NULL;
      freeaddrinfo (res);

      if (nfds <= 0)
	{
	  DBG (DBG_ERR, "main: couldn't bind an address. Exiting.\n");
	  exit (1);
	}
	
      DBG (DBG_MSG, "main: waiting for control connection\n");

      while (1)
	{
	  if (poll (fds, nfds, -1) < 0)
	    {
	      if (errno == EINTR)
		continue;
	      else
		{
		  DBG (DBG_ERR, "main: poll failed: %s\n", strerror (errno));
		  free (fds);
		  exit (1);
		}
	    }

	  for (i = 0, fdp = fds; i < nfds; i++, fdp++)
	    {
	      if (! (fdp->revents & POLLIN))
		continue;

	      wire.io.fd = accept (fdp->fd, 0, 0);
	      if (wire.io.fd < 0)
		{
		  DBG (DBG_ERR, "main: accept failed: %s", strerror (errno));
		  free (fds);
		  exit (1);
		}

	      for (i = 0, fdp = fds; i < nfds; i++, fdp++)
		close (fdp->fd);

	      free (fds);

	      break;
	    }
	  break;
	}
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
	DBG (DBG_WARN, "main: cannot look up `tcp' protocol number");
      }
    else
      level = p->p_proto;
  }
# endif	/* SOL_TCP */
  if (level == -1
      || setsockopt (wire.io.fd, level, TCP_NODELAY, &on, sizeof (on)))
    DBG (DBG_WARN, "main: failed to put socket in TCP_NODELAY mode (%s)",
	 strerror (errno));
#endif /* !TCP_NODELAY */

/* define the version string depending on which network code is used */
#ifdef ENABLE_IPV6
  DBG (DBG_WARN, "saned (AF-indep+IPv6) from %s ready\n", PACKAGE_STRING);
#else
  DBG (DBG_WARN, "saned (AF-indep) from %s ready\n", PACKAGE_STRING);
#endif /* ENABLE_IPV6 */

  if (init (&wire) < 0)
    quit (0);

  while (1)
    {
      reset_watchdog ();
      process_request (&wire);
    }
}

#else /* !SANED_USES_AF_INDEP */

int
main (int argc, char *argv[])
{
  int fd, on = 1;
#ifdef TCP_NODELAY
  int level = -1;
#endif

  debug = DBG_WARN;
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

  if (argc == 2 && 
      (strncmp (argv[1], "-d", 2) == 0 || strncmp (argv[1], "-s", 2) == 0))
    {
      /* don't operate in daemon mode: wait for connection request: */
      struct sockaddr_in sin;
      struct servent *serv;
      short port;

      if (argv[1][2])
	debug = atoi (argv[1] + 2);
      if (strncmp (argv[1], "-d", 2) == 0)
	log_to_syslog = SANE_FALSE;

      DBG (DBG_WARN, "main: starting debug mode (level %d)\n", debug);

      memset (&sin, 0, sizeof (sin));

      DBG (DBG_DBG, 
	   "main: trying to get port for service `sane' (getservbyname)\n");
      serv = getservbyname ("sane-port", "tcp");
      if (serv)
	{
	  port = serv->s_port;
	  DBG (DBG_MSG, "main: port is %d\n", ntohs (port));
	}
      else
	{
	  port = htons (6566);
	  DBG (DBG_WARN, "main: could not find `sane' service (%s)\n", 
	       strerror (errno));
	  DBG (DBG_WARN, "main: using default port %d\n", ntohs (port));
	}
      sin.sin_family = AF_INET;
      sin.sin_addr.s_addr = INADDR_ANY;
      sin.sin_port = port;

      DBG (DBG_DBG, "main: socket ()\n");
      fd = socket (AF_INET, SOCK_STREAM, 0);

      DBG (DBG_DBG, "main: setsockopt ()\n");
      if (setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on)))
	DBG (DBG_ERR, "failed to put socket in SO_REUSEADDR mode (%s)",
		strerror (errno));

      DBG (DBG_DBG, "main: bind ()\n");
      if (bind (fd, (struct sockaddr *) &sin, sizeof (sin)) < 0)
	{
	  DBG (DBG_ERR, "main: bind failed: %s", strerror (errno));
	  exit (1);
	}
      DBG (DBG_DBG, "main: listen ()\n");
      if (listen (fd, 1) < 0)
	{
	  DBG (DBG_ERR, "main: listen failed: %s", strerror (errno));
	  exit (1);
	}
      DBG (DBG_MSG, "main: waiting for control connection\n");
      wire.io.fd = accept (fd, 0, 0);
      if (wire.io.fd < 0)
	{
	  DBG (DBG_ERR, "main: accept failed: %s", strerror (errno));
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
	DBG (DBG_WARN, "main: cannot look up `tcp' protocol number");
      }
    else
      level = p->p_proto;
  }
# endif	/* SOL_TCP */
  if (level == -1
      || setsockopt (wire.io.fd, level, TCP_NODELAY, &on, sizeof (on)))
    DBG (DBG_WARN, "main: failed to put socket in TCP_NODELAY mode (%s)",
	 strerror (errno));
#endif /* !TCP_NODELAY */

  DBG (DBG_WARN, "saned from %s ready\n", PACKAGE_STRING);

  if (init (&wire) < 0)
    quit (0);

  while (1)
    {
      reset_watchdog ();
      process_request (&wire);
    }
}
#endif /* SANED_USES_AF_INDEP */
