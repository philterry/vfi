
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

#define MY_DEBUG      RDDMA_DBG_PARSE | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_PARSE | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/rddma.h>
#include <linux/rddma_parse.h>
#include <linux/rddma_ops.h>
#include <linux/rddma_fabric.h>
#include <linux/rddma_bind.h>
#ifdef CONFIG_MPC10X_BRIDGE
#include "mpc10xdma.h"
#else
#include <linux/rddma_dma_rio.h>
#endif
#include <linux/rddma_dst.h>
#include <linux/rddma_src.h>
#include <linux/rddma_dma.h>
#include <linux/slab.h>
#include <linux/string.h>

void desc_param_dump(struct rddma_desc_param *d)
{
	int i;
	if (d == NULL) {
		printk("NULL desc_param!\n");
		return;
	} else
		printk("desc_param @ %p\n", d);

	if (d->name)
		printk("name = %s\n", d->name);
	else
		printk("name = <NULL>\n");

	if (d->location)
		printk("location = %s\n", d->location);
	else
		printk("location = <NULL>\n");

	printk("offset = 0x%llx\n", d->offset);
	printk("extent = 0x%x\n", d->extent);
	i = 0;
	while (d->query && d->query[i]) {
		printk("query str @ %p", d->query[i]);
		printk(" = %s\n", d->query[i]);
		i++;
	}

	if (d->address) {
		printk("fabric name = %s\n", &d->address->name[0]);
	}

	printk("ops(%p) is ",d->ops);
	if (d->ops) {
		if (d->ops == &rddma_local_ops)
			printk("local\n");
		else if (d->ops == &rddma_fabric_ops)
			printk("fabric\n");
		else
			printk("unknown\n");
	}
	else
		printk("NULL\n");
	/* not dumping ops fields */
	return;
}
void bind_param_dump(struct rddma_bind_param *b)
{
	printk("Xfer param:\n");
	desc_param_dump(&b->xfer);
	printk("Destination param:\n");
	desc_param_dump(&b->dst);
	printk("Source param:\n");
	desc_param_dump(&b->src);
	return;

}

#if 0
void xfer_desc_dump(struct rddma_xfer_param *x)
{
	/* Consists of a desc_parm and a bind param */
	printk("Xfer desc param:\n");
	desc_param_dump(&x->xfer);
	printk("Xfer bind param:\n");
	bind_param_dump(&x->bind);
	return;
}
#endif

void rddma_dma_chain_dump(struct list_head *h)
{
	struct seg_desc *dma_desc;
	struct list_head *entry;
	int i;

	i = 0;

	list_for_each(entry, h) {
		dma_desc = to_sdesc(entry);
#ifndef CONFIG_MPC10X_BRIDGE
		printk("Descriptor %d @ %p, 0x%llx (phys)\n", ++i,
		       dma_desc, dma_desc->paddr);
		printk("	Src = 0x%x, Dest = 0x%x, len = 0x%x\n",
		       dma_desc->hw.saddr, dma_desc->hw.daddr,
		       dma_desc->hw.nbytes);

		if (dma_desc->hw.next == 1)
			printk("	<end-of-chain>\n");
		else
			printk("	Next = 0x%x\n", dma_desc->hw.next);

#else
		printk("Descriptor %d @ 0x%x, 0x%llx (phys)\n", ++i,
		       (unsigned int) dma_desc, dma_desc->paddr);
		printk("	Src = 0x%x, Dest = 0x%x, len = 0x%x\n",
		       readl(&dma_desc->hw.saddr), readl(&dma_desc->hw.daddr),
		       readl(&dma_desc->hw.nbytes));

		if (readl(&dma_desc->hw.next) == 1)
			printk("	<end-of-chain>\n");
		else
			printk("	Next = 0x%x\n", readl(&dma_desc->hw.next));
		printk("raw dump:\n");
		{
			int i;
			unsigned int *p = dma_desc;
			for (i = 0; i < 16; i++)
				printk("i = %d, val = 0x%x\n",i, *p++);
		}
#endif
	}
}
