#ifndef __WILDE_UTIL_H__
#define __WILDE_UTIL_H__

/* snprintf */
#include <stdio.h>

/* strlen */
#include <string.h>

#include <stdint.h>
#include <stddef.h>

#ifndef TEST

/* uk_coutk */
#include <uk/plat/console.h>

/* page size */
#include <uk/arch/limits.h>

/* aligning */
#include <uk/essentials.h>

/* configurations */
#include <uk/config.h>

#endif


#define UNUSED(X) (void)(X)

/* Rounding operations (efficient when n is a power of 2)
 * Round down to the nearest multiple of n */
#define ROUNDDOWN(a, n)                                                        \
  ({                                                                           \
    uintptr_t __a = (uintptr_t)(a);                                            \
    (typeof(a))(__a - __a % (n));                                              \
  })

/* Round up to the nearest multiple of n */
#define ROUNDUP(a, n)                                                          \
  ({                                                                           \
    uintptr_t __n = (uintptr_t)(n);                                            \
    (typeof(a))(ROUNDDOWN((uintptr_t)(a) + __n - 1, __n));                     \
  })

/* bit hacks for power of 2 */
#define IS_POWER_2(a)                                                          \
  ({                                                                           \
    uintptr_t __a = (uintptr_t)(a); /* only eval a once */                     \
    __a ? (__a & (__a - 1)) == 0 : 1;                                          \
  })

/* bit hack for LOG2, WILL RETURN -1 on 0 */
#define LOG2(a)                                                                \
  ({                                                                           \
    uintptr_t __a = (uintptr_t)(a); /* only eval a once */                     \
    __a ? 63 - __builtin_clzl(__a) : -1;                                       \
  })

// #ifndef COLOR
// #define COLOR "31"
// #endif

#ifdef CONFIG_LIBWILDE_DEBUG
#define dprintf lprintf
#else
#define dprintf(...)                                                           \
  do {                                                                         \
  } while (0)
#endif

#ifdef CONFIG_LIBWILDE_COLOR

#define COLOR_RED "\033[1;31m"
#define COLOR_GREEN "\033[1;32m"
#define COLOR_YELLOW "\033[1;33m"
#define COLOR_BLUE "\033[1;34m"
#define COLOR_PURPLE "\033[1;35m"
#define COLOR_CYAN "\033[1;36m"
#define COLOR_WHITE "\033[1;37m"
#define COLOR_RST "\033[0m"

#ifndef COLOR
#define COLOR COLOR_PURPLE
#endif

#define lprintf(fmt, ...)                                                      \
  ({                                                                           \
    char $buf[50 + sizeof(__func__) * 3];                                      \
    sprintf($buf, "[" COLOR "%s" COLOR_RST "]", __func__);               \
    hprintf("%-30s " fmt, $buf, ##__VA_ARGS__);                                \
  })

#else

#define COLOR_RED ""
#define COLOR_GREEN ""
#define COLOR_YELLOW ""
#define COLOR_BLUE ""
#define COLOR_PURPLE ""
#define COLOR_CYAN ""
#define COLOR_WHITE ""
#define COLOR_RST ""

#define lprintf(fmt, ...)                                                      \
  ({                                                                           \
    char $buf[50 + sizeof(__func__) * 3];                                      \
    sprintf($buf, "[%s]", __func__);                                     \
    hprintf("%-20s " fmt, $buf, ##__VA_ARGS__);                                \
  })

#endif

#define hprintf(...)                                                           \
  ({                                                                           \
    int ret;                                                                   \
    char $$$buf[1024];                                                         \
    int len = snprintf($$$buf, 1023, __VA_ARGS__);                             \
    while ((ret = ukplat_coutk($$$buf, len)) == -EINTR || ret == -EAGAIN) {    \
    }                                                                          \
    ret;                                                                       \
  })
/*
#define hprintf(...)                                                           \
  ({                                                                           \
    int ret;                                                                   \
    char $$$buf[1024];                                                         \
    int len = snprintf($$$buf, 1023, __VA_ARGS__);                             \
    ret = ukplat_coutk($$$buf, len);                                           \
    ret;                                                                       \
  })
*/
#endif
