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
#include <linux/pagemap.h>
#include <linux/vfi_bind.h>
#include <linux/vfi_ops.h>
#include <linux/vfi_smb.h>
#include <linux/sched.h>

int order(int x) {
	int ord = 0;
	while (x > 1) {
		x >>= 1;
		ord++;
	}
	return (ord);
}

void vfi_dealloc_pages(struct vfi_smb *smb)
{
	while(smb->num_pages--) {
		struct page *page = smb->pages[smb->num_pages];
		if (smb->address) {
			if (! PageReserved(page))
				SetPageDirty(page);
			page_cache_release(page);
		}
		else 
			__free_pages(smb->pages[smb->num_pages],0);
	}
	kfree(smb->pages);
}

int vfi_alloc_pages(struct vfi_smb *smb)
{
	size_t size = smb->size;
	int offset = smb->desc.offset;
	int page = 0;
	struct page **page_ary;

	if (smb->pages)
		return -EINVAL;
 
	smb->num_pages = ((size + offset) >> PAGE_SHIFT) + 
		((size + offset) & ~PAGE_MASK ? 1 : 0);

	page_ary = kzalloc(sizeof(struct page *)* smb->num_pages, GFP_KERNEL);
	if (NULL == page_ary)
		return -ENOMEM;

	if (smb->address) {
		char *stid;
		pid_t mytid;
		stid = vfi_get_option(&smb->desc,"mytid");
		if (stid) {
			struct task_struct *mycurrent;
			mytid = simple_strtoul(stid,NULL,10);
			mycurrent = find_task_by_pid(mytid);
			if (mycurrent) {
				down_read(&mycurrent->mm->mmap_sem);
				page = get_user_pages(mycurrent,mycurrent->mm,smb->address,smb->num_pages,1,0,page_ary,NULL);
				up_read(&mycurrent->mm->mmap_sem);
			}
		}
	}
	else if (vfi_get_option(&smb->desc,"discontig")) 
		for (page=0; page < smb->num_pages; page++) {
			if ( (page_ary[page] = alloc_page(GFP_KERNEL)) )
				continue;
			break;
		}
	else {
		int remainder = smb->num_pages;
#if 0
		/*
		 * When calling alloc_pages with order larger than 0, __free_pages has to be called
		 * with the same order. Implementing it by saving the order did not work so for now we revert to
		 * iterative allocation
		 */
		struct page *p;
		int ord;
		int npages;
		int i;

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
#endif
		while (remainder--) {
			if ( (page_ary[page++] = alloc_page(GFP_KERNEL)) )
				continue;
			else
				break;
		}
	}
       
	if ( page == smb->num_pages ) {
		smb->pages = page_ary;
		return 0;
	}

	while (page--)
		__free_pages(page_ary[page],0);

	smb->num_pages = 0;

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

	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p -> %d = %s\n",__FUNCTION__,rde,i,rde->name);
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
	if (rde)
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

