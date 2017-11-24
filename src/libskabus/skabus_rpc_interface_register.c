/* ISC license. */

#include <errno.h>
#include <skalibs/tai.h>
#include <skalibs/skaclient.h>
#include <skabus/rpc.h>

int skabus_rpc_interface_register (skabus_rpc_t *a, uint32_t *ifid, char const *ifname, skabus_rpc_interface_t const *ifbody, char const *re, tain_t const *deadline, tain_t *stamp)
{
  skabus_rpc_interface_result_t r ;
  if (!skabus_rpc_interface_register_async(a, ifname, ifbody, re, &r)) return 0 ;
  if (!skaclient_syncify(&a->connection, deadline, stamp)) return 0 ;
  if (r.err) return (errno = r.err, 0) ;
  *ifid = r.ifid ;
  return 1 ;
}
