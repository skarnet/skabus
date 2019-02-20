 /* ISC license. */

#include <errno.h>

#include <skalibs/posixishard.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/unixmessage.h>

#include <skabus/rpc.h>
#include "skabus-rpc-internal.h"

int skabus_rpc_interface_register_cb (unixmessage_t const *m, void *p)
{
  skabus_rpc_interface_result_t *r = p ;
  if (m->len != 1 || m->nfds) return (errno = EPROTO, 0) ;
  r->err = m->s[0] ;
  if (r->err) gensetdyn_delete(r->r, r->ifid) ;
  return 1 ;
}
