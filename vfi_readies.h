#ifndef VFI_READIES_H
#define VFI_READIES_H

#include <linux/vfi.h>

struct vfi_readies {
    struct kset kset;
};

static inline struct vfi_readies *to_vfi_readies(struct kobject *kobj)
{
    return kobj ? container_of(to_kset(kobj), struct vfi_readies,kset) : NULL;
}

static inline struct vfi_readies *vfi_readies_get(struct vfi_readies *vfi_readies)
{
    return to_vfi_readies(kobject_get(&vfi_readies->kset.kobj));
}

static inline void vfi_readies_put(struct vfi_readies *vfi_readies)
{
    kset_put(&vfi_readies->kset);
}
struct vfi_events;
extern int new_vfi_readies(struct vfi_readies **, struct vfi_subsys *, char *name);
extern int vfi_readies_register(struct vfi_readies *);
extern void vfi_readies_unregister(struct vfi_readies *);
extern int find_vfi_readies(struct vfi_events **, struct vfi_subsys *, char *);
extern struct kobj_type vfi_readies_type;
extern int vfi_readies_create(struct vfi_readies **, struct vfi_subsys *, char *name);
extern void vfi_readies_delete(struct vfi_readies *);
#endif /* VFI_READIES_H */
