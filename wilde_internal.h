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

#ifndef __WILDE_INTERNAL_H__
#define __WILDE_INTERNAL_H__

#include <uk/list.h>

extern struct uk_list_head vmem_free; /* vmem chunks ready for use */
extern struct uk_list_head vmem_gc;   /* vmem chunks ready for gc */

/*
 * wilde internals
 *   - wilde_map_init will create and initialise the vmas, requires memory
 *   - wilde_map_new will create an entirely new aligned mapping of at least size 
 *   - wilde_map_rm will wipe a mapping, likely not to be used again
 *   - wilde_get wil try to find the allocated mapping
 */
void wilde_map_init();

/*
 * @success: returns the aligned new mapping at least large enough to hold the size
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
 *          anyone accessing it
 *
 * @failure: if mapping doesn't exist, doesn't touch anything
 *
 * returns the mapping it removed (or NULL)
 */
void *wilde_map_rm(void *map_addr);

/*
 * @success: returns the address of the real address
 * @fail:    if nothing found, returns NULL
 */
void *wilde_map_get(void *map_addr);

#endif // __WILDE_INTERNAL_H__
