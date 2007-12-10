/* 
 * 
 * Copyright 2007 MicroMemory, LLC.
 * Phil Terry <pterry@micromemory.com> 
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#define MY_DEBUG      RDDMA_DBG_LOCOPS | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_LOCOPS | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/rddma_location.h>
#include <linux/rddma_ops.h>
#include <linux/rddma_smb.h>
#include <linux/rddma_dst.h>
#include <linux/rddma_src.h>
#include <linux/rddma_xfer.h>
#include <linux/rddma_bind.h>
#include <linux/rddma_binds.h>
#include <linux/rddma_smbs.h>
#include <linux/rddma_xfers.h>
#include <linux/rddma_srcs.h>
#include <linux/rddma_dsts.h>
#include <linux/rddma_parse.h>
#include <linux/rddma_drv.h>
#include <linux/rddma_mmap.h>
#include <linux/rddma_events.h>
#include <linux/rddma_event.h>

#include <linux/device.h>
#include <linux/mm.h>

extern void rddma_dma_chain_dump(struct list_head *h);
/*
 * F I N D    O P E R A T I O N S
 */
static struct rddma_location *rddma_local_location_find(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct rddma_location *newloc;
	RDDMA_DEBUG(MY_DEBUG,"%s %p %s %p %s,%s\n",__FUNCTION__,loc,loc->desc.name,desc,desc->name,desc->location);
	newloc  = to_rddma_location(kset_find_obj(&loc->kset,desc->name));
	RDDMA_DEBUG(MY_DEBUG,"%s %p %s %p %s ->%p\n",__FUNCTION__,loc,loc->desc.name,desc,desc->name,newloc);
	return newloc;
}

static void rddma_local_location_put(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct rddma_location *newloc;
	RDDMA_DEBUG(MY_DEBUG,"%s %p %p %s\n",__FUNCTION__,loc,desc,desc->name);
	newloc = to_rddma_location(kset_find_obj(&loc->kset,desc->name));
	RDDMA_DEBUG(MY_DEBUG,"%s %p %p %s ->%p\n",__FUNCTION__,loc,desc,desc->name,newloc);
	rddma_location_put(newloc);
	rddma_location_put(newloc);
}

static struct rddma_smb *rddma_local_smb_find(struct rddma_location *parent, struct rddma_desc_param *desc)
{
	struct rddma_smb *smb = to_rddma_smb(kset_find_obj(&parent->smbs->kset,desc->name));
	RDDMA_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,parent,desc,smb);
	return smb;
}

static struct rddma_xfer *rddma_local_xfer_find(struct rddma_location *parent, struct rddma_bind_param *desc)
{
	struct rddma_xfer *xfer = to_rddma_xfer(kset_find_obj(&parent->xfers->kset,desc->xfer.name));
	RDDMA_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,parent,desc,xfer);
	return xfer;
}

static struct rddma_bind *rddma_local_bind_find(struct rddma_xfer *parent, struct rddma_bind_param *desc)
{
	struct rddma_bind *bind = NULL;
	char buf[128];

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	if ( snprintf(buf,128,"#%llx:%x", desc->xfer.offset, desc->xfer.extent) > 128 )
		goto out;

	bind = to_rddma_bind(kset_find_obj(&parent->binds->kset,buf));

out:
	RDDMA_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,parent,desc,bind);
	return bind;
}

static struct rddma_dst *rddma_local_dst_find(struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	struct rddma_dst *dst = NULL;
	char *buf = kzalloc (2048, GFP_KERNEL);
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	if (!buf) goto out;
	
	if (snprintf (buf, 2048, "%s.%s#%llx:%x",
		      desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent
		    ) >= 2048) {
		goto fail_printf;
	}
	
	dst = to_rddma_dst (kset_find_obj (&parent->dsts->kset, buf));
	
fail_printf:
	kfree (buf);
out:
	RDDMA_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,parent,desc,dst);
	return dst;
}

static struct rddma_src *rddma_local_src_find(struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	struct rddma_src *src = NULL;
	char *buf = kzalloc (2048, GFP_KERNEL);
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	if (!buf) goto out;
	
	if (snprintf (buf, 2048, "#%llx:%x", desc->src.offset, desc->src.extent) >= 2048) {
		goto fail_printf;
	}
	
	src = to_rddma_src (kset_find_obj (&parent->srcs->kset, buf));
	
fail_printf:
	kfree (buf);
out:
	RDDMA_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,parent,desc,src);
	return src;
}

/*
 * C R E A T E     O P E R A T I O N S
 */
static struct rddma_location *rddma_local_location_create(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct rddma_location *newloc;
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);


	newloc = rddma_location_create(loc,desc);

	if (newloc && newloc->desc.address)
		newloc->desc.address->ops->register_location(newloc);

	RDDMA_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,loc,desc,newloc);

	return newloc;
}

static struct rddma_smb *rddma_local_smb_create(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct rddma_smb *smb = NULL;
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	smb = rddma_smb_create(loc, desc);
	if (smb == NULL)
		return NULL;

	/* Allocate memory for this SMB */
	if (rddma_alloc_pages(smb->size, smb->desc.offset, &smb->pages, 
		&smb->num_pages) == 0)
		return smb;

	/* Failed */
	rddma_smb_delete(smb);
	return NULL;
}

static struct rddma_mmap *rddma_local_mmap_create(struct rddma_smb *smb, struct rddma_desc_param *desc)
{
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	return rddma_mmap_create(smb,desc);
}

static int rddma_local_dst_events(struct rddma_bind *bind, struct rddma_bind_param *desc)
{
	/* Local destination SMB with a local transfer agent. */
	struct rddma_events *event_list;
	char *event_name;

	event_name = rddma_get_option(&bind->desc.dst,"event_name");

	event_list = find_rddma_events(rddma_subsys->dones,event_name);
	if (event_list == NULL)
		rddma_events_create(rddma_subsys->dones,event_name);
	bind->dst_done_event = rddma_event_create(event_list,&bind->desc.xfer,bind,0,-1);

	event_list = find_rddma_events(rddma_subsys->readies,event_name);
	if (event_list == NULL)
		rddma_events_create(rddma_subsys->readies,event_name);
	bind->dst_ready_event = rddma_event_create(event_list,&desc->dst,bind,desc->dst.ops->dst_ready,-1);
						   
	return 0;
}

static struct rddma_dsts *rddma_local_dsts_create(struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	int page, first_page, last_page;
	struct rddma_bind_param params = *desc;
	struct rddma_smb *dsmb = NULL;
	struct rddma_dst *new = NULL;

	parent->desc.xfer.ops->dst_events(parent,desc);

	rddma_dsts_create(parent,desc);

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	if ( NULL == (dsmb = find_rddma_smb(&desc->dst)) )
		goto fail_dsmb;

	if (!DESC_VALID(&dsmb->desc,&desc->dst))
		goto fail_ddesc;

	first_page = START_PAGE(&dsmb->desc,&desc->dst);
	last_page = first_page + NUM_DMA(&dsmb->desc,&desc->dst);

	params.dst.offset = START_OFFSET((&dsmb->desc), (&desc->dst));
	params.src.extent = params.dst.extent = START_SIZE(&dsmb->desc, &desc->dst);
	for (page = first_page; page < last_page; page++) {
		params.dst.offset += (unsigned long)page_address(dsmb->pages[page]);
#ifdef OPTIMIZE_DESCRIPTORS
join:
		if (page + 2 <= last_page) {
			if (dsmb->pages[page] + 1 == dsmb->pages[page+1]) {
				if (page + 2 == last_page && END_SIZE(&dsmb->desc,&desc->dst))
					params.dst.extent += END_SIZE(&dsmb->desc, &desc->dst);
				else
					params.dst.extent += PAGE_SIZE;
				params.src.extent = params.dst.extent;
				++page;
				goto join;
			}
		}
#endif

		new = parent->desc.xfer.ops->dst_create(parent,&params);
		if (NULL == new)
			goto fail_newdst;

		params.dst.offset = 0;
		params.src.offset += params.src.extent;
		if ( page + 2 == last_page && END_SIZE(&dsmb->desc,&desc->dst))
			params.dst.extent = params.src.extent = END_SIZE(&dsmb->desc,&desc->dst);
		else
			params.dst.extent = params.src.extent = PAGE_SIZE;
	} 

	return parent->dsts;

fail_newdst:
fail_ddesc:
	rddma_smb_put(dsmb);
fail_dsmb:
	return NULL;
}

/**
* rddma_local_bind_create - create a bind within an xfer
* @xfer:
* @desc:
*
* This function creates a new bind attached to the local instance 
* of a parent transfer (@xfer), and creates and populates the bind's 
* descriptor subtree.
*
**/
static struct rddma_bind *rddma_local_bind_create(struct rddma_xfer *xfer, struct rddma_bind_param *desc)
{
	struct rddma_dsts *dsts;
	struct rddma_bind *bind;
	struct rddma_smb *ssmb = NULL;
	struct rddma_smb *dsmb = NULL;
	RDDMA_DEBUG (MY_DEBUG,"%s: %s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x\n",
		    __FUNCTION__, 
		     desc->xfer.name, desc->xfer.location, desc->xfer.offset, desc->xfer.extent, 
		     desc->dst.name, desc->dst.location, desc->dst.offset, desc->dst.extent, 
		     desc->src.name, desc->src.location, desc->src.offset, desc->src.extent);

	if (!desc->xfer.offset)
		desc->xfer.offset = xfer->desc.xfer.extent;

	xfer->desc.xfer.extent += desc->xfer.extent;
	
	xfer->state = RDDMA_XFER_BINDING;
	if ( (bind = rddma_bind_create(xfer, desc))) {
		if ( (dsts = rddma_dsts_create(bind,desc)) ) {
			if ( NULL == (dsmb = find_rddma_smb(&desc->dst)) )
				goto fail_dsmb;

			if (!DESC_VALID(&dsmb->desc,&desc->dst))
				goto fail_ddesc;

			if ( NULL == (ssmb = find_rddma_smb(&desc->src)) )
				goto fail_ddesc;

			if (!DESC_VALID(&ssmb->desc,&desc->src))
				goto fail_sdesc;

			rddma_inherit(&bind->desc.src,&ssmb->desc);
			rddma_inherit(&bind->desc.dst,&dsmb->desc);

			if ( NULL == bind->desc.dst.ops->dsts_create(bind,desc))
				goto fail_dst;

			rddma_xfer_load_binds(xfer,bind);
			/*
			 * Increment the parent's bind counters, which it uses
			 * to coordinate transfer execution.
			 *
			 */
			atomic_inc (&xfer->bind_count);
	
			if (rddma_debug_level & RDDMA_DBG_DMA_CHAIN)
#ifdef SERIALIZE_BIND_PROCESSING
				rddma_dma_chain_dump(&xfer->dma_chain);
#else
			/* Jimmy, "bind" was "xfer" */
			rddma_dma_chain_dump(&bind->dma_chain);
#endif

			xfer->state = RDDMA_XFER_READY;
			return bind;
		}
		RDDMA_DEBUG (MY_DEBUG, "xxx Failed to create bind %s - deleting\n", kobject_name (&bind->kobj));
		rddma_bind_delete(xfer,desc);
		bind = NULL;
	}
		
	return bind;

fail_dst:
fail_sdesc:
	rddma_smb_put(ssmb);
fail_ddesc:
	rddma_smb_put(dsmb);
fail_dsmb:
	rddma_dsts_delete(dsts);
	return NULL;
}

static struct rddma_dst *rddma_local_dst_create(struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	struct rddma_dst *dst;
	struct rddma_srcs *srcs;

	dst = rddma_dst_create(parent,desc);
	
	if (dst) {
		srcs = parent->desc.src.ops->srcs_create(dst,desc);

		if (srcs) {
			rddma_bind_load_dsts(parent);

			if (rddma_debug_level & RDDMA_DBG_DMA_CHAIN)
				rddma_dma_chain_dump(&parent->dma_chain);

			parent->end_of_chain = parent->dma_chain.prev;
		}
	}

	return dst;
}


static struct rddma_xfer *rddma_local_xfer_create(struct rddma_location *loc, struct rddma_bind_param *desc)
{
	struct rddma_xfer *xfer;
	struct rddma_bind *bind;

	unsigned long extent = desc->xfer.extent;
	unsigned long long offset = desc->xfer.offset;

	desc->xfer.offset = 0;
	desc->xfer.extent = 0;

	xfer = rddma_xfer_create(loc,desc);

	desc->xfer.offset = offset;
	desc->xfer.extent = extent;

	RDDMA_DEBUG(MY_DEBUG,"%s %p %p %p\n",__FUNCTION__,loc,desc,xfer);

	bind = rddma_local_bind_create(xfer,desc);
	
	return xfer;
}

static int rddma_local_src_events(struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	/* Local source SMB with a local transfer agent. */
	struct rddma_bind *bind;
	struct rddma_events *event_list;
	char *event_name;

	bind = rddma_dst_parent(parent);

	if (bind->src_done_event)
		return 0;

	event_name = rddma_get_option(&bind->desc.src,"event_name");

	event_list = find_rddma_events(rddma_subsys->readies,event_name);
	if (event_list == NULL)
		rddma_events_create(rddma_subsys->readies,event_name);
	bind->src_done_event = rddma_event_create(event_list,&bind->desc.xfer,bind,0,-1);

	event_list = find_rddma_events(rddma_subsys->readies,event_name);
	if (event_list == NULL)
		rddma_events_create(rddma_subsys->readies,event_name);
	bind->src_ready_event = rddma_event_create(event_list,&desc->src,bind,desc->src.ops->src_ready,-1);
						   
	return 0;
}

static struct rddma_srcs *rddma_local_srcs_create(struct rddma_dst *parent, struct rddma_bind_param *desc)
{
/* 	srcs_create://tp.x:2000/d.p#uuuuu000:1000=s.r#c000:1000 */

	int page, first_page, last_page;
	struct rddma_srcs *srcs;
	struct rddma_smb *smb;
	struct rddma_src *src;
	struct rddma_bind_param params = *desc;
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	parent->desc.xfer.ops->src_events(parent,desc);

	srcs = rddma_srcs_create(parent,desc);

	smb = find_rddma_smb(&desc->src);

	first_page = START_PAGE(&smb->desc,&desc->src);
	last_page = first_page + NUM_DMA(&smb->desc,&desc->src);

	params.src.offset = START_OFFSET(&smb->desc, &desc->src);
	params.dst.extent = params.src.extent = START_SIZE(&smb->desc, &desc->src);
	for ( page = first_page; page < last_page ; page++ ) {
		params.src.offset += (unsigned long)page_address(smb->pages[page]);
#ifdef OPTIMIZE_DESCRIPTORS
join2:
		if (page + 2 <= last_page) {
			if (smb->pages[page] + 1 == smb->pages[page+1]) {
				if (page + 2 == last_page && END_SIZE(&smb->desc,&desc->src))
					params.src.extent += END_SIZE(&smb->desc, &desc->src);
				else
					params.src.extent += PAGE_SIZE;
				++page;
				goto join2;
			}
		}
#endif

		params.dst.extent = params.src.extent;
		src = parent->desc.xfer.ops->src_create(parent,&params);
		params.src.offset = 0;
		params.dst.offset += params.src.extent;
		if (page + 2 >= last_page && END_SIZE(&smb->desc,&desc->src))
			params.src.extent = END_SIZE(&smb->desc,&desc->src);
		else
			params.src.extent = PAGE_SIZE;
	} 

	return srcs;
}

static struct rddma_src *rddma_local_src_create(struct rddma_dst *parent, struct rddma_bind_param *desc)
{
/* 	src_create://tp.x:2000/d.p#uuuuu000:800=s.r#xxxxx800:800 */
	struct rddma_src *src;
	RDDMA_DEBUG(MY_DEBUG,"%s %s.%s#%llx:%x\n",__FUNCTION__,desc->src.name,desc->src.location, desc->src.offset,desc->src.extent);

	src = rddma_src_create(parent,desc);

	rddma_dst_load_srcs(parent);
	return src;
}


/*
 * D E L E T E    O P E R A T I O N S
 */
static void rddma_local_location_delete(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct rddma_location *target = loc;
	RDDMA_DEBUG(MY_DEBUG,"%s %p %p\n",__FUNCTION__,loc,desc);
	
	if (desc->location && *desc->location)
		target = find_rddma_name(loc,desc);

	rddma_location_delete(target);
}

static void rddma_local_smb_delete(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct rddma_smb *smb = rddma_local_smb_find(loc,desc);
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
	if (smb) {
		rddma_smb_put(smb);
		rddma_smb_delete(smb);
	}
}

static void rddma_local_mmap_delete(struct rddma_smb *smb, struct rddma_desc_param *desc)
{
	rddma_mmap_delete(smb,desc);
}

/**
 * rddma_local_srcs_delete
 *
 *
 **/
static void rddma_local_srcs_delete (struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	struct list_head *entry, *safety;
	struct rddma_srcs *srcs = parent->srcs;
	
	RDDMA_DEBUG (MY_DEBUG,"%s for %s.%s#%llx:%x\n",__FUNCTION__, 
		     parent->desc.xfer.name, parent->desc.xfer.location,parent->desc.xfer.offset, parent->desc.xfer.extent);
	if (srcs) {
		RDDMA_DEBUG (MY_DEBUG, "-- Srcs \"%s\"\n", kobject_name (&srcs->kset.kobj));
		if (!list_empty (&srcs->kset.list)) {
			list_for_each_safe (entry, safety, &srcs->kset.list) {
				struct rddma_src      *src;
				struct rddma_location *loc;
				src = to_rddma_src (to_kobj (entry));
				RDDMA_DEBUG(MY_DEBUG,"-- Delete src %s (kobj %s)...\n", src->desc.src.name, kobject_name (&src->kobj));
				if ((loc = locate_rddma_location (NULL,&src->desc.src))) {
					if (loc->desc.ops && loc->desc.ops->src_delete) {
						loc->desc.ops->src_delete (parent, &src->desc);
					}
					else {
						RDDMA_DEBUG (MY_DEBUG, "xx Can\'t: \"src_delete\" operation undefined!\n");
					}
					rddma_location_put(loc);
				}
				else {
					RDDMA_DEBUG (MY_DEBUG, "xx Can\'t: src location cannot be identified!\n");
				}
			}
		}
		RDDMA_DEBUG (MY_LIFE_DEBUG, "-- Srcs Count: %lx\n", (unsigned long)srcs->kset.kobj.kref.refcount.counter);
		rddma_srcs_delete (srcs);
	}
}


/**
* rddma_local_dsts_delete - Delete bind destinations
*
* This function is invoked as part of rddma_local_bind_delete with the express
* intent of running through the list of sub-bind destinations associated with
* a given bind and issuing "dst_delete" requests for each sub-bind encountered.
*
* This function should be run the the Transfer Agent that owns the transfer to
* which the bind belongs. The "dst_delete" requests, on the other hand, must
* be executed by the Destination Agent.
*
**/
static void rddma_local_dsts_delete (struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	struct list_head *entry, *safety;
	struct rddma_dsts *dsts = parent->dsts;
	
	RDDMA_DEBUG (MY_DEBUG,"%s for %s.%s#%llx:%x\n",__FUNCTION__, 
		     parent->desc.xfer.name, parent->desc.xfer.location,parent->desc.xfer.offset, parent->desc.xfer.extent);
	if (dsts) {
		RDDMA_DEBUG (MY_DEBUG, "-- Dsts \"%s\"\n", kobject_name (&dsts->kset.kobj));
		if (!list_empty (&dsts->kset.list)) {
			list_for_each_safe (entry, safety, &dsts->kset.list) {
				struct rddma_dst      *dst;
				struct rddma_location *loc;
				dst = to_rddma_dst (to_kobj (entry));
				RDDMA_DEBUG(MY_DEBUG,"-- Delete dst %s (kobj %s)...\n", dst->desc.dst.name, kobject_name (&dst->kobj));
				if ((loc = locate_rddma_location (NULL,&dst->desc.dst))) {
					if (loc->desc.ops && loc->desc.ops->dst_delete) {
						loc->desc.ops->dst_delete (parent, &dst->desc);
					}
					else {
						RDDMA_DEBUG (MY_DEBUG, "xx Can\'t: \"dst_delete\" operation undefined!\n");
					}
					rddma_location_put(loc);
				}
				else {
					RDDMA_DEBUG (MY_DEBUG, "xx Can\'t: dst location cannot be identified!\n");
				}
			}
		}
		RDDMA_DEBUG (MY_LIFE_DEBUG, "-- Dsts Count: %lx\n", (unsigned long)dsts->kset.kobj.kref.refcount.counter);
		rddma_dsts_delete (dsts);
	}
	
}

static void rddma_local_done(struct rddma_event *event)
{
	/* A DMA engine, either local or remote, has completed a
	 * transfer involving a local smb as the source or
	 * destination. Do vote adjustment. */
	struct rddma_events *e = to_rddma_events(event->kobj.parent);
	e->count--;
}

static void rddma_local_src_done(struct rddma_bind *bind)
{
	/* A DMA engine, either local or remote, has completed a
	 * transfer involving a local smb as the source. Do vote
	 * adjustment. */
	rddma_bind_src_done(bind);
	bind->desc.src.ops->done(bind->src_done_event);
}

static void rddma_local_dst_done(struct rddma_bind *bind)
{
	/* A DMA engine, either local or remote, has completed a
	 * transfer involving a local SMB as the destination. Do vote
	 * ajustment accordingly */
	rddma_bind_dst_done(bind);
	bind->desc.dst.ops->done(bind->dst_done_event);
}

static void rddma_local_src_ready(struct rddma_bind *bind)
{
	/* Someone, either locally or via the fabric, executed start
	 * on an event which is telling us, the local DMA engine
	 * assigned for this bind, that the source SMB, which may be
	 * local or remote, is ready for action. */
	if (rddma_bind_src_ready(bind))
		bind->desc.xfer.rde->ops->queue_transfer(&bind->descriptor);
	
}

static void rddma_local_dst_ready(struct rddma_bind *bind)
{
	/* Someone, either locally or via the fabric, executed start on
	 * an event which is telling us, the local DMA engine assigned
	 * for this bind, that the destination SMB, which may be local
	 * or remote, is ready for action. */
	if (rddma_bind_dst_ready(bind))
		bind->desc.xfer.rde->ops->queue_transfer(&bind->descriptor);
}

struct rddma_ops rddma_local_ops = {
	.location_create = rddma_local_location_create,
	.location_delete = rddma_local_location_delete,
	.location_find   = rddma_local_location_find,
	.location_put    = rddma_local_location_put,
	.smb_create      = rddma_local_smb_create,
	.smb_delete      = rddma_local_smb_delete,
	.smb_find        = rddma_local_smb_find,
	.mmap_create     = rddma_local_mmap_create,
	.mmap_delete     = rddma_local_mmap_delete,
	.xfer_create     = rddma_local_xfer_create,
	.xfer_delete     = rddma_xfer_delete,
	.xfer_find       = rddma_local_xfer_find,
	.srcs_create     = rddma_local_srcs_create,
	.src_create      = rddma_local_src_create,
	.src_delete      = rddma_src_delete,
	.src_find        = rddma_local_src_find,
	.dsts_create     = rddma_local_dsts_create,
	.dsts_delete     = rddma_local_dsts_delete, 
	.dst_create      = rddma_local_dst_create,
	.dst_delete      = rddma_dst_delete,
	.dst_find        = rddma_local_dst_find,
	.bind_find       = rddma_local_bind_find,
	.bind_create     = rddma_local_bind_create,
	.bind_delete     = rddma_bind_delete, 
	.src_done        = rddma_local_src_done,
	.dst_done        = rddma_local_dst_done,
	.done            = rddma_local_done,
	.src_ready       = rddma_local_src_ready,
	.dst_ready       = rddma_local_dst_ready,
	.dst_events      = rddma_local_dst_events,
	.src_events      = rddma_local_src_events,
};

