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


/*
 * The Vmetro FusionIPC Distributed DMA Driver Implementation
 */

#define MY_DEBUG      VFI_DBG_SUBSYS | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_SUBSYS | VFI_DBG_LIFE    | VFI_DBG_DEBUG

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/vfi.h>
#include <linux/vfi_drv.h>
#include <linux/vfi_subsys.h>
#include <linux/vfi_class.h>
#include <linux/vfi_bus.h>
#include <linux/vfi_cdev.h>
#include <linux/vfi_fabric.h>
#include <linux/vfi_mmap.h>

int vfi_major = 0;
int vfi_minor = 0;
int vfi_nr_minor = 1;
#ifdef CONFIG_VFI_DEBUG
unsigned int vfi_debug_level = VFI_DBG_ALL & ~VFI_DBG_PARSE;
EXPORT_SYMBOL(vfi_debug_level);
module_param(vfi_debug_level, uint, 0664);
#endif
module_param(vfi_major, int, 0664);
module_param(vfi_minor, int, 0664);
module_param(vfi_nr_minor, int, 0664);


struct vfi_subsys *vfi_subsys;

static int  __init vfi_init(void)
{
	int ret = -ENOMEM;

	ret = new_vfi_subsys(&vfi_subsys, "vfi");

	if ( NULL == vfi_subsys )
		return ret;

	if ( (ret = vfi_subsys_register(vfi_subsys)) )
		goto subsys_fail;

	if ( (ret = vfi_class_register(vfi_subsys)) )
		goto class_fail;

	if ( (ret = vfi_bus_register(vfi_subsys)) )
		goto bus_fail;

/* 	if ( (ret = vfi_fabric_register(vfi_subsys)) ) */
/* 		goto fabric_fail; */

	if ( (ret = vfi_cdev_register(vfi_subsys)) )
		goto cdev_fail;
	
	return 0;

cdev_fail:
/* 	vfi_fabric_unregister(vfi_subsys); */

/* fabric_fail: */
	vfi_bus_unregister(vfi_subsys);

bus_fail:
	vfi_class_unregister(vfi_subsys);

class_fail:
	vfi_subsys_unregister(vfi_subsys);

subsys_fail:
	vfi_subsys_put(vfi_subsys);
	return ret;
}

static void __exit vfi_cleanup(void)
{
	vfi_cdev_unregister(vfi_subsys);
/* 	vfi_fabric_unregister(vfi_subsys); */
	vfi_bus_unregister(vfi_subsys);
	vfi_class_unregister(vfi_subsys);
	vfi_subsys_unregister(vfi_subsys);
}

module_init(vfi_init);
module_exit(vfi_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phil Terry <pterry@vmetro.com>");
MODULE_DESCRIPTION("The Vmetro FusionIPC Distributed DMA Driver");
