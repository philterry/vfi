#define MY_DEBUG      RDDMA_DBG_RDYS | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_RDYS | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/rddma_event.h>
#include <linux/rddma_fabric.h>

#include <linux/slab.h>
#include <linux/module.h>

static void rddma_event_release(struct kobject *kobj)
{
    struct rddma_event *p = to_rddma_event(kobj);
    kfree(p);
}

struct rddma_event_attribute {
    struct attribute attr;
    ssize_t (*show)(struct rddma_event*, char *buffer);
    ssize_t (*store)(struct rddma_event*, const char *buffer, size_t size);
};

#define RDDMA_EVENT_ATTR(_name,_mode,_show,_store) struct rddma_event_attribute rddma_event_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t rddma_event_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct rddma_event_attribute *pattr = container_of(attr, struct rddma_event_attribute, attr);
    struct rddma_event *p = to_rddma_event(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t rddma_event_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct rddma_event_attribute *pattr = container_of(attr, struct rddma_event_attribute, attr);
    struct rddma_event *p = to_rddma_event(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops rddma_event_sysfs_ops = {
    .show = rddma_event_show,
    .store = rddma_event_store,
};


static ssize_t rddma_event_default_show(struct rddma_event *rddma_event, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "rddma_event_default");
}

static ssize_t rddma_event_default_store(struct rddma_event *rddma_event, const char *buffer, size_t size)
{
    return size;
}

RDDMA_EVENT_ATTR(default, 0644, rddma_event_default_show, rddma_event_default_store);

static ssize_t rddma_event_name_show(struct rddma_event *rddma_event, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "rddma_event_name");
}

static ssize_t rddma_event_name_store(struct rddma_event *rddma_event, const char *buffer, size_t size)
{
    return size;
}

RDDMA_EVENT_ATTR(name, 0644, rddma_event_name_show, rddma_event_name_store);

static ssize_t rddma_event_location_show(struct rddma_event *rddma_event, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "rddma_event_location");
}

static ssize_t rddma_event_location_store(struct rddma_event *rddma_event, const char *buffer, size_t size)
{
    return size;
}

RDDMA_EVENT_ATTR(location, 0644, rddma_event_location_show, rddma_event_location_store);

static ssize_t rddma_event_offset_show(struct rddma_event *rddma_event, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "rddma_event_offset");
}

static ssize_t rddma_event_offset_store(struct rddma_event *rddma_event, const char *buffer, size_t size)
{
    return size;
}

RDDMA_EVENT_ATTR(offset, 0644, rddma_event_offset_show, rddma_event_offset_store);

static struct attribute *rddma_event_default_attrs[] = {
    &rddma_event_attr_default.attr,
    &rddma_event_attr_name.attr,
    &rddma_event_attr_location.attr,
    &rddma_event_attr_offset.attr,
    0,
};

struct kobj_type rddma_event_type = {
    .release = rddma_event_release,
    .sysfs_ops = &rddma_event_sysfs_ops,
    .default_attrs = rddma_event_default_attrs,
};

struct rddma_event *find_rddma_event(struct rddma_events *parent, int id)
{
	char buf[20];
	sprintf(buf,"%x",id);
	return to_rddma_event(kset_find_obj(&parent->kset,buf));
}

static int rddma_event_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *rddma_event_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int rddma_event_uevent(struct kset *kset, struct kobject *kobj, char **envp, int num_envp, char *buffer, int buf_size)
{
	return 0; /* Do not generate event */
}


static struct kset_uevent_ops rddma_event_uevent_ops = {
	.filter = rddma_event_uevent_filter,
	.name = rddma_event_uevent_name,
	.uevent = rddma_event_uevent,
};

struct rddma_event *new_rddma_event(struct rddma_events *parent, struct rddma_desc_param *desc, int id)
{
    struct rddma_event *new = kzalloc(sizeof(struct rddma_event), GFP_KERNEL);
    
    if (NULL == new)
	return new;

    rddma_clone_desc(&new->desc,desc);
    kobject_set_name(&new->kobj,"%x", id);
    new->kobj.ktype = &rddma_event_type;
    new->kobj.kset = &parent->kset;

    return new;
}

int rddma_event_register(struct rddma_event *rddma_event)
{
    int ret = 0;

    if ( (ret = kobject_register(&rddma_event->kobj) ) )
	goto out;

      return ret;

out:
    return ret;
}

void rddma_event_unregister(struct rddma_event *rddma_event)
{
    
     kobject_unregister(&rddma_event->kobj);
}

struct rddma_event *rddma_event_create(struct rddma_events *parent, struct rddma_desc_param *desc, int id)
{
	struct rddma_event *new; 

	if ( (new = new_rddma_event(parent, desc, id)) ) {
		if (rddma_event_register(new)) {
			rddma_event_put(new);
			return NULL;
		}
	}
	return new;
}

void rddma_event_delete(struct rddma_event *rddma_event)
{
	rddma_event_unregister(rddma_event);
}

void rddma_event_send(struct rddma_event *event)
{
	rddma_doorbell_send(event->desc.address, event->event);
}
