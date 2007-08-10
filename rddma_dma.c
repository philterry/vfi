/* 
 * 
 * Copyright 2007 MicroMemory, LLC.
 * Phil Terry <pterry@micromemory.com> 
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/rddma_dma.h>
#include <linux/rio.h>
#include <linux/rio_ids.h>
#include <linux/rio_drv.h>

void rddma_dealloc_pages( struct page **pages, int num_pages)
{
	while(num_pages--)
		__free_pages(pages[num_pages],0);
	kfree(pages);
}

int rddma_alloc_pages( size_t size, struct page **pages[], int *num_pages)
{
	int page;
	struct page **page_ary;
	if (pages)
		*pages = 0;
	else
		return -EINVAL;
 
	if (num_pages)
		*num_pages = (size >> PAGE_SHIFT) + (size & PAGE_MASK ? 1 : 0);
	else
		return -EINVAL;

	page_ary = kzalloc(sizeof(struct page *)* *num_pages, GFP_KERNEL);
	if (NULL == page_ary)
		return -ENOMEM;

	for (page=0; page < *num_pages; page++) {
		if ( (page_ary[page] = alloc_page(GFP_KERNEL)) )
			continue;
		break;
	}
       
	if ( page == *num_pages ) {
		*pages = page_ary;
		return 0;
	}

	while (page--)
		__free_pages(page_ary[page],0);

	*num_pages = 0;

	kfree(page_ary);

	return -ENOMEM;
}

static struct rddma_dma_engine *dma_engines[RDDMA_MAX_DMA_ENGINES];

int rddma_dma_register(struct rddma_dma_engine *rde)
{
	int ret = -EEXIST;
	int i;

	for (i = 0; i < RDDMA_MAX_DMA_ENGINES && dma_engines[i] ; i++)
		if (!strcmp(rde->name, dma_engines[i]->name) )
			return ret;

	if ( i == RDDMA_MAX_DMA_ENGINES)
		return -ENOMEM;

	dma_engines[i] = rde;

	return 0;
}

void rddma_dma_unregister(const char *name)
{
	int i;

	for (i = 0; i < RDDMA_MAX_DMA_ENGINES && dma_engines[i] ; i++)
		if (dma_engines[i])
			if (!strcmp(name,dma_engines[i]->name) ) {
				dma_engines[i] = NULL;
				return;
			}
}

struct rddma_dma_engine *rddma_dma_find(const char *name)
{
	int i;
	struct rddma_dma_engine *rdp;

	for (i = 0, rdp = dma_engines[0]; i < RDDMA_MAX_DMA_ENGINES && rdp ; i++, rdp++)
		if (rdp)
			if (!strcmp(name,rdp->name) ) {
				if (try_module_get(rdp->owner)) {
					rdp->ops->get(rdp);
					return rdp;
				}
			}
	return NULL;
}

struct rddma_dma_engine *rddma_dma_get(struct rddma_dma_engine *rde)
{
	if (try_module_get(rde->owner)) {
		rde->ops->get(rde);
		return rde;
	}
	return NULL;
}

void rddma_dma_put(struct rddma_dma_engine *rde)
{
	rde->ops->put(rde);
	module_put(rde->owner);
}

EXPORT_SYMBOL(rddma_dma_register);
EXPORT_SYMBOL(rddma_dma_unregister);

