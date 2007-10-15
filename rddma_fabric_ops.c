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
	struct rddma_location *newloc = NULL;
	struct kset *ksetp;
	RDDMA_DEBUG(MY_DEBUG,"%s\n",__FUNCTION__);

	if (!loc)
		ksetp = &rddma_subsys->kset;
	else
		ksetp = &loc->kset;
	if ( (newloc = to_rddma_location(kset_find_obj(ksetp,desc->name))) )
		return newloc;

	if (loc) {
		skb = rddma_fabric_call(loc, 5, "location_find://%s.%s", desc->name,desc->location);
	}
	else {
		loc = new_rddma_location(NULL,desc);
		skb = rddma_fabric_call(loc, 5, "location_find://%s", desc->name);
		rddma_location_put(loc);
	}
	
	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"result=%d",&ret) == 1) && ret == 0 && reply.extent) {
				unsigned long tmp = reply.offset;
				reply.offset = reply.extent;
				reply.extent = tmp;
				newloc = rddma_location_create(loc,&reply);
			}
			rddma_clean_desc(&reply);
		}
	}

	return newloc;
}

static struct rddma_smb *rddma_fabric_smb_find(struct rddma_location *parent, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc;
	struct rddma_smb *smb = NULL;

	if (NULL == (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->location))) )
		return NULL;

	skb = rddma_fabric_call(loc, 5, "smb_find://%s", desc->name);

	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"result=%d",&ret) == 1) && ret == 0)
				smb = rddma_smb_create(loc,&reply);
			rddma_clean_desc(&reply);
		}
	}

	return smb;
}

static struct rddma_mmap *rddma_fabric_mmap_find(struct rddma_smb *parent, struct rddma_desc_param *desc)
{
	return NULL;
}

static struct rddma_xfer *rddma_fabric_xfer_find(struct rddma_location *parent, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc;
	struct rddma_xfer *xfer = NULL;

	if (NULL == (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->location))) )
		return NULL;

	skb = rddma_fabric_call(loc, 5, "xfer_find://%s", desc->name);
	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"result=%d",&ret) == 1) && ret == 0)
				xfer =  rddma_xfer_create(loc,&reply);
			rddma_clean_desc(&reply);
		}
	}

	return xfer;
}

static struct rddma_bind *rddma_fabric_bind_find(struct rddma_xfer *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc;
	struct rddma_bind *bind = NULL;

	if (NULL == (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->xfer.location))) )
		return NULL;

	skb = rddma_fabric_call(loc, 5, "bind_find://%s.%s/%s.%s=%s.%s", desc->xfer.name,
				desc->xfer.location,desc->dst.name,desc->dst.location,
				desc->src.name,desc->src.location);
	if (skb) {
		struct rddma_bind_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.xfer,"result"),"result=%d",&ret) == 1) && ret == 0)
				bind = rddma_bind_create(parent,&reply);
			rddma_clean_bind(&reply);
		}
	}

	return bind;
}

static struct rddma_dst *rddma_fabric_dst_find(struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc;
	struct rddma_dst *dst = NULL;

	if (NULL == (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->xfer.location))) )
		return NULL;

	skb = rddma_fabric_call(loc, 5, "dst_find://%s.%s/%s.%s=%s.%s", desc->xfer.name,desc->xfer.location,
				desc->dst.name,desc->dst.location,
				desc->src.name,desc->src.location);
	if (skb) {
		struct rddma_bind_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.xfer,"result"),"result=%d",&ret) == 1) && ret == 0)
				dst = rddma_dst_create(parent,&reply);
			rddma_clean_bind(&reply);
		}
	}

	return dst;
}

static struct rddma_src *rddma_fabric_src_find(struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc;
	struct rddma_src *src = NULL;

	if (NULL == (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->xfer.location))) )
		return NULL;

	skb = rddma_fabric_call(loc, 5, "src_find://%s.%s/%s.%s=%s.%s", desc->xfer.name,desc->xfer.location,
				desc->dst.name,desc->dst.location,
				desc->src.name,desc->src.location);
	if (skb) {
		struct rddma_bind_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.xfer,"result"),"result=%d",&ret) == 1) && ret == 0)
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
	struct rddma_location *myloc = NULL;
	RDDMA_DEBUG(MY_DEBUG,"%s entered\n",__FUNCTION__);

	if (loc)
		myloc = loc;
	else if ( desc->address )
		myloc = new_rddma_location(NULL,desc);

	if (myloc)
		skb = rddma_fabric_call(myloc, 5, "location_create://%s.%s#%x:%x?default_ops=public",
					desc->name, desc->location, desc->extent, (u32)(myloc->desc.offset & 0xffffffffULL));

	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"result=%d",&ret) == 1) && ret == 0) {
				reply.extent = desc->extent;
				reply.ops = &rddma_local_ops;
				newloc = rddma_location_create(loc,&reply);
				newloc->desc.address = myloc->desc.address;
				myloc->desc.address->ops->register_location(newloc);
			}
			rddma_clean_desc(&reply);
		}
	}

	if (!loc)
		rddma_location_put(myloc);

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

	skb = rddma_fabric_call(loc, 5, "smb_create://%s", desc->name);
	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"result=%d",&ret) == 1) && ret == 0)
				smb = rddma_smb_create(loc,&reply);
			rddma_clean_desc(&reply);
		}
	}

	return smb;
}

static struct rddma_mmap *rddma_fabric_mmap_create(struct rddma_smb *smb, struct rddma_desc_param *desc)
{
	return NULL;
}

static struct rddma_bind *rddma_fabric_bind_create(struct rddma_xfer *parent, struct rddma_bind_param *desc)
{
	return NULL;
}

static struct rddma_xfer *rddma_fabric_xfer_create(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_xfer *xfer = NULL;

	skb = rddma_fabric_call(loc, 5, "xfer_create://%s", desc->name);
	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"result=%d",&ret) == 1) && ret == 0)
				xfer = rddma_xfer_create(loc,&reply);
			rddma_clean_desc(&reply);
		}
	}

	return xfer;
}

static struct rddma_dsts *rddma_fabric_dsts_create(struct rddma_bind *parent, struct rddma_bind_param *desc, char *name, ...)
{
	struct sk_buff  *skb;
	struct rddma_location *loc;
	struct rddma_dsts *dsts = NULL;

	va_list ap;

	if (NULL == (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->xfer.location))) )
		return NULL;

	skb = rddma_fabric_call(loc, 5, "dsts_create://%s.%s/%s.%s=%s.%s", desc->xfer.name,desc->xfer.location,
				desc->dst.name,desc->dst.location,
				desc->src.name,desc->src.location);
	if (skb) {
		struct rddma_bind_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.xfer,"result"),"result=%d",&ret) == 1) && ret == 0) {
				va_start(ap,name);
				dsts = rddma_dsts_create_ap(parent,&reply,name,ap);
				va_end(ap);
			}
			rddma_clean_bind(&reply);
		}
	}

	return dsts;
}

static struct rddma_dst *rddma_fabric_dst_create(struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc;
	struct rddma_dst *dst = NULL;

	if (NULL == (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->xfer.location))) )
		return NULL;

	skb = rddma_fabric_call(loc, 5, "dst_create://%s.%s/%s.%s=%s.%s", desc->xfer.name,desc->xfer.location,
				desc->dst.name,desc->dst.location,
				desc->src.name,desc->src.location);
	if (skb) {
		struct rddma_bind_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.xfer,"result"),"result=%d",&ret) == 1) && ret == 0)
				dst = rddma_dst_create(parent,&reply);
			rddma_clean_bind(&reply);
		}
	}

	return dst;
}

static struct rddma_srcs *rddma_fabric_srcs_create(struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc;
	struct rddma_srcs *srcs = NULL;

	if (NULL == (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->xfer.location))) )
		return NULL;

	skb = rddma_fabric_call(loc, 5, "srcs_create://%s.%s/%s.%s=%s.%s", desc->xfer.name,desc->xfer.location,
				desc->dst.name,desc->dst.location,
				desc->src.name,desc->src.location);
	if (skb) {
		struct rddma_bind_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.xfer,"result"),"result=%d",&ret) == 1) && ret == 0)
				srcs = rddma_srcs_create(parent,&reply);
			rddma_clean_bind(&reply);
		}
	}

	return srcs;
}

static struct rddma_src *rddma_fabric_src_create(struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc;
	struct rddma_src *src = NULL;

	if (NULL == (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->xfer.location))) )
		return NULL;

	skb = rddma_fabric_call(loc, 5, "src_create://%s.%s/%s.%s=%s.%s", desc->xfer.name,desc->xfer.location,
				desc->dst.name,desc->dst.location,
				desc->src.name,desc->src.location);
	if (skb) {
		struct rddma_bind_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_bind(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.xfer,"result"),"result=%d",&ret) == 1) && ret == 0)
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

	skb = rddma_fabric_call(loc, 5, "location_delete://%s", desc->name);
	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"result=%d",&ret) == 1) && ret == 0)
				rddma_location_delete(loc);
			rddma_clean_desc(&reply);
		}
	}
}

static void rddma_fabric_smb_delete(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_smb *smb;

	if (NULL == (smb = to_rddma_smb(kset_find_obj(&loc->smbs->kset,desc->name))) )
		return;

	skb = rddma_fabric_call(loc, 5, "smb_delete://%s", desc->name);
	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"result=%d",&ret) == 1) && ret == 0)
				rddma_smb_delete(smb);
			rddma_clean_desc(&reply);
		}
	}
}

static void rddma_fabric_mmap_delete(struct rddma_smb *smb, struct rddma_desc_param *desc)
{
}

static void rddma_fabric_bind_delete(struct rddma_xfer *parent, struct rddma_bind_param *desc)
{
}

static void rddma_fabric_xfer_delete(struct rddma_location *loc, struct rddma_desc_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_xfer *xfer;

	if (NULL == (xfer = to_rddma_xfer(kset_find_obj(&loc->xfers->kset,desc->name))) )
		return;

	skb = rddma_fabric_call(loc, 5, "xfer_delete://%s", desc->name);
	if (skb) {
		struct rddma_desc_param reply;
		int ret = -EINVAL;
		if (!rddma_parse_desc(&reply,skb->data)) {
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply,"result"),"result=%d",&ret) == 1) && ret == 0)
				rddma_xfer_delete(xfer);
			rddma_clean_desc(&reply);
		}
	}
}

static void rddma_fabric_dst_delete(struct rddma_bind *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc;
	struct rddma_dst *dst;
	
	if (NULL == (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->xfer.location))) )
		return;

	if (NULL == (dst = to_rddma_dst(kset_find_obj(&parent->dsts->kset,desc->xfer.name))) )
		return;

	skb = rddma_fabric_call(loc, 5, "dst_delete://%s.%s/%s.%s=%s.%s", desc->xfer.name,desc->xfer.location,
				desc->dst.name,desc->dst.location,
				desc->src.name,desc->src.location);
	if (skb) {
		struct rddma_bind_param reply;
		if (!rddma_parse_bind(&reply,skb->data)) {
			int ret = -EINVAL;
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.xfer,"result"),"result=%d",&ret) == 1) && ret == 0)
				rddma_dst_delete(dst);
			rddma_clean_bind(&reply);
		}
	}
}

static void rddma_fabric_src_delete(struct rddma_dst *parent, struct rddma_bind_param *desc)
{
	struct sk_buff  *skb;
	struct rddma_location *loc;
	struct rddma_src *src;

	if (NULL == (loc = to_rddma_location(kset_find_obj(&rddma_subsys->kset,desc->xfer.location))) )
		return;

	if (NULL == (src = to_rddma_src(kset_find_obj(&parent->srcs->kset,desc->xfer.name))) )
		return;

	skb = rddma_fabric_call(loc, 5, "src_delete://%s.%s/%s.%s=%s.%s", desc->xfer.name,desc->xfer.location,
				desc->dst.name,desc->dst.location,
				desc->src.name,desc->src.location);
	if (skb) {
		struct rddma_bind_param reply;
		if (!rddma_parse_bind(&reply,skb->data)) {
			int ret = -EINVAL;
			dev_kfree_skb(skb);
			if ( (sscanf(rddma_get_option(&reply.xfer,"result"),"result=%d",&ret) == 1) && ret == 0)
				rddma_src_delete(src);
			rddma_clean_bind(&reply);
		}
	}
	
}

struct rddma_ops rddma_fabric_ops = {
	.location_create = rddma_fabric_location_create,
	.location_delete = rddma_fabric_location_delete,
	.location_find = rddma_fabric_location_find,
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

