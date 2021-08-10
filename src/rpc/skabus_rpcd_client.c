/* ISC license. */

#include <sys/types.h>
#include <stdint.h>
#include <errno.h>
#include <regex.h>
#include <skalibs/djbunix.h>
#include <skalibs/error.h>
#include <skalibs/strerr2.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/genalloc.h>
#include <skalibs/genset.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/unixmessage.h>
#include <skalibs/unixconnection.h>
#include <skalibs/skaclient.h>
#include <skabus/rpc.h>
#include "skabus-rpcd.h"

static inline void client_free (client_t *c)
{
  if (!genalloc_len(uint32_t, &c->interfaces)) regfree(&c->idstr_re) ;
  regfree(&c->interfaces_re) ;
  genalloc_free(unsigned int, &c->interfaces) ;
  gensetdyn_free(&c->queries) ;
  fd_close(unixmessage_sender_fd(&c->sync.out)) ;
  unixconnection_free(&c->sync) ;
  if (unixmessage_sender_fd(&c->async.out) >= 0)
  {
    fd_close(unixmessage_sender_fd(&c->async.out)) ;
    unixconnection_free(&c->async) ;
  }
}

genset *clients ;
unsigned int sentinel ;

static inline void client_delete (uint32_t i, uint32_t prev)
{
  CLIENT(prev)->next = CLIENT(i)->next ;
  client_free(CLIENT(i)) ;
  genset_delete(clients, i) ;
}

static int query_cancelremove_iter (char *s, void *reason)
{
  uint32_t i = *(uint32_t *)s ;
  return query_cancelremove(i, *(char *)reason) ;
}

void client_remove (uint32_t i, uint32_t prev)
{
  client_t *c = CLIENT(i) ;
  char reason = ECONNABORTED ;
  if (gensetdyn_iter(&c->queries, &query_cancelremove_iter, &reason) < gensetdyn_n(&c->queries))
    strerr_diefu1sys(111, "query_cancelremove_iter in client_remove") ;
  while (genalloc_len(uint32_t, &c->interfaces))
    interface_remove(genalloc_s(uint32_t, &c->interfaces)[genalloc_len(uint32_t, &c->interfaces) - 1]) ;
  client_delete(i, prev) ;
}

void client_setdeadline (client_t *c)
{
  tain blah ;
  tain_half(&blah, &tain_infinite_relative) ;
  tain_add_g(&blah, &blah) ;
  if (tain_less(&blah, &c->deadline)) tain_add_g(&c->deadline, &answertto) ;
}

void client_add (uint32_t *d, regex_t const *idstr_re, regex_t const *interfaces_re, uid_t uid, gid_t gid, int fdsock, uint32_t flags)
{
  uint32_t cc = genset_new(clients) ;
  client_t *c = CLIENT(cc) ;
  c->next = CLIENT(sentinel)->next ;
  c->uid = uid ;
  c->gid = gid ;
  tain_add_g(&c->deadline, &answertto) ;
  c->interfaces = genalloc_zero ;
  c->queries = gensetdyn_zero ;
  c->idstr_re = *idstr_re ;
  c->interfaces_re = *interfaces_re ;
  unixconnection_init(&c->sync, fdsock, fdsock) ;
  unixconnection_init(&c->async, -1, -1) ;
  c->async.out.fd = -(int)flags-1 ;
  CLIENT(sentinel)->next = cc ;
  *d = cc ;
}

void client_nextdeadline (uint32_t i, tain *deadline)
{
  client_t *c = CLIENT(i) ;
  if (tain_less(&c->deadline, deadline)) *deadline = c->deadline ;
}

int client_prepare_iopause (uint32_t cc, tain *deadline, iopause_fd *x, uint32_t *j, int notlameduck)
{
  client_t *c = CLIENT(cc) ;
  int inflight = 0 ;
  uint32_t i = genalloc_len(uint32_t, &c->interfaces) ;
  if (tain_less(&c->deadline, deadline)) *deadline = c->deadline ;
  if (!unixmessage_sender_isempty(&c->sync.out) | !unixmessage_receiver_isempty(&c->sync.in) || (notlameduck && !unixmessage_receiver_isfull(&c->sync.in)))
  {
    x[*j].fd = unixmessage_sender_fd(&c->sync.out) ;
    x[*j].events = ((!unixmessage_receiver_isempty(&c->sync.in) || (notlameduck && !unixmessage_receiver_isfull(&c->sync.in))) ? IOPAUSE_READ : 0)
                  | (!unixmessage_sender_isempty(&c->sync.out) ? IOPAUSE_WRITE : 0) ;
    c->xindex[0] = (*j)++ ;
  }
  else c->xindex[0] = 0 ;
  while (i--)
  {
    interface_t *y = INTERFACE(genalloc_s(uint32_t, &c->interfaces)[i]) ;
    if (gensetdyn_n(&y->queries))
    {
      inflight = 1 ;
      break ;
    }
  }
  if (!unixmessage_sender_isempty(&c->async.out) || !unixmessage_receiver_isempty(&c->async.in) || inflight)
  {
    x[*j].fd = unixmessage_sender_fd(&c->async.out) ;
    x[*j].events = (unixmessage_sender_isempty(&c->async.out) ? IOPAUSE_WRITE : 0)
                 | (!unixmessage_receiver_isempty(&c->async.in) || inflight ? IOPAUSE_READ : 0) ;
    c->xindex[1] = (*j)++ ;
  }
  else c->xindex[1] = 0 ;
  return c->xindex[0] || c->xindex[1] ;
}

int client_flush (uint32_t i, iopause_fd const *x)
{
  client_t *c = CLIENT(i) ;
  int isflushed = 2 ;
  if (c->xindex[0] && (x[c->xindex[0]].revents & IOPAUSE_WRITE))
  {
    if (!unixmessage_sender_flush(&c->sync.out))
      if (!error_isagain(errno)) return 0 ;
      else isflushed = 0 ;
    else isflushed = 1 ;
  }

  if (c->xindex[1] && (x[c->xindex[1]].revents & IOPAUSE_WRITE))
  {
    if (!unixmessage_sender_flush(&c->async.out))
      if (!error_isagain(errno)) return 0 ;
      else isflushed = 0 ;
    else isflushed = !!isflushed ;
  }

  if (isflushed == 1) tain_add_g(&c->deadline, &tain_infinite_relative) ;
  return 1 ;
}

int client_read (uint32_t cc, iopause_fd const *x)
{
  client_t *c = CLIENT(cc) ;
  if (!unixmessage_receiver_isempty(&c->sync.in) || (c->xindex[0] && x[c->xindex[0]].revents & IOPAUSE_READ))
  {
    if (unixmessage_sender_fd(&c->async.out) < 0)
    {
      unixmessage m ;
      int r = unixmessage_receive(&c->sync.in, &m) ;
      if (r < 0) return -1 ;
      if (r)
      {
        uint32_t flags = -(unixmessage_sender_fd(&c->async.out) + 1) ;
        if (!skaclient_server_bidi_ack(&m, &c->sync.out, &c->async.out, &c->async.in, c->async.mainbuf, UNIXMESSAGE_BUFSIZE, c->async.auxbuf, UNIXMESSAGE_AUXBUFSIZE, SKABUS_RPC_BANNER1, SKABUS_RPC_BANNER1_LEN, SKABUS_RPC_BANNER2, SKABUS_RPC_BANNER2_LEN))
        {
          unixmessage_drop(&m) ;
          return -1 ;
        }
        if (!(flags & 1)) unixmessage_receiver_refuse_fds(&c->sync.in) ;
        if (!(flags & 2)) unixmessage_receiver_refuse_fds(&c->async.in) ;
      }
    }
    else
    {
      int r = unixmessage_handle(&c->sync.in, &parse_protocol_sync, &cc) ;
      if (r <= 0) return r ;
    }
  }
  if (!unixmessage_receiver_isempty(&c->async.in) || (c->xindex[1] && x[c->xindex[1]].revents & IOPAUSE_READ))
  {
    int r = unixmessage_handle(&c->async.in, &parse_protocol_async, &cc) ;
    if (r <= 0) return r ;
  }
  return 1 ;
}
