#include <linux/rddma_xfer.h>
#include <linux/rddma_parse.h>
#include <linux/rddma_location.h>
#include <linux/rddma_xfers.h>
#include <linux/rddma_ops.h>
#include <linux/rddma_binds.h>
#include <linux/rddma_dma.h>

#include <linux/slab.h>
#include <linux/module.h>

static void rddma_xfer_release(struct kobject *kobj)
{
    struct rddma_xfer *p = to_rddma_xfer(kobj);
    kfree(p);
}

struct rddma_xfer_attribute {
    struct attribute attr;
    ssize_t (*show)(struct rddma_xfer*, char *buffer);
    ssize_t (*store)(struct rddma_xfer*, const char *buffer, size_t size);
};

#define RDDMA_XFER_ATTR(_name,_mode,_show,_store) struct rddma_xfer_attribute rddma_xfer_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t rddma_xfer_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct rddma_xfer_attribute *pattr = container_of(attr, struct rddma_xfer_attribute, attr);
    struct rddma_xfer *p = to_rddma_xfer(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t rddma_xfer_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct rddma_xfer_attribute *pattr = container_of(attr, struct rddma_xfer_attribute, attr);
    struct rddma_xfer *p = to_rddma_xfer(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops rddma_xfer_sysfs_ops = {
    .show = rddma_xfer_show,
    .store = rddma_xfer_store,
};


static ssize_t rddma_xfer_default_show(struct rddma_xfer *rddma_xfer, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "rddma_xfer_default");
}

static ssize_t rddma_xfer_default_store(struct rddma_xfer *rddma_xfer, const char *buffer, size_t size)
{
    return size;
}

RDDMA_XFER_ATTR(default, 0644, rddma_xfer_default_show, rddma_xfer_default_store);

static ssize_t rddma_xfer_location_show(struct rddma_xfer *rddma_xfer, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "rddma_xfer_location");
}

static ssize_t rddma_xfer_location_store(struct rddma_xfer *rddma_xfer, const char *buffer, size_t size)
{
    return size;
}

RDDMA_XFER_ATTR(location, 0644, rddma_xfer_location_show, rddma_xfer_location_store);

static ssize_t rddma_xfer_name_show(struct rddma_xfer *rddma_xfer, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "rddma_xfer_name");
}

static ssize_t rddma_xfer_name_store(struct rddma_xfer *rddma_xfer, const char *buffer, size_t size)
{
    return size;
}

RDDMA_XFER_ATTR(name, 0644, rddma_xfer_name_show, rddma_xfer_name_store);

static ssize_t rddma_xfer_extent_show(struct rddma_xfer *rddma_xfer, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "rddma_xfer_extent");
}

static ssize_t rddma_xfer_extent_store(struct rddma_xfer *rddma_xfer, const char *buffer, size_t size)
{
    return size;
}

RDDMA_XFER_ATTR(extent, 0644, rddma_xfer_extent_show, rddma_xfer_extent_store);

static ssize_t rddma_xfer_offset_show(struct rddma_xfer *rddma_xfer, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "rddma_xfer_offset");
}

static ssize_t rddma_xfer_offset_store(struct rddma_xfer *rddma_xfer, const char *buffer, size_t size)
{
    return size;
}

RDDMA_XFER_ATTR(offset, 0644, rddma_xfer_offset_show, rddma_xfer_offset_store);

static struct attribute *rddma_xfer_default_attrs[] = {
    &rddma_xfer_attr_default.attr,
    &rddma_xfer_attr_location.attr,
    &rddma_xfer_attr_name.attr,
    &rddma_xfer_attr_extent.attr,
    &rddma_xfer_attr_offset.attr,
    0,
};

struct kobj_type rddma_xfer_type = {
    .release = rddma_xfer_release,
    .sysfs_ops = &rddma_xfer_sysfs_ops,
    .default_attrs = rddma_xfer_default_attrs,
};

struct rddma_xfer *new_rddma_xfer(struct rddma_location *parent, struct rddma_xfer_param *desc)
{
	struct rddma_xfer *new = kzalloc(sizeof(struct rddma_xfer), GFP_KERNEL);
    
	if (NULL == new)
		goto out;

	new->desc = *desc;
	new->kobj.ktype = &rddma_xfer_type;
	kobject_set_name(&new->kobj,"%s", new->desc.xfer.name);

	new->kobj.kset = &parent->xfers->kset;
	new->desc.xfer.ops = parent->desc.ops;
out:
	return new;
}

int rddma_xfer_register(struct rddma_xfer *rddma_xfer)
{
    int ret = 0;

    if ( (ret = kobject_register(&rddma_xfer->kobj) ) ) {
	    rddma_xfer_put(rddma_xfer);
	    goto out;
    }

    ret = -ENOMEM;

    if ( NULL == (rddma_xfer->binds = new_rddma_binds("binds",rddma_xfer)))
	    goto fail_binds;

    if ( (ret = rddma_binds_register(rddma_xfer->binds)) )
	    goto fail_binds_reg;

    return ret;

fail_binds_reg:
    rddma_binds_put(rddma_xfer->binds);
fail_binds:
    kobject_unregister(&rddma_xfer->kobj);
out:
    return ret;
}

void rddma_xfer_unregister(struct rddma_xfer *rddma_xfer)
{
	rddma_binds_unregister(rddma_xfer->binds);
	kobject_unregister(&rddma_xfer->kobj);
}

struct rddma_xfer *find_rddma_xfer(struct rddma_xfer_param *desc)
{
	struct rddma_xfer *xfer = NULL;
	struct rddma_location *loc;

	loc = find_rddma_location(&desc->xfer);

	if (loc) 
		xfer = loc->desc.ops->xfer_find(loc,desc);

	rddma_location_put(loc);

	return xfer;
}

struct rddma_xfer *rddma_xfer_create(struct rddma_location *loc, struct rddma_xfer_param *desc)
{
	struct rddma_xfer *new;
	if ( (new = find_rddma_xfer(desc)) )
		return new;

	new = new_rddma_xfer(loc,desc);
	if (NULL == new)
		goto out;

	if (rddma_xfer_register(new))
		goto fail_reg;

	return new;

fail_reg:
	rddma_xfer_put(new);
out:	
	return NULL;
}

void rddma_xfer_delete(struct rddma_xfer *xfer)
{
	if (xfer) {
		rddma_xfer_unregister(xfer);
		rddma_xfer_put(xfer);
	}
}

void rddma_xfer_load_binds(struct rddma_xfer *xfer, struct rddma_bind *bind)
{
	if (xfer->head_bind) {
		xfer->desc.xfer.dma_ops->link_bind(xfer->head_bind, bind);
	}
	else
		xfer->head_bind = bind;
}
