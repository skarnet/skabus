/* ISC license. */

#include <errno.h>

#include <skalibs/posixishard.h>
#include <skalibs/uint64.h>
#include <skalibs/unixmessage.h>

#include <skabus/pub.h>

int skabus_pub_send_cb (unixmessage_t const *m, void *p)
{
  skabus_pub_send_result_t *r = p ;
  if (!m->len || m->nfds) return (errno = EPROTO, 0) ;
  if (m->s[0])
  {
    r->u = 0 ;
    r->err = m->s[0] ;
    return 1 ;
  }
  if (m->len != 9) return (errno = EPROTO, 0) ;
  uint64_unpack_big(m->s + 1, &r->u) ;
  r->err = 0 ;
  return 1 ;
}
