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

#include <linux/rddma_binds.h>
#include <linux/rddma_xfer.h>

#include <linux/slab.h>
#include <linux/module.h>

static void rddma_binds_release(struct kobject *kobj)
{
    struct rddma_binds *p = to_rddma_binds(kobj);
    kfree(p);
}

struct rddma_binds_attribute {
    struct attribute attr;
    ssize_t (*show)(struct rddma_binds*, char *buffer);
    ssize_t (*store)(struct rddma_binds*, const char *buffer, size_t size);
};

#define RDDMA_BINDS_ATTR(_name,_mode,_show,_store) struct rddma_binds_attribute rddma_binds_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t rddma_binds_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct rddma_binds_attribute *pattr = container_of(attr, struct rddma_binds_attribute, attr);
    struct rddma_binds *p = to_rddma_binds(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t rddma_binds_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct rddma_binds_attribute *pattr = container_of(attr, struct rddma_binds_attribute, attr);
    struct rddma_binds *p = to_rddma_binds(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops rddma_binds_sysfs_ops = {
    .show = rddma_binds_show,
    .store = rddma_binds_store,
};


static ssize_t rddma_binds_default_show(struct rddma_binds *rddma_binds, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "rddma_binds_default");
}

static ssize_t rddma_binds_default_store(struct rddma_binds *rddma_binds, const char *buffer, size_t size)
{
    return size;
}

RDDMA_BINDS_ATTR(default, 0644, rddma_binds_default_show, rddma_binds_default_store);

static struct attribute *rddma_binds_default_attrs[] = {
    &rddma_binds_attr_default.attr,
    0,
};

struct kobj_type rddma_binds_type = {
    .release = rddma_binds_release,
    .sysfs_ops = &rddma_binds_sysfs_ops,
    .default_attrs = rddma_binds_default_attrs,
};

static int rddma_binds_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *rddma_binds_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int rddma_binds_uevent(struct kset *kset, struct kobject *kobj, char **envp, int num_envp, char *buffer, int buf_size)
{
	return -ENODEV ; /* Do not generate event */
}


static struct kset_uevent_ops rddma_binds_uevent_ops = {
	.filter = rddma_binds_uevent_filter,
	.name = rddma_binds_uevent_name,
 	.uevent = rddma_binds_uevent, 
};

struct rddma_binds *new_rddma_binds(char *name, struct rddma_xfer *parent)
{
    struct rddma_binds *new = kzalloc(sizeof(struct rddma_binds), GFP_KERNEL);
    
    if (NULL == new)
	return new;

    kobject_set_name(&new->kset.kobj,name);
    new->kset.kobj.ktype = &rddma_binds_type;
    new->kset.uevent_ops = &rddma_binds_uevent_ops;
    new->kset.kobj.kset = &parent->binds->kset;

    return new;
}

int rddma_binds_register(struct rddma_binds *rddma_binds)
{
    int ret = 0;

    if ( (ret = kset_register(&rddma_binds->kset) ) )
	goto out;

      return ret;

out:
    return ret;
}

void rddma_binds_unregister(struct rddma_binds *rddma_binds)
{
    
     kset_unregister(&rddma_binds->kset);
}

