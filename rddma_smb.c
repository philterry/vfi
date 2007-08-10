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

#include <linux/rddma_smb.h>
#include <linux/rddma_parse.h>
#include <linux/rddma_drv.h>
#include <linux/rddma_smb.h>
#include <linux/rddma_smbs.h>
#include <linux/rddma_location.h>
#include <linux/rddma_src.h>
#include <linux/rddma_dst.h>
#include <linux/rddma_ops.h>



static void rddma_smb_release(struct kobject *kobj)
{
	struct rddma_smb *p = to_rddma_smb(kobj);
	kfree(p->desc.name);
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
    return snprintf(buffer, PAGE_SIZE, "rddma_smb_default");
}

static ssize_t rddma_smb_default_store(struct rddma_smb *rddma_smb, const char *buffer, size_t size)
{
    return size;
}

RDDMA_SMB_ATTR(default, 0644, rddma_smb_default_show, rddma_smb_default_store);

static ssize_t rddma_smb_location_show(struct rddma_smb *rddma_smb, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%s\n", rddma_smb->desc.location);
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

static ssize_t rddma_smb_size_show(struct rddma_smb *rddma_smb, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%d\n", (int)rddma_smb->size);
}

RDDMA_SMB_ATTR(size, 0444, rddma_smb_size_show, 0);

static struct attribute *rddma_smb_default_attrs[] = {
    &rddma_smb_attr_default.attr,
    &rddma_smb_attr_location.attr,
    &rddma_smb_attr_name.attr,
    &rddma_smb_attr_size.attr,
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
	
	kobject_set_name(&new->kobj,"%s.%s",new->desc.name,new->desc.location);
	new->kobj.ktype = &rddma_smb_type;

	new->kobj.kset = &loc->smbs->kset;
	new->desc.ops = loc->desc.ops;
out:
	return new;
}

int rddma_smb_register(struct rddma_smb *rddma_smb)
{
	int ret = 0;

	if ( (ret = kobject_register(&rddma_smb->kobj) ) )
		goto out;

	ret = -ENOMEM;

	return ret;

out:
	rddma_smb_put(rddma_smb);
	return ret;
}

void rddma_smb_unregister(struct rddma_smb *rddma_smb)
{
     kobject_unregister(&rddma_smb->kobj);
}

struct rddma_smb *find_rddma_smb(struct rddma_desc_param *desc)
{
	struct rddma_smb *smb = NULL;
	struct rddma_location *loc;

	loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->location));
	
	if (loc)
		smb = loc->desc.ops->smb_find(loc,desc);

	rddma_location_put(loc);

	return smb;
}

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

