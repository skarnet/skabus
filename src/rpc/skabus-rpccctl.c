/* ISC license. */

#include <stdint.h>
#include <string.h>
#include <skalibs/types.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/tai.h>
#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include "skabus-rpccctl.h"

#define USAGE "skabus-rpccctl [ -t timeout ] help|interface|interface-remove|query args..."
#define dieusage() strerr_dieusage(100, USAGE)

static tain_t deadline ;
static skabus_rpcc_t a = SKABUS_RPCC_ZERO ;

static void add_interface (char const *rpccpath, char const *ifname, char const *ifprog, char const *re)
{
  if (!skabus_rpcc_start_g(&a, rpccpath, &deadline))
    strerr_diefu2sys(111, "start session with skabus-rpcc instance at ", rpccpath) ;
  if (!skabus_rpcc_interface_register_g(&a, ifname, ifprog, re, &deadline))
    strerr_diefu6sys(111, "register interface ", ifname, " with body ", ifprog, " and regex ", re) ;
  skabus_rpcc_end(&a) ;
}

static void remove_interface (char const *rpccpath, char const *ifname)
{
  if (!skabus_rpcc_start_g(&a, rpccpath, &deadline))
    strerr_diefu2sys(111, "start session with skabus-rpcc instance at ", rpccpath) ;
  if (!skabus_rpcc_interface_unregister_g(&a, ifname, &deadline))
    strerr_diefu2sys(111, "unregister interface ", ifname) ;
  skabus_rpcc_end(&a) ;
}

static void query (char const *rpccpath, char const *ifname, char const *query, uint32_t timeout)
{
  stralloc sa = STRALLOC_ZERO ;
  if (!skabus_rpcc_start_g(&a, rpccpath, &deadline))
    strerr_diefu2sys(111, "start session with skabus-rpcc instance at ", rpccpath) ;
  if (!skabus_rpcc_query_g(&a, &sa, ifname, query, timeout, &deadline))
    strerr_diefu2sys(111, "send query to interface ", ifname) ;
  skabus_rpcc_end(&a) ;
  if (buffer_putflush(buffer_1, sa.s, sa.len) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
  stralloc_free(&sa) ;
}

static void quit (char const *rpccpath)
{
  if (!skabus_rpcc_start_g(&a, rpccpath, &deadline))
    strerr_diefu2sys(111, "start session with skabus-rpcc instance at ", rpccpath) ;
  if (!skabus_rpcc_quit_g(&a, &deadline))
    strerr_diefu1sys(111, "send quit command") ;
  skabus_rpcc_end(&a) ;
}

static inline void print_help (void)
{
  static char const *help =
"skabus-rpccctl help\n"
"skabus-rpccctl [ -t timeout ] interface rpccpath interface-name interface-program clientid-regex\n"
"skabus-rpccctl [ -t timeout ] interface-remove rpccpath interface-name\n"
"skabus-rpccctl [ -t timeout ] [ -T limit ] query rpccpath interface-name query\n"
"skabus-rpccctl quit\n" ;
  if (buffer_putsflush(buffer_1, help) < 0)
    strerr_diefu1sys(111, "write to stdout") ;
}

static inline unsigned int lookup (char const *const *table, char const *command)
{
  unsigned int i = 0 ;
  for (; table[i] ; i++) if (!strcmp(command, table[i])) break ;
  return i ;
}

static inline unsigned int parse_command (char const *command)
{
  static char const *const command_table[13] =
  {
    "help",
    "interface",
    "interface-remove",
    "query",
    "quit",
    0
  } ;
  unsigned int i = lookup(command_table, command) ;
  if (!command_table[i]) dieusage() ;
  return i ;
}

int main (int argc, char const *const *argv)
{
  unsigned int what ;
  uint32_t timeout = 0 ;
  PROG = "skabus-rpccctl" ;
  {
    unsigned int t = 0 ;
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "t:T:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 't' : if (!uint0_scan(l.arg, &t)) dieusage() ; break ;
        case 'T' : if (!uint320_scan(l.arg, &timeout)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (t) tain_from_millisecs(&deadline, t) ;
    else deadline = tain_infinite_relative ;
  }

  if (!argc) dieusage() ;

  tain_now_set_stopwatch() ;
  tain_now_g() ;
  tain_add_g(&deadline, &deadline) ;

  what = parse_command(argv[0]) ;
  switch (what)
  {
    case 0 :
      print_help() ;
      break ;
    case 1 :
      if (argc < 5) dieusage() ;
      add_interface(argv[2], argv[3], argv[4], argv[5]) ;
      break ;
    case 2 :
      if (argc < 3) dieusage() ;
      remove_interface(argv[2], argv[3]) ;
      break ;
    case 3 :
      if (argc < 4) dieusage() ;
      query(argv[2], argv[3], argv[4], timeout) ;
      break ;
    case 4 :
      if (argc < 2) dieusage() ;
      quit(argv[2]) ;
      break ;
    default : dieusage() ;
  }
  return 0 ;
}
