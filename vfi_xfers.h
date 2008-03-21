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

#ifndef RDDMA_XFERS_H
#define RDDMA_XFERS_H

#include <linux/rddma.h>
#include <linux/rddma_location.h>

struct rddma_xfers {
	struct kset kset;
};

static inline struct rddma_xfers *to_rddma_xfers(struct kobject *kobj)
{
	return kobj ? container_of(to_kset(kobj), struct rddma_xfers, kset) : NULL;
}

static inline struct rddma_xfers *rddma_xfers_get(struct rddma_xfers *rddma_xfers)
{
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,rddma_xfers);
	return to_rddma_xfers(kobject_get(&rddma_xfers->kset.kobj));
}

static inline void rddma_xfers_put(struct rddma_xfers *rddma_xfers)
{
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,rddma_xfers);
	if (rddma_xfers) kset_put(&rddma_xfers->kset);
}

extern int new_rddma_xfers(struct rddma_xfers **, char *name, struct rddma_location *);
extern int rddma_xfers_register(struct rddma_xfers *);
extern void rddma_xfers_unregister(struct rddma_xfers *);
extern struct kobj_type rddma_xfers_type;
#endif /* RDDMA_XFERS_H */
