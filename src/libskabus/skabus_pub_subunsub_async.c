/* ISC license. */

#include <string.h>
#include <sys/uio.h>
#include <errno.h>

#include <skalibs/skaclient.h>

#include <skabus/pub.h>

int skabus_pub_subunsub_async (skabus_pub_t *a, char what, char const *id, unsigned char *err)
{
  size_t idlen = strlen(id) ;
  char pack[2] = { what, (unsigned char)idlen } ;
  struct iovec v[2] =
  {
    { .iov_base = pack, .iov_len = 2 },
    { .iov_base = (char *)id, .iov_len = idlen+1 }
  } ;
  if (idlen > SKABUS_PUB_IDSTR_SIZE) return (errno = ERANGE, 0) ;
  return skaclient_putv(&a->connection, v, 2, &skaclient_default_cb, err) ;
}
