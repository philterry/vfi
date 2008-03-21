/* 
 * 
 * Copyright 2007 MicroMemory, LLC.
 * Phil Terry <pterry@micromemory.com> 
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef VFI_SMB_H
#define VFI_SMB_H

#include <linux/vfi.h>
#include <linux/vfi_location.h>
#include <linux/vfi_parse.h>
#include <linux/vfi_dsts.h>
#include <linux/vfi_srcs.h>

#define DESC_VALID(b,d) (((d)->offset + (d)->extent) <= (b)->extent)

#define START_PAGE(b,d) (((b)->offset + (d)->offset)>>PAGE_SHIFT)
#define END_PAGE(b,d) (((b)->offset + (d)->offset + (d)->extent -1) >> PAGE_SHIFT)

#define START_OFFSET(b,d) (((b)->offset + (d)->offset) & ~PAGE_MASK)

#define START_SIZE(b,d) ({unsigned int _avail = (PAGE_SIZE - START_OFFSET((b),(d))); _avail > (d)->extent ? (d)->extent : _avail;})

#define END_SIZE(b,d) ({ unsigned int apsize = (((b)->offset + (d)->offset + (d)->extent) & ~PAGE_MASK); apsize > (d)->extent ? 0 : apsize;})
#define NUM_DMA(b,d) (END_PAGE((b),(d)) - START_PAGE((b),(d)) + 1)
/* #define NUM_DMA(b,d) ((START_SIZE((b),(d)) ? 1 : 0) + (END_SIZE((b),(d)) ? 1 : 0) + ((((d)->extent - START_SIZE((b),(d))) - END_SIZE((b),(d))) >> PAGE_SHIFT) ) */


struct vfi_smb {
	struct vfi_desc_param desc;
	size_t size;
	
	struct page **pages;
	int num_pages;

	struct vfi_mmaps *mmaps;

	struct kobject kobj;
};

static inline struct vfi_smb *to_vfi_smb(struct kobject *kobj)
{
    return kobj ? container_of(kobj, struct vfi_smb, kobj) : NULL;
}

static inline struct vfi_smb *vfi_smb_get(struct vfi_smb *vfi_smb)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_smb);
	return to_vfi_smb(kobject_get(&vfi_smb->kobj));
}

static inline void vfi_smb_put(struct vfi_smb *vfi_smb)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,vfi_smb);
	if (vfi_smb) kobject_put(&vfi_smb->kobj);
}

extern int find_vfi_smb_in(struct vfi_smb **, struct vfi_location *,struct vfi_desc_param *);

static inline int find_vfi_smb(struct vfi_smb **smb,struct vfi_desc_param *desc)
{
	return find_vfi_smb_in(smb,NULL, desc);
}

extern int new_vfi_smb(struct vfi_smb **, struct vfi_location *, struct vfi_desc_param *);
extern int vfi_smb_register(struct vfi_smb *);
extern void vfi_smb_unregister(struct vfi_smb *);
extern int vfi_smb_create(struct vfi_smb **, struct vfi_location *, struct vfi_desc_param *);
extern void vfi_smb_delete(struct vfi_smb *);
extern struct kobj_type vfi_smb_type;
#endif /* VFI_SMB_H */
