 /* ISC license. */

#include <sys/uio.h>
#include <string.h>
#include <stdint.h>
#include <skalibs/uint32.h>
#include <skalibs/uint64.h>
#include <skalibs/tai.h>
#include <skalibs/strerr2.h>
#include <skalibs/genalloc.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/avltree.h>
#include <skalibs/unixmessage.h>
#include <skabus/rpc.h>
#include "skabus-rpcd.h"

static void *query_serial_dtok (uint32_t d, void *x)
{
  (void)x ;
  return &QUERY(d)->serial ;
}

static void *query_deadline_dtok (uint32_t d, void *x)
{
  (void)x ;
  return &QUERY(d)->deadline ;
}

static int query_serial_cmp (void const *a, void const *b, void *x)
{
  uint64_t aa = *(uint64_t *)a ;
  uint64_t bb = *(uint64_t *)b ;
  (void)x ;
  return aa < bb ? -1 : aa > bb ;
}

static int query_deadline_cmp (void const *a, void const *b, void *x)
{
  tain const *aa = (tain const *)a ;
  tain const *bb = (tain const *)b ;
  (void)x ;
  return tain_less(aa, bb) ? -1 : tain_less(bb, aa) ;
}

gensetdyn queries = GENSETDYN_ZERO ;
static avltree qserialdict = AVLTREE_INIT(10, 1, 2, &query_serial_dtok, &query_serial_cmp, 0) ;
static avltree qdeadlinedict = AVLTREE_INIT(10, 1, 2, &query_deadline_dtok, &query_deadline_cmp, 0) ;

static inline void query_delete (uint32_t i)
{
  if (!avltree_delete(&qdeadlinedict, &QUERY(i)->deadline))
    strerr_diefu1sys(111, "avltree_delete qdeadlinedict in query_delete") ;
  if (!avltree_delete(&qserialdict, &QUERY(i)->serial))
    strerr_diefu1sys(111, "avltree_delete qserialdict in query_delete") ;
  if (!gensetdyn_delete(&queries, i))
    strerr_diefu1sys(111, "gensetdyn_delete in query_delete") ;
}

void query_remove (uint32_t i)
{
  query_t *q = QUERY(i) ;
  client_t *c = CLIENT(q->client) ;
  interface_t *y = INTERFACE(q->interface) ;
  if (!gensetdyn_delete(&c->queries, q->clientindex))
    strerr_diefu1sys(111, "gensetdyn_delete c->queries in query_remove") ;
  if (!gensetdyn_delete(&y->queries, q->interfaceindex))
    strerr_diefu1sys(111, "gensetdyn_delete y->queries in query_remove") ;
  query_delete(i) ;
}

void query_fail (uint32_t i, char status)
{
  query_t *q = QUERY(i) ;
  client_t *c = CLIENT(q->client) ;
  char pack[10] = "Rssssssssr" ;
  unixmessage m = { .s = pack, .len = 10, .fds = 0, .nfds = 0 } ;
  uint64_pack_big(pack+1, q->serial) ;
  pack[9] = status ;
  if (!unixmessage_put(&c->async.out, &m))
    strerr_diefu1sys(111, "unixmessage_put in query_fail") ;
  query_remove(i) ;
  client_setdeadline(c) ;
}

int query_cancel (uint32_t i, char reason)
{
  query_t *q = QUERY(i) ;
  interface_t *y = INTERFACE(q->interface) ;
  client_t *c = CLIENT(y->client) ;
  char pack[14] = "Ciiiissssssssr" ;
  unixmessage m = { .s = pack, .len = 14, .fds = 0, .nfds = 0 } ;
  uint32_pack_big(pack+1, y->id) ;
  uint64_pack_big(pack+5, q->serial) ;
  pack[13] = reason ;
  if (!unixmessage_put(&c->async.out, &m)) return 0 ;
  client_setdeadline(c) ;
  return 1 ;
}

int query_cancelremove (uint32_t i, char reason)
{
  if (!query_cancel(i, reason)) return 0 ;
  query_remove(i) ;
  return 1 ;
}

int query_lookup_by_serial (uint64_t serial, uint32_t *d)
{
  return avltree_search(&qserialdict, &serial, d) ;
}

int query_lookup_by_mindeadline (uint32_t *d)
{
  return avltree_min(&qdeadlinedict, d) ;
}

void query_get_mindeadline (tain *deadline)
{
  uint32_t d ;
  if (query_lookup_by_mindeadline(&d)) *deadline = QUERY(d)->deadline ;
  else tain_add_g(deadline, &tain_infinite_relative) ;
}

int query_add (uint32_t *d, tain const *deadline, uint32_t client, uint32_t interface)
{
  static uint64_t serial = 1 ;
  uint32_t qq, cc, yy ;
  query_t *q ;
  if (!gensetdyn_new(&queries, &qq)) return 0 ;
  if (!gensetdyn_new(&CLIENT(client)->queries, &cc)) goto end0 ;
  if (!gensetdyn_new(&INTERFACE(interface)->queries, &yy)) goto end1 ;
  q = QUERY(qq) ;
  q->serial = serial ;
  q->deadline = *deadline ;
  q->client = client ;
  q->clientindex = cc ;
  q->interface = interface ;
  q->interfaceindex = yy ;
  if (!avltree_insert(&qserialdict, qq)) goto end2 ;
  for (;;)
  {
    static tain const nano1 = { .sec = TAI_ZERO, .nano = 1 } ;
    uint32_t d ;
    if (!avltree_search(&qdeadlinedict, &q->deadline, &d)) break ;
    tain_add(&q->deadline, &q->deadline, &nano1) ;
  }
  if (!avltree_insert(&qdeadlinedict, qq)) goto end3 ;
  serial++ ;
  *d = qq ;
  return 1 ;

 end3:
  if (!avltree_delete(&qserialdict, &serial))
    strerr_diefu1sys(111, "avltree_delete in query_add") ;
 end2:
  if (!gensetdyn_delete(&INTERFACE(interface)->queries, yy))
    strerr_diefu1sys(111, "gensetdyn_delete INTERFACE(interface)->queries in query_add") ;
 end1:
  if (!gensetdyn_delete(&CLIENT(client)->queries, cc))
    strerr_diefu1sys(111, "gensetdyn_delete CLIENT(client)->queries in query_add") ;
 end0:
  if (!gensetdyn_delete(&queries, qq))
    strerr_diefu1sys(111, "gensetdyn_delete queries in query_add") ;
  return 0 ;
}

int query_send (uint32_t qq, unixmessage const *m)
{
  skabus_rpc_rinfo_t rinfo ;
  char pack[4 + SKABUS_RPC_RINFO_PACK] = "Q" ;
  struct iovec v[2] = { { .iov_base = pack, .iov_len = 4 + SKABUS_RPC_RINFO_PACK }, { .iov_base = m->s, .iov_len = m->len } } ;
  unixmessagev mtosend = { .v = v, .vlen = 2, .fds = m->fds, .nfds = m->nfds } ;
  query_t *q = QUERY(qq) ;
  interface_t *y = INTERFACE(q->interface) ;
  client_t *c = CLIENT(q->client) ;
  char const *idstr = client_idstr(c) ;
  size_t idstrlen = strlen(idstr) ;
  rinfo.serial = q->serial ;
  rinfo.limit = q->deadline ;
  tain_copynow(&rinfo.timestamp) ;
  rinfo.uid = c->uid ;
  rinfo.gid = c->gid ;
  memcpy(rinfo.idstr, idstr, idstrlen) ;
  memset(rinfo.idstr + idstrlen, 0, SKABUS_RPC_IDSTR_SIZE + 1 - idstrlen) ;
  uint32_pack_big(pack+1, y->id) ;
  skabus_rpc_rinfo_pack(pack + 5, &rinfo) ;
  c = CLIENT(y->client) ;
  if (!unixmessage_putv_and_close(&c->async.out, &mtosend, unixmessage_bits_closeall)) return 0 ;
  client_setdeadline(c) ;
  return 1 ;
}

void query_reply (uint32_t qq, char result, unixmessage const *m)
{
  char pack[11] = "R" ;
  struct iovec v[2] = { { .iov_base = pack, .iov_len = 11 }, { .iov_base = m->s, .iov_len = m->len } } ;
  unixmessagev mtosend = { .v = v, .vlen = 2, .fds = m->fds, .nfds = m->nfds } ;
  query_t *q = QUERY(qq) ;
  client_t *c = CLIENT(q->client) ;
  uint64_pack_big(pack+1, q->serial) ;
  pack[9] = 0 ;
  pack[10] = result ;
  if (!unixmessage_putv_and_close(&c->async.out, &mtosend, unixmessage_bits_closeall))
    strerr_diefu1sys(111, "unixmessage_put in query_reply") ;
  client_setdeadline(c) ;
  query_remove(qq) ;
}
