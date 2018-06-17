/* ISC license. */

#ifndef SKABUS_RPC_H
#define SKABUS_RPC_H

#include <sys/types.h>
#include <sys/uio.h>
#include <stdint.h>
#include <errno.h>
#include <skalibs/config.h>
#include <skalibs/uint64.h>
#include <skalibs/types.h>
#include <skalibs/tai.h>
#include <skalibs/genalloc.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/avltree.h>
#include <skalibs/skaclient.h>


/* Misc constants */

#define SKABUS_RPC_MAX 1000
#define SKABUS_RPC_BANNER1 "skabus-rpc v1.0 (b)\n"
#define SKABUS_RPC_BANNER1_LEN (sizeof SKABUS_RPC_BANNER1 - 1)
#define SKABUS_RPC_BANNER2 "skabus-rpc v1.0 (a)\n"
#define SKABUS_RPC_BANNER2_LEN (sizeof SKABUS_RPC_BANNER2 - 1)
#define SKABUS_RPC_IDSTR_SIZE 254
#define SKABUS_RPC_INTERFACE_MAXLEN 255
#define SKABUS_RPC_RE_MAXLEN 1023

typedef struct skabus_rpc_s skabus_rpc_t, *skabus_rpc_t_ref ;

 /* Additional data transmitted to methods */

typedef struct skabus_rpc_rinfo_s skabus_rpc_rinfo_t, *skabus_rpc_rinfo_t_ref ;
struct skabus_rpc_rinfo_s
{
  uint64_t serial ;
  tain_t limit ;
  tain_t timestamp ;
  uid_t uid ;
  gid_t gid ;
  char idstr[SKABUS_RPC_IDSTR_SIZE + 1] ;
} ;
#define SKABUS_RPC_RINFO_ZERO { .serial = 0, .limit = TAIN_ZERO, .timestamp = TAIN_ZERO, .uid = -1, .gid = -1, .idstr = "" }
extern skabus_rpc_rinfo_t const skabus_rpc_rinfo_zero ;

#define SKABUS_RPC_RINFO_PACK (9 + (TAIN_PACK << 1) + UID_PACK + GID_PACK + SKABUS_RPC_IDSTR_SIZE)
extern void skabus_rpc_rinfo_pack (char *, skabus_rpc_rinfo_t const *) ;
extern void skabus_rpc_rinfo_unpack (char const *, skabus_rpc_rinfo_t *) ;


 /* Serving queries */

typedef int skabus_rpc_r_func_t (skabus_rpc_t *, skabus_rpc_rinfo_t const *, unixmessage_t const *, void *) ;
typedef skabus_rpc_r_func_t *skabus_rpc_r_func_t_ref ;
typedef int skabus_rpc_rcancel_func_t (uint64_t, char, void *) ;
typedef skabus_rpc_rcancel_func_t *skabus_rpc_rcancel_func_t_ref ;

typedef struct skabus_rpc_interface_s skabus_rpc_interface_t, *skabus_rpc_interface_t_ref ;
struct skabus_rpc_interface_s
{
  skabus_rpc_r_func_t_ref f ;
  skabus_rpc_rcancel_func_t_ref cancelf ;
  void *data ;
} ;
#define SKABUS_RPC_INTERFACE_ZERO { .f = &skabus_rpc_r_notimpl, .cancelf = &skabus_rpc_rcancel_ignore, .data = 0 }
extern skabus_rpc_interface_t const skabus_rpc_interface_zero ;

extern skabus_rpc_r_func_t skabus_rpc_r_notimpl ;
extern skabus_rpc_rcancel_func_t skabus_rpc_rcancel_ignore ;

extern int skabus_rpc_reply_withfds_async (skabus_rpc_t *, uint64_t, char, char const *, size_t, int const *, unsigned int, unsigned char const *) ;
#define skabus_rpc_reply_async(a, serial, result, s, len) skabus_rpc_reply_withfds(a, serial, result, s, len, 0, 0, unixmessage_bits_closenone)
extern int skabus_rpc_replyv_withfds_async (skabus_rpc_t *, uint64_t, char, struct iovec const *, unsigned int, int const *, unsigned int, unsigned char const *) ;
#define skabus_rpc_replyv_async(a, serial, result, v, vlen) skabus_rpc_replyv_withfds(a, serial, result, v, vlen, 0, 0, unixmessage_bits_closenone)

extern int skabus_rpc_reply_withfds (skabus_rpc_t *, uint64_t, char, char const *, size_t, int const *, unsigned int, unsigned char const *, tain_t const *, tain_t *) ;
#define skabus_rpc_reply(a, serial, result, s, len, deadline, stamp) skabus_rpc_reply_withfds(a, serial, result, s, len, 0, 0, unixmessage_bits_closenone, deadline, stamp)
extern int skabus_rpc_replyv_withfds (skabus_rpc_t *, uint64_t, char, struct iovec const *, unsigned int, int const *, unsigned int, unsigned char const *, tain_t const *, tain_t *) ;
#define skabus_rpc_replyv(a, serial, result, v, vlen, deadline, stamp) skabus_rpc_replyv_withfds(a, serial, result, v, vlen, 0, 0, unixmessage_bits_closenone, deadline, stamp)

#define skabus_rpc_reply_withfds_g(a, serial, result, s, len, fds, nfds, bits, deadline) skabus_rpc_reply_withfds(a, serial, result, s, len, fds, nfds, bits, (deadline), &STAMP)
#define skabus_rpc_reply_g(a, serial, result, s, len, deadline) skabus_rpc_reply(a, serial, result, s, len, (deadline), &STAMP)
#define skabus_rpc_replyv_withfds_g(a, serial, result, v, vlen, fds, nfds, bits, deadline) skabus_rpc_replyv_withfds(a, serial, result, v, vlen, fds, nfds, bits, (deadline), &STAMP)
#define skabus_rpc_replyv_g(a, serial, result, v, vlen, deadline) skabus_rpc_replyv(a, serial, result, v, vlen, (deadline), &STAMP)


 /* Internal client interface storage */

typedef struct skabus_rpc_ifnode_s skabus_rpc_ifnode_t, *skabus_rpc_ifnode_t_ref ;
struct skabus_rpc_ifnode_s
{
  char name[SKABUS_RPC_INTERFACE_MAXLEN + 1] ;
  skabus_rpc_interface_t body ;
} ;
#define SKABUS_RPC_IFNODE_ZERO { .name = "", .body = SKABUS_RPC_INTERFACE_ZERO }


 /* Internal client query storage */

typedef struct skabus_rpc_qinfo_s skabus_rpc_qinfo_t, *skabus_rpc_qinfo_t_ref ;
struct skabus_rpc_qinfo_s
{
  uint64_t serial ;
  char status ;
  char result ;
  unixmessage_t message ;
} ;
#define SKABUS_RPC_QINFO_ZERO { .serial = 0, .status = EINVAL, .result = ECONNABORTED, .message = UNIXMESSAGE_ZERO }
extern skabus_rpc_qinfo_t const skabus_rpc_qinfo_zero ;


 /* Client handle */

struct skabus_rpc_s
{
  skaclient_t connection ;
  uint32_t pmid ; /* index of the object interface */
  gensetdyn r ; /* set of skabus_rpc_ifnode_t */
  genalloc qlist ; /* array of uint64_t */
  gensetdyn q ; /* set of skabus_rpc_qinfo_t */
  avltree qmap ; /* serial -> index in q */
  skaclient_buffer_t buffers ;
} ;
#define SKABUS_RPC_ZERO \
{ \
  .connection = SKACLIENT_ZERO, \
  .pmid = (uint32_t)-1, \
  .r = GENSETDYN_ZERO, \
  .qlist = GENALLOC_ZERO, \
  .q = GENSETDYN_ZERO, \
  .qmap = AVLTREE_ZERO \
}

                                                                                 
/* Starting and ending a session */

typedef struct skabus_rpc_interface_result_s skabus_rpc_interface_result_t, *skabus_rpc_interface_result_t_ref ;

typedef struct skabus_rpc_start_result_s skabus_rpc_start_result_t, *skabus_rpc_start_result_t_ref ;
struct skabus_rpc_start_result_s
{
  skaclient_cbdata_t skaclient_cbdata ;
} ;

#define skabus_rpc_init(a, path, id, ifbody, re, deadline, stamp) (skabus_rpc_start(a, path, deadline, stamp) && skabus_rpc_idstr(a, id, ifbody, re, deadline, stamp))
#define skabus_rpc_init_g(a, path, id, ifbody, re, deadline) skabus_rpc_init(a, path, id, ifbody, re, (deadline), &STAMP)

extern int skabus_rpc_start_async (skabus_rpc_t *, char const *, skabus_rpc_start_result_t *) ;
extern int skabus_rpc_start (skabus_rpc_t *, char const *, tain_t const *, tain_t *) ;
#define skabus_rpc_start_g(a, path, deadline) skabus_rpc_start(a, path, (deadline), &STAMP)

extern int skabus_rpc_idstr_async (skabus_rpc_t *, char const *, skabus_rpc_interface_t const *, char const *, skabus_rpc_interface_result_t *) ;
extern int skabus_rpc_idstr (skabus_rpc_t *, char const *, skabus_rpc_interface_t const *, char const *, tain_t const *, tain_t *) ;
#define skabus_rpc_idstr_g(a, idstr, ifbody, re, deadline) skabus_rpc_idstr(a, idstr, ifbody, re, (deadline), &STAMP)

extern void skabus_rpc_end (skabus_rpc_t *) ;


 /* Getting results */

#define skabus_rpc_fd(a) skaclient_fd(&(a)->connection)
extern int skabus_rpc_update (skabus_rpc_t *) ;
extern size_t skabus_rpc_qlist (skabus_rpc_t *, uint64_t **) ;
extern int skabus_rpc_get (skabus_rpc_t *, uint64_t, int *, unixmessage_t *) ;
extern int skabus_rpc_release (skabus_rpc_t *, uint64_t) ;
extern void skabus_rpc_qlist_ack(skabus_rpc_t *, size_t) ;


 /* Registering an interface */

struct skabus_rpc_interface_result_s
{
  gensetdyn *r ;
  uint32_t ifid ;
  unsigned char err ;
} ;

extern int skabus_rpc_interface_register_async (skabus_rpc_t *, char const *, skabus_rpc_interface_t const *, char const *, skabus_rpc_interface_result_t *) ;
extern int skabus_rpc_interface_register (skabus_rpc_t *, uint32_t *, char const *, skabus_rpc_interface_t const *, char const *, tain_t const *, tain_t *) ;
#define skabus_rpc_interface_register_g(a, ifid, ifname, ifbody, re, deadline) skabus_rpc_interface_register(a, ifid, ifname, ifbody, re, (deadline), &STAMP)

extern int skabus_rpc_interface_unregister_async (skabus_rpc_t *, uint32_t, skabus_rpc_interface_result_t *) ;
extern int skabus_rpc_interface_unregister (skabus_rpc_t *, uint32_t, tain_t const *, tain_t *) ;
#define skabus_rpc_interface_unregister_g(a, ifid, deadline) skabus_rpc_interface_unregister(a, ifid, (deadline), &STAMP)


 /* Sending a query */

typedef struct skabus_rpc_send_result_s skabus_rpc_send_result_t, *skabus_rpc_send_result_t_ref ;
struct skabus_rpc_send_result_s
{
  skabus_rpc_t *a ;
  uint64_t u ;
  uint32_t i ;
  unsigned char err ;
} ;

extern int skabus_rpc_send_withfds_async (skabus_rpc_t *, char const *, char const *, size_t, int const *, unsigned int, unsigned char const *, tain_t const *, skabus_rpc_send_result_t *) ;
#define skabus_rpc_send_async(a, ifname, s, len, limit, r) skabus_rpc_send_withfds_async(a, ifname, s, len, 0, 0, unixmessage_bits_closenone, limit, r)

extern uint64_t skabus_rpc_send_withfds (skabus_rpc_t *, char const *, char const *, size_t, int const *, unsigned int, unsigned char const *, tain_t const *, tain_t const *, tain_t *) ;
#define skabus_rpc_send_withfds_g(a, ifname, s, len, fds, nfds, bits, limit, deadline) skabus_rpc_send_withfds(a, ifname, s, len, fds, nfds, bits, limit, (deadline), &STAMP)
#define skabus_rpc_send(a, ifname, s, len, limit, deadline, stamp) skabus_rpc_send_withfds(a, ifname, s, len, 0, 0, unixmessage_bits_closenone, limit, deadline, stamp)
#define skabus_rpc_send_g(a, ifname, s, len, limit, deadline) skabus_rpc_send(a, ifname, s, len, limit, (deadline), &STAMP)

extern int skabus_rpc_sendv_withfds_async (skabus_rpc_t *, char const *, struct iovec const *, unsigned int, int const *, unsigned int, unsigned char const *, tain_t const *, skabus_rpc_send_result_t *) ;
#define skabus_rpc_sendv_async(a, ifname, v, vlen, limit, r) skabus_rpc_sendv_withfds_async(a, ifname, v, vlen, 0, 0, unixmessage_bits_closenone, limit, r)

extern uint64_t skabus_rpc_sendv_withfds (skabus_rpc_t *, char const *, struct iovec const *, unsigned int, int const *, unsigned int, unsigned char const *, tain_t const *, tain_t const *, tain_t *) ;
#define skabus_rpc_sendv_withfds_g(a, ifname, v, vlen, fds, nfds, bits, limit, deadline) skabus_rpc_sendv_withfds(a, ifname, v, vlen, fds, nfds, bits, limit, (deadline), &STAMP)
#define skabus_rpc_sendv(a, ifname, v, vlen, limit, deadline, stamp) skabus_rpc_sendv_withfds(a, ifname, v, vlen, 0, 0, unixmessage_bits_closenone, limit, deadline, stamp)
#define skabus_rpc_sendv_g(a, ifname, v, vlen, limit, deadline) skabus_rpc_sendv(a, ifname, v, vlen, limit, (deadline), &STAMP)

extern int skabus_rpc_sendpm_withfds_async (skabus_rpc_t *, char const *, char const *, size_t, int const *, unsigned int, unsigned char const *, tain_t const *, skabus_rpc_send_result_t *) ;
#define skabus_rpc_sendpm_async(a, cname, s, len, limit, r) skabus_rpc_sendpm_withfds_async(a, cname, s, len, 0, 0, unixmessage_bits_closenone, limit, r)

extern uint64_t skabus_rpc_sendpm_withfds (skabus_rpc_t *, char const *, char const *, size_t, int const *, unsigned int, unsigned char const *, tain_t const *, tain_t const *, tain_t *) ;
#define skabus_rpc_sendpm_withfds_g(a, cname, s, len, fds, nfds, bits, limit, deadline) skabus_rpc_sendpm_withfds(a, cname, s, len, fds, nfds, bits, limit, (deadline), &STAMP)
#define skabus_rpc_sendpm(a, cname, s, len, limit, deadline, stamp) skabus_rpc_sendpm_withfds(a, cname, s, len, 0, 0, unixmessage_bits_closenone, limit, deadline, stamp)
#define skabus_rpc_sendpm_g(a, cname, s, len, limit, deadline) skabus_rpc_sendpm(a, cname, s, len, limit, (deadline), &STAMP)

extern int skabus_rpc_sendvpm_withfds_async (skabus_rpc_t *, char const *, struct iovec const *, unsigned int, int const *, unsigned int, unsigned char const *, tain_t const *, skabus_rpc_send_result_t *) ;
#define skabus_rpc_sendvpm_async(a, cname, v, vlen, limit, r) skabus_rpc_sendvpm_withfds_async(a, cname, v, vlen, 0, 0, unixmessage_bits_closenone, limit, r)

extern uint64_t skabus_rpc_sendvpm_withfds (skabus_rpc_t *, char const *, struct iovec const *, unsigned int, int const *, unsigned int, unsigned char const *, tain_t const *, tain_t const *, tain_t *) ;
#define skabus_rpc_sendvpm_withfds_g(a, cname, v, vlen, fds, nfds, bits, limit, deadline) skabus_rpc_sendvpm_withfds(a, cname, v, vlen, fds, nfds, bits, limit, (deadline), &STAMP)
#define skabus_rpc_sendvpm(a, cname, v, vlen, limit, deadline, stamp) skabus_rpc_sendvpm_withfds(a, cname, v, vlen, 0, 0, unixmessage_bits_closenone, limit, deadline, stamp)
#define skabus_rpc_sendvpm_g(a, cname, v, vlen, limit, deadline) skabus_rpc_sendvpm(a, cname, v, vlen, limit, (deadline), &STAMP)


 /* Cancelling an in-flight query */

extern int skabus_rpc_cancel_async (skabus_rpc_t *, uint64_t, unsigned char *) ;
extern int skabus_rpc_cancel (skabus_rpc_t *, uint64_t, tain_t const *, tain_t *) ;
#define skabus_rpc_cancel_g(a, qid, deadline) skabus_rpc_cancel(a, qid, (deadine), &STAMP)

#endif
