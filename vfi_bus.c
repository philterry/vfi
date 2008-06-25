/* 
 * 
 * Copyright 2008 Vmetro
 * Phil Terry <pterry@vmetro.com> 
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#define MY_DEBUG      VFI_DBG_SUBSYS | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_SUBSYS | VFI_DBG_LIFE    | VFI_DBG_DEBUG

#include <linux/vfi.h>

#include <linux/vfi_bus.h>
#include <linux/device.h>

ssize_t vfi_bus_attr_fred_show(struct bus_type *bus, char *buf)
{
	return sprintf(buf, "Hi I'm fred of %s\n",bus->name);
}

ssize_t vfi_bus_attr_fred_store(struct bus_type *bus, const char *buf, size_t count)
{
	return count;
}

struct bus_attribute vfi_bus_attribute = {
	.attr = {"vfi_bus_attr_fred", THIS_MODULE, 0644},
	.show = vfi_bus_attr_fred_show,
	.store = vfi_bus_attr_fred_store,
};

int vfi_bus_match(struct device *dev, struct device_driver *drv)
{
	return 0; /* Not mine */
}

int vfi_bus_probe(struct device *dev)
{
	return 0;
}

int vfi_bus_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	return -ENODEV;
}

struct bus_type vfi_bus_type = {
	.name = "vfi_bus",
	.probe = vfi_bus_probe,
	.match = vfi_bus_match,
 	.uevent = vfi_bus_uevent, 
};

int vfi_bus_register(struct vfi_subsys *rsys)
{
	return bus_register(&vfi_bus_type);
}

void vfi_bus_unregister(struct vfi_subsys *rsys)
{
	bus_unregister(&vfi_bus_type);
}
