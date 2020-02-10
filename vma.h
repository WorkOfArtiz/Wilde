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
 */

#ifndef __WILDE_VMA_H__
#define __WILDE_VMA_H__
#include <stdint.h>
#include <uk/list.h>
#include "util.h"

struct vma {
  size_t size;
  uintptr_t addr;
  uint32_t _; /* 0xdeaddead */
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