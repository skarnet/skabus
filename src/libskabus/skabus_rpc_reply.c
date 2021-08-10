/* ISC license. */

#include <skalibs/skaclient.h>

#include <skabus/rpc.h>

int skabus_rpc_reply_withfds (skabus_rpc_t *a, uint64_t serial, char result, char const *s, size_t len, int const *fds, unsigned int nfds, unsigned char const *bits, tain const *deadline, tain *stamp)
{
  return skabus_rpc_reply_withfds_async(a, serial, result, s, len, fds, nfds, bits)
   && skaclient_timed_aflush(&a->connection, deadline, stamp) ;
}
