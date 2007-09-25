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
#include <linux/platform_device.h>
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
		riolast->hw.next = rio2->paddr & ~0x1f;	/* 64-bit safe (0xffffffe0); */
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
		riolast->hw.next = rio2->paddr & ~0x1f;	/* 64-bit safe (0xffffffe0); */
	}
	list_splice(&second->srcs->dma_chain, first->prev);
}

/* Link the DMA chain in the "second" bind to the tail of "first" DMA chain */
static void dma_rio_link_bind(struct list_head *first, struct rddma_bind *second)
{
	struct seg_desc *rio2;
	struct seg_desc *riolast;
	if (!list_empty(first)) {
		riolast = to_sdesc(first->prev);
		rio2 = to_sdesc(second->dma_chain.next);
		riolast->hw.next = rio2->paddr & ~0x1f;	/* 64-bit safe (0xffffffe0); */
	}
	list_splice(&second->dma_chain, first->prev);
}

/* Unlink the DMA chain in the "second" bind from the "first" DMA chain */
static void dma_rio_unlink_bind(struct list_head *first, struct rddma_bind *second)
{
	struct seg_desc *rioend = NULL;
	struct seg_desc *rioprev = NULL;
	struct list_head *start = second->dma_chain.next;
	struct list_head *end = second->end_of_chain;
	/* link start->prev to end->next */
	start->prev->next = end->next;
	end->next->prev = start->prev;
	rioend = to_sdesc(end);
	if (start->prev != first) {
		rioprev = to_sdesc(start->prev);
		rioprev->hw.next = rioend->hw.next;
		rioprev->hw.next_ext = rioend->hw.next_ext;
	}
	rioend->hw.next = 1;
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
	.unlink_bind = dma_rio_unlink_bind,
	.get       = dma_rio_get,
	.put       = dma_rio_put,
};

/* Jimmy!  Temporary hack to test platform probe! */
static int __devinit bark_bark(struct platform_device *pdev);
static int __devinit piggy(struct platform_device *pdev);

static struct platform_driver dummy = {
	.probe = bark_bark,
	.remove = piggy,
	.driver = {
		.name = "fsl-dma",
		.owner = THIS_MODULE,
	},
};

static int __init dma_rio_init(void)
{
	struct dma_engine *de;
	struct rddma_dma_engine *rde;

platform_driver_register (&dummy);

	if ( (de = new_dma_engine()) ) {
		rde = &de->rde;
		snprintf(rde->name, RDDMA_MAX_DMA_NAME_LEN, "%s", "rddma_rio_dma");
		return rddma_dma_register(rde);
	}
	return -ENOMEM;
}

static int __devinit bark_bark (struct platform_device *pdev)
{
	struct resource *res;
	printk("BARK! BARK!\n");
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	printk("start = 0x%x\n", res->start);
	printk("end = 0x%x\n", res->end);
	printk("size = 0x%x\n", res->end - res->start);
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	return 0;
}

static int __devinit piggy (struct platform_device *pdev)
{
	printk("PIGGY! PIGGY!\n");
	printk("start = 0x%x\n", pdev->resource[0].start);
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
