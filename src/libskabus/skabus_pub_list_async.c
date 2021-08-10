/* ISC license. */

#include <stdint.h>
#include <errno.h>

#include <skalibs/posixishard.h>
#include <skalibs/uint32.h>
#include <skalibs/stralloc.h>
#include <skalibs/unixmessage.h>
#include <skalibs/skaclient.h>

#include <skabus/pub.h>

static int skabus_pub_list_cb (unixmessage const *m, void *p)
{
  skabus_pub_list_result_t *r = p ;
  uint32_t n ;
  size_t w = 9, len = m->len - 9 ;
  if (m->len < 9 || m->nfds) goto errproto ;
  r->err = m->s[0] ;
  if (r->err) return 1 ;
  uint32_unpack_big(m->s + 1, &n) ;
  if (n > 0x7fffffffu || m->len < 9 + (n<<1)) goto errproto ;
  if (!stralloc_readyplus(r->sa, m->len - 9 - n)) goto errproto ;
  r->n.left = n ;
  uint32_unpack_big(m->s + 5, &n) ; r->n.right = n ;
  for (uint32_t i = 0 ; i < r->n.left ; i++)
  {
    size_t thislen ;
    if (len < 2) goto errproto ;
    thislen = m->s[w++] + 1 ; len-- ;
    if ((thislen > len) || m->s[w + thislen]) goto errproto ;
    stralloc_catb(r->sa, m->s + w, thislen) ;
    w += thislen ; len -= thislen ;
  }
  return 1 ;
 errproto:
  return (errno = EPROTO, 0) ;
}

int skabus_pub_list_async (skabus_pub_t *a, stralloc *sa, skabus_pub_list_result_t *result)
{
  result->sa = sa ;
  return skaclient_put(&a->connection, "L", 1, &skabus_pub_list_cb, result) ;
}
