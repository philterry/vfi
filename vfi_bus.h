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

#ifndef VFI_BUS_H
#define VFI_BUS_H

#include <linux/vfi_subsys.h>

extern int vfi_bus_register(struct vfi_subsys *rsys);
extern void vfi_bus_unregister(struct vfi_subsys *rsys);


#endif	/* VFI_BUS_H */
