/* ISC license. */

#include <stdint.h>
#include <errno.h>

#include <skalibs/uint64.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/avltree.h>
#include <skalibs/skaclient.h>

#include <skabus/rpc.h>

int skabus_rpc_cancel_async (skabus_rpc_t *a, uint64_t serial, unsigned char *err)
{
  uint32_t id ;
  skabus_rpc_qinfo_t *p ;
  char pack[9] = "C" ;
  if (!avltree_search(&a->qmap, &serial, &id)) return 0 ;
  p = GENSETDYN_P(skabus_rpc_qinfo_t, &a->q, id) ;
  if (p->status != EAGAIN) return (errno = EINVAL, 0) ;
  uint64_pack_big(pack+1, serial) ;
  if (!skaclient_put(&a->connection, pack, 9, &skaclient_default_cb, err)) return 0 ;
  p->status = EINVAL ;
  avltree_delete(&a->qmap, &serial) ;
  gensetdyn_delete(&a->q, id) ;
  return 1 ;
}
