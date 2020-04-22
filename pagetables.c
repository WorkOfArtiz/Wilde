#define COLOR COLOR_BLUE
#include "util.h"
#include "pagetables.h"
#include "x86.h"
#include "shimming.h"
#include <stdio.h>
#include <stdbool.h>
#include <uk/plat/console.h>

/*******************************************************************************
 * We're dealing with a 4 level page table here, buckaroos :D
 *
 * Let's get down to business, to defeat the huns... pagetables, excuse me I
 * watched Mulan recently, songs still stuck in my head. Regardless page tables
 *
 * vaddr = |   A    |    B    |     C    |    D    |    E    |
 *          0000000   0000000   0000000    0000000   1111111111
 *
 * A:7  indexes the P1
 * B:7  indexes the P2
 * C:7  indexes the P3
 * D:7  indexes the P4
 * E:12 indexes the page
 *
 * v cr3
 *  ____P1___       ____P2___       ____P3___       ____P4___       ___page___
 * |         |  |->|         |  |->|         |  |->|         |  |->|         |
 * |_________|  |  |_________|  |  |_________|  |  |_________|  |  |         |
 * |_________|--|  |_________|--|  |_________|--|  |_________|--|  |         |
 * |         |     |         |     |         |     |         |     |         |
 * |         |     |         |     |         |     |         |     |         |
 * |         |     |         |     |         |     |         |     |         |
 * |         |     |         |     |         |     |         |     |         |
 * |         |     |         |     |         |     |         |     |         |
 * |         |     |_________|     |_________|     |         |     |         |
 * |_________|     |_________|--|  |_________|--|  |_________|     |_________|
 *                              |               |   _______2mb page___________
 *                              |               |->|                         |
 *                              |                  |                         |
 *                              |                  |                         |
 *                              |                  |                         |
 *                              |                  |                         |
 *                              |                  |                         |
 *                              |                  |                         |
 *                              |                  |_________________________|
 *                              |   _______________1gb page___________________
 *                              |->|                                          |
 *                                 |                                          |
 *                                 |                                          |
 *                                 |                                          |
 *                                 |                                          |
 *                                 |                                          |
 *                                 |                                          |
 *                                 |__________________________________________|
 *
 * 3 low level interfaces are defined here:
 *   uintptr_t pt_get_phys(unintptr_t vaddr):
 *      walks the page tables, if there an address at vaddr, returns it,
 *      otherwise NULL. Will also add remainder of vaddr to phys addr so
 *handling big tables is arbitrary
 *
 *   bool pt_set_phys(uintptr_t vaddr, uintptr_t phys)
 *      sets a vaddr to phys addr, if it's occupied already, fails, will
 *      allocate pagetables. Also this only sets 1 4kb page
 *
 *   void pt_rm_phys(uintptr_t vaddr)
 *      does a page walk and removes the vaddr entry, will free pagetables when
 *      they're not in use anymore.
 *
 * Helpers:
 *  uintptr_t *pt_pte_to_pt(uinptr_t pte);          convert pte to pt next level
 *
 *  p1_t *pt_get_p1_pte(uintptr_t *p1, uintptr_t vaddr, bool create)
 *  p2_t *pt_get_p2_pte(uintptr_t *p2, uintptr_t vaddr, bool create)
 *  p3_t *pt_get_p3_pte(uintptr_t *p3, uintptr_t vaddr, bool create)
 *  p4_t *pt_get_p4_pte(uintptr_t *p4, uintptr_t vaddr, bool create)
 *
 * they get as argument their pte, find the entry associated and return a
 * pointer to it. If the present bit is set, they'll return the address
 *
 * If the present bit is not set
 *      if create:
 *          fill in the page table, set present bit, return the address.
 *      else
 *          return NULL
 ******************************************************************************/
typedef uintptr_t p1_t;
typedef uintptr_t p2_t;
typedef uintptr_t p3_t;
typedef uintptr_t p4_t;

static inline uintptr_t pt_create()
{
  // dprintf("Allocating page\n");
  uintptr_t *page = shimmed->palloc(shimmed, 0);
  if (!page)
    UK_CRASH("Couldn't allocate a page table");

  memset(page, 0, __PAGE_SIZE);

  dprintf("Allocated new page at %p\n", page);
  return (uintptr_t)page;
}

/*
 * tries to remove/free pt
 */
static inline bool pt_try_remove(uintptr_t *pgdir_entry, uintptr_t *pgtable)
{
  UK_ASSERT(pgdir_entry);
  UK_ASSERT(pgtable);

  /* verify that we can remove pgtable */
  for (int i = 0; i < PT_P1_ENTRIES; i++) {
    if (pgtable[i] & PT_P1_PRESENT)
      return false;
  }

  if ((*pgdir_entry & PT_P2_PRESENT) == 0)
    UK_CRASH("Tried to remove an item from non-present page");

  /* we can remove it */
  *pgdir_entry = 0;
  shimmed->pfree(shimmed, pgtable, 0);

  return true;
}

static uintptr_t *pt_pte_to_pt(uintptr_t *pte)
{
  if (!pte)
    return NULL;

  return (uintptr_t *)(*pte & PT_P1_MASK_ADDR);
}

void print_binary_number(uintptr_t x)
{
  hprintf("bin {");
  for (unsigned i = 0; i < sizeof(uintptr_t) * 8; i++) {
    hprintf("%c", (x & (1 << (sizeof(x) * 8 - i - 1))) ? '1' : '0');
  }
  hprintf("}");
}

static uintptr_t print_p4(p4_t *p4_p, uintptr_t vaddr)
{
  uintptr_t empty = 0;

  for (uintptr_t p4 = 0; p4 < PT_P4_ENTRIES; p4++) {
    p4_t p4_e = p4_p[p4];
    p4_t p4_v = vaddr + (p4 << PT_P4_VA_SHIFT);
    UNUSED(p4_v);

    if (!(p4_e & PT_P4_PRESENT)) {
      empty++;
      continue;
    }

    dprintf("  |   |   |   |- p4[%3d] 4Kb page mapping vaddr %p-%p to phys "
            "%p-%p [bits:%llx]\n",
            (int)p4, (void *)p4_v, (void *)(p4_v + __PAGE_SIZE - 1),
            (void *)pt_pte_to_pt(&p4_e),
            (void *)((char *)pt_pte_to_pt(&p4_e) + __PAGE_SIZE - 1),
            p4_e & 0b111111111ULL);
  }

  return empty;
}

static void print_p3(p3_t *p3_p, uintptr_t vaddr)
{
  for (uintptr_t p3 = 0; p3 < PT_P3_ENTRIES; p3++) {
    p3_t p3_e = p3_p[p3];
    uintptr_t p3_v = vaddr + (p3 << PT_P3_VA_SHIFT);

    if (!(p3_e & PT_P3_PRESENT))
      continue;

    if (p3_e & PT_P3_2MB) {
      dprintf("  |   |   |- p3[%3d] 2Mb page mapping vaddr %p-%p to phys %p-%p "
              "[bits:%llx]\n",
              (int)p3, (void *)p3_v,
              (void *)((char *)pt_pte_to_pt(&p3_e) + (1 << PT_P3_VA_SHIFT) - 1),
              (void *)pt_pte_to_pt(&p3_e),
              (void *)((char *)pt_pte_to_pt(&p3_e) + (1 << PT_P3_VA_SHIFT) - 1),
              p3_e & 0b111111111ULL);

      continue;
    }

    p4_t *p4_p = pt_pte_to_pt(&p3_e);
    dprintf("  |   |   |- p3[%3d] entry -> %p [bits: %llx]\n", (int)p3, p4_p,
            p3_e & 0b111111111ULL);
    if (print_p4(p4_p, p3_v) == PT_P4_ENTRIES)
      dprintf("  |   |   |  ?? p4 was empty ??\n");

  }
}

static void print_p2(p2_t *p2_p, uintptr_t vaddr, bool skip_first_gb)
{
  for (uintptr_t p2 = 0 ; p2 < PT_P2_ENTRIES; p2++) {
    p2_t p2_e = p2_p[p2];
    uintptr_t p2_v = vaddr + (p2 << PT_P2_VA_SHIFT);

    if (!(p2_e & PT_P2_PRESENT))
      continue;

    if (skip_first_gb && p2_v < 1*GB) {
      dprintf("  |   |- p2[%3d] 1Gb page mapping ommitted\n", (int)p2);
      continue;
    }

    if (p2_e & PT_P2_1GB) {
      dprintf("  |   |- p2[%3d] 1Gb page mapping %p-%p\n", (int)p2,
              (void *)p2_v, (void *)(p2_v + (1 << PT_P2_VA_SHIFT) - 1));

      continue;
    }

    p3_t *p3_p = pt_pte_to_pt(&p2_e);
    dprintf("  |   |- p2[%3d] entry -> %p [bits:%llx]\n", (int)p2, p3_p,
            p2_e & 0b111111111ULL);
    print_p3(p3_p, p2_v);
  }
}

static void print_p1(p1_t *p1_p, bool skip_first_gb)
{
  for (uintptr_t p1 = 0; p1 < PT_P1_ENTRIES; p1++) {
    p1_t p1_e = p1_p[p1];
    uintptr_t p1_v = p1 << PT_P1_VA_SHIFT;

    if (!(p1_e & PT_P1_PRESENT))
      continue;

    p2_t *p2_p = pt_pte_to_pt(&p1_e);
    dprintf("  |- p1[%3d] entry -> %p\n", (int)p1, p2_p);
    print_p2(p2_p, p1_v, skip_first_gb);
  }
}

void print_pgtables(bool skip_first_gb)
{
  p1_t *p1_p = (p1_t *)rcr3(true);
  dprintf("cr3: %p\n", p1_p);
  print_p1(p1_p, skip_first_gb);
}

static inline uintptr_t *pt_next(uintptr_t *ptr, size_t index, uintptr_t flags, bool create)
{
  UK_ASSERT(index < PT_P1_ENTRIES);
  UK_ASSERT((uintptr_t) ptr < (1 * GB));

  if (!ptr)
    return NULL;

  if ((ptr[index] & flags) == flags)
    return (uintptr_t *)(ptr[index] & PT_MASK_ADDR);

  UK_ASSERT((ptr[index] & PT_P1_PRESENT) == 0);

  if (!create)
    return NULL;

  uintptr_t next = pt_create();
  ptr[index] = flags | next;
  return (uintptr_t *)next;
}

void remap_range(void *from, void *to, size_t size)
{
  // hprintf("Remapping range %p-%p => %p-%p\n", from, from + size - 1, to,
          // to + size - 1);

  /* I'm lazy, assume from is phys */
  UK_ASSERT((uintptr_t)from < (1 * GB));

  size_t p1i = PT_P1_IDX((uintptr_t)to);
  size_t p2i = PT_P2_IDX((uintptr_t)to);
  size_t p3i = PT_P3_IDX((uintptr_t)to);
  size_t p4i = PT_P4_IDX((uintptr_t)to);

  UK_ASSERT(p1i < PT_P1_ENTRIES);
  UK_ASSERT(p2i < PT_P1_ENTRIES);
  UK_ASSERT(p3i < PT_P1_ENTRIES);
  UK_ASSERT(p4i < PT_P1_ENTRIES);

  /* customised optimised page walking, auto create */
  p1_t *p1 = (p1_t *)rcr3(true); /* read the cr3 register to get a base */
  p2_t *p2 = pt_next(p1, p1i, PT_P1_PRESENT | PT_P1_WRITE, true);
  p3_t *p3 = pt_next(p2, p2i, PT_P2_PRESENT | PT_P2_WRITE, true);
  p4_t *p4 = pt_next(p3, p3i, PT_P3_PRESENT | PT_P3_WRITE, true);
  // dprintf("p4: %p [p4[vaddr]=%zu]\n", p4, p4[p4i]);

  dprintf("mapping in %p = [%zu, %zu, %zu, %zu] <- %p\n",
    to, p1i, p2i, p3i, p4i, from
  );
  for (size_t offset = 0; offset < size; offset += __PAGE_SIZE) {
    // dprintf("mapping in %p = [%zu, %zu, %zu, %zu] <- %p\n",
    //   to + offset, p1i, p2i, p3i, p4i, from + offset
    // );


    UK_ASSERT(p1i < 512);
    UK_ASSERT(p2i < 512);
    UK_ASSERT(p3i < 512);
    UK_ASSERT(p4i < 512);

    if (p4[p4i] & PT_P4_PRESENT)
      UK_CRASH("WILDE CRIT: Tried to remap %p to %p but it already pointed to phys %llx\n",
        from + offset, to + offset, p4[p4i] & PT_MASK_ADDR
      );

    // UK_ASSERT(!p4[p4i]);


    /* write the new entry in p4 table, pointing to previous memory */
    p4[p4i] = (p4_t)(from + offset) | PT_P4_BITS_SET;

    /* flush tlb cache for entry */
    // tlbflush_phys(from + offset);

    /* avoid creation of 1 too many page table pages */
    if (offset + __PAGE_SIZE >= size)
      break;

    /* cascading updating */
    p4i = (p4i + 1) % PT_P4_ENTRIES;
    if (p4i == 0) {
      p3i = (p3i + 1) % PT_P3_ENTRIES;

      if (p3i == 0) {
        p2i = (p2i + 1) % PT_P2_ENTRIES;

        if (p2i == 0) {
          p1i++;
          // dprintf("Updating p2: %p\n", p2);
          p2 = pt_next(p1, p1i, PT_P1_PRESENT | PT_P1_WRITE, true);
          // dprintf("New      p2: %p\n", p2);
        }

        // dprintf("Updating p3: %p\n", p3);
        p3 = pt_next(p2, p2i, PT_P2_PRESENT | PT_P2_WRITE, true);
        // dprintf("New      p3: %p\n", p3);

      }

      // dprintf("Updating p4: %p\n", p4);
      p4 = pt_next(p3, p3i, PT_P3_PRESENT | PT_P3_WRITE, true);
      // dprintf("New      p4: %p\n", p4);
    }
  }

  dprintf("until in %p = [%zu, %zu, %zu, %zu] <- %p\n",
    to + ROUNDUP(size, __PAGE_SIZE), p1i, p2i, p3i, p4i, from + ROUNDUP(size, __PAGE_SIZE)
  );

#ifdef CONFIG_LIBWILDE_TEST
  /* test if memory mapped correctly */
  for (size_t offset = 0; offset < size; offset++)
    UK_ASSERT(((char *)from)[offset] == ((char *)to)[offset]);
#endif
}


void unmap_range(void *addr, size_t size)
{
  dprintf("unmapping range %p-%p\n", addr, addr + size);

  /*****************************************************************
   * Setting up the initial data points to work with
   ****************************************************************/

  /* calculate the indices in page table levels 1,2,3,4 - integer calculation */
  size_t p1i = PT_P1_IDX((uintptr_t) addr);
  size_t p2i = PT_P2_IDX((uintptr_t) addr);
  size_t p3i = PT_P3_IDX((uintptr_t) addr);
  size_t p4i = PT_P4_IDX((uintptr_t) addr);

  /* calculate the start of every page table - expensive page table lookup */
  p1_t *p1 = (p1_t *)rcr3(true);
  p2_t *p2 = pt_next(p1, p1i, PT_P1_PRESENT, false);
  p3_t *p3 = pt_next(p2, p2i, PT_P2_PRESENT, false);
  p4_t *p4 = pt_next(p3, p3i, PT_P3_PRESENT, false);

  /* assert we can reach p2, p3, p4 */
  UK_ASSERT(p2);
  UK_ASSERT(p3);
  UK_ASSERT(p4);

  uintptr_t paddr;

  /******************************************************************
   * Main loop over all the addresses to unmap
   *****************************************************************/
  for (size_t offset = 0; offset < size; offset += __PAGE_SIZE) {
    uintptr_t vaddr = (uintptr_t)(addr + offset); /* address to unmap */

    if ((p4[p4i] & PT_P4_PRESENT) == 0)
      UK_CRASH("Could not unmap %lx, it's not mapped in\n", vaddr);

    UK_ASSERT(p1i < 512);
    UK_ASSERT(p2i < 512);
    UK_ASSERT(p3i < 512);
    UK_ASSERT(p4i < 512);

    paddr = p4[p4i] & PT_MASK_ADDR;

    /* unmap the page */
    p4[p4i] = ~PT_P4_PRESENT;

    /*
     * Now we need to go to the next page mapping, doing so is slightly
     * complicated.
     *
     * if we haven't run out of p4 table yet it's just a simple p4i++
     * otherwise we have to
     *
     * - find the next p2, p3 and p4 tables (in case p2i, p3i or p4i leapt over the boundary)
     * - possibly free the previous p3 and p4 tables (if they ended up empty)
     *   -> and update the tables pointing to them
     *  note that we need to nest these to not perform extra checks when they're not necessary
     *
     * Ideally this would be in seperate functions... but
     * a) pointers to pointers (pointing to pointers... etc) only get more confusing
     * b) this wouldn't be as performant
     * c) the code would still be a mess, some spaghetti required, just like the rest during COVID times :/
     */
    p4i = (p4i + 1) % PT_P4_ENTRIES;

    /* if we need to go to the next p4 table */
    if (p4i == 0) {
      if (pt_try_remove(&p3[p3i], p4)) {
        dprintf("p4 mapping %p-%p no longer has any mappings, removed\n",
          (void *)(ADDR_FROM_IDX(p1i, p2i, p3i, 0)),
          (void *)(ADDR_FROM_IDX(p1i, p2i, p3i, PT_P4_ENTRIES))
        );
      }

      p3i = (p3i + 1) % PT_P3_ENTRIES;  /* update p3i index */

      /* if we need to go to the next p3 table */
      if (p3i == 0) {
        if (pt_try_remove(&p2[p2i], p3)) {
          dprintf("p3 mapping %p-%p no longer has any mappings, removed\n",
            (void *)(ADDR_FROM_IDX(p1i, p2i, 0, 0)),
            (void *)(ADDR_FROM_IDX(p1i, p2i, PT_P3_ENTRIES, 0))
          );
        }

        p2i = (p2i + 1) % PT_P2_ENTRIES;  /* update p2i index */

        /* if we need to go to the next p2 table */
        if (p2i == 0) {
          p1i++;
          p2 = pt_next(p1, p1i, PT_P1_PRESENT, false);
        }

        p3 = pt_next(p2, p2i, PT_P2_PRESENT, false);
      }

      p4 = pt_next(p3, p3i, PT_P3_PRESENT, false);
    }
    /* if this is the last iteration */
    else if (offset + __PAGE_SIZE >= size) {
      pt_try_remove(&p3[p3i], p4);
    }

    /*
     * we need to flush the physical address after unmapping it to ensure
     * no trace get's left behind in the TLB.
     *
     * This is put at the end so that if we remove page tables, those changes will be
     * included in the INVLPG instruction.
     */
    tlbflush_phys(paddr);
  }
}
