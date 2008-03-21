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

#define MY_DEBUG      RDDMA_DBG_LOCATION | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_LOCATION | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/vfi_location.h>
#include <linux/vfi_drv.h>
#include <linux/vfi_parse.h>
#include <linux/vfi_ops.h>
#include <linux/vfi_smb.h>
#include <linux/vfi_smbs.h>
#include <linux/vfi_xfer.h>
#include <linux/vfi_xfers.h>
#include <linux/vfi_dma.h>

#include <linux/slab.h>
#include <linux/module.h>

static void rddma_location_release(struct kobject *kobj)
{
    struct rddma_location *p = to_rddma_location(kobj);
    RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,p);
    rddma_address_unregister(p);
    rddma_clean_desc(&p->desc);
    kfree(p);
}

struct rddma_location_attribute {
    struct attribute attr;
    ssize_t (*show)(struct rddma_location*, char *buffer);
    ssize_t (*store)(struct rddma_location*, const char *buffer, size_t size);
};

#define RDDMA_LOCATION_ATTR(_name,_mode,_show,_store) struct rddma_location_attribute rddma_location_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t rddma_location_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct rddma_location_attribute *pattr = container_of(attr, struct rddma_location_attribute, attr);
    struct rddma_location *p = to_rddma_location(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t rddma_location_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct rddma_location_attribute *pattr = container_of(attr, struct rddma_location_attribute, attr);
    struct rddma_location *p = to_rddma_location(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops rddma_location_sysfs_ops = {
    .show = rddma_location_show,
    .store = rddma_location_store,
};


static ssize_t rddma_location_default_show(struct rddma_location *rddma_location, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("Location %p is %s \n",rddma_location,rddma_location ? rddma_location->desc.name : NULL);
	if (rddma_location) {
		ATTR_PRINTF("ops is %p rde is %p address is %p\n",rddma_location->desc.ops,rddma_location->desc.rde,rddma_location->desc.address);
	}
	ATTR_PRINTF("refcount %d\n",atomic_read(&rddma_location->kset.kobj.kref.refcount));
	return size;
}

static ssize_t rddma_location_default_store(struct rddma_location *rddma_location, const char *buffer, size_t size)
{
    return size;
}

RDDMA_LOCATION_ATTR(default, 0644, rddma_location_default_show, rddma_location_default_store);

static ssize_t rddma_location_location_show(struct rddma_location *rddma_location, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("%s\n",rddma_location->desc.location);
	return size;
}

static ssize_t rddma_location_location_store(struct rddma_location *rddma_location, const char *buffer, size_t size)
{
    return size;
}

RDDMA_LOCATION_ATTR(location, 0644, rddma_location_location_show, rddma_location_location_store);

static ssize_t rddma_location_name_show(struct rddma_location *rddma_location, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("%s\n",rddma_location->desc.name);
	return size;
}

static ssize_t rddma_location_name_store(struct rddma_location *rddma_location, const char *buffer, size_t size)
{
    return size;
}

RDDMA_LOCATION_ATTR(name, 0644, rddma_location_name_show, rddma_location_name_store);

static ssize_t rddma_location_id_show(struct rddma_location *rddma_location, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("%llx/%x\n",rddma_location->desc.offset,rddma_location->desc.extent);
	return size;
}

static ssize_t rddma_location_id_store(struct rddma_location *rddma_location, const char *buffer, size_t size)
{
    return size;
}

RDDMA_LOCATION_ATTR(id, 0644, rddma_location_id_show, rddma_location_id_store);

static ssize_t rddma_location_type_show(struct rddma_location *rddma_location, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("%s\n", rddma_location->desc.ops == &rddma_fabric_ops ? "public" : rddma_location->desc.ops == &rddma_local_ops ? "private" : "NULL");
	return size;
}

static ssize_t rddma_location_type_store(struct rddma_location *rddma_location, const char *buffer, size_t size)
{
    return size;
}

RDDMA_LOCATION_ATTR(type, 0644, rddma_location_type_show, rddma_location_type_store);

static struct attribute *rddma_location_default_attrs[] = {
    &rddma_location_attr_default.attr,
    &rddma_location_attr_location.attr,
    &rddma_location_attr_name.attr,
    &rddma_location_attr_id.attr,
    &rddma_location_attr_type.attr,
    0,
};

struct kobj_type rddma_location_type = {
    .release = rddma_location_release,
    .sysfs_ops = &rddma_location_sysfs_ops,
    .default_attrs = rddma_location_default_attrs,
};

int new_rddma_location(struct rddma_location **newloc, struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct rddma_location *new = kzalloc(sizeof(struct rddma_location), GFP_KERNEL);
 	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
   
	*newloc = new;

	if (NULL == new)
		return -ENOMEM;

	rddma_clone_desc(&new->desc, desc);
	new->kset.kobj.ktype = &rddma_location_type;
	kobject_set_name(&new->kset.kobj, "%s", new->desc.name);

	/*
	* Parentage: provide pointer to parent kset in the /sys/rddma hierarchy.
	* Either we hook-up to a higher-level location, or we hook-up to /sys/rddma
	* itself.
	*
	* Actual hooking does not happen here - see rddma_location_register
	* for that.
	*/
	if (loc)
		new->kset.kobj.kset = &loc->kset;
	else
		new->kset.kobj.kset = &rddma_subsys->kset;

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
			new->desc.ops = &rddma_fabric_ops;
	}

	/*
	* Inherit Remote DMA Engine from parent, or leave unspecified.
	*/
	if (!new->desc.rde) {
		if (loc && loc->desc.rde)
			new->desc.rde = rddma_dma_get(loc->desc.rde);
	}

	/*
	* Inherit fabric address ops from parent, or leave unspecified.
	* The "address" - struct rddma_fabric_address - is not an actual
	* address, but a set of ops for manipulating fabric addresses.
	*
	* JUST a thought: isn't all this inheritance stuff redundant
	* because parent loc->desc is cloned at the start of this
	* function?
	*/
	if (!new->desc.address) {
		if (loc && loc->desc.address)
			new->desc.address = rddma_fabric_get(loc->desc.address);
	}

	/*
	* Parentage. Again.
	*
	* This time an explicit link to the parent's rddma_location
	* structure, rather than the convoluted link to its kobject/kset
	* laid down earlier for the benefit of sysfs. This one is for us.
	*/
	new->desc.ploc = loc;

	kobject_init(&new->kset.kobj);
	INIT_LIST_HEAD(&new->kset.list);
	spin_lock_init(&new->kset.list_lock);

	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,new);
	return 0;
}

int rddma_location_register(struct rddma_location *rddma_location)
{
	int ret = 0;

	RDDMA_DEBUG(MY_DEBUG,"%s %p\n",__FUNCTION__,rddma_location);
	
	/*
	* Hook the new kobject into the sysfs hierarchy.
	*
	*/
	if ( (ret = kobject_add(&rddma_location->kset.kobj) ) )
		goto out;

/* 	kobject_uevent(&rddma_location->kset.kobj, KOBJ_ADD); */

	ret = -ENOMEM;

	/*
	* Create ksets for subsidiary SMBs and Xfers, and register those
	* too. Presume we don't create these when we create the new location
	* because we want to hook-up the new location first?
	*/
	ret= new_rddma_smbs(&rddma_location->smbs,"smbs",rddma_location);
	if ( NULL == rddma_location->smbs)
		goto fail_smbs;

	ret = new_rddma_xfers(&rddma_location->xfers,"xfers",rddma_location);
	if ( NULL == rddma_location->xfers)
		goto fail_xfers;

	if ( (ret = rddma_smbs_register(rddma_location->smbs)) )
		goto fail_smbs_reg;

	if ( (ret = rddma_xfers_register(rddma_location->xfers)) )
		goto fail_xfers_reg;

	return ret;

fail_xfers_reg:
	rddma_smbs_unregister(rddma_location->smbs);
fail_smbs_reg:
	kset_put(&rddma_location->xfers->kset);
fail_xfers:
	kset_put(&rddma_location->smbs->kset);
fail_smbs:
	kobject_unregister(&rddma_location->kset.kobj);
out:
	return ret;
}

void rddma_location_unregister(struct rddma_location *rddma_location)
{
	RDDMA_DEBUG(MY_DEBUG,"%s %p\n",__FUNCTION__,rddma_location);
	rddma_xfers_unregister(rddma_location->xfers);

	rddma_smbs_unregister(rddma_location->smbs);

	kobject_unregister(&rddma_location->kset.kobj);
}

int find_rddma_name(struct rddma_location **newloc, struct rddma_location *loc, struct rddma_desc_param *params)
{
	RDDMA_DEBUG(MY_DEBUG,"%s %p %p %s,%s\n",__FUNCTION__,loc,params,params->name,params->location);
	if (loc)
		*newloc = to_rddma_location(kset_find_obj(&loc->kset,params->name));
	else
		*newloc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,params->name));
	RDDMA_DEBUG_SAFE(MY_DEBUG,*newloc,"%s -> %p %s,%s\n",__FUNCTION__,*newloc,(*newloc)->desc.name,(*newloc)->desc.location);
	return *newloc != 0;
}

/**
* find_rddma_location - find, or create, a named location on the RDDMA network.
*
* @params: pointer to command string descriptor for the command we are 
*          currently servicing.
* 
* This function attempts to find an RDDMA kobject that represents a specified
* network location - whose name is cited in the @params command string - and 
* returns the address of an rddma_location structure that describes that location.
* 
* If no such kobject can be found, the function will create one, with all 
* necessary accoutrements, using rddma_location_create ().
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
int find_rddma_location(struct rddma_location **newloc, struct rddma_location *loc, struct rddma_desc_param *params)
{
	int ret = 0;
	*newloc = NULL;

	RDDMA_DEBUG(MY_DEBUG,"%s %p %p %s,%s\n",__FUNCTION__,loc,params,params->name,params->location);

	if (loc) {
		ret = loc->desc.ops->location_find(newloc,loc,params);
		RDDMA_DEBUG(MY_DEBUG,"%s %p %s %p %s -> %p\n",__FUNCTION__,loc,loc->desc.name,params,params->name,newloc);
		return ret;
	}

	if (params->location && *params->location) {
		struct rddma_desc_param tmpparams;
		struct rddma_location *tmploc = NULL;

		if ( !rddma_parse_desc(&tmpparams,params->location) ) {
			RDDMA_DEBUG(MY_DEBUG,"%s %s,%s\n",__FUNCTION__,tmpparams.name,tmpparams.location);
			if ( (ret = find_rddma_location(&tmploc,loc,&tmpparams)) ) {
				ret = tmploc->desc.ops->location_find(newloc,tmploc,params);
				rddma_location_put(tmploc);
			}
			rddma_clean_desc(&tmpparams);
		}
	}
	else
		*newloc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,params->name));

	RDDMA_DEBUG_SAFE(MY_DEBUG,*newloc,"%s -> %p %s,%s\n",__FUNCTION__,*newloc,(*newloc)->desc.name,(*newloc)->desc.location);
	return *newloc != 0;
}

/**
* locate_rddma_location - find or create an RDDMA location within another. 
* @loc  - location to be searched
* @desc - parsed string containing the <loc-spec> to be found.
*
* Not at all an unfortunate choice of function name. Oh, no.
*
* I have no clear idea why this function exists. It is a wrapper for find_rddma_location
* that ensures that name, location, offset, and extent fields in @desc are preserved. 
*
* As find_rddma_location is itself a recursive function, one must assume that it is
* somewhat desctructive of name, location, offset, and extent?
*
**/
int locate_rddma_location(struct rddma_location **new_loc,struct rddma_location *loc, struct rddma_desc_param *desc)
{
	char *old_locstr, *old_namestr;
	char *new_locstr = NULL;
	u64 offset;
	unsigned int extent;
	int ret;

	RDDMA_DEBUG(MY_DEBUG,"%s %p %p %s,%s\n",__FUNCTION__,loc,desc,desc->name,desc->location);

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

	ret = find_rddma_location(new_loc,loc,desc);

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

	return ret;
}

int rddma_location_create(struct rddma_location **newloc, struct rddma_location *loc, struct rddma_desc_param *desc)
{
	int ret;

	RDDMA_DEBUG(MY_DEBUG,"%s %p %p\n",__FUNCTION__,loc,desc);

	ret = new_rddma_location(newloc,loc,desc);

	if (ret || NULL == *newloc)
		goto out;

	if ( ( rddma_location_register(*newloc)) )
		goto fail_reg;

	return 0;

fail_reg:
	rddma_location_put(*newloc);
out:
	return -EINVAL;
}

void rddma_location_delete(struct rddma_location *loc)
{
	RDDMA_DEBUG(MY_DEBUG,"%s %p\n",__FUNCTION__,loc);
	if (loc) {
		rddma_location_unregister(loc);
		
		if (loc && loc->desc.rde)
			rddma_dma_put(loc->desc.rde);
		
		if (loc && loc->desc.address)
			rddma_fabric_put(loc->desc.address);
		
		rddma_location_put(loc);
	}
}
