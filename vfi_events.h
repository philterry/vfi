/* 
 * 
 * Copyright 2008 Vmetro. 
 * Phil Terry <pterry@vmetro.com> 
 * 
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or  (at your
 * option) any later version.
 */

#ifndef VFI_EVENTS_H
#define VFI_EVENTS_H

#include <linux/vfi.h>
#include <linux/vfi_subsys.h>


struct vfi_events {
	int count;
	struct semaphore start_lock;
	struct completion dma_sync;
	struct kset kset;
	struct vfi_events *next;
	struct vfi_events *prev;
};

static inline struct vfi_events *to_vfi_events(struct kobject *kobj)
{
    return kobj ? container_of(to_kset(kobj), struct vfi_events,kset) : NULL;
}

static inline struct vfi_events *vfi_events_get(struct vfi_events *vfi_events)
{
    return to_vfi_events(kobject_get(&vfi_events->kset.kobj));
}

static inline void vfi_events_put(struct vfi_events *vfi_events)
{
	if (vfi_events)
		kset_put(&vfi_events->kset);
}

extern int new_vfi_events(struct vfi_events **, struct vfi_readies *, char *name);
extern int find_vfi_events(struct vfi_events **, struct vfi_readies *, char *);
extern struct kobj_type vfi_events_type;
extern int vfi_events_create(struct vfi_events **, struct vfi_readies *, char *name);
extern void vfi_events_delete(struct vfi_events *);
extern void vfi_events_start(struct vfi_events *);
#endif /* VFI_EVENTS_H */
