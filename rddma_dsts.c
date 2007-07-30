#include <linux/rddma_dsts.h>
#include <linux/rddma_bind.h>

#include <linux/slab.h>
#include <linux/module.h>

static void rddma_dsts_release(struct kobject *kobj)
{
    struct rddma_dsts *p = to_rddma_dsts(kobj);
    kfree(p);
}

struct rddma_dsts_attribute {
    struct attribute attr;
    ssize_t (*show)(struct rddma_dsts*, char *buffer);
    ssize_t (*store)(struct rddma_dsts*, const char *buffer, size_t size);
};

#define RDDMA_DSTS_ATTR(_name,_mode,_show,_store) struct rddma_dsts_attribute rddma_dsts_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t rddma_dsts_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct rddma_dsts_attribute *pattr = container_of(attr, struct rddma_dsts_attribute, attr);
    struct rddma_dsts *p = to_rddma_dsts(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t rddma_dsts_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct rddma_dsts_attribute *pattr = container_of(attr, struct rddma_dsts_attribute, attr);
    struct rddma_dsts *p = to_rddma_dsts(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops rddma_dsts_sysfs_ops = {
    .show = rddma_dsts_show,
    .store = rddma_dsts_store,
};


static ssize_t rddma_dsts_default_show(struct rddma_dsts *rddma_dsts, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "rddma_dsts_default");
}

static ssize_t rddma_dsts_default_store(struct rddma_dsts *rddma_dsts, const char *buffer, size_t size)
{
    return size;
}

RDDMA_DSTS_ATTR(default, 0644, rddma_dsts_default_show, rddma_dsts_default_store);

static struct attribute *rddma_dsts_default_attrs[] = {
    &rddma_dsts_attr_default.attr,
    0,
};

struct kobj_type rddma_dsts_type = {
    .release = rddma_dsts_release,
    .sysfs_ops = &rddma_dsts_sysfs_ops,
    .default_attrs = rddma_dsts_default_attrs,
};

static int rddma_dsts_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *rddma_dsts_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int rddma_dsts_uevent(struct kset *kset, struct kobject *kobj, char **envp, int num_envp, char *buffer, int buf_size)
{
	return -ENODEV; /* Do not generate event */
}


static struct kset_uevent_ops rddma_dsts_uevent_ops = {
	.filter = rddma_dsts_uevent_filter,
	.name = rddma_dsts_uevent_name,
 	.uevent = rddma_dsts_uevent, 
};

struct rddma_dsts *new_rddma_dsts(struct rddma_xfer_param *params, struct rddma_bind *parent, char *name, va_list args)
{
	char *buf;
	int size;
	struct rddma_dsts *new = kzalloc(sizeof(struct rddma_dsts), GFP_KERNEL);
    
	if (NULL == new)
		return new;
    
	if (NULL == (buf = kzalloc(PAGE_SIZE,GFP_KERNEL)) )
		goto fail_buf;

	if ( PAGE_SIZE-1 <= (size = vsnprintf(buf, PAGE_SIZE-1,name,args)) ) 
		goto fail_printf;
		
	kobject_set_name(&new->kset.kobj,"%s",buf);
	kfree(buf);
	new->kset.kobj.ktype = &rddma_dsts_type;
	new->kset.uevent_ops = &rddma_dsts_uevent_ops;
	new->kset.kobj.kset = &parent->dsts->kset;

	return new;
fail_printf:
	kfree(buf);
fail_buf:
	kfree(new);
	return NULL;
}

int rddma_dsts_register(struct rddma_dsts *rddma_dsts)
{
    int ret = 0;

    if ( (ret = kset_register(&rddma_dsts->kset) ) )
	goto out;

      return ret;

out:
    return ret;
}

void rddma_dsts_unregister(struct rddma_dsts *rddma_dsts)
{
    
     kset_unregister(&rddma_dsts->kset);
}

struct rddma_dsts *rddma_dsts_create(struct rddma_bind *parent, struct rddma_xfer_param *desc, char *name, ...)
{
	struct rddma_dsts  *dsts = NULL;
	va_list ap;
	va_start(ap,name);
	dsts = rddma_dsts_create_ap(parent,desc,name,ap);
	va_end(ap);
	return dsts;
}

struct rddma_dsts *rddma_dsts_create_ap(struct rddma_bind *parent, struct rddma_xfer_param *desc, char *name, va_list ap)
{
	struct rddma_dsts *new;

	if (NULL == (new = new_rddma_dsts(desc,parent,name,ap)) )
		goto out;

	if (rddma_dsts_register(new))
		goto fail_reg;

	return new;
fail_reg:
	rddma_dsts_put(new);
out:
	return NULL;
}

void rddma_dsts_delete(struct rddma_dsts *dsts)
{
	rddma_dsts_unregister(dsts);
	rddma_dsts_put(dsts);
}
