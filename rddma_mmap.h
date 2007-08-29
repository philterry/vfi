/* 
 * 
 * Copyright 2007 MicroMemory, LLC. 
 * Trevor Anderson <tanderson@micromemory.com> 
 * 
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or  (at your
 * option) any later version.
 */

#ifndef RDDMA_MMAP_H
#define RDDMA_MMAP_H

#include <linux/rddma.h>
#include <linux/rddma_smb.h>
#include <linux/rddma_parse.h>

struct rddma_mmap {
	struct rddma_desc_param desc;
	unsigned long		t_id;		/* Ticket number, for verification */
	struct page		**pg_tbl;	/* Page table pointer to start of block */
	unsigned long		n_pg;		/* Number of pages in block */
	struct kobject kobj;
};

static inline unsigned long mmap_to_ticket(struct rddma_mmap *m)
{
	unsigned long t = (unsigned long)m & 0xffffffffUL;
	t = (t & ~0xffff) ^ ((t & 0xffff) << 16);
	return t;
}

static inline int is_mmap_ticket(struct rddma_mmap *m, unsigned long t)
{
	unsigned long s = (unsigned long)m & 0xffffffffUL;
	return (((s & ~0xffff) ^ t) >> 16) == (s & 0xffff);	
}

static inline struct rddma_mmap *to_rddma_mmap(struct kobject *kobj)
{
    return kobj ? container_of(kobj, struct rddma_mmap,kobj) : NULL;
}

static inline struct rddma_mmap *rddma_mmap_get(struct rddma_mmap *rddma_mmap)
{
    return to_rddma_mmap(kobject_get(&rddma_mmap->kobj));
}

static inline void rddma_mmap_put(struct rddma_mmap *rddma_mmap)
{
    kobject_put(&rddma_mmap->kobj);
}

extern struct rddma_mmap *new_rddma_mmap(struct rddma_smb *, struct rddma_desc_param *);
extern int rddma_mmap_register(struct rddma_mmap *);
extern void rddma_mmap_unregister(struct rddma_mmap *);
extern struct rddma_mmap *find_rddma_mmap_by_id(unsigned long);
extern struct rddma_mmap *find_rddma_mmap(struct rddma_smb *, struct rddma_desc_param *);
extern struct rddma_mmap *rddma_mmap_create(struct rddma_smb *, struct rddma_desc_param *);
extern void rddma_mmap_delete(struct rddma_smb *, struct rddma_desc_param *);
extern struct kobj_type rddma_mmap_type;
#endif /* RDDMA_MMAP_H */
