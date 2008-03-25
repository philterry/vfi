/* 
 * 
 * Copyright 2008 Vmetro
 * Phil Terry <pterry@vmetro.com> 
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#define MY_DEBUG      VFI_DBG_SYNC | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_SYNC | VFI_DBG_LIFE    | VFI_DBG_DEBUG
#define MY_ERROR      VFI_DBG_SYNC | VFI_DBG_ERROR   | VFI_DBG_ERR

#include <linux/vfi_sync.h>
#include <linux/vfi_parse.h>
#include <linux/vfi_drv.h>
#include <linux/vfi_sync.h>
#include <linux/vfi_syncs.h>
#include <linux/vfi_location.h>
#include <linux/vfi_src.h>
#include <linux/vfi_dst.h>
#include <linux/vfi_ops.h>

#include <asm/semaphore.h>

static void vfi_sync_release(struct kobject *kobj)
{
	struct vfi_sync *p = to_vfi_sync(kobj);
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,p);
	if (p->desc.name) {
		VFI_DEBUG(MY_LIFE_DEBUG,"%s name %p\n",__FUNCTION__,p->desc.name);
		kfree(p->desc.name);
	}
	kfree(p);
}

struct vfi_sync_attribute {
	struct attribute attr;
	ssize_t (*show)(struct vfi_sync*, char *buffer);
	ssize_t (*store)(struct vfi_sync*, const char *buffer, size_t size);
};

#define VFI_SYNC_ATTR(_name,_mode,_show,_store) struct vfi_sync_attribute vfi_sync_attr_##_name = {\
     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },\
     .show = _show,\
     .store = _store\
 };

static ssize_t vfi_sync_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
	struct vfi_sync_attribute *pattr = container_of(attr, struct vfi_sync_attribute, attr);
	struct vfi_sync *p = to_vfi_sync(kobj);

	if (pattr && pattr->show)
		return pattr->show(p,buffer);

	return 0;
}

static ssize_t vfi_sync_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
	struct vfi_sync_attribute *pattr = container_of(attr, struct vfi_sync_attribute, attr);
	struct vfi_sync *p = to_vfi_sync(kobj);

	if (pattr && pattr->store)
		return pattr->store(p, buffer, size);

	return 0;
}

static struct sysfs_ops vfi_sync_sysfs_ops = {
	.show = vfi_sync_show,
	.store = vfi_sync_store,
};


static ssize_t vfi_sync_default_show(struct vfi_sync *vfi_sync, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("Sync %p is %s \n",vfi_sync,vfi_sync ? vfi_sync->desc.name : NULL);
	if (vfi_sync) {
		ATTR_PRINTF("ops is %p rde is %p address is %p\n",vfi_sync->desc.ops,vfi_sync->desc.rde,vfi_sync->desc.address);
	}
	ATTR_PRINTF("refcount %d\n",atomic_read(&vfi_sync->kobj.kref.refcount));
	return size;
}

static ssize_t vfi_sync_default_store(struct vfi_sync *vfi_sync, const char *buffer, size_t size)
{
    return size;
}

VFI_SYNC_ATTR(default, 0644, vfi_sync_default_show, vfi_sync_default_store);

static ssize_t vfi_sync_location_show(struct vfi_sync *vfi_sync, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("Sync %p is %s \n",vfi_sync,vfi_sync ? vfi_sync->desc.name : NULL);
	if (vfi_sync) {
		ATTR_PRINTF("ops is %p rde is %p address is %p\n",vfi_sync->desc.ops,vfi_sync->desc.rde,vfi_sync->desc.address);
	}
	return size;
}

static ssize_t vfi_sync_location_store(struct vfi_sync *vfi_sync, const char *buffer, size_t size)
{
    return size;
}

VFI_SYNC_ATTR(location, 0644, vfi_sync_location_show, vfi_sync_location_store);

static ssize_t vfi_sync_name_show(struct vfi_sync *vfi_sync, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%s\n", vfi_sync->desc.name);
}

VFI_SYNC_ATTR(name, 0444, vfi_sync_name_show, 0);

static ssize_t vfi_sync_offset_show(struct vfi_sync *vfi_sync, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%llx\n", vfi_sync->desc.offset);
}

VFI_SYNC_ATTR(offset, 0444, vfi_sync_offset_show, 0);

static ssize_t vfi_sync_extent_show(struct vfi_sync *vfi_sync, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%x\n", vfi_sync->desc.extent);
}

VFI_SYNC_ATTR(extent, 0444, vfi_sync_extent_show, 0);

static struct attribute *vfi_sync_default_attrs[] = {
    &vfi_sync_attr_default.attr,
    &vfi_sync_attr_location.attr,
    &vfi_sync_attr_name.attr,
    &vfi_sync_attr_offset.attr,
    &vfi_sync_attr_extent.attr,
    0,
};

struct kobj_type vfi_sync_type = {
	.release = vfi_sync_release,
	.sysfs_ops = &vfi_sync_sysfs_ops,
	.default_attrs = vfi_sync_default_attrs,
};

int new_vfi_sync(struct vfi_sync **sync, struct vfi_location *loc, struct vfi_desc_param *desc)
{
	struct vfi_sync *new = kzalloc(sizeof(struct vfi_sync), GFP_KERNEL);
    
	*sync = new;
	if (NULL == new)
		return VFI_RESULT(-ENOMEM);

	vfi_clone_desc(&new->desc, desc);
	
	kobject_set_name(&new->kobj,"%s",new->desc.name);
	new->kobj.ktype = &vfi_sync_type;

	new->kobj.kset = &loc->syncs->kset;
	new->desc.ops = loc->desc.ops;
	new->desc.rde = loc->desc.rde;
	new->desc.ploc = loc;

	sema_init(&new->sem,1);
	init_waitqueue_head(&new->waitq);

	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,new);
	return VFI_RESULT(0);
}

int vfi_sync_register(struct vfi_sync *vfi_sync)
{
	int ret = 0;

	ret = kobject_register(&vfi_sync->kobj);

	return VFI_RESULT(ret);
}

void vfi_sync_unregister(struct vfi_sync *vfi_sync)
{
	kobject_unregister(&vfi_sync->kobj);
}

int find_vfi_sync_in(struct vfi_sync **sync, struct vfi_location *loc, struct vfi_desc_param *desc)
{
	int ret;
	struct vfi_location *tmploc;

	if (loc)
		return VFI_RESULT(loc->desc.ops->sync_find(sync,loc,desc));

	if (desc->ploc)
		return VFI_RESULT(desc->ploc->desc.ops->sync_find(sync,loc,desc));

	*sync = NULL;

	ret = locate_vfi_location(&tmploc, NULL,desc);
	if (ret)
		return VFI_RESULT(ret);

	if (tmploc) {
		ret = loc->desc.ops->sync_find(sync,tmploc,desc);
		vfi_location_put(tmploc);
	}

	return VFI_RESULT(ret);
}

/**
* vfi_sync_create : Create VFI SYNC kobject and isntall it in sysfs
*
* @loc:  pointer to vfi_location that specifies where the SYNC is located
*        in the VFI network
* @desc: pointer to command string descriptor for the command we are 
*        servicing.
*
* This function creates a kobject to represent a new VFI shared-memory 
* buffer (SYNC) and installs it in the VFI sysfs tree. It does NOT create
* the SYNC itself.
*
**/
int vfi_sync_create(struct vfi_sync **sync,struct vfi_location *loc, struct vfi_desc_param *desc)
{
	int ret = new_vfi_sync(sync,loc,desc);

	if ( ret || NULL == *sync)
		goto out;

	if ( (vfi_sync_register(*sync)) ) 
		goto fail_reg;
	
	return VFI_RESULT(0);

fail_reg:
	vfi_sync_put(*sync);
out:
	return VFI_RESULT(-EINVAL);
}


void vfi_sync_delete(struct vfi_location *loc, struct vfi_desc_param *desc)
{
	struct vfi_sync *sync;
	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
	sync = to_vfi_sync(kset_find_obj(&loc->syncs->kset,desc->name));
	vfi_sync_put(sync);
	vfi_sync_unregister(sync);
}

