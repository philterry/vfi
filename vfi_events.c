/* 
 * 
 * Copyright 2008 Vmetro. 
 * Phil Terry <pterry@vmetro.com> 
 * 
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or  (at your
 * option) any later version.
 */

#define MY_DEBUG      VFI_DBG_RDYS | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_RDYS | VFI_DBG_LIFE    | VFI_DBG_DEBUG
#define MY_ERROR      VFI_DBG_RDYS | VFI_DBG_ERROR   | VFI_DBG_ERR

#include <linux/vfi_events.h>
#include <linux/vfi_subsys.h>
#include <linux/vfi_readies.h>
#include <linux/vfi_event.h>
#include <linux/vfi_drv.h>

#include <linux/slab.h>
#include <linux/module.h>

static void vfi_events_release(struct kobject *kobj)
{
	struct vfi_events *p = to_vfi_events(kobj);
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__func__,p);
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
	int left = PAGE_SIZE;
	int size = 0;
	ATTR_PRINTF("refcount %d\n",atomic_read(&vfi_events->kset.kobj.kref.refcount));
	return size;
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

static int vfi_events_uevent(struct kset *kset, struct kobject *kobj, struct kobj_uevent_env *env)
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
	int ret;
	struct vfi_events *new = kzalloc(sizeof(struct vfi_events), GFP_KERNEL);
    
	VFI_DEBUG(MY_DEBUG,"%s readies(%p) name(%s)\n",__FUNCTION__,parent,name);
    
	*events = new;

	if (NULL == new)
		return VFI_RESULT(-ENOMEM);

	kobject_set_name(&new->kset.kobj,name);
	new->kset.kobj.ktype = &vfi_events_type;
	new->kset.uevent_ops = &vfi_events_uevent_ops;
	new->kset.kobj.kset = &parent->kset;
	init_MUTEX(&new->start_lock);
	init_completion(&new->dma_sync);

	ret = kset_register(&new->kset);
	if (ret) {
		vfi_events_put(new);
		*events = NULL;
	}

	return VFI_RESULT(ret);
}

int vfi_events_create(struct vfi_events **events, struct vfi_readies *parent, char *name)
{
	int ret;
	VFI_DEBUG(MY_DEBUG,"%s readies(%p) name(%s)\n",__FUNCTION__,parent,name);

	ret = new_vfi_events(events, parent, name);

	VFI_DEBUG(MY_DEBUG,"%s returns(%p)\n",__FUNCTION__,*events);
	return VFI_RESULT(ret);
}

void vfi_events_delete(struct vfi_events *vfi_events)
{
	VFI_DEBUG(MY_DEBUG,"%s events(%p)\n",__FUNCTION__,vfi_events);
	if (vfi_events)
		kset_unregister(&vfi_events->kset);
}

void vfi_events_start(struct vfi_events *events)
{
	struct vfi_event *event;
	struct list_head *entry;

	int wait = 0;

	VFI_DEBUG(MY_DEBUG,"%s events(%p)\n",__FUNCTION__,events);

	if (events == NULL) {
		printk("Error, event is null\n");
		return;
	}

	if (down_trylock(&events->start_lock)) {
		printk("Error, event %s already started\n", events->kset.kobj.name);
		return;
	}

	spin_lock(&events->kset.list_lock);
	if (!list_empty(&events->kset.list)) {
		list_for_each(entry,&events->kset.list) {
			event = to_vfi_event(to_kobj(entry));
			if (event->start_event) {
				wait++;
				event->start_event(event->bind);
			}
		}
	}
	spin_unlock(&events->kset.list_lock);

	while (wait--)
		wait_for_completion(&events->dma_sync);

	up(&events->start_lock);
}
