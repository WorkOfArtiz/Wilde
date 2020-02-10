#define COLOR COLOR_GREEN

#include <uk/assert.h>
#include <uk/list.h>
#include <stdlib.h>

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "shimming.h"
#include "alias.h"
#include "util.h"

/* hash table/linked list, alias -> (size, origin) */
struct uk_list_head lookup[LOOKUP_SIZE] = {0};
static UK_LIST_HEAD(afreelist);

static inline void alias_clear(struct alias *a)
{
  a->alias = 0;
  a->size = 0;
  a->origin = 0;
  a->_ = 0xa11a50deadc001;
}

static void alias_batch_alloc(void)
{
  dprintf("Allocating a new batch of alias structs\n");
  struct alias *aliases = shimmed->palloc(shimmed, 1);
  size_t nr_aliases = __PAGE_SIZE / sizeof(struct alias);

  UK_ASSERT(aliases);

  for (unsigned i = 0; i < nr_aliases; i++) {
    alias_clear(&aliases[i]);
    uk_list_add(&aliases[i].list, &afreelist);
  }
}

void alias_init(void)
{
  dprintf("Initialising all lists\n");
  for (int i = 0; i < LOOKUP_SIZE; i++)
    UK_INIT_LIST_HEAD(&lookup[i]);
  dprintf("Done initialising\n");
}

void alias_dump(void)
{
  lprintf("alias_dump()\n");

  for (int i = 0; i < LOOKUP_SIZE; i++) {
    if (uk_list_empty(&lookup[i]))
      continue;

    dprintf("    [%d]", i);

    struct alias *iter;
    uk_list_for_each_entry(iter, &lookup[i], list)
      hprintf(" {alias=%p, mem=%p, .size=%ld}", (void *) iter->alias, (void *) iter->origin, iter->size);

    hprintf("\n");
  }

  lprintf("Alias dump done\n");
}

void alias_register(uintptr_t addr, uintptr_t alias, size_t size)
{
  dprintf("void alias_register(%#lx, %#lx, %ld)\n", addr, alias, size);
  uint16_t key = hash_address(alias) % LOOKUP_SIZE;
  
  struct alias *iter;
  uk_list_for_each_entry(iter, &lookup[key], list) {
    dprintf("{.alias=%p, .mem=%p, .size=%zu}\n", (void *) iter->alias, (void *) iter->origin, iter->size);
    if (iter->alias <= ROUNDUP(alias + size, __PAGE_SIZE) && alias <= ROUNDUP(iter->alias + iter->size, __PAGE_SIZE)) {
      uk_pr_crit("Critical error: 2 ranges overlap [%lux-%lux] and [%lux-%lux]\n", iter->alias, iter->alias+iter->size, alias, alias+size);
      UK_ASSERT(0);
    }
  }

  if (uk_list_empty(&afreelist))
    alias_batch_alloc();

  struct alias *a = uk_list_last_entry(&afreelist, struct alias, list);
  dprintf("alias a = %p\n", a);
  uk_list_del(&a->list);
  *a = (struct alias){.size = size, .alias = alias, .origin = addr};
  uk_list_add(&a->list, &lookup[key]);
}

/*
 * Guarantees the mapping is no longer in the allocated list.
 *
 * returns whether a mapping has been removed
 */
bool alias_unregister(uintptr_t alias)
{
  dprintf("alias_unregister(%p)\n", (void *) alias);
  // alias_dump();
  // UK_ASSERT(0);
  uint16_t key = hash_address(alias) % LOOKUP_SIZE;
  // dprintf("[unregister alias] key = %04X\n", key);

  struct alias *iter;
  uk_list_for_each_entry(iter, &lookup[key], list)
    if (iter->alias == alias) {
      uk_list_del(&iter->list);
      uk_list_add(&iter->list, &afreelist);
      return false;
    }

  return true;
}

/* returns NULL on not found or the alias struct if found */
const struct alias *alias_search(uintptr_t alias)
{
  // dprintf("alias_search(%p)\n", (void *) alias);
  // alias_dump();
  uint16_t key = hash_address(alias) % LOOKUP_SIZE;
  // dprintf("[alias search] key = %04X\n", key);

  struct alias *iter, *s = NULL;
  uk_list_for_each_entry(iter, &lookup[key], list)
    if (iter->alias == alias)
      s = iter;

  if (s)
    dprintf("Alias found {.alias=%p, .origin=%p, .size=%ld}\n",
            (void *)s->alias, (void *)s->origin, s->size);
  else
    dprintf("Alias not found\n");

  return s;
}
