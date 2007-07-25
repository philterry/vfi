#ifndef RDDMA_BIND_H
#define RDDMA_BIND_H

#include <linux/rddma.h>
#include <linux/rddma_xfer.h>
#include <linux/rddma_parse.h>

struct rddma_dst;

struct rddma_bind {
	struct rddma_bind_param desc;
	struct kobject kobj;
	struct rddma_dsts *dsts;
	struct rddma_dst *head_dst;
};

static inline struct rddma_bind *to_rddma_bind(struct kobject *kobj)
{
    return kobj ? container_of(kobj, struct rddma_bind, kobj) : NULL;
}

static inline struct rddma_bind *rddma_bind_get(struct rddma_bind *rddma_bind)
{
    return to_rddma_bind(kobject_get(&rddma_bind->kobj));
}

static inline void rddma_bind_put(struct rddma_bind *rddma_bind)
{
    kobject_put(&rddma_bind->kobj);
}

extern struct rddma_bind *new_rddma_bind(struct rddma_xfer *, struct rddma_xfer_param *);
extern int rddma_bind_register(struct rddma_bind *);
extern void rddma_bind_unregister(struct rddma_bind *);
extern struct rddma_bind *find_rddma_bind(struct rddma_xfer_param *);
extern struct rddma_bind *rddma_bind_create(struct rddma_xfer *, struct rddma_xfer_param *);
extern void rddma_bind_delete(struct rddma_bind *);
extern void rddma_bind_load_dsts(struct rddma_bind *);
extern struct kobj_type rddma_bind_type;
#endif /* RDDMA_BIND_H */
