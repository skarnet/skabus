/* ISC license. */

#include <errno.h>

#include <skalibs/skaclient.h>

#include <skabus/pub.h>

uint64_t skabus_pub_sendvpm_withfds (skabus_pub_t *a, char const *idstr, struct iovec const *v, unsigned int vlen, int const *fds, unsigned int nfds, unsigned char const *bits, tain const *deadline, tain *stamp)
{
  skabus_pub_send_result_t r ;
  if (!skabus_pub_sendvpm_withfds_async(a, idstr, v, vlen, fds, nfds, bits, &r)) return 0 ;
  if (!skaclient_syncify(&a->connection, deadline, stamp)) return 0 ;
  return r.err ? (errno = r.err, 0) : r.u ;
}
