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

#ifndef VFI_XFERS_H
#define VFI_XFERS_H

#include <linux/vfi.h>
#include <linux/vfi_location.h>

struct vfi_xfers {
	struct kset kset;
};

static inline struct vfi_xfers *to_vfi_xfers(struct kobject *kobj)
{
	return kobj ? container_of(to_kset(kobj), struct vfi_xfers, kset) : NULL;
}

static inline struct vfi_xfers *vfi_xfers_get(struct vfi_xfers *vfi_xfers)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_xfers);
	return to_vfi_xfers(kobject_get(&vfi_xfers->kset.kobj));
}

static inline void vfi_xfers_put(struct vfi_xfers *vfi_xfers)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_xfers);
	if (vfi_xfers) kset_put(&vfi_xfers->kset);
}

extern int new_vfi_xfers(struct vfi_xfers **, char *name, struct vfi_location *);
extern struct kobj_type vfi_xfers_type;
#endif /* VFI_XFERS_H */
