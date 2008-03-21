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

#define MY_DEBUG      RDDMA_DBG_SUBSYS | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_SUBSYS | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/rddma_subsys.h>
#include <linux/rddma_location.h>
#include <linux/rddma_readies.h>

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

#define SHOW_BIT(s,b) if (left && (rddma_debug_level & (b))) size = snprintf(bp,left,"%s ",s); bp+= size ; left -= size
#define SHOW_LEVEL() size = snprintf(bp, PAGE_SIZE, "%d\n", rddma_debug_level & RDDMA_DBG_WHEN); bp+= size ; left -= size
static ssize_t rddma_subsys_debug_show(struct rddma_subsys *rddma_subsys, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	char *bp = buffer;
	SHOW_BIT( "location", RDDMA_DBG_LOCATION);
	SHOW_BIT( "smb", RDDMA_DBG_SMB);
	SHOW_BIT( "xfer", RDDMA_DBG_XFER);
	SHOW_BIT( "cdev", RDDMA_DBG_CDEV);
	SHOW_BIT( "fabric", RDDMA_DBG_FABRIC);
	SHOW_BIT( "fabnet", RDDMA_DBG_FABNET);
	SHOW_BIT( "ops", RDDMA_DBG_OPS);
	SHOW_BIT( "private", RDDMA_DBG_LOCOPS);
	SHOW_BIT( "public", RDDMA_DBG_FABOPS);
	SHOW_BIT( "parse", RDDMA_DBG_PARSE);
	SHOW_BIT( "dma", RDDMA_DBG_DMA);
	SHOW_BIT( "riodma", RDDMA_DBG_DMARIO);
	SHOW_BIT( "netdma", RDDMA_DBG_DMANET);
	SHOW_BIT( "subsys", RDDMA_DBG_SUBSYS);
	SHOW_BIT( "bind", RDDMA_DBG_BIND);
	SHOW_BIT( "dst", RDDMA_DBG_DST);
	SHOW_BIT( "src", RDDMA_DBG_SRC);
	SHOW_BIT( "mmap", RDDMA_DBG_MMAP);
	SHOW_BIT( "rdys", RDDMA_DBG_RDYS);
	SHOW_BIT( "dones", RDDMA_DBG_DONES);
	SHOW_BIT( "evnts", RDDMA_DBG_EVNTS);
	SHOW_BIT( "evnt", RDDMA_DBG_EVNT);
	SHOW_BIT( "funcall", RDDMA_DBG_FUNCALL);
	SHOW_BIT( "life", RDDMA_DBG_LIFE);
	SHOW_LEVEL();
	return PAGE_SIZE - left;
}

#define STORE_BIT(s,b) if (!strncmp(token,s,toklen)) do {if (negate) whowhat &= ~b; else whowhat |= b; } while (0)

static ssize_t rddma_subsys_debug_store(struct rddma_subsys *rddma_subsys, const char *buffer, size_t size)
{
	unsigned int level = rddma_debug_level & RDDMA_DBG_WHEN;
	unsigned int whowhat = rddma_debug_level & RDDMA_DBG_WHO_WHAT;
	const char *token;
	int toklen;
	int negate = 0;
	while (*buffer == ' ') buffer++;
	if ( ( negate = (*buffer == '~')) ) buffer++;
	token = buffer;
	toklen = strcspn(buffer," \n");
	while ( toklen ) {
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
		STORE_BIT("dma", RDDMA_DBG_DMA);
		STORE_BIT("riodma", RDDMA_DBG_DMARIO);
		STORE_BIT("netdma", RDDMA_DBG_DMANET);
		STORE_BIT("subsys", RDDMA_DBG_SUBSYS);
		STORE_BIT("bind", RDDMA_DBG_BIND);
		STORE_BIT("dst", RDDMA_DBG_DST);
		STORE_BIT("src", RDDMA_DBG_SRC);
		STORE_BIT( "mmap", RDDMA_DBG_MMAP);
		STORE_BIT( "rdys", RDDMA_DBG_RDYS);
		STORE_BIT( "dones", RDDMA_DBG_DONES);
		STORE_BIT( "evnts", RDDMA_DBG_EVNTS);
		STORE_BIT( "evnt", RDDMA_DBG_EVNT);
		STORE_BIT("funcall", RDDMA_DBG_FUNCALL);
		STORE_BIT("life", RDDMA_DBG_LIFE);
		STORE_BIT("all", RDDMA_DBG_ALL);
		STORE_BIT("everyone", RDDMA_DBG_EVERYONE);
		STORE_BIT("everything", RDDMA_DBG_EVERYTHING);
		STORE_BIT("always", RDDMA_DBG_ALWAYS);
		sscanf(token,"%d", &level);
		token += toklen;
		while (*token == ' ') token++;
		if ( (negate = (*token == '~')) ) token++;
		toklen = strcspn(token, " \n");
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

int new_rddma_subsys(struct rddma_subsys **subsys, char *name)
{
    struct rddma_subsys *new = kzalloc(sizeof(struct rddma_subsys), GFP_KERNEL);
    
    *subsys = new;
    if (NULL == new)
	return -ENOMEM;

    if ( rddma_parse_desc( &new->desc, name) )
	    goto out;

    new->kset.ktype = &rddma_location_type;
    new->kset.uevent_ops = &rddma_subsys_uevent_ops;
    new->kset.kobj.ktype = &rddma_subsys_type;

    RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,new);
    return 0;
out:
    rddma_subsys_put(new);
    *subsys = NULL;
    RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,NULL);
    return -EINVAL;
}

int rddma_subsys_register(struct rddma_subsys *parent)
{
	int ret;
	struct rddma_readies *readies;

	kobject_set_name(&parent->kset.kobj,parent->desc.name);

	if ( (ret = kset_register(&parent->kset) ) ) {
		kset_put(&parent->kset);
		return ret;
	}

	ret = rddma_readies_create(&readies,parent,"events");

	if (readies == NULL)
		goto out;

	parent->events = readies;

	return 0;

out:
	rddma_subsys_unregister(parent);
	return -ENOMEM;
}

void rddma_subsys_unregister(struct rddma_subsys *parent)
{
	if (parent)
		kset_unregister(&parent->kset);
}

