/* ISC license. */

#ifndef SKABUS_RPCD_H
#define SKABUS_RPCD_H

#include <sys/types.h>
#include <stdint.h>
#include <regex.h>
#include <skalibs/uint64.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/genalloc.h>
#include <skalibs/genset.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/unixmessage.h>
#include <skalibs/unixconnection.h>
#include <skabus/rpc.h>

#define X() strerr_dief1x(101, "unexpected error - please submit a bug-report.") ;


 /*
    query: queries accepted from client, sent to interface
    The list is stored in a gensetdyn.
    Looked up by serial for answers or cancels.
 */

typedef struct query_s query_t, *query_t_ref ;
struct query_s
{
  uint64_t serial ;
  tain deadline ;
  uint32_t client ;
  uint32_t clientindex ;
  uint32_t interface ;
  uint32_t interfaceindex ;
} ;

#define QUERY_ZERO \
{ \
  .serial = 0, \
  .deadline = TAIN_ZERO, \
  .client = 0, \
  .clientindex = 0, \
  .interface = 0, \
  .interfaceindex = 0 \
}


 /*
    interfaces: registered R interfaces.
    The list is stored in a gensetdyn.
    Looked up by name.
 */

typedef struct interface_s interface_t, *interface_t_ref ;
struct interface_s
{
  char name[SKABUS_RPC_INTERFACE_MAXLEN+1] ;
  regex_t re ; /* clients who can access that interface */
  uint32_t id ;
  uint32_t client ;
  uint32_t index ; /* in the owner's interfaces list */
  gensetdyn queries ; /* uint32_t */
} ;
#define INTERFACE_ZERO { .name = "", .id = 0, .client = 0, .index = 0, .queries = GENSETDYN_ZERO }


 /*
    client: client connections.
    The list is stored in a genset.
    List browsed at every iopause iteration, so needs a next field.
 */

typedef struct client_s client_t, *client_t_ref ;
struct client_s
{
  uint32_t next ;
  uid_t uid ;
  gid_t gid ;
  tain deadline ;
  genalloc interfaces ; /* uint32_t */
  gensetdyn queries ; /* uint32_t */
  unixconnection sync ;
  unixconnection async ;
  uint32_t xindex[2] ;
  regex_t idstr_re ;
  regex_t interfaces_re ;
} ;
#define CLIENT_ZERO \
{ \
  .next = 0, \
  .uid = (uid_t)-1, \
  .gid = (gid_t)-1, \
  .deadline = TAIN_ZERO, \
  .interfaces = GENALLOC_ZERO, \
  .queries = GENALLOC_ZERO, \
  .sync = UNIXCONNECTION_ZERO, \
  .async = UNIXCONNECTION_ZERO, \
  .xindex = { 0, 0 }, \
}

extern gensetdyn queries ;
#define QUERY(i) GENSETDYN_P(query_t, &queries, (i))
#define queries_pending() gensetdyn_n(&queries)

extern gensetdyn interfaces ;
#define INTERFACE(i) GENSETDYN_P(interface_t, &interfaces, (i))

extern genset *clients ;
extern unsigned int sentinel ;
#define CLIENT(i) genset_p(client_t, clients, (i))
#define numconn (genset_n(clients) - 1)

extern void query_remove (uint32_t) ;
extern void query_fail (uint32_t, unsigned char) ;
extern int query_cancel (uint32_t, unsigned char) ;
extern int query_cancelremove (uint32_t, unsigned char) ;
extern int query_lookup_by_serial (uint64_t, uint32_t *) ;
extern int query_lookup_by_mindeadline (uint32_t *) ;
extern void query_get_mindeadline (tain *) ;
extern int query_add (uint32_t *, tain const *, uint32_t, uint32_t) ;
extern int query_send (uint32_t, unixmessage const *) ;
extern int query_sendpm (uint32_t, unixmessage const *) ;
extern void query_reply (uint32_t, char, unixmessage const *) ;

extern void interface_remove (uint32_t) ;
extern int interface_lookup_by_name (char const *, uint32_t *) ;
extern int interface_add (uint32_t *, char const *, size_t, uint32_t, char const *, uint32_t) ;

#define client_isregistered(cc) genalloc_len(uint32_t, &CLIENT(cc)->interfaces)
#define client_idstr(c) (INTERFACE(genalloc_s(uint32_t, &(c)->interfaces)[0])->name + 1)
extern void client_remove (uint32_t, uint32_t) ;
extern void client_add (uint32_t *, regex_t const *, regex_t const *, uid_t, gid_t, int, uint32_t) ;
extern void client_nextdeadline (uint32_t, tain *) ;
extern void client_setdeadline (client_t *) ;
extern int client_prepare_iopause (uint32_t, tain *, iopause_fd *, uint32_t *, int) ;
extern int client_flush (uint32_t, iopause_fd const *) ;
extern int client_read (uint32_t, iopause_fd const *) ;

extern int parse_protocol_sync (unixmessage const *, void *) ;
extern int parse_protocol_async (unixmessage const *, void *) ;

extern tain answertto ;

#endif
