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

#ifndef VFI_DRV_H
#define VFI_DRV_H

#include <linux/vfi_subsys.h>

#define MAJOR_VERSION(v) ((v) >> 16)
#define MINOR_VERSION(v) ((v) & 0xffff)
#define VERSION 1 << 16 | 1

extern int vfi_major;
extern int vfi_minor;
extern int vfi_nr_minor;

extern struct vfi_subsys *vfi_subsys;

#endif /* VFI_DRV_H */
