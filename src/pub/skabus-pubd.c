/* ISC license. */

#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <regex.h>

#include <skalibs/uint32.h>
#include <skalibs/uint64.h>
#include <skalibs/types.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/bytestr.h>
#include <skalibs/sgetopt.h>
#include <skalibs/env.h>
#include <skalibs/error.h>
#include <skalibs/strerr.h>
#include <skalibs/stralloc.h>
#include <skalibs/tai.h>
#include <skalibs/djbunix.h>
#include <skalibs/sig.h>
#include <skalibs/iopause.h>
#include <skalibs/selfpipe.h>
#include <skalibs/cdb.h>
#include <skalibs/socket.h>
#include <skalibs/genset.h>
#include <skalibs/avltree.h>
#include <skalibs/avltreen.h>
#include <skalibs/unixmessage.h>
#include <skalibs/unixconnection.h>
#include <skalibs/skaclient.h>

#include <skalibs/posixishard.h>

#include <s6/accessrules.h>

#include <skabus/pub.h>

#define USAGE "skabus-pubd [ -v verbosity ] [ -1 ] [ -c maxconn ] [ -t timeout ] [ -T lameducktimeout ] [ -i rulesdir | -x rulesfile ] [ -S | -s ] [ -k controlre ] [ -m maxfds ] msgfsdir"
#define dieusage() strerr_dieusage(100, USAGE) ;
#define dienomem() strerr_diefu1sys(111, "stralloc_catb") ;
#define die() strerr_dief1sys(101, "unexpected error") ;

#define MSGINFO_PACK(n) (11 + TAIN_PACK + (n))

static unsigned int verbosity = 1 ;
static int cont = 1 ;
static uint64_t serial = 1 ;
static tain answertto = TAIN_INFINITE_RELATIVE ;
static tain lameduckdeadline = TAIN_INFINITE_RELATIVE ;
static int flagidstrpub = 0 ;
static unsigned int maxfds = 1000 ;
static uint32_t *fdcount ;
static int maxed = 0 ;

static unsigned int rulestype = 0 ;
static char const *rules = 0 ;
static cdb cdbmap = CDB_ZERO ;

static char const *msgfsdir ;

static void handle_signals (void)
{
  for (;;) switch (selfpipe_read())
  {
    case -1 : strerr_diefu1sys(111, "selfpipe_read()") ;
    case 0 : return ;
    case SIGTERM :
    {
      if (cont)
      {
        cont = 0 ;
        tain_add_g(&lameduckdeadline, &lameduckdeadline) ;
      }
      break ;
    }
    case SIGHUP :
    {
      cdb c = CDB_ZERO ;
      if (rulestype != 2) break ;
      if (!cdb_init(&c, rules)) break ;
      cdb_free(&cdbmap) ;
      cdbmap = c ;
    }
    break ;
    default : break ;
  }
}

static void fdcount_closecb (int fd, void *p)
{
  if (!--fdcount[fd])
  {
    fd_close(fd) ;
    maxed = 0 ;
  }
}


 /* client */

static void *uint32_dtok (uint32_t d, void *x)
{
  (void)x ;
  return (void *)(uintptr_t)d ;
}

static int ptr_cmp (void const *a, void const *b, void *x)
{
  (void)x ;
  return a < b ? -1 : a > b ;
}

typedef struct client_s client_t, *client_t_ref ;
struct client_s
{
  uint32_t next ;
  uint32_t xindexsync ;
  uint32_t xindexasync ;
  tain deadline ;
  regex_t idstr_re ;
  regex_t subscribe_re ;
  regex_t write_re ;
  avltree subscribers ;
  unixmessage_sender asyncout ;
  unixconnection sync ;
  char idstr[SKABUS_PUB_IDSTR_SIZE + 1] ;
} ;

static genset clients = GENSET_ZERO ;
static unsigned int sentinel ;
static avltreen *clientmap ;
#define CLIENT(i) genset_p(client_t, &clients, (i))
#define numconn (genset_n(&clients) - 1)

static inline void client_free (client_t *c)
{
  fd_close(unixmessage_sender_fd(&c->sync.out)) ;
  if (unixmessage_sender_fd(&c->asyncout) >= 0)
    fd_close(unixmessage_sender_fd(&c->asyncout)) ;
  unixconnection_free(&c->sync) ;
  unixmessage_sender_free(&c->asyncout) ;
  if (c->idstr[0])
  {
    regfree(&c->subscribe_re) ;
    regfree(&c->write_re) ;
  }
  else regfree(&c->idstr_re) ;
  avltree_free(&c->subscribers) ;
}

static void *idstr_dtok (uint32_t d, void *x)
{
  (void)x ;
  return CLIENT(d)->idstr ;
}

static int idstr_cmp (void const *a, void const *b, void *x)
{
  (void)x ;
  return strcmp((char const *)a, (char const *)b) ;
}

static inline void client_delete (uint32_t cc, uint32_t prev)
{
  client_t *c = CLIENT(cc) ;
  uint32_t i = CLIENT(sentinel)->next ;
  while (i != sentinel)
  {
    client_t *p = CLIENT(i) ;
    avltree_delete(&p->subscribers, (void const *)(uintptr_t)cc) ;
    i = p->next ;
  }
  CLIENT(prev)->next = c->next ;
  if (c->idstr[0]) avltreen_delete(clientmap, c->idstr) ;
  client_free(c) ;
  genset_delete(&clients, cc) ;
}

static void remove (uint32_t *i, uint32_t j)
{
  client_delete(*i, j) ;
  *i = j ;
}

static void client_setdeadline (client_t *c)
{
  tain blah ;
  tain_half(&blah, &tain_infinite_relative) ;
  tain_add_g(&blah, &blah) ;
  if (tain_less(&blah, &c->deadline))
    tain_add_g(&c->deadline, &answertto) ;
}

static inline int client_prepare_iopause (uint32_t i, tain *deadline, iopause_fd *x, uint32_t *j)
{
  client_t *c = CLIENT(i) ;
  if (tain_less(&c->deadline, deadline)) *deadline = c->deadline ;
  x[*j].events = (maxed || unixmessage_receiver_isfull(&c->sync.in) || (unixmessage_receiver_isempty(&c->sync.in) && !cont) ? 0 : IOPAUSE_READ) | (unixmessage_sender_isempty(&c->sync.out) ? 0 : IOPAUSE_WRITE) ;
  if (x[*j].events)
  {
    x[*j].fd = unixmessage_sender_fd(&c->sync.out) ;
    c->xindexsync = (*j)++ ;
  }
  else c->xindexsync = 0 ;
  if (!unixmessage_sender_isempty(&c->asyncout))
  {
    x[*j].fd = unixmessage_sender_fd(&c->asyncout) ;
    x[*j].events = IOPAUSE_WRITE ;
    c->xindexasync = (*j)++ ;
  }
  else c->xindexasync = 0 ;
  return c->xindexsync || c->xindexasync ;
}

static inline void client_add (int fd, regex_t const *idstr_re, unsigned int flags)
{
  uint32_t i = genset_new(&clients) ;
  client_t *c = CLIENT(i) ;
  unixconnection_init(&c->sync, fd, fd) ;
  unixmessage_sender_init_withclosecb(&c->asyncout, -(int)flags - 1, &fdcount_closecb, 0) ;
  c->idstr[0] = 0 ;
  c->idstr_re = *idstr_re ;
  avltree_init(&c->subscribers, 3, 3, 8, &uint32_dtok, &ptr_cmp, 0) ;
  tain_add_g(&c->deadline, &answertto) ;
  c->next = CLIENT(sentinel)->next ;
  CLIENT(sentinel)->next = i ;
}

static int enqueue_message (uint32_t dd, unixmessage const *m)
{
  client_t *d = CLIENT(dd) ;
  if (!unixmessage_put_and_close(&d->asyncout, m, unixmessage_bits_closeall)) return 0 ;
  client_setdeadline(d) ;
  for (unsigned int i = 0 ; i < m->nfds ; i++) fdcount[m->fds[i]]++ ;
  return 1 ;
}

static int unsendmessage_iter (uint32_t dd, unsigned int h, void *data)
{
  unixmessage_unput(&CLIENT(dd)->asyncout) ;
  (void)h ;
  (void)data ;
  return 1 ;
}

static int sendmessage_iter (uint32_t dd, unsigned int h, void *data)
{
  (void)h ;
  return enqueue_message(dd, (unixmessage *)data) ;
}

static int store_text (char const *s, size_t len)
{
  int fd ;
  size_t msgfsdirlen = strlen(msgfsdir) ;
  char fn[msgfsdirlen + TAIN_PACK + 10] ;
  fn[0] = '@' ;
  memcpy(fn + 1, msgfsdir, msgfsdirlen) ;
  fn[1 + msgfsdirlen] = '/' ;
  tain_pack(fn + msgfsdirlen + 2, &STAMP) ;
  memcpy(fn + msgfsdirlen + 2 + TAIN_PACK, ":XXXXXX", 8) ;
  fd = mkstemp(fn) ;
  if (fd < 0) return fd ;
  if (!writenclose_unsafe(fd, s, len))
  {
    fd_close(fd) ;
    return -1 ;
  }
  fd_close(fd) ;
  fd = open2(fn, O_RDONLY) ;  /* too bad you can't just lose writing rights */
  unlink_void(fn) ;  /* still there as long as fd is open */
  return fd ;
}

static void fill_msginfo (char *pack, char const *idstr, size_t idlen, uint8_t flags)
{
  uint64_pack_big(pack, serial++) ;
  tain_pack(pack + 8, &STAMP) ;
  pack[8 + TAIN_PACK] = flags ;
  pack[9 + TAIN_PACK] = idlen ;
  memcpy(pack + 10 + TAIN_PACK, idstr, idlen + 1) ;
}

static int announce (char const *what, size_t whatlen, char const *idstr)
{
  size_t idlen = strlen(idstr) ;
  int fd ;
  char s[MSGINFO_PACK(idlen)] ;
  unixmessage m = { .s = s, .len = MSGINFO_PACK(idlen), .fds = &fd, .nfds = 1 } ;
  if (!*idstr) return 1 ;
  fill_msginfo(s, idstr, idlen, 0) ;
  fd = store_text(what, whatlen) ;
  if (fd < 0) return 0 ;
  if (!avltree_iter_withcancel(&CLIENT(sentinel)->subscribers, &sendmessage_iter, &unsendmessage_iter, &m)) return 0 ;
  return 1 ;
}

static inline int client_flush (uint32_t i, iopause_fd const *x)
{
  client_t *c = CLIENT(i) ;
  int isflushed = 2 ;
  if (c->xindexsync && (x[c->xindexsync].revents & IOPAUSE_WRITE))
  {
    if (!unixmessage_sender_flush(&c->sync.out))
      if (error_isagain(errno)) isflushed = 0 ;
      else
      {
        unsigned char what[2] = "-" ;
        what[1] = errno ;
        if (verbosity) strerr_warnwu2sys("unixmessage_sender_flush ", c->idstr) ;
        if (!announce((char *)what, 2, c->idstr)) dienomem() ;
        return 0 ;
      }
    else isflushed = 1 ;
  }

  if (c->xindexasync && (x[c->xindexasync].revents & IOPAUSE_WRITE))
  {
    if (!unixmessage_sender_flush(&c->asyncout))
      if (error_isagain(errno)) isflushed = 0 ;
      else
      {
        unsigned char what[2] = "-" ;
        what[1] = errno ;
        if (verbosity) strerr_warnwu2sys("unixmessage_sender_flush ", c->idstr) ;
        if (!announce((char *)what, 2, c->idstr)) dienomem() ;
        return 0 ;
      }
    else isflushed = !!isflushed ;
  }

  if (isflushed == 1) tain_add_g(&c->deadline, &tain_infinite_relative) ;
  return 1 ;
}

static int answer (client_t *c, unsigned char e)
{
  unixmessage m = { .s = (char *)&e, .len = 1, .fds = 0, .nfds = 0 } ;
  if (!unixmessage_put(&c->sync.out, &m)) return 0 ;
  client_setdeadline(c) ;
  return 1 ;
}

static int do_register (uint32_t cc, unixmessage const *m)
{
  uint32_t srelen, wrelen, dummy ;
  client_t *c = CLIENT(cc) ;
  char const *s = m->s ;
  size_t len = m->len ;
  int r ;
  size_t idlen ;
  if (len < 12 || m->nfds) return (errno = EPROTO, 0) ;
  idlen = (unsigned char)*s++ ; len-- ;
  uint32_unpack_big(s, &srelen) ; s += 4 ; len -= 4 ;
  uint32_unpack_big(s, &wrelen) ; s += 4 ; len -= 4 ;
  if (len != idlen + srelen + wrelen + 3 || s[idlen] || s[idlen + srelen + 1] || s[idlen + srelen + wrelen + 2] || !idlen || !s[idlen-1]) return (errno = EPROTO, 0) ;
  if (idlen > SKABUS_PUB_IDSTR_SIZE) return answer(c, ENAMETOOLONG) ;
  if (c->idstr[0]) return answer(c, EISCONN) ;
  if (regexec(&c->idstr_re, s, 0, 0, 0)) return answer(c, EPERM) ;
  if (avltreen_search(clientmap, s, &dummy)) return answer(c, EBUSY) ;
  r = regcomp(&c->subscribe_re, s + idlen + 1, REG_EXTENDED | REG_NOSUB) ;
  if (r) return answer(c, r == REG_ESPACE ? ENOMEM : EINVAL) ;
  r = regcomp(&c->write_re, s + idlen + srelen + 2, REG_EXTENDED | REG_NOSUB) ;
  if (r)
  {
    regfree(&c->subscribe_re) ;
    return answer(c, r == REG_ESPACE ? ENOMEM : EINVAL) ;
  }
  memcpy(c->idstr, s, idlen+1) ;
  avltreen_insert(clientmap, cc) ;
  if (!announce("+", 1, s))
  {
    unsigned char e = errno ;
    regfree(&c->write_re) ;
    regfree(&c->subscribe_re) ;
    avltreen_delete(clientmap, c->idstr) ;
    return answer(c, e) ;
  }
  regfree(&c->idstr_re) ;
  return answer(c, 0) ;
}

static int do_list (uint32_t cc, unixmessage const *m)
{
  client_t *c = CLIENT(cc) ;
  uint32_t dd = CLIENT(sentinel)->next ;
  if (m->len || m->nfds) return (errno = EPROTO, 0) ;
  unsigned int n = avltreen_len(clientmap) - 1 ;
  {
    char pack[9] = "" ;
    char lens[n] ;
    struct iovec v[1+(n<<1)] ;
    unixmessagev mreply = { .v = v, .vlen = 1+(n<<1), .fds = 0, .nfds = 0 } ;
    unsigned int registered = 0 ;
    v[0].iov_base = pack ; v[0].iov_len = 9 ;
    for (unsigned int i = 0 ; i < n ; i++)
    {
      client_t *d = CLIENT(dd) ;
      if (d->idstr[0])
      {
        size_t len = strlen(d->idstr) ;
        lens[i] = (unsigned char)len ;
        v[1+(i<<1)].iov_base = lens + i ;
        v[1+(i<<1)].iov_len = 1 ;
        v[2+(i<<1)].iov_base = d->idstr ;
        v[2+(i<<1)].iov_len = len+1 ;
        registered++ ;
      }
      dd = d->next ;
    }
    uint32_pack_big(pack+1, registered) ;
    uint32_pack_big(pack+5, n - registered) ;
    if (!unixmessage_putv(&c->sync.out, &mreply)) return answer(c, errno) ;
  }
  client_setdeadline(c) ;
  return 1 ;
}

static int do_unsubscribe (uint32_t cc, unixmessage const *m)
{
  uint32_t dd ;
  char const *s = m->s ;
  size_t len = m->len ;
  client_t *c = CLIENT(cc) ;
  if (!len-- || m->nfds) return (errno = EPROTO, 0) ;
  if (len != (unsigned int)(unsigned char)s[0] + 1 || s[len]) return (errno = EPROTO, 0) ;
  s++ ;
  if (!c->idstr[0]) return answer(c, EPERM) ;
  if (!avltreen_search(clientmap, s, &dd)) return answer(c, errno) ;
  if (!avltree_delete(&CLIENT(dd)->subscribers, (void const *)(uintptr_t)cc)) return answer(c, errno) ;
  return answer(c, 0) ;
}

static int do_subscribe (uint32_t cc, unixmessage const *m)
{
  client_t *c = CLIENT(cc) ;
  char const *s = m->s ;
  size_t len = m->len ;
  client_t *d ;
  uint32_t dd, dummy ;
  if (!len-- || m->nfds) return (errno = EPROTO, 0) ;
  if (len != (unsigned int)(unsigned char)s[0] + 1 || s[len]) return (errno = EPROTO, 0) ;
  s++ ;
  if (!c->idstr[0]) return answer(c, EPERM) ;
  if (!avltreen_search(clientmap, s, &dd)) return answer(c, errno) ;
  d = CLIENT(dd) ;
  if (regexec(&d->subscribe_re, c->idstr, 0, 0, 0)) return answer(c, EPERM) ;
  if (!avltree_search(&d->subscribers, c->idstr, &dummy))
  {
    if (!avltree_insert(&d->subscribers, cc)) return answer(c, errno) ;
  }
  return answer(c, 0) ;
}

static int do_sendpm (uint32_t cc, unixmessage const *m)
{
  client_t *c = CLIENT(cc) ;
  char const *s = m->s ;
  size_t len = m->len ;
  size_t idlen ;
  uint32_t dd ;
  client_t *d ;
  if (len < 2) return (errno = EPROTO, 0) ;
  idlen = (unsigned char)s[0] ; s++ ; len-- ;
  if (len < idlen + 1 || s[idlen]) return (errno = EPROTO, 0) ;
  if (idlen > SKABUS_PUB_IDSTR_SIZE)
  {
    unixmessage_drop(m) ;
    return answer(c, ENAMETOOLONG) ;
  }
  if (m->nfds > UNIXMESSAGE_MAXFDS - 1)
  {
    unixmessage_drop(m) ;
    return answer(c, ENFILE) ;
  }
  if (!c->idstr[0])
  {
    unixmessage_drop(m) ;
    return answer(c, EPERM) ;
  }
  if (!avltreen_search(clientmap, s, &dd))
  {
    unixmessage_drop(m) ;
    return answer(c, errno) ;
  }
  s += idlen+1 ; len -= idlen+1 ;
  d = CLIENT(dd) ;
  if (regexec(&d->write_re, c->idstr, 0, 0, 0))
  {
    unixmessage_drop(m) ;
    return answer(c, EPERM) ;
  }

  {
    size_t cidlen = strlen(c->idstr) ;
    char pack[1 + MSGINFO_PACK(cidlen)] ;
    int fds[1 + m->nfds] ;
    unixmessage mtosend = { .s = pack + 1, .len = MSGINFO_PACK(cidlen), .fds = fds, .nfds = 1 + m->nfds } ;
    fds[0] = store_text(s, len) ;
    if (fds[0] < 0)
    {
      unixmessage_drop(m) ;
      return answer(c, errno) ;
    }
    fill_msginfo(pack + 1, c->idstr, cidlen, 1) ;
    for (unsigned int i = 0 ; i < m->nfds ; i++) fds[1+i] = m->fds[i] ;
    if (!unixmessage_put_and_close(&d->asyncout, &mtosend, unixmessage_bits_closeall))
    {
      unixmessage_drop(m) ;
      return answer(c, errno) ;
    }
    client_setdeadline(d) ;
    pack[0] = 0 ;
    mtosend.s = pack ;
    mtosend.len = 9 ;
    mtosend.fds = 0 ;
    mtosend.nfds = 0 ;
    return enqueue_message(cc, &mtosend) ;
  }
}

static int do_send (uint32_t cc, unixmessage const *m)
{
  client_t *c = CLIENT(cc) ;
  size_t cidlen = strlen(c->idstr) ;
  if (m->nfds > UNIXMESSAGE_MAXFDS - 1)
  {
    unixmessage_drop(m) ;
    return answer(c, ENFILE) ;
  }
  {
    char pack[1 + MSGINFO_PACK(cidlen)] ;
    int fds[1 + m->nfds] ;
    unixmessage mtosend = { .s = pack + 1, .len = MSGINFO_PACK(cidlen), .fds = fds, .nfds = 1 + m->nfds } ;
    fds[0] = store_text(m->s, m->len) ;
    if (fds[0] < 0)
    {
      unixmessage_drop(m) ;
      return answer(c, errno) ;
    }
    fill_msginfo(pack + 1, c->idstr, cidlen, 0) ;
    for (unsigned int i = 0 ; i < m->nfds ; i++) fds[1+i] = m->fds[i] ;
    if (!avltree_iter_withcancel(&c->subscribers, &sendmessage_iter, &unsendmessage_iter, &mtosend))
    {
      unixmessage_drop(m) ;
      return answer(c, errno) ;
    }
    pack[0] = 0 ;
    mtosend.s = pack ;
    mtosend.len = 9 ;
    mtosend.fds = 0 ;
    mtosend.nfds = 0 ;
    return enqueue_message(cc, &mtosend) ;
  }
}

static int do_error (uint32_t cc, unixmessage const *m)
{
  (void)cc ;
  (void)m ;
  return (errno = EPROTO, 0) ;
}

typedef int parse_func (uint32_t, unixmessage const *) ;
typedef parse_func *parse_func_ref ;

static inline int parse_protocol (unixmessage const *m, void *p)
{
  static parse_func_ref const f[7] =
  {
    &do_send,
    &do_sendpm,
    &do_subscribe,
    &do_unsubscribe,
    &do_list,
    &do_register,
    &do_error
  } ;
  unixmessage mcopy = { .s = m->s + 1, .len = m->len - 1, .fds = m->fds, .nfds = m->nfds } ;
  if (!m->len)
  {
    unixmessage_drop(m) ;
    return (errno = EPROTO, 0) ;
  }
  if (!(*f[byte_chr("!+SULR", 6, m->s[0])])(*(uint32_t *)p, &mcopy))
  {
    unixmessage_drop(m) ;
    return 0 ;
  }
  return 1 ;
}

static inline int client_read (uint32_t cc, iopause_fd const *x)
{
  client_t *c = CLIENT(cc) ;
  if (!unixmessage_receiver_isempty(&c->sync.in) || (c->xindexsync && (x[c->xindexsync].revents & IOPAUSE_READ)))
  {
    if (unixmessage_sender_fd(&c->asyncout) < 0)
    {
      unixmessage m ;
      int r = unixmessage_receive(&c->sync.in, &m) ;
      if (r < 0) return -1 ;
      if (r)
      {
        unsigned int flags = -(unixmessage_sender_fd(&c->asyncout) + 1) ;
        if (!skaclient_server_ack(&m, &c->sync.out, &c->asyncout, SKABUS_PUB_BANNER1, SKABUS_PUB_BANNER1_LEN, SKABUS_PUB_BANNER2, SKABUS_PUB_BANNER2_LEN))
        {
          unixmessage_drop(&m) ;
          return -1 ;
        }
        if (!(flags & 1)) unixmessage_receiver_refuse_fds(&c->sync.in) ;
      }
    }
    else
    {
      int r = unixmessage_handle(&c->sync.in, &parse_protocol, &cc) ;
      if (r <= 0) return r ;
    }
  }
  return 1 ;
}


 /* Environment on new connections */

static int makere (regex_t *re, char const *s, char const *var)
{
  size_t varlen = strlen(var) ;
  if (str_start(s, var) && (s[varlen] == '='))
  {
    int r = regcomp(re, s + varlen + 1, REG_EXTENDED | REG_NOSUB) ;
    if (r)
    {
      if (verbosity)
      {
        char buf[256] ;
        regerror(r, re, buf, 256) ;
        strerr_warnw6x("invalid ", var, " value: ", s + varlen + 1, ": ", buf) ;
      }
      return -1 ;
    }
    else return 1 ;
  }
  return 0 ;
}

static void defaultre (regex_t *re, int flag)
{
  char const *s = flag ? ".*" : "^$" ;
  int r = regcomp(re, s, REG_EXTENDED | REG_NOSUB) ;
  if (r)
  {
    char buf[256] ;
    regerror(r, re, buf, 256) ;
    strerr_diefu4x(100, "compile ", s, " into a regular expression: ", buf) ;
  }
}

static inline int parse_env (char const *const *envp, regex_t *idstr_re, unsigned int *flags)
{
  unsigned int fl = 0 ;
  int idstr_done = 0 ;
  for (; *envp ; envp++)
  {
    if (str_start(*envp, "SKABUS_PUB_SENDFDS=")) fl |= 1 ;
    if (!idstr_done)
    {
      idstr_done = makere(idstr_re, *envp, "SKABUS_PUB_ID_REGEX") ;
      if (idstr_done < 0) return 0 ;
    }
    if (idstr_done) return 1 ;
  }
  if (!idstr_done) defaultre(idstr_re, flagidstrpub) ;
  *flags = fl ;
  return 1 ;
}

static inline int new_connection (int fd, regex_t *idstr_re, unsigned int *flags)
{
  s6_accessrules_params_t params = S6_ACCESSRULES_PARAMS_ZERO ;
  s6_accessrules_result_t result = S6_ACCESSRULES_ERROR ;
  uid_t uid ;
  gid_t gid ;

  if (getpeereid(fd, &uid, &gid) < 0)
  {
    if (verbosity) strerr_warnwu1sys("getpeereid") ;
    return 0 ;
  }
  switch (rulestype)
  {
    case 0 :
      result = S6_ACCESSRULES_ALLOW ; break ;
    case 1 :
      result = s6_accessrules_uidgid_fs(uid, gid, rules, &params) ; break ;
    case 2 :
      result = s6_accessrules_uidgid_cdb(uid, gid, &cdbmap, &params) ; break ;
    default : break ;
  }
  if (result != S6_ACCESSRULES_ALLOW)
  {
    if (verbosity && (result == S6_ACCESSRULES_ERROR))
       strerr_warnw1sys("error while checking rules") ;
    return 0 ;
  }
  if (params.exec.s)
  {
    stralloc_free(&params.exec) ;
    if (verbosity)
    {
      char fmtuid[UID_FMT] ;
      char fmtgid[GID_FMT] ;
      fmtuid[uid_fmt(fmtuid, uid)] = 0 ;
      fmtgid[gid_fmt(fmtgid, gid)] = 0 ;
      strerr_warnw4x("unused exec string in rules for uid ", fmtuid, " gid ", fmtgid) ;
    }
  }
  if (params.env.s)
  {
    size_t n = byte_count(params.env.s, params.env.len, '\0') ;
    char const *envp[n+1] ;
    if (!env_make(envp, n, params.env.s, params.env.len))
    {
      if (verbosity) strerr_warnwu1sys("env_make") ;
      stralloc_free(&params.env) ;
      return 0 ;
    }
    envp[n] = 0 ;
    if (!parse_env(envp, idstr_re, flags))
    {
      if (verbosity) strerr_warnwu1sys("parse_env") ;
      s6_accessrules_params_free(&params) ;
      return 0 ;
    }
    s6_accessrules_params_free(&params) ;
  }
  return 1 ;
}

int main (int argc, char const *const *argv, char const *const *envp)
{
  int flag1 = 0 ;
  char const *announce_re = "^$" ;
  unsigned int maxconn = 40 ;
  PROG = "skabus-pubd" ;

  {
    subgetopt l = SUBGETOPT_ZERO ;
    unsigned int t = 0, T = 0 ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "v:Ss1c:k:i:x:t:T:m:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'v' : if (!uint0_scan(l.arg, &verbosity)) dieusage() ; break ;
        case 'S' : flagidstrpub = 0 ; break ;
        case 's' : flagidstrpub = 1 ; break ;
        case '1' : flag1 = 1 ; break ;
        case 'k' : announce_re = l.arg ; break ;
        case 'i' : rules = l.arg ; rulestype = 1 ; break ;
        case 'x' : rules = l.arg ; rulestype = 2 ; break ;
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        case 'T' : if (!uint0_scan(l.arg, &T)) dieusage() ; break ;
        case 'c' : if (!uint0_scan(l.arg, &maxconn)) dieusage() ; break ;
        case 'm' : if (!uint0_scan(l.arg, &maxfds)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (t) tain_from_millisecs(&answertto, t) ;
    if (T) tain_from_millisecs(&lameduckdeadline, T) ;
  }
  if (!argc) dieusage() ;
  if (maxconn > SKABUS_PUB_MAX) maxconn = SKABUS_PUB_MAX ;
  if (!maxconn) maxconn = 1 ;
  if (maxfds > SKABUS_PUB_MAXFDS) maxfds = SKABUS_PUB_MAXFDS ;
  if (maxfds < 10) maxfds = 10 ;
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
  else fd_close(1) ;
  {
    struct stat st ;
    if (stat(argv[0], &st) < 0)
      strerr_diefu2sys(111, "stat ", argv[0]) ;
    if (!S_ISDIR(st.st_mode))
    {
      errno = ENOTDIR ;
      strerr_diefu2sys(100, "work in ", argv[0]) ;
    }
  }
  msgfsdir = argv[0] ;
  if (selfpipe_init() == -1) strerr_diefu1sys(111, "selfpipe_init") ;
  if (!sig_ignore(SIGPIPE)) strerr_diefu1sys(111, "ignore SIGPIPE") ;
  {
    sigset_t set ;
    sigemptyset(&set) ;
    sigaddset(&set, SIGTERM) ;
    sigaddset(&set, SIGHUP) ;
    if (!selfpipe_trapset(&set)) strerr_diefu1sys(111, "trap signals") ;
  }

  if (rulestype == 2)
  {
    if (!cdb_init(&cdbmap, rules))
      strerr_diefu2sys(111, "cdb_init ", rules) ;
  }

  {  /* I present to you: the stack */
    client_t clientstorage[1+maxconn] ;
    uint32_t clientfreelist[1+maxconn] ;
    uint32_t fdcountblob[6 + maxfds + maxconn] ;
    iopause_fd x[2 + (maxconn << 1)] ;
    AVLTREEN_DECLARE_AND_INIT(blobmap, 1+maxconn, &idstr_dtok, &idstr_cmp, 0) ;

    GENSET_init(&clients, client_t, clientstorage, clientfreelist, 1+maxconn) ;
    sentinel = genset_new(&clients) ;
    clientstorage[sentinel].next = sentinel ;
    clientstorage[sentinel].idstr[0] = 0 ;
    {
      int r = regcomp(&clientstorage[sentinel].subscribe_re, announce_re, REG_EXTENDED | REG_NOSUB) ;
      if (r)
      {
        char buf[256] ;
        regerror(r, &clientstorage[sentinel].subscribe_re, buf, 256) ;
        strerr_dief4x(100, "invalid control regex: ", announce_re, ": ", buf) ;
      }
    }
    if (regcomp(&clientstorage[sentinel].write_re, "^$", REG_NOSUB)) strerr_diefu1x(100, "regcomp ^$") ;
    avltree_init(&clientstorage[sentinel].subscribers, 3, 3, 8, &uint32_dtok, &ptr_cmp, 0) ;
    avltreen_insert(&blobmap, sentinel) ;
    clientmap = &blobmap ;
    x[0].fd = selfpipe_fd() ; x[0].events = IOPAUSE_READ ;
    x[1].fd = 0 ;
    for (unsigned int i = 0 ; i < sizeof(fdcountblob) / sizeof(uint32_t) ; i++) fdcountblob[i] = 0 ;
    fdcount = fdcountblob ;
      
    if (flag1)
    {
      fd_write(1, "\n", 1) ;
      fd_close(1) ;
    }
    tain_now_g() ;

    for (;;)
    {
      tain deadline ;
      uint32_t i = clientstorage[sentinel].next, j = 2 ;
      int r = 1 ;
      if (cont) tain_add_g(&deadline, &tain_infinite_relative) ;
      else deadline = lameduckdeadline ;
      x[1].events = (cont && (numconn < maxconn)) ? IOPAUSE_READ : 0 ;
      for (; i != sentinel ; i = clientstorage[i].next)
        if (client_prepare_iopause(i, &deadline, x, &j)) r = 0 ;
      if (!cont && r) break ;

      r = iopause_g(x, j, &deadline) ;
      if (r < 0) strerr_diefu1sys(111, "iopause") ;

      if (!r)
      {
        for (j = sentinel, i = clientstorage[sentinel].next ; i != sentinel ; j = i, i = clientstorage[i].next)
          if (!tain_future(&clientstorage[i].deadline))
          {
            unsigned char what[2] = "-" ;
            what[1] = ETIMEDOUT ;
            if (!announce((char *)what, 2, CLIENT(i)->idstr)) dienomem() ;
            remove(&i, j) ;
          }
        continue ;
      }

      if (x[0].revents & IOPAUSE_READ) handle_signals() ;

      for (j = sentinel, i = clientstorage[sentinel].next ; i != sentinel ; j = i, i = clientstorage[i].next)
        if (!client_flush(i, x)) remove(&i, j) ;

      if (!maxed)
        for (j = sentinel, i = clientstorage[sentinel].next ; i != sentinel ; j = i, i = clientstorage[i].next)
          switch (client_read(i, x))
          {
            case 0 : errno = 0 ;
            case -1 :
            case -2 :
            {
              unsigned char what[2] = "-" ;
              what[1] = errno ;
              if (!announce((char *)what, 2, CLIENT(i)->idstr)) dienomem() ;
            }
            remove(&i, j) ;
            case 1 : break ;
            default : errno = EILSEQ ; die() ;
          }

      if (x[1].revents & IOPAUSE_READ)
      {
        regex_t idstr_re ;
        unsigned int flags = 0 ;
        int fd = ipc_accept_nb(x[1].fd, 0, 0, 0) ;
        if (fd < 0)
          if (!error_isagain(errno)) strerr_diefu1sys(111, "accept") ;
          else continue ;
        else if (!new_connection(fd, &idstr_re, &flags)) fd_close(fd) ;
        else client_add(fd, &idstr_re, flags) ;
      }
    }
  }
  return 0 ;
}
