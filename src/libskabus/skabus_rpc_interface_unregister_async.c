/* ISC license. */

#include <string.h>
#include <sys/uio.h>
#include <errno.h>

#include <skalibs/posixishard.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/unixmessage.h>
#include <skalibs/skaclient.h>

#include <skabus/rpc.h>

static int skabus_rpc_interface_unregister_cb (unixmessage const *m, void *p)
{
  skabus_rpc_interface_result_t *r = p ;
  if (m->len != 1 || m->nfds) return (errno = EPROTO, 0) ;
  r->err = m->s[0] ;
  return r->err ? 1 : gensetdyn_delete(r->r, r->ifid) ;
}

int skabus_rpc_interface_unregister_async (skabus_rpc_t *a, uint32_t ifid, skabus_rpc_interface_result_t *result)
{
  char *name = GENSETDYN_P(skabus_rpc_ifnode_t, &a->r, ifid)->name ;
  size_t n = strlen(name) ;
  char pack[2] = { 'i', (unsigned char)n } ;
  struct iovec v[2] = { { .iov_base = pack, .iov_len = 2 }, { .iov_base = name, .iov_len = n+1 } } ;
  result->r = &a->r ;
  result->ifid = ifid ;
  return skaclient_putv(&a->connection, v, 2, &skabus_rpc_interface_unregister_cb, result) ;
}
