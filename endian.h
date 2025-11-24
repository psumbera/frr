/* substitution of system endina.h for Solaris */

#if !defined(__sun__)

#ifdef HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#endif
#include <endian.h>

#else

#ifndef _ENDIAN_H
#define _ENDIAN_H

#include <arpa/inet.h> 
#include <sys/types.h>
#include <sys/byteorder.h>

// Numeric values commonly used for endian macros:
#define LITTLE_ENDIAN 1234
#define BIG_ENDIAN    4321
#define PDP_ENDIAN    3412 // Rare, but occasionally referenced

#if defined(__sparc) || defined(__sparcv9)
#  define BYTE_ORDER    BIG_ENDIAN
#  define __BYTE_ORDER  BIG_ENDIAN
#elif defined(__i386) || defined(__amd64) || defined(__x86_64)
#  define BYTE_ORDER    LITTLE_ENDIAN
#  define __BYTE_ORDER  LITTLE_ENDIAN
#else
#  error "Unsupported architecture"
#endif

// Byte swap wrappers, mapping to Solaris equivalents if used by your codebase
#ifndef bswap_32
#  define bswap_32(x) BSWAP_32(x)
#endif

#ifndef bswap_16
#  define bswap_16(x) BSWAP_16(x)
#endif

#ifndef htobe64
static inline uint64_t htobe64(uint64_t x) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (((uint64_t)htonl(x & 0xFFFFFFFFULL)) << 32) | htonl(x >> 32);
#else
    return x;
#endif
}
#endif

#ifndef be64toh
static inline uint64_t be64toh(uint64_t x) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (((uint64_t)ntohl(x & 0xFFFFFFFFULL)) << 32) | ntohl(x >> 32);
#else
    return x;
#endif
}
#endif

#endif /* _ENDIAN_H */

#endif
