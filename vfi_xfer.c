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

#include <linux/vfi_xfer.h>
#include <linux/vfi_parse.h>
#include <linux/vfi_location.h>
#include <linux/vfi_xfers.h>
#include <linux/vfi_ops.h>
#include <linux/vfi_bind.h>
#include <linux/vfi_dma.h>

#include <linux/slab.h>
#include <linux/module.h>

static void vfi_xfer_release(struct kobject *kobj)
{
    struct vfi_xfer *p = to_vfi_xfer(kobj);
    VFI_DEBUG(MY_LIFE_DEBUG,"XXX %s %p (refc %lx)\n", __FUNCTION__, p, (unsigned long)kobj->kref.refcount.counter);
    vfi_clean_desc(&p->desc);
    
    kfree(p);
}

struct vfi_xfer_attribute {
    struct attribute attr;
    ssize_t (*show)(struct vfi_xfer*, char *buffer);
    ssize_t (*store)(struct vfi_xfer*, const char *buffer, size_t size);
};

#define VFI_XFER_ATTR(_name,_mode,_show,_store) struct vfi_xfer_attribute vfi_xfer_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t vfi_xfer_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct vfi_xfer_attribute *pattr = container_of(attr, struct vfi_xfer_attribute, attr);
    struct vfi_xfer *p = to_vfi_xfer(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t vfi_xfer_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct vfi_xfer_attribute *pattr = container_of(attr, struct vfi_xfer_attribute, attr);
    struct vfi_xfer *p = to_vfi_xfer(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops vfi_xfer_sysfs_ops = {
    .show = vfi_xfer_show,
    .store = vfi_xfer_store,
};


static ssize_t vfi_xfer_default_show(struct vfi_xfer *vfi_xfer, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("Xfer %p is %s \n",vfi_xfer,vfi_xfer ? vfi_xfer->desc.name : NULL);
	ATTR_PRINTF("xfer: ops is %p rde is %p address is %p ploc is %p\n",vfi_xfer->desc.ops,vfi_xfer->desc.rde,vfi_xfer->desc.address, vfi_xfer->desc.ploc);
	ATTR_PRINTF("refcount %d\n",atomic_read(&vfi_xfer->kset.kobj.kref.refcount));
	return size;
}

static ssize_t vfi_xfer_default_store(struct vfi_xfer *vfi_xfer, const char *buffer, size_t size)
{
    return size;
}

VFI_XFER_ATTR(default, 0644, vfi_xfer_default_show, vfi_xfer_default_store);

static ssize_t vfi_xfer_location_show(struct vfi_xfer *vfi_xfer, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%s\n",vfi_xfer->desc.location);
}

static ssize_t vfi_xfer_location_store(struct vfi_xfer *vfi_xfer, const char *buffer, size_t size)
{
    return size;
}

VFI_XFER_ATTR(location, 0644, vfi_xfer_location_show, vfi_xfer_location_store);

static ssize_t vfi_xfer_name_show(struct vfi_xfer *vfi_xfer, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%s\n", vfi_xfer->desc.name);
}

static ssize_t vfi_xfer_name_store(struct vfi_xfer *vfi_xfer, const char *buffer, size_t size)
{
    return size;
}

VFI_XFER_ATTR(name, 0644, vfi_xfer_name_show, vfi_xfer_name_store);

static ssize_t vfi_xfer_extent_show(struct vfi_xfer *vfi_xfer, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%x\n",vfi_xfer->desc.extent);
}

static ssize_t vfi_xfer_extent_store(struct vfi_xfer *vfi_xfer, const char *buffer, size_t size)
{
    return size;
}

VFI_XFER_ATTR(extent, 0644, vfi_xfer_extent_show, vfi_xfer_extent_store);

static ssize_t vfi_xfer_offset_show(struct vfi_xfer *vfi_xfer, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%llx\n",vfi_xfer->desc.offset);
}

static ssize_t vfi_xfer_offset_store(struct vfi_xfer *vfi_xfer, const char *buffer, size_t size)
{
    return size;
}

VFI_XFER_ATTR(offset, 0644, vfi_xfer_offset_show, vfi_xfer_offset_store);

static struct attribute *vfi_xfer_default_attrs[] = { 
    &vfi_xfer_attr_default.attr,
    &vfi_xfer_attr_location.attr,
    &vfi_xfer_attr_name.attr,
    &vfi_xfer_attr_extent.attr,
    &vfi_xfer_attr_offset.attr,
    0,
};

struct kobj_type vfi_xfer_type = {
    .release = vfi_xfer_release,
    .sysfs_ops = &vfi_xfer_sysfs_ops,
    .default_attrs = vfi_xfer_default_attrs,
};

int new_vfi_xfer(struct vfi_xfer **xfer, struct vfi_location *parent, struct vfi_desc_param *desc)
{
	int ret = 0;
	struct vfi_xfer *new = kzalloc(sizeof(struct vfi_xfer), GFP_KERNEL);
    
	*xfer = new;
	
	if (NULL == new)
		return VFI_RESULT(-ENOMEM);

	vfi_clone_desc(&new->desc, desc);
	vfi_inherit(&new->desc, &parent->desc);
	vfi_update_ploc(&new->desc, parent);

	new->kset.kobj.kset = &parent->xfers->kset;
	new->kset.kobj.ktype = &vfi_xfer_type;
	kobject_set_name(&new->kset.kobj, "%s", desc->name);
	ret = kset_register(&new->kset);
	if (ret) {
		kset_put(&new->kset);
		*xfer = NULL;
	}

	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,*xfer);
	return VFI_RESULT(ret);
}

/**
* find_vfi_xfer_in - find, or create, an Xfer object at a specified location.
* @loc  - location where Xfer is to be sought.
* @desc - string descriptor of Xfer specification
*
* The @loc argument, which specifies which location the Xfer is to be found or
* located at, may be NULL; in which case the location chain embedded in @desc 
* will be found/created first.
*
* If @loc is specified by the caller, it is TAKEN ON TRUST that the location provided
* matches the location specified in the <xfer-spec>.
* 
**/
int find_vfi_xfer_in(struct vfi_xfer **xfer,struct vfi_location *loc, struct vfi_desc_param *desc)
{
	int ret;
	struct vfi_location *tmploc;
	VFI_DEBUG(MY_DEBUG,"%s desc(%p) ploc(%p)\n",__FUNCTION__,desc,desc->ploc);

	if (loc && loc->desc.ops && loc->desc.ops->xfer_find)
		return VFI_RESULT(loc->desc.ops->xfer_find(xfer,loc,desc));

	if (desc->ploc && desc->ploc->desc.ops && desc->ploc->desc.ops->xfer_find)
		return VFI_RESULT(desc->ploc->desc.ops->xfer_find(xfer,desc->ploc,desc));

	*xfer = NULL;

	ret = locate_vfi_location(&tmploc,NULL,desc);
	if (ret)
		return VFI_RESULT(ret);

	vfi_update_ploc(desc,tmploc);

	if (tmploc && tmploc->desc.ops && tmploc->desc.ops->xfer_find) {
		ret = tmploc->desc.ops->xfer_find(xfer,tmploc,desc);
		vfi_location_put(tmploc);
	}

	return VFI_RESULT(ret);
}

/**
* vfi_xfer_create - create xfer object in location object tree
* @loc	: location where xfer is to be created
* @desc	: xfer parameter descriptor
*
* This function creates a new vfi_xfer object, described by @desc, 
* and binds it to the location @loc.
*
* If successful, one side-effect is a new entry in the location's kobject
* tree named for the new xfer.
*
**/
int vfi_xfer_create(struct vfi_xfer **xfer, struct vfi_location *loc, struct vfi_desc_param *desc)
{
	int ret = 0;
	VFI_DEBUG(MY_DEBUG,"%s %p %p\n",__FUNCTION__,loc,desc);

	*xfer = to_vfi_xfer(kset_find_obj(&loc->xfers->kset,desc->name));
	
	if (*xfer) {
		VFI_DEBUG(MY_DEBUG,"%s found %p %s locally in %p %s\n",__FUNCTION__,*xfer,desc->name,loc,loc->desc.name);
		vfi_xfer_put(*xfer);
	}
	else {
		ret = new_vfi_xfer(xfer,loc,desc);
		if (ret)
			*xfer = NULL;
	}

	return VFI_RESULT(ret);
}

void vfi_xfer_delete(struct vfi_xfer *xfer, struct vfi_desc_param *desc)
{
	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
	vfi_xfer_put(xfer);
}

void vfi_xfer_load_binds(struct vfi_xfer *xfer, struct vfi_bind *bind)
{
	VFI_DEBUG(MY_DEBUG,"%s %p %p\n",__FUNCTION__,xfer,bind);
	/* Added test, and diagnostics: this call sometimes fails, and Oops the kernel */
	if (xfer->desc.rde && xfer->desc.rde->ops && xfer->desc.rde->ops->link_bind) {
		xfer->desc.rde->ops->link_bind(NULL, bind);

		return;
	}
	VFI_DEBUG (MY_DEBUG, "xx %s: Xfer %s has incomplete operations set.\n", __FUNCTION__, kobject_name (&xfer->kset.kobj));
	VFI_DEBUG (MY_DEBUG, "   rde: %s\n", (xfer->desc.rde) ? "Present" : "Missing!");
	VFI_DEBUG (MY_DEBUG, "   rde ops: %s\n", (xfer->desc.rde && xfer->desc.rde->ops) ? "Present" : "Missing!");
	VFI_DEBUG (MY_DEBUG, "   rde link_bind op: %s\n", (xfer->desc.rde && xfer->desc.rde->ops && xfer->desc.rde->ops->link_bind) ? "Present" : "Missing!");
}


