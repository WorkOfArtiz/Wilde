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
#define COLOR COLOR_RED

#include <uk/alloc.h>
#include <uk/ctors.h>

#include <stdio.h>
#include <stdlib.h>
#include "wilde_internal.h"
#include "pagetables.h"
#include "alias.h"
#include "vma.h"
#include "shimming.h"
#include "util.h"

#define KB ((1UL) << 10)
#define MB ((1UL) << 20)
#define GB ((1UL) << 30)
#define TB ((1UL) << 40)

#define VMAP_START (2 * TB)
#define VMAP_SIZE (2 * GB)

/* define lists */
UK_LIST_HEAD(vmem_free);
UK_LIST_HEAD(vmem_gc);

static void print_sz(size_t size)
{
  char *ext = "bytes";

  if (size > 1024) {
    size /= 1024;
    ext = "Kb";
  }

  if (size > 1024) {
    size /= 1024;
    ext = "Mb";
  }

  if (size > 1024) {
    size /= 1024;
    ext = "Gb";
  }

  uk_pr_crit("%zu %s", size, ext);
}

void wilde_map_init(void)
{
  dprintf("Initialising the vmem structs\n");

  struct vma *initial = vma_alloc();
  *initial = (struct vma) {
    .addr = VMAP_START,
    .size = VMAP_SIZE,
    .list = UK_LIST_HEAD_INIT(initial->list)
  };

  uk_list_add_tail(&initial->list, &vmem_free);

  dprintf("Let's see if it was added:\n");
  struct vma *iter;
  uk_list_for_each_entry(iter, &vmem_free, list) {
    dprintf(" -> vma {.addr=%p, .size=%zu}\n", (void *) iter->addr, iter->size);
  }
}

void *wilde_map_new(void *real_addr, size_t size, size_t alignment)
{
  /* calculate start and end of page range in which the original allocation
   * falls */
  uintptr_t page_start = ROUNDDOWN(((uintptr_t)real_addr), __PAGE_SIZE);
  uintptr_t page_end = ROUNDUP((uintptr_t)(real_addr + size), __PAGE_SIZE);

  /* calculate internal offset and required map size */
  size_t offset = ((uintptr_t)real_addr) - page_start;
  size_t map_size = page_end - page_start;


  struct vma *iter;
  
  /* Assert we have any freelist */
  UK_ASSERT(!uk_list_empty(&vmem_free));

  dprintf("Going to loop over all vmem_free entries:\n");
  uk_list_for_each_entry(iter, &vmem_free, list) {
    dprintf(" -> vma {.addr=%p, .size=%zu}\n", (void *) iter->addr, iter->size);

    uintptr_t aligned = ROUNDUP(iter->addr, alignment);
    ssize_t   remaining = iter->size - (aligned - iter->addr);

    /* can create an aligned allocation */
    if (remaining >= (ssize_t) map_size) {

      /* Cut off bit before */
      if (aligned != iter->addr)
        iter = vma_split(iter, aligned);

      /* Cut off bit after */
      if (remaining > (ssize_t) map_size)
        vma_split(iter, aligned + map_size);

      /* remove vma from vmem_free list */
      uk_list_del_init(&iter->list);

      /* register the alias in our quick lookup */
      alias_register((uintptr_t) real_addr, iter->addr + offset, size);
      
      /* remap the memory range */
      remap_range((void *)page_start, (void *) iter->addr, map_size);

      /* free the vma struct pointer */
      vma_free(iter);

      return (void *) aligned;
    }
  }

  uk_pr_crit("couldn't alloc chunk of ");
  print_sz(map_size);
  uk_pr_crit("aligned to a size of %zu\n", alignment);
  UK_CRASH("My life is over");

// #ifndef CONFIG_LIBWILDE_SHAUN
//   /* we need to map a number of pages */
//   UK_ASSERT(map_size < available.size);
// #else
//   /* in case of shaun, we need an additional available page */
//   UK_ASSERT(map_size < available.size + __PAGE_SIZE);
// #endif

  return NULL;
}

void *wilde_map_rm(void *map_addr)
{
  dprintf("Removing allocation at %p\n", map_addr);
  const struct alias *result = alias_search((uintptr_t)map_addr);
  if (result == NULL)
    return NULL;

  dprintf("Found an alias mapping at {.alias=%p, .origin=%p, .size=%ld}\n",
          (void *)result->alias, (void *)result->origin, result->size);

  void *real_addr = (void *)result->origin;

  /* calculate start and end of page range in which the original allocation
   * falls */
  uintptr_t page_start = ROUNDDOWN(result->alias, __PAGE_SIZE);
  uintptr_t page_end = ROUNDUP((result->alias + result->size), __PAGE_SIZE);

  /* calculate internal offset and required map size */
  size_t map_size = page_end - page_start;

  unmap_range((void *)page_start, map_size);
  alias_unregister((uintptr_t)map_addr);

  return real_addr;
}

void *wilde_map_get(void *map_addr)
{
  const struct alias *a = alias_search((uintptr_t)map_addr);
  UK_ASSERT(a);
  return (void *)a->origin;
}

static void wilde_init(void)
{
  uk_pr_info("Initialising lib wilde\n");

  /* set up VMA */
  uk_pr_crit("vspace size:");
  print_sz(VMAP_SIZE);
  uk_pr_crit("\n");
  shim_init();
}

#ifndef CONFIG_LIBWILDE_DISABLE_WILDE
UK_CTOR_FUNC(1, wilde_init);
#endif