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

#ifndef VFI_CLASS_H
#define VFI_CLASS_H

#include <linux/vfi_subsys.h>

struct vfi_dev {
	struct class_device class_device;
};

extern int vfi_class_register(struct vfi_subsys *rsys);
extern void vfi_class_unregister(struct vfi_subsys *rsys);

#endif	/* VFI_CLASS_H */
