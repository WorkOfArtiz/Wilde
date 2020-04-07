#ifndef __WILDE_PGTABLES_H__
#define __WILDE_PGTABLES_H__

#include <stdint.h>
#include <uk/alloc.h>

#ifndef __x86_64__
#error "only x64 supported"
#endif

static __inline void lcr3(uintptr_t val)
{
  __asm __volatile("movq %0,%%cr3" : : "r"(val));
}

static __inline uintptr_t rcr3(void)
{
  uintptr_t val;
  __asm __volatile("movq %%cr3,%0" : "=r"(val));
  return val;
}

static __inline void tlbflush(void)
{
  uintptr_t cr3;
  __asm __volatile("movq %%cr3,%0" : "=r"(cr3));
  __asm __volatile("movq %0,%%cr3" : : "r"(cr3));
}

#define CR3_MASK_PCD (1 << 4)
#define CR3_MASK_WT (1 << 3)

#define POW2(x) (1 << x)

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
#define PT_P4_MASK_ADDR 0xfffffffffffff000ULL

#define MASK_1GB 0x3fffffff
#define MASK_2MB 0x1fffff
#define MASK_4KB 0xfff

/* debug dump */
void print_pgtables(void);

/* range remapping and unmapping */
void remap_range(void *from, void *to, size_t size);
void unmap_range(void *addr, size_t size);

#endif // __WILDE_PGTABLES_H__