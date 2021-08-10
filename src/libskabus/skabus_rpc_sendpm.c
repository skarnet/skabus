/* ISC license. */

#include <skabus/rpc.h>
#include "skabus-rpc-internal.h"

uint64_t skabus_rpc_sendpm_withfds (skabus_rpc_t *a, char const *cname, char const *s, size_t len, int const *fds, unsigned int nfds, unsigned char const *bits, tain const *limit, tain const *deadline, tain *stamp)
{
  return skabus_rpc_sendq_withfds(a, "\xff", 1, cname, s, len, fds, nfds, bits, limit, deadline, stamp) ;
}
