/* ISC license. */

#include <skalibs/nonposix.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/socket.h>  /* shutdown */
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include <skalibs/posixplz.h>
#include <skalibs/types.h>
#include <skalibs/siovec.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/sgetopt.h>
#include <skalibs/error.h>
#include <skalibs/bufalloc.h>
#include <skalibs/buffer.h>
#include <skalibs/strerr.h>
#include <skalibs/tai.h>
#include <skalibs/djbunix.h>
#include <skalibs/sig.h>
#include <skalibs/iopause.h>
#include <skalibs/selfpipe.h>
#include <skalibs/cdb.h>

#include <s6/accessrules.h>

#define USAGE "skabus-dynteed [ -d fdsocket ] [ -c maxconn ] [ -1 ] [ -t timeout ] [ -T lameducktimeout ] [ -i rulesdir | -x rulesfile ]"
#define dieusage() strerr_dieusage(100, USAGE) ;
#define dienomem() strerr_diefu1sys(111, "stralloc_catb") ;
#define die() strerr_dief1sys(101, "unexpected error") ;

#define SKABUS_DYNTEE_MAX 1000

static int cont = 1 ;
static tain lameduckdeadline ;

static unsigned int rulestype = 0 ;
static char const *rules = 0 ;
static cdb cdbmap = CDB_ZERO ;

typedef struct client_s client_t, *client_t_ref ;
struct client_s
{
  unsigned int xindex ;
  tain deadline ;
  bufalloc ba ;
} ;

static void client_free (client_t *c)
{
  fd_close(bufalloc_fd(&c->ba)) ;
  bufalloc_free(&c->ba) ;
}

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

static inline int new_connection (int fd)
{
  s6_accessrules_params_t params = S6_ACCESSRULES_PARAMS_ZERO ;
  uid_t uid ;
  gid_t gid ;
  if (!rulestype) return 1 ;
  if (getpeereid(fd, &uid, &gid) < 0)
  {
    strerr_warnwu1sys("getpeereid") ;
    return 0 ;
  }
  if ((rulestype == 1 ? s6_accessrules_uidgid_fs(uid, gid, rules, &params) : s6_accessrules_uidgid_cdb(uid, gid, &cdbmap, &params)) != S6_ACCESSRULES_ALLOW)
    return 0 ;
  s6_accessrules_params_free(&params) ;
  return 1 ;
}

int main (int argc, char const *const *argv, char const *const *envp)
{
  tain readtto ;
  int spfd ;
  int flag1 = 0 ;
  unsigned int maxconn = 40 ;
  unsigned int fdsocket = 3 ;
  PROG = "skabus-dynteed" ;

  {
    subgetopt l = SUBGETOPT_ZERO ;
    unsigned int t = 0, T = 0 ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "d:1c:i:x:t:T:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'd' : if (!uint0_scan(l.arg, &fdsocket)) dieusage() ; break ;
        case '1' : flag1 = 1 ; break ;
        case 'i' : rules = l.arg ; rulestype = 1 ; break ;
        case 'x' : rules = l.arg ; rulestype = 2 ; break ;
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        case 'T' : if (!uint0_scan(l.arg, &T)) dieusage() ; break ;
        case 'c' : if (!uint0_scan(l.arg, &maxconn)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (t) tain_from_millisecs(&readtto, t) ;
    else readtto = tain_infinite_relative ;
    if (T) tain_from_millisecs(&lameduckdeadline, T) ;
    else lameduckdeadline = tain_infinite_relative ;
  }
  if (maxconn > SKABUS_DYNTEE_MAX) maxconn = SKABUS_DYNTEE_MAX ;
  if (!maxconn) maxconn = 1 ;
  {
    struct stat st ;
    if (fstat(fdsocket, &st) < 0) strerr_diefu1sys(111, "fstat socket descriptor") ;
    if (!S_ISSOCK(st.st_mode)) strerr_dief1x(100, "descriptor is not a socket") ;
  }
  if (flag1)
  {
    if (fcntl(1, F_GETFD) < 0)
      strerr_dief1sys(100, "called with option -1 but stdout said") ;
  }
  else close(1) ;
  if (ndelay_on(0) < 0) strerr_diefu1sys(111, "set stdin non-blocking") ;
  spfd = selfpipe_init() ;
  if (spfd < 0) strerr_diefu1sys(111, "selfpipe_init") ;
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

  {
    unsigned int numconn = 0 ;
    client_t clients[maxconn] ;

    if (flag1)
    {
      fd_write(1, "\n", 1) ;
      fd_close(1) ;
    }
    tain_now_set_stopwatch_g() ;

    for (;;)
    {
      tain deadline ;
      int r = 2 ;
      iopause_fd x[2 + cont + numconn] ;
      x[0].fd = spfd ;
      x[0].events = IOPAUSE_READ ;
      x[1].fd = fdsocket ;
      x[1].events = (cont && (numconn < maxconn)) ? IOPAUSE_READ : 0 ;
      if (cont)
      {
        tain_add_g(&deadline, &tain_infinite_relative) ;
        x[2].fd = 0 ;
        x[2].events = IOPAUSE_READ ;
        r++ ;
      }
      else deadline = lameduckdeadline ;
      for (unsigned int i = 0 ; i < numconn ; i++) if (bufalloc_len(&clients[i].ba))
      {
        if (tain_less(&clients[i].deadline, &deadline)) deadline = clients[i].deadline ;
        x[r].fd = bufalloc_fd(&clients[i].ba) ;
        x[r].events = IOPAUSE_WRITE ;
        clients[i].xindex = r++ ;
      }

      r = iopause_g(x, r, &deadline) ;
      if (r < 0) strerr_diefu1sys(111, "iopause") ;

      if (!r)
      {
        for (unsigned int i = 0 ; i < numconn ; i++)
        {
          if (bufalloc_len(&clients[i].ba) && !tain_future(&clients[i].deadline))
          {
            client_free(clients + i) ;
            clients[i--] = clients[--numconn] ;
          }
        }
        if (!(cont || (numconn && tain_future(&lameduckdeadline)))) break ;
        continue ;
      }

      if (x[0].revents & IOPAUSE_READ) handle_signals() ;

      for (unsigned int i = 0 ; i < numconn ; i++) if (bufalloc_len(&clients[i].ba))
      {
        if (x[clients[i].xindex].revents & (IOPAUSE_WRITE | IOPAUSE_EXCEPT))
        {
          if (!bufalloc_flush(&clients[i].ba) && !error_isagain(errno))
          {
            client_free(clients + i) ;
            clients[i--] = clients[--numconn] ;
          }
        }
      }

      if (cont && x[1].revents & IOPAUSE_READ)
      {
        int fd = ipc_accept_nb(fdsocket, 0, 0, 0) ;
        if (fd < 0)
        {
          if (!error_isagain(errno)) strerr_diefu1sys(111, "accept") ;
        }
        else if (!new_connection(fd)) fd_close(fd) ;
        else if (shutdown(fd, SHUT_RD) < 0)
        {
          fd_close(fd) ;
          strerr_warnwu1sys("shutdown client connection for reading - aborting it") ;
        }
        else
        {
          bufalloc_init(&clients[numconn].ba, &fd_write, fd) ;
          tain_copynow(&clients[numconn].deadline) ;
          numconn++ ;
        }
      }

      if (cont && x[2].revents & IOPAUSE_READ)
      {
        ssize_t r = sanitize_read(buffer_fill(buffer_0)) ;
        if (r < 0)
        {
          fd_close(0) ;
          cont = 0 ;
          tain_add_g(&lameduckdeadline, &lameduckdeadline) ;
          if (errno != EPIPE) strerr_warnwu1sys("read from stdin") ;
        }
        if (r > 0)
        {
          struct iovec v[2] ;
          buffer_rpeek(buffer_0, v) ;
          for (unsigned int i = 0 ; i < numconn ; i++)
          {
            if (!bufalloc_putv(&clients[i].ba, v, 2)) dienomem() ;
            if (!tain_future(&clients[i].deadline)) tain_add_g(&clients[i].deadline, &readtto) ;
          }
          buffer_rseek(buffer_0, siovec_len(v, 2)) ;
        }
      }

      if (!cont)
      {
        for (unsigned int i = 0 ; i < numconn ; i++)
        {
          if (!bufalloc_len(&clients[i].ba))
          {
            client_free(clients + i) ;
            clients[i--] = clients[--numconn] ;
          }
        }
        if (!numconn) break ;
      }
    }
  }
  return 0 ;
}
