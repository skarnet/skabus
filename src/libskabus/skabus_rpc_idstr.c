/* ISC license. */

#include <errno.h>
#include <skalibs/tai.h>
#include <skalibs/skaclient.h>
#include <skabus/rpc.h>

int skabus_rpc_idstr (skabus_rpc_t *a, char const *idstr, skabus_rpc_interface_t const *ifbody, char const *re, tain_t const *deadline, tain_t *stamp)
{
  skabus_rpc_interface_result_t r ;
  if (!skabus_rpc_idstr_async(a, idstr, ifbody, re, &r)) return 0 ;
  if (!skaclient_syncify(&a->connection, deadline, stamp)) return 0 ;
  return r.err ? (errno = r.err, 0) : 1 ;
}
