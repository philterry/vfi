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


static void rddma_bind_release(struct kobject *kobj)
{
    struct rddma_bind *p = to_rddma_bind(kobj);
    RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,p);
    if (p->desc.bind.src.name) {
	    RDDMA_DEBUG(MY_LIFE_DEBUG,"%s free src %p\n",__FUNCTION__,p->desc.bind.src.name);
	    kfree(p->desc.bind.src.name);
    }
    if (p->desc.bind.dst.name) {
	    RDDMA_DEBUG(MY_LIFE_DEBUG,"%s free dst %p\n",__FUNCTION__,p->desc.bind.dst.name);
	    kfree(p->desc.bind.dst.name);
    }
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
	ATTR_PRINTF("Bind %p is %s = %s \n",rddma_bind,rddma_bind->desc.bind.dst.name, rddma_bind->desc.bind.src.name);
	if (rddma_bind) {
		ATTR_PRINTF("dst: ops is %p rde is %p address is %p\n",rddma_bind->desc.bind.dst.ops,rddma_bind->desc.bind.dst.rde,rddma_bind->desc.bind.dst.address);
		ATTR_PRINTF("src: ops is %p rde is %p address is %p\n",rddma_bind->desc.bind.src.ops,rddma_bind->desc.bind.src.rde,rddma_bind->desc.bind.src.address);
	}
	return size;

}

RDDMA_BIND_ATTR(default, 0644, rddma_bind_default_show, 0);

static ssize_t rddma_bind_offset_show(struct rddma_bind *rddma_bind, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%llx\n",rddma_bind->desc.bind.dst.offset);
}

RDDMA_BIND_ATTR(offset, 0644, rddma_bind_offset_show, 0);

static ssize_t rddma_bind_extent_show(struct rddma_bind *rddma_bind, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%x\n",rddma_bind->desc.bind.dst.extent);
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

struct rddma_bind *new_rddma_bind(struct rddma_xfer *parent, struct rddma_xfer_param *desc)
{
    struct rddma_bind *new = kzalloc(sizeof(struct rddma_bind), GFP_KERNEL);
    
    if (NULL == new)
	return new;

    rddma_clone_bind(&new->desc.bind, &desc->bind);
    kobject_set_name(&new->kobj,"%s#%llx:%x", desc->xfer.name, desc->xfer.offset,desc->xfer.extent);
    new->kobj.ktype = &rddma_bind_type;

    new->kobj.kset = &parent->binds->kset;

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

struct rddma_bind *find_rddma_bind(struct rddma_xfer_param *desc)
{
	struct rddma_xfer *xfer = NULL;
	struct rddma_bind *bind = NULL;
	xfer = find_rddma_xfer(desc);
	bind = xfer->desc.xfer.ops->bind_find(xfer,desc);
	rddma_xfer_put(xfer);
	return bind;
}

struct rddma_bind *rddma_bind_create(struct rddma_xfer *xfer, struct rddma_xfer_param *desc)
{
	struct rddma_bind *new = new_rddma_bind(xfer,desc);
	if (new)
		if (rddma_bind_register(new))
			return NULL;
	return new;
}

void rddma_bind_delete(struct rddma_bind *bind)
{
	rddma_bind_unregister(bind);
}

void rddma_bind_load_dsts(struct rddma_bind *bind)
{
	struct list_head * entry;

	struct rddma_dst * dst1 = NULL; 
	struct rddma_dst * dst2 = NULL; 
	RDDMA_DEBUG(MY_DEBUG,"%s %p\n",__FUNCTION__,bind);
	if (bind->dsts) {
		spin_lock(&bind->dsts->kset.list_lock);
		if (!list_empty(&bind->dsts->kset.list)) {
			list_for_each(entry,&bind->dsts->kset.list) {
				if (NULL == dst1) {
					dst1 = to_rddma_dst(to_kobj(entry));
					RDDMA_DEBUG(MY_DEBUG,"%s dst1 %p\n",__FUNCTION__,dst1);
					continue;
				}
				dst2 = to_rddma_dst(to_kobj(entry));
				RDDMA_DEBUG(MY_DEBUG,"%s dst2 %p\n",__FUNCTION__,dst1);
				dst1->desc.dst.rde->ops->link_dst(dst1,dst2);
			}
		}
		spin_unlock(&bind->dsts->kset.list_lock);
	}
	RDDMA_DEBUG(MY_DEBUG,"%s head_dst %p\n",__FUNCTION__,dst1);
	bind->head_dst = dst1;
}
