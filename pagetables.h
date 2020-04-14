#ifndef __WILDE_PGTABLES_H__
#define __WILDE_PGTABLES_H__

#include <stdint.h>
#include <stdbool.h>
#include <uk/alloc.h>
#include "util.h"

#ifndef __x86_64__
#error "only x64 supported"
#endif

#define CR3_MASK_PCD (1 << 4)
#define CR3_MASK_WT (1 << 3)


#define PT_ENTRIES 512
#define PT_MASK_ADDR 0xfffffffffffff000ULL

#define PT_P1_ENTRIES 512
#define PT_P1_VA_SHIFT ((12) + 9 * 3)
#define PT_P1_IDX(Vaddr) ((Vaddr >> PT_P1_VA_SHIFT) & ((1 << 9) - 1))
#define PT_P1_PRESENT POW2(0)
#define PT_P1_WRITE POW2(1)
#define PT_P1_EXEC POW2(2)
#define PT_P1_ACCESSED POW2(8)
#define PT_P1_UEXEC POW2(10)
#define PT_P1_MASK_ADDR 0xfffffffffffff000ULL

#define PT_P2_ENTRIES 512
#define PT_P2_VA_SHIFT ((12) + 9 * 2)
#define PT_P2_IDX(Vaddr) ((Vaddr >> PT_P2_VA_SHIFT) & ((1 << 9) - 1))
#define PT_P2_PRESENT POW2(0)
#define PT_P2_WRITE POW2(1)
#define PT_P2_EXEC POW2(2)
#define PT_P2_1GB POW2(7)
#define PT_P2_ACCESSED POW2(8)
#define PT_P2_DIRTY POW2(9)
#define PT_P2_UEXEC POW2(10)
#define PT_P2_MASK_ADDR 0xfffffffffffff000ULL

#define PT_P3_ENTRIES 512
#define PT_P3_VA_SHIFT ((12) + 9 * 1)
#define PT_P3_IDX(Vaddr) ((Vaddr >> PT_P3_VA_SHIFT) & ((1 << 9) - 1))
#define PT_P3_PRESENT POW2(0)
#define PT_P3_WRITE POW2(1)
#define PT_P3_EXEC POW2(2)
#define PT_P3_2MB POW2(7)
#define PT_P3_ACCESSED POW2(8)
#define PT_P3_DIRTY POW2(9)
#define PT_P3_MASK_ADDR 0xfffffffffffff000ULL

#define PT_P4_ENTRIES 512
#define PT_P4_VA_SHIFT ((12) + 9 * 0)
#define PT_P4_IDX(Vaddr) ((Vaddr >> PT_P4_VA_SHIFT) & ((1 << 9) - 1))
#define PT_P4_PRESENT POW2(0)
#define PT_P4_WRITE POW2(1)
#define PT_P4_EXEC POW2(2)
#define PT_P4_ACCESSED POW2(8)
#define PT_P4_DIRTY POW2(9)
#define PT_P4_NX POW2(63)
#define PT_P4_MASK_ADDR 0xfffffffffffff000ULL

#ifdef CONFIG_LIBWILDE_NX
#define PT_P4_BITS_SET ((PT_P4_PRESENT | PT_P4_WRITE | PT_P4_NX))
#else
#define PT_P4_BITS_SET ((PT_P4_PRESENT | PT_P4_WRITE))
#endif

#define ADDR_FROM_IDX(P1I, P2I, P3I, P4I)\
    (\
        ((P1I) << PT_P1_VA_SHIFT) + \
        ((P2I) << PT_P2_VA_SHIFT) + \
        ((P3I) << PT_P3_VA_SHIFT) + \
        ((P4I) << PT_P4_VA_SHIFT) \
    )


#define MASK_1GB 0x3fffffff
#define MASK_2MB 0x1fffff
#define MASK_4KB 0xfff

/* debug dump */
void print_pgtables(bool skip_first_gb);

/* range remapping and unmapping */
void remap_range(void *from, void *to, size_t size);
void unmap_range(void *addr, size_t size);

#endif // __WILDE_PGTABLES_H__