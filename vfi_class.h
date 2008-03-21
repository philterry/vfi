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

#ifndef RDDMA_CLASS_H
#define RDDMA_CLASS_H

#include <linux/vfi_subsys.h>

struct rddma_dev {
	struct class_device class_device;
};

extern int rddma_class_register(struct rddma_subsys *rsys);
extern void rddma_class_unregister(struct rddma_subsys *rsys);

#endif	/* RDDMA_CLASS_H */
