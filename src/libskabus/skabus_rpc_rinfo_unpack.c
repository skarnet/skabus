/* ISC license. */

#include <string.h>

#include <skalibs/uint32.h>
#include <skalibs/uint64.h>
#include <skalibs/types.h>
#include <skalibs/tai.h>

#include <skabus/rpc.h>

void skabus_rpc_rinfo_unpack (char const *s, skabus_rpc_rinfo_t *ri)
{
  uint64_unpack_big(s, &ri->serial) ; s += 8 ;
  tain_unpack(s, &ri->limit) ; s += TAIN_PACK ;
  tain_unpack(s, &ri->timestamp) ; s += TAIN_PACK ;
  uid_unpack_big(s, &ri->uid) ; s += UID_PACK ;
  gid_unpack_big(s, &ri->gid) ; s += GID_PACK ;
  memcpy(ri->idstr, s, SKABUS_RPC_IDSTR_SIZE + 1) ;
}
