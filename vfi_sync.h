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

#ifndef VFI_SYNC_H
#define VFI_SYNC_H

#include <linux/vfi.h>
#include <linux/vfi_location.h>
#include <linux/vfi_parse.h>
#include <linux/vfi_dsts.h>
#include <linux/vfi_srcs.h>
#include <linux/wait.h>

struct vfi_sync {
	struct vfi_desc_param desc;
	struct semaphore sem;
	wait_queue_head_t waitq;
	int count;
	struct kobject kobj;
};

static inline struct vfi_sync *to_vfi_sync(struct kobject *kobj)
{
    return kobj ? container_of(kobj, struct vfi_sync, kobj) : NULL;
}

static inline struct vfi_sync *vfi_sync_get(struct vfi_sync *vfi_sync)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_sync);
	return to_vfi_sync(kobject_get(&vfi_sync->kobj));
}

static inline void vfi_sync_put(struct vfi_sync *vfi_sync)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_sync);
	if (vfi_sync) kobject_put(&vfi_sync->kobj);
}

extern int find_vfi_sync_in(struct vfi_sync **, struct vfi_location *,struct vfi_desc_param *);

static inline int find_vfi_sync(struct vfi_sync **sync,struct vfi_desc_param *desc)
{
	return find_vfi_sync_in(sync,NULL, desc);
}

extern int new_vfi_sync(struct vfi_sync **, struct vfi_location *, struct vfi_desc_param *);
extern int vfi_sync_register(struct vfi_sync *);
extern void vfi_sync_unregister(struct vfi_sync *);
extern int vfi_sync_create(struct vfi_sync **, struct vfi_location *, struct vfi_desc_param *);
extern void vfi_sync_delete(struct vfi_location *, struct vfi_desc_param *);
extern struct kobj_type vfi_sync_type;
#endif /* VFI_SYNC_H */
