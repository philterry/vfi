#ifndef RDDMA_MMAPS_H
#define RDDMA_MMAPS_H

#include <linux/rddma_smb.h>

struct rddma_mmaps {
    struct kset kset;
};

static inline struct rddma_mmaps *to_rddma_mmaps(struct kobject *kobj)
{
    return kobj ? container_of(to_kset(kobj), struct rddma_mmaps,kset) : NULL;
}

static inline struct rddma_mmaps *rddma_mmaps_get(struct rddma_mmaps *rddma_mmaps)
{
    return to_rddma_mmaps(kobject_get(&rddma_mmaps->kset.kobj));
}

static inline void rddma_mmaps_put(struct rddma_mmaps *rddma_mmaps)
{
    kset_put(&rddma_mmaps->kset);
}

extern struct rddma_mmaps *new_rddma_mmaps(struct rddma_smb *,char *name);
extern int rddma_mmaps_register(struct rddma_mmaps *);
extern void rddma_mmaps_unregister(struct rddma_mmaps *);
extern struct rddma_mmaps *find_rddma_mmaps(char *);
extern struct kobj_type rddma_mmaps_type;
#endif /* RDDMA_MMAPS_H */
