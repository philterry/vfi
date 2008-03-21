/* 
 * 
 * Copyright 2007 MicroMemory, LLC. 
 * Trevor Anderson <tanderson@micromemory.com> 
 * 
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or  (at your
 * option) any later version.
 */

#ifndef VFI_MMAPS_H
#define VFI_MMAPS_H

#include <linux/vfi_smb.h>

struct vfi_mmaps {
    struct kset kset;
};

static inline struct vfi_mmaps *to_vfi_mmaps(struct kobject *kobj)
{
    return kobj ? container_of(to_kset(kobj), struct vfi_mmaps,kset) : NULL;
}

static inline struct vfi_mmaps *vfi_mmaps_get(struct vfi_mmaps *vfi_mmaps)
{
    return to_vfi_mmaps(kobject_get(&vfi_mmaps->kset.kobj));
}

static inline void vfi_mmaps_put(struct vfi_mmaps *vfi_mmaps)
{
    kset_put(&vfi_mmaps->kset);
}

extern int new_vfi_mmaps(struct vfi_mmaps **, struct vfi_smb *,char *name);
extern int vfi_mmaps_register(struct vfi_mmaps *);
extern void vfi_mmaps_unregister(struct vfi_mmaps *);
extern int find_vfi_mmaps(struct vfi_mmaps **, char *);
extern struct kobj_type vfi_mmaps_type;
#endif /* VFI_MMAPS_H */
