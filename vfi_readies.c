#define MY_DEBUG      RDDMA_DBG_RDYS | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_RDYS | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/vfi_drv.h>
#include <linux/vfi_readies.h>
#include <linux/vfi_events.h>
#include <linux/slab.h>
#include <linux/module.h>

static void rddma_readies_release(struct kobject *kobj)
{
    struct rddma_readies *p = to_rddma_readies(kobj);
    kfree(p);
}

struct rddma_readies_attribute {
    struct attribute attr;
    ssize_t (*show)(struct rddma_readies*, char *buffer);
    ssize_t (*store)(struct rddma_readies*, const char *buffer, size_t size);
};

#define RDDMA_READIES_ATTR(_name,_mode,_show,_store) struct rddma_readies_attribute rddma_readies_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t rddma_readies_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct rddma_readies_attribute *pattr = container_of(attr, struct rddma_readies_attribute, attr);
    struct rddma_readies *p = to_rddma_readies(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t rddma_readies_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct rddma_readies_attribute *pattr = container_of(attr, struct rddma_readies_attribute, attr);
    struct rddma_readies *p = to_rddma_readies(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops rddma_readies_sysfs_ops = {
    .show = rddma_readies_show,
    .store = rddma_readies_store,
};


static ssize_t rddma_readies_default_show(struct rddma_readies *rddma_readies, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "rddma_readies_default");
}

static ssize_t rddma_readies_default_store(struct rddma_readies *rddma_readies, const char *buffer, size_t size)
{
    return size;
}

RDDMA_READIES_ATTR(default, 0644, rddma_readies_default_show, rddma_readies_default_store);

static struct attribute *rddma_readies_default_attrs[] = {
    &rddma_readies_attr_default.attr,
    0,
};

struct kobj_type rddma_readies_type = {
    .release = rddma_readies_release,
    .sysfs_ops = &rddma_readies_sysfs_ops,
    .default_attrs = rddma_readies_default_attrs,
};

int find_rddma_readies(struct rddma_events **events, struct rddma_subsys *parent, char *name)
{
    *events = to_rddma_events(kset_find_obj(&parent->events->kset,name));
    return *events == NULL;
}

int find_rddma_dones(struct rddma_events **events, struct rddma_subsys *parent, char *name)
{
    *events = to_rddma_events(kset_find_obj(&parent->events->kset,name));
    return *events == NULL;
}

static int rddma_readies_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *rddma_readies_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int rddma_readies_uevent(struct kset *kset, struct kobject *kobj, char **envp, int num_envp, char *buffer, int buf_size)
{
	return 0; /* Do not generate event */
}


static struct kset_uevent_ops rddma_readies_uevent_ops = {
	.filter = rddma_readies_uevent_filter,
	.name = rddma_readies_uevent_name,
	.uevent = rddma_readies_uevent,
};

int new_rddma_readies(struct rddma_readies **newreadies, struct rddma_subsys *parent, char *name)
{
    struct rddma_readies *new = kzalloc(sizeof(struct rddma_readies), GFP_KERNEL);

    *newreadies = new;

    if (NULL == new)
	return -ENOMEM;

    kobject_set_name(&new->kset.kobj,name);
    new->kset.kobj.ktype = &rddma_readies_type;
    new->kset.uevent_ops = &rddma_readies_uevent_ops;
    new->kset.kobj.parent = &parent->kset.kobj;

    return 0;
}

int rddma_readies_register(struct rddma_readies *rddma_readies)
{
	return kset_register(&rddma_readies->kset);
}

void rddma_readies_unregister(struct rddma_readies *rddma_readies)
{
	if (rddma_readies)
		kset_unregister(&rddma_readies->kset);
}

int rddma_readies_create(struct rddma_readies **new, struct rddma_subsys *parent, char *name)
{
	int ret;

	ret = new_rddma_readies(new, parent, name);

	if (ret) 
		return ret;

	ret = rddma_readies_register(*new);
	
	if (ret) {
		rddma_readies_put(*new);
		return -EINVAL;
	}

	return 0;
}

void rddma_readies_delete(struct rddma_readies *rddma_readies)
{
	rddma_readies_unregister(rddma_readies);
}
