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

static struct rddma_dsts *rddma_local_dsts_create(struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	int page, first_page, last_page;
	struct rddma_bind_param params = *desc;
	struct rddma_smb *dsmb = NULL;
	struct rddma_dst *new = NULL;

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
		if ( (dsts = rddma_dsts_create(bind,desc,"%s.%s#%llx:%x=%s.%s#%llx:%x",
						     desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,
						     desc->src.name,desc->src.location,desc->src.offset,desc->src.extent)) ) {
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

	xfer = rddma_xfer_create(loc,desc);

	RDDMA_DEBUG(MY_DEBUG,"%s %p %p %p\n",__FUNCTION__,loc,desc,xfer);

	bind = rddma_local_bind_create(xfer,desc);
	
	return xfer;
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

static int rddma_local_xfer_start(struct rddma_location *loc, struct rddma_bind_param *desc)
{
	struct rddma_xfer *xfer;
	int ret = -EINVAL;
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
	xfer = rddma_local_xfer_find(loc,desc);
	if (xfer) {
		kobject_put(&xfer->kobj);
		rddma_xfer_start(xfer);
		ret = 0;
	}
	return ret;
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

/*
*     X F E R   S T A R T   O P E R A T I O N S
*    -------------------------------------------
*
*/


/**
* rddma_local_bind_dst_ready - signal xfer agent that bind destination is ready for transfer
*
* @xfer: the transfer to which the bind belongs
* @desc: description of the bind
*
* This function plays a role in xfer_start by signaling the xfer agent, who manages the
* transfer, that the destination buffer of a specific bind is now ready for data transfer.
* The xfer agent will be able to execute the bind once both its source and its destination
* buffers have signalled their readiness.
*
* Special Constraints
* -------------------
* Because a bind requires both its source buffer and its destination buffer to signal
* readiness separately, and because in most cases the source and destinations will live
* at different locations, this particular operation should ONLY be available locally - with
* no fabric counterpart. 
*
* Temporary Solution
* ------------------
* This function will one day buzz a RIO doorbell to signal the xfer agent, but until fabric
* functionality becomes available we use a temporary call hack to signal the xfer agent.
*
**/
static void rddma_local_bind_dst_ready (struct rddma_xfer *xfer, struct rddma_bind_param *desc)
{
	RDDMA_DEBUG (MY_DEBUG, "## %s for %s#%llx:%x/[ %s#%llx:%x ]=%s#%llx:%x\n", 
		     __FUNCTION__, 
		     desc->xfer.name, desc->xfer.offset, desc->xfer.extent, 
		     desc->dst.name, desc->dst.offset, desc->dst.extent, 
		     desc->src.name, desc->src.offset, desc->src.extent);
	if (xfer->desc.xfer.ops && xfer->desc.xfer.ops->bind_vote) {
		xfer->desc.xfer.ops->bind_vote (xfer, desc, 1, 0);
	}
	else {
		RDDMA_DEBUG (MY_DEBUG, "xx No xfer::bind_vote() operation!\n");
	}
}

/**
 * rddma_local_bind_src_ready - signal xfer agent that bind source is ready for transfer
 *
 * @xfer: the transfer to which the bind belongs
 * @desc: description of the bind
 *
 * This function plays a role in xfer_start by signaling the xfer agent, who manages the
 * transfer, that the source buffer of a specific bind is now ready for data transfer.
 * The xfer agent will be able to execute the bind once both its source and its destination
 * buffers have signalled their readiness.
 *
 * Special Constraints
 * -------------------
 * Because a bind requires both its source buffer and its destination buffer to signal
 * readiness separately, and because in most cases the source and destinations will live
 * at different locations, this particular operation should ONLY be available locally - with
 * no fabric counterpart. 
 *
 * Temporary Solution
 * ------------------
 * This function will one day buzz a RIO doorbell to signal the xfer agent, but until fabric
 * functionality becomes available we use a temporary call hack to signal the xfer agent.
 *
 **/
static void rddma_local_bind_src_ready (struct rddma_xfer *xfer, struct rddma_bind_param *desc)
{
	RDDMA_DEBUG (MY_DEBUG, "## %s for %s#%llx:%x/%s#%llx:%x=[ %s#%llx:%x ]\n", 
		     __FUNCTION__, 
		     desc->xfer.name, desc->xfer.offset, desc->xfer.extent, 
		     desc->dst.name, desc->dst.offset, desc->dst.extent, 
		     desc->src.name, desc->src.offset, desc->src.extent);
	if (xfer->desc.xfer.ops && xfer->desc.xfer.ops->bind_vote) {
		xfer->desc.xfer.ops->bind_vote (xfer, desc, 0, 1);
	}
	else {
		RDDMA_DEBUG (MY_DEBUG, "xx No xfer::bind_vote() operation!\n");
	}
}

/**
* rddma_local_bind_vote - add votes to bind towards xfer_start readiness
*
* @xfer:	Parent xfer 
* @desc:	Bind descriptor
* @d_votes:	Number of destination votes to add
* @s_votes:	Number of source votes to add
*
* This function is that part of the xfer_start suite that adds source and destination
* votes to a bind in order to adjust its update for xfer_start.
*
* This implementation of the function - the local version - is intended to run on
* the xfer agent, where xfer_start is ultimately controlled.
*
* The function works by finding the bind object in the local tree, then adding the
* given source and destination votes to its internal tallies. Once both source and
* destination are ready, a further vote will be submitted to the parent xfer to show
* that one more bind is now ready. And once the xfer has a vote from all of its binds, 
* xfer_start can finally execute.
*
**/
void rddma_local_bind_vote (struct rddma_xfer *xfer, struct rddma_bind_param *desc, int d_votes, int s_votes)
{
	struct rddma_bind *bind;
	int d, s;
#ifdef PARALLELIZE_BIND_PROCESSING
	struct list_head *entry;
#endif
	
	RDDMA_DEBUG (MY_DEBUG, "## %s for %s#%llx:%x/%s#%llx:%x[%+d]=%s#%llx:%x[%+d]\n", 
		     __FUNCTION__, 
		     desc->xfer.name, desc->xfer.offset, desc->xfer.extent, 
		     desc->dst.name, desc->dst.offset, desc->dst.extent, d_votes, 
		     desc->src.name, desc->src.offset, desc->src.extent, s_votes);
	if (!(bind = rddma_local_bind_find (xfer, desc))) {
		RDDMA_DEBUG (MY_DEBUG, "xx Unable to locate the bind!\n");
		return;
	}
	
	if (!d_votes && !s_votes) {
		return;
	}
	d = (d_votes) ? atomic_inc_return (&bind->dst_votes) : atomic_read (&bind->dst_votes);
	s = (s_votes) ? atomic_inc_return (&bind->src_votes) : atomic_read (&bind->src_votes);
	if (d == 1 && s == 1) {
		int x = atomic_inc_return (&xfer->start_votes);
		if (x == atomic_read (&xfer->bind_count)) {
			RDDMA_DEBUG (MY_DEBUG, ">>>>> BLAM BLAM BLAM BLAM! >>>>>\n");
#ifdef SERIALIZE_BIND_PROCESSING
			xfer->desc.xfer.rde->ops->load_transfer(xfer);
			xfer->desc.xfer.rde->ops->queue_transfer(&xfer->descriptor);
#else
			/* Loop over binds */
			list_for_each(entry,&xfer->binds->kset.list) {
				bind = to_rddma_bind(to_kobj(entry));
				xfer->desc.rde->ops->queue_transfer(&bind->descriptor);
			}
#endif
			return;
		}
		else {
			RDDMA_DEBUG (MY_DEBUG, "Xfer ---> %d of %d\n", x, atomic_read (&xfer->bind_count));
		}
	}
	else {
		RDDMA_DEBUG (MY_DEBUG, "Bind --> [d:%d s:%d]\n", d, s);
	}
}


struct rddma_ops rddma_local_ops = {
	.location_create = rddma_local_location_create,
	.location_delete = rddma_local_location_delete,
	.location_find = rddma_local_location_find,
	.location_put = rddma_local_location_put,
	.smb_create = rddma_local_smb_create,
	.smb_delete = rddma_local_smb_delete,
	.smb_find = rddma_local_smb_find,
	.mmap_create = rddma_local_mmap_create,
	.mmap_delete = rddma_local_mmap_delete,
	.xfer_create = rddma_local_xfer_create,
	.xfer_delete = rddma_xfer_delete,
	.xfer_start = rddma_local_xfer_start,
	.xfer_find = rddma_local_xfer_find,
	.srcs_create = rddma_local_srcs_create,
	.src_create = rddma_local_src_create,
	.src_delete = rddma_src_delete,
	.src_find = rddma_local_src_find,
	.dsts_create = rddma_local_dsts_create,
	.dsts_delete = rddma_local_dsts_delete, 
	.dst_create = rddma_local_dst_create,
	.dst_delete = rddma_dst_delete,
	.dst_find = rddma_local_dst_find,
	.bind_find = rddma_local_bind_find,
	.bind_create = rddma_local_bind_create,
	.bind_delete = rddma_bind_delete, 
	.bind_dst_ready = rddma_local_bind_dst_ready, 
	.bind_src_ready = rddma_local_bind_src_ready, 
	.bind_vote = rddma_local_bind_vote, 
};

