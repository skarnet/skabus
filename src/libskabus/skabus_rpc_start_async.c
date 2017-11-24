/* ISC license. */

#include <skalibs/skaclient.h>
#include <skabus/rpc.h>

int skabus_rpc_start_async (skabus_rpc_t *a, char const *path, skabus_rpc_start_result_t *data)
{
  return skaclient_start_async_b(
    &a->connection,
    &a->buffers,
    path,
    SKACLIENT_OPTION_ASYNC_ACCEPT_FDS,
    SKABUS_RPC_BANNER1,
    SKABUS_RPC_BANNER1_LEN,
    SKABUS_RPC_BANNER2,
    SKABUS_RPC_BANNER2_LEN,
    &data->skaclient_cbdata) ;
}
