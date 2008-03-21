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

#ifndef VFI_BINDS_H
#define VFI_BINDS_H

#include <linux/vfi.h>
#include <linux/vfi_xfer.h>

struct vfi_binds {
    struct kset kset;
};

static inline struct vfi_binds *to_vfi_binds(struct kobject *kobj)
{
	return kobj ? container_of(to_kset(kobj), struct vfi_binds,kset) : NULL;
}

static inline struct vfi_binds *vfi_binds_get(struct vfi_binds *vfi_binds)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_binds);
	return to_vfi_binds(kobject_get(&vfi_binds->kset.kobj));
}

static inline void vfi_binds_put(struct vfi_binds *vfi_binds)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_binds);
	if (vfi_binds) kset_put(&vfi_binds->kset);
}

extern int new_vfi_binds(struct vfi_binds **, char *name, struct vfi_xfer *);
extern int vfi_binds_register(struct vfi_binds *);
extern void vfi_binds_unregister(struct vfi_binds *);
extern struct kobj_type vfi_binds_type;
#endif /* VFI_BINDS_H */
