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

#ifndef VFI_SRCS_H
#define VFI_SRCS_H

#include <linux/vfi.h>
#include <linux/vfi_dst.h>

struct vfi_srcs {
	struct kset kset;
	struct list_head dma_chain;
};

static inline struct vfi_srcs *to_vfi_srcs(struct kobject *kobj)
{
	return kobj ? container_of(to_kset(kobj), struct vfi_srcs, kset) : NULL;
}

static inline struct vfi_srcs *vfi_srcs_get(struct vfi_srcs *vfi_srcs)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_srcs);
	return to_vfi_srcs(kobject_get(&vfi_srcs->kset.kobj));
}

static inline void vfi_srcs_put(struct vfi_srcs *vfi_srcs)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_srcs);
	if (vfi_srcs) kset_put(&vfi_srcs->kset);
}

extern int new_vfi_srcs(struct vfi_srcs **, struct vfi_bind_param *, struct vfi_dst *);
extern int vfi_srcs_register(struct vfi_srcs *);
extern void vfi_srcs_unregister(struct vfi_srcs *);
extern int vfi_srcs_create(struct vfi_srcs **, struct vfi_dst *,struct vfi_bind_param *);
extern void vfi_srcs_delete(struct vfi_srcs *srcs);
extern struct kobj_type vfi_srcs_type;
#endif /* VFI_SRCS_H */
