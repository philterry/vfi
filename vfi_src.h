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

#ifndef VFI_SRC_H
#define VFI_SRC_H

#include <linux/vfi.h>
#include <linux/vfi_parse.h>
#include <linux/vfi_dma.h>
#include <linux/vfi_dst.h>
#include <linux/vfi_ops.h>

struct vfi_src {
	struct vfi_dma_descriptor descriptor __attribute__ ((aligned(VFI_DESC_ALIGN)));
	struct vfi_bind_param desc;
	struct vfi_dst *dst;
	struct kobject kobj;
	void* free_p;			/* Original [misaligned] address */
};

static inline struct vfi_src *to_vfi_src(struct kobject *kobj)
{
	return kobj ? container_of(kobj, struct vfi_src, kobj) : NULL;
}

static inline struct vfi_src *vfi_src_get(struct vfi_src *vfi_src)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_src);
	return to_vfi_src(kobject_get(&vfi_src->kobj));
}

static inline void vfi_src_put(struct vfi_src *vfi_src)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_src);
	if (vfi_src)
		kobject_put(&vfi_src->kobj);
}

extern int new_vfi_src(struct vfi_src **, struct vfi_dst *, struct vfi_bind_param *);
extern int find_vfi_src(struct vfi_src **, struct vfi_desc_param *, struct vfi_dst *);
extern int vfi_src_create(struct vfi_src **, struct vfi_dst *, struct vfi_bind_param *);
extern void vfi_src_delete(struct vfi_dst *, struct vfi_bind_param *);
extern struct kobj_type vfi_src_type;
#endif /* VFI_SRC_H */
