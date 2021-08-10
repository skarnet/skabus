 /* ISC license. */

#include <sys/uio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>

#include <skalibs/posixishard.h>
#include <skalibs/uint32.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/socket.h>
#include <skalibs/textmessage.h>

#include <skabus/rpc.h>
#include "skabus-rpccctl.h"

int skabus_rpcc_start (skabus_rpcc_t *a, char const *path, tain const *deadline, tain *stamp)
{
  int fd = ipc_stream_nb() ;
  if (fd < 0) return 0 ;
  if (!ipc_timed_connect(fd, path, deadline, stamp))
  {
    fd_close(fd) ;
    return 0 ;
  }
  textmessage_sender_init(&a->out, fd) ;
  textmessage_receiver_init(&a->in, fd) ;
  return 1 ;
}

void skabus_rpcc_end (skabus_rpcc_t *a)
{
  fd_close(textmessage_sender_fd(&a->out)) ;
  textmessage_sender_free(&a->out) ;
  textmessage_receiver_free(&a->in) ;
}

int skabus_rpcc_interface_register (skabus_rpcc_t *a, char const *ifname, char const *ifprog, char const *re, tain const *deadline, tain *stamp)
{
  size_t ifnamelen, ifproglen, relen ;
  char *ifprogfn = realpath(ifprog, 0) ;
  if (!ifprogfn) return 0 ;
  ifnamelen = strlen(ifname) ;
  ifproglen = strlen(ifprogfn) ;
  relen = strlen(re) ;
  if (ifnamelen > SKABUS_RPC_INTERFACE_MAXLEN || ifproglen > PATH_MAX || relen > SKABUS_RPC_RE_MAXLEN) goto terr ;
  {
    char buf[9] ;
    struct iovec v[5] =
    {
      { .iov_base = "I", .iov_len = 1 },
      { .iov_base = buf, .iov_len = 9 },
      { .iov_base = ifname, .iov_len = ifnamelen + 1 },
      { .iov_base = ifprogfn, .iov_len = ifproglen + 1 },
      { .iov_base = re, .iov_len = relen + 1 }
    } ;
    buf[0] = (unsigned char)ifnamelen ;
    uint32_pack_big(buf + 1, ifproglen) ;
    uint32_pack_big(buf + 5, relen) ;
    if (!textmessage_timed_commandv(&a->out, v, 5, deadline, stamp)) goto err ;
  }
  free(ifprogfn) ;
  return 1 ;

 terr:
  errno = ENAMETOOLONG ;
 err:
  free(ifprogfn) ;
  return 0 ;
}

int skabus_rpcc_interface_unregister (skabus_rpcc_t *a, char const *ifname, tain const *deadline, tain *stamp)
{
  size_t ifnamelen = strlen(ifname) ;
  if (ifnamelen > SKABUS_RPC_INTERFACE_MAXLEN) return (errno = ENAMETOOLONG, 0) ;
  {
    unsigned char c = ifnamelen ;
    struct iovec v[3] =
    {
      { .iov_base = "i", .iov_len = 1 },
      { .iov_base = &c, .iov_len = 1 },
      { .iov_base = ifname, .iov_len = ifnamelen + 1 }
    } ;
    if (!textmessage_timed_commandv(&a->out, v, 3, deadline, stamp)) return 0 ;
  }
  return 1 ;
}

int skabus_rpcc_query (skabus_rpcc_t *a, stralloc *reply, char const *ifname, char const *query, uint32_t timeout, tain const *deadline, tain *stamp)
{
  size_t ifnamelen = strlen(ifname) ;
  size_t querylen = strlen(query) ;
  if (ifnamelen > SKABUS_RPC_INTERFACE_MAXLEN || querylen > UINT32_MAX) return (errno = ENAMETOOLONG, 0) ;
  {
    char buf[9] ;
    struct iovec v[4] =
    {
      { .iov_base = "Q", .iov_len = 1 },
      { .iov_base = buf, .iov_len = 59 },
      { .iov_base = ifname, .iov_len = ifnamelen + 1 },
      { .iov_base = query, .iov_len = querylen + 1 },
    } ;
    buf[0] = ifnamelen ;
    uint32_pack_big(buf + 1, querylen) ;
    uint32_pack_big(buf + 5, timeout) ;
    if (!textmessage_timed_sendv(&a->out, v, 4)) return 0 ;
  }
  {
    struct iovec v ;
    if (!textmessage_timed_receive(&a->in, &v, deadline, stamp)) return 0 ;
    if (!v.iov_len) return (errno = EPROTO, 0) ;
    if (*(unsigned char *)v.iov_base) return (errno = *(unsigned char)v.iov_base, 0) ;
    if (!stralloc_catb(reply, (char *)v.iov_base + 1, v.iov_len - 1)) return 0 ;
  }
  return 1 ;
}

int skabus_rpcc_quit (skabus_rpcc_t *a, tain const *deadline, tain *stamp)
{
  return textmessage_timed_command(&a->out, ".", 1, deadline, stamp) ; 
}
