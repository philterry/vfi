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

#include <linux/device.h>
#include <linux/mm.h>

/*
 * F I N D    O P E R A T I O N S
 */

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

static struct rddma_xfer *rddma_fabric_xfer_find(struct rddma_location *loc, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_xfer *xfer = to_rddma_xfer(kset_find_obj(&loc->xfers->kset,desc->xfer.name));

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	skb = rddma_fabric_call(loc, 5, "xfer_find://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent);
	if (skb) {
		struct rddma_bind_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0) {
				if (xfer)
					xfer->desc.xfer.extent = reply.xfer.extent;
				else
					xfer =  rddma_xfer_create(loc,&reply);
			}
			rddma_clean_bind(&reply);
		}
	}

	return xfer;
}

static struct rddma_bind *rddma_fabric_bind_find(struct rddma_xfer *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_bind *bind = NULL;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	skb = rddma_fabric_call(parent->desc.xfer.ploc, 5, "bind_find://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent);
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

static struct rddma_dst *rddma_fabric_dst_find(struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc = parent->desc.dst.ploc;
	struct rddma_dst *dst = NULL;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

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
	struct rddma_location *loc = parent->desc.xfer.ploc;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	skb = rddma_fabric_call(loc, 5, "bind_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent);
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

static struct rddma_xfer *rddma_fabric_xfer_create(struct rddma_location *loc, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_xfer *xfer = NULL;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	skb = rddma_fabric_call(loc, 5, "xfer_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent);
	if (skb) {
		struct rddma_bind_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret == 0)
				xfer = rddma_xfer_create(loc,&reply);
			rddma_clean_bind(&reply);
		}
	}

	return xfer;
}

static struct rddma_dsts *rddma_fabric_dsts_create(struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc = parent->desc.dst.ploc;
	struct rddma_dsts *dsts = rddma_dsts_create(parent,desc);

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	skb = rddma_fabric_call(loc, 5, "dsts_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent);
	if (skb) {
		struct rddma_bind_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret != 0) {
				dsts = NULL;
			}
			rddma_clean_bind(&reply);
		}
	}

	return dsts;
}

static struct rddma_dst *rddma_fabric_dst_create(struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc = parent->desc.dst.ploc;
	struct rddma_dst *dst = NULL;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	skb = rddma_fabric_call(loc, 5, "dst_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x",
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

static struct rddma_srcs *rddma_fabric_srcs_create(struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *sloc = parent->desc.src.ploc;
	struct rddma_srcs *srcs = rddma_srcs_create(parent,desc);

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	skb = rddma_fabric_call(sloc, 5, "srcs_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent,
				desc->dst.name,desc->dst.location,desc->dst.offset,desc->dst.extent,
				desc->src.name,desc->src.location,desc->src.offset,desc->src.extent);
	if (skb) {
		struct rddma_bind_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.src,"result"),"%d",&ret) == 1) && ret != 0)
				srcs = NULL;
			rddma_clean_bind(&reply);
		}
	}

	return srcs;
}

static struct rddma_src *rddma_fabric_src_create(struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *sloc = parent->desc.src.ploc;
	struct rddma_src *src = NULL;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	skb = rddma_fabric_call(sloc, 5, "src_create://%s.%s#%llx:%x/%s.%s#%llx:%x=%s.%s#%llx:%x",
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

	skb = rddma_fabric_call(loc, 5, "smb_delete://%s", desc->name);
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

static void rddma_fabric_bind_delete(struct rddma_xfer *xfer, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc = xfer->desc.xfer.ploc;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	skb = rddma_fabric_call(loc, 5, "bind_delete://%s.%s#%llx:%x",
				desc->xfer.name,desc->xfer.location,desc->xfer.offset,desc->xfer.extent);
	if (skb) {
		struct rddma_bind_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.xfer,"result"),"%d",&ret) == 1) && ret == 0)
				rddma_bind_delete(xfer,&reply);
			rddma_clean_bind(&reply);
		}
	}
}
static void rddma_fabric_xfer_delete(struct rddma_location *loc, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;

	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	skb = rddma_fabric_call(loc, 5, "xfer_delete://%s.%s", desc->xfer.name,desc->xfer.location);
	if (skb) {
		struct rddma_bind_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.xfer,"result"),"%d",&ret) == 1) && ret == 0)
				rddma_xfer_delete(loc,&reply);
			rddma_clean_bind(&reply);
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

struct rddma_ops rddma_fabric_ops = {
	.location_create = rddma_fabric_location_create,
	.location_delete = rddma_fabric_location_delete,
	.location_find = rddma_fabric_location_find,
	.location_put = rddma_fabric_location_put,
	.smb_create = rddma_fabric_smb_create,
	.smb_delete = rddma_fabric_smb_delete,
	.smb_find = rddma_fabric_smb_find,
	.mmap_create = rddma_fabric_mmap_create,
	.mmap_delete = rddma_fabric_mmap_delete,
	.mmap_find = rddma_fabric_mmap_find,
	.xfer_create = rddma_fabric_xfer_create,
	.xfer_delete = rddma_fabric_xfer_delete,
	.xfer_find = rddma_fabric_xfer_find,
	.srcs_create = rddma_fabric_srcs_create,
	.src_create = rddma_fabric_src_create,
	.src_delete = rddma_fabric_src_delete,
	.src_find = rddma_fabric_src_find,
	.dsts_create = rddma_fabric_dsts_create,
	.dst_create = rddma_fabric_dst_create,
	.dst_delete = rddma_fabric_dst_delete,
	.dst_find = rddma_fabric_dst_find,
	.bind_find = rddma_fabric_bind_find,
/*	.bind_dst_ready = NOTHING - local op only 
	.bind_src_ready = NOTHING - local op only */
	.bind_create = rddma_fabric_bind_create,
	.bind_delete = rddma_fabric_bind_delete,
};

