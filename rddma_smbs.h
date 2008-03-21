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

#ifndef RDDMA_SMBS_H
#define RDDMA_SMBS_H

#include <linux/rddma.h>
#include <linux/rddma_location.h>

struct rddma_smbs {
    struct kset kset;
};

static inline struct rddma_smbs *to_rddma_smbs(struct kobject *kobj)
{
    return kobj ? container_of(to_kset(kobj), struct rddma_smbs, kset) : NULL;
}

static inline struct rddma_smbs *rddma_smbs_get(struct rddma_smbs *rddma_smbs)
{
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,rddma_smbs);
	return to_rddma_smbs(kobject_get(&rddma_smbs->kset.kobj));
}

static inline void rddma_smbs_put(struct rddma_smbs *rddma_smbs)
{
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,rddma_smbs);
	if (rddma_smbs) kset_put(&rddma_smbs->kset);
}

extern int new_rddma_smbs(struct rddma_smbs **, char *name, struct rddma_location *);
extern int rddma_smbs_register(struct rddma_smbs *);
extern void rddma_smbs_unregister(struct rddma_smbs *);
extern struct kobj_type rddma_smbs_type;
#endif /* RDDMA_SMBS_H */
