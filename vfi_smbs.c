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

#define MY_DEBUG      VFI_DBG_SMB | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_SMB | VFI_DBG_LIFE    | VFI_DBG_DEBUG
#define MY_ERROR      VFI_DBG_SMB | VFI_DBG_ERROR   | VFI_DBG_ERR

#include <linux/vfi_smbs.h>

#include <linux/slab.h>
#include <linux/module.h>

static void vfi_smbs_release(struct kobject *kobj)
{
    struct vfi_smbs *p = to_vfi_smbs(kobj);
    VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,p);
    kfree(p);
}

struct vfi_smbs_attribute {
    struct attribute attr;
    ssize_t (*show)(struct vfi_smbs*, char *buffer);
    ssize_t (*store)(struct vfi_smbs*, const char *buffer, size_t size);
};

#define VFI_SMBS_ATTR(_name,_mode,_show,_store) struct vfi_smbs_attribute vfi_smbs_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t vfi_smbs_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct vfi_smbs_attribute *pattr = container_of(attr, struct vfi_smbs_attribute, attr);
    struct vfi_smbs *p = to_vfi_smbs(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t vfi_smbs_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct vfi_smbs_attribute *pattr = container_of(attr, struct vfi_smbs_attribute, attr);
    struct vfi_smbs *p = to_vfi_smbs(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops vfi_smbs_sysfs_ops = {
    .show = vfi_smbs_show,
    .store = vfi_smbs_store,
};


static ssize_t vfi_smbs_default_show(struct vfi_smbs *vfi_smbs, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("refcount %d\n",atomic_read(&vfi_smbs->kset.kobj.kref.refcount));
	return size;
}

static ssize_t vfi_smbs_default_store(struct vfi_smbs *vfi_smbs, const char *buffer, size_t size)
{
    return size;
}

VFI_SMBS_ATTR(default, 0644, vfi_smbs_default_show, vfi_smbs_default_store);

static struct attribute *vfi_smbs_default_attrs[] = {
    &vfi_smbs_attr_default.attr,
    0,
};

struct kobj_type vfi_smbs_type = {
    .release = vfi_smbs_release,
    .sysfs_ops = &vfi_smbs_sysfs_ops,
    .default_attrs = vfi_smbs_default_attrs,
};

static int vfi_smbs_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *vfi_smbs_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int vfi_smbs_uevent(struct kset *kset, struct kobject *kobj, struct kobj_uevent_env *env)
{
	return -ENODEV; /* Do not generate event */
}


static struct kset_uevent_ops vfi_smbs_uevent_ops = {
	.filter = vfi_smbs_uevent_filter,
	.name = vfi_smbs_uevent_name,
 	.uevent = vfi_smbs_uevent, 
};

int new_vfi_smbs(struct vfi_smbs **smbs, char *name, struct vfi_location *parent)
{
	int ret;
	struct vfi_smbs *new = kzalloc(sizeof(struct vfi_smbs), GFP_KERNEL);
    
	if (NULL == new) {
		*smbs = NULL;
		return VFI_RESULT(-ENOMEM);
	}

	kobject_set_name(&new->kset.kobj,name);
	new->kset.kobj.ktype = &vfi_smbs_type;
	new->kset.uevent_ops = &vfi_smbs_uevent_ops;
	new->kset.kobj.parent = &parent->kset.kobj;

	ret = kset_register(&new->kset);

	if ( !ret ) {
		*smbs = new;
	} else {
		vfi_smbs_put(new);
		if ( ret != -EEXIST ) {
			*smbs = NULL;
		}
	}

	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,*smbs);
	return VFI_RESULT(ret);
}

