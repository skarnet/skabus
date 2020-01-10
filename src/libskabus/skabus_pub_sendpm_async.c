/* ISC license. */

#include <string.h>
#include <sys/uio.h>
#include <errno.h>

#include <skalibs/unixmessage.h>
#include <skalibs/skaclient.h>

#include <skabus/pub.h>
#include "skabus-pub-internal.h"

int skabus_pub_sendpm_withfds_async (skabus_pub_t *a, char const *idstr, char const *s, size_t len, int const *fds, unsigned int nfds, unsigned char const *bits, skabus_pub_send_result_t *result)
{
  size_t idlen = strlen(idstr) ;
  char tmp[2] = "+" ;
  struct iovec v[3] =
  {
    { .iov_base = tmp, .iov_len = 2 },
    { .iov_base = (char *)idstr, .iov_len = idlen+1 },
    { .iov_base = (char *)s, .iov_len = len }
  } ;
  unixmessage_v_t m = { .v = v, .vlen = 3, .fds = (int *)fds, .nfds = nfds } ;
  if (idlen > SKABUS_PUB_IDSTR_SIZE) return (errno = ERANGE, 0) ;
  tmp[1] = (unsigned char)idlen ;
  return skaclient_putmsgv_and_close(&a->connection, &m, bits, &skabus_pub_send_cb, result) ;
}
