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

#define MY_DEBUG      RDDMA_DBG_SRC | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_SRC | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/rddma_srcs.h>
#include <linux/rddma_dst.h>

#include <linux/slab.h>
#include <linux/module.h>

/**
* rddma_srcs_release - Release (free) an rddma_srcs cleanly.
*
* @kobj - pointer to rddma_srcs-type kobject
*
* This function is invoked by the kernel kobject manager when an 
* rddma_srcs object has finally expired. Its job is to release any
* memory resources bound to the kobject. 
*
**/
static void rddma_srcs_release(struct kobject *kobj)
{
    struct rddma_srcs *p = to_rddma_srcs(kobj);
    RDDMA_DEBUG(MY_LIFE_DEBUG,"XXX %s %p (refc %lx)\n", __FUNCTION__, p, (unsigned long)kobj->kref.refcount.counter);
    kfree(p);
}

struct rddma_srcs_attribute {
    struct attribute attr;
    ssize_t (*show)(struct rddma_srcs*, char *buffer);
    ssize_t (*store)(struct rddma_srcs*, const char *buffer, size_t size);
};

#define RDDMA_SRCS_ATTR(_name,_mode,_show,_store) struct rddma_srcs_attribute rddma_srcs_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t rddma_srcs_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct rddma_srcs_attribute *pattr = container_of(attr, struct rddma_srcs_attribute, attr);
    struct rddma_srcs *p = to_rddma_srcs(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t rddma_srcs_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct rddma_srcs_attribute *pattr = container_of(attr, struct rddma_srcs_attribute, attr);
    struct rddma_srcs *p = to_rddma_srcs(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops rddma_srcs_sysfs_ops = {
    .show = rddma_srcs_show,
    .store = rddma_srcs_store,
};


static ssize_t rddma_srcs_default_show(struct rddma_srcs *rddma_srcs, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "rddma_srcs_default");
}

static ssize_t rddma_srcs_default_store(struct rddma_srcs *rddma_srcs, const char *buffer, size_t size)
{
    return size;
}

RDDMA_SRCS_ATTR(default, 0644, rddma_srcs_default_show, rddma_srcs_default_store);

static struct attribute *rddma_srcs_default_attrs[] = {
    &rddma_srcs_attr_default.attr,
    0,
};

struct kobj_type rddma_srcs_type = {
    .release = rddma_srcs_release,
    .sysfs_ops = &rddma_srcs_sysfs_ops,
    .default_attrs = rddma_srcs_default_attrs,
};

static int rddma_srcs_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *rddma_srcs_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int rddma_srcs_uevent(struct kset *kset, struct kobject *kobj, char **envp, int num_envp, char *buffer, int buf_size)
{
	return -ENODEV; /* Do not generate event */
}


static struct kset_uevent_ops rddma_srcs_uevent_ops = {
	.filter = rddma_srcs_uevent_filter,
	.name = rddma_srcs_uevent_name,
 	.uevent = rddma_srcs_uevent, 
};

struct rddma_srcs *new_rddma_srcs(struct rddma_bind_param *desc, struct rddma_dst *parent)
{
    struct rddma_srcs *new = kzalloc(sizeof(struct rddma_srcs), GFP_KERNEL);
    
    if (NULL == new)
	return new;

    kobject_set_name(&new->kset.kobj,"%s.%s#%llx:%x",desc->src.name,desc->src.location,desc->src.offset,desc->src.extent);
    new->kset.kobj.ktype = &rddma_srcs_type;
    new->kset.uevent_ops = &rddma_srcs_uevent_ops;
    new->kset.kobj.parent = &parent->kobj;
    INIT_LIST_HEAD(&new->dma_chain);

    parent->srcs = new;

    RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,new);
    return new;
}

int rddma_srcs_register(struct rddma_srcs *rddma_srcs)
{
	int ret = 0;

	RDDMA_KTRACE ("<*** %s IN ***>\n", __func__);
	ret = kset_register(&rddma_srcs->kset);
	RDDMA_KTRACE ("<*** %s OUT ***>\n", __func__);
	return ret;
}

void rddma_srcs_unregister(struct rddma_srcs *rddma_srcs)
{
    
	RDDMA_KTRACE ("<*** %s IN ***>\n", __func__);
	if (rddma_srcs)
		kset_unregister(&rddma_srcs->kset);
	RDDMA_KTRACE ("<*** %s OUT ***>\n", __func__);
}

struct rddma_srcs *rddma_srcs_create(struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	struct rddma_srcs *new;

	RDDMA_DEBUG(MY_DEBUG,"%s parent_dst(%p) desc(%p)\n",__FUNCTION__,parent,desc);

	new = new_rddma_srcs(desc,parent);

	if (NULL == new)
		goto out;

	if (rddma_srcs_register(new))
		goto fail_reg;

	return new;
fail_reg:
	rddma_srcs_put(new);
out:
	return NULL;
}

void rddma_srcs_delete(struct rddma_srcs *srcs)
{
	rddma_srcs_unregister(srcs);
}
