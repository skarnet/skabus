/* ISC license. */

#include <string.h>
#include <stdint.h>
#include <errno.h>

#include <skalibs/posixishard.h>
#include <skalibs/uint64.h>
#include <skalibs/tai.h>
#include <skalibs/alloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/unixmessage.h>
#include <skalibs/skaclient.h>

#include <skabus/pub.h>

static int handler (unixmessage const *m, void *x)
{
  skabus_pub_t *a = (skabus_pub_t *)x ;
  size_t n = genalloc_len(skabus_pub_cltinfo_t, &a->info) ;
  char const *s = m->s ;
  size_t len = m->len ;
  skabus_pub_cltinfo_t *p ;
  uint8_t idlen ;
  if (len < 11 + TAIN_PACK || !m->nfds) goto errproto ;
  if (!genalloc_readyplus(skabus_pub_cltinfo_t, &a->info, 1)) goto err ;
  p = genalloc_s(skabus_pub_cltinfo_t, &a->info) + n ;
  p->fd = m->fds[0] ;
  p->nfds = m->nfds - 1 ;
  if (p->nfds > 1)
  {
    p->fds = alloc(p->nfds * sizeof(int)) ;
    if (!p->fds) goto err ;
    for (size_t i = 0 ; i < p->nfds ; i++) p->fds[i] = m->fds[1 + i] ;
  }
  else p->fds = 0 ;
  uint64_unpack_big(s, &p->msginfo.serial) ; s += 8 ; len -= 8 ;
  tain_unpack(s, &p->msginfo.timestamp) ; s += TAIN_PACK ; len -= TAIN_PACK ;
  p->msginfo.flags = *s++ ; len-- ;
  idlen = *s++ ; len-- ;
  if (len != (size_t)idlen + 1 || s[idlen] || idlen > SKABUS_PUB_IDSTR_SIZE) goto errprotof ;
  memcpy(p->msginfo.sender, s, len) ;
  genalloc_setlen(skabus_pub_cltinfo_t, &a->info, n+1) ;
  return 1 ;

 errprotof:
  alloc_free(p->fds) ;
 errproto:
  errno = EPROTO ;
 err:
  unixmessage_drop(m) ;
  return 0 ;
}

int skabus_pub_update (skabus_pub_t *a)
{
  return skaclient_update(&a->connection, &handler, a) ;
}
