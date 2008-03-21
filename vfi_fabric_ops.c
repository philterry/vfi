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

#define MY_DEBUG      RDDMA_DBG_FABOPS | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_FABOPS | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#define MY_ERROR(x) (0x80000000 | 0x0001 | ((x) & 0xffff))

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
#include <linux/vfi_event.h>
#include <linux/device.h>
#include <linux/mm.h>

/*
 * F I N D    O P E R A T I O N S
 */
extern void rddma_dma_chain_dump(struct list_head *h);
extern void bind_param_dump(struct rddma_bind_param *);

static int rddma_fabric_location_find(struct rddma_location **newloc, struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *oldloc = NULL ;
	struct rddma_location *myloc = NULL;
	int ret;

	*newloc = NULL;

	RDDMA_DEBUG(MY_DEBUG,"%s %p %p %s,%s\n",__FUNCTION__,loc,desc,desc->name,desc->location);

	ret = find_rddma_name(&oldloc,loc,desc);
	if (ret)
		return ret;

	if (loc) {
		ret = rddma_fabric_call(&skb, loc, 5, "location_find://%s.%s", desc->name,desc->location);
	}
	else {
		ret = new_rddma_location(&myloc,NULL,desc);
		if (ret)
			return ret;
		ret = rddma_fabric_call(&skb, myloc, 5, "location_find://%s", desc->name);
		rddma_location_put(myloc);
	}

	RDDMA_DEBUG(MY_DEBUG,"%s skb(%p)\n",__FUNCTION__,skb);

	if (skb) {
		struct rddma_desc_param reply;

		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0 && reply.extent) {
				if (loc) {
					if (loc->desc.extent) {
						desc->offset = reply.offset;
						desc->extent = loc->desc.extent;
					}
					else {
						desc->offset = reply.offset;
						loc->desc.extent = desc->extent = reply.offset;
						desc->ops = &rddma_local_ops;
					}
				}
				else {
					desc->offset = reply.offset;
					desc->extent = 0;
				}

				rddma_clean_desc(&reply);

				if (oldloc) {
					oldloc->desc.extent = desc->extent;
					oldloc->desc.offset = desc->offset;
					*newloc = oldloc;
					return 0;
				}

				ret = rddma_location_create(&myloc,loc,desc);
				if (ret)
					return ret;
				if (loc && loc->desc.address)
					loc->desc.address->ops->register_location(myloc);
				*newloc = myloc;
				return 0;
			}
			rddma_clean_desc(&reply);
		}
	}

	return MY_ERROR(__LINE__);
}

static void rddma_fabric_location_put(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *oldloc;
	struct rddma_location *myloc = NULL;
	int ret;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	ret = find_rddma_name(&oldloc,loc,desc);

	if (ret || !oldloc)
		return;

	if (loc) {
		ret = rddma_fabric_call(&skb, loc, 5, "location_put://%s.%s", desc->name,desc->location);
	}
	else {
		ret = new_rddma_location(&myloc,NULL,desc);
		if (ret)
			return;
		ret = rddma_fabric_call(&skb,myloc, 5, "location_put://%s", desc->name);
		rddma_location_put(myloc);
		if (ret)
			return;
	}
	
	RDDMA_DEBUG(MY_DEBUG,"%s skb(%p)\n",__FUNCTION__,skb);

	if (skb) {
		struct rddma_desc_param reply;

		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0 ) {
				rddma_location_put(oldloc);
			}
			rddma_clean_desc(&reply);
		}
	}
	
	return;
}

int rddma_fabric_smb_find(struct rddma_smb **smb, struct rddma_location *parent, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	int ret;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	if ( (*smb = to_rddma_smb(kset_find_obj(&parent->smbs->kset,desc->name))) )
		return 0;

	ret = rddma_fabric_call(&skb, parent, 5, "smb_find://%s.%s", desc->name,desc->location);

	if (skb) {
		struct rddma_desc_param reply;
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0)
				ret = rddma_smb_create(smb,parent,&reply);
			rddma_clean_desc(&reply);
		}
	}

	return ret;
}

static int rddma_fabric_mmap_find(struct rddma_mmap **mmap, struct rddma_smb *parent, struct rddma_desc_param *desc)
{
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	return -EINVAL;
}

/**
* rddma_fabric_xfer_find - find, or create, an rddma_xfer object for a named xfer at a remote location
* @loc	: location where xfer officially resides
* @desc	: target xfer parameter descriptor
*
* This function finds an rddma_xfer object for the xfer described by @desc, which officially resides
* at a remote fabric location defined by @loc.
*
* If the xfer is found to exist at that site then a stub will be created for the xfer in the local tree.
*
* The function returns a pointer to the rddma_xfer object that represents the target xfer in the
* local tree. It will return NULL if no such xfer exists at the remote site.
*
**/
static int rddma_fabric_xfer_find(struct rddma_xfer **xfer, struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	int ret;

	RDDMA_DEBUG (MY_DEBUG, "%s (%s)\n", __func__, ((desc) ? ((desc->name) ? : "<UNK>") : "<NULL>"));

	/*
	* Look for an existing stub for the target xfer in the local tree.
	* 
	*/
	if ( (*xfer = to_rddma_xfer(kset_find_obj(&loc->xfers->kset,desc->name))) )
		return 0;

	/*
	* If no such xfer object currently exists at that site, then deliver an 
	* "xfer_find" request to the [remote] destination to look for it there.
	*/
	ret = rddma_fabric_call(&skb, loc, 5, "xfer_find://%s.%s",
				desc->name,desc->location
				);
	/*
	* We need a reply...
	*/
	if (skb) {
		struct rddma_desc_param reply;
		ret = MY_ERROR(__LINE__);
		/*
		* Parse the reply into a descriptor...
		*/
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			/*
			* ...and if the reply indicates success, create a local xfer object
			* and bind it to the location. Thus our sysfs tree now has local knowledge
			* of this xfer's existence, though it live somewhere else.
			*/
			if ( (sscanf(rddma_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0) {
				reply.extent = 0;
				reply.offset = 0;
				ret =  rddma_xfer_create(xfer,loc,&reply);
			}
			rddma_clean_desc(&reply);
		}
	}

	return ret;
}

/**
* rddma_fabric_bind_find - find a bind belonging to a specified [remote] xfer
* @parent : pointer to <xfer> object that bind belongs to
* @desc   : <xfer> specification string
*
* Let's reiterate a point made in comments at the local version of this function: the bind
* we are seeking is not implicitly named. Rather we construct a name using the offset and extent
* values of the xfer it belongs to... values which, given the dynamic nature of xfer offset
* and extent, the originator of the bind search should have frigged to match the dimensions of the
* bind to be found. You pays your money, you takes your chance.
*
* What we do know for sure is that if the bind object exists at all, it does so at the <xfer> site.
* and that is where we deliver this bind_find instruction.
*
* The function uses the reply it receives from <xfer> to create a stub for the bind in the local tree.
* It returns a pointer to that object to the caller.
*
**/
static int rddma_fabric_bind_find(struct rddma_bind **bind, struct rddma_xfer *parent, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	char buf[128];
	int ret;
	*bind = NULL;

	snprintf(buf,128,"#%llx:%x",desc->offset,desc->extent);

	RDDMA_DEBUG(MY_DEBUG,"%s parent(%p) desc(%p) bind(%s)\n",__FUNCTION__,parent,desc,buf);

	if ( (*bind = to_rddma_bind(kset_find_obj(&parent->binds->kset,buf))) )
		return 0;

	ret = rddma_fabric_call(&skb, parent->desc.ploc, 5, "bind_find://%s.%s#%llx:%x",
				desc->name,desc->location,desc->offset,desc->extent);
	/*
	* What we get back in reply should be a complete specification of the bind, 
	* including its <xfer>, <dst>, and <src> components. This allows us to instantiate
	* a local stub for the bind (just the bind) within our object tree.
	*/
	if (skb) {
		struct rddma_bind_param reply;
		ret = MY_ERROR(__LINE__);
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0)
				ret = rddma_bind_create(bind,parent,&reply);
			rddma_clean_bind(&reply);
		}
	}

	return ret;
}

extern int rddma_local_dst_find(struct rddma_dst **, struct rddma_bind *, struct rddma_bind_param *);
static int rddma_fabric_dst_find(struct rddma_dst **dst, struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	int ret;
	struct rddma_location *loc = parent->desc.dst.ploc;
	ret = rddma_local_dst_find(dst,parent,desc);

	RDDMA_DEBUG(MY_DEBUG,"%s parent(%p) desc(%p) dst(%p)\n",__FUNCTION__,parent,desc,dst);

	if (*dst)
		return 0;

	ret = rddma_fabric_call(&skb,loc, 5, "dst_find://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent);
	if (skb) {
		struct rddma_bind_param reply;
		ret = -EINVAL;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0)
				ret = rddma_dst_create(dst,parent,&reply);
			rddma_clean_bind(&reply);
		}
	}

	return ret;
}

static int rddma_fabric_src_find(struct rddma_src **src, struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc = parent->desc.src.ploc;
	int ret;

	*src = NULL;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	ret = rddma_fabric_call(&skb,loc, 5, "src_find://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent);
	if (skb) {
		struct rddma_bind_param reply;
		ret = -EINVAL;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0)
				ret = rddma_src_create(src,parent,&reply);
			rddma_clean_bind(&reply);
		}
	}

	return ret;
}

/*
 * C R E A T E     O P E R A T I O N S
 */
static int rddma_fabric_location_create(struct rddma_location **newloc,struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb = NULL;
	int ret;
	*newloc = NULL;

	RDDMA_DEBUG(MY_DEBUG,"%s entered\n",__FUNCTION__);

	if (!loc)
		return new_rddma_location(newloc,NULL,desc);

	ret = rddma_fabric_call(&skb, loc, 5, "location_create://%s.%s#%llx:%x",
				desc->name, desc->location, desc->offset, loc->desc.extent);

	if (skb) {
		struct rddma_desc_param reply;
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0) {
				desc->extent = reply.offset;
				desc->offset = reply.extent;

				if (desc->extent == desc->offset) {
					desc->extent = loc ? loc->desc.extent : 0;
				}
				else {
					desc->offset = desc->extent;
					desc->ops = &rddma_local_ops;
					if (loc)
						loc->desc.extent = desc->extent;
				}
				
				ret = rddma_location_create(newloc,loc,desc);
				if (*newloc && (*newloc)->desc.address)
					(*newloc)->desc.address->ops->register_location(*newloc);
			}
			rddma_clean_desc(&reply);
		}
	}

	return *newloc != 0;
}

/**
* rddma_fabric_smb_create - create SMB at remote location
* @loc
* @desc
*
* This function implements "smb_create" when the specified location
* for the buffer is some other node on the RDDMA network, reachable
* through the interconnect fabric.
*
**/
static int rddma_fabric_smb_create(struct rddma_smb **smb, struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	int ret;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	ret = rddma_fabric_call(&skb, loc, 5, "smb_create://%s.%s#%llx:%x", desc->name,desc->location,desc->offset, desc->extent);
	if (skb) {
		struct rddma_desc_param reply;
		ret = MY_ERROR(__LINE__);
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0)
				ret = rddma_smb_create(smb,loc,&reply);
			rddma_clean_desc(&reply);
		}
	}

	return ret;
}

static int rddma_fabric_mmap_create(struct rddma_mmap **mmap,struct rddma_smb *smb, struct rddma_desc_param *desc)
{
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	return -EINVAL;
}

/**
* rddma_fabric_bind_create
* @parent : pointer to <xfer> object that bind belongs to
* @desc   : <xfer> specification string
*
* bind_create must always run on a bind <xfer> agent.
*
* This function delivers bind_create to the <xfer> agent specified new bind. On return it will
* create a local stub for the bind in the object tree slung beneath the local stub of its parent
* <xfer>.
*
* The <xfer> object MUST exist in the local tree before this function is called.
*
**/
static int rddma_fabric_bind_create(struct rddma_bind **bind, struct rddma_xfer *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	int ret;
	struct rddma_location *loc = parent->desc.ploc;	/* Parent xfer location */
	*bind = NULL;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	ret = rddma_fabric_call(&skb,loc, 5, "bind_create://%s.%s#%llx:%x/%s.%s#%llx:%x?event_name(%s)=%s.%s#%llx:%x?event_name(%s)",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,rddma_get_option(&desc->dst,"event_name"),
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent,rddma_get_option(&desc->src,"event_name"));
	/*
	* Provided we receive a reply, one that can be parsed into a bind descriptor 
	* AND that indicates success at the remote site, then we may create a local
	* stub for the bind and insert it into our local tree.
	*/
	if (skb) {
		struct rddma_bind_param reply;
		ret = MY_ERROR(__LINE__);
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0)
				ret = rddma_bind_create(bind,parent,&reply);
			rddma_clean_bind(&reply);
		}
	}

	return ret;
}

static int rddma_fabric_xfer_create(struct rddma_xfer **xfer, struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	int ret;

	*xfer = NULL;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	ret = rddma_fabric_call(&skb,loc, 5, "xfer_create://%s.%s#%llx:%x",
				desc->name,desc->location,desc->offset,desc->extent
				);
	if (skb) {
		struct rddma_desc_param reply;
		ret = MY_ERROR(__LINE__);
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0)
				ret = rddma_xfer_create(xfer,loc,&reply);
			rddma_clean_desc(&reply);
		}
	}

	return ret;
}

/**
* rddma_fabric_dst_events - create <dst> events when <xfer> is remote
* @bind - bind object, subject of operation
* @desc - descriptor of the bind
*
* This function creates two <dst> events - dst_ready, and dst_done - when the
* bind <xfer> and <dst> are located at different network locations.
*
* It is always the <xfer> dst_events() op that is invoked, but this function always
* runs at the <dst> site, and is only ever invoked by rddma_local_dsts_create().
*
* Since <xfer> and <dst> occupy different locations, events are raised by delivering
* doorbells:
*
* - <xfer> will issue the dst_ready doorbell whose identity is embedded in @desc as "event_id(x)"
* - <dst> will raise the dst_done doorbell whose identity is allocated here, and reported to <xfer>
*   in fabric command replies.
*
**/
static int rddma_fabric_dst_events(struct rddma_bind *bind, struct rddma_bind_param *desc)
{
	/* Local destination SMB with a remote transfer agent. */
	char *event_str;
	char *event_name;
	int event_id = -1;
	struct rddma_events *event_list;
	int ret;
	
	RDDMA_DEBUG(MY_DEBUG,"%s bind(%p) desc(%p)\n",__FUNCTION__,bind,desc);

	bind->dst_ready_event = NULL;
	
	/*
	* The common event name is embedded in the command string as an
	* "event_name(n)" qualifier. The dst_ready doorbell identifier, allocated
	* by <xfer>, is embedded in the command as an "event_id(x)" qualifier.
	*
	*/
	event_str = rddma_get_option(&desc->dst,"event_id");
	event_name = rddma_get_option(&desc->dst,"event_name");

	if (event_str && event_name) {
		sscanf(event_str,"%d",&event_id);

		ret = find_rddma_events(&event_list,rddma_subsys->events, event_name);
		if (event_list == NULL)
			ret = rddma_events_create(&event_list,rddma_subsys->events,event_name);

		ret = rddma_event_create(&bind->dst_ready_event,event_list,&desc->xfer,bind,bind->desc.xfer.ops->dst_ready,event_id);
		bind->dst_ready_event_id = event_id;
		
		/*
		* The dst_done event identifier must be obtained by registering a new
		* doorbell. The event id is returned by the registration function.
		*
		*/
		event_id = rddma_doorbell_register(bind->desc.xfer.address,
						   (void (*)(void *))bind->desc.dst.ops->dst_done,
						   (void *)bind);

		ret = rddma_event_create(&bind->dst_done_event,event_list,&desc->dst,bind,0,event_id);
		bind->dst_done_event_id = event_id;
		return 0;
	}
	return -EINVAL;
}

/**
* rddma_fabric_dst_ev_delete - delete <dst> events when <xfer> is remote
* @bind : parent bind object
* @desc : bind descriptor
*
* This function complements rddma_fabric_dst_events() by providing the means to
* delete two <dst> events when <xfer> lives elsewhere on the network.
*
* It is always the <xfer> dst_ev_delete() op that is invoked, but this function always
* runs at the <dst> site, and is only ever invoked by rddma_local_dsts_delete().
*
**/
static void rddma_fabric_dst_ev_delete (struct rddma_bind *bind, struct rddma_bind_param *desc)
{
	RDDMA_DEBUG (MY_DEBUG, "%s: for %s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x\n", __func__,
	             desc->xfer.name, desc->xfer.location, desc->xfer.offset, desc->xfer.extent,
	             desc->dst.name, desc->dst.location, desc->dst.offset, desc->dst.extent,
	             desc->src.name, desc->src.location, desc->src.offset, desc->src.extent);
	RDDMA_DEBUG (MY_DEBUG, "dst_ready (%p, %08x), dst_done (%p, %08x)\n",
	             bind->dst_ready_event, bind->dst_ready_event_id,
	             bind->dst_done_event, bind->dst_done_event_id);
	rddma_event_delete (bind->dst_ready_event);
	rddma_event_delete (bind->dst_done_event);
	rddma_doorbell_unregister (bind->desc.xfer.address, bind->dst_done_event_id);
	bind->dst_ready_event = bind->dst_done_event = NULL;
	bind->dst_ready_event_id = bind->dst_done_event_id = 0;
}

/**
* rddma_fabric_dsts_create
* @parent : parent bind object
* @desc	  : bind descriptor
*
* dsts_create always runs on the bind <dst> location.
*
**/
static int rddma_fabric_dsts_create(struct rddma_dsts **dsts, struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc = parent->desc.dst.ploc;
	int event_id = -1;
	struct rddma_bind_param reply;
	int ret = -EINVAL;
	char *event_str;
	char *dst_event_name;
	char *src_event_name;

	/*
	* Both the dst and the src must be bound to an event, identified
	* by name as part of the bind descriptor.
	*
	*/
	dst_event_name = rddma_get_option(&parent->desc.dst,"event_name");
	src_event_name = rddma_get_option(&parent->desc.src,"event_name");

	if (dst_event_name == NULL || src_event_name == NULL)
		goto event_fail;

	event_id = rddma_doorbell_register(parent->desc.xfer.address,
					   (void (*)(void *))parent->desc.xfer.ops->dst_ready,
					   (void *)parent);
	if (event_id < 0)
		goto event_fail;

	/*
	* Create the dsts object, then send dsts_create to
	* the remote <dst> site.
	*
	* We do NOT build-out the dsts subtree further; at least
	* not directly. The only way we will ever see a subtree is
	* if the <src> location turns out to be here.
	* 
	* CAUTION:
	* --------
	* If this function is called from <xfer> during local_bind_create,
	* as should always happen, then rddma_dsts_create will ALREADY have
	* been called and succeeded as a pre-condition to invoking this 
	* dst->dsts_create fabric call.
	*
	* It happens that rddma_dsts_create will NOT attempt to create a duplicate
	* <dsts>, but it will re-register the existing <dsts>, doubling-up on 
	* associated counts.
	*/
	ret = rddma_dsts_create(dsts,parent,desc);

	if (ret || *dsts == NULL)
		goto dsts_fail;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	ret = rddma_fabric_call(&skb, loc, 5, "dsts_create://%s.%s#%llx:%x/%s.%s#%llx:%x?event_id(%d),event_name(%s)=%s.%s#%llx:%x?event_name(%s)",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,
				event_id,dst_event_name,
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent,
				src_event_name);
	/*
	* On return...
	*
	* We do need to parse the reply, provided it indicates a successful
	* creation. The reply will specify the bind in terms of its <xfer>, <dst>, 
	* and <src> components, but should include the identity of any event
	* associated with the <dst> component. This is the identity of the event
	* the <dst> location will raise when the <dst> part of a bind has completed.
	*/
	if (skb == NULL)
		goto skb_fail;
	
	if (rddma_parse_bind(&reply,skb->data)) 
			goto parse_fail;

	if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret != 0)
		goto result_fail;

	event_str = rddma_get_option(&reply.dst,"event_id");

	if (event_str == NULL)
		goto result_fail;

	if (sscanf(event_str,"%d",&parent->dst_done_event_id) != 1)
		goto result_fail;

	parent->dst_ready_event_id = event_id;

	rddma_bind_load_dsts(parent);

	if (rddma_debug_level & RDDMA_DBG_DMA_CHAIN)
		rddma_dma_chain_dump(&parent->dma_chain);

	parent->end_of_chain = parent->dma_chain.prev;

	dev_kfree_skb(skb);
	rddma_clean_bind(&reply);
	return 0;

result_fail:
	rddma_clean_bind(&reply);
parse_fail:
	dev_kfree_skb(skb);
skb_fail:
	rddma_dsts_delete(*dsts);
dsts_fail:
	rddma_doorbell_unregister(parent->desc.xfer.address, event_id);
event_fail:
	return MY_ERROR(__LINE__);
}

/**
* rddma_fabric_dst_create
* @parent : bind object that dst belongs to
* @desc   : bind descriptor, identifying <xfer>, <dst>, and <src> elements of the bind.
*
* This function is invoked to deliver a dst_create command to a bind <xfer> agent.
*
* dst_create always runs on the bind <xfer> agent.
*
**/
static int rddma_fabric_dst_create(struct rddma_dst **dst, struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc = parent->desc.xfer.ploc;	/* dst_create runs on <xfer> */
	char *src_event_name;
	int ret = -EINVAL;
	*dst = NULL;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	src_event_name = rddma_get_option(&desc->src,"event_name");

	if (src_event_name == NULL)
		goto fail_event;

	ret = rddma_fabric_call(&skb,loc, 5, "dst_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?event_name(%s)",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent,
				src_event_name);
	if (skb) {
		struct rddma_bind_param reply;
		ret = -EINVAL;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0) {
				ret = rddma_local_dst_find(dst,parent,&reply);
				if (NULL == *dst)
					ret = rddma_dst_create(dst,parent,&reply);
			}
			rddma_clean_bind(&reply);
		}
	}

fail_event:
	return ret;
}

/**
* rddma_fabric_src_events
* @parent : pointer to <dst> object that bind belongs to
* @desc   : <bind> specification string
*
* src_events must always run on a bind <xfer> agent.
*
**/
static int rddma_fabric_src_events(struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	/* Local source SMB with a remote transfer agent. */
	struct rddma_bind *bind;
	char *event_str;
	char *event_name;
	int event_id = -1;
	struct rddma_events *event_list;
	int ret;

	bind = rddma_dst_parent(parent);

	if (bind->src_ready_event)
		return 0;
	
	event_str = rddma_get_option(&desc->src,"event_id");
	event_name = rddma_get_option(&desc->src,"event_name");

	if (event_str && event_name) {
		sscanf(event_str,"%d",&event_id);

		ret = find_rddma_events(&event_list,rddma_subsys->events, event_name);
		if (event_list == NULL)
			ret = rddma_events_create(&event_list,rddma_subsys->events,event_name);

		ret = rddma_event_create(&bind->src_ready_event,event_list,&desc->xfer,bind,bind->desc.xfer.ops->src_ready,event_id);
		bind->src_ready_event_id = event_id;
		
		event_id = rddma_doorbell_register(bind->desc.xfer.address,
						   (void (*)(void *))bind->desc.src.ops->src_done,
						   (void *)bind);

		ret = rddma_event_create(&bind->src_done_event, event_list,&desc->src,bind,0,event_id);
		bind->src_done_event_id = event_id;
		return 0;
	}
	return -EINVAL;
}

/**
*
*
*
**/
static void rddma_fabric_src_ev_delete (struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	struct rddma_bind *bind = rddma_dst_parent (parent);
	
	RDDMA_DEBUG (MY_DEBUG, "%s: for %s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x\n", __func__,
	             desc->xfer.name, desc->xfer.location, desc->xfer.offset, desc->xfer.extent,
	             desc->dst.name, desc->dst.location, desc->dst.offset, desc->dst.extent,
	             desc->src.name, desc->src.location, desc->src.offset, desc->src.extent);
	RDDMA_DEBUG (MY_DEBUG, "src_ready (%p, %08x), src_done (%p, %08x)\n",
	             bind->src_ready_event, bind->src_ready_event_id,
	             bind->src_done_event, bind->src_done_event_id);
	rddma_event_delete (bind->src_ready_event);
	rddma_event_delete (bind->src_done_event);
	rddma_doorbell_unregister (bind->desc.xfer.address, bind->src_done_event_id);
	bind->src_ready_event = bind->src_done_event = NULL;
	bind->src_ready_event_id = bind->src_done_event_id = 0;
}




/**
* rddma_fabric_srcs_create
* @parent : pointer to <bind> object that srcs belongs to
* @desc   : <bind> specification string
*
*
* srcs_create must always run on a bind <src> agent, with a fabric call
* placed by the <xfer> agent.
*
**/
static int rddma_fabric_srcs_create(struct rddma_srcs **srcs, struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *sloc = parent->desc.src.ploc;
	int event_id = -1 ;
	struct rddma_bind *bind;
	struct rddma_bind_param reply;
	int ret = -EINVAL;
	char *event_str;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	event_str = rddma_get_option(&desc->src,"event_name");
	
	if (event_str == NULL)
		goto out;

	bind = rddma_dst_parent(parent);

	if (bind == NULL)
		goto out;

	if (bind->src_ready_event == NULL) {
		bind->src_ready_event_id = event_id = rddma_doorbell_register(bind->desc.xfer.address,
									   (void (*)(void *))bind->desc.xfer.ops->src_ready,
									   (void *)bind);
		if (event_id < 0)
			goto event_fail;
	}

	ret = rddma_srcs_create(srcs,parent,desc);

	if (*srcs == NULL)
		goto srcs_fail;

	ret = rddma_fabric_call(&skb,sloc, 5, "srcs_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?event_name(%s),event_id(%d)",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent,event_str,bind->src_ready_event_id);
	if (ret)
		goto skb_fail;

	ret = -EINVAL;

	if (skb == NULL)
		goto skb_fail;

	if (rddma_parse_bind(&reply,skb->data)) 
		goto parse_fail;

	if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret != 0)
		goto result_fail;

	if (bind->src_done_event_id < 0) {
		event_str = rddma_get_option(&reply.src,"event_id");

		if (event_str == NULL)
			goto result_fail;

		if (sscanf(event_str,"%d",&bind->src_done_event_id) != 1)
			goto result_fail;
	}

	rddma_dst_load_srcs(parent);

	rddma_clean_bind(&reply);
	dev_kfree_skb(skb);
	return ret;

result_fail:
	rddma_clean_bind(&reply);
parse_fail:
	dev_kfree_skb(skb);
skb_fail:
	rddma_srcs_delete(*srcs);
srcs_fail:
	if (event_id != -1)
		rddma_doorbell_unregister(bind->desc.xfer.address,event_id);
event_fail:
	rddma_bind_put(bind);
out:
	return ret;
}

/**
* rddma_fabric_src_create
* @parent : pointer to <dst> object that src belongs to
* @desc   : <bind> specification string
*
* src_create must always run on a bind <xfer> agent, and should be invoked
* by the <src> agent while constructing <srcs> for a specific <dst>.
*
**/
static int rddma_fabric_src_create(struct rddma_src **src, struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *sloc = parent->desc.xfer.ploc;	/* src_create runs on <xfer> */
	int ret;

	*src = NULL;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	ret = rddma_fabric_call(&skb,sloc, 5, "src_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent);
	ret = -EINVAL;
	
	if (skb) {
		struct rddma_bind_param reply;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0)
				ret = rddma_src_create(src,parent,&reply);
			rddma_clean_bind(&reply);
		}
	}

	return ret;
}

/*
 * D E L E T E    O P E R A T I O N S
 */
static void rddma_fabric_location_delete(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	int ret;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	rddma_fabric_call(&skb,loc, 5, "location_delete://%s.%s", desc->name,desc->location);

	if (skb) {
		struct rddma_desc_param reply;
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0)
				rddma_location_delete(loc);
			rddma_clean_desc(&reply);
		}
	}
}

static void rddma_fabric_smb_delete(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_smb *smb;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
	RDDMA_DEBUG(MY_DEBUG,"%s (%s.%s)\n",__FUNCTION__, desc->name, desc->location);

	if (NULL == (smb = to_rddma_smb(kset_find_obj(&loc->smbs->kset,desc->name))) )
		return;

	rddma_fabric_call(&skb, loc, 5, "smb_delete://%s.%s", desc->name, desc->location);
	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0)
				rddma_smb_delete(smb);
			rddma_clean_desc(&reply);
		}
	}
}

static void rddma_fabric_mmap_delete(struct rddma_smb *smb, struct rddma_desc_param *desc)
{
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

}

/**
* rddma_fabric_bind_delete - send bind_delete request to remote Xfer agent
* @xfer : pointer to xfer to be deleted, with ploc identifying <xfer> agent location
* @desc : descriptor of the bind to be deleted
*
* bind_delete must always run on the bind <xfer> agent.
*
* This function is invoked to deliver a bind_delete request to a remote bind <xfer> agent.
* Upon successful conclusion of the remote op, it will then delete the local instance of
* the subject bind.
*
**/
static void rddma_fabric_bind_delete(struct rddma_xfer *xfer, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc = xfer->desc.ploc;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	/*
	* Send bind_delete request to remote <xfer> agent using the 
	* offset and extent values specified in the descriptor. 
	*
	*/
	rddma_fabric_call(&skb,loc, 5, "bind_delete://%s.%s#%llx:%x",
				desc->name, desc->location, desc->offset, desc->extent);
	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0)
				rddma_bind_delete(xfer, &reply);
			rddma_clean_desc(&reply);
		}
	}
}
static void rddma_fabric_xfer_delete(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	rddma_fabric_call(&skb,loc, 5, "xfer_delete://%s.%s", desc->name,desc->location);
	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0)
				rddma_xfer_delete(loc,&reply);
			rddma_clean_desc(&reply);

		}
	}
}

/**
* rddma_fabric_dst_delete
* @parent : pointer to <bind> object that dst belongs to
* @desc   : <bind> specification string
*
* dst_delete must always run on the bind <xfer> agent, and ought to be invoked while
* executing dsts_delete on the <dst> agent.
*
**/
static void rddma_fabric_dst_delete(struct rddma_bind *bind, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc = bind->desc.xfer.ploc;	/* dst_delete runs on <xfer> */
	
	/*
	* HACK ALERT:
	* -----------
	* When deleting <dst> over fabric, we want to place a hold on deleting the local
	* instance until we return. We want to do this just in case both <dst> and <src>
	* are co-located somewhere outside the <xfer> agent - in which case an srcs_delete
	* will be sent back here meantime that we do NOT want to destroy the hierarchy that
	* we are currently working on.
	*
	* A simple find is sufficient to bump-up the refcount. We shall undo it in a moment.
	*
	*/
	struct rddma_dst *dst = NULL;
	int ret;
	
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
	RDDMA_KTRACE ("<*** %s find dst before fabric call IN ***>\n", __func__);
	find_rddma_dst_in (&dst,bind, desc);
	RDDMA_KTRACE ("<*** %s find dst before fabric call OUT ***>\n", __func__);
	
	rddma_fabric_call(&skb,loc, 5, "dst_delete://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x", 
	                        desc->xfer.name, desc->xfer.location, desc->xfer.offset, desc->xfer.extent,
	                        desc->dst.name, desc->dst.location, desc->dst.offset, desc->dst.extent,
	                        desc->src.name, desc->src.location, desc->src.offset, desc->src.extent);
	
	if (dst) rddma_dst_put (dst);
	if (skb) {
		struct rddma_bind_param reply;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0)
				rddma_dst_delete(bind,&reply);
			rddma_clean_bind(&reply);
		}
	}
}

/**
* rddma_fabric_src_delete
* @parent : pointer to <dst> object that src belongs to
* @desc   : <bind> specification string
*
* src_delete must always run on the bind <xfer> agent
**/
static void rddma_fabric_src_delete(struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc = parent->desc.xfer.ploc;
	int ret;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);
/*
	printk ("-- Parent: %s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x\n", 
		parent->desc.xfer.name, parent->desc.xfer.location, parent->desc.xfer.offset, parent->desc.xfer.extent,
	        parent->desc.dst.name, parent->desc.dst.location, parent->desc.dst.offset, parent->desc.dst.extent,
	        parent->desc.src.name, parent->desc.src.location, parent->desc.src.offset, parent->desc.src.extent);
	printk ("--   Desc: %s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x\n", 
		desc->xfer.name, desc->xfer.location, desc->xfer.offset, desc->xfer.extent,
	        desc->dst.name, desc->dst.location, desc->dst.offset, desc->dst.extent,
	        desc->src.name, desc->src.location, desc->src.offset, desc->src.extent);
*/
	
	/*
	* Compose and deliver src_delete command.
	*
	* Note the unusual composition of the bind parameters to be embedded in the command; how the <dst>
	* section is built from the parent spec, while <xfer> and <src> are built from the descriptor explicitly
	* passed to us. That is a bugfix to ensure that <xfer> is sent the same source bind specification on
	* src_delete that it was with src_create - when <src> is remote, the <dst> component in the explicit 
	* bind descriptor contains a base <dst> offset, not <dst> SMB address. 
	*
	* I have no idea how the discrepancy arose - probably because the srcs_delete code does not perform
	* xfer/bind/dst find calls the way that srcs_create does? 
	*
	*/
	rddma_fabric_call(&skb,loc, 5, "src_delete://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x", 
	                        desc->xfer.name, desc->xfer.location, desc->xfer.offset, desc->xfer.extent,
	                        parent->desc.dst.name, parent->desc.dst.location, parent->desc.dst.offset, parent->desc.dst.extent,
	                        desc->src.name, desc->src.location, desc->src.offset, desc->src.extent);
	
	if (skb) {
		struct rddma_bind_param reply;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0)
				rddma_src_delete(parent,&reply);
			rddma_clean_bind(&reply);
		}
	}
	
}

/**
* rddma_fabric_srcs_delete
* @parent : pointer to <dst> object that srcs belong to
* @desc   : <bind> specification string
*
* srcs_delete must always run on the bind <src> agent.
*
**/
static struct rddma_dst *rddma_fabric_srcs_delete(struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc = parent->desc.src.ploc;	/* srcs_delete runs on <src> */
	struct rddma_bind *bind = rddma_dst_parent (parent);
	int ret;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	rddma_fabric_call(&skb,loc, 5, "srcs_delete://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x",
				desc->xfer.name, desc->xfer.location, desc->xfer.offset, desc->xfer.extent,
				desc->dst.name,  desc->dst.location,  desc->dst.offset,  desc->dst.extent,
				desc->src.name,  desc->src.location,  desc->src.offset,  desc->src.extent);
	if (skb) {
		struct rddma_bind_param reply;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0) {
				rddma_doorbell_unregister (bind->desc.xfer.address, bind->src_ready_event_id);
				rddma_srcs_delete(parent->srcs);
			}
			rddma_clean_bind(&reply);
		}
	}
	return (parent);
}

/**
* rddma_fabric_dsts_delete - send dsts_delete request to bind <dst> agent
* @parent : pointer to bind object that is being deleted
* @desc   : descriptor of the bind being deleted
*
* dsts_delete must always run on a bind <dst> agent.
*
* This function, which runs as part of bind_delete, will deliver a dsts_delete request
* to the <dst> agent of a bind being deleted. It should ONLY be invoked at that time, as
* an integral part of bind_delete.
*
* The request is composed and delivered to the <dst> agent. Upon successful return, the local
* instance of <dsts> is deleted too.
*
**/
static struct rddma_bind *rddma_fabric_dsts_delete (struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc = parent->desc.dst.ploc;	/* dsts_delete runs on <dst> */

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	rddma_fabric_call(&skb,loc, 5, "dsts_delete://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x",
				desc->xfer.name, desc->xfer.location, desc->xfer.offset, desc->xfer.extent,
				desc->dst.name,  desc->dst.location,  desc->dst.offset,  desc->dst.extent,
				desc->src.name,  desc->src.location,  desc->src.offset,  desc->src.extent);
	if (skb) {
		struct rddma_bind_param reply;
		if (!rddma_parse_bind(&reply,skb->data)) {
			int ret = -EINVAL;
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0) {
				rddma_doorbell_unregister (parent->desc.xfer.address, parent->dst_ready_event_id);
				rddma_dsts_delete(parent->dsts);
			}
			rddma_clean_bind(&reply);
		}
	}
	return (parent);
}

static void rddma_fabric_done(struct rddma_event *event)
{
	/* Nothing to be done here mate */
}

static void rddma_fabric_src_done(struct rddma_bind *bind)
{
	struct rddma_fabric_address *address;
	
	/* Our local DMA engine, has completed a transfer involving a
	 * remote SMB as the source requiring us to send a done event
	 * to the remote source so that it may adjust its votes. */
	RDDMA_DEBUG(MY_DEBUG,"%s bind(%p)\n",__FUNCTION__,bind);
	rddma_bind_src_done(bind);
	address = (bind->desc.src.address) ? : ((bind->desc.src.ploc) ? bind->desc.src.ploc->desc.address : NULL);
	rddma_doorbell_send(address, bind->src_done_event_id);
}

static void rddma_fabric_dst_done(struct rddma_bind *bind)
{
	struct rddma_fabric_address *address;
	
	/* Our local DMA engine, has completed a transfer involving a
	 * remote SMB as the destination requiring us to send a done
	 * event to the remote destination so that it may adjust its
	 * votes. */
	RDDMA_DEBUG(MY_DEBUG,"%s bind(%p)\n",__FUNCTION__,bind);
	rddma_bind_dst_done(bind);
	
	address = (bind->desc.dst.address) ? : ((bind->desc.dst.ploc) ? bind->desc.dst.ploc->desc.address : NULL);
	rddma_doorbell_send(address, bind->dst_done_event_id);
}

static void rddma_fabric_src_ready(struct rddma_bind *bind)
{
	/* Someone locally executed start on an event associated with
	 * the local source SMB in a bind assigned a remote DMA
	 * engine. So we need to send the ready event to it so that it
	 * may adjust its vote accordingly. */
	RDDMA_DEBUG(MY_DEBUG,"%s bind(%p)\n",__FUNCTION__,bind);
	rddma_bind_src_ready(bind);
	rddma_doorbell_send(bind->desc.xfer.address,bind->src_ready_event_id);
}

static void rddma_fabric_dst_ready(struct rddma_bind *bind)
{
	/* Someone locally executed start on an event associated with
	 * the local destination SMB in a bind assigned a remote DMA
	 * engine. So we need to send the ready event to it so that it
	 * may adjust its vote accordingly. */
	RDDMA_DEBUG(MY_DEBUG,"%s bind(%p)\n",__FUNCTION__,bind);
	rddma_bind_dst_ready(bind);
	rddma_doorbell_send(bind->desc.xfer.address,bind->dst_ready_event_id);
}

static int rddma_fabric_event_start(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	int ret = -EINVAL;

	RDDMA_DEBUG(MY_DEBUG,"%s loc(%p) desc(%p)\n",__FUNCTION__, loc, desc);

	rddma_fabric_call(&skb,loc, 5, "event_start://%s.%s", desc->name,desc->location);
	if (skb) {
		struct rddma_desc_param reply;
		if (!rddma_parse_desc(&reply,skb->data)) {
			sscanf(rddma_get_option(&reply,"result"),"%d",&ret);
			rddma_clean_desc(&reply);
		}
		dev_kfree_skb(skb);
	}
	return ret;
}

struct rddma_ops rddma_fabric_ops = {
	.location_create = rddma_fabric_location_create,
	.location_delete = rddma_fabric_location_delete,
	.location_find   = rddma_fabric_location_find,
	.location_put    = rddma_fabric_location_put,
	.smb_create      = rddma_fabric_smb_create,
	.smb_delete      = rddma_fabric_smb_delete,
	.smb_find        = rddma_fabric_smb_find,
	.mmap_create     = rddma_fabric_mmap_create,
	.mmap_delete     = rddma_fabric_mmap_delete,
	.mmap_find       = rddma_fabric_mmap_find,
	.xfer_create     = rddma_fabric_xfer_create,
	.xfer_delete     = rddma_fabric_xfer_delete,
	.xfer_find       = rddma_fabric_xfer_find,
	.srcs_create     = rddma_fabric_srcs_create,
	.srcs_delete     = rddma_fabric_srcs_delete,
	.src_create      = rddma_fabric_src_create,
	.src_delete      = rddma_fabric_src_delete,
	.src_find        = rddma_fabric_src_find,
	.dsts_create     = rddma_fabric_dsts_create,
	.dsts_delete     = rddma_fabric_dsts_delete,
	.dst_create      = rddma_fabric_dst_create,
	.dst_delete      = rddma_fabric_dst_delete,
	.dst_find        = rddma_fabric_dst_find,
	.bind_find       = rddma_fabric_bind_find,
	.bind_create     = rddma_fabric_bind_create,
	.bind_delete     = rddma_fabric_bind_delete,
	.src_done        = rddma_fabric_src_done,
	.dst_done        = rddma_fabric_dst_done,
	.done            = rddma_fabric_done,
	.src_ready       = rddma_fabric_src_ready,
	.dst_ready       = rddma_fabric_dst_ready,
	.src_events      = rddma_fabric_src_events,
	.dst_events      = rddma_fabric_dst_events,
	.src_ev_delete   = rddma_fabric_src_ev_delete,
	.dst_ev_delete   = rddma_fabric_dst_ev_delete,
	.event_start     = rddma_fabric_event_start,
};

