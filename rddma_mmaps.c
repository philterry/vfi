#define MY_DEBUG      RDDMA_DBG_MMAP | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_MMAP | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/rddma_mmaps.h>

#include <linux/slab.h>
#include <linux/module.h>

static void rddma_mmaps_release(struct kobject *kobj)
{
    struct rddma_mmaps *p = to_rddma_mmaps(kobj);
    kfree(p);
}

struct rddma_mmaps_attribute {
    struct attribute attr;
    ssize_t (*show)(struct rddma_mmaps*, char *buffer);
    ssize_t (*store)(struct rddma_mmaps*, const char *buffer, size_t size);
};

#define RDDMA_MMAPS_ATTR(_name,_mode,_show,_store) struct rddma_mmaps_attribute rddma_mmaps_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t rddma_mmaps_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct rddma_mmaps_attribute *pattr = container_of(attr, struct rddma_mmaps_attribute, attr);
    struct rddma_mmaps *p = to_rddma_mmaps(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t rddma_mmaps_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct rddma_mmaps_attribute *pattr = container_of(attr, struct rddma_mmaps_attribute, attr);
    struct rddma_mmaps *p = to_rddma_mmaps(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops rddma_mmaps_sysfs_ops = {
    .show = rddma_mmaps_show,
    .store = rddma_mmaps_store,
};



struct kobj_type rddma_mmaps_type = {
    .release = rddma_mmaps_release,
    .sysfs_ops = &rddma_mmaps_sysfs_ops,
};

struct rddma_mmaps *find_rddma_mmaps(char *name)
{
	return NULL;
}

static int rddma_mmaps_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *rddma_mmaps_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int rddma_mmaps_uevent(struct kset *kset, struct kobject *kobj, char **envp, int num_envp, char *buffer, int buf_size)
{
	return 0; /* Do not generate event */
}


static struct kset_uevent_ops rddma_mmaps_uevent_ops = {
	.filter = rddma_mmaps_uevent_filter,
	.name = rddma_mmaps_uevent_name,
	.uevent = rddma_mmaps_uevent,
};

struct rddma_mmaps *new_rddma_mmaps(struct rddma_smb *parent, char *name)
{
    struct rddma_mmaps *new = kzalloc(sizeof(struct rddma_mmaps), GFP_KERNEL);
    
    if (NULL == new)
	return new;

    kobject_set_name(&new->kset.kobj,"%s",name);
    new->kset.kobj.ktype = &rddma_mmaps_type;
    new->kset.uevent_ops = &rddma_mmaps_uevent_ops;
    new->kset.kobj.parent = &parent->kobj;
    RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p %s %p\n",__FUNCTION__,new, kobject_name(&new->kset.kobj),parent);
    return new;
}

int rddma_mmaps_register(struct rddma_mmaps *rddma_mmaps)
{
    int ret = 0;
    RDDMA_DEBUG(MY_DEBUG,"%s %p %p\n",__FUNCTION__,rddma_mmaps,rddma_mmaps->kset.kobj.parent);
    if ( (ret = kset_register(&rddma_mmaps->kset) ) )
	goto out;

      return ret;

out:
    return ret;
}

void rddma_mmaps_unregister(struct rddma_mmaps *rddma_mmaps)
{
    
	if (rddma_mmaps)
		kset_unregister(&rddma_mmaps->kset);
}

