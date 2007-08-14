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

#define MY_DEBUG      RDDMA_DBG_SUBSYS | RDDMA_DBG_FUNCALL | RDDMA_DBG_ALWAYS
#define MY_LIFE_DEBUG RDDMA_DBG_SUBSYS | RDDMA_DBG_LIFE    | RDDMA_DBG_ALWAYS

#include <linux/rddma_subsys.h>
#include <linux/rddma_location.h>

#include <linux/slab.h>
#include <linux/module.h>

static void rddma_subsys_release(struct kobject *kobj)
{
    struct rddma_subsys *p = to_rddma_subsys(kobj);
    RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,p);
    
    kfree(p);
}

struct rddma_subsys_attribute {
    struct attribute attr;
    ssize_t (*show)(struct rddma_subsys*, char *buffer);
    ssize_t (*store)(struct rddma_subsys*, const char *buffer, size_t size);
};

#define RDDMA_SUBSYS_ATTR(_name,_mode,_show,_store) \
struct rddma_subsys_attribute rddma_subsys_attr_##_name = {\
     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },\
     .show = _show,\
     .store = _store \
};

static ssize_t rddma_subsys_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct rddma_subsys_attribute *pattr = container_of(attr, struct rddma_subsys_attribute, attr);
    struct rddma_subsys *p = to_rddma_subsys(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t rddma_subsys_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct rddma_subsys_attribute *pattr = container_of(attr, struct rddma_subsys_attribute, attr);
    struct rddma_subsys *p = to_rddma_subsys(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops rddma_subsys_sysfs_ops = {
    .show = rddma_subsys_show,
    .store = rddma_subsys_store,
};

#define SHOW_BIT(b,s) if (left && (rddma_debug_level & (b))) size = snprintf(bp,left,s); bp+= size ; left -= size
#define SHOW_LEVEL() size = snprintf(bp, PAGE_SIZE, "%d\n", rddma_debug_level & RDDMA_DBG_WHEN); bp+= size ; left -= size
static ssize_t rddma_subsys_debug_show(struct rddma_subsys *rddma_subsys, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	char *bp = buffer;
	SHOW_BIT( RDDMA_DBG_LOCATION,"location ");
	SHOW_BIT( RDDMA_DBG_SMB,"smb ");
	SHOW_BIT( RDDMA_DBG_XFER,"xfer ");
	SHOW_BIT( RDDMA_DBG_CDEV,"cdev ");
	SHOW_BIT( RDDMA_DBG_FABRIC,"fabric ");
	SHOW_BIT( RDDMA_DBG_FABNET,"fabnet ");
	SHOW_BIT( RDDMA_DBG_OPS,"ops ");
	SHOW_BIT( RDDMA_DBG_LOCOPS,"private ");
	SHOW_BIT( RDDMA_DBG_FABOPS,"public ");
	SHOW_BIT( RDDMA_DBG_PARSE,"parse ");
	SHOW_BIT( RDDMA_DBG_FUNCALL,"funcall ");
	SHOW_BIT( RDDMA_DBG_LIFE,"life ");
	SHOW_LEVEL();
	return PAGE_SIZE - left;
}

#define STORE_BIT(s,b) if (!strncmp(token,s,toklen))do { whowhat |= b ; continue; } while (0)

static ssize_t rddma_subsys_debug_store(struct rddma_subsys *rddma_subsys, const char *buffer, size_t size)
{
	int level = 0;
	int whowhat = 0;
	const char *token;
	int toklen;
	while (*buffer == ' ') buffer++;
	for (token = buffer, toklen = strcspn(buffer," "); toklen ; toklen = strcspn(token, " ")) {
		STORE_BIT("location", RDDMA_DBG_LOCATION);
		STORE_BIT("smb", RDDMA_DBG_SMB);
		STORE_BIT("xfer", RDDMA_DBG_XFER);
		STORE_BIT("cdev", RDDMA_DBG_CDEV);
		STORE_BIT("fabric", RDDMA_DBG_FABRIC);
		STORE_BIT("fabnet", RDDMA_DBG_FABNET);
		STORE_BIT("ops", RDDMA_DBG_OPS);
		STORE_BIT("private", RDDMA_DBG_LOCOPS);
		STORE_BIT("public", RDDMA_DBG_FABOPS);
		STORE_BIT("parse", RDDMA_DBG_PARSE);
		STORE_BIT("funcall", RDDMA_DBG_FUNCALL);
		STORE_BIT("life", RDDMA_DBG_LIFE);
		STORE_BIT("all", RDDMA_DBG_ALL);
		STORE_BIT("everyone", RDDMA_DBG_EVERYONE);
		STORE_BIT("everything", RDDMA_DBG_EVERYTHING);
		STORE_BIT("always", RDDMA_DBG_ALWAYS);
		sscanf(token,"%d", &level);
		token += toklen;
		while (*token == ' ') token++;
	}
	rddma_debug_level = (level & RDDMA_DBG_WHEN) | whowhat;
	return size;
}

RDDMA_SUBSYS_ATTR(debug, 0644, rddma_subsys_debug_show, rddma_subsys_debug_store);

static ssize_t rddma_subsys_location_show(struct rddma_subsys *rddma_subsys, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%s\n", rddma_subsys->desc.location);
}

RDDMA_SUBSYS_ATTR(location, 0644, rddma_subsys_location_show, 0);

static ssize_t rddma_subsys_name_show(struct rddma_subsys *rddma_subsys, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%s\n", rddma_subsys->desc.name);
}

RDDMA_SUBSYS_ATTR(name, 0644, rddma_subsys_name_show, 0);

static struct attribute *rddma_subsys_default_attrs[] = {
    &rddma_subsys_attr_debug.attr,
    &rddma_subsys_attr_location.attr,
    &rddma_subsys_attr_name.attr,
    0,
};

struct kobj_type rddma_subsys_type = {
    .release = rddma_subsys_release,
    .sysfs_ops = &rddma_subsys_sysfs_ops,
    .default_attrs = rddma_subsys_default_attrs,
};

static int rddma_subsys_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *rddma_subsys_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int rddma_subsys_uevent(struct kset *kset, struct kobject *kobj, char **envp, int num_envp, char *buffer, int buf_size)
{
	return -ENODEV; /* Do not generate event */
}


static struct kset_uevent_ops rddma_subsys_uevent_ops = {
	.filter = rddma_subsys_uevent_filter,
	.name = rddma_subsys_uevent_name,
 	.uevent = rddma_subsys_uevent, 
};

struct rddma_subsys *new_rddma_subsys(char *name)
{
    struct rddma_subsys *new = kzalloc(sizeof(struct rddma_subsys), GFP_KERNEL);
    
    if (NULL == new)
	return new;

    if ( rddma_parse_desc( &new->desc, name) )
	    goto out;

    new->kset.ktype = &rddma_location_type;
    new->kset.uevent_ops = &rddma_subsys_uevent_ops;
    new->kset.kobj.ktype = &rddma_subsys_type;

    RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,new);
    return new;
out:
    rddma_subsys_put(new);
    RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,NULL);
    return NULL;
}

int rddma_subsys_register(struct rddma_subsys *rddma_subsys)
{
	int ret = 0;

	kobject_set_name(&rddma_subsys->kset.kobj,rddma_subsys->desc.name);
	if ( (ret = kset_register(&rddma_subsys->kset) ) )
		kset_put(&rddma_subsys->kset);

	return ret;
}

void rddma_subsys_unregister(struct rddma_subsys *rddma_subsys)
{
     kset_unregister(&rddma_subsys->kset);
}

