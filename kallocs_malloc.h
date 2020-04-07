#ifndef __WILDE_KALLOCS_H__
#define __WILDE_KALLOCS_H__
#include "alias.h"
#include "util.h"

/* Custom slab allocator
 *
 * reimplementations of
 * - malloc
 * - calloc
 * - posix_memalign
 * - memalign
 * - realloc (more or less)
 * - free (more or less)
 *
 * The reason for these implementations is simply the following:
 * the only allocator in unikraft I've seen so far is a buddy allocator
 * which uses a very simple wrapper to achieve malloc. The wrapper works as follows.
 *
 * malloc(20) steps:
 *
 * It palloc's a page (or more):
 *
 * 0                                                                   4kb
 * +-------------------------------------------------------------------+
 * |                                                                   |
 * +-------------------------------------------------------------------+
 *
 * It adds meta data at the start
 *
 * 0 8                                                                 4kb
 * +-+-----------------------------------------------------------------+
 * |M|                                                                 |
 * +-+-----------------------------------------------------------------+
 *
 * Then it adds the memory space
 * 0 8   28                                                            4kb
 * +-+---+-------------------------------------------------------------+
 * |M| s |                                                             |
 * +-+---+-------------------------------------------------------------+
 *
 * Then it returns the start of the buffer, and ignores the rest of the memory.
 *
 * Obviously, this is a bad model, and ideally we'd have a nice arena
 * allocator with security features like DieHard, but since this paper is
 * all about hardware accelerated vulnerability detection...
 *
 * Let's create a memory model like this:
 *
 * 0                                                           4k-20  4kb
 * +------------------------------------------------------------+------+-------
 * |                                                            |  s   | GUARDED
 * +------------------------------------------------------------+------+-------
 *                                      returns pointer to here ^
 *
 * with meta data stored Bibop style in the already existing alias vault,
 * this way (combined with the sheep defence) we
 *  1. Eliminate metadata attacks
 *  2. Create zero-tolerance buffer overflow detection
 *  3. Make proper use of the guard pages
 *  4. Incur 0 additional spacial overhead, actually reduce some in the case of
 *     posix_memalign and memalign
 *
 */

void   *kallocs_malloc(size_t size);
void   *kallocs_calloc(size_t nmemb, size_t size);
void   *kallocs_memalign(size_t align, size_t size);
void   *kallocs_realloc(void *ptr, size_t old_size, size_t size);
void    kallocs_free(void *ptr, size_t size);

#endif // __WILDE_KALLOCS_H__