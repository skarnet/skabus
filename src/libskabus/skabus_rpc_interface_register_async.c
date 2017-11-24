/* ISC license. */

#include <string.h>
#include <sys/uio.h>
#include <errno.h>
#include <skalibs/uint32.h>
#include <skalibs/error.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/skaclient.h>
#include <skabus/rpc.h>
#include "skabus-rpc-internal.h"

int skabus_rpc_interface_register_async (skabus_rpc_t *a, char const *ifname, skabus_rpc_interface_t const *ifbody, char const *re, skabus_rpc_interface_result_t *result)
{
  size_t ifnamelen = strlen(ifname) ;
  size_t relen = strlen(re) ;
  skabus_rpc_ifnode_t *ifnode ;
  char pack[10] = "I" ;
  struct iovec v[3] = { { .iov_base = pack, .iov_len = 10 }, { .iov_base = (char *)ifname, .iov_len = ifnamelen + 1 }, { .iov_base = (char *)re, .iov_len = relen + 1 } } ;
  if (ifnamelen > SKABUS_RPC_INTERFACE_MAXLEN) return (errno = ENAMETOOLONG, 0) ;
  if (!gensetdyn_new(&a->r, &result->ifid)) return 0 ;
  result->r = &a->r ;
  ifnode = GENSETDYN_P(skabus_rpc_ifnode_t, &a->r, result->ifid) ;
  memcpy(ifnode->name, ifname, ifnamelen + 1) ;
  ifnode->body = *ifbody ;
  uint32_pack_big(pack+1, result->ifid) ;
  pack[5] = (unsigned char)ifnamelen ;
  uint32_pack_big(pack+6, relen) ;
  return skaclient_putv(&a->connection, v, 3, &skabus_rpc_interface_register_cb, result) ;
}
