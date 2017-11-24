/* ISC license. */

#include <stdint.h>
#include <errno.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/avltree.h>
#include <skabus/rpc.h>

int skabus_rpc_get (skabus_rpc_t *a, uint64_t serial, int *result, unixmessage_t *m)
{
  uint32_t id ;
  skabus_rpc_qinfo_t *p ;
  if (!avltree_search(&a->qmap, &serial, &id)) return 0 ;
  p = GENSETDYN_P(skabus_rpc_qinfo_t, &a->q, id) ;
  if (p->status) return (errno = p->status, 0) ;
  *result = (int)(unsigned char)p->result ;
  *m = p->message ;
  return 1 ;
}
