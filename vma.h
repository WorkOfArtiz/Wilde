#ifndef __WILDE_VMA_H__
#define __WILDE_VMA_H__
#include <stdint.h>
#include <uk/list.h>
#include "util.h"

struct vma {
  size_t size;
  uintptr_t addr;
  struct uk_list_head list;
};

#define VMA_BEGIN(Vma) (Vma)->addr
#define VMA_END(Vma) ((Vma)->addr + (Vma)->size)

/******************************************************************************
 * VMA struct memory management, move them from/to internal freelist          *
 *****************************************************************************/

/*
 * if freelist is empty, allocates a new block of vma objects, add them all to
 * the freelist except for one, which it clears and removes.
 *
 * Otherwise just pops one of freelist.
 */
struct vma *vma_alloc();

/*
 * unlinks the vma and adds it to the freelist, wiping it's state beforehand
 */
void vma_free(struct vma *free);

/******************************************************************************
 * VMA struct joining/splitting                                               *
 *****************************************************************************/

/*
 * Split a vma struct based on address.
 *
 * 2 vma structs will exist after, both in the same list as the original one.
 *
 * the original struct becomes [t.addr - addr],
 *                 the new one [addr   - t.addr + t.size]
 *
 * and the new one is returned.
 */
struct vma *vma_split(struct vma *t, uintptr_t addr);

/*
 * Joins 2 vma's into 1
 *   unlinks and frees 1 vma struct with vma_free and returns the modified other
 * one
 */
struct vma *vma_join(struct vma *v1, struct vma *v2);

#endif /* __WILDE_VMA_H__ */