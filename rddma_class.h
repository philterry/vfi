#ifndef RDDMA_CLASS_H
#define RDDMA_CLASS_H

#include <linux/rddma_subsys.h>

struct rddma_dev {
	struct class_device class_device;
};

extern int rddma_class_register(struct rddma_subsys *rsys);
extern void rddma_class_unregister(struct rddma_subsys *rsys);

#endif	/* RDDMA_CLASS_H */
