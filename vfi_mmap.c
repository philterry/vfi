/* 
 * 
 * Copyright 2008 Vmetro 
 * Trevor Anderson <tanderson@vmetro.com> 
 * 
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or  (at your
 * option) any later version.
 */

#define MY_DEBUG      VFI_DBG_MMAP | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_MMAP | VFI_DBG_LIFE    | VFI_DBG_DEBUG
#define MY_ERROR      VFI_DBG_MMAP | VFI_DBG_ERROR   | VFI_DBG_ERR

#include <linux/vfi_mmap.h>
#include <linux/vfi_smb.h>
#include <linux/vfi_smbs.h>
#include <linux/vfi_drv.h>
#include <linux/vfi_location.h>
#include <linux/vfi_ops.h>

#include <linux/slab.h>
#include <linux/module.h>

static void vfi_mmap_release(struct kobject *kobj)
{
    struct vfi_mmap *p = to_vfi_mmap(kobj);
    VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__func__,p);
    vfi_smb_put(p->smb);
    vfi_clean_desc(&p->desc);
    kfree(p);
}

struct vfi_mmap_attribute {
    struct attribute attr;
    ssize_t (*show)(struct vfi_mmap*, char *buffer);
    ssize_t (*store)(struct vfi_mmap*, const char *buffer, size_t size);
};

#define VFI_MMAP_ATTR(_name,_mode,_show,_store) struct vfi_mmap_attribute vfi_mmap_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t vfi_mmap_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct vfi_mmap_attribute *pattr = container_of(attr, struct vfi_mmap_attribute, attr);
    struct vfi_mmap *p = to_vfi_mmap(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t vfi_mmap_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct vfi_mmap_attribute *pattr = container_of(attr, struct vfi_mmap_attribute, attr);
    struct vfi_mmap *p = to_vfi_mmap(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops vfi_mmap_sysfs_ops = {
    .show = vfi_mmap_show,
    .store = vfi_mmap_store,
};


static ssize_t vfi_mmap_default_show(struct vfi_mmap *vfi_mmap, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("Mmap %p is %s \n",vfi_mmap,vfi_mmap ? vfi_mmap->desc.name : NULL);
	ATTR_PRINTF("mmap: ops is %p rde is %p address is %p ploc is %p\n",vfi_mmap->desc.ops,vfi_mmap->desc.rde,vfi_mmap->desc.address, vfi_mmap->desc.ploc);
	ATTR_PRINTF("refcount %d\n",atomic_read(&vfi_mmap->kobj.kref.refcount));
	return size;
}

static ssize_t vfi_mmap_default_store(struct vfi_mmap *vfi_mmap, const char *buffer, size_t size)
{
    return size;
}

VFI_MMAP_ATTR(default, 0644, vfi_mmap_default_show, vfi_mmap_default_store);

static ssize_t vfi_mmap_offset_show(struct vfi_mmap *vfi_mmap, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("%llx\n", vfi_mmap->desc.offset);
	return size;
}

static ssize_t vfi_mmap_offset_store(struct vfi_mmap *vfi_mmap, const char *buffer, size_t size)
{
    return size;
}

VFI_MMAP_ATTR(offset, 0644, vfi_mmap_offset_show, vfi_mmap_offset_store);

static ssize_t vfi_mmap_extent_show(struct vfi_mmap *vfi_mmap, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("%x\n", vfi_mmap->desc.extent);
	return size;
}

static ssize_t vfi_mmap_extent_store(struct vfi_mmap *vfi_mmap, const char *buffer, size_t size)
{
    return size;
}

VFI_MMAP_ATTR(extent, 0644, vfi_mmap_extent_show, vfi_mmap_extent_store);

static ssize_t vfi_mmap_tid_show(struct vfi_mmap *vfi_mmap, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("%lx\n", vfi_mmap->t_id);
	return size;
}

static ssize_t vfi_mmap_tid_store(struct vfi_mmap *vfi_mmap, const char *buffer, size_t size)
{
    return size;
}

VFI_MMAP_ATTR(tid, 0644, vfi_mmap_tid_show, vfi_mmap_tid_store);

static struct attribute *vfi_mmap_default_attrs[] = {
    &vfi_mmap_attr_default.attr,
    &vfi_mmap_attr_offset.attr,
    &vfi_mmap_attr_extent.attr,
    &vfi_mmap_attr_tid.attr,
    0,
};

struct kobj_type vfi_mmap_type = {
    .release = vfi_mmap_release,
    .sysfs_ops = &vfi_mmap_sysfs_ops,
    .default_attrs = vfi_mmap_default_attrs,
};

int find_vfi_mmap_in(struct vfi_mmap **mmap, struct vfi_smb *smb, struct vfi_desc_param *desc)
{
	int ret;
	struct vfi_smb *tmpsmb;
	VFI_DEBUG(MY_DEBUG,"%s desc(%p) ploc(%p)\n",__FUNCTION__,desc,desc->ploc);

	if (smb && smb->desc.ops && smb->desc.ops->mmap_find)
		return VFI_RESULT(smb->desc.ops->mmap_find(mmap,smb,desc));

	*mmap = NULL;

	ret = find_vfi_smb(&tmpsmb,desc);
	if (ret)
		return VFI_RESULT(ret);

	if (tmpsmb && tmpsmb->desc.ops && tmpsmb->desc.ops->mmap_find) {
		ret = tmpsmb->desc.ops->mmap_find(mmap,tmpsmb,desc);
		vfi_smb_put(tmpsmb);
	}

	return VFI_RESULT(ret);
}

static struct vfi_mmap *frm_by_loc(struct vfi_location *loc,unsigned long tid)
{
	struct vfi_location *new_loc;
	struct vfi_smb *smb;
	struct vfi_mmap *mmap = NULL;
	VFI_DEBUG(MY_DEBUG,"%s: %p %lx %s.%s\n",__func__,loc,tid,loc->desc.name,loc->desc.location);
	spin_lock(&loc->kset.list_lock);
	list_for_each_entry(new_loc,&loc->kset.list, kset.kobj.entry) {
		VFI_DEBUG(MY_DEBUG,"%s: %p %s.%s\n",__func__,new_loc,new_loc->desc.name,new_loc->desc.location);
		if ((mmap = frm_by_loc(new_loc,tid)))
			goto outloc;
	}
  	spin_unlock(&loc->kset.list_lock);

	VFI_DEBUG(MY_DEBUG,"%s: loc %p %s.%s smbs %p\n",__func__,loc,loc->desc.name, loc->desc.location, loc->smbs);
	spin_lock(&loc->smbs->kset.list_lock);
	list_for_each_entry(smb,&loc->smbs->kset.list,kset.kobj.entry) {
		VFI_DEBUG(MY_DEBUG,"%s: smb %s.%s\n",__func__,smb->desc.name,smb->desc.location);
		spin_lock(&smb->kset.list_lock);
		list_for_each_entry(mmap,&smb->kset.list,kobj.entry) {
			VFI_DEBUG(MY_DEBUG,"%s: %p %s.%s %lx\n",__func__,mmap,mmap->desc.name, mmap->desc.location, mmap->t_id);
			if (is_mmap_ticket(mmap,tid))
				goto out;
		}
		spin_unlock(&smb->kset.list_lock);
	}
	spin_unlock(&loc->smbs->kset.list_lock);

	return NULL;
out:
	spin_unlock(&smb->kset.list_lock);
	spin_unlock(&loc->smbs->kset.list_lock);
	return vfi_mmap_get(mmap);
outloc:
	spin_unlock(&loc->kset.list_lock);
	return mmap;
}

int find_vfi_mmap_by_id(struct vfi_mmap **mmap, unsigned long tid)
{
	struct vfi_location *loc;
	int ret = 0;

	spin_lock(&vfi_subsys->kset.list_lock);
	list_for_each_entry(loc, &vfi_subsys->kset.list, kset.kobj.entry) {
		VFI_DEBUG(MY_DEBUG, "%s: %p %s.%s\n",__func__,loc,loc->desc.name,loc->desc.location);
		if ((*mmap = frm_by_loc(loc,tid))) 
			goto out;
	}
	ret = -EINVAL;
out:
	spin_unlock(&vfi_subsys->kset.list_lock);
	return VFI_RESULT(ret);
}

static int vfi_mmap_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *vfi_mmap_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int vfi_mmap_uevent(struct kset *kset, struct kobject *kobj, struct kobj_uevent_env *env)
{
	return 0; /* Do not generate event */
}


static struct kset_uevent_ops vfi_mmap_uevent_ops = {
	.filter = vfi_mmap_uevent_filter,
	.name = vfi_mmap_uevent_name,
	.uevent = vfi_mmap_uevent,
};

int new_vfi_mmap(struct vfi_mmap **mmap, struct vfi_smb *parent, struct vfi_desc_param *desc)
{
	int ret;
	struct vfi_mmap *new = kzalloc(sizeof(struct vfi_mmap), GFP_KERNEL);
    
	*mmap = new;

	if (NULL == new)
		return VFI_RESULT(-ENOMEM);

	vfi_clone_desc(&new->desc, desc);
	vfi_inherit(&new->desc, &parent->desc);
	new->kobj.kset = &parent->kset;
	ret = kobject_init_and_add(&new->kobj, &vfi_mmap_type, NULL, "#%llx:%x",desc->offset,desc->extent);
	if (ret)
		kobject_put(&new->kobj);

	return VFI_RESULT(ret);
}

int vfi_mmap_create(struct vfi_mmap **mmap, struct vfi_smb *smb, struct vfi_desc_param *desc)
{
	struct page **pg_tbl = smb->pages;
	unsigned long n_pg = smb->num_pages;
	unsigned long offset = desc->offset;
	unsigned long extent = desc->extent;
	unsigned long firstpage;
	unsigned long lastpage;
	int ret;

	VFI_DEBUG (MY_DEBUG, "vfi_mmap_create: %lu-bytes at offset %lu of %lu-page table %p\n", 
		extent, offset, n_pg, pg_tbl);

	firstpage = (offset >> PAGE_SHIFT);
	lastpage = ((offset + extent - 1) >> PAGE_SHIFT);
	if (lastpage >= n_pg) {
		VFI_DEBUG (MY_DEBUG, "xx Requested region exceeds page table.\n"); 
		return VFI_RESULT(0);
	}

	ret = new_vfi_mmap(mmap,smb,desc);
	if (!ret) {
		(*mmap)->pg_tbl = &pg_tbl[firstpage];
		(*mmap)->n_pg = lastpage - firstpage + 1;
		(*mmap)->t_id = mmap_to_ticket(*mmap);
		(*mmap)->smb = vfi_smb_get(smb);
		VFI_DEBUG_SAFE (MY_DEBUG, *mmap, "-- Assigned %lu pages at %p\n",(*mmap)->n_pg, (*mmap)->pg_tbl);
	}

	return VFI_RESULT(ret);
}

void vfi_mmap_delete(struct vfi_mmap *mmap)
{
	vfi_mmap_put(mmap);
}
