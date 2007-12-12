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

#define MY_DEBUG      RDDMA_DBG_XFER | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_XFER | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/rddma_xfer.h>
#include <linux/rddma_parse.h>
#include <linux/rddma_location.h>
#include <linux/rddma_xfers.h>
#include <linux/rddma_ops.h>
#include <linux/rddma_binds.h>
#include <linux/rddma_bind.h>
#include <linux/rddma_dma.h>

#include <linux/slab.h>
#include <linux/module.h>

static void rddma_xfer_release(struct kobject *kobj)
{
    struct rddma_xfer *p = to_rddma_xfer(kobj);
    RDDMA_DEBUG(MY_LIFE_DEBUG,"XXX %s %p (refc %lx)\n", __FUNCTION__, p, (unsigned long)kobj->kref.refcount.counter);
    if (p->desc.name) {
	    RDDMA_DEBUG(MY_LIFE_DEBUG,"%s free xfer %p\n",__FUNCTION__,p->desc.name);
	    rddma_clean_desc(&p->desc);
    }
    kfree(p);
}

struct rddma_xfer_attribute {
    struct attribute attr;
    ssize_t (*show)(struct rddma_xfer*, char *buffer);
    ssize_t (*store)(struct rddma_xfer*, const char *buffer, size_t size);
};

#define RDDMA_XFER_ATTR(_name,_mode,_show,_store) struct rddma_xfer_attribute rddma_xfer_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t rddma_xfer_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct rddma_xfer_attribute *pattr = container_of(attr, struct rddma_xfer_attribute, attr);
    struct rddma_xfer *p = to_rddma_xfer(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t rddma_xfer_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct rddma_xfer_attribute *pattr = container_of(attr, struct rddma_xfer_attribute, attr);
    struct rddma_xfer *p = to_rddma_xfer(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops rddma_xfer_sysfs_ops = {
    .show = rddma_xfer_show,
    .store = rddma_xfer_store,
};


static ssize_t rddma_xfer_default_show(struct rddma_xfer *rddma_xfer, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("Xfer %p is %s \n",rddma_xfer,rddma_xfer ? rddma_xfer->desc.name : NULL);
	if (rddma_xfer) {
		ATTR_PRINTF("xfer: ops is %p rde is %p address is %p\n",rddma_xfer->desc.ops,rddma_xfer->desc.rde,rddma_xfer->desc.address);
	}
	return size;
}

static ssize_t rddma_xfer_default_store(struct rddma_xfer *rddma_xfer, const char *buffer, size_t size)
{
    return size;
}

RDDMA_XFER_ATTR(default, 0644, rddma_xfer_default_show, rddma_xfer_default_store);

static ssize_t rddma_xfer_location_show(struct rddma_xfer *rddma_xfer, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%s\n",rddma_xfer->desc.location);
}

static ssize_t rddma_xfer_location_store(struct rddma_xfer *rddma_xfer, const char *buffer, size_t size)
{
    return size;
}

RDDMA_XFER_ATTR(location, 0644, rddma_xfer_location_show, rddma_xfer_location_store);

static ssize_t rddma_xfer_name_show(struct rddma_xfer *rddma_xfer, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%s\n", rddma_xfer->desc.name);
}

static ssize_t rddma_xfer_name_store(struct rddma_xfer *rddma_xfer, const char *buffer, size_t size)
{
    return size;
}

RDDMA_XFER_ATTR(name, 0644, rddma_xfer_name_show, rddma_xfer_name_store);

static ssize_t rddma_xfer_extent_show(struct rddma_xfer *rddma_xfer, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%x\n",rddma_xfer->desc.extent);
}

static ssize_t rddma_xfer_extent_store(struct rddma_xfer *rddma_xfer, const char *buffer, size_t size)
{
    return size;
}

RDDMA_XFER_ATTR(extent, 0644, rddma_xfer_extent_show, rddma_xfer_extent_store);

static ssize_t rddma_xfer_offset_show(struct rddma_xfer *rddma_xfer, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%llx\n",rddma_xfer->desc.offset);
}

static ssize_t rddma_xfer_offset_store(struct rddma_xfer *rddma_xfer, const char *buffer, size_t size)
{
    return size;
}

RDDMA_XFER_ATTR(offset, 0644, rddma_xfer_offset_show, rddma_xfer_offset_store);

static struct attribute *rddma_xfer_default_attrs[] = { 
    &rddma_xfer_attr_default.attr,
    &rddma_xfer_attr_location.attr,
    &rddma_xfer_attr_name.attr,
    &rddma_xfer_attr_extent.attr,
    &rddma_xfer_attr_offset.attr,
    0,
};

struct kobj_type rddma_xfer_type = {
    .release = rddma_xfer_release,
    .sysfs_ops = &rddma_xfer_sysfs_ops,
    .default_attrs = rddma_xfer_default_attrs,
};

struct rddma_xfer *new_rddma_xfer(struct rddma_location *parent, struct rddma_desc_param *desc)
{
	struct rddma_xfer *new = kzalloc(sizeof(struct rddma_xfer), GFP_KERNEL);
    
	if (NULL == new)
		goto out;

	rddma_clone_desc(&new->desc, desc);
	new->kobj.ktype = &rddma_xfer_type;
	kobject_set_name(&new->kobj,"%s", desc->name);

	new->kobj.kset = &parent->xfers->kset;
	new->desc.ops = parent->desc.ops;
	new->desc.address = parent->desc.address;
	new->desc.rde = parent->desc.rde;
	new->desc.ploc = parent;

#ifdef SERIALIZE_BIND_PROCESSING  
	/* dma chain headed at transfer level, as all binds are linked
	 * into one continuous chain 
	 * */
	INIT_LIST_HEAD(&new->dma_chain);
#endif
out:
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,new);
	return new;
}

int rddma_xfer_register(struct rddma_xfer *rddma_xfer)
{
    int ret = 0;

    if ( (ret = kobject_register(&rddma_xfer->kobj) ) ) {
	    rddma_xfer_put(rddma_xfer);
	    goto out;
    }

    ret = -ENOMEM;

    if ( NULL == (rddma_xfer->binds = new_rddma_binds("binds",rddma_xfer)))
	    goto fail_binds;

    if ( (ret = rddma_binds_register(rddma_xfer->binds)) )
	    goto fail_binds_reg;

    return ret;

fail_binds_reg:
    rddma_binds_put(rddma_xfer->binds);
fail_binds:
    kobject_unregister(&rddma_xfer->kobj);
out:
    return ret;
}

void rddma_xfer_unregister(struct rddma_xfer *rddma_xfer)
{
	if (rddma_xfer) {
		rddma_binds_unregister(rddma_xfer->binds);
		kobject_unregister(&rddma_xfer->kobj);
	}
}

struct rddma_xfer *find_rddma_xfer(struct rddma_desc_param *desc)
{
	struct rddma_xfer *xfer = NULL;
	struct rddma_location *loc;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	if (desc->ploc)
		loc = desc->ploc;
	else
		loc = locate_rddma_location(NULL,desc);

	if (loc && loc->desc.ops && loc->desc.ops->xfer_find)
		xfer = loc->desc.ops->xfer_find(loc,desc);

	desc->ploc = loc;

	return xfer;
}

struct rddma_xfer *rddma_xfer_create(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct rddma_xfer *new;
	RDDMA_DEBUG(MY_DEBUG,"%s %p %p\n",__FUNCTION__,loc,desc);

	new = to_rddma_xfer(kset_find_obj(&loc->xfers->kset,desc->name));
	
	if (new) {
		RDDMA_DEBUG(MY_DEBUG,"%s found %p %s locally in %p %s\n",__FUNCTION__,new,desc->name,loc,loc->desc.name);
	}
	else {
		new = new_rddma_xfer(loc,desc);
		if (new)
			if (rddma_xfer_register(new))
				return NULL;
	}

	return new;
}

void rddma_xfer_delete(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct rddma_xfer *xfer;
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
	xfer = to_rddma_xfer(kset_find_obj(&loc->xfers->kset,desc->name));
	rddma_xfer_put(xfer);
	rddma_xfer_unregister(xfer);
}

void rddma_xfer_load_binds(struct rddma_xfer *xfer, struct rddma_bind *bind)
{
	RDDMA_DEBUG(MY_DEBUG,"%s %p %p\n",__FUNCTION__,xfer,bind);
	/* Added test, and diagnostics: this call sometimes fails, and Oops the kernel */
	if (xfer->desc.rde && xfer->desc.rde->ops && xfer->desc.rde->ops->link_bind) {
#ifdef SERIALIZE_BIND_PROCESSING
		xfer->desc.rde->ops->link_bind(&xfer->dma_chain, bind);
#else
		xfer->desc.rde->ops->link_bind(NULL, bind);
#endif

		return;
	}
	RDDMA_DEBUG (MY_DEBUG, "xx %s: Xfer %s has incomplete operations set.\n", __FUNCTION__, kobject_name (&xfer->kobj));
	RDDMA_DEBUG (MY_DEBUG, "   rde: %s\n", (xfer->desc.rde) ? "Present" : "Missing!");
	RDDMA_DEBUG (MY_DEBUG, "   rde ops: %s\n", (xfer->desc.rde && xfer->desc.rde->ops) ? "Present" : "Missing!");
	RDDMA_DEBUG (MY_DEBUG, "   rde link_bind op: %s\n", (xfer->desc.rde && xfer->desc.rde->ops && xfer->desc.rde->ops->link_bind) ? "Present" : "Missing!");
}


