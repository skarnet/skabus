/* ISC license. */

#include <errno.h>

#include <skalibs/tai.h>
#include <skalibs/skaclient.h>

#include <skabus/rpc.h>

int skabus_rpc_interface_unregister (skabus_rpc_t *a, uint32_t ifid, tain_t const *deadline, tain_t *stamp)
{
  skabus_rpc_interface_result_t r ;
  if (!skabus_rpc_interface_unregister_async(a, ifid, &r)) return 0 ;
  if (!skaclient_syncify(&a->connection, deadline, stamp)) return 0 ;
  if (r.err) return (errno = r.err, 0) ;
  return 1 ;
}
