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
#include "shimming.h"
#include "vma.h"

struct vma *freelist = NULL;

static void vma_batch_alloc(void) {
  dprintf("Allocating a new batch of vma structs\n");

  struct vma *vmas = shimmed->palloc(shimmed, 3);
  UK_ASSERT(vmas);

  for (int i = 0; i < (__PAGE_SIZE << 3); i++)
    vma_free(&vmas[i]);
}

struct vma *vma_alloc()
{
  if (!freelist)
    vma_batch_alloc();

  /* get last entry and decouple */
  struct vma *last = uk_list_last_entry(&freelist->list, struct vma, list);
  uk_list_del_init(&last->list);

  last->size = 0;
  last->addr = 0;
  return last;
}

void vma_free(struct vma *v)
{
  *v = (struct vma) {
    .size = 0,
    .addr = 0, 
    .list = UK_LIST_HEAD_INIT(v->list)
  };

  if (freelist)
    uk_list_add(&v->list, &freelist->list);
  else
    freelist = v;
}

struct vma *vma_split(struct vma *v, uintptr_t addr)
{
  UK_ASSERT(addr > VMA_BEGIN(v));
  UK_ASSERT(addr < VMA_END(v));

  struct vma *res = vma_alloc();
  res->addr = addr;
  res->size = (v->size - (addr - v->addr));
  v->size -= res->size;

  uk_list_add(&res->list, &v->list);
  return res;
}

struct vma *vma_join(struct vma *v1, struct vma *v2)
{
  UK_ASSERT(VMA_END(v1) == VMA_BEGIN(v2));

  v1 += v2->size;
  uk_list_del(&v2->list);
  vma_free(v2);
  return v1;
}