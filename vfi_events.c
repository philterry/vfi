#define MY_DEBUG      RDDMA_DBG_RDYS | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_RDYS | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/vfi_events.h>
#include <linux/vfi_subsys.h>
#include <linux/vfi_readies.h>
#include <linux/vfi_event.h>

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

int find_rddma_events(struct rddma_events **events, struct rddma_readies *p, char *name)
{
	RDDMA_DEBUG(MY_DEBUG,"%s readies(%p) name(%s)\n",__FUNCTION__,p,name);
	*events = to_rddma_events(kset_find_obj(&p->kset,name));
	return *events == NULL;
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

int new_rddma_events(struct rddma_events **events, struct rddma_readies *parent, char *name)
{
    struct rddma_events *new = kzalloc(sizeof(struct rddma_events), GFP_KERNEL);
    
    RDDMA_DEBUG(MY_DEBUG,"%s readies(%p) name(%s)\n",__FUNCTION__,parent,name);
    
    *events = new;

    if (NULL == new)
	return -ENOMEM;

    kobject_set_name(&new->kset.kobj,name);
    new->kset.kobj.ktype = &rddma_events_type;
    new->kset.uevent_ops = &rddma_events_uevent_ops;
    new->kset.kobj.kset = &parent->kset;
    init_MUTEX(&new->start_lock);
    init_completion(&new->dma_sync);

    return 0;
}

int rddma_events_register(struct rddma_events *rddma_events)
{
	RDDMA_DEBUG(MY_DEBUG,"%s events(%p)\n",__FUNCTION__,rddma_events);
	return kset_register(&rddma_events->kset);
}

void rddma_events_unregister(struct rddma_events *rddma_events)
{
	RDDMA_DEBUG(MY_DEBUG,"%s events(%p)\n",__FUNCTION__,rddma_events);
	if (rddma_events)
		kset_unregister(&rddma_events->kset);
}

int rddma_events_create(struct rddma_events **events, struct rddma_readies *parent, char *name)
{
	struct rddma_events *new; 
	int ret;
	RDDMA_DEBUG(MY_DEBUG,"%s readies(%p) name(%s)\n",__FUNCTION__,parent,name);

	ret = new_rddma_events(&new, parent, name);

	*events = new;

	if ( ret ) 
		return ret;

	if (rddma_events_register(new)) {
		rddma_events_put(new);
		*events = NULL;
		return -EINVAL;
	}

	RDDMA_DEBUG(MY_DEBUG,"%s returns(%p)\n",__FUNCTION__,new);
	return 0;
}

void rddma_events_delete(struct rddma_events *rddma_events)
{
	RDDMA_DEBUG(MY_DEBUG,"%s events(%p)\n",__FUNCTION__,rddma_events);
	rddma_events_unregister(rddma_events);
}

void rddma_events_start(struct rddma_events *events)
{
	struct rddma_event *ep;
	struct list_head *entry;
	int wait = 0;

	RDDMA_DEBUG(MY_DEBUG,"%s events(%p)\n",__FUNCTION__,events);

	if (events == NULL) 
		return;

	if (!down_trylock(&events->start_lock)) {
		spin_lock(&events->kset.list_lock);
		if (!list_empty(&events->kset.list)) {
			list_for_each(entry,&events->kset.list) {
				ep = to_rddma_event(to_kobj(entry));
				if (ep->start_event) {
					wait = 1;
					events->count++;
					ep->start_event(ep->bind);
				}
			}
		}
		spin_unlock(&events->kset.list_lock);
		if (wait)
			wait_for_completion(&events->dma_sync);
		up(&events->start_lock);
	}
	else {
		printk("Error, event already started\n");
	}

}
