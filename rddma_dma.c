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

#include <linux/rddma_dma.h>
#include <linux/rio.h>
#include <linux/rio_ids.h>
#include <linux/rio_drv.h>

void rddma_dealloc_pages( struct page **pages, int num_pages)
{
	while(num_pages--)
		__free_pages(pages[num_pages],0);
	kfree(pages);
}

int rddma_alloc_pages( size_t size, struct page **pages[], int *num_pages)
{
	int page;
	struct page **page_ary;
	if (pages)
		*pages = 0;
	else
		return -EINVAL;
 
	if (num_pages)
		*num_pages = (size >> PAGE_SHIFT) + (size & PAGE_MASK ? 1 : 0);
	else
		return -EINVAL;

	page_ary = kzalloc(sizeof(struct page *)* *num_pages, GFP_KERNEL);
	if (NULL == page_ary)
		return -ENOMEM;

	for (page=0; page < *num_pages; page++) {
		if ( (page_ary[page] = alloc_page(GFP_KERNEL)) )
			continue;
		break;
	}
       
	if ( page == *num_pages ) {
		*pages = page_ary;
		return 0;
	}

	while (page--)
		__free_pages(page_ary[page],0);

	*num_pages = 0;

	kfree(page_ary);

	return -ENOMEM;
}

static void rddma_dma_remove(struct rddma_dma_dev *rdev)
{
}

static int rddma_dma_probe(struct rddma_dma_dev *rdev, const struct rddma_dma_device_id *id)
{
	int rc = -ENODEV;
	return rc;
}

static struct rddma_dma_device_id rddma_dma_id_table[] = {

};

static struct rddma_dma_driver rddma_dma_driver = {
	.name = "rddma_drv",
	.id_table = rddma_dma_id_table,
	.probe = rddma_dma_probe,
	.remove = rddma_dma_remove,
};
static int dma_register_driver(struct rddma_dma_driver *driver)
{
	return driver_register(&driver->driver);
}

static void dma_unregister_driver(struct rddma_dma_driver *driver)
{
	driver_unregister(&driver->driver);
}

static int __init rddma_dma_init(void)
{
	return dma_register_driver(&rddma_dma_driver);
}

static void __exit rddma_dma_exit(void)
{
	dma_unregister_driver(&rddma_dma_driver);
}

module_init(rddma_dma_init);
module_exit(rddma_dma_exit);
