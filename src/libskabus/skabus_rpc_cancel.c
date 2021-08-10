/* ISC license. */

#include <errno.h>

#include <skalibs/tai.h>
#include <skalibs/skaclient.h>

#include <skabus/rpc.h>

int skabus_rpc_cancel (skabus_rpc_t *a, uint64_t serial, tain const *deadline, tain *stamp)
{
  unsigned char r ;
  if (!skabus_rpc_cancel_async(a, serial, &r)) return 0 ;
  if (!skaclient_syncify(&a->connection, deadline, stamp)) return 0 ;
  if (r) return (errno = r, 0) ;
  return 1 ;
}
