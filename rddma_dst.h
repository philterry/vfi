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

#ifndef RDDMA_DST_H
#define RDDMA_DST_H

#include <linux/rddma.h>
#include <linux/rddma_parse.h>
#include <linux/rddma_bind.h>

struct rddma_dst {
	struct rddma_bind_param desc;
	struct kobject kobj;
	struct rddma_srcs *srcs;
	struct rddma_src *head_src;
	struct rddma_bind *bind;
};

static inline struct rddma_dst *to_rddma_dst(struct kobject *kobj)
{
	return kobj ? container_of(kobj, struct rddma_dst, kobj) : NULL;
}

static inline struct rddma_dst *rddma_dst_get(struct rddma_dst *rddma_dst)
{
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,rddma_dst);
	return to_rddma_dst(kobject_get(&rddma_dst->kobj));
}

static inline void rddma_dst_put(struct rddma_dst *rddma_dst)
{
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,rddma_dst);
	if (rddma_dst)
		kobject_put(&rddma_dst->kobj);
}

static inline struct rddma_bind *rddma_dst_parent(struct rddma_dst *dst)
{
	return dst ? dst->bind : NULL;
}


extern struct rddma_dst *new_rddma_dst(struct rddma_bind *, struct rddma_bind_param *);
extern int rddma_dst_register(struct rddma_dst *);
extern void rddma_dst_unregister(struct rddma_dst *);
extern struct rddma_dst *find_rddma_dst(struct rddma_bind_param *);
extern struct rddma_dst *rddma_dst_create(struct rddma_bind *, struct rddma_bind_param *);
extern void rddma_dst_delete(struct rddma_bind *, struct rddma_bind_param *);
extern void rddma_dst_load_srcs(struct rddma_dst *);
extern struct kobj_type rddma_dst_type;
#endif /* RDDMA_DST_H */
