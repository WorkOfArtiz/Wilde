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
#ifndef __WILDE_SHIMMING_H_
#define __WILDE_SHIMMING_H_

/* uk specific */
#include <uk/alloc.h>
#include <uk/arch/limits.h>

/* wilde specific */
#include <wilde.h>
#include "util.h"

extern struct uk_alloc *shimmed; /* the allocator on top of this */
extern struct uk_alloc shim;     /* the shim itself, the new allocator */

/* initialises the shim */
struct uk_alloc *shim_init(void);

void *shim_malloc(struct uk_alloc *a, size_t size);
void *shim_calloc(struct uk_alloc *a, size_t nmemb, size_t size);
int shim_posix_memalign(struct uk_alloc *a, void **memptr, size_t align,
                        size_t size);
void *shim_memalign(struct uk_alloc *a, size_t align, size_t size);
void *shim_realloc(struct uk_alloc *a, void *ptr, size_t size);
void shim_free(struct uk_alloc *a, void *ptr);

#if CONFIG_LIBUKALLOC_IFPAGES
void *shim_palloc(struct uk_alloc *a, size_t order);
void shim_pfree(struct uk_alloc *a, void *ptr, size_t order);
#endif /* CONFIG_LIBUKALLOC_IFPAGES */

int shim_addmem(struct uk_alloc *a, void *base, size_t size);

#if CONFIG_LIBUKALLOC_IFSTATS
ssize_t shim_availmem(struct uk_alloc *a);
#endif /* CONFIG_LIBUKALLOC_IFSTATS */

#endif /* __WILDE_SHIMMING_H_ */