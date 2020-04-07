#define COLOR COLOR_BLUE
#include "util.h"
#include "pagetables.h"
#include "x86.h"
#include "shimming.h"
#include <stdio.h>
#include <stdbool.h>

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
  uintptr_t *page = shimmed->palloc(shimmed, 1);
  // dprintf("Allocated  page @ %p\n", page);
  UK_ASSERT(page);
  memset(page, 0, __PAGE_SIZE);

  // for (int i = 0; i < PT_P1_ENTRIES; i++)
  //  page[i] = !PT_P1_PRESENT;

  dprintf("Allocated new page at %p\n", page);
  return (uintptr_t)page;
}

static uintptr_t *pt_pte_to_pt(uintptr_t *pte)
{
  if (!pte)
    return NULL;

  return (uintptr_t *)(*pte & PT_P1_MASK_ADDR);
}

static inline p2_t *pt_get_p1_pte(uintptr_t *p1, uintptr_t vaddr, bool create)
{
  if (!p1)
    return NULL;

  p1_t *entry = &p1[PT_P1_IDX(vaddr)];
  if (*entry & PT_P1_PRESENT)
    return entry;

  if (create) {
    dprintf("Creating new page table\n");
    *entry = PT_P1_PRESENT | PT_P1_WRITE | pt_create();
    return entry;
  }

  return NULL;
}

static inline p2_t *pt_get_p2_pte(uintptr_t *p2, uintptr_t vaddr, bool create)
{
  if (!p2)
    return NULL;

  p2_t *entry = &p2[PT_P2_IDX(vaddr)];
  if (*entry & PT_P2_PRESENT)
    return entry;

  if (create) {
    dprintf("Creating new page table\n");
    *entry = PT_P2_PRESENT | PT_P2_WRITE | pt_create();
    return entry;
  }

  return NULL;
}

static inline p3_t *pt_get_p3_pte(uintptr_t *p3, uintptr_t vaddr, bool create)
{
  if (!p3)
    return NULL;

  p3_t *entry = &p3[PT_P3_IDX(vaddr)];
  if (*entry & PT_P3_PRESENT)
    return entry;

  if (create) {
    dprintf("Creating new page table\n");
    *entry = PT_P3_PRESENT | PT_P3_WRITE | pt_create();
    return entry;
  }

  return NULL;
}

static inline p4_t *pt_get_p4_pte(uintptr_t *p4, uintptr_t vaddr, bool create)
{
  if (!p4)
    return NULL;

  p4_t *entry = &p4[PT_P4_IDX(vaddr)];
  if (*entry & PT_P4_PRESENT)
    return entry;

  if (create) {
    // dprintf("Creating new page table\n");
    *entry = PT_P4_PRESENT | PT_P4_WRITE | pt_create();
    return entry;
  }

  return NULL;
}

/*
 * Page walks down the page table
 *
 * if no path exists 2 things can happen
 *  !create return NULL
 *  create -> path (p1, p2, p3, p4 tables) are allocated and populated, returns
 *    the entry for p4
static inline p4_t *page_walk(uintptr_t vaddr, bool create)
{
    p1_t *p1 = &rcr3()[PT_P1_IDX(vaddr)];
    if (!(*p1 & PT_P1_PRESENT))
        if (create)
            *p1 = PT_P1_PRESENT | PT_P1_WRITE | pt_create();
        else
            return NULL;

    p2_t *p2 = pt_pte_to_pt(*p1)[PT_P2_IDX(vaddr)];
    if (!(*p1 & PT_P1_PRESENT))
        if (create)
            *p1 = PT_P1_PRESENT | PT_P1_WRITE | pt_create();
        else
            return NULL;




}
*/

void print_binary_number(uintptr_t x)
{
  hprintf("bin {");
  for (unsigned i = 0; i < sizeof(uintptr_t) * 8; i++) {
    hprintf("%c", (x & (1 << (sizeof(x) * 8 - i - 1))) ? '1' : '0');
  }
  hprintf("}");

  // char arr[sizeof(x) * 8 + 1];

  // for (unsigned i = 0; i < sizeof(x) * 8; i++) {
  //     arr[sizeof(x) * 8 - i - 1] = (x >> i) ? '1' : '0';
  // }
  // arr[sizeof(x)*8] = '\0';

  // dprintf("%s", arr);
}

// static uintptr_t pt_get_phys(uintptr_t vaddr, bool create)
// {
//   dprintf("get_phys %p [%ld, %ld, %ld, %ld]\n", (void *)vaddr, PT_P1_IDX(vaddr),
//           PT_P2_IDX(vaddr), PT_P3_IDX(vaddr), PT_P4_IDX(vaddr));

//   p1_t *p1 = pt_get_p1_pte((p1_t *)rcr3(), vaddr, create);
//   dprintf("get_phys %p p1@%p\n", (void *)vaddr, p1);

//   p2_t *p2 = pt_get_p2_pte(pt_pte_to_pt(p1), vaddr, create);
//   dprintf("get_phys %p p2@%p\n", (void *)vaddr, p2);

//   if ((!p2 || !(*p2 | PT_P2_PRESENT)) && !create)
//     return 0;

//   // edge case 1, p2 can be a 1Gb page
//   if (p2 && (*p2 & PT_P2_1GB) && (*p2 & PT_P2_PRESENT)) {
//     dprintf("get_phys %p 1 gb, phys = %p\n", (void *)vaddr,
//             (void *)((*p2 & PT_P2_MASK_ADDR) + (vaddr & MASK_1GB)));
//     return (*p2 & PT_P2_MASK_ADDR) + (vaddr & MASK_1GB);
//   }

//   p3_t *p3 = pt_get_p3_pte(pt_pte_to_pt(p2), vaddr, create);
//   dprintf("get_phys %p p3@%p\n", (void *)vaddr, p3);

//   // edge case 2, p3 can be a 2Mb page
//   if (p3 && *p3 & PT_P3_2MB && *p3 & PT_P3_PRESENT) {
//     dprintf("get_phys %p 2 mb, phys = %p\n", (void *)vaddr,
//             (void *)((*p3 & PT_P3_MASK_ADDR) + (vaddr & MASK_2MB)));
//     return (*p3 & PT_P3_MASK_ADDR) + (vaddr & MASK_2MB);
//   }

//   p4_t *p4 = pt_get_p4_pte(pt_pte_to_pt(p3), vaddr, create);
//   dprintf("get_phys %p p4@%p\n", (void *)vaddr, p4);

//   // if it's not mapped in
//   if (!p4 || !(*p4 & PT_P4_PRESENT))
//     return 0;

//   dprintf("get_phys %p 4 4b, phys = %p\n", (void *)vaddr,
//           (void *)(*p4 & PT_P4_MASK_ADDR));
//   return *p4 & PT_P4_MASK_ADDR;
// }

static void print_p4(p4_t *p4_p, uintptr_t vaddr)
{
  for (uintptr_t p4 = 0; p4 < PT_P4_ENTRIES; p4++) {
    p4_t p4_e = p4_p[p4];
    p4_t p4_v = vaddr + (p4 << PT_P4_VA_SHIFT);
    UNUSED(p4_v);

    if (!(p4_e & PT_P4_PRESENT))
      continue;

    dprintf("  |   |   |   |- p4[%3d] 4Kb page mapping vaddr %p-%p to phys "
            "%p-%p [bits:%llx]\n",
            (int)p4, (void *)p4_v, (void *)(p4_v + __PAGE_SIZE - 1),
            (void *)pt_pte_to_pt(&p4_e),
            (void *)((char *)pt_pte_to_pt(&p4_e) + __PAGE_SIZE - 1),
            p4_e & 0b111111111ULL);
  }
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
    print_p4(p4_p, p3_v);
  }
}

static void print_p2(p2_t *p2_p, uintptr_t vaddr)
{
  for (uintptr_t p2 = 0; p2 < PT_P2_ENTRIES; p2++) {
    p2_t p2_e = p2_p[p2];
    uintptr_t p2_v = vaddr + (p2 << PT_P2_VA_SHIFT);

    if (!(p2_e & PT_P2_PRESENT))
      continue;

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

static void print_p1(p1_t *p1_p)
{
  for (uintptr_t p1 = 0; p1 < PT_P1_ENTRIES; p1++) {
    p1_t p1_e = p1_p[p1];
    uintptr_t p1_v = p1 << PT_P1_VA_SHIFT;

    if (!(p1_e & PT_P1_PRESENT))
      continue;

    p2_t *p2_p = pt_pte_to_pt(&p1_e);
    dprintf("  |- p1[%3d] entry -> %p\n", (int)p1, p2_p);
    print_p2(p2_p, p1_v);
  }
}

void print_pgtables(void)
{
  p1_t *p1_p = (p1_t *)rcr3();
  dprintf("cr3: %p\n", p1_p);
  print_p1(p1_p);
}

static uintptr_t *pt_next(uintptr_t *ptr, size_t index, uintptr_t flags)
{
  if ((ptr[index] & flags) == flags) {
    // dprintf("new page table not required, passing next pointer\n");
    return (uintptr_t *)(ptr[index] & PT_MASK_ADDR);
  }

  // dprintf("Allocating new page table\n");
  uintptr_t next = pt_create();
  // dprintf("Table allocated at %p\n", (void *) next);

  ptr[index] = flags | next;
  return (uintptr_t *)next;
}

void remap_range(void *from, void *to, size_t size)
{
  // hprintf("Remapping range %p-%p => %p-%p\n", from, from + size - 1, to,
          // to + size - 1);

  /* I'm lazy, assume from is phys */
  UK_ASSERT((uintptr_t)from < (1UL << 30));

  size_t p1i = PT_P1_IDX((uintptr_t)to);
  size_t p2i = PT_P2_IDX((uintptr_t)to);
  size_t p3i = PT_P3_IDX((uintptr_t)to);
  size_t p4i = PT_P4_IDX((uintptr_t)to);

  // dprintf("start: [%zu, %zu, %zu, %zu]\n", p1i, p2i, p3i, p4i);

  /* customised optimised page walking, auto create */
  p1_t *p1 = (p1_t *)rcr3(); /* read the cr3 register to get a base */
  // dprintf("p1: %p [p1[vaddr]=%zu]\n", p1, p1[p1i]);
  p2_t *p2 = pt_next(p1, p1i, PT_P1_PRESENT | PT_P1_WRITE);
  // dprintf("p2: %p [p2[vaddr]=%zu]\n", p2, p2[p2i]);
  p3_t *p3 = pt_next(p2, p2i, PT_P2_PRESENT | PT_P2_WRITE);
  // dprintf("p3: %p [p3[vaddr]=%zu]\n", p3, p3[p3i]);
  p4_t *p4 = pt_next(p3, p3i, PT_P3_PRESENT | PT_P3_WRITE);
  // dprintf("p4: %p [p4[vaddr]=%zu]\n", p4, p4[p4i]);

  for (size_t offset = 0; offset < size; offset += __PAGE_SIZE) {
    dprintf("mapping in %p = [%zu, %zu, %zu, %zu] <- %p\n", to + offset, p1i,
      p2i, p3i, p4i, (from + offset));

    /* assert we don't overwrite an entry */
    if (p4[p4i] & PT_P4_PRESENT) {
      dprintf("p4 entry: %p, bits: ", (void *) (p4[p4i] & PT_MASK_ADDR));
      print_binary_number(p4[p4i] & ~PT_MASK_ADDR);
      dprintf("\n");
      // print_pgtables();
      UK_CRASH("Would have overwritten an entry\n");
    }

    /* write the new entry in p4 table, pointing to previous memory */
#ifdef CONFIG_LIBWILDE_NX
    p4[p4i] = (p4_t)(from + offset) | PT_P4_PRESENT | PT_P4_WRITE | PT_P4_NX;
#else
    p4[p4i] = (p4_t)(from + offset) | PT_P4_PRESENT | PT_P4_WRITE;
#endif

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
          dprintf("Updating p2: %p\n", p2);
          p2 = pt_next(p1, p1i, PT_P1_PRESENT | PT_P1_WRITE);
          dprintf("New      p2: %p\n", p2);
        }

        dprintf("Updating p3: %p\n", p3);
        p3 = pt_next(p2, p2i, PT_P2_PRESENT | PT_P2_WRITE);
        dprintf("New      p3: %p\n", p3);

      }

      dprintf("Updating p4: %p\n", p4);
      p4 = pt_next(p3, p3i, PT_P3_PRESENT | PT_P3_WRITE);
      dprintf("New      p4: %p\n", p4);
    }
  }

  // dprintf("Done mapping\n");
  // print_pgtables();

#ifdef CONFIG_LIBWILDE_TEST
  /* test if memory mapped correctly */
  for (size_t offset = 0; offset < size; offset++)
    UK_ASSERT(((char *)from)[offset] == ((char *)to)[offset]);
#endif
}

#warning "Reimplement unmap_range with pt_next for improved performance"
void unmap_range(void *addr, size_t size)
{
  dprintf("unmapping range %p-%p\n", addr, addr + size);
  p1_t *cr3 = (p1_t *)rcr3();

  for (size_t i = 0; i < size; i += 4096) {
    uintptr_t vaddr = (uintptr_t)(addr + i);

    p1_t *p1 = pt_get_p1_pte(cr3, vaddr, false);
    p2_t *p2 = pt_get_p2_pte(pt_pte_to_pt(p1), vaddr, false);
    p3_t *p3 = pt_get_p3_pte(pt_pte_to_pt(p2), vaddr, false);
    p4_t *p4 = pt_get_p4_pte(pt_pte_to_pt(p3), vaddr, false);

    if (!p4)
      UK_CRASH("Something went horribly wrong, couldnt unmap %p", addr);

    *p4 = ~0ULL & !PT_P4_PRESENT;
  }
}
