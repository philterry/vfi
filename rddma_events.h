#ifndef RDDMA_EVENTS_H
#define RDDMA_EVENTS_H

#include <linux/rddma.h>
#include <linux/rddma_subsys.h>

struct rddma_events {
    struct kset kset;
};

static inline struct rddma_events *to_rddma_events(struct kobject *kobj)
{
    return kobj ? container_of(to_kset(kobj), struct rddma_events,kset) : NULL;
}

static inline struct rddma_events *rddma_events_get(struct rddma_events *rddma_events)
{
    return to_rddma_events(kobject_get(&rddma_events->kset.kobj));
}

static inline void rddma_events_put(struct rddma_events *rddma_events)
{
    kset_put(&rddma_events->kset);
}

extern struct rddma_events *new_rddma_events(struct rddma_subsys *, char *name);
extern int rddma_events_register(struct rddma_events *);
extern void rddma_events_unregister(struct rddma_events *);
extern struct rddma_events *find_rddma_events(struct rddma_subsys *, char *);
extern struct kobj_type rddma_events_type;
extern struct rddma_events *rddma_events_create(struct rddma_subsys *, char *name);
extern void rddma_events_delete(struct rddma_events *);
#endif /* RDDMA_EVENTS_H */
