/* ISC license. */

#include <skabus/rpc.h>
#include "skabus-rpc-internal.h"

uint64_t skabus_rpc_sendvpm_withfds (skabus_rpc_t *a, char const *cname, struct iovec const *v, unsigned int vlen, int const *fds, unsigned int nfds, unsigned char const *bits, tain const *limit, tain const *deadline, tain *stamp)
{
  return skabus_rpc_sendvq_withfds(a, "\xff", 1, cname, v, vlen, fds, nfds, bits, limit, deadline, stamp) ;
}
