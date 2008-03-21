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
#define MY_DEBUG      RDDMA_DBG_DMANET | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_DMANET | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/vfi.h>
#include <linux/vfi_dma.h>
#include <linux/vfi_dma_net.h>
#include <linux/vfi_src.h>
#include <linux/vfi_srcs.h>
#include <linux/vfi_binds.h>

struct dma_engine {
	struct rddma_dma_engine rde;
};
static struct dma_engine *de;

static inline struct dma_engine *to_dma_engine(struct rddma_dma_engine *rde)
{
	return rde ? container_of(rde, struct dma_engine, rde) : NULL;
}

static inline dma_addr_t ldesc_virt_to_phys(struct dma_list *d)
{
	struct my_xfer_object *va = (struct my_xfer_object *) d;
	return (u32) va->paddr;
}



/* Fill out fields of DMA descriptor */

static void dma_net_load(struct rddma_src *src)
{
	RDDMA_DEBUG(MY_DEBUG,"%s src(%p)\n",__FUNCTION__,src);
}

static void dma_net_link_src(struct list_head *first, struct rddma_src *second)
{
	struct seg_desc *rio2 = (struct seg_desc *)&second->descriptor;
	struct seg_desc *riolast; 
	RDDMA_DEBUG(MY_DEBUG,"%s first(%p) second(%p)\n",__FUNCTION__,first,second);
	if (!list_empty(first)) {
		riolast = to_sdesc(first->prev);
		riolast->hw.next = rio2->paddr & ~0x1f;	/* 64-bit safe (0xffffffe0); */
	}
	list_add_tail(&rio2->node, first);
}

static void dma_net_link_dst(struct list_head *first, struct rddma_dst *second)
{
	struct seg_desc *rio2;
	struct seg_desc *riolast;
	RDDMA_DEBUG(MY_DEBUG,"%s first(%p) second(%p)\n",__FUNCTION__,first,second);
	if (second->srcs) {
		if (!list_empty(first)) {
			riolast = to_sdesc(first->prev);
			rio2 = to_sdesc(second->srcs->dma_chain.next);
			riolast->hw.next = rio2->paddr & ~0x1f;	/* 64-bit safe (0xffffffe0); */
		}
		list_splice(&second->srcs->dma_chain, first->prev);
	}
}

static void dma_net_link_bind(struct list_head *first, struct rddma_bind *second)
{
	/* Hack for now!  Use link_bind to fill out a "transfer object" */
	struct seg_desc *seg;
	struct my_xfer_object *xfo = (struct my_xfer_object *) &second->descriptor;
	RDDMA_DEBUG(MY_DEBUG,"%s first(%p) second(%p)\n",__FUNCTION__,first,second);
#ifdef LOCAL_DMA_ADDRESS_TEST
	xfo->xf.cb = address_test_completion;
#else
	xfo->xf.cb = NULL;
#endif
	xfo->xf.flags = RDDMA_XFO_READY;
	xfo->xf.len = second->desc.src.extent;

	/* Fill out list descriptor! */

	xfo->hw.next = DMA_END_OF_CHAIN;
	seg = to_sdesc(second->dma_chain.next);
	xfo->hw.link = seg->paddr;
	/* list and link descriptors in low memory */
	xfo->hw.next_ext = xfo->hw.link_ext = 0;
	/* striding not supported for now */
	xfo->hw.src_stride = xfo->hw.dest_stride = 0;
	return;
}

static void dma_net_cancel_transfer(struct rddma_dma_descriptor *desc)
{
	RDDMA_DEBUG(MY_DEBUG,"%s desc(%p)\n",__FUNCTION__,desc);
}

static void dma_net_queue_transfer(struct rddma_dma_descriptor *list)
{
	RDDMA_DEBUG(MY_DEBUG,"%s list(%p)\n",__FUNCTION__,list);
}

static struct rddma_dma_engine *dma_net_get(struct rddma_dma_engine *rde)
{
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,rde);
	return rde;
}

static void dma_net_put(struct rddma_dma_engine *rde)
{
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,rde);
}

static struct rddma_dma_ops dma_net_ops = {
	.load      = dma_net_load,
	.link_src  = dma_net_link_src,
	.link_dst  = dma_net_link_dst,
	.link_bind = dma_net_link_bind,
	.queue_transfer = dma_net_queue_transfer,
	.cancel_transfer = dma_net_cancel_transfer,
	.get       = dma_net_get,
	.put       = dma_net_put,
};

static struct dma_engine *new_dma_engine(void)
{
	struct dma_engine *new;
	if ( (new = kzalloc(sizeof(struct dma_engine),GFP_KERNEL)) ) {
		new->rde.owner = THIS_MODULE;
		new->rde.ops = &dma_net_ops;
	}
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,new);
	return new;
}

static int __init dma_net_init(void)
{
	struct rddma_dma_engine *rde;
	int err;

	if ( (de = new_dma_engine()) ) {
		rde = &de->rde;
		snprintf(rde->name, RDDMA_MAX_DMA_NAME_LEN, "%s", "rddma_net_dma");
	}
	else
		return -ENOMEM;

	err = rddma_dma_register(rde);

	if (err == 0) {
		return 0;
	}

	return err;
}

static void __exit dma_net_close(void)
{
	rddma_dma_unregister("rddma_net_dma");
	kfree(de);
}

module_init(dma_net_init);
module_exit(dma_net_close);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phil Terry <pterry@micromemory.com>");
MODULE_DESCRIPTION("Dummy net DMA Engine for RDDMA");
