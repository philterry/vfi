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

#ifndef RDDMA_BINDS_H
#define RDDMA_BINDS_H

#include <linux/rddma.h>
#include <linux/rddma_xfer.h>

struct rddma_binds {
    struct kset kset;
};

static inline struct rddma_binds *to_rddma_binds(struct kobject *kobj)
{
    return kobj ? container_of(to_kset(kobj), struct rddma_binds,kset) : NULL;
}

static inline struct rddma_binds *rddma_binds_get(struct rddma_binds *rddma_binds)
{
    return to_rddma_binds(kobject_get(&rddma_binds->kset.kobj));
}

static inline void rddma_binds_put(struct rddma_binds *rddma_binds)
{
	if (rddma_binds) kset_put(&rddma_binds->kset);
}

extern struct rddma_binds *new_rddma_binds(char *name, struct rddma_xfer *);
extern int rddma_binds_register(struct rddma_binds *);
extern void rddma_binds_unregister(struct rddma_binds *);
extern struct kobj_type rddma_binds_type;
#endif /* RDDMA_BINDS_H */
