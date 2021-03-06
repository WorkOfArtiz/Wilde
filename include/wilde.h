#ifndef __WILDE_H__
#define __WILDE_H__

#include <uk/config.h>
#include <uk/alloc.h>
#include <stdint.h>
#include <stdbool.h>

/* if ARCH != X64 */
#ifndef CONFIG_ARCH_X86_64
  #error "Yeah, this aint supported on ARM64, sorry"
#endif

#ifndef CONFIG_PLAT_KVM
  #error "Only supported on KVM, sorry (XEN probably also works tho)"
#endif

#ifdef CONFIG_LIBWILDE_DISABLE_INJECTION
  #warning "Wilde is only passing through calls"
#endif

#ifdef __cplusplus
extern "C" {
#endif

void print_pgtables(bool skip_first_gb);

// TMP
void remap_range(void *from, void *to, size_t size);
void unmap_range(void *addr, size_t size);


#ifdef __cplusplus
}
#endif

#endif /* __WILDE_H__ */
