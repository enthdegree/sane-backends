/* SANE - Scanner Access Now Easy.

   Copyright (C) 2008  2012 by Louis Lagendijk

   This file is part of the SANE package.

   SANE is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   SANE is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with sane; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

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
*/
#include  "../include/sane/config.h"
#include  "../include/sane/sane.h"

/*
 * Standard types etc
 */
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <unistd.h>
#include <stdio.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

/* 
 * networking stuff
 */
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>
#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>
#endif
#ifdef HAVE_SYS_SELSECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#include <errno.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include "pixma_bjnp_private.h"
#include "pixma_bjnp.h"
#include "pixma_rename.h"
#include "pixma.h"
#include "pixma_common.h"


/* static data */
static device_t device[BJNP_NO_DEVICES];
static int bjnp_no_devices = 0;

/*
 * Private functions
 */

static int sa_is_equal( const struct sockaddr * sa1, const struct sockaddr * sa2)
{
  if ((sa1 == NULL) || (sa2 == NULL) )
    return 0;

  if (sa1->sa_family == sa2-> sa_family)
    {
      if( sa1 -> sa_family == AF_INET)
        {
          const struct sockaddr_in *sa1_4 = (const struct sockaddr_in *) sa1;
          const struct sockaddr_in *sa2_4 = (const struct sockaddr_in *) sa2;
          if ( (sa1_4->sin_port == sa2_4->sin_port) &&
               (sa1_4->sin_addr.s_addr == sa2_4->sin_addr.s_addr))
            {
            return 1;
            }
        } 
      else if (sa1 -> sa_family == AF_INET6 )
        {
          const struct sockaddr_in6 *sa1_6 = ( const struct sockaddr_in6 *) sa1;
          const struct sockaddr_in6 *sa2_6 = ( const struct sockaddr_in6 *) sa2;
          if ( (sa1_6->sin6_port == sa2_6->sin6_port) &&
              (memcmp(&(sa1_6->sin6_addr), &(sa2_6->sin6_addr), sizeof(sa1_6->sin6_addr)) == 0))
            {
              return 1;
            }
        } 
    }
    return 0; 
}

static int
parse_IEEE1284_to_model (char *scanner_id, char *model)
{
/*
 * parses the  IEEE1284  ID of the scanner to retrieve make and model
 * of the scanner
 * Returns: 0 = not found
 *          1 = found, model is set
 */

  char s[BJNP_IEEE1284_MAX];
  char *tok;

  strcpy (s, scanner_id);
  model[0] = '\0';

  tok = strtok (s, ";");
  while (tok != NULL)
    {
      /* DES contains make and model */

      if (strncmp (tok, "DES:", 4) == 0)
	{
	  strcpy (model, tok + 4);
	  return 1;
	}
      tok = strtok (NULL, ";");
    }
  return 0;
}

static int
charTo2byte (char *d, const char *s, int len)
{
  /*
   * copy ASCII string to 2 byte unicode string
   * len is length of destination buffer
   * Returns: number of characters copied
   */

  int done = 0;
  int copied = 0;
  int i;

  len = len / 2;
  for (i = 0; i < len; i++)
    {
      d[2 * i] = '\0';
      if (s[i] == '\0')
	{
	  done = 1;
	}
      if (done == 0)
	{
	  d[2 * i + 1] = s[i];
	  copied++;
	}
      else
	d[2 * i + 1] = '\0';
    }
  return copied;
}

static char *
getusername (void)
{
  static char noname[] = "sane_pixma";
  struct passwd *pwdent;

#ifdef HAVE_PWD_H
  if (((pwdent = getpwuid (geteuid ())) != NULL) && (pwdent->pw_name != NULL))
    return pwdent->pw_name;
#endif
  return noname;
}


static char *
truncate_hostname (char *hostname, char *short_hostname)
{
  char *dot;

  /* determine a short hostname (max HOSTNAME_SHORT_MAX chars */

  strncpy (short_hostname, hostname, SHORT_HOSTNAME_MAX);
  short_hostname[SHORT_HOSTNAME_MAX - 1] = '\0';

  if (strlen (hostname) > SHORT_HOSTNAME_MAX)
    {
      /* this is a hostname, not an ip-address, so remove domain part of the name */

      if ((dot = strchr (short_hostname, '.')) != NULL)
	*dot = '\0';
    }
  return short_hostname;
}

static int
bjnp_open_tcp (int devno)
{
  int sock;
  int val;
  int domain;

  domain = device[devno].addr -> sa_family;

  if ((sock = socket (domain, SOCK_STREAM, 0)) < 0)
    {
      PDBG (pixma_dbg (LOG_CRIT, "bjnp_open_tcp: Can not create socket: %s\n",
		       strerror (errno)));
      return -1;
    }

  val = 1;
  setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof (val));

#if 0
  val = 1;
  setsockopt (sock, SOL_SOCKET, SO_REUSEPORT, &val, sizeof (val));

  val = 1;
#endif

  /*
   * Using TCP_NODELAY improves responsiveness, especially on systems
   * with a slow loopback interface...
   */

  val = 1;
  setsockopt (sock, IPPROTO_TCP, TCP_NODELAY, &val, sizeof (val));

/*
 * Close this socket when starting another process...
 */

  fcntl (sock, F_SETFD, FD_CLOEXEC); 

  if (connect
      (sock, device[devno].addr, sizeof(struct sockaddr_storage )) != 0)
    {
      PDBG (pixma_dbg
	    (LOG_CRIT, "bjnp_open_tcp: Can not connect to scanner: %s\n",
	     strerror (errno)));
      return -1;
    }
  device[devno].tcp_socket = sock;
  return 0;
}

static int
split_uri (const char *devname, char *method, char *host, char *port,
	   char *args)
{
  char copy[1024];
  char *start;
  char next;
  int i;

  strcpy (copy, devname);
  start = copy;

/*
 * retrieve method
 */

  i = 0;
  while ((start[i] != '\0') && (start[i] != ':'))
    {
      i++;
    }

  if (((strncmp (start + i, "://", 3) != 0)) || (i > BJNP_METHOD_MAX -1 ))
    {
      PDBG (pixma_dbg (LOG_NOTICE, "Can not find method in %s (offset %d)\n",
		       devname, i));
      return -1;
    }

  start[i] = '\0';
  strcpy (method, start);
  start = start + i + 3;

/*
 * retrieve host
 */

  if (start[0] == '[')
    {
      /* literal IPv6 address */

      char *end_of_address = strchr(start, ']'); 

      if ( ( end_of_address == NULL) || 
           ( (end_of_address[1] != ':') && (end_of_address[1] != '/' ) &&  (end_of_address[1] != '\0' )) ||
           ( (end_of_address - start) >= BJNP_HOST_MAX ) )
        {
          PDBG (pixma_dbg (LOG_NOTICE, "Can not find hostname or address in %s\n", devname));
          return -1;
        }
      next = end_of_address[1];
      *end_of_address = '\0';
      strcpy(host, start + 1);
      start = end_of_address +1;
    }
  else
    {
      i = 0;
      while ((start[i] != '\0') && (start[i] != '/') && (start[i] != ':'))
        {
          i++;
        }
      next = start[i];
      start[i] = '\0';
      if ((i == 0) || (i >= BJNP_HOST_MAX ) )
        {
          PDBG (pixma_dbg (LOG_NOTICE, "Can not find hostname or address in %s\n", devname));
          return -1;
        }
      strcpy (host, start);
      start = start + i +1;
    }


/*
 * retrieve port number
 */

  if (next != ':')
    strcpy(port, "");
  else
    {
      char *end_of_port = strchr(start, '/');
      if (end_of_port == NULL) 
        { 
          next = '\0';
        }
      else
        {
          next = *end_of_port;
          *end_of_port = '\0';
        }
      if ((strlen(start) == 0) || (strlen(start) >= BJNP_PORT_MAX ) )
        {
          PDBG (pixma_dbg (LOG_NOTICE, "Can not find port in %s (have \"%s\")\n", devname, start));
          return -1;
        }
      strcpy(port, start);
    }

/*
 * Retrieve arguments
 */

  if (next == '/')
    {
    i = strlen(start);
    if ( i >= BJNP_ARGS_MAX)
      {
        PDBG (pixma_dbg (LOG_NOTICE, "Argument string too long in %s\n", devname));
      }
    strcpy (args, start);
    }
  else
    strcpy (args, "");
  return 0;
}



static void
set_cmd (int devno, struct BJNP_command *cmd, char cmd_code, int payload_len)
{
  /*
   * Set command buffer with command code, session_id and lenght of payload
   * Returns: sequence number of command
   */
  strncpy (cmd->BJNP_id, BJNP_STRING, sizeof (cmd->BJNP_id));
  cmd->dev_type = BJNP_CMD_SCAN;
  cmd->cmd_code = cmd_code;
  cmd->unknown1 = htons (0);
  if (devno == -1)
    {
      /* device not yet opened, use 0 for serial and session) */
      cmd->seq_no = htons (0);
      cmd->session_id = htons (0);
    }
  else
    {
      cmd->seq_no = htons (++(device[devno].serial));
      cmd->session_id = (cmd_code == CMD_UDP_POLL ) ? 0 : htons (device[devno].session_id);
      device[devno].last_cmd = cmd_code;
    }
  cmd->payload_len = htonl (payload_len);
}

static int
bjnp_setup_udp_socket ( const int dev_no )
{
  /*
   * Setup a udp socket for the given device
   * Returns the socket or -1 in case of error
   */

  int sockfd;
  char addr_string[256];
  int port;

  struct sockaddr * addr = device[dev_no].addr;
  if ( addr->sa_family == AF_INET)
    {
      struct sockaddr_in *addr_ipv4 = (struct sockaddr_in *) addr;
      inet_ntop( AF_INET, &(addr_ipv4 -> sin_addr.s_addr), addr_string, 256);
      port = ntohs (addr_ipv4->sin_port);
    }
  else if (addr->sa_family == AF_INET6)
    {
      struct sockaddr_in6 *addr_ipv6 = (struct sockaddr_in6 *) addr;
      inet_ntop( AF_INET6, &(addr_ipv6->sin6_addr), addr_string, 256);
      port = ntohs (addr_ipv6->sin6_port);
    }
  else 
    {
      /* unknown address family, should not occur */
      return -1;
    }

  PDBG (pixma_dbg (LOG_DEBUG, "setup_udp_socket: Setting up the UDP socket to: %s  port %d\n",
		   addr_string, port ) );

  if ((sockfd = socket (addr->sa_family, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
      PDBG (pixma_dbg
	    (LOG_CRIT, "setup_udp_socket: can not open socket - %s\n",
	     strerror (errno)));
      return -1;
    }

  if (connect
      (sockfd, device[dev_no].addr, sizeof(struct sockaddr_storage) )!= 0)
    {
      PDBG (pixma_dbg
	    (LOG_CRIT, "setup_udp_socket: connect failed- %s\n",
	     strerror (errno)));
      return -1;
    }
  device[dev_no].udp_socket = sockfd;
  return sockfd;
}

static int
udp_command (const int dev_no, char *command, int cmd_len, char *response,
	     int resp_len)
{
  /* 
   * send udp command to given device and recieve the response`
   * returns: the legth of the response or -1 
   */
  int sockfd = device[dev_no].udp_socket;
  struct timeval timeout;
  int result;
  int try, attempt;
  int numbytes;
  fd_set fdset;
  struct BJNP_command *resp = (struct BJNP_command *) response;
  struct BJNP_command *cmd = (struct BJNP_command *) command;
  
  for (try = 0; try < 3; try++)
    {
      if ((numbytes = send (sockfd, command, cmd_len, 0)) != cmd_len)
	{
	  PDBG (pixma_dbg
		(LOG_NOTICE, "udp_command: Sent only 0x%x = %d bytes of packet\n",
		 numbytes, numbytes));
	  continue;
	}

      attempt = 0;
      do
	{
	  /* wait for data to be received, ignore signals being received */
          /* skip late udp responses (they have an incorrect sequence number */
	  FD_ZERO (&fdset);
	  FD_SET (sockfd, &fdset);

	  timeout.tv_sec = BJNP_TIMEOUT_UDP;
	  timeout.tv_usec = 0; 
	}
      while (((result =
	       select (sockfd + 1, &fdset, NULL, NULL, &timeout)) <= 0)
	     && (errno == EINTR) && (attempt++ < MAX_SELECT_ATTEMPTS) 
             && resp-> seq_no != cmd->seq_no);

      if (result <= 0)
	{
	  PDBG (pixma_dbg
		(LOG_CRIT, "udp_command: No data received (select): %s\n",
		 result == 0 ? "timed out" : strerror (errno)));
	  continue;
	}

      if ((numbytes = recv (sockfd, response, resp_len, 0)) == -1)
	{
	  PDBG (pixma_dbg
		(LOG_CRIT, "udp_command: no data received (recv): %s",
		 strerror (errno)));
	  continue;
	}
      return numbytes;
    }

  /* no response even after retry */

  return -1;
}

static int
get_scanner_id (const int dev_no, char *model)
{
  /*
   * get scanner identity
   * Sets model (make and model)
   * Return 0 on success, -1 in case of errors
   */

  struct BJNP_command cmd;
  struct IDENTITY *id;
  char scanner_id[BJNP_IEEE1284_MAX];
  int resp_len;
  char resp_buf[BJNP_RESP_MAX];
  int id_len;

  /* set defaults */

  strcpy (model, "Unidentified scanner");

  set_cmd (dev_no, &cmd, CMD_UDP_GET_ID, 0);

  PDBG (pixma_dbg (LOG_DEBUG2, "Get scanner identity\n"));
  PDBG (pixma_hexdump (LOG_DEBUG2, (char *) &cmd,
		       sizeof (struct BJNP_command)));

  resp_len =
    udp_command (dev_no, (char *) &cmd, sizeof (struct BJNP_command),
		 resp_buf, BJNP_RESP_MAX);

  if (resp_len <= 0)
    return -1;

  PDBG (pixma_dbg (LOG_DEBUG2, "scanner identity:\n"));
  PDBG (pixma_hexdump (LOG_DEBUG2, resp_buf, resp_len));

  id = (struct IDENTITY *) resp_buf;

  /* truncate string to be safe */
  id_len = htons( id-> id_len ) - sizeof(id->id_len);
  id->id[id_len] = '\0';
  strcpy (scanner_id, id->id);

  PDBG (pixma_dbg (LOG_INFO, "Scanner identity string = %s - lenght = %d\n", scanner_id, id_len));

  /* get make&model from IEEE1284 id  */

  if (model != NULL)
  {
    parse_IEEE1284_to_model (scanner_id, model);
    PDBG (pixma_dbg (LOG_INFO, "Scanner model = %s\n", model));
  }
  return 0;
}

static void
get_scanner_name(const struct sockaddr *scanner_sa, char *host)
{
  /*
   * Parse identify command responses to ip-address
   * and hostname
   */

  struct addrinfo *results;
  struct addrinfo *result;
  const struct sockaddr_in * scanner_sa4;
  const struct sockaddr_in6 * scanner_sa6;
  char tmp_addr[INET6_ADDRSTRLEN];
  char ip_address[INET6_ADDRSTRLEN + 2];
  int error;
  int match = 0;
  char service[64];

  switch (scanner_sa -> sa_family)
    {
      case AF_INET:
        scanner_sa4 = (const struct sockaddr_in *) scanner_sa;
        inet_ntop( AF_INET, &(scanner_sa4 -> sin_addr), ip_address, sizeof(ip_address ) );
        sprintf(service, "%d", ntohs( scanner_sa4 -> sin_port ));
        break;
      default: 
        scanner_sa6 = (const struct sockaddr_in6 *) scanner_sa;
        inet_ntop( AF_INET6, &(scanner_sa6 -> sin6_addr), tmp_addr, sizeof(tmp_addr) );
        sprintf(ip_address, "[%s]", tmp_addr);
        sprintf(service, "%d", ntohs( scanner_sa6 -> sin6_port)) ;
    }        

  /* do reverse name lookup, if hostname can not be found return ip-address */

  if( (error = getnameinfo( scanner_sa, sizeof(struct sockaddr_storage), 
                  host, BJNP_HOST_MAX , NULL, 0, 0) ) != 0 )
    {
      PDBG (pixma_dbg(LOG_INFO, "Reverse lookup failed: %s\n", gai_strerror(error) ) );
    }
  else
    {
      /* some buggy routers return rubbish if reverse lookup fails, so 
       * we do a forward lookup on the received name to see if it matches */

      if (getaddrinfo(host , service, NULL, &results) == 0) 
        {
          result = results;

          while (result != NULL) 
            {
               if(sa_is_equal( scanner_sa, result-> ai_addr))
                 {
                     /* found match, good */
                     PDBG (pixma_dbg (LOG_INFO, 
                              "Forward lookup for %s succeeded, using as hostname\n", host));
                    match = 1;
                    break;
                 }
              result = result-> ai_next;
            }
          freeaddrinfo(results);

          if (match != 1) 
            {
              PDBG (pixma_dbg (LOG_INFO, 
                 "Reseverse lookup for %s succeeded, IP-address however not found, using IP-address %s instead\n", 
                 host, ip_address));
              strcpy (host, ip_address);
            }
         } 
       else 
         {
           /* lookup failed, use ip-address */
           PDBG ( pixma_dbg (LOG_INFO, "reverse lookup of %s failed, using IP-address", ip_address));
           strcpy (host, ip_address);
         }
    }
}

static int create_broadcast_socket( const struct sockaddr * local_addr, int local_port )
{
  struct sockaddr_in local_addr_4;
  int sockfd = -1;
  int broadcast = 1;


 if ((sockfd = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
      PDBG (pixma_dbg
            (LOG_CRIT, "create_braodcast_socket: can not open socket - %s",
             strerror (errno)));
      return -1;
    }

  /* Set broadcast flag on socket */

  if (setsockopt
      (sockfd, SOL_SOCKET, SO_BROADCAST, (const char *) &broadcast,
       sizeof (broadcast)) != 0)
    {
      PDBG (pixma_dbg
            (LOG_CRIT,
             "create_braodcast_socket: setting socket options failed - %s",
             strerror (errno)));
      close (sockfd);
      return -1;
    };

  /* Bind to local address, use BJNP port */

  memcpy( &local_addr_4, local_addr, sizeof(struct sockaddr_in) );
  local_addr_4.sin_port = htons (local_port);

  if (bind
      (sockfd, (struct sockaddr *) &local_addr_4,
       (socklen_t) sizeof (struct sockaddr_in)) != 0)
    {
      PDBG (pixma_dbg
            (LOG_CRIT,
             "create_braodcast_socket: bind socket to local address failed - %s\n",
             strerror (errno)));
      close (sockfd);
      return -1;
    }
  return sockfd;
}

static int 
prepare_socket(const char *if_name, const struct sockaddr *local_sa, 
               const struct sockaddr *broadcast_sa)
{
  int socket = -1;
  if ( local_sa == NULL )
    {
      PDBG (pixma_dbg (LOG_DEBUG, 
                       "%s is not a valid IPv4 interface, skipping...\n",
                       if_name));
      return -1;
    }
  switch( local_sa -> sa_family )
    {
      case AF_INET:
        {
          const struct sockaddr_in * broadcast_sa_4 = (const struct sockaddr_in *) broadcast_sa;
          if ( (broadcast_sa_4 == NULL) ||
               (broadcast_sa_4 -> sin_addr.s_addr == 
                                                    htonl (INADDR_LOOPBACK) ) )
            {
              /* not a valid IPv4 address */

              PDBG (pixma_dbg (LOG_DEBUG, 
                               "%s is not a valid IPv4 interface, skipping...\n",
	                       if_name));
              return -1;
            }
          else
            {
             socket = create_broadcast_socket( local_sa, BJNP_PORT_SCAN);
              if (socket != -1) 
                {
                  PDBG (pixma_dbg (LOG_INFO, "%s is IPv4 capable, sending broadcast, socket = %d\n",
                         if_name, socket));
                }
              else
                {
                   PDBG (pixma_dbg (LOG_INFO, "%s is IPv4 capable, but failed to create a socket.\n",
                         if_name));
                   return -1;
                }
            }
        }
        break;

      case AF_INET6:
        socket = -1;
        break;

      default:
        socket = -1;
    }
  return socket;
}

static int
bjnp_send_broadcast (int sockfd, struct in_addr broadcast_addr, 
                     struct BJNP_command cmd, int size)
{
  struct sockaddr_in sendaddr;
  int num_bytes;

  /* set address to send packet to */
  /* usebroadcast address of interface */

  sendaddr.sin_family = AF_INET;
  sendaddr.sin_port = htons (BJNP_PORT_SCAN);
  sendaddr.sin_addr = broadcast_addr;
  memset (sendaddr.sin_zero, '\0', sizeof sendaddr.sin_zero);

  /* TODO: IPv6 compat */
  if ((num_bytes = sendto (sockfd, &cmd, size, 0,
			  (struct sockaddr *) &sendaddr,
			  sizeof (sendaddr))) != size)
    {
      PDBG (pixma_dbg (LOG_INFO,
		       "bjnp_send_broadcast: Socket: %d: sent only %x = %d bytes of packet, error = %s\n",
		       sockfd, num_bytes, num_bytes, strerror (errno)));
      /* not allowed, skip this interface */

      close (sockfd);
      return -1;
    }
  return sockfd;
}

static void
bjnp_finish_job (int devno)
{
/* 
 * Signal end of scanjob to scanner
 */

  char resp_buf[BJNP_RESP_MAX];
  int resp_len;
  struct BJNP_command cmd;

  set_cmd (devno, &cmd, CMD_UDP_CLOSE, 0);

  PDBG (pixma_dbg (LOG_DEBUG2, "Finish scanjob\n"));
  PDBG (pixma_hexdump
	(LOG_DEBUG2, (char *) &cmd, sizeof (struct BJNP_command)));
  resp_len =
    udp_command (devno, (char *) &cmd, sizeof (struct BJNP_command), resp_buf,
		 BJNP_RESP_MAX);

  if (resp_len != sizeof (struct BJNP_command))
    {
      PDBG (pixma_dbg
	    (LOG_INFO,
	     "Received %d characters on close scanjob command, expected %d\n",
	     resp_len, (int) sizeof (struct BJNP_command)));
      return;
    }
  PDBG (pixma_dbg (LOG_DEBUG2, "Finish scanjob response\n"));
  PDBG (pixma_hexdump (LOG_DEBUG2, resp_buf, resp_len));

}

#ifdef PIXMA_BJNP_USE_STATUS
static int
bjnp_poll_scanner (int devno, char type,char *hostname, char *user, SANE_Byte *status, int size)
{
/* 
 * send details of user to the scanner
 */

  char cmd_buf[BJNP_CMD_MAX];
  char resp_buf[BJNP_RESP_MAX];
  int resp_len;
  int len = 0;			/* payload length */
  int buf_len;			/* length of the whole command  buffer */
  struct POLL_DETAILS *poll;
  struct POLL_RESPONSE *response;
  char user_host[256]; 
  time_t t;
  int user_host_len;

  poll = (struct POLL_DETAILS *) (cmd_buf);
  memset(poll, 0, sizeof( struct POLL_DETAILS));
  memset(resp_buf, 0, sizeof( resp_buf) );


  /* create payload */
  poll->type = htons(type);

  user_host_len =  sizeof( poll -> extensions.type2.user_host);
  snprintf(user_host, (user_host_len /2) ,"%s  %s", user, hostname);
  user_host[ user_host_len /2 + 1] = '\0';

  switch( type) {
    case 0:
      len = 80;
      break;
    case 1:
      charTo2byte(poll->extensions.type1.user_host, user_host, user_host_len);
      len = 80;
      break;
    case 2:
      poll->extensions.type2.dialog = htonl(device[devno].dialog);     
      charTo2byte(poll->extensions.type2.user_host, user_host, user_host_len); 
      poll->extensions.type2.unknown_1 = htonl(0x14);
      poll->extensions.type2.unknown_2 = htonl(0x10); 
      t = time (NULL);
      strftime (poll->extensions.type2.ascii_date, 
                sizeof (poll->extensions.type2.ascii_date), 
               "%Y%m%d%H%M%S", localtime (&t));
      len = 116;
      break;
    case 5:
      poll->extensions.type5.dialog = htonl(device[devno].dialog);     
      charTo2byte(poll->extensions.type5.user_host, user_host, user_host_len); 
      poll->extensions.type5.unknown_1 = htonl(0x14);
      poll->extensions.type5.key = htonl(device[devno].status_key); 
      len = 100;
      break;
    default:
      PDBG (pixma_dbg (LOG_INFO, "bjnp_poll_scanner: unknown packet type: %d\n", type));
      return -1;
  }; 
  /* we can only now set the header as we now know the length of the payload */
  set_cmd (devno, (struct BJNP_command *) cmd_buf, CMD_UDP_POLL,
	   len);

  buf_len = len + sizeof(struct BJNP_command);
  PDBG (pixma_dbg (LOG_DEBUG2, "Poll details (type %d)\n", type));
  PDBG (pixma_hexdump (LOG_DEBUG2, cmd_buf,
		       buf_len));

  resp_len = udp_command (devno, cmd_buf, buf_len,  resp_buf, BJNP_RESP_MAX);

  if (resp_len > 0)
    {
      PDBG (pixma_dbg (LOG_DEBUG2, "Poll details response:\n"));
      PDBG (pixma_hexdump (LOG_DEBUG2, resp_buf, resp_len));
      response = (struct POLL_RESPONSE *) resp_buf;

      device[devno].dialog = ntohl( response -> dialog );

      if ( response -> result[3] == 1 )
        {
          return BJNP_RESTART_POLL;
        }
      if ( (response -> result[2] & 0x80) != 0) 
        {
          memcpy( status, response->status, size);
          PDBG( pixma_dbg(LOG_INFO, "received button status!\n"));
	  PDBG (pixma_hexdump( LOG_DEBUG2, status, size ));
	  device[devno].status_key = ntohl( response -> key );
          return  size;
        }
    }
  return 0;
}
#endif

static void
bjnp_send_job_details (int devno, char *hostname, char *user, char *title)
{
/* 
 * send details of scanjob to scanner
 */

  char cmd_buf[BJNP_CMD_MAX];
  char resp_buf[BJNP_RESP_MAX];
  int resp_len;
  struct JOB_DETAILS *job;
  struct BJNP_command *resp;

  /* send job details command */

  set_cmd (devno, (struct BJNP_command *) cmd_buf, CMD_UDP_JOB_DETAILS,
	   sizeof (*job) - sizeof (struct BJNP_command));

  /* create payload */

  job = (struct JOB_DETAILS *) (cmd_buf);
  charTo2byte (job->unknown, "", sizeof (job->unknown));
  charTo2byte (job->hostname, hostname, sizeof (job->hostname));
  charTo2byte (job->username, user, sizeof (job->username));
  charTo2byte (job->jobtitle, title, sizeof (job->jobtitle));

  PDBG (pixma_dbg (LOG_DEBUG2, "Job details\n"));
  PDBG (pixma_hexdump (LOG_DEBUG2, cmd_buf,
		       (sizeof (struct BJNP_command) + sizeof (*job))));

  resp_len = udp_command (devno, cmd_buf,
			  sizeof (struct JOB_DETAILS), resp_buf,
			  BJNP_RESP_MAX);

  if (resp_len > 0)
    {
      PDBG (pixma_dbg (LOG_DEBUG2, "Job details response:\n"));
      PDBG (pixma_hexdump (LOG_DEBUG2, resp_buf, resp_len));
      resp = (struct BJNP_command *) resp_buf;
      device[devno].session_id = ntohs (resp->session_id);
    }
}

static int
bjnp_write (int devno, const SANE_Byte * buf, size_t count)
{
/*
 * This function writes scandata to the scanner. 
 * Returns: number of bytes written to the scanner
 */
  int sent_bytes;
  int terrno;
  struct SCAN_BUF bjnp_buf;

  if (device[devno].scanner_data_left)
    PDBG (pixma_dbg
	  (LOG_CRIT, "bjnp_write: ERROR: scanner data left = 0x%lx = %ld\n",
	   (unsigned long) device[devno].scanner_data_left,
	   (unsigned long) device[devno].scanner_data_left));

  /* set BJNP command header */

  set_cmd (devno, (struct BJNP_command *) &bjnp_buf, CMD_TCP_SEND, count);
  memcpy (bjnp_buf.scan_data, buf, count);
  PDBG (pixma_dbg (LOG_DEBUG, "bjnp_write: sending 0x%lx = %ld bytes\n",
		   (unsigned long) count, (unsigned long) count);
	PDBG (pixma_hexdump (LOG_DEBUG2, (char *) &bjnp_buf,
			     sizeof (struct BJNP_command) + count)));

  if ((sent_bytes =
       send (device[devno].tcp_socket, &bjnp_buf,
	     sizeof (struct BJNP_command) + count, 0)) <
      (ssize_t) (sizeof (struct BJNP_command) + count))
    {
      /* return result from write */
      terrno = errno;
      PDBG (pixma_dbg (LOG_CRIT, "bjnp_write: Could not send data!\n"));
      errno = terrno;
      return sent_bytes;
    }
  /* correct nr of bytes sent for length of command */

  else if (sent_bytes != (int) (sizeof (struct BJNP_command) + count))
    {
      errno = EIO;
      return -1;
    }
  return count;
}

static int
bjnp_send_read_request (int devno)
{
/*
 * This function reads responses from the scanner.  
 * Returns: 0 on success, else -1
 *  
 */
  int sent_bytes;
  int terrno;
  struct BJNP_command bjnp_buf;

  if (device[devno].scanner_data_left)
    PDBG (pixma_dbg
	  (LOG_CRIT,
	   "bjnp_send_read_request: ERROR scanner data left = 0x%lx = %ld\n",
	   (unsigned long) device[devno].scanner_data_left,
	   (unsigned long) device[devno].scanner_data_left));

  /* set BJNP command header */

  set_cmd (devno, (struct BJNP_command *) &bjnp_buf, CMD_TCP_REQ, 0);

  PDBG (pixma_dbg (LOG_DEBUG, "bjnp_send_read_req sending command\n"));
  PDBG (pixma_hexdump (LOG_DEBUG2, (char *) &bjnp_buf,
		       sizeof (struct BJNP_command)));

  if ((sent_bytes =
       send (device[devno].tcp_socket, &bjnp_buf, sizeof (struct BJNP_command),
	     0)) < 0)
    {
      /* return result from write */
      terrno = errno;
      PDBG (pixma_dbg
	    (LOG_CRIT, "bjnp_send_read_request: Could not send data!\n"));
      errno = terrno;
      return -1;
    }
  return 0;
}

static SANE_Status
bjnp_recv_header (int devno)
{
/*
 * This function receives the response header to bjnp commands.
 * devno device number
 * Returns: 
 * SANE_STATUS_IO_ERROR when any IO error occurs
 * SANE_STATUS_GOOD in case no errors were encountered
 */
  struct BJNP_command resp_buf;
  fd_set input;
  struct timeval timeout;
  int recv_bytes;
  int terrno;
  int result;
  int fd;
  int attempt;

  PDBG (pixma_dbg
	(LOG_DEBUG, "bjnp_recv_header: receiving response header\n"));
  fd = device[devno].tcp_socket;

  if (device[devno].scanner_data_left)
    PDBG (pixma_dbg
	  (LOG_CRIT,
	   "bjnp_send_request: ERROR scanner data left = 0x%lx = %ld\n",
	   (unsigned long) device[devno].scanner_data_left,
	   (unsigned long) device[devno].scanner_data_left));

  attempt = 0;
  do
    {
      /* wait for data to be received, ignore signals being received */
      FD_ZERO (&input);
      FD_SET (fd, &input);

      timeout.tv_sec = BJNP_TIMEOUT_TCP;
      timeout.tv_usec = 0;
    }
  while (((result = select (fd + 1, &input, NULL, NULL, &timeout)) == -1) &&
	 (errno == EINTR) && (attempt++ < MAX_SELECT_ATTEMPTS));

  if (result < 0)
    {
      terrno = errno;
      PDBG (pixma_dbg (LOG_CRIT,
		       "bjnp_recv_header: could not read response header (select): %s!\n",
		       strerror (terrno)));
      errno = terrno;
      return SANE_STATUS_IO_ERROR;
    }
  else if (result == 0)
    {
      terrno = errno;
      PDBG (pixma_dbg (LOG_CRIT,
		       "bjnp_recv_header: could not read response header (select timed out): %s!\n",
		       strerror (terrno)));
      errno = terrno;
      return SANE_STATUS_IO_ERROR;
    }

  /* get response header */

  if ((recv_bytes =
       recv (fd, (char *) &resp_buf,
	     sizeof (struct BJNP_command),
	     0)) != sizeof (struct BJNP_command))
    {
      terrno = errno;
      PDBG (pixma_dbg (LOG_CRIT,
		       "bjnp_recv_header: (recv) could not read response header, received %d bytes!\n",
		       recv_bytes));
      PDBG (pixma_dbg
	    (LOG_CRIT, "bjnp_recv_header: (recv) error: %s!\n",
	     strerror (terrno)));
      errno = terrno;
      return SANE_STATUS_IO_ERROR;
    }

  if (resp_buf.cmd_code != device[devno].last_cmd)
    {
      PDBG (pixma_dbg
	    (LOG_CRIT,
	     "bjnp_recv_header:ERROR, Received response has cmd code %d, expected %d\n",
	     resp_buf.cmd_code, device[devno].last_cmd));
      return SANE_STATUS_IO_ERROR;
    }

  if (ntohs (resp_buf.seq_no) != (uint16_t) device[devno].serial)
    {
      PDBG (pixma_dbg
	    (LOG_CRIT,
	     "bjnp_recv_header:ERROR, Received response has serial %d, expected %d\n",
	     (int) ntohs (resp_buf.seq_no), (int) device[devno].serial));
      return SANE_STATUS_IO_ERROR;
    }

  /* got response header back, retrieve length of scanner data */


  device[devno].scanner_data_left = ntohl (resp_buf.payload_len);
  PDBG (pixma_dbg
	(LOG_DEBUG2, "TCP response header(scanner data = %ld bytes):\n",
	 (unsigned long) device[devno].scanner_data_left));
  PDBG (pixma_hexdump
	(LOG_DEBUG2, (char *) &resp_buf, sizeof (struct BJNP_command)));
  return SANE_STATUS_GOOD;
}

static SANE_Status
bjnp_recv_data (int devno, SANE_Byte * buffer, size_t * len)
{
/*
 * This function receives the responses to the write commands.
 * NOTE: len may not exceed SSIZE_MAX (as that is max for recv)
 * Returns: number of bytes of payload received from device
 */

  fd_set input;
  struct timeval timeout;
  ssize_t recv_bytes;
  int terrno;
  int result;
  int fd;
  int attempt;

  PDBG (pixma_dbg (LOG_DEBUG, "bjnp_recv_data: receiving response data\n"));
  fd = device[devno].tcp_socket;

  PDBG (pixma_dbg
	(LOG_DEBUG, "bjnp_recv_data: read response payload (%ld bytes max)\n",
	 (unsigned long) *len));

  attempt = 0;
  do
    {
      /* wait for data to be received, retry on a signal being received */
      FD_ZERO (&input);
      FD_SET (fd, &input);
      timeout.tv_sec = BJNP_TIMEOUT_TCP;
      timeout.tv_usec = 0;
    }
  while (((result = select (fd + 1, &input, NULL, NULL, &timeout)) == -1) &&
	 (errno == EINTR) && (attempt++ < MAX_SELECT_ATTEMPTS));

  if (result < 0)
    {
      terrno = errno;
      PDBG (pixma_dbg (LOG_CRIT,
		       "bjnp_recv_data: could not read response payload (select): %s!\n",
		       strerror (errno)));
      errno = terrno;
      *len = 0;
      return SANE_STATUS_IO_ERROR;
    }
  else if (result == 0)
    {
      terrno = errno;
      PDBG (pixma_dbg (LOG_CRIT,
		       "bjnp_recv_data: could not read response payload (select timed out): %s!\n",
		       strerror (terrno)));
      errno = terrno;
      *len = 0;
      return SANE_STATUS_IO_ERROR;
    }

  if ((recv_bytes = recv (fd, buffer, *len, 0)) < 0)
    {
      terrno = errno;
      PDBG (pixma_dbg (LOG_CRIT,
		       "bjnp_recv_data: could not read response payload (recv): %s!\n",
		       strerror (errno)));
      errno = terrno;
      *len = 0;
      return SANE_STATUS_IO_ERROR;
    }
  PDBG (pixma_dbg (LOG_DEBUG2, "Received TCP response payload (%ld bytes):\n",
		   (unsigned long) recv_bytes));
  PDBG (pixma_hexdump (LOG_DEBUG2, buffer, recv_bytes));

  device[devno].scanner_data_left =
    device[devno].scanner_data_left - recv_bytes;

  *len = recv_bytes;
  return SANE_STATUS_GOOD;
}

static BJNP_Status
bjnp_allocate_device (SANE_String_Const devname, SANE_Int * dn,
		      char *res_host)
{
  char method[BJNP_METHOD_MAX]; 
  char host[BJNP_HOST_MAX];
  char port[BJNP_PORT_MAX] = "";
  char args[BJNP_ARGS_MAX];
  struct addrinfo *res, *cur;
  int result;
  int i;

  PDBG (pixma_dbg (LOG_DEBUG, "bjnp_allocate_device(%s) %d\n", devname, bjnp_no_devices));

  if (split_uri (devname, method, host, port, args) != 0)
    {
      return BJNP_STATUS_INVAL;
    }

  if (strlen (args) != 0)
    {
      PDBG (pixma_dbg
	    (LOG_CRIT,
	     "URI may not contain userid, password or aguments: %s\n",
	     devname));

      return BJNP_STATUS_INVAL;
    }
  if (strcmp (method, BJNP_METHOD) != 0)
    {
      PDBG (pixma_dbg
	    (LOG_CRIT, "URI %s contains invalid method: %s\n", devname,
	     method));
      return BJNP_STATUS_INVAL;
    }

  if (strlen(port) == 0)
    {
      sprintf( port, "%d", BJNP_PORT_SCAN );
    }

  result = getaddrinfo (host, port, NULL, &res );
  if (result != 0 ) 
    {
      PDBG (pixma_dbg (LOG_CRIT, "Cannot resolve host: %s\n", host));
      return SANE_STATUS_INVAL;
    }

  /* Check if a device number is already allocated to any of the scanner's addresses */

  for (i = 0; i < bjnp_no_devices; i++)
    {
      cur = res;
      while( cur != NULL)
        {
          struct sockaddr * dev_addr = device[i].addr;
          if (dev_addr->sa_family == cur -> ai_family)
            {
              if( cur -> ai_family == AF_INET)
                {
                  /* TODO: can we use memcmp? Or dow we have issues with zero field */
                  struct sockaddr_in *dev_addr4 = (struct sockaddr_in *) dev_addr;
                  struct sockaddr_in *res_addr4 = (struct sockaddr_in *) cur->ai_addr;
                  if ( (dev_addr4->sin_port == res_addr4->sin_port) &&
                      (dev_addr4->sin_addr.s_addr == res_addr4->sin_addr.s_addr))
                    {
                    *dn = i;
                    freeaddrinfo(res);
                    return BJNP_STATUS_ALREADY_ALLOCATED;
                    }
                } else if (cur -> ai_family == AF_INET6 )
                {
                  struct sockaddr_in6 *dev_addr6 = ( struct sockaddr_in6 *)  dev_addr;
                  struct sockaddr_in6 *res_addr6 = ( struct sockaddr_in6 *) cur->ai_addr;
                  if ( (dev_addr6->sin6_port == res_addr6->sin6_port) &&
                      (memcmp(&(dev_addr6->sin6_addr), &(res_addr6->sin6_addr), sizeof(struct sockaddr_in6)) == 0))
                    {
                    *dn = i;
                    freeaddrinfo(res);
                    return BJNP_STATUS_ALREADY_ALLOCATED;
                    }

                }
            }
          cur = cur->ai_next;
        }
    }
  PDBG (pixma_dbg (LOG_INFO, "Scanner not found, adding it: %s:%s\n", host, port));
  sleep(4);

  /* return hostname if required */

  if (res_host != NULL)
    strcpy (res_host, host);

  /*
   * No existing device structure found, fill new device structure
   */

  if (bjnp_no_devices == BJNP_NO_DEVICES)
    {
      PDBG (pixma_dbg
	    (LOG_CRIT,
	     "Too many devices, ran out of device structures, can not add %s\n",
	     devname));
      freeaddrinfo(res);
      return BJNP_STATUS_INVAL;
    }
  *dn = bjnp_no_devices;
  bjnp_no_devices++;
  device[*dn].open = 1;
  device[*dn].active = 0;
#ifdef PIXMA_BJNP_USE_STATUS
  device[*dn].polling_status = BJNP_POLL_STOPPED; 
  device[*dn].dialog = 0;
  device[*dn].status_key = 0;
#endif
  device[*dn].tcp_socket = -1;

  /* both a struct sockaddr_in and a sockaddr_in6 fir in sovkaddr_storage */
  device[*dn].addr = (struct sockaddr *) malloc(sizeof ( struct sockaddr_storage) );
  
  memcpy(device[*dn].addr, res-> ai_addr, sizeof(struct sockaddr_in6) );
  
  device[*dn].session_id = 0;
  device[*dn].serial = -1;
  device[*dn].bjnp_timeout = 0;
  device[*dn].scanner_data_left = 0;
  device[*dn].last_cmd = 0;

  /* we make a pessimistic guess on blocksize, will be corrected to max size
   * of received block when we read data  */

  device[*dn].blocksize = 1024;
  device[*dn].short_read = 0;
   
  freeaddrinfo(res);
  if (bjnp_setup_udp_socket(*dn) == -1 )
    {
      bjnp_no_devices--;
      free(device[*dn].addr);
      return  BJNP_STATUS_INVAL;
    }
  return BJNP_STATUS_GOOD;
}

static void add_scanner(SANE_Int *dev_no, 
                        const char *uri, 
			SANE_Status (*attach_bjnp)
			              (SANE_String_Const devname,
			               SANE_String_Const makemodel,
			               SANE_String_Const serial,
			               const struct pixma_config_t *
			               const pixma_devices[]),
			 const struct pixma_config_t *const pixma_devices[])

{
  char scanner_host[BJNP_HOST_MAX];
  char short_host[SHORT_HOSTNAME_MAX];
  char makemodel[BJNP_IEEE1284_MAX];

  /* Allocate device structure for scanner and read its model */
  switch (bjnp_allocate_device (uri, dev_no, scanner_host))
    {
      case BJNP_STATUS_GOOD:
        if (get_scanner_id (*dev_no, makemodel) != 0)
          {
            PDBG (pixma_dbg (LOG_CRIT, "Cannot read scanner make & model: %s\n", 
                             uri));
          }
        else
          {
          /*
           * inform caller of found scanner
           */

           truncate_hostname (scanner_host, short_host);
           attach_bjnp (uri, makemodel,
                        short_host, pixma_devices);
          }
        break;
      case BJNP_STATUS_ALREADY_ALLOCATED:
        PDBG (pixma_dbg (LOG_NOTICE, "Scanner at %s was added before, good!\n",
	                 uri));
        break;

      case BJNP_STATUS_INVAL:
        PDBG (pixma_dbg (LOG_NOTICE, "Scanner at %s can not be added\n",
	                 uri));
        break;
    }
}


/*
 * Public functions
 */

/** Initialize sanei_bjnp.
 *
 * Call this before any other sanei_bjnp function.
 */
extern void
sanei_bjnp_init (void)
{
  bjnp_no_devices = 0;
}

/** 
 * Find devices that implement the bjnp protocol
 *
 * The function attach is called for every device which has been found.
 *
 * @param attach attach function
 *
 * @return SANE_STATUS_GOOD - on success (even if no scanner was found)
 */
extern SANE_Status
sanei_bjnp_find_devices (const char **conf_devices,
			 SANE_Status (*attach_bjnp)
			 (SANE_String_Const devname,
			  SANE_String_Const makemodel,
			  SANE_String_Const serial,
			  const struct pixma_config_t *
			  const pixma_devices[]),
			 const struct pixma_config_t *const pixma_devices[])
{
  int numbytes = 0;
  struct BJNP_command cmd;
  char resp_buf[2048];
  int socket_fd[BJNP_SOCK_MAX];
  int no_sockets;
  int i;
  int attempt;
  int last_socketfd = 0;
  fd_set fdset;
  fd_set active_fdset;
  struct timeval timeout;
  char scanner_host[256]; 
  char uri[256];
  int dev_no;
  /* char serial[13]; */
  struct in_addr broadcast_addr[BJNP_SOCK_MAX];
  struct sockaddr_storage scanner_sa; 
  socklen_t socklen;
#ifdef HAVE_IFADDRS_H
  struct ifaddrs *interfaces = NULL;
  struct ifaddrs *interface;
#else
  struct in_addr local;
#endif

  PDBG (pixma_dbg (LOG_INFO, "sanei_bjnp_find_devices:\n"));
  bjnp_no_devices = 0;

  for (i=0; i < BJNP_SOCK_MAX; i++)
    {
      socket_fd[i] = -1;
    }
  /* First add devices from config file */

  if (conf_devices[0] == NULL)
    PDBG (pixma_dbg( LOG_DEBUG, "No devices specified in configuration file.\n" ) );

  for (i = 0; conf_devices[i] != NULL; i++)
    {
      PDBG (pixma_dbg
	    (LOG_DEBUG, "Adding scanner from pixma.conf: %s\n", conf_devices[i]));
      add_scanner(&dev_no, conf_devices[i], attach_bjnp, pixma_devices);
    }
  PDBG (pixma_dbg
	(LOG_DEBUG,
	 "Added all configured scanners, now do auto detection...\n"));

  /*
   * Send UDP DISCOVER to discover scanners and return the list of scanners found
   */

  FD_ZERO (&fdset);
  set_cmd (-1, &cmd, CMD_UDP_DISCOVER, 0);

#ifdef HAVE_IFADDRS_H
  no_sockets = 0;
  getifaddrs (&interfaces);

  /* create a socket for each suitable interface */

  interface = interfaces;
  while ((no_sockets < BJNP_SOCK_MAX) && (interface != NULL))
    {
      if ( ! (interface -> ifa_flags & IFF_POINTOPOINT) &&  
          ( (socket_fd[no_sockets] = prepare_socket( interface -> ifa_name, 
             interface -> ifa_addr, interface -> ifa_broadaddr) ) != -1 ))
        {
          broadcast_addr[no_sockets] = ((struct sockaddr_in *)
                                        interface->ifa_broadaddr)->sin_addr;
          /* track highest used socket for later use in select */
          if (socket_fd[no_sockets] > last_socketfd)
            {
              last_socketfd = socket_fd[no_sockets];
            }
          FD_SET (socket_fd[no_sockets], &fdset);
          no_sockets++;
        }
      interface = interface->ifa_next;
    }  
  freeifaddrs (interfaces);

#else
  /* we have no easy way to find interfaces with their broadcast addresses. */
  /* use global broadcast and all-hosts instead */

  local.s_addr = htonl (INADDR_ANY);
  broadcast_addr[0].s_addr = htonl (INADDR_BROADCAST);
  socket_fd[0] = create_broadcast_socket( local, BJNP_PORT_SCAN);
  FD_SET (socket_fd[0], &fdset);

  /* we can also try all hosts: 224.0.0.1 */
  broadcast_addr[1].s_addr = htonl (INADDR_ALLHOSTS_GROUP);
  socket_fd[1] = create_broadcast_socket( local, BJNP_PORT_SCAN);
  FD_SET (socket_fd[1], &fdset);

  no_sockets = 2;
  last_socketfd = `socket_fd[0] > socket_fd(1) ? socket_fd(0) : socket_fd(1);

#endif

  /* send MAX_SELECT_ATTEMPTS broadcasts on each socket */
  for (attempt = 0; attempt < MAX_SELECT_ATTEMPTS; attempt++)
    {
      for ( i=0; i < no_sockets; i++)
        {
          bjnp_send_broadcast ( socket_fd[i], broadcast_addr[i], cmd, sizeof (cmd));
	}
      /* wait for some time between broadcast packets */
      usleep (BJNP_BROADCAST_INTERVAL * USLEEP_MS);
    }

  /* wait for a UDP response */

  timeout.tv_sec = 0;
  timeout.tv_usec = 500 * USLEEP_MS;

  active_fdset = fdset;

  while (select (last_socketfd + 1, &active_fdset, NULL, NULL, &timeout) > 0)
    {
      PDBG (pixma_dbg (LOG_DEBUG, "Select returned, time left %d.%d....\n",
		       (int) timeout.tv_sec, (int) timeout.tv_usec));
      for (i = 0; i < no_sockets; i++)
	{
	  if (FD_ISSET (socket_fd[i], &active_fdset))
	    {
              socklen =  sizeof(scanner_sa);
	      if ((numbytes =
		   recvfrom (socket_fd[i], resp_buf, sizeof (resp_buf), 0, 
                             (struct sockaddr *) &scanner_sa, &socklen ) ) == -1)
		{
		  PDBG (pixma_dbg
			(LOG_INFO, "bjnp_send_broadcasts: no data received"));
		  break;
		}
	      else
		{
		  PDBG (pixma_dbg (LOG_DEBUG2, "Discover response:\n"));
		  PDBG (pixma_hexdump (LOG_DEBUG2, &resp_buf, numbytes));

		  /* check if something sensible is returned */
		  if ((numbytes != sizeof (struct DISCOVER_RESPONSE))
		      || (strncmp ("BJNP", resp_buf, 4) != 0))
		    {
		      /* not a valid response, assume not a scanner  */
		      break;
		    }
		};

	      /* scanner found, get IP-address and hostname */
              get_scanner_name( (struct sockaddr *) &scanner_sa, scanner_host);

	      /* construct URI */
	      sprintf (uri, "%s://%s:%d", BJNP_METHOD, scanner_host,
		       BJNP_PORT_SCAN);

              add_scanner( &dev_no, uri, attach_bjnp, pixma_devices); 

	    }
	}
      active_fdset = fdset;
      timeout.tv_sec = 0;
      timeout.tv_usec = 2 * BJNP_BROADCAST_INTERVAL * USLEEP_MS;
    }
  PDBG (pixma_dbg (LOG_DEBUG, "scanner discovery finished...\n"));

  for (i = 0; i < no_sockets; i++)
    close (socket_fd[i]);

  return SANE_STATUS_GOOD;
}

/** Open a BJNP device.
 *
 * The device is opened by its name devname and the device number is
 * returned in dn on success.  
 *
 * Device names consist of an URI                                     
 * Where:
 * type = bjnp
 * hostname = resolvable name or IP-address
 * port = 8612 for a scanner
 * An example could look like this: bjnp://host.domain:8612
 *
 * @param devname name of the device to open
 * @param dn device number
 *
 * @return
 * - SANE_STATUS_GOOD - on success
 * - SANE_STATUS_ACCESS_DENIED - if the file couldn't be accessed due to
 *   permissions
 * - SANE_STATUS_INVAL - on every other error
 */

extern SANE_Status
sanei_bjnp_open (SANE_String_Const devname, SANE_Int * dn)
{
  char pid_str[64];
  char my_hostname[256];
  char *login;
  int result;

  PDBG (pixma_dbg (LOG_INFO, "sanei_bjnp_open(%s, %d):\n", devname, *dn));

  result = bjnp_allocate_device (devname, dn, NULL);
  if ( (result != BJNP_STATUS_GOOD) && (result != BJNP_STATUS_ALREADY_ALLOCATED ) )
    return SANE_STATUS_INVAL; 

  login = getusername ();
  gethostname (my_hostname, 256);
  my_hostname[255] = '\0';
  sprintf (pid_str, "Process ID = %d", getpid ());

  bjnp_send_job_details (*dn, my_hostname, login, pid_str);

  if (bjnp_open_tcp (*dn) != 0)
    return SANE_STATUS_INVAL;

  return SANE_STATUS_GOOD;
}

/** Close a BJNP device.
 * 
 * @param dn device number
 */

void
sanei_bjnp_close (SANE_Int dn)
{
  PDBG (pixma_dbg (LOG_INFO, "sanei_bjnp_close(%d):\n", dn));
  if (device[dn].active)
    {
      sanei_bjnp_deactivate (dn);
    }
  /* we do not close the udp socket as long as the device remains allocated! */
  device[dn].open = 0;
  device[dn].active = 0;
}

/** Activate BJNP device connection
 *
 * @param dn device number
 */

SANE_Status
sanei_bjnp_activate (SANE_Int dn)
{
  char hostname[256];
  char pid_str[64];

  PDBG (pixma_dbg (LOG_INFO, "sanei_bjnp_activate (%d)\n", dn));
  gethostname (hostname, 256);
  hostname[255] = '\0';
  sprintf (pid_str, "Process ID = %d", getpid ());

  bjnp_send_job_details (dn, hostname, getusername (), pid_str);

  if (bjnp_open_tcp (dn) != 0)
    return SANE_STATUS_INVAL;

  return SANE_STATUS_GOOD;
}

/** Deactivate BJNP device connection
 *
 * @paran dn device number
 */

SANE_Status
sanei_bjnp_deactivate (SANE_Int dn)
{
  PDBG (pixma_dbg (LOG_INFO, "sanei_bjnp_deactivate (%d)\n", dn));
  bjnp_finish_job (dn);
  close (device[dn].tcp_socket);
  device[dn].tcp_socket = -1;
  return SANE_STATUS_GOOD;
}

/** Set the timeout for interrupt reads.
 *  we do not use it for bulk reads!
 * @param timeout the new timeout in ms
 */
extern void
sanei_bjnp_set_timeout (SANE_Int devno, SANE_Int timeout)
{
  PDBG (pixma_dbg (LOG_INFO, "bjnp_set_timeout to %d\n",
		   timeout));

  device[devno].bjnp_timeout = timeout;
}

/** Initiate a bulk transfer read.
 *
 * Read up to size bytes from the device to buffer. After the read, size
 * contains the number of bytes actually read.
 *
 * @param dn device number
 * @param buffer buffer to store read data in
 * @param size size of the data
 *
 * @return 
 * - SANE_STATUS_GOOD - on succes
 * - SANE_STATUS_EOF - if zero bytes have been read
 * - SANE_STATUS_IO_ERROR - if an error occured during the read
 * - SANE_STATUS_INVAL - on every other error
 *
 */

extern SANE_Status
sanei_bjnp_read_bulk (SANE_Int dn, SANE_Byte * buffer, size_t * size)
{
  SANE_Status result;
  SANE_Status error;
  size_t recvd;
  size_t more;
  size_t left;

  PDBG (pixma_dbg
	(LOG_INFO, "bjnp_read_bulk(%d, bufferptr, 0x%lx = %ld)\n", dn,
	 (unsigned long) *size, (unsigned long) size));

  recvd = 0;
  left = *size;

  if ((device[dn].scanner_data_left == 0) && (device[dn].short_read != 0))
    {
      /* new read, but we have no data queued from scanner, last read was short, */
      /* so scanner needs first a high level read command. This is not an error */

      PDBG (pixma_dbg
	    (LOG_DEBUG,
	     "Scanner has no more data available, return immediately!\n"));
      *size = 0;
      return SANE_STATUS_EOF;
    }

  PDBG (pixma_dbg
	(LOG_DEBUG, "bjnp_read_bulk: 0x%lx = %ld bytes available at start, "
	 "Short block = %d blocksize = 0x%lx = %ld\n",
	 (unsigned long) device[dn].scanner_data_left,
	 (unsigned long) device[dn].scanner_data_left,
	 (int) device[dn].short_read,
	 (unsigned long) device[dn].blocksize, 
	 (unsigned long) device[dn].blocksize));

  while ((recvd < *size)
	 && (!device[dn].short_read || device[dn].scanner_data_left))
    {
      PDBG (pixma_dbg
	    (LOG_DEBUG,
	     "So far received 0x%lx bytes = %ld, need 0x%lx = %ld\n",
	     (unsigned long) recvd, (unsigned long) recvd, 
	     (unsigned long) *size, (unsigned long) *size));

      if (device[dn].scanner_data_left == 0)
	{
	  /*
	   * send new read request
	   */

	  PDBG (pixma_dbg
		(LOG_DEBUG,
		 "No (more) scanner data available, requesting more\n"));

	  if ((error = bjnp_send_read_request (dn)) != SANE_STATUS_GOOD)
	    {
	      *size = recvd;
	      return SANE_STATUS_IO_ERROR;
	    }
	  if ((error = bjnp_recv_header (dn)) != SANE_STATUS_GOOD)
	    {
	      *size = recvd;
	      return SANE_STATUS_IO_ERROR;
	    }
	  PDBG (pixma_dbg
		(LOG_DEBUG, "Scanner reports 0x%lx = %ld bytes available\n",
		 (unsigned long) device[dn].scanner_data_left,
		 (unsigned long) device[dn].scanner_data_left));

	  /* correct blocksize if more data is sent by scanner than current blocksize assumption */

	  if (device[dn].scanner_data_left > device[dn].blocksize)
	    device[dn].blocksize = device[dn].scanner_data_left;

	  /* decide if we reached end of data to be sent by scanner */

	  device[dn].short_read =
	    (device[dn].scanner_data_left < device[dn].blocksize);
	}

      more = left;

      PDBG (pixma_dbg
	    (LOG_DEBUG,
	     "reading 0x%lx = %ld (of max 0x%lx = %ld) bytes more\n",
	     (unsigned long) device[dn].scanner_data_left, 
	     (unsigned long) device[dn].scanner_data_left,
	     (unsigned long) more, 
	     (unsigned long) more));
      result = bjnp_recv_data (dn, buffer, &more);
      if (result != SANE_STATUS_GOOD)
	{
	  *size = recvd;
	  return SANE_STATUS_IO_ERROR;
	}

      left = left - more;
      recvd = recvd + more;
      buffer = buffer + more;
    }
  *size = recvd;
  return SANE_STATUS_GOOD;
}

/** Initiate a bulk transfer write.
 *
 * Write up to size bytes from buffer to the device. After the write size
 * contains the number of bytes actually written.
 *
 * @param dn device number
 * @param buffer buffer to write to device
 * @param size size of the data
 *
 * @return 
 * - SANE_STATUS_GOOD - on succes
 * - SANE_STATUS_IO_ERROR - if an error occured during the write
 * - SANE_STATUS_INVAL - on every other error
 */

extern SANE_Status
sanei_bjnp_write_bulk (SANE_Int dn, const SANE_Byte * buffer, size_t * size)
{
  ssize_t sent;
  size_t recvd;
  uint32_t buf;

  PDBG (pixma_dbg
	(LOG_INFO, "bjnp_write_bulk(%d, bufferptr, 0x%lx = %ld)\n", dn,
	 (unsigned long) *size, (unsigned long) *size));
  sent = bjnp_write (dn, buffer, *size);
  if (sent < 0)
    return SANE_STATUS_IO_ERROR;
  if (sent != (int) *size)
    {
      PDBG (pixma_dbg
	    (LOG_CRIT, "Sent only %ld bytes to scanner, expected %ld!!\n",
	     (unsigned long) sent, (unsigned long) *size));
      return SANE_STATUS_IO_ERROR;
    }

  if (bjnp_recv_header (dn) != SANE_STATUS_GOOD)
    {
      PDBG (pixma_dbg (LOG_CRIT, "Could not read response to command!\n"));
      return SANE_STATUS_IO_ERROR;
    }

  if (device[dn].scanner_data_left != 4)
    {
      PDBG (pixma_dbg (LOG_CRIT,
		       "Scanner length of write confirmation = 0x%lx bytes = %ld, expected %d!!\n",
		       (unsigned long) device[dn].scanner_data_left,
		       (unsigned long) device[dn].scanner_data_left, 4));
      return SANE_STATUS_IO_ERROR;
    }
  recvd = 4;
  if ((bjnp_recv_data (dn, (unsigned char *) &buf, &recvd) !=
       SANE_STATUS_GOOD) || (recvd != 4))
    {
      PDBG (pixma_dbg (LOG_CRIT,
		       "Could not read length of data confirmed by device\n"));
      return SANE_STATUS_IO_ERROR;
    }
  recvd = ntohl (buf);
  if (recvd != *size)
    {
      PDBG (pixma_dbg
	    (LOG_CRIT, "Scanner confirmed %ld bytes, expected %ld!!\n",
	     (unsigned long) recvd, (unsigned long) *size));
      return SANE_STATUS_IO_ERROR;
    }

  /* we sent a new command, so reset end of block indication */

  device[dn].short_read = 0;

  return SANE_STATUS_GOOD;
}

/** Initiate a interrupt transfer read.
 *
 * Read up to size bytes from the interrupt endpoint from the device to
 * buffer. After the read, size contains the number of bytes actually read.
 *
 * @param dn device number
 * @param buffer buffer to store read data in
 * @param size size of the data
 *
 * @return 
 * - SANE_STATUS_GOOD - on succes
 * - SANE_STATUS_EOF - if zero bytes have been read
 * - SANE_STATUS_IO_ERROR - if an error occured during the read
 * - SANE_STATUS_INVAL - on every other error
 *
 */

extern SANE_Status
sanei_bjnp_read_int (SANE_Int dn, SANE_Byte * buffer, size_t * size)
{
#ifndef PIXMA_BJNP_USE_STATUS
  PDBG (pixma_dbg
	(LOG_INFO, "bjnp_read_int(%d, bufferptr, 0x%lx = %ld):\n", dn,
	 (unsigned long) *size, (unsigned long) *size));

  memset (buffer, 0, *size);
  sleep (1);
  return SANE_STATUS_IO_ERROR;
#else

  char hostname[256];
  int resp_len;
  int timeout;
  int seconds;
  
  PDBG (pixma_dbg
	(LOG_INFO, "bjnp_read_int(%d, bufferptr, 0x%lx = %ld):\n", dn,
	 (unsigned long) *size, (unsigned long) *size));

  memset (buffer, 0, *size);

  gethostname (hostname, 32);
  hostname[32] = '\0';


  switch (device[dn].polling_status)
    {
    case BJNP_POLL_STOPPED:

      /* establish dialog */

      if ( (bjnp_poll_scanner (dn, 0, hostname, getusername (), buffer, *size ) != 0) ||
           (bjnp_poll_scanner (dn, 1, hostname, getusername (), buffer, *size ) != 0) )
        {
	  PDBG (pixma_dbg (LOG_NOTICE, "Failed to setup read_intr dialog with device!\n"));
          device[dn].dialog = 0;
          device[dn].status_key = 0;
          return SANE_STATUS_IO_ERROR;
        }
      device[dn].polling_status = BJNP_POLL_STARTED;

    case BJNP_POLL_STARTED:
      /* we use only seonds accuracy between poll attempts */
      timeout = device[dn].bjnp_timeout /1000;
     
      do
        {
          if ( (resp_len = bjnp_poll_scanner (dn, 2, hostname, getusername (), buffer, *size ) ) < 0 )
            {
              PDBG (pixma_dbg (LOG_NOTICE, "Restarting polling dialog!\n"));
              device[dn].polling_status = BJNP_POLL_STOPPED;
              *size = 0;
              return SANE_STATUS_EOF;
            }
          *size = (size_t) resp_len;
          if ( resp_len > 0 )
            {
              device[dn].polling_status = BJNP_POLL_STATUS_RECEIVED;

              /* this is a bit of a hack, but the scanner does not like */
              /* us to continue using the existing tcp socket */

              sanei_bjnp_deactivate(dn);
              sanei_bjnp_activate(dn);

              return SANE_STATUS_GOOD;
            }
          seconds = timeout > 2 ? 2 : timeout;
          sleep(seconds);
          timeout = timeout - seconds;
        } while ( timeout > 0 ) ;
      break;
    case BJNP_POLL_STATUS_RECEIVED:
       if ( (resp_len = bjnp_poll_scanner (dn, 5, hostname, getusername (), buffer, *size ) ) < 0 )
        {
          PDBG (pixma_dbg (LOG_NOTICE, "Restarting polling dialog!\n"));
          device[dn].polling_status = BJNP_POLL_STOPPED;
          *size = 0;
          break;
        }
    }
  return SANE_STATUS_EOF;
#endif
}
