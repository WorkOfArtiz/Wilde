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
#include "vma.h"

// struct vma *freelist = NULL;

// struct vma *vma_freelist_pop()
// {
//   struct vma *current = freelist;
//   struct vma *next = current->next;
//   if (next)
//     next->prev = NULL;

//   freelist = next;
//   return current;
// }

// void vma_freelist_push(struct vma *t)
// {
//   t->next = freelist;
//   freelist->prev = t;
//   freelist = t;
// }

// void vma_unlink(struct vma *t)
// {
//   if (t->prev)
//     t->prev->next = t->next;

//   if (t->next)
//     t->next->prev = t->prev;
// }

struct vma *vma_clear(struct vma *t)
{
  t->size = -1;
  t->addr = 0;
  t->prev = NULL;
  t->next = NULL;
  return t;
}

// struct vma *vma_alloc()
// {
//   if (freelist)
//     return vma_clear(vma_pop());

//   int vma_pages = _PAGESIZE / sizeof(struct vma);
//   struct vma *to_add = shimmed->malloc(vma_pages);

//   for (int i = 0; i < vma_pages - 1; i++)
//     vma_freelist_push(vma_clear(&to_add[i]));

//   return vma_clear(&to_add[vma_pages - 1]);
// }

// void vma_free(struct vma *v)
// {
//   vma_freelist_push(vma_clear(v));
// }