/* ISC license. */

#include <skalibs/types.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>
#include <s6/config.h>
#include <skabus/config.h>
#include "skabus-rpcc.h"

#define USAGE "skabus-rpc-client [ -v verbosity ] [ -d | -D ] [ -1 ] [ -c maxconn ] [ -b backlog ] [ -t timeout ] [ -T lameducktimeout ] [ -C pmprog:sep:flags ] [ -y ifname:ifprog:sep:flags ... ] rpccpath rpcdpath clientname"
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv, char const *const *envp)
{
  unsigned int verbosity = 1 ;
  int flag1 = 0 ;
  int flagreuse = 1 ;
  unsigned int maxconn = 0 ;
  unsigned int backlog = (unsigned int)-1 ;
  unsigned int timeout = 0 ;
  unsigned int ltimeout = 0 ;
  PROG = "skabus-rpc-client" ;
  unsigned int ifn = 1 ;
  char const *interfaces[SKABUS_RPCC_INTERFACES_MAX] = { 0 } ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      register int opt = subgetopt_r(argc, argv, "Dd1v:c:b:t:T:C:y:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'D' : flagreuse = 0 ; break ;
        case 'd' : flagreuse = 1 ; break ;
        case '1' : flag1 = 1 ; break ;
        case 'v' : if (!uint0_scan(l.arg, &verbosity)) dieusage() ; break ;
        case 'c' : if (!uint0_scan(l.arg, &maxconn)) dieusage() ; if (!maxconn) maxconn = 1 ; break ;
        case 'b' : if (!uint0_scan(l.arg, &backlog)) dieusage() ; break ;
        case 't' : if (!uint0_scan(l.arg, &timeout)) dieusage() ; break ;
        case 'T' : if (!uint0_scan(l.arg, &ltimeout)) dieusage() ; break ;
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
    if (argc < 3) dieusage() ;
  }

  {
    unsigned int m = 0, pos = 0 ;
    char fmt[UINT_FMT * 5] ;
    char const *newargv[22 + 2 * ifn] ;
    newargv[m++] = S6_EXTBINPREFIX "s6-ipcserver-socketbinder" ;
    if (!flagreuse) newargv[m++] = "-D" ;
    newargv[m++] = "-a" ;
    newargv[m++] = "0700" ;
    if (backlog != (unsigned int)-1)
    {
      newargv[m++] = "-b" ;
      newargv[m++] = fmt + pos ;
      pos += uint_fmt(fmt + pos, backlog) ;
      fmt[pos++] = 0 ;
    }
    newargv[m++] = "--" ;
    newargv[m++] = *argv++ ;
    newargv[m++] = SKABUS_BINPREFIX "skabus-rpcc" ;
    if (verbosity != 1)
    {
      newargv[m++] = "-v" ;
      newargv[m++] = fmt + pos ;
      pos += uint_fmt(fmt + pos, verbosity) ;
      fmt[pos++] = 0 ;
    }
    if (flag1) newargv[m++] = "-1" ;
    if (maxconn)
    {
      newargv[m++] = "-c" ;
      newargv[m++] = fmt + pos ;
      pos += uint_fmt(fmt + pos, maxconn) ;
      fmt[pos++] = 0 ;
    }
    if (timeout)
    {
      newargv[m++] = "-t" ;
      newargv[m++] = fmt + pos ;
      pos += uint_fmt(fmt + pos, timeout) ;
      fmt[pos++] = 0 ;
    }
    if (ltimeout)
    {
      newargv[m++] = "-T" ;
      newargv[m++] = fmt + pos ;
      pos += uint_fmt(fmt + pos, timeout) ;
      fmt[pos++] = 0 ;
    }
    if (interfaces[0])
    {
      newargv[m++] = "-X" ;
      newargv[m++] = interfaces[0] ;
    }
    for (unsigned int i = 1 ; i < ifn ; i++)
    {
      newargv[m++] = "-y" ;
      newargv[m++] = interfaces[i] ;
    }
    newargv[m++] = "--" ;
    newargv[m++] = *argv++ ;
    newargv[m++] = *argv++ ;
    newargv[m++] = 0 ;
    xpathexec_run(newargv[0], newargv, envp) ;
  }
}
