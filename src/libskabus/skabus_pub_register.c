/* ISC license. */

#include <errno.h>

#include <skalibs/skaclient.h>

#include <skabus/pub.h>

int skabus_pub_register (skabus_pub_t *a, char const *id, char const *sre, char const *wre, tain_t const *deadline, tain_t *stamp)
{
  unsigned char r ;
  if (!skabus_pub_register_async(a, id, sre, wre, &r)) return 0 ;
  if (!skaclient_syncify(&a->connection, deadline, stamp)) return 0 ;
  return r ? (errno = r, 0) : 1 ;
}
