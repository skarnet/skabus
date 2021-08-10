/* ISC license. */

#include <sys/uio.h>
#include <errno.h>

#include <skalibs/posixishard.h>
#include <skalibs/uint64.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/avltree.h>
#include <skalibs/unixmessage.h>
#include <skalibs/skaclient.h>

#include <skabus/rpc.h>
#include "skabus-rpc-internal.h"

static int autocancel_cb (unixmessage const *m, void *p)
{
  (void)m ;
  (void)p ;
  return 1 ;
}

int skabus_rpc_send_cb (unixmessage const *m, void *p)
{
  skabus_rpc_send_result_t *r = p ;
  skabus_rpc_qinfo_t *info = GENSETDYN_P(skabus_rpc_qinfo_t, &r->a->q, r->i) ;
  if (!m->len) return (errno = EPROTO, 0) ;
  if (m->s[0])
  {
    r->err = m->s[0] ;
    r->u = 0 ;
    info->status = EINVAL ;
    gensetdyn_delete(&r->a->q, r->i) ;
    return 1 ;
  }
  if (m->len != 9) return (errno = EPROTO, 0) ;
  uint64_unpack_big(m->s+1, &info->serial) ;
  if (!avltree_insert(&r->a->qmap, r->i))
  {
   /* the client can't store the info but the server is performing the query,
      so we try to send a stealthy cancel */
    char what = 'C' ;
    struct iovec v[2] = { { .iov_base = &what, .iov_len = 1 }, { .iov_base = m->s + 1, .iov_len = 8 } } ;
    r->err = errno ;
    r->u = 0 ;
    info->status = EINVAL ;
    gensetdyn_delete(&r->a->q, r->i) ;
    if (skaclient_putv(&r->a->connection, v, 2, &autocancel_cb, 0))
      skaclient_flush(&r->a->connection) ;
    return 1 ;
  }
  r->u = info->serial ;
  info->status = EBUSY ;
  return 1 ;
}
