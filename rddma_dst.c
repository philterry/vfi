#include <linux/rddma_dst.h>
#include <linux/rddma_parse.h>
#include <linux/rddma_smb.h>
#include <linux/rddma_ops.h>
#include <linux/rddma_src.h>

#include <linux/slab.h>
#include <linux/module.h>

static void rddma_dst_release(struct kobject *kobj)
{
    struct rddma_dst *p = to_rddma_dst(kobj);
    kfree(p);
}

struct rddma_dst_attribute {
    struct attribute attr;
    ssize_t (*show)(struct rddma_dst*, char *buffer);
    ssize_t (*store)(struct rddma_dst*, const char *buffer, size_t size);
};

#define RDDMA_DST_ATTR(_name,_mode,_show,_store)\
 struct rddma_dst_attribute rddma_dst_attr_##_name = {\
     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },\
     .show = _show,\
     .store = _store \
};

static ssize_t rddma_dst_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct rddma_dst_attribute *pattr = container_of(attr, struct rddma_dst_attribute, attr);
    struct rddma_dst *p = to_rddma_dst(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t rddma_dst_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct rddma_dst_attribute *pattr = container_of(attr, struct rddma_dst_attribute, attr);
    struct rddma_dst *p = to_rddma_dst(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops rddma_dst_sysfs_ops = {
    .show = rddma_dst_show,
    .store = rddma_dst_store,
};


static ssize_t rddma_dst_location_show(struct rddma_dst *rddma_dst, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%s\n",rddma_dst->desc.dst.location);
}

RDDMA_DST_ATTR(location, 0644, rddma_dst_location_show, 0);

static ssize_t rddma_dst_name_show(struct rddma_dst *rddma_dst, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%s\n",rddma_dst->desc.dst.name);
}

RDDMA_DST_ATTR(name, 0644, rddma_dst_name_show, 0);

static ssize_t rddma_dst_extent_show(struct rddma_dst *rddma_dst, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%d\n",rddma_dst->desc.dst.extent);
}

RDDMA_DST_ATTR(extent, 0644, rddma_dst_extent_show,0);

static ssize_t rddma_dst_offset_show(struct rddma_dst *rddma_dst, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%llx\n",rddma_dst->desc.dst.offset);
}

RDDMA_DST_ATTR(offset, 0644, rddma_dst_offset_show, 0);

static struct attribute *rddma_dst_default_attrs[] = {
    &rddma_dst_attr_location.attr,
    &rddma_dst_attr_name.attr,
    &rddma_dst_attr_extent.attr,
    &rddma_dst_attr_offset.attr,
    0,
};

struct kobj_type rddma_dst_type = {
    .release = rddma_dst_release,
    .sysfs_ops = &rddma_dst_sysfs_ops,
    .default_attrs = rddma_dst_default_attrs,
};

struct rddma_dst *new_rddma_dst(struct rddma_bind *parent, struct rddma_xfer_param *desc)
{
	struct rddma_dst *new = kzalloc(sizeof(struct rddma_dst), GFP_KERNEL);
    
	if (NULL == new)
		goto out;

	new->desc = desc->bind;
	new->kobj.ktype = &rddma_dst_type;
	kobject_set_name(&new->kobj,"#%llx:%x", new->desc.dst.offset, new->desc.dst.extent);

	new->kobj.kset = &parent->dsts->kset;
	new->desc.dst.ops = parent->desc.dst.ops;
out:
	return new;
}

int rddma_dst_register(struct rddma_dst *rddma_dst)
{
    int ret = 0;

    if ( (ret = kobject_register(&rddma_dst->kobj) ) )
	goto out;

      return ret;

out:
    return ret;
}

void rddma_dst_unregister(struct rddma_dst *rddma_dst)
{
    
     kobject_unregister(&rddma_dst->kobj);
}

struct rddma_dst *find_rddma_dst(struct rddma_xfer_param *desc)
{
	struct rddma_bind *bind = NULL;
	struct rddma_dst *dst = NULL;

	bind = find_rddma_bind(desc);
	
	dst = bind->desc.dst.ops->dst_find(bind,desc);
	return dst;
}

struct rddma_dst *rddma_dst_create(struct rddma_bind *bind, struct rddma_xfer_param *desc)
{
	struct rddma_dst *new = new_rddma_dst(bind,desc);

	if (NULL == new)
		goto out;

	if (rddma_dst_register(new))
		goto fail_reg;

	return new;
fail_reg:
	rddma_dst_put(new);
out:
	return NULL;
}

void rddma_dst_delete(struct rddma_dst *dst)
{
	if (dst) {
		rddma_dst_unregister(dst);
		rddma_dst_put(dst);
	}
}

void rddma_dst_load_srcs(struct rddma_dst *dst)
{
	struct list_head * entry;

	struct rddma_src * src1 = NULL; 
	struct rddma_src * src2 = NULL; 
	spin_lock(&dst->srcs->kset.list_lock);
	list_for_each(entry,&dst->srcs->kset.list) {
		if (NULL == src1) {
			src1 = to_rddma_src(to_kobj(entry));
			src1->desc.src.dma_ops->load(src1);
			continue;
		}
		src2 = to_rddma_src(to_kobj(entry));
		src2->desc.src.dma_ops->load(src2);
		src1->desc.src.dma_ops->link_src(src1,src2);
	}
	spin_unlock(&dst->srcs->kset.list_lock);
	dst->head_src = src1;
}
