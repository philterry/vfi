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

#ifndef RDDMA_SMB_H
#define RDDMA_SMB_H

#include <linux/rddma.h>
#include <linux/rddma_location.h>
#include <linux/rddma_parse.h>
#include <linux/rddma_dsts.h>
#include <linux/rddma_srcs.h>

#define CLEAN_START(b,d) ((((b)->offset +(d)->offset) & PAGE_MASK) ? 1 : 0)
#define CLEAN_END(b,d) ((((b)->offset + (d)->offset + (d)->extent) & PAGE_MASK) ? 1 : 0)
#define CLEAN_SHIFT(b,d) (CLEAN_START((b),(d)) ? 0 : 1)
#define NUM_DMA(b,d) ((((d)->extent >> PAGE_SHIFT) << CLEAN_SHIFT((b),(d))) \
		      + CLEAN_START((b),(d)) + CLEAN_END((b),(d)))

#define START_PAGE(b,d) (((b)->offset + (d)->offset)>>PAGE_SHIFT)
#define START_OFFSET(b,d) (((b)->offset + (d)->offset) & PAGE_MASK)
#define START_SIZE(b,d) (PAGE_SIZE - START_OFFSET((b),(d)))

#define DESC_VALID(b,d) (((d)->offset + (d)->extent) < (b)->extent)

struct rddma_smb {
	struct rddma_desc_param desc;
	size_t size;
	
	struct page **pages;
	int num_pages;

	struct kobject kobj;
};

static inline struct rddma_smb *to_rddma_smb(struct kobject *kobj)
{
    return kobj ? container_of(kobj, struct rddma_smb, kobj) : NULL;
}

static inline struct rddma_smb *rddma_smb_get(struct rddma_smb *rddma_smb)
{
    return to_rddma_smb(kobject_get(&rddma_smb->kobj));
}

static inline void rddma_smb_put(struct rddma_smb *rddma_smb)
{
	if (rddma_smb) kobject_put(&rddma_smb->kobj);
}

extern struct rddma_smb *new_rddma_smb(struct rddma_location *, struct rddma_desc_param *);
extern int rddma_smb_register(struct rddma_smb *);
extern void rddma_smb_unregister(struct rddma_smb *);
extern struct rddma_smb *find_rddma_smb(struct rddma_desc_param *);
extern struct rddma_smb *rddma_smb_create(struct rddma_location *, struct rddma_desc_param *);
extern void rddma_smb_delete(struct rddma_smb *);
extern struct kobj_type rddma_smb_type;
#endif /* RDDMA_SMB_H */
