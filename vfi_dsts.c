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

#define MY_DEBUG      VFI_DBG_DST | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_DST | VFI_DBG_LIFE    | VFI_DBG_DEBUG

#include <linux/vfi_dsts.h>
#include <linux/vfi_bind.h>

#include <linux/slab.h>
#include <linux/module.h>

/**
* vfi_dsts_release - Release (free) an vfi_dsts cleanly.
*
* @kobj - pointer to vfi_dsts-type kobject
*
* This function is invoked by the kernel kobject manager when an 
* vfi_dsts object has finally expired. Its job is to release any
* memory resources bound to the kobject. 
*
**/
static void vfi_dsts_release(struct kobject *kobj)
{
    struct vfi_dsts *p = to_vfi_dsts(kobj);
    VFI_DEBUG(MY_LIFE_DEBUG,"XXX %s %p (refc %lx)\n", __FUNCTION__, p, (unsigned long)kobj->kref.refcount.counter);
    kfree(p);
}

struct vfi_dsts_attribute {
    struct attribute attr;
    ssize_t (*show)(struct vfi_dsts*, char *buffer);
    ssize_t (*store)(struct vfi_dsts*, const char *buffer, size_t size);
};

#define VFI_DSTS_ATTR(_name,_mode,_show,_store) struct vfi_dsts_attribute vfi_dsts_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t vfi_dsts_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct vfi_dsts_attribute *pattr = container_of(attr, struct vfi_dsts_attribute, attr);
    struct vfi_dsts *p = to_vfi_dsts(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t vfi_dsts_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct vfi_dsts_attribute *pattr = container_of(attr, struct vfi_dsts_attribute, attr);
    struct vfi_dsts *p = to_vfi_dsts(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops vfi_dsts_sysfs_ops = {
    .show = vfi_dsts_show,
    .store = vfi_dsts_store,
};


static ssize_t vfi_dsts_default_show(struct vfi_dsts *vfi_dsts, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "vfi_dsts_default");
}

static ssize_t vfi_dsts_default_store(struct vfi_dsts *vfi_dsts, const char *buffer, size_t size)
{
    return size;
}

VFI_DSTS_ATTR(default, 0644, vfi_dsts_default_show, vfi_dsts_default_store);

static struct attribute *vfi_dsts_default_attrs[] = {
    &vfi_dsts_attr_default.attr,
    0,
};

struct kobj_type vfi_dsts_type = {
    .release = vfi_dsts_release,
    .sysfs_ops = &vfi_dsts_sysfs_ops,
    .default_attrs = vfi_dsts_default_attrs,
};

static int vfi_dsts_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *vfi_dsts_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int vfi_dsts_uevent(struct kset *kset, struct kobject *kobj, char **envp, int num_envp, char *buffer, int buf_size)
{
	return -ENODEV; /* Do not generate event */
}


static struct kset_uevent_ops vfi_dsts_uevent_ops = {
	.filter = vfi_dsts_uevent_filter,
	.name = vfi_dsts_uevent_name,
 	.uevent = vfi_dsts_uevent, 
};

int new_vfi_dsts(struct vfi_dsts **dsts,struct vfi_bind_param *params, struct vfi_bind *parent)
{
	struct vfi_dsts *new = kzalloc(sizeof(struct vfi_dsts), GFP_KERNEL);
    
	*dsts = new;

	if (NULL == new)
		return -ENOMEM;
    
	kobject_set_name(&new->kset.kobj,"%s.%s#%llx:%x=%s.%s#%llx:%x",
						    params->dst.name,params->dst.location,params->dst.offset,params->dst.extent,
						    params->src.name,params->src.location,params->src.offset,params->src.extent);
	new->kset.kobj.ktype = &vfi_dsts_type;
	new->kset.uevent_ops = &vfi_dsts_uevent_ops;
	new->kset.kobj.parent = &parent->kobj;
	INIT_LIST_HEAD(&new->dma_chain);

	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,new);
	return 0;
}

int vfi_dsts_register(struct vfi_dsts *vfi_dsts)
{
    	int ret = 0;

	VFI_KTRACE ("<*** %s IN ***>\n", __func__);
	ret = kset_register(&vfi_dsts->kset);
	VFI_KTRACE ("<*** %s OUT ***>\n", __func__);
	return ret;
}

void vfi_dsts_unregister(struct vfi_dsts *vfi_dsts)
{
    
	VFI_KTRACE ("<*** %s IN ***>\n", __func__);
	if (vfi_dsts)
		kset_unregister(&vfi_dsts->kset);
	VFI_KTRACE ("<*** %s OUT ***>\n", __func__);
}

/**
* vfi_dsts_create - create a bind dsts kobject.
* @parent : pointer to parent bind that dsts is to be slung under
* @desc   : bind descriptor
*
* This function creates - if necessary - a <dsts> kobject and plugs it into a 
* parent bind.
*
*
**/
int vfi_dsts_create(struct vfi_dsts **dsts, struct vfi_bind *parent, struct vfi_bind_param *desc)
{
	int ret;
	VFI_DEBUG(MY_DEBUG,"%s: parent(%p) desc(%p)\n",__FUNCTION__,parent,desc);

	if (parent->dsts) {
		*dsts = parent->dsts;
		return 0;
	}

	ret = new_vfi_dsts(dsts,desc,parent);
	if (ret) 
		return ret;

	ret = vfi_dsts_register(*dsts);
	if (ret){
		vfi_dsts_put(*dsts);
		*dsts = NULL;
	}

	return ret;
}

void vfi_dsts_delete(struct vfi_dsts *dsts)
{
	if (dsts)
		vfi_dsts_unregister(dsts);
}
