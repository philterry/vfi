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

#define MY_DEBUG      RDDMA_DBG_SMB | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_SMB | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/rddma_smb.h>
#include <linux/rddma_parse.h>
#include <linux/rddma_drv.h>
#include <linux/rddma_smb.h>
#include <linux/rddma_smbs.h>
#include <linux/rddma_location.h>
#include <linux/rddma_src.h>
#include <linux/rddma_dst.h>
#include <linux/rddma_ops.h>
#include <linux/rddma_mmaps.h>


static void rddma_smb_release(struct kobject *kobj)
{
	struct rddma_smb *p = to_rddma_smb(kobj);
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,p);
	if (p->desc.name) {
		RDDMA_DEBUG(MY_LIFE_DEBUG,"%s name %p\n",__FUNCTION__,p->desc.name);
		kfree(p->desc.name);
	}
	if (p->pages)
		rddma_dealloc_pages(p->pages, p->num_pages);
	kfree(p);
}

struct rddma_smb_attribute {
	struct attribute attr;
	ssize_t (*show)(struct rddma_smb*, char *buffer);
	ssize_t (*store)(struct rddma_smb*, const char *buffer, size_t size);
};

#define RDDMA_SMB_ATTR(_name,_mode,_show,_store) struct rddma_smb_attribute rddma_smb_attr_##_name = {\
     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },\
     .show = _show,\
     .store = _store\
 };

static ssize_t rddma_smb_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
	struct rddma_smb_attribute *pattr = container_of(attr, struct rddma_smb_attribute, attr);
	struct rddma_smb *p = to_rddma_smb(kobj);

	if (pattr && pattr->show)
		return pattr->show(p,buffer);

	return 0;
}

static ssize_t rddma_smb_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
	struct rddma_smb_attribute *pattr = container_of(attr, struct rddma_smb_attribute, attr);
	struct rddma_smb *p = to_rddma_smb(kobj);

	if (pattr && pattr->store)
		return pattr->store(p, buffer, size);

	return 0;
}

static struct sysfs_ops rddma_smb_sysfs_ops = {
	.show = rddma_smb_show,
	.store = rddma_smb_store,
};


static ssize_t rddma_smb_default_show(struct rddma_smb *rddma_smb, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("Smb %p is %s \n",rddma_smb,rddma_smb ? rddma_smb->desc.name : NULL);
	if (rddma_smb) {
		ATTR_PRINTF("ops is %p rde is %p address is %p\n",rddma_smb->desc.ops,rddma_smb->desc.rde,rddma_smb->desc.address);
	}
	ATTR_PRINTF("refcount %d\n",atomic_read(&rddma_smb->kobj.kref.refcount));
	return size;
}

static ssize_t rddma_smb_default_store(struct rddma_smb *rddma_smb, const char *buffer, size_t size)
{
    return size;
}

RDDMA_SMB_ATTR(default, 0644, rddma_smb_default_show, rddma_smb_default_store);

static ssize_t rddma_smb_location_show(struct rddma_smb *rddma_smb, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("Smb %p is %s \n",rddma_smb,rddma_smb ? rddma_smb->desc.name : NULL);
	if (rddma_smb) {
		ATTR_PRINTF("ops is %p rde is %p address is %p\n",rddma_smb->desc.ops,rddma_smb->desc.rde,rddma_smb->desc.address);
	}
	return size;
}

static ssize_t rddma_smb_location_store(struct rddma_smb *rddma_smb, const char *buffer, size_t size)
{
    return size;
}

RDDMA_SMB_ATTR(location, 0644, rddma_smb_location_show, rddma_smb_location_store);

static ssize_t rddma_smb_name_show(struct rddma_smb *rddma_smb, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%s\n", rddma_smb->desc.name);
}

RDDMA_SMB_ATTR(name, 0444, rddma_smb_name_show, 0);

static ssize_t rddma_smb_offset_show(struct rddma_smb *rddma_smb, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%llx\n", rddma_smb->desc.offset);
}

RDDMA_SMB_ATTR(offset, 0444, rddma_smb_offset_show, 0);

static ssize_t rddma_smb_extent_show(struct rddma_smb *rddma_smb, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%x\n", rddma_smb->desc.extent);
}

RDDMA_SMB_ATTR(extent, 0444, rddma_smb_extent_show, 0);

static struct attribute *rddma_smb_default_attrs[] = {
    &rddma_smb_attr_default.attr,
    &rddma_smb_attr_location.attr,
    &rddma_smb_attr_name.attr,
    &rddma_smb_attr_offset.attr,
    &rddma_smb_attr_extent.attr,
    0,
};

struct kobj_type rddma_smb_type = {
	.release = rddma_smb_release,
	.sysfs_ops = &rddma_smb_sysfs_ops,
	.default_attrs = rddma_smb_default_attrs,
};

struct rddma_smb *new_rddma_smb(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct rddma_smb *new = kzalloc(sizeof(struct rddma_smb), GFP_KERNEL);
    
	if (NULL == new)
		goto out;

	rddma_clone_desc(&new->desc, desc);
	new->size = new->desc.extent;
	
	kobject_set_name(&new->kobj,"%s",new->desc.name);
	new->kobj.ktype = &rddma_smb_type;

	new->kobj.kset = &loc->smbs->kset;
	new->desc.ops = loc->desc.ops;
	new->desc.rde = loc->desc.rde;
out:
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,new);
	return new;
}

int rddma_smb_register(struct rddma_smb *rddma_smb)
{
	int ret = 0;

	if ( (ret = kobject_register(&rddma_smb->kobj) ) )
		goto out;

	if ( (rddma_smb->mmaps = new_rddma_mmaps(rddma_smb, "mmaps")) )
		if ( (ret = rddma_mmaps_register(rddma_smb->mmaps)) )
			goto mmaps;
	return ret;
mmaps:
	rddma_smb_unregister(rddma_smb);
out:
	return ret;
}

void rddma_smb_unregister(struct rddma_smb *rddma_smb)
{
	rddma_mmaps_unregister(rddma_smb->mmaps);
	kobject_unregister(&rddma_smb->kobj);
}

struct rddma_smb *find_rddma_smb(struct rddma_desc_param *desc)
{
	struct rddma_smb *smb = NULL;
	struct rddma_location *loc;

	loc = locate_rddma_location(NULL,desc);
	
	if (loc)
		smb = loc->desc.ops->smb_find(loc,desc);

	rddma_location_put(loc);

	return smb;
}

/**
* rddma_smb_create : Create RDDMA SMB kobject and isntall it in sysfs
*
* @loc:  pointer to rddma_location that specifies where the SMB is located
*        in the RDDMA network
* @desc: pointer to command string descriptor for the command we are 
*        servicing.
*
* This function creates a kobject to represent a new RDDMA shared-memory 
* buffer (SMB) and installs it in the RDDMA sysfs tree. It does NOT create
* the SMB itself.
*
**/
struct rddma_smb *rddma_smb_create(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct rddma_smb *smb = new_rddma_smb(loc,desc);

	if ( NULL == smb)
		goto out;

	if ( (rddma_smb_register(smb)) ) 
		goto fail_reg;
	
	return smb;

fail_reg:
	rddma_smb_put(smb);
out:
	return NULL;
}


void rddma_smb_delete(struct rddma_smb *smb)
{
	if (smb) {
		rddma_smb_unregister(smb);
	}
}

