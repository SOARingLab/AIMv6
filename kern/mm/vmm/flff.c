/* Copyright (C) 2016 David Gao <davidgao1001@gmail.com>
 *
 * This file is part of AIMv6.
 *
 * AIMv6 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * AIMv6 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <list.h>

/*
 * This file implements a first fit algorithm on a free list data structure
 * to work as a simple allocator.
 */

#include <mm.h>
#include <pmm.h>
#include <vmm.h>

#define ALLOC_ALIGN 16

/* This header directly leads the payload */
__attribute__ ((aligned(16)))
struct blockhdr {
	size_t size;
	bool free;
	struct list_head node;
};

#define PAYLOAD(bh)		((void *)((struct blockhdr *)(bh) + 1))
#define HEADER(payload)		((struct blockhdr *)(payload) - 1)

static struct simple_allocator __bootstrap_allocator;
static struct simple_allocator __allocator;
static struct list_head __bootstrap_head;
static struct list_head __head;
static struct pages *__backup = NULL;
//static lock_t lock;

/*
 * TODO: we probably need to explain the algorithm and data structure here
 */

static inline void *__alloc(struct list_head *head, size_t size, gfp_t flags)
{
	struct blockhdr *this, *newblock, *prev = NULL, *tmp;
	size_t allocsize, newsize;

	/* Make a good size */
	/* [Gan] TODO: remove this - reimplemented using util.h macros */
#if 0
	if ((size & (ALLOC_ALIGN - 1)) != 0) {
		size -= size & (ALLOC_ALIGN - 1);
		size += ALLOC_ALIGN;
	}
#else
	size = ALIGN_ABOVE(size, ALLOC_ALIGN);
#endif
	allocsize = size + sizeof(struct blockhdr);

	for_each_entry(this, head, node) {
		if (this->size >= allocsize)
			break;
	}

	if (&this->node == head) {
		this = (struct blockhdr *)(size_t)pa2kva(__backup->paddr);
		this->size = __backup->size;
		this->free = true;
		kfree(__backup);
		for_each_entry(tmp, head, node) {
			if (tmp >= this)
				break;
			prev = tmp;
		}
		if (prev != NULL)
			list_add_after(&this->node, &prev->node);
		else
			list_add_after(&this->node, head);
		__backup = alloc_pages(PAGE_SIZE, 0);
		if (__backup == NULL)
			while (1);	/* panic */
		for_each_entry(this, head, node) {
			if (this->size >= allocsize)
				break;
		}
	}

	newsize = this->size - allocsize;
	if (newsize >= sizeof(struct blockhdr) + ALLOC_ALIGN) {
		newblock = ((void *)this) + allocsize;
		newblock->size = newsize;
		newblock->free = true;
		this->size = allocsize;
		list_add_after(&newblock->node, &this->node);
	}
	this->free = false;
	list_del(&this->node);
	return PAYLOAD(this);
}

static inline void __free(struct list_head *head, void *obj)
{
	struct blockhdr *this, *prev = NULL, *tmp, *next = NULL;

	this = HEADER(obj);

	for_each_entry(tmp, head, node) {
		if (tmp >= this)
			break;
		prev = tmp;
	}

	this->free = true;
	if (prev != NULL)
		list_add_after(&this->node, &prev->node);
	else
		list_add_after(&this->node, head);

	/* merge downwards */
	if (
		((size_t)this & (size_t)(PAGE_SIZE - 1)) != 0 &&
		prev != NULL &&
		(void *)prev + prev->size == (void *)this
	) {
		prev->size += this->size;
		list_del(&this->node);
		this = prev;
	}

	/* merge upwards */
	if (!list_is_last(&this->node, head))
		next = list_next_entry(this, struct blockhdr, node);
	if (
		(((size_t)this + this->size) & (PAGE_SIZE - 1)) != 0 &&
		next != NULL &&
		(void *)this + this->size == (void *)next
	) {
		this->size += next->size;
		list_del(&next->node);
	}

	/* return page */
	if (this->size == PAGE_SIZE) {
		struct pages *pages = kmalloc(sizeof(struct pages), 0);
		pages->paddr = (addr_t)kva2pa((size_t)this);
		pages->size = PAGE_SIZE;
		list_del(&this->node);
		free_pages(pages);
	}
}

static size_t __size(void *obj)
{
	struct blockhdr *this;

	this = HEADER(obj);

	return this->size - sizeof(struct blockhdr);
}

static void *__bootstrap_alloc(size_t size, gfp_t flags)
{
	return __alloc(&__bootstrap_head, size, flags);
}

static void *__proper_alloc(size_t size, gfp_t flags)
{
	return __alloc(&__head, size, flags);
}

static void __bootstrap_free(void *obj)
{
	__free(&__bootstrap_head, obj);
}

static void __proper_free(void *obj)
{
	__free(&__head, obj);
}

int simple_allocator_bootstrap(void *pt, size_t size)
{
	struct blockhdr *block = pt;
	block->size = size;
	block->free = true;
	list_init(&__bootstrap_head);
	list_add_after(&block->node, &__bootstrap_head);
	__bootstrap_allocator.alloc = __bootstrap_alloc;
	__bootstrap_allocator.free = __bootstrap_free;
	__bootstrap_allocator.size = __size;
	set_simple_allocator(&__bootstrap_allocator);
	return 0;
}

int simple_allocator_init(void)
{
	struct pages *pages = alloc_pages(PAGE_SIZE, 0);
	if (pages == NULL)
		while (1);	/* panic */
	struct blockhdr *block =
		(struct blockhdr *)(size_t)pa2kva(pages->paddr);
	block->size = pages->size;
	block->free = true;
	kfree(pages);
	list_init(&__head);
	list_add_after(&block->node, &__head);

	__allocator.alloc = __proper_alloc;
	__allocator.free = __proper_free;
	__allocator.size = __size;
	__backup = alloc_pages(PAGE_SIZE, 0);
	set_simple_allocator(&__allocator);
	return 0;
}

