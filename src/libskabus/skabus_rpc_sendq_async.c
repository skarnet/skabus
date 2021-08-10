/* ISC license. */

#include <string.h>
#include <sys/uio.h>
#include <errno.h>

#include <skalibs/tai.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/unixmessage.h>
#include <skalibs/skaclient.h>

#include <skabus/rpc.h>
#include "skabus-rpc-internal.h"

int skabus_rpc_sendq_withfds_async (skabus_rpc_t *a, char const *prefix, size_t plen, char const *ifname, char const *s, size_t len, int const *fds, unsigned int nfds, unsigned char const *bits, tain const *limit, skabus_rpc_send_result_t *r)
{
  size_t iflen = strlen(ifname) ;
  char pack[2 + TAIN_PACK] = "Q" ;
  struct iovec v[4] = { { .iov_base = pack, .iov_len = 2 + TAIN_PACK }, { .iov_base = (char *)prefix, .iov_len = plen }, { .iov_base = (char *)ifname, .iov_len = iflen + 1 }, { .iov_base = (char *)s, .iov_len = len } } ;
  unixmessagev m = { .v = v, .vlen = 4, .fds = (int *)fds, .nfds = nfds } ;
  iflen += plen ;
  if (iflen > SKABUS_RPC_INTERFACE_MAXLEN) return (errno = ENAMETOOLONG, 0) ;
  if (!gensetdyn_new(&a->q, &r->i)) return 0 ;
  r->a = a ;
  if (limit) tain_pack(pack + 1, limit) ; else memset(pack + 1, 0, TAIN_PACK) ;
  pack[1 + TAIN_PACK] = (unsigned char)iflen ;
  if (!skaclient_putmsgv_and_close(&a->connection, &m, bits, &skabus_rpc_send_cb, r))
  {
    int e = errno ;
    gensetdyn_delete(&a->q, r->i) ;
    errno = e ;
    return 0 ;
  }
  GENSETDYN_P(skabus_rpc_qinfo_t, &a->q, r->i)->status = EINPROGRESS ;
  return 1 ;
}
