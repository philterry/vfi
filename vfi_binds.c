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

#define MY_DEBUG      VFI_DBG_BIND | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_BIND | VFI_DBG_LIFE    | VFI_DBG_DEBUG

#include <linux/vfi_binds.h>
#include <linux/vfi_xfer.h>

#include <linux/slab.h>
#include <linux/module.h>

static void vfi_binds_release(struct kobject *kobj)
{
    struct vfi_binds *p = to_vfi_binds(kobj);
    VFI_DEBUG(MY_LIFE_DEBUG,"XXX %s %p\n",__FUNCTION__,p);
    kfree(p);
}

struct vfi_binds_attribute {
    struct attribute attr;
    ssize_t (*show)(struct vfi_binds*, char *buffer);
    ssize_t (*store)(struct vfi_binds*, const char *buffer, size_t size);
};

#define VFI_BINDS_ATTR(_name,_mode,_show,_store) struct vfi_binds_attribute vfi_binds_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t vfi_binds_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct vfi_binds_attribute *pattr = container_of(attr, struct vfi_binds_attribute, attr);
    struct vfi_binds *p = to_vfi_binds(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t vfi_binds_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct vfi_binds_attribute *pattr = container_of(attr, struct vfi_binds_attribute, attr);
    struct vfi_binds *p = to_vfi_binds(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops vfi_binds_sysfs_ops = {
    .show = vfi_binds_show,
    .store = vfi_binds_store,
};


static ssize_t vfi_binds_default_show(struct vfi_binds *vfi_binds, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "vfi_binds_default");
}

static ssize_t vfi_binds_default_store(struct vfi_binds *vfi_binds, const char *buffer, size_t size)
{
    return size;
}

VFI_BINDS_ATTR(default, 0644, vfi_binds_default_show, vfi_binds_default_store);

static struct attribute *vfi_binds_default_attrs[] = {
    &vfi_binds_attr_default.attr,
    0,
};

struct kobj_type vfi_binds_type = {
    .release = vfi_binds_release,
    .sysfs_ops = &vfi_binds_sysfs_ops,
    .default_attrs = vfi_binds_default_attrs,
};

static int vfi_binds_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *vfi_binds_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int vfi_binds_uevent(struct kset *kset, struct kobject *kobj, char **envp, int num_envp, char *buffer, int buf_size)
{
	return -ENODEV ; /* Do not generate event */
}


static struct kset_uevent_ops vfi_binds_uevent_ops = {
	.filter = vfi_binds_uevent_filter,
	.name = vfi_binds_uevent_name,
 	.uevent = vfi_binds_uevent, 
};

int new_vfi_binds(struct vfi_binds **binds,char *name, struct vfi_xfer *parent)
{
    struct vfi_binds *new = kzalloc(sizeof(struct vfi_binds), GFP_KERNEL);
    
    *binds = new;

    if (NULL == new)
	return -ENOMEM;

    kobject_set_name(&new->kset.kobj,name);
    new->kset.kobj.ktype = &vfi_binds_type;
    new->kset.uevent_ops = &vfi_binds_uevent_ops;
    new->kset.kobj.parent = &parent->kobj;

    VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,new);
    return 0;
}

int vfi_binds_register(struct vfi_binds *vfi_binds)
{
	int ret = 0;

	VFI_KTRACE ("<*** %s IN ***>\n", __func__);
	ret = kset_register(&vfi_binds->kset);
	VFI_KTRACE ("<*** %s OUT ***>\n", __func__);
	return ret;
}

void vfi_binds_unregister(struct vfi_binds *vfi_binds)
{
    
	VFI_KTRACE ("<*** %s IN ***>\n", __func__);
	kset_unregister(&vfi_binds->kset);
	VFI_KTRACE ("<*** %s OUT ***>\n", __func__);
	
}

