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


/*
 * The Rincon Distributed DMA Driver Implementation
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/rddma.h>
#include <linux/rddma_drv.h>
#include <linux/rddma_subsys.h>
#include <linux/rddma_class.h>
#include <linux/rddma_bus.h>
#include <linux/rddma_cdev.h>
#include <linux/rddma_fabric.h>

int rddma_major = 0;
int rddma_minor = 0;
int rddma_nr_minor = 1;
#ifdef CONFIG_RDDMA_DEBUG
int rddma_debug_level = 7;
EXPORT_SYMBOL(rddma_debug_level);
module_param(rddma_debug_level, int, 0664);
#endif
module_param(rddma_major, int, 0664);
module_param(rddma_minor, int, 0664);
module_param(rddma_nr_minor, int, 0664);


struct rddma_subsys *rddma_subsys;

static int  __init rddma_init(void)
{
	int ret = -ENOMEM;

	rddma_subsys = new_rddma_subsys("rddma");

	if ( NULL == rddma_subsys )
		return ret;

	if ( (ret = rddma_subsys_register(rddma_subsys)) )
		goto subsys_fail;

	if ( (ret = rddma_class_register(rddma_subsys)) )
		goto class_fail;

	if ( (ret = rddma_bus_register(rddma_subsys)) )
		goto bus_fail;

/* 	if ( (ret = rddma_fabric_register(rddma_subsys)) ) */
/* 		goto fabric_fail; */

	if ( (ret = rddma_cdev_register(rddma_subsys)) )
		goto cdev_fail;

	return 0;

cdev_fail:
/* 	rddma_fabric_unregister(rddma_subsys); */

/* fabric_fail: */
	rddma_bus_unregister(rddma_subsys);

bus_fail:
	rddma_class_unregister(rddma_subsys);

class_fail:
	rddma_subsys_unregister(rddma_subsys);

subsys_fail:
	rddma_subsys_put(rddma_subsys);
	return ret;
}

static void __exit rddma_cleanup(void)
{
	rddma_cdev_unregister(rddma_subsys);
/* 	rddma_fabric_unregister(rddma_subsys); */
	rddma_bus_unregister(rddma_subsys);
	rddma_class_unregister(rddma_subsys);
	rddma_subsys_unregister(rddma_subsys);
}

module_init(rddma_init);
module_exit(rddma_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phil Terry <pterry@micromemory.com>");
MODULE_DESCRIPTION("The Micromemory Rincon Distributed DMA Driver");
