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

#include <linux/vfi_mmap.h>
#include <linux/vfi_smb.h>
#include <linux/vfi_smbs.h>
#include <linux/vfi_mmaps.h>
#include <linux/vfi_drv.h>
#include <linux/vfi_location.h>

#include <linux/slab.h>
#include <linux/module.h>

static void vfi_mmap_release(struct kobject *kobj)
{
    struct vfi_mmap *p = to_vfi_mmap(kobj);
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
    return snprintf(buffer, PAGE_SIZE, "vfi_mmap_default");
}

static ssize_t vfi_mmap_default_store(struct vfi_mmap *vfi_mmap, const char *buffer, size_t size)
{
    return size;
}

VFI_MMAP_ATTR(default, 0644, vfi_mmap_default_show, vfi_mmap_default_store);

static ssize_t vfi_mmap_offset_show(struct vfi_mmap *vfi_mmap, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "vfi_mmap_offset");
}

static ssize_t vfi_mmap_offset_store(struct vfi_mmap *vfi_mmap, const char *buffer, size_t size)
{
    return size;
}

VFI_MMAP_ATTR(offset, 0644, vfi_mmap_offset_show, vfi_mmap_offset_store);

static ssize_t vfi_mmap_extent_show(struct vfi_mmap *vfi_mmap, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "vfi_mmap_extent");
}

static ssize_t vfi_mmap_extent_store(struct vfi_mmap *vfi_mmap, const char *buffer, size_t size)
{
    return size;
}

VFI_MMAP_ATTR(extent, 0644, vfi_mmap_extent_show, vfi_mmap_extent_store);

static ssize_t vfi_mmap_pid_show(struct vfi_mmap *vfi_mmap, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "vfi_mmap_pid");
}

static ssize_t vfi_mmap_pid_store(struct vfi_mmap *vfi_mmap, const char *buffer, size_t size)
{
    return size;
}

VFI_MMAP_ATTR(pid, 0644, vfi_mmap_pid_show, vfi_mmap_pid_store);

static struct attribute *vfi_mmap_default_attrs[] = {
    &vfi_mmap_attr_default.attr,
    &vfi_mmap_attr_offset.attr,
    &vfi_mmap_attr_extent.attr,
    &vfi_mmap_attr_pid.attr,
    0,
};

struct kobj_type vfi_mmap_type = {
    .release = vfi_mmap_release,
    .sysfs_ops = &vfi_mmap_sysfs_ops,
    .default_attrs = vfi_mmap_default_attrs,
};

int find_vfi_mmap(struct vfi_mmap **mmap, struct vfi_smb *smb, struct vfi_desc_param *desc)
{
	char buf[512];
	snprintf(buf,512,"%d#%llx:%x",current->pid,desc->offset,desc->extent);
	*mmap = to_vfi_mmap(kset_find_obj(&smb->mmaps->kset,buf));
	return *mmap == NULL;
}
static struct vfi_mmap *frm_by_loc(struct vfi_location *loc,unsigned long tid)
{
	struct vfi_location *new_loc;
	struct vfi_smb *smb;
	struct vfi_mmap *mmap = NULL;

	spin_lock(&loc->kset.list_lock);
	list_for_each_entry(new_loc,&loc->kset.list, kset.kobj.entry) {
		if ((mmap = frm_by_loc(new_loc,tid)))
			goto outloc;
	}
	spin_unlock(&loc->kset.list_lock);

	spin_lock(&loc->smbs->kset.list_lock);
	list_for_each_entry(smb,&loc->smbs->kset.list,kobj.entry) {

		spin_lock(&smb->mmaps->kset.list_lock);
		list_for_each_entry(mmap,&smb->mmaps->kset.list,kobj.entry) {
			if (is_mmap_ticket(mmap,tid))
				goto out;
		}
		spin_unlock(&smb->mmaps->kset.list_lock);
	}
	spin_unlock(&loc->smbs->kset.list_lock);

	return NULL;
out:
	spin_unlock(&smb->mmaps->kset.list_lock);
	spin_unlock(&loc->smbs->kset.list_lock);
	return mmap;
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
		if ((*mmap = frm_by_loc(loc,tid))) 
			goto out;
	}
	ret = -EINVAL;
out:
	spin_unlock(&vfi_subsys->kset.list_lock);
	return ret;
}

static int vfi_mmap_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *vfi_mmap_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int vfi_mmap_uevent(struct kset *kset, struct kobject *kobj, char **envp, int num_envp, char *buffer, int buf_size)
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
    struct vfi_mmap *new = kzalloc(sizeof(struct vfi_mmap), GFP_KERNEL);
    
    *mmap = new;

    if (NULL == new)
	return -ENOMEM;

    kobject_set_name(&new->kobj,"%d#%llx:%x",current->pid, desc->offset,desc->extent);
    new->kobj.ktype = &vfi_mmap_type;
    new->kobj.kset = &parent->mmaps->kset;

    return 0;
}

int vfi_mmap_register(struct vfi_mmap *vfi_mmap)
{
    int ret = 0;

    if ( (ret = kobject_register(&vfi_mmap->kobj) ) )
	goto out;

      return ret;

out:
    return ret;
}

void vfi_mmap_unregister(struct vfi_mmap *vfi_mmap)
{
    
     kobject_unregister(&vfi_mmap->kobj);
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
		return 0;
	}
	ret = new_vfi_mmap(mmap,smb,desc);
	if (!ret) {
		ret = vfi_mmap_register(*mmap);
		if (!ret) {
			(*mmap)->pg_tbl = &pg_tbl[firstpage];
			(*mmap)->n_pg = lastpage - firstpage + 1;
		}
		else {
			vfi_mmap_put(*mmap);
			*mmap = NULL;
		}
	}
	VFI_DEBUG_SAFE (MY_DEBUG, *mmap, "-- Assigned %lu pages at %p\n",(*mmap)->n_pg, (*mmap)->pg_tbl);

	return ret;
}

void vfi_mmap_delete(struct vfi_smb *smb, struct vfi_desc_param *desc)
{
	struct vfi_mmap *mmap;
	int ret;
	ret = find_vfi_mmap(&mmap,smb,desc);
	vfi_mmap_unregister(mmap);
}
