#ifndef __WILDE_H__
#define __WILDE_H__

/*
 * The implemenation exists of 3 parts:
 *
 * [X] allocator injection, implemented in shimming.[ch]
 * [ ] managing allocated aliases, alias.[ch]
 * [ ] finding unused memory space
 * [ ] remapping pages with correct access rights
 */

#include <uk/config.h>
#include <uk/alloc.h>

/* if ARCH != X64 */
#ifndef CONFIG_ARCH_X86_64
#error "Yeah, this aint supported on ARM64, sorry"
#endif

#ifndef CONFIG_PLAT_KVM
#error "Only supported on KVM, sorry (XEN probably also works tho)"
#endif

#ifdef CONFIG_LIBWILDE_SHAUN
#warning "Electric sheep isn't fully tested, use at your own risk"
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct uk_alloc *wilde_init(void);
void print_pgtables(void);

#ifdef __cplusplus
}
#endif

#endif /* __WILDE_H__ */
