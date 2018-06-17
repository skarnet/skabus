/* ISC license. */

#ifndef SKABUS_RPCCCTL_H
#define SKABUS_RPCCCTL_H

#include <stdint.h>
#include <skalibs/tai.h>
#include <skalibs/stralloc.h>
#include <skalibs/textmessage.h>

typedef struct skabus_rpcc_s skabus_rpcc_t, *skabus_rpcc_t_ref ;
struct skabus_rpcc_s
{
  textmessage_sender_t out ;
  textmessage_receiver_t in ;
} ;
#define SKABUS_RPCC_ZERO { .in = TEXTMESSAGE_RECEIVER_ZERO, .out = TEXTMESSAGE_SENDER_ZERO }

extern int skabus_rpcc_start (skabus_rpcc_t *, char const *, tain_t const *, tain_t *) ;
#define skabus_rpcc_start_g(a, name, deadline) skabus_rpcc_start(a, name, (deadline), &STAMP)

extern void skabus_rpcc_end (skabus_rpcc_t *) ;

extern int skabus_rpcc_interface_register (skabus_rpcc_t *, char const *, char const *, char const *, tain_t const *, tain_t *) ;
#define skabus_rpcc_interface_register_g(a, ifname, ifprog, re, deadline) skabus_rpcc_interface_register(a, ifname, ifprog, re, (deadline), &STAMP)

extern int skabus_rpcc_interface_unregister (skabus_rpcc_t *, char const *, tain_t const *, tain_t *) ;
#define skabus_rpcc_interface_unregister_g(a, ifname, deadline) skabus_rpcc_interface_unregister(a, ifname, (deadline), &STAMP)

extern int skabus_rpcc_query (skabus_rpcc_t *, stralloc *, char const *, char const *, uint32_t, tain_t const *, tain_t *) ;
#define skabus_rpcc_query_g(a, reply, ifname, query, timeout, deadline) skabus_rpcc_query(a, reply, ifname, query, timeout, (deadline), &STAMP)

extern int skabus_rpcc_quit (skabus_rpcc_t *, tain_t const *, tain_t *) ;
#define skabus_rpcc_quit_g(a, deadline) skabus_rpcc_quit(a, (deadline), &STAMP)

#endif
