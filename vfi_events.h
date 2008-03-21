#ifndef RDDMA_EVENTS_H
#define RDDMA_EVENTS_H

#include <linux/vfi.h>
#include <linux/vfi_subsys.h>


struct rddma_events {
	int count;
	struct semaphore start_lock;
	struct completion dma_sync;
	struct kset kset;
	struct rddma_events *next;
	struct rddma_events *prev;
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
	if (rddma_events)
		kset_put(&rddma_events->kset);
}

extern int new_rddma_events(struct rddma_events **, struct rddma_readies *, char *name);
extern int rddma_events_register(struct rddma_events *);
extern void rddma_events_unregister(struct rddma_events *);
extern int find_rddma_events(struct rddma_events **, struct rddma_readies *, char *);
extern struct kobj_type rddma_events_type;
extern int rddma_events_create(struct rddma_events **, struct rddma_readies *, char *name);
extern void rddma_events_delete(struct rddma_events *);
extern void rddma_events_start(struct rddma_events *);
#endif /* RDDMA_EVENTS_H */
