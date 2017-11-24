/* ISC license. */

#include <stdint.h>
#include <errno.h>
#include <skalibs/uint64.h>
#include <skalibs/alloc.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/avltree.h>
#include <skabus/rpc.h>

int skabus_rpc_release (skabus_rpc_t *a, uint64_t serial)
{
  uint32_t id ;
  skabus_rpc_qinfo_t *p ;
  if (!avltree_search(&a->qmap, &serial, &id)) return 0 ;
  p = GENSETDYN_P(skabus_rpc_qinfo_t, &a->q, id) ;
  if (p->status) return (errno = p->status, 0) ;
  alloc_free(p->message.s) ;
  alloc_free(p->message.fds) ;
 /* fds are purposefully left open for the client. */
  p->status = EINVAL ;
  avltree_delete(&a->qmap, &serial) ;
  gensetdyn_delete(&a->q, id) ;
  return 1 ;
}
