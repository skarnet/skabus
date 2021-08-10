/* ISC license. */

#include <skalibs/skaclient.h>
#include <skabus/rpc.h>

int skabus_rpc_replyv_withfds (skabus_rpc_t *a, uint64_t serial, char result, struct iovec const *v, unsigned int vlen, int const *fds, unsigned int nfds, unsigned char const *bits, tain const *deadline, tain *stamp)
{
  return skabus_rpc_replyv_withfds_async(a, serial, result, v, vlen, fds, nfds, bits)
   && skaclient_timed_aflush(&a->connection, deadline, stamp) ;
}
