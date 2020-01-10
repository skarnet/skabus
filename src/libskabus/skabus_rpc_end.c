/* ISC license. */

#include <stdint.h>

#include <skalibs/uint64.h>
#include <skalibs/genalloc.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/avltree.h>
#include <skalibs/skaclient.h>

#include <skabus/rpc.h>

void skabus_rpc_end (skabus_rpc_t *a)
{
  skaclient_end(&a->connection) ;
  genalloc_free(uint64_t, &a->qlist) ;
  avltree_free(&a->qmap) ;
  gensetdyn_free(&a->q) ;
  gensetdyn_free(&a->r) ;
  a->pmid = (uint32_t)-1 ;
}
