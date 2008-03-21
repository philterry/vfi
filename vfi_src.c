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

#define MY_DEBUG      VFI_DBG_SRC | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_SRC | VFI_DBG_LIFE    | VFI_DBG_DEBUG

#include <linux/vfi_src.h>
#include <linux/vfi_parse.h>
#include <linux/vfi_smb.h>

#include <linux/slab.h>
#include <linux/module.h>

/**
* vfi_src_release - release (free) an vfi_src cleanly.
*
* @kobj - pointer to an vfi_src-type kobject to be released.
*
* This function is invoked by the kernel kobject manager when an 
* vfi_src object has finally expired. Its job is to release any
* memory resources bound to the kobject.
*
**/
static void vfi_src_release (struct kobject *kobj)
{
	struct vfi_src *p = to_vfi_src(kobj);
	VFI_DEBUG(MY_LIFE_DEBUG,"XXX %s %p (refc %lx)\n", __FUNCTION__, p, (unsigned long)kobj->kref.refcount.counter);
	
	/*
	* Free embedded resources - an vfi_bind_params
	*/
	vfi_clean_bind (&p->desc);
	/*
	* Free the vfi_src structure itself.
	*
	* Note that, due to alignment finagling, the active pointer
	* might be different from the originally kzalloc'ed pointer.
	* The original address - if different - is embedded within
	* the working structure body. If the embedded pointer is NULL 
	* then the working pointer is good.
	*/
	kfree ((p->free_p) ? : p);
}

struct vfi_src_attribute {
    struct attribute attr;
    ssize_t (*show)(struct vfi_src*, char *buffer);
    ssize_t (*store)(struct vfi_src*, const char *buffer, size_t size);
};

#define VFI_SRC_ATTR(_name,_mode,_show,_store) \
struct vfi_src_attribute vfi_src_attr_##_name = {\
     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },\
     .show = _show,\
     .store = _store \
};

static ssize_t vfi_src_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct vfi_src_attribute *pattr = container_of(attr, struct vfi_src_attribute, attr);
    struct vfi_src *p = to_vfi_src(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t vfi_src_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct vfi_src_attribute *pattr = container_of(attr, struct vfi_src_attribute, attr);
    struct vfi_src *p = to_vfi_src(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops vfi_src_sysfs_ops = {
    .show = vfi_src_show,
    .store = vfi_src_store,
};


static ssize_t vfi_src_default_show(struct vfi_src *vfi_src, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("Src %p is %s = %s\n",vfi_src,vfi_src->desc.dst.name,vfi_src->desc.src.name);
	if (vfi_src) {
		ATTR_PRINTF("dst: ops is %p rde is %p address is %p\n",vfi_src->desc.dst.ops,vfi_src->desc.dst.rde,vfi_src->desc.dst.address);
		ATTR_PRINTF("src: ops is %p rde is %p address is %p\n",vfi_src->desc.src.ops,vfi_src->desc.src.rde,vfi_src->desc.src.address);
	}
	return size;

}

VFI_SRC_ATTR(default, 0644, vfi_src_default_show, 0);

static ssize_t vfi_src_location_show(struct vfi_src *vfi_src, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%s\n", vfi_src->desc.src.location);
}

VFI_SRC_ATTR(location, 0644, vfi_src_location_show, 0);

static ssize_t vfi_src_name_show(struct vfi_src *vfi_src, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%s\n", vfi_src->desc.src.name);
}

VFI_SRC_ATTR(name, 0644, vfi_src_name_show, 0);

static ssize_t vfi_src_extent_show(struct vfi_src *vfi_src, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%x\n",vfi_src->desc.src.extent);
}

VFI_SRC_ATTR(extent, 0644, vfi_src_extent_show, 0);

static ssize_t vfi_src_offset_show(struct vfi_src *vfi_src, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%llx\n",vfi_src->desc.src.offset);
}

VFI_SRC_ATTR(offset, 0644, vfi_src_offset_show, 0);

static struct attribute *vfi_src_default_attrs[] = {
    &vfi_src_attr_default.attr,
    &vfi_src_attr_location.attr,
    &vfi_src_attr_name.attr,
    &vfi_src_attr_extent.attr,
    &vfi_src_attr_offset.attr,
    0,
};

struct kobj_type vfi_src_type = {
    .release = vfi_src_release,
    .sysfs_ops = &vfi_src_sysfs_ops,
    .default_attrs = vfi_src_default_attrs,
};

int new_vfi_src(struct vfi_src **src, struct vfi_dst *parent, struct vfi_bind_param *desc)
{
	struct vfi_src *new = kzalloc(sizeof(struct vfi_src) + VFI_DESC_ALIGN - 1, GFP_KERNEL);

	*src = new;
	if (NULL == new)
		return -ENOMEM;

	/* DMA descriptors are embedded in the vfi_src struct, so
	 * align the struct to what the DMA hardware requires
	 *
	 * TJA: made 64-bit safe [use long, not int, to avoid truncating 64-bit pointers]
	 * TJA: further, need to embed original kzalloc'ed address so that it can be kfree'd without breaking.
	 */
	if ((unsigned long) new & (VFI_DESC_ALIGN-1)) {
		struct vfi_src *alt = (struct vfi_src *) (((unsigned long) new + VFI_DESC_ALIGN) & ~(VFI_DESC_ALIGN - 1));
		alt->free_p = new;
		new = alt;
	}
	vfi_clone_bind(&new->desc, desc);
	new->desc.dst.offset = new->desc.dst.extent;
	new->desc.dst.extent = new->desc.src.extent;
	new->kobj.ktype = &vfi_src_type;
	kobject_set_name(&new->kobj,"%s.%s#%llx:%x",new->desc.src.name, new->desc.src.location, new->desc.src.offset, new->desc.src.extent);

	new->kobj.kset = &parent->srcs->kset;
	new->desc.src.ops = parent->desc.src.ops;
	new->desc.src.rde = parent->desc.src.rde;
	new->dst = parent;
	vfi_bind_inherit(&new->desc,&parent->desc);

	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p %p\n",__FUNCTION__,new,parent->srcs);
	return 0;
}

int vfi_src_register(struct vfi_src *vfi_src)
{
	int ret = 0;
	
	VFI_KTRACE ("<*** %s IN ***>\n", __func__);
	ret = kobject_register(&vfi_src->kobj);
	VFI_KTRACE ("<*** %s OUT ***>\n", __func__);
	return ret;
}

void vfi_src_unregister(struct vfi_src *vfi_src)
{
    
	VFI_KTRACE ("<*** %s IN ***>\n", __func__);
	kobject_unregister(&vfi_src->kobj);
	VFI_KTRACE ("<*** %s OUT ***>\n", __func__);
}

int find_vfi_src(struct vfi_src **src, struct vfi_desc_param *desc, struct vfi_dst *parent)
{
	char name[128];
	sprintf(name,"#%llx:%x",desc->offset,desc->extent);
	*src = to_vfi_src(kset_find_obj(&parent->srcs->kset, name));
	return *src == NULL;
}

int vfi_src_create(struct vfi_src **src, struct vfi_dst *parent, struct vfi_bind_param *desc)
{
	int ret;

	ret  = new_vfi_src(src,parent,desc);

	VFI_DEBUG(MY_DEBUG,"%s %p\n",__FUNCTION__,*src);

	if (ret)
		return ret;

	ret = vfi_src_register(*src);

	if (ret) {
		vfi_src_put(*src);
		*src = NULL;
	}

	return ret;
}

void vfi_src_delete (struct vfi_dst *parent, struct vfi_bind_param *desc)
{
	struct vfi_src *src;
	char buf[128];
	
	VFI_DEBUG(MY_DEBUG,"%s: %s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x\n", __FUNCTION__, 
		    desc->xfer.name, desc->xfer.location,desc->xfer.offset, desc->xfer.extent, 
		    desc->dst.name, desc->dst.location,desc->dst.offset, desc->dst.extent, 
		    desc->src.name, desc->src.location,desc->src.offset, desc->src.extent);
	
	if (snprintf (buf, 128, "%s.%s#%llx:%x", desc->src.name, desc->src.location, desc->src.offset, desc->src.extent) >= 128) {
		VFI_DEBUG(MY_DEBUG, "%s: buffer not big enough\n",__FUNCTION__);
	}
	
	src = to_vfi_src (kset_find_obj (&parent->srcs->kset, buf));
	if (!src) {
		VFI_DEBUG (MY_DEBUG, "xxx %s failed to find \"%s\"\n", __func__, buf);
		return;
	}
	vfi_src_put (src);		/* Put, to counteract the find... */
	vfi_src_unregister(src);
}

