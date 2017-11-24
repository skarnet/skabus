/* ISC license. */

#include <skabus/rpc.h>
#include "skabus-rpc-internal.h"

int skabus_rpc_sendv_withfds_async (skabus_rpc_t *a, char const *ifname, struct iovec const *v, unsigned int vlen, int const *fds, unsigned int nfds, unsigned char const *bits, tain_t const *limit, skabus_rpc_send_result_t *r)
{
  return skabus_rpc_sendvq_withfds_async(a, 0, 0, ifname, v, vlen, fds, nfds, bits, limit, r) ;
}
