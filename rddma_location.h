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

#ifndef RDDMA_LOCATION_H
#define RDDMA_LOCATION_H

#include <linux/rddma.h>
#include <linux/rddma_parse.h>
#include <linux/rddma_fabric.h>

struct rddma_ops;

struct rddma_location {
	struct rddma_desc_param desc;
	struct rddma_fabric_address address;

	struct kobject kobj;
	struct rddma_smbs *smbs;
	struct rddma_xfers *xfers;
};

static inline struct rddma_location *to_rddma_location(struct kobject *kobj)
{
	return kobj ? container_of(kobj, struct rddma_location, kobj) : NULL;
}

static inline struct rddma_location *rddma_location_get(struct rddma_location *rddma_location)
{
	return to_rddma_location(kobject_get(&rddma_location->kobj));
}

static inline void rddma_location_put(struct rddma_location *rddma_location)
{
	if (rddma_location) kobject_put(&rddma_location->kobj);
}

extern struct rddma_location *new_rddma_location(struct rddma_location *, struct rddma_desc_param *);
extern int rddma_location_register(struct rddma_location *);
extern void rddma_location_unregister(struct rddma_location *);
extern struct rddma_location *find_rddma_location(struct rddma_desc_param *);
extern struct rddma_location *find_rddma_name(struct rddma_desc_param *);
extern struct rddma_location *rddma_location_create(struct rddma_location *,struct rddma_desc_param *);
extern void rddma_location_delete(struct rddma_location *);
extern struct kobj_type rddma_location_type;
#endif /* RDDMA_LOCATION_H */
