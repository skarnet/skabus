/* ISC license. */

#include <skabus/rpc.h>
#include "skabus-rpc-internal.h"

int skabus_rpc_sendvpm_withfds_async (skabus_rpc_t *a, char const *cname, struct iovec const *v, unsigned int vlen, int const *fds, unsigned int nfds, unsigned char const *bits, tain const *limit, skabus_rpc_send_result_t *r)
{
  return skabus_rpc_sendvq_withfds_async(a, "\xff", 1, cname, v, vlen, fds, nfds, bits, limit, r) ;
}
