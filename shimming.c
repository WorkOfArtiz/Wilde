// vim: set fdm=marker:
/***************************************************************************
 *
 * This file contains the shim implementation, it registers a new allocator
 * that passes all memory interface calls on to the previous allocator.
 *
 * This is done to modify the return values, hardening the interface, the
 * implementation of the hardening is in wilde_internal.{c,h}
 */

// includes {{{
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>

/* uk specific */
#include <uk/alloc.h>           /* alloc interface */
#include <uk/allocbbuddy.h>     /* the default allocator */
#include <uk/plat/console.h>    /* output functions */
#include <uk/arch/limits.h>     /* all the standard limits, __PAGE_SIZE etc. */
#include <uk/print.h>
#include <uk/assert.h>
#include <uk/errptr.h>

/* wilde specific */
#include <wilde.h>
#include "wilde_internal.h"
#define COLOR COLOR_YELLOW
#include "util.h"
#include "vma.h"
// }}}

// macros {{{
/* locking functionality for multithreading, coarse grained locking */
#ifdef CONFIG_LIBWILDE_LOCKING
  #include <uk/mutex.h>

  static struct uk_mutex global_mutex = UK_MUTEX_INITIALIZER(global_mutex);
  #define alloc_lock()   do { uk_mutex_lock(&global_mutex); } while (0)
  #define alloc_unlock() do { uk_mutex_unlock(&global_mutex); } while (0)
#else
  #define alloc_lock() do {} while (0)
  #define alloc_unlock() do {} while (0)
#endif

/* debug prints are done with alloc_printf */
#ifdef CONFIG_LIBWILDE_ALLOC_DEBUG
  #define alloc_printf lprintf
#else
  #define alloc_printf(...) do {} while (0)
#endif

#ifdef CONFIG_LIBWILDE_INIT_MEMORY
  #define CLEAR(Mem, Size) memset((Mem), CONFIG_LIBWILDE_INIT_MEMORY_VALUE, (Size));
#else
  #define CLEAR(Mem, Size) do {} while (0)
#endif

#ifdef CONFIG_LIBWILDE_KELLOGS
#ifdef CONFIG_LIBWILDE_DISABLE_INJECTION
  #error "Kellogs depends on the wilde engine"
#endif
  #include "kallocs_malloc.h"

  #define kmalloc(Size)                        kallocs_malloc((Size))
  #define kcalloc(Nmemb, Size)                 kallocs_calloc((Nmemb), (Size))
  #define kmemalign(Align, Size)               kallocs_memalign((Align), (Size))
  #define krealloc(Ptr, OldSize, NewSize)      kallocs_realloc((Ptr), (OldSize), (NewSize))
  #define kfree(Ptr, Size)                     kallocs_free((Ptr),(Size))
#else
  #define kmalloc(Size)                        shimmed->malloc(shimmed, (Size))
  #define kcalloc(Nmemb, Size)                 shimmed->calloc(shimmed, (Nmemb), (Size))
  #define kmemalign(Align, Size)               shimmed->memalign(shimmed, (Align), (Size))
  #define krealloc(Ptr, OldSize, NewSize)      shimmed->realloc(shimmed, (Ptr), (NewSize))
  #define kfree(Ptr, Size)                     shimmed->free(shimmed, (Ptr))
#endif
// }}}

/* global data structures */
struct uk_alloc *shimmed; /* the allocator on top of this */
struct uk_alloc shim;     /* the shim itself, the new allocator */

/*
 * small function that allows for dumping the stack, based on rip being 1
 * above local vars
 */
static inline void *get_caller_address(int level)
{
  void **bp;

  asm("movq %%rbp, %0" : "=r"(bp));

  while (level-- && *bp)
    bp = *(void ***) bp;

  return bp[1];
}

// shim_malloc {{{
void *shim_malloc(struct uk_alloc *a, size_t size)
{
  UNUSED(a);
  UK_ASSERT(a);
  UK_ASSERT(size);

#ifdef CONFIG_LIBWILDE_DISABLE_INJECTION

  /* version without wilde */
  char *address = kmalloc(size);
  if (address == NULL) {
    alloc_printf("malloc(%zu) => NULL\n", size);
    return NULL;
  }
  alloc_printf("malloc(size=%zu) => %p\n", size, res);
  return address;

#else

  /* version with wilde */
  char *real_addr = kmalloc(size);
  UK_ASSERT(real_addr != 0);

  alloc_lock();
  char *alias_addr = wilde_map_new(real_addr, size, __PAGE_SIZE);
  alloc_unlock();

  CLEAR(alias_addr, size);

  alloc_printf("malloc(size=%zu) => %p [real=%p]\n", size, alias_addr, real_addr);
  return alias_addr;

#endif
}
// }}}

// shim_calloc {{{
void *shim_calloc(struct uk_alloc *a, size_t nmemb, size_t size)
{
  UNUSED(a);

#ifdef CONFIG_LIBWILDE_DISABLE_INJECTION

  /* version without wilde */
  char *address = kcalloc(nmemb, size);
  if (address == NULL) {
    alloc_printf("calloc(nmemb=%zu, size=%zu) => NULL\n", nmemb, size);
    return NULL;
  }

  alloc_printf("calloc(nmemb=%zu, size=%zu) => %p []\n", nmemb, size, address);
  return address;

#else

  /* version with wilde */
  char *real_addr = kcalloc(nmemb, size);

  alloc_lock();
  char *alias_addr = wilde_map_new(real_addr, nmemb * size, __PAGE_SIZE);
  alloc_unlock();
  alloc_printf("calloc(nmemb=%zu, size=%zu) => %p [real=%p]\n", nmemb, size, alias_addr, real_addr);

  return alias_addr;

#endif
}
// }}}

// shim_posix_memalign {{{
int shim_posix_memalign(struct uk_alloc *a, void **memptr, size_t align, size_t size)
{
  UNUSED(a);
  UK_ASSERT(size);
  UK_ASSERT(size > align);

#ifdef CONFIG_LIBWILDE_DISABLE_INJECTION

  /* version without wilde */
  int res = shimmed->posix_memalign(shimmed, memptr, align, size);
  if (res != 0) {
    alloc_printf("posix_memalign(memptr=%p, align=%zu, size=%zu) => %d []\n", memptr, align, size, res);
    return res;
  }

  alloc_printf("posix_memalign(memptr=%p, align=%zu, size=%zu) => 0 [memptr=%p]\n", memptr, align, size, real_addr);

#else

  /* version with wilde */
  void *real_addr = kmemalign(align, size);
  alloc_lock();
  *memptr = wilde_map_new(real_addr, size, ROUNDUP(align, __PAGE_SIZE));
  alloc_unlock();

  alloc_printf("posix_memalign(memptr=%p, align=%zu, size=%zu) => 0 [memptr=%p, real=%p]\n", memptr, align, size, *memptr, real_addr);

#endif

  CLEAR(*memptr, size);
  return 0;
}
// }}}

// shim_memalign {{{
void *shim_memalign(struct uk_alloc *a, size_t align, size_t size)
{
  UNUSED(a);
  UK_CRASH("Does anyone even use this at all?");

#ifdef CONFIG_LIBWILDE_DISABLE_INJECTION

  /* version without wilde */
  void *address = shimmed->memalign(shimmed, align, size);

  if (address == NULL)
    alloc_printf("memalign(align=%zu, size=%zu) => NULL []\n", align, size);
  else {
    alloc_printf("memalign(align=%zu, size=%zu) => %p []\n", align, size, address);
    CLEAR(address, size);
  }

  return address;

#else

  /* version with wilde */
  void *real_addr = kmemalign(align, size);

  alloc_lock();
  void *alias_addr = wilde_map_new(real_addr, size, ROUNDUP(size, __PAGE_SIZE));
  alloc_unlock();

  alloc_printf("memalign(align=%zu, size=%zu) => %p [real=%p]\n", align, size, alias_addr, real_addr);

  CLEAR(alias_addr, size);
  return alias_addr;
#endif
}
// }}}

// shim_realloc {{{
void *shim_realloc(struct uk_alloc *a, void *ptr, size_t size)
{
  UNUSED(a);

#ifdef CONFIG_LIBWILDE_DISABLE_INJECTION

  /* version without wilde */
  void *address = shimmed->realloc(shimmed, ptr, size);
  alloc_printf("realloc(ptr=%p, size=%ld) => %p []\n", ptr, size, address);
  return address;

#else

  /* edge case */
  if (ptr == NULL) {
    void *real_addr = kmalloc(size);

    alloc_lock();
    void *alias_addr = wilde_map_new(real_addr, size, __PAGE_SIZE);
    alloc_unlock();

    alloc_printf("realloc(ptr=NULL, size=%ld) => %p [real=%p]\n", size, alias_addr, real_addr);
    return alias_addr;
  }


  /* version with wilde */
  size_t old_size;

  alloc_lock();
  void *old_real = wilde_map_rm(ptr, &old_size);
  if (old_real == NULL) {
    alloc_unlock();
    UK_CRASH("[%s] invalid free at %p\n", __func__, ptr);
  }

  void *new_real = krealloc(old_real, old_size, size);
  void *new_alias = wilde_map_new(new_real, size, __PAGE_SIZE);
  alloc_unlock();

  alloc_printf("realloc(ptr=%p, size=%zu) => %p [old_real=%p, new_real=%p]\n", ptr, size, new_alias, old_real, new_real);

  return new_alias;

#endif
}
// }}}

// shim_free {{{
void shim_free(struct uk_alloc *a, void *ptr)
{
  UNUSED(a);

  if (ptr == NULL) {
    alloc_printf("free(ptr=NULL) => 0\n");
    return;
  }

#ifdef CONFIG_LIBWILDE_DISABLE_INJECTION

  /* version without wilde */
  shimmed->free(shimmed, ptr);
  alloc_printf("free(ptr=%p) => 0\n", ptr);

#else

  /* version with wilde */
  size_t size;

  alloc_lock();
  void  *real_addr = wilde_map_rm(ptr, &size);
  alloc_unlock();

  if (real_addr == NULL)
    UK_CRASH("[%s] invalid free at %p\n", __func__, ptr);

  kfree(real_addr, size);
  alloc_printf("free(ptr=%p) => 0 [real_addr=%p, size=%ld]\n", ptr, real_addr, size);

#endif
}
// }}}

// shim_palloc & shim_free {{{
#if CONFIG_LIBUKALLOC_IFPAGES
void *shim_palloc(struct uk_alloc *a, size_t order)
{

  UNUSED(a);

  void *address = shimmed->palloc(shimmed, order);
  if (address == NULL) {
    alloc_printf("palloc(order=%zu) => NULL\n", order);
    UK_CRASH("Allocating went wrong");
  }

#ifdef CONFIG_LIBWILDE_DISABLE_INJECTION

  /* version without wilde */
  alloc_printf("palloc(order=%zu) => %p []\n", order, address);
  CLEAR(address, __PAGE_SIZE << order);
  return address;

#else

  /* version with wilde */
  alloc_lock();
  void *alias_addr = wilde_map_new(address, __PAGE_SIZE << order, __PAGE_SIZE << order);
  alloc_unlock();

  alloc_printf("palloc(order=%zu) => %p [real=%p]\n", order, alias_addr, address);
  CLEAR(alias_addr, __PAGE_SIZE << order);
  return alias_addr;
#endif
}

void shim_pfree(struct uk_alloc *a, void *ptr, size_t order)
{
  UNUSED(a);

#ifdef CONFIG_LIBWILDE_DISABLE_INJECTION

  /* version without wilde */
  CLEAR(ptr, __PAGE_SIZE << order);
  shimmed->pfree(shimmed, ptr, order);
  alloc_printf("pfree(ptr=%p, order=%zu) => 0\n", ptr, order);

#else

  // Clear first so we'll hit page boundries if we do it wrong
  CLEAR(ptr, __PAGE_SIZE << order);

  /* version with wilde */
  alloc_lock();
  void *real_addr = wilde_map_rm(ptr, NULL);
  alloc_unlock();

  shimmed->pfree(shimmed, real_addr, order);
  alloc_printf("pfree(ptr=%p, order=%zu) => 0 [real=%p]\n", ptr, order, real_addr);
#endif
}
#endif
// }}}

// shim_addmem {{{
int shim_addmem(struct uk_alloc *a, void *base, size_t size)
{
  UNUSED(a);

  if (!shimmed) {
    dprintf("No backing allocator found, calling uk_alloc_buddy_init(base=%p, "
            "size=%zu)\n",
            base, size);
    shimmed = uk_allocbbuddy_init(base, size);

    dprintf("Buddy allocator initialised, reinserting ourselves\n");
    uk_alloc_set_default(&shim);

    /* initialise wilde map interface, requires memory */
    wilde_map_init();

    return 0;
  }

  int res = shimmed->addmem(shimmed, base, size);
  alloc_printf("addmem(base=%p, size=%zu) => %d\n", base, size, res);

  return res;
}
// }}}

// shim_stats {{{
#if CONFIG_LIBUKALLOC_IFSTATS
ssize_t shim_availmem(struct uk_alloc *a)
{
  UNUSED(a);

  ssize_t s = shimmed->availmem(shimmed);
  alloc_printf("availmem() => %zu\n", s);
  return s;
}
#endif
// }}}

// shim_init {{{
void *shim_init(void)
{
  uk_pr_debug("inserting shim @%p\n", &shim);
  shimmed = uk_alloc_get_default();

  if (!shimmed)
    uk_pr_debug("Couldn't get previous allocator, going to init allocbuddy\n");
  else
    wilde_map_init();

  dprintf("Found a proper allocator to shim at %p Gonna fill it with %p\n", shimmed, &shim);
  memset(&shim, 0x00, sizeof(shim));

/* optional ones first so we can keep adding commas behind */
#ifdef CONFIG_LIBUKALLOC_IFPAGES
  shim.palloc = shim_palloc;
  shim.pfree = shim_pfree;
#endif

#ifdef CONFIG_LIBUKALLOC_IFSTATS
  shim.availmem = shim_availmem;
#endif

  shim.malloc = shim_malloc;
  shim.calloc = shim_calloc;
  shim.realloc = shim_realloc;
  shim.posix_memalign = shim_posix_memalign;
  shim.memalign = shim_memalign;
  shim.free = shim_free;
  shim.addmem = shim_addmem;

  uk_alloc_set_default(&shim);
  if (uk_alloc_get_default() != &shim) {
    uk_pr_err("Couldn't set shim as default\n");
    return NULL;
  }

  return &shim;
}
// }}}
