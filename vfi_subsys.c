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

#define MY_DEBUG      VFI_DBG_SUBSYS | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_SUBSYS | VFI_DBG_LIFE    | VFI_DBG_DEBUG

#include <linux/vfi_subsys.h>
#include <linux/vfi_location.h>
#include <linux/vfi_readies.h>

#include <linux/slab.h>
#include <linux/module.h>

static void vfi_subsys_release(struct kobject *kobj)
{
    struct vfi_subsys *p = to_vfi_subsys(kobj);
    VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,p);
    
    kfree(p);
}

struct vfi_subsys_attribute {
    struct attribute attr;
    ssize_t (*show)(struct vfi_subsys*, char *buffer);
    ssize_t (*store)(struct vfi_subsys*, const char *buffer, size_t size);
};

#define VFI_SUBSYS_ATTR(_name,_mode,_show,_store) \
struct vfi_subsys_attribute vfi_subsys_attr_##_name = {\
     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },\
     .show = _show,\
     .store = _store \
};

static ssize_t vfi_subsys_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct vfi_subsys_attribute *pattr = container_of(attr, struct vfi_subsys_attribute, attr);
    struct vfi_subsys *p = to_vfi_subsys(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t vfi_subsys_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct vfi_subsys_attribute *pattr = container_of(attr, struct vfi_subsys_attribute, attr);
    struct vfi_subsys *p = to_vfi_subsys(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops vfi_subsys_sysfs_ops = {
    .show = vfi_subsys_show,
    .store = vfi_subsys_store,
};

#define SHOW_BIT(s,b) if (left && (vfi_debug_level & (b))) size = snprintf(bp,left,"%s ",s); bp+= size ; left -= size
#define SHOW_LEVEL() size = snprintf(bp, PAGE_SIZE, "%d\n", vfi_debug_level & VFI_DBG_WHEN); bp+= size ; left -= size
static ssize_t vfi_subsys_debug_show(struct vfi_subsys *vfi_subsys, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	char *bp = buffer;
	SHOW_BIT( "location", VFI_DBG_LOCATION);
	SHOW_BIT( "smb", VFI_DBG_SMB);
	SHOW_BIT( "xfer", VFI_DBG_XFER);
	SHOW_BIT( "cdev", VFI_DBG_CDEV);
	SHOW_BIT( "fabric", VFI_DBG_FABRIC);
	SHOW_BIT( "fabnet", VFI_DBG_FABNET);
	SHOW_BIT( "ops", VFI_DBG_OPS);
	SHOW_BIT( "private", VFI_DBG_LOCOPS);
	SHOW_BIT( "public", VFI_DBG_FABOPS);
	SHOW_BIT( "parse", VFI_DBG_PARSE);
	SHOW_BIT( "dma", VFI_DBG_DMA);
	SHOW_BIT( "riodma", VFI_DBG_DMARIO);
	SHOW_BIT( "netdma", VFI_DBG_DMANET);
	SHOW_BIT( "subsys", VFI_DBG_SUBSYS);
	SHOW_BIT( "bind", VFI_DBG_BIND);
	SHOW_BIT( "dst", VFI_DBG_DST);
	SHOW_BIT( "src", VFI_DBG_SRC);
	SHOW_BIT( "mmap", VFI_DBG_MMAP);
	SHOW_BIT( "rdys", VFI_DBG_RDYS);
	SHOW_BIT( "dones", VFI_DBG_DONES);
	SHOW_BIT( "evnts", VFI_DBG_EVNTS);
	SHOW_BIT( "evnt", VFI_DBG_EVNT);
	SHOW_BIT( "funcall", VFI_DBG_FUNCALL);
	SHOW_BIT( "life", VFI_DBG_LIFE);
	SHOW_LEVEL();
	return PAGE_SIZE - left;
}

#define STORE_BIT(s,b) if (!strncmp(token,s,toklen)) do {if (negate) whowhat &= ~b; else whowhat |= b; } while (0)

static ssize_t vfi_subsys_debug_store(struct vfi_subsys *vfi_subsys, const char *buffer, size_t size)
{
	unsigned int level = vfi_debug_level & VFI_DBG_WHEN;
	unsigned int whowhat = vfi_debug_level & VFI_DBG_WHO_WHAT;
	const char *token;
	int toklen;
	int negate = 0;
	while (*buffer == ' ') buffer++;
	if ( ( negate = (*buffer == '~')) ) buffer++;
	token = buffer;
	toklen = strcspn(buffer," \n");
	while ( toklen ) {
		STORE_BIT("location", VFI_DBG_LOCATION);
		STORE_BIT("smb", VFI_DBG_SMB);
		STORE_BIT("xfer", VFI_DBG_XFER);
		STORE_BIT("cdev", VFI_DBG_CDEV);
		STORE_BIT("fabric", VFI_DBG_FABRIC);
		STORE_BIT("fabnet", VFI_DBG_FABNET);
		STORE_BIT("ops", VFI_DBG_OPS);
		STORE_BIT("private", VFI_DBG_LOCOPS);
		STORE_BIT("public", VFI_DBG_FABOPS);
		STORE_BIT("parse", VFI_DBG_PARSE);
		STORE_BIT("dma", VFI_DBG_DMA);
		STORE_BIT("riodma", VFI_DBG_DMARIO);
		STORE_BIT("netdma", VFI_DBG_DMANET);
		STORE_BIT("subsys", VFI_DBG_SUBSYS);
		STORE_BIT("bind", VFI_DBG_BIND);
		STORE_BIT("dst", VFI_DBG_DST);
		STORE_BIT("src", VFI_DBG_SRC);
		STORE_BIT( "mmap", VFI_DBG_MMAP);
		STORE_BIT( "rdys", VFI_DBG_RDYS);
		STORE_BIT( "dones", VFI_DBG_DONES);
		STORE_BIT( "evnts", VFI_DBG_EVNTS);
		STORE_BIT( "evnt", VFI_DBG_EVNT);
		STORE_BIT("funcall", VFI_DBG_FUNCALL);
		STORE_BIT("life", VFI_DBG_LIFE);
		STORE_BIT("all", VFI_DBG_ALL);
		STORE_BIT("everyone", VFI_DBG_EVERYONE);
		STORE_BIT("everything", VFI_DBG_EVERYTHING);
		STORE_BIT("always", VFI_DBG_ALWAYS);
		sscanf(token,"%d", &level);
		token += toklen;
		while (*token == ' ') token++;
		if ( (negate = (*token == '~')) ) token++;
		toklen = strcspn(token, " \n");
	}
	vfi_debug_level = (level & VFI_DBG_WHEN) | whowhat;
	return size;
}

VFI_SUBSYS_ATTR(debug, 0644, vfi_subsys_debug_show, vfi_subsys_debug_store);

static ssize_t vfi_subsys_location_show(struct vfi_subsys *vfi_subsys, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%s\n", vfi_subsys->desc.location);
}

VFI_SUBSYS_ATTR(location, 0644, vfi_subsys_location_show, 0);

static ssize_t vfi_subsys_name_show(struct vfi_subsys *vfi_subsys, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%s\n", vfi_subsys->desc.name);
}

VFI_SUBSYS_ATTR(name, 0644, vfi_subsys_name_show, 0);

static struct attribute *vfi_subsys_default_attrs[] = {
    &vfi_subsys_attr_debug.attr,
    &vfi_subsys_attr_location.attr,
    &vfi_subsys_attr_name.attr,
    0,
};

struct kobj_type vfi_subsys_type = {
    .release = vfi_subsys_release,
    .sysfs_ops = &vfi_subsys_sysfs_ops,
    .default_attrs = vfi_subsys_default_attrs,
};

static int vfi_subsys_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *vfi_subsys_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int vfi_subsys_uevent(struct kset *kset, struct kobject *kobj, char **envp, int num_envp, char *buffer, int buf_size)
{
	return -ENODEV; /* Do not generate event */
}


static struct kset_uevent_ops vfi_subsys_uevent_ops = {
	.filter = vfi_subsys_uevent_filter,
	.name = vfi_subsys_uevent_name,
 	.uevent = vfi_subsys_uevent, 
};

int new_vfi_subsys(struct vfi_subsys **subsys, char *name)
{
    struct vfi_subsys *new = kzalloc(sizeof(struct vfi_subsys), GFP_KERNEL);
    
    *subsys = new;
    if (NULL == new)
	return -ENOMEM;

    if ( vfi_parse_desc( &new->desc, name) )
	    goto out;

    new->kset.ktype = &vfi_location_type;
    new->kset.uevent_ops = &vfi_subsys_uevent_ops;
    new->kset.kobj.ktype = &vfi_subsys_type;

    VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,new);
    return 0;
out:
    vfi_subsys_put(new);
    *subsys = NULL;
    VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,NULL);
    return -EINVAL;
}

int vfi_subsys_register(struct vfi_subsys *parent)
{
	int ret;
	struct vfi_readies *readies;

	kobject_set_name(&parent->kset.kobj,parent->desc.name);

	if ( (ret = kset_register(&parent->kset) ) ) {
		kset_put(&parent->kset);
		return ret;
	}

	ret = vfi_readies_create(&readies,parent,"events");

	if (readies == NULL)
		goto out;

	parent->events = readies;

	return 0;

out:
	vfi_subsys_unregister(parent);
	return -ENOMEM;
}

void vfi_subsys_unregister(struct vfi_subsys *parent)
{
	if (parent)
		kset_unregister(&parent->kset);
}

