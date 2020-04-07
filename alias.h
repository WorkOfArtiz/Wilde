#ifndef __WILDE_ALIAS_H__
#define __WILDE_ALIAS_H__
#include <stdint.h>
#include <stdbool.h>
#include <uk/list.h>

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

struct alias {
  struct uk_list_head list; /* list for linking aliases in hash buckets */
  size_t size;              /* size of the alias in bytes */
  uintptr_t alias;          /* alias start address */
  uintptr_t origin;         /* original addr, used for free() */
};

/* hash table */
#define LOOKUP_SIZE 0x2000
extern struct uk_list_head lookup[LOOKUP_SIZE];

static inline uintptr_t hash_address(uintptr_t x)
{
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
  x = x ^ (x >> 31);
  return x;
}

void alias_init(void);
void alias_dump(void);
void alias_register(uintptr_t addr, uintptr_t alias, size_t size);
bool alias_unregister(uintptr_t alias);
const struct alias *alias_search(uintptr_t alias);

#endif /* __WILDE_ALIAS_H__ */