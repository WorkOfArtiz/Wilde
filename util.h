#ifndef __WILDE_UTIL_H__
#define __WILDE_UTIL_H__

/* page size */
#include <uk/arch/limits.h>

/* aligning */
#include <uk/essentials.h>

#define UNUSED(X) (void)(X)

/* Rounding operations (efficient when n is a power of 2)
 * Round down to the nearest multiple of n */
#define ROUNDDOWN(a, n)                                                        \
({                                                                             \
    uintptr_t __a = (uintptr_t) (a);                                           \
    (typeof(a)) (__a - __a % (n));                                             \
})

/* Round up to the nearest multiple of n */
#define ROUNDUP(a, n)                                                          \
({                                                                             \
    uintptr_t __n = (uintptr_t) (n);                                           \
    (typeof(a)) (ROUNDDOWN((uintptr_t) (a) + __n - 1, __n));                   \
})

#ifndef COLOR
#define COLOR "31"
#endif

#define lprintf(fmt, ...) \
    hprintf("[\033[1;" COLOR "m%16s\033[0m] " fmt, __func__, ##__VA_ARGS__)

#define hprintf(...) \
  ({ int ret; while ((ret = printf(__VA_ARGS__)) == -EINTR || ret == -EAGAIN) {} ret; })

#endif