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

#ifndef RDDMA_DMA_RIO_H
#define RDDMA_DMA_RIO_H

#include <linux/rddma.h>

/* Memory format of DMA list descriptor 
 * (must be 32-byte aligned)
 * A list is a DMA chain, consisting of 1 or more segments (links)
 */
struct dma_list {
	u32 next_ext;
	u32 next;
	u32 link_ext;
	u32 link;
	u32 src_stride;
	u32 dest_stride;
	u32 dummy;
	u32 dummy2;
};

/*  Note:  The last 8 bytes of the H/W descriptor are reserved,
 *  so we could move "struct list_head" there and use a full
 *  32 bytes of user area if necessary.
 */
struct list_desc {
	struct dma_list hw;	/* 32 bytes, h/w descriptor */
	struct list_head node;  /* 8 bytes */
	u64 paddr;              /* 8 bytes */
	u32 flags; 		/* 4 extra bytes */
	u32 user1;		/* 12 extra bytes */
	u32 user2;
	u32 user3;
};

/* Memory format of DMA link descriptor 
 * (must be 32-byte aligned) 
 */
struct dma_link {
	u32 src_attr;
	u32 saddr;
	u32 dest_attr;
	u32 daddr;
	u32 next_ext;
	u32 next;
	u32 nbytes;
	int dummy;
};

struct seg_desc {
	struct dma_link hw;	/* 32 bytes, h/w descriptor */
	struct list_head node;  /* 8 bytes */
	u64 paddr;              /* 8 bytes */
	u32 user1;		/* 16 extra bytes */
	u32 user2;
	u32 user3;
	u32 user4;
};

#define to_ldesc(lh) ((lh) ? list_entry((lh), struct list_desc, node) : 0)
#define to_sdesc(sh) ((sh) ? list_entry((sh), struct seg_desc, node) : 0)
struct rddma_src;
extern struct rddma_dma_ops rddma_dma_ops_rio;

#endif /* RDDMA_DMA_RIO_H  */
