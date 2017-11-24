/* ISC license. */

#include <skabus/rpc.h>
#include "skabus-rpc-internal.h"

uint64_t skabus_rpc_sendv_withfds (skabus_rpc_t *a, char const *ifname, struct iovec const *v, unsigned int vlen, int const *fds, unsigned int nfds, unsigned char const *bits, tain_t const *limit, tain_t const *deadline, tain_t *stamp)
{
  return skabus_rpc_sendvq_withfds(a, 0, 0, ifname, v, vlen, fds, nfds, bits, limit, deadline, stamp) ;
}
