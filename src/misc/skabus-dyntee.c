/* ISC license. */

#include <unistd.h>
#include <limits.h>

#include <skalibs/types.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr.h>
#include <skalibs/exec.h>

#include <execline/config.h>

#include <s6/config.h>

#include <skabus/config.h>

#define USAGE "skabus-dyntee [ -d | -D ] [ -1 ] [ -c maxconn ] [ -b backlog ] [ -G gid,gid,... ] [ -g gid ] [ -u uid ] [ -U ] [ -t timeout ] [ -T lameducktimeout ] [ -i rulesdir | -x rulesfile ] socketpath"
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv)
{
  int flag1 = 0 ;
  int flagU = 0 ;
  int flagreuse = 1 ;
  uid_t uid = 0 ;
  gid_t gid = 0 ;
  gid_t gids[NGROUPS_MAX] ;
  size_t gidn = (size_t)-1 ;
  unsigned int maxconn = 0 ;
  unsigned int backlog = (unsigned int)-1 ;
  unsigned int timeout = 0 ;
  unsigned int ltimeout = 0 ;
  char const *rulesdir = 0 ;
  char const *rulesfile = 0 ;
  PROG = "skabus-dyntee" ;
  {
    subgetopt l = SUBGETOPT_ZERO ;
    for (;;)
    {
      register int opt = subgetopt_r(argc, argv, "Dd1Uc:b:u:g:G:t:T:i:x:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'D' : flagreuse = 0 ; break ;
        case 'd' : flagreuse = 1 ; break ;
        case '1' : flag1 = 1 ; break ;
        case 'c' : if (!uint0_scan(l.arg, &maxconn)) dieusage() ; if (!maxconn) maxconn = 1 ; break ;
        case 'b' : if (!uint0_scan(l.arg, &backlog)) dieusage() ; break ;
        case 'u' : if (!uid0_scan(l.arg, &uid)) dieusage() ; break ;
        case 'g' : if (!gid0_scan(l.arg, &gid)) dieusage() ; break ;
        case 'G' : if (!gid_scanlist(gids, NGROUPS_MAX, l.arg, &gidn) && *l.arg) dieusage() ; break ;
        case 'U' : flagU = 1 ; uid = 0 ; gid = 0 ; gidn = (unsigned int)-1 ; break ;
        case 't' : if (!uint0_scan(l.arg, &timeout)) dieusage() ; break ;
        case 'T' : if (!uint0_scan(l.arg, &ltimeout)) dieusage() ; break ;
        case 'i' : rulesdir = l.arg ; rulesfile = 0 ; break ;
        case 'x' : rulesfile = l.arg ; rulesdir = 0 ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (!argc) dieusage() ;
  }

  {
    size_t pos = 0 ;
    unsigned int m = 0 ;
    int fdin = dup(0) ;
    char const *newargv[31] ;
    char fmt[UINT_FMT * 5 + UID_FMT + GID_FMT * (1 + NGROUPS_MAX)] ;
    if (fdin < 0) strerr_diefu1sys(111, "dup(0)") ;    
    if (fdin < 3) strerr_dief1x(100, "invalid standard file descriptors") ;
    pos += uint_fmt(fmt + pos, fdin) ;
    fmt[pos++] = 0 ;
    newargv[m++] = S6_EXTBINPREFIX "s6-ipcserver-socketbinder" ;
    if (!flagreuse) newargv[m++] = "-D" ;
    if (backlog != (unsigned int)-1)
    {
      newargv[m++] = "-b" ;
      newargv[m++] = fmt + pos ;
      pos += uint_fmt(fmt + pos, backlog) ;
      fmt[pos++] = 0 ;
    }
    newargv[m++] = "--" ;
    newargv[m++] = argv[0] ;
    if (flagU || uid || gid || gidn != (size_t)-1)
    {
      newargv[m++] = S6_EXTBINPREFIX "s6-applyuidgid" ;
      if (flagU) newargv[m++] = "-Uz" ;
      if (uid)
      {
        newargv[m++] = "-u" ;
        newargv[m++] = fmt + pos ;
        pos += uid_fmt(fmt + pos, uid) ;
        fmt[pos++] = 0 ;
      }
      if (gid)
      {
        newargv[m++] = "-g" ;
        newargv[m++] = fmt + pos ;
        pos += gid_fmt(fmt + pos, gid) ;
        fmt[pos++] = 0 ;
      }
      if (gidn != (size_t)-1)
      {
        newargv[m++] = "-G" ;
        newargv[m++] = fmt + pos ;
        pos += gid_fmtlist(fmt + pos, gids, gidn) ;
        fmt[pos++] = 0 ;
      }
      newargv[m++] = "--" ;
    } 
    newargv[m++] = EXECLINE_EXTBINPREFIX "fdswap" ;
    newargv[m++] = "0" ;
    newargv[m++] = fmt ;
    newargv[m++] = SKABUS_BINPREFIX "skabus-dynteed" ;
    newargv[m++] = "-d" ;
    newargv[m++] = fmt ;
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
      pos += uint_fmt(fmt + pos, ltimeout) ;
      fmt[pos++] = 0 ;
    }
    if (rulesdir)
    {
      newargv[m++] = "-i" ;
      newargv[m++] = rulesdir ;
    }
    else if (rulesfile)
    {
      newargv[m++] = "-x" ;
      newargv[m++] = rulesfile ;
    }
    newargv[m++] = 0 ;
    xexec(newargv) ;
  }
}
