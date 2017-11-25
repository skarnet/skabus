/* ISC license. */

#include <sys/uio.h>
#include <skalibs/uint64.h>
#include <skalibs/unixmessage.h>
#include <skalibs/skaclient.h>
#include <skabus/rpc.h>

int skabus_rpc_reply_withfds_async (skabus_rpc_t *a, uint64_t serial, char result, char const *s, size_t len, int const *fds, unsigned int nfds, unsigned char const *bits)
{
  char pack[10] = "R" ;
  struct iovec v[2] = { { .iov_base = pack, .iov_len = 10 }, { .iov_base = (char *)s, .iov_len = len } } ;
  unixmessage_v_t m = { .v = v, .vlen = 2, .fds = (int *)fds, .nfds = nfds } ;
  uint64_pack_big(pack+1, serial) ;
  pack[9] = result ;
  return skaclient_aputv_and_close(&a->connection, &m, bits) ;
}
