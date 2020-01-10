/* ISC license. */

#include <errno.h>

#include <skalibs/skaclient.h>

#include <skabus/pub.h>

int skabus_pub_list (skabus_pub_t *a, stralloc *sa, diuint32 *n, tain_t const *deadline, tain_t *stamp)
{
  skabus_pub_list_result_t r ;
  if (!skabus_pub_list_async(a, sa, &r)) return 0 ;
  if (!skaclient_syncify(&a->connection, deadline, stamp)) return 0 ;
  if (r.err) return (errno = r.err, 0) ;
  *n = r.n ;
  return 1 ;
}
