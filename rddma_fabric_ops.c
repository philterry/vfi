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
#include <linux/rddma_event.h>
#include <linux/device.h>
#include <linux/mm.h>

/*
 * F I N D    O P E R A T I O N S
 */
extern void rddma_dma_chain_dump(struct list_head *h);
extern void bind_param_dump(struct rddma_bind_param *);

static struct rddma_location *rddma_fabric_location_find(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *oldloc = NULL ;
	struct rddma_location *myloc = NULL;

	RDDMA_DEBUG(MY_DEBUG,"%s %p %p %s,%s\n",__FUNCTION__,loc,desc,desc->name,desc->location);

	oldloc = find_rddma_name(loc,desc);

	if (loc) {
		skb = rddma_fabric_call(loc, 5, "location_find://%s.%s", desc->name,desc->location);
	}
	else {
		myloc = new_rddma_location(NULL,desc);
		skb = rddma_fabric_call(myloc, 5, "location_find://%s", desc->name);
		rddma_location_put(myloc);
	}

	RDDMA_DEBUG(MY_DEBUG,"%s skb(%p)\n",__FUNCTION__,skb);

	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;

		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0 && reply.extent) {
				desc->extent = reply.offset;
				desc->offset = reply.extent;
				rddma_clean_desc(&reply);

				if (desc->extent == desc->offset) {
					desc->extent = loc ? loc->desc.extent : 0;
				}
				else {
					desc->offset = desc->extent;
					desc->ops = &rddma_local_ops;
					if (loc)
						loc->desc.extent = desc->extent;
				}
				
				if (oldloc) {
					oldloc->desc.extent = desc->extent;
					oldloc->desc.offset = desc->offset;
					return oldloc;
				}

				oldloc = rddma_location_create(loc,desc);
				if (loc && loc->desc.address)
					loc->desc.address->ops->register_location(oldloc);
				return oldloc;
			}
			rddma_clean_desc(&reply);
		}
	}

	return NULL;
}

static void rddma_fabric_location_put(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *oldloc;
	struct rddma_location *myloc = NULL;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	oldloc = find_rddma_name(loc,desc);

	if (!oldloc)
		return;

	if (loc) {
		skb = rddma_fabric_call(loc, 5, "location_put://%s.%s", desc->name,desc->location);
	}
	else {
		myloc = new_rddma_location(NULL,desc);
		skb = rddma_fabric_call(myloc, 5, "location_put://%s", desc->name);
		rddma_location_put(myloc);
	}
	
	RDDMA_DEBUG(MY_DEBUG,"%s skb(%p)\n",__FUNCTION__,skb);

	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;

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

static struct rddma_smb *rddma_fabric_smb_find(struct rddma_location *parent, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_smb *smb = NULL;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	if ( (smb = to_rddma_smb(kset_find_obj(&parent->smbs->kset,desc->name))) )
		return smb;

	skb = rddma_fabric_call(parent, 5, "smb_find://%s.%s", desc->name,desc->location);

	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0)
				smb = rddma_smb_create(parent,&reply);
			rddma_clean_desc(&reply);
		}
	}

	return smb;
}

static struct rddma_mmap *rddma_fabric_mmap_find(struct rddma_smb *parent, struct rddma_desc_param *desc)
{
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	return NULL;
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
static struct rddma_xfer *rddma_fabric_xfer_find(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_xfer *xfer;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	/*
	* Look for an existing stub for the target xfer in the local tree.
	* 
	*/
	if ( (xfer = to_rddma_xfer(kset_find_obj(&loc->xfers->kset,desc->name))) )
		return xfer;

	/*
	* If no such xfer object currently exists at that site, then deliver an 
	* "xfer_find" request to the [remote] destination to look for it there.
	*/
	skb = rddma_fabric_call(loc, 5, "xfer_find://%s.%s",
				desc->name,desc->location
				);
	/*
	* We need a reply...
	*/
	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;
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
				xfer =  rddma_xfer_create(loc,&reply);
			}
			rddma_clean_desc(&reply);
		}
	}

	return xfer;
}

static struct rddma_bind *rddma_fabric_bind_find(struct rddma_xfer *parent, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_bind *bind = NULL;
	char buf[128];
	snprintf(buf,128,"#%llx:%x",desc->offset,desc->extent);

	RDDMA_DEBUG(MY_DEBUG,"%s parent(%p) desc(%p) bind(%s)\n",__FUNCTION__,parent,desc,buf);

	if ( (bind = to_rddma_bind(kset_find_obj(&parent->binds->kset,buf))) )
		return bind;

	skb = rddma_fabric_call(parent->desc.ploc, 5, "bind_find://%s.%s#%llx:%x",
				desc->name,desc->location,desc->offset,desc->extent);
	if (skb) {
		struct rddma_bind_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0)
				bind = rddma_bind_create(parent,&reply);
			rddma_clean_bind(&reply);
		}
	}

	return bind;
}

extern struct rddma_dst *rddma_local_dst_find(struct rddma_bind *, struct rddma_bind_param *);
static struct rddma_dst *rddma_fabric_dst_find(struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc = parent->desc.dst.ploc;
	struct rddma_dst *dst = rddma_local_dst_find(parent,desc);

	RDDMA_DEBUG(MY_DEBUG,"%s parent(%p) desc(%p) dst(%p)\n",__FUNCTION__,parent,desc,dst);

	if (dst)
		return dst;

	skb = rddma_fabric_call(loc, 5, "dst_find://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent);
	if (skb) {
		struct rddma_bind_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0)
				dst = rddma_dst_create(parent,&reply);
			rddma_clean_bind(&reply);
		}
	}

	return dst;
}

static struct rddma_src *rddma_fabric_src_find(struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc = parent->desc.src.ploc;
	struct rddma_src *src = NULL;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	skb = rddma_fabric_call(loc, 5, "src_find://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent);
	if (skb) {
		struct rddma_bind_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0)
				src = rddma_src_create(parent,&reply);
			rddma_clean_bind(&reply);
		}
	}

	return src;
}

/*
 * C R E A T E     O P E R A T I O N S
 */
static struct rddma_location *rddma_fabric_location_create(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb = NULL;
	struct rddma_location *newloc = NULL;

	RDDMA_DEBUG(MY_DEBUG,"%s entered\n",__FUNCTION__);

	if (!loc)
		return new_rddma_location(NULL,desc);

	skb = rddma_fabric_call(loc, 5, "location_create://%s.%s#%llx:%x",
				desc->name, desc->location, desc->offset, loc->desc.extent);

	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;
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
				
				newloc = rddma_location_create(loc,desc);
				if (newloc && newloc->desc.address)
					newloc->desc.address->ops->register_location(newloc);
			}
			rddma_clean_desc(&reply);
		}
	}

	return newloc;
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
static struct rddma_smb *rddma_fabric_smb_create(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_smb *smb = NULL;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	skb = rddma_fabric_call(loc, 5, "smb_create://%s.%s#%llx:%x", desc->name,desc->location,desc->offset, desc->extent);
	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0)
				smb = rddma_smb_create(loc,&reply);
			rddma_clean_desc(&reply);
		}
	}

	return smb;
}

static struct rddma_mmap *rddma_fabric_mmap_create(struct rddma_smb *smb, struct rddma_desc_param *desc)
{
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	return NULL;
}

static struct rddma_bind *rddma_fabric_bind_create(struct rddma_xfer *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_bind *bind = NULL;
	struct rddma_location *loc = parent->desc.ploc;	/* Parent xfer location */

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	skb = rddma_fabric_call(loc, 5, "bind_create://%s.%s#%llx:%x/%s.%s#%llx:%x?event_name(%s)=%s.%s#%llx:%x?event_name(%s)",
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
		int ret = -EINVAL;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0)
				bind = rddma_bind_create(parent,&reply);
			rddma_clean_bind(&reply);
		}
	}

	return bind;
}

static struct rddma_xfer *rddma_fabric_xfer_create(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_xfer *xfer = NULL;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	skb = rddma_fabric_call(loc, 5, "xfer_create://%s.%s#%llx:%x",
				desc->name,desc->location,desc->offset,desc->extent
				);
	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0)
				xfer = rddma_xfer_create(loc,&reply);
			rddma_clean_desc(&reply);
		}
	}

	return xfer;
}

static int rddma_fabric_dst_events(struct rddma_bind *bind, struct rddma_bind_param *desc)
{
	/* Local destination SMB with a remote transfer agent. */
	char *event_str;
	char *event_name;
	int event_id = -1;
	struct rddma_events *event_list;
	
	RDDMA_DEBUG(MY_DEBUG,"%s bind(%p) desc(%p)\n",__FUNCTION__,bind,desc);

	bind->dst_ready_event = NULL;
	
	event_str = rddma_get_option(&desc->dst,"event_id");
	event_name = rddma_get_option(&desc->dst,"event_name");

	if (event_str && event_name) {
		sscanf(event_str,"%d",&event_id);

		event_list = find_rddma_events(rddma_subsys->events, event_name);
		if (event_list == NULL)
			event_list = rddma_events_create(rddma_subsys->events,event_name);

		bind->dst_ready_event = rddma_event_create(event_list,&desc->xfer,bind,bind->desc.xfer.ops->dst_ready,event_id);
		bind->dst_ready_event_id = event_id;
		
		event_id = rddma_doorbell_register(bind->desc.xfer.address,
						   (void (*)(void *))bind->desc.dst.ops->dst_done,
						   (void *)bind);

		bind->dst_done_event = rddma_event_create(event_list,&desc->dst,bind,0,event_id);
		bind->dst_done_event_id = event_id;
		return 0;
	}
	return -EINVAL;
}

static struct rddma_dsts *rddma_fabric_dsts_create(struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc = parent->desc.dst.ploc;
	struct rddma_dsts *dsts;
	int event_id = -1;
	struct rddma_bind_param reply;
	int ret = -EINVAL;
	char *event_str;
	char *dst_event_name;
	char *src_event_name;

	dst_event_name = rddma_get_option(&parent->desc.dst,"event_name");
	src_event_name = rddma_get_option(&parent->desc.src,"event_name");

	if (dst_event_name == NULL || src_event_name == NULL)
		goto event_fail;

	event_id = rddma_doorbell_register(parent->desc.xfer.address,
					   (void (*)(void *))parent->desc.xfer.ops->dst_ready,
					   (void *)parent);
	if (event_id < 0)
		goto event_fail;

	dsts = rddma_dsts_create(parent,desc);

	if (dsts == NULL)
		goto dsts_fail;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	skb = rddma_fabric_call(loc, 5, "dsts_create://%s.%s#%llx:%x/%s.%s#%llx:%x?event_id(%d),event_name(%s)=%s.%s#%llx:%x?event_name(%s)",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,
				event_id,dst_event_name,
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent,
				src_event_name);
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
	return dsts;

result_fail:
	rddma_clean_bind(&reply);
parse_fail:
	dev_kfree_skb(skb);
skb_fail:
	rddma_dsts_delete(dsts);
dsts_fail:
	rddma_doorbell_unregister(parent->desc.xfer.address, event_id);
event_fail:
	return NULL;
}

static struct rddma_dst *rddma_fabric_dst_create(struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc = parent->desc.dst.ploc;
	struct rddma_dst *dst = NULL;
	char *src_event_name;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	src_event_name = rddma_get_option(&desc->src,"event_name");

	if (src_event_name == NULL)
		goto fail_event;

	skb = rddma_fabric_call(loc, 5, "dst_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?event_name(%s)",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent,
				src_event_name);
	if (skb) {
		struct rddma_bind_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0) {
				dst = rddma_local_dst_find(parent,&reply);
				if (NULL == dst)
					dst = rddma_dst_create(parent,&reply);
			}
			rddma_clean_bind(&reply);
		}
	}

	return dst;
fail_event:
	return NULL;
}

static int rddma_fabric_src_events(struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	/* Local source SMB with a remote transfer agent. */
	struct rddma_bind *bind;
	char *event_str;
	char *event_name;
	int event_id = -1;
	struct rddma_events *event_list;

	bind = rddma_dst_parent(parent);

	if (bind->src_ready_event)
		return 0;
	
	event_str = rddma_get_option(&desc->src,"event_id");
	event_name = rddma_get_option(&desc->src,"event_name");

	if (event_str && event_name) {
		sscanf(event_str,"%d",&event_id);

		event_list = find_rddma_events(rddma_subsys->events, event_name);
		if (event_list == NULL)
			event_list = rddma_events_create(rddma_subsys->events,event_name);

		bind->src_ready_event = rddma_event_create(event_list,&desc->xfer,bind,bind->desc.xfer.ops->src_ready,event_id);
		bind->src_ready_event_id = event_id;
		
		event_id = rddma_doorbell_register(bind->desc.xfer.address,
						   (void (*)(void *))bind->desc.src.ops->src_done,
						   (void *)bind);

		bind->src_done_event = rddma_event_create(event_list,&desc->src,bind,0,event_id);
		bind->src_done_event_id = event_id;
		return 0;
	}
	return -EINVAL;
}

static struct rddma_srcs *rddma_fabric_srcs_create(struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *sloc = parent->desc.src.ploc;
	struct rddma_srcs *srcs;
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

	srcs = rddma_srcs_create(parent,desc);

	if (srcs == NULL)
		goto srcs_fail;

	skb = rddma_fabric_call(sloc, 5, "srcs_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x?event_name(%s),event_id(%d)",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent,event_str,bind->src_ready_event_id);
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
	return srcs;

result_fail:
	rddma_clean_bind(&reply);
parse_fail:
	dev_kfree_skb(skb);
skb_fail:
	rddma_srcs_delete(srcs);
srcs_fail:
	if (event_id != -1)
		rddma_doorbell_unregister(bind->desc.xfer.address,event_id);
event_fail:
	rddma_bind_put(bind);
out:
	return NULL;
}

static struct rddma_src *rddma_fabric_src_create(struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *sloc = parent->desc.src.ploc;
	struct rddma_src *src = NULL;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	skb = rddma_fabric_call(sloc, 5, "src_create://%s.%s#%llx:%x/%s.%s#%llx:%llx=%s.%s#%llx:%x",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,parent->desc.dst.offset,desc->dst.offset,
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent);
	if (skb) {
		struct rddma_bind_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0)
				src = rddma_src_create(parent,&reply);
			rddma_clean_bind(&reply);
		}
	}

	return src;
}

/*
 * D E L E T E    O P E R A T I O N S
 */
static void rddma_fabric_location_delete(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	skb = rddma_fabric_call(loc, 5, "location_delete://%s.%s", desc->name,desc->location);
	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;
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

	if (NULL == (smb = to_rddma_smb(kset_find_obj(&loc->smbs->kset,desc->name))) )
		return;

	skb = rddma_fabric_call(loc, 5, "smb_delete://%s.%s", desc->name, desc->location);
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

static void rddma_fabric_bind_delete(struct rddma_xfer *xfer, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc = xfer->desc.ploc;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	skb = rddma_fabric_call(loc, 5, "bind_delete://%s.%s#%llx:%x",
				desc->name, desc->location, desc->offset, desc->extent);
	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"%d",&ret) == 1) && ret == 0)
				rddma_bind_delete(xfer,&reply);
			rddma_clean_desc(&reply);
		}
	}
}
static void rddma_fabric_xfer_delete(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	skb = rddma_fabric_call(loc, 5, "xfer_delete://%s.%s", desc->name,desc->location);
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

static void rddma_fabric_dst_delete(struct rddma_bind *bind, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc = bind->desc.xfer.ploc;
	
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	skb = rddma_fabric_call(loc, 5, "dst_delete://%s.%s/%s.%s=%s.%s", desc->xfer.name,desc->xfer.location,
				desc->dst.name,desc->dst.location,
				desc->src.name,desc->src.location);
	if (skb) {
		struct rddma_bind_param reply;
		if (!rddma_parse_bind(&reply,skb->data)) {
			int ret = -EINVAL;
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0)
				rddma_dst_delete(bind,&reply);
			rddma_clean_bind(&reply);
		}
	}
}

static void rddma_fabric_src_delete(struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc = parent->desc.xfer.ploc;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	skb = rddma_fabric_call(loc, 5, "src_delete://%s.%s/%s.%s=%s.%s", desc->xfer.name,desc->xfer.location,
				desc->dst.name,desc->dst.location,
				desc->src.name,desc->src.location);
	if (skb) {
		struct rddma_bind_param reply;
		if (!rddma_parse_bind(&reply,skb->data)) {
			int ret = -EINVAL;
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0)
				rddma_src_delete(parent,&reply);
			rddma_clean_bind(&reply);
		}
	}
	
}

static void rddma_fabric_srcs_delete(struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc = parent->desc.xfer.ploc;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	skb = rddma_fabric_call(loc, 5, "srcs_delete://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x",
				desc->xfer.name, desc->xfer.location, desc->xfer.offset, desc->xfer.extent,
				desc->dst.name,  desc->dst.location,  desc->dst.offset,  desc->dst.extent,
				desc->src.name,  desc->src.location,  desc->src.offset,  desc->src.extent);
	if (skb) {
		struct rddma_bind_param reply;
		if (!rddma_parse_bind(&reply,skb->data)) {
			int ret = -EINVAL;
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0)
				rddma_srcs_delete(parent->srcs);
			rddma_clean_bind(&reply);
		}
	}
}

static void rddma_fabric_dsts_delete(struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc = parent->desc.xfer.ploc;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	skb = rddma_fabric_call(loc, 5, "dsts_delete://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x",
				desc->xfer.name, desc->xfer.location, desc->xfer.offset, desc->xfer.extent,
				desc->dst.name,  desc->dst.location,  desc->dst.offset,  desc->dst.extent,
				desc->src.name,  desc->src.location,  desc->src.offset,  desc->src.extent);
	if (skb) {
		struct rddma_bind_param reply;
		if (!rddma_parse_bind(&reply,skb->data)) {
			int ret = -EINVAL;
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0)
				rddma_dsts_delete(parent->dsts);
			rddma_clean_bind(&reply);
		}
	}
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

	skb = rddma_fabric_call(loc, 5, "event_start://%s.%s", desc->name,desc->location);
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
	.event_start     = rddma_fabric_event_start,
};

