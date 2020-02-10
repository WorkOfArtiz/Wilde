// vim: set fdm=marker:
/* SPDX-License-Identifier: MIT */
/*
 * MIT License
 ****************************************************************************
 * (C) 2019 - Arthur de Fluiter - VU University Amsterdam
 ****************************************************************************
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 ***************************************************************************
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
/* uninitialized memory will be filled with this value */
#define CONFIG_MEMVAL 0xff

/* locking functionality for multithreading, coarse grained locking */
#ifdef CONFIG_LIBWILDE_LOCKING
  #error "not tested"
  #include <pthread.h>

  static pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;
  #define alloc_lock() pthread_mutex_lock(&global_mutex)
  #define alloc_unlock() pthread_mutex_unlock(&global_mutex);
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
  static size_t malloc_counter = 0;
  malloc_counter++;

  UNUSED(a);
  UK_ASSERT(a);
  UK_ASSERT(size);

  char *real_addr = shimmed->malloc(shimmed, size);
  if (!real_addr) {
    alloc_printf("malloc(%zu) => NULL [counter=%zu]\n", size, malloc_counter);
    UK_CRASH("Allocating went wrong");
    return NULL;
  }

#ifdef CONFIG_LIBWILDE_TEST
  for (size_t i = 0; i < size / sizeof(size_t); i++)
    ((size_t *) real_addr)[i] = (size_t)(malloc_counter);
#endif

  char *res = real_addr;
  dprintf("malloc(size=%zu) => %p now remapping\n", size, res);

#ifndef CONFIG_LIBWILDE_DISABLE_INJECTION
  alloc_lock();

  // wrap the return value in a new mapping
  res = wilde_map_new(res, size, __PAGE_SIZE);

  alloc_unlock();
  alloc_printf("malloc(size=%zu) => %p [real=%p, counter=%zu]\n",
    size, res, real_addr, malloc_counter);
#else
  alloc_printf("malloc(size=%zu) => %p [counter=%zu]\n", size, res, malloc_counter);
#endif

#ifdef CONFIG_LIBWILDE_TEST
  for (size_t i = 0; i < size / sizeof(size_t); i++)
    if(((size_t *) res)[i] != malloc_counter)
      UK_CRASH("Somehow it doesn't map to the same space\n");
#endif

#ifdef CONFIG_LIBWILDE_ZERO_MEMORY
  memset(res, malloc_counter, size);
#endif

  return res;
}
// }}}

// shim_calloc {{{
void *shim_calloc(struct uk_alloc *a, size_t nmemb, size_t size)
{
  UNUSED(a);

  char *real_addr = shimmed->calloc(shimmed, nmemb, size);
  if (real_addr == NULL) {
    alloc_printf("calloc(nmemb=%zu, size=%zu) => NULL\n", nmemb, size);
    UK_CRASH("Allocating went wrong");
    return NULL;
  }

#ifdef CONFIG_LIBWILDE_TEST
  for (size_t i = 0; i < nmemb * size; i++)
    if (real_addr[i] != 0)
      UK_CRASH("Somehow calloc returns uninitialised memory");
#endif

  char *res = real_addr;

#ifndef CONFIG_LIBWILDE_DISABLE_INJECTION
  dprintf("calloc(nmemb=%zu, size=%zu) => %p now remapping\n", nmemb, size,
               res);

  alloc_lock();
  res = wilde_map_new(res, nmemb * size, __PAGE_SIZE);
  alloc_unlock();
  alloc_printf("calloc(nmemb=%zu, size=%zu) => %p [real=%p]\n", nmemb, size, res,
               real_addr);
#else
  alloc_printf("calloc(nmemb=%zu, size=%zu) => %p []\n", nmemb, size, res);
#endif

#ifdef CONFIG_LIBWILDE_TEST
  for (size_t i = 0; i < nmemb * size; i++)
    if (res[i] != 0)
      UK_CRASH("Somehow it doesn't map to the same space\n");
#endif

  return res;
}
// }}}

// shim_posix_memalign {{{
int shim_posix_memalign(struct uk_alloc *a, void **memptr, size_t align,
                        size_t size)
{
  UNUSED(a);
  UK_ASSERT(size);
  UK_ASSERT(size > align);
  // UK_ASSERT(size % align == 0);

  int res = shimmed->posix_memalign(shimmed, memptr, align, size);
  if (res != 0) {
    alloc_printf("posix_memalign(memptr=%p, align=%zu, size=%zu) => %d []\n",
                 memptr, align, size, res);
    UK_CRASH("Allocating went wrong");
    return res;
  }

  void *real_addr = *memptr;
  dprintf("posix_memalign(memptr=%p, align=%zu, size=%zu) => %d "
               "[memptr=%p] now remapping\n",
               memptr, align, size, res, *memptr);

#ifndef CONFIG_LIBWILDE_DISABLE_INJECTION
  alloc_lock();

  /* intercept the return value */
  *memptr = wilde_map_new(*memptr, size, ROUNDUP(align, __PAGE_SIZE));

  alloc_unlock();

  alloc_printf("posix_memalign(memptr=%p, align=%zu, size=%zu) => %d "
               "[memptr=%p, real=%p]\n",
               memptr, align, size, res, *memptr, real_addr);
#else
  alloc_printf("posix_memalign(memptr=%p, align=%zu, size=%zu) => %d "
               "[memptr=%p]\n",
               memptr, align, size, res, real_addr);
#endif

#ifdef CONFIG_LIBWILDE_ZERO_MEMORY
  memset(*memptr, CONFIG_MEMVAL, size);
#endif

  return 0;
}
// }}}

// shim_memalign {{{
void *shim_memalign(struct uk_alloc *a, size_t align, size_t size)
{
  UK_CRASH("memalign used?");
  UNUSED(a);

  void *real_addr = shimmed->memalign(shimmed, align, size);

  if (real_addr == NULL) {
    alloc_printf("memalign(align=%zu, size=%zu) => NULL []\n", align, size);
    UK_CRASH("Allocating went wrong");
    return NULL;
  }

  void *res = real_addr;

#ifndef CONFIG_LIBWILDE_DISABLE_INJECTION
  dprintf("memalign(align=%zu, size=%zu) => %p now remapping\n", align,
               size, res);

  alloc_lock();
  res = wilde_map_new(res, size, ROUNDUP(size, __PAGE_SIZE));
  alloc_unlock();
  alloc_printf("memalign(align=%zu, size=%zu) => %p [real=%p]\n", align, size,
               res, real_addr);
#else
  alloc_printf("memalign(align=%zu, size=%zu) => %p []\n", align, size,
               res);
#endif

  UK_ASSERT(!PTRISERR(res));

  return res;
}
// }}}

// shim_realloc {{{
void *shim_realloc(struct uk_alloc *a, void *ptr, size_t size)
{
  UNUSED(a);
  // print_pgtables();

  void *old_real_addr = ptr;
  void *new_real_addr;
  void *res;

  // POSIX DEFINED EDGE CASE
  if (ptr == NULL) {
    dprintf("realloc(ptr=NULL, size=%zu) regular alloc", size);

    // realloc acts as malloc here
    char *real_addr = shimmed->malloc(shimmed, size);
    if (!real_addr) {
        alloc_printf("realloc(ptr=NULL, size=%zu) => NULL []\n", size);
        UK_CRASH("Allocating went wrong");
        return NULL;
    }

#ifdef CONFIG_LIBWILDE_DISABLE_INJECTION
    res = real_addr;
    alloc_printf("realloc(ptr=NULL, size=%zu) => %p []\n", size, real_addr);
#else
    alloc_lock();
    res = wilde_map_new(real_addr, size, __PAGE_SIZE);
    alloc_unlock();
    alloc_printf("realloc(ptr=NULL, size=%zu) => %p [real_addr=%p]\n", size, res, real_addr);
#endif

#ifdef CONFIG_LIBWILDE_ZERO_MEMORY
    memset(res, CONFIG_MEMVAL, size);
#else
  #error "whatever"
#endif

    return res;
  }

#ifndef CONFIG_LIBWILDE_DISABLE_INJECTION
  dprintf("realloc(ptr=%p, size=%zu) now resolving\n", ptr, size);

  alloc_lock();
  old_real_addr = wilde_map_rm(ptr);
  alloc_printf(" -> ptr %p resolves to %p\n", ptr, old_real_addr);

  if (old_real_addr == NULL) {
    alloc_unlock();
    UK_CRASH("[wilde] caught invalid free\n");
    return NULL;
  }

  new_real_addr = shimmed->realloc(shimmed, old_real_addr, size);

  if (new_real_addr == NULL) {
    alloc_printf("realloc(ptr=%p, size=%zu) => NULL [real=%p]\n", ptr,
                 size, old_real_addr);
    alloc_unlock();
    UK_CRASH("Allocating went wrong");
    return NULL;
  }

  res = wilde_map_new(new_real_addr, size, __PAGE_SIZE);
  alloc_unlock();
  alloc_printf("realloc(ptr=%p, size=%zu) => %p [real_ptr=%p, real_ret=%p]\n",
          ptr, size, res, old_real_addr, new_real_addr);

#else // CONFIG_LIBWILDE_DISABLE_INJECTION
  res = shimmed->realloc(shimmed, ptr, size);
  alloc_printf("realloc(ptr=%p, size=%zu) => %p\n", ptr, size, res);
#endif

  UK_ASSERT(!PTRISERR(res));

  return res;
}
// }}}

// shim_free {{{
void shim_free(struct uk_alloc *a, void *ptr)
{
  UNUSED(a);

  /* POSIX defined behaviour */
  if (ptr == NULL) {
    alloc_printf("free(ptr=NULL) => 0 []\n");
    return;
  }

  void *real_addr = ptr;

#ifndef CONFIG_LIBWILDE_DISABLE_INJECTION
  dprintf("free(ptr=%p) now resolving\n", ptr);
  alloc_lock();
  real_addr = wilde_map_rm(ptr);
  alloc_unlock();

  if (real_addr == NULL) {
    UK_CRASH("invalid free\n");
    // alloc_unlock();
    //alloc_printf("free(ptr=%p) was done in old allocator, passing through\n", ptr);
    // shimmed->free(shimmed, ptr);
    // return;
  }

  alloc_printf("free(ptr=%p) => 0 [ptr_real=%p]\n", ptr, real_addr);
#else
  alloc_printf("free(ptr=%p) => 0\n", real_addr);
#endif

  shimmed->free(shimmed, real_addr);
}
// }}}

// shim_palloc & shim_free {{{
#if CONFIG_LIBUKALLOC_IFPAGES
void *shim_palloc(struct uk_alloc *a, size_t order)
{
  UNUSED(a);

  void *real_addr = shimmed->palloc(shimmed, order);
  if (real_addr == NULL) {
    alloc_printf("palloc(order=%zu) => NULL\n", order);
    UK_CRASH("Allocating went wrong");
  }

  void *res = real_addr;

#ifndef CONFIG_LIBWILDE_DISABLE_INJECTION
  dprintf("palloc(order=%zu) => %p now resolving\n", order, real_addr);
  alloc_lock();

  res = wilde_map_new(res, __PAGE_SIZE << order, __PAGE_SIZE << order);

  alloc_unlock();
  alloc_printf("palloc(order=%zu) => %p[real=%p]\n", order, res, real_addr);
#else
  alloc_printf("palloc(order=%zu) => %p[]\n", order, res);
#endif

#ifdef CONFIG_LIBWILDE_ZERO_MEMORY
  memset(res, CONFIG_MEMVAL, 1 << order);
#endif

  return res;
}

void shim_pfree(struct uk_alloc *a, void *ptr, size_t order)
{
  UNUSED(a);
  void *real_addr = ptr;
  dprintf("pfree(ptr=%p, order=%zu) resolving now\n", ptr, order);

#ifndef CONFIG_LIBWILDE_DISABLE_INJECTION
  alloc_lock();
  real_addr = wilde_map_rm(ptr);
  alloc_unlock();

  if (real_addr == NULL)
    UK_CRASH("invalid free\n");

  alloc_printf("pfree(ptr=%p, order=%zu) => 0 [real=%p]\n", ptr, order, real_addr);
#else
  alloc_printf("pfree(ptr=%p, order=%zu) => 0\n", ptr, order);
#endif

#ifdef CONFIG_LIBWILDE_ZERO_MEMORY
  memset(real_addr, CONFIG_MEMVAL, 1 << order);
#endif

  shimmed->pfree(shimmed, real_addr, order);
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
  alloc_printf("availmem() => %zu", s);
  return s;
}
#endif
// }}}

// shim_init {{{
void *shim_init(void)
{
  // print_pgtables();
  // UK_CRASH("done mapping\n");

  uk_pr_err("inserting shim @%p\n", &shim);
  shimmed = uk_alloc_get_default();
  if (!shimmed) {
    uk_pr_err("Couldn't get previous allocator, going to init allocbuddy\n");
  } else {
    wilde_map_init();
  }

  dprintf("Found a proper allocator to shim at %p Gonna fill it with %p\n",
          shimmed, &shim);

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
