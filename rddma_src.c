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

#define MY_DEBUG      RDDMA_DBG_SRC | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_SRC | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/rddma_src.h>
#include <linux/rddma_parse.h>
#include <linux/rddma_smb.h>

#include <linux/slab.h>
#include <linux/module.h>

/**
* rddma_src_release - release (free) an rddma_src cleanly.
*
* @kobj - pointer to an rddma_src-type kobject to be released.
*
* This function is invoked by the kernel kobject manager when an 
* rddma_src object has finally expired. Its job is to release any
* memory resources bound to the kobject.
*
**/
static void rddma_src_release (struct kobject *kobj)
{
	struct rddma_src *p = to_rddma_src(kobj);
	RDDMA_DEBUG(MY_LIFE_DEBUG,"XXX %s %p (refc %lx)\n", __FUNCTION__, p, (unsigned long)kobj->kref.refcount.counter);
	
	/*
	* Free embedded resources - an rddma_bind_params
	*/
	rddma_clean_bind (&p->desc);
	/*
	* Free the rddma_src structure itself.
	*
	* Note that, due to alignment finagling, the active pointer
	* might be different from the originally kzalloc'ed pointer.
	* The original address - if different - is embedded within
	* the working structure body. If the embedded pointer is NULL 
	* then the working pointer is good.
	*/
	kfree ((p->free_p) ? : p);
}

struct rddma_src_attribute {
    struct attribute attr;
    ssize_t (*show)(struct rddma_src*, char *buffer);
    ssize_t (*store)(struct rddma_src*, const char *buffer, size_t size);
};

#define RDDMA_SRC_ATTR(_name,_mode,_show,_store) \
struct rddma_src_attribute rddma_src_attr_##_name = {\
     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },\
     .show = _show,\
     .store = _store \
};

static ssize_t rddma_src_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct rddma_src_attribute *pattr = container_of(attr, struct rddma_src_attribute, attr);
    struct rddma_src *p = to_rddma_src(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t rddma_src_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct rddma_src_attribute *pattr = container_of(attr, struct rddma_src_attribute, attr);
    struct rddma_src *p = to_rddma_src(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops rddma_src_sysfs_ops = {
    .show = rddma_src_show,
    .store = rddma_src_store,
};


static ssize_t rddma_src_default_show(struct rddma_src *rddma_src, char *buffer)
{
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("Src %p is %s = %s\n",rddma_src,rddma_src->desc.dst.name,rddma_src->desc.src.name);
	if (rddma_src) {
		ATTR_PRINTF("dst: ops is %p rde is %p address is %p\n",rddma_src->desc.dst.ops,rddma_src->desc.dst.rde,rddma_src->desc.dst.address);
		ATTR_PRINTF("src: ops is %p rde is %p address is %p\n",rddma_src->desc.src.ops,rddma_src->desc.src.rde,rddma_src->desc.src.address);
	}
	return size;

}

RDDMA_SRC_ATTR(default, 0644, rddma_src_default_show, 0);

static ssize_t rddma_src_location_show(struct rddma_src *rddma_src, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%s\n", rddma_src->desc.src.location);
}

RDDMA_SRC_ATTR(location, 0644, rddma_src_location_show, 0);

static ssize_t rddma_src_name_show(struct rddma_src *rddma_src, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%s\n", rddma_src->desc.src.name);
}

RDDMA_SRC_ATTR(name, 0644, rddma_src_name_show, 0);

static ssize_t rddma_src_extent_show(struct rddma_src *rddma_src, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%x\n",rddma_src->desc.src.extent);
}

RDDMA_SRC_ATTR(extent, 0644, rddma_src_extent_show, 0);

static ssize_t rddma_src_offset_show(struct rddma_src *rddma_src, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%llx\n",rddma_src->desc.src.offset);
}

RDDMA_SRC_ATTR(offset, 0644, rddma_src_offset_show, 0);

static struct attribute *rddma_src_default_attrs[] = {
    &rddma_src_attr_default.attr,
    &rddma_src_attr_location.attr,
    &rddma_src_attr_name.attr,
    &rddma_src_attr_extent.attr,
    &rddma_src_attr_offset.attr,
    0,
};

struct kobj_type rddma_src_type = {
    .release = rddma_src_release,
    .sysfs_ops = &rddma_src_sysfs_ops,
    .default_attrs = rddma_src_default_attrs,
};

struct rddma_src *new_rddma_src(struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	struct rddma_src *new = kzalloc(sizeof(struct rddma_src) + RDDMA_DESC_ALIGN - 1, GFP_KERNEL);

	if (NULL == new)
		goto out;

	/* DMA descriptors are embedded in the rddma_src struct, so
	 * align the struct to what the DMA hardware requires
	 *
	 * TJA: made 64-bit safe [use long, not int, to avoid truncating 64-bit pointers]
	 * TJA: further, need to embed original kzalloc'ed address so that it can be kfree'd without breaking.
	 */
	if ((unsigned long) new & (RDDMA_DESC_ALIGN-1)) {
		struct rddma_src *alt = (struct rddma_src *) (((unsigned long) new + RDDMA_DESC_ALIGN) & ~(RDDMA_DESC_ALIGN - 1));
		alt->free_p = new;
		new = alt;
	}
	rddma_clone_bind(&new->desc, desc);
	new->kobj.ktype = &rddma_src_type;
	kobject_set_name(&new->kobj,"%s.%s#%llx:%x",new->desc.src.name, new->desc.src.location, new->desc.src.offset, new->desc.src.extent);

	new->kobj.kset = &parent->srcs->kset;
	new->desc.src.ops = parent->desc.src.ops;
	new->desc.src.rde = parent->desc.src.rde;
	new->dst = parent;
	rddma_bind_inherit(&new->desc,&parent->desc);
out:
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p %p\n",__FUNCTION__,new,parent->srcs);
	return new;
}

int rddma_src_register(struct rddma_src *rddma_src)
{
    int ret = 0;

    if ( (ret = kobject_register(&rddma_src->kobj) ) )
	goto out;

      return ret;

out:
    return ret;
}

void rddma_src_unregister(struct rddma_src *rddma_src)
{
    
     kobject_unregister(&rddma_src->kobj);
}

struct rddma_src *find_rddma_src(struct rddma_desc_param *desc, struct rddma_dst *parent)
{
	char name[128];
	sprintf(name,"#%llx:%x",desc->offset,desc->extent);
	return to_rddma_src(kset_find_obj(&parent->srcs->kset, name));
}

struct rddma_src *rddma_src_create(struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	struct rddma_src *new = new_rddma_src(parent,desc);
	RDDMA_DEBUG(MY_DEBUG,"%s %p\n",__FUNCTION__,new);

	if (NULL == new)
		goto out;

	if (rddma_src_register(new))
		goto fail_reg;

	return new;

fail_reg:
	rddma_src_put(new);
out:
	return NULL;
}

void rddma_src_delete (struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	struct rddma_src *src;
	char buf[128];
	
	RDDMA_DEBUG(MY_DEBUG,"%s: %s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x\n", __FUNCTION__, 
		    desc->xfer.name, desc->xfer.location,desc->xfer.offset, desc->xfer.extent, 
		    desc->dst.name, desc->dst.location,desc->dst.offset, desc->dst.extent, 
		    desc->src.name, desc->src.location,desc->src.offset, desc->src.extent);
	
	if (snprintf (buf, 128, "#%llx:%x", desc->src.offset, desc->src.extent) >= 128) {
		RDDMA_DEBUG(MY_DEBUG, "%s: buffer not big enough\n",__FUNCTION__);
	}
	
	src = to_rddma_src (kset_find_obj (&parent->srcs->kset, buf));

	rddma_src_put (src);		/* Put, to counteract the find... */
	rddma_src_unregister(src);
}

