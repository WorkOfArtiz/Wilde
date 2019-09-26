#define COLOR "33"

#include <uk/assert.h>
#include <stdlib.h>

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "shimming.h"
#include "alias.h"
#include "util.h"



/* hash table/linked list, alias -> (size, origin) */
struct alias lookup[LOOKUP_SIZE] = {0};

static inline void alias_clear(struct alias *a) 
{
  a->alias = 0;
  a->size = 0;
  a->origin = 0;
  a->next = NULL;
}

void alias_dump(void)
{
  lprintf("alias_dump()\n");

  for (int i = 0; i < LOOKUP_SIZE; i++) {
    printf("    [%d] ", i);
    
    // if (!lookup[i].alias) {
    //   printf("Skip\n");
    //   continue;
    // }

    // struct alias *s = &lookup[i];
    // if (s->alias == 0) {
    //   printf("Empty\n");
    //   continue;
    // }
    // printf("Not Empty: ");

    // do {
    //   printf(" -> (alias=%p)", (void *) s->alias);
    //   s = s->next;
    // } while(s);

    printf("boop\n");
  }

  lprintf("Alias dump done\n");
}


void alias_register(uintptr_t addr, uintptr_t alias, size_t size) {
  lprintf("void alias_register(%#lx, %#lx, %ld)\n", addr, alias, size);
  uint16_t key = hash_address(alias) % LOOKUP_SIZE;
  if (!lookup[key].alias) {
    // lprintf("-> Registering directly in map\n");

    lookup[key] = (struct alias) {
      .size=size, 
      .alias=alias, 
      .origin=addr, 
      .next=NULL
    };

    return;
  }
  lprintf("-> Gonna have to allocate a new node\n");

  struct alias *s = &lookup[key];
  while (s->next && s->alias != alias)
    s = s->next;
  
  /* catch an edge case */
  if (s->alias == alias) {
    lprintf("you registered an alias twice, dipshit\n");
    UK_ASSERT(0); 
  }

  struct alias *a = malloc(sizeof(struct alias));
  
  a->origin = addr;
  a->alias = alias;
  a->size = size;
  a->next = NULL;

  s->next = a;
}

/* 
 * Guarantees the mapping is no longer in the allocated list.
 * 
 * returns whether a mapping has been removed
 */
bool alias_unregister(uintptr_t alias) {
  // lprintf("alias_unregister(%p)\n", (void *) alias);
  uint16_t key = hash_address(alias) % LOOKUP_SIZE;
  // lprintf("[unregister alias] key = %04X\n", key);

  /* check the first entry, edge case */
  if (lookup[key].alias == alias)
  {
    lprintf("Found in map\n");

    if (lookup[key].next) {
      struct alias *next = lookup[key].next;
      lookup[key] = *next;

      alias_clear(next);
      free(next);
    }
    else
      alias_clear(&lookup[key]);
    
    return true;
  }

  struct alias *prev = &lookup[key];
  struct alias *cur = lookup[key].next;

  while (cur) 
  {
    if (cur->alias == alias) 
      break;

    prev = cur;
    cur = cur->next;
  }

  if (cur) 
  {
    prev->next = cur->next;
    alias_clear(cur);
    free(cur);
  }
  
  return cur != NULL;
}

/* returns NULL on not found or the alias struct if found */
const struct alias *alias_search(uintptr_t alias) 
{
  uint16_t key = hash_address(alias) % LOOKUP_SIZE;
  
  struct alias *s = &lookup[key];
  while (s &&  s->alias != alias)
    s = s->next;
  // UK_ASSERT(s);

  // if (s)
    lprintf("Alias found {.alias=%p, .origin=%p, .size=%ld}\n", (void *) s->alias, (void *) s->origin, s->size);
  // else
  //   lprintf("Alias not found\n");

  return s;
}
