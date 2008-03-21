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
#include <linux/vfi_src.h>
#include <linux/vfi_srcs.h>
#include <linux/vfi_binds.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <asm/io.h>
#include "./mpc10xdma.h"
#include "./ringbuf.h"


#define LOCAL_DMA_ADDRESS_TEST

static char *dma_name = "ppc8245";

struct dma_engine {
	struct vfi_dma_engine rde;
	struct completion dma_callback_sem;
	struct ppc_dma_chan mpc10x_dma_chans[MPC10X_DMA_NCHANS];
	struct task_struct *callback_thread;
	int nchans;
	int next_channel;
};

static struct proc_dir_entry *proc_dev_dir;
extern struct proc_dir_entry *proc_root_vfi;

void address_test_completion (struct vfi_dma_descriptor *d);

static struct dma_engine *de;

static inline struct dma_engine *to_dma_engine(struct vfi_dma_engine *rde)
{
	return rde ? container_of(rde, struct dma_engine, rde) : NULL;
}

static void dma_6460_queue_transfer(struct vfi_dma_descriptor *list);

/* Default module param values */
static int first_chan = 0;
static int last_chan = MPC10X_DMA_NCHANS - 1;

#define NEVENTS 100
static int nevents = NEVENTS;

static RINGBUF ering1;
static RINGBUF ering2;
static RINGBUF *event_ring_out = &ering1;
static RINGBUF *event_ring_in = &ering2;
static struct ppc_dma_event *event_array;
static struct ppc_dma_event **event_in;
static struct ppc_dma_event **event_out;

static struct vfi_dma_ops dma_6460_ops;

static inline void dma_set_reg(struct ppc_dma_chan *chan,
			       unsigned int offset, u32 value)
{
	out_le32(chan->regbase + offset, value);
}

static inline u32 dma_get_reg(struct ppc_dma_chan *chan,
			      unsigned int offset)
{
	return in_le32(chan->regbase + offset);
}

static inline dma_addr_t sdesc_virt_to_phys(struct seg_desc *va)
{
#ifdef PADDR_NOT_IN_DESC
	return (dma_addr_t) ((u32) pdev->sdescs_phys +
			     ((u32) va - (u32) pdev->sdescs));
#else
	return (u32) va->paddr;
#endif
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
		pevent =
		    (struct ppc_dma_event *) ringbuf_get(event_ring_out);
		while (pevent) {
			chan = &de->mpc10x_dma_chans[pevent->chan_num];
			printk("DMA completion event on channel %d\n", chan->num);
			xfo = pevent->desc;
			/* Jimmy, take semaphore here */
			chan->bytes_queued -= xfo->xf.len;
			xfo->xf.flags = pevent->status;
			/* Jimmy, give semaphore here */
			if (xfo->xf.cb) {
				xfo->xf.cb((struct vfi_dma_descriptor *) xfo);
			}
			ringbuf_put(event_ring_in, (void *) pevent);
			pevent = (struct ppc_dma_event *)
			    ringbuf_get(event_ring_out);
		}
	}
}

static struct dma_engine *new_dma_engine(void)
{
	struct dma_engine *new;
	if ( (new = kzalloc(sizeof(struct dma_engine),GFP_KERNEL)) ) {
		new->rde.owner = THIS_MODULE;
		new->rde.ops = &dma_6460_ops;
	}
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,new);
	return new;
}

static void dma_6460_load(struct vfi_src *src)
{
	int i;
	int *p;
	struct seg_desc *rio = (struct seg_desc *)&src->descriptor;
	rio->paddr = (u64) __pa(rio);
	writel(__pa(src->desc.src.offset & 0xffffffff), &rio->hw.saddr);
#ifdef LOCAL_DMA_ADDRESS_TEST
	/* Jimmy...Address test:  write address into source array */
	p = (int *) ((int) src->desc.src.offset);
	for (i = 0; i < src->desc.src.extent/4; i++)
		*p++ = (unsigned int) p;
#endif
	writel(0, &rio->hw.saddr_ext);
	writel(__pa(src->desc.dst.offset & 0xffffffff), &rio->hw.daddr);
	writel(0, &rio->hw.daddr_ext);
	writel(src->desc.src.extent, &rio->hw.nbytes);  
	writel(0, &rio->hw.next_ext);
	writel(DMA_END_OF_CHAIN | DMA_BUS_LOCAL_MEM, &rio->hw.next);
}

static void dma_6460_link_src(struct list_head *first, struct vfi_src *second)
{
	struct seg_desc *rio2 = (struct seg_desc *)&second->descriptor;
	struct seg_desc *riolast; 
	unsigned int val;
	if (!list_empty(first)) {
		riolast = to_sdesc(first->prev);
		val = readl(&riolast->hw.next);
		val &= ~DMA_END_OF_CHAIN;
		val |= (rio2->paddr & 0xffffffe0);
		writel(val, &riolast->hw.next);
	}
	list_add_tail(&rio2->node, first);
}

static void dma_6460_link_dst(struct list_head *first, struct vfi_dst *second)
{
	struct seg_desc *rio2;
	struct seg_desc *riolast;
	unsigned int val;
	if (!list_empty(first)) {
		riolast = to_sdesc(first->prev);
		rio2 = to_sdesc(second->srcs->dma_chain.next);
		val = readl(&riolast->hw.next);
		val &= ~DMA_END_OF_CHAIN;
		val |= (rio2->paddr & 0xffffffe0);
		writel(val, &riolast->hw.next);
	}
	list_splice(&second->srcs->dma_chain, first->prev);
}

/* Bind doesn't need to be "linked"... Each bind is queued individually
 * Fill out its "transfer object".
 */
static void dma_6460_link_bind(struct list_head *first, struct vfi_bind *second)
{
	/* Hack for now!  Use link_bind to fill out a "transfer object" */
	struct my_xfer_object *xfo = (struct my_xfer_object *) &second->descriptor;
#ifdef LOCAL_DMA_ADDRESS_TEST
	xfo->xf.cb = address_test_completion;
#else
	xfo->xf.cb = vfi_dma_complete;
#endif
	xfo->xf.flags = VFI_BIND_READY;
	xfo->xf.len = second->desc.src.extent;
	xfo->desc = to_sdesc(second->dma_chain.next);
printk("Descriptor address in transfer object = 0x%x\n", (unsigned int) xfo->desc);
}

static struct vfi_dma_engine *dma_6460_get(struct vfi_dma_engine *rde)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,rde);
	return rde;
}

static void dma_6460_put(struct vfi_dma_engine *rde)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,rde);
}

static struct vfi_dma_ops dma_6460_ops = {
	.load      = dma_6460_load,
	.link_src  = dma_6460_link_src,
	.link_dst  = dma_6460_link_dst,
	.link_bind = dma_6460_link_bind,
	.queue_transfer = dma_6460_queue_transfer,
	.get       = dma_6460_get,
	.put       = dma_6460_put,
};

static void start_dma(struct ppc_dma_chan *chan, struct seg_desc *dma_desc)
{
	VFI_DEBUG(MY_DEBUG,"%s GO!! Desc @ = %p(va), %p(pa)\n", __FUNCTION__, dma_desc, 
		sdesc_virt_to_phys(dma_desc));
	chan->state = DMA_RUNNING;
#if 0
printk("SET DMA_CDAR, va = 0x%x\n",(unsigned int) dma_desc);
printk("              pa = 0x%x\n", sdesc_virt_to_phys(dma_desc));
#endif
	dma_set_reg(chan, DMA_CDAR, sdesc_virt_to_phys(dma_desc));
	dma_set_reg(chan, DMA_MR, chan->op_mode);
#if 0
printk("DMA_MR, GO!\n");
#endif
	dma_set_reg(chan, DMA_MR, chan->op_mode | DMA_MODE_START);

	return;
}

void address_test_completion (struct vfi_dma_descriptor *dma_desc)
{
	struct my_xfer_object *xfo = (struct my_xfer_object *) dma_desc;
	struct seg_desc *sdesc;
	struct vfi_bind *bind;
	struct vfi_xfer *xfer;
	struct list_head *entry;
	int *p;
	int *p2;
	int i;
	int len;
	printk("DMA complete, status = 0x%x\n", xfo->xf.flags);
	/* Address test, make sure all vals in destination equal corresponding
	 * source addresses
	 */
	sdesc = xfo->desc;
loop:
	p = phys_to_virt(readl(&sdesc->hw.daddr));
	p2 = phys_to_virt(readl(&sdesc->hw.saddr));
	len = (readl(&sdesc->hw.nbytes))/4;
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
	p2 = readl(&sdesc->hw.next);
	if (((unsigned int) p2 & 0x1) == 0) {
		p2 = (int*) ((unsigned int) p2 & 0xffffffe0);
		sdesc = phys_to_virt((unsigned int) p2);
		printk("segment ok, next desc at va 0x%x\n", (unsigned int) sdesc);
		goto loop;
	}

#if 0
	if (xfo->xf.cb == NULL)
		printk("DMA complete, status = 0x%x\n", status);
#endif

err:
	vfi_dma_complete ((struct vfi_bind *) dma_desc );
}

void send_completion (struct ppc_dma_chan *chan, struct my_xfer_object *xfo,
	int status)
{
	struct ppc_dma_event *event;

#if 0
	if (desc->flags & DMA_NO_CALLBACK)
		return;
#endif

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
		 * activity is a segment interrupt. Just ACK and return.
		 */
		if (status & DMA_STAT_SEGMENT_INTERRUPT)
			chan->seg_int++;
		else
			chan->bogus_int++;

		return (IRQ_HANDLED);
	}

	te = status & (DMA_STAT_PCI_ERR | DMA_STAT_MEM_ERR);
	if (te) {
		chan->err_int++;
		chan->state = DMA_ERR_STOP;
	}
	else
		chan->state = DMA_IDLE;
printk("DMA interrupt, chan->state = 0x%x\n", chan->state);

	if (status & DMA_STAT_SEGMENT_INTERRUPT)
		chan->seg_int++;
	if (status & DMA_STAT_CHAIN_INTERRUPT)
		chan->chain_int++;

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

	if (chan->state == DMA_ERROR || chan->state == DMA_ERR_STOP) {
		send_completion(chan, pdesc, DMA_ERROR);
	} else {
		chan->bytes_tx += pdesc->xf.len;
		send_completion(chan, pdesc, DMA_OK);
		if (!list_empty(&chan->dma_q)) {
			pdesc = to_xfer_object(chan->dma_q.next);
			pdesc->xf.flags = VFI_BIND_DMA_RUNNING;
			start_dma(chan, pdesc->desc);
		}
	}
	return (IRQ_HANDLED);
}

/* This is where we'll do channel load balancing.
 * For now it's shortest queue round robin.
 */
static struct ppc_dma_chan *load_balance(struct my_xfer_object *xfo)
{
	struct ppc_dma_chan *chan;
	int avail = de->next_channel;
	int i;
	int queue_len;

	/* Candidate is 'next_channel' */
	avail = de->next_channel;
	chan = &de->mpc10x_dma_chans[avail];

	/* Select shortest queue */
	queue_len = chan->bytes_queued;
	for (i = first_chan; i <= last_chan; i++) {
		if (de->mpc10x_dma_chans[i].bytes_queued < queue_len) {
			chan = &de->mpc10x_dma_chans[i];
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

int ppcdma_queue_chain(struct ppc_dma_chan *chan, struct my_xfer_object *xfo)
{
	unsigned long flags;
	struct seg_desc *seg = xfo->desc;

#ifdef COHERENCY_OFF
	flush_desc((void *) seg);
#endif
	spin_lock_irqsave(&chan->queuelock, flags);
	if (chan->state != DMA_IDLE && chan->state != DMA_RUNNING) {
		printk("ppcdma_queue_chain Error in chan state\n");
		/* channel in error state, don't queue this node */
		spin_unlock_irqrestore(&chan->queuelock, flags);
		return (-EAGAIN);
	}

	/* Spool the DMA chain */
	list_add_tail(&xfo->xf.node, &chan->dma_q);
	xfo->xf.flags = VFI_BIND_QUEUED;

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
	        xfo->xf.flags = VFI_BIND_DMA_RUNNING;
		start_dma(chan, seg);
	}
	spin_unlock_irqrestore(&chan->queuelock, flags);
	return 0;
}

static void dma_6460_queue_transfer(struct vfi_dma_descriptor *list)
{
	struct ppc_dma_chan *chan;
	struct my_xfer_object *xfo = (struct my_xfer_object *) list;
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n", __FUNCTION__, xfo);
	if (de->nchans == 1) {
		ppcdma_queue_chain(&de->mpc10x_dma_chans[first_chan], xfo);
		return;
	}
	chan = load_balance(xfo);
	ppcdma_queue_chain(chan, xfo);
}

static int proc_dump_dma_stats(char *buf, char **start, off_t offset,
	int count, int *eof, void *data)
{
	int len = 0;
	int index = (int) data;
	struct ppc_dma_chan *chan = &de->mpc10x_dma_chans[index];

	len += sprintf(buf + len, " Number of DMA chain complete interrupts = %d\n", 
			chan->chain_int);
	len += sprintf(buf + len, " Number of bytes transmitted = %lld, (0x%llx)\n",
			chan->bytes_tx, chan->bytes_tx);
	len += sprintf(buf + len, " Number of unknown interrupts = %d\n", 
			chan->bogus_int);
	len += sprintf(buf + len, " Number of errors = %d\n", 
			chan->err_int);
	return len;
}

static int setup_vfi_channel(struct platform_device *pdev)
{
	int index;
	int err;
	struct ppc_dma_chan *chan;
	struct resource *res;

	index = pdev->id;
	if (index < first_chan || index < 0)
		return -ELNRNG;

	if (index > last_chan || index > MPC10X_DMA_NCHANS)
		return -ELNRNG;

	chan = &de->mpc10x_dma_chans[index];
#if 0 /* Jimmy, do we need this?? */
	chan->device = de;
#endif
	res = platform_get_resource (pdev, IORESOURCE_MEM, 0);
	BUG_ON(!res);
	chan->regbase = ioremap_nocache(res->start, res->end - res->start + 1);
	chan->xfercap = DMA_MAX_XFER;
	chan->num = index;

	/* Using chaining mode, enable end-of-chain interrupt */
	chan->op_mode = DMA_MODE_PCI_DELAY2;  /* Let's hog the bus! */
	chan->op_mode |= DMA_MODE_ERR_INT_EN;
	chan->op_mode |= DMA_MODE_CHAIN_INT_EN;
	dma_set_reg(chan, DMA_MR, chan->op_mode);

	/* Using 32-bit descriptors and addresses for now */
	dma_set_reg(chan, DMA_HCDAR, 0);
	dma_set_reg(chan, DMA_HSAR, 0);
	dma_set_reg(chan, DMA_HDAR, 0);
	dma_set_reg(chan, DMA_HNDAR, 0);

	sprintf(chan->name, "fsl-dma.%d", index);
	res = platform_get_resource (pdev, IORESOURCE_IRQ, 0);
	BUG_ON(!res);
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
	chan->proc_dir = proc_mkdir(chan->name,proc_dev_dir);
	chan->proc_entry = create_proc_read_entry("stats", 0, chan->proc_dir,
			proc_dump_dma_stats, (void *) chan->num);

#else
	chan->proc_dir = NULL;
#endif
	return 0;
      err_irq:
	chan->state = DMA_UNINIT;
	iounmap (chan->regbase);
	return (err);
}

static int __devinit mpc10x_vfi_probe(struct platform_device *pdev);
static int __devinit mpc10x_vfi_remove(struct platform_device *pdev);

static struct platform_driver mpc10x_vfi_driver = {
	.probe = mpc10x_vfi_probe,
	.remove = mpc10x_vfi_remove,
	.driver = {
		.name = "fsl-dma",
		.owner = THIS_MODULE,
	},
};

static int __init dma_6460_init(void)
{
	struct vfi_dma_engine *rde;
	int err;
	int i;

printk("IN dma_6460_init!\n");
	if ( (de = new_dma_engine()) ) {
		rde = &de->rde;
		de->next_channel = first_chan;
		snprintf(rde->name, VFI_MAX_DMA_NAME_LEN, "%s", dma_name);
	}
	else
		return -ENOMEM;

#ifdef CONFIG_PROC_FS
	if (!proc_root_vfi) 
		proc_root_vfi = proc_mkdir ("vfi", proc_root_driver);

	proc_dev_dir = proc_mkdir (dma_name, proc_root_vfi);
#endif

#if 1
	platform_driver_register(&mpc10x_vfi_driver);
#endif

	/* Before calling register, make sure probe succeeded */

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
		/* goto err_event_in; */
	}
	event_out = kmalloc((nevents + 1) * sizeof(void *), GFP_KERNEL);
	if (!event_out) {
		err = -ENOMEM;
		/* goto err_event_out; */
	}

	for (i = 0; i < nevents; i++)
		event_in[i] = (void *) &event_array[i];

	ringbuf_init(event_ring_out, (void **) event_out, nevents + 1,
		     RING_EMPTY);
	ringbuf_init(event_ring_in, (void **) event_in, nevents + 1,
		     RING_FULL);

	/* returns a task_struct  */
	de->callback_thread = kthread_create(dma_completion_thread, NULL,
	       "DMA completion");

	if (IS_ERR(de->callback_thread)) {
		err = -ENOMEM;
	}
	/* Start up completion callback thread */
	wake_up_process(de->callback_thread);
	/* end completion callback mechanism setup */


	return vfi_dma_register(rde);
}

static void __exit dma_6460_close(void)
{
	vfi_dma_unregister(dma_name);
}

static int __devinit mpc10x_vfi_probe (struct platform_device *pdev)
{
	int status;
	status = setup_vfi_channel (pdev);
	return (status);
}

static int __devexit mpc10x_vfi_remove (struct platform_device *pdev)
{
	printk("PIGGY! PIGGY!\n");
	printk("start = 0x%x\n", pdev->resource[0].start);
	return 0;
}

module_init(dma_6460_init);
module_exit(dma_6460_close);

MODULE_PARM_DESC(first_chan, "First DMA channel, default=0");
MODULE_PARM_DESC(last_chan, "Last DMA channel, default=1");
MODULE_PARM_DESC(nevents, "Number of DMA completion events, default=100");
module_param(first_chan, int, 0);
module_param(last_chan, int, 0);
module_param(nevents, int, 0);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jimmy Blair <jblair@vmetro.com>");
MODULE_DESCRIPTION("DMA Engine for local VFI on PPC8245");
