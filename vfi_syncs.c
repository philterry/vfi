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

#define MY_DEBUG      VFI_DBG_SYNC | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_SYNC | VFI_DBG_LIFE    | VFI_DBG_DEBUG
#define MY_ERROR      VFI_DBG_SYNC | VFI_DBG_ERROR   | VFI_DBG_ERR

#include <linux/vfi_syncs.h>

#include <linux/slab.h>
#include <linux/module.h>

static void vfi_syncs_release(struct kobject *kobj)
{
    struct vfi_syncs *p = to_vfi_syncs(kobj);
    VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,p);
    kfree(p);
}

struct vfi_syncs_attribute {
    struct attribute attr;
    ssize_t (*show)(struct vfi_syncs*, char *buffer);
    ssize_t (*store)(struct vfi_syncs*, const char *buffer, size_t size);
};

#define VFI_SYNCS_ATTR(_name,_mode,_show,_store) struct vfi_syncs_attribute vfi_syncs_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t vfi_syncs_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct vfi_syncs_attribute *pattr = container_of(attr, struct vfi_syncs_attribute, attr);
    struct vfi_syncs *p = to_vfi_syncs(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t vfi_syncs_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct vfi_syncs_attribute *pattr = container_of(attr, struct vfi_syncs_attribute, attr);
    struct vfi_syncs *p = to_vfi_syncs(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops vfi_syncs_sysfs_ops = {
    .show = vfi_syncs_show,
    .store = vfi_syncs_store,
};


static ssize_t vfi_syncs_default_show(struct vfi_syncs *vfi_syncs, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("refcount %d\n",atomic_read(&vfi_syncs->kset.kobj.kref.refcount));
	return size;
}

static ssize_t vfi_syncs_default_store(struct vfi_syncs *vfi_syncs, const char *buffer, size_t size)
{
    return size;
}

VFI_SYNCS_ATTR(default, 0644, vfi_syncs_default_show, vfi_syncs_default_store);

static struct attribute *vfi_syncs_default_attrs[] = {
    &vfi_syncs_attr_default.attr,
    0,
};

struct kobj_type vfi_syncs_type = {
    .release = vfi_syncs_release,
    .sysfs_ops = &vfi_syncs_sysfs_ops,
    .default_attrs = vfi_syncs_default_attrs,
};

static int vfi_syncs_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *vfi_syncs_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int vfi_syncs_uevent(struct kset *kset, struct kobject *kobj, struct kobj_uevent_env *env)
{
	return -ENODEV; /* Do not generate event */
}


static struct kset_uevent_ops vfi_syncs_uevent_ops = {
	.filter = vfi_syncs_uevent_filter,
	.name = vfi_syncs_uevent_name,
 	.uevent = vfi_syncs_uevent, 
};

int new_vfi_syncs(struct vfi_syncs **syncs, char *name, struct vfi_location *parent)
{
	int ret;
	struct vfi_syncs *new = kzalloc(sizeof(struct vfi_syncs), GFP_KERNEL);
    
	if (NULL == new) {
		*syncs = NULL;
		return VFI_RESULT(-ENOMEM);
	}

	kobject_set_name(&new->kset.kobj,name);
	new->kset.kobj.ktype = &vfi_syncs_type;
	new->kset.uevent_ops = &vfi_syncs_uevent_ops;
	new->kset.kobj.parent = &parent->kset.kobj;

	ret = kset_register(&new->kset);

	if ( !ret ) {
		*syncs = new;
	} else {
		vfi_syncs_put(new);
		if ( ret != -EEXIST ) {
			*syncs = NULL;
		}
	}

	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,new);
	return VFI_RESULT(ret);
}
