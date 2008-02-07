/* 
 * 
 * Copyright 2007 MicroMemory, LLC. 
 * Trevor Anderson <tanderson@micromemory.com> 
 * 
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or  (at your
 * option) any later version.
 */

#define MY_DEBUG      RDDMA_DBG_MMAP | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_MMAP | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/rddma_mmap.h>
#include <linux/rddma_smb.h>
#include <linux/rddma_smbs.h>
#include <linux/rddma_mmaps.h>
#include <linux/rddma_drv.h>
#include <linux/rddma_location.h>

#include <linux/slab.h>
#include <linux/module.h>

static void rddma_mmap_release(struct kobject *kobj)
{
    struct rddma_mmap *p = to_rddma_mmap(kobj);
    kfree(p);
}

struct rddma_mmap_attribute {
    struct attribute attr;
    ssize_t (*show)(struct rddma_mmap*, char *buffer);
    ssize_t (*store)(struct rddma_mmap*, const char *buffer, size_t size);
};

#define RDDMA_MMAP_ATTR(_name,_mode,_show,_store) struct rddma_mmap_attribute rddma_mmap_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t rddma_mmap_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct rddma_mmap_attribute *pattr = container_of(attr, struct rddma_mmap_attribute, attr);
    struct rddma_mmap *p = to_rddma_mmap(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t rddma_mmap_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct rddma_mmap_attribute *pattr = container_of(attr, struct rddma_mmap_attribute, attr);
    struct rddma_mmap *p = to_rddma_mmap(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops rddma_mmap_sysfs_ops = {
    .show = rddma_mmap_show,
    .store = rddma_mmap_store,
};


static ssize_t rddma_mmap_default_show(struct rddma_mmap *rddma_mmap, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "rddma_mmap_default");
}

static ssize_t rddma_mmap_default_store(struct rddma_mmap *rddma_mmap, const char *buffer, size_t size)
{
    return size;
}

RDDMA_MMAP_ATTR(default, 0644, rddma_mmap_default_show, rddma_mmap_default_store);

static ssize_t rddma_mmap_offset_show(struct rddma_mmap *rddma_mmap, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "rddma_mmap_offset");
}

static ssize_t rddma_mmap_offset_store(struct rddma_mmap *rddma_mmap, const char *buffer, size_t size)
{
    return size;
}

RDDMA_MMAP_ATTR(offset, 0644, rddma_mmap_offset_show, rddma_mmap_offset_store);

static ssize_t rddma_mmap_extent_show(struct rddma_mmap *rddma_mmap, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "rddma_mmap_extent");
}

static ssize_t rddma_mmap_extent_store(struct rddma_mmap *rddma_mmap, const char *buffer, size_t size)
{
    return size;
}

RDDMA_MMAP_ATTR(extent, 0644, rddma_mmap_extent_show, rddma_mmap_extent_store);

static ssize_t rddma_mmap_pid_show(struct rddma_mmap *rddma_mmap, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "rddma_mmap_pid");
}

static ssize_t rddma_mmap_pid_store(struct rddma_mmap *rddma_mmap, const char *buffer, size_t size)
{
    return size;
}

RDDMA_MMAP_ATTR(pid, 0644, rddma_mmap_pid_show, rddma_mmap_pid_store);

static struct attribute *rddma_mmap_default_attrs[] = {
    &rddma_mmap_attr_default.attr,
    &rddma_mmap_attr_offset.attr,
    &rddma_mmap_attr_extent.attr,
    &rddma_mmap_attr_pid.attr,
    0,
};

struct kobj_type rddma_mmap_type = {
    .release = rddma_mmap_release,
    .sysfs_ops = &rddma_mmap_sysfs_ops,
    .default_attrs = rddma_mmap_default_attrs,
};

struct rddma_mmap *find_rddma_mmap(struct rddma_smb *smb, struct rddma_desc_param *desc)
{
	char buf[512];
	struct rddma_mmap *mmap;
	snprintf(buf,512,"%d#%llx:%x",current->pid,desc->offset,desc->extent);
	mmap = to_rddma_mmap(kset_find_obj(&smb->mmaps->kset,buf));
	return mmap;
}
static struct rddma_mmap *frm_by_loc(struct rddma_location *loc,unsigned long tid)
{
	struct rddma_location *new_loc;
	struct rddma_smb *smb;
	struct rddma_mmap *mmap = NULL;

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

struct rddma_mmap *find_rddma_mmap_by_id(unsigned long tid)
{
	struct rddma_location *loc;
	struct rddma_mmap *mmap = NULL;
	spin_lock(&rddma_subsys->kset.list_lock);
	list_for_each_entry(loc, &rddma_subsys->kset.list, kset.kobj.entry) {
		if ((mmap = frm_by_loc(loc,tid))) 
			goto out;
	}
out:
	spin_unlock(&rddma_subsys->kset.list_lock);
	return mmap;
}

static int rddma_mmap_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *rddma_mmap_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int rddma_mmap_uevent(struct kset *kset, struct kobject *kobj, char **envp, int num_envp, char *buffer, int buf_size)
{
	return 0; /* Do not generate event */
}


static struct kset_uevent_ops rddma_mmap_uevent_ops = {
	.filter = rddma_mmap_uevent_filter,
	.name = rddma_mmap_uevent_name,
	.uevent = rddma_mmap_uevent,
};

struct rddma_mmap *new_rddma_mmap(struct rddma_smb *parent, struct rddma_desc_param *desc)
{
    struct rddma_mmap *new = kzalloc(sizeof(struct rddma_mmap), GFP_KERNEL);
    
    if (NULL == new)
	return new;

    kobject_set_name(&new->kobj,"%d#%llx:%x",current->pid, desc->offset,desc->extent);
    new->kobj.ktype = &rddma_mmap_type;
    new->kobj.kset = &parent->mmaps->kset;

    return new;
}

int rddma_mmap_register(struct rddma_mmap *rddma_mmap)
{
    int ret = 0;

    if ( (ret = kobject_register(&rddma_mmap->kobj) ) )
	goto out;

      return ret;

out:
    return ret;
}

void rddma_mmap_unregister(struct rddma_mmap *rddma_mmap)
{
    
     kobject_unregister(&rddma_mmap->kobj);
}

struct rddma_mmap *rddma_mmap_create(struct rddma_smb *smb, struct rddma_desc_param *desc)
{
	struct page **pg_tbl = smb->pages;
	unsigned long n_pg = smb->num_pages;
	unsigned long offset = desc->offset;
	unsigned long extent = desc->extent;
	unsigned long firstpage;
	unsigned long lastpage;
	struct rddma_mmap* mmap = NULL;

	RDDMA_DEBUG (MY_DEBUG, "rddma_mmap_create: %lu-bytes at offset %lu of %lu-page table %p\n", 
		extent, offset, n_pg, pg_tbl);

	firstpage = (offset >> PAGE_SHIFT);
	lastpage = ((offset + extent - 1) >> PAGE_SHIFT);
	if (lastpage >= n_pg) {
		RDDMA_DEBUG (MY_DEBUG, "xx Requested region exceeds page table.\n"); 
		return 0;
	}
	
	if ( (mmap = new_rddma_mmap(smb,desc)) ) {
		if ( !rddma_mmap_register(mmap) ) {
			mmap->pg_tbl = &pg_tbl[firstpage];
			mmap->n_pg = lastpage - firstpage + 1;
		}
		else {
			rddma_mmap_put(mmap);
			mmap = NULL;
		}
	}
	RDDMA_DEBUG_SAFE (MY_DEBUG, mmap, "-- Assigned %lu pages at %p\n",mmap->n_pg, mmap->pg_tbl);
	return mmap;
}

void rddma_mmap_delete(struct rddma_smb *smb, struct rddma_desc_param *desc)
{
	struct rddma_mmap *mmap = find_rddma_mmap(smb,desc);
	
	
	rddma_mmap_unregister(mmap);
}
