/* ISC license. */

#include <skabus/rpc.h>
#include "skabus-rpc-internal.h"

uint64_t skabus_rpc_send_withfds (skabus_rpc_t *a, char const *ifname, char const *s, size_t len, int const *fds, unsigned int nfds, unsigned char const *bits, tain_t const *limit, tain_t const *deadline, tain_t *stamp)
{
  return skabus_rpc_sendq_withfds(a, 0, 0, ifname, s, len, fds, nfds, bits, limit, deadline, stamp) ;
}
