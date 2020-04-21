#ifndef __WILDE_X86_H__
#define __WILDE_X86_H__
#include "util.h"
#include <stdbool.h>


static __inline void lcr3(uintptr_t val)
{
  __asm __volatile("movq %0,%%cr3" : : "r"(val));
}

static __inline uintptr_t rcr3(bool use_cache)
{
  static uintptr_t val = 0;
  if (!val || !use_cache)
    __asm __volatile("movq %%cr3,%0" : "=r"(val));

  return val;
}

#define CR4_VME        POW2(0)
#define CR4_PVI        POW2(1)
#define CR4_TSD        POW2(2)
#define CR4_DE         POW2(3)
#define CR4_PSE        POW2(4)
#define CR4_PAE        POW2(5)
#define CR4_MCE        POW2(6)
#define CR4_PGE        POW2(7)
#define CR4_PCE        POW2(8)
#define CR4_OSFXSR     POW2(9)
#define CR4_OSXMMEXCPT POW2(10)
#define CR4_UMIP       POW2(11)
#define CR4_VMXE       POW2(13)
#define CR4_SMXE       POW2(14)
#define CR4_FSGBASE    POW2(16)
#define CR4_PCIDE      POW2(17)
#define CR4_OSXSAVE    POW2(18)
#define CR4_SMEP       POW2(20)
#define CR4_SMAP       POW2(21)
#define CR4_PKE        POW2(22)

static __inline void wcr4(uintptr_t val)
{
  __asm __volatile("movq %0,%%cr4" : : "r"(val));
}

static __inline uintptr_t rcr4(void)
{
  uintptr_t val;
  __asm __volatile("movq %%cr4,%0" : "=r"(val));
  return val;
}

/* flushes tbl only for a specific physical page */
static __inline void tlbflush_phys(uintptr_t addr)
{
  __asm __volatile("invlpg (%0)" ::"r" (addr) : "memory");
}

static __inline void tlbflush(void)
{
  uintptr_t cr3;
  __asm __volatile("movq %%cr3,%0" : "=r"(cr3));
  __asm __volatile("movq %0,%%cr3" : : "r"(cr3));
}


static __inline u64 read_msr(u32 identifier)
{
  u32 low, high;
  __asm __volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(identifier));

  u64 result = high;
  result = (result << 32) | low;
  return result;
}

static __inline void write_msr(u32 identifier, u64 value)
{
  u32 low = value & 0xffFFffFF;
  u32 high = value >> 32;

  asm volatile("wrmsr" : : "a"(low), "d"(high), "c"(identifier));
}


#define EFER_REGISTER 0xC0000080
#define EFER_NXE POW2(11)

#endif // __WILDE_X86_H__