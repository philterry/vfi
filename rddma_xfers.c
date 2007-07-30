#include <linux/rddma_xfers.h>

#include <linux/slab.h>
#include <linux/module.h>

static void rddma_xfers_release(struct kobject *kobj)
{
    struct rddma_xfers *p = to_rddma_xfers(kobj);
    kfree(p);
}

struct rddma_xfers_attribute {
    struct attribute attr;
    ssize_t (*show)(struct rddma_xfers*, char *buffer);
    ssize_t (*store)(struct rddma_xfers*, const char *buffer, size_t size);
};

#define RDDMA_XFERS_ATTR(_name,_mode,_show,_store) struct rddma_xfers_attribute rddma_xfers_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t rddma_xfers_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct rddma_xfers_attribute *pattr = container_of(attr, struct rddma_xfers_attribute, attr);
    struct rddma_xfers *p = to_rddma_xfers(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t rddma_xfers_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct rddma_xfers_attribute *pattr = container_of(attr, struct rddma_xfers_attribute, attr);
    struct rddma_xfers *p = to_rddma_xfers(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops rddma_xfers_sysfs_ops = {
    .show = rddma_xfers_show,
    .store = rddma_xfers_store,
};


static ssize_t rddma_xfers_default_show(struct rddma_xfers *rddma_xfers, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "rddma_xfers_default");
}

static ssize_t rddma_xfers_default_store(struct rddma_xfers *rddma_xfers, const char *buffer, size_t size)
{
    return size;
}

RDDMA_XFERS_ATTR(default, 0644, rddma_xfers_default_show, rddma_xfers_default_store);

static struct attribute *rddma_xfers_default_attrs[] = {
    &rddma_xfers_attr_default.attr,
    0,
};

struct kobj_type rddma_xfers_type = {
    .release = rddma_xfers_release,
    .sysfs_ops = &rddma_xfers_sysfs_ops,
    .default_attrs = rddma_xfers_default_attrs,
};

static int rddma_xfers_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *rddma_xfers_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int rddma_xfers_uevent(struct kset *kset, struct kobject *kobj, char **envp, int num_envp, char *buffer, int buf_size)
{
	return -ENODEV; /* Do not generate event */
}


static struct kset_uevent_ops rddma_xfers_uevent_ops = {
	.filter = rddma_xfers_uevent_filter,
	.name = rddma_xfers_uevent_name,
 	.uevent = rddma_xfers_uevent, 
};

struct rddma_xfers *new_rddma_xfers(char *name, struct rddma_location *parent)
{
    struct rddma_xfers *new = kzalloc(sizeof(struct rddma_xfers), GFP_KERNEL);
    
    if (NULL == new)
	return new;

    kobject_set_name(&new->kset.kobj,name);
    new->kset.kobj.ktype = &rddma_xfers_type;
    new->kset.uevent_ops = &rddma_xfers_uevent_ops;
    new->kset.kobj.kset = &parent->xfers->kset;

    return new;
}

int rddma_xfers_register(struct rddma_xfers *rddma_xfers)
{
    int ret = 0;

    if ( (ret = kset_register(&rddma_xfers->kset) ) )
	goto out;

      return ret;

out:
    return ret;
}

void rddma_xfers_unregister(struct rddma_xfers *rddma_xfers)
{
    
     kset_unregister(&rddma_xfers->kset);
}

