#define COLOR "34"
#include "util.h"
#include "pagetables.h"
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
 *      otherwise NULL. Will also add remainder of vaddr to phys addr so handling
 *      big tables is arbitrary
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

static inline uintptr_t pt_create() {
    uintptr_t *page = shimmed->memalign(shimmed, __PAGE_SIZE, __PAGE_SIZE);

    for (int i = 0; i < PT_P1_ENTRIES; i++)
        page[i] = !PT_P1_PRESENT;
    
    lprintf("Allocated new page at %p\n", page);
    return (uintptr_t) page;
}

static uintptr_t *pt_pte_to_pt(uintptr_t *pte)
{
    if (!pte)
        return NULL;
    
    return (uintptr_t *) (*pte & PT_P1_MASK_ADDR);
}

static inline p2_t *pt_get_p1_pte(uintptr_t *p1, uintptr_t vaddr, bool create)
{
    if (!p1)
        return NULL;

    p1_t *entry = &p1[PT_P1_IDX(vaddr)];    
    if (*entry & PT_P1_PRESENT)
        return entry;
    
    if (create) {
        lprintf("Creating new page table\n");
        *entry = PT_P1_PRESENT | pt_create();
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
        lprintf("Creating new page table\n");
        *entry = PT_P2_PRESENT | pt_create();
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
        lprintf("Creating new page table\n");
        *entry = PT_P3_PRESENT | pt_create();
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
        lprintf("Creating new page table\n");
        *entry = PT_P4_PRESENT | pt_create();
        return entry;
    }

    return NULL;
}

void print_binary_number(uintptr_t x) {
    hprintf("bin {");
    for (unsigned i = 0; i < sizeof(uintptr_t) * 8; i++) {
        hprintf("%c", (x & (1 << (sizeof(x) * 8 - i - 1))) ? '1' : '0');
    }
    hprintf("}");


    //char arr[sizeof(x) * 8 + 1];

    // for (unsigned i = 0; i < sizeof(x) * 8; i++) {
    //     arr[sizeof(x) * 8 - i - 1] = (x >> i) ? '1' : '0';
    // }
    // arr[sizeof(x)*8] = '\0';

    // hprintf("%s", arr);
}

static uintptr_t pt_get_phys(uintptr_t vaddr, bool create)
{
    lprintf("get_phys %p [%ld, %ld, %ld, %ld]\n", (void *) vaddr,
        PT_P1_IDX(vaddr), PT_P2_IDX(vaddr), PT_P3_IDX(vaddr), PT_P4_IDX(vaddr)
    );

    p1_t *p1 = pt_get_p1_pte((p1_t *) rcr3(), vaddr, create);
    lprintf("get_phys %p p1@%p\n", (void *) vaddr, p1);

    p2_t *p2 = pt_get_p2_pte(pt_pte_to_pt(p1), vaddr, create);
    lprintf("get_phys %p p2@%p\n", (void *) vaddr, p2);

    if ((!p2 || !(*p2 | PT_P2_PRESENT)) && !create)
        return 0;


    if (p2 && (*p2 & PT_P2_1GB) && (*p2 & PT_P2_PRESENT)) {
        lprintf("get_phys %p 1 gb, phys = %p\n", (void *) vaddr, (void *)((*p2 & PT_P2_MASK_ADDR) + (vaddr & MASK_1GB)));
        return (*p2 & PT_P2_MASK_ADDR) + (vaddr & MASK_1GB);
    }

    p3_t *p3 = pt_get_p3_pte(pt_pte_to_pt(p2), vaddr, create);
    lprintf("get_phys %p p3@%p\n", (void *) vaddr, p3);

    // edge case 2, p3 can be a 2Mb page
    if (p3 && *p3 & PT_P3_2MB && *p3 & PT_P3_PRESENT) {
        lprintf("get_phys %p 2 mb, phys = %p\n", (void *) vaddr, (void *) ((*p3 & PT_P3_MASK_ADDR) + (vaddr & MASK_2MB)));
        return (*p3 & PT_P3_MASK_ADDR) + (vaddr & MASK_2MB);
    }

    p4_t *p4 = pt_get_p4_pte(pt_pte_to_pt(p3), vaddr, create);
    lprintf("get_phys %p p4@%p\n", (void *) vaddr, p4);

    // if it's not mapped in
    if (!p4 || !(*p4 & PT_P4_PRESENT))
        return 0;
    
    lprintf("get_phys %p 4 4b, phys = %p\n", (void *) vaddr, (void *)(*p4 & PT_P4_MASK_ADDR));
    return *p4 & PT_P4_MASK_ADDR;
}



static void print_p4(p4_t *p4_p, uintptr_t vaddr)
{
    for (uintptr_t p4 = 0; p4 < PT_P4_ENTRIES; p4++) {
        p4_t p4_e = p4_p[p4];
        p4_t p4_v = vaddr + (p4 << PT_P4_VA_SHIFT);

        if (!(p4_e & PT_P4_PRESENT))
            continue;

        lprintf("  |   |   |   |- p4[%3d] 4Kb page mapping vaddr %p-%p to phys %p-%p\n", (int) p4,
            (void *) p4_v,
            (void *) (p4_v + __PAGE_SIZE - 1),
            (void *) pt_pte_to_pt(&p4_e),
            (void *) ((char *) pt_pte_to_pt(&p4_e) + __PAGE_SIZE - 1)
        );
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
            char *phys = (char *) pt_pte_to_pt(&p3_e);

            lprintf("  |   |   |- p3[%3d] 2Mb page mapping vaddr %p-%p to phys %p-%p\n", (int) p3,
                (void *) p3_v,
                (void *) (p3_v + (1 << PT_P3_VA_SHIFT) - 1),
                (void *) phys,
                (void *) (phys + (1 << PT_P3_VA_SHIFT) - 1)
            );

            continue;
        }

        p4_t *p4_p = pt_pte_to_pt(&p3_e);
        lprintf("  |   |   |- p3[%3d] entry -> %p\n", (int) p3, p4_p);
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
            lprintf("  |   |- p2[%3d] 1Gb page mapping %p-%p\n", (int) p2,
                (void *) p2_v,
                (void *) (p2_v + (1 << PT_P2_VA_SHIFT) - 1)
            );

            continue;
        }

        p3_t *p3_p = pt_pte_to_pt(&p2_e);
        lprintf("  |   |- p2[%3d] entry -> %p\n", (int) p2, p3_p);
        print_p3(p3_p, p2_v);
    }
}

void print_pgtables(void)
{
    p1_t *p1_p = (p1_t *) rcr3();
    lprintf("cr3: %p\n", p1_p);

    for (uintptr_t p1 = 0; p1 < PT_P1_ENTRIES; p1++) {
        p1_t p1_e = p1_p[p1];
        uintptr_t p1_v = p1 << PT_P1_VA_SHIFT;

        if (!(p1_e & PT_P1_PRESENT))
            continue;
        
        p2_t *p2_p = pt_pte_to_pt(&p1_e);
        lprintf("  |- p1[%3d] entry -> %p\n", (int) p1, p2_p);
        print_p2(p2_p, p1_v);
    }
}

void remap_range(void *from, void *to, size_t size)
{
    lprintf("remap_range %p-%p => %p-%p\n", from, from + size - 1, to, to + size - 1);
    p1_t *cr3 = (p1_t *) rcr3();

    for (size_t i = 0; i < size; i += 4096) {
        uintptr_t addr = pt_get_phys((uintptr_t) from + i, false);

        uintptr_t vaddr = (uintptr_t) (to + i);
        lprintf(" -> remapping phys page %p to %p\n", (void *) addr, (void *) vaddr);

        p1_t *p1 = pt_get_p1_pte(cr3, vaddr, true);
        p2_t *p2 = pt_get_p2_pte(pt_pte_to_pt(p1), vaddr, true);
        p3_t *p3 = pt_get_p3_pte(pt_pte_to_pt(p2), vaddr, true);
        p4_t *p4 = pt_get_p4_pte(pt_pte_to_pt(p3), vaddr, true);

        *p4 = addr | PT_P4_PRESENT | PT_P4_WRITE;
    }
}

void unmap_range(void *addr, size_t size)
{
    lprintf("unmapping range %p-%p\n", addr, addr + size);
    p1_t *cr3 = (p1_t *) rcr3();

    for (size_t i = 0; i < size; i += 4096) {
        uintptr_t vaddr = (uintptr_t) (addr + i);

        p1_t *p1 = pt_get_p1_pte(cr3, vaddr, false);
        p2_t *p2 = pt_get_p2_pte(pt_pte_to_pt(p1), vaddr, false);
        p3_t *p3 = pt_get_p3_pte(pt_pte_to_pt(p2), vaddr, false);
        p4_t *p4 = pt_get_p4_pte(pt_pte_to_pt(p3), vaddr, false);

        if (!p4)
            UK_CRASH("Something went horribly wrong, couldnt unmap %p", addr);

        *p4 = ~0ULL & !PT_P4_PRESENT;
    }
}