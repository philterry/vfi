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

#define MY_DEBUG      VFI_DBG_XFER | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_XFER | VFI_DBG_LIFE    | VFI_DBG_DEBUG

#include <linux/vfi_xfer.h>
#include <linux/vfi_parse.h>
#include <linux/vfi_location.h>
#include <linux/vfi_xfers.h>
#include <linux/vfi_ops.h>
#include <linux/vfi_binds.h>
#include <linux/vfi_bind.h>
#include <linux/vfi_dma.h>

#include <linux/slab.h>
#include <linux/module.h>

static void vfi_xfer_release(struct kobject *kobj)
{
    struct vfi_xfer *p = to_vfi_xfer(kobj);
    VFI_DEBUG(MY_LIFE_DEBUG,"XXX %s %p (refc %lx)\n", __FUNCTION__, p, (unsigned long)kobj->kref.refcount.counter);
    if (p->desc.name) {
	    VFI_DEBUG(MY_LIFE_DEBUG,"%s free xfer %p\n",__FUNCTION__,p->desc.name);
	    vfi_clean_desc(&p->desc);
    }
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
	if (vfi_xfer) {
		ATTR_PRINTF("xfer: ops is %p rde is %p address is %p\n",vfi_xfer->desc.ops,vfi_xfer->desc.rde,vfi_xfer->desc.address);
	}
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
	struct vfi_xfer *new = kzalloc(sizeof(struct vfi_xfer), GFP_KERNEL);
    
	*xfer = new;
	
	if (NULL == new)
		return -ENOMEM;

	vfi_clone_desc(&new->desc, desc);
	new->kobj.ktype = &vfi_xfer_type;
	kobject_set_name(&new->kobj,"%s", desc->name);

	new->kobj.kset = &parent->xfers->kset;		/* Pointer to location xfers kset, where this xfer will later hang */
	new->desc.ops = parent->desc.ops;		/* Pointer to location core ops */
	new->desc.address = parent->desc.address;	/* Pointer to location address ops */
	new->desc.rde = parent->desc.rde;		/* Pointer to location DMA engine ops */
	new->desc.ploc = parent;			/* Pointer to complete location object */

	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,new);
	return 0;
}

/**
* vfi_xfer_register - Register an xfer object with sysfs tree
* @xfer : xfer object to be registered.
*
* This function "registers" a new xfer object, to install it within the
* local VFI object hierarchy and the sysfs tree.
*
* Once the core xfer object has been registered with sysfs, a "binds/"
* subdirectory (kset) is created for the xfer and registered too.
*
**/
int vfi_xfer_register(struct vfi_xfer *vfi_xfer)
{
    int ret = 0;

    if ( (ret = kobject_register(&vfi_xfer->kobj) ) ) {
	    vfi_xfer_put(vfi_xfer);
	    goto out;
    }

    ret = new_vfi_binds(&vfi_xfer->binds,"binds",vfi_xfer);
    if (ret) 
	    goto fail_binds;

    ret = -ENOMEM;

    if ( NULL == vfi_xfer->binds)
	    goto fail_binds;

    if ( (ret = vfi_binds_register(vfi_xfer->binds)) )
	    goto fail_binds_reg;

    return ret;

fail_binds_reg:
    vfi_binds_put(vfi_xfer->binds);
fail_binds:
    kobject_unregister(&vfi_xfer->kobj);
out:
    return ret;
}

void vfi_xfer_unregister(struct vfi_xfer *vfi_xfer)
{
	if (vfi_xfer) {
		vfi_binds_unregister(vfi_xfer->binds);
		kobject_unregister(&vfi_xfer->kobj);
	}
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
	VFI_DEBUG(MY_DEBUG,"%s desc(%p) ploc(%p)\n",__FUNCTION__,desc,desc->ploc);

	/*
	* If an explicit location has been specified for the target xfer, 
	* then use that location's xfer_find op to complete the search.
	*/
	if (loc && loc->desc.ops && loc->desc.ops->xfer_find)
		return loc->desc.ops->xfer_find(xfer,loc,desc);

	/*
	* If a prior location has been found for this descriptor - if its ploc
	* field is non-zero - then invoke the xfer_find op for that location.
	*/
	if (desc->ploc && desc->ploc->desc.ops && desc->ploc->desc.ops->xfer_find)
		return desc->ploc->desc.ops->xfer_find(xfer,desc->ploc,desc);

	/*
	* If we reach here, it means that we do not yet know where to look
	* for the xfer. So find out where the xfer lives, and then use the
	* resultant location xfer_find op to find the xfer object itself.
	*/
	ret = locate_vfi_location(&loc,NULL,desc);
	if (ret)
		return ret;

	/*
	* Save the result of the location search in the xfer
	* descriptor. 
	*/
	desc->ploc = loc;

	if (loc && loc->desc.ops && loc->desc.ops->xfer_find) 
		return loc->desc.ops->xfer_find(xfer,loc,desc);

	return -EINVAL;
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
	int ret;
	VFI_DEBUG(MY_DEBUG,"%s %p %p\n",__FUNCTION__,loc,desc);

	/*
	* First, check that no such xfer entry already exists at this 
	* location. If one does - one with the same name - then do not
	* create a new xfer, just return a pointer to the extant xfer.
	*/
	*xfer = to_vfi_xfer(kset_find_obj(&loc->xfers->kset,desc->name));
	
	if (*xfer) {
		VFI_DEBUG(MY_DEBUG,"%s found %p %s locally in %p %s\n",__FUNCTION__,*xfer,desc->name,loc,loc->desc.name);
	}
	/*
	* If no existing xfer kobject could be found with that name,
	* then create one here and bind it to the location.
	*
	*/
	else {
		ret = new_vfi_xfer(xfer,loc,desc);
		if (ret)
			return ret;
		if (*xfer)
			if ( (ret = vfi_xfer_register(*xfer)))
				return ret;
	}

	return 0;
}

void vfi_xfer_delete(struct vfi_location *loc, struct vfi_desc_param *desc)
{
	struct vfi_xfer *xfer;
	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
	xfer = to_vfi_xfer(kset_find_obj(&loc->xfers->kset,desc->name));
	vfi_xfer_put(xfer);
	vfi_xfer_unregister(xfer);
}

void vfi_xfer_load_binds(struct vfi_xfer *xfer, struct vfi_bind *bind)
{
	VFI_DEBUG(MY_DEBUG,"%s %p %p\n",__FUNCTION__,xfer,bind);
	/* Added test, and diagnostics: this call sometimes fails, and Oops the kernel */
	if (xfer->desc.rde && xfer->desc.rde->ops && xfer->desc.rde->ops->link_bind) {
		xfer->desc.rde->ops->link_bind(NULL, bind);

		return;
	}
	VFI_DEBUG (MY_DEBUG, "xx %s: Xfer %s has incomplete operations set.\n", __FUNCTION__, kobject_name (&xfer->kobj));
	VFI_DEBUG (MY_DEBUG, "   rde: %s\n", (xfer->desc.rde) ? "Present" : "Missing!");
	VFI_DEBUG (MY_DEBUG, "   rde ops: %s\n", (xfer->desc.rde && xfer->desc.rde->ops) ? "Present" : "Missing!");
	VFI_DEBUG (MY_DEBUG, "   rde link_bind op: %s\n", (xfer->desc.rde && xfer->desc.rde->ops && xfer->desc.rde->ops->link_bind) ? "Present" : "Missing!");
}


