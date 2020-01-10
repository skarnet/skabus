/* ISC license. */

#include <errno.h>

#include <skalibs/skaclient.h>

#include <skabus/pub.h>

int skabus_pub_subunsub (skabus_pub_t *a, char what, char const *id, tain_t const *deadline, tain_t *stamp)
{
  unsigned char r ;
  if (!skabus_pub_subunsub_async(a, what, id, &r)) return 0 ;
  if (!skaclient_syncify(&a->connection, deadline, stamp)) return 0 ;
  return r ? (errno = r, 0) : 1 ;
}
