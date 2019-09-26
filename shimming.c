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
 * 
 ***************************************************************************
 * 
 * This file contains the shim implementation, it registers a new allocator
 * that passes all memory interface calls on to the previous allocator.
 * 
 * This is done to modify the return values, hardening the interface, the 
 * implementation of the hardening is in wilde_internal.{c,h}
 */
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>

/* uk specific */
#include <uk/alloc.h>
#include <uk/arch/limits.h>
#include <uk/print.h>
#include <uk/assert.h>

/* wilde specific */
#include <wilde.h>
#include "wilde_internal.h"
#define COLOR "36"
#include "util.h"
#include "vma.h"


struct uk_alloc *shimmed; /* the allocator on top of this */
struct uk_alloc  shim;    /* the shim itself, the new allocator */

/* Cheap-ass stubs */
void* shim_malloc(struct uk_alloc *a, size_t size)
{
    UNUSED(a);

    void *res = shimmed->malloc(shimmed, size);

#ifdef CONFIG_LIBWILDE_ALLOC_DEBUG
    lprintf("malloc(%zu) => %p\n", size, res);
#endif
    if (!res)
        return NULL;

#ifndef CONFIG_LIBWILDE_DISABLE_INJECTION
    // wrap the return value in a new mapping
    return wilde_map_new(res, size);
#else
    return res;
#endif
}

void* shim_calloc(struct uk_alloc *a, size_t nmemb, size_t size)
{
    UNUSED(a);

    void *res = shimmed->calloc(shimmed, nmemb, size);

#ifdef CONFIG_LIBWILDE_ALLOC_DEBUG    
    lprintf("calloc(nmemb=%zu, size=%zu) => %p\n", 
        nmemb, size, res);
#endif

    if (!res)
        return NULL;

#ifndef CONFIG_LIBWILDE_DISABLE_INJECTION
    return wilde_map_new(res, nmemb * size);
#else
    return res;
#endif
}

int shim_posix_memalign(struct uk_alloc *a, void **memptr, size_t align, size_t size)
{
    UNUSED(a);

    int res = shimmed->posix_memalign(shimmed, memptr, align, size);

#ifdef CONFIG_LIBWILDE_ALLOC_DEBUG
    lprintf("posix_memalign(memptr=%p, align=%zu, size=%zu) => %d\n",
        memptr, align, size, res);
#endif
    if (res != 0)
        return res;

#ifndef CONFIG_LIBWILDE_DISABLE_INJECTION
    /* intercept the return value */
    *memptr = wilde_map_new(*memptr, size);
#endif
    return res;
}

void* shim_memalign(struct uk_alloc *a, size_t align, size_t size)
{
    UNUSED(a);

    void *res = shimmed->memalign(shimmed, align, size);
#ifdef CONFIG_LIBWILDE_ALLOC_DEBUG
    lprintf("memalign(align=%zu, size=%zu) => %p\n", align, size,
        res);
#endif

#ifndef CONFIG_LIBWILDE_DISABLE_INJECTION
    return wilde_map_new(res, size);
#else
    return res;
#endif
}

void* shim_realloc(struct uk_alloc *a, void *ptr, size_t size)
{
    UNUSED(a);

#ifndef CONFIG_LIBWILDE_DISABLE_INJECTION
    /* TODO reconsider this, this makes all unchecked reallocs FAIL */
    void *real_ptr = wilde_map_rm(ptr);
#else
    void *real_ptr = ptr;
#endif

    void *res = shimmed->realloc(shimmed, real_ptr, size);

#ifdef CONFIG_LIBWILDE_ALLOC_DEBUG
    lprintf("realloc(ptr=%p, size=%zu) => %p\n", real_ptr, size, res);
#endif

#ifndef CONFIG_LIBWILDE_DISABLE_INJECTION
    return wilde_map_new(res, size);
#else
    return res;
#endif
}

void shim_free(struct uk_alloc *a, void *ptr)
{
    UNUSED(a);
#if CONFIG_LIBWILDE_ALLOC_DEBUG && !CONFIG_LIBWILDE_DISABLE_INJECTION
    lprintf("shim_free(ptr=%p)\n", ptr);
#endif

#ifndef CONFIG_LIBWILDE_DISABLE_INJECTION
    void *real_ptr = wilde_map_rm(ptr);

    if (real_ptr == NULL)
        UK_CRASH("invalid free\n");
#else
    void *real_ptr = ptr;
#endif

    shimmed->free(shimmed, real_ptr);

#ifdef CONFIG_LIBWILDE_ALLOC_DEBUG
    lprintf("free(ptr=%p)\n", real_ptr);
#endif

}

#if CONFIG_LIBUKALLOC_IFPAGES
void* shim_palloc(struct uk_alloc *a, size_t order)
{
    UNUSED(a);

    void *res = shimmed->palloc(shimmed, order);
#ifdef CONFIG_LIBWILDE_ALLOC_DEBUG
    lprintf("palloc(order=%zu) => %p\n", order, res);
#endif

#ifndef CONFIG_LIBWILDE_DISABLE_INJECTION
    return wilde_map_new(res, order * __PAGE_SIZE);
#else
    return res;
#endif
}

void shim_pfree(struct uk_alloc *a, void *ptr, size_t order)
{
    UNUSED(a);

#ifndef CONFIG_LIBWILDE_DISABLE_INJECTION
    void *real_ptr = wilde_map_rm(ptr);
    
    if (real_ptr == NULL)
        UK_CRASH("invalid free\n");
#else
    void *real_ptr = ptr;
#endif

    shimmed->pfree(shimmed, real_ptr, order);

#ifdef CONFIG_LIBWILDE_ALLOC_DEBUG
    lprintf("pfree(ptr=%p, order=%zu)\n", ptr, order);
#endif
}
#endif

int shim_addmem(struct uk_alloc *a, void *base, size_t size)
{
    UNUSED(a);

    int res = shimmed->addmem(shimmed, base, size);
#ifdef CONFIG_LIBWILDE_ALLOC_DEBUG
    lprintf("addmem(base=%p, size=%zu) => %d\n", base, size, res);
#endif
    return res;
}

#if CONFIG_LIBUKALLOC_IFSTATS
ssize_t shim_availmem(struct uk_alloc *a)
{
    UNUSED(a);

    ssize_t s = shimmed->availmem(shimmed);
#ifdef CONFIG_LIBWILDE_ALLOC_DEBUG
    lprintf("availmem() => %zu", s);
#endif
    return s;
}
#endif

struct uk_alloc *shim_init(void)
{
    shimmed = uk_alloc_get_default();

    if (!shimmed) {
        uk_pr_err("Couldn't get previous allocator\n");
        return NULL;
    }

    dprintf("Found a proper allocator to shim at %"__PRIuptr"\n",
           (uintptr_t) shimmed);


    shim = (struct uk_alloc) {

/* optional ones first so we can keep adding commas behind */
#if CONFIG_LIBUKALLOC_IFPAGES
        .palloc = shim_palloc,
        .pfree = shim_pfree,
#endif

#if CONFIG_LIBUKALLOC_IFSTATS
        .availmem = shim_availmem,
#endif

        .malloc = shim_malloc,
        .calloc = shim_calloc,
        .realloc = shim_realloc,
        .posix_memalign = shim_posix_memalign,
        .memalign = shim_memalign,
        .free = shim_free,
        .addmem = shim_addmem
    };

    uk_alloc_set_default(&shim);
    if (uk_alloc_get_default() != &shim) {
        uk_pr_err("Couldn't set shim as default\n");
        return NULL;
    }

    dprintf("Shimmied shim at %p\n", &shim);
    return &shim;
}
