#ifndef RDDMA_EVENT_H
#define RDDMA_EVENT_H

#include <linux/rddma.h>
#include <linux/rddma_parse.h>
#include <linux/rddma_events.h>

struct rddma_event {
	struct rddma_desc_param desc;
	int event_id;
	struct rddma_bind *bind;
	void (*start_event)(struct rddma_bind *);
	struct kobject kobj;
};

static inline struct rddma_event *to_rddma_event(struct kobject *kobj)
{
    return kobj ? container_of(kobj, struct rddma_event,kobj) : NULL;
}

static inline struct rddma_event *rddma_event_get(struct rddma_event *rddma_event)
{
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
    return to_rddma_event(kobject_get(&rddma_event->kobj));
}

static inline void rddma_event_put(struct rddma_event *rddma_event)
{
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	if(rddma_event)
		kobject_put(&rddma_event->kobj);
}

extern int new_rddma_event(struct rddma_event **, struct rddma_events *, struct rddma_desc_param *, struct rddma_bind *, void (*)(struct rddma_bind *),int);
extern int rddma_event_register(struct  rddma_event *);
extern void rddma_event_unregister(struct rddma_event *);
extern int find_rddma_event(struct rddma_event **, struct rddma_events *, int);
extern struct kobj_type rddma_event_type;
extern int rddma_event_create(struct rddma_event **, struct rddma_events *, struct rddma_desc_param *, struct rddma_bind *, void (*)(struct rddma_bind *), int);
extern void rddma_event_delete(struct rddma_event *);
extern void rddma_event_send(struct rddma_event *);
#endif /* RDDMA_EVENT_H */
