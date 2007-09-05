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

#ifndef RDDMA_SRC_H
#define RDDMA_SRC_H

#include <linux/rddma.h>
#include <linux/rddma_parse.h>
#include <linux/rddma_dma.h>
#include <linux/rddma_dst.h>
#include <linux/rddma_ops.h>

struct rddma_src {
	struct rddma_dma_descriptor descriptor __attribute__ ((aligned(RDDMA_DESC_ALIGN)));
	struct rddma_bind_param desc;
	struct kobject kobj;
};

static inline struct rddma_src *to_rddma_src(struct kobject *kobj)
{
	return kobj ? container_of(kobj, struct rddma_src, kobj) : NULL;
}

static inline struct rddma_src *rddma_src_get(struct rddma_src *rddma_src)
{
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,rddma_src);
	return to_rddma_src(kobject_get(&rddma_src->kobj));
}

static inline void rddma_src_put(struct rddma_src *rddma_src)
{
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,rddma_src);
	kobject_put(&rddma_src->kobj);
}

extern struct rddma_src *new_rddma_src(struct rddma_dst *, struct rddma_bind_param *);
extern int rddma_src_register(struct rddma_src *);
extern void rddma_src_unregister(struct rddma_src *);
extern struct rddma_src *find_rddma_src(struct rddma_desc_param *, struct rddma_dst *);
extern struct rddma_src *rddma_src_create(struct rddma_dst *, struct rddma_bind_param *);
extern void rddma_src_delete(struct rddma_src *);
extern struct kobj_type rddma_src_type;
#endif /* RDDMA_SRC_H */
