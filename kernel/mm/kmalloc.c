#include <common/types.h>
#include <common/macro.h>
#include <common/errno.h>
#include <common/util.h>

#include "slab.h"
#include "buddy.h"

#define _SIZE (1UL << SLAB_MAX_ORDER)

u64 size_to_page_order(u64 size)
{
	u64 order;
	u64 pg_num;
	u64 tmp;

	order = 0;
	pg_num = ROUND_UP(size, BUDDY_PAGE_SIZE) / BUDDY_PAGE_SIZE;
	tmp = pg_num;

	while (tmp > 1) {
		tmp >>= 1;
		order += 1;
	}

	if (pg_num > (1 << order))
		order += 1;

	return order;
}

extern struct global_mem global_mem;

void *kmalloc(size_t size)
{
	u64 order;
	struct page *p_page;

	if (size <= _SIZE) {
		return alloc_in_slab(size);
	}

	if (size <= BUDDY_PAGE_SIZE)
		order = 0;
	else
		order = size_to_page_order(size);

	p_page = buddy_get_pages(&global_mem, order);
	return page_to_virt(&global_mem, p_page);
}

void *kzalloc(size_t size)
{
	void *ptr;

	ptr = kmalloc(size);

	/* lack of memory */
	if (ptr == NULL)
		return NULL;

	memset(ptr, 0, size);
	return ptr;
}

/* TODO: what if ptr is not allocated */
void kfree(void *ptr)
{
	struct page *p_page;

	p_page = virt_to_page(&global_mem, ptr);
	if(p_page && p_page->slab)
		free_in_slab(ptr);
	else
		buddy_free_pages(&global_mem, p_page);
}

void *get_pages(int order)
{
	struct page *p_page;

	p_page = buddy_get_pages(&global_mem, order);
	return page_to_virt(&global_mem, p_page);
}

void free_pages(void *addr)
{
	struct page *p_page;
	p_page = virt_to_page(&global_mem, addr);
	buddy_free_pages(&global_mem, p_page);
}
