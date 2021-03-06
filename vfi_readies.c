/* 
 * 
 * Copyright 2008 Vmetro. 
 * Phil Terry <pterry@vmetro.com> 
 * 
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or  (at your
 * option) any later version.
 */

#define MY_DEBUG      VFI_DBG_RDYS | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_RDYS | VFI_DBG_LIFE    | VFI_DBG_DEBUG
#define MY_ERROR      VFI_DBG_RDYS | VFI_DBG_ERROR   | VFI_DBG_ERR

#include <linux/vfi_drv.h>
#include <linux/vfi_readies.h>
#include <linux/vfi_events.h>
#include <linux/slab.h>
#include <linux/module.h>

static void vfi_readies_release(struct kobject *kobj)
{
    struct vfi_readies *p = to_vfi_readies(kobj);
    VFI_DEBUG(MY_LIFE_DEBUG, "%s %p\n",__func__,p);
    kfree(p);
}

struct vfi_readies_attribute {
    struct attribute attr;
    ssize_t (*show)(struct vfi_readies*, char *buffer);
    ssize_t (*store)(struct vfi_readies*, const char *buffer, size_t size);
};

#define VFI_READIES_ATTR(_name,_mode,_show,_store) struct vfi_readies_attribute vfi_readies_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t vfi_readies_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct vfi_readies_attribute *pattr = container_of(attr, struct vfi_readies_attribute, attr);
    struct vfi_readies *p = to_vfi_readies(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t vfi_readies_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct vfi_readies_attribute *pattr = container_of(attr, struct vfi_readies_attribute, attr);
    struct vfi_readies *p = to_vfi_readies(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops vfi_readies_sysfs_ops = {
    .show = vfi_readies_show,
    .store = vfi_readies_store,
};


static ssize_t vfi_readies_default_show(struct vfi_readies *vfi_readies, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("Readies %p\n",vfi_readies);
	ATTR_PRINTF("refcount %d\n",atomic_read(&vfi_readies->kset.kobj.kref.refcount));
	return size;
}

static ssize_t vfi_readies_default_store(struct vfi_readies *vfi_readies, const char *buffer, size_t size)
{
    return size;
}

VFI_READIES_ATTR(default, 0644, vfi_readies_default_show, vfi_readies_default_store);

static struct attribute *vfi_readies_default_attrs[] = {
    &vfi_readies_attr_default.attr,
    0,
};

struct kobj_type vfi_readies_type = {
    .release = vfi_readies_release,
    .sysfs_ops = &vfi_readies_sysfs_ops,
    .default_attrs = vfi_readies_default_attrs,
};

int find_vfi_readies(struct vfi_events **events, struct vfi_subsys *parent, char *name)
{
    *events = to_vfi_events(kset_find_obj(&parent->events->kset,name));
    return VFI_RESULT(*events == NULL);
}

int find_vfi_dones(struct vfi_events **events, struct vfi_subsys *parent, char *name)
{
    *events = to_vfi_events(kset_find_obj(&parent->events->kset,name));
    return VFI_RESULT(*events == NULL);
}

static int vfi_readies_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *vfi_readies_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int vfi_readies_uevent(struct kset *kset, struct kobject *kobj, struct kobj_uevent_env *env)
{
	return 0; /* Do not generate event */
}


static struct kset_uevent_ops vfi_readies_uevent_ops = {
	.filter = vfi_readies_uevent_filter,
	.name = vfi_readies_uevent_name,
	.uevent = vfi_readies_uevent,
};

int new_vfi_readies(struct vfi_readies **newreadies, struct vfi_subsys *parent, char *name)
{
	int ret;
	struct vfi_readies *new = kzalloc(sizeof(struct vfi_readies), GFP_KERNEL);

	*newreadies = new;

	if (NULL == new)
		return VFI_RESULT(-ENOMEM);

	kobject_set_name(&new->kset.kobj,name);
	new->kset.kobj.ktype = &vfi_readies_type;
	new->kset.uevent_ops = &vfi_readies_uevent_ops;
	new->kset.kobj.parent = &parent->kset.kobj;

	ret = kset_register(&new->kset);
	if (ret) {
		vfi_readies_put(new);
		*newreadies = NULL;
	}

	return VFI_RESULT(ret);
}

int vfi_readies_create(struct vfi_readies **new, struct vfi_subsys *parent, char *name)
{
	int ret;

	ret = new_vfi_readies(new, parent, name);

	return VFI_RESULT(ret);
}

void vfi_readies_delete(struct vfi_readies *vfi_readies)
{
	vfi_readies_put(vfi_readies);
}
