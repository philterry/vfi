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
	ATTR_PRINTF("ops is %p rde is %p address is %p ploc is %p\n",
		    vfi_location->desc.ops,vfi_location->desc.rde,vfi_location->desc.address,vfi_location->desc.ploc);
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
	int ret;
	struct kset *parent;

	struct vfi_location *new = kzalloc(sizeof(struct vfi_location), GFP_KERNEL);
 	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
   
	*newloc = new;

	if (NULL == new)
		return VFI_RESULT(-ENOMEM);

	vfi_clone_desc(&new->desc, desc);

	if (loc)
		parent = &loc->kset;
	else
		parent = &vfi_subsys->kset;

	if (!new->desc.extent && loc)
		new->desc.extent = loc->desc.extent;

	if (!new->desc.offset && loc)
		new->desc.offset = loc->desc.offset;

	if (!new->desc.ops ) {
		if (loc && loc->desc.ops)
			new->desc.ops = loc->desc.ops;
		else
			new->desc.ops = &vfi_fabric_ops;
	}

	if (!new->desc.rde) {
		if (loc && loc->desc.rde)
			new->desc.rde = vfi_dma_get(loc->desc.rde);
	}

	if (!new->desc.address) {
		if (loc && loc->desc.address)
			new->desc.address = vfi_fabric_get(loc->desc.address);
	}

	if (!new->desc.ploc)
		new->desc.ploc = vfi_location_get(loc);

	kobject_set_name(&new->kset.kobj,"%s", new->desc.name);
	new->kset.kobj.ktype = &vfi_location_type;
	new->kset.kobj.kset = parent;
	ret = kset_register(&new->kset);

	if (ret)
		goto out;

	ret= new_vfi_smbs(&new->smbs,"smbs",new);
	if (ret)
		goto fail_smbs;

	ret = new_vfi_xfers(&new->xfers,"xfers",new);
	if (ret)
		goto fail_xfers;

	ret = new_vfi_syncs(&new->syncs,"syncs",new);
	if (ret)
		goto fail_syncs;


	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,*newloc);
	return VFI_RESULT(ret);

fail_syncs:
	vfi_xfers_put(new->xfers);

fail_xfers:
	vfi_smbs_put(new->smbs);

fail_smbs:
out:
	vfi_location_put(new);
	*newloc = NULL;
	return VFI_RESULT(ret);
}

int find_vfi_name(struct vfi_location **newloc, struct vfi_location *loc, struct vfi_desc_param *params)
{
	VFI_DEBUG(MY_DEBUG,"%s %p %p %s,%s\n",__FUNCTION__,loc,params,params->name,params->location);
	if (loc)
		*newloc = to_vfi_location(kset_find_obj(&loc->kset,params->name));
	else
		*newloc = to_vfi_location(kset_find_obj(&vfi_subsys->kset,params->name));
	VFI_DEBUG_SAFE(MY_DEBUG,*newloc,"%s -> %p %s,%s\n",__FUNCTION__,*newloc,(*newloc)->desc.name,(*newloc)->desc.location);
	if (*newloc)
		return VFI_RESULT(0);
	return VFI_RESULT(-EINVAL);
}

/**
* find_vfi_location - find a named location on the VFI network.
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
* The function implements a recursive search for a given location,
* findingits parent before the original target. A multi-component
* location of the form <a>.<b>.<c>... will therefore result in an
* inverted kobject tree of the form:
*			<c>
*			   <b>
*			      <a>
*
* If one of the antecedents is a remote location this function will
* appear to create local shadow locations as a side effect. However,
* no actual location is created by this function.
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
			if ( !(ret = find_vfi_location(&tmploc,loc,&tmpparams)) ) {
				ret = tmploc->desc.ops->location_find(newloc,tmploc,params);
				vfi_location_put(tmploc);
			}
			vfi_clean_desc(&tmpparams);
		}
	}
	else
		*newloc = to_vfi_location(kset_find_obj(&vfi_subsys->kset,params->name));

	VFI_DEBUG_SAFE(MY_DEBUG,*newloc,"%s -> %p %s,%s\n",__FUNCTION__,*newloc,(*newloc)->desc.name,(*newloc)->desc.location);
	if (*newloc)
		return VFI_RESULT(0);
	return VFI_RESULT(-EINVAL);
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

	return VFI_RESULT(ret);
}

void vfi_location_delete(struct vfi_location *loc)
{
	VFI_DEBUG(MY_DEBUG,"%s %p\n",__FUNCTION__,loc);
	if (loc) {
		vfi_smbs_put(loc->smbs);
		vfi_xfers_put(loc->xfers);
		vfi_syncs_put(loc->syncs);
	}
	vfi_location_put(loc);
}
