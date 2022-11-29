/* ISC license. */

#ifndef SKABUS_RPCC_H
#define SKABUS_RPCC_H

#include <skalibs/strerr.h>

#define SKABUS_RPCC_BUFSIZE 4095
#define SKABUS_RPCC_MAX 256
#define SKABUS_RPCC_INTERFACES_MAX 64
#define SKABUS_RPCC_QUERIES_MAX 1024

#define X() strerr_dief1x(101, "unexpected error - please submit a bug-report.")
#define dietoomanyinterfaces() strerr_dief1x(100, "too many interface definitions")
#define dieemptyifname() strerr_dief1x(100, "an interface name may not be empty")
#define dieemptyifprog() strerr_dief1x(100, "an interface program may not be empty")

#endif
