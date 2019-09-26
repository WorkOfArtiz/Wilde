#ifndef __WILDE_ALIAS_H__
#define __WILDE_ALIAS_H__
#include <stddef.h>
#include <stdbool.h>

/*
 * To create a proper aliasing system, we need to be able to remember where
 * we made them. E.g.
 * 
 * malloc(20)
 *   call underlying malloc(20) impl, returns x
 *   we alias x to y
 *   we return y
 *
 * ...
 *
 * free(y)
 *   look up x based on y
 *   unmap y
 *   call free on y
 */

/* virtual addresses consist of:
 *    [ sign extend:15 ] [ address bits:37 ] [ phys page offset:11 ]
 *    
 *    Since the phys page offset isn't interesting to keep for the alias 
 *    information, we want the alias information to be 
 *    
 *    [ size info:27 ] [ address bits:37 ]
 */
struct alias 
{
  size_t       size;    /* size of the alias in bytes */
  uintptr_t    alias;   /* alias start address */
  uintptr_t    origin;  /* original addr, used for free() */
  struct alias *next;   /* next address in linked list */
};

#define LOOKUP_SIZE 0x1000
extern struct alias lookup[LOOKUP_SIZE];

static inline uintptr_t hash_address(uintptr_t x) 
{ 
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
  x = x ^ (x >> 31);
  return x;
}
//   uint16_t result = 0;
//   for (unsigned i = 0; i < sizeof(alias); i++)
//   {
//     result ^= alias;
//     alias >>= 8;
//   }

//   return result;
// }

void alias_dump(void);
void alias_register(uintptr_t addr, uintptr_t alias, size_t size);
bool alias_unregister(uintptr_t alias);
const struct alias *alias_search(uintptr_t alias);

#endif /* __WILDE_ALIAS_H__ */