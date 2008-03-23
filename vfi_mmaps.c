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

#define MY_DEBUG      VFI_DBG_MMAP | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_MMAP | VFI_DBG_LIFE    | VFI_DBG_DEBUG
#define MY_ERROR      VFI_DBG_MMAP | VFI_DBG_ERROR   | VFI_DBG_ERR

#include <linux/vfi_mmaps.h>

#include <linux/slab.h>
#include <linux/module.h>

static void vfi_mmaps_release(struct kobject *kobj)
{
    struct vfi_mmaps *p = to_vfi_mmaps(kobj);
    kfree(p);
}

struct vfi_mmaps_attribute {
    struct attribute attr;
    ssize_t (*show)(struct vfi_mmaps*, char *buffer);
    ssize_t (*store)(struct vfi_mmaps*, const char *buffer, size_t size);
};

#define VFI_MMAPS_ATTR(_name,_mode,_show,_store) struct vfi_mmaps_attribute vfi_mmaps_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t vfi_mmaps_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct vfi_mmaps_attribute *pattr = container_of(attr, struct vfi_mmaps_attribute, attr);
    struct vfi_mmaps *p = to_vfi_mmaps(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t vfi_mmaps_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct vfi_mmaps_attribute *pattr = container_of(attr, struct vfi_mmaps_attribute, attr);
    struct vfi_mmaps *p = to_vfi_mmaps(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops vfi_mmaps_sysfs_ops = {
    .show = vfi_mmaps_show,
    .store = vfi_mmaps_store,
};



struct kobj_type vfi_mmaps_type = {
    .release = vfi_mmaps_release,
    .sysfs_ops = &vfi_mmaps_sysfs_ops,
};

int find_vfi_mmaps(struct vfi_mmaps **mmaps, char *name)
{
	*mmaps = NULL;
	return VFI_RESULT(-EINVAL);
}

static int vfi_mmaps_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *vfi_mmaps_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int vfi_mmaps_uevent(struct kset *kset, struct kobject *kobj, char **envp, int num_envp, char *buffer, int buf_size)
{
	return 0; /* Do not generate event */
}


static struct kset_uevent_ops vfi_mmaps_uevent_ops = {
	.filter = vfi_mmaps_uevent_filter,
	.name = vfi_mmaps_uevent_name,
	.uevent = vfi_mmaps_uevent,
};

int new_vfi_mmaps(struct vfi_mmaps **mmaps, struct vfi_smb *parent, char *name)
{
    struct vfi_mmaps *new = kzalloc(sizeof(struct vfi_mmaps), GFP_KERNEL);
    
    *mmaps = new;

    if (NULL == new)
	return VFI_RESULT(-ENOMEM);

    kobject_set_name(&new->kset.kobj,"%s",name);
    new->kset.kobj.ktype = &vfi_mmaps_type;
    new->kset.uevent_ops = &vfi_mmaps_uevent_ops;
    new->kset.kobj.parent = &parent->kobj;
    VFI_DEBUG(MY_LIFE_DEBUG,"%s %p %s %p\n",__FUNCTION__,new, kobject_name(&new->kset.kobj),parent);
    return VFI_RESULT(0);
}

int vfi_mmaps_register(struct vfi_mmaps *vfi_mmaps)
{
    int ret = 0;
    VFI_DEBUG(MY_DEBUG,"%s %p %p\n",__FUNCTION__,vfi_mmaps,vfi_mmaps->kset.kobj.parent);
    if ( (ret = kset_register(&vfi_mmaps->kset) ) )
	goto out;

      return VFI_RESULT(ret);

out:
    return VFI_RESULT(ret);
}

void vfi_mmaps_unregister(struct vfi_mmaps *vfi_mmaps)
{
    
	if (vfi_mmaps)
		kset_unregister(&vfi_mmaps->kset);
}

