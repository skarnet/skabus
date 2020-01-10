/* ISC license. */

#include <skalibs/genalloc.h>

#include <skabus/pub.h>

int skabus_pub_message_getnfds (skabus_pub_t const *a)
{
  return genalloc_len(skabus_pub_cltinfo_t, &a->info) <= a->head ? -1 :
   (int)genalloc_s(skabus_pub_cltinfo_t, &a->info)[a->head].nfds ;
}
