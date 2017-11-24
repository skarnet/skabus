/* ISC license. */

#include <string.h>
#include <skalibs/uint32.h>
#include <skalibs/uint64.h>
#include <skalibs/types.h>
#include <skalibs/tai.h>
#include <skabus/rpc.h>

void skabus_rpc_rinfo_pack (char *s, skabus_rpc_rinfo_t const *ri)
{
  uint64_pack_big(s, ri->serial) ; s += 8 ;
  tain_pack(s, &ri->limit) ; s += TAIN_PACK ;
  tain_pack(s, &ri->timestamp) ; s += TAIN_PACK ;
  uid_pack_big(s, ri->uid) ; s += UID_PACK ;
  gid_pack_big(s, ri->gid) ; s += GID_PACK ;
  memcpy(s, ri->idstr, SKABUS_RPC_IDSTR_SIZE + 1) ;
}
