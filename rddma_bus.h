#ifndef RDDMA_BUS_H
#define RDDMA_BUS_H

#include <linux/rddma_subsys.h>

extern int rddma_bus_register(struct rddma_subsys *rsys);
extern void rddma_bus_unregister(struct rddma_subsys *rsys);


#endif	/* RDDMA_BUS_H */
