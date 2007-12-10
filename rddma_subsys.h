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

#ifndef RDDMA_SUBSYS_H
#define RDDMA_SUBSYS_H

#include <linux/rddma.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>

#include <linux/rddma_parse.h>

struct rddma_subsys {
	struct rddma_desc_param desc;
	int version;
	struct cdev cdev;
	struct class class;
	dev_t dev;

	struct rddma_readies *readies;
	struct rddma_readies *dones;
	struct kset kset;
};

static inline struct rddma_subsys *to_rddma_subsys(struct kobject *kobj)
{
	return kobj ? container_of(to_kset(kobj), struct rddma_subsys, kset) : NULL;
}

static inline struct rddma_subsys *rddma_subsys_get(struct rddma_subsys *rddma_subsys)
{
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,rddma_subsys);
	return to_rddma_subsys(kobject_get(&rddma_subsys->kset.kobj));
}

static inline void rddma_subsys_put(struct rddma_subsys *rddma_subsys)
{
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,rddma_subsys);
	if (rddma_subsys) kset_put(&rddma_subsys->kset);
}

extern struct rddma_subsys *new_rddma_subsys(char *name);
extern int rddma_subsys_register(struct rddma_subsys *);
extern void rddma_subsys_unregister(struct rddma_subsys *);
extern struct kobj_type rddma_subsys_type;
#endif /* RDDMA_SUBSYS_H */
