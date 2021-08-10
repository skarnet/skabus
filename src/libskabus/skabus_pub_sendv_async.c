/* ISC license. */

#include <sys/uio.h>

#include <skalibs/unixmessage.h>
#include <skalibs/skaclient.h>

#include <skabus/pub.h>
#include "skabus-pub-internal.h"

int skabus_pub_sendv_withfds_async (skabus_pub_t *a, struct iovec const *v, unsigned int vlen, int const *fds, unsigned int nfds, unsigned char const *bits, skabus_pub_send_result_t *result)
{
  struct iovec vv[vlen+1] ;
  unixmessagev m = { .v = vv, .vlen = vlen+1, .fds = (int *)fds, .nfds = nfds } ;
  vv[0].iov_base = "!" ; vv[0].iov_len = 1 ;
  for (unsigned int i = 0 ; i < vlen ; i++) vv[1+i] = v[i] ;
  return skaclient_putmsgv_and_close(&a->connection, &m, bits, &skabus_pub_send_cb, result) ;
}
