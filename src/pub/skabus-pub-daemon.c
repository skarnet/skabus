/* ISC license. */

#include <sys/types.h>
#include <limits.h>

#include <skalibs/types.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr.h>
#include <skalibs/exec.h>

#include <s6/config.h>

#include <skabus/config.h>

#define USAGE "skabus-pub-daemon [ -v verbosity ] [ -d | -D ] [ -1 ] [ -c maxconn ] [ -b backlog ] [ -G gid,gid,... ] [ -g gid ] [ -u uid ] [ -U ] [ -t timeout ] [ -T lameducktimeout ] [ -i rulesdir | -x rulesfile ] [ -S | -s ] [ -k announcere ] path msgfsdir"
#define dieusage() strerr_dieusage(100, USAGE)

int main (int argc, char const *const *argv)
{
  unsigned int verbosity = 1 ;
  int flag1 = 0 ;
  int flagU = 0 ;
  int flagreuse = 1 ;
  uid_t uid = 0 ;
  gid_t gid = 0 ;
  gid_t gids[NGROUPS_MAX] ;
  size_t gidn = (size_t)-1 ;
  unsigned int maxconn = 0 ;
  unsigned int backlog = (unsigned int)-1 ;
  int flagpublic = 0 ;
  unsigned int timeout = 0 ;
  unsigned int ltimeout = 0 ;
  char const *rulesdir = 0 ;
  char const *rulesfile = 0 ;
  char const *announcere = 0 ;
  PROG = "skabus-pub-daemon" ;
  {
    subgetopt l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "Dd1USsv:c:b:u:g:G:t:T:i:x:k:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'D' : flagreuse = 0 ; break ;
        case 'd' : flagreuse = 1 ; break ;
        case '1' : flag1 = 1 ; break ;
        case 'v' : if (!uint0_scan(l.arg, &verbosity)) dieusage() ; break ;
        case 'c' : if (!uint0_scan(l.arg, &maxconn)) dieusage() ; if (!maxconn) maxconn = 1 ; break ;
        case 'b' : if (!uint0_scan(l.arg, &backlog)) dieusage() ; break ;
        case 'u' : if (!uid0_scan(l.arg, &uid)) dieusage() ; break ;
        case 'g' : if (!gid0_scan(l.arg, &gid)) dieusage() ; break ;
        case 'G' : if (!gid_scanlist(gids, NGROUPS_MAX, l.arg, &gidn) && *l.arg) dieusage() ; break ;
        case 'U' : flagU = 1 ; uid = 0 ; gid = 0 ; gidn = (size_t)-1 ; break ;
        case 'S' : flagpublic = 0 ; break ;
        case 's' : flagpublic = 1 ; break ;
        case 't' : if (!uint0_scan(l.arg, &timeout)) dieusage() ; break ;
        case 'T' : if (!uint0_scan(l.arg, &ltimeout)) dieusage() ; break ;
        case 'i' : rulesdir = l.arg ; rulesfile = 0 ; break ;
        case 'x' : rulesfile = l.arg ; rulesdir = 0 ; break ;
        case 'k' : announcere = l.arg ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (argc < 2) dieusage() ;
  }

  {
    unsigned int m = 0, pos = 0 ;
    char const *newargv[33] ;
    char fmt[UINT_FMT * 4 + UID_FMT + GID_FMT * (1 + NGROUPS_MAX)] ;
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
    newargv[m++] = *argv++ ;
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
      if (gidn != (unsigned int)-1)
      {
        newargv[m++] = "-G" ;
        newargv[m++] = fmt + pos ;
        pos += gid_fmtlist(fmt + pos, gids, gidn) ;
        fmt[pos++] = 0 ;
      }
      newargv[m++] = "--" ;
    }
    newargv[m++] = SKABUS_BINPREFIX "skabus-pubd" ;
    if (verbosity != 1)
    {
      newargv[m++] = "-v" ;
      newargv[m++] = fmt + pos ;
      pos += uint_fmt(fmt + pos, verbosity) ;
      fmt[pos++] = 0 ;
    }
    if (flag1) newargv[m++] = "-1" ;
    if (flagpublic) newargv[m++] = "-s" ;
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
    if (announcere)
    {
      newargv[m++] = "-k" ;
      newargv[m++] = announcere ;
    }
    newargv[m++] = "--" ;
    newargv[m++] = *argv++ ;
    newargv[m++] = 0 ;
    xexec(newargv) ;
  }
}
