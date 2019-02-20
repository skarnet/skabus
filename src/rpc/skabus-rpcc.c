/* ISC license. */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <limits.h>
#include <regex.h>
#include <errno.h>
#include <signal.h>

#include <skalibs/posixishard.h>
#include <skalibs/posixplz.h>
#include <skalibs/uint32.h>
#include <skalibs/uint64.h>
#include <skalibs/types.h>
#include <skalibs/bytestr.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/error.h>
#include <skalibs/cdb.h>
#include <skalibs/strerr2.h>
#include <skalibs/selfpipe.h>
#include <skalibs/sig.h>
#include <skalibs/stralloc.h>
#include <skalibs/bufalloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/sgetopt.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/env.h>
#include <skalibs/webipc.h>
#include <skalibs/gensetdyn.h>
#include <skalibs/genqdyn.h>
#include <skalibs/skamisc.h>
#include <execline/config.h>

#include <skabus/rpc.h>
#include "skabus-rpcc.h"

#define USAGE "skabus-rpcc [ -v verbosity ] [ -1 ] [ -c maxconn ] [ -t timeout ] [ -T lameducktimeout ] [ -C pmprog:sep:flags ] [ -y ifname:ifprog:sep:flags ... ] rpcdpath clientname"
#define dieusage() strerr_dieusage(100, USAGE) ;

static tain_t answertto = TAIN_INFINITE_RELATIVE ;

static unsigned int verbosity = 1 ;
static int cont = 1 ;
static tain_t lameduckdeadline = TAIN_INFINITE_RELATIVE ;
static skabus_rpc_t a = SKABUS_RPC_ZERO ;

static void quit (void)
{
  if (cont)
  {
    cont = 0 ;
    tain_add_g(&lameduckdeadline, &lameduckdeadline) ;
  }
}


 /* Interfaces */

typedef struct interface_s interface_t, *interface_t_ref ;
struct interface_s
{
  uint32_t next ;
  unsigned int xindex[2] ;
  stralloc storage ;
  pid_t pid ;
  uint32_t id ;
  buffer in ;
  stralloc insa ;
  bufalloc out ;
  tain_t tto ;
  tain_t deadline ;
  genqdyn serialq ;
  char sep[2] ;
  char buf[SKABUS_RPCC_BUFSIZE] ;
} ;

static gensetdyn interfaces = GENSETDYN_INIT(interface_t, 1, 0, 1) ;
#define numinterfaces (gensetdyn_n(&interfaces) - 1)
#define INTERFACE(i) GENSETDYN_P(interface_t, &interfaces, (i))
static uint32_t interface_sentinel ;

static inline void interface_free (interface_t *y)
{
  fd_close(bufalloc_fd(&y->out)) ;
  fd_close(buffer_fd(&y->in)) ;
  genqdyn_free(&y->serialq) ;
  bufalloc_free(&y->out) ;
  stralloc_free(&y->insa) ;
  stralloc_free(&y->storage) ;
  kill(y->pid, SIGTERM) ;
}

static inline void interface_delete (uint32_t i, uint32_t prev)
{
  interface_t *y = INTERFACE(i) ;
  INTERFACE(prev)->next = y->next ;
  interface_free(y) ;
  gensetdyn_delete(&interfaces, i) ; 
}

static int rclient_function (skabus_rpc_t *a, skabus_rpc_rinfo_t const *info, unixmessage_t const *m, void *aux)
{
  tain_t deadline ;
  interface_t *y = aux ;
  if (m->nfds) { unixmessage_drop(m) ; return (errno = EPROTO, 0) ; }
  if (cont)
  {
    if (!genqdyn_push(&y->serialq, &info->serial)) return 0 ;
    if (!bufalloc_put(&y->out, m->s, m->len)) goto err ;
    if (!bufalloc_put(&y->out, &y->sep[0], 1)) goto berr ;
    tain_add_g(&deadline, &y->tto) ;
    if (tain_less(&deadline, &y->deadline)) y->deadline = deadline ;
    return 1 ;
  }
  else
  {
    tain_uint(&deadline, 2) ;
    tain_add_g(&deadline, &deadline) ;
    return skabus_rpc_reply_g(a, info->serial, ESRCH, "", 0, &deadline) ; 
  }
 berr:
  bufalloc_unput(&y->out, m->len) ;
 err:
  genqdyn_unpush(&y->serialq) ;
  return 0 ;
}

static int rclient_cancel_function (uint64_t serial, char reason, void *aux)
{
  (void)serial ;
  (void)reason ;
  (void)aux ;
  return 1 ;
}

static int interface_add (char const *ifname, char const *ifprog, char const *re, unsigned int milli, char const *sep)
{
  uint32_t yy ;
  interface_t *y ;
  int fd[2] ;
  tain_t deadline ;
  skabus_rpc_interface_t ifbody = { .f = &rclient_function, .cancelf = &rclient_cancel_function, .aux = y }
  char const *argv[4] = { EXECLINE_EXTBINPREFIX "execlineb", "-c", ifprog, 0 } ; 
  pid_t pid = child_spawn2(argv[0], argv, (char const *const *)environ, fd) ;
  if (!pid) return 0 ;
  if (!gensetdyn_new(&interfaces, &yy)) goto err ;
  y = INTERFACE(yy) ;
  y->storage = stralloc_zero ;
  if (!stralloc_catb(&y->storage, ifname, strlen(ifname)+1)) goto err ;
  tain_uint(&deadline, 2) ;
  tain_add_g(&deadline, &deadline) ;
  if (!skabus_rpc_interface_register_g(a, &y->id, ifname, &ifbody, re, &deadline)) goto ferr ;
  y->pid = pid ;
  buffer_init(&y->in, &buffer_read, fd[0], y->buf, SKABUS_RPCC_BUFSIZE) ;
  y->insa = stralloc_zero ;
  bufalloc_init(&y->out, &fd_write, fd[1]) ;
  tain_from_millisecs(&y->tto, milli) ;
  tain_add_g(&y->deadline, tain_infinite_relative) ;
  genqdyn_init(&y->serialq, sizeof(uint64_t), 3, 8) ;
  y->sep[0] = sep[0] ;
  y->sep[1] = sep[1] ;
  y->next = INTERFACE(interface_sentinel)->next ;
  INTERFACE(interface_sentinel)->next = yy ;
  return 1 ;

 ferr:
  stralloc_free(&y->storage) ;
 err:
  fd_close(fd[1]) ;
  fd_close(fd[0]) ;
  return 0 ;
}

static inline int interface_lookup_by_name (char const *ifname, uint32_t *n, uint32_t *nprev)
{
  uint32_t i = interface_sentinel->next, prev = interface_sentinel ;
  while (i != interface_sentinel)
  {
    interface_t *y = INTERFACE(i) ;
    if (!strcmp(y->storage.s, ifname))
    {
      *n = i ;
      *nprev = prev ;
      return 1 ;
    }
    prev = i ;
    i = y->next ;
  }
  return 0 ;
}

static int interface_remove (char const *ifname)
{
  tain_t deadline ;
  uint32_t i, prev, id ;
  if (!interface_lookup_by_name(ifname, &i, &prev)) return 0 ;
  id = INTERFACE(i)->id ;
  interface_delete(i, prev) ;
  tain_uint(&deadline, 2) ;
  tain_add_g(&deadline, &deadline) ;
  return skabus_rpc_interface_unregister_g(a, INTERFACE(i)->id, &deadline) ;
}

static inline int interface_prepare_iopause (uint32_t i, tain_t *deadline, iopause_fd *x, uint32_t *j)
{
  interface_t *y = INTERFACE(i) ;
  if (tain_less(&y->deadline, deadline)) *deadline = y->deadline ;
  if (!bufalloc_isempty(&y->out))
  {
    x[*j].fd = bufalloc_fd(&y->out) ;
    x[*j].events = IOPAUSE_WRITE ;
    y->xindex[0] = (*j)++ ;
  }
  else y->xindex[0] = 0 ;
  if (genqdyn_n(&y->serialq))
  {
    x[*j].fd = buffer_fd(&y->in) ;
    x[*j].events = IOPAUSE_READ ;
    y->xindex[1] = (*j)++ ;
  }
  else y->xindex[1] = 0 ;
  return y->xindex[0] || y->xindex[1] ;
}

static inline int interface_event (iopause_fd const *x, uint32_t i)
{
  interface_t *y = INTERFACE(i) ;
  if (y->xindex[1] && x[y->xindex[1]].revents & IOPAUSE_READ)
  {
    if (!genqdyn_n(&y->serialq)) return 0 ;
    while (genqdyn_n(&y->serialq))
    {
      tain_t deadline ;
      int r = sanitize_read(skagetln(&y->in, &y->insa, y->sep[1])) ;
      if (r < 0) return 0 ;
      if (!r) break ;
      tain_uint(&deadline, 2) ;
      tain_add_g(&deadline, &deadline) ;
      if (!skabus_rpc_reply(a, *GENQDYN_PEEK(uint64_t, &y->serialq), 0, y->insa.s, y->insa.len - 1, &deadline)) return 0 ;
      genqdyn_pop(&y->serialq) ;
    }
  }
  if (y->xindex[0] && x[y->xindex[0]].revents & IOPAUSE_WRITE)
  {
    if (!bufalloc_flush(&y->out) && !error_isagain(errno)) return 0 ;
  }
}



static int query (char const *ifname, char const *q, tain_t const *limit, uint64_t *qid)
{
  uint64_t id ;
  tain_t deadline ;
  tain_uint(&deadline, 2) ;
  tain_add_g(&deadline, &deadline) ;
  id = skabus_rpc_send_g(a, ifname, q, strlen(q), limit, &deadline) ;
  if (!id) return 0 ;
  *qid = id ;
  return 1 ;
}


 /* Clients */

typedef struct client_s client_t, *client_t_ref ;
struct client_s
{
  uint32_t next ;
  uint32_t xindex ;
  textmessage_sender_t out ;
  textmessage_receiver_t in ;
  tain_t deadline ;
  uint64_t query ;
} ;

static gensetdyn clients = GENSETDYN_INIT(client_t, 1, 0, 1) ;
static uint32_t client_sentinel ;
#define CLIENT(i) GENSETDYN_P(client_t, &clients, (i))
#define numclients (gensetdyn_n(&clients) - 1)

static inline void client_free (client_t *c)
{
  fd_close(textmessage_sender_fd(&c->out)) ;
  textmessage_sender_free(&c->out) ;
  textmessage_receiver_free(&c->in) ;
}

static inline void client_delete (uint32_t cc, uint32_t prev)
{
  client_t *c = CLIENT(cc) ;
  CLIENT(prev)->next = c->next ;
  client_free(c) ;
  gensetdyn_delete(clients, cc) ;
}

static void client_setdeadline (client_t *c)
{
  tain_t blah ;
  tain_half(&blah, &tain_infinite_relative) ;
  tain_add_g(&blah, &blah) ;
  if (tain_less(&blah, &c->deadline))
    tain_add_g(&c->deadline, &answertto) ;
}

static inline int client_prepare_iopause (uint32_t i, tain_t *deadline, iopause_fd *x, uint32_t *j)
{
  client_t *c = CLIENT(i) ;
  if (tain_less(&c->deadline, deadline)) *deadline = c->deadline ;
  if (!textmessage_sender_isempty(&c->out) || !textmessage_receiver_isempty(&c->in) || (cont && !textmessage_receiver_isfull(&c->in)))
  {
    x[*j].fd = textmessage_sender_fd(&c->out) ;
    x[*j].events = (!textmessage_receiver_isempty(&c->in) || (cont && !textmessage_receiver_isfull(&c->in)) ? IOPAUSE_READ : 0) | (!textmessage_sender_isempty(&c->out) ? IOPAUSE_WRITE : 0) ;
    c->xindex = (*j)++ ;
  }
  else c->xindex = 0 ;
  return !!c->xindex ;
}

static inline int client_flush (uint32_t i, iopause_fd const *x)
{
  client_t *c = CLIENT(i) ;
  if (c->xindex && (x[c->xindex].revents & IOPAUSE_WRITE))
  {
    if (textmessage_sender_flush(&c->out))
      tain_add_g(&c->deadline, &tain_infinite_relative) ;
    else if (!error_isagain(errno)) return 0 ;
  }
  return 1 ;
}

static inline int client_add (uint32_t *cc, int fd)
{
  
  *cc = genset_new(clients) ;
  client_t *c = CLIENT(*cc) ;
  tain_add_g(&c->deadline, &answertto) ;
  textmessage_sender_init(&c->out, fd) ;
  textmessage_receiver_init(&c->in, fd) ;
  c->curquery = 0 ;
  c->next = CLIENT(sentinel)->next ;
  CLIENT(sentinel)->next = *cc ;
}

static int answer (uint32_t cc, char e)
{
  client_t *c = CLIENT(cc) ;
  if (!textmessage_put(&c->out, &e, 1)) return 0 ;
  client_setdeadline(c) ;
  return 1 ;
}

static inline void handle_signals (void)
{
  for (;;) switch (selfpipe_read())
  {
    case -1 : strerr_diefu1sys(111, "selfpipe_read()") ;
    case 0 : return ;
    case SIGTERM : quit() ; break ;
    case SIGCHLD :
    {
    }
    break ;
    default : break ;
  }
}

static int do_interface_register (uint32_t cc, char const *s, size_t len)
{
  uint32_t ifproglen, relen ;
  unsigned char ifnamelen ;
  if (len < 15) return (errno = EPROTO, 0) ;
  ifnamelen = s[0] ;
  uint32_unpack_big(s + 1, &ifproglen) ;
  uint32_unpack_big(s + 5, &relen) ;
  if (len != 12 + ifnamelen + ifproglen + relen) return (errno = EPROTO, 0) ;
  if (s[9 + ifnamelen] || s[10 + ifnamelen + ifproglen] || s[11 + ifnamelen + ifproglen + relen]) return (errno = EPROTO, 0) ;
  if (!interface_add(s + 9, s + 10 + ifnamelen, s + 11 + ifnamelen + ifproglen)) return answer(cc, errno) ;
  return answer(cc, 0) ;
}

static int do_interface_unregister (uint32_t cc, char const *s, size_t len)
{
  unsigned char ifnamelen ;
  if (len < 3) return (errno = EPROTO, 0) ;
  ifnamelen = s[0] ;
  if ((len != ifnamelen + 2) || s[ifnamelen+1]) return (errno = EPROTO, 0) ;
  if (!interface_remove(s+1)) return answer(cc, errno) ;
  return answer(cc, 0) ;
}

static int do_query (uint32_t cc, char const *s, size_t len)
{
  client_t *c = CLIENT(cc) ;
  tain_t limit ;
  uint32_t querylen, timeout ;
  unsigned char ifnamelen ;
  if (len < 12) return (errno = EPROTO, 0) ;
  ifnamelen = s[0] ;
  uint32_unpack_big(s + 1, &timeout) ;
  uint32_unpack_big(s + 5, &querylen) ;
  if (len != 11 + ifnamelen + querylen || s[9 + ifnamelen] || s[10 + ifnamelen + querylen]) return (errno = EPROTO, 0) ;
  if (timeout) tain_from_millisecs(&limit, timeout) ;
  else limit = tain_infinite_relative ;
  tain_add_g(&limit, &limit) ;
  if (!query(s + 9, s + 10 + ifnamelen, &limit, &c->query)) return answer(cc, errno) ;
  return answer(cc, 0) ;
}

static int do_quit (uint32_t cc, char const *s, size_t len)
{
  if (len) return (errno = EPROTO, 0) ;
  quit() ;
  (void)s ;
  return answer(cc, 0) ;
}

static int do_error (uint32_t cc, char const *s, size_t len)
{
  (void)cc ;
  (void)s ;
  (void)len ;
  return (errno = EPROTO, 0) ;
}

typedef int parsefunc_t (uint32_t, char const *, size_t) ;
typedef parsefunc_t *parsefunc_t_ref ;

static int parse_client_protocol (struct iovec const *v, void *p)
{
  static parsefunc_t_ref const f[5] =
  {
    &do_interface_register,
    &do_interface_unregister,
    &do_query,
    &do_quit,
    &do_error
  } ;
  if (!v->iov_len) return (errno = EPROTO, 0) ;
  return *f[byte_chr("IiQ.", 4, *(char const *)v->iov_base)])(*(uint32_t *)p, (char const *)v->iov_base + 1, v->iov_len - 1) ;
}

static void removeclient (uint32_t *i, uint32_t j)
{
  if (verbosity >= 2)
  {
    char fmt[UINT32_FMT] ;
    fmt[uint32_fmt(fmt, *i)] = 0 ;
    strerr_warni2sys("removing client ", fmt) ;
  }
  client_remove(*i, j) ;
  *i = j ;
}

int main (int argc, char const *const *argv, char const *const *envp)
{
  tain_t deadline ;
  int spfd ;
  int flag1 = 0 ;
  uint32_t maxconn = 8 ;
  unsigned int ifn = 1 ;
  PROG = "skabus-rpcc" ;

  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    unsigned int t = 0, T = 0 ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "v:1t:T:c:C:y:n:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'v' : if (!uint0_scan(l.arg, &verbosity)) dieusage() ; break ;
        case '1' : flag1 = 1 ; break ;
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        case 'T' : if (!uint0_scan(l.arg, &T)) dieusage() ; break ;
        case 'c' : if (!uint320_scan(l.arg, &maxconn)) dieusage() ; break ;
        case 'C' : interfaces[0] = l.arg ; break ;
        case 'y' :
        {
          if (ifn >= SKABUS_RPCC_INTERFACES_MAX) dietoomanyinterfaces() ;
          if (!l.arg[0]) dieemptyifname() ;
          interfaces[ifn++] = l.arg ;
          break ;
        }
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (t) tain_from_millisecs(&answertto, t) ;
    if (T) tain_from_millisecs(&lameduckdeadline, T) ;
  }
  if (argc < 2) dieusage() ;
  if (maxconn > SKABUS_RPCC_MAX) maxconn = SKABUS_RPCC_MAX ;
  if (!maxconn) maxconn = 1 ;
  {
    struct stat st ;
    if (fstat(0, &st) < 0) strerr_diefu1sys(111, "fstat stdin") ;
    if (!S_ISSOCK(st.st_mode)) strerr_dief1x(100, "stdin is not a socket") ;
  }
  if (flag1)
  {
    if (fcntl(1, F_GETFD) < 0) 
      strerr_dief1sys(100, "called with option -1 but stdout said") ;
  }
  else close(1) ;
  spfd = selfpipe_init() ;
  if (spfd < 0) strerr_diefu1sys(111, "selfpipe_init") ;
  if (sig_ignore(SIGPIPE) < 0) strerr_diefu1sys(111, "ignore SIGPIPE") ;
  {
    sigset_t set ;
    sigemptyset(&set) ;
    sigaddset(&set, SIGTERM) ;
    sigaddset(&set, SIGHUP) ;
    if (selfpipe_trapset(&set) < 0) strerr_diefu1sys(111, "trap signals") ;
  }

  tain_now_g() ;
  tain_add_g(&deadline, &answertto) ;
  
  {
    genset clientinfo ;
    client_t clientstorage[1+maxconn] ;
    uint32_t clientfreelist[1+maxconn] ;
    iopause_fd x[3 + (maxconn << 1) + (SKABUS_RPCC_INTERFACES_MAX) << 1] ;
    GENSET_init(&clientinfo, client_t, clientstorage, clientfreelist, 1+maxconn) ;
    sentinel = genset_new(&clientinfo) ;
    clientstorage[sentinel].next = sentinel ;
    clients = &clientinfo ;
    x[0].fd = spfd ; x[0].events = IOPAUSE_READ ;
    x[1].fd = 0 ;

    if (flag1)
    {
      fd_write(1, "\n", 1) ;
      fd_close(1) ;
    }

    for (;;)
    {
      int r = 1 ;
      uint32_t i = clientstorage[sentinel].next, j = 2 ;
      query_get_mindeadline(&deadline) ;
      if (!cont && tain_less(&lameduckdeadline, &deadline)) deadline = lameduckdeadline ;
      if (queries_pending()) r = 0 ;
      
      x[1].events = (cont && (numconn < maxconn)) ? IOPAUSE_READ : 0 ;
      for (; i != sentinel ; i = clientstorage[i].next)
        if (client_prepare_iopause(i, &deadline, x, &j, cont)) r = 0 ;
      if (!cont && r) break ;

      r = iopause_g(x, j, &deadline) ;
      if (r < 0) strerr_diefu1sys(111, "iopause") ;


     /* Timeout */

      if (!r)
      {
        if (!cont && !tain_future(&lameduckdeadline)) return 1 ;
        for (;;)
        {
          if (!query_lookup_by_mindeadline(&i)) break ;
          if (tain_future(&QUERY(i)->deadline)) break ;
          query_fail(i, ETIMEDOUT) ;
        }
        errno = ETIMEDOUT ;
        for (i = clientstorage[sentinel].next, j = sentinel ; i != sentinel ; j = i, i = clientstorage[i].next)
          if (!tain_future(&clientstorage[i].deadline)) removeclient(&i, j) ;
        continue ;
      }


     /* Signal */

      if (x[0].revents & IOPAUSE_READ) handle_signals() ;


     /* Event */

      for (j = sentinel, i = clientstorage[sentinel].next ; i != sentinel ; j = i, i = clientstorage[i].next)
        if (!client_flush(i, x)) removeclient(&i, j) ;

      for (j = sentinel, i = clientstorage[sentinel].next ; i != sentinel ; j = i, i = clientstorage[i].next)
        switch(client_read(i, x))
        {
          case 0 : errno = 0 ;
          case -1 :
          case -2 :
          {
            removeclient(&i, j) ;
            break ;
          }
          case 1 : break ;
          default : X() ;
        }


      /* New connection */

      if (x[1].revents & IOPAUSE_READ)
      {
        uint32_t flags = 0 ;
        uid_t uid ;
        gid_t gid ;
        regex_t idstr_re, interfaces_re ;
        int fd = ipc_accept_nb(x[1].fd, 0, 0, 0) ;
        if (fd < 0)
          if (!error_isagain(errno)) strerr_diefu1sys(111, "accept") ;
          else continue ;
        else if (!new_connection(fd, &uid, &gid, &idstr_re, &interfaces_re, &flags))
          fd_close(fd) ;
        else client_add(&i, &idstr_re, &interfaces_re, uid, gid, fd, flags) ;
      }
    }
  }
  return 0 ;
}
