#include "../include/sane/config.h"

#ifndef HAVE_INET_NTOP

#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

const char *
inet_ntop (int af, const void *src, char *dst, size_t cnt)
{
  struct in_addr in;
  char *text_addr;

  if (af == AF_INET)
    {
      memcpy (&in.s_addr, src, sizeof (in.s_addr));
      text_addr = inet_ntoa (in);
      if (text_addr && dst)
	{
	  strncpy (dst, text_addr, cnt);
	  return dst;
	}
      else
	return 0;
    }
  return 0;
}

#endif /* !HAVE_INET_NTOP */
