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

#define MY_DEBUG      RDDMA_DBG_SUBSYS | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_SUBSYS | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/rddma.h>
#include <linux/rddma_class.h>

#define to_rddma_dev(d) d ? container_of((d), struct rddma_dev, class_device) : NULL

static void rddma_release_device(struct class_device *dev)
{
	struct rddma_dev *p = to_rddma_dev(dev);
	RDDMA_DEBUG(MY_LIFE_DEBUG,"%s %p\n",__FUNCTION__,p);
	kfree(p);
}

static void rddma_release_class(struct class *class)
{
	
}

ssize_t rddma_class_dev_show(struct class_device *dev, char *buf)
{
	return sprintf(buf,"Hi I'm an rddma class device\n");
}

ssize_t rddma_class_dev_store(struct class_device *dev, const char *buf, size_t count)
{
	return count;
}

struct class_device_attribute rddma_class_dev_defaults[] = {
	__ATTR(fred,0644,rddma_class_dev_show, rddma_class_dev_store),
	__ATTR_NULL,
};

#define to_rddma(c) container_of((c), struct rddma, class)

ssize_t rddma_class_show(struct class *class, char *buf)
{
	return sprintf(buf,"Hi I'm the rddma class\n");
}

ssize_t rddma_class_store(struct class *class, const char *buf, size_t count)
{
	return count;
}

struct class_attribute rddma_class_attributes[] = {
	__ATTR(version,0644,rddma_class_show, rddma_class_store),
	__ATTR_NULL,
};

int rddma_class_register(struct rddma_subsys *rsys)
{
	struct class *class = &rsys->class;

	class->release = rddma_release_device;
	class->class_release = rddma_release_class;
	class->name = "rddma";
	class->owner = THIS_MODULE;
	class->class_attrs = rddma_class_attributes;
	class->class_dev_attrs = rddma_class_dev_defaults;
	return class_register(class);

}

void rddma_class_unregister(struct rddma_subsys *rsys)
{
	class_unregister(&rsys->class);
}
