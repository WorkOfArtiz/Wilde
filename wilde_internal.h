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

/*
 * wilde internals
 *   - wilde_map_new will create an entirely new mapping of at least size
 *   - wilde_map_rm will wipe a mapping, likely not to be used again
 *   - wilde_get wil try to find the allocated mapping
 */

/*
 * @success: returns the new mapping at least large enough to hold the size
 * @fail: crash
 *
 * will return NULL if NULL is given
 *
 * NOTE:
 *  CONFIG_LIBWILDE_SHEEP employs an extra defense, an overflow protection page
 *  after every allocation. This page is not mapped in, but reserved
 */
void *wilde_map_new(void *real_addr, size_t size);

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

struct uk_alloc *wilde_init(void);

#endif // __WILDE_INTERNAL_H__
