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
#define MY_DEBUG      RDDMA_DBG_DMARIO | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_DMARIO | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/rddma.h>
#include <linux/rddma_dma_rio.h>
#include <linux/rddma_src.h>
#include <linux/rddma_srcs.h>
#include <asm/io.h>

struct dma_engine {
	struct rddma_dma_engine rde;
};

static inline struct dma_engine *to_dma_engine(struct rddma_dma_engine *rde)
{
	return rde ? container_of(rde, struct dma_engine, rde) : NULL;
}

static struct rddma_dma_ops dma_rio_ops;

static struct dma_engine *new_dma_engine(void)
{
	struct dma_engine *new;
	if ( (new = kzalloc(sizeof(struct dma_engine),GFP_KERNEL)) ) {
		new->rde.owner = THIS_MODULE;
		new->rde.ops = &dma_rio_ops;
	}
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,new);
	return new;
}

static void dma_rio_load(struct rddma_src *src)
{
	struct seg_desc *rio = (struct seg_desc *)&src->descriptor;
	rio->paddr = virt_to_phys(rio);
	rio->hw.saddr = src->desc.src.offset & 0xffffffff;
	rio->hw.src_attr = src->desc.src.offset >> 32;
	rio->hw.daddr = src->desc.dst.offset & 0xffffffff;
	rio->hw.dest_attr = src->desc.dst.offset >> 32;
	rio->hw.nbytes = src->desc.src.extent; 
	rio->hw.next_ext = 0;
	rio->hw.next = 1;
}

static void dma_rio_link_src(struct list_head *first, struct rddma_src *second)
{
	struct seg_desc *rio2 = (struct seg_desc *)&second->descriptor;
	struct seg_desc *riolast; 
	if (!list_empty(first)) {
		riolast = to_sdesc(first->prev);
		riolast->hw.next = rio2->paddr & 0xffffffe0;
	}
	list_add_tail(&rio2->node, first);
}

static void dma_rio_link_dst(struct list_head *first, struct rddma_dst *second)
{
	struct seg_desc *rio2;
	struct seg_desc *riolast;
	if (!list_empty(first)) {
		riolast = to_sdesc(first->prev);
		rio2 = to_sdesc(second->srcs->dma_chain.next);
		riolast->hw.next = rio2->paddr &0xffffffe0;
	}
	list_splice(&second->srcs->dma_chain, first->prev);
}

static void dma_rio_link_bind(struct rddma_bind *first, struct rddma_bind *second)
{
	struct seg_desc *rio1 = (struct seg_desc *)&first->head_dst->head_src->descriptor;
	struct seg_desc *rio2 = (struct seg_desc *)&second->head_dst->head_src->descriptor;
	struct seg_desc *riolast = to_sdesc(rio1->node.prev);
	list_add_tail(&rio1->node, &rio2->node);
	riolast->hw.next = rio2->paddr &0xffffffe0;
}

static struct rddma_dma_engine *dma_rio_get(struct rddma_dma_engine *rde)
{
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,rde);
	return rde;
}

static void dma_rio_put(struct rddma_dma_engine *rde)
{
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,rde);
}

static struct rddma_dma_ops dma_rio_ops = {
	.load      = dma_rio_load,
	.link_src  = dma_rio_link_src,
	.link_dst  = dma_rio_link_dst,
	.link_bind = dma_rio_link_bind,
	.get       = dma_rio_get,
	.put       = dma_rio_put,
};

static int __init dma_rio_init(void)
{
	struct dma_engine *de;
	struct rddma_dma_engine *rde;

	if ( (de = new_dma_engine()) ) {
		rde = &de->rde;
		snprintf(rde->name, RDDMA_MAX_DMA_NAME_LEN, "%s", "rddma_rio_dma");
		return rddma_dma_register(rde);
	}
	return -ENOMEM;
}

static void __exit dma_rio_close(void)
{
	rddma_dma_unregister("rddma_rio_dma");
}

module_init(dma_rio_init);
module_exit(dma_rio_close);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phil Terry <pterry@micromemory.com>");
MODULE_DESCRIPTION("DMA Engine for RDDMA");
