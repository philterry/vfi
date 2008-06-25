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

#include <linux/vfi_event.h>
#include <linux/vfi_fabric.h>
#include <linux/vfi_ops.h>

#include <linux/slab.h>
#include <linux/module.h>

static void vfi_event_release(struct kobject *kobj)
{
    struct vfi_event *p = to_vfi_event(kobj);
    kfree(p);
}

struct vfi_event_attribute {
    struct attribute attr;
    ssize_t (*show)(struct vfi_event*, char *buffer);
    ssize_t (*store)(struct vfi_event*, const char *buffer, size_t size);
};

#define VFI_EVENT_ATTR(_name,_mode,_show,_store) struct vfi_event_attribute vfi_event_attr_##_name = {     .attr = { .name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE },     .show = _show,     .store = _store };

static ssize_t vfi_event_show(struct kobject *kobj, struct attribute *attr, char *buffer)
{
    struct vfi_event_attribute *pattr = container_of(attr, struct vfi_event_attribute, attr);
    struct vfi_event *p = to_vfi_event(kobj);

    if (pattr && pattr->show)
	return pattr->show(p,buffer);

    return 0;
}

static ssize_t vfi_event_store(struct kobject *kobj, struct attribute *attr, const char *buffer, size_t size)
{
    struct vfi_event_attribute *pattr = container_of(attr, struct vfi_event_attribute, attr);
    struct vfi_event *p = to_vfi_event(kobj);

    if (pattr && pattr->store)
	return pattr->store(p, buffer, size);

    return 0;
}

static struct sysfs_ops vfi_event_sysfs_ops = {
    .show = vfi_event_show,
    .store = vfi_event_store,
};


static ssize_t vfi_event_default_show(struct vfi_event *vfi_event, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "vfi_event_default");
}

static ssize_t vfi_event_default_store(struct vfi_event *vfi_event, const char *buffer, size_t size)
{
    return size;
}

VFI_EVENT_ATTR(default, 0644, vfi_event_default_show, vfi_event_default_store);

static ssize_t vfi_event_name_show(struct vfi_event *vfi_event, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "vfi_event_name");
}

static ssize_t vfi_event_name_store(struct vfi_event *vfi_event, const char *buffer, size_t size)
{
    return size;
}

VFI_EVENT_ATTR(name, 0644, vfi_event_name_show, vfi_event_name_store);

static ssize_t vfi_event_location_show(struct vfi_event *vfi_event, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "vfi_event_location");
}

static ssize_t vfi_event_location_store(struct vfi_event *vfi_event, const char *buffer, size_t size)
{
    return size;
}

VFI_EVENT_ATTR(location, 0644, vfi_event_location_show, vfi_event_location_store);

static ssize_t vfi_event_offset_show(struct vfi_event *vfi_event, char *buffer)
{
    return snprintf(buffer, PAGE_SIZE, "vfi_event_offset");
}

static ssize_t vfi_event_offset_store(struct vfi_event *vfi_event, const char *buffer, size_t size)
{
    return size;
}

VFI_EVENT_ATTR(offset, 0644, vfi_event_offset_show, vfi_event_offset_store);

static struct attribute *vfi_event_default_attrs[] = {
    &vfi_event_attr_default.attr,
    &vfi_event_attr_name.attr,
    &vfi_event_attr_location.attr,
    &vfi_event_attr_offset.attr,
    0,
};

struct kobj_type vfi_event_type = {
    .release = vfi_event_release,
    .sysfs_ops = &vfi_event_sysfs_ops,
    .default_attrs = vfi_event_default_attrs,
};

int find_vfi_event(struct vfi_event **event, struct vfi_events *parent, int id)
{
	char buf[20];
	sprintf(buf,"%x",id);
	*event = to_vfi_event(kset_find_obj(&parent->kset,buf));
	return VFI_RESULT(*event == NULL);
}

static int vfi_event_uevent_filter(struct kset *kset, struct kobject *kobj)
{
	return 0; /* Do not generate event */
}

static const char *vfi_event_uevent_name(struct kset *kset, struct kobject *kobj)
{
	return "dunno";
}

static int vfi_event_uevent(struct kset *kset, struct kobject *kobj, struct kobj_uevent_env *env)
{
	return 0; /* Do not generate event */
}


static struct kset_uevent_ops vfi_event_uevent_ops = {
	.filter = vfi_event_uevent_filter,
	.name = vfi_event_uevent_name,
	.uevent = vfi_event_uevent,
};

int new_vfi_event(struct vfi_event **event, struct vfi_events *parent, struct vfi_desc_param *desc, struct vfi_bind *bind, void (*f)(struct vfi_bind *),int id)
{
	struct vfi_event *new = kzalloc(sizeof(struct vfi_event), GFP_KERNEL);
	int ret;
    
	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	*event = new;

	if (NULL == new)
		return VFI_RESULT(-ENOMEM);

	vfi_clone_desc(&new->desc,desc);
	new->start_event = f;
	new->bind = bind;
	new->event_id = id;

	ret = kobject_init_and_add(&new->kobj, &vfi_event_type, &parent->kset.kobj, "%p:%x", desc, id);
	if (ret) 
		kobject_put(&new->kobj);

	return VFI_RESULT(ret);
}

int vfi_event_create(struct vfi_event **new, struct vfi_events *parent, struct vfi_desc_param *desc,
		       struct vfi_bind *bind, void (*f)(struct vfi_bind *),int id)
{
	int ret;

	VFI_DEBUG(MY_DEBUG,"%s parent(%p) desc(%p) bind(%p) f(%p) id(%08x)\n",__FUNCTION__,parent,desc,bind,f,id);

	ret = new_vfi_event(new,parent, desc, bind, f, id);

	if (ret) 
		return VFI_RESULT(ret);

	return VFI_RESULT(0);
}

void vfi_event_delete (struct vfi_event *vfi_event)
{
	VFI_KTRACE ("<*** %s (%p) Id: %08x IN ***>\n", __func__, vfi_event, (vfi_event) ? vfi_event->event_id : 0xffffffff);
	kobject_del (&vfi_event->kobj);
	VFI_KTRACE ("<*** %s OUT ***>\n", __func__);
}

void vfi_event_send(struct vfi_event *event)
{
	vfi_doorbell_send(event->desc.address, event->event_id);
}
