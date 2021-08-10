/* ISC license. */

#include <skabus/rpc.h>
#include "skabus-rpc-internal.h"

int skabus_rpc_send_withfds_async (skabus_rpc_t *a, char const *ifname, char const *s, size_t len, int const *fds, unsigned int nfds, unsigned char const *bits, tain const *limit, skabus_rpc_send_result_t *r)
{
  return skabus_rpc_sendq_withfds_async(a, 0, 0, ifname, s, len, fds, nfds, bits, limit, r) ;
}
