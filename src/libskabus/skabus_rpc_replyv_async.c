/* ISC license. */

#include <sys/uio.h>
#include <skalibs/uint64.h>
#include <skalibs/unixmessage.h>
#include <skalibs/skaclient.h>
#include <skabus/rpc.h>

int skabus_rpc_replyv_withfds_async (skabus_rpc_t *a, uint64_t serial, char result, struct iovec const *v, unsigned int vlen, int const *fds, unsigned int nfds, unsigned char const *bits)
{
  char pack[10] = "R" ;
  struct iovec vv[vlen + 1] ;
  unixmessagev m = { .v = vv, .vlen = vlen+1, .fds = (int *)fds, .nfds = nfds } ;
  vv[0].iov_base = pack ; vv[0].iov_len = 10 ;
  for (unsigned int i = 0 ; i < vlen ; i++) vv[1+i] = v[i] ;
  uint64_pack_big(pack+1, serial) ;
  pack[9] = result ;
  return skaclient_aputv_and_close(&a->connection, &m, bits) ;
}
