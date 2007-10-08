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

#include <linux/rddma_location.h>
#include <linux/rddma_drv.h>
#include <linux/rddma_parse.h>
#include <linux/rddma_ops.h>
#include <linux/rddma_smb.h>
#include <linux/rddma_smbs.h>
#include <linux/rddma_xfer.h>
#include <linux/rddma_xfers.h>
#include <linux/rddma_dma.h>

#include <linux/slab.h>
#include <linux/module.h>

static void rddma_location_release(struct kobject *kobj)
{
    struct rddma_location *p = to_rddma_location(kobj);
    rddma_clean_desc(&p->desc);
    if (p->desc.address)
	    rddma_fabric_put(p->desc.address);

    RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,p);
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

struct rddma_location *new_rddma_location(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct rddma_location *new = kzalloc(sizeof(struct rddma_location), GFP_KERNEL);
 	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
   
	if (NULL == new)
		goto out;
	rddma_clone_desc(&new->desc, desc);
	new->kobj.ktype = &rddma_location_type;
	if ( new->desc.name && *new->desc.name && new->desc.location && *new->desc.location)
		kobject_set_name(&new->kobj, "%s.%s", new->desc.name, new->desc.location);
	else if ( new->desc.name && *new->desc.name )
		kobject_set_name(&new->kobj, "%s", new->desc.name);
	else if (new->desc.location && *new->desc.location)
		kobject_set_name(&new->kobj, "%s", new->desc.location);
	else
		kobject_set_name(&new->kobj, "%s.%s", new->desc.name, new->desc.location);
	new->kobj.kset = &rddma_subsys->kset;
	if (!new->desc.ops ) {
		if (loc && loc->desc.ops)
			new->desc.ops = loc->desc.ops;
		else
			new->desc.ops = &rddma_fabric_ops;
	}

	if (!new->desc.rde) {
		if (loc && loc->desc.rde)
			new->desc.rde = rddma_dma_get(loc->desc.rde);
	}

	if (!new->desc.address) {
		if (loc && loc->desc.address)
			new->desc.address = rddma_fabric_get(loc->desc.address);
	}

	kobject_init(&new->kobj);
out:
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,new);
	return new;
}

int rddma_location_register(struct rddma_location *rddma_location)
{
	int ret = 0;

	if ( (ret = kobject_add(&rddma_location->kobj) ) )
		goto out;

	kobject_uevent(&rddma_location->kobj, KOBJ_ADD);

	ret = -ENOMEM;

	if ( NULL == (rddma_location->smbs = new_rddma_smbs("smbs",rddma_location)) )
		goto fail_smbs;

	if ( NULL == (rddma_location->xfers = new_rddma_xfers("xfers",rddma_location)) )
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
	kobject_unregister(&rddma_location->kobj);
out:
	return ret;
}

void rddma_location_unregister(struct rddma_location *rddma_location)
{
	rddma_xfers_unregister(rddma_location->xfers);

	rddma_smbs_unregister(rddma_location->smbs);

	kobject_unregister(&rddma_location->kobj);
}

struct rddma_location *find_rddma_name(struct rddma_desc_param *params)
{
	return to_rddma_location(kset_find_obj(&rddma_subsys->kset,params->name));
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
**/
struct rddma_location *find_rddma_location(struct rddma_desc_param *params)
{
	struct rddma_location *loc;

	if ( (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,params->name))) )
		return loc;

	/* Jimmy: this stops SIGSEGV when trying to find smb.unknown */
	if (!params->location)
		return NULL;

	if ( (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,params->location))) )
		return loc;

	if (params->location && *params->location && params->name != params->location) {
		struct rddma_desc_param tmpparams;
		struct rddma_location *tmploc = NULL;

		if ( !rddma_parse_desc(&tmpparams,params->location) ) {
			if ( (tmploc = find_rddma_location(&tmpparams)) ) {
				loc = tmploc->desc.ops->location_create(tmploc,params);
				rddma_location_put(tmploc);
			}
			rddma_clean_desc(&tmpparams);
		}
		return loc;
	}
	
	if (!*params->name)
		return rddma_location_create(NULL,params);

	return NULL;
}

struct rddma_location *rddma_location_create(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct rddma_location *new = new_rddma_location(loc,desc);

	if (NULL == new)
		goto out;

	if ( ( rddma_location_register(new)) )
		goto fail_reg;

	return new;

fail_reg:
	rddma_location_put(new);
out:
	return NULL;
}

void rddma_location_delete(struct rddma_location *loc)
{
	if (loc) {
		rddma_location_unregister(loc);
	}
}
