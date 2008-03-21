#define MY_DEBUG      VFI_DBG_RDYS | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_RDYS | VFI_DBG_LIFE    | VFI_DBG_DEBUG

#include <linux/vfi_events.h>
#include <linux/vfi_subsys.h>
#include <linux/vfi_readies.h>
#include <linux/vfi_event.h>

#include <linux/slab.h>
#include <linux/module.h>

static void vfi_events_release(struct kobject *kobj)
{
    struct vfi_events *p = to_vfi_events(kobj);
    kfree(p);
}

struct vfi_events_attribute {
    struct attribute attr;
    ssize_t (*show)(struct vfi_events*, char *buffer);
    ssize_t (*store)(struct vfi_events*, const char *buffer, size_t size);
};

#define VFI_EVENTS_ATTR(_name,_mode,_show,_store) struct vfi_events_attribute vfi_events_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t vfi_events_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct vfi_events_attribute *pattr = container_of(attr, struct vfi_events_attribute, attr);
    struct vfi_events *p = to_vfi_events(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t vfi_events_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct vfi_events_attribute *pattr = container_of(attr, struct vfi_events_attribute, attr);
    struct vfi_events *p = to_vfi_events(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops vfi_events_sysfs_ops = {
    .show = vfi_events_show,
    .store = vfi_events_store,
};


static ssize_t vfi_events_default_show(struct vfi_events *vfi_events, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "vfi_events_default");
}

static ssize_t vfi_events_default_store(struct vfi_events *vfi_events, const char *buffer, size_t size)
{
    return size;
}

VFI_EVENTS_ATTR(default, 0644, vfi_events_default_show, vfi_events_default_store);

static struct attribute *vfi_events_default_attrs[] = {
    &vfi_events_attr_default.attr,
    0,
};

struct kobj_type vfi_events_type = {
    .release = vfi_events_release,
    .sysfs_ops = &vfi_events_sysfs_ops,
    .default_attrs = vfi_events_default_attrs,
};

int find_vfi_events(struct vfi_events **events, struct vfi_readies *p, char *name)
{
	VFI_DEBUG(MY_DEBUG,"%s readies(%p) name(%s)\n",__FUNCTION__,p,name);
	*events = to_vfi_events(kset_find_obj(&p->kset,name));
	return *events == NULL;
}

static int vfi_events_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *vfi_events_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int vfi_events_uevent(struct kset *kset, struct kobject *kobj, char **envp, int num_envp, char *buffer, int buf_size)
{
	return 0; /* Do not generate event */
}


static struct kset_uevent_ops vfi_events_uevent_ops = {
	.filter = vfi_events_uevent_filter,
	.name = vfi_events_uevent_name,
	.uevent = vfi_events_uevent,
};

int new_vfi_events(struct vfi_events **events, struct vfi_readies *parent, char *name)
{
    struct vfi_events *new = kzalloc(sizeof(struct vfi_events), GFP_KERNEL);
    
    VFI_DEBUG(MY_DEBUG,"%s readies(%p) name(%s)\n",__FUNCTION__,parent,name);
    
    *events = new;

    if (NULL == new)
	return -ENOMEM;

    kobject_set_name(&new->kset.kobj,name);
    new->kset.kobj.ktype = &vfi_events_type;
    new->kset.uevent_ops = &vfi_events_uevent_ops;
    new->kset.kobj.kset = &parent->kset;
    init_MUTEX(&new->start_lock);
    init_completion(&new->dma_sync);

    return 0;
}

int vfi_events_register(struct vfi_events *vfi_events)
{
	VFI_DEBUG(MY_DEBUG,"%s events(%p)\n",__FUNCTION__,vfi_events);
	return kset_register(&vfi_events->kset);
}

void vfi_events_unregister(struct vfi_events *vfi_events)
{
	VFI_DEBUG(MY_DEBUG,"%s events(%p)\n",__FUNCTION__,vfi_events);
	if (vfi_events)
		kset_unregister(&vfi_events->kset);
}

int vfi_events_create(struct vfi_events **events, struct vfi_readies *parent, char *name)
{
	struct vfi_events *new; 
	int ret;
	VFI_DEBUG(MY_DEBUG,"%s readies(%p) name(%s)\n",__FUNCTION__,parent,name);

	ret = new_vfi_events(&new, parent, name);

	*events = new;

	if ( ret ) 
		return ret;

	if (vfi_events_register(new)) {
		vfi_events_put(new);
		*events = NULL;
		return -EINVAL;
	}

	VFI_DEBUG(MY_DEBUG,"%s returns(%p)\n",__FUNCTION__,new);
	return 0;
}

void vfi_events_delete(struct vfi_events *vfi_events)
{
	VFI_DEBUG(MY_DEBUG,"%s events(%p)\n",__FUNCTION__,vfi_events);
	vfi_events_unregister(vfi_events);
}

void vfi_events_start(struct vfi_events *events)
{
	struct vfi_event *ep;
	struct list_head *entry;
	int wait = 0;

	VFI_DEBUG(MY_DEBUG,"%s events(%p)\n",__FUNCTION__,events);

	if (events == NULL) 
		return;

	if (!down_trylock(&events->start_lock)) {
		spin_lock(&events->kset.list_lock);
		if (!list_empty(&events->kset.list)) {
			list_for_each(entry,&events->kset.list) {
				ep = to_vfi_event(to_kobj(entry));
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
