#include "kallocs_malloc.h"
#include "shimming.h"
#include <uk/assert.h>
#include <string.h>

static inline size_t ilog2(uintptr_t size)
{
    return 63 - __builtin_clzl(size);
}

static inline int min_page_order(size_t nbytes)
{
    /* figure out number of pages required, min 1 */
    nbytes = ROUNDUP(nbytes, __PAGE_SIZE);

    /* find order s.t. pages <= (__PAGE_SIZE << order) */
    return ilog2(nbytes - 1) - ilog2(__PAGE_SIZE - 1);
}

void *kallocs_malloc(size_t size)
{
    if (size == 0)
        UK_CRASH("malloc requested size 0");

    /* find min order, s.t. __PAGE_SIZE << order is bigger than size */
    int order = min_page_order(size);

    /* allocate required memory */
    char *memory = shimmed->palloc(shimmed, order);
    if (memory == NULL)
        UK_CRASH("Couldn't allocate enough memory");

    /* find the end of said memory */
    char *end_memory = memory + (__PAGE_SIZE << order);

    /* return size (aligned) bytes before end of allocation */
    return end_memory - ROUNDUP(size, sizeof(void *));
}

void *kallocs_calloc(size_t nmemb, size_t size)
{
    /* repurpose kallocs_malloc as this is roughly the same */
    void *buffer = kallocs_malloc(nmemb * size);

    /* don't forget to set the buffer to 0 */
    memset(buffer, 0, nmemb * size);

    return buffer;
}

void *kallocs_memalign(size_t align, size_t size)
{
    if (size == 0)
        UK_CRASH("memalign requested size 0\n");

    if (!IS_POWER_2(align) || align < sizeof(void *) || align > __PAGE_SIZE)
        UK_CRASH("memalign align %ld is wack\n", align);

    /* figure out number of pages required */
    uintptr_t pages = ROUNDUP(size, __PAGE_SIZE);

    /* find order s.t. pages <= (__PAGE_SIZE << order) */
    int order = ilog2(pages - 1) - ilog2(__PAGE_SIZE) + 1;

    /* allocate required memory */
    char *memory = shimmed->palloc(shimmed, order);
    if (memory == NULL)
        UK_CRASH("Couldn't allocate enough memory");

    /* find the end of said memory */
    char *end_memory = memory + (__PAGE_SIZE << order);

    /* return size (aligned to align) bytes before end of allocation */
    return end_memory - ROUNDUP(size, align);
}

void *kallocs_realloc(void *ptr, size_t old_size, size_t size)
{
    if (old_size == size)
        return ptr;

    void *new_ptr = kallocs_malloc(size);
    size_t copy_size = old_size < size ? old_size : size;
    memcpy(new_ptr, ptr, copy_size);
    kallocs_free(ptr, old_size);
    return new_ptr;
}

void kallocs_free(void *ptr, size_t size)
{
    size_t order = min_page_order(size);
    size_t p = __PAGE_SIZE << order;
    size_t mask = ~(p - 1);

    shimmed->pfree(shimmed, (void *)((uintptr_t) ptr & mask), order);
}