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

#ifndef VFI_DMA_NET_H
#define VFI_DMA_NET_H

#include <linux/vfi.h>

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
#define DMA_EOL END_OF_LLIST
#define DMA_END_OF_CHAIN DMA_EOL


#define to_xfer_object(lh) ((lh) ? list_entry((lh), struct my_xfer_object, xf.node) : 0)

#define to_sdesc(sh) ((sh) ? list_entry((sh), struct seg_desc, node) : 0)


#endif /* VFI_DMA_NET_H  */
