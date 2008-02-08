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

/**
* rddma_local_xfer_find - find an rddma_xfer object for a named xfer at the local site.
* @parent : location where xfer officially resides (right here!)
* @desc	  : target xfer parameter descriptor
*
* This function attempts to find an rddma_xfer object for the xfer described by @desc, 
* which officially resides at this site, whose location is formally defined by @parent.
*
* If no such xfer exists, the function will NOT attempt to create one. This conflicts somewhat
* with traditional "find" policy.
*
* The function returns a pointer to the rddma_xfer object that represents the target xfer in the
* local tree, or NULL if nonesuch exists. It is the responsibility of the caller to create an xfer
* in that case.
*
**/
static struct rddma_xfer *rddma_local_xfer_find(struct rddma_location *parent, struct rddma_desc_param *desc)
{
	struct rddma_xfer *xfer = to_rddma_xfer(kset_find_obj(&parent->xfers->kset,desc->name));
	RDDMA_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,parent,desc,xfer);
	return xfer;
}

/**
* rddma_local_bind_find - find a bind belonging to a specified xfer
* @parent : pointer to <xfer> object that bind belongs to
* @desc   : <xfer> specification string
*
* Wha Wha Whaaaaa?... find a bind without naming it? The @parent and @desc arguments tell us 
* nothing about the bind we are supposed to find, except the identity of the xfer object it 
* belongs to. 
*
* Well, No: binds are named for their offset/extent within the xfer. So what we look for
* is a bind named "#<xo>:<xe>". 
*
* This is only going to work if the offset/extent specified for the xfer are NOT its true
* offset and extent (which increase as new binds are added to the xfer) but simply the offset and 
* extent of the damned bind we want to look for. 
*
* In other words, something "smart" (read: smarmy) needs to have composed the xfer string; like
* some other part of the code.
*
**/
static struct rddma_bind *rddma_local_bind_find(struct rddma_xfer *parent, struct rddma_desc_param *desc)
{
	struct rddma_bind *bind = NULL;
	char buf[128];

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	if ( snprintf(buf,128,"#%llx:%x", desc->offset, desc->extent) > 128 )
		goto out;

	bind = to_rddma_bind(kset_find_obj(&parent->binds->kset,buf));

out:
	RDDMA_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,parent,desc,bind);
	return bind;
}

struct rddma_dst *rddma_local_dst_find(struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	struct rddma_dst *dst = NULL;
	char *buf = kzalloc (2048, GFP_KERNEL);
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	if (!buf) goto out;
	
	if (snprintf (buf, 2048, "%s.%s#%llx",
		      desc->dst.name,desc->dst.location,desc->dst.offset
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

/**
* rddma_local_dst_events
*
*
**/
static int rddma_local_dst_events(struct rddma_bind *bind, struct rddma_bind_param *desc)
{
	/* Local destination SMB with a local transfer agent. */
	struct rddma_events *event_list;
	char *event_name;

	event_name = rddma_get_option(&bind->desc.dst,"event_name");

	if (event_name == NULL)
		goto fail;

	event_list = find_rddma_events(rddma_subsys->events,event_name);
	if (event_list == NULL)
		event_list = rddma_events_create(rddma_subsys->events,event_name);

	if (event_list == NULL)
		goto fail;

	bind->dst_done_event = rddma_event_create(event_list,&desc->dst,bind,0,(int)&bind->dst_done_event);

	bind->dst_ready_event = rddma_event_create(event_list,&desc->dst,bind,bind->desc.dst.ops->dst_ready,(int)&bind->dst_ready_event);
						   
	return 0;

fail:
	return -EINVAL;
}

/**
* rddma_local_dsts_create - create bind destinations list, and its subsidiaries.
* @parent : the bind object whose dst components are here being created
* @desc   : full descriptor of the bind being created
*
* This function - which runs on the <dst> site of a bind - creates the list of 
* destination objects for that bind.
*
* Although a bind is defined in terms of a single destination (and source) specification, 
* it may be split into a number of parts to better match Linux page size at dst and src
* sites.
*
* The dst objects are installed in a list - the dsts list - that is a kset bound to the
* parent bind.
*
**/
static struct rddma_dsts *rddma_local_dsts_create(struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	int page, first_page, last_page;
	struct rddma_bind_param params = *desc;
	struct rddma_smb *dsmb = NULL;
	struct rddma_dst *new = NULL;

	RDDMA_DEBUG(MY_DEBUG,"%s parent(%p) desc(%p)\n",__FUNCTION__,parent,desc);

	/*
	* Invoke the <xfer> dst_events op, and check the result.
	* A whole nother can o' worms to be explored, later...
	*
	*/
	if (parent->desc.xfer.ops->dst_events(parent,desc))
		goto fail_events;

	rddma_dsts_create(parent,desc);

	if ( NULL == (dsmb = find_rddma_smb(&desc->dst)) )
		goto fail_dsmb;

	if (!DESC_VALID(&dsmb->desc,&desc->dst))
		goto fail_ddesc;

	/*
	* Decide how best to divvie-up the dst side of the full
	* bind extent. 
	*
	*/
	first_page = START_PAGE(&dsmb->desc,&desc->dst);
	last_page = first_page + NUM_DMA(&dsmb->desc,&desc->dst);

	params.dst.offset = START_OFFSET((&dsmb->desc), (&desc->dst));
	params.src.extent = params.dst.extent = START_SIZE(&dsmb->desc, &desc->dst);
	
	/*
	* And now, for each page of DMA transfers, create one dst object: in effect,
	* a chain of dst objects that collectively cover the bind extent.
	*
	*/
	for (page = first_page; page < last_page; page++) {
		params.dst.offset += virt_to_phys(page_address(dsmb->pages[page]));
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

		/*
		* Whoa - wasn't expecting this... dst_create runs on <xfer>, 
		* not on <dst>?
		*
		*/
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

	rddma_bind_load_dsts(parent);

	if (rddma_debug_level & RDDMA_DBG_DMA_CHAIN)
		rddma_dma_chain_dump(&parent->dma_chain);

	parent->end_of_chain = parent->dma_chain.prev;

	return parent->dsts;

fail_newdst:
fail_ddesc:
	rddma_smb_put(dsmb);
fail_dsmb:
fail_events:
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
/* Start Atomic */
	/*
	* Adjust bind and xfer offset/extent values.
	*
	* Unless otherwise specified, the bind should begin at the
	* current end of the xfer. The xfer extent should then increase
	* by the bind extent.
	*
	* There may be a subtle flaw in this calculation: bind offset
	* may be predefined in the descriptor, in which case it will 
	* not be adjusted and may not be contiguous with the xfer. Not 
	* sure what implications that might carry, if any.
	*/
	if (!desc->xfer.offset)
		desc->xfer.offset = xfer->desc.extent;

	xfer->desc.extent += desc->xfer.extent;
/* End Atomic */

	/*
	* Create and register the bind object, and if that succeeds
	* create its dsts/ subtree.
	*
	*/
	if ( (bind = rddma_bind_create(xfer, desc))) {
		/*
		* Once the bind object has been installed in the sysfs
		* tree, create and register its dsts subtree. Each bind
		* object has a pointer to a dsts kset: that is what we
		* try to create here.
		*
		* We begin by creating the dsts kobject/kset only, and 
		* if that proves successful we try to build it out by
		* populating the dst and src parts of the bind.
		*
		* Now: ask yourself why might we need multiple destination 
		* objects when a bind, by definition, has one destination 
		* component and one source component?
		*
		* The answer is that the complete bind is split into parts, 
		* whose size is a function of the underlying page size.
		* This occurs with the destination component, and with the
		* source component, and allows different page sizes at
		* different locations to interoperate.
		*
		*/
		if ( (dsts = rddma_dsts_create(bind,desc)) ) {
			/*
			* The dsts object is simply a hook, beneath
			* which we want to sling a series of bind
			* destination specs (and beneath each of those, 
			* a series of corresponding bind source specs).
			*
			* Before we do any of that, validate that the
			* destination and source SMBs referenced by the
			* bind exist, and are usable.
			*
			* 1. Destination SMB exists?
			*
			*/
			if ( NULL == (dsmb = find_rddma_smb(&desc->dst)) )
				goto fail_dsmb;
			/*
			* 2. Destination offset+extent fits inside it?
			*/
			if (!DESC_VALID(&dsmb->desc,&desc->dst))
				goto fail_ddesc;
			/*
			* 3. source SMB exists?
			*/
			if ( NULL == (ssmb = find_rddma_smb(&desc->src)) )
				goto fail_ddesc;
			/*
			* 4. Source offset_extent fits inside it?
			*/
			if (!DESC_VALID(&ssmb->desc,&desc->src))
				goto fail_sdesc;

			/*
			* src and dst objects must inherit the various
			* ops (and location) of their bind counterparts.
			*
			*/
			rddma_inherit(&bind->desc.src,&ssmb->desc);
			rddma_inherit(&bind->desc.dst,&dsmb->desc);

			/*
			* Create the dsts for real, and what lies beneath it.
			*
			*/
			if ( NULL == bind->desc.dst.ops->dsts_create(bind,desc))
				goto fail_dst;

			rddma_xfer_load_binds(xfer,bind);

			if (rddma_debug_level & RDDMA_DBG_DMA_CHAIN)
				rddma_dma_chain_dump(&bind->dma_chain);

			return bind;
		}
		RDDMA_DEBUG (MY_DEBUG, "xxx Failed to create bind %s - deleting\n", kobject_name (&bind->kobj));
		rddma_bind_delete(xfer,&desc->xfer);
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

/**
* rddma_local_dst_create
* @parent : parent bind object
* @desc   : bind descriptor
*
* This function creates a <dst> component of a bind, which is typically some fragment of the 
* complete bind extent chosen to suit the page size at the <dst> site.
*
* dst_create is invoked by the <dst> agent as part of dsts_create, but must run on the <xfer> agent.
* This function, then, is specifically written to run on a bind <xfer> agent.
*
*
**/
static struct rddma_dst *rddma_local_dst_create(struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	struct rddma_dst *dst;
	struct rddma_srcs *srcs;

	dst = rddma_dst_create(parent,desc);
	
	if (dst) {
		srcs = parent->desc.src.ops->srcs_create(dst,desc);
		if (!srcs) {
			rddma_dst_put(dst);
			dst = NULL;
		}
	}
	
	return dst;
}


static struct rddma_xfer *rddma_local_xfer_create(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct rddma_xfer *xfer;

	unsigned long extent = desc->extent;
	unsigned long long offset = desc->offset;

	desc->offset = 0;
	desc->extent = 0;

	xfer = rddma_xfer_create(loc,desc);

	desc->offset = offset;
	desc->extent = extent;

	RDDMA_DEBUG(MY_DEBUG,"%s %p %p %p\n",__FUNCTION__,loc,desc,xfer);

	return xfer;
}

static int rddma_local_src_events(struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	/* Local source SMB with a local transfer agent. */
	struct rddma_bind *bind;
	struct rddma_events *event_list;
	char *event_name;

	RDDMA_DEBUG(MY_DEBUG,"%s dst_parent(%p) desc(%p)\n",__FUNCTION__,parent,desc);

	bind = rddma_dst_parent(parent);

	RDDMA_DEBUG(MY_DEBUG,"%s bind(%p)->src_done_event(%p)\n",__FUNCTION__,bind,bind->src_done_event);
	if (bind->src_done_event)
		return 0;

	event_name = rddma_get_option(&desc->src,"event_name");
	
	if (event_name == NULL)
		goto event_name_fail;

	event_list = find_rddma_events(rddma_subsys->events,event_name);
	if (event_list == NULL)
		event_list = rddma_events_create(rddma_subsys->events,event_name);

	if (event_list == NULL)
		goto dones_fail;

	bind->src_done_event = rddma_event_create(event_list,&desc->src,bind,0,(int)parent);

	bind->src_ready_event = rddma_event_create(event_list,&desc->src,bind,bind->desc.src.ops->src_ready,(int)&parent->srcs);
		
	return 0;

dones_fail:
event_name_fail:				   
	return -EINVAL;
}

/**
* rddma_local_srcs_create - create bind <srcs> kset on local tree
* @parent : pointer to bind <dst> to which these <srcs> are to be attached
* @desc   : descriptor of governing bind
*
* This function creates the <srcs> kobject for a bind. It is intended to run, as a
* local function, on the bind <src> node.
*
* <srcs> is a kset of <src> fragments that describe individual DMA transactions
* to be raised to send <dst>-extent bytes from <src> to <dst>-extent bytes on <dst>.
* 
* There is one <srcs> (and perhaps multiple <src>) associated with every <dst> in a
* bind; just as there is one <dsts> (and perhaps multiple <dst>) associated with the
* bind itself.
*
* <dst> fragments are sized to match the page size of the <dst> machine, wherever that 
* might be. The same must happen with <src>: the <dst>-extent to which it is matched
* must be broken down into <src> fragments whose individual size is a function of page 
* size on the <src> machine.
*
* It is for this reason - that <src> fragment size can be calculated correctly - that
* <srcs> is created on the bind <src> machine. A loop is set-up to create <src> fragments
* whose collective size matches the extent of the parent <dst>.
*
* <src> fragments are NOT created here, but at the <xfer> agent. This function causes their
* creation.
* 
*
**/
static struct rddma_srcs *rddma_local_srcs_create(struct rddma_dst *parent, struct rddma_bind_param *desc)
{
/* 	srcs_create://tp.x:2000/d.p#uuuuu000:1000=s.r#c000:1000 */

	int page, first_page, last_page;
	struct rddma_srcs *srcs;
	struct rddma_smb *smb;
	struct rddma_src *src;
	struct rddma_bind_param params = *desc;
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	/*
	* Create the <srcs> kset and install it in the
	* local instance of its parent <dst>. 
	*
	*/
	srcs = rddma_srcs_create(parent,desc);

	if (srcs == NULL)
		return NULL;

	/*
	* Source-related events stuff. Not yet figured what
	* this does...
	*
	*/
	if (parent->desc.xfer.ops->src_events(parent,desc))
		return NULL;

	/*
	* Find the local representation of the <smb> we are
	* pulling data out of. This node we are running on
	* is home to the <smb>, otherwise we wouldn't be here.
	*
	*/
	smb = find_rddma_smb(&desc->src);

	/*
	* Now for page calculations. Calculate page address
	* for the start of the <src> and the number of pages
	* that need to be transferred.
	*/
	first_page = START_PAGE(&smb->desc,&desc->src);
	last_page = first_page + NUM_DMA(&smb->desc,&desc->src);

	params.src.offset = START_OFFSET(&smb->desc, &desc->src);
	params.dst.extent = params.dst.offset;
	params.src.extent = START_SIZE(&smb->desc, &desc->src);
	for ( page = first_page; page < last_page ; page++ ) {
		params.src.offset += virt_to_phys(page_address(smb->pages[page]));
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

		src = parent->desc.xfer.ops->src_create(parent,&params);
		if (!src)
			goto fail_newsrc;
		params.src.offset = 0;
		params.dst.extent += params.src.extent;
		if (page + 2 >= last_page && END_SIZE(&smb->desc,&desc->src))
			params.src.extent = END_SIZE(&smb->desc,&desc->src);
		else
			params.src.extent = PAGE_SIZE;
	} 

	rddma_dst_load_srcs(parent);

	return srcs;

fail_newsrc:
	rddma_smb_put(smb);
	rddma_srcs_delete(srcs);
	return NULL;
}

/**
* rddma_local_src_create - create a bind <src> component
* @parent : bind <dst> with which this <src> is to be associated
* @desc   : descriptor of overall bind
*
* This function creates a single <src> component that is one of the <srcs> associated
* with a specific <dst> in a bind. All <dst>=<src> are broken down into fragments related
* to the page size on the respective <dst> and <src> sites. They are linked together in
* <dsts> and <srcs> lists - where those lists are constructed at the relevant sites.
*
* Individual <src> (and <dst>) fragments are always built by the <xfer> agent, which 
* is where this local function is expected to be running.
*
**/
static struct rddma_src *rddma_local_src_create(struct rddma_dst *parent, struct rddma_bind_param *desc)
{
/* 	src_create://tp.x:2000/d.p#uuuuu000:800=s.r#xxxxx800:800 */
	struct rddma_src *src;
	RDDMA_DEBUG(MY_DEBUG,"%s %s.%s#%llx:%x\n",__FUNCTION__,desc->src.name,desc->src.location, desc->src.offset,desc->src.extent);

	src = rddma_src_create(parent,desc);

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
 * rddma_local_srcs_delete - delete bind <dst> fragment <srcs> and any <src> subsidiaries
 * @parent : bind to which target <srcs> belongs
 * @desc   : bind parameter descriptor
 *
 * This function - which must run local to the <src> node in a <bind> - will delete the
 * <srcs> associated with a specific bind <dst> fragment, and any <src> fragments slung
 * beneath it.
 *
 * To recap: a <bind> comprises a block of some <src> SMB to be transferred to a similar-sized
 * block of a <dst> SMB. The block size - extent of the bind - is divvied-up at the <dst>> site
 * into <dst> fragments that match the <dst> page size. Each <dst> fragment has an associated
 * set of <src> fragments whose quantity, and individual extents, are a reflection of <src>
 * page size. The <src> fragments associated with a particular <dst> fragment are collated
 * under a <srcs> object at the <src> location (although the individual <src> fragments are
 * "owned" by the bind <xfer> agent).
 *
 * So: since <srcs> is owned by the <src> agent, that is where this function must run, even
 * though the actual <src> fragments are owned by the <xfer> agent.
 *
 * The function works by running through the list of <src> fragments in <srcs>, deleting
 * each in turn. It then "deletes" the <srcs> kset itself, by means of a put on its refcount.
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
				src = to_rddma_src (to_kobj (entry));
				if (src && src->desc.xfer.ops->src_delete) {
					src->desc.xfer.ops->src_delete (parent, &src->desc);
				}
			}
		}
		RDDMA_DEBUG (MY_LIFE_DEBUG, "-- Srcs Count: %lx\n", (unsigned long)srcs->kset.kobj.kref.refcount.counter);
		rddma_srcs_delete (srcs);
	}
}

/**
* rddma_local_src_delete - Delete bind <src> fragment
* 
*
*
**/
static void rddma_local_src_delete (struct rddma_dst *parent, struct rddma_bind_param *desc)
{	
	RDDMA_DEBUG (MY_DEBUG, "%s (%s.%s#%llx:%x/%s.%s#%llx:%x=<*>)\n", __func__, 
	             desc->xfer.name, desc->xfer.location, desc->xfer.offset, desc->xfer.extent, 
	             desc->dst.name, desc->dst.location, desc->dst.offset, desc->dst.extent);
	printk ("-- Parent bind: %s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x\n", 
	        parent->desc.xfer.name, parent->desc.xfer.location, parent->desc.xfer.offset, parent->desc.xfer.extent, 
	        parent->desc.dst.name, parent->desc.dst.location, parent->desc.dst.offset, parent->desc.dst.extent, 
	        parent->desc.src.name, parent->desc.src.location, parent->desc.src.offset, parent->desc.src.extent);
	
	rddma_src_delete (parent, desc);
}


/**
* rddma_local_dsts_delete - Delete bind destinations
* @parent : pointer to bind object being deleted
* @desc   : descriptor of bind being deleted
*
* This function runs, locally, on the <dst> agent of a bind that is being deleted.
* The <dst> agent is the keeper of the <dsts> list of individual <dst> fragments 
* that make up the parent bind.
*
* The function is run as an integral part of a bind_delete operation being co-ordinated
* by the bind <xfer> agent.
*
* <dsts> is a list of <dst> fragments - each being "owned" by the bind <xfer> agent - that
* divide the bind extent into <dst> page-sized fragments. <dsts> is deleted by first issuing
* dst_delete requests for the <dst> fragments it carries.
*
**/
static struct rddma_bind *rddma_local_dsts_delete (struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	struct list_head *entry, *safety;
	struct rddma_dsts *dsts = parent->dsts;
	
	RDDMA_DEBUG (MY_DEBUG,"%s for %s.%s#%llx:%x\n",__FUNCTION__, 
		     parent->desc.xfer.name, parent->desc.xfer.location,parent->desc.xfer.offset, parent->desc.xfer.extent);
	if (dsts) {
		RDDMA_DEBUG (MY_DEBUG, "-- Dsts \"%s\"\n", kobject_name (&dsts->kset.kobj));
		if (!list_empty (&dsts->kset.list)) {
			list_for_each_safe (entry, safety, &dsts->kset.list) {
				struct rddma_dst      	*dst;
				dst = to_rddma_dst (to_kobj (entry));
				dst->desc.xfer.ops->dst_delete (parent, &dst->desc);
				rddma_dst_delete(parent,&dst->desc);
			}
		}
		
		RDDMA_DEBUG (MY_LIFE_DEBUG, "-- Dsts Count: %lx\n", (unsigned long)dsts->kset.kobj.kref.refcount.counter);
		rddma_dsts_delete (dsts);
		
		/*
		* HACK ALERT:
		* -----------
		* If the <dsts> site is not the same as the <xfer> site then the bind
		* will have been created on-the-fly during dsts_create, and will have
		* an imbalanced refcount that will not disappear when <dsts> unravels.
		*
		* So: compare the <dst> and <xfer> ops pointers to determine whether 
		* they run on the same or on different sites. And if different, unregister
		* the bind.
		*
		* Also take care of dubious refcounting: bind count needs to be 2, at least.
		* Bump it up articifially if necessary. THIS IS A TEMPORARY HACK-OF-HACK.
		*
		*/
		if (parent->desc.xfer.ops != parent->desc.dst.ops) {
			int bindref = atomic_read (&parent->kobj.kref.refcount);
			if (bindref < 2) {
				printk ("-- Whoopsie - bindref %d too small for \"%s\"\n", bindref, kobject_name (&parent->kobj));
				kobject_get (&parent->kobj);
			}
			else {
				printk ("-- Bindref is now at %d\n", bindref);
			}
			rddma_bind_unregister (parent);
			parent = NULL;
		}
	}
	return (parent);
}

/**
* rddma_local_dst_delete - Delete bind <dst> fragment and its <srcs> subsidiaries
* @parent : pointer to parent bind object
* @desc   : bind parameter descriptor
*
* This function - which runs locally at a bind <xfer> site - will cause the deletion
* of a bind <dst> fragment and its <srcs>/<src> subsidiaries.
*
* This function is invoked as an integral part of a higher-level bind_delete operation 
* as a consequence of dsts_delete, which runs on the bind <dst> site.  
*
*
**/
static void rddma_local_dst_delete(struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	struct rddma_dst *dst;

	RDDMA_DEBUG (MY_DEBUG, "%s (%s.%s#%llx:%x/%s.%s#%llx:%x=<*>)\n", __func__, 
		desc->xfer.name, desc->xfer.location, desc->xfer.offset, desc->xfer.extent, 
		desc->dst.name, desc->dst.location, desc->dst.offset, desc->dst.extent);

	dst = find_rddma_dst_in(parent,desc);
	parent->desc.src.ops->srcs_delete(dst,desc);
	rddma_dst_delete(parent,desc);
}

static void rddma_local_done(struct rddma_event *event)
{
	/* A DMA engine, either local or remote, has completed a
	 * transfer involving a local smb as the source or
	 * destination. Do vote adjustment. */
	struct rddma_events *e = to_rddma_events(event->kobj.parent);

	e->count--;

	if (e->count == 0)
		complete(&e->dma_sync);

	RDDMA_DEBUG(MY_DEBUG,"%s event(%p) events(%p) count(%d)\n",__FUNCTION__,event,e,e->count);
}

static void rddma_local_src_done(struct rddma_bind *bind)
{
	/* A DMA engine, either local or remote, has completed a
	 * transfer involving a local smb as the source. Do vote
	 * adjustment. */
	RDDMA_DEBUG(MY_DEBUG,"%s bind(%p)\n",__FUNCTION__,bind);
	rddma_bind_src_done(bind);
	bind->desc.src.ops->done(bind->src_done_event);
}

static void rddma_local_dst_done(struct rddma_bind *bind)
{
	/* A DMA engine, either local or remote, has completed a
	 * transfer involving a local SMB as the destination. Do vote
	 * ajustment accordingly */
	RDDMA_DEBUG(MY_DEBUG,"%s bind(%p)\n",__FUNCTION__,bind);
	rddma_bind_dst_done(bind);
	bind->desc.dst.ops->done(bind->dst_done_event);
}

static void rddma_local_src_ready(struct rddma_bind *bind)
{
	/* Someone, either locally or via the fabric, executed start
	 * on an event which is telling us, the local DMA engine
	 * assigned for this bind, that the source SMB, which may be
	 * local or remote, is ready for action. */
	RDDMA_DEBUG(MY_DEBUG,"%s bind(%p)\n",__FUNCTION__,bind);
	if (rddma_bind_src_ready(bind))
		bind->desc.xfer.rde->ops->queue_transfer(&bind->descriptor);
	
}

static void rddma_local_dst_ready(struct rddma_bind *bind)
{
	/* Someone, either locally or via the fabric, executed start on
	 * an event which is telling us, the local DMA engine assigned
	 * for this bind, that the destination SMB, which may be local
	 * or remote, is ready for action. */
	RDDMA_DEBUG(MY_DEBUG,"%s bind(%p)\n",__FUNCTION__,bind);
	if (rddma_bind_dst_ready(bind))
		bind->desc.xfer.rde->ops->queue_transfer(&bind->descriptor);
}

static int rddma_local_event_start(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct rddma_events *event_list;
	event_list = find_rddma_events(rddma_subsys->events,desc->name);
	if (event_list == NULL )
		return -EINVAL;
	rddma_events_start(event_list);
	return 0;
}

/**
* rddma_local_bind_delete - delete a bind associated with an xfer
* @parent : pointer to the <xfer> that the target <bind> belongs to.
* @desc   : descriptor for <xfer>. Its embedded offset is the primary means of bind selection.
*
* This function - which runs ONLY on the Xfer agent associated with the target bind - will
* delete the specified bind and all of its subsidiaries.
*
* 
**/
static void rddma_local_bind_delete(struct rddma_xfer *parent, struct rddma_desc_param *desc)
{
	struct list_head *entry, *safety;
	RDDMA_DEBUG (MY_DEBUG, "%s: (%s.%s#%llx:%x)\n", __func__, 
			desc->name, desc->location, desc->offset, desc->extent);
	/*
	* Every <xfer> has a <binds> kset that lists the various binds 
	* that exist for this transfer. Run through that list and delete
	* any binds whose offset lies within the offset/extent range of the
	* delete specification.
	*/
	if (!list_empty(&parent->binds->kset.list)) {
		list_for_each_safe(entry,safety,&parent->binds->kset.list) {
			struct rddma_bind *bind;
			bind = to_rddma_bind(to_kobj(entry));
			if (bind->desc.xfer.offset >= desc->offset &&
			    bind->desc.xfer.offset <= desc->offset + desc->extent) {
				bind->desc.dst.ops->dsts_delete(bind,&bind->desc);
			}
			rddma_bind_delete (parent, &bind->desc.xfer);
		}
	}
}

static void rddma_local_xfer_delete(struct rddma_location *parent, struct rddma_desc_param *desc)
{
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
	.xfer_delete     = rddma_local_xfer_delete,
	.xfer_find       = rddma_local_xfer_find,
	.srcs_create     = rddma_local_srcs_create,
	.srcs_delete     = rddma_local_srcs_delete,
	.src_create      = rddma_local_src_create,
	.src_delete      = rddma_local_src_delete,
	.src_find        = rddma_local_src_find,
	.dsts_create     = rddma_local_dsts_create,
	.dsts_delete     = rddma_local_dsts_delete, 
	.dst_create      = rddma_local_dst_create,
	.dst_delete      = rddma_local_dst_delete,
	.dst_find        = rddma_local_dst_find,
	.bind_find       = rddma_local_bind_find,
	.bind_create     = rddma_local_bind_create,
	.bind_delete     = rddma_local_bind_delete, 
	.src_done        = rddma_local_src_done,
	.dst_done        = rddma_local_dst_done,
	.done            = rddma_local_done,
	.src_ready       = rddma_local_src_ready,
	.dst_ready       = rddma_local_dst_ready,
	.dst_events      = rddma_local_dst_events,
	.src_events      = rddma_local_src_events,
	.event_start     = rddma_local_event_start,
};

EXPORT_SYMBOL (rddma_local_ops);

