/* ISC license. */

#include <string.h>
#include <sys/uio.h>
#include <errno.h>
#include <skalibs/uint32.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/skaclient.h>
#include <skabus/rpc.h>
#include "skabus-rpc-internal.h"

int skabus_rpc_idstr_async (skabus_rpc_t *a, char const *idstr, skabus_rpc_interface_t const *ifbody, char const *re, skabus_rpc_interface_result_t *result)
{
  size_t idlen = strlen(idstr) ;
  size_t relen = strlen(re) ;
  skabus_rpc_ifnode_t *ifnode ;
  char pack[10] = "S" ;
  struct iovec v[3] = { { .iov_base = pack, .iov_len = 10 }, { .iov_base = (char *)idstr, .iov_len = idlen+1 }, { .iov_base = (char *)re, .iov_len = relen+1 } } ;
  if (idlen > SKABUS_RPC_IDSTR_SIZE) return (errno = ENAMETOOLONG, 0) ;
  if (relen > 0x6ffffffe) return (errno = ENAMETOOLONG, 0) ;
  if (!gensetdyn_new(&a->r, &a->pmid)) return 0 ;
  result->ifid = a->pmid ;
  result->r = &a->r ;
  ifnode = GENSETDYN_P(skabus_rpc_ifnode_t, &a->r, a->pmid) ;
  ifnode->name[0] = 0 ;
  ifnode->body = *ifbody ;
  uint32_pack_big(pack+1, a->pmid) ;
  pack[5] = (unsigned char)idlen ;
  uint32_pack_big(pack+6, relen) ;
  return skaclient_putv(&a->connection, v, 3, &skabus_rpc_interface_register_cb, result) ;
}
