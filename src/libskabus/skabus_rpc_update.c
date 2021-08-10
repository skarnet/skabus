/* ISC license. */

#include <string.h>
#include <stdint.h>
#include <errno.h>

#include <skalibs/posixishard.h>
#include <skalibs/uint32.h>
#include <skalibs/uint64.h>
#include <skalibs/alloc.h>
#include <skalibs/bytestr.h>
#include <skalibs/genalloc.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/avltree.h>
#include <skalibs/unixmessage.h>
#include <skalibs/skaclient.h>

#include <skabus/rpc.h>

typedef int localhandler_func (skabus_rpc_t *, unixmessage *) ;
typedef localhandler_func *localhandler_func_ref ;

static int skabus_rpc_serve (skabus_rpc_t *a, unixmessage *m)
{
  skabus_rpc_rinfo_t rinfo ;
  uint32_t ifid ;
  skabus_rpc_interface_t *p ;
  if (m->len < 4 + SKABUS_RPC_RINFO_PACK) return (errno = EPROTO, 0) ;
  uint32_unpack_big(m->s, &ifid) ; m->s += 4 ; m->len -= 4 ;
  skabus_rpc_rinfo_unpack(m->s, &rinfo) ;
  m->s += SKABUS_RPC_RINFO_PACK ; m->len -= SKABUS_RPC_RINFO_PACK ;
  p = GENSETDYN_P(skabus_rpc_interface_t, &a->r, ifid) ;
  if (!p) return (errno = ESRCH, 0) ;
  return (*p->f)(a, &rinfo, m, p->data) ;
}

static int skabus_rpc_cancelr (skabus_rpc_t *a, unixmessage *m)
{
  uint64_t serial ;
  uint32_t ifid ;
  skabus_rpc_interface_t *p ; 
  if (m->len != 13 || m->nfds) return (errno = EPROTO, 0) ;
  uint32_unpack_big(m->s, &ifid) ;
  uint64_unpack_big(m->s+4, &serial) ;
  p = GENSETDYN_P(skabus_rpc_interface_t, &a->r, ifid) ;
  if (!p) return (errno = ESRCH, 0) ;
  return (*p->cancelf)(serial, m->s[12], p->data) ;
}

static int skabus_rpc_handle_reply (skabus_rpc_t *a, unixmessage *m)
{
  skabus_rpc_qinfo_t *p ;
  uint64_t serial ;
  uint32_t id ;	
  if (m->len < 9) return (errno = EPROTO, 0) ;
  uint64_unpack_big(m->s, &serial) ;
  if (!avltree_search(&a->qmap, &serial, &id))
  {
    unixmessage_drop(m) ;
    return 1 ;
  }
  p = GENSETDYN_P(skabus_rpc_qinfo_t, &a->q, id) ;
  if (!genalloc_readyplus(uint64_t, &a->qlist, 1)) return 0 ;
  if (!m->s[8])
  {
    if (m->len < 10) return (errno = EPROTO, 0) ;
    p->message.s = alloc(m->len - 10) ;
    if (!p->message.s) return 0 ;
    p->message.fds = (int *)alloc(m->nfds * sizeof(int)) ;
    if (!p->message.fds)
    {
      alloc_free(p->message.s) ;
      p->message.s = 0 ;
      return 0 ;
    }
    p->result = m->s[9] ;
    p->message.len = m->len - 10 ;
    p->message.nfds = m->nfds ;
    memcpy(p->message.s, m->s + 10, p->message.len) ;
    memcpy(p->message.fds, m->fds, p->message.nfds * sizeof(int)) ;
  }
  p->status = m->s[8] ;
  genalloc_append(uint64_t, &a->qlist, &serial) ;
  return 1 ;
}

static int skabus_rpc_error (skabus_rpc_t *a, unixmessage *m)
{
  (void)a ;
  (void)m ;
  return (errno = EPROTO, 0) ;
}

static int handler (unixmessage const *m, void *x)
{
  skabus_rpc_t *a = x ;
  unixmessage mm = { .s = m->s + 1, .len = m->len - 1, .fds = m->fds, .nfds = m->nfds } ;
  static localhandler_func_ref const f[4] = 
  {
    &skabus_rpc_serve,
    &skabus_rpc_handle_reply,
    &skabus_rpc_cancelr,
    &skabus_rpc_error
  } ;
  if (!m->len)
  {
    unixmessage_drop(m) ;
    return (errno = EPROTO, 0) ;
  }
  if (!(*f[byte_chr("QRC", 3, m->s[0])])(a, &mm))
  {
    unixmessage_drop(m) ;
    return 0 ;
  }
  return 1 ;
}

int skabus_rpc_update (skabus_rpc_t *a)
{
  return skaclient_update(&a->connection, &handler, a) ;
}
