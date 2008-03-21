#define MY_DEBUG      RDDMA_DBG_RDYS | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_RDYS | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/rddma_event.h>
#include <linux/rddma_fabric.h>
#include <linux/rddma_ops.h>

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

int find_rddma_event(struct rddma_event **event, struct rddma_events *parent, int id)
{
	char buf[20];
	sprintf(buf,"%x",id);
	*event = to_rddma_event(kset_find_obj(&parent->kset,buf));
	return *event == NULL;
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

int new_rddma_event(struct rddma_event **event, struct rddma_events *parent, struct rddma_desc_param *desc, struct rddma_bind *bind, void (*f)(struct rddma_bind *),int id)
{
	struct rddma_event *new = kzalloc(sizeof(struct rddma_event), GFP_KERNEL);
    
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	*event = new;

	if (NULL == new)
		return -ENOMEM;

	rddma_clone_desc(&new->desc,desc);
	kobject_set_name(&new->kobj,"%p:%x", desc,id);
	new->kobj.ktype = &rddma_event_type;
	new->kobj.kset = &parent->kset;
	new->start_event = f;
	new->bind = bind;
	new->event_id = id;

	RDDMA_DEBUG(MY_DEBUG,"%s returns %p\n",__FUNCTION__,new);
	return 0;
}

int rddma_event_register(struct rddma_event *rddma_event)
{
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	return kobject_register(&rddma_event->kobj);
}

void rddma_event_unregister(struct rddma_event *rddma_event)
{
	if (rddma_event)
		kobject_unregister(&rddma_event->kobj);
}

int rddma_event_create(struct rddma_event **new, struct rddma_events *parent, struct rddma_desc_param *desc,
		       struct rddma_bind *bind, void (*f)(struct rddma_bind *),int id)
{
	int ret;

	RDDMA_DEBUG(MY_DEBUG,"%s parent(%p) desc(%p) bind(%p) f(%p) id(%08x)\n",__FUNCTION__,parent,desc,bind,f,id);

	ret = new_rddma_event(new,parent, desc, bind, f, id);

	if (ret) 
		return ret;

	if (rddma_event_register(*new)) {
			rddma_event_put(*new);
			*new = NULL;
			return -EINVAL;
	}

	return 0;
}

void rddma_event_delete (struct rddma_event *rddma_event)
{
	RDDMA_KTRACE ("<*** %s (%p) Id: %08x IN ***>\n", __func__, rddma_event, (rddma_event) ? rddma_event->event_id : 0xffffffff);
	rddma_event_unregister (rddma_event);
	RDDMA_KTRACE ("<*** %s OUT ***>\n", __func__);
}

void rddma_event_send(struct rddma_event *event)
{
	rddma_doorbell_send(event->desc.address, event->event_id);
}
