#define MY_DEBUG      RDDMA_DBG_DONES | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_DONES | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/rddma_drv.h>
#include <linux/rddma_dones.h>
#include <linux/rddma_events.h>
#include <linux/slab.h>
#include <linux/module.h>

static void rddma_dones_release(struct kobject *kobj)
{
    struct rddma_dones *p = to_rddma_dones(kobj);
    kfree(p);
}

struct rddma_dones_attribute {
    struct attribute attr;
    ssize_t (*show)(struct rddma_dones*, char *buffer);
    ssize_t (*store)(struct rddma_dones*, const char *buffer, size_t size);
};

#define RDDMA_DONES_ATTR(_name,_mode,_show,_store) struct rddma_dones_attribute rddma_dones_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t rddma_dones_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct rddma_dones_attribute *pattr = container_of(attr, struct rddma_dones_attribute, attr);
    struct rddma_dones *p = to_rddma_dones(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t rddma_dones_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct rddma_dones_attribute *pattr = container_of(attr, struct rddma_dones_attribute, attr);
    struct rddma_dones *p = to_rddma_dones(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops rddma_dones_sysfs_ops = {
    .show = rddma_dones_show,
    .store = rddma_dones_store,
};


static ssize_t rddma_dones_default_show(struct rddma_dones *rddma_dones, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "rddma_dones_default");
}

static ssize_t rddma_dones_default_store(struct rddma_dones *rddma_dones, const char *buffer, size_t size)
{
    return size;
}

RDDMA_DONES_ATTR(default, 0644, rddma_dones_default_show, rddma_dones_default_store);

static struct attribute *rddma_dones_default_attrs[] = {
    &rddma_dones_attr_default.attr,
    0,
};

struct kobj_type rddma_dones_type = {
    .release = rddma_dones_release,
    .sysfs_ops = &rddma_dones_sysfs_ops,
    .default_attrs = rddma_dones_default_attrs,
};

struct rddma_events *find_rddma_dones(struct rddma_subsys *parent, char *name)
{
    return to_rddma_events(kset_find_obj(&parent->dones->kset,name));
}

static int rddma_dones_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *rddma_dones_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int rddma_dones_uevent(struct kset *kset, struct kobject *kobj, char **envp, int num_envp, char *buffer, int buf_size)
{
	return 0; /* Do not generate event */
}


static struct kset_uevent_ops rddma_dones_uevent_ops = {
	.filter = rddma_dones_uevent_filter,
	.name = rddma_dones_uevent_name,
	.uevent = rddma_dones_uevent,
};

struct rddma_dones *new_rddma_dones(struct rddma_subsys *parent, char *name)
{
    struct rddma_dones *new = kzalloc(sizeof(struct rddma_dones), GFP_KERNEL);
    
    if (NULL == new)
	return new;

    kobject_set_name(&new->kset.kobj,name);
    new->kset.kobj.ktype = &rddma_dones_type;
    new->kset.uevent_ops = &rddma_dones_uevent_ops;
    new->kset.kobj.parent = &parent->kset.kobj;

    return new;
}

int rddma_dones_register(struct rddma_dones *rddma_dones)
{
	return kset_register(&rddma_dones->kset);
}

void rddma_dones_unregister(struct rddma_dones *rddma_dones)
{
	if (rddma_dones)
		kset_unregister(&rddma_dones->kset);
}

struct rddma_dones *rddma_dones_create(struct rddma_subsys *parent)
{
	struct rddma_dones *new; 

	if ( (new = new_rddma_dones(parent, "dones")) ) {
		if (rddma_dones_register(new)) {
			rddma_dones_put(new);
			return NULL;
		}
		else
			parent->dones = new;
	}
	return new;
}

void rddma_dones_delete(struct rddma_dones *rddma_dones)
{
	rddma_dones_unregister(rddma_dones);
}
