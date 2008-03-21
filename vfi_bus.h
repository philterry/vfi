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

#ifndef RDDMA_BUS_H
#define RDDMA_BUS_H

#include <linux/vfi_subsys.h>

extern int rddma_bus_register(struct rddma_subsys *rsys);
extern void rddma_bus_unregister(struct rddma_subsys *rsys);


#endif	/* RDDMA_BUS_H */
