/* ISC license. */

#include <stdint.h>
#include <errno.h>

#include <skalibs/error.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/avltree.h>

#include <skabus/rpc.h>

int skabus_rpc_get (skabus_rpc_t *a, uint64_t serial, int *result, unixmessage *m)
{
  uint32_t id ;
  skabus_rpc_qinfo_t *p ;
  if (!avltree_search(&a->qmap, &serial, &id))
  {
    if (errno == ESRCH) errno = EINVAL ;
    return -1 ;
  }
  p = GENSETDYN_P(skabus_rpc_qinfo_t, &a->q, id) ;
  if (p->status)
    return error_isagain(p->status) ? 0 : (errno = p->status, -1) ;
  *result = (int)(unsigned char)p->result ;
  *m = p->message ;
  return 1 ;
}
