/* ISC license. */

#include <string.h>

#include <skalibs/uint64.h>
#include <skalibs/genalloc.h>

#include <skabus/rpc.h>

void skabus_rpc_qlist_ack (skabus_rpc_t *a, size_t n)
{
  uint64_t len = genalloc_len(uint64_t, &a->qlist) ;
  uint64_t *p = genalloc_s(uint64_t, &a->qlist) ;
  if (n > len) n = len ;
  memmove(p, p + n * sizeof(uint64_t), (len - n) * sizeof(uint64_t)) ;
  genalloc_setlen(uint64_t, &a->qlist, len - n) ;
}
