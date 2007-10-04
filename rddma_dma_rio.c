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
#include <linux/rddma_binds.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <asm/io.h>
#include "./ringbuf.h"

struct dma_engine {
	struct rddma_dma_engine rde;
	struct completion dma_callback_sem;
	struct ppc_dma_chan ppc8641_dma_chans[PPC8641_DMA_NCHANS];
	struct task_struct *callback_thread;
	int nchans;
	int next_channel;
};

static struct dma_engine *de;
static struct proc_dir_entry *proc_dev_dir;
static struct proc_dir_entry *proc_root_rddma;

/* Default module param values */
static int first_chan = 0;
static int last_chan = PPC8641_DMA_NCHANS - 1;

#define NEVENTS 100
static int nevents = NEVENTS;

static RINGBUF ering1;
static RINGBUF ering2;
static RINGBUF *event_ring_out = &ering1;
static RINGBUF *event_ring_in = &ering2;
static struct ppc_dma_event *event_array;
static struct ppc_dma_event **event_in;
static struct ppc_dma_event **event_out;

static inline struct dma_engine *to_dma_engine(struct rddma_dma_engine *rde)
{
	return rde ? container_of(rde, struct dma_engine, rde) : NULL;
}

static struct rddma_dma_ops dma_rio_ops;
static void dma_rio_queue_transfer;

static inline void dma_set_reg(struct ppc_dma_chan *chan,
			       unsigned int offset, u32 value)
{
	out_be32(chan->regbase + offset, value);
}

static inline u32 dma_get_reg(struct ppc_dma_chan *chan,
			      unsigned int offset)
{
	return in_be32(chan->regbase + offset);
}

static inline dma_addr_t ldesc_virt_to_phys(struct list_desc *va)
{
	return (u32) va->paddr;
}

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
			xfo->xf.rc = pevent->status;
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

#define HIGH_LOCAL_ADDR_MASK 0xf
#define HIGH_RIO_ADDR_MASK 0x3

/* Next 3 functions are stubs */
static int rddma_is_local(struct rddma_desc_param *src)
{
	return 1;
}
static int rddma_is_bypass_atmu(struct rddma_desc_param *src)
{
	return 0;
}
static int rddma_rio_id(struct rddma_desc_param *src)
{
	return 0;
}
static int rio_to_ocn(int rio_id, struct rddma_desc_param *src)
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
		/* Stash upper 4 bits of 36-bit OCN address and
		 * turn on snooping (for now)
		 */
		phys = virt_to_phys(src->desc.src.offset);
		rio->hw.saddr = phys & 0xffffffff;
		rio->hw.src_attr = (phys >> 32) & HIGH_LOCAL_ADDR_MASK;
		rio->hw.src_attr |= DMA_ATTR_LOCAL_SNOOP;
	}
	else if (rddma_is_bypass_atmu(&src->desc.src)) {
		/* Stash upper 2 bits of 34-bit RIO address and
		 * turn off snooping.  This is OK for "bypass ATMU" case.
		 */
		phys = src->desc.src.offset;
		rio->hw.saddr = phys & 0xffffffff;
		rio->hw.src_attr = (phys >> 32) & HIGH_RIO_ADDR_MASK;
		rio->hw.src_attr |= (DMA_ATTR_BYPASS_ATMU | DMA_ATTR_RIO | 
			DMA_ATTR_NREAD | DMA_ATTR_HI_FLO);
		rio->hw.src_attr |= DMA_ATTR_TID((rddma_rio_id(&src->desc.src));
	}
	else {
		/* Stash upper 4 bits of 36-bit OCN address and
		 * turn off snooping
		 */
		printk("Error!! ATMU mapping not supported\n");
		phys = rio_to_ocn(rddma_rio_id(&src->desc.src), 
			src->desc.src.offset);
		rio->hw.saddr = phys & 0xffffffff;
		rio->hw.src_attr = (phys >> 32) & HIGH_LOCAL_ADDR_MASK;
		rio->hw.src_attr |= DMA_ATTR_LOCAL_NOSNOOP;
	}

	if (rddma_is_local(&src->desc.dst)) {
		phys = virt_to_phys(src->desc.dst.offset);
		rio->hw.daddr = phys & 0xffffffff;
		rio->hw.dest_attr = (phys >> 32) & HIGH_LOCAL_ADDR_MASK;
		rio->hw.dest_attr |= DMA_ATTR_LOCAL_SNOOP;
	}
	else if (rddma_is_bypass_atmu(&src->desc.dst)) {
		phys = src->desc.dst.offset; /* Assume phys is 34-bit RIO addr */
		rio->hw.daddr = phys & 0xffffffff;
		rio->hw.dest_attr = (phys >> 32) & HIGH_RIO_ADDR_MASK;
		rio->hw.dest_attr |= DMA_ATTR_LOCAL_NOSNOOP;
		rio->hw.dest_attr |= (DMA_ATTR_BYPASS_ATMU | DMA_ATTR_RIO | 
			DMA_ATTR_NWRITE | DMA_ATTR_HI_FLO);
		rio->hw.dest_attr |= DMA_ATTR_TID(rddma_rio_id(&src->desc.dst));
	}
	else {
		printk("Error!! ATMU mapping not supported\n");
		phys = rio_to_ocn(rddma_rio_id(&src->desc.dst), 
			src->desc.dst.offset);
		rio->hw.daddr = phys & 0xffffffff;
		rio->hw.dest_attr = (phys >> 32) & HIGH_LOCAL_ADDR_MASK;
		rio->hw.dest_attr |= DMA_ATTR_LOCAL_NOSNOOP;
	}

	rio->hw.nbytes = src->desc.src.extent; 
	rio->hw.next_ext = 0;
	rio->hw.next = DMA_END_OF_CHAIN;
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

#ifdef SERIALIZE_BIND_PROCESSING
/* Link the DMA chain in the "second" bind to the tail of "first" DMA chain */
static void dma_rio_link_bind(struct list_head *first, struct rddma_bind *second)
{
	struct seg_desc *rio2;
	struct seg_desc *riolast;
	if (!list_empty(first)) {
		riolast = to_sdesc(first->prev);
		rio2 = to_sdesc(second->dma_chain.next);
		val = riolast->hw.next;
		val &= ~DMA_END_OF_CHAIN;
		val |= rio2->paddr & 0xffffffe0;
		riolast->hw.next = val;
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
	rioend->hw.next = DMA_END_OF_CHAIN;
}

static void dma_rio_load_transfer(struct rddma_xfer *xfer)
{
	struct my_xfer_object *xfo = (struct my_xfer_object *) &xfer->descriptor;
	/* Fill out a "transfer object" */
#ifdef LOCAL_DMA_ADDRESS_TEST
	xfo->xf.cb = address_test_completion;
#else
	xfo->xf.cb = NULL;
#endif
	xfo->xf.flags = RDDMA_XFER_READY;
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
#endif

#ifdef PARALLELIZE_BIND_PROCESSING
/* Bind doesn't need to be "linked"... Each bind is queued individually
 * Fill out its "transfer object".
 */
static void dma_rio_link_bind(struct list_head *first, struct rddma_bind *second)
{
	/* Hack for now!  Use link_bind to fill out a "transfer object" */
	struct seg_desc *seg;
	struct my_xfer_object *xfo = (struct my_xfer_object *) &second->descriptor;
#ifdef LOCAL_DMA_ADDRESS_TEST
	xfo->xf.cb = address_test_completion;
#else
	xfo->xf.cb = NULL;
#endif
	xfo->xf.flags = RDDMA_BIND_READY;
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

/* This is a no-op in parallel case since binds aren't linked together */
static void dma_rio_unlink_bind(struct list_head *first, struct rddma_bind *second)
{
	return;
}
static void dma_rio_load_transfer(struct rddma_xfer *xfer)
{
	/* Fill out a "transfer object" */
	return;
}
#endif

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
	unsigned int cb, te;
	struct list_head *desc_node;
	struct my_xfer_object *pdesc;

	status = dma_get_reg(chan, DMA_SR);
printk("DMA interrupt, status = 0x%x\n", status);
printk("CDAR =  0x%x\n", dma_get_reg(chan, DMA_CDAR));

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
#if 0
	/* Jimmy test: dump start of destination array */
	{
		int i;
		int *p = phys_to_virt(readl(&pdesc->desc->hw.daddr));
		int *p2 = phys_to_virt(readl(&pdesc->desc->hw.saddr));
		printk("src = 0x%x,  dest 0x%x\n",p2,p);
		for (i = 0; i < (readl(&pdesc->desc->hw.nbytes))/4; i++)
			printk("val %d,%d\n",*p2++, *p++);
	}
#endif
	list_del(desc_node); 	/* De-queue the completed chain */

	/* Wake up completion callback thread and launch the next DMA */
	if (chan->state == DMA_ERROR || chan->state == DMA_ERR_STOP) {
		send_completion(chan, pdesc, DMA_ERROR);
	} else {
		chan->bytes_tx += pdesc->xf.len;
		send_completion(chan, pdesc, DMA_OK);
		if (!list_empty(&chan->dma_q)) {
			pdesc = to_xfer_object(chan->dma_q.next);
#ifdef PARALLELIZE_BIND_PROCESSING
			pdesc->xf.flags = RDDMA_BIND_DMA_RUNNING;
#else
			pdesc->xf.flags = RDDMA_XFER_DMA_RUNNING;
#endif
			start_dma(chan, (struct list_desc *) pdesc);
		}
	}
	return (IRQ_HANDLED);
}
static void start_dma(struct ppc_dma_chan *chan, struct list_desc *dma_desc)
{
	chan->state = DMA_RUNNING;
	/* Extended mode, Single write start */
	dma_set_reg(chan, DMA_CLSDAR, ldesc_virt_to_phys(dma_desc));
	return;
}

int ppcdma_queue_chain(struct ppc_dma_chan *chan, struct my_xfer_object *xfo)
{
	unsigned long flags;

	/* Lock interrupts -- ISR can dequeue */
	spin_lock_irqsave(&chan->queuelock, flags);
	if (chan->state != DMA_IDLE && chan->state != DMA_RUNNING) {
		/* channel in error state, don't queue this node */
		spin_unlock_irqrestore(&chan->queuelock, flags);
		return (-EAGAIN);
	}

	/* Spool the DMA chain */
	list_add_tail(&xfo->xf.node, &chan->dma_q);
	xfo->xf.flags = RDDMA_BIND_QUEUED;

	/* Launch if DMA engine idle */
	if (chan->state == DMA_IDLE) {
#ifdef DEBUG_DMA
		if (chan->dma_q.prev != &xfo->xf.node) {
			printk
			    ("DMA-%d error: queue not empty when channel idle",
			     chan->num);
		}
#endif
	        chan->bytes_queued += xfo->xf.len;
	        xfo->xf.flags = RDDMA_BIND_DMA_RUNNING;
		start_dma(chan, (struct list_desc *) xfo);
	}
	spin_unlock_irqrestore(&chan->queuelock, flags);
	return 0;
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
	.load_transfer = dma_rio_load_transfer;
	.queue_transfer = dma_rio_queue_transfer;
	.cancel_transfer = dma_rio_cancel_transfer;
	.get       = dma_rio_get,
	.put       = dma_rio_put,
};

static int __init dma_rio_init(void)
{
	struct dma_engine *de;
	struct rddma_dma_engine *rde;

	if ( (de = new_dma_engine()) ) {
		rde = &de->rde;
		de->next_channel = first_chan;
		snprintf(rde->name, RDDMA_MAX_DMA_NAME_LEN, "%s", "rddma_rio_dma");
	}
	else
		return -ENOMEM;

#ifdef CONFIG_PROC_FS
	if (!proc_root_rddma) 
		proc_root_rddma = proc_mkdir ("rddma", proc_root_driver);

	proc_dev_dir = proc_mkdir ("rddma_rio_dma", proc_root_rddma);
#endif

	/* 
	platform_driver_register(&mpc10x_rddma_driver);
	*/

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
err_event_array: 

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
		if (de->mpc10x_dma_chans[i].bytes_queued < queue_len) {
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
	for (i = first_chan; i <= last_chan; i++)
		chan = &de->ppc8641_dma_chans[i];
		if (chan->state != DMA_UNINIT)
			dma_set_reg(chan, DMA_MR, chan->op_mode | DMA_MODE_ABORT);
	
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
