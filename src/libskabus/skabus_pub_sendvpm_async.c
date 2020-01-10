/* ISC license. */

#include <string.h>
#include <sys/uio.h>
#include <errno.h>

#include <skalibs/unixmessage.h>
#include <skalibs/skaclient.h>

#include <skabus/pub.h>
#include "skabus-pub-internal.h"

int skabus_pub_sendvpm_withfds_async (skabus_pub_t *a, char const *idstr, struct iovec const *v, unsigned int vlen, int const *fds, unsigned int nfds, unsigned char const *bits, skabus_pub_send_result_t *result)
{
  size_t idlen = strlen(idstr) ;
  char tmp[2] = "+" ;
  struct iovec vv[vlen + 2] ;
  unixmessage_v_t m = { .v = vv, .vlen = vlen + 2, .fds = (int *)fds, .nfds = nfds } ;
  if (idlen > SKABUS_PUB_IDSTR_SIZE) return (errno = ERANGE, 0) ;
  tmp[1] = (unsigned char)idlen ;
  vv[0].iov_base = tmp ; vv[0].iov_len = 2 ;
  vv[1].iov_base = (char *)idstr ; vv[1].iov_len = idlen+1 ;
  for (unsigned int i = 0 ; i < vlen ; i++) vv[2+i] = v[i] ;
  return skaclient_putmsgv_and_close(&a->connection, &m, bits, &skabus_pub_send_cb, result) ;
}
