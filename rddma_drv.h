#ifndef RDDMA_DRV_H
#define RDDMA_DRV_H

#include <linux/rddma_subsys.h>

#define MAJOR_VERSION(v) ((v) >> 16)
#define MINOR_VERSION(v) ((v) & 0xffff)
#define VERSION 1 << 16 | 1

extern int rddma_major;
extern int rddma_minor;
extern int rddma_nr_minor;

extern struct rddma_subsys *rddma_subsys;

#endif /* RDDMA_DRV_H */
