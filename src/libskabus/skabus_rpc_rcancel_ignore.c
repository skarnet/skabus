/* ISC license. */

#include <skabus/rpc.h>

int skabus_rpc_rcancel_ignore (uint64_t serial, char reason, void *data)
{
  (void)serial ;
  (void)reason ;
  (void)data ;
  return 1 ;
}
