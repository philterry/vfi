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

#ifndef RDDMA_DMA_H
#define RDDMA_DMA_H
#include <linux/rddma.h>
#if 0 /* Jimmy hacking! */
#include <linux/rddma_dma_rio.h>
#endif

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/device.h>
/*
 * Like the rddma_fabric interface the DMA interface is layered and
 * modularized. This is both to be portable to other devices but also
 * to enable us to run in test harnesses without target hardware by
 * substituting demo code for the lower/missing layers/modules.
 *
 * rddma_dma_net runs the DMA engine over ethernet frame level.
 * rddma_dma_rio runs the DMA engine over atmubypass descriptor based
 * rio fabric
 * rddma_drv runs the DMA engine over memory. This would include
 * PCI-Express based ATMU memory as well as srio based ATMU memory.
 * We might put a minimal rddma_dma_mem layer in to represent this so
 * we could also run without DMA hardware (processor copy loops
 * simulation).
 *
 * This file specifies the upper layer interface of the DMA to the
 * rest of the rddma driver. The lower interfaces of this layer are in
 * the respective rddma_dma_{mem rio net}.h files.
 */

#define RDDMA_XFER_UNINIT 0
#define RDDMA_XFER_BINDING 1
#define RDDMA_XFER_READY 2
#define RDDMA_XFER_QUEUED 3
#define RDDMA_XFER_BUSY 4
#define RDDMA_XFER_DONE 5

#define RDDMA_BIND_UNINIT 0
#define RDDMA_BIND_BINDING 1
#define RDDMA_BIND_READY 2
#define RDDMA_BIND_QUEUED 3
#define RDDMA_BIND_BUSY 4
#define RDDMA_BIND_DMA_RUNNING 5
#define RDDMA_BIND_DMA_DONE_OK 6
#define RDDMA_BIND_DMA_DONE_ERROR 7

#define RDDMA_XFO_UNINIT 0
#define RDDMA_XFO_READY 2
#define RDDMA_XFO_QUEUED 3
#define RDDMA_XFO_RUNNING 5
#define RDDMA_XFO_COMPLETE_OK 5
#define RDDMA_XFO_COMPLETE_ERROR 5
#define RDDMA_XFO_CANCELLED 5

#define RDDMA_DESC_ALIGN 32

#if 0
struct rddma_dma_descriptor {
	u32 words[8]; /* reserved for DMA h/w */
	u32 words2[2]; /* reserved for DMA h/w driver */
	u64 paddr;
	u32 flags;
	u32 state;  /* state machine for xfer tracking */
	u32 reserved[2];
} __attribute__((aligned(RDDMA_DESC_ALIGN)));
#else

struct rddma_dma_descriptor;

/* Transport-independent part of RDDMA transfer object */
struct rddma_xf {
	struct list_head node; 	     /* For queuing DMA's and completions */
	void (*cb)(struct rddma_dma_descriptor *);	/* rddma callback */
	u32 flags; /* xfer flags, rc */
	u32 len;  /* TBD */
	u32 extra;
};

#define XFO_STAT_MASK 0x000000ff
#define XFO_CHAN_MASK 0x0000ff00
#define XFO_STATUS(xfo) (xfo->xf.flags & XFO_STAT_MASK)
#define XFO_CHAN(xfo) (xfo->xf.flags & XFO_CHAN_MASK >> 8)

struct rddma_dma_descriptor {
	u32 words[8]; /* reserved for DMA engine (using 8641 "list" descriptor") */
	u32 words2[2]; /* reserved for DMA driver */
	struct rddma_xf xf;
} __attribute__((aligned(RDDMA_DESC_ALIGN)));
#endif

struct rddma_src;
struct rddma_dst;
struct rddma_bind;
struct rddma_dma_engine;
struct rddma_xfer;

struct rddma_dma_ops {
	void (*load)(struct rddma_src *);
	void (*link_src)(struct list_head *, struct rddma_src *);
	void (*link_dst)(struct list_head *, struct rddma_dst *);
	void (*link_bind)(struct list_head *, struct rddma_bind *);
	void (*unlink_bind)(struct list_head *, struct rddma_bind *);
	void (*load_transfer)(struct rddma_xfer *);
	void (*queue_transfer)(struct rddma_dma_descriptor *);
	void (*cancel_transfer)(struct rddma_dma_descriptor *);
	struct rddma_dma_chan *(*alloc_chan)(void);
	struct rddma_dma_engine *(*get)(struct rddma_dma_engine *);
	void (*put)(struct rddma_dma_engine *);
};
#define RDDMA_MAX_DMA_ENGINES  5
#define RDDMA_MAX_DMA_NAME_LEN 31

struct rddma_dma_engine {
	struct module *owner;
	struct rddma_dma_ops *ops;
	char name[RDDMA_MAX_DMA_NAME_LEN+1];
};

extern int rddma_dma_register(struct rddma_dma_engine *);
extern void rddma_dma_unregister(const char *);

extern struct rddma_dma_engine *rddma_dma_find(const char *);
extern struct rddma_dma_engine *rddma_dma_get(struct rddma_dma_engine *);
extern void rddma_dma_put(struct rddma_dma_engine *);

extern void rddma_dealloc_pages( struct page **pages, int num_pages);
extern int rddma_alloc_pages( size_t size, int offset, struct page **pages[], int *num_pages);

#endif
