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

#define MY_DEBUG      VFI_DBG_SMB | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_SMB | VFI_DBG_LIFE    | VFI_DBG_DEBUG
#define MY_ERROR      VFI_DBG_SMB | VFI_DBG_ERROR   | VFI_DBG_ERR

#include <linux/vfi_smb.h>
#include <linux/vfi_parse.h>
#include <linux/vfi_drv.h>
#include <linux/vfi_smb.h>
#include <linux/vfi_smbs.h>
#include <linux/vfi_location.h>
#include <linux/vfi_src.h>
#include <linux/vfi_dst.h>
#include <linux/vfi_ops.h>
#include <linux/vfi_mmaps.h>


static void vfi_smb_release(struct kobject *kobj)
{
	struct vfi_smb *p = to_vfi_smb(kobj);
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,p);
	if (p->desc.name) {
		VFI_DEBUG(MY_LIFE_DEBUG,"%s name %p\n",__FUNCTION__,p->desc.name);
		kfree(p->desc.name);
	}
	if (p->pages)
		vfi_dealloc_pages(p->pages, p->num_pages);
	kfree(p);
}

struct vfi_smb_attribute {
	struct attribute attr;
	ssize_t (*show)(struct vfi_smb*, char *buffer);
	ssize_t (*store)(struct vfi_smb*, const char *buffer, size_t size);
};

#define VFI_SMB_ATTR(_name,_mode,_show,_store) struct vfi_smb_attribute vfi_smb_attr_##_name = {\
     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },\
     .show = _show,\
     .store = _store\
 };

static ssize_t vfi_smb_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
	struct vfi_smb_attribute *pattr = container_of(attr, struct vfi_smb_attribute, attr);
	struct vfi_smb *p = to_vfi_smb(kobj);

	if (pattr && pattr->show)
		return pattr->show(p,buffer);

	return 0;
}

static ssize_t vfi_smb_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
	struct vfi_smb_attribute *pattr = container_of(attr, struct vfi_smb_attribute, attr);
	struct vfi_smb *p = to_vfi_smb(kobj);

	if (pattr && pattr->store)
		return pattr->store(p, buffer, size);

	return 0;
}

static struct sysfs_ops vfi_smb_sysfs_ops = {
	.show = vfi_smb_show,
	.store = vfi_smb_store,
};


static ssize_t vfi_smb_default_show(struct vfi_smb *vfi_smb, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("Smb %p is %s \n",vfi_smb,vfi_smb ? vfi_smb->desc.name : NULL);
	if (vfi_smb) {
		ATTR_PRINTF("ops is %p rde is %p address is %p\n",vfi_smb->desc.ops,vfi_smb->desc.rde,vfi_smb->desc.address);
	}
	ATTR_PRINTF("refcount %d\n",atomic_read(&vfi_smb->kobj.kref.refcount));
	return size;
}

static ssize_t vfi_smb_default_store(struct vfi_smb *vfi_smb, const char *buffer, size_t size)
{
    return size;
}

VFI_SMB_ATTR(default, 0644, vfi_smb_default_show, vfi_smb_default_store);

static ssize_t vfi_smb_location_show(struct vfi_smb *vfi_smb, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("Smb %p is %s \n",vfi_smb,vfi_smb ? vfi_smb->desc.name : NULL);
	if (vfi_smb) {
		ATTR_PRINTF("ops is %p rde is %p address is %p\n",vfi_smb->desc.ops,vfi_smb->desc.rde,vfi_smb->desc.address);
	}
	return size;
}

static ssize_t vfi_smb_location_store(struct vfi_smb *vfi_smb, const char *buffer, size_t size)
{
    return size;
}

VFI_SMB_ATTR(location, 0644, vfi_smb_location_show, vfi_smb_location_store);

static ssize_t vfi_smb_name_show(struct vfi_smb *vfi_smb, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%s\n", vfi_smb->desc.name);
}

VFI_SMB_ATTR(name, 0444, vfi_smb_name_show, 0);

static ssize_t vfi_smb_offset_show(struct vfi_smb *vfi_smb, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%llx\n", vfi_smb->desc.offset);
}

VFI_SMB_ATTR(offset, 0444, vfi_smb_offset_show, 0);

static ssize_t vfi_smb_extent_show(struct vfi_smb *vfi_smb, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%x\n", vfi_smb->desc.extent);
}

VFI_SMB_ATTR(extent, 0444, vfi_smb_extent_show, 0);

static struct attribute *vfi_smb_default_attrs[] = {
    &vfi_smb_attr_default.attr,
    &vfi_smb_attr_location.attr,
    &vfi_smb_attr_name.attr,
    &vfi_smb_attr_offset.attr,
    &vfi_smb_attr_extent.attr,
    0,
};

struct kobj_type vfi_smb_type = {
	.release = vfi_smb_release,
	.sysfs_ops = &vfi_smb_sysfs_ops,
	.default_attrs = vfi_smb_default_attrs,
};

int new_vfi_smb(struct vfi_smb **smb, struct vfi_location *loc, struct vfi_desc_param *desc)
{
	int ret = 0;
	struct vfi_smb *new = kzalloc(sizeof(struct vfi_smb), GFP_KERNEL);
    
	*smb = new;
	if (NULL == new)
		return VFI_RESULT(-ENOMEM);

	vfi_clone_desc(&new->desc, desc);
	new->size = new->desc.extent;
	
	new->desc.ops = loc->desc.ops;
	new->desc.rde = loc->desc.rde;
	new->desc.ploc = loc;

	ret = kobject_init_and_add(&new->kobj, &vfi_smb_type, &loc->smbs->kset.kobj, "%s",new->desc.name);
	if (ret)
		goto out;

	ret = new_vfi_mmaps(&new->mmaps, new, "mmaps");
	if (ret)
		goto mmaps;

	return VFI_RESULT(ret);
mmaps:
	kset_unregister(&new->mmaps->kset);
out:
	kobject_put(&new->kobj);

	return VFI_RESULT(ret);
}

int find_vfi_smb_in(struct vfi_smb **smb, struct vfi_location *loc, struct vfi_desc_param *desc)
{
	int ret;
	struct vfi_location *tmploc;

	if (loc)
		return VFI_RESULT(loc->desc.ops->smb_find(smb,loc,desc));

	if (desc->ploc)
		return VFI_RESULT(desc->ploc->desc.ops->smb_find(smb,loc,desc));

	*smb = NULL;

	ret = locate_vfi_location(&tmploc, NULL,desc);
	if (ret)
		return VFI_RESULT(ret);

	if (tmploc) {
		ret = tmploc->desc.ops->smb_find(smb,tmploc,desc);
		vfi_location_put(tmploc);
	}

	return VFI_RESULT(ret);
}

/**
* vfi_smb_create : Create VFI SMB kobject and isntall it in sysfs
*
* @loc:  pointer to vfi_location that specifies where the SMB is located
*        in the VFI network
* @desc: pointer to command string descriptor for the command we are 
*        servicing.
*
* This function creates a kobject to represent a new VFI shared-memory 
* buffer (SMB) and installs it in the VFI sysfs tree. It does NOT create
* the SMB itself.
*
**/
int vfi_smb_create(struct vfi_smb **smb,struct vfi_location *loc, struct vfi_desc_param *desc)
{
	int ret = new_vfi_smb(smb,loc,desc);

	if (ret)
		vfi_smb_put(*smb);

	return VFI_RESULT(ret);
}


void vfi_smb_delete(struct vfi_smb *smb)
{
	if (smb) {
		vfi_mmaps_unregister(smb->mmaps);
		kobject_del(&smb->kobj);
	}
}

