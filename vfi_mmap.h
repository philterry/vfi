/* 
 * 
 * Copyright 2008 Vmetro 
 * Trevor Anderson <tanderson@vmetro.com> 
 * 
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or  (at your
 * option) any later version.
 */

#ifndef VFI_MMAP_H
#define VFI_MMAP_H

#include <linux/vfi.h>
#include <linux/vfi_smb.h>
#include <linux/vfi_parse.h>

struct vfi_mmap {
	struct vfi_desc_param desc;
	unsigned long		t_id;		/* Ticket number, for verification */
	struct page		**pg_tbl;	/* Page table pointer to start of block */
	unsigned long		n_pg;		/* Number of pages in block */
	struct kobject kobj;
};

static inline unsigned long mmap_to_ticket(struct vfi_mmap *m)
{
	unsigned long t = (unsigned long)m & 0xffffffffUL;
	t = (t & ~0xffff) ^ ((t & 0xffff) << 16);
	return t;
}

static inline int is_mmap_ticket(struct vfi_mmap *m, unsigned long t)
{
	unsigned long s = (unsigned long)m & 0xffffffffUL;
	return (((s & ~0xffff) ^ t) >> 16) == (s & 0xffff);	
}

static inline struct vfi_mmap *to_vfi_mmap(struct kobject *kobj)
{
    return kobj ? container_of(kobj, struct vfi_mmap,kobj) : NULL;
}

static inline struct vfi_mmap *vfi_mmap_get(struct vfi_mmap *vfi_mmap)
{
    return to_vfi_mmap(kobject_get(&vfi_mmap->kobj));
}

static inline void vfi_mmap_put(struct vfi_mmap *vfi_mmap)
{
    kobject_put(&vfi_mmap->kobj);
}

extern int new_vfi_mmap(struct vfi_mmap **, struct vfi_smb *, struct vfi_desc_param *);
extern int vfi_mmap_register(struct vfi_mmap *);
extern void vfi_mmap_unregister(struct vfi_mmap *);
extern int find_vfi_mmap_by_id(struct vfi_mmap **, unsigned long);
extern int find_vfi_mmap(struct vfi_mmap **, struct vfi_smb *, struct vfi_desc_param *);
extern int vfi_mmap_create(struct vfi_mmap **, struct vfi_smb *, struct vfi_desc_param *);
extern void vfi_mmap_delete(struct vfi_smb *, struct vfi_desc_param *);
extern struct kobj_type vfi_mmap_type;
#endif /* VFI_MMAP_H */
