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

#ifndef RDDMA_BIND_H
#define RDDMA_BIND_H

#include <linux/rddma.h>
#include <linux/rddma_xfer.h>
#include <linux/rddma_parse.h>

struct rddma_dst;

struct rddma_bind {
	struct rddma_dma_descriptor descriptor __attribute__ ((aligned(RDDMA_DESC_ALIGN)));
	struct rddma_bind_param desc;
	struct kobject kobj;
	struct rddma_dsts *dsts;
	struct list_head dma_chain;
	struct list_head *end_of_chain;
	atomic_t src_votes;
	atomic_t dst_votes;
};

static inline struct rddma_bind *to_rddma_bind(struct kobject *kobj)
{
    return kobj ? container_of(kobj, struct rddma_bind, kobj) : NULL;
}

static inline struct rddma_bind *rddma_bind_get(struct rddma_bind *rddma_bind)
{
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,rddma_bind);
	return to_rddma_bind(kobject_get(&rddma_bind->kobj));
}

static inline void rddma_bind_put(struct rddma_bind *rddma_bind)
{
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,rddma_bind);
	kobject_put(&rddma_bind->kobj);
}

extern struct rddma_bind *new_rddma_bind(struct rddma_xfer *, struct rddma_bind_param *);
extern int rddma_bind_register(struct rddma_bind *);
extern void rddma_bind_unregister(struct rddma_bind *);
extern struct rddma_bind *find_rddma_bind(struct rddma_bind_param *);
extern struct rddma_bind *rddma_bind_create(struct rddma_xfer *, struct rddma_bind_param *);
extern void rddma_bind_delete(struct rddma_xfer *, struct rddma_bind_param *);
extern void rddma_bind_load_dsts(struct rddma_bind *);
extern struct kobj_type rddma_bind_type;
#endif /* RDDMA_BIND_H */
