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

#ifndef VFI_SUBSYS_H
#define VFI_SUBSYS_H

#include <linux/vfi.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>

#include <linux/vfi_parse.h>

struct vfi_subsys {
	struct vfi_desc_param desc;
	int version;
	struct cdev cdev;
	struct class class;
	dev_t dev;

	struct vfi_readies *events;
	struct kset kset;
};

static inline struct vfi_subsys *to_vfi_subsys(struct kobject *kobj)
{
	return kobj ? container_of(to_kset(kobj), struct vfi_subsys, kset) : NULL;
}

static inline struct vfi_subsys *vfi_subsys_get(struct vfi_subsys *vfi_subsys)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_subsys);
	return to_vfi_subsys(kobject_get(&vfi_subsys->kset.kobj));
}

static inline void vfi_subsys_put(struct vfi_subsys *vfi_subsys)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_subsys);
	if (vfi_subsys) kset_put(&vfi_subsys->kset);
}

extern int new_vfi_subsys(struct vfi_subsys **, char *name);
extern int vfi_subsys_register(struct vfi_subsys *);
extern void vfi_subsys_unregister(struct vfi_subsys *);
extern struct kobj_type vfi_subsys_type;
#endif /* VFI_SUBSYS_H */
