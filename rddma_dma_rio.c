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
#include <linux/rddma_dma.h>
#include <linux/rddma_dma_rio.h>
#include <linux/rddma_src.h>
#include <linux/rddma_srcs.h>
#include <linux/rddma_binds.h>
#include <linux/rddma_fabric.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <asm/io.h>
#include "./ringbuf.h"

#define SPOOL_AND_KICK
#undef LOCAL_DMA_ADDRESS_TEST

extern int get_rio_id (struct rddma_fabric_address *x);

struct dma_engine {
	struct rddma_dma_engine rde;
	struct completion dma_callback_sem;
	struct ppc_dma_chan ppc8641_dma_chans[PPC8641_DMA_NCHANS];
	struct task_struct *callback_thread;
	void *regbase;
	int nchans;
	int next_channel;
};

#if defined (CONFIG_FSL_SOC) || defined(CONFIG_FSL_BOOKE)
extern phys_addr_t get_immrbase(void); /* defined in sysdev/fsl_soc.c */
#else
/* A stub to allow us to compile on other platforms */
static inline void *get_immrbase(void) {
	return 0; 
}
#endif

#ifdef CONFIG_PROC_FS
extern struct proc_dir_entry *proc_root_rddma;
static struct proc_dir_entry *proc_dev_dir;
#endif

static struct dma_engine *de;

/* Default module param values */
static unsigned int first_chan = 0;
static unsigned int last_chan = PPC8641_DMA_NCHANS - 1;

#define NEVENTS 100
static int nevents = NEVENTS;

static RINGBUF ering1;
static RINGBUF ering2;
static RINGBUF *event_ring_out = &ering1;
static RINGBUF *event_ring_in = &ering2;
static struct ppc_dma_event *event_array;
static struct ppc_dma_event **event_in;
static struct ppc_dma_event **event_out;

static void start_dma(struct ppc_dma_chan *chan, struct my_xfer_object *xfo);
static struct ppc_dma_chan *load_balance(struct my_xfer_object *xfo);
#ifdef LOCAL_DMA_ADDRESS_TEST
static void address_test_completion (struct rddma_dma_descriptor *dma_desc);
#endif

static inline struct dma_engine *to_dma_engine(struct rddma_dma_engine *rde)
{
	return rde ? container_of(rde, struct dma_engine, rde) : NULL;
}

static struct rddma_dma_ops dma_rio_ops;
static int  ppcdma_queue_chain(struct ppc_dma_chan *chan,
       	struct my_xfer_object *xfo);

#if !defined(CONFIG_FSL_SOC) && !defined(CONFIG_FSL_BOOKE) /* So we can build for x86 */
#define out_be32(x,y) return
#define in_be32(x) 0
#endif

static inline void dma_set_reg(struct ppc_dma_chan *chan,
			       unsigned int offset, u32 value)
{
#ifdef DEBUG_ON_6460
	return; 
#endif
	out_be32(chan->regbase + offset, value);
}

static inline u32 dma_get_reg(struct ppc_dma_chan *chan,
			      unsigned int offset)
{
#ifdef DEBUG_ON_6460
	return;
#endif
	return in_be32(chan->regbase + offset);
}

static inline dma_addr_t ldesc_virt_to_phys(struct dma_list *d)
{
	struct my_xfer_object *va = (struct my_xfer_object *) d;
	return (u32) va->paddr;
}

static int __devinit mpc85xx_rddma_probe(struct platform_device *pdev);
static int __devinit mpc85xx_rddma_remove(struct platform_device *pdev);

static struct platform_driver mpc85xx_rddma_driver = {
	.probe = mpc85xx_rddma_probe,
	.remove = mpc85xx_rddma_remove,
	.driver = {
		.name = "fsl-dma",
		.owner = THIS_MODULE,
	},
};

static int dma_completion_thread(void *data)
{
	struct ppc_dma_event *pevent;
	struct ppc_dma_chan *chan;
	struct my_xfer_object *xfo;
	printk("Starting completion thread\n");
	/* Send completion messages to registered callback function */
	while (1) {
		wait_for_completion(&de->dma_callback_sem);
		if (kthread_should_stop())
			goto stop_thread;
		pevent =
		    (struct ppc_dma_event *) ringbuf_get(event_ring_out);
		while (pevent) {
			chan = &de->ppc8641_dma_chans[pevent->chan_num];
			printk("DMA completion event on channel %d\n", chan->num);
			xfo = pevent->desc;
			/* Jimmy, take semaphore here */
			chan->bytes_queued -= xfo->xf.len;
			if (pevent->status == PPCDMA_OK)
				xfo->xf.flags = RDDMA_XFO_COMPLETE_OK;
			else
				xfo->xf.flags = RDDMA_XFO_COMPLETE_ERROR;
			xfo->xf.flags |= pevent->chan_num < 8;
			/* Jimmy, give semaphore here */
			/* Invoke callback */
			if (xfo->xf.cb) {
				xfo->xf.cb((struct rddma_dma_descriptor *) xfo);
			}
			ringbuf_put(event_ring_in, (void *) pevent);
			pevent = (struct ppc_dma_event *)
			    ringbuf_get(event_ring_out);

			/* Test for driver exit again here, since callbacks
			 * can sleep
			 */
			if (kthread_should_stop())
				goto stop_thread;
		}
	}
stop_thread:
	printk("Exiting completion thread\n");
	return 0;
}

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


static int rddma_is_local(struct rddma_desc_param *desc)
{
	return ((unsigned int) desc->ops ==
	      	(unsigned int) &rddma_local_ops);
}
static int rddma_is_bypass_atmu(struct rddma_desc_param *src)
{
	/* That's all we're supporting for now, ATMU mapping comes later */
	return 1;
}

static int rddma_rio_id(struct rddma_desc_param *src)
{
	return (get_rio_id(src->address));
}

/* Stub for now */
static u64 rio_to_ocn(int rio_id, u64 addr)
{
	return 0;
}

/* Fill out fields of DMA descriptor */

/*  Note: Need upper level to provide functions:
 *
 * rddma_is_local(struct rddma_desc_param *src)
 * rddma_is_bypass_atmu(struct rddma_desc_param *src) 
 * rddma_rio_id(struct rddma_desc_param *src)
 * rio_to_ocn(int rio_id, struct rddma_desc_param *src)
 *
 * For now they're just stubs.  
 *
 * Assumptions:  Local src/dest addresses are kernel va's.
 *               Remote src/dest addresses are 34-bit RIO va's.
 *               DMA controller bypasses the ATMU's.
 */

static void dma_rio_load(struct rddma_src *src)
{
	struct seg_desc *rio = (struct seg_desc *)&src->descriptor;
	u64 phys;
	rio->paddr = (u64) virt_to_phys(rio);

	if (rddma_is_local(&src->desc.src)) {
		RDDMA_DEBUG(MY_DEBUG,"DMA source is local, va = 0x%x\n",
			(u32) src->desc.src.offset);
		/* Stash upper 4 bits of 36-bit OCN address and
		 * turn on snooping (for now)
		 */
		phys = virt_to_phys((void *) (u32) src->desc.src.offset);
		rio->hw.saddr = phys & 0x000000ffffffffULL;
		rio->hw.src_attr = (phys >> 32) & HIGH_LOCAL_ADDR_MASK;
		rio->hw.src_attr |= DMA_ATTR_LOCAL_SNOOP;
	}
	else if (rddma_is_bypass_atmu(&src->desc.src)) {
		/* Stash upper 2 bits of 34-bit RIO address and
		 * turn off snooping.  This is OK for "bypass ATMU" case.
		 */
		RDDMA_DEBUG(MY_DEBUG,"DMA source is remote\n");
		RDDMA_DEBUG(MY_DEBUG,"  RIO id = %d\n",rddma_rio_id(&src->desc.src));
		phys = src->desc.src.offset;
		rio->hw.saddr = phys & 0x00000000ffffffffULL;
		rio->hw.src_attr = (phys >> 32) & HIGH_RIO_ADDR_MASK;
		rio->hw.src_attr |= (DMA_ATTR_BYPASS_ATMU | DMA_ATTR_RIO | 
			DMA_ATTR_NREAD | DMA_ATTR_HI_FLOW);
		rio->hw.src_attr |= DMA_ATTR_TID(rddma_rio_id(&src->desc.src));
	}
	else {
		/* Stash upper 4 bits of 36-bit OCN address and
		 * turn off snooping
		 */
		RDDMA_DEBUG(MY_DEBUG,"Error!! ATMU mapping not supported\n");
		phys = rio_to_ocn(rddma_rio_id(&src->desc.src), 
			src->desc.src.offset);
		rio->hw.saddr = phys & 0x00000000ffffffffULL;
		rio->hw.src_attr = (phys >> 32) & HIGH_LOCAL_ADDR_MASK;
		rio->hw.src_attr |= DMA_ATTR_LOCAL_NOSNOOP;
	}

	if (rddma_is_local(&src->dst->desc.dst)) {
		RDDMA_DEBUG(MY_DEBUG,"DMA destination is local, va = 0x%x\n",
			(u32) src->desc.dst.offset);
		phys = virt_to_phys((void *) (u32) src->desc.dst.offset);
		rio->hw.daddr = phys & 0x00000000ffffffffULL;
		rio->hw.dest_attr = (phys >> 32) & HIGH_LOCAL_ADDR_MASK;
		rio->hw.dest_attr |= DMA_ATTR_LOCAL_SNOOP;
	}
	else if (rddma_is_bypass_atmu(&src->dst->desc.dst)) {
		RDDMA_DEBUG(MY_DEBUG,"DMA destination is remote\n");
		RDDMA_DEBUG(MY_DEBUG,"  RIO id = %d\n",rddma_rio_id(&src->desc.dst));
		phys = src->desc.dst.offset; /* Assume phys is 34-bit RIO addr */
		rio->hw.daddr = phys & 0x00000000ffffffffULL;
		rio->hw.dest_attr = (phys >> 32) & HIGH_RIO_ADDR_MASK;
		rio->hw.dest_attr |= DMA_ATTR_LOCAL_NOSNOOP;
		rio->hw.dest_attr |= (DMA_ATTR_BYPASS_ATMU | DMA_ATTR_RIO | 
			DMA_ATTR_NWRITE | DMA_ATTR_HI_FLOW);
		rio->hw.dest_attr |= DMA_ATTR_TID(rddma_rio_id(&src->desc.dst));
	}
	else {
		RDDMA_DEBUG(MY_DEBUG,"Error!! ATMU mapping not supported\n");
		phys = rio_to_ocn(rddma_rio_id(&src->desc.dst), 
			src->desc.dst.offset);
		rio->hw.daddr = phys & 0x00000000ffffffffULL;
		rio->hw.dest_attr = (phys >> 32) & HIGH_LOCAL_ADDR_MASK;
		rio->hw.dest_attr |= DMA_ATTR_LOCAL_NOSNOOP;
	}

	rio->hw.nbytes = src->desc.src.extent; 
	rio->hw.next_ext = 0;
	rio->hw.next = DMA_END_OF_CHAIN;

#ifdef LOCAL_DMA_ADDRESS_TEST
	if (rddma_is_local(&src->desc.src) &&
	    rddma_is_local(&src->desc.dst)) {
		/* Jimmy...Address test:  write address into source array */
		int *p;
		int i;
		p = (int *) ((int) src->desc.src.offset);
		for (i = 0; i < src->desc.src.extent/4; i++)
			*p++ = (unsigned int) p;
	}
#endif
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
	if (second->srcs) {
		if (!list_empty(first)) {
			riolast = to_sdesc(first->prev);
			rio2 = to_sdesc(second->srcs->dma_chain.next);
			riolast->hw.next = rio2->paddr & ~0x1f;	/* 64-bit safe (0xffffffe0); */
		}
		list_splice(&second->srcs->dma_chain, first->prev);
	}
}

static void dma_rio_link_bind(struct list_head *first, struct rddma_bind *second)
{
	/* Hack for now!  Use link_bind to fill out a "transfer object" */
	struct seg_desc *seg;
	struct my_xfer_object *xfo = (struct my_xfer_object *) &second->descriptor;
	xfo->paddr = (u64) virt_to_phys(&xfo->hw);
#ifdef LOCAL_DMA_ADDRESS_TEST
	xfo->xf.cb = address_test_completion;
#else
	xfo->xf.cb = (void (*)(struct rddma_dma_descriptor *))rddma_dma_complete;
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

int find_in_queue(struct list_head *q, struct my_xfer_object *target)
{
	struct list_head *desc_node;
	struct list_head *temp_node;
	struct my_xfer_object *curr;
	int i = 0;
	list_for_each_safe(desc_node, temp_node, q) {
		i++;
		curr = list_entry(desc_node, struct my_xfer_object, xf.node);
		if (curr == target) {
			return i;
		}
	}
	return 0;
}

/*
*   Remove 'desc' from the transfer queue
*   If 'desc' is running in the DMA engine, abort the transfer
*   return 0 if node deleted
*   return -EINVAL  if node not found in queue
*   return -EBUSY if internal error
*/
static void dma_rio_cancel_transfer(struct rddma_dma_descriptor *desc)
{
	struct my_xfer_object *xfo = (struct my_xfer_object *) desc;
	struct list_head *node;
	unsigned long flags;
	unsigned long status;
	struct ppc_dma_chan *chan;
	int match;
	int num;

	/* Get channel number */
	num = XFO_CHAN(xfo);

	if (num < first_chan || num > last_chan) 
		return /* -EINVAL */;

	chan = &de->ppc8641_dma_chans[num];
	spin_lock_irqsave(&chan->queuelock, flags);
	match = find_in_queue(&chan->dma_q, xfo);
	if (match == 0) {	/* Descriptor not found in queue */
		spin_unlock_irqrestore(&chan->queuelock, flags);
		return /* -EINVAL */;
	}

	/* Descriptor not at head of queue */
	if (match != 1) {
		list_del(&xfo->xf.node);
		xfo->xf.flags &= ~XFO_STAT_MASK;
		xfo->xf.flags |= RDDMA_XFO_CANCELLED;
		spin_unlock_irqrestore(&chan->queuelock, flags);
		return;
	}

	/* Descriptor at head of queue */
	if (chan->state != DMA_RUNNING) {
		list_del(&xfo->xf.node);
		xfo->xf.flags &= ~XFO_STAT_MASK;
		xfo->xf.flags |= RDDMA_XFO_CANCELLED;
		spin_unlock_irqrestore(&chan->queuelock, flags);
		return;
	}

	/* DMA in flight!  Abort the transer */
	dma_set_reg(chan, DMA_MR, dma_get_reg(chan, DMA_MR) | DMA_MODE_ABORT);
#if defined(CONFIG_FSL_SOC) || defined(CONFIG_FSL_BOOKE) /* More x86 */
	isync();
#endif
	chan->state = DMA_IDLE;
	/* Ack away interrupts */
	dma_set_reg(chan, DMA_SR, dma_get_reg(chan, DMA_SR));

	list_del(&xfo->xf.node);
	xfo->xf.flags &= ~XFO_STAT_MASK;
	xfo->xf.flags |= RDDMA_XFO_CANCELLED;

	/* If queue head was deleted, launch next transfer */
	if (!list_empty(&chan->dma_q)) {
		status = dma_get_reg(chan, DMA_SR);
		if (status & DMA_STAT_CHAN_BUSY) {
			printk
			    ("DMA-%i Error: channel abort failed\n",
			     chan->num);
			chan->state = DMA_ERR_STOP;
			spin_unlock_irqrestore(&chan->queuelock, flags);
			return /* -EBUSY */;
#if  0
			BUG_ON();
#endif
		}
		node = chan->dma_q.next;
		xfo = to_xfer_object(node);
		start_dma(chan, xfo);
	}

	spin_unlock_irqrestore(&chan->queuelock, flags);
	return;
}

static void dma_rio_queue_transfer(struct rddma_dma_descriptor *list)
{
	struct ppc_dma_chan *chan;
	struct my_xfer_object *xfo = (struct my_xfer_object *) list;
	if (de->nchans == 1) {
		ppcdma_queue_chain(&de->ppc8641_dma_chans[first_chan], xfo);
		return;
	}
	chan = load_balance(xfo);
	ppcdma_queue_chain(chan, xfo);
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

#ifdef LOCAL_DMA_ADDRESS_TEST
/* This is the original test code, before completion thread */
static void address_test_completion (struct rddma_dma_descriptor *dma_desc)
{
	struct my_xfer_object *xfo = (struct my_xfer_object *) dma_desc;
	struct dma_link *sdesc;
	struct rddma_bind *bind;
	struct rddma_xfer *xfer;
	struct list_head *entry;
	int *p;
	int *p2;
	int i;
	int len;
	printk("DMA complete, status = 0x%x\n", XFO_STATUS(xfo));
	/* Address test, make sure all vals in destination equal corresponding
	 * source addresses
	 */
	sdesc = (struct dma_link *) ((u32) phys_to_virt(xfo->hw.link) & 
		0xffffffe0);
loop:
	p = phys_to_virt(sdesc->daddr);
	p2 = phys_to_virt(sdesc->saddr);
	len = (sdesc->nbytes)/4;
	/* Compare current segment */
	for (i = 0; i < len; i++)
		if (*p != *p2) {
			printk("dma failed\n");
			goto err;
		}
		else {
			if (*p2 != (unsigned int) p2)
				printk("bogus test!\n");
			p++; 
			p2++; 
		}
        /* Next segment or terminate */
	if ((sdesc->next & 0x1) == 0) {
		sdesc = (struct dma_link *) ((u32) phys_to_virt(sdesc->next) & 
			0xffffffe0);
		printk("segment ok, next desc at va 0x%x\n", (unsigned int) sdesc);
		goto loop;
	}

#if 0
	if (xfo->xf.cb == NULL)
		printk("DMA complete, status = 0x%x\n", status);
#endif
err:
	rddma_dma_complete ((struct rddma_bind *) dma_desc);
}
#endif /* LOCAL_DMA_ADDRESS_TEST */

static void send_completion (struct ppc_dma_chan *chan, 
	struct my_xfer_object *xfo, int status)
{
	struct ppc_dma_event *event;

	event = (struct ppc_dma_event *) ringbuf_get(event_ring_in);
	if (!event) {
		printk("DMA Internal Error:  Completion ring overflow");
		return;
	}
	event->desc = xfo;
	event->status = status;
	event->chan_num = chan->num;
	ringbuf_put(event_ring_out, (void *) event);
	complete(&de->dma_callback_sem);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
static irqreturn_t do_interrupt(int irq, void *data, struct pt_regs *ep)
#else
static irqreturn_t do_interrupt(int irq, void *data)
#endif
{
	struct ppc_dma_chan *chan = (struct ppc_dma_chan *) data;
	unsigned int status;
	unsigned int cb, te, pe;
	struct list_head *desc_node;
	struct my_xfer_object *pdesc;

	status = dma_get_reg(chan, DMA_SR);
printk("DMA interrupt, status = 0x%x\n", status);

	/* Clear interrupts */
	dma_set_reg(chan, DMA_SR, status);

	/* Figure out the channel state */
	cb = status & DMA_STAT_CB;
	if (cb) {
		chan->state = DMA_RUNNING;
		/* Only valid interrupt in which channel continues
		 * activity is a segment interrupt... also a chain
		 * interrupt in extended mode.  Just ACK and return.
		 */
		if (status & DMA_STAT_SEGMENT_INTERRUPT)
			chan->seg_int++;
		else if (status & DMA_STAT_CHAIN_INTERRUPT)
			chan->chain_int++;
		else 
			chan->bogus_int++;

		return (IRQ_HANDLED);
	}

	chan->state = DMA_IDLE;

	/* Check for errors */
	te = status & DMA_STAT_TE;
	if (te) {
		chan->state = DMA_ERR_STOP;
		chan->errors++;
		chan->err_int++;
	}
	pe = status & DMA_STAT_PE;
	if (pe) {
		chan->state = DMA_ERR_STOP;
		chan->errors++;
		if (!te)
			chan->err_int++;
	}

printk("DMA interrupt, chan->state = 0x%x\n", chan->state);

	if (status & DMA_STAT_LIST_INTERRUPT)
		chan->list_int++;

	if (list_empty(&chan->dma_q)) {
printk("DMA interrupt, empty list\n");
		/* Spurious interrupt??  Transfer Q empty */
		chan->bogus_int++;
		chan->state = DMA_IDLE;
		return IRQ_HANDLED;
	}

	desc_node = chan->dma_q.next;
	pdesc = to_xfer_object(desc_node);
	list_del(desc_node); 	/* De-queue the completed chain */

	/* Wake up completion callback thread and launch the next DMA */
	if (chan->state == DMA_ERR_STOP) {
		pdesc->xf.extra = (te | pe);
		send_completion(chan, pdesc, PPCDMA_ERROR);
	} else {
		chan->bytes_tx += pdesc->xf.len;
		send_completion(chan, pdesc, PPCDMA_OK);
		if (!list_empty(&chan->dma_q)) {
			pdesc = to_xfer_object(chan->dma_q.next);
			start_dma(chan, pdesc);
		}
	}
	return (IRQ_HANDLED);
}
static void start_dma(struct ppc_dma_chan *chan, struct my_xfer_object *xfo)
{
	chan->state = DMA_RUNNING;
        xfo->xf.flags = (chan->num << 8) | RDDMA_XFO_RUNNING;
	/* Extended mode, Single write start */
	dma_set_reg(chan, DMA_CLSDAR, ldesc_virt_to_phys(&xfo->hw));
	return;
}

static int  ppcdma_queue_chain(struct ppc_dma_chan *chan, 
	struct my_xfer_object *xfo)
{
	unsigned long flags;
#ifdef DEBUG_ON_6460
	struct seg_desc *sdesc;
	void *sdesc_phys;

	/* Jimmy, just dump descriptors and return! */

 	printk("List descriptor at 0x%x (pa), 0x%x (va)\n", ldesc_virt_to_phys(&xfo->hw), &xfo->hw);
	printk("\tNext list desc extended addr = 0x%x\n", xfo->hw.next_ext);
	printk("\tNext list desc addr = 0x%x\n",xfo->hw.next);
	printk("\tFirst link desc extended addr = 0x%x\n", xfo->hw.link_ext);
	printk("\tFirst link desc addr = 0x%x\n", xfo->hw.link);
	printk("\tSource stride = 0x%x\n", xfo->hw.src_stride);
	printk("\tDest stride = 0x%x\n", xfo->hw.dest_stride);

	sdesc = (struct dma_link *) ((u32) phys_to_virt(xfo->hw.link) & 
		0xffffffe0);
	sdesc_phys = xfo->hw.link;

print_loop:

 	printk("List descriptor at 0x%x (pa), 0x%x (va)\n", sdesc_phys, sdesc);
	printk("\tSource attributes = 0x%x\n", sdesc->hw.src_attr);
	printk("\tSource address = 0x%x\n",sdesc->hw.saddr);
	printk("\tDest attributes = 0x%x\n", sdesc->hw.dest_attr);
	printk("\tDest addr = 0x%x\n", sdesc->hw.daddr);
	printk("\tNext link desc extended addr = 0x%x\n",sdesc->hw.next_ext);
	printk("\tNext link desc addr  = 0x%x\n", sdesc->hw.next);
	printk("\tByte count  = 0x%x\n", sdesc->hw.nbytes);
	
	sdesc->hw.next = DMA_END_OF_CHAIN;
	if (!(sdesc->hw.next == DMA_END_OF_CHAIN)) {
		sdesc = (struct dma_link *)
		       	((u32) phys_to_virt(sdesc->hw.next) & 0xffffffe0);
		sdesc_phys = sdesc->hw.next;
		goto print_loop;
	}
	return 0;

#endif /* DEBUG_ON_6460 */
 	

	/* Lock interrupts -- ISR can dequeue */
	spin_lock_irqsave(&chan->queuelock, flags);
	if (chan->state != DMA_IDLE && chan->state != DMA_RUNNING) {
		/* channel in error state, don't queue this node */
		spin_unlock_irqrestore(&chan->queuelock, flags);
		return (-EAGAIN);
	}

	/* Spool the DMA chain */
	list_add_tail(&xfo->xf.node, &chan->dma_q);
        chan->bytes_queued += xfo->xf.len;

	/* Launch if DMA engine idle */
	if (chan->state == DMA_IDLE) {
#ifdef DEBUG_DMA
		if (chan->dma_q.prev != &xfo->xf.node) {
			printk
			    ("DMA-%d error: queue not empty when channel idle",
			     chan->num);
		}
#endif
		start_dma(chan, xfo);
	}
	else
		xfo->xf.flags = (chan->num << 8) | RDDMA_XFO_QUEUED;

	spin_unlock_irqrestore(&chan->queuelock, flags);
	return 0;
}

static struct rddma_dma_ops dma_rio_ops = {
	.load      = dma_rio_load,
	.link_src  = dma_rio_link_src,
	.link_dst  = dma_rio_link_dst,
	.link_bind = dma_rio_link_bind,
	.queue_transfer = dma_rio_queue_transfer,
	.cancel_transfer = dma_rio_cancel_transfer,
	.get       = dma_rio_get,
	.put       = dma_rio_put,
};

#ifdef CONFIG_PROC_FS
static int proc_dump_dma_stats(char *buf, char **start, off_t offset,
	int count, int *eof, void *data)
{
	int len = 0;
	int index = (int) data;
	struct ppc_dma_chan *chan = &de->ppc8641_dma_chans[index];

	len += sprintf(buf + len, " Number of DMA list complete interrupts = %d\n", 
			chan->list_int);
	len += sprintf(buf + len, " Number of bytes transmitted = %lld, (0x%llx)\n",
			chan->bytes_tx, chan->bytes_tx);
	len += sprintf(buf + len, " Number of unknown interrupts = %d\n", 
			chan->bogus_int);
	len += sprintf(buf + len, " Number of errors = %d\n", 
			chan->err_int);
	return len;
}
#endif

#if defined(CONFIG_FSL_SOC) || defined(CONFIG_FSL_BOOKE) || defined(CONFIG_MPC10X_BRIDGE)
/* Initialize per-channel data structures and h/w.
 * Return number of channels.
 */
static int setup_rddma_channel(struct platform_device *pdev)
{
	u32 xfercap;
	int err;
	int index;
	struct ppc_dma_chan *chan;
	struct resource *res;

	/* 8641 -- (2**26 - 1) */
	xfercap = ~0xfc000000;
	index = pdev->id;
	if (index < first_chan || index < 0)
		return -ELNRNG;

	if (index > last_chan || index > PPC8641_DMA_NCHANS)
		return -ELNRNG;

	chan = &de->ppc8641_dma_chans[index];
	memset(chan, 0, sizeof(struct ppc_dma_chan));
	res = platform_get_resource (pdev, IORESOURCE_MEM, 0);
	BUG_ON(!res);
#if 0
	chan->device = device;
#endif
	chan->regbase = ioremap_nocache(res->start, res->end - res->start + 1);
	chan->xfercap = xfercap;
	chan->num = index;

	/* using chained list descriptors */
#ifdef SPOOL_AND_KICK
	/* 
	 * Using chained list descriptors.
	 * Write to CLSDAR register starts channel
	 */
	chan->op_mode = DMA_MODE_EXTENDED | DMA_MODE_QUICKSTART;
#else
	/* Using chained list descriptors. */
	chan->op_mode = DMA_MODE_EXTENDED;
#endif
	chan->op_mode |= DMA_MODE_ERR_INT_EN;
#ifdef SPOOL_AND_KICK
	chan->op_mode |= DMA_MODE_LIST_INT_EN;
#else
	chan->op_mode |= DMA_MODE_CHAIN_INT_EN;
#endif

	/* Might move this!! Jimmy */
	dma_set_reg(chan, DMA_MR, chan->op_mode);

	/* Using 32-bit descriptors */
	dma_set_reg(chan, DMA_ECLSDAR, 0);

	sprintf(chan->name, "rddma-chan%d", index);
	res = platform_get_resource (pdev, IORESOURCE_IRQ, 0);
	chan->irq = res->start;
	err = request_irq(chan->irq, &do_interrupt, 0, chan->name, chan);
	if (err)
		goto err_irq;
	spin_lock_init(&chan->cleanup_lock);
	spin_lock_init(&chan->queuelock);
	INIT_LIST_HEAD(&chan->dma_q);
	chan->state = DMA_IDLE;
	de->nchans++;
#ifdef CONFIG_PROC_FS
	chan->proc_dir = proc_mkdir(chan->name, proc_dev_dir);
	chan->proc_entry = create_proc_read_entry("stats", 0, 
		chan->proc_dir, proc_dump_dma_stats, (void *) chan->num);
#else
	chan->proc_dir = NULL;
#endif
	return 0;
err_irq:
	chan->state = DMA_UNINIT;
	return (err);
}
#endif

static int __init dma_rio_init(void)
{
	struct rddma_dma_engine *rde;
	int i;
	int err;

	/* check module params */
	if (last_chan >= PPC8641_DMA_NCHANS) {
		printk("Error, max value of last_chan is %d\n",
		       PPC8641_DMA_NCHANS - 1);
		return (-EINVAL);
	}
	if (first_chan > last_chan ) {
		printk("Error, first DMA channel number invalid\n");
		return (-EINVAL);
	}

	if ( (de = new_dma_engine()) ) {
		rde = &de->rde;
		de->next_channel = first_chan;
		snprintf(rde->name, RDDMA_MAX_DMA_NAME_LEN, "%s", "rddma_rio_dma");
	}
	else
		return -ENOMEM;

#if 0
/* Deal with 86XX later.  For now we're using 8548, which has
 * FSL platform bus
 */
	de->regbase = ioremap(get_ccsrbar() + MPC86XX_DMA_OFFSET,
			   MPC86XX_DMA_REG_SIZE);

	if (!de->regbase) {
		return (-ENOMEM);
	}
#endif

#ifdef CONFIG_PROC_FS
	if (!proc_root_rddma) 
		proc_root_rddma = proc_mkdir ("rddma", proc_root_driver);

	proc_dev_dir = proc_mkdir ("rddma_rio_dma", proc_root_rddma);
#endif

	platform_driver_register(&mpc85xx_rddma_driver);

#if 0  
/* 85XX:  DMA chans will be setup via callback to platform driver probe func */
#if defined(CONFIG_FSL_SOC) || defined(CONFIG_FSL_BOOKE)
	setup_dma_channels(de);
#endif
#endif

	/* Set up completion callback mechanism */
	init_completion(&de->dma_callback_sem);

	event_array = (struct ppc_dma_event *)
	    kmalloc((nevents + 1) * sizeof(struct ppc_dma_event),
		    GFP_KERNEL);
	if (!event_array) {
		err = -ENOMEM;
		/* goto err_event_array; */
	}

	event_in = kmalloc((nevents + 1) * sizeof(void *), GFP_KERNEL);
	if (!event_in) {
		err = -ENOMEM;
		goto err_event_in; 
	}
	event_out = kmalloc((nevents + 1) * sizeof(void *), GFP_KERNEL);
	if (!event_out) {
		err = -ENOMEM;
		goto err_event_out; 
	}

	for (i = 0; i < nevents; i++)
		event_in[i] = (void *) &event_array[i];

	ringbuf_init(event_ring_out, (void **) event_out, nevents + 1,
		     RING_EMPTY);
	ringbuf_init(event_ring_in, (void **) event_in, nevents + 1,
		     RING_FULL);

	/* returns a task_struct  */
	de->callback_thread = kthread_create(dma_completion_thread, NULL,
	       "RIO DMA completion");

	if (IS_ERR(de->callback_thread)) {
		err = -ENOMEM;
		goto err_cb_thread;
	}

	err = rddma_dma_register(rde);

	if (err == 0) {
	/* Start up completion callback thread */
		wake_up_process(de->callback_thread);
		return 0;
	}

err_cb_thread:
	kfree (event_out);
err_event_out: 
	kfree (event_in);
err_event_in: 
	kfree (event_array);
/* err_event_array:  */
	iounmap(de->regbase);
	kfree(de);

	return err;
}

/* This is where we'll do channel load balancing.
 * For now it's shortest queue round robin. All transfer types
 * are equally weighted.
 */
static struct ppc_dma_chan *load_balance(struct my_xfer_object *xfo)
{
	struct ppc_dma_chan *chan;
	int avail = de->next_channel;
	int i;
	int queue_len;

	/* Candidate is 'next_channel' */
	avail = de->next_channel;
	chan = &de->ppc8641_dma_chans[avail];

	/* Select shortest queue */
	queue_len = chan->bytes_queued;
	for (i = first_chan; i <= last_chan; i++) {
		if (de->ppc8641_dma_chans[i].bytes_queued < queue_len) {
			chan = &de->ppc8641_dma_chans[i];
			avail = i;
			queue_len = chan->bytes_queued;
		}
	}

	if (avail == last_chan)
		de->next_channel = first_chan;
	else
		de->next_channel++;

	return (chan);
}

static void __exit dma_rio_close(void)
{
	int i;
	struct ppc_dma_chan *chan;
	/* Stop all DMA channels */
	for (i = first_chan; i <= last_chan; i++) {
		chan = &de->ppc8641_dma_chans[i];
		if (chan->state != DMA_UNINIT)
			dma_set_reg(chan, DMA_MR, chan->op_mode | DMA_MODE_ABORT);
	}
	
	/* Stop the completion queue */
	if (de->callback_thread && !IS_ERR(de->callback_thread))
		kthread_stop(de->callback_thread);
	if (event_out)
		kfree (event_out);
	if (event_in)
		kfree (event_in);
	if (event_array)
		kfree (event_array);

	rddma_dma_unregister("rddma_rio_dma");
	iounmap(de->regbase);
	kfree(de);
}

static int __devinit mpc85xx_rddma_probe (struct platform_device *pdev)
{
	int status;
	status = setup_rddma_channel (pdev);
	return (status);
}

static int __devexit mpc85xx_rddma_remove (struct platform_device *pdev)
{
	printk("PIGGY! PIGGY!\n");
	printk("start = 0x%x\n", pdev->resource[0].start);
	return 0;
}

module_init(dma_rio_init);
module_exit(dma_rio_close);

MODULE_PARM_DESC(first_chan, "First DMA channel, default=0");
MODULE_PARM_DESC(last_chan, "Last DMA channel, default=3");
MODULE_PARM_DESC(nevents, "Number of DMA completion events, default=100");
module_param(first_chan, int, 0);
module_param(last_chan, int, 0);
module_param(nevents, int, 0);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phil Terry <pterry@micromemory.com>");
MODULE_DESCRIPTION("DMA Engine for RDDMA");
