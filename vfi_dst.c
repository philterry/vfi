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

#define MY_DEBUG      RDDMA_DBG_DST | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_DST | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/vfi_dst.h>
#include <linux/vfi_parse.h>
#include <linux/vfi_smb.h>
#include <linux/vfi_ops.h>
#include <linux/vfi_src.h>

#include <linux/slab.h>
#include <linux/module.h>

/**
* rddma_dst_release - release (free) an rddma_dst cleanly.
*
* @kobj - pointer to an rddma_dst-type kobject to be released.
*
* This function is invoked by the kernel kobject manager when an 
* rddma_dst object has finally expired. Its job is to release any
* memory resources bound to the kobject.
*
**/
static void rddma_dst_release(struct kobject *kobj)
{
    struct rddma_dst *p = to_rddma_dst(kobj);
    RDDMA_DEBUG(MY_LIFE_DEBUG,"XXX %s %p (refc %lx)\n", __FUNCTION__, p, (unsigned long)kobj->kref.refcount.counter);
    rddma_clean_bind (&p->desc);
    kfree (p);
}

struct rddma_dst_attribute {
    struct attribute attr;
    ssize_t (*show)(struct rddma_dst*, char *buffer);
    ssize_t (*store)(struct rddma_dst*, const char *buffer, size_t size);
};

#define RDDMA_DST_ATTR(_name,_mode,_show,_store)\
 struct rddma_dst_attribute rddma_dst_attr_##_name = {\
     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },\
     .show = _show,\
     .store = _store \
};

static ssize_t rddma_dst_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct rddma_dst_attribute *pattr = container_of(attr, struct rddma_dst_attribute, attr);
    struct rddma_dst *p = to_rddma_dst(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t rddma_dst_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct rddma_dst_attribute *pattr = container_of(attr, struct rddma_dst_attribute, attr);
    struct rddma_dst *p = to_rddma_dst(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops rddma_dst_sysfs_ops = {
    .show = rddma_dst_show,
    .store = rddma_dst_store,
};


static ssize_t rddma_dst_default_show(struct rddma_dst *rddma_dst, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("Dst %p is %s.%s = %s.%s\n",rddma_dst,rddma_dst->desc.dst.name,rddma_dst->desc.dst.location,
		    rddma_dst->desc.src.name,rddma_dst->desc.src.location);
	ATTR_PRINTF("xfer: ops is %p rde is %p address is %p\n",rddma_dst->desc.xfer.ops,rddma_dst->desc.xfer.rde,rddma_dst->desc.xfer.address);
	ATTR_PRINTF("dst: ops is %p rde is %p address is %p\n",rddma_dst->desc.dst.ops,rddma_dst->desc.dst.rde,rddma_dst->desc.dst.address);
	ATTR_PRINTF("src: ops is %p rde is %p address is %p\n",rddma_dst->desc.src.ops,rddma_dst->desc.src.rde,rddma_dst->desc.src.address);
	return size;

}

RDDMA_DST_ATTR(default, 0644, rddma_dst_default_show, 0);

static ssize_t rddma_dst_location_show(struct rddma_dst *rddma_dst, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%s\n",rddma_dst->desc.dst.location);
}

RDDMA_DST_ATTR(location, 0644, rddma_dst_location_show, 0);

static ssize_t rddma_dst_name_show(struct rddma_dst *rddma_dst, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%s\n",rddma_dst->desc.dst.name);
}

RDDMA_DST_ATTR(name, 0644, rddma_dst_name_show, 0);

static ssize_t rddma_dst_extent_show(struct rddma_dst *rddma_dst, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%x\n",rddma_dst->desc.dst.extent);
}

RDDMA_DST_ATTR(extent, 0644, rddma_dst_extent_show,0);

static ssize_t rddma_dst_offset_show(struct rddma_dst *rddma_dst, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%llx\n",rddma_dst->desc.dst.offset);
}

RDDMA_DST_ATTR(offset, 0644, rddma_dst_offset_show, 0);

static struct attribute *rddma_dst_default_attrs[] = {
	&rddma_dst_attr_default.attr,
	&rddma_dst_attr_location.attr,
	&rddma_dst_attr_name.attr,
	&rddma_dst_attr_extent.attr,
	&rddma_dst_attr_offset.attr,
	0,
};

struct kobj_type rddma_dst_type = {
    .release = rddma_dst_release,
    .sysfs_ops = &rddma_dst_sysfs_ops,
    .default_attrs = rddma_dst_default_attrs,
};

int new_rddma_dst(struct rddma_dst **dst, struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	struct rddma_dst *new = kzalloc(sizeof(struct rddma_dst), GFP_KERNEL);
    
	*dst = new;

	if (NULL == new)
		return -ENOMEM;

	rddma_clone_bind(&new->desc, desc);
	new->kobj.ktype = &rddma_dst_type;
	kobject_set_name(&new->kobj,"%s.%s#%llx", new->desc.dst.name, new->desc.dst.location, new->desc.dst.offset);
	new->bind = parent;
	new->kobj.kset = &parent->dsts->kset;
	rddma_bind_inherit(&new->desc,&parent->desc);

	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,new);
	return 0;
}

int rddma_dst_register(struct rddma_dst *rddma_dst)
{
	int ret = 0;

	RDDMA_KTRACE ("<*** %s IN ***>\n", __func__);
	ret = kobject_register(&rddma_dst->kobj);
	RDDMA_KTRACE ("<*** %s OUT ***>\n", __func__);
	return ret;
}

void rddma_dst_unregister(struct rddma_dst *rddma_dst)
{
	RDDMA_KTRACE ("<*** %s IN ***>\n", __func__);
	if (rddma_dst)
		kobject_unregister(&rddma_dst->kobj);
	RDDMA_KTRACE ("<*** %s OUT ***>\n", __func__);
}

/**
* find_rddma_dst_in - find an rddma_dst object in a specified bind
* @bind : pointer to parent bind. May be NULL.
* @desc : bind descriptor
*
* This function searches the local object tree for a <dst> object
* described in @desc.
*
* If @bind is non-zero, the function will search within that bind; 
* otherwise it searches the entire tree.
*
**/
int find_rddma_dst_in(struct rddma_dst **dst,struct rddma_bind *bind, struct rddma_bind_param *desc)
{
	int ret;
	struct rddma_bind *mybind = bind;
	struct rddma_dsts *dsts;

	*dst = NULL;

	RDDMA_DEBUG(MY_DEBUG,"%s desc(%p)\n",__FUNCTION__,desc);

	if (mybind == NULL) {
		ret = find_rddma_bind(&mybind,&desc->xfer);
		if (ret)
			return ret;
	}

	rddma_dsts_create(&dsts,mybind,desc);

	/*
	* Search for the <dst> at the <xfer> site.
	*
	*/
	if (mybind->desc.xfer.ops)
		ret = mybind->desc.xfer.ops->dst_find(dst,mybind,desc);

	if (!bind) {
		RDDMA_KTRACE ("<*** %s bind put after dst_find opcall ***>\n", __func__);
		rddma_bind_put (mybind);
	}

	return ret ? ret : *dst == NULL;
}

int rddma_dst_create(struct rddma_dst **dst, struct rddma_bind *bind, struct rddma_bind_param *desc)
{
	int ret;

	ret = new_rddma_dst(dst,bind,desc);

	if (ret)
		return ret;

	ret = rddma_dst_register(*dst);

	if (ret) 
		rddma_dst_put(*dst);

	return ret;
}

void rddma_dst_delete (struct rddma_bind *bind, struct rddma_bind_param *desc)
{
	struct rddma_dst *dst;
	char buf[128];

	RDDMA_DEBUG(MY_DEBUG,"%s: %s.%s#%llx:%x/%s.%s#%llx:%x\n", __FUNCTION__, 
		    desc->xfer.name, desc->xfer.location,desc->xfer.offset, desc->xfer.extent, 
		    desc->dst.name, desc->dst.location,desc->dst.offset, desc->dst.extent);
	
	/*
	* Hah - dst object name is composed "<name>.<loc>#<address>" - there is no
	* extent suffix.
	*
	*/
	if (snprintf (buf, 128, "%s.%s#%llx",
		      desc->dst.name, desc->dst.location, desc->dst.offset) >= 128) {
		RDDMA_DEBUG(MY_DEBUG, "%s buffer not big enough\n",__FUNCTION__);
	}
	
	dst = to_rddma_dst (kset_find_obj (&bind->dsts->kset, buf));
	if (dst) {
		rddma_dst_put (dst);		/* Put, to counteract the find... */
		rddma_dst_unregister(dst);
	}
}

void rddma_dst_load_srcs(struct rddma_dst *dst)
{
	struct list_head * entry;
	struct rddma_src * src2 = NULL; 
	RDDMA_DEBUG(MY_DEBUG,"%s %p\n",__FUNCTION__,dst->srcs);
	if (dst->srcs) {
		spin_lock(&dst->srcs->kset.list_lock);
		if (!list_empty(&dst->srcs->kset.list)) {
			list_for_each(entry,&dst->srcs->kset.list) {
				src2 = to_rddma_src(to_kobj(entry));
				RDDMA_DEBUG(MY_DEBUG,"%s src2 %p, name = %s\n",__FUNCTION__,src2->desc.src.rde, &src2->kobj.name[0]);
				if (src2->desc.src.rde && src2->desc.src.rde->ops && src2->desc.src.rde->ops->load && src2->desc.src.rde->ops->link_src) {
					src2->desc.src.rde->ops->load(src2);
					src2->desc.src.rde->ops->link_src(&dst->srcs->dma_chain, src2);
				}
				else {
					RDDMA_DEBUG (MY_DEBUG, "xx %s failed: src \"%s\" is missing RDE ops\n", 
							__FUNCTION__, src2->desc.src.name);
				}
			}
		}
		spin_unlock(&dst->srcs->kset.list_lock);
	}
}
