/* 
 * 
 * Copyright 2008 Vmetro
 * Phil Terry <pterry@vmetro.com> 
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef VFI_LOCATION_H
#define VFI_LOCATION_H

#include <linux/vfi.h>
#include <linux/vfi_parse.h>
#include <linux/vfi_fabric.h>

struct vfi_ops;

struct vfi_location {
	struct vfi_desc_param desc;

	struct kset kset;
	struct vfi_smbs *smbs;
	struct vfi_xfers *xfers;
	struct vfi_syncs *syncs;
};

static inline struct vfi_location *to_vfi_location(struct kobject *kobj)
{
	return kobj ? container_of(to_kset(kobj), struct vfi_location, kset) : NULL;
}

static inline struct vfi_location *vfi_location_get(struct vfi_location *vfi_location)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_location);
	return vfi_location ? to_vfi_location(kobject_get(&vfi_location->kset.kobj)) : NULL;
}

static inline void vfi_location_put(struct vfi_location *vfi_location)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_location);
	if (vfi_location) kset_put(&vfi_location->kset);
}

extern int new_vfi_location(struct vfi_location **, struct vfi_location *, struct vfi_desc_param *);
extern int find_vfi_name(struct vfi_location **, struct vfi_location *, struct vfi_desc_param *);
extern int find_vfi_location(struct vfi_location **, struct vfi_location *, struct vfi_desc_param *);
extern int locate_vfi_location(struct vfi_location  **, struct vfi_location *, struct vfi_desc_param *);
extern int vfi_location_create(struct vfi_location **, struct vfi_location *,struct vfi_desc_param *);
extern void vfi_location_delete(struct vfi_location *);
extern struct kobj_type vfi_location_type;
#endif /* VFI_LOCATION_H */
