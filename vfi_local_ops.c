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
#include <linux/vfi_smbs.h>
#include <linux/vfi_xfers.h>
#include <linux/vfi_srcs.h>
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
	return VFI_RESULT(find_vfi_name(newloc,loc,desc));
}

static void vfi_local_location_put(struct vfi_location *loc, struct vfi_desc_param *desc)
{
	struct vfi_location *newloc;
	VFI_DEBUG(MY_DEBUG,"%s %p %p %s\n",__FUNCTION__,loc,desc,desc->name);

	if (!find_vfi_name(&newloc,loc,desc)) {
		vfi_location_put(newloc);
		vfi_location_put(newloc);
	}
}

static void vfi_local_location_lose(struct vfi_location *loc, struct vfi_desc_param *desc)
{
	VFI_DEBUG(MY_DEBUG,"%s %p %p %s\n",__FUNCTION__,loc,desc,desc->name);
}

static int vfi_local_smb_find(struct vfi_smb **smb, struct vfi_location *parent, struct vfi_desc_param *desc)
{
	*smb = to_vfi_smb(kset_find_obj(&parent->smbs->kset,desc->name));
	VFI_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,parent,desc,*smb);
	if (*smb == NULL)
		return VFI_RESULT(-EINVAL);
	return VFI_RESULT(0);
}

static void vfi_local_smb_put(struct vfi_smb *smb, struct vfi_desc_param *desc)
{
	VFI_DEBUG(MY_DEBUG,"%s %p %p\n",__FUNCTION__,smb,desc);
	vfi_smb_put(smb);
}

static void vfi_local_smb_lose(struct vfi_smb *smb, struct vfi_desc_param *desc)
{
	VFI_DEBUG(MY_DEBUG,"%s %p %p\n",__FUNCTION__,smb,desc);
	/*
	 * We end up here oly upon a private driver call with "smb_lose" when the ref count of the
	 * remote cache instance has reached 0 and we want to decrement the refcount of the local instance
	 */
	vfi_smb_put(smb);
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

static void vfi_local_xfer_put(struct vfi_xfer *xfer,struct vfi_desc_param *desc)
{
	VFI_DEBUG(MY_DEBUG,"%s %p %p\n",__FUNCTION__,xfer,desc);
	vfi_xfer_put(xfer);
}

static void vfi_local_xfer_lose(struct vfi_xfer *xfer,struct vfi_desc_param *desc)
{
	/*
	 * We end up here oly upon a private driver call with "xfer_lose" when the ref count of the
	 * remote cache instance has reached 0 and we want to decrement the refcount of the local instance
	 */
	VFI_DEBUG(MY_DEBUG,"%s %p %p\n",__FUNCTION__,xfer,desc);
	vfi_xfer_put(xfer);
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
* vfi_local_sync_put - put an vfi_sync object for a named sync at the local site.
* @sync: sync to be put
* @desc: target sync parameter descriptor
*
*
**/
static void vfi_local_sync_put(struct vfi_sync *sync, struct vfi_desc_param *desc)
{
	VFI_DEBUG(MY_DEBUG,"%s %p %p\n",__FUNCTION__,sync,desc);
	vfi_sync_put(sync);
}

static int vfi_local_mmap_find(struct vfi_mmap **mmap, struct vfi_smb *parent, struct vfi_desc_param *desc)
{
	char buf[512];
	snprintf(buf,512,"#%llx:%x",desc->offset,desc->extent);
	if (parent)
		*mmap = to_vfi_mmap(kset_find_obj(&parent->kset,buf));
	else
		*mmap = NULL;
	VFI_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,parent,desc,*mmap);
	return VFI_RESULT(*mmap == NULL);
}

static void vfi_local_mmap_put(struct vfi_mmap *mmap, struct vfi_desc_param *desc)
{
	VFI_DEBUG(MY_DEBUG,"%s %p %p\n",__FUNCTION__,mmap,desc);
	vfi_mmap_put(mmap);
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

	*bind = to_vfi_bind(kset_find_obj(&parent->kset,buf));

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
	struct vfi_location *oldloc;
	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	/* Check if the location already exists */
	ret = find_vfi_name(&oldloc,loc,desc);
	if (!ret) {
		vfi_location_put(oldloc);
		return VFI_RESULT(-EEXIST);
	}

	ret = vfi_location_create(newloc,loc,desc);
	if (!ret) {
		/* 
		   If the parent location has a extent of 0 we need to set that here.
		   This can and will happen if the parent is a public root location. 
		*/
		if (loc && !(loc->desc.extent))
			loc->desc.extent = (*newloc)->desc.extent;
		
		if (*newloc && (*newloc)->desc.address)
			vfi_address_register(*newloc);
	}

	VFI_DEBUG(MY_DEBUG,"%s %p %p -> %p\n",__FUNCTION__,loc,desc,*newloc);
	return VFI_RESULT(ret);
}

static int vfi_local_smb_create(struct vfi_smb **newsmb, struct vfi_location *loc, struct vfi_desc_param *desc)
{
	int ret;
	struct vfi_smb *smb;
	char *sopt;
	*newsmb = NULL;

	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	ret = vfi_smb_create(&smb, loc, desc);

	if (ret)
		return VFI_RESULT(ret);

	*newsmb = smb;

/*
 * We can be allocating a kernel smb to be mapped to the application
 * or we can be allocating an smb to use an applications user
 * pages. Use the option map_address(xxx) to determine.
 *
 * If the user gives us a map_address which is not on a page boundary
 * *we can adjust the smb offset to account for it.
 *
 * In either case the user can give an offset of the logical smb which
 * the user may require because of restrictions on address boundaries
 * for some other devices so simply add map_address modulo page
 * boundary to desc.offset before allocating.
 *
 * The desc.extent *gives the size required in both cases.
 */
	if (smb->desc.extent)
		smb->size = smb->desc.extent;
	else if ( (sopt = vfi_get_option(&smb->desc,"map_extent")) ) 
		smb->size = smb->desc.extent =  simple_strtoul(sopt,NULL,16);
	else
		goto out;
		

	if ((sopt = vfi_get_option(&smb->desc,"map_address"))) {
		unsigned long address;
		unsigned long offset;
		address = simple_strtoul(sopt,NULL,16);
		offset = address & ~PAGE_MASK;
		address = address & PAGE_MASK;
		smb->desc.offset += offset;
		smb->size = smb->desc.extent += offset;
		smb->address = address;
	}

	if (smb->desc.offset > (PAGE_SIZE - 32)) {
		ret = -EINVAL;
		goto out;
	}

	/* Allocate memory for this SMB */
	ret = vfi_alloc_pages(smb);
	if (!ret)
		return VFI_RESULT(0);
out:
	/* Failed */
	vfi_smb_delete(smb);
	*newsmb = NULL;
	return VFI_RESULT(ret);
}

static int vfi_local_mmap_create(struct vfi_mmap **mmap, struct vfi_smb *smb, struct vfi_desc_param *desc)
{
	int ret;
	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	ret = find_vfi_mmap_in(mmap,smb,desc);

	if (ret)
		return VFI_RESULT(vfi_mmap_create(mmap,smb,desc));

	return VFI_RESULT(0);
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
	if (ret)
		goto event_done_fail;

	ret = vfi_event_create(&bind->dst_ready_event,event_list,&desc->dst,bind,bind->desc.dst.ops->dst_ready,(int)&bind->dst_ready_event);
	if (ret)
		goto event_ready_fail;

	vfi_events_put(event_list);
						   
	return VFI_RESULT(ret);

event_ready_fail:
	vfi_event_put(bind->dst_done_event);
event_done_fail:
	vfi_events_put(event_list);
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

	ret = parent->desc.xfer.ops->dst_events(parent,desc);
	if (ret)
		goto fail_events;

	ret = vfi_dsts_create(dsts,parent,desc);
	if (ret)
		goto fail_dsts;

	ret = find_vfi_smb(&dsmb,&desc->dst);
	if (ret)
		goto fail_dsmb;

	if (!DESC_VALID(&dsmb->desc,&desc->dst))
		goto fail_ddesc;

	first_page = START_PAGE(&dsmb->desc,&desc->dst);
	last_page = first_page + NUM_DMA(&dsmb->desc,&desc->dst);

	params.dst.offset = START_OFFSET((&dsmb->desc), (&desc->dst));
	params.src.extent = params.dst.extent = START_SIZE(&dsmb->desc, &desc->dst);
	
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
		if (ret)
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

	vfi_smb_put(dsmb);

	parent->end_of_chain = parent->dma_chain.prev;

	return VFI_RESULT(parent == NULL);

fail_newdst:
fail_ddesc:
	vfi_smb_put(dsmb);
fail_dsmb:
	vfi_dsts_put(*dsts);
fail_dsts:
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
	if (!desc->xfer.offset)
		desc->xfer.offset = xfer->desc.extent;

	xfer->desc.extent += desc->xfer.extent;
/* End Atomic */
	ret = vfi_bind_create(bind,xfer, desc);
	if (ret)
		goto fail_bind;

	ret = vfi_dsts_create(&dsts,*bind,desc);
	if (ret)
		goto fail_dsts;

	ret = find_vfi_smb(&dsmb,&desc->dst);
	if ( NULL == dsmb )
		goto fail_dsmb;

	if (!DESC_VALID(&dsmb->desc,&desc->dst))
		goto fail_ddesc;

	ret = find_vfi_smb(&ssmb,&desc->src);
	if ( NULL == ssmb)
		goto fail_ddesc;

	if (!DESC_VALID(&ssmb->desc,&desc->src))
		goto fail_sdesc;

	vfi_inherit(&(*bind)->desc.src,&ssmb->desc);
	vfi_inherit(&(*bind)->desc.dst,&dsmb->desc);

	ret = (*bind)->desc.dst.ops->dsts_create(&rdsts,*bind,desc);
	if ( NULL == rdsts)
		goto fail_dst;

	vfi_xfer_load_binds(xfer,*bind);

#ifdef CONFIG_VFI_DEBUG
	if (vfi_debug_level & VFI_DBG_DMA_CHAIN)
		vfi_dma_chain_dump(&(*bind)->dma_chain);
#endif

	/*
	 * The bind is keeping a reference on the SMBs until the bind itself is deleted
	 */
	// ssmb->desc.ops->smb_put(ssmb,&ssmb->desc);
	// dsmb->desc.ops->smb_put(dsmb,&dsmb->desc);

	return VFI_RESULT(0);

fail_dst:
fail_sdesc:
	ssmb->desc.ops->smb_put(ssmb,&ssmb->desc);
fail_ddesc:
	dsmb->desc.ops->smb_put(dsmb,&dsmb->desc);
fail_dsmb:
	vfi_dsts_delete(dsts);
fail_dsts:
	vfi_bind_delete(xfer,&desc->xfer);
fail_bind:
	*bind = NULL;
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

	if (ret)
		goto events_fail;

	ret = vfi_event_create(&bind->src_done_event,event_list,&desc->src,bind,0,(int)parent);
	if (ret)
		goto event_done_fail;

	ret = vfi_event_create(&bind->src_ready_event,event_list,&desc->src,bind,bind->desc.src.ops->src_ready,(int)&parent->srcs);
	if (ret)
		goto event_ready_fail;

	vfi_events_put(event_list);

	return VFI_RESULT(ret);

event_ready_fail:
	vfi_event_put(bind->src_done_event);
event_done_fail:
	vfi_events_put(event_list);
events_fail:
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

	ret = vfi_srcs_create(srcs,parent,desc);
	if (ret)
		goto fail_srcs;

	ret = parent->desc.xfer.ops->src_events(parent,desc);
	if (ret)
		goto fail_events;

	ret = find_vfi_smb(&smb,&desc->src);
	if (ret)
		goto fail_smb;

	if (!DESC_VALID(&smb->desc,&desc->src))
		goto fail_sdesc;

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
	
	vfi_smb_put(smb);

	return VFI_RESULT(0);

fail_newsrc:
fail_sdesc:
	vfi_smb_put(smb);
fail_smb:
fail_events:
	vfi_srcs_delete(*srcs);
fail_srcs:
	*srcs = NULL;
	return VFI_RESULT(ret);
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
	
	ret = find_vfi_name(&target,loc,desc);
	if (ret)
		return;
	vfi_location_delete(target);
	vfi_location_put(target);
}

static void vfi_local_smb_delete(struct vfi_smb *smb, struct vfi_desc_param *desc)
{
	VFI_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
	if (smb) {
		vfi_smb_delete(smb);
	}
}

static void vfi_local_mmap_delete(struct vfi_mmap *mmap, struct vfi_desc_param *desc)
{
	vfi_mmap_delete(mmap);
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
static void vfi_local_srcs_delete (struct vfi_dst *parent, struct vfi_bind_param *desc)
{
	struct list_head *entry, *safety;
	struct vfi_srcs *srcs = parent->srcs;
	
	VFI_DEBUG (MY_DEBUG,"%s for %s.%s#%llx:%x\n",__FUNCTION__, 
		     parent->desc.xfer.name, parent->desc.xfer.location,parent->desc.xfer.offset, parent->desc.xfer.extent);

	if (srcs) {
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
	}
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
static void vfi_local_dsts_delete (struct vfi_bind *parent, struct vfi_bind_param *desc)
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
	}
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

	if (!(ret = find_vfi_dst_in(&dst,parent,desc))) {
		parent->desc.src.ops->srcs_delete(dst,desc);
		vfi_dst_delete(parent,desc);
		vfi_dst_put(dst);
	}
}

static void vfi_local_done(struct vfi_event *event)
{
	/* A DMA engine, either local or remote, has completed a
	 * transfer involving a local smb as the source or
	 * destination. Do vote adjustment. */
	struct vfi_events *events = to_vfi_events(event->kobj.parent);
	complete(&events->dma_sync);
	VFI_DEBUG(MY_DEBUG,"%s event(%p) events(%p)\n",__FUNCTION__,event,events);
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
		/* find_vfi_events is doing a get */
		vfi_events_put(event_list);
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
	if (!list_empty(&parent->kset.list)) {
		list_for_each_safe(entry,safety,&parent->kset.list) {
			struct vfi_bind *bind;
			struct vfi_smb *ssmb = NULL;
			struct vfi_smb *dsmb = NULL;

			bind = to_vfi_bind(to_kobj(entry));
			if ( (bind->desc.xfer.offset >= desc->offset) &&
			     (!desc->extent || (bind->desc.xfer.offset <= desc->offset + desc->extent))) {
				VFI_DEBUG (MY_DEBUG, "%s:%d\n", __func__, __LINE__);
				bind->desc.dst.ops->dsts_delete(bind,&bind->desc);
			} else {
				VFI_DEBUG (MY_DEBUG, "%s:%d\n", __func__, __LINE__);
				continue;
			}
			/*
			 * The bind is keeping a reference on the SMBs until the bind itself is deleted
			 */
			if ( !find_vfi_smb(&dsmb, &bind->desc.dst) ) {
				/* put to accomodate for the find */
				vfi_smb_put(dsmb);
				/* put will release and  make sure it works accross the fabric */
				vfi_smb_put(dsmb);
			} else {
				VFI_DEBUG (MY_DEBUG, "%s:%d dsmb not found\n", __func__, __LINE__);
			}
			if( !find_vfi_smb(&ssmb, &bind->desc.src) ) {
				/* put to accomodate for the find */
				vfi_smb_put(ssmb);
				/* put will delete and make sure it works accross the fabric */
				vfi_smb_put(ssmb);
			} else {
				VFI_DEBUG (MY_DEBUG, "%s:%d ssmb not found\n", __func__, __LINE__);
			}

			vfi_bind_delete (parent, &bind->desc.xfer);
		}
	} else {
		VFI_DEBUG (MY_DEBUG, "%s:%d\n", __func__, __LINE__);
	}
}

/**
* vfi_local_bind_lose - puts a bind associated with an xfer
* We end up here oly upon a private driver call with "bind_lose" when the ref count of the
* remote cache instance has reached 0 and we want to decrement the refcount of the local instance.
* 
* @parent : pointer to the <xfer> that the target <bind> belongs to.
* @desc   : descriptor for <xfer>. Its embedded offset is the primary means of bind selection.
*
* This function - which runs ONLY on the Xfer agent associated with the target bind - will
* decrement the refcount of the specified bind and all of its subsidiaries.
* 
**/
static void vfi_local_bind_lose(struct vfi_xfer *parent, struct vfi_desc_param *desc)
{
	struct list_head *entry, *safety;
	struct vfi_bind *bind;

	VFI_DEBUG (MY_DEBUG, "%s: (%s.%s#%llx:%x)\n", __func__, 
			desc->name, desc->location, desc->offset, desc->extent);

	/*
	* Every <xfer> has a <binds> kset that lists the various binds 
	* that exist for this transfer. Run through that list and delete
	* any binds whose offset lies within the offset/extent range of the
	* delete specification.
	*/
	if (!list_empty(&parent->kset.list)) {
		list_for_each_safe(entry,safety,&parent->kset.list) {
			bind = to_vfi_bind(to_kobj(entry));
			if ( (bind->desc.xfer.offset >= desc->offset) &&
				 (!desc->extent || (bind->desc.xfer.offset <= desc->offset + desc->extent))) {
				vfi_bind_delete (parent, &bind->desc.xfer);
			} else {
				continue;
			}
		}
	} else {
		VFI_DEBUG (MY_DEBUG, "%s:%d\n", __func__, __LINE__);
	}
}

static void vfi_local_xfer_delete(struct vfi_xfer *xfer, struct vfi_desc_param *desc)
{
	vfi_xfer_delete(xfer,desc);
}

static void vfi_local_sync_delete(struct vfi_sync *sync, struct vfi_desc_param *desc)
{
	vfi_sync_delete(sync,desc);
}

static int vfi_local_sync_send(struct vfi_sync *sync, struct vfi_desc_param *desc)
{
	if (down_interruptible(&sync->sem))
		return VFI_RESULT(-ERESTARTSYS);

	if (sync->count == 0)
		sync->count += desc->offset;

	if (sync->count == 0)
		goto out;

	sync->count--;

	if (sync->count == 0)
		wake_up_interruptible(&sync->waitq);
out:
	up(&sync->sem);
	
	return VFI_RESULT(0);
}

static int vfi_local_sync_wait(struct vfi_sync *sync, struct vfi_desc_param *desc)
{
	if (down_interruptible(&sync->sem))
		return VFI_RESULT(-ERESTARTSYS);
	
	if (sync->count == 0)
		sync->count += desc->offset;

	if (sync->count == 0)
		goto out;

	sync->count--;

	if (sync->count == 0)
		wake_up_interruptible(&sync->waitq);
	else
		while (sync->count) {
			up(&sync->sem);

			if (wait_event_interruptible(sync->waitq, (sync->count == 0)))
				return VFI_RESULT(-ERESTARTSYS);

			if (down_interruptible(&sync->sem))
				return VFI_RESULT(-ERESTARTSYS);
		}
out:
	up(&sync->sem);

	return VFI_RESULT(0);
}

struct vfi_ops vfi_local_ops = {
	.location_create = vfi_local_location_create,
	.location_delete = vfi_local_location_delete,
	.location_find   = vfi_local_location_find,
	.location_put    = vfi_local_location_put,
	.location_lose	 = vfi_local_location_lose,
	.smb_create      = vfi_local_smb_create,
	.smb_delete      = vfi_local_smb_delete,
	.smb_find        = vfi_local_smb_find,
	.smb_put         = vfi_local_smb_put,
	.smb_lose        = vfi_local_smb_lose,
	.xfer_create     = vfi_local_xfer_create,
	.xfer_delete     = vfi_local_xfer_delete,
	.xfer_find       = vfi_local_xfer_find,
	.xfer_put        = vfi_local_xfer_put,
	.xfer_lose       = vfi_local_xfer_lose,
	.sync_create     = vfi_local_sync_create,
	.sync_delete     = vfi_local_sync_delete,
	.sync_find       = vfi_local_sync_find,
	.sync_put        = vfi_local_sync_put,
	.sync_send       = vfi_local_sync_send,
	.sync_wait       = vfi_local_sync_wait,
	.mmap_create     = vfi_local_mmap_create,
	.mmap_delete     = vfi_local_mmap_delete,
	.mmap_find       = vfi_local_mmap_find,
	.mmap_put        = vfi_local_mmap_put,
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
	.bind_lose       = vfi_local_bind_lose, 
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

