#ifndef __WILDE_X86_H__
#define __WILDE_X86_H__
#include "util.h"

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