/* 
 * 
 * Copyright 2007 MicroMemory, LLC.
 * Phil Terry <pterry@micromemory.com> 
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#define MY_DEBUG      VFI_DBG_SUBSYS | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_SUBSYS | VFI_DBG_LIFE    | VFI_DBG_DEBUG

#include <linux/vfi.h>
#include <linux/vfi_class.h>

#define to_vfi_dev(d) d ? container_of((d), struct vfi_dev, class_device) : NULL

static void vfi_release_device(struct class_device *dev)
{
	struct vfi_dev *p = to_vfi_dev(dev);
	VFI_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,p);
	kfree(p);
}

static void vfi_release_class(struct class *class)
{
	VFI_DEBUG(MY_LIFE_DEBUG,"%s FIX ME %p\n",__FUNCTION__,class);
	
}

ssize_t vfi_class_dev_show(struct class_device *dev, char *buf)
{
	return sprintf(buf,"Hi I'm an vfi class device\n");
}

ssize_t vfi_class_dev_store(struct class_device *dev, const char *buf, size_t count)
{
	return count;
}

struct class_device_attribute vfi_class_dev_defaults[] = {
	__ATTR(fred,0644,vfi_class_dev_show, vfi_class_dev_store),
	__ATTR_NULL,
};

#define to_vfi(c) container_of((c), struct vfi, class)

ssize_t vfi_class_show(struct class *class, char *buf)
{
	return sprintf(buf,"Hi I'm the vfi class\n");
}

ssize_t vfi_class_store(struct class *class, const char *buf, size_t count)
{
	return count;
}

struct class_attribute vfi_class_attributes[] = {
	__ATTR(version,0644,vfi_class_show, vfi_class_store),
	__ATTR_NULL,
};

int vfi_class_register(struct vfi_subsys *rsys)
{
	struct class *class = &rsys->class;

	class->release = vfi_release_device;
	class->class_release = vfi_release_class;
	class->name = "vfi";
	class->owner = THIS_MODULE;
	class->class_attrs = vfi_class_attributes;
	class->class_dev_attrs = vfi_class_dev_defaults;
	return class_register(class);

}

void vfi_class_unregister(struct vfi_subsys *rsys)
{
	class_unregister(&rsys->class);
}
