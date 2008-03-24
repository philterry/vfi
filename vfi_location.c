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

#define MY_DEBUG      VFI_DBG_LOCATION | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_LOCATION | VFI_DBG_LIFE    | VFI_DBG_DEBUG
#define MY_ERROR      VFI_DBG_LOCATION | VFI_DBG_ERROR   | VFI_DBG_ERR

#include <linux/vfi_location.h>
#include <linux/vfi_drv.h>
#include <linux/vfi_parse.h>
#include <linux/vfi_ops.h>
#include <linux/vfi_smb.h>
#include <linux/vfi_smbs.h>
#include <linux/vfi_xfer.h>
#include <linux/vfi_xfers.h>
#include <linux/vfi_sync.h>
#include <linux/vfi_syncs.h>
#include <linux/vfi_dma.h>

#include <linux/slab.h>
#include <linux/module.h>

static void vfi_location_release(struct kobject *kobj)
{
    struct vfi_location *p = to_vfi_location(kobj);
    VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,p);
    vfi_address_unregister(p);
    vfi_clean_desc(&p->desc);
    kfree(p);
}

struct vfi_location_attribute {
    struct attribute attr;
    ssize_t (*show)(struct vfi_location*, char *buffer);
    ssize_t (*store)(struct vfi_location*, const char *buffer, size_t size);
};

#define VFI_LOCATION_ATTR(_name,_mode,_show,_store) struct vfi_location_attribute vfi_location_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t vfi_location_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct vfi_location_attribute *pattr = container_of(attr, struct vfi_location_attribute, attr);
    struct vfi_location *p = to_vfi_location(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t vfi_location_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct vfi_location_attribute *pattr = container_of(attr, struct vfi_location_attribute, attr);
    struct vfi_location *p = to_vfi_location(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops vfi_location_sysfs_ops = {
    .show = vfi_location_show,
    .store = vfi_location_store,
};


static ssize_t vfi_location_default_show(struct vfi_location *vfi_location, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("Location %p is %s \n",vfi_location,vfi_location ? vfi_location->desc.name : NULL);
	if (vfi_location) {
		ATTR_PRINTF("ops is %p rde is %p address is %p\n",vfi_location->desc.ops,vfi_location->desc.rde,vfi_location->desc.address);
	}
	ATTR_PRINTF("refcount %d\n",atomic_read(&vfi_location->kset.kobj.kref.refcount));
	return size;
}

static ssize_t vfi_location_default_store(struct vfi_location *vfi_location, const char *buffer, size_t size)
{
    return size;
}

VFI_LOCATION_ATTR(default, 0644, vfi_location_default_show, vfi_location_default_store);

static ssize_t vfi_location_location_show(struct vfi_location *vfi_location, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("%s\n",vfi_location->desc.location);
	return size;
}

static ssize_t vfi_location_location_store(struct vfi_location *vfi_location, const char *buffer, size_t size)
{
    return size;
}

VFI_LOCATION_ATTR(location, 0644, vfi_location_location_show, vfi_location_location_store);

static ssize_t vfi_location_name_show(struct vfi_location *vfi_location, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("%s\n",vfi_location->desc.name);
	return size;
}

static ssize_t vfi_location_name_store(struct vfi_location *vfi_location, const char *buffer, size_t size)
{
    return size;
}

VFI_LOCATION_ATTR(name, 0644, vfi_location_name_show, vfi_location_name_store);

static ssize_t vfi_location_id_show(struct vfi_location *vfi_location, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("%llx/%x\n",vfi_location->desc.offset,vfi_location->desc.extent);
	return size;
}

static ssize_t vfi_location_id_store(struct vfi_location *vfi_location, const char *buffer, size_t size)
{
    return size;
}

VFI_LOCATION_ATTR(id, 0644, vfi_location_id_show, vfi_location_id_store);

static ssize_t vfi_location_type_show(struct vfi_location *vfi_location, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("%s\n", vfi_location->desc.ops == &vfi_fabric_ops ? "public" : vfi_location->desc.ops == &vfi_local_ops ? "private" : "NULL");
	return size;
}

static ssize_t vfi_location_type_store(struct vfi_location *vfi_location, const char *buffer, size_t size)
{
    return size;
}

VFI_LOCATION_ATTR(type, 0644, vfi_location_type_show, vfi_location_type_store);

static struct attribute *vfi_location_default_attrs[] = {
    &vfi_location_attr_default.attr,
    &vfi_location_attr_location.attr,
    &vfi_location_attr_name.attr,
    &vfi_location_attr_id.attr,
    &vfi_location_attr_type.attr,
    0,
};

struct kobj_type vfi_location_type = {
    .release = vfi_location_release,
    .sysfs_ops = &vfi_location_sysfs_ops,
    .default_attrs = vfi_location_default_attrs,
};

int new_vfi_location(struct vfi_location **newloc, struct vfi_location *loc, struct vfi_desc_param *desc)
{
	struct vfi_location *new = kzalloc(sizeof(struct vfi_location), GFP_KERNEL);
 	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
   
	*newloc = new;

	if (NULL == new)
		return VFI_RESULT(-ENOMEM);

	vfi_clone_desc(&new->desc, desc);
	new->kset.kobj.ktype = &vfi_location_type;
	kobject_set_name(&new->kset.kobj, "%s", new->desc.name);

	/*
	* Parentage: provide pointer to parent kset in the /sys/vfi hierarchy.
	* Either we hook-up to a higher-level location, or we hook-up to /sys/vfi
	* itself.
	*
	* Actual hooking does not happen here - see vfi_location_register
	* for that.
	*/
	if (loc)
		new->kset.kobj.kset = &loc->kset;
	else
		new->kset.kobj.kset = &vfi_subsys->kset;

	/*
	* Node identifiers: the extent and offset fields in the descriptor
	* are overloaded in this context to provide two degrees of node
	* identification. Neither of which is documented.
	*
	* Just sayin' - these are not really extents or offsets.
	*/
	if (!new->desc.extent && loc)
		new->desc.extent = loc->desc.extent;

	if (!new->desc.offset && loc)
		new->desc.offset = loc->desc.offset;

	/*
	* Inherit core operations from parent location, or default to
	* fabric ops (default_ops(public))
	*
	*/
	if (!new->desc.ops ) {
		if (loc && loc->desc.ops)
			new->desc.ops = loc->desc.ops;
		else
			new->desc.ops = &vfi_fabric_ops;
	}

	/*
	* Inherit Remote DMA Engine from parent, or leave unspecified.
	*/
	if (!new->desc.rde) {
		if (loc && loc->desc.rde)
			new->desc.rde = vfi_dma_get(loc->desc.rde);
	}

	/*
	* Inherit fabric address ops from parent, or leave unspecified.
	* The "address" - struct vfi_fabric_address - is not an actual
	* address, but a set of ops for manipulating fabric addresses.
	*
	* JUST a thought: isn't all this inheritance stuff redundant
	* because parent loc->desc is cloned at the start of this
	* function?
	*/
	if (!new->desc.address) {
		if (loc && loc->desc.address)
			new->desc.address = vfi_fabric_get(loc->desc.address);
	}

	/*
	* Parentage. Again.
	*
	* This time an explicit link to the parent's vfi_location
	* structure, rather than the convoluted link to its kobject/kset
	* laid down earlier for the benefit of sysfs. This one is for us.
	*/
	new->desc.ploc = loc;

	kobject_init(&new->kset.kobj);
	INIT_LIST_HEAD(&new->kset.list);
	spin_lock_init(&new->kset.list_lock);

	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,new);
	return VFI_RESULT(0);
}

int vfi_location_register(struct vfi_location *vfi_location)
{
	int ret = 0;

	VFI_DEBUG(MY_DEBUG,"%s %p\n",__FUNCTION__,vfi_location);
	
	/*
	* Hook the new kobject into the sysfs hierarchy.
	*
	*/
	if ( (ret = kobject_add(&vfi_location->kset.kobj) ) )
		goto out;

/* 	kobject_uevent(&vfi_location->kset.kobj, KOBJ_ADD); */

	ret = -ENOMEM;

	/*
	* Create ksets for subsidiary SMBs and Xfers, and register those
	* too. Presume we don't create these when we create the new location
	* because we want to hook-up the new location first?
	*/
	ret= new_vfi_smbs(&vfi_location->smbs,"smbs",vfi_location);
	if ( NULL == vfi_location->smbs)
		goto fail_smbs;

	ret = new_vfi_xfers(&vfi_location->xfers,"xfers",vfi_location);
	if ( NULL == vfi_location->xfers)
		goto fail_xfers;

	ret = new_vfi_syncs(&vfi_location->syncs,"syncs",vfi_location);
	if ( NULL == vfi_location->syncs)
		goto fail_syncs;

	if ( (ret = vfi_smbs_register(vfi_location->smbs)) )
		goto fail_smbs_reg;

	if ( (ret = vfi_xfers_register(vfi_location->xfers)) )
		goto fail_xfers_reg;

	if ( (ret = vfi_syncs_register(vfi_location->syncs)) )
		goto fail_syncs_reg;

	return VFI_RESULT(ret);

fail_syncs_reg:
	vfi_xfers_unregister(vfi_location->xfers);
fail_xfers_reg:
	vfi_smbs_unregister(vfi_location->smbs);
fail_smbs_reg:
	kset_put(&vfi_location->xfers->kset);
fail_syncs:
	kset_put(&vfi_location->syncs->kset);
fail_xfers:
	kset_put(&vfi_location->smbs->kset);
fail_smbs:
	kobject_unregister(&vfi_location->kset.kobj);
out:
	return VFI_RESULT(ret);
}

void vfi_location_unregister(struct vfi_location *vfi_location)
{
	VFI_DEBUG(MY_DEBUG,"%s %p\n",__FUNCTION__,vfi_location);
	vfi_syncs_unregister(vfi_location->syncs);

	vfi_xfers_unregister(vfi_location->xfers);

	vfi_smbs_unregister(vfi_location->smbs);

	kobject_unregister(&vfi_location->kset.kobj);
}

int find_vfi_name(struct vfi_location **newloc, struct vfi_location *loc, struct vfi_desc_param *params)
{
	VFI_DEBUG(MY_DEBUG,"%s %p %p %s,%s\n",__FUNCTION__,loc,params,params->name,params->location);
	if (loc)
		*newloc = to_vfi_location(kset_find_obj(&loc->kset,params->name));
	else
		*newloc = to_vfi_location(kset_find_obj(&vfi_subsys->kset,params->name));
	VFI_DEBUG_SAFE(MY_DEBUG,*newloc,"%s -> %p %s,%s\n",__FUNCTION__,*newloc,(*newloc)->desc.name,(*newloc)->desc.location);
	return VFI_RESULT(*newloc != 0);
}

/**
* find_vfi_location - find, or create, a named location on the VFI network.
*
* @params: pointer to command string descriptor for the command we are 
*          currently servicing.
* 
* This function attempts to find an VFI kobject that represents a specified
* network location - whose name is cited in the @params command string - and 
* returns the address of an vfi_location structure that describes that location.
* 
* If no such kobject can be found, the function will create one, with all 
* necessary accoutrements, using vfi_location_create ().
*
* The function implements a recursive search for a given location, finding or
* creating its parent before the original target. A multi-component location of
* the form <a>.<b>.<c>... will therefore result in an inverted kobject
* tree of the form:
*			<c>
*			   <b>
*			      <a>
*
*
**/
int find_vfi_location(struct vfi_location **newloc, struct vfi_location *loc, struct vfi_desc_param *params)
{
	int ret = 0;
	*newloc = NULL;

	VFI_DEBUG(MY_DEBUG,"%s %p %p %s,%s\n",__FUNCTION__,loc,params,params->name,params->location);

	if (loc) {
		ret = loc->desc.ops->location_find(newloc,loc,params);
		VFI_DEBUG(MY_DEBUG,"%s %p %s %p %s -> %p\n",__FUNCTION__,loc,loc->desc.name,params,params->name,newloc);
		return VFI_RESULT(ret);
	}

	if (params->location && *params->location) {
		struct vfi_desc_param tmpparams;
		struct vfi_location *tmploc = NULL;

		if ( !vfi_parse_desc(&tmpparams,params->location) ) {
			VFI_DEBUG(MY_DEBUG,"%s %s,%s\n",__FUNCTION__,tmpparams.name,tmpparams.location);
			if ( (ret = find_vfi_location(&tmploc,loc,&tmpparams)) ) {
				ret = tmploc->desc.ops->location_find(newloc,tmploc,params);
				vfi_location_put(tmploc);
			}
			vfi_clean_desc(&tmpparams);
		}
	}
	else
		*newloc = to_vfi_location(kset_find_obj(&vfi_subsys->kset,params->name));

	VFI_DEBUG_SAFE(MY_DEBUG,*newloc,"%s -> %p %s,%s\n",__FUNCTION__,*newloc,(*newloc)->desc.name,(*newloc)->desc.location);
	return VFI_RESULT(*newloc != 0);
}

/**
* locate_vfi_location - find or create an VFI location within another. 
* @loc  - location to be searched
* @desc - parsed string containing the <loc-spec> to be found.
*
* Not at all an unfortunate choice of function name. Oh, no.
*
* I have no clear idea why this function exists. It is a wrapper for find_vfi_location
* that ensures that name, location, offset, and extent fields in @desc are preserved. 
*
* As find_vfi_location is itself a recursive function, one must assume that it is
* somewhat desctructive of name, location, offset, and extent?
*
**/
int locate_vfi_location(struct vfi_location **new_loc,struct vfi_location *loc, struct vfi_desc_param *desc)
{
	char *old_locstr, *old_namestr;
	char *new_locstr = NULL;
	u64 offset;
	unsigned int extent;
	int ret;

	VFI_DEBUG(MY_DEBUG,"%s %p %p %s,%s\n",__FUNCTION__,loc,desc,desc->name,desc->location);

	/*
	* Save the name, location, offset, and extent 
	* values contained in the @desc descriptor. 
	* This allows @desc contents to be modified 
	* temporarily.
	*/
	old_locstr = desc->location;
	old_namestr = desc->name;
	offset = desc->offset;
	extent = desc->extent;

	if (old_locstr) {
		new_locstr = strchr(desc->location, '.');
		desc->name = desc->location;

		if (new_locstr) {
			desc->location = new_locstr + 1;
			*new_locstr = '\0';
		}
		else {
			desc->location = NULL;
		}
	}

	ret = find_vfi_location(new_loc,loc,desc);

	/*
	* Restore original name, location, offset, and
	* extent values to the descriptor.
	*
	* Take care to eliminate any mangling of the 
	* original location string.
	*/
	desc->location = old_locstr;
	desc->name = old_namestr;
	desc->offset = offset;
	desc->extent = extent;
	if (new_locstr)
		*new_locstr = '.';

	return VFI_RESULT(ret);
}

int vfi_location_create(struct vfi_location **newloc, struct vfi_location *loc, struct vfi_desc_param *desc)
{
	int ret;

	VFI_DEBUG(MY_DEBUG,"%s %p %p\n",__FUNCTION__,loc,desc);

	ret = new_vfi_location(newloc,loc,desc);

	if (ret || NULL == *newloc)
		goto out;

	if ( ( vfi_location_register(*newloc)) )
		goto fail_reg;

	return VFI_RESULT(0);

fail_reg:
	vfi_location_put(*newloc);
out:
	return VFI_RESULT(-EINVAL);
}

void vfi_location_delete(struct vfi_location *loc)
{
	VFI_DEBUG(MY_DEBUG,"%s %p\n",__FUNCTION__,loc);
	if (loc) {
		vfi_location_unregister(loc);
		
		if (loc && loc->desc.rde)
			vfi_dma_put(loc->desc.rde);
		
		if (loc && loc->desc.address)
			vfi_fabric_put(loc->desc.address);
		
		vfi_location_put(loc);
	}
}
