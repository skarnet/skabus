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

#include <skalibs/posixplz.h>
#include <skalibs/posixishard.h>
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
#include <skalibs/djbunix.h>
#include <skalibs/sgetopt.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/env.h>
#include <skalibs/getpeereid.h>
#include <skalibs/socket.h>
#include <skalibs/genset.h>
#include <skalibs/unixmessage.h>

#include <s6/accessrules.h>

#include <skabus/rpc.h>
#include "skabus-rpcd.h"

#define USAGE "skabus-rpcd [ -v verbosity ] [ -1 ] [ -c maxconn ] [ -t timeout ] [ -T lameducktimeout ] [ -i rulesdir | -x rulesfile ] [ -S | -s ] [ -J | -j ]"
#define dieusage() strerr_dieusage(100, USAGE) ;

tain_t answertto = TAIN_INFINITE_RELATIVE ;

static unsigned int verbosity = 1 ;
static int cont = 1 ;
static tain_t lameduckdeadline = TAIN_INFINITE_RELATIVE ;

static unsigned int rulestype = 0 ;
static char const *rules = 0 ;
static int cdbfd = -1 ;
static struct cdb cdbmap = CDB_ZERO ;

static inline void handle_signals (void)
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
      int fd ;
      struct cdb c = CDB_ZERO ;
      if (rulestype != 2) break ;
      fd = open_readb(rules) ;
      if (fd < 0) break ;
      if (cdb_init(&c, fd) < 0)
      {
        fd_close(fd) ;
        break ;
      }
      cdb_free(&cdbmap) ;
      fd_close(cdbfd) ;
      cdbfd = fd ;
      cdbmap = c ;
    }
    break ;
    default : break ;
  }
}

int parse_protocol_async (unixmessage_t const *m, void *p)
{
  uint64_t serial ;
  uint32_t qq ;
  unixmessage_t mtosend = { .s = m->s + 10, .len = m->len - 10, .fds = m->fds, .nfds = m->nfds } ;
  if (m->len < 10 || m->s[0] != 'R')
  {
    unixmessage_drop(m) ;
    return (errno = EPROTO, 0) ;
  }
  uint64_unpack_big(m->s + 1, &serial) ;
  if (!query_lookup_by_serial(serial, &qq))
  {
    unixmessage_drop(m) ;
    return 1 ;
  }
  if (INTERFACE(QUERY(qq)->interface)->client != *(uint32_t *)p)
  {
    unixmessage_drop(m) ;
    return 1 ;
  }
  query_reply(qq, m->s[9], &mtosend) ;
  return 1 ;
}

typedef int hlparsefunc_t (uint32_t, unixmessage_t *) ;
typedef hlparsefunc_t *hlparsefunc_t_ref ;

static int answer (uint32_t cc, char e)
{
  unixmessage_t m = { .s = &e, .len = 1, .fds = 0, .nfds = 0 } ;
  client_t *c = CLIENT(cc) ;
  if (!unixmessage_put(&c->sync.out, &m)) return 0 ;
  if (client_isregistered(cc)) client_setdeadline(c) ;
  return 1 ;
}

static int do_idstr (uint32_t cc, unixmessage_t *m)
{
  uint32_t relen, pmid, yy ;
  unsigned char idlen ;
  if (m->len < 11 || m->nfds) return (errno = EPROTO, 0) ;
  uint32_unpack_big(m->s, &pmid) ;
  idlen = m->s[4] ;
  uint32_unpack_big(m->s + 5, &relen) ;
  if (m->len != 11 + idlen + relen || m->s[9 + idlen] || m->s[10 + idlen + relen]) return (errno = EPROTO, 0) ;
  if (client_isregistered(cc)) return answer(cc, EISCONN) ;
  if (regexec(&CLIENT(cc)->idstr_re, m->s + 9, 0, 0, 0)) return answer(cc, EPERM) ;
  m->s[8] = 0xff ;
  if (interface_lookup_by_name(m->s + 8, &yy)) return answer(cc, EADDRINUSE) ;
  if (!interface_add(&yy, m->s + 8, idlen + 1, cc, m->s + 10 + idlen, pmid))
    return answer(cc, errno) ;
  regfree(&CLIENT(cc)->idstr_re) ;
  return answer(cc, 0) ;
}

static int do_interface_register (uint32_t cc, unixmessage_t *m)
{
  uint32_t id, relen, yy ;
  unsigned char iflen ;
  if (m->len < 11 || m->nfds) return (errno = EPROTO, 0) ;
  uint32_unpack_big(m->s, &id) ;
  iflen = m->s[4] ;
  uint32_unpack_big(m->s + 5, &relen) ;
  if (m->len != 11 + iflen + relen) return (errno = EPROTO, 0) ;
  if (m->s[9 + iflen] || m->s[10 + iflen + relen]) return (errno = EPROTO, 0) ;
  if (!client_isregistered(cc)) return answer(cc, ENOTCONN) ;
  if (regexec(&CLIENT(cc)->interfaces_re, m->s + 9, 0, 0, 0)) return answer(cc, EPERM) ;
  if (interface_lookup_by_name(m->s + 9, &yy)) return answer(cc, EADDRINUSE) ;
  if (!interface_add(&yy, m->s + 9, iflen, cc, m->s + 10 + iflen, id)) return answer(cc, errno) ;
  return answer(cc, 0) ;
}

static int do_interface_unregister (uint32_t cc, unixmessage_t *m)
{
  uint32_t yy ;
  unsigned char iflen ;
  if (!m->len || m->nfds) return (errno = EPROTO, 0) ;
  iflen = m->s[0] ;
  if ((m->len != iflen + 2) || m->s[iflen+1]) return (errno = EPROTO, 0) ;
  if (!client_isregistered(cc)) return answer(cc, ENOTCONN) ;
  if (!iflen || ((unsigned char const *)m->s)[1] == 0xff) return answer(cc, EINVAL) ;
  if (!interface_lookup_by_name(m->s + 1, &yy)) return answer(cc, errno) ;
  if (INTERFACE(yy)->client != cc) return answer(cc, EPERM) ;
  interface_remove(yy) ;
  return answer(cc, 0) ;
}

static int do_query_send (uint32_t cc, unixmessage_t *m)
{
  tain_t limit ;
  uint32_t yy, qq ;
  unsigned char iflen ;
  int e ;
  if (m->len < 2 + TAIN_PACK) return (errno = EPROTO, 0) ;
  tain_unpack(m->s, &limit) ;
  iflen = m->s[TAIN_PACK] ;
  if (m->len < 2 + TAIN_PACK + iflen || m->s[TAIN_PACK + 1 + iflen]) return (errno = EPROTO, 0) ;
  if (!client_isregistered(cc)) { e = ENOTCONN ; goto nope ; }
  if (!interface_lookup_by_name(m->s + TAIN_PACK + 1, &yy)) { e = ESRCH ; goto nope ; }
  if (regexec(&INTERFACE(yy)->re, client_idstr(CLIENT(cc)), 0, 0, 0)) { e = EPERM ; goto nope ; }
  m->s += 2 + TAIN_PACK + iflen ;
  m->len -= 2 + TAIN_PACK + iflen ;
  if (!query_add(&qq, &limit, cc, yy) || !query_send(qq, m)) { e = errno ; goto nope ; }
  return answer(cc, 0) ;

 nope:
  if (!answer(cc, e)) return 0 ;
  unixmessage_drop(m) ;
  return 1 ;
}

static int do_query_cancel (uint32_t cc, unixmessage_t *m)
{
  uint64_t serial ;
  uint32_t qq ;
  if (m->len != 8 || m->nfds) return (errno = EPROTO, 0) ;
  if (!client_isregistered(cc)) return answer(cc, ENOTCONN) ;
  uint64_unpack_big(m->s, &serial) ;
  if (!query_lookup_by_serial(serial, &qq)) return answer(cc, errno) ;
  if (cc != QUERY(qq)->client) return answer(cc, EPERM) ;
  if (!query_cancelremove(qq, 0)) return answer(cc, errno) ;
  return answer(cc, 0) ;
}

static int do_error (uint32_t cc, unixmessage_t *m)
{
  (void)cc ;
  (void)m ;
  return (errno = EPROTO, 0) ;
}

int parse_protocol_sync (unixmessage_t const *m, void *p)
{
  static hlparsefunc_t_ref const f[6] =
  {
    &do_idstr,
    &do_interface_register,
    &do_interface_unregister,
    &do_query_send,
    &do_query_cancel,
    &do_error
  } ;
  unixmessage_t mcopy = { .s = m->s + 1, .len = m->len -1, .fds = m->fds, .nfds = m->nfds } ;
  if (!m->len)
  {
    unixmessage_drop(m) ;
    return (errno = EPROTO, 0) ;
  }
  if (!(*f[byte_chr("SIiQC", 5, m->s[0])])(*(uint32_t *)p, &mcopy))
  {
    unixmessage_drop(m) ;
    return 0 ;
  }
  return 1 ;
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

static int makere (regex_t *re, char const *s, char const *var)
{
  size_t varlen = strlen(var) ;
  if (str_start(s, var) && (s[varlen] == '='))
  {
    int r = skalibs_regcomp(re, s + varlen + 1, REG_EXTENDED | REG_NOSUB) ;
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

static void defaultre (regex_t *re, unsigned int pubflag)
{
  char const *s = pubflag ? "" : ".^" ;
  int r = skalibs_regcomp(re, s, REG_EXTENDED | REG_NOSUB) ;
  if (r)
  {
    char buf[256] ;
    regerror(r, re, buf, 256) ;
    strerr_diefu4x(100, "compile ", s, " into a regular expression: ", buf) ;
  }
}

static inline int parse_env (char const *const *envp, regex_t *idstr_re, regex_t *interfaces_re, uint32_t *flags, unsigned int *donep)
{
  uint32_t fl = 0 ;
  unsigned int done = 0 ;
  for (; *envp ; envp++)
  {
    if (str_start(*envp, "SKABUS_RPC_QSENDFDS=")) fl |= 1 ;
    if (str_start(*envp, "SKABUS_RPC_RSENDFDS=")) fl |= 2 ;
    if (!(done & 1))
    {
      int r = makere(idstr_re, *envp, "SKABUS_RPC_ID_REGEX") ;
      if (r < 0)
      {
        if (done & 2) regfree(interfaces_re) ;
        return 0 ;
      }
      else if (r) done |= 1 ;
    }
    if (!(done & 2))
    {
      int r = makere(interfaces_re, *envp, "SKABUS_RPC_INTERFACES_REGEX") ;
      if (r < 0)
      {
        if (done & 1) regfree(idstr_re) ;
        return 0 ;
      }
      else if (r) done |= 2 ;
    }
  }
  *flags = fl ;
  *donep = done ;
  return 1 ;
}

static inline int new_connection (int fd, uid_t *uid, gid_t *gid, regex_t *idstr_re, regex_t *interfaces_re, uint32_t *flags, unsigned int pubflags)
{
  s6_accessrules_params_t params = S6_ACCESSRULES_PARAMS_ZERO ;
  s6_accessrules_result_t result = S6_ACCESSRULES_ERROR ;
  unsigned int done = 0 ;

  if (getpeereid(fd, uid, gid) < 0)
  {
    if (verbosity) strerr_warnwu1sys("getpeereid") ;
    return 0 ;
  }

  switch (rulestype)
  {
    case 0 :
      result = S6_ACCESSRULES_ALLOW ; break ;
    case 1 :
      result = s6_accessrules_uidgid_fs(*uid, *gid, rules, &params) ; break ;
    case 2 :
      result = s6_accessrules_uidgid_cdb(*uid, *gid, &cdbmap, &params) ; break ;
    default : break ;
  }
  if (result != S6_ACCESSRULES_ALLOW)
  {
    if (verbosity && (result == S6_ACCESSRULES_ERROR))
       strerr_warnw1sys("error while checking rules") ;
    return 0 ;
  }
  if (params.exec.len && verbosity)
  {
    char fmtuid[UID_FMT] ;
    char fmtgid[GID_FMT] ;
    fmtuid[uid_fmt(fmtuid, *uid)] = 0 ;
    fmtgid[gid_fmt(fmtgid, *gid)] = 0 ;
    strerr_warnw4x("unused exec string in rules for uid ", fmtuid, " gid ", fmtgid) ;
  }
  if (params.env.s)
  {
    size_t n = byte_count(params.env.s, params.env.len, '\0') ;
    char const *envp[n+1] ;
    if (!env_make(envp, n, params.env.s, params.env.len))
    {
      if (verbosity) strerr_warnwu1sys("env_make") ;
      s6_accessrules_params_free(&params) ;
      return 0 ;
    }
    envp[n] = 0 ;
    if (!parse_env(envp, idstr_re, interfaces_re, flags, &done))
    {
      if (verbosity) strerr_warnwu1sys("parse_env") ;
      s6_accessrules_params_free(&params) ;
      return 0 ;
    }
  }
  s6_accessrules_params_free(&params) ;
  if (!(done & 1)) defaultre(idstr_re, pubflags & 1) ;
  if (!(done & 2)) defaultre(interfaces_re, pubflags & 2) ;
  return 1 ;
}

int main (int argc, char const *const *argv, char const *const *envp)
{
  int spfd ;
  int flag1 = 0 ;
  uint32_t maxconn = 64 ;
  unsigned int pubflags = 0 ;
  PROG = "skabus-rpcd" ;

  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    unsigned int t = 0, T = 0 ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "v:SsJj1i:x:t:T:c:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'v' : if (!uint0_scan(l.arg, &verbosity)) dieusage() ; break ;
        case 'S' : pubflags &= ~1U ; break ;
        case 's' : pubflags |= 1U ; break ;
        case 'J' : pubflags &= ~2U ; break ;
        case 'j' : pubflags |= 2U ; break ;
        case '1' : flag1 = 1 ; break ;
        case 'i' : rules = l.arg ; rulestype = 1 ; break ;
        case 'x' : rules = l.arg ; rulestype = 2 ; break ;
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        case 'T' : if (!uint0_scan(l.arg, &T)) dieusage() ; break ;
        case 'c' : if (!uint320_scan(l.arg, &maxconn)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (t) tain_from_millisecs(&answertto, t) ;
    if (T) tain_from_millisecs(&lameduckdeadline, T) ;
  }
  if (maxconn > SKABUS_RPC_MAX) maxconn = SKABUS_RPC_MAX ;
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

  if (rulestype == 2)
  {
    cdbfd = open_readb(rules) ;
    if (cdbfd < 0) strerr_diefu3sys(111, "open ", rules, " for reading") ;
    if (cdb_init(&cdbmap, cdbfd) < 0)
      strerr_diefu2sys(111, "cdb_init ", rules) ;
  }

  {
    genset clientinfo ;
    client_t clientstorage[1+maxconn] ;
    uint32_t clientfreelist[1+maxconn] ;
    iopause_fd x[2 + (maxconn << 1)] ;
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
    tain_now_set_stopwatch_g() ;

    for (;;)
    {
      tain_t deadline ;
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
        else if (!new_connection(fd, &uid, &gid, &idstr_re, &interfaces_re, &flags, pubflags))
          fd_close(fd) ;
        else client_add(&i, &idstr_re, &interfaces_re, uid, gid, fd, flags) ;
      }
    }
  }
  return 0 ;
}
