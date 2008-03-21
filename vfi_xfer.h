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

#ifndef VFI_XFER_H
#define VFI_XFER_H

#include <linux/vfi.h>
#include <linux/vfi_dma.h>
#include <linux/vfi_parse.h>
#include <linux/vfi_location.h>

struct vfi_xfer {
	struct vfi_desc_param desc;
	struct kobject kobj;
	struct vfi_bind *head_bind;
	struct vfi_binds *binds;
	struct semaphore dma_sync; 
};

static inline struct vfi_xfer *to_vfi_xfer(struct kobject *kobj)
{
	return kobj ? container_of(kobj, struct vfi_xfer, kobj) : NULL;
}

static inline struct vfi_xfer *vfi_xfer_get(struct vfi_xfer *vfi_xfer)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_xfer);
	return to_vfi_xfer(kobject_get(&vfi_xfer->kobj));
}

static inline void vfi_xfer_put(struct vfi_xfer *vfi_xfer)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_xfer);
	if (vfi_xfer) kobject_put(&vfi_xfer->kobj);
}

extern int new_vfi_xfer(struct vfi_xfer **, struct vfi_location *, struct vfi_desc_param *);
extern int vfi_xfer_register(struct vfi_xfer *);
extern void vfi_xfer_unregister(struct vfi_xfer *);
extern int find_vfi_xfer_in(struct vfi_xfer **, struct vfi_location *, struct vfi_desc_param *);
static inline int find_vfi_xfer(struct vfi_xfer **xfer,struct vfi_desc_param *desc)
{
	return find_vfi_xfer_in(xfer, 0, desc);
}
extern int vfi_xfer_create(struct vfi_xfer **, struct vfi_location *, struct vfi_desc_param *);
extern void vfi_xfer_start(struct vfi_xfer *);
extern void vfi_xfer_delete(struct vfi_location *, struct vfi_desc_param *);
extern void vfi_xfer_load_binds(struct vfi_xfer *, struct vfi_bind *);
extern void vfi_xfer_start (struct vfi_xfer*);
extern struct kobj_type vfi_xfer_type;
#endif /* VFI_XFER_H */
