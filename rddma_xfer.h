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

#ifndef RDDMA_XFER_H
#define RDDMA_XFER_H

#include <linux/rddma.h>
#include <linux/rddma_dma.h>
#include <linux/rddma_parse.h>
#include <linux/rddma_location.h>

struct rddma_xfer {
#ifdef SERIALIZE_BIND_PROCESSING
	struct rddma_dma_descriptor descriptor __attribute__ ((aligned(RDDMA_DESC_ALIGN)));
#endif
	struct rddma_desc_param desc;
	struct kobject kobj;
	struct rddma_bind *head_bind;
	struct rddma_binds *binds;
	struct list_head dma_chain;
	struct semaphore dma_sync; 
};

static inline struct rddma_xfer *to_rddma_xfer(struct kobject *kobj)
{
	return kobj ? container_of(kobj, struct rddma_xfer, kobj) : NULL;
}

static inline struct rddma_xfer *rddma_xfer_get(struct rddma_xfer *rddma_xfer)
{
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,rddma_xfer);
	return to_rddma_xfer(kobject_get(&rddma_xfer->kobj));
}

static inline void rddma_xfer_put(struct rddma_xfer *rddma_xfer)
{
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,rddma_xfer);
	if (rddma_xfer) kobject_put(&rddma_xfer->kobj);
}

extern struct rddma_xfer *new_rddma_xfer(struct rddma_location *, struct rddma_desc_param *);
extern int rddma_xfer_register(struct rddma_xfer *);
extern void rddma_xfer_unregister(struct rddma_xfer *);
extern struct rddma_xfer *find_rddma_xfer(struct rddma_desc_param *);
extern struct rddma_xfer *rddma_xfer_create(struct rddma_location *, struct rddma_desc_param *);
extern void rddma_xfer_start(struct rddma_xfer *);
extern void rddma_xfer_delete(struct rddma_location *, struct rddma_desc_param *);
extern void rddma_xfer_load_binds(struct rddma_xfer *, struct rddma_bind *);
extern void rddma_xfer_start (struct rddma_xfer*);
extern struct kobj_type rddma_xfer_type;
#endif /* RDDMA_XFER_H */
