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
#include <linux/rddma_dma_rio.h>

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

struct rddma_dma_descriptor {
	u32 words[16];
} __attribute__((aligned(32)));

struct rddma_src;
struct rddma_dst;
struct rddma_bind;
struct rddma_dma_engine;

struct rddma_dma_ops {
	void (*load)(struct rddma_src *);
	void (*link_src)(struct list_head *, struct rddma_src *);
	void (*link_dst)(struct list_head *, struct rddma_dst *);
	void (*link_bind)(struct rddma_bind *, struct rddma_bind *);
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
