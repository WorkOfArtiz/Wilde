#ifndef __WILDE_INTERNAL_H__
#define __WILDE_INTERNAL_H__

#include <uk/list.h>

extern struct uk_list_head vmem_free; /* vmem chunks ready for use */
extern struct uk_list_head vmem_gc;   /* vmem chunks ready for gc */

/*
 * wilde internals
 *   - wilde_map_init will create and initialise the vmas, requires memory
 *   - wilde_map_new will create an entirely new aligned mapping of at least
 * size
 *   - wilde_map_rm will wipe a mapping, likely not to be used again
 *   - wilde_get wil try to find the allocated mapping
 */
void wilde_map_init();

/*
 * @success: returns the aligned new mapping at least large enough to hold the
 * size
 * @fail: crash
 *
 * will return NULL if NULL is given
 *
 * NOTE:
 *  CONFIG_LIBWILDE_SHEEP employs an extra defense, an overflow protection page
 *  after every allocation. This page is not mapped in, but reserved
 */
void *wilde_map_new(void *real_addr, size_t size, size_t align);

/*
 * @success removes a mapping for forever, never to be used again, and disallows
 *          anyone accessing it, (given out_size != NULL), will fill it with size
 *
 * @failure: if mapping doesn't exist, doesn't touch anything, leaves out_size untouched
 *
 * returns the mapping it removed (or NULL)
 */
void *wilde_map_rm(void *map_addr, size_t *out_size);

/*
 * @success: returns the address of the real address
 * @fail:    if nothing found, returns NULL
 */
void *wilde_map_get(void *map_addr);

#endif // __WILDE_INTERNAL_H__
