/* ISC license. */

#include <string.h>

#include <skalibs/alloc.h>
#include <skalibs/genalloc.h>

#include <skabus/pub.h>
#include "skabus-pub-internal.h"

size_t skabus_pub_message_get (skabus_pub_t *a, skabus_pub_msginfo_t *info, int *fd, int *fds)
{
  size_t n = genalloc_len(skabus_pub_cltinfo_t, &a->info) ;
  skabus_pub_cltinfo_t *p = genalloc_s(skabus_pub_cltinfo_t, &a->info) + a->head ;
  if (!n) return 0 ;
  *info = p->msginfo ;
  *fd = p->fd ;
  if (p->nfds)
  {
    for (size_t i = 0 ; i < p->nfds ; i++) fds[i] = p->fds[i] ;
    alloc_free(p->fds) ;
  }

  if (++a->head == n || a->head > SKABUS_HEAD_MAX)
  {
    n -= a->head ;
    memmove(a->info.s, a->info.s + a->head * sizeof(skabus_pub_cltinfo_t), n * sizeof(skabus_pub_cltinfo_t)) ;
    genalloc_setlen(skabus_pub_cltinfo_t, &a->info, n) ;
    a->head = 0 ;
  }
  return 1 + n - a->head ;
}
