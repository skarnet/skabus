/* ISC license. */

#include <string.h>
#include <sys/uio.h>
#include <errno.h>

#include <skalibs/tai.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/unixmessage.h>
#include <skalibs/skaclient.h>

#include <skabus/rpc.h>
#include "skabus-rpc-internal.h"

int skabus_rpc_sendvq_withfds_async (skabus_rpc_t *a, char const *prefix, size_t plen, char const *ifname, struct iovec const *v, unsigned int vlen, int const *fds, unsigned int nfds, unsigned char const *bits, tain_t const *limit, skabus_rpc_send_result_t *r)
{
  size_t iflen = strlen(ifname) ;
  char pack[2 + TAIN_PACK] = "Q" ;
  struct iovec vv[vlen + 3] ;
  unixmessage_v_t m = { .v = vv, .vlen = vlen + 3, .fds = (int *)fds, .nfds = nfds } ;
  iflen += plen ;
  if (iflen > SKABUS_RPC_INTERFACE_MAXLEN) return (errno = ENAMETOOLONG, 0) ;
  if (!gensetdyn_new(&a->q, &r->i)) return 0 ;
  vv[0].iov_base = pack ; vv[0].iov_len = 2 + TAIN_PACK ;
  vv[1].iov_base = (char *)prefix ; vv[1].iov_len = plen ;
  vv[2].iov_base = (char *)ifname ; vv[2].iov_len = iflen + 1 ;
  for (unsigned int i = 0 ; i < vlen ; i++) vv[3 + i] = v[i] ;
  r->a = a ;
  if (limit) tain_pack(pack + 1, limit) ; else memset(pack + 1, 0, TAIN_PACK) ;
  pack[1 + TAIN_PACK] = (unsigned char)iflen ;
  if (!skaclient_putmsgv_and_close(&a->connection, &m, bits, &skabus_rpc_send_cb, r))
  {
    int e = errno ;
    gensetdyn_delete(&a->q, r->i) ;
    errno = e ;
    return 0 ;
  }
  GENSETDYN_P(skabus_rpc_qinfo_t, &a->q, r->i)->status = EINPROGRESS ;
  return 1 ;
}
