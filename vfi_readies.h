#ifndef RDDMA_READIES_H
#define RDDMA_READIES_H

#include <linux/rddma.h>

struct rddma_readies {
    struct kset kset;
};

static inline struct rddma_readies *to_rddma_readies(struct kobject *kobj)
{
    return kobj ? container_of(to_kset(kobj), struct rddma_readies,kset) : NULL;
}

static inline struct rddma_readies *rddma_readies_get(struct rddma_readies *rddma_readies)
{
    return to_rddma_readies(kobject_get(&rddma_readies->kset.kobj));
}

static inline void rddma_readies_put(struct rddma_readies *rddma_readies)
{
    kset_put(&rddma_readies->kset);
}
struct rddma_events;
extern int new_rddma_readies(struct rddma_readies **, struct rddma_subsys *, char *name);
extern int rddma_readies_register(struct rddma_readies *);
extern void rddma_readies_unregister(struct rddma_readies *);
extern int find_rddma_readies(struct rddma_events **, struct rddma_subsys *, char *);
extern struct kobj_type rddma_readies_type;
extern int rddma_readies_create(struct rddma_readies **, struct rddma_subsys *, char *name);
extern void rddma_readies_delete(struct rddma_readies *);
#endif /* RDDMA_READIES_H */
