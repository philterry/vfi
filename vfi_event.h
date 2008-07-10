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

#ifndef VFI_EVENT_H
#define VFI_EVENT_H

#include <linux/vfi.h>
#include <linux/vfi_parse.h>
#include <linux/vfi_events.h>

struct vfi_event {
	struct vfi_desc_param desc;
	int event_id;
	struct vfi_bind *bind;
	void (*start_event)(struct vfi_bind *);
	struct kobject kobj;
};

static inline struct vfi_event *to_vfi_event(struct kobject *kobj)
{
    return kobj ? container_of(kobj, struct vfi_event,kobj) : NULL;
}

static inline struct vfi_event *vfi_event_get(struct vfi_event *vfi_event)
{
	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
    return to_vfi_event(kobject_get(&vfi_event->kobj));
}

static inline void vfi_event_put(struct vfi_event *vfi_event)
{
	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	if(vfi_event)
		kobject_put(&vfi_event->kobj);
}

extern int new_vfi_event(struct vfi_event **, struct vfi_events *, struct vfi_desc_param *, struct vfi_bind *, void (*)(struct vfi_bind *),int);
extern int find_vfi_event(struct vfi_event **, struct vfi_events *, int);
extern struct kobj_type vfi_event_type;
extern int vfi_event_create(struct vfi_event **, struct vfi_events *, struct vfi_desc_param *, struct vfi_bind *, void (*)(struct vfi_bind *), int);
extern void vfi_event_delete(struct vfi_event *);
extern void vfi_event_send(struct vfi_event *);
#endif /* VFI_EVENT_H */
