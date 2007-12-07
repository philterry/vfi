#ifndef RDDMA_EVENT_H
#define RDDMA_EVENT_H

#include <linux/rddma.h>
#include <linux/rddma_parse.h>
#include <linux/rddma_events.h>

struct rddma_event {
	struct rddma_desc_param desc;
	struct kobject kobj;
};

static inline struct rddma_event *to_rddma_event(struct kobject *kobj)
{
    return kobj ? container_of(kobj, struct rddma_event,kobj) : NULL;
}

static inline struct rddma_event *rddma_event_get(struct rddma_event *rddma_event)
{
    return to_rddma_event(kobject_get(&rddma_event->kobj));
}

static inline void rddma_event_put(struct rddma_event *rddma_event)
{
    kobject_put(&rddma_event->kobj);
}

extern struct rddma_event *new_rddma_event(struct rddma_events *, char *name);
extern int rddma_event_register(struct  rddma_event *);
extern void rddma_event_unregister(struct rddma_event *);
extern struct rddma_event *find_rddma_event(struct rddma_events *, char *);
extern struct kobj_type rddma_event_type;
#endif /* RDDMA_EVENT_H */
