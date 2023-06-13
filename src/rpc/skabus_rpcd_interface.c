/* ISC license. */

#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <regex.h>

#include <skalibs/posixplz.h>
#include <skalibs/strerr.h>
#include <skalibs/genalloc.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/avltree.h>
#include <skabus/rpc.h>
#include "skabus-rpcd.h"

static inline void interface_free (interface_t *p)
{
  p->name[0] = p->name[1] = 0 ;
  gensetdyn_free(&p->queries) ;
}

static void *if_dtok (uint32_t d, void *x)
{
  (void)x ;
  return INTERFACE(d)->name ;
}

static int if_cmp (void const *a, void const *b, void *x)
{
  (void)x ;
  return strncmp((char const *)a, (char const *)b, SKABUS_RPC_INTERFACE_MAXLEN) ;
}

gensetdyn interfaces = GENSETDYN_ZERO ;
static avltree ifdict = AVLTREE_INIT(2, 3, 8, &if_dtok, &if_cmp, 0) ;

static inline void interface_delete (uint32_t i)
{
  interface_t *y = INTERFACE(i) ;
  if (!avltree_delete(&ifdict, y->name))
    strerr_diefu1sys(111, "avltree_delete in interface_delete") ;
  interface_free(y) ;
  if (!gensetdyn_delete(&interfaces, i))
    strerr_diefu1sys(111, "gensetdyn_delete in interface_delete") ;
}

static int query_fail_iter (void *s, void *reason)
{
  query_fail(*(uint32_t *)s, *(unsigned char *)reason) ;
  return 1 ;
}

static inline void client_interfacemove (client_t *c, uint32_t from, uint32_t to)
{
  uint32_t *ifaces = genalloc_s(uint32_t, &c->interfaces) ;
  INTERFACE(ifaces[from])->index = to ;
  ifaces[to] = ifaces[from] ;
}

void interface_remove (uint32_t i)
 {
  interface_t *y = INTERFACE(i) ;
  client_t *c = CLIENT(y->client) ;
  uint32_t n = gensetdyn_n(&y->queries) ;
  unsigned char reason = ECONNRESET ;
  gensetdyn_iter(&y->queries, &query_fail_iter, &reason) ;
  n = genalloc_len(uint32_t, &c->interfaces) ;
  client_interfacemove(c, n-1, y->index) ;
  genalloc_setlen(uint32_t, &c->interfaces, n-1) ;
  interface_delete(i) ;
}

int interface_lookup_by_name (char const *s, uint32_t *d)
{
  return avltree_search(&ifdict, s, d) ;
}

int interface_add (uint32_t *d, char const *name, size_t namelen, uint32_t client, char const *re, uint32_t id)
{
  uint32_t yy ;
  int e ;
  genalloc *g = &CLIENT(client)->interfaces ;
  if (!genalloc_readyplus(uint32_t, g, 1)) return 0 ;
  if (!gensetdyn_new(&interfaces, &yy)) return 0 ;
  {
    interface_t *y = INTERFACE(yy) ;
    int r = skalibs_regcomp(&y->re, re, REG_EXTENDED | REG_NOSUB) ;
    if (r)
    {
      e = r == REG_ESPACE ? ENOMEM : EINVAL ;
      goto err ;
    }
    memcpy(y->name, name, namelen) ; y->name[namelen] = 0 ;
    y->id = id ;
    y->client = client ;
    y->index = genalloc_len(uint32_t, g) - 1 ;
    y->queries = gensetdyn_zero ;
    if (!avltree_insert(&ifdict, yy))
    {
      e = errno ;
      regfree(&y->re) ;
      goto err ;
    }
  }
  genalloc_append(uint32_t, g, &yy) ;
  *d = yy ;
  return 1 ;

 err:
  if (!gensetdyn_delete(&interfaces, yy))
    strerr_diefu1sys(111, "gensetdyn_delete in interface_add") ;
  errno = e ;
  return 0 ;
}
