#define MY_DEBUG      RDDMA_DBG_RDYS | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_RDYS | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/rddma_events.h>
#include <linux/rddma_subsys.h>
#include <linux/rddma_readies.h>
#include <linux/rddma_event.h>

#include <linux/slab.h>
#include <linux/module.h>

static void rddma_events_release(struct kobject *kobj)
{
    struct rddma_events *p = to_rddma_events(kobj);
    kfree(p);
}

struct rddma_events_attribute {
    struct attribute attr;
    ssize_t (*show)(struct rddma_events*, char *buffer);
    ssize_t (*store)(struct rddma_events*, const char *buffer, size_t size);
};

#define RDDMA_EVENTS_ATTR(_name,_mode,_show,_store) struct rddma_events_attribute rddma_events_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t rddma_events_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct rddma_events_attribute *pattr = container_of(attr, struct rddma_events_attribute, attr);
    struct rddma_events *p = to_rddma_events(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t rddma_events_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct rddma_events_attribute *pattr = container_of(attr, struct rddma_events_attribute, attr);
    struct rddma_events *p = to_rddma_events(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops rddma_events_sysfs_ops = {
    .show = rddma_events_show,
    .store = rddma_events_store,
};


static ssize_t rddma_events_default_show(struct rddma_events *rddma_events, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "rddma_events_default");
}

static ssize_t rddma_events_default_store(struct rddma_events *rddma_events, const char *buffer, size_t size)
{
    return size;
}

RDDMA_EVENTS_ATTR(default, 0644, rddma_events_default_show, rddma_events_default_store);

static struct attribute *rddma_events_default_attrs[] = {
    &rddma_events_attr_default.attr,
    0,
};

struct kobj_type rddma_events_type = {
    .release = rddma_events_release,
    .sysfs_ops = &rddma_events_sysfs_ops,
    .default_attrs = rddma_events_default_attrs,
};

struct rddma_events *find_rddma_events(struct rddma_readies *p, char *name)
{
    return to_rddma_events(kset_find_obj(&p->kset,name));
}

static int rddma_events_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *rddma_events_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int rddma_events_uevent(struct kset *kset, struct kobject *kobj, char **envp, int num_envp, char *buffer, int buf_size)
{
	return 0; /* Do not generate event */
}


static struct kset_uevent_ops rddma_events_uevent_ops = {
	.filter = rddma_events_uevent_filter,
	.name = rddma_events_uevent_name,
	.uevent = rddma_events_uevent,
};

struct rddma_events *new_rddma_events(struct rddma_readies *parent, char *name)
{
    struct rddma_events *new = kzalloc(sizeof(struct rddma_events), GFP_KERNEL);
    
    if (NULL == new)
	return new;

    kobject_set_name(&new->kset.kobj,name);
    new->kset.kobj.ktype = &rddma_events_type;
    new->kset.uevent_ops = &rddma_events_uevent_ops;
    new->kset.kobj.kset = &parent->kset;

    return new;
}

int rddma_events_register(struct rddma_events *rddma_events)
{
	return kset_register(&rddma_events->kset);
}

void rddma_events_unregister(struct rddma_events *rddma_events)
{
	if (rddma_events)
		kset_unregister(&rddma_events->kset);
}

struct rddma_events *rddma_events_create(struct rddma_readies *parent, char *name)
{
	struct rddma_events *new; 

	if ( (new = new_rddma_events(parent, name)) ) {
		if (rddma_events_register(new)) {
			rddma_events_put(new);
			return NULL;
		}
	}
	return new;
}

void rddma_events_delete(struct rddma_events *rddma_events)
{
	rddma_events_unregister(rddma_events);
}

void rddma_events_start(struct rddma_events *events)
{
	struct rddma_event *ep;
	struct list_head *entry;

	if (events == NULL) 
		return;

	spin_lock(&events->kset.list_lock);
	if (!list_empty(&events->kset.list)) {
		list_for_each(entry,&events->kset.list) {
			ep = to_rddma_event(to_kobj(entry));
			ep->start_event(ep->bind);
			events->count++;
		}
	}
	spin_unlock(&events->kset.list_lock);
}
