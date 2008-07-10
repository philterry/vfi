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

#define MY_DEBUG      VFI_DBG_DST | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_DST | VFI_DBG_LIFE    | VFI_DBG_DEBUG
#define MY_ERROR      VFI_DBG_DST | VFI_DBG_ERROR   | VFI_DBG_ERR

#include <linux/vfi_dst.h>
#include <linux/vfi_parse.h>
#include <linux/vfi_smb.h>
#include <linux/vfi_ops.h>
#include <linux/vfi_src.h>

#include <linux/slab.h>
#include <linux/module.h>

/**
* vfi_dst_release - release (free) an vfi_dst cleanly.
*
* @kobj - pointer to an vfi_dst-type kobject to be released.
*
* This function is invoked by the kernel kobject manager when an 
* vfi_dst object has finally expired. Its job is to release any
* memory resources bound to the kobject.
*
**/
static void vfi_dst_release(struct kobject *kobj)
{
    struct vfi_dst *p = to_vfi_dst(kobj);
    VFI_DEBUG(MY_LIFE_DEBUG,"XXX %s %p (refc %lx)\n", __FUNCTION__, p, (unsigned long)kobj->kref.refcount.counter);
    vfi_clean_bind (&p->desc);
    kfree (p);
}

struct vfi_dst_attribute {
    struct attribute attr;
    ssize_t (*show)(struct vfi_dst*, char *buffer);
    ssize_t (*store)(struct vfi_dst*, const char *buffer, size_t size);
};

#define VFI_DST_ATTR(_name,_mode,_show,_store)\
 struct vfi_dst_attribute vfi_dst_attr_##_name = {\
     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },\
     .show = _show,\
     .store = _store \
};

static ssize_t vfi_dst_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct vfi_dst_attribute *pattr = container_of(attr, struct vfi_dst_attribute, attr);
    struct vfi_dst *p = to_vfi_dst(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t vfi_dst_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct vfi_dst_attribute *pattr = container_of(attr, struct vfi_dst_attribute, attr);
    struct vfi_dst *p = to_vfi_dst(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops vfi_dst_sysfs_ops = {
    .show = vfi_dst_show,
    .store = vfi_dst_store,
};


static ssize_t vfi_dst_default_show(struct vfi_dst *vfi_dst, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("Dst %p is %s.%s = %s.%s\n",vfi_dst,vfi_dst->desc.dst.name,vfi_dst->desc.dst.location,
		    vfi_dst->desc.src.name,vfi_dst->desc.src.location);
	ATTR_PRINTF("xfer: ops is %p rde is %p address is %p ploc is %p\n",
		    vfi_dst->desc.xfer.ops,vfi_dst->desc.xfer.rde,vfi_dst->desc.xfer.address,vfi_dst->desc.xfer.ploc);
	ATTR_PRINTF("dst: ops is %p rde is %p address is %p ploc is %p\n",
		    vfi_dst->desc.dst.ops,vfi_dst->desc.dst.rde,vfi_dst->desc.dst.address,vfi_dst->desc.dst.ploc);
	ATTR_PRINTF("src: ops is %p rde is %p address is %p ploc is %p\n",
		    vfi_dst->desc.src.ops,vfi_dst->desc.src.rde,vfi_dst->desc.src.address,vfi_dst->desc.src.ploc);
	ATTR_PRINTF("refcount %d\n",atomic_read(&vfi_dst->kobj.kref.refcount));
	return size;

}

VFI_DST_ATTR(default, 0644, vfi_dst_default_show, 0);

static ssize_t vfi_dst_location_show(struct vfi_dst *vfi_dst, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%s\n",vfi_dst->desc.dst.location);
}

VFI_DST_ATTR(location, 0644, vfi_dst_location_show, 0);

static ssize_t vfi_dst_name_show(struct vfi_dst *vfi_dst, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%s\n",vfi_dst->desc.dst.name);
}

VFI_DST_ATTR(name, 0644, vfi_dst_name_show, 0);

static ssize_t vfi_dst_extent_show(struct vfi_dst *vfi_dst, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%x\n",vfi_dst->desc.dst.extent);
}

VFI_DST_ATTR(extent, 0644, vfi_dst_extent_show,0);

static ssize_t vfi_dst_offset_show(struct vfi_dst *vfi_dst, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%llx\n",vfi_dst->desc.dst.offset);
}

VFI_DST_ATTR(offset, 0644, vfi_dst_offset_show, 0);

static struct attribute *vfi_dst_default_attrs[] = {
	&vfi_dst_attr_default.attr,
	&vfi_dst_attr_location.attr,
	&vfi_dst_attr_name.attr,
	&vfi_dst_attr_extent.attr,
	&vfi_dst_attr_offset.attr,
	0,
};

struct kobj_type vfi_dst_type = {
    .release = vfi_dst_release,
    .sysfs_ops = &vfi_dst_sysfs_ops,
    .default_attrs = vfi_dst_default_attrs,
};

int new_vfi_dst(struct vfi_dst **dst, struct vfi_bind *parent, struct vfi_bind_param *desc)
{
	int ret;
	struct vfi_dst *new = kzalloc(sizeof(struct vfi_dst), GFP_KERNEL);
    
	*dst = new;

	if (NULL == new)
		return VFI_RESULT(-ENOMEM);

	vfi_clone_bind(&new->desc, desc);

	new->bind = parent;

	vfi_bind_inherit(&new->desc,&parent->desc);

	new->kobj.kset = &parent->dsts->kset;

	ret = kobject_init_and_add(&new->kobj, &vfi_dst_type, NULL,
				   "%s.%s#%llx:%x",
				   new->desc.dst.name,
				   new->desc.dst.location,
				   new->desc.dst.offset,
				   new->desc.dst.extent);
	if (ret) {
		kobject_put(&new->kobj);
		*dst = NULL;
	}

	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,*dst);
	return VFI_RESULT(ret);
}

/**
* find_vfi_dst_in - find an vfi_dst object in a specified bind
* @bind : pointer to parent bind. May be NULL.
* @desc : bind descriptor
*
* This function searches the local object tree for a <dst> object
* described in @desc.
*
* If @bind is non-zero, the function will search within that bind; 
* otherwise it searches the entire tree.
*
**/
int find_vfi_dst_in(struct vfi_dst **dst,struct vfi_bind *bind, struct vfi_bind_param *desc)
{
	int ret;
	struct vfi_bind *mybind = bind;
	struct vfi_dsts *dsts;

	*dst = NULL;

	VFI_DEBUG(MY_DEBUG,"%s desc(%p)\n",__FUNCTION__,desc);

	if (mybind == NULL) {
		ret = find_vfi_bind(&mybind,&desc->xfer);
		if (ret)
			return VFI_RESULT(ret);
	}

	vfi_dsts_create(&dsts,mybind,desc);

	/*
	* Search for the <dst> at the <xfer> site.
	*
	*/
	if (mybind->desc.xfer.ops)
		ret = mybind->desc.xfer.ops->dst_find(dst,mybind,desc);

	if (!bind) {
		VFI_KTRACE ("<*** %s bind put after dst_find opcall ***>\n", __func__);
		vfi_bind_put (mybind);
	}

	return VFI_RESULT(ret ? ret : *dst == NULL);
}

int vfi_dst_create(struct vfi_dst **dst, struct vfi_bind *bind, struct vfi_bind_param *desc)
{
	int ret;

	ret = new_vfi_dst(dst,bind,desc);

	return VFI_RESULT(ret);
}

void vfi_dst_delete (struct vfi_bind *bind, struct vfi_bind_param *desc)
{
	struct vfi_dst *dst;
	int ret;
	char *buf = kmalloc(128,GFP_KERNEL);

	VFI_DEBUG(MY_DEBUG,"%s: %s.%s#%llx:%x/%s.%s#%llx:%x\n", __FUNCTION__, 
		    desc->xfer.name, desc->xfer.location,desc->xfer.offset, desc->xfer.extent, 
		    desc->dst.name, desc->dst.location,desc->dst.offset, desc->dst.extent);
	
	ret = snprintf (buf, 128, "%s.%s#%llx:%x",
			desc->dst.name, desc->dst.location, desc->dst.offset, desc->dst.extent);
	if (ret >= 128) {
		VFI_DEBUG(MY_DEBUG, "%s buffer not big enough\n",__FUNCTION__);
		buf = krealloc(&buf,ret+1,GFP_KERNEL);
		snprintf (buf, ret, "%s.%s#%llx:%x",
			  desc->dst.name, desc->dst.location, desc->dst.offset, desc->dst.extent);
	}
	
	dst = to_vfi_dst (kset_find_obj (&bind->dsts->kset, buf));
	if (dst) {
		vfi_dst_put(dst);		/* Put, to counteract the find... */
		vfi_dst_put(dst);
	}
	kfree(buf);
}

void vfi_dst_load_srcs(struct vfi_dst *dst)
{
	struct list_head * entry;
	struct vfi_src * src2 = NULL; 
	VFI_DEBUG(MY_DEBUG,"%s %p\n",__FUNCTION__,dst->srcs);
	if (dst->srcs) {
		spin_lock(&dst->srcs->kset.list_lock);
		if (!list_empty(&dst->srcs->kset.list)) {
			list_for_each(entry,&dst->srcs->kset.list) {
				src2 = to_vfi_src(to_kobj(entry));
				VFI_DEBUG(MY_DEBUG,"%s src2 %p, name = %s\n",__FUNCTION__,src2->desc.src.rde, &src2->kobj.name[0]);
				if (src2->desc.src.rde && src2->desc.src.rde->ops && src2->desc.src.rde->ops->load && src2->desc.src.rde->ops->link_src) {
					src2->desc.src.rde->ops->load(src2);
					src2->desc.src.rde->ops->link_src(&dst->srcs->dma_chain, src2);
				}
				else {
					VFI_DEBUG (MY_DEBUG, "xx %s failed: src \"%s\" is missing RDE ops\n", 
							__FUNCTION__, src2->desc.src.name);
				}
			}
		}
		spin_unlock(&dst->srcs->kset.list_lock);
	}
}
