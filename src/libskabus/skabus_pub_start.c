/* ISC license. */

#include <skalibs/skaclient.h>

#include <skabus/pub.h>

int skabus_pub_start (skabus_pub_t *a, char const *path, tain const *deadline, tain *stamp)
{
  return skaclient_start_b(
    &a->connection,
    &a->buffers,
    path,
    SKACLIENT_OPTION_ASYNC_ACCEPT_FDS,
    SKABUS_PUB_BANNER1,
    SKABUS_PUB_BANNER1_LEN,
    SKABUS_PUB_BANNER2,
    SKABUS_PUB_BANNER2_LEN,
    deadline,
    stamp) ;
}
