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
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>

/* uk specific */
#include <uk/alloc.h>
#include <uk/allocbbuddy.h> /* the default allocator */
#include <uk/plat/console.h>
#include <uk/arch/limits.h>
#include <uk/print.h>
#include <uk/assert.h>

/* wilde specific */
#include <wilde.h>
#include "wilde_internal.h"
#define COLOR COLOR_YELLOW
#include "util.h"
#include "vma.h"

#ifdef CONFIG_LIBWILDE_DISABLE_INJECTION
#warning "Wilde is only passing through calls"
#endif

#ifdef CONFIG_LIBWILDE_LOCKING
#include <pthread.h>

static pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;
#define alloc_lock() pthread_mutex_lock(&global_mutex)
#define alloc_unlock() pthread_mutex_unlock(&global_mutex);
#else
#define alloc_lock()
#define alloc_unlock()
#endif

#ifdef CONFIG_LIBWILDE_ALLOC_DEBUG
#define alloc_printf lprintf
#else
#define alloc_printf
#endif

struct uk_alloc *shimmed; /* the allocator on top of this */
struct uk_alloc shim;     /* the shim itself, the new allocator */

void *shim_malloc(struct uk_alloc *a, size_t size)
{
  UNUSED(a);

  void *real_addr = shimmed->malloc(shimmed, size);
  if (!real_addr) {
    alloc_printf("malloc(%zu) => NULL\n", size);
    return NULL;
  }

  void *res = real_addr;
  alloc_printf("malloc(%zu) => %p now remapping\n", size, res);

#ifndef CONFIG_LIBWILDE_DISABLE_INJECTION
  alloc_lock();

  // wrap the return value in a new mapping
  res = wilde_map_new(res, size, __PAGE_SIZE);

  alloc_unlock();
  alloc_printf("malloc(%zu) => %p[real:%p]\n", size, res, real_addr);
#endif

  return res;
}

void *shim_calloc(struct uk_alloc *a, size_t nmemb, size_t size)
{
  UNUSED(a);

  void *real_addr = shimmed->calloc(shimmed, nmemb, size);

  if (!real_addr) {
    alloc_printf("calloc(nmemb=%zu, size=%zu) => NULL\n", nmemb, size);
    return NULL;
  }

  void *res = real_addr;
  alloc_printf("calloc(nmemb=%zu, size=%zu) => %p now remapping\n", nmemb, size,
               res);

#ifndef CONFIG_LIBWILDE_DISABLE_INJECTION
  alloc_lock();
  res = wilde_map_new(res, nmemb * size, __PAGE_SIZE);
  alloc_unlock();
  alloc_printf("calloc(nmemb=%zu, size=%zu) => %p[real:%p]\n", nmemb, size, res,
               real_addr);
#endif

  return res;
}

int shim_posix_memalign(struct uk_alloc *a, void **memptr, size_t align,
                        size_t size)
{
  UNUSED(a);

  int res = shimmed->posix_memalign(shimmed, memptr, align, size);
  if (res != 0) {
    alloc_printf("posix_memalign(memptr=%p, align=%zu, size=%zu) => %d\n",
                 memptr, align, size, res);
    return res;
  }

  void *real_addr = *memptr;
  alloc_printf("posix_memalign(memptr=%p, align=%zu, size=%zu) => %d "
               "[*memptr=%p] now remapping\n",
               memptr, align, size, res, *memptr);

#ifndef CONFIG_LIBWILDE_DISABLE_INJECTION
  alloc_lock();

  /* intercept the return value */
  *memptr = wilde_map_new(*memptr, size, __PAGE_SIZE);

  alloc_unlock();

  alloc_printf("posix_memalign(memptr=%p, align=%zu, size=%zu) => %d "
               "[*memptr=%p, real:%p]\n",
               memptr, align, size, res, *memptr, real_addr);
#endif

  return 0;
}

void *shim_memalign(struct uk_alloc *a, size_t align, size_t size)
{
  UNUSED(a);

  void *real_addr = shimmed->memalign(shimmed, align, size);

  if (real_addr == NULL) {
    alloc_printf("memalign(align=%zu, size=%zu) => NULL\n", align, size);
    return NULL;
  }

  void *res = real_addr;
  alloc_printf("memalign(align=%zu, size=%zu) => %p now remapping\n", align,
               size, res);

#ifndef CONFIG_LIBWILDE_DISABLE_INJECTION
  alloc_lock();

  res = wilde_map_new(res, size, ROUNDUP(size, __PAGE_SIZE));

  alloc_unlock();
  alloc_printf("memalign(align=%zu, size=%zu) => %p[real:%p]\n", align, size,
               res, real_addr);
#endif

  return res;
}

void *shim_realloc(struct uk_alloc *a, void *ptr, size_t size)
{
  UNUSED(a);
  // print_pgtables();

  void *old_real_addr = ptr;
  void *new_real_addr;
  void *res;

  // POSIX DEFINED EDGE CASE
  if (ptr == NULL) {
    alloc_printf("realloc(NULL, size=%zu) let's just call shim_malloc(%zu)\n",
                 size, size);
    return shim_malloc(a, size);
  }

#ifndef CONFIG_LIBWILDE_DISABLE_INJECTION
  alloc_printf("realloc(ptr=%p, size=%zu) now resolving\n", ptr, size);

  alloc_lock();
  old_real_addr = wilde_map_rm(ptr);
  alloc_printf(" -> ptr resolves to %p\n", old_real_addr);

  if (old_real_addr == NULL) {
    alloc_unlock();
    UK_CRASH("[wilde] caught invalid free\n");
    return NULL;
  }

  new_real_addr = shimmed->realloc(shimmed, old_real_addr, size);

  if (new_real_addr == NULL) {
    alloc_printf("realloc(ptr=%p[real:%p], size=%zu) => NULL\n", ptr,
                 old_real_addr, size);
    return NULL;
  }

  res = wilde_map_new(new_real_addr, size, __PAGE_SIZE);
  alloc_unlock();
  alloc_printf("realloc(ptr=%p[real:%p], size=%zu) => %p[real:%p]\n", ptr,
               old_real_addr, size, res, new_real_addr);

#else // CONFIG_LIBWILDE_DISABLE_INJECTION
  res = shimmed->realloc(shimmed, ptr, size);
            alloc_printf("realloc(ptr=%p, size=%zu) => %p\n", ptr, size, res);
#endif

  return res;
}

void shim_free(struct uk_alloc *a, void *ptr)
{
  UNUSED(a);

  /* POSIX defined behaviour */
  if (ptr == NULL) {
    alloc_printf("free(ptr=NULL)\n");
    return;
  }

  void *real_addr = ptr;
  alloc_printf("free(ptr=%p) now resolving\n", ptr);

#ifndef CONFIG_LIBWILDE_DISABLE_INJECTION
  alloc_lock();
  real_addr = wilde_map_rm(ptr);
  alloc_unlock();

  if (real_addr == NULL)
    UK_CRASH("invalid free\n");
#endif

  shimmed->free(shimmed, real_addr);
  alloc_printf("free(ptr=%p[real:%p])\n", ptr, real_addr);
}

#if CONFIG_LIBUKALLOC_IFPAGES
void *shim_palloc(struct uk_alloc *a, size_t order)
{
  UNUSED(a);

  void *real_addr = shimmed->palloc(shimmed, order);
  void *res = real_addr;

  alloc_printf("pre palloc(order=%zu)\n", order);

#ifndef CONFIG_LIBWILDE_DISABLE_INJECTION
  alloc_lock();

  res = wilde_map_new(res, __PAGE_SIZE << order, __PAGE_SIZE << order);

  alloc_unlock();
#endif

  alloc_printf("palloc(order=%zu) => %p[real:%p]\n", order, res, real_addr);
  return res;
}

void shim_pfree(struct uk_alloc *a, void *ptr, size_t order)
{
  UNUSED(a);
  void *real_addr = ptr;
  alloc_printf("pfree(ptr=%p, order=%zu) resolving now\n", ptr, order);

#ifndef CONFIG_LIBWILDE_DISABLE_INJECTION
  alloc_lock();
  real_addr = wilde_map_rm(ptr);
  alloc_unlock();

  if (real_addr == NULL)
    UK_CRASH("invalid free\n");
#endif

  shimmed->pfree(shimmed, real_addr, order);

  alloc_printf("pfree(ptr=%p, order=%zu) [->real:%p]\n", ptr, order, real_addr);
}
#endif

#ifdef CONFIG_LIBWILDE_TEST

void wilde_test(void) {
  char *strings[30];

  for (int i = 0; i < 30; i++) {
    strings[i] = uk_malloc(30);
    memset(strings[i], i, 30);
  }

  for (int i = 0; i < 30; i++) {
    for (int j = 0; j < 30; j++)
      UK_ASSERT(strings[i][j] == i);
    uk_free(strings[i]);
  }
}

#endif

int shim_addmem(struct uk_alloc *a, void *base, size_t size)
{
  UNUSED(a);

  if (!shimmed) {
    dprintf("No backing allocator found, calling uk_alloc_buddy_init(base=%p, size=%zu)\n", base, size);
    shimmed = uk_allocbbuddy_init(base, size);

    dprintf("Buddy allocator initialised, reinserting ourselves\n");
    uk_alloc_set_default(&shim);

    /* initialise wilde map interface, requires memory */
    wilde_map_init();

#ifdef CONFIG_LIBWILDE_TEST
    wilde_test();
#endif

    return 0;
  }

  int res = shimmed->addmem(shimmed, base, size);
  alloc_printf("addmem(base=%p, size=%zu) => %d\n", base, size, res);

  return res;
}

#if CONFIG_LIBUKALLOC_IFSTATS
ssize_t shim_availmem(struct uk_alloc *a)
{
  UNUSED(a);

  ssize_t s = shimmed->availmem(shimmed);
  alloc_printf("availmem() => %zu", s);
  return s;
}
#endif

void *shim_init(void)
{
  uk_pr_err("inserting shim @%p\n", &shim);
  shimmed = uk_alloc_get_default();
  if (!shimmed) {
    uk_pr_err("Couldn't get previous allocator, going to init allocbuddy\n");
  }
  else {
    wilde_map_init();
  }

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
