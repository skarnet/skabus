/* ISC license. */

#include <skalibs/nonposix.h>
#include <sys/socket.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>
#include <skalibs/webipc.h>

#define USAGE "skabus-dyntee-client path prog..."
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv, char const *const *envp)
{
  PROG = "skabus-dyntee-client" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (argc < 2) dieusage() ;
  }

  {
    int fd = ipc_stream_b() ;
    if (fd < 0) strerr_diefu1sys(111, "create socket") ;
    if (!ipc_connect(fd, argv[0])) strerr_diefu2sys(111, "connect to ", argv[0]) ;
    if (shutdown(fd, SHUT_WR) < 0) strerr_diefu1sys(111, "shutdown socket for writing") ;
    if (fd_move(0, fd) < 0) strerr_diefu1sys(111, "move socket fd to stdin") ;
  }
  xpathexec_run(argv[1], argv+1, envp) ;
}
