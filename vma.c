#include "shimming.h"
#include "vma.h"

static UK_LIST_HEAD(freelist);

static void vma_batch_alloc(void)
{
  dprintf("Allocating a new batch of vma structs\n");
  struct vma *vmas = shimmed->palloc(shimmed, 1);
  size_t nr_vmas = (__PAGE_SIZE << 1) / sizeof(struct vma);

  UK_ASSERT(vmas);

  for (unsigned i = 0; i < nr_vmas; i++)
    vma_free(&vmas[i]);
}

struct vma *vma_alloc()
{
  if (uk_list_empty(&freelist))
    vma_batch_alloc();

  /* get last entry and decouple */
  struct vma *last = uk_list_last_entry(&freelist, struct vma, list);
  uk_list_del_init(&last->list);

  last->size = 0;
  last->addr = 0;

  dprintf("allocating a new vma: %p\n", last);
  return last;
}

void vma_free(struct vma *v)
{
  dprintf("vma_free(%p)\n", v);
  *v = (struct vma){.size = 0xaa55aa55, .addr = 0x55aa11aa, .list = UK_LIST_HEAD_INIT(v->list)};
  uk_list_add(&v->list, &freelist);
}

struct vma *vma_split(struct vma *v, uintptr_t addr)
{
  UK_ASSERT(addr > VMA_BEGIN(v));
  UK_ASSERT(addr < VMA_END(v));

  struct vma *res = vma_alloc();
  res->addr = addr;
  res->size = (v->size - (addr - v->addr));
  v->size -= res->size;

  uk_list_add(&res->list, &v->list);
  return res;
}

struct vma *vma_join(struct vma *v1, struct vma *v2)
{
  UK_ASSERT(VMA_END(v1) == VMA_BEGIN(v2));

  v1 += v2->size;
  uk_list_del(&v2->list);
  vma_free(v2);
  return v1;
}