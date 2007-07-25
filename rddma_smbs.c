#include <linux/rddma_smbs.h>

#include <linux/slab.h>
#include <linux/module.h>

static void rddma_smbs_release(struct kobject *kobj)
{
    struct rddma_smbs *p = to_rddma_smbs(kobj);
    kfree(p);
}

struct rddma_smbs_attribute {
    struct attribute attr;
    ssize_t (*show)(struct rddma_smbs*, char *buffer);
    ssize_t (*store)(struct rddma_smbs*, const char *buffer, size_t size);
};

#define RDDMA_SMBS_ATTR(_name,_mode,_show,_store) struct rddma_smbs_attribute rddma_smbs_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t rddma_smbs_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct rddma_smbs_attribute *pattr = container_of(attr, struct rddma_smbs_attribute, attr);
    struct rddma_smbs *p = to_rddma_smbs(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t rddma_smbs_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct rddma_smbs_attribute *pattr = container_of(attr, struct rddma_smbs_attribute, attr);
    struct rddma_smbs *p = to_rddma_smbs(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops rddma_smbs_sysfs_ops = {
    .show = rddma_smbs_show,
    .store = rddma_smbs_store,
};


static ssize_t rddma_smbs_default_show(struct rddma_smbs *rddma_smbs, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "rddma_smbs_default");
}

static ssize_t rddma_smbs_default_store(struct rddma_smbs *rddma_smbs, const char *buffer, size_t size)
{
    return size;
}

RDDMA_SMBS_ATTR(default, 0644, rddma_smbs_default_show, rddma_smbs_default_store);

static struct attribute *rddma_smbs_default_attrs[] = {
    &rddma_smbs_attr_default.attr,
    0,
};

struct kobj_type rddma_smbs_type = {
    .release = rddma_smbs_release,
    .sysfs_ops = &rddma_smbs_sysfs_ops,
    .default_attrs = rddma_smbs_default_attrs,
};

static int rddma_smbs_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *rddma_smbs_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int rddma_smbs_uevent(struct kset *kset, struct kobject *kobj, char **envp, int num_envp, char *buffer, int buf_size)
{
	return 0; /* Do not generate event */
}


static struct kset_uevent_ops rddma_smbs_uevent_ops = {
	.filter = rddma_smbs_uevent_filter,
	.name = rddma_smbs_uevent_name,
	.uevent = rddma_smbs_uevent,
};

struct rddma_smbs *new_rddma_smbs(char *name, struct rddma_location *parent)
{
    struct rddma_smbs *new = kzalloc(sizeof(struct rddma_smbs), GFP_KERNEL);
    
    if (NULL == new)
	return new;

    kobject_set_name(&new->kset.kobj,name);
    new->kset.kobj.ktype = &rddma_smbs_type;
    new->kset.uevent_ops = &rddma_smbs_uevent_ops;
    new->kset.kobj.kset = &parent->smbs->kset;

    return new;
}

int rddma_smbs_register(struct rddma_smbs *rddma_smbs)
{
    int ret = 0;

    if ( (ret = kset_register(&rddma_smbs->kset) ) )
	goto out;

      return ret;

out:
    return ret;
}

void rddma_smbs_unregister(struct rddma_smbs *rddma_smbs)
{
    
     kset_unregister(&rddma_smbs->kset);
}

