/* ISC license. */

#include <sys/uio.h>

#include <skalibs/unixmessage.h>
#include <skalibs/skaclient.h>

#include <skabus/pub.h>
#include "skabus-pub-internal.h"

int skabus_pub_send_withfds_async (skabus_pub_t *a, char const *s, size_t len, int const *fds, unsigned int nfds, unsigned char const *bits, skabus_pub_send_result_t *result)
{
  struct iovec v[2] = { { .iov_base = "!", .iov_len = 1 }, { .iov_base = (char *)s, .iov_len = len } } ;
  unixmessage_v_t m = { .v = v, .vlen = 2, .fds = (int *)fds, .nfds = nfds } ;
  return skaclient_putmsgv_and_close(&a->connection, &m, bits, &skabus_pub_send_cb, result) ;
}
