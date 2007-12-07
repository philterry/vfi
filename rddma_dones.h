#ifndef RDDMA_DONES_H
#define RDDMA_DONES_H

#include <linux/rddma.h>

struct rddma_dones {
    struct kset kset;
};

static inline struct rddma_dones *to_rddma_dones(struct kobject *kobj)
{
    return kobj ? container_of(to_kset(kobj), struct rddma_dones,kset) : NULL;
}

static inline struct rddma_dones *rddma_dones_get(struct rddma_dones *rddma_dones)
{
    return to_rddma_dones(kobject_get(&rddma_dones->kset.kobj));
}

static inline void rddma_dones_put(struct rddma_dones *rddma_dones)
{
    kset_put(&rddma_dones->kset);
}

extern struct rddma_dones *new_rddma_dones(struct rddma_subsys *, char *name);
extern int rddma_dones_register(struct rddma_dones *);
extern void rddma_dones_unregister(struct rddma_dones *);
extern struct rddma_events *find_rddma_dones(struct rddma_subsys *, char *);
extern struct kobj_type rddma_dones_type;
#endif /* RDDMA_DONES_H */
