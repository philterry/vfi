/* 
 * 
 * Copyright 2008 Vmetro
 * Phil Terry <pterry@vmetro.com> 
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#define MY_DEBUG      VFI_DBG_DMA | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_DMA | VFI_DBG_LIFE    | VFI_DBG_DEBUG

#include <linux/vfi.h>
#include <linux/vfi_dma.h>
#include <linux/rio.h>
#include <linux/rio_ids.h>
#include <linux/rio_drv.h>
#include <linux/mm_types.h>
#include <linux/vfi_bind.h>
#include <linux/vfi_ops.h>

int order(int x) {
	int ord = 0;
	while (x > 1) {
		x >>= 1;
		ord++;
	}
	return (ord);
}

void vfi_dealloc_pages( struct page **pages, int num_pages)
{
	while(num_pages--)
		__free_pages(pages[num_pages],0);
	kfree(pages);
}

#define CONTIGUOUS_PAGES
int vfi_alloc_pages( size_t size, int offset, struct page **pages[], int *num_pages)
{
	int page = 0;
	struct page **page_ary;
	if (pages)
		*pages = 0;
	else
		return -EINVAL;
 
	if (num_pages)
		*num_pages = ((size + offset) >> PAGE_SHIFT) + 
			((size + offset) & ~PAGE_MASK ? 1 : 0);
	else
		return -EINVAL;

	page_ary = kzalloc(sizeof(struct page *)* *num_pages, GFP_KERNEL);
	if (NULL == page_ary)
		return -ENOMEM;

#ifndef CONTIGUOUS_PAGES
	for (page=0; page < *num_pages; page++) {
		if ( (page_ary[page] = alloc_page(GFP_KERNEL)) )
			continue;
		break;
	}
#else
	{
	struct page *p;
	int npages;
	int i;
	int remainder = *num_pages;
	int ord;

	ord = order(remainder);

	/* Allocate pages in largest possible contiguous groups */

	while(ord) {
		p = alloc_pages(GFP_KERNEL, ord);
		if (p == NULL) { 
			/* Alloc failed, try a smaller size */
			ord--;
			continue;
		}
		else {
			/* Copy pointers to page_array */
			npages = 1 << ord;
			remainder -= npages;

			for (i = 0; i < npages; i++) 
				page_ary[page++] = (p + i);

			if (remainder) 
				ord = order(remainder);
			else
				break;
		}
	}

	while (remainder--) {
		if ( (page_ary[page++] = alloc_page(GFP_KERNEL)) )
			continue;
		else
			break;
	}
	}
#endif
       
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

static struct vfi_dma_engine *dma_engines[VFI_MAX_DMA_ENGINES];

int vfi_dma_register(struct vfi_dma_engine *rde)
{
	int ret = -EEXIST;
	int i;

	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,rde);
	for (i = 0; i < VFI_MAX_DMA_ENGINES && dma_engines[i] ; i++)
		if (!strcmp(rde->name, dma_engines[i]->name) ) {
			VFI_DEBUG(MY_LIFE_DEBUG,"%s %p -> %d\n",__FUNCTION__,rde,ret);
			return ret;
		}

	if ( i == VFI_MAX_DMA_ENGINES) {
		VFI_DEBUG(MY_LIFE_DEBUG,"%s %p -> %d\n",__FUNCTION__,rde,-ENOMEM);
		return -ENOMEM;
	}

	dma_engines[i] = rde;

	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p -> %d = %p\n",__FUNCTION__,rde,i,rde);
	return 0;
}

void vfi_dma_unregister(const char *name)
{
	int i;

	VFI_DEBUG(MY_LIFE_DEBUG,"%s %s\n",__FUNCTION__,name);
	for (i = 0; i < VFI_MAX_DMA_ENGINES && dma_engines[i] ; i++)
		if (dma_engines[i])
			if (!strcmp(name,dma_engines[i]->name) ) {
				dma_engines[i] = NULL;
				return;
			}
}

struct vfi_dma_engine *vfi_dma_find(const char *name)
{
	int i;
	struct vfi_dma_engine *rdp;

	for (i = 0, rdp = dma_engines[0]; i < VFI_MAX_DMA_ENGINES && rdp ; i++, rdp++)
		if (rdp)
			if (!strcmp(name,rdp->name) ) {
				if (try_module_get(rdp->owner)) {
					rdp->ops->get(rdp);
					VFI_DEBUG(MY_LIFE_DEBUG,"%s %s -> %p\n",__FUNCTION__,name,rdp);
					return rdp;
				}
			}
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %s -> %p\n",__FUNCTION__,name,NULL);
	return NULL;
}

struct vfi_dma_engine *vfi_dma_get(struct vfi_dma_engine *rde)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,rde);
	if (try_module_get(rde->owner)) {
		rde->ops->get(rde);
		return rde;
	}
	return NULL;
}

void vfi_dma_put(struct vfi_dma_engine *rde)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,rde);
	if (rde) {
		rde->ops->put(rde);
		module_put(rde->owner);
	}
}

void vfi_dma_complete(struct vfi_bind *bind)
{
	/* DMA engines call this upcall when they get the interrupt
	 * that a chain of descriptors representing a bind has
	 * completed. */
	if (bind) {
		vfi_bind_done_pending(bind);
		if (bind->desc.dst.ops && bind->desc.dst.ops->dst_done)
			bind->desc.dst.ops->dst_done(bind);
		if (bind->desc.src.ops && bind->desc.src.ops->src_done)
			bind->desc.src.ops->src_done(bind);
		vfi_bind_done_pending(bind);
	}
}

EXPORT_SYMBOL(vfi_dma_complete);
EXPORT_SYMBOL(vfi_dma_register);
EXPORT_SYMBOL(vfi_dma_unregister);
