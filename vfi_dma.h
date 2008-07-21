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

#ifndef VFI_DMA_H
#define VFI_DMA_H
#include <linux/vfi.h>

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/device.h>
/*
 * Like the vfi_fabric interface the DMA interface is layered and
 * modularized. This is both to be portable to other devices but also
 * to enable us to run in test harnesses without target hardware by
 * substituting demo code for the lower/missing layers/modules.
 *
 * vfi_dma_net runs the DMA engine over ethernet frame level.
 * vfi_dma_rio runs the DMA engine over atmubypass descriptor based
 * rio fabric
 * vfi_drv runs the DMA engine over memory. This would include
 * PCI-Express based ATMU memory as well as srio based ATMU memory.
 * We might put a minimal vfi_dma_mem layer in to represent this so
 * we could also run without DMA hardware (processor copy loops
 * simulation).
 *
 * This file specifies the upper layer interface of the DMA to the
 * rest of the vfi driver. The lower interfaces of this layer are in
 * the respective vfi_dma_{mem rio net}.h files.
 */

#define VFI_XFER_UNINIT 0
#define VFI_XFER_BINDING 1
#define VFI_XFER_READY 2
#define VFI_XFER_QUEUED 3
#define VFI_XFER_BUSY 4
#define VFI_XFER_DONE 5

#define VFI_BIND_UNINIT 0
#define VFI_BIND_BINDING 1
#define VFI_BIND_READY 2
#define VFI_BIND_QUEUED 3
#define VFI_BIND_BUSY 4
#define VFI_BIND_DMA_RUNNING 5
#define VFI_BIND_DMA_DONE_OK 6
#define VFI_BIND_DMA_DONE_ERROR 7

#define VFI_XFO_UNINIT 0
#define VFI_XFO_READY 2
#define VFI_XFO_QUEUED 3
#define VFI_XFO_RUNNING 5
#define VFI_XFO_COMPLETE_OK 6
#define VFI_XFO_COMPLETE_ERROR 7
#define VFI_XFO_CANCELLED 8

#define VFI_DESC_ALIGN 32

#if 0
struct vfi_dma_descriptor {
	u32 words[8]; /* reserved for DMA h/w */
	u32 words2[2]; /* reserved for DMA h/w driver */
	u64 paddr;
	u32 flags;
	u32 state;  /* state machine for xfer tracking */
	u32 reserved[2];
} __attribute__((aligned(VFI_DESC_ALIGN)));
#else

struct vfi_dma_descriptor;

/* Transport-independent part of VFI transfer object */
struct vfi_xf {
	struct list_head node; 	     /* For queuing DMA's and completions */
	void (*cb)(struct vfi_dma_descriptor *);	/* vfi callback */
	u32 flags; /* xfer flags, rc */
	u32 len;  /* TBD */
	u32 extra;
};

#define XFO_STAT_MASK 0x000000ff
#define XFO_CHAN_MASK 0x0000ff00
#define XFO_STATUS(xfo) (xfo->xf.flags & XFO_STAT_MASK)
#define XFO_CHAN(xfo) (xfo->xf.flags & XFO_CHAN_MASK >> 8)

struct vfi_dma_descriptor {
	u32 words[8]; /* reserved for DMA engine (using 8641 "list" descriptor") */
	u32 words2[2]; /* reserved for DMA driver */
	struct vfi_xf xf;
} __attribute__((aligned(VFI_DESC_ALIGN)));
#endif

struct vfi_src;
struct vfi_dst;
struct vfi_bind;
struct vfi_dma_engine;
struct vfi_xfer;

struct vfi_dma_ops {
	void (*load)(struct vfi_src *);
	void (*link_src)(struct list_head *, struct vfi_src *);
	void (*link_dst)(struct list_head *, struct vfi_dst *);
	void (*link_bind)(struct list_head *, struct vfi_bind *);
	void (*queue_transfer)(struct vfi_dma_descriptor *);
	void (*cancel_transfer)(struct vfi_dma_descriptor *);
	struct vfi_dma_chan *(*alloc_chan)(void);
	struct vfi_dma_engine *(*get)(struct vfi_dma_engine *);
	void (*put)(struct vfi_dma_engine *);
};
#define VFI_MAX_DMA_ENGINES  5
#define VFI_MAX_DMA_NAME_LEN 31

struct vfi_dma_engine {
	struct module *owner;
	struct vfi_dma_ops *ops;
	char name[VFI_MAX_DMA_NAME_LEN+1];
};

extern int vfi_dma_register(struct vfi_dma_engine *);
extern void vfi_dma_unregister(const char *);

extern struct vfi_dma_engine *vfi_dma_find(const char *);
extern struct vfi_dma_engine *vfi_dma_get(struct vfi_dma_engine *);
extern void vfi_dma_put(struct vfi_dma_engine *);

struct vfi_smb;
extern void vfi_dealloc_pages( struct vfi_smb *);
extern int vfi_alloc_pages(struct vfi_smb *);

extern void vfi_dma_complete(struct vfi_bind *);
#endif
