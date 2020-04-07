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