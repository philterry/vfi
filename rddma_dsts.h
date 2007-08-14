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

#ifndef RDDMA_DSTS_H
#define RDDMA_DSTS_H

#include <linux/rddma.h>
#include <linux/rddma_bind.h>

struct rddma_dsts {
	struct kset kset;
};

static inline struct rddma_dsts *to_rddma_dsts(struct kobject *kobj)
{
	return kobj ? container_of(to_kset(kobj), struct rddma_dsts, kset) : NULL;
}

static inline struct rddma_dsts *rddma_dsts_get(struct rddma_dsts *rddma_dsts)
{
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,rddma_dsts);
	return to_rddma_dsts(kobject_get(&rddma_dsts->kset.kobj));
}

static inline void rddma_dsts_put(struct rddma_dsts *rddma_dsts)
{
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,rddma_dsts);
	if (rddma_dsts) kset_put(&rddma_dsts->kset);
}

extern struct rddma_dsts *new_rddma_dsts(struct rddma_xfer_param *, struct rddma_bind *, char *, va_list );
extern int rddma_dsts_register(struct rddma_dsts *);
extern void rddma_dsts_unregister(struct rddma_dsts *);
extern struct rddma_dsts *rddma_dsts_create(struct rddma_bind *, struct rddma_xfer_param *, char *, ...) __attribute__((format(printf, 3,4)));
extern struct rddma_dsts *rddma_dsts_create_ap(struct rddma_bind *, struct rddma_xfer_param *, char *, va_list );
extern void rddma_dsts_delete(struct rddma_dsts *);
extern struct kobj_type rddma_dsts_type;
#endif /* RDDMA_DSTS_H */
