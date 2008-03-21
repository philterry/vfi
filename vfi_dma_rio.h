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

#ifndef VFI_DMA_RIO_H
#define VFI_DMA_RIO_H

#include <linux/vfi.h>

#define MPC86XX_DMA_OFFSET 0x21000
#define MPC86XX_DMA_REG_SIZE 0x1000
#define DMA_CHANNEL_ADDR(chan) (MPC86XX_DMA_OFFSET + 0x100 + 0x80 * (chan))
#define PPC8641_DMA_NCHANS 4
#define DMA_INTERNAL_INT_NUMBER 4     /* First channel, others consecutive */

/*
 * Assign bandwidth limit for priority level.  (Put low at 256 because
 * that's a good size for rapidio.)  Round robin weighting is determined
 * by how many channels are allocated and how each channel sets its
 * priority.
 */
#define DMA_LOW_PRIORITY 0x8	/* 256 bytes */
#define DMA_MED_PRIORITY 0x9 	/* 512 bytes */
#define DMA_HIGH_PRIORITY 0xA	/* 1024 bytes */
#define DMA_PRIORITY_MASK 0x0f000000

/* Per-channel DMA registers */
#define DMA_MR 0x000
#define DMA_SR 0x004
#define DMA_ECLNDAR 0x008
#define DMA_CLNDAR 0x00c
#define DMA_SATR 0x010
#define DMA_SAR 0x014
#define DMA_DATR 0x018
#define DMA_DAR 0x01c
#define DMA_BCR 0x020
#define DMA_ENLNDAR 0x024
#define DMA_NLNDAR 0x028
#define DMA_ECLSDAR 0x030
#define DMA_CLSDAR 0x034
#define DMA_ENLSDAR 0x038
#define DMA_NLSDAR 0x03c
#define DMA_SSR 0x040
#define DMA_DSR 0x044

/* Memory format of DMA list descriptor 
 * (must be 32-byte aligned)
 * A list is a DMA chain, consisting of 1 or more segments (links)
 */
struct dma_list {
	unsigned int next_ext;
	unsigned int next;
	unsigned int link_ext;
	unsigned int link;
	unsigned int src_stride;
	unsigned int dest_stride;
	int dummy;
	int dummy2;
};

/*  Note:  The last 8 bytes of the H/W descriptor are reserved,
 *  so we could move "struct list_head" there and use a full
 *  32 bytes of user area if necessary.
 */
struct list_desc {
	struct dma_list hw;	/* 32 bytes, h/w descriptor */
	struct list_head node;  /* 8 bytes */
	u32 paddr;              /* 4 bytes */
	u32 flags; 		/* 4 extra bytes */
	u32 user1;		/* 16 extra bytes */
	u32 user2;
	u32 user3;
	u32 user4;
};

/* Note: Descriptor linked list pointers are 36 bits, source and 
 * destination addresses are the low-order 32 bits.... What about the
 * upper 4 bits??  They're stuffed in the attributes register.
 */

/* Memory format of DMA link descriptor 
 * (must be 32-byte aligned) 
 */
struct dma_link {
	unsigned int src_attr;
	unsigned int saddr;
	unsigned int dest_attr;
	unsigned int daddr;
	unsigned int next_ext;
	unsigned int next;
	unsigned int nbytes;
	int dummy;
};

struct seg_desc {
	struct dma_link hw;	/* 32 bytes, h/w descriptor */
	struct list_head node;  /* 8 bytes */
	u64 paddr;              /* 8 bytes */
	u32 user2;		/* 16 extra bytes */
	u32 user3;
	u32 user4;
	u32 user5;
};

/* NB: must be same size as struct vfi_dma_descriptor */
struct my_xfer_object {
	struct dma_list hw;	/* 32 bytes */
	u64 paddr;		/* 8 bytes */
	struct vfi_xf xf;	/* 24 bytes VFI */
};

#define DESC_INT_FLAG 0x8
#define SEG_INT_FLAG 0x4
#define END_OF_LLIST 0x1

/* Mode Register bits 
 * Not defining them all because
 * external mastering and address holding are unsupported
 */
#define DMA_MODE_BANDWIDTH_CONTROL 0x0f000000
#define DMA_MODE_SEGMENT_INT_EN 0x200
#define DMA_MODE_CHAIN_INT_EN   0x100
#define DMA_MODE_LIST_INT_EN    0x80
#define DMA_MODE_ERR_INT_EN     0x40
#define DMA_MODE_EXTENDED       0x20
#define DMA_MODE_QUICKSTART     0x10
#define DMA_MODE_ABORT	        0x8 
#define DMA_MODE_DIRECT	        0x4 
#define DMA_MODE_CONTINUE       0x2
#define DMA_MODE_START 	        0x1

/* Status register bits
 */
#define DMA_STAT_TRANSFER_ERR     0x80
#define DMA_STAT_STOPPED          0x20
#define DMA_STAT_PROGRAM_ERR      0x10
#define DMA_STAT_CHAIN_INTERRUPT   0x8
#define DMA_STAT_CHAN_BUSY	   0x4
#define DMA_STAT_SEGMENT_INTERRUPT 0x2
#define DMA_STAT_LIST_INTERRUPT    0x1

#define DMA_STAT_CB DMA_STAT_CHAN_BUSY
#define DMA_STAT_TE DMA_STAT_TRANSFER_ERR
#define DMA_STAT_PE DMA_STAT_PROGRAM_ERR

/* Interrupt when DMA unit goes idle, i.e. at the end of every DMA list */
#define DMA_INTERRUPTS \
	(DMA_MODE_ERR_INTERRUPT | DMA_MODE_LIST_INT_EN)

#define DMA_OP_MODE \
	(DMA_MODE_EXTENDED | DMA_MODE_QUICKSTART | DMA_MODE_CHAINING)

/* Channel states */
#define DMA_UNINIT 	0
#define DMA_IDLE 	1
#define DMA_RUNNING	2
#define DMA_ERR_STOP	3
#define DMA_SHUTDOWN 	5

/* Status of completed descriptors */
#define PPCDMA_OK 0x1
#define PPCDMA_ERROR  0x2
#define DMA_CHANNEL_DOWN  0x8

#define DMA_ATTR_LOCAL_NOSNOOP  0x00040000
#define DMA_ATTR_LOCAL_SNOOP 	0x00050000
#define DMA_ATTR_BYPASS_ATMU    0x20000000
/* Attribute fields for BYPASS ATMU mode */
#define DMA_ATTR_RIO		0x00c00000
#define DMA_ATTR_HI_FLOW	0x08000000
#define DMA_ATTR_MED_FLOW	0x04000000
#define DMA_ATTR_LO_FLOW	0x00000000
#define DMA_ATTR_SWRITE		0x00030000
#define DMA_ATTR_NWRITE		0x00040000
#define DMA_ATTR_NWRITE_R	0x00050000
#define DMA_ATTR_IOREAD		0x00020000  /* nuke? */
#define DMA_ATTR_NREAD		0x00040000
#define DMA_ATTR_TID(x)		(x<<2)

#define HIGH_LOCAL_ADDR_MASK 0xf
#define HIGH_RIO_ADDR_MASK 0x3

/* Descriptor flags -- so far just 1 */
#define DMA_NO_CALLBACK 0x1

/* Descriptor bits (link descriptor, also in next link reg) */
#define DMA_SEG_INT 0x8
/* Descriptor bits (list descriptor) */
/*   none */
/* End of list bit in next list descriptor address register */
#define DMA_EOL END_OF_LLIST
#define DMA_END_OF_CHAIN DMA_EOL

#define BUS_LOCAL_MEM 0xf
#define BUS_RIO 0xc
#define BUS_PCI1 0
#define BUS_PCI2 0x1

struct target_bus {
	int type;
	int bypass_atmu;
	int tid;
};

struct ppc_dma_event {
	struct my_xfer_object *desc;
	int chan_num;
	int status;
};

struct client {
	int dummy;
};

struct dma_engine;

struct ppc_dma_chan {
	struct dma_engine *device;
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry *proc_dir;
	struct proc_dir_entry *proc_entry;
#endif
	void *regbase;
	int state;    
	unsigned int op_mode;
	int priority; /* 4 values? */
	unsigned int xfercap;
	char name[16];
	int num;
	int irq;
	int flags;
	struct list_head dma_q;  
	spinlock_t queuelock;
	/* void (*client_cb)(struct list_desc *, int);  */
	struct list_desc *null_desc;
	dma_addr_t null_desc_phys;
	spinlock_t cleanup_lock;  /* not used for now */
	int seg_int;  		 /* counters */
	int list_int;
	int chain_int;
	int bogus_int;
	int err_int;
	int errors;
	int bytes_queued;
	int jtotal;
	int jstart;
	u64 bytes_tx;
};

#define to_xfer_object(lh) ((lh) ? list_entry((lh), struct my_xfer_object, xf.node) : 0)

#define to_sdesc(sh) ((sh) ? list_entry((sh), struct seg_desc, node) : 0)
struct vfi_src;
extern struct vfi_dma_ops vfi_dma_ops_rio;

#endif /* VFI_DMA_RIO_H  */
