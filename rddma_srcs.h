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

#ifndef RDDMA_SRCS_H
#define RDDMA_SRCS_H

#include <linux/rddma.h>
#include <linux/rddma_dst.h>

struct rddma_srcs {
	struct kset kset;
};

static inline struct rddma_srcs *to_rddma_srcs(struct kobject *kobj)
{
	return kobj ? container_of(to_kset(kobj), struct rddma_srcs, kset) : NULL;
}

static inline struct rddma_srcs *rddma_srcs_get(struct rddma_srcs *rddma_srcs)
{
	return to_rddma_srcs(kobject_get(&rddma_srcs->kset.kobj));
}

static inline void rddma_srcs_put(struct rddma_srcs *rddma_srcs)
{
	if (rddma_srcs) kset_put(&rddma_srcs->kset);
}

extern struct rddma_srcs *new_rddma_srcs(struct rddma_xfer_param *, struct rddma_dst *);
extern int rddma_srcs_register(struct rddma_srcs *);
extern void rddma_srcs_unregister(struct rddma_srcs *);
extern struct rddma_srcs *rddma_srcs_create(struct rddma_dst *,struct rddma_xfer_param *);
extern void rddma_srcs_delete(struct rddma_srcs *srcs);
extern struct kobj_type rddma_srcs_type;
#endif /* RDDMA_SRCS_H */
