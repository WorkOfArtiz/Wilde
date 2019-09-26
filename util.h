#ifndef __WILDE_UTIL_H__
#define __WILDE_UTIL_H__

/* page size */
#include <uk/arch/limits.h>

/* aligning */
#include <uk/essentials.h>

/* configurations */
#include <uk/config.h>

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

// #ifndef COLOR
// #define COLOR "31"
// #endif

#ifdef CONFIG_LIBWILDE_DEBUG
#define dprintf lprintf
#else
#define dprintf(...) do {} while(0)
#endif

#ifdef CONFIG_LIBWILDE_COLOR
#define lprintf(fmt, ...)                                                      \
    ({                                                                         \
      char $buf[50 + sizeof(__func__) * 3];                                    \
      sprintf($buf, "[\033[1;" COLOR "mwilde:%s\033[0m]", __func__);           \
      hprintf("%-30s " fmt, $buf, ##__VA_ARGS__);                              \
    })

#else
#define lprintf(fmt, ...)                                                      \
    ({                                                                         \
      char $buf[50 + sizeof(__func__) * 3];                                    \
      sprintf($buf, "[wilde:%s]", __func__);                                   \
      hprintf("%-20s " fmt, $buf, ##__VA_ARGS__);                              \
    })
#endif

#define hprintf(...) \
  ({ int ret; while ((ret = printf(__VA_ARGS__)) == -EINTR || ret == -EAGAIN) {} ret; })

#endif