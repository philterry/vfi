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

#ifndef VFI_SYNCS_H
#define VFI_SYNCS_H

#include <linux/vfi.h>
#include <linux/vfi_location.h>

struct vfi_syncs {
    struct kset kset;
};

static inline struct vfi_syncs *to_vfi_syncs(struct kobject *kobj)
{
    return kobj ? container_of(to_kset(kobj), struct vfi_syncs, kset) : NULL;
}

static inline struct vfi_syncs *vfi_syncs_get(struct vfi_syncs *vfi_syncs)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_syncs);
	return to_vfi_syncs(kobject_get(&vfi_syncs->kset.kobj));
}

static inline void vfi_syncs_put(struct vfi_syncs *vfi_syncs)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_syncs);
	if (vfi_syncs) kset_put(&vfi_syncs->kset);
}

extern int new_vfi_syncs(struct vfi_syncs **, char *name, struct vfi_location *);
extern struct kobj_type vfi_syncs_type;
#endif /* VFI_SYNCS_H */
