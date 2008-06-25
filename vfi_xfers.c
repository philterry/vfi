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

#define MY_DEBUG      VFI_DBG_XFER | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_XFER | VFI_DBG_LIFE    | VFI_DBG_DEBUG
#define MY_ERROR      VFI_DBG_XFER | VFI_DBG_ERROR   | VFI_DBG_ERR

#include <linux/vfi_xfers.h>

#include <linux/slab.h>
#include <linux/module.h>

static void vfi_xfers_release(struct kobject *kobj)
{
    struct vfi_xfers *p = to_vfi_xfers(kobj);
    VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,p);
    kfree(p);
}

struct vfi_xfers_attribute {
    struct attribute attr;
    ssize_t (*show)(struct vfi_xfers*, char *buffer);
    ssize_t (*store)(struct vfi_xfers*, const char *buffer, size_t size);
};

#define VFI_XFERS_ATTR(_name,_mode,_show,_store) struct vfi_xfers_attribute vfi_xfers_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t vfi_xfers_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct vfi_xfers_attribute *pattr = container_of(attr, struct vfi_xfers_attribute, attr);
    struct vfi_xfers *p = to_vfi_xfers(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t vfi_xfers_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct vfi_xfers_attribute *pattr = container_of(attr, struct vfi_xfers_attribute, attr);
    struct vfi_xfers *p = to_vfi_xfers(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops vfi_xfers_sysfs_ops = {
    .show = vfi_xfers_show,
    .store = vfi_xfers_store,
};


static ssize_t vfi_xfers_default_show(struct vfi_xfers *vfi_xfers, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "vfi_xfers_default");
}

static ssize_t vfi_xfers_default_store(struct vfi_xfers *vfi_xfers, const char *buffer, size_t size)
{
    return size;
}

VFI_XFERS_ATTR(default, 0644, vfi_xfers_default_show, vfi_xfers_default_store);

static struct attribute *vfi_xfers_default_attrs[] = {
    &vfi_xfers_attr_default.attr,
    0,
};

struct kobj_type vfi_xfers_type = {
    .release = vfi_xfers_release,
    .sysfs_ops = &vfi_xfers_sysfs_ops,
    .default_attrs = vfi_xfers_default_attrs,
};

static int vfi_xfers_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *vfi_xfers_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int vfi_xfers_uevent(struct kset *kset, struct kobject *kobj, struct kobj_uevent_env *env)
{
	return -ENODEV; /* Do not generate event */
}


static struct kset_uevent_ops vfi_xfers_uevent_ops = {
	.filter = vfi_xfers_uevent_filter,
	.name = vfi_xfers_uevent_name,
 	.uevent = vfi_xfers_uevent, 
};

int new_vfi_xfers(struct vfi_xfers **xfers, char *name, struct vfi_location *parent)
{
    struct vfi_xfers *new = kzalloc(sizeof(struct vfi_xfers), GFP_KERNEL);
    
    *xfers = new;

    if (NULL == new)
	return VFI_RESULT(-ENOMEM);

    kobject_set_name(&new->kset.kobj,name);
    new->kset.kobj.ktype = &vfi_xfers_type;
    new->kset.uevent_ops = &vfi_xfers_uevent_ops;
    new->kset.kobj.parent = &parent->kset.kobj;

    VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,new);
    return VFI_RESULT(0);
}

int vfi_xfers_register(struct vfi_xfers *vfi_xfers)
{
    int ret = 0;

    if ( (ret = kset_register(&vfi_xfers->kset) ) )
	goto out;

      return VFI_RESULT(ret);

out:
    return VFI_RESULT(ret);
}

void vfi_xfers_unregister(struct vfi_xfers *vfi_xfers)
{
    
     kset_unregister(&vfi_xfers->kset);
}

