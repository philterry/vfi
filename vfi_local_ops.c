/* 
 * 
 * Copyright 2008 Vmetro
 * Phil Terry <pterry@vmetro.com> 
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#define MY_DEBUG      VFI_DBG_LOCOPS | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_LOCOPS | VFI_DBG_LIFE    | VFI_DBG_DEBUG
#define MY_ERROR      VFI_DBG_LOCOPS | VFI_DBG_ERROR   | VFI_DBG_ERR

#include <linux/vfi_location.h>
#include <linux/vfi_ops.h>
#include <linux/vfi_smb.h>
#include <linux/vfi_dst.h>
#include <linux/vfi_src.h>
#include <linux/vfi_xfer.h>
#include <linux/vfi_bind.h>
#include <linux/vfi_binds.h>
#include <linux/vfi_smbs.h>
#include <linux/vfi_xfers.h>
#include <linux/vfi_srcs.h>
#include <linux/vfi_dsts.h>
#include <linux/vfi_parse.h>
#include <linux/vfi_drv.h>
#include <linux/vfi_mmap.h>
#include <linux/vfi_events.h>
#include <linux/vfi_event.h>
#include <linux/vfi_sync.h>
#include <linux/vfi_syncs.h>

#include <linux/wait.h>
#include <linux/device.h>
#include <linux/mm.h>

extern void vfi_dma_chain_dump(struct list_head *h);
/*
 * F I N D    O P E R A T I O N S
 */
static int vfi_local_location_find(struct vfi_location **newloc,struct vfi_location *loc, struct vfi_desc_param *desc)
{
	VFI_DEBUG(MY_DEBUG,"%s %p %s %p %s,%s\n",__FUNCTION__,loc,loc->desc.name,desc,desc->name,desc->location);
	*newloc  = to_vfi_location(kset_find_obj(&loc->kset,desc->name));
	VFI_DEBUG(MY_DEBUG,"%s %p %s %p %s ->%p\n",__FUNCTION__,loc,loc->desc.name,desc,desc->name,*newloc);
	return VFI_RESULT(*newloc == NULL);
}

static void vfi_local_location_put(struct vfi_location *loc, struct vfi_desc_param *desc)
{
	struct vfi_location *newloc;
	VFI_DEBUG(MY_DEBUG,"%s %p %p %s\n",__FUNCTION__,loc,desc,desc->name);
	newloc = to_vfi_location(kset_find_obj(&loc->kset,desc->name));
	VFI_DEBUG(MY_DEBUG,"%s %p %p %s ->%p\n",__FUNCTION__,loc,desc,desc->name,newloc);
	vfi_location_put(newloc);
	vfi_location_put(newloc);
}

static int vfi_local_smb_find(struct vfi_smb **smb, struct vfi_location *parent, struct vfi_desc_param *desc)
{
	*smb = to_vfi_smb(kset_find_obj(&parent->smbs->kset,desc->name));
	VFI_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,parent,desc,*smb);
	if (*smb == NULL)
		return VFI_RESULT(-EINVAL);
	return VFI_RESULT(0);
}

/**
* vfi_local_xfer_find - find an vfi_xfer object for a named xfer at the local site.
* @parent : location where xfer officially resides (right here!)
* @desc	  : target xfer parameter descriptor
*
* This function attempts to find an vfi_xfer object for the xfer described by @desc, 
* which officially resides at this site, whose location is formally defined by @parent.
*
* If no such xfer exists, the function will NOT attempt to create one. This conflicts somewhat
* with traditional "find" policy.
*
* The function returns a pointer to the vfi_xfer object that represents the target xfer in the
* local tree, or NULL if nonesuch exists. It is the responsibility of the caller to create an xfer
* in that case.
*
**/
static int vfi_local_xfer_find(struct vfi_xfer **xfer, struct vfi_location *parent, struct vfi_desc_param *desc)
{
	*xfer = to_vfi_xfer(kset_find_obj(&parent->xfers->kset,desc->name));
	VFI_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,parent,desc,*xfer);
	return VFI_RESULT(*xfer == NULL);
}

/**
* vfi_local_sync_find - find an vfi_sync object for a named sync at the local site.
* @parent : location where sync officially resides (right here!)
* @desc	  : target sync parameter descriptor
*
* This function attempts to find an vfi_sync object for the sync described by @desc, 
* which officially resides at this site, whose location is formally defined by @parent.
*
* If no such sync exists, the function will NOT attempt to create one. This conflicts somewhat
* with traditional "find" policy.
*
* The function returns a pointer to the vfi_sync object that represents the target sync in the
* local tree, or NULL if nonesuch exists. It is the responsibility of the caller to create an sync
* in that case.
*
**/
static int vfi_local_sync_find(struct vfi_sync **sync, struct vfi_location *parent, struct vfi_desc_param *desc)
{
	*sync = to_vfi_sync(kset_find_obj(&parent->syncs->kset,desc->name));
	VFI_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,parent,desc,*sync);
	return VFI_RESULT(*sync == NULL);
}

/**
* vfi_local_bind_find - find a bind belonging to a specified xfer
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
static int vfi_local_bind_find(struct vfi_bind **bind, struct vfi_xfer *parent, struct vfi_desc_param *desc)
{
	char buf[128];

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	*bind = NULL;

	if ( snprintf(buf,128,"#%llx:%x", desc->offset, desc->extent) > 128 )
		goto out;

	*bind = to_vfi_bind(kset_find_obj(&parent->binds->kset,buf));

out:
	VFI_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,parent,desc,*bind);
	return VFI_RESULT(*bind == NULL);
}

int vfi_local_dst_find(struct vfi_dst **dst, struct vfi_bind *parent, struct vfi_bind_param *desc)
{
	char *buf = kzalloc (2048, GFP_KERNEL);
	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	*dst = NULL;

	if (!buf) goto out;
	
	if (snprintf (buf, 2048, "%s.%s#%llx",
		      desc->dst.name,desc->dst.location,desc->dst.offset
		    ) >= 2048) {
		goto fail_printf;
	}
	
	*dst = to_vfi_dst (kset_find_obj (&parent->dsts->kset, buf));
	
fail_printf:
	kfree (buf);
out:
	VFI_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,parent,desc,*dst);
	return VFI_RESULT(*dst == NULL);
}

static int vfi_local_src_find(struct vfi_src **src, struct vfi_dst *parent, struct vfi_bind_param *desc)
{
	char *buf = kzalloc (2048, GFP_KERNEL);
	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	*src = NULL;

	if (!buf) goto out;
	
	if (snprintf (buf, 2048, "#%llx:%x", desc->src.offset, desc->src.extent) >= 2048) {
		goto fail_printf;
	}
	
	*src = to_vfi_src (kset_find_obj (&parent->srcs->kset, buf));
	
fail_printf:
	kfree (buf);
out:
	VFI_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,parent,desc,*src);
	return VFI_RESULT(*src == NULL);
}

/*
 * C R E A T E     O P E R A T I O N S
 */
static int vfi_local_location_create(struct vfi_location **newloc,struct vfi_location *loc, struct vfi_desc_param *desc)
{
	int ret;
	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	ret = vfi_location_create(newloc,loc,desc);

	/* 
	   If the parent location has a extent of 0 we need to set that here.
	   This can and will happen if the parent is a public root location. 
	*/
	if (loc && !(loc->desc.extent))
		loc->desc.extent = (*newloc)->desc.extent;

	if (*newloc && (*newloc)->desc.address)
		(*newloc)->desc.address->ops->register_location(*newloc);

	VFI_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,loc,desc,*newloc);

	return VFI_RESULT(ret);
}

static int vfi_local_smb_create(struct vfi_smb **newsmb, struct vfi_location *loc, struct vfi_desc_param *desc)
{
	int ret;
	struct vfi_smb *smb;

	*newsmb = NULL;

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	ret = vfi_smb_create(&smb, loc, desc);

	if (smb == NULL)
		return VFI_RESULT(-EINVAL);

	*newsmb = smb;

	/* Allocate memory for this SMB */
	if (vfi_alloc_pages(smb->size, smb->desc.offset, &smb->pages, 
		&smb->num_pages) == 0)
		return VFI_RESULT(0);

	/* Failed */
	vfi_smb_delete(smb);
	*newsmb = NULL;
	return VFI_RESULT(-ENOMEM);
}

static int vfi_local_mmap_create(struct vfi_mmap **mmap, struct vfi_smb *smb, struct vfi_desc_param *desc)
{
	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	return VFI_RESULT(vfi_mmap_create(mmap,smb,desc));
}

/**
* vfi_local_dst_events - create events for bind <dst>
* @bind : bind object
* @desc : bind descriptor
*
* This function creates two <dst> events - dst_ready and dst_done - when <dst> and
* <xfer> occupy the same network location.
*
* There is no need, in this case, to use fabric doorbells to deliver signals: all we
* need do is create two pure event objects with rigged identifiers, bound where necessary
* to "start" functions that are aware of that.
*
* The dst_ready event is sent from <xfer> to <dst> to trigger a bind transfer action.
* It uses the <dst>->dst_ready() op to deliver the signal. We specify <dst> here to show
* intent, although in practise - since <dst> and <xfer> are co-located and local - it
* always boils down to vfi_local_dst_ready().
*
* The dst_done event does not have a "start" op. That is because the VFI DMA layer knows
* how to deliver the event - all it needs is the event pointer to do so.
*
**/
static int vfi_local_dst_events(struct vfi_bind *bind, struct vfi_bind_param *desc)
{
	/* Local destination SMB with a local transfer agent. */
	struct vfi_events *event_list;
	char *event_name;
	int ret;

	event_name = vfi_get_option(&bind->desc.dst,"event_name");

	if (event_name == NULL)
		goto fail;

	ret = find_vfi_events(&event_list,vfi_subsys->events,event_name);
	if (event_list == NULL)
		ret = vfi_events_create(&event_list,vfi_subsys->events,event_name);

	if (event_list == NULL)
		goto fail;

	ret = vfi_event_create(&bind->dst_done_event,event_list,&desc->dst,bind,0,(int)&bind->dst_done_event);

	ret = vfi_event_create(&bind->dst_ready_event,event_list,&desc->dst,bind,bind->desc.dst.ops->dst_ready,(int)&bind->dst_ready_event);
						   
	return VFI_RESULT(0);

fail:
	return VFI_RESULT(-EINVAL);
}

/**
* vfi_local_dst_ev_delete - delete <dst> events
* @bind : bind object that events belong to
* @desc : bind descriptor
*
* This function compliments vfi_local_dst_events() by deleting the two <dst> events that
* function creates. Pointers to dst_ready and dst_done events are required to be present in @bind.
*
* This "local" function will run when <dst> and <xfer> share the same location.
*
**/
static void vfi_local_dst_ev_delete (struct vfi_bind *bind, struct vfi_bind_param *desc)
{
	VFI_DEBUG (MY_DEBUG, "%s: for %s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x\n", __func__,
	             desc->xfer.name, desc->xfer.location, desc->xfer.offset, desc->xfer.extent,
	             desc->dst.name, desc->dst.location, desc->dst.offset, desc->dst.extent,
	             desc->src.name, desc->src.location, desc->src.offset, desc->src.extent);
	VFI_DEBUG (MY_DEBUG, "dst_ready (%p, %08x), dst_done (%p, %08x)\n",
	             bind->dst_ready_event, bind->dst_ready_event_id,
	             bind->dst_done_event, bind->dst_done_event_id);
	vfi_event_delete (bind->dst_ready_event);
	vfi_event_delete (bind->dst_done_event);
	bind->dst_ready_event = bind->dst_done_event = NULL;
	bind->dst_ready_event_id = bind->dst_done_event_id = 0;
}

/**
* vfi_local_dsts_create - create bind destinations list, and its subsidiaries.
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
static int vfi_local_dsts_create(struct vfi_dsts **dsts, struct vfi_bind *parent, struct vfi_bind_param *desc)
{
	int page, first_page, last_page;
	struct vfi_bind_param params = *desc;
	struct vfi_smb *dsmb = NULL;
	struct vfi_dst *new = NULL;
	int ret;

	VFI_DEBUG(MY_DEBUG,"%s parent(%p) desc(%p)\n",__FUNCTION__,parent,desc);

	/*
	* Invoke the <xfer> dst_events op, and check the result.
	* A whole nother can o' worms to be explored, later...
	*
	*/
	if (parent->desc.xfer.ops->dst_events(parent,desc))
		goto fail_events;

	ret = vfi_dsts_create(dsts,parent,desc);

	ret = find_vfi_smb(&dsmb,&desc->dst);
	if ( NULL == dsmb )
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

		ret = parent->desc.xfer.ops->dst_create(&new,parent,&params);
		if (NULL == new)
			goto fail_newdst;

		params.dst.offset = 0;
		params.src.offset += params.src.extent;
		if ( page + 2 == last_page && END_SIZE(&dsmb->desc,&desc->dst))
			params.dst.extent = params.src.extent = END_SIZE(&dsmb->desc,&desc->dst);
		else
			params.dst.extent = params.src.extent = PAGE_SIZE;
	} 

	vfi_bind_load_dsts(parent);

#ifdef CONFIG_VFI_DEBUG
	if (vfi_debug_level & VFI_DBG_DMA_CHAIN)
		vfi_dma_chain_dump(&parent->dma_chain);
#endif

	parent->end_of_chain = parent->dma_chain.prev;

	return VFI_RESULT(parent->dsts == NULL);

fail_newdst:
fail_ddesc:
	vfi_smb_put(dsmb);
fail_dsmb:
fail_events:
	return VFI_RESULT(-EINVAL);
}

/**
* vfi_local_bind_create - create a bind within an xfer
* @xfer:
* @desc:
*
* This function creates a new bind attached to the local instance 
* of a parent transfer (@xfer), and creates and populates the bind's 
* descriptor subtree.
*
**/
static int vfi_local_bind_create(struct vfi_bind **bind, struct vfi_xfer *xfer, struct vfi_bind_param *desc)
{
	struct vfi_dsts *dsts;
	struct vfi_dsts *rdsts;
	struct vfi_smb *ssmb = NULL;
	struct vfi_smb *dsmb = NULL;
	int ret;
	VFI_DEBUG (MY_DEBUG,"%s: %s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x\n",
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
	if ( !(ret = vfi_bind_create(bind,xfer, desc))) {
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
		if ( !(ret = vfi_dsts_create(&dsts,*bind,desc)) ) {
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
			ret = find_vfi_smb(&dsmb,&desc->dst);
			if ( NULL == dsmb )
				goto fail_dsmb;
			/*
			* 2. Destination offset+extent fits inside it?
			*/
			if (!DESC_VALID(&dsmb->desc,&desc->dst))
				goto fail_ddesc;
			/*
			* 3. source SMB exists?
			*/
			ret = find_vfi_smb(&ssmb,&desc->src);
			if ( NULL == ssmb)
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
			vfi_inherit(&(*bind)->desc.src,&ssmb->desc);
			vfi_inherit(&(*bind)->desc.dst,&dsmb->desc);

			/*
			* Create the dsts for real, and what lies beneath it.
			*
			*/
			ret = (*bind)->desc.dst.ops->dsts_create(&rdsts,*bind,desc);
			if ( NULL == rdsts)
				goto fail_dst;

			vfi_xfer_load_binds(xfer,*bind);

#ifdef CONFIG_VFI_DEBUG
			if (vfi_debug_level & VFI_DBG_DMA_CHAIN)
				vfi_dma_chain_dump(&(*bind)->dma_chain);
#endif

			return VFI_RESULT(0);
		}
		VFI_DEBUG (MY_DEBUG, "xxx Failed to create bind %s - deleting\n", kobject_name (&(*bind)->kobj));
		vfi_bind_delete(xfer,&desc->xfer);
		*bind = NULL;
		return VFI_RESULT(ret);
	}
		
	return VFI_RESULT(ret);

fail_dst:
fail_sdesc:
	vfi_smb_put(ssmb);
fail_ddesc:
	vfi_smb_put(dsmb);
fail_dsmb:
	vfi_dsts_delete(dsts);
	if (!ret)
		ret = -ENOMEM;
	return VFI_RESULT(ret);
}

/**
* vfi_local_dst_create
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
static int vfi_local_dst_create(struct vfi_dst **dst, struct vfi_bind *parent, struct vfi_bind_param *desc)
{
	struct vfi_srcs *srcs;
	int ret;

	ret = vfi_dst_create(dst,parent,desc);

	if (ret)
		return VFI_RESULT(ret);

	if (*dst == NULL)
		return VFI_RESULT(-ENOMEM);
	
	ret = parent->desc.src.ops->srcs_create(&srcs,*dst,desc);
	
	if (ret || !srcs) {
		vfi_dst_put(*dst);
		*dst = NULL;
		return VFI_RESULT(ret ? ret : -ENOMEM);
	}
	return VFI_RESULT(0);
}


static int vfi_local_xfer_create(struct vfi_xfer **xfer, struct vfi_location *loc, struct vfi_desc_param *desc)
{
	unsigned long extent = desc->extent;
	unsigned long long offset = desc->offset;
	int ret;

	desc->offset = 0;
	desc->extent = 0;

	ret = vfi_xfer_create(xfer,loc,desc);

	desc->offset = offset;
	desc->extent = extent;

	VFI_DEBUG(MY_DEBUG,"%s %p %p %p\n",__FUNCTION__,loc,desc,*xfer);

	return VFI_RESULT(ret);
}

static int vfi_local_sync_create(struct vfi_sync **sync, struct vfi_location *loc, struct vfi_desc_param *desc)
{

	int ret;

	ret = vfi_sync_create(sync,loc,desc);

	VFI_DEBUG(MY_DEBUG,"%s %p %p %p\n",__FUNCTION__,loc,desc,*sync);

	return VFI_RESULT(ret);
}

/**
* vfi_local_src_events - create events for bind <src>
* @dst  : parent <dst> object
* @desc : bind descriptor
*
* This function creates two <src> events - src_ready and src_done - when <src> and
* <xfer> occupy the same network location.
*
* There is no need, in this case, to use fabric doorbells to deliver signals: all we
* need do is create two pure event objects with rigged identifiers, bound where necessary
* to "start" functions that are aware of that.
*
* The src_ready event is sent from <xfer> to <src> to trigger a bind transfer action.
* It uses the <src>->src_ready() op to deliver the signal. We specify <src> here to show
* intent, although in practise - since <src> and <xfer> are co-located and local - it
* always boils down to vfi_local_src_ready().
*
* The src_done event does not have a "start" op. That is because the VFI DMA layer knows
* how to deliver the event - all it needs is the event pointer to do so.
*
* The @dst argument points to the <dst> with which this <src> is associated: it is required
* that @dst parent point at the <bind> that owns both.
**/
static int vfi_local_src_events(struct vfi_dst *parent, struct vfi_bind_param *desc)
{
	/* Local source SMB with a local transfer agent. */
	struct vfi_bind *bind;
	struct vfi_events *event_list;
	char *event_name;
	int ret;

	VFI_DEBUG(MY_DEBUG,"%s dst_parent(%p) desc(%p)\n",__FUNCTION__,parent,desc);

	bind = vfi_dst_parent(parent);

	VFI_DEBUG(MY_DEBUG,"%s bind(%p)->src_done_event(%p)\n",__FUNCTION__,bind,bind->src_done_event);
	if (bind->src_done_event)
		return VFI_RESULT(0);

	event_name = vfi_get_option(&desc->src,"event_name");
	
	if (event_name == NULL)
		goto event_name_fail;

	ret = find_vfi_events(&event_list,vfi_subsys->events,event_name);
	if (event_list == NULL)
		ret = vfi_events_create(&event_list,vfi_subsys->events,event_name);

	if (event_list == NULL)
		goto dones_fail;

	ret = vfi_event_create(&bind->src_done_event,event_list,&desc->src,bind,0,(int)parent);

	ret = vfi_event_create(&bind->src_ready_event,event_list,&desc->src,bind,bind->desc.src.ops->src_ready,(int)&parent->srcs);
		
	return VFI_RESULT(ret);

dones_fail:
event_name_fail:				   
	return VFI_RESULT(-EINVAL);
}

/**
*
*
*
**/
static void vfi_local_src_ev_delete (struct vfi_dst *parent, struct vfi_bind_param *desc)
{
	struct vfi_bind *bind = vfi_dst_parent (parent);
	
	VFI_DEBUG (MY_DEBUG, "%s: for %s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x\n", __func__,
	             desc->xfer.name, desc->xfer.location, desc->xfer.offset, desc->xfer.extent,
	             desc->dst.name, desc->dst.location, desc->dst.offset, desc->dst.extent,
	             desc->src.name, desc->src.location, desc->src.offset, desc->src.extent);
	VFI_DEBUG (MY_DEBUG, "src_ready (%p, %08x), src_done (%p, %08x)\n",
	             bind->src_ready_event, bind->src_ready_event_id,
	             bind->src_done_event, bind->src_done_event_id);
	vfi_event_delete (bind->src_ready_event);
	vfi_event_delete (bind->src_done_event);
	bind->src_ready_event = bind->src_done_event = NULL;
	bind->src_ready_event_id = bind->src_done_event_id = 0;
}

/**
* vfi_local_srcs_create - create bind <srcs> kset on local tree
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
static int vfi_local_srcs_create(struct vfi_srcs **srcs, struct vfi_dst *parent, struct vfi_bind_param *desc)
{
/* 	srcs_create://tp.x:2000/d.p#uuuuu000:1000=s.r#c000:1000 */

	int page, first_page, last_page;
	int ret;
	struct vfi_smb *smb;
	struct vfi_src *src;
	struct vfi_bind_param params = *desc;
	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	/*
	* Create the <srcs> kset and install it in the
	* local instance of its parent <dst>. 
	*
	*/
	ret = vfi_srcs_create(srcs,parent,desc);

	if (ret)
		return VFI_RESULT(ret);

	/*
	* Source-related events stuff. Not yet figured what
	* this does...
	*
	*/
	if (parent->desc.xfer.ops->src_events(parent,desc))
		return VFI_RESULT(-EINVAL);

	/*
	* Find the local representation of the <smb> we are
	* pulling data out of. This node we are running on
	* is home to the <smb>, otherwise we wouldn't be here.
	*
	*/
	ret = find_vfi_smb(&smb,&desc->src);

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

		ret = parent->desc.xfer.ops->src_create(&src,parent,&params);
		if (!src)
			goto fail_newsrc;
		params.src.offset = 0;
		params.dst.extent += params.src.extent;
		if (page + 2 >= last_page && END_SIZE(&smb->desc,&desc->src))
			params.src.extent = END_SIZE(&smb->desc,&desc->src);
		else
			params.src.extent = PAGE_SIZE;
	} 

	vfi_dst_load_srcs(parent);

	return VFI_RESULT(0);

fail_newsrc:
	vfi_smb_put(smb);
	vfi_srcs_delete(*srcs);
	*srcs = NULL;
	return VFI_RESULT(-EINVAL);
}

/**
* vfi_local_src_create - create a bind <src> component
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
static int vfi_local_src_create(struct vfi_src **src, struct vfi_dst *parent, struct vfi_bind_param *desc)
{
/* 	src_create://tp.x:2000/d.p#uuuuu000:800=s.r#xxxxx800:800 */
	int ret;
	VFI_DEBUG(MY_DEBUG,"%s %s.%s#%llx:%x\n",__FUNCTION__,desc->src.name,desc->src.location, desc->src.offset,desc->src.extent);

	ret = vfi_src_create(src,parent,desc);

	return VFI_RESULT(ret);
}


/*
 * D E L E T E    O P E R A T I O N S
 */
static void vfi_local_location_delete(struct vfi_location *loc, struct vfi_desc_param *desc)
{
	struct vfi_location *target = loc;
	int ret;
	VFI_DEBUG(MY_DEBUG,"%s %p %p\n",__FUNCTION__,loc,desc);
	
	if (desc->location && *desc->location)
		ret = find_vfi_name(&target,loc,desc);

	vfi_location_delete(target);
}

static void vfi_local_smb_delete(struct vfi_location *loc, struct vfi_desc_param *desc)
{
	struct vfi_smb *smb;

	vfi_local_smb_find(&smb, loc,desc);
	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
	if (smb) {
		vfi_smb_put(smb);
		vfi_smb_delete(smb);
	}
}

static void vfi_local_mmap_delete(struct vfi_smb *smb, struct vfi_desc_param *desc)
{
	vfi_mmap_delete(smb,desc);
}

/**
 * vfi_local_srcs_delete - delete bind <dst> fragment <srcs> and any <src> subsidiaries
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
static struct vfi_dst *vfi_local_srcs_delete (struct vfi_dst *parent, struct vfi_bind_param *desc)
{
	struct list_head *entry, *safety;
	struct vfi_srcs *srcs = parent->srcs;
	struct vfi_bind *bind = parent->bind;
	
	VFI_DEBUG (MY_DEBUG,"%s for %s.%s#%llx:%x\n",__FUNCTION__, 
		     parent->desc.xfer.name, parent->desc.xfer.location,parent->desc.xfer.offset, parent->desc.xfer.extent);
#if 0	
	printk ("%s: Dst  - %s.%s#%llx:%x(%p)/%s.%s#%llx:%x(%p)=%s.%s#%llx:%x(%p)\n", 
	        __func__, 
	        parent->desc.xfer.name, parent->desc.xfer.location, parent->desc.xfer.offset, parent->desc.xfer.extent, parent->desc.xfer.ops, 
	        parent->desc.dst.name, parent->desc.dst.location, parent->desc.dst.offset, parent->desc.dst.extent, parent->desc.dst.ops, 
	        parent->desc.src.name, parent->desc.src.location, parent->desc.src.offset, parent->desc.src.extent, parent->desc.src.ops);
	printk ("%s: Bind - %s.%s#%llx:%x(%p)/%s.%s#%llx:%x(%p)=%s.%s#%llx:%x(%p)\n", 
	        __func__, 
	        bind->desc.xfer.name, bind->desc.xfer.location, bind->desc.xfer.offset, bind->desc.xfer.extent, bind->desc.xfer.ops, 
	        bind->desc.dst.name, bind->desc.dst.location, bind->desc.dst.offset, bind->desc.dst.extent, bind->desc.dst.ops, 
	        bind->desc.src.name, bind->desc.src.location, bind->desc.src.offset, bind->desc.src.extent, bind->desc.src.ops);
	printk ("%s: Desc - %s.%s#%llx:%x(%p)/%s.%s#%llx:%x(%p)=%s.%s#%llx:%x(%p)\n", 
	        __func__, 
	        desc->xfer.name, desc->xfer.location, desc->xfer.offset, desc->xfer.extent, desc->xfer.ops, 
	        desc->dst.name, desc->dst.location, desc->dst.offset, desc->dst.extent, desc->dst.ops, 
	        desc->src.name, desc->src.location, desc->src.offset, desc->src.extent, desc->src.ops);
#endif

	if (srcs) {
		int dstref = 0;
		VFI_DEBUG (MY_DEBUG, "-- Srcs \"%s\"\n", kobject_name (&srcs->kset.kobj));
		if (!list_empty (&srcs->kset.list)) {
			list_for_each_safe (entry, safety, &srcs->kset.list) {
				struct vfi_src      *src;
				src = to_vfi_src (to_kobj (entry));
				if (src && src->desc.xfer.ops->src_delete) {
					src->desc.xfer.ops->src_delete (parent, &src->desc);
				}
			}
			if (!list_empty (&srcs->kset.list)) {
				printk ("WTF> Srcs %s kset is not yet empty!\n", kobject_name (&srcs->kset.kobj));
				list_for_each_safe (entry, safety, &srcs->kset.list) {
					struct kobject 		*kobj;
					int i;
					kobj = to_kobj (entry);
					i = atomic_read (&kobj->kref.refcount);
					printk ("--- \"%s\" [%x]\n", kobject_name (kobj), i);
				}
			}
		}
		VFI_DEBUG (MY_LIFE_DEBUG, "-- Srcs Count: %lx\n", (unsigned long)srcs->kset.kobj.kref.refcount.counter);

		if (parent->desc.xfer.ops->src_ev_delete) {
			parent->desc.xfer.ops->src_ev_delete (parent, desc);
		}
		else {
			printk ("xxx %s: bind xfer has no src_ev_delete() op!\n", __func__);
		}
		
		

		
		vfi_srcs_delete (srcs);
		
		/*
		* Clean-up on-the-fly bind hierarchy
		*
		* This clause only runs if <src> is remote from <xfer>. 
		* BUT: it ought only to run if <src> is also remote from <dst>, 
		* but we have no way to know that (the plocs and ops are screwed,
		* so when one or other is remote from <xfer>, both appear to be
		* remote at the same location).
		*
		*/
		dstref = atomic_read (&parent->kobj.kref.refcount);
//		printk ("%s: Parent Refcount is %d\n", __func__, dstref);
		if (parent->desc.src.ops != parent->desc.xfer.ops && dstref == 2) {
			struct vfi_dsts *dsts = (bind) ? bind->dsts : NULL;
			
			VFI_KTRACE ("<*** %s - unravel on-the-fly bind...dst IN ***>\n", __func__);
			VFI_KTRACE ("-- %s: Bind (%p), Dsts (%p), Dst (%p)\n", __func__, bind, dsts, parent);
			vfi_dst_unregister (parent);
			parent = NULL;
			
			/*
			* Examine the dsts list, and if it is empty, delete
			* first <dsts> then <bind>.
			*
			*/
			if (dsts && list_empty (&dsts->kset.list)) {
				int bindref = atomic_read (&bind->kobj.kref.refcount);
				/*
				* HACK ATTACK> make sure the bind has a refcount of 
				* three before we unregister the dsts and the bind.
				*
				*/
				if (bindref < 3) {
					VFI_KTRACE ("-- Whoopsie - bindref %d too small for \"%s\"\n", bindref, kobject_name (&bind->kobj));
					while (bindref < 3) {
						kobject_get (&bind->kobj);
						bindref++;
					}
				}
				vfi_dsts_unregister (dsts);
				vfi_bind_unregister (bind);
			}
			VFI_KTRACE ("<*** %s - unravel on-the-fly bind...dst OUT ***>\n", __func__);
		}
		else {
			VFI_KTRACE ("<*** %s - did NOT unravel any on-the-fly bind ***>\n", __func__);
		}
	}
	return (parent);
}

/**
* vfi_local_src_delete - Delete bind <src> fragment
* 
*
*
**/
static void vfi_local_src_delete (struct vfi_dst *parent, struct vfi_bind_param *desc)
{	
	VFI_DEBUG (MY_DEBUG, "%s (%s.%s#%llx:%x/%s.%s#%llx:%x=<*>)\n", __func__, 
	             desc->xfer.name, desc->xfer.location, desc->xfer.offset, desc->xfer.extent, 
	             desc->dst.name, desc->dst.location, desc->dst.offset, desc->dst.extent);
	printk ("-- Parent bind: %s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x\n", 
	        parent->desc.xfer.name, parent->desc.xfer.location, parent->desc.xfer.offset, parent->desc.xfer.extent, 
	        parent->desc.dst.name, parent->desc.dst.location, parent->desc.dst.offset, parent->desc.dst.extent, 
	        parent->desc.src.name, parent->desc.src.location, parent->desc.src.offset, parent->desc.src.extent);
	
	vfi_src_delete (parent, desc);
}


/**
* vfi_local_dsts_delete - Delete bind destinations
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
static struct vfi_bind *vfi_local_dsts_delete (struct vfi_bind *parent, struct vfi_bind_param *desc)
{
	struct list_head *entry, *safety;
	struct vfi_dsts *dsts = parent->dsts;
	
	VFI_DEBUG (MY_DEBUG,"%s for %s.%s#%llx:%x\n",__FUNCTION__, 
		     parent->desc.xfer.name, parent->desc.xfer.location,parent->desc.xfer.offset, parent->desc.xfer.extent);
	if (dsts) {
		VFI_DEBUG (MY_DEBUG, "-- Dsts \"%s\"\n", kobject_name (&dsts->kset.kobj));
		if (!list_empty (&dsts->kset.list)) {
			list_for_each_safe (entry, safety, &dsts->kset.list) {
				struct vfi_dst      	*dst;
				dst = to_vfi_dst (to_kobj (entry));
				dst->desc.xfer.ops->dst_delete (parent, &dst->desc);
			}
			
			if (!list_empty (&dsts->kset.list)) {
				printk ("WTF> Dsts %s kset is not yet empty!\n", kobject_name (&dsts->kset.kobj));
				list_for_each_safe (entry, safety, &dsts->kset.list) {
					struct kobject 		*kobj;
					int i;
					kobj = to_kobj (entry);
					i = atomic_read (&kobj->kref.refcount);
					printk ("--- \"%s\" [%x]\n", kobject_name (kobj), i);
				}
				
			}
		}
		
		VFI_DEBUG (MY_LIFE_DEBUG, "-- Dsts Count: %lx\n", (unsigned long)dsts->kset.kobj.kref.refcount.counter);

		if (parent->desc.xfer.ops->dst_ev_delete) {
			parent->desc.xfer.ops->dst_ev_delete (parent, desc);
		}
		else {
			printk ("xxx %s: bind xfer has no dst_ev_delete() op!\n", __func__);
		}
		
		vfi_dsts_delete (dsts);
		
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
			vfi_bind_unregister (parent);
			parent = NULL;
		}
	}
	return (parent);
}

/**
* vfi_local_dst_delete - Delete bind <dst> fragment and its <srcs> subsidiaries
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
static void vfi_local_dst_delete(struct vfi_bind *parent, struct vfi_bind_param *desc)
{
	struct vfi_dst *dst;
	int ret;

	VFI_DEBUG (MY_DEBUG, "%s (%s.%s#%llx:%x/%s.%s#%llx:%x=<*>)\n", __func__, 
		desc->xfer.name, desc->xfer.location, desc->xfer.offset, desc->xfer.extent, 
		desc->dst.name, desc->dst.location, desc->dst.offset, desc->dst.extent);
/*
	printk ("-- Parent bind: %s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x\n", 
		parent->desc.xfer.name, parent->desc.xfer.location, parent->desc.xfer.offset, parent->desc.xfer.extent, 
		parent->desc.dst.name, parent->desc.dst.location, parent->desc.dst.offset, parent->desc.dst.extent, 
		parent->desc.src.name, parent->desc.src.location, parent->desc.src.offset, parent->desc.src.extent);
*/

	/*
	* INVESTIGATE: the find call will, if successful, leave
	* the dst refcount incremented - need to ensure that this
	* additional refcount is successfully countermanded somewhere
	* in the dst_delete chain.
	*/
	if (!(ret = find_vfi_dst_in(&dst,parent,desc))) {
		parent->desc.src.ops->srcs_delete(dst,desc);
		vfi_dst_delete(parent,desc);
	}
}

static void vfi_local_done(struct vfi_event *event)
{
	/* A DMA engine, either local or remote, has completed a
	 * transfer involving a local smb as the source or
	 * destination. Do vote adjustment. */
	struct vfi_events *e = to_vfi_events(event->kobj.parent);

	e->count--;

	if (e->count == 0)
		complete(&e->dma_sync);

	VFI_DEBUG(MY_DEBUG,"%s event(%p) events(%p) count(%d)\n",__FUNCTION__,event,e,e->count);
}

static void vfi_local_src_done(struct vfi_bind *bind)
{
	/* A DMA engine, either local or remote, has completed a
	 * transfer involving a local smb as the source. Do vote
	 * adjustment. */
	VFI_DEBUG(MY_DEBUG,"%s bind(%p)\n",__FUNCTION__,bind);
	vfi_bind_src_done(bind);
	bind->desc.src.ops->done(bind->src_done_event);
}

static void vfi_local_dst_done(struct vfi_bind *bind)
{
	/* A DMA engine, either local or remote, has completed a
	 * transfer involving a local SMB as the destination. Do vote
	 * ajustment accordingly */
	VFI_DEBUG(MY_DEBUG,"%s bind(%p)\n",__FUNCTION__,bind);
	vfi_bind_dst_done(bind);
	bind->desc.dst.ops->done(bind->dst_done_event);
}

static void vfi_local_src_ready(struct vfi_bind *bind)
{
	/* Someone, either locally or via the fabric, executed start
	 * on an event which is telling us, the local DMA engine
	 * assigned for this bind, that the source SMB, which may be
	 * local or remote, is ready for action. */
	VFI_DEBUG(MY_DEBUG,"%s bind(%p)\n",__FUNCTION__,bind);
	if (vfi_bind_src_ready(bind))
		bind->desc.xfer.rde->ops->queue_transfer(&bind->descriptor);
	
}

static void vfi_local_dst_ready(struct vfi_bind *bind)
{
	/* Someone, either locally or via the fabric, executed start on
	 * an event which is telling us, the local DMA engine assigned
	 * for this bind, that the destination SMB, which may be local
	 * or remote, is ready for action. */
	VFI_DEBUG(MY_DEBUG,"%s bind(%p)\n",__FUNCTION__,bind);
	if (vfi_bind_dst_ready(bind))
		bind->desc.xfer.rde->ops->queue_transfer(&bind->descriptor);
}

static int vfi_local_event_start(struct vfi_location *loc, struct vfi_desc_param *desc)
{
	struct vfi_events *event_list;
	int ret;
	ret = find_vfi_events(&event_list,vfi_subsys->events,desc->name);
	if (event_list == NULL )
		return VFI_RESULT(-EINVAL);

	/* Loop through the event chain if any */
	while (event_list) {
		vfi_events_start(event_list);
		event_list = event_list->next;
	}

	return VFI_RESULT(0);
}



static int vfi_local_event_find(struct vfi_location *loc, struct vfi_desc_param *desc)
{
	struct vfi_events *event_list_this;
	int ret;

	VFI_DEBUG (MY_DEBUG, "#### %s entered \n", __FUNCTION__);

	ret = find_vfi_events(&event_list_this,vfi_subsys->events, desc->name);
	if (event_list_this == NULL)
		return VFI_RESULT(-EINVAL);

	return VFI_RESULT(ret);
}

static int vfi_local_event_chain(struct vfi_location *loc, struct vfi_desc_param *desc)
{

	char                *event_name;
	struct vfi_events *event_list_this;
	struct vfi_events *event_list_next;
	int ret;

	VFI_DEBUG (MY_DEBUG,
		     "#### %s entered \n",

		     __FUNCTION__);

	/* Find the event_name in the desc */
	event_name = vfi_get_option(desc, "event_name");
	if (event_name == NULL) 
		return VFI_RESULT(-EINVAL);

	/* Lookup this event */
	ret = find_vfi_events(&event_list_this,vfi_subsys->events, desc->name);
	if (event_list_this == NULL)
		return VFI_RESULT(-EINVAL);

	/* Lookup next event */
	ret = find_vfi_events(&event_list_next,vfi_subsys->events, event_name);
	if (event_list_next == NULL)
		return VFI_RESULT(-EINVAL);

	/* Make this event point to next event */
	event_list_this->next = event_list_next;
	event_list_next->prev = event_list_this;

	return VFI_RESULT(0);
}




/**
* vfi_local_bind_delete - delete a bind associated with an xfer
* @parent : pointer to the <xfer> that the target <bind> belongs to.
* @desc   : descriptor for <xfer>. Its embedded offset is the primary means of bind selection.
*
* This function - which runs ONLY on the Xfer agent associated with the target bind - will
* delete the specified bind and all of its subsidiaries.
*
* 
**/
static void vfi_local_bind_delete(struct vfi_xfer *parent, struct vfi_desc_param *desc)
{
	struct list_head *entry, *safety;
	VFI_DEBUG (MY_DEBUG, "%s: (%s.%s#%llx:%x)\n", __func__, 
			desc->name, desc->location, desc->offset, desc->extent);
	/*
	* Every <xfer> has a <binds> kset that lists the various binds 
	* that exist for this transfer. Run through that list and delete
	* any binds whose offset lies within the offset/extent range of the
	* delete specification.
	*/
	if (!list_empty(&parent->binds->kset.list)) {
		list_for_each_safe(entry,safety,&parent->binds->kset.list) {
			struct vfi_bind *bind;
			bind = to_vfi_bind(to_kobj(entry));
			if (bind->desc.xfer.offset >= desc->offset &&
			    bind->desc.xfer.offset <= desc->offset + desc->extent) {
				bind->desc.dst.ops->dsts_delete(bind,&bind->desc);
			}
			vfi_bind_delete (parent, &bind->desc.xfer);
		}
	}
	else {
		printk ("xxx %s: binds kset is empty.\n", __func__);
	}
}

static void vfi_local_xfer_delete(struct vfi_location *parent, struct vfi_desc_param *desc)
{
}

static void vfi_local_sync_delete(struct vfi_location *parent, struct vfi_desc_param *desc)
{
}

static int vfi_local_sync_send(struct vfi_sync *sync, struct vfi_desc_param *desc)
{
	if (down_interruptible(&sync->sem))
		return VFI_RESULT(-ERESTARTSYS);

	if (sync->count == 0)
		sync->count += desc->offset;

	if (sync->count)
		sync->count--;

	if (sync->count == 0)
		wake_up_interruptible(&sync->waitq);

	up(&sync->sem);
	
	return VFI_RESULT(0);
}

static int vfi_local_sync_wait(struct vfi_sync *sync, struct vfi_desc_param *desc)
{
	if (down_interruptible(&sync->sem))
		return VFI_RESULT(-ERESTARTSYS);
	
	if (sync->count == 0)
		sync->count += desc->offset;

	while (sync->count) {
		up(&sync->sem);

		if (wait_event_interruptible(sync->waitq, (sync->count)))
			return VFI_RESULT(-ERESTARTSYS);

		if (down_interruptible(&sync->sem))
			return VFI_RESULT(-ERESTARTSYS);
	}
	up(&sync->sem);

	return VFI_RESULT(0);
}

struct vfi_ops vfi_local_ops = {
	.location_create = vfi_local_location_create,
	.location_delete = vfi_local_location_delete,
	.location_find   = vfi_local_location_find,
	.location_put    = vfi_local_location_put,
	.smb_create      = vfi_local_smb_create,
	.smb_delete      = vfi_local_smb_delete,
	.smb_find        = vfi_local_smb_find,
	.mmap_create     = vfi_local_mmap_create,
	.mmap_delete     = vfi_local_mmap_delete,
	.xfer_create     = vfi_local_xfer_create,
	.xfer_delete     = vfi_local_xfer_delete,
	.xfer_find       = vfi_local_xfer_find,
	.sync_create     = vfi_local_sync_create,
	.sync_delete     = vfi_local_sync_delete,
	.sync_find       = vfi_local_sync_find,
	.sync_send       = vfi_local_sync_send,
	.sync_wait       = vfi_local_sync_wait,
	.srcs_create     = vfi_local_srcs_create,
	.srcs_delete     = vfi_local_srcs_delete,
	.src_create      = vfi_local_src_create,
	.src_delete      = vfi_local_src_delete,
	.src_find        = vfi_local_src_find,
	.dsts_create     = vfi_local_dsts_create,
	.dsts_delete     = vfi_local_dsts_delete, 
	.dst_create      = vfi_local_dst_create,
	.dst_delete      = vfi_local_dst_delete,
	.dst_find        = vfi_local_dst_find,
	.bind_find       = vfi_local_bind_find,
	.bind_create     = vfi_local_bind_create,
	.bind_delete     = vfi_local_bind_delete, 
	.src_done        = vfi_local_src_done,
	.dst_done        = vfi_local_dst_done,
	.done            = vfi_local_done,
	.src_ready       = vfi_local_src_ready,
	.dst_ready       = vfi_local_dst_ready,
	.dst_events      = vfi_local_dst_events,
	.src_events      = vfi_local_src_events,
	.dst_ev_delete	 = vfi_local_dst_ev_delete,
	.src_ev_delete	 = vfi_local_src_ev_delete,
	.event_start     = vfi_local_event_start,
	.event_find     = vfi_local_event_find,
	.event_chain     = vfi_local_event_chain,
};

EXPORT_SYMBOL (vfi_local_ops);

