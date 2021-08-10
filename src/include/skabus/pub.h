/* ISC license. */

#ifndef SKABUS_PUB_H
#define SKABUS_PUB_H

#include <stdint.h>
#include <sys/uio.h>

#include <skalibs/uint64.h>
#include <skalibs/diuint32.h>
#include <skalibs/tai.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/unixmessage.h>
#include <skalibs/skaclient.h>


/* Misc constants */

#define SKABUS_PUB_MAX 4000
#define SKABUS_PUB_MAXFDS 4000
#define SKABUS_PUB_BANNER1 "skabus_pub v1.0 (b)\n"
#define SKABUS_PUB_BANNER1_LEN (sizeof SKABUS_PUB_BANNER1 - 1)
#define SKABUS_PUB_BANNER2 "skabus_pub v1.0 (a)\n"
#define SKABUS_PUB_BANNER2_LEN (sizeof SKABUS_PUB_BANNER2 - 1)
#define SKABUS_PUB_IDSTR_SIZE 254


 /* skabus_pub auxiliary data */

typedef struct skabus_pub_msginfo_s skabus_pub_msginfo_t, *skabus_pub_msginfo_t_ref ;
struct skabus_pub_msginfo_s
{
  uint64_t serial ;
  tain timestamp ;
  uint8_t flags ;
  char sender[SKABUS_PUB_IDSTR_SIZE + 1] ;
} ;
#define SKABUS_PUB_MSGINFO_ZERO { .serial = 0, .timestamp = TAIN_ZERO, .flags = 0, .sender = "" }


 /* internal client message storage */

typedef struct skabus_pub_cltinfo_s skabus_pub_cltinfo_t, *skabus_pub_cltinfo_t_ref ;
struct skabus_pub_cltinfo_s
{
  skabus_pub_msginfo_t msginfo ;
  int fd ;
  size_t nfds ;
  int *fds ;
} ;
#define SKABUS_PUB_CLTINFO_ZERO { .msginfo = SKABUS_PUB_MSGINFO_ZERO, .fd = -1, .nfds = 0, .fds = 0 }


 /* skabus_pub client connection */

typedef struct skabus_pub_s skabus_pub_t, *skabus_pub_t_ref ;
struct skabus_pub_s
{
  skaclient connection ;
  genalloc info ; /* array of skabus_pub_cltinfo_t */
  size_t head ;
  skaclient_buffer buffers ;
} ;
#define SKABUS_PUB_ZERO { .connection = SKACLIENT_ZERO, .info = GENALLOC_ZERO, .head = 0 }


 /* Starting and ending a session */

typedef struct skabus_pub_start_result_s skabus_pub_start_result_t, *skabus_pub_start_result_t_ref ;
struct skabus_pub_start_result_s
{
  skaclient_cbdata skaclient_cbdata ;
} ;

#define skabus_pub_init(a, path, id, sre, wre, deadline, stamp) (skabus_pub_start(a, path, deadline, stamp) && skabus_pub_register(a, id, sre, wre, deadline, stamp))
#define skabus_pub_init_g(a, path, id, sre, wre, deadline) skabus_pub_init(a, path, id, sre, wre, (deadline), &STAMP)

extern int skabus_pub_start_async (skabus_pub_t *, char const *, skabus_pub_start_result_t *) ;
extern int skabus_pub_start (skabus_pub_t *, char const *, tain const *, tain *) ;
#define skabus_pub_start_g(a, path, deadline) skabus_pub_start(a, path, (deadline), &STAMP)

extern int skabus_pub_register_async (skabus_pub_t *, char const *, char const *, char const *, unsigned char *) ;
extern int skabus_pub_register (skabus_pub_t *, char const *, char const *, char const *, tain const *, tain *) ;
#define skabus_pub_register_g(a, id, sre, wre, deadline) skabus_pub_register(a, id, sre, wre, (deadline), &STAMP)

extern void skabus_pub_end (skabus_pub_t *) ;


 /* Reading messages */

#define skabus_pub_fd(a) skaclient_fd(&(a)->connection)
extern int skabus_pub_update (skabus_pub_t *) ;
extern int skabus_pub_message_getnfds (skabus_pub_t const *) ;
extern size_t skabus_pub_message_get (skabus_pub_t *, skabus_pub_msginfo_t *, int *, int *) ;


 /* Sending public messages */

typedef struct skabus_pub_send_result_s skabus_pub_send_result_t, *skabus_pub_send_result_t_ref ;
struct skabus_pub_send_result_s
{
  uint64_t u ;
  unsigned char err ;
} ;

extern int skabus_pub_send_withfds_async (skabus_pub_t *, char const *, size_t, int const *, unsigned int, unsigned char const *, skabus_pub_send_result_t *) ;
#define skabus_pub_send_async(a, s, len, res) skabus_pub_send_withfds_async(a, s, len, 0, 0, unixmessage_bits_closenone, res)

extern uint64_t skabus_pub_send_withfds (skabus_pub_t *, char const *, size_t, int const *, unsigned int, unsigned char const *, tain const *, tain *) ;
#define skabus_pub_send_withfds_g(a, s, len, fds, nfds, bits, deadline) skabus_pub_send_withfds(a, s, len, fds, nfds, bits, (deadline), &STAMP)
#define skabus_pub_send(a, s, len, deadline, stamp) skabus_pub_send_withfds(a, s, len, 0, 0, unixmessage_bits_closenone, deadline, stamp)
#define skabus_pub_send_g(a, s, len, deadline) skabus_pub_send(a, s, len, (deadline), &STAMP)

extern int skabus_pub_sendv_withfds_async (skabus_pub_t *, struct iovec const *, unsigned int, int const *, unsigned int, unsigned char const *, skabus_pub_send_result_t *) ;
#define skabus_pub_sendv_async(a, v, vlen, res) skabus_pub_sendv_withfds_async(a, v, vlen, 0, 0, unixmessage_bits_closenone, res)

extern uint64_t skabus_pub_sendv_withfds (skabus_pub_t *, struct iovec const *, unsigned int, int const *, unsigned int, unsigned char const *, tain const *, tain *) ;
#define skabus_pub_sendv_withfds_g(a, v, vlen, fds, nfds, bits, deadline) skabus_pub_sendv_withfds(a, v, vlen, fds, nfds, bits, (deadline), &STAMP)
#define skabus_pub_sendv(a, v, vlen, deadline, stamp) skabus_pub_sendv_withfds(a, v, vlen, 0, 0, unixmessage_bits_closenone, deadline, stamp)
#define skabus_pub_sendv_g(a, v, vlen, deadline) skabus_pub_sendv(a, v, vlen, (deadline), &STAMP)


 /* Sending private messages */

extern int skabus_pub_sendpm_withfds_async (skabus_pub_t *, char const *, char const *, size_t, int const *, unsigned int, unsigned char const *, skabus_pub_send_result_t *) ;
#define skabus_pub_sendpm_async(a, id, s, len, res) skabus_pub_sendpm_withfds_async(a, id, s, len, 0, 0, unixmessage_bits_closenone, res)

extern uint64_t skabus_pub_sendpm_withfds (skabus_pub_t *, char const *, char const *, size_t, int const *, unsigned int, unsigned char const *, tain const *, tain *) ;
#define skabus_pub_sendpm_withfds_g(a, id, s, len, fds, nfds, bits, deadline) skabus_pub_sendpm_withfds(a, id, s, len, fds, nfds, bits, (deadline), &STAMP)
#define skabus_pub_sendpm(a, id, s, len, deadline, stamp) skabus_pub_sendpm_withfds(a, id, s, len, 0, 0, unixmessage_bits_closenone, deadline, stamp)
#define skabus_pub_sendpm_g(a, id, s, len, deadline) skabus_pub_sendpm(a, id, s, len, (deadline), &STAMP)

extern int skabus_pub_sendvpm_withfds_async (skabus_pub_t *, char const *, struct iovec const *, unsigned int, int const *, unsigned int, unsigned char const *, skabus_pub_send_result_t *) ;
#define skabus_pub_sendvpm_async(a, id, v, vlen, res) skabus_pub_sendvpm_withfds_async(a, id, v, vlen, 0, 0, unixmessage_bits_closenone, res)

extern uint64_t skabus_pub_sendvpm_withfds (skabus_pub_t *, char const *, struct iovec const *, unsigned int, int const *, unsigned int, unsigned char const *, tain const *, tain *) ;
#define skabus_pub_sendvpm_withfds_g(a, id, v, vlen, fds, nfds, bits, deadline) skabus_pub_sendvpm_withfds(a, id, v, vlen, fds, nfds, bits, (deadline), &STAMP)
#define skabus_pub_sendvpm(a, id, v, vlen, deadline, stamp) skabus_pub_sendvpm_withfds(a, id, v, vlen, 0, 0, unixmessage_bits_closenone, deadline, stamp)
#define skabus_pub_sendvpm_g(a, id, v, vlen, deadline) skabus_pub_sendvpm(a, id, v, vlen, (deadline), &STAMP)


 /* Subscribing to a sender */

extern int skabus_pub_subunsub_async (skabus_pub_t *, char, char const *, unsigned char *) ;
#define skabus_pub_subscribe_async(a, id, err) skabus_pub_subunsub_async(a, 'S', id, err)
#define skabus_pub_unsubscribe_async(a, id, err) skabus_pub_subunsub_async(a, 'U', id, err)
extern int skabus_pub_subunsub (skabus_pub_t *, char, char const *, tain const *, tain *) ;
#define skabus_pub_subscribe(a, id, deadline, stamp) skabus_pub_subunsub(a, 'S', id, deadline, stamp)
#define skabus_pub_subscribe_g(a, id, deadline) skabus_pub_subscribe(a, id, (deadline), &STAMP)
#define skabus_pub_unsubscribe(a, id, deadline, stamp) skabus_pub_subunsub(a, 'U', id, deadline, stamp)
#define skabus_pub_unsubscribe_g(a, id, deadline) skabus_pub_unsubscribe(a, id, (deadline), &STAMP)


 /* Listing all clients */

typedef struct skabus_pub_list_result_s skabus_pub_list_result_t, *skabus_pub_list_result_t_ref ;
struct skabus_pub_list_result_s
{
  stralloc *sa ;
  diuint32 n ;
  unsigned char err ;
} ;

extern int skabus_pub_list_async (skabus_pub_t *, stralloc *, skabus_pub_list_result_t *) ;
extern int skabus_pub_list (skabus_pub_t *, stralloc *, diuint32 *, tain const *, tain *) ;
#define skabus_pub_list_g(a, sa, deadline) skabus_pub_list(a, sa, (deadline), &STAMP)

#endif
