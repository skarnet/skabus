/* ISC license. */

#ifndef SKABUS_RPC_INTERNAL_H
#define SKABUS_RPC_INTERNAL_H

#include <sys/uio.h>
#include <skalibs/uint64.h>
#include <skalibs/tai.h>
#include <skalibs/unixmessage.h>
#include <skabus/rpc.h>

extern int skabus_rpc_sendq_withfds_async (skabus_rpc_t *, char const *, size_t, char const *, char const *, size_t, int const *, unsigned int, unsigned char const *, tain_t const *, skabus_rpc_send_result_t *) ;
extern uint64_t skabus_rpc_sendq_withfds (skabus_rpc_t *, char const *, size_t, char const *, char const *, size_t, int const *, unsigned int, unsigned char const *, tain_t const *, tain_t const *, tain_t *) ;
extern int skabus_rpc_sendvq_withfds_async (skabus_rpc_t *, char const *, size_t, char const *, struct iovec const *, unsigned int, int const *, unsigned int, unsigned char const *, tain_t const *, skabus_rpc_send_result_t *) ;
extern uint64_t skabus_rpc_sendvq_withfds (skabus_rpc_t *, char const *, size_t, char const *, struct iovec const *, unsigned int, int const *, unsigned int, unsigned char const *, tain_t const *, tain_t const *, tain_t *) ;

extern unixmessage_handler_func_t skabus_rpc_send_cb ;
extern unixmessage_handler_func_t skabus_rpc_interface_register_cb ;

#endif
