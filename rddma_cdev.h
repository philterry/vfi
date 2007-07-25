#ifndef RDDMA_CDEV_H
#define RDDMA_CDEV_H

#include <linux/rddma_subsys.h>

extern int rddma_cdev_register(struct rddma_subsys *rsys);
extern void rddma_cdev_unregister(struct rddma_subsys *rsys);

#endif	/* RDDMA_CDEV_H */
