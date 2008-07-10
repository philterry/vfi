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

#ifndef VFI_SMBS_H
#define VFI_SMBS_H

#include <linux/vfi.h>
#include <linux/vfi_location.h>

struct vfi_smbs {
    struct kset kset;
};

static inline struct vfi_smbs *to_vfi_smbs(struct kobject *kobj)
{
    return kobj ? container_of(to_kset(kobj), struct vfi_smbs, kset) : NULL;
}

static inline struct vfi_smbs *vfi_smbs_get(struct vfi_smbs *vfi_smbs)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_smbs);
	return to_vfi_smbs(kobject_get(&vfi_smbs->kset.kobj));
}

static inline void vfi_smbs_put(struct vfi_smbs *vfi_smbs)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_smbs);
	if (vfi_smbs) kset_put(&vfi_smbs->kset);
}

extern int new_vfi_smbs(struct vfi_smbs **, char *name, struct vfi_location *);
extern struct kobj_type vfi_smbs_type;
#endif /* VFI_SMBS_H */
