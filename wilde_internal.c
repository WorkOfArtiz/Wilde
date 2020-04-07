/***************************************************************************
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
#include "x86.h"



#define VMAP_START ((4 * TB))
#define VMAP_SIZE  ((4 * TB))

/* define lists */
UK_LIST_HEAD(vmem_free);
UK_LIST_HEAD(vmem_gc);

/* define random generator */
struct uk_swrand wilde_rand;

static void print_sz(size_t size)
{
  char *ext = "bytes";

  if (size >= 1024) {
    size /= 1024;
    ext = "Kb";
  }

  if (size >= 1024) {
    size /= 1024;
    ext = "Mb";
  }

  if (size >= 1024) {
    size /= 1024;
    ext = "Gb";
  }

  uk_pr_crit("%zu%s", size, ext);
}

void wilde_map_init(void)
{
  dprintf("Initialising the vmem structs\n");

  struct vma *initial = vma_alloc();
  *initial = (struct vma){.addr = VMAP_START,
                          .size = VMAP_SIZE,
                          .list = UK_LIST_HEAD_INIT(initial->list)};

  uk_list_add_tail(&initial->list, &vmem_free);

  dprintf("Let's see if it was added:\n");
  struct vma *iter;
  uk_list_for_each_entry(iter, &vmem_free, list)
  {
    dprintf(" -> vma {.addr=%p, .size=%zu}\n", (void *)iter->addr, iter->size);
  }
}

void *wilde_map_new(void *real_addr, size_t size, size_t alignment)
{
  dprintf("wilde_map_new(addr=%p, size=%zu, alignment=%zu)\n", real_addr, size, alignment);
  /* calculate start and end of page range in which the original allocation
   * falls */
  uintptr_t page_start = ROUNDDOWN(((uintptr_t)real_addr), __PAGE_SIZE);
  uintptr_t page_end = ROUNDUP((uintptr_t)(real_addr + size), __PAGE_SIZE);

  /* calculate internal VMAP_START and required map size */
  size_t offset = ((uintptr_t)real_addr) - page_start;
  size_t map_size = page_end - page_start;

  #ifndef CONFIG_LIBWILDE_SHAUN
    size_t reserved_size = map_size;
  #elif CONFIG_LIBWILDE_BLACKSHEEP
    #warning "Extreme memory wastage, use at your own peril"

    /* black sheep is extreme quarantine mode, we must stop the spread! reserve massive chunks of blank space */
    size_t reserved_size = map_size * 2 + __PAGE_SIZE;
  #else
    /* in case of shaun, we need an additional available page */
    size_t reserved_size = map_size + __PAGE_SIZE;
  #endif

  struct vma *iter;

  /* Assert we have any freelist */
  UK_ASSERT(!uk_list_empty(&vmem_free));

  // dprintf("Going to loop over all vmem_free entries:\n");
  uk_list_for_each_entry(iter, &vmem_free, list)
  {
    dprintf(" -> free vma {.addr=%p, .size=%zu}\n", (void *)iter->addr, iter->size);

#ifdef CONFIG_LIBWILDE_SHAUN
    /* optimisation, filter out unusable areas */
    if (iter->size - __PAGE_SIZE == 0) {
      uk_list_del_init(&iter->list);
      vma_free(iter); // TODO garbage collect
      continue;
    }
#endif

#ifdef CONFIG_LIBWILDE_ASLR
    /* randomisation implemented by calculating the offset into a specific vma */
    uintptr_t first = ROUNDUP(iter->addr, alignment);
    uintptr_t last  = ROUNDDOWN((iter->addr + iter->size) - reserved_size, alignment);

    /* short circuit out the invalid options */
    if (last < first)
      continue;

    uintptr_t slots = (last - first) / alignment;

    /* pick a random number in [0, slots] */
    uintptr_t slot = uk_swrand_randr_r(&wilde_rand) % (slots + 1);
    uintptr_t aligned = first + slot * alignment;
    dprintf("Going to use ASLR allocating in [%#lx, %#lx, %lu] number of choices: %lu => %lx\n", iter->addr, iter->addr+iter->size, alignment, slots, aligned);
#else
    uintptr_t aligned = ROUNDUP(iter->addr, alignment);
#endif
    ssize_t remaining = iter->size - (aligned - iter->addr);



    /* can create an aligned allocation */
    if (remaining >= (ssize_t) reserved_size) {

      /* Cut off bit before */
      if (aligned != iter->addr)
        iter = vma_split(iter, aligned);

      /* Cut off bit after */
      if (remaining > (ssize_t) reserved_size)
        vma_split(iter, aligned + reserved_size);

      UK_ASSERT(aligned == iter->addr);
      UK_ASSERT(reserved_size == iter->size);

      /* remove vma from vmem_free list */
      uk_list_del_init(&iter->list);

      /* register the alias in our quick lookup */
      alias_register((uintptr_t)real_addr, iter->addr + offset, size);

      /* remap the memory range */
      remap_range((void *)page_start, (void *) aligned, map_size);

      /* free the vma struct pointer */
      vma_free(iter);

      // return real_addr;
      return (void *)(aligned + offset);
    }
  }

  uk_pr_crit("couldn't alloc virtual memory chunk of ");
  print_sz(map_size);
  uk_pr_crit(" aligned to a size of ");
  print_sz(alignment);
  uk_pr_crit("\n");
  UK_CRASH("My life is over\n");
  return NULL;
}

void *wilde_map_rm(void *map_addr, size_t *out_size)
{
  dprintf("Removing allocation at %p\n", map_addr);
  const struct alias *result = alias_search((uintptr_t)map_addr);
  if (result == NULL)
    return NULL;

  dprintf("Found an alias mapping at {.alias=%p, .origin=%p, .size=%ld}\n",
          (void *)result->alias, (void *)result->origin, result->size);

  void *real_addr = (void *)result->origin;
  if (out_size)
    *out_size = result->size;

  /* calculate start and end of page range in which the original allocation
   * falls */
  uintptr_t page_start = ROUNDDOWN(result->alias, __PAGE_SIZE);
  uintptr_t page_end = ROUNDUP((result->alias + result->size), __PAGE_SIZE);

  /* calculate internal VMAP_START and required map size */
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

#ifdef CONFIG_LIBWILDE_ASLR
  uk_pr_info("Seeding the ASLR with compile time included /dev/urandom data\n");
  uk_swrand_init_r(&wilde_rand, WILDE_SEED);
#endif

#ifdef CONFIG_LIBWILDE_NX
  uk_pr_info("Enabling the NX-bit\n");
  write_msr(EFER_REGISTER, read_msr(EFER_REGISTER) | EFER_NXE);
#endif

  /* set up alias hash table */
  alias_init();

  /* print memory usage */
  uk_pr_crit("vspace size: ");
  print_sz(VMAP_SIZE);
  uk_pr_crit("\n");

  uk_pr_info("Now intialising the shim\n");

  /* set up shimming, which in turn will init the vma interface */
  shim_init();
}

#ifndef CONFIG_LIBWILDE_DISABLE_WILDE
UK_CTOR_FUNC(1, wilde_init);
#endif