/* ISC license. */

#include <errno.h>

#include <skabus/rpc.h>

int skabus_rpc_r_notimpl (skabus_rpc_t *a, skabus_rpc_rinfo_t const *info, unixmessage_t const *m, void *data)
{
  (void)m ;
  (void)data ;
  return skabus_rpc_reply(a, info->serial, ENOSYS, "", 0, 0, 0) ;
}
