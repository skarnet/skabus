/* ISC license. */

#include <skalibs/alloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/genalloc.h>
#include <skalibs/skaclient.h>

#include <skabus/pub.h>

static void skabus_pub_cltinfo_free (skabus_pub_cltinfo_t *p)
{
  fd_close(p->fd) ;
  if (p->nfds)
  {
    for (size_t i = 0 ; i < p->nfds ; i++) fd_close(p->fds[i]) ;
    alloc_free(p->fds) ;
  }
}

void skabus_pub_end (skabus_pub_t *a)
{
  skaclient_end(&a->connection) ;
  genalloc_deepfree(skabus_pub_cltinfo_t, &a->info, &skabus_pub_cltinfo_free) ;
  a->head = 0 ;
}
