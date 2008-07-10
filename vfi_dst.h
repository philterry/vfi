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

#ifndef VFI_DST_H
#define VFI_DST_H

#include <linux/vfi.h>
#include <linux/vfi_parse.h>
#include <linux/vfi_bind.h>

struct vfi_dst {
	struct vfi_bind_param desc;
	struct kobject kobj;
	struct vfi_srcs *srcs;
	struct vfi_src *head_src;
	struct vfi_bind *bind;
};

static inline struct vfi_dst *to_vfi_dst(struct kobject *kobj)
{
	return kobj ? container_of(kobj, struct vfi_dst, kobj) : NULL;
}

static inline struct vfi_dst *vfi_dst_get(struct vfi_dst *vfi_dst)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_dst);
	return to_vfi_dst(kobject_get(&vfi_dst->kobj));
}

static inline void vfi_dst_put(struct vfi_dst *vfi_dst)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_dst);
	if (vfi_dst)
		kobject_put(&vfi_dst->kobj);
}

static inline struct vfi_bind *vfi_dst_parent(struct vfi_dst *dst)
{
	return dst ? dst->bind : NULL;
}


extern int new_vfi_dst(struct vfi_dst **, struct vfi_bind *, struct vfi_bind_param *);
extern int find_vfi_dst_in(struct vfi_dst **, struct vfi_bind *, struct vfi_bind_param *);
static inline int find_vfi_dst(struct vfi_dst **dst, struct vfi_bind_param *desc)
{
	return find_vfi_dst_in(dst,0,desc);
}
extern int vfi_dst_create(struct vfi_dst **, struct vfi_bind *, struct vfi_bind_param *);
extern void vfi_dst_delete(struct vfi_bind *, struct vfi_bind_param *);
extern void vfi_dst_load_srcs(struct vfi_dst *);
extern struct kobj_type vfi_dst_type;
#endif /* VFI_DST_H */
