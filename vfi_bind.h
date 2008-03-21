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

#ifndef VFI_BIND_H
#define VFI_BIND_H

#include <linux/vfi.h>
#include <linux/vfi_xfer.h>
#include <linux/vfi_parse.h>

struct vfi_dst;

struct vfi_bind {
	struct vfi_dma_descriptor descriptor __attribute__ ((aligned(VFI_DESC_ALIGN)));
	struct vfi_bind_param desc;
	struct kobject kobj;
	struct vfi_dsts *dsts;
	struct list_head dma_chain;
	struct list_head *end_of_chain;

	/* doorbell events created or used by the transfer agent */
	int dst_done_event_id;
	int src_done_event_id;
	int dst_ready_event_id;
	int src_ready_event_id;

	/* doorbell events created or used by the src/dst agents */
	struct vfi_event *dst_ready_event;
	struct vfi_event *src_ready_event;
	struct vfi_event *dst_done_event;
	struct vfi_event *src_done_event;

	int ready;
#define VFI_BIND_DONE 0
#define VFI_BIND_SRC_RDY 1
#define VFI_BIND_DST_RDY 2
#define VFI_BIND_RDY 3

};

static inline int is_vfi_bind_ready(struct vfi_bind *b)
{
	return b->ready == VFI_BIND_RDY;
}

static inline int is_vfi_bind_done(struct vfi_bind *b)
{ 
	b->ready ^= VFI_BIND_RDY;
	return ! b->ready;
}

static inline int vfi_bind_src_ready(struct vfi_bind *b)
{ 
	b->ready ^= VFI_BIND_SRC_RDY;
	VFI_ASSERT(b->ready & VFI_BIND_SRC_RDY,"%s\n",__FUNCTION__);
	return b->ready == VFI_BIND_RDY;
}

static inline int vfi_bind_dst_ready(struct vfi_bind *b)
{ 
	b->ready ^= VFI_BIND_DST_RDY;
	VFI_ASSERT(b->ready & VFI_BIND_DST_RDY,"%s\n",__FUNCTION__);
	return b->ready == VFI_BIND_RDY;
}

static inline void vfi_bind_src_done(struct vfi_bind *b)
{ 
	b->ready ^= VFI_BIND_SRC_RDY;
	VFI_ASSERT(!(b->ready & VFI_BIND_SRC_RDY),"%s\n",__FUNCTION__);
}

static inline void vfi_bind_dst_done(struct vfi_bind *b)
{ 
	b->ready ^= VFI_BIND_DST_RDY;
	VFI_ASSERT(!(b->ready & VFI_BIND_DST_RDY),"%s\n",__FUNCTION__);
}

static inline struct vfi_bind *to_vfi_bind(struct kobject *kobj)
{
    return kobj ? container_of(kobj, struct vfi_bind, kobj) : NULL;
}

static inline struct vfi_bind *vfi_bind_get(struct vfi_bind *vfi_bind)
{
	VFI_DEBUG (MY_LIFE_DEBUG, "<*** %s %s ***>\n", __func__, (vfi_bind) ? kobject_name (&vfi_bind->kobj) : "<NULL>");
	return to_vfi_bind(kobject_get(&vfi_bind->kobj));
}

static inline void vfi_bind_put(struct vfi_bind *vfi_bind)
{
	VFI_DEBUG (MY_LIFE_DEBUG, "<*** %s %s ***>\n", __func__, (vfi_bind) ? kobject_name (&vfi_bind->kobj) : "<NULL>");
	if (vfi_bind)
		kobject_put(&vfi_bind->kobj);
}

extern int new_vfi_bind(struct vfi_bind **, struct vfi_xfer *, struct vfi_bind_param *);
extern int vfi_bind_register(struct vfi_bind *);
extern void vfi_bind_unregister(struct vfi_bind *);
extern int find_vfi_bind_in(struct vfi_bind **, struct vfi_xfer *, struct vfi_desc_param *);
static inline int find_vfi_bind(struct vfi_bind **bind, struct vfi_desc_param *desc)
{
	return find_vfi_bind_in(bind,0,desc);
}
extern int vfi_bind_create(struct vfi_bind **, struct vfi_xfer *, struct vfi_bind_param *);
extern void vfi_bind_delete(struct vfi_xfer *, struct vfi_desc_param *);
extern void vfi_bind_load_dsts(struct vfi_bind *);
extern struct kobj_type vfi_bind_type;
#endif /* VFI_BIND_H */
