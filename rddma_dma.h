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

struct rddma_dma_ops {
	void (*load)(struct rddma_src *);
	void (*link_src)(struct rddma_src *, struct rddma_src *);
	void (*link_dst)(struct rddma_dst *, struct rddma_dst *);
	void (*link_bind)(struct rddma_bind *, struct rddma_bind *);
};

struct rddma_dma_device_id {
	u16 did, vid;
	u16 asm_did, asm_vid;
};
struct rddma_dma_dev {
};

struct rddma_dma_driver {
	struct list_head node;
	char *name;
	const struct rddma_dma_device_id *id_table;
	int (*probe) (struct rddma_dma_dev * dev, const struct rddma_dma_device_id * id);
	void (*remove) (struct rddma_dma_dev * dev);
	int (*suspend) (struct rddma_dma_dev * dev, u32 state);
	int (*resume) (struct rddma_dma_dev * dev);
	int (*enable_wake) (struct rddma_dma_dev * dev, u32 state, int enable);
	struct device_driver driver;
};

extern void rddma_dealloc_pages( struct page **pages, int num_pages);
extern int rddma_alloc_pages( size_t size, struct page **pages[], int *num_pages);

#endif
