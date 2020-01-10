/* ISC license. */

#include <string.h>
#include <errno.h>
#include <sys/uio.h>

#include <skalibs/uint32.h>
#include <skalibs/skaclient.h>

#include <skabus/pub.h>

int skabus_pub_register_async (skabus_pub_t *a, char const *id, char const *sre, char const *wre, unsigned char *err)
{
  size_t idlen = strlen(id) ;
  size_t srelen = strlen(sre) ;
  size_t wrelen = strlen(wre) ;
  char pack[10] = "R" ;
  struct iovec v[4] =
  {
    { .iov_base = pack, .iov_len = 10 },
    { .iov_base = (char *)id, .iov_len = idlen+1 },
    { .iov_base = (char *)sre, .iov_len = srelen+1 },
    { .iov_base = (char *)wre, .iov_len = wrelen+1 }
  } ;
  if (idlen > SKABUS_PUB_IDSTR_SIZE) return (errno = ERANGE, 0) ;
  pack[1] = (unsigned char)idlen ;
  uint32_pack_big(pack+2, srelen) ;
  uint32_pack_big(pack+6, wrelen) ;
  return skaclient_putv(&a->connection, v, 4, &skaclient_default_cb, err) ;
}
