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

#define MY_DEBUG      RDDMA_DBG_BIND | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_BIND | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/rddma_bind.h>
#include <linux/rddma_xfer.h>
#include <linux/rddma_ops.h>
#include <linux/rddma_binds.h>
#include <linux/rddma_dsts.h>
#include <linux/rddma_dst.h>
#include <linux/rddma_dma.h>

#include <linux/slab.h>
#include <linux/module.h>


/**
* rddma_bind_release - release (free) an rddma_bind cleanly.
*
* @kobj - pointer to an rddma_bind-type kobject to be released.
*
* This function is invoked by the kernel kobject manager when an 
* rddma_bind object has finally expired. Its job is to release any
* memory resources bound to the kobject.
*
**/
static void rddma_bind_release(struct kobject *kobj)
{
    struct rddma_bind *p = to_rddma_bind(kobj);
    RDDMA_DEBUG(MY_LIFE_DEBUG,"XXX %s %p (refc %lx)\n", __FUNCTION__, p, (unsigned long)kobj->kref.refcount.counter);
    rddma_clean_bind (&p->desc);
    kfree(p);
}

struct rddma_bind_attribute {
    struct attribute attr;
    ssize_t (*show)(struct rddma_bind*, char *buffer);
    ssize_t (*store)(struct rddma_bind*, const char *buffer, size_t size);
};

#define RDDMA_BIND_ATTR(_name,_mode,_show,_store) \
struct rddma_bind_attribute rddma_bind_attr_##_name = {\
     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },\
     .show = _show,\
     .store = _store\
};

static ssize_t rddma_bind_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct rddma_bind_attribute *pattr = container_of(attr, struct rddma_bind_attribute, attr);
    struct rddma_bind *p = to_rddma_bind(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t rddma_bind_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct rddma_bind_attribute *pattr = container_of(attr, struct rddma_bind_attribute, attr);
    struct rddma_bind *p = to_rddma_bind(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops rddma_bind_sysfs_ops = {
    .show = rddma_bind_show,
    .store = rddma_bind_store,
};


static ssize_t rddma_bind_default_show(struct rddma_bind *rddma_bind, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("Bind %p is %s.%s = %s.%s \n",rddma_bind,rddma_bind->desc.dst.name, rddma_bind->desc.dst.location,
		    rddma_bind->desc.src.name,rddma_bind->desc.src.location);
	if (rddma_bind) {
		ATTR_PRINTF("dst: ops is %p rde is %p address is %p\n",rddma_bind->desc.dst.ops,rddma_bind->desc.dst.rde,rddma_bind->desc.dst.address);
		ATTR_PRINTF("src: ops is %p rde is %p address is %p\n",rddma_bind->desc.src.ops,rddma_bind->desc.src.rde,rddma_bind->desc.src.address);
	}
	return size;

}

RDDMA_BIND_ATTR(default, 0644, rddma_bind_default_show, 0);

static ssize_t rddma_bind_offset_show(struct rddma_bind *rddma_bind, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%llx\n",rddma_bind->desc.dst.offset);
}

RDDMA_BIND_ATTR(offset, 0644, rddma_bind_offset_show, 0);

static ssize_t rddma_bind_extent_show(struct rddma_bind *rddma_bind, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%x\n",rddma_bind->desc.dst.extent);
}

RDDMA_BIND_ATTR(extent, 0644, rddma_bind_extent_show, 0);

static struct attribute *rddma_bind_default_attrs[] = {
    &rddma_bind_attr_offset.attr,
    &rddma_bind_attr_extent.attr,
    &rddma_bind_attr_default.attr,
    0,
};

struct kobj_type rddma_bind_type = {
    .release = rddma_bind_release,
    .sysfs_ops = &rddma_bind_sysfs_ops,
    .default_attrs = rddma_bind_default_attrs,
};

/**
* new_rddma_bind - create a new rddma_bind kobject and attach it to parent xfer
* 
* @xfer: pointer to parent xfer, to which new bind should be attached.
* @desc: pointer to bind parameter descriptor, identifying participants and
*        other bind attributes.
*
* This function creates a new rddma_bind kobject and attaches it to its parent
* xfer. 
*
**/
struct rddma_bind *new_rddma_bind(struct rddma_xfer *parent, struct rddma_bind_param *desc)
{
	struct rddma_bind *new = kzalloc(sizeof(struct rddma_bind), GFP_KERNEL);
	
	if (NULL == new)
	return new;
	
	rddma_clone_bind(&new->desc, desc);
	
	kobject_set_name(&new->kobj,"#%llx:%x",desc->xfer.offset,desc->xfer.extent);
	new->kobj.ktype = &rddma_bind_type;
	
	new->kobj.kset = &parent->binds->kset;

	rddma_inherit(&new->desc.xfer,&parent->desc);
	rddma_inherit(&new->desc.dst,&parent->desc);
	rddma_inherit(&new->desc.src,&parent->desc);
	new->desc.dst.ops = &rddma_local_ops;
	new->desc.src.ops = &rddma_local_ops;

	new->src_done_event_id = -1;
	new->src_ready_event_id = -1;
	new->dst_done_event_id = -1;
	new->dst_ready_event_id = -1;

	INIT_LIST_HEAD(&new->dma_chain);

	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,new);
	return new;
}

int rddma_bind_register(struct rddma_bind *rddma_bind)
{
	return kobject_register(&rddma_bind->kobj);
}

void rddma_bind_unregister(struct rddma_bind *rddma_bind)
{
    
     kobject_unregister(&rddma_bind->kobj);
}

struct rddma_bind *find_rddma_bind(struct rddma_desc_param *desc)
{
	struct rddma_xfer *xfer = NULL;
	struct rddma_bind *bind = NULL;

	xfer = find_rddma_xfer(desc);

	if (xfer && xfer->desc.ops)
		bind = xfer->desc.ops->bind_find(xfer,desc);

	rddma_xfer_put(xfer);

	return bind;
}

/**
* rddma_bind_create - create a new rddma_bind kobject and attach it to parent xfer
* 
* @xfer: pointer to parent xfer, to which new bind should be attached.
* @desc: pointer to bind parameter descriptor, identifying participants and
*        other bind attributes.
*
* This function is a front-end wrapper for bind creation that will create a new
* bind only if no other bind currently exists for the same <xfer>/<dst>=<src> triplet.
* 
* The function will return a pointer either to a pre-existing bind for the triplet, 
* or a new bind created by new_rddma_bind ().
*
**/
struct rddma_bind *rddma_bind_create(struct rddma_xfer *xfer, struct rddma_bind_param *desc)
{
	struct rddma_bind *new;
	char buf[128];

	snprintf(buf,128,"#%llx:%x",desc->xfer.offset,desc->xfer.extent);

	new = to_rddma_bind(kset_find_obj(&xfer->binds->kset,buf));

	if (new)
		return NULL;

	if ( (new = new_rddma_bind(xfer,desc)) )
		if (rddma_bind_register(new))
			return NULL;

	return new;
}

void rddma_bind_delete(struct rddma_xfer *xfer, struct rddma_desc_param *desc)
{
	struct rddma_bind *bind = NULL;
	char buf[128];

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	if ( snprintf(buf,128,"#%llx:%x", desc->offset, desc->extent) > 128 )
		goto out;

	bind = to_rddma_bind(kset_find_obj(&xfer->binds->kset, buf));

	if (bind) 
		rddma_bind_unregister (bind);
out:
	RDDMA_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,xfer,desc,bind);
}

void rddma_bind_load_dsts(struct rddma_bind *bind)
{
	struct list_head * entry;
	struct rddma_dst * dst2 = NULL; 

	RDDMA_DEBUG(MY_DEBUG,"%s %p\n",__FUNCTION__,bind);
	if (bind->dsts) {
		spin_lock(&bind->dsts->kset.list_lock);
		if (!list_empty(&bind->dsts->kset.list)) {
			list_for_each(entry,&bind->dsts->kset.list) {
				dst2 = to_rddma_dst(to_kobj(entry));
				RDDMA_DEBUG(MY_DEBUG,"%s dst2 %p name=%s\n",__FUNCTION__,dst2, &dst2->kobj.name[0]);
				if (dst2->desc.dst.rde && dst2->desc.dst.rde->ops && dst2->desc.dst.rde->ops->link_dst) {
					dst2->desc.dst.rde->ops->link_dst(&bind->dsts->dma_chain,dst2);
				}
				else {
					RDDMA_DEBUG (MY_DEBUG, "xx %s failed: Dst \"%s\" is missing RDE ops!\n", 
							__FUNCTION__, dst2->desc.dst.name);
				}
			}
		}
		spin_unlock(&bind->dsts->kset.list_lock);
	}
	list_splice(&bind->dsts->dma_chain, &bind->dma_chain);
}
