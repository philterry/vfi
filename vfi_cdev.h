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

#ifndef VFI_CDEV_H
#define VFI_CDEV_H

#include <linux/vfi_subsys.h>

extern int vfi_cdev_register(struct vfi_subsys *rsys);
extern void vfi_cdev_unregister(struct vfi_subsys *rsys);

#endif	/* VFI_CDEV_H */
