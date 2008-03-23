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

#define MY_DEBUG      VFI_DBG_BIND | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_BIND | VFI_DBG_LIFE    | VFI_DBG_DEBUG
#define MY_ERROR      VFI_DBG_BIND | VFI_DBG_ERROR   | VFI_DBG_ERR

#include <linux/vfi_bind.h>
#include <linux/vfi_xfer.h>
#include <linux/vfi_ops.h>
#include <linux/vfi_binds.h>
#include <linux/vfi_dsts.h>
#include <linux/vfi_dst.h>
#include <linux/vfi_dma.h>

#include <linux/slab.h>
#include <linux/module.h>


/**
* vfi_bind_release - release (free) an vfi_bind cleanly.
*
* @kobj - pointer to an vfi_bind-type kobject to be released.
*
* This function is invoked by the kernel kobject manager when an 
* vfi_bind object has finally expired. Its job is to release any
* memory resources bound to the kobject.
*
**/
static void vfi_bind_release(struct kobject *kobj)
{
    struct vfi_bind *p = to_vfi_bind(kobj);
    VFI_DEBUG(MY_LIFE_DEBUG,"XXX %s %p (refc %lx)\n", __FUNCTION__, p, (unsigned long)kobj->kref.refcount.counter);
    vfi_clean_bind (&p->desc);
    kfree(p);
}

struct vfi_bind_attribute {
    struct attribute attr;
    ssize_t (*show)(struct vfi_bind*, char *buffer);
    ssize_t (*store)(struct vfi_bind*, const char *buffer, size_t size);
};

#define VFI_BIND_ATTR(_name,_mode,_show,_store) \
struct vfi_bind_attribute vfi_bind_attr_##_name = {\
     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },\
     .show = _show,\
     .store = _store\
};

static ssize_t vfi_bind_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct vfi_bind_attribute *pattr = container_of(attr, struct vfi_bind_attribute, attr);
    struct vfi_bind *p = to_vfi_bind(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t vfi_bind_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct vfi_bind_attribute *pattr = container_of(attr, struct vfi_bind_attribute, attr);
    struct vfi_bind *p = to_vfi_bind(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops vfi_bind_sysfs_ops = {
    .show = vfi_bind_show,
    .store = vfi_bind_store,
};


static ssize_t vfi_bind_default_show(struct vfi_bind *vfi_bind, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("Bind %p is %s.%s = %s.%s \n",vfi_bind,vfi_bind->desc.dst.name, vfi_bind->desc.dst.location,
		    vfi_bind->desc.src.name,vfi_bind->desc.src.location);
	if (vfi_bind) {
		ATTR_PRINTF("dst: ops is %p rde is %p address is %p\n",vfi_bind->desc.dst.ops,vfi_bind->desc.dst.rde,vfi_bind->desc.dst.address);
		ATTR_PRINTF("src: ops is %p rde is %p address is %p\n",vfi_bind->desc.src.ops,vfi_bind->desc.src.rde,vfi_bind->desc.src.address);
	}
	return size;

}

VFI_BIND_ATTR(default, 0644, vfi_bind_default_show, 0);

static ssize_t vfi_bind_offset_show(struct vfi_bind *vfi_bind, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%llx\n",vfi_bind->desc.dst.offset);
}

VFI_BIND_ATTR(offset, 0644, vfi_bind_offset_show, 0);

static ssize_t vfi_bind_extent_show(struct vfi_bind *vfi_bind, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%x\n",vfi_bind->desc.dst.extent);
}

VFI_BIND_ATTR(extent, 0644, vfi_bind_extent_show, 0);

static struct attribute *vfi_bind_default_attrs[] = {
    &vfi_bind_attr_offset.attr,
    &vfi_bind_attr_extent.attr,
    &vfi_bind_attr_default.attr,
    0,
};

struct kobj_type vfi_bind_type = {
    .release = vfi_bind_release,
    .sysfs_ops = &vfi_bind_sysfs_ops,
    .default_attrs = vfi_bind_default_attrs,
};

/**
* new_vfi_bind - create a new vfi_bind kobject and attach it to parent xfer
* 
* @xfer: pointer to parent xfer, to which new bind should be attached.
* @desc: pointer to bind parameter descriptor, identifying participants and
*        other bind attributes.
*
* This function creates a new vfi_bind kobject and attaches it to its parent
* xfer. 
*
**/
int new_vfi_bind(struct vfi_bind **bind, struct vfi_xfer *parent, struct vfi_bind_param *desc)
{
	struct vfi_bind *new = kzalloc(sizeof(struct vfi_bind), GFP_KERNEL);
	
	*bind = new;

	if (NULL == new)
		return VFI_RESULT(-ENOMEM);
	
	vfi_clone_bind(&new->desc, desc);
	
	kobject_set_name(&new->kobj,"#%llx:%x",desc->xfer.offset,desc->xfer.extent);
	new->kobj.ktype = &vfi_bind_type;
	
	new->kobj.kset = &parent->binds->kset;

	vfi_inherit(&new->desc.xfer,&parent->desc);
	vfi_inherit(&new->desc.dst,&parent->desc);
	vfi_inherit(&new->desc.src,&parent->desc);
	new->desc.dst.ops = &vfi_local_ops;
	new->desc.src.ops = &vfi_local_ops;

	new->src_done_event_id = -1;
	new->src_ready_event_id = -1;
	new->dst_done_event_id = -1;
	new->dst_ready_event_id = -1;

	INIT_LIST_HEAD(&new->dma_chain);

	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,new);
	return VFI_RESULT(0);
}

int vfi_bind_register(struct vfi_bind *vfi_bind)
{
	int rslt;
	VFI_KTRACE ("<*** %s IN ***>\n", __func__);
	rslt = kobject_register(&vfi_bind->kobj);
	VFI_KTRACE ("<*** %s OUT ***>\n", __func__);
	return VFI_RESULT(rslt);
}

void vfi_bind_unregister(struct vfi_bind *vfi_bind)
{
    
	VFI_KTRACE ("<*** %s (%s) IN ***>\n", __func__, (vfi_bind) ? kobject_name (&vfi_bind->kobj) : "<NULL>");
	kobject_unregister(&vfi_bind->kobj);
	VFI_KTRACE ("<*** %s OUT ***>\n", __func__);
}

/*
* find_vfi_bind_in - find a bind object within some arbitrary xfer.
* @xfer : xfer to be searched
* @desc : xfer descriptor identifying the <xfer> component of the bind by name
* 
* The @xfer argument may be NULL, in which case the function fill try to find the xfer matching the
* bind's <xfer> component.
*
* Hmmm... trying to find a bind using only the name of its <xfer> component. How is that going to work?
*
* BEWARE:
* -------
* A successful search will cause the bind refcount to be incremented.
*
*/
int find_vfi_bind_in(struct vfi_bind **bind, struct vfi_xfer *xfer, struct vfi_desc_param *desc)
{
	int ret;
	VFI_DEBUG(MY_DEBUG,"%s desc(%p)\n",__FUNCTION__,desc);

	/*
	* If an xfer has been specified up-front, invoke its
	* bind_find op.
	*
	*/
	if (xfer && xfer->desc.ops)
		return VFI_RESULT(xfer->desc.ops->bind_find(bind,xfer,desc));

	/*
	* Otherwise try to find the xfer in the local tree, and
	* invoke its bind_find function afterwards.
	*
	* This might fail - if the xfer does not exist, then 
	* neither does the bind.
	*
	* We will not create a missing xfer automagically, 
	* but we will leave its refcount incremented if the 
	* search is successful.
	*/
	ret = find_vfi_xfer(&xfer,desc);
	if (ret)
		return VFI_RESULT(ret);

	ret = -EINVAL;

	if (xfer) {
		if (xfer->desc.ops) 
			ret = xfer->desc.ops->bind_find(bind,xfer,desc);
		vfi_xfer_put(xfer);
	}


	return VFI_RESULT(ret);
}

/**
* vfi_bind_create - create a new vfi_bind kobject and attach it to parent xfer
* 
* @xfer: pointer to parent xfer, to which new bind should be attached.
* @desc: pointer to bind parameter descriptor, identifying participants and
*        other bind attributes.
*
* This function is a front-end wrapper for bind creation that will create a new
* bind only if no other bind currently exists for the same <xfer>/<dst>=<src> triplet.
* 
* The function will return a pointer either to a pre-existing bind for the triplet, 
* or a new bind created by new_vfi_bind ().
*
**/
int vfi_bind_create(struct vfi_bind **newbind, struct vfi_xfer *xfer, struct vfi_bind_param *desc)
{
	struct vfi_bind *new;
	char buf[128];
	int ret;

	VFI_DEBUG(MY_DEBUG,"%s xfer(%p) desc(%p)\n",__FUNCTION__,xfer,desc);

	snprintf(buf,128,"#%llx:%x",desc->xfer.offset,desc->xfer.extent);

	new = to_vfi_bind(kset_find_obj(&xfer->binds->kset,buf));

	if (new) {
		VFI_DEBUG(MY_DEBUG,"%s found %p locally in %p\n",__FUNCTION__,new,xfer);
		*newbind = new;
		return VFI_RESULT(0);
	}

	ret = new_vfi_bind(newbind,xfer,desc);

	if (ret)
		return VFI_RESULT(ret);

	ret = vfi_bind_register(*newbind);
	if (ret)
		return VFI_RESULT(ret);

	return VFI_RESULT(0);
}

/**
* vfi_bind_delete - remove bind object from Xfer <binds> list
* @xfer : Xfer object that bind belongs to
* @desc : Xfer parameter descriptor
*
*
**/
void vfi_bind_delete(struct vfi_xfer *xfer, struct vfi_desc_param *desc)
{
	struct vfi_bind *bind = NULL;
	char buf[128];

	VFI_DEBUG(MY_DEBUG,"%s from Xfer %s.%s#%llx:%x\n", __func__, 
	           xfer->desc.name, xfer->desc.location, xfer->desc.offset, xfer->desc.extent);

	if ( snprintf(buf,128,"#%llx:%x", desc->offset, desc->extent) > 128 )
		goto out;

	bind = to_vfi_bind(kset_find_obj(&xfer->binds->kset, buf));

	if (bind) 
		vfi_bind_unregister (bind);
out:
	VFI_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,xfer,desc,bind);
}

/**
* vfi_bind_load_dsts - 
* @bind : parent bind object
*
*
**/
void vfi_bind_load_dsts(struct vfi_bind *bind)
{
	struct list_head * entry;
	struct vfi_dst * dst2 = NULL; 

	VFI_DEBUG(MY_DEBUG,"%s %p\n",__FUNCTION__,bind);
	if (bind->dsts) {
		spin_lock(&bind->dsts->kset.list_lock);
		if (!list_empty(&bind->dsts->kset.list)) {
			list_for_each(entry,&bind->dsts->kset.list) {
				dst2 = to_vfi_dst(to_kobj(entry));
				VFI_DEBUG(MY_DEBUG,"%s dst2 %p name=%s\n",__FUNCTION__,dst2, &dst2->kobj.name[0]);
				if (dst2->desc.dst.rde && dst2->desc.dst.rde->ops && dst2->desc.dst.rde->ops->link_dst) {
					dst2->desc.dst.rde->ops->link_dst(&bind->dsts->dma_chain,dst2);
				}
				else {
					VFI_DEBUG (MY_DEBUG, "xx %s failed: Dst \"%s\" is missing RDE ops!\n", 
							__FUNCTION__, dst2->desc.dst.name);
				}
			}
		}
		spin_unlock(&bind->dsts->kset.list_lock);
	}
	list_splice(&bind->dsts->dma_chain, &bind->dma_chain);
}
