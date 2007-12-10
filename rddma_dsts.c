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

#include <linux/rddma_dsts.h>
#include <linux/rddma_bind.h>

#include <linux/slab.h>
#include <linux/module.h>

/**
* rddma_dsts_release - Release (free) an rddma_dsts cleanly.
*
* @kobj - pointer to rddma_dsts-type kobject
*
* This function is invoked by the kernel kobject manager when an 
* rddma_dsts object has finally expired. Its job is to release any
* memory resources bound to the kobject. 
*
**/
static void rddma_dsts_release(struct kobject *kobj)
{
    struct rddma_dsts *p = to_rddma_dsts(kobj);
    RDDMA_DEBUG(MY_LIFE_DEBUG,"XXX %s %p (refc %lx)\n", __FUNCTION__, p, (unsigned long)kobj->kref.refcount.counter);
    kfree(p);
}

struct rddma_dsts_attribute {
    struct attribute attr;
    ssize_t (*show)(struct rddma_dsts*, char *buffer);
    ssize_t (*store)(struct rddma_dsts*, const char *buffer, size_t size);
};

#define RDDMA_DSTS_ATTR(_name,_mode,_show,_store) struct rddma_dsts_attribute rddma_dsts_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t rddma_dsts_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct rddma_dsts_attribute *pattr = container_of(attr, struct rddma_dsts_attribute, attr);
    struct rddma_dsts *p = to_rddma_dsts(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t rddma_dsts_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct rddma_dsts_attribute *pattr = container_of(attr, struct rddma_dsts_attribute, attr);
    struct rddma_dsts *p = to_rddma_dsts(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops rddma_dsts_sysfs_ops = {
    .show = rddma_dsts_show,
    .store = rddma_dsts_store,
};


static ssize_t rddma_dsts_default_show(struct rddma_dsts *rddma_dsts, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "rddma_dsts_default");
}

static ssize_t rddma_dsts_default_store(struct rddma_dsts *rddma_dsts, const char *buffer, size_t size)
{
    return size;
}

RDDMA_DSTS_ATTR(default, 0644, rddma_dsts_default_show, rddma_dsts_default_store);

static struct attribute *rddma_dsts_default_attrs[] = {
    &rddma_dsts_attr_default.attr,
    0,
};

struct kobj_type rddma_dsts_type = {
    .release = rddma_dsts_release,
    .sysfs_ops = &rddma_dsts_sysfs_ops,
    .default_attrs = rddma_dsts_default_attrs,
};

static int rddma_dsts_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *rddma_dsts_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int rddma_dsts_uevent(struct kset *kset, struct kobject *kobj, char **envp, int num_envp, char *buffer, int buf_size)
{
	return -ENODEV; /* Do not generate event */
}


static struct kset_uevent_ops rddma_dsts_uevent_ops = {
	.filter = rddma_dsts_uevent_filter,
	.name = rddma_dsts_uevent_name,
 	.uevent = rddma_dsts_uevent, 
};

struct rddma_dsts *new_rddma_dsts(struct rddma_bind_param *params, struct rddma_bind *parent)
{
	struct rddma_dsts *new = kzalloc(sizeof(struct rddma_dsts), GFP_KERNEL);
    
	if (NULL == new)
		return new;
    
	kobject_set_name(&new->kset.kobj,"%s.%s#%llx:%x=%s.%s#%llx:%x",
						    params->dst.name,params->dst.location,params->dst.offset,params->dst.extent,
						    params->src.name,params->src.location,params->src.offset,params->src.extent);
	new->kset.kobj.ktype = &rddma_dsts_type;
	new->kset.uevent_ops = &rddma_dsts_uevent_ops;
	new->kset.kobj.parent = &parent->kobj;
	INIT_LIST_HEAD(&new->dma_chain);

	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,new);
	return new;
}

int rddma_dsts_register(struct rddma_dsts *rddma_dsts)
{
    int ret = 0;

    if ( (ret = kset_register(&rddma_dsts->kset) ) )
	goto out;

      return ret;

out:
    return ret;
}

void rddma_dsts_unregister(struct rddma_dsts *rddma_dsts)
{
    
	if (rddma_dsts)
		kset_unregister(&rddma_dsts->kset);
}

struct rddma_dsts *rddma_dsts_create(struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	struct rddma_dsts *dsts = NULL;

	RDDMA_DEBUG(MY_DEBUG,"%s parent(%p) desc(%p)\n",__FUNCTION__,parent,desc);

	if (NULL == parent->dsts)
		dsts = new_rddma_dsts(desc,parent);

	if (dsts) {
		if (rddma_dsts_register(dsts))
			goto fail_reg;
		parent->dsts = dsts;
	}

	return parent->dsts;
fail_reg:
	rddma_dsts_put(dsts);
	return NULL;
}

void rddma_dsts_delete(struct rddma_dsts *dsts)
{
	if (dsts)
		rddma_dsts_unregister(dsts);
}
