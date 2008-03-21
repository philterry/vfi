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

#define MY_DEBUG      VFI_DBG_SRC | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_SRC | VFI_DBG_LIFE    | VFI_DBG_DEBUG

#include <linux/vfi_srcs.h>
#include <linux/vfi_dst.h>

#include <linux/slab.h>
#include <linux/module.h>

/**
* vfi_srcs_release - Release (free) an vfi_srcs cleanly.
*
* @kobj - pointer to vfi_srcs-type kobject
*
* This function is invoked by the kernel kobject manager when an 
* vfi_srcs object has finally expired. Its job is to release any
* memory resources bound to the kobject. 
*
**/
static void vfi_srcs_release(struct kobject *kobj)
{
    struct vfi_srcs *p = to_vfi_srcs(kobj);
    VFI_DEBUG(MY_LIFE_DEBUG,"XXX %s %p (refc %lx)\n", __FUNCTION__, p, (unsigned long)kobj->kref.refcount.counter);
    kfree(p);
}

struct vfi_srcs_attribute {
    struct attribute attr;
    ssize_t (*show)(struct vfi_srcs*, char *buffer);
    ssize_t (*store)(struct vfi_srcs*, const char *buffer, size_t size);
};

#define VFI_SRCS_ATTR(_name,_mode,_show,_store) struct vfi_srcs_attribute vfi_srcs_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t vfi_srcs_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct vfi_srcs_attribute *pattr = container_of(attr, struct vfi_srcs_attribute, attr);
    struct vfi_srcs *p = to_vfi_srcs(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t vfi_srcs_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct vfi_srcs_attribute *pattr = container_of(attr, struct vfi_srcs_attribute, attr);
    struct vfi_srcs *p = to_vfi_srcs(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops vfi_srcs_sysfs_ops = {
    .show = vfi_srcs_show,
    .store = vfi_srcs_store,
};


static ssize_t vfi_srcs_default_show(struct vfi_srcs *vfi_srcs, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "vfi_srcs_default");
}

static ssize_t vfi_srcs_default_store(struct vfi_srcs *vfi_srcs, const char *buffer, size_t size)
{
    return size;
}

VFI_SRCS_ATTR(default, 0644, vfi_srcs_default_show, vfi_srcs_default_store);

static struct attribute *vfi_srcs_default_attrs[] = {
    &vfi_srcs_attr_default.attr,
    0,
};

struct kobj_type vfi_srcs_type = {
    .release = vfi_srcs_release,
    .sysfs_ops = &vfi_srcs_sysfs_ops,
    .default_attrs = vfi_srcs_default_attrs,
};

static int vfi_srcs_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *vfi_srcs_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int vfi_srcs_uevent(struct kset *kset, struct kobject *kobj, char **envp, int num_envp, char *buffer, int buf_size)
{
	return -ENODEV; /* Do not generate event */
}


static struct kset_uevent_ops vfi_srcs_uevent_ops = {
	.filter = vfi_srcs_uevent_filter,
	.name = vfi_srcs_uevent_name,
 	.uevent = vfi_srcs_uevent, 
};

int new_vfi_srcs(struct vfi_srcs **srcs, struct vfi_bind_param *desc, struct vfi_dst *parent)
{
    struct vfi_srcs *new = kzalloc(sizeof(struct vfi_srcs), GFP_KERNEL);
    
    *srcs = new;
    if (NULL == new)
	return -ENOMEM;

    kobject_set_name(&new->kset.kobj,"%s.%s#%llx:%x",desc->src.name,desc->src.location,desc->src.offset,desc->src.extent);
    new->kset.kobj.ktype = &vfi_srcs_type;
    new->kset.uevent_ops = &vfi_srcs_uevent_ops;
    new->kset.kobj.parent = &parent->kobj;
    INIT_LIST_HEAD(&new->dma_chain);

    parent->srcs = new;

    VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,new);
    return 0;
}

int vfi_srcs_register(struct vfi_srcs *vfi_srcs)
{
	int ret = 0;

	VFI_KTRACE ("<*** %s IN ***>\n", __func__);
	ret = kset_register(&vfi_srcs->kset);
	VFI_KTRACE ("<*** %s OUT ***>\n", __func__);
	return ret;
}

void vfi_srcs_unregister(struct vfi_srcs *vfi_srcs)
{
    
	VFI_KTRACE ("<*** %s IN ***>\n", __func__);
	if (vfi_srcs)
		kset_unregister(&vfi_srcs->kset);
	VFI_KTRACE ("<*** %s OUT ***>\n", __func__);
}

int vfi_srcs_create(struct vfi_srcs **srcs, struct vfi_dst *parent, struct vfi_bind_param *desc)
{
	int ret;
	VFI_DEBUG(MY_DEBUG,"%s parent_dst(%p) desc(%p)\n",__FUNCTION__,parent,desc);

	ret = new_vfi_srcs(srcs,desc,parent);

	if (ret)
		return ret;

	ret = vfi_srcs_register(*srcs);
	if (ret) {
		vfi_srcs_put(*srcs);
		*srcs = NULL;
	}

	return ret;
}

void vfi_srcs_delete(struct vfi_srcs *srcs)
{
	vfi_srcs_unregister(srcs);
}
