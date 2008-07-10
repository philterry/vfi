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

#ifndef VFI_DSTS_H
#define VFI_DSTS_H

#include <linux/vfi.h>
#include <linux/vfi_bind.h>
#include <linux/vfi_smb.h>

struct vfi_dsts {
	struct kset kset;
	struct list_head dma_chain;
	struct vfi_smb  *smb;
};

static inline struct vfi_dsts *to_vfi_dsts(struct kobject *kobj)
{
	return kobj ? container_of(to_kset(kobj), struct vfi_dsts, kset) : NULL;
}

static inline struct vfi_dsts *vfi_dsts_get(struct vfi_dsts *vfi_dsts)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_dsts);
	return to_vfi_dsts(kobject_get(&vfi_dsts->kset.kobj));
}

static inline void vfi_dsts_put(struct vfi_dsts *vfi_dsts)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_dsts);
	if (vfi_dsts) kset_put(&vfi_dsts->kset);
}

extern int new_vfi_dsts(struct vfi_dsts **, struct vfi_bind_param *, struct vfi_bind *);
extern int vfi_dsts_create(struct vfi_dsts **, struct vfi_bind *, struct vfi_bind_param *);
extern void vfi_dsts_delete(struct vfi_dsts *);
extern struct kobj_type vfi_dsts_type;
#endif /* VFI_DSTS_H */
