/* ISC license. */

#include <skalibs/uint64.h>
#include <skalibs/genalloc.h>
#include <skabus/rpc.h>

size_t skabus_rpc_qlist (skabus_rpc_t *a, uint64_t **list)
{
  uint64_t n = genalloc_len(uint64_t, &a->qlist) ;
  if (n) *list = genalloc_s(uint64_t, &a->qlist) ;
  return n ;
}
