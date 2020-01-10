/* ISC license. */

#include <skalibs/skaclient.h>

#include <skabus/rpc.h>
#include "skabus-rpc-internal.h"

uint64_t skabus_rpc_sendvq_withfds (skabus_rpc_t *a, char const *prefix, size_t plen, char const *ifname, struct iovec const *v, unsigned int vlen, int const *fds, unsigned int nfds, unsigned char const *bits, tain_t const *limit, tain_t const *deadline, tain_t *stamp)
{
  skabus_rpc_send_result_t r ;
  if (!skabus_rpc_sendvq_withfds_async(a, prefix, plen, ifname, v, vlen, fds, nfds, bits, limit, &r)) return 0 ;
  if (!skaclient_syncify(&a->connection, deadline, stamp)) return 0 ;
  return r.err ? (errno = r.err, 0) : r.u ;
}
