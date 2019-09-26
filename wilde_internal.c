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
 ****************************************************************************
 * The implemenation exists of 3 parts:
 *
 * [X] allocator injection (making a shim of the allocator)
 * [X] remapping pages with correct access rights
 * [X] a virtual memory management structure
 *     [X] on alloc, find new piece of memory
 *     [X] on free, find original allocation
 */
#include <uk/alloc.h>
#include <stdio.h>
#include <stdlib.h>
#include "wilde_internal.h"
#include "pagetables.h"
#include "alias.h"
#include "vma.h"
#include "shimming.h"

#define COLOR "33"
#include "util.h"

#define MB               ((1UL) << 10))
#define GB               ((1UL  << 20))
#define TB               ((1UL  << 30))
#define VMAP_START            (2 * TB)
#define VMAP_SIZE             (2 * GB)

/* available space, later to be extended to include GC */
struct vma available;

void *wilde_map_new(void *real_addr, size_t size)
{
  /* calculate start and end of page range in which the original allocation falls */
  uintptr_t page_start = ROUNDDOWN(((uintptr_t) real_addr), __PAGE_SIZE);
  uintptr_t page_end   = ROUNDUP((uintptr_t) (real_addr + size), __PAGE_SIZE);

  /* calculate internal offset and required map size */
  size_t    offset     = ((uintptr_t) real_addr) - page_start;
  size_t    map_size   = page_end - page_start;

#ifndef CONFIG_LIBWILDE_SHAUN
  /* we need to map a number of pages */
  UK_ASSERT(map_size < available.size);
#else
  /* in case of shaun, we need an additional available page */
  UK_ASSERT(map_size < available.size + __PAGE_SIZE);
#endif

  /* now map in the range at the alias */
  void *alias_base_addr = (void *) available.addr;
  remap_range((void *) page_start, alias_base_addr, map_size);

  /* add to administration */
  alias_register((uintptr_t) real_addr, (uintptr_t) alias_base_addr + offset, size);

  /*
   * if we're adding electric sheep, add another page to the reservation, which
   * obviously should not be mapped.
   */
#ifdef CONFIG_LIBWILDE_SHAUN
  map_size += __PAGE_SIZE;
#endif

  /* change the available space */
  available.addr += map_size;
  available.size -= map_size;

  return alias_base_addr + offset;
}

void *wilde_map_rm(void *map_addr)
{
  dprintf("Removing allocation at %p\n", map_addr);
  const struct alias *result = alias_search((uintptr_t) map_addr);
  if (result == NULL)
    return NULL;

  dprintf("Found an alias mapping at {.alias=%p, .origin=%p, .size=%ld}\n",
    (void *) result->alias, (void *) result->origin, result->size);

  void  *real_addr = (void *) result->origin;

  /* calculate start and end of page range in which the original allocation falls */
  uintptr_t page_start = ROUNDDOWN(result->alias, __PAGE_SIZE);
  uintptr_t page_end   = ROUNDUP((result->alias + result->size), __PAGE_SIZE);

  /* calculate internal offset and required map size */
  size_t    map_size   = page_end - page_start;

  unmap_range((void *) page_start, map_size);
  alias_unregister((uintptr_t) map_addr);

  return real_addr;
}

void *wilde_map_get(void *map_addr)
{
  const struct alias *a = alias_search((uintptr_t) map_addr);
  UK_ASSERT(a);
  return (void *) a->origin;
}

struct uk_alloc *wilde_init()
{
  struct uk_alloc *shim = shim_init();

  /* set up VMA */
  // available = vma_alloc();
  available.addr = VMAP_START;
  available.size = VMAP_SIZE;

#if 0
  char *page = shimmed->memalign(shimmed, __PAGE_SIZE, __PAGE_SIZE);
  lprintf("Allocated random page @ %p\n", page);

  lprintf("Filling with random data\n");
  for (unsigned i = 0; i < __PAGE_SIZE; i++)
    page[i] = i;

  char *remote_alias = (void *) (4ULL << (8 * 4));

  remap_range(page, remote_alias, __PAGE_SIZE);

  for (unsigned i = 0; i < 2; i++) {
    lprintf("%u\n", i);
    lprintf("%d, %d\n", (char) i % 10, remote_alias[i]);
    UK_ASSERT((char) i % 10 == remote_alias[i]);
  }
  remap_range((void *) 0x201000ULL, (void *) (4ULL << (8 * 4)), __PAGE_SIZE);
#endif

  return shim;
}